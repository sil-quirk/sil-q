#include "angband.h"
#include "sdl/main-sdl-private.h"

bool sdl_player_map_rect(int y, int x, SDL_FRect* out_rect)
{
    int cell_cols = use_bigtile ? 2 : 1;
    int term_row;
    int term_col;

    if (!out_rect || !p_ptr)
        return false;
    if (!panel_contains(y, x))
        return false;

    term_row = ROW_MAP + (y - p_ptr->wy);
    term_col = COL_MAP + (x - p_ptr->wx) * cell_cols;
    return sdl_main_cell_rect(term_col, term_row, cell_cols, 1, out_rect);
}

bool sdl_point_in_frect(const SDL_FRect* rect, float x, float y)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return false;

    return x >= rect->x && x < rect->x + rect->w
        && y >= rect->y && y < rect->y + rect->h;
}

void sdl_player_confirm_at_player(void)
{
    sdl_player_action_menu_cancel();
    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    sdl_touch_pane_send_confirm_action();
}

bool sdl_main_view_point_is_player_grid(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x))
        return false;

    return map_y == p_ptr->py && map_x == p_ptr->px;
}

bool sdl_player_has_equipped_staff(void)
{
    return inventory[INVEN_STAFF].k_idx
        && inventory[INVEN_STAFF].tval == TV_STAFF;
}

bool sdl_player_has_equipped_horn(void)
{
    return inventory[INVEN_HORN].k_idx
        && inventory[INVEN_HORN].tval == TV_HORN;
}

bool sdl_player_has_singable_song(void)
{
    if (!p_ptr)
        return false;

    for (int i = 0; i < SNG_MAX; i++) {
        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
            continue;
        if (p_ptr->active_ability[S_SNG][i])
            return true;
    }

    return false;
}

void sdl_player_action_menu_add_entry(player_action_menu_entry* entries,
    int* count, int kind, int command, cptr label)
{
    if (!entries || !count || *count >= SDL_PLAYER_ACTION_MAX)
        return;

    entries[*count].kind = kind;
    entries[*count].command = command;
    entries[*count].label = label;
    entries[*count].rect = (SDL_FRect) { 0 };
    (*count)++;
}

int sdl_player_action_menu_collect(player_action_menu_entry* entries)
{
    int count = 0;

    sdl_player_action_menu_add_entry(entries, &count, SDL_PLAYER_ACTION_WAIT,
        'z', "Wait");
    sdl_player_action_menu_add_entry(entries, &count, SDL_PLAYER_ACTION_USE,
        'u', "Use");
    sdl_player_action_menu_add_entry(entries, &count,
        SDL_PLAYER_ACTION_STEALTH, 'S', "Stealth");
    if (sdl_player_has_singable_song()) {
        sdl_player_action_menu_add_entry(entries, &count, SDL_PLAYER_ACTION_SING,
            's', "Sing");
    }
    if (p_ptr && p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES]) {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_EXCHANGE, 'X', "Xchg");
    }
    if (p_ptr && p_ptr->active_ability[S_ARC][ARC_FLETCHERY]) {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_FLETCH, '-', "Fletch");
    }
    sdl_player_action_menu_add_entry(entries, &count,
        SDL_PLAYER_ACTION_EXAMINE, 'x', "Desc");
    if (sdl_player_has_equipped_staff()) {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_ACTIVATE, 'a', "Staff");
    }
    if (sdl_player_has_equipped_horn()) {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_HORN, 'p', "Horn");
    }

    return count;
}

bool sdl_player_has_floor_item_underfoot(void)
{
    int floor_list[MAX_FLOOR_STACK];

    if (!p_ptr)
        return false;

    return scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px,
        0x00) > 0;
}

bool sdl_player_action_menu_kind_supports_secondary(int kind)
{
    return kind == SDL_PLAYER_ACTION_WAIT || kind == SDL_PLAYER_ACTION_USE
        || kind == SDL_PLAYER_ACTION_EXAMINE;
}

int sdl_player_action_menu_default_kind(void)
{
    player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = sdl_player_action_menu_collect(entries);

    if (count <= 0)
        return SDL_PLAYER_ACTION_NONE;

    for (int i = 0; i < count; i++) {
        if (entries[i].kind == SDL_PLAYER_ACTION_USE)
            return entries[i].kind;
    }

    return entries[0].kind;
}

int sdl_player_action_menu_hover_index(
    player_action_menu_entry* entries, int count)
{
    if (!entries || count <= 0)
        return -1;

    for (int i = 0; i < count; i++) {
        if (entries[i].kind == g_player_action_menu.hover_kind)
            return i;
    }

    return -1;
}

