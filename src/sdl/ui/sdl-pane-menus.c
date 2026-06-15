#include "angband.h"
#include "sdl/main-sdl-private.h"

bool sdl_main_screen_handle_supporting_pane_pointer(float x, float y)
{
    if (!sdl_pane_command_shortcuts_active())
        return false;
    if (!sdl_should_show_supporting_panes())
        return false;

    if (sdl_point_in_view_rect(PANE_INVENTORY, x, y)) {
        sdl_enqueue_bypassed_command('i');
        return true;
    }

    if (sdl_point_in_view_rect(PANE_SUPPLY, x, y)) {
        sdl_enqueue_bypassed_command('q');
        return true;
    }

    if (sdl_point_in_view_rect(PANE_WORN, x, y)) {
        sdl_enqueue_bypassed_command('e');
        return true;
    }

    if (sdl_point_in_view_rect(PANE_LOG, x, y)) {
        sdl_enqueue_bypassed_command(KTRL('P'));
        return true;
    }

    if (sdl_point_in_view_rect(PANE_ROLLS, x, y)) {
        sdl_enqueue_bypassed_command(KTRL('P'));
        return true;
    }

    return false;
}

bool sdl_pane_layout_group_enabled(enum pane_placement where)
{
    if (pane_placement_is_overlay(where))
        return false;
    if (pane_placement_is_side(where))
        return config.enable_right_panes;
    if (pane_placement_is_bottom(where))
        return config.enable_bottom_panes;

    return false;
}

bool sdl_pane_layout_config_draggable(int index)
{
    enum pane_type type;
    const SDL_Rect* rect;

    if (index < 0 || index >= pane_config_count)
        return false;
    if (!pane_config[index].enabled)
        return false;
    if (pane_placement_is_overlay(pane_config[index].where))
        return false;
    if (!sdl_pane_layout_group_enabled(pane_config[index].where))
        return false;

    type = pane_config[index].pane;
    if (type <= PANE_MAIN || type >= PANE_MAX)
        return false;
    if (type == PANE_TOUCH || type == PANE_MAP || type == PANE_LEFT_PANEL
        || type == PANE_STATUS || type == PANE_DEPTH
        || type == PANE_DESCRIPTION || type == PANE_OVERLAY_MENU
        || type == PANE_COMBAT)
    {
        return false;
    }
    if (!pane_placement_is_side(pane_config[index].where)
        && !pane_placement_is_bottom(pane_config[index].where))
    {
        return false;
    }

    rect = &g_pane_rects[type];
    return sdl_rect_has_area(rect);
}

int sdl_pane_layout_config_at(float x, float y)
{
    for (int i = 0; i < pane_config_count; i++) {
        enum pane_type type;

        if (!sdl_pane_layout_config_draggable(i))
            continue;

        type = pane_config[i].pane;
        if (sdl_point_in_rect(&g_pane_rects[type], x, y))
            return i;
    }

    return -1;
}

float sdl_pane_layout_drag_threshold_px(void)
{
    float threshold = g_state.system_scale * 8.0f;

    if (threshold < 8.0f)
        threshold = 8.0f;
    if (threshold > 16.0f)
        threshold = 16.0f;

    return threshold;
}

bool sdl_pane_layout_move_config(int from, int insert_before)
{
    struct pane_config moved;

    if (from < 0 || from >= pane_config_count)
        return false;
    if (insert_before < 0)
        insert_before = 0;
    if (insert_before > pane_config_count)
        insert_before = pane_config_count;
    if (insert_before == from || insert_before == from + 1)
        return false;

    moved = pane_config[from];
    if (from < insert_before) {
        memmove(&pane_config[from], &pane_config[from + 1],
            sizeof(pane_config[0]) * (size_t)(insert_before - from - 1));
        pane_config[insert_before - 1] = moved;
        g_pane_layout_drag.pane_config_index = insert_before - 1;
    } else {
        memmove(&pane_config[insert_before + 1], &pane_config[insert_before],
            sizeof(pane_config[0]) * (size_t)(from - insert_before));
        pane_config[insert_before] = moved;
        g_pane_layout_drag.pane_config_index = insert_before;
    }

    return true;
}

bool sdl_pane_layout_drag_reorder_at(float x, float y)
{
    int target = -1;
    int insert_before;

    for (int i = 0; i < pane_config_count; i++) {
        const SDL_Rect* rect;
        enum pane_type type;
        float axis;
        float center;

        if (i == g_pane_layout_drag.pane_config_index)
            continue;
        if (!sdl_pane_layout_config_draggable(i))
            continue;
        if (pane_config[i].where != g_pane_layout_drag.where)
            continue;

        type = pane_config[i].pane;
        rect = &g_pane_rects[type];
        if (!sdl_point_in_rect(rect, x, y))
            continue;

        axis = pane_placement_is_side(g_pane_layout_drag.where) ? y : x;
        center = pane_placement_is_side(g_pane_layout_drag.where)
            ? ((float)rect->y + (float)rect->h * 0.5f)
            : ((float)rect->x + (float)rect->w * 0.5f);
        target = i;
        insert_before = (axis >= center) ? (target + 1) : target;

        if (!sdl_pane_layout_move_config(
                g_pane_layout_drag.pane_config_index, insert_before))
        {
            return false;
        }

        sdl_store_active_pane_profile(config.min_terminal_mode);
        sdl_apply_config();
        g_pane_layout_drag.changed = true;
        log_debug("Dragged pane %d within %s group",
            g_pane_layout_drag.pane,
            pane_placement_name(g_pane_layout_drag.where));
        return true;
    }

    return false;
}

void sdl_pane_layout_drag_send_click(enum pane_type pane)
{
    if (!sdl_pane_command_shortcuts_active())
        return;

    switch (pane) {
    case PANE_INVENTORY:
        sdl_enqueue_bypassed_command('i');
        break;
    case PANE_SUPPLY:
        sdl_enqueue_bypassed_command('q');
        break;
    case PANE_WORN:
        sdl_enqueue_bypassed_command('e');
        break;
    case PANE_LOG:
        sdl_enqueue_bypassed_command(KTRL('P'));
        break;
    case PANE_ROLLS:
        sdl_enqueue_bypassed_command(KTRL('P'));
        break;
    default:
        break;
    }
}

