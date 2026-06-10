#include "angband.h"
#include "externs.h"
#include "sdl-config.h"
#include "ui/targeting/targeting-internal.h"

/*
 * Interactive aim selection used by the fire/throw/aim commands.
 *
 * Invoking an aim command (e.g. 'f') opens this UI with the selection
 * centred on the closest target.  Direction keys and mouse hover move the
 * selection between targets, 'x' toggles a manual mode where any square
 * can be selected, and f/Space/Enter, a left click, or a tap on the
 * already-selected square fires.  Escape cancels.
 *
 * Nothing is printed on the top line: the selected square is described by
 * the map tooltip popup and the available commands are shown as the
 * clickable button bar overlay at the bottom of the map.
 */

static char aim_select_up_key(void) { return hjkl_movement ? 'c' : 'u'; }

static int aim_select_prompt_row(void)
{
    int row;

    if (!Term || Term->hgt <= 0)
        return 0;

    row = ROW_MAP + SCREEN_HGT - 1;
    if (row < 0)
        row = 0;
    if (row >= Term->hgt)
        row = Term->hgt - 1;

    return row;
}

static void aim_select_show_prompt(bool manual, bool allow_vertical)
{
    char fire_label[24];
    char cancel_label[24];
    char fire_full[40];
    char cancel_full[40];
    char up_full[16];
    char up_tiny[8];
    char up_key = aim_select_up_key();

    /* Gamepad binding labels only when controller controls are active */
    if (steamdeck_controls_active())
    {
        target_prompt_label('f', "f", fire_label, sizeof(fire_label));
        target_prompt_label(ESCAPE, "Esc", cancel_label, sizeof(cancel_label));
    }
    else
    {
        SDL_strlcpy(fire_label, "f", sizeof(fire_label));
        SDL_strlcpy(cancel_label, "Esc", sizeof(cancel_label));
    }

    strnfmt(fire_full, sizeof(fire_full), "%s Fire", fire_label);
    strnfmt(cancel_full, sizeof(cancel_full), "%s Cancel", cancel_label);
    strnfmt(up_full, sizeof(up_full), "%c Up", up_key);
    strnfmt(up_tiny, sizeof(up_tiny), "%c", up_key);

    sdl_unified_look_prompt_begin(aim_select_prompt_row());
    sdl_unified_look_prompt_add(
        'f', fire_full, fire_full, fire_full, fire_label);
    sdl_unified_look_prompt_add('x', manual ? "x Targets" : "x Manual",
        manual ? "x Targets" : "x Manual", manual ? "x Tgt" : "x Man", "x");
    if (allow_vertical)
    {
        sdl_unified_look_prompt_add(up_key, up_full, up_full, up_full,
            up_tiny);
        sdl_unified_look_prompt_add('d', "d Down", "d Down", "d Down", "d");
    }
    sdl_unified_look_prompt_add(
        ESCAPE, cancel_full, cancel_full, "Esc", "Esc");
    sdl_unified_look_prompt_finish();
}

/*
 * Whether a plain location at (y,x) can be fired at: within range, in the
 * line of fire (or a wall, for horn effects), and reachable along the
 * projection path -- the same path test target_set_interactive uses.
 */
static bool aim_select_location_fireable(int adjusted_range, int y, int x)
{
    u16b path[MAX_RANGE];
    int max;
    int y2 = y;
    int x2 = x;
    int py = p_ptr->py;
    int px = p_ptr->px;

    if ((y == py) && (x == px))
        return false;
    if (distance(py, px, y, x) > adjusted_range)
        return false;
    if (!(cave_info[y][x] & (CAVE_FIRE | CAVE_WALL)))
        return false;

    max = project_path(path, adjusted_range, py, px, &y2, &x2,
        (PROJECT_THRU | PROJECT_INVISIPASS));

    if (max == 0)
        return true;

    return ((((GRID_Y(path[max - 1]) <= y) && (y <= py))
                || ((GRID_Y(path[max - 1]) >= y) && (y >= py)))
        && (((GRID_X(path[max - 1]) <= x) && (x <= px))
            || ((GRID_X(path[max - 1]) >= x) && (x >= px))));
}

