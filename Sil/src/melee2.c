/* File: melee2.c */

/*
 * Copyright (c) 2001 Leon Marrick & Bahman Rabii, Ben Harrison,
 * James E. Wilson, Robert A. Koeneke
 *
 * Additional code and concepts by David Reeve Sward, Keldon Jones,
 * and others.
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"



/*
 * Terrified monsters will turn to fight if they are slower than the
 * character, and closer to him than this distance.
 */
#define TURN_RANGE      3

/*
 * Calculate minimum and desired combat ranges.  -BR-
 */
static void find_range(monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	/* All "afraid" monsters will run away */
	if (m_ptr->stance == STANCE_FLEEING) m_ptr->min_range = FLEE_RANGE;

	/* Some monsters run when low on mana */
	else if ((r_ptr->flags2 & (RF2_LOW_MANA_RUN)) &&
	    (m_ptr->mana < MON_MANA_MAX / 5)) m_ptr->min_range = FLEE_RANGE;
	
	/*mindless monsters always charge*/
	else if (r_ptr->flags2 & (RF2_MINDLESS)) m_ptr->min_range = 1;

	/* Other monsters default to range 1 */
	else	m_ptr->min_range = 1;

	if (m_ptr->min_range < FLEE_RANGE)
	{
		/* Creatures that don't move never like to get too close */
		if (r_ptr->flags1 & (RF1_NEVER_MOVE)) m_ptr->min_range += 3;

		/* Spellcasters that don't strike never like to get too close */
		if (r_ptr->flags1 & (RF1_NEVER_BLOW)) m_ptr->min_range += 3;
		
		// Spies have a high minimum range
		if ((r_ptr->flags2 & (RF2_SMART)) && (r_ptr->flags4 & (RF4_SHRIEK)) && (m_ptr->stance != STANCE_AGGRESSIVE))
		{
			m_ptr->min_range = 10;
		}
	}

	/* Handle range greater than FLEE Range (but without an if statement
     * for efficiency)
	 */
	else m_ptr->min_range = FLEE_RANGE;

	/* Nearby monsters that cannot run away will stand and fight */
	if ((m_ptr->cdis < TURN_RANGE) && (m_ptr->mspeed < p_ptr->pspeed))
		m_ptr->min_range = 1;

	/* Now find preferred range */
	m_ptr->best_range = m_ptr->min_range;

	if ((r_ptr->freq_ranged > 15) && (m_ptr->r_idx != R_IDX_MORGOTH))
	{
		/* Breathers like range 2  */
		if ((r_ptr->flags4 & (RF4_BREATH_MASK)) &&
		    (m_ptr->best_range < 6))
		{
			m_ptr->best_range = 2;
		}
		
		/* Specialized ranged attackers will sit back */
		else if (m_ptr->mana >= MON_MANA_MAX / 5)
		{ 
			m_ptr->best_range += (r_ptr->freq_ranged - 15) / 5;
			if (m_ptr->best_range > 8) m_ptr->best_range = 8;
			m_ptr->min_range = m_ptr->best_range - 1;
		}
	}
	
	// Deal with the 'truce' on Morgoth's level (overrides everything else)
	if (p_ptr->truce && (m_ptr->min_range < 5))
	{ 
		m_ptr->min_range = 5;
		m_ptr->best_range = 5;
	}
}



/*
 * Given a central direction at position [dir #][0], return a series
 * of directions radiating out on both sides from the central direction
 * all the way back to its rear.
 *
 * Side directions come in pairs; for example, directions '1' and '3'
 * flank direction '2'.  The code should know which side to consider
 * first.  If the left, it must add 10 to the central direction to
 * access the second part of the table.
 */
static byte side_dirs[20][8] =
{
	{ 0, 0, 0, 0, 0, 0, 0, 0 },	/* bias right */
	{ 1, 4, 2, 7, 3, 8, 6, 9 },
	{ 2, 1, 3, 4, 6, 7, 9, 8 },
	{ 3, 2, 6, 1, 9, 4, 8, 7 },
	{ 4, 7, 1, 8, 2, 9, 3, 6 },
	{ 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 6, 3, 9, 2, 8, 1, 7, 4 },
	{ 7, 8, 4, 9, 1, 6, 2, 3 },
	{ 8, 9, 7, 6, 4, 3, 1, 2 },
	{ 9, 6, 8, 3, 7, 2, 4, 1 },

	{ 0, 0, 0, 0, 0, 0, 0, 0 },	/* bias left */
	{ 1, 2, 4, 3, 7, 6, 8, 9 },
	{ 2, 3, 1, 6, 4, 9, 7, 8 },
	{ 3, 6, 2, 9, 1, 8, 4, 7 },
	{ 4, 1, 7, 2, 8, 3, 9, 6 },
	{ 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 6, 9, 3, 8, 2, 7, 1, 4 },
	{ 7, 4, 8, 1, 9, 2, 6, 3 },
	{ 8, 7, 9, 4, 6, 1, 3, 2 },
	{ 9, 8, 6, 7, 3, 4, 2, 1 }
};


/*
 * Get and return the strength (age) of scent in a given grid.
 *
 * Return "-1" if no scent exists in the grid.
 */
int get_scent(int y, int x)
{
	int age;
	int scent;

	/* Check Bounds */
	if (!(in_bounds(y, x))) return (-1);

	/* Sent trace? */
	scent = cave_when[y][x];

	/* No scent at all */
	if (!scent) return (-1);

	/* Get age of scent */
	age = scent - scent_when;
	
	if (age > SMELL_STRENGTH) return (-1);

	/* Return the age of the scent */
	return (age);
}


/*
 * Can the monster catch a whiff of the character?
 *
 * Many more monsters can smell, but they find it hard to smell and
 * track down something at great range.
 */
static bool monster_can_smell(monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	int age;

	/* Get the age of the scent here */
	age = get_scent(m_ptr->fy, m_ptr->fx);

	/* No scent */
	if (age == -1) return (FALSE);

	/* Wolves are amazing trackers */
	if (strchr("C", r_ptr->d_char))
	{
		/* I smell a character! */
		return (TRUE);
	}

	/* Felines are also quite good */
	else if (strchr("f", r_ptr->d_char))
	{
		if (age <= SMELL_STRENGTH / 2)
		{
			/* Something's in the air... */
			return (TRUE);
		}
	}

	/* You're imagining things. */
	return (FALSE);
}


/*
 * Used to exclude spells which are too expensive for the
 * monster to cast.  Excludes all spells that cost more than the
 * current available mana.
 *
 * Smart monsters may also exclude spells that use a lot of mana,
 * even if they have enough.
 *
 * -BR-
 */
static void remove_expensive_spells(int m_idx, u32b *f4p)
{
	monster_type *m_ptr = &mon_list[m_idx];

	int i;

	u32b f4 = (*f4p);

	/* check innate spells for mana available */
	for (i = 0; i < 32; i++)
	{
		if (spell_info_RF4[i][COL_SPELL_MANA_COST] > m_ptr->mana) f4 &= ~(0x00000001 << i);
	}

	/* Modify the spell list. */
	(*f4p) = f4;
}

/*
 * Intelligent monsters use this function to filter away spells
 * which have no benefit.
 */
static void remove_invalid_spells(int m_idx, u32b *f4p)
{
	monster_type *m_ptr = &mon_list[m_idx];
	
	u32b f4 = (*f4p);

	int dy, dx;
	int dist = distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px);
	
	// Screech only works at very close range
	if (m_ptr->cdis > 2)
	{
		f4 &= ~(RF4_SCREECH);
	}

	// make sure that missile attacks are never done at melee range or when afraid
	if ((dist == 1) || (m_ptr->stance == STANCE_FLEEING) || p_ptr->truce)
	{
		f4 &= ~(RF4_ARROW1);
		f4 &= ~(RF4_ARROW2);
		f4 &= ~(RF4_BOULDER);
		f4 &= ~(RF4_EARTHQUAKE);
	}
	
    // make sure that breath attacks are never used when the monster is fleeing
    if (m_ptr->stance == STANCE_FLEEING)
    {
        f4 &= ~(RF4_BREATH_MASK);
    }
    
    // no songs during the truce
    if (p_ptr->truce)
    {
        f4 &= ~(RF4_SNG_MASK);
    }
    
    // no songs by Morgoth until uncrowned
    if ((m_ptr->r_idx == R_IDX_MORGOTH) && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
    {
        f4 &= ~(RF4_SNG_MASK);
    }
    
	// projectiles have limited range
	if (dist > 5)		f4 &= ~(RF4_BOULDER);
	if (dist > 10)		f4 &= ~(RF4_ARROW1);
	if (dist > 16)		f4 &= ~(RF4_ARROW2);
	
	// Earthquake is only useful at close range and if there is no monster in the smashed square
	dy = (m_ptr->fy > p_ptr->py) ? -1 : ((m_ptr->fy < p_ptr->py) ? 1 : 0);
	dx = (m_ptr->fx > p_ptr->px) ? -1 : ((m_ptr->fx < p_ptr->px) ? 1 : 0);
	if ((m_ptr->cdis > 3) || (cave_m_idx[m_ptr->fy + dy][m_ptr->fx + dx] > 0))
	{
		f4 &= ~(RF4_EARTHQUAKE);
	}

	/* Darkness is only useful if the player's square is lit */
	if (!(cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW))) f4 &= ~(RF4_DARKNESS);
		
	/* Modify the spell list. */
	(*f4p) = f4;

}


/*
 * Count the number of castable spells.
 *
 * If exactly 1 spell is available cast it.  If more than more is
 * available, and the random bit is set, pick one.
 *
 * Used as a short cut in 'choose_attack_spell' to circumvent AI
 * when there is only 1 choice. (random=FALSE)
 *
 * Also used in 'choose_attack_spell' to circumvent AI when
 * casting randomly (random=TRUE), as with dumb monsters.
 */
static int choose_attack_spell_fast(u32b *f4p, bool do_random)
{
	int i, num=0;
	byte spells[128];

	u32b f4 = (*f4p);

	/* Extract the 'spells' */
	for (i = 0; i < 32; i++)
	{
		if (f4 & (1L << i)) spells[num++] = i + 32 * 3;
	}

	/* Paranoia */
	if (num == 0) return (0);

	/* Go quick if possible */
	if (num == 1)
	{
		/* Cast the one spell */
		return (spells[0]);
	}

	/*
	 * If we aren't allowed to choose at random
	 * and we have multiple spells left, give up on quick
	 * selection
	 */
	if (!(do_random)) return (0);

	/* Pick at random */
	return (spells[rand_int(num)]);
}

/*
 * Have a monster choose a spell.
 *
 * Monster at m_idx uses this function to select a legal attack spell.
 * Spell casting AI is based here.
 *
 * First the code will try to save time by seeing if
 * choose_attack_spell_fast is helpful.  Otherwise, various AI
 * parameters are used to calculate a 'desirability' for each spell.
 * There is some randomness.  The most desirable spell is cast.
 *
 * Returns the spell number, of '0' if no spell is selected.
 *
 *-BR-
 */
static int choose_ranged_attack(int m_idx)
{
	monster_type *m_ptr = &mon_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	u32b f4;

	byte spell_range;

	bool do_random = FALSE;

	int i;
	int path;

	int cur_range=0;

	int best_spell=0, best_spell_rating=0;
	int cur_spell_rating;

	/* Extract the racial spell flags */
	f4 = r_ptr->flags4;

	/* Check what kinds of spells can hit player */
	path = projectable(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK);

	/* do we have the player in sight at all? */
	if (path == PROJECT_NO)
	{
		return (0);
	}

	/* remove boulders and archery */
	else if (path == PROJECT_NOT_CLEAR)
	{
		f4 &= ~(RF4_ARCHERY_MASK);
	}

	/* No spells left */
	if (!f4) return (0);

	/* Spells we can not afford */
	remove_expensive_spells(m_idx, &f4);

	/* No spells left */
	if (!f4) return (0);

	/* Mindless monsters choose at random. */
	if (r_ptr->flags2 & (RF2_MINDLESS)) return (choose_attack_spell_fast(&f4, TRUE));

	/* Remove spells that have unfulfilled conditions */
	remove_invalid_spells(m_idx, &f4);

	/* No spells left */
	if (!f4) return (0);

	/* Sometimes non-dumb monsters cast randomly (though from the
	 * restricted list)
	 */
	if ((!(r_ptr->flags2 & (RF2_SMART))) && (one_in_(5))) do_random = TRUE;

	/* Try 'fast' selection first.
	 * If there is only one spell, choose that spell.
	 * If there are multiple spells, choose one randomly if the 'random' flag is set.
	 * Otherwise fail, and let the AI choose.
	 */
	best_spell = choose_attack_spell_fast(&f4, do_random);
	if (best_spell) return (best_spell);

	/* Check if no spells left */
	if (!f4) return (0);

	/* The conditionals are written for speed rather than readability
	 * They should probably stay that way. */
	for (i = 0; i < 32; i++)
	{
		/* Do we even have this spell? */
		if (!(f4 &(1L <<  i    ))) continue;
		spell_range = spell_info_RF4[i][COL_SPELL_BEST_RANGE];

		/* Base Desirability*/
		cur_spell_rating = spell_desire_RF4[i][D_BASE];

		/* Penalty for range if attack drops off in power */
		if (spell_range)
		{
			cur_range = m_ptr->cdis;
			while (cur_range-- > spell_range)
				cur_spell_rating = (cur_spell_rating * spell_desire_RF4[i][D_RANGE])/100;
		}

		/* Random factor; less random for smart monsters */
		if (r_ptr->flags2 & (RF2_SMART)) cur_spell_rating += rand_int(10);
		else cur_spell_rating += rand_int(50);

		/* Is this the best spell yet?, or alternate between equal spells*/
		if ((cur_spell_rating > best_spell_rating) ||
			((cur_spell_rating == best_spell_rating) && one_in_(2)))
		{
			best_spell_rating = cur_spell_rating;
			best_spell = i + 96;
		}
	}

	if (p_ptr->wizard)
	{
		msg_format("Spell rating: %i.", best_spell_rating);
	}

	// Abort if there are no good spells
	if (best_spell_rating == 0) return (0);
	
	/* Return Best Spell */
	return (best_spell);
}




/*
 * Can the monster exist in this grid?
 *
 * Because this function is designed for use in monster placement and
 * generation as well as movement, it cannot accept monster-specific
 * data, but must rely solely on racial information.
 */
bool cave_exist_mon(monster_race *r_ptr, int y, int x, bool occupied_ok, bool can_dig)
{
	int feat;

	/* Check Bounds */
	if (!in_bounds(y, x)) return (FALSE);

	/* Check location */
	feat = cave_feat[y][x];

	/* The grid is already occupied. */
	if (cave_m_idx[y][x] != 0)
	{
		if (!occupied_ok) return (FALSE);
	}

	/* Glyphs -- must break first */
	if (feat == FEAT_GLYPH) return (FALSE);


	/*** Check passability of various features. ***/

    // only flying creatures can pass chasms
    if (cave_feat[y][x] == FEAT_CHASM)
    {
        if (r_ptr->flags2 & (RF2_FLYING))   return (TRUE);
        else                                return (FALSE);
    }
    
	/* Feature is not a wall */
	if (!(cave_info[y][x] & (CAVE_WALL))) return (TRUE);

	/* Feature is a wall */
	else
	{
		/* Permanent walls are never OK */
		if (feat == FEAT_WALL_PERM)    return (FALSE);

		/* Monster can pass through walls */
		if (r_ptr->flags2 & (RF2_PASS_WALL)) return (TRUE);

		/* Monster can bore through walls, and is allowed to. */
		if ((r_ptr->flags2 & (RF2_KILL_WALL)) && (can_dig)) return (TRUE);

		/* Monster can dig through walls, and is allowed to. */
		if ((r_ptr->flags2 & (RF2_TUNNEL_WALL)) && (can_dig)) return (TRUE);
        
        // Some monsters can pass under doors
		if (cave_any_closed_door_bold(y,x) && (r_ptr->flags2 & (RF2_PASS_DOOR)))
		{
			return (TRUE);
		}
		
		else return (FALSE);
	}

}


/*
 * Can the monster enter this grid?  How easy is it for them to do so?
 *
 * Returns the percentage chance of success.
 *
 * The code that uses this function sometimes assumes that it will never
 * return a value greater than 100.
 *
 * The usage of level to determine whether one monster can push past
 * another is a tad iffy, but ensures that orc soldiers can always
 * push past other orc soldiers.
 */
int cave_passable_mon(monster_type *m_ptr, int y, int x, bool *bash)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	/* Assume nothing in the grid other than the terrain hinders movement */
	int move_chance = 100;

	int feat;
	
	/* Check Bounds */
	if (!in_bounds(y, x)) return (0);

	/* Check location */
	feat = cave_feat[y][x];

	/* Permanent walls are never passable */
	if (feat == FEAT_WALL_PERM)    return (0);

	/* The grid is occupied by the player. */
	if (cave_m_idx[y][x] < 0)
	{
		/* Monster has no melee blows - character's grid is off-limits. */
		if (r_ptr->flags1 & (RF1_NEVER_BLOW)) return (0);

		/* Any monster with melee blows can attack the character. */
		else return (100);
	}

	/* The grid is occupied by a monster. */
	else if (cave_m_idx[y][x] > 0)
	{
		monster_type *n_ptr = &mon_list[cave_m_idx[y][x]];
		monster_race *nr_ptr = &r_info[n_ptr->r_idx];

		/* Some creatures can kill weaker monsters */
		if ((r_ptr->flags2 & (RF2_KILL_BODY)) &&
		    (!(nr_ptr->flags1 & (RF1_UNIQUE))) &&
		    (r_ptr->level > nr_ptr->level))
		{
			move_chance = 100;
		}

		/* All can attempt to push past monsters that can move */
		else if (!(nr_ptr->flags1 & (RF1_NEVER_MOVE)) && !(nr_ptr->flags1 & (RF1_HIDDEN_MOVE)))
		{
			// It is easy to push past unwary or sleeping monsters
			if ((n_ptr->alertness < ALERTNESS_ALERT) && (m_ptr->wandering_idx != n_ptr->wandering_idx)) move_chance = 80;
			
			// It is easy for non-fleeing monsters to push past fleeing monsters
			else if ((n_ptr->stance == STANCE_FLEEING) && (m_ptr->stance != STANCE_FLEEING)) move_chance = 80;
			
			// It is easy for fleeing monsters to push past non-fleeing monsters
			else if ((n_ptr->stance != STANCE_FLEEING) && (m_ptr->stance == STANCE_FLEEING)) move_chance = 80;

			// It is easy to push past weaker monsters
			else if (r_ptr->level > nr_ptr->level) move_chance = 80;
			
			// It is quite hard to push past monsters of equal strength
			else if (r_ptr->level == nr_ptr->level) move_chance = 20;
			
			// It is very difficult to move past alert, unafraid, stronger monsters
			else move_chance = 10;
		}

		/* Cannot do anything to clear away the other monster */
		else return (0);
	}

	/* Glyphs */
	if (feat == FEAT_GLYPH)
	{
		// a simulated Will check
		int break_chance = success_chance(10, monster_skill(m_ptr, S_WIL), 20);
		
		// can always attack the player if the player is standing on the glyph
		if ((p_ptr->py == y) && (p_ptr->px == x)) break_chance = 100;
		
		// unwary monsters won't break glyphs
		if (m_ptr->alertness < ALERTNESS_ALERT) break_chance = 0;
		
		/* Glyphs are hard to break */
		if (move_chance > break_chance)
			 move_chance = break_chance;
		
	}

    // only flying creatures can pass chasms
    if (cave_feat[y][x] == FEAT_CHASM)
    {
        if (!(r_ptr->flags2 & (RF2_FLYING)))   return (0);
    }


	/*** Check passability of various features. ***/

	/* Feature is not a wall */
	if (!(cave_info[y][x] & (CAVE_WALL)))
	{
		/* Any monster can handle floors, except glyphs and chasms, which are handled above */
		return (move_chance);
	}


	/* Feature is a 'wall', including doors */
	else
	{
		/* Granite, Quartz, Rubble */
		if (((feat >= FEAT_QUARTZ) && (feat <= FEAT_WALL_SOLID)) || (feat == FEAT_RUBBLE))
		{
			/* Impassible except for monsters that move through walls */
			if ((r_ptr->flags2 & (RF2_PASS_WALL)) || (r_ptr->flags2 & (RF2_KILL_WALL))) return (move_chance);
            
            // alert monsters can slowly tunnel through walls
            else if ((r_ptr->flags2 & (RF2_TUNNEL_WALL)) && (m_ptr->alertness >= ALERTNESS_ALERT)) return (move_chance);
            
			else return (0);
		}

		/* Doors */
		if (cave_any_closed_door_bold(y,x))
		{
            int unlock_chance = 0;
			int bash_chance = 0;

			// monsters don't open doors in the tutorial mode
			//if (p_ptr->game_type < 0)
			//{
			//	return (0);
			//}

			// Some monsters can simply pass through doors
			if (r_ptr->flags2 & (RF2_PASS_DOOR) || (r_ptr->flags2 & (RF2_PASS_WALL)))
			{
				return (move_chance);
			}
				
			// unwary monsters won't open doors in vaults or interesting rooms
			if ((m_ptr->alertness < ALERTNESS_ALERT) && (cave_info[y][x] & (CAVE_ICKY)))
			{
				return (0);
			}
            
			// no monsters will open secret doors in vaults or interesting rooms
			if ((cave_feat[y][x] == FEAT_SECRET) && (cave_info[y][x] & (CAVE_ICKY)))
			{
				return (0);
			}
            
			/* Monster can open doors */
			if (r_ptr->flags2 & (RF2_OPEN_DOOR))
			{
				/* Closed doors and secret doors */
				if ((feat == FEAT_DOOR_HEAD) || (feat == FEAT_SECRET))
				{
					/*
					 * Note:  This section will have to be rewritten if
					 * secret doors can be jammed or locked as well.
					 */

					/*
					 * It usually takes two turns to open a door
					 * and move into the doorway.
					 */
					return(move_chance);
				}

				/*
				 * Locked doors (not jammed).  Monsters know how hard
				 * doors in their neighborhood are to unlock.
				 */
				else if ((r_ptr->flags2 & (RF2_UNLOCK_DOOR)) && (feat < FEAT_DOOR_HEAD + 0x08))
				{
					int difficulty, skill;
					
					/* Lock difficulty (power + 5) */
					difficulty = (feat - FEAT_DOOR_HEAD) + 5;

					/* Unlocking skill equals monster perception */
					skill = monster_skill(m_ptr, S_PER);
										
					/*
					 * we ignore the fact that it takes extra time to
					 * open the door and walk into the entranceway.
					 */
					unlock_chance = success_chance(10,skill,difficulty);
				}
			}

			/* Monster can bash doors */
			if (r_ptr->flags2 & (RF2_BASH_DOOR))
			{
				int difficulty, skill;

				/* Door difficulty (power + 2) */
				/*
				 * XXX - just because a door is difficult to unlock
				 * shouldn't mean that it's hard to bash.  Until the
				 * character door bashing code is changed, however,
				 * we'll stick with this.
				 */
				difficulty = ((feat - FEAT_DOOR_HEAD) % 8);

				/*
				 * Calculate bashing ability (ie effective strength)
				 */
				skill = monster_stat(m_ptr, A_STR) * 2;

				/*
				 * Note that
				 * monsters "fall" into the entranceway in the same
				 * turn that they bash the door down.
				 */
				bash_chance = success_chance(10, skill, difficulty);
			}

			/*
			 * A monster cannot both bash and unlock a door in the same
			 * turn.  It needs to pick one of the two methods to use.
			 */
            
			if ((unlock_chance > bash_chance) || (bash_chance == 0)) *bash = FALSE;
			else *bash = TRUE;
			
			return (MIN(move_chance, (MAX(unlock_chance, bash_chance))));
		}

		/* Any wall grid that isn't explicitly made passable is impassable. */
		return (0);
	}

}


