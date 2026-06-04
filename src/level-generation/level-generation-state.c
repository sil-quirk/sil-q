/* File: level-generation-state.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

/*
 * Note that Level generation is *not* an important bottleneck,
 * though it can be annoyingly slow on older machines...  Thus
 * we emphasize "simplicity" and "correctness" over "speed".
 *
 * This entire file is only needed for generating levels.
 * This may allow smart compilers to only load it when needed.
 *
 * Consider the "vault.txt" file for vault generation.
 *
 * In this file, we use the "special" granite and perma-wall sub-types,
 * where "basic" is normal, "inner" is inside a room, "outer" is the
 * outer wall of a room, and "solid" is the outer wall of the dungeon
 * or any walls that may not be pierced by corridors.
 *
 * Note that the cave grid flags changed in a rather drastic manner
 * for Angband 2.8.0 (and 2.7.9+), in particular, dungeon terrain
 * features, such as doors and stairs and traps and rubble and walls,
 * are all handled as a set of 64 possible "terrain features", and
 * not as "fake" objects (440-479) as in pre-2.8.0 versions.
 *
 * The 64 new "dungeon features" will also be used for "visual display"
 * but we must be careful not to allow, for example, the user to display
 * hidden traps in a different way from floors, or secret doors in a way
 * different from granite walls, or even permanent granite in a different
 * way from granite.  XXX XXX XXX
 *
 * Sil notes:
 *
 * I do not make any use of "solid" walls, but have left the type in.
 * The code previously used a lot of 11x11 blocks in room generation.
 * I have mostly removed references to this now.
 * The rooms are now placed at random in the dungeon.
 * The corridor generation has been simplified a lot for aesthetic purposes.
 * Note that level generation can fail (if the level is unconnected, or for
 * other reasons) and that each room and corridor generation can fail too. This
 * is not a problem as they are generated until success and often succeed.
 */

/*
 * Dungeon generation values
 */

#define DUN_DEST 1 /* 1/chance of having a destroyed level */

/*
 * Dungeon streamer generation values
 */
#define DUN_STR_DEN 5 /* Density of streamers */
#define DUN_STR_RNG 2 /* Width of streamers */
#define DUN_STR_QUA 4 /* Number of quartz streamers */

/*
 * Dungeon treausre allocation values
 */
#define DUN_OBJ_CHANCE_ROOM 30 /* determines number of items found in rooms */
#define DUN_OBJ_CHANCE_BOTH                                                    \
    5 /* determines number of items found in rooms/corridors */

/*
 * Hack -- Dungeon allocation "places"
 */
#define ALLOC_SET_CORR 1 /* Hallway */
#define ALLOC_SET_ROOM 2 /* Room */
#define ALLOC_SET_BOTH 3 /* Anywhere */

/*
 * Hack -- Dungeon allocation "types"
 */
#define ALLOC_TYP_RUBBLE 1 /* Rubble */
#define ALLOC_TYP_OBJECT 5 /* Object */

/*
 * Maximum numbers of rooms along each axis (currently 6x18)
 */

#define MAX_ROOMS_ROW (MAX_DUNGEON_HGT / BLOCK_HGT)
#define MAX_ROOMS_COL (MAX_DUNGEON_WID / BLOCK_WID)

/*
 * Bounds on some arrays used in the "dun_data" structure.
 * These bounds are checked, though usually this is a formality.
 */
#define CENT_MAX DUN_ROOMS  /* Keep room storage in lockstep with connection matrix */
#define DOOR_MAX 200
#define WALL_MAX 500
#define TUNN_MAX 900

bool allow_uniques;

/*
 * Maximal number of room types
 */
#define ROOM_MAX 12
#define ROOM_MIN 2

/*
 * Simple structure to hold a map location
 */


/*
 * Dungeon generation data -- see "cave_gen()"
 */
dun_data dun_body;
dun_data* dun = &dun_body;

/*
 * Array[DUNGEON_HGT][DUNGEON_WID].
 * Each corridor square it is marked for each room that it connects.
 */
