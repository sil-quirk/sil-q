/* File: spells1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/*
 * Mega-Hack -- count number of monsters killed out of sight
 */
static int death_count;


/*
 * Teleport a monster, normally up to "dis" grids away.
 *
 * Attempt to move the monster at least "dis/2" grids away.
 *
 * But allow variation to prevent infinite loops.
 */
void teleport_away(int m_idx, int dis)
{
	int ny, nx, oy, ox, d, i, min;

	bool look = TRUE;

	monster_type *m_ptr = &mon_list[m_idx];

	/* Paranoia */
	if (!m_ptr->r_idx) return;

	/* Save the old location */
	oy = m_ptr->fy;
	ox = m_ptr->fx;

	/* Minimum distance */
	min = dis / 2;

	/* Look until done */
	while (look)
	{
		/* Verify max distance */
		if (dis > 200) dis = 200;

		/* Try several locations */
		for (i = 0; i < 500; i++)
		{
			/* Pick a (possibly illegal) location */
			while (1)
			{
				ny = rand_spread(oy, dis);
				nx = rand_spread(ox, dis);
				d = distance(oy, ox, ny, nx);
				if ((d >= min) && (d <= dis)) break;
			}

			/* Ignore illegal locations */
			if (!in_bounds_fully(ny, nx)) continue;

			/* Require "empty" floor space */
			if (!cave_empty_bold(ny, nx)) continue;

			/* Hack -- no teleport onto glyph of warding */
			if (cave_feat[ny][nx] == FEAT_GLYPH) continue;

			/* No teleporting into vaults and such */
			/* if (cave_info[ny][nx] & (CAVE_ICKY)) continue; */

			/* This grid looks good */
			look = FALSE;

			/* Stop looking */
			break;
		}

		/* Increase the maximum distance */
		dis = dis * 2;

		/* Decrease the minimum distance */
		min = min / 2;
	}

	/* Sound */
	sound(MSG_TPOTHER);

	/*the monster should re-evaluate their target*/
	m_ptr->target_y = 0;
	m_ptr->target_x = 0;

	/* Swap the monsters */
	monster_swap(oy, ox, ny, nx);
}


/*
 * Teleport the player to a location up to "dis" grids away.
 *
 * If no such spaces are readily available, the distance may increase.
 * Try very hard to move the player at least a quarter that distance.
 */
void teleport_player(int dis)
{
	int x_location_tables [20];
	int y_location_tables [20];
	int spot_counter = 0;

	int py = p_ptr->py;
	int px = p_ptr->px;

	int d, i, min, y, x;

	bool look = TRUE;
	
	/* Minimum distance */
	min = dis / 2;

	/*guage the dungeon size*/
	d = distance(p_ptr->cur_map_hgt, p_ptr->cur_map_wid, 0, 0);

	/*first start with a realistic range*/
	if (dis > d) dis = d;

	/*must have a realistic minimum*/
	if (min > (d * 4 / 10))
	{
		min = (d * 4 / 10);
	}

	/* Look until done */
	while (look)
	{

		/*find the allowable range*/
		int min_y = MAX((py - dis), 0);
		int min_x = MAX((px - dis), 0);
		int max_y = MIN((py + dis), (p_ptr->cur_map_hgt - 1));
		int max_x = MIN((px + dis), (p_ptr->cur_map_wid - 1));

		/* Try several locations */
		for (i = 0; i < 10000; i++)
		{

			/* Pick a (possibly illegal) location */
			y = rand_range(min_y, max_y);
			x = rand_range(min_x, max_x);
			d = distance(py, px, y, x);
			if ((d <= min) || (d >= dis)) continue;

			/*only open floor space*/
			if (!cave_naked_bold(y, x)) continue;

			/* No teleporting into vaults and such */
			//if (cave_info[y][x] & (CAVE_ICKY)) continue;

			/*don't go over size of array*/
			if (spot_counter < 20)
			{
				x_location_tables[spot_counter] = x;
				y_location_tables[spot_counter] = y;

				/*increase the counter*/
				spot_counter++;
			}

			/*we have enough spots, keep looking*/
			if (spot_counter == 20)
			{
				/* This grid looks good */
				look = FALSE;

				/* Stop looking */
				break;
			}
		}

		/*we have enough random spots*/
		if (spot_counter > 3) break;

		/* Increase the maximum distance */
		dis = dis * 2;

		/* Decrease the minimum distance */
		min = min * 6 / 10;

	}

	i = rand_int(spot_counter);

	/* Mark the location */
	x = x_location_tables[i];
	y = y_location_tables[i];

	/* Sound */
	sound(MSG_TELEPORT);

	/* Move player */
	monster_swap(py, px, y, x);

	/* Handle stuff XXX XXX XXX */
	handle_stuff();
}



/*
 * Teleport player to a grid near the given location
 *
 * This function is slightly obsessive about correctness.
 * This function allows teleporting into vaults (!)
 */
void teleport_player_to(int ny, int nx)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int y, x;

	int dis = 0, ctr = 0;

	/* Initialize */
	y = py;
	x = px;

	/* Find a usable location */
	while (1)
	{
		/* Pick a nearby legal location */
		while (1)
		{
			y = rand_spread(ny, dis);
			x = rand_spread(nx, dis);
			if (in_bounds_fully(y, x)) break;
		}

		/* Accept "naked" floor grids */
		if (cave_naked_bold(y, x)) break;

		/* Occasionally advance the distance */
		if (++ctr > (4 * dis * dis + 4 * dis + 1))
		{
			ctr = 0;
			dis++;
		}
	}

	/* Sound */
	sound(MSG_TELEPORT);

	/* Move player */
	monster_swap(py, px, y, x);

	/* Handle stuff XXX XXX XXX */
	handle_stuff();
}


/*
 * Teleport monster to a grid near the given location.  This function is
 * used in the monster spell "TELE_SELF_TO", to allow monsters both to
 * suddenly jump near the character, and to make them "dance" around the
 * character.
 *
 * Usually, monster will teleport to a grid that is not more than 4
 * squares away from the given location, and not adjacent to the given
 * location.  These restrictions are relaxed if necessary.
 *
 * This function allows teleporting into vaults.
 */
void teleport_towards(int oy, int ox, int ny, int nx)
{
	int y, x;

	int dist;
	int ctr = 0;
	int min = 2, max = 4;

	/* Find a usable location */
	while (TRUE)
	{
		/* Pick a nearby legal location */
		while (TRUE)
		{
			y = rand_spread(ny, max);
			x = rand_spread(nx, max);
			if (in_bounds_fully(y, x)) break;
		}

		/* Consider all empty grids */
		if (cave_empty_bold(y, x))
		{
			/*Don't allow monster to teleport onto glyphs*/
			if (cave_feat[y][x] == FEAT_GLYPH) continue;

			/* Calculate distance between target and current grid */
			dist = distance(ny, nx, y, x);

			/* Accept grids that are the right distance away. */
			if ((dist >= min) && (dist <= max)) break;
		}

		/* Occasionally relax the constraints */
		if (++ctr > 15)
		{
			ctr = 0;

			max++;
			if (max > 5) min = 0;
		}
	}

	/* Sound (assumes monster is moving) */
	sound(SOUND_TPOTHER);

	/* Move monster */
	monster_swap(oy, ox, y, x);

	/* Handle stuff XXX XXX XXX */
	handle_stuff();
}




/*
 * Teleport the player one level up or down (random when legal)
 */
void teleport_player_level()
{

	bool go_up = FALSE;
	bool go_down = FALSE;

	if (birth_ironman)
	{
		msg_print("Nothing happens.");
		return;
	}

	if (!p_ptr->depth) go_down = TRUE;

	/*
	 * the bottom of the dungeon.
	 */
	if (p_ptr->depth >= MORGOTH_DEPTH)
	{
		go_up = TRUE;
	}

	/*
	 * the surface.
	 */
	if (p_ptr->depth == 0)
	{
		go_down = TRUE;
	}

	/*We don't have a direction yet, pick one at random*/
	if ((!go_up) && (!go_down))
	{
		if (one_in_(2)) go_up = TRUE;
		else go_down = TRUE;
	}

	/*up*/
	if (go_up == TRUE)
	{
		message(MSG_TPLEVEL, 0, "You rise up through the ceiling.");

		// make a note if the player loses a greater vault
		note_lost_greater_vault();

		/* New depth */
		p_ptr->depth--;

		/* Leaving */
		p_ptr->leaving = TRUE;
	}

	else
	{
		message(MSG_TPLEVEL, 0, "You sink through the floor.");

		// make a note if the player loses a greater vault
		note_lost_greater_vault();

		/* New depth */
		p_ptr->depth++;

		/* Leaving */
		p_ptr->leaving = TRUE;
	}

}






/*
 * Return a color to use for the bolt/ball spells
 */
static byte spell_color(int type)
{
	/* Analyze */
	switch (type)
	{
		case GF_ARROW:		return (TERM_L_UMBER);
		case GF_BOULDER:	return (TERM_SLATE);
		case GF_ACID:		return (TERM_SLATE);
		case GF_ELEC:		return (TERM_BLUE);
		case GF_FIRE:		return (TERM_RED);
		case GF_COLD:		return (TERM_WHITE);
		case GF_POIS:		return (TERM_GREEN);
		case GF_CONFUSION:	return (TERM_L_UMBER);
		case GF_SOUND:		return (TERM_L_WHITE);
		case GF_LIGHT:	    return (TERM_WHITE);
		case GF_DARK_WEAK:	return (TERM_L_DARK);
		case GF_DARK:		return (TERM_L_DARK);
		case GF_IDENTIFY:	return (TERM_WHITE);
		case GF_EARTHQUAKE:	return (TERM_SLATE);
	}

	/* Standard "color" */
	return (TERM_L_WHITE);
}



/*
 * Find the attr/char pair to use for a spell effect
 *
 * It is moving (or has moved) from (x,y) to (nx,ny).
 *
 * If the distance is not "one", we (may) return "*".
 */
static u16b bolt_pict(int y, int x, int ny, int nx, int typ)
{
	int base;

	byte k;

	byte a;
	char c;

	if (!(use_graphics && (arg_graphics == GRAPHICS_DAVID_GERVAIS)))
	{
		/* No motion (*) */
		if ((ny == y) && (nx == x)) base = 0x30;

		/* Vertical (|) */
		else if (nx == x) base = 0x40;

		/* Horizontal (-) */
		else if (ny == y) base = 0x50;

		/* Diagonal (/) */
		else if ((ny-y) == (x-nx)) base = 0x60;

		/* Diagonal (\) */
		else if ((ny-y) == (nx-x)) base = 0x70;

		/* Weird (*) */
		else base = 0x30;

		/* Basic spell color */
		k = spell_color(typ);

		/* Obtain attr/char */
		a = misc_to_attr[base+k];
		c = misc_to_char[base+k];
	}
	else
	{
		int add;

		/* No motion (*) */
		if ((ny == y) && (nx == x)) {base = 0x00; add = 0;}

		/* Vertical (|) */
		else if (nx == x) {base = 0x40; add = 0;}

		/* Horizontal (-) */
		else if (ny == y) {base = 0x40; add = 1;}

		/* Diagonal (/) */
		else if ((ny-y) == (x-nx)) {base = 0x40; add = 2;}

		/* Diagonal (\) */
		else if ((ny-y) == (nx-x)) {base = 0x40; add = 3;}

		/* Weird (*) */
		else {base = 0x00; add = 0;}

		if (typ >= 0x40) k = 0;
		else k = typ;

		/* Obtain attr/char */
		a = misc_to_attr[base+k];
		c = misc_to_char[base+k] + add;
	}

	/* Create pict */
	return (PICT(a,c));
}




/*
 * Decreases players hit points and sets death flag if necessary
 *
 * Invulnerability needs to be changed into a "shield" XXX XXX XXX
 *
 * Hack -- this function allows the user to save (or quit) the game
 * when he dies, since the "You die." message is shown before setting
 * the player to "dead".
 */
void take_hit(int dam, cptr kb_str)
{
	int old_chp = p_ptr->chp;

	int warning = (p_ptr->mhp * op_ptr->hitpoint_warn / 10);

	time_t ct = time((time_t*)0);
	char long_day[40];
	char buf[120];

	/* Paranoia */
	if (p_ptr->is_dead) return;

	/* Disturb */
	disturb(1, 0);

	/* Hurt the player */
	p_ptr->chp -= dam;

	/* Display the hitpoints */
	p_ptr->redraw |= (PR_HP);

	/* Window stuff */
	p_ptr->window |= (PW_PLAYER_0);

	/* Dead player */
	if (p_ptr->chp <= 0)
	{
		/* Hack -- Note death */
		message(MSG_DEATH, 0, "You die.");
		message_flush();
		
		/* Note cause of death */
		if (p_ptr->image == 0)
		{
			my_strcpy(p_ptr->died_from, kb_str, sizeof(p_ptr->died_from));
		}
		else
		{
			strnfmt(p_ptr->died_from, sizeof(p_ptr->died_from), "%s (while hallucinating)", kb_str);
		}

		/* Note death */
		p_ptr->is_dead = TRUE;

		/* Leaving */
		p_ptr->leaving = TRUE;

		/* Write a note */

		/* Get time */
		(void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));

		/* Add note */
		my_strcat(notes_buffer, "\n", sizeof(notes_buffer));
		
		/*killed by */
		sprintf(buf, "Slain by %s.", p_ptr->died_from);
		
		/* Write message */
		do_cmd_note(buf,  p_ptr->depth);
		
		/* date and time*/
		sprintf(buf, "Died on %s.", long_day);
		
		/* Write message */
		do_cmd_note(buf,  p_ptr->depth);
		
		my_strcat(notes_buffer, "\n", sizeof(notes_buffer));

		/* Dead */
		return;
	}

	/* Hitpoint warning */
	if (p_ptr->chp < warning)
	{
		/* Hack -- bell on first notice */
		if (old_chp > warning)
		{
			bell("Low hitpoint warning!");
		}

		/* Message */
		message(MSG_HITPOINT_WARN, 0, "*** LOW HITPOINT WARNING! ***");
		message_flush();
	}

	// Cancel entrancement
	set_entranced(0);
	
}





/*
 * Does a given class of objects (usually) hate acid?
 * Note that acid can either melt or corrode something.
 */
bool hates_acid(const object_type *o_ptr)
{
	/* Analyze the type */
	switch (o_ptr->tval)
	{
		/* Wearable items */
		case TV_ARROW:
		case TV_BOW:
		case TV_SWORD:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			return (TRUE);
		}

		/* Staffs are wood */
		case TV_STAFF:
		{
			return (TRUE);
		}

		/* Ouch */
		case TV_CHEST:
		{
			return (TRUE);
		}

		/* Useless */
		case TV_USELESS:
		{
			return (TRUE);
		}
	}

	return (FALSE);
}


/*
 * Does a given object (usually) hate electricity?
 */
bool hates_elec(const object_type *o_ptr)
{
	switch (o_ptr->tval)
	{
		case TV_RING:
		{
			return (TRUE);
		}
	}

	return (FALSE);
}


/*
 * Does a given object (usually) hate fire?
 * Hafted/Polearm weapons have wooden shafts.
 * Arrows/Bows are mostly wooden.
 */
bool hates_fire(const object_type *o_ptr)
{
	/* Analyze the type */
	switch (o_ptr->tval)
	{
		/* Wearable */
		case TV_ARROW:
		case TV_BOW:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		{
			return (TRUE);
		}

		/* Chests */
		case TV_CHEST:
		{
			return (TRUE);
		}

		/* Torches */
		case TV_LIGHT:
		{
			if (o_ptr->sval == SV_LIGHT_TORCH)	return (TRUE);
			else								return (FALSE);
		}
			
		/* Notes burn */
		case TV_NOTE:
		{
			return (TRUE);
		}

		/* Staffs burn */
		case TV_STAFF:
		{
			return (TRUE);
		}
	}

	return (FALSE);
}


/*
 * Does a given object (usually) hate cold?
 */
bool hates_cold(const object_type *o_ptr)
{
	switch (o_ptr->tval)
	{
		case TV_POTION:
		case TV_FLASK:
		{
			return (TRUE);
		}
	}

	return (FALSE);
}



