/* File: dungeon/dungeon-loop.c */

#include "angband.h"
#include "dungeon-internal.h"

/*
 * Interact with the current dungeon level.
 *
 * This function will not exit until the level is completed,
 * the user dies, or the game is terminated.
 */
void dungeon(void)
{
    monster_type* m_ptr;
    int i;

    log_debug("Entering dungeon level %d", p_ptr->depth);

    /* Play level transition sound (but not on first entry) */
    if (!first_entry_to_dungeon) {
        sound(MSG_LEVEL);
    }
    first_entry_to_dungeon = false;
    
    /* Depth 0 (the Gates) is still active gameplay and should use ambient music. */
    bool was_in_dungeon = (last_music_depth >= 0);
    bool now_in_dungeon = (p_ptr->depth >= 0);
    
    if (now_in_dungeon && !was_in_dungeon) {
        /* Entering dungeon from surface - switch to ambient */
        log_debug("Switching to ambient music (entering dungeon)");
        sdl_music_stop_main();
        sdl_music_play_ambient();
    } else if (!now_in_dungeon && was_in_dungeon) {
        /* Leaving dungeon gameplay - keep ambient running, clear any overlay. */
        log_debug("Leaving dungeon gameplay - preserving ambient music");
        sdl_music_stop_main();
        sdl_music_play_ambient();
    }
    
    last_music_depth = p_ptr->depth;

    /* Hack -- enforce illegal panel */
    p_ptr->wy = p_ptr->cur_map_hgt;
    p_ptr->wx = p_ptr->cur_map_wid;

    /* Not leaving */
    p_ptr->leaving = false;

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
    shimmer_monsters = true;
    shimmer_objects = true;

    /* Reset repair flags */
    repair_mflag_show = true;
    repair_mflag_mark = true;

    /* Disturb */
    disturb(0, 0);

    /* Track maximum dungeon level */
    if (p_ptr->max_depth < p_ptr->depth)
    {
        log_info("Player reached new maximum depth: %d", p_ptr->depth);
        for (i = p_ptr->max_depth + 1; i <= p_ptr->depth; i++)
        {
            if (i > 1)
            {
                int new_exp = i * 50;
                gain_exp(new_exp);
                p_ptr->descent_exp += new_exp;

                log_debug("Depth %d reached, gained %d descent experience", i, new_exp);

                // Sil-x
                // do_cmd_note(format("exp:%d = s:5000 + e:%d + k:%d + d:%d +
                // i:%d",
                //		    p_ptr->exp, p_ptr->encounter_exp,
                // p_ptr->kill_exp, p_ptr->descent_exp, p_ptr->ident_exp), i);
            }
        }
        p_ptr->max_depth = p_ptr->depth;
    }

    /* No stairs from the surface */
    if (!p_ptr->depth)
    {
        p_ptr->create_stair = false;
    }

    /* Make a staircase */
    if (p_ptr->create_stair)
    {
        log_debug("Creating staircase at player position");
        /* Place a staircase */
        if (cave_valid_bold(p_ptr->py, p_ptr->px))
        {
            /* XXX XXX XXX */
            delete_object(p_ptr->py, p_ptr->px);

            cave_set_feat(p_ptr->py, p_ptr->px, p_ptr->create_stair);

            /* Mark the stairs as known */
            cave_info[p_ptr->py][p_ptr->px] |= (CAVE_MARK);

            log_trace("Staircase created and marked at (%d, %d)", p_ptr->py, p_ptr->px);
        }

        /* Cancel the stair request */
        p_ptr->create_stair = false;
    }

    /* Make rubble */
    if (p_ptr->create_rubble)
    {
        log_debug("Creating rubble via earthquake");
        earthquake(p_ptr->py, p_ptr->px, -1, -1, 5, 0);

        /* Cancel the rubble request */
        p_ptr->create_rubble = false;
    }

    /* Choose panel */
    log_debug("Centering initial panel on player");
    (void)modify_panel(p_ptr->py - SCREEN_HGT / 2,
        p_ptr->px - SCREEN_WID / 2);

    /* Flush messages */
    log_debug("Flushing messages");
    message_flush();

    /* Set labyrinth LOS-only map restriction before first draw. */
    update_labyrinth_view_state(false);

    /* Hack -- Increase "xtra" depth */
    log_debug("Increasing character_xtra depth for display setup");
    character_xtra++;

    /* Clear */
    log_debug("Clearing terminal");
    Term_clear();

    /* Update stuff */
    log_info("Starting initial dungeon display setup");
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

    /* Update stuff */
    log_debug("Running initial update_stuff");
    update_stuff();

    /* Fully update the visuals (and monster distances) */
    log_debug("Setting up view and distance updates");
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_DISTANCE);

    /* Redraw dungeon */
    log_debug("Setting up full redraw");
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);

    /* Window stuff */
    log_debug("Setting up window updates");
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0 | PW_SUPPLY);

    /* Window stuff */
    p_ptr->window |= (PW_MONSTER | PW_MONLIST | PW_COMBAT_ROLLS);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);

    /* Update stuff */
    log_debug("Running second update_stuff");
    update_stuff();

    /* Redraw stuff */
    log_debug("Running redraw_stuff");
    TIME_PHASE("redraw_stuff", redraw_stuff());

    /* Redraw stuff */
    log_debug("Running window_stuff");
    TIME_PHASE("window_stuff", window_stuff());

    /* Hack -- Decrease "xtra" depth */
    log_debug("Decreasing character_xtra depth after display setup");
    character_xtra--;

    /* Update stuff */
    log_debug("Final update_stuff in setup");
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

    /* Combine / Reorder the pack */
    log_debug("Setting up inventory notices");
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Notice stuff */
    log_debug("Running notice_stuff");
    notice_stuff();

    /* Update stuff */
    log_debug("Running final update_stuff");
    update_stuff();

    /* Redraw stuff */
    log_debug("Running final redraw_stuff");
    TIME_PHASE("redraw_stuff", redraw_stuff());

    /* Window stuff */
    log_debug("Running final window_stuff");
    TIME_PHASE("window_stuff", window_stuff());

    /* Refresh */
    log_debug("Final terminal refresh");
    Term_fresh();

    /* Show partition entry messages/XP after the initial draw so they can't be cleared by the setup flush. */
    {
        int entry_mode = PARTITION_NARRATIVE_OFF;
        if (op_ptr->level_entry_narrative_mode == LEVEL_ENTRY_NARRATIVE_MESSAGE)
            entry_mode = PARTITION_NARRATIVE_MESSAGE;
        handle_partition_entry(true, entry_mode);
    }

    sdl_touch_maybe_show_first_game_tutorial();
    sdl_mouse_maybe_show_first_game_tutorial();

    log_info("Dungeon display setup completed successfully");

    /* Log final state after setup */
    log_debug("Final setup state: character_generated=%s, character_icky=%d, update=0x%08X, redraw=0x%08X, window=0x%08X",
              character_generated ? "true" : "false", character_icky,
              p_ptr->update, p_ptr->redraw, p_ptr->window);

    /* Handle delayed death */
    if (p_ptr->is_dead) {
        log_info("Player is dead, exiting dungeon");
        return;
    }

    /* Announce (or repeat) the feeling */
    // if ((p_ptr->depth) && (do_feeling)) do_cmd_feeling();

    /* Announce a player ghost challenge. -LM- */
    if (bones_selector)
        ghost_challenge();

    // explain the truce for the final level
    if ((p_ptr->depth == MORGOTH_DEPTH) && p_ptr->truce)
    {
        msg_print("There is a strange tension in the air.");
        if (p_ptr->skill_use[S_PER] >= 15)
            msg_print("You feel that Morgoth's servants are reluctant to "
                      "attack before he "
                      "delivers judgment.");
    }

    /*** Process this dungeon level ***/

    /* Reset generation depth; the Gates use depth 20 tables while displayed as 0. */
    monster_level = player_generation_depth();
    object_level = player_generation_depth();

    /* Show initial partition narrative according to the configured display mode. */
    if ((op_ptr->level_entry_narrative_mode == LEVEL_ENTRY_NARRATIVE_BANNER_DELAY)
        || (op_ptr->level_entry_narrative_mode == LEVEL_ENTRY_NARRATIVE_BANNER))
    {
        int spawn_sidx = styles_decode_color_style(cave_color[p_ptr->py][p_ptr->px]);
        level_partition_kind spawn_kind =
            level_partition_kind_for_point(p_ptr->py, p_ptr->px);
        if (spawn_sidx >= 0) {
            display_partition_narrative_banner(
                -1, spawn_sidx, spawn_kind,
                op_ptr->level_entry_narrative_mode
                    == LEVEL_ENTRY_NARRATIVE_BANNER_DELAY);
        }
    }

    varda_quest_notice_bastion_level_entry();

    was_in_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH) && (cave_info[p_ptr->py][p_ptr->px] & CAVE_G_VAULT);
    if ((p_ptr->depth == MORGOTH_DEPTH) && !p_ptr->morgoth_hall_entered
        && (was_in_morgoth_vault || (silmarils_possessed() > 0)))
    {
        p_ptr->morgoth_hall_entered = true;
    }
    log_live_special_vault_only_monsters("dungeon loop start");
    last_player_y = p_ptr->py;
    last_player_x = p_ptr->px;

    log_info("Starting main dungeon loop for depth %d", p_ptr->depth);

    /* Main loop */
    while (true)
    {
        /* Hack -- Compact the monster list occasionally */
        if (mon_cnt + 10 > MAX_MONSTERS) {
            log_debug("Compacting monster list (count: %d)", mon_cnt);
            compact_monsters(20);
        }

        /* Hack -- Compress the monster list occasionally */
        if (mon_cnt + 32 < MAX_MONSTERS)
            compact_monsters(0);

        /* Hack -- Compact the object list occasionally */
        if (o_cnt + 32 > z_info->o_max) {
            log_debug("Compacting object list (count: %d)", o_cnt);
            compact_objects(64);
        }

        /* Hack -- Compress the object list occasionally */
        if (o_cnt + 32 < o_max)
            compact_objects(0);

        /*** Apply energy ***/

          /* Can the player move? */
        while ((p_ptr->energy >= 100) && (!p_ptr->leaving))
          {
            /* Start a new combat round BEFORE any actors move this turn.
                    This ensures monsters that act before the player (due to higher
                    energy) are recorded in the same current round as the player's
                    actions, avoiding a one-turn lag in the bottom log. */
            log_trace("[LOOP] Begin player-energy turn: energy=%d", p_ptr->energy);
                new_combat_round();
            log_trace("[LOOP] After new_combat_round: turns_since_combat=%d combat_number=%d old=%d", turns_since_combat, combat_number, combat_number_old);

                /* Process monster with even more energy first */
            log_trace("[LOOP] process_monsters pre-player: threshold=%d", p_ptr->energy + 1);
            TIME_PHASE("monsters(pre-player)", process_monsters(p_ptr->energy + 1));
            log_trace("[LOOP] after process_monsters pre-player: combat_number=%d old=%d", combat_number, combat_number_old);

            /* If still alive */
            if (!p_ptr->leaving)
            {
                /* Update stuff */
                if (p_ptr->update) {
                    update_stuff();
                }

                /* Redraw stuff */
                if (p_ptr->redraw) {
                    TIME_PHASE("redraw_stuff", redraw_stuff());
                }

                /* Process the player */
                log_trace("[LOOP] process_player start");
                TIME_PHASE("process_player", process_player());
                log_trace("[LOOP] process_player end: combat_number=%d old=%d", combat_number, combat_number_old);
                
                /* Scan for artifacts near player and mark as seen */
                scan_artifacts_near_player();
                
            }
        }

        /* Notice stuff */
        if (p_ptr->notice) {
            notice_stuff();
        }

        /* Update stuff */
        if (p_ptr->update) {
            update_stuff();
        }

        /* Redraw stuff */
        if (p_ptr->redraw) {
            TIME_PHASE("redraw_stuff", redraw_stuff());
        }

        /* Redraw stuff */
        if (p_ptr->window) {
            TIME_PHASE("window_stuff", window_stuff());
        }

        /* Place the cursor on the player or target */
        if (hilite_player)
            move_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

        /* Optional fresh */
        if (fresh_after)
            Term_fresh();

        /* Handle "leaving" */
    if (p_ptr->leaving) {
            log_info("Player leaving dungeon level %d", p_ptr->depth);
            break;
        }

        /* Process monsters (any that haven't had a chance to move yet) */
    log_trace("[LOOP] process_monsters post-player: threshold=100");
    TIME_PHASE("monsters(post-player)", process_monsters(100));
    log_trace("[LOOP] after process_monsters post-player: combat_number=%d old=%d", combat_number, combat_number_old);
    
        /* Notice stuff */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff */
        if (p_ptr->redraw)
            TIME_PHASE("redraw_stuff", redraw_stuff());

        /* Redraw stuff */
        if (p_ptr->window)
            TIME_PHASE("window_stuff", window_stuff());

        /* Place the cursor on the player or target */
        if (hilite_player)
            move_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

        /* Optional fresh */
        if (fresh_after)
            Term_fresh();

        /* Handle "leaving" */
    if (p_ptr->leaving)
            break;

        /* Process the world */
        TIME_PHASE("process_world", process_world());

        /* Notice stuff */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff */
        if (p_ptr->redraw)
            TIME_PHASE("redraw_stuff", redraw_stuff());

        /* Window stuff */
        if (p_ptr->window)
            TIME_PHASE("window_stuff", window_stuff());

        /* Place the cursor on the player or target */
        if (hilite_player)
            move_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

        /* Optional fresh */
        if (fresh_after)
            Term_fresh();

        /* Handle "leaving" */
    if (p_ptr->leaving)
            break;

        /* Give the player some energy */
        p_ptr->energy += extract_energy[p_ptr->pspeed];

        /* Give energy to all monsters */
        bool freeze_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH)
            && !p_ptr->morgoth_hall_entered && (silmarils_possessed() == 0);
        for (i = mon_max - 1; i >= 1; i--)
        {
            /* Access the monster */
            m_ptr = &mon_list[i];

            /* Ignore "dead" monsters */
            if (!m_ptr->r_idx)
                continue;

            /* Keep Morgoth's hall frozen until the player enters it */
            if (freeze_morgoth_vault
                && (cave_info[m_ptr->fy][m_ptr->fx] & CAVE_G_VAULT))
            {
                m_ptr->energy = 0;
                continue;
            }

            /* Give this monster some energy */
            m_ptr->energy += extract_energy[m_ptr->mspeed];
        }

        /* Count game turns */
        turn++;
    }
}

/* Tiny proxy for frontends to query current depth without including player headers */
int p_ptr_depth_proxy(void) { return p_ptr ? p_ptr->depth : 0; }
