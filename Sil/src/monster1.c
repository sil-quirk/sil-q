/* File: monster1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


/*
 * Pronoun arrays, by gender.
 */
static cptr wd_he[3] =
{ "it", "he", "she" };
static cptr wd_his[3] =
{ "its", "his", "her" };
static cptr wd_him[3] =
{ "it", "him", "her" };


/*
 * Pluralizer.  Args(count, singular, plural)
 */
#define plural(c,s,p) \
	(((c) == 1) ? (s) : (p))

#define PLAYER_GHOST_TRIES_MAX 30

#define MANY_MANY_KILLS 10000

/*
 * Determine if the "armour" is known
 * One kill is needed.
 */
static bool know_armour(s32b kills)
{
	/* Normal monsters */
	if (kills > 0) return (TRUE);

	/* Assume false */
	return (FALSE);
}


/*
 * Determine if the "damage" of the given attack is known.
 * One attack is needed.
 */
static bool know_damage(const monster_lore *l_ptr, int i)
{
	/* Unique monsters */
	if (l_ptr->blows[i]) return (TRUE);

	/* Assume false */
	return (FALSE);
}


static void describe_monster_desc(int r_idx)
{
	const monster_race *r_ptr = &r_info[r_idx];
	char buf[2048];

	/* Simple method */
	my_strcpy(buf, r_text + r_ptr->text, sizeof(buf));

	/* Dump it */
	text_out(buf);
	text_out("\n");
}


static void describe_monster_spells(int r_idx, const monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];
	int m, n;
	int msex = 0;
	int spower;
	bool breath = FALSE;
	bool magic = FALSE;
	int vn;
	cptr vp[64];
	int attack = -1;

	/* Extract a gender (if applicable) */
	if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
	else if (r_ptr->flags1 & RF1_MALE) msex = 1;

	/* Get spell power */
	spower = r_ptr->spell_power;

	/* Collect innate attacks */
	vn = 0;

	if (l_ptr->flags4 & (RF4_ARROW1))
	{
		vp[vn++] = "fire a shortbow";
		attack = 96+0;
	}
	
	if (l_ptr->flags4 & (RF4_ARROW2))
	{
		vp[vn++] = "fire a longbow";
		attack = 96+1;
	}
	
	if (l_ptr->flags4 & (RF4_BOULDER))
	{
		vp[vn++] = "throw boulders";
		attack = 96+2;
	}

	/* Describe innate attacks */
	if (vn)
	{
		/* Intro */
		text_out(format("%^s", wd_he[msex]));

		/* Scan */
		for (n = 0; n < vn; n++)
		{
			/* Intro */
			if (n == 0)
			{
				text_out(" may ");
			}

			else if (n < vn-1) text_out(", ");
			else if (n == 1) text_out(" or ");
			else text_out(", or ");

			/* Dump */
			text_out_c(TERM_L_RED, vp[n]);
		}

		/* Damage */
		if (attack > -1)
		{
			// RF4_ARROW1
            if (attack == 96+0)
			{
				text_out_c(TERM_UMBER, format(" (%+d, 1d%d)", r_ptr->spell_power, get_sides(attack)));
			}
			// RF4_ARROW2
			else if (attack == 96+1)
			{
				text_out_c(TERM_UMBER, format(" (%+d, 2d%d)", r_ptr->spell_power, get_sides(attack)));
			}
			// GF_BOULDER
			else if (attack == 96+2)
			{
				text_out_c(TERM_UMBER, format(" (%+d, 6d%d)", r_ptr->spell_power, get_sides(attack)));
			}
		}

		/* End */
		text_out(".  ");
	}

	/* Collect breaths */
	vn = 0;

	if (l_ptr->flags4 & (RF4_BRTH_FIRE))       vp[vn++] = "fire";
	if (l_ptr->flags4 & (RF4_BRTH_COLD))       vp[vn++] = "frost";
	if (l_ptr->flags4 & (RF4_BRTH_POIS))       vp[vn++] = "poison";
	if (l_ptr->flags4 & (RF4_BRTH_DARK))	   vp[vn++] = "darkness";

	/* Describe breaths */
	if (vn)
	{
		/* Note breath */
		breath = TRUE;

		/* Intro */
		text_out(format("%^s", wd_he[msex]));

		/* Scan */
		for (n = 0; n < vn; n++)
		{
			/* Intro */
			if (n == 0) text_out(" may breathe ");
			else if (n < vn-1) text_out(", ");
			else if (n == 1) text_out(" or ");
			else text_out(", or ");

			/* Dump */
			text_out_c(TERM_L_RED, vp[n]);
		}
		
		text_out(format(" (%dd%d). ", spower, 4));
	}


	/* Collect spells */
	vn = 0;

	if (l_ptr->flags4 & (RF4_EARTHQUAKE))   vp[vn++] = "cause earthquakes";
	if (l_ptr->flags4 & (RF4_SHRIEK))		vp[vn++] = "call for help";
	if (l_ptr->flags4 & (RF4_SCREECH))		vp[vn++] = "cause a hideous screeching";
	if (l_ptr->flags4 & (RF4_DARKNESS))     vp[vn++] = "create darkness";
	if (l_ptr->flags4 & (RF4_FORGET))       vp[vn++] = "remove memories";
	if (l_ptr->flags4 & (RF4_SCARE))		vp[vn++] = "terrify";
	if (l_ptr->flags4 & (RF4_CONF))			vp[vn++] = "confuse";
	if (l_ptr->flags4 & (RF4_HOLD))			vp[vn++] = "entrance";
	if (l_ptr->flags4 & (RF4_SLOW))			vp[vn++] = "slow";

	m = vn;

	/* Describe spells */
	if (vn)
	{
		/* Note magic */
		magic = TRUE;

		/* Intro */
		text_out(format("%^s may ", wd_he[msex]));

		/* Normal spells */
		for (n = 0; n < m; n++)
		{
			if (n == 0)       text_out("attempt to ");
			else if (n < m-1) text_out(", ");
			else if (n != 1)  text_out(", or ");
			else              text_out(" or ");

			/* Dump */
			text_out_c(TERM_ORANGE, vp[n]);
		}

		/* End this sentence */
		text_out(".  ");
	}
	
}


