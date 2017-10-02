/* File: dungeon.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


/*
 * Return a "feeling" (or NULL) about an item.  Method 1 (Weak).
 * Sil - this method can't distinguish artefacts from ego items
 */
int value_check_aux1(const object_type *o_ptr)
{
	/* Artefacts */
	if (artefact_p(o_ptr))
	{
		/* Normal */
		return (INSCRIP_EXCELLENT);
	}

	/* Ego-Items */
	if (ego_item_p(o_ptr))
	{
		/* Normal */
		return (INSCRIP_EXCELLENT);
	}

	/* Cursed items */
	//if (cursed_p(o_ptr)) return (INSCRIP_CURSED);

	/* Broken items */
	//if (broken_p(o_ptr)) return (INSCRIP_BROKEN);

	/* Good evasion or protection dice/sides */
	//if (o_ptr->evn > k_info[o_ptr->k_idx].evn) return (INSCRIP_GOOD_STRONG);
	//if (o_ptr->pd > k_info[o_ptr->k_idx].pd) return (INSCRIP_GOOD_STRONG);
	//if (o_ptr->ps > k_info[o_ptr->k_idx].ps) return (INSCRIP_GOOD_STRONG);

	/* Good attack or damage dice/sides*/
	//if (o_ptr->att > k_info[o_ptr->k_idx].att) return (INSCRIP_GOOD_STRONG);
	//if (o_ptr->dd > k_info[o_ptr->k_idx].dd) return (INSCRIP_GOOD_STRONG);
	//if (o_ptr->ds > k_info[o_ptr->k_idx].ds) return (INSCRIP_GOOD_STRONG);

	/* Default to "average" */
	return (INSCRIP_AVERAGE);
}


/*
 * Return a "feeling" (or NULL) about an item.  Method 2 (Strong).
 * Sil - this method can distinguish artefacts from special items
 */
int value_check_aux2(const object_type *o_ptr)
{
	/* Artefacts */
	if (artefact_p(o_ptr))
	{
		/* Cursed */
		if (cursed_p(o_ptr)) return (INSCRIP_TERRIBLE);

		/* Normal */
		return (INSCRIP_SPECIAL);
	}

	/* Ego-Items */
	if (ego_item_p(o_ptr))
	{
		/* Cursed */
		if (cursed_p(o_ptr)) return (INSCRIP_WORTHLESS);

		/* Normal */
		return (INSCRIP_EXCELLENT);
	}

	/* Cursed items */
	//if (cursed_p(o_ptr)) return (INSCRIP_CURSED);

	/* Broken items */
	//if (broken_p(o_ptr)) return (INSCRIP_BROKEN);

	/* Good evasion or protection dice/sides */
	//if (o_ptr->evn > k_info[o_ptr->k_idx].evn) return (INSCRIP_GOOD_STRONG);
	//if (o_ptr->pd > k_info[o_ptr->k_idx].pd) return (INSCRIP_GOOD_STRONG);
	//if (o_ptr->ps > k_info[o_ptr->k_idx].ps) return (INSCRIP_GOOD_STRONG);

	/* Good attack or damage dice/sides*/
	//if (o_ptr->att > k_info[o_ptr->k_idx].att) return (INSCRIP_GOOD_STRONG);
	//if (o_ptr->dd > k_info[o_ptr->k_idx].dd) return (INSCRIP_GOOD_STRONG);
	//if (o_ptr->ds > k_info[o_ptr->k_idx].ds) return (INSCRIP_GOOD_STRONG);

	/* Default to "average" */
	return (INSCRIP_AVERAGE);
}


/*
 * Returns TRUE if this object can be pseudo-ided.
 */
bool can_be_pseudo_ided(const object_type *o_ptr)
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
      	{
			return (TRUE);
			break;
      	}
    	case TV_LIGHT:
      	{
			if (o_ptr->sval == SV_LIGHT_LANTERN) return (TRUE);
			if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL) return (TRUE);
			if (o_ptr->sval == SV_LIGHT_FEANORIAN) return (TRUE);
			break;
      	}
  	}
  	return (FALSE);
}


/*
 * Pseudo-id an item
 */
void pseudo_id(object_type *o_ptr)
{
	int feel;

	char o_name[80];

	/* Skip non-sense machines */
	if (!can_be_pseudo_ided(o_ptr)) return;

	/* It is known, no information needed */
	if (object_known_p(o_ptr)) return;

	/* Check for a feeling */
	if (p_ptr->active_ability[S_PER][PER_LORE1])
	{
		feel = value_check_aux2(o_ptr);
	}
	else
	{
		feel = value_check_aux1(o_ptr);
	}

	/* Skip non-feelings */
	if (!feel) return;

	/* Get an object description */
	object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);

	/* Sense the object */
	o_ptr->discount = feel;

	/* The object has been "sensed" */
	o_ptr->ident |= (IDENT_SENSE);


}



void pseudo_id_everything(void)
{
	int i;
	object_type *o_ptr;
	
	for (i = 1; i < o_max; i++)
	{
		/* Get the next object from the dungeon */
		o_ptr = &o_list[i];
		
		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;
		
		/* Ignore known objects */
		if (object_known_p(o_ptr)) continue;
		
		/* Pseudo-id it */
		pseudo_id(o_ptr);
	}
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		/* Get the object */
		o_ptr = &inventory[i];
		
		/* Ignore empty objects */
		if (!o_ptr->k_idx) continue;
		
		/* Ignore known objects */
		if (object_known_p(o_ptr)) continue;
		
		/* Pseudo-id it */
		pseudo_id(o_ptr);
	}
	
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
	handle_stuff();
}

void id_everything(void)
{
	int i;
	object_type *o_ptr;
	
	for (i = 1; i < o_max; i++)
	{
		/* Get the next object from the dungeon */
		o_ptr = &o_list[i];
		
		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;
		
		/* Ignore known objects */
		if (object_known_p(o_ptr)) continue;
		
		/* Identify it */
		ident(o_ptr);
	}
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		/* Get the object */
		o_ptr = &inventory[i];
		
		/* Ignore empty objects */
		if (!o_ptr->k_idx) continue;
		
		/* Ignore known objects */
		if (object_known_p(o_ptr)) continue;
		
		/* Identify it */
		ident(o_ptr);
	}
	
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
	handle_stuff();
}


/*
 * automatically identify items of {special} types that the player knows about
 */
void id_known_specials(void)
{
	int i;
	object_type *o_ptr;
	
	for (i = 1; i < o_max; i++)
	{
		/* Get the next object from the dungeon */
		o_ptr = &o_list[i];
		
		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;
		
		/* Automatically identify any special items you have seen before*/
		if (o_ptr->name2 && !object_known_p(o_ptr) && (e_info[o_ptr->name2].aware))
		{
			ident(o_ptr);
		}
	}
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		/* Get the object */
		o_ptr = &inventory[i];
		
		/* Ignore empty objects */
		if (!o_ptr->k_idx) continue;
		
		/* Automatically identify any special items you have seen before*/
		if (o_ptr->name2 && !object_known_p(o_ptr) && (e_info[o_ptr->name2].aware))
		{
			ident(o_ptr);
		}
	}
	
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
	handle_stuff();
}


/*
 *  Determines how many points of health/song is regenerated next round
 *  assuming it increases by 'max' points every 'regen_period'.
 *  Note that players use 'playerturn' and monsters use 'turn'.
 *  This lets hasted players regenerate at the right speed.
 */

int regen_amount(int turn_number, int max, int regen_period)
{
	int regen_so_far, regen_next;
	
	if (turn_number == 0)
	{
		/* do nothing on the first turn of the game */
		return (0);
	}
	if ((turn_number % regen_period) > 0)
	{
		regen_so_far = (max * ((turn_number - 1) % regen_period)) / regen_period;
		regen_next =   (max * ((turn_number    ) % regen_period)) / regen_period;
	}
	else
	{
		regen_so_far = (max * ((turn_number - 1) % regen_period)) / regen_period;
		regen_next =   (max * (                    regen_period)) / regen_period;
	}
	
	return (regen_next - regen_so_far);
}


/*
 * Regenerate hit points
 */
static void regenhp(int regen_multiplier)
{
	int old_chp;

	// exit immediately if the multiplier is zero (avoids div by zero error)
	if (regen_multiplier == 0) return;
	
	/* Save the old hitpoints */
	old_chp = p_ptr->chp;

	/* Work out how much increase is due */
	/* where the player should get completely healed every PY_REGEN_HP_PERIOD player turns */
	
	p_ptr->chp += regen_amount(playerturn, p_ptr->mhp, PY_REGEN_HP_PERIOD / regen_multiplier);

	/* Fully healed */
	if (p_ptr->chp >= p_ptr->mhp)
	{
		p_ptr->chp = p_ptr->mhp;
	}

	/* Notice changes */
	if (old_chp != p_ptr->chp)
	{
		/* Redraw */
		p_ptr->redraw |= (PR_HP);

		/* Window stuff */
		p_ptr->window |= (PW_PLAYER_0);
	}
}


/*
 * Regenerate mana points
 */
