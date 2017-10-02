/* File: cmd2.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/*
 * Determines the shallowest a player is allowed to go.
 * As time goes on, they are forced deeper and deeper.
 */
int min_depth(void)
{
	int p = 0;
	int d = 0;
	
	// base minimum depth
	while (p < playerturn)
	{
		d += 1;
		p += 1000 + 50*d;
	}
	
	// bounds on the base
	if (d < 1)				d = 1;
	if (d > MORGOTH_DEPTH)	d = MORGOTH_DEPTH;
	
	// can't leave the throne room
	if (p_ptr->depth == MORGOTH_DEPTH)
	{
		d = MORGOTH_DEPTH;
	}
	
	// no limits in the endgame
	if (p_ptr->on_the_run)
	{
		d = 0;
	}
		
	return (d);
}


void note_lost_greater_vault(void)
{
	char note[120];
	char *fmt = "Left without entering %s";
	int y, x;
	bool discovered = FALSE;

	/* Handle lost greater vaults */
	if (g_vault_name[0] != '\0')
	{
		/* Analyze the actual map */
		for (y = 0; y < p_ptr->cur_map_hgt; y++)
		{
			for (x = 0; x < p_ptr->cur_map_wid; x++)
			{
				if ((cave_info[y][x] & (CAVE_G_VAULT)) && (cave_info[y][x] & (CAVE_MARK)))
				{
					discovered = TRUE;
				}
			}
		}
		
		if (discovered)
		{
			strnfmt(note, sizeof(note), fmt, g_vault_name);
			do_cmd_note(note, p_ptr->depth);
		}
		
		g_vault_name[0] = '\0';
	}
}


/*
 * Determines whether a staircase is 'trapped' like a false floor trap.
 * This means you fall a level below where you expected to end up (if you were going upwards),
 * take some minor damage, and have no stairs back.
 *
 * It gets more likely the more stairs you have recently taken.
 * It is designed to stop you stair-scumming.
 */
bool trapped_stairs(void)
{
	int chance;
	
	chance = p_ptr->staircasiness / 100;
	chance = chance * chance * chance;	
	chance = chance / 10000;
	
	//msg_debug("%d, %d", p_ptr->staircasiness, chance);
	
	if (percent_chance(chance))		return (TRUE);
	else							return (FALSE);
}


/*
 * Go up a staircase
 */
void do_cmd_go_up(void)
{	
	int min;
	int new;
	
	/* Verify stairs */
	if (!cave_up_stairs_bold(p_ptr->py, p_ptr->px))
	{
		msg_print("You see no up staircase here.");
		return;
	}

	/* Ironman */
	if (birth_ironman && (silmarils_possessed() == 0))
	{
		msg_print("You have vowed to not to return until you hold a Silmaril.");
		return;
	}
	
	/* Hack -- take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	// Cannot flee Morgoth's throne room without a Silmaril
	if ((p_ptr->depth == MORGOTH_DEPTH) && (silmarils_possessed() == 0))
	{
		msg_print("You enter a maze of staircases, but cannot find your way.");

		return;
	}

    // Store information for the combat rolls window
	combat_roll_special_char = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_char;
	combat_roll_special_attr = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_attr;
    
	// calculate the new depth to arrive at
	min = min_depth();
	if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_LESS_SHAFT) && (p_ptr->depth > 0))
	{
		/* Create a way back (usually) */
		p_ptr->create_stair = FEAT_MORE_SHAFT;

		new = p_ptr->depth - 2;
	}
	else
	{
		/* Create a way back */
		p_ptr->create_stair = FEAT_MORE;

		new = p_ptr->depth - 1;
	}	

	// deal with most cases where you can't find your way
	if ((new < min) && (p_ptr->depth != MORGOTH_DEPTH))
	{
		message(MSG_STAIRS, 0, "You enter a maze of up staircases, but cannot find your way.");
		
		// deal with trapped stairs when trying and failing to go upwards
		if (trapped_stairs())
		{
			msg_print("The stairs crumble beneath you!");
			message_flush();
			msg_print("You fall through...");
			message_flush();
			msg_print("...and land somewhere deeper in the Iron Hells.");
			message_flush();

			// add to the notes file
			do_cmd_note("Fell through a crumbling stair", p_ptr->depth);
			
			// take some damage
			falling_damage(FALSE);
			
			// no stairs back
			p_ptr->create_stair = FALSE;
		}
		
		else
		{
			if (p_ptr->depth == min)
			{
				message(MSG_STAIRS, 0, "You emerge near where you began.");
			}
			else
			{
				message(MSG_STAIRS, 0, "You emerge even deeper in the dungeon.");
			}
			
			if (p_ptr->create_stair == FEAT_MORE)
			{
				/* Change the way back */
				p_ptr->create_stair = FEAT_LESS;
			}
			else
			{
				/* Change the way back */
				p_ptr->create_stair = FEAT_LESS_SHAFT;
			}
		}
		
		new = min;
	}
	
	// deal with cases where you can find your way
	else
	{
		message(MSG_STAIRS, 0, "You enter a maze of up staircases.");
		
		if (silmarils_possessed() > 0)
		{
			message(MSG_STAIRS, 0, "The divine light reveals the way.");
		}
		
		if (p_ptr->depth == MORGOTH_DEPTH)
		{
			if (!p_ptr->morgoth_slain)
			{
				msg_print("As you climb the stair, a great cry of rage and anguish comes from below.");
				msg_print("Make quick your escape: it shall be hard-won.");
			}
			
			// set the 'on the run' flag
			p_ptr->on_the_run = TRUE;

			// remove the 'truce' flag if it hasn't been done already
			p_ptr->truce = FALSE;
		}
		
		// deal with trapped stairs when going upwards
		else if (trapped_stairs())
		{
			msg_print("The stairs crumble beneath you!");
			message_flush();
			msg_print("You fall through...");
			message_flush();
			msg_print("...and land somewhere deeper in the Iron Hells.");
			message_flush();

			// add to the notes file
			do_cmd_note("Fell through a crumbling stair", p_ptr->depth);
			
			// take some damage
			falling_damage(FALSE);
			
			// no stairs back
			p_ptr->create_stair = FALSE;
			
			// go to a lower floor
			new++;
		}
		
	}
	
	// make a note if the player loses a greater vault
	note_lost_greater_vault();
	
	/* New depth */
	p_ptr->depth = new;

	// another staircase has been used...
	p_ptr->stairs_taken++;
	p_ptr->staircasiness += 1000;

	/* Remember disconnected stairs */
	if (birth_discon_stair)	p_ptr->create_stair = FALSE;
	
	/* Leaving */
	p_ptr->leaving = TRUE;
}


/*
 * Go down a staircase
 */
void do_cmd_go_down(void)
{
	int min;
	int new;

	/* Verify stairs */
	if (!cave_down_stairs_bold(p_ptr->py, p_ptr->px))
	{
		msg_print("You see no down staircase here.");
		return;
	}
	
	// special message for tutorial
	if (p_ptr->game_type == -1)
	{
		// display the tutorial leaving text
		if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE)
		{
			pause_with_text(tutorial_leave_text, 5, 10);
		}
		else
		{
			pause_with_text(tutorial_win_text, 5, 10);
		}

		p_ptr->is_dead = TRUE;
		p_ptr->energy_use = 100;
		p_ptr->leaving = TRUE;
		close_game();
		return;
	}
	
	// Do not descend from the Gates
	if (p_ptr->depth == 0)
	{
		msg_print("You have made it to the very gates of Angband and can once more taste the freshness on the air.");
		msg_print("You will not re-enter that fell pit.");
		return;
	}

    // Store information for the combat rolls window
	combat_roll_special_char = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_char;
	combat_roll_special_attr = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_attr;
    
	min = min_depth();
	if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE_SHAFT) && (p_ptr->depth < MORGOTH_DEPTH - 1))
	{
		/* Create a way back (usually) */
		p_ptr->create_stair = FEAT_LESS_SHAFT;
		
		new = p_ptr->depth + 2;
	}
	else
	{
		/* Create a way back */
		p_ptr->create_stair = FEAT_LESS;
		
		new = p_ptr->depth + 1;
	}	

	// warn players if this could lead them to Morgoth's Throne Room
	if (new == MORGOTH_DEPTH)
	{
		if (!p_ptr->on_the_run)
		{
			msg_print("From up this stair comes the harsh din of feasting in Morgoth's own hall.");
			if (!get_check("Are you completely sure you wish to descend? "))
			{
				p_ptr->create_stair = FALSE;
				return;
			}			
		}
	}	
	
	/* Hack -- take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	message(MSG_STAIRS, 0, "You enter a maze of down staircases.");

	// Can never return to the throne room...
	if ((p_ptr->on_the_run) && (new == MORGOTH_DEPTH))
	{
		message(MSG_STAIRS, 0, "Try though you might, you cannot find your way back to Morgoth's throne.");
		message(MSG_STAIRS, 0, "You emerge near where you began.");
		p_ptr->create_stair = FEAT_MORE;
		new = MORGOTH_DEPTH - 1;
	}

	// deal with trapped stairs
	else if (trapped_stairs())
	{
		msg_print("The stairs crumble beneath you!");
		message_flush();
		msg_print("You fall through...");
		message_flush();
		msg_print("...and land somewhere deeper in the Iron Hells.");
		message_flush();

		// add to the notes file
		do_cmd_note("Fell through a crumbling stair", p_ptr->depth);
		
		// take some damage
		falling_damage(FALSE);

		// no stairs back
		p_ptr->create_stair = FALSE;
	}
	
	else if (new < min)
	{
		message(MSG_STAIRS, 0, "You emerge much deeper in the dungeon.");
		new = min;
	}
	
	// make a note if the player loses a greater vault
	note_lost_greater_vault();

	/* New depth */
	p_ptr->depth = new;

	// another staircase has been used...
	p_ptr->stairs_taken++;
	p_ptr->staircasiness += 1000;

	/* Remember disconnected stairs */
	if (birth_discon_stair)	p_ptr->create_stair = FALSE;
		
	/* Leaving */
	p_ptr->leaving = TRUE;
}


/*
 * Simple command to "search" for one turn
 */
void do_cmd_search(void)
{
	/* Allow repeated command */
	if (p_ptr->command_arg)
	{
		/* Set repeat count */
		p_ptr->command_rep = p_ptr->command_arg - 1;

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);

		/* Cancel the arg */
		p_ptr->command_arg = 0;
	}

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;

	/* Search */
	search();
}


/*
 * Hack -- toggle stealth mode
 */
void do_cmd_toggle_stealth(void)
{
    if (p_ptr->climbing)
    {
        msg_print("You cannot use stealth mode while climbing.");
        return;
    }
        
	/* Stop stealth mode */
	if (p_ptr->stealth_mode)
	{
		/* Clear the stealth mode flag */
		p_ptr->stealth_mode = FALSE;

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);
	}

	/* Start stealth mode */
	else
	{
		/* Set the stealth mode flag */
		p_ptr->stealth_mode = TRUE;

		/* Update stuff */
		p_ptr->update |= (PU_BONUS);

		/* Redraw stuff */
		p_ptr->redraw |= (PR_STATE | PR_SPEED);
	}
}



/*
 * Determine if a grid contains a chest
 */
static s16b chest_check(int y, int x)
{
	s16b this_o_idx, next_o_idx = 0;


	/* Scan all objects in the grid */
	for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr;

		/* Get the object */
		o_ptr = &o_list[this_o_idx];

		/* Get the next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Skip unknown chests XXX XXX */
		/* if (!o_ptr->marked) continue; */

		/* Check for chest */
		if (o_ptr->tval == TV_CHEST) return (this_o_idx);
	}

	/* No chest */
	return (0);
}


/*
 * Allocate objects upon opening a chest
 *
 * Disperse treasures from the given chest, centered at (x,y).
 *
 */
static void chest_death(int y, int x, s16b o_idx)
{
	int number, quality;
	
	int i, failed;
	int generated_an_item = FALSE;
	int original_object_level = object_level;

	bool good, great;

	int chesttheme = 0;

	object_type *o_ptr;

	object_type *i_ptr;

	object_type object_type_body;

	/* Get the chest */
	o_ptr = &o_list[o_idx];

	/* Determine how much to drop (see above)
	 *
	 * Small chests get 2-3 objects */

	number = rand_range(2,3);

	/* large chests get 4 */
	if (o_ptr->sval >= SV_CHEST_MIN_LARGE) number = 4;

	/* Zero pval means empty chest */
	if (!o_ptr->pval) return;

	/* Opening a chest */
	object_generation_mode = OB_GEN_MODE_CHEST;

	/* Determine the "value" of the items */
	object_level = ABS(o_ptr->pval);

	/*paranoia*/
	if (object_level < 1) object_level = 1;

	/*the theme of the chest is created during object generation*/
	chesttheme = (o_ptr->xtra1);

	if (o_ptr->sval == SV_CHEST_PRESENT)
	{
		number = 1;
	}
	
	/* Drop some objects (non-chests) */
	for (; number > 0; --number)
	{
		/* Get local object */
		i_ptr = &object_type_body;

		/* Wipe the object */
		object_wipe(i_ptr);

		/*used to determine quality of item, gets more likely
	 	 *to be great as you get deeper.
	 	 */
		if (object_level > 0)	quality = dieroll(object_level);
		else					quality = 0;
		
		if ((o_ptr->sval == SV_CHEST_SMALL_STEEL) || (o_ptr->sval == SV_CHEST_LARGE_STEEL))
		{
			quality += 5;
		}
		if ((o_ptr->sval == SV_CHEST_SMALL_JEWELLED) || (o_ptr->sval == SV_CHEST_LARGE_JEWELLED))
		{
			quality += 10;
		}
		if (o_ptr->sval == SV_CHEST_PRESENT)
		{
			quality += 20;
		}
		
		/* Regular objects in chests will become quite
		 * rare as depth approaches 1250'.
		 * All items with i > 10 are guaranteed good,
		 * all items with i > 15 are guaranteed great,
		 * all items with i > 20  get 4 chances
		 * to become an artefact.
		 * Chests should be extremely lucritive
		 * as a player approaches 1250'.
		 * For potions having the
		 * good and great flags checked increase the
		 * max object generation level, but have no
		 * other effect.  JG
		 */
		if (quality <= 10)
		{
			good = FALSE;
			great = FALSE;
		}
		else if (quality <= 15)
		{
			good = TRUE;
			great = FALSE;
		}
		else if (quality <= 20)
		{
			good = FALSE;
			great = TRUE;
		}
		else
		{
			good = TRUE;
			great = TRUE;
		}

		i = 0;
		failed = FALSE;
		
		while (!make_object(i_ptr, good, great, chesttheme))
		{
			i++;
			if (i == 100)
			{
				failed = TRUE;
				break;
			}
			continue;
		}

		if (!failed)
		{
			/* Drop it in the dungeon */
			generated_an_item = TRUE;
			drop_near(i_ptr, -1, y, x);
		}
	}

	/* Reset the object level */
	object_level = original_object_level;

	/* No longer opening a chest */
	object_generation_mode = OB_GEN_MODE_NORMAL;

	/* Empty */
	o_ptr->pval = 0;

	/*Paranoia, delete chest theme*/
	o_ptr->xtra1 = 0;

	/* Known */
	object_known(o_ptr);

	if (!generated_an_item)
	{
		msg_print("The chest is empty.");
	}
}


