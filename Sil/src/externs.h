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
 * (z-virt.h, z-util.h, z-form.h, z-term.h, z-rand.h)
 */


/*
 * Automatically generated "variable" declarations
 */

extern int  max_macrotrigger;
extern cptr macro_template;
extern cptr macro_modifier_chr;
extern cptr macro_modifier_name[MAX_MACRO_MOD];
extern cptr macro_trigger_name[MAX_MACRO_TRIGGER];
extern cptr macro_trigger_keycode[2][MAX_MACRO_TRIGGER];


/* tables.c */
extern const s16b ddd[9];
extern const s16b ddx[10];
extern const s16b ddy[10];
extern const s16b ddx_ddd[9];
extern const s16b ddy_ddd[9];
extern const char hexsym[16];
extern const byte extract_energy[8];
extern const player_sex sex_info[MAX_SEXES+1];
extern const byte chest_traps[25+1];
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
char mini_screenshot_char[7][7];
byte mini_screenshot_attr[7][7];
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
extern bool shimmer_monsters;
extern bool shimmer_objects;
extern bool repair_mflag_mark;
extern bool repair_mflag_show;
extern s16b o_max;
extern s16b o_cnt;
extern s16b mon_max;
extern s16b mon_cnt;
extern byte feeling;
extern bool do_feeling;
extern s16b rating;
extern bool good_item_flag;
extern bool closing_flag;
extern int player_uid;
extern int player_euid;
extern int player_egid;
extern char savefile[1024];
extern s16b macro__num;
extern cptr *macro__pat;
extern cptr *macro__act;
extern term *angband_term[ANGBAND_TERM_MAX];
extern char angband_term_name[ANGBAND_TERM_MAX][16];
extern byte angband_color_table[256][4];
extern const cptr angband_sound_name[MSG_MAX];
extern int view_n;
extern u16b *view_g;
extern int temp_n;
extern u16b *temp_g;
extern byte *temp_y;
extern byte *temp_x;
extern u16b (*cave_info)[256];
extern byte (*cave_feat)[MAX_DUNGEON_WID];
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


extern s16b stealth_score;
extern bool player_attacked;
extern bool attacked_player;
extern maxima *z_info;
extern object_type *o_list;
extern monster_type *mon_list;
extern monster_lore *l_list;
extern object_type *inventory;
extern s16b alloc_kind_size;
extern alloc_entry *alloc_kind_table;
extern s16b alloc_ego_size;
extern alloc_entry *alloc_ego_table;
extern s16b alloc_race_size;
extern alloc_entry *alloc_race_table;
extern byte misc_to_attr[256];
extern char misc_to_char[256];
extern byte tval_to_attr[128];
extern char macro_buffer[1024];
extern cptr keymap_act[KEYMAP_MODES][256];
extern const player_sex *sp_ptr;
extern const player_race *rp_ptr;
extern player_house *hp_ptr;
extern player_other *op_ptr;
extern player_type *p_ptr;
extern vault_type *v_info;
extern char *v_name;
extern char *v_text;
extern feature_type *f_info;
extern char *f_name;
extern char *f_text;
extern object_kind *k_info;
extern char *k_name;
extern char *k_text;
extern ability_type *b_info;
extern char *b_name;
extern char *b_text;
extern artefact_type *a_info;
extern char *a_text;
extern ego_item_type *e_info;
extern char *e_name;
extern char *e_text;
extern monster_race *r_info;
extern char *r_name;
extern char *r_text;
extern player_race *p_info;
extern char *p_name;
extern char *p_text;
extern player_house *c_info;
extern char *c_name;
extern char *c_text;
extern hist_type *h_info;
extern char *h_text;
extern flavor_type *flavor_info;
extern char *flavor_name;
extern char *flavor_text;
extern names_type *n_info;

extern combat_roll combat_rolls[2][MAX_COMBAT_ROLLS];
extern int combat_number;
extern int combat_number_old;
extern int turns_since_combat;
extern char combat_roll_special_char;
extern byte combat_roll_special_attr;

extern bool project_path_ignore;
extern int project_path_ignore_y;
extern int project_path_ignore_x;

extern cptr ANGBAND_SYS;
extern cptr ANGBAND_GRAF;
extern cptr ANGBAND_DIR;
extern cptr ANGBAND_DIR_APEX;
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
extern bool (*ang_sort_comp)(const void *u, const void *v, int a, int b);
extern void (*ang_sort_swap)(void *u, void *v, int a, int b);
extern bool (*get_mon_num_hook)(int r_idx);
extern bool (*get_obj_num_hook)(int k_idx);
extern void (*object_info_out_flags)(const object_type *o_ptr, u32b *f1, u32b *f2, u32b *f3);
extern FILE *text_out_file;
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
extern bool waiting_for_command;
extern bool skill_gain_in_progress;
extern bool save_game_quietly;
extern bool stop_stealth_mode;
extern bool use_background_colors;



/*
 * Automatically generated "function declarations"
 */

/* birth.c */
extern void player_birth(void);
extern bool gain_skills(void);

/* automaton.c */
extern void do_cmd_automaton(void);

