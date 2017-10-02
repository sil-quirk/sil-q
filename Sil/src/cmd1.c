/* File: cmd1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


void new_wandering_flow(monster_type *m_ptr, int ty, int tx)
{	
	int y, x, i;
	int wandering_idx = m_ptr->wandering_idx;
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	
	if (wandering_idx < FLOW_WANDERING_HEAD)
	{
		return;
	}

	// territorial monsters target their creation location
	// same with the tutorial
	if ((r_ptr->flags2 & (RF2_TERRITORIAL)) || (p_ptr->game_type < 0))
	{
		// they only pick a new location on creation
		// Sil-y: Hack: using the fact that speed hasn't been determined yet on creation
		if (m_ptr->mspeed == 0)
		{
			// update the flow
			update_flow(m_ptr->fy, m_ptr->fx, wandering_idx);
		}
	}
	
	// if a location was requested, use that
	else if (in_bounds_fully(ty, tx))
	{
		y = ty;
		x = tx;
		
		// update the flow
		update_flow(y, x, wandering_idx);
	}
	
	// otherwise choose a location
	else
	{
		// sometimes intelligent monsters want to pick a staircase and leave the level
		if ((r_ptr->flags2 & (RF2_SMART)) && !(r_ptr->flags2 & (RF2_TERRITORIAL)) && (p_ptr->depth != MORGOTH_DEPTH) &&
		    one_in_(5) && random_stair_location(&y, &x) && (cave_m_idx[y][x] >= 0) && !(cave_info[y][x] & (CAVE_ICKY)))
		{
			// update the flow
			update_flow(y, x, wandering_idx);
		}
		
		// otherwise pick a random location (on a floor, in a room, and not in a vault)
		else
		{
			// give up after 100 tries
			for (i = 0; i < 100; i++)
			{
				y = rand_int(p_ptr->cur_map_hgt);
				x = rand_int(p_ptr->cur_map_wid);
				if (in_bounds_fully(y, x) && (cave_feat[y][x] == FEAT_FLOOR) && 
					(cave_info[y][x] & (CAVE_ROOM)) &&
					!(cave_info[y][x] & (CAVE_ICKY)))
				{
					// update the flow
					update_flow(y, x, wandering_idx);
					break;
				}
			}
		}
	
	}
	
	// reset the pause (if any)
	wandering_pause[wandering_idx] = 0;
}


/*
 * Determines a wandering-destination for a monster.
 * default_idx_ptr is the wandering index to use by default, and gets updated by this function.
 */
void new_wandering_destination(monster_type *m_ptr, monster_type *leader_ptr)
{
	int i;
	bool wandering_indices[FLOW_WANDERING_TAIL+1];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	// many monsters don't get wandering destinations:
	if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) || (r_ptr->flags1 & (RF1_HIDDEN_MOVE)) 
		|| !((r_ptr->flags2 & (RF2_SMART)) || (r_ptr->flags4 & (RF4_SHRIEK))))
	{
		return;
	}
	
	// there is a special way of finding indices at the Gates level
	// as otherwise we run out too quickly
	if (p_ptr->depth == 0)
	{		
		// mark the used indices
		for (i = 1; i < mon_max; i++)
		{
			monster_type *n_ptr = &mon_list[i];
			
			/* Skip dead monsters */
			if (!n_ptr->r_idx) continue;
			
			if ((n_ptr->r_idx == m_ptr->r_idx) && one_in_(2)) leader_ptr = n_ptr;
		}		
	}
	
	// find a new index if one is not specified
	if (leader_ptr != NULL)
	{
		i = leader_ptr->wandering_idx;
	}
	else
	{
		// clear the index array
		for (i = 0; i <= FLOW_WANDERING_TAIL; i++)
		{
			wandering_indices[i] = FALSE;
		}
		
		// mark the used indices
		for (i = 1; i < mon_max; i++)
		{
			monster_type *n_ptr = &mon_list[i];
			
			/* Skip dead monsters */
			if (!n_ptr->r_idx) continue;
			
			wandering_indices[n_ptr->wandering_idx] = TRUE;
		}
		
		// find the smallest unused index
		for (i = FLOW_WANDERING_HEAD; i <= FLOW_WANDERING_TAIL; i++)
		{
			if (!wandering_indices[i]) break;
		}
	}
	
	// if we have a valid index, then find a location and build the noise flow
	if (i <= FLOW_WANDERING_TAIL)
	{
		m_ptr->wandering_idx = i;
		m_ptr->wandering_dist = MON_WANDER_RANGE;
		new_wandering_flow(m_ptr, 0, 0);
	}

	// if we can't store any more indices, then just set it to zero, which means
    // that the monster will just move randomly and won't wander properly
	// this is very rare, but does occasionally happen (1 in 100 deep levels?)
	else
	{
        //msg_debug("Out of wandering monster indices.");
		m_ptr->wandering_idx = 0;
		m_ptr->wandering_dist = MON_WANDER_RANGE;
	}
	
}

/*
 * Makes Morgoth drop his Iron Crown with an appropriate message.
 */

void drop_iron_crown(monster_type *m_ptr, const char *msg)
{
	int i, near_y, near_x;
	
	if ((&a_info[ART_MORGOTH_3])->cur_num == 0)
	{
		msg_print(msg);
		
		// choose a nearby location, but not his own square
		for (i = 0; i < 1000; i++)
		{
			near_y = m_ptr->fy + rand_range(-1,1);
			near_x = m_ptr->fx + rand_range(-1,1);
			
			if (((near_y != m_ptr->fy) || (near_x != m_ptr->fx)) && cave_floor_bold(near_y, near_x)) break;					
		}
		
		// drop it there
		create_chosen_artefact(ART_MORGOTH_3, near_y, near_x, TRUE);
		
		// lower Morgoth's protection, remove his light source, increase his will and perception
		(&r_info[R_IDX_MORGOTH])->pd -= 1;
		(&r_info[R_IDX_MORGOTH])->light = 0;
		(&r_info[R_IDX_MORGOTH])->wil += 5;
		(&r_info[R_IDX_MORGOTH])->per += 5;

	}
}

void make_alert(monster_type *m_ptr)
{
	int random_level = rand_range(ALERTNESS_ALERT, ALERTNESS_QUITE_ALERT);
	set_alertness(m_ptr, MAX(m_ptr->alertness, random_level));
}

/*
 * Changes a monster's alertness value and displays any appropriate messages
 */
void set_alertness(monster_type *m_ptr, int alertness)
{
	char m_name[80];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	bool redisplay = FALSE;
	
	// Nothing to be done...
	if (m_ptr->alertness == alertness) return;
	
	// cap the alertness value
	if (alertness < ALERTNESS_MIN) alertness = ALERTNESS_MIN;
	if (alertness > ALERTNESS_MAX) alertness = ALERTNESS_MAX;
	
	/* Get the monster name */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);
	
	// First deal with cases where the monster becomes more alert
	if (m_ptr->alertness < alertness)
	{
		if ((m_ptr->alertness < ALERTNESS_UNWARY) && (alertness >= ALERTNESS_ALERT))
		{
			// Monster must spend its next turn noticing you
			m_ptr->skip_next_turn = TRUE;
			
			// Notice the "waking up and noticing"
			if (m_ptr->ml)
			{
				// Dump a message
				msg_format("%^s wakes up and notices you.", m_name);
				
				// disturb the player
				disturb(1, 0);
				
				// redisplay the monster
				redisplay = TRUE;
			}
		}
		else if ((m_ptr->alertness < ALERTNESS_UNWARY) && (alertness >= ALERTNESS_UNWARY))
		{
			// Notice the "waking up"
			if (m_ptr->ml)
			{
				// Dump a message
				msg_format("%^s wakes up.", m_name);
				
				// disturb the player
				disturb(1, 0);
				
				// redisplay the monster
				redisplay = TRUE;
			}
		}
		else if ((m_ptr->alertness < ALERTNESS_ALERT) && (alertness >= ALERTNESS_ALERT))
		{
			// Monster must spend its next turn noticing you
			m_ptr->skip_next_turn = TRUE;
            
			// Notice the "noticing" (!)
			if (m_ptr->ml)
			{
				// Dump a message
				msg_format("%^s notices you.", m_name);
				
				// disturb the player
				disturb(1, 0);
				
				// redisplay the monster
				redisplay = TRUE;
			}
		}
		else if ((m_ptr->alertness < ALERTNESS_UNWARY) && (alertness < ALERTNESS_UNWARY) && (alertness >= ALERTNESS_UNWARY - 2))
		{
			// Notice the "stirring"
			if (m_ptr->ml)
			{
				// Dump a message
				msg_format("%^s stirs.", m_name);
			}
		}
		else if ((m_ptr->alertness < ALERTNESS_ALERT) && (alertness < ALERTNESS_ALERT) && (alertness >= ALERTNESS_ALERT - 2))
		{
			// Notice the "looking around"
			if (m_ptr->ml)
			{
				// Dump a message
				msg_format("%^s looks around.", m_name);
			}
		}
	}
	// First deal with cases where the monster becomes less alert
	else
	{
		if ((m_ptr->alertness >= ALERTNESS_UNWARY) && (alertness < ALERTNESS_UNWARY))
		{
			// Notice the falling asleep
			if (m_ptr->ml)
			{
				// Dump a message
				msg_format("%^s falls asleep.", m_name);
				
				// Morgoth drops his iron crown if he falls asleep
				if (m_ptr->r_idx == R_IDX_MORGOTH)
				{
					drop_iron_crown(m_ptr, "His crown slips from off his brow and falls to the ground nearby.");
				}

				// redisplay the monster
				redisplay = TRUE;
			}
		}
		else if ((m_ptr->alertness >= ALERTNESS_ALERT) && (alertness < ALERTNESS_ALERT))
		{
			// Notice the becoming unwary
			if (m_ptr->ml)
			{
				// Dump a message
				msg_format("%^s becomes unwary.", m_name);

				// redisplay the monster
				redisplay = TRUE;

				// give the monster a new place to wander towards
				if (!(r_ptr->flags2 & (RF2_TERRITORIAL))) new_wandering_flow(m_ptr, p_ptr->py, p_ptr->px);
				
			}
		}
		else if (alertness < ALERTNESS_UNWARY)
		{
			// Notice the deepening sleep
			if (m_ptr->ml)
			{
				// Dump a message
				//msg_format("%^s's sleep deepens.", m_name);
			}
		}
		else if (alertness < ALERTNESS_ALERT)
		{
			// Notice the increasing unwariness
			if (m_ptr->ml)
			{
				// Dump a message
				//msg_format("%^s becomes more unwary.", m_name);
			}
		}
		else
		{
			// Notice the decreasing alertness
			if (m_ptr->ml)
			{
				// Dump a message
				//msg_format("%^s looks less alert.", m_name);
			}
		}
	}
	
	// do the actual alerting
	m_ptr->alertness = alertness;
	
	// redisplay the monster
	if (redisplay) lite_spot(m_ptr->fy, m_ptr->fx);

}


/*
 * Determines the chance of a skill or hit roll succeeding.
 * (1 d sides + skill) - (1 d sides + difficulty)
 * Results <= 0 count as fails.
 * Results > 0 are successes.
 *
 * returns the number of ways you could succeed
 * (i.e. number of chances out of sides*sides
 *
 * note that this will be a percentage for normal skills (10 sides)
 * but will be out of 400 for hit rolls
 */
extern int success_chance(int sides, int skill, int difficulty)
{
	int i, j;
	int ways = 0;
		
	for (i=1; i<=sides; i++)
		for (j=1; j<=sides; j++)
			if (i + skill > j + difficulty)
				ways++;

	return ways;
}



/*
 * Determine the result of a skill check.
 * (1d10 + skill) - (1d10 + difficulty)
 * Results <= 0 count as fails.
 * Results > 0 are successes.
 *
 * There is a fake skill check in monster_perception (where player roll is used once for all monsters)
 * so if something changes here, remember to change it there.
 */
int skill_check(monster_type *m_ptr1, int skill, int difficulty, monster_type *m_ptr2)
{
	int skill_total;
	int difficulty_total;
	int skill_total_alt;
	int difficulty_total_alt;

	// bonuses against your enemy of choice
	if ((m_ptr1 == PLAYER) && (m_ptr2 != NULL)) skill += bane_bonus(m_ptr2);
	if ((m_ptr2 == PLAYER) && (m_ptr1 != NULL)) difficulty += bane_bonus(m_ptr1);
    
    // elf-bane bonus against you
	if ((m_ptr1 == PLAYER) && (m_ptr2 != NULL)) difficulty += elf_bane_bonus(m_ptr2);
	if ((m_ptr2 == PLAYER) && (m_ptr1 != NULL)) skill += elf_bane_bonus(m_ptr1);
	
	// the basic rolls
	skill_total = dieroll(10) + skill;
	difficulty_total = dieroll(10) + difficulty;

	// alternate rolls for dealing with the curse
	skill_total_alt = dieroll(10) + skill;
	difficulty_total_alt = dieroll(10) + difficulty;
	
	// player curse?
	if (p_ptr->cursed)
	{ 
		if (m_ptr1 == PLAYER) skill_total = MIN(skill_total, skill_total_alt);
		if (m_ptr2 == PLAYER) difficulty_total = MIN(difficulty_total, difficulty_total_alt);
	}
	
	/* Debugging message */
	if (cheat_skill_rolls)
	{
		msg_format("{%d+%d v %d+%d = %d}.", skill_total - skill, skill, 
		                                    difficulty_total - difficulty, difficulty, 
											skill_total - difficulty_total);
	}

	return (skill_total - difficulty_total);
}

/*
 * Light hating monsters get a penalty to hit/evn if the player's
 * square is too bright.
 */

extern int light_penalty(const monster_type *m_ptr)
{
	int penalty = 0;
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	
	if (r_ptr->flags3 & (RF3_HURT_LITE))
	{
		penalty = (cave_light[m_ptr->fy][m_ptr->fx] - 2);
		if (penalty < 0) penalty = 0;
	}
	
	return (penalty);
}

/*
 * Determine the result of an attempt to hit an opponent.
 * Results <= 0 count as misses.
 * Results > 0 are hits and, if high enough, are criticals.
 *
 * The monster is the creature doing the attacking. 
 * This is used in displaying the attack roll details.
 * attacker_vis is whether the attacker is visible.
 * this is used in displaying the attack roll details.
 */
int hit_roll(int att, int evn, const monster_type *m_ptr1, const monster_type *m_ptr2, bool display_roll)
{
	int attack_score, attack_score_alt;
	int evasion_score, evasion_score_alt;
	bool non_player_visible;
	
	// determine the visibility for  the combat roll window
	if (m_ptr1 == PLAYER)
	{
		if (m_ptr2 == NULL) non_player_visible = TRUE;
		else				non_player_visible = m_ptr2->ml;
	}
	else
	{
		if (m_ptr1 == NULL) non_player_visible = TRUE;
		else				non_player_visible = m_ptr1->ml;
	}

	// roll the dice...
	attack_score = dieroll(20) + att;
	attack_score_alt = dieroll(20) + att;
	evasion_score = dieroll(20) + evn;
	evasion_score_alt = dieroll(20) + evn;

	// take the worst of two rolls for cursed players
	if (p_ptr->cursed)
	{
		if (m_ptr1 == PLAYER)
		{
			attack_score = MIN(attack_score, attack_score_alt);
		}
		else
		{
			evasion_score = MIN(evasion_score, evasion_score_alt);
		}
	}
	
	// set the information for the combat roll window
	if (display_roll)
	{
		update_combat_rolls1(m_ptr1, m_ptr2, non_player_visible, att, attack_score - att, evn, evasion_score - evn);
	}
	
	return (attack_score - evasion_score);
}



/*
 * Determines the player's evasion based on all the relevant attributes and modifiers.
 */

int total_player_attack(monster_type *m_ptr, int base)
{ 
	int att = base;

	// reward concentration ability (if applicable)
	att += concentration_bonus(m_ptr->fy, m_ptr->fx);

	// reward focused attack ability (if applicable)
	att += focused_attack_bonus();
	
	// reward bane ability (if applicable)
	att += bane_bonus(m_ptr);

	// reward master hunter ability (if applicable)
	att += master_hunter_bonus(m_ptr);
	
	// penalise distance -- note that this penalty will equal 0 in melee
	att -= distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) / 5;
		
	// halve attack score for certain situations (and only halve positive scores!)
	if (att > 0)
	{
		// penalise the player if (s)he can't see the monster
		if (!m_ptr->ml) att /= 2;
		
		// penalise the player if (s)he is in a pit or web
		if (cave_pit_bold(p_ptr->py,p_ptr->px) || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB))
		{
			att /= 2;
		}
	}
	
	return (att);
}


/*
 * Determines the player's evasion based on all the relevant attributes and modifiers.
 */

int total_player_evasion(monster_type *m_ptr, bool archery)
{ 
	int evn = p_ptr->skill_use[S_EVN];
	
	// reward successful use of the dodging ability 
	evn += dodging_bonus();
	
	// reward successful use of the bane ability
	evn += bane_bonus(m_ptr);

	// halve evasion for certain situations (and only halve positive evasion!)
	if (evn > 0)
	{
		// penalise the player if (s)he can't see the monster
		if (!m_ptr->ml) evn /= 2;
		
		// penalise targets of archery attacks
		if (archery) evn /= 2;

		// penalise the player if (s)he is in a pit or web
		if (cave_pit_bold(p_ptr->py,p_ptr->px) || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB))
		{
			evn /= 2;
		}
	}
		
	return (evn);
}


