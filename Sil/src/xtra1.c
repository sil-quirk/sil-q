/* File: xtra1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/*
 * Determines the total melee damage dice (before criticals and slays)
 */
 
byte total_mdd(const object_type *o_ptr)
{
	byte dd;
	
	/* if no weapon is wielded, use 1d1 */
	if (o_ptr->tval == 0)  
	{
		dd = 1;
	}
	/* otherwise use the weapon dice */
	else
	{
		dd = o_ptr->dd;
	}
	/* add the modifiers */
	dd += p_ptr->to_mdd;
	
	return (dd);
}

/*
 * Determines the strength modified damage sides for a melee or thrown weapon
 * Includes factors for strength and weight, but not bonuses from ring of damage etc
 */
byte strength_modified_ds(const object_type *o_ptr, int str_adjustment)
{
	byte mds;
	int int_mds; /* to allow negative values in the intermediate stages */
	int str_to_mds;
	int divisor;
	
	str_to_mds = p_ptr->stat_use[A_STR] + str_adjustment;
		
	/* if no weapon, use 1d1 and don't limit strength bonus */
	if (o_ptr->tval == 0)  
	{
		int_mds = 1;
		int_mds += str_to_mds;
	}
	/* if a weapon is being assessed, use its dice and limit bonus */
	else 
	{
		int_mds = o_ptr->ds;
		
		if (two_handed_melee())
		{
			divisor = 10;

			/* Bonus for 'hand and a half' weapons like the bastard sword when used with two hands */
			int_mds += hand_and_a_half_bonus(o_ptr);
		}
		else
		{
			divisor = 10;
		}
		
		// apply the Momentum ability
		if (p_ptr->active_ability[S_MEL][MEL_MOMENTUM]) divisor /= 2;
		
		/* limit the strength sides bonus by weapon weight */
		if ((str_to_mds > 0) && (str_to_mds > (o_ptr->weight / divisor)))
		{
			int_mds += o_ptr->weight / divisor;
		}
		else if ((str_to_mds < 0) && (str_to_mds < -(o_ptr->weight / divisor)))
		{
			int_mds += -(o_ptr->weight / divisor);
		}
		else
		{
			int_mds += str_to_mds;
		}
	}

	// add generic damage bonus
	int_mds += p_ptr->to_mds;

	// bonus for users of 'mighty blows' ability
	if (p_ptr->active_ability[S_MEL][MEL_POWER])
	{
		int_mds += 1;
	}
		
	/* make sure the total is non-negative */
	mds = (int_mds < 0) ? 0 : int_mds;

	return (mds);
}

/*
 * Determines the total melee damage sides (from strength and to_mds)
 * Does include strength and weight modifiers
 *
 * This function seems rather unnecessary these days...
 */
extern byte total_mds(const object_type *o_ptr, int str_adjustment)
{
	byte mds;
	int int_mds; /* to allow negative values in the inetermediate stages */
	
	int_mds = strength_modified_ds(o_ptr, str_adjustment);
	
	/* make sure the total is non-negative */
	mds = (int_mds < 0) ? 0 : int_mds;

	return (mds);
}

/*
 * Two handed melee weapon (including bastard sword used two handed)
 */
extern bool two_handed_melee(void)
{
	object_type *o_ptr = &inventory[INVEN_WIELD];
	
	if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED)) || hand_and_a_half_bonus(o_ptr))
	{
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Bonus for 'hand and a half' weapons like the bastard sword when wielded with two hands
 */
extern int hand_and_a_half_bonus(const object_type *o_ptr)
{
	if ((k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF)) &&
		(&inventory[INVEN_WIELD] == o_ptr) &&
	    (!inventory[INVEN_ARM].k_idx))
	{
		return (2);
	}
	return (0);
}

/*
 * Bonus for certain races/houses (elves) using blades
 */
int blade_bonus(const object_type *o_ptr)
{
	int bonus = 0;
	
	if ((rp_ptr->flags & RHF_BLADE_PROFICIENCY) && (o_ptr->tval == TV_SWORD))
	{
		bonus += 1;
	}
	if ((hp_ptr->flags & RHF_BLADE_PROFICIENCY) && (o_ptr->tval == TV_SWORD))
	{
		bonus += 1;
	}

	return bonus;
}

/*
 * Bonus for certain races/houses (dwarves) using axes
 */
int axe_bonus(const object_type *o_ptr)
{
	int bonus = 0;

	u32b f1, f2, f3;
	
	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);
	
	if ((rp_ptr->flags & RHF_AXE_PROFICIENCY) && (f3 & (TR3_AXE)))
	{
	    bonus += 1;
	}
	if ((hp_ptr->flags & RHF_AXE_PROFICIENCY) && (f3 & (TR3_AXE)))
	{
	    bonus += 1;
	}

	return bonus;
}


/*
 * Bonus for people with polearm affinity
 */
int polearm_bonus(const object_type *o_ptr)
{
	int bonus = 0;
	
	u32b f1, f2, f3;
	
	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);
	
	if (p_ptr->active_ability[S_MEL][MEL_POLEARMS] && (f3 & (TR3_POLEARM)))
	{
	    bonus += 1;
	}
	
	return bonus;
}

/*
 * Determines the total damage side for archery
 * based on the weight of the bow, strength, and the sides of the bow
 */
 
extern byte total_ads(const object_type *j_ptr, bool single_shot)
{
	byte ads;
	int int_ads; /* to allow negative values in the intermediate stages */
	int str_to_ads;
	
	str_to_ads = p_ptr->stat_use[A_STR];

	if (p_ptr->active_ability[S_ARC][ARC_RAPID_FIRE] && !single_shot) str_to_ads -= 3;
	
	int_ads = j_ptr->ds;
	
	/* limit the strength sides bonus by bow weight */
	if ((str_to_ads > 0) && (str_to_ads > (j_ptr->weight / 10))) 
	{
		int_ads += j_ptr->weight / 10;
	}
	else if ((str_to_ads < 0) && (str_to_ads < -(j_ptr->weight / 10)))
	{
		int_ads += -(j_ptr->weight / 10);
	}
	else
	{
		int_ads += str_to_ads;
	}
	
	// add archery damage bonus
	int_ads += p_ptr->to_ads;

	/* make sure the total is non-negative */
	ads = (int_ads < 0) ? 0 : int_ads;

	return (ads);
}


/*
 * Converts stat num into a two-char (right justified) string
 * Sil: rather pointless since stats no longer have and 18/XYZ format
 */
void cnv_stat(int val, char *out_val)
{
	sprintf(out_val, "%2d", val);
}



/*
 * Print character info at given row, column in a 13 char field
 */
static void prt_field(cptr info, int row, int col)
{
	/* Dump 13 spaces to clear */
	c_put_str(TERM_WHITE, "             ", row, col);

	/* Dump the info itself */
	c_put_str(TERM_L_BLUE, info, row, col);
}




/*
 * Print character stat in given row, column
 */
static void prt_stat(int stat)
{
	char tmp[32];

	/* Display "injured" stat */
	if (p_ptr->stat_drain[stat] < 0)
	{
		put_str(stat_names_reduced[stat], ROW_STAT + stat, 0);
		cnv_stat(p_ptr->stat_use[stat], tmp);
		c_put_str(TERM_YELLOW, tmp, ROW_STAT + stat, COL_STAT + 10);
	}

	/* Display "healthy" stat */
	else
	{
		put_str(stat_names[stat], ROW_STAT + stat, 0);
		cnv_stat(p_ptr->stat_use[stat], tmp);
		c_put_str(TERM_L_GREEN, tmp, ROW_STAT + stat, COL_STAT + 10);
	}

	/* Indicate temporary modifiers */
	if ((stat == A_STR) && p_ptr->tmp_str) put_str("*", ROW_STAT + stat, 3);
	if ((stat == A_DEX) && p_ptr->tmp_dex) put_str("*", ROW_STAT + stat, 3);
	if ((stat == A_CON) && p_ptr->tmp_con) put_str("*", ROW_STAT + stat, 3);
	if ((stat == A_GRA) && p_ptr->tmp_gra) put_str("*", ROW_STAT + stat, 3);
}



/*
 * Display the experience
 */
static void prt_exp(void)
{
	char out_val[32];
	byte attr;

	attr = TERM_L_GREEN;

	/*Print experience label*/
	put_str("Exp ", ROW_EXP, 0);
	
	comma_number(out_val, p_ptr->new_exp);

	c_put_str(attr, format("%8s", out_val), ROW_EXP, COL_EXP + 4);

}



/*
 * Prints current mel
 */
static void prt_mel(void)
{
	char buf[32];
	int mod = 0;

	if (((&inventory[INVEN_ARM])->k_idx) && ((&inventory[INVEN_ARM])->tval != TV_SHIELD)) mod = -1;

	/* Melee attacks */
	strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
	Term_putstr(COL_MEL, ROW_MEL + mod, -1, TERM_L_WHITE, format("%12s", buf));
	
	if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
	{
		Term_putstr(COL_MEL, ROW_MEL + mod, -1, TERM_WHITE, "2x");
	}
	
	if (mod == -1)
	{
		strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod, 
		                                         p_ptr->mdd2, p_ptr->mds2);
		Term_putstr(COL_MEL, ROW_MEL, -1, TERM_L_WHITE, format("%12s", buf));
	}
	else
	{
		strnfmt(buf, sizeof(buf), "            ");
		Term_putstr(COL_MEL, ROW_MEL - 1, -1, TERM_L_BLUE, format("%12s", buf));
	}
}

/*
 * Prints current arc
 */
static void prt_arc(void)
{
	char buf[32];

	/* Range attacks */
	if ((&inventory[INVEN_BOW])->k_idx)
	{
		strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
		Term_putstr(COL_ARC, ROW_ARC, -1, TERM_UMBER, format("%12s", buf));
		
		if (p_ptr->active_ability[S_ARC][ARC_RAPID_FIRE])
		{
			Term_putstr(COL_ARC, ROW_ARC, -1, TERM_L_UMBER, "2x");
		}
	}
	else
	{
		strnfmt(buf, sizeof(buf), "            ");
		Term_putstr(COL_ARC, ROW_ARC, -1, TERM_L_BLUE, format("%12s", buf));
	}
}


/*
 * Prints current evn
 */
static void prt_evn(void)
{
	char buf[32];

	/* Total Armor */
	strnfmt(buf, sizeof(buf), "[%+d,%d-%d]", p_ptr->skill_use[S_EVN], p_min(GF_HURT, TRUE), p_max(GF_HURT, TRUE));
	Term_putstr(COL_EVN, ROW_EVN, -1, TERM_SLATE, format("%12s", buf));
	
}


/*
 * Prints Cur/Max hit points
 */
static void prt_hp(void)
{
	char tmp[32];
	int len;
	byte color;

	if (p_ptr->mhp >= 100)  put_str("Hth        ", ROW_HP, COL_HP);
	else                    put_str("Health      ", ROW_HP, COL_HP);
    
	len = sprintf(tmp, "%d:%d", p_ptr->chp, p_ptr->mhp);

	c_put_str(TERM_L_GREEN, tmp, ROW_HP, COL_HP + 12 - len);

	/* Done? */
	if (p_ptr->chp >= p_ptr->mhp) return;

	if (p_ptr->chp > (p_ptr->mhp * op_ptr->hitpoint_warn) / 10)
	{
		color = TERM_YELLOW;
	}
	else
	{
		color = TERM_RED;
	}

	/* Show current hitpoints using another color */
	sprintf(tmp, "%d", p_ptr->chp);

	c_put_str(color, tmp, ROW_HP, COL_HP + 12 - len);
}


/*
 * Prints player's max/cur spell points
 */
