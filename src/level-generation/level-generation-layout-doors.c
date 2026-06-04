/* File: level-generation-layout-doors.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

void repair_all_outer_walls(void)
{
    int repaired = 0;

    /* Scan entire map for wall tiles that border CAVE_ROOM floor */
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            /* Skip if already floor or already outer wall */
            if (cave_floor_bold(y, x))
                continue;
            if (cave_feat[y][x] == FEAT_WALL_OUTER)
                continue;
            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                continue;

            /* Check if this wall borders any CAVE_ROOM floor */
            bool borders_room_floor = false;
            for (int dy = -1; dy <= 1 && !borders_room_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_room_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = y + dy, nx = x + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_room_floor = true;
                    }
                }
            }

            if (borders_room_floor)
            {
                cave_set_feat(y, x, FEAT_WALL_OUTER);
                repaired++;
            }
        }
    }

    if (repaired > 0)
    {
        log_trace("repair_all_outer_walls: converted %d WALL_EXTRA to WALL_OUTER", repaired);
    }
}

/* Fallback builder to guarantee the minimum room count before connectivity work */
void ensure_minimum_rooms(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return;
    if (dun->cent_n >= ROOM_MIN)
        return;

    int before = dun->cent_n;
    /* Try a mix of simple rooms near the centre to avoid hard failures */
    for (int attempt = 0; attempt < 50 && dun->cent_n < ROOM_MIN && dun->cent_n < room_capacity_limit(); ++attempt)
    {
        int y = rand_range(4, p_ptr->cur_map_hgt - 4);
        int x = rand_range(4, p_ptr->cur_map_wid - 4);

        /* Alternate basic shapes to improve odds in cramped layouts */
        if (attempt % 3 == 0)
            build_type1(y, x);
        else if (attempt % 3 == 1)
            build_type2(y, x);
        else
            build_type6(y, x, false);
    }

    if (dun->cent_n > before)
    {
        log_trace("Room fallback: added %d emergency rooms (now %d)", dun->cent_n - before, dun->cent_n);
    }
}

bool feature_is_any_door(int feat)
{
    return (feat == FEAT_SECRET) || (feat == FEAT_OPEN) || (feat == FEAT_BROKEN)
        || ((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_DOOR_TAIL));
}

bool doorway_neighbor_open(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;

    if (cave_impassable_bold(y, x))
        return false;

    return !feature_is_any_door(cave_feat[y][x]);
}

bool doorway_neighbor_blocked(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return true;

    return cave_impassable_bold(y, x);
}

/* Keep non-vault doors only when they still sit in a real doorway:
 * open on one axis, blocked on the perpendicular axis. */
bool doorway_geometry_ok(int y, int x)
{
    bool north_open = doorway_neighbor_open(y - 1, x);
    bool south_open = doorway_neighbor_open(y + 1, x);
    bool west_open = doorway_neighbor_open(y, x - 1);
    bool east_open = doorway_neighbor_open(y, x + 1);

    bool vertical_open = north_open && south_open;
    bool horizontal_open = west_open && east_open;
    bool vertical_blocked = doorway_neighbor_blocked(y - 1, x)
        && doorway_neighbor_blocked(y + 1, x);
    bool horizontal_blocked = doorway_neighbor_blocked(y, x - 1)
        && doorway_neighbor_blocked(y, x + 1);

    return (vertical_open && horizontal_blocked)
        || (horizontal_open && vertical_blocked);
}

/* Organic cave/blob anchors should meet corridors as open floor, not doors.
 * Chasm anchors reuse the same kind marker for shaping, so exclude them. */
bool room_prefers_floor_thresholds(int room_idx)
{
    int cy, cx;

    if (room_idx < 0 || room_idx >= dun->cent_n || room_idx >= CENT_MAX)
        return false;
    if (room_anchor_kind[room_idx] != LAYOUT_ANCHOR_CA_BLOB)
        return false;

    cy = dun->cent[room_idx].y;
    cx = dun->cent[room_idx].x;
    if (!in_bounds_fully(cy, cx))
        return false;

    return ((cave_info[cy][cx] & CAVE_CHASM_AREA) == 0);
}

bool tunnel_prefers_floor_thresholds(int r1, int r2)
{
    return room_prefers_floor_thresholds(r1)
        || room_prefers_floor_thresholds(r2);
}

void carve_floor_threshold(
    int y, int x, int r1, int r2, bool mark_escape)
{
    cave_set_feat(y, x, FEAT_FLOOR);
    cave_corridor1[y][x] = r1;
    cave_corridor2[y][x] = r2;
    if (mark_escape)
        mark_generation_escape_tunnel(y, x);
}

/* Collapse adjacent doors outside vaults to avoid double-door seams */
int squash_double_doors(void)
{
    int removed = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            if (!feature_is_any_door(cave_feat[y][x])) continue;
            if (cave_info[y][x] & (CAVE_ICKY)) continue;

            /* Only clear east/south neighbors to keep at most one door */
            int ny = y, nx = x + 1;
            if ((nx < p_ptr->cur_map_wid - 1) &&
                !(cave_info[ny][nx] & (CAVE_ICKY)) &&
                feature_is_any_door(cave_feat[ny][nx]))
            {
                cave_set_feat(ny, nx, FEAT_FLOOR);
                removed++;
            }
            ny = y + 1; nx = x;
            if ((ny < p_ptr->cur_map_hgt - 1) &&
                !(cave_info[ny][nx] & (CAVE_ICKY)) &&
                feature_is_any_door(cave_feat[ny][nx]))
            {
                cave_set_feat(ny, nx, FEAT_FLOOR);
                removed++;
            }
        }
    }
    log_trace("squash_double_doors: converted %d adjacent doors to floor", removed);
    return removed;
}

int prune_invalid_nonvault_doors(void)
{
    int removed = 0;

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            if (!feature_is_any_door(cave_feat[y][x])) continue;
            if (cave_info[y][x] & (CAVE_ICKY)) continue;
            if (doorway_geometry_ok(y, x)) continue;

            cave_set_feat(y, x, FEAT_FLOOR);
            removed++;
        }
    }

    log_trace("prune_invalid_nonvault_doors: converted %d malformed doors to floor", removed);
    return removed;
}
