/* File: spell/spell-projection.c */

#include "angband.h"
#include "externs.h"
#include "spell/spell-projection-internal.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

/*
 * Calculate and store the arcs used to make starbursts.
 */
static void calc_starburst(
    int height, int width, byte* arc_first, byte* arc_dist, int* arc_num)
{
    int i;
    int size, dist, vert_factor;
    int degree_first, center_of_arc;

    /* Note the "size" */
    size = 2 + div_round(width + height, 22);

    /* Ask for a reasonable number of arcs. */
    *arc_num = 8 + (height * width / 80);
    *arc_num = rand_spread(*arc_num, 3);
    if (*arc_num < 8)
        *arc_num = 8;
    if (*arc_num > 45)
        *arc_num = 45;

    /* Determine the start degrees and expansion distance for each arc. */
    for (degree_first = 0, i = 0; i < *arc_num; i++)
    {
        /* Get the first degree for this arc (using 180-degree circles). */
        arc_first[i] = degree_first;

        /* Get a slightly randomized start degree for the next arc. */
        degree_first += div_round(180, *arc_num);

        /* Do not entirely leave the usual range */
        if (degree_first < 180 * (i + 1) / *arc_num)
            degree_first = 180 * (i + 1) / *arc_num;
        if (degree_first > (180 + *arc_num) * (i + 1) / *arc_num)
            degree_first = (180 + *arc_num) * (i + 1) / *arc_num;

        /* Get the center of the arc (convert from 180 to 360 circle). */
        center_of_arc = degree_first + arc_first[i];

        /* Get arc distance from the horizontal (0 and 180 degrees) */
        if (center_of_arc <= 90)
            vert_factor = center_of_arc;
        else if (center_of_arc >= 270)
            vert_factor = ABS(center_of_arc - 360);
        else
            vert_factor = ABS(center_of_arc - 180);

        /*
         * Usual case -- Calculate distance to expand outwards.  Pay more
         * attention to width near the horizontal, more attention to height
         * near the vertical.
         */
        dist = ((height * vert_factor) + (width * (90 - vert_factor))) / 90;

        /* Randomize distance (should never be greater than radius) */
        arc_dist[i] = rand_range(dist / 4, dist / 2);

        /* Keep variability under control (except in special cases). */
        if ((dist != 0) && (i != 0))
        {
            int diff = arc_dist[i] - arc_dist[i - 1];

            if (ABS(diff) > size)
            {
                if (diff > 0)
                    arc_dist[i] = arc_dist[i - 1] + size;
                else
                    arc_dist[i] = arc_dist[i - 1] - size;
            }
        }
    }

    /* Neaten up final arc of circle by comparing it to the first. */
    if (true)
    {
        int diff = arc_dist[*arc_num - 1] - arc_dist[0];

        if (ABS(diff) > size)
        {
            if (diff > 0)
                arc_dist[*arc_num - 1] = arc_dist[0] + size;
            else
                arc_dist[*arc_num - 1] = arc_dist[0] - size;
        }
    }
}