void sdl_player_action_menu_select_default(void)
{
    int kind = sdl_player_action_menu_default_kind();

    if (kind == SDL_PLAYER_ACTION_NONE)
        return;

    if (g_player_action_menu.hover_kind != kind) {
        g_player_action_menu.hover_kind = kind;
        g_state.need_present = true;
    }
}

void sdl_player_action_menu_move_hover(int delta)
{
    player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = sdl_player_action_menu_collect(entries);
    int index;

    if (count <= 0)
        return;

    index = sdl_player_action_menu_hover_index(entries, count);
    if (index < 0) {
        index = (delta < 0) ? count - 1 : 0;
    } else {
        index = (index + delta) % count;
        if (index < 0)
            index += count;
    }

    if (g_player_action_menu.hover_kind != entries[index].kind) {
        g_player_action_menu.hover_kind = entries[index].kind;
        g_state.need_present = true;
    }
}

void sdl_player_action_menu_activate_hover(void)
{
    if (g_player_action_menu.hover_kind == SDL_PLAYER_ACTION_NONE)
        sdl_player_action_menu_select_default();

    if (g_player_action_menu.hover_kind != SDL_PLAYER_ACTION_NONE)
        sdl_player_action_menu_activate_kind(g_player_action_menu.hover_kind,
            false);
}

void sdl_player_action_menu_start_gamepad_press(int button, int kind)
{
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return;
    if (!sdl_player_action_menu_kind_supports_secondary(kind))
        return;

    sdl_player_action_menu_cancel_press();
    g_player_action_menu.press_active = true;
    g_player_action_menu.press_mouse = false;
    g_player_action_menu.press_gamepad = true;
    g_player_action_menu.press_secondary = false;
    g_player_action_menu.press_finger_id = 0;
    g_player_action_menu.press_button = button;
    g_player_action_menu.press_kind = kind;
    g_player_action_menu.press_start_x = 0.0f;
    g_player_action_menu.press_start_y = 0.0f;
    g_player_action_menu.press_start_time = SDL_GetTicksNS();
    g_state.need_present = true;
}

