/* File: load.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

#include "init.h"


/*
 * This file loads savefiles from Sil.
 *
 * We attempt to prevent corrupt savefiles from inducing memory errors.
 *
 * Note that this file should not use the random number generator, the
 * object flavors, the visual attr/char mappings, or anything else which
 * is initialized *after* or *during* the "load character" function.
 *
 * This file assumes that the monster/object records are initialized
 * to zero, and the race/kind tables have been loaded correctly.  The
 * order of object stacks is currently not saved in the savefiles, but
 * the "next" pointers are saved, so all necessary knowledge is present.
 *
 * We should implement simple "savefile extenders" using some form of
 * "sized" chunks of bytes, with a {size,type,data} format, so everyone
 * can know the size, interested people can know the type, and the actual
 * data is available to the parsing routines that acknowledge the type.
 *
 * Consider changing the "globe of invulnerability" code so that it
 * takes some form of "maximum damage to protect from" in addition to
 * the existing "number of turns to protect for", and where each hit
 * by a monster will reduce the shield by that amount.  XXX XXX XXX
 */

/*
 * Local "savefile" pointer
 */
static FILE	*fff;

/*
 * Hack -- old "encryption" byte
 */
static byte	xor_byte;

/*
 * Hack -- simple "checksum" on the actual values
 */
static u32b	v_check = 0L;

/*
 * Hack -- simple "checksum" on the encoded bytes
 */
static u32b	x_check = 0L;

static u16b new_artefacts;
static u16b art_norm_count;


/*
 * Hack -- Show information on the screen, one line at a time.
 *
 * Avoid the top two lines, to avoid interference with "msg_print()".
 */
static void note(cptr msg)
{
	static int y = 2;

	/* Draw the message */
	prt(msg, y, 0);

	/* Advance one line (wrap if needed) */
	if (++y >= 24) y = 2;

	/* Flush it */
	Term_fresh();
}

/*
 * This counts the artefacts generated so far
 */
static int artefact_count(void)
{
	int i, count = 0;
	
	// note that it only counts through the fixed and random artefacts, not the self-made ones
	for (i = 0; i < z_info->art_rand_max; i++)
	{
		if (((&a_info[i])->cur_num > 0) && !((a_info[i].flags3 & (TR3_INSTA_ART))))
		{
			count++;
		}
	}
	
	return (count);
}

/*
 * This function determines if the version of the savefile
 * currently being read is older than version "x.y.z".
 */
static bool older_than(int x, int y, int z)
{
	/* Much older, or much more recent */
	if (sf_major < x) return (TRUE);
	if (sf_major > x) return (FALSE);

	/* Distinctly older, or distinctly more recent */
	if (sf_minor < y) return (TRUE);
	if (sf_minor > y) return (FALSE);

	/* Barely older, or barely more recent */
	if (sf_patch < z) return (TRUE);
	if (sf_patch > z) return (FALSE);

	/* Identical versions */
	return (FALSE);
}


/*
 * Hack -- determine if an item is "wearable" (or a missile)
 */
static bool wearable_p(const object_type *o_ptr)
{
	/* Valid "tval" codes */
	switch (o_ptr->tval)
	{
		case TV_ARROW:
		case TV_BOW:
		case TV_DIGGING:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_SWORD:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		case TV_LIGHT:
		case TV_AMULET:
		case TV_RING:
		{
			return (TRUE);
		}
	}

	/* Nope */
	return (FALSE);
}


/*
 * The following functions are used to load the basic building blocks
 * of savefiles.  They also maintain the "checksum" info.
 */

static byte sf_get(void)
{
	byte c, v;

	/* Get a character, decode the value */
	c = getc(fff) & 0xFF;
	v = c ^ xor_byte;
	xor_byte = c;

	/* Maintain the checksum info */
	v_check += v;
	x_check += xor_byte;

	/* Return the value */
	return (v);
}

static void rd_byte(byte *ip)
{
	*ip = sf_get();
}

static void rd_u16b(u16b *ip)
{
	(*ip) = sf_get();
	(*ip) |= ((u16b)(sf_get()) << 8);
}

static void rd_s16b(s16b *ip)
{
	rd_u16b((u16b*)ip);
}

static void rd_u32b(u32b *ip)
{
	(*ip) = sf_get();
	(*ip) |= ((u32b)(sf_get()) << 8);
	(*ip) |= ((u32b)(sf_get()) << 16);
	(*ip) |= ((u32b)(sf_get()) << 24);
}

static void rd_s32b(s32b *ip)
{
	rd_u32b((u32b*)ip);
}


/*
 * Hack -- read a string
 */
static void rd_string(char *str, int max)
{
	int i;

	/* Read the string */
	for (i = 0; TRUE; i++)
	{
		byte tmp8u;

		/* Read a byte */
		rd_byte(&tmp8u);

		/* Collect string while legal */
		if (i < max) str[i] = tmp8u;

		/* End of string */
		if (!tmp8u) break;
	}

	/* Terminate */
	str[max-1] = '\0';
}


/*
 * Hack -- strip some bytes
 */
static void strip_bytes(int n)
{
	byte tmp8u;

	/* Strip the bytes */
	while (n--) rd_byte(&tmp8u);
}


/*
 * Read an object
 *
 * This function attempts to "repair" old savefiles, and to extract
 * the most up to date values for various object fields.
 */
