/* File: level-generation-terrain-connectivity.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

bool connectivity_rescue_traversable(int ry, int rx)
{
    if (!in_bounds_fully(ry, rx))
        return false;

    if (cave_feat[ry][rx] == FEAT_WALL_PERM)
        return false;
    if (cave_feat[ry][rx] == FEAT_CHASM)
        return false;

    bool is_wall = (cave_feat[ry][rx] >= FEAT_WALL_HEAD)
        && (cave_feat[ry][rx] <= FEAT_WALL_TAIL)
        && (cave_feat[ry][rx] != FEAT_SECRET);

    /* Never carve through Morgoth's vault walls: require using the forced doors. */
    if (morgoth_level_active && (cave_info[ry][rx] & CAVE_G_VAULT) && is_wall)
        return false;

    /* Avoid carving new routes inside the sealed Morgoth region: only traverse
     * existing vault/tunnel squares there (and don't cross permanent walls). */
    if (coord_in_morgoth_region(ry, rx, 0)
        && !(cave_info[ry][rx] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
        && is_wall)
    {
        return false;
    }

    return true;
}

int connectivity_unreachable_component(
    int start_y, int start_x,
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID])
{
    static const int ddy8[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int ddx8[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int head = 0;
    int tail = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            component[y][x] = 0;
        }
    }

    if (!in_bounds_fully(start_y, start_x))
        return 0;
    if (cave_access[start_y][start_x])
        return 0;
    if (!player_passable(start_y, start_x, true))
        return 0;

    component[start_y][start_x] = 1;
    component_cells[tail++] = start_y * MAX_DUNGEON_WID + start_x;

    while (head < tail)
    {
        int cur = component_cells[head++];
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        for (int d = 0; d < 8; ++d)
        {
            int ny = cy + ddy8[d];
            int nx = cx + ddx8[d];
            int nidx;

            if (!in_bounds_fully(ny, nx))
                continue;
            if (component[ny][nx])
                continue;
            if (cave_access[ny][nx])
                continue;
            if (!player_passable(ny, nx, true))
                continue;

            component[ny][nx] = 1;
            nidx = ny * MAX_DUNGEON_WID + nx;
            if (tail < MAX_DUNGEON_HGT * MAX_DUNGEON_WID)
                component_cells[tail++] = nidx;
        }
    }

    return tail;
}

bool connectivity_component_boundary_cell(
    int y, int x,
    byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};

    for (int d = 0; d < 4; ++d)
    {
        int ny = y + ddy4[d];
        int nx = x + ddx4[d];

        if (!in_bounds_fully(ny, nx))
            continue;
        if (component[ny][nx])
            continue;
        if (!connectivity_rescue_traversable(ny, nx))
            continue;

        return true;
    }

    return false;
}