/*
 * Generic "beam"/"bolt"/"ball" projection routine.
 *
 * Input:
 *   who: Index of "source" monster (negative for "player")
 *   rad: Radius of explosion (0 = beam/bolt, 1 to 9 = ball)
 *   y,x: Target location (or location to travel "towards")
 *   dam: Base damage roll to apply to affected monsters (or player)
 *   typ: Type of damage to apply to monsters (and objects)
 *   flg: Extra bit flags (see PROJECT_xxxx in "defines.h")
 *   degrees: How wide an arc spell is (in degrees).
 *   uniform: uniform means no damage reduction with range, otherwise it is one
 * die per square.
 *
 * Return:
 *   true if any "effects" of the projection were observed, else false
 *
 * At present, there are five major types of projections:
 *
 * Point-effect projection:  (no PROJECT_BEAM flag, radius of zero, and either
 *   jumps directly to target or has a single source and target grid)
 * A point-effect projection has no line of projection, and only affects one
 *   grid.  It is used for most area-effect spells (like dispel evil) and
 *   pinpoint strikes.
 *
 * Bolt:  (no PROJECT_BEAM flag, radius of zero, has to travel from source to
 *   target)
 * A bolt travels from source to target and affects only the final grid in its
 *   projection path.  If given the PROJECT_STOP flag, it is stopped by any
 *   monster or character in its path (at present, all bolts use this flag).
 *
 * Beam:  (PROJECT_BEAM)
 * A beam travels from source to target, affecting all grids passed through
 *   with full damage.  It is never stopped by monsters in its path.  Beams
 *   may never be combined with any other projection type.
 *
 * Ball:  (positive radius, unless the PROJECT_ARC flag is set)
 * A ball travels from source towards the target, and always explodes.  Unless
 *   specified, it does not affect wall grids, but otherwise affects any grids
 *   in LOS from the center of the explosion.
 * If used with a direction, a ball will explode on the first occupied grid in
 *   its path.  If given a target, it will explode on that target.  If a
 *   wall is in the way, it will explode against the wall.  If a ball reaches
 *   MAX_RANGE without hitting anything or reaching its target, it will
 *   explode at that point.
 *
 * Arc:  (positive radius, with the PROJECT_ARC flag set)
 * An arc is a portion of a source-centered ball that explodes outwards
 *   towards the target grid.  Like a ball, it affects all non-wall grids in
 *   LOS of the source in the explosion area.  The width of arc spells is con-
 *   trolled by degrees.
 * An arc is created by rejecting all grids that form the endpoints of lines
 *   whose angular difference (in degrees) from the centerline of the arc is
 *   greater than one-half the input "degrees".  See the table "get_
 *   angle_to_grid" in support/geometry.c for more information.
 * Note:  An arc with a value for degrees of zero is actually a beam of
 *   defined length.
 *
 * Projections that affect all monsters in LOS are handled through the use
 *   of "project_los()", which applies a single-grid projection to individual
 *   monsters.  Projections that light up rooms or affect all monsters on the
 *   level are more efficiently handled through special functions.
 *
 *
 * Variations:
 *
 * PROJECT_STOP forces a path of projection to stop at the first occupied
 *   grid it hits.  This is used with bolts, and also by ball spells
 *   travelling in a specific direction rather than towards a target.
 *
 * PROJECT_THRU allows a path of projection towards a target to continue
 *   past that target.
 *
 * PROJECT_JUMP allows a projection to immediately set the source of the pro-
 *   jection to the target.  This is used for all area effect spells (like
 *   dispel evil), and can also be used for bombardments.
 *
 * PROJECT_WALL allows a projection, not just to affect one layer of any
 *   passable wall (rubble, trees), but to affect the surface of any wall.
 *   Certain projection types always have this flag.
 *
 * PROJECT_PASS allows projections to ignore walls completely.
 *   Certain projection types always have this flag.
 *
 * PROJECT_HIDE erases all graphical effects, making the projection
 *   invisible.
 *
 * PROJECT_GRID allows projections to affect terrain features.
 *
 * PROJECT_ITEM allows projections to affect objects on the ground.
 *
 * PROJECT_KILL allows projections to affect monsters.
 *
 * PROJECT_PLAY allows projections to affect the player.
 *
 * degrees controls the width of arc spells.  With a value for
 *   degrees of zero, arcs act like beams of defined length.
 *
 * Implementation notes:
 *
 * If the source grid is not the same as the target, we project along the path
 *   between them.  Bolts stop if they hit anything, beams stop if they hit a
 *   wall, and balls and arcs may exhibit either behavior.  When they reach
 *   the final grid in the path, balls and arcs explode.  We do not allow beams
 *   to be combined with explosions.
 * Balls affect all floor grids in LOS (optionally, also wall grids adjacent
 *   to a grid in LOS) within their radius.  Arcs do the same, but only within
 *   their cone of projection.
 * Because affected grids are only scanned once, and it is really helpful to
 *   have explosions that travel outwards from the source, they are sorted by
 *   distance.  For each distance, an adjusted damage is calculated.
 * In successive passes, the code then displays explosion graphics, erases
 *   these graphics, marks terrain for possible later changes, affects
 *   objects, monsters, the character, and finally changes features and
 *   teleports monsters and characters in marked grids.
 *
 *
 * Usage and graphics notes:
 *
 * If the option "fresh_before" is on, or the delay factor is anything other
 * than zero, bolt and explosion pictures will be momentarily shown on screen.
 *
 * Only 256 grids can be affected per projection, limiting the effective
 * radius of standard ball attacks to nine units (diameter nineteen).  Arcs
 * can have larger radii; an arc capable of going out to range 20 should not
 * be wider than 70 degrees.
 *
 * Balls must explode BEFORE hitting walls, or they would affect monsters on
 * both sides of a wall.
 *
 * Note that for consistency, we pretend that the bolt actually takes time
 * to move from point A to point B, even if the player cannot see part of the
 * projection path.  Note that in general, the player will *always* see part
 * of the path, since it either starts at the player or ends on the player.
 *
 * Hack -- we assume that every "projection" is "self-illuminating".
 *
 * Hack -- when only a single monster is affected, we automatically track
 * (and recall) that monster, unless "PROJECT_JUMP" is used.
 *
 * Note that we must call "handle_stuff()" after affecting terrain features
 * in the blast radius, in case the illumination of the grid was changed,
 * and "update_view()" and "update_monsters()" need to be called.
 */