/*
 * Melt something
 */
static int set_acid_destroy(const object_type *o_ptr)
{
	u32b f1, f2, f3;
	if (!hates_acid(o_ptr)) return (FALSE);
	object_flags(o_ptr, &f1, &f2, &f3);
	if (f3 & (TR3_IGNORE_ACID)) return (FALSE);
	return (TRUE);
}


/*
 * Electrical damage
 */
static int set_elec_destroy(const object_type *o_ptr)
{
	u32b f1, f2, f3;
	if (!hates_elec(o_ptr)) return (FALSE);
	object_flags(o_ptr, &f1, &f2, &f3);
	if (f3 & (TR3_IGNORE_ELEC)) return (FALSE);
	return (TRUE);
}


/*
 * Burn something
 */
static int set_fire_destroy(const object_type *o_ptr)
{
	u32b f1, f2, f3;
	if (!hates_fire(o_ptr)) return (FALSE);
	object_flags(o_ptr, &f1, &f2, &f3);
	if (f3 & (TR3_IGNORE_FIRE)) return (FALSE);
	return (TRUE);
}


/*
 * Freeze things
 */
static int set_cold_destroy(const object_type *o_ptr)
{
	u32b f1, f2, f3;
	if (!hates_cold(o_ptr)) return (FALSE);
	object_flags(o_ptr, &f1, &f2, &f3);
	if (f3 & (TR3_IGNORE_COLD)) return (FALSE);
	return (TRUE);
}




/*
 * This seems like a pretty standard "typedef"
 */
typedef int (*inven_func)(const object_type *);

/*
 * Destroys a type of item on a given percent chance
 * Note that missiles are no longer necessarily all destroyed
 *
 * Returns number of items destroyed.
 */
static int inven_damage(inven_func typ, int perc, int resistance)
{
	int i, j, k, amt;

	object_type *o_ptr;

	char o_name[80];

	/* Count the casualties */
	k = 0;

	/* Scan through the slots backwards */
	for (i = 0; i < INVEN_PACK; i++)
	{
		o_ptr = &inventory[i];

		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;

		/* Hack -- for now, skip artefacts */
		if (artefact_p(o_ptr)) continue;

		/* Give this item slot a shot at death */
		if ((*typ)(o_ptr))
		{
			/* Count the casualties */
			for (amt = j = 0; j < o_ptr->number; ++j)
			{
				if (percent_chance(perc) && ((resistance < 0) || one_in_(resistance))) amt++;
			}

			/* Some casualities */
			if (amt)
			{
				int old_charges = 0;

				/*hack, make sure the proper number of charges is displayed in the message*/
				if (((o_ptr->tval == TV_STAFF) || (o_ptr->tval == TV_HORN)) && (amt < o_ptr->number))
				{
					/*save the number of charges*/
					old_charges = o_ptr->pval;

					/*distribute the charges*/
					o_ptr->pval -= o_ptr->pval * amt / o_ptr->number;

					o_ptr->pval = old_charges - o_ptr->pval;
				}

				/* Get a description */
				object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 3);

				/* Message */
				msg_format("%sour %s (%c) %s destroyed!",
				           ((o_ptr->number > 1) ?
				            ((amt == o_ptr->number) ? "All of y" :
				             (amt > 1 ? "Some of y" : "One of y")) : "Y"),
				           o_name, index_to_label(i),
				           ((amt > 1) ? "were" : "was"));

				/*hack, restore the proper number of charges after the messages have printed
	 			 * so the proper number of charges are destroyed*/
				 if (old_charges) o_ptr->pval = old_charges;

				/* Hack -- If staffs are destroyed, the total maximum
				 * timeout or charges of the stack needs to be reduced,
				 * unless all the items are being destroyed. -LM-
				 */
				if ((o_ptr->tval == TV_STAFF) && (amt < o_ptr->number))
				{
					o_ptr->pval -= o_ptr->pval * amt / o_ptr->number;
				}

				/* Destroy "amt" items */
				inven_item_increase(i, -amt);
				inven_item_optimize(i);

				/* Count the casualties */
				k += amt;
			}
		}
	}

	/* Return the casualty count */
	return (k);
}




/*
 * Acid has hit the player, attempt to affect some armor.
 */
static int damage_armour(void)
{
	object_type *o_ptr = NULL;

	u32b f1, f2, f3;

	char o_name[80];
	
	int item = INVEN_BODY; // a default value to soothe compilation warnings

	/* Pick a (possibly empty) inventory slot */
	switch (dieroll(6))
	{
		case 1: item = INVEN_BODY; break;
		case 2: item = INVEN_ARM; break;
		case 3: item = INVEN_OUTER; break;
		case 4: item = INVEN_HANDS; break;
		case 5: item = INVEN_HEAD; break;
		case 6: item = INVEN_FEET; break;
	}

	o_ptr = &inventory[item];

	/* Nothing to damage */
	if (!o_ptr->k_idx || ((item == INVEN_ARM) && (o_ptr->tval != TV_SHIELD))) return (FALSE);

	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);

	/* Describe */
	object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);

	/* Object resists */
	if (f3 & (TR3_IGNORE_ACID))
	{
		msg_format("Your %s is unaffected!", o_name);

		return (TRUE);
	}

	/* No damage left to be done */
	if ((o_ptr->ps <= 0) && (o_ptr->evn <= 0))
	{
		/* Destroy the item */
		inven_item_increase(item, -1);
		inven_item_optimize(item);

		/* Message */
		msg_format("Your %s is destroyed!", o_name);
	}
	else if (o_ptr->evn >= 0)
	{
		/* Damage the item */
		o_ptr->evn--;
	
		/* Message */
		msg_format("Your %s is damaged!", o_name);
	}
	else
	{
		/* Damage the item */
		o_ptr->ps--;
	
		/* Message */
		msg_format("Your %s is damaged!", o_name);
	}

	/* Calculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Window stuff */
	p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);

	/* Item was damaged */
	return (TRUE);
}


/*
 * Hurt the player with Acid
 */
void acid_dam(int dam, cptr kb_str)
{
	int inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;
	
	/* Abort if no damage to receive */
	if (dam <= 0) return;

	/* Damage armour */
	damage_armour();

	/* Take damage */
	take_hit(dam, kb_str);

	/* Inventory damage */
	inven_damage(set_acid_destroy, inv, 1);
}


/*
 * Hurt the player with electricity
 */
void elec_dam(int dam, cptr kb_str)
{
	int inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;

	/* Abort if no damage to receive */
	if (dam <= 0) return;

	/* Take damage */
	take_hit(dam, kb_str);

	/* Inventory damage */
	inven_damage(set_elec_destroy, inv, 1);
}


/*
 * The player's fire resistance depends on equipment and temporary effects
 */
extern int resist_fire(void)
{
	int res = p_ptr->resist_fire;
	
	if (p_ptr->oppose_fire) res++;
	
	// represent overall vulnerabilities as negatives of the normal range
	if (res < 1) res -= 2;
	
	return (res);
}

/*
 * The player's cold resistance depends on equipment and temporary effects
 */
extern int resist_cold(void)
{
	int res = p_ptr->resist_cold;
	
	if (p_ptr->oppose_cold) res++;
	
	// represent overall vulnerabilities as negatives of the normal range
	if (res < 1) res -= 2;
		
	return (res);
}

/*
 * The player's poison resistance depends on equipment and temporary effects
 */
extern int resist_pois(void)
{
	int res = p_ptr->resist_pois;
	
	if (p_ptr->oppose_pois) res++;
	
	// represent overall vulnerabilities as negatives of the normal range
	if (res < 1) res -= 2;
	
	return (res);
}

/*
 * The player's dark resistance is strictly dependent
 * on the brightness of their square
 */
extern int resist_dark(void)
{
	int res = cave_light[p_ptr->py][p_ptr->px];
	
	if (res < 1) res = 1;
	
	return (res);
}


/*
 * Hurt the player with Fire
 */
void fire_dam_mixed(int dam, cptr kb_str)
{
	int inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;
		
	/* Abort if no damage to receive */
	if (dam <= 0) return;
	
	/* Take damage */
	take_hit(dam, kb_str);
	
	/* Inventory damage */
	inven_damage(set_fire_destroy, inv, resist_fire());

	// possibly identify relevant items
	ident_resist(TR2_RES_FIRE);
}

/*
 * Hurt the player with Fire
 */
void fire_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
	int dam = damroll(dd, ds);
	int net_dam;
	int prt = protection_roll(GF_FIRE, FALSE);
	int inv;
	int resistance = resist_fire();
			
	if (resistance > 0) net_dam = dam / resistance;
	else				net_dam = dam * (-resistance);

	net_dam = net_dam > prt ? net_dam - prt : 0;

	inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;
	
	if (update_rolls)
	{
		update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_FIRE, FALSE);
	}
	
	/* Abort if no damage to receive */
	if (net_dam <= 0) return;
		
	/* Take damage */
	take_hit(net_dam, kb_str);
	
	/* Inventory damage */
	inven_damage(set_fire_destroy, inv, resistance);

	// possibly identify relevant items
	ident_resist(TR2_RES_FIRE);
}

/*
 * Hurt the player with Cold
 */
void cold_dam_mixed(int dam, cptr kb_str)
{
	int inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;
	
	/* Abort if no damage to receive */
	if (dam <= 0) return;
	
	/* Take damage */
	take_hit(dam, kb_str);
	
	/* Inventory damage */
	inven_damage(set_cold_destroy, inv, resist_cold());
	
	// possibly identify relevant items
	ident_resist(TR2_RES_COLD);
}


/*
 * Hurt the player with Cold
 */
void cold_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
	int dam = damroll(dd, ds);
	int net_dam;
	int prt = protection_roll(GF_COLD, FALSE);
	int inv;
	int resistance = resist_cold();
		
	if (resistance > 0) net_dam = dam / resistance;
	else				net_dam = dam * (-resistance);

	net_dam = net_dam > prt ? net_dam - prt : 0;
	
	inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;
	
	if (update_rolls)
	{
		update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_COLD, FALSE);
	}
	
	/* Abort if no damage to receive */
	if (net_dam <= 0) return;
	
	/* Take damage */
	take_hit(net_dam, kb_str);
	
	/* Inventory damage */
	inven_damage(set_cold_destroy, inv, resistance);
	
	// possibly identify relevant items
	ident_resist(TR2_RES_COLD);
}

/*
 * Hurt the player with Darkness from melee
 */
void dark_dam_mixed(int dam, cptr kb_str)
{
	/* Abort if no damage to receive */
	if (dam <= 0) return;
	
	/* Take damage */
	take_hit(dam, kb_str);
}


/*
 * Hurt the player with Darkness from breaths
 */
void dark_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
	int dam = damroll(dd, ds);
	int net_dam;
	int prt = protection_roll(GF_DARK, FALSE);
	int resistance = resist_dark();
	
	net_dam = dam / resistance;
	net_dam = net_dam > prt ? net_dam - prt : 0;
	
	if (update_rolls)
	{
		update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_DARK, FALSE);
	}

	// 'pure' darkness attacks can also blind
	if (one_in_(resistance) && allow_player_blind(NULL))
	{  
		(void)set_blind(p_ptr->blind + damroll(2,4));
	}
	
	/* Abort if no damage to receive */
	if (net_dam <= 0) return;
	
	/* Take damage */
	take_hit(net_dam, kb_str);
}


/*
 * Poison the player from melee
 */
void pois_dam_mixed(int dam)
{
	/* Abort if no damage to receive */
	if (dam <= 0) return;
	
	/* Set poison counter */
	set_poisoned(p_ptr->poisoned + dam);
	
	// possibly identify relevant items
	ident_resist(TR2_RES_POIS);
}


/*
 * Poison the player from breaths etc
 */
void pois_dam_pure(int dd, int ds, bool update_rolls)
{
	int dam = damroll(dd, ds);
	int net_dam;
	int prt = protection_roll(GF_POIS, FALSE);
	int resistance = resist_pois();
	
	if (resistance > 0) net_dam = dam / resistance;
	else				net_dam = dam * (-resistance);

	net_dam = net_dam > prt ? net_dam - prt : 0;
	
	if (update_rolls)
	{
		update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_POIS, FALSE);
	}
	
	/* Abort if no damage to receive */
	if (net_dam <= 0) return;

	/* Set poison counter */
	set_poisoned(p_ptr->poisoned + net_dam);
	
	// possibly identify relevant items
	ident_resist(TR2_RES_POIS);
}


/*
 * Increase a stat by one randomized level
 *
 * Most code will "restore" a stat before calling this function,
 * in particular, stat potions will always restore the stat and
 * then increase the fully restored value.
 */
bool inc_stat(int stat)
{
	/* Cannot go above BASE_STAT_MAX */
	if (p_ptr->stat_base[stat] < BASE_STAT_MAX)
	{
		p_ptr->stat_base[stat]++;

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Redisplay the stats later */
		p_ptr->redraw |= (PR_STATS);

		/* Success */
		return (TRUE);
	}

	/* Nothing to gain */
	return (FALSE);
}



/*
 * Decreases a stat by a number of points.
 *
 * Note that "permanent" means that the *given* amount is permanent,
 * not that the new value becomes permanent.
 */
bool dec_stat(int stat, int amount, bool permanent)
{
	int result = FALSE;

	/* Temporary damage */
	if (!permanent)
	{
		p_ptr->stat_drain[stat] -= amount;
		result = TRUE;
	}

	/* Permanent damage */
	if (permanent && (p_ptr->stat_base[stat] > 0))
	{
		if (amount > p_ptr->stat_base[stat])
			p_ptr->stat_base[stat] = 0;
		else
			p_ptr->stat_base[stat] -= amount;
			
		result = TRUE;
	}

	/* Apply changes */
	if (result)
	{
		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Redisplay the stats later */
		p_ptr->redraw |= (PR_STATS);
	}

	/* Done */
	return (result);
}


/*
 * Restore a stat by the number of points.
 * Return TRUE only if this actually makes a difference.
 */
bool res_stat(int stat, int points)
{
	/* Restore if needed */
	if (p_ptr->stat_drain[stat] < 0)
	{
		/* Restore */
		p_ptr->stat_drain[stat] += points;
		
		if (p_ptr->stat_drain[stat] > 0) p_ptr->stat_drain[stat] = 0;

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Redisplay the stats later */
		p_ptr->redraw |= (PR_STATS);

		/* Success */
		return (TRUE);
	}

	/* Nothing to restore */
	return (FALSE);
}

/*
 * Inflict disease on the character.
 */
void disease(int *damage)
{
	int con, attempts;
	int i;

	/* Get current constitution */
	con = p_ptr->stat_use[A_CON];

	/* Adjust damage and choose message based on constitution */
	if (con < -2)
	{
		msg_print("You feel deathly ill.");
		*damage *= 2;
	}

	else if (con < 0)
	{
		msg_print("You feel seriously ill.");
	}

	else if (con < 2)
	{
		msg_print("You feel quite ill.");
		*damage = *damage * 2 / 3;
	}

	else if (con < 5)
	{
		msg_print("You feel ill.");
		*damage /= 2;
	}

	else if (con < 7)
	{
		msg_print("You feel sick.");
		*damage /= 3;
	}

	else
	{
		msg_print("You feel a bit sick.");
		*damage /= 4;
	}

	/* Infect the character (fully cumulative) */
	set_poisoned(p_ptr->poisoned + *damage + 1);

	/* Determine # of stat-reduction attempts */
	attempts = (5 + *damage) / 5;

	/* Attack stats */
	for (i = 0; i < attempts; i++)
	{
		/* Each attempt has a 10% chance of success */
		if (one_in_(10))
		{
			/* Damage a random stat */
			(void)do_dec_stat(rand_int(A_MAX), NULL);
		}
	}
}



/*
 * Apply disenchantment to the player's stuff
 *
 * This function is also called from the "melee" code.
 *
 * The "mode" is currently unused.
 *
 * Return "TRUE" if the player notices anything.
 *
 * Sil-y: this presently brings att, evn, dd, ds, pd, ds down towards their base values by one point each
 */