bool sdl_player_action_menu_handle_gamepad_confirm(int button, bool down)
{
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return false;

    if (g_player_action_menu.press_active
        && g_player_action_menu.press_gamepad)
    {
        int kind = g_player_action_menu.press_kind;
        bool activate_secondary = false;

        if (g_player_action_menu.press_button != button)
            return true;
        if (down)
            return true;

        if (sdl_player_action_menu_kind_supports_secondary(kind)
            && g_player_action_menu.press_start_time
            && SDL_GetTicksNS() - g_player_action_menu.press_start_time
                >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        {
            activate_secondary = true;
        }

        sdl_player_action_menu_cancel_press();
        if (kind != SDL_PLAYER_ACTION_NONE)
            sdl_player_action_menu_activate_kind(kind, activate_secondary);
        return true;
    }

    if (!down)
        return true;

    if (g_player_action_menu.hover_kind == SDL_PLAYER_ACTION_NONE)
        sdl_player_action_menu_select_default();

    if (sdl_player_action_menu_kind_supports_secondary(
            g_player_action_menu.hover_kind))
    {
        sdl_player_action_menu_start_gamepad_press(button,
            g_player_action_menu.hover_kind);
    } else {
        sdl_player_action_menu_activate_hover();
    }

    return true;
}

void sdl_player_action_menu_slot_offset(int slot, int count,
    float* out_x, float* out_y)
{
    static const float offsets[SDL_PLAYER_ACTION_MAX + 1]
        [SDL_PLAYER_ACTION_MAX][2] = {
            { { 0.0f, 0.0f } },
            { { 0.0f, -1.0f } },
            { { -0.78f, -0.78f }, { 0.78f, -0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f } },
            { { -0.78f, -0.78f }, { 0.78f, -0.78f },
              { 0.78f, 0.78f }, { -0.78f, 0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 0.78f, 0.78f },
              { -0.78f, 0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 0.78f, 0.78f },
              { 0.0f, 1.0f }, { -0.78f, 0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 1.0f, 0.0f },
              { 0.78f, 0.78f }, { -0.78f, 0.78f },
              { -1.0f, 0.0f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 1.0f, 0.0f },
              { 0.78f, 0.78f }, { 0.0f, 1.0f },
              { -0.78f, 0.78f }, { -1.0f, 0.0f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 1.0f, -0.34f },
              { 1.0f, 0.34f }, { 0.78f, 0.78f },
              { 0.0f, 1.0f }, { -0.78f, 0.78f },
              { -1.0f, 0.0f } },
        };
    float x = 0.0f;
    float y = -1.0f;

    if (count < 1)
        count = 1;
    if (count > SDL_PLAYER_ACTION_MAX)
        count = SDL_PLAYER_ACTION_MAX;
    if (slot >= 0 && slot < count) {
        x = offsets[count][slot][0];
        y = offsets[count][slot][1];
    }

    if (out_x)
        *out_x = x;
    if (out_y)
        *out_y = y;
}

bool sdl_player_action_menu_layout(player_action_menu_entry* entries,
    int* out_count)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    SDL_FRect player_rect;
    SDL_FRect bounds;
    float button_w;
    float button_h;
    float radius;
    float center_x;
    float center_y;
    float larger;
    int count;

    if (!p_ptr || !entries || !out_count)
        return false;
    if (!sdl_player_map_rect(p_ptr->py, p_ptr->px, &player_rect))
        return false;

    bounds = (SDL_FRect) {
        .x = (float)(view->rect.x + view->margin_x),
        .y = (float)(view->rect.y + view->margin_y),
        .w = (float)(sdl_main_view_visual_cols(view) * view->cell_w),
        .h = (float)(sdl_main_view_visual_rows(view) * view->cell_h),
    };
    if (bounds.w <= 44.0f || bounds.h <= 44.0f)
        return false;

    count = sdl_player_action_menu_collect(entries);
    if (count <= 0)
        return false;

    button_w = sdl_touch_pane_clampf((float)view->cell_w * 6.8f, 74.0f,
        108.0f);
    button_h = sdl_touch_pane_clampf((float)view->cell_h * 2.65f, 42.0f,
        62.0f);
    if (button_w > bounds.w - 6.0f)
        button_w = bounds.w - 6.0f;
    if (button_h > bounds.h - 6.0f)
        button_h = bounds.h - 6.0f;
    if (button_w <= 0.0f || button_h <= 0.0f)
        return false;

    larger = (button_w > button_h) ? button_w : button_h;
    radius = sdl_touch_pane_clampf(larger * 1.34f, 84.0f, 138.0f);
    center_x = player_rect.x + player_rect.w * 0.5f;
    center_y = player_rect.y + player_rect.h * 0.5f;

    for (int i = 0; i < count; i++) {
        float dx;
        float dy;
        SDL_FRect rect;

        sdl_player_action_menu_slot_offset(i, count, &dx, &dy);
        rect.w = button_w;
        rect.h = button_h;
        rect.x = center_x + dx * radius - rect.w * 0.5f;
        rect.y = center_y + dy * radius - rect.h * 0.5f;
        rect.x = sdl_touch_pane_clampf(rect.x, bounds.x,
            bounds.x + bounds.w - rect.w);
        rect.y = sdl_touch_pane_clampf(rect.y, bounds.y,
            bounds.y + bounds.h - rect.h);
        entries[i].rect = rect;
    }

    *out_count = count;
    return true;
}

int sdl_player_action_menu_kind_at(float x, float y)
{
    player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = 0;

    if (!sdl_player_action_menu_layout(entries, &count))
        return SDL_PLAYER_ACTION_NONE;

    for (int i = count - 1; i >= 0; i--) {
        if (sdl_point_in_frect(&entries[i].rect, x, y))
            return entries[i].kind;
    }

    return SDL_PLAYER_ACTION_NONE;
}

void sdl_player_action_menu_cancel_press(void)
{
    g_player_action_menu.press_active = false;
    g_player_action_menu.press_mouse = false;
    g_player_action_menu.press_gamepad = false;
    g_player_action_menu.press_secondary = false;
    g_player_action_menu.press_finger_id = 0;
    g_player_action_menu.press_button = -1;
    g_player_action_menu.press_kind = SDL_PLAYER_ACTION_NONE;
    g_player_action_menu.press_start_x = 0.0f;
    g_player_action_menu.press_start_y = 0.0f;
    g_player_action_menu.press_start_time = 0;
}

void sdl_player_action_menu_cancel(void)
{
    bool was_active = g_player_action_menu.active;

    g_player_action_menu.active = false;
    g_player_action_menu.hover_kind = SDL_PLAYER_ACTION_NONE;
    sdl_player_action_menu_cancel_press();

    if (was_active)
        g_state.need_present = true;
}

bool sdl_player_exchange_target_valid(int y, int x, int* out_m_idx)
{
    int m_idx = 0;
    monster_type* m_ptr;
    monster_race* r_ptr;

    if (out_m_idx)
        *out_m_idx = 0;
    if (!p_ptr || !p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
        return false;
    if (distance(p_ptr->py, p_ptr->px, y, x) != 1)
        return false;
    if (!sdl_mouse_grid_has_visible_monster(y, x, &m_idx))
        return false;

    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx)
        return false;
    r_ptr = &r_info[m_ptr->r_idx];
    if (r_ptr->flags1 & (RF1_NEVER_MOVE | RF1_HIDDEN_MOVE))
        return false;

    if (out_m_idx)
        *out_m_idx = m_idx;
    return true;
}

