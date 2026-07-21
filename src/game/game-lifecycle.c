/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "blitz.h"
#include "game/game-lifecycle.h"
#include "fs/file.h"
#include "fs/io_sdl.h"
#include "log/log.h"
#include "metarun.h"
#include "player/killer.h"
#include "score/score_entry.h"
#include "score/score_file_compat.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_postmortem.h"
#include "score/score_runs.h"
#include "ui/character-dump.h"
#include "ui/character-screen.h"
#include "ui/story.h"
#include "ui/story_font.h"
#include "externs.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
void do_cmd_escape(int silmarils)
{
    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[120];

    /* set the escaped flag */
    p_ptr->escaped = true;

    /* Flush input */
    flush();

    /* Commit suicide */
     p_ptr->is_dead = true;

    /* Stop playing */
    p_ptr->playing = false;

    /* Leaving */
    p_ptr->leaving = true;

    /* Get time */
    (void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));

    /* Add note */
    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    /*killed by */
    sprintf(buf, "You escaped the Iron Hells on %s.", long_day);

    /* Write message */
    do_cmd_note(buf, p_ptr->depth);

    // make a note
    switch (silmarils)
    {
    case 0:
        do_cmd_note("You returned empty handed.", p_ptr->depth);
        break;
    case 1:
        do_cmd_note(
            "You brought back a Silmaril from Morgoth's crown!", p_ptr->depth);
        break;
    case 2:
        do_cmd_note("You brought back two Silmarils from Morgoth's crown!",
            p_ptr->depth);
        break;
    case 3:
        do_cmd_note(
            "You brought back all three Silmarils from Morgoth's crown!",
            p_ptr->depth);
        break;
    default:
        do_cmd_note("You brought back so many Silmarils that people should be "
                    "suspicious!",
            p_ptr->depth);
        break;
    }

    if (p_ptr->oath_type > 0)
    {
        if (oath_invalid(p_ptr->oath_type))
        {
            /* Use oath-specific death/escape message */
            char* death_msg = oath_death_message(p_ptr->oath_type);
            if (death_msg && death_msg[0]) {
                do_cmd_note(death_msg, p_ptr->depth);
            } else {
                /* Fallback to generic message if no specific text found */
                do_cmd_note(
                    "You passed from the world, but the stain of a faithless heart remains. You will be remembered not for your deeds, but as a shameful Oathbreaker.",
                    p_ptr->depth);
            }
        }
        else
        {
            do_cmd_note("You kept your oath to the very end.", p_ptr->depth);
        }
    }

    // (void)inkey();

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    /* Cause of death */
    SDL_strlcpy(p_ptr->died_from, "ripe old age", sizeof(p_ptr->died_from));

    /* Defer metarun exit processing until close_game_aux() has recorded the
     * final score. Otherwise the rollover can start a fresh metarun before
     * this escape is written, and the winning character lands in the new run.
     */
    log_info("Player escaped with %d Silmarils (metarun processing deferred until close_game_aux)", silmarils);

}

/*
 * Hack -- victory by slaying Morgoth's illusion
 */
void do_cmd_morgoth_victory(void)
{
    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[160];

    /* Ensure the victory flag is set */
    p_ptr->morgoth_slain = true;

    /* Flush input ahead of the scripted sequence */
    flush();

    /* Treat as a completed run */
    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;
    p_ptr->escaped = false;

    /* Mark the calendar moment */
    (void)strftime(long_day, sizeof(long_day), "%d %B %Y", localtime(&ct));

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    strnfmt(buf, sizeof(buf),
            "On %s you broke the illusion binding Morgoth to his throne.",
            long_day);
    do_cmd_note(buf, p_ptr->depth);

    do_cmd_note(
        "The Valar hail your impossible triumph and pour out their blessing.",
        p_ptr->depth);

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    /* Record cause for high scores */
    SDL_strlcpy(p_ptr->died_from, "Morgoth's illusory defeat",
        sizeof(p_ptr->died_from));

    killer_mark_other(SCORE_KILLER_OTHER);
    killer_commit(p_ptr->died_from);

    if (run_mode_is_blitz())
        blitz_show_end_summary(3);
}

/*
 * Hack -- commit suicide
 */
void do_cmd_suicide(void)
{
    /* Flush input */
    flush();

    /* Verify */
    if (!get_check("This will destroy the current character: are you sure? "))
        return;

    /* Special Verification for suicide (second confirm panel) */
    if (!get_check("Really abandon this character for good? "))
        return;

    /* Commit suicide */
    p_ptr->is_dead = true;

    /* Stop playing */
    p_ptr->playing = false;

    /* Leaving */
    p_ptr->leaving = true;

    SDL_strlcpy(p_ptr->died_from, "their own hand", sizeof(p_ptr->died_from));

    killer_mark_other(SCORE_KILLER_SELF);
    killer_commit(p_ptr->died_from);
}

