/* File: level-generation-layout-connectivity.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

#if 0
void ensure_partition_connectivity(void)
{
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
    int grid_rows = current_partition_rows;
    int grid_cols = current_partition_cols;

    /* Reuse the grid chosen during generation; fall back if unavailable */
    if (grid_rows <= 0 || grid_cols <= 0) {
        fallback_partition_grid_from_blocks(blocks, &grid_rows, &grid_cols);
    }

    int connections_added = 0;
    const int SEARCH_DEPTH = 15;  /* How far into partition to look for floor (was 5) */
    const int CORRIDOR_LEN = 8;   /* How long the carved corridor is (was 3) */
    const int ATTEMPTS_PER_SEGMENT = 3;  /* Try multiple positions per boundary segment */

    genlog_connect("ensure_partition_connectivity: %dx%d grid, searching %d deep, carving %d long",
                   grid_rows, grid_cols, SEARCH_DEPTH, CORRIDOR_LEN);

    /* Create horizontal boundary connections (between rows) */
    for (int row = 0; row < grid_rows - 1; ++row)
    {
        int boundary_y = ((row + 1) * p_ptr->cur_map_hgt / grid_rows);

        for (int col = 0; col < grid_cols; ++col)
        {
            int x1 = (col * p_ptr->cur_map_wid / grid_cols) + 2;
            int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols) - 2;

            /* Try multiple x positions for better coverage */
            for (int attempt = 0; attempt < ATTEMPTS_PER_SEGMENT; ++attempt)
            {
                int cx = rand_range(x1 + 2, x2 - 2);

                /* Find nearest floor above and below the boundary */
                int floor_above_y = -1, floor_above_x = -1;
                int floor_below_y = -1, floor_below_x = -1;

                for (int dx = -5; dx <= 5; ++dx)
                {
                    int tx = cx + dx;
                    if (tx < 1 || tx >= p_ptr->cur_map_wid - 1) continue;

                    for (int dy = 1; dy <= SEARCH_DEPTH; ++dy)
                    {
                        if (floor_above_y < 0 && in_bounds_fully(boundary_y - dy, tx)
                            && cave_floor_bold(boundary_y - dy, tx))
                        {
                            floor_above_y = boundary_y - dy;
                            floor_above_x = tx;
                        }
                        if (floor_below_y < 0 && in_bounds_fully(boundary_y + dy, tx)
                            && cave_floor_bold(boundary_y + dy, tx))
                        {
                            floor_below_y = boundary_y + dy;
                            floor_below_x = tx;
                        }
                    }
                }

                /* If both partitions have floor nearby, check if connection needed */
                if (floor_above_y >= 0 && floor_below_y >= 0)
                {
                    /* Check if boundary is already connected */
                    bool boundary_connected = false;
                    for (int dx = -3; dx <= 3; ++dx)
                    {
                        int tx = cx + dx;
                        for (int dy = -2; dy <= 2; ++dy)
                        {
                            if (in_bounds_fully(boundary_y + dy, tx) && cave_floor_bold(boundary_y + dy, tx))
                            {
                                boundary_connected = true;
                                break;
                            }
                        }
                        if (boundary_connected) break;
                    }

                    if (!boundary_connected)
                    {
                        /* Carve from floor_above to floor_below through the boundary */
                        int mid_x = (floor_above_x + floor_below_x) / 2;

                        /* Carve vertical corridor centered on boundary */
                        for (int dy = -CORRIDOR_LEN; dy <= CORRIDOR_LEN; ++dy)
                        {
                            int ty = boundary_y + dy;
                            if (in_bounds_fully(ty, mid_x) &&
                                (cave_feat[ty][mid_x] == FEAT_WALL_EXTRA || cave_feat[ty][mid_x] == FEAT_WALL_OUTER))
                            {
                                cave_set_feat(ty, mid_x, FEAT_FLOOR);
                            }
                        }
                        connections_added++;
                        genlog_connect("H-boundary row=%d col=%d: carved at x=%d from y=%d to y=%d",
                                       row, col, mid_x, boundary_y - CORRIDOR_LEN, boundary_y + CORRIDOR_LEN);
                        break;  /* Only one connection per segment needed */
                    }
                }
            }
        }
    }

    /* Create vertical boundary connections (between columns) */
    for (int col = 0; col < grid_cols - 1; ++col)
    {
        int boundary_x = ((col + 1) * p_ptr->cur_map_wid / grid_cols);

        for (int row = 0; row < grid_rows; ++row)
        {
            int y1 = (row * p_ptr->cur_map_hgt / grid_rows) + 2;
            int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows) - 2;

            for (int attempt = 0; attempt < ATTEMPTS_PER_SEGMENT; ++attempt)
            {
                int cy = rand_range(y1 + 2, y2 - 2);

                int floor_left_y = -1, floor_left_x = -1;
                int floor_right_y = -1, floor_right_x = -1;

                for (int dy = -5; dy <= 5; ++dy)
                {
                    int ty = cy + dy;
                    if (ty < 1 || ty >= p_ptr->cur_map_hgt - 1) continue;

                    for (int dx = 1; dx <= SEARCH_DEPTH; ++dx)
                    {
                        if (floor_left_x < 0 && in_bounds_fully(ty, boundary_x - dx)
                            && cave_floor_bold(ty, boundary_x - dx))
                        {
                            floor_left_y = ty;
                            floor_left_x = boundary_x - dx;
                        }
                        if (floor_right_x < 0 && in_bounds_fully(ty, boundary_x + dx)
                            && cave_floor_bold(ty, boundary_x + dx))
                        {
                            floor_right_y = ty;
                            floor_right_x = boundary_x + dx;
                        }
                    }
                }

                if (floor_left_x >= 0 && floor_right_x >= 0)
                {
                    bool boundary_connected = false;
                    for (int dy = -3; dy <= 3; ++dy)
                    {
                        int ty = cy + dy;
                        for (int dx = -2; dx <= 2; ++dx)
                        {
                            if (in_bounds_fully(ty, boundary_x + dx) && cave_floor_bold(ty, boundary_x + dx))
                            {
                                boundary_connected = true;
                                break;
                            }
                        }
                        if (boundary_connected) break;
                    }

                    if (!boundary_connected)
                    {
                        int mid_y = (floor_left_y + floor_right_y) / 2;

                        for (int dx = -CORRIDOR_LEN; dx <= CORRIDOR_LEN; ++dx)
                        {
                            int tx = boundary_x + dx;
                            if (in_bounds_fully(mid_y, tx) &&
                                (cave_feat[mid_y][tx] == FEAT_WALL_EXTRA || cave_feat[mid_y][tx] == FEAT_WALL_OUTER))
                            {
                                cave_set_feat(mid_y, tx, FEAT_FLOOR);
                            }
                        }
                        connections_added++;
                        genlog_connect("V-boundary row=%d col=%d: carved at y=%d from x=%d to x=%d",
                                       row, col, mid_y, boundary_x - CORRIDOR_LEN, boundary_x + CORRIDOR_LEN);
                        break;
                    }
                }
            }
        }
    }

    if (connections_added > 0)
    {
        log_trace("Partition connectivity: added %d boundary connections", connections_added);
        genlog_connect("Partition connectivity: added %d boundary connections total", connections_added);
    }
    else
    {
        genlog_connect("Partition connectivity: no new connections needed");
    }
}
#endif