static void regenmana(int regen_multiplier)
{
	int old_csp;

	// exit immediately if the multiplier is zero (avoids div by zero error)
	if (regen_multiplier == 0) return;
	
	// don't regenerate voice if singing
	if (!singing(SNG_NOTHING)) return;
	
	/* Save the old hitpoints */
	old_csp = p_ptr->csp;

	/* Work out how much increase is due */
	/* where the player should get completely recovered every PY_REGEN_SP_PERIOD player turns */
	
	p_ptr->csp += regen_amount(playerturn, p_ptr->msp, PY_REGEN_SP_PERIOD / regen_multiplier);

	/* Fully recovered */
	if (p_ptr->csp >= p_ptr->msp)
	{
		p_ptr->csp = p_ptr->msp;
	}

	/* Redraw mana */
	if (old_csp != p_ptr->csp)
	{
		/* Redraw */
		p_ptr->redraw |= (PR_VOICE);

		/* Window stuff */
		p_ptr->window |= (PW_PLAYER_0);
	}
}


/*
 * Regenerate the monsters (once per 100 game turns)
 */

static void regen_monsters(void)
{
	int i;
	int regen_period;

	/* Regenerate everyone */
	for (i = 1; i < mon_max; i++)
	{
		/* Check the i'th monster */
		monster_type *m_ptr = &mon_list[i];
		monster_race *r_ptr = &r_info[m_ptr->r_idx];

		/* Skip dead monsters */
		if (!m_ptr->r_idx) continue;

		/* Allow hp regeneration, if needed. */
		if (m_ptr->hp != m_ptr->maxhp)
		{
			/* Some monsters regenerate quickly */
			if (r_ptr->flags2 & (RF2_REGENERATE))
			{
				regen_period = MON_REGEN_HP_PERIOD / 5;
			}
			else
			{
				regen_period = MON_REGEN_HP_PERIOD;
			}
			
			m_ptr->hp += regen_amount(turn / 10, m_ptr->maxhp, regen_period);

			/* Do not over-regenerate */
			if (m_ptr->hp > m_ptr->maxhp) m_ptr->hp = m_ptr->maxhp;
			
			/* Fully healed -> flag minimum range for recalculation */
			if (m_ptr->hp == m_ptr->maxhp) m_ptr->min_range = 0;
		}

		/* Allow mana regeneration, if needed. */
		if (m_ptr->mana != MON_MANA_MAX)
		{
            // can only regenerate mana if not singing
            if (m_ptr->song == SNG_NOTHING)
            {
                m_ptr->mana += regen_amount(turn / 10, MON_MANA_MAX, MON_REGEN_SP_PERIOD);
                
                /* Do not over-regenerate */
                if (m_ptr->mana > MON_MANA_MAX) m_ptr->mana = MON_MANA_MAX;
                
                /* Fully healed -> flag minimum range for recalculation */
                if (m_ptr->mana == MON_MANA_MAX) m_ptr->min_range = 0;
            }
		}
	}
}



/*
 * If player has inscribed the object with "!!", let him know when it's
 * recharged. -LM-
 */
static void recharged_notice(object_type *o_ptr)
{
	char o_name[120];

	cptr s;

	/* No inscription */
	if (!o_ptr->obj_note) return;

	/* Find a '!' */
	s = strchr(quark_str(o_ptr->obj_note), '!');

	/* Process notification request. */
	while (s)
	{
		/* Find another '!' */
		if (s[1] == '!')
		{
			/* Describe (briefly) */
			object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);

			/*Disturb the player*/
			disturb(0, 0);

			/* Notify the player */
			if (o_ptr->number > 1)
				msg_format("Your %s are all recharged.", o_name);

			/*artefacts*/
			else if (o_ptr->name1)
			{
				msg_format("The %s has recharged.", o_name);
			}

			/*single, non-artefact items*/
			else msg_format("Your %s has recharged.", o_name);

			/* Done. */
			return;
		}

		/* Keep looking for '!'s */
		s = strchr(s + 1, '!');
	}
}



/*
 * Handle certain things once every 10 game turns
 */
static void process_world(void)
{
	int i, j;

	object_type *o_ptr;

	bool was_ghost = FALSE;

	/* Stop now unless the turn count is divisible by 10 */
	if (turn % 10) return;

	/*** Check the Time and Load ***/
	if (!(turn % 1000))
	{
		/* Check time and load */
		if (0 != check_time())
		{
			/* Warning */
			if (closing_flag <= 2)
			{
				/* Disturb */
				disturb(0, 0);

				/* Count warnings */
				closing_flag++;

				/* Message */
				msg_print("The gates to ANGBAND are closing...");
				msg_print("Please finish up and/or save your game.");
			}

			/* Slam the gate */
			else
			{
				/* Message */
				msg_print("The gates to ANGBAND are now closed.");

				/* Stop playing */
				p_ptr->playing = FALSE;

				/* Leaving */
				p_ptr->leaving = TRUE;
			}
		}
	}


	/*** Handle the "surface" ***/

	/* While on the surface */
	if (p_ptr->depth == 0)
	{
		if (percent_chance(10))
		{
			/* Make a new monster */
			(void)alloc_monster(TRUE, FALSE);
		}
	}
	

	/*** Process the monsters ***/

	/* Hack - see if there is already a player ghost on the level */
	if (bones_selector) was_ghost = TRUE;

	/* Vastly more wandering monsters during the endgame when you have 2 or 3 Silmarils */
	if (silmarils_possessed() >= 2)
	{
		int percent = (p_ptr->cur_map_hgt * p_ptr->cur_map_wid) / (PANEL_HGT * PANEL_WID_FIXED);
		
		if (percent_chance(percent))
		{
			/* Make a new monster */
			(void)alloc_monster(TRUE, FALSE);
		}
	}
	
	/* Check for normal wandering monster generation */
	else if (one_in_(MAX_M_ALLOC_CHANCE))
	{
		/* Make a new monster */
		(void)alloc_monster(TRUE, FALSE);
	}
	
	// Players with the haunted curse attract wraiths
	if (percent_chance(p_ptr->haunted))
	{
		/* Make a new wraith */
		(void)alloc_monster(TRUE, TRUE);
	}

	/* Hack - if there is a ghost now, and there was not before,
	 * give a challenge */
	if ((bones_selector) && (!(was_ghost))) ghost_challenge();

	/* Regenerate creatures */
	regen_monsters();


	/*** Process Light ***/

	/* Check for light being wielded */
	o_ptr = &inventory[INVEN_LITE];

	/* Burn some fuel in the current lite */
	if (o_ptr->tval == TV_LIGHT)
	{
		/* Hack -- Use some fuel */
		if (o_ptr->timeout > 0)
		{
			/* Decrease life-span */
			o_ptr->timeout--;

			/* Hack -- notice interesting fuel steps */
			if ((o_ptr->timeout < 100) || (!(o_ptr->timeout % 100)))
			{
				/* Window stuff */
				p_ptr->window |= (PW_EQUIP);
			}

			/* Hack -- Special treatment when blind */
			if (p_ptr->blind)
			{
				/* Hack -- save some light for later */
				if (o_ptr->timeout == 0) o_ptr->timeout++;
			}

			/* The light is now out */
			else if (o_ptr->timeout == 0)
			{
				disturb(0, 0);
				msg_print("Your light has gone out!");
			}

			/* The light is getting dim */
			else if ((o_ptr->timeout <= 100) && (!(o_ptr->timeout % 20)))
			{
				// disturb the first time
				if (o_ptr->timeout == 100)	disturb(0, 0);
				
				msg_print("Your light is growing faint.");
			}
		}
	}

	/*** Process Inventory ***/

	/* Process equipment */
	for (j = 0, i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		/* Get the object */
		o_ptr = &inventory[i];

		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;

		/* Recharge activatable objects */
		if (o_ptr->timeout > 0 && !fuelable_light_p(o_ptr))
		{
			/* Recharge */
			o_ptr->timeout--;

			/* Notice changes */
			if (!(o_ptr->timeout))
			{
				/* Update window */
		    	j++;

				/* Message if item is recharged, if inscribed !! */
				if (!(o_ptr->timeout)) recharged_notice(o_ptr);
			}
		}

	}

	/* Notice changes */
	if (j)
	{
		/* Window stuff */
		p_ptr->window |= (PW_EQUIP);
	}


	/* Notice changes */
	if (j)
	{
		/* Combine pack */
		p_ptr->notice |= (PN_COMBINE);

		/* Window stuff */
		p_ptr->window |= (PW_INVEN);
	}

	/*** Process Objects ***/

	/* Process objects */
	for (i = 1; i < o_max; i++)
	{
		/* Get the object */
		o_ptr = &o_list[i];

		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;
	}

}

/*
 * Verify use of "wizard" mode
 */
static bool enter_wizard_mode(void)
{
	/* Ask first time - unless resurrecting a dead character */
	if (!(p_ptr->noscore & 0x0008) && !p_ptr->is_dead)
	{
		/* Explanation */
		msg_print("You can only enter wizard mode from within debug mode.");
		
		return (FALSE);
	}

	/* Mark savefile */
	p_ptr->noscore |= 0x0002;

	/* Success */
	return (TRUE);
}



#ifdef ALLOW_DEBUG

/*
 * Verify use of "debug" mode
 */
static bool verify_debug_mode(void)
{
	char buf[80] = "It is not mellon";
	
	/* Ask first time */
	if (!(p_ptr->noscore & 0x0008))
	{
		/* Mention effects */
		msg_print("You are about to use the dangerous, unsupported, debug commands!");
		msg_print("Your machine may crash, and your savefile may become corrupted!");
		message_flush();

		/* Verify request */
		if (!get_check("Are you sure you want to use the debug commands? "))
		{
			return (FALSE);
		}
		
		// ask for password in deployment versions
		if (DEPLOYMENT)
		{
			if (term_get_string("Password: ", buf, sizeof(buf)))
			{
				if (strcmp(buf,"Gondolin") == 0)
				{
					/* Mark savefile */
					p_ptr->noscore |= 0x0008;
					
					/* Okay */
					return (TRUE);
				}
			}
			
			msg_print("Incorrect password.");
			return (FALSE);
		}
	}

	/* Mark savefile */
	p_ptr->noscore |= 0x0008;

	/* Okay */
	return (TRUE);
}