/* cave.c */
extern int distance(int y1, int x1, int y2, int x2);
extern int distance_squared(int y1, int x1, int y2, int x2);
extern bool los(int y1, int x1, int y2, int x2);
extern void random_unseen_floor(int *ry, int *rx);
extern bool no_light(void);
extern bool seen_by_keen_senses(int y, int x);
extern bool cave_valid_bold(int y, int x);
extern bool feat_supports_lighting(int feat);
extern void map_info(int y, int x, byte *ap, char *cp, byte *tap, char *tcp);
extern void map_info_default(int y, int x, byte *ap, char *cp);
extern void move_cursor_relative(int y, int x);
extern void print_rel(char c, byte a, int y, int x);
extern void note_spot(int y, int x);
extern void lite_spot(int y, int x);
extern void prt_map(void);
extern void display_map(int *cy, int *cx);
extern void do_cmd_view_map(void);
extern errr vinfo_init(void);
extern void forget_view(void);
extern void update_view(void);
extern int flow_dist(int which_flow, int y, int x);
extern void update_flow(int cy, int cx, int which_flow);
extern void update_smell(void);
extern void map_area(void);
extern void wiz_light(void);
extern void wiz_dark(void);
extern void gates_illuminate(bool daytime);
extern void cave_set_feat(int y, int x, int feat);
extern int  project_path(u16b *gp, int range, int y1, int x1, int *y2, int *x2, u32b flg);
extern byte projectable(int y1, int x1, int y2, int x2, u32b flg);
extern void scatter(int *yp, int *xp, int y, int x, int d, int m);
extern void health_track(int m_idx);
extern void monster_race_track(int r_idx);
extern void object_kind_track(int k_idx);
extern void disturb(int stop_stealth, int unused_flag);

/* cmd1.c */
extern void new_wandering_flow(monster_type *m_ptr, int y, int x);
extern void new_wandering_destination(monster_type *m_ptr, monster_type *leader_ptr);
extern void drop_iron_crown(monster_type *m_ptr, const char *msg);
extern void make_alert(monster_type *m_ptr);
extern void set_alertness(monster_type *m_ptr, int alertness);
extern void perceive(void);
extern int success_chance(int sides, int skill, int difficulty);
extern int skill_check(monster_type *m_ptr1, int skill, int difficulty, monster_type *m_ptr2);
extern int light_penalty(const monster_type *m_ptr);
extern bool check_hit(int power, bool display_roll);
extern int hit_roll(int att, int evn, const monster_type *m_ptr1, const monster_type *m_ptr2, bool display_roll);
extern int total_player_attack(monster_type *m_ptr, int base);
extern int total_player_evasion(monster_type *m_ptr, bool archery);
extern int total_monster_attack(monster_type *m_ptr, int base);
extern int total_monster_evasion(monster_type *m_ptr, bool archery);
extern int stealth_melee_bonus(const monster_type *m_ptr);
extern int overwhelming_att_mod(monster_type *m_ptr);
extern int crit_bonus(int hit_result, int weight, const monster_race *r_ptr, int skill_type, bool thrown);
extern void ident(object_type *o_ptr);
extern void ident_on_wield(object_type *o_ptr);
extern void ident_resist(u32b flag);
extern void ident_passive(void);
extern void ident_see_invisible(const monster_type *m_ptr);
extern void ident_haunted(void);
extern void ident_cowardice(void);
extern void ident_hunger(void);
extern void ident_weapon_by_use(object_type *o_ptr, const monster_type *m_ptr, u32b flag);
extern void ident_bow_arrow_by_use(object_type *j_ptr, object_type *i_ptr, object_type *o_ptr,
                       const monster_type *m_ptr, u32b bow_flag, u32b arrow_flag);
extern int slay_bonus(const object_type *o_ptr, const monster_type *m_ptr, u32b *noticed_flag);
extern int prt_after_sharpness(const object_type *o_ptr, u32b *noticed_flag);
extern void search(void);
extern void do_cmd_pickup_from_pile(void);
extern void py_pickup_aux(int o_idx);
extern void py_pickup(void);
extern void hit_trap(int y, int x);
extern void display_hit(int y, int x, int net_dam, int dam_type, bool fatal_blow);
extern int concentration_bonus(int y, int x);
extern int focused_attack_bonus(void);
extern int master_hunter_bonus(monster_type *m_ptr);
extern bool knock_back(int y1, int x1, int y2, int x2);
extern void attack_punctuation(char *punctuation, int net_dam, int crit_bonus_dice);
extern void py_attack_aux(int y, int x, int attack_type);
extern void py_attack(int y, int x, int attack_type);
extern void flanking_or_retreat(int y, int x);
extern void move_player(int dir);
extern const byte cycle[];
extern const byte chome[];
extern void run_step(int dir);

