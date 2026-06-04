#include "angband.h"
#include "externs.h"
#include "melee/melee-attack.h"
#include "melee/melee-movement.h"
#include "melee/melee-movement-internal.h"
#include "melee/melee-process.h"
#include "melee/melee-util.h"

/*
 * Can the monster catch a whiff of the character?
 *
 * Many more monsters can smell, but they find it hard to smell and
 * track down something at great range.
 */
bool monster_can_smell(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    int age;

    /* Get the age of the scent here */
    age = get_scent(m_ptr->fy, m_ptr->fx);

    /* No scent */
    if (age == -1)
        return (false);

    /* Wolves are amazing trackers */
    if (strchr("C", r_ptr->d_char))
    {
        /* I smell a character! */
        return (true);
    }

    /* Felines are also quite good */
    else if (strchr("f", r_ptr->d_char))
    {
        if (age <= SMELL_STRENGTH / 2)
        {
            /* Something's in the air... */
            return (true);
        }
    }

    /* You're imagining things. */
    return (false);
}

/*
 *  Determine the next move for an unwary wandering monster
 */
bool get_move_wander(monster_type* m_ptr, int* ty, int* tx)
{
    int d;

    int dist;
    int closest = FLOW_MAX_DIST - 1;

    byte y, x, y1, x1;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    bool no_move = false;
    bool random_move = false;

    /* Monster location */
    y1 = m_ptr->fy;
    x1 = m_ptr->fx;

    // Deal with monsters that don't have a destination
    if (!m_ptr->wandering_idx)
    {
        // Some monsters cannot move at all
        if (r_ptr->flags1 & (RF1_NEVER_MOVE))
        {
            return (false);
        }

        // Some just never wander
        else if ((r_ptr->flags2 & (RF2_SHORT_SIGHTED))
            || (r_ptr->flags1 & (RF1_HIDDEN_MOVE)))
        {
            return (false);
        }

        // Many monsters can only make random moves
        else
        {
            random_move = true;
        }
    }

    // Deal with some special cases for monsters that have a destination
    else
    {
        int i;
        int group_size = 0;
        int group_furthest = 0;
        int group_sleepers = 0;
        int sleeper_y = 0;
        int sleeper_x = 0;
        int max_drop;

        // how far is the monster from its wandering destination?
        dist = flow_dist(m_ptr->wandering_idx, y1, x1);

        // check out monsters with the same index
        for (i = 1; i < mon_max; i++)
        {
            monster_type* n_ptr = &mon_list[i];

            /* Skip dead monsters */
            if (!n_ptr->r_idx)
                continue;

            // record some features of the group
            if (n_ptr->wandering_idx == m_ptr->wandering_idx)
            {
                group_size++;

                if (n_ptr->alertness < ALERTNESS_UNWARY)
                {
                    group_sleepers++;
                    if (group_sleepers == 1)
                    {
                        sleeper_y = n_ptr->fy;
                        sleeper_x = n_ptr->fx;
                    }
                }
                if (n_ptr->wandering_dist > group_furthest)
                    group_furthest = n_ptr->wandering_dist;
            }
        }

        // no wandering on the Gates level
        if (p_ptr->depth == 0)
        {
            return (false);
        }

        // no wandering in the throne room during the truce
        if (p_ptr->truce)
        {
            return (false);
        }

        // determine if the monster has a hoard
        max_drop = (((r_ptr->flags1 & RF1_DROP_4D2) ? 8 : 0)
            + ((r_ptr->flags1 & RF1_DROP_3D2) ? 6 : 0)
            + ((r_ptr->flags1 & RF1_DROP_2D2) ? 4 : 0)
            + ((r_ptr->flags1 & RF1_DROP_1D2) ? 2 : 0)
            + ((r_ptr->flags1 & RF1_DROP_100) ? 1 : 0)
            + ((r_ptr->flags1 & RF1_DROP_33) ? 1 : 0)
            + ((r_ptr->flags3 & RF3_DROP_1D3) ? 3 : 0));

        // treasure-hoarding territorial monsters stay still at their hoard...
        if ((r_ptr->flags2 & (RF2_TERRITORIAL)) && (max_drop > 0)
            && (flow_dist(m_ptr->wandering_idx, y1, x1) == 0))
        {
            // very occasionally fall asleep
            if (one_in_(100) && (p_ptr->game_type >= 0)
                && !(r_ptr->flags3 & (RF3_NO_SLEEP)))
            {
                set_alertness(
                    m_ptr, rand_range(ALERTNESS_MIN, ALERTNESS_UNWARY - 1));
            }

            return (false);
        }

        // if the destination is too far away, pick a new one
        if (dist > MON_WANDER_RANGE)
        {
            new_wandering_flow(m_ptr, 0, 0);
        }

        // if there is no pausing going on and it is at the destination, then
        // start pausing
        if ((wandering_pause[m_ptr->wandering_idx] == 0) && (dist <= 0))
        {
            wandering_pause[m_ptr->wandering_idx] = dieroll(50) * group_size;
        }

        // if the monster is pausing, then decrease the pause counter
        else if (wandering_pause[m_ptr->wandering_idx] > 1)
        {
            random_move = true;
            wandering_pause[m_ptr->wandering_idx]--;
        }

        // if the monster has finished pausing at an old destination
        else if (wandering_pause[m_ptr->wandering_idx] == 1)
        {
            // choose a new destination
            new_wandering_flow(m_ptr, 0, 0);
            wandering_pause[m_ptr->wandering_idx]--;
        }

        // if the monster is not making progress
        if (dist >= m_ptr->wandering_dist)
        {
            // possibly pick a new destination
            if (one_in_(20 * group_size))
            {
                new_wandering_flow(m_ptr, 0, 0);
            }
        }

        // sometimes delay to let others catch up
        if (dist < group_furthest - group_size)
        {
            if (one_in_(2))
                no_move = true;
        }

        // unwary monsters won't wander off while others in the group are
        // sleeping
        if ((m_ptr->alertness < ALERTNESS_ALERT) && (group_sleepers > 0))
        {
            // only set the new flow if needed
            if ((flow_center_y[m_ptr->wandering_idx] != sleeper_y)
                || (flow_center_x[m_ptr->wandering_idx] != sleeper_x))
            {
                new_wandering_flow(m_ptr, sleeper_y, sleeper_x);
            }

            if (one_in_(2))
                random_move = true;
        }

        // non-territorial monsters in vaults move randomly
        if (!(r_ptr->flags2 & (RF2_TERRITORIAL))
            && (cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_ICKY)))
        {
            random_move = true;
        }

        // update the wandering_dist
        m_ptr->wandering_dist = dist;
    }

    if (no_move)
        return (false);

    // do a random move if needed
    if (random_move)
    {
        // mostly stay still
        if (!one_in_(4))
        {
            return (false);
        }

        // sometimes move
        else
        {
            /* Random direction */
            d = ddd[rand_int(8)];

            y = y1 + ddy_ddd[d];
            x = x1 + ddx_ddd[d];

            /* Check Bounds */
            if (!in_bounds(y, x))
                return (false);

            // Monsters in vaults shouldn't leave them
            if ((cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_ICKY))
                && !(cave_info[y][x] & (CAVE_ICKY)))
                return (false);

            /* Save the location */
            *ty = y;
            *tx = x;
        }
    }

    // move towards destination
    else
    {
        // smart monsters who are at the stairs they are aiming for leave the
        // level
        if ((r_ptr->flags2 & (RF2_SMART))
            && !(r_ptr->flags2 & (RF2_TERRITORIAL))
            && (p_ptr->depth != MORGOTH_DEPTH)
            && cave_stair_bold(m_ptr->fy, m_ptr->fx)
            && (m_ptr->wandering_dist == 0))
        {
            char m_name[80];

            if (m_ptr->ml)
            {
                monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);
                if (cave_down_stairs_bold(m_ptr->fy, m_ptr->fx))
                    msg_format("%^s goes down the stairs.", m_name);
                else
                    msg_format("%^s goes up the stairs.", m_name);
            }

            // stop pausing to allow others to use the stairs
            wandering_pause[m_ptr->wandering_idx] = 0;

            delete_monster(m_ptr->fy, m_ptr->fx);
            return (false);
        }

        /* Using flow information.  Check nearby grids, diagonals first. */
        for (d = 7; d >= 0; d--)
        {
            /* Get the location */
            y = y1 + ddy_ddd[d];
            x = x1 + ddx_ddd[d];

            /* Check Bounds */
            if (!in_bounds(y, x))
                continue;

            dist = flow_dist(m_ptr->wandering_idx, y, x);

            // ignore grids that are further than the current favourite
            if (closest < dist)
                continue;

            closest = dist;

            /* Save the location */
            *ty = y;
            *tx = x;
        }

        // if no useful square to wander into was found, then abort
        if (closest == FLOW_MAX_DIST - 1)
        {
            return (false);
        }
    }

    // success
    return (true);
}