/*
 * Try to accept (y,x) as the target: a targetable monster in range, a
 * fireable location, or (for wizards) any square.  Sets the target and
 * health tracking on success.
 */
static bool aim_select_try_confirm(int adjusted_range, int y, int x)
{
    int m_idx = cave_m_idx[y][x];

    if ((y == p_ptr->py) && (x == p_ptr->px))
        return false;

    if ((m_idx > 0) && target_able(m_idx)
        && (distance(p_ptr->py, p_ptr->px, y, x) <= adjusted_range))
    {
        health_track(m_idx);
        target_set_monster(m_idx);
        return true;
    }

    if (aim_select_location_fireable(adjusted_range, y, x)
        || (p_ptr->wizard && in_bounds_fully(y, x)))
    {
        target_set_location(y, x);
        health_track(0);
        return true;
    }

    return false;
}

static bool aim_select_find_in_list(int y, int x, int* m)
{
    int i;

    for (i = 0; i < temp_n; i++)
    {
        if ((temp_y[i] == y) && (temp_x[i] == x))
        {
            if (m)
                *m = i;
            return true;
        }
    }

    return false;
}

bool target_select_aim(int range, bool allow_vertical, int* dp)
{
    int adjusted_range = (range == 0) ? MAX_RANGE : range;
    int m = 0;
    int y = p_ptr->py;
    int x = p_ptr->px;
    int i;
    int chosen_dir = 0;
    bool manual = false;
    bool done = false;
    char up_key = aim_select_up_key();
    char query;

    message_flush();

    get_sorted_target_list(TARGET_KILL, range);

    /* Centre the selection on the closest target; with no targets in
     * view, fall back to manual selection from the player's square. */
    if (temp_n)
    {
        y = temp_y[0];
        x = temp_x[0];
    }
    else
    {
        manual = true;
    }

    sdl_pointer_aim_select_begin(range, allow_vertical);
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);

    while (!done)
    {
        int d = 0;

        /* In target mode the selection follows the list entry */
        if (!manual && temp_n)
        {
            if (m >= temp_n)
                m = temp_n - 1;
            y = temp_y[m];
            x = temp_x[m];
        }

        /* Mirror the state to the SDL overlays */
        sdl_pointer_aim_select_set_manual(manual);
        sdl_pointer_aim_select_update(y, x);
        aim_select_show_prompt(manual, allow_vertical);

        /* Track the selected monster (health bar, recall window) */
        if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
        {
            monster_race_track(mon_list[cave_m_idx[y][x]].r_idx);
            health_track(cave_m_idx[y][x]);
        }
        else
        {
            health_track(0);
        }
        handle_stuff();

        /* Describe the selected square with the tooltip popup */
        (void)sdl_object_tooltip_show_grid(y, x, false);

        move_cursor_relative(y, x);

        query = inkey();

        /* A click on the command bar arrives as a pending choice */
        if ((query == UI_MENU_CLICK_WAKE_KEY) || (query == '\r')
            || (query == '\n'))
        {
            int clicked = 0;

            if (ui_menu_click_take(&clicked))
                query = (char)clicked;
        }

        /* Pointer events from the map: hover moves the selection, a
         * click fires, a tap selects first and fires on the selected
         * square. */
        if (query == UI_MENU_CLICK_WAKE_KEY)
        {
            int kind = 0;
            int ey = 0;
            int ex = 0;

            if (!sdl_pointer_aim_select_take_event(&kind, &ey, &ex))
                continue;
            if (!in_bounds_fully(ey, ex))
                continue;

            if ((kind == AIM_SELECT_EVENT_HOVER)
                || ((kind == AIM_SELECT_EVENT_TAP)
                    && ((ey != y) || (ex != x))))
            {
                if (manual)
                {
                    y = ey;
                    x = ex;
                }
                else if (aim_select_find_in_list(ey, ex, &m))
                {
                    y = ey;
                    x = ex;
                }
                continue;
            }

            if (aim_select_try_confirm(adjusted_range, ey, ex))
            {
                y = ey;
                x = ex;
                chosen_dir = 5;
                done = true;
            }
            else
            {
                bell("No clear shot.");
            }
            continue;
        }

        switch (query)
        {
        case ESCAPE:
        case 'q':
        {
            done = true;
            break;
        }

        /* Toggle manual (any square) selection */
        case 'x':
        {
            if (manual)
            {
                int bd = 999;

                get_sorted_target_list(TARGET_KILL, range);

                m = 0;
                for (i = 0; i < temp_n; i++)
                {
                    int t = distance(y, x, temp_y[i], temp_x[i]);

                    if (t < bd)
                    {
                        m = i;
                        bd = t;
                    }
                }

                if (bd == 999)
                    bell("No targets in view.");
                else
                    manual = false;
            }
            else
            {
                manual = true;
            }
            break;
        }

        /* Cycle through the target list */
        case '*':
        case '+':
        {
            if (!manual && temp_n)
                m = (m + 1) % temp_n;
            break;
        }

        case '-':
        {
            if (!manual && temp_n)
                m = (m + temp_n - 1) % temp_n;
            break;
        }

        /* Fire at the selection */
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
            if (aim_select_try_confirm(adjusted_range, y, x))
            {
                chosen_dir = 5;
                done = true;
            }
            else
            {
                bell("No clear shot.");
            }
            break;
        }

        case 'd':
        case 'D':
        case '>':
        {
            if (allow_vertical)
            {
                chosen_dir = DIRECTION_DOWN;
                done = true;
            }
            else
            {
                bell("Illegal command for target mode!");
            }
            break;
        }

        default:
        {
            if (allow_vertical
                && ((query == '<')
                    || (tolower((unsigned char)query) == up_key)))
            {
                chosen_dir = DIRECTION_UP;
                done = true;
                break;
            }

            /* Extract a direction */
            d = target_dir(query);

            if (!d)
                bell("Illegal command for target mode!");

            break;
        }
        }

        /* Move the selection */
        if (d)
        {
            if (manual)
            {
                x += ddx[d];
                y += ddy[d];

                /* Slide into legality */
                if (x >= p_ptr->cur_map_wid - 1)
                    x--;
                else if (x <= 0)
                    x++;

                /* Slide into legality */
                if (y >= p_ptr->cur_map_hgt - 1)
                    y--;
                else if (y <= 0)
                    y++;

                /* Adjust panel if needed */
                if (adjust_panel(y, x))
                {
                    handle_stuff();
                    get_sorted_target_list(TARGET_KILL, range);
                }
            }
            else if (temp_n)
            {
                int old_y = temp_y[m];
                int old_x = temp_x[m];

                /* Find a new target in that direction */
                i = target_pick(old_y, old_x, ddy[d], ddx[d]);

                /* Scroll to find a target off-panel */
                if (i < 0)
                {
                    int old_wy = p_ptr->wy;
                    int old_wx = p_ptr->wx;

                    if (change_panel(d))
                    {
                        get_sorted_target_list(TARGET_KILL, range);

                        i = target_pick(old_y, old_x, ddy[d], ddx[d]);

                        /* Restore panel if needed */
                        if ((i < 0) && modify_panel(old_wy, old_wx))
                            get_sorted_target_list(TARGET_KILL, range);

                        handle_stuff();
                    }
                }

                if (i >= 0)
                    m = i;
            }
        }
    }

    /* Forget the target list */
    temp_n = 0;

    sdl_object_tooltip_clear();
    sdl_unified_look_prompt_clear();
    ui_menu_click_clear();
    sdl_pointer_aim_select_end();

    /* Leave health tracking on the actual target */
    if (p_ptr->target_set && (p_ptr->target_who > 0))
        health_track(p_ptr->target_who);
    else
        health_track(0);

    /* Recenter around player */
    verify_panel();

    handle_stuff();

    if (!chosen_dir)
        return (false);

    *dp = chosen_dir;

    return (true);
}