/*
 * Determines a monster's attack score based on all the relevant attributes and modifiers.
 */

int total_monster_attack(monster_type *m_ptr, int base)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	int att = base;
	bool unseen = FALSE;
		
	// penalise stunning 
	if (m_ptr->stunned) att -= 2;
	
	// penalise being in bright light for light-averse monsters
	att -= light_penalty(m_ptr);
	
	// reward surrounding the player
	att += overwhelming_att_mod(m_ptr);

	// penalise distance
	att -= distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) / 5;
    
    // elf-bane bonus
    att += elf_bane_bonus(m_ptr);
	
	// halve attack score for certain situations (and only halve positive scores!)
	if (att > 0)
	{
		// check if player is unseen
		if ((r_ptr->light > 0) && strchr("@G", r_ptr->d_char) && (cave_light[p_ptr->py][p_ptr->px] <= 0)) unseen = TRUE;
		
		// penalise monsters who can't see the player
		if (unseen) att /= 2;
	}

	return (att);
}


/*
 * Determines a monster's evasion based on all the relevant attributes and modifiers.
 */

int total_monster_evasion(monster_type *m_ptr, bool archery)
{ 
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	int evn = r_ptr->evn;
	bool unseen = FALSE;
	
	// penalise stunning
	if (m_ptr->stunned) evn -= 2;
	
	// penalise being in bright light for light-averse monsters
	evn -= light_penalty(m_ptr);
	
    // elf-bane bonus
    evn += elf_bane_bonus(m_ptr);
	
    // halve evasion for certain situations (and only halve positive evasion!)
	if (evn > 0)
	{
		// check if player is unseen
		if ((r_ptr->light > 0) && strchr("@G", r_ptr->d_char) && (cave_light[p_ptr->py][p_ptr->px] <= 0)) unseen = TRUE;
		
		// penalise unwary monsters, or those who can't see the player
		if (unseen || (m_ptr->alertness < ALERTNESS_ALERT)) evn /= 2;
		
		// penalise targets of archery attacks
		if (archery) evn /= 2;
	}
	
	// finally, all sleeping monsters have -5 total evasion
	if (m_ptr->alertness < ALERTNESS_UNWARY) evn = -5;
	
	return (evn);
}


/*
 * Monsters are already given a large set penalty for being asleep
 * (total evasion mod of -5) and unwary (evasion score / 2),
 * but we also give a bonus for high stealth characters who have ASSASSINATION.
 */

int stealth_melee_bonus(const monster_type *m_ptr)
{
	int stealth_bonus = 0;
		
	if (p_ptr->active_ability[S_STL][STL_ASSASSINATION])
	{
		if ((m_ptr->alertness < ALERTNESS_ALERT) && m_ptr->ml && !(p_ptr->confused))
		{
			stealth_bonus = p_ptr->skill_use[S_STL];
		}
	}
	return (stealth_bonus);
}

/*
 * Give a bonus to attack the player depending on the number of adjacent monsters.
 * This is +1 for monsters near the attacker or to the sides,
 * and +2 for monsters in the three positions behind the player:
 * 
 * 1M1  M11
 * 1@1  1@2
 * 222  122
 *
 * We should lessen this with the crowd fighting ability
 */
int overwhelming_att_mod(monster_type *m_ptr)
{
	int mod = 0;
    int dir;
	int dy, dx;
	int py = p_ptr->py;
	int px = p_ptr->px;
	
    // determine the main direction from the player to the monster
    dir = rough_direction(py, px, m_ptr->fy, m_ptr->fx);
    
    // extract the deltas from the direction
    dy = ddy[dir];
    dx = ddx[dir];
	
	// if monster in an orthogonal direction   753
	//                                         8@M
	//                                         642
	if (dy * dx == 0)
	{
		// increase modifier for monsters engaged with the player...
		if (cave_m_idx[py+dx+dy][px-dy+dx] > 0) mod++;    // direction 2 
		if (cave_m_idx[py-dx+dy][px+dy+dx] > 0) mod++;    // direction 3
		if (cave_m_idx[py+dx][px-dy] > 0)       mod++;    // direction 4
		if (cave_m_idx[py-dx][px+dy] > 0)       mod++;    // direction 5
		
		// ...especially if they are behind the player
		if (cave_m_idx[py+dx-dy][px-dy-dx] > 0) mod += 2; // direction 6
		if (cave_m_idx[py-dx-dy][px+dy-dx] > 0) mod += 2; // direction 7
		if (cave_m_idx[py-dy][px-dx] > 0)       mod += 2; // direction 8
	}
	// if monster in a diagonal direction   875
	//                                      6@3
	//                                      42M
	else
	{
		// increase modifier for monsters engaged with the player...
		if (cave_m_idx[py+dy][px] > 0)    mod++;    // direction 2
		if (cave_m_idx[py][px+dx] > 0)    mod++;    // direction 3
		if (cave_m_idx[py+dx][px-dy] > 0) mod++;    // direction 4
		if (cave_m_idx[py-dx][px+dy] > 0) mod++;    // direction 5
		
		// ...especially if they are behind the player
		if (cave_m_idx[py-dy][px] > 0)    mod += 2; // direction 6
		if (cave_m_idx[py][px-dx] > 0)    mod += 2; // direction 7
		if (cave_m_idx[py-dy][px-dx] > 0) mod += 2; // direction 8
	}
	
	// adjust for crowd fighting ability
	if (p_ptr->active_ability[S_EVN][EVN_CROWD_FIGHTING])
	{
		mod /= 2;
	}
	
	return (mod);
}



/*
 * Determines the number of bonus dice from a (potentially) critical hit
 *
 * bonus of 1 die for every (6 + weight_in_pounds) over what is needed.
 * (using rounding at 0.5 instead of always rounding up)
 *
 * Thus for a Dagger (0.8lb):         7, 14, 20, 27...  (6+weight)
 *            Short Sword (1.5lb):    8, 15, 23, 30...
 *            Long Sword (3lb):       9, 18, 27, 35...
 *            Bastard Sword (4lb):   10, 20, 30, 40...
 *            Great Sword (7lb):     13, 26, 39, 52...
 *            Shortbow (2lb):         8, 16, 24, 32...
 *            Longbow (3lb):          9, 18, 27, 36...
 *            m 1dX (2lb):            8, 16, 24, 32...
 *            m 2dX (4lb):           10, 20, 30, 40...
 *            m 3dX (6lb):           12, 24, 36, 48...
 *
 * (old versions)
 * Thus for a Dagger (0.8lb):         9, 13, 17, 21...  5 then (3+weight)
 *            Short Sword (1.5lb):   10, 14, 19, 23...
 *            Long Sword (3lb):      11, 17, 23, 29...
 *            Bastard Sword (4lb):   12, 19, 26, 33...
 *            Great Sword (7lb):     15, 25, 35, 45...
 *            Shortbow (2lb):        10, 15, 20, 25...
 *            Longbow (3lb):         11, 17, 23, 29...
 *            m 1dX (2lb):           10, 15, 20, 25...
 *            m 2dX (4lb):           12, 19, 26, 33...
 *            m 3dX (6lb):           14, 23, 32, 41...
 * Thus for a Dagger (0.8lb):        11, 12, 13, 14...  (10 then weightx)
 *            Short Sword (1.5lb):   12, 13, 15, 16...
 *            Long Sword (3lb):      13, 16, 19, 22...
 *            Bastard Sword (4lb):   14, 18, 22, 26...
 *            Great Sword (7lb):     17, 24, 31, 38...
 *            Shortbow (2lb):        12, 14, 16, 18...
 *            Longbow (3lb):         13, 16, 19, 22...
 * Thus for a Dagger (0.8lb):         6, 12, 18, 24...  (5+weight)
 *            Short Sword (1.5lb):    7, 13, 20, 26...
 *            Long Sword (3lb):       8, 16, 24, 32...
 *            Bastard Sword (4lb):    9, 18, 27, 36...
 *            Great Sword (7lb):     12, 24, 36, 48...
 *            Shortbow (2lb):         7, 14, 21, 28...
 *            Longbow (3lb):          8, 16, 24, 32...
 * Thus for a Dagger (0.8lb):         4,  8, 12, 16...  (3+weight)
 *            Short Sword (1.5lb):    5,  9, 14, 18...
 *            Long Sword (3lb):       6, 12, 18, 25...
 *            Bastard Sword (4lb):    7, 14, 21, 28...
 *            Great Sword (7lb):     10, 20, 30, 40...
 *            Shortbow (2lb):         5, 10, 15, 20...
 *            Longbow (3lb):          6, 12, 18, 24...
 * Thus for a Dagger (0.8lb):         8, 12, 15, 18...  (old1)
 *            Short Sword (1.5lb):    9, 14, 18, 23...
 *            Long Sword (3lb):      11, 17, 23, 29...
 *            Bastard Sword (3.5lb): 11, 18, 24, 31...
 *            Great Sword (7lb):     15, 25, 35, 45...
 * Thus for a Dagger (0.8lb):         7, 10, 12, 14...  (old2)
 *            Short Sword (1.5lb):    8, 12, 15, 19...
 *            Long Sword (3lb):      10, 15, 20, 25...
 *            Bastard Sword (3.5lb): 10, 16, 21, 27...
 *            Great Sword (7lb):     14, 23, 32, 41...
 */
int crit_bonus(int hit_result, int weight, const monster_race *r_ptr, int skill_type, bool thrown)
{
	int crit_bonus_dice;
	int crit_seperation = 70;
		
	// When attacking a monster...
	if (r_ptr->level != 0)
	{
		// Can have improved criticals for melee
		if ((skill_type == S_MEL) && p_ptr->active_ability[S_MEL][MEL_FINESSE])				crit_seperation -= 10;
			
		// Can have improved criticals for melee with one handed weapons
		if ((skill_type == S_MEL) && p_ptr->active_ability[S_MEL][MEL_CONTROL] &&
			!thrown && !two_handed_melee() && !inventory[INVEN_ARM].k_idx)					crit_seperation -= 20;
		
		// Can have inferior criticals for melee
		if ((skill_type == S_MEL) && p_ptr->active_ability[S_MEL][MEL_POWER])				crit_seperation += 10;
		
		// Can have improved criticals for archery
		if ((skill_type == S_ARC) && p_ptr->active_ability[S_ARC][ARC_IMPROVED_CRITICALS])	crit_seperation -= 10;
	}
	// When attacking the player...
	else
	{
		// Resistance to criticals increases what they need for each bonus die
		if (p_ptr->active_ability[S_WIL][WIL_CRITICAL_RESISTANCE]) crit_seperation += (p_ptr->skill_use[S_WIL] / 5) * 10;	
	}

	// note: the +4 in this calculation is for rounding purposes
	crit_bonus_dice = (hit_result * 10 + 4) / (crit_seperation + weight);

	// When attacking a monster...
	if (r_ptr->level != 0)
	{
		// Resistance to criticals doubles what you need for each bonus die
		if (r_ptr->flags1 & (RF1_RES_CRIT)) crit_bonus_dice /= 2;

		// certain creatures cannot suffer crits as they have no vulnerable areas
		if (r_ptr->flags1 & (RF1_NO_CRIT)) crit_bonus_dice = 0;
	}
	
	// can't have fewer than zero dice
	if (crit_bonus_dice < 0) crit_bonus_dice = 0;
	
	return crit_bonus_dice;
}

/*
 * Describes the effect of a slay
 */
void slay_desc(char *description, u32b flag, const monster_type *m_ptr)
{
	char m_name[80];
	
	/* Monster description */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	switch (flag)
	{
		case TR1_SHARPNESS:
        	sprintf(description, "cuts deeply");
			break;
		case TR1_SHARPNESS2:
        	sprintf(description, "cuts effortlessly");
			break;
		case TR1_VAMPIRIC:
        	sprintf(description, "drains life from %s", m_name);
			break;
		case TR1_SLAY_ORC:
        	sprintf(description, "strikes truly");
			break;
		case TR1_SLAY_WOLF:
        	sprintf(description, "strikes truly");
			break;
		case TR1_SLAY_SPIDER:
        	sprintf(description, "strikes truly");
			break;
		case TR1_SLAY_UNDEAD:
        	sprintf(description, "strikes truly");
			break;
		case TR1_SLAY_RAUKO:
        	sprintf(description, "strikes truly");
			break;
		case TR1_SLAY_DRAGON:
        	sprintf(description, "strikes truly");
			break;
		case TR1_SLAY_TROLL:
        	sprintf(description, "strikes truly");
			break;
		case TR1_BRAND_ELEC:
        	sprintf(description, "shocks %s with the force of lightning", m_name);
			break;
		case TR1_BRAND_FIRE:
        	sprintf(description, "burns %s with an inner fire", m_name);
			break;
		case TR1_BRAND_COLD:
        	sprintf(description, "freezes %s", m_name);
			break;
		case TR1_BRAND_POIS:
        	sprintf(description, "poisons %s", m_name);
			break;
	}
	
	return;
}


extern void ident(object_type *o_ptr)
{	
	/* Identify it */
	object_aware(o_ptr);
	object_known(o_ptr);
	
	/* Apply an autoinscription, if necessary */
	apply_autoinscription(o_ptr);
	
	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);
	
	/* Combine / Reorder the pack (later) */
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);
	
	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
		
	return;
}