static void prt_sp(void)
{
	char tmp[32];
	byte color;
	int len;

	if (p_ptr->msp >= 100)  put_str("Vce         ", ROW_SP, COL_SP);
	else                    put_str("Voice       ", ROW_SP, COL_SP);

	len = sprintf(tmp, "%d:%d", p_ptr->csp, p_ptr->msp);

	c_put_str(TERM_L_GREEN, tmp, ROW_SP, COL_SP + 12 - len);

	/* Done? */
	if (p_ptr->csp >= p_ptr->msp) return;

	if (p_ptr->csp > (p_ptr->msp * op_ptr->hitpoint_warn) / 10)
	{
		color = TERM_YELLOW;
	}
	else
	{
		color = TERM_RED;
	}


	/* Show current mana using another color */
	sprintf(tmp, "%d", p_ptr->csp);

	c_put_str(color, tmp, ROW_SP, COL_SP + 12 - len);
}


/*
 * Prints player's current song (if any)
 */
static void prt_song(void)
{
	char *song1_name = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
	char *song2_name = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;
	char buf[80];
	int slaying_bonus = slaying_song_bonus();
	
	// wipe old songs
	put_str("             ", ROW_SONG, COL_SONG);
	put_str("             ", ROW_SONG + 1, COL_SONG);
	
	// show the first song
	if (p_ptr->song1 != SNG_NOTHING)
	{
		c_put_str(TERM_L_BLUE, song1_name + 8, ROW_SONG, COL_SONG);
	}

	// show the second song
	if (p_ptr->song2 != SNG_NOTHING)
	{
		c_put_str(TERM_BLUE, song2_name + 8, ROW_SONG + 1, COL_SONG);
	}
	
	// show the slaying score
	if (slaying_bonus > 0)
	{
		sprintf(buf, "+%d", slaying_bonus);
		
		if (p_ptr->song1 == SNG_SLAYING)
		{
			c_put_str(TERM_L_BLUE, buf, ROW_SONG, COL_SONG + 8);
		}
		else if (p_ptr->song2 == SNG_SLAYING)
		{
			c_put_str(TERM_BLUE, buf, ROW_SONG + 1, COL_SONG + 8);
		}
	}
}


/*
 * Prints depth in stat area
 */
static void prt_depth(void)
{
	char depths[32];
	s16b attr = TERM_WHITE;

	if (!p_ptr->depth)
	{
		my_strcpy(depths, "Surface", sizeof (depths));
	}
	else
	{
		sprintf(depths, "%d ft", p_ptr->depth * 50);
	}

	/* Get color of level based on feeling  -JSV- */
	if ((p_ptr->depth) && (do_feeling))
	{
		if (feeling ==  1) attr = TERM_VIOLET;
		else if (feeling ==  2) attr = TERM_RED;
		else if (feeling ==  3) attr = TERM_L_RED;
		else if (feeling ==  4) attr = TERM_ORANGE;
		else if (feeling ==  5) attr = TERM_ORANGE;
		else if (feeling ==  6) attr = TERM_YELLOW;
		else if (feeling ==  7) attr = TERM_YELLOW;
		else if (feeling ==  8) attr = TERM_WHITE;
		else if (feeling ==  9) attr = TERM_WHITE;
		else if (feeling == 10) attr = TERM_L_WHITE;
		else if (feeling >= LEV_THEME_HEAD) attr = TERM_BLUE;
	}

	/* Right-Adjust the "depth", and clear old values */
	c_prt(attr, format("%7s", depths), ROW_DEPTH, COL_DEPTH);
}


/*
 * Prints status of hunger
 */
static void prt_hunger(void)
{
	/* Fainting / Starving */
	if (p_ptr->food < PY_FOOD_STARVE)
	{
		c_put_str(TERM_RED, "Starving", ROW_HUNGRY, COL_HUNGRY);
	}

	/* Weak */
	else if (p_ptr->food < PY_FOOD_WEAK)
	{
		c_put_str(TERM_ORANGE, "Weak    ", ROW_HUNGRY, COL_HUNGRY);
	}

	/* Hungry */
	else if (p_ptr->food < PY_FOOD_ALERT)
	{
		c_put_str(TERM_YELLOW, "Hungry  ", ROW_HUNGRY, COL_HUNGRY);
	}

	/* Normal */
	else if (p_ptr->food < PY_FOOD_FULL)
	{
		c_put_str(TERM_L_GREEN, "        ", ROW_HUNGRY, COL_HUNGRY);
	}

	/* Full */
	else if (p_ptr->food < PY_FOOD_MAX)
	{
		c_put_str(TERM_L_GREEN, "Full    ", ROW_HUNGRY, COL_HUNGRY);
	}

	/* Gorged */
	else
	{
		c_put_str(TERM_GREEN, "Gorged  ", ROW_HUNGRY, COL_HUNGRY);
	}
}


/*
 * Prints Blind status
 */
static void prt_blind(void)
{
	if (p_ptr->blind)
	{
		c_put_str(TERM_ORANGE, "Blind", ROW_BLIND, COL_BLIND);
	}
	else
	{
		put_str("     ", ROW_BLIND, COL_BLIND);
	}
}


/*
 * Prints Confusion status
 */
static void prt_confused(void)
{
	if (p_ptr->confused)
	{
		c_put_str(TERM_ORANGE, "Confused", ROW_CONFUSED, COL_CONFUSED);
	}
	else
	{
		put_str("        ", ROW_CONFUSED, COL_CONFUSED);
	}
}


/*
 * Prints Fear status
 */
static void prt_afraid(void)
{
	if (p_ptr->afraid)
	{
		c_put_str(TERM_ORANGE, "Afraid", ROW_AFRAID, COL_AFRAID);
	}
	else
	{
		put_str("      ", ROW_AFRAID, COL_AFRAID);
	}
}

/*
 *  Displays the amount of bleeding.
 *  This is a bit tricky as it is in the same row as poison, *unless* you have both.
 *  In which case it is the row above.
 */

static void prt_cut(void)
{
	int c = p_ptr->cut;
	char buf[20];
	
	int r = ROW_CUT;
	
	if (p_ptr->poisoned) r--;
	
	put_str("            ", ROW_CUT - 1, COL_CUT);

	if (c > 100)
	{
		c_put_str(TERM_RED, "Mortal wound", r, COL_CUT);
	}
	else if (c > 20)
	{
		sprintf(buf, "Bleeding %-2d", c);
		c_put_str(TERM_RED, buf, r, COL_CUT);
	}
	else if (c > 0)
	{
		sprintf(buf, "Bleeding %-2d", c);
		c_put_str(TERM_L_RED, buf, r, COL_CUT);
	}
	else
	{
		put_str("            ", r, COL_CUT);
	}
}



/*
 * Prints Poisoned status
 */
static void prt_poisoned(void)
{
	int p = p_ptr->poisoned;
	char buf[20];

	if (p > 20)
	{
		sprintf(buf, "Poisoned %-3d", p);
		c_put_str(TERM_L_GREEN, buf, ROW_POISONED, COL_POISONED);
	}
	else if (p > 0)
	{
		sprintf(buf, "Poisoned %-3d", p);
		c_put_str(TERM_GREEN, buf, ROW_POISONED, COL_POISONED);
	}
	else
	{
		put_str("            ", ROW_POISONED, COL_POISONED);
	}
}


/*
 * Prints Searching, Resting, Entrancement, Smithing, or 'count' status
 * Display is always exactly 10 characters wide (see below)
 *
 * This function was a major bottleneck when resting, so a lot of
 * the text formatting code was optimized in place below.
 */
static void prt_state(void)
{
	byte attr = TERM_WHITE;

	char text[16];

	/* Entrancement */
	if (p_ptr->entranced)
	{
		attr = TERM_RED;

		my_strcpy(text, "Entranced!", sizeof (text));
	}

	/* Smithing */
	if (p_ptr->smithing)
	{		
		my_strcpy(text, "Smithing  ", sizeof (text));
	}
	
	/* Resting */
	else if (p_ptr->resting)
	{
		int i;
		int n = p_ptr->resting;

		/* Start with "Rest" */
		my_strcpy(text, "Rest      ", sizeof (text));

		/* Extensive (timed) rest */
		if (n >= 1000)
		{
			i = n / 100;
			text[9] = '0';
			text[8] = '0';
			text[7] = I2D(i % 10);
			if (i >= 10)
			{
				i = i / 10;
				text[6] = I2D(i % 10);
				if (i >= 10)
				{
					text[5] = I2D(i / 10);
				}
			}
		}

		/* Long (timed) rest */
		else if (n >= 100)
		{
			i = n;
			text[9] = I2D(i % 10);
			i = i / 10;
			text[8] = I2D(i % 10);
			text[7] = I2D(i / 10);
		}

		/* Medium (timed) rest */
		else if (n >= 10)
		{
			i = n;
			text[9] = I2D(i % 10);
			text[8] = I2D(i / 10);
		}

		/* Short (timed) rest */
		else if (n > 0)
		{
			i = n;
			text[9] = I2D(i);
		}

		/* Rest until healed */
		else if (n == -1)
		{
			text[5] = text[6] = text[7] = text[8] = text[9] = '*';
		}

		/* Rest until done */
		else if (n == -2)
		{
			text[5] = text[6] = text[7] = text[8] = text[9] = '&';
		}
	}

	/* Repeating */
	else if (p_ptr->command_rep)
	{
		if (p_ptr->command_rep > 999)
		{
			sprintf(text, "Rep. %3d00", p_ptr->command_rep / 100);
		}
		else
		{
			sprintf(text, "Repeat %3d", p_ptr->command_rep);
		}
	}

	/* Stealth mode */
	else if (p_ptr->stealth_mode)
	{
		my_strcpy(text, "Stealth   ", sizeof (text));
	}

	/* Climbing */
	//else if (p_ptr->climbing)
	//{
	//	my_strcpy(text, "Climbing  ", sizeof (text));
	//}
    
	/* Nothing interesting */
	else
	{
		my_strcpy(text, "          ", sizeof (text));
	}

	/* Display the info (or blanks) */
	c_put_str(attr, text, ROW_STATE, COL_STATE);
}


/*
 * Prints the speed of a character.			-CJS-
 */
static void prt_speed(void)
{
	int i = p_ptr->pspeed;

	byte attr = TERM_WHITE;
	char buf[32] = "";

	/* Fast */
	if (i > 2)
	{
		attr = TERM_L_GREEN;
		sprintf(buf, "Fast");
	}

	/* Slow */
	else if (i < 2)
	{
		attr = TERM_ORANGE;
		sprintf(buf, "Slow");
	}

	/* Display the speed */
	c_put_str(attr, format("%-4s", buf), ROW_SPEED, COL_SPEED);
}


/*
 * Prints message regarding difficult terrain
 */
static void prt_terrain(void)
{
	if (cave_pit_bold(p_ptr->py, p_ptr->px))
	{
		c_put_str(TERM_ORANGE, "Pit", ROW_TERRAIN, COL_TERRAIN);
	}
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
		c_put_str(TERM_ORANGE, "Web", ROW_TERRAIN, COL_TERRAIN);
    }
	else
	{
		put_str("   ", ROW_TERRAIN, COL_TERRAIN);
	}
}




static void prt_stun(void)
{
	int s = p_ptr->stun;

	if (s > 100)
	{
		c_put_str(TERM_RED, "Knocked out ", ROW_STUN, COL_STUN);
	}
	else if (s > 50)
	{
		c_put_str(TERM_ORANGE, "Heavy stun  ", ROW_STUN, COL_STUN);
	}
	else if (s)
	{
		c_put_str(TERM_ORANGE, "Stun        ", ROW_STUN, COL_STUN);
	}
	else
	{
		put_str("            ", ROW_STUN, COL_STUN);
	}
}

/*
 *  Represents the different levels of health.
 *  Note that it is a bit odd with fewer health levels in the SOMEWHAT_WOUNDED category.
 *  This is due to a rounding off tension between the natural way to do the colours (perfect having its own)
 *  and the natural way to do the stars for the health bar (zero having its own).
 *  It should be unnoticeable to the player.
 */