#endif /* ALLOW_DEBUG */



/*
 * Verify use of "automaton" mode
 */
static bool verify_automaton_mode(void)
{
	/* Ask first time */
	if (!(p_ptr->noscore & 0x0010))
	{
		/* Mention effects */
		msg_print("You are about to turn on the automaton.");
		msg_print("This is highly experimental and will probably kill your character.");
		message_flush();
        
		/* Verify request */
		if (!get_check("Are you sure you want to use the automaton? "))
		{
			return (FALSE);
		}
	}
    
    /* Mark savefile */
    p_ptr->noscore |= 0x0010;
    
	/* Okay */
	return (TRUE);
}



/*
 * Parse and execute the current command
 * Give "Warning" on illegal commands.
 */
static void process_command(void)
{

#ifdef ALLOW_REPEAT

	/* Handle repeating the last command */
	repeat_check();

#endif /* ALLOW_REPEAT */

    
	/* Parse the command */
	switch (p_ptr->command_cmd)
	{
		/* Ignore */
		case ' ':
		case '\n':
        case '\r':
		case '\a':
		{
			break;
		}


		/*** Cheating Commands ***/

		/* Toggle Wizard Mode */
		case KTRL('W'):
		{
			if (p_ptr->wizard)
			{
				p_ptr->wizard = FALSE;
				msg_print("Wizard mode off.");
				p_ptr->update |= (PU_BONUS);
			}
			else if (enter_wizard_mode())
			{
				p_ptr->wizard = TRUE;
				msg_print("Wizard mode on.");
				p_ptr->update |= (PU_BONUS);
			}

			/* Update monsters */
			p_ptr->update |= (PU_MONSTERS);

			break;
		}


#ifdef ALLOW_DEBUG

		/* Special "debug" commands */
		case KTRL('A'):
		{
			if (verify_debug_mode()) do_cmd_debug();
			break;
		}

#endif

		/*** Inventory Commands ***/

		/* Wear/wield equipment */
		case 'w':
		{
			do_cmd_wield(NULL, 0);
			break;
		}

		/* Remove equipment */
		case 'r':
		{
			do_cmd_takeoff(NULL, 0);
			break;
		}

		/* Drop an item */
		case 'd':
		{
			do_cmd_drop();
			break;
		}

		/* Destroy an item */
		case 'k':
		{
			do_cmd_destroy();
			break;
		}

		/* Equipment list */
		case 'e':
		{
			do_cmd_equip();
			break;
		}

		/* Inventory list */
		case 'i':
		{
			do_cmd_inven();
			break;
		}

		/* Sing */
		case 's':
		{
			do_cmd_change_song();
			break;
		}
			
		/* Ability screen */
		case '\t':
		{
			do_cmd_ability_screen();
			break;
		}

		/* Smithing screen */
		case '0':
		case KTRL('D'): // Hack: required to get Angband-like keyset to work
		{
			do_cmd_smithing_screen();
			break;
		}

		/*** Various commands ***/

		/* Examine an object */
		case 'x':
		{
			do_cmd_observe();
			break;
		}

		/* Hack -- toggle windows */
		case KTRL('E'):
		{
			toggle_inven_equip();
			break;
		}


		/*** Standard "Movement" Commands ***/

		/* Alternate action */
		case '/':
		{
			do_cmd_alter();
			break;
		}

		/* Dig a tunnel */
		case 'T':
		{
			do_cmd_tunnel();
			break;
		}

		/* Walk */
		case ';':
		{
			do_cmd_walk();
			break;
		}

		/*** Running, Resting, Searching, Staying */

		/* Begin Running -- Arg is Max Distance */
		case '.':
		{
			do_cmd_run();
			break;
		}

		/* Hold still */
		case 'z':
		{
			do_cmd_hold();
			break;
		}

		/* Rest */
		case '%':
		case 'Z':
		{
			do_cmd_rest();
			break;
		}
			
		/* Get */
		case 'g':
		{
			do_cmd_pickup();
			break;
		}
			
		/* Toggle stealth mode */
		case 'S':
		{
			do_cmd_toggle_stealth();
			break;
		}

        /* Automaton */
		case '|':
		{
			if (verify_automaton_mode()) do_cmd_automaton();
			break;
		}

		/*** Stairs and Doors and Chests and Traps ***/

		/* Go up staircase */
		case '<':
		{
			// Autosave
			save_game_quietly = TRUE;
			do_cmd_save_game();
			
			do_cmd_go_up();
			break;
		}

		/* Go down staircase */
		case '>':
		{
			// Autosave
			save_game_quietly = TRUE;
			do_cmd_save_game();
			
			do_cmd_go_down();
			break;
		}

		/* Open a door or chest */
		case 'o':
		{
			do_cmd_open();
			break;
		}

		/* Close a door */
		case 'c':
		{
			do_cmd_close();
			break;
		}

		/* Bash a door */
		case 'b':
		{
			do_cmd_bash();
			break;
		}
			
		/* Disarm a trap or chest */
		case 'D':
		{
			do_cmd_disarm();
			break;
		}

		/* Exchange places */
		case 'X':
		{
			do_cmd_exchange();
			break;
		}
			
			
		/*** Use various objects ***/

		/* Inscribe an object */
		case '{':
		{
			do_cmd_inscribe();
			break;
		}

		/* Activate a staff */
		case 'a':
		{
			do_cmd_activate_staff(NULL, 0);
			break;
		}

		/* Eat some food */
		case 'E':
		{
			do_cmd_eat_food(NULL, 0);
			break;
		}

		/* Fire an arrow from the 1st quiver */
		case 'f':
		{
			do_cmd_fire(1);
			break;
		}

		/* Fire an arrow from the 2nd quiver */
		case 'F':
		{
			do_cmd_fire(2);
			break;
		}
			
		/* Throw an item */
		case 't':
		{
			do_cmd_throw(FALSE);
			break;
		}

        /* Throw an automatically chosen item at nearest target */
		case KTRL('T'):
		{
			do_cmd_throw(TRUE);
			break;
		}

		/* Play an instrument */
		case 'p':
		{
			do_cmd_play_instrument(NULL, 0);
			break;
		}
			
		/* Quaff a potion */
		case 'q':
		{
			do_cmd_quaff_potion(NULL, 0);
			break;
		}

		/* Use an item */
		case 'u':
		{
			do_cmd_use_item();
			break;
		}
			
			
		/*** Looking at Things (nearby or on map) ***/

		/* Full dungeon map */
		case 'M':
		{
			do_cmd_view_map();
			break;
		}

		/* Locate player on map */
		case 'L':
		{
			do_cmd_locate();
			break;
		}

		/* Look around */
		case 'l':
		{
			do_cmd_look();
			break;
		}

		/* Target monster or location */
		//case '*':
		//{
		//	do_cmd_target();
		//	break;
		//}



		/*** Help and Such ***/

		/* Help */
		case '?':
		{
			do_cmd_help();
			break;
		}

		/* Main menu */
		case 'm':
		{
			do_cmd_main_menu();
			break;
		}
		case ESCAPE:
		{
			if (easy_main_menu && !p_ptr->automaton) do_cmd_main_menu();
			break;
		}
			
		/* Identify symbol */
		//case '/':
		//{
		//	do_cmd_query_symbol();
		//	break;
		//}

		/* Character sheet */
		case '@':
		{
			do_cmd_character_sheet();
			break;
		}


		/*** System Commands ***/

		/* Hack -- User interface */
		case '!':
		{
			(void)Term_user(0);
			break;
		}

		/* Single line from a pref file */
		//case '"':
		//{
		//	do_cmd_pref();
		//	break;
		//}

		/* Interact with macros */
		case '$':
		{
			do_cmd_macros();
			break;
		}

		/* Interact with visuals */
		//case '%':
		//{
		//	do_cmd_visuals();
		//	break;
		//}

		/* Interact with colors */
		case '&':
		{
			do_cmd_colors();
			break;
		}

		/* Interact with options */
		case 'O':
		{
			do_cmd_options();
			do_cmd_redraw();
			break;
		}


		/*** Misc Commands ***/

		/* Take notes */
		case ':':
		{
			do_cmd_note("",  p_ptr->depth);
			break;
		}

		/* Version info */
		case 'V':
		{
			do_cmd_version();
			break;
		}

		/* Show previous message */
		case KTRL('O'):
		{
			do_cmd_message_one();
			break;
		}

		/* Show previous messages */
		case KTRL('P'):
		{
			do_cmd_messages();
			break;
		}

		/* Redraw the screen */
		case KTRL('R'):
		{
			do_cmd_redraw();
			break;
		}

#ifndef VERIFY_SAVEFILE

		/* Hack -- Save and don't quit */
		case KTRL('S'):
		{
			do_cmd_save_game();
			break;
		}

#endif

		/* Save and quit */
		case KTRL('X'):
		case KTRL('C'):
		{
			/* Stop playing */
			p_ptr->playing = FALSE;

			/* Leaving */
			p_ptr->leaving = TRUE;
			break;
        }

		/* Quit (commit suicide) */
		case 'Q':
		{
			do_cmd_suicide();
			break;
		}

		/* Check knowledge */
		case '~':
		{
			do_cmd_knowledge();
			break;
		}

		/* Save "screen shot" */
		case ')':
		{
			do_cmd_save_screen();
			break;
		}
        
		/* Hack -- Unknown command */
		default:
		{
			prt("Type '?' for help.", 0, 0);
			break;
		}
	}
}