/*
 * Chests have traps too.
 *
 * Exploding chest destroys contents (and traps).
 * Note that the chest itself is never destroyed.
 */
static void chest_trap(int y, int x, s16b o_idx)
{
	int trap, dam;

	object_type *o_ptr = &o_list[o_idx];

	(void) x; // casting to soothe compilation warnings
	(void) y;

	/* Ignore disarmed chests */
	if (o_ptr->pval <= 0) return;

	/* Obtain the traps */
	trap = chest_traps[o_ptr->pval];

	// Store information for the combat rolls window
	combat_roll_special_char = object_char(o_ptr);
	combat_roll_special_attr = object_attr(o_ptr);
	
	/* Needle - Hallucination */
	if (trap & (CHEST_NEEDLE_HALLU))
	{
		if (skill_check(NULL, 2, p_ptr->stat_use[A_DEX] * 2, PLAYER) > 0)
		{
			msg_print("A small needle has pricked you!");
			if (allow_player_image(NULL))
			{
				set_image(p_ptr->image + damroll(80,4));
			}
			else
			{
				msg_print("You resist the effects!");
			}
		}
		else
		{
			msg_print("A small needle just misses you.");
		}
	}

	/* Needle - Entrancement */
	if (trap & (CHEST_NEEDLE_ENTRANCE))
	{
		if (skill_check(NULL, 2, p_ptr->stat_use[A_DEX] * 2, PLAYER) > 0)
		{
			msg_print("A small needle has pricked you!");
			if (allow_player_entrancement(NULL))
			{
				set_entranced(damroll(10, 4));
			}
			else
			{
				msg_print("You resist the effects!");
			}
		}
		else
		{
			msg_print("A small needle just misses you.");
		}
	}

	/* Needle - Lose strength */
	if (trap & (CHEST_NEEDLE_LOSE_STR))
	{
		if (skill_check(NULL, 2, p_ptr->stat_use[A_DEX] * 2, PLAYER) > 0)
		{
			msg_print("A small needle has pricked you!");
			(void)do_dec_stat(A_STR, NULL);
		}
		else
		{
			msg_print("A small needle just misses you.");
		}
	}

	/* Confusion Gas */
	if (trap & (CHEST_GAS_CONF))
	{
		msg_print("A noxious vapour escapes from the chest!");
		if (allow_player_confusion(NULL))
		{
			(void)set_confused(p_ptr->confused + damroll(4,4));
		}
		else
		{
			msg_print("You resist the effects.");
		}
	}

	/* Acrid Smoke */
	if (trap & (CHEST_GAS_STUN))
	{
		msg_print("Acrid smoke pours from the chest!");
		if (allow_player_stun(NULL))
		{
			msg_print("It fills your lungs and your mind reels.");
			
			dam = damroll(3,4);
			
			update_combat_rolls1b(NULL, PLAYER, TRUE);
			update_combat_rolls2(3, 4, dam, -1, -1, 0, 0, GF_HURT, FALSE);
			
			take_hit(dam, "a trapped chest");
			
			set_stun(p_ptr->stun + damroll(30, 4));
		}
		else
		{
			msg_print("You resist the effects.");
		}
	}

	/* Poison Gas */
	if (trap & (CHEST_GAS_POISON))
	{
		msg_print("A noxious vapour escapes from the chest!");
		
		update_combat_rolls1b(NULL, PLAYER, TRUE);
		
		(void)pois_dam_pure(10, 4, TRUE);
	}

	/* Flame */
	if (trap & (CHEST_FLAME))
	{
		msg_print("There is a sudden burst of flame!");

		update_combat_rolls1b(NULL, PLAYER, TRUE);

		fire_dam_pure(10, 4, TRUE, "a trapped chest");

		/* Make some noise */
		monster_perception(TRUE, FALSE, -5);
	}
}


/*
 * Attempt to open the given chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns TRUE if repeated commands may continue
 */
static bool do_cmd_open_chest(int y, int x, s16b o_idx)
{
	int score, power, difficulty;

	bool flag = TRUE;

	bool more = FALSE;

	object_type *o_ptr = &o_list[o_idx];

	time_t c;      // time variables
	struct tm *tp; //

	if (o_ptr->sval == SV_CHEST_PRESENT)
	{		
		c = time((time_t *)0);
		tp = localtime(&c);
		
		// cause problems opening presents before Christmas day
		if ((tp->tm_mon == 11) && (tp->tm_mday >= 20) && (tp->tm_mday < 25))// && !((tp->tm_mday == 24) && (strncmp(tp->tm_zone,"CET",4) == 0)))
		{
			if (get_check("Are you sure you wish to open your present before Christmas? "))
			{
				msg_print("You have a very bad feeling about this.");
				p_ptr->cursed = TRUE;
			}
			else
			{
				return (FALSE);
			}
		}
	}

	/* Attempt to unlock it */
	if (o_ptr->pval > 0)
	{
		/* Assume locked, and thus not open */
		flag = FALSE;
		

		/* Get the score in favour (=perception) */
		score = p_ptr->skill_use[S_PER];

		/* Determine trap power based on the chest pval (power is 1--7)*/
		power = 1 + (o_ptr->pval / 4);

		// Base difficulty is the lock power + 5
		difficulty = power + 5;

		/* Penalize some conditions */
		if (p_ptr->blind || no_light() || p_ptr->image) difficulty += 5;
		if (p_ptr->confused) difficulty += 5;

		/* Success -- May still have traps */
		if (skill_check(PLAYER, score, difficulty, NULL) > 0)
		{
			msg_print("You have picked the lock.");
			flag = TRUE;
		}

		/* Failure -- Keep trying */
		else
		{
			/* We may continue repeating */
			more = TRUE;
			flush();
			message(MSG_LOCKPICK_FAIL, 0, "You failed to pick the lock.");
		}
	}

	/* Allowed to open */
	if (flag)
	{
		/* Apply chest traps, if any */
		chest_trap(y, x, o_idx);

		/* Let the Chest drop items */
		chest_death(y, x, o_idx);

		/*squelch chest if autosquelch calls for it*/
		if ((squelch_level[CHEST_INDEX]) == SQUELCH_OPENED_CHESTS)
		{

				delete_object_idx(o_idx);
				msg_print("Chest squelched after it was opened.");
		}
	}

	/* Result */
	return (more);
}


/*
 * Attempt to disarm the chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns TRUE if repeated commands may continue
 */
static bool do_cmd_disarm_chest(int y, int x, s16b o_idx)
{
	int score, power, difficulty, result;

	bool more = FALSE;

	object_type *o_ptr = &o_list[o_idx];


	/* Get the score in favour (=perception) */
	score = p_ptr->skill_use[S_PER];

	/* Determine trap power based on the trap pval (power is 1--7)*/
	power = 1 + (o_ptr->pval / 4);

	// Base difficulty is the lock power
	difficulty = power;

	/* Penalize some conditions */
	if (p_ptr->blind || no_light() || p_ptr->image) difficulty += 5;
	if (p_ptr->confused) difficulty += 5;

	// perform the check
	result = skill_check(PLAYER, score, difficulty, NULL);

	/* Must find the trap first. */
	if (!object_known_p(o_ptr))
	{
		msg_print("You don't see any traps.");
	}

	/* Already disarmed/unlocked */
	else if (o_ptr->pval <= 0)
	{
		msg_print("The chest is not trapped.");
	}

	/* No traps to find. */
	else if (!chest_traps[o_ptr->pval])
	{
		msg_print("The chest is not trapped.");
	}

	/* Success (get a lot of experience) */
	else if (result > 0)
	{
		msg_print("You have disarmed the chest.");
		o_ptr->pval = (0 - o_ptr->pval);
	}

	/* Failure -- Keep trying */
	else if (result > -3)
	{
		/* We may keep trying */
		more = TRUE;
		flush();
		msg_print("You failed to disarm the chest.");
	}

	/* Failure -- Set off the trap */
	else
	{
		msg_print("You set off a trap!");
		chest_trap(y, x, o_idx);
	}

	/* Result */
	return (more);
}


/*
 * Return TRUE if the given feature is an open door
 */
static bool is_open(int feat)
{
	return (feat == FEAT_OPEN);
}


/*
 * Return TRUE if the given feature is a closed door
 */
static bool is_closed(int feat)
{
	return ((feat >= FEAT_DOOR_HEAD) &&
	        (feat <= FEAT_DOOR_TAIL));
}


/*
 * Return TRUE if the given feature is a trap
 */
static bool is_trap(int feat)
{
	bool test_trap = FALSE;

	if ((feat >= FEAT_TRAP_HEAD) &&
	        (feat <= FEAT_TRAP_TAIL)) test_trap = TRUE;

	return (test_trap);
}


/*
 * Return the number of doors/traps around (or under) the character.
 */
static int count_feats(int *y, int *x, bool (*test)(int feat), bool under)
{
	int d;
	int xx, yy;
	int count = 0; /* Count how many matches */

	/* Check around (and under) the character */
	for (d = 0; d < 9; d++)
	{
		/* if not searching under player continue */
		if ((d == 8) && !under) continue;

		/* Extract adjacent (legal) location */
		yy = p_ptr->py + ddy_ddd[d];
		xx = p_ptr->px + ddx_ddd[d];

		/* Paranoia */
		if (!in_bounds_fully(yy, xx)) continue;

		/* Must have knowledge */
		if (!(cave_info[yy][xx] & (CAVE_MARK))) continue;

		/* Not looking for this feature */
		if (!((*test)(cave_feat[yy][xx]))) continue;

		/* Count it */
		++count;

		/* Remember the location of the last door found */
		*y = yy;
		*x = xx;
	}

	/* All done */
	return count;
}


/*
 * Return the number of chests around (or under) the character.
 * If requested, count only trapped chests.
 */
static int count_chests(int *y, int *x, bool trapped)
{
	int d, count, o_idx;

	object_type *o_ptr;

	/* Count how many matches */
	count = 0;

	/* Check around (and under) the character */
	for (d = 0; d < 9; d++)
	{
		/* Extract adjacent (legal) location */
		int yy = p_ptr->py + ddy_ddd[d];
		int xx = p_ptr->px + ddx_ddd[d];

		/* No (visible) chest is there */
		if ((o_idx = chest_check(yy, xx)) == 0) continue;

		/* Grab the object */
		o_ptr = &o_list[o_idx];

		/* Already open */
		if (o_ptr->pval == 0) continue;

		/* No (known) traps here */
		if (trapped &&
		    (!object_known_p(o_ptr) ||
		     (o_ptr->pval < 0) ||
		     !chest_traps[o_ptr->pval]))
		{
			continue;
		}

		/* Count it */
		++count;

		/* Remember the location of the last chest found */
		*y = yy;
		*x = xx;
	}

	/* All done */
	return count;
}


/*
 * Extract a "direction" which will move one step from the player location
 * towards the given "target" location (or "5" if no motion necessary).
 */
static int coords_to_dir(int y, int x)
{
	return (motion_dir(p_ptr->py, p_ptr->px, y, x));
}

/*
 * Determine if a given grid may be "opened"
 */
static bool do_cmd_open_test(int y, int x)
{
	/* Must have knowledge */
	if (!(cave_info[y][x] & (CAVE_MARK)))
	{
		/* Message */
		msg_print("You see nothing there.");

		/* Nope */
		return (FALSE);
	}

	/* Must be a closed door */
	if (!cave_known_closed_door_bold(y,x))
	{
		/* Message */
		message(MSG_NOTHING_TO_OPEN, 0, "You see nothing there to open.");

		/* Nope */
		return (FALSE);
	}

	/* Okay */
	return (TRUE);
}


/*
 * Perform the basic "open" command on doors
 *
 * Assume there is no monster blocking the destination
 *
 * Returns TRUE if repeated commands may continue
 */