bool apply_disenchant(int mode)
{
	int t = 0;

	object_type *o_ptr;

	object_kind *k_ptr;

	char o_name[80];


	/* Unused parameter */
	(void)mode;

	/* Pick a random slot */
	switch (dieroll(8))
	{
		case 1: t = INVEN_WIELD; break;
		case 2: t = INVEN_BOW; break;
		case 3: t = INVEN_BODY; break;
		case 4: t = INVEN_OUTER; break;
		case 5: t = INVEN_ARM; break;
		case 6: t = INVEN_HEAD; break;
		case 7: t = INVEN_HANDS; break;
		case 8: t = INVEN_FEET; break;
	}

	/* Get the item */
	o_ptr = &inventory[t];

	k_ptr = &k_info[o_ptr->k_idx];

	/* No item, nothing happens */
	if (!o_ptr->k_idx) return (FALSE);

	/* Check to see if it is disenchantable */

	/* Describe the object */
	object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);

	/* Artefacts have 60% chance to resist */
	if (artefact_p(o_ptr) && percent_chance(60))
	{
		/* Message */
		msg_format("Your %s (%c) resist%s disenchantment!",
		           o_name, index_to_label(t),
		           ((o_ptr->number != 1) ? "" : "s"));

		/* Notice */
		return (TRUE);
	}

	/* Do the disenchanting */
	if (o_ptr->att > k_ptr->att) o_ptr->att--;
	if (o_ptr->evn > k_ptr->evn) o_ptr->evn--;
	if (o_ptr->ds > k_ptr->ds) o_ptr->ds--;
	if (o_ptr->dd > k_ptr->dd) o_ptr->dd--;
	if (o_ptr->ps > k_ptr->ps) o_ptr->ps--;
	if (o_ptr->pd > k_ptr->pd) o_ptr->pd--;

	msg_format("Your %s (%c) %s disenchanted!",
	           o_name, index_to_label(t),
	           ((o_ptr->number != 1) ? "were" : "was"));

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Window stuff */
	p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);

	/* Notice */
	return (TRUE);
}


/*
 * Mega-Hack -- track "affected" monsters (see "project()" comments)
 */
static int project_m_n;
static int project_m_x;
static int project_m_y;


/*
 * Magically close/lock/restore a door at a particular grid
 */
bool lock_door(int y, int x, int power)
{
    int lock_level;
    int obvious = FALSE;
    
    if (cave_feat[y][x] == FEAT_BROKEN)  power -= 10;
    
    if ((power > 0) && (cave_m_idx[y][x] == 0))
    {
        if (cave_known_closed_door_bold(y,x) ||
            (cave_feat[y][x] == FEAT_OPEN) || (cave_feat[y][x] == FEAT_BROKEN) )
        {
            if ((cave_feat[y][x] == FEAT_OPEN) || (cave_feat[y][x] == FEAT_BROKEN))
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD);
                
                obvious = TRUE;
                
                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The door slams shut.");
                }
                else
                {
                    msg_print("You hear a door slam shut.");
                }
            }
            
            // lock the door more firmly than it was before
            lock_level = cave_feat[y][x] - FEAT_DOOR_HEAD + power/2;
            if (lock_level > 7)
            {
                lock_level = 7;
            }
            
            if (cave_feat[y][x] != FEAT_DOOR_HEAD + lock_level)
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD + lock_level);
                
                msg_print("You hear a 'click'.");
            }
            
            /* Update the flow code and visuals */
            p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
            
        }
    }
    
    return (obvious);
}


/*
 * We are called from "project()" to "damage" terrain features
 *
 * We are called both for "beam" effects and "ball" effects.
 *
 * The "r" parameter is the "distance from ground zero".
 *
 * Note that we determine if the player can "see" anything that happens
 * by taking into account: blindness, line-of-sight, and illumination.
 *
 * We return "TRUE" if the effect of the projection is "obvious".
 *
 * Hack -- We also "see" grids which are "memorized".
 *
 * Perhaps we should affect doors and/or walls.
 */
static bool project_f(int who, int y, int x, int dist, int dd, int ds, int dif, int typ)
{

	bool obvious = FALSE;
	monster_type *who_ptr = (who == -1) ? PLAYER : &mon_list[who]; // Sil-y

	/* Unused parameters */
	(void)dist;
	(void)dd;
	(void)ds;


	/* Analyze the type */
	switch (typ)
	{
		/* Ignore most effects */

		/* Destroy Traps */
		case GF_KILL_TRAP:
		{
			/* Destroy traps */
			if (cave_trap_bold(y,x))
			{
				/* Check line of sight */
				if (player_has_los_bold(y, x) && !cave_floorlike_bold(y,x))
				{
					obvious = TRUE;
				}

				/* Forget the trap */
				cave_info[y][x] &= ~(CAVE_MARK);

				/* Destroy the trap */
				cave_set_feat(y, x, FEAT_FLOOR);
			}

			break;
		}

		/* unlock/open/break Doors */
		case GF_KILL_DOOR:
		{
			if (cave_known_closed_door_bold(y,x))
			{
				int result = skill_check(who_ptr, dif, 0, NULL);
				
				if (result <= 0)
				{
					/* Do nothing */
				} 
				else if (result <= 5)
				{
					/* Unlock the door */
					cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);				

					msg_print("You hear a 'click'.");
				} 
				else if (result <= 10)
				{
					/* Forget the door */
					//cave_info[y][x] &= ~(CAVE_MARK);
					
					/* Open the door */
					cave_set_feat(y, x, FEAT_OPEN);		
							
					/* Update the flow code */
					p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
				
					obvious = TRUE;

					if (cave_info[y][x] & (CAVE_SEEN))
					{
						msg_print("The door flies open.");
					}
					else
					{
						msg_print("You hear a door burst open.");
					}
				}
				else
				{
					/* Break the door */
					cave_set_feat(y, x, FEAT_BROKEN);
									
					/* Update the flow code */
					p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
				
					obvious = TRUE;

					if (cave_info[y][x] & (CAVE_SEEN))
					{
						msg_print("The door is ripped from its hinges.");
					}
					else
					{
						msg_print("You hear a door burst open.");
					}
				}
			}

			if (cave_feat[y][x] == FEAT_RUBBLE)
			{
				int result = skill_check(who_ptr, dif, 0, NULL);

				if (result <= 0)
				{
					/* Do nothing */
				}
				
				else
				{
					/* Disperse the rubble */
					cave_set_feat(y, x, FEAT_FLOOR);
					
					obvious = TRUE;
					
					/* Update the flow code */
					p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
					
					if (cave_info[y][x] & (CAVE_SEEN))
					{
						msg_print("The rubble is scattered across the floor.");
					}
					else
					{
						msg_print("You hear a loud rumbling.");
					}				
				}
			}

			break;
		}

		/* Destroy walls (and doors) */
		case GF_KILL_WALL:
		{
			/* Non-walls (etc) */
			if (cave_floor_bold(y, x)) break;

			/* Permanent walls */
			if (cave_feat[y][x] == FEAT_WALL_PERM) break;

            /* Granite */
            if (cave_feat[y][x] >= FEAT_WALL_EXTRA)
            {
                // skill check of Will vs 10
                if (skill_check(PLAYER, dif, 10, NULL) > 0)
                {
                    /* Message */
                    if (cave_info[y][x] & (CAVE_MARK))
                    {
                        msg_print("The wall shatters!");
                        obvious = TRUE;
                    }
                    
                    /* Forget the wall */
                    cave_info[y][x] &= ~(CAVE_MARK);
                    
                    /* Destroy the wall */
                    cave_set_feat(y, x, FEAT_RUBBLE);
                }
                
                // if Will check fails
                else
                {
                    /* Message */
                    if (cave_info[y][x] & (CAVE_MARK))
                    {
                        msg_print("You fail to blow hard enough to shatter the wall.");
                        obvious = TRUE;
                    }
                }
            }
            
            /* Quartz */
            else if (cave_feat[y][x] >= FEAT_QUARTZ)
            {
                // skill check of Will vs 10
                if (skill_check(PLAYER, dif, 10, NULL) > 0)
                {
                    /* Message */
                    if (cave_info[y][x] & (CAVE_MARK))
                    {
                        msg_print("The vein shatters!");
                        obvious = TRUE;
                    }
                    
                    /* Forget the wall */
                    cave_info[y][x] &= ~(CAVE_MARK);
                    
                    /* Destroy the wall */
                    cave_set_feat(y, x, FEAT_RUBBLE);
                }
                
                // if Will check fails
                else
                {
                    /* Message */
                    if (cave_info[y][x] & (CAVE_MARK))
                    {
                        msg_print("You fail to blow hard enough to shatter the quartz.");
                        obvious = TRUE;
                    }
                }
            }
            
            /* Rubble */
            else if (cave_feat[y][x] == FEAT_RUBBLE)
            {
                // skill check of Will vs 10
                if (skill_check(PLAYER, dif, 10, NULL) > 0)
                {
                    /* Message */
                    if (cave_info[y][x] & (CAVE_MARK))
                    {
                        msg_print("The rubble is blown away!");
                        obvious = TRUE;
                    }
                    
                    /* Forget the wall */
                    cave_info[y][x] &= ~(CAVE_MARK);
                    
                    /* Destroy the rubble */
                    cave_set_feat(y, x, FEAT_FLOOR);
                }
                
                // if Will check fails
                else
                {
                    /* Message */
                    if (cave_info[y][x] & (CAVE_MARK))
                    {
                        msg_print("You fail to blow hard enough to smash the rubble.");
                        obvious = TRUE;
                    }
                }
            }
            
            /* Destroy doors (and secret doors) */
            else if (cave_any_closed_door_bold(y,x))
            {
                // skill check of Will vs 10
                if (skill_check(PLAYER, dif, 10, NULL) > 0)
                {
                    /* Hack -- special message */
                    if (cave_info[y][x] & (CAVE_MARK))
                    {
                        msg_print("The door is blown from its hinges!");
                        obvious = TRUE;
                    }
                    
                    /* Forget the wall */
                    cave_info[y][x] &= ~(CAVE_MARK);
                    
                    /* Destroy the feature */
                    cave_set_feat(y, x, FEAT_BROKEN);
                }
            
                // if Will check fails
                else
                {
                    /* Hack -- special message */
                    if (cave_info[y][x] & (CAVE_MARK))
                    {
                        msg_print("You fail to blow hard enough to force the door open.");
                        obvious = TRUE;
                    }
                }
            }

			/* Update the visuals */
			p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

			break;
		}

		/* Lock Doors */
		case GF_LOCK_DOOR:
		{
            obvious = lock_door(y, x, skill_check(who_ptr, dif, 0, NULL));

			break;
		}

		/* Lite up the grid */
		case GF_LIGHT:
		{
			// Must make sure it is viewable (passwall was only used to guarantee wall lighting)
			if (cave_info[y][x] & (CAVE_VIEW))
			{
				/* Turn on the light */
				cave_info[y][x] |= (CAVE_GLOW);
			}

			/* Grid is in line of sight */
			if (player_has_los_bold(y, x))
			{
				if (!p_ptr->blind)
				{
					/* Observe */
					obvious = TRUE;
				}

				/* Fully update the visuals */
				p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
			}

			break;
		}

		/* Darken the grid */
		case GF_DARK_WEAK:
		case GF_DARK:
		{
			if (cave_info[y][x] & (CAVE_GLOW))
			{
				/* Turn off the light */
				cave_info[y][x] &= ~(CAVE_GLOW);
				
				/* Hack -- Forget "boring" grids */
				if (cave_floorlike_bold(y,x))
				{
					/* Forget */
					cave_info[y][x] &= ~(CAVE_MARK);
				}
				/* Grid is in line of sight */
				if (player_has_los_bold(y, x))
				{
					/* Observe */
					obvious = TRUE;
					
					/* Fully update the visuals */
					p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
				}
			}

			/* All done */
			break;
		}
	}

	/* Return "Anything seen?" */
	return (obvious);
}



/*
 * We are called from "project()" to "damage" objects
 *
 * We are called both for "beam" effects and "ball" effects.
 *
 * Perhaps we should only SOMETIMES damage things on the ground.
 *
 * The "r" parameter is the "distance from ground zero".
 *
 * Note that we determine if the player can "see" anything that happens
 * by taking into account: blindness, line-of-sight, and illumination.
 *
 * Hack -- We also "see" objects which are "memorized".
 *
 * We return "TRUE" if the effect of the projection is "obvious".
 */
static bool project_o(int who, int y, int x, int dd, int ds, int dif, int typ)
{
	s16b this_o_idx, next_o_idx = 0;

	bool obvious = FALSE;

	u32b f1, f2, f3;

	char o_name[80];

	/* Unused parameters */
	(void)who;
	(void)dif;

	/* Scan all objects in the grid */
	for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr;

		bool is_art = FALSE;
		bool ignore = FALSE;
		bool plural = FALSE;
		bool do_kill = FALSE;

		cptr note_kill = NULL;

		// Sil-y: previously used damage to see if items were broken, now just ignoring damage
		//int dam = damroll(dd, ds);
		(void) dd; // cast to soothe compiler warnings
		(void) ds; // cast to soothe compiler warnings

		/* Get the object */
		o_ptr = &o_list[this_o_idx];

		/* Get the next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Extract the flags */
		object_flags(o_ptr, &f1, &f2, &f3);

		/* Get the "plural"-ness */
		if (o_ptr->number > 1) plural = TRUE;

		/* Check for artefact */
		if (artefact_p(o_ptr)) is_art = TRUE;

		/* Analyze the type */
		switch (typ)
		{
			/* Acid -- Lots of things */
			case GF_ACID:
			{
				if (hates_acid(o_ptr))
				{
					do_kill = TRUE;
					note_kill = (plural ? " melt!" : " melts!");
					if (f3 & (TR3_IGNORE_ACID)) ignore = TRUE;
				}
				break;
			}

			/* Elec -- Rings */
			case GF_ELEC:
			{
				if (hates_elec(o_ptr))
				{
					do_kill = TRUE;
					note_kill = (plural ? " are destroyed!" : " is destroyed!");
					if (f3 & (TR3_IGNORE_ELEC)) ignore = TRUE;
				}
				break;
			}

			/* Fire -- Flammable objects */
			case GF_FIRE:
			{
				if (hates_fire(o_ptr))
				{
					do_kill = TRUE;
					note_kill = (plural ? " burn up!" : " burns up!");
					if (f3 & (TR3_IGNORE_FIRE)) ignore = TRUE;
				}
				break;
			}

			/* Cold -- potions and flasks */
			case GF_COLD:
			{
				if (hates_cold(o_ptr))
				{
					note_kill = (plural ? " shatter!" : " shatters!");
					do_kill = TRUE;
					if (f3 & (TR3_IGNORE_COLD)) ignore = TRUE;
				}
				break;
			}

			/* Hack -- break potions and such */
			case GF_SOUND:
			case GF_EARTHQUAKE:
			{
				if (hates_cold(o_ptr))
				{
					note_kill = (plural ? " shatter!" : " shatters!");
					do_kill = TRUE;
				}
				break;
			}

			/* Unlock chests */
			case GF_KILL_TRAP:
			case GF_KILL_DOOR:
			{
				/* Chests are noticed only if trapped or locked */
				if (o_ptr->tval == TV_CHEST)
				{
					/* Disarm/Unlock traps */
					if (o_ptr->pval > 0)
					{
						/* Disarm or Unlock */
						o_ptr->pval = (0 - o_ptr->pval);

						/* Identify */
						object_known(o_ptr);
					}
				}

				break;
			}

			/* Mass-identify */
			case GF_IDENTIFY:
			{
			  	int squelch;

				/* Ignore hidden objects */
			  	if (!o_ptr->marked) continue;

				/* Ignore known objects */
				if (object_known_p(o_ptr)) continue;

			  	/* Identify object and get squelch setting */
				/* Note the first argument */
			  	squelch = do_ident_item(-1, o_ptr);

				/* Redraw purple dots */
				lite_spot(y, x);

				/* Squelch? */
				if (squelch == SQUELCH_YES) do_kill = TRUE;

 				break;
			}
		}

		/* Attempt to destroy the object */
		if (do_kill)
		{
			/* Effect "observed" */
			if (o_ptr->marked)
			{
				obvious = TRUE;
				object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);
			}

			/* Artefacts, and other objects, get to resist */
			if (is_art || ignore)
			{
				/* Observe the resist */
				if (o_ptr->marked)
				{
					msg_format("The %s %s unaffected!",
					           o_name, (plural ? "are" : "is"));
				}
			}

			/* Kill it */
			else
			{
				/* Describe if needed */
				if (o_ptr->marked && note_kill)
				{
					msg_format("The %s%s", o_name, note_kill);
				}

				/* Delete the object */
				delete_object_idx(this_o_idx);

				/* Redraw */
				lite_spot(y, x);
			}
		}

	}
	
	/* Return "Anything seen?" */
	return (obvious);
}