static errr rd_item(object_type *o_ptr)
{
	u32b f1, f2, f3;

	object_kind *k_ptr;

	char buf[128];
	
	int i;

	/* Kind */
	rd_s16b(&o_ptr->k_idx);

	/* Paranoia */
	if ((o_ptr->k_idx < 0) || (o_ptr->k_idx >= z_info->k_max))
	{
		return (-1);
	}

	/* Hallucinatory Kind */
	rd_s16b(&o_ptr->image_k_idx);

	/* Location */
	rd_byte(&o_ptr->iy);
	rd_byte(&o_ptr->ix);

	/* Type/Subtype */
	rd_byte(&o_ptr->tval);
	rd_byte(&o_ptr->sval);

	/* Special pval */
	rd_s16b(&o_ptr->pval);

	rd_byte(&o_ptr->discount);

	rd_byte(&o_ptr->number);
	rd_s16b(&o_ptr->weight);

	rd_byte(&o_ptr->name1);
	rd_byte(&o_ptr->name2);

	rd_s16b(&o_ptr->timeout);

	rd_s16b(&o_ptr->att);
	rd_byte(&o_ptr->dd);
	rd_byte(&o_ptr->ds);
	rd_s16b(&o_ptr->evn);
	rd_byte(&o_ptr->pd);
	rd_byte(&o_ptr->ps);
	rd_byte(&o_ptr->pickup);

	rd_u32b(&o_ptr->ident);

	rd_byte(&o_ptr->marked);

	/* Monster holding object */
	rd_s16b(&o_ptr->held_m_idx);
	
	/* Special powers */
	rd_byte(&o_ptr->xtra1);
	
	// granted abilities
	rd_byte(&o_ptr->abilities);
	for (i = 0; i < 8; i++)
	{
		rd_byte(&o_ptr->skilltype[i]);
		rd_byte(&o_ptr->abilitynum[i]);
	}

	// 8 spare bytes
	strip_bytes(8);
		
	/* Inscription */
	rd_string(buf, sizeof(buf));

	/* Save the inscription */
	if (buf[0]) o_ptr->obj_note = quark_add(buf);

	/* Obtain the "kind" template */
	k_ptr = &k_info[o_ptr->k_idx];

	/* Obtain tval/sval from k_info */
	o_ptr->tval = k_ptr->tval;
	o_ptr->sval = k_ptr->sval;

	/* Hack -- notice "broken" items */
	if (k_ptr->cost <= 0) o_ptr->ident |= (IDENT_BROKEN);

	/* Repair non "wearable" items */
	if (!wearable_p(o_ptr))
	{
		/* Get the correct fields */
		o_ptr->att = k_ptr->att;
		o_ptr->evn = k_ptr->evn;

		/* Get the correct fields */
		o_ptr->dd = k_ptr->dd;
		o_ptr->ds = k_ptr->ds;
		o_ptr->pd = k_ptr->pd;
		o_ptr->ps = k_ptr->ps;

		/* Get the correct weight */
		o_ptr->weight = k_ptr->weight;

		/* Paranoia */
		o_ptr->name1 = o_ptr->name2 = 0;

		/* All done */
		return (0);
	}

	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);

	/* Paranoia */
	if (o_ptr->name1)
	{
		artefact_type *a_ptr;

		/*hack - adjust if new artefact*/
		if (o_ptr->name1 >= art_norm_count)
		{

			o_ptr->name1 += new_artefacts;
		}

		/* Paranoia */
		if (o_ptr->name1 >= z_info->art_max)
		{
	    	return (-1);
		}

		/* Obtain the artefact info */
		a_ptr = &a_info[o_ptr->name1];

		/* Verify that artefact */
		if (a_ptr->tval + a_ptr->sval == 0)
		{
			o_ptr->name1 = 0;
		}
	}

	/* Paranoia */
	if (o_ptr->name2)
	{
		ego_item_type *e_ptr;

		/* Paranoia */
		if (o_ptr->name2 >= z_info->e_max)
		{
		    return (-1);
		}

		/* Obtain the special item info */
		e_ptr = &e_info[o_ptr->name2];

		/* Verify that special item */
		if (!e_ptr->name) o_ptr->name2 = 0;
	}
	
	/* Hack -- extract the "broken" flag */
	if (o_ptr->pval < 0) o_ptr->ident |= (IDENT_BROKEN);

	/* Artefacts */
	if (o_ptr->name1)
	{
		artefact_type *a_ptr;

		/* Obtain the artefact info */
		a_ptr = &a_info[o_ptr->name1];

		/* Get the new artefact "pval" */
		o_ptr->pval = a_ptr->pval;

		/* Get the new artefact fields */
		o_ptr->dd = a_ptr->dd;
		o_ptr->ds = a_ptr->ds;
		o_ptr->pd = a_ptr->pd;
		o_ptr->ps = a_ptr->ps;
		o_ptr->evn = a_ptr->evn;

		/* Get the new artefact weight */
		o_ptr->weight = a_ptr->weight;

		/* Hack -- extract the "broken" flag */
		if (!a_ptr->cost) o_ptr->ident |= (IDENT_BROKEN);
	}

	/* Ego items */
	if (o_ptr->name2)
	{
		ego_item_type *e_ptr;

		/* Obtain the special item info */
		e_ptr = &e_info[o_ptr->name2];

		/* Hack -- extract the "broken" flag */
		if (!e_ptr->cost) o_ptr->ident |= (IDENT_BROKEN);

		/* Hack -- enforce legal pval */
		if (e_ptr->flags1 & (TR1_PVAL_MASK))
		{
			/* Force a meaningful pval */
			if (!o_ptr->pval) o_ptr->pval = 1;
		}

	}

	/* Used to add back boosted damage dice and sides */
	/* No longer needed as we don't repair non-artefacts anymore */

	/* Success */
	return (0);
}




/*
 * Read a monster
 */
static void rd_monster(monster_type *m_ptr)
{
    int i;
    
	/* Read the monster race */
	rd_s16b(&m_ptr->r_idx);

	/* Read the other information */
	rd_s16b(&m_ptr->image_r_idx);
	rd_byte(&m_ptr->fy);
	rd_byte(&m_ptr->fx);
	rd_s16b(&m_ptr->hp);
	rd_s16b(&m_ptr->maxhp);
	rd_s16b(&m_ptr->alertness);
	rd_byte(&m_ptr->skip_next_turn);
	rd_byte(&m_ptr->mspeed);
	rd_byte(&m_ptr->energy);
	rd_byte(&m_ptr->stunned);
	rd_byte(&m_ptr->confused);
	rd_s16b(&m_ptr->hasted);
	rd_s16b(&m_ptr->slowed);
	rd_byte(&m_ptr->stance);
	rd_s16b(&m_ptr->morale);
	rd_s16b(&m_ptr->tmp_morale);
	rd_byte(&m_ptr->noise);
	rd_byte(&m_ptr->encountered);
	rd_byte(&m_ptr->target_y);
	rd_byte(&m_ptr->target_x);
	rd_s16b(&m_ptr->wandering_idx);
	rd_byte(&m_ptr->wandering_dist);
	rd_byte(&m_ptr->mana);
	rd_byte(&m_ptr->song);
    rd_byte(&m_ptr->skip_this_turn);
    
	// 1 spare bytes
	strip_bytes(1);
    
	rd_s16b(&m_ptr->consecutive_attacks);
	rd_s16b(&m_ptr->turns_stationary);
	rd_u32b(&m_ptr->mflag);
    
    for (i = 0; i < ACTION_MAX; i++)
    {
        rd_byte(&m_ptr->previous_action[i]);
    }

	// 8 spare bytes
	strip_bytes(8);
}





/*
 * Read the monster lore
 */
