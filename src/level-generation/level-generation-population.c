/* File: level-generation-population.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

void cave_set_feat_style(int y, int x, int feat, int style_idx)
{
    if (style_idx >= 0)
        cave_set_feat_with_color(y, x, feat, style_idx);
    else
        cave_set_feat(y, x, feat);
}

void partition_theme_depth_band(int depth, int* min_depth, int* max_depth)
{
    int min_level = MAX(1, depth - PARTITION_THEME_LEVEL_DELTA);
    int max_level = MIN(MORGOTH_DEPTH + 3, depth + PARTITION_THEME_LEVEL_DELTA);

    if (min_depth)
        *min_depth = min_level;
    if (max_depth)
        *max_depth = max_level;
}

bool chasm_theme_monster_ok(
    int r_idx, int min_depth, int max_depth, bool allow_unique, bool unique_only)
{
    monster_race* r_ptr = &r_info[r_idx];

    if (!r_ptr->name || !r_ptr->rarity)
        return false;
    if (!allow_unique && (r_ptr->flags1 & RF1_UNIQUE))
        return false;
    if (unique_only && !(r_ptr->flags1 & RF1_UNIQUE))
        return false;
    if (r_ptr->flags3 & RF3_SPECIAL_VAULT_ONLY)
        return false;
    if (r_ptr->flags1 & RF1_SPECIAL_GEN)
        return false;
    if (r_ptr->light >= 0)
        return false;
    if (r_ptr->level < min_depth || r_ptr->level > max_depth)
        return false;
    if ((r_ptr->flags1 & RF1_FORCE_DEPTH) && (r_ptr->level > p_ptr->depth))
        return false;
    if (r_ptr->cur_num >= r_ptr->max_num)
        return false;

    return true;
}

bool partition_mode_uses_monster_pools(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_CAVEY:
    case QUAD_MODE_LABYRINTH:
    case QUAD_MODE_CHASM:
    case QUAD_MODE_BIG_CAVE:
        return true;
    default:
        return false;
    }
}

bool monster_name_contains_ci(const monster_race* r_ptr, cptr needle)
{
    size_t len;
    const char* name;

    if (!r_ptr || !needle || !needle[0] || !r_ptr->name)
        return false;

    len = SDL_strlen(needle);
    name = r_name + r_ptr->name;

    for (const char* p = name; *p; ++p)
    {
        if (SDL_strncasecmp(p, needle, len) == 0)
            return true;
    }

    return false;
}

bool monster_is_bat(const monster_race* r_ptr)
{
    return r_ptr && (r_ptr->d_char == 'b')
        && monster_name_contains_ci(r_ptr, "bat");
}

bool monster_has_blow_effect(const monster_race* r_ptr, byte effect)
{
    if (!r_ptr)
        return false;

    for (int i = 0; i < MONSTER_BLOW_MAX; ++i)
    {
        if (!r_ptr->blow[i].method)
            continue;
        if (r_ptr->blow[i].effect == effect)
            return true;
    }

    return false;
}

bool monster_counts_toward_labyrinth_fixed_cap(const monster_race* r_ptr)
{
    return r_ptr
        && ((r_ptr->flags1 & (RF1_NEVER_MOVE | RF1_HIDDEN_MOVE)) != 0);
}

bool monster_matches_partition_theme(
    const monster_race* r_ptr, quadrant_mode_t mode, big_cave_type_t cave_type)
{
    if (!r_ptr)
        return false;

    switch (mode)
    {
    case QUAD_MODE_CHASM:
        return (r_ptr->light < 0);

    case QUAD_MODE_BIG_CAVE:
        if (r_ptr->flags3 & (RF3_TROLL | RF3_GIANT))
            return true;

        switch (cave_type)
        {
        case BIG_CAVE_FIRE:
            return ((r_ptr->flags4 & RF4_BRTH_FIRE) != 0)
                || monster_has_blow_effect(r_ptr, RBE_FIRE);
        case BIG_CAVE_ICE:
            return ((r_ptr->flags4 & RF4_BRTH_COLD) != 0)
                || monster_has_blow_effect(r_ptr, RBE_COLD);
        case BIG_CAVE_POIS:
            return ((r_ptr->flags4 & RF4_BRTH_POIS) != 0)
                || monster_has_blow_effect(r_ptr, RBE_POISON);
        case BIG_CAVE_NONE:
        case BIG_CAVE_TYPE_MAX:
        default:
            return ((r_ptr->flags3
                        & (RF3_WOLF | RF3_SPIDER | RF3_VAMPIRE
                            | RF3_TROLL | RF3_GIANT))
                       != 0)
                || monster_is_bat(r_ptr);
        }

    case QUAD_MODE_CAVEY:
        return ((r_ptr->flags3
                    & (RF3_WOLF | RF3_SPIDER | RF3_VAMPIRE
                        | RF3_TROLL | RF3_GIANT))
                   != 0)
            || monster_is_bat(r_ptr);

    case QUAD_MODE_LABYRINTH:
        return ((r_ptr->flags2 & RF2_INVISIBLE) != 0)
            || ((r_ptr->flags1 & (RF1_NEVER_MOVE | RF1_HIDDEN_MOVE)) != 0)
            || ((r_ptr->flags4 & RF4_DIM) != 0)
            || ((r_ptr->flags3 & RF3_VAMPIRE) != 0);

    default:
        return false;
    }
}

bool partition_pool_monster_ok(
    const partition_population_plan* plan, int r_idx, int min_depth,
    int max_depth, bool themed, int labyrinth_fixed_remaining)
{
    monster_race* r_ptr = &r_info[r_idx];

    if (!plan)
        return false;
    if (!r_ptr->name || !r_ptr->rarity)
        return false;
    if (r_ptr->flags3 & RF3_SPECIAL_VAULT_ONLY)
        return false;
    if (r_ptr->flags1 & RF1_SPECIAL_GEN)
        return false;
    if (r_ptr->level < min_depth || r_ptr->level > max_depth)
        return false;
    if ((r_ptr->flags1 & RF1_FORCE_DEPTH) && (r_ptr->level > p_ptr->depth))
        return false;
    if (r_ptr->cur_num >= r_ptr->max_num)
        return false;
    if (plan->mode == QUAD_MODE_LABYRINTH
        && labyrinth_fixed_remaining <= 0
        && monster_counts_toward_labyrinth_fixed_cap(r_ptr))
    {
        return false;
    }
    if (themed && !monster_matches_partition_theme(r_ptr, plan->mode, plan->cave_type))
        return false;

    return true;
}

s16b choose_partition_pool_monster(
    const partition_population_plan* plan, bool themed, int min_depth,
    int max_depth, int labyrinth_fixed_remaining)
{
    alloc_entry* table = alloc_race_table;
    long total = 0L;

    if (!plan)
        return 0;
    if (min_depth < 1)
        min_depth = 1;
    if (max_depth > MORGOTH_DEPTH + 3)
        max_depth = MORGOTH_DEPTH + 3;
    if (min_depth > max_depth)
        return 0;

    for (int i = 0; i < alloc_race_size; ++i)
    {
        int r_idx = table[i].index;

        if (table[i].level > max_depth)
            break;
        if (!partition_pool_monster_ok(plan, r_idx, min_depth, max_depth,
                themed, labyrinth_fixed_remaining))
        continue;

        total += table[i].prob1;
    }

    if (total <= 0)
        return 0;

    {
        long value = rand_int(total);

        for (int i = 0; i < alloc_race_size; ++i)
        {
            int r_idx = table[i].index;

            if (table[i].level > max_depth)
                break;
            if (!partition_pool_monster_ok(plan, r_idx, min_depth, max_depth,
                    themed, labyrinth_fixed_remaining))
                continue;

            if (value < table[i].prob1)
                return r_idx;

            value -= table[i].prob1;
        }
    }

    return 0;
}

bool place_partition_pool_monster(
    const partition_population_plan* plan, int y, int x, bool themed,
    int labyrinth_fixed_remaining)
{
    int min_depth;
    int max_depth;

    partition_theme_depth_band(p_ptr->depth, &min_depth, &max_depth);

    for (int tries = 0; tries < 24; ++tries)
    {
        s16b r_idx = choose_partition_pool_monster(plan, themed, min_depth,
            max_depth, labyrinth_fixed_remaining);

        if (!r_idx)
            return false;

        if (plan->mode == QUAD_MODE_CHASM && themed)
        {
            if (place_chasm_theme_monster_at(y, x, r_idx))
                return true;
        }
        else if (place_monster_one(y, x, r_idx, true, false, NULL))
        {
            return true;
        }
    }

    return false;
}

s16b choose_chasm_theme_monster(
    int min_depth, int max_depth, bool allow_unique, bool unique_only)
{
    alloc_entry* table = alloc_race_table;
    long total = 0L;

    if (min_depth < 1)
        min_depth = 1;
    if (max_depth > MORGOTH_DEPTH + 3)
        max_depth = MORGOTH_DEPTH + 3;
    if (min_depth > max_depth)
        return 0;

    for (int i = 0; i < alloc_race_size; ++i)
    {
        int r_idx = table[i].index;

        if (!chasm_theme_monster_ok(
                r_idx, min_depth, max_depth, allow_unique, unique_only))
        {
            continue;
        }

        total += table[i].prob1;
    }

    if (total <= 0)
        return 0;

    {
        long value = rand_int(total);

        for (int i = 0; i < alloc_race_size; ++i)
        {
            int r_idx = table[i].index;

            if (!chasm_theme_monster_ok(
                    r_idx, min_depth, max_depth, allow_unique, unique_only))
            {
                continue;
            }

            if (value < table[i].prob1)
                return r_idx;

            value -= table[i].prob1;
        }
    }

    return 0;
}

bool place_chasm_theme_monster_at(int y, int x, int r_idx)
{
    bool had_glyph = false;

    if (!in_bounds_fully(y, x))
        return false;

    if (cave_feat[y][x] == FEAT_GLYPH)
    {
        cave_set_feat(y, x, FEAT_FLOOR);
        had_glyph = true;
    }

    if (!cave_empty_bold(y, x))
    {
        if (had_glyph)
            cave_set_feat(y, x, FEAT_GLYPH);
        return false;
    }

    if (!place_monster_one(y, x, r_idx, true, false, NULL))
    {
        if (had_glyph)
            cave_set_feat(y, x, FEAT_GLYPH);
        return false;
    }

    awaken_chasm_sanctum_monster(y, x);

    if (had_glyph)
        cave_set_feat(y, x, FEAT_GLYPH);

    return true;
}

bool chasm_sanctum_drop_present(int y, int x)
{
    return chasm_sanctum_drop_marker_present(y, x);
}

void clear_chasm_sanctum_drop_marker(int y, int x)
{
    for (object_type* o_ptr = get_first_object(y, x); o_ptr;
        o_ptr = get_next_object(o_ptr))
    {
        o_ptr->ident &= ~IDENT_CHASM_SANCTUM_ITEM;
    }
}

void awaken_chasm_sanctum_monster(int y, int x)
{
    int m_idx = cave_m_idx[y][x];

    if (m_idx <= 0)
        return;

    monster_type* m_ptr = &mon_list[m_idx];
    m_ptr->alertness = MAX(m_ptr->alertness, ALERTNESS_ALERT);
    m_ptr->skip_next_turn = false;
    m_ptr->mflag |= MFLAG_ACTV;
    m_ptr->min_range = 0;
}

bool relocate_chasm_sanctum_blocker(int y, int x)
{
    int m_idx = cave_m_idx[y][x];

    if (m_idx <= 0)
        return true;

    for (int tries = 0; tries < 200; ++tries)
    {
        int ny = rand_spread(y, 6);
        int nx = rand_spread(x, 6);
        monster_type* m_ptr = &mon_list[m_idx];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        if (!in_bounds_fully(ny, nx))
            continue;
        if (!cave_empty_bold(ny, nx))
            continue;
        if (cave_glyph(ny, nx))
            continue;
        if (chasm_sanctum_ambush_tile(ny, nx))
            continue;
        if (!cave_exist_mon(r_ptr, ny, nx, false, false))
            continue;

        monster_swap(y, x, ny, nx);
        return true;
    }

    teleport_away(m_idx, 10);
    return (cave_m_idx[y][x] == 0);
}

void trigger_chasm_sanctum_ambush_if_needed(int y, int x)
{
    int min_depth = 0;
    int max_depth = 0;
    s16b base_r_idx = 0;
    s16b unique_r_idx = 0;
    int unique_slot = -1;
    int placed = 0;

    if (!in_bounds_fully(y, x))
        return;
    if (!chasm_sanctum_drop_present(y, x))
        return;

    clear_chasm_sanctum_drop_marker(y, x);

    msg_print("The evil artefact calls to its own.");
    msg_print("A cry goes up from the deeps, and black shadows gather.");

    partition_theme_depth_band(p_ptr->depth, &min_depth, &max_depth);
    base_r_idx = choose_chasm_theme_monster(min_depth, max_depth, false, false);
    if (!base_r_idx)
    {
        log_warn("Chasm sanctum ambush: no themed monster available at depth=%d band=[%d,%d]",
            p_ptr->depth, min_depth, max_depth);
        return;
    }

    if (base_r_idx && one_in_(CHASM_AMBUSH_UNIQUE_SUB_PERCENT))
    {
        unique_r_idx = choose_chasm_theme_monster(min_depth, max_depth, true, true);
        if (unique_r_idx)
            unique_slot = rand_int(8);
    }

    for (int i = 0; i < 8; ++i)
    {
        int ny = y + chasm_sanctum_ambush_offsets[i][0];
        int nx = x + chasm_sanctum_ambush_offsets[i][1];
        bool slot_placed = false;

        if (!in_bounds_fully(ny, nx))
            continue;
        if (!relocate_chasm_sanctum_blocker(ny, nx))
            continue;

        if (base_r_idx)
        {
            s16b r_idx = (unique_r_idx && (i == unique_slot)) ? unique_r_idx : base_r_idx;

            slot_placed = place_chasm_theme_monster_at(ny, nx, r_idx);
            if (!slot_placed && unique_r_idx && (i == unique_slot))
                slot_placed = place_chasm_theme_monster_at(ny, nx, base_r_idx);
        }
        if (slot_placed)
            placed++;
    }

    (void)explosion(-1, 1, y, x, 0, 0, 0, GF_DARK_WEAK);
    (void)set_darkened(MAX(p_ptr->darkened, 8));
    monster_perception(true, false, -15);
    break_truce(true);

    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS | PU_DISTANCE);
    p_ptr->redraw |= (PR_MAP);
    handle_stuff();

    log_trace("Chasm sanctum ambush: triggered at (%d,%d), placed=%d",
        y, x, placed);
}

int partition_base_monsters_for_mode(quadrant_mode_t mode, int room_count)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));

    if (room_count <= 0)
        return 0;

    int base = (room_count + dieroll(MAX(1, room_count))) / 2;
    if (!cfg || cfg->base_monster_scale_num <= 0)
        return 0;

    return (base * cfg->base_monster_scale_num)
        / MAX(1, cfg->base_monster_scale_den);
}

int partition_apply_monster_curse_scale(int monster_count)
{
    int stacks = curse_flag_count_cur(CUR_MON_NUM);
    if (!stacks || monster_count <= 0)
        return monster_count;

    return monster_count * (100 + 30 * stacks) / 100;
}

int partition_direct_floor_monsters(quadrant_mode_t mode, int floor_count)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));
    const partition_count_rule* rule = cfg ? &cfg->direct_monsters : NULL;

    if (floor_count <= 0)
        return 0;

    if (!rule || rule->divisor <= 0)
        return 0;

    int target = floor_count / rule->divisor;
    if (target < rule->min_count)
        target = rule->min_count;
    if (rule->max_count > 0 && target > rule->max_count)
        target = rule->max_count;
    return target;
}

int partition_extra_monster_target_for_depth(
    quadrant_mode_t mode, int floor_count, int depth)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));
    const partition_depth_rule* rule = cfg ? &cfg->depth_monsters : NULL;
    int target = 0;

    if (floor_count <= 0)
        return 0;

    if (!rule || rule->divisor <= 0)
        return 0;

    target = floor_count / rule->divisor;
    if (target < rule->min_count)
        target = rule->min_count;
    if (rule->max_count > 0 && target > rule->max_count)
        target = rule->max_count;

    if (target <= 0)
        return 0;

    int scale_pct = 100;
    if (rule->scale_pct_at_depth_20 > 100 && depth > 0)
    {
        int extra_pct = rule->scale_pct_at_depth_20 - 100;
        scale_pct += (extra_pct * depth) / 20;
        if (scale_pct > rule->scale_pct_at_depth_20)
            scale_pct = rule->scale_pct_at_depth_20;
    }

    target = target * scale_pct / 100;

    {
        int hard_cap = floor_count / MAX(1, rule->hard_cap_divisor);
        if (hard_cap < 1) hard_cap = 1;
        if (target > hard_cap)
            target = hard_cap;
    }

    return target;
}

int partition_depth_bonus_monsters(quadrant_mode_t mode, int floor_count, int depth)
{
    int target_at_20 = partition_extra_monster_target_for_depth(mode, floor_count, 20);

    if (depth <= 1 || target_at_20 <= 0)
        return 0;
    if (depth >= 20)
        return target_at_20;

    return (target_at_20 * (depth - 1) + 9) / 19;
}

int partition_object_scale_pct(void)
{
    switch (op_ptr->vault_drop_frequency)
    {
    case VDF_PLENTIFUL: return 150;
    case VDF_NORMAL:    return 100;
    case VDF_MODEST:    return 67;
    case VDF_SCARCE:    return 33;
    case VDF_MEAGER:    return 10;
    default:            return 100;
    }
}

void partition_object_counts_from_total_monsters(
    quadrant_mode_t mode, int total_monsters, int* room_objects, int* corr_objects)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));
    int room_count = 0;
    int corr_count = 0;
    int pct = partition_object_scale_pct();

    if (cfg)
    {
        if (cfg->room_object_divisor > 0)
            room_count = total_monsters / cfg->room_object_divisor;
        if (cfg->corridor_object_divisor > 0)
            corr_count = total_monsters / cfg->corridor_object_divisor;
    }

    if (pct != 100)
    {
        room_count = MAX(0, room_count * pct / 100);
        corr_count = MAX(0, corr_count * pct / 100);
    }

    if (room_objects)
        *room_objects = room_count;
    if (corr_objects)
        *corr_objects = corr_count;
}

void rebalance_partition_corridor_objects(partition_population_plan* plan)
{
    int corridor_cap;
    int overflow;

    if (!plan)
        return;

    if (plan->corr_objects <= 0)
        return;

    switch (plan->mode)
    {
    case QUAD_MODE_CAVEY:
    case QUAD_MODE_LABYRINTH:
    case QUAD_MODE_BIG_CAVE:
    case QUAD_MODE_CHASM:
        plan->room_objects += plan->corr_objects;
        plan->corr_objects = 0;
        return;

    case QUAD_MODE_ROOMY:
        return;

    default:
        break;
    }

    if (plan->corridor_floor_count <= 0)
    {
        plan->room_objects += plan->corr_objects;
        plan->corr_objects = 0;
        return;
    }

    corridor_cap = plan->corridor_floor_count / 8;
    if (corridor_cap <= 0)
        corridor_cap = 1;

    if (corridor_cap >= plan->corr_objects)
        return;

    overflow = plan->corr_objects - corridor_cap;
    plan->corr_objects = corridor_cap;
    plan->room_objects += overflow;
}

void distribute_partition_base_monsters(
    partition_population_plan* plans, int plan_count)
{
    int rooms_by_mode[QUAD_MODE_BIG_CAVE + 1] = {0};

    for (int i = 0; i < plan_count; ++i)
    {
        if (plans[i].mode >= QUAD_MODE_ROOMY && plans[i].mode <= QUAD_MODE_BIG_CAVE)
            rooms_by_mode[plans[i].mode] += plans[i].room_centers;
        plans[i].monsters_base = 0;
    }

    for (int mode = QUAD_MODE_ROOMY; mode <= QUAD_MODE_BIG_CAVE; ++mode)
    {
        int total_rooms = rooms_by_mode[mode];
        int total_monsters;
        int assigned = 0;
        int remainders[PARTITION_META_MAX];

        if (total_rooms <= 0)
            continue;

        total_monsters = partition_base_monsters_for_mode((quadrant_mode_t)mode, total_rooms);
        if (total_monsters <= 0)
            continue;

        for (int i = 0; i < PARTITION_META_MAX; ++i)
            remainders[i] = -1;

        for (int i = 0; i < plan_count; ++i)
        {
            long weighted;

            if (plans[i].mode != (quadrant_mode_t)mode || plans[i].room_centers <= 0)
                continue;

            weighted = (long)total_monsters * (long)plans[i].room_centers;
            plans[i].monsters_base = (int)(weighted / total_rooms);
            remainders[i] = (int)(weighted % total_rooms);
            assigned += plans[i].monsters_base;
        }

        for (int left = total_monsters - assigned; left > 0; --left)
        {
            int best_i = -1;
            int best_rem = -1;

            for (int i = 0; i < plan_count; ++i)
            {
                if (remainders[i] > best_rem)
                {
                    best_i = i;
                    best_rem = remainders[i];
                }
            }

            if (best_i < 0)
                break;

            plans[best_i].monsters_base++;
            remainders[best_i] = -1;
        }
    }
}

void apply_curse_scale_to_partition_totals(
    partition_population_plan* plans, int plan_count)
{
    int total_monsters = 0;
    int scaled_total;
    int assigned = 0;
    int remainders[PARTITION_META_MAX];

    for (int i = 0; i < plan_count; ++i)
    {
        plans[i].monsters_curse_bonus = 0;
        total_monsters += plans[i].monsters_precurse;
        remainders[i] = -1;
    }

    if (total_monsters <= 0)
    {
        for (int i = 0; i < plan_count; ++i)
            plans[i].monsters_total = plans[i].monsters_precurse;
        return;
    }

    scaled_total = partition_apply_monster_curse_scale(total_monsters);
    if (scaled_total <= total_monsters)
    {
        for (int i = 0; i < plan_count; ++i)
            plans[i].monsters_total = plans[i].monsters_precurse;
        return;
    }

    for (int i = 0; i < plan_count; ++i)
    {
        long weighted = (long)scaled_total * (long)plans[i].monsters_precurse;

        plans[i].monsters_total = (int)(weighted / total_monsters);
        remainders[i] = (int)(weighted % total_monsters);
        assigned += plans[i].monsters_total;
    }

    for (int left = scaled_total - assigned; left > 0; --left)
    {
        int best_i = -1;
        int best_rem = -1;

        for (int i = 0; i < plan_count; ++i)
        {
            if (remainders[i] > best_rem)
            {
                best_i = i;
                best_rem = remainders[i];
            }
        }

        if (best_i < 0)
            break;

        plans[best_i].monsters_total++;
        remainders[best_i] = -1;
    }

    for (int i = 0; i < plan_count; ++i)
        plans[i].monsters_curse_bonus =
            plans[i].monsters_total - plans[i].monsters_precurse;
}

int build_partition_population_plans(
    partition_population_plan* plans, int max_plans)
{
    int room_centers[PARTITION_META_MAX] = {0};
    int count = MIN(current_partition_count, max_plans);

    if (count <= 0 || current_partition_rows <= 0 || current_partition_cols <= 0)
        return 0;

    for (int i = 0; i < dun->cent_n; ++i)
    {
        int pi = level_partition_index_for_point(dun->cent[i].y, dun->cent[i].x);
        if (pi >= 0 && pi < count)
            room_centers[pi]++;
    }

    for (int pi = 0; pi < count; ++pi)
    {
        partition_population_plan* plan = &plans[pi];

        memset(plan, 0, sizeof(*plan));
        plan->pi = pi;
        plan->mode = current_partition_modes[pi];
        plan->cave_type = current_partition_big_cave_types[pi];
        plan->meta = current_partition_population_meta[pi];
        plan->room_centers = room_centers[pi];

        if (!compute_partition_bounds(
                pi, current_partition_rows, current_partition_cols,
                &plan->y1, &plan->y2, &plan->x1, &plan->x2))
        {
            continue;
        }

        for (int y = plan->y1; y <= plan->y2; ++y)
        {
            for (int x = plan->x1; x <= plan->x2; ++x)
            {
                bool is_room;

                if (!in_bounds_fully(y, x))
                    continue;
                if (!partition_population_floor_bold(plan->mode, y, x))
                    continue;

                is_room = (cave_info[y][x] & CAVE_ROOM) ? true : false;

                plan->floor_count++;
                if (is_room)
                    plan->room_floor_count++;
                else
                    plan->corridor_floor_count++;
                if (!(cave_info[y][x] & CAVE_ICKY))
                    plan->floor_count_non_icky++;
                if (!(cave_info[y][x] & CAVE_G_VAULT))
                    plan->floor_count_non_vault++;
            }
        }
    }

    distribute_partition_base_monsters(plans, count);

    for (int i = 0; i < count; ++i)
    {
        plans[i].monsters_floor =
            partition_direct_floor_monsters(plans[i].mode, plans[i].floor_count);
        plans[i].monsters_depth =
            partition_depth_bonus_monsters(
                plans[i].mode, plans[i].floor_count_non_icky, p_ptr->depth);
        plans[i].monsters_precurse =
            plans[i].monsters_base + plans[i].monsters_floor + plans[i].monsters_depth;
    }

    apply_curse_scale_to_partition_totals(plans, count);

    for (int i = 0; i < count; ++i)
    {
        partition_object_counts_from_total_monsters(
            plans[i].mode, plans[i].monsters_precurse,
            &plans[i].room_objects, &plans[i].corr_objects);
        rebalance_partition_corridor_objects(&plans[i]);
    }

    return count;
}

bool choose_partition_monster_location(
    const partition_population_plan* plan, int* out_y, int* out_x)
{
    /* CAVEY partitions should populate across their full floor footprint.
     * Reusing the chest-style room-only filter dumps the whole monster quota
     * into whichever plain room happens to dominate the partition. */
    bool avoid_corridors = partition_mode_avoids_corridor_spawns(plan->mode)
        && (plan->mode != QUAD_MODE_CAVEY);

    for (int tries = 0; tries < 250; ++tries)
    {
        int y = rand_range(plan->y1, plan->y2);
        int x = rand_range(plan->x1, plan->x2);

        if (!in_bounds_fully(y, x))
            continue;
        if (level_partition_index_for_point(y, x) != plan->pi)
            continue;
        if (cave_info[y][x] & CAVE_ICKY)
            continue;
        if (!partition_population_naked_bold(plan->mode, y, x))
            continue;
        if (avoid_corridors && !(cave_info[y][x] & CAVE_ROOM))
            continue;
        *out_y = y;
        *out_x = x;
        return true;
    }

    return false;
}

