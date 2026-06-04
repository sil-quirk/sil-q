/* File: level-generation-layout-caves.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

void scatter_quartz_veins_in_bounds(int y1, int y2, int x1, int x2, u16b info_flag)
{
    int vein_count = 0;

    /* Iterate over the bounds and convert some adjacent-to-floor walls to quartz */
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;

            /* Only consider granite walls */
            int feat = cave_feat[gy][gx];
            if (feat < FEAT_WALL_EXTRA || feat > FEAT_WALL_SOLID)
                continue;

            /* Check if adjacent to at least one cave floor tile (CAVE_ROOM) */
            bool adj_cave_floor = false;
            for (int dy = -1; dy <= 1 && !adj_cave_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !adj_cave_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                        adj_cave_floor = true;
                }
            }

            /* If adjacent to cave floor, ~30% chance to become quartz vein */
            if (adj_cave_floor && (rand_int(100) < 30))
            {
                cave_set_feat(gy, gx, FEAT_QUARTZ);
                /* Mark as part of a room so tunneling can detect cave quartz */
                cave_info[gy][gx] |= (CAVE_ROOM | info_flag);
                vein_count++;
            }
        }
    }

    if (vein_count > 0)
    {
        log_trace("scatter_quartz_veins: placed %d veins in bounds (%d,%d)-(%d,%d)",
                  vein_count, y1, x1, y2, x2);
    }
}

/* Return true when the bounds contain tagged native chasm floor tiles. */
bool bounds_have_chasm_tag(int y1, int y2, int x1, int x2)
{
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx) && (cave_info[gy][gx] & CAVE_CHASM_AREA))
                return true;
        }
    }
    return false;
}

/* Carve a small cellular-automata style blob and register it as an anchor */
#if 0
bool carve_ca_blob_anchor(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;

    /* Pick blob dimensions (moderate footprint to avoid over-densifying) */
    int h = rand_range(8, 12);
    int w = rand_range(10, 16);
    int y1 = rand_range(3, p_ptr->cur_map_hgt - h - 3);
    int x1 = rand_range(3, p_ptr->cur_map_wid - w - 3);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    /* Ensure we are carving into untouched granite */
    /* Allow slight overlap with walls but not existing floors */
    if (y1 < 1 || x1 < 1 || y2 >= p_ptr->cur_map_hgt - 1 || x2 >= p_ptr->cur_map_wid - 1)
        return false;
    for (int y = y1 - 1; y <= y2 + 1; ++y)
    {
        for (int x = x1 - 1; x <= x2 + 1; ++x)
        {
            if (cave_floor_bold(y, x))
                return false;
        }
    }

    /* Simple CA grid stored on stack (max ~20x20) */
    bool grid[24][24];
    if (h > 24 || w > 24)
        return false;

    /* Seed noise with a bias to produce irregular shapes */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            grid[y][x] = (rand_int(100) < 45); /* 45% initial fill */

    /* Run several smoothing steps to create rounded blobs */
    int steps = 3;
    for (int step = 0; step < steps; ++step)
    {
        bool next[24][24];
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0)
                            continue;
                        int ny = y + dy;
                        int nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (grid[ny][nx])
                            neighbors++;
                    }
                }
                /* Slightly denser survival/birth to keep blobs cohesive */
                if (grid[y][x])
                    next[y][x] = (neighbors >= 4);
                else
                    next[y][x] = (neighbors >= 5);
            }
        }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                grid[y][x] = next[y][x];
    }

    /* Apply to dungeon */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    /* Clear box to raw granite to avoid rectangular outlines */
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            if (in_bounds_fully(gy, gx))
                cave_set_feat(gy, gx, FEAT_WALL_EXTRA);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (!grid[y][x])
                continue;
            int gy = y1 + y;
            int gx = x1 + x;
            cave_set_feat(gy, gx, FEAT_FLOOR);
            cave_info[gy][gx] |= CAVE_ROOM;
            floor_count++;
            if (gy < min_y)
                min_y = gy;
            if (gy > max_y)
                max_y = gy;
            if (gx < min_x)
                min_x = gx;
            if (gx > max_x)
                max_x = gx;
        }
    }

    /* Ragged edge expansion to break rectangular silhouette */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        {
            for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            {
                if (cave_floor_bold(gy, gx))
                    continue;
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dy || dx)
                        {
                            int ny = gy + dy, nx = gx + dx;
                            if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                                adj++;
                        }
                if (adj >= 3 && one_in_(2 + pass))
                {
                    cave_set_feat(gy, gx, FEAT_FLOOR);
                    cave_info[gy][gx] |= CAVE_ROOM;
                    floor_count++;
                    if (gy < min_y)
                        min_y = gy;
                    if (gy > max_y)
                        max_y = gy;
                    if (gx < min_x)
                        min_x = gx;
                    if (gx > max_x)
                        max_x = gx;
                }
            }
        }
    }

    /* Bleed outward a little to break boxy outlines */
    const int bleed_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!cave_floor_bold(gy, gx))
                continue;
            bool on_edge = (gy == y1 - 1) || (gy == y2 + 1) || (gx == x1 - 1) || (gx == x2 + 1);
            if (!on_edge)
                continue;
            for (int d = 0; d < 4; ++d)
            {
                int ny = gy + bleed_dirs[d][0];
                int nx = gx + bleed_dirs[d][1];
                if (!in_bounds_fully(ny, nx))
                    continue;
                if (cave_floor_bold(ny, nx))
                    continue;
                if (cave_feat[ny][nx] != FEAT_WALL_EXTRA)
                    continue;
                if (one_in_(4))
                {
                    cave_set_feat(ny, nx, FEAT_FLOOR);
                    cave_info[ny][nx] |= CAVE_ROOM;
                    floor_count++;
                    if (ny < min_y)
                        min_y = ny;
                    if (ny > max_y)
                        max_y = ny;
                    if (nx < min_x)
                        min_x = nx;
                    if (nx > max_x)
                        max_x = nx;
                }
            }
        }
    }

    if (floor_count < 8)
        return false;

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
            }
        }
    }

    /* Pick a center on a floor tile */
    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty;
            cx = tx;
            break;
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, one_in_(3));

    /* Scatter quartz veins around the cave walls for natural appearance */
    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);

    log_trace("CA blob anchor: carved floor_count=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d)",
        floor_count, min_y, min_x, max_y, max_x, cy, cx);
    genlog_anchor("CA_BLOB: carved %d floor tiles at (%d,%d)-(%d,%d), center=(%d,%d)",
        floor_count, min_y, min_x, max_y, max_x, cy, cx);
    return true;
}
#endif