int partition_index_from_point(int y, int x, int rows, int cols)
{
    if (rows <= 0 || cols <= 0) return -1;
    if (p_ptr->cur_map_hgt <= 0 || p_ptr->cur_map_wid <= 0) return -1;
    int row = (y * rows) / p_ptr->cur_map_hgt;
    int col = (x * cols) / p_ptr->cur_map_wid;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= rows) row = rows - 1;
    if (col >= cols) col = cols - 1;
    return row * cols + col;
}

int room_partition_index(int room_idx)
{
    if (room_idx < 0 || room_idx >= dun->cent_n)
        return -1;
    if (current_partition_rows <= 0 || current_partition_cols <= 0
        || current_partition_count <= 0)
    {
        return -1;
    }

    return partition_index_from_point(dun->cent[room_idx].y, dun->cent[room_idx].x,
        current_partition_rows, current_partition_cols);
}

bool tunnel_should_mark_escape(int r1, int r2)
{
    int p1 = room_partition_index(r1);
    int p2 = room_partition_index(r2);
    bool big1 = (p1 >= 0 && p1 < current_partition_count && p1 < 25
        && is_big_partition_mode(current_partition_modes[p1]));
    bool big2 = (p2 >= 0 && p2 < current_partition_count && p2 < 25
        && is_big_partition_mode(current_partition_modes[p2]));

    if (!big1 && !big2)
        return false;

    if (p1 >= 0 && p2 >= 0 && p1 == p2)
        return false;

    return true;
}

