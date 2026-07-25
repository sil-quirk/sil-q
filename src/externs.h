/* File: externs.h */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

/*
 * Note that some files have their own header files
 * (z-virt.h, z-term.h, rng.h)
 */

/*
 * Automatically generated "variable" declarations
 */
#include "h-basic.h"
#include "score/score_io.h"
#include "score/score_ui.h"
#include "spell/spell.h"
#include "player/player-songs.h"
#include "ui/story_font.h"
// extern FILE *log_file;
extern int max_macrotrigger;
extern cptr macro_template;
extern cptr macro_modifier_chr;
extern cptr macro_modifier_name[MAX_MACRO_MOD];
extern cptr macro_trigger_name[MAX_MACRO_TRIGGER];
extern cptr macro_trigger_keycode[2][MAX_MACRO_TRIGGER];

#ifndef LEVEL_LAYOUT_INFO_DEFINED
#define LEVEL_LAYOUT_INFO_DEFINED
typedef enum
{
    LEVEL_PART_NONE = 0,
    LEVEL_PART_ROOMY,
    LEVEL_PART_CAVEY,
    LEVEL_PART_RUINED,
    LEVEL_PART_LABYRINTH,
    LEVEL_PART_CHASM,
    LEVEL_PART_BIG_CAVE,
    LEVEL_PART_MAX
} level_partition_kind;

typedef struct
{
    int map_wid;
    int map_hgt;
    int partition_rows;
    int partition_cols;
    int partition_count;
    int labyrinth_parts;
    int big_cave_parts;
    int chasm_parts;
    level_partition_kind dominant_kind;
} level_layout_info;

typedef enum
{
    BIG_CAVE_NONE = 0,
    BIG_CAVE_ICE,
    BIG_CAVE_FIRE,
    BIG_CAVE_POIS,
    BIG_CAVE_TYPE_MAX
} big_cave_type_t;

typedef enum
{
    PART_STYLE_CA_BLOB = 0,
    PART_STYLE_LABYRINTH,
    PART_STYLE_CHASM_FLOOR,
    PART_STYLE_CHASM_BRIDGE,
    PART_STYLE_BIG_CAVE_ICE,
    PART_STYLE_BIG_CAVE_FIRE,
    PART_STYLE_BIG_CAVE_POIS,
    PART_STYLE_MAX
} partition_style_kind_t;

typedef enum
{
    PARTITION_DROP_SOURCE_FLOOR = 0,
    PARTITION_DROP_SOURCE_CHEST,
    PARTITION_DROP_SOURCE_MONSTER,
    PARTITION_DROP_SOURCE_MAX
} partition_drop_source_t;
#endif

#ifndef SKELETON_NOTE_STATE_SAVE_DEFINED
#define SKELETON_NOTE_STATE_SAVE_DEFINED
#define SKELETON_NOTE_SEEN_MAX 8

typedef struct skeleton_note_state_save {
    s16b level_depth;
    s16b note_cap;
    s16b notes_shown;
    s16b map_wid;
    s16b map_hgt;
    u32b hint_used_mask;
    byte hint_use_counts[SKEL_HINT_MAX];
    byte seen_count;
    s16b seen_ids[SKELETON_NOTE_SEEN_MAX];
} skeleton_note_state_save;
#endif

#ifndef HINT_MESSAGE_META_DEFINED
#define HINT_MESSAGE_META_DEFINED
#define HINT_MESSAGE_CUE_MAX 2
#define HINT_MESSAGE_CUE_TEXT_MAX 64
typedef struct hint_message_meta {
    s16b source_y;
    s16b source_x;
    byte cue_count;
    char cue_dirs[HINT_MESSAGE_CUE_MAX][HINT_MESSAGE_CUE_TEXT_MAX];
    char cue_dists[HINT_MESSAGE_CUE_MAX][HINT_MESSAGE_CUE_TEXT_MAX];
} hint_message_meta;
#endif

#ifndef PARTITION_META_SAVE_DEFINED
#define PARTITION_META_SAVE_DEFINED
#define PARTITION_META_MAX 25
typedef struct partition_meta_save {
    s16b grid_rows;
    s16b grid_cols;
    s16b partition_count;
    byte modes[PARTITION_META_MAX];
    byte big_cave_types[PARTITION_META_MAX];
} partition_meta_save;
#endif

/* tables.c */
extern const s16b ddd[9];
extern const s16b ddx[10];
extern const s16b ddy[10];
extern const s16b ddx_ddd[9];
extern const s16b ddy_ddd[9];
extern const char hexsym[16];
extern const byte extract_energy[8];
extern const byte chest_traps[25 + 1];
extern cptr color_names[16];
extern cptr stat_names[A_MAX];
extern cptr stat_names_reduced[A_MAX];
extern cptr stat_names_full[A_MAX];
extern cptr skill_names[S_MAX];
extern cptr skill_names_full[S_MAX];
extern cptr window_flag_desc[32];
extern cptr option_text[OPT_MAX];
extern cptr option_desc[OPT_MAX];
extern const bool option_norm[OPT_MAX];
extern const byte option_page[OPT_PAGE_MAX][OPT_PAGE_PER];
extern cptr inscrip_text[MAX_INSCRIP];
extern byte spell_info_RF4[32][3];
extern byte spell_desire_RF4[32][2];

/* variable.c */
extern cptr copyright;
extern byte version_major;
extern byte version_minor;
extern byte version_patch;
extern byte version_extra;
extern byte sf_major;
extern byte sf_minor;
extern byte sf_patch;
extern byte sf_extra;
extern u32b sf_xtra;
extern u32b sf_when;
extern u16b sf_lives;
extern u16b sf_saves;
extern bool arg_fiddle;
extern bool arg_wizard;
extern bool arg_sound;
extern bool arg_graphics;
extern bool arg_force_original;
extern bool arg_force_roguelike;
extern bool character_generated;
extern bool character_dungeon;
extern bool character_loaded;
extern bool character_loaded_dead;
extern bool character_saved;
extern s16b character_icky;
extern s16b character_xtra;
extern u32b seed_randart;
extern u32b seed_flavor;
extern s16b num_repro;
extern s16b object_level;
extern s16b monster_level;
extern char summon_kin_type;
extern s32b turn;
extern s32b playerturn;
extern s32b min_depth_counter;
extern bool use_sound;
extern int use_graphics;
extern s16b image_count;
extern bool use_bigtile;
extern s16b signal_count;
extern bool msg_flag;
extern bool inkey_base;
extern bool inkey_xtra;
extern bool inkey_scan;
extern bool inkey_flag;
extern bool hide_cursor;
extern byte object_generation_mode;
extern bool drop_allow_noble;
extern bool drop_allow_evil;
extern bool drop_allow_noble_from_quality;
extern bool shimmer_monsters;
extern bool shimmer_objects;
extern bool repair_mflag_mark;
extern bool repair_mflag_show;
extern s16b o_max;
extern s16b o_cnt;
extern s16b mon_max;
extern s16b mon_cnt;
extern byte feeling;
extern byte do_feeling;
extern s16b rating;
extern bool good_item_flag;
extern int closing_flag;
extern int player_uid;
extern int player_euid;
extern int player_egid;
extern char savefile[1024];
extern s16b macro__num;
extern cptr* macro__pat;
extern cptr* macro__act;
extern term* angband_term[ANGBAND_TERM_MAX];
extern char angband_term_name[ANGBAND_TERM_MAX][16];
extern byte angband_color_table[256][4];
extern const cptr angband_sound_name[MSG_MAX];
extern int view_n;
extern u16b* view_g;
extern int temp_n;
extern u16b* temp_g;
extern byte* temp_y;
extern byte* temp_x;
extern u16b (*cave_info)[256];
extern byte (*cave_feat)[MAX_DUNGEON_WID];
extern byte (*cave_color)[MAX_DUNGEON_WID];
extern byte (*cave_natural)[MAX_DUNGEON_WID];
extern byte (*cave_rewired)[MAX_DUNGEON_WID];
extern s16b (*cave_light)[MAX_DUNGEON_WID];
extern s16b (*cave_o_idx)[MAX_DUNGEON_WID];
extern s16b (*cave_m_idx)[MAX_DUNGEON_WID];
extern u32b mon_power_ave[MAX_DEPTH][CREATURE_TYPE_MAX];