static void rd_lore(int r_idx)
{
	int i;

	monster_race *r_ptr = &r_info[r_idx];
	monster_lore *l_ptr = &l_list[r_idx];

	/* Count deaths/sights/kills */
	rd_s16b(&l_ptr->deaths);
	rd_s16b(&l_ptr->psights);
	rd_s16b(&l_ptr->tsights);
	rd_s16b(&l_ptr->pkills);
	rd_s16b(&l_ptr->tkills);

	/* Count notices and ignores */
	rd_byte(&l_ptr->notice);
	rd_byte(&l_ptr->ignore);

	rd_byte(&l_ptr->drop_item);

	rd_byte(&l_ptr->ranged);

	/* Count blows of each type */
	for (i = 0; i < MONSTER_BLOW_MAX; i++)
		rd_byte(&l_ptr->blows[i]);

	/* Memorize flags */
	rd_u32b(&l_ptr->flags1);
	rd_u32b(&l_ptr->flags2);
	rd_u32b(&l_ptr->flags3);
	rd_u32b(&l_ptr->flags4);

	/* Read the "Racial" monster limit per level */
	rd_byte(&r_ptr->max_num);

	// 8 spare bytes
	strip_bytes(8);
		
	/* Repair the lore flags */
	l_ptr->flags1 &= r_ptr->flags1;
	l_ptr->flags2 &= r_ptr->flags2;
	l_ptr->flags3 &= r_ptr->flags3;
	l_ptr->flags4 &= r_ptr->flags4;
}


/*
 * Read RNG state
 */
static void rd_randomizer(void)
{
	int i;

	// 8 spare bytes
	strip_bytes(8);
	
	/* Place */
	rd_u16b(&Rand_place);

	/* State */
	for (i = 0; i < RAND_DEG; i++)
	{
		rd_u32b(&Rand_state[i]);
	}

	/* Accept */
	Rand_quick = FALSE;
}



/*
 * Read options
 *
 * Note that the normal options are stored as a set of 256 bit flags,
 * plus a set of 256 bit masks to indicate which bit flags were defined
 * at the time the savefile was created.  This will allow new options
 * to be added, and old options to be removed, at any time, without
 * hurting old savefiles.
 *
 * The window options are stored in the same way, but note that each
 * window gets 32 options, and their order is fixed by certain defines.
 */
static void rd_options(void)
{
	int i, n;

	byte b;

	u32b flag[8];
	u32b mask[8];
	u32b window_flag[ANGBAND_TERM_MAX];
	u32b window_mask[ANGBAND_TERM_MAX];


	/*** Special info */

	/* Read "delay_factor" */
	rd_byte(&b);
	op_ptr->delay_factor = b;

	/* Read "hitpoint_warn" */
	rd_byte(&b);
	op_ptr->hitpoint_warn = b;

	// 8 spare bytes
	strip_bytes(8);
	
	/*** Normal Options ***/

	/* Read the option flags */
	for (n = 0; n < 8; n++) rd_u32b(&flag[n]);

	/* Read the option masks */
	for (n = 0; n < 8; n++) rd_u32b(&mask[n]);

	/* Analyze the options */
	for (i = 0; i < OPT_MAX; i++)
	{
		int os = i / 32;
		int ob = i % 32;

		/* Process real entries */
		if (option_text[i])
		{
			/* Process saved entries */
			if (mask[os] & (1L << ob))
			{
				/* Set flag */
				if (flag[os] & (1L << ob))
				{
					/* Set */
					op_ptr->opt[i] = TRUE;
				}

				/* Clear flag */
				else
				{
					/* Set */
					op_ptr->opt[i] = FALSE;
				}
			}
		}
	}


	/*** Window Options ***/

	/* Read the window flags */
	for (n = 0; n < ANGBAND_TERM_MAX; n++)
	{
		rd_u32b(&window_flag[n]);
	}

	/* Read the window masks */
	for (n = 0; n < ANGBAND_TERM_MAX; n++)
	{
		rd_u32b(&window_mask[n]);
	}

	/* Analyze the options */
	for (n = 0; n < ANGBAND_TERM_MAX; n++)
	{
		/* Analyze the options */
		for (i = 0; i < 32; i++)
		{
			/* Process valid flags */
			if (window_flag_desc[i])
			{
				/* Process valid flags */
				if (window_mask[n] & (1L << i))
				{
					/* Set */
					if (window_flag[n] & (1L << i))
					{
						/* Set */
						op_ptr->window_flag[n] |= (1L << i);
					}
				}
			}
		}
	}

}



static u32b randart_version;


/*
 * Read the "extra" information
 */