int room_connection_degree(int room_idx)
{
    if (room_idx < 0 || room_idx >= dun->cent_n)
        return 0;
    int deg = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        if (dun->connection[room_idx][i])
            deg++;
    }
    return deg;
}

bool connect_rooms_with_logging(int r1, int r2, const char *tag, bool allow_desperate)
{
    if (r1 < 0 || r2 < 0 || r1 == r2)
        return false;

    if (dun->connection[r1][r2])
        return true;

    bool ok = connect_two_rooms(r1, r2, true, false);
    if (!ok && allow_desperate)
        ok = connect_two_rooms(r1, r2, true, true);

    if (ok && tag)
    {
        int dist = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y, dun->cent[r2].x);
        genlog_connect("%s: linked room %d -> %d (dist=%d)", tag, r1, r2, dist);
    }
    return ok;
}

bool is_big_partition_mode(quadrant_mode_t mode)
{
    return (mode == QUAD_MODE_LABYRINTH || mode == QUAD_MODE_BIG_CAVE || mode == QUAD_MODE_CHASM);
}

bool big_partition_boundary_floor_ok(quadrant_mode_t mode, int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (!cave_floor_bold(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_CHASM)
        return false;
    if (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT))
        return false;
    if (mode == QUAD_MODE_CHASM)
        return ((cave_info[y][x] & (CAVE_ROOM | CAVE_CHASM_AREA))
            == (CAVE_ROOM | CAVE_CHASM_AREA));
    return ((cave_info[y][x] & CAVE_ROOM) != 0);
}

int partition_bridge_style_for_index(int pi)
{
    if (pi < 0 || pi >= current_partition_count || pi >= 25)
        return -1;
    return current_partition_bridge_styles[pi];
}

/* Shared-boundary fallback connector for adjacent partitions.
 * Standard tunnel rules often reject some otherwise-valid joins, so when two
 * partitions have native walkable floor near the same shared boundary we carve
 * a straight doorway/corridor between those two populated sides. */