/*
 * Helper function for "project()" below.
 *
 * Handle a beam/bolt/ball/arc causing damage to a monster.
 *
 * This routine takes a "source monster" (by index) which is mostly used to
 * determine if the player is causing the damage, and a "radius" (see below),
 * which is used to decrease the power of explosions with distance, and a
 * location, via integers which are modified by certain types of attacks
 * (polymorph and teleport being the obvious ones), a default damage, which
 * is modified as needed based on various properties, and finally a "damage
 * type" (see below).
 *
 * Note that this routine can handle "no damage" attacks (like teleport) by
 * taking a "zero" damage, and can even take "parameters" to attacks (like
 * confuse) by accepting a "damage", using it to calculate the effect, and
 * then setting the damage to zero.  Note that the "damage" parameter is
 * lessened by two dice for each square of distance from the center.
 *
 * Note that "polymorph" is dangerous, since a failure in "place_monster()"'
 * may result in a dereference of an invalid pointer.  XXX XXX XXX
 *
 * In this function, "result" messages are postponed until the end, where
 * the "note" string is appended to the monster name, if not NULL.  So,
 * to make a spell have "no effect" just set "note" to NULL.  You should
 * also set "notice" to FALSE, or the player will learn what the spell does.
 *
 * We attempt to return "TRUE" if the player saw anything "useful" happen.
 */
static bool project_m(int who, int y, int x, int dd, int ds, int dif, int typ, u32b flg)
{
	int tmp;
	bool suppress_message = FALSE;

	monster_type *m_ptr;
	monster_race *r_ptr;
	monster_lore *l_ptr;

	monster_type *who_ptr = (who == -1) ? PLAYER : &mon_list[who]; // Sil-y
	bool who_vis = (who == -1) ? TRUE : who_ptr->ml;

	cptr name;
	
	int dam = damroll(dd, ds);

	// Monster's skill modifier
	int resistance;
	
	// Result of opposed check
	int result;

	/* Is the monster "seen"? */
	bool seen = FALSE;

	/* Were the effects "obvious" (if seen)? */
	bool obvious = FALSE;

	/* Were the effects "irrelevant"? */
	bool skipped = FALSE;
	
	/* Does it alert the monster */
	bool alerting = TRUE;

	/* Polymorph setting (true or false) */
	int do_poly = 0;

	/* Teleport setting (max distance) */
	int do_dist = 0;

	/* Confusion setting (amount to confuse) */
	int do_conf = 0;

	/* Stunning setting (amount to stun) */
	int do_stun = 0;

	/* Slow setting (amount to haste) */
	int do_slow = 0;

	/* Haste setting (amount to haste) */
	int do_haste = 0;

	/* Sleep amount (amount to sleep) */
	int do_sleep = 0;

	/* Fear amount (amount to fear) */
	int do_fear = 0;

	/* Hold the monster name */
	char m_name[80];

	/* Assume no note */
	cptr note = NULL;

	/* Assume a default death */
	cptr note_dies = " dies.";

	/* Unused parameter*/
	(void)flg;

	/* Walls protect monsters */
	if (!cave_floor_bold(y,x)) return (FALSE);

	/* No monster here */
	if (!(cave_m_idx[y][x] > 0)) return (FALSE);

	/* Never affect projector */
	if (cave_m_idx[y][x] == who) return (FALSE);

	/* Obtain monster info */
	m_ptr = &mon_list[cave_m_idx[y][x]];
	r_ptr = &r_info[m_ptr->r_idx];
	l_ptr = &l_list[m_ptr->r_idx];
	name = (r_name + r_ptr->name);
	if (m_ptr->ml) seen = TRUE;

	/* Get the monster name*/
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	/* Some monsters get "destroyed" */
	if (monster_nonliving(r_ptr))
	{
		/* Special note at death */
		note_dies = " is destroyed.";
	}

	/* Monster goes active */
	m_ptr->mflag |= (MFLAG_ACTV);

	/*Mark the monster as attacked by the player*/
	if (who < 0) m_ptr->mflag |= (MFLAG_HIT_BY_RANGED);

	/* Analyze the damage type */
	switch (typ)
	{
		/* Acid */
		case GF_ACID:
		{
			if (seen) obvious = TRUE;
			break;
		}

		/* Electricity */
		case GF_ELEC:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_RES_ELEC))
			{
				note = " resists.";
				dam = 0;
				if (seen) l_ptr->flags3 |= (RF3_RES_ELEC);
			}
			break;
		}

		/* Fire damage */
		case GF_FIRE:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_RES_FIRE))
			{
				note = " resists.";
				dam = 0;
				if (seen) l_ptr->flags3 |= (RF3_RES_FIRE);
			}
			if (r_ptr->flags3 & (RF3_HURT_FIRE))
			{
				note = " is badly hurt.";
				dam *= 2;
				if (seen) l_ptr->flags3 |= (RF3_HURT_FIRE);
			}
			break;
		}

		/* Cold */
		case GF_COLD:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_RES_COLD))
			{
				note = " resists.";
				dam = 0;
				if (seen) l_ptr->flags3 |= (RF3_RES_COLD);
			}
			if (r_ptr->flags3 & (RF3_HURT_COLD))
			{
				note = " is badly hurt.";
				dam *= 2;
				if (seen) l_ptr->flags3 |= (RF3_HURT_COLD);
			}
			break;
		}

		/* Poison */
		case GF_POIS:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_RES_POIS))
			{
				note = " resists.";
				dam = 0;
				if (seen) l_ptr->flags3 |= (RF3_RES_POIS);
			}
			break;
		}

		/* Sound (use "dam" as amount of stunning) */
		case GF_SOUND:
		{
			obvious = TRUE;

			do_stun = dam;
			
			/* No "real" damage */
			dam = 0;
			break;
		}

		/* Heal Monster (use "dam" as amount of healing) */
		case GF_HEAL:
		{
			bool healed = TRUE;

			/*does monster need healing?*/
			if (m_ptr->hp == m_ptr->maxhp) healed = FALSE;

			if (seen) obvious = TRUE;

			/* Monster goes active */
			m_ptr->mflag |= (MFLAG_ACTV);

			/* Heal */
			m_ptr->hp += dam;

			/* No overflow */
			if (m_ptr->hp > m_ptr->maxhp) m_ptr->hp = m_ptr->maxhp;

			/* Redraw (later) if needed */
			if (p_ptr->health_who == cave_m_idx[y][x]) p_ptr->redraw |= (PR_HEALTHBAR);

			/*monster was at full hp to begin*/
			if (!healed)
			{
				obvious = FALSE;
			}

			/* Message */
			else note = " looks healthier.";
			
			// doesn't alert sleeping monsters
			if (m_ptr->alertness < ALERTNESS_UNWARY) alerting = FALSE;

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Speed Monster */
		case GF_SPEED:
		{
			if (seen) obvious = TRUE;

			/* Speed up */
			do_haste = dam;

			// doesn't alert sleeping monsters
			if (m_ptr->alertness < ALERTNESS_UNWARY) alerting = FALSE;

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Slow Monster (Use "dif" as difficulty and for duration) */
		case GF_SLOW:
		{		
			if (seen) obvious = TRUE;
			
			resistance = monster_skill(m_ptr, S_WIL);
			if (r_ptr->flags3 & (RF3_NO_SLOW))  resistance += 100;

			// adjust difficulty by the distance to the monster
			result = skill_check(who_ptr, dif - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);
			
			/* If successful, slow the monster */
			if (result > 0)
			{
				do_slow = result + 10;
			}
			else
			{
				note = " is unaffected!";
				obvious = FALSE;
				if ((seen) && (r_ptr->flags3 & (RF3_NO_SLOW)))
				    l_ptr->flags3 |= (RF3_NO_SLOW);
			}
			
			// doesn't alert sleeping or unaffected monsters
			if ((m_ptr->alertness < ALERTNESS_UNWARY) || (do_slow == 0)) alerting = FALSE;

			/* No "real" damage */
			dam = 0;
			
			break;
		}


		/* Sleep (Use "dif" as difficulty and for strength) */
		case GF_SLEEP:
		{
			if (seen) obvious = TRUE;
			
			resistance = monster_skill(m_ptr, S_WIL);
			if (r_ptr->flags3 & (RF3_NO_SLEEP))  resistance += 100;

			// adjust difficulty by the distance to the monster
			result = skill_check(who_ptr, dif - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);
			
			/* If successful, (partially) put the monster to sleep */
			if (result > 0)
			{
				do_sleep = result + 5;
			}
			else
			{
				note = " is unaffected!";
				obvious = FALSE;
				if ((seen) && (r_ptr->flags3 & (RF3_NO_SLEEP)))
				    l_ptr->flags3 |= (RF3_NO_SLEEP);
			}

			// doesn't alert monsters
			alerting = FALSE;

			/* No "real" damage */
			dam = 0;

			break;
		}


		/* Confusion (Use "dif" as difficulty and for duration) */
		case GF_CONFUSION:
		{
			if (seen) obvious = TRUE;
			
			resistance = monster_skill(m_ptr, S_WIL);
			if (r_ptr->flags3 & (RF3_NO_CONF))  resistance += 100;

			// adjust difficulty by the distance to the monster
			result = skill_check(who_ptr, dif - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);
			
			/* If successful, slow the monster */
			if (result > 0)
			{
				do_conf = result + 10;
			}
			else
			{
				note = " is unaffected!";
				obvious = FALSE;
				if ((seen) && (r_ptr->flags3 & (RF3_NO_CONF)))
				    l_ptr->flags3 |= (RF3_NO_CONF);
			}

			// doesn't alert monsters (they are either unaffected or too confused)
			alerting = FALSE;

			/* No "real" damage */
			dam = 0;

			break;
		}


		/* Lite, but only hurts susceptible creatures */
		case GF_LIGHT:
		{
			// Must make sure it is viewable (passwall was only used to guarantee wall lighting)
			if (cave_info[y][x] & (CAVE_VIEW))
			{
				/* Hurt by light */
				if (r_ptr->flags3 & (RF3_HURT_LITE))
				{
					/* Obvious effect */
					if (seen) obvious = TRUE;
					
					/* Memorize the effects */
					if (seen) l_ptr->flags3 |= (RF3_HURT_LITE);
					
					// do stunning
					m_ptr->stunned += dam;
					
					/*possibly update the monster health bar*/
					if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
						p_ptr->redraw |= (PR_HEALTHBAR);
					
					/* No "real" damage */
					dam = 0;
					
					/* Special effect */
					note = " cringes from the light!";
				}
				
				/* Normally no damage */
				else
				{
					/* No damage */
					dam = 0;
				}
			}
			
			// Doesn't alert monsters (there is a seperate function to do this for light)
			alerting = FALSE;

			break;
		}


		/* Dark */
		case GF_DARK:
		{
			if (seen) obvious = TRUE;
			if ((r_ptr->flags4 & (RF4_BRTH_DARK)) || (r_ptr->flags3 & (RF3_UNDEAD)) || (r_ptr->light < 0))
			{
				note = " resists.";
				dam = 0;
			}
			break;
		}


		/* Blasting */
		case GF_KILL_WALL:
		{
			/* Hurt by rock remover */
			if (r_ptr->flags3 & (RF3_STONE))
			{
				/* Notice effect */
				if (seen) obvious = TRUE;

				/* Memorize the effects */
				if (seen) l_ptr->flags3 |= (RF3_STONE);

                // skill check of Will vs Con * 2
                if (skill_check(PLAYER, dif, monster_stat(m_ptr, A_CON) * 2, m_ptr) > 0)
                {
                    /* Cute little message */
                    note = " partly shatters!";
                    note_dies = " shatters!";
                }
                
                // Will check fails
                else
                {
                    note = " resists!";

                    /* No damage */
                    dam = 0;
                }
			}

			/* Usually, ignore the effects */
			else
			{
				// doesn't alert unaffected monsters
				alerting = FALSE;
				
				/* No damage */
				dam = 0;
			}

			break;
		}

		/* Teleport monster (Use "dam" as "power") */
		case GF_AWAY_ALL:
		{
			/* Obvious */
			if (seen) obvious = TRUE;

			/* Prepare to teleport */
			do_dist = dam;

			/* No "real" damage */
			dam = 0;
			break;
		}

		/* Fear (Use "dif" as difficulty and for duration) */
		case GF_FEAR:
		{
			resistance = monster_skill(m_ptr, S_WIL);
			if (r_ptr->flags3 & (RF3_NO_FEAR))  resistance += 100;
			
			// adjust difficulty by the distance to the monster
			result = skill_check(who_ptr, dif + 5 - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);

			if (result > 0)
			{
				/* Obvious */
				if (seen) obvious = TRUE;
				
				/* Apply some fear */
				do_fear = result * 20;
			}
			else
			{
				// Doesn't alert unaffected monsters
				alerting = FALSE;

				/* No obvious effect */
				note = " is unaffected!";
				obvious = FALSE;
				
				if ((seen) && (r_ptr->flags3 & (RF3_NO_FEAR)))
					l_ptr->flags3 |= (RF3_NO_FEAR);
			}

			/* No "real" damage */
			dam = 0;
			break;
		}

		/* No effect */
		case GF_NOTHING:
		{
			break;
		}
			
		/* Default */
		default:
		{
			/* Irrelevant */
			skipped = TRUE;

			/* No damage */
			dam = 0;

			break;
		}
	}


	/* Absolutely no effect */
	if (skipped) return (FALSE);

	/* "Unique" monsters cannot be polymorphed */
	if (r_ptr->flags1 & (RF1_UNIQUE)) do_poly = FALSE;

	/* "Unique" monsters can only be "killed" by the player */
	//if (r_ptr->flags1 & (RF1_UNIQUE))
	//{
	//	/* Uniques may only be killed by the player */
	//	if ((who > 0) && (dam > m_ptr->hp)) dam = m_ptr->hp;
	//}

	/* Check for death */
	if (dam > m_ptr->hp)
	{
		/* Extract method of death */
		note = note_dies;
	}

	/* Mega-Hack -- Handle "polymorph" -- monsters get a saving throw */
	else if (do_poly && (dieroll(90) > r_ptr->level))
	{
		/* Default -- assume no polymorph */
		note = " is unaffected!";

		/* Pick a "new" monster race */
		tmp = poly_r_idx(m_ptr);

		/* Handle polymorph */
		if (tmp != m_ptr->r_idx)
		{
			/* Obvious */
			if (seen) obvious = TRUE;

			/* Monster polymorphs */
			note = " changes!";

			/* Turn off the damage */
			dam = 0;

			/* "Kill" the "old" monster */
			delete_monster_idx(cave_m_idx[y][x]);

			/* Create a new monster (no groups) */
			(void)place_monster_aux(y, x, tmp, FALSE, FALSE);

			/* Hack -- Assume success XXX XXX XXX */

			/* Hack -- Get new monster */
			m_ptr = &mon_list[cave_m_idx[y][x]];

			/* Hack -- Get new race */
			r_ptr = &r_info[m_ptr->r_idx];
		}
	}

	/* Handle "teleport" */
	else if (do_dist)
	{
		/* no teleporting on certain levels */
		if ((p_ptr->depth != 0) && (p_ptr->depth != MORGOTH_DEPTH))
		{
			/* Obvious */
			if (seen) obvious = TRUE;

			/* Message */
			note = " disappears!";

			/* Teleport */
			teleport_away(cave_m_idx[y][x], do_dist);

			/* Hack -- get new location */
			y = m_ptr->fy;
			x = m_ptr->fx;
		}
	}

	/* Stunning */
	else if (do_stun)
	{
		/* Obvious */
		if (seen) obvious = TRUE;

		/* Get confused */
		if (m_ptr->stunned) note = " is more dazed.";
		else                note = " is dazed.";

        tmp = m_ptr->stunned + do_stun;
        
		/*some creatures are resistant to stunning*/
		if (r_ptr->flags3 & RF3_NO_STUN)
		{
			/*mark the lore*/
			if (seen) l_ptr->flags3 |= (RF3_NO_STUN);

			note = " is unaffected!";

		}

		/* Apply stun */
		else m_ptr->stunned += (tmp < 200) ? tmp : 200;

		/*possibly update the monster health bar*/
		if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
					p_ptr->redraw |= (PR_HEALTHBAR);
	}

	/* Confusion  */
	else if (do_conf)
	{
		/* Obvious */
		if (seen) obvious = TRUE;

		/* Generate message */
		if (m_ptr->confused)  note = " looks more confused.";
		else                  note = " looks confused.";

		tmp = m_ptr->confused + do_conf;

		/* Apply confusion */
		m_ptr->confused += (tmp < 200) ? tmp : 200;

		if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
		p_ptr->redraw |= (PR_HEALTHBAR);
	}

	/*Slowing*/
	else if (do_slow)
	{
		/* Increase slowing */
		tmp = m_ptr->slowed + do_slow;

		/* set or add to slow counter */
		set_monster_slow(cave_m_idx[m_ptr->fy][m_ptr->fx],
						tmp, seen);
	}

	/* Hasting */
	else if (do_haste)
	{
		/* Increase haste */
		tmp = m_ptr->hasted + do_haste;

		/* set or add to slow counter */
		set_monster_haste(cave_m_idx[m_ptr->fy][m_ptr->fx],
						tmp, seen);
	}


	/* Fear */
	if (do_fear)
	{
		/* Decrease temporary morale */
		m_ptr->tmp_morale -= do_fear;
	}

	// update combat info
	if ((dam > 0) && m_ptr->ml)
	{
		update_combat_rolls1b(who_ptr, m_ptr, who_vis);
		update_combat_rolls2(dd, ds, dam, -1, -1, 0, 0, typ, FALSE);
	}
	
	/* If another monster did the damage, hurt the monster by hand */
	if (who > 0)
	{
		/* Redraw (later) if needed */
		if (p_ptr->health_who == cave_m_idx[y][x]) p_ptr->redraw |= (PR_HEALTHBAR);

		/* Monster goes active */
		m_ptr->mflag |= (MFLAG_ACTV);

		/* Hurt the monster */
		m_ptr->hp -= dam;


		/* Dead monster */
		if (m_ptr->hp <= 0)
		{
			/* Generate treasure, etc */
			monster_death(cave_m_idx[y][x]);

			/* Delete the monster */
			delete_monster_idx(cave_m_idx[y][x]);

			/* Give detailed messages if destroyed */
			if ((note) && (seen))
			{
				/* dump the note*/
				if (!suppress_message) msg_format("%^s%s", m_name, note);
			}

			else death_count++;
		}

		/* Damaged monster */
		else
		{
			// Alert it
			make_alert(m_ptr);
			
			/* Give detailed messages if visible or destroyed */
			if (note && seen)
			{
				/* dump the note*/
				if (!suppress_message) msg_format("%^s%s", m_name, note);
			}

			/* Hack -- Pain message */
			else if (dam > 0) message_pain(cave_m_idx[y][x], dam);

			/* Hack -- handle sleep */
			if (do_sleep) 
			{
				set_alertness(m_ptr, m_ptr->alertness - do_sleep);
			}
		}
	}

	/* If the player did it, give him experience, check fear */
	else
	{
		/*hack - only give message if seen*/
		if (!seen) note_dies = "";

		/* Hurt the monster, check for death */
		if (mon_take_hit(cave_m_idx[y][x], dam, note_dies, who))
		{
			/* Note death */
			if (!seen) death_count++;
		}

		/* Damaged monster */
		else
		{
			// Alert it, if there has been no damage to alert it so far
			if (alerting && (dam == 0))		make_alert(m_ptr);


			/* Give detailed messages if visible or destroyed */
			if (note && seen)
			{
				if (!suppress_message) msg_format("%^s%s", m_name, note);
			}

			/* Hack -- Pain message */
			else if (dam > 0) message_pain(cave_m_idx[y][x], dam);

			/* Take note */
			if ((do_fear) && (m_ptr->ml) && (!suppress_message))
			{
				/* Message */
				message_format(MSG_FLEE, m_ptr->r_idx,
				               "%^s cowers.", m_name);
			}

			/* Hack -- handle sleep */
			if (do_sleep) 
			{
				set_alertness(m_ptr, m_ptr->alertness - do_sleep);
			}
		}
	}


	/* Verify this code XXX XXX XXX */

	/* Update the monster */
	update_mon(cave_m_idx[y][x], FALSE);

	/* Redraw the monster grid */
	lite_spot(y, x);

	/* Update monster recall window */
	if (p_ptr->monster_race_idx == m_ptr->r_idx)
	{
		/* Window stuff */
		p_ptr->window |= (PW_MONSTER);
	}

	/* Track it */
	project_m_n++;
	project_m_x = x;
	project_m_y = y;

	/*
	 * If this is the first monster hit, the spell was capable
	 * of causing damage, and the player was the source of the spell,
	 * make noise. -LM-
	 */
	if ((project_m_n == 1) && (who <= 0) && (dam))
	{
		stealth_score -= 0;
	}

	/* Return "Anything seen?" */
	return (obvious);
}






