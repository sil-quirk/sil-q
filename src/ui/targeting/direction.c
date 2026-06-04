#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "ui/targeting/targeting-internal.h"

/*
 * Takes a delta coordinates and returns a direction.
 * e.g. (1,0) is south, which is direction 2.
 */
int dir_from_delta(int deltay, int deltax)
{
    s16b dird[3][3] = { { 7, 8, 9 }, { 4, 5, 6 }, { 1, 2, 3 } };

    return (dird[deltay + 1][deltax + 1]);
}

/*
 * Gives the overall direction from point 1 to point 2.
 * Uses orthogonals when breaking ties.
 */
int rough_direction(int y1, int x1, int y2, int x2)
{
    int deltay = y2 - y1; // these represent the displacement
    int deltax = x2 - x1;

    int dy, dx; // these represent the direction

    // determine the main direction from the source to the target
    if (deltay == 0)
        dy = 0;
    else
        dy = (deltay > 0) ? 1 : -1;

    if (deltax == 0)
        dx = 0;
    else
        dx = (deltax > 0) ? 1 : -1;

    if ((deltax != 0) && (ABS(deltay) / ABS(deltax) >= 2))
        dx = 0;
    if ((deltay != 0) && (ABS(deltax) / ABS(deltay) >= 2))
        dy = 0;

    return (dir_from_delta(dy, dx));
}

static bool player_visual_facing_enabled(void)
{
    return op_ptr && mirror_player_tile_facing;
}

void player_set_visual_facing_dir(int dir)
{
    if (!p_ptr)
        return;

    if ((dir >= 1) && (dir <= 9))
        p_ptr->visual_facing_dir = (byte)dir;
}

void player_set_visual_facing_target(int y, int x)
{
    if (!p_ptr)
        return;

    player_set_visual_facing_dir(rough_direction(p_ptr->py, p_ptr->px, y, x));
}

static void player_refresh_visual_facing(void)
{
    if (!p_ptr || !character_dungeon || !player_visual_facing_enabled())
        return;
    if (!in_bounds(p_ptr->py, p_ptr->px))
        return;

    lite_spot(p_ptr->py, p_ptr->px);
    handle_stuff();
    Term_fresh();
}

void player_set_visual_facing_dir_immediate(int dir)
{
    player_set_visual_facing_dir(dir);
    player_refresh_visual_facing();
}

void player_set_visual_facing_target_immediate(int y, int x)
{
    player_set_visual_facing_target(y, x);
    player_refresh_visual_facing();
}

/*
 * Get an "aiming direction" (1,2,3,4,6,7,8,9 or 5) from the user.
 *
 * Return true if a direction was chosen, otherwise return false.
 *
 * The direction "5" is special, and means "use current target".
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", if it is set.
 *
 * Note that "Force Target", if set, will pre-empt user interaction,
 * if there is a usable target already set.
 *
 * If the range variable is 0, there is no range limit.
 *
 * Currently this function applies confusion directly.
 */
static char get_aim_vertical_up_key(void)
{
    return hjkl_movement ? 'c' : 'u';
}

static void get_aim_prompt(char* buf, size_t buflen, bool has_target,
    bool allow_vertical)
{
    char up_key;
    char fire_label[24];
    char confirm_label[24];
    char select_label[24];
    char up_label[24];
    char down_label[24];
    char cancel_label[24];

    if (!buf || !buflen)
        return;

    up_key = get_aim_vertical_up_key();

    if (!steamdeck_controls_active())
    {
        if (allow_vertical)
        {
            if (has_target)
            {
                strnfmt(buf, buflen,
                    "Aim: arrows/touch, f/Space target, * select, %c up, d down, Esc? ",
                    up_key);
            }
            else
            {
                strnfmt(buf, buflen,
                    "Aim: arrows/touch, f/Space closest, * select, %c up, d down, Esc? ",
                    up_key);
            }
        }
        else
        {
            if (has_target)
            {
                SDL_strlcpy(buf,
                    "Aim: arrows/touch, f/Space target, * select, Esc? ",
                    buflen);
            }
            else
            {
                SDL_strlcpy(buf,
                    "Aim: arrows/touch, f/Space closest, * select, Esc? ",
                    buflen);
            }
        }
        return;
    }

    target_prompt_label('f', "B", fire_label, sizeof(fire_label));
    target_prompt_label(INPUT_BIND_CONFIRM, "A", confirm_label,
        sizeof(confirm_label));
    target_prompt_label('s', "Y", select_label, sizeof(select_label));
    target_prompt_label(up_key, hjkl_movement ? "c" : "u", up_label,
        sizeof(up_label));
    target_prompt_label('d', "d", down_label, sizeof(down_label));
    target_prompt_label(ESCAPE, "Start", cancel_label, sizeof(cancel_label));

    if (has_target)
    {
        if (allow_vertical)
        {
            strnfmt(buf, buflen,
                "Aim (%s/%s target, %s select, %s up, %s down, %s cancel)? ",
                fire_label, confirm_label, select_label, up_label,
                down_label, cancel_label);
            if (strlen(buf) > 49)
                strnfmt(buf, buflen,
                    "Aim (%s/%s tgt, %s sel, %s up, %s down, %s cancel)? ",
                    fire_label, confirm_label, select_label, up_label,
                    down_label, cancel_label);
        }
        else
        {
            strnfmt(buf, buflen,
                "Aim (%s/%s target, %s select, %s cancel)? ", fire_label,
                confirm_label, select_label, cancel_label);
            if (strlen(buf) > 49)
                strnfmt(buf, buflen,
                    "Aim (%s/%s tgt, %s sel, %s cancel)? ", fire_label,
                    confirm_label, select_label, cancel_label);
        }
    }
    else
    {
        if (allow_vertical)
        {
            strnfmt(buf, buflen,
                "Aim (%s/%s closest, %s select, %s up, %s down, %s cancel)? ",
                fire_label, confirm_label, select_label, up_label,
                down_label, cancel_label);
            if (strlen(buf) > 49)
                strnfmt(buf, buflen,
                    "Aim (%s/%s close, %s sel, %s up, %s down, %s cancel)? ",
                    fire_label, confirm_label, select_label, up_label,
                    down_label, cancel_label);
        }
        else
        {
            strnfmt(buf, buflen,
                "Aim (%s/%s closest, %s select, %s cancel)? ", fire_label,
                confirm_label, select_label, cancel_label);
            if (strlen(buf) > 49)
                strnfmt(buf, buflen,
                    "Aim (%s/%s close, %s sel, %s cancel)? ", fire_label,
                    confirm_label, select_label, cancel_label);
        }
    }
}