/*
 * Determine if the object can be picked up, and either has "=g" in its inscription
 * or has the pickup flag set to true (e.g. for thrown and fired items)
 */
static bool auto_pickup_okay(const object_type *o_ptr)
{
	//cptr s;
    
	/* It can't be carried */
	if (!inven_carry_okay(o_ptr)) return (FALSE);
    
	/*object is marked to not pickup*/
	if ((k_info[o_ptr->k_idx].squelch == NO_SQUELCH_NEVER_PICKUP) &&
	    object_aware_p(o_ptr)) return (FALSE);
    
	/*object is marked to not pickup*/
	if ((k_info[o_ptr->k_idx].squelch == NO_SQUELCH_ALWAYS_PICKUP) &&
	    object_aware_p(o_ptr)) return (TRUE);
    
	/* object has pickup flag set */
	if (o_ptr->pickup) return (TRUE);
    
	/* No inscription */
	if (!o_ptr->obj_note) return (FALSE);
    
	/* Find a '=' */
	//s = strchr(quark_str(o_ptr->obj_note), '=');
    
	/* Process inscription */ // Sil-y: turned the =g inscriptions off for now
	//while (s)
	//{
	//	/* Auto-pickup on "=g" */
	//	if (s[1] == 'g') return (TRUE);
    
	//	/* Find another '=' */
	//	s = strchr(s + 1, '=');
	//}
    
	/* Don't auto pickup */
	return (FALSE);
}

/*
 * Finish your leap
 */
void land(void)
{
    // the player has landed
    p_ptr->leaping = FALSE;
    
    // make some noise when landing
    stealth_score -= 5;

	/* Set off traps */
	if (cave_trap_bold(p_ptr->py, p_ptr->px) || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM))
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

/*
 * Continue your leap
 */
void continue_leap(void)
{
    int dir;
	int y_end, x_end;   // the desired endpoint of the leap
	
	dir = p_ptr->previous_action[1];
    
	/* Get location */
	y_end = p_ptr->py + ddy[dir];
	x_end = p_ptr->px + ddx[dir];
    
    // display a message until player input is received
    msg_print("You fly through the air.");
    message_flush();
    
	/* Take a turn */
	p_ptr->energy_use = 100;
    
	// store the action type
	p_ptr->previous_action[0] = dir;
	
    // solid objects end the leap
    if (cave_info[y_end][x_end] & (CAVE_WALL))
    {
        if (cave_feat[y_end][x_end] == FEAT_RUBBLE)
        {
            msg_print("You slam into a wall of rubble.");
        }
        if (cave_wall_bold(y_end, x_end))
        {
            msg_print("You slam into a wall.");
        }
        else if (cave_any_closed_door_bold(y_end, x_end))
        {
            msg_print("You slam into a door.");
        }
    }
    
    // monsters end the leap
    else if (cave_m_idx[y_end][x_end] > 0)
    {
        monster_type *m_ptr = &mon_list[cave_m_idx[y_end][x_end]];
        char m_name[80];
        
        /* Get the monster name */
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);
        
        if (m_ptr->ml)  msg_format("%^s blocks your landing.", m_name);
        else            msg_format("Some unseen foe blocks your landing.", m_name);
    }
    
    // successful leap
    else
    {
        // we generously give you your free flanking attack...
        flanking_or_retreat(y_end, x_end);
        
        // move player to the new position
        monster_swap(p_ptr->py, p_ptr->px, y_end, x_end);
    }
    
    // land on the ground
    land();
}


/*
 * Hack -- helper function for "process_player()"
 *
 * Check for changes in the "monster memory"
 */
static void process_player_aux(void)
{
	int i;
	bool changed = FALSE;

	static int old_monster_race_idx = 0;

	static u32b	old_flags1 = 0L;
	static u32b	old_flags2 = 0L;
	static u32b	old_flags3 = 0L;
	static u32b	old_flags4 = 0L;

	static byte old_blows[MONSTER_BLOW_MAX];

	static byte	old_ranged = 0;


	/* Tracking a monster */
	if (p_ptr->monster_race_idx)
	{
		/* Get the monster lore */
		monster_lore *l_ptr = &l_list[p_ptr->monster_race_idx];

		for (i = 0; i < MONSTER_BLOW_MAX; i++)
		{
			if (old_blows[i] != l_ptr->blows[i])
			{
				changed = TRUE;
				break;
			}
		}

		/* Check for change of any kind */
		if (changed ||
		    (old_monster_race_idx != p_ptr->monster_race_idx) ||
		    (old_flags1 != l_ptr->flags1) ||
		    (old_flags2 != l_ptr->flags2) ||
		    (old_flags3 != l_ptr->flags3) ||
		    (old_flags4 != l_ptr->flags4) ||
		    (old_ranged != l_ptr->ranged))

		{
			/* Memorize old race */
			old_monster_race_idx = p_ptr->monster_race_idx;

			/* Memorize flags */
			old_flags1 = l_ptr->flags1;
			old_flags2 = l_ptr->flags2;
			old_flags3 = l_ptr->flags3;
			old_flags4 = l_ptr->flags4;

			/* Memorize blows */
			for (i = 0; i < MONSTER_BLOW_MAX; i++)
				old_blows[i] = l_ptr->blows[i];

			/* Memorize castings */
			old_ranged = l_ptr->ranged;

			/* Window stuff */
			p_ptr->window |= (PW_MONSTER);

			/* Window stuff */
			window_stuff();
		}
	}
}


/*
 * Process the player
 *
 * Notice the annoying code to handle "pack overflow", which
 * must come first just in case somebody manages to corrupt
 * the savefiles by clever use of menu commands or something.
 *
 * Notice the annoying code to handle "monster memory" changes,
 * which allows us to avoid having to update the window flags
 * every time we change any internal monster memory field, and
 * also reduces the number of times that the recall window must
 * be redrawn.
 *
 * Note that the code to check for user abort during repeated commands
 * and running and resting can be disabled entirely with an option, and
 * even if not disabled, it will only check during every 128th game turn
 * while resting, for efficiency.
 */
