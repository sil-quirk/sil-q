/* File: cave-flow.c */

#include "cave-internal.h"

/*
 * Determines how far a grid is from the source using the given flow.
 *
 * Given some changes to the code, it is currently a rather unnecessary
 * abstraction which could be replaced with a direct array reference if desired.
 */
int flow_dist(int which_flow, int y, int x)
{
    int dist;

    dist = cave_cost[which_flow][y][x];

    return (dist);
}

/*
 * Sil needs various 'flows', which are arrays of the same size as the map,
 * with a number for each map square.
 *
 * One of these flows is used to represent the from the player noise at each
 * location. Another is used to represent the noise from a particular monster.
 *
 * 200 of them are used for alert monster pathfinding, representing the shortest
 * route each monster could take to get to the player.
 *
 * 100 of them are used for the pathfinding of unwary monsters who move in their
 * initial groups to various locations around the map.
 *
 * There is an intermediate data-structure called the flow table, which is
 * 3-dimensional. The first dimension allows the table to both store and
 * overwrite grids safely.  The second indicates whether this value is that for
 * x or for y.  The third is the number of grids able to be stored at any flow
 * distance.
 *
 * Note that the noise is generated around the centre cy, cx
 * This is often the player, but can be a monster (for FLOW_MONSTER_NOISE)
 *
 */