/*
 * Helper function for "project()" below.
 *
 * Handle a beam/bolt/ball causing damage to the player.
 *
 * This routine takes a "source monster" (by index), a "distance", a default
 * "damage", and a "damage type".  See "project_m()" above.
 *
 * If "rad" is non-zero, then the blast was centered elsewhere, and the damage
 * is reduced (see "project_m()" above).  This can happen if a monster breathes
 * at the player and hits a wall instead.
 *
 * We return "TRUE" if any "obvious" effects were observed.
 *
 * Actually, for historical reasons, we just assume that the effects were
 * obvious.  XXX XXX XXX
 */
static bool project_p(int who, int y, int x, int dd, int ds, int dif, int typ)
{
	/* Hack -- assume obvious */
	bool obvious = TRUE;

	/* Player blind-ness */
	bool blind = (p_ptr->blind ? TRUE : FALSE);

	/* Source monster */
	monster_type *m_ptr;
	monster_race *r_ptr;

	/* Monster name (for attacks) */
	char m_name[80];

	/* Monster name (for damage) */
	char killer[80];
	
	int dam;
	
	bool do_disturb = TRUE;
		
	// Sil-y: unusued parameter, casting it to soothe compilation warnings
	(void)dif;

	/* No player here */
	if (!(cave_m_idx[y][x] < 0)) return (FALSE);

	/* Never affect projector */
	if (cave_m_idx[y][x] == who) return (FALSE);

	/* Get the source monster */
	m_ptr = &mon_list[who];

	/* Get the monster race. */
	r_ptr = &r_info[m_ptr->r_idx];

	/* Get the monster name */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	/* Get the monster's real name */
	monster_desc(killer, sizeof(killer), m_ptr, 0x88);

	dam = damroll(dd, ds);

	// generate the display messages for undodgable attacks
	if ((dam > 0) && (typ != GF_ARROW) && (typ != GF_BOULDER))
	{
		update_combat_rolls1b(m_ptr, PLAYER, m_ptr->ml);
		
		if ((typ != GF_FIRE) && (typ != GF_COLD) && (typ != GF_POIS) && (typ != GF_DARK))
		{ 
			update_combat_rolls2(dd, ds, dam, -1, -1, 0, 0, typ, FALSE);
		}
	}

	/* Analyze the damage */
	switch (typ)
	{
		/* Standard damage -- hurts inventory too */
		case GF_ACID:
		{
			if (blind) msg_print("You are hit by acid!");
			acid_dam(dam, killer);
			break;
		}

		/* Standard damage -- hurts inventory too */
		case GF_ELEC:
		{
			if (blind) msg_print("You are hit by lightning!");
			elec_dam(dam, killer);
			break;
		}
			
		/* Standard damage -- hurts inventory too */
		case GF_FIRE:
		{
			if (blind) msg_print("You are hit by fire!");
			fire_dam_pure(dd, ds, TRUE, killer);
			break;
		}

		/* Standard damage -- hurts inventory too */
		case GF_COLD:
		{
			if (blind) msg_print("You are hit by cold!");
			cold_dam_pure(dd, ds, TRUE, killer);
			break;
		}

		/* Dark  */
		case GF_DARK:
		{
			if (blind) msg_print("You are hit by something!");
			dark_dam_pure(dd, ds, TRUE, killer);
			break;
		}

		/* Weak Dark -- nothing! */
		case GF_DARK_WEAK:
		{
			do_disturb = FALSE;
			obvious = FALSE;
			break;
		}
			
		/* Posion */
		case GF_POIS:
		{
			if (blind) msg_print("You are hit by poison!");
			(void)pois_dam_pure(dd, ds, TRUE);
			break;
		}

						
		/* Arrow */
		case GF_ARROW:
		{
			int total_attack_mod, total_evasion_mod, crit_bonus_dice, hit_result;
			int total_dd, total_ds;
			int prt, net_dam, weight;

			// attacks with GF_ARROW will require an attack roll
			
			// determine the monster's attack score
			total_attack_mod = total_monster_attack(m_ptr, r_ptr->spell_power);
			
			// determine the player's evasion score
			total_evasion_mod = total_player_evasion(m_ptr, FALSE);

			// target only gets half the evasion modifier against archery
			total_evasion_mod /= 2;
			
			// simulate weights of longbows and shortbows
			if (ds >= 11) weight = 30;
			else          weight = 20;
						
			// perform the hit roll
			hit_result = hit_roll(total_attack_mod, total_evasion_mod, m_ptr, PLAYER, TRUE);
			
			if (hit_result > 0)
			{
				crit_bonus_dice = crit_bonus(hit_result, weight, &r_info[0], S_ARC, FALSE);
				total_dd = dd + crit_bonus_dice;
				total_ds = ds;
								
				dam = damroll(total_dd, total_ds);
				
				// armour is effective against GF_ARROW
				prt = protection_roll(GF_HURT, FALSE);
				net_dam = (dam - prt > 0) ? (dam - prt) : 0;

				if (blind)
				{
					msg_print("You are hit by something sharp.");
				}
				else
				{
					if (net_dam > 0)
					{
						if (crit_bonus_dice == 0)
						{
							msg_print("It hits you.");
						}
						else
						{
							msg_print("It hits!");
						}
					}
				}

				update_combat_rolls2(total_dd, total_ds, dam, -1, -1, prt, 100, GF_HURT, FALSE);
				display_hit(p_ptr->py, p_ptr->px, net_dam, GF_HURT, p_ptr->is_dead);

				if (net_dam)
				{
					take_hit(net_dam, killer);
                    
                    // deal with crippling shot ability
                    if ((r_ptr->flags2 & (RF2_CRIPPLING)) && (crit_bonus_dice >= 1) && (net_dam > 0))
                    {
                        // Sil-y: ideally we'd use a call to allow_player_slow() here, but that doesn't
                        //        work as it can't take the level of the critical into account.
                        //        Sadly my solution doesn't let you ID free action items.
                        int difficulty = p_ptr->skill_use[S_WIL] + (p_ptr->free_act * 10);
                        
                        if (skill_check(m_ptr, crit_bonus_dice * 4, difficulty, PLAYER) > 0)
                        {
                            monster_lore *l_ptr = &l_list[m_ptr->r_idx];

                            // remember that the monster can do this
                            if (m_ptr->ml)  l_ptr->flags2 |= (RF2_CRIPPLING);

                            msg_format("The shot tears into your thigh!");

                            // slow the player
                            set_slow(p_ptr->slow + crit_bonus_dice);
                        }
                    }
				}

				/* Make some noise */
				monster_perception(TRUE, FALSE, -5);
			}

			break;
		}

		/* Boulder */
		/* mostly the same as GF_ARROW, but doing 6d4 damage instead*/
		case GF_BOULDER:
		{
			int total_attack_mod, total_evasion_mod, crit_bonus_dice, hit_result;
			int total_dd, total_ds;
			int prt, net_dam;

			// attacks with GF_BOULDER will require an attack roll
			
			// determine the monster's attack score
			total_attack_mod = total_monster_attack(m_ptr, r_ptr->spell_power);

			// determine the player's evasion score
			total_evasion_mod = total_player_evasion(m_ptr, FALSE);
						
			// perform the hit roll
			hit_result = hit_roll(total_attack_mod, total_evasion_mod, m_ptr, PLAYER, TRUE);
			
			if (hit_result > 0)
			{
				crit_bonus_dice = crit_bonus(hit_result, 100, &r_info[0], S_ARC, TRUE);
				total_dd = dd + crit_bonus_dice;
				total_ds = ds;
				
				dam = damroll(total_dd, total_ds);
				
				// armour is effective against GF_BOULDER
				prt = protection_roll(GF_HURT, FALSE);
				net_dam = (dam - prt > 0) ? (dam - prt) : 0;

				if (blind)
				{
					msg_print("You are hit by something very heavy.");
				}
				else
				{
					if (net_dam > 0)
					{
						if (crit_bonus_dice == 0)
						{
							msg_print("It hits you.");
						}
						else
						{
							msg_print("It hits!");
						}
					}
				}

				update_combat_rolls2(total_dd, total_ds, dam, -1, -1, prt, 100, GF_HURT, FALSE);
				display_hit(p_ptr->py, p_ptr->px, net_dam, GF_HURT, p_ptr->is_dead);

				if (net_dam)
				{
					take_hit(net_dam, killer);
				}

				/* Make some noise */
				monster_perception(TRUE, FALSE, -10);
			}

			break;
		}

		/* Sound (use "dam" as stunning) */
		case GF_SOUND:
		{
			if (blind) msg_print("You are hit by a cacophony of sound!");
			if (allow_player_stun(m_ptr))
			{
				(void)set_stun(p_ptr->stun + dam);
			}
			else
			{
				msg_print("You are unfazed.");
			}
			break;
		}

		/* Does nothing */
		case GF_NOTHING:
		{
			do_disturb = FALSE;
			obvious = FALSE;
			break;
		}
			
		/* Default */
		default:
		{
			/* No damage */
			dam = 0;

			break;
		}
	}


	/* Disturb */
	if (do_disturb) disturb(1, 0);

	p_ptr->window |= (PW_COMBAT_ROLLS);

	/* Return "Anything seen?" */
	return (obvious);
}

/*
 * Calculate and store the arcs used to make starbursts.
 */