bool carve_straight_big_partition_connector(
    int y1, int x1, int y2, int x2, int r1, int r2, int rows, int cols)
{
    int dy = (y2 > y1) ? 1 : (y2 < y1) ? -1 : 0;
    int dx = (x2 > x1) ? 1 : (x2 < x1) ? -1 : 0;
    bool floor_thresholds = tunnel_prefers_floor_thresholds(r1, r2);

    /* Must be a straight segment. */
    if (!((dy == 0) ^ (dx == 0)))
        return false;

    if (morgoth_segment_blocked(y1, x1, y2, x2, 2))
        return false;

    bool carved = false;
    int y = y1;
    int x = x1;

    for (;;)
    {
        if (!in_bounds_fully(y, x))
            return false;

        if (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT))
            return false;

        int feat = cave_feat[y][x];
        if (feat == FEAT_WALL_PERM)
            return false;

        if (feat == FEAT_WALL_OUTER)
        {
            if (floor_thresholds)
            {
                carve_floor_threshold(y, x, r1, r2, false);
            }
            else
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD);
            }
            carved = true;
        }
        else if (feat == FEAT_WALL_EXTRA || feat == FEAT_CHASM)
        {
            if (feat == FEAT_CHASM)
            {
                int pi = partition_index_from_point(y, x, rows, cols);
                int bridge_style = partition_bridge_style_for_index(pi);

                if (pi < 0 || pi >= 25 || pi >= current_partition_count
                    || current_partition_modes[pi] != QUAD_MODE_CHASM)
                {
                    return false;
                }

                cave_set_feat_style(y, x, FEAT_FLOOR, bridge_style);
                cave_info[y][x] |= CAVE_CHASM_AREA;
                cave_info[y][x] &= ~CAVE_ROOM;
            }
            else
            {
                cave_set_feat(y, x, FEAT_FLOOR);
            }
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
            carved = true;
        }
        else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
        {
            /* Don't carve through inner/solid room walls, rubble walls, etc. */
            if (feat != FEAT_WALL_EXTRA)
                return false;
        }
        else if (feature_is_any_door(feat) || feat == FEAT_FLOOR)
        {
            /* Already passable; keep it. */
        }
        else
        {
            /* Avoid unexpected terrain (stairs, traps, etc.) */
            return false;
        }

        if (!(cave_info[y][x] & CAVE_ROOM))
            mark_generation_escape_tunnel(y, x);

        if (y == y2 && x == x2)
            break;
        y += dy;
        x += dx;
    }

    return carved;
}

bool connect_adjacent_big_partitions_by_boundary(
    int pi_a, int pi_b, const rectangle *bounds_a, const rectangle *bounds_b,
    int rows, int cols, int hub_a, int hub_b, bool vertical_boundary)
{
    const int SEARCH_DEPTH = 32;
    quadrant_mode_t mode_a = current_partition_modes[pi_a];
    quadrant_mode_t mode_b = current_partition_modes[pi_b];

    if (hub_a < 0 || hub_b < 0)
        return false;

    if (!bounds_a || !bounds_b)
        return false;

    if (vertical_boundary)
    {
        /* A is left of B: boundary at the start column of B. */
        int boundary_x = ((pi_b % cols) * p_ptr->cur_map_wid / cols);
        int y_lo = MAX(bounds_a->y1, bounds_b->y1);
        int y_hi = MIN(bounds_a->y2, bounds_b->y2);

        boundary_x = MAX(1, MIN(p_ptr->cur_map_wid - 2, boundary_x));
        if (y_hi - y_lo < 6)
            return false;

        int best_y = -1;
        int best_left = -1;
        int best_right = -1;
        int best_len = 999999;

        for (int y = y_lo + 2; y <= y_hi - 2; ++y)
        {
            int x_left = -1;
            for (int dx = 1; dx <= SEARCH_DEPTH; ++dx)
            {
                int x = boundary_x - dx;
                if (x < bounds_a->x1)
                    break;
                if (partition_index_from_point(y, x, rows, cols) != pi_a)
                    continue;
                if (big_partition_boundary_floor_ok(mode_a, y, x))
                {
                    x_left = x;
                    break;
                }
            }

            int x_right = -1;
            for (int dx = 0; dx <= SEARCH_DEPTH; ++dx)
            {
                int x = boundary_x + dx;
                if (x > bounds_b->x2)
                    break;
                if (partition_index_from_point(y, x, rows, cols) != pi_b)
                    continue;
                if (big_partition_boundary_floor_ok(mode_b, y, x))
                {
                    x_right = x;
                    break;
                }
            }

            if (x_left >= 0 && x_right >= 0 && x_left < x_right)
            {
                int len = x_right - x_left;
                if (len < best_len || (len == best_len && one_in_(2)))
                {
                    best_len = len;
                    best_y = y;
                    best_left = x_left;
                    best_right = x_right;
                }
            }
        }

        if (best_y < 0)
            return false;

        if (!carve_straight_big_partition_connector(
                best_y, best_left, best_y, best_right,
                hub_a, hub_b, rows, cols))
            return false;

        dun->connection[hub_a][hub_b] = true;
        dun->connection[hub_b][hub_a] = true;
        genlog_connect("Partition boundary: carved H link rooms %d<->%d at y=%d x=%d..%d",
            hub_a, hub_b, best_y, best_left, best_right);
        return true;
    }

    /* Horizontal boundary: A is above B. */
    int boundary_y = ((pi_b / cols) * p_ptr->cur_map_hgt / rows);
    int x_lo = MAX(bounds_a->x1, bounds_b->x1);
    int x_hi = MIN(bounds_a->x2, bounds_b->x2);

    boundary_y = MAX(1, MIN(p_ptr->cur_map_hgt - 2, boundary_y));
    if (x_hi - x_lo < 6)
        return false;

    int best_x = -1;
    int best_up = -1;
    int best_down = -1;
    int best_len = 999999;

    for (int x = x_lo + 2; x <= x_hi - 2; ++x)
    {
        int y_up = -1;
        for (int dy = 1; dy <= SEARCH_DEPTH; ++dy)
        {
            int y = boundary_y - dy;
            if (y < bounds_a->y1)
                break;
            if (partition_index_from_point(y, x, rows, cols) != pi_a)
                continue;
            if (big_partition_boundary_floor_ok(mode_a, y, x))
            {
                y_up = y;
                break;
            }
        }

        int y_down = -1;
        for (int dy = 0; dy <= SEARCH_DEPTH; ++dy)
        {
            int y = boundary_y + dy;
            if (y > bounds_b->y2)
                break;
            if (partition_index_from_point(y, x, rows, cols) != pi_b)
                continue;
            if (big_partition_boundary_floor_ok(mode_b, y, x))
            {
                y_down = y;
                break;
            }
        }

        if (y_up >= 0 && y_down >= 0 && y_up < y_down)
        {
            int len = y_down - y_up;
            if (len < best_len || (len == best_len && one_in_(2)))
            {
                best_len = len;
                best_x = x;
                best_up = y_up;
                best_down = y_down;
            }
        }
    }

    if (best_x < 0)
        return false;

    if (!carve_straight_big_partition_connector(
            best_up, best_x, best_down, best_x,
            hub_a, hub_b, rows, cols))
        return false;

    dun->connection[hub_a][hub_b] = true;
    dun->connection[hub_b][hub_a] = true;
    genlog_connect("Partition boundary: carved V link rooms %d<->%d at x=%d y=%d..%d",
        hub_a, hub_b, best_x, best_up, best_down);
    return true;
}