bool place_partition_themed_monster(
    const partition_population_plan* plan, int y, int x)
{
    if (partition_mode_uses_monster_pools(plan->mode))
        return place_partition_pool_monster(plan, y, x, true, 5);

    switch (plan->mode)
    {
    default:
        break;
    }

    return false;
}

bool partition_monster_pass_skips_plan(
    const partition_population_plan* plan)
{
    if (!plan)
        return false;

    return morgoth_region_active() && (plan->pi == morgoth_partition_index);
}

int run_partition_monster_pass(
    const partition_population_plan* plans, int plan_count)
{
    int total_placed = 0;

    for (int i = 0; i < plan_count; ++i)
    {
        const partition_population_plan* plan = &plans[i];
        bool use_pool_theme = partition_mode_uses_monster_pools(plan->mode);
        int labyrinth_fixed_remaining =
            (plan->mode == QUAD_MODE_LABYRINTH) ? 5 : 0;
        int generic_remaining = 0;
        int themed_remaining = 0;
        int generic_target = 0;
        int themed_target = 0;
        int target_total = 0;
        int placed = 0;
        int attempts;

        if (use_pool_theme)
        {
            target_total = plan->monsters_total;
            themed_target =
                (target_total * PARTITION_THEME_MONSTER_PERCENT + 50) / 100;
            if (themed_target > target_total)
                themed_target = target_total;
            generic_target = target_total - themed_target;
            themed_remaining = themed_target;
            generic_remaining = generic_target;
        }
        else
        {
            int precurse_total;
            int curse_bonus;

            generic_remaining = plan->monsters_base;
            themed_remaining = plan->monsters_floor + plan->monsters_depth;
            precurse_total = generic_remaining + themed_remaining;
            curse_bonus = plan->monsters_curse_bonus;

            if (curse_bonus > 0)
            {
                int generic_bonus = 0;

                if (precurse_total > 0 && generic_remaining > 0)
                {
                    long weighted_generic =
                        (long)curse_bonus * (long)generic_remaining;

                    generic_bonus = (int)(weighted_generic / precurse_total);
                    if ((weighted_generic % precurse_total) * 2 >= precurse_total)
                        generic_bonus++;
                }

                if (generic_bonus > curse_bonus)
                    generic_bonus = curse_bonus;

                generic_remaining += generic_bonus;
                themed_remaining += curse_bonus - generic_bonus;
            }

            generic_target = generic_remaining;
            themed_target = themed_remaining;
            target_total = generic_remaining + themed_remaining;
        }

        attempts = MAX(1, target_total) * 250;

        if (partition_monster_pass_skips_plan(plan))
        {
            if (target_total > 0)
            {
                log_trace(
                    "Partition monsters: pi=%d mode=%d skipped for Morgoth throne partition",
                    plan->pi, plan->mode);
            }
            continue;
        }

        for (int tries = 0;
             tries < attempts && (generic_remaining > 0 || themed_remaining > 0);
             ++tries)
        {
            bool themed =
                (themed_remaining > 0)
                && ((generic_remaining == 0)
                    || (rand_int(generic_remaining + themed_remaining) < themed_remaining));
            int y, x;
            bool placed_mon = false;
            bool consume_themed_quota = themed;

            if (!choose_partition_monster_location(plan, &y, &x))
                continue;

            if (use_pool_theme)
            {
                placed_mon = place_partition_pool_monster(plan, y, x, themed,
                    labyrinth_fixed_remaining);
                if (!placed_mon && themed)
                {
                    placed_mon = place_partition_pool_monster(plan, y, x, false,
                        labyrinth_fixed_remaining);
                    if (placed_mon && generic_remaining > 0)
                        consume_themed_quota = false;
                }
            }
            else
            {
                if (themed)
                    placed_mon = place_partition_themed_monster(plan, y, x);

                if (!placed_mon)
                    placed_mon = place_monster(y, x, true,
                        (!themed && plan->mode == QUAD_MODE_ROOMY), false);
            }

            if (!placed_mon)
                continue;

            if (consume_themed_quota)
                themed_remaining--;
            else
                generic_remaining--;

            if (plan->mode == QUAD_MODE_LABYRINTH && cave_m_idx[y][x] > 0
                && labyrinth_fixed_remaining > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
                monster_race* r_ptr = &r_info[m_ptr->r_idx];

                if (monster_counts_toward_labyrinth_fixed_cap(r_ptr))
                    labyrinth_fixed_remaining--;
            }

            placed++;
            total_placed++;
        }

        log_trace(
            "Partition monsters: pi=%d mode=%d rooms=%d floors=%d base=%d floor=%d depth=%d precurse=%d curse=%d total=%d theme_target=%d global_target=%d placed=%d",
            plan->pi, plan->mode, plan->room_centers, plan->floor_count,
            plan->monsters_base, plan->monsters_floor, plan->monsters_depth,
            plan->monsters_precurse, plan->monsters_curse_bonus,
            target_total, themed_target, generic_target, placed);
    }

    return total_placed;
}