static void describe_monster_drop(int r_idx, const monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];

	bool sin = FALSE;

	int n;

	cptr p;

	int msex = 0;


	/* Extract a gender (if applicable) */
	if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
	else if (r_ptr->flags1 & RF1_MALE) msex = 1;

	/* Drops items */
	if (l_ptr->drop_item)
	{
		if (r_ptr->flags2 & (RF2_TERRITORIAL))
		{
			/* Intro */
			text_out(format("%^s may be found with", wd_he[msex]));
		}
		else
		{
			/* Intro */
			text_out(format("%^s may carry", wd_he[msex]));
		}

		/* Count maximum drop */
		n = l_ptr->drop_item;

		/* One drop (may need an "n") */
		if (n == 1)
		{
			text_out(" a");
			sin = TRUE;
		}

		/* Two drops */
		else if (n == 2)
		{
			text_out(" one or two");
		}

		/* Many drops */
		else
		{
			text_out(format(" up to %d", n));
		}


		/* Chests are not noted as good or great
		 * (no "n" needed)
		 */
		if (l_ptr->flags1 & RF1_DROP_CHEST)
		{
			p = NULL;
			sin = FALSE;
		}

		/* Great */
		else if (l_ptr->flags1 & RF1_DROP_GREAT)
		{
			p = " exceptional";
		}

		/* Good (no "n" needed) */
		else if (l_ptr->flags1 & RF1_DROP_GOOD)
		{
			p = " good";
			sin = FALSE;
		}

		/* Okay */
		else
		{
			p = NULL;
		}


		/* Objects */
		if (l_ptr->drop_item)
		{
			/* Handle singular "an" */
			if (sin) text_out("n");
			sin = FALSE;

			/* Dump "object(s)" */
			if (p) text_out(p);

			/*specify chests where needed*/
			if (l_ptr->flags1 & RF1_DROP_CHEST) text_out(" chest");
			else text_out(" object");
			if (n != 1) text_out("s");
		}

		/* End this sentence */
		text_out(".  ");
	}
}


static void describe_monster_attack(int r_idx, const monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];
	int m, r, n;
	cptr p, q;

	int msex = 0;

	/* Extract a gender (if applicable) */
	if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
	else if (r_ptr->flags1 & RF1_MALE) msex = 1;

	/* Count the number of "known" attacks */
	for (n = 0, m = 0; m < MONSTER_BLOW_MAX; m++)
	{
		/* Skip non-attacks */
		if (!r_ptr->blow[m].method) continue;

		/* Count known attacks */
		if ((l_ptr->blows[m]) || (l_ptr->tsights == MAX_SHORT) ||
			                      (l_ptr->ranged == MAX_UCHAR)) n++;
	}

	/* Examine (and count) the actual attacks */
	for (r = 0, m = 0; m < MONSTER_BLOW_MAX; m++)
	{
		int method, effect, att, d1, d2;

		/* Skip non-attacks */
		if (!r_ptr->blow[m].method) continue;

		/* Skip unknown attacks */
		if (!l_ptr->blows[m]) continue;

		/* Extract the attack info */
		method = r_ptr->blow[m].method;
		effect = r_ptr->blow[m].effect;
		att = r_ptr->blow[m].att;
		d1 = r_ptr->blow[m].dd;
		d2 = r_ptr->blow[m].ds;

		/* No method yet */
		p = NULL;

		/* Get the method */
		switch (method)
		{
			case RBM_HIT:           p = "hit"; break;
			case RBM_TOUCH:         p = "touch (ignoring armour)"; break;
			case RBM_CLAW:          p = "claw"; break;
			case RBM_BITE:          p = "bite"; break;
			case RBM_PECK:          p = "peck"; break;
			case RBM_STING:         p = "sting"; break;
			case RBM_WHIP:          p = "whip"; break;
			case RBM_CRUSH:         p = "crush"; break;
			case RBM_ENGULF:        p = "engulf"; break;
			case RBM_CRAWL:         p = "crawl on you"; break;
			case RBM_THORN:         p = "pierce you with thorns"; break;
			case RBM_SPORE:         p = "release spores (ignoring evasion and armour)"; break;
		}


		/* Default effect */
		q = NULL;

		/* Get the effect */
		switch (effect)
		{
			case RBE_HURT:          q = "attack"; break;
			case RBE_WOUND:         q = "wound"; break;
			case RBE_BATTER:        q = "stun"; break;
			case RBE_SHATTER:       q = "cause earthquakes"; break;

			case RBE_UN_BONUS:      q = "disenchant"; break;
			case RBE_UN_POWER:      q = "drain charges"; break;
			case RBE_LOSE_MANA:     q = "drain mana"; break;
			case RBE_SLOW:          q = "slow"; break;
			case RBE_EAT_ITEM:      q = "steal items"; break;
			case RBE_EAT_FOOD:      q = "eat your food"; break;
			case RBE_HUNGER:        q = "cause hunger"; break;

			case RBE_POISON:        q = "poison"; break;
			case RBE_ACID:          q = "corrode"; break;
			case RBE_ELEC:          q = "shock"; break;
			case RBE_FIRE:          q = "burn"; break;
			case RBE_COLD:          q = "freeze"; break;
			case RBE_DARK:          q = "darken"; break;

			case RBE_BLIND:         q = "blind"; break;
			case RBE_CONFUSE:       q = "confuse"; break;
			case RBE_TERRIFY:       q = "terrify"; break;
			case RBE_ENTRANCE:      q = "entrance"; break;
			case RBE_HALLU:         q = "induce hallucinations"; break;
			case RBE_DISEASE:       q = "cause disease"; break;

			case RBE_LOSE_STR:      q = "reduce strength"; break;
			case RBE_LOSE_DEX:      q = "reduce dexterity"; break;
			case RBE_LOSE_CON:      q = "reduce constitution"; break;
			case RBE_LOSE_GRA:      q = "reduce grace"; break;
			case RBE_LOSE_STR_CON:  q = "reduce strength and constitution"; break;
			case RBE_LOSE_ALL:      q = "reduce all stats"; break;

			case RBE_DISARM:        q = "disarm"; break;
		}

		/* Introduce the attack description */
		if (!r)
		{
			text_out(format("%^s can ", wd_he[msex]));
		}
		else if (r < n-1)
		{
			text_out(", ");
		}
		else
		{
			text_out(", or ");
		}


		/* Hack -- force a method */
		if (!p) p = "do something weird";

		/* Describe the method */
		text_out(p);

		/* Describe the effect (if any) */
		if (q)
		{
			/* Describe the attack type */
			text_out(" to ");
			text_out_c(TERM_L_RED, q);

			/* Describe damage (if known) */
			if ((know_damage(l_ptr, m)) || (l_ptr->tsights == MAX_SHORT) || (l_ptr->ranged == MAX_UCHAR))
			{
				if (d2)
				{
					/* Display the damage */
					text_out_c(TERM_L_WHITE, format(" (%+d, %dd%d)", att, d1, d2));
				}
				else
				{
					/* Display the attack rating */
					text_out_c(TERM_L_WHITE, format(" (%+d)", att));
				}
			}
		}


		/* Count the attacks as printed */
		r++;
	}

	/* Finish sentence above */
	if (r)
	{
		text_out(".  ");
	}

	/* Notice lack of attacks */
	else if (l_ptr->flags1 & RF1_NEVER_BLOW)
	{
		text_out(format("%^s has no physical attacks.  ", wd_he[msex]));
	}

	/* Or describe the lack of knowledge */
	else
	{
		text_out(format("Nothing is known about %s attack.  ", wd_his[msex]));
	}

}