bool do_cmd_open_aux(int y, int x)
{
	int score, power, difficulty;

	bool more = FALSE;


	/* Verify legality */
	if (!do_cmd_open_test(y, x)) return (FALSE);


	/* Jammed door */
	if (cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x08)
	{
		/* Stuck */
		msg_print("The door appears to be stuck.");
	}

	/* Locked door */
	else if (cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x01)
	{
	
		/* Get the score in favour (=perception) */
		score = p_ptr->skill_use[S_PER];

		/* Determine door power based on the door power (1 to 7)*/
		power = cave_feat[y][x] - FEAT_DOOR_HEAD;

		// Base difficulty is the door power + 5
		difficulty = power + 5;

		/* Penalize some conditions */
		if (p_ptr->blind || no_light() || p_ptr->image) difficulty += 5;
		if (p_ptr->confused) difficulty += 5;

		/* Success */
		if (skill_check(PLAYER, score, difficulty, NULL) > 0)
		{
			/* Message */
			message(MSG_OPENDOOR, 0, "You have picked the lock.");

			/* Open the door */
			cave_set_feat(y, x, FEAT_OPEN);

			/* Update the visuals */
			p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		}

		/* Failure */
		else
		{
			/* Failure */
			flush();

			/* Message */
			message(MSG_LOCKPICK_FAIL, 0, "You failed to pick the lock.");

			/* We may keep trying */
			more = TRUE;
		}
	}

	/* Closed door */
	else
	{
		/* Open the door */
		cave_set_feat(y, x, FEAT_OPEN);

		/* Update the visuals */
		p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

		/* Sound */
		sound(MSG_OPENDOOR);
	}

	/* Result */
	return (more);
}



/*
 * Open a closed/locked/jammed door or a closed/locked chest.
 *
 * Unlocking a locked door/chest is worth some experience.
 */
void do_cmd_open(void)
{
	int y, x, dir;

	s16b o_idx;

	bool more = FALSE;

	int num_doors, num_chests;

	/* Count closed doors */
	num_doors = count_feats(&y, &x, is_closed, FALSE);

	/* Count chests (locked) */
	num_chests = count_chests(&y, &x, FALSE);

	/* See if only one target */
	if ((num_doors + num_chests) == 1)
	{
		p_ptr->command_dir = coords_to_dir(y, x);
	}
	
	else if ((num_doors + num_chests) == 0)
	{
		msg_print("There is nothing in your square (or adjacent) to open.");
		return;
	}

	/* Get a direction (or abort) */
	if (!get_rep_dir(&dir)) return;

	/* Get location */
	y = p_ptr->py + ddy[dir];
	x = p_ptr->px + ddx[dir];

	/* Check for chests */
	o_idx = chest_check(y, x);


	/* Verify legality */
	if (!o_idx && !do_cmd_open_test(y, x)) return;


	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (confuse_dir(&dir))
	{
		/* Get location */
		y = p_ptr->py + ddy[dir];
		x = p_ptr->px + ddx[dir];

		/* Check for chest */
		o_idx = chest_check(y, x);
	}


	/* Allow repeated command */
	if (p_ptr->command_arg)
	{
		/* Set repeat count */
		p_ptr->command_rep = p_ptr->command_arg - 1;

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);

		/* Cancel the arg */
		p_ptr->command_arg = 0;
	}

	/* Monster */
	if (cave_m_idx[y][x] > 0)
	{
		/* Message */
		msg_print("There is a monster in the way!");

		/* Attack */
		py_attack(y, x, ATT_MAIN);
	}

	/* Chest */
	else if (o_idx)
	{
		/* Open the chest */
		more = do_cmd_open_chest(y, x, o_idx);
	}

	/* Door */
	else
	{
		/* Open the door */
		more = do_cmd_open_aux(y, x);
	}

	/* Cancel repeat unless we may continue */
	if (!more) disturb(0, 0);
}


/*
 * Determine if a given grid may be "closed"
 */
static bool do_cmd_close_test(int y, int x)
{
	/* Must have knowledge */
	if (!(cave_info[y][x] & (CAVE_MARK)))
	{
		/* Message */
		msg_print("You see nothing there.");

		/* Nope */
		return (FALSE);
	}

 	/* Require open/broken door */
	if ((cave_feat[y][x] != FEAT_OPEN) &&
	    (cave_feat[y][x] != FEAT_BROKEN))
	{
		/* Message */
		msg_print("You see nothing there to close.");

		/* Nope */
		return (FALSE);
	}

	/* Okay */
	return (TRUE);
}


/*
 * Perform the basic "close" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns TRUE if repeated commands may continue
 */
static bool do_cmd_close_aux(int y, int x)
{
	bool more = FALSE;


	/* Verify legality */
	if (!do_cmd_close_test(y, x)) return (FALSE);


	/* Broken door */
	if (cave_feat[y][x] == FEAT_BROKEN)
	{
		/* Message */
		msg_print("The door appears to be broken.");
	}

	/* Open door */
	else
	{
		/* Close the door */
		cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);

		/* Update the visuals */
		p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

		/* Sound */
		sound(MSG_SHUTDOOR);
	}

	/* Result */
	return (more);
}


/*
 * Close an open door.
 */
void do_cmd_close(void)
{
	int y, x, dir;

	bool more = FALSE;

	/* Count open doors */
	if (count_feats(&y, &x, is_open, FALSE) == 1)
	{
		p_ptr->command_dir = coords_to_dir(y, x);
	}

	else if (count_feats(&y, &x, is_open, FALSE) == 0)
	{
		msg_print("There is no adjacent door to close.");
		return;
	}
	
	/* Get a direction (or abort) */
	if (!get_rep_dir(&dir)) return;

	/* Get location */
	y = p_ptr->py + ddy[dir];
	x = p_ptr->px + ddx[dir];


	/* Verify legality */
	if (!do_cmd_close_test(y, x)) return;


	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (confuse_dir(&dir))
	{
		/* Get location */
		y = p_ptr->py + ddy[dir];
		x = p_ptr->px + ddx[dir];
	}

	/* Allow repeated command */
	if (p_ptr->command_arg)
	{
		/* Set repeat count */
		p_ptr->command_rep = p_ptr->command_arg - 1;

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);

		/* Cancel the arg */
		p_ptr->command_arg = 0;
	}

	/* Monster */
	if (cave_m_idx[y][x] > 0)
	{
		/* Message */
		msg_print("There is a monster in the way!");

		/* Attack */
		py_attack(y, x, ATT_MAIN);
	}

	/* Door */
	else
	{
		/* Close door */
		more = do_cmd_close_aux(y, x);
	}

	/* Cancel repeat unless told not to */
	if (!more) disturb(0, 0);
}



/*
 * Exchange places with a monster.
 */
void do_cmd_exchange(void)
{
	int y, x, dir;
	
	monster_type *m_ptr;
	monster_race *r_ptr;
	char m_name[80];
	
	if (!p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
	{
		msg_print("You need the ability 'exchange places' to use this command.");
		return;
	}
	
	/* Get a direction (or abort) */
	if (!get_rep_dir(&dir)) return;
		
	/* Get location */
	y = p_ptr->py + ddy[dir];
	x = p_ptr->px + ddx[dir];

	// deal with overburdened characters
	if (p_ptr->total_weight > weight_limit()*3/2)
	{
		/* Abort */
		msg_print("You are too burdened to move.");
		
		return;
	}
	
	// Can't exchange from within pits
	if (cave_pit_bold(p_ptr->py,p_ptr->px))
	{
		/* Message */
		msg_print("You would have to escape the pit before being able to exchange places.");
		
		return;
	}
	// Can't exchange from within webs
	else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
	{
		/* Message */
		msg_print("You would have to escape the web before being able to exchange places.");
		
		return;
	}
	else if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
	{
		/* Message */
		msg_print("You cannot see a monster there to exchange places with.");
		
		return;
	}
	else if (cave_wall_bold(y, x))
	{
		/* Message */
		msg_print("You cannot enter the wall.");
		
		return;
	}
	else if (cave_any_closed_door_bold(y, x))
	{
		/* Message */
		msg_print("You cannot enter the closed door.");
		
		return;
	} 
	else if (cave_feat[y][x] == FEAT_RUBBLE)
	{
		/* Message */
		msg_print("You cannot enter the rubble.");
		
		return;
	}
	else
	{
		m_ptr = &mon_list[cave_m_idx[y][x]];
		r_ptr = &r_info[m_ptr->r_idx];
		
		if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) || (r_ptr->flags1 & (RF1_HIDDEN_MOVE)))
		{
			monster_desc(m_name, sizeof(m_name), m_ptr, 0);

			/* Message */
			msg_format("You cannot get past %s.", m_name);
			
			return;
		}
	}
	
	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	/* Apply confusion */
	if (confuse_dir(&dir))
	{
		/* Get location */
		y = p_ptr->py + ddy[dir];
		x = p_ptr->px + ddx[dir];
	}
	
	// re-check for a visible monster (in case confusion changed the move)
	if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
	{
		/* Message */
		msg_print("You cannot see a monster there to exchange places with.");
		
		return;
	}

	else if (cave_wall_bold(y, x))
	{
		/* Message */
		msg_print("There is a wall in the way.");
		
		return;
	}
	else if (cave_any_closed_door_bold(y, x))
	{
		/* Message */
		msg_print("There is a door in the way.");
		
		return;
	} 
	else if (cave_feat[y][x] == FEAT_RUBBLE)
	{
		/* Message */
		msg_print("There is a pile of rubble in the way.");
		
		return;
	} 
	else if (cave_feat[y][x] == FEAT_CHASM)
	{
		/* Message */
		msg_print("You cannot exchange places over the chasm.");
		
		return;
	}
	
	// recalculate the monster info (in case confusion changed the move)
	m_ptr = &mon_list[cave_m_idx[y][x]];
	r_ptr = &r_info[m_ptr->r_idx];
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);
		
	/* Message */
	msg_format("You exchange places with %s.", m_name);
	
	// attack of opportunity
	if ((m_ptr->alertness >= ALERTNESS_ALERT) && !m_ptr->confused && !(r_ptr->flags2 & (RF2_MINDLESS)))
	{
		msg_print("It attacks you as you slip past.");
		make_attack_normal(m_ptr);
	}

	// Alert the monster
	make_alert(m_ptr);

	// Swap positions with the monster
	monster_swap(p_ptr->py, p_ptr->px, y, x);
	
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
 * Determine if a given grid may be "tunneled"
 */
static bool do_cmd_tunnel_test(int y, int x)
{
	/* Must have knowledge */
	if (!(cave_info[y][x] & (CAVE_MARK)))
	{
		/* Message */
		msg_print("You see nothing there.");

		/* Nope */
		return (FALSE);
	}

	/* Must be a wall or rubble */
	if (cave_floor_bold(y, x))
	{
		/* Message */
		msg_print("You see nothing there to tunnel.");

		/* Nope */
		return (FALSE);
	}
	if (cave_known_closed_door_bold(y,x))
	{
		/* Message */
		msg_print("You cannot tunnel through a door. Try bashing it.");
		
		/* Nope */
		return (FALSE);
	}
    
	/* Permanent */
	if (cave_feat[y][x] == FEAT_WALL_PERM)
	{
		/* Message */
		msg_print("You cannot tunnel any further in that direction.");
        
        /* Nope */
		return (FALSE);
	}
	

	/* Okay */
	return (TRUE);
}


/*
 * Tunnel through wall.  Assumes valid location.
 *
 * Note that it is impossible to "extend" rooms past their
 * outer walls (which are actually part of the room).
 *
 * Attempting to do so will produce floor grids which are not part
 * of the room, and whose "illumination" status do not change with
 * the rest of the room.
 */
static bool twall(int y, int x)
{
	/* Paranoia -- Require a wall or door or some such */
	if (cave_floor_bold(y, x)) return (FALSE);

	/* Sound */
	sound(MSG_DIG);

	/* Forget the wall */
	//cave_info[y][x] &= ~(CAVE_MARK);

	/* Granite */
	/* Quartz */
	if (cave_feat[y][x] >= FEAT_QUARTZ)
	{
		/* Leave a pile of rubble */
		cave_set_feat(y, x, FEAT_RUBBLE);
	}
    
	/* Rubble */
	else if (cave_feat[y][x] == FEAT_RUBBLE)
	{
		/* Clear the rubble */
		cave_set_feat(y, x, FEAT_FLOOR);
	}
    
	/* Secret doors */
	else
	{
        /* Leave a closed door */
        place_closed_door(y, x);
    }

	/* Update the visuals */
	p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Result */
	return (TRUE);
}


/*
 * Perform the basic "tunnel" command
 *
 * Assumes that no monster is blocking the destination
 *
 * Uses "twall" (above) to do all "terrain feature changing".
 *
 * Returns TRUE if repeated commands may continue
 */