void sdl_pane_layout_drag_cancel(void)
{
    g_pane_layout_drag.active = false;
    g_pane_layout_drag.dragged = false;
    g_pane_layout_drag.changed = false;
    g_pane_layout_drag.pane_config_index = -1;
    g_pane_layout_drag.pane = PANE_MAIN;
    g_pane_layout_drag.where = PLACE_RIGHT;
    g_pane_layout_drag.start_x = 0.0f;
    g_pane_layout_drag.start_y = 0.0f;
}

bool sdl_pane_layout_drag_handle_pointer_down(float x, float y)
{
    int index;

    if (!sdl_should_show_supporting_panes())
        return false;

    index = sdl_pane_layout_config_at(x, y);
    if (index < 0)
        return false;

    g_pane_layout_drag.active = true;
    g_pane_layout_drag.dragged = false;
    g_pane_layout_drag.changed = false;
    g_pane_layout_drag.pane_config_index = index;
    g_pane_layout_drag.pane = pane_config[index].pane;
    g_pane_layout_drag.where = pane_config[index].where;
    g_pane_layout_drag.start_x = x;
    g_pane_layout_drag.start_y = y;
    return true;
}

bool sdl_pane_layout_drag_handle_pointer_motion(float x, float y)
{
    float dx;
    float dy;
    float threshold;

    if (!g_pane_layout_drag.active)
        return false;

    if (!sdl_should_show_supporting_panes()
        || !sdl_pane_layout_config_draggable(
            g_pane_layout_drag.pane_config_index))
    {
        sdl_pane_layout_drag_cancel();
        return true;
    }

    dx = x - g_pane_layout_drag.start_x;
    dy = y - g_pane_layout_drag.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_pane_layout_drag_threshold_px();
    if (!g_pane_layout_drag.dragged && dx < threshold && dy < threshold)
        return true;

    g_pane_layout_drag.dragged = true;
    (void)sdl_pane_layout_drag_reorder_at(x, y);
    return true;
}

bool sdl_pane_layout_drag_handle_pointer_up(float x, float y)
{
    enum pane_type pane;
    bool clicked;
    bool changed;

    if (!g_pane_layout_drag.active)
        return false;

    pane = g_pane_layout_drag.pane;
    clicked = !g_pane_layout_drag.dragged
        && sdl_point_in_rect(&g_pane_rects[pane], x, y);
    changed = g_pane_layout_drag.changed;
    sdl_pane_layout_drag_cancel();

    if (changed)
        (void)save_pane_config_to_json();
    if (clicked)
        sdl_pane_layout_drag_send_click(pane);

    return true;
}

bool sdl_log_history_filter_is_valid(int filter)
{
    return filter >= LOG_HISTORY_FILTER_ALL
        && filter <= LOG_HISTORY_FILTER_COMBAT;
}

void sdl_log_pane_sync_display_filter_from_config(void)
{
    int filter = config.log_pane_display_filter;

    if (!sdl_log_history_filter_is_valid(filter))
        filter = LOG_HISTORY_FILTER_ALL;

    config.log_pane_display_filter = filter;
    g_log_pane_display_filters[PANE_LOG] = filter;
    if (!sdl_log_history_filter_is_valid(g_log_pane_display_filters[PANE_ROLLS]))
        g_log_pane_display_filters[PANE_ROLLS] = LOG_HISTORY_FILTER_ALL;
}

bool sdl_log_pane_is_filterable(enum pane_type pane)
{
    return pane == PANE_LOG || pane == PANE_ROLLS;
}

static enum pane_type sdl_log_pane_other(enum pane_type pane)
{
    return (pane == PANE_ROLLS) ? PANE_LOG : PANE_ROLLS;
}

static void sdl_log_pane_switch_to(enum pane_type pane)
{
    int index = sdl_log_pane_config_index(pane);

    if (!sdl_log_pane_is_filterable(pane))
        return;
    if (index < 0)
        return;

    if (pane == PANE_LOG) {
        if (!pane_placement_is_bottom(pane_config[index].where))
            pane_config[index].where = PLACE_BOTTOM;
        if (pane_config[index].rect.rows <= 0)
            pane_config[index].rect.rows = SDL_LOG_PANE_DEFAULT_ROWS;
        config.enable_bottom_panes = true;
    } else if (pane == PANE_ROLLS) {
        if (!pane_placement_is_overlay(pane_config[index].where))
            pane_config[index].where = PLACE_TOP_RIGHT;
        if (pane_config[index].rect.rows <= 0)
            pane_config[index].rect.rows = SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS;
        pane_config[index].rect.cols = 0;
    }

    set_sdl_pane_enabled(index, true);
    sdl_store_active_pane_profile(config.min_terminal_mode);
    sdl_apply_config();
    (void)save_pane_config_to_json();

    if (p_ptr)
        p_ptr->window |= (PW_MESSAGE | PW_COMBAT_ROLLS);
    g_state.need_present = true;
}

int sdl_log_pane_display_filter(int pane)
{
    int filter;

    if (!sdl_log_pane_is_filterable((enum pane_type)pane))
        return LOG_HISTORY_FILTER_ALL;

    filter = g_log_pane_display_filters[pane];
    if (!sdl_log_history_filter_is_valid(filter))
    {
        filter = LOG_HISTORY_FILTER_ALL;
    }

    return filter;
}

void sdl_log_pane_apply_display_filter(enum pane_type pane, int filter)
{
    if (!sdl_log_pane_is_filterable(pane))
        return;
    if (!sdl_log_history_filter_is_valid(filter))
        filter = LOG_HISTORY_FILTER_ALL;

    g_log_pane_display_filters[pane] = filter;
    if (pane == PANE_LOG)
        config.log_pane_display_filter = filter;

    if (p_ptr)
    {
        p_ptr->window |= (PW_MESSAGE | PW_COMBAT_ROLLS);
        handle_stuff();
    }

    g_state.need_present = true;

    if (pane == PANE_LOG)
        (void)save_pane_config_to_json();
}

void sdl_log_pane_queue_display_filter(enum pane_type pane, int filter)
{
    if (!sdl_log_history_filter_is_valid(filter))
        filter = LOG_HISTORY_FILTER_ALL;
    if (!sdl_log_pane_is_filterable(pane))
        return;

    g_log_pane_pending_pane = pane;
    g_log_pane_pending_filter = filter;
    g_log_pane_display_pending = true;

    if (character_icky > 0)
        Term_keypress(ESCAPE);
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
}