static void describe_monster_abilities(int r_idx, const monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];

	int n;

	int vn;
	cptr vp[64];

	int msex = 0;

	/* Extract a gender (if applicable) */
	if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
	else if (r_ptr->flags1 & RF1_MALE) msex = 1;

    /* Collect Abilities */
    vn = 0;
    if (r_ptr->flags2 & (RF2_ELFBANE))            vp[vn++] = "elf-bane";      // elf-bane is obvious
    if (l_ptr->flags2 & (RF2_CHARGE))             vp[vn++] = "charge";
    if (l_ptr->flags2 & (RF2_KNOCK_BACK))         vp[vn++] = "knock back";
    if (l_ptr->flags2 & (RF2_CRIPPLING) )         vp[vn++] = "crippling shot";
    if (l_ptr->flags2 & (RF2_CRUEL_BLOW))         vp[vn++] = "cruel blow";
    if (l_ptr->flags2 & (RF2_OPPORTUNIST))        vp[vn++] = "opportunist";
    if (l_ptr->flags2 & (RF2_ZONE_OF_CONTROL))    vp[vn++] = "zone of control";
    if (l_ptr->flags2 & (RF2_EXCHANGE_PLACES))    vp[vn++] = "exchange places";
    if (l_ptr->flags2 & (RF2_RIPOSTE))            vp[vn++] = "riposte";
    if (l_ptr->flags2 & (RF2_FLANKING))           vp[vn++] = "flanking";
    if (l_ptr->flags4 & (RF4_SNG_BINDING))        vp[vn++] = "song of binding";
    if (l_ptr->flags4 & (RF4_SNG_PIERCING))       vp[vn++] = "song of piercing";
    if (l_ptr->flags4 & (RF4_SNG_OATHS))          vp[vn++] = "song of oaths";
   
    /* Describe Abilities */
	if (vn)
	{
		/* Intro */
		text_out(format("%^s", wd_he[msex]));
        
		/* Scan */
		for (n = 0; n < vn; n++)
		{
			/* Intro */
			if ((n == 0) && (vn > 1))   text_out(" has the abilities: ");
            else if (n == 0)            text_out(" has the ability: ");
			else if (n < vn-1)          text_out(", ");
			else                        text_out(" and ");
            
			/* Dump */
			text_out_c(TERM_RED, vp[n]);
		}
        
		/* End */
		text_out(".  ");
	}

    
	/* Collect special abilities. */
	vn = 0;
	if (r_ptr->light > 0)
	{
		/*humaniods carry torches, others glow*/
		if (!strchr("@G", r_ptr->d_char)) vp[vn++] = "radiate light";
		else vp[vn++] = "use a light source";
	}
	if (r_ptr->light < 0) vp[vn++] = "produce an unnatural darkness";
	if (l_ptr->flags2 & RF2_FLYING) vp[vn++] = "fly";
	if (l_ptr->flags2 & RF2_OPEN_DOOR) vp[vn++] = "open doors";
	if (l_ptr->flags2 & RF2_PASS_DOOR) vp[vn++] = "pass through doors";
	if (l_ptr->flags2 & RF2_UNLOCK_DOOR) vp[vn++] = "unlock doors";
	if (l_ptr->flags2 & RF2_BASH_DOOR) vp[vn++] = "bash down doors";
	if (l_ptr->flags2 & RF2_PASS_WALL) vp[vn++] = "pass through walls";
	if (l_ptr->flags2 & RF2_KILL_WALL) vp[vn++] = "bore through walls";
	if (l_ptr->flags2 & RF2_TUNNEL_WALL) vp[vn++] = "tunnel through walls";
	if (l_ptr->flags2 & RF2_KILL_BODY) vp[vn++] = "destroy weaker monsters";
	if (l_ptr->flags2 & RF2_TAKE_ITEM) vp[vn++] = "pick up objects";
	if (l_ptr->flags2 & RF2_KILL_ITEM) vp[vn++] = "destroy objects";

	/* Describe special abilities. */
	if (vn)
	{
		/* Intro */
		text_out(format("%^s", wd_he[msex]));

		/* Scan */
		for (n = 0; n < vn; n++)
		{
			/* Intro */
			if (n == 0) text_out(" can ");
			else if (n < vn-1) text_out(", ");
			else text_out(" and ");

			/* Dump */
			text_out(vp[n]);
		}

		/* End */
		text_out(".  ");
	}

	/* Describe special abilities. */
	if (l_ptr->flags2 & RF2_INVISIBLE)
	{
		text_out(format("%^s is very difficult to see.  ", wd_he[msex]));
	}
	if (l_ptr->flags2 & RF2_MULTIPLY)
	{
		text_out(format("%^s breeds explosively.  ", wd_he[msex]));
	}
	if (l_ptr->flags2 & RF2_REGENERATE)
	{
		text_out(format("%^s regenerates quickly.  ", wd_he[msex]));
	}
	if (l_ptr->flags1 & RF1_NO_CRIT)
	{
		text_out(format("%^s cannot be critically hit.  ", wd_he[msex]));
	}
	if (l_ptr->flags1 & RF1_RES_CRIT)
	{
		text_out(format("%^s is resistant to critical hits.  ", wd_he[msex]));
	}
	
	if (l_ptr->flags2 & (RF2_CLOUD_SURROUND))
	{
		int typ = 0, dd = 0, ds = 0, rad = 0;

		/* Get type of cloud */
		cloud_surround(r_idx, &typ, &dd, &ds, &rad);

		/*hack - alter type for char-attr monster*/

		if ((r_ptr->flags4 & (RF4_BRTH_FIRE)) &&
			(r_ptr->flags4 & (RF4_BRTH_POIS)) &&
			(r_ptr->flags4 & (RF4_BRTH_DARK)) &&
			(r_ptr->flags4 & (RF4_BRTH_COLD)))
			{
				text_out(format("%^s is surrounded by shimmering haze of ever changing elements (%dd%d).  ", wd_he[msex], dd, ds));
			}


		/* We emit something */
		else if (typ)
		{
			text_out(format("%^s is surrounded by a shimmering haze of ", wd_he[msex]));

			/* Describe cloud */
			if (typ == GF_FIRE)           text_out_c(TERM_L_RED, "fire");
			else if (typ == GF_COLD)      text_out_c(TERM_L_RED, "frost");
			else if (typ == GF_POIS)      text_out_c(TERM_L_RED, "poison");
			else if (typ == GF_DARK)      text_out_c(TERM_L_RED, "darkness");
			text_out(format(" (%dd%d).  ", dd, ds));
		}
	}

	/* Describe intelligence. */
	if (r_ptr->flags2 & RF2_MINDLESS)
	{
		text_out(format("%^s is mindless.  ", wd_he[msex]));
	}
	else if (r_ptr->flags2 & RF2_SMART)
	{
		//text_out(format("%^s is a servant of the dark.  ", wd_he[msex]));
	}
	else
	{
		text_out(format("%^s is unintelligent.  ", wd_he[msex]));
	}
	
	/* Collect susceptibilities */
	vn = 0;
	if (l_ptr->flags3 & RF3_STONE) vp[vn++] = "shattering";
	if (l_ptr->flags3 & RF3_HURT_LITE) vp[vn++] = "bright light";
	if (l_ptr->flags3 & RF3_HURT_FIRE) vp[vn++] = "fire";
	if (l_ptr->flags3 & RF3_HURT_COLD) vp[vn++] = "cold";

	/* Describe susceptibilities */
	if (vn)
	{
		/* Intro */
		text_out(format("%^s", wd_he[msex]));

		/* Scan */
		for (n = 0; n < vn; n++)
		{
			/* Intro */
			if (n == 0) text_out(" is vulnerable to ");
			else if (n < vn-1) text_out(", ");
			else text_out(" and ");

			/* Dump */
			text_out_c(TERM_L_BLUE, vp[n]);
		}

		/* End */
		text_out(".  ");
	}


	/* Collect resistances */
	vn = 0;
	if (l_ptr->flags3 & RF3_RES_ELEC) vp[vn++] = "lightning";
	if (l_ptr->flags3 & RF3_RES_FIRE) vp[vn++] = "fire";
	if (l_ptr->flags3 & RF3_RES_COLD) vp[vn++] = "cold";
	if (l_ptr->flags3 & RF3_RES_POIS) vp[vn++] = "poison";

	/* Describe immunities */
	if (vn)
	{
		/* Intro */
		text_out(format("%^s", wd_he[msex]));

		/* Scan */
		for (n = 0; n < vn; n++)
		{
			/* Intro */
			if (n == 0) text_out(" resists ");
			else if (n < vn-1) text_out(", ");
			else text_out(" and ");

			/* Dump */
			text_out(vp[n]);
		}

		/* End */
		text_out(".  ");
	}

	/* Collect non-effects */
	vn = 0;
	if (l_ptr->flags3 & RF3_NO_SLOW) vp[vn++] = "slowed";
	if (l_ptr->flags3 & RF3_NO_STUN) vp[vn++] = "stunned";
	if (l_ptr->flags3 & RF3_NO_FEAR) vp[vn++] = "frightened";
	if (l_ptr->flags3 & RF3_NO_CONF) vp[vn++] = "confused";
	if (l_ptr->flags3 & RF3_NO_SLEEP) vp[vn++] = "put to sleep";

	/* Describe non-effects */
	if (vn)
	{
		/* Intro */
		text_out(format("%^s", wd_he[msex]));

		/* Scan */
		for (n = 0; n < vn; n++)
		{
			/* Intro */
			if (n == 0) text_out(" cannot be ");
			else if (n < vn-1) text_out(", ");
			else text_out(" or ");

			/* Dump */
			text_out_c(TERM_YELLOW, vp[n]);
		}

		/* End */
		text_out(".  ");
	}
    
	/* Describe escort */
	if (l_ptr->flags1 & RF1_ESCORT)
	{
		text_out(format("%^s usually appears with escorts.  ",
		            wd_he[msex]));
	}
	
	/* Describe escorts */
	if (l_ptr->flags1 & RF1_ESCORTS)
	{
		text_out(format("%^s usually appears with many escorts.  ",
						wd_he[msex]));
	}
	
	/* Describe friend */
	else if (l_ptr->flags1 & RF1_FRIEND)
	{
		text_out(format("%^s sometimes appears in groups.  ",
		            wd_he[msex]));
	}
	
	/* Describe friends */
	else if (l_ptr->flags1 & RF1_FRIENDS)
	{
		text_out(format("%^s usually appears in groups.  ",
						wd_he[msex]));
	}
}