/* cmd2.c */
extern int min_depth(void);
extern void note_lost_greater_vault(void);
extern void do_cmd_go_up(void);
extern void do_cmd_go_down(void);
extern void do_cmd_search(void);
extern void do_cmd_toggle_stealth(void);
extern bool do_cmd_open_aux(int y, int x);
extern void do_cmd_open(void);
extern void do_cmd_close(void);
extern void do_cmd_exchange(void);
extern void do_cmd_tunnel(void);
extern bool break_free_of_web(void);
extern void do_cmd_disarm(void);
extern void do_cmd_bash(void);
extern void do_cmd_steal(void);
extern void do_cmd_alter(void);
extern void do_cmd_spike(void);
extern bool do_cmd_walk_test(int y, int x);
extern void do_cmd_walk(void);
extern void do_cmd_jump(void);
extern void do_cmd_run(void);
extern void do_cmd_hold(void);
extern void do_cmd_pickup(void);
extern void do_cmd_rest(void);
extern int archery_range(const object_type *j_ptr);
extern int throwing_range(const object_type *i_ptr);
extern void attacks_of_opportunity(int neutralized_y, int neutralized_x);
extern void do_cmd_fire(int quiver);
extern void do_cmd_throw(bool automatic);

/* cmd3.c */
extern void do_cmd_use_item(void);
extern void do_cmd_inven(void);
extern void do_cmd_equip(void);
extern void do_cmd_wield(object_type *default_o_ptr, int default_item);
extern void do_cmd_takeoff(object_type *default_o_ptr, int default_item);
extern void do_cmd_drop(void);
extern void do_cmd_destroy(void);
extern void do_cmd_observe(void);
extern void do_cmd_uninscribe(void);
extern void do_cmd_inscribe(void);
extern void do_cmd_refuel_lamp(object_type *default_o_ptr, int default_item);
extern void do_cmd_refuel_torch(object_type *default_o_ptr, int default_item);
extern void do_cmd_refuel(void);
extern void do_cmd_target(void);
extern void do_cmd_look(void);
extern void do_cmd_locate(void);
extern void do_cmd_query_symbol(void);
extern bool ang_sort_comp_hook(const void *u, const void *v, int a, int b);
extern void ang_sort_swap_hook(void *u, void *v, int a, int b);
extern void py_steal(int y, int x);


/* cmd4.c */
extern object_type *smith_o_ptr;
extern void do_cmd_redraw(void);
extern void options_birth_menu(bool adult);
extern void do_cmd_character_sheet(void);
extern void do_cmd_change_song(void);
extern int ability_index(int skilltype, int abilitynum);
extern int elf_bane_bonus(monster_type *m_ptr);
extern int bane_bonus(monster_type *m_ptr);
extern int spider_bane_bonus(void);
extern void do_cmd_ability_screen(void);
extern int object_difficulty(object_type *o_ptr);
extern void do_cmd_smithing_screen(void);
extern void create_smithing_item(void);
extern void do_cmd_main_menu(void);
extern void do_cmd_message_one(void);
extern void do_cmd_messages(void);
extern void do_cmd_options_aux(int page, cptr info);
extern void do_cmd_options(void);
extern void do_cmd_pref(void);
extern void do_cmd_macros(void);
extern void do_cmd_visuals(void);
extern void do_cmd_colors(void);
extern void do_cmd_note(char *note, int what_depth);
extern void do_cmd_version(void);
extern void do_cmd_feeling(void);
extern void do_cmd_knowledge_notes(void);
extern void do_cmd_knowledge_artefacts(void);
extern void do_cmd_knowledge_monsters(void);
extern void do_cmd_knowledge_objects(void);
extern void do_cmd_knowledge_kills(void);
extern void ghost_challenge(void);
extern void do_cmd_save_screen(void);
extern void desc_art_fake(int a_idx);
extern void apply_magic_fake(object_type *o_ptr);
extern void do_cmd_knowledge(void);

/* cmd5.c */
extern void display_koff(int k_idx);

/* cmd6.c */
extern void do_cmd_eat_food(object_type *default_o_ptr, int default_item);
extern void do_cmd_quaff_potion(object_type *default_o_ptr, int default_item);
extern void do_cmd_activate_staff(object_type *default_o_ptr, int default_item);
extern void do_cmd_play_instrument(object_type *default_o_ptr, int default_item);
extern void do_cmd_activate(void);

/* dungeon.c */
extern bool can_be_pseudo_ided(const object_type *o_ptr);
extern int value_check_aux1(const object_type *o_ptr);
extern void land(void);
extern void pseudo_id(object_type *o_ptr);
extern void pseudo_id_everything(void);
extern void id_known_specials(void);
extern void id_everything(void);
extern void play_game(bool new_game);

/* files.c */
extern void html_screenshot(cptr name);
extern void safe_setuid_drop(void);
extern void safe_setuid_grab(void);
extern s16b tokenize(char *buf, s16b num, char **tokens);
extern errr process_pref_file_command(char *buf);
extern errr process_pref_file(cptr name);
extern errr check_time(void);
extern errr check_time_init(void);
extern void display_player_stat_info(int row, int col);
extern void display_player_xtra_info(int mode);
extern void display_player(int mode);
extern errr file_character(cptr name, bool full);
extern bool show_buffer(cptr name, cptr what, int line);
extern bool show_file(cptr name, cptr what, int line);
extern void do_cmd_help(void);
extern void process_player_name(bool sf);
extern bool get_name(void);
extern void do_cmd_escape(void);
extern void do_cmd_suicide(void);
extern void do_cmd_save_game(void);
extern void show_scores(void);
extern void comma_number(char *output, int number);
extern void atomonth(int number, char *output);
extern void display_single_score(byte attr, int row, int col, int place, int fake, high_score *the_score);
extern void display_scores(int from, int to);
extern void close_game(void);
extern void exit_game_panic(void);
#ifdef HANDLE_SIGNALS
extern void (*(*signal_aux)(int, void (*)(int)))(int);
#endif
extern void signals_ignore_tstp(void);
extern void signals_handle_tstp(void);
extern void signals_init(void);
extern void mini_screenshot(void);
extern void prt_mini_screenshot(int col, int row);
extern int silmarils_possessed(void);