/*
 * Save the game
 */
void do_cmd_save_game(void)
{
    /* Disturb the player */
    disturb(1, 0);

    // in final deployment versions, you cannot save in the tutorial
    if (DEPLOYMENT && p_ptr->game_type != 0)
    {
        return;
    }

    /* Clear messages */
    message_flush();

    /* Handle stuff */
    handle_stuff();

    if (!save_game_quietly)
    {
        /* Message (in-game message log, not row 0) */
        msg_print("Saving game...");
    }

    /* Refresh */
    Term_fresh();

    /* The player is not dead */
    SDL_strlcpy(p_ptr->died_from, "(saved)", sizeof(p_ptr->died_from));

    /* Forbid suspend */
    signals_ignore_tstp();

    /* Save the player */
    /* Make sure meta-run data (curses, flags, etc.) is up-to-date even
      when the player merely saves & quits. */
    log_info("Saving game and updating metarun data");
    if (!run_mode_is_blitz())
        metarun_update_on_exit(false, false, 0, 0);

    if (save_player())
    {
    log_debug("Game saved successfully");
        if (!save_game_quietly)
        {
            msg_print("Saving game... done.");
        }

        /* Desktop quit still upserts from close_game().  Mobile also updates
         * the live entry here so an OS suspend/terminate can resume cleanly. */

        high_score live_score;
        if (build_live_preview_score(&live_score)) {
            time_t now = time(NULL);
            if (!score_runs_record_current_run(&live_score, now, SCORE_RECORD_ALIVE)) {
                log_warn("Failed to persist live run snapshot for '%s'", op_ptr->full_name);
            }
        }

#if defined(__ANDROID__) || defined(SIL_IOS)
        upsert_live_score_on_save();
#endif

        if (!save_game_quietly && p_ptr->playing && !p_ptr->leaving)
            sdl_popup_notification_show("Saved");
    }

    /* Save failed (oops) */
    else
    {
    log_error("Game save failed");
        if (!save_game_quietly)
        {
            msg_print("Saving game... failed!");
        }
    }

    /* Allow suspend again */
    signals_handle_tstp();

    /* Refresh */
    Term_fresh();

    /* Note that the player is not dead */
    SDL_strlcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));

    /* Reset the quietly flag */
    save_game_quietly = false;
}

/*
 * Handle character death
 */
static void close_game_aux(void)
{
    static bool death_processing = false;
    high_score the_score;

    /* Prevent duplicate death processing */
    if (death_processing)
    {
        log_debug("Death processing already in progress - skipping duplicate call");
        return;
    }
    death_processing = true;
    score_postmortem_clear();

    log_debug("Processing character death for '%s' (wizard=%d, noscore=0x%04X, savefile='%s')",
             op_ptr->full_name, p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore, savefile);

    /* Dump bones file */
    // make_bones();

    /* Save dead player */
    log_info("saving dead player (noscore=0x%04X) -> '%s'", (unsigned)p_ptr->noscore, savefile);
    if (!save_player())
    {
        log_error("Death save failed - player data may be lost");
        msg_print("death save failed!");
        message_flush();
    }

    /* Get time of death */
    time_t death_time = time(NULL);
    score_entry_set_death_time(death_time);

    /* Clear screen */
    Term_clear();

    /* Enter player in high score list */
    log_info("entering score");
    create_score(&the_score);
    score_record_status final_status = p_ptr->escaped ? SCORE_RECORD_ESCAPED : SCORE_RECORD_DEAD;
    u32b run_record_id = SCORE_RUNS_METARUN_UNKNOWN;
    if (score_runs_record_current_run_with_id(&the_score, death_time,
        final_status, &run_record_id)) {
        score_runs_set_legacy_link(&the_score, run_record_id);
    } else {
        log_warn("Failed to persist run statistics for '%s'", op_ptr->full_name);
    }
    score_entry_enter(&the_score);

    // cure hallucination and rage
    p_ptr->rage = 0;
    p_ptr->image = 0;

    // Automatic character dump
    char curr_time[30], sheet[90];
    time_t ct = time((time_t*)0);
    (void)strftime(curr_time, 30, "%Y%m%d-%H%M%S.txt", localtime(&ct));
    sprintf(sheet, "%s-%s", op_ptr->full_name, curr_time);
    errr err;
    // Save the screen
    screen_save();
    // Dump a character file
    err = file_character(sheet, false);
    // Load the screen
    screen_load();
    // Check result
    if (err)
    {
        // Clear screen
        Term_clear();
        // Warning
        msg_print("Automatic character dump failed!");
        // Flush messages
        message_flush();
    }

    /* Record this run's outcome for the metarun ledger */
    int final_score = score_points(&the_score);
    if (!run_mode_is_blitz())
    {
        if (p_ptr->escaped)
        {
            int escaped_silmarils = parse_score_int(the_score.silmarils,
                sizeof(the_score.silmarils), 0);
            log_info("Player escaped - updating metarun data after score entry");
            metarun_update_on_exit(false, true, (byte)MAX(escaped_silmarils, 0),
                final_score);
        }
        else if (p_ptr->morgoth_slain && !p_ptr->escaped)
        {
            log_info("Player achieved Morgoth victory - updating metarun data");
            metarun_update_on_exit(false, false, 3, final_score);
        }
        else
        {
            log_info("Player died - updating metarun data");
            if (!p_ptr->escaped)
                metarun_update_on_exit(true, false, 0, final_score);
        }
    }
    else
    {
        int blitz_silmarils = silmarils_possessed();

        if (blitz_silmarils < 0)
            blitz_silmarils = 0;
        if (p_ptr->morgoth_slain && blitz_silmarils < 3)
            blitz_silmarils = 3;

        blitz_show_end_summary((byte)blitz_silmarils);
    }

    /* Final Look now owns the complete postmortem UI.  Choosing Quit there
     * returns directly to the welcome screen; there is no second tomb menu. */
    death_spectator_view();

    score_postmortem_clear();
    death_processing = false;
}

