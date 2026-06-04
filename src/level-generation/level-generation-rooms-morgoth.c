/* File: level-generation-rooms-morgoth.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

void carve_morgoth_entry_tunnels(const vault_type* v_ptr, int y0, int x0)
{
    if (!v_ptr)
        return;

    int top_y = y0 - v_ptr->hgt / 2;
    int left_x = x0 - v_ptr->wid / 2;
    int right_x = left_x + v_ptr->wid - 1;

    /* Collect contiguous '$' runs on the top row (stored as FEAT_WALL_OUTER) */
    int seg_start[4];
    int seg_end[4];
    int segs = 0;

    for (int x = left_x; x <= right_x; x++)
    {
        if (cave_feat[top_y][x] == FEAT_WALL_OUTER)
        {
            if (segs == 0 || x != seg_end[segs - 1] + 1)
            {
                if (segs >= 4)
                    break;
                seg_start[segs] = seg_end[segs] = x;
                segs++;
            }
            else
            {
                seg_end[segs - 1] = x;
            }
        }
    }

    if (segs == 0)
        return;

    int tunnel_limit = morgoth_partition_reserved ? morgoth_partition_bounds.y1 - 2 : top_y - 20;
    if (tunnel_limit < 1)
        tunnel_limit = 1;
    if (tunnel_limit > top_y)
        tunnel_limit = top_y;

    /* Track which segments have joined independently */
    bool seg_joined[4] = {false, false, false, false};

    for (int s = 0; s < segs; s++)
    {
        int x1 = seg_start[s];
        int x2 = seg_end[s];

        /* Place forced closed doors in the vault's outer wall (end of corridor) */
        for (int x = x1; x <= x2; x++)
        {
            if (!in_bounds_fully(top_y, x))
                continue;
            cave_set_feat(top_y, x, FEAT_DOOR_HEAD + 0x00);
        }

        /* Carve a tunnel northwards from just outside the doors */
        for (int y = top_y - 1; y >= tunnel_limit; y--)
        {
            /* Skip if this segment already joined */
            if (seg_joined[s])
                break;

            bool this_seg_joined = false;

            for (int x = x1; x <= x2; x++)
            {
                if (!in_bounds_fully(y, x))
                    continue;

                /* Stop this segment once it reaches existing open floor outside the reserved region */
                if (!morgoth_region_active() || !coord_in_morgoth_region(y, x, 0))
                {
                    if (cave_floor_bold(y, x) && !(cave_info[y][x] & CAVE_ICKY))
                    {
                        this_seg_joined = true;
                        continue;
                    }
                }

                if (cave_feat[y][x] == FEAT_WALL_PERM)
                    continue;

                cave_set_feat(y, x, FEAT_FLOOR);
                cave_info[y][x] &= ~(CAVE_G_VAULT | CAVE_ICKY);
                cave_info[y][x] |= CAVE_MORGOTH_TUNNEL;
            }

            if (this_seg_joined)
            {
                seg_joined[s] = true;
                break;
            }
        }
    }
}

/* Extend the carved entry tunnels so both connect to the main level. */
bool morgoth_tunnel_traversable(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_WALL_PERM)
        return false;
    if ((cave_info[y][x] & CAVE_ICKY) && !coord_in_morgoth_region(y, x, 0))
        return false;
    return true;
}

bool morgoth_tunnel_target(int y, int x)
{
    if (coord_in_morgoth_region(y, x, 0))
        return false;
    if (cave_info[y][x] & CAVE_ICKY)
        return false;
    return player_passable(y, x, true);
}

bool connect_morgoth_tunnel_component(int start_y, int start_x)
{
    static int prev[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int head = 0;
    int tail = 0;
    int found_y = -1;
    int found_x = -1;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
            prev[y][x] = -1;

    int start_idx = start_y * MAX_DUNGEON_WID + start_x;
    prev[start_y][start_x] = start_idx;
    queue[tail++] = start_idx;

    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};

    while (head < tail)
    {
        int cur = queue[head++];
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        for (int d = 0; d < 4; ++d)
        {
            int ny = cy + ddy4[d];
            int nx = cx + ddx4[d];
            if (!in_bounds_fully(ny, nx))
                continue;
            if (prev[ny][nx] != -1)
                continue;
            if (!morgoth_tunnel_traversable(ny, nx))
                continue;

            int nidx = ny * MAX_DUNGEON_WID + nx;
            prev[ny][nx] = cur;
            if (tail < (int)N_ELEMENTS(queue))
                queue[tail++] = nidx;

            if (morgoth_tunnel_target(ny, nx))
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

    int cur = found_y * MAX_DUNGEON_WID + found_x;
    int safety = 0;
    while (safety++ < (int)N_ELEMENTS(queue))
    {
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        if (cave_feat[cy][cx] != FEAT_WALL_PERM)
        {
            if (!cave_floor_bold(cy, cx)
                && (cave_feat[cy][cx] < FEAT_DOOR_HEAD
                    || cave_feat[cy][cx] > FEAT_DOOR_TAIL))
            {
                cave_set_feat(cy, cx, FEAT_FLOOR);
            }

            if (coord_in_morgoth_region(cy, cx, 0))
                cave_info[cy][cx] |= CAVE_MORGOTH_TUNNEL;
        }

        if (cur == prev[start_y][start_x])
            break;
        int p = prev[cy][cx];
        if (p == cur)
            break;
        cur = p;
        if (cur == start_idx)
            break;
    }

    return true;
}
