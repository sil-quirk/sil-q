#include "angband.h"
#include "ui/menu-click.h"
#include "externs.h"
#include "sdl-config.h"

#define UI_MENU_CLICK_MAX_ENTRIES 256

typedef struct ui_menu_click_entry
{
    int choice;
    int col;
    int row;
    int width;
} ui_menu_click_entry;

static ui_menu_click_entry ui_menu_click_entries[UI_MENU_CLICK_MAX_ENTRIES];
static int ui_menu_click_entry_count = 0;
static bool ui_menu_click_active = false;
static bool ui_menu_click_pending = false;
static int ui_menu_click_pending_choice = 0;
static int ui_menu_click_pending_action = UI_MENU_CLICK_PRIMARY;
static bool ui_menu_click_hover_enabled = false;
static bool ui_menu_click_cancel_outside_enabled = false;
static bool ui_menu_click_hover_wake_pending = false;
static bool ui_menu_click_hover_current = false;
static bool ui_menu_click_preserve_hover_once = false;
static bool ui_menu_click_hover_redraw_pending = false;
static int ui_menu_click_hover_choice = 0;
static int ui_menu_click_touch_category = SDL_TOUCH_MENU_CATEGORY_OTHER;
static bool ui_scroll_area_active = false;
static int ui_scroll_area_top_row = 0;
static int ui_scroll_area_bottom_row = -1;
static int ui_scroll_area_touch_category = SDL_TOUCH_MENU_CATEGORY_OTHER;
static int ui_scroll_area_positive_y_key = '8';
static int ui_scroll_area_negative_y_key = '2';
static int ui_scroll_area_positive_x_key = '6';
static int ui_scroll_area_negative_x_key = '4';
static int ui_scroll_area_tap_key = 0;
static bool ui_key_wait_dismiss_active = false;
static int ui_key_wait_dismiss_key = '\r';

void ui_menu_click_clear(void)
{
    bool preserve_hover = ui_menu_click_preserve_hover_once
        && ui_menu_click_hover_current;
    int preserved_hover_choice = ui_menu_click_hover_choice;

    ui_menu_click_active = false;
    ui_menu_click_entry_count = 0;
    ui_menu_click_pending = false;
    ui_menu_click_pending_choice = 0;
    ui_menu_click_pending_action = UI_MENU_CLICK_PRIMARY;
    ui_menu_click_hover_enabled = false;
    ui_menu_click_cancel_outside_enabled = false;
    ui_menu_click_hover_wake_pending = false;
    ui_menu_click_hover_current = preserve_hover;
    ui_menu_click_hover_choice = preserve_hover ? preserved_hover_choice : 0;
    ui_menu_click_preserve_hover_once = preserve_hover;
    ui_menu_click_hover_redraw_pending = false;
    ui_menu_click_touch_category = SDL_TOUCH_MENU_CATEGORY_OTHER;
}

void ui_menu_click_begin(void)
{
    bool preserve_hover = ui_menu_click_preserve_hover_once
        && ui_menu_click_hover_current;
    int preserved_hover_choice = ui_menu_click_hover_choice;

    sdl_mouse_path_cancel();

    ui_menu_click_active = true;
    ui_menu_click_entry_count = 0;
    ui_menu_click_pending = false;
    ui_menu_click_pending_choice = 0;
    ui_menu_click_pending_action = UI_MENU_CLICK_PRIMARY;
    ui_menu_click_hover_enabled = false;
    ui_menu_click_cancel_outside_enabled = false;
    ui_menu_click_hover_wake_pending = false;
    ui_menu_click_hover_current = preserve_hover;
    ui_menu_click_hover_choice = preserve_hover ? preserved_hover_choice : 0;
    ui_menu_click_preserve_hover_once = false;
    ui_menu_click_hover_redraw_pending = false;
    ui_menu_click_touch_category = SDL_TOUCH_MENU_CATEGORY_OTHER;
}