bool project(int who, int rad, int y0, int x0, int y1, int x1, int dd, int ds,
    int dif, int typ, u32b flg, int degrees, bool uniform)
{
    int i, j, k;
    int dist = 0;

    u32b dam_temp;
    int centerline = 0;

    int y = y0;
    int x = x0;
    int n1y = 0;
    int n1x = 0;
    int y2, x2;

    int msec = op_ptr->delay_factor * op_ptr->delay_factor;

    /* Assume the player sees nothing */
    bool notice = false;

    /* Assume the player has seen nothing */
    bool visual = false;

    /* Assume the player has seen no blast grids */
    bool drawn = false;

    /* Is the player blind? */
    bool blind = (p_ptr->blind ? true : false);

    /* Number of grids in the "path" */
    int path_n = 0;

    /* Actual grids in the "path" */
    u16b path_g[512];

    /* Number of grids in the "blast area" (including the "beam" path) */
    int grids = 0;

    /* Coordinates of the affected grids */
    byte gx[256], gy[256];

    /* Distance to each of the affected grids. */
    byte gd[256];

    /* Precalculated damage values for each distance. */
    int dam_at_dist[MAX_RANGE + 1];

    /*
     * Starburst projections only --
     * Holds first degree of arc, maximum effect distance in arc.
     */
    byte arc_first[45];
    byte arc_dist[45];

    /* Number (max 45) of arcs. */
    int arc_num = 0;

    int degree, max_dist;

    /* Hack -- Flush any pending output */
    handle_stuff();

    /* Make certain that the radius is not too large */
    if (rad > MAX_SIGHT)
        rad = MAX_SIGHT;

    /* Some projection types always PROJECT_WALL. */
    if ((typ == GF_KILL_WALL) || (typ == GF_KILL_DOOR))
    {
        flg |= (PROJECT_WALL);
    }

    /* Hack -- Jump to target, but require a valid target */
    if ((flg & (PROJECT_JUMP)) && (y1) && (x1))
    {
        y0 = y1;
        x0 = x1;

        /* Clear the flag */
        flg &= ~(PROJECT_JUMP);
    }

    /* If a single grid is both source and destination, store it. */
    if ((x1 == x0) && (y1 == y0))
    {
        gy[grids] = y0;
        gx[grids] = x0;
        gd[grids++] = 0;
    }

    /* Otherwise, unless an arc or a star, travel along the projection path. */
    else if (!(flg & (PROJECT_ARC | PROJECT_STAR)))
    {
        /* Determine maximum length of projection path */
        if (flg & (PROJECT_BOOM))
            dist = MAX_RANGE;
        else if (rad <= 0)
            dist = MAX_RANGE;
        else
            dist = rad;

        /* Calculate the projection path */
        path_n = project_path(path_g, dist, y0, x0, &y1, &x1, flg);

        /* Project along the path */
        for (i = 0; i < path_n; ++i)
        {
            int oy = y;
            int ox = x;

            int ny = GRID_Y(path_g[i]);
            int nx = GRID_X(path_g[i]);

            /* Hack -- Balls explode before reaching walls. */
            if ((flg & (PROJECT_BOOM)) && (!cave_floor_bold(ny, nx)))
            {
                break;
            }

            /* Advance */
            y = ny;
            x = nx;

            /* If a beam, collect all grids in the path. */
            if (flg & (PROJECT_BEAM))
            {
                gy[grids] = y;
                gx[grids] = x;
                gd[grids++] = 0;
            }

            /* Otherwise, collect only the final grid in the path. */
            else if (i == path_n - 1)
            {
                gy[grids] = y;
                gx[grids] = x;
                gd[grids++] = 0;
            }

            /* Only do visuals if requested */
            if (!blind && !(flg & (PROJECT_HIDE)))
            {
                /* Only do visuals if the player can "see" the projection */
                if (panel_contains(y, x) && player_has_los_bold(y, x))
                {
                    u16b p;

                    byte a;
                    char c;

                    /* Obtain the bolt pict */
                    p = bolt_pict(oy, ox, y, x, typ);

                    /* Extract attr/char */
                    a = PICT_A(p);
                    c = PICT_C(p);

                    /* Display the visual effects */
                    print_rel(c, a, y, x);
                    move_cursor_relative(y, x);
                    if (op_ptr->delay_factor)
                        Term_fresh();

                    /* Delay */
                    Term_xtra(TERM_XTRA_DELAY, msec);

                    /* Erase the visual effects */
                    lite_spot(y, x);
                    if (op_ptr->delay_factor)
                        Term_fresh();

                    /* Re-display the beam  XXX */
                    if (flg & (PROJECT_BEAM))
                    {
                        /* Obtain the explosion pict */
                        p = bolt_pict(y, x, y, x, typ);

                        /* Extract attr/char */
                        a = PICT_A(p);
                        c = PICT_C(p);

                        /* Visual effects */
                        print_rel(c, a, y, x);
                    }

                    /* Hack -- Activate delay */
                    visual = true;
                }

                /* Hack -- Always delay for consistency */
                else if (visual)
                {
                    /* Delay for consistency */
                    Term_xtra(TERM_XTRA_DELAY, msec);
                }
            }
        }
    }

    /* Save the "blast epicenter" */
    y2 = y;
    x2 = x;

    /* Beams have already stored all the grids they will affect. */
    if (flg & (PROJECT_BEAM))
    {
        /* No special actions */
    }

    /* Handle explosions */
    else if (flg & (PROJECT_BOOM))
    {
        /* Some projection types always PROJECT_WALL. */
        if (typ == GF_ACID)
        {
            /* Note that acid only affects monsters if it melts the wall. */
            flg |= (PROJECT_WALL);
        }

        /* Pre-calculate some things for starbursts. */
        if (flg & (PROJECT_STAR))
        {
            calc_starburst(
                1 + rad * 2, 1 + rad * 2, arc_first, arc_dist, &arc_num);

            /* Mark the area nearby -- limit range, ignore rooms */
            spread_cave_temp(y0, x0, rad, false);
        }

        /* Pre-calculate some things for arcs. */
        if (flg & (PROJECT_ARC))
        {
            /* The radius of arcs cannot be more than 20 */
            if (rad > 20)
                rad = 20;

            /* Reorient the grid forming the end of the arc's centerline. */
            n1y = y1 - y0 + 20;
            n1x = x1 - x0 + 20;

            /* Correct overly large or small values */
            if (n1y > 40)
                n1y = 40;
            if (n1x > 40)
                n1x = 40;
            if (n1y < 0)
                n1y = 0;
            if (n1x < 0)
                n1x = 0;

            /* Get the angle of the arc's centerline */
            centerline = 90 - get_angle_to_grid[n1y][n1x];
        }

        /*
         * If the center of the explosion hasn't been
         * saved already, save it now.
         */
        if (grids == 0)
        {
            gy[grids] = y2;
            gx[grids] = x2;
            gd[grids++] = 0;
        }

        /*
         * Scan every grid that might possibly
         * be in the blast radius.
         */
        for (y = y2 - rad; y <= y2 + rad; y++)
        {
            for (x = x2 - rad; x <= x2 + rad; x++)
            {
                /* Center grid has already been stored. */
                if ((y == y2) && (x == x2))
                    continue;

                /* Precaution: Stay within area limit. */
                if (grids >= 255)
                    break;

                /* Ignore "illegal" locations */
                if (!in_bounds(y, x))
                    continue;

                /* This is a wall grid (whether passable or not). */
                if (!cave_floor_bold(y, x))
                {
                    /* Spell with PROJECT_PASS ignore walls */
                    if (!(flg & (PROJECT_PASS)))
                    {
                        /* This grid is passable, or PROJECT_WALL is active */
                        if ((flg & (PROJECT_WALL)) || (cave_floor_bold(y, x)))
                        {
                            /* Allow grids next to grids in LOS of explosion
                             * center */
                            for (i = 0, k = 0; i < 8; i++)
                            {
                                int yy = y + ddy_ddd[i];
                                int xx = x + ddx_ddd[i];

                                /* Stay within dungeon */
                                if (!in_bounds(yy, xx))
                                    continue;

                                if (los(y2, x2, yy, xx))
                                {
                                    k++;
                                    break;
                                }
                            }

                            /* Require at least one adjacent grid in LOS */
                            if (!k)
                                continue;
                        }

                        /* We can't affect this non-passable wall */
                        else
                            continue;
                    }
                }

                /* Must be within maximum distance. */
                dist = (distance(y2, x2, y, x));
                if (dist > rad)
                    continue;

                /* Projection is a starburst */
                if (flg & (PROJECT_STAR))
                {
                    /* Grid is within effect range */
                    if (cave_info[y][x] & (CAVE_TEMP))
                    {
                        /* Reorient current grid for table access. */
                        int ny = y - y2 + 20;
                        int nx = x - x2 + 20;

                        /* Illegal table access is bad. */
                        if ((ny < 0) || (ny > 40) || (nx < 0) || (nx > 40))
                            continue;

                        /* Get angle to current grid. */
                        degree = get_angle_to_grid[ny][nx];

                        /* Scan arcs to find the one that applies here. */
                        for (i = arc_num - 1; i >= 0; i--)
                        {
                            if (arc_first[i] <= degree)
                            {
                                max_dist = arc_dist[i];

                                /* Must be within effect range. */
                                if (max_dist >= dist)
                                {
                                    gy[grids] = y;
                                    gx[grids] = x;
                                    gd[grids] = 0;
                                    grids++;
                                }

                                /* Arc found.  End search */
                                break;
                            }
                        }
                    }
                }

                /* Use angle comparison to delineate an arc. */
                else if (flg & (PROJECT_ARC))
                {
                    int n2y, n2x, tmp, diff;

                    /* Reorient current grid for table access. */
                    n2y = y - y2 + 20;
                    n2x = x - x2 + 20;

                    /*
                     * Find the angular difference (/2) between
                     * the lines to the end of the arc's center-
                     * line and to the current grid.
                     */
                    tmp = ABS(get_angle_to_grid[n2y][n2x] + centerline) % 180;
                    diff = ABS(90 - tmp);

                    /*
                     * If difference is not greater then that
                     * allowed, and the grid is in LOS, accept it.
                     */
                    if (diff < (degrees + 6) / 4)
                    {
                        if (los(y2, x2, y, x))
                        {
                            gy[grids] = y;
                            gx[grids] = x;
                            gd[grids] = dist;
                            grids++;
                        }
                    }
                }

                /* Standard ball spell -- accept all grids in LOS. */
                else
                {
                    if (flg & (PROJECT_PASS) || los(y2, x2, y, x))
                    {
                        gy[grids] = y;
                        gx[grids] = x;
                        gd[grids] = dist;
                        grids++;
                    }
                }
            }
        }
    }

    /* Clear the "temp" array  XXX */
    clear_temp_array();

    /* Calculate and store the actual damage at each distance. */
    for (i = 0; i <= MAX_RANGE; i++)
    {
        /* No damage outside the radius. */
        if (i > rad)
            dam_temp = 0;

        /* No damage reduction with range if uniform. */
        else if (uniform)
        {
            dam_temp = dd;
        }

        /* Otherwise, lose two dice per square. */
        else
        {
            if (dd > 2 * i)
                dam_temp = dd - 2 * i;
            else
                dam_temp = 0;
        }

        /* Store it. */
        dam_at_dist[i] = dam_temp;
    }

    /* Sort the blast grids by distance, starting at the origin. */
    for (i = 0, k = 0; i < rad; i++)
    {
        int tmp_y, tmp_x, tmp_d;

        /* Collect all the grids of a given distance together. */
        for (j = k; j < grids; j++)
        {
            if (gd[j] == i)
            {
                tmp_y = gy[k];
                tmp_x = gx[k];
                tmp_d = gd[k];

                gy[k] = gy[j];
                gx[k] = gx[j];
                gd[k] = gd[j];

                gy[j] = tmp_y;
                gx[j] = tmp_x;
                gd[j] = tmp_d;

                /* Write to next slot */
                k++;
            }
        }
    }

    /* Display the "blast area" if allowed */
    if (!blind && !(flg & (PROJECT_HIDE)))
    {
        /* Do the blast from inside out */
        for (i = 0; i < grids; i++)
        {
            /* Extract the location */
            y = gy[i];
            x = gx[i];

            /* Only do visuals if the player can "see" the blast */
            if (panel_contains(y, x) && player_has_los_bold(y, x))
            {
                u16b p;

                byte a;
                char c;

                drawn = true;

                /* Obtain the explosion pict */
                p = bolt_pict(y, x, y, x, typ);

                /* Extract attr/char */
                a = PICT_A(p);
                c = PICT_C(p);

                /* Visual effects -- Display */
                print_rel(c, a, y, x);
            }

            /* Hack -- center the cursor */
            move_cursor_relative(y2, x2);

            /* New radius is about to be drawn */
            if ((i == grids - 1) || ((i < grids - 1) && (gd[i + 1] > gd[i])))
            {
                /* Flush each radius separately */
                if (op_ptr->delay_factor)
                    Term_fresh();

                /* Delay (efficiently) */
                if (visual || drawn)
                {
                    Term_xtra(TERM_XTRA_DELAY, msec);
                }
            }
        }

        /* Delay for a while if there are pretty graphics to show */
        if ((grids > 1) && (visual || drawn))
        {
            if (!op_ptr->delay_factor)
                Term_fresh();
            Term_xtra(TERM_XTRA_DELAY, 50 + msec);
        }

        /* Flush the erasing -- except if we specify lingering graphics */
        if ((drawn) && (!(flg & (PROJECT_NO_REDRAW))))
        {
            /* Erase the explosion drawn above */
            for (i = 0; i < grids; i++)
            {
                /* Extract the location */
                y = gy[i];
                x = gx[i];

                /* Hack -- Erase if needed */
                if (panel_contains(y, x) && player_has_los_bold(y, x))
                {
                    lite_spot(y, x);
                }
            }

            /* Hack -- center the cursor */
            move_cursor_relative(y2, x2);

            /* Flush the explosion */
            if (op_ptr->delay_factor)
                Term_fresh();
        }
    }

    /* Check features */
    if (flg & (PROJECT_GRID))
    {
        /* Scan for features */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Affect the feature in that grid */
            if (project_f(who, y, x, gd[i], dam_at_dist[gd[i]], ds, dif, typ))
                notice = true;
        }
    }

    /* Check objects */
    if (flg & (PROJECT_ITEM))
    {
        /* Scan for objects */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Affect the object in the grid */
            if (project_o(who, y, x, dam_at_dist[gd[i]], ds, dif, typ))
                notice = true;
        }
    }

    /* Check monsters */
    if (flg & (PROJECT_KILL))
    {
        /* Mega-Hack */
        project_m_n = 0;
        project_m_x = 0;
        project_m_y = 0;
        death_count = 0;

        /* Scan for monsters */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Affect the monster in the grid */
            if (project_m(who, y, x, dam_at_dist[gd[i]], ds, dif, typ, flg))
                notice = true;
        }

        /* Player affected one monster (without "jumping") */
        if ((who < 0) && (project_m_n == 1) && !(flg & (PROJECT_JUMP)))
        {
            /* Location */
            x = project_m_x;
            y = project_m_y;

            /* Track if possible */
            if (cave_m_idx[y][x] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];

                /* Hack -- auto-recall */
                if (m_ptr->ml)
                    monster_race_track(m_ptr->r_idx);

                /* Hack - auto-track */
                // Sil-y: turned this off experimentally
                // if (m_ptr->ml) health_track(cave_m_idx[y][x]);
            }
        }

        /* Hack -- Moria-style death messages for non-visible monsters */
        if (death_count)
        {
            /* One monster */
            if (death_count == 1)
            {
                msg_print("You hear a scream of agony!");
            }

            /* Several monsters */
            else
            {
                msg_print("You hear several screams of agony!");
            }

            /* Reset */
            death_count = 0;
        }
    }

    /* Check player */
    if (flg & (PROJECT_PLAY))
    {
        /* Scan for player */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Player is in this grid */
            if (cave_m_idx[y][x] < 0)
            {
                /* Affect the player */
                if (project_p(who, y, x, dam_at_dist[gd[i]], ds, dif, typ))
                {
                    notice = true;

                    /* Only affect the player once */
                    break;
                }
            }
        }
    }

    /* Clear the "temp" array  (paranoia is good) */
    clear_temp_array();

    /* Update stuff if needed */
    if (p_ptr->update)
        update_stuff();

    /* Return "something was noticed" */
    return (notice);
}