extern void ident_on_wield(object_type *o_ptr)
{
	u32b f1, f2, f3;
		
	bool notice = FALSE;
	
	char o_full_name[80];
	
	object_kind *k_ptr = &k_info[o_ptr->k_idx];
	
	/* Get the flags */
	object_flags(o_ptr, &f1, &f2, &f3);
	
	// Ignore previously identified items
	if (object_known_p(o_ptr))
	{
		return;
	}
	
	// identify the special item types that do nothing much
	// (since they have no hidden abilities, they must already be obvious)
	if (o_ptr->name2)
	{
		ego_item_type *e_ptr = &e_info[o_ptr->name2];
				
		if ((e_ptr->flags1 == 0L) && (e_ptr->flags2 == 0L) && 
		    ((e_ptr->flags3 | (TR3_IGNORE_ALL)) == (TR3_IGNORE_ALL)) && (e_ptr->abilities == 0))
		{
			notice = TRUE;
		}
	}

    // identify true sight if it cures blindness
	if (p_ptr->blind && (f2 & (TR2_SEE_INVIS)))
	{
		notice = TRUE;
	}
    
	if (o_ptr->name1 || o_ptr->name2)
	{
		// For special items and artefacts, we need to ignore the flags that are basic 
		// to the object type and focus on the special/artefact ones. We can do this by 
		// subtracting out the basic flags
		
		f1 &= ~(k_ptr->flags1);
		f2 &= ~(k_ptr->flags2);
		f3 &= ~(k_ptr->flags3);
	}
	
	if (f2 & (TR2_DARKNESS))
	{
		notice = TRUE;
		msg_print("It creates an unnatural darkness.");
	}
	else if (f2 & (TR2_LIGHT))
	{
		if (o_ptr->tval != TV_LIGHT)
		{
			notice = TRUE;
			msg_print("It glows with a wondrous light.");
		}
		else if ((o_ptr->sval == SV_LIGHT_FEANORIAN) || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL) || (o_ptr->timeout > 0))
		{
			notice = TRUE;
			msg_print("It glows very brightly.");
		}
	}
	else if (f2 & (TR2_SLOWNESS))
	{
		notice = TRUE;
		msg_print("It slows your movement.");
	}
	else if (f2 & (TR2_SPEED))
	{
		notice = TRUE;
		msg_print("It speeds your movement.");
	}

	else if (f1 & (TR1_DAMAGE_SIDES))
	{
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (o_ptr->pval > 0)
		{
			notice = TRUE;
			msg_print("You feel more forceful in melee.");
		}
		else if (o_ptr->pval < 0)
		{
			notice = TRUE;
			msg_print("You feel less forceful in melee.");
		}
	}
	else if ((f1 & (TR1_STR)) || (f1 & (TR1_NEG_STR)))
	{
		int bonus = (f1 & (TR1_STR)) ? o_ptr->pval : -(o_ptr->pval);
		
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (bonus > 0)
		{
			notice = TRUE;
			msg_print("You feel stronger.");
		}
		else if (bonus < 0)
		{
			notice = TRUE;
			msg_print("You feel less strong.");
		}
	}
	else if ((f1 & (TR1_DEX)) || (f1 & (TR1_NEG_DEX)))
	{
		int bonus = (f1 & (TR1_DEX)) ? o_ptr->pval : -(o_ptr->pval);
		
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (bonus > 0)
		{
			notice = TRUE;
			msg_print("You feel more agile.");
		}
		else if (bonus < 0)
		{
			notice = TRUE;
			msg_print("You feel less agile.");
		}
	}
	else if ((f1 & (TR1_CON)) || (f1 & (TR1_NEG_CON)))
	{
		int bonus = (f1 & (TR1_CON)) ? o_ptr->pval : -(o_ptr->pval);
		
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (bonus > 0)
		{
			notice = TRUE;
			msg_print("You feel more resilient.");
		}
		else if (bonus < 0)
		{
			notice = TRUE;
			msg_print("You feel less resilient.");
		}
	}
	else if ((f1 & (TR1_GRA)) || (f1 & (TR1_NEG_GRA)))
	{
		int bonus = (f1 & (TR1_GRA)) ? o_ptr->pval : -(o_ptr->pval);
		
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (bonus > 0)
		{
			notice = TRUE;
			msg_print("You feel more attuned to the world.");
		}
		else if (bonus < 0)
		{
			notice = TRUE;
			msg_print("You feel less attuned to the world.");
		}
	}
	else if (f1 & (TR1_MEL))
	{
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (o_ptr->pval > 0)
		{
			notice = TRUE;
			msg_print("You feel more in control of your weapon.");
		}
		else if (o_ptr->pval < 0)
		{
			notice = TRUE;
			msg_print("You feel less in control of your weapon.");
		}
	}
	else if (f1 & (TR1_ARC))
	{
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (o_ptr->pval > 0)
		{
			notice = TRUE;
			msg_print("You feel more accurate at archery.");
		}
		else if (o_ptr->pval < 0)
		{
			notice = TRUE;
			msg_print("You feel less accurate at archery.");
		}
	}
	else if (f1 & (TR1_STL))
	{
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (o_ptr->pval > 0)
		{
			notice = TRUE;
			msg_print("Your movements become quieter.");
		}
		else if (o_ptr->pval < 0)
		{
			notice = TRUE;
			msg_print("Your movements less quiet.");
		}
	}
	else if (f1 & (TR1_PER))
	{
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (o_ptr->pval > 0)
		{
			notice = TRUE;
			msg_print("You feel more perceptive.");
		}
		else if (o_ptr->pval < 0)
		{
			notice = TRUE;
			msg_print("You feel less perceptive.");
		}
	}
	else if (f1 & (TR1_WIL))
	{
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (o_ptr->pval > 0)
		{
			notice = TRUE;
			msg_print("You feel more firm of will.");
		}
		else if (o_ptr->pval < 0)
		{
			notice = TRUE;
			msg_print("You feel less firm of will.");
		}
	}
	else if (f1 & (TR1_SMT))
	{
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (o_ptr->pval > 0)
		{
			notice = TRUE;
			msg_print("You feel a desire to craft things with your hands.");
		}
		else if (o_ptr->pval < 0)
		{
			notice = TRUE;
			msg_print("You feel less able to craft things.");
		}
	}
	else if (f1 & (TR1_SNG))
	{
		// can identify <+0> items if you already know the flavour
		if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (o_ptr->pval > 0)
		{
			notice = TRUE;
			msg_print("You are filled with inspiration.");
		}
		else if (o_ptr->pval < 0)
		{
			notice = TRUE;
			msg_print("You feel a loss of inspiration.");
		}
	}

	// identify the special item types that grant abilities
	else if (o_ptr->name2)
	{
		ego_item_type *e_ptr = &e_info[o_ptr->name2];
			
		if (e_ptr->abilities > 0)
		{
			notice = TRUE;
			msg_format("You have gained the ability '%s'.", 
			           b_name + (&b_info[ability_index(e_ptr->skilltype[0], e_ptr->abilitynum[0])])->name);
		}
	}

	// identify the artefacts that grant abilities
	else if (o_ptr->name1)
	{
		artefact_type *a_ptr = &a_info[o_ptr->name1];
		
		if (a_ptr->abilities > 0)
		{
			notice = TRUE;
			msg_format("You have gained the ability '%s'.", 
					   b_name + (&b_info[ability_index(a_ptr->skilltype[0], a_ptr->abilitynum[0])])->name);
		}
	}
	
    // can identify <+0> items if you already know the flavour
	else if (k_info[o_ptr->k_idx].flavor)
	{
		if (object_aware_p(o_ptr))
		{
			notice = TRUE;
		}
		else if (o_ptr->att > 0)
		{
			notice = TRUE;
			msg_print("You somehow feel more accurate in combat.");
		}
		else if (o_ptr->att < 0)
		{
			notice = TRUE;
			msg_print("You somehow feel less accurate in combat.");
		}
		else if (o_ptr->evn > 0)
		{
			notice = TRUE;
			msg_print("You somehow feel harder to hit.");
		}
		else if (o_ptr->evn < 0)
		{
			notice = TRUE;
			msg_print("You somehow feel more vulnerable.");
		}
		else if (o_ptr->pd > 0)
		{
			notice = TRUE;
			msg_print("You somehow feel more protected.");
		}
	}
    
				
				 
	if (notice)
	{
		/* identify the object */
		ident(o_ptr);
		
		/* Full object description */
		object_desc(o_full_name, sizeof(o_full_name), o_ptr, TRUE, 3);
		
		/* Print the messages */
		msg_format("You recognize it as %s.", o_full_name);
	}
	
	return;
}

extern void ident_resist(u32b flag)
{
	u32b f1, f2, f3;
	
	int i;
	
	bool notice = FALSE;
	
	char effect_string[80];
	char o_full_name[80];
	char o_short_name[80];
	
	object_type *o_ptr;
	object_kind *k_ptr;
		
	/* Scan the equipment */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
		k_ptr = &k_info[o_ptr->k_idx];
		
		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;
		
		/* Extract the item flags */
		object_flags(o_ptr, &f1, &f2, &f3);

		if (o_ptr->name1 || o_ptr->name2)
		{
			// For special items and artefacts, we need to ignore the flags that are basic 
			// to the object type and focus on the special/artefact ones. We can do this by 
			// subtracting out the basic flags
			
			f1 &= ~(k_ptr->flags1);
			f2 &= ~(k_ptr->flags2);
			f3 &= ~(k_ptr->flags3);
		}
		
		if (!object_known_p(o_ptr))
		{
			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), o_ptr, FALSE, 0);
			
			if ((flag == TR2_RES_COLD) && (f2 & (TR2_RES_COLD)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s partly protects you from the chill.", o_short_name);
			}
			else if ((flag == TR2_RES_FIRE) && (f2 & (TR2_RES_FIRE)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s partly protects you from the flame.", o_short_name);
			}
			else if ((flag == TR2_RES_POIS) && (f2 & (TR2_RES_POIS)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s partly protects you from the poison.", o_short_name);
			}
			else if ((flag == TR2_RES_COLD) && (f2 & (TR2_VUL_COLD)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s intensifies the chill.", o_short_name);
			}
			else if ((flag == TR2_RES_FIRE) && (f2 & (TR2_VUL_FIRE)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s intensifies the flame.", o_short_name);
			}
			else if ((flag == TR2_RES_POIS) && (f2 & (TR2_VUL_POIS)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s intensifies the poison.", o_short_name);
			}
			else if ((flag == TR2_RES_FEAR) && (f2 & (TR2_RES_FEAR)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s fills you with courage.", o_short_name);
			}
			else if ((flag == TR2_RES_BLIND) && (f2 & (TR2_RES_BLIND)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s protects your sight.", o_short_name);
			}
			else if ((flag == TR2_RES_HALLU) && (f2 & (TR2_RES_HALLU)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s protects your sight.", o_short_name);
			}
			else if ((flag == TR2_RES_CONFU) && (f2 & (TR2_RES_CONFU)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s fills you with calm.", o_short_name);
			}
			else if ((flag == TR2_RES_STUN) && (f2 & (TR2_RES_STUN)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s fills you with calm.", o_short_name);
			}
			else if ((flag == TR2_FREE_ACT) && (f2 & (TR2_FREE_ACT)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s glows softly.", o_short_name);
			}
			else if ((flag == TR2_SUST_STR) && (f2 & (TR2_SUST_STR)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s sustains your strength.", o_short_name);
			}
			else if ((flag == TR2_SUST_DEX) && (f2 & (TR2_SUST_DEX)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s sustains your dexterity.", o_short_name);
			}
			else if ((flag == TR2_SUST_CON) && (f2 & (TR2_SUST_CON)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s sustains your constitution.", o_short_name);
			}
			else if ((flag == TR2_SUST_GRA) && (f2 & (TR2_SUST_GRA)))
			{
				notice = TRUE;
				strnfmt(effect_string, sizeof(effect_string), "Your %s sustains your grace.", o_short_name);
			}
			
		}
		
		if (notice)
		{
			/* identify the object */
			ident(o_ptr);
			
			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), o_ptr, TRUE, 3);
			
			/* Print the messages */
			msg_format("%s", effect_string);
			msg_format("You realize that it is %s.", o_full_name);
			
			return;
		}		
	}
	
	return;
}


extern void ident_passive(void)
{
	u32b f1, f2, f3;
	
	int i;
	
	bool notice = FALSE;
	
	char effect_string[80];
	char o_full_name[80];
	char o_short_name[80];
	
	object_type *o_ptr;
		
	/* Scan the equipment */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
		
		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;
		
		/* Extract the item flags */
		object_flags(o_ptr, &f1, &f2, &f3);
		
		if (!object_known_p(o_ptr))
		{			
			if ((f2 & (TR2_REGEN)) && (p_ptr->chp < p_ptr->mhp))
			{
				notice = TRUE;
				my_strcpy(effect_string, "You notice that you are recovering much faster than usual.", sizeof (effect_string));
			}
			else if ((f2 & (TR2_AGGRAVATE)))
			{
				notice = TRUE;
				my_strcpy(effect_string, "You notice that you are enraging your enemies.", sizeof (effect_string));
			}
			else if ((f2 & (TR2_DANGER)))
			{
				notice = TRUE;
				my_strcpy(effect_string, "You notice that you are attracting more powerful enemies.", sizeof (effect_string));
			}
		}
		
		if (notice)
		{
			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), o_ptr, FALSE, 0);
			
			/* identify the object */
			ident(o_ptr);
			
			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), o_ptr, TRUE, 3);
			
			/* Print the messages */
			msg_format("%s", effect_string);
			msg_format("You realize that your %s is %s.", o_short_name, o_full_name);
			
			return;
		}		
	}
	
	return;
}


extern void ident_see_invisible(const monster_type *m_ptr)
{
	u32b f1, f2, f3;
	
	int i;
	
	bool notice = FALSE;
	
	char m_name[80];
	char o_full_name[80];
	char o_short_name[80];
	
	object_type *o_ptr;
	
	/* Scan the equipment */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
		
		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;
		
		/* Extract the item flags */
		object_flags(o_ptr, &f1, &f2, &f3);
		
		if (!object_known_p(o_ptr))
		{
			if ((f2 & (TR2_SEE_INVIS)))
			{
				notice = TRUE;
			}
		}
		
		if (notice)
		{
			/* Get the monster name */
			monster_desc(m_name, sizeof(m_name), m_ptr, 0);
			
			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), o_ptr, FALSE, 0);
			
			/* identify the object */
			ident(o_ptr);
			
			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), o_ptr, TRUE, 3);
			
			/* Print the messages */
			msg_format("You notice that you can see %s very clearly.", m_name);
			msg_format("You realize that your %s is %s.", o_short_name, o_full_name);
			
			return;
		}		
	}
	
	return;
}

extern void ident_haunted(void)
{
	u32b f1, f2, f3;
	
	int i;
	
	bool notice = FALSE;
	
	char o_full_name[80];
	char o_short_name[80];
	
	object_type *o_ptr;
	
	/* Scan the equipment */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
		
		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;
		
		/* Extract the item flags */
		object_flags(o_ptr, &f1, &f2, &f3);
		
		if (!object_known_p(o_ptr))
		{
			if ((f2 & (TR2_HAUNTED)))
			{
				notice = TRUE;
			}
		}
		
		if (notice)
		{
			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), o_ptr, FALSE, 0);
			
			/* identify the object */
			ident(o_ptr);
			
			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), o_ptr, TRUE, 3);
			
			/* Print the messages */
			msg_print("You notice that wraiths are being drawn to you.");
			msg_format("You realize that your %s is %s.", o_short_name, o_full_name);
			
			return;
		}		
	}
	
	return;
}


extern void ident_cowardice(void)
{
	u32b f1, f2, f3;
	
	int i;
	
	bool notice = FALSE;
	
	char o_full_name[80];
	char o_short_name[80];
	
	object_type *o_ptr;
		
	/* Scan the equipment */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
		
		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;
		
		/* Extract the item flags */
		object_flags(o_ptr, &f1, &f2, &f3);
		
		if (!object_known_p(o_ptr))
		{
			if ((f2 & (TR2_FEAR)))
			{
				notice = TRUE;
			}
		}
		
		if (notice)
		{
			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), o_ptr, FALSE, 0);
			
			/* identify the object */
			ident(o_ptr);
			
			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), o_ptr, TRUE, 3);
			
			/* Print the message */
			msg_format("You realize that your %s is %s.", o_short_name, o_full_name);
			
			return;
		}		
	}
	
	return;
}

/* 
 * Identifies a hunger or sustenance item and prints a message
 */
void ident_hunger(void)
{
	u32b f1, f2, f3;
	int i;
	bool notice = FALSE;
	char o_full_name[80];
	char o_short_name[80];
	object_type *o_ptr;
    
	/* Scan the equipment */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];

		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;

		/* Extract the item flags */
		object_flags(o_ptr, &f1, &f2, &f3);

		if (!object_known_p(o_ptr))
		{
			if ((f2 & (TR2_HUNGER)) && (p_ptr->hunger > 0))
            {
				notice = TRUE;
            }
                
            if ((f2 & (TR2_SLOW_DIGEST)) && (p_ptr->hunger < 0))
			{
				notice = TRUE;
			}
		}

		if (notice)
		{
			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), o_ptr, FALSE, 0);

			/* identify the object */
			ident(o_ptr);

			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), o_ptr, TRUE, 3);

			/* Print the messages */
            if (f2 & (TR2_HUNGER))              msg_print("You notice that you are growing hungry much faster than before.");
            else if (f2 & (TR2_SLOW_DIGEST))    msg_print("You notice that you are growing hungry slower than before.");
            
			msg_format("You realize that your %s is %s.", o_short_name, o_full_name);

			return;
		}
	}

	return;
}

/*
 * Identifies a weapon from one of its slays being active and prints a message
 */
void ident_weapon_by_use(object_type *o_ptr, const monster_type *m_ptr, u32b flag)
{	
	char o_short_name[80];
	char o_full_name[80];
	char slay_description[160];
	
	/* Short, pre-identification object description */
	object_desc(o_short_name, sizeof(o_short_name), o_ptr, FALSE, 0);
	
	/* identify the object */
	ident(o_ptr);
	
	/* Full object description */
	object_desc(o_full_name, sizeof(o_full_name), o_ptr, TRUE, 3);
	
	/* Description of the 'slay' */
	slay_desc(slay_description, flag, m_ptr);
	
	/* Print the messages */
	msg_format("Your %s %s.", o_short_name, slay_description);
	msg_format("You recognize it as %s.", o_full_name);
	
	return;
}


void ident_bow_arrow_by_use(object_type *j_ptr, object_type *i_ptr, object_type *o_ptr,
                       const monster_type *m_ptr, u32b bow_flag, u32b arrow_flag)
{
	char i_short_name[80];
	char i_full_name[80];
	char j_short_name[80];
	char j_full_name[80];
	char slay_description[160];

	/* Short, pre-identification bow and arrow description */
	object_desc(j_short_name, sizeof(j_short_name), j_ptr, FALSE, 0);
	object_desc(i_short_name, sizeof(i_short_name), i_ptr, FALSE, 0);

	if (arrow_flag)
	{

		/* Identify the arrow and remaining arrows */
		object_aware(i_ptr);
		object_known(i_ptr);
		object_aware(o_ptr);
		object_known(o_ptr);

		/* Apply an autoinscription, if necessary */
		apply_autoinscription(i_ptr);
		apply_autoinscription(o_ptr);

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Combine / Reorder the pack (later) */
		p_ptr->notice |= (PN_COMBINE | PN_REORDER);

		/* Window stuff */
		p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

		/* Full arrow description */
		object_desc(i_full_name, sizeof(i_full_name), i_ptr, TRUE, 3);

		slay_desc(slay_description, arrow_flag, m_ptr);

		msg_format("Your %s %s.", i_short_name, slay_description);
		msg_format("You recognize it as %s.", i_full_name);

		// don't carry on to identify the bow on the same shot
		return;
	}
	
	
	if (bow_flag)
	{
		/* Identify the bow */
		object_aware(j_ptr);
		object_known(j_ptr);

		/* Apply an autoinscription, if necessary */
		apply_autoinscription(j_ptr);

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Combine / Reorder the pack (later) */
		p_ptr->notice |= (PN_COMBINE | PN_REORDER);

		/* Window stuff */
		p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

		/* Full arrow description */
		object_desc(j_full_name, sizeof(j_full_name), j_ptr, TRUE, 3);

		slay_desc(slay_description, bow_flag, m_ptr);

		msg_format("Your shot %s.", slay_description);
		msg_format("You recognize your %s to be %s.", j_short_name, j_full_name);
	}


	return;


}



/*
 * Makes checks against perception to see if the weapon becomes identified
 *
 * Returns the flag that was noticed, the calling function can send this to
 * ident_weapon_by_use
 */

u32b maybe_notice_slay(const object_type *o_ptr, u32b flag)
{
	u32b noticed_flag = 0L;
	
	if (!object_known_p(o_ptr))
	{
		noticed_flag = flag;
	}
	
	return noticed_flag;
}


/*
 * Determines the number of bonus dice from slays/brands
 *
 * Note that "flasks of oil" do NOT do fire damage, although they
 * certainly could be made to do so.  XXX XXX
 *
 * All 'slays' and 'brands' do one additional die (these are cumulative)
 * 'kills' do an additional two dice.
 */