static void calc_starburst(int height, int width, byte *arc_first,
	byte *arc_dist, int *arc_num)
{
	int i;
	int size, dist, vert_factor;
	int degree_first, center_of_arc;


	/* Note the "size" */
	size = 2 + div_round(width + height, 22);

	/* Ask for a reasonable number of arcs. */
	*arc_num = 8 + (height * width / 80);
	*arc_num = rand_spread(*arc_num, 3);
	if (*arc_num < 8)  *arc_num = 8;
	if (*arc_num > 45) *arc_num = 45;

	/* Determine the start degrees and expansion distance for each arc. */
	for (degree_first = 0, i = 0; i < *arc_num; i++)
	{
		/* Get the first degree for this arc (using 180-degree circles). */
		arc_first[i] = degree_first;

		/* Get a slightly randomized start degree for the next arc. */
		degree_first += div_round(180, *arc_num);

		/* Do not entirely leave the usual range */
		if (degree_first < 180 * (i+1) / *arc_num)
		    degree_first = 180 * (i+1) / *arc_num;
		if (degree_first > (180 + *arc_num) * (i+1) / *arc_num)
		    degree_first = (180 + *arc_num) * (i+1) / *arc_num;


		/* Get the center of the arc (convert from 180 to 360 circle). */
		center_of_arc = degree_first + arc_first[i];

		/* Get arc distance from the horizontal (0 and 180 degrees) */
		if      (center_of_arc <=  90) vert_factor = center_of_arc;
		else if (center_of_arc >= 270) vert_factor = ABS(center_of_arc - 360);
		else                           vert_factor = ABS(center_of_arc - 180);

		/*
		 * Usual case -- Calculate distance to expand outwards.  Pay more
		 * attention to width near the horizontal, more attention to height
		 * near the vertical.
		 */
		dist = ((height * vert_factor) + (width * (90 - vert_factor))) / 90;

		/* Randomize distance (should never be greater than radius) */
		arc_dist[i] = rand_range(dist / 4, dist / 2);

		/* Keep variability under control (except in special cases). */
		if ((dist != 0) && (i != 0))
		{
			int diff = arc_dist[i] - arc_dist[i-1];

			if (ABS(diff) > size)
			{
				if (diff > 0)
					arc_dist[i] = arc_dist[i-1] + size;
				else
					arc_dist[i] = arc_dist[i-1] - size;
			}
		}
	}

	/* Neaten up final arc of circle by comparing it to the first. */
	if (TRUE)
	{
		int diff = arc_dist[*arc_num - 1] - arc_dist[0];

		if (ABS(diff) > size)
		{
			if (diff > 0)
				arc_dist[*arc_num - 1] = arc_dist[0] + size;
			else
				arc_dist[*arc_num - 1] = arc_dist[0] - size;
		}
	}
}


/*
 * Generic "beam"/"bolt"/"ball" projection routine.
 *
 * Input:
 *   who: Index of "source" monster (negative for "player")
 *   rad: Radius of explosion (0 = beam/bolt, 1 to 9 = ball)
 *   y,x: Target location (or location to travel "towards")
 *   dam: Base damage roll to apply to affected monsters (or player)
 *   typ: Type of damage to apply to monsters (and objects)
 *   flg: Extra bit flags (see PROJECT_xxxx in "defines.h")
 *   degrees: How wide an arc spell is (in degrees).
 *   uniform: uniform means no damage reduction with range, otherwise it is one die per square.
 *
 * Return:
 *   TRUE if any "effects" of the projection were observed, else FALSE
 *
* At present, there are five major types of projections:
 *
 * Point-effect projection:  (no PROJECT_BEAM flag, radius of zero, and either
 *   jumps directly to target or has a single source and target grid)
 * A point-effect projection has no line of projection, and only affects one
 *   grid.  It is used for most area-effect spells (like dispel evil) and
 *   pinpoint strikes.
 *
 * Bolt:  (no PROJECT_BEAM flag, radius of zero, has to travel from source to
 *   target)
 * A bolt travels from source to target and affects only the final grid in its
 *   projection path.  If given the PROJECT_STOP flag, it is stopped by any
 *   monster or character in its path (at present, all bolts use this flag).
 *
 * Beam:  (PROJECT_BEAM)
 * A beam travels from source to target, affecting all grids passed through
 *   with full damage.  It is never stopped by monsters in its path.  Beams
 *   may never be combined with any other projection type.
 *
 * Ball:  (positive radius, unless the PROJECT_ARC flag is set)
 * A ball travels from source towards the target, and always explodes.  Unless
 *   specified, it does not affect wall grids, but otherwise affects any grids
 *   in LOS from the center of the explosion.
 * If used with a direction, a ball will explode on the first occupied grid in
 *   its path.  If given a target, it will explode on that target.  If a
 *   wall is in the way, it will explode against the wall.  If a ball reaches
 *   MAX_RANGE without hitting anything or reaching its target, it will
 *   explode at that point.
 *
 * Arc:  (positive radius, with the PROJECT_ARC flag set)
 * An arc is a portion of a source-centered ball that explodes outwards
 *   towards the target grid.  Like a ball, it affects all non-wall grids in
 *   LOS of the source in the explosion area.  The width of arc spells is con-
 *   trolled by degrees.
 * An arc is created by rejecting all grids that form the endpoints of lines
 *   whose angular difference (in degrees) from the centerline of the arc is
 *   greater than one-half the input "degrees".  See the table "get_
 *   angle_to_grid" in "util.c" for more information.
 * Note:  An arc with a value for degrees of zero is actually a beam of
 *   defined length.
 *
 * Projections that affect all monsters in LOS are handled through the use
 *   of "project_los()", which applies a single-grid projection to individual
 *   monsters.  Projections that light up rooms or affect all monsters on the
 *   level are more efficiently handled through special functions.
 *
 *
 * Variations:
 *
 * PROJECT_STOP forces a path of projection to stop at the first occupied
 *   grid it hits.  This is used with bolts, and also by ball spells
 *   travelling in a specific direction rather than towards a target.
 *
 * PROJECT_THRU allows a path of projection towards a target to continue
 *   past that target.
 *
 * PROJECT_JUMP allows a projection to immediately set the source of the pro-
 *   jection to the target.  This is used for all area effect spells (like
 *   dispel evil), and can also be used for bombardments.
 *
 * PROJECT_WALL allows a projection, not just to affect one layer of any
 *   passable wall (rubble, trees), but to affect the surface of any wall.
 *   Certain projection types always have this flag.
 *
 * PROJECT_PASS allows projections to ignore walls completely.
 *   Certain projection types always have this flag.
 *
 * PROJECT_HIDE erases all graphical effects, making the projection
 *   invisible.
 *
 * PROJECT_GRID allows projections to affect terrain features.
 *
 * PROJECT_ITEM allows projections to affect objects on the ground.
 *
 * PROJECT_KILL allows projections to affect monsters.
 *
 * PROJECT_PLAY allows projections to affect the player.
 *
 * degrees controls the width of arc spells.  With a value for
 *   degrees of zero, arcs act like beams of defined length.
 *
 * Implementation notes:
 *
 * If the source grid is not the same as the target, we project along the path
 *   between them.  Bolts stop if they hit anything, beams stop if they hit a
 *   wall, and balls and arcs may exhibit either behavior.  When they reach
 *   the final grid in the path, balls and arcs explode.  We do not allow beams
 *   to be combined with explosions.
 * Balls affect all floor grids in LOS (optionally, also wall grids adjacent
 *   to a grid in LOS) within their radius.  Arcs do the same, but only within
 *   their cone of projection.
 * Because affected grids are only scanned once, and it is really helpful to
 *   have explosions that travel outwards from the source, they are sorted by
 *   distance.  For each distance, an adjusted damage is calculated.
 * In successive passes, the code then displays explosion graphics, erases
 *   these graphics, marks terrain for possible later changes, affects
 *   objects, monsters, the character, and finally changes features and
 *   teleports monsters and characters in marked grids.
 *
 *
 * Usage and graphics notes:
 *
 * If the option "fresh_before" is on, or the delay factor is anything other
 * than zero, bolt and explosion pictures will be momentarily shown on screen.
 *
 * Only 256 grids can be affected per projection, limiting the effective
 * radius of standard ball attacks to nine units (diameter nineteen).  Arcs
 * can have larger radii; an arc capable of going out to range 20 should not
 * be wider than 70 degrees.
 *
 * Balls must explode BEFORE hitting walls, or they would affect monsters on
 * both sides of a wall.
 *
 * Note that for consistency, we pretend that the bolt actually takes time
 * to move from point A to point B, even if the player cannot see part of the
 * projection path.  Note that in general, the player will *always* see part
 * of the path, since it either starts at the player or ends on the player.
 *
 * Hack -- we assume that every "projection" is "self-illuminating".
 *
 * Hack -- when only a single monster is affected, we automatically track
 * (and recall) that monster, unless "PROJECT_JUMP" is used.
 *
 * Note that we must call "handle_stuff()" after affecting terrain features
 * in the blast radius, in case the illumination of the grid was changed,
 * and "update_view()" and "update_monsters()" need to be called.
 */