static void process_player(void)
{
	int i;
	int amount;
	int regen_multiplier;
		
	// reset the number of times you have riposted since last turn
	p_ptr->ripostes = 0;
	
	// reset whether you have just woken up from entrancement
	p_ptr->was_entranced = FALSE;

	// update the player's torch radius
	calc_torch();
	
	/*** Check certain things between player turns (don't need to do this when restoring a game) ***/

	if (!p_ptr->restoring)
	{
		/*** Check for interrupts ***/

		/* Complete resting */
		if (p_ptr->resting < 0)
		{
			/* Basic resting */
			if (p_ptr->resting == -1)
			{
				/* Stop resting */
				if ((p_ptr->chp == p_ptr->mhp) &&
					(p_ptr->csp == p_ptr->msp))
				{
					disturb(0, 0);
				}
			}

			/* Complete resting */
			else if (p_ptr->resting == -2)
			{
				/* Stop resting */
				if ((p_ptr->chp == p_ptr->mhp) &&
					((p_ptr->csp == p_ptr->msp) || !singing(SNG_NOTHING)) &&
					!p_ptr->blind && !p_ptr->confused &&
					!p_ptr->poisoned && !p_ptr->afraid &&
					!p_ptr->stun && !p_ptr->cut &&
					!p_ptr->slow && !p_ptr->entranced)
				{
					disturb(0, 0);
				}
			}
		}

		/* Check for "player abort" */
		if (p_ptr->running ||
			p_ptr->smithing ||
			p_ptr->command_rep ||
			(p_ptr->resting && !(turn & 0x7F)))
		{
			/* Do not wait */
			inkey_scan = TRUE;

			/* Check for a key */
			if (inkey())
			{
				/* Flush input */
				flush();

				/* Disturb */
				disturb(0, 0);
                
                // cancel automaton control
                p_ptr->automaton = FALSE;
                
				/* Hack -- Show a Message */
				msg_print("Cancelled.");
			}
		}

		/*** Other checks ***/

		// Make the stealth-modified noise (has to occur after monsters have had a chance to move)
		monster_perception(TRUE, TRUE, stealth_score);

		// Stop stealth mode if something happened
		if (stop_stealth_mode)
		{
			/* Cancel */
			p_ptr->stealth_mode = FALSE;
			
			/* Recalculate bonuses */
			p_ptr->update |= (PU_BONUS);
			
			/* Redraw the state */
			p_ptr->redraw |= (PR_STATE);			
			
			// Reset the flag
			stop_stealth_mode = FALSE;
		}
		
		// Morgoth will announce a challenge if adjacent
		if (p_ptr->truce && (p_ptr->depth == MORGOTH_DEPTH))
		{
			int d, yy, xx;
			
			/* Check around the character */
			for (d = 0; d < 8; d++)
			{
				monster_type *m_ptr;
				
				/* Extract adjacent (legal) location */
				yy = p_ptr->py + ddy_ddd[d];
				xx = p_ptr->px + ddx_ddd[d];
								
				// paranoia
				if (cave_m_idx[yy][xx] < 0) continue;
				
				m_ptr = &mon_list[cave_m_idx[yy][xx]];
				
				if ((m_ptr->r_idx == R_IDX_MORGOTH) && (m_ptr->alertness >= ALERTNESS_ALERT))
				{
					msg_print("With a voice as of rolling thunder, Morgoth, Lord of Darkness, speaks:");
					msg_print("'You dare challenge me in mine own hall? Now is your death upon you!'");

					// Break the truce (always)
					break_truce(TRUE);
				}
			}
		}

		/* List all challenge options at the start of the game */
		if (playerturn == 1)
		{
			for (i = 0; i < OPT_PAGE_PER; i++)
			{
				int option_number = option_page[CHALLENGE_PAGE][i];
				
				/* Collect options on this "page" */
				if ((option_number != OPT_NONE) && (op_ptr->opt[option_number]))
				{
					do_cmd_note(format("Challenge: %s", option_desc[option_number]), p_ptr->depth);
				}
			}
		}

		// shuffle along the array of previous actions
		for (i = ACTION_MAX-1; i > 0; i--)
		{
			p_ptr->previous_action[i] = p_ptr->previous_action[i-1];
		}
		// put in a default for this turn
		// Sil-y: it is possible that this isn't always changed to something else, but I think it is
		p_ptr->previous_action[0] = ACTION_NOTHING;
				
		/* Redraw stuff (if needed) */
		if (p_ptr->window) window_stuff();
		
		// Sil-y: have to update the player bonuses at every turn with sprinting, dodging etc.
		//        this might cause annoying slowdowns, I'm not sure
		p_ptr->update |= (PU_BONUS);

	}

	
	/*** Handle actual user input ***/

	/* Repeat until energy is reduced */
	do
	{
		/* Notice stuff (if needed) */
		if (p_ptr->notice) notice_stuff();

		/* Update stuff (if needed) */
		if (p_ptr->update) update_stuff();

		/* Redraw stuff (if needed) */
		if (p_ptr->redraw) redraw_stuff();

		/* Redraw stuff (if needed) */
		if (p_ptr->window) window_stuff();

		/* Place the cursor on the player or target */
		if (hilite_player) move_cursor_relative(p_ptr->py, p_ptr->px);
		if (hilite_target && target_sighted()) move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

		if (cheat_noise) display_noise_map();
		else if (cheat_scent) display_scent_map();
		else if (cheat_light) display_light_map();

		/* Refresh */
		Term_fresh();

		/* Hack -- Pack Overflow if needed */
		check_pack_overflow();

		if (cave_o_idx[p_ptr->py][p_ptr->px] != 0)
		{
			(&o_list[cave_o_idx[p_ptr->py][p_ptr->px]])->marked = TRUE;
		}

		/* Hack -- cancel "lurking browse mode" */
		if (!p_ptr->command_new) p_ptr->command_see = FALSE;

		/* Assume free turn */
		p_ptr->energy_use = 0;

		// Reset number of attacks this turn
		new_combat_round();

		// get base stealth score for the round
		// this will get modified by the type of action
		stealth_score = p_ptr->skill_use[S_STL];

		// display a note at the start of the game
		if ((cave_o_idx[p_ptr->py][p_ptr->px] != 0))
		{
			object_type *o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
			if ((o_ptr->tval == TV_NOTE) && (playerturn == 1)) 
			{
				note_info_screen(o_ptr);
			}
		}
				
        /* Leaping */
        if (p_ptr->leaping)
        {
            continue_leap();
        }
        
		/* Entranced or Knocked Out */
		else if ((p_ptr->entranced) || (p_ptr->stun > 100))
		{
			// stop singing
			change_song(SNG_NOTHING);
			
			/* Take a turn */
			p_ptr->energy_use = 100;
			
			// store the action type
			p_ptr->previous_action[0] = ACTION_MISC;
		}

		/* Smithing */
		else if (p_ptr->smithing)
		{
			if (p_ptr->smithing == 1)
			{
				// Display a message
				msg_print("You complete your work.");

				create_smithing_item();
			}
			
			/* Reduce smithing count */
			p_ptr->smithing--;

			/* Reduce smithing leftover counter */
			p_ptr->smithing_leftover--;

			/* Take a turn */
			p_ptr->energy_use = 100;

			// store the action type
			p_ptr->previous_action[0] = ACTION_MISC;
			
			/* Redraw the state */
			p_ptr->redraw |= (PR_STATE);
		}
		
		/* Resting */
		else if (p_ptr->resting)
		{
			/* Timed rest */
			if (p_ptr->resting > 0)
			{
				/* Reduce rest count */
				p_ptr->resting--;
				
				/* Redraw the state */
				p_ptr->redraw |= (PR_STATE);
			}

			/* Take a turn */
			p_ptr->energy_use = 100;

			// store the action type
			p_ptr->previous_action[0] = 5;
			
			// store the 'focus' attribute
			p_ptr->focused = TRUE;

			/* Searching */ 
			search();
		}
        
        /* Recovering footing */
        else if (p_ptr->skip_next_turn)
        {
            // let the player know
            msg_print("You recover your footing.");
            
            // force a -more-
            message_flush();
            
            // reset flag
            p_ptr->skip_next_turn = FALSE;

            /* Take a turn */
			p_ptr->energy_use = 100;
			
			// store the action type
			p_ptr->previous_action[0] = ACTION_MISC;
        }

		/* Running */
		else if (p_ptr->running)
		{
			/* Take a step */
			run_step(0);
			
			// Pause for 17 miliseconds (minimum needed for mac OS X to pause)
			if (!instant_run)
			{
				Term_xtra(TERM_XTRA_DELAY, 17);
			}
		}
        
		/* Repeated command */
		else if (p_ptr->command_rep)
		{
			/* Hack -- Assume messages were seen */
			msg_flag = FALSE;

			/* Clear the top line */
			prt("", 0, 0);

			/* Process the command */
			process_command();

			/* Count this execution */
			if (p_ptr->command_rep)
			{
				/* Count this execution */
				p_ptr->command_rep--;

				/* Redraw the state */
				p_ptr->redraw |= (PR_STATE);

			}
		}

		/* Normal command */
		else
		{
            char out_val[160];
            char o_name[80];
            object_type *o_ptr;
                        
            // build an object description
            if (cave_o_idx[p_ptr->py][p_ptr->px])
            {
                o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
                
                /* Describe the object */
                object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
                strnfmt(out_val, sizeof(out_val), "Pick up %s? ", o_name);
            }

            // always offer to pickup if the mode is on, there is an object present, and you have just moved
            if (always_pickup && cave_o_idx[p_ptr->py][p_ptr->px] && (o_ptr->tval != TV_NOTE) &&
                (p_ptr->previous_action[1] >= 1) && (p_ptr->previous_action[1] <= 9) && (p_ptr->previous_action[1] != 5))
            {
                // allow the player to decline to pick up the object
                if (get_check(out_val))
                {
                    /* Handle "objects" */
                    py_pickup();
                }
            }
            
            // if the player hasn't used their turn picking something up...
            if (p_ptr->energy_use < 100)
            {
                /* Check monster recall */
                process_player_aux();
                
                /* Place the cursor on the player or target */
                if (hilite_player) move_cursor_relative(p_ptr->py, p_ptr->px);
                if (hilite_target && target_sighted()) move_cursor_relative(p_ptr->target_row, p_ptr->target_col);
                
                /* We are certainly no longer in the process of restoring a game */
                p_ptr->restoring = FALSE;
                
                /* Get a command (normal) */
                request_command();
                
                /* Process the command */
                process_command();
            }
            
            // check the item under the player
            o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

            /* Test for auto-pickup for thrown/fired items */
            if (auto_pickup_okay(o_ptr))
            {
                /* Pick up the object */
                py_pickup_aux(cave_o_idx[p_ptr->py][p_ptr->px]);
            }
		}		
		
		/*** Clean up ***/

		/* Check for greater vault squares */
		if ((cave_info[p_ptr->py][p_ptr->px] & (CAVE_G_VAULT)) &&
		    (g_vault_name[0] != '\0'))
		{
			char note[120];
			char *fmt = "Entered %s";

			strnfmt(note, sizeof(note), fmt, g_vault_name);

			do_cmd_note(note, p_ptr->depth);

            // give a message unless it is the Gates or the Throne Room
			if (p_ptr->depth > 0 && p_ptr->depth < 20)
            {
				msg_format("You have entered %s.", g_vault_name);
            }

		  	g_vault_name[0] = '\0';
		}

		/* Significant */
		if (p_ptr->energy_use)
		{
			/* Use some energy */
			p_ptr->energy -= p_ptr->energy_use;


			/* Hack -- constant hallucination */
			if (p_ptr->image) p_ptr->redraw |= (PR_MAP);


			/* Shimmer monsters if needed */
			if (shimmer_monsters)
			{
				/* Clear the flag */
				shimmer_monsters = FALSE;

				/* Shimmer multi-hued monsters */
				for (i = 1; i < mon_max; i++)
				{
					monster_type *m_ptr;
					monster_race *r_ptr;

					/* Get the monster */
					m_ptr = &mon_list[i];

					/* Skip dead monsters */
					if (!m_ptr->r_idx) continue;

					/* Get the monster race */
					r_ptr = &r_info[m_ptr->r_idx];

					/* Skip non-multi-hued monsters */
					if (!(r_ptr->flags1 & (RF1_ATTR_MULTI))) continue;

					/* Reset the flag */
					shimmer_monsters = TRUE;

					/* Redraw regardless */
					lite_spot(m_ptr->fy, m_ptr->fx);
				}
			}

			/* Repair "mark" flags */
			if (repair_mflag_mark)
			{
				/* Reset the flag */
				repair_mflag_mark = FALSE;

				/* Process the monsters */
				for (i = 1; i < mon_max; i++)
				{
					monster_type *m_ptr;

					/* Get the monster */
					m_ptr = &mon_list[i];

					/* Skip dead monsters */
					/* if (!m_ptr->r_idx) continue; */

					/* Repair "mark" flag */
					if (m_ptr->mflag & (MFLAG_MARK))
					{
						/* Skip "show" monsters */
						if (m_ptr->mflag & (MFLAG_SHOW))
						{
							/* Repair "mark" flag */
							repair_mflag_mark = TRUE;

							/* Skip */
							continue;
						}

						/* Forget flag */
						m_ptr->mflag &= ~(MFLAG_MARK);

						/* Update the monster */
						update_mon(i, FALSE);
					}
				}
			}
		}
		
		/* Repair "show" flags */
		if (repair_mflag_show)
		{
			/* Reset the flag */
			repair_mflag_show = FALSE;

			/* Process the monsters */
			for (i = 1; i < mon_max; i++)
			{
				monster_type *m_ptr;

				/* Get the monster */
				m_ptr = &mon_list[i];

				/* Skip dead monsters */
				/* if (!m_ptr->r_idx) continue; */

				/* Clear "show" flag */
				m_ptr->mflag &= ~(MFLAG_SHOW);
			}
		}
	}
	while (!p_ptr->energy_use && !p_ptr->leaving);

    // if the player is exiting the the game in some manner then stop processing now
    if (p_ptr->leaving) return;
    
	/* Do song effects */
	sing();

	// make less noise if you did nothing at all
	// (+7 in total whether or not stealth mode is used)
	if (p_ptr->resting)
	{
		if (p_ptr->stealth_mode) stealth_score += 2;
		else					 stealth_score += 7;
	}

	// make much more noise when smithing
	if (p_ptr->smithing)
	{
		/* Make a lot of noise */
		monster_perception(TRUE, FALSE, -10);
	}
	
    // update player noise
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);

	/* Update scent trail */
	update_smell();

	/* possibly identify passive abilities every so often*/
	if (one_in_(500))
	{
		ident_passive();
	}
	
	/*** Damage over Time ***/

	/* Take damage from poison */
	if (p_ptr->poisoned)
	{
		/* Take damage */
		
		// amount is one fifth of the poison, rounding up 
		amount = (p_ptr->poisoned + 4) / 5;
			
		take_hit(amount, "poison");
	}

	/* Take damage from cuts */
	if (p_ptr->cut)
	{
		amount = (p_ptr->cut + 4) / 5;
			
		/* Take damage */
		take_hit(amount, "a fatal wound");
	}

	/* Reduce the wrath counter */
	if (p_ptr->wrath)
	{
		amount = (p_ptr->wrath / 100) * (p_ptr->wrath / 100);
		
		// half as fast if still singing the song
		if (singing(SNG_SLAYING))
		{
			p_ptr->wrath -= MAX(amount/2, 1);
		}
		else
		{
			p_ptr->wrath -= MAX(amount, 1);
		}
		p_ptr->update |= (PU_BONUS);
		p_ptr->redraw |= (PR_SONG);
	}
	
	/*** Check the Food, and Regenerate ***/

	/* Basic digestion rate */
	i = 1;
	
	// Note: speed and regeneration are taken into account already in the hunger rate

	// Hack: slow hunger rates are done statistically
	if (p_ptr->hunger < 0)
	{
		if (!one_in_(int_exp(3,-(p_ptr->hunger))))
		{
			i = 0;
		}
	}
	else if (p_ptr->hunger > 0)
	{
		i *= int_exp(3,p_ptr->hunger);
	}
	
	/* Digest quickly when gorged */
	if (p_ptr->food >= PY_FOOD_MAX) i *= 9;
		
	/* Digest some food */
	(void)set_food(p_ptr->food - i);
	
	/* Starve to death (slowly) */
	if (p_ptr->food < PY_FOOD_STARVE)
	{
		/* Calculate damage */
		i = 1; // old: (PY_FOOD_STARVE - p_ptr->food) / 10;

		/* Take damage */
		take_hit(i, "starvation");
	}

	/* Lower the staircasiness */
	if (p_ptr->staircasiness > 0)
	{
        // decreases much faster on the escape
        if (p_ptr->on_the_run)
        {
            // amount is one hundredth of the current value, rounding up
            amount = (p_ptr->staircasiness + 99) / 100;
        }
        
        else
        {
            // amount is one thousandth of the current value, rounding up
            amount = (p_ptr->staircasiness + 999) / 1000;
        }
	       
		p_ptr->staircasiness -= amount;
	}
	
	/* Increase the time since the last forge */
	p_ptr->forge_drought++;
	
	/* Regeneration ability */
    regen_multiplier = p_ptr->regenerate + 1;
	
	/* Regenerate the mana */
	if (p_ptr->csp < p_ptr->msp)
	{
		regenmana(regen_multiplier);
	}

	/* Various things interfere with healing */
    if (p_ptr->food < PY_FOOD_STARVE) regen_multiplier = 0;
	if (p_ptr->poisoned) regen_multiplier = 0;
	if (p_ptr->cut) regen_multiplier = 0;

	if (singing(SNG_ESTE))
	{
		regen_multiplier *= ability_bonus(S_SNG, SNG_ESTE);
	}
	
	/* Regenerate Hit Points if needed */
	if (p_ptr->chp < p_ptr->mhp)
	{
		regenhp(regen_multiplier);
	}
	
	/*** Timeout Various Things ***/

	if (singing(SNG_ESTE))
	{
		amount = ability_bonus(S_SNG, SNG_ESTE);
	}
	else
	{
		amount = 1;
	}

	/* Hack -- Hallucinating */
	if (p_ptr->image)
	{
		(void)set_image(p_ptr->image - amount);
	}

	/* Blindness */
	if (p_ptr->blind)
	{
		(void)set_blind(p_ptr->blind - amount);
	}

	/* Timed see-invisible */
	if (p_ptr->tim_invis)
	{
		(void)set_tim_invis(p_ptr->tim_invis - 1);
	}

	/* Entranced */
	if (p_ptr->entranced)
	{
		(void)set_entranced(p_ptr->entranced - amount);
	}

	/* Confusion */
	if (p_ptr->confused)
	{
		(void)set_confused(p_ptr->confused - amount);
	}

	/* Afraid */
	if (p_ptr->afraid)
	{
		(void)set_afraid(p_ptr->afraid - amount);
	}

	/* Darkened */
	if (p_ptr->darkened)
	{
		(void)set_darkened(p_ptr->darkened - amount);
	}
		
	/* Fast */
	if (p_ptr->fast)
	{
		(void)set_fast(p_ptr->fast - 1);
	}

	/* Slow */
	if (p_ptr->slow)
	{
		if (singing(SNG_FREEDOM)) amount = ability_bonus(S_SNG, SNG_FREEDOM);
		
		(void)set_slow(p_ptr->slow - amount);
	}

	/* Rage */
	if (p_ptr->rage)
	{
		(void)set_rage(p_ptr->rage - 1);
	}

	/* Temporary Strength */
	if (p_ptr->tmp_str)
	{
		(void)set_tmp_str(p_ptr->tmp_str - 1);
	}

	/* Temporary Dexterity */
	if (p_ptr->tmp_dex)
	{
		(void)set_tmp_dex(p_ptr->tmp_dex - 1);
	}

	/* Temporary Constitution */
	if (p_ptr->tmp_con)
	{
		(void)set_tmp_con(p_ptr->tmp_con - 1);
	}

	/* Temporary Grace */
	if (p_ptr->tmp_gra)
	{
		(void)set_tmp_gra(p_ptr->tmp_gra - 1);
	}

	/* Oppose Fire */
	if (p_ptr->oppose_fire)
	{
		(void)set_oppose_fire(p_ptr->oppose_fire - 1);
	}

	/* Oppose Cold */
	if (p_ptr->oppose_cold)
	{
		(void)set_oppose_cold(p_ptr->oppose_cold - 1);
	}

	/* Oppose Poison */
	if (p_ptr->oppose_pois)
	{
		(void)set_oppose_pois(p_ptr->oppose_pois - 1);
	}

	/*** Poison and Stun and Cut ***/

	/* Poison */
	if (p_ptr->poisoned)
	{
		// adjust is one fifth of the poison, rounding up 
		int adjust = (p_ptr->poisoned + 4) / 5;
			
		/* Apply some healing */
		(void)set_poisoned(p_ptr->poisoned - adjust * amount);
	}

	/* Stun */
	if (p_ptr->stun)
	{
		int adjust = 1;

		/* Apply some healing */
		(void)set_stun(p_ptr->stun - adjust * amount);
	}

	/* Cut */
	if (p_ptr->cut)
	{
		// adjust is one fifth of the wound, rounding up 
		int adjust = (p_ptr->cut + 4) / 5;

		/* Apply some healing */
		(void)set_cut(p_ptr->cut - adjust * amount);
	}
	
	// reset the focus flag if the player didn't 'pass' this turn
	if (p_ptr->previous_action[0] != 5)
	{
		p_ptr->focused = FALSE;
	}
	
	// if the player didn't attack or 'pass' then the consecutive attacks needs to be reset
	if (!player_attacked && (p_ptr->previous_action[0] != 5))
	{
		p_ptr->consecutive_attacks = 0;
		p_ptr->last_attack_m_idx = 0;
	}
	
	// boots of radiance
	if (inventory[INVEN_FEET].k_idx)
	{
		u32b f1, f2, f3;
		object_type *o_ptr = &inventory[INVEN_FEET];
		
		/* Extract the flags */
		object_flags(o_ptr, &f1, &f2, &f3);

		if (f2 & (TR2_RADIANCE))
		{
			if (!(cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW)))
			{
				if (!object_known_p(o_ptr) && one_in_(10))
				{
					char o_short_name[80];
					char o_full_name[80];
					
					object_desc(o_short_name, sizeof(o_short_name), o_ptr, FALSE, 0);
					object_aware(o_ptr);
					object_known(o_ptr);
					object_desc(o_full_name, sizeof(o_full_name), o_ptr, TRUE, 3);
					
					msg_print("Your footsteps leave a trail of light!");
					msg_format("You recognize your %s to be %s", o_short_name, o_full_name);
				}
				
				cave_info[p_ptr->py][p_ptr->px] |= CAVE_GLOW;
			}
		}
	}
	
	playerturn++;

	/* Window stuff */
	
	// Sil-y: note that these are now being set every single turn, somewhat defeating their purpose
	p_ptr->window |= (PW_INVEN | PW_EQUIP);
	p_ptr->window |= (PW_COMBAT_ROLLS);

}