int alloc_objects_from_plan(
    const partition_population_plan* plan, int set, int num)
{
    int placed = 0;

    for (int k = 0; k < num; ++k)
    {
        partition_drop_profile active_profile =
            partition_drop_profile_for_mode_source_cfg(
                plan->mode, PARTITION_DROP_SOURCE_FLOOR);
        int y = 0;
        int x = 0;
        int i;

        for (i = 0; i < 10000; ++i)
        {
            bool is_room;

            y = rand_range(plan->y1, plan->y2);
            x = rand_range(plan->x1, plan->x2);

            if (!in_bounds_fully(y, x))
                continue;
            if (!partition_population_naked_bold(plan->mode, y, x))
                continue;
            if (level_partition_index_for_point(y, x) != plan->pi)
                continue;

            is_room = (cave_info[y][x] & CAVE_ROOM) ? true : false;

            if (plan->mode != QUAD_MODE_CHASM)
            {
                if ((set == ALLOC_SET_CORR) && is_room)
                    continue;
                if ((set == ALLOC_SET_ROOM) && !is_room)
                    continue;
            }

            active_profile = partition_drop_profile_for_mode_source_cfg(
                drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
            if (!active_profile.allow_floor_drops)
                continue;

            break;
        }

        if (i >= 10000)
            continue;

        place_object_with_profile(y, x, &active_profile);
        if (cave_o_idx[y][x] != 0)
            placed++;
    }

    return placed;
}

int run_partition_object_pass(
    const partition_population_plan* plans, int plan_count, bool rooms)
{
    int total_placed = 0;

    for (int i = 0; i < plan_count; ++i)
    {
        int target = rooms ? plans[i].room_objects : plans[i].corr_objects;
        int placed;

        if (target <= 0)
            continue;

        placed = alloc_objects_from_plan(
            &plans[i], rooms ? ALLOC_SET_ROOM : ALLOC_SET_CORR, target);
        total_placed += placed;

        log_trace("Partition %s objects: pi=%d mode=%d target=%d placed=%d total_monsters=%d",
            rooms ? "room" : "corridor", plans[i].pi, plans[i].mode,
            target, placed, plans[i].monsters_total);
    }

    return total_placed;
}

int place_partition_skeletons(
    const partition_population_plan* plan, int target,
    int human_pct, int elf_pct, bool avoid_rubble)
{
    int placed = 0;

    for (int sk = 0; sk < target; ++sk)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int sy = rand_range(plan->y1, plan->y2);
            int sx = rand_range(plan->x1, plan->x2);
            int roll;
            s16b k_idx;
            object_type object_type_body;
            object_type *i_ptr = &object_type_body;

            if (!in_bounds_fully(sy, sx))
                continue;
            if (!cave_floor_bold(sy, sx))
                continue;
            if (generation_escape_tunnel_bold(sy, sx))
                continue;
            if ((cave_info[sy][sx] & CAVE_G_VAULT) != 0)
                continue;
            if (avoid_rubble && cave_feat[sy][sx] == FEAT_RUBBLE)
                continue;
            if (cave_o_idx[sy][sx] != 0)
                continue;

            object_wipe(i_ptr);

            roll = rand_int(100);
            if (roll < human_pct)
                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);
            else if (roll < human_pct + elf_pct)
                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ELF);
            else
                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ORC);

            object_prep(i_ptr, k_idx);
            i_ptr->pval = 1;
            drop_near(i_ptr, -1, sy, sx);
            placed++;
            break;
        }
    }

    return placed;
}