bool sdl_player_exchange_has_any_target(void)
{
    for (int d = 1; d <= 9; d++) {
        int y;
        int x;

        if (d == 5)
            continue;
        y = p_ptr->py + ddy[d];
        x = p_ptr->px + ddx[d];
        if (sdl_player_exchange_target_valid(y, x, NULL))
            return true;
    }

    return false;
}

int sdl_player_exchange_collect_targets(
    player_exchange_target_entry* entries, int max_entries)
{
    static const int dir_order[SDL_PLAYER_EXCHANGE_MAX_TARGETS] =
        { 8, 9, 6, 3, 2, 1, 4, 7 };
    int count = 0;

    if (!p_ptr || !entries || max_entries <= 0)
        return 0;

    for (int i = 0; i < SDL_PLAYER_EXCHANGE_MAX_TARGETS
         && count < max_entries; i++)
    {
        int dir = dir_order[i];
        int y = p_ptr->py + ddy[dir];
        int x = p_ptr->px + ddx[dir];
        int m_idx = 0;

        if (!sdl_player_exchange_target_valid(y, x, &m_idx))
            continue;

        entries[count].y = y;
        entries[count].x = x;
        entries[count].m_idx = m_idx;
        count++;
    }

    return count;
}

int sdl_player_exchange_target_index(
    const player_exchange_target_entry* entries, int count, int y, int x,
    int m_idx)
{
    if (!entries || count <= 0)
        return -1;

    for (int i = 0; i < count; i++) {
        if (entries[i].y == y && entries[i].x == x
            && entries[i].m_idx == m_idx)
        {
            return i;
        }
    }

    return -1;
}

void sdl_player_exchange_set_hover(
    const player_exchange_target_entry* entry)
{
    if (!entry)
        return;

    if (!g_player_exchange_target.hover_active
        || g_player_exchange_target.hover_y != entry->y
        || g_player_exchange_target.hover_x != entry->x
        || g_player_exchange_target.hover_m_idx != entry->m_idx)
    {
        g_player_exchange_target.hover_active = true;
        g_player_exchange_target.hover_y = entry->y;
        g_player_exchange_target.hover_x = entry->x;
        g_player_exchange_target.hover_m_idx = entry->m_idx;
        g_state.need_present = true;
    }
}

void sdl_player_exchange_select_default(void)
{
    player_exchange_target_entry entries[SDL_PLAYER_EXCHANGE_MAX_TARGETS];
    int count = sdl_player_exchange_collect_targets(entries,
        SDL_PLAYER_EXCHANGE_MAX_TARGETS);

    if (count > 0)
        sdl_player_exchange_set_hover(&entries[0]);
}

void sdl_player_exchange_cancel_press(void)
{
    g_player_exchange_target.press_active = false;
    g_player_exchange_target.press_mouse = false;
    g_player_exchange_target.press_finger_id = 0;
    g_player_exchange_target.press_y = 0;
    g_player_exchange_target.press_x = 0;
    g_player_exchange_target.press_start_x = 0.0f;
    g_player_exchange_target.press_start_y = 0.0f;
}

void sdl_player_exchange_cancel(void)
{
    bool was_active = g_player_exchange_target.active;

    g_player_exchange_target.active = false;
    g_player_exchange_target.hover_active = false;
    g_player_exchange_target.hover_y = 0;
    g_player_exchange_target.hover_x = 0;
    g_player_exchange_target.hover_m_idx = 0;
    g_player_exchange_target.selected = false;
    g_player_exchange_target.selected_y = 0;
    g_player_exchange_target.selected_x = 0;
    g_player_exchange_target.selected_m_idx = 0;
    sdl_player_exchange_cancel_press();

    if (was_active)
        g_state.need_present = true;
}

bool sdl_player_exchange_context_active(void)
{
    return g_player_exchange_target.active
        && character_generated
        && character_dungeon
        && p_ptr
        && character_icky == 0
        && !ui_menu_click_is_active()
        && g_views[PANE_MAIN].term_ready;
}