static errr rd_extra(void)
{
	int i, j;

	byte tmp8u;
	u16b file_e_max;


	rd_string(op_ptr->full_name, sizeof(op_ptr->full_name));

	rd_string(p_ptr->died_from, 80);

	rd_string(p_ptr->history, 250);

	/* Player race */
	rd_byte(&p_ptr->prace);

	/* Verify player race */
	if (p_ptr->prace >= z_info->p_max)
	{
		note(format("Invalid player race (%d).", p_ptr->prace));
		return (-1);
	}

	/* Player house */
	rd_byte(&p_ptr->phouse);

	/* Verify player house */
	if (p_ptr->phouse >= z_info->c_max)
	{
		note(format("Invalid player house (%d).", p_ptr->phouse));
		return (-1);
	}

	/* Player sex */
	rd_byte(&p_ptr->psex);

	/* Tutorial? */
	rd_s16b(&p_ptr->game_type);

	/* Age/Height/Weight */
	rd_s16b(&p_ptr->age);
	rd_s16b(&p_ptr->ht);
	rd_s16b(&p_ptr->wt);

	/* Read the stat info */
	for (i = 0; i < A_MAX; i++) rd_s16b(&p_ptr->stat_base[i]);
	for (i = 0; i < A_MAX; i++) rd_s16b(&p_ptr->stat_drain[i]);

	/* Read the skill info */
	for (i = 0; i < S_MAX; i++) rd_s16b(&p_ptr->skill_base[i]);
	
	/* Read the abilities info */
	for (i = 0; i < S_MAX; i++)
	{
		for (j = 0; j < ABILITIES_MAX; j++)
		{
			rd_byte(&p_ptr->innate_ability[i][j]);
			rd_byte(&p_ptr->active_ability[i][j]);
		}
	}
	
	rd_s16b(&p_ptr->last_attack_m_idx);	
	rd_s16b(&p_ptr->consecutive_attacks);
	rd_s16b(&p_ptr->bane_type);
		
	for (i = 0; i < ACTION_MAX; ++i) 
	{
		rd_byte(&(p_ptr->previous_action[i]));
	}
	rd_byte(&p_ptr->focused);
	
	rd_s32b(&p_ptr->new_exp);
	rd_s32b(&p_ptr->exp);

	rd_s32b(&p_ptr->encounter_exp);
	rd_s32b(&p_ptr->kill_exp);
	rd_s32b(&p_ptr->descent_exp);
	rd_s32b(&p_ptr->ident_exp);

	rd_s16b(&p_ptr->mhp);
	rd_s16b(&p_ptr->chp);
	rd_u16b(&p_ptr->chp_frac);

	rd_s16b(&p_ptr->msp);
	rd_s16b(&p_ptr->csp);
	rd_u16b(&p_ptr->csp_frac);

	rd_s16b(&p_ptr->max_depth);

	/* Hack -- Repair maximum dungeon level */
	if (p_ptr->max_depth < 0) p_ptr->max_depth = 1;

	rd_u16b(&p_ptr->staircasiness);

	/* More info */
	rd_s16b(&p_ptr->sc);

	/* Read the flags */
	rd_byte(&p_ptr->song1);
	rd_byte(&p_ptr->song2);
	rd_s16b(&p_ptr->song_duration);
	rd_s16b(&p_ptr->wrath);
	rd_s16b(&p_ptr->blind);
	rd_s16b(&p_ptr->entranced);
	rd_s16b(&p_ptr->confused);
	rd_s16b(&p_ptr->food);
	rd_u16b(&p_ptr->stairs_taken);
	rd_u16b(&p_ptr->forge_drought);
	rd_u16b(&p_ptr->forge_count);
	rd_s16b(&p_ptr->energy);
	rd_s16b(&p_ptr->fast);
	rd_s16b(&p_ptr->slow);
	rd_s16b(&p_ptr->afraid);
	rd_s16b(&p_ptr->cut);
	rd_s16b(&p_ptr->stun);
	rd_s16b(&p_ptr->poisoned);
	rd_s16b(&p_ptr->image);
	rd_s16b(&p_ptr->rage);
	rd_s16b(&p_ptr->tmp_str);
	rd_s16b(&p_ptr->tmp_dex);
	rd_s16b(&p_ptr->tmp_con);
	rd_s16b(&p_ptr->tmp_gra);
	rd_s16b(&p_ptr->tim_invis);
	rd_s16b(&p_ptr->word_recall);
	rd_s16b(&p_ptr->darkened);
	rd_s16b(&p_ptr->oppose_fire);
	rd_s16b(&p_ptr->oppose_cold);
	rd_s16b(&p_ptr->oppose_pois);

	rd_byte(&p_ptr->stealth_mode);
	rd_byte(&p_ptr->self_made_arts);
	rd_byte(&p_ptr->climbing);

	// 19 spare bytes
	strip_bytes(19);

		
	/* Read item-quality squelch sub-menu */
 	for (i = 0; i < SQUELCH_BYTES; i++) rd_byte(&squelch_level[i]);

	/* Load the name of the current greater vault */
	rd_string(g_vault_name, sizeof(g_vault_name));

	/* Read the number of saved special item types */
	rd_u16b(&file_e_max);

	/* Read special item squelch settings */
	for (i = 0; i < z_info->e_max; i++)
	{
		ego_item_type *e_ptr = &e_info[i];

		tmp8u = 0;

		if (i < file_e_max) rd_byte(&tmp8u);

		e_ptr->squelch |= (tmp8u & 0x01);
		e_ptr->everseen |= (tmp8u & 0x02);
		e_ptr->aware |= (tmp8u & 0x04);

		/* Hack - Repair the savefile */
		if (!e_ptr->everseen) e_ptr->squelch = FALSE;
	}

	/* Read possible extra elements */
	while (i < file_e_max)
	{
		rd_byte(&tmp8u);
		i++;
	}

	/*Write the current number of auto-inscriptions*/
	rd_u16b(&inscriptionsCount);

	/*Write the autoinscriptions array*/
	for(i = 0; i < inscriptionsCount; i++)
	{
		char tmp[80];

		rd_s16b(&inscriptions[i].kindIdx);

		rd_string(tmp, 80);

		inscriptions[i].inscriptionIdx = quark_add(tmp);
	}

	for (i = 0; i < MAX_GREATER_VAULTS; i++)
	{
		s16b n;
		
		rd_s16b(&n);
		p_ptr->greater_vaults[i] = n;
	}

	/* Read the randart version */
	rd_u32b(&randart_version);

	/* Read the randart seed */
	rd_u32b(&seed_randart);

	/* Hack -- the two "special seeds" */
	rd_u32b(&seed_flavor);
	
	/* Special stuff */
	rd_u16b(&p_ptr->panic_save);
	rd_byte(&p_ptr->truce);
	rd_byte(&p_ptr->morgoth_hits);
	rd_byte(&p_ptr->crown_hint);
	rd_byte(&p_ptr->crown_shatter);
	rd_byte(&p_ptr->cursed);
	rd_byte(&p_ptr->on_the_run);
	rd_byte(&p_ptr->morgoth_slain);
	rd_u16b(&p_ptr->escaped);
	rd_u16b(&p_ptr->noscore);
	rd_s16b(&p_ptr->smithing_leftover);

	rd_byte(&tmp8u);
	p_ptr->unique_forge_made = tmp8u;
	rd_byte(&tmp8u);
	p_ptr->unique_forge_seen = tmp8u;
	
	/* Read "death" */
	rd_byte(&tmp8u);
	p_ptr->is_dead = tmp8u;

	/* Read "feeling" */
	rd_byte(&tmp8u);
	feeling = tmp8u;

	/*read the level feeling*/
	rd_byte(&tmp8u);
	do_feeling = tmp8u;

	/* Current turn */
	rd_s32b(&turn);
	
	/* Current player turn */
	rd_s32b(&playerturn);

	return (0);
}

/*
 * Read the random artefacts
 */
