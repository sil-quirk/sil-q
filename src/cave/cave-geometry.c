/* File: cave-geometry.c */

#include "cave-internal.h"

/*
 * Support for tilesets, lighting and transparency effects
 * by Robert Ruehlmann (rr9@thangorodrim.net)
 */

/*
 * Approximate distance between two points.
 *
 * When either the X or Y component dwarfs the other component,
 * this function is almost perfect, and otherwise, it tends to
 * over-estimate about one grid per fifteen grids of distance.
 *
 * Algorithm: hypot(dy,dx) = max(dy,dx) + min(dy,dx) / 2
 */
int distance(int y1, int x1, int y2, int x2)
{
    int ay, ax;

    /* Find the absolute y/x distance components */
    ay = (y1 > y2) ? (y1 - y2) : (y2 - y1);
    ax = (x1 > x2) ? (x1 - x2) : (x2 - x1);

    /* Hack -- approximate the distance */
    return ((ay > ax) ? (ay + (ax >> 1)) : (ax + (ay >> 1)));
}

/*
 * The square of the distance between two points.
 *
 * Used when we need a fine-grained ordering of euclidean distance.
 * e.g. helps an archer who is stuck against a wall to find his way out.
 */
int distance_squared(int y1, int x1, int y2, int x2)
{
    int ay, ax;

    /* Find the absolute y/x distance components */
    ay = (y1 > y2) ? (y1 - y2) : (y2 - y1);
    ax = (x1 > x2) ? (x1 - x2) : (x2 - x1);

    /* Hack -- approximate the distance */
    return (ay * ay + ax * ax);
}

/*
 * A simple, fast, integer-based line-of-sight algorithm.  By Joseph Hall,
 * 4116 Brewster Drive, Raleigh NC 27606.  Email to jnh@ecemwl.ncsu.edu.
 *
 * This function returns true if a "line of sight" can be traced from the
 * center of the grid (x1,y1) to the center of the grid (x2,y2), with all
 * of the grids along this path (except for the endpoints) being non-wall
 * grids.  Actually, the "chess knight move" situation is handled by some
 * special case code which allows the grid diagonally next to the player
 * to be obstructed, because this yields better gameplay semantics.  This
 * algorithm is totally reflexive, except for "knight move" situations.
 *
 * Because this function uses (short) ints for all calculations, overflow
 * may occur if dx and dy exceed 90.
 *
 * Once all the degenerate cases are eliminated, we determine the "slope"
 * ("m"), and we use special "fixed point" mathematics in which we use a
 * special "fractional component" for one of the two location components
 * ("qy" or "qx"), which, along with the slope itself, are "scaled" by a
 * scale factor equal to "abs(dy*dx*2)" to keep the math simple.  Then we
 * simply travel from start to finish along the longer axis, starting at
 * the border between the first and second tiles (where the y offset is
 * thus half the slope), using slope and the fractional component to see
 * when motion along the shorter axis is necessary.  Since we assume that
 * vision is not blocked by "brushing" the corner of any grid, we must do
 * some special checks to avoid testing grids which are "brushed" but not
 * actually "entered".
 *
 * Sil has three different "line of sight" type concepts, including this
 * function (which is used almost nowhere), the "project()" method (which
 * is used for determining the paths of projectables and spells and such),
 * and the "update_view()" concept (which is used to determine which grids
 * are "viewable" by the player, which is used for many things, such as
 * determining which grids are illuminated by the player's torch, and which
 * grids and monsters can be "seen" by the player, etc).
 */
