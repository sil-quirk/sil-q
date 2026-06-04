/* File: cave-projection.c */

#include "cave-internal.h"

/*
 * Determine the path taken by a projection.  -BEN-, -LM-
 *
 * The projection will always start one grid from the grid (y1,x1), and will
 * travel towards the grid (y2,x2), touching one grid per unit of distance
 * along the major axis, and stopping when it satisfies certain conditions
 * or has travelled the maximum legal distance of "range".  Projections
 * cannot extend further than MAX_SIGHT (at least at present).
 *
 * A projection only considers those grids which contain the line(s) of fire
 * from the start to the end point.  Along any step of the projection path,
 * either one or two grids may be valid options for the next step.  When a
 * projection has a choice of grids, it chooses that which offers the least
 * resistance.  Given a choice of clear grids, projections prefer to move
 * orthogonally.
 *
 * Also, projections to or from the character must stay within the pre-
 * calculated field of fire ("cave_info & (CAVE_FIRE)").  This is a hack.
 * XXX XXX
 *
 * The path grids are saved into the grid array pointed to by "gp", and
 * there should be room for at least "range" grids in "gp".  Note that
 * due to the way in which distance is calculated, this function normally
 * uses fewer than "range" grids for the projection path, so the result
 * of this function should never be compared directly to "range".  Note
 * that the initial grid (y1,x1) is never saved into the grid array, not
 * even if the initial grid is also the final grid.  XXX XXX XXX
 *
 * We modify y2 and x2 if they are too far away, or (for PROJECT_PASS only)
 * if the projection threatens to leave the dungeon.
 *
 * The "flg" flags can be used to modify the behavior of this function:
 *    PROJECT_STOP:  projection stops when it cannot bypass a monster.
 *    PROJECT_CHCK:  projection notes when it cannot bypass a monster.
 *    PROJECT_THRU:  projection extends past destination grid
 *    PROJECT_PASS:  projection passes through walls
 *    PROJECT_INVISIPASS:  projection passes through invisible walls (ie unknown
 * ones)
 *
 * This function returns the number of grids (if any) in the path.  This
 * may be zero if no grids are legal except for the starting one.
 */
