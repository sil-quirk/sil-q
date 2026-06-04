#include "angband.h"
#include "sdl/main-sdl-private.h"

bool sdl_pointer_attack_mode_is_ranged(int mode)
{
    return mode == SDL_POINTER_ATTACK_RANGED_1
        || mode == SDL_POINTER_ATTACK_RANGED_2;
}

bool sdl_pointer_attack_manual_modifier_active(void)
{
    return ((SDL_GetModState() & SDL_KMOD_CTRL) != 0)
        || sdl_gamepad_ctrl_active();
}

bool sdl_pointer_attack_input_context_active(void)
{
    return sdl_main_screen_click_shortcuts_active()
        && (!p_ptr || p_ptr->command_cmd == 0);
}

bool sdl_pointer_attack_mode_active(void)
{
    return sdl_pointer_attack_current_mode() != SDL_POINTER_ATTACK_NONE
        && sdl_pointer_attack_input_context_active();
}

void sdl_pointer_attack_clear_hover(void)
{
    if (!g_pointer_attack.hover_visible && !g_pointer_attack.hover_valid)
        return;

    g_pointer_attack.hover_visible = false;
    g_pointer_attack.hover_valid = false;
    g_pointer_attack.hover_actionable = false;
    g_pointer_attack.hover_manual = false;
    g_pointer_attack.hover_kind = SDL_POINTER_ATTACK_TARGET_NONE;
    g_pointer_attack.hover_y = 0;
    g_pointer_attack.hover_x = 0;
    g_pointer_attack.hover_m_idx = 0;
    g_state.need_present = true;
}

void sdl_pointer_attack_clear_touch_selection(void)
{
    if (!g_pointer_attack.touch_selected)
        return;

    g_pointer_attack.touch_selected = false;
    g_pointer_attack.touch_selected_manual = false;
    g_pointer_attack.touch_selected_mode = SDL_POINTER_ATTACK_NONE;
    g_pointer_attack.touch_selected_kind = SDL_POINTER_ATTACK_TARGET_NONE;
    g_pointer_attack.touch_selected_y = 0;
    g_pointer_attack.touch_selected_x = 0;
    g_pointer_attack.touch_selected_m_idx = 0;
    g_state.need_present = true;
}

void sdl_pointer_attack_cancel_touch_press(void)
{
    if (!g_pointer_attack.touch_press_active)
        return;

    g_pointer_attack.touch_press_active = false;
    g_pointer_attack.touch_repeat_target = false;
    g_pointer_attack.touch_finger_id = 0;
    g_pointer_attack.touch_press_y = 0;
    g_pointer_attack.touch_press_x = 0;
    g_pointer_attack.touch_start_x = 0.0f;
    g_pointer_attack.touch_start_y = 0.0f;
    g_pointer_attack.touch_start_time = 0;
}

void sdl_pointer_attack_clear_pending(void)
{
    g_pointer_attack.pending = false;
    g_pointer_attack.pending_kind = SDL_POINTER_ATTACK_TARGET_NONE;
    g_pointer_attack.pending_mode = SDL_POINTER_ATTACK_NONE;
    g_pointer_attack.pending_m_idx = 0;
    g_pointer_attack.pending_y = 0;
    g_pointer_attack.pending_x = 0;
    g_pointer_attack.pending_wake = false;
}

int sdl_pointer_attack_binding_mode(int binding)
{
    switch (binding) {
    case '/':
    case ';':
        return SDL_POINTER_ATTACK_MELEE;
    default:
        return SDL_POINTER_ATTACK_NONE;
    }
}

bool sdl_pointer_attack_binding_toggled(int binding)
{
    int mode = sdl_pointer_attack_binding_mode(binding);

    return mode != SDL_POINTER_ATTACK_NONE
        && sdl_pointer_attack_current_mode() == mode;
}

int sdl_pointer_attack_current_mode(void)
{
    return (g_pointer_attack.mode == SDL_POINTER_ATTACK_NONE)
        ? SDL_POINTER_ATTACK_MELEE
        : g_pointer_attack.mode;
}

int sdl_pointer_attack_panel_display_mode(void)
{
    return (g_pointer_attack.panel_hover_mode != SDL_POINTER_ATTACK_NONE)
        ? g_pointer_attack.panel_hover_mode
        : sdl_pointer_attack_current_mode();
}

bool sdl_pointer_attack_panel_mode_highlighted(int mode)
{
    return mode != SDL_POINTER_ATTACK_NONE
        && sdl_pointer_attack_panel_display_mode() == mode;
}

const char* sdl_pointer_attack_mode_name(int mode)
{
    switch (mode) {
    case SDL_POINTER_ATTACK_MELEE:
        return "Melee";
    case SDL_POINTER_ATTACK_RANGED_1:
        return "Ranged";
    case SDL_POINTER_ATTACK_RANGED_2:
        return "Ranged 2";
    default:
        return "Pointer attack";
    }
}

void sdl_pointer_attack_refresh_mode_display(bool redraw_map)
{
    if (p_ptr && character_generated) {
        p_ptr->redraw |= PR_BASIC;
        if (redraw_map)
            p_ptr->redraw |= PR_MAP;
        handle_stuff();
        if (Term)
            Term_fresh();
    }

    g_state.need_present = true;
}

void sdl_pointer_attack_reset_to_melee(void)
{
    g_pointer_attack.mode = SDL_POINTER_ATTACK_MELEE;
    g_pointer_attack.panel_hover_mode = SDL_POINTER_ATTACK_NONE;
    sdl_pointer_attack_clear_pending();
    sdl_pointer_attack_clear_hover();
    sdl_pointer_attack_clear_touch_selection();
    sdl_pointer_attack_cancel_touch_press();
    sdl_mouse_path_cancel();
    sdl_pointer_attack_refresh_mode_display(true);
}

void sdl_pointer_attack_set_mode(int mode)
{
    if (mode == SDL_POINTER_ATTACK_NONE)
        mode = SDL_POINTER_ATTACK_MELEE;

    if (g_pointer_attack.mode == mode) {
        g_pointer_attack.panel_hover_mode = SDL_POINTER_ATTACK_NONE;
        sdl_pointer_attack_refresh_mode_display(false);
        return;
    }

    g_pointer_attack.mode = mode;
    g_pointer_attack.panel_hover_mode = SDL_POINTER_ATTACK_NONE;
    sdl_pointer_attack_clear_pending();
    sdl_pointer_attack_clear_hover();
    sdl_pointer_attack_clear_touch_selection();
    sdl_pointer_attack_cancel_touch_press();
    sdl_mouse_path_cancel();

    msg_format("%s pointer mode.", sdl_pointer_attack_mode_name(mode));
    sdl_pointer_attack_refresh_mode_display(true);
}