/* generate.c */
extern void place_monster_by_flag(int y, int x, int flagset, u32b f, bool allow_unique, int max_depth);
extern void place_random_stairs(int y, int x);
extern byte get_nest_theme(int nestlevel);
extern byte get_pit_theme(int pitlevel);
extern void generate_cave(void);

/* init2.c */
extern void init_file_paths(char *path);
extern void display_introduction(void);
extern void init_angband(void);
extern void autoinscribe_clean(void);
extern void autoinscribe_init(void);
extern void re_init_some_things(void);
extern int initial_menu(int *highlight);
extern void cleanup_angband(void);

/* load.c */
extern bool load_player(void);

/* melee1.c */
extern int protection_roll(int typ, bool melee);
extern int p_min(int typ, bool melee);
extern int p_max(int typ, bool melee);
extern int get_sides(int attack);
extern int dodging_bonus(void);
extern bool make_attack_normal(monster_type *m_ptr);
extern bool make_attack_ranged(monster_type *m_ptr, int attack);
extern void mon_cloud(int m_idx, int typ, int dd, int ds, int dif, int rad);
extern void cloud_surround(int r_idx, int *typ, int *dd, int *ds, int *rad);
extern void shriek(monster_type *m_ptr);
extern void new_combat_round(void);
extern void update_combat_rolls1(const monster_type *m_ptr1, const monster_type *m_ptr2, bool vis, int att, int att_roll, int evn, int evn_roll);
extern void update_combat_rolls1b(const monster_type *m_ptr1, const monster_type *m_ptr2, bool vis);
extern void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps, int prot, int prt_percent, int dam_type, bool melee);
extern void display_combat_rolls(void);

/* melee2.c */
extern int adj_mon_count(int y, int x);
extern int get_scent(int y, int x);
extern bool cave_exist_mon(monster_race *r_ptr, int y, int x, bool occupied_ok, bool can_dig);
extern int cave_passable_mon(monster_type *m_ptr, int y, int x, bool *bash);
extern void tell_allies(int y, int x, u32b flag);
extern void process_monsters(s16b minimum_energy);
extern void calc_morale(monster_type *m_ptr);
extern void calc_stance(monster_type *m_ptr);
extern void monster_perception(bool player_centered, bool main_roll, int difficulty);

/* monster1.c */
extern void describe_monster(int r_idx, bool spoilers);
extern void roff_top(int r_idx);
extern void screen_roff(int r_idx);
extern void display_roff(int r_idx);

/* monster2.c */
extern s16b poly_r_idx(const monster_type *m_ptr);
extern void delete_monster_idx(int i);
extern void delete_monster(int y, int x);
extern void compact_monsters(int size);
extern void wipe_mon_list(void);
extern s16b mon_pop(void);
extern errr get_mon_num_prep(void);
extern s16b get_mon_num(int level, bool special, bool allow_non_smart, bool vault);
extern void display_monlist(void);
extern void monster_desc(char *desc, size_t max, const monster_type *m_ptr, int mode);
extern void monster_desc_race(char *desc, size_t max, int r_idx);
extern void lore_probe_aux(int r_idx);
extern void lore_treasure(int m_idx, int num_item);
extern int  monster_skill(monster_type *m_ptr, int skill_type);
extern int  monster_stat(monster_type *m_ptr, int stat_type);
extern void update_mon(int m_idx, bool full);
extern void update_monsters(bool full);
extern s16b monster_carry(int m_idx, object_type *j_ptr);
extern void monster_swap(int y1, int x1, int y2, int x2);
extern s16b player_place(int y, int x);
extern s16b monster_place(int y, int x, monster_type *n_ptr);
extern void calc_monster_speed(int y, int x);
extern void set_monster_haste(s16b m_idx, s16b counter, bool message);
extern void set_monster_slow(s16b m_idx, s16b counter, bool message);
extern void produce_cloud(monster_type *m_ptr);
extern bool place_monster_one(int y, int x, int r_idx, bool slp, bool ingnore_depth, monster_type *m_ptr);
extern bool place_monster_aux(int y, int x, int r_idx, bool slp, bool grp);
extern bool place_monster(int y, int x, bool slp, bool grp, bool vault);
extern bool alloc_monster(bool on_stairs, bool force_undead);
extern bool summon_specific(int y1, int x1, int lev, int type);
extern bool multiply_monster(int m_idx);
extern void message_pain(int m_idx, int dam);

/* obj-info.c */
extern bool object_info_out(const object_type *o_ptr);
extern void note_info_screen(const object_type *o_ptr);
extern void object_info_screen(const object_type *o_ptr);

