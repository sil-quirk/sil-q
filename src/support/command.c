#include "angband.h"
#include "support/command.h"
#include "externs.h"
#include "support/input.h"
#include "support/message.h"
#include "support/movement-input.h"
#include "sdl-config.h"

/*
 * Hack -- special buffer to hold the action of the current keymap
 */
static char request_command_buffer[256];

static bool request_command_take_movement(void)
{
    movement_input_command command;
    char legacy_char;

    if (!movement_input_take_command(MOVEMENT_INPUT_CONTEXT_DUNGEON,
            &command))
    {
        return false;
    }

    legacy_char = movement_input_command_legacy_char(&command);
    if (!legacy_char)
        return false;

    p_ptr->command_cmd = legacy_char;
    p_ptr->command_dir = movement_input_command_legacy_dir(&command);
    return true;
}

/*
 * Request a command from the user.
 *
 * Sets p_ptr->command_cmd, p_ptr->command_dir, p_ptr->command_rep,
 * p_ptr->command_arg.  May modify p_ptr->command_new.
 *
 * Note that "caret" ("^") is treated specially, and is used to
 * allow manual input of control characters.  This can be used
 * on many machines to request repeated tunneling (Ctrl-H).
 *
 * Note that "backslash" is treated specially, and is used to bypass any
 * keymap entry for the following character.  This is useful for macros.
 *
 * Note that this command is used both in the dungeon and in
 * stores, and must be careful to work in both situations.
 *
 * Note that "p_ptr->command_new" may not work any more.  XXX XXX XXX
 */