static void describe_monster_kills(int r_idx, const monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];

	int msex = 0;

	bool out = TRUE;
	
	int real_tkills;

	/* Extract a gender (if applicable) */
	if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
	else if (r_ptr->flags1 & RF1_MALE) msex = 1;

	// determine the real number of ancestor kills for printing purposes
	if (l_ptr->tkills < MANY_MANY_KILLS) real_tkills = l_ptr->tkills;
	else								 real_tkills = l_ptr->tkills - MANY_MANY_KILLS;
	
	/* Treat uniques differently */
	if (l_ptr->flags1 & RF1_UNIQUE)
	{
		/* Determine if the unique is "dead" */
		bool dead = (r_ptr->max_num == 0) ? TRUE : FALSE;

		/* We've been killed... */
		if (l_ptr->deaths)
		{
			/* Killed predecessors */
			text_out(format("%^s has slain %d of your predecessors",
			            wd_he[msex], l_ptr->deaths));

			/* But we've also killed it */
			if (dead)
			{
				text_out(", but you have taken revenge!  ");
			}

			/* Unavenged (ever) */
			else
			{
				text_out(format(", who %s unavenged.  ",
				            plural(l_ptr->deaths, "remains", "remain")));
			}
		}

		/* Dead unique who never hurt us */
		else if (dead)
		{
			text_out("You have slain this foe.  ");
		}
		
		if (!dead)
		{
			if (l_ptr->psights)
			{
				text_out("You have encountered this foe.  ");
			}
			else
			{
				text_out("You are yet to encounter this foe.  ");
			}
		}
	}

	/* Not unique, but killed us */
	else if (l_ptr->deaths)
	{
		/* Dead predecessors */
		text_out(format("%d of your predecessors %s been killed by this creature, ",
		            l_ptr->deaths, plural(l_ptr->deaths, "has", "have")));

		/* Some kills this life */
		if (l_ptr->pkills)
		{
			text_out(format("and you have slain %d of the %d you have encountered.  ",
			            l_ptr->pkills, l_ptr->psights));
		}

		/* Some kills past lives */
		else if (real_tkills)
		{
			text_out(format("and %s have slain %d in return.  ",
			            "your predecessors", real_tkills));
						
			if (l_ptr->psights)
			{
				text_out(format("You have encountered %d.  ",
								l_ptr->psights));
			}
			else
			{
				text_out("You are yet to encounter one.  ");
			}
		}

		/* No kills */
		else
		{
			text_out_c(TERM_RED, format("and it is not known to have been defeated.  "));

			if (l_ptr->psights)
			{
				text_out(format("You have encountered %d.  ",
								l_ptr->psights));
			}
			else
			{
				text_out("You are yet to encounter one.  ");
			}
		}
	}

	/* Normal monsters */
	else
	{
		/* Encountered some this life */
		if (l_ptr->psights && !l_ptr->pkills)
		{
			text_out(format("You have encountered %d of these creatures, ",
							l_ptr->psights));
							
			/* Killed some last life */
			if (real_tkills)
			{
				text_out(format("and your predecessors have slain %d.  ",
								real_tkills));
			}
			
			/* Killed none */
			else
			{
				text_out("but no battles to the death are recalled.  ");
			}
		}
		
		/* Killed some this life */
		else if (l_ptr->pkills)
		{
			text_out(format("You have slain %d of the %d you have encountered.  ",  l_ptr->pkills, l_ptr->psights));
		}

		/* Encountered some this life */
		else
		{
			text_out(format("You have encountered none of these creatures, ",
							l_ptr->psights));

			/* Killed some this life -- should be impossible to reach here */
			if (l_ptr->pkills)
			{
				text_out(format("but slain %d.  ",  l_ptr->pkills));
			}
			
			/* Killed some last life */
			else if (real_tkills)
			{
				text_out(format("but your predecessors have slain %d.  ",
								real_tkills));
			}
			
			/* Killed none */
			else
			{
				text_out("and no battles to the death are recalled.  ");
			}
		}
		


	}

	/* Separate */
	if (out) text_out("\n");
}