void sdl_pointer_attack_set_panel_hover_mode(int mode)
{
    if (mode == SDL_POINTER_ATTACK_NONE)
    {
        if (g_pointer_attack.panel_hover_mode == SDL_POINTER_ATTACK_NONE)
            return;
        g_pointer_attack.panel_hover_mode = SDL_POINTER_ATTACK_NONE;
        sdl_pointer_attack_refresh_mode_display(false);
        return;
    }

    if (!sdl_pointer_attack_input_context_active())
    {
        if (g_pointer_attack.panel_hover_mode != SDL_POINTER_ATTACK_NONE)
        {
            g_pointer_attack.panel_hover_mode = SDL_POINTER_ATTACK_NONE;
            sdl_pointer_attack_refresh_mode_display(false);
        }
        return;
    }

    if (g_pointer_attack.panel_hover_mode == mode)
        return;

    g_pointer_attack.panel_hover_mode = mode;
    sdl_pointer_attack_refresh_mode_display(false);
}

bool sdl_pointer_attack_toggle_binding(int binding)
{
    int mode = sdl_pointer_attack_binding_mode(binding);

    if (mode == SDL_POINTER_ATTACK_NONE)
        return false;
    if (!sdl_pointer_attack_input_context_active())
        return false;

    sdl_pointer_attack_set_mode(mode);
    return true;
}

int sdl_pointer_attack_ranged_range(int mode)
{
    object_type* ammo;
    object_type* bow;
    u32b f1 = 0;
    u32b f2 = 0;
    u32b f3 = 0;
    u32b f4 = 0;

    if (!sdl_pointer_attack_mode_is_ranged(mode))
        return 0;

    ammo = &inventory[(mode == SDL_POINTER_ATTACK_RANGED_2)
        ? INVEN_QUIVER2 : INVEN_QUIVER1];
    if (!ammo->k_idx)
        return 0;

    object_flags4(ammo, &f1, &f2, &f3, &f4);
    if (player_can_treat_as_throwing_flags(ammo, f3))
        return throwing_range(ammo);

    bow = &inventory[INVEN_BOW];
    if (!bow->tval || !p_ptr->ammo_tval)
        return 0;

    return archery_range(bow);
}

bool sdl_pointer_attack_adjacent_dir_for_grid(int y, int x,
    int* out_dir)
{
    int dir;

    if (out_dir)
        *out_dir = 0;
    if (!p_ptr)
        return false;

    dir = motion_dir(p_ptr->py, p_ptr->px, y, x);
    if (dir < 1 || dir > 9 || dir == 5)
        return false;
    if ((y != p_ptr->py + ddy[dir]) || (x != p_ptr->px + ddx[dir]))
        return false;

    if (out_dir)
        *out_dir = dir;
    return true;
}

bool sdl_pointer_attack_adjacent_tunnel_target(int y, int x,
    int* out_dir)
{
    int dir = 0;

    if (out_dir)
        *out_dir = 0;
    if (!in_bounds(y, x))
        return false;
    if (!sdl_pointer_attack_adjacent_dir_for_grid(y, x, &dir))
        return false;
    if (!cave_wall_bold(y, x) && cave_feat[y][x] != FEAT_RUBBLE)
        return false;

    if (out_dir)
        *out_dir = dir;
    return true;
}

bool sdl_pointer_attack_manual_location_valid(int mode, int y, int x)
{
    int range;
    int ty;
    int tx;
    int path_n;
    u16b path[MAX_RANGE];

    if (!sdl_pointer_attack_mode_is_ranged(mode))
        return false;
    if (!p_ptr || !in_bounds_fully(y, x))
        return false;
    if (y == p_ptr->py && x == p_ptr->px)
        return false;

    range = sdl_pointer_attack_ranged_range(mode);
    if (range <= 0)
        return false;
    if (distance(p_ptr->py, p_ptr->px, y, x) > range)
        return false;
    if (!(cave_info[y][x] & (CAVE_FIRE | CAVE_WALL)))
        return false;

    ty = y;
    tx = x;
    path_n = project_path(path, range, p_ptr->py, p_ptr->px, &ty, &tx,
        PROJECT_THRU | PROJECT_INVISIPASS);
    if (path_n == 0)
        return true;

    ty = GRID_Y(path[path_n - 1]);
    tx = GRID_X(path[path_n - 1]);
    return ((((ty <= y) && (y <= p_ptr->py))
                || ((ty >= y) && (y >= p_ptr->py)))
            && (((tx <= x) && (x <= p_ptr->px))
                || ((tx >= x) && (x >= p_ptr->px))));
}

int sdl_pointer_attack_target_kind(int mode, bool manual, int y, int x,
    int* out_m_idx)
{
    int m_idx;
    monster_type* m_ptr;
    monster_race* r_ptr;

    if (out_m_idx)
        *out_m_idx = 0;
    if (mode != SDL_POINTER_ATTACK_MELEE
        && !sdl_pointer_attack_mode_is_ranged(mode))
        return SDL_POINTER_ATTACK_TARGET_NONE;
    if (!sdl_mouse_gameplay_context_active())
        return SDL_POINTER_ATTACK_TARGET_NONE;
    if (!in_bounds(y, x))
        return SDL_POINTER_ATTACK_TARGET_NONE;

    if (manual) {
        (void)sdl_mouse_grid_has_visible_monster(y, x, out_m_idx);
        if (sdl_pointer_attack_adjacent_tunnel_target(y, x, NULL))
            return SDL_POINTER_ATTACK_TARGET_ALTER;
        if (mode == SDL_POINTER_ATTACK_MELEE)
            return SDL_POINTER_ATTACK_TARGET_ALTER;
        if (sdl_pointer_attack_mode_is_ranged(mode))
            return SDL_POINTER_ATTACK_TARGET_LOCATION;
        return SDL_POINTER_ATTACK_TARGET_NONE;
    }

    if (!grid_info_is_available(y, x))
        return SDL_POINTER_ATTACK_TARGET_NONE;

    m_idx = cave_m_idx[y][x];
    if (m_idx <= 0)
        return SDL_POINTER_ATTACK_TARGET_NONE;

    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx || !m_ptr->ml)
        return SDL_POINTER_ATTACK_TARGET_NONE;

    r_ptr = &r_info[m_ptr->r_idx];
    if (r_ptr->flags1 & (RF1_PEACEFUL))
        return SDL_POINTER_ATTACK_TARGET_NONE;

    if (out_m_idx)
        *out_m_idx = m_idx;
    return SDL_POINTER_ATTACK_TARGET_MONSTER;
}