int cave_corridor1[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
int cave_corridor2[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
bool cave_escape_tunnel[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

#define LAYOUT_ANCHOR_MAX CENT_MAX


layout_anchor_t layout_anchors[LAYOUT_ANCHOR_MAX];
int layout_anchor_count = 0;
layout_anchor_kind_t room_anchor_kind[CENT_MAX];
bool room_anchor_requires_neighbor[CENT_MAX];

/* Shared guard to prevent room/anchor writes past allocated arrays/connection matrix */


#define LABYRINTH_START_DEPTH 7
#define BIG_CAVE_START_DEPTH 10
#define CHASM_START_DEPTH 14
#define PARTITION_THEME_MONSTER_PERCENT 80
#define PARTITION_THEME_LEVEL_DELTA 2
#define CHASM_WHISPERING_SHADOW_TARGET 8
#define CHASM_AMBUSH_UNIQUE_SUB_PERCENT 10
#define SPECIAL_CAP_STEP 5
#define SPECIAL_CAP_MAX 3
/* Special-mode depth gates and caps (tweak to rebalance rarity) */

/* Persist the chosen partition grid for later passes (connectivity, stair guarantees) */
int current_partition_rows = 0;
int current_partition_cols = 0;
int current_partition_count = 0;
quadrant_mode_t current_partition_modes[25];
density_level_t current_partition_densities[25];
big_cave_type_t current_partition_big_cave_types[25];
int current_partition_bridge_styles[25];
void reset_partition_population_metadata(void);
partition_population_meta current_partition_population_meta[25];

void init_partition_chest_recipe(partition_chest_recipe* recipe)
{
    if (!recipe)
        return;

    recipe->chest_mode = 0;
    recipe->material_wood_pct = -1;
    recipe->material_steel_pct = -1;
    recipe->material_jewel_pct = -1;
    recipe->anchor_pref = PARTITION_CHEST_ANCHOR_ANY;
}

void set_partition_chest_recipe(partition_population_meta* meta, int slot,
    int chest_mode, int wooden_pct, int steel_pct, int jewel_pct,
    partition_chest_anchor_pref anchor_pref)
{
    if (!meta || slot < 0 || slot >= PARTITION_CHEST_RECIPE_MAX)
        return;

    meta->chest_recipes[slot].chest_mode = (byte)chest_mode;
    meta->chest_recipes[slot].material_wood_pct = (s16b)wooden_pct;
    meta->chest_recipes[slot].material_steel_pct = (s16b)steel_pct;
    meta->chest_recipes[slot].material_jewel_pct = (s16b)jewel_pct;
    meta->chest_recipes[slot].anchor_pref = (byte)anchor_pref;
    if (meta->chest_count < slot + 1)
        meta->chest_count = slot + 1;
}

const char* level_gen_partition_mode_name(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return "Roomy";
    case QUAD_MODE_CAVEY:
        return "Cavey";
    case QUAD_MODE_RUINED:
        return "Ruined";
    case QUAD_MODE_LABYRINTH:
        return "Labyrinth";
    case QUAD_MODE_CHASM:
        return "Chasm";
    case QUAD_MODE_BIG_CAVE:
        return "Big cave";
    }

    return "Unknown";
}

void level_gen_screen_append_list_item(char* buf, size_t buflen,
    cptr text)
{
    char tmp[256];

    if (!buf || buflen == 0 || !text || !text[0])
        return;

    if (buf[0])
        strnfmt(tmp, sizeof(tmp), "%s, %s", buf, text);
    else
        strnfmt(tmp, sizeof(tmp), "%s", text);

    SDL_strlcpy(buf, tmp, buflen);
}

void level_gen_screen_build_partition_summary(char* total_buf,
    size_t total_buflen, char* types_buf, size_t types_buflen)
{
    int mode_counts[QUAD_MODE_BIG_CAVE + 1] = {0};
    int total = MIN(current_partition_count, 25);
    int distinct = 0;

    if (total_buflen > 0)
        total_buf[0] = '\0';
    if (types_buflen > 0)
        types_buf[0] = '\0';

    if (total <= 0)
    {
        SDL_strlcpy(total_buf, "Partitions: (pending)", total_buflen);
        return;
    }

    for (int i = 0; i < total; ++i)
    {
        int mode = current_partition_modes[i];

        if (mode < QUAD_MODE_ROOMY || mode > QUAD_MODE_BIG_CAVE)
            mode = QUAD_MODE_ROOMY;

        if (mode_counts[mode]++ == 0)
            distinct++;
    }

    strnfmt(total_buf, total_buflen, "Partitions: %d total, %d type%s",
        total, distinct, (distinct == 1) ? "" : "s");

    for (int mode = QUAD_MODE_ROOMY; mode <= QUAD_MODE_BIG_CAVE; ++mode)
    {
        char item[64];

        if (mode_counts[mode] <= 0)
            continue;

        strnfmt(item, sizeof(item), "%d %s", mode_counts[mode],
            level_gen_partition_mode_name((quadrant_mode_t)mode));
        level_gen_screen_append_list_item(types_buf, types_buflen, item);
    }

    if (types_buf[0])
    {
        char mix[256];

        strnfmt(mix, sizeof(mix), "Type mix: %s", types_buf);
        SDL_strlcpy(types_buf, mix, types_buflen);
    }
}

/* Per-depth big cave type weights (ICE/FIRE/POIS); when unset, default to equal odds. */
bool g_big_cave_type_rule_set[32];
int g_big_cave_type_weight[32][BIG_CAVE_TYPE_MAX];

/* Track labyrinth partition count for boosting monsters and stairs in mazes */
int current_labyrinth_partitions = 0;

/* Morgoth throne room placement state for the current generation attempt */
bool morgoth_level_active = false;
bool morgoth_partition_reserved = false;
int morgoth_partition_index = -1;
rectangle morgoth_partition_bounds;
int morgoth_vault_center_y = 0;
int morgoth_vault_center_x = 0;

bool morgoth_region_active(void)
{
    return morgoth_level_active && morgoth_partition_reserved;
}

bool coord_in_morgoth_region(int y, int x, int margin)
{
    if (!morgoth_region_active())
        return false;

    return (y >= morgoth_partition_bounds.y1 - margin)
        && (y <= morgoth_partition_bounds.y2 + margin)
        && (x >= morgoth_partition_bounds.x1 - margin)
        && (x <= morgoth_partition_bounds.x2 + margin);
}

/* Axis-aligned segment vs. Morgoth region intersection */
bool morgoth_segment_blocked(int y1, int x1, int y2, int x2, int margin)
{
    if (!morgoth_region_active())
        return false;

    /* Quick reject if both points are completely outside in same half-plane */
    if (y1 < morgoth_partition_bounds.y1 - margin && y2 < morgoth_partition_bounds.y1 - margin)
        return false;
    if (y1 > morgoth_partition_bounds.y2 + margin && y2 > morgoth_partition_bounds.y2 + margin)
        return false;
    if (x1 < morgoth_partition_bounds.x1 - margin && x2 < morgoth_partition_bounds.x1 - margin)
        return false;
    if (x1 > morgoth_partition_bounds.x2 + margin && x2 > morgoth_partition_bounds.x2 + margin)
        return false;

    /* Horizontal segment */
    if (y1 == y2)
    {
        int y = y1;
        int xa = MIN(x1, x2);
        int xb = MAX(x1, x2);
        int rx1 = morgoth_partition_bounds.x1 - margin;
        int rx2 = morgoth_partition_bounds.x2 + margin;
        int ry1 = morgoth_partition_bounds.y1 - margin;
        int ry2 = morgoth_partition_bounds.y2 + margin;
        if (y >= ry1 && y <= ry2 && xb >= rx1 && xa <= rx2)
            return true;
    }

    /* Vertical segment */
    if (x1 == x2)
    {
        int x = x1;
        int ya = MIN(y1, y2);
        int yb = MAX(y1, y2);
        int rx1 = morgoth_partition_bounds.x1 - margin;
        int rx2 = morgoth_partition_bounds.x2 + margin;
        int ry1 = morgoth_partition_bounds.y1 - margin;
        int ry2 = morgoth_partition_bounds.y2 + margin;
        if (x >= rx1 && x <= rx2 && yb >= ry1 && ya <= ry2)
            return true;
    }

    return false;
}

void big_cave_type_rules_clear(void)
{
    for (int d = 0; d < 32; ++d) {
        g_big_cave_type_rule_set[d] = false;
        for (int t = 0; t < BIG_CAVE_TYPE_MAX; ++t)
            g_big_cave_type_weight[d][t] = 0;
    }
}

void big_cave_type_set_rule(int depth, int ice_weight, int fire_weight, int pois_weight)
{
    if (depth < 0 || depth >= 32) return;
    g_big_cave_type_rule_set[depth] = true;
    g_big_cave_type_weight[depth][BIG_CAVE_ICE] = MAX(0, ice_weight);
    g_big_cave_type_weight[depth][BIG_CAVE_FIRE] = MAX(0, fire_weight);
    g_big_cave_type_weight[depth][BIG_CAVE_POIS] = MAX(0, pois_weight);
}

big_cave_type_t big_cave_type_pick_for_depth(int depth)
{
    int d = depth;
    if (d < 0) d = 0;
    if (d >= 32) d = 31;

    int ice_w = 1;
    int fire_w = 1;
    int pois_w = 1;
    if (g_big_cave_type_rule_set[d]) {
        ice_w = g_big_cave_type_weight[d][BIG_CAVE_ICE];
        fire_w = g_big_cave_type_weight[d][BIG_CAVE_FIRE];
        pois_w = g_big_cave_type_weight[d][BIG_CAVE_POIS];
    }

    int total = ice_w + fire_w + pois_w;
    if (total <= 0) {
        ice_w = fire_w = pois_w = 1;
        total = 3;
    }

    int r = rand_int(total);
    if (r < ice_w) return BIG_CAVE_ICE;
    r -= ice_w;
    if (r < fire_w) return BIG_CAVE_FIRE;
    return BIG_CAVE_POIS;
}

/* After placing Morgoth's vault, seal a small buffer around it with permanent walls.
 * This prevents accidental extra entrances while keeping the rest of the reserved
 * partition traversable for normal dungeon connectivity. */
void seal_morgoth_partition(const vault_type* v_ptr, int y0, int x0)
{
    if (!morgoth_region_active() || !v_ptr)
        return;

    int top_y = y0 - v_ptr->hgt / 2;
    int bot_y = top_y + v_ptr->hgt - 1;
    int left_x = x0 - v_ptr->wid / 2;
    int right_x = left_x + v_ptr->wid - 1;

    /* Extend sealing bounds to include the full tunnel path northward.
     * Tunnels are carved to partition_bounds.y1 - 2, so seal from there. */
    int tunnel_limit = morgoth_partition_bounds.y1 - 2;
    if (tunnel_limit < 1) tunnel_limit = 1;

    int margin = 4;
    int y1 = MAX(1, MIN(morgoth_partition_bounds.y1, tunnel_limit));  /* Include tunnel area */
    int y2 = MIN(morgoth_partition_bounds.y2, bot_y + margin);
    int x1 = MAX(morgoth_partition_bounds.x1, left_x - margin);
    int x2 = MIN(morgoth_partition_bounds.x2, right_x + margin);

    /* Update the active Morgoth "no-go" bounds to protect the tunnels too. */
    morgoth_partition_bounds.y1 = y1;
    morgoth_partition_bounds.y2 = y2;
    morgoth_partition_bounds.x1 = x1;
    morgoth_partition_bounds.x2 = x2;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            /* Preserve the vault area and any carved entry tunnels */
            if (cave_info[y][x] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
                continue;

            /* Don't seal over existing passable floor - only convert walls/granite.
             * This prevents trapping unreachable floor areas that were built before
             * the vault placement. */
            if (cave_floor_bold(y, x) || (cave_feat[y][x] >= FEAT_DOOR_HEAD && cave_feat[y][x] <= FEAT_DOOR_TAIL))
                continue;

            cave_set_feat(y, x, FEAT_WALL_PERM);
            cave_info[y][x] &= ~(CAVE_ROOM | CAVE_ICKY);
        }
    }
}

bool room_kind_is_vault(byte kind)
{
    return (kind >= ROOM_KIND_INTERESTING);
}

bool area_is_basic_granite(int y1, int x1, int y2, int x2)
{
    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT || y1 < 0 || x1 < 0)
        return false;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                return false;
        }
    }
    return true;
}