void request_command(void)
{
    char ch;

    int mode;

    cptr act;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* No command yet */
    p_ptr->command_cmd = 0;

    /* No "argument" yet */
    p_ptr->command_arg = 0;

    /* No "direction" yet */
    p_ptr->command_dir = 0;

    /* Establish a clean baseline so a context leaked by a non-local exit
     * (e.g. an interrupted targeting/direction prompt) cannot persist. */
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);

    /* Get command */
    while (1)
    {
        if (active_narrative_banner_consumes_input())
        {
            msg_flag = false;
            inkey_flag = true;
            (void)inkey();
            if (ui_message_line_enabled())
                prt("", 0, 0);
            clear_active_narrative_banner();
            do_cmd_redraw();
            continue;
        }

        if (sdl_mouse_recall_process_pending())
            continue;
        if (sdl_log_pane_display_process_pending())
            continue;

        if (request_command_take_movement())
            break;

        {
            int mouse_command = 0;
            int mouse_dir = 0;

            if (sdl_pointer_attack_take_command(&mouse_command, &mouse_dir))
            {
                p_ptr->command_cmd = (char)mouse_command;
                p_ptr->command_dir = mouse_dir;
                break;
            }

            if (sdl_mouse_path_take_step_command(&mouse_command, &mouse_dir))
            {
                p_ptr->command_cmd = (char)mouse_command;
                p_ptr->command_dir = mouse_dir;
                break;
            }
        }

        /* Hack -- auto-commands */
        if (p_ptr->command_new)
        {
            /* Flush messages */
            message_flush();

            /* Use auto-command */
            ch = (char)p_ptr->command_new;

            /* Forget it */
            p_ptr->command_new = 0;
        }

        /* Get a keypress in "command" mode */
        else
        {
            /* Hack -- no flush needed */
            msg_flag = false;

            /* Activate "command mode" */
            inkey_flag = true;

            /* Get a command */
            ch = inkey_movement_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
        }

        if (sdl_mouse_recall_process_pending())
            continue;
        if (sdl_log_pane_display_process_pending())
            continue;

        if (request_command_take_movement())
            break;

        if (ch == UI_MENU_CLICK_WAKE_KEY)
        {
            int mouse_command = 0;
            int mouse_dir = 0;

            if (sdl_pointer_attack_take_command(&mouse_command, &mouse_dir))
            {
                p_ptr->command_cmd = (char)mouse_command;
                p_ptr->command_dir = mouse_dir;
                break;
            }

            if (sdl_mouse_path_take_step_command(&mouse_command, &mouse_dir))
            {
                p_ptr->command_cmd = (char)mouse_command;
                p_ptr->command_dir = mouse_dir;
                break;
            }

            continue;
        }
        else
        {
            sdl_mouse_path_cancel();
        }

        /* Clear top line */
        prt("", 0, 0);
        message_line_reset_column();

        /* Command Count */
        if (((ch == 'R') && !angband_keyset) || ((ch == '0') && angband_keyset))
        {
            int old_arg = p_ptr->command_arg;

            /* Reset */
            p_ptr->command_arg = 0;

            /* Begin the input */
            prt("Repeat how many times: ", 0, 0);

            /* Get a command count */
            while (1)
            {
                /* Get a new keypress */
                ch = inkey();

                /* Simple editing (delete or backspace) */
                if ((ch == 0x7F) || (ch == KTRL('H')))
                {
                    /* Delete a digit */
                    p_ptr->command_arg = p_ptr->command_arg / 10;

                    /* Show current count */
                    prt(format("Repeat how many times: %d", p_ptr->command_arg),
                        0, 0);
                }

                /* Actual numeric data */
                else if (isdigit((unsigned char)ch))
                {
                    /* Stop count at 9999 */
                    if (p_ptr->command_arg >= 1000)
                    {
                        /* Warn */
                        bell("Invalid repeat count!");

                        /* Limit */
                        p_ptr->command_arg = 9999;
                    }

                    /* Increase count */
                    else
                    {
                        /* Incorporate that digit */
                        p_ptr->command_arg = p_ptr->command_arg * 10 + D2I(ch);
                    }

                    /* Show current count */
                    prt(format("Repeat how many times: %d", p_ptr->command_arg),
                        0, 0);
                }

                /* Exit on "unusable" input */
                else
                {
                    break;
                }
            }

            /* Hack -- Handle "zero" */
            if (p_ptr->command_arg == 0)
            {
                /* Default to 99 */
                p_ptr->command_arg = 99;

                /* Show current count */
                prt(format("Repeat how many times: %d", p_ptr->command_arg), 0,
                    0);
            }

            /* Hack -- Handle "old_arg" */
            if (old_arg != 0)
            {
                /* Restore old_arg */
                p_ptr->command_arg = old_arg;

                /* Show current count */
                prt(format("Repeat how many times: %d", p_ptr->command_arg), 0,
                    0);
            }

            /* Hack -- white-space means "enter command now" */
            if ((ch == ' ') || (ch == '\n') || (ch == '\r'))
            {
                /* Get a real command */
                if (!get_com("Command: ", &ch))
                {
                    /* Clear count */
                    p_ptr->command_arg = 0;

                    /* Continue */
                    continue;
                }
            }
        }

        /* Allow "keymaps" to be bypassed */
        if (ch == '\\')
        {
            /* Get a real command */
            (void)get_com("Command: ", &ch);

            /* Hack -- bypass keymaps */
            if (!inkey_next_active())
                inkey_next_set("");
        }

        /* Allow "control chars" to be entered */
        if (ch == '^')
        {
            /* Get a new command and controlify it */
            if (get_com("Control: ", &ch))
                ch = KTRL(ch);
        }

        /* Look up applicable keymap */
        act = keymap_act[mode][(byte)(ch)];

        /* Apply keymap if not inside a keymap already */
        if (act && !inkey_next_active() && ((ch != ' ') || space_acts_as_comma))
        {
            /* Install the keymap */
            SDL_strlcpy(
                request_command_buffer, act, sizeof(request_command_buffer));

            /* Start using the buffer */
            inkey_next_set(request_command_buffer);

            /* Continue */
            continue;
        }

        /* Paranoia */
        if (ch == '\0')
            continue;

        /* Use command */
        p_ptr->command_cmd = ch;

        /* Done */
        break;
    }

    /* Hack -- erase the message line. */
    if (ui_message_line_enabled())
        prt("", 0, 0);
}