/*
 *  Determine the next move for an unwary wandering monster
 */
static bool get_move_wander(monster_type *m_ptr, int *ty, int *tx)
{
	int d;
    
    int dist;
	int closest = FLOW_MAX_DIST - 1;
    
	byte y, x, y1, x1;
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	bool no_move = FALSE;
	bool random_move = FALSE;
					
	/* Monster location */
	y1 = m_ptr->fy;
	x1 = m_ptr->fx;
	
	// Deal with monsters that don't have a destination
	if (!m_ptr->wandering_idx)
	{
		// Some monsters cannot move at all
		if (r_ptr->flags1 & (RF1_NEVER_MOVE))
		{
			return (FALSE);
		}
		
		// Some just never wander
		else if ((r_ptr->flags2 & (RF2_SHORT_SIGHTED)) || (r_ptr->flags1 & (RF1_HIDDEN_MOVE)))
		{
			return (FALSE);
		}
		
		// Many monsters can only make random moves
		else
		{
			random_move = TRUE;
		}
	}
	
	// Deal with some special cases for monsters that have a destination
	else
	{
		int i;
		int group_size = 0;
		int group_furthest = 0;
		int group_sleepers = 0;
		int sleeper_y = 0;
		int sleeper_x = 0;
		int max_drop;
		
        // how far is the monster from its wandering destination?
        dist = flow_dist(m_ptr->wandering_idx, y1, x1);
        
		// check out monsters with the same index
		for (i = 1; i < mon_max; i++)
		{
			monster_type *n_ptr = &mon_list[i];
			
			/* Skip dead monsters */
			if (!n_ptr->r_idx) continue;
			
			// record some features of the group
			if (n_ptr->wandering_idx == m_ptr->wandering_idx)
			{
				group_size++;
				
				if (n_ptr->alertness < ALERTNESS_UNWARY)
				{
					group_sleepers++;
					if (group_sleepers == 1)
					{
						sleeper_y = n_ptr->fy;
						sleeper_x = n_ptr->fx;
					}
				}
				if (n_ptr->wandering_dist > group_furthest)		group_furthest = n_ptr->wandering_dist;
			}
		}
		
		// no wandering on the Gates level
		if (p_ptr->depth == 0)
		{
			return (FALSE);
		}
		
		// no wandering in the throne room during the truce
		if (p_ptr->truce)
		{
			return (FALSE);
		}
		
		// determine if the monster has a hoard
		max_drop = 
		(((r_ptr->flags1 & RF1_DROP_4D2) ? 8 : 0) +
		 ((r_ptr->flags1 & RF1_DROP_3D2) ? 6 : 0) +
		 ((r_ptr->flags1 & RF1_DROP_2D2) ? 4 : 0) +
		 ((r_ptr->flags1 & RF1_DROP_1D2) ? 2 : 0) +
		 ((r_ptr->flags1 & RF1_DROP_100)  ? 1 : 0) +
		 ((r_ptr->flags1 & RF1_DROP_33)  ? 1 : 0));
		
		// treasure-hoarding territorial monsters stay still at their hoard...
		if ((r_ptr->flags2 & (RF2_TERRITORIAL)) && (max_drop > 0) && 
		    (flow_dist(m_ptr->wandering_idx, y1, x1) == 0))
		{
			// very occasionally fall asleep
			if (one_in_(100) && (p_ptr->game_type >= 0) && !(r_ptr->flags3 & (RF3_NO_SLEEP))) 
			{
				set_alertness(m_ptr, rand_range(ALERTNESS_MIN, ALERTNESS_UNWARY-1));
			}
			
			return (FALSE);
		}
		
		// if the destination is too far away, pick a new one
		if (dist > MON_WANDER_RANGE)
		{
			new_wandering_flow(m_ptr, 0, 0);
		}

		// if there is no pausing going on and it is at the destination, then start pausing
		if ((wandering_pause[m_ptr->wandering_idx] == 0) && (dist <= 0))
		{
			wandering_pause[m_ptr->wandering_idx] = dieroll(50) * group_size;
		}

		// if the monster is pausing, then decrease the pause counter
		else if (wandering_pause[m_ptr->wandering_idx] > 1)
		{
			random_move = TRUE;
			wandering_pause[m_ptr->wandering_idx]--;
		}
		
		// if the monster has finished pausing at an old destination
		else if (wandering_pause[m_ptr->wandering_idx] == 1)
		{
			// choose a new destination
			new_wandering_flow(m_ptr, 0, 0);
			wandering_pause[m_ptr->wandering_idx]--;
		}

		
		// if the monster is not making progress
		if (dist >= m_ptr->wandering_dist)
		{
			// possibly pick a new destination
			if (one_in_(20 * group_size))
			{
				new_wandering_flow(m_ptr, 0, 0);
			}
		}
		
		// sometimes delay to let others catch up
		if (dist < group_furthest - group_size)
		{
			if (one_in_(2)) no_move = TRUE;
		}
		
		// unwary monsters won't wander off while others in the group are sleeping
		if ((m_ptr->alertness < ALERTNESS_ALERT) && (group_sleepers > 0))
		{
			// only set the new flow if needed
			if ((flow_center_y[m_ptr->wandering_idx] != sleeper_y) || (flow_center_x[m_ptr->wandering_idx] != sleeper_x))
			{
				new_wandering_flow(m_ptr, sleeper_y, sleeper_x);
			}
			
			if (one_in_(2)) random_move = TRUE;
		}
		
		// non-territorial monsters in vaults move randomly
		if (!(r_ptr->flags2 & (RF2_TERRITORIAL)) && (cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_ICKY)))
		{
			random_move = TRUE;
		}
		
		// update the wandering_dist
		m_ptr->wandering_dist = dist;
	}

	if (no_move) return (FALSE);

	// do a random move if needed
	if (random_move)
	{        
		// mostly stay still
		if (!one_in_(4))
		{
			return (FALSE);
		}
		
		// sometimes move
		else
		{
			/* Random direction */
			d = ddd[rand_int(8)];

			y = y1 + ddy_ddd[d];
			x = x1 + ddx_ddd[d];

			/* Check Bounds */
			if (!in_bounds(y, x)) return (FALSE);

			// Monsters in vaults shouldn't leave them
			if ((cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_ICKY)) && !(cave_info[y][x] & (CAVE_ICKY))) return (FALSE);

			/* Save the location */
			*ty = y;
			*tx = x;
		}
	}
	
	// move towards destination
	else
	{
		// smart monsters who are at the stairs they are aiming for leave the level
		if ((r_ptr->flags2 & (RF2_SMART)) && !(r_ptr->flags2 & (RF2_TERRITORIAL)) && (p_ptr->depth != MORGOTH_DEPTH) &&
		     cave_stair_bold(m_ptr->fy, m_ptr->fx) && (m_ptr->wandering_dist == 0))
		{
			char m_name[80];
			
			if (m_ptr->ml)
			{
				monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);
				if (cave_down_stairs_bold(m_ptr->fy, m_ptr->fx))	msg_format("%^s goes down the stairs.", m_name);
				else                                            msg_format("%^s goes up the stairs.", m_name);
			}
			
			// stop pausing to allow others to use the stairs
			wandering_pause[m_ptr->wandering_idx] = 0;
			
			delete_monster(m_ptr->fy, m_ptr->fx);
			return (FALSE);
		}
					
		/* Using flow information.  Check nearby grids, diagonals first. */
		for (d = 7; d >= 0; d--)
		{
			/* Get the location */
			y = y1 + ddy_ddd[d];
			x = x1 + ddx_ddd[d];
			
			/* Check Bounds */
			if (!in_bounds(y, x)) continue;
			
			dist = flow_dist(m_ptr->wandering_idx, y, x);
			
			// ignore grids that are further than the current favourite
			if (closest < dist) continue;
            
			closest = dist;
			
			/* Save the location */
			*ty = y;
			*tx = x;
		}
		
		// if no useful square to wander into was found, then abort
		if (closest == FLOW_MAX_DIST - 1)
		{
			return (FALSE);
		}
		
	}
	
	// success
	return (TRUE);
}



/*
 * "Do not be seen."
 *
 * Monsters in LOS that want to retreat are primarily interested in
 * finding a nearby place that the character can't see into.
 * Search for such a place with the lowest cost to get to up to 15
 * grids away.
 *
 * Look outward from the monster's current position in a square-
 * shaped search pattern.  Calculate the approximate cost in monster
 * turns to get to each passable grid, using a crude route finder.  Penal-
 * ize grids close to or approaching the character.  Ignore hiding places
 * with no safe exit.  Once a passable grid is found that the character
 * can't see, the code will continue to search a little while longer,
 * depending on how pricey the first option seemed to be.
 *
 * If the search is successful, the monster will target that grid,
 * and (barring various special cases) run for it until it gets there.
 *
 * We use a limited waypoint system (see function "get_route_to_target()"
 * to reduce the likelihood that monsters will get stuck at a wall between
 * them and their target (which is kinda embarrassing...).
 *
 * This function does not yield perfect results; it is known to fail
 * in cases where the previous code worked just fine.  The reason why
 * it is used is because its failures are less common and (usually)
 * less embarrassing than was the case before.  In particular, it makes
 * monsters great at not being seen.
 *
 * This function is fairly expensive.  Call it only when necessary.
 */
static bool find_safety(monster_type *m_ptr, int *ty, int *tx)
{
	int i, j, d;

	int y, x, yy, xx;

	int countdown = HIDE_RANGE;

	int least_cost = 100;
	int least_cost_y = 0;
	int least_cost_x = 0;
	int chance, cost, parent_cost;
	bool dummy;
	bool stair;
    
    monster_race *r_ptr = &r_info[m_ptr->r_idx];
	
	/* Factors for converting table to actual dungeon grids */
	int conv_y, conv_x;
	
	/*
	 * Allocate and initialize a table of movement costs.
	 * Both axis must be (2 * HIDE_RANGE + 1).
	 */
	byte safe_cost[HIDE_RANGE*2+1][HIDE_RANGE*2+1];

	for (i = 0; i < (HIDE_RANGE*2+1); i++)
	{
		for (j = 0; j < (HIDE_RANGE*2+1); j++)
		{
			safe_cost[i][j] = 0;
		}
	}

	conv_y = HIDE_RANGE - m_ptr->fy;
	conv_x = HIDE_RANGE - m_ptr->fx;

	/* Mark the origin */
	safe_cost[HIDE_RANGE][HIDE_RANGE] = 1;

	/* If the character's grid is in range, mark it as being off-limits */
	if ((ABS(m_ptr->fy - p_ptr->py) <= HIDE_RANGE) &&
	    (ABS(m_ptr->fx - p_ptr->px) <= HIDE_RANGE))
	{
		safe_cost[p_ptr->py + conv_y][p_ptr->px + conv_x] = 100;
	}

	/* Work outward from the monster's current position */
	for (d = 0; d < HIDE_RANGE; d++)
	{
		for (y = HIDE_RANGE - d; y <= HIDE_RANGE + d; y++)
		{
			for (x = HIDE_RANGE - d; x <= HIDE_RANGE + d;)
			{
				int x_tmp;

				/*
				 * Scan all grids of top and bottom rows, just
				 * outline other rows.
				 */
				if ((y != HIDE_RANGE - d) && (y != HIDE_RANGE + d))
				{
					if (x == HIDE_RANGE + d) x_tmp = 999;
					else x_tmp = HIDE_RANGE + d;
				}
				else x_tmp = x + 1;

				/* Grid and adjacent grids must be legal */
				if (!in_bounds_fully(y - conv_y, x - conv_x))
				{
					x = x_tmp;
					continue;
				}

				/* Grid is inaccessible (or at least very difficult to enter) */
				if ((safe_cost[y][x] == 0) || (safe_cost[y][x] >= 100))
				{
					x = x_tmp;
					continue;
				}

				/* Get the accumulated cost to enter this grid */
				parent_cost = safe_cost[y][x];

				/* Scan all adjacent grids */
				for (i = 0; i < 8; i++)
				{
					yy = y + ddy_ddd[i];
					xx = x + ddx_ddd[i];

					/* check bounds */
					if ((yy < 0) || (yy > HIDE_RANGE*2) || (xx < 0) || (xx > HIDE_RANGE*2)) continue;

					/*
					 * Handle grids with empty cost and passable grids
					 * with costs we have a chance of beating.
					 */
					if ((safe_cost[yy][xx] == 0) ||
					      ((safe_cost[yy][xx] > parent_cost + 1) &&
					       (safe_cost[yy][xx] < 100)))
					{
						/* Get the cost to enter this grid */
						chance = cave_passable_mon(m_ptr, yy - conv_y,
						         xx - conv_x, &dummy);

						/* Impassable */
						if (!chance)
						{
							/* Cannot enter this grid */
							safe_cost[yy][xx] = 100;
							continue;
						}

						/* Calculate approximate cost (in monster turns) */
						cost = 100 / chance;

						/* Next to character */
						if (distance(yy - conv_y, xx - conv_x,
						    p_ptr->py, p_ptr->px) <= 1)
						{
							/* Don't want to maneuver next to the character */
							cost += 3;
						}

						/* Mark this grid with a cost value */
						safe_cost[yy][xx] = parent_cost + cost;
						
						// check whether it is a stair and the monster can use these
						stair = cave_stair_bold(yy - conv_y, xx - conv_x) &&
                                (r_ptr->flags2 & (RF2_SMART)) && !(r_ptr->flags2 & (RF2_TERRITORIAL));

						/* Character can't see this grid, or it is a stair... */
						if (!player_can_see_bold(yy - conv_y, xx - conv_x) || stair)
						{
							int this_cost = safe_cost[yy][xx];

							/* Penalize grids that approach character */
							if (ABS(p_ptr->py - (yy - conv_y)) <
							    ABS(m_ptr->fy - (yy - conv_y)))
							{
								 this_cost *= 2;
							}
							if (ABS(p_ptr->px - (xx - conv_x)) <
							    ABS(m_ptr->fx - (xx - conv_x)))
							{
								 this_cost *= 2;
							}

							// Value stairs very highly
							if (stair)
							{
								this_cost /= 2;
							}
							
							/* Accept lower-cost, sometimes accept same-cost options */
							if ((least_cost > this_cost) ||
							    (least_cost == this_cost && one_in_(2)))
							{
								bool has_escape = FALSE;

								/* Scan all adjacent grids for escape routes */
								for (j = 0; j < 8; j++)
								{
									/* Calculate real adjacent grids */
									int yyy = yy - conv_y + ddy_ddd[i];
									int xxx = xx - conv_x + ddx_ddd[i];

									/* Check bounds */
									if (!in_bounds(yyy, xxx)) continue;

									/* Look for any passable grid that isn't in LOS */
									if ((!player_can_see_bold(yyy, xxx)) &&
									    (cave_passable_mon(m_ptr, yyy, xxx, &dummy)))
									{
										/* Not a one-grid cul-de-sac */
										has_escape = TRUE;
										break;
									}
								}

								/* Ignore cul-de-sacs other than stairs */
								if ((has_escape == FALSE) && !stair) continue;

								least_cost = this_cost;
								least_cost_y = yy;
								least_cost_x = xx;

								/*
								 * Look hard for alternative hiding places if
								 * this one seems pricey.
								 */
								countdown = 1 + least_cost - d;
							}
						}
					}
				}

				/* Adjust x as instructed */
				x = x_tmp;
			}
		}

		/*
		 * We found a good place a while ago, and haven't done better
		 * since, so we're probably done.
		 */
		if (countdown-- <= 0) break;
	}

	/* We found a place that can be reached in reasonable time */
	if (least_cost < 50)
	{
		/* Convert to actual dungeon grid. */
		y = least_cost_y - conv_y;
		x = least_cost_x - conv_x;

		/* Move towards the hiding place */
		*ty = y;
		*tx = x;

		/* Target the hiding place */
		m_ptr->target_y = y;
		m_ptr->target_x = x;

		return (TRUE);
	}


	/* No good place found */
	return (FALSE);
}


/*
 * Helper function for monsters that want to retreat from the character.
 * Used for any monster that is terrified, frightened, is looking for a
 * temporary hiding spot, or just wants to open up some space between it
 * and the character.
 *
 * If the monster is well away from danger, let it relax.
 * If the monster's current target is not in LOS, use it (+).
 * If the monster is not in LOS, and cannot pass through walls, try to
 * use flow (noise) information.
 * If the monster is in LOS, even if it can pass through walls,
 * search for a hiding place (helper function "find_safety()").
 * If no hiding place is found, and there seems no way out, go down
 * fighting.
 *
 * If none of the above solves the problem, run away blindly.
 *
 * (+) There is one exception to the automatic usage of a target.  If the
 * target is only out of LOS because of "knight's move" rules (distance
 * along one axis is 2, and along the other, 1), then the monster will try
 * to find another adjacent grid that is out of sight.  What all this boils
 * down to is that monsters can now run around corners properly!
 *
 * Return TRUE if the monster did actually want to do anything.
 */