bool project_bolt(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg)
{
    /* Add the bolt bitflags */
    flg |= PROJECT_STOP | PROJECT_KILL | PROJECT_THRU;

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= PROJECT_PLAY;

    /* Limit range */
    if ((rad > MAX_RANGE) || (rad <= 0))
        rad = MAX_RANGE;

    /* Cast a bolt */
    return (project(who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, 0, false));
}

/*
 * Handle beam spells.
 *
 * Beams affect every grid they touch, go right through monsters, and
 * (almost) never affect items on the floor.
 */
bool project_beam(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg)
{
    /* Add the beam bitflags */
    flg |= PROJECT_BEAM | PROJECT_KILL | PROJECT_THRU;

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= (PROJECT_PLAY);

    /* Limit range */
    if ((rad > MAX_RANGE) || (rad <= 0))
        rad = MAX_RANGE;

    /* Cast a beam */
    return (project(who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, 0, false));
}

/*
 * Handle ball spells.
 *
 * Balls act like bolt spells, except that they do not pass their target,
 * and explode when they hit a monster, a wall, their target, or the edge
 * of sight.  Within the explosion radius, they affect items on the floor.
 *
 * Balls may jump to the target, and have any source diameter (which affects
 * how quickly their damage falls off with distance from the center of the
 * explosion).
 */
