/* File: level-generation-layout-big-cave.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

bool carve_big_cave_bounds(int y_min, int y_max, int x_min, int x_max,
    int style_idx, big_cave_type_t cave_type)
{
    (void)cave_type;

    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("BIG_CAVE: rejected - room capacity limit reached");
        return false;
    }

    /* Big caves need substantial space */
    /* Bounds are inclusive. Keep the local mask dimensions aligned with the
     * generation loops so the temporary cave/platform arrays cover every tile. */
    int avail_h = y_max - y_min + 1;
    int avail_w = x_max - x_min + 1;
    if (avail_h < 15 || avail_w < 20)
    {
        genlog_anchor("BIG_CAVE: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }

    /* Use smaller margins to create larger, more expansive caves */
    int margin_y1 = rand_range(2, MAX(4, avail_h / 5));
    int margin_y2 = rand_range(2, MAX(4, avail_h / 5));
    int margin_x1 = rand_range(2, MAX(4, avail_w / 5));
    int margin_x2 = rand_range(2, MAX(4, avail_w / 5));
    int y1 = y_min + margin_y1;
    int x1 = x_min + margin_x1;
    int y2 = y_max - margin_y2;
    int x2 = x_max - margin_x2;
    int h = y2 - y1 + 1;
    int w = x2 - x1 + 1;

    if (h < 10 || w < 12)
    {
        genlog_anchor("BIG_CAVE: rejected - after margins too small: h=%d w=%d",
                      h, w);
        return false;
    }

    /* Check area is basic granite */
    for (int y = y1 - 1; y <= y2 + 1; ++y)
    {
        for (int x = x1 - 1; x <= x2 + 1; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("BIG_CAVE: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }

    /* Let caves grow to fill the partition without artificial size caps */
    /* Removed h>50, w>60 limits for more expansive caves on larger levels */

    /* Use multiple overlapping CA blobs to create one large organic cave */
    /* This approach creates natural irregular shapes instead of rectangles */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;

    /* Number of blob centers based on area - more blobs for bigger caves */
    int num_centers = 4 + (h * w) / 150;  /* Increased from 3 + area/200 */
    if (num_centers > 12) num_centers = 12;  /* Raised cap from 8 to 12 */

    /* Generate random center points for blob nuclei */
    int centers_y[12], centers_x[12];  /* Increased from 8 to 12 */
    for (int c = 0; c < num_centers; ++c)
    {
        centers_y[c] = rand_range(y1 + 2, y2 - 2);
        centers_x[c] = rand_range(x1 + 2, x2 - 2);
    }

    /* Carve floor by distance from nearest center with noise */
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;

            /* Find distance to nearest center */
            int min_dist = 9999;
            for (int c = 0; c < num_centers; ++c)
            {
                int dy = ABS(gy - centers_y[c]);
                int dx = ABS(gx - centers_x[c]);
                int dist = dy + dx;  /* Manhattan distance */
                if (dist < min_dist) min_dist = dist;
            }

            /* Carve floor based on distance with randomness for organic edges */
            int threshold = (h + w) / 4;  /* Base carve radius */
            int noise = rand_int(threshold / 2);  /* Add randomness */

            if (min_dist < threshold - noise)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }

    /* Smooth the edges with a CA pass */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = min_y; gy <= max_y; ++gy)
        {
            for (int gx = min_x; gx <= max_x; ++gx)
            {
                if (!in_bounds_fully(gy, gx)) continue;
                if (cave_floor_bold(gy, gx)) continue;

                /* Count floor neighbors */
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dy || dx) && in_bounds_fully(gy+dy, gx+dx) && cave_floor_bold(gy+dy, gx+dx))
                            adj++;

                /* Fill in isolated wall cells surrounded by floor */
                if (adj >= 6)
                {
                    cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                    cave_info[gy][gx] |= CAVE_ROOM;
                    floor_count++;
                }
            }
        }
    }

    /* Carve a few boundary notches so the cave silhouette reads less like a box. */
    int bite_count = 2 + rand_int(3);
    for (int bite = 0; bite < bite_count; ++bite)
    {
        int side = rand_int(4);
        int by = 0;
        int bx = 0;
        int radius_y = rand_range(3, MAX(4, h / 5));
        int radius_x = rand_range(4, MAX(5, w / 5));

        switch (side)
        {
        case 0: /* top */
            by = y1 + rand_range(0, 2);
            bx = rand_range(x1 + MAX(3, w / 6), x2 - MAX(3, w / 6));
            break;
        case 1: /* bottom */
            by = y2 - rand_range(0, 2);
            bx = rand_range(x1 + MAX(3, w / 6), x2 - MAX(3, w / 6));
            break;
        case 2: /* left */
            by = rand_range(y1 + MAX(3, h / 6), y2 - MAX(3, h / 6));
            bx = x1 + rand_range(0, 2);
            break;
        default: /* right */
            by = rand_range(y1 + MAX(3, h / 6), y2 - MAX(3, h / 6));
            bx = x2 - rand_range(0, 2);
            break;
        }

        for (int gy = y1; gy <= y2; ++gy)
        {
            for (int gx = x1; gx <= x2; ++gx)
            {
                int dy = ABS(gy - by);
                int dx = ABS(gx - bx);
                int metric;

                if (!cave_floor_bold(gy, gx))
                    continue;
                if (!(cave_info[gy][gx] & CAVE_ROOM))
                    continue;
                if (dy > radius_y || dx > radius_x)
                    continue;

                metric = (dy * 100) / MAX(1, radius_y)
                    + (dx * 100) / MAX(1, radius_x);
                if (metric > 125 + rand_int(20))
                    continue;

                cave_set_feat_style(gy, gx, FEAT_WALL_EXTRA, style_idx);
                cave_info[gy][gx] &= ~CAVE_ROOM;
                floor_count--;
            }
        }
    }

    /* Erode some edge floor tiles for more irregular shape */
    for (int gy = min_y; gy <= max_y; ++gy)
    {
        for (int gx = min_x; gx <= max_x; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (!cave_floor_bold(gy, gx)) continue;

            /* Count wall neighbors */
            int walls = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if ((dy || dx) && in_bounds_fully(gy+dy, gx+dx) && !cave_floor_bold(gy+dy, gx+dx))
                        walls++;

            /* Erode edge tiles more aggressively for irregular cave-like edges */
            if (walls >= 3 && rand_int(100) < 45)  /* Increased from one_in_(3) = 33% to 45% */
            {
                cave_set_feat_style(gy, gx, FEAT_WALL_EXTRA, style_idx);
                cave_info[gy][gx] &= ~CAVE_ROOM;
                floor_count--;
            }
        }
    }

    prune_big_cave_detached_components(y1, y2, x1, x2, style_idx);

    /* Recalculate bounds after erosion and detached-pocket cleanup */
    min_y = y2; max_y = y1; min_x = x2; max_x = x1;
    floor_count = 0;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (cave_floor_bold(gy, gx) && (cave_info[gy][gx] & CAVE_ROOM))
            {
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }

    if (floor_count < 40)
        return false;

    /* Add some internal pillars for visual interest */
    int pillar_count = floor_count / 60;
    for (int p = 0; p < pillar_count; ++p)
    {
        for (int tries = 0; tries < 20; ++tries)
        {
            int py = rand_range(min_y + 2, max_y - 2);
            int px = rand_range(min_x + 2, max_x - 2);
            if (cave_floor_bold(py, px))
            {
                bool all_floor = true;
                for (int dy = -1; dy <= 1 && all_floor; ++dy)
                    for (int dx = -1; dx <= 1 && all_floor; ++dx)
                        if (!cave_floor_bold(py + dy, px + dx))
                            all_floor = false;
                if (all_floor)
                {
                    cave_set_feat_style(py, px, FEAT_WALL_EXTRA, style_idx);
                    break;
                }
            }
        }
    }

    /* Set outer walls */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
        }
    }

    /* Pick center - prefer floor tile adjacent to outer wall for tunnel connectivity */
    int cy = (min_y + max_y) / 2, cx = (min_x + max_x) / 2;
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
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, false);

    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);

    log_trace("Big cave anchor: bounds=(%d,%d)-(%d,%d) center=(%d,%d) edge=%d floors=%d pillars=%d",
        min_y, min_x, max_y, max_x, cy, cx, found_edge, floor_count, pillar_count);
    genlog_anchor("BIG_CAVE: bounds=(%d,%d)-(%d,%d), %d floor tiles, %d pillars",
        min_y, min_x, max_y, max_x, floor_count, pillar_count);
    return true;
}