int health_level(int current, int max)
{
	int level;
	
	if (current == max)
	{ 
		level = HEALTH_UNHURT;										// 100%
	}
	
	else
	{
		switch ((4 * current + max - 1 ) / max)
		{
			case  4:	level = HEALTH_SOMEWHAT_WOUNDED	;	break;  //  76% - 99%
			case  3:	level = HEALTH_WOUNDED			;	break;  //  51% - 75%
			case  2:	level = HEALTH_BADLY_WOUNDED	;	break;  //  26% - 50%
			case  1:	level = HEALTH_ALMOST_DEAD		;	break;  //   1% - 25%
			default:	level = HEALTH_DEAD				;	break;  //   0%
		}
	}
	
	return (level);
}

/*
 *  Assigns colours to the health levels.
 */
byte health_attr(int current, int max)
{
	byte a;
	
	switch (health_level(current,max))
	{
		case  HEALTH_UNHURT:			a = TERM_WHITE	;	break;  // 100%
		case  HEALTH_SOMEWHAT_WOUNDED:	a = TERM_YELLOW	;	break;  //  76% - 99%
		case  HEALTH_WOUNDED:			a = TERM_ORANGE	;	break;  //  51% - 75%
		case  HEALTH_BADLY_WOUNDED:		a = TERM_L_RED	;	break;  //  26% - 50%
		case  HEALTH_ALMOST_DEAD:		a = TERM_RED	;	break;  //   1% - 25%
		default:						a = TERM_RED	;	break;  //   0%
	}
	
	return (a);
}

/*
 * Redraw the "monster health bar"
 *
 * The "monster health bar" provides visual feedback on the "health"
 * of the monster currently being "tracked".  There are several ways
 * to "track" a monster, including targetting it, attacking it, and
 * affecting it (and nobody else) with a ranged attack.  When nothing
 * is being tracked, we clear the health bar.  If the monster being
 * tracked is not currently visible, a special health bar is shown.
 */
static void health_redraw(void)
{

	/* Not tracking */
	if (!p_ptr->health_who)
	{
		/* Erase the health bar */
		Term_erase(COL_INFO, ROW_INFO, 12);
		/* Erase the morale bar */
		Term_erase(COL_INFO, ROW_INFO+1, 12);
	}

	/* Tracking an unseen monster */
	else if (!mon_list[p_ptr->health_who].ml)
	{
		/* Erase the health bar */
		Term_erase(COL_INFO, ROW_INFO, 12);
		/* Erase the morale bar */
		Term_erase(COL_INFO, ROW_INFO+1, 12);
	}

	/* Tracking a hallucinatory monster */
	else if (p_ptr->image)
	{
		/* Erase the health bar */
		Term_erase(COL_INFO, ROW_INFO, 12);
		/* Erase the morale bar */
		Term_erase(COL_INFO, ROW_INFO+1, 12);
	}

	/* Tracking a dead monster (?) */
	else if (mon_list[p_ptr->health_who].hp <= 0)
	{
		/* Erase the health bar */
		Term_erase(COL_INFO, ROW_INFO, 12);
		/* Erase the morale bar */
		Term_erase(COL_INFO, ROW_INFO+1, 12);
	}

	/* Tracking a visible monster */
	else
	{
		int len;
		char tmp[20];
		char buf[20];

		monster_type *m_ptr = &mon_list[p_ptr->health_who];
		monster_race *r_ptr = &r_info[m_ptr->r_idx];

		/* Default to almost dead */
		byte attr = health_attr(m_ptr->hp, m_ptr->maxhp);

		/* Afraid */
		//if (m_ptr->stance == STANCE_FLEEING) attr = TERM_VIOLET;

		/* Convert into health bar (using ceiling for length) */
		len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;

		/* Default to "unknown" */
		Term_putstr(COL_INFO, ROW_INFO, 12, TERM_L_DARK, "  --------  ");

		/* Dump the current "health" (handle monster stunning, confusion) */
		
		if (m_ptr->confused && m_ptr->stunned)
			Term_putstr(COL_INFO+2, ROW_INFO, len, attr, "cscscscs");
		else if (m_ptr->confused)
			Term_putstr(COL_INFO+2, ROW_INFO, len, attr, "cccccccc");
		else if (m_ptr->stunned)
			Term_putstr(COL_INFO+2, ROW_INFO, len, attr, "ssssssss");
		else
			Term_putstr(COL_INFO+2, ROW_INFO, len, attr, "********");

		Term_erase(COL_INFO, ROW_INFO+1, 12);

		if (m_ptr->alertness < ALERTNESS_UNWARY)
		{
			my_strcpy(buf, "Sleeping", sizeof(tmp));
			attr = TERM_BLUE;
		}
		else if (m_ptr->alertness < ALERTNESS_ALERT)
		{
			my_strcpy(buf, "Unwary", sizeof(tmp));
			attr = TERM_L_BLUE;
		}
		else
		{
			if (r_ptr->flags2 & (RF2_MINDLESS))
			{
				my_strcpy(buf, "Mindless", sizeof(tmp));
				attr = TERM_L_DARK;
			}
			else
			{
				if (m_ptr->stance == STANCE_FLEEING)
				{
					my_strcpy(tmp, "Fleeing", sizeof(tmp));
					attr = TERM_VIOLET;
				}
				else if (m_ptr->stance == STANCE_CONFIDENT)
				{
					my_strcpy(tmp, "Confident", sizeof(tmp));
					attr = TERM_L_WHITE;
				}
				else if (m_ptr->stance == STANCE_AGGRESSIVE)
				{
					my_strcpy(tmp, "Aggress", sizeof(tmp));
					attr = TERM_L_WHITE;
				}
                
                // sometimes (only in debugging?) we are looking at a monster before it has a stance
                // in this case just exit and don't do anything (to avoid printing uninitialised strings!)
                else
                {
					return;
                }
				
				if (m_ptr->morale >= 0)	sprintf(buf, "%s %d", tmp, (m_ptr->morale + 9) / 10);
				else					sprintf(buf, "%s %d", tmp, m_ptr->morale / 10);
			}

		}
		
		Term_putstr(COL_INFO+(13-strlen(buf))/2, ROW_INFO+1, MIN(strlen(buf),12), attr, buf);

	}
	
	
}


/*
 * Display basic info (mostly left of map)
 */
static void prt_frame_basic(void)
{
	int i;

	/* Name */
	if (strlen(op_ptr->full_name) <= 12)
	{
		prt_field(op_ptr->full_name, ROW_NAME, COL_NAME);
	}

	/* Level/Experience */
	prt_exp();

	/* All Stats */
	for (i = 0; i < A_MAX; i++) prt_stat(i);

	/* Hitpoints */
	prt_hp();

	/* Spellpoints */
	prt_sp();

	/* Melee */
	prt_mel();

	/* Archery */
	prt_arc();

	/* Evasion */
	prt_evn();

	/* Song */
	prt_song();

	/* Current depth */
	prt_depth();

	/* redraw monster health */
	health_redraw();
}


/*
 * Display extra info (mostly below map)
 */
static void prt_frame_extra(void)
{
	/* Stun */
	prt_stun();

	/* Food */
	prt_hunger();

	/* Various */
	prt_blind();
	prt_confused();
	prt_afraid();
	prt_poisoned();
	prt_cut();
    prt_terrain();
    
	/* State */
	prt_state();

	/* Speed */
	prt_speed();
}


/*
 * Hack -- display inventory in sub-windows
 */
static void fix_inven(void)
{
	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		term *old = Term;

		/* No window */
		if (!angband_term[j]) continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_INVEN))) continue;

		/* Activate */
		Term_activate(angband_term[j]);

		/* Display inventory */
		display_inven();

		/* Fresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}
}

/*
 * Hack -- display monsters in sub-windows
 */
static void fix_monlist(void)
{
	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		term *old = Term;

		/* No window */
		if (!angband_term[j]) continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_MONLIST))) continue;

		/* Activate */
		Term_activate(angband_term[j]);

		/* Display visible monsters */
		display_monlist();

		/* Fresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}
}


/*
 * Hack -- display combat rolls in sub-windows
 */
static void fix_combat_rolls(void)
{
	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		term *old = Term;

		/* No window */
		if (!angband_term[j]) continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_COMBAT_ROLLS))) continue;

		/* Activate */
		Term_activate(angband_term[j]);

		/* Display visible monsters */
		display_combat_rolls();

		/* Fresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}
}


/*
 * Hack -- display equipment in sub-windows
 */
static void fix_equip(void)
{
	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		term *old = Term;

		/* No window */
		if (!angband_term[j]) continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_EQUIP))) continue;

		/* Activate */
		Term_activate(angband_term[j]);

		/* Display equipment */
		display_equip();

		/* Fresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}
}


/*
 * Hack -- display player in sub-windows (mode 0)
 */
static void fix_player_0(void)
{
	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		term *old = Term;

		/* No window */
		if (!angband_term[j]) continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_PLAYER_0))) continue;

		/* Activate */
		Term_activate(angband_term[j]);

		/* Display player */
		display_player(0);

		/* Fresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}
}



/*
 * Hack -- display recent messages in sub-windows
 *
 * Adjust for width and split messages.  XXX XXX XXX
 */
static void fix_message(void)
{
	int j, i;
	int w, h;
	int x, y;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		term *old = Term;

		/* No window */
		if (!angband_term[j]) continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_MESSAGE))) continue;

		/* Activate */
		Term_activate(angband_term[j]);

		/* Get size */
		Term_get_size(&w, &h);

		/* Dump messages */
		for (i = 0; i < h; i++)
		{
			byte color = message_color((s16b)i);

			/* Dump the message on the appropriate line */
			Term_putstr(0, (h - 1) - i, -1, color, message_str((s16b)i));

			/* Cursor */
			Term_locate(&x, &y);

			/* Clear to end of line */
			Term_erase(x, y, 255);
		}

		/* Fresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}
}




/*
 * Hack -- display monster recall in sub-windows
 */
static void fix_monster(void)
{
	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		term *old = Term;

		/* No window */
		if (!angband_term[j]) continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_MONSTER))) continue;

		/* Activate */
		Term_activate(angband_term[j]);

		/* Display monster race info */
		if (p_ptr->monster_race_idx) display_roff(p_ptr->monster_race_idx);

		/* Fresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}
}


/*
 * Calculate maximum voice. 
 *
 * This function induces status messages.
 */
extern void calc_voice(void)
{
	int msp;
	int i;
	int tmp;
	
	/* Get voice value */
	// 20 + a compounding 20% bonus per point of gra
	
	tmp = 20 * 100;
		
	if (p_ptr->stat_use[A_GRA] >= 0)
	{
		for (i = 0; i < p_ptr->stat_use[A_GRA]; i++)
		{
			tmp = tmp * 12 / 10;
		}
	}
	else
	{
		for (i = 0; i < -(p_ptr->stat_use[A_GRA]); i++)
		{
			tmp = tmp * 10 / 12;
		}
	}
	msp = tmp / 100;
	
	/* New maximum hitpoints */
	if (p_ptr->msp != msp)
	{
		int i = 100;
		
		/* Get percentage of maximum sp */
		if (p_ptr->msp) i = ((100 * p_ptr->csp) / p_ptr->msp);
		
		/* Save new limit */
		p_ptr->msp = msp;
		
		/* Update current maximum sp */
		p_ptr->csp = ((i * p_ptr->msp) / 100) + (((i * p_ptr->msp) % 100 >= 50)	? 1 : 0);
		
		/* Hack - any change in max voice resets frac */
		p_ptr->csp_frac = 0;
		
		/* Display sp later */
		p_ptr->redraw |= (PR_VOICE);
		
		/* Window stuff */
		p_ptr->window |= (PW_PLAYER_0);
	}

	/* Hack -- handle "xtra" mode */
	if (character_xtra) return;

}



/*
 * Calculate the player's (maximal) hit points
 *
 * Adjust current hitpoints if necessary
 *
 * Sil - modified substantially to reflect absence of chance and fixed bonus, not per level
 */
