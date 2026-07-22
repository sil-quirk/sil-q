/* File: cave-view.c */

#include "cave-internal.h"

vinfo_type vinfo[VINFO_MAX_GRIDS];

typedef struct vinfo_hack vinfo_hack;

/*
 * Temporary data used by "vinfo_init()"
 *
 *	- Number of line of sight slopes
 *
 *	- Slope values
 *
 *	- Slope range for each grid
 */
struct vinfo_hack
{
    int num_slopes;

    long slopes[VINFO_MAX_SLOPES];

    long slopes_min[MAX_SIGHT + 1][MAX_SIGHT + 1];
    long slopes_max[MAX_SIGHT + 1][MAX_SIGHT + 1];
};

/*
 * Sorting hook -- comp function -- array of long's (see below)
 *
 * We use "u" to point to an array of long integers.
 */
static bool ang_sort_comp_hook_longs(const void* u, const void* v, int a, int b)
{
    long* x = (long*)(u);

    /* Unused parameter */
    (void)v;

    return (x[a] <= x[b]);
}

/*
 * Sorting hook -- comp function -- array of long's (see below)
 *
 * We use "u" to point to an array of long integers.
 */
static void ang_sort_swap_hook_longs(void* u, void* v, int a, int b)
{
    long* x = (long*)(u);

    long temp;

    /* Unused parameter */
    (void)v;

    /* Swap */
    temp = x[a];
    x[a] = x[b];
    x[b] = temp;
}

/*
 * Save a slope
 */
static void vinfo_init_aux(vinfo_hack* hack, int y, int x, long m)
{
    int i;

    /* Handle "legal" slopes */
    if ((m > 0) && (m <= SCALE))
    {
        /* Look for that slope */
        for (i = 0; i < hack->num_slopes; i++)
        {
            if (hack->slopes[i] == m)
                break;
        }

        /* New slope */
        if (i == hack->num_slopes)
        {
            /* Paranoia */
            if (hack->num_slopes >= VINFO_MAX_SLOPES)
            {
                quit(format("Too many LOS slopes (%d)!", VINFO_MAX_SLOPES));
            }

            /* Save the slope, increment count */
            hack->slopes[hack->num_slopes++] = m;
        }
    }

    /* Track slope range */
    if (hack->slopes_min[y][x] > m)
        hack->slopes_min[y][x] = m;
    if (hack->slopes_max[y][x] < m)
        hack->slopes_max[y][x] = m;
}

/*
 * Initialize the "vinfo" array
 *
 * Full Octagon (radius 20), Grids=1149
 *
 * Quadrant (south east), Grids=308, Slopes=251
 *
 * Octant (east then south), Grids=161, Slopes=126
 *
 * This function assumes that VINFO_MAX_GRIDS and VINFO_MAX_SLOPES
 * have the correct values, which can be derived by setting them to
 * a number which is too high, running this function, and using the
 * error messages to obtain the correct values.
 */
