/* File: level-generation-finalize.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

void set_perm_boundry(void)
{
    int y, x;

    /* Special boundary walls -- Top */
    for (x = 0; x < p_ptr->cur_map_wid; x++)
    {
        y = 0;

        /* Clear previous contents, add perma-wall */
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }

    /* Special boundary walls -- Bottom */
    for (x = 0; x < p_ptr->cur_map_wid; x++)
    {
        y = p_ptr->cur_map_hgt - 1;

        /* Clear previous contents, add perma-wall */
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }

    /* Special boundary walls -- Left */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        x = 0;

        /* Clear previous contents, add perma-wall */
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }

    /* Special boundary walls -- Right */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        x = p_ptr->cur_map_wid - 1;

        /* Clear previous contents, add perma-wall */
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }
}

/* Start new level with a map entirely of basic granite */
void basic_granite(void)
{
    int y, x;
    int depth_color = get_depth_color(p_ptr->depth);

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Create granite wall with depth-based color */
            cave_set_feat_with_color(y, x, FEAT_WALL_EXTRA, depth_color);

            // initialise the corridor id array
            cave_corridor1[y][x] = -1;
            cave_corridor2[y][x] = -1;
            cave_escape_tunnel[y][x] = false;
        }
    }
}

void make_patch_of_sunlight(int y, int x)
{
    int m, n, floor;

    if (cave_info[y][x] & CAVE_GLOW)
    {
        floor = 0;
        for (n = (y - 1); n <= (y + 1); n++)
        {
            for (m = (x - 1); m <= (x + 1); m++)
            {
                if (cave_feat[n][m] == FEAT_FLOOR)
                    floor++;
            }
        }
        if (floor > 6)
        {
            if (cave_feat[y][x] == FEAT_FLOOR
                && !((y == p_ptr->py) && (x == p_ptr->px)))
                cave_set_feat(y, x, FEAT_RUBBLE);
            for (n = (y - 1); n <= (y + 1); n++)
            {
                for (m = (x - 1); m <= (x + 1); m++)
                {
                    if ((n == p_ptr->py) && (m == p_ptr->px))
                        continue;
                    if ((cave_info[n][m] & CAVE_GLOW)
                        && cave_feat[n][m] == FEAT_FLOOR && one_in_(4))
                    {
                        if (cave_feat[n][m] == FEAT_FLOOR)
                            cave_set_feat(n, m, FEAT_SUNLIGHT);
                    }
                }
            }
        }
    }
}

void make_patches_of_sunlight()
{
    int i, x, y;

    // bunch near the player
    for (i = 0; i < 40; ++i)
    {
        y = rand_range(MAX(p_ptr->py - 5, 1),
            MIN(p_ptr->py + 5, p_ptr->cur_map_hgt - 2));
        x = rand_range(MAX(p_ptr->px - 5, 1),
            MIN(p_ptr->px + 5, p_ptr->cur_map_wid - 2));
        make_patch_of_sunlight(y, x);
    }

    // and a few scattered over the first level
    for (i = 0; i < 20; ++i)
    {
        y = rand_range(10, p_ptr->cur_map_hgt - 10);
        x = rand_range(10, p_ptr->cur_map_wid - 10);
        make_patch_of_sunlight(y, x);
    }
}

bool varda_sunlight_tile_ok(int y, int x, bool require_empty)
{
    if (!in_bounds_fully(y, x)) return false;
    if (cave_feat[y][x] != FEAT_SUNLIGHT) return false;
    if (!cave_floor_bold(y, x)) return false;
    if (cave_info[y][x] & CAVE_ICKY) return false;
    if (require_empty && cave_m_idx[y][x] != 0) return false;

    return true;
}