extern byte cave_cost[MAX_FLOWS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
extern byte (*cave_when)[MAX_DUNGEON_WID];
extern int scent_when;
extern byte flow_center_y[MAX_FLOWS];
extern byte flow_center_x[MAX_FLOWS];
extern byte update_center_y[MAX_FLOWS];
extern byte update_center_x[MAX_FLOWS];
extern s16b wandering_pause[MAX_FLOWS];

/* Public style color encoding base for save/load */
#ifndef COLOR_STYLE_BASE
#define COLOR_STYLE_BASE 128
#endif

extern s16b stealth_score;
extern bool player_attacked;
extern bool attacked_player;
extern maxima* z_info;
extern object_type* o_list;
extern monster_type* mon_list;
extern monster_lore* l_list;
extern object_type* inventory;
extern s16b alloc_kind_size;
extern alloc_entry* alloc_kind_table;
extern s16b alloc_ego_size;
extern alloc_entry* alloc_ego_table;
extern s16b alloc_race_size;
extern alloc_entry* alloc_race_table;
extern byte misc_to_attr[256];
extern char misc_to_char[256];
extern byte tval_to_attr[128];
extern char macro_buffer[1024];
extern cptr keymap_act[KEYMAP_MODES][256];
extern const player_race* rp_ptr;
extern character_profile* current_character_profile;
extern player_other* op_ptr;
extern player_type* p_ptr;
extern vault_type* v_info;
extern char* v_name;
extern char* v_text;
extern feature_type* f_info;
extern char* f_name;
extern char* f_text;
extern object_kind* k_info;
extern char* k_name;
extern char* k_text;
extern ability_type* b_info;
extern char* b_name;
extern char* b_text;
extern artefact_type* a_info;
extern char* a_text;
extern bool* valar_reserved_artifacts;
extern ego_item_type* e_info;
extern char* e_name;
extern char* e_text;
extern monster_race* r_info;
extern monster_race* r_base;
extern char* r_name;
extern char* r_text;
extern player_race* p_info;
extern char* p_name;
extern char* p_text;
extern character_profile* c_info;
extern char* c_name;
extern char* c_text;
extern hist_type* h_info;
extern story_type* st_info;
extern char* st_text;
extern char* st_name;
extern curse_type* cu_info;
extern char* cu_text;
extern char* cu_name;
extern major_blessing_type* mb_info;
extern char* mb_text;
extern char* mb_name;
extern quest_type* quest_info;
extern char* quest_name_text;
extern char* quest_desc_text;
extern char* q_text;
extern oath_type* oath_info;
extern char* oath_name_text;
extern char* oath_desc_text;
extern char* h_text;
extern flavor_type* flavor_info;
extern char* flavor_name;
extern char* flavor_text;
extern names_type* n_info;
extern style_type* style_info;
extern skeleton_note_template* skeleton_note_info;
extern char* skeleton_note_text;
/* Default vein tile accessors (defined in init/init-style.c) */
byte get_default_vein_row(void);
byte get_default_vein_col(void);
bool get_overlay_key_enabled(void);
void get_overlay_key_rgb(byte* r, byte* g, byte* b);
extern char* style_name;

extern combat_roll combat_rolls[2][MAX_COMBAT_ROLLS];
extern int combat_number;
extern int combat_number_old;
extern int turns_since_combat;
extern char combat_roll_special_char;
extern byte combat_roll_special_attr;

extern combat_history_round combat_history[MAX_COMBAT_HISTORY];
extern int combat_history_head;   /* Index of the most recent entry */
extern int combat_history_count;  /* Number of stored rounds */

extern bool project_path_ignore;
extern int project_path_ignore_y;
extern int project_path_ignore_x;

extern cptr ANGBAND_SYS;
extern cptr ANGBAND_GRAF;
extern cptr ANGBAND_DIR;
extern cptr ANGBAND_DIR_APEX;
extern cptr ANGBAND_DIR_METARUN;
extern cptr ANGBAND_DIR_BONE;
extern cptr ANGBAND_DIR_DATA;
extern cptr ANGBAND_DIR_EDIT;
extern cptr ANGBAND_DIR_FILE;
extern cptr ANGBAND_DIR_HELP;
extern cptr ANGBAND_DIR_INFO;
extern cptr ANGBAND_DIR_SAVE;
extern cptr ANGBAND_DIR_PREF;
extern cptr ANGBAND_DIR_USER;
extern cptr ANGBAND_DIR_XTRA;
extern cptr ANGBAND_DIR_SCRIPT;
extern bool item_tester_full;
extern byte item_tester_tval;
extern bool (*item_tester_hook)(const object_type*);
extern bool (*ang_sort_comp)(const void* u, const void* v, int a, int b);
extern void (*ang_sort_swap)(void* u, void* v, int a, int b);
extern bool (*get_mon_num_hook)(int r_idx);
extern bool (*get_obj_num_hook)(int k_idx);
extern void (*object_info_out_flags)(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
extern SDL_IOStream* text_out_file;
extern void (*text_out_hook)(byte a, cptr str);
extern int text_out_wrap;
extern int text_out_indent;
extern bool use_transparency;
extern char notes_buffer[NOTES_LENGTH];
extern byte recent_failed_thefts;
extern autoinscription* inscriptions;
extern u16b inscriptionsCount;
extern byte num_trap_on_level;
extern byte bones_selector;
extern int r_ghost;
extern char ghost_name[80];
extern char g_vault_name[80];
extern bool g_labyrinth_view_active;
extern bool skill_gain_in_progress;
extern bool save_game_quietly;
extern bool stop_stealth_mode;
extern bool use_background_colors;

extern metarun metar;
extern int meta_fd;
/* metarun/score helpers */
extern bool clear_scorefile(void);
extern bool autoload_alive_from_scores(void);
extern void metarun_finalize_scores_and_saves(void);
extern void backup_and_clear_saves(void);

/*
 * Rage and labyrinth partitions both suppress remembered-grid information.
 * When inactive, remembered information remains available to look/target UI.
 */
#ifndef GRID_INFO_VISIBILITY_HELPERS_DEFINED
#define GRID_INFO_VISIBILITY_HELPERS_DEFINED
static inline bool player_suppresses_unseen_grid_info(void)
{
    return (!p_ptr->is_dead) && (p_ptr->rage || g_labyrinth_view_active);
}

static inline bool grid_info_is_available(int y, int x)
{
    return !player_suppresses_unseen_grid_info() || player_can_see_bold(y, x);
}
#endif

#ifndef GENERATION_DEPTH_HELPERS_DEFINED
#define GENERATION_DEPTH_HELPERS_DEFINED
static inline int generation_depth_for_level(int depth)
{
    if (depth == 0)
        return MORGOTH_DEPTH;
    if (depth < 1)
        return 1;
    return depth;
}

static inline int player_generation_depth(void)
{
    if (!p_ptr)
        return 1;
    return generation_depth_for_level(p_ptr->depth);
}
#endif

/*
 * Automatically generated "function declarations"
 */

/* birth/ */
extern NavResult player_birth(void);
extern NavResult gain_skills(void);
extern NavResult character_creation(void);
extern NavResult character_creation_resume_character(void);
extern NavResult blitz_character_creation(void);
void player_wipe(void);

/* cave/ */
extern int distance(int y1, int x1, int y2, int x2);
extern int distance_squared(int y1, int x1, int y2, int x2);
extern bool los(int y1, int x1, int y2, int x2);
extern void random_unseen_floor(int* ry, int* rx);
extern bool no_light(void);
extern bool seen_by_keen_senses(int y, int x);
extern bool cave_valid_bold(int y, int x);
extern bool feat_supports_lighting(int feat);
extern void map_info(int y, int x, byte* ap, char* cp, byte* tap, char* tcp);
extern void map_info_default(int y, int x, byte* ap, char* cp);
extern int player_tile_offset(void);
extern void move_cursor_relative(int y, int x);
extern void print_rel(char c, byte a, int y, int x);
extern void note_spot(int y, int x);
extern void lite_spot(int y, int x);
extern void prt_map(void);
extern void force_map_redraw(void);
extern void display_map(int* cy, int* cx);
extern void do_cmd_view_map(void);
extern errr vinfo_init(void);
extern void forget_view(void);
extern void update_view(void);
extern int flow_dist(int which_flow, int y, int x);
extern void update_flow(int cy, int cx, int which_flow);
extern void update_smell(void);
extern void map_feature(int y, int x);
extern void map_area(void);
extern void map_area_radius(int radius);
extern void wiz_light(void);
extern void wiz_dark(void);
extern void gates_illuminate(bool daytime);
extern void cave_set_feat(int y, int x, int feat);
extern void cave_set_feat_with_color(int y, int x, int feat, int color);
extern byte get_depth_color(int depth);
extern void reset_depth_color_cache(void);
/* Style-weight APIs */
extern void styles_init_for_level(void);
extern void styles_begin_vault(int extra_sidx, int extra_weight);
extern void styles_end_vault(void);
extern void styles_reset_level_weights(void);
extern void styles_add_level_weight(int sidx, int weight);
extern void styles_reset_vault_weights(void);
extern void styles_add_vault_weight(int sidx, int weight);
extern void styles_add_vault_from_level(int factor);
extern void styles_set_vault_avoid_style(int sidx);
extern void styles_default_vault_clear(void);
extern void styles_default_vault_add(int sidx_or_star, int weight);
extern void styles_apply_vault_list(const int* sidx, const int* weight, int count);
extern void styles_vault_rules_clear(void);
extern void styles_set_vault_rule(int depth, const int* sidx, const int* weight, int count);
extern void styles_apply_vault_default_for_depth(int depth);
extern void styles_partition_rules_clear(void);
extern void styles_add_partition_rule(int depth, int kind, const int* sidx, const int* weight, int count);
extern int styles_pick_partition_style(int depth, int kind);
extern int styles_get_level_primary_style(void);
extern int styles_get_vault_primary_style(void);
extern void styles_select_vault_primary(void);
extern int styles_pick_random_from_level(void);
extern int styles_decode_color_style(byte color_value);
extern void styles_rules_clear(void);
extern void styles_add_level_rule(int min_depth, int max_depth, const int* sidx, const int* weight, int count);
/* Narrative text: from style.txt (S:/M1:/M2: lines) */
extern const char* styles_get_style_display(int sidx);
extern const char* styles_get_style_short_desc(int sidx);
extern const char* styles_get_style_m1(int sidx);
extern const char* styles_get_style_m2(int sidx);
/* Narrative banner state. Positive values keep the banner visible across
 * player turns; a 0-turn banner instead consumes the next command input. */
extern int g_banner_force_redraw_remaining;
extern bool active_narrative_banner_visible(void);
extern cptr active_narrative_banner_text(void);
extern bool active_narrative_banner_consumes_input(void);
extern void clear_active_narrative_banner(void);
extern bool dismiss_active_narrative_banner(void);
extern void sdl_narrative_banner_show(bool line_delay, bool fast_fade);
extern void sdl_popup_notification_show(cptr text);
extern void styles_reload_messages_from_text(void);
extern void styles_clear_display_messages(void);
extern int p_ptr_depth_proxy(void);
extern void styles_set_loaded_level_primary(int sidx);
/* Persisted door-style variant choices for consistency across save/load */
extern int styles_get_choice_capacity(void);
extern void styles_copy_level_door_choices(byte* out_buf, int max_n);
extern void styles_load_level_door_choices(const byte* in_buf, int n);
extern void hallucination_randomize_style_transitions(void);
extern void hallucination_clear_style_transitions(void);
extern int project_path(
    u16b* gp, int range, int y1, int x1, int* y2, int* x2, u32b flg);
extern byte projectable(int y1, int x1, int y2, int x2, u32b flg);
extern void scatter(int* yp, int* xp, int y, int x, int d, int m);
extern void health_track(int m_idx);
extern void monster_race_track(int r_idx);
extern void object_kind_track(int k_idx);
extern void disturb(int stop_stealth, int unused_flag);

/* cmd1.c */
extern void apply_oath_breaking_curse(int oath_type);
extern void give_player_item(object_type * o_ptr);
extern bool player_auto_identifies_object(const object_type* o_ptr);
extern void player_mark_object_experienced(object_type* o_ptr);
extern bool player_try_identify_smithing_object(
    object_type* o_ptr, bool is_equipped, int bonus);
extern bool player_try_identify_smithing_object_on_examine(
    object_type* o_ptr, bool is_equipped);
extern bool player_auto_identify_smithing_object(
    object_type* o_ptr, bool ignore_distance_penalty);
extern bool graphics_are_ascii();
extern void new_wandering_flow(monster_type* m_ptr, int y, int x);
extern void new_wandering_destination(
    monster_type* m_ptr, monster_type* leader_ptr);
extern void drop_iron_crown(monster_type* m_ptr, const char* msg);
extern void make_alert(monster_type* m_ptr);
extern void set_alertness(monster_type* m_ptr, int alertness);
extern void perceive(void);
extern int success_chance(int sides, int skill, int difficulty);
extern int player_skill_check_success_percent(int skill, int difficulty,
    int skill_sides, int difficulty_sides);
extern int skill_check(
    monster_type* m_ptr1, int skill, int difficulty, monster_type* m_ptr2);
extern int skill_check_details(monster_type* m_ptr1, int skill, int difficulty,
    monster_type* m_ptr2, skill_roll_details* details);
extern int skill_check_details_sided(monster_type* m_ptr1, int skill,
    int difficulty, monster_type* m_ptr2, int skill_sides,
    int difficulty_sides, skill_roll_details* details);
extern int light_penalty(const monster_type* m_ptr);
extern bool check_hit(int power, bool display_roll);
extern int hit_roll(int att, int evn, const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool display_roll);
extern int hit_roll_details(int att, int evn, const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool display_roll, int* attack_die,
    int* evasion_die);
extern int total_player_attack(monster_type* m_ptr, int base);
extern int total_player_attack_ex(monster_type* m_ptr, int base,
    bool include_concentration, bool include_focus);
extern int total_player_evasion(monster_type* m_ptr, bool archery);
extern int total_monster_attack(monster_type* m_ptr, int base);
extern int total_monster_evasion(monster_type* m_ptr, bool archery);
extern int stealth_melee_bonus(const monster_type* m_ptr, bool allow_unseen);
extern int overwhelming_att_mod(monster_type* m_ptr);
extern int crit_bonus(int hit_result, int weight, const monster_race* r_ptr,
    int skill_type, bool thrown, monster_type* attacker,
    const object_type* o_ptr);
extern void ident(object_type* o_ptr);
extern void ident_on_wield(object_type* o_ptr);
extern void ident_resist(u32b flag);
extern void ident_passive(void);
extern void ident_see_invisible(const monster_type* m_ptr);
extern void ident_haunted(void);
extern void ident_hunger(void);
extern void ident_f2(u32b flag, object_type* supplied_object);
extern void ident_f3(u32b flag, object_type* supplied_object);
extern void ident_weapon_by_use(
    object_type* o_ptr, const monster_type* m_ptr, u32b flag);
extern void ident_weapon_by_use_context(object_type* o_ptr,
    const monster_type* m_ptr, u32b flag, cptr context);
extern void ident_bow_arrow_by_use(object_type* j_ptr, object_type* i_ptr,
    object_type* o_ptr, const monster_type* m_ptr, u32b bow_flag,
    u32b arrow_flag);
extern void apply_weapon_combat_effects(object_type* o_ptr,
    monster_type* m_ptr, int skill_type, int net_dam, bool fatal_blow,
    cptr armor_shatter_noun);
extern int slay_bonus(
    const object_type* o_ptr, const monster_type* m_ptr, u32b* noticed_flag);
extern int prt_after_sharpness(const object_type* o_ptr, u32b* noticed_flag);
extern void search(void);
extern void do_cmd_pickup_from_pile(void);
extern void py_pickup_aux(int o_idx);
extern void py_pickup(void);
extern bool prepare_brass_lamp_flask_replacement(
    const object_type* incoming, int* flasks_to_replace, int* flask_oil,
    bool* aborted);
extern bool commit_brass_lamp_flask_replacement(
    int flasks_to_replace, int flask_oil);
extern bool player_channel_floor_staff(object_type* donor, int floor_o_idx);
extern bool smith_oath_forbids_object(const object_type* o_ptr);
extern bool smith_oath_confirm_break(void);
extern void hit_trap(int y, int x);
extern void display_hit(
    int y, int x, int net_dam, int dam_type, bool fatal_blow);
extern int concentration_bonus(int y, int x);
extern int focused_attack_bonus(void);
extern int master_hunter_bonus(monster_type* m_ptr);
extern bool knock_back(int y1, int x1, int y2, int x2);
extern bool abort_for_mercy(monster_type* m_ptr);
extern bool abort_for_valorous(monster_type* m_ptr);
extern bool cowardly_attack(monster_type* m_ptr);
extern bool is_aoe_attack_type(int attack_type);
extern void break_mercy_oath(monster_type* m_ptr, int damage);
extern void break_valorous_oath(monster_type* m_ptr, int damage, int attack_type, int damage_source);
extern void attack_punctuation(
    char* punctuation, int net_dam, int crit_bonus_dice);
extern int count_open_adjacent_squares(int y, int x);
extern void py_attack_aux(int y, int x, int attack_type);
extern void py_attack(int y, int x, int attack_type);
extern void flanking_or_retreat(int y, int x);
extern void move_player(int dir);
extern void player_allow_trap_step(int y, int x);
extern const byte cycle[];
extern const byte chome[];
extern void run_step(int dir);

/* cmd2.c */
extern int min_depth(void);
extern void min_depth_timer_status(int* base_increment, int* additional_increment,
    int* total_increment, int* progress, int* threshold);
extern void morgoth_call_sync_loaded_stage(void);
extern void process_morgoth_call_pressure(void);
extern void note_lost_greater_vault(void);
extern void do_cmd_go_up(void);
extern void do_cmd_go_down(void);
extern void do_cmd_search(void);
extern void do_cmd_toggle_stealth(void);
extern bool do_cmd_open_aux(int y, int x);
extern void do_cmd_open(void);
extern void do_cmd_close(void);
extern void do_cmd_exchange(void);
extern void do_cmd_swap_quivers(void);
extern void do_cmd_swap_staff(void);
extern void do_cmd_fletchery(void);
extern void finish_fletching(int);
extern void do_cmd_tunnel(void);
extern bool break_free_of_web(void);
extern bool do_cmd_disarm_aux(int y, int x);
extern bool trap_disarm_power(int feat, int* power);
extern bool trap_is_rewireable(int feat);
extern int show_interaction_skill_roll_animation(cptr title, cptr action,
    int y, int x, int skill, int difficulty, skill_roll_details* roll);
extern int show_interaction_skill_roll_animation_sided(cptr title, cptr action,
    int y, int x, int skill, int difficulty, int skill_sides,
    int difficulty_sides, skill_roll_details* roll);
extern int show_interaction_skill_roll_animation_actor(monster_type* actor,
    cptr title, cptr action, int y, int x, int skill, int difficulty,
    skill_roll_details* roll);
extern void show_interaction_skill_roll_pair(cptr title, int y, int x,
    cptr first_label, const skill_roll_details* first,
    cptr second_label, const skill_roll_details* second);
extern void show_interaction_skill_roll_status(cptr title, int y, int x,
    cptr roll_label, const skill_roll_details* roll, cptr status,
    byte status_attr);
extern void do_cmd_disarm(void);
extern void do_cmd_bash(void);
extern void do_cmd_steal(void);
extern void do_cmd_alter(void);
extern bool grid_interact_available(int y, int x);
extern bool grid_interact_question(int y, int x, int* out_command,
    int* out_dir);
extern void do_cmd_spike(void);
extern void chest_release_contents(struct object_type* o_ptr, int y, int x,
    int destroy_typ);
extern bool do_cmd_walk_test(int y, int x);
extern void do_cmd_walk(void);
extern void do_cmd_jump(void);
extern void do_cmd_run(void);
extern void do_cmd_hold(void);
extern void do_cmd_pickup(void);
extern void do_cmd_rest(void);
extern int archery_range(const object_type* j_ptr);
extern int throwing_range(const object_type* i_ptr);
extern void attacks_of_opportunity(int neutralized_y, int neutralized_x);
extern void do_cmd_fire(int quiver);
extern bool do_cmd_fire_at_adjacent(int y, int x);
extern void do_cmd_throw(bool automatic);
extern void do_cmd_throw_from_slot(int slot);
extern bool throw_slot_menu_active;
extern bool throw_slot_enabled[INVEN_TOTAL];

/* cmd3.c */
extern void do_cmd_use_item_by_index(int item);
extern void do_cmd_use_item(void);
extern void do_cmd_use_item_enhanced(void);
extern void do_cmd_inven(void);
extern void do_cmd_inven_direct(void);
extern void do_cmd_equip(void);
extern void do_cmd_equip_direct(void);
extern void do_cmd_wield(object_type* default_o_ptr, int default_item);
extern void do_cmd_wield_to_slot(
    object_type* default_o_ptr, int default_item, int forced_slot);
extern void do_cmd_wield_wrapper(void);
extern void do_cmd_wield_enhanced(void);
extern void do_cmd_takeoff(object_type* default_o_ptr, int default_item);
extern bool do_cmd_jewelry_preset_apply(int preset);
extern bool do_cmd_jewelry_preset_store(int preset);
extern bool do_cmd_jewelry_preset_clear(int preset);
extern void do_cmd_jewelry_preset_shortcut(void);
extern void do_cmd_drop_item_by_index(int item);
extern bool do_cmd_drop_item_by_index_confirm(int item, bool confirm);
extern void do_cmd_drop(void);
extern bool open_supplies_menu_with_context(supply_menu_action default_action, int default_group, bool default_focus, bool default_hotkey);
extern bool open_inventory_menu_page(supply_menu_page page);
extern bool open_inventory_menu_category(inventory_menu_group group);
extern bool open_inventory_replacement_menu(inventory_menu_group group,
    const object_type* incoming, bool include_equip, bool include_supplies,
    cptr reason, int* replacement_item);
extern bool open_inventory_slot_pick_menu(const object_type* incoming,
    const bool* enabled, cptr reason, int* slot_out);
extern bool open_inventory_item_select_menu(int mode, cptr reason,
    cptr none_msg, int* item_out);
extern void do_cmd_destroy(void);
extern bool do_cmd_delete_item_by_index(int item);
extern void do_cmd_observe(void);
extern void do_cmd_observe_enhanced(void);
extern cptr item_use_action_name(const object_type* o_ptr, int item);
extern bool touch_shortcut_context_action(int binding, bool description_open,
    int* out_key, char* label, size_t label_len);
extern void do_cmd_uninscribe(void);
extern void do_cmd_inscribe(void);
extern void do_cmd_refuel_lamp(object_type* default_o_ptr, int default_item);
extern void do_cmd_refuel_torch(
    object_type* default_o_ptr, int default_item, bool is_mallorn);
extern void do_cmd_refuel(void);
extern void do_cmd_target(void);
extern void do_cmd_look(void);
extern void do_cmd_look_at(int y, int x);
extern void do_cmd_unified_look(void);
extern void do_cmd_locate(void);
extern void do_cmd_query_symbol(void);
extern void do_cmd_view_monsters(void);
extern void do_cmd_view_objects(void);
extern void show_unified_sidebar(unified_look_state* state);
extern int unified_look_find_cursor_selection(const unified_look_state* state,
    int cursor_y, int cursor_x);
extern void highlight_entity_on_map(int y, int x, bool highlight);
extern void highlight_entity_on_map_type(int y, int x, bool highlight, int entity_type);
extern bool ang_sort_comp_hook(const void* u, const void* v, int a, int b);
extern void ang_sort_swap_hook(void* u, void* v, int a, int b);
extern void py_steal(int y, int x);

/* cmd4.c */
extern object_type* smith_o_ptr;
extern void do_cmd_redraw(void);
extern void options_birth_menu(bool adult);
extern void do_cmd_character_sheet(void);
extern void character_sheet_show_birth_preview(void);
extern cptr character_sheet_skill_description(int skill);
extern cptr character_sheet_trait_description(cptr label);
extern void character_sheet_format_vital_description(cptr label, char* buf,
    size_t buflen);
extern void character_sheet_format_stat_hint(int stat, int value,
    bool has_value, char* buf, size_t buflen);
extern void character_sheet_format_trait_description(cptr label, int skill,
    int trait_score, bool proficiency, u32b aff_flag, u32b pen_flag,
    cptr desc, char* buf, size_t buflen);
extern void do_cmd_change_song(void);
extern void wipe_screen_from(int col);
extern int ability_index(int skilltype, int abilitynum);
extern byte ability_skill_color(int skilltype);
extern void ability_log_reset(void);
extern void ability_log_record_gain(int skilltype, int abilitynum);
extern void ability_log_sync_missing(void);
extern int elf_bane_bonus(monster_type* m_ptr);
extern int dwarf_bane_bonus(monster_type* m_ptr);
extern int edain_bane_bonus(monster_type* m_ptr);
extern char* bane_name[];
extern int bane_bonus(monster_type* m_ptr);
extern int bane_bonus_for_type(int bane_type_idx);
extern int artifact_bane_bonus(monster_type* m_ptr);
extern int spider_bane_bonus(void);
extern int artifact_spider_bane_bonus(void);
extern int unique_bane_bonus(monster_type* m_ptr);
extern int unique_bane_type_killed(void);
extern char* oath_name[];
extern bool oath_invalid(int i);
extern bool chosen_oath(int oath);
extern char* oath_confirmation_prompt(int oath_id);
extern char* oath_curse_message(int oath_id);
extern char* oath_permanent_message(int oath_id);
extern char* oath_death_message(int oath_id);
extern char* oath_banned_text(int oath_id);
extern char* oath_name_str(int oath_id);
extern char* oath_description(int oath_id);
extern char* oath_pledge(int oath_id);
extern char* oath_forbidden(int oath_id);
extern char* oath_reward_text(int oath_id);
extern void do_cmd_ability_screen(void);
extern int object_difficulty(object_type* o_ptr);
extern void do_cmd_smithing_screen(void);
extern void create_smithing_item(void);
#define MAIN_MENU_CHARACTER 1
#define MAIN_MENU_INVENTORY 2
#define MAIN_MENU_SMITHING 3
#define MAIN_MENU_KNOWLEDGE 4
#define MAIN_MENU_HINTS_QUESTS 5
#define MAIN_MENU_HALLS_OF_MANDOS 6
#define MAIN_MENU_MAP 7
#define MAIN_MENU_LOG_HISTORY 8
#define MAIN_MENU_STORY 9
#define MAIN_MENU_STORY_STATS 10
#define MAIN_MENU_BLITZ 11
#define MAIN_MENU_OPTIONS 12
#define MAIN_MENU_HELP 13
#define MAIN_MENU_ABOUT 14
#define MAIN_MENU_SAVE 15
#define MAIN_MENU_SAVE_QUIT 16
#define MAIN_MENU_RETURN_GAME 17
#define MAIN_MENU_MAX 17
extern cptr main_menu_title(int choice);
extern int main_menu_keyboard_key(int choice);
extern void main_menu_shortcut_label(int choice, char* buf, size_t buflen);
extern int main_menu_choice_from_key(int key);
extern bool main_menu_choice_is_disabled(int choice);
extern bool do_cmd_main_menu_execute_choice(int choice);
extern void sdl_quick_access_suggest_skill_shortcut(int skill);
extern void sdl_quick_access_suggest_ability_shortcut(int skill, int ability);
extern void sdl_quick_access_suggest_starting_shortcuts(void);
extern void sdl_quick_access_suggest_equipped_item(int tval);
extern void do_cmd_main_menu(void);
extern void do_cmd_messages(void);
extern void do_cmd_messages_with_filter(int initial_filter);
extern void do_cmd_options_aux(int page, cptr info);
extern void do_cmd_options(void);
extern void do_cmd_pane_settings(void);
extern bool do_cmd_touch_top_widget_pick_button(int slot);
extern void do_cmd_macros(void);
extern void do_cmd_keybinds(void);
extern void do_cmd_visuals(void);
extern void do_cmd_colors(void);
extern void do_cmd_note(char* note, int what_depth);
extern void do_cmd_version(void);
extern void do_cmd_feeling(void);
extern void do_cmd_knowledge_notes(void);
extern void do_cmd_knowledge_oaths(void);
extern void do_cmd_knowledge_artefacts(void);
extern void do_cmd_knowledge_monsters(void);
extern bool do_cmd_knowledge_supplies(const supply_menu_request* request);
extern void do_cmd_knowledge_objects(void);
extern void do_cmd_knowledge_kills(void);
#define KNOWLEDGE_PAGE_ARTEFACTS 0
#define KNOWLEDGE_PAGE_OBJECTS 1
#define KNOWLEDGE_PAGE_MONSTERS 2
#define KNOWLEDGE_PAGE_CURSES 3
extern void do_cmd_knowledge_browser_page(int page);
extern void ghost_challenge(void);
extern void desc_art_fake(int a_idx);
extern void apply_magic_fake(object_type* o_ptr);
extern void do_cmd_knowledge(void);
extern void add_random_curse(object_type *o_ptr);

/* cmd5.c */
extern void display_koff(int k_idx);

/* cmd6.c */
extern void do_cmd_eat_food(object_type* default_o_ptr, int default_item);
extern void do_cmd_quaff_potion(object_type* default_o_ptr, int default_item);
extern void do_cmd_use_gem(object_type* default_o_ptr, int default_item);
extern int understanding_gem_count_for_item_description(
    const object_type* viewed_o_ptr);
extern bool do_cmd_use_understanding_gem_on_item(
    const object_type* viewed_o_ptr);
extern void self_knowledge_defer_display_push(void);
extern void self_knowledge_defer_display_pop(void);
extern bool self_knowledge_display_pending(void);
extern void do_cmd_activate_staff(object_type* default_o_ptr, int default_item);
extern void do_cmd_play_instrument(
    object_type* default_o_ptr, int default_item);
extern void do_cmd_activate(void);

/* dungeon/ */
extern bool can_be_pseudo_ided(const object_type* o_ptr);
extern int value_check_aux1(const object_type* o_ptr);
extern void land(void);
extern void pseudo_id(object_type* o_ptr);
extern void pseudo_id_everything(void);
extern void id_known_specials(void);
extern void id_everything(void);
extern PlayResult play_game(void);
extern void death_spectator_view(void);
extern bool death_spectator_active(void);
extern void death_spectator_request_exit(void);
extern void reset_dungeon_state(void);

/* Former files.c split across fs/, ui/, game/, score/, metarun/, and platform modules */
extern void safe_setuid_drop(void);
extern void safe_setuid_grab(void);
extern s16b tokenize(char* buf, s16b num, char** tokens);
extern errr check_time(void);
extern errr check_time_init(void);
extern void display_player_stat_info(int row, int col);
extern void display_player_xtra_info(int mode);
extern void display_player_standard_layout_set(int skill_row, int history_row);
extern void display_player_standard_layout_clear(void);
extern void display_player(int mode);
extern void display_player_compact_set_scroll(int scroll);
extern int display_player_compact_get_max_scroll(void);
extern void display_player_compact_stats_skills_highlighted(int selected_skill);
extern void display_player_compact_stats_skills_highlighted_stat(int selected_stat);
extern void display_character_tutorial(void);

/*
 * First-run "coach": a guided walkthrough that overlays short callouts on the
 * real character-creation screens (selection -> attributes -> skills) and on
 * the live character sheet, instead of separate text pages.  Implemented in the
 * SDL layer; a no-op on builds without an SDL window.
 */
enum {
    BIRTH_COACH_SELECT = 0,
    BIRTH_COACH_STATS,
    BIRTH_COACH_SKILLS,
    BIRTH_COACH_SHEET,
    BIRTH_COACH_STAGE_MAX
};
extern void birth_coach_show(int stage);
extern void birth_coach_show_once(int stage);

extern errr file_character(cptr name, bool full);
extern bool show_buffer(cptr name, int line);
extern bool show_file(cptr name, cptr what, int line);
extern void do_cmd_help(void);
extern void do_cmd_help_menu(void);
extern void process_player_name(bool sf);
extern bool get_name(void);
extern void do_cmd_escape(int);
extern void do_cmd_morgoth_victory(void);
extern void do_cmd_suicide(void);
extern void do_cmd_save_game(void);
extern void comma_number(char* output, int number);
extern void atomonth(int number, char* output);
extern int highscore_dead(char* name);
extern bool highscore_is_empty();
extern void close_game(void);
extern void exit_game_panic(void);
extern errr create_score(high_score* the_score);
extern int score_points(const high_score* score);
extern int score_count_alive_entries(void);
extern int score_count_story_alive_entries(void);
extern int score_count_alive_entries_at_path(const char* score_path);
extern bool score_count_story_alive_entries_checked(int* alive_count);
extern bool score_count_alive_entries_at_path_checked(const char* score_path,
                                                      int* alive_count);
extern bool score_story_ledger_exists(void);
extern u32b score_sum_dead_points(void);
extern u32b score_sum_story_dead_points(void);
extern int collect_story_high_scores(high_score* out, int capacity,
                                     bool sort_by_score);
#ifdef HANDLE_SIGNALS
extern void (*(*signal_aux)(int, void (*)(int)))(int);
#endif
extern void signals_ignore_tstp(void);
extern void signals_handle_tstp(void);
extern void signals_init(void);
extern void mini_screenshot(void);
extern void prt_mini_screenshot(int col, int row);
extern int silmarils_possessed(void);
extern int has_iron_crown(void);
extern int meta_write(const metarun*);
extern errr meta_read(metarun*);
extern int meta_seek(int i);
extern int meta_fill(bool);
extern void print_story_intro(void);
extern void print_story(int last_parts, bool fade_in);
extern const char *kinslayer_try_kill(uint8_t n_sils, bool do_roll);
extern bool clear_scorefile(void);
extern bool autoload_alive_from_scores(void);
extern bool mobile_autosave_game(cptr reason);

/* generate.c */
extern void place_monster_by_flag(
    int y, int x, int flagset, u32b f, bool allow_unique, int max_depth);
extern void place_random_stairs(int y, int x);
extern byte get_nest_theme(int nestlevel);
extern byte get_pit_theme(int pitlevel);
extern void level_layout_info_current(level_layout_info* out);
extern level_partition_kind level_partition_kind_for_point(int y, int x);
extern int level_partition_index_for_point(int y, int x);
extern void level_partition_meta_get(partition_meta_save* out);
extern void level_partition_meta_set(const partition_meta_save* in);
extern void big_cave_type_rules_clear(void);
extern void big_cave_type_set_rule(int depth, int ice_weight, int fire_weight, int pois_weight);
extern big_cave_type_t big_cave_type_pick_for_depth(int depth);
extern big_cave_type_t level_partition_big_cave_type_for_point(int y, int x);
extern big_cave_type_t level_partition_big_cave_type_for_index(int pi);
extern void log_partition_debug_for_point(const char* tag, int y, int x);
extern void skeleton_note_level_reset(void);
extern void reset_hint_skeleton_state(void);
extern void skeleton_note_get_state(skeleton_note_state_save* out);
extern void skeleton_note_set_state(const skeleton_note_state_save* in);
extern void hint_messages_level_reset(void);
extern void hint_messages_ensure_level_state(void);
extern byte hint_messages_count_for_save(void);
extern s16b hint_messages_level_depth_for_save(void);
extern s16b hint_messages_map_wid_for_save(void);
extern s16b hint_messages_map_hgt_for_save(void);
extern byte hint_messages_message_line_count(int index);
extern const char* hint_messages_message_line(int index, int line);
extern void hint_messages_message_meta(int index, hint_message_meta* out);
extern void hint_text_for_current_platform(const char* src, char* out,
    size_t out_sz);
extern bool hint_messages_short_tip(int index, char* out, size_t out_sz);
extern bool hint_messages_short_tip_for_source(int y, int x, char* out,
    size_t out_sz);
extern void hint_messages_clear_for_load(s16b level_depth, s16b map_wid, s16b map_hgt);
extern int hint_messages_add_for_load(
    const char lines[][100], int line_count, const hint_message_meta* meta);
extern int hint_messages_add_note_lines(
    const char note_lines[][100], const hint_message_meta* meta);
extern void show_hint_message_screen(int index);
extern void trigger_chasm_sanctum_ambush_if_needed(int y, int x);
extern void generate_cave(void);

#ifdef ALLOW_DEBUG
extern void debug_run_quest_roulette(void);
extern int debug_get_quest_lottery_winner(void);
#endif /* ALLOW_DEBUG */

/* init/ lifecycle and path initialization */
extern void init_file_paths(char* path);
extern void display_introduction(void);
extern void init_angband(void);
extern void autoinscribe_clean(void);
extern void autoinscribe_init(void);
extern void re_init_some_things(void);
extern NavResult initial_menu(bool *start_new);
extern void cleanup_angband(void);

/* load.c */
extern bool load_player(void);
extern bool load_meta(void);

/* melee/melee-attack*.c, melee/melee-combat-display.c */
extern int protection_roll(int typ, bool melee);
extern int p_min(int typ, bool melee);
extern int p_max(int typ, bool melee);
extern int get_sides(int attack);
extern int dodging_bonus(void);
extern bool blocking_bonus_active(void);
extern bool make_attack_normal(monster_type* m_ptr);
extern bool make_attack_ranged(monster_type* m_ptr, int attack);
extern void mon_cloud(int m_idx, int typ, int dd, int ds, int dif, int rad);
extern void cloud_surround(int r_idx, int* typ, int* dd, int* ds, int* rad);
extern void shriek(monster_type* m_ptr);
extern void new_combat_round(void);
extern void update_combat_rolls1(const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool vis, int att, int att_roll, int evn,
    int evn_roll);
extern void update_combat_rolls1b(
    const monster_type* m_ptr1, const monster_type* m_ptr2, bool vis);
extern void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps,
    int prot, int prt_percent, int dam_type, bool melee);
extern void update_combat_rolls2_combo(int dd, int ds, int dam, int dd2,
    int ds2, int dam2, int pd, int ps, int prot, int prt_percent,
    int dam_type, bool melee);
extern void update_combat_rolls_no_damage(void);
extern void display_combat_rolls(void);
extern void display_combat_roll_line_at(int row, int base_col_offset,
    const combat_roll* roll);
extern int combat_roll_emit_tokens(const combat_roll* roll,
    combat_roll_token* out, int max);
extern int pane_log_combat_row_tokens(int row, const combat_roll_token** out);
extern bool pane_log_overlay_message_row(int row, cptr* out_text,
    byte* out_attr);
extern void add_combat_round_to_history(void);
extern void do_cmd_combat_history(void);
extern void display_combat_round_details(combat_history_round* round);
extern void do_betrayal_ring_amulet();

/* melee/melee-movement*.c, melee/melee-process.c, melee/melee-util.c */
extern bool attacker_at(int y, int x);
extern int adj_mon_count(int y, int x);
extern int get_scent(int y, int x);
extern bool cave_exist_mon(
    monster_race* r_ptr, int y, int x, bool occupied_ok, bool can_dig);
extern int cave_passable_mon(monster_type* m_ptr, int y, int x, bool* bash);
extern void tell_allies(int y, int x, u32b flag);
extern void process_monsters(s16b minimum_energy);
extern void calc_morale(monster_type* m_ptr);
extern void calc_stance(monster_type* m_ptr);
extern void monster_perception(
    bool player_centered, bool main_roll, int difficulty);

/* monster/ -- recall & lore display */
extern void describe_monster(
    int r_idx, bool spoilers, const monster_type* m_ptr);
extern void roff_top(int r_idx);
extern int screen_roff(int r_idx, const monster_type* m_ptr);
extern void display_roff(int r_idx, const monster_type* m_ptr);

/* monster/ -- monster lifecycle (list, selection, placement, summoning) */
extern s16b poly_r_idx(const monster_type* m_ptr);
extern void delete_monster_idx(int i);
extern void delete_monster(int y, int x);
extern void compact_monsters(int size);
extern void wipe_mon_list(void);
extern s16b mon_pop(void);
extern errr get_mon_num_prep(void);
extern s16b get_mon_num(
    int level, bool special, bool allow_non_smart, bool vault);
extern void display_monlist(void);
extern void monster_desc(
    char* desc, size_t max, const monster_type* m_ptr, int mode);
extern void monster_desc_race(char* desc, size_t max, int r_idx);
extern void lore_probe_aux(int r_idx);
extern void lore_treasure(int m_idx, int num_item);
extern int monster_skill(monster_type* m_ptr, int skill_type);
extern int monster_stat(monster_type* m_ptr, int stat_type);
extern void update_mon(int m_idx, bool full);
extern void update_monsters(bool full);
extern bool detect_monster_noise(monster_type* m_ptr, int skill);
extern s16b monster_carry(int m_idx, object_type* j_ptr);
extern int monster_base_armour_sides(const monster_type* m_ptr);
extern int monster_song_hp_loss(const monster_type* m_ptr);
extern void monster_add_song_hp_loss(monster_type* m_ptr, int amount);
extern void monster_swap(int y1, int x1, int y2, int x2);
extern bool monster_race_is_vala(int r_idx);
extern bool monster_clear_vala_state(monster_type* m_ptr);
extern s16b player_place(int y, int x);
extern s16b monster_place(int y, int x, monster_type* n_ptr);
extern void calc_monster_speed(int y, int x);
extern void set_monster_haste(s16b m_idx, s16b counter, bool message);
extern void set_monster_slow(s16b m_idx, s16b counter, bool message);
extern void produce_cloud(monster_type* m_ptr);
extern s16b monster_lookup_guid(u64b guid);
extern s16b monster_lookup_guid_text(const char* text);
extern bool place_monster_by_guid(
    int y, int x, u64b guid, bool slp, bool ignore_depth, monster_type* summoner);
extern void monster_special_vault_debug_context(
    int* build_vault_type, bool* exact_token);
extern void log_live_special_vault_only_monsters(const char* reason);
extern bool monster_special_vault_selection_allowed(void);
extern bool monster_special_vault_only_allowed_at(int y, int x);
extern bool place_monster_one(
    int y, int x, int r_idx, bool slp, bool ingnore_depth, monster_type* m_ptr);
extern bool place_monster_aux(int y, int x, int r_idx, bool slp, bool grp);
extern bool place_monster(int y, int x, bool slp, bool grp, bool vault);
extern bool quest_monster_spawn_okay(int r_idx);
extern bool alloc_monster(bool on_stairs, bool force_undead);
extern bool summon_specific(int y1, int x1, int lev, int type);
extern bool reproduce_monster(int old_m_idx, int new_r_idx);
extern void message_pain(int m_idx, int dam);

/* object/object-info.c */
#ifndef OBJECT_INFO_SCREEN_ACTION_DEFINED
#define OBJECT_INFO_SCREEN_ACTION_DEFINED
typedef struct object_info_screen_action
{
    int key;
    cptr token;
} object_info_screen_action;
#endif

extern bool object_info_out(const object_type* o_ptr);
extern cptr object_lore_select_base_text(const object_type* o_ptr, char* out,
    size_t out_sz);
extern void note_info_screen(const object_type* o_ptr);
extern void object_info_screen(const object_type* o_ptr);
extern void object_info_screen_multi(const object_type** objects, const char** headings, int count);
extern char object_info_screen_multi_with_actions(const object_type** objects,
    const char** headings, int count, cptr footer,
    const object_info_screen_action* actions, int action_count);
extern bool object_info_overlay_show_multi(const object_type** objects,
    const char** headings, int count);
extern void object_info_overlay_clear(void);
extern void describe_item_with_comparisons(int item_index, bool include_comparisons);
extern char describe_item_with_floor_actions(int item_index,
    bool include_comparisons);

/* object/object-flavor.c, object/object-flags.c, object/object-desc.c, object/object-ui-*.c */
extern bool easter_time(void);
extern void flavor_init(void);
extern void reset_visuals(bool prefs);
extern void object_flags(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
extern void object_flags4(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4);
extern void object_flags_known(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
extern void object_flags_known4(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4);
extern bool object_grants_ability(
    const object_type* o_ptr, int skilltype, int abilitynum);
extern void strip_name(char* buf, int k_idx);
extern void object_desc(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
extern void object_desc_floor(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
extern void object_desc_spoil(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
extern void identify_random_gen(const object_type* o_ptr);
extern byte object_attr_graphics_override(
    const object_type* o_ptr, byte base_attr);
extern char object_char_graphics_override(
    const object_type* o_ptr, char base_char);
extern char index_to_label(int i);
extern s16b label_to_inven(int c);
extern s16b label_to_equip(int c);
extern s16b wield_slot(const object_type* o_ptr);
extern cptr describe_empty_slot(int i);
extern cptr mention_use(int i);
extern cptr describe_use(int i);
extern bool object_is_searched_skeleton(const object_type* o_ptr);
extern bool item_tester_okay(const object_type* o_ptr);
extern int scan_floor(int* items, int size, int y, int x, int mode);
extern bool get_item_okay(int item);
extern bool get_item_allow(int item);
extern void display_inven(void);
extern void display_equip(void);
extern void display_supplies(void);
extern void show_inven(void);
extern void show_equip(void);
extern void show_inven_enhanced(void);
extern bool inventory_menu_set_include_equip(bool include);
extern bool inventory_menu_set_expand_supplies(bool enabled);
extern void show_equip_enhanced(void);
extern void show_floor(const int* floor_list, int floor_num);
extern void toggle_inven_equip(void);
extern bool get_item(int* cp, cptr pmt, cptr str, int mode);
extern bool player_can_treat_as_throwing(const object_type* o_ptr);
extern bool player_can_treat_as_throwing_flags(const object_type* o_ptr, u32b f3);
extern bool weapon_is_impale_eligible(const object_type* o_ptr);
extern int get_paired_artefact(int art_idx);
extern bool potion_has_thrown_effect(const object_type* o_ptr);

/* object/object-*.c */
extern void excise_object_idx(int o_idx);
extern void delete_object_idx(int o_idx);
extern void delete_object(int y, int x);
extern void compact_objects(int size);
extern void wipe_o_list(void);
extern s16b o_pop(void);
extern object_type* get_first_object(int y, int x);
extern object_type* get_next_object(const object_type* o_ptr);
extern void get_obj_num_prep(void);
extern s16b get_obj_num(int level);
extern void object_known(object_type* o_ptr);
extern void object_aware(object_type* o_ptr);
extern void object_tried(object_type* o_ptr);
extern bool object_has_ego_flag4(const object_type* o_ptr, u32b flag);
extern s32b object_value(const object_type* o_ptr);
extern bool object_similar(const object_type* o_ptr, const object_type* j_ptr);
extern void object_absorb(object_type* o_ptr, object_type* j_ptr);
extern s16b lookup_kind(int tval, int sval);
extern void object_wipe(object_type* o_ptr);
extern void object_copy(object_type* o_ptr, const object_type* j_ptr);
extern byte object_chest_trap_flags(const object_type* o_ptr);
extern void object_prep(object_type* o_ptr, int k_idx);
extern void object_refresh_weight(object_type* o_ptr);
extern void object_into_artefact(object_type* o_ptr, artefact_type* a_ptr);
extern u32b object_kind_pval_flags1(const object_kind* k_ptr);
extern u32b artefact_pval_flags1(const artefact_type* a_ptr);
extern u32b ego_item_pval_flags1(const ego_item_type* e_ptr);
extern u32b object_pval_flags1(const object_type* o_ptr);
extern void object_apply_pval_delta_with_mask(object_type* o_ptr, u32b mask, int delta);
extern bool object_apply_ego_affix(object_type* o_ptr, int e_idx, bool smithing);
extern bool object_break_brass_lantern(object_type* o_ptr);
extern bool object_is_fire_broken(const object_type* o_ptr);
extern bool object_has_broken_prefix(const object_type* o_ptr);
extern bool object_break_shafted_weapon_by_fire(object_type* o_ptr);
extern bool object_repair_fire_broken_weapon(object_type* o_ptr);
extern void object_into_special(object_type* o_ptr, int lev, bool smithing);
extern void check_artifact_visibility(void);
extern void apply_magic(object_type* o_ptr, int lev, bool okay, bool good,
    bool great, bool allow_insta);
#ifndef DROP_QUALITY_T_DEFINED
#define DROP_QUALITY_T_DEFINED
typedef enum
{
    DROP_QUALITY_NORMAL = 0,
    DROP_QUALITY_GOOD = 1,
    DROP_QUALITY_GREAT = 2,
    DROP_QUALITY_SUPERB = 3,
    DROP_QUALITY_ARTEFACT = 4
} drop_quality;
#endif
#define DROP_BONUS_GOOD 5
#define DROP_BONUS_GREAT 10
#define DROP_BONUS_SUPERB 15
#define DROP_BONUS_ARTEFACT 20
#define DROP_GREAT_ARTEFACT_WEIGHT_MULTIPLIER 5
#define DROP_CHEST_NOBLE_RARITY_BONUS 20
#ifndef DROP_PROFILE_T_DEFINED
#define DROP_PROFILE_T_DEFINED
typedef struct
{
    int weight_weapon;
    int weight_armor;
    int weight_jewelry;
    int weight_supply;
    int supply_potion;
    int supply_herb;
    int supply_gem;
    int supply_staff;
    int supply_light;
    int supply_arrows;
    int supply_tunneling;
    bool allow_damaged;
} drop_profile;
#endif
extern void drop_profile_for_partition_kind(level_partition_kind kind,
    drop_profile* out);
extern void drop_profile_for_partition_kind_source(level_partition_kind kind,
    partition_drop_source_t source, drop_profile* out);
extern drop_quality drop_quality_from_flags(bool good, bool great, bool superb);
extern void drop_profile_default(drop_profile* profile);
extern void partition_config_reset(void);
extern void partition_config_set_drop_profile(level_partition_kind kind,
    partition_drop_source_t source, const drop_profile* profile);
extern void partition_config_set_floor_rules(level_partition_kind kind,
    bool allow_floor_drops);
extern void partition_config_set_base_monster_scale(level_partition_kind kind,
    int numerator, int denominator);
extern void partition_config_set_direct_monster_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count);
extern void partition_config_set_depth_monster_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count, int scale_pct_at_depth_20,
    int hard_cap_divisor);
extern void partition_config_set_object_rules(level_partition_kind kind,
    int room_divisor, int corridor_divisor);
extern void partition_config_set_metal_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count, int min_depth);
extern void partition_config_set_discovery_text(level_partition_kind kind,
    cptr text);
extern void partition_config_set_big_cave_discovery_text(
    big_cave_type_t cave_type, cptr text);
extern cptr partition_config_get_discovery_text(level_partition_kind kind,
    big_cave_type_t cave_type);
extern bool object_uses_smithing_difficulty(const object_type* o_ptr);
extern int object_smithing_difficulty(const object_type* o_ptr);
extern int object_weight_rarity(const object_type* o_ptr, int depth);
extern void drop_system_init(void);
extern bool drop_generate_object(int depth, drop_quality quality, int droptype,
    bool allow_artefacts, object_type* out);
extern bool drop_generate_object_with_bonus(
    int depth, drop_quality quality, int droptype, int extra_bonus,
    bool allow_artefacts, object_type* out);
extern bool drop_generate_object_with_bonus_depths(
    int depth, int min_depth_penalty_depth, drop_quality quality, int droptype,
    int extra_bonus, bool allow_artefacts, object_type* out);
extern bool drop_generate_object_profiled(int depth, drop_quality quality,
    int droptype, int extra_bonus, bool allow_artefacts,
    const drop_profile* profile, object_type* out);
extern bool drop_generate_object_profiled_depths(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    int extra_bonus, bool allow_artefacts, const drop_profile* profile,
    object_type* out);
extern bool drop_generate_object_profiled_depths_biased(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    int extra_bonus, bool allow_artefacts, int artefact_weight_multiplier,
    const drop_profile* profile, object_type* out);
extern bool drop_generate_guaranteed_artefact(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    const drop_profile* profile, object_type* out);
extern bool drop_generate_chasm_sanctum_object(int depth, object_type* out);
extern void drop_set_chest_vault_type(int vault_type);
extern void drop_set_chest_mode(int mode);
extern void drop_set_chest_material_weights(int wooden_pct, int steel_pct,
    int jewelled_pct);
extern void drop_clear_chest_material_weights(void);

/* thrall_quest.c */
extern bool is_alert_thrall(monster_type* m_ptr);
extern void init_thrall_quest(monster_type* m_ptr);
extern cptr get_thrall_quest_item_name(byte quest_item);
extern int player_has_thrall_quest_item(byte quest_item);
extern bool handle_thrall_interaction(monster_type* m_ptr);
extern void complete_thrall_quest(monster_type* m_ptr, int item_slot);
extern bool object_is_damaged_item(const object_type* o_ptr);
extern bool object_can_repair_damage(const object_type* o_ptr);
extern int find_broken_item_to_upgrade(void);
extern bool repair_damaged_item(int slot);
extern bool is_smithed_by_player(const object_type* o_ptr);
extern bool upgrade_broken_item(int slot);
extern bool reveal_random_artifact(void);

extern bool make_object(
    object_type* j_ptr, drop_quality quality, int objecttype);
extern bool make_object_with_profile(object_type* j_ptr, drop_quality quality,
    int objecttype, const drop_profile* profile);
extern bool make_guaranteed_artefact(
    object_type* j_ptr, drop_quality quality, int objecttype);
extern bool make_guaranteed_artefact_with_profile(object_type* j_ptr,
    drop_quality quality, int objecttype, const drop_profile* profile);
extern bool prep_object_theme(int themetype);
extern s16b floor_carry(int y, int x, object_type* j_ptr);
extern s16b drop_near(object_type* j_ptr, int chance, int y, int x);
extern void acquirement(int y1, int x1, int num, drop_quality quality);
extern void place_object(int y, int x, drop_quality quality, int droptype,
    bool allow_artefacts);
extern void place_trap(int y, int x);
extern void reveal_trap(int y, int x);
extern void place_secret_door(int y, int x);
extern void place_closed_door(int y, int x);

extern void place_random_door(int y, int x);
extern void place_forge(int y, int x);
extern void inven_item_charges(int item);
extern void inven_item_describe(int item);
extern void inven_item_increase(int item, int num);
extern void inven_item_optimize(int item);
extern void floor_item_charges(int item);
extern void floor_item_describe(int item);
extern void floor_item_increase(int item, int num);
extern void floor_item_optimize(int item);
extern void check_pack_overflow(void);
extern bool inven_carry_okay(const object_type* o_ptr);
extern bool inven_carry_okay_after_removing(
    const object_type* o_ptr, int remove_item, int remove_amt);
extern bool inven_carry_limit_failed(void);
#ifndef INVENTORY_LIMIT_GROUP_DEFINED
#define INVENTORY_LIMIT_GROUP_DEFINED
enum inventory_limit_group
{
    INV_LIMIT_NONE = 0,
    INV_LIMIT_ARROW,
    INV_LIMIT_BOW,
    INV_LIMIT_STAFF,
    INV_LIMIT_HORN,
    INV_LIMIT_DIGGING,
    INV_LIMIT_BOOTS,
    INV_LIMIT_GLOVES,
    INV_LIMIT_HELM_CROWN,
    INV_LIMIT_ROUND_SHIELD,
    INV_LIMIT_OTHER_SHIELD,
    INV_LIMIT_CLOAK,
    INV_LIMIT_SOFT_ARMOUR,
    INV_LIMIT_MAIL,
    INV_LIMIT_MELEE_WEAPON,
    INV_LIMIT_THROWABLE,
    INV_LIMIT_SUPPLY_WEIGHT,
    INV_LIMIT_TORCHES,
    INV_LIMIT_BRASS_LAMPS,
    INV_LIMIT_LESSER_JEWEL,
    INV_LIMIT_FEANORIAN_LAMP
};
#endif
extern inventory_menu_group inventory_menu_group_for_limit_group(
    enum inventory_limit_group limit_group);
extern enum inventory_limit_group inven_carry_limit_group(void);
extern cptr inven_carry_limit_label(void);
extern int inven_carry_limit_value(void);
extern bool inven_carry_limit_is_supply_weight(void);
extern bool inven_carry_limit_can_replace(const object_type* o_ptr);
extern enum inventory_limit_group inventory_limit_group_for_object(
    const object_type* o_ptr);
extern bool inventory_limit_info_for_object(const object_type* o_ptr,
    enum inventory_limit_group* group, int* limit, int* cost);
extern int inventory_limit_usage_for_group(enum inventory_limit_group group);
extern int inventory_limit_limit_for_group(enum inventory_limit_group group);
extern int inventory_limit_space_for_object(const object_type* o_ptr);
extern bool inventory_limit_object_matches_group(
    enum inventory_limit_group group, const object_type* o_ptr);
extern cptr inventory_limit_group_name(enum inventory_limit_group group);
extern int object_stack_limit(const object_type* o_ptr);
extern s16b inven_carry(object_type* o_ptr, bool combine_ammo);
extern s16b inven_takeoff(int item, int amt);
extern void inven_drop(int item, int amt);
extern void inven_enforce_current_pack_limits(void);
extern void combine_pack(void);
extern void reorder_pack(bool display_message);
extern void steal_object_from_monster(int y, int x);
extern byte allow_altered_inventory;

/* randart.c */
extern void make_random_name(char* random_name, size_t max);
extern s32b artefact_power(int a_idx);
extern void build_randart_tables(void);
extern void free_randart_tables(void);
extern errr do_randart(u32b randart_seed, bool full);
extern bool make_one_randart(
    object_type* o_ptr, int art_power, bool namechoice);
extern void artefact_wipe(int a_idx);
extern bool can_be_randart(const object_type* o_ptr);

/* save.c */
extern bool save_player(void);

/* object/object-autoinscribe.c */
extern int do_cmd_autoinscribe_item(s16b k_idx);
extern int get_autoinscription_index(s16b k_idx);
extern void obliterate_autoinscription(s16b kind);
extern void autoinscribe_ground(void);
extern void autoinscribe_pack(void);
extern int remove_autoinscription(s16b kind);
extern int add_autoinscription(s16b kind, cptr inscription);
extern int apply_autoinscription(object_type* o_ptr);

/*use-obj.c*/
extern int consumable_healing_points(const object_type* o_ptr);
extern bool use_object(object_type* o_ptr, bool* ident);
extern bool use_sanctity_gem_on(object_type* target_o_ptr, bool* ident);

/* Split support/ui utility modules, formerly util.c */
#include "support/command.h"
#include "support/editing-buffer.h"
#include "support/feedback.h"
#include "support/geometry.h"
#include "support/input.h"
#include "support/macro.h"
#include "support/message.h"
#include "support/misc.h"
#include "support/prompt.h"
#include "support/quark.h"
#include "support/screen.h"
#include "support/text-output.h"
#include "support/utf8.h"
#include "ui/colors.h"
#include "ui/menu-click.h"

/* SDL3-based file I/O operations */
extern errr sdl_fclose(SDL_IOStream* stream);
extern errr sdl_fgets(SDL_IOStream* stream, char* buf, size_t n);
extern errr sdl_fputs(SDL_IOStream* stream, cptr buf, size_t n);
extern errr sdl_read(SDL_IOStream* stream, char* buf, size_t n);
extern errr sdl_write(SDL_IOStream* stream, cptr buf, size_t n);
extern errr sdl_seek(SDL_IOStream* stream, Sint64 offset);
extern Sint64 sdl_tell(SDL_IOStream* stream);
extern Sint64 sdl_size(SDL_IOStream* stream);

/* Legacy - still used by some systems */
extern errr check_modification_date(int fd, cptr template_file);
extern errr check_modification_date_sdl(cptr raw_path, cptr txt_path);

extern void text_to_ascii(char* buf, size_t len, cptr str);
extern void ascii_to_text(char* buf, size_t len, cptr str);
extern bool utf8_has_non_ascii(cptr str);
extern bool utf8_has_non_ascii_n(cptr str, int len);
extern int utf8_sequence_len(cptr str);
extern int utf8_sequence_len_n(cptr str, int len);
extern int utf8_display_width_n(cptr str, int len);
extern int utf8_safe_prefix_len(cptr str, int len);
extern int macro_find_exact(cptr pat);
extern errr macro_add(cptr pat, cptr act);
extern errr macro_init(void);
extern errr macro_free(void);
extern errr macro_trigger_free(void);
extern void flush(void);
extern void flush_fail(void);
extern char inkey(void);
extern void bell(cptr reason);
extern void sound(int val);
extern void sound_delayed(int val, unsigned int delay_ms);
extern void sdl_present_batch_begin(void);
extern void sdl_present_batch_end(void);
extern void sdl_left_panel_source_invalidate(void);
extern s16b quark_add(cptr str);
extern cptr quark_str(s16b i);
extern bool parse_u64b_hex(const char* text, u64b* out);
extern errr quarks_init(void);
extern errr quarks_free(void);
extern s16b message_num(void);
extern cptr message_str(s16b age);
extern u16b message_type(s16b age);
extern byte message_color(s16b age);
extern u32b message_sequence(s16b age);
extern errr message_color_define(u16b type, byte color);
extern void message_add(cptr str, u16b type);
extern void message_set_latest_sequence(u32b sequence);
extern u32b log_history_next_sequence(void);
extern void log_history_note_sequence(u32b sequence);
extern errr messages_init(void);
extern void messages_free(void);
extern void move_cursor(int row, int col);
extern void msg_print(cptr msg);
extern void msg_format(cptr fmt, ...);
extern void msg_debug(cptr fmt, ...);
extern void message(u16b message_type, s16b extra, cptr message);
extern void message_format(u16b message_type, s16b extra, cptr fmt, ...);
extern void message_flush(void);
extern void screen_save(void);
extern void screen_load(void);
extern void screen_load_quiet(void);
extern void screen_clear_all_terms_no_fresh(void);
extern void message_discard_pending(void);
extern void startup_loading_overlay_arm(void);
extern void startup_loading_overlay_disarm(void);
extern bool sdl_welcome_screen_show_intro(int intro_style, bool show_wizard);
extern bool sdl_welcome_screen_show_menu(bool show_wizard, bool new_metarun);
extern bool sdl_welcome_screen_set_status(cptr status);
extern bool sdl_welcome_screen_show_loading(cptr status);
extern void sdl_welcome_screen_hide(void);
extern bool sdl_welcome_screen_active(void);
extern void sdl_poetry_screen_begin(cptr title, cptr body,
    cptr transition, cptr prompt);
extern void sdl_poetry_sequence_layout_begin(void);
extern void sdl_poetry_sequence_layout_end(void);
extern void sdl_poetry_screen_begin_choices(cptr title);
extern void sdl_poetry_screen_begin_blocks(cptr title, cptr prompt);
extern int sdl_poetry_screen_add_block(cptr text, byte attr);
extern void sdl_poetry_screen_set_block_visible(int block, bool visible);
extern void sdl_poetry_screen_set_block_alpha(int block, byte alpha);
extern void sdl_poetry_screen_set_block_attr(int block, byte attr);
extern void sdl_poetry_screen_add_choice(int choice, cptr label, cptr body);
extern void sdl_poetry_screen_set_choice_visible(int choice, bool visible,
    byte label_attr, byte body_attr);
extern void sdl_poetry_screen_set_choice_alpha(int choice, byte alpha);
extern void sdl_poetry_screen_set_highlight(int choice);
extern void sdl_poetry_screen_set_prompt(cptr prompt, bool visible);
extern void sdl_poetry_screen_set_alpha(byte title_alpha, byte body_alpha,
    byte transition_alpha, byte prompt_alpha);
extern void sdl_poetry_screen_update(bool title_visible,
    byte title_attr, bool body_visible, byte body_attr,
    bool transition_visible, byte transition_attr, bool prompt_visible);
extern void sdl_poetry_screen_hide(void);
extern bool sdl_poetry_screen_active(void);
extern bool sdl_pause_text_screen_begin(void);
extern void sdl_pause_text_screen_add_line(cptr text, byte attr, int indent);
extern void sdl_pause_text_screen_set_visible_lines(int visible_lines);
extern void sdl_pause_text_screen_hide(void);
extern bool sdl_pause_text_screen_active(void);
extern bool sdl_tale_screen_begin(cptr title);
extern void sdl_tale_screen_add_entry(cptr heading, cptr body);
extern void sdl_tale_screen_set_manuscript(bool enabled);
extern int sdl_tale_screen_current_page_entry_count(void);
extern int sdl_tale_screen_current_page_entry_at(int position);
extern int sdl_tale_screen_entry_character_count(int entry);
extern int sdl_tale_screen_entry_character_at(int entry, int position);
extern void sdl_tale_screen_set_active_entry(int active_entry, byte alpha);
extern void sdl_tale_screen_set_typewriter_entry(int active_entry,
    int visible_characters, bool cursor_visible);
extern void sdl_tale_screen_set_prompt(cptr prompt, bool visible,
    bool final);
extern bool sdl_tale_screen_advance_page(void);
extern bool sdl_tale_screen_is_last_page(void);
extern void sdl_tale_screen_hide(void);
extern bool sdl_tale_screen_active(void);
extern void sdl_halls_screen_begin(cptr subtitle, cptr page_status,
    bool detailed, int outside_choice);
extern int sdl_halls_screen_page_capacity(bool detailed);
extern void sdl_halls_screen_add_entry(int choice, cptr rank, cptr name,
    cptr score, cptr outcome, cptr details, cptr honors,
    cptr score_increases, cptr score_decreases, byte attr, bool selected);
extern void sdl_halls_screen_set_empty(cptr text);
extern void sdl_halls_screen_add_action(int choice, cptr label, byte attr,
    bool enabled);
extern void sdl_halls_screen_hide(void);
extern bool sdl_halls_screen_active(void);
extern bool sdl_character_sheet_screen_active(void);
extern void sdl_character_sheet_screen_hide(void);
extern void sdl_character_sheet_screen_begin_live(int focus_choice);
extern void sdl_character_sheet_screen_begin_birth_preview(void);
extern void sdl_character_sheet_screen_add_live_item(int choice, int kind,
    int skill, int value_kind, cptr label, cptr desc);
extern void sdl_character_sheet_screen_show_birth_stats(const int* stats,
    const int* costs, int selected_stat, int points_left);
extern void sdl_character_sheet_screen_show_birth_skills(const int* old_base,
    const int* skill_gain, const int* costs, int selected_skill,
    int points_left);
extern void sdl_character_sheet_screen_begin_select(int focus_choice,
    cptr title);
extern void sdl_character_sheet_screen_set_select_menu_style(bool enabled);
extern void sdl_character_sheet_screen_set_select_dynamic_description(
    bool enabled);
extern int sdl_character_sheet_screen_select_menu_rows_per_column(void);
extern void sdl_character_sheet_screen_add_select_row(int choice, cptr label,
    int attr, cptr desc);
extern void sdl_character_sheet_screen_set_last_select_row_reset(
    int reset_choice);
extern void sdl_character_sheet_screen_set_last_select_row_confirmable(
    bool confirmable);
extern void sdl_character_sheet_screen_add_select_detail(cptr text, int attr,
    cptr desc);
extern void sdl_character_sheet_screen_set_select_detail_size_hint(
    int stat_rows, int ability_rows, int trait_rows);
extern void sdl_character_sheet_screen_set_select_ability_rows(int rows);
extern void sdl_character_sheet_screen_set_select_title_detail(cptr title,
    cptr suffix, int suffix_attr);
extern void sdl_character_sheet_screen_add_select_title_candidate(cptr title,
    cptr suffix);
extern void sdl_character_sheet_screen_begin_select_rating_summary(cptr title);
extern void sdl_character_sheet_screen_add_select_rating(cptr group,
    cptr stars, int count, int attr, cptr desc);
extern void sdl_character_sheet_screen_add_select_heading(cptr label);
extern void sdl_character_sheet_screen_set_select_intro(cptr text);
extern void sdl_character_sheet_screen_set_select_frame(cptr top, cptr bottom);
extern void sdl_character_sheet_screen_show_select_choice_page_only(void);
extern void sdl_character_sheet_screen_open_select_choice_page(void);
/* Race "book" page-turn: click ids (mouse) + navigation API for birth/. */
#define SDL_SELECT_CLICK_PAGE_PREV (-20)
#define SDL_SELECT_CLICK_PAGE_NEXT (-21)
/* Mobile character carousel: triangle ids that step to the prev/next hero. */
#define SDL_SELECT_CLICK_CAROUSEL_PREV (-22)
#define SDL_SELECT_CLICK_CAROUSEL_NEXT (-23)
/* Narrative book: an on-screen exit button for mouse/touch traversal. */
#define SDL_SELECT_CLICK_CLOSE (-24)
extern bool sdl_character_sheet_screen_mobile_carousel_active(void);
extern void sdl_character_sheet_screen_reset_select_page(void);
extern int sdl_character_sheet_screen_select_page(void);
extern int sdl_character_sheet_screen_select_page_count(void);
extern bool sdl_character_sheet_screen_page_turning(void);
extern void sdl_character_sheet_screen_begin_page_turn(int dir);
extern void sdl_character_sheet_screen_begin_page_turn_to(int page);
extern bool sdl_character_sheet_screen_scroll_book(int direction);
extern void sdl_character_sheet_screen_set_select_size_hint(cptr longest_desc);
extern void sdl_character_sheet_screen_add_select_description_candidate(
    cptr text);
extern void sdl_character_sheet_screen_add_select_welcome(cptr text);
extern void sdl_character_sheet_screen_set_select_description(cptr text);
extern bool sdl_character_sheet_screen_commit_select(int selected_index);
/* Narrative "book" (N pages with optional actions): quest text, stats, etc.  Reuses the page-turn
 * navigation accessors above (select_page / page_turning / begin_page_turn). */
extern void sdl_character_sheet_screen_begin_book(cptr title);
extern void sdl_character_sheet_screen_add_book_paragraph(cptr text);
extern void sdl_character_sheet_screen_add_book_paragraph_colored(cptr text,
    int attr);
extern void sdl_character_sheet_screen_add_book_action(cptr text, int choice);
extern void sdl_character_sheet_screen_add_book_action_colored(cptr text,
    int choice, int attr);
extern void sdl_character_sheet_screen_add_book_contents(cptr label,
    int choice, int page);
extern void sdl_character_sheet_screen_set_book_lamp(u32b current,
    u32b maximum, int page);
extern void sdl_character_sheet_screen_set_book_close_button(bool enabled);
extern void sdl_character_sheet_screen_set_book_close_label(cptr label);
extern void sdl_character_sheet_screen_set_book_target_page_count(
    int page_count);
extern int sdl_character_sheet_screen_book_contents_page(int contents_index);
extern void sdl_character_sheet_screen_break_book_page(void);
extern void sdl_character_sheet_screen_highlight_book_paragraph(void);
extern void sdl_character_sheet_screen_commit_book(void);
extern void sdl_character_sheet_screen_set_book_page(int page);
extern bool screen_saved_fullscreen_active(void);
extern void screen_push_supporting_panes_hidden(void);
extern void screen_pop_supporting_panes_hidden(void);
extern bool screen_supporting_panes_hidden_active(void);
extern void screen_set_startup_supporting_panes_hidden(bool hidden);
extern bool screen_startup_supporting_panes_hidden_active(void);
extern void screen_push_touch_pane_hidden(void);
extern void screen_pop_touch_pane_hidden(void);
extern bool screen_touch_pane_hidden_active(void);
extern void screen_set_startup_touch_pane_hidden(bool hidden);
extern bool screen_startup_touch_pane_hidden_active(void);
extern void screen_push_touch_pane_proto(void);
extern void screen_pop_touch_pane_proto(void);
extern bool screen_touch_pane_proto_active(void);
extern void sdl_refresh_supporting_panes_layout(void);
extern void sdl_refresh_supporting_panes_layout_deferred(void);
extern void sdl_push_saved_screen_left_panel_pane(void);
extern void sdl_pop_saved_screen_left_panel_pane(void);
#define SDL_POINTER_ATTACK_NONE 0
#define SDL_POINTER_ATTACK_MELEE 1
#define SDL_POINTER_ATTACK_RANGED_1 2
#define SDL_POINTER_ATTACK_RANGED_2 3
#define SDL_PANEL_CLICK_NONE 0
#define SDL_PANEL_CLICK_CHARACTER 1
#define SDL_PANEL_CLICK_SONG 2
#define SDL_PANEL_CLICK_SUPPLIES_LIGHTS 3
#define SDL_PANEL_CLICK_SKILL_DISTRIBUTION 4
#define SDL_PANEL_CLICK_INVENTORY 5
#define SDL_PANEL_CLICK_ABILITIES 6
#define SDL_PANEL_CLICK_SMITHING 7
#define SDL_PANEL_CLICK_EQUIPMENT 8
#define SDL_PANEL_CLICK_COMPACT 9
#define SDL_STATUS_CLICK_NONE 0
#define SDL_STATUS_CLICK_MAIN_MENU 1
#define SDL_STATUS_CLICK_SONG 2
#define SDL_STATUS_CLICK_MAP 3
#define SDL_STATUS_CLICK_VIEW 4
extern int sdl_pointer_attack_current_mode(void);
extern bool sdl_pointer_attack_panel_mode_highlighted(int mode);
extern bool sdl_pointer_attack_panel_quiver_highlighted(int mode);
extern bool sdl_pointer_attack_take_command(int* command, int* dir);
extern void sdl_pointer_attack_reset_to_melee(void);
extern void sdl_pointer_aim_begin(int range, bool allow_vertical);
extern void sdl_pointer_aim_end(void);
extern bool sdl_pointer_aim_take_direction(int* dir);
extern void sdl_pointer_aim_select_begin(int range, bool allow_vertical);
extern void sdl_pointer_aim_select_end(void);
extern void sdl_pointer_aim_select_set_manual(bool manual);
extern void sdl_pointer_aim_select_set_location(bool location);
extern void sdl_pointer_aim_select_update(int y, int x);
extern bool sdl_pointer_aim_select_take_event(int* kind, int* y, int* x);
extern void sdl_pointer_aim_select_set_choices(const int* ys, const int* xs,
    int count, cptr prompt);
extern bool sdl_mouse_path_take_step_command(int* command, int* dir);
extern bool sdl_mouse_path_is_following(void);
extern bool sdl_mouse_recall_process_pending(void);
extern bool sdl_log_pane_display_process_pending(void);
extern int sdl_log_pane_display_filter(int pane);
extern void sdl_mouse_path_cancel(void);
extern void sdl_player_exchange_begin_direction_prompt(void);
extern void sdl_player_exchange_cancel_direction_prompt(void);
extern void sdl_unified_look_set_active(bool active);
extern void sdl_unified_look_prompt_clear(void);
extern void sdl_unified_look_prompt_begin(int anchor_row);
extern void sdl_unified_look_prompt_add(int choice, cptr full, cptr medium,
    cptr compact, cptr tiny);
extern void sdl_unified_look_prompt_finish(void);
extern void sdl_unified_look_sidebar_clear(void);
extern void sdl_unified_look_sidebar_begin(bool compact, bool has_selection,
    int selected_choice);
extern void sdl_unified_look_sidebar_add_header(cptr text);
extern void sdl_unified_look_sidebar_add_entry(int choice, int entity_type,
    int y, int x, byte symbol_attr, byte text_attr, cptr symbol, cptr text);
extern void sdl_unified_look_sidebar_finish(void);
extern void sdl_unified_look_set_map_hover_enabled(bool enabled);
extern void sdl_object_tooltip_clear(void);
extern bool sdl_object_tooltip_show_grid(int map_y, int map_x, bool touch);
extern bool sdl_unified_look_take_map_hover(int* y, int* x);
extern bool sdl_unified_look_take_map_describe(int* y, int* x);
extern bool sdl_unified_look_take_map_target(int* y, int* x);
extern bool sdl_unified_look_take_map_pan(int* dy, int* dx);
extern bool sdl_unified_look_take_main_zoom(int* scale);
extern bool sdl_status_line_touch_zone_selected(int action, int col, int width);
extern bool sdl_character_panel_touch_zone_selected(int action, int row);
extern bool sdl_display_pixel_map(int* cy, int* cx);
extern void sdl_minimap_begin(void);
extern void sdl_minimap_end(void);
extern void sdl_minimap_focus(int y, int x);
extern bool sdl_minimap_adjust_zoom(int delta);
extern bool sdl_minimap_pan(int dx, int dy);
extern bool sdl_minimap_take_hint_click(int* out_index);
extern void c_put_str(byte attr, cptr str, int row, int col);
extern void put_str(cptr str, int row, int col);
extern void c_prt(byte attr, cptr str, int row, int col);
extern void prt(cptr str, int row, int col);
extern void text_out_to_file(byte attr, cptr str);
extern int count_wrapped_lines(cptr str, int wrap_width, int indent);
extern void text_out_to_screen(byte a, cptr str);
extern void text_out(cptr str);
extern void text_out_c(byte a, cptr str);
extern void clear_from(int row);
extern bool askfor_aux(char* buf, size_t len);
extern bool askfor_name(char* buf, size_t len);
extern bool term_get_string(cptr prompt, char* buf, size_t len);
extern bool get_string_panel(cptr prompt, char* buf, size_t len);
extern s16b get_quantity(cptr prompt, int max);
extern s16b get_quantity_action(cptr prompt, cptr action, int max);
extern s16b get_quantity_touch_category(cptr prompt, int max,
    int touch_category);
extern s16b get_quantity_touch_category_action(cptr prompt, cptr action,
    int max, int touch_category);
extern s16b get_quantity_touch_category_force_prompt(cptr prompt, int max,
    int touch_category);
extern s16b get_quantity_touch_category_force_prompt_action(cptr prompt,
    cptr action, int max, int touch_category);
extern bool get_check(cptr prompt);
extern bool get_check_near(int y, int x, cptr prompt);
extern bool get_check_oath_multiline(cptr prompt);
extern void ui_menu_click_clear(void);
extern void ui_menu_click_begin(void);
extern void ui_menu_click_set_hover_enabled(bool enabled);
extern void ui_menu_click_set_outside_cancel_enabled(bool enabled);
extern bool ui_menu_click_outside_cancel_enabled(void);
extern void ui_menu_click_set_touch_exit_button(bool enabled);
extern bool ui_menu_click_touch_exit_button_active(void);
extern int sdl_touch_menu_button_reserved_rows(void);
extern void ui_menu_click_add_touch_button(int choice, cptr label, byte attr);
extern int ui_menu_click_touch_button_count(void);
extern bool ui_menu_click_touch_button_get(int index, int* choice,
    cptr* label, byte* attr);
extern bool ui_menu_click_is_active(void);
extern void ui_menu_click_set_touch_category(int category);
extern int ui_menu_click_get_touch_category(void);
extern void ui_menu_click_add(int choice, int col, int row, int width);
extern void ui_menu_click_add_full_row(int choice, int row);
extern void ui_menu_click_add_span(int choice, int col, int row, int end_col);
extern void ui_menu_click_add_text_span(int choice, int col, int row,
    cptr text, int start_offset, int end_offset);
extern void ui_menu_click_add_text_token(int choice, int col, int row, cptr text,
    cptr token);
extern bool ui_menu_click_has_cell(int col, int row);
extern bool ui_menu_click_handle_hover_cell(int col, int row, bool* wake);
extern bool ui_menu_click_clear_hover(bool* wake);
extern bool ui_menu_click_handle_choice_action(int choice, int action,
    bool* wake);
extern bool ui_menu_click_handle_cell(int col, int row);
extern bool ui_menu_click_handle_cell_action(int col, int row, int action);
extern bool ui_menu_click_has_pending(void);
extern bool ui_menu_click_clear_pending_hover(void);
extern bool ui_menu_click_take_hover_redraw(void);
extern bool ui_menu_click_get_hover_choice(int* choice);
extern bool ui_menu_click_take(int* choice);
extern bool ui_menu_click_take_action(int* choice, int* action);
extern void ui_scroll_area_clear(void);
extern void ui_scroll_area_begin(int top_row, int bottom_row, int touch_category);
extern void ui_scroll_area_begin_cols(int left_col, int right_col, int top_row,
    int bottom_row, int touch_category);
extern bool ui_scroll_area_add_cols(int left_col, int right_col, int top_row,
    int bottom_row, int touch_category);
extern bool ui_scroll_area_has_cell(int col, int row);
extern int ui_scroll_area_selected_index(void);
extern bool ui_scroll_area_select_index(int index);
extern int ui_scroll_area_get_touch_category(void);
extern void ui_scroll_area_set_keys(int positive_y_key, int negative_y_key,
    int positive_x_key, int negative_x_key);
extern int ui_scroll_area_get_vertical_key(int direction);
extern int ui_scroll_area_get_horizontal_key(int direction);
extern void ui_scroll_area_set_tap_key(int key);
extern int ui_scroll_area_get_tap_key(void);
extern void ui_scroll_area_set_page_mode(bool enabled);
extern bool ui_scroll_area_is_page_mode(void);
extern void ui_scroll_area_set_offset_target(int* offset, int max_offset);
extern bool ui_scroll_area_has_offset_target(void);
extern bool ui_scroll_area_offset_scroll(int delta);
extern bool ui_scroll_area_take_touch_scrolled(void);
extern void ui_key_wait_dismiss_begin(int key);
extern void ui_key_wait_dismiss_clear(void);
extern bool ui_key_wait_dismiss_is_active(void);
extern int ui_key_wait_dismiss_get_key(void);
extern void ui_reset_transient_state_for_new_session(void);
extern bool get_com(cptr prompt, char* command);
extern bool preconfirm_enter_morgoth_hall(void);
extern void any_key_prompt_text(char* buf, size_t len, cptr action);
extern void pause_line(int row);
extern void request_command(void);
extern int int_exp(int base, int power);
extern int damroll(int num, int sides);
extern bool is_a_vowel(int ch);
extern int color_char_to_attr(char c);
extern int color_text_to_attr(cptr name);

#ifdef SUPPORT_GAMMA
extern void build_gamma_table(int gamma);
extern byte gamma_table[256];
#endif /* SUPPORT_GAMMA */

extern byte get_angle_to_grid[41][41];
extern int get_angle_to_target(int y0, int x0, int y1, int x1, int dir);
extern void get_grid_using_angle(int angle, int y0, int x0, int* ty, int* tx);
extern void editing_buffer_init(
    editing_buffer* eb_ptr, const char* buf, size_t max_size);
extern void editing_buffer_destroy(editing_buffer* eb_ptr);
extern int editing_buffer_put_chr(editing_buffer* eb_ptr, char ch);
extern int editing_buffer_set_position(editing_buffer* eb_ptr, size_t new_pos);
extern void editing_buffer_display(
    editing_buffer* eb_ptr, int x, int y, byte col);
extern int editing_buffer_delete(editing_buffer* eb_ptr);
extern void editing_buffer_clear(editing_buffer* eb_ptr);
extern void editing_buffer_get_all(
    editing_buffer* eb_ptr, char buf[], size_t max_size);
extern int editing_buffer_put_str(
    editing_buffer* eb_ptr, const char* str, int n);
extern cptr get_ext_color_name(byte ext_color);

/* Player/status/upkeep modules */
extern byte total_mdd(const object_type* o_ptr);
extern byte strength_modified_ds(const object_type* o_ptr, int str_adjustment);
extern byte total_mds(const object_type* o_ptr, int str_adjustment);
extern byte total_mds_for_weapon_mode(
    const object_type* o_ptr, int str_adjustment, int mode);
extern bool two_handed_melee(void);
extern bool armour_is_light(const object_type* o_ptr);
extern bool wearing_only_light_armour(void);
extern int hand_and_a_half_bonus(const object_type* o_ptr);
extern int axe_bonus(const object_type* o_ptr);
extern int polearm_bonus(const object_type* o_ptr);
extern byte total_ads(const object_type* j_ptr);
extern byte total_ads_for_weapon_mode(const object_type* j_ptr, int mode);
extern int player_active_weapon_mode(void);
extern bool player_active_weapon_is_melee(void);
extern bool player_active_weapon_is_ranged(void);
extern bool player_active_weapon_mode_is_ranged(int mode);
extern int player_active_weapon_mode_for_quiver(int quiver);
extern int player_last_ranged_weapon_mode(void);
extern int player_selected_ranged_quiver_number(void);
extern int player_opposite_active_weapon_mode(void);
extern int player_active_weapon_quiver_slot(void);
extern int player_active_weapon_quiver_number(void);
extern bool player_set_active_weapon_mode(
    int mode, bool confirm, bool take_turn);
extern void do_cmd_toggle_active_weapon(void);
extern void player_queue_active_weapon_mode(int mode);
extern void player_queue_ranged_quiver_mode(int mode);
extern void do_cmd_pending_active_weapon_mode(void);
extern bool player_weapon_slot_combat_bonuses_active(
    int slot, const object_type* o_ptr);
extern bool player_weapon_slot_combat_bonuses_active_for_mode(
    int mode, int slot, const object_type* o_ptr);
extern bool player_shield_counts_for_active_weapon(const object_type* o_ptr);
extern bool player_can_quick_throw_from_quiver(int slot);
extern int player_quick_throw_quiver_slot(void);
extern bool player_power_throw_weapon_eligible(const object_type* o_ptr);
extern bool player_power_throw_ready(void);
extern int player_power_throw_target_m_idx(void);
extern bool player_can_power_throw_from_quiver(int slot);
extern int player_power_throw_quiver_slot(void);
extern bool player_can_throw_potions(void);
extern bool player_has_throwable_potion(void);
extern bool player_quick_throw_available(void);
extern void cnv_stat(int val, char* out_val);
extern int health_level(int current, int max);
extern bool monster_health_bar_allowed(const monster_type* m_ptr);
extern int monster_health_bar_text(
    const monster_type* m_ptr, char* buf, size_t buflen, int max_symbols);
extern int monster_health_bar_put(
    const monster_type* m_ptr, int max_symbols);
extern bool get_alertness_text(
    monster_type* m_ptr, int text_size, char* text, int* color);
extern byte health_attr(int current, int max);
extern void notice_stuff(void);
extern void update_stuff(void);
extern void redraw_stuff(void);
extern void window_stuff(void);
extern void handle_stuff(void);
extern void prt_frame_basic(void);
extern int weight_limit(void);
extern bool sprinting(void);
extern void calc_voice(void);
extern bool weapon_glows(const object_type* o_ptr);
extern byte object_display_color(const object_type* o_ptr, byte base_color);
extern void calc_torch(void);
extern int song_effective_skill(int song);
extern int ability_current_skill_bonus(int skilltype, int abilitynum);
extern int ability_potential_skill_bonus(int skilltype, int abilitynum);
extern int ability_potential_skill_bonus_with_partner(
    int skilltype, int abilitynum);
extern int ability_bonus(int skilltype, int abilitynum);
extern int affinity_level(int skilltype);
extern int minstrel_level(void);

/* Effects/world/targeting/quest modules */
extern bool saving_throw(monster_type* m_ptr, int resistance);
extern bool turin_resist_bad_effect(void);
extern bool allow_player_blind(monster_type* m_ptr);
extern bool set_blind(int v);
extern bool allow_player_confusion(monster_type* m_ptr);
extern bool set_confused(int v);
extern bool set_poisoned(int v);
extern bool allow_player_fear(monster_type* m_ptr);
extern bool set_afraid(int v);
extern bool allow_player_entrancement(monster_type* m_ptr);
extern bool set_entranced(int v);
extern bool allow_player_image(monster_type* m_ptr);
extern bool set_image(int v);
extern bool set_fast(int v);
extern bool allow_player_slow(monster_type* m_ptr);
extern bool set_slow(int v);
extern bool set_shield(int v);
extern bool set_blessed(int v);
extern bool set_hero(int v);
extern bool set_rage(int v);
extern bool set_tmp_str(int v);
extern bool set_tmp_dex(int v);
extern bool set_tmp_con(int v);
extern bool set_tmp_gra(int v);
extern bool set_protevil(int v);
extern bool set_tmp_per(int v);
extern bool set_tim_invis(int v);
extern bool set_darkened(int v);
extern bool set_oppose_fire(int v);
extern bool set_oppose_cold(int v);
extern bool set_oppose_pois(int v);
extern bool allow_player_stun(monster_type* m_ptr);
extern bool set_stun(int v);
extern bool set_cut(int v);
extern bool set_food(int v);
extern void falling_damage(bool stun);
extern void check_experience(void);
extern void gain_skills_set_initial_skill(int skill);
extern s32b adjusted_mon_exp(const monster_race* r_ptr, bool kill);
extern void gain_exp(s32b amount);
extern void lose_exp(s32b amount);
extern bool random_stair_location(int* sy, int* sx);
extern void break_truce(bool obvious);
extern bool similar_monsters(int m1y, int m1x, int m2y, int m2x);
extern void scare_onlooking_friends(const monster_type* m_ptr, int amount);
extern void create_chosen_artefact(byte name1, int y, int x, bool identify);
extern int drop_loot(monster_type* m_ptr);
extern void apply_quest_rewards(int quest_idx);
extern bool check_quest_eligibility(int quest_idx, int depth);
typedef enum hint_quest_page
{
    HINT_QUEST_PAGE_EXIT = 0,
    HINT_QUEST_PAGE_HINTS,
    HINT_QUEST_PAGE_QUESTS,
    HINT_QUEST_PAGE_THRALLS
} hint_quest_page;
enum {
    HINT_QUEST_CLICK_HINTS_TAB = -20101,
    HINT_QUEST_CLICK_QUESTS_TAB = -20102,
    HINT_QUEST_CLICK_THRALLS_TAB = -20103,
    HINT_QUEST_CLICK_RETURN = -20104,
    HINT_QUEST_CLICK_CONTINUE = -20105
};
extern hint_quest_page do_cmd_quest_status_page(void);
extern cptr* extract_quest_init_texts(int quest_idx, int* count);
extern cptr* extract_quest_completion_texts(int quest_idx, int* count);
extern void free_quest_texts(cptr* texts, int count);
extern void quest_typewriter_menu(cptr title, cptr texts[], int total_texts, byte title_color, byte text_color);
extern void quest_typewriter_menu_pages(cptr title, cptr texts[],
    int total_texts, byte title_color, byte text_color,
    int target_page_count);
extern void tulkas_quest_interaction(void);
extern void check_tulkas_quest_interaction(void);
extern void check_tulkas_quest_completion(int r_idx);
extern void validate_tulkas_quest_on_load(void);
extern void remove_quest_giver(int quest_giver_r_idx);
extern bool is_quest_giver_present(int quest_giver_r_idx);
extern bool spawn_quest_giver_near_player(int quest_giver_r_idx);
extern void aule_quest_interaction(void);
extern void check_aule_quest_interaction(void);
extern void varda_quest_interaction(void);
extern void check_varda_quest_interaction(void);
extern void check_varda_quest_completion(int r_idx);
extern bool varda_quest_bastion_level_active(void);
extern void varda_quest_notice_bastion_level_entry(void);
extern bool varda_quest_confirm_leave_bastion(void);
extern void varda_quest_fail_if_bastion_missed(void);
extern void mandos_quest_interaction(void);
extern void check_mandos_quest_interaction(void);
extern void check_mandos_quest_completion(int r_idx);
extern void niena_quest_interaction(void);
extern void check_niena_quest_interaction(void);
extern void check_niena_quest_completion(void);
extern void check_orome_quest_completion(void);
extern void orome_quest_interaction(void);
extern void check_orome_quest_interaction(void);
extern void grant_unique_bane_ability(void);
extern void anger_morgoth(int level);
extern void maybe_update_morgoth_state_from_hp(monster_type* m_ptr);
extern bool morgoth_enter_final_stage(int m_idx);
extern void monster_death(int m_idx);
extern bool mon_take_hit(int m_idx, int dam, cptr note, int who);
extern bool modify_panel(int wy, int wx);
extern bool adjust_panel(int y, int x);
extern bool change_panel(int dir);
extern void verify_panel(void);
extern void ang_sort_aux(void* u, void* v, int p, int q);
extern void ang_sort(void* u, void* v, int n);
extern int motion_dir(int y1, int x1, int y2, int x2);
extern int target_dir(char ch);
extern bool target_able(int m_idx);
extern bool target_okay(int range);
extern bool target_sighted(void);
extern void target_set_monster(int m_idx);
extern void target_set_location(int y, int x);
extern void get_sorted_target_list(int mode, int range);
extern bool target_set_interactive(int mode, int range);
extern bool target_select_location(cptr action, int* y, int* x);
extern int dir_from_delta(int deltay, int deltax);
extern int rough_direction(int y1, int x1, int y2, int x2);
extern void player_set_visual_facing_dir(int dir);
extern void player_set_visual_facing_target(int y, int x);
extern void player_set_visual_facing_dir_immediate(int dir);
extern void player_set_visual_facing_target_immediate(int y, int x);
extern void monster_set_visual_facing_dir(monster_type* m_ptr, int dir);
extern void monster_set_visual_facing_target(monster_type* m_ptr, int y, int x);
extern void monster_set_visual_facing_dir_immediate(monster_type* m_ptr, int dir);
extern void monster_set_visual_facing_target_immediate(
    monster_type* m_ptr, int y, int x);
extern bool get_aim_dir(int* dp, int range);
extern bool get_aim_dir_vertical(int* dp, int range);
extern bool get_rep_dir(int* dp);
extern bool get_grid_choice_dir(cptr prompt, const int ys[], const int xs[],
    const int dirs[], int count, int* dp);
extern bool confuse_dir(int* dp);
extern const char tutorial_leave_text[][100];
extern const char tutorial_win_text[][100];
extern const char tutorial_early_death_text[][100];
extern const char tutorial_late_death_text[][100];
extern const char entry_poetry[][100];
extern const char throne_poetry[][100];
extern const char ultimate_bug_text[][100];
void pause_with_text(const char desc[][100], int row, int col,
                     const char extra[][100], byte extra_attr);

/*
 * Hack -- conditional (or "bizarre") externs
 */

#ifdef SET_UID
#ifndef HAVE_USLEEP
/* support/misc.c */
extern int usleep(unsigned long usecs);
#endif /* HAVE_USLEEP */
extern void user_name(char* buf, size_t len, int id);
#endif /* SET_UID */

#ifdef ALLOW_REPEAT
extern void repeat_push(int what);
extern bool repeat_pull(int* what);
extern void repeat_clear(void);
extern void repeat_check(void);
#endif /* ALLOW_REPEAT */

#ifdef ALLOW_DEBUG
/* wizard2.c */
void display_light_map(void);
void display_scent_map(void);
void display_noise_map(void);
extern void do_cmd_debug(void);
extern void do_cmd_wiz_unhide(int d);
#endif /* ALLOW_DEBUG */

#ifdef ALLOW_SPOILERS

/* wizard1.c */
extern void do_cmd_spoilers(void);

#endif /* ALLOW_SPOILERS */
extern bool make_fake_artefact(object_type* o_ptr, byte name1);

// Metarun.c

extern errr load_metaruns(bool create_if_missing);
extern bool metarun_created; 
extern u32b curse_flag_mask(void);
extern int curse_flag_count_rhf(u32b rhf_flag);
extern int curse_flag_count_cur(u32b cur_flag);
extern int curse_flag_delta_cur(u32b cur_flag);
extern int  any_curse_flag_active(u32b flag); /* CUR-only */

// init/init-flags.c
extern void dbg_show_active_flags(void);

// Enhanced menu system globals
#define ENHANCED_ACTION_NONE 0
#define ENHANCED_ACTION_SWITCH 1
#define ENHANCED_ACTION_EXAMINE 2
#define ENHANCED_ACTION_USE 3
#define ENHANCED_ACTION_DROP 4
#define ENHANCED_ACTION_SUPPLIES 5
/* Must not collide with real floor item indices such as -1. */
#define ENHANCED_MENU_NO_SELECTION (-2147483647 - 1)

extern int enhanced_menu_action;
extern int enhanced_inventory_selected_item;
extern int enhanced_equip_action;
extern int enhanced_equipment_selected_item;
extern char current_menu_command;
extern int current_menu_state;

/* SDL pane configuration functions (main-sdl.c) */
extern void get_sdl_config_info(char* buf, size_t size);
extern bool save_pane_config_to_json(void);
extern cptr get_sdl_config_path(void);
extern int get_sdl_main_view_scale(void);
extern void set_sdl_main_view_scale(int value);
extern int get_sdl_effective_main_view_scale(void);
extern bool set_sdl_main_view_zoom_scale(int value);
extern int get_sdl_terminal_menu_scale_offset(void);
extern void set_sdl_terminal_menu_scale_offset(int value);
extern int get_sdl_mobile_starting_zoom_offset(void);
extern void set_sdl_mobile_starting_zoom_offset(int value);
extern bool get_sdl_mobile_portrait_mode(void);
extern void set_sdl_mobile_portrait_mode(bool value);
extern bool sdl_prepare_first_gameplay_main_view_zoom(void);
extern int get_sdl_min_main_view_scale(void);
extern int get_sdl_platform_max_main_view_scale(void);
extern int get_sdl_terminal_menu_scale(void);
extern void sdl_push_terminal_menu_scale(void);
extern void sdl_pop_terminal_menu_scale(void);
extern int sdl_description_overlay_max_cols(void);
extern int sdl_description_overlay_capture_cols(int terminal_cols,
    bool interactive);
extern int sdl_description_overlay_visible_cols(void);
extern int sdl_description_overlay_text_px(void);
extern int sdl_description_overlay_story_text_width(cptr text, int len, int slot);
extern void sdl_push_description_overlay_main_anchor(void);
extern void sdl_pop_description_overlay_main_anchor(void);
extern void sdl_push_description_overlay_full_main_anchor(void);
extern void sdl_pop_description_overlay_full_main_anchor(void);
extern bool sdl_description_overlay_present(const byte* attrs,
    const char* chars, const byte* tattrs, const char* tchars,
    const byte* story, const byte* health, int width, int height,
    int target_cols, int scroll, bool interactive, int* out_visible_rows,
    int* out_max_scroll);
extern void sdl_description_overlay_set_footer(cptr text, bool always);
extern void sdl_description_overlay_clear_footer_actions(void);
extern void sdl_description_overlay_add_footer_action(int key, cptr token);
extern void sdl_description_overlay_set_avoid_term_rect(int col, int row,
    int wid, int hgt);
extern void sdl_description_overlay_clear_avoid(void);
extern bool sdl_description_overlay_scroll_by(int rows);
extern bool sdl_description_overlay_scroll_page(int direction);
extern void sdl_description_overlay_clear(void);
extern void sdl_song_menu_begin(cptr title);
extern void sdl_song_menu_add_entry(int choice, cptr letter, cptr text,
    byte attr);
extern void sdl_song_menu_add_text(cptr text, byte attr);
extern void sdl_song_menu_set_highlight(int choice);
extern void sdl_song_menu_finish(void);
extern void sdl_song_menu_clear(void);
extern void sdl_question_menu_begin(cptr title);
extern void sdl_question_menu_set_anchor_grid(int y, int x);
extern void sdl_question_menu_set_desc(cptr text);
extern void sdl_question_menu_add_entry(int choice, cptr letter, cptr text,
    byte attr);
extern void sdl_question_menu_add_button(int choice, cptr text, byte attr);
extern void sdl_question_menu_add_text(cptr text, byte attr);
extern void sdl_question_menu_set_highlight(int choice);
extern void sdl_question_menu_finish(void);
extern void sdl_question_menu_clear(void);
extern void sdl_question_menu_clear_nonblocking(void);
extern void sdl_question_menu_set_scroll_offset_target(int* offset,
    bool follow_highlight);
extern bool sdl_question_menu_take_touch_scrolled(void);
extern void sdl_question_menu_set_blocking_input(bool blocking);
extern bool sdl_question_menu_blocks_input(void);
extern void sdl_question_menu_set_nonblocking(bool nonblocking);
extern void sdl_question_menu_set_timeout_ms(int ms);
extern void sdl_hint_quest_menu_begin(hint_quest_page page, cptr title,
    cptr section, bool show_tabs, bool center_body, int selected_choice);
extern void sdl_hint_quest_menu_add_block(cptr text, byte attr, int indent,
    int choice);
extern void sdl_hint_quest_menu_add_button(int choice, cptr label, byte attr);
extern void sdl_hint_quest_menu_finish(void);
extern void sdl_hint_quest_menu_prepare_page_turn(
    hint_quest_page next_page);
extern void sdl_hint_quest_menu_prepare_leaf_turn(
    hint_quest_page next_page, int direction);
extern void sdl_hint_quest_menu_hide(void);
extern bool sdl_hint_quest_menu_active(void);
extern void sdl_suspend_main_view_zoom_for_saved_screen(void);
extern bool sdl_resume_main_view_zoom_for_saved_screen(void);
extern void sdl_reset_main_view_zoom(void);
extern void sdl_set_present_suppressed(bool suppressed);
extern int get_sdl_min_terminal_mode(void);
extern void set_sdl_min_terminal_mode(int value);
extern int get_sdl_aux_view_font_size(void);
extern int get_sdl_effective_aux_view_font_size(void);
extern void set_sdl_aux_view_font_size(int value);
extern int get_sdl_dice_roll_lock_ms(void);
extern void set_sdl_dice_roll_lock_ms(int value);
extern int get_sdl_dice_roll_overlay_ms(void);
extern void set_sdl_dice_roll_overlay_ms(int value);
extern int get_sdl_popup_notification_ms(void);
extern void set_sdl_popup_notification_ms(int value);
extern bool get_sdl_show_main_menu_button(void);
extern void set_sdl_show_main_menu_button(bool value);
extern int get_sdl_margin(void);
extern void set_sdl_margin(int value);
extern int get_sdl_camera_center_clearance(void);
extern void set_sdl_camera_center_clearance(int value);
extern bool get_sdl_fullscreen(void);
extern void set_sdl_fullscreen(bool value);
extern bool get_sdl_tiles(void);
extern void set_sdl_tiles(bool value);
extern bool get_sdl_use_unsafe_area(void);
extern void set_sdl_use_unsafe_area(bool value);
extern int get_pane_config_count(void);
extern bool get_sdl_enable_right_panes(void);
extern void set_sdl_enable_right_panes(bool value);
extern bool get_sdl_enable_bottom_panes(void);
extern void set_sdl_enable_bottom_panes(bool value);
extern bool get_sdl_show_pane_borders(void);
extern void set_sdl_show_pane_borders(bool value);
extern bool get_sdl_left_overlays_touch_screen_edge(void);
extern void set_sdl_left_overlays_touch_screen_edge(bool value);
extern bool get_sdl_show_overlay_log_border(void);
extern void set_sdl_show_overlay_log_border(bool value);
extern bool g_hide_left_panel;
#ifdef USE_SDL
extern bool g_sdl_left_panel_pane_source_active;
extern void sdl_side_map_pane_forget_level(void);
extern void sdl_side_map_pane_invalidate_cell(int y, int x);
#endif
extern bool g_suppress_hidden_left_panel_overlay;
extern byte g_hidden_left_panel_overlay_start_row;
extern byte g_hidden_left_panel_overlay_rows;
extern byte g_hidden_left_panel_overlay_start_cols[16];
extern byte g_hidden_left_panel_overlay_widths[16];
extern byte g_hidden_left_panel_overlay_attack_modes[16];
extern bool g_hidden_left_panel_overlay_attack_quivers[16];
extern byte g_hidden_left_panel_overlay_attack_start_cols[16];
extern byte g_hidden_left_panel_overlay_attack_end_cols[16];
extern byte g_hidden_left_panel_overlay_click_actions[16];
extern byte g_hidden_left_panel_overlay_click_start_cols[16];
extern byte g_hidden_left_panel_overlay_click_end_cols[16];
extern byte g_left_panel_quiver_attack_modes[2];
extern byte g_left_panel_quiver_attack_start_cols[2];
extern byte g_left_panel_quiver_attack_end_cols[2];
extern bool get_sdl_hide_left_panel(void);
extern bool sdl_left_panel_pane_renders_character_panel(void);
extern bool get_sdl_left_panel_expanded_on_launch(void);
extern void set_sdl_left_panel_expanded_on_launch(bool value);
extern int get_sdl_left_panel_compact_mode(void);
extern void set_sdl_left_panel_compact_mode(int mode);
extern void redraw_hidden_left_panel_overlay(void);
extern int get_sdl_pane_type(int index);
extern int get_sdl_pane_where(int index);
extern void set_sdl_pane_where(int index, int where);
extern int get_sdl_pane_stack_order(int index);
extern int get_sdl_pane_stack_count(int where);
extern void set_sdl_pane_where_order(int index, int where, int order);
extern bool get_sdl_pane_enabled(int index);
extern bool get_sdl_pane_default_enabled(int index);
extern int get_sdl_pane_default_where(int index);
extern int get_sdl_pane_default_rows(int index);
extern int get_sdl_pane_default_cols(int index);
extern int get_sdl_pane_rows(int index);
extern int get_sdl_pane_cols(int index);
extern int get_sdl_pane_font_size(int index);
extern int get_sdl_pane_effective_font_size(int index);
extern int get_sdl_pane_current_rows(int index);
extern int get_sdl_pane_current_cols(int index);
extern void set_sdl_pane_rows(int index, int rows);
extern void set_sdl_pane_cols(int index, int cols);
extern void set_sdl_pane_font_size(int index, int font_size);
extern void set_sdl_pane_enabled(int index, bool enabled);
extern int  get_sdl_intro_style(void);
extern void set_sdl_intro_style(int style);
extern void sdl_config_load_app_options(const char* filename);
extern void sdl_config_reset_app_options_to_defaults(void);
extern void sdl_reset_interface_settings_to_defaults(void);
extern void sdl_reset_interface_settings_to_defaults_for_migration(void);
extern bool sdl_config_should_force_intro_flame(void);
extern void sdl_config_mark_intro_seen(void);
extern bool sdl_config_keyboard_preset_prompt_seen(void);
extern void sdl_config_mark_keyboard_preset_prompt_seen(void);
extern bool option_is_app_persistent(int opt);
extern int get_sdl_max_scale(void);
extern int get_sdl_max_main_view_zoom_scale(void);
extern void sdl_apply_config(void);
extern void sdl_apply_config_no_redraw(void);
extern void sdl_request_redraw(void);
extern void sdl_main_menu_overlay_begin(void);
extern bool steamdeck_controls_active(void);
extern bool sdl_menu_letters_enabled(void);
extern bool portable_controls_active(void);
extern bool get_sdl_gamepad_enabled(void);
extern void set_sdl_gamepad_enabled(bool value);
extern bool get_sdl_gamepad_auto_mode(void);
extern void set_sdl_gamepad_auto_mode(bool value);
extern bool get_sdl_steamdeck_mode(void);
extern void set_sdl_steamdeck_mode(bool value);
extern bool get_sdl_steamdeck_inv_equip_same_button_cycle(void);
extern void set_sdl_steamdeck_inv_equip_same_button_cycle(bool value);
extern bool get_sdl_gamepad_use_dpad(void);
extern void set_sdl_gamepad_use_dpad(bool value);
extern bool get_sdl_gamepad_use_left_stick(void);
extern void set_sdl_gamepad_use_left_stick(bool value);
extern bool get_sdl_gamepad_default_enabled(void);
extern bool get_sdl_gamepad_default_auto_mode(void);
extern bool get_sdl_steamdeck_default_mode(void);
extern bool get_sdl_steamdeck_default_inv_equip_same_button_cycle(void);
extern bool get_sdl_gamepad_default_use_dpad(void);
extern bool get_sdl_gamepad_default_use_left_stick(void);
extern int get_sdl_gamepad_button_binding(int button);
extern void set_sdl_gamepad_button_binding(int button, int binding);
extern int get_sdl_gamepad_trigger_binding(int index);
extern void set_sdl_gamepad_trigger_binding(int index, int binding);
extern int get_sdl_gamepad_left_stick_binding(int dir);
extern void set_sdl_gamepad_left_stick_binding(int dir, int binding);
extern int get_sdl_gamepad_right_stick_binding(int dir);
extern void set_sdl_gamepad_right_stick_binding(int dir, int binding);
extern int get_sdl_gamepad_combo_binding(int modifier, int type, int id);
extern void set_sdl_gamepad_combo_binding(int modifier, int type, int id, int binding);
extern int get_sdl_gamepad_shoulder_combo_binding(void);
extern void set_sdl_gamepad_shoulder_combo_binding(int binding);
extern int get_sdl_gamepad_default_button_binding(int button);
extern int get_sdl_gamepad_default_trigger_binding(int index);
extern int get_sdl_gamepad_default_left_stick_binding(int dir);
extern int get_sdl_gamepad_default_right_stick_binding(int dir);
extern int get_sdl_gamepad_default_combo_binding(int modifier, int type, int id);
extern int get_sdl_gamepad_default_shoulder_combo_binding(void);
extern void sdl_gamepad_reset_bindings_to_default(void);
extern void sdl_gamepad_action_binding_label(int binding, char* buf, size_t buflen);
extern void sdl_gamepad_action_binding_short_label(int binding, char* buf, size_t buflen);
extern int get_sdl_mouse_movement_mode(void);
extern void set_sdl_mouse_movement_mode(int mode);
extern int get_sdl_mouse_movement_default_mode(void);
extern bool get_sdl_mouse_enabled(void);
extern void set_sdl_mouse_enabled(bool enabled);
extern bool get_sdl_mouse_default_enabled(void);
extern bool get_sdl_mouse_tile_pointer(void);
extern void set_sdl_mouse_tile_pointer(bool enabled);
extern bool get_sdl_mouse_default_tile_pointer(void);
extern void sdl_screen_back_gesture_begin(void);
extern void sdl_screen_back_gesture_end(void);
extern bool sdl_hover_tooltip_show_text(int col, int row, int cols, cptr text,
    bool touch);
extern void sdl_hover_tooltip_clear(void);
extern bool get_sdl_touch_pane_enabled(void);
extern void set_sdl_touch_pane_enabled(bool value);
extern bool get_sdl_touch_pane_default_open(void);
extern void set_sdl_touch_pane_default_open(bool value);
extern bool get_sdl_touch_pane_default_open_default(void);
extern bool get_sdl_touch_pane_key_labels_visible(void);
extern void set_sdl_touch_pane_key_labels_visible(bool value);
extern bool get_sdl_touch_pane_key_labels_default_visible(void);
extern bool get_sdl_touch_pane_inventory_equipment_cycle(void);
extern void set_sdl_touch_pane_inventory_equipment_cycle(bool value);
extern bool get_sdl_touch_pane_inventory_equipment_default_cycle(void);
extern int get_sdl_touch_pane_placement(void);
extern void set_sdl_touch_pane_placement(int placement);
extern int get_sdl_touch_pane_binding(int index);
extern void set_sdl_touch_pane_binding(int index, int binding);
extern int get_sdl_touch_pane_default_binding(int index);
extern int get_sdl_touch_pane_binding_for_panel(int panel, int index);
extern void set_sdl_touch_pane_binding_for_panel(int panel, int index, int binding);
extern int get_sdl_touch_pane_default_binding_for_panel(int panel, int index);
extern void sdl_touch_pane_reset_bindings_to_default(void);
extern void sdl_touch_pane_begin_yes_no_prompt(cptr prompt);
extern void sdl_touch_pane_begin_yes_no_prompt_lower(cptr prompt);
extern void sdl_touch_pane_begin_yes_no_prompt_near(cptr prompt, int map_y,
    int map_x);
extern void sdl_touch_pane_end_yes_no_prompt(void);
extern cptr get_sdl_touch_pane_slot_name(int index);
extern void get_sdl_touch_pane_button_label(int index, char* buf, size_t buflen);
extern void set_sdl_touch_pane_button_label(int index, cptr label);
extern void clear_sdl_touch_pane_button_label(int index);
extern void get_sdl_touch_pane_button_label_for_panel(int panel, int index, char* buf, size_t buflen);
extern void set_sdl_touch_pane_button_label_for_panel(int panel, int index, cptr label);
extern void clear_sdl_touch_pane_button_label_for_panel(int panel, int index);
extern void get_sdl_touch_pane_panel_name(int panel, char* buf, size_t buflen);
extern void set_sdl_touch_pane_panel_name(int panel, cptr name);
extern bool get_sdl_touch_menu_commands_enabled(int category);
extern void set_sdl_touch_menu_commands_enabled(int category, bool value);
extern bool get_sdl_touch_menu_commands_default_enabled(int category);
extern bool sdl_touch_only_device_active(void);
extern int get_sdl_touch_profile(void);
extern void set_sdl_touch_profile(int profile);
extern int get_sdl_touch_profile_default(void);
extern void sdl_touch_apply_profile(int profile);
extern void sdl_touch_request_tutorial_from_settings(void);
extern bool sdl_touch_settings_tutorial_requested(void);
extern void sdl_touch_show_requested_tutorial(void);
extern void sdl_touch_show_tutorial(void);
extern void sdl_touch_maybe_show_first_game_tutorial(void);
extern bool sdl_touch_tutorial_device_available(void);
extern void sdl_mouse_request_tutorial_from_settings(void);
extern bool sdl_mouse_settings_tutorial_requested(void);
extern void sdl_mouse_show_requested_tutorial(void);
extern void sdl_mouse_show_tutorial(void);
extern void sdl_mouse_maybe_show_first_game_tutorial(void);
extern void keyboard_preset_maybe_show_first_game_selection(void);
extern bool keyboard_preset_choose_and_apply(void);
extern void sdl_character_wheel_request_tutorial_from_settings(void);
extern bool sdl_character_wheel_settings_tutorial_requested(void);
extern void sdl_zones_request_tutorial_from_settings(void);
extern bool sdl_zones_settings_tutorial_requested(void);
extern void sdl_zones_show_tutorial(void);
extern void sdl_zones_show_requested_tutorial(void);
extern int get_sdl_touch_movement_mode(void);
extern void set_sdl_touch_movement_mode(int mode);
extern int get_sdl_touch_movement_default_mode(void);
extern bool get_sdl_touch_round_movement_enabled(void);
extern void set_sdl_touch_round_movement_enabled(bool value);
extern bool get_sdl_touch_round_movement_default_enabled(void);
extern int get_sdl_touch_zone_overlay_mode(void);
extern void set_sdl_touch_zone_overlay_mode(int mode);
extern int get_sdl_touch_zone_overlay_default_mode(void);
extern int get_sdl_touch_zone_center_binding(int index);
extern void set_sdl_touch_zone_center_binding(int index, int binding);
extern int get_sdl_touch_zone_center_default_binding(int index);
extern int get_sdl_touch_corner_up_down_side(void);
extern void set_sdl_touch_corner_up_down_side(int side);
extern int get_sdl_touch_corner_up_down_default_side(void);
extern int get_sdl_touch_corner_action_binding(int index);
extern void set_sdl_touch_corner_action_binding(int index, int binding);
extern int get_sdl_touch_corner_action_default_binding(int index);
extern bool get_sdl_touch_top_panel_arrows_visible(void);
extern void set_sdl_touch_top_panel_arrows_visible(bool value);
extern bool get_sdl_touch_top_panel_arrows_default_visible(void);
extern bool get_sdl_touch_top_panel_default_open(void);
extern void set_sdl_touch_top_panel_default_open(bool value);
extern bool get_sdl_touch_top_panel_default_open_default(void);
extern float get_sdl_touch_top_panel_size(void);
extern void set_sdl_touch_top_panel_size(float size);
extern float get_sdl_touch_top_panel_default_size(void);
extern int get_sdl_touch_top_panel_columns(void);
extern void set_sdl_touch_top_panel_columns(int columns);
extern int get_sdl_touch_top_panel_default_columns(void);
extern int get_sdl_touch_top_panel_cell_count(void);
extern void set_sdl_touch_top_panel_cell_count(int count);
extern int get_sdl_touch_top_panel_default_cell_count(void);
extern int get_sdl_touch_top_panel_rows(void);
extern void set_sdl_touch_top_panel_rows(int rows);
extern int get_sdl_touch_top_panel_default_rows(void);
extern int get_sdl_touch_top_panel_binding(int index, bool long_press);
extern void set_sdl_touch_top_panel_binding(int index, bool long_press, int binding);
extern int get_sdl_touch_top_panel_default_binding(int index, bool long_press);
extern bool get_sdl_touch_thumb_enabled(void);
extern void set_sdl_touch_thumb_enabled(bool value);
extern bool get_sdl_touch_thumb_default_enabled(void);
extern int get_sdl_touch_thumb_binding(int index, bool long_press);
extern void set_sdl_touch_thumb_binding(int index, bool long_press, int binding);
extern int get_sdl_touch_thumb_default_binding(int index, bool long_press);
extern bool get_sdl_touch_swipe_enabled(void);
extern void set_sdl_touch_swipe_enabled(bool value);
extern int get_sdl_touch_swipe_binding(int dir);
extern void set_sdl_touch_swipe_binding(int dir, int binding);
extern bool get_sdl_touch_swipe_default_enabled(void);
extern int get_sdl_touch_swipe_default_binding(int dir);
/* Controller UI menu helpers - get key bindings for menu actions */
extern int steamdeck_back_key(void);      /* B button (EAST) - for back/quit */
extern int steamdeck_confirm_key(void);   /* A button (SOUTH) - for confirm/ok */
extern int steamdeck_prev_page_key(void); /* L1 button - for previous page/tab */
extern int steamdeck_next_page_key(void); /* R1 button - for next page/tab */
extern int steamdeck_menu_key(int key, int prev_page_key, int next_page_key);
extern int steamdeck_info_key(void);      /* RS Right - for info/recall */
extern int steamdeck_alt_action_key(void);/* X button (WEST) - for alternate action */
extern int steamdeck_secondary_key(void); /* Y button (NORTH) - for secondary action */
#define GAMEPAD_CAPTURE_BUTTON 0
#define GAMEPAD_CAPTURE_TRIGGER 1
#define GAMEPAD_CAPTURE_LEFT_STICK 2
#define GAMEPAD_CAPTURE_RIGHT_STICK 3
#define GAMEPAD_CAPTURE_SHOULDER_COMBO 4
extern bool sdl_gamepad_capture_begin(bool allow_modifier_combo);
extern void sdl_gamepad_capture_cancel(void);
extern bool sdl_gamepad_capture_poll(int* out_type, int* out_id, int* out_modifier);
extern bool sdl_keyboard_capture_begin(void);
extern void sdl_keyboard_capture_cancel(void);
extern bool sdl_keyboard_capture_poll(SDL_Scancode* out_scancode,
    u16b* out_modifiers);

/* SDL story font control (main-sdl.c) */
extern void sdl_story_font_enable(void);
extern void sdl_story_font_disable(void);
extern void sdl_story_font_reset(void);
extern bool sdl_is_story_font_enabled(void);
extern void sdl_story_font_set_grid(bool grid);
extern bool sdl_is_story_font_grid(void);
extern void sdl_story_font_set_slot(int slot);
extern int sdl_story_font_text_width(cptr text, int len);
extern int sdl_overlay_log_wrap(const char* msg, int max_segs, int* out_off,
    int* out_len);
extern int sdl_get_cell_width(void);
extern int sdl_main_view_visible_col0(void);
extern int sdl_main_view_visible_cols(void);
extern bool sdl_left_panel_pane_map_coverage(int* start_col, int* cols,
    int* start_row, int* rows);
extern bool sdl_combat_overlay_pane_map_coverage(int* start_col, int* cols,
    int* start_row, int* rows);
extern bool sdl_overlay_log_pane_map_coverage(int* start_col, int* cols,
    int* start_row, int* rows);
extern int sdl_map_overlay_map_coverages(int max_rects, int* start_cols,
    int* cols, int* start_rows, int* rows);
extern void binding_action_label(int binding, char* buf, size_t buflen);
extern void binding_action_short(int binding, char* buf, size_t buflen);