/*
 * Interact with the current dungeon level.
 *
 * This function will not exit until the level is completed,
 * the user dies, or the game is terminated.
 */
static void dungeon(void)
{
	monster_type *m_ptr;
	int i;

	/* Hack -- enforce illegal panel */
	p_ptr->wy = p_ptr->cur_map_hgt;
	p_ptr->wx = p_ptr->cur_map_wid;

	/* Not leaving */
	p_ptr->leaving = FALSE;

	/* Reset the "command" vars */
	p_ptr->command_cmd = 0;
	p_ptr->command_new = 0;
	p_ptr->command_rep = 0;
	p_ptr->command_arg = 0;
	p_ptr->command_dir = 0;


	/* Cancel the target */
	target_set_monster(0);

	/* Cancel the health bar */
	health_track(0);

	/* Reset shimmer flags */
	shimmer_monsters = TRUE;
	shimmer_objects = TRUE;

	/* Reset repair flags */
	repair_mflag_show = TRUE;
	repair_mflag_mark = TRUE;

	/* Disturb */
	disturb(0, 0);

	/* Track maximum dungeon level */
	if (p_ptr->max_depth < p_ptr->depth)
	{
		for (i = p_ptr->max_depth + 1; i <= p_ptr->depth; i++)
		{
			if (i > 1)
			{
				int new_exp = i * 50;
				gain_exp(new_exp);
				p_ptr->descent_exp += new_exp;
				
				// Sil-x
				//do_cmd_note(format("exp:%d = s:5000 + e:%d + k:%d + d:%d + i:%d",
				//		    p_ptr->exp, p_ptr->encounter_exp, p_ptr->kill_exp, p_ptr->descent_exp, p_ptr->ident_exp), i);
			}
		}
		p_ptr->max_depth = p_ptr->depth;
	}

	/* No stairs from the surface */
	if (!p_ptr->depth)
	{
		p_ptr->create_stair = FALSE;
	}

	/* Make a staircase */
	if (p_ptr->create_stair)
	{
		/* Place a staircase */
		if (cave_valid_bold(p_ptr->py, p_ptr->px))
		{
			/* XXX XXX XXX */
			delete_object(p_ptr->py, p_ptr->px);

			cave_set_feat(p_ptr->py, p_ptr->px, p_ptr->create_stair);

			/* Mark the stairs as known */
			cave_info[p_ptr->py][p_ptr->px] |= (CAVE_MARK);
		}

		/* Cancel the stair request */
		p_ptr->create_stair = FALSE;
	}

	/* Make rubble */
	if (p_ptr->create_rubble)
	{
		earthquake(p_ptr->py, p_ptr->px, -1, -1, 5, 0);
		
		/* Cancel the rubble request */
		p_ptr->create_rubble = FALSE;
	}
	
	/* Choose panel */
	verify_panel();

	/* Flush messages */
	message_flush();

	/* Hack -- Increase "xtra" depth */
	character_xtra++;

	/* Clear */
	Term_clear();

	/* Update stuff */
	p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

	/* Update stuff */
	update_stuff();

	/* Fully update the visuals (and monster distances) */
	p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_DISTANCE);

	/* Redraw dungeon */
	p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);

	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

	/* Window stuff */
	p_ptr->window |= (PW_MONSTER | PW_MONLIST | PW_COMBAT_ROLLS);

	/* Window stuff */
	p_ptr->window |= (PW_OVERHEAD);

	/* Update stuff */
	update_stuff();

	/* Redraw stuff */
	redraw_stuff();

	/* Redraw stuff */
	window_stuff();

	/* Hack -- Decrease "xtra" depth */
	character_xtra--;

	/* Update stuff */
	p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

	/* Combine / Reorder the pack */
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);

	/* Notice stuff */
	notice_stuff();

	/* Update stuff */
	update_stuff();

	/* Redraw stuff */
	redraw_stuff();

	/* Window stuff */
	window_stuff();

	/* Refresh */
	Term_fresh();

	/* Handle delayed death */
	if (p_ptr->is_dead) return;

	/* Announce (or repeat) the feeling */
	//if ((p_ptr->depth) && (do_feeling)) do_cmd_feeling();

	/* Announce a player ghost challenge. -LM- */
	if (bones_selector) ghost_challenge();

	// explain the truce for the final level
	if ((p_ptr->depth == MORGOTH_DEPTH) && p_ptr->truce)
	{
		msg_print("There is a strange tension in the air.");
		if (p_ptr->skill_use[S_PER] >= 15) msg_print("You feel that Morgoth's servants are reluctant to attack before he delivers judgment.");	
	}

	/*** Process this dungeon level ***/

	/* Reset the monster generation level */
	monster_level = p_ptr->depth;

	/* Reset the object generation level */
	object_level = p_ptr->depth;

	/* Main loop */
	while (TRUE)
	{

		/* Hack -- Compact the monster list occasionally */
		if (mon_cnt + 10 > MAX_MONSTERS) compact_monsters(20);

		/* Hack -- Compress the monster list occasionally */
		if (mon_cnt + 32 < MAX_MONSTERS) compact_monsters(0);

		/* Hack -- Compact the object list occasionally */
		if (o_cnt + 32 > z_info->o_max) compact_objects(64);

		/* Hack -- Compress the object list occasionally */
		if (o_cnt + 32 < o_max) compact_objects(0);


		/*** Apply energy ***/

		/* Can the player move? */
		while ((p_ptr->energy >= 100) && (!p_ptr->leaving))
		{
			/* Process monster with even more energy first */
			process_monsters(p_ptr->energy + 1);

			/* If still alive */
			if (!p_ptr->leaving)
			{
				/* Update stuff */
				if (p_ptr->update) update_stuff();
				
				/* Redraw stuff */
				if (p_ptr->redraw) redraw_stuff();

				/* Process the player */
				process_player();
			}

		}

		/* Notice stuff */
		if (p_ptr->notice) notice_stuff();

		/* Update stuff */
		if (p_ptr->update) update_stuff();

		/* Redraw stuff */
		if (p_ptr->redraw) redraw_stuff();

		/* Redraw stuff */
		if (p_ptr->window) window_stuff();

		/* Place the cursor on the player or target */
		if (hilite_player) move_cursor_relative(p_ptr->py, p_ptr->px);
		if (hilite_target && target_sighted()) move_cursor_relative(p_ptr->target_row, p_ptr->target_col);
				
		/* Optional fresh */
		if (fresh_after) Term_fresh();

		/* Handle "leaving" */
		if (p_ptr->leaving) break;

		/* Process monsters (any that haven't had a chance to move yet) */
		process_monsters(100);

		/* Notice stuff */
		if (p_ptr->notice) notice_stuff();

		/* Update stuff */
		if (p_ptr->update) update_stuff();

		/* Redraw stuff */
		if (p_ptr->redraw) redraw_stuff();

		/* Redraw stuff */
		if (p_ptr->window) window_stuff();

		/* Place the cursor on the player or target */
		if (hilite_player) move_cursor_relative(p_ptr->py, p_ptr->px);
		if (hilite_target && target_sighted()) move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

		/* Optional fresh */
		if (fresh_after) Term_fresh();

		/* Handle "leaving" */
		if (p_ptr->leaving) break;

		/* Process the world */
		process_world();

		/* Notice stuff */
		if (p_ptr->notice) notice_stuff();

		/* Update stuff */
		if (p_ptr->update) update_stuff();

		/* Redraw stuff */
		if (p_ptr->redraw) redraw_stuff();

		/* Window stuff */
		if (p_ptr->window) window_stuff();

		/* Place the cursor on the player or target */
		if (hilite_player) move_cursor_relative(p_ptr->py, p_ptr->px);
		if (hilite_target && target_sighted()) move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

		/* Optional fresh */
		if (fresh_after) Term_fresh();

		/* Handle "leaving" */
		if (p_ptr->leaving) break;

		/* Give the player some energy */
		p_ptr->energy += extract_energy[p_ptr->pspeed];

		/* Give energy to all monsters */
		for (i = mon_max - 1; i >= 1; i--)
		{
			/* Access the monster */
			m_ptr = &mon_list[i];

			/* Ignore "dead" monsters */
			if (!m_ptr->r_idx) continue;

			/* Give this monster some energy */
			m_ptr->energy += extract_energy[m_ptr->mspeed];
		}

		/* Count game turns */
		turn++;
	}
}