void remember_partition_grid(int rows, int cols, int count)
{
    current_partition_rows = rows;
    current_partition_cols = cols;
    current_partition_count = count;
    reset_partition_population_metadata();
    for (int i = 0; i < 25; ++i)
    {
        current_partition_modes[i] = QUAD_MODE_ROOMY;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = BIG_CAVE_NONE;
        current_partition_bridge_styles[i] = -1;
    }
}

void record_partition_metadata(
    const quadrant_mode_t* modes, const density_level_t* densities, int count)
{
    int capped = MIN(count, 25);
    for (int i = 0; i < capped; ++i)
    {
        current_partition_modes[i] = modes[i];
        current_partition_densities[i] = densities[i];
    }
}

void reset_morgoth_layout_state(bool active)
{
    morgoth_level_active = active;
    morgoth_partition_reserved = false;
    morgoth_partition_index = -1;
    morgoth_partition_bounds.y1 = 0;
    morgoth_partition_bounds.y2 = 0;
    morgoth_partition_bounds.x1 = 0;
    morgoth_partition_bounds.x2 = 0;
    morgoth_vault_center_y = 0;
    morgoth_vault_center_x = 0;
}

void fallback_partition_grid_from_blocks(int blocks, int *rows, int *cols)
{
    /* Mirror the choices in apply_quadrant_generation_modes but without randomness */
    if (blocks <= 9) {
        *rows = 2; *cols = 2;
    } else if (blocks == 10) {
        *rows = 3; *cols = 2;
    } else if (blocks <= 13) {
        *rows = 3; *cols = 3;
    } else if (blocks == 14) {
        *rows = 3; *cols = 4;
    } else if (blocks <= 16) {
        *rows = 4; *cols = 4;
    } else if (blocks <= 20) {
        *rows = 5; *cols = 4;
    } else {
        *rows = 5; *cols = 5;
    }
}