bool partition_exact_monster_tile_ok(
    const partition_population_plan* plan, int y, int x)
{
    if (!plan)
        return false;
    if (!in_bounds_fully(y, x))
        return false;
    if (level_partition_index_for_point(y, x) != plan->pi)
        return false;
    if (cave_info[y][x] & CAVE_G_VAULT)
        return false;
    if (!partition_population_naked_bold(plan->mode, y, x))
        return false;

    return true;
}

int place_partition_exact_monster_tokens(
    const partition_population_plan* plan, char token, int target)
{
    int placed = 0;

    if (!plan || target <= 0)
        return 0;

    for (int n = 0; n < target; ++n)
    {
        bool placed_this = false;

        for (int tries = 0; tries < 500; ++tries)
        {
            int y = rand_range(plan->y1, plan->y2);
            int x = rand_range(plan->x1, plan->x2);

            if (!partition_exact_monster_tile_ok(plan, y, x))
                continue;
            if (!place_vault_monster_token(token, y, x))
                continue;

            placed++;
            placed_this = true;
            break;
        }

        if (!placed_this)
        {
            for (int y = plan->y1; y <= plan->y2 && !placed_this; ++y)
            {
                for (int x = plan->x1; x <= plan->x2; ++x)
                {
                    if (!partition_exact_monster_tile_ok(plan, y, x))
                        continue;
                    if (!place_vault_monster_token(token, y, x))
                        continue;

                    placed++;
                    placed_this = true;
                    break;
                }
            }
        }

        if (!placed_this)
            break;
    }

    return placed;
}