void varda_make_sunlight_pool(int y, int x)
{
    for (int ny = y - 1; ny <= y + 1; ny++)
    {
        for (int nx = x - 1; nx <= x + 1; nx++)
        {
            if (!in_bounds_fully(ny, nx)) continue;
            if (cave_info[ny][nx] & CAVE_ICKY) continue;
            if (cave_feat[ny][nx] != FEAT_FLOOR && cave_feat[ny][nx] != FEAT_RAGE_FLOOR
                && cave_feat[ny][nx] != FEAT_SUNLIGHT) continue;
            cave_set_feat(ny, nx, FEAT_SUNLIGHT);
        }
    }
}

bool varda_no_rubble_path_tile_ok(int y, int x,
    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    if (!access[y][x]) return false;

    /* Avoid spawning her adjacent to the player (quest can auto-trigger before encounter XP). */
    if (distance(p_ptr->py, p_ptr->px, y, x) < 2) return false;

    return true;
}

/*
 * Pick a sunlight tile suitable for spawning Varda:
 * - empty
 * - reachable from the player without digging rubble / crossing chasms
 * - not adjacent to the player
 *
 * Returns the number of spawnable sunlight tiles found (0 if none).
 * Optionally returns:
 * - total sunlight tiles (occupied or not)
 * - empty sunlight tiles (regardless of reachability)
 */
int pick_varda_sunlight_spawn_tile(int *out_y, int *out_x,
    int *out_total_sunlight, int *out_empty_sunlight)
{
    int total = 0;
    int empty = 0;
    int spawnable = 0;
    int pick_y = -1;
    int pick_x = -1;

    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            access[y][x] = false;
        }
    }
    flood_access(p_ptr->py, p_ptr->px, access, false);

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (!varda_sunlight_tile_ok(y, x, false)) continue;
            total++;

            if (!varda_sunlight_tile_ok(y, x, true)) continue;
            empty++;

            if (!varda_no_rubble_path_tile_ok(y, x, access)) continue;
            spawnable++;
            if (one_in_(spawnable)) {
                pick_y = y;
                pick_x = x;
            }
        }
    }

    if (out_total_sunlight) *out_total_sunlight = total;
    if (out_empty_sunlight) *out_empty_sunlight = empty;
    if (spawnable > 0 && out_y && out_x) {
        *out_y = pick_y;
        *out_x = pick_x;
    }

    return spawnable;
}

bool force_varda_sunlight_tile(int *out_y, int *out_x)
{
    int count = 0;
    int pick_y = -1;
    int pick_x = -1;

    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            access[y][x] = false;
        }
    }
    flood_access(p_ptr->py, p_ptr->px, access, false);

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (cave_info[y][x] & CAVE_ICKY) continue;
            if (!varda_no_rubble_path_tile_ok(y, x, access)) continue;
            if (!cave_empty_bold(y, x)) continue;
            if (cave_feat[y][x] != FEAT_FLOOR && cave_feat[y][x] != FEAT_RAGE_FLOOR) continue;

            count++;
            if (one_in_(count)) {
                pick_y = y;
                pick_x = x;
            }
        }
    }

    if (count == 0) return false;

    varda_make_sunlight_pool(pick_y, pick_x);
    if (out_y) {
        *out_y = pick_y;
        *out_x = pick_x;
    }

    return true;
}

void ensure_sunlight_for_varda(void)
{
    /* Only relevant for the first few levels */
    if (p_ptr->depth > 3) return;

    /* Check for valid sunlight spawn locations */
    int total_sunlight = 0;
    int empty_sunlight = 0;
    int spawnable_sunlight = pick_varda_sunlight_spawn_tile(NULL, NULL, &total_sunlight, &empty_sunlight);

    if (spawnable_sunlight == 0) {
        log_trace("Varda spawn: No valid sunlight spawn locations detected (total=%d, empty=%d), seeding patches",
            total_sunlight, empty_sunlight);
        make_patches_of_sunlight();

        /* Verify at least one valid location exists after patching */
        total_sunlight = 0;
        empty_sunlight = 0;
        spawnable_sunlight = pick_varda_sunlight_spawn_tile(NULL, NULL, &total_sunlight, &empty_sunlight);

        if (spawnable_sunlight > 0) {
            log_trace("Varda spawn: Verified sunlight after patching (total=%d, empty=%d, spawnable=%d)",
                total_sunlight, empty_sunlight, spawnable_sunlight);
            return;
        }

        int forced_y = -1;
        int forced_x = -1;
        if (force_varda_sunlight_tile(&forced_y, &forced_x)) {
            log_trace("Varda spawn: Forced sunlight at (%d,%d) to guarantee spawn", forced_y, forced_x);
            return;
        }

        log_trace("Varda spawn: WARNING - No valid sunlight locations after patching or forcing!");
    }
}