bool sdl_log_pane_display_process_pending(void)
{
    int filter;
    enum pane_type pane;

    if (!g_log_pane_display_pending)
        return false;

    pane = g_log_pane_pending_pane;
    filter = g_log_pane_pending_filter;
    g_log_pane_display_pending = false;
    g_log_pane_pending_pane = PANE_MAIN;
    g_log_pane_pending_filter = LOG_HISTORY_FILTER_ALL;
    (void)sdl_mouse_consume_wake_key();

    sdl_log_pane_apply_display_filter(pane, filter);
    return true;
}

bool sdl_log_pane_menu_point_to_log_pane(float x, float y,
    enum pane_type* out_pane)
{
    if (out_pane)
        *out_pane = PANE_MAIN;

    if (!sdl_pane_command_shortcuts_active())
        return false;
    if (!sdl_should_show_supporting_panes())
        return false;

    if (sdl_point_in_view_rect(PANE_LOG, x, y)) {
        if (out_pane)
            *out_pane = PANE_LOG;
        return true;
    }

    if (sdl_point_in_view_rect(PANE_ROLLS, x, y)) {
        if (out_pane)
            *out_pane = PANE_ROLLS;
        return true;
    }

    return false;
}

int sdl_log_pane_config_index(enum pane_type pane)
{
    return sdl_pane_config_index_in_array(pane_config, pane_config_count,
        pane);
}

int sdl_log_pane_current_rows(enum pane_type pane)
{
    int index = sdl_log_pane_config_index(pane);
    int default_rows = (pane == PANE_ROLLS)
        ? SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS : SDL_LOG_PANE_DEFAULT_ROWS;
    int rows;

    if (index < 0)
        return default_rows;

    rows = get_sdl_pane_current_rows(index);
    if (rows <= 0)
        rows = pane_config[index].rect.rows;
    if (rows <= 0)
        rows = default_rows;

    return rows;
}

void sdl_log_pane_set_rows(enum pane_type pane, int rows)
{
    int index = sdl_log_pane_config_index(pane);

    if (index < 0)
        return;
    if (rows < SDL_LOG_PANE_MIN_ROWS)
        rows = SDL_LOG_PANE_MIN_ROWS;
    if (rows > SDL_LOG_PANE_MAX_ROWS)
        rows = SDL_LOG_PANE_MAX_ROWS;
    if (pane_config[index].rect.rows == rows)
        return;

    pane_config[index].rect.rows = rows;
    sdl_apply_config();
    (void)save_pane_config_to_json();

    if (p_ptr)
        p_ptr->window |= (PW_MESSAGE | PW_COMBAT_ROLLS);
}

void sdl_log_pane_menu_add_entry(log_pane_menu_entry* entries,
    int* count, log_pane_menu_action action, int filter, int row_delta,
    cptr label, cptr hint)
{
    log_pane_menu_entry* entry;

    if (!entries || !count || *count >= SDL_LOG_PANE_MENU_MAX_ENTRIES)
        return;

    entry = &entries[*count];
    memset(entry, 0, sizeof(*entry));
    entry->action = action;
    entry->filter = filter;
    entry->row_delta = row_delta;
    SDL_strlcpy(entry->label, label ? label : "", sizeof(entry->label));
    SDL_strlcpy(entry->hint, hint ? hint : "", sizeof(entry->hint));
    (*count)++;
}

int sdl_log_pane_menu_collect(enum pane_type pane,
    log_pane_menu_entry* entries)
{
    int count = 0;
    int rows = sdl_log_pane_current_rows(pane);
    enum pane_type other = sdl_log_pane_other(pane);
    char rows_hint[32];

    strnfmt(rows_hint, sizeof(rows_hint), "currently %d", rows);

    sdl_log_pane_menu_add_entry(entries, &count, LOG_PANE_MENU_FILTER,
        LOG_HISTORY_FILTER_ALL, 0, "Combined", "log + combat");
    sdl_log_pane_menu_add_entry(entries, &count, LOG_PANE_MENU_FILTER,
        LOG_HISTORY_FILTER_MESSAGES, 0, "Log only", "messages");
    sdl_log_pane_menu_add_entry(entries, &count, LOG_PANE_MENU_FILTER,
        LOG_HISTORY_FILTER_COMBAT, 0, "Combat only", "rolls");
    sdl_log_pane_menu_add_entry(entries, &count, LOG_PANE_MENU_ROWS,
        LOG_HISTORY_FILTER_ALL, -1, "Rows -", rows_hint);
    sdl_log_pane_menu_add_entry(entries, &count, LOG_PANE_MENU_ROWS,
        LOG_HISTORY_FILTER_ALL, 1, "Rows +", rows_hint);
    sdl_log_pane_menu_add_entry(entries, &count, LOG_PANE_MENU_SWITCH,
        LOG_HISTORY_FILTER_ALL, 0,
        (other == PANE_ROLLS) ? "Use overlay" : "Use bottom",
        "switch");

    return count;
}

int sdl_log_pane_menu_font_px(enum pane_type pane)
{
    const sdl_view* view = &g_views[pane];
    int font_px = 0;

    if (pane == PANE_ROLLS) {
        if (sdl_view_is_overlay_log_pane(view) && view->cell_h > 0)
            font_px = view->cell_h;
    } else if (view->term_ready && view->cell_h > 0) {
        font_px = view->cell_h;
    }

    if (font_px <= 0)
        font_px = sdl_effective_pane_cell_height_for_type(pane);
    if (font_px < 8)
        font_px = 8;

    return font_px;
}

bool sdl_log_pane_menu_layout(log_pane_menu_entry* entries,
    int* out_count, SDL_FRect* out_panel)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    int count;
    float cell_w = (float)g_views[PANE_MAIN].cell_w;
    float font_px;
    float pad;
    float header_h;
    float row_h;
    float panel_w;
    float panel_h;
    float gap;
    float x;
    float y;

    if (!entries || !out_count)
        return false;
    if (!sdl_rect_has_area(&screen))
        return false;

    count = sdl_log_pane_menu_collect(g_log_pane_menu.target_pane, entries);
    if (count <= 0)
        return false;

    if (cell_w <= 0.0f)
        cell_w = 8.0f;

    font_px = (float)sdl_log_pane_menu_font_px(g_log_pane_menu.target_pane);

    pad = sdl_touch_pane_clampf(g_state.system_scale * 7.0f
        * SIDE_PANE_MENU_SCALE, 9.0f, 18.0f);
    gap = sdl_touch_pane_clampf(g_state.system_scale * 8.0f
        * SIDE_PANE_MENU_SCALE, 12.0f, 21.0f);
    header_h = font_px * 1.9f;
    if (header_h < 48.0f)
        header_h = 48.0f;