int slay_bonus(const object_type *o_ptr, const monster_type *m_ptr, u32b *noticed_flag)
{
	int slay_bonus_dice = 0;
	int brand_bonus_dice = 0;

	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];

	u32b f1, f2, f3;

	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);

	/* Some "weapons" and "arrows" do extra damage */
	switch (o_ptr->tval)
	{
		case TV_ARROW:
		case TV_BOW:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_SWORD:
		case TV_DIGGING:
		{
			/* Slay Wolf */
			if ((f1 & (TR1_SLAY_WOLF)) &&
			    (r_ptr->flags3 & (RF3_WOLF)))
			{
				if (m_ptr->ml)
				{
					l_ptr->flags3 |= (RF3_WOLF);
				}

				slay_bonus_dice += 1;
				
				*noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_WOLF);
			}

			/* Slay Spider */
			if ((f1 & (TR1_SLAY_SPIDER)) &&
			    (r_ptr->flags3 & (RF3_SPIDER)))
			{
				if (m_ptr->ml)
				{
					l_ptr->flags3 |= (RF3_SPIDER);
				}

				slay_bonus_dice += 1;
				
				*noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_SPIDER);
			}
			
			/* Slay Undead */
			if ((f1 & (TR1_SLAY_UNDEAD)) &&
			    (r_ptr->flags3 & (RF3_UNDEAD)))
			{
				if (m_ptr->ml)
				{
					l_ptr->flags3 |= (RF3_UNDEAD);
				}

				slay_bonus_dice += 1;
				
				*noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_UNDEAD);
			}

			/* Slay Rauko */
			if ((f1 & (TR1_SLAY_RAUKO)) &&
			    (r_ptr->flags3 & (RF3_RAUKO)))
			{
				if (m_ptr->ml)
				{
					l_ptr->flags3 |= (RF3_RAUKO);
				}

				slay_bonus_dice += 1;

				*noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_RAUKO);
			}

			/* Slay Orc */
			if ((f1 & (TR1_SLAY_ORC)) &&
			    (r_ptr->flags3 & (RF3_ORC)))
			{
				if (m_ptr->ml)
				{
					l_ptr->flags3 |= (RF3_ORC);
				}

				slay_bonus_dice += 1;

				*noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_ORC);
			}

			/* Slay Troll */
			if ((f1 & (TR1_SLAY_TROLL)) &&
			    (r_ptr->flags3 & (RF3_TROLL)))
			{
				if (m_ptr->ml)
				{
					l_ptr->flags3 |= (RF3_TROLL);
				}

				slay_bonus_dice += 1;

				*noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_TROLL);
			}

			/* Slay Dragon */
			if ((f1 & (TR1_SLAY_DRAGON)) &&
			    (r_ptr->flags3 & (RF3_DRAGON)))
			{
				if (m_ptr->ml)
				{
					l_ptr->flags3 |= (RF3_DRAGON);
				}

				slay_bonus_dice += 1;

				*noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_DRAGON);
			}

			/* Brand (Elec) */
			if (f1 & (TR1_BRAND_ELEC))
			{
				/* Notice immunity */
				if (r_ptr->flags3 & (RF3_RES_ELEC))
				{
					if (m_ptr->ml)
					{
						l_ptr->flags3 |= (RF3_RES_ELEC);
					}
				}

				/* Otherwise, take the damage */
				else
				{
					brand_bonus_dice += 1;

					*noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_ELEC);
				}
			}

			/* Brand (Fire) */
			if (f1 & (TR1_BRAND_FIRE))
			{
				/* Notice immunity */
				if (r_ptr->flags3 & (RF3_RES_FIRE))
				{
					if (m_ptr->ml)
					{
						l_ptr->flags3 |= (RF3_RES_FIRE);
					}
				}

				/* Otherwise, take the damage */
				else
				{
					brand_bonus_dice += 1;

					*noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_FIRE);

					// extra bonus against vulnerable creatures
					if (r_ptr->flags3 & (RF3_HURT_FIRE))
					{
						brand_bonus_dice += 1;
						
						/* Memorize the effects */
						l_ptr->flags3 |= (RF3_HURT_FIRE);
					}

				}
			}

			/* Brand (Cold) */
			if (f1 & (TR1_BRAND_COLD))
			{
				/* Notice immunity */
				if (r_ptr->flags3 & (RF3_RES_COLD))
				{
					if (m_ptr->ml)
					{
						l_ptr->flags3 |= (RF3_RES_COLD);
					}
				}

				/* Otherwise, take the damage */
				else
				{
					brand_bonus_dice += 1;

					*noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_COLD);
					
					// extra bonus against vulnerable creatures
					if (r_ptr->flags3 & (RF3_HURT_COLD))
					{
						brand_bonus_dice += 1;
						
						/* Memorize the effects */
						l_ptr->flags3 |= (RF3_HURT_COLD);
					}
				}
			}

			/* Brand (Poison) */
			if (f1 & (TR1_BRAND_POIS))
			{
				/* Notice immunity */
				if (r_ptr->flags3 & (RF3_RES_POIS))
				{
					if (m_ptr->ml)
					{
						l_ptr->flags3 |= (RF3_RES_POIS);
					}
				}

				/* Otherwise, take the damage */
				else
				{
					brand_bonus_dice += 1;

					*noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_POIS);
				}
			}

			break;
		}
	}

	if ((slay_bonus_dice > 0) || (brand_bonus_dice > 1))
	{
		// cause a temporary morale penalty
		scare_onlooking_friends(m_ptr, -20);
	}
	
	return (slay_bonus_dice + brand_bonus_dice);
}

/*
 * Determines the protection percentage
 */
extern int prt_after_sharpness(const object_type *o_ptr, u32b *noticed_flag)
{
	int protection = 100;
	
	u32b f1, f2, f3;

	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);

	/* Sharpness */
	if (f1 & (TR1_SHARPNESS))
	{
		*noticed_flag = maybe_notice_slay(o_ptr, TR1_SHARPNESS);
		protection = 50;
	}

	/* Sharpness 2 */
	if (f1 & (TR1_SHARPNESS2))
	{
		*noticed_flag = maybe_notice_slay(o_ptr, TR1_SHARPNESS2);
		protection = 0;
	}
	
	// Song of sharpness
	if (singing(SNG_SHARPNESS))
	{
		int tval = o_ptr->tval;
				
		if ((tval == TV_SWORD) || (tval == TV_POLEARM) || (tval == TV_ARROW))
		{
			protection -= ability_bonus(S_SNG, SNG_SHARPNESS);
		}
	}
    
    if (protection < 0) protection = 0;
	
	return protection;
}


/*
 * Search a single square for hidden things 
 * (a utility function called by 'search' and 'perceive')
 */
void search_square(int y, int x, int dist, int searching)
{
	int score = 0;
	int difficulty = 0;
	int chest_level = 0;

	object_type *o_ptr;
	int chest_trap_present = FALSE;

	// determine if a trap is present
	for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
	{
		if ((o_ptr->tval == TV_CHEST) && chest_traps[o_ptr->pval] && !object_known_p(o_ptr))
		{
			chest_trap_present = TRUE;
			chest_level = o_ptr->pval;
			break;
		}
	}
	
	// if searching, discover unknown adjacent squares of interest
	if (searching)
	{
		if ((dist == 1) && !(cave_info[y][x] & (CAVE_MARK))) 
		{
			// mark all non-floor non-trap squares
            if (!cave_floorlike_bold(y,x))
            {
                cave_info[y][x] |= (CAVE_MARK);
            }
            			
			// mark an object, but not the square it is in
			if (cave_o_idx[y][x] != 0)
			{
				(&o_list[cave_o_idx[y][x]])->marked = TRUE;
			}
				
			/* Redraw */
			lite_spot(y, x);
		}
	}

	// if there is anything to notice...
	if ((cave_trap_bold(y,x) && (cave_info[y][x] & (CAVE_HIDDEN))) || (cave_feat[y][x] == FEAT_SECRET) || chest_trap_present)
	{

		// give up if the square is unseen and not adjacent
		if ( (dist > 1) && !(cave_info[y][x] & (CAVE_SEEN)) )  return;

		// no bonus for searching on your own square
		if (dist < 1)
		{
			dist = 1;
		}

		// Determine the base score
		score = p_ptr->skill_use[S_PER];
		
		// If using the search command give a score bonus
		if (searching) score += 5;
		
		// Eye for Detail ability
		if (p_ptr->active_ability[S_PER][PER_EYE_FOR_DETAIL]) score += 5;
		
		// Determine the base difficulty
		if (chest_trap_present)
		{
			difficulty = chest_level / 2;
		}
		else
		{
			if (p_ptr->depth > 0)
			{
				difficulty = p_ptr->depth / 2;
			}
			else
			{
				difficulty = 10;
			}
		}
		
		// Give various penalties
		if (p_ptr->blind || no_light() || p_ptr->image)     difficulty +=  5;   // can't see properly
		if (p_ptr->confused)								difficulty +=  5;   // confused
		if (dist == 2)										difficulty +=  5;   // distance 2
		if (dist == 3)										difficulty += 10;   // distance 3
		if (dist == 4)										difficulty += 15;   // distance 4
		if cave_trap_bold(y,x)								difficulty +=  5;   // dungeon trap
		if (cave_feat[y][x] == FEAT_SECRET)					difficulty += 10;   // secret door
		if (chest_trap_present)								difficulty += 15;   // chest trap
		//if (cave_info[y][x] & (CAVE_ICKY))				difficulty +=  2;   // inside least/lesser/greater vaults

		// Spider bane bonus helps to find webs
		if (cave_feat[y][x] == FEAT_TRAP_WEB) difficulty -= spider_bane_bonus();

		/* Sometimes, notice things */
		if (skill_check(PLAYER, score, difficulty, NULL) > 0)
		{
			/* Dungeon trap */
			if (cave_trap_bold(y,x))
			{
				/* Reveal the trap */
				reveal_trap(y, x);

				/* Message */
				msg_print("You have found a trap.");

				/* Disturb */
				disturb(0, 0);
			}

			/* Secret door */
			if (cave_feat[y][x] == FEAT_SECRET)
			{
				/* Message */
				msg_print("You have found a secret door.");

				/* Pick a door */
				place_closed_door(y, x);

				/* Disturb */
				disturb(0, 0);
			}

			if (chest_trap_present)
			{
				/* Message */
				msg_print("You have discovered a trap on the chest!");

				/* Know the trap */
				object_known(o_ptr);

				/* Notice it */
				disturb(0, 0);
			}

		}
	}
}

/*
 * Search for adjacent hidden things
 */
void search(void)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int y, x;

	/* Search the adjacent grids */
	for (y = (py - 1); y <= (py + 1); y++)
	{
		for (x = (px - 1); x <= (px + 1); x++)
		{
			if ((x != px) || (y != py))
			search_square(y, x, 1, TRUE);
		}
	}
	
	// also make the normal perception check
	perceive();
}

/*
 * Maybe notice hidden things nearby
 */
extern void perceive(void)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int y, x, dist;

	/* Search nearby grids */
	for (y = (py - 4); y <= (py + 4); y++)
	{
		for (x = (px - 4); x <= (px + 4); x++)
		{
			if (in_bounds(y, x))
			{
				dist = distance(py, px, y, x);
				
				/* Search only if adjacent, player lit or permanently lit */
				if ((dist <= 1) || (p_ptr->cur_light >= dist) || (cave_info[y][x] & (CAVE_GLOW)))
				{
					/* Search only if also within four grids and in line of sight*/
					if ((dist <= 4) && los(py, px, y, x))
					{
						search_square(y, x, dist, FALSE);
					}
				}
			}
		}
	}
}



/*
 * Helper routine for py_pickup() and py_pickup_floor().
 *
 * Add the given dungeon object to the character's inventory.
 *
 * Delete the object afterwards.
 */
void py_pickup_aux(int o_idx)
{
	int slot;

	char o_name[80];
	object_type *o_ptr;

	o_ptr = &o_list[o_idx];

	/*hack - don't pickup &nothings*/
	if (o_ptr->k_idx)
	{
		/* Carry the object */
		slot = inven_carry(o_ptr);

		/* Get the object again */
		o_ptr = &inventory[slot];

		/* Describe the object */
		object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

		/* Message */
		if (slot >= 0)	msg_format("You have %s (%c).", o_name, index_to_label(slot));

		// Break the truce if creatures see
		break_truce(FALSE);
	}

	/* Delete the object */
	delete_object_idx(o_idx);
}

/*
 * Allow the player to sort through items in a pile and
 * pickup what they want.  This command does not use
 * any energy because it costs a player no extra energy
 * to walk into a grid and automatically pick up items
 */
void do_cmd_pickup_from_pile(void)
{
	bool picked_up_item  = FALSE;

	/*
	 * Loop through and pick up objects until escape is hit or the backpack
	 * can't hold anything else.
	 */
	while (TRUE)
	{
		int item;

		char prompt[80];

		int floor_list[MAX_FLOOR_STACK];

		int floor_num;

		/*start with everything updated*/
		handle_stuff();

		/* Scan for floor objects */
		floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x01);

		/* No pile */
		if (floor_num < 1)
		{
			if (picked_up_item) msg_format("There are no more objects where you are standing.");
			else msg_format("There are no objects where you are standing.");
			break;
		}

		/* Restrict the choices */
		item_tester_hook = inven_carry_okay;

		/* re-test to see if we can pick any of them up */
		floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x01);

		/* Nothing can be picked up */
		if (floor_num < 1)
		{
			msg_format("Your backpack is full.");
			break;
		}

		/* Save screen */
		screen_save();

		/* Display */
		show_floor(floor_list, floor_num);

		my_strcpy(prompt, "Pick up which object? (ESC to cancel):", sizeof(prompt));

		/*clear the restriction*/
		item_tester_hook = NULL;

		/* Get the object number to be bought */
		item = get_menu_choice(floor_num, prompt);

		/*player chose escape*/
		if (item == -1)
		{
			screen_load();
			break;
		}

		/* Pick up the object */
		py_pickup_aux(floor_list[item]);

		/*Mark that we picked something up*/
		picked_up_item = TRUE;

		/* Load screen */
		screen_load();
	}

	/*clear the restriction*/
	item_tester_hook = NULL;

	/* Combine / Reorder the pack */
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);

	/* Just be sure all inventory management is done. */
	notice_stuff();

}

/*
 * Make the player carry everything in a grid.
 *
 * If "pickup" is FALSE then nothing will be picked up.
 */
void py_pickup(void)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	s16b this_o_idx, next_o_idx = 0;

	object_type *o_ptr;

	char o_name[80];

 	/* Automatically destroy squelched items in pile if necessary */
	do_squelch_pile(py, px);

	/* Scan the pile of objects */
	for (this_o_idx = cave_o_idx[py][px]; this_o_idx; this_o_idx = next_o_idx)
	{
		bool do_not_pickup = FALSE;

		/* Get the object */
		o_ptr = &o_list[this_o_idx];

		/* Describe the object */
		object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

		/* Get the next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Hack -- disturb */
		disturb(0, 0);

		/* End loop if squelched stuff reached */
		if ((k_info[o_ptr->k_idx].squelch == SQUELCH_ALWAYS) &&
	    	(k_info[o_ptr->k_idx].aware))
		{
			next_o_idx = 0;
			continue;
		}

		/*some items are marked to never pickup*/
		if ((k_info[o_ptr->k_idx].squelch == NO_SQUELCH_NEVER_PICKUP)
		    && object_aware_p(o_ptr))
		{
			do_not_pickup = TRUE;
		}

		/* Note that the pack is too full */
		if (!inven_carry_okay(o_ptr))
		{
			if (o_ptr->k_idx) msg_format("You have no room for %s.", o_name);

			/* Check the next object */
			continue;
		}
		
		// Check whether it would be too heavy
		if (p_ptr->total_weight + o_ptr->weight > weight_limit()*3/2)
		{
			if (o_ptr->k_idx) msg_format("You cannot lift %s.", o_name);
			
			/* Check the next object */
			continue;
		}

        // store the action type
		p_ptr->previous_action[0] = ACTION_MISC;
        
		/* Take a turn */
		p_ptr->energy_use = 100;
        
		/* Pick up the object */
		py_pickup_aux(this_o_idx);
	}

}


/*
 * Determine if a trap affects the player.
 * Based on player's evasion.
 */
extern bool check_hit(int power, bool display_roll)
{
	if (hit_roll(power, p_ptr->skill_use[S_EVN] + dodging_bonus(), NULL, PLAYER, display_roll) > 0) return (TRUE);
	else return (FALSE);
}


/*
 * Handle player hitting a real trap
 */
