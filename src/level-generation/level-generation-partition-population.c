/* File: level-generation-partition-population.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"


partition_rule_config g_partition_rules[LEVEL_PART_MAX];
bool g_partition_rules_initialized = false;
const partition_rule_config* partition_config_get(level_partition_kind kind);

level_partition_kind partition_config_normalize_kind(level_partition_kind kind)
{
    if (kind <= LEVEL_PART_NONE || kind >= LEVEL_PART_MAX)
        return LEVEL_PART_ROOMY;
    return kind;
}

void partition_config_profile_assign(drop_profile* profile,
    int weapon, int armor, int jewelry, int supply, int potion, int herb,
    int gem, int staff, int light, int arrows, int tunneling)
{
    if (!profile)
        return;

    profile->weight_weapon = weapon;
    profile->weight_armor = armor;
    profile->weight_jewelry = jewelry;
    profile->weight_supply = supply;
    profile->supply_potion = potion;
    profile->supply_herb = herb;
    profile->supply_gem = gem;
    profile->supply_staff = staff;
    profile->supply_light = light;
    profile->supply_arrows = arrows;
    profile->supply_tunneling = tunneling;
}

void partition_config_set_defaults_for_kind(level_partition_kind kind)
{
    partition_rule_config* cfg;
    drop_profile base_profile;

    kind = partition_config_normalize_kind(kind);
    cfg = &g_partition_rules[kind];

    memset(cfg, 0, sizeof(*cfg));
    drop_profile_default(&base_profile);

    cfg->profiles[PARTITION_DROP_SOURCE_FLOOR] = base_profile;
    cfg->profiles[PARTITION_DROP_SOURCE_CHEST] = base_profile;
    cfg->profiles[PARTITION_DROP_SOURCE_MONSTER] = base_profile;
    cfg->allow_floor_drops = true;
    cfg->base_monster_scale_num = 1;
    cfg->base_monster_scale_den = 1;
    cfg->room_object_divisor = 8;
    cfg->corridor_object_divisor = 12;

    switch (kind)
    {
    case LEVEL_PART_ROOMY:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            40, 30, 10, 20, 1, 1, 1, 1, 1, 1, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            40, 30, 10, 0, 0, 0, 0, 0, 0, 0, 0);
        break;

    case LEVEL_PART_CAVEY:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            0, 0, 0, 100, 0, 0, 12, 3, 6, 6, 1);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            25, 25, 25, 0, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 3;
        cfg->base_monster_scale_den = 1;
        cfg->depth_monsters.divisor = 120;
        cfg->depth_monsters.min_count = 0;
        cfg->depth_monsters.max_count = 18;
        cfg->depth_monsters.scale_pct_at_depth_20 = 140;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 8;
        cfg->corridor_object_divisor = 12;
        cfg->metal_drops.divisor = 300;
        cfg->metal_drops.max_count = 2;
        cfg->metal_drops.min_depth = 8;
        break;

    case LEVEL_PART_RUINED:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            40, 35, 0, 25, 7, 2, 1, 3, 15, 15, 2);
        cfg->profiles[PARTITION_DROP_SOURCE_FLOOR].allow_damaged = true;
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            40, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 2;
        cfg->base_monster_scale_den = 1;
        cfg->depth_monsters.divisor = 180;
        cfg->depth_monsters.min_count = 0;
        cfg->depth_monsters.max_count = 14;
        cfg->depth_monsters.scale_pct_at_depth_20 = 133;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 2;
        cfg->corridor_object_divisor = 4;
        break;

    case LEVEL_PART_LABYRINTH:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            0, 0, 35, 65, 15, 2, 2, 15, 5, 5, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            0, 0, 35, 0, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 2;
        cfg->base_monster_scale_den = 1;
        cfg->direct_monsters.divisor = 7;
        cfg->direct_monsters.min_count = 8;
        cfg->direct_monsters.max_count = 45;
        cfg->depth_monsters.divisor = 80;
        cfg->depth_monsters.min_count = 0;
        cfg->depth_monsters.max_count = 25;
        cfg->depth_monsters.scale_pct_at_depth_20 = 133;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 2;
        cfg->corridor_object_divisor = 4;
        break;

    case LEVEL_PART_CHASM:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            40, 30, 20, 10, 1, 1, 1, 1, 1, 1, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            40, 30, 20, 0, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 4;
        cfg->base_monster_scale_den = 1;
        cfg->direct_monsters.divisor = 38;
        cfg->direct_monsters.min_count = 10;
        cfg->direct_monsters.max_count = 42;
        cfg->depth_monsters.divisor = 38;
        cfg->depth_monsters.min_count = 10;
        cfg->depth_monsters.max_count = 40;
        cfg->depth_monsters.scale_pct_at_depth_20 = 133;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 2;
        cfg->corridor_object_divisor = 4;
        cfg->metal_drops.divisor = 240;
        cfg->metal_drops.max_count = 4;
        cfg->metal_drops.min_depth = 8;
        break;

    case LEVEL_PART_BIG_CAVE:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            20, 20, 15, 45, 0, 2, 8, 2, 0, 0, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            20, 20, 15, 0, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 5;
        cfg->base_monster_scale_den = 1;
        cfg->direct_monsters.divisor = 55;
        cfg->direct_monsters.min_count = 10;
        cfg->direct_monsters.max_count = 36;
        cfg->depth_monsters.divisor = 45;
        cfg->depth_monsters.min_count = 12;
        cfg->depth_monsters.max_count = 40;
        cfg->depth_monsters.scale_pct_at_depth_20 = 133;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 2;
        cfg->corridor_object_divisor = 4;
        cfg->metal_drops.divisor = 500;
        cfg->metal_drops.max_count = 2;
        cfg->metal_drops.min_depth = 8;
        break;

    case LEVEL_PART_NONE:
    case LEVEL_PART_MAX:
    default:
        break;
    }
}

void partition_config_ensure_initialized(void)
{
    if (g_partition_rules_initialized)
        return;

    partition_config_reset();
}

void partition_config_reset(void)
{
    for (int kind = 0; kind < LEVEL_PART_MAX; ++kind)
        partition_config_set_defaults_for_kind((level_partition_kind)kind);

    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
    g_partition_rules_initialized = true;
}

void partition_config_set_drop_profile(level_partition_kind kind,
    partition_drop_source_t source, const drop_profile* profile)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    if (source < PARTITION_DROP_SOURCE_FLOOR
        || source >= PARTITION_DROP_SOURCE_MAX)
    {
        return;
    }

    if (profile)
        g_partition_rules[kind].profiles[source] = *profile;
    else
        drop_profile_default(&g_partition_rules[kind].profiles[source]);

    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_floor_rules(level_partition_kind kind,
    bool allow_floor_drops)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].allow_floor_drops = allow_floor_drops;
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_base_monster_scale(level_partition_kind kind,
    int numerator, int denominator)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].base_monster_scale_num = MAX(0, numerator);
    g_partition_rules[kind].base_monster_scale_den = MAX(1, denominator);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_direct_monster_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].direct_monsters.divisor = MAX(0, divisor);
    g_partition_rules[kind].direct_monsters.min_count = MAX(0, min_count);
    g_partition_rules[kind].direct_monsters.max_count = MAX(0, max_count);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_depth_monster_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count, int scale_pct_at_depth_20,
    int hard_cap_divisor)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].depth_monsters.divisor = MAX(0, divisor);
    g_partition_rules[kind].depth_monsters.min_count = MAX(0, min_count);
    g_partition_rules[kind].depth_monsters.max_count = MAX(0, max_count);
    g_partition_rules[kind].depth_monsters.scale_pct_at_depth_20 =
        MAX(0, scale_pct_at_depth_20);
    g_partition_rules[kind].depth_monsters.hard_cap_divisor =
        MAX(1, hard_cap_divisor);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_object_rules(level_partition_kind kind,
    int room_divisor, int corridor_divisor)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].room_object_divisor = MAX(0, room_divisor);
    g_partition_rules[kind].corridor_object_divisor = MAX(0, corridor_divisor);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_metal_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count, int min_depth)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].metal_drops.divisor = MAX(0, divisor);
    g_partition_rules[kind].metal_drops.min_count = MAX(0, min_count);
    g_partition_rules[kind].metal_drops.max_count = MAX(0, max_count);
    g_partition_rules[kind].metal_drops.min_depth = MAX(0, min_depth);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_discovery_text(level_partition_kind kind, cptr text)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    SDL_strlcpy(g_partition_rules[kind].discovery_text, text ? text : "",
        sizeof(g_partition_rules[kind].discovery_text));
}

void partition_config_set_big_cave_discovery_text(big_cave_type_t cave_type,
    cptr text)
{
    partition_config_ensure_initialized();

    if (cave_type <= BIG_CAVE_NONE || cave_type >= BIG_CAVE_TYPE_MAX)
        return;

    SDL_strlcpy(g_partition_rules[LEVEL_PART_BIG_CAVE]
                    .big_cave_discovery_text[cave_type],
        text ? text : "",
        sizeof(g_partition_rules[LEVEL_PART_BIG_CAVE]
                   .big_cave_discovery_text[cave_type]));
}

cptr partition_config_get_discovery_text(level_partition_kind kind,
    big_cave_type_t cave_type)
{
    const partition_rule_config* cfg = partition_config_get(kind);

    if (!cfg)
        return NULL;

    if (kind == LEVEL_PART_BIG_CAVE
        && cave_type > BIG_CAVE_NONE && cave_type < BIG_CAVE_TYPE_MAX
        && cfg->big_cave_discovery_text[cave_type][0])
    {
        return cfg->big_cave_discovery_text[cave_type];
    }

    return cfg->discovery_text[0] ? cfg->discovery_text : NULL;
}

const partition_rule_config* partition_config_get(level_partition_kind kind)
{
    partition_config_ensure_initialized();
    return &g_partition_rules[partition_config_normalize_kind(kind)];
}

quadrant_mode_t partition_mode_for_point(int y, int x)
{
    /* If we're on a loaded level (no generation metadata), infer a reasonable grid and
     * classify big partitions so runtime systems (drops, UI messages) can work. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
    {
        /* Candidate grids matching apply_quadrant_generation_modes() (including its random-orientation cases). */
        int blocks = (PANEL_HGT > 0) ? (p_ptr->cur_map_hgt / PANEL_HGT) : 0;
        int grids[2][2] = {{0, 0}, {0, 0}};
        int grid_count = 0;

        if (blocks > 0)
        {
            if (blocks <= 9)
            {
                grids[0][0] = 2; grids[0][1] = 2; grid_count = 1;
            }
            else if (blocks == 10)
            {
                grids[0][0] = 3; grids[0][1] = 2;
                grids[1][0] = 2; grids[1][1] = 3;
                grid_count = 2;
            }
            else if (blocks <= 13)
            {
                grids[0][0] = 3; grids[0][1] = 3; grid_count = 1;
            }
            else if (blocks == 14)
            {
                grids[0][0] = 3; grids[0][1] = 4;
                grids[1][0] = 4; grids[1][1] = 3;
                grid_count = 2;
            }
            else if (blocks <= 16)
            {
                grids[0][0] = 4; grids[0][1] = 4; grid_count = 1;
            }
            else if (blocks <= 20)
            {
                grids[0][0] = 5; grids[0][1] = 4;
                grids[1][0] = 4; grids[1][1] = 5;
                grid_count = 2;
            }
            else
            {
                grids[0][0] = 5; grids[0][1] = 5; grid_count = 1;
            }
        }

        int best_rows = 0, best_cols = 0;
        int best_score = -1000000;
        quadrant_mode_t best_modes[25];
        memset(best_modes, 0, sizeof(best_modes));

        for (int gi = 0; gi < grid_count; ++gi)
        {
            int rows = grids[gi][0];
            int cols = grids[gi][1];
            if (rows <= 0 || cols <= 0)
                continue;

            int count = rows * cols;
            if (count <= 0 || count > 25)
                continue;

            quadrant_mode_t modes_tmp[25];
            for (int i = 0; i < 25; ++i)
                modes_tmp[i] = QUAD_MODE_ROOMY;

            int chasm_parts = 0, labyrinth_parts = 0, cave_parts = 0;
            int total_chasm_tiles = 0, max_chasm_tiles = 0;

            for (int pi = 0; pi < count; ++pi)
            {
                int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
                if (!compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
                    continue;

                int tiles = 0;
                int open_tiles = 0;
                int open_dead_ends = 0;
                int open_wide = 0;
                int open_corridor = 0;
                int chasm_tiles = 0;

                for (int yy = y1; yy <= y2; ++yy)
                {
                    for (int xx = x1; xx <= x2; ++xx)
                    {
                        if (!in_bounds_fully(yy, xx))
                            continue;
                        tiles++;

                        if ((cave_info[yy][xx] & CAVE_CHASM_AREA) || (cave_feat[yy][xx] == FEAT_CHASM))
                        {
                            chasm_tiles++;
                            continue;
                        }

                        if (!cave_floorlike_bold(yy, xx))
                            continue;

                        open_tiles++;

                        int n = 0;
                        if (in_bounds_fully(yy - 1, xx) && cave_floorlike_bold(yy - 1, xx) && cave_feat[yy - 1][xx] != FEAT_CHASM) n++;
                        if (in_bounds_fully(yy + 1, xx) && cave_floorlike_bold(yy + 1, xx) && cave_feat[yy + 1][xx] != FEAT_CHASM) n++;
                        if (in_bounds_fully(yy, xx - 1) && cave_floorlike_bold(yy, xx - 1) && cave_feat[yy][xx - 1] != FEAT_CHASM) n++;
                        if (in_bounds_fully(yy, xx + 1) && cave_floorlike_bold(yy, xx + 1) && cave_feat[yy][xx + 1] != FEAT_CHASM) n++;

                        if (n <= 1) open_dead_ends++;
                        if (n >= 3) open_wide++;
                        if (n == 2) open_corridor++;
                    }
                }

                total_chasm_tiles += chasm_tiles;
                if (chasm_tiles > max_chasm_tiles) max_chasm_tiles = chasm_tiles;

                quadrant_mode_t picked = QUAD_MODE_ROOMY;

                if (chasm_tiles > 0)
                {
                    picked = QUAD_MODE_CHASM;
                    chasm_parts++;
                }
                else if (tiles > 0 && open_tiles > 0)
                {
                    int open_pct = (open_tiles * 100) / tiles;
                    int wide_pct = (open_wide * 100) / open_tiles;
                    int dead_pct = (open_dead_ends * 100) / open_tiles;
                    int corridor_pct = (open_corridor * 100) / open_tiles;

                    /* BIG_CAVE: lots of open area, many wide tiles (3-4 neighbors). */
                    if (open_pct >= 38 && wide_pct >= 40)
                    {
                        picked = QUAD_MODE_BIG_CAVE;
                        cave_parts++;
                    }
                    /* LABYRINTH: corridor-dominated maze with relatively few open 'wide' tiles. */
                    else if (wide_pct <= 28 && corridor_pct >= 50 && dead_pct >= 8 && open_pct <= 55)
                    {
                        picked = QUAD_MODE_LABYRINTH;
                        labyrinth_parts++;
                    }
                }

                modes_tmp[pi] = picked;
            }

            /* Score grids that keep special features concentrated (avoid splitting a big area across partitions). */
            int score = 0;
            score -= (chasm_parts * 100);
            score -= ((labyrinth_parts + cave_parts) * 20);
            if (total_chasm_tiles > 0)
                score += (max_chasm_tiles * 500) / total_chasm_tiles;

            if (score > best_score)
            {
                best_score = score;
                best_rows = rows;
                best_cols = cols;
                memcpy(best_modes, modes_tmp, sizeof(best_modes));
            }
        }

        if (best_rows > 0 && best_cols > 0)
        {
            int count = best_rows * best_cols;
            remember_partition_grid(best_rows, best_cols, count);
            for (int i = 0; i < count; ++i)
                current_partition_modes[i] = best_modes[i];
            for (int i = count; i < 25; ++i)
                current_partition_modes[i] = QUAD_MODE_ROOMY;

            /* Densities are only used for generation decisions, so default to NORMAL. */
            for (int i = 0; i < 25; ++i)
                current_partition_densities[i] = DENSITY_NORMAL;

            log_trace("Inferred partition grid for runtime: blocks=%d grid=%dx%d score=%d",
                      blocks, best_rows, best_cols, best_score);
        }
    }

    int pi = partition_index_from_point(
        y, x, current_partition_rows, current_partition_cols);
    if (pi >= 0 && pi < current_partition_count)
        return current_partition_modes[pi];
    return QUAD_MODE_ROOMY;
}