static bool get_move_retreat(monster_type *m_ptr, int *ty, int *tx)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
    int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];
    
	int i;
	int y, x;

	bool done = FALSE;
	bool dummy;

	// if it can call for help, then it might
	if ((r_ptr->flags4 & (RF4_SHRIEK)) && percent_chance(r_ptr->freq_ranged))
	{
		shriek(m_ptr);
		return (FALSE);
	}
		
	/* If the monster is well away from danger, let it relax. */
	if (m_ptr->cdis >= FLEE_RANGE)
	{
		return (FALSE);
	}
	
	// intelligent monsters that are fleeing can try to use stairs
	if ((r_ptr->flags2 & (RF2_SMART)) && !(r_ptr->flags2 & (RF2_TERRITORIAL)) && (m_ptr->stance == STANCE_FLEEING))
	{
		if (cave_stair_bold(m_ptr->fy, m_ptr->fx))
		{
			*ty = m_ptr->fy;
			*tx = m_ptr->fx;
			return (TRUE);
		}
		
		// check for adjacent stairs and move towards one
		for (i = 0; i < 8; i++)
		{
			int yy = m_ptr->fy + ddy_ddd[i];
			int xx = m_ptr->fx + ddx_ddd[i];
			bool dummy;
			
			// check for (accessible) stairs
			if (cave_stair_bold(yy,xx) && (cave_passable_mon(m_ptr, yy, xx, &dummy) > 0) && (cave_m_idx[yy][xx] >= 0))
			{
				*ty = yy;
				*tx = xx;
				return (TRUE);
			}
		}
	}
	
	// monsters that like ranged attacks a lot (e.g. archers) try to stay in good shooting locations
	if (r_ptr->freq_ranged >= 50)
	{
		//int prev_cost = cave_cost[which_flow][m_ptr->fy][m_ptr->fx];
		int start = rand_int(8);
		
		bool acceptable = FALSE;
		int best_score = 0;
		int best_y = m_ptr->fy, best_x = m_ptr->fx;
		int dist;
				
		// Set up the 'score to beat' as the score for the monster's current square
		dist = distance_squared(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px);
		best_score += dist;
		if (projectable(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px, PROJECT_STOP) && (m_ptr->cdis > 1)) best_score += 100;

		// the position is only acceptable if it is not adjacent to the player
		if (m_ptr->cdis > 1) acceptable = TRUE;
	
		// Set some hacky global variables so that the project_path() 
		// function doesn't consider the monster's current location to block line of fire.
		project_path_ignore = TRUE;
		project_path_ignore_y = m_ptr->fy;
		project_path_ignore_x = m_ptr->fx;
	
		/* Look for adjacent shooting places */
		for (i = start; i < 8 + start; i++)
		{
			int score = 0;
			
			y = m_ptr->fy + ddy_ddd[i % 8];
			x = m_ptr->fx + ddx_ddd[i % 8];
			
			dist = distance_squared(y, x, p_ptr->py, p_ptr->px);
			
			/* Check Bounds */
			if (!in_bounds(y, x)) continue;
						
			// skip the player's square
			if ((y == p_ptr->py) && (x == p_ptr->px)) continue;
			
			/* Grid must be pretty easy to enter */
			if (cave_passable_mon(m_ptr, y, x, &dummy) < 50) continue;
			
			// skip adjacent squares
			if (distance(y, x, p_ptr->py, p_ptr->px) == 1) continue;

			// any position non-adjacent to the player will be acceptable
			acceptable = TRUE;
			
			// reward distance from player
			score += dist;
			
			/* reward having a shot at the player */
			if (projectable(y, x, p_ptr->py, p_ptr->px, PROJECT_STOP) && (dist > 1)) score += 100;
			
			/* Penalize any grid that doesn't have a lower flow (noise) cost. */
			// Sil-y: I'm not sure what this step does
			//if (cave_cost[which_flow][y][x] < prev_cost) score -= 10;
						
			if (score > best_score)
			{
				best_score = score;
				best_y = y;
				best_x = x;
			}
		}

		// Unset some hacky global variables so that the project_path() 
		// function didn't consider the monster's current location to block line of fire.
		project_path_ignore = FALSE;
		project_path_ignore_y = 0;
		project_path_ignore_x = 0;
		
		if (acceptable)
		{
			*ty = best_y;
			*tx = best_x;
			
			/* Success */
			return (TRUE);
		}
		
		// Sil-y:
		// This step is artificial stupidity for archers and other serious ranged weapon users.
		// They only evade you properly near walls if they are: afraid or uniques or invisible
		// Otherwise things are a bit too annoying
		else if ((m_ptr->stance != STANCE_FLEEING) && !(r_ptr->flags1 & (RF1_UNIQUE)) && m_ptr->ml)
		{
			return (FALSE);
		}
		
	}

	/* Monster has a target */
	if ((m_ptr->target_y) && (m_ptr->target_x))
	{
		/* It's out of LOS; keep using it, except in "knight's move" cases */
		if (!player_has_los_bold(m_ptr->target_y, m_ptr->target_x))
		{
			/* Get axis distance from character to current target */
			int dist_y = ABS(p_ptr->py - m_ptr->target_y);
			int dist_x = ABS(p_ptr->px - m_ptr->target_x);

			/* It's only out of LOS because of "knight's move" rules */
			if (((dist_y == 2) && (dist_x == 1)) ||
			    ((dist_y == 1) && (dist_x == 2)))
			{
				/*
				 * If there is another grid adjacent to the monster that
				 * the character cannot see into, and it isn't any harder
				 * to enter, use it instead.  Prefer diagonals.
				 */
				for (i = 7; i >= 0; i--)
				{
					y = m_ptr->fy + ddy_ddd[i];
					x = m_ptr->fx + ddx_ddd[i];

					/* Check Bounds */
					if (!in_bounds(y, x)) continue;

					if (player_has_los_bold(y, x)) continue;

					if ((y == m_ptr->target_y) && (x == m_ptr->target_x)) continue;

					if (cave_passable_mon(m_ptr, m_ptr->target_y, m_ptr->target_x, &dummy) >
					    cave_passable_mon(m_ptr, y, x, &dummy)) continue;

					m_ptr->target_y = y;
					m_ptr->target_x = x;
					break;
				}
			}

			/* Move towards the target */
			*ty = m_ptr->target_y;
			*tx = m_ptr->target_x;
			return (TRUE);
		}

		/* It's in LOS, but not a stair; cancel it. */
		else if (!cave_stair_bold(m_ptr->target_y, m_ptr->target_x))
		{
			m_ptr->target_y = 0;
			m_ptr->target_x = 0;
		}
	}

	/* The monster is not in LOS, but thinks it's still too close. */
	if (!player_has_los_bold(m_ptr->fy, m_ptr->fx))
	{
        /* Run away from noise */
        if (flow_dist(m_idx, m_ptr->fy, m_ptr->fx) < FLOW_MAX_DIST)
        {
            /* Look at adjacent grids, diagonals first */
            for (i = 7; i >= 0; i--)
            {
                y = m_ptr->fy + ddy_ddd[i];
                x = m_ptr->fx + ddx_ddd[i];

                /* Check bounds */
                if (!in_bounds(y, x)) continue;

                /* Accept the first non-visible grid with a higher cost */
                if (flow_dist(m_idx, y, x) > flow_dist(m_idx, m_ptr->fy, m_ptr->fx))
                {
                    if (!player_has_los_bold(y, x))
                    {
                        *ty = y;  *tx = x;
                        done = TRUE;
                        break;
                    }
                }
            }

            /* Return if successful */
            if (done) return (TRUE);
        }

		/* No flow info, or don't need it -- see bottom of function */
	}

	/* The monster is in line of sight. */
	else
	{
		int prev_dist = flow_dist(m_idx, m_ptr->fy, m_ptr->fx);
		int start = rand_int(8);

		/* Look for adjacent hiding places */
		for (i = start; i < 8 + start; i++)
		{
			y = m_ptr->fy + ddy_ddd[i % 8];
			x = m_ptr->fx + ddx_ddd[i % 8];

			/* Check Bounds */
			if (!in_bounds(y, x)) continue;

			/* No grids in LOS */
			if (player_has_los_bold(y, x)) continue;

			/* Grid must be pretty easy to enter */
			if (cave_passable_mon(m_ptr, y, x, &dummy) < 50) continue;

			/* Accept any grid that doesn't have a lower flow (noise) cost. */
			if (flow_dist(m_idx, y, x) >= prev_dist)
			{
				*ty = y;
				*tx = x;
				prev_dist = flow_dist(m_idx, y, x);

				/* Success */
				return (TRUE);
			}
		}

		/* Find a nearby grid not in LOS of the character. */
		if (find_safety(m_ptr, ty, tx) == TRUE) return (TRUE);

		/*
		 * No safe place found.  If monster is in LOS and close,
		 * it will turn to fight.
		 */
		if ((player_has_los_bold(m_ptr->fy, m_ptr->fx)) &&
		    ((m_ptr->cdis < TURN_RANGE) || (m_ptr->mspeed < p_ptr->pspeed)) && !p_ptr->truce && (r_ptr->freq_ranged < 50))
		{			
			/* Message if visible */
			if (m_ptr->ml)
			{
				char m_name[80];
				
				/* Get the monster name */
				monster_desc(m_name, sizeof(m_name), m_ptr, 0);
				
				/* Dump a message */
				msg_format("%^s panics.", m_name);
			}
			
            // boost morale and make the monster aggressive
            m_ptr->tmp_morale = MAX(m_ptr->tmp_morale + 60, 60);
            calc_morale(m_ptr);
            calc_stance(m_ptr);
            m_ptr->mflag |= (MFLAG_AGGRESSIVE);

			return (TRUE);
		}
	}

	// Sil-y: This code below seemed hopelessly wrong, so I'm trying out a new version
	/* Move directly away from character. */
	//*ty = -(p_ptr->py - m_ptr->fy);
	//*tx = -(p_ptr->px - m_ptr->fx);
	*ty = m_ptr->fy - (p_ptr->py - m_ptr->fy);
	*tx = m_ptr->fx - (p_ptr->px - m_ptr->fx);

	/* We want to run away */
	return (TRUE);
}


/*
 * Helper function for monsters that want to advance toward the character.
 * Assumes that the monster isn't frightened, and is not in LOS of the
 * character.
 *
 * Ghosts and rock-eaters do not use flow information, because they
 * can - in general - move directly towards the character.  We could make
 * them look for a grid at their preferred range, but the character
 * would then be able to avoid them better (it might also be a little
 * hard on those poor warriors...).
 *
 * Other monsters will use target information, then their ears, then their
 * noses (if they can), and advance blindly if nothing else works.
 *
 * When flowing, monsters prefer non-diagonal directions.
 *
 * XXX - At present, this function does not handle difficult terrain
 * intelligently.  Monsters using flow may bang right into a door that
 * they can't handle.  Fixing this may require code to set monster
 * paths.
 */
static void get_move_advance(monster_type *m_ptr, int *ty, int *tx)
{
	int py = p_ptr->py;
	int px = p_ptr->px;
    
	int i;
    
	byte y, x, y1, x1;
    
	int closest = FLOW_MAX_DIST;
    
	bool can_use_sound = FALSE;
	bool can_use_scent = FALSE;
    
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];
    
    int m_idx;
    
	// Some monsters don't try to pursue when out of sight
	if ((r_ptr->flags2 & (RF2_TERRITORIAL)) && !los(py, px, m_ptr->fy, m_ptr->fx))
	{
        // remember that the monster behaves this
		l_ptr->flags2 |= (RF2_TERRITORIAL);
        
		*ty = m_ptr->fy;
		*tx = m_ptr->fx;
		
		// sometimes become unwary and wander back to its lair
		if (one_in_(10) && (m_ptr->alertness >= ALERTNESS_ALERT)) set_alertness(m_ptr, m_ptr->alertness - 1);
        
		return;
	}
    
	/* Monster location */
	y1 = m_ptr->fy;
	x1 = m_ptr->fx;
    
    // Monster index
    m_idx = cave_m_idx[y1][x1];
    
	/* Use target information if available */
	if ((m_ptr->target_y) && (m_ptr->target_x))
	{
		*ty = m_ptr->target_y;
		*tx = m_ptr->target_x;
		return;
	}
    
    
	/* If we can hear noises, advance towards them */
	if (flow_dist(m_idx, y1, x1) < FLOW_MAX_DIST)
	{
		can_use_sound = TRUE;
	}
    
	/* Otherwise, try to follow a scent trail */
	else if (monster_can_smell(m_ptr))
	{
		can_use_scent = TRUE;
	}
    
	/* Otherwise */
	if ((!can_use_sound) && (!can_use_scent))
	{
        // sight but no 'sound' implies blocked by a chasm, so get out of there!
        if (los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
        {
            get_move_retreat(m_ptr, ty, tx);
            return;
        }
        
        // no sound, no scent, no sight: advance blindly
        else
        {
            *ty = py;
            *tx = px;
            return;
        }
	}
    
	/* Using flow information.  Check nearby grids, diagonals first. */
	for (i = 7; i >= 0; i--)
	{
		/* Get the location */
		y = y1 + ddy_ddd[i];
		x = x1 + ddx_ddd[i];
        
		/* Check Bounds */
		if (!in_bounds(y, x)) continue;
        
		/* We're following a scent trail */
		if (can_use_scent)
		{
			int age = get_scent(y, x);
			if (age == -1) continue;
            
			/* Accept younger scent */
			if (closest < age) continue;
			closest = age;
		}
        
		/* We're using sound */
		else
		{
			int dist = flow_dist(m_idx, y, x);
            
			/* Accept louder sounds */
			if (closest < dist) continue;
			closest = dist;
		}
        
		/* Save the location */
		*ty = y;
		*tx = x;
	}
}



// This determines how vulnerable the player is to monster attacks
// It combines elements for available spaces to attack from and for
// the player's condition and other monsters attacking
//
// I'm sure it could be further improved

static int calc_vulnerability(int fy, int fx)
{
	int py = p_ptr->py;
	int px = p_ptr->px;
	int dy, dx;
    int dir;
	int vulnerability;

	// reset the vulnerability
	vulnerability = 0;

    // determine the main direction from the player to the monster
    dir = rough_direction(py, px, fy, fx);
    
    // extract the deltas from the direction
    dy = ddy[dir];
    dx = ddx[dir];
	
	// if monster in an orthogonal direction   753
	//                                         8@1 m
	//                                         642
	if (dy * dx == 0)
	{
		// increase vulnerability for each open square towards the monster
		if (cave_floor_bold(py+dy, px+dx))       vulnerability++;   // direction 1
		if (cave_floor_bold(py+dx+dy, px-dy+dx)) vulnerability++;   // direction 2
		if (cave_floor_bold(py-dx+dy, px+dy+dx)) vulnerability++;   // direction 3
		if (cave_floor_bold(py+dx, px-dy))       vulnerability++;   // direction 4
		if (cave_floor_bold(py-dx, px+dy))       vulnerability++;   // direction 5
		
		// increase vulnerability for monsters already engaged with the player...
		//if (cave_m_idx[py+dy][px+dx] > 0)       vulnerability++;    // direction 1
		if (cave_m_idx[py+dx+dy][px-dy+dx] > 0) vulnerability++;    // direction 2 
		if (cave_m_idx[py-dx+dy][px+dy+dx] > 0) vulnerability++;    // direction 3
		if (cave_m_idx[py+dx][px-dy] > 0)       vulnerability++;    // direction 4
		if (cave_m_idx[py-dx][px+dy] > 0)       vulnerability++;    // direction 5

		// ...especially if they are behind the player
		if (cave_m_idx[py+dx-dy][px-dy-dx] > 0) vulnerability += 2; // direction 6
		if (cave_m_idx[py-dx-dy][px+dy-dx] > 0) vulnerability += 2; // direction 7
		if (cave_m_idx[py-dy][px-dx] > 0)       vulnerability += 2; // direction 8
	}
	// if monster in a diagonal direction   875
	//                                      6@3
	//                                      421
	//                                          m
	else
	{
		// increase vulnerability for each open square towards the monster
		if (cave_floor_bold(py+dy, px+dx)) vulnerability++;   // direction 1
		if (cave_floor_bold(py+dy, px))    vulnerability++;   // direction 2
		if (cave_floor_bold(py, px+dx))    vulnerability++;   // direction 3
		if (cave_floor_bold(py+dx, px-dy)) vulnerability++;   // direction 4
		if (cave_floor_bold(py-dx, px+dy)) vulnerability++;   // direction 5

		// increase vulnerability for monsters already engaged with the player...
		//if (cave_m_idx[py+dy][px+dx] > 0) vulnerability++;    // direction 1
		if (cave_m_idx[py+dy][px] > 0)    vulnerability++;    // direction 2
		if (cave_m_idx[py][px+dx] > 0)    vulnerability++;    // direction 3
		if (cave_m_idx[py+dx][px-dy] > 0) vulnerability++;    // direction 4
		if (cave_m_idx[py-dx][px+dy] > 0) vulnerability++;    // direction 5
		
		// ...especially if they are behind the player
		if (cave_m_idx[py-dy][px] > 0)    vulnerability += 2; // direction 6
		if (cave_m_idx[py][px-dx] > 0)    vulnerability += 2; // direction 7
		if (cave_m_idx[py-dy][px-dx] > 0) vulnerability += 2; // direction 8
	}
	
	// Take player's health into account
	switch (health_level(p_ptr->chp, p_ptr->mhp))
	{
		case  HEALTH_WOUNDED:		vulnerability += 1;	break;  // <= 75% health
		case  HEALTH_BADLY_WOUNDED:	vulnerability += 1;	break;  // <= 50% health
		case  HEALTH_ALMOST_DEAD:	vulnerability += 2;	break;  // <= 25% health
	}

	// Take player's conditions into account
	if (p_ptr->blind || p_ptr->image || p_ptr->confused ||
		p_ptr->afraid || p_ptr->entranced || (p_ptr->stun > 50) || p_ptr->slow)
	{
		vulnerability += 2;
	}
	
	return vulnerability;
}


// This determines how hesitant the monster is to attack.
// If the hesitance is lower than the player's vulnerability, it will attack
//
// The main way to gain hesitance is to have similar smart monsters who could
// gang up if they waited for the player to get into the open.

int calc_hesitance(monster_type *m_ptr)
{
	int x, y;
	int fy = m_ptr->fy;
	int fx = m_ptr->fx;
	int hesitance = 1;
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_type *n_ptr;

	// gain hesitance for up to one nearby similar monster
	// who isn't yet engaged in combat
	for (y = -5; y <= +5; y++)
	{
		for (x = -5; x <= +5; x++)
		{
			if (!((x == 0) && (y == 0)) && in_bounds(fy+y,fx+x))
			{
				if (cave_m_idx[fy+y][fx+x] > 0)
				{
					n_ptr = &mon_list[cave_m_idx[fy+y][fx+x]];
					
					if (similar_monsters(fy, fx, fy+y, fx+x) &&
					    (distance(fy+y,fx+x,p_ptr->py,p_ptr->px) > 1) && (hesitance < 2))
					{
						hesitance++;
					}
				}
			}
		}
	}
	
	// archers should be slightly more hesitant as they are in an excellent situation
	if ((r_ptr->freq_ranged > 30) && (hesitance == 2))
	{
		hesitance++;
	}
	
	return (hesitance);
}

/*
 * Counts the number of monsters adjacent to a given square
 */
int adj_mon_count(int y, int x)
{
	int xx, yy;
	int count = 0;
	
	for (yy = -1; yy <= +1; yy++)
	{
		for (xx = -1; xx <= +1; xx++)
		{
			if (!((xx == 0) && (yy == 0)))
			{
				if (cave_m_idx[y+yy][x+xx] > 0)
				{
					count++;
				}
			}
		}
	}
	
	return (count);
}

/*
 * Choose the probable best direction for a monster to move in.  This
 * is done by choosing a target grid and then finding the direction that
 * best approaches it.
 *
 * Monsters that cannot move always attack if possible.
 * Frightened monsters retreat.
 * Monsters adjacent to the character attack if possible.
 *
 * Monster packs lure the character into open ground and then leap
 * upon him.  Monster groups try to surround the character.  -KJ-
 *
 * Monsters not in LOS always advance (this avoids player frustration).
 * Monsters in LOS will advance to the character, up to their standard
 * combat range, to a grid that allows them to target the character, or
 * just stay still if they are happy where they are, depending on the
 * tactical situation and the monster's preferred and minimum combat
 * ranges.
 * NOTE:  Here is an area that would benefit from more development work.
 *
 * Non-trivial movement calculations are performed by the helper
 * functions "get_move_advance" and "get_move_retreat", which keeps
 * this function relatively simple.
 *
 * The variable "must_use_target" is used for monsters that can't
 * currently perceive the character, but have a known target to move
 * towards.  With a bit more work, this will lead to semi-realistic
 * "hunting" behavior.
 *
 * Return FALSE if monster doesn't want to move or can't.
 */