void ui_menu_click_set_hover_enabled(bool enabled)
{
    ui_menu_click_hover_enabled = enabled;
    if (!enabled && ui_menu_click_pending_action == UI_MENU_CLICK_HOVER)
    {
        ui_menu_click_pending = false;
        ui_menu_click_pending_choice = 0;
        ui_menu_click_pending_action = UI_MENU_CLICK_PRIMARY;
        ui_menu_click_hover_wake_pending = false;
    }
}

void ui_menu_click_set_outside_cancel_enabled(bool enabled)
{
    ui_menu_click_cancel_outside_enabled = enabled;
}

bool ui_menu_click_outside_cancel_enabled(void)
{
    return ui_menu_click_active && ui_menu_click_cancel_outside_enabled;
}

bool ui_menu_click_is_active(void)
{
    return ui_menu_click_active;
}

void ui_menu_click_set_touch_category(int category)
{
    if (category < 0 || category >= SDL_TOUCH_MENU_CATEGORY_COUNT)
        category = SDL_TOUCH_MENU_CATEGORY_OTHER;

    ui_menu_click_touch_category = category;
}

int ui_menu_click_get_touch_category(void)
{
    if (ui_menu_click_touch_category < 0
        || ui_menu_click_touch_category >= SDL_TOUCH_MENU_CATEGORY_COUNT)
    {
        return SDL_TOUCH_MENU_CATEGORY_OTHER;
    }

    return ui_menu_click_touch_category;
}

void ui_menu_click_add(int choice, int col, int row, int width)
{
    int term_wid = 0;
    int term_hgt = 0;

    if (!ui_menu_click_active)
        return;
    if (ui_menu_click_entry_count >= UI_MENU_CLICK_MAX_ENTRIES)
        return;
    if (width <= 0)
        return;

    if (Term)
        Term_get_size(&term_wid, &term_hgt);

    if (term_hgt > 0 && (row < 0 || row >= term_hgt))
        return;
    if (term_wid > 0)
    {
        if (col >= term_wid)
            return;
        if (col < 0)
        {
            width += col;
            col = 0;
        }
        if (width <= 0)
            return;
        if (col + width > term_wid)
            width = term_wid - col;
    }

    ui_menu_click_entries[ui_menu_click_entry_count++] = (ui_menu_click_entry){
        choice, col, row, width
    };
}

void ui_menu_click_add_full_row(int choice, int row)
{
    int term_wid = 80;
    int term_hgt = 24;

    if (Term)
        Term_get_size(&term_wid, &term_hgt);

    (void)term_hgt;
    ui_menu_click_add(choice, 0, row, term_wid);
}

void ui_menu_click_add_span(int choice, int col, int row, int end_col)
{
    int width = end_col - col;

    if (width < 1)
        width = 1;

    ui_menu_click_add(choice, col, row, width);
}

void ui_menu_click_add_text_span(int choice, int col, int row, cptr text,
    int start_offset, int end_offset)
{
    int len;

    if (!text)
        return;

    len = (int)strlen(text);
    if (start_offset < 0)
        start_offset = 0;
    if (end_offset > len)
        end_offset = len;
    if (end_offset <= start_offset)
        return;

    ui_menu_click_add_span(choice, col + start_offset, row, col + end_offset);

    if (ui_menu_click_hover_current && ui_menu_click_hover_choice == choice)
    {
        Term_putstr(col + start_offset, row, end_offset - start_offset,
            TERM_L_BLUE, text + start_offset);
    }
}