int run_partition_special_scatter_pass(
    const partition_population_plan* plans, int plan_count)
{
    int total_placed = 0;

    for (int i = 0; i < plan_count; ++i)
    {
        const partition_population_plan* plan = &plans[i];

        total_placed += place_partition_metal_drops(plan);

        if (plan->mode == QUAD_MODE_BIG_CAVE && plan->floor_count > 0)
        {
            int target = plan->floor_count / 40;
            if (target < 2) target = 2;
            if (target > 8) target = 8;
            total_placed += place_partition_skeletons(plan, target, 35, 35, false);
        }
        else if (plan->mode == QUAD_MODE_LABYRINTH && plan->floor_count > 0)
        {
            int target = plan->floor_count / 25;
            if (target < 2) target = 2;
            if (target > 6) target = 6;
            total_placed += place_partition_skeletons(plan, target, 30, 60, false);
        }
        else if (plan->mode == QUAD_MODE_RUINED && plan->floor_count_non_vault > 0)
        {
            int skeleton_target = plan->floor_count_non_vault / 15;

            if (skeleton_target < 3) skeleton_target = 3;
            if (skeleton_target > 10) skeleton_target = 10;
            total_placed += place_partition_skeletons(plan, skeleton_target, 20, 20, true);
        }
        else if (plan->mode == QUAD_MODE_CHASM && plan->floor_count > 0)
        {
            total_placed += place_partition_exact_monster_tokens(
                plan, 'q', CHASM_WHISPERING_SHADOW_TARGET);
        }

        for (int chest = 0; chest < plan->meta.chest_count; ++chest)
        {
            place_chest_in_partition(
                plan->pi, plan->y1, plan->y2, plan->x1, plan->x2,
                &plan->meta.chest_recipes[chest], plan->mode);
        }

        log_trace("Partition specials: pi=%d mode=%d chests=%d",
            plan->pi, plan->mode, plan->meta.chest_count);
    }

    return total_placed;
}