/* object1.c */
extern bool easter_time(void);
extern void flavor_init(void);
extern void reset_visuals(bool prefs);
extern void object_flags(const object_type *o_ptr, u32b *f1, u32b *f2, u32b *f3);
extern void object_flags_known(const object_type *o_ptr, u32b *f1, u32b *f2, u32b *f3);
extern void strip_name(char *buf, int k_idx);
extern void object_desc(char *buf, size_t max, const object_type *o_ptr, int pref, int mode);
extern void object_desc_spoil(char *buf, size_t max, const object_type *o_ptr, int pref, int mode);
extern void identify_random_gen(const object_type *o_ptr);
extern char index_to_label(int i);
extern s16b label_to_inven(int c);
extern s16b label_to_equip(int c);
extern s16b wield_slot(const object_type *o_ptr);
extern cptr describe_empty_slot(int i);
extern cptr mention_use(int i);
extern cptr describe_use(int i);
extern bool item_tester_okay(const object_type *o_ptr);
extern int scan_floor(int *items, int size, int y, int x, int mode);
extern void display_inven(void);
extern void display_equip(void);
extern void show_inven(void);
extern void show_equip(void);
extern void show_floor(const int *floor_list, int floor_num);
extern void toggle_inven_equip(void);
extern bool get_item(int *cp, cptr pmt, cptr str, int mode);


/* object2.c */
extern void excise_object_idx(int o_idx);
extern void delete_object_idx(int o_idx);
extern void delete_object(int y, int x);
extern void compact_objects(int size);
extern void wipe_o_list(void);
extern s16b o_pop(void);
extern object_type* get_first_object(int y, int x);
extern object_type* get_next_object(const object_type *o_ptr);
extern errr get_obj_num_prep(void);
extern s16b get_obj_num(int level);
extern void object_known(object_type *o_ptr);
extern void object_aware(object_type *o_ptr);
extern void object_tried(object_type *o_ptr);
extern s32b object_value(const object_type *o_ptr);
extern bool object_similar(const object_type *o_ptr, const object_type *j_ptr);
extern void object_absorb(object_type *o_ptr, object_type *j_ptr);
extern s16b lookup_kind(int tval, int sval);
extern void object_wipe(object_type *o_ptr);
extern void object_copy(object_type *o_ptr, const object_type *j_ptr);
extern void object_prep(object_type *o_ptr, int k_idx);
extern void object_into_artefact(object_type *o_ptr, artefact_type *a_ptr);
extern void object_into_special(object_type *o_ptr, int lev, bool smithing);
extern void apply_magic(object_type *o_ptr, int lev, bool okay, bool good, bool great, bool allow_insta);
extern bool make_object(object_type *j_ptr, bool good, bool great, int objecttype);
extern bool prep_object_theme(int themetype);
extern s16b floor_carry(int y, int x, object_type *j_ptr);
extern void drop_near(object_type *j_ptr, int chance, int y, int x);
extern void acquirement(int y1, int x1, int num, bool great);
extern void place_object(int y, int x, bool good, bool great, int droptype);
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
extern bool inven_carry_okay(const object_type *o_ptr);
extern s16b inven_carry(object_type *o_ptr);
extern s16b inven_takeoff(int item, int amt);
extern void inven_drop(int item, int amt);
extern void combine_pack(void);
extern void reorder_pack(bool display_message);
extern void steal_object_from_monster(int y, int x);
extern byte allow_altered_inventory;

/* randart.c */
extern void make_random_name(char *random_name, size_t max);
extern s32b artefact_power(int a_idx);
extern void build_randart_tables(void);
extern void free_randart_tables(void);
extern errr do_randart(u32b randart_seed, bool full);
extern bool make_one_randart(object_type *o_ptr, int art_power, bool namechoice);
extern void artefact_wipe(int a_idx);
extern bool can_be_randart(const object_type *o_ptr);

/* save.c */
extern bool save_player(void);

/* spells1.c */
extern void teleport_away(int m_idx, int dis);
extern void teleport_player(int dis);
extern void teleport_player_to(int ny, int nx);
extern void teleport_towards(int oy, int ox, int ny, int nx);
extern void teleport_player_level(void);
extern void take_hit(int dam, cptr kb_str);
extern bool hates_acid(const object_type *o_ptr);
extern bool hates_elec(const object_type *o_ptr);
extern bool hates_fire(const object_type *o_ptr);
extern bool hates_cold(const object_type *o_ptr);
extern void acid_dam(int dam, cptr kb_str);
extern void elec_dam(int dam, cptr kb_str);
extern int resist_fire(void);
extern int resist_cold(void);
extern int resist_pois(void);
extern int resist_dark(void);
extern void fire_dam_mixed(int dam, cptr kb_str);
extern void fire_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str);
extern void cold_dam_mixed(int dam, cptr kb_str);
extern void cold_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str);
extern void dark_dam_mixed(int dam, cptr kb_str);
extern void dark_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str);
extern void pois_dam_mixed(int dam);
extern void pois_dam_pure(int dd, int ds, bool update_rolls);
extern bool inc_stat(int stat);
extern bool dec_stat(int stat, int amount, bool permanent);
extern bool res_stat(int stat, int points);
extern void disease(int *damage);
extern bool apply_disenchant(int mode);
extern bool project(int who, int rad, int y0, int x0, int y1, int x1, int dd, int ds, int dif, int typ,
			 u32b flg, int degrees, bool uniform);