/* Bounded version for quadrants */
bool carve_ca_blob_anchor_bounds(int y_min, int y_max, int x_min, int x_max, int style_idx)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;
    int old_h = p_ptr->cur_map_hgt;
    int old_w = p_ptr->cur_map_wid;
    /* Temporarily clamp selection by picking starting coordinates inside bounds */
    if (y_max - y_min < 8 || x_max - x_min < 8)
        return false;
    int h = rand_range(8, MIN(14, y_max - y_min));
    int w = rand_range(10, MIN(16, x_max - x_min));
    int y1 = rand_range(y_min + 1, y_max - h);
    int x1 = rand_range(x_min + 1, x_max - w);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (y1 < 1 || x1 < 1 || y2 >= old_h - 1 || x2 >= old_w - 1)
        return false;
    for (int y = y1 - 1; y <= y2 + 1; ++y)
        for (int x = x1 - 1; x <= x2 + 1; ++x)
            if (cave_floor_bold(y, x))
                return false;

    /* Simple CA grid stored on stack */
    bool grid[24][24];
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            grid[y][x] = (rand_int(100) < 45);

    int steps = 3;
    for (int step = 0; step < steps; ++step)
    {
        bool next[24][24];
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0) continue;
                        int ny = y + dy, nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (grid[ny][nx])
                            neighbors++;
                    }
                }
                next[y][x] = grid[y][x] ? (neighbors >= 4) : (neighbors >= 5);
            }
        }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                grid[y][x] = next[y][x];
    }

    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    int floor_count = 0;
    /* Clear box to raw granite to avoid rectangular outlines */
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            if (in_bounds_fully(gy, gx))
                cave_set_feat_style(gy, gx, FEAT_WALL_EXTRA, style_idx);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (!grid[y][x]) continue;
            int gy = y1 + y;
            int gx = x1 + x;
            cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
            cave_info[gy][gx] |= CAVE_ROOM;
            floor_count++;
            if (gy < min_y) min_y = gy;
            if (gy > max_y) max_y = gy;
            if (gx < min_x) min_x = gx;
            if (gx > max_x) max_x = gx;
        }
    }
    if (floor_count < 8)
        return false;

    /* Ragged edge expansion to break rectangular silhouette */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        {
            for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            {
                if (cave_floor_bold(gy, gx))
                    continue;
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dy || dx)
                        {
                            int ny = gy + dy, nx = gx + dx;
                            if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                                adj++;
                        }
                if (adj >= 3 && one_in_(2 + pass))
                {
                    cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                    cave_info[gy][gx] |= CAVE_ROOM;
                    floor_count++;
                    if (gy < min_y)
                        min_y = gy;
                    if (gy > max_y)
                        max_y = gy;
                    if (gx < min_x)
                        min_x = gx;
                    if (gx > max_x)
                        max_x = gx;
                }
            }
        }
    }

    /* Bleed outward along the edge to soften rectangles */
    const int bleed_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!cave_floor_bold(gy, gx))
                continue;
            bool on_edge = (gy == y1 - 1) || (gy == y2 + 1) || (gx == x1 - 1) || (gx == x2 + 1);
            if (!on_edge)
                continue;
            for (int d = 0; d < 4; ++d)
            {
                int ny = gy + bleed_dirs[d][0];
                int nx = gx + bleed_dirs[d][1];
                if (!in_bounds_fully(ny, nx))
                    continue;
                if (cave_floor_bold(ny, nx))
                    continue;
                if (cave_feat[ny][nx] != FEAT_WALL_EXTRA)
                    continue;
                if (one_in_(4))
                {
                    cave_set_feat_style(ny, nx, FEAT_FLOOR, style_idx);
                    cave_info[ny][nx] |= CAVE_ROOM;
                    floor_count++;
                    if (ny < min_y)
                        min_y = ny;
                    if (ny > max_y)
                        max_y = ny;
                    if (nx < min_x)
                        min_x = nx;
                    if (nx > max_x)
                        max_x = nx;
                }
            }
        }
    }

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
            }
        }
    }

    /* Pick center - prefer floor tile adjacent to outer wall for tunnel connectivity */
    int cy = min_y, cx = min_x;
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (!cave_floor_bold(ty, tx)) continue;

        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0) continue;
                if (in_bounds_fully(ty + dy, tx + dx) &&
                    cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    cy = ty; cx = tx;
                    found_edge = true;
                }
            }
        }
    }
    /* Fallback: any floor tile */
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(min_y, max_y);
            int tx = rand_range(min_x, max_x);
            if (cave_floor_bold(ty, tx))
            {
                cy = ty; cx = tx; break;
            }
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, one_in_(4));

    /* Scatter quartz veins around the cave walls for natural appearance */
    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);

    log_trace("CA blob (bounded) anchor: bounds=(%d,%d)-(%d,%d) center=(%d,%d) floors=%d", min_y, min_x, max_y, max_x, cy, cx, floor_count);
    return true;
}