/* Carve a chasm area with organic cave shape and islands connected by bridges */
bool carve_chasm_with_bridges(int y_min, int y_max, int x_min, int x_max,
    int floor_style, int bridge_style)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("CHASM: rejected - room capacity limit reached");
        return false;
    }

    /* Bounds are inclusive. Keep the local mask dimensions aligned with the
     * generation loops so the temporary cave/platform arrays cover every tile. */
    int avail_h = y_max - y_min + 1;
    int avail_w = x_max - x_min + 1;
    if (avail_h < 16 || avail_w < 20)
    {
        genlog_anchor("CHASM: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }

    /* Use variable margins to create organic outer boundary */
    int h = avail_h;
    int w = avail_w;
    int y1 = y_min;
    int x1 = x_min;
    int y2 = y_max;
    int x2 = x_max;

    /* Check area is basic granite */
    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("CHASM: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }

    /*
     * CHASM GENERATION APPROACH:
     * 1. Use CA to create organic cave boundary (not rectangular)
     * 2. Create multiple platform islands within the cave
     * 3. Fill non-platform areas with chasms
     * 4. Connect platforms with narrow bridges
     */

    /* Track what's inside the cave vs wall, and what's platform vs chasm */
    bool* is_cave = mem_alloc_array(h * w, bool);
    bool* is_platform = mem_alloc_array(h * w, bool);
    if (!is_cave || !is_platform)
    {
        if (is_cave) mem_free(is_cave);
        if (is_platform) mem_free(is_platform);
        return false;
    }

    /* Initialize: seed cave shape with multi-center distance + noise */
    int num_cave_centers = 3 + rand_int(3);  /* 3-5 centers for cave shape */
    int cave_cy[6], cave_cx[6];
    for (int c = 0; c < num_cave_centers; ++c)
    {
        cave_cy[c] = rand_range(h / 4, 3 * h / 4);
        cave_cx[c] = rand_range(w / 4, 3 * w / 4);
    }

    /* Carve organic cave shape using distance from centers + noise */
    int base_radius = (h + w) / 5;
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            /* Find distance to nearest center */
            int min_dist = 9999;
            for (int c = 0; c < num_cave_centers; ++c)
            {
                int dy = ABS(ly - cave_cy[c]);
                int dx = ABS(lx - cave_cx[c]);
                int dist = dy + (dx * 2 / 3);  /* Wider horizontally */
                if (dist < min_dist) min_dist = dist;
            }

            /* Cave extends with noise for organic edges */
            int threshold = base_radius + rand_int(base_radius / 2) - rand_int(base_radius / 3);
            is_cave[ly * w + lx] = (min_dist < threshold);
            is_platform[ly * w + lx] = false;
        }
    }

    /* CA smoothing for organic cave boundary */
    bool* next_cave = mem_alloc_array(h * w, bool);
    if (!next_cave) { mem_free(is_cave); mem_free(is_platform); return false; }

    for (int step = 0; step < 3; ++step)
    {
        for (int ly = 0; ly < h; ++ly)
        {
            for (int lx = 0; lx < w; ++lx)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0) continue;
                        int ny = ly + dy, nx = lx + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors += 0;  /* Edges are wall */
                        else if (is_cave[ny * w + nx])
                            neighbors++;
                    }
                }
                /* Cave survives with 4+ neighbors, born with 5+ */
                next_cave[ly * w + lx] = is_cave[ly * w + lx] ? (neighbors >= 4) : (neighbors >= 5);
            }
        }
        for (int i = 0; i < h * w; ++i) is_cave[i] = next_cave[i];
    }
    mem_free(next_cave);

    /* Ensure cave doesn't touch absolute edges */
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (ly < 2 || ly >= h - 2 || lx < 2 || lx >= w - 2)
                is_cave[ly * w + lx] = false;
        }
    }

    /* Now create 5-9 platform islands within the cave area */
    int num_platforms = rand_range(5, 9);
    int plat_cy[10], plat_cx[10], plat_radius[10];
    int platforms_placed = 0;
    int sanctum_cy = -1;
    int sanctum_cx = -1;

    if (!choose_chasm_sanctum_seed(is_cave, h, w, &sanctum_cy, &sanctum_cx))
    {
        mem_free(is_cave);
        mem_free(is_platform);
        genlog_anchor("CHASM: rejected - no buffered central sanctum site");
        return false;
    }

    plat_cy[platforms_placed] = sanctum_cy;
    plat_cx[platforms_placed] = sanctum_cx;
    plat_radius[platforms_placed] = rand_range(3, 4);
    platforms_placed++;

    for (int attempt = 0; attempt < 300 && platforms_placed < num_platforms; ++attempt)
    {
        int py = rand_range(4, h - 5);
        int px = rand_range(5, w - 6);

        /* Must be inside cave */
        if (!is_cave[py * w + px]) continue;

        /* Check distance from other platforms */
        bool too_close = false;
        int min_sep = 5 + rand_int(3);  /* Variable separation */
        for (int i = 0; i < platforms_placed; ++i)
        {
            int dist = ABS(py - plat_cy[i]) + ABS(px - plat_cx[i]);
            if (dist < min_sep)
            {
                too_close = true;
                break;
            }
        }
        if (too_close) continue;

        plat_cy[platforms_placed] = py;
        plat_cx[platforms_placed] = px;
        plat_radius[platforms_placed] = rand_range(2, 4);
        platforms_placed++;
    }

    /* Create organic platform shapes */
    for (int p = 0; p < platforms_placed; ++p)
    {
        int cy = plat_cy[p];
        int cx = plat_cx[p];
        int base_r = plat_radius[p];

        for (int ly = 0; ly < h; ++ly)
        {
            for (int lx = 0; lx < w; ++lx)
            {
                if (!is_cave[ly * w + lx]) continue;

                int dy = ABS(ly - cy);
                int dx = ABS(lx - cx);
                int dist = dy + (dx * 2 / 3);

                int threshold = base_r + rand_int(2);
                if (dist <= threshold)
                    is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Reserve one buffered 5x5 sanctuary on the center-leaning island so the
     * 3x3 sanctum sits away from chasm edges. */
    for (int dy = -2; dy <= 2; ++dy)
    {
        for (int dx = -2; dx <= 2; ++dx)
        {
            int ly = sanctum_cy + dy;
            int lx = sanctum_cx + dx;

            if (ly < 0 || lx < 0 || ly >= h || lx >= w)
                continue;
            if (!is_cave[ly * w + lx])
                continue;

            is_platform[ly * w + lx] = true;
        }
    }

    /* Extend platforms organically */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int ly = 1; ly < h - 1; ++ly)
        {
            for (int lx = 1; lx < w - 1; ++lx)
            {
                if (!is_cave[ly * w + lx]) continue;
                if (is_platform[ly * w + lx]) continue;

                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dy || dx) && is_platform[(ly+dy) * w + (lx+dx)])
                            adj++;

                if (adj >= 3 && one_in_(3))
                    is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Restore the sparse edge nubs that helped the previous bridge layout stay
     * legible without creating a trivial perimeter walkway. */
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (!is_cave[ly * w + lx]) continue;

            bool edge_of_cave = false;
            int adj_platforms = 0;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = ly + dy, nx = lx + dx;
                    if (ny < 0 || nx < 0 || ny >= h || nx >= w
                        || !is_cave[ny * w + nx])
                        edge_of_cave = true;
                    else if (is_platform[ny * w + nx])
                        adj_platforms++;
                }
            }

            if (edge_of_cave && !is_platform[ly * w + lx]
                && adj_platforms >= 2 && one_in_(4))
            {
                is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Apply to cave: inside cave + platform = floor, inside cave + !platform = chasm */
    int chasm_count = 0;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            int ly = gy - y1, lx = gx - x1;

            if (!is_cave[ly * w + lx])
                continue;  /* Leave as granite wall */

            if (is_platform[ly * w + lx])
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, floor_style);
                cave_info[gy][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
            }
            else
            {
                cave_set_feat(gy, gx, FEAT_CHASM);
                cave_info[gy][gx] |= CAVE_CHASM_AREA;
                chasm_count++;
            }
        }
    }

    /* Now connect platforms with bridges (MST-style) */
    int global_plat_y[10], global_plat_x[10];
    for (int p = 0; p < platforms_placed; ++p)
    {
        global_plat_y[p] = y1 + plat_cy[p];
        global_plat_x[p] = x1 + plat_cx[p];
    }

    bool* connected = mem_alloc_array(platforms_placed, bool);
    if (!connected) { mem_free(is_cave); mem_free(is_platform); return false; }
    for (int i = 0; i < platforms_placed; ++i) connected[i] = false;
    if (platforms_placed > 0) connected[0] = true;

    int bridges_built = 0;
    for (int iter = 0; iter < platforms_placed; ++iter)
    {
        int best_from = -1, best_to = -1, best_dist = 9999;

        for (int i = 0; i < platforms_placed; ++i)
        {
            if (!connected[i]) continue;
            for (int j = 0; j < platforms_placed; ++j)
            {
                if (connected[j]) continue;
                int dist = distance(global_plat_y[i], global_plat_x[i],
                                   global_plat_y[j], global_plat_x[j]);
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best_from = i;
                    best_to = j;
                }
            }
        }

        if (best_to < 0) break;

        int sy = global_plat_y[best_from];
        int sx = global_plat_x[best_from];
        int ey = global_plat_y[best_to];
        int ex = global_plat_x[best_to];

        /* L-shaped bridge */
        if (one_in_(2))
        {
            int x_lo = MIN(sx, ex), x_hi = MAX(sx, ex);
            for (int gx = x_lo; gx <= x_hi; ++gx)
                if (in_bounds_fully(sy, gx) && cave_feat[sy][gx] == FEAT_CHASM)
                {
                    cave_set_feat_style(sy, gx, FEAT_FLOOR, bridge_style);
                    cave_info[sy][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
            int y_lo = MIN(sy, ey), y_hi = MAX(sy, ey);
            for (int gy = y_lo; gy <= y_hi; ++gy)
                if (in_bounds_fully(gy, ex) && cave_feat[gy][ex] == FEAT_CHASM)
                {
                    cave_set_feat_style(gy, ex, FEAT_FLOOR, bridge_style);
                    cave_info[gy][ex] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
        }
        else
        {
            int y_lo = MIN(sy, ey), y_hi = MAX(sy, ey);
            for (int gy = y_lo; gy <= y_hi; ++gy)
                if (in_bounds_fully(gy, sx) && cave_feat[gy][sx] == FEAT_CHASM)
                {
                    cave_set_feat_style(gy, sx, FEAT_FLOOR, bridge_style);
                    cave_info[gy][sx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
            int x_lo = MIN(sx, ex), x_hi = MAX(sx, ex);
            for (int gx = x_lo; gx <= x_hi; ++gx)
                if (in_bounds_fully(ey, gx) && cave_feat[ey][gx] == FEAT_CHASM)
                {
                    cave_set_feat_style(ey, gx, FEAT_FLOOR, bridge_style);
                    cave_info[ey][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
        }

        connected[best_to] = true;
        bridges_built++;
    }

    place_chasm_island_sanctum(y1 + sanctum_cy, x1 + sanctum_cx);

    mem_free(connected);
    mem_free(is_cave);
    mem_free(is_platform);

    /* Track bounds of just the floor tiles (not chasm) for proper tunnel connectivity */
    int floor_min_y = y2, floor_max_y = y1, floor_min_x = x2, floor_max_x = x1;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (cave_floor_bold(gy, gx))
            {
                if (gy < floor_min_y) floor_min_y = gy;
                if (gy > floor_max_y) floor_max_y = gy;
                if (gx < floor_min_x) floor_min_x = gx;
                if (gx > floor_max_x) floor_max_x = gx;
            }
        }
    }

    /* Set outer walls ONLY around floor tiles (not chasm) for proper tunnel connectivity */
    for (int gy = floor_min_y - 1; gy <= floor_max_y + 1; ++gy)
    {
        for (int gx = floor_min_x - 1; gx <= floor_max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            if (cave_feat[gy][gx] == FEAT_CHASM) continue;  /* Don't convert chasm */
            if (cave_feat[gy][gx] != FEAT_WALL_EXTRA) continue;

            /* Only set outer wall if bordering actual floor (not chasm) */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx) &&
                        (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, floor_style);
        }
    }

    /* Find center on a floor tile near an outer wall (better for tunnel connectivity) */
    int cy = (floor_min_y + floor_max_y) / 2;
    int cx = (floor_min_x + floor_max_x) / 2;

    /* First try: find floor tile adjacent to outer wall */
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(floor_min_y, floor_max_y);
        int tx = rand_range(floor_min_x, floor_max_x);
        if (!cave_floor_bold(ty, tx)) continue;

        /* Check if adjacent to outer wall */
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
            int ty = rand_range(floor_min_y, floor_max_y);
            int tx = rand_range(floor_min_x, floor_max_x);
            if (cave_floor_bold(ty, tx))
            {
                cy = ty; cx = tx;
                break;
            }
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    /* Use floor bounds, not full chasm bounds, for tunnel connectivity */
    dun->corner[idx].y1 = floor_min_y;
    dun->corner[idx].x1 = floor_min_x;
    dun->corner[idx].y2 = floor_max_y;
    dun->corner[idx].x2 = floor_max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, false);

    log_trace("Chasm organic: %d platforms, %d bridges, %d chasm tiles, floor=(%d,%d)-(%d,%d) center=(%d,%d)",
        platforms_placed, bridges_built, chasm_count, floor_min_y, floor_min_x, floor_max_y, floor_max_x, cy, cx);
    log_trace("Chasm organic extras: sanctum=(%d,%d)",
        y1 + sanctum_cy, x1 + sanctum_cx);
    genlog_anchor("CHASM: %d platforms, %d bridges, %d chasm tiles at (%d,%d)-(%d,%d)",
        platforms_placed, bridges_built, chasm_count, floor_min_y, floor_min_x, floor_max_y, floor_max_x);
    return true;
}

/* Carve a labyrinth-style maze with organic shape using cellular automata */
bool carve_labyrinth_bounds(int y_min, int y_max, int x_min, int x_max,
    density_level_t density, int style_idx)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("LABYRINTH: rejected - room capacity limit reached");
        return false;
    }

    int avail_h = y_max - y_min;
    int avail_w = x_max - x_min;
    if (avail_h < 10 || avail_w < 12)
    {
        genlog_anchor("LABYRINTH: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }

    /* Use small margins to maximize labyrinth size while avoiding partition overlap */
    int margin_y = rand_range(3, 5);
    int margin_x = rand_range(3, 5);
    int y1 = y_min + margin_y;
    int x1 = x_min + margin_x;
    int y2 = y_max - margin_y;
    int x2 = x_max - margin_x;
    int h = y2 - y1 + 1;
    int w = x2 - x1 + 1;

    if (h < 8 || w < 10)
    {
        genlog_anchor("LABYRINTH: rejected - after margins too small: h=%d w=%d (margins y=%d x=%d)",
                      h, w, margin_y, margin_x);
        return false;
    }

    /* Check area is basic granite - if floor exists, another partition already carved here */
    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("LABYRINTH: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }

    /* Use CA to create organic boundary mask - no size caps, use full partition */
    /* Note: h and w already set from margins above, keep them as-is */

    bool* mask = mem_alloc_array(h * w, bool);
    if (!mask) return false;

    /* Seed with 60% fill for corridors */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            mask[y * w + x] = (rand_int(100) < 60);

    /* CA smoothing to create organic boundary */
    bool* next = mem_alloc_array(h * w, bool);
    if (!next) { mem_free(mask); return false; }

    for (int step = 0; step < 3; ++step)
    {
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0) continue;
                        int ny = y + dy, nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (mask[ny * w + nx])
                            neighbors++;
                    }
                next[y * w + x] = (neighbors >= 4);
            }
        }
        for (int i = 0; i < h * w; ++i) mask[i] = next[i];
    }
    mem_free(next);

    /* Carve corridors in a grid pattern, but only within the organic mask */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    /* Vary corridor spacing by density: sparse=4 (open), normal=3, dense=2 (tight maze) */
    int corridor_spacing = (density == DENSITY_SPARSE) ? 4 : (density == DENSITY_DENSE) ? 2 : 3;

    /* Horizontal corridors */
    for (int ly = 1; ly < h - 1; ly += corridor_spacing)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (!mask[ly * w + lx]) continue;
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (in_bounds_fully(gy, gx) && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }

    /* Vertical corridors */
    for (int lx = 1; lx < w - 1; lx += corridor_spacing)
    {
        for (int ly = 0; ly < h; ++ly)
        {
            if (!mask[ly * w + lx]) continue;
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (in_bounds_fully(gy, gx) && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }

    mem_free(mask);

    /* Block some corridor segments to create dead ends */
    for (int ly = 1; ly < h - 1; ly += corridor_spacing)
    {
        for (int lx = 1; lx < w - 1; lx += corridor_spacing)
        {
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (!in_bounds_fully(gy, gx) || !cave_floor_bold(gy, gx))
                continue;

            if (rand_int(100) < 45)
            {
                int block_dir = rand_int(4);
                int dy = (block_dir == 0) ? -1 : (block_dir == 1) ? 1 : 0;
                int dx = (block_dir == 2) ? -1 : (block_dir == 3) ? 1 : 0;

                for (int step = 1; step < corridor_spacing; ++step)
                {
                    int ny = gy + dy * step;
                    int nx = gx + dx * step;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                    {
                        cave_set_feat_style(ny, nx, FEAT_WALL_EXTRA, style_idx);
                        cave_info[ny][nx] &= ~CAVE_ROOM;
                        floor_count--;
                    }
                }
            }
        }
    }

    /* Add chambers at some intersections */
    int chamber_count = rand_range(2, 5);
    for (int c = 0; c < chamber_count; ++c)
    {
        int cy = rand_range(min_y + 2, max_y - 2);
        int cx = rand_range(min_x + 2, max_x - 2);
        if (!cave_floor_bold(cy, cx)) continue;

        int ch_h = rand_range(2, 4);
        int ch_w = rand_range(2, 5);

        for (int dy = -ch_h; dy <= ch_h; ++dy)
        {
            for (int dx = -ch_w; dx <= ch_w; ++dx)
            {
                int ty = cy + dy;
                int tx = cx + dx;
                if (!in_bounds_fully(ty, tx)) continue;
                if (cave_feat[ty][tx] != FEAT_WALL_EXTRA) continue;

                cave_set_feat_style(ty, tx, FEAT_FLOOR, style_idx);
                cave_info[ty][tx] |= CAVE_ROOM;
                floor_count++;
                if (ty < min_y) min_y = ty;
                if (ty > max_y) max_y = ty;
                if (tx < min_x) min_x = tx;
                if (tx > max_x) max_x = tx;
            }
        }
    }

    if (floor_count < 25)
        return false;

    /* Set outer walls */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
        }
    }

    /* Pick center - prefer floor tile adjacent to outer wall for tunnel connectivity */
    int center_y = (min_y + max_y) / 2, center_x = (min_x + max_x) / 2;
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
                    center_y = ty; center_x = tx;
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
                center_y = ty; center_x = tx; break;
            }
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = center_y;
    dun->cent[idx].x = center_x;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, false);

    /* === LABYRINTH STAIR PLACEMENT === */
    /* Place 1-2 stairs inside the labyrinth for navigation */
    int lab_stairs = 1 + (floor_count > 60 ? 1 : 0);
    int stairs_placed = 0;

    for (int s = 0; s < lab_stairs; ++s)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int sy = rand_range(min_y, max_y);
            int sx = rand_range(min_x, max_x);
            if (!in_bounds_fully(sy, sx)) continue;
            if (!cave_naked_bold(sy, sx)) continue;
            if (!cave_floor_bold(sy, sx)) continue;

            /* Avoid placing next to doors */
            if (cave_feat[sy - 1][sx] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy + 1][sx] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy][sx - 1] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy][sx + 1] == FEAT_DOOR_HEAD) continue;

            /* Alternate between up and down stairs */
            int feat = (s % 2 == 0) ? FEAT_MORE : FEAT_LESS;

            /* At surface, only down; at Morgoth depth, only up */
            if (p_ptr->depth == 0) feat = FEAT_MORE;
            else if (p_ptr->depth >= MORGOTH_DEPTH) feat = FEAT_LESS;

            cave_set_feat(sy, sx, feat);
            stairs_placed++;
            break;
        }
    }

    log_trace("Labyrinth anchor (organic): bounds=(%d,%d)-(%d,%d) center=(%d,%d) edge=%d floors=%d chambers=%d stairs=%d",
        min_y, min_x, max_y, max_x, center_y, center_x, found_edge, floor_count, chamber_count, stairs_placed);
    genlog_anchor("LABYRINTH: bounds=(%d,%d)-(%d,%d), %d floor tiles, %d chambers, %d stairs",
        min_y, min_x, max_y, max_x, floor_count, chamber_count, stairs_placed);
    return true;
}