static bool get_move(monster_type *m_ptr, int *ty, int *tx, bool *fear,
                     bool must_use_target)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];

	int i, start;
	int y, x;

	int py = p_ptr->py;
	int px = p_ptr->px;
	
	/* Assume no movement */
	*ty = m_ptr->fy;
	*tx = m_ptr->fx;

	/*
	 * Some monsters will not move into sight of the player.
	 */
	if ((r_ptr->flags1 & (RF1_HIDDEN_MOVE)) && 
	    ((cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_SEEN)) || seen_by_keen_senses(m_ptr->fy,m_ptr->fx)))
	{
		/* Memorize lack of moves after a while. */
		if (!(l_ptr->flags1 & (RF1_HIDDEN_MOVE)))
		{
			if (m_ptr->ml && (one_in_(50)))
			{
				l_ptr->flags1 |= (RF1_HIDDEN_MOVE);
			}
		}
		/* If we are in sight, do not move */
		return (FALSE);
	}
	
	// Morgoth will not move during the 'truce'
	if ((m_ptr->r_idx == R_IDX_MORGOTH) && p_ptr->truce)
	{
		return (FALSE);
	}

    // worm masses, nameless things and the like won't deliberately move towards the player if she is too far away
	if ((r_ptr->flags2 & (RF2_MINDLESS)) && (r_ptr->flags2 & (RF2_TERRITORIAL)) && (m_ptr->cdis > 5))
	{
		return (FALSE);
	}
	
	/*
	 * Monsters that cannot move will attack the character if he is
	 * adjacent.  Otherwise, they cannot move.
	 */
	if (r_ptr->flags1 & (RF1_NEVER_MOVE))
	{
		/* Hack -- memorize lack of moves after a while. */
		if (!(l_ptr->flags1 & (RF1_NEVER_MOVE)))
		{
			if (m_ptr->ml && (one_in_(20)))
				l_ptr->flags1 |= (RF1_NEVER_MOVE);
		}

		/* Is character in range? */
		if (m_ptr->cdis <= 1)
		{
			/* Monster can't melee either (pathetic little creature) */
			if (r_ptr->flags1 & (RF1_NEVER_BLOW))
			{
				/* Hack -- memorize lack of attacks after a while */
				if (!(l_ptr->flags1 & (RF1_NEVER_BLOW)))
				{
					if (m_ptr->ml && (one_in_(10)))
						l_ptr->flags1 |= (RF1_NEVER_BLOW);
				}
			}

			/* Can attack */
			else
			{
				/* Kill. */
				*fear = FALSE;
				*ty = py;
				*tx = px;
				return (TRUE);
			}
		}

		/* If we can't hit anything, do not move */
		return (FALSE);
	}

	/*
	 * Monster is only allowed to use targeting information.
	 */
	if (must_use_target)
	{
		*ty = m_ptr->target_y;
		*tx = m_ptr->target_x;
		return (TRUE);
	}

	/*** Handle monster fear -- only for monsters that can move ***/

	/* Is the monster scared? */
	if ((m_ptr->min_range >= FLEE_RANGE) || (m_ptr->stance == STANCE_FLEEING)) *fear = TRUE;
	else *fear = FALSE;


	/* Monster is frightened or terrified. */
	if (*fear)
	{
		/* The character is too close to avoid, and faster than we are */
		if ((m_ptr->stance != STANCE_FLEEING) && (m_ptr->cdis < TURN_RANGE) &&
		     (p_ptr->pspeed > m_ptr->mspeed))
		{
			/* Recalculate range */
			find_range(m_ptr);

			/* Note changes in monster attitude */
			if (m_ptr->min_range < m_ptr->cdis)
			{
				/* Cancel fear */
				*fear = FALSE;

				/* No message -- too annoying */

				/* Charge! */
				*ty = py;
				*tx = px;

				return (TRUE);
			}
		}

		/* The monster is within 25 grids of the character */
		else if (m_ptr->cdis < FLEE_RANGE)
		{
			/* Find and move towards a hidey-hole */
			get_move_retreat(m_ptr, ty, tx);
			return (TRUE);
		}

		/* Monster is well away from danger */
		else
		{
			/* No need to move */
			return (FALSE);
		}
	}

	// if far too close, step back towards the monster's minimum range
	if ((!*fear) && (m_ptr->cdis < m_ptr->min_range - 2))
	{
		if (get_move_retreat(m_ptr, ty, tx))
		{
			*fear = TRUE;
			return (TRUE);
		}
		else
		{
			/* No safe spot -- charge */
			*ty = py;
			*tx = px;
		}			
	}

	/* If the character is adjacent, back off, surround the player, or attack.  */
	if ((!*fear) && (m_ptr->cdis <= 1))
	{
		/* Monsters that cannot attack back off. */
		if (r_ptr->flags1 & (RF1_NEVER_BLOW))
		{
			/* Hack -- memorize lack of attacks after a while */
			if (!(l_ptr->flags1 & (RF1_NEVER_BLOW)))
			{
				if (m_ptr->ml && (one_in_(10)))
					l_ptr->flags1 |= (RF1_NEVER_BLOW);
			}

			/* Back away */
			*fear = TRUE;
		}

		else
		{
			// Smart monsters try harder to surround the player
			if (r_ptr->flags2 & (RF2_SMART))
			{				
				int fy = m_ptr->fy;
				int fx = m_ptr->fx;
				int count = adj_mon_count(fy, fx);
				int dy = py - fy;
				int dx = px - fx;
				
				start = rand_int(8);
				
				/* Maybe move to a less crowded square near the player if possible */
				for (i = start; i < 8 + start; i++)
				{
					/* Pick squares near player */
					y = py + ddy_ddd[i % 8];
					x = px + ddx_ddd[i % 8];
					
					// if also adjacent to monster
					if ((ABS(fy - y) <= 1) && (ABS(fx - x) <= 1) && !((fy == y) && (fx == x)))
					{
						// if it is free...
						if (cave_floor_bold(y,x) && (cave_m_idx[y][x] <= 0))
						{
							// and has a lower count...
							if ((adj_mon_count(y,x) <= count) && ((r_ptr->flags2 & (RF2_FLANKING)) || one_in_(2)))
							{
								// then maybe set it as a new target
								*ty = y;
								*tx = x;
								return (TRUE);
							}
							
						}
					}
				}
				
				/* If the monster didn't do that, then check for end-corridor cases */
				
				// if player is in an orthogonal direction, eg:
				//
				//  X#A
				//  Xo@
				//  X#B
				//
				if (dy * dx == 0)
				{
					// if walls on either side of monster ('#')
					if (cave_wall_bold(fy+dx,fx+dy) && cave_wall_bold(fy-dx,fx-dy))
					{
						// if there is a monster in one of the three squares behind ('X')
						if ((cave_m_idx[fy+dx-dy][fx+dy-dx] > 0) || 
							(cave_m_idx[fy-dy][fx-dx] > 0)       || 
							(cave_m_idx[fy-dx-dy][fx-dy-dx] > 0))
						{
							// if 'A' and 'B' are free, go to one at random
							if ((cave_m_idx[fy+dx+dy][fx+dy+dx] <= 0) &&
								(cave_m_idx[fy-dx+dy][fx-dy+dx] <= 0))
							{
								if (one_in_(2))
								{
									*ty = fy+dx+dy;
									*tx = fx+dy+dx;
								}
								else
								{
									*ty = fy-dx+dy;
									*tx = fx-dy+dx;
								}
								return (TRUE);
							}
							// if 'A' is free, go there
							else if (cave_m_idx[fy+dx+dy][fx+dy+dx] <= 0)
							{
								*ty = fy+dx+dy;
								*tx = fx+dy+dx;
								return (TRUE);
							}
							// if 'B' is free, go there
							else if (cave_m_idx[fy-dx+dy][fx-dy+dx] <= 0)
							{
								*ty = fy-dx+dy;
								*tx = fx-dy+dx;
								return (TRUE);
							}
							
						}
						
					}
				}
				// if player is in a diagonal direction, eg:
				//
				//  X#       XXX
				//  XoA  or  #o#
				//  X#@       A@
				//
				else
				{
					// if walls north and south of monster ('#')
					if (cave_wall_bold(fy+1,fx) && cave_wall_bold(fy-1,fx))
					{
						// if there is a monster in one of the three squares behind ('X')
						if ((cave_m_idx[fy-1][fx-dx] > 0) || 
							(cave_m_idx[fy][fx-dx] > 0)   || 
							(cave_m_idx[fy+1][fx-dx] > 0))
						{
							// if 'A' is free, go there
							if (cave_m_idx[fy][fx+dx] <= 0)
							{
								*ty = fy;
								*tx = fx+dx;
								return (TRUE);
							}
						}
					}
					// if walls east and west of monster ('#')
					else if (cave_wall_bold(fy,fx-1) && cave_wall_bold(fy,fx+1))
					{
						// if there is a monster in one of the three squares behind ('X')
						if ((cave_m_idx[fy-dy][fx-1] > 0) || 
							(cave_m_idx[fy-dy][fx] > 0)   || 
							(cave_m_idx[fy-dy][fx+1] > 0))
						{
							// if 'A' is free, go there
							if (cave_m_idx[fy+dy][fx] <= 0)
							{
								*ty = fy+dy;
								*tx = fx;
								return (TRUE);
							}
						}
					}
				}
				
			}
			
			/* All other monsters attack. */
			*ty = py;
			*tx = px;
			return (TRUE);
		}
	}
		
	// Smart monsters try to lure the character into the open.
	if ((!*fear) && (r_ptr->flags2 & (RF2_SMART)) &&
		!(r_ptr->flags2 & (RF2_PASS_WALL | RF2_KILL_WALL)) &&
		(m_ptr->stance == STANCE_CONFIDENT))
	{
		// determine how vulnerable the player is
		int vulnerability = calc_vulnerability(m_ptr->fy,m_ptr->fx);
		
		// determine how hesitant the monster is
		int hesitance = calc_hesitance(m_ptr);
		
		// Character is insufficiently vulnerable
		if (vulnerability < hesitance)
		{
			/* Monster has to be willing to melee */
			if (m_ptr->min_range == 1)
			{
				/* If we're in sight, find a hiding place */
				if (cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_FIRE | CAVE_SEEN))
				{
					/* Find a safe spot to lurk in */
					if (get_move_retreat(m_ptr, ty, tx))
					{
						*fear = TRUE;
					}
					else
					{
						/* No safe spot -- charge */
						*ty = py;
						*tx = px;
					}
				}

				/* Otherwise, we advance cautiously */
				else
				{
					/* Advance, ... */
					get_move_advance(m_ptr, ty, tx);

					/* ... but make sure we stay hidden. */
					if (m_ptr->cdis > 1) *fear = TRUE;
				}

				/* done */
				return (TRUE);
			}
			else
			{
				/* If we're in sight, find a hiding place */
				if (cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_FIRE | CAVE_SEEN))
				{
					/* Find a safe spot to lurk in */
					if (get_move_retreat(m_ptr, ty, tx))
					{
						*fear = TRUE;
					}
					else
					{
						/* No safe spot -- charge */
						*ty = py;
						*tx = px;
					}
				}
			}
		}
	}

	/* Monster groups try to surround the character */
	if ((!*fear) && ((r_ptr->flags1 & (RF1_FRIENDS)) || (r_ptr->flags1 & (RF1_FRIEND))) &&
	    (m_ptr->cdis <= 3) && (player_has_los_bold(m_ptr->fy, m_ptr->fx)))
	{
		/*Only if we do not have a clean path to player*/
		if (projectable(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK) != PROJECT_CLEAR)
		{
			start = rand_int(8);

			/* Find a random empty square next to the player to head for */
			for (i = start; i < 8 + start; i++)
			{			
				/* Pick squares near player */
				y = py + ddy_ddd[i % 8];
				x = px + ddx_ddd[i % 8];

				/* Check Bounds */
				if (!in_bounds(y, x)) continue;

				/* Ignore occupied grids */
				if (cave_m_idx[y][x] != 0) continue;

				/* Ignore grids that monster can't enter immediately */
				if (!cave_exist_mon(r_ptr, y, x, FALSE, TRUE)) continue;

				/* Accept */
				*ty = y;
				*tx = x;
				return (TRUE);
			}
		}
	}

	/* No special moves made -- use standard movement */

	/* Not frightened */
	if (!*fear)
	{
		/*
		 * XXX XXX -- The monster cannot see the character.  Make it
		 * advance, so the player can have fun ambushing it.
		 */
		if (!player_has_los_bold(m_ptr->fy, m_ptr->fx))
		{
			/* Advance */
			get_move_advance(m_ptr, ty, tx);
		}

		/* Monster can see the character */
		else
		{
			/* Always reset the monster's target */
			m_ptr->target_y = py;
			m_ptr->target_x = px;

			/* Monsters too far away will advance. */
			if (m_ptr->cdis > m_ptr->best_range)
			{
				*ty = py;
				*tx = px;
			}

			/* Monsters not too close will often advance */
			else if ((m_ptr->cdis > m_ptr->min_range)  && (one_in_(2)))
			{
				*ty = py;
				*tx = px;
			}

			/* Monsters that can't target the character will advance. */
			else if (!player_can_fire_bold(m_ptr->fy, m_ptr->fx))
			{
				*ty = py;
				*tx = px;
			}

			/* Otherwise they will stay still or move randomly. */
			else
			{
				/*
				 * It would be odd if monsters that move randomly
				 * were to stay still.
				 */
				if (r_ptr->flags1 & (RF1_RAND_50 | RF1_RAND_25))
				{
					/* Pick a random grid next to the monster */
					i = rand_int(8);

					*ty = m_ptr->fy + ddy_ddd[i];
					*tx = m_ptr->fx + ddx_ddd[i];
				}

				/* Monsters could look for better terrain... */
			}
            
            // in most cases where the monster is targetting the player, use the clever pathfinding instead
            if ((*ty == py) && (*tx == px))
            {
                m_ptr->target_y = 0;
                m_ptr->target_x = 0;

                /* Advance */
                get_move_advance(m_ptr, ty, tx);
            }

		}
	}

	/* Monster is frightened */
	else
	{
		/* Back away -- try to be smart about it */
		get_move_retreat(m_ptr, ty, tx);
	}

	/* We do not want to move */
	if ((*ty == m_ptr->fy) && (*tx == m_ptr->fx)) return (FALSE);

	/* We want to move */
	return (TRUE);
}




/*
 * A simple method to help fleeing monsters who are having trouble getting
 * to their target.  It's very stupid, but works fairly well in the
 * situations it is called upon to resolve.  XXX XXX
 *
 * If this function claims success, ty and tx must be set to a grid
 * adjacent to the monster.
 *
 * Return TRUE if this function actually did any good.
 */
static bool get_route_to_target(monster_type *m_ptr, int *ty, int *tx)
{
	int i, j;
	int y, x, yy, xx;
	int tar_y, tar_x, dist_y, dist_x;

	bool dummy;
	bool below = FALSE;
	bool right = FALSE;

	tar_y = 0;
	tar_x = 0;

	/* Is the target further away vertically or horizontally? */
	dist_y = ABS(m_ptr->target_y - m_ptr->fy);
	dist_x = ABS(m_ptr->target_x - m_ptr->fx);

	/* Target is further away vertically than horizontally */
	if (dist_y > dist_x)
	{
		/* Find out if the target is below the monster */
		if (m_ptr->target_y - m_ptr->fy > 0) below = TRUE;

		/* Search adjacent grids */
		for (i = 0; i < 8; i++)
		{
			y = m_ptr->fy + ddy_ddd[i];
			x = m_ptr->fx + ddx_ddd[i];

			/* Check bounds */
			if (!in_bounds_fully(y, x)) continue;

			/* Grid is not passable */
			if (!cave_passable_mon(m_ptr, y, x, &dummy)) continue;

			/* Grid will take me further away */
			if ((( below) && (y < m_ptr->fy)) ||
			    ((!below) && (y > m_ptr->fy)))
			{
				continue;
			}

			/* Grid will not take me closer or further */
			else if (y == m_ptr->fy)
			{
				/* See if it leads to better things */
				for (j = 0; j < 8; j++)
				{
					yy = y + ddy_ddd[j];
					xx = x + ddx_ddd[j];

					/* Grid does lead to better things */
					if ((( below) && (yy > m_ptr->fy)) ||
					    ((!below) && (yy < m_ptr->fy)))
					{
						/* But it is not passable */
						if (!cave_passable_mon(m_ptr, yy, xx, &dummy)) continue;

						/*
						 * Accept (original) grid, but don't immediately claim
						 * success
						 */
						tar_y = y;
						tar_x = x;
					}
				}
			}

			/* Grid will take me closer */
			else
			{
				/* Don't look this gift horse in the mouth. */
				*ty = y;
				*tx = x;
				return (TRUE);
			}
		}
	}

	/* Target is further away horizontally than vertically */
	else if (dist_x > dist_y)
	{
		/* Find out if the target is right of the monster */
		if (m_ptr->target_x - m_ptr->fx > 0) right = TRUE;

		/* Search adjacent grids */
		for (i = 0; i < 8; i++)
		{
			y = m_ptr->fy + ddy_ddd[i];
			x = m_ptr->fx + ddx_ddd[i];

			/* Check bounds */
			if (!in_bounds_fully(y, x)) continue;

			/* Grid is not passable */
			if (!cave_passable_mon(m_ptr, y, x, &dummy)) continue;

			/* Grid will take me further away */
			if ((( right) && (x < m_ptr->fx)) ||
			    ((!right) && (x > m_ptr->fx)))
			{
				continue;
			}

			/* Grid will not take me closer or further */
			else if (x == m_ptr->fx)
			{
				/* See if it leads to better things */
				for (j = 0; j < 8; j++)
				{
					yy = y + ddy_ddd[j];
					xx = x + ddx_ddd[j];

					/* Grid does lead to better things */
					if ((( right) && (xx > m_ptr->fx)) ||
					    ((!right) && (xx < m_ptr->fx)))
					{
						/* But it is not passable */
						if (!cave_passable_mon(m_ptr, yy, xx, &dummy)) continue;

						/* Accept (original) grid, but don't immediately claim success */
						tar_y = y;
						tar_x = x;
					}
				}
			}

			/* Grid will take me closer */
			else
			{
				/* Don't look this gift horse in the mouth. */
				*ty = y;
				*tx = x;
				return (TRUE);
			}
		}
	}

	/* Target is the same distance away along both axes. */
	else
	{
		/* XXX XXX - code something later to fill this hole. */
		return (FALSE);
	}

	/* If we found a solution, claim success */
	if ((tar_y) && (tar_x))
	{
		*ty = tar_y;
		*tx = tar_x;
		return (TRUE);
	}

	/* No luck */
	return (FALSE);
}


/*
 * Confused monsters bang into walls and doors, and wander into lava or
 * water.  This function assumes that the monster does not belong in this
 * grid, and therefore should suffer for trying to enter it.
 */
static void make_confused_move(monster_type *m_ptr, int y, int x)
{
	char m_name[80];

	int feat;

	monster_race *r_ptr;

	bool seen = FALSE;

	bool confused = m_ptr->confused;

	r_ptr = &r_info[m_ptr->r_idx];
    
	/* Check Bounds (fully) */
	if (!in_bounds_fully(y, x)) return;

	/* Check location */
	feat = cave_feat[y][x];

	/* Check visibility */
	if ((m_ptr->ml) && (cave_info[y][x] & (CAVE_SEEN))) seen = TRUE;

	/* Get the monster name/poss */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    // Feature is a chasm
    if (cave_feat[y][x] == FEAT_CHASM)
    {
       // The creature can't fly and the grid is empty
        if (!(r_ptr->flags2 & (RF2_FLYING)) && (cave_m_idx[y][x] < 0))
        {
            monster_swap(m_ptr->fy, m_ptr->fx, y, x);
        }
    }
        
	/* Feature is a wall */
	else if (cave_info[y][x] & (CAVE_WALL))
	{
		/* Feature is a (known) door */
		if (cave_known_closed_door_bold(y,x))
		{
			if (seen && confused)
				msg_format("%^s staggers into a door.", m_name);
		}

		/* Rubble */
		else if (feat == FEAT_RUBBLE)
		{
			if (seen && confused)
				msg_format("%^s staggers into some rubble.", m_name);
		}

		/* Otherwise, we assume that the feature is a "wall".  XXX  */
		else
		{
			if (seen && confused)
				msg_format("%^s bashes into a wall.", m_name);
		}

		/*possibly update the monster health bar*/
		if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
					p_ptr->redraw |= (PR_HEALTHBAR);
	}

	/* Feature is not a wall */
	else
	{
		/* No changes */
	}

}


/*
 * Given a target grid, calculate the grid the monster will actually
 * attempt to move into.
 *
 * The simplest case is when the target grid is adjacent to us and
 * able to be entered easily.  Usually, however, one or both of these
 * conditions don't hold, and we must pick an initial direction, than
 * look at several directions to find that most likely to be the best
 * choice.  If so, the monster needs to know the order in which to try
 * other directions on either side.  If there is no good logical reason
 * to prioritize one side over the other, the monster will act on the
 * "spur of the moment", using current turn as a randomizer.
 *
 * The monster then attempts to move into the grid.  If it fails, this
 * function returns FALSE and the monster ends its turn.
 *
 * The variable "fear" is used to invoke any special rules for monsters
 * wanting to retreat rather than advance.  For example, such monsters
 * will not leave an non-viewable grid for a viewable one and will try
 * to avoid the character.
 *
 * The variable "bash" remembers whether a monster had to bash a door
 * or not.  This has to be remembered because the choice to bash is
 * made in a different function than the actual bash move.  XXX XXX  If
 * the number of such variables becomes greater, a structure to hold them
 * would look better than passing them around from function to function.
 */