static errr rd_randarts(void)
{

	int i;
	byte tmp8u;
	s16b tmp16s;
	u16b tmp16u;
	u16b artefact_count, begin;
	s32b tmp32s;
	u32b tmp32u;

	/* Read the number of artefacts */
	rd_u16b(&begin);
	rd_u16b(&artefact_count);

	rd_u16b(&art_norm_count);

	/* Alive or cheating death */
	if (!p_ptr->is_dead || arg_wizard)
	{
		/* Incompatible save files */
		if ((artefact_count > z_info->art_max) || (art_norm_count > z_info->art_norm_max))
		{
			note(format("Too many (%u) artefacts!", artefact_count));
			return (-1);
		}
		/*Mark any new added artefacts*/
		if (art_norm_count < z_info->art_norm_max)
		{
			new_artefacts = z_info->art_norm_max - art_norm_count;
		}
		else new_artefacts = 0;

		/* Mark the old artefacts as "empty" */
		for (i = begin; i < z_info->art_max; i++)
		{
			artefact_type *a_ptr = &a_info[i];

			/*hack - if a new "normal artefact has been added in mid-game, don't erase it*/
			if ((i >= art_norm_count) && (i < z_info->art_norm_max)) continue;

			a_ptr->tval = 0;
			a_ptr->sval = 0;
			a_ptr->name[0] = '\0';
		}

		/* Read the artefacts */
		for (i = begin; i < artefact_count; i++)
		{

			artefact_type *a_ptr = &a_info[i];

			/*hack - if a new "normal artefact has been added in mid-game, don't erase it*/
			if ((i >= art_norm_count) && (i < z_info->art_norm_max)) continue;

			rd_string (a_ptr->name, MAX_LEN_ART_NAME);

			rd_byte(&a_ptr->tval);
			rd_byte(&a_ptr->sval);
			rd_s16b(&a_ptr->pval);

			rd_s16b(&a_ptr->att);
			rd_byte(&a_ptr->dd);
			rd_byte(&a_ptr->ds);
			rd_s16b(&a_ptr->evn);
			rd_byte(&a_ptr->pd);
			rd_byte(&a_ptr->ps);

			rd_s16b(&a_ptr->weight);
			rd_s32b(&a_ptr->cost);
			
			rd_u32b(&a_ptr->flags1);
			rd_u32b(&a_ptr->flags2);
			rd_u32b(&a_ptr->flags3);
			rd_byte(&a_ptr->level);
			rd_byte(&a_ptr->rarity);
			rd_byte(&a_ptr->activation);
			rd_u16b(&a_ptr->time);
			rd_u16b(&a_ptr->randtime);
		}
	}
	else
	{
		/* Strip the the artefacts for a dead/new character*/
		for (i = begin; i < artefact_count; i++)
		{
			char tmpstr[MAX_LEN_ART_NAME];
			rd_string (tmpstr, sizeof (tmpstr)); /*a_ptr->name*/
			rd_byte(&tmp8u); /* a_ptr->tval */
			rd_byte(&tmp8u); /* a_ptr->sval */
			rd_s16b(&tmp16s); /* a_ptr->pval */
			
			/* Added for Sil */
			rd_s16b(&tmp16s);	// a_ptr->att
			rd_byte(&tmp8u);	// a_ptr->dd
			rd_byte(&tmp8u);	// a_ptr->ds
			rd_s16b(&tmp16s);	// a_ptr->evn
			rd_byte(&tmp8u);	// a_ptr->pd
			rd_byte(&tmp8u);	// a_ptr->ps

			rd_s16b(&tmp16s); /* a_ptr->weight */
			rd_s32b(&tmp32s); /* a_ptr->cost */

			rd_u32b(&tmp32u); /* a_ptr->flags1 */
			rd_u32b(&tmp32u); /* a_ptr->flags2 */
			rd_u32b(&tmp32u); /* a_ptr->flags3 */
			rd_byte(&tmp8u); /* a_ptr->level */
			rd_byte(&tmp8u); /* a_ptr->rarity */

			rd_byte(&tmp8u); /* a_ptr->activation */
			rd_u16b(&tmp16u); /* a_ptr->time */
			rd_u16b(&tmp16u); /* a_ptr->randtime */
		}
	}

	return (0);

}

/*
 * Read the notes. Every new savefile has at least NOTES_MARK.
 */
static bool rd_notes(void)
{
	int alive = (!p_ptr->is_dead || arg_wizard);
	char tmpstr[100];
	int i;

	// reset the notes buffer
	for (i = 0; i < NOTES_LENGTH; i++)
	{
		notes_buffer[i] = '\0';
	}

	if (alive)
	{
		/* Append the notes in the savefile to the buffer */
		while (TRUE)
		{
			rd_string(tmpstr, sizeof(tmpstr));
			/* Found the end? */
			if (strstr(tmpstr, NOTES_MARK))
				break;
			my_strcat(notes_buffer, format("%s\n", tmpstr), sizeof(notes_buffer));
		}
	}
	/* Ignore the notes */
	else
	{
		while (TRUE)
		{
			rd_string(tmpstr, sizeof(tmpstr));

			/* Found the end? */
			if (strstr(tmpstr, NOTES_MARK))
			{
				break;
			}
		}
	}

	return 0;
}






/*
 * Read the player inventory (and the smithing object)
 *
 * Note that the inventory is "re-sorted" later by "dungeon()".
 */
static errr rd_inventory(void)
{
	int slot = 0;

	object_type *i_ptr;
	object_type object_type_body;

	/* Wipe the smithing object */
	object_wipe(smith_o_ptr);
	
	/* Read the smithing object */
	if (rd_item(smith_o_ptr))
	{
		note("Error reading smithing item");
		return (-1);
	}

	/* Read until done */
	while (1)
	{
		u16b n;

		/* Get the next item index */
		rd_u16b(&n);

		/* Nope, we reached the end */
		if (n == 0xFFFF) break;

		/* Get local object */
		i_ptr = &object_type_body;

		/* Wipe the object */
		object_wipe(i_ptr);

		/* Read the item */
		if (rd_item(i_ptr))
		{
			note("Error reading item");
			return (-1);
		}

		/* Hack -- verify item */
		if (!i_ptr->k_idx)	return (-1);

		/* Verify slot */
		if (n >= INVEN_TOTAL) return (-1);

		/* Wield equipment */
		if (n >= INVEN_WIELD)
		{
			/* Copy object */
			object_copy(&inventory[n], i_ptr);

			/* One more item */
			p_ptr->equip_cnt++;
		}

		/* Warning -- backpack is full */
		else if (p_ptr->inven_cnt == INVEN_PACK)
		{
			/* Oops */
			note("Too many items in the inventory!");

			/* Fail */
			return (-1);
		}

		/* Carry inventory */
		else
		{
			/* Get a slot */
			n = slot++;

			/* Copy object */
			object_copy(&inventory[n], i_ptr);

			/* One more item */
			p_ptr->inven_cnt++;
		}
	}

	/* Success */
	return (0);
}



/*
 * Read the saved messages
 */
static void rd_messages(void)
{
	int i;
	char buf[128];
	u16b tmp16u;

	s16b num;

	/* Total */
	rd_s16b(&num);

	/* Read the messages */
	for (i = 0; i < num; i++)
	{
		/* Read the message */
		rd_string(buf, sizeof(buf));

		/* Read the message type */
		rd_u16b(&tmp16u);

		/* Save the message */
		message_add(buf, tmp16u);
	}
}