/* Quick scan to see if a region is already reserved/occupied heavily (quest vaults, prefab icky) */
bool area_is_reserved_or_dense(int y1, int y2, int x1, int x2, int *floor_pct_out, int *icky_pct_out)
{
    int tiles = 0, icky = 0, floors = 0;
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            if (!in_bounds_fully(y, x)) continue;
            tiles++;
            if (cave_info[y][x] & CAVE_ICKY) icky++;
            if (cave_floor_bold(y, x)) floors++;
        }
    }
    int floor_pct = (tiles > 0) ? (floors * 100) / tiles : 0;
    int icky_pct = (tiles > 0) ? (icky * 100) / tiles : 0;
    if (floor_pct_out) *floor_pct_out = floor_pct;
    if (icky_pct_out) *icky_pct_out = icky_pct;

    /* Only treat as reserved if the area is heavily occupied */
    if (floor_pct >= 80) return true;       /* >80% carved already */
    if (icky_pct >= 60) return true;        /* quest/vault dominates area */
    return false;
}

/* Bounded placement helper used by partitions (prototype for early use) */
bool room_build_in_bounds(int typ, int y1, int y2, int x1, int x2);

/* Partition helper: compute bounds for a given partition index */
bool compute_partition_bounds(int pi, int rows, int cols, int *y1, int *y2, int *x1, int *x2)
{
    if (rows <= 0 || cols <= 0)
        return false;
    int total = rows * cols;
    if (pi < 0 || pi >= total)
        return false;

    int row = pi / cols;
    int col = pi % cols;

    /* Keep inclusive partition bounds aligned with partition_index_from_point():
     * each partition owns [start, next_start), so the inclusive max is next_start - 1.
     * Without that, adjacent partitions overlap by one row/column during carving
     * while runtime lookups only assign those tiles to one side. */
    int ly1 = (row * p_ptr->cur_map_hgt / rows);
    int ly2 = (((row + 1) * p_ptr->cur_map_hgt) / rows) - 1;
    int lx1 = (col * p_ptr->cur_map_wid / cols);
    int lx2 = (((col + 1) * p_ptr->cur_map_wid) / cols) - 1;

    if (ly1 < 1) ly1 = 1;
    if (lx1 < 1) lx1 = 1;
    if (ly2 >= p_ptr->cur_map_hgt - 1) ly2 = p_ptr->cur_map_hgt - 2;
    if (lx2 >= p_ptr->cur_map_wid - 1) lx2 = p_ptr->cur_map_wid - 2;
    if (ly2 < ly1 || lx2 < lx1)
        return false;

    *y1 = ly1;
    *y2 = ly2;
    *x1 = lx1;
    *x2 = lx2;
    return true;
}