void update_flow(int cy, int cx, int which_flow)
{
    int cost;

    int i, d;
    byte y, x, y2, x2;
    int last_index;
    int grid_count = 0;

    /* Note where we get information from, and where we overwrite */
    int this_cycle = 0;
    int next_cycle = 1;

    bool monster_flow = false;
    bool bash = false;
    bool found = false;

    monster_type* m_ptr = NULL; // default to soothe compiler warnings
    monster_race* r_ptr = NULL; // default to soothe compiler warnings

    byte flow_table[2][2][8 * FLOW_MAX_DIST];

    // pull out the relevant monster info for the monster flows
    if (which_flow < MAX_MONSTERS)
    {
        monster_flow = true;

        m_ptr = &mon_list[which_flow];
        r_ptr = &r_info[m_ptr->r_idx];
    }

    // pull out the relevant monster info for the wandering monster flows
    else if (which_flow <= FLOW_WANDERING_TAIL)
    {
        monster_flow = true;

        // search the monsters to find one with that flow
        for (i = 1; i < mon_max; i++)
        {
            m_ptr = &mon_list[i];

            // Skip dead monsters
            if (!m_ptr->r_idx)
                continue;

            r_ptr = &r_info[m_ptr->r_idx];

            // find the first monster with this flow
            if (m_ptr->wandering_idx == which_flow)
                found = true;

            if (found)
                break;
        }

        // stop if this is just a vestigial flow left after the monsters died
        // (these are attempted to be reprocessed on save game load)
        if (!found)
            return;
    }

    /* Save the new flow epicenter */
    flow_center_y[which_flow] = cy;
    flow_center_x[which_flow] = cx;
    update_center_y[which_flow] = cy;
    update_center_x[which_flow] = cx;

    /* Erase all of the current flow (noise) information */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            cave_cost[which_flow][y][x] = FLOW_MAX_DIST;
        }
    }

    /*** Update or rebuild the flow ***/

    /* Store base cost at the character location */
    cave_cost[which_flow][cy][cx] = 0;

    /* Store this grid in the flow table, note that we've done so */
    flow_table[this_cycle][0][0] = cy;
    flow_table[this_cycle][1][0] = cx;
    grid_count = 1;

    /* Extend the noise burst out to its limits */
    for (cost = 1; cost <= FLOW_MAX_DIST; cost++)
    {
        /* Get the number of grids we'll be looking at */
        last_index = grid_count;

        /* Stop if we've run out of work to do */
        if (last_index == 0)
            break;

        /* Clear the grid count */
        grid_count = 0;

        /* Get each valid entry in the flow table in turn. */
        for (i = 0; i < last_index; i++)
        {
            /* Get this grid */
            y = flow_table[this_cycle][0][i];
            x = flow_table[this_cycle][1][i];

            // Some grids are not ready to process immediately.
            // For example doors, which add 5 cost to noise, 3 cost to movement.
            // They keep getting put back on the queue until ready.
            if (cave_cost[which_flow][y][x] >= cost)
            {
                /* Store this grid in the flow table */
                flow_table[next_cycle][0][grid_count] = y;
                flow_table[next_cycle][1][grid_count] = x;

                /* Increment number of grids stored */
                grid_count++;
            }

            // if the grid is ready to process...
            else
            {
                /* Look at all adjacent grids */
                for (d = 0; d < 8; d++)
                {
                    int extra_cost = 0;

                    /* Child location */
                    y2 = y + ddy_ddd[d];
                    x2 = x + ddx_ddd[d];

                    /* Check Bounds */
                    if (!in_bounds(y2, x2))
                        continue;

                    /* Ignore previously marked grids, unless this is a shorter
                     * distance
                     */
                    if (cave_cost[which_flow][y2][x2] < FLOW_MAX_DIST)
                        continue;

                    // Deal with monster pathfinding
                    if (monster_flow)
                    {
                        // get the percentage chance of the monster being able
                        // to move onto that square
                        int chance = cave_passable_mon(m_ptr, y2, x2, &bash);

                        // if there is any chance, then convert it to a number
                        // of turns
                        if (chance > 0)
                        {
                            extra_cost += (100 / chance) - 1;

                            // add an extra turn for unlocking/opening doors as
                            // this action doesn't move the monster
                            if (cave_any_closed_door_bold(y2, x2) && !bash)
                            {
                                if (!((r_ptr->flags2 & (RF2_PASS_DOOR))
                                        || (r_ptr->flags2 & (RF2_PASS_WALL))))
                                {
                                    extra_cost += 1;
                                }
                            }

                            // add extra turn(s) for tunneling through
                            // rubble/walls as this action doesn't move the
                            // monster
                            else if (cave_wall_bold(y2, x2)
                                && (r_ptr->flags2 & (RF2_TUNNEL_WALL)))
                            {
                                if (cave_feat[y2][x2] == FEAT_RUBBLE)
                                    extra_cost
                                        += 1; // an extra turn to dig through
                                else
                                    extra_cost += 2; // two extra turns to dig
                                                     // through granite/quartz
                            }

                            else if (cave_wall_bold(y2, x2)
                                && (r_ptr->flags2 & (RF2_KILL_WALL)))
                            {
                                extra_cost += 1; // pretend it would take an
                                                 // extra turn (to prefer routes
                                                 // with less wall destruction
                            }
                        }

                        // if there is no chance, just skip this square
                        else
                        {
                            continue;
                        }
                    }

                    // Deal with noise flows
                    else
                    {
                        // ignore walls
                        if (cave_wall_bold(y2, x2)
                            && (cave_feat[y2][x2] != FEAT_SECRET))
                            continue;

                        // penalize doors by 5 when calculating the real noise
                        if (cave_any_closed_door_bold(y2, x2))
                        {
                            extra_cost += 5;
                        }
                    }

                    /* Monsters at this site need to re-consider their targets
                     */

                    if (cave_m_idx[y2][x2] > 0)
                    {
                        monster_type* n_ptr = &mon_list[cave_m_idx[y2][x2]];

                        n_ptr->target_x = 0;
                        n_ptr->target_y = 0;
                    }

                    /* Store cost at this location */
                    cave_cost[which_flow][y2][x2] = cost + extra_cost;

                    /* Store this grid in the flow table */
                    flow_table[next_cycle][0][grid_count] = y2;
                    flow_table[next_cycle][1][grid_count] = x2;

                    /* Increment number of grids stored */
                    grid_count++;
                }
            }
        }

        /* Swap write and read portions of the table */
        if (this_cycle == 0)
        {
            this_cycle = 1;
            next_cycle = 0;
        }
        else
        {
            this_cycle = 0;
            next_cycle = 1;
        }
    }
}

/*
 * Characters leave scent trails for perceptive monsters to track.  -LM-
 *
 * Smell is rather more limited than sound.  Many creatures cannot use
 * it at all, it doesn't extend very far outwards from the character's
 * current position, and monsters can use it to home in the character,
 * but not to run away from him.
 *
 * Smell is valued according to age.  When a character takes his turn,
 * scent is aged by one, and new scent of the current age is laid down.
 * Speedy characters leave more scent, true, but it also ages faster,
 * which makes it harder to hunt them down.
 *
 * Whenever the age count loops, most of the scent trail is erased and
 * the age of the remainder is recalculated.
 */