extern void add_wrath(void);
extern int  slaying_song_bonus(void);
extern void song_of_binding(monster_type *m_ptr);
extern void song_of_piercing(monster_type *m_ptr);
extern void song_of_oaths(monster_type *m_ptr);
extern void change_song(int song);
extern bool singing(int song);
extern void sing(void);

/* spells2.c */
extern bool hp_player(int x, bool percent, bool message);
extern void warding_glyph(void);
extern bool do_dec_stat(int stat, monster_type *m_ptr);
extern bool do_res_stat(int stat, int points);
extern bool do_inc_stat(int stat);
extern void identify_pack(void);
extern void uncurse_object(object_type *o_ptr);
extern bool remove_curse(bool star_curse);
extern void self_knowledge(void);
extern void detect_all_doors_traps(void);
extern bool detect_traps(void);
extern bool detect_doors(void);
extern bool detect_stairs(void);
extern bool detect_objects_normal(void);
extern bool detect_objects_magic(void);
extern bool detect_monsters(void);
extern bool detect_monsters_invis(void);
extern bool detect_all(void);
extern void stair_creation(void);
extern bool item_tester_hook_digger(const object_type *o_ptr);
extern bool item_tester_hook_ided_weapon(const object_type *o_ptr);
extern bool item_tester_hook_weapon(const object_type *o_ptr);
extern bool item_tester_hook_wieldable_ided_weapon(const object_type *o_ptr);
extern bool item_tester_hook_wieldable_weapon(const object_type *o_ptr);
extern bool item_tester_hook_ided_armour(const object_type *o_ptr);
extern bool item_tester_hook_armour(const object_type *o_ptr);
extern bool item_tester_hook_elvenkindable_armour(const object_type *o_ptr);
extern bool item_tester_hook_enchantable_ring(const object_type *o_ptr);
extern bool item_tester_hook_enchantable_amulet(const object_type *o_ptr);
extern int do_ident_item(int item, object_type *o_ptr);
extern bool ident_spell(void);
extern bool item_tester_hook_recharge(const object_type *o_ptr);
extern void recharge_staff_wand(object_type *o_ptr, int lev, int num);
extern bool recharge(int num);
extern bool slow_monsters(int power);
extern bool sleep_monsters(int power);
extern bool destroy_traps(int power);
extern bool open_doors(int power);
extern bool lock_doors(int power);
extern bool turn_undead(int dd, int ds, int power);
extern bool dispel_undead(int dd, int ds);
extern void wake_all_monsters(int who);
extern bool make_aggressive(void);
extern bool banishment(void);
extern bool mass_banishment(void);
extern void destroy_area(int y1, int x1, int r, bool full);
extern void earthquake(int cy, int cx, int pit_y, int pit_x, int r, int who);
extern bool close_chasm(int y, int x, int power);
extern bool close_chasms(int power);
extern void light_room(int y1, int x1);
extern void darken_room(int y1, int x1);
extern bool light_area(int dd, int ds, int rad);
extern bool darken_area(int dd, int ds, int rad);
extern bool fire_bolt_or_beam(int prob, int typ, int dir, int dd, int ds, int dif);
extern bool fire_bolt_beam_special(int typ, int dir, int dd, int ds, int dif, int rad, u32b flg);
extern bool fire_ball(int typ, int dir, int dd, int ds, int dif, int rad);
extern bool fire_arc(int typ, int dir, int dd, int ds, int dif, int rad, int degrees);
extern bool fire_bolt(int typ, int dir, int dd, int ds, int dif);
extern bool fire_beam(int typ, int dir, int dd, int ds, int dif);
extern bool fire_bolt_or_beam(int prob, int typ, int dir, int dd, int ds, int dif);
extern bool project_arc(int who, int rad, int y0, int x0, int y1, int x1,
	int dd, int ds, int dif, int typ, u32b flg, int degrees);
extern bool project_los_not_player(int y1, int x1, int dd, int ds, int dif, int typ);
extern bool project_los(int typ, int dd, int ds, int dif);
extern void clear_temp_array(void);
extern void cave_temp_mark(int y, int x, bool room);
extern void spread_cave_temp(int y1, int x1, int range, bool room);
extern bool explosion(int who, int rad, int y0, int x0, int dd, int ds, int dif, int typ);
extern bool light_line(int dir);
extern bool blast(int dir, int dd, int ds, int dif);
extern bool destroy_door(int dir);
extern bool disarm_trap(int dir);
extern bool curse_armor(void);
extern bool curse_weapon(void);
extern bool item_tester_hook_ided_ammo(const object_type *o_ptr);
extern bool item_tester_hook_ammo(const object_type *o_ptr);
extern void identify_and_squelch_pack(void);
extern bool mass_identify(int rad);