/*
 * Read the dungeon
 *
 * The monsters/objects must be loaded in the same order
 * that they were stored, since the actual indexes matter.
 *
 * Note that the size of the dungeon is now hard-coded to
 * DUNGEON_HGT by DUNGEON_WID, and any dungeon with another
 * size will be silently discarded by this routine.
 *
 * Note that dungeon objects, including objects held by monsters, are
 * placed directly into the dungeon, using "object_copy()", which will
 * copy "iy", "ix", and "held_m_idx", leaving "next_o_idx" blank for
 * objects held by monsters, since it is not saved in the savefile.
 *
 * After loading the monsters, the objects being held by monsters are
 * linked directly into those monsters.
 */
static errr rd_dungeon(void)
{
	int i, y, x;

	s16b depth;
	s16b py, px;

	byte count;
	byte tmp8u;

	u16b limit;


	/*** Basic info ***/

	/* Header info */
	rd_s16b(&depth);
	rd_s16b(&py);
	rd_s16b(&px);
	rd_byte(&p_ptr->cur_map_hgt);
	rd_byte(&p_ptr->cur_map_wid);

	/* Ignore illegal dungeons */
	if ((depth < 0) || (depth > MORGOTH_DEPTH))
	{
		note(format("Ignoring illegal dungeon depth (%d)", depth));
		return (0);
	}

	/* Ignore illegal dungeons */
	if ((p_ptr->cur_map_hgt > MAX_DUNGEON_HGT) || (p_ptr->cur_map_wid > MAX_DUNGEON_WID))
	{
		/* XXX XXX XXX */
		note(format("Ignoring illegal dungeon size (%d,%d).", p_ptr->cur_map_hgt, p_ptr->cur_map_wid));
		return (0);
	}

	/* Ignore illegal dungeons */
	if ((px < 0) || (px >= p_ptr->cur_map_wid) ||
		(py < 0) || (py >= p_ptr->cur_map_hgt))
	{
		note(format("Ignoring illegal player location (%d,%d).", py, px));
		return (1);
	}


	/*** Run length decoding ***/

	/* Load the dungeon data */
	for (x = y = 0; y < p_ptr->cur_map_hgt; )
	{
		/* Grab RLE info */
		rd_byte(&count);
		rd_byte(&tmp8u);

		/* Apply the RLE info */
		for (i = count; i > 0; i--)
		{
			/* Extract "info" */
			cave_info[y][x] = tmp8u;

			/* Advance/Wrap */
			if (++x >= p_ptr->cur_map_wid)
			{
				/* Wrap */
				x = 0;

				/* Advance/Wrap */
				if (++y >= p_ptr->cur_map_hgt) break;
			}
		}
	}


	/*** Run length decoding ***/

	/* Load the dungeon data */
	for (x = y = 0; y < p_ptr->cur_map_hgt; )
	{
		/* Grab RLE info */
		rd_byte(&count);
		rd_byte(&tmp8u);

		/* Apply the RLE info */
		for (i = count; i > 0; i--)
		{
			/* Extract "feat" */
			cave_set_feat(y, x, tmp8u);

			/* Advance/Wrap */
			if (++x >= p_ptr->cur_map_wid)
			{
				/* Wrap */
				x = 0;

				/* Advance/Wrap */
				if (++y >= p_ptr->cur_map_hgt) break;
			}
		}
	}


	/*** Player ***/

	/* Load depth */
	p_ptr->depth = depth;

	/* Place player in dungeon */
	if (!player_place(py, px))
	{
		note(format("Cannot place player (%d,%d)!", py, px));
		return (-1);
	}


	/*** Objects ***/

	/* Read the item count */
	rd_u16b(&limit);

	/* Verify maximum */
	if (limit > z_info->o_max)
	{
		note(format("Too many (%d) object entries!", limit));
		return (-1);
	}

	/* Read the dungeon items */
	for (i = 1; i < limit; i++)
	{
		object_type *i_ptr;
		object_type object_type_body;

		s16b o_idx;
		object_type *o_ptr;


		/* Get the object */
		i_ptr = &object_type_body;

		/* Wipe the object */
		object_wipe(i_ptr);

		/* Read the item */
		if (rd_item(i_ptr))
		{
			note("Error reading item");
			return (-1);
		}

		/* Make an object */
		o_idx = o_pop();

		/* Paranoia */
		if (o_idx != i)
		{
			note(format("Cannot place object %d!", i));
			return (-1);
		}

		/* Get the object */
		o_ptr = &o_list[o_idx];

		/* Structure Copy */
		object_copy(o_ptr, i_ptr);

		/* Dungeon floor */
		if (!i_ptr->held_m_idx)
		{
			int x = i_ptr->ix;
			int y = i_ptr->iy;

			/* ToDo: Verify coordinates */

			/* Link the object to the pile */
			o_ptr->next_o_idx = cave_o_idx[y][x];

			/* Link the floor to the object */
			cave_o_idx[y][x] = o_idx;

			/* Rearrange stack if needed */
			rearrange_stack(y, x);
		}
	}


	/*** Monsters ***/

	/* Read the monster count */
	rd_u16b(&limit);

	/* Hack -- verify */
	if (limit > MAX_MONSTERS)
	{
		note(format("Too many (%d) monster entries!", limit));
		return (-1);
	}

	/* Read the monsters */
	for (i = 1; i < limit; i++)
	{
		monster_type *n_ptr;
		monster_type monster_type_body;
		monster_race *r_ptr;

		int r_idx;

		/* Get local monster */
		n_ptr = &monster_type_body;

		/* Clear the monster */
		(void)WIPE(n_ptr, monster_type);

		/* Read the monster */
		rd_monster(n_ptr);

		/* Access the "r_idx" of the chosen monster */
		r_idx = n_ptr->r_idx;

		/* Access the actual race */
		r_ptr = &r_info[r_idx];

		/* Place monster in dungeon */
		if (monster_place(n_ptr->fy, n_ptr->fx, n_ptr) != i)
		{
			note(format("Cannot place monster %d", i));
			return (-1);
		}
	}

	/*** Holding ***/

	/* Reacquire objects */
	for (i = 1; i < o_max; ++i)
	{
		object_type *o_ptr;

		monster_type *m_ptr;

		/* Get the object */
		o_ptr = &o_list[i];

		/* Ignore dungeon objects */
		if (!o_ptr->held_m_idx) continue;

		/* Verify monster index */
		if (o_ptr->held_m_idx > MAX_MONSTERS)
		{
			note("Invalid monster index");
			return (-1);
		}

		/* Get the monster */
		m_ptr = &mon_list[o_ptr->held_m_idx];

		/* Link the object to the pile */
		o_ptr->next_o_idx = m_ptr->hold_o_idx;

		/* Link the monster to the object */
		m_ptr->hold_o_idx = i;
	}

	// dump the wandering monster information
	for (i = FLOW_WANDERING_HEAD; i <= FLOW_WANDERING_TAIL; i++)
	{
		rd_byte(&flow_center_y[i]);
		rd_byte(&flow_center_x[i]);
		rd_s16b(&wandering_pause[i]);
		
		update_flow(flow_center_y[i], flow_center_x[i], i);
	}
	
		
	/*** Success ***/

	/* The dungeon is ready */
	character_dungeon = TRUE;

	/* Success */
	return (0);
}

