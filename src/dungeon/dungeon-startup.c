/* File: dungeon/dungeon-startup.c */

#include "angband.h"
#include "dungeon-internal.h"

/*
 * Hack - Know inventory upon death
 */
static void death_knowledge(void)
{
    int i;

    object_type* o_ptr;

    /* Hack -- Know everything in the inven/equip */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

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

static bool startup_try_autoload_current_mode(cptr mode_name)
{
    bool autoloaded;

    startup_loading_overlay_arm();
    autoloaded = autoload_alive_from_scores();
    startup_loading_overlay_disarm();

    if (autoloaded && character_loaded)
    {
        log_info("Auto-loaded alive %s character from scores; skipping selection",
            mode_name ? mode_name : "current-mode");
        return true;
    }

    return false;
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
 * Some platforms (Windows) start brand new games with "savefile" and 
 * "op_ptr->base_name" both empty, and initialize them later based on 
 * the player name. To prevent weirdness, we must initialize 
 * "op_ptr->base_name" to "nameless" if it is empty.
 *
 * Note that we load the RNG state from savefiles and
 * only initialize it when starting a brand new character.
 */
PlayResult play_game(void)
{
    bool new_game = false;
    bool startup_icky_active = false;

    log_info("play_game: FUNCTION ENTERED");

    ui_reset_transient_state_for_new_session();

    /* Safety: Fix character_icky imbalance from previous game sessions */
    if (character_icky != 0)
    {
        log_info("play_game: Fixing character_icky imbalance - was %d, resetting to 0", character_icky);
        character_icky = 0;
    }

    /* Hack -- Increase "icky" depth */
    character_icky++;
    startup_icky_active = true;
    log_debug("play_game: character_icky incremented to %d", character_icky);

    /* Verify main term */
    if (!term_screen)
    {
        quit("main window does not exist");
    }

    /* Make sure main term is active */
    Term_activate(term_screen);

    /* Verify minimum size */
    {
        /* get_sdl_min_terminal_mode() returns 0=normal, 1=compact. */
        const bool compact_mode = (get_sdl_min_terminal_mode() != 0);
        const int min_hgt = compact_mode ? 18 : 24;
        const int min_wid = compact_mode ? 50 : 80;
        if ((Term->hgt < min_hgt) || (Term->wid < min_wid))
        {
#if defined(__ANDROID__) || defined(SIL_IOS)
            log_error("main window too small on mobile: %dx%d (need at least %dx%d)",
                Term->wid, Term->hgt, min_wid, min_hgt);
#else
            log_error("main window too small: %dx%d (need at least %dx%d)",
                Term->wid, Term->hgt, min_wid, min_hgt);
#endif
            quit("main window is too small");
        }
    }

    /* Hack -- Turn off the cursor */
    (void)Term_set_cursor(false);

    /* Hack -- Default base_name */
    if (!op_ptr->base_name[0])
    {
        SDL_strlcpy(op_ptr->base_name, "nameless", sizeof(op_ptr->base_name));
    }

    bool explicit_blitz_launch = blitz_launch_requested();
    run_mode_activate_pending();
    if (explicit_blitz_launch)
        blitz_launch_clear();

    bool prefer_blitz_startup = !run_mode_is_blitz()
        && op_ptr && op_ptr->opt[OPT_load_blitz_by_default];

    if (!run_mode_is_blitz() && !prefer_blitz_startup) {
        if (metarun_created) /* show only the first time ever */
        {
            print_story_intro();
            screen_clear_all_terms_no_fresh();
            message_discard_pending();
        }
    }

    /* New startup behavior: try to auto-load any alive character
     * lingering in the scorefile. If successful, skip character
     * selection and proceed directly. */
    character_loaded = false;
    character_loaded_dead = false;

    if (prefer_blitz_startup)
    {
        run_mode_set_pending(RUN_MODE_BLITZ);
        run_mode_set_current(RUN_MODE_BLITZ);

        if (startup_try_autoload_current_mode("Blitz"))
        {
            new_game = false;
        }
        else
        {
            run_mode_set_pending(RUN_MODE_STORY);
            run_mode_set_current(RUN_MODE_STORY);

            print_metarun_stats();

            if (!run_mode_is_blitz()
                && startup_try_autoload_current_mode("story"))
            {
                new_game = false;
            }
        }
    }
    else
    {
        bool startup_score_empty = highscore_is_empty();

        if (explicit_blitz_launch && run_mode_is_blitz())
        {
            log_info("Explicit Blitz launch requested; checking for an alive Blitz character");
        }

        if (startup_try_autoload_current_mode(
                run_mode_is_blitz() ? "Blitz" : "story"))
        {
            new_game = false;
        }
        else if (!run_mode_is_blitz() && !metarun_created
            && !startup_score_empty && score_count_alive_entries() == 0)
        {
            print_metarun_stats();
        }
    }

    if (!character_loaded)
    {
        screen_clear_all_terms_no_fresh();
        message_discard_pending();
    }

    log_info("Starting new game session");

     /* Only reset flags if no character has been loaded yet.
         If autoload succeeded, keep the loaded state and the
         dungeon-loaded flag set by load_player(). */
     if (!character_loaded) {
          character_dungeon = false;
          character_loaded = false;
          character_loaded_dead = false;
     }

    bool resume_character_selection = false;
    byte resume_prace = 0;
    byte resume_pcharacter = 0;

    for (;;)
    {
        /* If we already loaded a living character, break to init */
        if (character_loaded) break;

        /* Wipe the player each time we enter a fresh creation sequence. */
        if (!resume_character_selection)
            player_wipe();
        else
        {
            p_ptr->prace = resume_prace;
            p_ptr->pcharacter = resume_pcharacter;
            if (z_info && p_ptr->prace < z_info->p_max)
                rp_ptr = &p_info[p_ptr->prace];
            if (z_info && p_ptr->pcharacter < z_info->c_max)
                current_character_profile = &c_info[p_ptr->pcharacter];
        }

        log_info("Choosing character");
        NavResult cr = run_mode_is_blitz() ? blitz_character_creation()
            : (resume_character_selection ? character_creation_resume_character()
                                          : character_creation());
        resume_character_selection = false;
        if (cr == NAV_TO_MAIN) {
            log_info("Returning to main menu from character creation");
            sdl_music_stop_main();
            sdl_music_stop_ambient();
            play_game_pop_startup_icky(&startup_icky_active,
                "returning to main menu from character creation");
            return PLAY_DONE;
        }
        if (cr == NAV_QUIT) {
            log_info("Quitting from character creation");
            play_game_pop_startup_icky(&startup_icky_active,
                "quitting from character creation");
            return PLAY_QUIT;
        }

        /* Set player name from character BEFORE load_player() so savefile path is correct */
        SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name, sizeof(op_ptr->full_name));
        process_player_name(true);  /* Update savefile path */
        log_debug("Player name set to: %s (character %d), savefile: %s", op_ptr->full_name, p_ptr->pcharacter, savefile);

        /* Attempt to load (manual path) */
        (void)load_player();

        /*
         * If we loaded a dead character savefile, load_player() returns false but
         * leaves p_ptr populated with the dead character's state. We need to wipe
         * and properly re-initialize for a fresh start with the same character type.
         */
        if (character_loaded_dead)
        {
            log_info("Loaded dead character from '%s' - wiping for fresh restart",
                savefile);

            /* Wipe player data - this will restore prace/pcharacter/stats from dead char */
            player_wipe();

            /* Re-initialize global race/character pointers to match restored values */
            rp_ptr = &p_info[p_ptr->prace];
            current_character_profile = &c_info[p_ptr->pcharacter];

            /* Clear the flag after wipe so subsequent code knows we're starting fresh */
            character_loaded_dead = false;
        }

        log_info(character_loaded ? "Character loaded" :
            (character_loaded_dead ? "Character loaded dead" : "Character creation started"));

        new_game = !character_loaded;

        if (new_game)
        {
            log_info("Starting new game - initializing character");
        /* Init RNG */
        {
            u64b seed = (u64b)time(NULL);

#ifdef SET_UID
            seed ^= ((seed >> 3) * (getpid() << 1));
#endif

            Rand_state_init(seed);
            log_debug("RNG initialized with seed: %llu", (unsigned long long)seed);
        }

        log_info("Rolling up a new character");
        log_trace("Character creation phase: setting up dungeon state");
        /* The dungeon is not ready */
        character_dungeon = false;

        /* Hack -- seed for flavors */
        seed_flavor = rand_int(0x10000000);

        /* Hack -- seed for random artefacts */
        seed_randart = rand_int(0x10000000);

        log_debug("Game seeds initialized - flavor: %u, randart: %u", seed_flavor, seed_randart);

        /* Roll up a new character */
        NavResult br = player_birth();
        if (br == NAV_BACK) {
            log_debug("Returning to character selection from birth");
            if (!run_mode_is_blitz())
            {
                resume_prace = p_ptr->prace;
                resume_pcharacter = p_ptr->pcharacter;
                player_wipe();
                resume_character_selection = true;
            }
            /* back to Character Selection */
            continue;
        }
        if (br == NAV_TO_MAIN) {
            log_info("Returning to main menu from character birth");
            sdl_music_stop_main();
            sdl_music_stop_ambient();
            play_game_pop_startup_icky(&startup_icky_active,
                "returning to main menu from character birth");
            return PLAY_DONE;
        }
        if (br == NAV_QUIT) {
            log_info("Quitting from character birth");
            play_game_pop_startup_icky(&startup_icky_active,
                "quitting from character birth");
            return PLAY_QUIT;
        }
        /* NAV_OK falls through */

        sdl_pointer_attack_reset_to_melee();

        // Reset the autoinscriptions
        autoinscribe_clean();
        autoinscribe_init();

        log_debug("New character rolled up - autoinscriptions reset");

        /* Hack -- enter the world */
        if (!character_loaded) {
        turn = 1;
        playerturn = 0;
        min_depth_counter = 0;

        /* Start player on level 1 */
        p_ptr->depth = 1;

        log_debug("New game state initialized - starting at depth 1, turn 1");
        }
        }

        /* succeeded (either loaded or created) - exit the creation loop */
        break;
    }

    /* Normal machine (process player name) */
    if (savefile[0])
    {
        process_player_name(false);
    }

    /* Weird machine (process player name, pick savefile name) */
    else
    {
        process_player_name(true);
    }

    /* Only show story when no alive character exists (fresh start or all characters dead) */
    if (!run_mode_is_blitz() && score_count_alive_entries() == 0)
    {
        sdl_music_play_main_full();
        print_story(15,1);
        screen_clear_all_terms_no_fresh();
        message_discard_pending();
    }

    log_debug("Game initialization complete, starting main game loop");
    log_trace("QUEST DEBUG: Quest states loaded - Aulë: %d, Mandos: %d, Tulkas: %d",
             p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->tulkas_quest);
    log_trace("QUEST DEBUG: Special abilities - have_ability[S_SPC][SPC_MANDOS]=%d, have_ability[S_SPC][SPC_AULE]=%d",
             p_ptr->have_ability[S_SPC][SPC_MANDOS], p_ptr->have_ability[S_SPC][SPC_AULE]);
    log_trace("QUEST DEBUG: Special abilities - active_ability[S_SPC][SPC_MANDOS]=%d, active_ability[S_SPC][SPC_AULE]=%d",
             p_ptr->active_ability[S_SPC][SPC_MANDOS], p_ptr->active_ability[S_SPC][SPC_AULE]);

    /* Validate quest states after load (auto-complete if targets are dead) */
    validate_tulkas_quest_on_load();

    /* Hack -- Enter wizard mode */
    if (arg_wizard && enter_wizard_mode())
    {
        p_ptr->wizard = true;
        log_debug("Wizard mode activated");
    }

    /* Flavor the objects */
    flavor_init();

    /* Build or load the drop catalog (needs flavored kinds) */
    drop_system_init();

    /* Reset visuals */
    reset_visuals(true);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0 | PW_SUPPLY);

    /* Window stuff */
    p_ptr->window |= (PW_MONSTER | PW_MESSAGE);

    /* Window stuff */
    window_stuff();

    /* Set or clear "hjkl_movement" if requested */
    if (arg_force_original)
        hjkl_movement = false;
    if (arg_force_roguelike)
        hjkl_movement = true;

    /* React to changes */
    Term_xtra(TERM_XTRA_REACT, 0);

    /* Generate a dungeon level if needed */
    if (!character_dungeon)
    {
        log_info("Generating initial dungeon level");
        reset_level_entry_tracking();
        /* About to call generate_cave() function */
        generate_cave();
        log_debug("Initial dungeon level generated successfully");
    }

    /* Character is now "complete" */
    character_generated = true;
    log_debug("play_game: character_generated set to true - character creation complete");
    ability_log_sync_missing();
    snapshot_run_history("character start");

    /* If Tulkas quest was auto-completed on load, spawn Tulkas and show messages */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE && p_ptr->tulkas_quest_complete == 1)
    {
        int y, x;
        bool spawned = false;
        
        log_trace("Spawning Tulkas for auto-completed quest on load");
        
        /* Try to find a suitable spot near the player */
        for (y = p_ptr->py - 3; y <= p_ptr->py + 3 && !spawned; y++)
        {
            for (x = p_ptr->px - 3; x <= p_ptr->px + 3 && !spawned; x++)
            {
                if (in_bounds(y, x) && cave_floor_bold(y, x) && 
                    cave_m_idx[y][x] == 0 && distance(p_ptr->py, p_ptr->px, y, x) >= 2)
                {
                    if (place_monster_one(y, x, R_IDX_TULKAS, true, true, NULL))
                    {
                        msg_print("Upon loading, you recall that your quest target has already fallen!");
                        msg_print("Tulkas Unclad materializes nearby with a booming laugh, ready to reward your valor!");
                        spawned = true;
                        log_trace("Tulkas spawned at (%d, %d) for auto-completed quest", y, x);
                    }
                }
            }
        }
        
        if (!spawned)
        {
            log_trace("Failed to spawn Tulkas near player, will retry on next level");
        }
    }

    /* Start with normal object generation mode */
    object_generation_mode = OB_GEN_MODE_NORMAL;

    /* Start playing */
    p_ptr->playing = true;
    metarun_created = false;

    log_info("Game session started - entering play mode");