/* squelch.c */
extern byte squelch_level[SQUELCH_BYTES];
extern int do_cmd_autoinscribe_item(s16b k_idx);
extern void do_cmd_squelch_autoinsc(void);
extern int squelch_itemp(object_type *o_ptr, byte feeling, bool fullid);
extern int do_squelch_item(int squelch, int item, object_type *o_ptr);
extern void rearrange_stack(int y, int x);
extern void do_squelch_pile(int y, int x);
extern int get_autoinscription_index(s16b k_idx);
extern void obliterate_autoinscription(s16b kind);
extern void autoinscribe_ground(void);
extern void autoinscribe_pack(void);
extern int remove_autoinscription(s16b kind);
extern int add_autoinscription(s16b kind, cptr inscription);
extern int apply_autoinscription(object_type *o_ptr);
extern char *squelch_to_label(int squelch);

/*use-obj.c*/
extern bool use_object(object_type *o_ptr, bool *ident);

/* util.c */
extern errr path_parse(char *buf, size_t max, cptr file);
extern errr path_build(char *buf, size_t max, cptr path, cptr file);
extern FILE *my_fopen(cptr file, cptr mode);
extern FILE *my_fopen_temp(char *buf, size_t max);
extern errr my_fclose(FILE *fff);
extern errr my_fgets(FILE *fff, char *buf, size_t n);
extern errr my_fputs(FILE *fff, cptr buf, size_t n);
extern errr fd_kill(cptr file);
extern errr fd_move(cptr file, cptr what);
extern errr fd_copy(cptr file, cptr what);
extern int fd_make(cptr file, int mode);
extern int fd_open(cptr file, int flags);
extern errr fd_lock(int fd, int what);
extern errr fd_seek(int fd, long n);
extern errr fd_read(int fd, char *buf, size_t n);
extern errr fd_write(int fd, cptr buf, size_t n);
extern errr fd_close(int fd);
extern errr check_modification_date(int fd, cptr template_file);
extern void text_to_ascii(char *buf, size_t len, cptr str);
extern void ascii_to_text(char *buf, size_t len, cptr str);
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
extern s16b quark_add(cptr str);
extern cptr quark_str(s16b i);
extern errr quarks_init(void);
extern errr quarks_free(void);
extern s16b message_num(void);
extern cptr message_str(s16b age);
extern u16b message_type(s16b age);
extern byte message_color(s16b age);
extern errr message_color_define(u16b type, byte color);
extern void message_add(cptr str, u16b type);
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
extern void c_put_str(byte attr, cptr str, int row, int col);
extern void put_str(cptr str, int row, int col);
extern void c_prt(byte attr, cptr str, int row, int col);
extern void prt(cptr str, int row, int col);
extern void text_out_to_file(byte attr, cptr str);
extern void text_out_to_screen(byte a, cptr str);
extern void text_out(cptr str);
extern void text_out_c(byte a, cptr str);
extern void clear_from(int row);
extern bool askfor_aux(char *buf, size_t len);
extern bool askfor_name(char *buf, size_t len);
extern bool term_get_string(cptr prompt, char *buf, size_t len);
extern s16b get_quantity(cptr prompt, int max);
extern int get_check_other(cptr prompt, char other);
extern bool get_check(cptr prompt);
extern int get_menu_choice(s16b max, char *prompt);
extern bool get_com(cptr prompt, char *command);
extern void pause_line(int row);
extern void request_command(void);
extern int int_exp(int base, int power);
extern int damroll(int num, int sides);
extern bool is_a_vowel(int ch);
extern int color_char_to_attr(char c);
extern int color_text_to_attr(cptr name);
extern cptr attr_to_text(byte a);

#ifdef SUPPORT_GAMMA
extern void build_gamma_table(int gamma);
extern byte gamma_table[256];
#endif /* SUPPORT_GAMMA */

extern byte get_angle_to_grid[41][41];
extern int get_angle_to_target(int y0, int x0, int y1, int x1, int dir);
extern void get_grid_using_angle(int angle, int y0, int x0,
	int *ty, int *tx);
extern void editing_buffer_init(editing_buffer *eb_ptr, const char *buf, size_t max_size);
extern void editing_buffer_destroy(editing_buffer *eb_ptr);
extern int editing_buffer_put_chr(editing_buffer *eb_ptr, char ch);
extern int editing_buffer_set_position(editing_buffer *eb_ptr, size_t new_pos);
extern void editing_buffer_display(editing_buffer *eb_ptr, int x, int y, byte col);
extern int editing_buffer_delete(editing_buffer *eb_ptr);
extern void editing_buffer_clear(editing_buffer *eb_ptr);
extern void editing_buffer_get_all(editing_buffer *eb_ptr, char buf[], size_t max_size);
extern int editing_buffer_put_str(editing_buffer *eb_ptr, const char *str, int n);
extern cptr get_ext_color_name(byte ext_color);


