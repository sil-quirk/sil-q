/* File: level-generation-internal.h */
#ifndef INCLUDED_LEVEL_GENERATION_INTERNAL_H
#define INCLUDED_LEVEL_GENERATION_INTERNAL_H

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "gen-log.h"
#include "metarun.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

/* Quest vault debug instrumentation */
#define DEBUG_QUEST_VAULT 0

/* Dungeon generation values */
#define DUN_DEST 1
#define DUN_STR_DEN 5
#define DUN_STR_RNG 2
#define DUN_STR_QUA 4
#define DUN_OBJ_CHANCE_ROOM 30
#define DUN_OBJ_CHANCE_BOTH 5
#define ALLOC_SET_CORR 1
#define ALLOC_SET_ROOM 2
#define ALLOC_SET_BOTH 3
#define ALLOC_TYP_RUBBLE 1
#define ALLOC_TYP_OBJECT 5
#define MAX_ROOMS_ROW (MAX_DUNGEON_HGT / BLOCK_HGT)
#define MAX_ROOMS_COL (MAX_DUNGEON_WID / BLOCK_WID)
#define CENT_MAX DUN_ROOMS
#define DOOR_MAX 200
#define WALL_MAX 500
#define TUNN_MAX 900
#define ROOM_MAX 12
#define ROOM_MIN 2
#define LAYOUT_ANCHOR_MAX CENT_MAX
#define LABYRINTH_START_DEPTH 7
#define BIG_CAVE_START_DEPTH 10
#define CHASM_START_DEPTH 14
#define PARTITION_THEME_MONSTER_PERCENT 80
#define PARTITION_THEME_LEVEL_DELTA 2
#define CHASM_WHISPERING_SHADOW_TARGET 8
#define CHASM_AMBUSH_UNIQUE_SUB_PERCENT 10
#define SPECIAL_CAP_STEP 5
#define SPECIAL_CAP_MAX 3
#define PARTITION_CHEST_RECIPE_MAX 3

typedef enum {
    LEVEL_GEN_STAGE_PLANNING = 0,
    LEVEL_GEN_STAGE_FOUNDATIONS,
    LEVEL_GEN_STAGE_SHAPING,
    LEVEL_GEN_STAGE_LINKING,
    LEVEL_GEN_STAGE_ENTRY,
    LEVEL_GEN_STAGE_TREASURE,
    LEVEL_GEN_STAGE_MONSTERS,
    LEVEL_GEN_STAGE_FINALIZING,
    LEVEL_GEN_STAGE_COUNT
} level_gen_screen_stage_t;

#define LEVEL_GEN_STAGE_DONE LEVEL_GEN_STAGE_COUNT
#define LEVEL_GEN_SCREEN_DEBUG_LINES 32
#define LEVEL_GEN_SCREEN_ISSUES 12

typedef struct level_gen_issue_count {
    char key[64];
    int count;
} level_gen_issue_count;

typedef struct level_gen_screen_state {
    bool active;
    bool debug;
    bool screen_saved;
    int attempt;
    int total_failures;
    int stage;
    int spinner;
    Uint64 last_draw_ticks;
    char depth_label[64];
    char status_text[160];
    char detail_text[160];
    char final_text[160];
    char last_failure[160];
    char last_quest_vault_failure[160];
    char debug_lines[LEVEL_GEN_SCREEN_DEBUG_LINES][160];
    int debug_count;
    level_gen_issue_count issues[LEVEL_GEN_SCREEN_ISSUES];
    int issue_count;
} level_gen_screen_state;

typedef struct {
    bool has_aule_change;
    bool has_mandos_change;
    bool has_varda_change;
    int aule_level;
    int mandos_level;
    int varda_level;
    int aule_forge_y, aule_forge_x;
    int mandos_vault_y, mandos_vault_x;
    int varda_vault_y, varda_vault_x;
} pending_quest_states_t;

typedef struct coord coord;

struct coord
{
    byte y;
    byte x;
};

/*
 * Simple structure to hold a map location
 */

typedef struct rectangle rectangle;

struct rectangle
{
    byte y1;
    byte x1;
    byte y2;
    byte x2;
};

typedef enum room_kind
{
    ROOM_KIND_NONE = 0,
    ROOM_KIND_CLASSIC = 1,
    ROOM_KIND_CROSS = 2,
    ROOM_KIND_INTERESTING = 6,
    ROOM_KIND_LESSER_VAULT = 7,
    ROOM_KIND_GREATER_VAULT = 8
} room_kind_t;

/*
 * Structure to hold all "dungeon generation" data
 */

typedef struct dun_data dun_data;

struct dun_data
{
    /* Classifies each room slot by the builder that created it (1,2,6,7,8) */
    byte kind[CENT_MAX];
    bool is_quest[CENT_MAX];

    /* Array of centers of rooms */
    int cent_n;
    coord cent[CENT_MAX];

    /* Sil: Array of room corners */
    rectangle corner[CENT_MAX];

    /* Sil: Array of what dungeon piece each room is in */
    byte piece[CENT_MAX];