bool project_ball(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, bool uniform)
{
    /* Add the ball bitflags */
    flg |= PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;

    /* Add the STOP flag if appropriate */
    if ((who < 0)
        && (!target_okay(0) || y1 != p_ptr->target_row
            || x1 != p_ptr->target_col))
    {
        flg |= (PROJECT_STOP);
    }

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= (PROJECT_PLAY);

    /* Limit radius to nine (up to 256 grids affected) */
    if (rad > 9)
        rad = 9;

    /* Cast a ball */
    return (
        project(who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, 0, uniform));
}

/*
 * Handle ball spells that explode immediately on the target and
 * hurt everything.
 */
bool explosion(
    int who, int rad, int y0, int x0, int dd, int ds, int dif, int typ)
{
    /* Add the explosion bitflags */
    u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_JUMP | PROJECT_ITEM
        | PROJECT_KILL | PROJECT_PLAY;

    /* Explode */
    return (
        project_ball(who, rad, y0, x0, y0, x0, dd, ds, dif, typ, flg, false));
}

/*
 * Handle arc spells.
 *
 * Arcs are a pie-shaped segment (with a width determined by "degrees")
 * of a explosion outwards from the source grid.  They are centered
 * along a line extending from the source towards the target.  -LM-
 *
 * Because all arcs start out as being one grid wide, arc spells with a
 * value for degrees of arc less than (roughly) 60 do not dissipate as
 * quickly.  In the extreme case where degrees of arc is 0, the arc is
 * actually a defined length beam, and loses no strength at all over the
 * ranges found in the game.
 *
 * Arcs affect items on the floor.
 */