bool project(int who, int rad, int y0, int x0, int y1, int x1, int dd, int ds, int dif, int typ,
			 u32b flg, int degrees, bool uniform)
{
	int i, j, k;
	int dist = 0;

	u32b dam_temp;
	int centerline = 0;

	int y = y0;
	int x = x0;
	int n1y = 0;
	int n1x = 0;
	int y2, x2;

	int msec = op_ptr->delay_factor * op_ptr->delay_factor;

	/* Assume the player sees nothing */
	bool notice = FALSE;

	/* Assume the player has seen nothing */
	bool visual = FALSE;

	/* Assume the player has seen no blast grids */
	bool drawn = FALSE;

	/* Is the player blind? */
	bool blind = (p_ptr->blind ? TRUE : FALSE);

	/* Number of grids in the "path" */
	int path_n = 0;

	/* Actual grids in the "path" */
	u16b path_g[512];

	/* Number of grids in the "blast area" (including the "beam" path) */
	int grids = 0;

	/* Coordinates of the affected grids */
	byte gx[256], gy[256];

	/* Distance to each of the affected grids. */
	byte gd[256];

	/* Precalculated damage values for each distance. */
	int dam_at_dist[MAX_RANGE+1];

	/*
	 * Starburst projections only --
	 * Holds first degree of arc, maximum effect distance in arc.
	 */
	byte arc_first[45];
	byte arc_dist[45];

	/* Number (max 45) of arcs. */
	int arc_num = 0;

	int degree, max_dist;

	/* Hack -- Flush any pending output */
	handle_stuff();

	/* Make certain that the radius is not too large */
	if (rad > MAX_SIGHT) rad = MAX_SIGHT;

	/* Some projection types always PROJECT_WALL. */
	if ((typ == GF_KILL_WALL) || (typ == GF_KILL_DOOR))
	{
		flg |= (PROJECT_WALL);
	}

	/* Hack -- Jump to target, but require a valid target */
	if ((flg & (PROJECT_JUMP)) && (y1) && (x1))
	{
		y0 = y1;
		x0 = x1;

		/* Clear the flag */
		flg &= ~(PROJECT_JUMP);
	}

	/* If a single grid is both source and destination, store it. */
	if ((x1 == x0) && (y1 == y0))
	{
		gy[grids] = y0;
		gx[grids] = x0;
		gd[grids++] = 0;
	}

	/* Otherwise, unless an arc or a star, travel along the projection path. */
	else if (!(flg & (PROJECT_ARC | PROJECT_STAR)))
	{
		/* Determine maximum length of projection path */
		if (flg & (PROJECT_BOOM)) dist = MAX_RANGE;
		else if (rad <= 0)        dist = MAX_RANGE;
		else                      dist = rad;

		/* Calculate the projection path */
		path_n = project_path(path_g, dist, y0, x0, &y1, &x1, flg);

		/* Project along the path */
		for (i = 0; i < path_n; ++i)
		{
			int oy = y;
			int ox = x;

			int ny = GRID_Y(path_g[i]);
			int nx = GRID_X(path_g[i]);

			/* Hack -- Balls explode before reaching walls. */
			if ((flg & (PROJECT_BOOM)) && (!cave_floor_bold(ny, nx)))
			{
				break;
			}

			/* Advance */
			y = ny;
			x = nx;

			/* If a beam, collect all grids in the path. */
			if (flg & (PROJECT_BEAM))
			{
				gy[grids] = y;
				gx[grids] = x;
				gd[grids++] = 0;
			}

			/* Otherwise, collect only the final grid in the path. */
			else if (i == path_n - 1)
			{
				gy[grids] = y;
				gx[grids] = x;
				gd[grids++] = 0;
			}

			/* Only do visuals if requested */
			if (!blind && !(flg & (PROJECT_HIDE)))
			{
				/* Only do visuals if the player can "see" the projection */
				if (panel_contains(y, x) && player_has_los_bold(y, x))
				{
					u16b p;

					byte a;
					char c;

					/* Obtain the bolt pict */
					p = bolt_pict(oy, ox, y, x, typ);

					/* Extract attr/char */
					a = PICT_A(p);
					c = PICT_C(p);

					/* Display the visual effects */
					print_rel(c, a, y, x);
					move_cursor_relative(y, x);
					if (op_ptr->delay_factor) Term_fresh();

					/* Delay */
					Term_xtra(TERM_XTRA_DELAY, msec);

					/* Erase the visual effects */
					lite_spot(y, x);
					if (op_ptr->delay_factor) Term_fresh();

					/* Re-display the beam  XXX */
					if (flg & (PROJECT_BEAM))
					{
						/* Obtain the explosion pict */
						p = bolt_pict(y, x, y, x, typ);

						/* Extract attr/char */
						a = PICT_A(p);
						c = PICT_C(p);

						/* Visual effects */
						print_rel(c, a, y, x);
					}

					/* Hack -- Activate delay */
					visual = TRUE;
				}

				/* Hack -- Always delay for consistency */
				else if (visual)
				{
					/* Delay for consistency */
					Term_xtra(TERM_XTRA_DELAY, msec);
				}
			}
		}
	}

	/* Save the "blast epicenter" */
	y2 = y;
	x2 = x;

	/* Beams have already stored all the grids they will affect. */
	if (flg & (PROJECT_BEAM))
	{
		/* No special actions */
	}

	/* Handle explosions */
	else if (flg & (PROJECT_BOOM))
	{
		/* Some projection types always PROJECT_WALL. */
		if (typ == GF_ACID)
		{
			/* Note that acid only affects monsters if it melts the wall. */
			flg |= (PROJECT_WALL);
		}

		/* Pre-calculate some things for starbursts. */
		if (flg & (PROJECT_STAR))
		{
			calc_starburst(1 + rad * 2, 1 + rad * 2, arc_first, arc_dist,
				&arc_num);

			/* Mark the area nearby -- limit range, ignore rooms */
			spread_cave_temp(y0, x0, rad, FALSE);

		}

		/* Pre-calculate some things for arcs. */
		if (flg & (PROJECT_ARC))
		{
			/* The radius of arcs cannot be more than 20 */
			if (rad > 20) rad = 20;

			/* Reorient the grid forming the end of the arc's centerline. */
			n1y = y1 - y0 + 20;
			n1x = x1 - x0 + 20;

			/* Correct overly large or small values */
			if (n1y > 40) n1y = 40;
			if (n1x > 40) n1x = 40;
			if (n1y <  0) n1y =  0;
			if (n1x <  0) n1x =  0;

			/* Get the angle of the arc's centerline */
			centerline = 90 - get_angle_to_grid[n1y][n1x];
		}

		/*
		 * If the center of the explosion hasn't been
		 * saved already, save it now.
		 */
		if (grids == 0)
		{
			gy[grids] = y2;
			gx[grids] = x2;
			gd[grids++] = 0;
		}

		/*
		 * Scan every grid that might possibly
		 * be in the blast radius.
		 */
		for (y = y2 - rad; y <= y2 + rad; y++)
		{
			for (x = x2 - rad; x <= x2 + rad; x++)
			{
				/* Center grid has already been stored. */
				if ((y == y2) && (x == x2)) continue;

				/* Precaution: Stay within area limit. */
				if (grids >= 255) break;

				/* Ignore "illegal" locations */
				if (!in_bounds(y, x)) continue;

				/* This is a wall grid (whether passable or not). */
				if (!cave_floor_bold(y, x))
				{
					/* Spell with PROJECT_PASS ignore walls */
					if (!(flg & (PROJECT_PASS)))
					{
						/* This grid is passable, or PROJECT_WALL is active */
						if ((flg & (PROJECT_WALL)) || (cave_floor_bold(y, x)))
						{
							/* Allow grids next to grids in LOS of explosion center */
							for (i = 0, k = 0; i < 8; i++)
							{
								int yy = y + ddy_ddd[i];
								int xx = x + ddx_ddd[i];

								/* Stay within dungeon */
								if (!in_bounds(yy, xx)) continue;

								if (los(y2, x2, yy, xx))
								{
									k++;
									break;
								}
							}

							/* Require at least one adjacent grid in LOS */
							if (!k) continue;
						}

						/* We can't affect this non-passable wall */
						else continue;
					}
				}

				/* Must be within maximum distance. */
				dist = (distance(y2, x2, y, x));
				if (dist > rad) continue;


				/* Projection is a starburst */
				if (flg & (PROJECT_STAR))
				{
					/* Grid is within effect range */
					if (cave_info[y][x] & (CAVE_TEMP))
					{
						/* Reorient current grid for table access. */
						int ny = y - y2 + 20;
						int nx = x - x2 + 20;

						/* Illegal table access is bad. */
						if ((ny < 0) || (ny > 40) || (nx < 0) || (nx > 40))
							continue;

						/* Get angle to current grid. */
						degree = get_angle_to_grid[ny][nx];

						/* Scan arcs to find the one that applies here. */
						for (i = arc_num - 1; i >= 0; i--)
						{
							if (arc_first[i] <= degree)
							{
								max_dist = arc_dist[i];

								/* Must be within effect range. */
								if (max_dist >= dist)
								{
									gy[grids] = y;
									gx[grids] = x;
									gd[grids] = 0;
									grids++;
								}

								/* Arc found.  End search */
								break;
							}
						}
					}
				}

				/* Use angle comparison to delineate an arc. */
				else if (flg & (PROJECT_ARC))
				{
					int n2y, n2x, tmp, diff;

					/* Reorient current grid for table access. */
					n2y = y - y2 + 20;
					n2x = x - x2 + 20;

					/*
					 * Find the angular difference (/2) between
					 * the lines to the end of the arc's center-
					 * line and to the current grid.
					 */
					tmp = ABS(get_angle_to_grid[n2y][n2x] + centerline) % 180;
					diff = ABS(90 - tmp);

					/*
					 * If difference is not greater then that
					 * allowed, and the grid is in LOS, accept it.
					 */
					if (diff < (degrees + 6) / 4)
					{
						if (los(y2, x2, y, x))
						{
							gy[grids] = y;
							gx[grids] = x;
							gd[grids] = dist;
							grids++;
						}
					}
				}

				/* Standard ball spell -- accept all grids in LOS. */
				else
				{
					if (flg & (PROJECT_PASS) || los(y2, x2, y, x))
					{
						gy[grids] = y;
						gx[grids] = x;
						gd[grids] = dist;
						grids++;
					}
				}
			}
		}
	}

	/* Clear the "temp" array  XXX */
	clear_temp_array();

	/* Calculate and store the actual damage at each distance. */
	for (i = 0; i <= MAX_RANGE; i++)
	{
		/* No damage outside the radius. */
		if (i > rad) dam_temp = 0;

		/* No damage reduction with range if uniform. */
		else if (uniform)
		{
			dam_temp = dd;
		}

		/* Otherwise, lose two dice per square. */
		else
		{
			if (dd > 2*i)	dam_temp = dd - 2*i;
			else			dam_temp = 0;
		}

		/* Store it. */
		dam_at_dist[i] = dam_temp;
	}

	/* Sort the blast grids by distance, starting at the origin. */
	for (i = 0, k = 0; i < rad; i++)
	{
		int tmp_y, tmp_x, tmp_d;

		/* Collect all the grids of a given distance together. */
		for (j = k; j < grids; j++)
		{
			if (gd[j] == i)
			{
				tmp_y = gy[k];
				tmp_x = gx[k];
				tmp_d = gd[k];

				gy[k] = gy[j];
				gx[k] = gx[j];
				gd[k] = gd[j];

				gy[j] = tmp_y;
				gx[j] = tmp_x;
				gd[j] = tmp_d;

				/* Write to next slot */
				k++;
			}
		}
	}


	/* Display the "blast area" if allowed */
	if (!blind && !(flg & (PROJECT_HIDE)))
	{
		/* Do the blast from inside out */
		for (i = 0; i < grids; i++)
		{
			/* Extract the location */
			y = gy[i];
			x = gx[i];

			/* Only do visuals if the player can "see" the blast */
			if (panel_contains(y, x) && player_has_los_bold(y, x))
			{
				u16b p;

				byte a;
				char c;

				drawn = TRUE;

				/* Obtain the explosion pict */
				p = bolt_pict(y, x, y, x, typ);

				/* Extract attr/char */
				a = PICT_A(p);
				c = PICT_C(p);

				/* Visual effects -- Display */
				print_rel(c, a, y, x);
			}

			/* Hack -- center the cursor */
			move_cursor_relative(y2, x2);

			/* New radius is about to be drawn */
			if ((i == grids - 1) || ((i < grids - 1) && (gd[i + 1] > gd[i])))
			{
				/* Flush each radius separately */
				if (op_ptr->delay_factor) Term_fresh();

				/* Delay (efficiently) */
				if (visual || drawn)
				{
					Term_xtra(TERM_XTRA_DELAY, msec);
				}
			}
		}

		/* Delay for a while if there are pretty graphics to show */
		if ((grids > 1) && (visual || drawn))
		{
			if (!op_ptr->delay_factor) Term_fresh();
			Term_xtra(TERM_XTRA_DELAY, 50 + msec);
		}

		/* Flush the erasing -- except if we specify lingering graphics */
		if ((drawn) && (!(flg & (PROJECT_NO_REDRAW))))
		{
			/* Erase the explosion drawn above */
			for (i = 0; i < grids; i++)
			{
				/* Extract the location */
				y = gy[i];
				x = gx[i];

				/* Hack -- Erase if needed */
				if (panel_contains(y, x) && player_has_los_bold(y, x))
				{
					lite_spot(y, x);
				}
			}

			/* Hack -- center the cursor */
			move_cursor_relative(y2, x2);

			/* Flush the explosion */
			if (op_ptr->delay_factor) Term_fresh();
		}
	}


	/* Check features */
	if (flg & (PROJECT_GRID))
	{

		/* Scan for features */
		for (i = 0; i < grids; i++)
		{
			/* Get the grid location */
			y = gy[i];
			x = gx[i];

			/* Affect the feature in that grid */
			if (project_f(who, y, x, gd[i], dam_at_dist[gd[i]], ds, dif, typ))
				notice = TRUE;
		}
	}

	/* Check objects */
	if (flg & (PROJECT_ITEM))
	{

		/* Scan for objects */
		for (i = 0; i < grids; i++)
		{

			/* Get the grid location */
			y = gy[i];
			x = gx[i];

			/* Affect the object in the grid */
			if (project_o(who, y, x, dam_at_dist[gd[i]], ds, dif, typ)) notice = TRUE;
		}
	}


	/* Check monsters */
	if (flg & (PROJECT_KILL))
	{
		/* Mega-Hack */
		project_m_n = 0;
		project_m_x = 0;
		project_m_y = 0;
		death_count = 0;

		/* Scan for monsters */
		for (i = 0; i < grids; i++)
		{

			/* Get the grid location */
			y = gy[i];
			x = gx[i];

			/* Affect the monster in the grid */
			if (project_m(who, y, x, dam_at_dist[gd[i]], ds, dif, typ, flg))
				notice = TRUE;
		}

		/* Player affected one monster (without "jumping") */
		if ((who < 0) && (project_m_n == 1) && !(flg & (PROJECT_JUMP)))
		{
			/* Location */
			x = project_m_x;
			y = project_m_y;

			/* Track if possible */
			if (cave_m_idx[y][x] > 0)
			{
				monster_type *m_ptr = &mon_list[cave_m_idx[y][x]];

				/* Hack -- auto-recall */
				if (m_ptr->ml) monster_race_track(m_ptr->r_idx);

				/* Hack - auto-track */
				// Sil-y: turned this off experimentally
				//if (m_ptr->ml) health_track(cave_m_idx[y][x]);
			}
		}

		/* Hack -- Moria-style death messages for non-visible monsters */
		if (death_count)
		{
			/* One monster */
			if (death_count == 1)
			{
				msg_print("You hear a scream of agony!");
			}

			/* Several monsters */
			else
			{
				msg_print("You hear several screams of agony!");
			}

			/* Reset */
			death_count = 0;
		}

	}

	/* Check player */
	if (flg & (PROJECT_PLAY))
	{

		/* Scan for player */
		for (i = 0; i < grids; i++)
		{

			/* Get the grid location */
			y = gy[i];
			x = gx[i];

			/* Player is in this grid */
			if (cave_m_idx[y][x] < 0)
			{

				/* Affect the player */
				if (project_p(who, y, x, dam_at_dist[gd[i]], ds, dif, typ))
				{
					notice = TRUE;

					/* Only affect the player once */
					break;
				}

			}
		}
	}

	/* Clear the "temp" array  (paranoia is good) */
	clear_temp_array();

	/* Update stuff if needed */
	if (p_ptr->update) update_stuff();

	/* Return "something was noticed" */
	return (notice);
}

void add_wrath(void)
{
	int new_wrath = 100;
	p_ptr->update |= (PU_BONUS);
	p_ptr->redraw |= (PR_SONG);

	p_ptr->wrath += new_wrath;
}

int slaying_song_bonus(void)
{
	return ((ability_bonus(S_SNG, SNG_SLAYING) * p_ptr->wrath + 999) / 1000);
}

/*
 *  Do the effects of Song of Freedom
 */
void song_of_freedom(int score)
{
    int y, x;
    int base_difficulty, difficulty;
    int result;
    int new_feat;
    object_type *o_ptr;
    bool closed_chasm = FALSE;
        
    // set the base difficulty
    if (p_ptr->depth > 0)
    {
        base_difficulty = p_ptr->depth / 2;
    }
    else
    {
        base_difficulty = 10;
    }
    
    /* Scan the map */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x)) continue;
            
            // get the object present (if any)
            o_ptr = &o_list[cave_o_idx[y][x]];
            
            /* Locked/trapped chest */
            if (o_ptr->tval == TV_CHEST)
            {
                /* Disarm/Unlock traps */
                if (o_ptr->pval > 0)
                {
                    difficulty = base_difficulty + 5 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                    if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                    {
                        /* Disarm or Unlock */
                        o_ptr->pval = (0 - o_ptr->pval);
                        
                        /* Identify */
                        object_known(o_ptr);
                    }
                }
            }
            
            // Chasm
            else if (cave_feat[y][x] == FEAT_CHASM)
            {
                closed_chasm |= close_chasm(y, x, score - flow_dist(FLOW_PLAYER_NOISE, y, x) - 5);
            }
            
            /* Invisible trap */
            else if (cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
            {
                difficulty = base_difficulty + 5 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                {
                    /* Remove the trap */
                    cave_feat[y][x] = FEAT_FLOOR;
                }
            }
            
            /* Visible trap */
            else if (cave_trap_bold(y,x))
            {
                difficulty = base_difficulty + 5 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                {
                    /* Remove the trap */
                    cave_feat[y][x] = FEAT_FLOOR;
                    
                    if (cave_info[y][x] & (CAVE_SEEN))
                    {
                        lite_spot(y, x);
                    }
                }
            }
            
            /* Secret door */
            else if (cave_feat[y][x] == FEAT_SECRET)
            {
                difficulty = base_difficulty + 0 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                {
                    /* Pick a door */
                    place_closed_door(y, x);
                    
                    if (cave_info[y][x] & (CAVE_SEEN))
                    {
                        /* Message */
                        msg_print("You have found a secret door.");
                        
                        /* Disturb */
                        disturb(0, 0);
                    }
                }
            }
            
            /* Stuck door */
            else if ((cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x08) && (cave_feat[y][x] <= FEAT_DOOR_TAIL))
            {
                difficulty = base_difficulty + 0 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                result = skill_check(PLAYER, score, difficulty, NULL);
                if (result > 0)
                {
                    new_feat = cave_feat[y][x] - result;
                    
                    if (new_feat <= FEAT_DOOR_HEAD + 0x08) new_feat = FEAT_DOOR_HEAD;
                    
                    cave_feat[y][x] = new_feat;
                }
            }
            
            /* Locked door */
            else if ((cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x01) && (cave_feat[y][x] <= FEAT_DOOR_HEAD + 0x07))
            {
                difficulty = base_difficulty + 0 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                result = skill_check(PLAYER, score, difficulty, NULL);
                if (result > 0)
                {
                    new_feat = cave_feat[y][x] - result;
                    
                    if (new_feat < FEAT_DOOR_HEAD) new_feat = FEAT_DOOR_HEAD;
                    
                    cave_feat[y][x] = new_feat;
                }
            }
            
            /* Rubble */
            else if (cave_feat[y][x] == FEAT_RUBBLE)
            {
                int noise_dist = 100;
                int d, dir;
                
                // check adjacent squares for valid noise distances, since rubble is impervious to sound
                for (d = 0; d < 8; d++)
                {
                    dir = cycle[d];
                    noise_dist = MIN(noise_dist, flow_dist(FLOW_PLAYER_NOISE, y + ddy[dir], x + ddx[dir]));
                }
                noise_dist++;
                
                difficulty = base_difficulty + 5 + noise_dist;
                result = skill_check(PLAYER, score, difficulty, NULL);
                if (result > 0)
                {
                    /* Disperse the rubble */
                    cave_set_feat(y, x, FEAT_FLOOR);
                    
                    /* Update the flow code */
                    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
                } 
            }
        }
    }
    
    // then, if any chasms were marked to be closed, do the closing
    if (closed_chasm)
    {
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_feat[y][x] == FEAT_CHASM) && (cave_info[y][x] & (CAVE_TEMP)))
                {
                    // remove the temporary marking
                    cave_info[y][x] &= ~(CAVE_TEMP);
                    
                    // close the chasm
                    cave_set_feat(y, x, FEAT_FLOOR);
                    
                    // update the visuals
                    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
                }
            }
        }
    }
}


/*
 *  Do the effects of (the monster song) Song of Binding
 */
void song_of_binding(monster_type *m_ptr)
{
    int y, x;
    int resistance;
    int result;
    int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    char m_name[80];
    cptr description;
    
    int song_skill = 20; // Morgoth's song skill. If more monsters get songs I'll put this in monster.txt
    
    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

    // messages for beginning a new song
    if (m_ptr->song != SNG_BINDING)
    {
        msg_format("%^s begins a song of binding.", m_name);

        // and remember the monsters is now singing this song
        m_ptr->song = SNG_BINDING;
        
        // disturb if message printed
        disturb(1, 0);
    }
    
    // messages for continuing a song
    else
    {
        switch (dieroll(8))
        {
            case 1:     description = "durance";                break;
            case 2:     description = "chains";                 break;
            case 3:     description = "thralls";                break;
            case 4:     description = "prison walls";           break;
            case 5:     description = "locks without keys";     break;
            default:    description = "binding";
        }
        
        if (m_ptr->ml)          msg_format("%^s sings of %s.", m_name, description);
        else if (dist <= 20)    msg_format("You hear a song of %s.", description);
        else if (dist <= 30)    msg_print("You hear singing in the distance.");

        // disturb if message printed
        if (m_ptr->ml || (dist <= 30)) disturb(1, 0);
    }

    // use the monster noise flow to represent the song levels at each square
    update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);

    // scan the map, closing doors
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x)) continue;

            // if there is no player/monster in the square
            if (cave_m_idx[y][x] == 0)
            {
                // if it is a door
                if ((cave_feat[y][x] == FEAT_OPEN) || (cave_feat[y][x] == FEAT_BROKEN) || cave_known_closed_door_bold(y,x))
                {
                    // if the door isn't between the monster and the player
                    if (!(ORDERED(m_ptr->fy, y, p_ptr->py) && ORDERED(m_ptr->fx, x, p_ptr->px)))
                    {
                        result = skill_check(m_ptr, song_skill, 15 + flow_dist(FLOW_MONSTER_NOISE, y, x), NULL);
                        
                        (void) lock_door(y, x, result);
                    }
                }
            }
        }
    }
    
    /*
    // scan the map, slowing monsters
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x)) continue;
            
            // if there is a monster in the square
            if ((cave_m_idx[y][x] > 0) && !((y == m_ptr->fy) && (x == m_ptr->fx)))
            {
                monster_type *n_ptr = &mon_list[cave_m_idx[y][x]];
                
                resistance = monster_skill(n_ptr, S_WIL) + 5 + flow_dist(FLOW_MONSTER_NOISE, y, x);

                result = skill_check(m_ptr, song_skill, resistance, n_ptr);
                
                // if the check succeeds, the monster is slowed for at least 2 rounds
                if (result > 0)
                {
                    set_monster_slow(cave_m_idx[y][x], MAX(m_ptr->slowed, 2), mon_list[cave_m_idx[y][x]].ml);
                }
            }
        }
    }
    */

    // if the player is singing the song of silence, then the  monster suffers a penalty
	if (singing(SNG_SILENCE)) song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;
    
    // determine the player's resistance
    // Sil-y: might want to add in the same +5 bonus as against Mastery and Lorien
    resistance = p_ptr->skill_use[S_WIL] + (p_ptr->free_act * 10) + flow_dist(FLOW_MONSTER_NOISE, p_ptr->py, p_ptr->px);

    // Sil-y: ideally we'd use a call to allow_player_slow() here, but that doesn't
    //        work as it can't take the noise distance into account.
    //        Sadly my solution doesn't let you ID free action items.
    result = skill_check(m_ptr, song_skill, resistance, PLAYER);
    
    // if the check succeeds, the player is slowed for at least 2 rounds
    // note that only the first of these affects you as you aren't slow on the round it wears off
    if (result > 0)
    {
        set_slow(MAX(p_ptr->slow, 2));
    }
}