/* Determine appropriate drop mode for a location based on partition type. */
quadrant_mode_t drop_mode_for_point(int y, int x)
{
    return partition_mode_for_point(y, x);
}

level_partition_kind partition_kind_from_mode(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return LEVEL_PART_ROOMY;
    case QUAD_MODE_CAVEY:
        return LEVEL_PART_CAVEY;
    case QUAD_MODE_RUINED:
        return LEVEL_PART_RUINED;
    case QUAD_MODE_LABYRINTH:
        return LEVEL_PART_LABYRINTH;
    case QUAD_MODE_CHASM:
        return LEVEL_PART_CHASM;
    case QUAD_MODE_BIG_CAVE:
        return LEVEL_PART_BIG_CAVE;
    default:
        return LEVEL_PART_NONE;
    }
}

/* Native chasm walkable terrain is the platform/bridge floor carved by the
 * chasm generator, not later boundary openings or rescue corridors. */
bool chasm_native_walkable_bold(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (!cave_floor_bold(y, x))
        return false;
    return ((cave_info[y][x] & (CAVE_ROOM | CAVE_CHASM_AREA))
        == (CAVE_ROOM | CAVE_CHASM_AREA));
}

bool partition_population_floor_bold(quadrant_mode_t mode, int y, int x)
{
    if (generation_escape_tunnel_bold(y, x))
        return false;
    if (mode == QUAD_MODE_CHASM)
        return chasm_native_walkable_bold(y, x);
    return cave_floor_bold(y, x);
}

