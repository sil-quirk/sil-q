/* File: object/object-internal.h */

#ifndef INCLUDED_OBJECT_INTERNAL_H
#define INCLUDED_OBJECT_INTERNAL_H

#include "angband.h"

#define ENHANCED_MAX_LIST 80

bool object_is_unidentified_for_display(const object_type* o_ptr);

extern bool inventory_menu_include_equip;
extern bool inventory_menu_expand_supplies;
extern bool inventory_choice_debug_logging;
extern int inventory_menu_scroll_offset;
extern bool story_inventory_list_active;
extern bool story_equipment_list_active;

bool death_spectator_allow_menu_action(void);
bool supplies_visible_for_current_filter(void);
bool inventory_menu_uses_visible_labels(void);
bool inventory_menu_uses_expanded_supplies(void);
int inventory_visible_supply_count(void);
int inventory_visible_supply_item_at(int ordinal);
int inventory_visible_supply_ordinal(int item);
int inventory_visible_inven_item_at(int ordinal);
int inventory_visible_inven_ordinal(int item);
char inventory_visible_label_for_item(int item);
bool inventory_item_is_supply_summary(int item);
bool inventory_item_is_supply_entry(int item);
bool inventory_item_is_equipment(int item);
object_type* inventory_item_to_object_ptr(int item);
bool inventory_item_uses_inven_channel(int item);
void describe_inventory_menu_entry(int item, char* buf, size_t len);
bool get_item_okay(int item);
void inventory_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);
bool inventory_menu_same_button_cycle_enabled(void);
bool item_prompt_is_replace(cptr pmt);
void story_print_equipment_prefix(int row, int col, byte attr, cptr prefix);
bool menu_prompt_drop_suffix_if_wrapped(char* prompt, cptr suffix,
    int term_wid, bool use_story_font);
void log_inventory_selector_state(cptr stage, cptr pmt,
    const int vis_inven[], int vis_inven_cnt);
void format_supply_summary(char* buf, size_t len);
int draw_item_tile(int x, int y, object_type* o_ptr);
int draw_item_tile_with_background(int x, int y, object_type* o_ptr,
    byte background_attr);
object_type* prepare_supply_icon_object(object_type* o_ptr);
int menu_term_width(void);
int menu_term_height(void);
byte inventory_menu_selected_attr(byte source_attr);
int inventory_menu_visible_rows_for_height(int term_hgt);
int menu_weight_col_for_width(int term_wid);
int menu_label_col_for_width(int term_wid, bool display_weights);
int menu_center_col_for_len(int term_wid, int len);
int menu_overlay_clear_col(int col);
void inventory_menu_fill_selected_span(int start_col, int end_col, int row,
    byte attr);
int inventory_menu_scroll_to_selection(int scroll, int selected_row,
    int total_rows, int visible_rows, int extra_rows_after_selection);
int menu_desc_limit(int text_col, int label_col, int weight_col,
    bool display_weights);
void story_render_inventory_entry(int row, int base_col, int label_col,
    cptr desc, byte desc_attr, bool display_weights, cptr weight_text,
    byte weight_attr, cptr label_text, byte label_attr, const object_type* o_ptr,
    bool highlight, int story_term_w);
void story_render_equipment_entry(int row, int col, int slot, cptr prefix,
    byte prefix_attr, cptr desc, byte desc_attr, bool display_weights,
    cptr weight_text, byte weight_attr, cptr label_text, byte label_attr,
    const object_type* o_ptr, bool highlight, int story_term_w);
void equipment_weight_layout_rows(int first_row, int item_count,
    int term_hgt, int* divider_row, int* text_row);
int equipment_first_occupied_row(int entry_count, const int* out_index);
int equipment_next_occupied_row(int entry_count, const int* out_index,
    int current, int direction);
void draw_equipment_story_rows(int col, int entry_count, int* out_index,
    byte* out_color, char out_desc[][80], bool highlight_active,
    int highlight_index, bool display_weights, int story_term_w);

void clear_inventory_limit_failure(void);
bool inven_index_valid(int item, cptr context);
bool inventory_limit_is_stack_counted(const object_type* o_ptr);
bool player_light_capacity_okay(const object_type* o_ptr, bool record_failure);
void set_inventory_limit_failure(enum inventory_limit_group group, int limit,
    const object_type* o_ptr);
bool inventory_type_slot_available(const object_type* o_ptr,
    bool record_failure);
bool kind_is_damaged_item(int k_idx);
bool kind_is_not_damaged(int k_idx);
s16b object_roll_base_weight(const object_kind* k_ptr);
void apply_object_weight_flags(object_type* o_ptr, int base_weight, u32b flags4);

#endif /* INCLUDED_OBJECT_INTERNAL_H */