bool project_arc(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, int degrees)
{
    /* Radius of zero means no fixed limit. */
    if (rad == 0)
        rad = MAX_SIGHT;

    /* If the arc has no spread, it's actually a beam */
    if (degrees <= 0)
    {
        /* Add the beam bitflags */
        flg |= (PROJECT_BEAM | PROJECT_KILL);
    }

    /* If a full circle is asked for, we cast a ball spell. */
    else if (degrees >= 360)
    {
        /* Add the ball bitflags */
        flg |= PROJECT_STOP | PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM
            | PROJECT_KILL;
    }

    /* Otherwise, we fire an arc */
    else
    {
        /* Add the arc bitflags */
        flg |= PROJECT_ARC | PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM
            | PROJECT_KILL;
    }

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= (PROJECT_PLAY);

    /* Cast an arc (or a ball) */
    return (project(
        who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, degrees, false));
}

/*
 * Handle target grids for projections under the control of
 * the character.  - Chris Wilde, Morgul
 */
static void adjust_target(int dir, int y0, int x0, int* y1, int* x1)
{
    /* If no direction is given, and a target is, use the target. */
    if ((dir == 5) && target_okay(0))
    {
        *y1 = p_ptr->target_row;
        *x1 = p_ptr->target_col;
    }
    else if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
    {
        *y1 = y0;
        *x1 = x0;
    }

    /* Otherwise, use the given direction */
    else
    {
        *y1 = y0 + MAX_RANGE * ddy[dir];
        *x1 = x0 + MAX_RANGE * ddx[dir];
    }
}

