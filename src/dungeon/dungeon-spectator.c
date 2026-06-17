/* File: dungeon/dungeon-spectator.c */

#include "angband.h"
#include "dungeon-internal.h"

bool death_spectator_command_allowed(int command)
{
    if (command == 0)
        return true;

    switch (command)
    {
    case ' ':
    case '\n':
    case '\r':
    case '\a':
    case '?':
    case '@':
    case 'h':
    case 'H':
    case 'i':
    case 'e':
    case 'x':
    case 'M':
    case 'L':
    case 'l':
    case 'm':
    case 'O':
    case ':':
    case 'j':
    case '~':
    case '[':
    case ']':
    case KTRL('E'):
    case KTRL('P'):
    case KTRL('Q'):
    case KTRL('R'):
    case ESCAPE:
        return true;
    default:
        return false;
    }
}

static bool death_spectator_continue_input(int command)
{
    if ((command == ' ') || (command == '\n') || (command == '\r'))
    {
        return true;
    }

    if (steamdeck_controls_active() && (command == steamdeck_confirm_key()))
        return true;

    return false;
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
        "You linger for a final look. Press Esc, Space, or Enter to continue to the tomb.");
}

void death_spectator_view(void)
{
    death_spectator_mode = true;
    death_spectator_exit_requested = false;
    sdl_mouse_path_cancel();

    /* Clear any queued commands from the main loop. */
    p_ptr->command_cmd = 0;
    p_ptr->command_new = 0;
    p_ptr->command_rep = 0;
    p_ptr->command_arg = 0;
    p_ptr->command_dir = 0;

    /* Prevent lingering keypresses from auto-triggering commands. */
    flush();

    death_spectator_prepare_display();

    while (true)
    {
        request_command();

        if ((p_ptr->command_cmd == ESCAPE)
            || death_spectator_continue_input(p_ptr->command_cmd))
        {
            break;
        }

        if (!death_spectator_command_allowed(p_ptr->command_cmd))
        {
            if (p_ptr->command_cmd)
            {
                msg_print("You can no longer take that action.");
            }
            p_ptr->command_cmd = 0;
            continue;
        }

        process_command();
        handle_stuff();

        if (death_spectator_exit_requested)
            break;

        /* Reset command state for the next iteration. */
        p_ptr->command_cmd = 0;
        p_ptr->command_new = 0;
        p_ptr->command_rep = 0;
        p_ptr->command_arg = 0;
        p_ptr->command_dir = 0;
    }

    death_spectator_mode = false;
    death_spectator_exit_requested = false;

    /* Ensure no residual actions are pending. */
    p_ptr->energy_use = 0;
    p_ptr->command_cmd = 0;
    p_ptr->command_new = 0;
    p_ptr->command_rep = 0;
    p_ptr->command_arg = 0;
    p_ptr->command_dir = 0;
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