static void calc_hitpoints(void)
{
	int mhp;
	int i;
	int tmp;

	/* Get hitpoint value */
	// 20 + a compounding 20% bonus per point of con
	
	tmp = 20 * 100;
	if (p_ptr->stat_use[A_CON] >= 0)
	{
		for (i = 0; i < p_ptr->stat_use[A_CON]; i++)
		{
			tmp = tmp * 12 / 10;
		}
	}
	else
	{
		for (i = 0; i < -(p_ptr->stat_use[A_CON]); i++)
		{
			tmp = tmp * 10 / 12;
		}
	}
	mhp = tmp / 100;
	
	/* New maximum hitpoints */
	if (p_ptr->mhp != mhp)
	{
		int i = 100;

		/* Get percentage of maximum hp */
		if (p_ptr->mhp) i = ((100 * p_ptr->chp) / p_ptr->mhp);

		/* Save new limit */
		p_ptr->mhp = mhp;

		/* Update current maximum hp */
		p_ptr->chp = ((i * p_ptr->mhp) / 100) + (((i * p_ptr->mhp) % 100 >= 50)	? 1 : 0);

		/* Hack - any change in max hitpoint resets frac */
		p_ptr->chp_frac = 0;

		/* Display hp later */
		p_ptr->redraw |= (PR_HP);

		/* Window stuff */
		p_ptr->window |= (PW_PLAYER_0);
	}
}


/*
 * Determine the radius of possibly flickering lights
 */
int light_up_to(int base_radius, object_type *o_ptr)
{ 
	int radius = base_radius;
	u32b f1, f2, f3;
	
	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);
	
	// Some lights flicker
	if (f2 & (TR2_DARKNESS))
	{
		while ((radius > -2) && one_in_(3))
		{
			radius--;
		}
	}
	else if (o_ptr->timeout < 100)
	{
		while ((radius > 0) && one_in_(3))
		{
			radius--;
		}
	}
	
	return (radius);
}

/*
 *  Determines how much an enemy in a given location should make the sword glow
 */
int hate_level(int y, int x, int multiplier)
{
	int dist;
	
	// check distance of monster from player (by noise)
	dist = flow_dist(FLOW_MONSTER_NOISE, y, x);
	
	// Avoid a division by zero
	if (dist == 0) dist = 1;
	
	// determine the danger level
	return ((50 * multiplier) / dist);
}

/*
 * Determine whether a melee weapon is glowing in response to nearby enemies
 */
bool weapon_glows(object_type *o_ptr)
{
	int total_hate = 0;
	int i;
	int iy = o_ptr->iy; // weapon location
	int ix = o_ptr->ix;
	int py = p_ptr->py; // player location
	int px = p_ptr->px;
	int y, x;			// generic location
	u32b f1, f2, f3;
	bool viewable = FALSE;
	
	bool glows = FALSE;
	
	if (!character_dungeon) return (FALSE);

	// Must be a melee weapon
	if (wield_slot(o_ptr) != INVEN_WIELD)	return (FALSE);

	// use the player's position where needed
	if ((iy == 0) && (ix == 0))
	{
		iy = py;
		ix = px;
	}
	
	// out of LOS objects don't glow (or it can't be seen)
	if (cave_info[iy-1][ix-1] & (CAVE_VIEW))	viewable = TRUE;
	if (cave_info[iy-1][ix] & (CAVE_VIEW))		viewable = TRUE;
	if (cave_info[iy-1][ix+1] & (CAVE_VIEW))	viewable = TRUE;
	if (cave_info[iy][ix-1] & (CAVE_VIEW))		viewable = TRUE;
	if (cave_info[iy][ix] & (CAVE_VIEW))		viewable = TRUE;
	if (cave_info[iy][ix+1] & (CAVE_VIEW))		viewable = TRUE;
	if (cave_info[iy+1][ix-1] & (CAVE_VIEW))	viewable = TRUE;
	if (cave_info[iy+1][ix] & (CAVE_VIEW))		viewable = TRUE;
	if (cave_info[iy+1][ix+1] & (CAVE_VIEW))	viewable = TRUE;
	
	if (!viewable) return (FALSE);
	
	// create a 'flow' around the object
	update_flow(iy, ix, FLOW_MONSTER_NOISE);
		
	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);
		
	/* Add up the total of creatures vulnerable to the weapon's slays */
	for (i = 1; i < mon_max; i++)
	{
		bool target = FALSE;
		int multiplier = 1;
		monster_type *m_ptr = &mon_list[i];
		monster_race *r_ptr = &r_info[m_ptr->r_idx];
		
		/* Paranoia -- Skip dead monsters */
		if (!m_ptr->r_idx) continue;
		
		// Determine if a slay is applicable
		if ((f1 & (TR1_SLAY_WOLF)) && (r_ptr->flags3 & (RF3_WOLF)))			target = TRUE;
		if ((f1 & (TR1_SLAY_SPIDER)) && (r_ptr->flags3 & (RF3_SPIDER)))		target = TRUE;
		if ((f1 & (TR1_SLAY_UNDEAD)) && (r_ptr->flags3 & (RF3_UNDEAD)))		target = TRUE;
		if ((f1 & (TR1_SLAY_RAUKO)) && (r_ptr->flags3 & (RF3_RAUKO)))		target = TRUE;
		if ((f1 & (TR1_SLAY_ORC)) && (r_ptr->flags3 & (RF3_ORC)))			target = TRUE;
		if ((f1 & (TR1_SLAY_TROLL)) && (r_ptr->flags3 & (RF3_TROLL)))		target = TRUE;
		if ((f1 & (TR1_SLAY_DRAGON)) && (r_ptr->flags3 & (RF3_DRAGON)))		target = TRUE;
		
		// skip inapplicable monsters
		if (!target) continue;
		
		// increase the effect for uniques
		if (r_ptr->flags1 & (RF1_UNIQUE)) multiplier *= 2;
		
		// increase the effect for individually occuring creatures
		if (!(r_ptr->flags1 & (RF1_FRIENDS)) && !(r_ptr->flags1 & (RF1_FRIEND)) && 
		    !(r_ptr->flags1 & (RF1_ESCORTS)) && !(r_ptr->flags1 & (RF1_ESCORT)))	multiplier *= 2;

		// add up the 'hate'
		total_hate += hate_level(m_ptr->fy, m_ptr->fx, multiplier);
	}
	
	/* Add a similar effect for very nearby webs for spider slaying wearpons */
	if (f1 & (TR1_SLAY_SPIDER))
	{
		for (y = (iy - 2); y <= (iy + 2); y++)
		{
			for (x = (ix - 2); x <= (ix + 2); x++)
			{
				if (in_bounds(y, x))
				{
					// skip inapplicable squares
					if (cave_feat[y][x] != FEAT_TRAP_WEB)	continue;
					
					// add up the 'hate'
					total_hate += hate_level(y, x, 1);
				}
			}
		}	
	}
	
	if (total_hate >= 15) glows = TRUE;
		
	return (glows);
}


/*
 * Extract and set the current "lite radius"
 */
void calc_torch(void)
{
	int i;
	object_type *o_ptr;
	u32b f1, f2, f3;
	int old_light;

	/* Store old value */
	old_light = p_ptr->cur_light;

	/* Assume no light */
	p_ptr->cur_light = 0;
	
	/* Loop through all wielded items */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
	
		/* Skip empty slots */
		if (!o_ptr->k_idx) continue;

		/* Extract the flags */
		object_flags(o_ptr, &f1, &f2, &f3);

		/* Does this item glow? */
		if ((f2 & TR2_LIGHT) && (i != INVEN_LITE)) p_ptr->cur_light++;

		/* Does this item create darkness? */
		if ((f2 & TR2_DARKNESS) && (i != INVEN_LITE)) p_ptr->cur_light--;

		/* Examine actual light */
		if (o_ptr->tval == TV_LIGHT)
		{
			bool extinguished = FALSE;
			
			/* Some items provide permanent, bright, light */
			if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)		p_ptr->cur_light += RADIUS_LESSER_JEWEL;
			else if (o_ptr->sval == SV_LIGHT_FEANORIAN)		p_ptr->cur_light += RADIUS_FEANORIAN;
			else if (o_ptr->sval == SV_LIGHT_SILMARIL)		p_ptr->cur_light += RADIUS_SILMARIL;

			/* Torches (with fuel) provide some light */
			else if ((o_ptr->sval == SV_LIGHT_TORCH) && (o_ptr->timeout > 0))
			{
				p_ptr->cur_light += light_up_to(RADIUS_TORCH, o_ptr);
			}

			/* Lanterns (with fuel) provide more light */
			else if ((o_ptr->sval == SV_LIGHT_LANTERN) && (o_ptr->timeout > 0))
			{
				p_ptr->cur_light += light_up_to(RADIUS_LANTERN, o_ptr);
			}
			
			else
			{
				extinguished = TRUE;
			}
			
			if (!extinguished && (f2 & TR2_LIGHT))
			{
				p_ptr->cur_light++;
			}			
		}
		
	}

	// increase radius when the player's weapon glows
	if (weapon_glows(&inventory[INVEN_WIELD])) p_ptr->cur_light++;
	if (weapon_glows(&inventory[INVEN_ARM])) p_ptr->cur_light++;

	/* Player is darkened */
	if (p_ptr->darkened && (p_ptr->cur_light > 0)) p_ptr->cur_light--;

	// Smithing brightens the room a bit
	if (p_ptr->smithing) p_ptr->cur_light += 2;

	// Song of the trees
	if (singing(SNG_TREES))
	{
		p_ptr->cur_light += ability_bonus(S_SNG, SNG_TREES);
	}
		
	/* Update the visuals */
	p_ptr->update |= (PU_UPDATE_VIEW);
	p_ptr->update |= (PU_MONSTERS);

	/* Notice changes in the "lite radius" */
	if (old_light != p_ptr->cur_light)
	{
		/* Update the visuals */
		p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	}
}

int affinity_level(int skilltype)
{
	int level = 0;
	u32b affinity_flag = 0L, penalty_flag = 0L; // default values to soothe compilation warnings
	
	switch (skilltype)
	{
		case S_MEL:
		{
			affinity_flag = RHF_MEL_AFFINITY;
			penalty_flag = RHF_MEL_PENALTY;
			break;
		}
		case S_ARC:
		{
			affinity_flag = RHF_ARC_AFFINITY;
			penalty_flag = RHF_ARC_PENALTY;
			break;
		}
		case S_EVN:
		{
			affinity_flag = RHF_EVN_AFFINITY;
			penalty_flag = RHF_EVN_PENALTY;
			break;
		}
		case S_STL:
		{
			affinity_flag = RHF_STL_AFFINITY;
			penalty_flag = RHF_STL_PENALTY;
			break;
		}
		case S_PER:
		{
			affinity_flag = RHF_PER_AFFINITY;
			penalty_flag = RHF_PER_PENALTY;
			break;
		}
		case S_WIL:
		{
			affinity_flag = RHF_WIL_AFFINITY;
			penalty_flag = RHF_WIL_PENALTY;
			break;
		}
		case S_SMT:
		{
			affinity_flag = RHF_SMT_AFFINITY;
			penalty_flag = RHF_SMT_PENALTY;
			break;
		}
		case S_SNG:
		{
			affinity_flag = RHF_SNG_AFFINITY;
			penalty_flag = RHF_SNG_PENALTY;
			break;
		}
	}
	
	if (rp_ptr->flags & affinity_flag)	level += 1;
	if (hp_ptr->flags & affinity_flag)	level += 1;
	if (rp_ptr->flags & penalty_flag)	level -= 1;
	if (hp_ptr->flags & penalty_flag)	level -= 1;
	
	return (level);
}