errr vinfo_init(void)
{
    int i, g;
    int y, x;

    long m;

    vinfo_hack* hack;

    int num_grids = 0;

    int queue_head = 0;
    int queue_tail = 0;
    vinfo_type* queue[VINFO_MAX_GRIDS * 2];

    /* Make hack */
    hack = mem_alloc(vinfo_hack);

    /* Analyze grids */
    for (y = 0; y <= MAX_SIGHT; ++y)
    {
        for (x = y; x <= MAX_SIGHT; ++x)
        {
            /* Skip grids which are out of sight range */
            if (distance(0, 0, y, x) > MAX_SIGHT)
                continue;

            /* Default slope range */
            hack->slopes_min[y][x] = 999999999;
            hack->slopes_max[y][x] = 0;

            /* Paranoia */
            if (num_grids >= VINFO_MAX_GRIDS)
            {
                quit(format(
                    "Too many grids (%d >= %d)!", num_grids, VINFO_MAX_GRIDS));
            }

            /* Count grids */
            num_grids++;

            /* Slope to the top right corner */
            m = SCALE * (1000L * y - 500) / (1000L * x + 500);

            /* Handle "legal" slopes */
            vinfo_init_aux(hack, y, x, m);

            /* Slope to top left corner */
            m = SCALE * (1000L * y - 500) / (1000L * x - 500);

            /* Handle "legal" slopes */
            vinfo_init_aux(hack, y, x, m);

            /* Slope to bottom right corner */
            m = SCALE * (1000L * y + 500) / (1000L * x + 500);

            /* Handle "legal" slopes */
            vinfo_init_aux(hack, y, x, m);

            /* Slope to bottom left corner */
            m = SCALE * (1000L * y + 500) / (1000L * x - 500);

            /* Handle "legal" slopes */
            vinfo_init_aux(hack, y, x, m);
        }
    }

    /* Enforce maximal efficiency (grids) */
    if (num_grids < VINFO_MAX_GRIDS)
    {
        quit(format("Too few grids (%d < %d)!", num_grids, VINFO_MAX_GRIDS));
    }

    /* Enforce maximal efficiency (line of sight slopes) */
    if (hack->num_slopes < VINFO_MAX_SLOPES)
    {
        quit(format("Too few LOS slopes (%d < %d)!", hack->num_slopes,
            VINFO_MAX_SLOPES));
    }

    /* Sort slopes numerically */
    ang_sort_comp = ang_sort_comp_hook_longs;

    /* Sort slopes numerically */
    ang_sort_swap = ang_sort_swap_hook_longs;

    /* Sort the (unique) LOS slopes */
    ang_sort(hack->slopes, NULL, hack->num_slopes);

    /* Enqueue player grid */
    queue[queue_tail++] = &vinfo[0];

    /* Process queue */
    while (queue_head < queue_tail)
    {
        int e;

        /* Index */
        e = queue_head++;

        /* Main Grid */
        g = vinfo[e].grid[0];

        /* Location */
        y = GRID_Y(g);
        x = GRID_X(g);

        /* Compute grid offsets */
        vinfo[e].grid[0] = GRID(+y, +x);
        vinfo[e].grid[1] = GRID(+x, +y);
        vinfo[e].grid[2] = GRID(+x, -y);
        vinfo[e].grid[3] = GRID(+y, -x);
        vinfo[e].grid[4] = GRID(-y, -x);
        vinfo[e].grid[5] = GRID(-x, -y);
        vinfo[e].grid[6] = GRID(-x, +y);
        vinfo[e].grid[7] = GRID(-y, +x);

        /* Skip player grid */
        if (e > 0)
        {
            long slope_fire;

            long tmp0 = 0;
            long tmp1 = 0;
            long tmp2 = 999999L;

            /* Determine LOF slope for this grid */
            if (x == 0)
                slope_fire = SCALE;
            else
                slope_fire = SCALE * (1000L * y) / (1000L * x);

            /* Analyze LOS slopes */
            for (i = 0; i < hack->num_slopes; ++i)
            {
                m = hack->slopes[i];

                /* Memorize intersecting slopes */
                if ((hack->slopes_min[y][x] < m)
                    && (hack->slopes_max[y][x] > m))
                {
                    /* Add it to the LOS slope set */
                    switch (i / 32)
                    {
                    case 3:
                        vinfo[e].bits_3 |= (1L << (i % 32));
                        break;
                    case 2:
                        vinfo[e].bits_2 |= (1L << (i % 32));
                        break;
                    case 1:
                        vinfo[e].bits_1 |= (1L << (i % 32));
                        break;
                    case 0:
                        vinfo[e].bits_0 |= (1L << (i % 32));
                        break;
                    }

                    /* Check for exact match with the LOF slope */
                    if (m == slope_fire)
                        tmp0 = i;

                    /* Remember index of nearest LOS slope < than LOF slope */
                    else if ((m < slope_fire) && (m > tmp1))
                        tmp1 = i;

                    /* Remember index of nearest LOS slope > than LOF slope */
                    else if ((m > slope_fire) && (m < tmp2))
                        tmp2 = i;
                }
            }

            /* There is a perfect match with one of the LOS slopes */
            if (tmp0)
            {
                /* Save the (unique) slope */
                vinfo[e].slope_fire_index1 = tmp0;

                /* Mark the other empty */
                vinfo[e].slope_fire_index2 = 0;
            }

            /* The LOF slope lies between two LOS slopes */
            else
            {
                /* Save the first slope */
                vinfo[e].slope_fire_index1 = tmp1;

                /* Save the second slope */
                vinfo[e].slope_fire_index2 = tmp2;
            }
        }

        /* Default */
        vinfo[e].next_0 = &vinfo[0];

        /* Grid next child */
        if (distance(0, 0, y, x + 1) <= MAX_SIGHT)
        {
            g = GRID(y, x + 1);

            if (queue[queue_tail - 1]->grid[0] != g)
            {
                vinfo[queue_tail].grid[0] = g;
                queue[queue_tail] = &vinfo[queue_tail];
                queue_tail++;
            }

            vinfo[e].next_0 = &vinfo[queue_tail - 1];
        }

        /* Default */
        vinfo[e].next_1 = &vinfo[0];

        /* Grid diag child */
        if (distance(0, 0, y + 1, x + 1) <= MAX_SIGHT)
        {
            g = GRID(y + 1, x + 1);

            if (queue[queue_tail - 1]->grid[0] != g)
            {
                vinfo[queue_tail].grid[0] = g;
                queue[queue_tail] = &vinfo[queue_tail];
                queue_tail++;
            }

            vinfo[e].next_1 = &vinfo[queue_tail - 1];
        }

        /* Hack -- main diagonal has special children */
        if (y == x)
            vinfo[e].next_0 = vinfo[e].next_1;

        /* Grid coordinates, approximate distance  */
        vinfo[e].y = y;
        vinfo[e].x = x;
        vinfo[e].d = ((y > x) ? (y + x / 2) : (x + y / 2));
        vinfo[e].r = ((!y) ? x : (!x) ? y : (y == x) ? y : 0);
    }

    /* Verify maximal bits XXX XXX XXX */
    if (((vinfo[1].bits_3 | vinfo[2].bits_3) != VINFO_BITS_3)
        || ((vinfo[1].bits_2 | vinfo[2].bits_2) != VINFO_BITS_2)
        || ((vinfo[1].bits_1 | vinfo[2].bits_1) != VINFO_BITS_1)
        || ((vinfo[1].bits_0 | vinfo[2].bits_0) != VINFO_BITS_0))
    {
        quit("Incorrect bit masks!");
    }

    /* Kill hack */
    mem_free_null(hack);

    /* Success */
    return (0);
}