int morgoth_escape_path_step_cost(monster_type* m_ptr, int y, int x)
{
    bool bash = false;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int chance = cave_passable_mon(m_ptr, y, x, &bash);
    int cost;

    if (chance <= 0)
        return 0;

    cost = MAX(1, 100 / chance);

    if (cave_any_closed_door_bold(y, x) && !bash)
    {
        if (!((r_ptr->flags2 & (RF2_PASS_DOOR))
                || (r_ptr->flags2 & (RF2_PASS_WALL))))
        {
            cost += 1;
        }
    }
    else if (cave_wall_bold(y, x) && (r_ptr->flags2 & (RF2_TUNNEL_WALL)))
    {
        cost += (cave_feat[y][x] == FEAT_RUBBLE) ? 1 : 2;
    }
    else if (cave_wall_bold(y, x) && (r_ptr->flags2 & (RF2_KILL_WALL)))
    {
        cost += 1;
    }

    return cost;
}

void build_morgoth_escape_path_distances(
    int path_dist[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], int max_path)
{
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static byte in_queue[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static const int ddy8[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int ddx8[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    monster_type morgoth;
    int head = 0;
    int tail = 0;
    int queued = 0;

    for (int yy = 0; yy < p_ptr->cur_map_hgt; yy++)
    {
        for (int xx = 0; xx < p_ptr->cur_map_wid; xx++)
        {
            path_dist[yy][xx] = max_path + 1;
            in_queue[yy][xx] = false;
        }
    }

    if (!in_bounds_fully(p_ptr->py, p_ptr->px))
        return;

    memset(&morgoth, 0, sizeof(morgoth));
    morgoth.r_idx = R_IDX_MORGOTH;
    morgoth.alertness = ALERTNESS_ALERT;
    morgoth.stance = STANCE_CONFIDENT;

    path_dist[p_ptr->py][p_ptr->px] = 0;
    queue[tail++] = p_ptr->py * MAX_DUNGEON_WID + p_ptr->px;
    in_queue[p_ptr->py][p_ptr->px] = true;
    queued = 1;

    while (queued > 0)
    {
        int idx = queue[head++];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;

        if (head == (int)N_ELEMENTS(queue))
            head = 0;

        in_queue[cy][cx] = false;
        queued--;

        for (int d = 0; d < 8; d++)
        {
            int ny = cy + ddy8[d];
            int nx = cx + ddx8[d];
            int step_cost;
            int new_dist;

            if (!in_bounds_fully(ny, nx))
                continue;

            step_cost = morgoth_escape_path_step_cost(&morgoth, ny, nx);
            if (step_cost <= 0)
                continue;

            new_dist = path_dist[cy][cx] + step_cost;
            if (new_dist > max_path)
                continue;
            if (new_dist >= path_dist[ny][nx])
                continue;

            path_dist[ny][nx] = new_dist;

            if (!in_queue[ny][nx] && queued < (int)N_ELEMENTS(queue))
            {
                queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                if (tail == (int)N_ELEMENTS(queue))
                    tail = 0;
                in_queue[ny][nx] = true;
                queued++;
            }
        }
    }
}

bool morgoth_escape_spawn_path_ok(int y, int x, int distance_roll,
    int path_dist[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    if (distance_roll <= 0)
        return false;

    return path_dist[y][x] <= distance_roll * 3;
}