/*
 * "Do not be seen."
 *
 * Monsters in LOS that want to retreat are primarily interested in
 * finding a nearby place that the character can't see into.
 * Search for such a place with the lowest cost to get to up to 15
 * grids away.
 *
 * Look outward from the monster's current position in a square-
 * shaped search pattern.  Calculate the approximate cost in monster
 * turns to get to each passable grid, using a crude route finder.  Penal-
 * ize grids close to or approaching the character.  Ignore hiding places
 * with no safe exit.  Once a passable grid is found that the character
 * can't see, the code will continue to search a little while longer,
 * depending on how pricey the first option seemed to be.
 *
 * If the search is successful, the monster will target that grid,
 * and (barring various special cases) run for it until it gets there.
 *
 * We use a limited waypoint system (see function "get_route_to_target()"
 * to reduce the likelihood that monsters will get stuck at a wall between
 * them and their target (which is kinda embarrassing...).
 *
 * This function does not yield perfect results; it is known to fail
 * in cases where the previous code worked just fine.  The reason why
 * it is used is because its failures are less common and (usually)
 * less embarrassing than was the case before.  In particular, it makes
 * monsters great at not being seen.
 *
 * This function is fairly expensive.  Call it only when necessary.
 */
static bool find_safety(monster_type* m_ptr, int* ty, int* tx)
{
    int i, j, d;

    int y, x, yy, xx;

    int countdown = HIDE_RANGE;

    int least_cost = 100;
    int least_cost_y = 0;
    int least_cost_x = 0;
    int chance, cost, parent_cost;
    bool dummy;
    bool stair;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Factors for converting table to actual dungeon grids */
    int conv_y, conv_x;

    /*
     * Allocate and initialize a table of movement costs.
     * Both axis must be (2 * HIDE_RANGE + 1).
     */
    byte safe_cost[HIDE_RANGE * 2 + 1][HIDE_RANGE * 2 + 1];

    for (i = 0; i < (HIDE_RANGE * 2 + 1); i++)
    {
        for (j = 0; j < (HIDE_RANGE * 2 + 1); j++)
        {
            safe_cost[i][j] = 0;
        }
    }

    conv_y = HIDE_RANGE - m_ptr->fy;
    conv_x = HIDE_RANGE - m_ptr->fx;

    /* Mark the origin */
    safe_cost[HIDE_RANGE][HIDE_RANGE] = 1;

    /* If the character's grid is in range, mark it as being off-limits */
    if ((ABS(m_ptr->fy - p_ptr->py) <= HIDE_RANGE)
        && (ABS(m_ptr->fx - p_ptr->px) <= HIDE_RANGE))
    {
        safe_cost[p_ptr->py + conv_y][p_ptr->px + conv_x] = 100;
    }

    /* Work outward from the monster's current position */
    for (d = 0; d < HIDE_RANGE; d++)
    {
        for (y = HIDE_RANGE - d; y <= HIDE_RANGE + d; y++)
        {
            for (x = HIDE_RANGE - d; x <= HIDE_RANGE + d;)
            {
                int x_tmp;

                /*
                 * Scan all grids of top and bottom rows, just
                 * outline other rows.
                 */
                if ((y != HIDE_RANGE - d) && (y != HIDE_RANGE + d))
                {
                    if (x == HIDE_RANGE + d)
                        x_tmp = 999;
                    else
                        x_tmp = HIDE_RANGE + d;
                }
                else
                    x_tmp = x + 1;

                /* Grid and adjacent grids must be legal */
                if (!in_bounds_fully(y - conv_y, x - conv_x))
                {
                    x = x_tmp;
                    continue;
                }

                /* Grid is inaccessible (or at least very difficult to enter) */
                if ((safe_cost[y][x] == 0) || (safe_cost[y][x] >= 100))
                {
                    x = x_tmp;
                    continue;
                }

                /* Get the accumulated cost to enter this grid */
                parent_cost = safe_cost[y][x];

                /* Scan all adjacent grids */
                for (i = 0; i < 8; i++)
                {
                    yy = y + ddy_ddd[i];
                    xx = x + ddx_ddd[i];

                    /* check bounds */
                    if ((yy < 0) || (yy > HIDE_RANGE * 2) || (xx < 0)
                        || (xx > HIDE_RANGE * 2))
                        continue;

                    /*
                     * Handle grids with empty cost and passable grids
                     * with costs we have a chance of beating.
                     */
                    if ((safe_cost[yy][xx] == 0)
                        || ((safe_cost[yy][xx] > parent_cost + 1)
                            && (safe_cost[yy][xx] < 100)))
                    {
                        /* Get the cost to enter this grid */
                        chance = cave_passable_mon(
                            m_ptr, yy - conv_y, xx - conv_x, &dummy);

                        /* Impassable */
                        if (!chance)
                        {
                            /* Cannot enter this grid */
                            safe_cost[yy][xx] = 100;
                            continue;
                        }

                        /* Calculate approximate cost (in monster turns) */
                        cost = 100 / chance;

                        /* Next to character */
                        if (distance(
                                yy - conv_y, xx - conv_x, p_ptr->py, p_ptr->px)
                            <= 1)
                        {
                            /* Don't want to maneuver next to the character */
                            cost += 3;
                        }

                        /* Mark this grid with a cost value */
                        safe_cost[yy][xx] = parent_cost + cost;

                        // check whether it is a stair and the monster can use
                        // these
                        stair = cave_stair_bold(yy - conv_y, xx - conv_x)
                            && (r_ptr->flags2 & (RF2_SMART))
                            && !(r_ptr->flags2 & (RF2_TERRITORIAL));

                        /* Character can't see this grid, or it is a stair... */
                        if (!player_can_see_bold(yy - conv_y, xx - conv_x)
                            || stair)
                        {
                            int this_cost = safe_cost[yy][xx];

                            /* Penalize grids that approach character */
                            if (ABS(p_ptr->py - (yy - conv_y))
                                < ABS(m_ptr->fy - (yy - conv_y)))
                            {
                                this_cost *= 2;
                            }
                            if (ABS(p_ptr->px - (xx - conv_x))
                                < ABS(m_ptr->fx - (xx - conv_x)))
                            {
                                this_cost *= 2;
                            }

                            // Value stairs very highly
                            if (stair)
                            {
                                this_cost /= 2;
                            }

                            /* Accept lower-cost, sometimes accept same-cost
                             * options */
                            if ((least_cost > this_cost)
                                || (least_cost == this_cost && one_in_(2)))
                            {
                                bool has_escape = false;

                                /* Scan all adjacent grids for escape routes */
                                for (j = 0; j < 8; j++)
                                {
                                    /* Calculate real adjacent grids */
                                    int yyy = yy - conv_y + ddy_ddd[i];
                                    int xxx = xx - conv_x + ddx_ddd[i];

                                    /* Check bounds */
                                    if (!in_bounds(yyy, xxx))
                                        continue;

                                    /* Look for any passable grid that isn't in
                                     * LOS */
                                    if ((!player_can_see_bold(yyy, xxx))
                                        && (cave_passable_mon(
                                            m_ptr, yyy, xxx, &dummy)))
                                    {
                                        /* Not a one-grid cul-de-sac */
                                        has_escape = true;
                                        break;
                                    }
                                }

                                /* Ignore cul-de-sacs other than stairs */
                                if ((has_escape == false) && !stair)
                                    continue;

                                least_cost = this_cost;
                                least_cost_y = yy;
                                least_cost_x = xx;

                                /*
                                 * Look hard for alternative hiding places if
                                 * this one seems pricey.
                                 */
                                countdown = 1 + least_cost - d;
                            }
                        }
                    }
                }

                /* Adjust x as instructed */
                x = x_tmp;
            }
        }

        /*
         * We found a good place a while ago, and haven't done better
         * since, so we're probably done.
         */
        if (countdown-- <= 0)
            break;
    }

    /* We found a place that can be reached in reasonable time */
    if (least_cost < 50)
    {
        /* Convert to actual dungeon grid. */
        y = least_cost_y - conv_y;
        x = least_cost_x - conv_x;

        /* Move towards the hiding place */
        *ty = y;
        *tx = x;

        /* Target the hiding place */
        m_ptr->target_y = y;
        m_ptr->target_x = x;

        return (true);
    }

    /* No good place found */
    return (false);
}