void update_smell(void)
{
    int i, j;
    int y, x;
    int py = p_ptr->py;
    int px = p_ptr->px;
    /* Create a table that controls the spread of scent */
    int scent_adjust[5][5] = {
        { 250, 2, 2, 2, 250 },
        { 2, 1, 1, 1, 2 },
        { 2, 1, 0, 1, 2 },
        { 2, 1, 1, 1, 2 },
        { 250, 2, 2, 2, 250 },
    };
    /* Scent becomes "younger" */
    scent_when--;

    /* Loop the age and adjust scent values when necessary */
    if (scent_when <= 0)
    {
        /* Scan the entire dungeon */
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                /* Ignore non-existent scent */
                if (cave_when[y][x] == 0)
                    continue;

                /* Erase the earlier part of the previous cycle */
                if (cave_when[y][x] > SMELL_STRENGTH)
                    cave_when[y][x] = 0;

                /* Reset the ages of the most recent scent */
                else
                    cave_when[y][x] = 250 - SMELL_STRENGTH + cave_when[y][x];
            }
        }

        /* Reset the age value */
        scent_when = 250 - SMELL_STRENGTH;
    }
    /* Lay down new scent */
    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 5; j++)
        {
            /* Translate table to map grids */
            y = i + py - 2;
            x = j + px - 2;

            /* Check Bounds */
            if (!in_bounds(y, x))
                continue;

            /* Walls cannot hold scent. */
            if (cave_info[y][x] & (CAVE_WALL))
            {
                continue;
            }

            /* Grid must not be blocked by walls from the character */
            if (!los(p_ptr->py, p_ptr->px, y, x))
                continue;

            /* Note grids that are too far away */
            if (scent_adjust[i][j] == 250)
                continue;

            /* Mark the grid with new scent */
            cave_when[y][x] = scent_when + scent_adjust[i][j];
        }
    }
}

void map_feature(int y, int x)
{
    int i;

    if (!in_bounds_fully(y, x))
        return;

    /* All non-walls are "checked", including rubble */
    if ((cave_feat[y][x] < FEAT_WALL_HEAD) || (cave_stair_bold(y, x))
        || (cave_feat[y][x] == FEAT_RUBBLE) || cave_forge_bold(y, x)
        || (cave_feat[y][x] == FEAT_CHASM))
    {
        /* Memorize normal features */
        if ((cave_feat[y][x] >= FEAT_DOOR_HEAD) || (cave_stair_bold(y, x))
            || (cave_feat[y][x] == FEAT_RUBBLE) || cave_forge_bold(y, x)
            || (cave_feat[y][x] == FEAT_CHASM))
        {
            /* Memorize the feature */
            cave_info[y][x] |= (CAVE_MARK);
        }

        /* Memorize adjacent walls */
        for (i = 0; i < 8; i++)
        {
            int yy = y + ddy_ddd[i];
            int xx = x + ddx_ddd[i];

            /* Memorize walls (etc) */
            if (cave_wall_bold(yy, xx))
            {
                /* Memorize the walls */
                cave_info[yy][xx] |= (CAVE_MARK);
            }
        }
    }
}

/*
 * Map the dungeon ala "magic mapping"
 *
 * We must never attempt to map the outer dungeon walls, or we
 * might induce illegal cave grid references.
 */
void map_area(void)
{
    int x, y;

    /* Scan that area */
    for (y = 1; y < MAX_DUNGEON_HGT; y++)
    {
        for (x = 1; x < MAX_DUNGEON_WID; x++)
        {
            map_feature(y, x);
        }
    }

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);
}

/*
 * Map the dungeon within a specific radius from the player
 */
void map_area_radius(int radius)
{
    int x, y;
    int py = p_ptr->py;
    int px = p_ptr->px;

    /* Scan within radius */
    for (y = 1; y < MAX_DUNGEON_HGT; y++)
    {
        for (x = 1; x < MAX_DUNGEON_WID; x++)
        {
            /* Check if within radius */
            if (radius > 0)
            {
                int dist = distance(py, px, y, x);
                if (dist > radius)
                    continue;
            }

            map_feature(y, x);
        }
    }

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);
}

/*
 * Light up the dungeon using "clairvoyance"
 *
 * This function "illuminates" every grid in the dungeon, memorizes all
 * "objects", memorizes all grids as with magic mapping, and
 * memorizes all floor grids too.
 */
void wiz_light(void)
{
    int i, y, x;

    /* Memorize objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        /* Memorize */
        o_ptr->marked = true;
    }

    /* Scan all normal grids */
    for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        /* Scan all normal grids */
        for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            /* Process all non-walls, but don't count rubble */
            if ((!cave_wall_bold(y, x)) || (cave_feat[y][x] == FEAT_RUBBLE))
            {
                /* Scan all neighbors */
                for (i = 0; i < 9; i++)
                {
                    int yy = y + ddy_ddd[i];
                    int xx = x + ddx_ddd[i];

                    /* Perma-lite the grid */
                    cave_info[yy][xx] |= (CAVE_GLOW);

                    /* Remember the grid */
                    cave_info[yy][xx] |= (CAVE_MARK);
                }
            }
        }
    }

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD | PW_MONLIST);
}

