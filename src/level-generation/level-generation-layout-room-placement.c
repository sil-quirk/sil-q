/* File: level-generation-layout-room-placement.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

bool room_build_in_bounds(int typ, int y1, int y2, int x1, int x2)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;
    if (y2 - y1 < 6 || x2 - x1 < 8)
        return false;

    int y = rand_range(MAX(5, y1 + 3), MIN(p_ptr->cur_map_hgt - 5, y2 - 3));
    int x = rand_range(MAX(5, x1 + 3), MIN(p_ptr->cur_map_wid - 5, x2 - 3));

    switch (typ)
    {
    case 8: return build_type8(y, x);
    case 7: return build_type7(y, x);
    case 6: return build_type6(y, x, false);
    case 2: return build_type2(y, x);
    case 1: return build_type1(y, x);
    default: return false;
    }
}

/* Place rooms in randomized order within a partition */
void place_rooms_randomized(int y1, int y2, int x1, int x2, int depth,
                                   int t1_count, int t2_count, int t6_count, int t7_count,
                                   int *budget_t6, int *budget_t7, int *budget_t8,
                                   int *used_t6, int *used_t7, int *used_t8)
{
    /* Build an array of all room placements needed */
    int total = t1_count + t2_count + t6_count + t7_count;
    if (total <= 0) return;
    if (total > 50) total = 50;  /* Safety cap */

    int room_types[50];
    int idx = 0;
    for (int i = 0; i < t1_count && idx < 50; ++i) room_types[idx++] = 1;
    for (int i = 0; i < t2_count && idx < 50; ++i) room_types[idx++] = 2;
    for (int i = 0; i < t6_count && idx < 50; ++i) room_types[idx++] = 6;
    for (int i = 0; i < t7_count && idx < 50; ++i) room_types[idx++] = 7;

    /* Fisher-Yates shuffle */
    for (int i = total - 1; i > 0; --i)
    {
        int j = rand_int(i + 1);
        int temp = room_types[i];
        room_types[i] = room_types[j];
        room_types[j] = temp;
    }

    /* Place rooms in shuffled order */
    for (int i = 0; i < total; ++i)
    {
        int typ = room_types[i];
        int priority = (typ >= 6) ? 3 : 2;
        place_room_with_budget(typ, y1, y2, x1, x2, priority, depth,
                               budget_t6, budget_t7, budget_t8,
                               used_t6, used_t7, used_t8);
    }
}

/* Smallest depth at which a non-quest greater vault can appear */
int min_nonquest_gv_depth(void)
{
    static int cached_min_depth = -1;
    if (cached_min_depth >= 0)
        return cached_min_depth;

    int min_depth = 127; /* high sentinel */
    for (int i = 0; i < z_info->v_max; ++i)
    {
        vault_type *v_ptr = &v_info[i];
        if (v_ptr->typ != 8)
            continue;
        if (v_ptr->flags & VLT_QUEST)
            continue;
        if (v_ptr->depth < min_depth)
            min_depth = v_ptr->depth;
    }

    /* Fallback to old gating depth if no candidates are present */
    if (min_depth == 127)
        min_depth = 15;

    cached_min_depth = min_depth;
    return cached_min_depth;
}

int vault_type8_generation_rarity(const vault_type* v_ptr, int depth)
{
    int rarity = v_ptr->rarity;

    if ((depth >= 6) && (v_ptr->flags & (VLT_SURFACE)))
    {
        rarity += (1 << depth);
    }

    return rarity;
}

bool quest_vault_surface_roll_allows(const vault_type* v_ptr, int depth)
{
    if (v_ptr->typ == 6)
    {
        if (depth < 6)
        {
            if (!(v_ptr->flags & (VLT_SURFACE)) && !one_in_(4))
                return false;
        }
        else if (v_ptr->flags & (VLT_SURFACE))
        {
            if (!one_in_(1 << depth))
                return false;
        }
    }
    else if ((depth >= 6) && (v_ptr->flags & (VLT_SURFACE)))
    {
        if (!one_in_(1 << depth))
            return false;
    }

    return true;
}

/* Roll whether this level should reserve a greater vault slot based on vault rarities */
bool gv_level_roll_allows(int depth, int *out_candidates)
{
    int candidate_count = 0;
    bool passed = false;

    for (int i = 0; i < z_info->v_max; ++i)
    {
        vault_type *v_ptr = &v_info[i];
        if (v_ptr->typ != 8) continue;
        if (v_ptr->flags & VLT_QUEST) continue;
        if (v_ptr->depth > depth) continue;
        if (v_ptr->max_depth != 0 && depth > v_ptr->max_depth) continue;

        /* Skip already-used greater vaults to mirror build_type8 checks */
        bool repeated = false;
        for (int j = 0; j < MAX_GREATER_VAULTS; ++j)
        {
            if (p_ptr->greater_vaults[j] == i)
            {
                repeated = true;
                break;
            }
        }
        if (repeated) continue;

        candidate_count++;
        if (!passed && one_in_(vault_type8_generation_rarity(v_ptr, depth)))
        {
            passed = true;
        }
    }

    if (out_candidates) *out_candidates = candidate_count;

    if (candidate_count == 0)
    {
        genlog_partition("GV roll: depth=%d -> no eligible type8 templates (used or quest-only)", depth);
        return false;
    }

    if (passed)
    {
        genlog_partition("GV roll: depth=%d candidates=%d -> PASS (reserve GV this level)", depth, candidate_count);
    }
    else
    {
        genlog_partition("GV roll: depth=%d candidates=%d -> FAIL (no GV this level)", depth, candidate_count);
    }

    return passed;
}