bool connectivity_rescue_component(
    byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID],
    int component_count,
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int *out_source_y, int *out_source_x,
    int *out_target_y, int *out_target_x,
    int *out_carve_count, int *out_boundary_sources)
{
    static int prev[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};
    int head = 0;
    int tail = 0;
    int found_y = -1;
    int found_x = -1;
    int source_y = -1;
    int source_x = -1;
    int carve_count = 0;
    int boundary_sources = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            prev[y][x] = -1;
        }
    }

    for (int i = 0; i < component_count; ++i)
    {
        int idx = component_cells[i];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;
        prev[cy][cx] = -2;
    }

    for (int i = 0; i < component_count; ++i)
    {
        int idx = component_cells[i];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;

        if (cave_feat[cy][cx] == FEAT_CHASM)
            continue;
        if (!connectivity_component_boundary_cell(cy, cx, component))
            continue;

        prev[cy][cx] = idx;
        if (tail < (int)N_ELEMENTS(queue))
            queue[tail++] = idx;
        boundary_sources++;
    }

    if (out_boundary_sources)
        *out_boundary_sources = boundary_sources;

    if (boundary_sources == 0)
        return false;

    while (head < tail)
    {
        int cur = queue[head++];
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        for (int d = 0; d < 4; ++d)
        {
            int ny = cy + ddy4[d];
            int nx = cx + ddx4[d];
            int nidx;

            if (!in_bounds_fully(ny, nx))
                continue;
            if (prev[ny][nx] != -1)
                continue;
            if (!connectivity_rescue_traversable(ny, nx))
                continue;

            nidx = ny * MAX_DUNGEON_WID + nx;
            prev[ny][nx] = cur;
            if (tail < (int)N_ELEMENTS(queue))
                queue[tail++] = nidx;

            if (cave_access[ny][nx]
                && player_passable(ny, nx, true)
                && !coord_in_morgoth_region(ny, nx, 1))
            {
                found_y = ny;
                found_x = nx;
                head = tail;
                break;
            }
        }
    }

    if (found_y < 0 || found_x < 0)
        return false;

    {
        int cur = found_y * MAX_DUNGEON_WID + found_x;
        int safety = 0;

        while (safety++ < (int)N_ELEMENTS(queue))
        {
            int cy = cur / MAX_DUNGEON_WID;
            int cx = cur % MAX_DUNGEON_WID;

            if (cave_feat[cy][cx] != FEAT_WALL_PERM)
            {
                bool in_morgoth = coord_in_morgoth_region(cy, cx, 0);
                bool allow_morgoth = (cave_info[cy][cx] & CAVE_MORGOTH_TUNNEL) != 0;

                if (!in_morgoth || allow_morgoth)
                {
                    if (!cave_floor_bold(cy, cx)
                        && (cave_feat[cy][cx] < FEAT_DOOR_HEAD
                            || cave_feat[cy][cx] > FEAT_DOOR_TAIL))
                    {
                        cave_set_feat(cy, cx, FEAT_FLOOR);
                        carve_count++;
                    }

                    if (!(cave_info[cy][cx] & CAVE_ROOM))
                        mark_generation_escape_tunnel(cy, cx);
                }
            }

            if (prev[cy][cx] == cur)
            {
                source_y = cy;
                source_x = cx;
                break;
            }

            if (prev[cy][cx] < 0)
                break;
            cur = prev[cy][cx];
        }
    }

    if (out_source_y)
        *out_source_y = source_y;
    if (out_source_x)
        *out_source_x = source_x;
    if (out_target_y)
        *out_target_y = found_y;
    if (out_target_x)
        *out_target_x = found_x;
    if (out_carve_count)
        *out_carve_count = carve_count;

    return (source_y >= 0 && source_x >= 0);
}

/*
 *  Make sure that the level is sufficiently connected.
 */