bool level_has_chasm_partition(void)
{
    for (int pi = 0; pi < current_partition_count; ++pi)
    {
        if (current_partition_modes[pi] == QUAD_MODE_CHASM)
            return true;
    }

    return false;
}

/* Normalize CAVE_CHASM_AREA so it only marks the actual chasm footprint and
 * the native platform/bridge floor inside chasm partitions. Tagging whole
 * partition rectangles or failed chasm fallbacks made non-chasm tiles render
 * and light like chasms. */
void apply_chasm_partition_tags(void)
{
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        return;

    for (int pi = 0; pi < current_partition_count; ++pi)
    {
        bool is_chasm_partition = (current_partition_modes[pi] == QUAD_MODE_CHASM);

        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        if (!compute_partition_bounds(pi, current_partition_rows, current_partition_cols, &y1, &y2, &x1, &x2))
            continue;

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                if (!in_bounds(y, x))
                    continue;

                if (is_chasm_partition
                    && (cave_feat[y][x] == FEAT_CHASM
                    || ((cave_info[y][x] & (CAVE_ROOM | CAVE_CHASM_AREA))
                        == (CAVE_ROOM | CAVE_CHASM_AREA))))
                {
                    cave_info[y][x] |= CAVE_CHASM_AREA;
                }
                else
                {
                    cave_info[y][x] &= ~CAVE_CHASM_AREA;
                }
            }
        }
    }
}