int project_path(
    u16b* gp, int range, int y1, int x1, int* y2, int* x2, u32b flg)
{
    int i, j, k;
    int dy, dx;
    int num, dist, octant;
    int grids = 0;
    bool line_fire;
    bool full_stop = false;

    int y_a, x_a, y_b, x_b;
    int y = 0, old_y = 0;
    int x = 0, old_x = 0;

    /* Start with all lines of sight unobstructed */
    u32b bits0 = VINFO_BITS_0;
    u32b bits1 = VINFO_BITS_1;
    u32b bits2 = VINFO_BITS_2;
    u32b bits3 = VINFO_BITS_3;

    int slope_fire1 = -1, slope_fire2 = 0;

    /* Projections are either vertical or horizontal */
    bool vertical;

    /* Require projections to be strictly LOF when possible  XXX XXX */
    bool require_strict_lof = false;

    /* Count of grids in LOF, storage of LOF grids */
    u16b tmp_grids[80];

    /* Count of grids in projection path */
    int step;

    /* Remember whether and how a grid is blocked */
    int blockage[2];

    /* Assume no monsters in way */
    bool monster_in_way = false;

    /* Initial grid */
    u16b g0 = (u16b)GRID(y1, x1);

    u16b g;

    /* Pointer to vinfo data */
    vinfo_type* p;

    /* Handle projections of zero length */
    if ((range <= 0) || ((*y2 == y1) && (*x2 == x1)))
        return (0);

    /* Note that the character is the source or target of the projection */
    if (((y1 == p_ptr->py) && (x1 == p_ptr->px))
        || ((*y2 == p_ptr->py) && (*x2 == p_ptr->px)))
    {
        /* Require strict LOF */
        require_strict_lof = true;
    }

    /* Get position change (signed) */
    dy = *y2 - y1;
    dx = *x2 - x1;

    /* Get distance from start to finish */
    dist = distance(y1, x1, *y2, *x2);

    /* Must stay within the field of sight XXX XXX */
    if (dist > MAX_SIGHT)
    {
        /* Always watch your (+/-) when doing rounded integer math. */
        int round_y = (dy < 0 ? -(dist / 2) : (dist / 2));
        int round_x = (dx < 0 ? -(dist / 2) : (dist / 2));

        /* Rescale the endpoints */
        dy = ((dy * (MAX_SIGHT - 1)) + round_y) / dist;
        dx = ((dx * (MAX_SIGHT - 1)) + round_x) / dist;
        *y2 = y1 + dy;
        *x2 = x1 + dx;
    }

    /* Get the correct octant */
    if (dy < 0)
    {
        /* Up and to the left */
        if (dx < 0)
        {
            /* More upwards than to the left - octant 4 */
            if (ABS(dy) > ABS(dx))
                octant = 5;

            /* At least as much left as upwards - octant 3 */
            else
                octant = 4;
        }
        else
        {
            if (ABS(dy) > ABS(dx))
                octant = 6;
            else
                octant = 7;
        }
    }
    else
    {
        if (dx < 0)
        {
            if (ABS(dy) > ABS(dx))
                octant = 2;
            else
                octant = 3;
        }
        else
        {
            if (ABS(dy) > ABS(dx))
                octant = 1;
            else
                octant = 0;
        }
    }

    /* Determine whether the major axis is vertical or horizontal */
    if ((octant == 5) || (octant == 6) || (octant == 2) || (octant == 1))
    {
        vertical = true;
    }
    else
    {
        vertical = false;
    }

    /* Scan the octant, find the grid corresponding to the end point */
    for (j = 1; j < VINFO_MAX_GRIDS; j++)
    {
        int vy, vx;

        /* Point to this vinfo record */
        p = &vinfo[j];

        /* Extract grid value */
        g = (u16b)(g0 + p->grid[octant]);

        /* Get axis coordinates */
        vy = GRID_Y(g);
        vx = GRID_X(g);

        /* Require that grid be correct */
        if ((vy != *y2) || (vx != *x2))
            continue;

        /* Store lines of fire */
        slope_fire1 = p->slope_fire_index1;
        slope_fire2 = p->slope_fire_index2;

        break;
    }

    /* Note failure XXX XXX */
    if (slope_fire1 == -1)
        return (0);

    /* Scan the octant, collect all grids having the correct line of fire */
    for (j = 1; j < VINFO_MAX_GRIDS; j++)
    {
        line_fire = false;

        /* Point to this vinfo record */
        p = &vinfo[j];

        /* See if any lines of sight pass through this grid */
        if (!((bits0 & (p->bits_0)) || (bits1 & (p->bits_1))
                || (bits2 & (p->bits_2)) || (bits3 & (p->bits_3))))
        {
            continue;
        }

        /*
         * Extract grid value.  Use pointer shifting to get the
         * correct grid offset for this octant.
         */
        g = (u16b)(g0 + *((s16b*)(((byte*)(p)) + (octant * 2))));

        y = GRID_Y(g);
        x = GRID_X(g);

        /* Must be legal (this is important) */
        if (!in_bounds_fully(y, x))
            continue;

        /* Check for first possible line of fire */
        i = slope_fire1;

        /* Check line(s) of fire */
        while (true)
        {
            switch (i / 32)
            {
            case 3:
            {
                if (bits3 & (1L << (i % 32)))
                {
                    if (p->bits_3 & (1L << (i % 32)))
                        line_fire = true;
                }
                break;
            }
            case 2:
            {
                if (bits2 & (1L << (i % 32)))
                {
                    if (p->bits_2 & (1L << (i % 32)))
                        line_fire = true;
                }
                break;
            }
            case 1:
            {
                if (bits1 & (1L << (i % 32)))
                {
                    if (p->bits_1 & (1L << (i % 32)))
                        line_fire = true;
                }
                break;
            }
            case 0:
            {
                if (bits0 & (1L << (i % 32)))
                {
                    if (p->bits_0 & (1L << (i % 32)))
                        line_fire = true;
                }
                break;
            }
            }

            /* We're done if no second LOF exists, or when we've checked it */
            if ((!slope_fire2) || (i == slope_fire2))
                break;

            /* Check second possible line of fire */
            i = slope_fire2;
        }

        /* This grid contains at least one of the lines of fire */
        if (line_fire)
        {
            /* Do not accept breaks in the series of grids  XXX XXX */
            if ((grids) && ((ABS(y - old_y) > 1) || (ABS(x - old_x) > 1)))
            {
                break;
            }

            /* Optionally, require strict line of fire */
            if ((!require_strict_lof) || (cave_info[y][x] & (CAVE_FIRE))
                || ((flg & (PROJECT_INVISIPASS))
                    && !(cave_info[y][x] & (CAVE_MARK))))
            {
                /* Store grid value */
                tmp_grids[grids++] = g;
            }

            /* Remember previous coordinates */
            old_y = y;
            old_x = x;
        }

        /*
         * Handle wall (unless ignored).  Walls can be in a projection path,
         * but the path cannot pass through them.
         */
        if (!(flg & (PROJECT_PASS)) && (cave_info[y][x] & (CAVE_WALL)))
        {
            if (!(flg & (PROJECT_INVISIPASS))
                || (cave_info[y][x] & (CAVE_MARK)))
            {
                /* Clear any lines of sight passing through this grid */
                bits0 &= ~(p->bits_0);
                bits1 &= ~(p->bits_1);
                bits2 &= ~(p->bits_2);
                bits3 &= ~(p->bits_3);
            }
        }

        /*
         * Handle chasms if they are designated to block the line
         */
        if ((flg & (PROJECT_NO_CHASM)) && (cave_feat[y][x] & (FEAT_CHASM)))
        {
            /* Clear any lines of sight passing through this grid */
            bits0 &= ~(p->bits_0);
            bits1 &= ~(p->bits_1);
            bits2 &= ~(p->bits_2);
            bits3 &= ~(p->bits_3);
        }
    }

    /* Scan the grids along the line(s) of fire */
    for (step = 0, j = 0; j < grids;)
    {
        /* Get the coordinates of this grid */
        y_a = GRID_Y(tmp_grids[j]);
        x_a = GRID_X(tmp_grids[j]);

        /* Get the coordinates of the next grid, if legal */
        if (j < grids - 1)
        {
            y_b = GRID_Y(tmp_grids[j + 1]);
            x_b = GRID_X(tmp_grids[j + 1]);
        }
        else
        {
            y_b = -1;
            x_b = -1;
        }

        /*
         * We always have at least one legal grid, and may have two.  Allow
         * the second grid if its position differs only along the minor axis.
         */
        if (vertical ? y_a == y_b : x_a == x_b)
            num = 2;
        else
            num = 1;

        /* Scan one or both grids */
        for (i = 0; i < num; i++)
        {
            blockage[i] = 0;

            /* Get the coordinates of this grid */
            y = (i == 0 ? y_a : y_b);
            x = (i == 0 ? x_a : x_b);

            /* Determine perpendicular distance */
            k = (vertical ? ABS(x - x1) : ABS(y - y1));

            /* Hack -- Check maximum range */
            if ((i == num - 1) && (step + (k >> 1)) >= range - 1)
            {
                /* End of projection */
                full_stop = true;
            }

            /* Sometimes stop at destination grid */
            if (!(flg & (PROJECT_THRU)))
            {
                if ((y == *y2) && (x == *x2))
                {
                    /* End of projection */
                    full_stop = true;
                }
            }

            /* Usually stop at wall grids */
            if (!(flg & (PROJECT_PASS))
                && (!(flg & (PROJECT_INVISIPASS))
                    || (cave_info[y][x] & (CAVE_MARK))))
            {
                if (!cave_floor_bold(y, x))
                    blockage[i] = 2;
            }

            /* If we don't stop at wall grids, we must explicitly check legality
             */
            else if (!in_bounds_fully(y, x))
            {
                /* End of projection */
                full_stop = true;
                blockage[i] = 3;
            }

            /* Try to avoid monsters/players between the endpoints */
            if ((cave_m_idx[y][x] != 0) && (blockage[i] < 2))
            {
                // Hack: ignore monsters on the designated square if these flags
                // are set
                if (!(project_path_ignore && (y == project_path_ignore_y)
                        && (x == project_path_ignore_x)))
                {
                    if (flg & (PROJECT_STOP))
                        blockage[i] = 2;
                    else if (flg & (PROJECT_CHCK))
                        blockage[i] = 1;
                }
            }
        }

        /* Pick the first grid if possible, the second if necessary */
        if ((num == 1) || (blockage[0] <= blockage[1]))
        {
            /* Store the first grid, advance */
            if (blockage[0] < 3)
                gp[step++] = tmp_grids[j];

            /* Blockage of 2 or greater means the projection ends */
            if (blockage[0] >= 2)
                break;

            /* Blockage of 1 means a monster bars the path */
            if (blockage[0] == 1)
            {
                /* Endpoints are always acceptable */
                if ((y != *y2) || (x != *x2))
                    monster_in_way = true;
            }

            /* Handle end of projection */
            if (full_stop)
                break;
        }
        else
        {
            /* Store the second grid, advance */
            if (blockage[1] < 3)
                gp[step++] = tmp_grids[j + 1];

            /* Blockage of 2 or greater means the projection ends */
            if (blockage[1] >= 2)
                break;

            /* Blockage of 1 means a monster bars the path */
            if (blockage[1] == 1)
            {
                /* Endpoints are always acceptable */
                if ((y != *y2) || (x != *x2))
                    monster_in_way = true;
            }

            /* Handle end of projection */
            if (full_stop)
                break;
        }

        /*
         * Hack -- If we require orthogonal movement, but are moving
         * diagonally, we have to plot an extra grid.  XXX XXX
         */
        if ((flg & (PROJECT_ORTH)) && (step > 1))
        {
            /* Get grids for this projection step and the last */
            y_a = GRID_Y(gp[step - 1]);
            x_a = GRID_X(gp[step - 1]);

            y_b = GRID_Y(gp[step - 2]);
            x_b = GRID_X(gp[step - 2]);

            /* The grids differ along both axis -- we moved diagonally */
            if ((y_a != y_b) && (x_a != x_b))
            {
                /* Get locations for the connecting grids */
                int y_c = y_a;
                int x_c = x_b;
                int y_d = y_b;
                int x_d = x_a;

                /* Back up one step */
                step--;

                /* Assume both grids are available */
                blockage[0] = 0;
                blockage[1] = 0;

                /* Hack -- Check legality */
                if (!in_bounds_fully(y_c, x_c))
                    blockage[0] = 2;
                if (!in_bounds_fully(y_d, x_d))
                    blockage[1] = 2;

                /* Usually stop at wall grids */
                if (!(flg & (PROJECT_PASS))
                    && (!(flg & (PROJECT_INVISIPASS))
                        || (cave_info[y][x] & (CAVE_MARK))))
                {
                    if (!cave_floor_bold(y_c, x_c))
                        blockage[0] = 2;
                    if (!cave_floor_bold(y_d, x_d))
                        blockage[1] = 2;
                }

                /* Try to avoid non-initial monsters/players */
                if (cave_m_idx[y_c][x_c] != 0)
                {
                    // Hack: ignore monsters on the designated square if these
                    // flags are set
                    if (!(project_path_ignore && (y_c == project_path_ignore_y)
                            && (x_c == project_path_ignore_x)))
                    {
                        if (flg & (PROJECT_STOP))
                            blockage[0] = 2;
                        else if (flg & (PROJECT_CHCK))
                            blockage[0] = 1;
                    }
                }
                if (cave_m_idx[y_d][x_d] != 0)
                {
                    // Hack: ignore monsters on the designated square if these
                    // flags are set
                    if (!(project_path_ignore && (y_c == project_path_ignore_y)
                            && (x_c == project_path_ignore_x)))
                    {
                        if (flg & (PROJECT_STOP))
                            blockage[1] = 2;
                        else if (flg & (PROJECT_CHCK))
                            blockage[1] = 1;
                    }
                }

                /* Both grids are blocked -- we have to stop now */
                if ((blockage[0] >= 2) && (blockage[1] >= 2))
                    break;

                /* Accept the first grid if possible, the second if necessary */
                if (blockage[0] <= blockage[1])
                    gp[step++] = GRID(y_c, x_c);
                else
                    gp[step++] = GRID(y_d, x_d);

                /* Re-insert the original grid, take an extra step */
                gp[step++] = GRID(y_a, x_a);

                /* Increase range to accommodate this extra step */
                range++;
            }
        }

        /* Advance to the next unexamined LOF grid */
        j += num;
    }

    /* Accept last grid as the new endpoint */
    *y2 = GRID_Y(gp[step - 1]);
    *x2 = GRID_X(gp[step - 1]);

    /* Return count of grids in projection path */
    if (monster_in_way)
        return (-step);
    else
        return (step);
}