static bool make_move(monster_type *m_ptr, int *ty, int *tx, bool fear,
	bool *bash)
{
	int i, j;
	
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	/* Start direction, current direction */
	int dir0, dir;

	/* Deltas, absolute axis distances from monster to target grid */
	int dy, ay, dx, ax;

	/* Existing monster location, proposed new location */
	int oy, ox, ny, nx;
    
	bool avoid = FALSE;
	bool passable = FALSE;
	bool look_again = FALSE;

	int chance;

	/* Remember where monster is */
	oy = m_ptr->fy;
	ox = m_ptr->fx;

	/* Get the change in position needed to get to the target */
	dy = *ty - oy;
	dx = *tx - ox;

	/* Calculate vertical and horizontal distances */
	ay = ABS(dy);
	ax = ABS(dx);

	/* We mostly want to move vertically */
	if (ay > (ax * 2))
	{
		/* Choose between directions '8' and '2' */
		if (dy < 0)
		{
			/* We're heading up */
			dir0 = 8;
			if ((dx < 0) || (dx == 0 && (turn % 2 == 0))) dir0 += 10;
		}
		else
		{
			/* We're heading down */
			dir0 = 2;
			if ((dx > 0) || (dx == 0 && (turn % 2 == 0))) dir0 += 10;
		}
	}

	/* We mostly want to move horizontally */
	else if (ax > (ay * 2))
	{
		/* Choose between directions '4' and '6' */
		if (dx < 0)
		{
			/* We're heading left */
			dir0 = 4;
			if ((dy > 0) || (dy == 0 && (turn % 2 == 0))) dir0 += 10;
		}
		else
		{
			/* We're heading right */
			dir0 = 6;
			if ((dy < 0) || (dy == 0 && (turn % 2 == 0))) dir0 += 10;
		}
	}

	/* We want to move up and sideways */
	else if (dy < 0)
	{
		/* Choose between directions '7' and '9' */
		if (dx < 0)
		{
			/* We're heading up and left */
			dir0 = 7;
			if ((ay < ax) || (ay == ax && (turn % 2 == 0))) dir0 += 10;
		}
		else
		{
			/* We're heading up and right */
			dir0 = 9;
			if ((ay > ax) || (ay == ax && (turn % 2 == 0))) dir0 += 10;
		}
	}

	/* We want to move down and sideways */
	else
	{
		/* Choose between directions '1' and '3' */
		if (dx < 0)
		{
			/* We're heading down and left */
			dir0 = 1;
			if ((ay > ax) || (ay == ax && (turn % 2 == 0))) dir0 += 10;
		}
		else
		{
			/* We're heading down and right */
			dir0 = 3;
			if ((ay < ax) || (ay == ax && (turn % 2 == 0))) dir0 += 10;
		}
	}

	// Sil-y: not sure why this bit is needed, but it seemed to be, so I added it
	
	// If the monster wants to stay still...
	if ((*ty == m_ptr->fy) && (*tx == m_ptr->fx))
	{
		// if it is adjacent to the player, and can attack, then just try that.
		if ((m_ptr->cdis == 1) && !(r_ptr->flags1 & (RF1_NEVER_BLOW)))
		{
			*ty = p_ptr->py;
			*tx = p_ptr->px;
		}
		
		// otherwise just do nothing
		else
		{
			return (FALSE);
		}
	}

	/* Apply monster confusion */
	if ((m_ptr->confused) && (!(r_ptr->flags1 & (RF1_NEVER_MOVE))))
	{
		// undo +10 modifiers
		if (dir0 > 10) dir0 -= 10;

		// gives 3 chances to be turned left and 3 chances to be turned right
		// leads to a binomial distribution of direction around the intended one:
		//
		// 15 20 15
		//  6     6   (chances are all out of 64)
		//  1  0  1
		
		i = damroll(3,2) - damroll(3,2);
		dir0 = cycle[chome[dir0] + i];
	}
		
	/* Is the target grid adjacent to the current monster's position? */
	if ((dy >= -1) && (dy <= 1) && (dx >= -1) && (dx <= 1) && !m_ptr->confused)
	{
		/* If it is, try the shortcut of simply moving into the grid */
		
		/* Get the probability of entering this grid */
		chance = cave_passable_mon(m_ptr, *ty, *tx, bash);
		
		/* Grid must be pretty easy to enter */
		if (chance >= 50)
		{
			/* We can enter this grid */
			if ((chance >= 100) || percent_chance(chance))
			{
				return (TRUE);
			}
			
			/* Failure to enter grid.  Cancel move */
			else
			{
				return (FALSE);
			}
		}
	}
		
	/*
	 * Now that we have an initial direction, we must determine which
	 * grid to actually move into.
	 */
	if (TRUE)
	{
		/* Build a structure to hold movement data */
		typedef struct move_data move_data;
		struct move_data
		{
			int move_chance;
			bool move_bash;
		};
		move_data moves_data[8];

		/*
		 * Scan each of the eight possible directions, in the order of
		 * priority given by the table "side_dirs", choosing the one that
		 * looks like it will get the monster to the character - or away
		 * from him - most effectively.
		 */
		for (i = 0; i <= 8; i++)
		{
			/* Out of options */
			if (i == 8) break;

			/* Get the actual direction */
			dir = side_dirs[dir0][i];

			/* Get the grid in our chosen direction */
			ny = oy + ddy[dir];
			nx = ox + ddx[dir];

			/* Check Bounds */
			if (!in_bounds(ny, nx)) continue;

			/* Store this grid's movement data. */
			moves_data[i].move_chance =
				cave_passable_mon(m_ptr, ny, nx, bash);
			moves_data[i].move_bash = *bash;

			/* Confused monsters must choose the first grid */
			if (m_ptr->confused) break;

			/* If this grid is totally impassable, skip it */
			if (moves_data[i].move_chance == 0) continue;

			/* Frightened monsters work hard not to be seen. */
			if (fear)
			{
				/* Monster is having trouble navigating to its target. */
				if ((m_ptr->target_y) && (m_ptr->target_x) && (i >= 2) &&
				    (distance(m_ptr->fy, m_ptr->fx, m_ptr->target_y, m_ptr->target_x) > 1))
				{
					/* Look for an adjacent grid leading to the target */
					if (get_route_to_target(m_ptr, ty, tx))
					{
						/* Calculate the chance to enter the grid */
						chance = cave_passable_mon(m_ptr, *ty, *tx, bash);

						/* Try to move into the grid */
						if (!percent_chance(chance))
						{
							/* Can't move */
							return (FALSE);
						}

						/* Can move */
						return (TRUE);
					}

					/* No good route found */
					else if (i >= 3)
					{
						/*
						 * We can't get to our hiding place.  We're in line of fire.
						 * The only thing left to do is go down fighting.  XXX XXX
						 */
                        if ((m_ptr->ml) && (player_can_fire_bold(oy, ox)) && !p_ptr->truce && (r_ptr->freq_ranged < 50))
                        {
                            /* Message if visible */
							if (m_ptr->ml)
							{
								char m_name[80];
								 
								/* Get the monster name */
								monster_desc(m_name, sizeof(m_name), m_ptr, 0);
								 
								/* Dump a message */
								msg_format("%^s panics.", m_name);
							}
							
                            // boost morale and make the monster aggressive
                            m_ptr->tmp_morale = MAX(m_ptr->tmp_morale + 60, 60);
                            calc_morale(m_ptr);
                            calc_stance(m_ptr);
                            m_ptr->mflag |= (MFLAG_AGGRESSIVE);
						}
					}
				}

				/* Attacking the character as a first choice? */
				if ((i == 0) && (ny == p_ptr->py) && (nx == p_ptr->px))
				{
					/* Need to rethink some plans XXX XXX XXX */
					m_ptr->target_y = 0;
					m_ptr->target_x = 0;
				}

				/* Monster is visible */
				if (m_ptr->ml)
				{
					/* And is in LOS */
					if (player_has_los_bold(oy, ox))
					{
						/* Accept any easily passable grid out of LOS */
						if ((!player_has_los_bold(ny, nx)) &&
							(moves_data[i].move_chance > 40))
						{
							break;
						}
					}
					else
					{
						/* Do not enter a grid in LOS */
						if (player_has_los_bold(ny, nx))
						{
							moves_data[i].move_chance = 0;
							continue;
						}
					}
				}

				/* Monster can't be seen, and is not in a "seen" grid. */
				if ((!m_ptr->ml) && (!player_can_see_bold(oy, ox)))
				{
					/* Do not enter a "seen" grid */
					if (player_can_see_bold(ny, nx))
					{
						moves_data[i].move_chance = 0;
						continue;
					}
				}
			}

			/* XXX XXX -- Sometimes attempt to break glyphs. */
			if ((cave_feat[ny][nx] == FEAT_GLYPH) && (!fear) &&
			    (one_in_(5)))
			{
				break;
			}
			
			/* Initial direction is almost certainly the best one */
			if ((i == 0) && (moves_data[i].move_chance >= 80)) 
			{
				/*
				 * If backing away and close, try not to walk next
				 * to the character, or get stuck fighting him.
				 */
				if ((fear) && (m_ptr->cdis <= 2) &&
					(distance(p_ptr->py, p_ptr->px, ny, nx) <= 1))
				{
					avoid = TRUE;
				}

				else break;
			}

			/* Either of the first two side directions looks good */
			else if (((i == 1) || (i == 2)) &&
			         (moves_data[i].move_chance >= 50))
			{
				/* Accept the central direction if at least as good */
				if ((moves_data[0].move_chance >=
				     moves_data[i].move_chance))
				{
					if (avoid)
					{
						/* Frightened monsters try to avoid the character */
						if (distance(p_ptr->py, p_ptr->px, ny, nx) == 0)
						{
							i = 0;
						}
					}
					else
					{
						i = 0;
					}
				}

				/* Accept this direction */
				break;
			}

			/* This is the first passable direction */
			if (!passable)
			{
				/* Note passable */
				passable = TRUE;

				/* All the best directions are blocked. */
				if (i >= 3)
				{
					/* Settle for "good enough" */
					break;
				}
			}

			/* We haven't made a decision yet; look again. */
			if (i == 7) look_again = TRUE;
		}

		/* We've exhausted all the easy answers. */
		if (look_again)
		{
			/* There are no passable directions. */
			if (!passable)
			{
				return (FALSE);
			}

			/* We can move. */
			for (j = 0; j < 8; j++)
			{
				/* Accept the first option, however poor.  XXX */
				if (moves_data[j].move_chance)
				{
					i = j;
					break;
				}
			}
		}

		/* If no direction was acceptable, end turn */
		if (i >= 8)
		{
			return (FALSE);
		}

		/* Get movement information (again) */
		dir = side_dirs[dir0][i];
		*bash = moves_data[i].move_bash;

		/* No good moves, so we just sit still and wait. */
		if ((dir == 5) || (dir == 0))
		{
			return (FALSE);
		}

		/* Get grid to move into */
		*ty = oy + ddy[dir];
		*tx = ox + ddx[dir];

		/*
		 * Amusing messages and effects for confused monsters trying
		 * to enter terrain forbidden to them.
		 */
		if ((m_ptr->confused) && (moves_data[i].move_chance <= 25))
		{
			/* Sometimes hurt the poor little critter */
			make_confused_move(m_ptr, *ty, *tx);

			/* Do not actually move */
			if (!moves_data[i].move_chance) return (FALSE);
		}

		/* Try to move in the chosen direction.  If we fail, end turn. */
		if ((moves_data[i].move_chance < 100) &&
		    !percent_chance(moves_data[i].move_chance))
		{
			return (FALSE);
		}
	}

	/* Monster is frightened, and is obliged to fight. */
	if ((fear) && (cave_m_idx[*ty][*tx] < 0) && !p_ptr->truce)
	{
		/* Message if visible */
		if (m_ptr->ml)
		{
			char m_name[80];
			
			/* Get the monster name */
			monster_desc(m_name, sizeof(m_name), m_ptr, 0);
			
			/* Dump a message */
			msg_format("%^s panics.", m_name);
		}
		
		// boost morale and make the monster aggressive
		m_ptr->tmp_morale = MAX(m_ptr->tmp_morale + 60, 60);
		calc_morale(m_ptr);
		calc_stance(m_ptr);
		m_ptr->mflag |= (MFLAG_AGGRESSIVE);
	}

	/* We can move. */
	return (TRUE);
}

/*
 * If one monster moves into another monster's grid, they will
 * normally swap places.  If the second monster cannot exist in the
 * grid the first monster left, this can't happen.  In such cases,
 * the first monster tries to push the second out of the way.
 */
static bool push_aside(monster_type *m_ptr, monster_type *n_ptr)
{
	/* Get racial information about the second monster */
	monster_race *nr_ptr = &r_info[n_ptr->r_idx];

	int y, x, i;
	int dir = 0;

	/*
	 * Translate the difference between the locations of the two
	 * monsters into a direction of travel.
	 */
	for (i = 0; i < 10; i++)
	{
		/* Require correct difference along the y-axis */
		if ((n_ptr->fy - m_ptr->fy) != ddy[i]) continue;

		/* Require correct difference along the x-axis */
		if ((n_ptr->fx - m_ptr->fx) != ddx[i]) continue;

		/* Found the direction */
		dir = i;
		break;
	}

	/* Favor either the left or right side on the "spur of the moment". */
	if (one_in_(2)) dir += 10;

	/* Check all directions radiating out from the initial direction. */
	for (i = 0; i < 7; i++)
	{
		int side_dir = side_dirs[dir][i];

		y = n_ptr->fy + ddy[side_dir];
		x = n_ptr->fx + ddx[side_dir];

		/* Illegal grid */
		if (!in_bounds_fully(y, x)) continue;

		/* Grid is not occupied, and the 2nd monster can exist in it. */
		if (cave_exist_mon(nr_ptr, y, x, FALSE, TRUE))
		{
			/* Push the 2nd monster into the empty grid. */
			monster_swap(n_ptr->fy, n_ptr->fx, y, x);
			return (TRUE);
		}
	}

	/* We didn't find any empty, legal grids */
	return (FALSE);
}



static void warning_message(monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	char c = r_ptr->d_char;
	char m_name[80];
	
	/* Get the monster name */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);
	
	if (strchr("o@Gp",c))
	{
		if (singing(SNG_SILENCE))
		{
			if (m_ptr->ml)  msg_format("%^s shouts a muffled warning.", m_name);
			else            msg_print("You hear a muffled warning shout.");
		}
		else
		{
			if (m_ptr->ml)  msg_format("%^s shouts a warning.", m_name);
			else            msg_print("You hear a warning shout.");
		}
	}
	else if (strchr("dDsS",c))
	{
		if (singing(SNG_SILENCE))
		{
			if (m_ptr->ml)  msg_format("%^s lets out a muffled roar.", m_name);
			else            msg_print("You hear a muffled roar.");
		}
		else
		{
			if (m_ptr->ml)  msg_format("%^s roars in anger.", m_name);
			else            msg_print("You hear a loud roar.");
		}
	}
	else if (strchr("T",c))
	{
		if (singing(SNG_SILENCE))
		{
			if (m_ptr->ml)  msg_format("%^s lets out a muffled grunt.", m_name);
			else            msg_print("You hear a muffled grunt.");
		}
		else
		{
			if (m_ptr->ml)  msg_format("%^s grunts in anger.", m_name);
			else            msg_print("You hear a loud grunt.");
		}
	}
	else if (strchr("C",c))
	{
		if (singing(SNG_SILENCE))
		{
			if (m_ptr->ml)  msg_format("%^s makes a muffled howl.", m_name);
			else            msg_print("You hear a muffled howl.");
		}
		else
		{
			if (m_ptr->ml)  msg_format("%^s makes a low howl.", m_name);
			else            msg_print("You hear a low howl.");
		}
	}
	else
	{
		if (singing(SNG_SILENCE))
		{
			if (m_ptr->ml)  msg_format("%^s cries out a muffled warning.", m_name);
			else            msg_print("You hear something cry out a muffled warning.");
		}
		else
		{
			if (m_ptr->ml)  msg_format("%^s cries out a warning.", m_name);
			else            msg_print("You hear something cry out a warning.");
		}
	}
	
	disturb(1, 0);
	
	/* Hard not to notice */
	update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
	monster_perception(FALSE, FALSE, -10);

	// makes monster noise too
	m_ptr->noise += 10;
}

static void pursuit_message(monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	char c = r_ptr->d_char;
	char m_name[80];
	int dist = distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx);
	
	/* Get the monster name */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);
	
	if (strchr("o@Gp",c))
	{
		if (m_ptr->ml)        msg_format("%^s shouts excitedly.", m_name);
		else if (dist < 20)   msg_print("You hear an shout.");
		else                  msg_print("You hear a distant shout.");
	}
	else if (strchr("dDsS",c))
	{
		if (m_ptr->ml)        msg_format("%^s roars.", m_name);
		else if (dist < 20)   msg_print("You hear a loud roar.");
		else                  msg_print("You hear a distant roar.");
	}
	else if (strchr("T",c))
	{
		if (m_ptr->ml)        msg_format("%^s grunts.", m_name);
		else if (dist < 20)   msg_print("You hear a low grunt.");
		else                  msg_print("You hear a distant grunt.");
	}
	else if (strchr("C",c))
	{
		if (m_ptr->ml)        msg_format("%^s makes a low howl.", m_name);
		else if (dist < 20)   msg_print("You hear a low howl.");
		else                  msg_print("You hear a distant howl.");
	}
	else if (strchr("f",c))
	{
		if (m_ptr->ml)        msg_format("%^s makes a yowling sound.", m_name);
		else if (dist < 20)   msg_print("You hear yowling nearby.");
		else                  msg_print("You hear distant yowling.");
	}
	else
	{
		if (m_ptr->ml)        msg_format("%^s roars in anger.", m_name);
		else if (dist < 20)   msg_print("You hear a loud roar.");
		else                  msg_print("You hear a distant roar.");
	}

}

/*
 * Deal with the monster Ability: exchange places
 */
void monster_exchange_places(monster_type *m_ptr)
{
    monster_lore *l_ptr = &l_list[m_ptr->r_idx];
    char m_name1[80];
    char m_name2[80];
    char m_name3[80];
    
    int y = m_ptr->fy;
    int x = m_ptr->fx;
    
    monster_desc(m_name1, sizeof(m_name1), m_ptr, 0);
    monster_desc(m_name2, sizeof(m_name2), m_ptr, 0x21);
    monster_desc(m_name3, sizeof(m_name3), m_ptr, 0x20);
    
    /* Message */
    msg_format("%^s exchanges places with you.", m_name1);
    
    // swap positions with the player
    monster_swap(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px);
    
    // update some things
    update_view();

    // attack of opportunity
    if (!p_ptr->afraid && !p_ptr->entranced && (p_ptr->stun <= 100))
    {
        // this might be the most complicated auto-grammatical message in the game...
        msg_format("You attack %s as %s slips past.", m_name2, m_name3);
        py_attack_aux(m_ptr->fy, m_ptr->fx, ATT_OPPORTUNITY);
    }
    
    // remember that the monster can do this
    if (m_ptr->ml)  l_ptr->flags2 |= (RF2_EXCHANGE_PLACES);
    
    /* Set off traps */
    if (cave_trap_bold(y,x) || (cave_feat[y][x] == FEAT_CHASM))
    {
        // If it is hidden
        if (cave_info[y][x] & (CAVE_HIDDEN))
        {
            /* Reveal the trap */
            reveal_trap(y, x);
        }
        
        /* Hit the trap */
        hit_trap(y, x);
    }
}


/*
 * Process a monster's move.
 *
 * All the plotting and planning has been done, and all this function
 * has to do is move the monster into the chosen grid.
 *
 * This may involve attacking the character, breaking a glyph of
 * warding, bashing down a door, etc..  Once in the grid, monsters may
 * stumble into monster traps, hit a scent trail, pick up or destroy
 * objects, and so forth.
 *
 * A monster's move may disturb the character, depending on which
 * disturbance options are set.
 */
