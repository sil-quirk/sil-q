/* File: level-generation-terrain-placement.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

bool alloc_stairs(int feat, int num)
{
    int x;

    /* Stairs can be placed anywhere on the map - rooms or corridors */

    /* Place "num" stairs */
    for (x = 0; x < num; x++)
    {
        int i;

        int yy, xx;

        for (i = 0; i < 1000; i++)
        {
            yy = rand_int(p_ptr->cur_map_hgt);
            xx = rand_int(p_ptr->cur_map_wid);

            /* make sure the square is empty (floor) and has no adjacent doors */
            if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx))
                if ((cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD))
                {
                    break;
                }
        }

        /* Failed to find valid location after 1000 attempts */
        if (i == 1000)
        {
            log_trace("alloc_stairs: Failed to find valid location for stair %d/%d after 1000 attempts", x+1, num);
            return (false);
        }

        /* Surface -- must go down */
        if (!p_ptr->depth)
        {
            /* Clear previous contents, add down stairs */
            cave_set_feat(yy, xx, FEAT_MORE);
        }

        /* Bottom -- must go up */
        else if (p_ptr->depth >= MORGOTH_DEPTH)
        {
            /* Clear previous contents, add up stairs */
            if (x != 0)
                cave_set_feat(yy, xx, FEAT_LESS);
            else
                cave_set_feat(yy, xx, choose_up_stairs());
        }

        /* Requested type */
        else
        {
            /* Allow shafts, but guarantee the first one is an ordinary stair */
            if (x != 0)
            {
                if (feat == FEAT_LESS)
                    feat = choose_up_stairs();
                else if (feat == FEAT_MORE)
                    feat = choose_down_stairs();
            }

            /* Clear previous contents, add stairs */
            cave_set_feat(yy, xx, feat);
        }
    }

    return (true);
}

bool feat_within_los(int y0, int x0, int feat)
{
    int y, x;

    bool detect = false;

    /* Scan the visible area */
    for (y = y0 - 15; y < y0 + 15; y++)
    {
        for (x = x0 - 15; x < x0 + 15; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!los(y0, x0, y, x))
                continue;

            /* Detect invisible traps */
            if (cave_feat[y][x] == feat)
            {
                detect = true;
            }
        }
    }

    /* Result */
    return (detect);
}

/*
 * Are there any stairs within line of sight?
 */
bool stairs_within_los(int y, int x)
{
    if (feat_within_los(y, x, FEAT_LESS))
        return (true);
    if (feat_within_los(y, x, FEAT_MORE))
        return (true);
    if (feat_within_los(y, x, FEAT_LESS_SHAFT))
        return (true);
    if (feat_within_los(y, x, FEAT_MORE_SHAFT))
        return (true);

    // else:

    return (false);
}

/*
 * Determines the chance (out of 1000) that a trap will be placed in a given
 * square.
 */
int trap_placement_chance(int y, int x)
{
    int yy, xx;

    int chance = 0;
    /* extra traps from CUR_TRAPS */
    int bonus_traps = curse_flag_count_cur(CUR_TRAPS);
    if (bonus_traps)
        chance += 10 * bonus_traps;   /* +10/20/30% on top of normal */

    // extra chance of having a trap for certain squares inside rooms
    if (cave_clean_bold(y, x) && (cave_info[y][x] & (CAVE_ROOM)))
    {
        chance = 1;

        // check the squares that neighbour (y,x)
        for (yy = y - 1; yy <= y + 1; yy++)
        {
            for (xx = x - 1; xx <= x + 1; xx++)
            {
                if (!((yy == y) && (xx == x)))
                {
                    // item
                    if (cave_o_idx[yy][xx] != 0)
                        chance += 10;

                    // stairs
                    if (cave_stair_bold(yy, xx))
                        chance += 10;

                    // closed doors (including secret)
                    if (cave_any_closed_door_bold(yy, xx))
                        chance += 10;
                }
            }
        }

        // opposing impassable squares (chasm or wall)
        if (cave_impassable_bold(y - 1, x) && cave_impassable_bold(y + 1, x))
            chance += 10;
        if (cave_impassable_bold(y, x - 1) && cave_impassable_bold(y, x + 1))
            chance += 10;
    }

    /* Small caves (CA-blob partitions): sprinkle a few extra traps on open cave floors. */
    if (p_ptr->depth >= 8 && cave_clean_bold(y, x) && !(cave_info[y][x] & CAVE_ICKY)
        && (level_partition_kind_for_point(y, x) == LEVEL_PART_CAVEY))
    {
        chance = MAX(chance, 2);
    }

    return (chance);
}

/*
 * Place traps randomly on the level.
 * Biased towards certain sneaky locations.
 */
void place_traps(void)
{
    int y, x;

    // scan the map
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            // randomly determine whether to place a trap based on the above
            if (dieroll(1000) <= trap_placement_chance(y, x))
            {
                place_trap(y, x);
            }
        }
    }
}

bool place_rubble_player(void)
{
    int r;
    int y, x;
    int i, panels;
    bool niena_level = (quest_lottery_winner == QUEST_ID_NIENA);

    /* Basic "amount" */

    panels = (p_ptr->cur_map_hgt / PANEL_HGT)
        * (p_ptr->cur_map_wid / PANEL_WID_FIXED);

    r = dieroll(panels / 3);

    // occasionally produce much more rubble on deep levels
    if ((p_ptr->depth >= 10) && one_in_(10))
        r += panels * 2;

    /* Put some rubble in corridors */
    alloc_object_global(ALLOC_SET_BOTH, ALLOC_TYP_RUBBLE, r, false);

    /* simple way to place player */
    for (i = 0; i <= 100; i++)
    {
        y = rand_int(p_ptr->cur_map_hgt);
        x = rand_int(p_ptr->cur_map_wid);
        // require empty square that isn't in an interesting room or vault
        if (cave_naked_bold(y, x) && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            // require a room if it is the first level
            if ((playerturn > 0) || (cave_info[y][x] & (CAVE_ROOM)))
            {
                // don't generate stairs within line of sight if player arrived
                // using stairs
                if (!stairs_within_los(y, x) || (p_ptr->create_stair == false))
                {
                    if (niena_level)
                    {
                        int nearest_down = calculate_nearest_down_stair_distance_from(y, x);
                        if (nearest_down < 87)
                            continue;
                    }

                    player_place(y, x);
                    break;
                }
            }
        }
        if (i == 100)
        {
            log_trace("place_rubble_player failed: Could not find suitable player placement after 100 attempts");
            return (false);
        }
    }

    /* Nienna levels need a long run to the only down stair; do an exhaustive
     * fallback scan before giving up on the level. */
    if (niena_level)
    {
        int nearest_down = calculate_nearest_down_stair_distance_from(p_ptr->py, p_ptr->px);
        if (nearest_down < 87)
        {
            for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
            {
                for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
                {
                    if (!cave_naked_bold(y, x) || (cave_info[y][x] & CAVE_ICKY))
                        continue;
                    if ((playerturn == 0) && !(cave_info[y][x] & CAVE_ROOM))
                        continue;

                    nearest_down = calculate_nearest_down_stair_distance_from(y, x);
                    if (nearest_down < 87)
                        continue;

                    player_place(y, x);
                    return true;
                }
            }

            log_trace("place_rubble_player failed: Nienna level could not find player start at least 87 from the down stair");
            return false;
        }
    }

    return (true);
}
