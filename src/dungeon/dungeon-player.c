/* File: dungeon/dungeon-player.c */

#include "angband.h"
#include "dungeon-internal.h"

static bool auto_pickup_okay(const object_type* o_ptr)
{
    int max_qty;

    /* It can't be carried */
    if (!inven_carry_okay(o_ptr))
        return (false);

    /*
     * Don't interrupt movement with a quantity prompt when a supply stack
     * only fits partially. The player can still pick it up manually.
     */
    if (supplies_is_supply_object(o_ptr) && o_ptr->number > 1)
    {
        max_qty = supplies_max_absorbable_quantity(o_ptr);
        if ((max_qty > 0) && (max_qty < o_ptr->number))
            return (false);
    }

    /* object has pickup flag set */
    if (o_ptr->pickup)
        return (true);

    /* Don't auto pickup */
    return (false);
}

/*
 * Finish your leap
 */
void land(void)
{
    // the player has landed
    p_ptr->leaping = false;

    // make some noise when landing
    stealth_score -= 5;

    /* Set off traps */
    if (cave_trap_bold(p_ptr->py, p_ptr->px)
        || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM))
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
    int y_end, x_end; // the desired endpoint of the leap

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
        monster_type* m_ptr = &mon_list[cave_m_idx[y_end][x_end]];
        char m_name[80];

        /* Get the monster name */
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        if (m_ptr->ml)
            msg_format("%^s blocks your landing.", m_name);
        else
            msg_format("Some unseen foe blocks your landing.", m_name);
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
void process_player_aux(void)
{
    int i;
    bool changed = false;

    static int old_monster_race_idx = 0;

    static u32b old_flags1 = 0L;
    static u32b old_flags2 = 0L;
    static u32b old_flags3 = 0L;
    static u32b old_flags4 = 0L;

    static byte old_blows[MONSTER_BLOW_MAX];

    static byte old_ranged = 0;

    /* Tracking a monster */
    if (p_ptr->monster_race_idx)
    {
        /* Get the monster lore */
        monster_lore* l_ptr = &l_list[p_ptr->monster_race_idx];

        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            if (old_blows[i] != l_ptr->blows[i])
            {
                changed = true;
                break;
            }
        }

        /* Check for change of any kind */
        if (changed || (old_monster_race_idx != p_ptr->monster_race_idx)
            || (old_flags1 != l_ptr->flags1) || (old_flags2 != l_ptr->flags2)
            || (old_flags3 != l_ptr->flags3) || (old_flags4 != l_ptr->flags4)
            || (old_ranged != l_ptr->ranged))

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
void process_player(void)
{
    int i;
    int amount;
    int regen_multiplier;
    int depth_counter_increment;

    // reset the number of times you have riposted since last turn
    p_ptr->ripostes = 0;

    // reset whether you have just woken up from entrancement
    p_ptr->was_entranced = false;

    // update the player's torch radius
    calc_torch();

    song_disguise_new_player_turn();
    song_duels_new_player_turn();

    /*** Check certain things between player turns (don't need to do this when
     * restoring a game) ***/

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
                if ((p_ptr->chp == p_ptr->mhp) && (p_ptr->csp == p_ptr->msp))
                {
                    disturb(0, 0);
                }
            }

            /* Complete resting */
            else if (p_ptr->resting == -2)
            {
                /* Stop resting */
                if ((p_ptr->chp == p_ptr->mhp)
                    && ((p_ptr->csp == p_ptr->msp) || !singing(SNG_NOTHING))
                    && !p_ptr->blind && !p_ptr->confused && !p_ptr->poisoned
                    && !p_ptr->afraid && !p_ptr->stun && !p_ptr->cut
                    && !p_ptr->slow && !p_ptr->entranced)
                {
                    disturb(0, 0);
                }
            }
        }

        /* Check for "player abort" */
        if (p_ptr->running || p_ptr->fletching || p_ptr->smithing
            || p_ptr->command_rep || (p_ptr->resting && !(turn & 0x7F)))
        {
            /* Do not wait */
            inkey_scan = true;

            /* Check for a key */
            if (inkey())
            {
                /* Flush input */
                flush();

                /* Disturb */
                disturb(0, 0);

                /* Hack -- Show a Message */
                msg_print("Cancelled.");
            }
        }

        /*** Other checks ***/

        do_betrayal_ring_amulet();

        // Make the stealth-modified noise (has to occur after monsters have had
        // a chance to move)
        monster_perception(true, true, stealth_score);

        // Stop stealth mode if something happened
        if (stop_stealth_mode)
        {
            /* Cancel */
            p_ptr->stealth_mode = false;

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS);

            /* Redraw the state */
            p_ptr->redraw |= (PR_STATE);
            if (pixel_monster_status_icons)
                p_ptr->redraw |= (PR_MAP);

            // Reset the flag
            stop_stealth_mode = false;
        }

        // Morgoth will announce a challenge if adjacent
        if (p_ptr->truce && (p_ptr->depth == MORGOTH_DEPTH))
        {
            int d, yy, xx;

            /* Check around the character */
            for (d = 0; d < 8; d++)
            {
                monster_type* m_ptr;

                /* Extract adjacent (legal) location */
                yy = p_ptr->py + ddy_ddd[d];
                xx = p_ptr->px + ddx_ddd[d];

                // paranoia
                if (cave_m_idx[yy][xx] < 0)
                    continue;

                m_ptr = &mon_list[cave_m_idx[yy][xx]];

                if ((m_ptr->r_idx == R_IDX_MORGOTH)
                    && (m_ptr->alertness >= ALERTNESS_ALERT))
                {
                    msg_print("With a voice as of rolling thunder, Morgoth, "
                              "Lord of Darkness, "
                              "speaks:");
                    msg_print("'You dare challenge me in mine own hall? Now is "
                              "your death upon "
                              "you!'");

                    // Break the truce (always)
                    break_truce(true);
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
                    do_cmd_note(
                        format("Challenge: %s", option_desc[option_number]),
                        p_ptr->depth);
                }
            }
        }

        if (p_ptr->previous_action[0] != ACTION_ARCHERY)
        {
            p_ptr->killed_enemy_with_arrow = false;
            p_ptr->redraw |= PR_ARC;
        }

        // shuffle along the array of previous actions
        for (i = ACTION_MAX - 1; i > 0; i--)
        {
            p_ptr->previous_action[i] = p_ptr->previous_action[i - 1];
        }
        // put in a default for this turn
        // Sil-y: it is possible that this isn't always changed to something
        // else, but I think it is
        p_ptr->previous_action[0] = ACTION_NOTHING;

        /* Redraw stuff (if needed) */
        if (p_ptr->window)
            window_stuff();

        // Sil-y: have to update the player bonuses at every turn with
        // sprinting, dodging etc.
        //        this might cause annoying slowdowns, I'm not sure
        p_ptr->update |= (PU_BONUS);
    }

    /*** Handle actual user input ***/

    /* Repeat until energy is reduced */
    do
    {
        /* Notice stuff (if needed) */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff (if needed) */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff (if needed) */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Redraw stuff (if needed) */
        if (p_ptr->window)
            window_stuff();

        /* Place the cursor on the player or target */
        if (hilite_player)
            move_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted()
            && panel_contains(p_ptr->target_row, p_ptr->target_col))
            move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

        if (cheat_noise)
            display_noise_map();
        else if (cheat_scent)
            display_scent_map();
        else if (cheat_light)
            display_light_map();

        /* Refresh */
        Term_fresh();

        /* Hack -- Pack Overflow if needed */
        check_pack_overflow();

        if (cave_o_idx[p_ptr->py][p_ptr->px] != 0)
        {
            (&o_list[cave_o_idx[p_ptr->py][p_ptr->px]])->marked = true;
        }

        /* Hack -- cancel "lurking browse mode" */
        if (!p_ptr->command_new)
            p_ptr->command_see = false;

        /* Assume free turn */
        p_ptr->energy_use = 0;

    // Reset number of attacks this turn happens at start of player energy loop

        // get base stealth score for the round
        // this will get modified by the type of action
        stealth_score = p_ptr->skill_use[S_STL];

        // display a note at the start of the game
        if ((cave_o_idx[p_ptr->py][p_ptr->px] != 0))
        {
            object_type* o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
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

                /* Aulë quest: check for success condition during forging */
                {
                    int diff = object_difficulty(smith_o_ptr);
                    p_ptr->aule_last_object_diff = diff;
                    if (diff > 20 && p_ptr->aule_quest == AULE_QUEST_ACTIVE) {
                        p_ptr->aule_quest = AULE_QUEST_SUCCESS;
                        log_trace("Aulë quest: state -> SUCCESS (diff=%d)", diff);
                        msg_print("Your forging radiates unparalleled craft!");
                        msg_print("You sense that Aulë would be pleased with this work...");
                        msg_print("Seek out Aulë to receive his blessing.");
                    }
                }
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
    /* Aulë quest: no longer requires standing at special forge; acceptance handled during forging */

        /* Fletching */
        else if (p_ptr->fletching)
        {
            if (p_ptr->fletching == 1)
            {
                // Display a message
                msg_print("You complete your work.");

                finish_fletching(0);
            }

            /* Reduce fletching count */
            p_ptr->fletching--;

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
            p_ptr->focused = true;

            /* Searching */
            search();
        }

        /* Recovering footing */
        else if (p_ptr->skip_next_turn)
        {
            // let the player know
            if (p_ptr->knocked_back)
            {
                msg_print("You recover your footing.");

                // force a -more-
                message_flush();
                p_ptr->knocked_back = false;
            }

            // reset flag
            p_ptr->skip_next_turn = false;

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;

            // Pause to show enemies moving.
            Term_xtra(TERM_XTRA_DELAY, 500);
        }

        /* Running */
        else if (p_ptr->running)
        {
            /* Take a step */
            run_step(0);

            /* Pace running so movement remains readable at the chosen speed. */
            if (running_step_delay_ms > 0)
                Term_xtra(TERM_XTRA_DELAY, running_step_delay_ms);
        }

        /* Repeated command */
        else if (p_ptr->command_rep)
        {
            /* Hack -- Assume messages were seen */
            msg_flag = false;

            /* Clear the top line */
            if (ui_message_line_enabled())
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
            object_type* o_ptr;

            // build an object description
            if (cave_o_idx[p_ptr->py][p_ptr->px])
            {
                o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

                /* Describe the object */
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                strnfmt(out_val, sizeof(out_val), "Pick up %s? ", o_name);
            }

            // always offer to pickup if the mode is on, there is an object
            // present, and you have just moved
            if (always_pickup && cave_o_idx[p_ptr->py][p_ptr->px]
                && (o_ptr->tval != TV_NOTE) && (p_ptr->previous_action[1] >= 1)
                && (p_ptr->previous_action[1] <= 9)
                && (p_ptr->previous_action[1] != 5))
            {
                // allow the player to decline to pick up the object
                if (get_check_near(p_ptr->py, p_ptr->px, out_val))
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
                if (hilite_player)
                    move_cursor_relative(p_ptr->py, p_ptr->px);
                if (hilite_target && target_sighted()
                    && panel_contains(p_ptr->target_row, p_ptr->target_col))
                    move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

                /* We are certainly no longer in the process of restoring a game
                 */
                p_ptr->restoring = false;

                /* Get a command (normal) */
                TIME_PHASE("request_command", request_command());
                if (p_ptr->leaving)
                {
                    log_debug("process_player: leaving set while waiting for command; command=%d playing=%d",
                        (int)p_ptr->command_cmd, p_ptr->playing ? 1 : 0);
                    break;
                }

                /* Process the command */
                TIME_PHASE("process_command", process_command());
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

        /* Update labyrinth map restriction and partition-entry messages/XP. */
        update_labyrinth_view_state(true);
        handle_partition_entry(false, op_ptr->partition_narrative_mode);

        bool in_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH)
            && (cave_info[p_ptr->py][p_ptr->px] & (CAVE_G_VAULT));

        /* Check for greater vault squares */
        if ((cave_info[p_ptr->py][p_ptr->px] & (CAVE_G_VAULT))
            && (g_vault_name[0] != '\0') && !was_in_morgoth_vault)
        {
            bool clear_vault_name = true;

            if (strcmp(greater_vault_xp_name, g_vault_name) != 0)
            {
                SDL_strlcpy(greater_vault_xp_name, g_vault_name, sizeof(greater_vault_xp_name));
                greater_vault_xp_awarded = false;
            }

            if (in_morgoth_vault)
            {
                bool allow_entry = morgoth_entry_preconfirmed;
                if (!allow_entry)
                    allow_entry = confirm_enter_morgoth_hall();

                if (!allow_entry)
                {
                    clear_vault_name = false;

                    if (restore_player_position_after_denied_move(last_player_y, last_player_x))
                    {
                        in_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH)
                            && (cave_info[p_ptr->py][p_ptr->px] & (CAVE_G_VAULT));
                    }
                }
                else
                {
                    const int vault_xp = 500;
                    char note[120];
                    strnfmt(note, sizeof(note), "Entered %s", g_vault_name);
                    do_cmd_note(note, p_ptr->depth);

                    p_ptr->morgoth_hall_entered = true;

                    describe_greater_vault_entry(g_vault_name);
                    msg_print("From within you hear the harsh din of feasting in Morgoth's own hall.");
                    if (!greater_vault_xp_awarded)
                    {
                        gain_exp(vault_xp);
                        greater_vault_xp_awarded = true;
                    }

                    pause_with_text(throne_poetry, 5, 13, NULL, 0);
                    p_ptr->truce = true;
                    msg_print("There is a strange tension in the air.");
                    if (p_ptr->skill_use[S_PER] >= 15)
                        msg_print("You feel that Morgoth's servants are reluctant to attack before he delivers judgment.");
                }
            }
            else
            {
                const int vault_xp = 500;
                char note[120];
                strnfmt(note, sizeof(note), "Entered %s", g_vault_name);

                do_cmd_note(note, p_ptr->depth);
                describe_greater_vault_entry(g_vault_name);
                if (!greater_vault_xp_awarded)
                {
                    gain_exp(vault_xp);
                    greater_vault_xp_awarded = true;
                }
            }

            if (clear_vault_name)
            {
                g_vault_name[0] = '\0';
                greater_vault_xp_name[0] = '\0';
                greater_vault_xp_awarded = false;
            }
        }

        in_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH)
            && (cave_info[p_ptr->py][p_ptr->px] & (CAVE_G_VAULT));

        if (p_ptr->morgoth_hall_entered && was_in_morgoth_vault && !in_morgoth_vault
            && (silmarils_possessed() == 0))
        {
            msg_print("The Shadow bars your way: you cannot flee without a Silmaril.");

            if (restore_player_position_after_denied_move(last_player_y, last_player_x))
            {
                in_morgoth_vault = true;
            }
        }

        if (was_in_morgoth_vault && !in_morgoth_vault && p_ptr->truce)
        {
            break_truce(true);
        }

        was_in_morgoth_vault = in_morgoth_vault;
        last_player_y = p_ptr->py;
        last_player_x = p_ptr->px;
        morgoth_entry_preconfirmed = false;

        /* Significant */
        if (p_ptr->energy_use)
        {
            /* Use some energy */
            p_ptr->energy -= p_ptr->energy_use;

            /* Hack -- constant hallucination */
            if (p_ptr->image)
                p_ptr->redraw |= (PR_MAP);

            /* Shimmer monsters if needed */
            if (shimmer_monsters)
            {
                /* Clear the flag */
                shimmer_monsters = false;

                /* Shimmer multi-hued monsters */
                for (i = 1; i < mon_max; i++)
                {
                    monster_type* m_ptr;
                    monster_race* r_ptr;

                    /* Get the monster */
                    m_ptr = &mon_list[i];

                    /* Skip dead monsters */
                    if (!m_ptr->r_idx)
                        continue;

                    /* Get the monster race */
                    r_ptr = &r_info[m_ptr->r_idx];

                    /* Skip non-multi-hued monsters */
                    if (!(r_ptr->flags1 & (RF1_ATTR_MULTI)))
                        continue;

                    /* Reset the flag */
                    shimmer_monsters = true;

                    /* Redraw regardless */
                    lite_spot(m_ptr->fy, m_ptr->fx);
                }
            }

            /* Repair "mark" flags */
            if (repair_mflag_mark)
            {
                /* Reset the flag */
                repair_mflag_mark = false;

                /* Process the monsters */
                for (i = 1; i < mon_max; i++)
                {
                    monster_type* m_ptr;

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
                            repair_mflag_mark = true;

                            /* Skip */
                            continue;
                        }

                        /* Forget flag */
                        m_ptr->mflag &= ~(MFLAG_MARK);

                        /* Update the monster */
                        update_mon(i, false);
                    }
                }
            }
        }

        /* Repair "show" flags */
        if (repair_mflag_show)
        {
            /* Reset the flag */
            repair_mflag_show = false;

            /* Process the monsters */
            for (i = 1; i < mon_max; i++)
            {
                monster_type* m_ptr;

                /* Get the monster */
                m_ptr = &mon_list[i];

                /* Skip dead monsters */
                /* if (!m_ptr->r_idx) continue; */

                /* Clear "show" flag */
                m_ptr->mflag &= ~(MFLAG_SHOW);
            }
        }
    } while (!p_ptr->energy_use && !p_ptr->leaving);

    // if the player is exiting the the game in some manner then stop processing
    // now
    if (p_ptr->leaving)
        return;

    /* Do song effects */
    sing();

    // make less noise if you did nothing at all
    // (+7 in total whether or not stealth mode is used)
    if (p_ptr->resting)
    {
        if (p_ptr->stealth_mode)
            stealth_score += 2;
        else
            stealth_score += 7;
    }

    // make much more noise when smithing
    if (p_ptr->smithing)
    {
        /* Make a lot of noise */
        monster_perception(true, false, -10);
    }

    // update player noise
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);

    /* Update scent trail */
    update_smell();

    /* possibly identify passive abilities every so often*/
    if (one_in_(100))
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

        killer_mark_other(SCORE_KILLER_OTHER);
        take_hit(amount, "poison");
    }

    /* Take damage from cuts */
    if (p_ptr->cut)
    {
        amount = (p_ptr->cut + 4) / 5;

        /* Take damage */
        killer_mark_other(SCORE_KILLER_OTHER);
        take_hit(amount, "a fatal wound");
    }

    /*** Check the Food, and Regenerate ***/

    /* Basic digestion rate */
    i = 1;

    // Note: speed and regeneration are taken into account already in the hunger
    // rate

    // Hack: slow hunger rates are done statistically
    if (p_ptr->hunger < 0)
    {
        if (!one_in_(int_exp(3, -(p_ptr->hunger))))
        {
            i = 0;
        }
    }
    else if (p_ptr->hunger > 0)
    {
        i *= int_exp(3, p_ptr->hunger);
    }

    /* Digest very quickly when gorged */
    if (p_ptr->food >= PY_FOOD_MAX)
        i *= 50;

    /* CUR_HUNGER increases p_ptr->hunger modifier (applied in calc_bonuses) */
    /* This is now handled via p_ptr->hunger in calc_bonuses() */
    /* Each stack adds +1 to hunger rate, giving 3x, 9x, 27x scaling */

    /* Digest some food */
    (void)set_food(p_ptr->food - i);

    /* Starve to death (slowly) */
    if (p_ptr->food < PY_FOOD_STARVE)
    {
        /* Calculate damage */
        i = 1; // old: (PY_FOOD_STARVE - p_ptr->food) / 10;

        /* Take damage */
        killer_mark_other(SCORE_KILLER_OTHER);
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

    /* Regeneration ability */
    regen_multiplier = p_ptr->regenerate + 1;

    /* Regenerate the mana */
    if (p_ptr->csp < p_ptr->msp)
    {
        regenmana(regen_multiplier);
    }

    /* Various things interfere with healing */
    if (p_ptr->food < PY_FOOD_STARVE)
        regen_multiplier = 0;
    if (p_ptr->poisoned)
        regen_multiplier = 0;
    if (p_ptr->cut)
        regen_multiplier = 0;

    /* Regenerate Hit Points if needed */
    if (p_ptr->chp < p_ptr->mhp)
    {
        regenhp(regen_multiplier);
    }

    /*** Timeout Various Things ***/

    amount = 1;

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
        if (singing(SNG_FREEDOM))
            (void)set_slow(p_ptr->slow - ability_bonus(S_SNG, SNG_FREEDOM));
        else
            (void)set_slow(p_ptr->slow - 1);
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

    /* Temporary Perception */
    if (p_ptr->tmp_per)
    {
        (void)set_tmp_per(p_ptr->tmp_per - 1);
    }

    /* Song of Challenge lingering effect */
    if (p_ptr->song_challenge_effect)
    {
        p_ptr->song_challenge_effect -= 1;
    }

    /* Song of Elbereth lingering effect */
    if (p_ptr->song_elbereth_effect)
    {
        p_ptr->song_elbereth_effect -= 1;
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
        p_ptr->focused = false;
    }

    // if the player didn't attack or 'pass' then the consecutive attacks needs
    // to be reset
    if (!player_attacked && (p_ptr->previous_action[0] != 5))
    {
        p_ptr->consecutive_attacks = 0;
        p_ptr->last_attack_m_idx = 0;
    }

    // boots of radiance
    if (inventory[INVEN_FEET].k_idx)
    {
        u32b f1, f2, f3;
        object_type* o_ptr = &inventory[INVEN_FEET];

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

                    object_desc(
                        o_short_name, sizeof(o_short_name), o_ptr, false, 0);
                    object_aware(o_ptr);
                    object_known(o_ptr);
                    object_desc(
                        o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                    msg_print("Your footsteps leave a trail of light!");
                    msg_format("You recognize your %s to be %s", o_short_name,
                        o_full_name);
                }

                cave_info[p_ptr->py][p_ptr->px] |= CAVE_GLOW;
            }
        }
    }

    playerturn++;

    /* Count down active narrative banners by full player turns.
       0-turn banners are dismissed in request_command() before any action. */
    if (g_banner_force_redraw_remaining > 0)
    {
        if (g_active_partition_banner_skip_next_decay)
        {
            g_active_partition_banner_skip_next_decay = false;
        }
        else
        {
            g_banner_force_redraw_remaining--;
        }
        if (g_banner_force_redraw_remaining == 0)
        {
            g_active_partition_banner_consumes_input = false;
            g_active_partition_banner_text[0] = '\0';
            do_cmd_redraw();
        }
    }

    min_depth_timer_status(NULL, NULL, &depth_counter_increment, NULL, NULL);

    min_depth_counter += depth_counter_increment > 0 ?
        depth_counter_increment : 0;

    process_morgoth_call_pressure();

    /* Window stuff */

    // Sil-y: note that these are now being set every single turn, somewhat
    // defeating their purpose
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
    
    /*
     * Do NOT set PW_COMBAT_ROLLS unconditionally here - it should only be
     * set when combat data actually changes (via update_combat_rolls functions).
     * Setting it every turn causes the combat roll subwindow to refresh with
     * stale data before new combat happens, creating a one-turn delay.
     * 
     * Also, do NOT refresh the main-terminal combat rolls here.
     * We refresh them after monster processing in the main loop so that
     * both sides of the current round (player and monsters) are included.
     */
}