static bool do_cmd_tunnel_aux(int y, int x)
{
    int i;
    int item;
	bool more = FALSE;
    bool digger_choice = FALSE;
	int difficulty;
    int digging_score = 0;
    char o_name[80];
	char success_message[80];
	char failure_message[80];
	object_type *o_ptr;
	object_type *digger_ptr = NULL; // default to soothe compiler warnings
    
    u32b f1, f2, f3;
	
	/* Verify legality */
	if (!do_cmd_tunnel_test(y, x)) return (FALSE);

    // examine the wielded weapon
    o_ptr = &inventory[INVEN_WIELD];
    object_flags(o_ptr, &f1, &f2, &f3);

    // if it is a digger, then use it
    if (f1 & (TR1_TUNNEL))
    {
        digging_score = o_ptr->pval;
        digger_ptr = o_ptr;
    }
    else
    {
        // find one or more diggers in the pack
        for (i = 0; i < INVEN_PACK; i++)
        {
            o_ptr = &inventory[i];
            
            object_flags(o_ptr, &f1, &f2, &f3);
            
            if (f1 & (TR1_TUNNEL))
            {
                if (digging_score > 0)
                {
                    digger_choice = TRUE;
                }
                digging_score = o_ptr->pval;
                digger_ptr = o_ptr;
            }
        }
        
        if (digger_choice)
        {
            /* Restrict the choices */
            item_tester_hook = item_tester_hook_digger;
            
            /* Get an item */
            if (!get_item(&item, "Use which digger? ", "You are not carrying a shovel or mattock.", (USE_INVEN))) return (FALSE);
            else
            {
                /* Get the object */
                if (item >= 0)
                {
                    digger_ptr = &inventory[item];
                }
                else
                {
                    digger_ptr = &o_list[0 - item];
                }
                
                digging_score = digger_ptr->pval;
            }
        }
    }
    
    // abort if you have no digger
    if (digging_score == 0)
    {
        // confused players trying to dig without a digger waste their turn
        // (otherwise control-dir is safe in a corridor)
        if (p_ptr->confused)
        {
            if (cave_feat[y][x] == FEAT_RUBBLE)     msg_print("You bump into the rubble.");
            else                                    msg_print("You bump into the wall.");
            
            return (FALSE);
        }
        
        else
        {
            msg_print("You are not carrying a shovel or mattock.");
            
            // reset the action type
            p_ptr->previous_action[0] = ACTION_NOTHING;
            
            // don't take a turn
            p_ptr->energy_use = 0;
            
            return (FALSE);
        }
    }
    
    // get the short name of the item
    object_desc(o_name, sizeof(o_name), digger_ptr, FALSE, -1);
        
	/* Granite */
    if (cave_feat[y][x] >= FEAT_WALL_EXTRA)
	{
		difficulty = 3;
		my_strcpy(success_message, "You break through the granite.", sizeof (success_message));
        
        if (difficulty > digging_score)
        {
            strnfmt(failure_message, sizeof(failure_message), "You are unable to break the granite with your %s.", o_name);
        }
        else 
        {
            strnfmt(failure_message, sizeof(failure_message), "You are not strong enough to break the granite.");
        }
	}
	/* Quartz */
	else if (cave_feat[y][x] >= FEAT_QUARTZ)
	{
		difficulty = 2;
		my_strcpy(success_message, "You shatter the quartz.", sizeof (success_message));
        
        if (difficulty > digging_score)
        {
            strnfmt(failure_message, sizeof(failure_message), "You are unable to break the quartz with your %s.", o_name);
        }
        else
        {
            strnfmt(failure_message, sizeof(failure_message), "You are not strong enough to break the quartz.");
        }
    }
	/* Rubble */
	else if (cave_feat[y][x] == FEAT_RUBBLE)
	{
		difficulty = 1;
		my_strcpy(success_message, "You clear the rubble.", sizeof (success_message));
        
        if (difficulty > digging_score)
        {
            strnfmt(failure_message, sizeof(failure_message), "You are unable to shift the rubble with your %s.", o_name);
        }
        else
        {
            strnfmt(failure_message, sizeof(failure_message), "You are not strong enough to shift the rubble.");
        }
	}
	/* Secret doors */
	else
	{
		difficulty = 3;
		my_strcpy(success_message, "You uncover a secret door.", sizeof (success_message));

        if (difficulty > digging_score)
        {
            strnfmt(failure_message, sizeof(failure_message), "You are unable to break the granite with your %s.", o_name);
        }
        else
        {
            strnfmt(failure_message, sizeof(failure_message), "You are not strong enough to break the granite.");
        }
	}
    
	/* test for success */
	if ((difficulty <= digging_score) && (difficulty <= p_ptr->stat_use[A_STR]))
	{
		u32b f1, f2, f3;
		object_flags(digger_ptr, &f1, &f2, &f3);
		
		/* Make a lot of noise */
		monster_perception(TRUE, FALSE, -10);
		
		twall(y, x);
		msg_print(success_message);

		// Possibly identify the digger
		if (!object_known_p(digger_ptr) && (f1 & (TR1_TUNNEL)))
		{ 
            char o_short_name[80];
            char o_full_name[80];
            
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), digger_ptr, FALSE, 0);
            
            ident(digger_ptr);
            
            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), digger_ptr, TRUE, 3);
            
            /* Print the messages */
            msg_format("You notice that your %s is especially suited to tunneling.", o_short_name);
            msg_format("You are wielding %s.", o_full_name);
		}
	}
    
	else
	{
        msg_print(failure_message);

        // confused players trying to dig without a digger waste their turn
        // (otherwise control-dir is safe in a corridor)
        if (!p_ptr->confused)
        {
            // reset the action type
            p_ptr->previous_action[0] = ACTION_NOTHING;
            
            // don't take a turn
            p_ptr->energy_use = 0;
        }
                        
        return (FALSE);
        
	}

	// Break the truce if creatures see
	break_truce(FALSE);
    
    // provoke attacks of opportunity from adjacent monsters
    attacks_of_opportunity(0, 0);
	
	/* Result */
	return (more);
}


/*
 * Tunnel through "walls" (including rubble and secret doors)
 *
 * Digging is only possible with a "digger" weapon.
 */
void do_cmd_tunnel(void)
{
	int y, x, dir;

	bool more = FALSE;

	/* Get a direction (or abort) */
	if (!get_rep_dir(&dir)) return;

	/* Get location */
	y = p_ptr->py + ddy[dir];
	x = p_ptr->px + ddx[dir];

	/* Oops */
	if (!do_cmd_tunnel_test(y, x)) return;
	
	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (confuse_dir(&dir))
	{
		/* Get location */
		y = p_ptr->py + ddy[dir];
		x = p_ptr->px + ddx[dir];
	}

	/* Allow repeated command */
	if (p_ptr->command_arg)
	{
		/* Set repeat count */
		p_ptr->command_rep = p_ptr->command_arg - 1;

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);

		/* Cancel the arg */
		p_ptr->command_arg = 0;
	}

	/* Monster */
	if (cave_m_idx[y][x] > 0)
	{
		/* Message */
		msg_print("There is a monster in the way!");

		/* Attack */
		py_attack(y, x, ATT_MAIN);
	}

	/* Walls */
	else
	{
		/* Tunnel through walls */
		more = do_cmd_tunnel_aux(y, x);
	}
	
	/* Cancel repetition unless we can continue */
	if (!more) disturb(0, 0);
}


/*
 * Determine if a given grid may be "disarmed"
 */
static bool do_cmd_disarm_test(int y, int x)
{
	bool can_disarm = FALSE;

	/* Must have knowledge */
	if (!(cave_info[y][x] & (CAVE_MARK)))
	{
		/* Message */
		msg_print("You see nothing there.");

		/* Nope */
		return (FALSE);
	}

	/* Require an actual trap */
	if (cave_trap_bold(y,x) && !cave_floorlike_bold(y,x))
	{
		can_disarm = TRUE;
	}

	/*not a trap*/
	else msg_print("You see nothing there to disarm.");

	/* Okay */
	return (can_disarm);
}


/*
 * Attempts to break free of a web.
 */
bool break_free_of_web(void)
{
	int difficulty = 7;
	int score = MAX(p_ptr->stat_use[A_STR] * 2, difficulty-8); // capped so you always have some chance
	
	/* Disturb the player */
	disturb(0, 0);
	
	// Free action helps a lot
	if (p_ptr->free_act) difficulty -= 10 * p_ptr->free_act;
	
	// Spider bane bonus helps
	difficulty -= spider_bane_bonus();
	
	if (skill_check(PLAYER, score, difficulty, NULL) <= 0)
	{	
		msg_print("You fail to break free of the web.");
		
		/* Take a turn */
		p_ptr->energy_use = 100;
		
		// store the action type
		p_ptr->previous_action[0] = ACTION_MISC;
		
		return (FALSE);
	}
	else
	{
		msg_print("You break free!");
		
		/* Forget the trap */
		cave_info[p_ptr->py][p_ptr->px] &= ~(CAVE_MARK);
		
		/* Remove the trap */
		cave_set_feat(p_ptr->py, p_ptr->px, FEAT_FLOOR);
		
		return (TRUE);
	}
}

/*
 * Perform the basic "disarm" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns TRUE if repeated commands may continue
 */
static bool do_cmd_disarm_aux(int y, int x)
{
	int score, difficulty, result;
	int power = 0; // default to soothe compiler warnings

	cptr name;

	bool more = FALSE;


	/* Verify legality */
	if (!do_cmd_disarm_test(y, x)) return (FALSE);

	/* Get the trap name */
	name = (f_name + f_info[cave_feat[y][x]].name);

	/* Get the score in favour (=perception) */
	score = p_ptr->skill_use[S_PER];

	/* Determine trap power based on the dungeon level (1--7)*/
	//power = 1 + p_ptr->depth / 5;
	//if (p_ptr->depth == 0) power = 7;

	switch (cave_feat[y][x])
	{
		case FEAT_TRAP_FALSE_FLOOR:
		{
			power = 6;
			break;
		}
		case FEAT_TRAP_PIT:
		{
			msg_format("You cannot disarm the %s.", name);
			return (FALSE);
		}
		case FEAT_TRAP_SPIKED_PIT:
		{
			msg_format("You cannot disarm the %s.", name);
			return (FALSE);
		}
		case FEAT_TRAP_DART:
		{
			power = 3;
			break;
		}
		case FEAT_TRAP_GAS_CONF:
		{
			power = 5;
			break;
		}
		case FEAT_TRAP_GAS_MEMORY:
		{
			power = 5;
			break;
		}
		case FEAT_TRAP_ALARM:
		{
			power = 2;
			break;
		}
		case FEAT_TRAP_FLASH:
		{
			power = 4;
			break;
		}
		case FEAT_TRAP_CALTROPS:
		{
			power = 1;
			break;
		}
		case FEAT_TRAP_ROOST:
		{
			msg_format("You cannot disarm the %s.", name);
			return (FALSE);
		}
		case FEAT_TRAP_WEB:
		{
			if ((p_ptr->py == y) && (p_ptr->px == x))
			{
				int more = break_free_of_web();
				return (!more);
			}
			else
			{
				msg_format("You cannot disarm the %s.", name);
				return (FALSE);
			}
		}
		case FEAT_TRAP_DEADFALL:
		{
			power = 7;
			break;
		}
		case FEAT_TRAP_ACID:
		{
			power = 5;
			break;
		}
	}
	
	/* Prevent glyphs of warding granting exp. */
	if (cave_feat[y][x] == FEAT_GLYPH) power = 0;

	// Base difficulty is the trap power
	difficulty = power;

	/* Penalize some conditions */
	if (p_ptr->blind || no_light() || p_ptr->image) difficulty += 5;
	if (p_ptr->confused) difficulty += 5;

	// perform the check
	result = skill_check(PLAYER, score, difficulty, NULL);

	/* Success, always succeed with player trap */
	if (result > 0)
	{
		/* Special message for glyphs. */
		if (cave_feat[y][x] == FEAT_GLYPH)
			msg_format("You have scuffed the %s.", name);

		/* Normal message otherwise */
		else msg_format("You have disarmed the %s.", name);

		/* Forget the trap */
		cave_info[y][x] &= ~(CAVE_MARK);

		/* Remove the trap */
		cave_set_feat(y, x, FEAT_FLOOR);
	}

	/* Failure by a small amount allows one to keep trying */
	else if (result > -3)
	{
		/* Failure */
		flush();

		/* Message */
		msg_format("You failed to disarm the %s.", name);

		/* We may keep trying */
		more = TRUE;
	}

	/* Failure by a larger amount sets off the trap */
	else
	{
		/* Message */
		monster_swap(p_ptr->py, p_ptr->px, y, x);
		msg_format("You set off the %s!", name);

		/* Hit the trap */
		hit_trap(y, x);
	}

	/* Result */
	return (more);
}


/*
 * Disarms a trap, or a chest
 */
void do_cmd_disarm(void)
{
	int y, x, dir;

	s16b o_idx;

	bool more = FALSE;

	int num_traps, num_chests;

	/* Count visible traps */
	num_traps = count_feats(&y, &x, is_trap, TRUE);

	/* Count chests (trapped) */
	num_chests = count_chests(&y, &x, TRUE);

	/* See if only one target */
	if (num_traps || num_chests)
	{
		if (num_traps + num_chests <= 1)
			p_ptr->command_dir = coords_to_dir(y, x);
	}

	/* Get a direction (or abort) */
	if (!get_rep_dir(&dir)) return;

	/* Get location */
	y = p_ptr->py + ddy[dir];
	x = p_ptr->px + ddx[dir];

	/* Check for chests */
	o_idx = chest_check(y, x);


	/* Verify legality */
	if (!o_idx && !do_cmd_disarm_test(y, x)) return;

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (confuse_dir(&dir))
	{
		/* Get location */
		y = p_ptr->py + ddy[dir];
		x = p_ptr->px + ddx[dir];

		/* Check for chests */
		o_idx = chest_check(y, x);
	}


	/* Allow repeated command */
	if (p_ptr->command_arg)
	{
		/* Set repeat count */
		p_ptr->command_rep = p_ptr->command_arg - 1;

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);

		/* Cancel the arg */
		p_ptr->command_arg = 0;
	}

	/* Monster */
	if (cave_m_idx[y][x] > 0)
	{
		/* Message */
		msg_print("There is a monster in the way!");

		/* Attack */
		py_attack(y, x, ATT_MAIN);
	}

	/* Chest */
	else if (o_idx)
	{
		/* Disarm the chest */
		more = do_cmd_disarm_chest(y, x, o_idx);
	}

	/* Disarm trap */
	else
	{
		/* Disarm the trap */
		more = do_cmd_disarm_aux(y, x);
	}

	/* Cancel repeat unless told not to */
	if (!more) disturb(0, 0);
}


/*
 * Determine if a given grid may be "bashed"
 */
static bool do_cmd_bash_test(int y, int x)
{
	/* Must have knowledge */
	if (!(cave_info[y][x] & (CAVE_MARK)))
	{
		/* Message */
		msg_print("You see nothing there.");

		/* Nope */
		return (FALSE);
	}

	/* Require a door */
	if (!cave_known_closed_door_bold(y,x))
	{
		/* Message */
		msg_print("You see no door there to bash.");

		/* Nope */
		return (FALSE);
	}

	/* Okay */
	return (TRUE);
}


/*
 * Perform the basic "bash" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns TRUE if repeated commands may continue
 */