static void process_move(monster_type *m_ptr, int ty, int tx, bool bash)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];

	int feat;

	/* Existing monster location, proposed new location */
	int oy, ox, ny, nx;

	/* Default move, default lack of view */
	bool do_move = TRUE;
	bool do_view = FALSE;

	/* Assume nothing */
    bool did_swap = FALSE;
	bool did_pass_door = FALSE;
	bool did_open_door = FALSE;
	bool did_unlock_door = FALSE;
	bool did_bash_door = FALSE;
	bool did_take_item = FALSE;
	bool did_kill_item = FALSE;
	bool did_kill_body = FALSE;
	bool did_pass_wall = FALSE;
	bool did_kill_wall = FALSE;
	bool did_tunnel_wall = FALSE;

	/* Remember where monster is */
	oy = m_ptr->fy;
	ox = m_ptr->fx;

	/* Get the destination */
	ny = ty;
	nx = tx;

	/* Check Bounds */
	if (!in_bounds(ny, nx)) return;

	/*
	 * Some monsters will not move into sight of the player.
	 */
	if ((r_ptr->flags1 & (RF1_HIDDEN_MOVE)) && 
	    ((cave_info[ty][tx] & (CAVE_SEEN)) || seen_by_keen_senses(ty,tx)))
	{
		/* Hack -- memorize lack of moves after a while. */
		if (!(l_ptr->flags1 & (RF1_HIDDEN_MOVE)))
		{
			if (m_ptr->ml && (one_in_(50)))
			{
				l_ptr->flags1 |= (RF1_HIDDEN_MOVE);
			}
		}
		/* If we are in sight, do not move */
		return;		
	}
	
	/* The grid is occupied by the player. */
	if (cave_m_idx[ny][nx] < 0)
	{
		// unalert monsters notice the player instead of attacking
		if (m_ptr->alertness < ALERTNESS_ALERT)
		{
			set_alertness(m_ptr, rand_range(ALERTNESS_ALERT, ALERTNESS_ALERT + 5));
			
			// Hack: we must unset this flag to avoid the monster missing *2* turns
			m_ptr->skip_next_turn = FALSE;
		}
		
		// Otherwise attack if possible
		else if (!p_ptr->truce && !(r_ptr->flags1 & (RF1_NEVER_BLOW)))
		{
            if (r_ptr->flags2 & (RF2_EXCHANGE_PLACES) && one_in_(4) &&
                (adj_mon_count(m_ptr->fy, m_ptr->fx) >= adj_mon_count(p_ptr->py, p_ptr->px)))
            {
                monster_exchange_places(m_ptr);
            }
            else
            {
                (void)make_attack_normal(m_ptr);
            }
		}

		/* End move */
		do_move = FALSE;
	}

	/* Can still move */
	if (do_move)
	{
		/* Get the feature in the grid that the monster is trying to enter. */
		feat = cave_feat[ny][nx];

		/* Entering a wall */
		if (cave_info[ny][nx] & (CAVE_WALL))
		{
			/* Monster passes through walls (and doors) */
			if (r_ptr->flags2 & (RF2_PASS_WALL))
			{
				/* Monster went through a wall */
				did_pass_wall = TRUE;
			}

			/* Monster destroys walls (and doors) */
			else if (r_ptr->flags2 & (RF2_KILL_WALL))
			{
				bool msg = FALSE;

				/* Noise distance depends on monster "dangerousness"  XXX */
				int noise_dist = 10;

				/* Forget the wall */
				cave_info[ny][nx] &= ~(CAVE_MARK);

				/* Note that the monster killed the wall */
				if (player_can_see_bold(ny, nx))
				{
					do_view = TRUE;
					did_kill_wall = TRUE;
				}

				/* Output warning messages if the racket gets too loud */
				else if (m_ptr->cdis <= noise_dist)
				{
					msg = TRUE;
				}

				/* Grid is currently a door */
				if (cave_any_closed_door_bold(ny, nx))
				{
					cave_set_feat(ny, nx, FEAT_BROKEN);

					if (msg)
					{
						// disturb the player
						disturb(0, 0);
						msg_print("You hear a door being smashed open.");
					}
	
					// monster noise
					m_ptr->noise += 10;
				}

				/* Grid is anything else */
				else
				{
					cave_set_feat(ny, nx, FEAT_FLOOR);
					
					if (msg)
					{
						// disturb the player
						disturb(0, 0);
						msg_print("You hear grinding noises.");
					}

					// monster noise
					m_ptr->noise += 15;
				}
			}

            /* Monster tunnels walls */
			else if ((r_ptr->flags2 & (RF2_TUNNEL_WALL)) && !cave_any_closed_door_bold(ny,nx))
			{
				bool msg = FALSE;
                
				/* Noise distance depends on monster "dangerousness"  XXX */
				int noise_dist = 10;
                
				/* Forget the wall */
				cave_info[ny][nx] &= ~(CAVE_MARK);
                
                /* Do not move */
                do_move = FALSE;
                
				/* Note that the monster killed the wall */
				if (player_can_see_bold(ny, nx))
				{
					do_view = TRUE;
					did_kill_wall = TRUE;
				}
                
				/* Output warning messages if the racket gets too loud */
				else if (m_ptr->cdis <= noise_dist)
				{
					msg = TRUE;
				}
                
				/* Grid is currently rubble */
				if (cave_feat[ny][nx] == FEAT_RUBBLE)
				{
					cave_set_feat(ny, nx, FEAT_FLOOR);
                    
					if (msg)
					{
						// disturb the player
						disturb(0, 0);
						msg_print("You hear grinding noises.");
					}
                    
					// monster noise
					m_ptr->noise += 15;
				}
                
				/* Grid is granite or quartz */
				else
				{
					cave_set_feat(ny, nx, FEAT_RUBBLE);
					
					if (msg)
					{
						// disturb the player
						disturb(0, 0);
						msg_print("You hear grinding noises.");
					}
                    
					// monster noise
					m_ptr->noise += 15;
				}
			}

			/* Doors */
			else if (cave_any_closed_door_bold(ny, nx))
			{
				/* Monster passes through doors */
				if (r_ptr->flags2 & (RF2_PASS_DOOR))
				{
					/* Monster went through a door */
					did_pass_door = TRUE;
				}

				/* Monster bashes the door down */
				else if (bash)
				{
					/* Handle doors in sight */
					if (player_can_see_bold(ny, nx))
					{
						/* Always disturb */
						disturb(0, 0);

						msg_print("The door bursts open!");
						
						do_view = TRUE;
					}
					/* Character is not too far away */
					else if (m_ptr->cdis < 20)
					{
						// disturb the player
						disturb(0, 0);

						/* Message */
						msg_print("You hear a door burst open!");
					}

					/* Note that the monster bashed the door (if visible) */
					did_bash_door = TRUE;

					// monster noise
					m_ptr->noise += 10;

					/* Break down the door */
					if (one_in_(2)) cave_set_feat(ny, nx, FEAT_BROKEN);
					else cave_set_feat(ny, nx, FEAT_OPEN);
				}

				/* Monster opens the door */
				else
				{
					/* Locked doors */
					if (cave_feat[ny][nx] != FEAT_DOOR_HEAD + 0x00)
					{
						/* Note that the monster unlocked the door (if visible) */
						did_unlock_door = TRUE;

						/* Unlock the door */
						cave_set_feat(ny, nx, FEAT_DOOR_HEAD + 0x00);

						/* Do not move */
						do_move = FALSE;

						/* Handle doors in sight */
						if (player_has_los_bold(ny, nx))
						{
							msg_print("You hear a 'click'.");
						}
					}

					/* Ordinary doors */
					else
					{
						/* Note that the monster opened the door (if visible) */
						did_open_door = TRUE;

						/* Open the door */
						cave_set_feat(ny, nx, FEAT_OPEN);

						/* Step into doorway sometimes */
						if (!one_in_(5)) do_move = FALSE;
					}

					/* Handle doors in sight */
					if (player_can_see_bold(ny, nx))
					{
						/* Do not disturb automatically */

						do_view = TRUE;
					}
				}
			}

			/* Paranoia -- Ignore all features not added to this code */
			else return;
		}



		/* Glyphs */
		else if (cave_feat[ny][nx] == FEAT_GLYPH)
		{
			/* Describe observable breakage */
			if (cave_info[ny][nx] & (CAVE_MARK))
			{
				msg_print("The glyph of warding is broken!");
			}

			/* Forget the rune */
			cave_info[ny][nx] &= ~(CAVE_MARK);

			/* Break the rune */
			cave_set_feat(ny, nx, FEAT_FLOOR);
		}
	}

	/* Monster is allowed to move */
	if (do_move)
	{
		/* The grid is occupied by a monster. */
		if (cave_m_idx[ny][nx] > 0)
		{
			monster_type *n_ptr = &mon_list[cave_m_idx[ny][nx]];
			monster_race *nr_ptr = &r_info[n_ptr->r_idx];

			/* XXX - Kill (much) weaker monsters */
			if ((r_ptr->flags2 & (RF2_KILL_BODY)) &&
			    (!(nr_ptr->flags1 & (RF1_UNIQUE))) &&
			    (r_ptr->level > nr_ptr->level * 2))
			{
				/* Note that the monster killed another monster (if visible) */
				did_kill_body = TRUE;

				/* Kill the monster */
				delete_monster(ny, nx);
			}

			/* Swap with or push aside the other monster */
			else
			{
                did_swap = TRUE;
                
				/* If other monster can't move, then abort the swap */
				
				// Sil-y: should no longer need this
				
				//if (nr_ptr->flags1 & (RF1_NEVER_MOVE))
				//{
				//	/* Cancel move */
				//	do_move = FALSE;
				//}
				
				/* The other monster cannot switch places */
				if (!cave_exist_mon(nr_ptr, m_ptr->fy, m_ptr->fx, TRUE, TRUE))
				{
					/* Try to push it aside */
					if (!push_aside(m_ptr, n_ptr))
					{
						/* Cancel move on failure */
						do_move = FALSE;
					}
				}
			}
		}
	}


	/* Monster can (still) move */
	if (do_move)
	{
        // deal with possible flanking attack
        if ((r_ptr->flags2 & (RF2_FLANKING)) &&
            (distance(oy, ox, p_ptr->py, p_ptr->px) == 1) && (distance(ny, nx, p_ptr->py, p_ptr->px) == 1) &&
            (m_ptr->alertness >= ALERTNESS_ALERT) && (m_ptr->stance != STANCE_FLEEING) && !m_ptr->confused && !did_swap)
        {
            char m_name[80];
            
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);
            
            msg_format("%^s attacks you as it moves by.", m_name);
            make_attack_normal(m_ptr);
            
            // remember that the monster can do this
            if (m_ptr->ml)  l_ptr->flags2 |= (RF2_FLANKING);
        }

		/* Move the monster */
		monster_swap(oy, ox, ny, nx);
        
		/* Cancel target when reached */
		if ((m_ptr->target_y == ny) && (m_ptr->target_x == nx))
		{
			m_ptr->target_y = 0;
			m_ptr->target_x = 0;
		}

		/*Did a new monster get pushed into the old space?*/
		if (cave_m_idx[oy][ox] > 0)
		{
			monster_type *n_ptr;
			monster_type monster_type_body;
			
			/* Get local monster */
			n_ptr = &monster_type_body;
			
			n_ptr = &mon_list[cave_m_idx[oy][ox]];
			
			n_ptr->mflag |= (MFLAG_PUSHED);

            // exchanging places doesn't count as movement in a direction for abilities
        }
        
        // if it moved into a free space
        else
        {
            // record the direction of travel (for charge attacks)
            m_ptr->previous_action[0] = rough_direction(oy, ox, ny, nx);
        }

		/*
		 * If a member of a monster group capable of smelling hits a
		 * scent trail while out of LOS of the character, it will
		 * communicate this to similar monsters.
		 */
		if ((!player_has_los_bold(ny, nx)) && (r_ptr->flags1 & (RF1_FRIENDS)) &&
		    (monster_can_smell(m_ptr)) && (get_scent(oy, ox) == -1) &&
		    (!m_ptr->target_y) && (!m_ptr->target_x))
		{
			int i;
			monster_type *n_ptr;
			monster_race *nr_ptr;
			bool alerted_others = FALSE;

			/* Scan all other monsters */
			for (i = mon_max - 1; i >= 1; i--)
			{
				/* Access the monster */
				n_ptr = &mon_list[i];
				nr_ptr = &r_info[n_ptr->r_idx];

				/* Ignore dead monsters */
				if (!n_ptr->r_idx) continue;

				/* Ignore monsters with the wrong symbol */
				if (r_ptr->d_char != nr_ptr->d_char) continue;

				/* Ignore monsters with specific orders */
				if ((n_ptr->target_x) || (n_ptr->target_y)) continue;

				/* Ignore monsters picking up a good scent */
				if (get_scent(n_ptr->fy, n_ptr->fx) < SMELL_STRENGTH - 10)
					continue;

				/* Ignore monsters not in LOS */
				if (!los(m_ptr->fy, m_ptr->fx, n_ptr->fy, n_ptr->fx))
					continue;

				/* Activate all other monsters and give directions */
				make_alert(m_ptr);
				n_ptr->mflag |= (MFLAG_ACTV);
				n_ptr->target_y = ny;   n_ptr->target_x = nx;
				
				alerted_others = TRUE;
			}
			
			if (alerted_others)
			{ 
				pursuit_message(m_ptr);
			}
		}

		/* Monster is visible and not cloaked */
		if (m_ptr->ml)
		{
			// report passing through doors
			if (cave_any_closed_door_bold(m_ptr->fy, m_ptr->fx))
			{
				/* Monster passes through doors */
				if (r_ptr->flags2 & (RF2_PASS_DOOR))
				{
					char m_name[80];
					
					/* Always disturb */
					disturb(0, 0);
					
					/* Get the monster name */
					monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);
					
					/* Dump a message */
					msg_format("%^s passes under the door.", m_name);
				}
			}
				
			/* Player will always be disturbed if monster moves adjacent */
			if (m_ptr->cdis == 1)
			{
				disturb(0, 0);
			}

			/* be disturbed by all other monster movement */
			else
			{
				disturb(0, 0);
			}

		}

		/* Take or kill objects on the floor */
		if ((r_ptr->flags2 & (RF2_TAKE_ITEM)) ||
			 (r_ptr->flags2 & (RF2_KILL_ITEM)))
		{
			u32b f1, f2, f3;

			u32b flg3 = 0L;

			char m_name[80];
			char o_name[120];

			s16b this_o_idx, next_o_idx = 0;


			/* Scan all objects in the grid */
			for (this_o_idx = cave_o_idx[ny][nx]; this_o_idx;
			     this_o_idx = next_o_idx)
			{
				object_type *o_ptr;

				/* Get the object */
				o_ptr = &o_list[this_o_idx];

				/* Get the next object */
				next_o_idx = o_ptr->next_o_idx;

				/* Extract some flags */
				object_flags(o_ptr, &f1, &f2, &f3);

				/* React to objects that hurt the monster */
				if (f1 & (TR1_SLAY_DRAGON))  flg3 |= (RF3_DRAGON);
				if (f1 & (TR1_SLAY_TROLL))   flg3 |= (RF3_TROLL);
				if (f1 & (TR1_SLAY_ORC))     flg3 |= (RF3_ORC);
				if (f1 & (TR1_SLAY_RAUKO))   flg3 |= (RF3_RAUKO);
				if (f1 & (TR1_SLAY_UNDEAD))  flg3 |= (RF3_UNDEAD);
				if (f1 & (TR1_SLAY_WOLF))    flg3 |= (RF3_WOLF);
				if (f1 & (TR1_SLAY_SPIDER))  flg3 |= (RF3_SPIDER);

				/* Don't pick up cursed items */
				if ((r_ptr->flags2 & (RF2_TAKE_ITEM)) && (cursed_p(o_ptr) || broken_p(o_ptr)))
				{
					/* Describe observable situations */
					if (m_ptr->ml && player_has_los_bold(ny, nx))
					{
						/* Get the object name */
						object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
			
						/* Get the monster name */
						monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);
			
						/* Dump a message */
						msg_format("%^s looks at %s, but moves on.", m_name, o_name);
					}
				}
				
					
				/* The object cannot be picked up by the monster */
				//else if (artefact_p(o_ptr) || (f3 & flg3))
				//{
				//	/* Only give a message for "take_item" */
				//	if (r_ptr->flags2 & (RF2_TAKE_ITEM))
				//	{
				//		/* Take note */
				//		did_take_item = TRUE;

				//		/* Describe observable situations */
				//		if (m_ptr->ml && player_has_los_bold(ny, nx))
				//		{
				//			/* Get the object name */
				//			object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

				//			/* Get the monster name */
				//			monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

				//			/* Dump a message */
				//			msg_format("%^s tries to pick up %s, but fails.",
				//				   m_name, o_name);
				//		}
				//	}
				//}

				/* Pick up the item */
				else if (r_ptr->flags2 & (RF2_TAKE_ITEM))
				{
					object_type *i_ptr;
					object_type object_type_body;

					/* Take note */
					did_take_item = TRUE;

					/* Describe observable situations */
					if (player_has_los_bold(ny, nx) && m_ptr->ml)
					{
						/* Get the object name */
						object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

						/* Get the monster name */
						monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

						/* Dump a message */
						msg_format("%^s picks up %s.", m_name, o_name);
					}

					/* Get local object */
					i_ptr = &object_type_body;

					/* Obtain local object */
					object_copy(i_ptr, o_ptr);

					/* Delete the object */
					delete_object_idx(this_o_idx);

					/* Carry the object */
					(void)monster_carry(cave_m_idx[m_ptr->fy][m_ptr->fx], i_ptr);
				}

				/* Destroy the item */
				else
				{
					/* Take note */
					did_kill_item = TRUE;

					/* Describe observable situations */
					if (player_has_los_bold(ny, nx))
					{
						/* Get the object name */
						object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

						/* Get the monster name */
						monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

						/* Dump a message */
						msg_format("%^s crushes %s.", m_name, o_name);
					}

					/* Delete the object */
					delete_object_idx(this_o_idx);
				}
			}
		}
	}             /* End of monster's move */


	/* Notice changes in view */
	if (do_view)
	{
		/* Update the visuals */
		p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	}

	/* Learn things from observable monster */
	if (m_ptr->ml)
	{
		/* Monster passed through a door */
		if (did_pass_door) l_ptr->flags2 |= (RF2_PASS_DOOR);
		
		/* Monster opened a door */
		if (did_open_door) l_ptr->flags2 |= (RF2_OPEN_DOOR);

		/* Monster unlocked a door */
		if (did_unlock_door) l_ptr->flags2 |= (RF2_UNLOCK_DOOR);

		/* Monster bashed a door */
		if (did_bash_door) l_ptr->flags2 |= (RF2_BASH_DOOR);

		/* Monster tried to pick something up */
		if (did_take_item) l_ptr->flags2 |= (RF2_TAKE_ITEM);

		/* Monster tried to crush something */
		if (did_kill_item) l_ptr->flags2 |= (RF2_KILL_ITEM);

		/* Monster ate another monster */
		if (did_kill_body) l_ptr->flags2 |= (RF2_KILL_BODY);

		/* Monster passed through a wall */
		if (did_pass_wall) l_ptr->flags2 |= (RF2_PASS_WALL);

		/* Monster killed a wall */
		if (did_kill_wall) l_ptr->flags2 |= (RF2_KILL_WALL);
        
		/* Monster tunneled through a wall */
		if (did_tunnel_wall) l_ptr->flags2 |= (RF2_TUNNEL_WALL);
	}
}

/* check whether there are nearby kin who are asleep/unwary */
static bool has_sleeping_kin(monster_type *m_ptr)
{
	int fy = m_ptr->fy;
	int fx = m_ptr->fx;
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	
	int i;
	bool has_kin = FALSE;
		
	/* Scan all other monsters */
	for (i = mon_max - 1; i >= 1; i--)
	{
		/* Access the monster */
		monster_type *n_ptr = &mon_list[i];
		monster_race *nr_ptr = &r_info[n_ptr->r_idx];
				
		/* Access the monster */
		n_ptr = &mon_list[i];
		nr_ptr = &r_info[n_ptr->r_idx];
		
		/* Ignore dead monsters */
		if (!n_ptr->r_idx) continue;
		
		/* Ignore monsters with the wrong symbol */
		if (r_ptr->d_char != nr_ptr->d_char) continue;
		
		// determine the distance between the monsters
		if (!los(fy, fx, n_ptr->fy, n_ptr->fx)) continue;
				
		// Ignore monsters that are awake
		if (n_ptr->alertness >= ALERTNESS_ALERT) continue;
		
		/* Activate all other monsters and communicate to them */
		has_kin = TRUE;
	}
	
	return (has_kin);
}

/*alert others in pack about something using the m_flag, and wake them up*/
void tell_allies(int y, int x, u32b flag)
{
	monster_type *m_ptr;
	monster_race *r_ptr;

	int i;
	
	bool warned = FALSE;
	
	// paranoia
	if (cave_m_idx[y][x] <= 0) return;

	m_ptr = &mon_list[cave_m_idx[y][x]];
	r_ptr = &r_info[m_ptr->r_idx];

	/* Scan all other monsters */
	for (i = mon_max - 1; i >= 1; i--)
	{
		/* Access the monster */
		monster_type *n_ptr = &mon_list[i];
		monster_race *nr_ptr = &r_info[n_ptr->r_idx];
		
		int dist;

		// Access the monster
		n_ptr = &mon_list[i];
		nr_ptr = &r_info[n_ptr->r_idx];

		// Ignore dead monsters
		if (!n_ptr->r_idx) continue;

		// Ignore monsters with the wrong symbol
		if (r_ptr->d_char != nr_ptr->d_char) continue;

		// Ignore monsters that already know
		if ((n_ptr->alertness >= ALERTNESS_ALERT) && (n_ptr->mflag & (flag))) continue;

		// determine the distance between the monsters
		dist = distance(m_ptr->fy, m_ptr->fx, n_ptr->fy, n_ptr->fx);
		
		// penalize this for not being in line of sight
		if (!los(y, x, n_ptr->fy, n_ptr->fx)) dist *= 2;
		
		// Ignore monsters that are too far away
		if (dist > 15) continue;
		
		// When the first monter in need of warning is found, make the warning shout
		if (!warned)
		{
			warning_message(m_ptr);
			warned = TRUE;
		}

		// If an eligible monster is now alert, then set the flag
		if (n_ptr->alertness >= ALERTNESS_ALERT)
		{
			// Set the flag
			n_ptr->mflag |= (MFLAG_ACTV | flag);
		}
	}
}