#if SIL_SDL_MOBILE_BUILD
    row_h = font_px * 2.8f;
    if (row_h < 58.0f)
        row_h = 58.0f;
#else
    row_h = font_px * 2.4f;
    if (row_h < 50.0f)
        row_h = 50.0f;
#endif
    panel_w = sdl_touch_pane_clampf(cell_w * 26.0f
        * SIDE_PANE_MENU_SCALE, 270.0f, 390.0f);
    if (panel_w < font_px * 9.0f)
        panel_w = font_px * 9.0f;
    if (panel_w > (float)screen.w - 8.0f)
        panel_w = (float)screen.w - 8.0f;
    if (panel_w < 120.0f)
        panel_w = 120.0f;

    panel_h = pad * 2.0f + header_h + row_h * (float)count;
    if (panel_h > (float)screen.h - 12.0f) {
        row_h -= (panel_h - ((float)screen.h - 12.0f)) / (float)count;
        if (row_h < 34.0f)
            row_h = 34.0f;
#if SIL_SDL_MOBILE_BUILD
        if (row_h < 44.0f)
            row_h = 44.0f;
#endif
        panel_h = pad * 2.0f + header_h + row_h * (float)count;
    }

    x = g_log_pane_menu.anchor_x + gap;
    if (g_log_pane_menu.anchor_x > (float)screen.x + (float)screen.w * 0.5f)
        x = g_log_pane_menu.anchor_x - panel_w - gap;
    y = g_log_pane_menu.anchor_y - header_h * 0.5f;

    x = sdl_touch_pane_clampf(x, (float)screen.x + 6.0f,
        (float)(screen.x + screen.w) - panel_w - 6.0f);
    y = sdl_touch_pane_clampf(y, (float)screen.y + 6.0f,
        (float)(screen.y + screen.h) - panel_h - 6.0f);

    if (out_panel) {
        *out_panel = (SDL_FRect){
            .x = x,
            .y = y,
            .w = panel_w,
            .h = panel_h,
        };
    }

    for (int i = 0; i < count; i++) {
        entries[i].rect = (SDL_FRect){
            .x = x + pad,
            .y = y + pad + header_h + row_h * (float)i + 3.0f,
            .w = panel_w - pad * 2.0f,
            .h = row_h - 6.0f,
        };
    }

    *out_count = count;
    return true;
}

int sdl_log_pane_menu_index_at(float x, float y)
{
    log_pane_menu_entry entries[SDL_LOG_PANE_MENU_MAX_ENTRIES];
    int count = 0;

    if (!sdl_log_pane_menu_layout(entries, &count, NULL))
        return -1;

    for (int i = 0; i < count; i++) {
        if (sdl_point_in_frect(&entries[i].rect, x, y))
            return i;
    }

    return -1;
}

void sdl_log_pane_menu_clear_long_press(void)
{
    g_log_pane_menu.long_press_active = false;
    g_log_pane_menu.long_press_opened = false;
    g_log_pane_menu.long_press_finger_id = 0;
    g_log_pane_menu.long_press_pane = PANE_MAIN;
    g_log_pane_menu.long_press_start_x = 0.0f;
    g_log_pane_menu.long_press_start_y = 0.0f;
    g_log_pane_menu.long_press_start_time = 0;
}

void sdl_log_pane_menu_cancel(void)
{
    bool was_active = g_log_pane_menu.active;

    g_log_pane_menu.active = false;
    g_log_pane_menu.hover_index = -1;
    g_log_pane_menu.press_active = false;
    g_log_pane_menu.press_mouse = false;
    g_log_pane_menu.press_finger_id = 0;
    g_log_pane_menu.press_index = -1;
    g_log_pane_menu.target_pane = PANE_MAIN;
    g_log_pane_menu.anchor_x = 0.0f;
    g_log_pane_menu.anchor_y = 0.0f;
    sdl_log_pane_menu_clear_long_press();

    if (was_active)
        g_state.need_present = true;
}

bool sdl_log_pane_menu_open_at(float x, float y, enum pane_type pane)
{
    log_pane_menu_entry entries[SDL_LOG_PANE_MENU_MAX_ENTRIES];

    if (!sdl_log_pane_is_filterable(pane))
        return false;
    if (sdl_log_pane_menu_collect(pane, entries) <= 0)
        return false;

    sdl_side_pane_menu_cancel();
    sdl_player_action_menu_cancel();
    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    sdl_pane_layout_drag_cancel();

    g_log_pane_menu.active = true;
    g_log_pane_menu.press_active = false;
    g_log_pane_menu.press_mouse = false;
    g_log_pane_menu.press_finger_id = 0;
    g_log_pane_menu.press_index = -1;
    g_log_pane_menu.target_pane = pane;
    g_log_pane_menu.anchor_x = x;
    g_log_pane_menu.anchor_y = y;
    g_log_pane_menu.hover_index = -1;
    g_state.need_present = true;
    return true;
}

bool sdl_log_pane_menu_open_from_pointer(float x, float y)
{
    enum pane_type pane = PANE_MAIN;

    if (!sdl_log_pane_menu_point_to_log_pane(x, y, &pane))
        return false;

    return sdl_log_pane_menu_open_at(x, y, pane);
}

void sdl_log_pane_menu_activate(int menu_index)
{
    log_pane_menu_entry entries[SDL_LOG_PANE_MENU_MAX_ENTRIES];
    int count = 0;
    log_pane_menu_entry entry;
    enum pane_type target_pane;

    if (!sdl_log_pane_menu_layout(entries, &count, NULL))
        return;
    if (menu_index < 0 || menu_index >= count)
        return;

    entry = entries[menu_index];
    target_pane = g_log_pane_menu.target_pane;
    sdl_log_pane_menu_cancel();
    if (entry.action == LOG_PANE_MENU_ROWS)
        sdl_log_pane_set_rows(target_pane,
            sdl_log_pane_current_rows(target_pane) + entry.row_delta);
    else if (entry.action == LOG_PANE_MENU_SWITCH)
        sdl_log_pane_switch_to(sdl_log_pane_other(target_pane));
    else
        sdl_log_pane_queue_display_filter(target_pane, entry.filter);
}

