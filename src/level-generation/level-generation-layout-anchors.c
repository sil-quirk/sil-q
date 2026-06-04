/* File: level-generation-layout-anchors.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

#if 0
void seed_ca_blob_anchors(void);
void seed_bsp_slice_anchors(void);
void ensure_partition_connectivity(void);
#endif

/* Attempt to place a prefab vault/room as a generation anchor */
bool place_prefab_anchor_of_type(int typ, bool require_neighbor)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        return false;
    }

    int y = rand_range(5, p_ptr->cur_map_hgt - 5);
    int x = rand_range(5, p_ptr->cur_map_wid - 5);
    int before = dun->cent_n;
    bool ok = false;

    switch (typ)
    {
    case 8:
        ok = build_type8(y, x);
        break;
    case 7:
        ok = build_type7(y, x);
        break;
    case 6:
    default:
        ok = build_type6(y, x, false);
        break;
    }

    if (!ok || dun->cent_n <= before)
    {
        return false;
    }

    int slot = dun->cent_n - 1;
    mark_room_anchor_meta(slot, LAYOUT_ANCHOR_PREFAB, require_neighbor);
    return true;
}

/* Seed a small number of prefab anchors up-front to diversify layouts */
void seed_prefab_anchors(void)
{
    int target = 1;
    if (p_ptr->depth >= 15)
        target++;
    if (p_ptr->depth >= 30 && one_in_(2))
        target++;

    int placed = 0;
    int attempts = 0;
    int max_attempts = target * 6;

    while (placed < target && attempts < max_attempts)
    {
        attempts++;
        /* Bias deeper levels toward larger prefabs, shallow toward type6 */
        int roll = rand_int(100);
        int typ = 6;
        if (roll > 85 && p_ptr->depth > 25)
            typ = 8;
        else if (roll > 60 && p_ptr->depth > 10)
            typ = 7;

        /* Reserve some anchors for future adjacency setpieces */
        bool require_neighbor = one_in_(3);

        if (place_prefab_anchor_of_type(typ, require_neighbor))
        {
            placed++;
            log_trace("Prefab anchor: placed type %d (require_neighbor=%s) [placed=%d target=%d attempts=%d]",
                typ, require_neighbor ? "true" : "false", placed, target, attempts);
        }
    }

    log_trace("Prefab anchor seeding complete: placed=%d target=%d attempts=%d depth=%d",
        placed, target, attempts, p_ptr->depth);
}
#if 0
/* Try to seed a few CA blob anchors in unused granite */
void seed_ca_blob_anchors(void)
{
    /* Scale CA blobs by map size to add connective floor on big levels */
    int panels = (p_ptr->cur_map_hgt / PANEL_HGT);
    int target = 1 + panels / 3; /* e.g., 9 panels -> 4 blobs */
    if (target > 4) target = 4;
    int placed = 0;
    int max_attempts = target * 8;
    for (int attempt = 0; attempt < max_attempts && placed < target; ++attempt)
    {
        if (carve_ca_blob_anchor())
            placed++;
    }
    log_trace("CA blob seeding complete: placed=%d target=%d attempts=%d", placed, target, max_attempts);
}
#endif