/*
 * Forget the "CAVE_VIEW" grids, redrawing as needed
 */
void forget_view(void)
{
    int i, g;

    int fast_view_n = view_n;
    u16b* fast_view_g = view_g;

    u16b* fast_cave_info = &cave_info[0][0];

    /* None to forget */
    if (!fast_view_n)
        return;

    /* Clear them all */
    for (i = 0; i < fast_view_n; i++)
    {
        int y, x;

        /* Grid */
        g = fast_view_g[i];

        /* Location */
        y = GRID_Y(g);
        x = GRID_X(g);

        /* Clear "CAVE_VIEW" and "CAVE_SEEN" flags */
        fast_cave_info[g] &= ~(CAVE_VIEW | CAVE_SEEN | CAVE_FIRE);

        /* Clear "CAVE_LITE" flag */
        /* fast_cave_info[g] &= ~(CAVE_LITE); */

        /* Redraw */
        lite_spot(y, x);
    }

    /* None left */
    fast_view_n = 0;

    /* Save 'view_n' */
    view_n = fast_view_n;
}

bool same_side_of_wall_as_player(int y, int x, int fy, int fx)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    bool same = true;

    // if one above and one below
    if (((py <= y) && (fy >= y)) || ((py >= y) && (fy <= y)))
    {
        if ((px < x) && (fx < x))
        {
            if (cave_info[y][x - 1] & (CAVE_WALL))
            {
                same = false;
            }
        }
        else if ((px > x) && (fx > x))
        {
            if (cave_info[y][x + 1] & (CAVE_WALL))
            {
                same = false;
            }
        }
        else
        {
            same = false;
        }
    }

    // if one left and one right
    if (((px <= x) && (fx >= x)) || ((px >= x) && (fx <= x)))
    {
        // if both above
        if ((py < y) && (fy < y))
        {
            if (cave_info[y - 1][x] & (CAVE_WALL))
            {
                same = false;
            }
        }
        else if ((py > y) && (fy > y))
        {
            if (cave_info[y + 1][x] & (CAVE_WALL))
            {
                same = false;
            }
        }
        else
        {
            same = false;
        }
    }

    return (same);
}