/*
 * Determine if a bolt spell cast from (y1,x1) to (y2,x2) will arrive
 * at the final destination, using the "project_path()" function to check
 * the projection path.
 *
 * Accept projection flags, and pass them onto "project_path()".
 *
 * Note that no grid is ever "projectable()" from itself.
 * This function is used to determine if the player can (easily) target
 * a given grid, if a monster can target the player, and if a clear shot
 * exists from monster to player.
 */

byte projectable(int y1, int x1, int y2, int x2, u32b flg)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    int grid_n = 0;
    u16b grid_g[512];

    int old_y2 = y2;
    int old_x2 = x2;

    /* We do not have permission to pass through walls */
    if (!(flg & (PROJECT_WALL | PROJECT_PASS)))
    {
        /* The character is the source of the projection */
        if ((y1 == py) && (x1 == px))
        {
            /* Require that destination be in line of fire */
            if (!(cave_info[y2][x2] & (CAVE_FIRE)))
                return (PROJECT_NO);
        }

        /* The character is the target of the projection */
        else if ((y2 == py) && (x2 == px))
        {
            /* Require that source be in line of fire */
            if (!(cave_info[y1][x1] & (CAVE_FIRE)))
                return (PROJECT_NO);
        }
    }

    /* Check the projection path */
    grid_n = project_path(grid_g, MAX_RANGE, y1, x1, &y2, &x2, flg);

    /* No grid is ever projectable from itself */
    if (!grid_n)
        return (PROJECT_NO);

    /* Final grid.  As grid_n may be negative, use absolute value.  */
    y = GRID_Y(grid_g[ABS(grid_n) - 1]);
    x = GRID_X(grid_g[ABS(grid_n) - 1]);

    /* May not end in an unrequested grid */
    if ((y != old_y2) || (x != old_x2))
        return (PROJECT_NO);

    /* May not end in a wall */
    if (!cave_floor_bold(y, x))
        return (PROJECT_NO);

    /* Promise a clear bolt shot if we have verified that there is one */
    if ((flg & (PROJECT_STOP)) || (flg & (PROJECT_CHCK)))
    {
        /* Positive value for grid_n mean no obstacle was found. */
        if (grid_n > 0)
            return (PROJECT_CLEAR);
    }

    /* Assume projectable, but make no promises about clear shots */
    return (PROJECT_NOT_CLEAR);
}

/*
 * Standard "find me a location" function
 *
 * Obtains a legal location within the given distance of the initial
 * location, and with "los()" from the source to destination location.
 *
 * This function is often called from inside a loop which searches for
 * locations while increasing the "d" distance.
 *
 * Currently the "m" parameter is unused.
 */
void scatter(int* yp, int* xp, int y, int x, int d, int m)
{
    int nx, ny;

    /* Unused parameter */
    (void)m;

    /* Pick a location */
    while (true)
    {
        /* Pick a new location */
        ny = rand_spread(y, d);
        nx = rand_spread(x, d);

        /* Ignore annoying locations */
        if (!in_bounds_fully(ny, nx))
            continue;

        /* Ignore "excessively distant" locations */
        if ((d > 1) && (distance(y, x, ny, nx) > d))
            continue;

        /* Require "line of sight" */
        if (los(y, x, ny, nx))
            break;
    }

    /* Save the location */
    (*yp) = ny;
    (*xp) = nx;
}