/*
 * Deal with monsters trying to wander around the dungeon
 */
void wander(monster_type *m_ptr)
{
	int ty, tx;
	bool fear = FALSE;
	bool bash = FALSE;
    monster_race *r_ptr = &r_info[m_ptr->r_idx];
    
    // begin a song of piercing if possible
    // note that Morgoth must be uncrowned
    if ((r_ptr->flags4 & (RF4_SNG_PIERCING)) && (m_ptr->song != SNG_PIERCING) &&
        (m_ptr->alertness < ALERTNESS_ALERT) && (m_ptr->mana >= MON_MANA_COST) && ((&a_info[ART_MORGOTH_3])->cur_num == 1))
    {
        // 96+17 is RF4_SNG_PIERCING
        make_attack_ranged(m_ptr, 96+17);
    }
    
    // occasionally update the flow (keeping the centre the same)
    // to take account of changes in the dungeon (new glyphs of warding, doors closed etc)
    if (one_in_(10))
    {
        // only do this if they have a real wandering index (not a zero due to too many wandering monster groups)
        if (m_ptr->wandering_idx >= FLOW_WANDERING_HEAD)
        {
            update_flow(flow_center_y[m_ptr->wandering_idx], flow_center_x[m_ptr->wandering_idx], m_ptr->wandering_idx);
        }
    }
    
	/* Choose a pair of target grids, or cancel the move. */
	if (!get_move_wander(m_ptr, &ty, &tx))
	{
		return;
	}
	
	/* Calculate the actual move.  Cancel move on failure to enter grid. */
	if (!make_move(m_ptr, &ty, &tx, fear, &bash)) return;
	
	/* Change terrain, move the monster, handle secondary effects. */
	process_move(m_ptr, ty, tx, bash);
}


/*
 * Monster takes its turn.
 */
static void process_monster(monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];
    int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];
    
	int i, k, y, x;
	int ty, tx;
	int chance = 0;
	int choice = 0;
    
	bool fear = FALSE;

	bool bash = FALSE;

	/* Assume the monster doesn't have a target */
	bool must_use_target = FALSE;

	/* Will the monster move randomly? */
	bool random_move = FALSE;
    
    // assume we are not under the influence of the Song of Mastery
    m_ptr->skip_this_turn = FALSE;

	// first work out if the song of mastery stops the monster's turn
	if (singing(SNG_MASTERY))
	{
		if (skill_check(PLAYER,
		                ability_bonus(S_SNG, SNG_MASTERY),
			            monster_skill(m_ptr, S_WIL) + 5 + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
						m_ptr) > 0)
		{
            
            // make sure the monster doesn't do any free attacks before its next turn
            m_ptr->skip_this_turn = TRUE;
			
            // end the monster's turn
            return;
		}
	}

    // deal with monster songs
    if (m_ptr->song != SNG_NOTHING)
    {
        int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
        char m_name[80];
        
        /* Get the monster name */
        monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);
        
        if ((m_ptr->mana == 0) || ((m_ptr->song == SNG_PIERCING) && (m_ptr->alertness >= ALERTNESS_ALERT)))
        {
            if (m_ptr->ml)          msg_format("%^s ends his song.", m_name);
            else if (dist <= 30)    msg_print("The song ends.");
            m_ptr->song = SNG_NOTHING;
        }
        
        else
        {
            m_ptr->mana--;
            
            switch (m_ptr->song)
            {
                case (SNG_BINDING):     song_of_binding(m_ptr);     break;
                case (SNG_PIERCING):    song_of_piercing(m_ptr);    break;
                case (SNG_OATHS):       song_of_oaths(m_ptr);       break;
            }
        }
    }
    
	// need to update view if the monster affects light and is close enough
	if ((r_ptr->light != 0) && (m_ptr->cdis < MAX_SIGHT + ABS(r_ptr->light)))
	{
		p_ptr->update |= (PU_UPDATE_VIEW);
	}

    // shuffle along the array of previous actions
    for (i = ACTION_MAX-1; i > 0; i--)
    {
        m_ptr->previous_action[i] = m_ptr->previous_action[i-1];
    }
    // put in a default for this turn
    m_ptr->previous_action[0] = ACTION_MISC;
    
	// unwary but awake monsters can wander around the dungeon
	if (m_ptr->alertness < ALERTNESS_ALERT)
	{
		wander(m_ptr);
		return;
	}

    // Update monster flow information
    update_flow(p_ptr->py, p_ptr->px, m_idx);
     
	/* Calculate the monster's preferred combat range when needed */
	if (m_ptr->min_range == 0) find_range(m_ptr);


	// determine if the monster should be active:

	// first, reset the active flag
	m_ptr->mflag &= ~(MFLAG_ACTV);
	
	// monsters with targets are all active
	if ((m_ptr->target_y) && (m_ptr->target_x)) m_ptr->mflag |= (MFLAG_ACTV);

	// monsters that are fleeing are active, otherwise they can't get far enough away
	if (m_ptr->stance == STANCE_FLEEING) m_ptr->mflag |= (MFLAG_ACTV);

	// Morgoth is always active during the escape
	// Sil-y: but this might be irrelevant as he can be unwary...
	if ((m_ptr->r_idx == R_IDX_MORGOTH) && p_ptr->on_the_run) m_ptr->mflag |= (MFLAG_ACTV);

	// Pursuing creatures are always active at the Gates
	if ((r_ptr->level > 17) && (p_ptr->depth == 0)) m_ptr->mflag |= (MFLAG_ACTV);

	// 'short sighted' monsters are active when the player is *very* close
	if (r_ptr->flags2 & (RF2_SHORT_SIGHTED))
	{
		if (m_ptr->cdis <= 2) m_ptr->mflag |= (MFLAG_ACTV);
	}
	
	// other monsters
	else
	{
		// monsters that can see the player are active
		if (los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px)) m_ptr->mflag |= (MFLAG_ACTV);
		
		// monsters that can hear the player are active (Sil-y: note this is a rather arbitrary calculation)
		if (flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx) < 20) m_ptr->mflag |= (MFLAG_ACTV);
		//if (flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx) < 20 + monster_skill(m_ptr, S_PER) - stealth_score) m_ptr->mflag |= (MFLAG_ACTV);

		// monsters that can smell the player are active (Sil-y: I don't think this ever happens)
		if (monster_can_smell(m_ptr)) m_ptr->mflag |= (MFLAG_ACTV);
	}
	
	/*
	 * Special handling if the first turn a monster has after
	 * being attacked by the player, but the player is out of sight
	 */
	if (m_ptr->mflag & (MFLAG_HIT_BY_RANGED))
	{
		// Monster will be very upset if it can't see the player
		// or if it is in a corridor and can't fire back
		if (((m_ptr->best_range == 1) && !(cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_ROOM))) ||
			!player_has_los_bold(m_ptr->fy, m_ptr->fx))
		{
			m_ptr->mflag |= (MFLAG_AGGRESSIVE);

			/*if smart and has allies, let them know*/
			if ((r_ptr->flags2 & (RF2_SMART)) &&
			    ((r_ptr->flags1 & (RF1_FRIENDS)) ||
				 (r_ptr->flags1 & (RF1_FRIEND)) ||
				 (r_ptr->flags1 & (RF1_UNIQUE_FRIEND)) ||
				 (r_ptr->flags1 & (RF1_ESCORT)) ||
				 (r_ptr->flags1 & (RF1_ESCORTS)) ||
				 (r_ptr->flags4 & (RF4_SHRIEK))))
			{
				tell_allies(m_ptr->fy, m_ptr->fx, MFLAG_AGGRESSIVE);
			}
			
			/*Monsters with ranged attacks will try to cast a spell*/
			if (r_ptr->freq_ranged) m_ptr->mflag |= (MFLAG_ALWAYS_CAST);

			calc_monster_speed(m_ptr->fy, m_ptr->fx);
		}

		/*clear the flag*/
		m_ptr->mflag &= ~(MFLAG_HIT_BY_RANGED);

	}

	/*This if the first turn a monster has after being attacked by the player*/
	if (m_ptr->mflag & (MFLAG_HIT_BY_MELEE))
	{
		/*
		 * Monster will be very upset if:
		 * (1) if it isn't next to the player on its turn (pillar dance, hack-n-back, etc)
		 */
		if (((m_ptr->cdis > 1) && !(m_ptr->mflag & (MFLAG_PUSHED))))
		{
			m_ptr->mflag |= (MFLAG_AGGRESSIVE);

			/*if smart and has allies, let them know*/
			if ((r_ptr->flags2 & (RF2_SMART)) &&
			    ((r_ptr->flags1 & (RF1_FRIENDS)) ||
				 (r_ptr->flags1 & (RF1_FRIEND)) ||
				 (r_ptr->flags1 & (RF1_UNIQUE_FRIEND)) ||
				 (r_ptr->flags1 & (RF1_ESCORT)) ||
				 (r_ptr->flags1 & (RF1_ESCORTS)) ||
				 (r_ptr->flags4 & (RF4_SHRIEK))))
			{
				tell_allies(m_ptr->fy, m_ptr->fx, MFLAG_AGGRESSIVE);
			}

			/*Monsters with ranged attacks will try to cast a spell*/
			if (r_ptr->freq_ranged) m_ptr->mflag |= (MFLAG_ALWAYS_CAST);
			
			calc_monster_speed(m_ptr->fy, m_ptr->fx);
		}
		
		/*clear the flags*/
		m_ptr->mflag &= ~(MFLAG_HIT_BY_MELEE);
	}

	// clear CHARGED flag
	if (m_ptr->mflag & (MFLAG_CHARGED))
	{
		m_ptr->mflag &= ~(MFLAG_CHARGED);
	}
	
	// If a smart monster has sleeping friends and sees player, sometimes shout a warning
	if (one_in_(2) && (r_ptr->flags2 & (RF2_SMART)) &&
	    player_has_los_bold(m_ptr->fy,m_ptr->fx) && has_sleeping_kin(m_ptr))
	{
		/*if part of a pack, let them know*/
		if ((r_ptr->flags1 & (RF1_FRIENDS)) ||
			(r_ptr->flags1 & (RF1_FRIEND)) ||
			(r_ptr->flags1 & (RF1_UNIQUE_FRIEND)) ||
			(r_ptr->flags1 & (RF1_ESCORT)) ||
			(r_ptr->flags1 & (RF1_ESCORTS)) ||
			(r_ptr->flags4 & (RF4_SHRIEK)))
		{
			// just make them active, not aggressive
			tell_allies(m_ptr->fy, m_ptr->fx, MFLAG_ACTV);
		}
		
		calc_monster_speed(m_ptr->fy, m_ptr->fx);
	}
	
	/*clear the 'pushed' flag*/
	m_ptr->mflag &= ~(MFLAG_PUSHED);

	/* A monster in passive mode will end its turn at this point. */
	if (!(m_ptr->mflag & (MFLAG_ACTV)))
	{
		wander(m_ptr);
		return;
	}
	
	/* Hack -- Always redraw the current target monster health bar */
	if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
		p_ptr->redraw |= (PR_HEALTHBAR);

	/* Attempt to multiply if able to and allowed */
	if ((r_ptr->flags2 & (RF2_MULTIPLY)) &&
	    (mon_cnt < MAX_MONSTERS - 50))
	{        
		/* Count the adjacent monsters (including itself) */
		for (k = 0, y = m_ptr->fy - 1; y <= m_ptr->fy + 1; y++)
		{
			for (x = m_ptr->fx - 1; x <= m_ptr->fx + 1; x++)
			{
				/* Check Bounds */
				if (!in_bounds(y, x)) continue;

				/* Count monsters */
				if (cave_m_idx[y][x] > 0) k++;
			}
		}

		/* Hack -- multiply slower in crowded areas */
		if ((k <= 3) && (one_in_(k * 8)))
		{
			/* Try to multiply */
			if (multiply_monster(cave_m_idx[m_ptr->fy][m_ptr->fx]))
			{
				/* Take note if visible */
				if (m_ptr->ml)
				{
					l_ptr->flags2 |= (RF2_MULTIPLY);
				}

				/* Multiplying takes energy */
				return;
			}
		}
	}

	/*** Ranged attacks ***/

	/* Monster can cast spells */
	if (r_ptr->freq_ranged)
	{
		/* Extract the ranged attack probability. */
		chance = r_ptr->freq_ranged;
		
		/* Certain conditions always cause a monster to always cast */
		if (m_ptr->mflag & (MFLAG_ALWAYS_CAST)) chance = 100;

		/* Cannot use ranged attacks when confused. */
		if (m_ptr->confused) chance = 0;

		/* Cannot use ranged attacks during the truce. */
		if (p_ptr->truce) chance = 0;
		
		/* Stunned monsters use ranged attacks half as often. */
		if ((chance) && (m_ptr->stunned)) chance /= 2;

		/* Monster can use ranged attacks */
		if ((chance) && percent_chance(chance))
		{
			/* Pick a ranged attack */
			choice = choose_ranged_attack(cave_m_idx[m_ptr->fy][m_ptr->fx]);
		}
		
		/* Selected a ranged attack? */
		if (choice != 0)
		{
			/* Execute said attack */
			make_attack_ranged(m_ptr, choice);
				
			/* End turn */
			return;
		}
	}
	
	/*** Movement ***/

	/* Assume no movement */
	ty = 0;
	tx = 0;


	/*
	 * Innate semi-random movement.  Monsters adjacent to the character
	 * have more chance of just attacking normally.
	 */
	if (r_ptr->flags1 & (RF1_RAND_50 | RF1_RAND_25))
	{
		chance = 0;

		/* RAND_25 and RAND_50 are cumulative */
		if (r_ptr->flags1 & (RF1_RAND_25))
		{
			chance += 25;
			if (m_ptr->ml) l_ptr->flags1 |= (RF1_RAND_25);
		}
		if (r_ptr->flags1 & (RF1_RAND_50))
		{
			chance += 50;
			if (m_ptr->ml) l_ptr->flags1 |= (RF1_RAND_50);
		}

		if (m_ptr->cdis > 1)
		{
			chance /= 2;
		}
 
		/* Chance of moving randomly */
		if (percent_chance(chance)) random_move = TRUE;
	}

	/* Monster isn't moving randomly, isn't running away
	 * doesn't hear or smell the character
	 */
	if (!random_move)
	{
		/*
	     * First, monsters who can't cast, are aggressive, and
		 * are not afraid just want to charge
		 */
		if (m_ptr->stance != STANCE_FLEEING)
		{

			if ((m_ptr->stance == STANCE_AGGRESSIVE) && (!r_ptr->freq_ranged))
			{
				m_ptr->target_y = 0;
				m_ptr->target_x = 0;
			}

			/* Player can see the monster, and it is not afraid */
			if (player_has_los_bold(m_ptr->fy, m_ptr->fx))
			{
				m_ptr->target_y = 0;
				m_ptr->target_x = 0;
			}
		}

		/* Monster has a known target */
		if ((m_ptr->target_y) && (m_ptr->target_x)) must_use_target = TRUE;
	}
	
	/*** Find a target to move to ***/

	/* Monster isn't confused, just moving semi-randomly */
	if (random_move)
	{

		int start = rand_int(8);
		bool dummy;

		/* Is the monster scared? */
		if ((!(r_ptr->flags1 & (RF1_NEVER_MOVE))) &&
		    ((m_ptr->min_range >= FLEE_RANGE) || (m_ptr->stance == STANCE_FLEEING)))
		{
			fear = TRUE;
		}

		/* Look at adjacent grids, starting at random. */
		for (i = start; i < 8 + start; i++)
		{
			y = m_ptr->fy + ddy_ddd[i % 8];
			x = m_ptr->fx + ddx_ddd[i % 8];

			/* Accept first passable grid. */
			if (cave_passable_mon(m_ptr, y, x, &dummy) != 0)
			{
				ty = y;
				tx = x;
				break;
			}
		}

		/* No passable grids found */
		if ((ty == 0) && (tx == 0)) return;

		/* Cannot move, target grid does not contain the character */
		if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) &&
		    (cave_m_idx[ty][tx] >= 0))
		{
			/* Cannot move */
			return;
		}
	}

	/* Normal movement */
	else
	{
        // *extremely* frightened monsters next to chasms may jump into the void
        if ((m_ptr->stance == STANCE_FLEEING) && (m_ptr->morale < -200) && !(r_ptr->flags2 & (RF2_FLYING)) && one_in_(2))
        {
            int chasm_y = 0;
            int chasm_x = 0;
            
            /* Look at adjacent grids */
            for (i = 0; i < 8; i++)
            {
                int y = m_ptr->fy + ddy_ddd[i];
                int x = m_ptr->fx + ddx_ddd[i];
                
                /* Check bounds */
                if (!in_bounds(y, x)) continue;
                
                /* Accept a chasm square */
                if (cave_feat[y][x] == FEAT_CHASM)
                {
                    chasm_y = y;
                    chasm_x = x;
                    
                    break;
                }
            }
            
            if (chasm_y != 0)
            {
                monster_swap(m_ptr->fy, m_ptr->fx, chasm_y, chasm_x);
                return;
            }
        }
        
		/* Choose a pair of target grids, or cancel the move. */
		if (!get_move(m_ptr, &ty, &tx, &fear, must_use_target))
		{
			return;
		}
	}
	
	// If the monster thinks its location is optimal...
	if ((ty == m_ptr->fy) && (tx == m_ptr->fx))
	{
		// intelligent monsters that are fleeing can try to use stairs (but not territorial ones)
		if ((r_ptr->flags2 & (RF2_SMART)) && !(r_ptr->flags2 & (RF2_TERRITORIAL)) && (m_ptr->stance == STANCE_FLEEING))
		{
			if (cave_stair_bold(m_ptr->fy, m_ptr->fx))
			{
				char m_name[80];
								
				if (m_ptr->ml)
				{
					monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);
					if (cave_down_stairs_bold(m_ptr->fy, m_ptr->fx))	msg_format("%^s flees down the stairs.", m_name);
					else                                                msg_format("%^s flees up the stairs.", m_name);
				}
				
				// if adjacent, you get a chance for an opportunist attack, which might kill them
                // (skip_next_turn is there to stop you getting opportunist attacks afer knocking someone back)
				if (p_ptr->active_ability[S_STL][STL_OPPORTUNIST] && m_ptr->ml && !m_ptr->skip_next_turn && (m_ptr->alertness >= ALERTNESS_ALERT) && !p_ptr->truce &&
					!p_ptr->confused && !p_ptr->afraid && !p_ptr->entranced && (p_ptr->stun <= 100))
				{
					if ((distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px) == 1))
					{
						py_attack_aux(m_ptr->fy, m_ptr->fx, ATT_OPPORTUNIST);
					}
				}

				// removes the monster if it is still alive
				delete_monster(ty, tx);
				
				return;
			}
		}
		
		// if the square is non-adjacent to the player, then allow a ranged attack instead of a move
		if ((m_ptr->cdis > 1) && r_ptr->freq_ranged)
		{
			choice = choose_ranged_attack(cave_m_idx[m_ptr->fy][m_ptr->fx]);

			/* Selected a ranged attack? */
			if (choice != 0)
			{
				/* Execute said attack */
				make_attack_ranged(m_ptr, choice);
			}			
		}
		
		return;
	}
		
	/* Calculate the actual move.  Cancel move on failure to enter grid. */
	if (!make_move(m_ptr, &ty, &tx, fear, &bash)) return;
		
	/* Change terrain, move the monster, handle secondary effects. */
	process_move(m_ptr, ty, tx, bash);

	/* End turn */
	return;
}

/*
 * Produces a cloud if there is one.
 */
extern void produce_cloud(monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];

	/* Handle any area-effects of the monster - only if active */
	if ((r_ptr->flags2 & (RF2_CLOUD_SURROUND)) &&
	    (m_ptr->mflag & (MFLAG_ACTV)))
	{
		/* Assume no affect */
		bool affect = FALSE;

		int typ, dd, ds, rad;

		/* Get information */
		cloud_surround(m_ptr->r_idx, &typ, &dd, &ds, &rad);

		/* Monsters wait for the character to approach and in line of sight */
		if ((m_ptr->cdis <= 5) && (player_can_see_bold(m_ptr->fy, m_ptr->fx))) affect = TRUE;

		/* Affect surroundings if appropriate */
		if (affect)
		{
			/* Learn about monster (before visibility changes) */
			if ((m_ptr->ml) && (r_ptr->flags2 & (RF2_CLOUD_SURROUND)))
			{
				l_ptr->flags2 |= (RF2_CLOUD_SURROUND);
			}

			// Sil-y: might want to change difficulty of cloud if I have fear clouds etc
			/* Release of cloud (can affect visibility) */
			if (typ) mon_cloud(cave_m_idx[m_ptr->fy][m_ptr->fx], typ, dd, ds, -1, rad);
		}
	}
	
	return;
}

/*
 *  Calculate the number of monsters of the same type (same letter or RF3 type)
 *  within LOS of a given monster.
 */