void hit_trap(int y, int x)
{
	int i, dam, prt, net_dam;
	int feat = cave_feat[y][x];

	cptr name = "a trap";

	/* Disturb the player */
	disturb(0, 0);

	// Store information for the combat rolls window
	combat_roll_special_char = (&f_info[feat])->d_char;
	combat_roll_special_attr = (&f_info[feat])->d_attr;

	/* Analyze XXX XXX XXX */
	switch (feat)
	{
        // not really a trap, but handled here due to similarities
		case FEAT_CHASM:
 		{
			// give several messages so the player has a chance to see it happen
			msg_print("You fall into the darkness!");
			message_flush();
			msg_print("...and land somewhere deeper in the Iron Hells.");
			message_flush();
            
			// add to the notes file
			do_cmd_note("Fell into a chasm", p_ptr->depth);
            
			// take some damage
			falling_damage(FALSE);
			
			// make a note if the player loses a greater vault
			note_lost_greater_vault();
            
			/* New depth */
			p_ptr->depth = MIN(p_ptr->depth + 2, MORGOTH_DEPTH - 1);
            
			/* Leaving */
			p_ptr->leaving = TRUE;
            
			break;
		}

		case FEAT_TRAP_FALSE_FLOOR:
 		{			
			// give several messages so the player has a chance to see it happen
			msg_print("The floor crumbles beneath you!");
			message_flush();
			msg_print("You fall through...");
			message_flush();
			msg_print("...and land somewhere deeper in the Iron Hells.");
			message_flush();

			// add to the notes file
			do_cmd_note("Fell through a false floor", p_ptr->depth);
						
			// take some damage
			falling_damage(FALSE);
			
			// make a note if the player loses a greater vault
			note_lost_greater_vault();

			/* New depth */
			p_ptr->depth++;

			/* Leaving */
			p_ptr->leaving = TRUE;

			break;
		}

		case FEAT_TRAP_PIT:
		{
			msg_print("You fall into a pit!");

			/* Falling damage */
			dam = damroll(2, 4);

			update_combat_rolls1b(NULL, PLAYER, TRUE);
			update_combat_rolls2(2, 4, dam, -1, -1, 0, 0, GF_HURT, FALSE);

			/* Take the damage */
			take_hit(dam, name);

			/* Make some noise */
			stealth_score -= 5;
			
			break;
		}

		case FEAT_TRAP_SPIKED_PIT:
		{						
			msg_print("You fall into a spiked pit!");

			/* Falling damage */
			dam = damroll(2, 4);

			update_combat_rolls1b(NULL, PLAYER, TRUE);
			update_combat_rolls2(2, 4, dam, -1, -1, 0, 0, GF_HURT, FALSE);

			/* Take the damage */
			take_hit(dam, name);

			/* Extra spike damage */
			dam = damroll(4, 5);

			/* Protection */
			prt = protection_roll(GF_HURT, TRUE);
			
			net_dam = (dam - prt > 0) ? (dam - prt) : 0;

			update_combat_rolls1b(NULL, PLAYER, TRUE);
			update_combat_rolls2(4, 5, dam, -1, -1, prt, 100, GF_HURT, TRUE);
			
			if (net_dam > 0)
			{
				msg_print("You are impaled!");

				/* Take the damage */
				take_hit(net_dam, name);
				
				(void)set_cut(p_ptr->cut + (net_dam+1)/2);
			}
			else
			{				
				msg_print("Your armour protects you.");
			}

			/* Make some noise */
			stealth_score -= 10;
			
			break;
		}

		case FEAT_TRAP_DART:
		{			
			if (check_hit(15, TRUE))
			{
				dam = damroll(1,15);
				prt = protection_roll(GF_HURT, FALSE);
				
				if (dam > prt)
				{
					msg_print("A small dart hits you!");
					
					// do a tiny amount of damage
					take_hit(1, name);
					
					update_combat_rolls2(1, 15, prt+1, -1, -1, prt, 100, GF_HURT, FALSE);

					(void)do_dec_stat(A_STR, NULL);
				}
				else
				{
					msg_print("A small dart hits you, but is deflected by your armour.");

					update_combat_rolls2(1, 15, dam, -1, -1, prt, 100, GF_HURT, FALSE);
				}
			}
			else
			{
				msg_print("A small dart barely misses you.");
			}

			/* Make a small amount of noise */
			monster_perception(TRUE, FALSE, 5);

			break;
		}

		case FEAT_TRAP_FLASH:
		{
			if (!p_ptr->blind)
			{
				msg_print("There is a searing flash of light!");
				if (allow_player_blind(NULL))
				{
					(void)set_blind(p_ptr->blind + damroll(5,4));
				}
				else
				{
					msg_print("Your vision quickly clears.");
				}
			}

			/* Make a small amount of noise */
			monster_perception(TRUE, FALSE, 5);
			
			break;
		}

		case FEAT_TRAP_GAS_CONF:
		{
			msg_print("A vapor fills the air and you feel yourself becoming lightheaded.");
			if (allow_player_confusion(NULL))
			{
				(void)set_confused(p_ptr->confused + damroll(4,4));
			}
			else
			{
				msg_print("You resist the effects!");
			}
			explosion(-1, 1, y, x, 3, 4, 10, GF_CONFUSION);

			/* Make a small amount of noise */
			monster_perception(TRUE, FALSE, 10);

			break;
		}

		case FEAT_TRAP_GAS_MEMORY:
		{
			msg_print("You are surrounded by a strange mist!");
			if (saving_throw(NULL, 0))
			{
				msg_print("You resist the effects!");
			}
			else
			{
				msg_print("Your memories fade away.");
				wiz_dark();
			}
			
			// Aesthetic explosion that does nothing
			explosion(-1, 1, y, x, 0, 0, 0, GF_NOTHING);

			/* Make a small amount of noise */
			monster_perception(TRUE, FALSE, 10);

			break;
		}
		
		case FEAT_TRAP_ACID:
		{
			msg_print("You are splashed with acid!");
			
			/* Acid damage */
			dam = damroll(4, 4);
			
			/* Protection */
			prt = protection_roll(GF_HURT, FALSE);
			
			net_dam = (dam - prt > 0) ? (dam - prt) : 0;
			
			update_combat_rolls1b(NULL, PLAYER, TRUE);
			update_combat_rolls2(4, 4, dam, -1, -1, prt, 100, GF_HURT, FALSE);

			acid_dam(net_dam, "an acid trap");

			/* Make a small amount of noise */
			monster_perception(TRUE, FALSE, 10);
			
			break;
		}

		case FEAT_TRAP_ALARM:
		{
			if (singing(SNG_SILENCE))
			{
				msg_print("You hear the muffled toll of a bell above your head.");
			}
			else
			{
				msg_print("You hear a bell toll loudly above your head.");
			}

			/* Make a lot of noise */
			monster_perception(TRUE, FALSE, -20);
			
			break;
		}

		case FEAT_TRAP_CALTROPS:
		{
			if (skill_check(PLAYER, p_ptr->skill_use[S_PER], 10, NULL) > 0)
			{
				msg_print("You step carefully amidst a field of caltrops.");
			}
			else
			{
				msg_print("You step on a caltrop.");
				
				dam = damroll(1, 4);
				
				update_combat_rolls1b(NULL, PLAYER, TRUE);
				update_combat_rolls2(1, 4, dam, -1, -1, 0, 0, GF_HURT, TRUE);
				
				take_hit(dam, name);
				
				if (allow_player_slow(NULL))
				{
					msg_print("It pierces your foot.");
					set_slow(p_ptr->slow + damroll(4,4));
				}
			}
			
			/* Make some noise */
			stealth_score -= 10;
			
			break;
		}
			
		case FEAT_TRAP_ROOST:
		{
			int count = 0;
			
			for (i = 0; i < 1000; i++)
			{
				if (count < 2)
				{
					count += summon_specific(y, x, p_ptr->depth + damroll(2,2) - damroll(2,2), SUMMON_BIRD_BAT);
				}
			}
			
			if (count >= 1)
			{
				msg_print("There is a flutter of wings from high above.");
				
				/* Forget the trap */
				cave_info[y][x] &= ~(CAVE_MARK);
				
				/* Remove the trap */
				cave_set_feat(y, x, FEAT_FLOOR);
			}
			
			break;
		}

		case FEAT_TRAP_WEB:
		{
			int count = 0;
			
			msg_print("You are caught in a vast black web.");

			for (i = 0; i < 1000; i++)
			{
				if (count < 1)
				{
					count += summon_specific(y, x, p_ptr->depth + damroll(2,2) - damroll(2,2), SUMMON_SPIDER);
				}
			}
			
			if (count >= 1)
			{
				msg_print("A spider descends from the gloom.");
			}
			
			break;
		}
			
		case FEAT_TRAP_DEADFALL:
		{
			int yy, xx;
			int sy = y; // to soothe compiler warnings
			int sx = x; // to soothe compiler warnings
			int sn = 0;
			
			msg_print("The ceiling collapses!");
			
			/* Check around the player */
			for (i = 0; i < 8; i++)
			{
				/* Get the location */
				yy = p_ptr->py + ddy_ddd[i];
				xx = p_ptr->px + ddx_ddd[i];
				
				/* Skip non-empty grids */
				if (!cave_empty_bold(yy, xx)) continue;
				
				/* Count "safe" grids, apply the randomizer */
				if ((++sn > 1) && (rand_int(sn) != 0)) continue;
				
				/* Save the safe location */
				sy = yy; sx = xx;
			}
						
			/* Hurt the player a lot */
			if (!sn)
			{
				/* Message and damage */
				msg_print("You are severely crushed!");
				dam = damroll(6, 8);

				/* Protection */
				prt = protection_roll(GF_HURT, FALSE);
				
				net_dam = (dam - prt > 0) ? (dam - prt) : 0;

				update_combat_rolls1b(NULL, PLAYER, TRUE);
				update_combat_rolls2(6, 8, dam, -1, -1, prt, 100, GF_HURT, FALSE);
				
				if (allow_player_stun(NULL))
				{ 
					(void)set_stun(p_ptr->stun + dam * 4);
				}
			}
			
			/* Destroy the grid, and push the player to safety */
			else
			{
				/* Calculate results */
				if (check_hit(20, TRUE))
				{
					msg_print("You are struck by rubble!");
					dam = damroll(4, 8);
					
					/* Protection */
					prt = protection_roll(GF_HURT, FALSE);
					
					update_combat_rolls2(4, 8, dam, -1, -1, prt, 100, GF_HURT, FALSE);

					net_dam = (dam - prt > 0) ? (dam - prt) : 0;

					if (allow_player_stun(NULL))
					{ 
						(void)set_stun(p_ptr->stun + dam * 4);
					}
				}
				else
				{
					msg_print("You nimbly dodge the falling rock!");
					net_dam = 0;
				}
				
				/* Move player */
				monster_swap(p_ptr->py, p_ptr->px, sy, sx);
			}
			
			/* Take the damage */
			take_hit(net_dam, name);

			/* Forget the trap */
			cave_info[y][x] &= ~(CAVE_MARK);
			
			/* Replace the trap with rubble */
			cave_set_feat(y, x, FEAT_RUBBLE);
			
			/* Make a lot of noise */
			monster_perception(TRUE, FALSE, -20);

			break;
		}
			
	}
}


/*
 * Find the attr/char pair to use for a visual hit effect
 *
 */
static u16b hit_pict(int net_dam, int dam_type, bool fatal_blow)
{
	int base;

	byte k;

	byte a;
	char c;

	if (!(use_graphics && (arg_graphics == GRAPHICS_DAVID_GERVAIS)))
	{
		/* Base graphic '*' */
		base = 0x30;


		/* Basic hit color */
		if (fatal_blow)
		{
			k = TERM_RED;
		}
		else if (net_dam == 0)
		{
			// only knock back overrides the default for zero damage hits
			if (dam_type == GF_SOUND)
			{
				k = TERM_L_UMBER;
			}
			else
			{
				k = TERM_L_WHITE;
			}
		}
		else
		{
			if (dam_type == GF_POIS)
			{
				k = TERM_GREEN;
			}
			else if (dam_type == GF_SOUND)
			{
				k = TERM_L_UMBER;
			}
			else
			{
				k = TERM_L_RED;
			}
		}
		
		/* Obtain attr/char */
		a = misc_to_attr[base+k];
		
		c = misc_to_char[base+k];
		
		if (net_dam > 0)
		{
			//if (net_dam < 20)	c = 48 + (net_dam % 10);
			c = 48 + (net_dam % 10);
		}
	}
	else
	{
		int add;

    	msg_print("Error: displaying hits doesn't work with tiles.");
  
		// Sil-y: this might look very silly in graphical tiles, but then we don't support them at all
		/* base graphic */
		base = 0x00;
		add = 0;

		k = 0;

		/* Obtain attr/char */
		a = misc_to_attr[base+k];
		c = misc_to_char[base+k] + add;
	}

	/* Create pict */
	return (PICT(a,c));
}

void display_hit(int y, int x, int net_dam, int dam_type, bool fatal_blow)
{
	u16b p;

	byte a;
	char c;

	// do nothing unless the appropriate option is set
	if (!display_hits) return; 

	/* Obtain the hit pict */
	p = hit_pict(net_dam, dam_type, fatal_blow);

	/* Extract attr/char */
	a = PICT_A(p);
	c = PICT_C(p);

	/* Display the visual effects */
	print_rel(c, a, y, x);
	move_cursor_relative(y, x);
	
	if (net_dam >= 10)	print_rel((char) 48 + (net_dam / 10), a, y, x-1);
	move_cursor_relative(y, x-1);
	
	Term_fresh();

	/* Delay */
	Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);

	/* Erase the visual effects */
	lite_spot(y, x);
	lite_spot(y, x-1);
	Term_fresh();
}


/*
 *  Determines whether an attack is a charge attack
 */

bool valid_charge(int fy, int fx, int attack_type)
{
	int d, i;
	
	int deltay = fy - p_ptr->py;
	int deltax = fx - p_ptr->px;
	
	if (p_ptr->active_ability[S_MEL][MEL_CHARGE] && (p_ptr->pspeed > 1) &&
	    ((attack_type == ATT_MAIN) || (attack_type == ATT_FLANKING) || (attack_type == ATT_CONTROLLED_RETREAT)))
	{ 
		// try all three directions
		for (i = -1; i <= 1; i++)
		{
			d = cycle[chome[dir_from_delta(deltay, deltax)] + i];
						
			if (p_ptr->previous_action[1] == d)
			{
				return (TRUE);
			}		
		}
	}
	
	return (FALSE);
}


/*
 *  Attacks a new monster with 'follow through' if applicable
 */

void possible_follow_through(int fy, int fx, int attack_type)
{
	int d, i;
	
	int y, x;
	
	int deltay = fy - p_ptr->py;
	int deltax = fx - p_ptr->px;
	
	if (p_ptr->active_ability[S_MEL][MEL_FOLLOW_THROUGH] && !(p_ptr->confused) &&
	    ((attack_type == ATT_MAIN) || (attack_type == ATT_FLANKING) || 
		 (attack_type == ATT_CONTROLLED_RETREAT) || (attack_type == ATT_FOLLOW_THROUGH)))
	{
        // look through adjacent squares in an anticlockwise direction
        for (i = 1; i < 8; i++)
        {
            d = cycle[chome[dir_from_delta(deltay, deltax)] + i];
            
            y = p_ptr->py + ddy[d];
            x = p_ptr->px + ddx[d];
            
            if (cave_m_idx[y][x] > 0)
            {
                monster_type *m_ptr = &mon_list[cave_m_idx[y][x]];
                
                if (m_ptr->ml && (!forgo_attacking_unwary || (m_ptr->alertness >= ALERTNESS_ALERT)))
                {
                    msg_print("You continue your attack!");
                    py_attack_aux(y, x, ATT_FOLLOW_THROUGH);
                    return;
                }
            }
        }
	}
}

/*
 *  Determines the bonus for the ability 'concentration' and updates some related variables.
 */

int concentration_bonus(int y, int x)
{
	int bonus = 0;
	
	// deal with 'concentration' ability
	if (p_ptr->active_ability[S_PER][PER_CONCENTRATION] && (p_ptr->last_attack_m_idx == cave_m_idx[y][x]))
	{
		bonus = MIN(p_ptr->consecutive_attacks, p_ptr->skill_use[S_PER] / 2);
	}
	
	// If the player is not engaged with this monster, reset the attack count and mosnter
	if ((p_ptr->last_attack_m_idx != cave_m_idx[y][x]))
	{
		p_ptr->consecutive_attacks = 0;
		p_ptr->last_attack_m_idx = cave_m_idx[y][x];
	}
	
	return (bonus);
}


/*
 *  Determines the bonus for the ability 'focused attack'.
 */

int focused_attack_bonus(void)
{
	// focused attack
	if (p_ptr->focused)
	{
		p_ptr->focused = FALSE;
		
		if (p_ptr->active_ability[S_PER][PER_FOCUSED_ATTACK])
		{
			return (p_ptr->skill_use[S_PER] / 2);
		}
	}

	return (0);
}


/*
 *  Determines the bonus for the ability 'master hunter'.
 */

int master_hunter_bonus(monster_type *m_ptr)
{
	// master hunter bonus
	if (p_ptr->active_ability[S_PER][PER_MASTER_HUNTER])
	{
		return (MIN((&l_list[m_ptr->r_idx])->pkills, p_ptr->skill_use[S_PER]/4));
	}
	else
	{
		return (0);
	}
}


void attack_punctuation(char *punctuation, int net_dam, int crit_bonus_dice)
{
	int i;
	
	if (net_dam == 0)
	{
		my_strcpy(punctuation, "...", sizeof(punctuation));
	}
	else if (crit_bonus_dice == 0)
	{
		my_strcpy(punctuation, ".", sizeof(punctuation));
	}
	else
	{
		for (i = 0; (i < crit_bonus_dice) && (i < 20); i++)
		{
			punctuation[i] = '!';
		}
		punctuation[i] = '\0';
	}
	
}