static bool do_cmd_bash_aux(int y, int x)
{
	int score, difficulty, power;

	bool more = FALSE;
	bool success = FALSE;
	
	/* Verify legality */
	if (!do_cmd_bash_test(y, x)) return (FALSE);
	
	/* If it was actually a door */
	if (cave_known_closed_door_bold(y,x))
	{
		/* Message */
		msg_print("You slam into the door!");

		// get the score in favour (=str)
		score = p_ptr->stat_use[A_STR] * 2;
		
		// get the difficulty
		power = ((cave_feat[y][x] - FEAT_DOOR_HEAD) & 0x07);
		
		// the base difficulty is the door power 
		difficulty = 0;
		difficulty += power;
		
		if (skill_check(PLAYER, score, difficulty, NULL) > 0)
		{
			success = TRUE;
			
			if (cave_feat[y][x] == FEAT_SECRET)
			{
				if (singing(SNG_SILENCE))
				{
					/* Message */
					message(MSG_OPENDOOR, 0, "A door opens with a muffled crash!");
				}
				else
				{
					/* Message */
					message(MSG_OPENDOOR, 0, "A door crashes open!");
				}
			}
			else
			{
				if (singing(SNG_SILENCE))
				{
					/* Message */
					message(MSG_OPENDOOR, 0, "The door opens with a muffled crash!");
				}
				else
				{
					/* Message */
					message(MSG_OPENDOOR, 0, "The door crashes open!");
				}
				
			}

			/* Break down the door */
			if (one_in_(2))
			{
				cave_set_feat(y, x, FEAT_BROKEN);
			}
			
			/* Open the door */
			else
			{
				cave_set_feat(y, x, FEAT_OPEN);
			}

			// Move the player onto the door square
			monster_swap(p_ptr->py, p_ptr->px, y, x);
			
			/* Make a lot of noise */
			monster_perception(TRUE, FALSE, -10);
			
			/* Update the visuals */
			p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		}
	}
	
	if (!success)
	{
		if (cave_known_closed_door_bold(y,x))
		{
			/* Message */
			msg_print("The door holds firm.");
		}

		/* Stuns */
		if (allow_player_stun(NULL))
		{ 
			(void)set_stun(p_ptr->stun + 10);
		}
		else
		{
			/* Allow repeated bashing */
			more = TRUE;
		}
		
		/* Make some noise */
		monster_perception(TRUE, FALSE, -5);
	}

	/* Result */
	return (more);
}


/*
 * Bash open a door, success based on character strength
 *
 * For a closed door, pval is positive if locked; negative if stuck.
 *
 * For an open door, pval is positive for a broken door.
 *
 * A closed door can be opened - harder if locked. Any door might be
 * bashed open (and thereby broken). Bashing a door is (potentially)
 * faster! You move into the door way. To open a stuck door, it must
 * be bashed.
 *
 * Creatures can also open or bash doors, see elsewhere.
 */
void do_cmd_bash(void)
{
	int y, x, dir;

	/* Get a direction (or abort) */
	if (!get_rep_dir(&dir)) return;

	/* Get location */
	y = p_ptr->py + ddy[dir];
	x = p_ptr->px + ddx[dir];


	/* Verify legality */
	if (!do_cmd_bash_test(y, x)) return;

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (confuse_dir(&dir))
	{
		/* Get location */
		y = p_ptr->py + ddy[dir];
		x = p_ptr->px + ddx[dir];
	}

	/* Allow repeated command */
	if (p_ptr->command_arg)
	{
		/* Set repeat count */
		p_ptr->command_rep = p_ptr->command_arg - 1;

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);

		/* Cancel the arg */
		p_ptr->command_arg = 0;
	}

	/* Monster */
	if (cave_m_idx[y][x] > 0)
	{
		/* Message */
		msg_print("There is a monster in the way!");

		/* Attack */
		py_attack(y, x, ATT_MAIN);
	}

	/* Door */
	else
	{
		/* Bash the door */
		if (!do_cmd_bash_aux(y, x))
		{
			/* Cancel repeat */
			disturb(0, 0);
		}
	}
}

/*
 * Manipulate an adjacent grid in some way
 *
 * Attack monsters, tunnel through walls, disarm traps, open doors.
 *
 * This command must always take energy, to prevent free detection
 * of invisible monsters.
 *
 * The "semantics" of this command must be chosen before the player
 * is confused, and it must be verified against the new grid.
 */
void do_cmd_alter(void)
{
	int y, x, dir;

	int feat;
	
	bool chest_trap = FALSE;
	bool chest_present = FALSE;

	bool more = FALSE;

	/* Get a direction */
	if (!get_rep_dir(&dir)) return;

	/* Get location */
	y = p_ptr->py + ddy[dir];
	x = p_ptr->px + ddx[dir];

	/* Original feature */
	feat = cave_feat[y][x];

	/* Must have knowledge to know feature XXX XXX */
	if (!(cave_info[y][x] & (CAVE_MARK))) feat = FEAT_NONE;

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	/* Apply confusion */
	if (confuse_dir(&dir))
	{
		/* Get location */
		y = p_ptr->py + ddy[dir];
		x = p_ptr->px + ddx[dir];
	}


	/* Allow repeated command */
	if (p_ptr->command_arg)
	{
		/* Set repeat count */
		p_ptr->command_rep = p_ptr->command_arg - 1;

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);

		/* Cancel the arg */
		p_ptr->command_arg = 0;
	}

	// check for chests and chest traps
	if (cave_o_idx[y][x])
	{
		object_type *o_ptr = &o_list[cave_o_idx[y][x]];
		
		if (o_ptr->tval == TV_CHEST)
		{
			chest_present = TRUE;

			if ((o_ptr->pval > 0) && chest_traps[o_ptr->pval] && object_known_p(o_ptr)) chest_trap = TRUE;
		}
	}

	/*Is there a monster on the space?*/
	if (cave_m_idx[y][x] > 0)
	{
		py_attack(y, x, ATT_MAIN);
	}
	
	// deal with players who can't see the square
	else if ((dir != 5) && !(cave_info[y][x] & (CAVE_MARK)))
	{
		if (cave_floor_bold(y,x))
		{
			/* Oops */
			msg_print("You strike, but there is nothing there.");
		}
		else
		{
			msg_print("You hit something hard.");
			cave_info[y][x] |= (CAVE_MARK);
			lite_spot(y, x);
		}
	}

	/* Tunnel through walls */
	else if (cave_wall_bold(y,x))
	{
		/* Tunnel */
		do_cmd_tunnel_aux(y, x);
	}

	/* Bash doors */
	else if (cave_known_closed_door_bold(y,x))
	{
		/* Bash */
		do_cmd_bash_aux(y, x);
	}

	/* Disarm known dungeon traps */
	else if (cave_trap_bold(y,x) && !cave_floorlike_bold(y,x))
	{
		/* Disarm */
		more = do_cmd_disarm_aux(y, x);
	}

	/* Disarm known chest traps */
	else if (chest_trap)
	{
		/* Disarm */
		more = do_cmd_disarm_chest(y, x, cave_o_idx[y][x]);
	}
	
	/* Open chest with no known traps */
	else if (chest_present)
	{
		/* Disarm */
		more = do_cmd_open_chest(y, x, cave_o_idx[y][x]);
	}
	
	/* Close open doors */
	else if (feat == FEAT_OPEN)
	{
		if (dir == 5)
		{
			msg_print("To close the door you would need to move out from the doorway.");
		}
		else
		{
			/* Close */
			do_cmd_close_aux(y, x);
		}
	}

	/* Ascend upwards stairs */
	else if ((dir == 5) && ((feat == FEAT_LESS) || (feat == FEAT_LESS_SHAFT)))
	{
		/* Ascend */
		if (get_check("Are you sure you wish to ascend? "))		do_cmd_go_up();
	}
	
	/* Descend downwards stairs */
	else if ((dir == 5) && ((feat == FEAT_MORE) || (feat == FEAT_MORE_SHAFT)))
	{
		/* Descend */
		if (get_check("Are you sure you wish to descend? "))	do_cmd_go_down();
	}

	/* Use forges */
	else if ((dir == 5) && cave_forge_bold(y,x))
	{
		/* Use forge */
		do_cmd_smithing_screen();
		more = TRUE;

		// don't take a turn...
		p_ptr->energy_use = 0;
	}

	/* Pick up items */
	else if ((dir == 5) && (cave_o_idx[y][x]))
	{
		/* Get item */
		do_cmd_pickup();
	}
	
	/* Oops */
	else if (dir == 5)
	{
		/* Oops */
		msg_print("There is nothing here to use.");

		// don't take a turn...
		p_ptr->energy_use = 0;
	}
	
	/* Oops */
	else
	{
		/* Oops */
		msg_print("You strike, but there is nothing there.");
	}

	/* Cancel repetition unless we can continue */
	if (!more) disturb(0, 0);
}


/*
 * Determine if a given grid may be "walked"
 */
bool do_cmd_walk_test(int y, int x)
{
	/* Hack -- walking obtains knowledge XXX XXX */
	if (!(cave_info[y][x] & (CAVE_MARK))) return (TRUE);

	/* Allow attack on visible monsters */
	if ((cave_m_idx[y][x] > 0) && (mon_list[cave_m_idx[y][x]].ml))
	{
		return TRUE;
	}

	/* Require open space */
	if (!cave_floor_bold(y, x))
	{
		/* Rubble */
		if (cave_feat[y][x] == FEAT_RUBBLE)
		{
			/* Message */
			msg_print("There is a pile of rubble in the way!");

			// store the action type
			p_ptr->previous_action[0] = ACTION_MISC;
		}

		/* Door */
		else if (cave_known_closed_door_bold(y,x))
		{

			/* Hack -- Handle "easy_alter" */
			return (TRUE);
		}

		/* Wall */
		else
		{
			/* Message */
			msg_print("There is a wall in the way!");

			// store the action type
			p_ptr->previous_action[0] = ACTION_MISC;
		}

		/* Nope */
		return (FALSE);
	}

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for the "walk" and "jump" commands.
 */
void do_cmd_walk(void)
{
	int y, x, dir;
    
	/* Get a direction (or abort) */
	if (!get_rep_dir(&dir)) return;

    // convert walking in place to 'hold'
    if (dir == 5)
    {
        do_cmd_hold();
        return;
    }
    
	/* Get location */
	y = p_ptr->py + ddy[dir];
	x = p_ptr->px + ddx[dir];

	/* Verify legality */
	if (!do_cmd_walk_test(y, x)) return;

	/* Take a turn */
	p_ptr->energy_use = 100;

	/* Confuse direction */
	if (confuse_dir(&dir))
	{
		/* Get location */
		y = p_ptr->py + ddy[dir];
		x = p_ptr->px + ddx[dir];
	}

	/* Verify legality */
	if (!do_cmd_walk_test(y, x)) return;

	/* Allow repeated command */
	if (p_ptr->command_arg)
	{
		/* Set repeat count */
		p_ptr->command_rep = p_ptr->command_arg - 1;

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);

		/* Cancel the arg */
		p_ptr->command_arg = 0;
	}

	/* Move the player */
	move_player(dir);
    
}



/*
 * Start running.
 *
 * Note that running while confused is not allowed.
 */
void do_cmd_run(void)
{
	int y, x, dir;


	/* Hack XXX XXX XXX */
	if (p_ptr->confused)
	{
		msg_print("You are too confused!");
		return;
	}


	/* Get a direction (or abort) */
	if (!get_rep_dir(&dir)) return;
	
	// convert into rest
	if (dir == 5)
	{
		do_cmd_rest();
	}

	/* Get location */
	y = p_ptr->py + ddy[dir];
	x = p_ptr->px + ddx[dir];


	/* Verify legality */
	if (!do_cmd_walk_test(y, x)) return;


	/* Start run */
	run_step(dir);
}


/*
 * Hold still
 */
void do_cmd_hold(void)
{
	/* Allow repeated command */
	if (p_ptr->command_arg)
	{
		/* Set repeat count */
		p_ptr->command_rep = p_ptr->command_arg - 1;
		
		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);
		
		/* Cancel the arg */
		p_ptr->command_arg = 0;
	}
	
	/* Take a turn */
	p_ptr->energy_use = 100;
	
	// store the action type
	p_ptr->previous_action[0] = 5;
	
	// store the 'focus' attribute
	p_ptr->focused = TRUE;
	
	// make less noise if you did nothing at all
	// (+7 in total whether or not stealth mode is used)
	if (p_ptr->stealth_mode) stealth_score += 2;
	else					 stealth_score += 7;
	
    // passing in stealth mode removes the speed penalty (as there was no bonus either)
    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_STATE | PR_SPEED);
    
	/* Searching */ 
	search();
}


/*
 * Get items
 */
void do_cmd_pickup(void)
{
	// Usually pickup if there is an object here
	if (cave_o_idx[p_ptr->py][p_ptr->px])
	{
		/* Handle "objects" */
		py_pickup();
	}
	
	else
	{
		msg_print("There is nothing here to get.");
	}
}


/*
 * Rest (restores hit points and mana and such)
 */
void do_cmd_rest(void)
{
	/* Prompt for time if needed */
	if (p_ptr->command_arg == 0)
	{
		p_ptr->command_arg = (-2);
	}

	// typically resting ends your current song
	if (stop_singing_on_rest) change_song(SNG_NOTHING);

	/* Take a turn XXX XXX XXX (?) */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = 5;
	
	// store the 'focus' attribute
	p_ptr->focused = TRUE;

	/* Save the rest code */
	p_ptr->resting = p_ptr->command_arg;

	/* Cancel the arg */
	p_ptr->command_arg = 0;

	/* Cancel stealth mode */
	p_ptr->stealth_mode = FALSE;

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Redraw the state */
	p_ptr->redraw |= (PR_STATE);

	/* Handle stuff */
	handle_stuff();

	/* Refresh */
	Term_fresh();
}






/*
 * Determines the percentage chance of an object breaking when thrown or fired
 *
 * Note that artefacts never break, see the "drop_near()" function.
 */