bool connect_morgoth_entry_tunnels(void)
{
    if (!morgoth_region_active())
        return true;

    static bool visited[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    int components_found = 0;
    int components_connected = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
            visited[y][x] = false;

    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            if (!(cave_info[y][x] & CAVE_MORGOTH_TUNNEL))
                continue;
            if (visited[y][x])
                continue;

            components_found++;
            int head = 0;
            int tail = 0;
            int min_y = y;
            int min_x = x;
            int max_x = x;
            bool start_found = false;

            int start_idx = y * MAX_DUNGEON_WID + x;
            queue[tail++] = start_idx;
            visited[y][x] = true;

            while (head < tail)
            {
                int cur = queue[head++];
                int cy = cur / MAX_DUNGEON_WID;
                int cx = cur % MAX_DUNGEON_WID;

                if (cy < min_y)
                {
                    min_y = cy;
                    min_x = cx;
                    max_x = cx;
                    start_found = true;
                }
                else if (cy == min_y)
                {
                    if (!start_found)
                    {
                        min_x = cx;
                        max_x = cx;
                        start_found = true;
                    }
                    else
                    {
                        min_x = MIN(min_x, cx);
                        max_x = MAX(max_x, cx);
                    }
                }

                for (int d = 0; d < 4; ++d)
                {
                    int ny = cy + ddy4[d];
                    int nx = cx + ddx4[d];
                    if (!in_bounds_fully(ny, nx))
                        continue;
                    if (visited[ny][nx])
                        continue;
                    if (!(cave_info[ny][nx] & CAVE_MORGOTH_TUNNEL))
                        continue;
                    visited[ny][nx] = true;
                    if (tail < (int)N_ELEMENTS(queue))
                        queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }

            int start_y = min_y;
            int start_x = (min_x + max_x) / 2;
            if (!(cave_info[start_y][start_x] & CAVE_MORGOTH_TUNNEL))
            {
                bool found = false;
                for (int tx = min_x; tx <= max_x; ++tx)
                {
                    if (cave_info[start_y][tx] & CAVE_MORGOTH_TUNNEL)
                    {
                        start_x = tx;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    start_x = x;
            }

            if (connect_morgoth_tunnel_component(start_y, start_x))
            {
                components_connected++;
            }
            else
            {
                log_trace("connect_morgoth_entry_tunnels: component at (%d,%d) could not reach outside-region floor",
                    start_y, start_x);
            }
        }
    }

    if (components_found == 0)
    {
        log_trace("connect_morgoth_entry_tunnels: no tunnel components found");
        genlog_fail("CONNECTIVITY FAILED: Morgoth entry tunnels missing");
        return false;
    }

    if (components_connected != components_found)
    {
        log_trace("connect_morgoth_entry_tunnels: connected %d/%d tunnel components",
            components_connected, components_found);
        genlog_fail("CONNECTIVITY FAILED: Morgoth entry tunnels connected %d/%d components",
            components_connected, components_found);
        return false;
    }

    log_trace("connect_morgoth_entry_tunnels: connected %d/%d tunnel components",
        components_connected, components_found);
    genlog_connect("Morgoth entry tunnels: connected %d/%d components",
        components_connected, components_found);
    return true;
}

/*
 * Type 10 -- The Gates of Angband (see "vault.txt")
 */
bool build_type10(int y0, int x0)
{
    vault_type* v_ptr;

    /* Get the first vault record */
    v_ptr = &v_info[1];

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, false))
    {
        return (false);
    }

    /* Cause a special feeling */
    good_item_flag = true;

    /* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
    if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return (true);
}