bool check_connectivity(void)
{
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int y, x;

    // Reset the array used for checking connectivity
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    /* Log which room centers are unreachable before rescue attempts */
    flood_access(p_ptr->py, p_ptr->px, cave_access, true);
    int unreachable_rooms = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        int ry = dun->cent[i].y;
        int rx = dun->cent[i].x;
        if (in_bounds_fully(ry, rx) && !cave_access[ry][rx])
        {
            unreachable_rooms++;
            genlog_connect("UNREACHABLE ROOM #%d at (%d,%d) bounds=(%d,%d)-(%d,%d)",
                           i, ry, rx,
                           dun->corner[i].y1, dun->corner[i].x1,
                           dun->corner[i].y2, dun->corner[i].x2);
        }
    }
    if (unreachable_rooms > 0)
    {
        genlog_fail("PRE-RESCUE: %d/%d rooms unreachable from player at (%d,%d)",
                    unreachable_rooms, dun->cent_n, p_ptr->py, p_ptr->px);
    }

    /* Reset for rescue loop */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    /* Attempt connectivity with iterative rescue tunnels for each disconnected component */
    int rescue_attempts = 0;
    while (true)
    {
        // Make sure entire dungeon is connected (ignoring rubble and chasms)
        flood_access(p_ptr->py, p_ptr->px, cave_access, true);
        int unreachable = 0;
        int sample_y = -1, sample_x = -1;
        for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
            for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
                if (player_passable(y, x, true) && (cave_access[y][x] == false))
                {
                    unreachable++;
                    if (sample_y < 0)
                    {
                        sample_y = y;
                        sample_x = x;
                    }
                }

        if (unreachable == 0)
            break;

        /* Prefer sampling an unreachable room center to connect large components early. */
        if (dun)
        {
            for (int i = 0; i < dun->cent_n; ++i)
            {
                int ry = dun->cent[i].y;
                int rx = dun->cent[i].x;
                if (!in_bounds_fully(ry, rx)) continue;
                if (cave_access[ry][rx]) continue;
                if (!player_passable(ry, rx, true)) continue;
                sample_y = ry;
                sample_x = rx;
                break;
            }
        }

        /* Stop if we've tried too many rescues - scale with level size */
        /* Larger levels need more rescue attempts: base 20 + (blocks-8)*4 (and at least ~half room count). */
        int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
        int max_rescues = 20 + MAX(0, (blocks - 8) * 4);  /* 20 for 8 blocks, 72 for 21 blocks */
        if (dun) max_rescues = MAX(max_rescues, 20 + (dun->cent_n / 2));
        if (rescue_attempts++ >= max_rescues)
        {
            log_trace("check_connectivity: %d unreachable passable grids after %d rescues (first at %d,%d) -- FAILING",
                      unreachable, rescue_attempts, sample_y, sample_x);
            genlog_fail("CONNECTIVITY FAILED: %d unreachable passable grids after %d rescues (max=%d), first at (%d,%d)",
                        unreachable, rescue_attempts, max_rescues, sample_y, sample_x);
            return false;
        }

        {
            int component_count;
            int source_y = -1, source_x = -1;
            int found_y = -1, found_x = -1;
            int carve_count = 0;
            int boundary_sources = 0;

            component_count = connectivity_unreachable_component(
                sample_y, sample_x, cave_access, component, component_cells);

            if (component_count <= 0)
            {
                log_trace("check_connectivity: failed to flood unreachable component from (%d,%d)", sample_y, sample_x);
                genlog_fail("CONNECTIVITY FAILED: could not flood unreachable component from (%d,%d)",
                    sample_y, sample_x);
                return false;
            }

            if (!connectivity_rescue_component(
                    component, component_cells, component_count, cave_access,
                    &source_y, &source_x, &found_y, &found_x,
                    &carve_count, &boundary_sources))
            {
                log_trace("check_connectivity: BFS rescue could not find a reachable target from component at (%d,%d) size=%d boundary=%d",
                    sample_y, sample_x, component_count, boundary_sources);
                genlog_fail("CONNECTIVITY FAILED: BFS rescue could not find reachable target from (%d,%d)",
                    sample_y, sample_x);
                return false;
            }

            log_trace("check_connectivity: component rescue from (%d,%d) boundary=(%d,%d) to reachable (%d,%d), component=%d boundary=%d carved=%d (unreachable=%d, attempt=%d)",
                sample_y, sample_x, source_y, source_x, found_y, found_x,
                component_count, boundary_sources, carve_count, unreachable,
                rescue_attempts);
            genlog_connect("RESCUE TUNNEL: component=%d boundary=%d from (%d,%d) to (%d,%d), carved=%d",
                component_count, boundary_sources, source_y, source_x, found_y,
                found_x, carve_count);
        }

        /* Clear and loop to re-check connectivity */
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
            for (x = 0; x < p_ptr->cur_map_wid; x++)
                cave_access[y][x] = false;
    }

    // Reset the array used for checking connectivity
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    if (p_ptr->depth >= MORGOTH_DEPTH)
    {
        return (true);
    }

    if (p_ptr->create_stair == FEAT_MORE
        || p_ptr->create_stair == FEAT_MORE_SHAFT)
    {
        return (true);
    }

    // Make sure player can reach down stairs without going through rubble and
    // chasms
    flood_access(p_ptr->py, p_ptr->px, cave_access, false);
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (((cave_feat[y][x] == FEAT_MORE) && (cave_access[y][x] == true))
                || ((cave_feat[y][x] == FEAT_MORE_SHAFT)
                    && (cave_access[y][x] == true)))
            {
                return (true);
            }
        }

    genlog_fail("CONNECTIVITY FAILED: player cannot reach down stairs without rubble/chasms");
    return (false);
}

/*
 *  Check if there are two adjacent doors on the level.
 */
bool doubled_doors(void)
{
    int y, x;

    // Check each grid within boundary
    for (y = 0; y < p_ptr->cur_map_hgt - 1; y++)
        for (x = 0; x < p_ptr->cur_map_wid - 1; x++)
            if (cave_known_closed_door_bold(y, x))
            {
                if (cave_known_closed_door_bold(y + 1, x))
                    return (true);
                if (cave_known_closed_door_bold(y, x + 1))
                    return (true);
            }

    return (false);
}