/*
 * Process some user pref files
 */
static void process_some_user_pref_files(void)
{
	char buf[1024];


	/* Process the "user.prf" file */
	(void)process_pref_file("user.prf");

	/* Process the "user.scb" autoinscriptions file */
	(void)process_pref_file("user.scb");

	/* Process the "races.prf" file */
	(void)process_pref_file("races.prf");

	/* Get the "PLAYER.prf" filename */
	(void)strnfmt(buf, sizeof(buf), "%s.prf", op_ptr->base_name);

	/* Process the "PLAYER.prf" file */
	(void)process_pref_file(buf);
}


/*
 * Hack - Know inventory upon death
 */
static void death_knowledge(void)
{
	int i;

	object_type *o_ptr;

	/* Hack -- Know everything in the inven/equip */
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];

		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;

		/* Aware and Known */
		object_aware(o_ptr);
		object_known(o_ptr);
	}

	p_ptr->window |= (PW_INVEN | PW_EQUIP);

	/* Hack -- Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Handle stuff */
	handle_stuff();
}




/*
 * Actually play a game.
 *
 * This function is called from a variety of entry points, since both
 * the standard "main.c" file, as well as several platform-specific
 * "main-xxx.c" files, call this function to start a new game with a
 * new savefile, start a new game with an existing savefile, or resume
 * a saved game with an existing savefile.
 *
 * If the "new_game" parameter is true, and the savefile contains a
 * living character, then that character will be killed, so that the
 * player may start a new game with that savefile.  This is only used
 * by the "-n" option in "main.c".
 *
 * If the savefile does not exist, cannot be loaded, or contains a dead
 * (non-wizard-mode) character, then a new game will be started.
 *
 * Several platforms (Windows, Macintosh) start brand new games
 * with "savefile" and "op_ptr->base_name" both empty, and initialize
 * them later based on the player name.  To prevent weirdness, we must
 * initialize "op_ptr->base_name" to "nameless" if it is empty.
 *
 * Note that we load the RNG state from savefiles and
 * so we only initialize it if we were unable to load it.  The loading
 * code marks successful loading of the RNG state using the "Rand_quick"
 * flag, which is a hack, but which optimizes loading of savefiles.
 */