bool sdl_log_pane_menu_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int index;

    if (!g_log_pane_menu.active)
        return false;

    index = sdl_log_pane_menu_index_at(x, y);
    if (index < 0) {
        sdl_log_pane_menu_cancel();
        return true;
    }

    g_log_pane_menu.press_active = true;
    g_log_pane_menu.press_mouse = mouse;
    g_log_pane_menu.press_finger_id = finger_id;
    g_log_pane_menu.press_index = index;
    g_log_pane_menu.hover_index = index;
    g_state.need_present = true;
    return true;
}

bool sdl_log_pane_menu_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int index;

    (void)finger_id;
    (void)mouse;

    if (!g_log_pane_menu.active)
        return false;

    index = sdl_log_pane_menu_index_at(x, y);
    if (g_log_pane_menu.hover_index != index) {
        g_log_pane_menu.hover_index = index;
        g_state.need_present = true;
    }

    return true;
}

bool sdl_log_pane_menu_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int index;

    if (!g_log_pane_menu.active)
        return false;
    if (!g_log_pane_menu.press_active)
        return true;
    if (g_log_pane_menu.press_mouse != mouse
        || g_log_pane_menu.press_finger_id != finger_id)
    {
        return true;
    }

    index = sdl_log_pane_menu_index_at(x, y);
    if (index == g_log_pane_menu.press_index)
        sdl_log_pane_menu_activate(index);

    g_log_pane_menu.press_active = false;
    g_log_pane_menu.press_index = -1;
    return true;
}

bool sdl_log_pane_menu_handle_long_press_down(float x, float y,
    SDL_FingerID finger_id)
{
    enum pane_type pane = PANE_MAIN;

    if (g_log_pane_menu.active)
        return false;
    if (!sdl_log_pane_menu_point_to_log_pane(x, y, &pane))
        return false;

    sdl_log_pane_menu_clear_long_press();
    g_log_pane_menu.long_press_active = true;
    g_log_pane_menu.long_press_finger_id = finger_id;
    g_log_pane_menu.long_press_pane = pane;
    g_log_pane_menu.long_press_start_x = x;
    g_log_pane_menu.long_press_start_y = y;
    g_log_pane_menu.long_press_start_time = SDL_GetTicksNS();
    return true;
}

bool sdl_log_pane_menu_handle_long_press_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;

    if (!g_log_pane_menu.long_press_active
        || g_log_pane_menu.long_press_finger_id != finger_id)
    {
        return false;
    }
    if (g_log_pane_menu.long_press_opened)
        return true;

    dx = x - g_log_pane_menu.long_press_start_x;
    dy = y - g_log_pane_menu.long_press_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        sdl_log_pane_menu_clear_long_press();
        return true;
    }

    return true;
}

bool sdl_log_pane_menu_handle_long_press_up(float x, float y,
    SDL_FingerID finger_id)
{
    enum pane_type pane;
    bool opened;
    bool clicked;

    if (!g_log_pane_menu.long_press_active
        || g_log_pane_menu.long_press_finger_id != finger_id)
    {
        return false;
    }

    pane = g_log_pane_menu.long_press_pane;
    opened = g_log_pane_menu.long_press_opened;
    clicked = !opened && sdl_point_in_rect(&g_pane_rects[pane], x, y);
    sdl_log_pane_menu_clear_long_press();

    if (clicked)
        sdl_pane_layout_drag_send_click(pane);

    return true;
}

void sdl_log_pane_menu_cancel_long_press(SDL_FingerID finger_id)
{
    if (!g_log_pane_menu.long_press_active)
        return;
    if (g_log_pane_menu.long_press_finger_id != finger_id)
        return;

    sdl_log_pane_menu_clear_long_press();
}

int sdl_log_pane_menu_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_log_pane_menu.long_press_active)
        return -1;
    if (g_log_pane_menu.long_press_opened)
        return -1;
    if (!g_log_pane_menu.long_press_start_time)
        return -1;

    elapsed = now_ns - g_log_pane_menu.long_press_start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_log_pane_menu_flush_pending_press(Uint64 now_ns)
{
    if (sdl_log_pane_menu_pending_timeout_ms(now_ns) != 0)
        return false;

    if (!sdl_log_pane_menu_open_from_pointer(
            g_log_pane_menu.long_press_start_x,
            g_log_pane_menu.long_press_start_y))
    {
        sdl_log_pane_menu_clear_long_press();
        return false;
    }

    g_log_pane_menu.long_press_opened = true;
    return true;
}

void sdl_log_pane_menu_render(void)
{
    log_pane_menu_entry entries[SDL_LOG_PANE_MENU_MAX_ENTRIES];
    int count = 0;
    SDL_FRect panel;
    SDL_FRect shadow;
    SDL_FRect header;
    SDL_Color border = g_state.palette[TERM_L_BLUE];
    SDL_Color hover_border = g_state.palette[TERM_YELLOW];
    SDL_Color text = g_state.palette[TERM_WHITE];
    float pad;
    int font_px;
    int hint_px;

    if (!g_log_pane_menu.active)
        return;
    if (!sdl_log_pane_menu_layout(entries, &count, &panel))
        return;

    font_px = sdl_log_pane_menu_font_px(g_log_pane_menu.target_pane);
    hint_px = (font_px * 4) / 5;
    if (hint_px < 10)
        hint_px = 10;

    pad = sdl_touch_pane_clampf(g_state.system_scale * 7.0f
        * SIDE_PANE_MENU_SCALE, 9.0f, 18.0f);
    header = (SDL_FRect){
        .x = panel.x + pad,
        .y = panel.y + pad,
        .w = panel.w - pad * 2.0f,
        .h = entries[0].rect.y - panel.y - pad - 2.0f,
    };
    shadow = panel;
    shadow.x += 4.5f;
    shadow.y += 4.5f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 142);
    SDL_RenderFillRect(g_state.renderer, &shadow);
    SDL_SetRenderDrawColor(g_state.renderer, 12, 15, 17, 242);
    SDL_RenderFillRect(g_state.renderer, &panel);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
        210);
    SDL_RenderRect(g_state.renderer, &panel);

    SDL_SetRenderDrawColor(g_state.renderer, 22, 27, 30, 245);
    SDL_RenderFillRect(g_state.renderer, &header);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
        150);
    SDL_RenderRect(g_state.renderer, &header);
    sdl_touch_pane_draw_button_text_px(&header, NULL,
        g_log_pane_menu.target_pane == PANE_ROLLS ? "Overlay Log" : "Log Pane",
        text, font_px, font_px);

    for (int i = 0; i < count; i++) {
        bool hover = (i == g_log_pane_menu.hover_index);
        bool pressed = g_log_pane_menu.press_active
            && i == g_log_pane_menu.press_index;
        SDL_Color line = hover ? hover_border : border;
        const SDL_FRect* rect = &entries[i].rect;

        SDL_SetRenderDrawColor(g_state.renderer,
            pressed ? 48 : 32, pressed ? 54 : 38, pressed ? 58 : 42, 238);
        SDL_RenderFillRect(g_state.renderer, rect);
        SDL_SetRenderDrawColor(g_state.renderer, line.r, line.g, line.b,
            hover ? 238 : 166);
        SDL_RenderRect(g_state.renderer, rect);

        sdl_touch_pane_draw_button_text_px(rect, entries[i].label,
            entries[i].hint, text, font_px, hint_px);
    }
}

