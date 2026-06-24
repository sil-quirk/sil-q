#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "support/input.h"
#include "support/movement-input.h"
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

static bool monster_visual_facing_enabled(void)
{
    return op_ptr && mirror_monster_tile_facing;
}

void monster_set_visual_facing_dir(monster_type* m_ptr, int dir)
{
    if (!m_ptr)
        return;

    if ((dir >= 1) && (dir <= 9))
        m_ptr->visual_facing_dir = (byte)dir;
}

void monster_set_visual_facing_target(monster_type* m_ptr, int y, int x)
{
    if (!m_ptr)
        return;

    monster_set_visual_facing_dir(
        m_ptr, rough_direction(m_ptr->fy, m_ptr->fx, y, x));
}

static void monster_refresh_visual_facing(const monster_type* m_ptr)
{
    if (!p_ptr || !character_dungeon || !monster_visual_facing_enabled())
        return;
    if (!m_ptr || !m_ptr->ml || !in_bounds(m_ptr->fy, m_ptr->fx))
        return;

    lite_spot(m_ptr->fy, m_ptr->fx);
    handle_stuff();
    Term_fresh();
}

void monster_set_visual_facing_dir_immediate(monster_type* m_ptr, int dir)
{
    monster_set_visual_facing_dir(m_ptr, dir);
    monster_refresh_visual_facing(m_ptr);
}

void monster_set_visual_facing_target_immediate(monster_type* m_ptr, int y, int x)
{
    monster_set_visual_facing_target(m_ptr, y, x);
    monster_refresh_visual_facing(m_ptr);
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
 * When no direction was pre-supplied, the interactive aim selection
 * (target_select_aim) is opened: it centres on the closest target,
 * moves the selection with direction keys or mouse hover, and fires
 * on f/click/tap.
 *
 * If the range variable is 0, there is no range limit.
 *
 * Currently this function applies confusion directly.
 */
static bool get_aim_dir_aux(int* dp, int range, bool allow_vertical)
{
    int dir;

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

    /* Global direction (e.g. supplied by a pointer-attack map click) */
    dir = p_ptr->command_dir;
    if ((dir == 5) && !target_okay(range))
        dir = 0;
    if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
        dir = 0;

    /* No direction given: run the interactive aim selection */
    if (!dir)
    {
        if (!target_select_aim(range, allow_vertical, &dir))
            return (false);
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
 * Interactive "which adjacent grid?" selection shared by every command
 * that used to ask for a direction on the top message row.
 *
 * Each candidate grid is highlighted on the map and a small popup prompt
 * is shown next to the player (never the top message row).  The player
 * can click or tap a candidate, hover to move the highlight, cycle with
 * +/-, press the direction toward a candidate to pick it directly, or
 * confirm the highlight with Enter/Space.  Escape cancels.
 *
 * Returns true and stores the chosen direction in *dp; false on cancel.
 */
bool get_grid_choice_dir(cptr prompt, const int ys[], const int xs[],
    const int dirs[], int count, int* dp)
{
    int sel = 0;
    bool done = false;
    bool chosen = false;
    char query;

    if (!dp || (count <= 0))
        return false;

    message_flush();

    sdl_pointer_aim_select_begin(1, false);
    sdl_pointer_aim_select_set_choices(ys, xs, count, prompt);
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);

    /* request_command blanked the map's top row before this handler ran;
     * repaint the map so the highlights/popup sit over a clean view. */
    p_ptr->redraw |= (PR_MAP);

    while (!done)
    {
        sdl_pointer_aim_select_update(ys[sel], xs[sel]);
        handle_stuff();
        move_cursor_relative(ys[sel], xs[sel]);

        query = inkey_movement_context(
            MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT);

        if (query == UI_MENU_CLICK_WAKE_KEY)
        {
            int d = 0;

            if (movement_input_take_legacy_direction(
                    MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT, &d))
            {
                bool matched = false;

                for (int i = 0; i < count; i++)
                {
                    if (dirs[i] == d)
                    {
                        sel = i;
                        chosen = true;
                        done = true;
                        matched = true;
                        break;
                    }
                }

                if (!matched && (d == 5))
                {
                    chosen = true;
                    done = true;
                }
                else if (!matched)
                {
                    bell("Nothing that way.");
                }

                continue;
            }
        }

        /* Pointer events from the map: hover moves the highlight, a click or
         * tap on a candidate confirms it. */
        if (query == UI_MENU_CLICK_WAKE_KEY)
        {
            int kind = 0, ey = 0, ex = 0;

            if (!sdl_pointer_aim_select_take_event(&kind, &ey, &ex))
                continue;

            for (int i = 0; i < count; i++)
            {
                if ((ys[i] == ey) && (xs[i] == ex))
                {
                    sel = i;
                    if ((kind == AIM_SELECT_EVENT_CLICK)
                        || (kind == AIM_SELECT_EVENT_TAP))
                    {
                        chosen = true;
                        done = true;
                    }
                    break;
                }
            }
            continue;
        }

        switch (query)
        {
        case ESCAPE:
        case 'q':
            done = true;
            break;

        case '*':
        case '+':
            sel = (sel + 1) % count;
            break;

        case '-':
            sel = (sel + count - 1) % count;
            break;

        case ' ':
        case '\r':
        case '\n':
        case 't':
        case (char)INPUT_BIND_CONFIRM: /* truncate so it matches under unsigned char (ARM) */
            chosen = true;
            done = true;
            break;

        default:
        {
            int d = target_dir(query);

            if (d)
            {
                bool matched = false;

                /* Press the direction toward a candidate to pick it. */
                for (int i = 0; i < count; i++)
                {
                    if (dirs[i] == d)
                    {
                        sel = i;
                        chosen = true;
                        done = true;
                        matched = true;
                        break;
                    }
                }

                /* '5' with no self candidate confirms the highlight. */
                if (!matched && (d == 5))
                {
                    chosen = true;
                    done = true;
                }
                else if (!matched)
                {
                    bell("Nothing that way.");
                }
            }
            else
            {
                bell("Illegal direction.");
            }
            break;
        }
        }
    }

    sdl_object_tooltip_clear();
    sdl_pointer_aim_select_end();
    ui_menu_click_clear();
    verify_panel();
    handle_stuff();

    if (!chosen)
        return false;

    *dp = dirs[sel];
    return true;
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
 *
 * When no direction was pre-supplied, the adjacent squares are offered
 * through the interactive grid selection (click, tap, arrows or a
 * direction key) instead of the old top-row "Direction?" prompt.
 */
bool get_rep_dir(int* dp)
{
    int dir;

    int ys[9], xs[9], dirs[9];
    int count = 0;

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

    if (!dir)
        (void)movement_input_take_legacy_direction(
            MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT, &dir);

    /* Get a direction interactively (own square first, then neighbours) */
    if (!dir)
    {
        ys[count] = p_ptr->py;
        xs[count] = p_ptr->px;
        dirs[count] = 5;
        count++;

        for (int d = 0; d < 8; d++)
        {
            int yy = p_ptr->py + ddy_ddd[d];
            int xx = p_ptr->px + ddx_ddd[d];

            if (!in_bounds_fully(yy, xx))
                continue;

            ys[count] = yy;
            xs[count] = xx;
            dirs[count] = dir_from_delta(yy - p_ptr->py, xx - p_ptr->px);
            count++;
        }

        if (!get_grid_choice_dir("Which direction?", ys, xs, dirs, count,
                &dir))
        {
            dir = 0;
        }
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