/* Carve a BSP-style sliced region into rooms-like rectangles and register anchor */
#if 0
bool carve_bsp_slice_anchor(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;

    int h = rand_range(10, 18);
    int w = rand_range(12, 24);
    int y1 = rand_range(3, p_ptr->cur_map_hgt - h - 3);
    int x1 = rand_range(3, p_ptr->cur_map_wid - w - 3);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (!area_is_basic_granite(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
        return false;

    typedef struct {
        int y1, x1, y2, x2;
    } slice_rect;

    slice_rect rects[12];
    int rect_count = 1;
    rects[0].y1 = y1;
    rects[0].x1 = x1;
    rects[0].y2 = y2;
    rects[0].x2 = x2;

    int splits = rand_range(2, 4);
    for (int s = 0; s < splits && rect_count < 12; ++s)
    {
        int pick = rand_int(rect_count);
        slice_rect r = rects[pick];
        int rw = r.x2 - r.x1 + 1;
        int rh = r.y2 - r.y1 + 1;
        bool vertical = (rw > rh) ? true : (rh > rw ? false : one_in_(2));

        if (vertical && rw > 10)
        {
            int cut = rand_range(r.x1 + rw / 3, r.x2 - rw / 3);
            slice_rect a = {r.y1, r.x1, r.y2, cut};
            slice_rect b = {r.y1, cut + 1, r.y2, r.x2};
            if ((a.x2 - a.x1) >= 5 && (b.x2 - b.x1) >= 5)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
        else if (!vertical && rh > 8)
        {
            int cut = rand_range(r.y1 + rh / 3, r.y2 - rh / 3);
            slice_rect a = {r.y1, r.x1, cut, r.x2};
            slice_rect b = {cut + 1, r.x1, r.y2, r.x2};
            if ((a.y2 - a.y1) >= 4 && (b.y2 - b.y1) >= 4)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
    }

    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    int floor_count = 0;
    for (int i = 0; i < rect_count; ++i)
    {
        slice_rect *r = &rects[i];
        for (int y = r->y1 + 1; y < r->y2; ++y)
        {
            for (int x = r->x1 + 1; x < r->x2; ++x)
            {
                cave_set_feat(y, x, FEAT_FLOOR);
                cave_info[y][x] |= CAVE_ROOM;
                floor_count++;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }

    if (floor_count < 20)
        return false;

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

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, one_in_(4));

    log_trace("BSP slice anchor: carved floor_count=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d) rects=%d",
        floor_count, min_y, min_x, max_y, max_x, cy, cx, rect_count);
    return true;
    return true;
}
#endif

bool carve_bsp_slice_anchor_bounds(int y_min, int y_max, int x_min, int x_max)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;
    if (y_max - y_min < 8 || x_max - x_min < 10)
        return false;

    int h = rand_range(8, MIN(16, y_max - y_min));
    int w = rand_range(10, MIN(20, x_max - x_min));
    int y1 = rand_range(y_min + 1, y_max - h);
    int x1 = rand_range(x_min + 1, x_max - w);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (!area_is_basic_granite(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
        return false;

    typedef struct { int y1, x1, y2, x2; } slice_rect;
    slice_rect rects[10];
    int rect_count = 1;
    rects[0] = (slice_rect){y1, x1, y2, x2};

    int splits = rand_range(1, 3);
    for (int s = 0; s < splits && rect_count < 10; ++s)
    {
        int pick = rand_int(rect_count);
        slice_rect r = rects[pick];
        int rw = r.x2 - r.x1 + 1;
        int rh = r.y2 - r.y1 + 1;
        bool vertical = (rw > rh) ? true : (rh > rw ? false : one_in_(2));
        if (vertical && rw > 8)
        {
            int cut = rand_range(r.x1 + rw / 3, r.x2 - rw / 3);
            slice_rect a = {r.y1, r.x1, r.y2, cut};
            slice_rect b = {r.y1, cut + 1, r.y2, r.x2};
            if ((a.x2 - a.x1) >= 4 && (b.x2 - b.x1) >= 4)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
        else if (!vertical && rh > 6)
        {
            int cut = rand_range(r.y1 + rh / 3, r.y2 - rh / 3);
            slice_rect a = {r.y1, r.x1, cut, r.x2};
            slice_rect b = {cut + 1, r.x1, r.y2, r.x2};
            if ((a.y2 - a.y1) >= 3 && (b.y2 - b.y1) >= 3)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
    }

    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    int floor_count = 0;
    for (int i = 0; i < rect_count; ++i)
    {
        slice_rect *r = &rects[i];
        for (int y = r->y1 + 1; y < r->y2; ++y)
        {
            for (int x = r->x1 + 1; x < r->x2; ++x)
            {
                cave_set_feat(y, x, FEAT_FLOOR);
                cave_info[y][x] |= CAVE_ROOM;
                floor_count++;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }
    if (floor_count < 15)
        return false;

    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty; cx = tx; break;
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
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
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
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, one_in_(5));
    log_trace("BSP slice (bounded) anchor: bounds=(%d,%d)-(%d,%d) center=(%d,%d) floor=%d rects=%d",
        min_y, min_x, max_y, max_x, cy, cx, floor_count, rect_count);
    return true;
}

#if 0
/* Try to seed BSP-sliced anchors in spare granite */
void seed_bsp_slice_anchors(void)
{
    int target = (p_ptr->depth >= 8) ? 1 : 0;
    if (p_ptr->depth >= 20)
        target++;
    int placed = 0;
    for (int attempt = 0; attempt < 16 && placed < target; ++attempt)
    {
        if (carve_bsp_slice_anchor())
            placed++;
    }
    log_trace("BSP slice seeding complete: placed=%d target=%d", placed, target);
}
#endif