bool sdl_player_exchange_begin(bool report_no_target)
{
    if (!p_ptr || !p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
        return false;
    if (!sdl_player_exchange_has_any_target()) {
        if (report_no_target)
            bell("No adjacent exchange target.");
        return false;
    }

    sdl_player_action_menu_cancel();
    sdl_mouse_path_cancel();
    memset(&g_player_exchange_target, 0, sizeof(g_player_exchange_target));
    g_gamepad_state.dpad_up = false;
    g_gamepad_state.dpad_down = false;
    g_gamepad_state.dpad_left = false;
    g_gamepad_state.dpad_right = false;
    g_gamepad_state.dpad_dir = 0;
    sdl_gamepad_clear_pending_dpad();
    sdl_gamepad_clear_pending_left_stick();
    g_player_exchange_target.active = true;
    sdl_player_exchange_select_default();
    g_state.need_present = true;
    return true;
}

void sdl_player_exchange_begin_direction_prompt(void)
{
    (void)sdl_player_exchange_begin(false);
}

void sdl_player_exchange_cancel_direction_prompt(void)
{
    sdl_player_exchange_cancel();
}

void sdl_player_exchange_update_hover(float x, float y)
{
    int map_y = 0;
    int map_x = 0;
    int m_idx = 0;
    bool hover_active = false;

    if (sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        && sdl_player_exchange_target_valid(map_y, map_x, &m_idx))
    {
        hover_active = true;
    }

    if (g_player_exchange_target.hover_active != hover_active
        || g_player_exchange_target.hover_y != map_y
        || g_player_exchange_target.hover_x != map_x
        || g_player_exchange_target.hover_m_idx != m_idx)
    {
        g_player_exchange_target.hover_active = hover_active;
        g_player_exchange_target.hover_y = hover_active ? map_y : 0;
        g_player_exchange_target.hover_x = hover_active ? map_x : 0;
        g_player_exchange_target.hover_m_idx = hover_active ? m_idx : 0;
        g_state.need_present = true;
    }
}

bool sdl_player_exchange_same_selection(int y, int x, int m_idx)
{
    return g_player_exchange_target.selected
        && g_player_exchange_target.selected_y == y
        && g_player_exchange_target.selected_x == x
        && g_player_exchange_target.selected_m_idx == m_idx;
}

void sdl_player_exchange_execute(int y, int x)
{
    int dir = motion_dir(p_ptr->py, p_ptr->px, y, x);

    if (dir == 5)
        return;

    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    if (!p_ptr || p_ptr->command_cmd != 'X')
        sdl_enqueue_bypassed_command('X');
    Term_keypress('0' + dir);
}

void sdl_player_exchange_select_or_execute(int y, int x, int m_idx)
{
    if (sdl_player_exchange_same_selection(y, x, m_idx)) {
        sdl_player_exchange_execute(y, x);
        return;
    }

    g_player_exchange_target.selected = true;
    g_player_exchange_target.selected_y = y;
    g_player_exchange_target.selected_x = x;
    g_player_exchange_target.selected_m_idx = m_idx;
    g_state.need_present = true;
}

void sdl_player_exchange_move_hover(int delta)
{
    player_exchange_target_entry entries[SDL_PLAYER_EXCHANGE_MAX_TARGETS];
    int count = sdl_player_exchange_collect_targets(entries,
        SDL_PLAYER_EXCHANGE_MAX_TARGETS);
    int index = -1;

    if (count <= 0)
        return;

    if (g_player_exchange_target.hover_active) {
        index = sdl_player_exchange_target_index(entries, count,
            g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x,
            g_player_exchange_target.hover_m_idx);
    }
    if (index < 0 && g_player_exchange_target.selected) {
        index = sdl_player_exchange_target_index(entries, count,
            g_player_exchange_target.selected_y,
            g_player_exchange_target.selected_x,
            g_player_exchange_target.selected_m_idx);
    }

    if (index < 0) {
        index = (delta < 0) ? count - 1 : 0;
    } else {
        index = (index + delta) % count;
        if (index < 0)
            index += count;
    }

    sdl_player_exchange_set_hover(&entries[index]);
}

void sdl_player_exchange_activate_hover(void)
{
    int m_idx = 0;

    if (g_player_exchange_target.hover_active
        && sdl_player_exchange_target_valid(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x, &m_idx))
    {
        sdl_player_exchange_execute(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x);
        return;
    }

    if (g_player_exchange_target.selected
        && sdl_player_exchange_target_valid(g_player_exchange_target.selected_y,
            g_player_exchange_target.selected_x, &m_idx))
    {
        sdl_player_exchange_execute(g_player_exchange_target.selected_y,
            g_player_exchange_target.selected_x);
        return;
    }

    sdl_player_exchange_select_default();
    if (g_player_exchange_target.hover_active
        && sdl_player_exchange_target_valid(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x, &m_idx))
    {
        sdl_player_exchange_execute(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x);
        return;
    }

    bell("Choose an adjacent monster.");
}

void sdl_player_action_menu_activate_kind(int kind, bool secondary)
{
    int command = 0;
    bool select_floor = false;

    switch (kind) {
    case SDL_PLAYER_ACTION_EXCHANGE:
        (void)sdl_player_exchange_begin(true);
        return;
    case SDL_PLAYER_ACTION_WAIT:
        command = secondary ? 'Z' : 'z';
        break;
    case SDL_PLAYER_ACTION_USE:
        command = 'u';
        select_floor = !secondary && sdl_player_has_floor_item_underfoot();
        break;
    case SDL_PLAYER_ACTION_STEALTH:
        command = 'S';
        break;
    case SDL_PLAYER_ACTION_SING:
        command = 's';
        break;
    case SDL_PLAYER_ACTION_FLETCH:
        command = '-';
        break;
    case SDL_PLAYER_ACTION_EXAMINE:
        command = 'x';
        select_floor = !secondary && sdl_player_has_floor_item_underfoot();
        break;
    case SDL_PLAYER_ACTION_ACTIVATE:
        command = 'a';
        break;
    case SDL_PLAYER_ACTION_HORN:
        command = 'p';
        break;
    default:
        return;
    }

    sdl_player_action_menu_cancel();
    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    sdl_enqueue_bypassed_command(command);
    if (select_floor)
        Term_keypress('-');
}

bool sdl_player_action_menu_open(void)
{
    if (!sdl_main_screen_click_shortcuts_active())
        return false;

    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    sdl_player_action_menu_cancel_press();
    g_gamepad_state.dpad_up = false;
    g_gamepad_state.dpad_down = false;
    g_gamepad_state.dpad_left = false;
    g_gamepad_state.dpad_right = false;
    g_gamepad_state.dpad_dir = 0;
    sdl_gamepad_clear_pending_dpad();
    sdl_gamepad_clear_pending_left_stick();
    g_player_action_menu.active = true;
    g_player_action_menu.hover_kind = SDL_PLAYER_ACTION_NONE;
    g_state.need_present = true;
    return true;
}

bool sdl_player_action_menu_handle_gamepad_button(
    SDL_GamepadButton button, bool down)
{
    if (!g_player_action_menu.active)
        return false;

    if (!sdl_main_screen_click_shortcuts_active()) {
        sdl_player_action_menu_cancel();
        return true;
    }

    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        if (down) {
            if (g_player_action_menu.press_active
                && g_player_action_menu.press_gamepad)
            {
                sdl_player_action_menu_cancel_press();
            }
            sdl_player_action_menu_move_hover(-1);
        }
        return true;

    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        if (down) {
            if (g_player_action_menu.press_active
                && g_player_action_menu.press_gamepad)
            {
                sdl_player_action_menu_cancel_press();
            }
            sdl_player_action_menu_move_hover(1);
        }
        return true;

    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_START:
    case SDL_GAMEPAD_BUTTON_BACK:
        if (down)
            sdl_player_action_menu_cancel();
        return true;

    default:
        break;
    }

    if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT
        && sdl_gamepad_action_is_confirm(config.gamepad_button_bindings[button]))
    {
        return sdl_player_action_menu_handle_gamepad_confirm(
            (int)button, down);
    }

    return false;
}