/* Gameplay lighting rules:
 * - Labyrinth partitions are always dark (no permanent CAVE_GLOW).
 * - CA_BLOB rooms ("caves") are always dark (no permanent CAVE_GLOW). */
void apply_partition_and_room_glow_rules(void)
{
    /* Labyrinth partitions: clear CAVE_GLOW across full partition bounds. */
    if (current_partition_rows > 0 && current_partition_cols > 0 && current_partition_count > 0)
    {
        for (int pi = 0; pi < current_partition_count; ++pi)
        {
            if (current_partition_modes[pi] != QUAD_MODE_LABYRINTH)
                continue;

            int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
            if (!compute_partition_bounds(pi, current_partition_rows, current_partition_cols, &y1, &y2, &x1, &x2))
                continue;

            for (int y = y1; y <= y2; ++y)
                for (int x = x1; x <= x2; ++x)
                    cave_info[y][x] &= ~(CAVE_GLOW);
        }
    }

    /* CA_BLOB rooms: clear CAVE_GLOW in room bounds (expanded by 1 to cover the
     * typical outer wall ring used by rectangular room builders). */
    for (int r = 0; r < dun->cent_n; ++r)
    {
        if (room_anchor_kind[r] != LAYOUT_ANCHOR_CA_BLOB)
            continue;

        int y1 = dun->corner[r].y1 - 1;
        int y2 = dun->corner[r].y2 + 1;
        int x1 = dun->corner[r].x1 - 1;
        int x2 = dun->corner[r].x2 + 1;

        if (y1 < 0) y1 = 0;
        if (x1 < 0) x1 = 0;
        if (y2 >= MAX_DUNGEON_HGT) y2 = MAX_DUNGEON_HGT - 1;
        if (x2 >= MAX_DUNGEON_WID) x2 = MAX_DUNGEON_WID - 1;

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                if (!in_bounds(y, x))
                    continue;
                cave_info[y][x] &= ~(CAVE_GLOW);
            }
        }
    }
}