int ability_bonus(int skilltype, int abilitynum)
{
	int bonus = 0;
	int skill = p_ptr->skill_use[skilltype];
	
	if (skilltype == S_SNG)
	{
		// penalize minor themes
		if (p_ptr->song1 != abilitynum) skill /= 2;
		
		switch (abilitynum)
		{
			case SNG_ELBERETH:
			{
				bonus = skill;
				break;
			}
			case SNG_SLAYING:
			{
				bonus = skill;
				break;
			}
			case SNG_SILENCE:
			{
				bonus = skill / 2;
				break;
			}
			case SNG_FREEDOM:
			{
				bonus = skill;
				break;
			}
			case SNG_TREES:
			{
				bonus = skill / 5;
				break;
			}
			case SNG_AULE:
			{
				bonus = skill / 4;
				break;
			}
			case SNG_STAYING:
			{
				bonus = skill / 3;
				break;
			}
			case SNG_LORIEN:
			{
				bonus = skill;
				break;
			}
			case SNG_ESTE:
			{
				bonus = skill / 4;
				if (bonus < 2) bonus = 2;
				break;
			}
			case SNG_SHARPNESS:
			{
				bonus = skill * 2;
				break;
			}
			case SNG_MASTERY:
			{
				bonus = skill;
				break;
			}
		}
		
		// these bonuses are never negative
		if (bonus < 0) bonus = 0;
	}
	
	return (bonus);
}


/*
 * Computes current weight limit in tenths of pounds.
 *
 * 100 + a compounding 20% bonus per point of str
 */
int weight_limit(void)
{
	int i;
	int limit;
	
	limit = 1000;
	if (p_ptr->stat_use[A_STR] >= 0)
	{
		for (i = 0; i < p_ptr->stat_use[A_STR]; i++)
		{
			limit = limit * 12 / 10;
		}
	}
	else
	{
		for (i = 0; i < -(p_ptr->stat_use[A_STR]); i++)
		{
			limit = limit * 10 / 12;
		}
	}

	/* Return the result */
	return (limit);
}


bool sprinting(void)
{
	int i;
	int turns = 1;
	
	if (p_ptr->active_ability[S_EVN][EVN_SPRINTING])
	{
		for (i = 1; i < 4; i++)
		{
			if ((p_ptr->previous_action[i] >= 1) && (p_ptr->previous_action[i] <= 9) && (p_ptr->previous_action[i] != 5))
			{ 
				if ((p_ptr->previous_action[i+1] >= 1) && (p_ptr->previous_action[i+1] <= 9) && (p_ptr->previous_action[i+1] != 5))
				{ 
					if (p_ptr->previous_action[i] == p_ptr->previous_action[i+1])
					{
						turns++;
					}
					else if (p_ptr->previous_action[i] == cycle[chome[p_ptr->previous_action[i+1]]-1])
					{
						turns++;
					}
					else if (p_ptr->previous_action[i] == cycle[chome[p_ptr->previous_action[i+1]]+1])
					{
						turns++;
					}
				}
			}
		}
	}
	
	return (turns >= 4);
}

/*
 * Calculate the player's current "state", taking into account
 * not only race/house intrinsics, but also objects being worn
 * and temporary spell effects.
 *
 * See also calc_voice() and calc_hitpoints().
 *
 * The "weapon" and "bow" do *not* add to the bonuses to hit or to
 * damage, since that would affect non-combat things.  These values
 * are actually added in later, at the appropriate place.
 *
 * This function induces various "status" messages.
 */
