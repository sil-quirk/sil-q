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