static void describe_monster_toughness(int r_idx, const monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];

	int msex = 0;

	/* Extract a gender (if applicable) */
	if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
	else if (r_ptr->flags1 & RF1_MALE) msex = 1;

	/* Describe monster "toughness" */
	if ((know_armour(l_ptr->tkills)) || (l_ptr->tsights == MAX_SHORT) ||
			 (l_ptr->ranged == MAX_UCHAR))
	{
		// Health
		text_out(format("%^s has ", wd_he[msex]));
		if (r_ptr->flags1 & (RF1_UNIQUE))
		{
			text_out_c(TERM_GREEN, format("%d ", r_ptr->hdice * (1 + r_ptr->hside) / 2));
		}
		else
		{
			text_out_c(TERM_GREEN, format("%dd%d ", r_ptr->hdice, r_ptr->hside));
		}
		text_out("health ");
		
		// Evasion and Armour
		if ((r_ptr->pd > 0) && (r_ptr->ps > 0))
		{
			text_out("and a defence of ");
			text_out_c(TERM_SLATE, format("[%+d, %dd%d].  ",
										  r_ptr->evn, r_ptr->pd, r_ptr->ps));
		}
		else
		{
			text_out("and a defence of ");
			text_out_c(TERM_SLATE, format("[%+d].  ",
										  r_ptr->evn));
		}
		
	}
}