bool sdl_pointer_attack_target_hoverable(int mode, bool manual, int y,
    int x, int* out_m_idx, int* out_kind)
{
    int kind;

    if (out_kind)
        *out_kind = SDL_POINTER_ATTACK_TARGET_NONE;

    kind = sdl_pointer_attack_target_kind(mode, manual, y, x, out_m_idx);
    if (kind == SDL_POINTER_ATTACK_TARGET_NONE)
        return false;

    if (out_kind)
        *out_kind = kind;
    return true;
}

bool sdl_pointer_attack_target_actionable(int mode, bool manual, int y,
    int x, int* out_m_idx)
{
    int m_idx = 0;
    int kind;

    if (out_m_idx)
        *out_m_idx = 0;

    kind = sdl_pointer_attack_target_kind(mode, manual, y, x, &m_idx);
    if (kind == SDL_POINTER_ATTACK_TARGET_NONE)
        return false;

    if (kind == SDL_POINTER_ATTACK_TARGET_ALTER) {
        if (!sdl_pointer_attack_adjacent_dir_for_grid(y, x, NULL))
            return false;
    } else if (kind == SDL_POINTER_ATTACK_TARGET_LOCATION) {
        if (!sdl_pointer_attack_manual_location_valid(mode, y, x))
            return false;
    } else if (mode == SDL_POINTER_ATTACK_MELEE) {
        if (distance(p_ptr->py, p_ptr->px, y, x) != 1)
            return false;
    } else if (sdl_pointer_attack_mode_is_ranged(mode)) {
        int range = sdl_pointer_attack_ranged_range(mode);

        if (range <= 0)
            return false;
        if (distance(p_ptr->py, p_ptr->px, y, x) > range)
            return false;
        if (!target_able(m_idx))
            return false;
    } else {
        return false;
    }

    if (out_m_idx)
        *out_m_idx = m_idx;
    return true;
}

cptr sdl_pointer_attack_blocked_message(int mode, int kind)
{
    if (kind == SDL_POINTER_ATTACK_TARGET_LOCATION
        || sdl_pointer_attack_mode_is_ranged(mode))
    {
        return "No clear shot.";
    }
    if (kind == SDL_POINTER_ATTACK_TARGET_ALTER)
        return "No adjacent action.";
    return "No adjacent melee target.";
}

bool sdl_pointer_attack_update_hover_grid(int map_y, int map_x,
    bool manual)
{
    int m_idx = 0;
    int kind = SDL_POINTER_ATTACK_TARGET_NONE;
    bool hoverable = sdl_pointer_attack_target_hoverable(g_pointer_attack.mode,
        manual, map_y, map_x, &m_idx, &kind);
    bool actionable = hoverable
        && sdl_pointer_attack_target_actionable(g_pointer_attack.mode,
            manual, map_y, map_x, NULL);
    bool changed = g_pointer_attack.hover_visible != hoverable
        || g_pointer_attack.hover_valid != hoverable
        || g_pointer_attack.hover_actionable != actionable
        || g_pointer_attack.hover_manual != manual
        || g_pointer_attack.hover_kind != kind
        || g_pointer_attack.hover_y != map_y
        || g_pointer_attack.hover_x != map_x
        || g_pointer_attack.hover_m_idx != m_idx;

    g_pointer_attack.hover_visible = hoverable;
    g_pointer_attack.hover_valid = hoverable;
    g_pointer_attack.hover_actionable = actionable;
    g_pointer_attack.hover_manual = manual;
    g_pointer_attack.hover_kind = hoverable ? kind : SDL_POINTER_ATTACK_TARGET_NONE;
    g_pointer_attack.hover_y = hoverable ? map_y : 0;
    g_pointer_attack.hover_x = hoverable ? map_x : 0;
    g_pointer_attack.hover_m_idx = hoverable ? m_idx : 0;

    if (changed)
        g_state.need_present = true;

    return hoverable;
}

bool sdl_pointer_attack_queue_target(int mode, bool manual, int map_y,
    int map_x)
{
    int m_idx = 0;
    int kind = SDL_POINTER_ATTACK_TARGET_NONE;

    if (!sdl_pointer_attack_target_hoverable(mode, manual, map_y, map_x,
            &m_idx, &kind))
    {
        return false;
    }

    if (m_idx > 0)
        health_track(m_idx);
    if (!sdl_pointer_attack_target_actionable(mode, manual, map_y, map_x,
            &m_idx))
    {
        bell(sdl_pointer_attack_blocked_message(mode, kind));
        return false;
    }

    if (kind == SDL_POINTER_ATTACK_TARGET_MONSTER
        && sdl_pointer_attack_mode_is_ranged(mode))
    {
        target_set_monster(m_idx);
    }

    g_pointer_attack.pending = true;
    g_pointer_attack.pending_kind = kind;
    g_pointer_attack.pending_mode = mode;
    g_pointer_attack.pending_m_idx = m_idx;
    g_pointer_attack.pending_y = map_y;
    g_pointer_attack.pending_x = map_x;
    g_pointer_attack.pending_wake = true;
    g_state.need_present = true;
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_pointer_attack_handle_motion(float x, float y)
{
    int map_y = 0;
    int map_x = 0;
    bool manual;
    bool hoverable;

    if (!sdl_pointer_attack_mode_active())
        return false;

    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)) {
        sdl_pointer_attack_clear_hover();
        return false;
    }

    manual = sdl_pointer_attack_manual_modifier_active();
    hoverable = sdl_pointer_attack_update_hover_grid(map_y, map_x, manual);
    if ((manual || hoverable) && !g_mouse_path.follow_active)
        sdl_mouse_path_cancel();

    return manual || hoverable;
}