static bool get_aim_com(cptr prompt, bool has_target, int range,
    bool allow_vertical, char* command)
{
    char up_key = get_aim_vertical_up_key();
    char ch;
    int clicked_choice = 0;

    message_flush();

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
    sdl_pointer_aim_begin(range, allow_vertical);
    prt(prompt, 0, 0);

    ui_menu_click_add_text_token('f', 0, 0, prompt,
        has_target ? "target" : "closest");
    ui_menu_click_add_text_token('*', 0, 0, prompt, "select");
    if (allow_vertical)
    {
        ui_menu_click_add_text_token(up_key, 0, 0, prompt, "up");
        ui_menu_click_add_text_token('d', 0, 0, prompt, "down");
    }
    ui_menu_click_add_text_token(ESCAPE, 0, 0, prompt, "Esc");
    ui_menu_click_add_text_token(ESCAPE, 0, 0, prompt, "cancel");

    ch = inkey();
    if ((ch == UI_MENU_CLICK_WAKE_KEY || ch == '\r' || ch == '\n')
        && ui_menu_click_take(&clicked_choice))
    {
        ch = (char)clicked_choice;
    }

    sdl_pointer_aim_end();
    ui_menu_click_clear();
    prt("", 0, 0);

    *command = ch;
    return (ch != ESCAPE);
}