static int breakage_chance(const object_type *o_ptr, bool hit_wall)
{
	int p = 0;
	
	/* Examine the item type */
	switch (o_ptr->tval)
	{
		/* Always break */
		case TV_FLASK:
		case TV_POTION:
		case TV_USELESS:
		{
			p = 100;
			break;
		}

		/* Often break */
		case TV_LIGHT:
		{
			/* Jewels don't break */
			if ((o_ptr->tval == TV_LIGHT) && ((o_ptr->sval == SV_LIGHT_SILMARIL) || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)))
			{
				p = 0;
			}
			else
			{
				p = 20;
			}
			break;
		}

		/* Sometimes break */
		case TV_ARROW:
		{
			p = 20;
			
			// halved chance if careful
			if (p_ptr->active_ability[S_ARC][ARC_CAREFUL]) p /= 2;
			
			// 100% chance if flaming arrows
			if (p_ptr->active_ability[S_ARC][ARC_FLAMING]) p = 100;
			
			break;
		}
		
		/* Rarely break */
		default:
		{
            if (p_ptr->active_ability[S_MEL][MEL_THROWING]) p = 0;
            else                                            p = 5;
            
			break;
		}
	}

	/* double breakage chance if it hit a wall */
	if (hit_wall)
	{
		p *= 2;
		if (p > 100) p = 100;
	}
	// Unless they hit a wall, items designed for throwing won't break
	else if (k_info[o_ptr->k_idx].flags3 & (TR3_THROWING))
	{
		p = 0;
	}
	
	return (p);
}

/*
 *  Determines if a bow shoots radiant arrows and lights the current grid if so
 */
bool do_radiance(int y, int x, const object_type *j_ptr)
{
	bool radiance = FALSE;
	
	// determine if the bow has 'radiance'
	if (j_ptr->name1 && (a_info[j_ptr->name1].flags2 & (TR2_RADIANCE))) radiance = TRUE;
	if (j_ptr->name2 && (e_info[j_ptr->name2].flags2 & (TR2_RADIANCE))) radiance = TRUE;
	
	// If the bow has 'radiance' and the square is dark, then light it
	if (radiance && !(cave_info[y][x] & (CAVE_GLOW)))
	{
		// Give it light
		cave_info[y][x] |= (CAVE_GLOW);
		
		// Remember the grid
		cave_info[y][x] |= (CAVE_MARK);

		// Fully update the visuals
		p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

		// Update stuff
		update_stuff();
		
		return (TRUE);
	}
	else
	{
		return (FALSE);
	}
}

extern int archery_range(const object_type *j_ptr)
{
	int range;

	range = (j_ptr->dd * total_ads(j_ptr, FALSE) * 3) / 2;
	if (range > MAX_RANGE) range = MAX_RANGE;
	
	return (range);
}


extern int throwing_range(const object_type *i_ptr)
{
	int div;
	int range;
	u32b f1, f2, f3;
	
	object_flags(i_ptr, &f1, &f2, &f3);
	
	/* the divisor is the weight + 2lb */
	div = i_ptr->weight + 20;

	range = (weight_limit()/5) / div;
	
	/* Max distance of MAX_RANGE */
	if (range > MAX_RANGE) range = MAX_RANGE;
	
	/* Min distance of 1 */
	if (range < 1) range = 1;
	
	return (range);
}

/*
 * Give all adjacent, alert, non-mindless opponents (except one whose coordinates are supplied)
 * a free attack on the player.
 */
void attacks_of_opportunity(int neutralized_y, int neutralized_x)
{
    int i;
    int y, x;
    int start;
	int opportunity_attacks = 0;
    
    monster_type *m_ptr;
    monster_race *r_ptr;
	
    start = rand_int(8);
	
    /* Look for adjacent monsters */
    for (i = start; i < 8 + start; i++)
    {
        y = p_ptr->py + ddy_ddd[i % 8];
        x = p_ptr->px + ddx_ddd[i % 8];
        
        /* Check Bounds */
        if (!in_bounds(y, x)) continue;
        
        // 'Point blank archery' avoids attacks of opportunity from the monster shot at
        if (p_ptr->active_ability[S_ARC][ARC_POINT_BLANK] && (neutralized_y == y) && (neutralized_x == x)) continue;
        
        // if it is occupied by a monster
        if (cave_m_idx[y][x] > 0)
        {
            m_ptr = &mon_list[cave_m_idx[y][x]];
            r_ptr = &r_info[m_ptr->r_idx];
            
            // the monster must be alert, not confused, and not mindless
            if ((m_ptr->alertness >= ALERTNESS_ALERT) && !m_ptr->confused && (m_ptr->stance != STANCE_FLEEING) && !(r_ptr->flags2 & (RF2_MINDLESS))
                && !m_ptr->skip_next_turn && !m_ptr->skip_this_turn)
            {
                opportunity_attacks++;
                
                if (opportunity_attacks == 1)
                {
                    msg_print("You provoke attacks of opportunity from adjacent enemies!");
                }
                
                make_attack_normal(m_ptr);
            }
        }
    }

    return;
}

/*
 * Fire an object from the pack or floor.
 *
 * See "calc_bonuses()" for more calculations and such.
 *
 * Note that "firing" a missile is MUCH better than "throwing" it.
 *
 * Note: "unseen" monsters are very hard to hit.
 *
 * Objects are more likely to break if they "attempt" to hit a monster.
 *
 * The "extra shot" code works by decreasing the amount of energy
 * required to make each shot, spreading the shots out over time.
 *
 * Note that when firing missiles, the launcher multiplier is applied
 * after all the bonuses are added in, making multipliers very useful.
 */