bool sdl_pointer_attack_handle_left_click(float x, float y)
{
    int map_y = 0;
    int map_x = 0;
    bool manual;
    bool hoverable;

    if (!sdl_pointer_attack_mode_active())
        return false;
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x))
        return false;

    manual = sdl_pointer_attack_manual_modifier_active();
    hoverable = sdl_pointer_attack_update_hover_grid(map_y, map_x, manual);
    if (!hoverable) {
        sdl_pointer_attack_clear_touch_selection();
        return manual;
    }
    if (!g_mouse_path.follow_active)
        sdl_mouse_path_cancel();

    (void)sdl_pointer_attack_queue_target(g_pointer_attack.mode, manual, map_y,
        map_x);
    return true;
}

bool sdl_pointer_attack_touch_same_selected_target(int mode,
    bool manual, int map_y, int map_x)
{
    int m_idx = 0;
    int kind = SDL_POINTER_ATTACK_TARGET_NONE;

    if (!g_pointer_attack.touch_selected)
        return false;
    if (g_pointer_attack.touch_selected_mode != mode)
        return false;
    if (g_pointer_attack.touch_selected_manual != manual)
        return false;
    if (g_pointer_attack.touch_selected_y != map_y
        || g_pointer_attack.touch_selected_x != map_x)
    {
        return false;
    }
    if (!sdl_pointer_attack_target_hoverable(mode, manual, map_y, map_x,
            &m_idx, &kind))
    {
        return false;
    }
    if (g_pointer_attack.touch_selected_kind != kind)
        return false;
    if (kind != SDL_POINTER_ATTACK_TARGET_MONSTER)
        return true;

    return g_pointer_attack.touch_selected_m_idx == m_idx;
}

bool sdl_pointer_attack_handle_touch_down(float x, float y,
    SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;
    bool manual;
    bool hoverable;

    if (sdl_touch_round_layer_controls_active()
        && !sdl_touch_round_point_excluded(x, y))
    {
        return false;
    }
    if (!sdl_pointer_attack_mode_active())
        return false;
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x))
        return false;
    (void)sdl_object_tooltip_show_grid(map_y, map_x, true);
    manual = sdl_pointer_attack_manual_modifier_active();
    hoverable = sdl_pointer_attack_update_hover_grid(map_y, map_x, manual);
    if (!hoverable) {
        sdl_pointer_attack_clear_touch_selection();
        return false;
    }

    sdl_pointer_attack_cancel_touch_press();
    g_pointer_attack.touch_press_active = true;
    g_pointer_attack.touch_repeat_target =
        sdl_pointer_attack_touch_same_selected_target(g_pointer_attack.mode,
            manual, map_y, map_x);
    g_pointer_attack.touch_finger_id = finger_id;
    g_pointer_attack.touch_press_y = map_y;
    g_pointer_attack.touch_press_x = map_x;
    g_pointer_attack.touch_start_x = x;
    g_pointer_attack.touch_start_y = y;
    g_pointer_attack.touch_start_time = SDL_GetTicksNS();
    if (!g_mouse_path.follow_active)
        sdl_mouse_path_cancel();
    return true;
}

bool sdl_pointer_attack_handle_touch_motion(float x, float y,
    SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;
    float dx;
    float dy;

    if (!g_pointer_attack.touch_press_active
        || g_pointer_attack.touch_finger_id != finger_id)
    {
        return false;
    }

    dx = x - g_pointer_attack.touch_start_x;
    dy = y - g_pointer_attack.touch_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        || map_y != g_pointer_attack.touch_press_y
        || map_x != g_pointer_attack.touch_press_x
        || dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        float start_x = g_pointer_attack.touch_start_x;
        float start_y = g_pointer_attack.touch_start_y;

        sdl_pointer_attack_cancel_touch_press();
        sdl_pointer_attack_clear_hover();
        if (sdl_touch_swipe_handle_pointer_down(start_x, start_y, finger_id))
            (void)sdl_touch_swipe_handle_pointer_motion(x, y, finger_id);
        return true;
    }

    return true;
}

bool sdl_pointer_attack_handle_touch_up(float x, float y,
    SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;
    int m_idx = 0;
    int kind = SDL_POINTER_ATTACK_TARGET_NONE;
    bool manual;
    bool repeat_target;

    if (!g_pointer_attack.touch_press_active
        || g_pointer_attack.touch_finger_id != finger_id)
    {
        return false;
    }

    repeat_target = g_pointer_attack.touch_repeat_target;
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        || map_y != g_pointer_attack.touch_press_y
        || map_x != g_pointer_attack.touch_press_x)
    {
        sdl_pointer_attack_cancel_touch_press();
        sdl_pointer_attack_clear_hover();
        return true;
    }

    if (SDL_GetTicksNS() - g_pointer_attack.touch_start_time
        >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        (void)sdl_pointer_attack_flush_pending_press(SDL_GetTicksNS());
        return true;
    }

    sdl_pointer_attack_cancel_touch_press();
    manual = sdl_pointer_attack_manual_modifier_active();

    if (repeat_target) {
        (void)sdl_pointer_attack_queue_target(g_pointer_attack.mode, manual,
            map_y, map_x);
        return true;
    }

    if (sdl_pointer_attack_target_hoverable(g_pointer_attack.mode, manual,
            map_y, map_x, &m_idx, &kind))
    {
        g_pointer_attack.touch_selected = true;
        g_pointer_attack.touch_selected_manual = manual;
        g_pointer_attack.touch_selected_mode = g_pointer_attack.mode;
        g_pointer_attack.touch_selected_kind = kind;
        g_pointer_attack.touch_selected_y = map_y;
        g_pointer_attack.touch_selected_x = map_x;
        g_pointer_attack.touch_selected_m_idx = m_idx;
        (void)sdl_pointer_attack_update_hover_grid(map_y, map_x, manual);
    } else {
        sdl_pointer_attack_clear_touch_selection();
        sdl_pointer_attack_clear_hover();
        bell(sdl_pointer_attack_blocked_message(g_pointer_attack.mode, kind));
    }

    g_state.need_present = true;
    return true;
}

