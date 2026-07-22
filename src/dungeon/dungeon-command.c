/* File: dungeon/dungeon-command.c */

#include "angband.h"
#include "dungeon-internal.h"

/*
 * Verify use of "wizard" mode
 */
bool enter_wizard_mode(void)
{
    /* Ask first time - unless resurrecting a dead character */
    if (!(p_ptr->noscore & 0x0008) && !p_ptr->is_dead)
    {
        /* Explanation */
        msg_print("You can only enter wizard mode from within debug mode.");
        log_debug("Wizard mode denied - must be in debug mode first");

        return (false);
    }

    /* Mark savefile */
    p_ptr->noscore |= 0x0002;

    log_info("Entering wizard mode - savefile marked (noscore=0x%04X, savefile='%s')",
             (unsigned)p_ptr->noscore, savefile);

    /* Success */
    return (true);
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
        msg_print(
            "You are about to use the dangerous, unsupported, debug commands!");
        msg_print(
            "Your machine may crash, and your savefile may become corrupted!");
        message_flush();

        /* Verify request */
        if (!get_check("Are you sure you want to use the debug commands? "))
        {
            return (false);
        }

        // ask for password in deployment versions
        if (DEPLOYMENT)
        {
            if (term_get_string("Password: ", buf, sizeof(buf)))
            {
                if (strcmp(buf, "Gondolin") == 0)
                {
                    /* Mark savefile */
                    p_ptr->noscore |= 0x0008;

                    /* Okay */
                    return (true);
                }
            }

            msg_print("Incorrect password.");
            return (false);
        }
    }

    /* Mark savefile */
    p_ptr->noscore |= 0x0008;

    log_info("Debug mode enabled (noscore=0x%04X, savefile='%s')",
             (unsigned)p_ptr->noscore, savefile);

    /* Okay */
    return (true);
}

#endif /* ALLOW_DEBUG */

/*
 * Restore the gameplay map after commands that use cursor-style selection.
 *
 * The shared aiming UI stores an empty-grid choice as a location target so
 * direction 5 can deliver it to the command.  Unlike a monster target, that
 * location never invalidates on its own.  Keeping it after the command makes
 * normal input place the cursor on that grid indefinitely.  Also flush both
 * terminal cursor layers here: inkey() can restore the hidden logical state
 * without repainting the cell where its visible cursor was last drawn, and a
 * menu can use the separate extra cursor without moving the real one.
 */
static void finish_command_cursor_state(void)
{
    if (p_ptr->target_set && (p_ptr->target_who == 0))
    {
        target_set_monster(0);
        health_track(0);
    }

    (void)Term_set_extra_cursor(false, 0, 0, false);
    (void)Term_set_cursor(false);
    Term_fresh();
}

/*
 * Parse and execute the current command
 * Give "Warning" on illegal commands.
 */
void process_command(void)
{
    log_trace("process_command: character_icky=%d, command='%c' (%d)",
              character_icky, p_ptr->command_cmd, (int)p_ptr->command_cmd);

    /* Debug: Log character_icky state but don't aggressively reset it during normal operation */
    if (character_icky > 0) {
        log_debug("process_command: character_icky is %d (may be normal during menu operations)", character_icky);
    }

#ifdef ALLOW_REPEAT

    /* Handle repeating the last command */
    repeat_check();

#endif /* ALLOW_REPEAT */

    /* Disallow actions that would advance time while viewing the final map. */
    if (death_spectator_mode
        && !death_spectator_command_allowed(p_ptr->command_cmd))
    {
        if (p_ptr->command_cmd)
        {
            msg_print("You cannot do that during this final look.");
        }
        /* Pointer movement can queue a whole path.  Rejecting only its first
         * generated step would otherwise immediately feed the next step back
         * into request_command() and spam the read-only message. */
        sdl_mouse_path_cancel();
        p_ptr->energy_use = 0;
        p_ptr->command_cmd = 0;
        finish_command_cursor_state();
        return;
    }

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
            p_ptr->wizard = false;
            msg_print("Wizard mode off.");
            p_ptr->update |= (PU_BONUS);
        }
        else if (enter_wizard_mode())
        {
            p_ptr->wizard = true;
            msg_print("Wizard mode on.");
            p_ptr->update |= (PU_BONUS);
        }

        /* Update monsters */
        p_ptr->update |= (PU_MONSTERS);

        break;
    }