void ui_menu_click_add_text_token(int choice, int col, int row, cptr text,
    cptr token)
{
    cptr match;
    int token_col;
    int token_width;

    if (!text || !token || !token[0])
        return;

    match = strstr(text, token);
    if (!match)
        return;

    token_col = col + (int)(match - text);
    token_width = (int)strlen(token);

    if (sdl_is_story_font_enabled() && !sdl_is_story_font_grid())
    {
        int cell_width = sdl_get_cell_width();
        int prefix_px = sdl_story_font_text_width(text, (int)(match - text));
        int token_px = sdl_story_font_text_width(token, token_width);

        if (cell_width > 0 && token_px > 0)
        {
            int start = prefix_px / cell_width;
            int end = (prefix_px + token_px + cell_width - 1) / cell_width;

            token_col = col + start;
            token_width = end - start;
            if (token_width < 1)
                token_width = 1;
        }
    }

#ifdef SIL_MOBILE
    token_col -= 2;
    token_width += 4;
#endif

    ui_menu_click_add(choice, token_col, row, token_width);

    if (ui_menu_click_hover_current && ui_menu_click_hover_choice == choice)
    {
        Term_putstr(col + (int)(match - text), row, (int)strlen(token),
            TERM_L_BLUE, token);
    }
}

static const ui_menu_click_entry* ui_menu_click_find_cell(int col, int row)
{
    if (!ui_menu_click_active)
        return NULL;

    for (int i = 0; i < ui_menu_click_entry_count; i++)
    {
        const ui_menu_click_entry* entry = &ui_menu_click_entries[i];

        if (row != entry->row)
            continue;
        if (col < entry->col || col >= entry->col + entry->width)
            continue;

        return entry;
    }

    return NULL;
}

bool ui_menu_click_has_cell(int col, int row)
{
    return ui_menu_click_find_cell(col, row) != NULL;
}

bool ui_menu_click_handle_hover_cell(int col, int row, bool* wake)
{
    const ui_menu_click_entry* entry = NULL;
    bool changed = false;

    if (wake)
        *wake = false;

    if (!ui_menu_click_hover_enabled)
        return false;

    entry = ui_menu_click_find_cell(col, row);
    if (!entry)
        return false;

    changed = !ui_menu_click_hover_current
        || ui_menu_click_hover_choice != entry->choice;
    ui_menu_click_hover_current = true;
    ui_menu_click_hover_choice = entry->choice;

    if (!changed)
        return true;

    if (ui_menu_click_pending && ui_menu_click_pending_action != UI_MENU_CLICK_HOVER)
        return true;

    ui_menu_click_pending = true;
    ui_menu_click_pending_choice = entry->choice;
    ui_menu_click_pending_action = UI_MENU_CLICK_HOVER;

    if (changed)
    {
        if (wake)
            *wake = true;
        ui_menu_click_hover_wake_pending = true;
    }

    return true;
}

bool ui_menu_click_clear_hover(bool* wake)
{
    if (wake)
        *wake = false;

    if (!ui_menu_click_hover_current)
        return false;

    ui_menu_click_hover_current = false;
    ui_menu_click_hover_choice = 0;
    ui_menu_click_preserve_hover_once = false;
    ui_menu_click_hover_redraw_pending = true;
    if (ui_menu_click_pending
        && ui_menu_click_pending_action == UI_MENU_CLICK_HOVER)
    {
        ui_menu_click_pending = false;
        ui_menu_click_pending_choice = 0;
        ui_menu_click_pending_action = UI_MENU_CLICK_PRIMARY;
    }
    ui_menu_click_hover_wake_pending = false;

    if (wake)
        *wake = true;

    return true;
}

bool ui_menu_click_handle_choice_action(int choice, int action, bool* wake)
{
    bool changed = false;

    if (wake)
        *wake = false;
    if (!ui_menu_click_active)
        return false;

    if (action != UI_MENU_CLICK_SECONDARY && action != UI_MENU_CLICK_HOVER)
        action = UI_MENU_CLICK_PRIMARY;

    if (action == UI_MENU_CLICK_HOVER)
    {
        if (!ui_menu_click_hover_enabled)
            return false;

        changed = !ui_menu_click_hover_current
            || ui_menu_click_hover_choice != choice;
        ui_menu_click_hover_current = true;
        ui_menu_click_hover_choice = choice;

        if (!changed)
            return true;

        if (ui_menu_click_pending
            && ui_menu_click_pending_action != UI_MENU_CLICK_HOVER)
        {
            return true;
        }

        ui_menu_click_pending = true;
        ui_menu_click_pending_choice = choice;
        ui_menu_click_pending_action = UI_MENU_CLICK_HOVER;
        ui_menu_click_hover_wake_pending = true;

        if (wake)
            *wake = true;
        return true;
    }

    ui_menu_click_hover_wake_pending = false;
    ui_menu_click_pending = true;
    ui_menu_click_pending_choice = choice;
    ui_menu_click_pending_action = action;
    return true;
}