bool connect_rooms_stairs(void)
{
    int i;
    int corridor_attempts;
    int r1, r2, r_closest, d_closest, d;
    int pieces = 0;
    int stairs = 0;
    int initial_up = FEAT_LESS;
    int initial_down = FEAT_MORE;

    bool joined;
    bool no_down_stairs = (p_ptr->depth >= MORGOTH_DEPTH);
    bool niena_level = (quest_lottery_winner == QUEST_ID_NIENA);

    /* Add backbone links across partition neighbors */
    connect_partition_hubs();

    // Phase 1:
    // connect each room to the closest room (if not already connected)
    // Try normal mode first, then desperate mode if that fails

    for (r1 = 0; r1 < dun->cent_n; r1++)
    {
        /* find closest room */
        r_closest = 0; /* default values that will get beaten trivially */
        d_closest = 1000;
        for (r2 = 0; r2 < dun->cent_n; r2++)
        {
            if (r2 != r1)
            {
                d = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y,
                    dun->cent[r2].x);
                if (d < d_closest)
                {
                    d_closest = d;
                    r_closest = r2;
                }
            }
        }

        /* connect the rooms, if not already connected */
        if (!(dun->connection[r1][r_closest]))
        {
            /* Try normal mode first, then desperate mode */
            if (!connect_two_rooms(r1, r_closest, true, false))
            {
                (void)connect_two_rooms(r1, r_closest, true, true);
            }
        }
    }

    // Phase 1.5: Connect to second-closest room as well for redundancy
    for (r1 = 0; r1 < dun->cent_n; r1++)
    {
        int closest1 = -1, closest2 = -1;
        int dist1 = 99999, dist2 = 99999;

        for (r2 = 0; r2 < dun->cent_n; r2++)
        {
            if (r2 == r1) continue;
            d = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y, dun->cent[r2].x);
            if (d < dist1)
            {
                dist2 = dist1; closest2 = closest1;
                dist1 = d; closest1 = r2;
            }
            else if (d < dist2)
            {
                dist2 = d; closest2 = r2;
            }
        }

        /* Try to connect to second-closest if not already connected */
        if (closest2 >= 0 && !(dun->connection[r1][closest2]))
        {
            (void)connect_two_rooms(r1, closest2, true, false);
        }
    }

    // Phase 2:
    // make some random connections between rooms so long as they don't
    // intersect things

    switch (p_ptr->cur_map_hgt / PANEL_HGT)
    {
    case 3:
        corridor_attempts = dun->cent_n * dun->cent_n;
        break;
    case 4:
        corridor_attempts = dun->cent_n * dun->cent_n * 2;
        break;
    case 5:
    default:
        corridor_attempts = dun->cent_n * dun->cent_n * 10;
        break;
    }

    for (i = 0; i < corridor_attempts; i++)
    {
        r1 = rand_int(dun->cent_n);
        r2 = rand_int(dun->cent_n);
        if ((r1 != r2) && !(dun->connection[r1][r2]))
        {
            (void)connect_two_rooms(r1, r2, true, false);
        }
    }

    // add some T-intersections in the corridors
    for (i = 0; i < corridor_attempts; i++)
    {
        r1 = rand_int(dun->cent_n);
        (void)connect_room_to_corridor(r1);
    }

    // Phase 3:
    // cut the dungeon up into connected pieces and try hard to make corridors
    // that connect them

    pieces = dungeon_pieces();
    while (pieces > 1)
    {
        joined = false;

        for (r1 = 0; r1 < dun->cent_n; r1++)
        {
            for (r2 = 0; r2 < dun->cent_n; r2++)
            {
                if (!joined && (dun->piece[r1] != dun->piece[r2]))
                {
                    for (i = 0; i < 10; i++)
                    {
                        if (!(dun->connection[r1][r2]))
                        {
                            joined = connect_two_rooms(r1, r2, true, true);
                        }
                    }
                }
            }
        }

        if (!joined)
            break;

        // cut the dungeon up into connected pieces and stop if there is only
        // one
        pieces = dungeon_pieces();
    }

    /* Phase 3.5: L-shaped corridor fallback before force-connect.
     * Try carving clean L-shaped corridors between disconnected pieces.
     * This produces better-looking results than diagonal Bresenham carving. */
    if (pieces > 1)
    {
        int l_connects = 0;
        for (int attempt = 0; attempt < 100 && pieces > 1; ++attempt)
        {
            /* Find the nearest pair of rooms from different pieces */
            int best_a = -1, best_b = -1;
            int best_dist = 999999;

            for (int ra = 0; ra < dun->cent_n; ++ra)
            {
                for (int rb = ra + 1; rb < dun->cent_n; ++rb)
                {
                    if (dun->piece[ra] == dun->piece[rb])
                        continue;
                    if (dun->connection[ra][rb])
                        continue;

                    int dist = distance(dun->cent[ra].y, dun->cent[ra].x,
                                        dun->cent[rb].y, dun->cent[rb].x);
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_a = ra;
                        best_b = rb;
                    }
                }
            }

            if (best_a < 0 || best_b < 0)
                break;

            int y0 = dun->cent[best_a].y, x0 = dun->cent[best_a].x;
            int y1 = dun->cent[best_b].y, x1 = dun->cent[best_b].x;

            /* Try L-shaped corridor (horizontal then vertical, or vice versa) */
            bool carved = false;
            for (int dir = 0; dir < 2 && !carved; ++dir)
            {
                bool valid = true;

                /* Check if the L-path is carveable (no permanent walls) */
                int min_x = MIN(x0, x1), max_x = MAX(x0, x1);
                int min_y = MIN(y0, y1), max_y = MAX(y0, y1);

                /* Check horizontal leg */
                int leg_y = (dir == 0) ? y0 : y1;
                for (int tx = min_x; tx <= max_x && valid; ++tx)
                {
                    if (!in_bounds_fully(leg_y, tx) || cave_feat[leg_y][tx] == FEAT_WALL_PERM)
                        valid = false;
                    if (coord_in_morgoth_region(leg_y, tx, 1))
                        valid = false;
                }

                /* Check vertical leg */
                int leg_x = (dir == 0) ? x1 : x0;
                for (int ty = min_y; ty <= max_y && valid; ++ty)
                {
                    if (!in_bounds_fully(ty, leg_x) || cave_feat[ty][leg_x] == FEAT_WALL_PERM)
                        valid = false;
                    if (coord_in_morgoth_region(ty, leg_x, 1))
                        valid = false;
                }

                if (valid)
                {
                    /* Carve horizontal leg */
                    for (int tx = min_x; tx <= max_x; ++tx)
                    {
                        if (coord_in_morgoth_region(leg_y, tx, 1))
                        {
                            valid = false;
                            break;
                        }
                        if (!cave_floor_bold(leg_y, tx))
                            cave_set_feat(leg_y, tx, FEAT_FLOOR);
                    }
                    if (!valid) continue;
                    /* Carve vertical leg */
                    for (int ty = min_y; ty <= max_y; ++ty)
                    {
                        if (coord_in_morgoth_region(ty, leg_x, 1))
                        {
                            valid = false;
                            break;
                        }
                        if (!cave_floor_bold(ty, leg_x))
                            cave_set_feat(ty, leg_x, FEAT_FLOOR);
                    }
                    if (!valid) continue;

                    dun->connection[best_a][best_b] = true;
                    dun->connection[best_b][best_a] = true;
                    carved = true;
                    l_connects++;
                }
            }

            pieces = dungeon_pieces();
        }

        if (l_connects > 0)
            log_trace("connect_rooms_stairs: L-shaped fallback carved %d connections, pieces now %d", l_connects, pieces);
    }

    /* Last resort: forcibly connect distinct pieces by digging a straight corridor
     * ignoring tunnel safety checks (but respecting permanent walls). This handles
     * adjacent-but-unconnected rooms/vaults seen on dense maps.
     * IMPROVED: Instead of picking random pairs, find the NEAREST pair of rooms
     * from different pieces to minimize ugly cross-map tunnels. */
    if (pieces > 1)
    {
        for (int attempt = 0; attempt < 50 && pieces > 1; ++attempt)
        {
            /* Find the nearest pair of rooms from different pieces */
            int best_a = -1, best_b = -1;
            int best_dist = 999999;

            for (int ra = 0; ra < dun->cent_n; ++ra)
            {
                for (int rb = ra + 1; rb < dun->cent_n; ++rb)
                {
                    if (dun->piece[ra] == dun->piece[rb])
                        continue;

                    int dist = distance(dun->cent[ra].y, dun->cent[ra].x,
                                        dun->cent[rb].y, dun->cent[rb].x);
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_a = ra;
                        best_b = rb;
                    }
                }
            }

            if (best_a < 0 || best_b < 0)
                break;  /* No valid pair found */

            int a = best_a;
            int b = best_b;

            int y0 = dun->cent[a].y, x0 = dun->cent[a].x;
            int y1 = dun->cent[b].y, x1 = dun->cent[b].x;

            log_trace("force-connect: linking room %d (piece %d) to room %d (piece %d), dist=%d",
                      a, dun->piece[a], b, dun->piece[b], best_dist);

            /* Bresenham carve that ignores h/v tunnel constraints */
            int dy = ABS(y1 - y0), sx = (x0 < x1) ? 1 : -1;
            int dx = ABS(x1 - x0), sy = (y0 < y1) ? 1 : -1;
            int err = (dx > dy ? dx : -dy) / 2;
            int y = y0, x = x0;
            bool aborted = false;
            while (true)
            {
                if (coord_in_morgoth_region(y, x, 1))
                {
                    aborted = true;
                    break;
                }
                if (in_bounds_fully(y, x) && cave_feat[y][x] != FEAT_WALL_PERM)
                {
                    if (!cave_floor_bold(y, x))
                        cave_set_feat(y, x, FEAT_FLOOR);
                }
                if (y == y1 && x == x1) break;
                int e2 = err;
                if (e2 > -dx) { err -= dy; x += sx; }
                if (e2 < dy)  { err += dx; y += sy; }
            }

            if (!aborted)
            {
                dun->connection[a][b] = dun->connection[b][a] = true;
                pieces = dungeon_pieces();
            }
        }

        log_trace("connect_rooms_stairs: forced-connect phase reduced pieces to %d", pieces);
    }

    // label_rooms();

    /* Calculate number of stairs based on map size: 2 for 66x66, 8 for 165x165 */
    /* Linear interpolation: stairs = 2 + (size - 66) * (8 - 2) / (165 - 66) */
    int map_size = (p_ptr->cur_map_hgt + p_ptr->cur_map_wid) / 2;  /* Average dimension */
    int stairs_max_base = 8;
    int stairs_max_total = 12;
    if (more_stairs)
    {
        stairs_max_base *= 2;
        stairs_max_total *= 2;
    }
    stairs = 2 + ((map_size - 66) * 6) / 99;  /* 6 = (8-2), 99 = (165-66) */
    if (stairs < 2) stairs = 2;   /* Minimum 2 */
    if (stairs > stairs_max_base) stairs = stairs_max_base;  /* Maximum 8 (or doubled) */

    /* Labyrinth bonus: +1 stair per labyrinth partition (more escape routes in mazes) */
    if (current_labyrinth_partitions > 0)
    {
        int stair_bonus = current_labyrinth_partitions;
        stairs += stair_bonus;
        log_trace("Labyrinth stair bonus: +%d stairs from %d labyrinth partitions (total=%d)",
                  stair_bonus, current_labyrinth_partitions, stairs);
    }

    if (more_stairs)
    {
        stairs += (stairs + 1) / 2; /* +50% (rounded up) */
    }
    if (stairs > stairs_max_total) stairs = stairs_max_total;

    log_trace("Map size %d leads to %d stairs each direction", map_size, stairs);
    if (niena_level && !no_down_stairs)
    {
        log_trace("Nienna level: limiting down stairs to a single target stair for the mercy quest");
    }

    /* Determine partition count for guaranteed stair placement */
    int partition_count = (map_size <= 80) ? 2 : 3;  /* Reduced from 4/9 to match lower stair count */
    int grid_rows = 1;
    int grid_cols = partition_count;
    if (partition_count == 4)
    {
        grid_rows = 2;
        grid_cols = 2;
    }
    else if (partition_count == 9)
    {
        grid_rows = 3;
        grid_cols = 3;
    }

    /* Place guaranteed stairs: at least one up and one down per partition */
    int down_placed = 0;
    int up_placed = 0;

    /* First pass: place one of each type per partition */
    for (int pi = 0; pi < partition_count; ++pi)
    {
        int row = pi / grid_cols;
        int col = pi % grid_cols;

        int y1 = 1 + (row * p_ptr->cur_map_hgt / grid_rows);
        int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows) - 1;
        int x1 = 1 + (col * p_ptr->cur_map_wid / grid_cols);
        int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols) - 1;

        /* Clamp boundaries */
        if (y2 >= p_ptr->cur_map_hgt - 1) y2 = p_ptr->cur_map_hgt - 2;
        if (x2 >= p_ptr->cur_map_wid - 1) x2 = p_ptr->cur_map_wid - 2;

        /* Place one down stair in this partition (unless final level) */
        if (!no_down_stairs && !niena_level)
        {
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                int yy = rand_range(y1, y2);
                int xx = rand_range(x1, x2);

                if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                    cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                    cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                    cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                    cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
                {
                    int feat = (p_ptr->on_the_run) ? FEAT_MORE_SHAFT :
                              (down_placed == 0 || p_ptr->depth >= MORGOTH_DEPTH) ? FEAT_MORE :
                              choose_down_stairs();
                    cave_set_feat(yy, xx, feat);
                    down_placed++;
                    break;
                }
            }
        }

        /* Place one up stair in this partition */
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            int yy = rand_range(y1, y2);
            int xx = rand_range(x1, x2);

            if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
            {
                int feat = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT :
                          (up_placed == 0 || !p_ptr->depth) ? FEAT_LESS :
                          choose_up_stairs();
                cave_set_feat(yy, xx, feat);
                up_placed++;
                break;
            }
        }
    }

    log_trace("Guaranteed partition stairs: %d down, %d up placed", down_placed, up_placed);

    /* Second pass: place remaining stairs randomly across the map */
    int down_remaining = 0;
    if (!no_down_stairs)
    {
        down_remaining = niena_level ? 1 : (stairs - down_placed);
    }
    int up_remaining = stairs - up_placed;

    /* Place remaining down stairs */
    int down_stairs = down_remaining;
    if (p_ptr->on_the_run)
        down_stairs *= 2;
    if ((p_ptr->create_stair == FEAT_MORE) || (p_ptr->create_stair == FEAT_MORE_SHAFT))
        down_stairs--;

    initial_down = p_ptr->on_the_run ? FEAT_MORE_SHAFT : FEAT_MORE;

    if (no_down_stairs)
        down_stairs = 0;

    if (down_stairs > 0 && !(alloc_stairs(initial_down, down_stairs)))
    {
        if (cheat_room)
            msg_format("Failed to place remaining down stairs.");
        log_trace("connect_rooms_stairs failed: Could not place %d remaining down stairs", down_stairs);
        return (false);
    }

    /* Place remaining up stairs */
    int up_stairs = up_remaining;
    if (p_ptr->on_the_run && p_ptr->depth >= 2)
        up_stairs *= 2;
    if ((p_ptr->create_stair == FEAT_LESS) || (p_ptr->create_stair == FEAT_LESS_SHAFT))
        up_stairs--;

    initial_up = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT : FEAT_LESS;

    if (up_stairs > 0 && !(alloc_stairs(initial_up, up_stairs)))
    {
        if (cheat_room)
            msg_format("Failed to place remaining up stairs.");
        log_trace("connect_rooms_stairs failed: Could not place %d remaining up stairs", up_stairs);
        return (false);
    }

    log_trace("Total stairs placed: %d down, %d up", down_placed + down_stairs, up_placed + up_stairs);

    /* Hack -- Add some quartz streamers */
    for (i = 0; i < DUN_STR_QUA; i++)
    {
        /*if we can't build streamers, something is wrong with level*/
        if (!build_streamer(FEAT_QUARTZ))
        {
            log_trace("connect_rooms_stairs failed: Could not build quartz streamer %d", i);
            return (false);
        }
    }

    /* Do not mix the legacy random-chasm pass with partition chasm rooms. The
     * two systems use different styling/connectivity rules and produce broken
     * visuals/access when overlaid. */
    if (!level_has_chasm_partition())
    {
        build_chasms();
    }
    else
    {
        log_trace("connect_rooms_stairs: skipping legacy build_chasms() because the partition generator already placed chasm terrain");
    }

    return (true);
}