    /* Array of connections between rooms */
    bool connection[DUN_ROOMS][DUN_ROOMS];
};

typedef enum layout_anchor_kind {
    LAYOUT_ANCHOR_NONE = 0,
    LAYOUT_ANCHOR_ROOM,
    LAYOUT_ANCHOR_PREFAB, /* seeded prefab anchor (vault/room) */
    LAYOUT_ANCHOR_CA_BLOB,
    LAYOUT_ANCHOR_BSP_SLICE,
    LAYOUT_ANCHOR_SETPIECE
} layout_anchor_kind_t;

typedef struct layout_anchor {
    layout_anchor_kind_t kind;
    rectangle bounds;
    coord center;
    byte room_kind;
    bool is_quest;
    bool requires_neighbor;
    bool neighbor_linked;
    int style_primary;
    int room_slot;
} layout_anchor_t;

static inline int room_capacity_limit(void)
{
    return MIN(CENT_MAX, DUN_ROOMS - 1);
}

typedef enum quadrant_mode {
    QUAD_MODE_ROOMY = 0,
    QUAD_MODE_CAVEY,
    QUAD_MODE_RUINED,
    QUAD_MODE_LABYRINTH,  /* Twisting corridors and small chambers */
    QUAD_MODE_CHASM,      /* Large open areas with pillars and bridges */
    QUAD_MODE_BIG_CAVE    /* Single large cavern filling most of the partition */
} quadrant_mode_t;

typedef enum density_level {
    DENSITY_SPARSE = 0,   /* Fewer rooms/carvings */
    DENSITY_NORMAL,       /* Standard amount */
    DENSITY_DENSE         /* More rooms/carvings */
} density_level_t;

typedef enum partition_chest_anchor_pref {
    PARTITION_CHEST_ANCHOR_ANY = 0,
    PARTITION_CHEST_ANCHOR_BSP_SLICE
} partition_chest_anchor_pref;

typedef struct partition_chest_recipe {
    byte chest_mode;
    s16b material_wood_pct;
    s16b material_steel_pct;
    s16b material_jewel_pct;
    byte anchor_pref;
} partition_chest_recipe;

#define PARTITION_CHEST_RECIPE_MAX 3

typedef struct partition_population_meta {
    int chest_count;
    partition_chest_recipe chest_recipes[PARTITION_CHEST_RECIPE_MAX];
} partition_population_meta;

typedef struct {
    rectangle bounds;
    coord center;
    int rooms[CENT_MAX];
    int room_count;
    int hub_room;
} partition_link_data_t;

typedef struct partition_drop_profile {
    bool allow_floor_drops;
    drop_profile profile;
} partition_drop_profile;

typedef struct partition_population_plan {
    int pi;
    quadrant_mode_t mode;
    big_cave_type_t cave_type;
    int y1, y2, x1, x2;
    int room_centers;
    int floor_count;
    int room_floor_count;
    int corridor_floor_count;
    int floor_count_non_icky;
    int floor_count_non_vault;
    partition_population_meta meta;
    int monsters_base;
    int monsters_floor;
    int monsters_depth;
    int monsters_precurse;
    int monsters_curse_bonus;
    int monsters_total;
    int room_objects;
    int corr_objects;
} partition_population_plan;

typedef struct partition_count_rule {
    int divisor;
    int min_count;
    int max_count;
} partition_count_rule;

typedef struct partition_depth_rule {
    int divisor;
    int min_count;
    int max_count;
    int scale_pct_at_depth_20;
    int hard_cap_divisor;
} partition_depth_rule;

typedef struct partition_metal_rule {
    int divisor;
    int min_count;
    int max_count;
    int min_depth;
} partition_metal_rule;

typedef struct partition_rule_config {
    drop_profile profiles[PARTITION_DROP_SOURCE_MAX];
    bool allow_floor_drops;
    int base_monster_scale_num;
    int base_monster_scale_den;
    partition_count_rule direct_monsters;
    partition_depth_rule depth_monsters;
    int room_object_divisor;
    int corridor_object_divisor;
    partition_metal_rule metal_drops;
    char discovery_text[1024];
    char big_cave_discovery_text[BIG_CAVE_TYPE_MAX][1024];
} partition_rule_config;

typedef enum {
    TUNNEL_TREAT_NONE = 0,
    TUNNEL_TREAT_NICHES,
    TUNNEL_TREAT_PILLARS
} tunnel_treatment;

typedef struct tunnel_profile {
    byte width;          /* 1 = normal, 2 = offset double, 3 = grand hall */
    int side_bias;       /* -1/0/1: which side to favour when width == 2 */
    tunnel_treatment treatment;
} tunnel_profile;

typedef enum vault_drop_gate_kind {
    VDG_NORMAL = 0,
    VDG_GOOD,
    VDG_GREAT,
    VDG_CHEST
} vault_drop_gate_kind;

typedef enum vault_dock_dir
{
    VAULT_DOCK_NORTH = 0,
    VAULT_DOCK_EAST = 1,
    VAULT_DOCK_SOUTH = 2,
    VAULT_DOCK_WEST = 3
} vault_dock_dir_t;