bool partition_population_naked_bold(quadrant_mode_t mode, int y, int x)
{
    if (generation_escape_tunnel_bold(y, x))
        return false;
    if (mode == QUAD_MODE_CHASM)
    {
        if (chasm_sanctum_ambush_tile(y, x))
            return false;
        return chasm_native_walkable_bold(y, x) && cave_naked_bold(y, x);
    }
    return cave_naked_bold(y, x);
}

bool partition_mode_avoids_corridor_spawns(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_CAVEY:
    case QUAD_MODE_LABYRINTH:
    case QUAD_MODE_BIG_CAVE:
    case QUAD_MODE_CHASM:
        return true;

    default:
        return false;
    }
}

const char* quadrant_mode_debug_name(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return "ROOMY";
    case QUAD_MODE_CAVEY:
        return "CAVEY";
    case QUAD_MODE_RUINED:
        return "RUINED";
    case QUAD_MODE_LABYRINTH:
        return "LABYRINTH";
    case QUAD_MODE_CHASM:
        return "CHASM";
    case QUAD_MODE_BIG_CAVE:
        return "BIG_CAVE";
    default:
        return "UNKNOWN";
    }
}

const char* partition_kind_debug_name(level_partition_kind kind)
{
    switch (kind)
    {
    case LEVEL_PART_ROOMY:
        return "ROOMY";
    case LEVEL_PART_CAVEY:
        return "CAVEY";
    case LEVEL_PART_RUINED:
        return "RUINED";
    case LEVEL_PART_LABYRINTH:
        return "LABYRINTH";
    case LEVEL_PART_CHASM:
        return "CHASM";
    case LEVEL_PART_BIG_CAVE:
        return "BIG_CAVE";
    default:
        return "NONE";
    }
}