const char* sdl_side_pane_menu_label(enum pane_type pane)
{
    switch (pane) {
    case PANE_INVENTORY: return "Inventory";
    case PANE_SUPPLY: return "Supply";
    case PANE_WORN: return "Equipment";
    case PANE_INFO: return "Info";
    case PANE_CHARACTER: return "Character";
    case PANE_LOG: return "Messages";
    case PANE_MONSTERS: return "Monsters";
    case PANE_MAP: return "Map";
    case PANE_ROLLS: return "Overlay Log";
    case PANE_LEFT_PANEL: return "Left Panel";
    case PANE_STATUS: return "Status";
    case PANE_DEPTH: return "Depth";
    case PANE_DESCRIPTION: return "Description";
    case PANE_OVERLAY_MENU: return "Quick Access";
    case PANE_COMBAT: return "Combat";
    default: return "Pane";
    }
}

bool sdl_side_pane_menu_config_is_entry(int index)
{
    enum pane_type type;

    if (index < 0 || index >= pane_config_count)
        return false;

    type = pane_config[index].pane;
    if (type <= PANE_MAIN || type >= PANE_MAX)
        return false;
    if (type == PANE_TOUCH || type == PANE_LEFT_PANEL || type == PANE_STATUS
        || type == PANE_DEPTH || type == PANE_DESCRIPTION
        || type == PANE_OVERLAY_MENU || type == PANE_COMBAT)
    {
        return false;
    }

    return pane_placement_is_side(pane_config[index].where)
        && !pane_placement_is_overlay(pane_config[index].where);
}

int sdl_side_pane_menu_collect(side_pane_menu_entry* entries)
{
    int count = 0;

    for (int i = 0; i < pane_config_count && count < MAX_PANE_CONFIGS; i++) {
        if (!sdl_side_pane_menu_config_is_entry(i))
            continue;

        if (entries) {
            entries[count].pane_config_index = i;
            entries[count].pane = pane_config[i].pane;
            entries[count].enabled = pane_config[i].enabled;
            entries[count].rect = (SDL_FRect){ 0 };
        }
        count++;
    }

    return count;
}

bool sdl_side_pane_menu_point_to_side_pane(float x, float y,
    bool include_map, int* out_config_index, enum pane_type* out_pane)
{
    if (out_config_index)
        *out_config_index = -1;
    if (out_pane)
        *out_pane = PANE_MAIN;

    if (!sdl_should_show_supporting_panes())
        return false;

    for (int i = 0; i < pane_config_count; i++) {
        enum pane_type type;

        if (!sdl_side_pane_menu_config_is_entry(i))
            continue;
        if (!pane_config[i].enabled)
            continue;

        type = pane_config[i].pane;
        if (!include_map && type == PANE_MAP)
            continue;
        if (!sdl_rect_has_area(&g_pane_rects[type]))
            continue;
        if (!sdl_point_in_rect(&g_pane_rects[type], x, y))
            continue;

        if (out_config_index)
            *out_config_index = i;
        if (out_pane)
            *out_pane = type;
        return true;
    }

    return false;
}

bool sdl_side_pane_menu_layout(side_pane_menu_entry* entries,
    int* out_count, SDL_FRect* out_panel)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    int count;
    float cell_w = (float)g_views[PANE_MAIN].cell_w;
    float cell_h = (float)g_views[PANE_MAIN].cell_h;
    float pad;
    float header_h;
    float row_h;
    float panel_w;
    float panel_h;
    float gap;
    float x;
    float y;

    if (!entries || !out_count)
        return false;
    if (!sdl_rect_has_area(&screen))
        return false;

    count = sdl_side_pane_menu_collect(entries);
    if (count <= 0)
        return false;

    if (cell_w <= 0.0f)
        cell_w = 8.0f;
    if (cell_h <= 0.0f)
        cell_h = 16.0f;

    pad = sdl_touch_pane_clampf(g_state.system_scale * 7.0f
        * SIDE_PANE_MENU_SCALE, 9.0f, 18.0f);
    gap = sdl_touch_pane_clampf(g_state.system_scale * 8.0f
        * SIDE_PANE_MENU_SCALE, 12.0f, 21.0f);
    header_h = sdl_touch_pane_clampf(cell_h * 2.2f
        * SIDE_PANE_MENU_SCALE, 51.0f, 72.0f);
#if SIL_SDL_MOBILE_BUILD
    row_h = sdl_touch_pane_clampf(cell_h * 3.0f
        * SIDE_PANE_MENU_SCALE, 70.0f, 98.0f);
#else
    row_h = sdl_touch_pane_clampf(cell_h * 2.7f
        * SIDE_PANE_MENU_SCALE, 63.0f, 87.0f);