void seed_partition_adjacency(const int *room_to_part, int part_count, bool adj[25][25], int degree[25])
{
    for (int i = 0; i < part_count; ++i)
        degree[i] = 0;

    for (int i = 0; i < part_count; ++i)
        for (int j = 0; j < part_count; ++j)
            adj[i][j] = false;

    for (int a = 0; a < dun->cent_n; ++a)
    {
        int pa = (a < CENT_MAX) ? room_to_part[a] : -1;
        if (pa < 0 || pa >= part_count) continue;

        for (int b = a + 1; b < dun->cent_n; ++b)
        {
            if (!dun->connection[a][b]) continue;
            int pb = (b < CENT_MAX) ? room_to_part[b] : -1;
            if (pb < 0 || pb >= part_count || pb == pa) continue;
            if (!adj[pa][pb])
            {
                adj[pa][pb] = adj[pb][pa] = true;
                degree[pa]++;
                degree[pb]++;
            }
        }
    }
}

void mark_partition_edge(int p1, int p2, bool adj[25][25], int degree[25])
{
    if (p1 < 0 || p2 < 0 || p1 >= 25 || p2 >= 25 || p1 == p2)
        return;
    if (!adj[p1][p2])
    {
        adj[p1][p2] = adj[p2][p1] = true;
        degree[p1]++;
        degree[p2]++;
    }
}