static void calc_bonuses(void)
{
	int i, j;

	int old_speed;

	int old_telepathy;
	int old_see_inv;
	
	int old_mdd = p_ptr->mdd;
	int old_mds = p_ptr->mds;

	int old_mdd2 = p_ptr->mdd2;
	int old_mds2 = p_ptr->mds2;

	int old_add = p_ptr->add;
	int old_ads = p_ptr->ads;
	
	int new_p_min = p_min(GF_HURT, TRUE);
	int new_p_max = p_max(GF_HURT, TRUE);
		
	int old_stat_use[A_MAX];
	int old_stat_tmp_mod[A_MAX];

	int old_skill_use[S_MAX];

	object_type *o_ptr;

	u32b f1, f2, f3;

	int armour_weight = 0;

	// Remove off-hand weapons if you cannot wield them
	if (!p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON])
	{
		o_ptr = &inventory[INVEN_ARM];
		
		if ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM) || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING))
		{
			char o_name[80];
			
			/* Full object description */
			object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);
			
			/* Print the messages */
			msg_print("You can no longer wield both weapons.");
			
			// take it off
			do_cmd_takeoff(o_ptr, INVEN_ARM);
		}
	}
	
	/*** Memorize ***/

	/* Save the old speed */
	old_speed = p_ptr->pspeed;

	/* Save the old vision stuff */
	old_telepathy = p_ptr->telepathy;
	old_see_inv = p_ptr->see_inv;

	/* Save the old stats */
	for (i = 0; i < A_MAX; i++)
	{
		old_stat_use[i] = p_ptr->stat_use[i];
		old_stat_tmp_mod[i] = p_ptr->stat_drain[i];
	}

	/* Save the old skills */
	for (i = 0; i < S_MAX; i++)
	{
		old_skill_use[i] = p_ptr->skill_use[i];
	}

	/*** Reset ***/

	/* Reset player speed */
	p_ptr->pspeed = 2;

	/* Reset "fire" info */
	p_ptr->add = 0;
	p_ptr->ads = 0;
	p_ptr->ammo_tval = 0;

	/* Clear the equipment stat modifiers */
	for (i = 0; i < A_MAX; i++) p_ptr->stat_equip_mod[i] = 0;

	/* Clear the misc stat modifiers */
	for (i = 0; i < A_MAX; i++) p_ptr->stat_misc_mod[i] = 0;

	/* Clear the total values of the skills */
	for (i = 0; i < S_MAX; i++) p_ptr->skill_use[i] = 0;

	/* Clear the stat modifiers of the skills */
	for (i = 0; i < S_MAX; i++) p_ptr->skill_stat_mod[i] = 0;

	/* Clear the equipment modifiers of the skills */
	for (i = 0; i < S_MAX; i++) p_ptr->skill_equip_mod[i] = 0;

	/* Clear the misc modifiers of the skills */
	for (i = 0; i < S_MAX; i++) p_ptr->skill_misc_mod[i] = 0;

	/* Clear other bonuses */
	p_ptr->to_mdd = 0;
	p_ptr->to_mds = 0;
	p_ptr->mdd = 0;
	p_ptr->mds = 0;
	p_ptr->mdd2 = 0;
	p_ptr->mds2 = 0;
	p_ptr->offhand_mel_mod = 0;
	p_ptr->to_ads = 0;

	/* Clear all the flags */
	p_ptr->hunger = 0;
	p_ptr->danger = 0;
	p_ptr->aggravate = 0;
	p_ptr->cowardice = 0;
	p_ptr->haunted = 0;
	p_ptr->see_inv = 0;
	p_ptr->free_act = 0;
	p_ptr->regenerate = 0;
	p_ptr->telepathy = 0;
	p_ptr->sustain_str = 0;
	p_ptr->sustain_con = 0;
	p_ptr->sustain_dex = 0;
	p_ptr->sustain_gra = 0;
	p_ptr->resist_fire = 1;
	p_ptr->resist_cold = 1;
	p_ptr->resist_pois = 1;
	p_ptr->resist_fear = 0;
	p_ptr->resist_blind = 0;
	p_ptr->resist_confu = 0;
	p_ptr->resist_stun = 0;
	p_ptr->resist_hallu = 0;
    
    p_ptr->climbing = FALSE;

	/* Clear the item granted abilities */
	for (i = 0; i < S_MAX; i++)
	{
		for (j = 0; j < ABILITIES_MAX; j++)
		{
			p_ptr->have_ability[i][j] = p_ptr->innate_ability[i][j];
		}
	}
	
	/*** Extract race/house info ***/

	// Recalculate total weight
	p_ptr->total_weight = 0;
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
		p_ptr->total_weight += o_ptr->number * o_ptr->weight;
		
		// *all* carried objects still cause danger
		object_flags(o_ptr, &f1, &f2, &f3);
		if (f2 & (TR2_DANGER)) p_ptr->danger += 1;
	}

	/*** Analyze equipment ***/

	/* Scan the equipment */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];

		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;

		/* Extract the item flags */
		object_flags(o_ptr, &f1, &f2, &f3);

		/* Affect stats */
		if (f1 & (TR1_STR)) p_ptr->stat_equip_mod[A_STR] += o_ptr->pval;
		if (f1 & (TR1_DEX)) p_ptr->stat_equip_mod[A_DEX] += o_ptr->pval;
		if (f1 & (TR1_CON)) p_ptr->stat_equip_mod[A_CON] += o_ptr->pval;
		if (f1 & (TR1_GRA)) p_ptr->stat_equip_mod[A_GRA] += o_ptr->pval;
		if (f1 & (TR1_NEG_STR)) p_ptr->stat_equip_mod[A_STR] -= o_ptr->pval;
		if (f1 & (TR1_NEG_DEX)) p_ptr->stat_equip_mod[A_DEX] -= o_ptr->pval;
		if (f1 & (TR1_NEG_CON)) p_ptr->stat_equip_mod[A_CON] -= o_ptr->pval;
		if (f1 & (TR1_NEG_GRA)) p_ptr->stat_equip_mod[A_GRA] -= o_ptr->pval;

		/* Affect skills */
		if (f1 & (TR1_MEL)) p_ptr->skill_equip_mod[S_MEL] += o_ptr->pval;
		if (f1 & (TR1_ARC)) p_ptr->skill_equip_mod[S_ARC] += o_ptr->pval;
		if (f1 & (TR1_STL)) p_ptr->skill_equip_mod[S_STL] += o_ptr->pval;
		if (f1 & (TR1_PER)) p_ptr->skill_equip_mod[S_PER] += o_ptr->pval;
		if (f1 & (TR1_WIL)) p_ptr->skill_equip_mod[S_WIL] += o_ptr->pval;
		if (f1 & (TR1_SMT)) p_ptr->skill_equip_mod[S_SMT] += o_ptr->pval;
		if (f1 & (TR1_SNG)) p_ptr->skill_equip_mod[S_SNG] += o_ptr->pval;

		/* Affect Damage Sides */
		if (f1 & (TR1_DAMAGE_SIDES))
		{
			p_ptr->to_mds += o_ptr->pval;
			p_ptr->to_ads += o_ptr->pval;
		}
		
		/* Good flags */
		if (f2 & (TR2_SLOW_DIGEST)) p_ptr->hunger -= 1;
		if (f2 & (TR2_REGEN)) p_ptr->regenerate += 1;
				
		if (f2 & (TR2_SEE_INVIS)) 
		{
			(void) set_blind(0);
			p_ptr->see_inv += 1;
		}
		if (f2 & (TR2_FREE_ACT)) p_ptr->free_act += 1;
		if (f2 & (TR2_SPEED))
		{ 
			p_ptr->pspeed += 1;
		}
		
		/* Bad flags */
		if (f2 & (TR2_HUNGER)) p_ptr->hunger += 1;
		if (f2 & (TR2_SLOWNESS)) p_ptr->pspeed -= 1;
		if (f2 & (TR2_AGGRAVATE)) p_ptr->aggravate += 1;
		if (f2 & (TR2_FEAR)) p_ptr->cowardice += 1;
		if (f2 & (TR2_HAUNTED)) p_ptr->haunted += 1;

		// danger has already been handled in the general inventory
		//if (f2 & (TR2_DANGER)) p_ptr->danger += 1;
		
		// darkness and light are handled later...

		/* Resistance flags */
		if (f2 & (TR2_RES_COLD)) p_ptr->resist_cold += 1;
		if (f2 & (TR2_RES_FIRE)) p_ptr->resist_fire += 1;
		if (f2 & (TR2_RES_POIS)) p_ptr->resist_pois += 1;

		if (f2 & (TR2_VUL_COLD)) p_ptr->resist_cold -= 1;
		if (f2 & (TR2_VUL_FIRE)) p_ptr->resist_fire -= 1;
		if (f2 & (TR2_VUL_POIS)) p_ptr->resist_pois -= 1;
		
		if (f2 & (TR2_RES_FEAR))  p_ptr->resist_fear   += 1;
		if (f2 & (TR2_RES_BLIND)) p_ptr->resist_blind  += 1;
		if (f2 & (TR2_RES_CONFU)) p_ptr->resist_confu  += 1;
		if (f2 & (TR2_RES_STUN))  p_ptr->resist_stun   += 1;
		if (f2 & (TR2_RES_HALLU))  p_ptr->resist_hallu += 1;

		/* Sustain flags */
		if (f2 & (TR2_SUST_STR)) p_ptr->sustain_str += 1;
		if (f2 & (TR2_SUST_DEX)) p_ptr->sustain_dex += 1;
		if (f2 & (TR2_SUST_CON)) p_ptr->sustain_con += 1;
		if (f2 & (TR2_SUST_GRA)) p_ptr->sustain_gra += 1;

		/* Apply the bonus to evasion */
		p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;
		
		// Parrying grants extra bonus for weapon evasion:
		if (p_ptr->active_ability[S_EVN][EVN_PARRY] && (i == INVEN_WIELD))
		{
			p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;
		}
		
		/* Add up the armour weight */
		if ((i >= INVEN_BODY) && (i <= INVEN_FEET)) armour_weight += o_ptr->weight;

		// add the abilities
		for (j = 0; j < o_ptr->abilities; j++)
		{
			p_ptr->have_ability[o_ptr->skilltype[j]][o_ptr->abilitynum[j]] = TRUE;
		}

		/* Hack -- do not apply "melee" to-hit bonuses yet */
		if (i == INVEN_WIELD) continue;

		/* Hack -- do not apply "melee" to-hit bonuses yet */
		if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD)) continue;
		
		/* Hack -- do not apply "bow" to-hit bonuses yet */
		if (i == INVEN_BOW) continue;

		/* Hack -- do not apply "arrow" to-hit bonuses at all */
		if (i == INVEN_QUIVER1) continue;
		if (i == INVEN_QUIVER2) continue;

		/* Apply the bonus to hit */
		p_ptr->skill_equip_mod[S_MEL] += o_ptr->att;
		p_ptr->skill_equip_mod[S_ARC] += o_ptr->att;		
	}

	/* Clear the old item granted abilities */
	for (i = 0; i < S_MAX; i++)
	{
		for (j = 0; j < ABILITIES_MAX; j++)
		{
			if (!p_ptr->have_ability[i][j])
			{
				p_ptr->active_ability[i][j] = FALSE;
			}
		}
	}
	

	/*** Most abilities ***/

	if (p_ptr->active_ability[S_MEL][MEL_STR]) p_ptr->stat_misc_mod[A_STR]++;
	if (p_ptr->active_ability[S_ARC][ARC_DEX]) p_ptr->stat_misc_mod[A_DEX]++;
	if (p_ptr->active_ability[S_EVN][EVN_DEX]) p_ptr->stat_misc_mod[A_DEX]++;
	if (p_ptr->active_ability[S_STL][STL_DEX]) p_ptr->stat_misc_mod[A_DEX]++;
	if (p_ptr->active_ability[S_PER][PER_GRA]) p_ptr->stat_misc_mod[A_GRA]++;
	if (p_ptr->active_ability[S_WIL][WIL_CON]) p_ptr->stat_misc_mod[A_CON]++;
	if (p_ptr->active_ability[S_SMT][SMT_GRA]) p_ptr->stat_misc_mod[A_GRA]++;
	if (p_ptr->active_ability[S_SNG][SNG_GRA]) p_ptr->stat_misc_mod[A_GRA]++;

	if (p_ptr->active_ability[S_WIL][WIL_STRENGTH_IN_ADVERSITY])
	{
		// if <= 50% health, give a bonus to strength and grace
		if (health_level(p_ptr->chp, p_ptr->mhp) <= HEALTH_BADLY_WOUNDED)
		{
			p_ptr->stat_misc_mod[A_STR]++;
			p_ptr->stat_misc_mod[A_GRA]++;
		}

		// if <= 25% health, give an extra bonus
		if (health_level(p_ptr->chp, p_ptr->mhp) <= HEALTH_ALMOST_DEAD)
		{
			p_ptr->stat_misc_mod[A_STR]++;
			p_ptr->stat_misc_mod[A_GRA]++;
		}
	}

	if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
	{
		p_ptr->skill_misc_mod[S_MEL] -= 3;
	}
	
	if (p_ptr->active_ability[S_ARC][ARC_RAPID_FIRE])
	{
		p_ptr->skill_misc_mod[S_ARC] -= 3;
	}
	
	if (p_ptr->active_ability[S_WIL][WIL_POISON_RESISTANCE])
	{
		p_ptr->resist_pois += 1;
	}
	
	/*** Temporary flags ***/

	/* Apply temporary "stun" */
	if (p_ptr->stun >= 50)
	{
		for (i = 0; i < S_MAX; i++)
		{
			p_ptr->skill_misc_mod[i] -= 4;
		}
	}
	else if (p_ptr->stun)
	{
		for (i = 0; i < S_MAX; i++)
		{
			p_ptr->skill_misc_mod[i] -= 2;
		}
	}


	/* Temporary "Rage" */
	if (p_ptr->rage)
	{
		p_ptr->stat_misc_mod[A_STR] += 1;
		p_ptr->stat_misc_mod[A_DEX] -= 1;
		p_ptr->stat_misc_mod[A_CON] += 1;
		p_ptr->stat_misc_mod[A_GRA] -= 1;
	}

	/* Temporary Strength */
	if (p_ptr->tmp_str)
	{
		p_ptr->stat_misc_mod[A_STR] += 3;
        p_ptr->sustain_str += 1;
	}

	/* Temporary Dexterity */
	if (p_ptr->tmp_dex)
	{
		p_ptr->stat_misc_mod[A_DEX] += 3;
        p_ptr->sustain_dex += 1;
	}

	/* Temporary Constitution */
	if (p_ptr->tmp_con)
	{
		p_ptr->stat_misc_mod[A_CON] += 3;
        p_ptr->sustain_con += 1;
	}

	/* Temporary Grace */
	if (p_ptr->tmp_gra)
	{
		p_ptr->stat_misc_mod[A_GRA] += 3;
        p_ptr->sustain_gra += 1;
	}

	/* Temporary "fast" */
	if (p_ptr->fast)
	{
		p_ptr->pspeed += 1;
	}

	/* Temporary "slow" */
	if (p_ptr->slow)
	{
		p_ptr->pspeed -= 1;
	}

	/* Temporary see invisible, resist blindness, and resist hallucination */
	if (p_ptr->tim_invis)
	{
		/* Hack */
		p_ptr->see_inv += 1;

		/* Hack */
		p_ptr->resist_blind += 1;

		/* Hack */
		p_ptr->resist_hallu += 1;
	}

	/* Weak with hunger */
	if (p_ptr->food < PY_FOOD_WEAK)
	{
		p_ptr->stat_misc_mod[A_STR] -= 1;
	}
	
	// decrease food consumption with 'mind over body' ability
	if (p_ptr->active_ability[S_WIL][WIL_MIND_OVER_BODY])
	{
		p_ptr->hunger -= 1;
	}

	// provide resist_confusion, resist_stunning and resist_hallucinaton with 'clarity' ability
	if (p_ptr->active_ability[S_WIL][WIL_CLARITY])
	{
		p_ptr->resist_confu += 1;
		p_ptr->resist_stun += 1;
		p_ptr->resist_hallu += 1;
	}

	/*** Handle stats ***/

	/* Calculate stats */
	for (i = 0; i < A_MAX; i++)
	{
		/* Extract the new "stat_use" value for the stat */
		p_ptr->stat_use[i] = p_ptr->stat_base[i] + p_ptr->stat_equip_mod[i]
		                     + p_ptr->stat_drain[i] + p_ptr->stat_misc_mod[i];

		/* cap to -9 and 20 */
		if (p_ptr->stat_use[i] < BASE_STAT_MIN)
			p_ptr->stat_use[i] = BASE_STAT_MIN;
		else if (p_ptr->stat_use[i] > BASE_STAT_MAX)
			p_ptr->stat_use[i] = BASE_STAT_MAX;
	}

	/*** Analyze weight ***/

	/* Extract the current weight (in tenth pounds) */
	j = p_ptr->total_weight;

	/* Extract the "weight limit" (in tenth pounds) */
	i = weight_limit();

	/* Apply "encumbrance" from weight */
	if (j > i) p_ptr->pspeed -= 1;

    // climbing slows the player down (and is incompatible with stealth)
    if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM) && !p_ptr->leaping)
    {
        p_ptr->climbing = TRUE;
        p_ptr->pspeed -= 1;
    }
    
	/* Stealth slows the player down (unless they are passing) */
	if (p_ptr->stealth_mode)
	{
		if (p_ptr->previous_action[0] != 5) p_ptr->pspeed -= 1;
 	    p_ptr->skill_misc_mod[S_STL] += STEALTH_MODE_BONUS;
	}

    // sprinting speed the player up
	if (sprinting() && !p_ptr->climbing)
	{
		p_ptr->pspeed += 1;
	}

	/* Speed must lie between 1 and 3 */
	if (p_ptr->pspeed < 1) p_ptr->pspeed = 1;
	if (p_ptr->pspeed > 3) p_ptr->pspeed = 3;
	
	// Increase food consumption if regenerating
	if (p_ptr->regenerate) p_ptr->hunger += 1;

	/* armour weight (not inventory weight reduces stealth */
	/* by 1 point per 10 pounds (rounding down) */
	p_ptr->skill_equip_mod[S_STL] -= armour_weight / 100;

	// Penalise stealth based on song(s) being sung
	if (p_ptr->song1 != SNG_NOTHING)
	{
		int song_noise = 0;
		int song;
		
		for (i = 0; i < 2; i++)
		{
			if (i == 0) song = p_ptr->song1;
			else		song = p_ptr->song2;
			
			switch (song)
			{
				case SNG_NOTHING:	song_noise += 0; break;
				case SNG_ELBERETH:	song_noise += 8; break;
				case SNG_SLAYING:	song_noise += 12; break;
				case SNG_SILENCE:	song_noise += 0; break;
				case SNG_FREEDOM:	song_noise += 4; break;
				case SNG_TREES:		song_noise += 4; break;
				case SNG_AULE:		song_noise += 8; break;
				case SNG_STAYING:	song_noise += 4; break;
				case SNG_LORIEN:	song_noise += 4; break;
				case SNG_ESTE:		song_noise += 4; break;
				case SNG_SHARPNESS:	song_noise += 8; break;
				case SNG_MASTERY:	song_noise += 8; break;
			}		
		}
		
		// average the noise if there are two songs
		if (p_ptr->song2 != SNG_NOTHING) song_noise /= 2;
		
		p_ptr->skill_misc_mod[S_STL] -= song_noise;
	}

	/* Race/House skill flags */
	p_ptr->skill_misc_mod[S_MEL] += affinity_level(S_MEL);
	p_ptr->skill_misc_mod[S_ARC] += affinity_level(S_ARC);
	p_ptr->skill_misc_mod[S_EVN] += affinity_level(S_EVN);
	p_ptr->skill_misc_mod[S_STL] += affinity_level(S_STL);
	p_ptr->skill_misc_mod[S_PER] += affinity_level(S_PER);
	p_ptr->skill_misc_mod[S_WIL] += affinity_level(S_WIL);
	p_ptr->skill_misc_mod[S_SMT] += affinity_level(S_SMT);
	p_ptr->skill_misc_mod[S_SNG] += affinity_level(S_SNG);
	

	/*** Modify skills by ability scores ***/

	/* Affect Skill -- melee (DEX) */
	p_ptr->skill_stat_mod[S_MEL] = p_ptr->stat_use[A_DEX];

	/* Affect Skill -- archery (DEX) */
	p_ptr->skill_stat_mod[S_ARC] = p_ptr->stat_use[A_DEX];
	
	/* Affect Skill -- evasion (DEX) */
	p_ptr->skill_stat_mod[S_EVN] = p_ptr->stat_use[A_DEX];

	/* Affect Skill -- stealth (DEX) */
	p_ptr->skill_stat_mod[S_STL] = p_ptr->stat_use[A_DEX];

	/* Affect Skill -- perception (GRA) */
	p_ptr->skill_stat_mod[S_PER] = p_ptr->stat_use[A_GRA];

	/* Affect Skill -- will (GRA) */
	p_ptr->skill_stat_mod[S_WIL] = p_ptr->stat_use[A_GRA];

	/* Affect Skill -- smithing (GRA) */
	p_ptr->skill_stat_mod[S_SMT] = p_ptr->stat_use[A_GRA];

	/* Affect Skill -- song (GRA) */
	p_ptr->skill_stat_mod[S_SNG] = p_ptr->stat_use[A_GRA];
	
	// Finalise song first as it modifies some other skills...
	p_ptr->skill_use[S_SNG] = p_ptr->skill_base[S_SNG] + p_ptr->skill_equip_mod[S_SNG] + 
							  p_ptr->skill_stat_mod[S_SNG] + p_ptr->skill_misc_mod[S_SNG];

	// Apply song effects that modify skills
	if (singing(SNG_SLAYING))
	{
		p_ptr->skill_misc_mod[S_MEL] += slaying_song_bonus();
		p_ptr->skill_misc_mod[S_ARC] += slaying_song_bonus();
	}
	if (singing(SNG_AULE))
	{
		p_ptr->skill_misc_mod[S_SMT] += ability_bonus(S_SNG, SNG_AULE);
	}
	if (singing(SNG_STAYING))
	{
		p_ptr->skill_misc_mod[S_WIL] += ability_bonus(S_SNG, SNG_STAYING);
	}
	if (singing(SNG_FREEDOM))
	{
		p_ptr->free_act += 1;
	}

	
	/*** Finalise all skills other than combat skills  (as bows/weapons must be analysed first) ***/
	
	p_ptr->skill_use[S_STL] = p_ptr->skill_base[S_STL] + p_ptr->skill_equip_mod[S_STL] + 
						      p_ptr->skill_stat_mod[S_STL] + p_ptr->skill_misc_mod[S_STL];
	p_ptr->skill_use[S_PER] = p_ptr->skill_base[S_PER] + p_ptr->skill_equip_mod[S_PER] + 
						      p_ptr->skill_stat_mod[S_PER] + p_ptr->skill_misc_mod[S_PER];
	p_ptr->skill_use[S_WIL] = p_ptr->skill_base[S_WIL] + p_ptr->skill_equip_mod[S_WIL] + 
						      p_ptr->skill_stat_mod[S_WIL] + p_ptr->skill_misc_mod[S_WIL];
	p_ptr->skill_use[S_SMT] = p_ptr->skill_base[S_SMT] + p_ptr->skill_equip_mod[S_SMT] + 
						      p_ptr->skill_stat_mod[S_SMT] + p_ptr->skill_misc_mod[S_SMT];

	/*** Analyze current bow ***/

	/* Examine the "current bow" */
	o_ptr = &inventory[INVEN_BOW];

	p_ptr->skill_equip_mod[S_ARC] += o_ptr->att;

	/* Analyze launcher */
	if (o_ptr->k_idx)
	{
		p_ptr->ammo_tval = TV_ARROW;

		p_ptr->add = o_ptr->dd;
		p_ptr->ads = total_ads(o_ptr, FALSE);
		
		/* set the archery skill (if using a bow) -- it gets set again later, anyway */
		p_ptr->skill_use[S_ARC] = p_ptr->skill_base[S_ARC] + p_ptr->skill_equip_mod[S_ARC] + 
								  p_ptr->skill_stat_mod[S_ARC] + p_ptr->skill_misc_mod[S_ARC];
	}

	/*** Analyze melee weapon ***/

	/* Examine the "current melee weapon" */
	o_ptr = &inventory[INVEN_WIELD];

	// add the weapon's attack mod
	p_ptr->skill_equip_mod[S_MEL] += o_ptr->att;
	
	// attack bonuses for matched weapon types
	p_ptr->skill_misc_mod[S_MEL] += blade_bonus(o_ptr) + axe_bonus(o_ptr) + polearm_bonus(o_ptr);

	// deal with the 'Versatility' ability
	if (p_ptr->active_ability[S_ARC][ARC_VERSATILITY] && (p_ptr->skill_base[S_ARC] > p_ptr->skill_base[S_MEL]))
	{
		p_ptr->skill_misc_mod[S_MEL] += (p_ptr->skill_base[S_ARC] - p_ptr->skill_base[S_MEL]) / 2;
	}
	
	/* generate the melee dice/sides from weapon, to_mdd, to_mds and strength */
	p_ptr->mdd = total_mdd(o_ptr);
	p_ptr->mds = total_mds(o_ptr, p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] ? -3 : 0);
	
    // determine the off-hand melee score, damage and sides
	if (p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON] && 
	    (((&inventory[INVEN_ARM])->tval != TV_SHIELD) && ((&inventory[INVEN_ARM])->tval != 0)))
	{
		// remove main-hand specific bonuses
		p_ptr->offhand_mel_mod -= o_ptr->att + blade_bonus(o_ptr) + axe_bonus(o_ptr) + polearm_bonus(o_ptr);
		if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK]) p_ptr->offhand_mel_mod += 3;
		
		// add off-hand specific bonuses
		o_ptr = &inventory[INVEN_ARM];
		p_ptr->offhand_mel_mod += o_ptr->att + blade_bonus(o_ptr) + axe_bonus(o_ptr) + polearm_bonus(o_ptr) - 3;

		p_ptr->mdd2 = total_mdd(o_ptr);
		p_ptr->mds2 = total_mds(o_ptr, -3);
	}

	/* Entrancement or being knocked out sets total evasion score to -5 */
	if (p_ptr->entranced || (p_ptr->stun > 100))
	{
		p_ptr->skill_misc_mod[S_EVN] = -5 - (p_ptr->skill_base[S_EVN] + p_ptr->skill_equip_mod[S_EVN] + 
										     p_ptr->skill_stat_mod[S_EVN]);
	}
	
	
	/* finalise the combat and evasion skills */
	
	p_ptr->skill_use[S_MEL] = p_ptr->skill_base[S_MEL] + p_ptr->skill_equip_mod[S_MEL] + 
							  p_ptr->skill_stat_mod[S_MEL] + p_ptr->skill_misc_mod[S_MEL];
	p_ptr->skill_use[S_ARC] = p_ptr->skill_base[S_ARC] + p_ptr->skill_equip_mod[S_ARC] + 
							  p_ptr->skill_stat_mod[S_ARC] + p_ptr->skill_misc_mod[S_ARC];
	p_ptr->skill_use[S_EVN] = p_ptr->skill_base[S_EVN] + p_ptr->skill_equip_mod[S_EVN] + 
							  p_ptr->skill_stat_mod[S_EVN] + p_ptr->skill_misc_mod[S_EVN];
	

	/* Blows (melee attacks per round) and digging power */
	if (o_ptr->k_idx)
	{
		/* Extract the item flags */
		object_flags(o_ptr, &f1, &f2, &f3);
	}


	/*** Notice changes ***/

	/* Analyze stats */
	for (i = 0; i < A_MAX; i++)
	{
		/* Notice changes */
		if (p_ptr->stat_drain[i] != old_stat_tmp_mod[i])
		{
			/* Redisplay the stats later */
			p_ptr->redraw |= (PR_STATS);

			/* Window stuff */
			p_ptr->window |= (PW_PLAYER_0);
		}

		/* Notice changes */
		if (p_ptr->stat_use[i] != old_stat_use[i])
		{
			/* Redisplay the stats later */
			p_ptr->redraw |= (PR_STATS);

			/* Window stuff */
			p_ptr->window |= (PW_PLAYER_0);
			/* Change in CON affects Hitpoints */
			if (i == A_CON)
			{
				p_ptr->update |= (PU_HP);
			}
		}
	}

	/* Recalculate voice needed */
	if (p_ptr->stat_use[A_GRA] != old_stat_use[A_GRA])
	{
		p_ptr->update |= (PU_MANA);
	}

	/* Hack -- Telepathy Change */
	if (p_ptr->telepathy != old_telepathy)
	{
		/* Update monster visibility */
		p_ptr->update |= (PU_MONSTERS);
	}

	/* Hack -- See Invis Change */
	if (p_ptr->see_inv != old_see_inv)
	{
		/* Update monster visibility */
		p_ptr->update |= (PU_MONSTERS);
	}

	/* Redraw speed (if needed) */
	if (p_ptr->pspeed != old_speed)
	{
		/* Redraw speed */
		p_ptr->redraw |= (PR_SPEED);
	}

    /* Always redraw terrain */
    p_ptr->redraw |= (PR_TERRAIN);
    
	/* Redraw melee (if needed) */
	if ((p_ptr->skill_use[S_MEL] != old_skill_use[S_MEL]) || (p_ptr->mdd != old_mdd) || (p_ptr->mds != old_mds) || 
															 (p_ptr->mdd2 != old_mdd2) || (p_ptr->mds2 != old_mds2))
	{
		/* Redraw */
		p_ptr->redraw |= (PR_MEL);

		/* Window stuff */
		p_ptr->window |= (PW_PLAYER_0);
	}

	/* Redraw archery (if needed) */
	if ((p_ptr->skill_use[S_ARC] != old_skill_use[S_ARC]) || (p_ptr->add != old_add) || (p_ptr->ads != old_ads))
	{
		/* Redraw */
		p_ptr->redraw |= (PR_ARC);

		/* Window stuff */
		p_ptr->window |= (PW_PLAYER_0);
	}

	/* Redraw armor */
	if ((p_ptr->skill_use[S_EVN] != old_skill_use[S_EVN]) || (p_ptr->old_p_min != new_p_min) || (p_ptr->old_p_max != new_p_max))
	{
		/* Redraw */
		p_ptr->redraw |= (PR_ARMOR);

		/* Window stuff */
		p_ptr->window |= (PW_PLAYER_0);
		
		p_ptr->old_p_min = new_p_min;
		p_ptr->old_p_max = new_p_max;
	}

	/* Hack -- handle "xtra" mode */
	if (character_xtra) return;

	if (p_ptr->active_ability[S_PER][PER_LORE1])
	{
		pseudo_id_everything();
	}

	// identify {special} items when the type has been seen before
	id_known_specials();
	reorder_pack(FALSE);
	
}