/* Keep only the dominant big-cave component so later rescue tunneling does not
 * turn detached floor pockets into odd loot closets. */
int prune_big_cave_detached_components(
    int y1, int y2, int x1, int x2, int style_idx)
{
    static u16b component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};
    int next_component = 1;
    int best_component = 0;
    int best_size = 0;
    int pruned = 0;

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            component[y][x] = 0;

    for (int sy = y1; sy <= y2; ++sy)
    {
        for (int sx = x1; sx <= x2; ++sx)
        {
            if (component[sy][sx] != 0)
                continue;
            if (!cave_floor_bold(sy, sx) || !(cave_info[sy][sx] & CAVE_ROOM))
                continue;

            int head = 0;
            int tail = 0;
            int size = 0;

            component[sy][sx] = (u16b)next_component;
            queue[tail++] = sy * MAX_DUNGEON_WID + sx;

            while (head < tail)
            {
                int cur = queue[head++];
                int cy = cur / MAX_DUNGEON_WID;
                int cx = cur % MAX_DUNGEON_WID;

                size++;

                for (int d = 0; d < 4; ++d)
                {
                    int ny = cy + ddy4[d];
                    int nx = cx + ddx4[d];

                    if (ny < y1 || ny > y2 || nx < x1 || nx > x2)
                        continue;
                    if (component[ny][nx] != 0)
                        continue;
                    if (!cave_floor_bold(ny, nx) || !(cave_info[ny][nx] & CAVE_ROOM))
                        continue;

                    component[ny][nx] = (u16b)next_component;
                    queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }

            if (size > best_size)
            {
                best_size = size;
                best_component = next_component;
            }

            next_component++;
        }
    }

    if (best_component == 0)
        return 0;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (component[y][x] == 0 || component[y][x] == best_component)
                continue;

            cave_set_feat_style(y, x, FEAT_WALL_EXTRA, style_idx);
            cave_info[y][x] &= ~(CAVE_ROOM | CAVE_CHASM_AREA);
            pruned++;
        }
    }

    if (pruned > 0)
    {
        log_trace("Big cave cleanup: pruned %d detached floor tiles in bounds (%d,%d)-(%d,%d)",
            pruned, y1, x1, y2, x2);
        genlog_anchor("BIG_CAVE: pruned %d detached floor tiles in bounds (%d,%d)-(%d,%d)",
            pruned, y1, x1, y2, x2);
    }

    return pruned;
}

#if 0
/* Experimental chasm-mask shaping/connectivity helpers. Disabled because they
 * were producing unstable chasm layouts in the current generator. */
bool chasm_mask_has_clearance(
    const bool* is_cave, int h, int w, int ly, int lx, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            int ny = ly + dy;
            int nx = lx + dx;

            if (ny < 0 || ny >= h || nx < 0 || nx >= w)
                return false;
            if (!is_cave[ny * w + nx])
                return false;
        }
    }

    return true;
}