/*
 * Helper function for monsters that want to retreat from the character.
 * Used for any monster that is terrified, frightened, is looking for a
 * temporary hiding spot, or just wants to open up some space between it
 * and the character.
 *
 * If the monster is well away from danger, let it relax.
 * If the monster's current target is not in LOS, use it (+).
 * If the monster is not in LOS, and cannot pass through walls, try to
 * use flow (noise) information.
 * If the monster is in LOS, even if it can pass through walls,
 * search for a hiding place (helper function "find_safety()").
 * If no hiding place is found, and there seems no way out, go down
 * fighting.
 *
 * If none of the above solves the problem, run away blindly.
 *
 * (+) There is one exception to the automatic usage of a target.  If the
 * target is only out of LOS because of "knight's move" rules (distance
 * along one axis is 2, and along the other, 1), then the monster will try
 * to find another adjacent grid that is out of sight.  What all this boils
 * down to is that monsters can now run around corners properly!
 *
 * Return true if the monster did actually want to do anything.
 */
bool get_move_retreat(monster_type* m_ptr, int* ty, int* tx)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

    int i;
    int y, x;

    bool done = false;
    bool dummy;

    // if it can call for help, then it might
    if ((r_ptr->flags4 & (RF4_SHRIEK)) && percent_chance(r_ptr->freq_ranged))
    {
        shriek(m_ptr);
        return (false);
    }

    /* If the monster is well away from danger, let it relax. */
    if (m_ptr->cdis >= FLEE_RANGE)
    {
        return (false);
    }

    // intelligent monsters that are fleeing can try to use stairs
    if ((r_ptr->flags2 & (RF2_SMART)) && !(r_ptr->flags2 & (RF2_TERRITORIAL))
        && (m_ptr->stance == STANCE_FLEEING))
    {
        if (cave_stair_bold(m_ptr->fy, m_ptr->fx))
        {
            *ty = m_ptr->fy;
            *tx = m_ptr->fx;
            return (true);
        }

        // check for adjacent stairs and move towards one
        for (i = 0; i < 8; i++)
        {
            int yy = m_ptr->fy + ddy_ddd[i];
            int xx = m_ptr->fx + ddx_ddd[i];
            bool dummy;

            // check for (accessible) stairs
            if (cave_stair_bold(yy, xx)
                && (cave_passable_mon(m_ptr, yy, xx, &dummy) > 0)
                && (cave_m_idx[yy][xx] >= 0))
            {
                *ty = yy;
                *tx = xx;
                return (true);
            }
        }
    }

    // monsters that like ranged attacks a lot (e.g. archers) try to stay in
    // good shooting locations
    if (r_ptr->freq_ranged >= 50)
    {
        // int prev_cost = cave_cost[which_flow][m_ptr->fy][m_ptr->fx];
        int start = rand_int(8);

        bool acceptable = false;
        int best_score = 0;
        int best_y = m_ptr->fy, best_x = m_ptr->fx;
        int dist;

        // Set up the 'score to beat' as the score for the monster's current
        // square
        dist = distance_squared(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px);
        best_score += dist;
        if (projectable(
                m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px, PROJECT_STOP)
            && (m_ptr->cdis > 1))
            best_score += 100;

        // the position is only acceptable if it is not adjacent to the player
        if (m_ptr->cdis > 1)
            acceptable = true;

        // Set some hacky global variables so that the project_path()
        // function doesn't consider the monster's current location to block
        // line of fire.
        project_path_ignore = true;
        project_path_ignore_y = m_ptr->fy;
        project_path_ignore_x = m_ptr->fx;

        /* Look for adjacent shooting places */
        for (i = start; i < 8 + start; i++)
        {
            int score = 0;

            y = m_ptr->fy + ddy_ddd[i % 8];
            x = m_ptr->fx + ddx_ddd[i % 8];

            dist = distance_squared(y, x, p_ptr->py, p_ptr->px);

            /* Check Bounds */
            if (!in_bounds(y, x))
                continue;

            // skip the player's square
            if ((y == p_ptr->py) && (x == p_ptr->px))
                continue;

            /* Grid must be pretty easy to enter */
            if (cave_passable_mon(m_ptr, y, x, &dummy) < 50)
                continue;

            // skip adjacent squares
            if (distance(y, x, p_ptr->py, p_ptr->px) == 1)
                continue;

            // any position non-adjacent to the player will be acceptable
            acceptable = true;

            // reward distance from player
            score += dist;

            /* reward having a shot at the player */
            if (projectable(y, x, p_ptr->py, p_ptr->px, PROJECT_STOP)
                && (dist > 1))
                score += 100;

            /* Penalize any grid that doesn't have a lower flow (noise) cost. */
            // Sil-y: I'm not sure what this step does
            // if (cave_cost[which_flow][y][x] < prev_cost) score -= 10;

            if (score > best_score)
            {
                best_score = score;
                best_y = y;
                best_x = x;
            }
        }

        // Unset some hacky global variables so that the project_path()
        // function didn't consider the monster's current location to block line
        // of fire.
        project_path_ignore = false;
        project_path_ignore_y = 0;
        project_path_ignore_x = 0;

        if (acceptable)
        {
            *ty = best_y;
            *tx = best_x;

            /* Success */
            return (true);
        }

        // Sil-y:
        // This step is artificial stupidity for archers and other serious
        // ranged weapon users. They only evade you properly near walls if they
        // are: afraid or uniques or invisible Otherwise things are a bit too
        // annoying
        else if ((m_ptr->stance != STANCE_FLEEING)
            && !(r_ptr->flags1 & (RF1_UNIQUE)) && m_ptr->ml)
        {
            return (false);
        }
    }

    /* Monster has a target */
    if ((m_ptr->target_y) && (m_ptr->target_x))
    {
        /* It's out of LOS; keep using it, except in "knight's move" cases */
        if (!player_has_los_bold(m_ptr->target_y, m_ptr->target_x))
        {
            /* Get axis distance from character to current target */
            int dist_y = ABS(p_ptr->py - m_ptr->target_y);
            int dist_x = ABS(p_ptr->px - m_ptr->target_x);

            /* It's only out of LOS because of "knight's move" rules */
            if (((dist_y == 2) && (dist_x == 1))
                || ((dist_y == 1) && (dist_x == 2)))
            {
                /*
                 * If there is another grid adjacent to the monster that
                 * the character cannot see into, and it isn't any harder
                 * to enter, use it instead.  Prefer diagonals.
                 */
                for (i = 7; i >= 0; i--)
                {
                    y = m_ptr->fy + ddy_ddd[i];
                    x = m_ptr->fx + ddx_ddd[i];

                    /* Check Bounds */
                    if (!in_bounds(y, x))
                        continue;

                    if (player_has_los_bold(y, x))
                        continue;

                    if ((y == m_ptr->target_y) && (x == m_ptr->target_x))
                        continue;

                    if (cave_passable_mon(
                            m_ptr, m_ptr->target_y, m_ptr->target_x, &dummy)
                        > cave_passable_mon(m_ptr, y, x, &dummy))
                        continue;

                    m_ptr->target_y = y;
                    m_ptr->target_x = x;
                    break;
                }
            }

            /* Move towards the target */
            *ty = m_ptr->target_y;
            *tx = m_ptr->target_x;
            return (true);
        }

        /* It's in LOS, but not a stair; cancel it. */
        else if (!cave_stair_bold(m_ptr->target_y, m_ptr->target_x))
        {
            m_ptr->target_y = 0;
            m_ptr->target_x = 0;
        }
    }

    /* The monster is not in LOS, but thinks it's still too close. */
    if (!player_has_los_bold(m_ptr->fy, m_ptr->fx))
    {
        /* Run away from noise */
        if (flow_dist(m_idx, m_ptr->fy, m_ptr->fx) < FLOW_MAX_DIST)
        {
            /* Look at adjacent grids, diagonals first */
            for (i = 7; i >= 0; i--)
            {
                y = m_ptr->fy + ddy_ddd[i];
                x = m_ptr->fx + ddx_ddd[i];

                /* Check bounds */
                if (!in_bounds(y, x))
                    continue;

                /* Accept the first non-visible grid with a higher cost */
                if (flow_dist(m_idx, y, x)
                    > flow_dist(m_idx, m_ptr->fy, m_ptr->fx))
                {
                    if (!player_has_los_bold(y, x))
                    {
                        *ty = y;
                        *tx = x;
                        done = true;
                        break;
                    }
                }
            }

            /* Return if successful */
            if (done)
                return (true);
        }

        /* No flow info, or don't need it -- see bottom of function */
    }

    /* The monster is in line of sight. */
    else
    {
        int prev_dist = flow_dist(m_idx, m_ptr->fy, m_ptr->fx);
        int start = rand_int(8);

        /* Look for adjacent hiding places */
        for (i = start; i < 8 + start; i++)
        {
            y = m_ptr->fy + ddy_ddd[i % 8];
            x = m_ptr->fx + ddx_ddd[i % 8];

            /* Check Bounds */
            if (!in_bounds(y, x))
                continue;

            /* No grids in LOS */
            if (player_has_los_bold(y, x))
                continue;

            /* Grid must be pretty easy to enter */
            if (cave_passable_mon(m_ptr, y, x, &dummy) < 50)
                continue;

            /* Accept any grid that doesn't have a lower flow (noise) cost. */
            if (flow_dist(m_idx, y, x) >= prev_dist)
            {
                *ty = y;
                *tx = x;
                prev_dist = flow_dist(m_idx, y, x);

                /* Success */
                return (true);
            }
        }

        /* Find a nearby grid not in LOS of the character. */
        if (find_safety(m_ptr, ty, tx) == true)
            return (true);

        /*
         * No safe place found.  If monster is in LOS and close,
         * it will turn to fight.
         */
        if ((player_has_los_bold(m_ptr->fy, m_ptr->fx))
            && ((m_ptr->cdis < TURN_RANGE) || (m_ptr->mspeed < p_ptr->pspeed))
            && !p_ptr->truce && (r_ptr->freq_ranged < 50))
        {
            /* Message if visible */
            if (m_ptr->ml)
            {
                char m_name[80];

                /* Get the monster name */
                monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                /* Dump a message */
                msg_format("%^s panics.", m_name);
            }

            // boost morale and make the monster aggressive
            m_ptr->tmp_morale = MAX(m_ptr->tmp_morale + 60, 60);
            calc_morale(m_ptr);
            calc_stance(m_ptr);
            m_ptr->mflag |= (MFLAG_AGGRESSIVE);

            return (true);
        }
    }

    // Sil-y: This code below seemed hopelessly wrong, so I'm trying out a new
    // version
    /* Move directly away from character. */
    *ty = m_ptr->fy - (p_ptr->py - m_ptr->fy);
    *tx = m_ptr->fx - (p_ptr->px - m_ptr->fx);

    /* We want to run away */
    return (true);
}

