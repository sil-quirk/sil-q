/* File: dungeon/dungeon-spectator.c */

#include "angband.h"
#include "dungeon-internal.h"

bool death_spectator_command_allowed(int command)
{
    /* Final Look is the normal UI in a frozen dungeon, not a separate
     * read-only command set.  Keep menus, panes, and their navigation on the
     * usual command path; reject only commands that immediately perform a
     * gameplay action.  Commands that open a browser are deliberately not in
     * this list: their individual commit paths refuse the action. */
    switch (command)
    {
    case CMD_ACTIVE_WEAPON_MODE:
    case '\t':
    case '/':
    case 'T':
    case ';':
    case '.':
    case 'z':
    case '%':
    case 'Z':
    case 'g':
    case 'S':
    case '<':
    case '>':
    case 'o':
    case 'c':
    case 'b':
    case 'D':
    case 'X':
    case '-':
    case '{':
    case 'a':
    case KTRL('A'):
    case 'E':
    case KTRL('F'):
    case 'f':
    case 'F':
    case 't':
    case KTRL('T'):
    case 'p':
    case KTRL('W'):
    case KTRL('Y'):
    case KTRL('S'):
    case KTRL('X'):
    case KTRL('C'):
        return false;
    default:
        return true;
    }
}

static void death_spectator_reset_command(void)
{
    p_ptr->command_cmd = 0;
    p_ptr->command_new = 0;
    p_ptr->command_rep = 0;
    p_ptr->command_arg = 0;
    p_ptr->command_dir = 0;
}

typedef struct death_spectator_game_state
{
    bool is_dead;
    bool playing;
    bool leaving;
    int character_icky;
} death_spectator_game_state;

/*
 * The postmortem code reaches Final Look after the ordinary dungeon loop has
 * already marked the character dead/leaving and entered an "icky" full-screen
 * context.  Those flags are also the normal UI's definition of "not in the
 * game", so leaving them set turns Final Look into a collection of special
 * cases: panes disappear, mouse inspection is disabled, and menus take
 * different paths.
 *
 * Present the saved dungeon through the real live-game UI instead.  The
 * death_spectator flag remains true solely as the read-only action guard; the
 * actual death/close-game state is restored verbatim on exit.
 */
static death_spectator_game_state death_spectator_enter_game_ui(void)
{
    death_spectator_game_state old_state = {
        p_ptr->is_dead,
        p_ptr->playing,
        p_ptr->leaving,
        character_icky
    };

    p_ptr->is_dead = false;
    p_ptr->playing = true;
    p_ptr->leaving = false;
    character_icky = 0;

    return old_state;
}

static void death_spectator_leave_game_ui(
    const death_spectator_game_state* old_state)
{
    if (!old_state)
        return;

    if (character_icky != 0)
    {
        log_warn("Final Look UI returned with character_icky=%d; restoring %d",
            character_icky, old_state->character_icky);
    }

    p_ptr->is_dead = old_state->is_dead;
    p_ptr->playing = old_state->playing;
    p_ptr->leaving = old_state->leaving;
    character_icky = old_state->character_icky;
}

static void death_spectator_prepare_display(void)
{
    int i;

    /* Reveal player knowledge of objects on the final level. */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        if (!o_ptr->k_idx)
            continue;

        object_aware(o_ptr);
        object_known(o_ptr);
    }

    /* Fully light the level and reveal monsters. */
    Term_clear();
    wiz_light();
    do_cmd_wiz_unhide(255);

    /* Force a comprehensive redraw across all panes. */
    p_ptr->redraw |= 0x0FFFFFFFL;
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0 | PW_MONSTER
        | PW_MONLIST | PW_COMBAT_ROLLS | PW_OVERHEAD | PW_SUPPLY);

    handle_stuff();

    msg_print(
        "Final look: you may inspect the dungeon and use every menu, but cannot act.");
}

void death_spectator_view(void)
{
    death_spectator_game_state old_state;

    /* Arm the read-only guard before exposing live-game state to the UI. */
    death_spectator_mode = true;
    death_spectator_exit_requested = false;
    old_state = death_spectator_enter_game_ui();
    sdl_mouse_path_cancel();

    /* Clear any queued commands from the main loop. */
    death_spectator_reset_command();

    /* Prevent lingering keypresses from auto-triggering commands. */
    flush();

    death_spectator_prepare_display();

    while (true)
    {
        request_command();

        /* The main menu's Quit entry is the sole way to finish Final Look.
         * Escape therefore behaves exactly as it does during play: it opens
         * that menu instead of silently leaving this screen. */
        if (death_spectator_exit_requested)
            break;

        /* Use the normal game dispatcher.  Its Final Look guard rejects only
         * movement and gameplay actions; browsers, menus, overlays, pane
         * controls, and mouse recall all retain their in-game behavior. */
        process_command();
        handle_stuff();
        Term_fresh();

        if (death_spectator_exit_requested)
            break;

        /* Reset command state for the next iteration. */
        death_spectator_reset_command();
    }

    /* Ensure no residual actions are pending. */
    p_ptr->energy_use = 0;
    death_spectator_reset_command();
    sdl_mouse_path_cancel();
    death_spectator_leave_game_ui(&old_state);

    /* Drop the guard only after the real postmortem state is back in place. */
    death_spectator_mode = false;
    death_spectator_exit_requested = false;
}

bool death_spectator_active(void)
{
    return death_spectator_mode;
}

void death_spectator_request_exit(void)
{
    if (death_spectator_mode)
        death_spectator_exit_requested = true;
}