/*
 *  Do the effects of (the monster song) Song of Piercing
 */
void song_of_piercing(monster_type *m_ptr)
{
    int resistance;
    int result;
    int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    char m_name[80];
    cptr description;
    
    int song_skill = 20; // Morgoth's song skill. If more monsters get songs I'll put this in monster.txt

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);
    
    // messages for beginning a new song
    if ((m_ptr->song != SNG_PIERCING) && m_ptr->ml)
    {
        msg_format("%^s begins a song of piercing.", m_name);
        
        // and remember the monsters is now singing this song
        m_ptr->song = SNG_PIERCING;
        
        // disturb if message printed
        disturb(1, 0);
    }
    
    // messages for continuing a song
    else
    {
        switch (dieroll(8))
        {
            case 1:     description = "opening";    break;
            case 2:     description = "treachery";  break;
            case 3:     description = "revealing";  break;
            case 4:     description = "uncovering"; break;
            case 5:     description = "betraying";  break;
            default:    description = "piercing";
        }
        
        if (m_ptr->ml)          msg_format("%^s sings of %s.", m_name, description);
        else if (dist <= 20)    msg_format("You hear a song of %s.", description);
        else if (dist <= 30)    msg_print("You hear singing in the distance.");

        // disturb if message printed
        if (m_ptr->ml || (dist <= 30)) disturb(1, 0);
    }
    
    // if the player is singing the song of silence, then the  monster suffers a penalty
	if (singing(SNG_SILENCE)) song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;
    
    // determine the player's resistance
    resistance = p_ptr->skill_use[S_WIL] + dist + 5;
    
    // perform the skill check
    result = skill_check(m_ptr, song_skill, resistance, PLAYER);
    
    // if the check succeeds, Morgoth knows the player's location
    if (result > 0)
    {
        msg_print("You feel your mind laid bare before Morgoth's will.");
        set_alertness(m_ptr, MIN(result, ALERTNESS_VERY_ALERT));
    }
    
    else if (result > -5)
    {
        msg_print("You feel the force of Morgoth's will searching for the intruder.");
    }
}


/*
 *  Do the effects of (the monster song) Song of Oaths
 */
void song_of_oaths(monster_type *m_ptr)
{
    int y, x;
    int result;
    int range;
    int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    char m_name[80];
    cptr description;

    int song_skill = 21; // Gorthaur's song skill. If more monsters get songs I'll put this in monster.txt

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);
    
    // messages for beginning a new song
    if (m_ptr->song != SNG_OATHS)
    {
        msg_format("%^s begins a song of oaths.", m_name);
        
        // and remember the monsters is now singing this song
        m_ptr->song = SNG_OATHS;
        
        // disturb if message printed
        disturb(1, 0);
    }
    
    // messages for continuing a song
    else
    {
        switch (dieroll(8))
        {
            case 1:     description = "vows broken";        break;
            case 2:     description = "promises";           break;
            case 3:     description = "duty";               break;
            case 4:     description = "tasks forgotten";    break;
            case 5:     description = "redemption";         break;
            default:    description = "oaths";
        }
        
        if (m_ptr->ml)          msg_format("%^s sings of %s.", m_name, description);
        else if (dist <= 20)    msg_format("You hear a song of %s.", description);
        else if (dist <= 30)    msg_print("You hear singing in the distance.");

        // Disturb if message printed
        if (m_ptr->ml || (dist <= 30)) disturb(1, 0);
    }

    // use the monster noise flow to represent the song levels at each square
    update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
    
    // if the player is singing the song of silence, then the  monster suffers a penalty
	if (singing(SNG_SILENCE)) song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;
    
    // perform the skill check
    result = skill_check(m_ptr, song_skill, 15, PLAYER);
    
    // if the check was successful, summon an oathwraith to a nearby square
    if (result > 0)
    {
        // the greatest distance away the wraith can be summoned -- smaller is typically better
        range = MAX(15 - result, 3);
        
        while (TRUE)
        {
            // choose a random square
            y = rand_int(p_ptr->cur_map_hgt);
            x = rand_int(p_ptr->cur_map_wid);
            
            if (!in_bounds(y,x)) continue;
            
            // check the square is empty and close enough
            if (cave_empty_bold(y,x) && flow_dist(FLOW_MONSTER_NOISE, y, x) <= range)
            {
                monster_type *n_ptr;
                
                // place it
                place_monster_one(y, x, R_IDX_OATHWRAITH, TRUE, FALSE, NULL);
                
                n_ptr = &mon_list[cave_m_idx[y][x]];
                
                // message if visible
                if (n_ptr->ml)  msg_print("An Oathwraith appears.");
                
                // mark the wraith as having been summoned
                n_ptr->mflag |= (MFLAG_SUMMONED);

                // let it know where the player is
                set_alertness(n_ptr, ALERTNESS_QUITE_ALERT);
                
                break;
            }
        }
    }

}


/*
 *  Allows you to change the song you are singing to a new one.
 *  If you have the ability 'woven themes' and try to sing a different song, 
 *  it will add it as a theme or change the current theme.
 *  If you have 'woven themes' and choose again to sing the main song, it will cancel any minor theme.
 *  Starting a new song (or changing songs) takes a turn, but ending a song/theme does not.
 */

void change_song(int song)
{
	int song_to_change;
	int old_song;
	
	if (p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES] && (p_ptr->song1 != SNG_NOTHING) && (song != SNG_NOTHING))
	{
		song_to_change = 2;
		old_song = p_ptr->song2;
	}
	else
	{
		song_to_change = 1;
		old_song = p_ptr->song1;
	}

	// attempting to change to the same song
	if (p_ptr->song1 == song)
	{
		// this can cancel minor themes
		if (p_ptr->song2 != SNG_NOTHING)
		{
			song = SNG_NOTHING;
		}
		// but otherwise does nothing
		else if (song != SNG_NOTHING)
		{
			msg_print("You were already singing that.");
			return;
		}
	}
	
	// attempting to change minor theme to itself
	else if ((song_to_change == 2) && (p_ptr->song2 == song))
	{
		msg_print("You are already using that minor theme.");
		return;
	}
			
	// Recalculate various bonuses
	p_ptr->redraw |= (PR_SONG);
	p_ptr->update |= (PU_BONUS);
	
	// swap the minor and major themes
	if (song == SNG_EXCHANGE_THEMES)
	{
		p_ptr->song2 = p_ptr->song1;
		p_ptr->song1 = old_song;
		
		msg_print("You change the order of your themes.");
		
		/* Take time */
		p_ptr->energy_use = 100;
		
		// store the action type
		p_ptr->previous_action[0] = ACTION_MISC;
		
		return;
	}
	
	// Reset wrath counter if stopping singing of slaying
	//if (old_song == SNG_SLAYING)
	//{
	//	p_ptr->wrath = 0;
	//}

	// Reset the song duration counter if changing major theme
	if (song_to_change == 1)
	{
		p_ptr->song_duration = 0;
	}


	switch (song)
	{
		case SNG_NOTHING:
		{
			if ((song_to_change == 1) && (p_ptr->song1 != SNG_NOTHING))
			{
				msg_print("You end your song.");
			}
			else if ((song_to_change == 2) && (p_ptr->song2 != SNG_NOTHING))
			{
				msg_print("You end your minor theme.");
			}
			break;
		}
		case SNG_ELBERETH:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a song to the Queen of the Stars.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme about the Queen of the Stars.");
			}
			else
			{
				msg_print("You change your minor theme to one about the Queen of the Stars.");
			}
			break;
		}
		case SNG_SLAYING:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a song of fury and dread.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme of fury and dread.");
			}
			else
			{
				msg_print("You change your minor theme to one of fury and dread.");
			}
			break;
		}
		case SNG_SILENCE:
		{
			if (song_to_change == 1)
			{
				msg_print("You whisper a song of silence.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme of silence.");
			}
			else
			{
				msg_print("You change your minor theme to one of silence.");
			}
			break;
		}
		case SNG_FREEDOM:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a song of freedom and safe passage.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme of freedom and safe passage.");
			}
			else
			{
				msg_print("You change your minor theme to one of freedom and safe passage.");
			}
			break;
		}
		case SNG_TREES:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a song about the Two Trees of Valinor.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme about the Two Trees of Valinor.");
			}
			else
			{
				msg_print("You change your minor theme to one about the Two Trees of Valinor.");
			}
			msg_print("A memory of their light wells up around you.");
			break;
		}
		case SNG_AULE:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a song of great enchantment.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme of great enchantment.");
			}
			else
			{
				msg_print("You change your minor theme to one of great enchantment.");
			}
			break;
		}
		case SNG_STAYING:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a song about the courage of great heroes past.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme about the courage of great heroes past.");
			}
			else
			{
				msg_print("You change your minor theme to one about the courage of great heroes past.");
			}
			break;
		}
		case SNG_LORIEN:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a soothing song about weariness and rest.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme about weariness and rest.");
			}
			else
			{
				msg_print("You change your minor theme to one about weariness and rest.");
			}
			break;
		}
		case SNG_ESTE:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a song about gentle growth and recovery.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme about gentle growth and recovery.");
			}
			else
			{
				msg_print("You change your minor theme to one about gentle growth and recovery.");
			}
			break;
		}
		case SNG_SHARPNESS:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a whetting song about things that cut deep and true.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme about things that cut deep and true.");
			}
			else
			{
				msg_print("You change your minor theme to one about things that cut deep and true.");
			}
			break;
		}
		case SNG_MASTERY:
		{
			if (song_to_change == 1)
			{
				msg_print("You begin a song of mastery and command.");
			}
			else if (old_song == SNG_NOTHING)
			{
				msg_print("You add a minor theme of mastery and command.");
			}
			else
			{
				msg_print("You change your minor theme to one of mastery and command.");
			}
			break;
		}
	}

	// Actually set the song
	if (song_to_change == 1)
	{
		p_ptr->song1 = song;
	}
	if ((song_to_change == 2) || (song == SNG_NOTHING))
	{
		p_ptr->song2 = song;
	}
	
	// beginning/changing songs takes time
	if (song != SNG_NOTHING)
	{
		/* Take time */
		p_ptr->energy_use = 100;
		
		// store the action type
		p_ptr->previous_action[0] = ACTION_MISC;
	}
		
}

bool singing(int song)
{
	if (song == SNG_NOTHING)
	{
		if (p_ptr->song1 == song) return (TRUE);
	}
	else
	{
		if (p_ptr->song1 == song) return (TRUE);
		if (p_ptr->song2 == song) return (TRUE);
	}
	
	return (FALSE);
}

void sing(void)
{
	int i, type;
	int song = p_ptr->song1; // a default to soothe compilation warnings
	int score = 0;
	int cost = 0;

	if (p_ptr->song1 == SNG_NOTHING)
		return;
		
	// abort song if out of voice, lost the ability to weave themes, or lost either song ability
	if ((p_ptr->csp < 1) || 
	    ((p_ptr->song2 != SNG_NOTHING) && !p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]) ||
	    (!p_ptr->active_ability[S_SNG][p_ptr->song1]) ||
	    ((p_ptr->song2 != SNG_NOTHING) && !p_ptr->active_ability[S_SNG][p_ptr->song2]))
	{
		/* Stop singing */
		change_song(SNG_NOTHING);

		/* Disturb */
		disturb(0, 0);
		return;
	}
	else
	{
		p_ptr->song_duration++;
	}
	
	for (type = 1; type <= 2; type++)
	{
		if (type == 1) song = p_ptr->song1;
		if (type == 2) song = p_ptr->song2;

		score = ability_bonus(S_SNG, song);
		
		switch (song)
		{
			case SNG_ELBERETH:
			{
				cost += 1;
				
				/* Scan all other monsters */
				for (i = mon_max - 1; i >= 1; i--)
				{
					int resistance;
					int result;
					
					/* Access the monster */
					monster_type *m_ptr = &mon_list[i];
					monster_race *r_ptr = &r_info[m_ptr->r_idx];
					
					/* Ignore dead monsters */
					if (!m_ptr->r_idx) continue;
					
					resistance = monster_skill(m_ptr, S_WIL);
					
					// only intelligent monsters are affected
					if (!(r_ptr->flags2 & (RF2_SMART)))  resistance += 100;

					// Morgoth is not affected
					if (m_ptr->r_idx == R_IDX_MORGOTH)   resistance += 100;
					
					// adjust difficulty by the distance to the monster
					result = skill_check(PLAYER, score, resistance + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx), m_ptr);
										
					/* If successful, cause fear in the monster */
					if (result > 0)
					{
						/* Decrease temporary morale */
						m_ptr->tmp_morale -= result * 10;
					}
				}
				break;
			}
			case SNG_SLAYING:
			{
				if ((p_ptr->song_duration % 3) == type - 1) cost += 1;
				break;
			}
			case SNG_SILENCE:
			{
				if ((p_ptr->song_duration % 3) == type - 1) cost += 1;
				break;
			}
			case SNG_FREEDOM:
			{
				if ((p_ptr->song_duration % 3) == type - 1) cost += 1;
                song_of_freedom(score);
				break;
			}
			case SNG_TREES:
			{
				if ((p_ptr->song_duration % 3) == type - 1) cost += 1;
				break;
			}
			case SNG_AULE:
			{
				if ((p_ptr->song_duration % 3) == type - 1) cost += 1;
				break;
			}
			case SNG_STAYING:
			{
				cost += 1;
				break;
			}
			case SNG_LORIEN:
			{
				cost += 1;

				/* Scan all other monsters */
				for (i = mon_max - 1; i >= 1; i--)
				{
					int resistance;
					int result;
					
					/* Access the monster */
					monster_type *m_ptr = &mon_list[i];
					monster_race *r_ptr = &r_info[m_ptr->r_idx];
					monster_lore *l_ptr = &l_list[m_ptr->r_idx];
					
					/* Ignore dead monsters */
					if (!m_ptr->r_idx) continue;
					
					resistance = monster_skill(m_ptr, S_WIL);
					
					// Deal with sleep resistance
					if (r_ptr->flags3 & (RF3_NO_SLEEP))
					{
						resistance += 100;
						if (m_ptr->ml) l_ptr->flags3 |= (RF3_NO_SLEEP);
					}

					// adjust difficulty by the distance to the monster
					result = skill_check(PLAYER, score, resistance + 5 + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx), m_ptr);
					
					/* If successful, (partially) put the monster to sleep */
					if (result > 0)
					{
						set_alertness(m_ptr, m_ptr->alertness - result);
					}
				}
				break;
			}
			case SNG_ESTE:
			{
				cost += 1;
				break;
			}
			case SNG_SHARPNESS:
			{
				cost += 1;
				break;
			}
			case SNG_MASTERY:
			{
				cost += 1;
				break;
			}
		}		
	}
	
	// pay the price of the singing
	if (p_ptr->csp >= cost)		p_ptr->csp -= cost;
	else						p_ptr->csp = 0;
	
	p_ptr->redraw |= (PR_VOICE);
}