/*
 * Helper function for monsters that want to advance toward the character.
 * Assumes that the monster isn't frightened, and is not in LOS of the
 * character.
 *
 * Ghosts and rock-eaters do not use flow information, because they
 * can - in general - move directly towards the character.  We could make
 * them look for a grid at their preferred range, but the character
 * would then be able to avoid them better (it might also be a little
 * hard on those poor warriors...).
 *
 * Other monsters will use target information, then their ears, then their
 * noses (if they can), and advance blindly if nothing else works.
 *
 * When flowing, monsters prefer non-diagonal directions.
 *
 * XXX - At present, this function does not handle difficult terrain
 * intelligently.  Monsters using flow may bang right into a door that
 * they can't handle.  Fixing this may require code to set monster
 * paths.
 */
void get_move_advance(monster_type* m_ptr, int* ty, int* tx)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i;

    byte y, x, y1, x1;

    int closest = FLOW_MAX_DIST;

    bool can_use_sound = false;
    bool can_use_scent = false;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    int m_idx;

    // Some monsters don't try to pursue when out of sight
    if ((r_ptr->flags2 & (RF2_TERRITORIAL))
        && !los(py, px, m_ptr->fy, m_ptr->fx))
    {
        // remember that the monster behaves this
        l_ptr->flags2 |= (RF2_TERRITORIAL);

        *ty = m_ptr->fy;
        *tx = m_ptr->fx;

        // sometimes become unwary and wander back to its lair
        if (one_in_(10) && (m_ptr->alertness >= ALERTNESS_ALERT))
            set_alertness(m_ptr, m_ptr->alertness - 1);

        return;
    }

    /* Monster location */
    y1 = m_ptr->fy;
    x1 = m_ptr->fx;

    // Monster index
    m_idx = cave_m_idx[y1][x1];

    /* Use target information if available */
    if ((m_ptr->target_y) && (m_ptr->target_x))
    {
        *ty = m_ptr->target_y;
        *tx = m_ptr->target_x;
        return;
    }

    /* If we can hear noises, advance towards them */
    if (flow_dist(m_idx, y1, x1) < FLOW_MAX_DIST)
    {
        can_use_sound = true;
    }

    /* Otherwise, try to follow a scent trail */
    else if (monster_can_smell(m_ptr))
    {
        can_use_scent = true;
    }

    /* Otherwise */
    if ((!can_use_sound) && (!can_use_scent))
    {
        // sight but no 'sound' implies blocked by a chasm, so get out of there!
        if (los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
        {
            get_move_retreat(m_ptr, ty, tx);
            return;
        }

        // no sound, no scent, no sight: advance blindly
        else
        {
            *ty = py;
            *tx = px;
            return;
        }
    }

    /* Using flow information.  Check nearby grids, diagonals first. */
    for (i = 7; i >= 0; i--)
    {
        /* Get the location */
        y = y1 + ddy_ddd[i];
        x = x1 + ddx_ddd[i];

        /* Check Bounds */
        if (!in_bounds(y, x))
            continue;

        /* We're following a scent trail */
        if (can_use_scent)
        {
            int age = get_scent(y, x);
            if (age == -1)
                continue;

            /* Accept younger scent */
            if (closest < age)
                continue;
            closest = age;
        }

        /* We're using sound */
        else
        {
            int dist = flow_dist(m_idx, y, x);

            /* Accept louder sounds */
            if (closest < dist)
                continue;
            closest = dist;
        }

        /* Save the location */
        *ty = y;
        *tx = x;
    }
}