bool knock_back(int y1, int x1, int y2, int x2)
{
    bool knocked = FALSE;
    
    bool monster_target = FALSE;
    
    int mod, d, i;
    int y3, x3; // the location to get knocked to
    int dir;
    
    int dy, dx;
    
    // default to there being no monster
    monster_type *m_ptr = NULL;
    
    // determine the main direction from the source to the target
    dir = rough_direction(y1, x1, y2, x2);
    
    // extract the deltas from the direction
    dy = ddy[dir];
    dx = ddx[dir];
    
    // knocking a monster back...
    if (cave_m_idx[y2][x2] > 0)
    {
        monster_target = TRUE;
        m_ptr = &mon_list[cave_m_idx[y2][x2]];
    }
    
    // first try to knock it straight back
    if (cave_floor_bold(y2 + dy, x2 + dx) && (cave_m_idx[y2 + dy][x2 + dx] == 0))
    {
        y3 = y2 + dy;
        x3 = x2 + dx;
        knocked = TRUE;
    }
    
    // then try the adjacent directions
    else
    {
        // randomize clockwise or anticlockwise
        if (one_in_(2)) mod = -1;
        else			mod = +1;
        
        // try both directions
        for (i = 0; i < 2; i++)
        {
            d = cycle[chome[dir_from_delta(dy, dx)] + mod];
            y3 = y2 + ddy[d];
            x3 = x2 + ddx[d];
            if (cave_floor_bold(y3, x3) && (cave_m_idx[y3][x3] == 0))
            {
                knocked = TRUE;
                break;
            }
            
            // switch direction
            mod *= -1;
        }
    }
    
    // make the target skip a turn
    if (knocked)
    {
        if (monster_target)
        {
            m_ptr->skip_next_turn = TRUE;
            
            // actually move the monster
            monster_swap(y2, x2, y3, x3);
        }
        else
        {
            msg_print("You are knocked back.");

            p_ptr->skip_next_turn = TRUE;

            // actually move the player
            monster_swap(y2, x2, y3, x3);

            // cannot stay in the air
            p_ptr->leaping = FALSE;

            // make some noise when landing
			stealth_score -= 5;

            /* Set off traps */
            if (cave_trap_bold(p_ptr->py, p_ptr->px) || ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM)))
            {
                // If it is hidden
                if (cave_info[p_ptr->py][p_ptr->px] & (CAVE_HIDDEN))
                {
                    /* Reveal the trap */
                    reveal_trap(p_ptr->py, p_ptr->px);
                }
                
                /* Hit the trap */
                hit_trap(p_ptr->py, p_ptr->px);
            }
        }
    }
    
    return (knocked);
}

/*
 * Attack the monster at the given location
 *
 * If no "weapon" is available, then "punch" the monster one time.
 */
void py_attack_aux(int y, int x, int attack_type)
{
	int num = 0;
	
	int attack_mod = 0, total_attack_mod = 0, total_evasion_mod = 0;
	int hit_result = 0;
	int crit_bonus_dice = 0, slay_bonus_dice = 0;
	int dam = 0, prt = 0;
	int net_dam = 0;
	int prt_percent = 100;
	int hits = 0;
	int weapon_weight;
	int total_dice;
	int blows;
	int mdd, mds;
	int stealth_bonus = 0;
    int monster_ripostes = 0;
	int effective_strength;

	monster_type *m_ptr;
	monster_race *r_ptr;
	monster_lore *l_ptr;

	object_type *o_ptr;

	char m_name[80];
	char punctuation[20];

    bool abort_attack = FALSE;
	bool do_knock_back = FALSE;
	bool knocked = FALSE;
	bool charge = FALSE;
	bool rapid_attack = FALSE;
	bool off_hand_blow = FALSE;
	bool fatal_blow = FALSE;

	u32b f1, f2, f3; // the weapon's flags

	u32b noticed_flag = 0; // if any slay is observed and the weapon thus identified it goes here

	/* Get the monster */
	m_ptr = &mon_list[cave_m_idx[y][x]];
	r_ptr = &r_info[m_ptr->r_idx];
	l_ptr = &l_list[m_ptr->r_idx];

	/*possibly update the monster health bar*/
	if (p_ptr->health_who == cave_m_idx[y][x]) p_ptr->redraw |= (PR_HEALTHBAR);

	/* Disturb the player */
	disturb(0, 0);

	/* Extract monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	/* Auto-Recall if possible and visible */
	if (m_ptr->ml) monster_race_track(m_ptr->r_idx);

	/* Track a new monster */
	if (m_ptr->ml) health_track(cave_m_idx[y][x]);

	/* Target this monster */
	if (m_ptr->ml) target_set_monster(cave_m_idx[y][x]);

	/* Get the weapon */
	o_ptr = &inventory[INVEN_WIELD];
    
	/* Handle player fear */
	if (p_ptr->afraid)
	{
		/* Message */
		msg_format("You are too afraid to attack %s!", m_name);

		abort_attack = TRUE;
	}

    // inscribing an object with "!a" produces prompts to confirm that you with to attack with it
    // idea and code from MarvinPA
    if (o_ptr->obj_note && !p_ptr->truce)
    {
        cptr s;
        /* Find a '!' */
        s = strchr(quark_str(o_ptr->obj_note), '!');
    
        /* Process inscription */
        while (s)
        {
            if ((s[1] == 'a') && !get_check("Are you sure you wish to attack? "))
            {
                abort_attack = TRUE;
            }
    	
            /* Find another '!' */
            s = strchr(s + 1, '!');
        }
    }
    
 	// Warning about breaking the truce
	if ((p_ptr->truce) && !get_check("Are you sure you wish to attack? "))
	{
        abort_attack = TRUE;
	}
	
    // Warn about fighting with fists
    if ((o_ptr->weight == 0) && !get_check("Are you sure you wish to attack with no weapon? "))
    {
        abort_attack = TRUE;
    }

    // Warn about fighting with shovel
    if ((o_ptr->tval == TV_DIGGING) && (o_ptr->sval == SV_SHOVEL) && !get_check("Are you sure you wish to attack with your shovel? "))
    {
        abort_attack = TRUE;
    }

    // Cancel the attack if needed
    if (abort_attack)
    {
        if (!player_attacked)
        {
            // reset the action type
            p_ptr->previous_action[0] = ACTION_NOTHING;
            
            // don't take a turn
            p_ptr->energy_use = 0;
        }
        
        /* Done */
        return;
    }
    
	// fighting with fists is equivalent to a 4 lb weapon for the purpose of criticals
	weapon_weight = o_ptr->weight ? o_ptr->weight : 40;

	mdd = p_ptr->mdd;
	mds = p_ptr->mds;

	object_flags(o_ptr, &f1, &f2, &f3);

	// determine the base for the attack_mod
	attack_mod = p_ptr->skill_use[S_MEL];
		
	/* Monsters might notice */
	player_attacked = TRUE;
		
	// Determine the number of attacks
	blows = 1;
	if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
	{
		blows++;
		rapid_attack = TRUE;
	}
	if (p_ptr->mds2 > 0)
	{
		blows++;
	}
	
	// Attack types that take place in the opponents' turns only allow a single attack
	if ((attack_type != ATT_MAIN) && (attack_type != ATT_FLANKING) && (attack_type != ATT_CONTROLLED_RETREAT))
	{
		blows = 1;
		
		// undo strength adjustment to the attack (if any)
		mds = total_mds(o_ptr, 0);
		
		// undo the dexterity adjustment to the attack (if any)
		if (rapid_attack)
		{ 
			rapid_attack = FALSE;
			attack_mod += 3;
		}
	}

	/* Attack once for each legal blow */
	while (num++ < blows)
	{
		do_knock_back = FALSE;
		knocked = FALSE;
		
		// if the previous blow was a charge, undo the charge effects for later blows
		if (charge)
		{
			charge = FALSE;
			attack_mod -= 3;
			mds = p_ptr->mds;
		}

		// adjust for off-hand weapon if it is being used
		if ((num == blows) && (num != 1) && (p_ptr->mds2 > 0))
		{
			off_hand_blow = TRUE;
			rapid_attack = FALSE;
			
			attack_mod += p_ptr->offhand_mel_mod;
			mdd = p_ptr->mdd2;
			mds = p_ptr->mds2;
			o_ptr = &inventory[INVEN_ARM];
			weapon_weight = o_ptr->weight;
			object_flags(o_ptr, &f1, &f2, &f3);
		}

		// +3 Str/Dex on first blow when charging
		if ((num == 1) && valid_charge(y, x, attack_type))
		{
			int str_adjustment = 3;
			
			if (rapid_attack) str_adjustment -= 3;
			
			charge = TRUE;
			attack_mod += 3;

			// undo strength adjustment to the attack (if any)
			mds = total_mds(o_ptr, str_adjustment);
		}

		// reward melee attacks on sleeping monsters by characters with the asssassination ability
		// (only when a main, flanking, or controlled retreat attack, and not charging)
		if (((attack_type == ATT_MAIN) || (attack_type == ATT_FLANKING) || (attack_type == ATT_CONTROLLED_RETREAT)) && !charge)
		{
			stealth_bonus = stealth_melee_bonus(m_ptr);
		}
		else
		{
			stealth_bonus = 0;
		}

		// Determine the player's attack score after all modifiers
		total_attack_mod = total_player_attack(m_ptr, attack_mod + stealth_bonus);

		// Determine the monster's evasion score after all modifiers
		total_evasion_mod = total_monster_evasion(m_ptr, FALSE);
				
		/* Test for hit */
		hit_result = hit_roll(total_attack_mod, total_evasion_mod, PLAYER, m_ptr, TRUE);
		
		/* If the attack connects... */
		if (hit_result > 0)
		{
			hits++;

			/* Mark the monster as attacked */
			m_ptr->mflag |= (MFLAG_HIT_BY_MELEE);
			
			/* Mark the monster as charged */
			if (charge) m_ptr->mflag |= (MFLAG_CHARGED);
			
			/* Calculate the damage */
			crit_bonus_dice = crit_bonus(hit_result, weapon_weight, r_ptr, S_MEL, FALSE);
			slay_bonus_dice = slay_bonus(o_ptr, m_ptr, &noticed_flag);
			total_dice = mdd + slay_bonus_dice + crit_bonus_dice;
			
			dam = damroll(total_dice, mds);
			prt = damroll(r_ptr->pd, r_ptr->ps);

			prt_percent = prt_after_sharpness(o_ptr, &noticed_flag);
			prt = (prt * prt_percent) / 100;
			
			net_dam = dam - prt;
			
			/* No negative damage */
			if (net_dam < 0) net_dam = 0;

			// determine the punctuation for the attack ("...", ".", "!" etc)
			attack_punctuation(punctuation, net_dam, crit_bonus_dice);
			
			/* Special message for visible unalert creatures */
			if (stealth_bonus)
			{
				message_format(MSG_HIT, m_ptr->r_idx, "You stealthily attack %s%s", m_name, punctuation);
			}
			else
			{
				/* Message */
				if (charge)
				{
					message_format(MSG_HIT, m_ptr->r_idx, "You charge %s%s", m_name, punctuation);
				}
				else
				{
					message_format(MSG_HIT, m_ptr->r_idx, "You hit %s%s", m_name, punctuation);
				}

			}
			
			update_combat_rolls2(total_dice, mds, dam, r_ptr->pd, r_ptr->ps, 
			                     prt, prt_percent, GF_HURT, TRUE); 
			
			// determine the player's score for knocking an opponent backwards if they have the ability
            // first calculate their strength including modifiers for this attack
            effective_strength = p_ptr->stat_use[A_STR];
			if (charge) effective_strength += 3;
			if (rapid_attack) effective_strength -= 3;
			if (off_hand_blow) effective_strength -= 3;
            
            // cap the value by the weapon weight
			if (effective_strength > weapon_weight / 10) effective_strength = weapon_weight / 10;
            if ((effective_strength < 0) && (-effective_strength > weapon_weight / 10)) effective_strength = -(weapon_weight / 10);
            
            // give an extra +2 bonus for using a weapon two-handed
            if (two_handed_melee()) effective_strength += 2;
                            
			// check whether the effect triggers
			if (p_ptr->active_ability[S_MEL][MEL_KNOCK_BACK] && (attack_type != ATT_OPPORTUNIST) && !(r_ptr->flags1 & (RF1_NEVER_MOVE)) &&
			    (skill_check(PLAYER, effective_strength * 2, monster_stat(m_ptr, A_CON) * 2, m_ptr) > 0))
			{
				// remember this for later when the effect is applied
				do_knock_back = TRUE;
			}
			
			// damage, check for death
			fatal_blow = mon_take_hit(cave_m_idx[y][x], net_dam, NULL, -1);

			// use different colours depending on whether knock back triggered
			if (do_knock_back)
			{
				display_hit(y, x, net_dam, GF_SOUND, fatal_blow);
			}
			else
			{
				display_hit(y, x, net_dam, GF_HURT, fatal_blow);
			}
			
			
			// if a slay was noticed, then identify the weapon
			if (noticed_flag)
			{
				ident_weapon_by_use(o_ptr, m_ptr, noticed_flag);
				noticed_flag = FALSE;
			}
						
			// deal with killing blows
			if (fatal_blow)
			{
				// heal with a vampiric weapon
				if ((f1 & (TR1_VAMPIRIC)) && !monster_nonliving(r_ptr))
				{
					if (hp_player(7, FALSE, FALSE) && !object_known_p(o_ptr))
					{
						ident_weapon_by_use(o_ptr, m_ptr, TR1_VAMPIRIC);
					}
				}
				
				// gain wrath if singing song of slaying
				if (singing(SNG_SLAYING))
				{
					add_wrath();
				}
				
				// deal with 'follow_through' ability
				possible_follow_through(y, x, attack_type);
				
				// stop attacking
				break;
			}
			
			// if the monster didn't die...
			else
			{
				// deal with knock back ability if it triggered
				if (do_knock_back)
				{
                    knocked = knock_back(p_ptr->py, p_ptr->px, y, x);
 				}

				// Morgoth drops his iron crown if he is hit for 10 or more net damage twice
				if ((m_ptr->r_idx == R_IDX_MORGOTH) && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
				{
					if (net_dam >= 10)
					{
						if (p_ptr->morgoth_hits == 0)
						{
							msg_print("The force of your blow knocks the Iron Crown off balance.");
							p_ptr->morgoth_hits++;
						}
						else if (p_ptr->morgoth_hits == 1)
						{
							drop_iron_crown(m_ptr, "You knock his crown from off his brow, and it falls to the ground nearby.");
							p_ptr->morgoth_hits++;
						}
					}
				}
								
				// Deal with cruel blow ability
				if (p_ptr->active_ability[S_STL][STL_CRUEL_BLOW] && (crit_bonus_dice > 0) && (net_dam > 0) && !(r_ptr->flags1 & (RF1_RES_CRIT)))
				{
					if (skill_check(PLAYER, crit_bonus_dice * 4, monster_skill(m_ptr, S_WIL), m_ptr) > 0)
					{
						msg_format("%^s reels in pain!", m_name);
						
						// confuse the monster (if possible)
						if (!(r_ptr->flags3 & (RF3_NO_CONF)))
						{
							// The +1 is needed as a turn of this wears off immediately
							m_ptr->confused += crit_bonus_dice + 1;
						}
						
						// cause a temporary morale penalty
						scare_onlooking_friends(m_ptr, -20);
					}
				}
			}
		}

		/* Player misses */
		else
		{
			/* Message */
			message_format(MSG_MISS, m_ptr->r_idx, "You miss %s.", m_name);
			
			// Occasional warning about fighting from within a pit
			if (cave_pit_bold(p_ptr->py,p_ptr->px) && one_in_(3))
			{
				msg_print("(It is very hard to dodge or attack from within a pit.)");
			}

			// Occasional warning about fighting from within a web
			if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB) && one_in_(3))
			{
				msg_print("(It is very hard to dodge or attack from within a web.)");
			}

            // allow for ripostes
			// treats attack a weapon weighing 2 pounds per damage die
            if ((r_ptr->flags2 & (RF2_RIPOSTE)) &&
                (monster_ripostes == 0) &&
                !m_ptr->confused &&
                (m_ptr->stance != STANCE_FLEEING) &&
                !m_ptr->skip_this_turn &&
                !m_ptr->skip_next_turn &&
                (hit_result <= -10 - (2 * r_ptr->blow[0].dd)))
            {
                msg_format("%^s ripostes!", m_name);
                make_attack_normal(m_ptr);
                monster_ripostes++;
            }
		}

		// alert the monster, even if no damage was done or the player missed
		make_alert(m_ptr);
		
		// stop attacking if you displace the creature
		if (knocked) break;
		
	}

	// Break the truce if creatures see
	break_truce(FALSE);
		
}

bool whirlwind_possible(void)
{
	int d, dir, y, x;
		
	if (!p_ptr->active_ability[S_MEL][MEL_WHIRLWIND_ATTACK])
	{
		return (FALSE);
	}
	
	 // check adjacent squares for impassable squares
	 for (d = 0; d < 8; d++)
	 {
		 dir = cycle[d];
		 
		 y = p_ptr->py + ddy[dir];
		 x = p_ptr->px + ddx[dir];
		 
		 if (!cave_floor_bold(y,x))
		 {
			 return (FALSE);
		 }
	 }
		 
	return (TRUE);
}