void play_game(bool new_game)
{
    
	/* Hack -- Increase "icky" depth */
	character_icky++;

	/* Verify main term */
	if (!term_screen)
	{
		quit("main window does not exist");
	}

	/* Make sure main term is active */
	Term_activate(term_screen);

	/* Verify minimum size */
	if ((Term->hgt < 24) || (Term->wid < 80))
	{
		quit("main window is too small");
	}

	/* Hack -- Turn off the cursor */
	(void)Term_set_cursor(FALSE);

	character_loaded = FALSE;
	character_loaded_dead = FALSE;

	/* Attempt to load */
	if (!load_player())
	{
		/* Oops */
		character_icky--;
		return;
	////	quit("broken savefile");
	}

	/* Nothing loaded (and living) */
	if (!character_loaded)
	{
		/* Make new player */
		new_game = TRUE;

		/* The dungeon is not ready */
		character_dungeon = FALSE;
	}

	/* Hack -- Default base_name */
	if (!op_ptr->base_name[0])
	{
		my_strcpy(op_ptr->base_name, "nameless", sizeof (op_ptr->base_name));
	}

	/* Init RNG */
	if (Rand_quick)
	{
		u32b seed;

		/* Basic seed */
		seed = (time(NULL));

#ifdef SET_UID

		/* Mutate the seed on Unix machines */
		seed = ((seed >> 3) * (getpid() << 1));

#endif

		/* Use the complex RNG */
		Rand_quick = FALSE;

		/* Seed the "complex" RNG */
		Rand_state_init(seed);
	}

	/* Roll new character */
	if (new_game)
	{
		/* The dungeon is not ready */
		character_dungeon = FALSE;

		/* Start at level 1 */
		p_ptr->depth = 1;

		/* Hack -- seed for flavors */
		seed_flavor = rand_int(0x10000000);
		
		/* Hack -- seed for random artefacts */
		seed_randart = rand_int(0x10000000);

		/* Roll up a new character */
		player_birth();
		
		/* Randomize the artefacts */
		if (adult_rand_artefacts)
		{
			do_randart(seed_randart, TRUE);
		}

		// Reset the autoinscriptions
		autoinscribe_clean();
		autoinscribe_init();

		/* Hack -- enter the world */
		turn = 1;
		playerturn = 0;
		
		/* Start player on level 1 */
		p_ptr->depth = 1;
	}

	/* Normal machine (process player name) */
	if (savefile[0])
	{
		process_player_name(FALSE);
	}

	/* Weird machine (process player name, pick savefile name) */
	else
	{
		process_player_name(TRUE);
	}

	/* Flash a message */
	prt("Please wait...", 0, 0);

	/* Flush the message */
	Term_fresh();

	/* Hack -- Enter wizard mode */
	if (arg_wizard && enter_wizard_mode()) p_ptr->wizard = TRUE;

	/* Flavor the objects */
	flavor_init();

	/* Reset visuals */
	reset_visuals(TRUE);

	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

	/* Window stuff */
	p_ptr->window |= (PW_MONSTER | PW_MESSAGE);

	/* Window stuff */
	window_stuff();

	/* Process some user pref files */
	process_some_user_pref_files();

	/* Set or clear "hjkl_movement" if requested */
	if (arg_force_original) hjkl_movement = FALSE;
	if (arg_force_roguelike) hjkl_movement = TRUE;

	/* React to changes */
	Term_xtra(TERM_XTRA_REACT, 0);

	/* Generate a dungeon level if needed */
	if (!character_dungeon) generate_cave();

	/* Character is now "complete" */
	character_generated = TRUE;

	/* Start with normal object generation mode */
	object_generation_mode = OB_GEN_MODE_NORMAL;

	/* Hack -- Decrease "icky" depth */
	character_icky--;

	/* Start playing */
	p_ptr->playing = TRUE;

	/* Hack -- Enforce "delayed death" */
	if (p_ptr->chp <= 0) p_ptr->is_dead = TRUE;

	/* Redraw everything */
	// Sil-y: added to get 'shades' right in extra inventory terms
	do_cmd_redraw();
    
    // update player noise
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);

	// reset combat roll info
	turns_since_combat = 0;
    
    // assume the player is on the ground
    p_ptr->leaping = FALSE;
	
	/* Process */
	while (TRUE)
	{		
		/* Process the level */
		dungeon();

		/* Notice stuff */
		if (p_ptr->notice) notice_stuff();

		/* Update stuff */
		if (p_ptr->update) update_stuff();

		/* Redraw stuff */
		if (p_ptr->redraw) redraw_stuff();

		/* Window stuff */
		if (p_ptr->window) window_stuff();

		/* Cancel the target */
		target_set_monster(0);

		/* Cancel the health bar */
		health_track(0);

		/* Forget the view */
		forget_view();

		/* Handle "quit and save" */
		if (!p_ptr->playing && !p_ptr->is_dead) break;

		/* Erase the old cave */
		/* If the character is dead, then we don't erase yet */
		if (!p_ptr->is_dead) 
		{
			wipe_o_list();
			wipe_mon_list();
		}

		/* XXX XXX XXX */
		message_flush();

		/* Accidental Death */
		if (p_ptr->playing && p_ptr->is_dead)
		{
			/* Mega-Hack -- Allow player to cheat death */
			if ((p_ptr->wizard || (p_ptr->noscore & 0x0008) || cheat_live) && !get_check("Die? "))
			{
				/* Mark savefile */
				p_ptr->noscore |= 0x0001;

				/* Message */
				msg_print("You invoke wizard mode and cheat death.");
				message_flush();

				/* Cheat death */
				p_ptr->is_dead = FALSE;

				/* Restore hit points */
				p_ptr->chp = p_ptr->mhp;
				p_ptr->chp_frac = 0;

				/* Restore voice */
				p_ptr->csp = p_ptr->msp;
				p_ptr->csp_frac = 0;

				/* Hack -- Healing */
				(void)set_blind(0);
				(void)set_confused(0);
				(void)set_poisoned(0);
				(void)set_afraid(0);
				(void)set_entranced(0);
				(void)set_image(0);
				(void)set_stun(0);
				(void)set_cut(0);
                (void)res_stat(A_STR, 20);
                (void)res_stat(A_CON, 20);
                (void)res_stat(A_DEX, 20);
                (void)res_stat(A_GRA, 20);

				/* Hack -- Prevent starvation */
				(void)set_food(PY_FOOD_FULL - 1);

				/* Hack -- cancel recall */
				if (p_ptr->word_recall)
				{
					/* Message */
					msg_print("A tension leaves the air around you...");
					message_flush();

					/* Hack -- Prevent recall */
					p_ptr->word_recall = 0;
				}

				/* Note cause of death XXX XXX XXX */
				my_strcpy(p_ptr->died_from, "Cheating death", sizeof (p_ptr->died_from));

				/* Need to generate a new level */
				p_ptr->leaving = TRUE;
			}
		}

		/* Take a mini screenshot for dead characters */
		if (p_ptr->is_dead)
		{
			death_knowledge();

			do_cmd_wiz_unhide(255);
			update_view();
			mini_screenshot();
			detect_all_doors_traps();
		}

		/* Handle "death" */
		if (p_ptr->is_dead) break;

		/* Make a new level */
		generate_cave();

	}

	/* Close stuff */
	close_game();
}