const char* big_cave_type_debug_name(big_cave_type_t cave_type)
{
    switch (cave_type)
    {
    case BIG_CAVE_ICE:
        return "ICE";
    case BIG_CAVE_FIRE:
        return "FIRE";
    case BIG_CAVE_POIS:
        return "POIS";
    default:
        return "NONE";
    }
}

bool suppress_partition_effects_for_point(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    return (cave_info[y][x] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0;
}

level_partition_kind level_partition_kind_for_point(int y, int x)
{
    /* Suppress partition effects (labyrinth memory loss, big-cave penalties, etc.)
     * inside greater vault regions and Morgoth's entry tunnels. */
    if (suppress_partition_effects_for_point(y, x))
        return LEVEL_PART_ROOMY;

    /* Chests should follow the partition they spawned in (not room overrides). */
    quadrant_mode_t mode = partition_mode_for_point(y, x);
    return partition_kind_from_mode(mode);
}

void level_partition_meta_get(partition_meta_save* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    /* Populate metadata if this is a loaded level. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        (void)partition_mode_for_point(p_ptr->py, p_ptr->px);

    out->grid_rows = (s16b)current_partition_rows;
    out->grid_cols = (s16b)current_partition_cols;
    out->partition_count = (s16b)current_partition_count;

    for (int i = 0; i < PARTITION_META_MAX; ++i)
        out->modes[i] = (byte)current_partition_modes[i];

    for (int i = 0; i < PARTITION_META_MAX; ++i)
        out->big_cave_types[i] = (byte)current_partition_big_cave_types[i];
}

void level_partition_meta_set(const partition_meta_save* in)
{
    if (!in)
        return;

    int rows = in->grid_rows;
    int cols = in->grid_cols;
    int count = in->partition_count;

    if (rows <= 0 || cols <= 0 || count <= 0 || count > PARTITION_META_MAX || rows * cols != count)
    {
        current_partition_rows = 0;
        current_partition_cols = 0;
        current_partition_count = 0;
        reset_partition_population_metadata();
        for (int i = 0; i < PARTITION_META_MAX; ++i)
        {
            current_partition_modes[i] = QUAD_MODE_ROOMY;
            current_partition_densities[i] = DENSITY_NORMAL;
            current_partition_big_cave_types[i] = BIG_CAVE_NONE;
        }
        return;
    }

    remember_partition_grid(rows, cols, count);
    for (int i = 0; i < PARTITION_META_MAX; ++i)
    {
        quadrant_mode_t mode = QUAD_MODE_ROOMY;
        big_cave_type_t cave_type = BIG_CAVE_NONE;
        if (i < count)
        {
            byte raw = in->modes[i];
            if (raw <= QUAD_MODE_BIG_CAVE)
                mode = (quadrant_mode_t)raw;
            if (mode == QUAD_MODE_BIG_CAVE)
            {
                byte raw_type = in->big_cave_types[i];
                if (raw_type > BIG_CAVE_NONE && raw_type < BIG_CAVE_TYPE_MAX)
                    cave_type = (big_cave_type_t)raw_type;
            }
        }
        current_partition_modes[i] = mode;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = cave_type;
    }
}

int level_partition_index_for_point(int y, int x)
{
    /* Ensure partition metadata exists even for loaded levels. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        (void)partition_mode_for_point(y, x);

    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        return -1;

    int pi = partition_index_from_point(y, x, current_partition_rows, current_partition_cols);
    if (pi < 0 || pi >= current_partition_count)
        return -1;

    return pi;
}

big_cave_type_t level_partition_big_cave_type_for_index(int pi)
{
    if (pi < 0 || pi >= current_partition_count)
        return BIG_CAVE_NONE;
    if (current_partition_modes[pi] != QUAD_MODE_BIG_CAVE)
        return BIG_CAVE_NONE;
    return current_partition_big_cave_types[pi];
}

big_cave_type_t level_partition_big_cave_type_for_point(int y, int x)
{
    if (suppress_partition_effects_for_point(y, x))
        return BIG_CAVE_NONE;

    int pi = level_partition_index_for_point(y, x);
    if (pi < 0)
        return BIG_CAVE_NONE;
    return level_partition_big_cave_type_for_index(pi);
}

void log_partition_debug_for_point(const char* tag, int y, int x)
{
    const char* label = tag ? tag : "partition_debug";
    const bool in_bounds = in_bounds_fully(y, x);
    const bool suppressed = in_bounds && suppress_partition_effects_for_point(y, x);
    int pi = -1;
    int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
    quadrant_mode_t raw_mode = QUAD_MODE_ROOMY;
    level_partition_kind eff_kind = LEVEL_PART_NONE;
    big_cave_type_t raw_big_cave = BIG_CAVE_NONE;
    big_cave_type_t eff_big_cave = BIG_CAVE_NONE;

    if (!in_bounds)
    {
        log_debug("%s: point=(%d,%d) out_of_bounds", label, y, x);
        return;
    }

    if (current_partition_rows <= 0 || current_partition_cols <= 0
        || current_partition_count <= 0)
    {
        (void)partition_mode_for_point(y, x);
    }

    pi = level_partition_index_for_point(y, x);
    if (pi >= 0 && pi < current_partition_count)
    {
        raw_mode = current_partition_modes[pi];
        raw_big_cave = current_partition_big_cave_types[pi];
        (void)compute_partition_bounds(pi, current_partition_rows,
            current_partition_cols, &y1, &y2, &x1, &x2);
    }

    eff_kind = level_partition_kind_for_point(y, x);
    eff_big_cave = level_partition_big_cave_type_for_point(y, x);

    log_debug(
        "%s: point=(%d,%d) pi=%d grid=%dx%d/%d bounds=(%d,%d)-(%d,%d) raw_mode=%s raw_big_cave=%s effective_kind=%s effective_big_cave=%s suppressed=%d room=%d gvault=%d morgoth_tunnel=%d feat=%d cave_info=0x%08X",
        label, y, x, pi, current_partition_rows, current_partition_cols,
        current_partition_count, y1, x1, y2, x2,
        quadrant_mode_debug_name(raw_mode),
        big_cave_type_debug_name(raw_big_cave),
        partition_kind_debug_name(eff_kind),
        big_cave_type_debug_name(eff_big_cave), suppressed ? 1 : 0,
        (cave_info[y][x] & CAVE_ROOM) ? 1 : 0,
        (cave_info[y][x] & CAVE_G_VAULT) ? 1 : 0,
        (cave_info[y][x] & CAVE_MORGOTH_TUNNEL) ? 1 : 0,
        cave_feat[y][x], (unsigned int)cave_info[y][x]);
}

void level_layout_info_current(level_layout_info* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    out->map_wid = p_ptr->cur_map_wid;
    out->map_hgt = p_ptr->cur_map_hgt;
    out->partition_rows = current_partition_rows;
    out->partition_cols = current_partition_cols;
    out->partition_count = current_partition_count;

    int area_by_kind[LEVEL_PART_MAX] = {0};

    for (int i = 0; i < current_partition_count; ++i)
    {
        level_partition_kind kind = partition_kind_from_mode(current_partition_modes[i]);
        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        int area = 0;

        if (compute_partition_bounds(
                i, current_partition_rows, current_partition_cols, &y1, &y2, &x1, &x2))
        {
            area = (y2 - y1 + 1) * (x2 - x1 + 1);
        }

        if (kind == LEVEL_PART_LABYRINTH)
            out->labyrinth_parts++;
        else if (kind == LEVEL_PART_BIG_CAVE)
            out->big_cave_parts++;
        else if (kind == LEVEL_PART_CHASM)
            out->chasm_parts++;

        if (kind > LEVEL_PART_NONE && kind < LEVEL_PART_MAX)
            area_by_kind[kind] += area;
    }

    const level_partition_kind preference[] = {LEVEL_PART_LABYRINTH,
        LEVEL_PART_BIG_CAVE, LEVEL_PART_CHASM, LEVEL_PART_RUINED,
        LEVEL_PART_CAVEY, LEVEL_PART_ROOMY};

    int dominant_area = 0;
    level_partition_kind dominant_kind = LEVEL_PART_NONE;
    for (size_t i = 0; i < N_ELEMENTS(preference); ++i)
    {
        level_partition_kind kind = preference[i];
        int area = area_by_kind[kind];
        if (area > dominant_area)
        {
            dominant_area = area;
            dominant_kind = kind;
        }
    }

    out->dominant_kind = dominant_kind;
}

partition_drop_profile partition_drop_profile_for_mode(quadrant_mode_t mode)
{
    partition_drop_profile prof;
    prof.allow_floor_drops = true;
    drop_profile_default(&prof.profile);

    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        /* Default (ROOMY) -- 40:30:10:20 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 30;
        prof.profile.weight_jewelry = 10;
        prof.profile.weight_supply = 20;
        prof.profile.supply_potion = 2;
        prof.profile.supply_herb = 2;
        prof.profile.supply_gem = 2;
        prof.profile.supply_staff = 2;
        prof.profile.supply_light = 1;
        prof.profile.supply_arrows = 1;
        break;
    case QUAD_MODE_LABYRINTH:
        /* LABYRINTH - 0:0:35:65 */
        prof.profile.weight_weapon = 0;
        prof.profile.weight_armor = 0;
        prof.profile.weight_jewelry = 35;
        prof.profile.weight_supply = 65;
        prof.profile.supply_potion = 30;
        prof.profile.supply_herb = 4;
        prof.profile.supply_gem = 4;
        prof.profile.supply_staff = 30;
        prof.profile.supply_light = 5;
        prof.profile.supply_arrows = 5;
        break;
    case QUAD_MODE_RUINED:
        /* RUINED 40:35:0:25 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 35;
        prof.profile.weight_jewelry = 0;
        prof.profile.weight_supply = 25;
        prof.profile.supply_potion = 14;
        prof.profile.supply_herb = 4;
        prof.profile.supply_gem = 2;
        prof.profile.supply_staff = 6;
        prof.profile.supply_light = 15;
        prof.profile.supply_arrows = 15; /* arrows plus misc leftovers */
        prof.profile.supply_tunneling = 4; /* small chance for shovels/mattocks */
        prof.profile.allow_damaged = true;
        break;
    case QUAD_MODE_CAVEY:
        prof.profile.weight_weapon = 0;
        prof.profile.weight_armor = 0;
        prof.profile.weight_jewelry = 0;
        prof.profile.weight_supply = 100;
        prof.profile.supply_potion = 0;
        prof.profile.supply_herb = 0;
        prof.profile.supply_gem = 24;
        prof.profile.supply_staff = 6;
        prof.profile.supply_light = 6;
        prof.profile.supply_arrows = 6;
        prof.profile.supply_tunneling = 2;
        break;
    case QUAD_MODE_BIG_CAVE:
        /* BIG_CAVE 20:20:15:45 */
        prof.profile.weight_weapon = 20;
        prof.profile.weight_armor = 20;
        prof.profile.weight_jewelry = 15;
        prof.profile.weight_supply = 45;
        prof.profile.supply_potion = 0;
        prof.profile.supply_herb = 4;
        prof.profile.supply_gem = 16;
        prof.profile.supply_staff = 4;
        prof.profile.supply_light = 0;
        prof.profile.supply_arrows = 0;
        break;
    case QUAD_MODE_CHASM:
        /* CHASM 40:30:20:10 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 30;
        prof.profile.weight_jewelry = 20;
        prof.profile.weight_supply = 10;
        prof.profile.supply_potion = 2;
        prof.profile.supply_herb = 2;
        prof.profile.supply_gem = 2;
        prof.profile.supply_staff = 2;
        prof.profile.supply_light = 1;
        prof.profile.supply_arrows = 1;
        break;
    default:
        break;
    }

    return prof;
}

drop_profile drop_profile_for_mode(quadrant_mode_t mode)
{
    partition_drop_profile prof = partition_drop_profile_for_mode(mode);
    return prof.profile;
}

void drop_profile_for_partition_kind(level_partition_kind kind, drop_profile* out)
{
    drop_profile_for_partition_kind_source(
        kind, PARTITION_DROP_SOURCE_FLOOR, out);
}

partition_drop_profile partition_drop_profile_for_kind_source_cfg(
    level_partition_kind kind, partition_drop_source_t source)
{
    const partition_rule_config* cfg = partition_config_get(kind);
    partition_drop_profile prof;

    drop_profile_default(&prof.profile);
    prof.allow_floor_drops = true;

    if (cfg && source >= PARTITION_DROP_SOURCE_FLOOR
        && source < PARTITION_DROP_SOURCE_MAX)
    {
        prof.profile = cfg->profiles[source];
        if (source == PARTITION_DROP_SOURCE_FLOOR)
        {
            prof.allow_floor_drops = cfg->allow_floor_drops;
        }
    }

    return prof;
}

partition_drop_profile partition_drop_profile_for_mode_source_cfg(
    quadrant_mode_t mode, partition_drop_source_t source)
{
    return partition_drop_profile_for_kind_source_cfg(
        partition_kind_from_mode(mode), source);
}

void drop_profile_for_partition_kind_source(level_partition_kind kind,
    partition_drop_source_t source, drop_profile* out)
{
    if (!out)
        return;

    *out = partition_drop_profile_for_kind_source_cfg(kind, source).profile;
}

void place_object_with_profile_params(
    int y, int x, int base_depth, int min_depth_penalty_depth,
    drop_quality quality, int droptype, bool allow_artefacts,
    int artefact_weight_multiplier, u32b extra_ident,
    const partition_drop_profile* prof);

void place_object_with_profile(
    int y, int x, const partition_drop_profile* prof)
{
    place_object_with_profile_params(
        y, x, object_level, object_level, DROP_QUALITY_NORMAL, DROP_TYPE_UNTHEMED,
        false, 1, 0, prof);
}

void place_object_with_profile_params(
    int y, int x, int base_depth, int min_depth_penalty_depth,
    drop_quality quality, int droptype, bool allow_artefacts,
    int artefact_weight_multiplier, u32b extra_ident,
    const partition_drop_profile* prof)
{
    if (!in_bounds(y, x))
        return;
    if (!cave_clean_bold(y, x))
        return;

    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    object_wipe(i_ptr);

    int attempts = 0;
    const drop_profile* dp = (prof) ? &prof->profile : NULL;

    while (!drop_generate_object_profiled_depths_biased(base_depth,
               min_depth_penalty_depth, quality, droptype, 0, allow_artefacts,
               artefact_weight_multiplier, dp, i_ptr))
    {
        attempts++;
        if (attempts > 200)
            return;
    }

    if (i_ptr->tval == TV_CHEST)
        i_ptr->xtra1 = (byte)(0x80 | (byte)level_partition_kind_for_point(y, x));
    if (extra_ident)
        i_ptr->ident |= extra_ident;

    if (!floor_carry(y, x, i_ptr))
    {
        a_info[i_ptr->name1].cur_num = 0;
    }
}

int partition_metal_drop_target(quadrant_mode_t mode, int floor_count,
    int depth)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));
    const partition_metal_rule* rule = cfg ? &cfg->metal_drops : NULL;
    int target = 0;

    if (floor_count <= 0)
        return 0;
    if (!rule || rule->divisor <= 0)
        return 0;
    if (depth < rule->min_depth)
        return 0;

    target = floor_count / rule->divisor;
    if (target < rule->min_count)
        target = rule->min_count;
    if (rule->max_count > 0 && target > rule->max_count)
        target = rule->max_count;

    return target;
}

s16b partition_metal_kind_for_mode(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_CAVEY:
    case QUAD_MODE_BIG_CAVE:
        return lookup_kind(TV_METAL, SV_METAL_MITHRIL);
    case QUAD_MODE_CHASM:
        return lookup_kind(TV_METAL, SV_METAL_STAR_IRON);
    default:
        return 0;
    }
}

bool partition_metal_tile_ok(const partition_population_plan* plan,
    int y, int x, bool require_chasm_tag)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (level_partition_index_for_point(y, x) != plan->pi)
        return false;
    if (cave_info[y][x] & CAVE_G_VAULT)
        return false;
    if (!partition_population_naked_bold(plan->mode, y, x))
        return false;
    if (plan->mode == QUAD_MODE_CHASM && require_chasm_tag
        && !(cave_info[y][x] & CAVE_CHASM_AREA))
    {
        return false;
    }

    return true;
}

int place_partition_metal_drops(const partition_population_plan* plan)
{
    int target;
    int placed = 0;
    s16b k_idx;
    bool require_chasm_tag;

    if (!plan)
        return 0;

    target = partition_metal_drop_target(plan->mode,
        (plan->floor_count_non_vault > 0) ? plan->floor_count_non_vault
                                          : plan->floor_count,
        p_ptr->depth);
    if (target <= 0)
        return 0;

    k_idx = partition_metal_kind_for_mode(plan->mode);
    if (k_idx <= 0)
        return 0;

    require_chasm_tag = (plan->mode == QUAD_MODE_CHASM)
        && bounds_have_chasm_tag(plan->y1, plan->y2, plan->x1, plan->x2);

    for (int n = 0; n < target; ++n)
    {
        bool placed_this = false;

        for (int tries = 0; tries < 200; ++tries)
        {
            int y = rand_range(plan->y1, plan->y2);
            int x = rand_range(plan->x1, plan->x2);
            object_type object_type_body;
            object_type* i_ptr = &object_type_body;

            if (!partition_metal_tile_ok(plan, y, x, require_chasm_tag))
                continue;

            object_wipe(i_ptr);
            object_prep(i_ptr, k_idx);
            i_ptr->number = 1;

            if (floor_carry(y, x, i_ptr))
            {
                placed++;
                placed_this = true;
                break;
            }
        }

        if (!placed_this)
            break;
    }

    if (placed > 0)
    {
        log_trace("Partition metal drops: pi=%d mode=%d placed=%d depth=%d",
            plan->pi, plan->mode, placed, p_ptr ? p_ptr->depth : 0);
    }

    return placed;
}