bool sdl_player_exchange_handle_gamepad_button(
    SDL_GamepadButton button, bool down)
{
    if (!g_player_exchange_target.active)
        return false;

    if (!sdl_player_exchange_context_active()) {
        sdl_player_exchange_cancel();
        return true;
    }

    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        if (down)
            sdl_player_exchange_move_hover(-1);
        return true;

    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        if (down)
            sdl_player_exchange_move_hover(1);
        return true;

    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_START:
    case SDL_GAMEPAD_BUTTON_BACK:
        if (down)
            sdl_player_exchange_cancel();
        return true;

    default:
        break;
    }

    if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT
        && sdl_gamepad_action_is_confirm(config.gamepad_button_bindings[button]))
    {
        if (down)
            sdl_player_exchange_activate_hover();
        return true;
    }

    return false;
}

bool sdl_player_action_menu_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse, bool secondary)
{
    int kind;

    if (!g_player_action_menu.active)
        return false;
    if (!sdl_main_screen_click_shortcuts_active()) {
        sdl_player_action_menu_cancel();
        return true;
    }

    kind = sdl_player_action_menu_kind_at(x, y);
    if (kind == SDL_PLAYER_ACTION_NONE) {
        sdl_player_action_menu_cancel();
        return true;
    }
    if (secondary && !sdl_player_action_menu_kind_supports_secondary(kind)) {
        sdl_player_action_menu_cancel();
        return true;
    }

    sdl_player_action_menu_cancel_press();
    g_player_action_menu.press_active = true;
    g_player_action_menu.press_mouse = mouse;
    g_player_action_menu.press_secondary = secondary;
    g_player_action_menu.press_finger_id = finger_id;
    g_player_action_menu.press_kind = kind;
    g_player_action_menu.press_start_x = x;
    g_player_action_menu.press_start_y = y;
    g_player_action_menu.press_start_time = SDL_GetTicksNS();
    g_player_action_menu.hover_kind = kind;
    g_state.need_present = true;
    return true;
}