// This determines how vulnerable the player is to monster attacks
// It combines elements for available spaces to attack from and for
// the player's condition and other monsters attacking
//
// I'm sure it could be further improved

int calc_vulnerability(int fy, int fx)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int dy, dx;
    int dir;
    int vulnerability;

    // reset the vulnerability
    vulnerability = 0;

    // determine the main direction from the player to the monster
    dir = rough_direction(py, px, fy, fx);

    // extract the deltas from the direction
    dy = ddy[dir];
    dx = ddx[dir];

    // if monster in an orthogonal direction   753
    //                                         8@1 m
    //                                         642
    if (dy * dx == 0)
    {
        // increase vulnerability for each open square towards the monster
        if (cave_floor_bold(py + dy, px + dx))
            vulnerability++; // direction 1
        if (cave_floor_bold(py + dx + dy, px - dy + dx))
            vulnerability++; // direction 2
        if (cave_floor_bold(py - dx + dy, px + dy + dx))
            vulnerability++; // direction 3
        if (cave_floor_bold(py + dx, px - dy))
            vulnerability++; // direction 4
        if (cave_floor_bold(py - dx, px + dy))
            vulnerability++; // direction 5

        // increase vulnerability for monsters already engaged with the
        // player... if (cave_m_idx[py+dy, px+dx])       vulnerability++;    //
        // direction 1
        if (attacker_at(py + dx + dy, px - dy + dx))
            vulnerability++; // direction 2
        if (attacker_at(py - dx + dy, px + dy + dx))
            vulnerability++; // direction 3
        if (attacker_at(py + dx, px - dy))
            vulnerability++; // direction 4
        if (attacker_at(py - dx, px + dy))
            vulnerability++; // direction 5

        // ...especially if they are behind the player
        if (attacker_at(py + dx - dy, px - dy - dx))
            vulnerability += 2; // direction 6
        if (attacker_at(py - dx - dy, px + dy - dx))
            vulnerability += 2; // direction 7
        if (attacker_at(py - dy, px - dx))
            vulnerability += 2; // direction 8
    }
    // if monster in a diagonal direction   875
    //                                      6@3
    //                                      421
    //                                          m
    else
    {
        // increase vulnerability for each open square towards the monster
        if (cave_floor_bold(py + dy, px + dx))
            vulnerability++; // direction 1
        if (cave_floor_bold(py + dy, px))
            vulnerability++; // direction 2
        if (cave_floor_bold(py, px + dx))
            vulnerability++; // direction 3
        if (cave_floor_bold(py + dx, px - dy))
            vulnerability++; // direction 4
        if (cave_floor_bold(py - dx, px + dy))
            vulnerability++; // direction 5

        // increase vulnerability for monsters already engaged with the
        // player... if (cave_m_idx[py+dy, px+dx)) vulnerability++;    //
        // direction 1
        if (attacker_at(py + dy, px))
            vulnerability++; // direction 2
        if (attacker_at(py, px + dx))
            vulnerability++; // direction 3
        if (attacker_at(py + dx, px - dy))
            vulnerability++; // direction 4
        if (attacker_at(py - dx, px + dy))
            vulnerability++; // direction 5

        // ...especially if they are behind the player
        if (attacker_at(py - dy, px))
            vulnerability += 2; // direction 6
        if (attacker_at(py, px - dx))
            vulnerability += 2; // direction 7
        if (attacker_at(py - dy, px - dx))
            vulnerability += 2; // direction 8
    }

    if (!p_ptr->active_ability[S_WIL][WIL_FORMIDABLE])
    {
        // Take player's health into account
        switch (health_level(p_ptr->chp, p_ptr->mhp))
        {
        case HEALTH_WOUNDED:
            vulnerability += 1;
            break; // <= 75% health
        case HEALTH_BADLY_WOUNDED:
            vulnerability += 1;
            break; // <= 50% health
        case HEALTH_ALMOST_DEAD:
            vulnerability += 2;
            break; // <= 25% health
        }
    }

    // Take player's conditions into account
    if (p_ptr->blind || p_ptr->image || p_ptr->confused || p_ptr->afraid
        || p_ptr->entranced || (p_ptr->stun > 50) || p_ptr->slow)
    {
        vulnerability += 2;
    }

    return vulnerability;
}