static void describe_monster_skills(int r_idx, const monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];
		
	int msex = 0;
	cptr act;
		
	/* Extract a gender (if applicable) */
	if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
	else if (r_ptr->flags1 & RF1_MALE) msex = 1;
	
	/* Describe experience if known */
	if ((l_ptr->ranged == MAX_UCHAR) || ((l_ptr->tsights > 1) && (10 - l_ptr->tsights < p_ptr->skill_use[S_PER])))
	{
		text_out(format("%^s has %d Will,", wd_he[msex], r_ptr->wil));
		if (p_ptr->active_ability[S_PER][PER_LISTEN]) text_out(format(" %d Stealth,", r_ptr->stl));
		text_out(format(" %d Perception", r_ptr->per));
					
		if (r_ptr->sleep > 20)				// 21 +
		{
			act = "is usually found asleep";
		}
		else if (r_ptr->sleep > 15)			// 16 to 20
		{
			act = "is often found asleep";
		}
		else if (r_ptr->sleep > 10)			// 11 to 15
		{
			act = "is sometimes found asleep";
		}
		else if (r_ptr->sleep > 5)			// 6 to 10
		{
			act = "is never found asleep";
		}
		else if (r_ptr->sleep > 1)			// 2 to 5 
		{
			act = "is quick to notice intruders";
		}
		else if (r_ptr->sleep > 0)			// 1
		{
			act = "is very quick to notice intruders";
		}
		else								// 0
		{
			act = "is ever vigilant";
		}
		
		if (r_ptr->flags2 & (RF2_MINDLESS))
		{
			text_out(".  ");
		}
		else
		{
			text_out(format(", and %s.  ", act));
		}
	}
}


static void describe_monster_exp(int r_idx, const monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];

	long i;
	
	int msex = 0;
	
	/* Extract a gender (if applicable) */
	if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
	else if (r_ptr->flags1 & RF1_MALE) msex = 1;
	
	/* Describe experience if known */
	if (l_ptr->tkills || l_ptr->tsights)
	{
		/* Introduction for Encounters */
		if (l_ptr->psights)
		{
			if (l_ptr->flags1 & RF1_UNIQUE)
			{
				text_out(format("Encountering %s was worth", wd_him[msex]));
			}
			else
				text_out("Encountering another would be worth");
		}
		else
		{
			if (l_ptr->flags1 & RF1_UNIQUE)
				text_out(format("Encountering %s would be worth", wd_him[msex]));
			else
				text_out("Encountering one would be worth");
		}
		
		/* calculate the integer exp part */
		i = adjusted_mon_exp(r_ptr, FALSE);
		
		/* Mention the experience */
		text_out(format(" %ld experience.  ", (long) i));

		

		/* Introduction for Kills */
		if (l_ptr->pkills)
		{
			if (l_ptr->flags1 & RF1_UNIQUE)
				text_out(format("Killing %s was worth", wd_him[msex]));
			else
				text_out("Killing another would be worth");
		}
		else
		{
			if (l_ptr->flags1 & RF1_UNIQUE)
				text_out(format("Killing %s would be worth", wd_him[msex]));
			else
				text_out("Killing one would be worth");
		}

		/* calculate the integer exp part */
		i = adjusted_mon_exp(r_ptr, TRUE);

		/* Mention the experience */
		text_out(format(" %ld.  ", (long) i));
	}
}