int sdl_pointer_attack_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_pointer_attack.touch_press_active)
        return -1;

    elapsed = now_ns - g_pointer_attack.touch_start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_pointer_attack_flush_pending_press(Uint64 now_ns)
{
    float x;
    float y;

    if (!g_pointer_attack.touch_press_active)
        return false;
    if (now_ns - g_pointer_attack.touch_start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    x = g_pointer_attack.touch_start_x;
    y = g_pointer_attack.touch_start_y;
    sdl_pointer_attack_cancel_touch_press();
    sdl_pointer_attack_clear_touch_selection();

    if (!sdl_mouse_recall_handle_right_click(x, y))
        sdl_pointer_attack_clear_hover();

    return true;
}

bool sdl_pointer_attack_take_render_target(int* mode, bool* manual,
    int* kind, int* y, int* x, int* m_idx, bool* actionable)
{
    if (g_pointer_attack.hover_valid) {
        if (mode)
            *mode = g_pointer_attack.mode;
        if (manual)
            *manual = g_pointer_attack.hover_manual;
        if (kind)
            *kind = g_pointer_attack.hover_kind;
        if (y)
            *y = g_pointer_attack.hover_y;
        if (x)
            *x = g_pointer_attack.hover_x;
        if (m_idx)
            *m_idx = g_pointer_attack.hover_m_idx;
        if (actionable)
            *actionable = g_pointer_attack.hover_actionable;
        return true;
    }

    if (g_pointer_attack.touch_selected
        && sdl_pointer_attack_target_hoverable(
            g_pointer_attack.touch_selected_mode,
            g_pointer_attack.touch_selected_manual,
            g_pointer_attack.touch_selected_y,
            g_pointer_attack.touch_selected_x, m_idx, kind))
    {
        if (mode)
            *mode = g_pointer_attack.touch_selected_mode;
        if (manual)
            *manual = g_pointer_attack.touch_selected_manual;
        if (kind && *kind == SDL_POINTER_ATTACK_TARGET_NONE)
            *kind = g_pointer_attack.touch_selected_kind;
        if (y)
            *y = g_pointer_attack.touch_selected_y;
        if (x)
            *x = g_pointer_attack.touch_selected_x;
        if (actionable)
            *actionable = sdl_pointer_attack_target_actionable(
                g_pointer_attack.touch_selected_mode,
                g_pointer_attack.touch_selected_manual,
                g_pointer_attack.touch_selected_y,
                g_pointer_attack.touch_selected_x, NULL);
        return true;
    }

    return false;
}

void sdl_pointer_attack_render_cell(int y, int x, SDL_Color color,
    bool target)
{
    int cell_cols = use_bigtile ? 2 : 1;
    int term_row;
    int term_col;
    SDL_FRect rect;

    if (!panel_contains(y, x))
        return;

    term_row = ROW_MAP + (y - p_ptr->wy);
    term_col = COL_MAP + (x - p_ptr->wx) * cell_cols;
    if (!sdl_main_cell_rect(term_col, term_row, cell_cols, 1, &rect))
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        target ? 96 : 46);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        target ? 225 : 150);
    SDL_RenderRect(g_state.renderer, &rect);
}

void sdl_pointer_attack_render_manual_ranged_overlay(int mode,
    SDL_Color color)
{
    if (!sdl_pointer_attack_mode_is_ranged(mode))
        return;
    if (sdl_pointer_attack_ranged_range(mode) <= 0)
        return;

    for (int y = p_ptr->wy; y < p_ptr->wy + SCREEN_HGT; y++) {
        for (int x = p_ptr->wx; x < p_ptr->wx + SCREEN_WID; x++) {
            if (!sdl_pointer_attack_manual_location_valid(mode, y, x))
                continue;
            sdl_pointer_attack_render_cell(y, x, color, false);
        }
    }
}

void sdl_pointer_attack_render(void)
{
    int mode = SDL_POINTER_ATTACK_NONE;
    bool manual = false;
    int kind = SDL_POINTER_ATTACK_TARGET_NONE;
    int y = 0;
    int x = 0;
    int m_idx = 0;
    bool actionable = false;
    SDL_Color path_color = g_state.palette[TERM_UMBER];
    SDL_Color target_color = g_state.palette[TERM_L_RED];
    SDL_Color blocked_color = g_state.palette[TERM_SLATE];

    if (!sdl_mouse_gameplay_context_active())
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    if (sdl_pointer_attack_manual_modifier_active()
        && sdl_pointer_attack_mode_is_ranged(g_pointer_attack.mode))
    {
        sdl_pointer_attack_render_manual_ranged_overlay(
            g_pointer_attack.mode, target_color);
    }

    if (!sdl_pointer_attack_take_render_target(&mode, &manual, &kind, &y, &x,
        &m_idx, &actionable))
    {
        return;
    }

    (void)m_idx;
    (void)manual;
    if (!actionable) {
        sdl_pointer_attack_render_cell(y, x, blocked_color, true);
    } else if (kind == SDL_POINTER_ATTACK_TARGET_LOCATION
        || (kind == SDL_POINTER_ATTACK_TARGET_MONSTER
            && sdl_pointer_attack_mode_is_ranged(mode)))
    {
        int range = sdl_pointer_attack_ranged_range(mode);
        int ty = y;
        int tx = x;
        u16b path[256];
        int path_n = project_path(path, range, p_ptr->py, p_ptr->px,
            &ty, &tx, PROJECT_THRU | PROJECT_INVISIPASS);

        if (path_n <= 0) {
            sdl_pointer_attack_render_cell(y, x, target_color, true);
            return;
        }

        for (int i = 0; i < path_n; i++) {
            int py = GRID_Y(path[i]);
            int px = GRID_X(path[i]);
            bool target = (py == y && px == x);

            sdl_pointer_attack_render_cell(py, px,
                target ? target_color : path_color, target);
            if (target)
                break;
        }
    } else {
        sdl_pointer_attack_render_cell(y, x, target_color, true);
    }
}