int choose_partition_hub(const partition_link_data_t *part)
{
    int best = -1;
    int best_rank = -1;
    int best_area = -1;
    int best_dist = 999999;

    int limit = MIN(part->room_count, CENT_MAX);
    for (int i = 0; i < limit; ++i)
    {
        int r = part->rooms[i];
        int area = (dun->corner[r].y2 - dun->corner[r].y1 + 1) *
                   (dun->corner[r].x2 - dun->corner[r].x1 + 1);
        int dist = distance(dun->cent[r].y, dun->cent[r].x, part->center.y, part->center.x);
        int rank = room_anchor_requires_neighbor[r] ? 2 :
                   (room_anchor_kind[r] != LAYOUT_ANCHOR_NONE ? 1 : 0);

        if (rank > best_rank ||
            (rank == best_rank && area > best_area) ||
            (rank == best_rank && area == best_area && dist < best_dist))
        {
            best = r;
            best_rank = rank;
            best_area = area;
            best_dist = dist;
        }
    }
    return best;
}

int find_anchor_target(int src, const int *room_to_part, const bool *skip, int part_count)
{
    int src_part = (src >= 0 && src < CENT_MAX) ? room_to_part[src] : -1;
    int src_piece = (src >= 0 && src < dun->cent_n) ? dun->piece[src] : -1;
    int best = -1;
    int best_tier = 10;
    int best_dist = 999999;

    for (int r = 0; r < dun->cent_n; ++r)
    {
        if (r == src) continue;
        if (skip && skip[r]) continue;
        if (dun->connection[src][r]) continue;

        int tier = 2;
        if (src_piece > 0 && dun->piece[r] > 0 && dun->piece[r] != src_piece)
            tier = 0;
        else if (room_to_part && r < CENT_MAX && room_to_part[r] != src_part)
            tier = 1;

        if (part_count > 0 && room_to_part && (room_to_part[r] < 0 || room_to_part[r] >= part_count))
            continue;

        int dist = distance(dun->cent[src].y, dun->cent[src].x, dun->cent[r].y, dun->cent[r].x);
        if (tier < best_tier || (tier == best_tier && dist < best_dist))
        {
            best_tier = tier;
            best_dist = dist;
            best = r;
        }
    }
    return best;
}

void connect_anchor_backbone(const int *room_to_part, int part_count)
{
    if (layout_anchor_count <= 0 || dun->cent_n <= 0)
        return;

    (void)dungeon_pieces();

    int anchors_linked = 0;
    int anchors_considered = 0;

    for (int i = 0; i < layout_anchor_count; ++i)
    {
        int r = layout_anchors[i].room_slot;
        if (r < 0 || r >= dun->cent_n)
            continue;

        anchors_considered++;
        int area = (dun->corner[r].y2 - dun->corner[r].y1 + 1) *
                   (dun->corner[r].x2 - dun->corner[r].x1 + 1);
        int target_degree = 1;
        if (layout_anchors[i].requires_neighbor)
            target_degree = 2;
        if (area >= 600)
            target_degree = MAX(target_degree, 2);
        if (area >= 900)
            target_degree = MAX(target_degree, 3);

        int deg = room_connection_degree(r);
        bool tried[CENT_MAX];
        for (int t = 0; t < CENT_MAX; ++t) tried[t] = false;

        int attempts = 0;
        while (deg < target_degree && attempts < 8)
        {
            attempts++;
            int target = find_anchor_target(r, room_to_part, tried, part_count);
            if (target < 0)
                break;

            tried[target] = true;
            if (connect_rooms_with_logging(r, target, "Anchor backbone", true))
            {
                anchors_linked++;
                deg++;
                (void)dungeon_pieces();
            }
        }
    }

    if (anchors_linked > 0)
    {
        genlog_connect("Anchor backbone: linked %d/%d anchors to reduce isolation", anchors_linked, anchors_considered);
    }
}

/* Add connective tissue between partitions by linking a representative room in each partition,
 * then ensure special anchors have multiple exits to avoid dead ends. */