void do_cmd_fire(int quiver)
{
	int dir, item;
	int i, y, x, ty, tx;
	int ty2, tx2; //dummy variables needed to pass to the path projection function
    int first_y = 0, first_x = 0;
	int tdis;

	u32b f1, f2, f3; // the bow's flags
	
	int attack_mod = 0, total_attack_mod = 0;
	int total_evasion_mod = 0;
	int hit_result = 0;
	int crit_bonus_dice = 0, slay_bonus_dice = 0;
	int total_dd = 0, total_ds = 0;
	int dam = 0, prt = 0, prt_percent = 100;
	int net_dam = 0;
	
	int shot;
	int shots = 1;
	
	object_type *o_ptr;
	object_type *j_ptr;

	object_type *i_ptr;
	object_type object_type_body;

	monster_type *m_ptr;
	monster_race *r_ptr;
	
	byte missile_attr;
	char missile_char;

	char o_name[80];
	char punctuation[20];

	int path_n;
	u16b path_g[256];

	int msec = op_ptr->delay_factor * op_ptr->delay_factor;

	u32b noticed_arrow_flag = 0L;	// if a slay is noticed on the arrow/bow it is recorded here
	u32b noticed_bow_flag = 0L;		// and the arrow/bow will be identified.

	bool noticed_radiance = FALSE;
	bool fatal_blow = FALSE;
	
	bool pierce = FALSE;
	bool targets_remaining = FALSE;

	/* Get the "bow" (if any) */
	j_ptr = &inventory[INVEN_BOW];

	/* Require a usable launcher */
	if (!j_ptr->tval || !p_ptr->ammo_tval)
	{
		msg_print("You have nothing to fire with.");
		return;
	}

	/* Base range */
	tdis = archery_range(j_ptr);
		
	// bow flags
	object_flags(j_ptr, &f1, &f2, &f3);

	// determine the arrow to fire
	if (quiver == 1)
	{
		o_ptr = &inventory[INVEN_QUIVER1];
		item = INVEN_QUIVER1;
		
		if (!o_ptr->k_idx)
		{
			msg_print("You have no arrows in your 1st quiver.");
			return;
		}
	}
	else 
	{
		o_ptr = &inventory[INVEN_QUIVER2];
		item = INVEN_QUIVER2;
		
		if (!o_ptr->k_idx)
		{
			msg_print("You have no arrows in your 2nd quiver.");
			return;
		}
	}
	
	/* Get a direction (or cancel) */
	if (!get_aim_dir(&dir, tdis)) return;

	/* Start at the player */
	y = p_ptr->py;
	x = p_ptr->px;
	
	/* Predict the "target" location */
	ty = p_ptr->py + 99 * ddy[dir];
	tx = p_ptr->px + 99 * ddx[dir];
	
	/* Check for "target request" */
	if ((dir == 5) && target_okay(tdis))
	{
		ty = p_ptr->target_row;
		tx = p_ptr->target_col;
	}
	
	if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
	{
		ty = p_ptr->py;
		tx = p_ptr->px;
	}

	/* Handle player fear */
	if (p_ptr->afraid)
	{
		/* Message */
		msg_print("You are too afraid to aim properly!");
		
		/* Done */
		return;
	}
		
	/* Get local object */
	i_ptr = &object_type_body;
	
	/* Obtain a local object */
	object_copy(i_ptr, o_ptr);
	
	/* Determine the base attack score */
	attack_mod = (p_ptr->skill_use[S_ARC] + i_ptr->att);
	
	// deal with the rapid fire ability
	if (p_ptr->active_ability[S_ARC][ARC_RAPID_FIRE])
	{
		// if the player has enough arrows, then allow 2 shots
		if (o_ptr->number > 1)
		{
			shots = 2;
		}

		// otherwise remove the rapid fire penalty to attack
		else
		{
			attack_mod += 3;
		}
	}
	
	/* Single object */
	i_ptr->number = 1;
	
	/* Set pickup on fired arrow */
	i_ptr->pickup = TRUE;

	/* Sound */
	sound(MSG_SHOOT);

	/* Describe the object */
	object_desc(o_name, sizeof(o_name), i_ptr, FALSE, 3);

	/* Find the color and symbol for the object for throwing */
	missile_attr = object_attr(i_ptr);
	missile_char = object_char(i_ptr);
	
	// flaming arrows are red
	if (p_ptr->active_ability[S_ARC][ARC_FLAMING]) missile_attr = TERM_RED;

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;

	// set dummy variables to pass to project_path (so it doesn't clobber the real ones)
	ty2 = ty;
	tx2 = tx;

	/* Calculate the path */
	path_n = project_path(path_g, tdis, p_ptr->py, p_ptr->px, &ty2, &tx2, PROJECT_THRU);

	/* Hack -- Handle stuff */
	handle_stuff();

	/* If the bow has 'radiance', then light the starting square */
	noticed_radiance = do_radiance(y, x, j_ptr);

	for (shot = 0; shot < shots; shot++)
	{
		bool hit_wall = FALSE;
		bool ghost_arrow = FALSE;
		int missed_monsters = 0;
		int final_y = GRID_Y(path_g[path_n-1]);
		int final_x = GRID_X(path_g[path_n-1]);
		
		// abort the later shot(s) if there is no target on the trajectory
		if ((shot > 0) && !targets_remaining)	break;
		targets_remaining = FALSE;

		/* Reduce and describe inventory */
		if (item >= 0)
		{
			inven_item_increase(item, -1);
			inven_item_describe(item);
			inven_item_optimize(item);
		}
		
		/* Reduce and describe floor item */
		else
		{
			floor_item_increase(0 - item, -1);
			floor_item_optimize(0 - item);
		}
				
		/* Project along the path */
		for (i = 0; i < path_n; ++i)
		{
			int ny = GRID_Y(path_g[i]);
			int nx = GRID_X(path_g[i]);
						
			/* Hack -- Stop before hitting walls */
			if (!cave_floor_bold(ny, nx)) 
			{
				// if the arrow hasn't already stopped, do some things...
				if (!ghost_arrow)
				{
					hit_wall = TRUE;
					
					// record resting place of arrow
					final_y = y;
					final_x = x;

					// Show collision
					/* Only do visuals if the player can "see" the missile */
					if (panel_contains(ny, nx))
					{
						/* Visual effects */
						print_rel('*', TERM_L_WHITE, ny, nx);
						move_cursor_relative(ny, nx);
						Term_fresh();
						Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
						lite_spot(ny, nx);
						Term_fresh();
					}
					
					/* Delay anyway for consistency */
					else
					{
						/* Pause anyway, for consistancy */
						Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
					}
				}
				
				break;
			}
			
			/* Advance */
			x = nx;
			y = ny;
			
			// after an arrow has stopped, keep looking along the path,
			// but don't attempt to hit creatures, or display graphics or anything
			if (ghost_arrow)
			{
				if (cave_m_idx[y][x] > 0)
                {
                    if (!forgo_attacking_unwary || ((&mon_list[cave_m_idx[y][x]])->alertness >= ALERTNESS_ALERT)) targets_remaining = TRUE;
                }
                    
				continue;
			}
			
			/* If the bow has 'radiance', then light the square being passed over */
			noticed_radiance = do_radiance(y, x, j_ptr);
			
			/* Only do visuals if the player can "see" the missile */
			if (panel_contains(y, x) && player_can_see_bold(y, x))
			{
				/* Visual effects */
				print_rel(missile_char, missile_attr, y, x);
				move_cursor_relative(y, x);
				Term_fresh();
				Term_xtra(TERM_XTRA_DELAY, msec);
				lite_spot(y, x);
				Term_fresh();
			}
			
			/* Delay anyway for consistency */
			else
			{
				/* Pause anyway, for consistancy */
				Term_xtra(TERM_XTRA_DELAY, msec);
			}
						
			/* Handle monster */
			if (cave_m_idx[y][x] > 0)
			{
				m_ptr = &mon_list[cave_m_idx[y][x]];
				r_ptr = &r_info[m_ptr->r_idx];
                
                // record the co-ordinates of the first monster in line of fire
                if (first_y == 0) first_y = y;
                if (first_x == 0) first_x = x;
				
				// Determine the player's attack score after all modifiers
				total_attack_mod = total_player_attack(m_ptr, attack_mod);

				/* Monsters might notice */
				player_attacked = TRUE;
								
				// Modifications for shots that go past the target or strike things before the target...
				if ((dir == 5) && target_okay(tdis))
				{
					// if there is a specific target and this is not it, then massively penalise
					if ((ty != y) || (tx != x))
					{
						total_attack_mod = 0;
					}
				}
				// if it is just a shot in a direction and has already missed something, then massively penalise
				else if (missed_monsters > 0)
				{
					total_attack_mod = 0;
				}
				// if it is a shot in a direction and this is the first monster
				else
				{
					/* Hack -- Track this monster race */
					if (m_ptr->ml) monster_race_track(m_ptr->r_idx);
					
					/* Hack -- Track this monster */
					if (m_ptr->ml) health_track(cave_m_idx[y][x]);
					
					/* Hack -- Target this monster */
					if (m_ptr->ml) target_set_monster(cave_m_idx[y][x]);
				}
				
				// Determine the monster's evasion after all modifiers
				total_evasion_mod = total_monster_evasion(m_ptr, TRUE);
				
				/* Test for hit */
				hit_result = hit_roll(total_attack_mod, total_evasion_mod, PLAYER, m_ptr, TRUE);				
				
				/* If it hit */
				if (hit_result > 0)
				{
					/* Assume a default death */
					cptr note_dies = " dies.";
					
					char m_name[80];
					
					/* Get the monster name (or "it") */
					monster_desc(m_name, sizeof(m_name), m_ptr, 0);
					
					/*Mark the monster as attacked by the player*/
					m_ptr->mflag |= (MFLAG_HIT_BY_RANGED);
					
					if (monster_nonliving(r_ptr))
					{
						/* Special note at death */
						note_dies = " is destroyed.";
					}
					
					// Handle sharpness (which can change 'hit' message)
					prt_percent = prt_after_sharpness(i_ptr, &noticed_arrow_flag);
					if (percent_chance(100 - prt_percent))
					{
						pierce = TRUE;
					}
										
					/* Add 'critical hit' dice based on bow weight */
					crit_bonus_dice = crit_bonus(hit_result, j_ptr->weight, r_ptr, S_ARC, FALSE);
										
					/* Add slay (or brand) dice based on both arrow and bow */
					slay_bonus_dice = slay_bonus(i_ptr, m_ptr, &noticed_arrow_flag);
					slay_bonus_dice += slay_bonus(j_ptr, m_ptr, &noticed_bow_flag);
					
					// bonus for flaming arrows
					if (p_ptr->active_ability[S_ARC][ARC_FLAMING])
					{
						monster_lore *l_ptr = &l_list[m_ptr->r_idx];
						
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
							slay_bonus_dice += 1;
														
							// extra bonus against vulnerable creatures
							if (r_ptr->flags3 & (RF3_HURT_FIRE))
							{
								slay_bonus_dice += 1;
								
								/* Memorize the effects */
								l_ptr->flags3 |= (RF3_HURT_FIRE);

								// cause a temporary morale penalty
								scare_onlooking_friends(m_ptr, -20);
							}
							
						}
					}
					
					/* Calculate the damage done */
					total_dd = j_ptr->dd + crit_bonus_dice + slay_bonus_dice;
					
					// Sil-y: debugging test case
					if (p_ptr->ads <= 0)
					{
						msg_format("BUG: Your damage sides for archery are %d.", p_ptr->ads);
						msg_format("BUG: Recalculating them would give %d.", total_ads(j_ptr, shots == 1));
						msg_format("BUG: j_ptr->ds is %d.", j_ptr->ds);
					}
					
					// Sil-y: does this cause a bug?
					// Note that this is recalculated in case the player has rapid shots but only one arrow
					total_ds = total_ads(j_ptr, shots == 1);
					
					/* Can't have a negative number of sides */
					if (total_ds < 0) total_ds = 0;
					
					dam = damroll(total_dd, total_ds);
					prt = damroll(r_ptr->pd, r_ptr->ps);
					
					prt = (prt * prt_percent) / 100;
					
					net_dam = dam - prt;

					// no negative damage
					if (net_dam < 0) net_dam = 0;
					
					/* Handle unseen monster */
					if (!(m_ptr->ml))
					{
						/* Unseen monster */
						msg_format("The %s finds a mark.", o_name);
					}
					
					/* Handle visible monster */
					else
					{
						char m_name[80];
						
						/* Get "the monster" or "it" */
						monster_desc(m_name, sizeof(m_name), m_ptr, 0);
						
						// determine the punctuation for the attack ("...", ".", "!" etc)
						attack_punctuation(punctuation, net_dam, crit_bonus_dice);
						
						/* Message */
						if (pierce) msg_format("The %s pierces %s%s", o_name, m_name, punctuation);
						else		msg_format("The %s hits %s%s", o_name, m_name, punctuation);
					}

					// if a slay was noticed, then identify the bow/arrow
					if (noticed_arrow_flag || noticed_bow_flag)
					{
						ident_bow_arrow_by_use(j_ptr, i_ptr, o_ptr, m_ptr, noticed_bow_flag, noticed_arrow_flag);
					}
					
					/* No negative damage */
					if (net_dam < 0) net_dam = 0;
					
					update_combat_rolls2(total_dd, total_ds, dam, r_ptr->pd, r_ptr->ps, prt, prt_percent, GF_HURT, FALSE); 
					
					// hit the monster, check for death
					fatal_blow = mon_take_hit(cave_m_idx[y][x], net_dam, note_dies, -1);
					
					display_hit(y, x, net_dam, GF_HURT, fatal_blow);
					
					// if this was the killing shot
					if (fatal_blow)
					{
						// gain wrath if singing song of slaying
						if (singing(SNG_SLAYING))
						{
							add_wrath();
						}
					}
					
					// if it is still alive
					else
					{
						// there is at least one target left on the trajectory
						targets_remaining = TRUE;

						// alert the monster, even if no damage was done
						// (if damage was done, then it was alerted by mon_take_hit() )
						if (net_dam == 0)
						{
							make_alert(m_ptr);
						}
						
						// Morgoth drops his iron crown if he is hit for 10 or more net damage twice
						if ((m_ptr->r_idx == R_IDX_MORGOTH) && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
						{
							if (net_dam >= 10)
							{
								if (p_ptr->morgoth_hits == 0)
								{
									msg_print("The force of your shot knocks the Iron Crown off balance.");
									p_ptr->morgoth_hits++;
								}
								else if (p_ptr->morgoth_hits == 1)
								{
									drop_iron_crown(m_ptr, "You knock his crown from off his brow, and it falls to the ground nearby.");
									p_ptr->morgoth_hits++;
								}
							}
						}

						/* Message */
						message_pain(cave_m_idx[y][x], net_dam);
						
						// Deal with crippling shot ability
						if (p_ptr->active_ability[S_ARC][ARC_CRIPPLING] && (crit_bonus_dice >= 1) && (net_dam > 0) && !(r_ptr->flags1 & (RF1_RES_CRIT)))
						{
							if (skill_check(PLAYER, crit_bonus_dice * 4, monster_skill(m_ptr, S_WIL), m_ptr) > 0)
							{
								msg_format("Your shot cripples %^s!", m_name);
								
								// slow the monster
								// The +1 is needed as a turn of this wears off immediately
								set_monster_slow(cave_m_idx[m_ptr->fy][m_ptr->fx], m_ptr->slowed + crit_bonus_dice + 1, FALSE);
							}							
						}
					}
					
					/* Stop looking if a monster was hit but not pierced */
					if (!pierce)
					{
						// continue checking trajectory, but without affecting things
						ghost_arrow = TRUE;
						
						// record resting place of arrow
						final_y = y;
						final_x = x;
					}
					else
					{
						pierce = FALSE;
					}
				}
				
				// if it misses the monster...
				else
				{
					// there is at least one target left on the trajectory
					targets_remaining = TRUE;
				}
				
				/* we have missed a target, but could still hit something (with a penalty) */
				missed_monsters++;
			}
		}
		
		if (!object_known_p(j_ptr) && noticed_radiance)
		{
			char j_short_name[80];
			char j_full_name[80];
			
			object_desc(j_short_name, sizeof(j_short_name), j_ptr, FALSE, 0);
			object_aware(j_ptr);
			object_known(j_ptr);
			object_desc(j_full_name, sizeof(j_full_name), j_ptr, TRUE, 3);
			
			msg_print("The arrow leaves behind a trail of light!");
			msg_format("You recognize your %s to be %s", j_short_name, j_full_name);
		}
		
		// Break the truce if creatures see
		break_truce(FALSE);
		
		/* Drop (or break) near that location */
		drop_near(i_ptr, breakage_chance(i_ptr, hit_wall), final_y, final_x);
	}


	/* Have to set this here as well, just in case... */
	/* Monsters might notice */
	player_attacked = TRUE;
	
    // provoke attacks of opportunity
    if (p_ptr->active_ability[S_ARC][ARC_POINT_BLANK])  attacks_of_opportunity(first_y, first_x);
    else                                                attacks_of_opportunity(0, 0);
}

/*handle special effects of throwing certain potions*/
static bool thrown_potion_effects(object_type *o_ptr, bool *is_dead, int m_idx)
{
	monster_type *m_ptr = &mon_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];

	int y = m_ptr->fy;
	int x = m_ptr->fx;

	bool ident = FALSE;

	bool un_confuse = FALSE;
	bool un_stun = FALSE;

	bool used_potion = TRUE;

	/* Hold the monster name */
	char m_name[80];
	char m_poss[80];

	/* Get the monster name*/
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	/* Get the monster possessive ("his"/"her"/"its") */
	monster_desc(m_poss, sizeof(m_name), m_ptr, 0x22);

	/* Analyze the potion */
	switch (o_ptr->sval)
	{

		case SV_POTION_SLOWNESS:
		{
			/*slowness explosion at the site, radius 0*/
			ident = explosion(-1, 0, y, x, 0, 0, 10, GF_SLOW);
			break;
		}

		case SV_POTION_CONFUSION:
		{
			/*confusion explosion at the site, radius 0*/
			ident = explosion(-1, 0, y, x, 0, 0, 10, GF_CONFUSION);
			break;
		}

		case SV_POTION_TRUE_SIGHT:
		{
			if ((!m_ptr->ml)&& (r_ptr->flags2 & (RF2_INVISIBLE)))
			{
				/* Mark as visible */
				m_ptr->ml = TRUE;

				/*re-draw the spot*/
				lite_spot(y, x);

				/* Update the monster name*/
				monster_desc(m_name, sizeof(m_name), m_ptr, 0);

				/*monster forgets player history*/
				msg_format("%^s appears for an instant!", m_name);

				/*update the lore*/
				l_ptr->flags2 |= (RF2_INVISIBLE);

				ident = TRUE;
			}

			/* Potion isn't idntified */
			else used_potion = FALSE;

			break;
		}

		case SV_POTION_QUICKNESS:
		{

			/*speed explosion at the site, radius 0*/
			ident = explosion(-1, 0, y, x, 1, 4, -1, GF_SPEED);
			break;
		}

		/*potion just gets thrown as normal object*/
		default:
		{
			used_potion = FALSE;

			break;
		}
	}

	/*monster is now dead, skip messages below*/
	if (cave_m_idx[y][x] == 0)
	{
		un_confuse = FALSE;
		un_stun = FALSE;
		*is_dead = TRUE;
	}

	if (un_confuse)
	{
		if (m_ptr->confused)
		{
			/* No longer confused */
			m_ptr->confused = 0;

			/* Dump a message */
			if (m_ptr->ml)
			{
				msg_format("%^s is no longer confused.", m_name);

				ident = TRUE;
			}
		}
	}

	if (un_stun)
	{
		if (m_ptr->stunned)
		{
			/* No longer confused */
			m_ptr->stunned = 0;

			/* Dump a message */
			if (m_ptr->ml)
			{

				msg_format("%^s is no longer stunned.", m_name);

				ident = TRUE;
			}
		}
	}

	/*inform them of the potion, mark it as known*/
	if ((ident) && (!(k_info[o_ptr->k_idx].aware)))
	{

		char o_name[80];

		/* Identify it fully */
		object_aware(o_ptr);
		object_known(o_ptr);

		/* Description */
		object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

		/* Describe the potion */
		msg_format("You threw %s.", o_name);

		/* Combine / Reorder the pack (later) */
		p_ptr->notice |= (PN_COMBINE | PN_REORDER);

		/* Window stuff */
		p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

	}

	/* Redraw if necessary*/
	if (used_potion) p_ptr->redraw |= (PR_HEALTHBAR);

	/* Handle stuff */
	handle_stuff();

	return (used_potion);

}

/*
 * Throw an object from the pack or floor.
 *
 * Note: "unseen" monsters are very hard to hit.
 *
 * Should throwing a weapon do full damage?  Should it allow the magic
 * to hit bonus of the weapon to have an effect?  Should it ever cause
 * the item to be destroyed?  Should it do any damage at all?
 */