bool sdl_player_action_menu_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int kind;
    float dx;
    float dy;

    if (!g_player_action_menu.active)
        return false;
    if (!sdl_main_screen_click_shortcuts_active()) {
        sdl_player_action_menu_cancel();
        return true;
    }

    kind = sdl_player_action_menu_kind_at(x, y);
    if (g_player_action_menu.hover_kind != kind) {
        g_player_action_menu.hover_kind = kind;
        g_state.need_present = true;
    }

    if (!g_player_action_menu.press_active)
        return true;
    if (g_player_action_menu.press_gamepad)
        return true;
    if (g_player_action_menu.press_mouse != mouse
        || g_player_action_menu.press_finger_id != finger_id)
    {
        return true;
    }

    dx = x - g_player_action_menu.press_start_x;
    dy = y - g_player_action_menu.press_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (kind != g_player_action_menu.press_kind
        || dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        sdl_player_action_menu_cancel_press();
    }

    return true;
}

bool sdl_player_action_menu_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse, bool secondary)
{
    int kind;
    bool activate_secondary;

    if (!g_player_action_menu.active)
        return false;
    if (!sdl_main_screen_click_shortcuts_active()) {
        sdl_player_action_menu_cancel();
        return true;
    }
    if (!g_player_action_menu.press_active)
        return true;
    if (g_player_action_menu.press_gamepad
        || g_player_action_menu.press_mouse != mouse
        || g_player_action_menu.press_secondary != secondary
        || g_player_action_menu.press_finger_id != finger_id)
    {
        return true;
    }

    kind = sdl_player_action_menu_kind_at(x, y);
    if (kind == g_player_action_menu.press_kind) {
        activate_secondary = g_player_action_menu.press_secondary;
        if (!mouse && sdl_player_action_menu_kind_supports_secondary(kind)
            && g_player_action_menu.press_start_time
            && SDL_GetTicksNS() - g_player_action_menu.press_start_time
                >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        {
            activate_secondary = true;
        }
        sdl_player_action_menu_activate_kind(kind, activate_secondary);
    } else {
        sdl_player_action_menu_cancel_press();
    }

    return true;
}

int sdl_player_action_menu_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_player_action_menu.active)
        return -1;
    if (!g_player_action_menu.press_active
        || g_player_action_menu.press_mouse
        || g_player_action_menu.press_secondary
        || !sdl_player_action_menu_kind_supports_secondary(
            g_player_action_menu.press_kind)
        || !g_player_action_menu.press_start_time)
    {
        return -1;
    }

    elapsed = now_ns - g_player_action_menu.press_start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_player_action_menu_flush_pending_press(Uint64 now_ns)
{
    int kind;

    if (sdl_player_action_menu_pending_timeout_ms(now_ns) != 0)
        return false;

    kind = g_player_action_menu.press_kind;
    sdl_player_action_menu_activate_kind(kind, true);
    return true;
}

bool sdl_player_exchange_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int map_y = 0;
    int map_x = 0;

    if (!g_player_exchange_target.active)
        return false;
    if (!sdl_player_exchange_context_active()) {
        sdl_player_exchange_cancel();
        return true;
    }

    sdl_player_exchange_update_hover(x, y);
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)) {
        sdl_player_exchange_cancel();
        return true;
    }

    sdl_player_exchange_cancel_press();
    g_player_exchange_target.press_active = true;
    g_player_exchange_target.press_mouse = mouse;
    g_player_exchange_target.press_finger_id = finger_id;
    g_player_exchange_target.press_y = map_y;
    g_player_exchange_target.press_x = map_x;
    g_player_exchange_target.press_start_x = x;
    g_player_exchange_target.press_start_y = y;
    return true;
}

bool sdl_player_exchange_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int map_y = 0;
    int map_x = 0;
    float dx;
    float dy;

    if (!g_player_exchange_target.active)
        return false;
    if (!sdl_player_exchange_context_active()) {
        sdl_player_exchange_cancel();
        return true;
    }

    sdl_player_exchange_update_hover(x, y);

    if (!g_player_exchange_target.press_active)
        return true;
    if (g_player_exchange_target.press_mouse != mouse
        || g_player_exchange_target.press_finger_id != finger_id)
    {
        return true;
    }

    dx = x - g_player_exchange_target.press_start_x;
    dy = y - g_player_exchange_target.press_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        || map_y != g_player_exchange_target.press_y
        || map_x != g_player_exchange_target.press_x
        || dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        sdl_player_exchange_cancel_press();
    }

    return true;
}