static void describe_monster_movement(int r_idx, const monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];

	bool old = FALSE;

	text_out("This");

	if (l_ptr->flags3 & RF3_UNDEAD) text_out_c(TERM_L_BLUE, " undead");

	if (l_ptr->flags3 & RF3_DRAGON) text_out_c(TERM_L_BLUE, " dragon");
	else if (l_ptr->flags3 & RF3_SERPENT) text_out_c(TERM_L_BLUE, " serpent");
	else if (l_ptr->flags3 & RF3_RAUKO) text_out_c(TERM_L_BLUE, " rauko");
	else if (l_ptr->flags3 & RF3_TROLL) text_out_c(TERM_L_BLUE, " troll");
	else if (l_ptr->flags3 & RF3_ORC) text_out_c(TERM_L_BLUE, " orc");
	else if (l_ptr->flags3 & RF3_WOLF) text_out_c(TERM_L_BLUE, " wolf");
	else if (l_ptr->flags3 & RF3_SPIDER) text_out_c(TERM_L_BLUE, " spider");
	else text_out(" creature");

	/* Describe location */
	if (r_ptr->level == 0)
	{
		text_out_c(TERM_YELLOW, " dwells at the gates of Angband");
		old = TRUE;
	}
	else
	{
		if (l_ptr->flags1 & RF1_FORCE_DEPTH)
			text_out(" is found ");
		else
			text_out(" is normally found ");

		if (r_idx == R_IDX_CARCHAROTH)
		{
			text_out_c(TERM_YELLOW, "guarding the gates of Angband");
		}
		else if (r_ptr->level < MORGOTH_DEPTH)
		{
			text_out("at depths of ");
			text_out_c(TERM_YELLOW, format("%d feet", r_ptr->level * 50));
		}
		else
		{
			text_out("at depths of ");
			text_out_c(TERM_YELLOW, format("%d feet", MORGOTH_DEPTH * 50));
		}
		old = TRUE;
	}

	/* Deal with non-moving creatures */
	if (l_ptr->flags1 & RF1_NEVER_MOVE)
	{
		if (old) text_out(", and");
		text_out(" cannot move");
	}

	/* Deal with non-moving creatures */
	else if (l_ptr->flags1 & RF1_HIDDEN_MOVE)
	{
		if (old) text_out(", and");
		text_out(" never moves when you are looking");
	}
	
	/* most other creatures display their speed */
	else if ((r_ptr->speed != 2) || (l_ptr->flags1 & RF1_RAND_50) || (l_ptr->flags1 & RF1_RAND_25))
	{
		if (old) text_out(", and");
		text_out(" moves");
		
		/* Random-ness */
		if ((l_ptr->flags1 & RF1_RAND_50) || (l_ptr->flags1 & RF1_RAND_25))
		{
			/* Adverb */
			if ((l_ptr->flags1 & RF1_RAND_50) && (l_ptr->flags1 & RF1_RAND_25))
			{
				text_out(" extremely");
			}
			else if (l_ptr->flags1 & RF1_RAND_50)
			{
				text_out(" somewhat");
			}
			else if (l_ptr->flags1 & RF1_RAND_25)
			{
				text_out(" a bit");
			}
			
			/* Adjective */
			text_out(" erratically");
			
			/* Hack -- Occasional conjunction */
			if (r_ptr->speed != 2) text_out(", and");
		}
		
		
		/* Speed */
		if (r_ptr->speed > 2)
		{
			
			if (r_ptr->speed > 5) text_out_c(TERM_L_GREEN, " incredibly");
			else if (r_ptr->speed > 4) text_out_c(TERM_L_GREEN, " extremely");
			else if (r_ptr->speed > 3) text_out_c(TERM_L_GREEN, " very");
			text_out_c(TERM_L_GREEN, " quickly");
			
		}
		else if (r_ptr->speed < 2)
		{
			text_out_c(TERM_L_UMBER, " slowly");
		}
	}



	/* End this sentence */
	text_out(".  ");
    
	/*note if this monster does not pursue you*/
	if (l_ptr->flags2 & (RF2_TERRITORIAL))
	{
		int msex = 0;
		
		/* Extract a gender (if applicable) */
		if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
		else if (r_ptr->flags1 & RF1_MALE) msex = 1;
		
		text_out(format("%^s does not deign to pursue you.  ", wd_he[msex]));
	}
	
}


/*
 * Learn everything about a monster (by cheating)
 */
static void cheat_monster_lore(int r_idx, monster_lore *l_ptr)
{
	const monster_race *r_ptr = &r_info[r_idx];

	int i;

	/* Hack -- Maximal predecessors kills */
	if (l_ptr->tkills < MANY_MANY_KILLS) l_ptr->tkills += MANY_MANY_KILLS;

	/* Hack -- Maximal info */
	l_ptr->notice = l_ptr->ignore = MAX_UCHAR;

	/* Observe "maximal" attacks */
	for (i = 0; i < MONSTER_BLOW_MAX; i++)
	{
		/* Examine "actual" blows */
		if (r_ptr->blow[i].effect || r_ptr->blow[i].method)
		{
			/* Hack -- maximal observations */
			l_ptr->blows[i] = MAX_UCHAR;
		}
	}

	/* Hack -- maximal drops */
	l_ptr->drop_item =
	(((r_ptr->flags1 & RF1_DROP_4D2) ? 8 : 0) +
	 ((r_ptr->flags1 & RF1_DROP_3D2) ? 6 : 0) +
	 ((r_ptr->flags1 & RF1_DROP_2D2) ? 4 : 0) +
	 ((r_ptr->flags1 & RF1_DROP_1D2) ? 2 : 0) +
	 ((r_ptr->flags1 & RF1_DROP_100)  ? 1 : 0) +
	 ((r_ptr->flags1 & RF1_DROP_33)  ? 1 : 0));
	
	/* Hack -- observe many spells */
	l_ptr->ranged = MAX_UCHAR;

	/* Hack -- know all the flags */
	l_ptr->flags1 = r_ptr->flags1;
	l_ptr->flags2 = r_ptr->flags2;
	l_ptr->flags3 = r_ptr->flags3;
	l_ptr->flags4 = r_ptr->flags4;
}