bool place_chasm_theme_monster_at(int y, int x, int r_idx);
void awaken_chasm_sanctum_monster(int y, int x);

/* Gentle scaling helper: add ~50% per size tier (caps explosive growth) */
int scaled_attempts(int base, int area_factor)
{
    if (area_factor <= 1) return base;
    int extra = (base + 1) / 2;
    return base + extra * (area_factor - 1);
}

quadrant_mode_t pick_weighted_mode(const int *weights, int count)
{
    int total = 0;
    for (int i = 0; i < count; ++i)
        total += MAX(0, weights[i]);
    if (total <= 0)
        return QUAD_MODE_ROOMY;
    int roll = rand_int(total);
    for (int i = 0; i < count; ++i) {
        int w = MAX(0, weights[i]);
        if (roll < w)
            return (quadrant_mode_t)i;
        roll -= w;
    }
    return QUAD_MODE_ROOMY;
}

int special_mode_start_depth(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_LABYRINTH:
        return LABYRINTH_START_DEPTH;
    case QUAD_MODE_BIG_CAVE:
        return BIG_CAVE_START_DEPTH;
    case QUAD_MODE_CHASM:
        return CHASM_START_DEPTH;
    default:
        return 0;
    }
}

int mode_cap_for_depth(
    quadrant_mode_t mode, int depth, int partition_count)
{
    int start = special_mode_start_depth(mode);
    if (start <= 0)
        return partition_count;
    if (depth < start)
        return 0;

    int cap = 1 + (depth - start) / SPECIAL_CAP_STEP;
    if (cap > SPECIAL_CAP_MAX)
        cap = SPECIAL_CAP_MAX;
    return cap;
}

int mode_weight_for_depth(quadrant_mode_t mode, int depth, int blocks,
    const int* mode_counts, int partition_count)
{
    int cap = mode_cap_for_depth(mode, depth, partition_count);
    if (cap == 0)
        return 0;
    if (mode_counts && mode_counts[mode] >= cap)
        return 0;

    (void)blocks; /* No longer used for scaling */

    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return 25;
    case QUAD_MODE_CAVEY:
        /* Increase up to depth 12, then decrease */
        if (depth <= 12)
            return 15 + depth;  /* 15 at depth 0, 27 at depth 12 */
        else
            return MAX(5, 27 - (depth - 12));  /* Decrease after 12, minimum 5 */
    case QUAD_MODE_RUINED:
        /* Decrease with depth */
        return MAX(5, 15 + 10 - depth);  /* 25 at depth 0, decreases to minimum 5 */
    case QUAD_MODE_LABYRINTH:
        /* Increase with depth (starts at depth 7) */
        return 10 + MAX(0, depth - LABYRINTH_START_DEPTH);
    case QUAD_MODE_BIG_CAVE:
        /* Increase with depth (starts at depth 10) */
        return 8 + MAX(0, depth - BIG_CAVE_START_DEPTH);
    case QUAD_MODE_CHASM:
        /* Increase with depth (starts at depth 14) */
        return 8 + MAX(0, depth - CHASM_START_DEPTH);
    default:
        return 0;
    }
}