/*
 * Close up the current game (player may or may not be dead)
 *
 * Death processing saves and records the finalized run before Final Look is
 * entered.  Leaving Final Look completes shutdown and returns to welcome.
 */
void close_game(void)
{
    char buf[1024];

    log_info("Starting game close sequence for player '%s'", op_ptr->full_name);

    /* Handle stuff */
    handle_stuff();

    /* Flush the messages */
    message_flush();

    /* Flush the input */
    flush();

    /* No suspending now */
    signals_ignore_tstp();

    /* Hack -- Increase "icky" depth */
    character_icky++;
    log_debug("game-lifecycle.c: character_icky incremented to %d (opening scores file)", character_icky);

    /* Build the filename */
    build_current_score_path(buf, sizeof(buf));

    log_debug("Opening scores file for read/write: %s", buf);

    /* Create backup before opening for write operations */
    backup_scores_file(buf);

    /* Grab permissions */
    safe_setuid_grab();

    /* Open the high score file, for reading/writing */
    highscore_fd = score_file_open(buf, O_RDWR);

    /* Drop permissions */
    safe_setuid_drop();
    
    if (!highscore_fd) {
    log_error("Failed to open scores file for read/write");
    }

    /* Handle death */
    if (p_ptr->is_dead)
    {
        /* Auxiliary routine in normal games */
        if (p_ptr->game_type == 0)
        {
            log_info("Player %s died at depth %d in %s.",
                op_ptr->full_name, p_ptr->depth, p_ptr->died_from);
            close_game_aux();
        }
        else if (p_ptr->game_type == -1)
        {
            monster_lore* l_ptr = &l_list[R_IDX_ORC_ARCHER];

            if (p_ptr->chp <= 0)
            {
                if (l_ptr->psights == 0)
                {
                    pause_with_text(tutorial_early_death_text, 5, 10, NULL, 0);
                }
                else
                {
                    pause_with_text(tutorial_late_death_text, 5, 10, NULL, 0);
                }
            }
        }

        /* Now wipe the level */
        wipe_o_list();
        wipe_mon_list();
        cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    }

    /* Still alive */
    else
    {
        /* Save the game without transient message-line text before scores. */
        save_game_quietly = true;
        do_cmd_save_game();
        Term_erase(0, 0, 255);
        Term_fresh();

        high_score preview;
        if (build_live_preview_score(&preview))
        {
            u32b run_record_id = SCORE_RUNS_METARUN_UNKNOWN;
            time_t now = time(NULL);
            if (score_runs_record_current_run_with_id(&preview, now,
                SCORE_RECORD_ALIVE, &run_record_id)) {
                score_runs_set_legacy_link(&preview, run_record_id);
            } else {
                log_warn("Failed to persist live run snapshot for '%s'",
                    op_ptr->full_name);
            }
            /* A Blitz relaunch leaves the story session silently; the alive
             * snapshot above is still recorded so the story run resumes, but
             * the score screen is skipped for a seamless switch. */
            if (!blitz_launch_requested())
                show_scores_interactive_highlight(&preview);
        }
        else if (!blitz_launch_requested())
            show_scores_interactive();

        /* Update the live character entry in the scores file so that
           scores.raw acts as a database of current running characters.
           We record an entry with how == "(alive and well)". */
        if (highscore_fd) {
            char saved_how[sizeof(p_ptr->died_from)];
            SDL_strlcpy(saved_how, p_ptr->died_from, sizeof(saved_how));
            SDL_strlcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));
            high_score live_score;
            create_score(&live_score);
            {
                u32b run_record_id = SCORE_RUNS_METARUN_UNKNOWN;
                time_t now = time(NULL);
                if (score_runs_record_current_run_with_id(&live_score, now,
                    SCORE_RECORD_ALIVE, &run_record_id)) {
                    score_runs_set_legacy_link(&live_score, run_record_id);
                } else {
                    log_warn("Failed to persist live run snapshot for '%s'",
                        op_ptr->full_name);
                }
            }

            /* Restore original (probably redundant during quit) */
            SDL_strlcpy(p_ptr->died_from, saved_how, sizeof(p_ptr->died_from));

            /* Acquire write lock while we upsert */
            safe_setuid_grab();
            /* TODO: File locking not supported with FILE* - temporarily disabled */
            /* bool have_lock = (fd_lock(highscore_fd, F_WRLCK) == 0); */
            bool have_lock = true; /* assume we have lock for now */
            safe_setuid_drop();

            if (!have_lock) {
                log_warn("Could not acquire lock to upsert live score entry");
            } else {
                /* Try to find existing alive entry for this player */
                if (highscore_seek(0) == 0) {
                    high_score tmp;
                    int idx;
                    u32b limit = MIN(scores_file_entry_count,
                        (u32b)MAX_HISCORES);
                    bool found = false;
                    for (idx = 0; idx < (int)limit; idx++) {
                        if (highscore_read(&tmp)) break; /* EOF */
                        if (streq(tmp.who, live_score.who) && streq(tmp.how, "(alive and well)")) {
                            found = true; break;
                        }
                    }
                    if (found) {
                        log_debug("Updating existing live score entry for %s at %d", live_score.who, idx);
                        highscore_seek(idx);
                        highscore_write(&live_score);
                    } else {
                        log_debug("Inserting new live score entry for %s", live_score.who);
                        highscore_add(&live_score); /* adds (may not perfectly shift ordering) */
                    }
                }

                /* Release lock */
                safe_setuid_grab();
                /* TODO: File locking not supported with FILE* - temporarily disabled */
                /* (void)fd_lock(highscore_fd, F_UNLCK); */
                safe_setuid_drop();
            }
        }

        // Sil-y: Sil used to crash on loading a saved game from the main menu
        //        immediately after quitting via Control-X.
        //        adding the following lines seems to stop that.

        /* Now wipe the level */
        wipe_o_list();
        wipe_mon_list();
    }

    /* Shut the high score file */
    log_debug("Closing highscore file");
    SDL_CloseIO(highscore_fd);

    /* Forget the high score fd */
    highscore_fd = NULL;

    log_info("Game close sequence completed");

    /* Hack -- Decrease "icky" depth */
    character_icky--;
    log_debug("game-lifecycle.c: character_icky decremented to %d (scores file closed)", character_icky);

    /* Allow suspending now */
    signals_handle_tstp();
}

/*
 * Handle abrupt death of the visual system
 *
 * This routine is called only in very rare situations, and only
 * by certain visual systems, when they experience fatal errors.
 *
 * XXX XXX Hack -- clear the death flag when creating a HANGUP
 * save file so that player can see tombstone when restart.
 */
void exit_game_panic(void)
{
    /* If nothing important has happened, just quit */
    if (!character_generated || character_saved)
        quit("panic");

    /* Mega-Hack -- see "msg_print()" */
    msg_flag = false;

    /* Clear the top line */
    prt("", 0, 0);

    /* Hack -- turn off some things */
    disturb(1, 0);

    /* Hack -- Delay death XXX XXX XXX */
    if (p_ptr->chp <= 0)
        p_ptr->is_dead = false;

    /* Hardcode panic save */
    p_ptr->panic_save = 1;

    /* Forbid suspend */
    signals_ignore_tstp();

    /* Indicate panic save */
    SDL_strlcpy(p_ptr->died_from, "(panic save)", sizeof(p_ptr->died_from));

    /* Panic save, or get worried */
    if (!save_player())
        quit("panic save failed!");

    /* Successful panic save */
    quit("panic save succeeded!");
}