/* Check whether a partition is fully interior (no map-border contact) */
bool partition_is_interior(int row, int col, int rows, int cols)
{
    return (row > 0) && (row < rows - 1) && (col > 0) && (col < cols - 1);
}

bool generation_escape_tunnel_bold(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;

    return cave_escape_tunnel[y][x];
}

void mark_generation_escape_tunnel(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return;

    cave_escape_tunnel[y][x] = true;
}

/* Pick the partition whose centre is closest to the map centre, preferring interior slots */
int choose_central_partition_index(int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        return -1;

    int best_idx = -1;
    int best_score = 1 << 30;
    int map_cy = p_ptr->cur_map_hgt / 2;
    int map_cx = p_ptr->cur_map_wid / 2;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int pi = row * cols + col;
            int y1, y2, x1, x2;
            if (!compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
                continue;

            int cy = (y1 + y2) / 2;
            int cx = (x1 + x2) / 2;
            int dist = distance(map_cy, map_cx, cy, cx);
            int penalty = partition_is_interior(row, col, rows, cols) ? 0 : 10000;
            int score = dist + penalty;

            if (score < best_score)
            {
                best_score = score;
                best_idx = pi;
            }
        }
    }

    return best_idx;
}

/* Try to drop a greater vault inside the provided partition bounds */
bool place_gv_in_partition(int y1, int y2, int x1, int x2, int *budget_t8, int *used_t8)
{
    if (!budget_t8 || *budget_t8 <= 0)
        return false;

    if (dun->cent_n >= room_capacity_limit())
        return false;
    if (y2 - y1 < 6 || x2 - x1 < 8)
        return false;

    /* Can only have one greater vault per level */
    if (g_vault_name[0] != '\0')
        return false;

    bool placed = false;
    for (int attempt = 0; attempt < 3 && !placed; ++attempt)
    {
        int cy = rand_range(MAX(5, y1 + 3), MIN(p_ptr->cur_map_hgt - 5, y2 - 3));
        int cx = rand_range(MAX(5, x1 + 3), MIN(p_ptr->cur_map_wid - 5, x2 - 3));
        placed = build_reserved_type8(cy, cx);
    }

    if (!placed)
    {
        int scan_y1 = MAX(5, y1 + 3);
        int scan_y2 = MIN(p_ptr->cur_map_hgt - 5, y2 - 3);
        int scan_x1 = MAX(5, x1 + 3);
        int scan_x2 = MIN(p_ptr->cur_map_wid - 5, x2 - 3);

        if (scan_y1 <= scan_y2 && scan_x1 <= scan_x2)
        {
            log_trace("Greater vault: random partition placement missed, scanning bounds (%d,%d)-(%d,%d)",
                y1, x1, y2, x2);

            for (int cy = scan_y1; cy <= scan_y2 && !placed; ++cy)
            {
                for (int cx = scan_x1; cx <= scan_x2 && !placed; ++cx)
                {
                    placed = build_reserved_type8(cy, cx);
                }
            }
        }
    }

    if (placed)
    {
        (*budget_t8)--;
        if (used_t8)
            (*used_t8)++;
    }

    return placed;
}

/* Place a chest in a random floor location within partition bounds */
drop_profile drop_profile_for_mode(quadrant_mode_t mode);
bool partition_mode_avoids_corridor_spawns(quadrant_mode_t mode);
bool place_partition_chest_at(int pi, int y, int x,
    const partition_chest_recipe* recipe, quadrant_mode_t mode,
    bool require_room_tile)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    int depth = p_ptr->depth;
    drop_profile active_profile = drop_profile_for_mode(mode);
    int chest_mode = 0;

    if (!in_bounds_fully(y, x))
        return false;
    if (pi >= 0 && level_partition_index_for_point(y, x) != pi)
        return false;

    if (mode == QUAD_MODE_CHASM && !chasm_native_walkable_bold(y, x))
        return false;
    if (generation_escape_tunnel_bold(y, x))
        return false;

    /* Chests should land on an actual open floor tile, not on stairs or vault cells.
     * In corridor-avoiding modes, keep them on room-like tiles as well. */
    if (!cave_clean_bold(y, x) || cave_m_idx[y][x]
        || (cave_info[y][x] & CAVE_G_VAULT))
    {
        return false;
    }
    if (require_room_tile && !(cave_info[y][x] & CAVE_ROOM))
        return false;

    if (recipe)
        chest_mode = recipe->chest_mode;
    if (chest_mode < 0 || chest_mode > 2)
        chest_mode = 0;

    drop_set_chest_mode(chest_mode);
    drop_clear_chest_material_weights();
    if (recipe
        && recipe->material_wood_pct >= 0
        && recipe->material_steel_pct >= 0
        && recipe->material_jewel_pct >= 0)
    {
        drop_set_chest_material_weights(recipe->material_wood_pct,
            recipe->material_steel_pct, recipe->material_jewel_pct);
    }
    drop_set_chest_vault_type(0);

    object_wipe(i_ptr);

    if (!drop_generate_object_profiled(
            depth, DROP_QUALITY_NORMAL, DROP_TYPE_CHEST, 0, false,
            &active_profile, i_ptr))
    {
        drop_clear_chest_material_weights();
        drop_set_chest_mode(0);
        return false;
    }

    if (i_ptr->tval == TV_CHEST)
        i_ptr->xtra1 = (byte)(0x80 | (byte)level_partition_kind_for_point(y, x));

    if (!floor_carry(y, x, i_ptr))
    {
        genlog_anchor("Failed to carry chest in partition at (%d,%d)", y, x);
        return false;
    }

    return true;
}