bool los(int y1, int x1, int y2, int x2)
{
    /* Delta */
    int dx, dy;

    /* Absolute */
    int ax, ay;

    /* Signs */
    int sx, sy;

    /* Fractions */
    int qx, qy;

    /* Scanners */
    int tx, ty;

    /* Scale factors */
    int f1, f2;

    /* Slope, or 1/Slope, of LOS */
    int m;

    /* Extract the offset */
    dy = y2 - y1;
    dx = x2 - x1;

    /* Extract the absolute offset */
    ay = ABS(dy);
    ax = ABS(dx);

    /* Handle adjacent (or identical) grids */
    if ((ax < 2) && (ay < 2))
        return (true);

    /* Directly South/North */
    if (!dx)
    {
        /* South -- check for walls */
        if (dy > 0)
        {
            for (ty = y1 + 1; ty < y2; ty++)
            {
                if (!cave_floor_bold(ty, x1))
                    return (false);
            }
        }

        /* North -- check for walls */
        else
        {
            for (ty = y1 - 1; ty > y2; ty--)
            {
                if (!cave_floor_bold(ty, x1))
                    return (false);
            }
        }

        /* Assume los */
        return (true);
    }

    /* Directly East/West */
    if (!dy)
    {
        /* East -- check for walls */
        if (dx > 0)
        {
            for (tx = x1 + 1; tx < x2; tx++)
            {
                if (!cave_floor_bold(y1, tx))
                    return (false);
            }
        }

        /* West -- check for walls */
        else
        {
            for (tx = x1 - 1; tx > x2; tx--)
            {
                if (!cave_floor_bold(y1, tx))
                    return (false);
            }
        }

        /* Assume los */
        return (true);
    }

    /* Extract some signs */
    sx = (dx < 0) ? -1 : 1;
    sy = (dy < 0) ? -1 : 1;

    /* Vertical "knights" */
    if (ax == 1)
    {
        if (ay == 2)
        {
            if (cave_floor_bold(y1 + sy, x1))
                return (true);
        }
    }

    /* Horizontal "knights" */
    else if (ay == 1)
    {
        if (ax == 2)
        {
            if (cave_floor_bold(y1, x1 + sx))
                return (true);
        }
    }

    /* Calculate scale factor div 2 */
    f2 = (ax * ay);

    /* Calculate scale factor */
    f1 = f2 << 1;

    /* Travel horizontally */
    if (ax >= ay)
    {
        /* Let m = dy / dx * 2 * (dy * dx) = 2 * dy * dy */
        qy = ay * ay;
        m = qy << 1;

        tx = x1 + sx;

        /* Consider the special case where slope == 1. */
        if (qy == f2)
        {
            ty = y1 + sy;
            qy -= f1;
        }
        else
        {
            ty = y1;
        }

        /* Note (below) the case (qy == f2), where */
        /* the LOS exactly meets the corner of a tile. */
        while (x2 - tx)
        {
            if (!cave_floor_bold(ty, tx))
                return (false);

            qy += m;

            if (qy < f2)
            {
                tx += sx;
            }
            else if (qy > f2)
            {
                ty += sy;
                if (!cave_floor_bold(ty, tx))
                    return (false);
                qy -= f1;
                tx += sx;
            }
            else
            {
                ty += sy;
                qy -= f1;
                tx += sx;
            }
        }
    }

    /* Travel vertically */
    else
    {
        /* Let m = dx / dy * 2 * (dx * dy) = 2 * dx * dx */
        qx = ax * ax;
        m = qx << 1;

        ty = y1 + sy;

        if (qx == f2)
        {
            tx = x1 + sx;
            qx -= f1;
        }
        else
        {
            tx = x1;
        }

        /* Note (below) the case (qx == f2), where */
        /* the LOS exactly meets the corner of a tile. */
        while (y2 - ty)
        {
            if (!cave_floor_bold(ty, tx))
                return (false);

            qx += m;

            if (qx < f2)
            {
                ty += sy;
            }
            else if (qx > f2)
            {
                tx += sx;
                if (!cave_floor_bold(ty, tx))
                    return (false);
                qx -= f1;
                ty += sy;
            }
            else
            {
                tx += sx;
                qx -= f1;
                ty += sy;
            }
        }
    }

    /* Assume los */
    return (true);
}

void random_unseen_floor(int* ry, int* rx)
{
    int i, y, x;

    // (poor) defaults in case no floor space can be found
    *ry = p_ptr->py;
    *rx = p_ptr->px;

    /* simple way to pick a random floor tile */
    for (i = 0; i <= 1000; i++)
    {
        y = rand_int(p_ptr->cur_map_hgt);
        x = rand_int(p_ptr->cur_map_wid);
        if (cave_naked_bold(y, x) && !player_can_see_bold(y, x))
        {
            *ry = y;
            *rx = x;
            /* Legacy group/color defines removed: styles are selected directly now. */
            return;
        }
    }
}

bool seen_by_keen_senses(int fy, int fx)
{
    int d;

    if (p_ptr->active_ability[S_PER][PER_KEEN_SENSES]
        && (cave_info[fy][fx] & (CAVE_VIEW)) && (cave_light[fy][fx] == 0))
    {
        for (d = 0; d < 8; d++)
        {
            /* Child location */
            int y2 = fy + ddy_ddd[d];
            int x2 = fx + ddx_ddd[d];

            /* Check Bounds */
            if (!in_bounds(y2, x2))
                continue;

            if ((cave_light[y2][x2] > 0) && cave_floor_bold(y2, x2)
                && (cave_info[y2][x2] & (CAVE_VIEW)))
            {
                return (true);
            }
        }
    }

    return (false);
}

/*
 * Determine if a given location may be "destroyed"
 *
 * Used by destruction spells, and for placing stairs, etc.
 */
bool cave_valid_bold(int y, int x)
{
    object_type* o_ptr;

    /* Forbid perma-grids */
    if (cave_perma_bold(y, x))
        return (false);

    /* Check objects */
    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        // Don't destroy the crown
        if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_3))
            return false;
    }

    /* Accept */
    return (true);
}