/* Budget-aware placement helper with limited retries */
bool place_room_with_budget(int typ, int y1, int y2, int x1, int x2, int max_tries, int depth,
    int *budget_t6, int *budget_t7, int *budget_t8, int *used_t6, int *used_t7, int *used_t8)
{
    int actual = typ;
    (void)depth; /* unused after removing GV promotion */

    if (actual == 8 && (!budget_t8 || *budget_t8 <= 0))
        actual = (budget_t7 && *budget_t7 > 0) ? 7 : ((budget_t6 && *budget_t6 > 0) ? 6 : 2);
    if (actual == 7 && budget_t7 && *budget_t7 <= 0)
        actual = (budget_t6 && *budget_t6 > 0) ? 6 : 2;
    if (actual == 6 && budget_t6 && *budget_t6 <= 0)
        actual = 2;  /* downgrade to simple cross room if out of budget */

    for (int attempt = 0; attempt < max_tries; ++attempt) {
        if (room_build_in_bounds(actual, y1, y2, x1, x2)) {
            if (actual == 6 && budget_t6 && *budget_t6 > 0) { (*budget_t6)--; if (used_t6) (*used_t6)++; }
            else if (actual == 7 && budget_t7 && *budget_t7 > 0) { (*budget_t7)--; if (used_t7) (*used_t7)++; }
            else if (actual == 8 && budget_t8 && *budget_t8 > 0) { (*budget_t8)--; if (used_t8) (*used_t8)++; }
            return true;
        }
    }
    return false;
}

/* Decode a style index from the color encoding at (y,x); returns -1 if none */
int style_at_color(int y, int x)
{
    if (y < 0 || x < 0 || y >= MAX_DUNGEON_HGT || x >= MAX_DUNGEON_WID)
        return -1;
    return styles_decode_color_style(cave_color[y][x]);
}

void layout_anchor_reset(void)
{
    layout_anchor_count = 0;
    for (int i = 0; i < LAYOUT_ANCHOR_MAX; ++i)
    {
        layout_anchors[i].kind = LAYOUT_ANCHOR_NONE;
        layout_anchors[i].style_primary = -1;
        layout_anchors[i].room_slot = -1;
        layout_anchors[i].requires_neighbor = false;
        layout_anchors[i].neighbor_linked = false;
    }
    for (int i = 0; i < CENT_MAX; ++i)
    {
        room_anchor_kind[i] = LAYOUT_ANCHOR_NONE;
        room_anchor_requires_neighbor[i] = false;
    }
}

void mark_room_anchor_meta(int room_idx, layout_anchor_kind_t kind, bool requires_neighbor)
{
    if (room_idx < 0 || room_idx >= CENT_MAX)
        return;
    room_anchor_kind[room_idx] = kind;
    room_anchor_requires_neighbor[room_idx] = requires_neighbor;
}

void layout_anchor_capture_room(int room_idx)
{
    if (layout_anchor_count >= LAYOUT_ANCHOR_MAX)
    {
        return;
    }

    layout_anchor_t* a = &layout_anchors[layout_anchor_count++];
    layout_anchor_kind_t kind = room_anchor_kind[room_idx];
    if (kind == LAYOUT_ANCHOR_NONE)
        kind = LAYOUT_ANCHOR_ROOM;
    a->kind = kind;
    a->bounds = dun->corner[room_idx];
    a->center = dun->cent[room_idx];
    a->room_kind = dun->kind[room_idx];
    a->is_quest = dun->is_quest[room_idx];
    a->style_primary = style_at_color(a->center.y, a->center.x);
    a->requires_neighbor = room_anchor_requires_neighbor[room_idx];
    a->neighbor_linked = false;
    a->room_slot = room_idx;
}

void layout_anchor_capture_existing_rooms(void)
{
    for (int i = 0; i < dun->cent_n; ++i)
    {
        layout_anchor_capture_room(i);
    }
}