#endif
    panel_w = sdl_touch_pane_clampf(cell_w * 24.0f
        * SIDE_PANE_MENU_SCALE, 270.0f, 375.0f);
    if (panel_w > (float)screen.w - 8.0f)
        panel_w = (float)screen.w - 8.0f;
    if (panel_w < 120.0f)
        panel_w = 120.0f;

    panel_h = pad * 2.0f + header_h + row_h * (float)count;
    if (panel_h > (float)screen.h - 8.0f && count > 0) {
        float available = (float)screen.h - 8.0f - pad * 2.0f - header_h;
#if SIL_SDL_MOBILE_BUILD
        float minimum_compressed_row_h = 52.0f;
#else
        float minimum_compressed_row_h = 42.0f;
#endif

        if (available > (float)count * minimum_compressed_row_h)
            row_h = available / (float)count;
        panel_h = pad * 2.0f + header_h + row_h * (float)count;
    }

    x = g_side_pane_menu.anchor_x + gap;
    if (g_side_pane_menu.anchor_x > (float)screen.x + (float)screen.w * 0.5f)
        x = g_side_pane_menu.anchor_x - panel_w - gap;
    y = g_side_pane_menu.anchor_y - header_h * 0.5f;

    x = sdl_touch_pane_clampf(x, (float)screen.x + 6.0f,
        (float)(screen.x + screen.w) - panel_w - 6.0f);
    y = sdl_touch_pane_clampf(y, (float)screen.y + 6.0f,
        (float)(screen.y + screen.h) - panel_h - 6.0f);

    if (out_panel) {
        *out_panel = (SDL_FRect){
            .x = x,
            .y = y,
            .w = panel_w,
            .h = panel_h,
        };
    }

    for (int i = 0; i < count; i++) {
        entries[i].rect = (SDL_FRect){
            .x = x + pad,
            .y = y + pad + header_h + row_h * (float)i + 3.0f,
            .w = panel_w - pad * 2.0f,
            .h = row_h - 6.0f,
        };
    }

    *out_count = count;
    return true;
}

int sdl_side_pane_menu_index_at(float x, float y)
{
    side_pane_menu_entry entries[MAX_PANE_CONFIGS];
    int count = 0;

    if (!sdl_side_pane_menu_layout(entries, &count, NULL))
        return -1;

    for (int i = 0; i < count; i++) {
        if (sdl_point_in_frect(&entries[i].rect, x, y))
            return i;
    }

    return -1;
}

void sdl_side_pane_menu_clear_long_press(void)
{
    g_side_pane_menu.long_press_active = false;
    g_side_pane_menu.long_press_opened = false;
    g_side_pane_menu.long_press_finger_id = 0;
    g_side_pane_menu.long_press_pane = PANE_MAIN;
    g_side_pane_menu.long_press_start_x = 0.0f;
    g_side_pane_menu.long_press_start_y = 0.0f;
    g_side_pane_menu.long_press_start_time = 0;
}

void sdl_side_pane_menu_cancel(void)
{
    bool was_active = g_side_pane_menu.active;

    g_side_pane_menu.active = false;
    g_side_pane_menu.hover_index = -1;
    g_side_pane_menu.press_active = false;
    g_side_pane_menu.press_mouse = false;
    g_side_pane_menu.press_finger_id = 0;
    g_side_pane_menu.press_index = -1;
    g_side_pane_menu.anchor_x = 0.0f;
    g_side_pane_menu.anchor_y = 0.0f;
    sdl_side_pane_menu_clear_long_press();

    if (was_active)
        g_state.need_present = true;
}

bool sdl_side_pane_menu_open_at(float x, float y)
{
    side_pane_menu_entry entries[MAX_PANE_CONFIGS];

    if (sdl_side_pane_menu_collect(entries) <= 0)
        return false;

    sdl_log_pane_menu_cancel();
    sdl_player_action_menu_cancel();
    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    sdl_pane_layout_drag_cancel();

    g_side_pane_menu.active = true;
    g_side_pane_menu.press_active = false;
    g_side_pane_menu.press_mouse = false;
    g_side_pane_menu.press_finger_id = 0;
    g_side_pane_menu.press_index = -1;
    g_side_pane_menu.anchor_x = x;
    g_side_pane_menu.anchor_y = y;
    g_side_pane_menu.hover_index = -1;
    g_state.need_present = true;
    return true;
}

bool sdl_side_pane_menu_open_from_pointer(float x, float y)
{
    if (!sdl_side_pane_menu_point_to_side_pane(x, y, true, NULL, NULL))
        return false;

    return sdl_side_pane_menu_open_at(x, y);
}

void sdl_side_pane_menu_toggle(int menu_index)
{
    side_pane_menu_entry entries[MAX_PANE_CONFIGS];
    int count = 0;
    int pane_index;

    if (!sdl_side_pane_menu_layout(entries, &count, NULL))
        return;
    if (menu_index < 0 || menu_index >= count)
        return;

    pane_index = entries[menu_index].pane_config_index;
    if (pane_index < 0 || pane_index >= pane_config_count)
        return;

    set_sdl_pane_enabled(pane_index, !pane_config[pane_index].enabled);
    sdl_store_active_pane_profile(config.min_terminal_mode);
    sdl_apply_config();
    (void)save_pane_config_to_json();
    if (g_side_pane_menu.hover_index >= sdl_side_pane_menu_collect(NULL))
        g_side_pane_menu.hover_index = -1;
    g_state.need_present = true;
}

bool sdl_side_pane_menu_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int index;

    if (!g_side_pane_menu.active)
        return false;

    index = sdl_side_pane_menu_index_at(x, y);
    if (index < 0) {
        sdl_side_pane_menu_cancel();
        return true;
    }

    g_side_pane_menu.press_active = true;
    g_side_pane_menu.press_mouse = mouse;
    g_side_pane_menu.press_finger_id = finger_id;
    g_side_pane_menu.press_index = index;
    g_side_pane_menu.hover_index = index;
    g_state.need_present = true;
    return true;
}

bool sdl_side_pane_menu_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int index;

    (void)finger_id;
    (void)mouse;

    if (!g_side_pane_menu.active)
        return false;

    index = sdl_side_pane_menu_index_at(x, y);
    if (g_side_pane_menu.hover_index != index) {
        g_side_pane_menu.hover_index = index;
        g_state.need_present = true;
    }

    return true;
}