void do_cmd_throw(bool automatic)
{
	int dir, item;
	int i, j, y, x, ty, tx;
	int ty2, tx2; //dummy variables needed to pass to the path projection function
	int tdis;
	u32b f1, f2, f3;

	int attack_mod = 0, total_attack_mod = 0;
	int total_evasion_mod = 0;
	int hit_result = 0;
	int crit_bonus_dice = 0, slay_bonus_dice = 0;
	int total_bonus_dice = 0;
	int total_ds = 0;
	int dam = 0, prt = 0, prt_percent = 100;
	int net_dam = 0;

	object_type *o_ptr;

	object_type *i_ptr;
	object_type object_type_body;

	bool hit_body = FALSE;
	bool hit_wall = FALSE;

	int missed_monsters = 0;
	
	bool spatial_target = FALSE;

	byte missile_attr;
	char missile_char;

	char o_name[80];
	char punctuation[20];

	int path_n;
	u16b path_g[256];

	cptr q, s;

	int msec = op_ptr->delay_factor * op_ptr->delay_factor;

	u32b noticed_flag = 0;	// if a slay is noticed it is recorded here and the item identified

    if (automatic)
    {
        bool found = FALSE;
     
        /* Scan the inventory */
        for (i = 0; i < INVEN_PACK; i++)
        {
            o_ptr = &inventory[i];
            
            /* Skip non-objects */
            if (!o_ptr->k_idx) continue;
            
            /* Extract the item flags */
            object_flags(o_ptr, &f1, &f2, &f3);
            
            if (f3 & (TR3_THROWING))
            {
                item = i;
                found = TRUE;
                break;
            }
        }
        
        if (!found)
        {
            msg_print("You don't have anything designed for throwing in your inventory.");
            return;
        }
    }
    
    else
    {
        /* Get an item */
        q = "Throw which item? ";
        s = "You have nothing to throw.";
        if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR | USE_EQUIP))) return;
    }

	/* Get the object */
	if (item >= 0)
	{
		o_ptr = &inventory[item];
	}
	else
	{
		o_ptr = &o_list[0 - item];
	}

	/* Hack -- Cannot remove cursed items */
	if ((item >= INVEN_WIELD) && cursed_p(o_ptr))
	{
		if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
		{
			/* Message */
			msg_print("With a great strength of will, you break the curse!");
			
			/* Uncurse the object */
			uncurse_object(o_ptr);						
		}
		else
		{
			/* Oops */
			msg_print("You cannot bear to part with it.");
			
			/* Nope */
			return;
		}
	}
	
	// Determine throwing range
	tdis = throwing_range(o_ptr);
	
	/* Examine the item */
	object_flags(o_ptr, &f1, &f2, &f3);

    // Aim automatically if asked
    if (automatic)
    {
        if (target_okay(tdis)) dir = 5;
        
        else
        {
            /* Prepare the "temp" array */
            target_set_interactive_prepare(TARGET_KILL, tdis);
            
            /* Monster */
            if (temp_n)
            {
                target_set_monster(cave_m_idx[temp_y[0]][temp_x[0]]);
                health_track(cave_m_idx[temp_y[0]][temp_x[0]]);
                dir = 5;
            }
            
            else
            {
                msg_print("No clear target for automatic throwing.");
                return;
            }
        }
        
        if (p_ptr->confused)
        {
            dir = ddd[rand_int(8)];
        }
    }
    
	// Otherwise get a direction (or cancel) */
	else if (!get_aim_dir(&dir, tdis)) return;

	/* Take off equipment first */
	if (item >= INVEN_WIELD)
	{
		/* Take off first */
		item = inven_takeoff(item, 1);
		
		/* Get the original object */
		o_ptr = &inventory[item];
	}	

	/* Start at the player */
	y = p_ptr->py;
	x = p_ptr->px;
	
	/* Predict the "target" location */
	ty = p_ptr->py + 99 * ddy[dir];
	tx = p_ptr->px + 99 * ddx[dir];
	
	/* Check for "target request" */
	if ((dir == 5) && target_okay(tdis))
	{
		ty = p_ptr->target_row;
		tx = p_ptr->target_col;
		
		// targetting the square itself
		if (cave_m_idx[y][x] == 0)
		{
			spatial_target = TRUE;
		}
	}
	
	if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
	{
		ty = p_ptr->py;
		tx = p_ptr->px;
	}

	/* Handle player fear */
	if (p_ptr->afraid)
	{
		/* Message */
		msg_print("You are too afraid to aim properly!");
		
		/* Done */
		return;
	}
		
	/* Get local object */
	i_ptr = &object_type_body;

	/* Obtain a local object */
	object_copy(i_ptr, o_ptr);

	/* Single object */
	i_ptr->number = 1;

	/* Set pickup on thrown item */
	i_ptr->pickup = TRUE;

	/* Reduce and describe inventory */
	if (item >= 0)
	{
		inven_item_increase(item, -1);
		inven_item_describe(item);
		inven_item_optimize(item);
	}

	/* Reduce and describe floor item */
	else
	{
		floor_item_increase(0 - item, -1);
		floor_item_optimize(0 - item);
	}


	/* Description */
	object_desc(o_name, sizeof(o_name), i_ptr, FALSE, 3);

	/* Find the color and symbol for the object for throwing */
	missile_attr = object_attr(i_ptr);
	missile_char = object_char(i_ptr);

	attack_mod = p_ptr->skill_use[S_MEL] + i_ptr->att;
	
	// subtract out the melee weapon's bonus (as we had already accounted for it)
	attack_mod -= (&inventory[INVEN_WIELD])->att;
	attack_mod -= blade_bonus(&inventory[INVEN_WIELD]);
	attack_mod -= axe_bonus(&inventory[INVEN_WIELD]);
	attack_mod -= polearm_bonus(&inventory[INVEN_WIELD]);

	/* Weapons that are not good for throwing are much less accurate */
	if (!(f3 & (TR3_THROWING)))
	{
		attack_mod -= 5;
	}

	// give people their weapon affinity bonuses if the weapon is thrown
	attack_mod += blade_bonus(i_ptr);
	attack_mod += axe_bonus(i_ptr);
	attack_mod += polearm_bonus(i_ptr);
	
	// bonus for throwing proficiency ability
	if (p_ptr->active_ability[S_MEL][MEL_THROWING]) attack_mod += 5;

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;

	// set dummy variables to pass to project_path (so it doesn't clobber the real ones)
	ty2 = ty;
	tx2 = tx;

	if (spatial_target)
	{
		/* Calculate the path */
		path_n = project_path(path_g, tdis, p_ptr->py, p_ptr->px, &ty2, &tx2, PROJECT_THRU);
	}
	else
	{	
		/* Calculate the path */
		path_n = project_path(path_g, tdis, p_ptr->py, p_ptr->px, &ty2, &tx2, 0);
	}
	
	/* Hack -- Handle stuff */
	handle_stuff();

	/* Project along the path */
	for (i = 0; i < path_n; ++i)
	{
		int ny = GRID_Y(path_g[i]);
		int nx = GRID_X(path_g[i]);

		/* Hack -- Stop before hitting walls */
		if (!cave_floor_bold(ny, nx)) 
		{
			hit_wall = TRUE;
			
			// Show collision
			/* Only do visuals if the player can "see" the missile */
			if (panel_contains(ny, nx))
			{
				/* Visual effects */
				print_rel('*', TERM_L_WHITE, ny, nx);
				move_cursor_relative(ny, nx);
				Term_fresh();
				Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
				lite_spot(ny, nx);
				Term_fresh();
			}
			
			/* Delay anyway for consistency */
			else
			{
				/* Pause anyway, for consistancy */
				Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
			}
			break;
		}
		
		/* Advance */
		x = nx;
		y = ny;

		/* Only do visuals if the player can "see" the missile */
		if (panel_contains(y, x) && player_can_see_bold(y, x))
		{
			/* Visual effects */
			print_rel(missile_char, missile_attr, y, x);
			move_cursor_relative(y, x);
			Term_fresh();
			Term_xtra(TERM_XTRA_DELAY, msec);
			lite_spot(y, x);
			Term_fresh();
		}

		/* Delay anyway for consistency */
		else
		{
			/* Pause anyway, for consistancy */
			Term_xtra(TERM_XTRA_DELAY, msec);
		}

		/* Handle monster */
		if (cave_m_idx[y][x] > 0)
		{
			monster_type *m_ptr = &mon_list[cave_m_idx[y][x]];
			monster_race *r_ptr = &r_info[m_ptr->r_idx];
						
			bool potion_effect = FALSE;
			int pdam = 0;
			bool fatal_blow = FALSE;

			// Determine the player's attack score after all modifiers
			total_attack_mod = total_player_attack(m_ptr, attack_mod);

			/* Monsters might notice */
			player_attacked = TRUE;
						
			// Modifications for shots that go past the target or strike things before the target...
			if ((dir == 5) && target_okay(tdis))
			{
				// if there is a specific target and this is not it, then massively penalise
				if ((ty != y) || (tx != x))
				{
					total_attack_mod = 0;
				}
			}
			// if it is just a shot in a direction and has already missed something, then massively penalise
			else if (missed_monsters > 0)
			{
				total_attack_mod = 0;
			}
			// if it is a shot in a direction and this is the first monster
			else
			{
				/* Hack -- Track this monster race */
				if (m_ptr->ml) monster_race_track(m_ptr->r_idx);
				
				/* Hack -- Track this monster */
				if (m_ptr->ml) health_track(cave_m_idx[y][x]);
				
				/* Hack -- Target this monster */
				if (m_ptr->ml) target_set_monster(cave_m_idx[y][x]);
			}
			
			// Determine the monster's evasion after all modifiers
			total_evasion_mod = total_monster_evasion(m_ptr, FALSE);
			
			/* Test for hit */
			hit_result = hit_roll(total_attack_mod, total_evasion_mod, PLAYER, m_ptr, TRUE);

			/* If it hit... */
			if (hit_result > 0) 
			{
				/* Note the collision */
				hit_body = TRUE;
				
				/* Assume a default death */
				cptr note_dies = " dies.";

				/*Mark the monster as attacked by the player*/
				m_ptr->mflag |= (MFLAG_HIT_BY_RANGED);

				/* Some monsters get "destroyed" */
				if (monster_nonliving(r_ptr))
				{
					/* Special note at death */
					note_dies = " is destroyed.";
				}

				/* Apply special damage XXX XXX XXX */
				crit_bonus_dice = crit_bonus(hit_result, i_ptr->weight, r_ptr, S_MEL, TRUE);
				slay_bonus_dice = slay_bonus(i_ptr, m_ptr, &noticed_flag);

				/* Calculate the damage from the thrown object */
				total_bonus_dice = crit_bonus_dice + slay_bonus_dice;
				total_ds = strength_modified_ds(i_ptr, 0);

				/* Penalise items that aren't made to be thrown */
				if (!(f3 & (TR3_THROWING)))	total_ds /= 2;
			
				/* Can't have a negative number of sides */
				if (total_ds < 0) total_ds = 0;
				
				dam = damroll(i_ptr->dd + total_bonus_dice, total_ds);
				prt = damroll(r_ptr->pd, r_ptr->ps);

				prt_percent = prt_after_sharpness(i_ptr, &noticed_flag);
				prt = (prt * prt_percent) / 100;

				net_dam = dam - prt;

				// no negative damage
				if (net_dam < 0) net_dam = 0;

				/* Handle unseen monster */
				if (!(m_ptr->ml))
				{
					/* Invisible monster */
					msg_format("The %s finds a mark.", o_name);
				}
				
				/* Handle visible monster */
				else
				{
					char m_name[80];
					
					/* Get "the monster" or "it" */
					monster_desc(m_name, sizeof(m_name), m_ptr, 0);
					
					// determine the punctuation for the attack ("...", ".", "!" etc)
					attack_punctuation(punctuation, net_dam, crit_bonus_dice);
					
					/* Message */
					msg_format("The %s hits %s%s", o_name, m_name, punctuation);
					
					/* Hack -- Track this monster race */
					if (m_ptr->ml) monster_race_track(m_ptr->r_idx);
					
					/* Hack -- Track this monster */
					if (m_ptr->ml) health_track(cave_m_idx[y][x]);
					
					/* Hack -- Target this monster */
					if (m_ptr->ml) target_set_monster(cave_m_idx[y][x]);
				}
				
				/*special effects sometimes reveal the kind of potion*/
				if (i_ptr->tval == TV_POTION)
				{
					/*record monster hit points*/
					pdam = m_ptr->hp;
					
					msg_print("The bottle breaks.");
					
					/*returns true if the damage has already been handled*/
					potion_effect = (thrown_potion_effects(i_ptr, &fatal_blow, cave_m_idx[y][x]));
					
					/*check the change in monster hp*/
					pdam -= m_ptr->hp;
					
					/*monster could have been healed*/
					if (pdam < 0) pdam = 0;
				}

				// if a slay was noticed, then identify the weapon
				if (noticed_flag)
				{
					ident_weapon_by_use(i_ptr, m_ptr, noticed_flag);
				}

				/* No negative net damage */
				if (net_dam < 0) net_dam = 0;

				update_combat_rolls2(i_ptr->dd + total_bonus_dice, total_ds, dam, r_ptr->pd, r_ptr->ps, prt, prt_percent, GF_HURT, FALSE); 

				/* Hit the monster, unless a potion effect has already been done */
				if (!potion_effect)
				{
					 fatal_blow = (mon_take_hit(cave_m_idx[y][x], net_dam, note_dies, -1));
					 
					// gain wrath if singing song of slaying
					if (fatal_blow && singing(SNG_SLAYING))
					{
						add_wrath();
					}
				}

				display_hit(y, x, net_dam, GF_HURT, fatal_blow);

				/* Still alive */
				if (!fatal_blow)
				{
					// alert the monster, even if no damage was done
					// (if damage was done, then it was alerted by mon_take_hit() )
					if (net_dam == 0)
					{
						make_alert(m_ptr);
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
					
					/* Message if applicable*/
					if ((!potion_effect) || (pdam > 0))
						message_pain(cave_m_idx[y][x],  (pdam ? pdam : net_dam));
				}
				/* Stop looking if a monster was hit */
				break;
			}
			else
			{
				/* we have missed a target, but could still hit something (with a penalty) */
				missed_monsters++;
			}

		}		
	}

	// need to print this message even if the potion missed
	if (!hit_body && (i_ptr->tval == TV_POTION)) msg_print("The bottle breaks.");
	
	/* Have to set this here as well, just in case... */
	/* Monsters might notice */
	player_attacked = TRUE;
	
	/* Chance of breakage (during attacks) */
	j = breakage_chance(i_ptr, hit_wall);

	/* throwing weapons have a lesser chance */
	if (f3 & (TR3_THROWING)) j /= 4;

	/* Drop (or break) near that location */
	drop_near(i_ptr, j, y, x);

	// Break the truce if creatures see
	break_truce(FALSE);
}