static bool get_aim_dir_aux(int* dp, int range, bool allow_vertical)
{
    int dir;

    char ch;

    char prompt[80];

#ifdef ALLOW_REPEAT

    if (repeat_pull(dp))
    {
        /* Verify */
        if (!(*dp == 5 && !target_okay(range)))
        {
            return (true);
        }
        else
        {
            /* Invalid repeat - reset it */
            repeat_clear();
        }
    }

#endif /* ALLOW_REPEAT */

    /* Initialize */
    (*dp) = 0;

    /* Global direction */
    dir = p_ptr->command_dir;
    if ((dir == 5) && !target_okay(range))
        dir = 0;
    if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
        dir = 0;

    /* Hack -- auto-target if requested */
    //	if (use_old_target && target_okay(range)) dir = 5;

    /* Ask until satisfied */
    while (!dir)
    {
        bool has_target = target_okay(range);

        /* Choose a prompt */
        get_aim_prompt(prompt, sizeof(prompt), has_target, allow_vertical);

        /* Get a command (or Cancel) */
        if (!get_aim_com(prompt, has_target, range, allow_vertical, &ch))
            break;

        /* Analyze */
        switch (ch)
        {
        /* Set new target, use target if legal */
        case 's':
        case '*':
        {
            if (target_set_interactive(TARGET_KILL, range))
                dir = 5;
            break;
        }

        /* Use current target, if set and legal, otherwise pick next target */
        case 'f':
        case 'F':
        case 't':
        case '5':
        case 'z':
        case ' ':
        case '\r':
        case '\n':
        case INPUT_BIND_CONFIRM:
        {
            if (target_okay(range))
                dir = 5;
            else
            {
                /* Prepare the "temp" array */
                get_sorted_target_list(TARGET_KILL, range);

                /* Monster */
                if (temp_n)
                {
                    target_set_monster(cave_m_idx[temp_y[0]][temp_x[0]]);
                    health_track(cave_m_idx[temp_y[0]][temp_x[0]]);
                    dir = 5;
                }
            }
            break;
        }

        case 'u':
        case 'U':
        case 'c':
        case 'C':
        {
            if (allow_vertical
                && (tolower((unsigned char)ch) == get_aim_vertical_up_key()))
            {
                dir = DIRECTION_UP;
            }
            break;
        }

        case 'd':
        case 'D':
        {
            if (allow_vertical)
                dir = DIRECTION_DOWN;
            break;
        }

        case UI_MENU_CLICK_WAKE_KEY:
        {
            int mouse_command = 0;
            int mouse_dir = 0;

            if (sdl_pointer_aim_take_direction(&dir))
            {
                /* Direction supplied by a mouse or touch map target. */
            }
            else if (sdl_pointer_attack_take_command(&mouse_command, &mouse_dir)
                && ((mouse_command == 'f') || (mouse_command == 'F'))
                && mouse_dir)
            {
                dir = mouse_dir;
            }
            else
            {
                continue;
            }
            break;
        }

            // Sil-y: there is some chance that these UP and DOWN things
            //        will cause trouble elsewhere

        case '>':
        {
            dir = DIRECTION_DOWN;
            break;
        }
        case '<':
        {
            dir = DIRECTION_UP;
            break;
        }

        /* Possible direction */
        default:
        {
            dir = target_dir(ch);
            if ((dir == 5) && !target_okay(range))
                dir = 0;
            break;
        }
        }

        /* Error */
        if (!dir)
            bell("Illegal aim direction!");
    }

    /* No direction */
    if (!dir)
    {
        return (false);
    }

    /* Save the direction */
    p_ptr->command_dir = dir;

    /* Check for confusion */
    // Sil-y: Doesn't use the new confusion method, but might be difficult to
    // use it
    if ((dir != DIRECTION_UP) && (dir != DIRECTION_DOWN) && p_ptr->confused)
    {
        /* Randomish direction */
        dir = ddd[rand_int(8)];
    }

    /* Notice confusion */
    if (p_ptr->command_dir != dir)
    {
        /* Warn the user */
        msg_print("You are confused.");
    }

    /* Save direction */
    (*dp) = dir;

    if ((dir == 5) && target_okay(range))
        player_set_visual_facing_target_immediate(p_ptr->target_row, p_ptr->target_col);
    else
        player_set_visual_facing_dir_immediate(dir);

#ifdef ALLOW_REPEAT

    repeat_push(dir);

#endif /* ALLOW_REPEAT */

    /* A "valid" direction was entered */
    return (true);
}

bool get_aim_dir(int* dp, int range)
{
    return get_aim_dir_aux(dp, range, false);
}

bool get_aim_dir_vertical(int* dp, int range)
{
    return get_aim_dir_aux(dp, range, true);
}

/*
 * Request a "movement" direction (1,2,3,4,5,6,7,8,9) from the user.
 *
 * Return true if a direction was chosen, otherwise return false.
 *
 * Direction "0" is illegal and will not be accepted.
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", if it is set.
 */
bool get_rep_dir(int* dp)
{
    int dir;

    char ch;

    cptr p;

#ifdef ALLOW_REPEAT

    if (repeat_pull(dp))
    {
        return (true);
    }

#endif /* ALLOW_REPEAT */

    /* Initialize */
    (*dp) = 0;

    /* Global direction */
    dir = p_ptr->command_dir;

    /* Get a direction */
    while (!dir)
    {
        /* Choose a prompt */
        p = "Direction (Escape to cancel)? ";

        /* Get a command (or Cancel) */
        if (!get_com(p, &ch))
            break;

        /* Convert keypress into a direction */
        dir = target_dir(ch);

        /* Oops */
        if (!dir)
            bell("Illegal repeatable direction!");
    }

    /* Aborted */
    if (!dir)
        return (false);

    /* Save desired direction */
    p_ptr->command_dir = dir;

    /* Save direction */
    (*dp) = dir;

    player_set_visual_facing_dir_immediate(dir);

#ifdef ALLOW_REPEAT

    repeat_push(dir);

#endif /* ALLOW_REPEAT */

    /* Success */
    return (true);
}

/*
 * Apply confusion, if needed, to a direction
 *
 * Display a message and return true if direction changes.
 */
bool confuse_dir(int* dp)
{
    int dir;
    int i;

    /* Default */
    dir = (*dp);

    /* Apply "confusion" */
    if (p_ptr->confused)
    {
        /* If no direction given, then completely randomise it */
        if (dir == 5)
        {
            /* Random direction */
            dir = ddd[rand_int(8)];
        }
        else
        {
            // gives 3 chances to be turned left and 3 chances to be turned
            // right leads to a binomial distribution of direction around the
            // intended one:
            //
            // 15 20 15
            //  6     6   (chances are all out of 64)
            //  1  0  1

            i = damroll(3, 2) - damroll(3, 2);

            dir = cycle[chome[*dp] + i];
        }
    }

    /* Notice confusion */
    if ((*dp) != dir)
    {
        /* Warn the user */
        msg_print("You are confused.");

        /* Save direction */
        (*dp) = dir;

        /* Confused */
        return (true);
    }

    /* Not confused */
    return (false);
}