void py_attack(int y, int x, int attack_type)
{
	int dir, dir0, yy, xx;
	
	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	if ((p_ptr->rage || whirlwind_possible()) && (adj_mon_count(p_ptr->py, p_ptr->px) > 1) && !p_ptr->afraid)
	{
		int i;
		bool clockwise = one_in_(2);
		
		// message only for rage (too annoying otherwise)
		if (p_ptr->rage)
		{
			msg_print("You strike out at everything around you!");
		}
		
		dir = dir_from_delta(y - p_ptr->py, x - p_ptr->px);
		
		/* Extract cycle index */
		dir0 = chome[dir];
		
		// attack the adjacent squares in sequence
		for (i = 0; i < 8; i++)
		{
			if (clockwise)  dir = cycle[dir0+i];
			else            dir = cycle[dir0-i];
							
			yy = p_ptr->py + ddy[dir];
			xx = p_ptr->px + ddx[dir];
			
			if (cave_m_idx[yy][xx] > 0)
			{
                monster_type *m_ptr = &mon_list[cave_m_idx[yy][xx]];
                                                
				//if (i == 0)                                                                           py_attack_aux(yy, xx, ATT_MAIN);
				if (p_ptr->rage)                                                                        py_attack_aux(yy, xx, ATT_RAGE);
				else if ((i == 0) || !forgo_attacking_unwary || (m_ptr->alertness >= ALERTNESS_ALERT))  py_attack_aux(yy, xx, ATT_WHIRLWIND);
			}
		}
	}
	else
	{
		py_attack_aux(y, x, attack_type);
	}
}

/*
 *  Does any flanking or controlled retreat attack necessary when player moves to square y,x
 */
void flanking_or_retreat(int y, int x)
{
	int py = p_ptr->py;
	int px = p_ptr->px;
	int d;
	int fy, fx;
	int start;
	monster_type *m_ptr;
	
	bool flanking = p_ptr->active_ability[S_EVN][EVN_FLANKING];
	bool controlled_retreat = FALSE;
	
	// need to have the ability, and to have not moved last round
	if (p_ptr->active_ability[S_EVN][EVN_CONTROLLED_RETREAT] && 
	    ((p_ptr->previous_action[1] > 9) || (p_ptr->previous_action[1] == 5)))
	{
		controlled_retreat = TRUE;
	}
	
	if (!p_ptr->confused && (flanking || controlled_retreat))
	{
		fy = p_ptr->target_row;
		fx = p_ptr->target_col;
		
		// first see if the targetted monster is eligible and attack it if so
		if ((cave_m_idx[fy][fx] > 0) && !p_ptr->confused && !p_ptr->afraid && !p_ptr->truce)
		{
			m_ptr = &mon_list[cave_m_idx[fy][fx]];
			
			// base conditions for an attack
			if (m_ptr->ml && (!forgo_attacking_unwary || (m_ptr->alertness >= ALERTNESS_ALERT)))
			{
				// try a flanking attack
				if (flanking && (distance(py, px, fy, fx) == 1) && (distance(y, x, fy, fx) == 1))
				{
					py_attack(fy, fx, ATT_FLANKING);
					return;
				}
				// try a controlled retreat attack
				if (controlled_retreat && (distance(py, px, fy, fx) == 1) && (distance(y, x, fy, fx) > 1))
				{
					py_attack(fy, fx, ATT_CONTROLLED_RETREAT);
					return;
				}
			}
		}
		
		// otherwise we will look through the eligible monsters and choose one randomly
		start = rand_int(8);
		
		/* Look for adjacent monsters */
		for (d = start; d < 8 + start; d++)
		{
			fy = py + ddy_ddd[d % 8];
			fx = px + ddx_ddd[d % 8];
				
			/* Check Bounds */
			if (!in_bounds(fy, fx)) continue;
			
			if ((cave_m_idx[fy][fx] > 0) && !p_ptr->confused && !p_ptr->afraid && !p_ptr->truce)
			{
				m_ptr = &mon_list[cave_m_idx[fy][fx]];
				
				// base conditions for an attack
				if (m_ptr->ml && (!forgo_attacking_unwary || (m_ptr->alertness >= ALERTNESS_ALERT)))
				{
					// try a flanking attack
					if (flanking && (distance(py, px, fy, fx) == 1) && (distance(y, x, fy, fx) == 1))
					{
						py_attack(fy, fx, ATT_FLANKING);
						return;
					}
					// try a controlled retreat attack
					if (controlled_retreat && (distance(py, px, fy, fx) == 1) && (distance(y, x, fy, fx) > 1))
					{
						py_attack(fy, fx, ATT_CONTROLLED_RETREAT);
						return;
					}
				}
			}
		}
	}
}

/*
 * Is it valid to climb from (y1,x1) to (y2,x2)
 */
bool climbable(int y1, int x1, int y2, int x2)
{
    //int i;
    //int walls = 0;
    //int yy, xx;
    
    int a = y1 + x1 + y2 + x2;  // Sil-y: soothing complilation warnings
    a = 0;                      // Sil-y: soothing complilation warnings
    return (FALSE);             // Sil-x: no climbing in this version

    // at least one must be a chasm
    //if ((cave_feat[y1][x1] != FEAT_CHASM) && (cave_feat[y2][x2] != FEAT_CHASM)) return (FALSE);
                
    /* Count adjacent walls */
    //
    //for (i = 0; i < 8; i++)
    //{
    //    yy = y2 + ddy_ddd[i];
    //    xx = x2 + ddx_ddd[i];
        
        // Note that rubble doesn't count
    //    if (cave_wall_bold(yy,xx) && (cave_feat[yy][xx] != FEAT_RUBBLE))
    //    {
            // count the walls
    //        walls++;
    //    }
    //}
    
    //if (walls > 0)  return (TRUE);
    //else            return (FALSE);
}

/*
 * Move player in the given direction, with the given "pickup" flag.
 *
 * This routine should only be called when energy has been expended.
 *
 * Note that this routine handles monsters in the destination grid,
 * and also handles attempting to move into walls/doors/rubble/etc.
 */
void move_player(int dir)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

    int oy = p_ptr->py; // old location
    int ox = p_ptr->px;
    
	int y, x;
    
    bool climb = FALSE;
    
	/* Find the result of moving */
	y = py + ddy[dir];
	x = px + ddx[dir];

	/* deal with leaving the map */
	if ((y < 0) || (x < 0) || (y >= p_ptr->cur_map_hgt) || (x >= p_ptr->cur_map_wid))
	{
		do_cmd_escape();
		return;
	}

	/* Hack -- attack visible monsters */
	if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
	{
        /* Attack */
        py_attack(y, x, ATT_MAIN);
	}

	/* open known doors on movement */
	else if ((cave_info[y][x] & (CAVE_MARK)) && cave_known_closed_door_bold(y, x))
	{
		/* Open */
		do_cmd_open_aux(y, x);
	}

	/* Player can not walk through "walls", but can go through traps */
	else if (!cave_floor_bold(y, x))
	{
		/* Disturb the player */
		disturb(0, 0);

		/* Notice unknown obstacles */
		if (!(cave_info[y][x] & (CAVE_MARK)))
		{
			/* Rubble */
			if (cave_feat[y][x] == FEAT_RUBBLE)
			{
				message(MSG_HITWALL, 0, "You feel a pile of rubble blocking your way.");
				cave_info[y][x] |= (CAVE_MARK);
				lite_spot(y, x);
			}

			/* Closed door */
			else if (cave_known_closed_door_bold(y,x))
			{
				message(MSG_HITWALL, 0, "You feel a door blocking your way.");
				cave_info[y][x] |= (CAVE_MARK);
				lite_spot(y, x);
			}

			/* Wall (or secret door) */
			else
			{
				message(MSG_HITWALL, 0, "You feel a wall blocking your way.");
				cave_info[y][x] |= (CAVE_MARK);
				lite_spot(y, x);
			}
		}

		/* Mention known obstacles */
		else
		{
			/* Rubble */
			if (cave_feat[y][x] == FEAT_RUBBLE)
			{
				message(MSG_HITWALL, 0, "There is a pile of rubble blocking your way.");
			}

			/* Closed door */
			else if (cave_known_closed_door_bold(y,x))
			{
				message(MSG_HITWALL, 0, "There is a door blocking your way.");
			}

			/* Wall (or secret door) */
			else
			{
				message(MSG_HITWALL, 0, "There is a wall blocking your way.");
			}
		}
		
		// store the action type
		p_ptr->previous_action[0] = ACTION_MISC;
		
	}

	/* Normal movement */
	else
	{
		// deal with overburdened characters
		if (p_ptr->total_weight > weight_limit()*3/2)
		{
			/* Abort */
			msg_print("You are too burdened to move.");

			/* Disturb the player */
			disturb(0, 0);
			
			// don't take a turn...
			p_ptr->energy_use = 0;
			
			return;
		}
		
		/* Check before walking on known traps/chasms on movement */
		if ((!p_ptr->confused) && (cave_info[y][x] & (CAVE_MARK)))
		{
            // leapable things: chasms, known pits, known false floors
            if (((cave_feat[y][x] == FEAT_CHASM) && !climbable(p_ptr->py, p_ptr->px, y, x)) ||
                ((cave_pit_bold(y,x) || (cave_feat[y][x] == FEAT_TRAP_FALSE_FLOOR)) && !cave_floorlike_bold(y,x)))
            {
                char prompt[80];
                int i;
                int d;
                bool run_up = FALSE;
                bool confirm = TRUE;
                
                // test all three directions roughly towards the chasm/pit
                for (i = -1; i <= 1; i++)
                {
                    d = cycle[chome[dir_from_delta(y - p_ptr->py, x - p_ptr->px)] + i];
                    
                    // if the last action was a move in this direction, we have a valid run_up
                    if (p_ptr->previous_action[1] == d) run_up = TRUE;
                }

                if (p_ptr->active_ability[S_EVN][EVN_LEAPING])
                {
                    int y_mid, x_mid; // the midpoint of the leap
                    int y_end, x_end; // the endpoint of the leap
                                                            
                    /* Get location */
                    y_mid = p_ptr->py + ddy[dir];
                    x_mid = p_ptr->px + ddx[dir];
                    y_end = y_mid + ddy[dir];
                    x_end = x_mid + ddx[dir];

                    /* Disturb the player */
                    disturb(0, 0);
                    
                    /* Flush input */
                    flush();

                    // Can't jump from within pits
                    if (cave_pit_bold(p_ptr->py,p_ptr->px))
                    {
                        msg_print("You cannot leap from within a pit.");
                    }
                    
                    // Can't jump from within webs
                    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
                    {
                        msg_print("You cannot leap from within a web.");
                    }
                    
                    // Can't jump without a run up
                    else if (!run_up)
                    {
                        msg_print("You cannot leap without a run up.");
                    }
                    
                    // need room to land
                    else if ((cave_info[y_end][x_end] & (CAVE_MARK)) &&
                             (cave_wall_bold(y_end, x_end) || cave_any_closed_door_bold(y_end, x_end)))
                    {
                        msg_print("You cannot leap over as there is no room to land.");
                    }                    
                    
                    else
                    {
                        // confirm if the destination is unknown
                        if (!(cave_info[y_end][x_end] & (CAVE_SEEN)) && !(cave_info[y_end][x_end] & (CAVE_MARK)))
                        {
                            strnfmt(prompt, sizeof(prompt), "Are you sure you wish to leap into the unknown? ");
                        }
                        
                        // confirm if the destination is in the chasm
                        else if (cave_feat[y_end][x_end] == FEAT_CHASM)
                        {
                            strnfmt(prompt, sizeof(prompt), "Are you sure you wish to leap into the abyss? ");
                        }
                        
                        // confirm if the destination has a visible monster
                        else if ((cave_m_idx[y_end][x_end] > 0) && (&mon_list[cave_m_idx[y_end][x_end]])->ml)
                        {
                            monster_type *m_ptr = &mon_list[cave_m_idx[y_end][x_end]];
                            char m_name[80];
                            
                            /* Get the monster name */
                            monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                            
                            strnfmt(prompt, sizeof(prompt), "Are you sure you wish to leap into %s? ", m_name);
                        }
                        
                        // default confirmation
                        else
                        {
                            confirm = FALSE;
                            //strnfmt(prompt, sizeof(prompt), "Leap over the %s? ", f_name + f_info[cave_feat[y_mid][x_mid]].name);
                        }
                        
                        // if you say 'yes' to the prompt, then try to leap
                        if (!confirm || get_check(prompt))
                        {
                            // at this point attack any invisible monster that may be there
                            if (cave_m_idx[y][x] > 0)
                            {
                                msg_print("An unseen foe blocks your way.");

                                /* Attack */
                                py_attack(y, x, ATT_MAIN);
                                
                                return;
                            }

                            // otherwise do the leap!
                            else
                            {
                                // we generously give you your free flanking attack...
                                flanking_or_retreat(y_mid, x_mid);

                                /* Take a turn */
                                p_ptr->energy_use = 100;
                                
                                // store the action type
                                p_ptr->previous_action[0] = dir;
                                
                                // move player to the new position
                                monster_swap(p_ptr->py, p_ptr->px, y_mid, x_mid);
                                
                                // remember that the player is in the air now
                                p_ptr->leaping = TRUE;
                                
                                return;
                            }
                        }
                    }
                }

                // if the player hasn't already leapt
                if (!p_ptr->leaping && (cave_feat[y][x] == FEAT_CHASM))
                {
                    /* Disturb the player */
                    disturb(0, 0);
                    
                    /* Flush input */
                    flush();
                    
                    if (!get_check("Step into the chasm? "))
                    {
                        // don't take a turn...
                        p_ptr->energy_use = 0;
                        
                        return;
                    }
                }
                
            }
            
            // traps
            if ((cave_trap_bold(y,x) && !cave_floorlike_bold(y,x)))
            {
                /* Disturb the player */
                disturb(0, 0);
                
                /* Flush input */
                flush();
                
                if (!get_check("Are you sure you want to step on the trap? "))
                {
                    // don't take a turn...
                    p_ptr->energy_use = 0;
                    
                    return;
                }
            }
		}

        // if there is an invisible monster present and you haven't yet attacked, do so now
        if (cave_m_idx[y][x] > 0)
        {
            msg_print("An unseen foe blocks your way.");
            
            /* Attack */
            py_attack(y, x, ATT_MAIN);
            
            return;
        }

		// It is hard to get out of a pit
		if (cave_pit_bold(py,px))
		{
			int difficulty;
			
			if (cave_feat[py][px] == FEAT_TRAP_PIT)     difficulty = 10;
			else                                        difficulty = 15;
			 
			/* Disturb the player */
			disturb(0, 0);

			if (check_hit(difficulty, FALSE))
			{
				msg_print("You try to climb out of the pit, but fail.");

				/* Take a turn */
				p_ptr->energy_use = 100;
				
				// store the action type
				p_ptr->previous_action[0] = ACTION_MISC;

				return;
			}
			else
			{
				msg_print("You climb out of the pit.");
			}
		}

		// It is hard to get out of a web
		if (cave_feat[py][px] == FEAT_TRAP_WEB)
		{
			if (!break_free_of_web())	return;
		}
		
		
		/* Sound XXX XXX XXX */
		/* sound(MSG_WALK); */

		// do flanking or controlled retreat attack if any
		flanking_or_retreat(y, x);

        // check whether the player can climb to the new square
        climb = climbable(py, px, y, x);
        
		/* Move player */
		monster_swap(py, px, y, x);

		/* New location */
		y = py = p_ptr->py;
		x = px = p_ptr->px;
		
		/* Spontaneous Searching */
		perceive();
        
        // remember this direction of movement
		p_ptr->previous_action[0] = dir;
		
		/* Discover stairs if blind */
		if (cave_stair_bold(y,x))
		{
			cave_info[y][x] |= (CAVE_MARK);
			lite_spot(y, x);
		}

		/* Remark on Forge and discover it if blind */
		if (cave_forge_bold(p_ptr->py, p_ptr->px))
		{
			if ((cave_feat[p_ptr->py][p_ptr->px] >= FEAT_FORGE_UNIQUE_HEAD) && !p_ptr->unique_forge_seen)
			{
				msg_print("You enter the forge 'Orodruth' - the Mountain's Anger - where Grond was made in days of old.");
				msg_print("The fires burn still.");
				p_ptr->unique_forge_seen = TRUE;
				do_cmd_note("Entered the forge 'Orodruth'", p_ptr->depth);
			}
			
			else
			{
				char *article;
				
				if (cave_feat[p_ptr->py][p_ptr->px] >= FEAT_FORGE_UNIQUE_HEAD)		article = "the";
				else if (cave_feat[p_ptr->py][p_ptr->px] >= FEAT_FORGE_GOOD_HEAD)	article = "an";
				else																article = "a";
				
				msg_format("You enter %s %s.", article, f_name + f_info[cave_feat[p_ptr->py][p_ptr->px]].name);
			}
            
			cave_info[y][x] |= (CAVE_MARK);
			lite_spot(y, x);
		}
		
        // check for climbing
        if (climb)
        {
            if ((cave_feat[oy][ox] != FEAT_CHASM) && (cave_feat[y][x] == FEAT_CHASM))
            {
                msg_print("You begin your climb.");
            }
        }
        
        // check for ending a climb
        if (p_ptr->climbing)
        {
            if ((cave_feat[oy][ox] == FEAT_CHASM) && (cave_feat[y][x] != FEAT_CHASM))
            {
                msg_print("You are back to solid ground.");
            }
        }

        
		/* Set off traps */
        if (cave_trap_bold(y,x) || ((cave_feat[y][x] == FEAT_CHASM) && !climb))
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
		
		// read any notes the player stumbles upon
		if ((cave_o_idx[p_ptr->py][p_ptr->px] != 0))
		{
			object_type *o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
			if (o_ptr->tval == TV_NOTE) 
			{
				note_info_screen(o_ptr);
			}
		}
	}
}