bool sdl_pointer_attack_take_command(int* command, int* dir)
{
    int kind;
    int mode;
    int m_idx;
    int target_y;
    int target_x;
    monster_type* m_ptr;

    if (!command || !dir)
        return false;
    if (!g_pointer_attack.pending)
        return false;

    if (g_pointer_attack.pending_wake) {
        (void)sdl_mouse_consume_wake_key();
        g_pointer_attack.pending_wake = false;
    }

    kind = g_pointer_attack.pending_kind;
    mode = g_pointer_attack.pending_mode;
    m_idx = g_pointer_attack.pending_m_idx;
    target_y = g_pointer_attack.pending_y;
    target_x = g_pointer_attack.pending_x;
    g_pointer_attack.pending = false;
    g_pointer_attack.pending_kind = SDL_POINTER_ATTACK_TARGET_NONE;
    g_pointer_attack.pending_mode = SDL_POINTER_ATTACK_NONE;
    g_pointer_attack.pending_m_idx = 0;
    g_pointer_attack.pending_y = 0;
    g_pointer_attack.pending_x = 0;

    if (!sdl_mouse_gameplay_context_active())
        return false;

    if (kind == SDL_POINTER_ATTACK_TARGET_LOCATION) {
        if (!sdl_pointer_attack_target_actionable(mode, true, target_y,
                target_x, NULL))
        {
            bell("No clear shot.");
            return false;
        }

        target_set_location(target_y, target_x);
        health_track(0);
        *command = (mode == SDL_POINTER_ATTACK_RANGED_2) ? 'F' : 'f';
        *dir = 5;
        return true;
    }

    if (kind == SDL_POINTER_ATTACK_TARGET_ALTER) {
        int alter_dir = 0;

        if (!sdl_pointer_attack_target_actionable(mode, true, target_y,
                target_x, NULL)
            || !sdl_pointer_attack_adjacent_dir_for_grid(target_y, target_x,
                &alter_dir))
        {
            bell("No adjacent action.");
            return false;
        }

        if (sdl_pointer_attack_adjacent_tunnel_target(target_y, target_x,
                NULL))
        {
            sdl_mouse_note_feature_for_action(target_y, target_x);
        }
        *command = '/';
        *dir = alter_dir;
        return true;
    }

    if (kind != SDL_POINTER_ATTACK_TARGET_MONSTER || m_idx <= 0)
        return false;

    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx || !m_ptr->ml) {
        bell("Target is gone.");
        return false;
    }

    if (!sdl_pointer_attack_target_actionable(mode, false, m_ptr->fy, m_ptr->fx,
        NULL))
    {
        bell(sdl_pointer_attack_blocked_message(mode, kind));
        return false;
    }

    health_track(m_idx);
    if (sdl_pointer_attack_mode_is_ranged(mode)) {
        target_set_monster(m_idx);
        *command = (mode == SDL_POINTER_ATTACK_RANGED_2) ? 'F' : 'f';
        *dir = 5;
    } else {
        *command = ';';
        *dir = motion_dir(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx);
        if (*dir == 5)
            return false;
    }

    return true;
}

void sdl_pointer_aim_cancel_touch_press(void)
{
    g_pointer_aim.touch_press_active = false;
    g_pointer_aim.touch_repeat_target = false;
    g_pointer_aim.touch_finger_id = 0;
    g_pointer_aim.touch_press_y = 0;
    g_pointer_aim.touch_press_x = 0;
    g_pointer_aim.touch_start_x = 0.0f;
    g_pointer_aim.touch_start_y = 0.0f;
}

void sdl_pointer_aim_clear_hover(void)
{
    if (!g_pointer_aim.hover_visible)
        return;

    g_pointer_aim.hover_visible = false;
    g_pointer_aim.hover_y = 0;
    g_pointer_aim.hover_x = 0;
    g_state.need_present = true;
}

void sdl_pointer_aim_clear_touch_selection(void)
{
    if (!g_pointer_aim.touch_selected)
        return;

    g_pointer_aim.touch_selected = false;
    g_pointer_aim.touch_selected_y = 0;
    g_pointer_aim.touch_selected_x = 0;
    g_state.need_present = true;
}

bool sdl_pointer_aim_point_to_map(float x, float y, int* out_y,
    int* out_x)
{
    int col = 0;
    int row = 0;
    int map_row;
    int map_col;
    int map_y;
    int map_x;

    if (!g_pointer_aim.active || !out_y || !out_x)
        return false;
    if (!character_generated || !character_dungeon || !p_ptr
        || !g_views[PANE_MAIN].term_ready)
    {
        return false;
    }
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (sdl_main_screen_cell_hits_character_panel(col, row))
        return false;
    if (row < ROW_MAP || col < COL_MAP)
        return false;

    map_row = row - ROW_MAP;
    map_col = col - COL_MAP;
    if (use_bigtile)
        map_col /= 2;

    if (map_row < 0 || map_col < 0 || map_row >= SCREEN_HGT
        || map_col >= SCREEN_WID)
    {
        return false;
    }

    map_y = p_ptr->wy + map_row;
    map_x = p_ptr->wx + map_col;
    if (!in_bounds(map_y, map_x))
        return false;

    *out_y = map_y;
    *out_x = map_x;
    return true;
}

bool sdl_pointer_aim_in_range(int y, int x)
{
    return g_pointer_aim.range <= 0
        || distance(p_ptr->py, p_ptr->px, y, x) <= g_pointer_aim.range;
}

bool sdl_pointer_aim_location_targetable(int y, int x)
{
    if ((y == p_ptr->py) && (x == p_ptr->px))
        return false;
    if (!sdl_pointer_aim_in_range(y, x))
        return false;

    return (cave_info[y][x] & (CAVE_FIRE | CAVE_WALL)) != 0;
}

bool sdl_pointer_aim_resolve_grid(int map_y, int map_x, int* out_dir,
    bool* out_exact_target)
{
    int dir = 0;
    int m_idx = 0;
    bool exact_target = false;

    if (out_dir)
        *out_dir = 0;
    if (out_exact_target)
        *out_exact_target = false;
    if (!g_pointer_aim.active || !p_ptr || !in_bounds(map_y, map_x))
        return false;
    if ((map_y == p_ptr->py) && (map_x == p_ptr->px))
        return false;

    m_idx = cave_m_idx[map_y][map_x];
    if ((m_idx > 0) && sdl_pointer_aim_in_range(map_y, map_x)
        && target_able(m_idx))
    {
        dir = 5;
        exact_target = true;
    }
    else if (sdl_pointer_aim_location_targetable(map_y, map_x))
    {
        dir = 5;
        exact_target = true;
    }
    else
    {
        dir = rough_direction(p_ptr->py, p_ptr->px, map_y, map_x);
    }

    if (!dir || (dir == 5 && !exact_target))
        return false;

    if (out_dir)
        *out_dir = dir;
    if (out_exact_target)
        *out_exact_target = exact_target;
    return true;
}