/*
 * Hack -- display monster information using "roff()"
 *
 * Note that there is now a compiler option to only read the monster
 * descriptions from the raw file when they are actually needed, which
 * saves about 60K of memory at the cost of disk access during monster
 * recall, which is optional to the user.
 *
 * This function should only be called with the cursor placed at the
 * left edge of the screen, on a cleared line, in which the recall is
 * to take place.  One extra blank line is left after the recall.
 */
void describe_monster(int r_idx, bool spoilers)
{
	monster_lore lore;

	monster_lore save_mem;

	/* Get the race and lore */
	const monster_race *r_ptr = &r_info[r_idx];
	monster_lore *l_ptr = &l_list[r_idx];

	/* Cheat -- know everything */
	if ((cheat_know) || p_ptr->active_ability[S_PER][PER_LORE2])
	{
		/* XXX XXX XXX */

		/* Hack -- save memory */
		COPY(&save_mem, l_ptr, monster_lore);
	}

	/* Hack -- create a copy of the monster-memory */
	COPY(&lore, l_ptr, monster_lore);

	/* Assume some "obvious" flags */
	lore.flags1 |= (r_ptr->flags1 & RF1_OBVIOUS_MASK);

	/* Killing a monster reveals some properties */
	if (lore.tkills)
	{
		/* Know "race" flags */
		lore.flags3 |= (r_ptr->flags3 & RF3_RACE_MASK);

		/* Know "forced" flags */
		lore.flags1 |= (r_ptr->flags1 & (RF1_FORCE_DEPTH));
	}

	/* Cheat -- know everything */
	if ((cheat_know) || p_ptr->active_ability[S_PER][PER_LORE2] || spoilers)
	{
		cheat_monster_lore(r_idx, &lore);
	}

	/* Show kills of monster vs. player(s) */
	if (!spoilers)
		describe_monster_kills(r_idx, &lore);

	/* Monster description */
	describe_monster_desc(r_idx);

	/* Describe the movement and level of the monster */
	describe_monster_movement(r_idx, &lore);

	/* Describe spells and innate attacks */
	describe_monster_spells(r_idx, &lore);

	/* Describe the abilities of the monster */
	describe_monster_abilities(r_idx, &lore);
	
	/* Describe the known attacks */
	describe_monster_attack(r_idx, &lore);
	
	/* Describe monster "toughness" */
	describe_monster_toughness(r_idx, &lore);

	/* Describe the known skills */
	describe_monster_skills(r_idx, &lore);

	/* Describe experience */
	if (!spoilers) describe_monster_exp(r_idx, &lore);
	
	/* Describe the monster drop */
	describe_monster_drop(r_idx, &lore);
	
	/* All done */
	text_out("\n");

	/* Cheat -- know everything */
	if ((cheat_know) || p_ptr->active_ability[S_PER][PER_LORE2])
	{
		/* Hack -- restore memory */
		COPY(l_ptr, &save_mem, monster_lore);
	}
}





/*
 * Hack -- Display the "name" and "attr/chars" of a monster race
 */
void roff_top(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	byte a1, a2;
	char c1, c2;

	/* Get the chars */
	c1 = r_ptr->d_char;
	c2 = r_ptr->x_char;

	/* Get the attrs */
	a1 = r_ptr->d_attr;
	a2 = r_ptr->x_attr;

	/* Clear the top line */
	Term_erase(0, 0, 255);

	/* Reset the cursor */
	Term_gotoxy(0, 0);

	/* A title (use "The" for non-uniques) */
	if (!(r_ptr->flags1 & RF1_UNIQUE))
	{
		Term_addstr(-1, TERM_WHITE, "The ");
	}

	/* Dump the name */
	Term_addstr(-1, TERM_WHITE, (r_name + r_ptr->name));

	/* Append the "standard" attr/char info */
	Term_addstr(-1, TERM_WHITE, " - ");
	Term_addch(a1, c1);
	Term_addstr(-1, TERM_SLATE, "");

	// /* Append the "optional" attr/char info */
	//Term_addstr(-1, TERM_WHITE, "/('");
	//Term_addch(a2, c2);
	//if (use_bigtile && (a2 & 0x80)) Term_addch(255, -1);
	//Term_addstr(-1, TERM_WHITE, "'):");
}



/*
 * Hack -- describe the given monster race at the top of the screen
 */
void screen_roff(int r_idx)
{
	/* Flush messages */
	message_flush();

	/* Begin recall */
	Term_erase(0, 1, 255);

	/* Output to the screen */
	text_out_hook = text_out_to_screen;

	/* Recall monster */
	describe_monster(r_idx, FALSE);

	/* Describe monster */
	roff_top(r_idx);
}




/*
 * Hack -- describe the given monster race in the current "term" window
 */
void display_roff(int r_idx)
{
	int y;

	/* Erase the window */
	for (y = 0; y < Term->hgt; y++)
	{
		/* Erase the line */
		Term_erase(0, y, 255);
	}

	/* Begin recall */
	Term_gotoxy(0, 1);

	/* Output to the screen */
	text_out_hook = text_out_to_screen;

	/* Recall monster */
	describe_monster(r_idx, FALSE);

	/* Describe monster */
	roff_top(r_idx);
}