bool ui_menu_click_handle_cell_action(int col, int row, int action)
{
    const ui_menu_click_entry* entry = ui_menu_click_find_cell(col, row);

    if (!entry)
        return false;

    if (action != UI_MENU_CLICK_SECONDARY && action != UI_MENU_CLICK_HOVER)
        action = UI_MENU_CLICK_PRIMARY;

    if (action == UI_MENU_CLICK_HOVER && !ui_menu_click_hover_enabled)
        return false;
    if (action != UI_MENU_CLICK_HOVER)
        ui_menu_click_hover_wake_pending = false;

    ui_menu_click_pending = true;
    ui_menu_click_pending_choice = entry->choice;
    ui_menu_click_pending_action = action;
    return true;
}

bool ui_menu_click_handle_cell(int col, int row)
{
    return ui_menu_click_handle_cell_action(col, row, UI_MENU_CLICK_PRIMARY);
}

bool ui_menu_click_has_pending(void)
{
    return ui_menu_click_pending;
}

bool ui_menu_click_clear_pending_hover(void)
{
    bool had_hover = ui_menu_click_hover_current
        || (ui_menu_click_pending
            && ui_menu_click_pending_action == UI_MENU_CLICK_HOVER);

    if (ui_menu_click_pending
        && ui_menu_click_pending_action == UI_MENU_CLICK_HOVER)
    {
        ui_menu_click_pending = false;
        ui_menu_click_pending_choice = 0;
        ui_menu_click_pending_action = UI_MENU_CLICK_PRIMARY;
    }

    ui_menu_click_hover_current = false;
    ui_menu_click_hover_choice = 0;
    ui_menu_click_preserve_hover_once = false;
    if (had_hover)
        ui_menu_click_hover_redraw_pending = true;
    ui_menu_click_hover_wake_pending = false;

    return had_hover;
}

bool ui_menu_click_take_hover_redraw(void)
{
    bool pending = ui_menu_click_hover_redraw_pending;

    ui_menu_click_hover_redraw_pending = false;
    return pending;
}

bool ui_menu_click_get_hover_choice(int* choice)
{
    if (!ui_menu_click_active || !ui_menu_click_hover_current)
        return false;

    if (choice)
        *choice = ui_menu_click_hover_choice;

    return true;
}

bool ui_menu_click_take_action(int* choice, int* action)
{
    if (!ui_menu_click_pending)
        return false;

    if (choice)
        *choice = ui_menu_click_pending_choice;
    if (action)
        *action = ui_menu_click_pending_action;
    if (ui_menu_click_pending_action == UI_MENU_CLICK_HOVER)
        ui_menu_click_preserve_hover_once = true;

    ui_menu_click_hover_wake_pending = false;

    ui_menu_click_pending = false;
    ui_menu_click_pending_choice = 0;
    ui_menu_click_pending_action = UI_MENU_CLICK_PRIMARY;
    return true;
}

bool ui_menu_click_take(int* choice)
{
    int action = UI_MENU_CLICK_PRIMARY;

    if (!ui_menu_click_take_action(choice, &action))
        return false;

    return action == UI_MENU_CLICK_PRIMARY;
}