/*
 * Forget the dungeon map (a la "Thinking of Maud...").
 */
void wiz_dark(void)
{
    int i, y, x;

    /* Forget every grid */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Process the grid */
            cave_info[y][x] &= ~(CAVE_MARK);

            // forget all traps!
            if (cave_trap_bold(y, x) && !((p_ptr->py == y) && (p_ptr->px == x)))
            {
                cave_info[y][x] |= (CAVE_HIDDEN);
            }
        }
    }

    /* Forget all objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        /* Forget the object */
        o_ptr->marked = false;
    }

    /* Process monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        // unmoving mindless monsters need to be forgotten too
        if ((r_ptr->flags1 & (RF1_NEVER_MOVE))
            && (r_ptr->flags2 & (RF2_MINDLESS)) && m_ptr->encountered)
        {
            // Sil-y: this is a bit of a hack as it means you can get the
            // experience for seeing them again but it will only be at most an
            // extra 50 experience per game, more likely about 10
            m_ptr->encountered = false;
        }
    }

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD | PW_MONLIST);
}

/*
 * Light or Darken the gates
 */
void gates_illuminate(bool daytime)
{
    int y, x;

    /* Apply light or darkness */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Interesting grids */
            if (!cave_floorlike_bold(y, x))
            {
                /* Illuminate the grid */
                cave_info[y][x] |= (CAVE_GLOW);

                /* Memorize the grid */
                cave_info[y][x] |= (CAVE_MARK);
            }

            /* Boring grids (light) */
            else if (daytime)
            {
                /* Illuminate the grid */
                cave_info[y][x] |= (CAVE_GLOW);

                /* Memorize grids */
                cave_info[y][x] |= (CAVE_MARK);
            }

            /* Boring grids (dark) */
            else
            {
                /* Darken the grid */
                cave_info[y][x] &= ~(CAVE_GLOW);

                /* Forget grids */
                cave_info[y][x] &= ~(CAVE_MARK);
            }
        }
    }

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD | PW_MONLIST);
}

/* Legacy floor/wall color codes and group identifiers removed; using styles only */

/* Get default encoded color for current depth. Now returns
 * COLOR_STYLE_BASE + <chosen level style> for consistency. */
byte get_depth_color(int depth)
{
    /* Ensure depth 0 gets a valid encoded style even if rules haven't loaded yet */
    if (depth == 0) {
        int sidx = styles_get_level_primary_style();
        if (sidx < 0) sidx = 13; /* default Gates style */
        byte c = (byte)(COLOR_STYLE_BASE + (sidx & (COLOR_STYLE_SLOT_MAX - 1)));
        return c;
    }
    byte c = cave_get_active_style_color();
    return c;
}

/*
 * Change the "feat" flag and color for a grid, and notice/redraw the grid
 */
void cave_set_feat_with_color(int y, int x, int feat, int color)
{
    /* Change the feature */
    cave_feat[y][x] = feat;

    /* Set the color (0 means use depth default) */
    if (color == 0)
    {
        /* Preserve existing per-cell style if already encoded. */
        if (cave_color[y][x] >= COLOR_STYLE_BASE) {
            /* Keep the chosen style for this cell; only the feature changes. */
        } else {
            /* No style encoded yet: use active style (level or vault). */
            cave_color[y][x] = cave_get_active_style_color();
        }
    }
    else
    {
        /* If a raw style index is passed, encode it; if already encoded, keep */
        if (color < COLOR_STYLE_BASE) cave_color[y][x] = (byte)(COLOR_STYLE_BASE + color);
        else cave_color[y][x] = color;
    }

    /* Handle "wall/door" grids */
    if (((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_WALL_TAIL))
        || feat == FEAT_WARDED || feat == FEAT_WARDED2 || feat == FEAT_WARDED3)
    {
        cave_info[y][x] |= (CAVE_WALL);
    }

    /* Handle "floor"/etc grids */
    else
    {
        cave_info[y][x] &= ~(CAVE_WALL);
    }

    /* Notice/Redraw */
    if (character_dungeon)
    {
        /* Notice */
        note_spot(y, x);

        /* Redraw */
        lite_spot(y, x);
    }
}

/*
 * Change the "feat" flag for a grid, and notice/redraw the grid
 */
void cave_set_feat(int y, int x, int feat)
{
    cave_set_feat_with_color(y, x, feat, 0); /* Use default depth color */
}