bool sdl_side_pane_menu_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int index;

    if (!g_side_pane_menu.active)
        return false;
    if (!g_side_pane_menu.press_active)
        return true;
    if (g_side_pane_menu.press_mouse != mouse
        || g_side_pane_menu.press_finger_id != finger_id)
    {
        return true;
    }

    index = sdl_side_pane_menu_index_at(x, y);
    if (index == g_side_pane_menu.press_index)
        sdl_side_pane_menu_toggle(index);

    g_side_pane_menu.press_active = false;
    g_side_pane_menu.press_index = -1;
    return true;
}

bool sdl_side_pane_menu_handle_long_press_down(float x, float y,
    SDL_FingerID finger_id)
{
    enum pane_type pane = PANE_MAIN;

    if (g_side_pane_menu.active)
        return false;
    if (!sdl_side_pane_menu_point_to_side_pane(x, y, false, NULL, &pane))
        return false;

    sdl_side_pane_menu_clear_long_press();
    g_side_pane_menu.long_press_active = true;
    g_side_pane_menu.long_press_finger_id = finger_id;
    g_side_pane_menu.long_press_pane = pane;
    g_side_pane_menu.long_press_start_x = x;
    g_side_pane_menu.long_press_start_y = y;
    g_side_pane_menu.long_press_start_time = SDL_GetTicksNS();
    return true;
}

bool sdl_side_pane_menu_handle_long_press_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;

    if (!g_side_pane_menu.long_press_active
        || g_side_pane_menu.long_press_finger_id != finger_id)
    {
        return false;
    }
    if (g_side_pane_menu.long_press_opened)
        return true;

    dx = x - g_side_pane_menu.long_press_start_x;
    dy = y - g_side_pane_menu.long_press_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        sdl_side_pane_menu_clear_long_press();
        return true;
    }

    return true;
}

bool sdl_side_pane_menu_handle_long_press_up(float x, float y,
    SDL_FingerID finger_id)
{
    enum pane_type pane;
    bool opened;
    bool clicked;

    if (!g_side_pane_menu.long_press_active
        || g_side_pane_menu.long_press_finger_id != finger_id)
    {
        return false;
    }

    pane = g_side_pane_menu.long_press_pane;
    opened = g_side_pane_menu.long_press_opened;
    clicked = !opened && sdl_point_in_rect(&g_pane_rects[pane], x, y);
    sdl_side_pane_menu_clear_long_press();

    if (clicked)
        sdl_pane_layout_drag_send_click(pane);

    return true;
}

void sdl_side_pane_menu_cancel_long_press(SDL_FingerID finger_id)
{
    if (!g_side_pane_menu.long_press_active)
        return;
    if (g_side_pane_menu.long_press_finger_id != finger_id)
        return;

    sdl_side_pane_menu_clear_long_press();
}

int sdl_side_pane_menu_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_side_pane_menu.long_press_active)
        return -1;
    if (g_side_pane_menu.long_press_opened)
        return -1;
    if (!g_side_pane_menu.long_press_start_time)
        return -1;

    elapsed = now_ns - g_side_pane_menu.long_press_start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_side_pane_menu_flush_pending_press(Uint64 now_ns)
{
    if (sdl_side_pane_menu_pending_timeout_ms(now_ns) != 0)
        return false;

    if (!sdl_side_pane_menu_open_at(g_side_pane_menu.long_press_start_x,
            g_side_pane_menu.long_press_start_y))
    {
        sdl_side_pane_menu_clear_long_press();
        return false;
    }

    g_side_pane_menu.long_press_opened = true;
    return true;
}

void sdl_side_pane_menu_render(void)
{
    side_pane_menu_entry entries[MAX_PANE_CONFIGS];
    int count = 0;
    SDL_FRect panel;
    SDL_FRect shadow;
    SDL_FRect header;
    SDL_Color border = g_state.palette[TERM_L_BLUE];
    SDL_Color hover_border = g_state.palette[TERM_YELLOW];
    SDL_Color text = g_state.palette[TERM_WHITE];
    SDL_Color muted = g_state.palette[TERM_SLATE];
    float pad;

    if (!g_side_pane_menu.active)
        return;
    if (!sdl_side_pane_menu_layout(entries, &count, &panel))
        return;

    pad = sdl_touch_pane_clampf(g_state.system_scale * 7.0f
        * SIDE_PANE_MENU_SCALE, 9.0f, 18.0f);
    header = (SDL_FRect){
        .x = panel.x + pad,
        .y = panel.y + pad,
        .w = panel.w - pad * 2.0f,
        .h = entries[0].rect.y - panel.y - pad - 2.0f,
    };
    shadow = panel;
    shadow.x += 4.5f;
    shadow.y += 4.5f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 142);
    SDL_RenderFillRect(g_state.renderer, &shadow);
    SDL_SetRenderDrawColor(g_state.renderer, 12, 15, 17, 242);
    SDL_RenderFillRect(g_state.renderer, &panel);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b, 210);
    SDL_RenderRect(g_state.renderer, &panel);

    SDL_SetRenderDrawColor(g_state.renderer, 22, 27, 30, 245);
    SDL_RenderFillRect(g_state.renderer, &header);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b, 150);
    SDL_RenderRect(g_state.renderer, &header);
    sdl_touch_pane_draw_button_text_scaled(&header, NULL, "Side Panes",
        text, 0.48f, 0.63f);

    for (int i = 0; i < count; i++) {
        bool hover = (i == g_side_pane_menu.hover_index);
        bool pressed = g_side_pane_menu.press_active
            && i == g_side_pane_menu.press_index;
        SDL_Color line = hover ? hover_border : border;
        SDL_Color label = entries[i].enabled ? text : muted;
        const char* status = entries[i].enabled ? "on" : "off";
        const SDL_FRect* rect = &entries[i].rect;

        if (entries[i].enabled)
            SDL_SetRenderDrawColor(g_state.renderer,
                pressed ? 48 : 32, pressed ? 54 : 38, pressed ? 58 : 42, 238);
        else
            SDL_SetRenderDrawColor(g_state.renderer,
                pressed ? 34 : 22, pressed ? 36 : 24, pressed ? 38 : 26, 230);
        SDL_RenderFillRect(g_state.renderer, rect);
        SDL_SetRenderDrawColor(g_state.renderer, line.r, line.g, line.b,
            hover ? 238 : 166);
        SDL_RenderRect(g_state.renderer, rect);

        sdl_touch_pane_draw_button_text_scaled(rect,
            sdl_side_pane_menu_label(entries[i].pane), status, label,
            0.30f, 0.40f);
    }
}