/*
 * Apply a "project()" directly to all monsters in view of a certain spot.
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 *
 * This function is not optimized for efficieny.  It should only be used
 * in non-bottleneck functions such as spells. It should not be used in
 * functions that are major code bottlenecks such as process monster or
 * update_view. -JG
 */
bool project_los_not_player(int y1, int x1, int dd, int ds, int dif, int typ)
{
    int i, x, y;

    u32b flg = PROJECT_JUMP | PROJECT_KILL | PROJECT_HIDE;

    bool obvious = false;

    /* Affect all (nearby) monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Location */
        y = m_ptr->fy;
        x = m_ptr->fx;

        /*The LOS function doesn't do well with long distances*/
        if (distance(y1, x1, y, x) > MAX_RANGE)
            continue;

        /* Require line of sight or the monster being right on the square */
        if ((y != y1) || (x != x1))
        {
            if (!los(y1, x1, y, x))
                continue;
        }

        /* Jump directly to the target monster */
        if (project(-1, 0, y, x, y, x, dd, ds, dif, typ, flg, 0, false))
            obvious = true;
    }

    /* Result */
    return (obvious);
}

/*
 * Apply a "project()" directly to all viewable monsters
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 */
bool project_los(int typ, int dd, int ds, int dif, bool silent)
{
    int i, x, y;

    u32b flg = PROJECT_JUMP | PROJECT_KILL | PROJECT_HIDE;
    if (silent)
    {
        flg |= PROJECT_SILENT;
    }

    bool obvious = false;

    /* Affect all (nearby) monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Location */
        y = m_ptr->fy;
        x = m_ptr->fx;

        /* Require line of fire */
        if (!player_can_fire_bold(y, x))
            continue;
        if (!player_has_los_bold(y, x))
            continue;

        /* Jump directly to the target monster */
        if (project(-1, 0, y, x, y, x, dd, ds, dif, typ, flg, 0, false))
            obvious = true;
    }

    /* Result */
    return (obvious);
}