bool repair_chasm_walkable_connectivity(
    int y1, int y2, int x1, int x2, int bridge_style)
{
    static u16b component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int component_size[MAX_DUNGEON_HGT * MAX_DUNGEON_WID + 1];
    static int prev[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy8[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int ddx8[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};
    int bridges_added = 0;

    for (;;)
    {
        int component_count = 0;
        int main_component = 0;
        int main_size = 0;

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                component[y][x] = 0;
            }
        }

        for (int i = 0; i < (int)N_ELEMENTS(component_size); ++i)
            component_size[i] = 0;

        for (int sy = y1; sy <= y2; ++sy)
        {
            for (int sx = x1; sx <= x2; ++sx)
            {
                if (component[sy][sx] != 0)
                    continue;
                if (!chasm_native_walkable_bold(sy, sx))
                    continue;

                int head = 0;
                int tail = 0;

                component_count++;
                component[sy][sx] = (u16b)component_count;
                queue[tail++] = sy * MAX_DUNGEON_WID + sx;

                while (head < tail)
                {
                    int cur = queue[head++];
                    int cy = cur / MAX_DUNGEON_WID;
                    int cx = cur % MAX_DUNGEON_WID;

                    component_size[component_count]++;

                    for (int d = 0; d < 8; ++d)
                    {
                        int ny = cy + ddy8[d];
                        int nx = cx + ddx8[d];

                        if (ny < y1 || ny > y2 || nx < x1 || nx > x2)
                            continue;
                        if (component[ny][nx] != 0)
                            continue;
                        if (!chasm_native_walkable_bold(ny, nx))
                            continue;

                        component[ny][nx] = (u16b)component_count;
                        queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                    }
                }

                if (component_size[component_count] > main_size)
                {
                    main_size = component_size[component_count];
                    main_component = component_count;
                }
            }
        }

        if (component_count <= 1)
            break;

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                prev[y][x] = -1;
            }
        }

        int head = 0;
        int tail = 0;
        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                if (component[y][x] != main_component)
                    continue;

                prev[y][x] = y * MAX_DUNGEON_WID + x;
                queue[tail++] = y * MAX_DUNGEON_WID + x;
            }
        }

        int found_y = -1;
        int found_x = -1;
        while (head < tail && found_y < 0)
        {
            int cur = queue[head++];
            int cy = cur / MAX_DUNGEON_WID;
            int cx = cur % MAX_DUNGEON_WID;

            for (int d = 0; d < 4; ++d)
            {
                int ny = cy + ddy4[d];
                int nx = cx + ddx4[d];
                int nidx;

                if (ny < y1 || ny > y2 || nx < x1 || nx > x2)
                    continue;
                if (prev[ny][nx] != -1)
                    continue;
                if (!(cave_info[ny][nx] & CAVE_CHASM_AREA))
                    continue;

                if (component[ny][nx] != 0 && component[ny][nx] != main_component)
                {
                    prev[ny][nx] = cur;
                    found_y = ny;
                    found_x = nx;
                    break;
                }

                if (cave_feat[ny][nx] != FEAT_CHASM)
                    continue;

                nidx = ny * MAX_DUNGEON_WID + nx;
                prev[ny][nx] = cur;
                queue[tail++] = nidx;
            }
        }

        if (found_y < 0 || found_x < 0)
        {
            log_trace("Chasm connectivity repair failed in bounds (%d,%d)-(%d,%d): components=%d",
                y1, x1, y2, x2, component_count);
            genlog_anchor("CHASM: connectivity repair failed in bounds (%d,%d)-(%d,%d), components=%d",
                y1, x1, y2, x2, component_count);
            return false;
        }

        int cur = found_y * MAX_DUNGEON_WID + found_x;
        while (prev[cur / MAX_DUNGEON_WID][cur % MAX_DUNGEON_WID] != cur)
        {
            int cy = cur / MAX_DUNGEON_WID;
            int cx = cur % MAX_DUNGEON_WID;

            if (cave_feat[cy][cx] == FEAT_CHASM)
            {
                cave_set_feat_style(cy, cx, FEAT_FLOOR, bridge_style);
                cave_info[cy][cx] |= (CAVE_ROOM | CAVE_CHASM_AREA);
                bridges_added++;
            }

            cur = prev[cy][cx];
            if (cur < 0)
                break;
        }
    }

    if (bridges_added > 0)
    {
        log_trace("Chasm connectivity repair: added %d bridge tiles in bounds (%d,%d)-(%d,%d)",
            bridges_added, y1, x1, y2, x2);
        genlog_anchor("CHASM: connectivity repair added %d bridge tiles in bounds (%d,%d)-(%d,%d)",
            bridges_added, y1, x1, y2, x2);
    }

    return true;
}
#endif
