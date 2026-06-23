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
    if (p_ptr && character_generated)
        return player_active_weapon_mode();

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
    int power_throw_slot;
    bool power_throw_mode;

    if (mode == SDL_POINTER_ATTACK_NONE)
        mode = SDL_POINTER_ATTACK_MELEE;

    if (sdl_pointer_attack_current_mode() == mode) {
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

    power_throw_slot = (p_ptr && character_generated)
        ? player_power_throw_quiver_slot() : 0;
    power_throw_mode = (power_throw_slot == INVEN_QUIVER1
            && mode == SDL_POINTER_ATTACK_RANGED_1)
        || (power_throw_slot == INVEN_QUIVER2
            && mode == SDL_POINTER_ATTACK_RANGED_2);

    if (p_ptr && character_generated) {
        if (!power_throw_mode)
        {
            player_queue_active_weapon_mode(mode);
            sdl_enqueue_bypassed_command(CMD_ACTIVE_WEAPON_MODE);
        }
    }
    else
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
    int mode = sdl_pointer_attack_current_mode();
    int m_idx = 0;
    int kind = SDL_POINTER_ATTACK_TARGET_NONE;
    bool hoverable = sdl_pointer_attack_target_hoverable(mode,
        manual, map_y, map_x, &m_idx, &kind);
    bool actionable = hoverable
        && sdl_pointer_attack_target_actionable(mode,
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

    (void)sdl_pointer_attack_queue_target(
        sdl_pointer_attack_current_mode(), manual, map_y, map_x);
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
    int mode;
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
    mode = sdl_pointer_attack_current_mode();
    manual = sdl_pointer_attack_manual_modifier_active();
    hoverable = sdl_pointer_attack_update_hover_grid(map_y, map_x, manual);
    if (!hoverable) {
        sdl_pointer_attack_clear_touch_selection();
        return false;
    }

    sdl_pointer_attack_cancel_touch_press();
    g_pointer_attack.touch_press_active = true;
    g_pointer_attack.touch_repeat_target =
        sdl_pointer_attack_touch_same_selected_target(mode,
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
    int mode;
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
    mode = sdl_pointer_attack_current_mode();
    manual = sdl_pointer_attack_manual_modifier_active();

    if (repeat_target) {
        (void)sdl_pointer_attack_queue_target(mode, manual, map_y, map_x);
        return true;
    }

    if (sdl_pointer_attack_target_hoverable(mode, manual,
            map_y, map_x, &m_idx, &kind))
    {
        g_pointer_attack.touch_selected = true;
        g_pointer_attack.touch_selected_manual = manual;
        g_pointer_attack.touch_selected_mode = mode;
        g_pointer_attack.touch_selected_kind = kind;
        g_pointer_attack.touch_selected_y = map_y;
        g_pointer_attack.touch_selected_x = map_x;
        g_pointer_attack.touch_selected_m_idx = m_idx;
        (void)sdl_pointer_attack_update_hover_grid(map_y, map_x, manual);
    } else {
        sdl_pointer_attack_clear_touch_selection();
        sdl_pointer_attack_clear_hover();
        bell(sdl_pointer_attack_blocked_message(mode, kind));
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
            *mode = sdl_pointer_attack_current_mode();
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

    mode = sdl_pointer_attack_current_mode();
    if (sdl_pointer_attack_manual_modifier_active()
        && sdl_pointer_attack_mode_is_ranged(mode))
    {
        sdl_pointer_attack_render_manual_ranged_overlay(
            mode, target_color);
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
    /* Let the aim-select command bar handle its own pointer input. */
    if (g_pointer_aim.select_mode && sdl_unified_look_prompt_contains_point(x, y))
        return false;
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

static void sdl_pointer_aim_select_clear_event(void)
{
    g_pointer_aim.event_pending = false;
    g_pointer_aim.event_kind = 0;
    g_pointer_aim.event_y = 0;
    g_pointer_aim.event_x = 0;
    g_pointer_aim.event_wake = false;
}

static void sdl_pointer_aim_select_clear_mouse_press(void)
{
    g_pointer_aim.select_mouse_press = false;
    g_pointer_aim.select_mouse_dragged = false;
    g_pointer_aim.select_mouse_start_x = 0.0f;
    g_pointer_aim.select_mouse_start_y = 0.0f;
    g_pointer_aim.select_mouse_last_x = 0.0f;
    g_pointer_aim.select_mouse_last_y = 0.0f;
    g_pointer_aim.select_mouse_accum_x = 0.0f;
    g_pointer_aim.select_mouse_accum_y = 0.0f;
}

void sdl_pointer_aim_select_begin(int range, bool allow_vertical)
{
    sdl_pointer_aim_begin(range, allow_vertical);
    g_pointer_aim.select_mode = true;
    g_pointer_aim.select_manual = false;
    g_pointer_aim.select_location = false;
    g_pointer_aim.select_visible = false;
    g_pointer_aim.select_y = 0;
    g_pointer_aim.select_x = 0;
    g_pointer_aim.select_adjacent = false;
    g_pointer_aim.select_choice_count = 0;
    g_pointer_aim.select_prompt[0] = '\0';
    sdl_pointer_aim_select_clear_mouse_press();
    sdl_pointer_aim_select_clear_event();
    sdl_pointer_attack_clear_pending();
    g_state.need_present = true;
}

void sdl_pointer_aim_select_end(void)
{
    g_pointer_aim.select_mode = false;
    g_pointer_aim.select_manual = false;
    g_pointer_aim.select_location = false;
    g_pointer_aim.select_visible = false;
    g_pointer_aim.select_y = 0;
    g_pointer_aim.select_x = 0;
    g_pointer_aim.select_adjacent = false;
    g_pointer_aim.select_choice_count = 0;
    g_pointer_aim.select_prompt[0] = '\0';
    sdl_pointer_aim_select_clear_mouse_press();
    sdl_pointer_aim_select_clear_event();
    sdl_pointer_aim_end();
}

void sdl_pointer_aim_select_set_choices(const int* ys, const int* xs,
    int count, cptr prompt)
{
    int max = (int)(sizeof(g_pointer_aim.select_choice_y)
        / sizeof(g_pointer_aim.select_choice_y[0]));

    if (count < 0)
        count = 0;
    if (count > max)
        count = max;

    g_pointer_aim.select_adjacent = true;
    g_pointer_aim.select_choice_count = count;
    for (int i = 0; i < count; i++) {
        g_pointer_aim.select_choice_y[i] = ys ? ys[i] : 0;
        g_pointer_aim.select_choice_x[i] = xs ? xs[i] : 0;
    }

    SDL_strlcpy(g_pointer_aim.select_prompt, prompt ? prompt : "",
        sizeof(g_pointer_aim.select_prompt));
    g_state.need_present = true;
}

static bool sdl_pointer_aim_select_is_choice(int y, int x)
{
    for (int i = 0; i < g_pointer_aim.select_choice_count; i++) {
        if (g_pointer_aim.select_choice_y[i] == y
            && g_pointer_aim.select_choice_x[i] == x)
        {
            return true;
        }
    }

    return false;
}

void sdl_pointer_aim_select_set_manual(bool manual)
{
    if (g_pointer_aim.select_manual == manual)
        return;

    g_pointer_aim.select_manual = manual;
    g_state.need_present = true;
}

void sdl_pointer_aim_select_set_location(bool location)
{
    if (g_pointer_aim.select_location == location)
        return;

    g_pointer_aim.select_location = location;
    g_state.need_present = true;
}

void sdl_pointer_aim_select_update(int y, int x)
{
    if (!g_pointer_aim.select_mode)
        return;
    if (g_pointer_aim.select_visible && g_pointer_aim.select_y == y
        && g_pointer_aim.select_x == x)
    {
        return;
    }

    g_pointer_aim.select_visible = true;
    g_pointer_aim.select_y = y;
    g_pointer_aim.select_x = x;
    g_state.need_present = true;
}

bool sdl_pointer_aim_select_take_event(int* kind, int* y, int* x)
{
    if (!g_pointer_aim.event_pending)
        return false;

    if (g_pointer_aim.event_wake)
    {
        (void)sdl_mouse_consume_wake_key();
        g_pointer_aim.event_wake = false;
    }

    if (kind)
        *kind = g_pointer_aim.event_kind;
    if (y)
        *y = g_pointer_aim.event_y;
    if (x)
        *x = g_pointer_aim.event_x;

    sdl_pointer_aim_select_clear_event();
    return true;
}

/* In aim-select mode, hover only moves the selection between monsters the
 * game-side target list would accept; manual mode roams any square. In the
 * adjacent-choice flavour, only the listed cells (e.g. doors) are hoverable. */
static bool sdl_pointer_aim_select_grid_hoverable(int y, int x)
{
    int m_idx;

    if (g_pointer_aim.select_adjacent)
        return sdl_pointer_aim_select_is_choice(y, x);

    if (g_pointer_aim.select_manual)
        return true;

    m_idx = cave_m_idx[y][x];
    if (m_idx <= 0)
        return false;
    if (!target_able(m_idx))
        return false;

    return sdl_pointer_aim_in_range(y, x);
}

static void sdl_pointer_aim_select_push_event(int kind, int y, int x)
{
    if (kind == AIM_SELECT_EVENT_PAN && g_pointer_aim.event_pending
        && g_pointer_aim.event_kind == AIM_SELECT_EVENT_PAN)
    {
        g_pointer_aim.event_y += y;
        g_pointer_aim.event_x += x;
        g_state.need_present = true;
        return;
    }

    if (g_pointer_aim.event_pending && g_pointer_aim.event_kind == kind
        && g_pointer_aim.event_y == y && g_pointer_aim.event_x == x)
    {
        return;
    }

    g_pointer_aim.event_pending = true;
    g_pointer_aim.event_kind = kind;
    g_pointer_aim.event_y = y;
    g_pointer_aim.event_x = x;
    g_pointer_aim.event_wake = true;
    g_state.need_present = true;
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
}

static bool sdl_pointer_aim_select_drag_motion(float x, float y)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    float cell_w;
    float cell_h;
    float total_dx;
    float total_dy;
    int pan_dy = 0;
    int pan_dx = 0;
    int old_wy;
    int old_wx;

    if (!g_pointer_aim.select_mouse_press)
        return false;

    g_pointer_aim.select_mouse_accum_x +=
        x - g_pointer_aim.select_mouse_last_x;
    g_pointer_aim.select_mouse_accum_y +=
        y - g_pointer_aim.select_mouse_last_y;
    g_pointer_aim.select_mouse_last_x = x;
    g_pointer_aim.select_mouse_last_y = y;

    total_dx = x - g_pointer_aim.select_mouse_start_x;
    total_dy = y - g_pointer_aim.select_mouse_start_y;
    if (total_dx < 0.0f)
        total_dx = -total_dx;
    if (total_dy < 0.0f)
        total_dy = -total_dy;
    if (total_dx > sdl_touch_swipe_threshold_px()
        || total_dy > sdl_touch_swipe_threshold_px())
    {
        g_pointer_aim.select_mouse_dragged = true;
    }

    cell_w = (view->term_ready && view->cell_w > 0)
        ? (float)view->cell_w : 1.0f;
    cell_h = (view->term_ready && view->cell_h > 0)
        ? (float)view->cell_h : 1.0f;
    if (use_bigtile)
        cell_w *= 2.0f;

    while (g_pointer_aim.select_mouse_accum_y >= cell_h)
    {
        pan_dy--;
        g_pointer_aim.select_mouse_accum_y -= cell_h;
    }
    while (g_pointer_aim.select_mouse_accum_y <= -cell_h)
    {
        pan_dy++;
        g_pointer_aim.select_mouse_accum_y += cell_h;
    }
    while (g_pointer_aim.select_mouse_accum_x >= cell_w)
    {
        pan_dx--;
        g_pointer_aim.select_mouse_accum_x -= cell_w;
    }
    while (g_pointer_aim.select_mouse_accum_x <= -cell_w)
    {
        pan_dx++;
        g_pointer_aim.select_mouse_accum_x += cell_w;
    }

    if (pan_dy || pan_dx)
    {
        g_pointer_aim.select_mouse_dragged = true;
        old_wy = p_ptr->wy;
        old_wx = p_ptr->wx;
        (void)sdl_main_map_apply_pan(pan_dy, pan_dx);
        if ((p_ptr->wy != old_wy) || (p_ptr->wx != old_wx))
        {
            sdl_pointer_aim_select_push_event(AIM_SELECT_EVENT_PAN,
                p_ptr->wy - old_wy, p_ptr->wx - old_wx);
        }
    }

    return true;
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
    if (g_pointer_aim.select_mode && g_pointer_aim.select_location
        && sdl_pointer_aim_select_drag_motion(x, y))
    {
        return true;
    }
    if (!sdl_pointer_aim_point_to_map(x, y, &map_y, &map_x))
    {
        sdl_pointer_aim_clear_hover();
        return false;
    }

    if (g_pointer_aim.select_mode)
    {
        if (sdl_pointer_aim_select_grid_hoverable(map_y, map_x)
            && !(g_pointer_aim.select_visible
                && g_pointer_aim.select_y == map_y
                && g_pointer_aim.select_x == map_x))
        {
            sdl_pointer_aim_select_push_event(AIM_SELECT_EVENT_HOVER, map_y,
                map_x);
        }
        sdl_mouse_path_cancel();
        return true;
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

    if (g_pointer_aim.select_mode)
    {
        if (g_pointer_aim.select_location)
        {
            sdl_pointer_aim_select_clear_mouse_press();
            g_pointer_aim.select_mouse_press = true;
            g_pointer_aim.select_mouse_start_x = x;
            g_pointer_aim.select_mouse_start_y = y;
            g_pointer_aim.select_mouse_last_x = x;
            g_pointer_aim.select_mouse_last_y = y;
            return true;
        }

        sdl_pointer_aim_select_push_event(AIM_SELECT_EVENT_CLICK, map_y,
            map_x);
        return true;
    }

    (void)sdl_pointer_aim_update_hover_grid(map_y, map_x);
    (void)sdl_pointer_aim_queue_grid(map_y, map_x);
    return true;
}

bool sdl_pointer_aim_handle_left_release(float x, float y)
{
    int map_y = 0;
    int map_x = 0;
    bool dragged;

    if (!g_pointer_aim.active || !g_pointer_aim.select_mode
        || !g_pointer_aim.select_location
        || !g_pointer_aim.select_mouse_press)
    {
        return false;
    }

    dragged = g_pointer_aim.select_mouse_dragged;
    sdl_pointer_aim_select_clear_mouse_press();
    if (dragged)
        return true;

    if (sdl_pointer_aim_point_to_map(x, y, &map_y, &map_x))
    {
        sdl_pointer_aim_select_push_event(
            AIM_SELECT_EVENT_CLICK, map_y, map_x);
    }

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
    if (g_pointer_aim.select_mode)
    {
        if (same_cell)
            sdl_pointer_aim_select_push_event(AIM_SELECT_EVENT_TAP, map_y,
                map_x);
        return true;
    }
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

/* Render the aim-select selection: the projected shot path up to the
 * selected square, or a blocked highlight when the square cannot be
 * reached. */
static void sdl_pointer_aim_render_selection(void)
{
    int y = g_pointer_aim.select_y;
    int x = g_pointer_aim.select_x;
    int range = (g_pointer_aim.range > 0) ? g_pointer_aim.range : MAX_RANGE;
    int ty = y;
    int tx = x;
    u16b path[256];
    int path_n;
    bool reached = false;
    SDL_Color path_color = g_state.palette[TERM_UMBER];
    SDL_Color target_color = g_state.palette[TERM_L_RED];
    SDL_Color blocked_color = g_state.palette[TERM_SLATE];

    if (!g_pointer_aim.select_visible)
        return;
    if ((y == p_ptr->py) && (x == p_ptr->px))
        return;

    if (g_pointer_aim.select_location)
    {
        sdl_pointer_attack_render_cell(y, x, target_color, true);
        return;
    }

    path_n = project_path(path, range, p_ptr->py, p_ptr->px, &ty, &tx,
        PROJECT_THRU | PROJECT_INVISIPASS);

    for (int i = 0; i < path_n; i++)
    {
        int gy = GRID_Y(path[i]);
        int gx = GRID_X(path[i]);
        bool target = (gy == y) && (gx == x);

        sdl_pointer_attack_render_cell(gy, gx,
            target ? target_color : path_color, target);
        if (target)
        {
            reached = true;
            break;
        }
    }

    if (!reached)
        sdl_pointer_attack_render_cell(y, x, blocked_color, true);
}

/* Pixel rect of a map cell, or false when it is off the current panel. */
static bool sdl_pointer_aim_cell_rect(int y, int x, SDL_FRect* out)
{
    int cell_cols = use_bigtile ? 2 : 1;
    int term_row = ROW_MAP + (y - p_ptr->wy);
    int term_col = COL_MAP + (x - p_ptr->wx) * cell_cols;

    if (!panel_contains(y, x))
        return false;
    return sdl_main_cell_rect(term_col, term_row, cell_cols, 1, out);
}

/* Draw the small prompt popup ("Close which door?") for adjacent-choice
 * selection, anchored just outside the player's 3x3 door ring so it sits
 * next to the doors without covering them. */
static void sdl_pointer_aim_render_choice_prompt(void)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    TTF_Font* font;
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect box;
    SDL_FRect text_dst;
    SDL_FRect player_rect;
    SDL_Color text_color = g_state.palette[TERM_WHITE];
    SDL_Color border = g_state.palette[TERM_YELLOW];
    float pad;
    float gap;
    int font_px;
    float view_x = (float)(view->rect.x + view->margin_x);
    float view_y = (float)(view->rect.y + view->margin_y);
    float view_w = (float)(sdl_main_view_visual_cols(view) * view->cell_w);
    float view_h = (float)(sdl_main_view_visual_rows(view) * view->cell_h);
    float cell_h = (float)view->cell_h;

    if (!g_pointer_aim.select_prompt[0])
        return;
    if (!sdl_pointer_aim_cell_rect(p_ptr->py, p_ptr->px, &player_rect))
        return;

    font_px = sdl_object_tooltip_font_px();
    font = sdl_story_font_for_height_slot(font_px, STORY_FONT_SLOT_SECONDARY);
    if (!font)
        return;

    pad = sdl_touch_pane_clampf((float)font_px * 0.4f, 8.0f, 16.0f);
    gap = sdl_touch_pane_clampf(cell_h * 0.4f, 6.0f, 14.0f);
    surface = sdl_object_tooltip_render_text_surface(font,
        g_pointer_aim.select_prompt, text_color, view_w - pad * 2.0f);
    if (!surface)
        return;
    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }

    box.w = (float)surface->w + pad * 2.0f;
    box.h = (float)surface->h + pad * 2.0f;

    /* Centre on the player horizontally and sit just below the bottom row of
     * adjacent doors; flip above if there is no room below. */
    box.x = player_rect.x + player_rect.w * 0.5f - box.w * 0.5f;
    box.y = player_rect.y + player_rect.h + cell_h + gap;
    if (box.y + box.h > view_y + view_h - gap)
        box.y = player_rect.y - cell_h - box.h - gap;

    /* Keep the popup within the map view. */
    if (box.x < view_x + gap)
        box.x = view_x + gap;
    if (box.x + box.w > view_x + view_w - gap)
        box.x = view_x + view_w - gap - box.w;
    if (box.y < view_y + gap)
        box.y = view_y + gap;
    if (box.y + box.h > view_y + view_h - gap)
        box.y = view_y + view_h - gap - box.h;

    text_dst = (SDL_FRect){ box.x + pad, box.y + pad, (float)surface->w,
        (float)surface->h };

    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 232);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b, 200);
    SDL_RenderRect(g_state.renderer, &box);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &text_dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

/* Highlight each adjacent candidate grid; the current selection is brighter. */
static void sdl_pointer_aim_render_choices(void)
{
    SDL_Color choice_color = g_state.palette[TERM_L_BLUE];
    SDL_Color target_color = g_state.palette[TERM_L_RED];

    for (int i = 0; i < g_pointer_aim.select_choice_count; i++) {
        int cy = g_pointer_aim.select_choice_y[i];
        int cx = g_pointer_aim.select_choice_x[i];
        bool selected = g_pointer_aim.select_visible
            && g_pointer_aim.select_y == cy && g_pointer_aim.select_x == cx;

        sdl_pointer_attack_render_cell(cy, cx,
            selected ? target_color : choice_color, selected);
    }

    sdl_pointer_aim_render_choice_prompt();
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

    if (g_pointer_aim.select_mode)
    {
        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        if (g_pointer_aim.select_adjacent)
            sdl_pointer_aim_render_choices();
        else
            sdl_pointer_aim_render_selection();
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
    if (!get_check_near(bash_y, bash_x,
            "Stuck door, do you want to bash it? "))
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

/*
 * Run the queued grid interaction popup (right-click / long-press on an
 * adjacent grid) in game context and translate the answer into a normal
 * game command.  Consumes the pending request even when cancelled.
 */
bool sdl_grid_question_take_command(int* command, int* dir)
{
    int ask_y;
    int ask_x;

    if (!command || !dir)
        return false;
    if (!g_mouse_path.grid_question_pending)
        return false;

    (void)sdl_mouse_consume_wake_key();

    ask_y = g_mouse_path.grid_question_y;
    ask_x = g_mouse_path.grid_question_x;
    g_mouse_path.grid_question_pending = false;
    g_mouse_path.grid_question_y = 0;
    g_mouse_path.grid_question_x = 0;

    *command = ' ';
    *dir = 0;

    if (!sdl_mouse_gameplay_context_active())
        return true;

    if (!grid_interact_question(ask_y, ask_x, command, dir))
    {
        *command = ' ';
        *dir = 0;
        return true;
    }

    sdl_mouse_note_feature_for_action(ask_y, ask_x);
    return true;
}