int morale_from_friends(monster_type *m_ptr)
{
	int i;
	int fy, fx, y, x;
	int morale_bonus = 0;
	int morale_penalty = 0;
	
	/* Location of main monster */
	fy = m_ptr->fy;
	fx = m_ptr->fx;
	
	/* Scan monsters */
	for (i = 1; i < mon_max; i++)
	{
		monster_type *n_ptr = &mon_list[i];
		
		/* Skip dead monsters */
		if (!n_ptr->r_idx) continue;
		
		/* Location of other monster */
		y = n_ptr->fy;
		x = n_ptr->fx;

		/* Skip self! */
		if ((fy == y) && (fx == x)) continue;
		
		// Only consider alert monsters of the same type in line of sight
		if ((n_ptr->alertness >= ALERTNESS_ALERT) && similar_monsters(fy, fx, y, x) && los(fy, fx, y, x))
		{
			monster_race *nr_ptr = &r_info[n_ptr->r_idx];
			int multiplier = 1;
			
			if ((nr_ptr->flags1 & (RF1_ESCORT)) || (nr_ptr->flags1 & (RF1_ESCORTS)))	multiplier = 4;
			
			// add bonus or penalty to morale
			if (n_ptr->stance == STANCE_FLEEING)	morale_penalty += 10 * multiplier;
			else									morale_bonus   += 10 * multiplier;
		}
	}
	
	return (morale_bonus - morale_penalty);
}


/*
 * Calculate the morale for a monster.
 */
void calc_morale(monster_type *m_ptr)
{
	int morale;
    int difference;
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	s16b this_o_idx, next_o_idx = 0;
	
	// Starting morale is 60
	morale = 60;
	
	// Monsters have boosted morale during the endgame
	if (p_ptr->on_the_run)
	{
		morale += 20;
	}
	
	// Monsters have higher morale if they are usually found deeper than this and vice versa
	else
	{
		morale += (r_ptr->level - p_ptr->depth) * 10;
		
		// make sure orcs etc in throne room don't have too low morale
		if (p_ptr->depth == MORGOTH_DEPTH) morale = MAX(morale, 20);
	}
		
	// Take player's conditions into account
	if (p_ptr->image)			morale += 20;
	if (p_ptr->blind)			morale += 20;
	if (p_ptr->confused)		morale += 40;
	if (p_ptr->slow)			morale += 40;
	if (p_ptr->afraid)			morale += 40;
	if (p_ptr->entranced)		morale += 80;
	else if (p_ptr->stun > 100)	morale += 80;
	else if (p_ptr->stun > 50)	morale += 40;
	else if (p_ptr->stun > 0)	morale += 20;
	
	// Take player's health into account
	switch (health_level(p_ptr->chp, p_ptr->mhp))
	{
		case  HEALTH_WOUNDED:		morale += 20;	break;  // <= 75% health
		case  HEALTH_BADLY_WOUNDED:	morale += 40;	break;  // <= 50% health
		case  HEALTH_ALMOST_DEAD:	morale += 80;	break;  // <= 25% health
	}

	// Take monster's conditions into account
	if (m_ptr->stunned)			morale -= 20;
	// skip confusion as it is less good if confused monsters flee
	if (m_ptr->hasted)			morale += 40;

	// Take monster's health into account
	switch (health_level(m_ptr->hp, m_ptr->maxhp))
	{
		case  HEALTH_WOUNDED:		morale -= 20;	break;  // <= 75% health
		case  HEALTH_BADLY_WOUNDED:	morale -= 40;	break;  // <= 50% health
		case  HEALTH_ALMOST_DEAD:	morale -= 80;	break;  // <= 25% health
	}

	// Extra penalty if <=75% health and already fleeing
	// helps avoid them coming back too quickly
	if ((m_ptr->stance == STANCE_FLEEING) && (health_level(m_ptr->hp, m_ptr->maxhp) <= HEALTH_WOUNDED))
	{
		morale -= 20;
	}

	// Get a bonus for non-fleeing friends and a penalty for fleeing ones
	morale += morale_from_friends(m_ptr);
			
	// Reduce morale for light averse monsters facing a brightly lit player
	if ((r_ptr->flags3 & (RF3_HURT_LITE)) && (cave_light[p_ptr->py][p_ptr->px] >= 4))
	{
		morale -= (cave_light[p_ptr->py][p_ptr->px] - 3) * 10;
	}
		
	// Reduce morale for each carried object for non-uniques, so thieves avoid player
	if (!(r_ptr->flags1 & (RF1_UNIQUE)))
	{
		for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
		{
			object_type *o_ptr;
			
			/* Get the object */
			o_ptr = &o_list[this_o_idx];
			
			/* Get the next object */
			next_o_idx = o_ptr->next_o_idx;
			
			// Lower morale
			morale -= 20;
		}
	}
		
	// reduce morale for the Majesty ability
    difference = MAX(p_ptr->skill_use[S_WIL] - monster_skill(m_ptr, S_WIL), 0);
	if (p_ptr->active_ability[S_WIL][WIL_MAJESTY]) morale -= difference / 2 * 10;

	// reduce morale for the Bane ability
	if (p_ptr->active_ability[S_PER][PER_BANE]) morale -= bane_bonus(m_ptr) * 10;

    // increase morale for the Elf-Bane ability
	morale += elf_bane_bonus(m_ptr) * 10;

	// add temporary morale modifiers
	morale += m_ptr->tmp_morale;

	// update the morale
	m_ptr->morale = morale;

	return;
}	

/*
 * Calculate the stance for a monster.
 *
 * Based on the monster's morale, type, and other effects.
 *
 * Can be:
 *    STANCE_FLEEING
 *    STANCE_CONFIDENT
 *    STANCE_AGGRESSIVE
 */
void calc_stance(monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	
	int stance;
	int stances[3];
		
	// set the default stances
	stances[0] = STANCE_FLEEING;
	stances[1] = STANCE_CONFIDENT;
	stances[2] = STANCE_AGGRESSIVE;
		
	// Some monsters are immune to (non-magical) fear
	if ((r_ptr->flags3 & (RF3_NO_FEAR)) && (m_ptr->tmp_morale >= 0))
	{
		stances[0] = STANCE_CONFIDENT;
	}

	// Mindless monsters just attack
	if (r_ptr->flags2 & (RF2_MINDLESS))
	{
		stances[0] = STANCE_AGGRESSIVE;
		stances[1] = STANCE_AGGRESSIVE;
	}
	
	// Trolls are aggressive rather than confident
	if (r_ptr->flags3 & (RF3_TROLL))
	{
		stances[1] = STANCE_AGGRESSIVE;
	}
	
	// aggravation makes non-mindless things much more hostile
	if (p_ptr->aggravate & !(r_ptr->flags2 & (RF2_MINDLESS)))
	{
		stances[1] = STANCE_AGGRESSIVE;
	}

	// monsters that have been angered have confident turned into aggressive
	if (m_ptr->mflag & (MFLAG_AGGRESSIVE))
	{
		stances[1] = STANCE_AGGRESSIVE;
	}
	
	// Determine the stance
	if (m_ptr->morale > 200)					stance = stances[2];
	else if (m_ptr->morale > 0)					stance = stances[1];
	else										stance = stances[0];

	// override this for unwary/sleeping monsters
	if (m_ptr->alertness < ALERTNESS_ALERT)	stance = stances[1];
	
	// React to changes in stance
	if (stance != m_ptr->stance)
	{
		char m_name[80];
		char buf[160];
		bool message = FALSE;
		
		/* Get the monster name */
		monster_desc(m_name, sizeof(m_name), m_ptr, 0);
		
		switch (m_ptr->stance)
		{
			case STANCE_FLEEING:
			{
				// give the monster a temporary 'rally' bonus to its morale
				m_ptr->tmp_morale += 60;
				calc_morale(m_ptr);
				
				if (!p_ptr->truce)	sprintf(buf, "turns to fight!");
				else				sprintf(buf, "recovers its composure.");
				
				message = TRUE;
				
				break;
			}
			case STANCE_CONFIDENT:
			{
				if (stance == STANCE_FLEEING)
				{
					// give the monster a temporary 'break' penalty to its morale
					m_ptr->tmp_morale -= 60;
					calc_morale(m_ptr);
					
					sprintf(buf, "flees in terror!");
					message = TRUE;
				}
				break;
			}
			case STANCE_AGGRESSIVE:
			{
				if (stance == STANCE_FLEEING)
				{
					// give the monster a temporary 'break' penalty to its morale
					m_ptr->tmp_morale -= 60;
					calc_morale(m_ptr);
					
					sprintf(buf, "flees in terror!");
					message = TRUE;
				}
				break;
			}
		}
		
		// Inform player of visible changes
		if (message && m_ptr->ml && !(r_ptr->flags1 & (RF1_NEVER_MOVE)))
		{
			msg_format("%^s %s", m_name, buf);
		}

		// force recalculation of range if stance changes
		m_ptr->min_range = 0;
		
 	}
	
	// update the monster's stance
	m_ptr->stance = stance;
}


/*
 * Monster regeneration of recovery from all temporary
 * conditions.
 *
 * This function is called a lot, and is therefore fairly expensive.
 */
static void recover_monster(monster_type *m_ptr)
{
	bool visible = FALSE;
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
    int i;

    // summoned monsters have a half-life of one turn after the song stops
    if (m_ptr->mflag & (MFLAG_SUMMONED))
    {
        int still_singing = FALSE;
        
        for (i = 1; i < mon_max; i++)
        {
            monster_type *n_ptr = &mon_list[i];
            
            /* Skip dead monsters */
            if (!n_ptr->r_idx) continue;
            
            // note if any monster is singing the song of oaths
            if (n_ptr->song == SNG_OATHS) still_singing = TRUE;
        }
        
        if (!still_singing && one_in_(2))
        {
            // removes the monster
            delete_monster(m_ptr->fy, m_ptr->fx);
        }
    }
    
	/* Visible monsters must be both seen and noticed */
	if (m_ptr->ml)
	{
		visible = TRUE;
	}

	/* produce a cloud if appropriate */
	produce_cloud(m_ptr);

	/* Recover from stuns */
	if (m_ptr->stunned)
	{
		/* Recover somewhat */
		m_ptr->stunned -= 1;

		/* Message if visible */
		if ((m_ptr->stunned == 0) && visible)
		{
			char m_name[80];

			/* Acquire the monster name */
			monster_desc(m_name, sizeof(m_name), m_ptr, 0);

			/* Dump a message */
			msg_format("%^s is no longer stunned.", m_name);
		}
	}


	/* Recover from confusion */
	if (m_ptr->confused)
	{
		/* Recover somewhat */
		m_ptr->confused -= 1;

		/* Message if visible */
		if ((m_ptr->confused == 0) && visible)
		{
			char m_name[80];

			/* Acquire the monster name */
			monster_desc(m_name, sizeof(m_name), m_ptr, 0);

			/* Dump a message */
			msg_format("%^s is no longer confused.", m_name);
		}
	}


	/* Reduce temporary morale modifiers by 10% */
	if (m_ptr->tmp_morale != 0)
	{
		m_ptr->tmp_morale *= 9;
		m_ptr->tmp_morale /= 10;
	}
	

	/*
	 * Handle haste counter
	 */
	if (m_ptr->hasted)
	{

		/*efficiency*/
		if (m_ptr->hasted > 1) m_ptr->hasted -= 1;

		/*set to 0 and give message*/
		else set_monster_haste(cave_m_idx[m_ptr->fy][m_ptr->fx], 0, visible);

	}

	/*
	 * Handle slow counter
	 */
	if (m_ptr->slowed)
	{

		/*efficiency*/
		if (m_ptr->slowed > 1) m_ptr->slowed -= 1;

		/*set to 0 and give message*/
		else set_monster_slow(cave_m_idx[m_ptr->fy][m_ptr->fx], 0, visible);

	}

	/* Hack -- Update the health and mana bar (always) */
	if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
		p_ptr->redraw |= (PR_HEALTHBAR);

	// Monsters who are out of sight and fail their perception rolls by 25 or more (15 with Vanish) start to lose track of the player
	if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px) && (m_ptr->alertness >= ALERTNESS_ALERT) && 
	    (m_ptr->stance != STANCE_FLEEING) && (r_ptr->sleep > 0))
	{
		int perception_bonus = p_ptr->active_ability[S_STL][STL_VANISH] ? 15 : 25;
		int result = skill_check(m_ptr, 
		                         monster_skill(m_ptr, S_PER) + perception_bonus,
		                         p_ptr->skill_use[S_STL] + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
								 PLAYER);

		if (result < 0)
		{
			set_alertness(m_ptr, MAX(m_ptr->alertness + result, ALERTNESS_UNWARY));
		}
	}
	
	// calculate the monster's morale and stance
	calc_morale(m_ptr);
	calc_stance(m_ptr);
			
}



/*
 * Process all living monsters, once per game turn.
 *
 * Scan through the list of all living monsters, (backwards, so we can
 * excise any "freshly dead" monsters).
 *
 * Regenerate monsters when it is their turn to move.
 * Allow fully energized monsters to take their turns.*
 * This function and its children are responsible for at least a third of
 * the processor time in normal situations.  If the character is resting,
 * this may rise substantially.
 */
void process_monsters(s16b minimum_energy)
{
	int i;
	monster_type *m_ptr;

	// if time is stopped, no monsters can move
	if (cheat_timestop) return;
	
	/* Process the monsters (backwards) */
	for (i = mon_max - 1; i >= 1; i--)
	{

		/* Player is dead or leaving the current level */
		if (p_ptr->leaving) break;

		/* Access the monster */
		m_ptr = &mon_list[i];

		/* Ignore dead monsters */
		if (!m_ptr->r_idx) continue;

		/* Leave monsters without enough energy for later */
		if (m_ptr->energy < minimum_energy) continue;

		/* End the turn of monsters without enough energy to move*/
		if (m_ptr->energy < 100) continue;

		/* Handle temporary monster attributes */
		recover_monster(m_ptr);

		/* Use up some energy */
		m_ptr->energy -= 100;

		/* Sleeping monsters don't get a move */
		if (m_ptr->alertness < ALERTNESS_UNWARY) continue;
		
		// Monsters who have just noticed you miss their turns (as do those who have been knocked back...)
		if (m_ptr->skip_next_turn)
		{
            // reset its previous movement to stop it charging etc.
            m_ptr->previous_action[0] = ACTION_MISC;
			
			m_ptr->skip_next_turn = FALSE;
			continue;
		}

		/* Let the monster take its turn */
		process_monster(m_ptr);

	}
	
}


/*
 * Lets all monsters attempt to notice the player.
 * It can get called multiple times per playerturn. 
 * Once each turn is the 'main roll' which is handled differently from the others.
 * The other rolls correspond to noisy events.
 * These events can be caused by the player (in which case 'player_centered' is set to true),
 * or can be caused by a monster, in which case it will be false and FLOW_MONSTER_NOISE will be used
 * instead of the usual FLOW_PLAYER_NOISE.
 */

void monster_perception(bool player_centered, bool main_roll, int difficulty)
{
	int i;
	int m_perception;
	int result;
	int noise_dist;
	monster_type *m_ptr;
	monster_race *r_ptr;
	monster_lore *l_ptr;
	int difficulty_roll;
	int difficulty_roll_alt;
		
	int combat_noise_bonus = 0;
	int combat_sight_bonus = 0;
		
	/* Player is dead or leaving the current level */
	if (p_ptr->leaving) return;
	
	// no perception on the first turn of the game
	if (playerturn == 0) return;
	
	// if time is stopped, no monsters can perceive
	if (cheat_timestop) return;
	
	// bonuses for the monster if the player attacked a monster or was attacked
	if (main_roll)
	{
		if (player_attacked)
		{
			combat_noise_bonus += 2;
			combat_sight_bonus += 2;
			player_attacked = FALSE;

			// keep track of this for the ability 'Concentration'
			p_ptr->consecutive_attacks++;
		}
		if (attacked_player)
		{
			combat_noise_bonus += 2;
			combat_sight_bonus += 2;
			attacked_player = FALSE;
		}
	}

	// display the amount of noise the player is making in the bottom left of the screen
	if (main_roll)
	{
		char buf[13];
		byte attr;
		int dif = difficulty - combat_noise_bonus;
				
		if (singing(SNG_SILENCE))	dif += ability_bonus(S_SNG, SNG_SILENCE);
		
		if (dif < -5)		attr = TERM_RED;
		else if (dif < 0)	attr = TERM_L_RED;
		else if (dif < 5)	attr = TERM_ORANGE;
		else if (dif < 10)	attr = TERM_YELLOW;
		else if (dif < 15)	attr = TERM_L_GREEN;
		else if (dif < 20)	attr = TERM_L_BLUE;
		else				attr = TERM_BLUE;
		
		my_strcpy(buf, format("%3d", dif), sizeof(buf));
		//c_put_str(attr, buf, ROW_STEALTH, COL_STEALTH);
	}
	
	// make the difficulty roll just once per sound source
	// i.e. once per call to this function
	// this is a manual version of a 'skill_check()' and should be treated as such
	difficulty_roll = difficulty + dieroll(10);

	// deal with player curses for skill rolls
	// this is not perfect as some 'player_centered' things are not actually caused by the player
	difficulty_roll_alt = difficulty + dieroll(10);
	if (p_ptr->cursed && player_centered) difficulty_roll = MIN(difficulty_roll, difficulty_roll_alt);
	
	// the song of silence quietens this a bit
	if (singing(SNG_SILENCE)) difficulty_roll += ability_bonus(S_SNG, SNG_SILENCE);

	/* Process the monsters (backwards) */
	for (i = mon_max - 1; i >= 1; i--)
	{
		/* Access the monster */
		m_ptr = &mon_list[i];
		
		// Access the race and lore information
		r_ptr = &r_info[m_ptr->r_idx];
		l_ptr = &l_list[m_ptr->r_idx];
		
		/* Ignore dead monsters */
		if (!m_ptr->r_idx) continue;
	
		/* If character is within detection range (unlimited for most monsters, 2 for shortsighted ones) */		
		if (!((r_ptr->flags2 & (RF2_SHORT_SIGHTED)) && (m_ptr->cdis > 2)))
		{
			if (player_centered)
			{
				noise_dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
			}
			else
			{
				noise_dist = flow_dist(FLOW_MONSTER_NOISE, m_ptr->fy, m_ptr->fx);
			}
			
			// start building up the monster's total perception
			m_perception = monster_skill(m_ptr, S_PER) - noise_dist + combat_noise_bonus;

			// deal with bane ability (theoretically should modify player roll, but this is equivalent)
			m_perception -= bane_bonus(m_ptr);

            // increase morale for the Elf-Bane ability
            m_perception += elf_bane_bonus(m_ptr);
            
			// monsters are looking more carefully during the escape
			if (p_ptr->on_the_run) m_perception += 5;
			
			// monsters that are already alert get a penalty to the roll to stop them getting *too* alert
			if (m_ptr->alertness >= ALERTNESS_ALERT) m_perception -= m_ptr->alertness;
			
			// aggravation makes non-sleeping monsters much more likely to notice you
			if (p_ptr->aggravate && (m_ptr->alertness >= ALERTNESS_UNWARY) && 
				!(r_ptr->flags2 & (RF2_MINDLESS))) 
			{
				m_perception += p_ptr->aggravate * 10;
			}
							
			// awake creatures who have line of sight on player get a bonus
			if (los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px) && (m_ptr->alertness >= ALERTNESS_UNWARY))
			{
				int d, dir, y, x, open_squares = 0;
			                
				// check adjacent squares for impassable squares
				for (d = 0; d < 8; d++)
				{
					dir = cycle[d];
					
					y = p_ptr->py + ddy[dir];
					x = p_ptr->px + ddx[dir];
					
					if (cave_floor_bold(y,x))
					{
						open_squares++;
					}
				}
				
				// bonus reduced if the player has 'disguise'
				if (p_ptr->active_ability[S_STL][STL_DISGUISE])
				{
					m_perception += (open_squares + combat_sight_bonus) / 2;
				}
				else
				{
					m_perception += open_squares + combat_sight_bonus;
				}
			}
			
			// do the 'skill_check()' versus the quietness of the sound...
			result = (m_perception + dieroll(10)) - difficulty_roll;

			/* Debugging message */
			if (cheat_skill_rolls)
			{
				msg_format("{%d+%d v %d+%d = %d}.", result - m_perception + difficulty_roll, m_perception, 
						   difficulty_roll - difficulty, difficulty,
						   result);
			}
			
			if (result > 0)
			{					
				// Partly alert monster
				set_alertness(m_ptr, m_ptr->alertness + result);
									
				/* Still not alert */
				if (m_ptr->alertness < ALERTNESS_ALERT)
				{
					/* Notice the "not noticing" */
					if (m_ptr->ml && (l_ptr->ignore < MAX_UCHAR))
					{
						l_ptr->ignore++;
					}
				}
				
				/* Just became alert */
				else
				{
					/* Notice the "noticing" */
					if (m_ptr->ml && (l_ptr->notice < MAX_UCHAR))
					{
						l_ptr->notice++;
					}
				}
			}
		}
	}
}
