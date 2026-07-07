#ifndef INCLUDED_UI_MENU_CLICK_H
#define INCLUDED_UI_MENU_CLICK_H

#include "h-basic.h"

#define UI_MENU_CLICK_PRIMARY 1
#define UI_MENU_CLICK_SECONDARY 2
#define UI_MENU_CLICK_HOVER 3
#define UI_MENU_CLICK_WAKE_KEY KTRL('\\')

void ui_menu_click_clear(void);
void ui_menu_click_begin(void);
void ui_menu_click_set_hover_enabled(bool enabled);
void ui_menu_click_set_outside_cancel_enabled(bool enabled);
bool ui_menu_click_outside_cancel_enabled(void);
void ui_menu_click_set_touch_exit_button(bool enabled);
bool ui_menu_click_touch_exit_button_active(void);
void ui_menu_click_add_touch_button(int choice, cptr label, byte attr);
int ui_menu_click_touch_button_count(void);
bool ui_menu_click_touch_button_get(int index, int* choice, cptr* label,
    byte* attr);
bool ui_menu_click_is_active(void);
void ui_menu_click_set_touch_category(int category);
int ui_menu_click_get_touch_category(void);
void ui_menu_click_add(int choice, int col, int row, int width);
void ui_menu_click_add_full_row(int choice, int row);
void ui_menu_click_add_span(int choice, int col, int row, int end_col);
void ui_menu_click_add_text_span(int choice, int col, int row, cptr text,
    int start_offset, int end_offset);
void ui_menu_click_add_text_token(int choice, int col, int row, cptr text,
    cptr token);
int ui_menu_click_put_button(int choice, int row, int col, byte attr,
    cptr label);
bool ui_menu_click_has_cell(int col, int row);
bool ui_menu_click_handle_hover_cell(int col, int row, bool* wake);
bool ui_menu_click_clear_hover(bool* wake);
bool ui_menu_click_handle_choice_action(int choice, int action, bool* wake);
bool ui_menu_click_handle_cell_action(int col, int row, int action);
bool ui_menu_click_handle_cell(int col, int row);
bool ui_menu_click_has_pending(void);
bool ui_menu_click_clear_pending_hover(void);
bool ui_menu_click_take_hover_redraw(void);
bool ui_menu_click_get_hover_choice(int* choice);
bool ui_menu_click_take_action(int* choice, int* action);
bool ui_menu_click_take(int* choice);

void ui_scroll_area_clear(void);
void ui_scroll_area_begin(int top_row, int bottom_row, int touch_category);
void ui_scroll_area_begin_cols(int left_col, int right_col, int top_row,
    int bottom_row, int touch_category);
bool ui_scroll_area_add_cols(int left_col, int right_col, int top_row,
    int bottom_row, int touch_category);
bool ui_scroll_area_has_cell(int col, int row);
int ui_scroll_area_selected_index(void);
bool ui_scroll_area_select_index(int index);
int ui_scroll_area_get_touch_category(void);
void ui_scroll_area_set_keys(int positive_y_key, int negative_y_key,
    int positive_x_key, int negative_x_key);
int ui_scroll_area_get_vertical_key(int direction);
int ui_scroll_area_get_horizontal_key(int direction);
void ui_scroll_area_set_tap_key(int key);
int ui_scroll_area_get_tap_key(void);
void ui_scroll_area_set_page_mode(bool enabled);
bool ui_scroll_area_is_page_mode(void);
void ui_scroll_area_set_offset_target(int* offset, int max_offset);
bool ui_scroll_area_has_offset_target(void);
bool ui_scroll_area_offset_scroll(int delta);
bool ui_scroll_area_take_touch_scrolled(void);

void ui_key_wait_dismiss_begin(int key);
void ui_key_wait_dismiss_clear(void);
bool ui_key_wait_dismiss_is_active(void);
int ui_key_wait_dismiss_get_key(void);

#endif /* INCLUDED_UI_MENU_CLICK_H */