bool sdl_player_exchange_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int map_y = 0;
    int map_x = 0;
    int m_idx = 0;

    if (!g_player_exchange_target.active)
        return false;
    if (!sdl_player_exchange_context_active()) {
        sdl_player_exchange_cancel();
        return true;
    }
    if (!g_player_exchange_target.press_active)
        return true;
    if (g_player_exchange_target.press_mouse != mouse
        || g_player_exchange_target.press_finger_id != finger_id)
    {
        return true;
    }

    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        || map_y != g_player_exchange_target.press_y
        || map_x != g_player_exchange_target.press_x)
    {
        sdl_player_exchange_cancel_press();
        return true;
    }

    sdl_player_exchange_cancel_press();
    if (sdl_player_exchange_target_valid(map_y, map_x, &m_idx)) {
        if (mouse)
            sdl_player_exchange_execute(map_y, map_x);
        else
            sdl_player_exchange_select_or_execute(map_y, map_x, m_idx);
    } else {
        bell("Choose an adjacent monster.");
    }

    return true;
}

void sdl_player_exchange_render_cell(int y, int x, SDL_Color color,
    int alpha, bool thick)
{
    SDL_FRect rect;

    if (!sdl_player_map_rect(y, x, &rect))
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, alpha);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        thick ? 235 : 170);
    SDL_RenderRect(g_state.renderer, &rect);
    if (thick) {
        rect.x += 1.0f;
        rect.y += 1.0f;
        rect.w -= 2.0f;
        rect.h -= 2.0f;
        if (rect.w > 1.0f && rect.h > 1.0f)
            SDL_RenderRect(g_state.renderer, &rect);
    }
}

void sdl_player_exchange_render(void)
{
    SDL_Color available = g_state.palette[TERM_L_BLUE];
    SDL_Color hover = g_state.palette[TERM_L_RED];
    SDL_Color selected = g_state.palette[TERM_YELLOW];

    if (!g_player_exchange_target.active)
        return;
    if (!sdl_player_exchange_context_active())
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    for (int d = 1; d <= 9; d++) {
        int y;
        int x;

        if (d == 5)
            continue;
        y = p_ptr->py + ddy[d];
        x = p_ptr->px + ddx[d];
        if (!sdl_player_exchange_target_valid(y, x, NULL))
            continue;

        sdl_player_exchange_render_cell(y, x, available, 54, false);
    }

    if (g_player_exchange_target.selected) {
        sdl_player_exchange_render_cell(g_player_exchange_target.selected_y,
            g_player_exchange_target.selected_x, selected, 96, true);
    }
    if (g_player_exchange_target.hover_active) {
        sdl_player_exchange_render_cell(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x, hover, 82, true);
    }
}

void sdl_player_action_menu_render(void)
{
    player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = 0;
    SDL_Rect clip;
    SDL_Color bg = { 16, 20, 22, 226 };
    SDL_Color hover_bg = { 48, 54, 58, 236 };
    SDL_Color border = g_state.palette[TERM_L_BLUE];
    SDL_Color hover_border = g_state.palette[TERM_YELLOW];
    SDL_Color text = g_state.palette[TERM_WHITE];

    if (!g_player_action_menu.active)
        return;
    if (!sdl_main_screen_click_shortcuts_active())
        return;
    if (!sdl_player_action_menu_layout(entries, &count))
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    clip = g_views[PANE_MAIN].rect;
    SDL_SetRenderClipRect(g_state.renderer, &clip);

    for (int i = 0; i < count; i++) {
        SDL_FRect shadow = entries[i].rect;
        bool hover = entries[i].kind == g_player_action_menu.hover_kind;
        SDL_Color fill = hover ? hover_bg : bg;
        SDL_Color line = hover ? hover_border : border;

        shadow.x += 2.0f;
        shadow.y += 2.0f;
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 112);
        SDL_RenderFillRect(g_state.renderer, &shadow);

        SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g, fill.b,
            fill.a);
        SDL_RenderFillRect(g_state.renderer, &entries[i].rect);
        SDL_SetRenderDrawColor(g_state.renderer, line.r, line.g, line.b,
            hover ? 238 : 176);
        SDL_RenderRect(g_state.renderer, &entries[i].rect);

        sdl_touch_pane_draw_button_text_scaled(&entries[i].rect, NULL,
            entries[i].label, text, 0.36f, 0.48f);
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
}