bool sdl_pointer_aim_queue_grid(int map_y, int map_x)
{
    int dir = 0;
    bool exact_target = false;

    if (!sdl_pointer_aim_resolve_grid(map_y, map_x, &dir, &exact_target))
    {
        bell("Pick a direction.");
        return false;
    }

    if (exact_target)
    {
        int m_idx = cave_m_idx[map_y][map_x];

        if ((m_idx > 0) && target_able(m_idx))
        {
            health_track(m_idx);
            target_set_monster(m_idx);
        }
        else
        {
            health_track(0);
            target_set_location(map_y, map_x);
        }
    }

    g_pointer_aim.pending = true;
    g_pointer_aim.pending_dir = dir;
    g_pointer_aim.pending_wake = true;
    g_state.need_present = true;
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

void sdl_pointer_aim_begin(int range, bool allow_vertical)
{
    g_pointer_aim.active = true;
    g_pointer_aim.range = range;
    g_pointer_aim.allow_vertical = allow_vertical;
    g_pointer_aim.hover_visible = false;
    g_pointer_aim.hover_y = 0;
    g_pointer_aim.hover_x = 0;
    g_pointer_aim.touch_selected = false;
    g_pointer_aim.touch_selected_y = 0;
    g_pointer_aim.touch_selected_x = 0;
    g_pointer_aim.pending = false;
    g_pointer_aim.pending_dir = 0;
    g_pointer_aim.pending_wake = false;
    sdl_pointer_aim_cancel_touch_press();
    sdl_pointer_attack_clear_hover();
    sdl_pointer_attack_clear_touch_selection();
    sdl_pointer_attack_cancel_touch_press();
    sdl_mouse_path_cancel();
}

void sdl_pointer_aim_end(void)
{
    if (!g_pointer_aim.active)
        return;

    g_pointer_aim.active = false;
    sdl_pointer_aim_clear_hover();
    sdl_pointer_aim_clear_touch_selection();
    sdl_pointer_aim_cancel_touch_press();
    g_state.need_present = true;
}

bool sdl_pointer_aim_take_direction(int* dir)
{
    if (!dir || !g_pointer_aim.pending)
        return false;

    if (g_pointer_aim.pending_wake)
    {
        (void)sdl_mouse_consume_wake_key();
        g_pointer_aim.pending_wake = false;
    }

    *dir = g_pointer_aim.pending_dir;
    g_pointer_aim.pending = false;
    g_pointer_aim.pending_dir = 0;
    return (*dir != 0);
}

bool sdl_pointer_aim_update_hover_grid(int map_y, int map_x)
{
    int dir = 0;
    bool exact_target = false;
    bool hoverable = sdl_pointer_aim_resolve_grid(map_y, map_x, &dir,
        &exact_target);
    bool changed = g_pointer_aim.hover_visible != hoverable
        || g_pointer_aim.hover_y != map_y
        || g_pointer_aim.hover_x != map_x;

    (void)dir;
    (void)exact_target;

    g_pointer_aim.hover_visible = hoverable;
    g_pointer_aim.hover_y = hoverable ? map_y : 0;
    g_pointer_aim.hover_x = hoverable ? map_x : 0;

    if (changed)
        g_state.need_present = true;

    return hoverable;
}

bool sdl_pointer_aim_handle_motion(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (!g_pointer_aim.active)
        return false;
    if (!sdl_pointer_aim_point_to_map(x, y, &map_y, &map_x))
    {
        sdl_pointer_aim_clear_hover();
        return false;
    }

    (void)sdl_pointer_aim_update_hover_grid(map_y, map_x);
    sdl_mouse_path_cancel();
    return true;
}

bool sdl_pointer_aim_handle_left_click(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (!sdl_pointer_aim_point_to_map(x, y, &map_y, &map_x))
        return false;

    (void)sdl_pointer_aim_update_hover_grid(map_y, map_x);
    (void)sdl_pointer_aim_queue_grid(map_y, map_x);
    return true;
}

bool sdl_pointer_aim_handle_touch_down(float x, float y,
    SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;

    if (!sdl_pointer_aim_point_to_map(x, y, &map_y, &map_x))
        return false;

    sdl_pointer_aim_clear_hover();
    sdl_pointer_aim_cancel_touch_press();
    g_pointer_aim.touch_press_active = true;
    g_pointer_aim.touch_repeat_target = g_pointer_aim.touch_selected
        && g_pointer_aim.touch_selected_y == map_y
        && g_pointer_aim.touch_selected_x == map_x;
    g_pointer_aim.touch_finger_id = finger_id;
    g_pointer_aim.touch_press_y = map_y;
    g_pointer_aim.touch_press_x = map_x;
    g_pointer_aim.touch_start_x = x;
    g_pointer_aim.touch_start_y = y;
    return true;
}

bool sdl_pointer_aim_handle_touch_motion(float x, float y,
    SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;
    float dx;
    float dy;

    if (!g_pointer_aim.touch_press_active
        || g_pointer_aim.touch_finger_id != finger_id)
    {
        return false;
    }

    dx = x - g_pointer_aim.touch_start_x;
    dy = y - g_pointer_aim.touch_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (!sdl_pointer_aim_point_to_map(x, y, &map_y, &map_x)
        || map_y != g_pointer_aim.touch_press_y
        || map_x != g_pointer_aim.touch_press_x
        || dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        sdl_pointer_aim_cancel_touch_press();
    }

    return true;
}

bool sdl_pointer_aim_handle_touch_up(float x, float y,
    SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;
    bool same_cell;
    bool repeat_target;

    if (!g_pointer_aim.touch_press_active
        || g_pointer_aim.touch_finger_id != finger_id)
    {
        return false;
    }

    same_cell = sdl_pointer_aim_point_to_map(x, y, &map_y, &map_x)
        && map_y == g_pointer_aim.touch_press_y
        && map_x == g_pointer_aim.touch_press_x;
    repeat_target = g_pointer_aim.touch_repeat_target;

    sdl_pointer_aim_cancel_touch_press();
    if (same_cell)
    {
        if (repeat_target)
        {
            (void)sdl_pointer_aim_queue_grid(map_y, map_x);
        }
        else if (sdl_pointer_aim_resolve_grid(map_y, map_x, NULL, NULL))
        {
            g_pointer_aim.touch_selected = true;
            g_pointer_aim.touch_selected_y = map_y;
            g_pointer_aim.touch_selected_x = map_x;
            g_state.need_present = true;
        }
        else
        {
            sdl_pointer_aim_clear_touch_selection();
            bell("Pick a direction.");
        }
    }

    return true;
}

bool sdl_pointer_aim_take_render_target(int* y, int* x)
{
    if (g_pointer_aim.hover_visible)
    {
        if (y)
            *y = g_pointer_aim.hover_y;
        if (x)
            *x = g_pointer_aim.hover_x;
        return true;
    }

    if (g_pointer_aim.touch_selected
        && sdl_pointer_aim_resolve_grid(g_pointer_aim.touch_selected_y,
            g_pointer_aim.touch_selected_x, NULL, NULL))
    {
        if (y)
            *y = g_pointer_aim.touch_selected_y;
        if (x)
            *x = g_pointer_aim.touch_selected_x;
        return true;
    }

    return false;
}

void sdl_pointer_aim_render_center_path(int dir, int target_y,
    int target_x, SDL_Color path_color, SDL_Color target_color)
{
    int ty = target_y;
    int tx = target_x;
    u16b path[MAX_RANGE];
    int path_n;

    if (dir != 5)
    {
        ty = p_ptr->py + 3 * ddy[dir];
        tx = p_ptr->px + 3 * ddx[dir];
    }

    path_n = project_path(path, 3, p_ptr->py, p_ptr->px, &ty, &tx,
        PROJECT_THRU | PROJECT_INVISIPASS);

    for (int i = 0; i < path_n; i++)
    {
        int py = GRID_Y(path[i]);
        int px = GRID_X(path[i]);
        bool target = (i == path_n - 1);

        sdl_pointer_attack_render_cell(py, px,
            target ? target_color : path_color, target);
    }
}

void sdl_pointer_aim_render_cone(int dir, int target_y, int target_x,
    SDL_Color path_color, SDL_Color target_color)
{
    int y0 = p_ptr->py;
    int x0 = p_ptr->px;
    int y1 = target_y;
    int x1 = target_x;
    int n1y;
    int n1x;
    int centerline;

    if (dir != 5)
    {
        y1 = y0 + MAX_RANGE * ddy[dir];
        x1 = x0 + MAX_RANGE * ddx[dir];
    }

    n1y = y1 - y0 + 20;
    n1x = x1 - x0 + 20;
    if (n1y > 40)
        n1y = 40;
    if (n1x > 40)
        n1x = 40;
    if (n1y < 0)
        n1y = 0;
    if (n1x < 0)
        n1x = 0;

    centerline = 90 - get_angle_to_grid[n1y][n1x];

    for (int y = y0 - 3; y <= y0 + 3; y++)
    {
        for (int x = x0 - 3; x <= x0 + 3; x++)
        {
            int dist;
            int n2y;
            int n2x;
            int tmp;
            int diff;

            if ((y == y0) && (x == x0))
                continue;
            if (!in_bounds(y, x))
                continue;
            dist = distance(y0, x0, y, x);
            if (dist > 3)
                continue;

            n2y = y - y0 + 20;
            n2x = x - x0 + 20;
            tmp = ABS(get_angle_to_grid[n2y][n2x] + centerline) % 180;
            diff = ABS(90 - tmp);
            if (diff >= (90 + 6) / 4)
                continue;
            if (!los(y0, x0, y, x))
                continue;

            sdl_pointer_attack_render_cell(y, x, path_color, false);
        }
    }

    sdl_pointer_aim_render_center_path(dir, target_y, target_x, path_color,
        target_color);
}

void sdl_pointer_aim_render(void)
{
    int y = 0;
    int x = 0;
    int dir = 0;
    bool exact_target = false;
    SDL_Color path_color = g_state.palette[TERM_UMBER];
    SDL_Color target_color = g_state.palette[TERM_L_RED];

    if (!g_pointer_aim.active || !character_generated || !character_dungeon
        || !p_ptr || !g_views[PANE_MAIN].term_ready)
    {
        return;
    }
    if (!sdl_pointer_aim_take_render_target(&y, &x))
        return;
    if (!sdl_pointer_aim_resolve_grid(y, x, &dir, &exact_target))
        return;

    (void)exact_target;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    sdl_pointer_aim_render_cone(dir, y, x, path_color, target_color);
}

bool sdl_mouse_stuck_door_bash_take_command(int* command, int* dir)
{
    int bash_y;
    int bash_x;
    int bash_dir;

    if (!command || !dir)
        return false;
    if (!g_mouse_path.stuck_door_bash_pending)
        return false;

    (void)sdl_mouse_consume_wake_key();

    bash_y = g_mouse_path.stuck_door_bash_y;
    bash_x = g_mouse_path.stuck_door_bash_x;
    bash_dir = g_mouse_path.stuck_door_bash_dir;
    g_mouse_path.stuck_door_bash_pending = false;
    g_mouse_path.stuck_door_bash_y = 0;
    g_mouse_path.stuck_door_bash_x = 0;
    g_mouse_path.stuck_door_bash_dir = 0;

    *command = ' ';
    *dir = 0;

    if (!sdl_mouse_gameplay_context_active())
        return true;
    if (!sdl_mouse_stuck_door_bash_target(bash_y, bash_x, &bash_dir))
    {
        log_debug("Queued stuck-door bash target is no longer valid at (%d,%d)",
            bash_y, bash_x);
        return true;
    }
    if (!get_check("Stuck door, do you want to bash it? "))
        return true;
    if (!sdl_mouse_stuck_door_bash_target(bash_y, bash_x, &bash_dir))
    {
        log_debug("Queued stuck-door bash target changed after confirmation at (%d,%d)",
            bash_y, bash_x);
        return true;
    }

    sdl_mouse_note_feature_for_action(bash_y, bash_x);
    *command = 'b';
    *dir = bash_dir;
    return true;
}