/*
 * Calculate the complete field of view using a new algorithm
 *
 * If "view_g" and "temp_g" were global pointers to arrays of grids, as
 * opposed to actual arrays of grids, then we could be more efficient by
 * using "pointer swapping".
 *
 * Note the following idiom, which is used in the function below.
 * This idiom processes each "octant" of the field of view, in a
 * clockwise manner, starting with the east strip, south side,
 * and for each octant, allows a simple calculation to set "g"
 * equal to the proper grids, relative to "pg", in the octant.
 *
 *   for (o2 = 0; o2 < 8; o2++)
 *   ...
 *         g = pg + p->grid[o2];
 *   ...
 *
 *
 * Normally, vision along the major axes is more likely than vision
 * along the diagonal axes, so we check the bits corresponding to
 * the lines of sight near the major axes first.
 *
 * We use the "temp_g" array (and the "CAVE_TEMP" flag) to keep track of
 * which grids were previously marked "CAVE_SEEN", since only those grids
 * whose "CAVE_SEEN" value changes during this routine must be redrawn.
 *
 * This function is now responsible for maintaining the "CAVE_SEEN"
 * flags as well as the "CAVE_VIEW" flags, which is good, because
 * the only grids which normally need to be memorized and/or redrawn
 * are the ones whose "CAVE_SEEN" flag changes during this routine.
 *
 * Basically, this function divides the "octagon of view" into octants of
 * grids (where grids on the main axes and diagonal axes are "shared" by
 * two octants), and processes each octant one at a time, processing each
 * octant one grid at a time, processing only those grids which "might" be
 * viewable, and setting the "CAVE_VIEW" flag for each grid for which there
 * is an (unobstructed) line of sight from the center of the player grid to
 * any internal point in the grid (and collecting these "CAVE_VIEW" grids
 * into the "view_g" array), and setting the "CAVE_SEEN" flag for the grid
 * if, in addition, the grid is "illuminated" in some way.
 *
 * This function relies on a theorem (suggested and proven by Mat Hostetter)
 * which states that in each octant of a field of view, a given grid will
 * be "intersected" by one or more unobstructed "lines of sight" from the
 * center of the player grid if and only if it is "intersected" by at least
 * one such unobstructed "line of sight" which passes directly through some
 * corner of some grid in the octant which is not shared by any other octant.
 * The proof is based on the fact that there are at least three significant
 * lines of sight involving any non-shared grid in any octant, one which
 * intersects the grid and passes though the corner of the grid closest to
 * the player, and two which "brush" the grid, passing through the "outer"
 * corners of the grid, and that any line of sight which intersects a grid
 * without passing through the corner of a grid in the octant can be "slid"
 * slowly towards the corner of the grid closest to the player, until it
 * either reaches it or until it brushes the corner of another grid which
 * is closer to the player, and in either case, the existanc of a suitable
 * line of sight is thus demonstrated.
 *
 * It turns out that in each octant of the radius 20 "octagon of view",
 * there are 161 grids (with 128 not shared by any other octant), and there
 * are exactly 126 distinct "lines of sight" passing from the center of the
 * player grid through any corner of any non-shared grid in the octant.  To
 * determine if a grid is "viewable" by the player, therefore, you need to
 * simply show that one of these 126 lines of sight intersects the grid but
 * does not intersect any wall grid closer to the player.  So we simply use
 * a bit vector with 126 bits to represent the set of interesting lines of
 * sight which have not yet been obstructed by wall grids, and then we scan
 * all the grids in the octant, moving outwards from the player grid.  For
 * each grid, if any of the lines of sight which intersect that grid have not
 * yet been obstructed, then the grid is viewable.  Furthermore, if the grid
 * is a wall grid, then all of the lines of sight which intersect the grid
 * should be marked as obstructed for future reference.  Also, we only need
 * to check those grids for whom at least one of the "parents" was a viewable
 * non-wall grid, where the parents include the two grids touching the grid
 * but closer to the player grid (one adjacent, and one diagonal).  For the
 * bit vector, we simply use 4 32-bit integers.  All of the static values
 * which are needed by this function are stored in the large "vinfo" array
 * (above), which is machine generated by another program.  XXX XXX XXX
 *
 * Hack -- The queue must be able to hold more than VINFO_MAX_GRIDS grids
 * because the grids at the edge of the field of view use "grid zero" as
 * their children, and the queue must be able to hold several of these
 * special grids.  Because the actual number of required grids is bizarre,
 * we simply allocate twice as many as we would normally need.  XXX XXX XXX
 */