/*
 * Apply a "project()" directly to all viewable grids
 */
bool project_los_grids(int typ, int dd, int ds, int dif)
{
    int x, y;
    u32b flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_HIDE | PROJECT_JUMP;

    bool obvious = false;

    for (y = p_ptr->py - MAX_SIGHT; y < p_ptr->py + MAX_SIGHT; y++)
    {
        for (x = p_ptr->px - MAX_SIGHT; x < p_ptr->px + MAX_SIGHT; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!player_has_los_bold(y, x))
                continue;

            if (project(-1, 0, y, x, y, x, dd, ds, dif, typ, flg, 0, false))
            {
                obvious = true;
            }
        }
    }
    /* Result */
    return (obvious);
}

bool fire_bolt_beam_special(
    int typ, int dir, int dd, int ds, int dif, int rad, u32b flg)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* This is a beam spell */
    if (flg & (PROJECT_BEAM))
    {
        /* Cast a beam */
        return (project_beam(
            -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, flg));
    }

    /* This is a bolt spell */
    else
    {
        /* Cast a bolt */
        return (project_bolt(
            -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, flg));
    }
}

/*
 * Character casts a (simple) ball spell.
 */
bool fire_ball(int typ, int dir, int dd, int ds, int dif, int rad)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast a (simple) ball */
    return (project_ball(
        -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L, false));
}

/*
 * Character casts an arc spell.
 */
bool fire_arc(int typ, int dir, int dd, int ds, int dif, int rad, int degrees)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast an arc */
    return (project_arc(
        -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L, degrees));
}

/*
 * Character casts a bolt spell.
 */
bool fire_bolt(int typ, int dir, int dd, int ds, int dif)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast a bolt */
    return (project_bolt(
        -1, MAX_RANGE, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L));
}

/*
 * Character casts a beam spell.
 */
bool fire_beam(int typ, int dir, int dd, int ds, int dif)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast a beam */
    return (project_beam(
        -1, MAX_RANGE, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L));
}

/*
 * Cast a bolt or a beam spell
 */
bool fire_bolt_or_beam(int prob, int typ, int dir, int dd, int ds, int dif)
{
    if (percent_chance(prob))
    {
        return (fire_beam(typ, dir, dd, ds, dif));
    }
    else
    {
        return (fire_bolt(typ, dir, dd, ds, dif));
    }
}