/*
 * Actually read the savefile
 */
static errr rd_savefile_new_aux(void)
{
	int i;

	byte tmp8u;
	u16b tmp16u;

	u32b n_x_check, n_v_check;
	u32b o_x_check, o_v_check;

	/* Mention the savefile version */
	note(format("Loading a %d.%d.%d savefile...",
	            sf_major, sf_minor, sf_patch));

	/* Strip the version bytes */
	strip_bytes(4);

	/* Hack -- decrypt */
	xor_byte = sf_extra;

	/* Clear the checksums */
	v_check = 0L;
	x_check = 0L;

	/* Operating system info */
	rd_u32b(&sf_xtra);

	/* Time of savefile creation */
	rd_u32b(&sf_when);

	/* Number of resurrections */
	rd_u16b(&sf_lives);

	/* Number of times played */
	rd_u16b(&sf_saves);

	// 8 spare bytes
	strip_bytes(8);

	/* Read RNG state */
	rd_randomizer();
	if (arg_fiddle) note("Loaded Randomizer Info");

	/* Then the options */
	rd_options();
	if (arg_fiddle) note("Loaded Option Flags");

	/* Then the "messages" */
	rd_messages();
	if (arg_fiddle) note("Loaded Messages");

	/* Monster Memory */
	rd_u16b(&tmp16u);

	/* Incompatible save files */
	if (tmp16u > z_info->r_max)
	{
		note(format("Too many (%u) monster races!", tmp16u));
		return (-1);
	}

	/* Read the available records */
	for (i = 0; i < tmp16u; i++)
	{
		/* Read the lore */
		rd_lore(i);
	}
	if (arg_fiddle) note("Loaded Monster Memory");


	/* Object Memory */
	rd_u16b(&tmp16u);

	/* Incompatible save files */
	if (tmp16u > z_info->k_max)
	{
		note(format("Too many (%u) object kinds!", tmp16u));
		return (-1);
	}

	/* Read the object memory */
	for (i = 0; i < tmp16u; i++)
	{
		byte tmp8u;

		object_kind *k_ptr = &k_info[i];

		rd_byte(&tmp8u);

		k_ptr->aware = (tmp8u & 0x01) ? TRUE: FALSE;
		k_ptr->tried = (tmp8u & 0x02) ? TRUE: FALSE;
		k_ptr->everseen = (tmp8u & 0x08) ? TRUE: FALSE;

		rd_byte(&k_ptr->squelch);
	}
	if (arg_fiddle) note("Loaded Object Memory");

	/* Load the Artefacts */
	rd_u16b(&tmp16u);

	/* Incompatible save files */
	if (tmp16u > z_info->art_max)
	{
		note(format("Too many (%u) artefacts!", tmp16u));
		return (-1);
	}

	/* Read the artefact flags */
	for (i = 0; i < tmp16u; i++)
	{
		rd_byte(&tmp8u);
		a_info[i].cur_num = tmp8u;
		rd_byte(&tmp8u);
		a_info[i].found_num = tmp8u;
	}
	if (arg_fiddle) note("Loaded Artefacts");


	/* Read the extra stuff */
	if (rd_extra()) return (-1);
	if (arg_fiddle) note("Loaded extra information");

	if (rd_randarts()) return (-1);
	if (arg_fiddle) note("Loaded Random Artefacts");

	if (rd_notes()) return (-1);
	if (arg_fiddle) note("Loaded Notes");

	/* Important -- Initialize the sex */
	sp_ptr = &sex_info[p_ptr->psex];

	/* Important -- Initialize the race/house */
	rp_ptr = &p_info[p_ptr->prace];
	hp_ptr = &c_info[p_ptr->phouse];

	/* Read the inventory */
	if (rd_inventory())
	{
		note("Unable to read inventory");
		return (-1);
	}

	/* I'm not dead yet... */
	if (!p_ptr->is_dead)
	{
		/* Dead players have no dungeon */
		note("Restoring Dungeon...");
		if (rd_dungeon())
		{
			note("Error reading dungeon data");
			return (-1);
		}

	}

	/* Save the checksum */
	n_v_check = v_check;

	/* Read the old checksum */
	rd_u32b(&o_v_check);

	/* Verify */
	if (o_v_check != n_v_check)
	{
		note("Invalid checksum");
		return (-1);
	}

	/* Save the encoded checksum */
	n_x_check = x_check;

	/* Read the checksum */
	rd_u32b(&o_x_check);

	/* Verify */
	if (o_x_check != n_x_check)
	{
		note("Invalid encoded checksum");
		return (-1);
	}

	/* Success */
	return (0);
}


/*
 * Actually read the savefile
 */
static errr rd_savefile(void)
{
	errr err;

	/* Grab permissions */
	safe_setuid_grab();

	/* The savefile is a binary file */
	fff = my_fopen(savefile, "rb");

	/* Drop permissions */
	safe_setuid_drop();

	/* Paranoia */
	if (!fff) return (-1);

	/* Call the sub-function */
	err = rd_savefile_new_aux();

	/* Check for errors */
	if (ferror(fff)) err = -1;

	/* Close the file */
	my_fclose(fff);

	/* Result */
	return (err);
}


/*
 * Attempt to Load a "savefile"
 *
 * On multi-user systems, you may only "read" a savefile if you will be
 * allowed to "write" it later, this prevents painful situations in which
 * the player loads a savefile belonging to someone else, and then is not
 * allowed to save his game when he quits.
 *
 * We return "TRUE" if the savefile was usable, and we set the global
 * flag "character_loaded" if a real, living, character was loaded.
 *
 * Note that we always try to load the "current" savefile, even if
 * there is no such file, so we must check for "empty" savefile names.
 */
