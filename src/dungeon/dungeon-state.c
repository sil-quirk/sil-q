/* File: dungeon/dungeon-state.c */

#include "angband.h"
#include "dungeon-internal.h"

/* Morgoth vault tracking variables - file scope for dungeon modules */
int last_player_y = 0;
int last_player_x = 0;
bool was_in_morgoth_vault = false;
bool morgoth_entry_preconfirmed = false;

/* Track last depth for music changes (moved from dungeon() for proper reset) */
int last_music_depth = -999;

/* Track first entry to skip level sound (moved from dungeon() for proper reset) */
bool first_entry_to_dungeon = true;

/* True while the post-mortem spectator viewport is active. */
bool death_spectator_mode = false;
bool death_spectator_exit_requested = false;
void play_game_pop_startup_icky(bool* startup_icky_active,
    cptr reason)
{
    if (!startup_icky_active || !*startup_icky_active)
        return;

    if (character_icky > 0)
        character_icky--;
    else
        log_warn("play_game: startup character_icky pop requested while value is %d",
            character_icky);

    *startup_icky_active = false;
    log_debug("play_game: character_icky decremented to %d (%s)",
        character_icky, reason ? reason : "startup complete");
}

bool restore_player_position_after_denied_move(int y, int x)
{
    int old_y;
    int old_x;

    if (!p_ptr || !in_bounds_fully(y, x))
        return false;

    old_y = p_ptr->py;
    old_x = p_ptr->px;

    if ((old_y == y) && (old_x == x))
        return true;

    if (cave_m_idx[y][x] > 0)
    {
        log_warn("restore denied move blocked: destination (%d,%d) has monster idx %d",
            y, x, cave_m_idx[y][x]);
        return false;
    }

    if (in_bounds_fully(old_y, old_x) && (cave_m_idx[old_y][old_x] < 0))
        cave_m_idx[old_y][old_x] = 0;

    cave_m_idx[y][x] = -1;
    p_ptr->py = y;
    p_ptr->px = x;

    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_DISTANCE | PU_PANEL);
    p_ptr->redraw |= (PR_MAP);
    p_ptr->window |= (PW_OVERHEAD);

    if (in_bounds_fully(old_y, old_x))
        lite_spot(old_y, old_x);
    lite_spot(y, x);

    return true;
}

void snapshot_run_history(const char* reason)
{
    if (!character_generated || !p_ptr || p_ptr->is_dead)
        return;

    high_score preview;
    if (!build_live_preview_score(&preview))
        return;

    time_t now = time(NULL);
    if (now == (time_t)-1)
        now = 0;

    if (!score_runs_record_current_run(&preview, now, SCORE_RECORD_ALIVE)) {
        log_warn("run snapshot failed (%s)", reason ? reason : "unspecified");
    } else if (reason) {
        log_trace("run snapshot recorded (%s)", reason);
    }
}

/*
 * Reset all dungeon-related static state for a new game.
 * Called from re_init_some_things() to ensure clean state
 * when starting a new game after death without restarting the app.
 */
void reset_dungeon_state(void)
{
    /* Reset file-scope static variables */
    last_player_y = 0;
    last_player_x = 0;
    was_in_morgoth_vault = false;
    morgoth_entry_preconfirmed = false;
    death_spectator_mode = false;
    death_spectator_exit_requested = false;
    /* Reset music/sound tracking */
    last_music_depth = -999;
    first_entry_to_dungeon = true;

    /* Reset level entry tracking */
    reset_level_entry_tracking();
}