/*
 * Handle "p_ptr->notice"
 */
void notice_stuff(void)
{
	/* Notice stuff */
	if (!p_ptr->notice) return;


	/* Combine the pack */
	if (p_ptr->notice & (PN_COMBINE))
	{
		p_ptr->notice &= ~(PN_COMBINE);
		combine_pack();
	}

	/* Reorder the pack */
	if (p_ptr->notice & (PN_REORDER))
	{
		p_ptr->notice &= ~(PN_REORDER);
		reorder_pack(TRUE);
	}

	if(p_ptr->notice & PN_AUTOINSCRIBE)
	{
		p_ptr->notice &= ~(PN_AUTOINSCRIBE);
		autoinscribe_pack();
		autoinscribe_ground();
	}


}

/*
 * Helper function for update_lore()
 */
void update_lore_aux(object_type *o_ptr)
{
	// identify seen items with Lore-Master
	if (!object_known_p(o_ptr) && p_ptr->active_ability[S_PER][PER_LORE2] &&
        (o_ptr->tval != TV_CHEST))
	{
		ident(o_ptr);
	}
	
	// Mark new identified artefacts/specials and gain experience for them
	if (object_known_p(o_ptr) && !p_ptr->leaving)
	{
		if (o_ptr->name1)
		{
			artefact_type *a_ptr = &a_info[o_ptr->name1];
			char note[120];
			char shorter_desc[120];
			int new_exp;
			
			if (a_ptr->found_num == 0)
			{
				// mark
				a_ptr->found_num = 1;
				
				// gain experience for identification
				new_exp = 100;
				gain_exp(new_exp);
				p_ptr->ident_exp += new_exp;
				
				// display a note for new artefacts
				if ((o_ptr->name1 != ART_MORGOTH_2) && (o_ptr->name1 != ART_MORGOTH_1) && (o_ptr->name1 != ART_MORGOTH_0))
				{
					/* Get a shorter description to fit the notes file */
					object_desc(shorter_desc, sizeof(shorter_desc), o_ptr, TRUE, 0);
					
					/* Build note and write */
					if (o_ptr->xtra1 == p_ptr->depth)
					{
						sprintf(note, "Found %s", shorter_desc);
					}
					else
					{
						sprintf(note, "Found %s (from %d ft)", shorter_desc, o_ptr->xtra1 * 50);
					}
					
					/* Record the depth where the artefact was identified */
					do_cmd_note(note, p_ptr->depth);
				}
			}
		}
		
		else if (o_ptr->name2)
		{
			int new_exp;
			
			/* We now know about the special item type */
			e_info[o_ptr->name2].everseen = TRUE;
			
			if (!(e_info[o_ptr->name2].aware))
			{
				// mark
				e_info[o_ptr->name2].aware = TRUE;
				
				// gain experience for identification
				new_exp = 100;
				gain_exp(new_exp);
				p_ptr->ident_exp += new_exp;
			}
		}
	}
}