/*
 * Hack -- Check for a "known wall" (see below)
 */
static int see_wall(int dir, int y, int x)
{
	/* Get the new location */
	y += ddy[dir];
	x += ddx[dir];

	/* Illegal grids are not known walls XXX XXX XXX */
	if (!in_bounds(y, x)) return (FALSE);

	/* Non-wall grids are not known walls */
	if (!cave_wall_bold(y,x)) return (FALSE);

	/* Unknown walls are not known walls */
	if (!(cave_info[y][x] & (CAVE_MARK))) return (FALSE);

	/* Default */
	return (TRUE);
}


/*
 * Hack -- Check for an "unknown corner" (see below)
 */
//static int see_nothing(int dir, int y, int x)
//{
//	/* Get the new location */
//	y += ddy[dir];
//	x += ddx[dir];

//	/* Illegal grids are unknown XXX XXX XXX */
//	if (!in_bounds(y, x)) return (TRUE);

//	/* Memorized grids are always known */
//	if (cave_info[y][x] & (CAVE_MARK)) return (FALSE);

//	/* Default */
//	return (TRUE);
//}





/*
 * The running algorithm  -CJS-
 *
 * Basically, once you start running, you keep moving until something
 * interesting happens.  In an enclosed space, you run straight, but
 * you follow corners as needed (i.e. hallways).  In an open space,
 * you run straight, but you stop before entering an enclosed space
 * (i.e. a room with a doorway).  In a semi-open space (with walls on
 * one side only), you run straight, but you stop before entering an
 * enclosed space or an open space (i.e. running along side a wall).
 *
 * All discussions below refer to what the player can see, that is,
 * an unknown wall is just like a normal floor.  This means that we
 * must be careful when dealing with "illegal" grids.
 *
 * No assumptions are made about the layout of the dungeon, so this
 * algorithm works in hallways, rooms, destroyed areas, etc.
 *
 * In the diagrams below, the player has just arrived in the grid
 * marked as '@', and he has just come from a grid marked as 'o',
 * and he is about to enter the grid marked as 'x'.
 *
 * Running while confused is not allowed, and so running into a wall
 * is only possible when the wall is not seen by the player.  This
 * will take a turn and stop the running.
 *
 * Several conditions are tracked by the running variables.
 *
 *   p_ptr->run_open_area (in the open on at least one side)
 *   p_ptr->run_break_left (wall on the left, stop if it opens)
 *   p_ptr->run_break_right (wall on the right, stop if it opens)
 *
 * When running begins, these conditions are initialized by examining
 * the grids adjacent to the requested destination grid (marked 'x'),
 * two on each side (marked 'L' and 'R').  If either one of the two
 * grids on a given side is a wall, then that side is considered to
 * be "closed".  Both sides enclosed yields a hallway.
 *
 *    LL                     @L
 *    @x      (normal)       RxL   (diagonal)
 *    RR      (east)          R    (south-east)
 *
 * In the diagram below, in which the player is running east along a
 * hallway, he will stop as indicated before attempting to enter the
 * intersection (marked 'x').  Starting a new run in any direction
 * will begin a new hallway run.
 *
 *  #.#
 * ##.##
 * o@x..
 * ##.##
 *  #.#
 *
 * Note that a minor hack is inserted to make the angled corridor
 * entry (with one side blocked near and the other side blocked
 * further away from the runner) work correctly. The runner moves
 * diagonally, but then saves the previous direction as being
 * straight into the gap. Otherwise, the tail end of the other
 * entry would be perceived as an alternative on the next move.
 *
 * In the diagram below, the player is running east down a hallway,
 * and will stop in the grid (marked '1') before the intersection.
 * Continuing the run to the south-east would result in a long run
 * stopping at the end of the hallway (marked '2').
 *
 * ##################
 * o@x       1
 * ########### ######
 * #2          #
 * #############
 *
 * After each step, the surroundings are examined to determine if
 * the running should stop, and to determine if the running should
 * change direction.  We examine the new current player location
 * (at which the runner has just arrived) and the direction from
 * which the runner is considered to have come.
 *
 * Moving one grid in some direction places you adjacent to three
 * or five new grids (for straight and diagonal moves respectively)
 * to which you were not previously adjacent (marked as '!').
 *
 *   ...!              ...
 *   .o@!  (normal)    .o.!  (diagonal)
 *   ...!  (east)      ..@!  (south east)
 *                      !!!
 *
 * If any of the newly adjacent grids are "interesting" (monsters,
 * objects, some terrain features) then running stops.
 *
 * If any of the newly adjacent grids seem to be open, and you are
 * looking for a break on that side, then running stops.
 *
 * If any of the newly adjacent grids do not seem to be open, and
 * you are in an open area, and the non-open side was previously
 * entirely open, then running stops.
 *
 * If you are in a hallway, then the algorithm must determine if
 * the running should continue, turn, or stop.  If only one of the
 * newly adjacent grids appears to be open, then running continues
 * in that direction, turning if necessary.  If there are more than
 * two possible choices, then running stops.  If there are exactly
 * two possible choices, separated by a grid which does not seem
 * to be open, then running stops.  Otherwise, as shown below, the
 * player has probably reached a "corner".
 *
 *    ###             o##
 *    o@x  (normal)   #@!   (diagonal)
 *    ##!  (east)     ##x   (south east)
 *
 * In this situation, there will be two newly adjacent open grids,
 * one touching the player on a diagonal, and one directly adjacent.
 * We must consider the two "option" grids further out (marked '?').
 * We assign "option" to the straight-on grid, and "option2" to the
 * diagonal grid.  For some unknown reason, we assign "check_dir" to
 * the grid marked 's', which may be incorrectly labelled.
 *
 *    ###s
 *    o@x?   (may be incorrect diagram!)
 *    ##!?
 *
 * If both "option" grids are closed, then there is no reason to enter
 * the corner, and so we can cut the corner, by moving into the other
 * grid (diagonally).  If we choose not to cut the corner, then we may
 * go straight, but we pretend that we got there by moving diagonally.
 * Below, we avoid the obvious grid (marked 'x') and cut the corner
 * instead (marked 'n').
 *
 *    ###:               o##
 *    o@x#   (normal)    #@n    (maybe?)
 *    ##n#   (east)      ##x#
 *                       ####
 *
 * If one of the "option" grids is open, then we may have a choice, so
 * we check to see whether it is a potential corner or an intersection
 * (or room entrance).  If the grid two spaces straight ahead, and the
 * space marked with 's' are both open, then it is a potential corner
 * and we enter it if requested.  Otherwise, we stop, because it is
 * not a corner, and is instead an intersection or a room entrance.
 *
 *    ###
 *    o@x
 *    ##!#
 *
 * (This documentation may no longer be correct)
 */



/*
 * Hack -- allow quick "cycling" through the legal directions
 */
const byte cycle[] =
{ 1, 2, 3, 6, 9, 8, 7, 4, 1, 2, 3, 6, 9, 8, 7, 4, 1, 2, 3, 6, 9, 8, 7, 4};

/*
 * Hack -- map each direction into the "middle" of the "cycle[]" array
 */
const byte chome[] =
{ 0, 8, 9, 10, 15, 0, 11, 14, 13, 12 };


/*
 * Initialize the running algorithm for a new direction.
 *
 * Diagonal Corridor -- allow diaginal entry into corridors.
 *
 * Blunt Corridor -- If there is a wall two spaces ahead and
 * we seem to be in a corridor, then force a turn into the side
 * corridor, must be moving straight into a corridor here. (?)
 *
 * Diagonal Corridor    Blunt Corridor (?)
 *       # #                  #
 *       #x#                 @x#
 *       @p.                  p
 */
static void run_init(int dir)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int i, row, col;

	bool deepleft, deepright;
	bool shortleft, shortright;

	/* Save the direction */
	p_ptr->run_cur_dir = dir;

	/* Assume running straight */
	p_ptr->run_old_dir = dir;

	/* Assume looking for open area */
	p_ptr->run_open_area = TRUE;

	/* Assume not looking for breaks */
	p_ptr->run_break_right = FALSE;
	p_ptr->run_break_left = FALSE;

	/* Assume no nearby walls */
	deepleft = deepright = FALSE;
	shortright = shortleft = FALSE;

	/* Find the destination grid */
	row = py + ddy[dir];
	col = px + ddx[dir];

	/* Extract cycle index */
	i = chome[dir];

	/* Check for nearby wall */
	if (see_wall(cycle[i+1], py, px))
	{
		p_ptr->run_break_left = TRUE;
		shortleft = TRUE;
	}

	/* Check for distant wall */
	else if (see_wall(cycle[i+1], row, col))
	{
		p_ptr->run_break_left = TRUE;
		deepleft = TRUE;
	}

	/* Check for nearby wall */
	if (see_wall(cycle[i-1], py, px))
	{
		p_ptr->run_break_right = TRUE;
		shortright = TRUE;
	}

	/* Check for distant wall */
	else if (see_wall(cycle[i-1], row, col))
	{
		p_ptr->run_break_right = TRUE;
		deepright = TRUE;
	}

	/* Looking for a break */
	if (p_ptr->run_break_left && p_ptr->run_break_right)
	{
		/* Not looking for open area */
		p_ptr->run_open_area = FALSE;

		/* Hack -- allow angled corridor entry */
		if (dir & 0x01)
		{
			if (deepleft && !deepright)
			{
				p_ptr->run_old_dir = cycle[i - 1];
			}
			else if (deepright && !deepleft)
			{
				p_ptr->run_old_dir = cycle[i + 1];
			}
		}

		/* Hack -- allow blunt corridor entry */
		else if (see_wall(cycle[i], row, col))
		{
			if (shortleft && !shortright)
			{
				p_ptr->run_old_dir = cycle[i - 2];
			}
			else if (shortright && !shortleft)
			{
				p_ptr->run_old_dir = cycle[i + 2];
			}
		}
	}
}


/*
 * Update the current "run" path
 *
 * Return TRUE if the running should be stopped
 */
static bool run_test(void)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int prev_dir;
	int new_dir;
	int check_dir = 0;

	int row, col;
	int i, max, inv;
	int option, option2;


	/* No options yet */
	option = 0;
	option2 = 0;

	/* Where we came from */
	prev_dir = p_ptr->run_old_dir;

	/* Range of newly adjacent grids */
	max = (prev_dir & 0x01) + 1;

	/* Look at every newly adjacent square. */
	for (i = -max; i <= max; i++)
	{
		object_type *o_ptr;

		/* New direction */
		new_dir = cycle[chome[prev_dir] + i];

		/* New location */
		row = py + ddy[new_dir];
		col = px + ddx[new_dir];

		/* Visible monsters abort running */
		if (cave_m_idx[row][col] > 0)
		{
			monster_type *m_ptr = &mon_list[cave_m_idx[row][col]];

			/* Visible monster */
			if (m_ptr->ml) return (TRUE);

		}

		/* Visible objects abort running */
		for (o_ptr = get_first_object(row, col); o_ptr; o_ptr = get_next_object(o_ptr))
		{
			/* Visible object */
			if (o_ptr->marked) return (TRUE);
		}

		/* Assume unknown */
		inv = TRUE;

		/* Check memorized grids */
		if (cave_info[row][col] & (CAVE_MARK))
		{
			bool notice = TRUE;

			/* Examine the terrain */
			switch (cave_feat[row][col])
			{
				/* Floors */
				case FEAT_FLOOR:

				/* Secret doors */
				case FEAT_SECRET:

				/* Walls */
				case FEAT_QUARTZ:
				case FEAT_WALL_EXTRA:
				case FEAT_WALL_INNER:
				case FEAT_WALL_OUTER:
				case FEAT_WALL_SOLID:
				case FEAT_WALL_PERM:
				{
					/* Ignore */
					notice = FALSE;

					/* Done */
					break;
				}

				/* Open doors */
				case FEAT_OPEN:
				case FEAT_BROKEN:
				{
					/* ignore */
					notice = FALSE;

					/* Done */
					break;
				}

				/* Stairs */
				case FEAT_LESS:
				case FEAT_MORE:
				case FEAT_LESS_SHAFT:
				case FEAT_MORE_SHAFT:
				{
					/* Done */
					break;
				}
				
				/* Deal with traps */
				default:
				{
					// ignore hidden traps
					if (cave_floorlike_bold(row,col))
					{
						/* ignore */
						notice = FALSE;
						
						/* Done */
						break;
					}
				}
			}

			/* Interesting feature */
			if (notice) return (TRUE);

			/* The grid is "visible" */
			inv = FALSE;
		}

		/* Analyze unknown grids and floors */
		if (inv || cave_floor_bold(row, col))
		{
			/* Looking for open area */
			if (p_ptr->run_open_area)
			{
				/* Nothing */
			}

			/* The first new direction. */
			else if (!option)
			{
				option = new_dir;
			}

			/* Three new directions. Stop running. */
			else if (option2)
			{
				return (TRUE);
			}

			/* Two non-adjacent new directions.  Stop running. */
			else if (option != cycle[chome[prev_dir] + i - 1])
			{
				return (TRUE);
			}

			/* Two new (adjacent) directions (case 1) */
			else if (new_dir & 0x01)
			{
				check_dir = cycle[chome[prev_dir] + i - 2];
				option2 = new_dir;
			}

			/* Two new (adjacent) directions (case 2) */
			else
			{
				check_dir = cycle[chome[prev_dir] + i + 1];
				option2 = option;
				option = new_dir;
			}
		}

		/* Obstacle, while looking for open area */
		else
		{
			if (p_ptr->run_open_area)
			{
				if (i < 0)
				{
					/* Break to the right */
					p_ptr->run_break_right = TRUE;
				}

				else if (i > 0)
				{
					/* Break to the left */
					p_ptr->run_break_left = TRUE;
				}
			}
		}
	}

	// Now check to see if running another step would bring us next to an
	// immobile monster (such as a mold).
	/* Look at every soon to be newly adjacent square. */
	for (i = -max; i <= max; i++)
	{		
		/* New direction */
		new_dir = cycle[chome[prev_dir] + i];
		
		/* New location */
		row = py + ddy[prev_dir] + ddy[new_dir];
		col = px + ddx[prev_dir] + ddx[new_dir];
		
		/* Visible immovable monsters abort running */
		if (cave_m_idx[row][col] > 0)
		{
			monster_type *m_ptr = &mon_list[cave_m_idx[row][col]];
			monster_race *r_ptr = &r_info[m_ptr->r_idx];
			
			/* Visible monster */
			if (m_ptr->ml && (r_ptr->flags1 & (RF1_NEVER_MOVE))) return (TRUE);
		}
	}
	
	/* Looking for open area */
	if (p_ptr->run_open_area)
	{
		/* Hack -- look again */
		for (i = -max; i < 0; i++)
		{
			new_dir = cycle[chome[prev_dir] + i];

			row = py + ddy[new_dir];
			col = px + ddx[new_dir];

			/* Unknown grid or non-wall */
			/* Was: cave_floor_bold(row, col) */
			if (!(cave_info[row][col] & (CAVE_MARK)) ||
			    (!cave_wall_bold(row,col)))
			{
				/* Looking to break right */
				if (p_ptr->run_break_right)
				{
					return (TRUE);
				}
			}

			/* Obstacle */
			else
			{
				/* Looking to break left */
				if (p_ptr->run_break_left)
				{
					return (TRUE);
				}
			}
		}

		/* Hack -- look again */
		for (i = max; i > 0; i--)
		{
			new_dir = cycle[chome[prev_dir] + i];

			row = py + ddy[new_dir];
			col = px + ddx[new_dir];

			/* Unknown grid or non-wall */
			/* Was: cave_floor_bold(row, col) */
			if (!(cave_info[row][col] & (CAVE_MARK)) ||
			    (!cave_wall_bold(row,col)))
			{
				/* Looking to break left */
				if (p_ptr->run_break_left)
				{
					return (TRUE);
				}
			}

			/* Obstacle */
			else
			{
				/* Looking to break right */
				if (p_ptr->run_break_right)
				{
					return (TRUE);
				}
			}
		}
	}


	/* Not looking for open area */
	else
	{
		/* No options */
		if (!option)
		{
			return (TRUE);
		}

		/* One option */
		else if (!option2)
		{
			/* Primary option */
			p_ptr->run_cur_dir = option;

			/* No other options */
			p_ptr->run_old_dir = option;
		}

		/* Two options, examining corners */
		else
		{
			/* Primary option */
			p_ptr->run_cur_dir = option;
			
			/* Hack -- allow curving */
			p_ptr->run_old_dir = option2;
		}
	}
	
	
	/* About to hit a known wall, stop */
	if (see_wall(p_ptr->run_cur_dir, py, px))
	{
		return (TRUE);
	}


	/* Failure */
	return (FALSE);
}



/*
 * Take one step along the current "run" path
 *
 * Called with a real direction to begin a new run, and with zero
 * to continue a run in progress.
 */
void run_step(int dir)
{
	/* Start run */
	if (dir)
	{
		/* Initialize */
		run_init(dir);

		/* Hack -- Set the run counter */
		p_ptr->running = (p_ptr->command_arg ? p_ptr->command_arg : 1000);
	}

	/* Continue run */
	else
	{
		/* Update run */
		if (run_test())
		{
			/* Disturb */
			disturb(0, 0);

			/* Done */
			return;
		}
	}

	/* Decrease counter */
	p_ptr->running--;

	/* Take time */
	p_ptr->energy_use = 100;

	/* Move the player */
	move_player(p_ptr->run_cur_dir);
}