void ui_scroll_area_clear(void)
{
    ui_scroll_area_active = false;
    ui_scroll_area_top_row = 0;
    ui_scroll_area_bottom_row = -1;
    ui_scroll_area_touch_category = SDL_TOUCH_MENU_CATEGORY_OTHER;
    ui_scroll_area_positive_y_key = '8';
    ui_scroll_area_negative_y_key = '2';
    ui_scroll_area_positive_x_key = '6';
    ui_scroll_area_negative_x_key = '4';
    ui_scroll_area_tap_key = 0;
}

void ui_scroll_area_begin(int top_row, int bottom_row, int touch_category)
{
    int term_hgt = 0;

    if (Term)
    {
        int term_wid = 0;
        Term_get_size(&term_wid, &term_hgt);
        (void)term_wid;
    }

    if (touch_category < 0 || touch_category >= SDL_TOUCH_MENU_CATEGORY_COUNT)
        touch_category = SDL_TOUCH_MENU_CATEGORY_OTHER;

    if (term_hgt > 0)
    {
        if (top_row < 0)
            top_row = 0;
        if (bottom_row >= term_hgt)
            bottom_row = term_hgt - 1;
    }

    if (bottom_row < top_row)
    {
        ui_scroll_area_clear();
        return;
    }

    ui_scroll_area_active = true;
    ui_scroll_area_top_row = top_row;
    ui_scroll_area_bottom_row = bottom_row;
    ui_scroll_area_touch_category = touch_category;
    ui_scroll_area_positive_y_key = '8';
    ui_scroll_area_negative_y_key = '2';
    ui_scroll_area_positive_x_key = '6';
    ui_scroll_area_negative_x_key = '4';
    ui_scroll_area_tap_key = 0;
}

bool ui_scroll_area_has_cell(int col, int row)
{
    int term_wid = 0;
    int term_hgt = 0;

    if (!ui_scroll_area_active)
        return false;

    if (Term)
        Term_get_size(&term_wid, &term_hgt);

    if (term_wid > 0 && (col < 0 || col >= term_wid))
        return false;
    if (term_hgt > 0 && (row < 0 || row >= term_hgt))
        return false;

    return row >= ui_scroll_area_top_row
        && row <= ui_scroll_area_bottom_row;
}

int ui_scroll_area_get_touch_category(void)
{
    if (ui_scroll_area_touch_category < 0
        || ui_scroll_area_touch_category >= SDL_TOUCH_MENU_CATEGORY_COUNT)
    {
        return SDL_TOUCH_MENU_CATEGORY_OTHER;
    }

    return ui_scroll_area_touch_category;
}

void ui_scroll_area_set_keys(int positive_y_key, int negative_y_key,
    int positive_x_key, int negative_x_key)
{
    ui_scroll_area_positive_y_key = positive_y_key;
    ui_scroll_area_negative_y_key = negative_y_key;
    ui_scroll_area_positive_x_key = positive_x_key;
    ui_scroll_area_negative_x_key = negative_x_key;
}

int ui_scroll_area_get_vertical_key(int direction)
{
    return (direction >= 0)
        ? ui_scroll_area_positive_y_key
        : ui_scroll_area_negative_y_key;
}

int ui_scroll_area_get_horizontal_key(int direction)
{
    return (direction >= 0)
        ? ui_scroll_area_positive_x_key
        : ui_scroll_area_negative_x_key;
}

void ui_scroll_area_set_tap_key(int key)
{
    ui_scroll_area_tap_key = key;
}

int ui_scroll_area_get_tap_key(void)
{
    return ui_scroll_area_tap_key;
}

void ui_key_wait_dismiss_begin(int key)
{
    ui_key_wait_dismiss_active = true;
    ui_key_wait_dismiss_key = key ? key : '\r';
}

void ui_key_wait_dismiss_clear(void)
{
    ui_key_wait_dismiss_active = false;
    ui_key_wait_dismiss_key = '\r';
}

bool ui_key_wait_dismiss_is_active(void)
{
    return ui_key_wait_dismiss_active;
}

int ui_key_wait_dismiss_get_key(void)
{
    return ui_key_wait_dismiss_key ? ui_key_wait_dismiss_key : '\r';
}