/*
 * This function does a few book keeping things for item identification.
 *
 * It identifies visible objects for the Lore-Keeper ability,
 * marks artefacts/specials as seen and grants experience for the first sighting.
 */
void update_lore(void)
{
	int i;
	object_type *o_ptr;
	
	// Scan all dungeon objects that are 'seen' (in LOS and lit)
	for (i = 1; i < o_max; i++)
	{
		/* Get the next object from the dungeon */
		o_ptr = &o_list[i];
		
		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;
		
		/* Skip held objects */
		if (o_ptr->held_m_idx) continue;
		
		/* If the object is in sight, or under the player... */
		if ((cave_info[o_ptr->iy][o_ptr->ix] & (CAVE_SEEN)) || 
		    ((p_ptr->py == o_ptr->iy) && (p_ptr->px == o_ptr->ix)))
		{
			update_lore_aux(o_ptr);
		}
	}
	
	// Scan the inventory / equipment
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		/* Get the next object from the inventory/equipment */
		o_ptr = &inventory[i];
		
		/* Skip empty objects */
		if (!o_ptr->k_idx) continue;
		
		update_lore_aux(o_ptr);
	}
}

/*
 * Handle "p_ptr->update"
 */
void update_stuff(void)
{
	// always handle the Lore-Keeper ability
	update_lore();

	/* Update stuff */
	if (!p_ptr->update) return;

	if (p_ptr->update & (PU_BONUS))
	{
		p_ptr->update &= ~(PU_BONUS);
		calc_bonuses();
	}

	if (p_ptr->update & (PU_HP))
	{
		p_ptr->update &= ~(PU_HP);
		calc_hitpoints();
	}

	if (p_ptr->update & (PU_MANA))
	{
		p_ptr->update &= ~(PU_MANA);
		calc_voice();
	}


	/* Character is not ready yet, no screen updates */
	if (!character_generated) return;

	/* Character is in "icky" mode, no screen updates */
	if (character_icky) return;
	
	if (p_ptr->update & (PU_FORGET_VIEW))
	{
		p_ptr->update &= ~(PU_FORGET_VIEW);
		forget_view();
	}

	if (p_ptr->update & (PU_UPDATE_VIEW))
	{
		p_ptr->update &= ~(PU_UPDATE_VIEW);
		update_view();
	}

	if (p_ptr->update & (PU_DISTANCE))
	{
		p_ptr->update &= ~(PU_DISTANCE);
		p_ptr->update &= ~(PU_MONSTERS);
		update_monsters(TRUE);
	}

	if (p_ptr->update & (PU_MONSTERS))
	{
		p_ptr->update &= ~(PU_MONSTERS);
		update_monsters(FALSE);
	}

	if (p_ptr->update & (PU_PANEL))
	{
		p_ptr->update &= ~(PU_PANEL);
		verify_panel();
	}

}


/*
 * Handle "p_ptr->redraw"
 */
void redraw_stuff(void)
{
	/* Redraw stuff */
	if (!p_ptr->redraw) return;

	/* Character is not ready yet, no screen updates */
	if (!character_generated) return;

	/* Character is in "icky" mode, no screen updates */
	if (character_icky && !p_ptr->is_dead) return;

	if (p_ptr->redraw & (PR_MAP))
	{
		p_ptr->redraw &= ~(PR_MAP);
		prt_map();
	}

	if (p_ptr->redraw & (PR_BASIC))
	{
		p_ptr->redraw &= ~(PR_BASIC);
		p_ptr->redraw &= ~(PR_STATS);
		p_ptr->redraw &= ~(PR_MEL | PR_EXP | PR_ARC);
		p_ptr->redraw &= ~(PR_ARMOR | PR_HP | PR_VOICE | PR_SONG);
		p_ptr->redraw &= ~(PR_DEPTH | PR_HEALTHBAR);
		p_ptr->redraw &= ~(PR_RESIST);
		prt_frame_basic();
	}

	if (p_ptr->redraw & (PR_MISC))
	{
		p_ptr->redraw &= ~(PR_MISC);
		
		/* Name */
		c_put_str(TERM_WHITE, "            ", ROW_NAME, COL_NAME);
		if (strlen(op_ptr->full_name) <= 12)
		{
			prt_field(op_ptr->full_name, ROW_NAME, COL_NAME);
		}
	}

	if (p_ptr->redraw & (PR_EXP))
	{
		p_ptr->redraw &= ~(PR_EXP);
		prt_exp();
	}

	if (p_ptr->redraw & (PR_STATS))
	{
		p_ptr->redraw &= ~(PR_STATS);
		prt_stat(A_STR);
		prt_stat(A_DEX);
		prt_stat(A_CON);
		prt_stat(A_GRA);
	}

	if (p_ptr->redraw & (PR_MEL))
	{
		p_ptr->redraw &= ~(PR_MEL);
		prt_mel();
	}

	if (p_ptr->redraw & (PR_ARC))
	{
		p_ptr->redraw &= ~(PR_ARC);
		prt_arc();
	}

	if (p_ptr->redraw & (PR_ARMOR))
	{
		p_ptr->redraw &= ~(PR_ARMOR);
		prt_evn();
	}

	if (p_ptr->redraw & (PR_HP))
	{
		p_ptr->redraw &= ~(PR_HP);
		prt_hp();

		/*
		 * hack:  redraw player, since the player's color
		 * now indicates approximate health. 
		 */
		if (arg_graphics == GRAPHICS_NONE)
		{
		 	lite_spot(p_ptr->py, p_ptr->px);
		}

	}

	if (p_ptr->redraw & (PR_VOICE))
	{
		p_ptr->redraw &= ~(PR_VOICE);
		prt_sp();
	}

	/* Sil - Hack: always redraw song (really should invent redraw flag for it etc. */
	if (p_ptr->redraw & (PR_SONG))
	{
		p_ptr->redraw &= ~(PR_SONG);
		prt_song();
	}

	if (p_ptr->redraw & (PR_DEPTH))
	{
		p_ptr->redraw &= ~(PR_DEPTH);
		prt_depth();
	}

	if (p_ptr->redraw & (PR_HEALTHBAR))
	{
		p_ptr->redraw &= ~(PR_HEALTHBAR);
		health_redraw();
	}

	if (p_ptr->redraw & (PR_EXTRA))
	{
		p_ptr->redraw &= ~(PR_EXTRA);
		p_ptr->redraw &= ~(PR_CUT | PR_STUN);
		p_ptr->redraw &= ~(PR_HUNGER);
		p_ptr->redraw &= ~(PR_BLIND | PR_CONFUSED);
		p_ptr->redraw &= ~(PR_AFRAID | PR_POISONED);
		p_ptr->redraw &= ~(PR_STATE | PR_SPEED);
		prt_frame_extra();
	}

	if (p_ptr->redraw & (PR_CUT))
	{
		p_ptr->redraw &= ~(PR_CUT);
		prt_cut();
	}

	if (p_ptr->redraw & (PR_STUN))
	{
		p_ptr->redraw &= ~(PR_STUN);
		prt_stun();
	}

	if (p_ptr->redraw & (PR_HUNGER))
	{
		p_ptr->redraw &= ~(PR_HUNGER);
		prt_hunger();
	}

	if (p_ptr->redraw & (PR_BLIND))
	{
		p_ptr->redraw &= ~(PR_BLIND);
		prt_blind();
	}

	if (p_ptr->redraw & (PR_CONFUSED))
	{
		p_ptr->redraw &= ~(PR_CONFUSED);
		prt_confused();
	}

	if (p_ptr->redraw & (PR_AFRAID))
	{
		p_ptr->redraw &= ~(PR_AFRAID);
		prt_afraid();
	}

	if (p_ptr->redraw & (PR_POISONED))
	{
		p_ptr->redraw &= ~(PR_POISONED);
		prt_poisoned();
	}

	if (p_ptr->redraw & (PR_STATE))
	{
		p_ptr->redraw &= ~(PR_STATE);
		prt_state();
	}

	if (p_ptr->redraw & (PR_SPEED))
	{
		p_ptr->redraw &= ~(PR_SPEED);
		prt_speed();
	}
    
	if (p_ptr->redraw & (PR_TERRAIN))
	{
		p_ptr->redraw &= ~(PR_TERRAIN);
        prt_terrain();
	}
}


/*
 * Handle "p_ptr->window"
 */
void window_stuff(void)
{
	int j;

	u32b mask = 0L;


	/* Nothing to do */
	if (!p_ptr->window) return;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		/* Save usable flags */
		if (angband_term[j])
		{
			/* Build the mask */
			mask |= op_ptr->window_flag[j];
		}
	}

	/* Apply usable flags */
	p_ptr->window &= (mask);

	/* Nothing to do */
	if (!p_ptr->window) return;


	/* Display inventory */
	if (p_ptr->window & (PW_INVEN))
	{
		p_ptr->window &= ~(PW_INVEN);
		fix_inven();
	}

	/* Display monster list */
	if (p_ptr->window & (PW_MONLIST))
	{
		p_ptr->window &= ~(PW_MONLIST);
		fix_monlist();
	}

	/* Display equipment */
	if (p_ptr->window & (PW_EQUIP))
	{
		p_ptr->window &= ~(PW_EQUIP);
		fix_equip();
	}

	/* Display player (mode 0) */
	if (p_ptr->window & (PW_PLAYER_0))
	{
		p_ptr->window &= ~(PW_PLAYER_0);
		fix_player_0();
	}

	/* Display combat rolls */
	if (p_ptr->window & (PW_COMBAT_ROLLS))
	{
		p_ptr->window &= ~(PW_COMBAT_ROLLS);
		fix_combat_rolls();
	}

	/* Display message recall */
	if (p_ptr->window & (PW_MESSAGE))
	{
		p_ptr->window &= ~(PW_MESSAGE);
		fix_message();
	}

	/* Display monster recall */
	if (p_ptr->window & (PW_MONSTER))
	{
		p_ptr->window &= ~(PW_MONSTER);
		fix_monster();
	}

}


/*
 * Handle "p_ptr->update" and "p_ptr->redraw" and "p_ptr->window"
 */
void handle_stuff(void)
{
	/* Update stuff */
	if (p_ptr->update) update_stuff();

	/* Redraw stuff */
	if (p_ptr->redraw) redraw_stuff();

	/* Window stuff */
	if (p_ptr->window) window_stuff();
}