// This determines how hesitant the monster is to attack.
// If the hesitance is lower than the player's vulnerability, it will attack
//
// The main way to gain hesitance is to have similar smart monsters who could
// gang up if they waited for the player to get into the open.

int calc_hesitance(monster_type* m_ptr)
{
    int x, y;
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;
    int hesitance = 1;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    // gain hesitance for up to one nearby similar monster
    // who isn't yet engaged in combat
    for (y = -5; y <= +5; y++)
    {
        for (x = -5; x <= +5; x++)
        {
            if (!((x == 0) && (y == 0)) && in_bounds(fy + y, fx + x))
            {
                if (cave_m_idx[fy + y][fx + x] > 0)
                {
                    if (similar_monsters(fy, fx, fy + y, fx + x)
                        && (distance(fy + y, fx + x, p_ptr->py, p_ptr->px) > 1)
                        && (hesitance < 2))
                    {
                        hesitance++;
                    }
                }
            }
        }
    }

    // archers should be slightly more hesitant as they are in an excellent
    // situation
    if ((r_ptr->freq_ranged > 30) && (hesitance == 2))
    {
        hesitance++;
    }

    return (hesitance);
}

bool get_route_to_target(monster_type* m_ptr, int* ty, int* tx)
{
    int i, j;
    int y, x, yy, xx;
    int tar_y, tar_x, dist_y, dist_x;

    bool dummy;
    bool below = false;
    bool right = false;

    tar_y = 0;
    tar_x = 0;

    /* Is the target further away vertically or horizontally? */
    dist_y = ABS(m_ptr->target_y - m_ptr->fy);
    dist_x = ABS(m_ptr->target_x - m_ptr->fx);

    /* Target is further away vertically than horizontally */
    if (dist_y > dist_x)
    {
        /* Find out if the target is below the monster */
        if (m_ptr->target_y - m_ptr->fy > 0)
            below = true;

        /* Search adjacent grids */
        for (i = 0; i < 8; i++)
        {
            y = m_ptr->fy + ddy_ddd[i];
            x = m_ptr->fx + ddx_ddd[i];

            /* Check bounds */
            if (!in_bounds_fully(y, x))
                continue;

            /* Grid is not passable */
            if (!cave_passable_mon(m_ptr, y, x, &dummy))
                continue;

            /* Grid will take me further away */
            if (((below) && (y < m_ptr->fy)) || ((!below) && (y > m_ptr->fy)))
            {
                continue;
            }

            /* Grid will not take me closer or further */
            else if (y == m_ptr->fy)
            {
                /* See if it leads to better things */
                for (j = 0; j < 8; j++)
                {
                    yy = y + ddy_ddd[j];
                    xx = x + ddx_ddd[j];

                    /* Grid does lead to better things */
                    if (((below) && (yy > m_ptr->fy))
                        || ((!below) && (yy < m_ptr->fy)))
                    {
                        /* But it is not passable */
                        if (!cave_passable_mon(m_ptr, yy, xx, &dummy))
                            continue;

                        /*
                         * Accept (original) grid, but don't immediately claim
                         * success
                         */
                        tar_y = y;
                        tar_x = x;
                    }
                }
            }

            /* Grid will take me closer */
            else
            {
                /* Don't look this gift horse in the mouth. */
                *ty = y;
                *tx = x;
                return (true);
            }
        }
    }

    /* Target is further away horizontally than vertically */
    else if (dist_x > dist_y)
    {
        /* Find out if the target is right of the monster */
        if (m_ptr->target_x - m_ptr->fx > 0)
            right = true;

        /* Search adjacent grids */
        for (i = 0; i < 8; i++)
        {
            y = m_ptr->fy + ddy_ddd[i];
            x = m_ptr->fx + ddx_ddd[i];

            /* Check bounds */
            if (!in_bounds_fully(y, x))
                continue;

            /* Grid is not passable */
            if (!cave_passable_mon(m_ptr, y, x, &dummy))
                continue;

            /* Grid will take me further away */
            if (((right) && (x < m_ptr->fx)) || ((!right) && (x > m_ptr->fx)))
            {
                continue;
            }

            /* Grid will not take me closer or further */
            else if (x == m_ptr->fx)
            {
                /* See if it leads to better things */
                for (j = 0; j < 8; j++)
                {
                    yy = y + ddy_ddd[j];
                    xx = x + ddx_ddd[j];

                    /* Grid does lead to better things */
                    if (((right) && (xx > m_ptr->fx))
                        || ((!right) && (xx < m_ptr->fx)))
                    {
                        /* But it is not passable */
                        if (!cave_passable_mon(m_ptr, yy, xx, &dummy))
                            continue;

                        /* Accept (original) grid, but don't immediately claim
                         * success */
                        tar_y = y;
                        tar_x = x;
                    }
                }
            }

            /* Grid will take me closer */
            else
            {
                /* Don't look this gift horse in the mouth. */
                *ty = y;
                *tx = x;
                return (true);
            }
        }
    }

    /* Target is the same distance away along both axes. */
    else
    {
        /* XXX XXX - code something later to fill this hole. */
        return (false);
    }

    /* If we found a solution, claim success */
    if ((tar_y) && (tar_x))
    {
        *ty = tar_y;
        *tx = tar_x;
        return (true);
    }

    /* No luck */
    return (false);
}