#if defined(__ANDROID__) || defined(SIL_IOS)
    (void)mobile_autosave_game("game session start");
#endif
    
    /* Any active run, including the Gates at depth 0, uses ambient gameplay music. */
    log_debug("Starting game session at depth=%d - switching to ambient music", p_ptr->depth);
    sdl_music_stop_main();
    sdl_music_play_ambient();
    last_music_depth = p_ptr->depth;

    /* Hack -- Enforce "delayed death" */
    if (p_ptr->chp <= 0)
        p_ptr->is_dead = true;

    /* Redraw everything */
    // Sil-y: added to get 'shades' right in extra inventory terms
    screen_set_startup_supporting_panes_hidden(false);
    screen_set_startup_touch_pane_hidden(false);
#if defined(USE_SDL) && (defined(__ANDROID__) || defined(SIL_IOS))
    (void)sdl_prepare_first_gameplay_main_view_zoom(1);
#endif
    do_cmd_redraw();

    // update player noise
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);

    // reset combat roll info
    turns_since_combat = 0;

    // assume the player is on the ground and not being knocked back
    p_ptr->leaping = false;
    p_ptr->knocked_back = false;

    /* Startup is complete; normal saved screens now own character_icky. */
    play_game_pop_startup_icky(&startup_icky_active,
        "entering main game loop");

    /* Process */
    while (true)
    {
        log_trace("Starting dungeon level processing loop");
        /* Process the level */
        dungeon();

        /* Notice stuff */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Window stuff */
        if (p_ptr->window)
            window_stuff();

        /* Cancel the target */
        target_set_monster(0);

        /* Cancel the health bar */
        health_track(0);

        /* Forget the view */
        forget_view();

        /* Handle "quit and save" */
        if (!p_ptr->playing && !p_ptr->is_dead)
        {
            log_info("Player quit and saved - exiting game loop");
            
            /* Stop gameplay audio; the title screen chooses its own track. */
            sdl_music_stop_main();
            sdl_music_stop_ambient();
            
            break;
        }

        /* Erase the old cave */
        /* If the character is dead, then we don't erase yet */
        if (!p_ptr->is_dead)
        {
            log_trace("Cleaning up level data for transition");
            wipe_o_list();
            wipe_mon_list();
        }

        /* XXX XXX XXX */
        message_flush();

        /* Accidental Death */
        if (p_ptr->playing && p_ptr->is_dead)
        {
            log_info("Player '%s' died at level %d, turn %d.",
                op_ptr->base_name, p_ptr->depth, turn);
            /* Mega-Hack -- Allow player to cheat death */
            if ((p_ptr->wizard || (p_ptr->noscore & 0x0008) || cheat_live)
                && !get_check("Die? "))
            {
                log_debug("Player cheated death - restoring to full health");
                /* Mark savefile */
                p_ptr->noscore |= 0x0001;

                /* Message */
                msg_print("You invoke wizard mode and cheat death.");
                message_flush();

                /* Cheat death */
                p_ptr->is_dead = false;

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

                /* Note cause of death XXX XXX XXX */
                SDL_strlcpy(p_ptr->died_from, "Cheating death",
                    sizeof(p_ptr->died_from));

                /* Need to generate a new level */
                p_ptr->leaving = true;
            }
        }

        /* Take a mini screenshot for dead characters */
        if (p_ptr->is_dead)
        {
            log_debug("Character dead - taking screenshot and revealing map");
            
            /* Stop gameplay audio; the title screen chooses its own track. */
            sdl_music_stop_main();
            sdl_music_stop_ambient();
            if (p_ptr->escaped || p_ptr->morgoth_slain)
            {
                sdl_music_play_main();
            }
            else
            {
                sdl_music_play_death();
            }
            
            death_knowledge();

            do_cmd_wiz_unhide(255);
            update_view();
            mini_screenshot();
            detect_all_doors_traps();
        }

        /* Handle "death" */
        if (p_ptr->is_dead)
        {
            log_info("Character '%s' died - ending game session", op_ptr->base_name);
            break;
        }

        /* Make a new level */
        log_info("Generating new dungeon level at depth %d", p_ptr->depth);
        reset_level_entry_tracking();
        generate_cave();
        log_debug("New dungeon level generated successfully");
    }

    /* Close stuff */
    log_info("Player '%s' has left the game.", op_ptr->base_name);

    play_game_pop_startup_icky(&startup_icky_active, "function exit");

    close_game();
    if (!p_ptr->is_dead && !p_ptr->playing)
    {
        if (p_ptr->quit_to_menu)
        {
            p_ptr->quit_to_menu = false; /* Reset the flag */
            return PLAY_DONE;
        }
        else
        {
            return PLAY_QUIT;
        }
    } else {
        return PLAY_DONE;
    }
}