extern bool allow_uniques;
extern level_gen_screen_state level_gen_screen;
extern int current_build_vault_type;
extern bool current_build_vault_exact_token;
extern const int chasm_sanctum_ambush_offsets[8][2];
extern char level_gen_debug_last_greater_vault_name[80];
extern char level_gen_debug_active_quest_vault_name[80];
extern char level_gen_debug_last_quest_vault_name[80];
extern char level_gen_debug_questgiver_name[80];
extern char level_gen_debug_last_room_name[80];
extern pending_quest_states_t pending_quest_states;
extern int quest_lottery_winner;
extern bool quest_lottery_resolved;
extern int cached_quest_vault_roll;
extern bool cached_gv_level_roll_resolved;
extern bool cached_gv_level_roll_allowed;
extern int cached_gv_level_roll_candidates;
extern dun_data* dun;
extern int cave_corridor1[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
extern int cave_corridor2[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
extern bool cave_escape_tunnel[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
extern layout_anchor_t layout_anchors[LAYOUT_ANCHOR_MAX];
extern int layout_anchor_count;
extern layout_anchor_kind_t room_anchor_kind[CENT_MAX];
extern bool room_anchor_requires_neighbor[CENT_MAX];
extern int current_partition_rows;
extern int current_partition_cols;
extern int current_partition_count;
extern quadrant_mode_t current_partition_modes[25];
extern density_level_t current_partition_densities[25];
extern big_cave_type_t current_partition_big_cave_types[25];
extern int current_partition_bridge_styles[25];
extern partition_population_meta current_partition_population_meta[25];
extern bool g_big_cave_type_rule_set[32];
extern int g_big_cave_type_weight[32][BIG_CAVE_TYPE_MAX];
extern int current_labyrinth_partitions;
extern bool morgoth_level_active;
extern bool morgoth_partition_reserved;
extern int morgoth_partition_index;
extern rectangle morgoth_partition_bounds;
extern int morgoth_vault_center_y;
extern int morgoth_vault_center_x;
extern int qv_stored_y1, qv_stored_x1, qv_stored_y2, qv_stored_x2;
extern bool qv_placed_this_level;

extern void qv_capture(void);
extern char qv_glyph(int f);
extern void qv_dump(const char *phase);
extern void qv_compare(void);
extern bool monster_special_vault_selection_allowed(void);
extern bool monster_special_vault_only_allowed_at(int y, int x);
extern void monster_special_vault_debug_context( int* build_vault_type, bool* exact_token);
extern bool place_vault_monster_token(char symbol, int y, int x);
extern bool is_vault_monster_token(char symbol);
extern bool chasm_mask_has_square_space( const bool* mask, int h, int w, int cy, int cx, int radius);
extern bool choose_chasm_sanctum_seed( const bool* is_cave, int h, int w, int* out_y, int* out_x);
extern bool chasm_sanctum_drop_marker_present(int y, int x);
extern bool chasm_sanctum_ambush_tile(int y, int x);
extern bool place_exact_skeleton_at(int y, int x, byte sval);
extern bool place_chasm_sanctum_drop_at(int y, int x);
extern void place_chasm_island_sanctum(int cy, int cx);
extern void level_gen_debug_reset_context(void);
extern int level_gen_screen_utf8_prefix_len(cptr text, int max_cols);
extern void level_gen_screen_fit_text(char* buf, size_t buflen, cptr text, int max_chars);
extern void level_gen_screen_put_centered(int row, byte attr, cptr text);
extern void level_gen_screen_put_fitted(int col, int row, int width, byte attr, cptr text);
extern int level_gen_screen_print_wrapped(int row, int col, int width, int max_lines, byte attr, cptr text);
extern int level_gen_screen_count_wrapped_lines(int width, int max_lines, cptr text);
extern void level_gen_screen_format_depth_label(char* buf, size_t buflen);
extern bool level_gen_screen_capture_category(cptr category);
extern void level_gen_screen_append_debug_line(cptr text);
extern bool level_gen_screen_extract_context_value(cptr reason, cptr key, char* buf, size_t buflen);
extern void level_gen_screen_extract_issue_key(cptr reason, char* buf, size_t buflen);
extern void level_gen_screen_record_issue(cptr reason);
extern void level_gen_debug_note_room_name(cptr name);
extern void level_gen_debug_note_greater_vault_name(cptr name);
extern void level_gen_debug_note_quest_vault_name(cptr name);
extern const char* level_gen_debug_quest_name(int quest_id);
extern void level_gen_debug_note_questgiver(int quest_id);
extern void level_gen_debug_activate_quest_vault_name(cptr name);
extern void level_gen_debug_append_context(char* buf, size_t buflen, cptr key, cptr value);
extern void level_gen_debug_build_failure_reason(char* buf, size_t buflen, cptr reason);
extern void level_gen_screen_build_generated_summary(char* quest_buf, size_t quest_buflen, char* roulette_buf, size_t roulette_buflen, char* giver_buf, size_t giver_buflen, char* gv_buf, size_t gv_buflen);
extern void level_gen_screen_draw_recent_events(int row, int col, int width, int max_rows);
extern void level_gen_screen_draw_user(int wid, int hgt);
extern void level_gen_screen_draw_debug(int wid, int hgt);
extern void level_gen_screen_draw_now(void);
extern void level_gen_screen_maybe_draw(bool force);
extern void level_gen_screen_observer(cptr category, cptr message);
extern void level_gen_screen_begin(void);
extern void level_gen_screen_start_attempt(void);
extern void level_gen_screen_set_stage(level_gen_screen_stage_t stage, cptr detail);
extern void level_gen_screen_note_failure(cptr reason);
extern void level_gen_screen_finish(bool success);
extern bool quest_metarun_blocked(int quest_id, u32b metarun_flag);
extern float calculate_parametric_probability(quest_type* q_ptr, int depth);
extern bool generic_eligibility_check(int depth, int quest_id);
extern bool generic_probability_roll(int depth, int quest_id);
extern byte* get_quest_state_ptr(u32b var_name_offset);
extern int get_metarun_quest_id(u32b id_name_offset);
extern void init_roulette_quest_registry(void);
extern bool data_driven_eligibility_check(int depth, int quest_id);
extern bool tulkas_probability_roll(int depth, int quest_id);
extern bool niena_probability_roll(int depth, int quest_id);
extern void debug_run_quest_roulette(void);
extern int debug_get_quest_lottery_winner(void);
extern void run_quest_lottery(void);
extern void reset_generation_retry_locks(void);
extern void reset_pending_quest_states(void);
extern byte run_quest_initiated_count(void);
extern void reset_quest_vault_states(byte preserved_initiated_count);
extern void apply_pending_quest_states(void);
extern void init_partition_chest_recipe(partition_chest_recipe* recipe);
extern void set_partition_chest_recipe(partition_population_meta* meta, int slot, int chest_mode, int wooden_pct, int steel_pct, int jewel_pct, partition_chest_anchor_pref anchor_pref);
extern const char* level_gen_partition_mode_name(quadrant_mode_t mode);
extern void level_gen_screen_append_list_item(char* buf, size_t buflen, cptr text);
extern void level_gen_screen_build_partition_summary(char* total_buf, size_t total_buflen, char* types_buf, size_t types_buflen);
extern bool morgoth_region_active(void);
extern bool coord_in_morgoth_region(int y, int x, int margin);
extern bool morgoth_segment_blocked(int y1, int x1, int y2, int x2, int margin);
extern void big_cave_type_rules_clear(void);
extern void big_cave_type_set_rule(int depth, int ice_weight, int fire_weight, int pois_weight);
extern big_cave_type_t big_cave_type_pick_for_depth(int depth);
extern void seal_morgoth_partition(const vault_type* v_ptr, int y0, int x0);
extern bool room_kind_is_vault(byte kind);
extern bool area_is_basic_granite(int y1, int x1, int y2, int x2);
extern void remember_partition_grid(int rows, int cols, int count);
extern void record_partition_metadata( const quadrant_mode_t* modes, const density_level_t* densities, int count);
extern void reset_morgoth_layout_state(bool active);
extern void fallback_partition_grid_from_blocks(int blocks, int *rows, int *cols);
extern bool area_is_reserved_or_dense(int y1, int y2, int x1, int x2, int *floor_pct_out, int *icky_pct_out);
extern bool compute_partition_bounds(int pi, int rows, int cols, int *y1, int *y2, int *x1, int *x2);
extern bool level_has_chasm_partition(void);
extern void apply_chasm_partition_tags(void);
extern void apply_partition_and_room_glow_rules(void);
extern int scaled_attempts(int base, int area_factor);
extern quadrant_mode_t pick_weighted_mode(const int *weights, int count);
extern int special_mode_start_depth(quadrant_mode_t mode);
extern int mode_cap_for_depth( quadrant_mode_t mode, int depth, int partition_count);
extern int mode_weight_for_depth(quadrant_mode_t mode, int depth, int blocks, const int* mode_counts, int partition_count);
extern bool place_room_with_budget(int typ, int y1, int y2, int x1, int x2, int max_tries, int depth, int *budget_t6, int *budget_t7, int *budget_t8, int *used_t6, int *used_t7, int *used_t8);
extern int style_at_color(int y, int x);
extern void layout_anchor_reset(void);
extern void mark_room_anchor_meta(int room_idx, layout_anchor_kind_t kind, bool requires_neighbor);
extern void layout_anchor_capture_room(int room_idx);
extern void layout_anchor_capture_existing_rooms(void);
extern bool place_prefab_anchor_of_type(int typ, bool require_neighbor);
extern void seed_prefab_anchors(void);
extern void scatter_quartz_veins_in_bounds(int y1, int y2, int x1, int x2, u16b info_flag);
extern bool bounds_have_chasm_tag(int y1, int y2, int x1, int x2);
extern bool carve_ca_blob_anchor(void);
extern bool carve_ca_blob_anchor_bounds(int y_min, int y_max, int x_min, int x_max, int style_idx);
extern int prune_big_cave_detached_components( int y1, int y2, int x1, int x2, int style_idx);
extern bool chasm_mask_has_clearance( const bool* is_cave, int h, int w, int ly, int lx, int radius);
extern bool repair_chasm_walkable_connectivity( int y1, int y2, int x1, int x2, int bridge_style);
extern bool carve_big_cave_bounds(int y_min, int y_max, int x_min, int x_max, int style_idx, big_cave_type_t cave_type);
extern bool carve_chasm_with_bridges(int y_min, int y_max, int x_min, int x_max, int floor_style, int bridge_style);
extern bool carve_labyrinth_bounds(int y_min, int y_max, int x_min, int x_max, density_level_t density, int style_idx);
extern void seed_ca_blob_anchors(void);
extern bool carve_bsp_slice_anchor(void);
extern bool carve_bsp_slice_anchor_bounds(int y_min, int y_max, int x_min, int x_max);
extern void seed_bsp_slice_anchors(void);
extern bool room_build_in_bounds(int typ, int y1, int y2, int x1, int x2);
extern void place_rooms_randomized(int y1, int y2, int x1, int x2, int depth, int t1_count, int t2_count, int t6_count, int t7_count, int *budget_t6, int *budget_t7, int *budget_t8, int *used_t6, int *used_t7, int *used_t8);
extern int min_nonquest_gv_depth(void);
extern int vault_type8_generation_rarity(const vault_type* v_ptr, int depth);
extern bool quest_vault_surface_roll_allows(const vault_type* v_ptr, int depth);
extern bool gv_level_roll_allows(int depth, int *out_candidates);
extern bool partition_is_interior(int row, int col, int rows, int cols);
extern bool generation_escape_tunnel_bold(int y, int x);
extern void mark_generation_escape_tunnel(int y, int x);
extern int choose_central_partition_index(int rows, int cols);
extern bool place_gv_in_partition(int y1, int y2, int x1, int x2, int *budget_t8, int *used_t8);
extern bool place_partition_chest_at(int pi, int y, int x, const partition_chest_recipe* recipe, quadrant_mode_t mode, bool require_room_tile);
extern void reset_partition_population_metadata(void);
extern bool place_chest_in_bounds( int pi, int y1, int y2, int x1, int x2, const partition_chest_recipe* recipe, quadrant_mode_t mode, bool require_room_tile);
extern bool place_chest_in_partition( int pi, int y1, int y2, int x1, int x2, const partition_chest_recipe* recipe, quadrant_mode_t mode);
extern void apply_quadrant_generation_modes(void);
extern void ensure_partition_connectivity(void);
extern int partition_index_from_point(int y, int x, int rows, int cols);
extern int room_partition_index(int room_idx);
extern bool tunnel_should_mark_escape(int r1, int r2);
extern int room_connection_degree(int room_idx);
extern bool connect_rooms_with_logging(int r1, int r2, const char *tag, bool allow_desperate);
extern bool is_big_partition_mode(quadrant_mode_t mode);
extern bool big_partition_boundary_floor_ok(quadrant_mode_t mode, int y, int x);
extern int partition_bridge_style_for_index(int pi);
extern bool carve_straight_big_partition_connector( int y1, int x1, int y2, int x2, int r1, int r2, int rows, int cols);
extern bool connect_adjacent_big_partitions_by_boundary( int pi_a, int pi_b, const rectangle *bounds_a, const rectangle *bounds_b, int rows, int cols, int hub_a, int hub_b, bool vertical_boundary);
extern void seed_partition_adjacency(const int *room_to_part, int part_count, bool adj[25][25], int degree[25]);
extern void mark_partition_edge(int p1, int p2, bool adj[25][25], int degree[25]);
extern int choose_partition_hub(const partition_link_data_t *part);
extern int find_anchor_target(int src, const int *room_to_part, const bool *skip, int part_count);
extern void connect_anchor_backbone(const int *room_to_part, int part_count);
extern void connect_partition_hubs(void);
extern void repair_all_outer_walls(void);
extern void ensure_minimum_rooms(void);
extern bool feature_is_any_door(int feat);
extern bool doorway_neighbor_open(int y, int x);
extern bool doorway_neighbor_blocked(int y, int x);
extern bool doorway_geometry_ok(int y, int x);
extern bool room_prefers_floor_thresholds(int room_idx);
extern bool tunnel_prefers_floor_thresholds(int r1, int r2);
extern void carve_floor_threshold( int y, int x, int r1, int r2, bool mark_escape);
extern int squash_double_doors(void);
extern int prune_invalid_nonvault_doors(void);
extern bool player_passable(int y, int x, bool ignore_rubble_and_chasms);
extern void flood_access(int y, int x, int access_array[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], bool ignore_rubble_and_chasms);
extern void label_rooms(void);
extern void flood_piece(int n, int piece_num);
extern int dungeon_pieces(void);
extern void place_rubble(int y, int x);
extern int choose_up_stairs(void);
extern int choose_down_stairs(void);
extern int calculate_nearest_down_stair_distance_from(int y0, int x0);
extern void place_random_stairs(int y, int x);
extern bool wearable_p(const object_type *o_ptr);
extern void place_item_randomly(int tval, int sval, bool close);
extern level_partition_kind partition_config_normalize_kind(level_partition_kind kind);
extern void partition_config_profile_assign(drop_profile* profile, int weapon, int armor, int jewelry, int supply, int potion, int herb, int gem, int staff, int light, int arrows, int tunneling);
extern void partition_config_set_defaults_for_kind(level_partition_kind kind);
extern void partition_config_ensure_initialized(void);
extern void partition_config_reset(void);
extern void partition_config_set_drop_profile(level_partition_kind kind, partition_drop_source_t source, const drop_profile* profile);
extern void partition_config_set_floor_rules(level_partition_kind kind, bool allow_floor_drops);
extern void partition_config_set_base_monster_scale(level_partition_kind kind, int numerator, int denominator);
extern void partition_config_set_direct_monster_rule(level_partition_kind kind, int divisor, int min_count, int max_count);
extern void partition_config_set_depth_monster_rule(level_partition_kind kind, int divisor, int min_count, int max_count, int scale_pct_at_depth_20, int hard_cap_divisor);
extern void partition_config_set_object_rules(level_partition_kind kind, int room_divisor, int corridor_divisor);
extern void partition_config_set_metal_rule(level_partition_kind kind, int divisor, int min_count, int max_count, int min_depth);
extern void partition_config_set_discovery_text(level_partition_kind kind, cptr text);
extern void partition_config_set_big_cave_discovery_text(big_cave_type_t cave_type, cptr text);
extern cptr partition_config_get_discovery_text(level_partition_kind kind, big_cave_type_t cave_type);
extern const partition_rule_config* partition_config_get(level_partition_kind kind);
extern quadrant_mode_t partition_mode_for_point(int y, int x);
extern quadrant_mode_t drop_mode_for_point(int y, int x);
extern level_partition_kind partition_kind_from_mode(quadrant_mode_t mode);
extern bool chasm_native_walkable_bold(int y, int x);
extern bool partition_population_floor_bold(quadrant_mode_t mode, int y, int x);
extern bool partition_population_naked_bold(quadrant_mode_t mode, int y, int x);
extern bool partition_mode_avoids_corridor_spawns(quadrant_mode_t mode);
extern const char* quadrant_mode_debug_name(quadrant_mode_t mode);
extern const char* partition_kind_debug_name(level_partition_kind kind);
extern const char* big_cave_type_debug_name(big_cave_type_t cave_type);
extern bool suppress_partition_effects_for_point(int y, int x);
extern level_partition_kind level_partition_kind_for_point(int y, int x);
extern void level_partition_meta_get(partition_meta_save* out);
extern void level_partition_meta_set(const partition_meta_save* in);
extern int level_partition_index_for_point(int y, int x);
extern big_cave_type_t level_partition_big_cave_type_for_index(int pi);
extern big_cave_type_t level_partition_big_cave_type_for_point(int y, int x);
extern void log_partition_debug_for_point(const char* tag, int y, int x);
extern void level_layout_info_current(level_layout_info* out);
extern partition_drop_profile partition_drop_profile_for_mode(quadrant_mode_t mode);
extern drop_profile drop_profile_for_mode(quadrant_mode_t mode);
extern void drop_profile_for_partition_kind(level_partition_kind kind, drop_profile* out);
extern partition_drop_profile partition_drop_profile_for_kind_source_cfg( level_partition_kind kind, partition_drop_source_t source);
extern partition_drop_profile partition_drop_profile_for_mode_source_cfg( quadrant_mode_t mode, partition_drop_source_t source);
extern void drop_profile_for_partition_kind_source(level_partition_kind kind, partition_drop_source_t source, drop_profile* out);
extern void place_object_with_profile( int y, int x, const partition_drop_profile* prof);
extern void place_object_with_profile_params( int y, int x, int base_depth, int min_depth_penalty_depth, drop_quality quality, int droptype, bool allow_artefacts, int artefact_weight_multiplier, u32b extra_ident, const partition_drop_profile* prof);
extern int partition_metal_drop_target(quadrant_mode_t mode, int floor_count, int depth);
extern s16b partition_metal_kind_for_mode(quadrant_mode_t mode);
extern bool partition_metal_tile_ok(const partition_population_plan* plan, int y, int x, bool require_chasm_tag);
extern int place_partition_metal_drops(const partition_population_plan* plan);
extern void alloc_object_global(int set, int typ, int num, bool out_of_sight);
extern bool build_streamer(int feat);
extern bool build_chasm(void);
extern void build_chasms(void);
extern bool solid_rock(int y1, int x1, int y2, int x2);
extern bool doubled_wall(int y1, int x1, int y2, int x2);
extern void generate_room(int y1, int x1, int y2, int x2, int light);
extern void generate_fill(int y1, int x1, int y2, int x2, int feat);
extern void generate_draw(int y1, int x1, int y2, int x2, int feat);
extern void generate_plus(int y1, int x1, int y2, int x2, int feat);
extern bool h_tunnel_ok( int x1, int x2, int y, bool tentative, int desired_changes);
extern bool v_tunnel_ok( int y1, int y2, int x, bool tentative, int desired_changes);
extern tunnel_profile choose_tunnel_profile(bool tentative);
extern void apply_tunnel_niche_torch_glow(int niche_y, int niche_x, int front_dy, int front_dx);
extern void apply_v_tunnel_treatment( int r1, int r2, int y_lo, int y_hi, int x, bool widen_west, bool widen_east, const tunnel_profile* profile, bool mark_escape);
extern void apply_h_tunnel_treatment( int r1, int r2, int x_lo, int x_hi, int y, bool widen_north, bool widen_south, const tunnel_profile* profile, bool mark_escape);
extern void build_v_tunnel( int r1, int r2, int y1, int y2, int x, const tunnel_profile* profile);
extern void build_h_tunnel( int r1, int r2, int x1, int x2, int y, const tunnel_profile* profile);
extern bool build_tunnel( int r1, int r2, int y1, int x1, int y2, int x2, bool tentative);
extern bool connect_two_rooms(int r1, int r2, bool tentative, bool desperate);
extern bool connect_room_to_corridor(int r);
extern bool alloc_stairs(int feat, int num);
extern bool feat_within_los(int y0, int x0, int feat);
extern bool stairs_within_los(int y, int x);
extern int trap_placement_chance(int y, int x);
extern void place_traps(void);
extern bool place_rubble_player(void);
extern bool connectivity_rescue_traversable(int ry, int rx);
extern int connectivity_unreachable_component( int start_y, int start_x, int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID]);
extern bool connectivity_component_boundary_cell( int y, int x, byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID]);
extern bool connectivity_rescue_component( byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID], int component_count, int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], int *out_source_y, int *out_source_x, int *out_target_y, int *out_target_x, int *out_carve_count, int *out_boundary_sources);
extern bool check_connectivity(void);
extern bool doubled_doors(void);
extern bool connect_rooms_stairs(void);
extern bool build_type1(int y0, int x0);
extern bool build_type2(int y0, int x0);
extern void place_monster_by_flag( int y, int x, int flagset, u32b f, bool allow_unique, int max_depth);
extern void place_monster_by_letter( int y, int x, char c, bool allow_unique, int max_depth);
extern int vault_drop_gate_percent(vault_drop_gate_kind kind);
extern bool vault_drop_passes(vault_drop_gate_kind kind);
extern bool build_vault(int y0, int x0, vault_type* v_ptr, bool flip_d);
extern bool solid_rock_reduced_padding(int y1, int x1, int y2, int x2);
extern void compute_vault_bounds( int y0, int x0, const vault_type* v_ptr, bool flip_d, int* y1, int* x1, int* y2, int* x2);
extern bool place_room_forced_internal( int y0, int x0, vault_type* v_ptr, bool flip_d, bool log_failures);
extern bool place_room_forced(int y0, int x0, vault_type* v_ptr);
extern bool place_room_forced_exhaustive( vault_type* v_ptr, int* placed_y, int* placed_x);
extern bool place_room(int y0, int x0, vault_type* v_ptr);
extern bool area_clear_for_vault_dock( int y1, int x1, int y2, int x2, vault_dock_dir_t dir);
extern bool choose_vault_contact( int base_idx, vault_dock_dir_t dir, int* y_out, int* x_out);
extern bool try_place_docked_vault( vault_type* v_ptr, int* out_y0, int* out_x0);
extern bool vault_template_has_aule(vault_type *v);
extern bool vault_template_has_mandos(vault_type *v);
extern bool vault_template_has_duruin(vault_type *v);
extern void check_quest_vault_integrity(const char* checkpoint_name);
extern void process_quest_vault_area(int y0, int x0, vault_type *qv);
extern bool build_type6(int y0, int x0, bool force_forge);
extern bool build_type7(int y0, int x0);
extern bool mark_g_vault(int y0, int x0, int ymax, int xmax);
extern bool vault_type8_is_repeated(s16b v_idx);
extern bool vault_type8_is_eligible(s16b v_idx, bool test_only);
extern bool any_eligible_type8_test_vault(void);
extern bool choose_reserved_type8(vault_type** out_v_ptr, s16b* out_v_idx);
extern bool place_type8_vault(int y0, int x0, vault_type* v_ptr, s16b v_idx);
extern bool build_reserved_type8(int y0, int x0);
extern bool build_type8(int y0, int x0);
extern bool build_type9(int y0, int x0, vault_type** used_vault);
extern void carve_morgoth_entry_tunnels(const vault_type* v_ptr, int y0, int x0);
extern bool morgoth_tunnel_traversable(int y, int x);
extern bool morgoth_tunnel_target(int y, int x);
extern bool connect_morgoth_tunnel_component(int start_y, int start_x);
extern void cave_set_feat_style(int y, int x, int feat, int style_idx);
extern void partition_theme_depth_band(int depth, int* min_depth, int* max_depth);
extern bool chasm_theme_monster_ok( int r_idx, int min_depth, int max_depth, bool allow_unique, bool unique_only);
extern bool partition_mode_uses_monster_pools(quadrant_mode_t mode);
extern bool monster_name_contains_ci(const monster_race* r_ptr, cptr needle);
extern bool monster_is_bat(const monster_race* r_ptr);
extern bool monster_has_blow_effect(const monster_race* r_ptr, byte effect);
extern bool monster_counts_toward_labyrinth_fixed_cap(const monster_race* r_ptr);
extern bool monster_matches_partition_theme( const monster_race* r_ptr, quadrant_mode_t mode, big_cave_type_t cave_type);
extern bool partition_pool_monster_ok( const partition_population_plan* plan, int r_idx, int min_depth, int max_depth, bool themed, int labyrinth_fixed_remaining);
extern s16b choose_partition_pool_monster( const partition_population_plan* plan, bool themed, int min_depth, int max_depth, int labyrinth_fixed_remaining);
extern bool place_partition_pool_monster( const partition_population_plan* plan, int y, int x, bool themed, int labyrinth_fixed_remaining);
extern s16b choose_chasm_theme_monster( int min_depth, int max_depth, bool allow_unique, bool unique_only);
extern bool place_chasm_theme_monster_at(int y, int x, int r_idx);
extern bool chasm_sanctum_drop_present(int y, int x);
extern void clear_chasm_sanctum_drop_marker(int y, int x);
extern void awaken_chasm_sanctum_monster(int y, int x);
extern bool relocate_chasm_sanctum_blocker(int y, int x);
extern void trigger_chasm_sanctum_ambush_if_needed(int y, int x);
extern int partition_base_monsters_for_mode(quadrant_mode_t mode, int room_count);
extern int partition_apply_monster_curse_scale(int monster_count);
extern int partition_direct_floor_monsters(quadrant_mode_t mode, int floor_count);
extern int partition_extra_monster_target_for_depth( quadrant_mode_t mode, int floor_count, int depth);
extern int partition_depth_bonus_monsters(quadrant_mode_t mode, int floor_count, int depth);
extern int partition_object_scale_pct(void);
extern void partition_object_counts_from_total_monsters( quadrant_mode_t mode, int total_monsters, int* room_objects, int* corr_objects);
extern void rebalance_partition_corridor_objects(partition_population_plan* plan);
extern void distribute_partition_base_monsters( partition_population_plan* plans, int plan_count);
extern void apply_curse_scale_to_partition_totals( partition_population_plan* plans, int plan_count);
extern int build_partition_population_plans( partition_population_plan* plans, int max_plans);
extern bool choose_partition_monster_location( const partition_population_plan* plan, int* out_y, int* out_x);
extern bool place_partition_themed_monster( const partition_population_plan* plan, int y, int x);
extern bool partition_monster_pass_skips_plan( const partition_population_plan* plan);
extern int run_partition_monster_pass( const partition_population_plan* plans, int plan_count);
extern int alloc_objects_from_plan( const partition_population_plan* plan, int set, int num);
extern int run_partition_object_pass( const partition_population_plan* plans, int plan_count, bool rooms);
extern int place_partition_skeletons( const partition_population_plan* plan, int target, int human_pct, int elf_pct, bool avoid_rubble);
extern bool partition_exact_monster_tile_ok( const partition_population_plan* plan, int y, int x);
extern int place_partition_exact_monster_tokens( const partition_population_plan* plan, char token, int target);
extern int run_partition_special_scatter_pass( const partition_population_plan* plans, int plan_count);
extern bool connect_morgoth_entry_tunnels(void);
extern bool build_type10(int y0, int x0);
extern bool room_build(int typ);
extern bool place_duruin_bastion(void);
extern bool try_quest_vault_type(int v_type, bool *had_eligible_candidate);
extern void set_perm_boundry(void);
extern void basic_granite(void);
extern void make_patch_of_sunlight(int y, int x);
extern void make_patches_of_sunlight();
extern bool varda_sunlight_tile_ok(int y, int x, bool require_empty);
extern void varda_make_sunlight_pool(int y, int x);
extern bool varda_no_rubble_path_tile_ok(int y, int x, int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID]);
extern int pick_varda_sunlight_spawn_tile(int *out_y, int *out_x, int *out_total_sunlight, int *out_empty_sunlight);
extern bool force_varda_sunlight_tile(int *out_y, int *out_x);
extern void ensure_sunlight_for_varda(void);
extern int morgoth_escape_path_step_cost(monster_type* m_ptr, int y, int x);
extern void build_morgoth_escape_path_distances( int path_dist[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], int max_path);
extern bool morgoth_escape_spawn_path_ok(int y, int x, int distance_roll, int path_dist[MAX_DUNGEON_HGT][MAX_DUNGEON_WID]);
extern bool cave_gen(void);
extern void gates_gen(void);
extern void throne_gen(void);
extern void unring_a_bell(void);
extern void generate_cave(void);

#endif /* INCLUDED_LEVEL_GENERATION_INTERNAL_H */