/* xtra1.c */
extern byte total_mdd(const object_type *o_ptr);
extern byte strength_modified_ds(const object_type *o_ptr, int str_adjustment);
extern byte total_mds(const object_type *o_ptr, int str_adjustment);
extern bool two_handed_melee(void);
extern int hand_and_a_half_bonus(const object_type *o_ptr);
extern int blade_bonus(const object_type *o_ptr);
extern int axe_bonus(const object_type *o_ptr);
extern int polearm_bonus(const object_type *o_ptr);
extern byte total_ads(const object_type *j_ptr, bool single_shot);
extern void cnv_stat(int val, char *out_val);
extern int health_level(int current, int max);
extern byte health_attr(int current, int max);
extern void notice_stuff(void);
extern void update_stuff(void);
extern void redraw_stuff(void);
extern void window_stuff(void);
extern void handle_stuff(void);
extern int weight_limit(void);
extern void calc_voice(void);
extern bool weapon_glows(object_type *o_ptr);
extern void calc_torch(void);
extern int ability_bonus(int skilltype, int abilitynum);
extern int affinity_level(int skilltype);

/* xtra2.c */
extern bool saving_throw(monster_type *m_ptr, int resistance);
extern bool allow_player_blind(monster_type *m_ptr);
extern bool set_blind(int v);
extern bool allow_player_confusion(monster_type *m_ptr);
extern bool set_confused(int v);
extern bool set_poisoned(int v);
extern bool allow_player_fear(monster_type *m_ptr);
extern bool set_afraid(int v);
extern bool allow_player_entrancement(monster_type *m_ptr);
extern bool set_entranced(int v);
extern bool allow_player_image(monster_type *m_ptr);
extern bool set_image(int v);
extern bool set_fast(int v);
extern bool allow_player_slow(monster_type *m_ptr);
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
extern bool set_tim_invis(int v);
extern bool set_darkened(int v);
extern bool set_oppose_fire(int v);
extern bool set_oppose_cold(int v);
extern bool set_oppose_pois(int v);
extern bool allow_player_stun(monster_type *m_ptr);
extern bool set_stun(int v);
extern bool set_cut(int v);
extern bool set_food(int v);
extern void falling_damage(bool stun);
extern void check_experience(void);
extern s32b adjusted_mon_exp(const monster_race *r_ptr, bool kill);
extern void gain_exp(s32b amount);
extern void lose_exp(s32b amount);
extern bool random_stair_location(int *sy, int *sx);
extern void break_truce(bool obvious);
extern bool similar_monsters(int m1y, int m1x, int m2y, int m2x);
extern void scare_onlooking_friends(const monster_type *m_ptr, int amount);
extern void create_chosen_artefact(byte name1, int y, int x, bool identify);
extern void drop_loot(monster_type *m_ptr);
extern void monster_death(int m_idx);
extern bool mon_take_hit(int m_idx, int dam, cptr note, int who);
extern bool modify_panel(int wy, int wx);
extern bool adjust_panel(int y, int x);
extern bool change_panel(int dir);
extern void verify_panel(void);
extern void ang_sort_aux(void *u, void *v, int p, int q);
extern void ang_sort(void *u, void *v, int n);
extern int motion_dir(int y1, int x1, int y2, int x2);
extern int target_dir(char ch);
extern bool target_able(int m_idx);
extern bool target_okay(int range);
extern bool target_sighted(void);
extern void target_set_monster(int m_idx);
extern void target_set_location(int y, int x);
extern void target_set_interactive_prepare(int mode, int range);
extern bool target_set_interactive(int mode, int range);
extern int dir_from_delta(int deltay, int deltax);
extern int rough_direction(int y1, int x1, int y2, int x2);
extern bool get_aim_dir(int *dp, int range);
extern bool get_rep_dir(int *dp);
extern bool confuse_dir(int *dp);
extern const char tutorial_leave_text[][100];
extern const char tutorial_win_text[][100];
extern const char tutorial_early_death_text[][100];
extern const char tutorial_late_death_text[][100];
extern const char female_entry_poetry[][100];
extern const char male_entry_poetry[][100];
extern const char throne_poetry[][100];
extern const char ultimate_bug_text[][100];
extern void pause_with_text(const char desc[][100], int row, int col);


/*
 * Hack -- conditional (or "bizarre") externs
 */

#ifdef SET_UID
# ifndef HAVE_USLEEP
/* util.c */
extern int usleep(unsigned long usecs);
# endif /* HAVE_USLEEP */
extern void user_name(char *buf, size_t len, int id);
#endif /* SET_UID */


#ifdef ALLOW_REPEAT
/* util.c */
extern int interactive_input(bool look_ahead);
extern void repeat_push(int what);
extern bool repeat_pull(int *what);
extern void repeat_clear(void);
extern void repeat_check(void);
#endif /* ALLOW_REPEAT */



#ifdef RISCOS
/* main-ros.c */
extern char *riscosify_name(cptr path);
#endif /* RISCOS */

#if defined(MACH_O_CARBON)
/* main-mac.c, or its derivatives */
extern u32b _fcreator;
extern u32b _ftype;
# if defined(MACH_O_CARBON)
extern void fsetfileinfo(cptr path, u32b fcreator, u32b ftype);
# endif
#endif



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
extern bool make_fake_artefact(object_type *o_ptr, byte name1);



#ifdef ALLOW_DATA_DUMP
/*
 *dump_items.c
 */

extern void write_r_info_txt(void);
extern void write_o_info_txt(void);
extern void write_e_info_txt(void);
extern void write_a_info_txt(void);
extern void dump_artefact_power(void);
extern void write_mon_power(void);

#endif /*ALLOW_DATA_DUMP*/
