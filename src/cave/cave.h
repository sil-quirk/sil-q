/* File: cave.h */

#ifndef INCLUDED_CAVE_CAVE_H
#define INCLUDED_CAVE_CAVE_H

#include "../h-basic.h"

int distance(int y1, int x1, int y2, int x2);
int distance_squared(int y1, int x1, int y2, int x2);
bool los(int y1, int x1, int y2, int x2);
void random_unseen_floor(int* ry, int* rx);
bool seen_by_keen_senses(int y, int x);
bool cave_valid_bold(int y, int x);

bool feat_supports_lighting(int feat);
int player_tile_offset(void);
void map_info(int y, int x, byte* ap, char* cp, byte* tap, char* tcp);
void map_info_default(int y, int x, byte* ap, char* cp);

void move_cursor_relative(int y, int x);
void print_rel(char c, byte a, int y, int x);
void note_spot(int y, int x);
void lite_spot(int y, int x);
void prt_map(void);
void display_map(int* cy, int* cx);
void do_cmd_view_map(void);

errr vinfo_init(void);
void forget_view(void);
void update_view(void);

int flow_dist(int which_flow, int y, int x);
void update_flow(int cy, int cx, int which_flow);
void update_smell(void);
void map_feature(int y, int x);
void map_area(void);
void map_area_radius(int radius);
void wiz_light(void);
void wiz_dark(void);
void gates_illuminate(bool daytime);
byte get_depth_color(int depth);
void cave_set_feat_with_color(int y, int x, int feat, int color);
void cave_set_feat(int y, int x, int feat);

void reset_depth_color_cache(void);
void styles_init_for_level(void);
void styles_begin_vault(int extra_sidx, int extra_weight);
void styles_end_vault(void);
void styles_reset_level_weights(void);
void styles_add_level_weight(int sidx, int weight);
void styles_reset_vault_weights(void);
void styles_add_vault_weight(int sidx, int weight);
void styles_add_vault_from_level(int factor);
void styles_set_vault_avoid_style(int sidx);
void styles_default_vault_clear(void);
void styles_default_vault_add(int sidx_or_star, int weight);
void styles_apply_vault_list(const int* sidx, const int* weight, int count);
void styles_vault_rules_clear(void);
void styles_set_vault_rule(
    int depth, const int* sidx, const int* weight, int count);
void styles_apply_vault_default_for_depth(int depth);
void styles_partition_rules_clear(void);
void styles_add_partition_rule(
    int depth, int kind, const int* sidx, const int* weight, int count);
int styles_pick_partition_style(int depth, int kind);
int styles_get_level_primary_style(void);
void styles_set_loaded_level_primary(int sidx);
int styles_get_vault_primary_style(void);
void styles_select_vault_primary(void);
int styles_pick_random_from_level(void);
int styles_decode_color_style(byte color_value);
void styles_rules_clear(void);
void styles_add_level_rule(
    int min_depth, int max_depth, const int* sidx, const int* weight, int count);
int styles_get_choice_capacity(void);
void styles_copy_level_door_choices(byte* out_buf, int max_n);
void styles_load_level_door_choices(const byte* in_buf, int n);
void hallucination_randomize_style_transitions(void);
void hallucination_clear_style_transitions(void);

int project_path(
    u16b* gp, int range, int y1, int x1, int* y2, int* x2, u32b flg);
byte projectable(int y1, int x1, int y2, int x2, u32b flg);
void scatter(int* yp, int* xp, int y, int x, int d, int m);

void health_track(int m_idx);
void monster_race_track(int r_idx);
void object_kind_track(int k_idx);
void disturb(int stop_stealth, int unused_flag);

#endif /* INCLUDED_CAVE_CAVE_H */