void connect_partition_hubs(void)
{
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
    int rows = current_partition_rows;
    int cols = current_partition_cols;
    int count = current_partition_count;

    if (rows <= 0 || cols <= 0) {
        fallback_partition_grid_from_blocks(blocks, &rows, &cols);
        count = rows * cols;
    }
    if (count <= 1 || rows <= 0 || cols <= 0)
        return;

    partition_link_data_t parts[25];
    int room_to_part[CENT_MAX];
    for (int i = 0; i < CENT_MAX; ++i) room_to_part[i] = -1;

    for (int pi = 0; pi < count && pi < 25; ++pi)
    {
        parts[pi].room_count = 0;
        parts[pi].hub_room = -1;
        int y1, y2, x1, x2;
        if (compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
        {
            parts[pi].bounds.y1 = y1;
            parts[pi].bounds.y2 = y2;
            parts[pi].bounds.x1 = x1;
            parts[pi].bounds.x2 = x2;
            parts[pi].center.y = (y1 + y2) / 2;
            parts[pi].center.x = (x1 + x2) / 2;
        }
    }

    for (int r = 0; r < dun->cent_n && r < CENT_MAX; ++r)
    {
        int pi = partition_index_from_point(dun->cent[r].y, dun->cent[r].x, rows, cols);
        room_to_part[r] = pi;
        if (pi < 0 || pi >= count || pi >= 25)
            continue;
        int idx = parts[pi].room_count++;
        if (idx < CENT_MAX)
            parts[pi].rooms[idx] = r;
    }

    for (int pi = 0; pi < count && pi < 25; ++pi)
        parts[pi].hub_room = choose_partition_hub(&parts[pi]);

    bool adj[25][25];
    int degree[25];
    seed_partition_adjacency(room_to_part, count, adj, degree);

    /* Connect adjacent big partitions (labyrinth, big_cave, chasm) FIRST before regular backbone */
    /* This ensures big partitions get priority connections to each other */
    int big_links = 0;
    int big_adjacencies_found = 0;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int idx = row * cols + col;
            if (idx >= count || idx >= 25)
                continue;

            quadrant_mode_t mode = current_partition_modes[idx];
            bool is_big = is_big_partition_mode(mode);
            if (!is_big)
                continue;

            int hub_here = parts[idx].hub_room;
            if (hub_here < 0)
                continue;

            /* Check right neighbor */
            if (col + 1 < cols)
            {
                int idx_r = row * cols + (col + 1);
                if (idx_r < count && idx_r < 25)
                {
                    quadrant_mode_t mode_r = current_partition_modes[idx_r];
                    bool is_big_r = is_big_partition_mode(mode_r);
                    if (is_big_r && !adj[idx][idx_r])
                    {
                        big_adjacencies_found++;
                        int hub_right = parts[idx_r].hub_room;
                        bool ok = false;
                        if (hub_right >= 0)
                        {
                            ok = connect_rooms_with_logging(hub_here, hub_right, "Big partition bridge H", true);
                            if (!ok)
                            {
                                ok = connect_adjacent_big_partitions_by_boundary(
                                    idx, idx_r, &parts[idx].bounds, &parts[idx_r].bounds,
                                    rows, cols, hub_here, hub_right, true);
                            }
                        }

                        if (ok)
                        {
                            mark_partition_edge(idx, idx_r, adj, degree);
                            big_links++;
                            genlog_connect("Big partition bridge: connected %s at [%d,%d] to %s at [%d,%d] (horizontal)",
                                         mode == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col,
                                         mode_r == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode_r == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col + 1);
                        }
                    }
                }
            }

            /* Check down neighbor */
            if (row + 1 < rows)
            {
                int idx_d = (row + 1) * cols + col;
                if (idx_d < count && idx_d < 25)
                {
                    quadrant_mode_t mode_d = current_partition_modes[idx_d];
                    bool is_big_d = is_big_partition_mode(mode_d);
                    if (is_big_d && !adj[idx][idx_d])
                    {
                        big_adjacencies_found++;
                        int hub_down = parts[idx_d].hub_room;
                        bool ok = false;
                        if (hub_down >= 0)
                        {
                            ok = connect_rooms_with_logging(hub_here, hub_down, "Big partition bridge V", true);
                            if (!ok)
                            {
                                ok = connect_adjacent_big_partitions_by_boundary(
                                    idx, idx_d, &parts[idx].bounds, &parts[idx_d].bounds,
                                    rows, cols, hub_here, hub_down, false);
                            }
                        }

                        if (ok)
                        {
                            mark_partition_edge(idx, idx_d, adj, degree);
                            big_links++;
                            genlog_connect("Big partition bridge: connected %s at [%d,%d] to %s at [%d,%d] (vertical)",
                                         mode == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col,
                                         mode_d == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode_d == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row + 1, col);
                        }
                    }
                }
            }
        }
    }

    if (big_links > 0)
    {
        log_trace("Big partition bridges: added %d connections between adjacent labyrinths/caves/chasms (found %d adjacencies)",
                  big_links, big_adjacencies_found);
        genlog_connect("Big partition bridges: connected %d pairs of adjacent big partitions", big_links);
    }

    /* Now run regular partition backbone connections */
    int links = 0;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int idx = row * cols + col;
            int hub_here = (idx < 25) ? parts[idx].hub_room : -1;
            if (hub_here < 0)
                continue;

            if (col + 1 < cols)
            {
                int idx_r = row * cols + (col + 1);
                int hub_right = parts[idx_r].hub_room;
                bool ok = false;
                if (hub_right >= 0)
                {
                    ok = connect_rooms_with_logging(hub_here, hub_right, "Partition backbone H", true);
                    if (!ok)
                        ok = connect_adjacent_big_partitions_by_boundary(
                            idx, idx_r, &parts[idx].bounds, &parts[idx_r].bounds,
                            rows, cols, hub_here, hub_right, true);
                }
                if (ok)
                {
                    mark_partition_edge(idx, idx_r, adj, degree);
                    links++;
                }
            }

            if (row + 1 < rows)
            {
                int idx_d = (row + 1) * cols + col;
                int hub_down = parts[idx_d].hub_room;
                bool ok = false;
                if (hub_down >= 0)
                {
                    ok = connect_rooms_with_logging(hub_here, hub_down, "Partition backbone V", true);
                    if (!ok)
                        ok = connect_adjacent_big_partitions_by_boundary(
                            idx, idx_d, &parts[idx].bounds, &parts[idx_d].bounds,
                            rows, cols, hub_here, hub_down, false);
                }
                if (ok)
                {
                    mark_partition_edge(idx, idx_d, adj, degree);
                    links++;
                }
            }

            if (col + 1 < cols && row + 1 < rows)
            {
                int idx_dr = (row + 1) * cols + (col + 1);
                int hub_diag = parts[idx_dr].hub_room;
                if (hub_diag >= 0 && connect_rooms_with_logging(hub_here, hub_diag, "Partition backbone D", true))
                {
                    mark_partition_edge(idx, idx_dr, adj, degree);
                    links++;
                }
            }
        }
    }

    int target_degree = (count >= 3) ? 2 : 1;
    for (int pi = 0; pi < count && pi < 25; ++pi)
    {
        if (parts[pi].hub_room < 0)
            continue;
        if (degree[pi] >= target_degree)
            continue;

        int attempts = 0;
        bool failed_candidate[25] = {false};
        while (degree[pi] < target_degree && attempts < count)
        {
            attempts++;
            int best = -1;
            int best_dist = 999999;
            for (int pj = 0; pj < count && pj < 25; ++pj)
            {
                if (pj == pi) continue;
                if (parts[pj].hub_room < 0) continue;
                if (adj[pi][pj]) continue;
                if (failed_candidate[pj]) continue;
                int dist = distance(parts[pi].center.y, parts[pi].center.x, parts[pj].center.y, parts[pj].center.x);
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best = pj;
                }
            }

            if (best < 0)
                break;

            if (connect_rooms_with_logging(parts[pi].hub_room, parts[best].hub_room, "Partition backbone fill", true))
            {
                mark_partition_edge(pi, best, adj, degree);
                links++;
            }
            else
            {
                failed_candidate[best] = true;
            }
        }
    }

    if (links > 0)
        log_trace("Partition hub pass: added %d backbone links (grid %dx%d)", links, rows, cols);

    connect_anchor_backbone(room_to_part, count);
}