#ifdef ALLOW_DEBUG

    /* Special "debug" commands */
    case KTRL('Y'):
    {
        if (verify_debug_mode())
        {
            log_info("Ctrl-Y debug menu opened (wizard=%d, noscore=0x%04X, savefile='%s')",
                     p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore, savefile);
            do_cmd_debug();
        }
        break;
    }

#endif

    /*** Inventory Commands ***/

    /* Wear/wield equipment */
    case 'w':
    {
        do_cmd_wield_wrapper();
        break;
    }

    /* Equipped browser / remove equipment */
    case 'r':
    {
        do_cmd_equip_direct();
        break;
    }

    /* Equipment list */
    case 'e':
    {
        do_cmd_equip_direct();
        break;
    }

    /* Inventory list */
    case 'i':
    {
        do_cmd_inven_direct();
        break;
    }

    /* Sing */
    case 's':
    {
        do_cmd_change_song();
        break;
    }

    /* Internal pointer/touch active weapon selector */
    case CMD_ACTIVE_WEAPON_MODE:
    {
        do_cmd_pending_active_weapon_mode();
        break;
    }

    /* Change active weapon */
    case '\t':
    {
        do_cmd_toggle_active_weapon();
        break;
    }

    /* Ability screen */
    case 'y':
    {
        do_cmd_ability_screen();
        
        /* Force full redraw after screen_load() restored old content */
        p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EXP);
        handle_stuff();
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

    /*** Stairs and Doors and Chests and Traps ***/

    /* Go up staircase */
    case '<':
    {
        // Autosave
        save_game_quietly = true;
        do_cmd_save_game();

        do_cmd_go_up();
        break;
    }

    /* Go down staircase */
    case '>':
    {
        // Autosave
        save_game_quietly = true;
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

    case '-':
    {
        do_cmd_fletchery();
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

    /* Swap the equipped staff with one from the pack */
    case KTRL('A'):
    {
        do_cmd_swap_staff();
        break;
    }

    /* Eat some food */
    case 'E':
    {
        do_cmd_eat_food(NULL, 0);
        break;
    }

    /* Swap the 1st and 2nd quivers */
    case KTRL('F'):
    {
        do_cmd_swap_quivers();
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
        do_cmd_throw(false);
        break;
    }

        /* Throw an automatically chosen item at nearest target */
    case KTRL('T'):
    {
        do_cmd_throw(true);
        break;
    }

    /* Play an instrument */
    case 'p':
    {
        do_cmd_play_instrument(NULL, 0);
        break;
    }

    /* Use a supply item */
    case 'q':
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE,
            SUPPLY_GROUP_POTIONS, true, true);
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
    // case '*':
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

    /* Character sheet (alternative key) */
    case 'h':
    {
        do_cmd_character_sheet();
        break;
    }
    
    /* Direct access to skill distribution */
    case 'H':
    {
        /* Save screen */
        screen_save();
        
        /* Open skill distribution directly */
        gain_skills();
        
        /* Load screen */
        screen_load();
        
        /* Force full redraw after screen_load() restored old content */
        p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EXP);
        handle_stuff();
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
        do_cmd_main_menu();
        break;
    }

    /* Identify symbol */
    // case '/':
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

    /* Interact with visuals */
    // case '%':
    //{
    //	do_cmd_visuals();
    //	break;
    //}

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
        do_cmd_note("", p_ptr->depth);
        break;
    }

    /* Show previous messages */
    case KTRL('P'):
    {
        do_cmd_messages();
        break;
    }

    /* Show combat rolls */
    case KTRL('Q'):
    {
        do_cmd_combat_history();
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
        p_ptr->playing = false;

        /* Leaving */
        p_ptr->leaving = true;
        break;
    }

    /* Supplies overview */
    case 'j':
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_NONE,
            SUPPLY_GROUP_SUPPLY, true, false);
        break;
    }

    /* Jewelry preset shortcut */
    case 'J':
    {
        do_cmd_jewelry_preset_shortcut();
        break;
    }

    /* Check knowledge */
    case '~':
    {
        do_cmd_knowledge();
        break;
    }

    case '[':
    {
        do_cmd_view_monsters();
        break;
    }

    case ']':
    {
        do_cmd_view_objects();
        break;
    }

    /* Hack -- Unknown command */
    default:
    {
        msg_print("Type '?' for help.");
        break;
    }
    }

    finish_command_cursor_state();
}