void update_view(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int pg = GRID(py, px);

    int i, j, g, o2;

    int player_light = p_ptr->cur_light;
    int player_rad = ABS(player_light);

    int fy, fx, k;

    int fast_view_n = view_n;
    u16b* fast_view_g = view_g;

    int fast_temp_n = 0;
    u16b* fast_temp_g = temp_g;

    u16b* fast_cave_info = &cave_info[0][0];

    u16b info;

    bool in_pit = cave_pit_bold(p_ptr->py, p_ptr->px) && !p_ptr->leaping;

    /*** Step 0 -- Begin ***/

    /* Save the old "view" grids for later */
    for (i = 0; i < fast_view_n; i++)
    {
        /* Grid */
        g = fast_view_g[i];

        /* Get grid info */
        info = fast_cave_info[g];

        /* Save "CAVE_SEEN" grids */
        if (info & (CAVE_SEEN))
        {
            /* Set "CAVE_TEMP" flag */
            info |= (CAVE_TEMP);

            /* Save grid for later */
            fast_temp_g[fast_temp_n++] = g;
        }

        /* Clear "CAVE_VIEW", "CAVE_SEEN" & cave_fire flags */
        info &= ~(CAVE_VIEW | CAVE_SEEN | CAVE_FIRE);

        /* Clear "CAVE_LITE" flag */
        /* info &= ~(CAVE_LITE); */

        /* Save cave info */
        fast_cave_info[g] = info;
    }

    /* Reset the "view" array */
    fast_view_n = 0;

    /*** Step 1 -- player grid ***/

    /* Player grid */
    g = pg;

    /* Get grid info */
    info = fast_cave_info[g];

    /* Assume viewable */
    info |= (CAVE_VIEW | CAVE_FIRE | CAVE_SEEN);

    /* Save cave info */
    fast_cave_info[g] = info;

    /* Save in array */
    fast_view_g[fast_view_n++] = g;

    /*** Step 2 -- octants ***/

    /* Scan each octant */
    for (o2 = 0; o2 < 8; o2++)
    {
        vinfo_type* p;

        /* Last added */
        vinfo_type* last = &vinfo[0];

        /* Grid queue */
        int queue_head = 0;
        int queue_tail = 0;
        vinfo_type* queue[VINFO_MAX_GRIDS * 2];

        /* Slope bit vector */
        u32b bits0 = VINFO_BITS_0;
        u32b bits1 = VINFO_BITS_1;
        u32b bits2 = VINFO_BITS_2;
        u32b bits3 = VINFO_BITS_3;

        /* Reset queue */
        queue_head = queue_tail = 0;

        /* Initial grids */
        queue[queue_tail++] = &vinfo[1];
        queue[queue_tail++] = &vinfo[2];

        /* Process queue */
        while (queue_head < queue_tail)
        {
            /* Assume no line of fire */
            bool line_fire = false;

            /* Dequeue next grid */
            p = queue[queue_head++];

            /* Check bits */
            if ((bits0 & (p->bits_0)) || (bits1 & (p->bits_1))
                || (bits2 & (p->bits_2)) || (bits3 & (p->bits_3)))
            {
                /* Extract grid value XXX XXX XXX */
                g = pg + p->grid[o2];

                /* Get grid info */
                info = fast_cave_info[g];

                /* Check for first possible line of fire */
                i = p->slope_fire_index1;

                /* Check line(s) of fire */
                while (true)
                {
                    switch (i / 32)
                    {
                    case 3:
                    {
                        if (bits3 & (1L << (i % 32)))
                            line_fire = true;
                        break;
                    }
                    case 2:
                    {
                        if (bits2 & (1L << (i % 32)))
                            line_fire = true;
                        break;
                    }
                    case 1:
                    {
                        if (bits1 & (1L << (i % 32)))
                            line_fire = true;
                        break;
                    }
                    case 0:
                    {
                        if (bits0 & (1L << (i % 32)))
                            line_fire = true;
                        break;
                    }
                    }

                    /* Check second LOF slope if necessary */
                    if ((!p->slope_fire_index2) || (line_fire)
                        || (i == p->slope_fire_index2))
                    {
                        break;
                    }

                    /* Check second possible line of fire */
                    i = p->slope_fire_index2;
                }

                /* Note line of fire */
                if (line_fire)
                {
                    info |= (CAVE_FIRE);
                }

                /* Handle wall */
                if (info & (CAVE_WALL))
                {
                    /* Clear bits */
                    bits0 &= ~(p->bits_0);
                    bits1 &= ~(p->bits_1);
                    bits2 &= ~(p->bits_2);
                    bits3 &= ~(p->bits_3);

                    /* Newly viewable wall */
                    if (!(info & (CAVE_VIEW)))
                    {
                        /* Mark as viewable */
                        info |= (CAVE_VIEW);

                        /* Torch-lit grids */
                        if (p->d <= player_light)
                        {
                            /* Mark as "CAVE_SEEN" */
                            info |= (CAVE_SEEN);

                            /* Mark as "CAVE_LITE" */
                            /* info |= (CAVE_LITE); */
                        }

                        /* Perma-lit grids */
                        else if (info & (CAVE_GLOW))
                        {
                            int y = GRID_Y(g);
                            int x = GRID_X(g);

                            /* Hack -- move towards player */
                            int yy
                                = (y < py) ? (y + 1) : (y > py) ? (y - 1) : y;
                            int xx
                                = (x < px) ? (x + 1) : (x > px) ? (x - 1) : x;

                            /* Check for "complex" illumination */
                            if ((!(cave_info[yy][xx] & (CAVE_WALL))
                                    && (cave_info[yy][xx] & (CAVE_GLOW)))
                                || (!(cave_info[y][xx] & (CAVE_WALL))
                                    && (cave_info[y][xx] & (CAVE_GLOW)))
                                || (!(cave_info[yy][x] & (CAVE_WALL))
                                    && (cave_info[yy][x] & (CAVE_GLOW))))
                            {
                                /* Mark as seen */
                                info |= (CAVE_SEEN);
                            }
                        }

                        /* Save in array */
                        fast_view_g[fast_view_n++] = g;
                    }
                }

                /* Handle non-wall */
                else
                {
                    /* Enqueue child */
                    if (last != p->next_0)
                    {
                        queue[queue_tail++] = last = p->next_0;
                    }

                    /* Enqueue child */
                    if (last != p->next_1)
                    {
                        queue[queue_tail++] = last = p->next_1;
                    }

                    /* Newly viewable non-wall */
                    if (!(info & (CAVE_VIEW)))
                    {
                        /* Mark as "viewable" */
                        info |= (CAVE_VIEW);

                        /* Torch-lit grids */
                        if (p->d <= player_light)
                        {
                            /* Mark as "CAVE_SEEN" */
                            info |= (CAVE_SEEN);

                            /* Mark as "CAVE_LITE" */
                            /* info |= (CAVE_LITE); */
                        }

                        /* Perma-lit grids */
                        else if (info & (CAVE_GLOW))
                        {
                            /* Mark as "CAVE_SEEN" */
                            info |= (CAVE_SEEN);
                        }

                        /* Save in array */
                        fast_view_g[fast_view_n++] = g;
                    }
                }

                /* Save cave info */
                fast_cave_info[g] = info;
            }
        }
    }

    // restrict the view of players in pits
    if (in_pit)
    {
        for (i = 0; i < fast_view_n; i++)
        {
            int y, x;

            g = fast_view_g[i];

            y = GRID_Y(g);
            x = GRID_X(g);

            // quick check to see if the square is not-adjacent
            if ((abs(y - py) > 1) || (abs(x - px) > 1))
            {
                fast_cave_info[g] &= ~(CAVE_SEEN | CAVE_VIEW | CAVE_FIRE);
            }
        }
    }

    /*** Step 2b -- handle the Sil-style light ***/

    /* this is the only step that even looks at these light values */

    // Sil: get the starting light values based on permanent light (and backup
    // old values)
    for (i = 0; i < MAX_DUNGEON_HGT; i++)
    {
        for (j = 0; j < MAX_DUNGEON_WID; j++)
        {
            if (cave_info[i][j] & (CAVE_GLOW))
            {
                cave_light[i][j] = 1;
            }
            else
            {
                cave_light[i][j] = 0;
            }
            
            /* Chasm partitions absorb light - apply -4 penalty */
            if (cave_info[i][j] & CAVE_CHASM_AREA)
            {
                cave_light[i][j] -= 4;
            }
        }
    }

    // Sil: update the light values with the torch/lantern light

    /* Calculate DARKNESS bonus once (items give +1 light power each) */
    int darkness_bonus = 0;
    {
        int slot;
        for (slot = INVEN_WIELD; slot < INVEN_TOTAL; slot++)
        {
            object_type* o_ptr = &inventory[slot];
            u32b f1, f2, f3;

            if (!o_ptr->k_idx) continue;
            if (slot == INVEN_LITE) continue;

            object_flags(o_ptr, &f1, &f2, &f3);
            if (f2 & TR2_DARKNESS)
                darkness_bonus++;
        }
    }

    for (i = -player_rad; i <= player_rad; i++)
    {
        for (j = -player_rad; j <= player_rad; j++)
        {
            int dist = distance(0, 0, i, j);
            int bonus_light = darkness_bonus;

            if (p_ptr->active_ability[S_WIL][WIL_INNER_LIGHT])
            {
                bonus_light += 2;
            }
            if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT)
            {
                bonus_light += 3;
            }

            // Don't darken/brighten the centre square too much
            // if (dist == 0) dist++;

            // Sil-y: previously used los(py,px,py+i,px+j) rather than CAVE_VIEW
            // in the below, but this seems better
            if (in_bounds(py + i, px + j) && (dist <= player_rad)
                && (cave_info[py + i][px + j] & (CAVE_VIEW)))
            {
                if (player_light > 0)
                {
                    cave_light[py + i][px + j]
                        += player_rad + 1 - dist + bonus_light;
                }
                if (player_light < 0)
                {
                    cave_light[py + i][px + j]
                        -= player_rad + 1 - dist + bonus_light;
                }
            }
        }
    }

    // Sil: generate darkness or light for the all the monsters
    for (k = 1; k < mon_max; k++) // Sil-x: changed to mon_max from
                                  // z_info->m_max. I think I'm right about this
    {
        /* Check the k'th monster */
        monster_type* m_ptr = &mon_list[k];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Access the location */
        fx = m_ptr->fx;
        fy = m_ptr->fy;

        int mon_light = r_ptr->light;
        int mon_rad = ABS(mon_light);

        bool glow = r_ptr->flags2 & (RF2_GLOW);

        // Do darkness or light for this monster
        if (mon_rad > 0)
        {
            for (i = -mon_rad; i <= mon_rad; i++)
            {
                for (j = -mon_rad; j <= mon_rad; j++)
                {
                    int y = fy + i;
                    int x = fx + j;

                    int dist = distance(0, 0, i, j);

                    g = GRID(y, x);

                    info = fast_cave_info[g];

                    // Don't darken/brighten the centre square too much
                    // if ((dist == 0) && (distance(py,px,fy,fx) == 1)) dist++;

                    if (in_bounds(y, x) && (dist <= mon_rad)
                        && los(fy, fx, y, x))
                    {
                        // Only set it if the player can see it
                        if ((distance(py, px, y, x) <= MAX_SIGHT)
                            && (info & (CAVE_VIEW)))
                        {
                            if (((cave_info[y][x] & (CAVE_WALL))
                                    && same_side_of_wall_as_player(
                                        y, x, fy, fx))
                                || !(cave_info[y][x] & (CAVE_WALL)))
                            {
                                // Glowing monsters lighten their own square
                                if ((i == 0) && (j == 0) && glow)
                                {
                                    cave_light[y][x] += 1;

                                    /* Mark as seen */
                                    info |= (CAVE_SEEN);

                                    /* Save cave info */
                                    fast_cave_info[g] = info;

                                    /* Save in array */
                                    fast_view_g[fast_view_n++] = g;
                                }

                                // Brighten the square
                                else if (mon_light > 0)
                                {
                                    cave_light[y][x] += mon_rad + 1 - dist;

                                    /* Mark as seen */
                                    info |= (CAVE_SEEN);

                                    /* Save cave info */
                                    fast_cave_info[g] = info;

                                    /* Save in array */
                                    fast_view_g[fast_view_n++] = g;
                                }
                                // Darken the square
                                else
                                {
                                    cave_light[y][x] -= mon_rad + 1 - dist;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Sil: generate darkness or light for the all the objects
    for (k = 1; k < o_max; k++)
    {
        /* Get the next object from the dungeon */
        object_type* o_ptr = &o_list[k];

        u32b f1, f2, f3;

        int obj_light = 0;
        int obj_rad;

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Access the location */
        fx = o_ptr->ix;
        fy = o_ptr->iy;

        object_flags(o_ptr, &f1, &f2, &f3);

        // The Iron Crown glows
        if ((o_ptr->name1 >= ART_MORGOTH_1) && (o_ptr->name1 <= ART_MORGOTH_3))
        {
            obj_light += o_ptr->pval;
        }

        // A floor weapon glowing in response to nearby enemies lights its
        // own square and the surrounding squares.
        if (!o_ptr->held_m_idx && weapon_glows(o_ptr))
        {
            obj_light += 1;
        }

        obj_rad = ABS(obj_light);

        // Do darkness or light for this object
        if (obj_rad > 0)
        {
            for (i = -obj_rad; i <= obj_rad; i++)
            {
                for (j = -obj_rad; j <= obj_rad; j++)
                {
                    int y = fy + i;
                    int x = fx + j;

                    int dist = distance(0, 0, i, j);

                    g = GRID(y, x);

                    info = fast_cave_info[g];

                    // Don't darken/brighten the centre square too much
                    // if ((dist == 0) && (distance(py,px,fy,fx) == 1)) dist++;

                    if (in_bounds(y, x) && (dist <= obj_rad)
                        && los(fy, fx, y, x))
                    {
                        // Only set it if the player can see it
                        if ((distance(py, px, y, x) <= MAX_SIGHT)
                            && (info & (CAVE_VIEW)))
                        {
                            if (((cave_info[y][x] & (CAVE_WALL))
                                    && same_side_of_wall_as_player(
                                        y, x, fy, fx))
                                || !(cave_info[y][x] & (CAVE_WALL)))
                            {
                                // Brighten the square
                                if (obj_light > 0)
                                {
                                    cave_light[y][x] += obj_rad + 1 - dist;

                                    /* Mark as seen */
                                    info |= (CAVE_SEEN);

                                    /* Save cave info */
                                    fast_cave_info[g] = info;

                                    /* Save in array */
                                    fast_view_g[fast_view_n++] = g;
                                }
                                // Darken the square
                                else
                                {
                                    cave_light[y][x] -= obj_rad + 1 - dist;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Sil: this removes the 'seen' flag from squares that have zero or less
    // light
    for (i = 0; i < fast_view_n; i++)
    {
        int y, x;

        g = fast_view_g[i];

        y = GRID_Y(g);
        x = GRID_X(g);

        // Remove 'seen' flag from squares that have zero or less light
        if (cave_light[y][x] <= 0)
        {
            fast_cave_info[g] &= ~(CAVE_SEEN);
        }
    }

    /*** Step 3 -- Complete the algorithm ***/

    /* Handle blindness */
    if (p_ptr->blind)
    {
        /* Process "new" grids */
        for (i = 0; i < fast_view_n; i++)
        {
            /* Grid */
            g = fast_view_g[i];

            /* Grid cannot be "CAVE_SEEN" */
            fast_cave_info[g] &= ~(CAVE_SEEN);
        }
    }

    /* Process "new" grids */
    for (i = 0; i < fast_view_n; i++)
    {
        /* Grid */
        g = fast_view_g[i];

        /* Get grid info */
        info = fast_cave_info[g];

        /* Was not "CAVE_SEEN", is now "CAVE_SEEN" */
        if ((info & (CAVE_SEEN)) && !(info & (CAVE_TEMP)))
        {
            int y, x;

            /* Location */
            y = GRID_Y(g);
            x = GRID_X(g);

            /* Note */
            note_spot(y, x);

            /* Redraw */
            lite_spot(y, x);
        }
    }

    // Sil-y: for some reason we need to update the visibility info for monsters
    // (the ->ml attribute).
    //        Otherwise, dark producing monsters are occasionally visible when
    //        they are following you.
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Update the monster */
        update_mon(i, false);
    }

    /* Process "old" grids */
    for (i = 0; i < fast_temp_n; i++)
    {
        /* Grid */
        g = fast_temp_g[i];

        /* Get grid info */
        info = fast_cave_info[g];

        /* Clear "CAVE_TEMP" flag */
        info &= ~(CAVE_TEMP);

        /* Save cave info */
        fast_cave_info[g] = info;

        /* Was "CAVE_SEEN", is now not "CAVE_SEEN" */
        if (!(info & (CAVE_SEEN)))
        {
            int y, x;

            /* Location */
            y = GRID_Y(g);
            x = GRID_X(g);

            /* Redraw */
            lite_spot(y, x);
        }
    }

    // Sil: this is needed to properly darken certain spots
    for (i = 0; i < MAX_DUNGEON_HGT; i++)
    {
        for (j = 0; j < MAX_DUNGEON_WID; j++)
        {
            if ((cave_info[i][j] & (CAVE_GLOW))
                && (cave_info[i][j] & (CAVE_VIEW))
                && !(cave_info[i][j] & (CAVE_SEEN)))
            {
                /* Redraw */
                lite_spot(i, j);
            }
        }
    }

    // Sil: disturb the player when the lighting changes unexpectedly
    for (i = py - MAX_SIGHT; i <= py + MAX_SIGHT; i++)
    {
        for (j = px - MAX_SIGHT; j <= px + MAX_SIGHT; j++)
        {
            if (in_bounds_fully(i, j) && ((i != py) || (j != px)))
            {
                info = cave_info[i][j];

                if ((info & (CAVE_OLD_VIEW)) && (info & (CAVE_VIEW)))
                {
                    // check recently darkened squares
                    if ((info & (CAVE_OLD_LIT)) && (cave_light[i][j] <= 0))
                    {
                        // if they didn't just fall out of torch radius
                        if (!((info & (CAVE_OLD_TORCH))
                                && (distance(py, px, i, j) > player_rad)))
                        {
                            // ignore in some negative light situations (not a
                            // perfect fix, but good enough)
                            if ((p_ptr->old_light >= 0)
                                || (distance(py, px, i, j) > player_rad + 1))
                            {
                                disturb(0, 0);
                                // msg_format("(%d,%d) Disturbed on loss of
                                // light.",i,j);
                            }
                        }
                    }

                    // check recently lit squares
                    if (!(info & (CAVE_OLD_LIT)) && (cave_light[i][j] > 0))
                    {
                        // if they didn't just enter torch radius
                        if (!(!(info & (CAVE_OLD_TORCH))
                                && (distance(py, px, i, j) <= player_rad)))
                        {
                            // ignore in some negative light situations (not a
                            // perfect fix, but good enough)
                            if ((p_ptr->old_light >= 0)
                                || (distance(py, px, i, j) > player_rad + 1))
                            {
                                disturb(0, 0);
                                // msg_format("(%d,%d) Disturbed on gain of
                                // light.",i,j);
                            }
                        }
                    }
                }
            }
        }
    }

    // Sil: record information about view and lighting for next call to
    // update_view()
    //      so that they player can be disturbed when lighting changes
    //      unexpectedly
    p_ptr->old_light = p_ptr->cur_light;
    for (i = 0; i < MAX_DUNGEON_HGT; i++)
    {
        for (j = 0; j < MAX_DUNGEON_WID; j++)
        {
            // store view information for last turn
            if (cave_info[i][j] & (CAVE_VIEW))
            {
                cave_info[i][j] |= (CAVE_OLD_VIEW);
            }
            else
            {
                cave_info[i][j] &= ~(CAVE_OLD_VIEW);
            }

            // store lighting information for last turn
            if (cave_light[i][j] > 0)
            {
                cave_info[i][j] |= (CAVE_OLD_LIT);
            }
            else
            {
                cave_info[i][j] &= ~(CAVE_OLD_LIT);
            }

            // store 'torchlight' information for last turn
            if (distance(py, px, i, j) <= p_ptr->old_light)
            {
                cave_info[i][j] |= (CAVE_OLD_TORCH);
            }
            else
            {
                cave_info[i][j] &= ~(CAVE_OLD_TORCH);
            }
        }
    }
    /* ------------------------------------------------------------
     * Meta-run curse: CUR_LIGHTP
     *   Each stack makes darkness 1 level "stronger".
     *   We post-process the finished cave_light[][] buffer so that
     *   every lit square is dimmed once per stack, down to a floor
     *   of -5 (same as full darkness elsewhere in the engine).
     * ------------------------------------------------------------ */
    {
        int dark_delta = curse_flag_delta_cur(CUR_LIGHTP);
        if (dark_delta)
        {
            int i, g, y, x;

            /* Iterate over the grids we just updated */
            for (i = 0; i < view_n; i++)
            {
                g = view_g[i];            /* packed grid index      */
                y = GRID_Y(g);            /* unpack coordinates     */
                x = GRID_X(g);

                cave_light[y][x] -= dark_delta;
                if (cave_light[y][x] < -5) cave_light[y][x] = -5;
            }
        }
    }

    /* Passing through a grid gives the player persistent terrain knowledge.
     * For ordinary floors, CAVE_MARK enables navigation while the existing
     * lighting code still chooses the normal or dark floor visual. */
    cave_info[py][px] |= CAVE_MARK;

    /* Save 'view_n' */
    view_n = fast_view_n;
}