bool load_player(void)
{
	int fd = -1;

	errr err = 0;

	byte vvv[4];

#ifdef VERIFY_TIMESTAMP
	struct stat	statbuf;
#endif /* VERIFY_TIMESTAMP */

	cptr what = "generic";

	/* Paranoia */
	turn = 0;
	
	/* Paranoia */
	p_ptr->is_dead = FALSE;
	
	// Set a flag to show that we are restoring a game
	p_ptr->restoring = TRUE;

	/* Allow empty savefile name */
	if (!savefile[0]) return (TRUE);

	/* Grab permissions */
	safe_setuid_grab();

	/* Open the savefile */
	fd = fd_open(savefile, O_RDONLY);

	/* Drop permissions */
	safe_setuid_drop();

	/* No file */
	if (fd < 0)
	{
		/* Give a message */
		msg_format("Savefile \"%s\" does not exist.", savefile);
		message_flush();

		/* Allow this */
		p_ptr->restoring = FALSE;
		return (FALSE);////
	}

	/* Close the file */
	fd_close(fd);


#ifdef VERIFY_SAVEFILE

	/* Verify savefile usage */
	if (!err)
	{
		FILE *fkk;

		char temp[1024];

		/* Extract name of lock file */
		my_strcpy(temp, savefile, sizeof(temp));
		my_strcat(temp, ".lok", sizeof(temp));

		/* Grab permissions */
		safe_setuid_grab();

		/* Check for lock */
		fkk = my_fopen(temp, "r");

		/* Drop permissions */
		safe_setuid_drop();

		/* Oops, lock exists */
		if (fkk)
		{
			/* Close the file */
			my_fclose(fkk);

			/* Message */
			msg_print("Savefile is currently in use.");
			message_flush();

			/* Oops */
			return (FALSE);
		}

		/* Grab permissions */
		safe_setuid_grab();

		/* Create a lock file */
		fkk = my_fopen(temp, "w");

		/* Drop permissions */
		safe_setuid_drop();

		/* Dump a line of info */
		fprintf(fkk, "Lock file for savefile '%s'\n", savefile);

		/* Close the lock file */
		my_fclose(fkk);
	}

#endif /* VERIFY_SAVEFILE */


	/* Okay */
	if (!err)
	{
		/* Grab permissions */
		safe_setuid_grab();

		/* Open the savefile */
		fd = fd_open(savefile, O_RDONLY);

		/* Drop permissions */
		safe_setuid_drop();

		/* No file */
		if (fd < 0) err = -1;

		/* Message (below) */
		if (err) what = "Cannot open savefile";
	}

	/* Process file */
	if (!err)
	{

#ifdef VERIFY_TIMESTAMP

		/* Grab permissions */
		safe_setuid_grab();

		/* Get the timestamp */
		(void)fstat(fd, &statbuf);

		/* Drop permissions */
		safe_setuid_drop();

#endif /* VERIFY_TIMESTAMP */

		/* Read the first four bytes */
		if (fd_read(fd, (char*)(vvv), sizeof(vvv))) err = -1;

		/* What */
		if (err) what = "Cannot read savefile";

		/* Close the file */
		fd_close(fd);
	}

	/* Process file */
	if (!err)
	{
		/* Extract version */
		sf_major = vvv[0];
		sf_minor = vvv[1];
		sf_patch = vvv[2];
		sf_extra = vvv[3];

		/* Clear screen */
		Term_clear();

		if (older_than(OLD_VERSION_MAJOR, OLD_VERSION_MINOR, OLD_VERSION_PATCH))
		{
			err = -1;
			what = "Savefile is too old";
		}
		
		else if (!older_than(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH + 1))
		{
			err = -1;
			what = "Savefile is from the future";
		}
		
		else
		{
			/* Attempt to load */
			err = rd_savefile();

			/* Message (below) */
			if (err) what = "Cannot parse savefile";
		}
	}

	/* Paranoia */
	if (!err)
	{
		/* Invalid turn */
		if (!turn) err = -1;

		/* Message (below) */
		if (err) what = "Broken savefile";
	}

#ifdef VERIFY_TIMESTAMP
	/* Verify timestamp */
	if (!err && !arg_wizard)
	{
		/* Hack -- Verify the timestamp */
		if (sf_when > (statbuf.st_ctime + 100) ||
		    sf_when < (statbuf.st_ctime - 100))
		{
			/* Message */
			what = "Invalid timestamp";

			/* Oops */
			err = -1;
		}
	}
#endif /* VERIFY_TIMESTAMP */


	/* Okay */
	if (!err)
	{
		/* Give a conversion warning */
		if ((version_major != sf_major) ||
		    (version_minor != sf_minor) ||
		    (version_patch != sf_patch))
		{
			/* Message */
			msg_format("Converted a %d.%d.%d savefile.",
			           sf_major, sf_minor, sf_patch);
			message_flush();
		}

		// if Morgoth has lost his crown...
		if ((&a_info[ART_MORGOTH_3])->cur_num == 1)
		{
			// lower Morgoth's protection, remove his light source, increase his will and perception
			(&r_info[R_IDX_MORGOTH])->pd -= 1;
			(&r_info[R_IDX_MORGOTH])->light = 0;
			(&r_info[R_IDX_MORGOTH])->wil += 5;
			(&r_info[R_IDX_MORGOTH])->per += 5;
		}
			
		/* Player is dead */
		if (p_ptr->is_dead)
		{
			/* Cheat death (unless the character retired) */
			if (arg_wizard)
			{
				/*heal the player*/
				hp_player(100, TRUE, TRUE);

				/* Forget death */
				p_ptr->is_dead = FALSE;

				/* A character was loaded */
				character_loaded = TRUE;

				// put the character somewhere sensible
				p_ptr->depth = min_depth();

				// Mark savefile
				p_ptr->noscore |= 0x0001;

				/* Done */
				return (TRUE);
			}

			/* Forget death */
			p_ptr->is_dead = FALSE;

			/* Count lives */
			sf_lives++;

			/* Forget turns */
			turn = 0;
			playerturn = 0;

			/* A dead character was loaded */
			character_loaded_dead = TRUE;////

			/* Done */
			return (TRUE);
		}

		/* A character was loaded */
		character_loaded = TRUE;

		/* Still alive */
		if (p_ptr->chp >= 0)
		{
			/* Reset cause of death */
			my_strcpy(p_ptr->died_from, "(alive and well)", sizeof (p_ptr->died_from));
		}

		// count the artefacts seen for the player
		p_ptr->artefacts = artefact_count();
        
		/* Success */
		return (TRUE);
	}


#ifdef VERIFY_SAVEFILE

	/* Verify savefile usage */
	if (TRUE)
	{
		char temp[1024];

		/* Extract name of lock file */
		my_strcpy(temp, savefile, sizeof(temp));
		my_strcat(temp, ".lok", sizeof(temp));

		/* Grab permissions */
		safe_setuid_grab();

		/* Remove lock */
		fd_kill(temp);

		/* Drop permissions */
		safe_setuid_drop();
	}

#endif /* VERIFY_SAVEFILE */


	/* Message */
	msg_format("Error (%s) reading %d.%d.%d savefile.",
	           what, sf_major, sf_minor, sf_patch);
	message_flush();

	/* Oops */
	return (FALSE);
}

