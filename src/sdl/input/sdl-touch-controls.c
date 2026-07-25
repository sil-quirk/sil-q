#include "angband.h"
#include "sdl/main-sdl-private.h"

#define SDL_TOUCH_ROUND_INNER_RADIUS_FRAC 0.45f
#define SDL_TOUCH_ROUND_DRAG_THRESHOLD_FRAC 0.62f
#define SDL_TOUCH_ROUND_RUN_BUTTON_DISTANCE_MULT 1.3f
/* The whole inner disc (up to the drag threshold) repeats the last direction,
 * so the centre reads as a single "repeat" button with no dead band. */
#define SDL_TOUCH_ROUND_CENTER_REPEAT_FRAC 0.62f
#define SDL_TOUCH_ROUND_HIT_SLOP_FRAC 0.18f

static int sdl_touch_context_binding(int binding);
static void sdl_touch_top_panel_set_hover_slot(int slot);
static SDL_FRect g_touch_top_panel_cached_buttons[
    SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
static Uint64 g_touch_top_panel_cached_generation;
static bool g_touch_top_panel_cached_layout_valid;

/* Run a main-menu action that opens a modal screen inline (Quick Access / thumb
 * / swipe buttons bound to a main-menu choice).  Those screens call inkey(),
 * whose cleanup clears inkey_flag; when an outer command-wait inkey() is still
 * blocked (the usual case when a button fires during gameplay) that leaves
 * pointer/mouse command shortcuts inactive until the next physical keypress.
 * Mirror sdl_main_menu_overlay_choose() and restore the command-wait state. */
static void sdl_touch_run_main_menu_choice(int choice)
{
    bool restore_command_wait = inkey_flag;

    (void)do_cmd_main_menu_execute_choice(choice);
    if (restore_command_wait && character_icky == 0)
        inkey_flag = true;
}

static void sdl_touch_run_quick_access_picker(int slot)
{
    bool restore_command_wait = inkey_flag;

    (void)do_cmd_touch_top_widget_pick_button(slot);
    if (restore_command_wait && character_icky == 0)
        inkey_flag = true;
}

void sdl_touch_pane_send_confirm_action(void)
{
    if (character_dungeon) {
        Term_keypress(' ');
        return;
    }

    Term_keypress('\r');
}

void sdl_touch_pane_send_binding(int binding, bool second_panel, bool long_press)
{
    if (binding == GAMEPAD_BIND_NONE)
        return;

    if (binding == TOUCH_BIND_TOP_PANEL_OPEN) {
        sdl_touch_top_panel_set_open(true);
        return;
    }

    if (binding == TOUCH_BIND_TOP_PANEL_CLOSE) {
        sdl_touch_top_panel_set_open(false);
        return;
    }

    if (binding == TOUCH_BIND_MAIN_MENU_KNOWLEDGE) {
        sdl_touch_run_main_menu_choice(MAIN_MENU_KNOWLEDGE);
        return;
    }

    if (binding == TOUCH_BIND_MAIN_MENU_HINTS_QUESTS) {
        sdl_touch_run_main_menu_choice(MAIN_MENU_HINTS_QUESTS);
        return;
    }

    if (binding == TOUCH_BIND_TOGGLE_TILES) {
        set_sdl_tiles(!get_sdl_tiles());
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        g_touch_pane_second_panel = !g_touch_pane_second_panel;
        g_state.need_present = true;
        return;
    }

    if (binding == GAMEPAD_BIND_CTRL) {
        g_touch_pane_ctrl_toggle = !g_touch_pane_ctrl_toggle;
        sdl_gamepad_apply_modifier(binding, g_touch_pane_ctrl_toggle);
        return;
    }

    if (binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, true);
        sdl_gamepad_apply_modifier(binding, false);
        return;
    }

    if (sdl_pointer_attack_toggle_binding(binding))
        return;

    if (!long_press)
        binding = sdl_touch_context_binding(binding);

    if (sdl_touch_pane_confirm_binding(binding)) {
        if (long_press && character_dungeon) {
            Term_keypress('z');
        } else {
            sdl_touch_pane_send_confirm_action();
        }
        return;
    }

    if (sdl_touch_pane_binding_is_direction(binding)) {
        sdl_gamepad_send_direction_mods(binding - '0',
            ((!long_press) && second_panel) || sdl_gamepad_shift_active(),
            long_press || sdl_gamepad_ctrl_active(),
            sdl_gamepad_alt_active());
        return;
    }

    if (binding == 'z' && long_press) {
        Term_keypress('Z');
        return;
    }

    if (character_icky > 0
        && sdl_pane_command_shortcuts_active()
        && sdl_binding_opens_pane_menu(binding))
    {
        sdl_enqueue_bypassed_command(binding);
        return;
    }

    sdl_gamepad_send_key(binding, false);
}

int sdl_inventory_equipment_cycle_binding(int binding)
{
    if (!config.touch_pane_inventory_equipment_cycle)
        return binding;
    if (character_icky <= 0)
        return binding;
    if (binding != 'i' && binding != 'e')
        return binding;

    if (p_ptr) {
        if (p_ptr->command_wrk == USE_INVEN && binding == 'i')
            return 'e';
        if (p_ptr->command_wrk == USE_EQUIP && binding == 'e')
            return 'i';
    }

    if (current_menu_command != 0) {
        if (current_menu_state == 0 && binding == 'i')
            return 'e';
        if (current_menu_state == 1 && binding == 'e')
            return 'i';
    }

    return binding;
}

void sdl_touch_pane_send_slot(int panel, int index, bool long_press)
{
    int binding;

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    binding = sdl_touch_pane_effective_binding_for_panel(panel, index);
    binding = sdl_inventory_equipment_cycle_binding(binding);
    sdl_touch_pane_send_binding(binding, panel == SDL_TOUCH_PANE_PANEL_SECOND,
        long_press);
}

int sdl_touch_swipe_index_for_keypad_dir(int dir)
{
    switch (dir) {
    case 8:
        return TOUCH_SWIPE_DIR_UP;
    case 2:
        return TOUCH_SWIPE_DIR_DOWN;
    case 4:
        return TOUCH_SWIPE_DIR_LEFT;
    case 6:
        return TOUCH_SWIPE_DIR_RIGHT;
    default:
        return -1;
    }
}

float sdl_touch_swipe_threshold_px(void)
{
    int cell_px = (g_views[0].cell_w > g_views[0].cell_h) ? g_views[0].cell_w : g_views[0].cell_h;
    float threshold = (float)cell_px * 0.75f;

    if (threshold < TOUCH_SWIPE_MIN_DISTANCE_PX)
        threshold = TOUCH_SWIPE_MIN_DISTANCE_PX;
    if (threshold > TOUCH_SWIPE_MAX_DISTANCE_PX)
        threshold = TOUCH_SWIPE_MAX_DISTANCE_PX;

    return threshold;
}

int sdl_touch_swipe_direction_for_delta(float dx, float dy, float threshold)
{
    float abs_x = (dx >= 0.0f) ? dx : -dx;
    float abs_y = (dy >= 0.0f) ? dy : -dy;

    if (abs_x < threshold && abs_y < threshold)
        return 0;

    if (abs_x >= abs_y)
        return (dx >= 0.0f) ? 6 : 4;

    return (dy >= 0.0f) ? 2 : 8;
}

float sdl_touch_swipe_edge_px(const SDL_Rect* screen)
{
    float inset;
    int min_dim;

    if (!screen || !sdl_rect_has_area(screen))
        return 56.0f;

    min_dim = (screen->w < screen->h) ? screen->w : screen->h;
    inset = (float)min_dim * 0.055f;
    if (inset < 56.0f)
        inset = 56.0f;
    if (inset > 96.0f)
        inset = 96.0f;

    return inset;
}

bool sdl_touch_swipe_point_near_top_panel_edge(float x, float y)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    float inset;

    if (!sdl_rect_has_area(&screen))
        return false;

    inset = sdl_touch_swipe_edge_px(&screen);
    return x >= (float)screen.x
        && x < (float)(screen.x + screen.w)
        && y <= (float)(screen.y + screen.h)
        && y > (float)(screen.y + screen.h) - inset;
}

bool sdl_touch_swipe_round_layer_start_allowed(float x, float y)
{
    if (!sdl_touch_round_layer_controls_active())
        return true;
    if (sdl_touch_top_panel_point_to_slot(x, y, NULL))
        return true;
    if (sdl_touch_pane_point_to_slot(x, y, NULL))
        return true;
    if (sdl_touch_top_panel_layout_visible()
        && sdl_touch_swipe_point_near_top_panel_edge(x, y))
    {
        return true;
    }

    return false;
}

bool sdl_touch_swipe_start_can_toggle_top_panel(void)
{
    return sdl_touch_top_panel_point_to_slot(g_touch_swipe.start_x,
            g_touch_swipe.start_y, NULL)
        || sdl_touch_swipe_point_near_top_panel_edge(g_touch_swipe.start_x,
            g_touch_swipe.start_y);
}

bool sdl_touch_swipe_binding_is_top_panel_action(int binding)
{
    return binding == TOUCH_BIND_TOP_PANEL_OPEN
        || binding == TOUCH_BIND_TOP_PANEL_CLOSE;
}

void sdl_touch_swipe_cancel(void)
{
    g_touch_swipe.active = false;
    g_touch_swipe.triggered = false;
    g_touch_swipe.finger_id = 0;
    g_touch_swipe.start_x = 0.0f;
    g_touch_swipe.start_y = 0.0f;
    g_touch_swipe.last_x = 0.0f;
    g_touch_swipe.last_y = 0.0f;
}

bool sdl_touch_swipe_handle_pointer_down(float x, float y, SDL_FingerID finger_id)
{
    int slot = -1;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_touch_swipe_round_layer_start_allowed(x, y))
    {
        return false;
    }
    if (!config.touch_swipe_enabled)
        return false;
    if (sdl_touch_pane_is_config_enabled()
        && sdl_touch_pane_point_to_slot(x, y, &slot))
    {
        return false;
    }

    sdl_touch_swipe_cancel();
    g_touch_swipe.active = true;
    g_touch_swipe.triggered = false;
    g_touch_swipe.finger_id = finger_id;
    g_touch_swipe.start_x = x;
    g_touch_swipe.start_y = y;
    g_touch_swipe.last_x = x;
    g_touch_swipe.last_y = y;
    return true;
}

bool sdl_touch_swipe_handle_pointer_motion(float x, float y, SDL_FingerID finger_id)
{
    int dir;
    int binding;
    int swipe_index;
    float dx;
    float dy;

    if (!g_touch_swipe.active || g_touch_swipe.finger_id != finger_id)
        return false;
    if (!sdl_main_screen_click_shortcuts_active()) {
        sdl_touch_swipe_cancel();
        return false;
    }

    g_touch_swipe.last_x = x;
    g_touch_swipe.last_y = y;

    if (g_touch_swipe.triggered)
        return true;

    dx = x - g_touch_swipe.start_x;
    dy = y - g_touch_swipe.start_y;
    dir = sdl_touch_swipe_direction_for_delta(dx, dy, sdl_touch_swipe_threshold_px());
    if (!dir)
        return true;

    if (!config.touch_swipe_enabled) {
        sdl_touch_swipe_cancel();
        return false;
    }

    swipe_index = sdl_touch_swipe_index_for_keypad_dir(dir);
    if (swipe_index < 0)
        return true;

    binding = config.touch_swipe_bindings[swipe_index];
    if (sdl_touch_round_layer_controls_active()
        && sdl_touch_swipe_binding_is_top_panel_action(binding)
        && !sdl_touch_swipe_start_can_toggle_top_panel())
    {
        sdl_touch_swipe_cancel();
        return false;
    }
    if (binding != GAMEPAD_BIND_NONE)
        sdl_touch_pane_send_binding(binding, false, false);
    g_touch_swipe.triggered = true;
    return true;
}

void sdl_touch_swipe_handle_pointer_up(float x, float y, SDL_FingerID finger_id)
{
    if (!sdl_touch_swipe_handle_pointer_motion(x, y, finger_id))
        return;

    sdl_touch_swipe_cancel();
}

void sdl_touch_pane_cancel_press(void)
{
    if (!g_touch_pane_press.active && g_touch_pane_pressed_slot < 0)
        return;

    g_touch_pane_press.active = false;
    g_touch_pane_press.long_press_enabled = false;
    g_touch_pane_press.start_x = 0.0f;
    g_touch_pane_press.start_y = 0.0f;
    g_touch_pane_pressed_slot = -1;
    g_state.need_present = true;
}

int sdl_touch_pane_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_touch_pane_press.active)
        return -1;
    if (!g_touch_pane_press.long_press_enabled)
        return -1;

    elapsed = now_ns - g_touch_pane_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_touch_pane_flush_pending_press(Uint64 now_ns)
{
    int slot;
    int panel;

    if (!g_touch_pane_press.active)
        return false;
    if (!g_touch_pane_press.long_press_enabled)
        return false;
    if (now_ns - g_touch_pane_press.start_time < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return false;

    slot = g_touch_pane_press.slot;
    panel = g_touch_pane_press.panel;
    sdl_touch_pane_cancel_press();
    if (slot == 0) {
        sdl_touch_pane_begin_reset_confirm();
    } else {
        sdl_touch_pane_send_slot(panel, slot, true);
        g_touch_pane_flash_slot = slot;
        g_touch_pane_flash_until = SDL_GetTicksNS() + 150000000ULL;
        g_state.need_present = true;
    }
    return true;
}

bool sdl_touch_pane_handle_pointer_down(float x, float y, bool mouse, SDL_FingerID finger_id)
{
    int slot = -1;
    int panel;
    int binding;
    bool long_press_enabled;

    if (!sdl_touch_pane_point_to_slot(x, y, &slot))
        return false;

    if (slot < 0)
        return true;

    panel = sdl_touch_pane_active_panel();
    binding = sdl_touch_pane_effective_binding_for_panel(panel, slot);
    long_press_enabled = !sdl_touch_pane_proto_mode_active()
        && (sdl_touch_pane_uses_mobile_toggle()
            || sdl_touch_pane_slot_uses_long_press(slot, binding));

    if (!mouse || long_press_enabled) {
        sdl_touch_pane_cancel_press();
        g_touch_pane_press.active = true;
        g_touch_pane_press.mouse = mouse;
        g_touch_pane_press.finger_id = finger_id;
        g_touch_pane_press.panel = panel;
        g_touch_pane_press.slot = slot;
        g_touch_pane_press.long_press_enabled = long_press_enabled;
        g_touch_pane_press.start_x = x;
        g_touch_pane_press.start_y = y;
        g_touch_pane_press.start_time = SDL_GetTicksNS();
        g_touch_pane_pressed_slot = slot;
        g_state.need_present = true;
        return true;
    }

    sdl_touch_pane_send_slot(panel, slot, false);
    g_touch_pane_flash_slot = slot;
    g_touch_pane_flash_until = SDL_GetTicksNS() + 150000000ULL;
    g_state.need_present = true;
    return true;
}

bool sdl_touch_pane_handle_pointer_motion(float x, float y, bool mouse,
    SDL_FingerID finger_id)
{
    int slot = -1;
    float dx;
    float dy;
    float threshold;

    if (!g_touch_pane_press.active)
        return false;
    if (g_touch_pane_press.mouse != mouse)
        return false;
    if (!mouse && g_touch_pane_press.finger_id != finger_id)
        return false;

    dx = x - g_touch_pane_press.start_x;
    dy = y - g_touch_pane_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_touch_swipe_threshold_px();
    if (dx > threshold || dy > threshold) {
        float start_x = g_touch_pane_press.start_x;
        float start_y = g_touch_pane_press.start_y;

        sdl_touch_pane_cancel_press();
        if (!mouse && !sdl_touch_pane_proto_mode_active()) {
            if (g_touch_swipe.active && g_touch_swipe.finger_id == finger_id) {
                (void)sdl_touch_swipe_handle_pointer_motion(x, y, finger_id);
            } else if (sdl_touch_swipe_handle_pointer_down(start_x, start_y,
                finger_id))
            {
                (void)sdl_touch_swipe_handle_pointer_motion(x, y, finger_id);
            }
        }
        return true;
    }

    if (!sdl_touch_pane_point_to_slot(x, y, &slot)
        || slot != g_touch_pane_press.slot)
    {
        sdl_touch_pane_cancel_press();
        return true;
    }

    return true;
}

void sdl_touch_pane_handle_pointer_up(float x, float y, bool mouse,
    SDL_FingerID finger_id)
{
    Uint64 press_time;
    bool ctrl_direction_override;
    int slot;
    int panel;
    int release_slot = -1;

    if (!g_touch_pane_press.active)
        return;
    if (g_touch_pane_press.mouse != mouse)
        return;
    if (!mouse && g_touch_pane_press.finger_id != finger_id)
        return;

    press_time = SDL_GetTicksNS() - g_touch_pane_press.start_time;
    ctrl_direction_override = g_touch_pane_press.long_press_enabled
        && (press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL);
    slot = g_touch_pane_press.slot;
    panel = g_touch_pane_press.panel;
    (void)sdl_touch_pane_point_to_slot(x, y, &release_slot);
    sdl_touch_pane_cancel_press();
    if (release_slot != slot)
        return;

    if (slot == 0 && ctrl_direction_override) {
        sdl_touch_pane_begin_reset_confirm();
    } else {
        sdl_touch_pane_send_slot(panel, slot, ctrl_direction_override);
        g_touch_pane_flash_slot = slot;
        g_touch_pane_flash_until = SDL_GetTicksNS() + 150000000ULL;
        g_state.need_present = true;
    }
}

#define SDL_TOUCH_THUMB_BIND_SPACE_CONTEXT -101
#define SDL_TOUCH_THUMB_BIND_FIRE_SELECTED -102
#define SDL_TOUCH_THUMB_BIND_CHANGE_QUIVER -103
#define SDL_TOUCH_THUMB_BIND_QUICK_THROW -104
#define SDL_TOUCH_THUMB_BIND_QUICK_THROW_TARGET -105

enum {
    SDL_TOUCH_THUMB_GAMEPLAY_ACTION_COUNT = 4,
    /* Interactive descriptions already own the largest possible action list.
     * Reusing that source capacity means Quick Touch adds no smaller display
     * cap of its own. */
    SDL_TOUCH_THUMB_RUNTIME_CAPACITY = SDL_DESCRIPTION_OVERLAY_MAX_ACTIONS
};

typedef struct touch_thumb_button {
    int tap_binding;
    int long_binding;
} touch_thumb_button;

typedef struct touch_thumb_button_set {
    touch_thumb_button buttons[SDL_TOUCH_THUMB_RUNTIME_CAPACITY];
    int count;
    int gameplay_base_count;
} touch_thumb_button_set;

typedef struct touch_target_layout_state {
    bool locked;
    bool thumb_valid;
    SDL_FRect thumb_rects[SDL_TOUCH_THUMB_RUNTIME_CAPACITY];
    bool wheel_valid;
    float wheel_cx;
    float wheel_cy;
    float wheel_radius;
    float wheel_inner_radius;
    SDL_Rect wheel_clip;
} touch_target_layout_state;

static touch_target_layout_state g_touch_target_layout;

static bool sdl_touch_thumb_description_open(void);

/* ---- Thumb buttons --------------------------------------------------------
 *
 * Semi-transparent overlay buttons that fill the left-edge gap below the
 * COLLAPSED ("compact") left character panel and above the combat overlay pane.
 * Column mode follows the left-panel width; row mode keeps the equivalent
 * compact-column footprint instead of stretching across the horizontal panel.
 * Touch-only.
 * Gameplay publishes every contextual action that currently applies: selected
 * quiver fire, quick throw, the current Space/floor-item action, and wait/rest.
 * Interactive descriptions publish their complete footer action list.  Layout
 * is driven by the resulting count rather than by fixed top/bottom/sidecar
 * slots.
 * The press/long-press machinery and the action dispatch mirror the touch-pane
 * buttons; the look mirrors the translucent movement button-wheel.
 */

static bool sdl_touch_thumb_space_action(
    int* out_key, char* label, size_t label_len)
{
    int key = ' ';
    char ctx[32];

    if (!touch_shortcut_context_action(' ',
            sdl_touch_thumb_description_open(), &key, ctx, sizeof(ctx)))
    {
        return false;
    }
    if (strcmp(ctx, "Confirm") == 0)
        return false;
    if (strcmp(ctx, "Pick Up") == 0)
        return false;

    if (out_key)
        *out_key = key;
    if (label && label_len)
        SDL_strlcpy(label, ctx, label_len);
    return true;
}

static bool sdl_touch_thumb_pickup_action(void)
{
    char ctx[32];

    if (!touch_shortcut_context_action(' ',
            sdl_touch_thumb_description_open(), NULL, ctx, sizeof(ctx)))
    {
        return false;
    }

    return strcmp(ctx, "Pick Up") == 0;
}

static bool sdl_touch_thumb_ranged_mode_active(void)
{
    return character_dungeon && p_ptr && p_ptr->playing && !p_ptr->is_dead
        && player_active_weapon_is_ranged();
}

static bool sdl_touch_thumb_quick_throw_active(void)
{
    if (!character_dungeon || !p_ptr || !p_ptr->playing || p_ptr->is_dead)
        return false;

    return player_quick_throw_available();
}

static void sdl_touch_thumb_throw_label(char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    SDL_strlcpy(buf, "Quick Throw", buflen);
}

static bool sdl_touch_thumb_fire_targeting_active(void)
{
    return g_pointer_aim.active && g_pointer_aim.select_mode
        && !g_pointer_aim.select_location;
}

static bool sdl_touch_round_aim_targeting_active(void)
{
    return g_pointer_aim.active && g_pointer_aim.select_mode
        && !g_pointer_aim.select_location;
}

static void sdl_touch_thumb_append_button(touch_thumb_button_set* set,
    int tap_binding, int long_binding)
{
    if (!set || set->count >= SDL_TOUCH_THUMB_RUNTIME_CAPACITY)
        return;
    set->buttons[set->count++] = (touch_thumb_button){
        tap_binding, long_binding
    };
}

static void sdl_touch_thumb_collect_buttons(touch_thumb_button_set* set)
{
    bool ranged;
    bool quick_throw;
    bool space_action;
    bool pickup_action;
    touch_thumb_button primary = {
        GAMEPAD_BIND_NONE, GAMEPAD_BIND_NONE
    };
    touch_thumb_button extras[SDL_TOUCH_THUMB_GAMEPLAY_ACTION_COUNT];
    int extra_count = 0;

    if (!set)
        return;
    *set = (touch_thumb_button_set){ 0 };

    if (sdl_touch_thumb_description_open()) {
        for (int i = 0; i < g_description_overlay.footer_action_count; i++) {
            int key = g_description_overlay.footer_actions[i].key;

            if (key != 0)
                sdl_touch_thumb_append_button(set, key, GAMEPAD_BIND_NONE);
        }
        return;
    }

    ranged = sdl_touch_thumb_ranged_mode_active();
    quick_throw = sdl_touch_thumb_quick_throw_active();
    if (sdl_touch_thumb_fire_targeting_active()) {
        if (ranged || quick_throw) {
            sdl_touch_thumb_append_button(set,
                SDL_TOUCH_THUMB_BIND_FIRE_SELECTED, GAMEPAD_BIND_NONE);
        }
        return;
    }

    space_action = sdl_touch_thumb_space_action(NULL, NULL, 0);
    pickup_action = sdl_touch_thumb_pickup_action();

    if (ranged) {
        primary = (touch_thumb_button){
            SDL_TOUCH_THUMB_BIND_FIRE_SELECTED,
            SDL_TOUCH_THUMB_BIND_CHANGE_QUIVER
        };
        if (quick_throw) {
            extras[extra_count++] = (touch_thumb_button){
                SDL_TOUCH_THUMB_BIND_QUICK_THROW,
                SDL_TOUCH_THUMB_BIND_QUICK_THROW_TARGET
            };
        }
    } else if (quick_throw) {
        primary = (touch_thumb_button){
            SDL_TOUCH_THUMB_BIND_QUICK_THROW,
            SDL_TOUCH_THUMB_BIND_QUICK_THROW_TARGET
        };
    } else if (space_action) {
        primary = (touch_thumb_button){
            SDL_TOUCH_THUMB_BIND_SPACE_CONTEXT, GAMEPAD_BIND_NONE
        };
    } else if (pickup_action) {
        primary = (touch_thumb_button){ 'x', 'g' };
    }

    if (primary.tap_binding != SDL_TOUCH_THUMB_BIND_SPACE_CONTEXT
        && space_action)
    {
        extras[extra_count++] = (touch_thumb_button){
            SDL_TOUCH_THUMB_BIND_SPACE_CONTEXT, GAMEPAD_BIND_NONE
        };
    } else if (primary.tap_binding != 'x' && pickup_action) {
        extras[extra_count++] = (touch_thumb_button){ 'x', 'g' };
    }

    if (primary.tap_binding != GAMEPAD_BIND_NONE) {
        sdl_touch_thumb_append_button(set, primary.tap_binding,
            primary.long_binding);
    }
    sdl_touch_thumb_append_button(set, 'z', 'Z');
    set->gameplay_base_count = set->count;

    for (int i = 0; i < extra_count; i++) {
        sdl_touch_thumb_append_button(set, extras[i].tap_binding,
            extras[i].long_binding);
    }
}

static int sdl_touch_thumb_button_count(void)
{
    touch_thumb_button_set set;

    sdl_touch_thumb_collect_buttons(&set);
    return set.count;
}

int sdl_touch_thumb_button_binding(int index, bool long_press)
{
    touch_thumb_button_set set;

    sdl_touch_thumb_collect_buttons(&set);
    if (index < 0 || index >= set.count)
        return GAMEPAD_BIND_NONE;
    return long_press ? set.buttons[index].long_binding
                      : set.buttons[index].tap_binding;
}

static bool sdl_touch_thumb_button_has_binding(int index)
{
    return sdl_touch_thumb_button_binding(index, false) != GAMEPAD_BIND_NONE
        || sdl_touch_thumb_button_binding(index, true) != GAMEPAD_BIND_NONE;
}

bool sdl_touch_thumb_config_enabled(void)
{
    if (!sdl_touch_only_device_active())
        return false;
    if (!config.touch_thumb_enabled)
        return false;
    return sdl_touch_thumb_button_count() > 0;
}

static bool sdl_touch_thumb_description_open(void)
{
    return g_description_overlay.active && g_description_overlay.interactive;
}

bool sdl_touch_thumb_layout_active(void)
{
    if (!sdl_touch_thumb_config_enabled())
        return false;
    if (!sdl_mobile_portrait_layout_active()
        && !sdl_left_panel_pane_collapsed())
    {
        return false;
    }
    /* Normal gameplay. */
    if (sdl_mouse_gameplay_context_active()
        && sdl_left_panel_pane_runtime_active())
    {
        return true;
    }
    /* Fire targeting keeps the fire thumb button available as an accept/fire
     * control while the aim selector owns normal gameplay input. */
    if (sdl_touch_thumb_fire_targeting_active()
        && (sdl_touch_thumb_ranged_mode_active()
            || sdl_touch_thumb_quick_throw_active())
        && sdl_left_panel_pane_layout_enabled()
        && p_ptr && p_ptr->playing && !p_ptr->is_dead)
    {
        return true;
    }
    /* While an interactive item description popup is open the panel is still
     * laid out, but character_icky > 0 makes the presentation/runtime predicate
     * false; allow the buttons (so 'x' can morph into Wield/Use) as long as the
     * panel layout is enabled and we are alive in the dungeon. */
    if (sdl_touch_thumb_description_open()
        && sdl_left_panel_pane_layout_enabled()
        && p_ptr && p_ptr->playing && !p_ptr->is_dead)
    {
        return true;
    }
    return false;
}

static float sdl_touch_thumb_region_bottom(float x, float width, float top,
    const SDL_Rect* screen, float margin)
{
    SDL_Rect combat;
    float bottom;

    if (!screen)
        return top;

    bottom = (float)(screen->y + screen->h) - margin;
    if (sdl_combat_overlay_pane_current_rect(&combat)
        && combat.w > 0 && combat.h > 0)
    {
        bool overlaps_column = (float)combat.x < x + width
            && (float)(combat.x + combat.w) > x;
        float combat_top = (float)combat.y - margin;

        if (overlaps_column && combat_top > top && combat_top < bottom)
            bottom = combat_top;
    }

    return bottom;
}

static float sdl_touch_thumb_row_mode_width(const sdl_view* view,
    const sdl_left_panel_metrics* metrics)
{
    int content_cols = 0;

    if (!view || !metrics || metrics->cell_w <= 0)
        return 0.0f;

    for (int i = 0; i < metrics->compact_segment_count; i++) {
        int width = sdl_left_panel_compact_source_row_width_for_view(view,
            metrics->compact_source_rows[i], false);

        if (width <= 0)
            width = LEFT_PANEL_CONTENT_WID;
        if (width > LEFT_PANEL_CONTENT_WID)
            width = LEFT_PANEL_CONTENT_WID;
        if (width > content_cols)
            content_cols = width;
    }
    if (content_cols <= 0)
        content_cols = LEFT_PANEL_CONTENT_WID;

    /* Include the two border columns used by compact column mode. */
    return (float)((content_cols + 2) * metrics->cell_w);
}

bool sdl_touch_thumb_compute_rects(SDL_FRect* out_rects)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    sdl_left_panel_metrics metrics;
    SDL_FRect panel;
    SDL_Rect screen;
    float px;
    float pw;
    float top;
    float region_bottom;
    float margin;
    float inner_gap;
    float avail;
    float button_h;
    float min_button_h;
    float max_button_h;

    if (out_rects) {
        for (int i = 0; i < SDL_TOUCH_THUMB_BUTTON_COUNT; i++)
            out_rects[i] = (SDL_FRect){ 0 };
    }
    if (!sdl_touch_thumb_layout_active())
        return false;

    if (sdl_mobile_portrait_layout_active()) {
        SDL_Rect touch_column;
        float gap;
        float button_h;

        if (!sdl_mobile_portrait_control_regions(NULL, &touch_column))
        {
            return false;
        }
        gap = (float)touch_column.h / 48.0f;
        if (gap < 8.0f)
            gap = 8.0f;
        button_h = ((float)touch_column.h - gap) / 2.0f;
        if (button_h < 28.0f)
            return false;
        if (out_rects) {
            out_rects[0] = (SDL_FRect){
                (float)touch_column.x, (float)touch_column.y,
                (float)touch_column.w, button_h
            };
            out_rects[1] = (SDL_FRect){
                (float)touch_column.x,
                (float)touch_column.y + button_h + gap,
                (float)touch_column.w, button_h
            };
        }
        return true;
    }

    if (!view->term_ready || view->cell_w <= 0 || view->cell_h <= 0)
        return false;

    /* Derive the left-panel rect straight from its metrics rather than from
     * g_pane_rects[PANE_LEFT_PANEL]: the latter is zeroed while an item
     * description popup is open (the popup sets character_icky), but the panel
     * is still laid out and the metrics path works in both cases. */
    if (!sdl_left_panel_metrics_for_view(view, &metrics))
        return false;
    if (!sdl_left_panel_pane_rect_for_metrics(view, &metrics, &panel))
        return false;
    if (panel.w <= 0.0f || panel.h <= 0.0f)
        return false;

    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return false;

    margin = (float)screen.h / 120.0f;
    if (margin < 6.0f)
        margin = 6.0f;
    inner_gap = margin;

    px = panel.x;
    pw = panel.w;
    if (metrics.compact_row) {
        float row_mode_width = sdl_touch_thumb_row_mode_width(view, &metrics);

        if (row_mode_width > 0.0f && row_mode_width < pw) {
            enum pane_placement where = sdl_left_panel_pane_placement();

            if (sdl_left_panel_pane_placement_is_right(where))
                px += pw - row_mode_width;
            else if (sdl_left_panel_pane_placement_is_horizontal_center(where))
                px += (pw - row_mode_width) * 0.5f;
            pw = row_mode_width;
        }
    }
    top = panel.y + panel.h + margin;

    /* The gap ends at the combat pane when it sits in the same column, else at
     * the bottom of the usable screen. */
    region_bottom = sdl_touch_thumb_region_bottom(px, pw, top, &screen,
        margin);

    avail = region_bottom - top;
    min_button_h = (float)screen.h / 14.0f;
    if (min_button_h < 40.0f)
        min_button_h = 40.0f;
    max_button_h = (float)screen.h / 5.0f;

    if (avail < min_button_h * 2.0f + inner_gap)
        return false;

    button_h = (avail - inner_gap) / 2.0f;
    if (button_h > max_button_h)
        button_h = max_button_h;

    if (out_rects) {
        out_rects[0] = (SDL_FRect){ px, top, pw, button_h };
        out_rects[1] = (SDL_FRect){
            px, top + button_h + inner_gap, pw, button_h
        };
    }
    return true;
}

static int sdl_touch_thumb_visual_index(const touch_thumb_button_set* set,
    int slot, int columns)
{
    int extra_count;

    if (!set || slot < 0 || slot >= set->count)
        return -1;
    extra_count = set->count - set->gameplay_base_count;
    if (columns != 1 || set->gameplay_base_count <= 0 || extra_count <= 0)
        return slot;

    /* A single vertical stack keeps contextual extras above the established
     * primary and Wait/Rest pair.  Once columns are needed, the pair keeps the
     * first column and extras flow into the remaining measured cells. */
    if (slot < extra_count)
        return set->gameplay_base_count + slot;
    return slot - extra_count;
}

static bool sdl_touch_thumb_compute_runtime_rects(
    SDL_FRect* out_rects, int rect_count)
{
    touch_thumb_button_set set;
    SDL_FRect base[SDL_TOUCH_THUMB_BUTTON_COUNT];
    SDL_Rect screen;
    float gap;

    if (out_rects) {
        for (int i = 0; i < rect_count; i++)
            out_rects[i] = (SDL_FRect){ 0 };
    }
    if (!out_rects || rect_count < SDL_TOUCH_THUMB_RUNTIME_CAPACITY)
        return false;
    if (g_touch_target_layout.locked) {
        if (!g_touch_target_layout.thumb_valid)
            return false;
        for (int i = 0; i < SDL_TOUCH_THUMB_RUNTIME_CAPACITY; i++)
            out_rects[i] = g_touch_target_layout.thumb_rects[i];
        return true;
    }

    sdl_touch_thumb_collect_buttons(&set);
    if (set.count <= 0 || !sdl_touch_thumb_compute_rects(base))
        return false;

    gap = base[1].y - (base[0].y + base[0].h);
    if (gap < 4.0f)
        gap = base[0].w / 12.0f;
    if (gap < 4.0f)
        gap = 4.0f;

    /* With no contextual primary action, preserve the established lower
     * position of the lone Wait/Rest button. */
    if (set.count == 1 && set.gameplay_base_count == 1) {
        out_rects[0] = base[1];
        return true;
    }

    if (sdl_mobile_portrait_layout_active()) {
        if (sdl_touch_thumb_description_open()) {
            SDL_Rect wheel_region;
            SDL_Rect button_region;
            float grid_x;
            float grid_y;
            float grid_w;
            float grid_h;
            float button_w;
            float button_h;
            float preferred_w;
            float four_column_w;
            int columns;
            int rows;

            if (!sdl_mobile_portrait_control_regions(&wheel_region,
                    &button_region))
            {
                return false;
            }

            grid_x = (float)wheel_region.x;
            grid_w = (float)(button_region.x + button_region.w) - grid_x;
            preferred_w = base[0].w;
            columns = (int)((grid_w + gap) / (preferred_w + gap));
            if (columns < 1)
                columns = 1;
            if (columns > set.count)
                columns = set.count;
            /* Description actions may use the combined portrait control width.
             * Prefer four across when every target remains comfortably sized. */
            four_column_w = (grid_w - gap * 3.0f) / 4.0f;
            if (set.count >= 4 && four_column_w >= 48.0f)
                columns = 4;
            rows = (set.count + columns - 1) / columns;
            button_w = (grid_w - gap * (float)(columns - 1))
                / (float)columns;
            button_h = base[0].h;
            grid_h = button_h * (float)rows + gap * (float)(rows - 1);
            if (grid_h > (float)button_region.h) {
                button_h = ((float)button_region.h
                    - gap * (float)(rows - 1)) / (float)rows;
                grid_h = (float)button_region.h;
            }
            grid_y = (float)(button_region.y + button_region.h) - grid_h;

            if (button_h <= 0.0f || button_w <= 0.0f)
                return false;

            for (int index = 0; index < set.count; index++) {
                int row = index / columns;
                int col = index % columns;

                out_rects[index] = (SDL_FRect){
                    grid_x + (button_w + gap) * (float)col,
                    grid_y + (button_h + gap) * (float)row,
                    button_w,
                    button_h,
                };
            }
            return true;
        }

        {
            float column_h = base[1].y + base[1].h - base[0].y;
            float button_h = (column_h - gap * (float)(set.count - 1))
                / (float)set.count;

            if (button_h <= 0.0f)
                return false;
            for (int slot = 0; slot < set.count; slot++) {
                int index = sdl_touch_thumb_visual_index(&set, slot, 1);

                out_rects[index] = (SDL_FRect){
                    base[0].x,
                    base[0].y + (button_h + gap) * (float)slot,
                    base[0].w,
                    button_h,
                };
            }
        }
        return true;
    }

    {
        enum pane_placement where = sdl_left_panel_pane_placement();
        float region_bottom;
        float available_h;
        float min_button_h;
        float max_button_h;
        float button_h;
        float button_w;
        float side_space;
        float first_x;
        bool expand_left;
        int rows_fit;
        int rows;
        int columns;

        screen = sdl_get_layout_screen_rect();
        region_bottom = sdl_touch_thumb_region_bottom(base[0].x, base[0].w,
            base[0].y, &screen, gap);
        available_h = region_bottom - base[0].y;
        min_button_h = (float)screen.h / 14.0f;
        if (min_button_h < 40.0f)
            min_button_h = 40.0f;
        max_button_h = (float)screen.h / 5.0f;

        rows_fit = (int)((available_h + gap) / (min_button_h + gap));
        if (rows_fit < 1)
            rows_fit = 1;
        if (rows_fit > set.count)
            rows_fit = set.count;
        columns = (set.count + rows_fit - 1) / rows_fit;
        rows = (set.count + columns - 1) / columns;
        button_h = (available_h - gap * (float)(rows - 1)) / (float)rows;
        if (button_h > max_button_h)
            button_h = max_button_h;
        if (button_h <= 0.0f)
            return false;

        expand_left = sdl_left_panel_pane_placement_is_right(where);
        if (!expand_left
            && sdl_left_panel_pane_placement_is_horizontal_center(where))
        {
            float left_space = base[0].x - (float)screen.x;
            float right_space = (float)(screen.x + screen.w)
                - (base[0].x + base[0].w);
            expand_left = left_space > right_space;
        }
        side_space = expand_left
            ? base[0].x - (float)screen.x
            : (float)(screen.x + screen.w) - (base[0].x + base[0].w);
        button_w = (base[0].w + side_space
            - gap * (float)(columns - 1)) / (float)columns;
        if (button_w > base[0].w)
            button_w = base[0].w;
        if (button_w <= 0.0f)
            return false;
        first_x = expand_left ? base[0].x + base[0].w - button_w
                              : base[0].x;

        for (int slot = 0; slot < set.count; slot++) {
            int index = sdl_touch_thumb_visual_index(&set, slot, columns);
            int row = slot % rows;
            int col = slot / rows;
            float x = expand_left
                ? first_x - (button_w + gap) * (float)col
                : first_x + (button_w + gap) * (float)col;

            out_rects[index] = (SDL_FRect){
                x,
                base[0].y + (button_h + gap) * (float)row,
                button_w,
                button_h,
            };
        }
    }

    return true;
}

bool sdl_touch_thumb_current_bounds(SDL_FRect* out_bounds)
{
    SDL_FRect rects[SDL_TOUCH_THUMB_RUNTIME_CAPACITY];
    SDL_FRect bounds = { 0 };
    bool have_bounds = false;
    int button_count = sdl_touch_thumb_button_count();

    if (out_bounds)
        *out_bounds = (SDL_FRect){ 0 };
    if (!sdl_touch_thumb_compute_runtime_rects(
            rects, SDL_TOUCH_THUMB_RUNTIME_CAPACITY))
    {
        return false;
    }

    for (int i = 0; i < button_count; i++) {
        if (!sdl_touch_thumb_button_has_binding(i))
            continue;
        if (rects[i].w <= 0.0f || rects[i].h <= 0.0f)
            continue;

        if (!have_bounds) {
            bounds = rects[i];
            have_bounds = true;
        } else {
            float right = MAX(bounds.x + bounds.w,
                rects[i].x + rects[i].w);
            float bottom = MAX(bounds.y + bounds.h,
                rects[i].y + rects[i].h);

            bounds.x = MIN(bounds.x, rects[i].x);
            bounds.y = MIN(bounds.y, rects[i].y);
            bounds.w = right - bounds.x;
            bounds.h = bottom - bounds.y;
        }
    }

    if (!have_bounds)
        return false;
    if (out_bounds)
        *out_bounds = bounds;
    return true;
}

bool sdl_touch_thumb_point_to_button(float px, float py, int* out_index)
{
    SDL_FRect rects[SDL_TOUCH_THUMB_RUNTIME_CAPACITY];
    int button_count = sdl_touch_thumb_button_count();

    if (out_index)
        *out_index = -1;
    if (!sdl_touch_thumb_compute_runtime_rects(
            rects, SDL_TOUCH_THUMB_RUNTIME_CAPACITY))
    {
        return false;
    }

    for (int i = 0; i < button_count; i++) {
        if (!sdl_touch_thumb_button_has_binding(i))
            continue;
        if (rects[i].w <= 0.0f || rects[i].h <= 0.0f)
            continue;
        if (px >= rects[i].x && px < rects[i].x + rects[i].w
            && py >= rects[i].y && py < rects[i].y + rects[i].h)
        {
            if (out_index)
                *out_index = i;
            return true;
        }
    }
    return false;
}

static int sdl_touch_context_binding(int binding)
{
    int key = binding;
    int context_binding = (binding == INPUT_BIND_CONFIRM) ? ' ' : binding;

    if (sdl_touch_thumb_description_open()
        && sdl_description_overlay_has_footer_action(binding))
    {
        return binding;
    }
    if (binding == 'g' && sdl_touch_thumb_description_open())
        return ' ';

    (void)touch_shortcut_context_action(context_binding,
        sdl_touch_thumb_description_open(), &key, NULL, 0);
    return key;
}

static void sdl_touch_context_label_for_binding(int binding, char* buf,
    size_t buflen)
{
    char ctx[32];
    int context_binding = (binding == INPUT_BIND_CONFIRM) ? ' ' : binding;

    if (!buf || !buflen)
        return;
    if (binding == GAMEPAD_BIND_NONE) {
        SDL_strlcpy(buf, "Off", buflen);
        return;
    }
    if (sdl_touch_thumb_description_open()
        && sdl_description_overlay_footer_action_label(binding, buf, buflen))
    {
        return;
    }
    if (binding == SDL_TOUCH_THUMB_BIND_SPACE_CONTEXT) {
        if (sdl_touch_thumb_space_action(NULL, buf, buflen))
            return;
        SDL_strlcpy(buf, "Confirm", buflen);
        return;
    }
    if (binding == SDL_TOUCH_THUMB_BIND_FIRE_SELECTED) {
        if (sdl_touch_thumb_fire_targeting_active()) {
            SDL_strlcpy(buf,
                sdl_touch_thumb_ranged_mode_active()
                    ? "Fire at target" : "Throw at target",
                buflen);
            return;
        }
        SDL_strlcpy(buf,
            (player_selected_ranged_quiver_number() == 2)
                ? "Fire 2nd quiver" : "Fire 1st quiver",
            buflen);
        return;
    }
    if (binding == SDL_TOUCH_THUMB_BIND_CHANGE_QUIVER) {
        SDL_strlcpy(buf,
            (player_selected_ranged_quiver_number() == 2)
                ? "Change to 1st quiver"
                : "Change to 2nd quiver",
            buflen);
        return;
    }
    if (binding == SDL_TOUCH_THUMB_BIND_QUICK_THROW) {
        sdl_touch_thumb_throw_label(buf, buflen);
        return;
    }
    if (binding == SDL_TOUCH_THUMB_BIND_QUICK_THROW_TARGET) {
        SDL_strlcpy(buf, "Choose target", buflen);
        return;
    }
    if (binding == TOUCH_BIND_TOGGLE_TILES) {
        SDL_strlcpy(buf, get_sdl_tiles() ? "Change to ASCII"
                                        : "Change to Tiles", buflen);
        return;
    }
    if (binding == 'f') {
        SDL_strlcpy(buf, "Fire 1st quiver", buflen);
        return;
    }
    if (binding == 'F') {
        SDL_strlcpy(buf, "Fire 2nd quiver", buflen);
        return;
    }
    if (binding == 'g') {
        SDL_strlcpy(buf, "Pick Up", buflen);
        return;
    }
    /* Space/'x' adapt their name to the player's situation (stairs, item,
     * open description, ...). */
    if (touch_shortcut_context_action(context_binding,
            sdl_touch_thumb_description_open(),
            NULL, ctx, sizeof(ctx)))
    {
        SDL_strlcpy(buf, ctx, buflen);
        return;
    }
    binding_action_short(binding, buf, buflen);
}

static void sdl_touch_thumb_render_button(const SDL_FRect* rect, int index,
    bool pressed)
{
    SDL_Color frame = g_state.palette[TERM_WHITE];
    SDL_Color text = g_state.palette[TERM_WHITE];
    int tap_binding = sdl_touch_thumb_button_binding(index, false);
    int long_binding = sdl_touch_thumb_button_binding(index, true);
    char label[32];
    char* detail;

    /* Translucent body + border, in the style of the movement button-wheel. */
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, pressed ? 200 : 150);
    SDL_RenderFillRect(g_state.renderer, rect);

    frame.a = pressed ? 230 : 185;
    SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, frame.a);
    SDL_RenderRect(g_state.renderer, rect);

    text.a = pressed ? 255 : 245;
    sdl_touch_context_label_for_binding(tap_binding, label, sizeof(label));
    detail = sdl_touch_thumb_description_open() ? strstr(label, " (") : NULL;

    if (long_binding != GAMEPAD_BIND_NONE) {
        SDL_FRect main_rect = *rect;
        SDL_FRect long_rect = *rect;
        SDL_Color hint = g_state.palette[TERM_L_WHITE];
        char long_label[32];

        main_rect.h = rect->h * 0.56f;
        long_rect.y = rect->y + rect->h * 0.54f;
        long_rect.h = rect->h * 0.46f;

        sdl_touch_context_label_for_binding(long_binding, long_label,
            sizeof(long_label));
        hint.a = pressed ? 240 : 215;

        sdl_touch_pane_draw_button_text_scaled(&main_rect, NULL, label, text,
            0.40f, 0.60f);
        sdl_touch_pane_draw_button_text_scaled(&long_rect, NULL, long_label,
            hint, 0.36f, 0.52f);
    } else if (detail) {
        SDL_FRect main_rect = *rect;
        SDL_FRect detail_rect = *rect;
        SDL_Color hint = g_state.palette[TERM_L_WHITE];

        *detail = '\0';
        detail++;
        main_rect.h = rect->h * 0.60f;
        detail_rect.y = rect->y + rect->h * 0.56f;
        detail_rect.h = rect->h * 0.44f;
        hint.a = pressed ? 240 : 215;

        sdl_touch_pane_draw_button_text_scaled(&main_rect, NULL, label, text,
            0.40f, 0.60f);
        sdl_touch_pane_draw_button_text_scaled(&detail_rect, NULL, detail,
            hint, 0.34f, 0.52f);
    } else {
        sdl_touch_pane_draw_button_text_scaled(rect, NULL, label, text,
            0.40f, 0.64f);
    }
}

void sdl_touch_thumb_render(void)
{
    SDL_FRect rects[SDL_TOUCH_THUMB_RUNTIME_CAPACITY];
    int button_count = sdl_touch_thumb_button_count();

    if (g_touch_thumb_flash_button >= 0
        && SDL_GetTicksNS() >= g_touch_thumb_flash_until)
    {
        g_touch_thumb_flash_button = -1;
        g_touch_thumb_flash_until = 0;
    }

    if (!sdl_touch_thumb_compute_runtime_rects(
            rects, SDL_TOUCH_THUMB_RUNTIME_CAPACITY))
    {
        sdl_touch_thumb_cancel_press();
        return;
    }

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < button_count; i++) {
        bool pressed;

        if (!sdl_touch_thumb_button_has_binding(i))
            continue;
        if (rects[i].w <= 0.0f || rects[i].h <= 0.0f)
            continue;
        pressed = (g_touch_thumb_pressed_button == i)
            || (g_touch_thumb_flash_button == i);
        sdl_touch_thumb_render_button(&rects[i], i, pressed);
    }
}

void sdl_touch_thumb_cancel_press(void)
{
    if (!g_touch_thumb_press.active && g_touch_thumb_pressed_button < 0)
        return;

    g_touch_thumb_press.active = false;
    g_touch_thumb_press.long_press_enabled = false;
    g_touch_thumb_pressed_button = -1;
    g_state.need_present = true;
}

static void sdl_touch_thumb_queue_quiver_change(void)
{
    int next_quiver = (player_selected_ranged_quiver_number() == 2) ? 1 : 2;

    player_queue_ranged_quiver_mode(
        player_active_weapon_mode_for_quiver(next_quiver));
    sdl_enqueue_bypassed_command(CMD_ACTIVE_WEAPON_MODE);
}

static bool sdl_touch_thumb_send_internal_binding(int binding)
{
    int key = ' ';

    switch (binding) {
    case SDL_TOUCH_THUMB_BIND_SPACE_CONTEXT:
        if (!sdl_touch_thumb_space_action(&key, NULL, 0))
            return false;
        sdl_touch_pane_send_binding(key, false, false);
        return true;
    case SDL_TOUCH_THUMB_BIND_FIRE_SELECTED:
        Term_keypress(
            (player_selected_ranged_quiver_number() == 2) ? 'F' : 'f');
        return true;
    case SDL_TOUCH_THUMB_BIND_CHANGE_QUIVER:
        sdl_touch_thumb_queue_quiver_change();
        return true;
    case SDL_TOUCH_THUMB_BIND_QUICK_THROW:
        Term_keypress(KTRL('T'));
        return true;
    case SDL_TOUCH_THUMB_BIND_QUICK_THROW_TARGET:
        Term_keypress('t');
        return true;
    default:
        return false;
    }
}

static void sdl_touch_thumb_fire(int index, bool long_press)
{
    int binding = sdl_touch_thumb_button_binding(index, long_press);
    int key;

    /* A long press on a button with no long binding still does the tap action,
     * and vice versa, so a half-configured button is never a dead zone. */
    if (binding == GAMEPAD_BIND_NONE)
        binding = sdl_touch_thumb_button_binding(index, !long_press);
    if (binding == GAMEPAD_BIND_NONE)
        return;

    /* Description buttons are registered action keys, not configurable
     * gameplay shortcuts.  Send them straight to the overlay's inkey() loop so
     * keys such as 'i' cannot be intercepted as pane-opening commands. */
    if (sdl_touch_thumb_description_open()) {
        Term_keypress(binding);
        g_touch_thumb_flash_button = index;
        g_touch_thumb_flash_until = SDL_GetTicksNS() + 150000000ULL;
        g_state.need_present = true;
        return;
    }

    if (sdl_touch_thumb_send_internal_binding(binding)) {
        g_touch_thumb_flash_button = index;
        g_touch_thumb_flash_until = SDL_GetTicksNS() + 150000000ULL;
        g_state.need_present = true;
        return;
    }

    /* Resolve a contextual binding (Space/'x') to the key its current label
     * promises; non-contextual bindings are left untouched. */
    key = sdl_touch_context_binding(binding);

    sdl_touch_pane_send_binding(key, false, false);
    g_touch_thumb_flash_button = index;
    g_touch_thumb_flash_until = SDL_GetTicksNS() + 150000000ULL;
    g_state.need_present = true;
}

int sdl_touch_thumb_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_touch_thumb_press.active)
        return -1;
    if (!g_touch_thumb_press.long_press_enabled)
        return -1;

    elapsed = now_ns - g_touch_thumb_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_touch_thumb_flush_pending_press(Uint64 now_ns)
{
    int index;

    if (!g_touch_thumb_press.active)
        return false;
    if (!g_touch_thumb_press.long_press_enabled)
        return false;
    if (now_ns - g_touch_thumb_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    index = g_touch_thumb_press.slot;
    sdl_touch_thumb_cancel_press();
    sdl_touch_thumb_fire(index, true);
    return true;
}

bool sdl_touch_thumb_handle_pointer_down(float x, float y, bool mouse,
    SDL_FingerID finger_id)
{
    int index = -1;

    if (!sdl_touch_thumb_point_to_button(x, y, &index))
        return false;
    if (index < 0)
        return true;

    sdl_touch_thumb_cancel_press();
    g_touch_thumb_press.active = true;
    g_touch_thumb_press.mouse = mouse;
    g_touch_thumb_press.finger_id = finger_id;
    g_touch_thumb_press.panel = 0;
    g_touch_thumb_press.slot = index;
    g_touch_thumb_press.long_press_enabled =
        (sdl_touch_thumb_button_binding(index, true) != GAMEPAD_BIND_NONE);
    g_touch_thumb_press.start_x = x;
    g_touch_thumb_press.start_y = y;
    g_touch_thumb_press.start_time = SDL_GetTicksNS();
    g_touch_thumb_pressed_button = index;
    g_state.need_present = true;
    return true;
}

bool sdl_touch_thumb_handle_pointer_motion(float x, float y, bool mouse,
    SDL_FingerID finger_id)
{
    int index = -1;
    float dx;
    float dy;
    float threshold;

    if (!g_touch_thumb_press.active)
        return false;
    if (g_touch_thumb_press.mouse != mouse)
        return false;
    if (!mouse && g_touch_thumb_press.finger_id != finger_id)
        return false;

    dx = x - g_touch_thumb_press.start_x;
    dy = y - g_touch_thumb_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_touch_swipe_threshold_px();
    if (dx > threshold || dy > threshold) {
        sdl_touch_thumb_cancel_press();
        return true;
    }

    if (!sdl_touch_thumb_point_to_button(x, y, &index)
        || index != g_touch_thumb_press.slot)
    {
        sdl_touch_thumb_cancel_press();
    }
    return true;
}

bool sdl_touch_thumb_handle_pointer_up(float x, float y, bool mouse,
    SDL_FingerID finger_id)
{
    Uint64 press_time;
    bool long_fire;
    int index;
    int release_index = -1;

    if (!g_touch_thumb_press.active)
        return false;
    if (g_touch_thumb_press.mouse != mouse)
        return false;
    if (!mouse && g_touch_thumb_press.finger_id != finger_id)
        return false;

    press_time = SDL_GetTicksNS() - g_touch_thumb_press.start_time;
    long_fire = g_touch_thumb_press.long_press_enabled
        && (press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL);
    index = g_touch_thumb_press.slot;
    (void)sdl_touch_thumb_point_to_button(x, y, &release_index);
    sdl_touch_thumb_cancel_press();
    if (release_index != index)
        return true;

    sdl_touch_thumb_fire(index, long_fire);
    return true;
}

bool sdl_touch_round_layer_config_enabled(void)
{
    return sdl_touch_only_mobile_device_active()
        && (config.touch_round_movement_enabled
            || sdl_mobile_portrait_layout_active());
}

bool sdl_touch_round_layer_controls_active(void)
{
    return sdl_touch_round_layer_config_enabled()
        && !g_main_menu_overlay_active
        && (sdl_main_screen_click_shortcuts_active()
            || sdl_touch_round_aim_targeting_active());
}

bool sdl_touch_movement_point_blocked_by_overlay(float x, float y)
{
    SDL_Rect touch_rect;
    SDL_FRect panel;

    if (sdl_description_overlay_contains_point(x, y))
        return true;
    if (sdl_main_menu_pane_hit(x, y, NULL))
        return true;
    if (sdl_depth_menu_pane_hit_action(x, y, NULL)
        != SDL_DEPTH_PANE_HOVER_NONE)
    {
        return true;
    }
    if (sdl_status_depth_pane_current_rect(&panel)
        && sdl_point_in_frect(&panel, x, y))
    {
        return true;
    }
    if (sdl_touch_pane_current_rect(&touch_rect)
        && sdl_point_in_rect(&touch_rect, x, y))
    {
        return true;
    }
    if (sdl_touch_top_panel_layout_visible()
        && sdl_touch_top_panel_compute_layout(NULL, &panel)
        && sdl_point_in_frect(&panel, x, y))
    {
        return true;
    }

    return false;
}

static float sdl_touch_round_inner_radius_px(float radius)
{
    return radius * SDL_TOUCH_ROUND_INNER_RADIUS_FRAC;
}

/* Pull the [*top, *bottom] band in to clear the overlay panes anchored against
 * the right edge of the map (depth/log above, status/combat below), so a
 * right-anchored wheel lands in the open gap between them. Panes that do not
 * reach the right portion of the map are ignored, so moving one elsewhere
 * hands its space back to the wheel. */
static void sdl_touch_round_clip_band_to_panes(const SDL_Rect* bounds,
    float right_edge, float* top, float* bottom)
{
    status_pane_layout status_layout;
    float slop;
    float right_threshold;
    float mid_y;
    /* Keep the wheel from sitting flush against an adjacent pane (e.g. the
     * log layer above): leave a small visual margin between them. */
    const float pane_gap = 12.0f;
    SDL_Rect rects[5];
    int count = 0;
    SDL_Rect rect;
    SDL_FRect frect;

    if (!bounds || !top || !bottom)
        return;

    slop = (float)bounds->w * 0.12f;
    if (slop < 64.0f)
        slop = 64.0f;
    if (slop > 180.0f)
        slop = 180.0f;
    right_threshold = right_edge - slop;
    mid_y = (float)bounds->y + (float)bounds->h * 0.5f;

    if (sdl_depth_menu_pane_current_rect(&frect)) {
        rects[count++] = (SDL_Rect){ (int)frect.x, (int)frect.y,
            (int)frect.w, (int)frect.h };
    }
    if (sdl_status_depth_pane_current_rect(&frect)) {
        rects[count++] = (SDL_Rect){ (int)frect.x, (int)frect.y,
            (int)frect.w, (int)frect.h };
    }
    if (sdl_overlay_log_pane_current_rect(&rect))
        rects[count++] = rect;
    /*
     * The configured status pane is only an anchor.  Its rendered panel grows
     * as conditions are added, so reserve the live panel rather than the
     * one-row anchor or the wheel can sit underneath poison/bleeding/stun.
     */
    if (sdl_status_pane_layout(&status_layout)) {
        rects[count++] = (SDL_Rect){
            (int)status_layout.panel.x, (int)status_layout.panel.y,
            (int)status_layout.panel.w, (int)status_layout.panel.h
        };
    }
    if (sdl_combat_overlay_pane_current_rect(&rect))
        rects[count++] = rect;

    for (int i = 0; i < count; i++) {
        float pane_right = (float)(rects[i].x + rects[i].w);
        float pane_top = (float)rects[i].y;
        float pane_bottom = (float)(rects[i].y + rects[i].h);
        float pane_mid = (pane_top + pane_bottom) * 0.5f;

        if (rects[i].w <= 0 || rects[i].h <= 0)
            continue;
        if (pane_right < right_threshold)
            continue;

        if (pane_mid <= mid_y) {
            if (pane_bottom + pane_gap > *top)
                *top = pane_bottom + pane_gap;
        } else if (pane_top - pane_gap < *bottom) {
            *bottom = pane_top - pane_gap;
        }
    }
}

bool sdl_touch_round_compute_layout(float* out_cx, float* out_cy,
    float* out_radius, float* out_inner_radius, SDL_Rect* out_clip)
{
    SDL_Rect clip;
    SDL_Rect render_clip;
    float radius;
    float inner_radius;
    float margin;
    float max_radius;
    float cx;
    float cy;
    float right_edge;
    float right_margin;
    bool anchor_right;
    bool anchor_center = false;
    bool sized = false;

    if (g_touch_target_layout.locked) {
        if (!g_touch_target_layout.wheel_valid)
            return false;
        if (out_cx)
            *out_cx = g_touch_target_layout.wheel_cx;
        if (out_cy)
            *out_cy = g_touch_target_layout.wheel_cy;
        if (out_radius)
            *out_radius = g_touch_target_layout.wheel_radius;
        if (out_inner_radius)
            *out_inner_radius = g_touch_target_layout.wheel_inner_radius;
        if (out_clip)
            *out_clip = g_touch_target_layout.wheel_clip;
        return true;
    }

    if (!sdl_touch_round_compute_clip_rect(&clip))
        return false;
    if (!sdl_rect_has_area(&clip))
        return false;

    margin = 8.0f;
    anchor_right = sdl_touch_round_layer_config_enabled()
        || sdl_touch_pane_is_left_placement();
    render_clip = clip;
    right_edge = (float)(clip.x + clip.w);
    right_margin = margin;

    /* The round movement wheel is a right-edge control.  Right-side panes
     * choose the vertical lane, but transient overlay redraws must not resize
     * the wheel into a tiny top-gap control. */
    if (sdl_touch_round_layer_config_enabled()) {
        SDL_Rect screen = sdl_get_layout_screen_rect();
        SDL_Rect touch_rect;
        float band_top;
        float band_bottom;
        float band_h;
        float max_r_w;

        if (!sdl_rect_has_area(&screen))
            return false;

        if (sdl_mobile_portrait_layout_active()) {
            SDL_Rect wheel_region;
            float band_top;
            float band_bottom;
            float band_h;
            float max_r_w;

            if (!sdl_mobile_portrait_control_regions(&wheel_region, NULL))
            {
                return false;
            }
            band_top = (float)wheel_region.y + margin;
            band_bottom = (float)(wheel_region.y + wheel_region.h)
                - margin;
            band_h = band_bottom - band_top;
            if (band_h < 56.0f)
                return false;

            /* Portrait lets the wheel fill the whole lower-left region.  The
             * configured desktop radius is intentionally ignored here. */
            max_r_w = ((float)wheel_region.w - margin * 2.0f) * 0.5f;
            radius = MIN(max_r_w, band_h * 0.5f);
            if (radius < 28.0f)
                return false;

            render_clip = wheel_region;
            clip = render_clip;
            right_edge = (float)(wheel_region.x + wheel_region.w);
            right_margin = 0.0f;
            cx = (float)wheel_region.x
                + (float)wheel_region.w * 0.5f;
            cy = (band_top + band_bottom) * 0.5f;
            anchor_center = true;
            sized = true;
        }

        if (!sized)
            right_edge = (float)(screen.x + screen.w);
        if (!sdl_touch_pane_is_left_placement()
            && !sized && sdl_touch_pane_current_rect(&touch_rect))
        {
            right_edge = (float)touch_rect.x;
        }
        if (!sized && right_edge <= (float)screen.x + 56.0f)
            return false;

        if (!sized) {
            band_top = (float)screen.y + margin;
            band_bottom = (float)(screen.y + screen.h) - margin;
            sdl_touch_round_clip_band_to_panes(&screen, right_edge, &band_top,
                &band_bottom);
            band_h = band_bottom - band_top;
            if (band_h < 56.0f)
                return false;

            radius = band_h * 0.5f;
            max_r_w = (right_edge - (float)screen.x - margin * 2.0f) * 0.5f;
            if (max_r_w > 0.0f && radius > max_r_w)
                radius = max_r_w;
            if (radius < 28.0f)
                return false;

            render_clip = (SDL_Rect){
                .x = screen.x,
                .y = (int)band_top,
                .w = (int)(right_edge - (float)screen.x),
                .h = (int)(band_h + 0.5f),
            };
            if (!sdl_rect_has_area(&render_clip))
                return false;

            clip = render_clip;
            right_margin = 0.0f;
            cy = (band_top + band_bottom) * 0.5f;
            sized = true;
        }
    }

    if (!sized) {
        radius = sdl_touch_round_radius_px();
        margin = radius * 0.12f;
        if (margin < 8.0f)
            margin = 8.0f;

        max_radius = ((float)clip.w - margin * 2.0f) * 0.5f;
        if ((float)clip.h - margin * 2.0f < max_radius * 2.0f)
            max_radius = ((float)clip.h - margin * 2.0f) * 0.5f;
        if (max_radius > 0.0f && radius > max_radius)
            radius = max_radius;
        cy = (float)(clip.y + clip.h) - margin - radius;
        right_edge = (float)(clip.x + clip.w);
        right_margin = margin;
    }

    if (radius < 28.0f)
        return false;

    inner_radius = sdl_touch_round_inner_radius_px(radius);
    if (!anchor_center) {
        cx = anchor_right
            ? right_edge - right_margin - radius
            : (float)clip.x + margin + radius;
    }

    /* Clamp to the same rectangle that will be used for rendering. */
    if (cx - radius < (float)render_clip.x)
        cx = (float)render_clip.x + radius;
    if (cx + radius > right_edge)
        cx = right_edge - radius;
    if (cy - radius < (float)clip.y)
        cy = (float)clip.y + radius;
    if (cy + radius > (float)(clip.y + clip.h))
        cy = (float)(clip.y + clip.h) - radius;

    if (out_cx)
        *out_cx = cx;
    if (out_cy)
        *out_cy = cy;
    if (out_radius)
        *out_radius = radius;
    if (out_inner_radius)
        *out_inner_radius = inner_radius;
    if (out_clip)
        *out_clip = render_clip;

    return true;
}

void sdl_touch_target_layout_begin(void)
{
    touch_target_layout_state snapshot = { 0 };

    /* Target selectors temporarily change input ownership, button bindings,
     * and which overlay panes report live rectangles.  Snapshot the gameplay
     * geometry before that transition so the wheel and contextual buttons do
     * not jump when choosing a ranged, thrown, song, or other target. */
    snapshot.thumb_valid = sdl_touch_thumb_compute_runtime_rects(
        snapshot.thumb_rects, SDL_TOUCH_THUMB_RUNTIME_CAPACITY);
    snapshot.wheel_valid = sdl_touch_round_compute_layout(
        &snapshot.wheel_cx, &snapshot.wheel_cy, &snapshot.wheel_radius,
        &snapshot.wheel_inner_radius, &snapshot.wheel_clip);
    snapshot.locked = true;
    g_touch_target_layout = snapshot;
}

void sdl_touch_target_layout_end(void)
{
    g_touch_target_layout = (touch_target_layout_state){ 0 };
}

static bool sdl_touch_round_point_to_wheel(float x, float y,
    float* out_cx, float* out_cy, float* out_radius,
    float* out_inner_radius, bool* out_outer_button, int* out_dir,
    float* out_dist)
{
    float cx;
    float cy;
    float radius;
    float inner_radius;
    float dx;
    float dy;
    float dist;
    bool outer_button;

    if (out_outer_button)
        *out_outer_button = false;
    if (out_dir)
        *out_dir = 0;
    if (out_dist)
        *out_dist = 0.0f;

    if (!sdl_touch_round_compute_layout(&cx, &cy, &radius, &inner_radius,
            NULL))
    {
        return false;
    }

    dx = x - cx;
    dy = y - cy;
    dist = SDL_sqrtf(dx * dx + dy * dy);
    if (dist > radius)
        return false;

    outer_button = (dist >= inner_radius);
    if (out_cx)
        *out_cx = cx;
    if (out_cy)
        *out_cy = cy;
    if (out_radius)
        *out_radius = radius;
    if (out_inner_radius)
        *out_inner_radius = inner_radius;
    if (out_outer_button)
        *out_outer_button = outer_button;
    if (out_dir
        && (outer_button
            || dist >= inner_radius * SDL_TOUCH_ROUND_DRAG_THRESHOLD_FRAC))
    {
        *out_dir = sdl_touch_round_dir_for_delta(dx, dy);
    }
    if (out_dist)
        *out_dist = dist;

    return true;
}

/* True when (x,y) lies in the area the wheel can occupy: the map view, plus
 * the right-edge strip used by the round movement wheel. */
static bool sdl_touch_round_point_in_bounds(float x, float y)
{
    SDL_Rect clip;

    if (sdl_point_in_view_rect(PANE_MAIN, x, y))
        return true;
    if (!sdl_touch_round_layer_config_enabled())
        return false;
    if (!sdl_touch_round_compute_layout(NULL, NULL, NULL, NULL, &clip))
        return false;
    return x >= (float)clip.x && x < (float)(clip.x + clip.w)
        && y >= (float)clip.y && y < (float)(clip.y + clip.h);
}

bool sdl_touch_round_point_excluded(float x, float y)
{
    if (!sdl_touch_round_layer_controls_active())
        return false;
    if (!sdl_touch_round_point_in_bounds(x, y))
        return true;
    if (sdl_touch_movement_point_blocked_by_overlay(x, y))
        return true;

    return !sdl_touch_round_point_to_wheel(x, y, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL);
}

float sdl_touch_round_radius_px(void)
{
    const SDL_Rect* main_rect = &g_views[PANE_MAIN].rect;
    int min_dim;
    int cell_px;
    float radius;

    if (!sdl_rect_has_area(main_rect))
        return 72.0f;

    min_dim = (main_rect->w < main_rect->h) ? main_rect->w : main_rect->h;
    cell_px = (g_views[PANE_MAIN].cell_w > g_views[PANE_MAIN].cell_h)
        ? g_views[PANE_MAIN].cell_w
        : g_views[PANE_MAIN].cell_h;
    if (cell_px <= 0)
        cell_px = TILE_SIZE;

    radius = (float)cell_px * 4.5f;
    if (radius < 72.0f)
        radius = 72.0f;
    if (radius > 150.0f)
        radius = 150.0f;
    if (min_dim > 0 && radius > (float)min_dim * 0.42f)
        radius = (float)min_dim * 0.42f;
    if (radius < 32.0f)
        radius = 32.0f;

    return radius;
}

bool sdl_touch_round_compute_clip_rect(SDL_Rect* out_clip)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int tile_cols = use_bigtile ? 2 : 1;
    int inner_cols;
    int inner_rows;

    if (!out_clip)
        return false;
    if (!sdl_rect_has_area(&view->rect))
        return false;
    if (!view->term_ready || !view->canvas
        || view->cell_w <= 0 || view->cell_h <= 0
        || view->cols <= 0 || view->rows <= 0)
    {
        *out_clip = view->rect;
        return true;
    }
    if (SCREEN_WID <= 2 || SCREEN_HGT <= 2)
        return false;

    inner_cols = (SCREEN_WID - 2) * tile_cols;
    inner_rows = SCREEN_HGT - 2;
    if (inner_cols <= 0 || inner_rows <= 0)
        return false;

    {
        SDL_FRect rect;

        if (!sdl_main_cell_rect(COL_MAP + tile_cols, ROW_MAP + 1,
                inner_cols, inner_rows, &rect))
        {
            return false;
        }

        *out_clip = (SDL_Rect){
            .x = (int)rect.x,
            .y = (int)rect.y,
            .w = (int)rect.w,
            .h = (int)rect.h,
        };
    }

    return sdl_rect_has_area(out_clip);
}

int sdl_touch_round_dir_for_delta(float dx, float dy)
{
    float abs_x = dx;
    float abs_y = dy;

    if (abs_x < 0.0f)
        abs_x = -abs_x;
    if (abs_y < 0.0f)
        abs_y = -abs_y;
    if (abs_x <= 0.0f && abs_y <= 0.0f)
        return 0;

    /* Approximate 22.5 degree sector boundaries without trigonometry. */
    if (abs_x > abs_y * 2.41421356f)
        return (dx >= 0.0f) ? 6 : 4;
    if (abs_y > abs_x * 2.41421356f)
        return (dy >= 0.0f) ? 2 : 8;

    if (dy < 0.0f)
        return (dx < 0.0f) ? 7 : 9;
    return (dx < 0.0f) ? 1 : 3;
}

static bool sdl_touch_round_drag_is_run(float dist, float inner_radius,
    float radius)
{
    float button_distance;

    if (inner_radius <= 0.0f || radius <= inner_radius)
        return false;

    /* Direction buttons are drawn halfway through the outer ring.  Require a
     * drag 1.3 times as far from the wheel centre as that button centre. */
    button_distance = (inner_radius + radius) * 0.5f;
    return dist >= button_distance
        * SDL_TOUCH_ROUND_RUN_BUTTON_DISTANCE_MULT;
}

void sdl_touch_round_send_dir(int dir, bool ctrl, bool run)
{
    if (dir < 1 || dir > 9 || dir == 5)
        return;

    g_touch_round_last_dir = dir;
    sdl_gamepad_send_direction_mods(dir, run, ctrl, false);
}

void sdl_touch_round_cancel_press(void)
{
    if (!g_touch_round_press.active)
        return;

    g_touch_round_press.active = false;
    g_touch_round_press.finger_id = 0;
    g_touch_round_press.center_x = 0.0f;
    g_touch_round_press.center_y = 0.0f;
    g_touch_round_press.current_x = 0.0f;
    g_touch_round_press.current_y = 0.0f;
    g_touch_round_press.radius = 0.0f;
    g_touch_round_press.inner_radius = 0.0f;
    g_touch_round_press.selected_dir = 0;
    g_touch_round_press.button_press = false;
    g_touch_round_press.button_dir = 0;
    g_touch_round_press.start_time = 0;
    g_state.need_present = true;
}

bool sdl_touch_round_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id)
{
    float cx;
    float cy;
    float radius;
    float inner_radius;
    bool outer_button;
    int dir;

    if (!sdl_touch_round_layer_controls_active())
        return false;
    /* The aim command bar owns its buttons even when the direction wheel
     * overlaps the same part of the map. */
    if (sdl_touch_round_aim_targeting_active()
        && sdl_unified_look_prompt_contains_point(x, y))
    {
        return false;
    }
    if (sdl_touch_movement_point_blocked_by_overlay(x, y))
        return false;
    if (!sdl_touch_round_point_in_bounds(x, y))
        return false;
    if (!sdl_touch_round_point_to_wheel(x, y, &cx, &cy, &radius,
            &inner_radius, &outer_button, &dir, NULL))
    {
        return false;
    }

    sdl_menu_touch_cancel();
    sdl_menu_scroll_cancel();
    sdl_map_touch_cancel_press();
    sdl_pointer_attack_cancel_touch_press();
    sdl_touch_zone_cancel_press();
    sdl_touch_top_panel_cancel_press();
    sdl_touch_swipe_cancel();
    sdl_touch_round_cancel_press();

    g_touch_round_press.active = true;
    g_touch_round_press.finger_id = finger_id;
    g_touch_round_press.center_x = cx;
    g_touch_round_press.center_y = cy;
    g_touch_round_press.current_x = x;
    g_touch_round_press.current_y = y;
    g_touch_round_press.radius = radius;
    g_touch_round_press.inner_radius = inner_radius;
    g_touch_round_press.selected_dir = outer_button ? dir : 0;
    g_touch_round_press.button_press = outer_button;
    g_touch_round_press.button_dir = outer_button ? dir : 0;
    g_touch_round_press.start_time = SDL_GetTicksNS();
    g_state.need_present = true;
    return true;
}

bool sdl_touch_round_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float dist;
    float radius;

    if (!g_touch_round_press.active
        || g_touch_round_press.finger_id != finger_id)
    {
        return false;
    }

    dx = x - g_touch_round_press.center_x;
    dy = y - g_touch_round_press.center_y;
    dist = SDL_sqrtf(dx * dx + dy * dy);
    radius = g_touch_round_press.radius;
    if (radius <= 0.0f)
        radius = sdl_touch_round_radius_px();

    g_touch_round_press.current_x = x;
    g_touch_round_press.current_y = y;
    if (g_touch_round_press.button_press) {
        if (dist >= g_touch_round_press.inner_radius
            && dist <= radius * (1.0f + SDL_TOUCH_ROUND_HIT_SLOP_FRAC))
        {
            g_touch_round_press.selected_dir =
                sdl_touch_round_dir_for_delta(dx, dy);
            g_touch_round_press.button_dir =
                g_touch_round_press.selected_dir;
        } else if (dist >= g_touch_round_press.inner_radius
                * SDL_TOUCH_ROUND_DRAG_THRESHOLD_FRAC) {
            g_touch_round_press.selected_dir =
                sdl_touch_round_dir_for_delta(dx, dy);
            g_touch_round_press.button_dir = 0;
        } else {
            g_touch_round_press.selected_dir = 0;
            g_touch_round_press.button_dir = 0;
        }
    } else if (dist >= g_touch_round_press.inner_radius
            * SDL_TOUCH_ROUND_DRAG_THRESHOLD_FRAC) {
        g_touch_round_press.selected_dir = sdl_touch_round_dir_for_delta(dx, dy);
    } else {
        g_touch_round_press.selected_dir = 0;
    }

    g_state.need_present = true;
    return true;
}

bool sdl_touch_round_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float dist;
    float radius;
    Uint64 press_time;
    int dir = 0;
    bool ctrl = false;
    bool run = false;

    if (!g_touch_round_press.active
        || g_touch_round_press.finger_id != finger_id)
    {
        return false;
    }

    dx = x - g_touch_round_press.center_x;
    dy = y - g_touch_round_press.center_y;
    dist = SDL_sqrtf(dx * dx + dy * dy);
    radius = g_touch_round_press.radius;
    if (radius <= 0.0f)
        radius = sdl_touch_round_radius_px();
    press_time = SDL_GetTicksNS() - g_touch_round_press.start_time;

    if (g_touch_round_press.button_press) {
        if (dist >= g_touch_round_press.inner_radius
            && dist <= radius * (1.0f + SDL_TOUCH_ROUND_HIT_SLOP_FRAC))
        {
            dir = sdl_touch_round_dir_for_delta(dx, dy);
        } else if (g_touch_round_press.button_dir) {
            dir = g_touch_round_press.button_dir;
        }
        /* Holding a direct direction button long enough makes it act like
         * Ctrl+direction (alter: attack/tunnel/disarm...), matching the touch
         * command pane.  Only direct button presses get this; the inner-disc
         * "drag to move" path stays plain movement. */
        if (dir && press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
            ctrl = true;
    } else if (!sdl_touch_round_aim_targeting_active()
        && dist <= g_touch_round_press.inner_radius
            * SDL_TOUCH_ROUND_CENTER_REPEAT_FRAC) {
        if (press_time < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
            dir = g_touch_round_last_dir;
    } else if (dist >= g_touch_round_press.inner_radius
            * SDL_TOUCH_ROUND_DRAG_THRESHOLD_FRAC) {
        dir = sdl_touch_round_dir_for_delta(dx, dy);
        run = sdl_touch_round_drag_is_run(dist,
            g_touch_round_press.inner_radius, radius);
    } else if (g_touch_round_press.selected_dir) {
        dir = g_touch_round_press.selected_dir;
    }

    /* During aim selection the wheel is a direct firing control, never a
     * run or Ctrl+direction gameplay gesture. */
    if (sdl_touch_round_aim_targeting_active()) {
        ctrl = false;
        run = false;
    }

    sdl_touch_round_cancel_press();
    if (dir)
        sdl_touch_round_send_dir(dir, ctrl, run);
    return true;
}

/* Time (ms) until a held direct button press crosses the long-press threshold
 * and turns into a Ctrl+direction action, so the event loop can wake to redraw
 * the wheel with its Ctrl preview.  -1 once there is nothing to wait for. */
int sdl_touch_round_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_touch_round_press.active)
        return -1;
    if (!g_touch_round_press.button_press)
        return -1;

    elapsed = now_ns - g_touch_round_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return -1;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

/* Once a direct button press has been held past the long-press threshold,
 * request a redraw so the wheel shows the Ctrl target square and action label.
 * The Ctrl+direction key only fires on release, so this neither sends a key nor
 * cancels the press. */
void sdl_touch_round_flush_pending_highlight(Uint64 now_ns)
{
    if (!g_touch_round_press.active)
        return;
    if (!g_touch_round_press.button_press)
        return;
    if (now_ns - g_touch_round_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return;
    }

    g_state.need_present = true;
}

void sdl_touch_round_draw_circle(float cx, float cy, float radius,
    SDL_Color color)
{
    static const float points[TOUCH_ROUND_CIRCLE_SEGMENTS][2] = {
        { 1.000000f, 0.000000f }, { 0.980785f, 0.195090f },
        { 0.923880f, 0.382683f }, { 0.831470f, 0.555570f },
        { 0.707107f, 0.707107f }, { 0.555570f, 0.831470f },
        { 0.382683f, 0.923880f }, { 0.195090f, 0.980785f },
        { 0.000000f, 1.000000f }, { -0.195090f, 0.980785f },
        { -0.382683f, 0.923880f }, { -0.555570f, 0.831470f },
        { -0.707107f, 0.707107f }, { -0.831470f, 0.555570f },
        { -0.923880f, 0.382683f }, { -0.980785f, 0.195090f },
        { -1.000000f, 0.000000f }, { -0.980785f, -0.195090f },
        { -0.923880f, -0.382683f }, { -0.831470f, -0.555570f },
        { -0.707107f, -0.707107f }, { -0.555570f, -0.831470f },
        { -0.382683f, -0.923880f }, { -0.195090f, -0.980785f },
        { 0.000000f, -1.000000f }, { 0.195090f, -0.980785f },
        { 0.382683f, -0.923880f }, { 0.555570f, -0.831470f },
        { 0.707107f, -0.707107f }, { 0.831470f, -0.555570f },
        { 0.923880f, -0.382683f }, { 0.980785f, -0.195090f },
    };

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        color.a);
    for (int i = 0; i < TOUCH_ROUND_CIRCLE_SEGMENTS; i++) {
        int next = (i + 1) % TOUCH_ROUND_CIRCLE_SEGMENTS;

        SDL_RenderLine(g_state.renderer,
            cx + points[i][0] * radius,
            cy + points[i][1] * radius,
            cx + points[next][0] * radius,
            cy + points[next][1] * radius);
    }
}

void sdl_touch_round_draw_sector_lines(float cx, float cy,
    float inner_radius, float outer_radius, SDL_Color color)
{
    static const float sector_points[8][2] = {
        { 0.923880f, 0.382683f },
        { 0.382683f, 0.923880f },
        { -0.382683f, 0.923880f },
        { -0.923880f, 0.382683f },
        { -0.923880f, -0.382683f },
        { -0.382683f, -0.923880f },
        { 0.382683f, -0.923880f },
        { 0.923880f, -0.382683f },
    };

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        color.a);
    for (int i = 0; i < (int)N_ELEMENTS(sector_points); i++) {
        float dx = sector_points[i][0];
        float dy = sector_points[i][1];

        SDL_RenderLine(g_state.renderer,
            cx + dx * inner_radius,
            cy + dy * inner_radius,
            cx + dx * outer_radius,
            cy + dy * outer_radius);
    }
}

bool sdl_touch_round_dir_to_map_rect(int dir, SDL_FRect* out_rect)
{
    int map_y;
    int map_x;
    int map_row;
    int map_col;
    int term_col;
    int term_row;
    int tile_cols = use_bigtile ? 2 : 1;

    if (!out_rect)
        return false;
    if (!p_ptr || dir < 1 || dir > 9 || dir == 5)
        return false;

    map_y = p_ptr->py + ddy[dir];
    map_x = p_ptr->px + ddx[dir];
    if (!in_bounds(map_y, map_x))
        return false;
    if (!panel_contains(map_y, map_x))
        return false;

    map_row = map_y - p_ptr->wy;
    map_col = map_x - p_ptr->wx;
    if (map_row < 0 || map_col < 0
        || map_row >= SCREEN_HGT || map_col >= SCREEN_WID)
    {
        return false;
    }

    term_row = ROW_MAP + map_row;
    term_col = COL_MAP + map_col * tile_cols;
    return sdl_main_cell_rect(term_col, term_row, tile_cols, 1, out_rect);
}

void sdl_touch_round_render_target_square(int dir, bool ctrl, bool run)
{
    SDL_FRect rect;
    int map_y;
    int map_x;
    int m_idx = 0;
    SDL_Color accent = g_state.palette[TERM_YELLOW];

    if (!sdl_touch_round_dir_to_map_rect(dir, &rect))
        return;

    if (ctrl) {
        accent = g_state.palette[TERM_L_RED];
    } else if (run) {
        accent = g_state.palette[TERM_L_GREEN];
    } else {
        map_y = p_ptr->py + ddy[dir];
        map_x = p_ptr->px + ddx[dir];
        if (sdl_mouse_grid_has_visible_monster(map_y, map_x, &m_idx)
            && sdl_mouse_monster_is_friendly(m_idx))
        {
            accent = g_state.palette[TERM_L_GREEN];
        }
    }

    SDL_SetRenderDrawColor(g_state.renderer, accent.r, accent.g, accent.b,
        84);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, accent.r, accent.g, accent.b,
        190);
    SDL_RenderRect(g_state.renderer, &rect);
}

const char* sdl_touch_round_dir_label(int dir)
{
    switch (dir) {
    case 1: return "SW";
    case 2: return "S";
    case 3: return "SE";
    case 4: return "W";
    case 6: return "E";
    case 7: return "NW";
    case 8: return "N";
    case 9: return "NE";
    default: return "";
    }
}

const char* sdl_touch_round_ctrl_action_for_dir(int dir)
{
    int y;
    int x;
    int feat;
    bool is_marked;
    bool is_visible;

    if (!p_ptr || dir < 1 || dir > 9 || dir == 5)
        return "Alter";

    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];
    if (!in_bounds(y, x))
        return "Alter";

    if (sdl_mouse_grid_has_visible_monster(y, x, NULL))
        return "Attack";

    is_marked = (cave_info[y][x] & CAVE_MARK) != 0;
    is_visible = (cave_info[y][x] & CAVE_SEEN) != 0;
    if (!is_marked && !is_visible)
        return "Strike";

    feat = is_marked ? cave_feat[y][x] : FEAT_NONE;

    if (cave_wall_bold(y, x))
        return "Tunnel";
    if (cave_known_closed_door_bold(y, x))
        return "Bash door";
    if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
        return "Disarm trap";

    if (cave_o_idx[y][x]) {
        object_type* o_ptr = &o_list[cave_o_idx[y][x]];

        if (o_ptr->marked && o_ptr->tval == TV_CHEST) {
            if (chest_trap_minigame && o_ptr->pval != 0)
                return "Handle chest";
            if ((o_ptr->pval > 0) && object_chest_trap_flags(o_ptr)
                && object_known_p(o_ptr))
            {
                return "Disarm chest";
            }

            return "Open chest";
        }

        if (o_ptr->marked && o_ptr->tval == TV_SKELETON
            && !object_is_searched_skeleton(o_ptr))
        {
            return "Search skeleton";
        }
    }

    if (feat == FEAT_OPEN)
        return "Close door";

    return "Strike";
}

void sdl_touch_round_ctrl_action_label(int dir, char* buf, size_t buflen)
{
    const char* dir_label;
    const char* action;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    dir_label = sdl_touch_round_dir_label(dir);
    if (!dir_label[0])
        return;

    action = sdl_touch_round_ctrl_action_for_dir(dir);
    strnfmt(buf, buflen, "Ctrl+%s: %s", dir_label, action);
}

static void sdl_touch_round_render_action_label(const char* label,
    SDL_Color text, float radius, const SDL_Rect* clip)
{
    SDL_Color border = text;
    SDL_FRect rect;
    float label_w;
    float label_h;
    float margin;
    float min_y;
    float max_y;

    if (!clip || !label || !label[0])
        return;

    margin = 8.0f;
    label_w = radius * 2.10f;
    if (label_w < 170.0f)
        label_w = 170.0f;
    if (label_w > (float)clip->w - margin * 2.0f)
        label_w = (float)clip->w - margin * 2.0f;
    if (label_w <= 0.0f)
        return;
    label_h = radius * 0.36f;
    if (label_h < 42.0f)
        label_h = 42.0f;
    if (label_h > 72.0f)
        label_h = 72.0f;

    rect = (SDL_FRect){
        .x = g_touch_round_press.center_x - label_w * 0.5f,
        .y = g_touch_round_press.center_y - radius - label_h - margin,
        .w = label_w,
        .h = label_h,
    };

    min_y = (float)clip->y + margin;
    max_y = (float)(clip->y + clip->h) - label_h - margin;
    if (max_y < min_y)
        max_y = min_y;
    if (rect.y < min_y)
        rect.y = g_touch_round_press.center_y - radius + margin;
    if (rect.y > max_y)
        rect.y = max_y;
    if (rect.x < (float)clip->x + margin)
        rect.x = (float)clip->x + margin;
    if (rect.x + rect.w > (float)(clip->x + clip->w) - margin)
        rect.x = (float)(clip->x + clip->w) - rect.w - margin;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 172);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
        232);
    SDL_RenderRect(g_state.renderer, &rect);
    sdl_touch_pane_draw_button_text_scaled(&rect, NULL, label, text, 0.48f,
        0.76f);
}

void sdl_touch_round_render_ctrl_action_label(int dir, float radius,
    const SDL_Rect* clip)
{
    char label[48];

    sdl_touch_round_ctrl_action_label(dir, label, sizeof(label));
    sdl_touch_round_render_action_label(label,
        g_state.palette[TERM_L_RED], radius, clip);
}

static void sdl_touch_round_render_run_action_label(int dir, float radius,
    const SDL_Rect* clip)
{
    const char* dir_label;
    char label[48];

    if (!clip || dir <= 0)
        return;

    dir_label = sdl_touch_round_dir_label(dir);
    if (!dir_label[0])
        return;
    strnfmt(label, sizeof(label), "Run %s", dir_label);
    sdl_touch_round_render_action_label(label,
        g_state.palette[TERM_L_GREEN], radius, clip);
}

static void sdl_touch_round_render_button_arrows(float cx, float cy,
    float inner_radius, float radius, int target_dir, bool active,
    bool run_preview)
{
    static const int dirs[] = { 7, 8, 9, 4, 6, 1, 2, 3 };
    SDL_Color arrow = g_state.palette[TERM_WHITE];
    SDL_Color chosen = g_state.palette[
        run_preview ? TERM_L_GREEN : TERM_YELLOW];
    float mid = (inner_radius + radius) * 0.5f;
    float ring_w = radius - inner_radius;
    float button_size = ring_w * 1.14f;

    if (button_size < 28.0f)
        button_size = 28.0f;
    if (button_size > radius * 0.54f)
        button_size = radius * 0.54f;

    for (int i = 0; i < (int)N_ELEMENTS(dirs); i++) {
        int dir = dirs[i];
        float ux = (float)ddx[dir];
        float uy = (float)ddy[dir];
        float len = SDL_sqrtf(ux * ux + uy * uy);
        SDL_Color color;
        SDL_FRect rect;

        if (len <= 0.0f)
            continue;
        ux /= len;
        uy /= len;

        color = (dir == target_dir) ? chosen : arrow;
        color.a = (dir == target_dir) ? 242 : (active ? 172 : 128);
        rect = (SDL_FRect){
            .x = cx + ux * mid - button_size * 0.5f,
            .y = cy + uy * mid - button_size * 0.5f,
            .w = button_size,
            .h = button_size,
        };

        if (dir == target_dir)
            sdl_touch_round_draw_circle(rect.x + rect.w * 0.5f,
                rect.y + rect.h * 0.5f, button_size * 0.42f, color);
        sdl_touch_pane_draw_arrow(&rect, '0' + dir, color);
    }
}

void sdl_touch_round_render(void)
{
    SDL_Color frame = g_state.palette[TERM_WHITE];
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    SDL_Color selected = g_state.palette[TERM_YELLOW];
    SDL_Color line_color;
    SDL_FRect center_arrow;
    SDL_Rect clip;
    SDL_Rect map_clip;
    bool active = g_touch_round_press.active;
    float cx;
    float cy;
    float radius;
    float inner_radius;
    float dx;
    float dy;
    float dist;
    float end_x;
    float end_y;
    Uint64 press_time;
    bool center_repeat;
    bool ctrl_preview;
    bool run_preview;
    bool aim_targeting = sdl_touch_round_aim_targeting_active();
    int target_dir;
    int dir;

    if (!sdl_rect_has_area(&g_views[PANE_MAIN].rect))
        return;
    if (g_main_menu_overlay_active) {
        sdl_touch_round_cancel_press();
        return;
    }

    if (active) {
        if (!sdl_touch_round_compute_layout(NULL, NULL, NULL, NULL, &clip))
            return;
        cx = g_touch_round_press.center_x;
        cy = g_touch_round_press.center_y;
        radius = g_touch_round_press.radius;
        inner_radius = g_touch_round_press.inner_radius;
        if (radius <= 0.0f)
            radius = sdl_touch_round_radius_px();
        if (inner_radius <= 0.0f)
            inner_radius = sdl_touch_round_inner_radius_px(radius);
    } else {
        if (!sdl_touch_round_layer_config_enabled())
            return;
        if (!sdl_mouse_gameplay_context_active() && !aim_targeting)
            return;
        if (!sdl_touch_round_compute_layout(&cx, &cy, &radius,
                &inner_radius, &clip))
        {
            return;
        }
    }

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    if (aim_targeting)
        accent = g_state.palette[TERM_L_RED];
    frame.a = (active || aim_targeting) ? 166 : 112;
    accent.a = (active || aim_targeting) ? 190 : 138;
    selected.a = 230;

    dx = active ? g_touch_round_press.current_x - cx : 0.0f;
    dy = active ? g_touch_round_press.current_y - cy : 0.0f;
    dist = SDL_sqrtf(dx * dx + dy * dy);
    press_time = active ? SDL_GetTicksNS() - g_touch_round_press.start_time : 0;
    center_repeat = active && !aim_targeting
        && !g_touch_round_press.button_press
        && dist <= inner_radius * SDL_TOUCH_ROUND_CENTER_REPEAT_FRAC
        && press_time < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL
        && g_touch_round_last_dir != 0;
    target_dir = active
        ? (g_touch_round_press.selected_dir
            ? g_touch_round_press.selected_dir
            : (center_repeat ? g_touch_round_last_dir : 0))
        : 0;

    /* A direct button press held past the long-press threshold previews the
     * Ctrl+direction action (released as Ctrl+dir).  Inner-disc drags choose
     * between step and run by distance, so they never preview Ctrl. */
    ctrl_preview = active
        && g_touch_round_press.button_press
        && target_dir != 0
        && press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL;
    run_preview = active
        && !g_touch_round_press.button_press
        && target_dir != 0
        && sdl_touch_round_drag_is_run(dist, inner_radius, radius);

    /* The wheel's pane-aware clip can be only the vertical lane containing
     * the control, while the selected map square follows the player anywhere
     * in the map.  Draw drag/repeat previews against the full map clip, then
     * restore the wheel clip for the control itself.  Direct outer-button
     * presses intentionally act immediately without a map-square preview. */
    if (target_dir && !g_touch_round_press.button_press
        && sdl_touch_round_compute_clip_rect(&map_clip))
    {
        SDL_SetRenderClipRect(g_state.renderer, &map_clip);
        sdl_touch_round_render_target_square(target_dir, ctrl_preview,
            run_preview);
    }

    SDL_SetRenderClipRect(g_state.renderer, &clip);

    sdl_touch_round_draw_circle(cx, cy, radius, frame);
    sdl_touch_round_draw_circle(cx, cy, radius - 2.0f, frame);
    sdl_touch_round_draw_circle(cx, cy, inner_radius, accent);
    sdl_touch_round_draw_sector_lines(cx, cy, inner_radius, radius, frame);
    sdl_touch_round_render_button_arrows(cx, cy, inner_radius, radius,
        target_dir, active, run_preview);

    if (active && !g_touch_round_press.button_press) {
        end_x = g_touch_round_press.current_x;
        end_y = g_touch_round_press.current_y;
        if (dist > inner_radius && dist > 0.0f) {
            end_x = cx + dx * inner_radius / dist;
            end_y = cy + dy * inner_radius / dist;
        }

        line_color = run_preview
            ? g_state.palette[TERM_L_GREEN]
            : accent;
        SDL_SetRenderDrawColor(g_state.renderer, line_color.r, line_color.g,
            line_color.b, line_color.a);
        SDL_RenderLine(g_state.renderer, cx, cy, end_x, end_y);
    }

    dir = target_dir;
    if (dir) {
        SDL_Color arrow_color = ctrl_preview
            ? g_state.palette[TERM_L_RED]
            : (run_preview ? g_state.palette[TERM_L_GREEN] : selected);

        center_arrow = (SDL_FRect){
            .x = cx - inner_radius,
            .y = cy - inner_radius,
            .w = inner_radius * 2.0f,
            .h = inner_radius * 2.0f,
        };
        sdl_touch_pane_draw_arrow(&center_arrow, '0' + dir, arrow_color);
    } else if (aim_targeting) {
        /* Name the otherwise ambiguous centre of the wheel so touch users can
         * distinguish this direct-direction action from exact map targeting. */
        center_arrow = (SDL_FRect){
            .x = cx - inner_radius * 0.92f,
            .y = cy - inner_radius * 0.92f,
            .w = inner_radius * 1.84f,
            .h = inner_radius * 1.84f,
        };
        sdl_touch_pane_draw_button_text_scaled(&center_arrow, NULL,
            "Fire direction", accent, 0.22f, 0.54f);
    }

    if (ctrl_preview)
        sdl_touch_round_render_ctrl_action_label(target_dir, radius, &clip);
    else if (run_preview)
        sdl_touch_round_render_run_action_label(target_dir, radius, &clip);

    SDL_SetRenderClipRect(g_state.renderer, NULL);
}

int sdl_touch_zone_overlay_mode_normalized(int mode)
{
    if (mode >= SDL_TOUCH_ZONE_OVERLAY_OFF
        && mode < SDL_TOUCH_ZONE_OVERLAY_COUNT)
    {
        return mode;
    }

    return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
}

bool sdl_touch_zone_overlay_visible(void)
{
    return sdl_touch_pane_uses_mobile_toggle()
        && sdl_touch_profile_normalized(config.touch_profile) == SDL_TOUCH_PROFILE_CORNERS
        && !sdl_touch_round_layer_config_enabled()
        && !sdl_touch_pane_mobile_layout_open()
        && sdl_mouse_gameplay_context_active();
}

bool sdl_touch_zone_layout_visible(void)
{
    return sdl_touch_zone_overlay_visible();
}

bool sdl_touch_zone_controls_active(void)
{
    return sdl_touch_zone_layout_visible()
        && sdl_main_screen_click_shortcuts_active();
}

bool sdl_touch_zone_compute_layout_for_screen(const SDL_Rect* screen,
    SDL_FRect* zone_rects)
{
    int size_px;
    int max_size_px;
    int start_y_px;
    float size;
    float left_x;
    float right_x;
    float start_y;

    if (!screen || !zone_rects)
        return false;
    if (!sdl_rect_has_area(screen))
        return false;

    size_px = screen->h / 3;
    max_size_px = screen->w / 4;
    if (size_px > max_size_px)
        size_px = max_size_px;
    if (size_px <= 0)
        return false;

    start_y_px = screen->y + (screen->h - size_px * 3) / 2;
    size = (float)size_px;
    left_x = (float)screen->x;
    right_x = (float)(screen->x + screen->w - size_px * 2);
    start_y = (float)start_y_px;

    zone_rects[TOUCH_ZONE_LEFT_NW] = (SDL_FRect){
        .x = left_x,
        .y = start_y,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_LEFT_N] = (SDL_FRect){
        .x = left_x + size,
        .y = start_y,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_LEFT_W] = (SDL_FRect){
        .x = left_x,
        .y = start_y + size,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_LEFT_Z] = (SDL_FRect){
        .x = left_x + size,
        .y = start_y + size,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_LEFT_SW] = (SDL_FRect){
        .x = left_x,
        .y = start_y + size * 2.0f,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_LEFT_S] = (SDL_FRect){
        .x = left_x + size,
        .y = start_y + size * 2.0f,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_RIGHT_N] = (SDL_FRect){
        .x = right_x,
        .y = start_y,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_RIGHT_NE] = (SDL_FRect){
        .x = right_x + size,
        .y = start_y,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_RIGHT_SPACE] = (SDL_FRect){
        .x = right_x,
        .y = start_y + size,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_RIGHT_E] = (SDL_FRect){
        .x = right_x + size,
        .y = start_y + size,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_RIGHT_S] = (SDL_FRect){
        .x = right_x,
        .y = start_y + size * 2.0f,
        .w = size,
        .h = size,
    };
    zone_rects[TOUCH_ZONE_RIGHT_SE] = (SDL_FRect){
        .x = right_x + size,
        .y = start_y + size * 2.0f,
        .w = size,
        .h = size,
    };

    return true;
}

bool sdl_touch_zone_compute_layout(SDL_FRect* zone_rects)
{
    SDL_Rect screen;

    if (!zone_rects)
        return false;
    if (!sdl_touch_zone_overlay_visible())
        return false;

    screen = sdl_get_layout_screen_rect();
    return sdl_touch_zone_compute_layout_for_screen(&screen, zone_rects);
}

bool sdl_touch_zone_point_to_zone(float x, float y, int* out_zone)
{
    SDL_FRect zone_rects[TOUCH_ZONE_COUNT];

    if (out_zone)
        *out_zone = -1;
    if (!sdl_touch_zone_compute_layout(zone_rects))
        return false;

    for (int i = 0; i < TOUCH_ZONE_COUNT; i++) {
        const SDL_FRect* rect = &zone_rects[i];

        if (x >= rect->x && x < rect->x + rect->w
            && y >= rect->y && y < rect->y + rect->h)
        {
            if (out_zone)
                *out_zone = i;
            return true;
        }
    }

    return false;
}

int sdl_touch_corner_up_down_side_normalized(int side)
{
    if (side == SDL_TOUCH_CORNER_UP_DOWN_LEFT
        || side == SDL_TOUCH_CORNER_UP_DOWN_RIGHT)
    {
        return side;
    }

    return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
}

bool sdl_touch_corner_up_down_on_left(void)
{
    return sdl_touch_corner_up_down_side_normalized(
        config.touch_corner_up_down_side) == SDL_TOUCH_CORNER_UP_DOWN_LEFT;
}

int sdl_touch_zone_center_binding_index(int zone, bool long_press)
{
    switch (zone) {
    case TOUCH_ZONE_LEFT_Z:
        return long_press ? SDL_TOUCH_ZONE_CENTER_LEFT_LONG_TAP
                          : SDL_TOUCH_ZONE_CENTER_LEFT_TAP;
    case TOUCH_ZONE_RIGHT_SPACE:
        return long_press ? SDL_TOUCH_ZONE_CENTER_RIGHT_LONG_TAP
                          : SDL_TOUCH_ZONE_CENTER_RIGHT_TAP;
    default:
        return -1;
    }
}

int sdl_touch_zone_binding_for_center(int zone, bool long_press)
{
    int index = sdl_touch_zone_center_binding_index(zone, long_press);

    if (index < 0 || index >= SDL_TOUCH_ZONE_CENTER_BINDING_COUNT)
        return GAMEPAD_BIND_NONE;

    return config.touch_zone_center_bindings[index];
}

int sdl_touch_zone_corner_action_binding_index(int zone,
    bool long_press)
{
    bool up_down_left = sdl_touch_corner_up_down_on_left();

    switch (zone) {
    case TOUCH_ZONE_LEFT_N:
        if (up_down_left)
            return -1;
        return long_press ? SDL_TOUCH_CORNER_ACTION_TOP_LONG_TAP
                          : SDL_TOUCH_CORNER_ACTION_TOP_TAP;
    case TOUCH_ZONE_RIGHT_N:
        if (!up_down_left)
            return -1;
        return long_press ? SDL_TOUCH_CORNER_ACTION_TOP_LONG_TAP
                          : SDL_TOUCH_CORNER_ACTION_TOP_TAP;
    case TOUCH_ZONE_LEFT_S:
        if (up_down_left)
            return -1;
        return long_press ? SDL_TOUCH_CORNER_ACTION_BOTTOM_LONG_TAP
                          : SDL_TOUCH_CORNER_ACTION_BOTTOM_TAP;
    case TOUCH_ZONE_RIGHT_S:
        if (!up_down_left)
            return -1;
        return long_press ? SDL_TOUCH_CORNER_ACTION_BOTTOM_LONG_TAP
                          : SDL_TOUCH_CORNER_ACTION_BOTTOM_TAP;
    default:
        return -1;
    }
}

int sdl_touch_zone_binding_for_corner_action(int zone,
    bool long_press)
{
    int index = sdl_touch_zone_corner_action_binding_index(zone, long_press);

    if (index < 0 || index >= SDL_TOUCH_CORNER_ACTION_BINDING_COUNT)
        return GAMEPAD_BIND_NONE;

    return config.touch_corner_action_bindings[index];
}

void sdl_touch_corner_action_binding_label(int binding, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    switch (binding) {
    case '+':
    case '=':
        SDL_strlcpy(buf, "Zoom +", buflen);
        return;
    case '-':
    case '_':
        SDL_strlcpy(buf, "Zoom -", buflen);
        return;
    case 'f':
        SDL_strlcpy(buf, "Shoot", buflen);
        return;
    case 'F':
        SDL_strlcpy(buf, "Shoot 2", buflen);
        return;
    default:
        binding_action_short(binding, buf, buflen);
        return;
    }
}

bool sdl_touch_corner_action_apply_zoom_binding(int binding)
{
    switch (binding) {
    case '+':
    case '=':
        return sdl_main_screen_adjust_main_view_scale(1);
    case '-':
    case '_':
        return sdl_main_screen_adjust_main_view_scale(-1);
    default:
        return false;
    }
}

bool sdl_touch_zone_corner_action_label(int zone, char* name,
    size_t name_len, char* symbol, size_t symbol_len)
{
    int tap_binding;
    int long_binding;

    if (sdl_touch_zone_corner_action_binding_index(zone, false) < 0)
        return false;

    tap_binding = sdl_touch_zone_binding_for_corner_action(zone, false);
    long_binding = sdl_touch_zone_binding_for_corner_action(zone, true);

    sdl_touch_corner_action_binding_label(tap_binding, name, name_len);
    if (symbol && symbol_len) {
        if (long_binding == GAMEPAD_BIND_NONE)
            symbol[0] = '\0';
        else
            sdl_touch_corner_action_binding_label(long_binding, symbol,
                symbol_len);
    }

    return true;
}

void sdl_touch_zone_button_label(int zone, char* name, size_t name_len,
    char* symbol, size_t symbol_len)
{
    if (name && name_len)
        name[0] = '\0';
    if (symbol && symbol_len)
        symbol[0] = '\0';

    if (!name || !name_len)
        return;

    switch (zone) {
    case TOUCH_ZONE_LEFT_NW:
        SDL_strlcpy(name, "NW", name_len);
        return;
    case TOUCH_ZONE_LEFT_N:
        if (sdl_touch_corner_up_down_on_left()) {
            SDL_strlcpy(name, "N", name_len);
            return;
        }
        (void)sdl_touch_zone_corner_action_label(zone, name, name_len,
            symbol, symbol_len);
        return;
    case TOUCH_ZONE_RIGHT_N:
        if (!sdl_touch_corner_up_down_on_left()) {
            SDL_strlcpy(name, "N", name_len);
            return;
        }
        (void)sdl_touch_zone_corner_action_label(zone, name, name_len,
            symbol, symbol_len);
        return;
    case TOUCH_ZONE_RIGHT_NE:
        SDL_strlcpy(name, "NE", name_len);
        return;
    case TOUCH_ZONE_LEFT_W:
        SDL_strlcpy(name, "W", name_len);
        return;
    case TOUCH_ZONE_RIGHT_E:
        SDL_strlcpy(name, "E", name_len);
        return;
    case TOUCH_ZONE_LEFT_SW:
        SDL_strlcpy(name, "SW", name_len);
        return;
    case TOUCH_ZONE_LEFT_S:
        if (sdl_touch_corner_up_down_on_left()) {
            SDL_strlcpy(name, "S", name_len);
            return;
        }
        (void)sdl_touch_zone_corner_action_label(zone, name, name_len,
            symbol, symbol_len);
        return;
    case TOUCH_ZONE_RIGHT_S:
        if (!sdl_touch_corner_up_down_on_left()) {
            SDL_strlcpy(name, "S", name_len);
            return;
        }
        (void)sdl_touch_zone_corner_action_label(zone, name, name_len,
            symbol, symbol_len);
        return;
    case TOUCH_ZONE_RIGHT_SE:
        SDL_strlcpy(name, "SE", name_len);
        return;
    case TOUCH_ZONE_LEFT_Z:
    case TOUCH_ZONE_RIGHT_SPACE:
        binding_action_short(sdl_touch_zone_binding_for_center(zone, false),
            name, name_len);
        if (symbol && symbol_len)
            binding_action_short(sdl_touch_zone_binding_for_center(zone, true),
                symbol, symbol_len);
        return;
    default:
        return;
    }
}

void sdl_touch_zone_render_markers(void)
{
    SDL_FRect zone_rects[TOUCH_ZONE_COUNT];
    SDL_Rect screen;
    SDL_Color marker_color = g_state.palette[TERM_L_BLUE];
    int overlay_mode =
        sdl_touch_zone_overlay_mode_normalized(config.touch_zone_overlay_mode);
    bool draw_markers = (overlay_mode == SDL_TOUCH_ZONE_OVERLAY_MARKERS);
    bool draw_borders = (overlay_mode == SDL_TOUCH_ZONE_OVERLAY_BORDERS
        || overlay_mode == SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS);
    bool draw_labels = (overlay_mode == SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS);
    float size;
    float start_y;
    float bottom_y;
    float left_split_x;
    float left_inner_x;
    float right_inner_x;
    float right_split_x;
    float marker_len;
    float marker_thickness;
    float top_bottom_markers[4];

    if (overlay_mode == SDL_TOUCH_ZONE_OVERLAY_OFF)
        return;
    if (!sdl_touch_zone_compute_layout(zone_rects))
        return;

    if (draw_borders) {
        SDL_SetRenderDrawColor(g_state.renderer, marker_color.r,
            marker_color.g, marker_color.b, 36);
        for (int i = 0; i < TOUCH_ZONE_COUNT; i++)
            SDL_RenderRect(g_state.renderer, &zone_rects[i]);
    }

    if (draw_labels) {
        SDL_Color label_color = marker_color;

        label_color.a = 92;
        for (int i = 0; i < TOUCH_ZONE_COUNT; i++) {
            char name[32];
            char symbol[32];

            sdl_touch_zone_button_label(i, name, sizeof(name), symbol,
                sizeof(symbol));
            sdl_touch_pane_draw_button_text_scaled(&zone_rects[i], name,
                symbol, label_color, 0.24f, 0.30f);
        }
    }

    if (!draw_markers)
        return;

    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return;

    size = zone_rects[TOUCH_ZONE_LEFT_NW].h;
    start_y = zone_rects[TOUCH_ZONE_LEFT_NW].y;
    bottom_y = start_y + size * 3.0f;
    left_split_x = zone_rects[TOUCH_ZONE_LEFT_N].x;
    left_inner_x = zone_rects[TOUCH_ZONE_LEFT_N].x + size;
    right_inner_x = zone_rects[TOUCH_ZONE_RIGHT_N].x;
    right_split_x = zone_rects[TOUCH_ZONE_RIGHT_NE].x;
    marker_len = sdl_touch_pane_clampf(size * 0.20f, 18.0f, 44.0f);
    marker_thickness = sdl_touch_pane_clampf(size * 0.018f, 2.0f, 4.0f);
    top_bottom_markers[0] = left_split_x;
    top_bottom_markers[1] = left_inner_x;
    top_bottom_markers[2] = right_inner_x;
    top_bottom_markers[3] = right_split_x;

    SDL_SetRenderDrawColor(g_state.renderer, marker_color.r, marker_color.g,
        marker_color.b, 150);

    for (int i = 1; i <= 2; i++) {
        float y = start_y + size * (float)i - marker_thickness * 0.5f;

        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = (float)screen.x,
            .y = y,
            .w = marker_len,
            .h = marker_thickness,
        });
        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = (float)(screen.x + screen.w) - marker_len,
            .y = y,
            .w = marker_len,
            .h = marker_thickness,
        });
    }

    for (int i = 0; i < (int)N_ELEMENTS(top_bottom_markers); i++) {
        float x = top_bottom_markers[i] - marker_thickness * 0.5f;

        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = x,
            .y = start_y,
            .w = marker_thickness,
            .h = marker_len,
        });
        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = x,
            .y = bottom_y - marker_len,
            .w = marker_thickness,
            .h = marker_len,
        });
    }
}

bool sdl_touch_zone_is_arrow(int zone)
{
    bool up_down_left = sdl_touch_corner_up_down_on_left();

    return zone == TOUCH_ZONE_LEFT_NW
        || zone == TOUCH_ZONE_LEFT_W
        || zone == TOUCH_ZONE_LEFT_SW
        || zone == TOUCH_ZONE_RIGHT_NE
        || zone == TOUCH_ZONE_RIGHT_E
        || zone == TOUCH_ZONE_RIGHT_SE
        || (up_down_left
            && (zone == TOUCH_ZONE_LEFT_N || zone == TOUCH_ZONE_LEFT_S))
        || (!up_down_left
            && (zone == TOUCH_ZONE_RIGHT_N || zone == TOUCH_ZONE_RIGHT_S));
}

int sdl_touch_zone_arrow_dir(int zone)
{
    switch (zone) {
    case TOUCH_ZONE_LEFT_NW:
        return 7;
    case TOUCH_ZONE_LEFT_N:
    case TOUCH_ZONE_RIGHT_N:
        return 8;
    case TOUCH_ZONE_RIGHT_NE:
        return 9;
    case TOUCH_ZONE_LEFT_W:
        return 4;
    case TOUCH_ZONE_RIGHT_E:
        return 6;
    case TOUCH_ZONE_LEFT_SW:
        return 1;
    case TOUCH_ZONE_LEFT_S:
    case TOUCH_ZONE_RIGHT_S:
        return 2;
    case TOUCH_ZONE_RIGHT_SE:
        return 3;
    default:
        return 0;
    }
}

SDL_FColor sdl_touch_hidden_indicator_fcolor(SDL_Color color)
{
    return (SDL_FColor){
        (float)color.r / 255.0f,
        (float)color.g / 255.0f,
        (float)color.b / 255.0f,
        (float)color.a / 255.0f,
    };
}

float sdl_touch_hidden_indicator_size_for_screen(const SDL_Rect* screen)
{
    float size;

    if (!screen)
        return 0.0f;

    size = (float)screen->h * 0.035f;
    return sdl_touch_pane_clampf(size, 28.0f, 52.0f);
}

void sdl_touch_hidden_indicator_draw_triangle(
    const SDL_FPoint points[3], SDL_Color fill, SDL_Color outline)
{
    SDL_FColor fcolor = sdl_touch_hidden_indicator_fcolor(fill);
    SDL_Vertex vertices[3];
    int indices[3] = { 0, 1, 2 };

    if (!points)
        return;

    vertices[0] = (SDL_Vertex){ points[0], fcolor, { 0.0f, 0.0f } };
    vertices[1] = (SDL_Vertex){ points[1], fcolor, { 0.0f, 0.0f } };
    vertices[2] = (SDL_Vertex){ points[2], fcolor, { 0.0f, 0.0f } };
    SDL_RenderGeometry(g_state.renderer, NULL, vertices, 3, indices, 3);

    SDL_SetRenderDrawColor(g_state.renderer, outline.r, outline.g,
        outline.b, outline.a);
    SDL_RenderLine(g_state.renderer, points[0].x, points[0].y,
        points[1].x, points[1].y);
    SDL_RenderLine(g_state.renderer, points[1].x, points[1].y,
        points[2].x, points[2].y);
    SDL_RenderLine(g_state.renderer, points[2].x, points[2].y,
        points[0].x, points[0].y);
}

void sdl_touch_hidden_indicator_render_pane(const SDL_Rect* screen,
    bool right_side, float size, SDL_Color fill, SDL_Color outline)
{
    float edge_x;
    float center_y;
    float half = size * 0.5f;
    float depth = size * 0.72f;
    SDL_FPoint points[3];

    if (!screen)
        return;

    edge_x = right_side ? (float)(screen->x + screen->w - 1)
                        : (float)screen->x;
    center_y = (float)screen->y + (float)screen->h * 0.5f;

    if (right_side) {
        points[0] = (SDL_FPoint){ edge_x, center_y - half };
        points[1] = (SDL_FPoint){ edge_x, center_y + half };
        points[2] = (SDL_FPoint){ edge_x - depth, center_y };
    } else {
        points[0] = (SDL_FPoint){ edge_x, center_y - half };
        points[1] = (SDL_FPoint){ edge_x + depth, center_y };
        points[2] = (SDL_FPoint){ edge_x, center_y + half };
    }

    sdl_touch_hidden_indicator_draw_triangle(points, fill, outline);
}

void sdl_touch_hidden_indicator_render_bottom(const SDL_Rect* screen,
    float size, SDL_Color fill, SDL_Color outline)
{
    float center_x;
    float edge_y;
    float half = size * 0.5f;
    float depth = size * 0.72f;
    SDL_FPoint points[3];

    if (!screen)
        return;

    center_x = (float)screen->x + (float)screen->w * 0.5f;
    edge_y = (float)(screen->y + screen->h - 1);
    points[0] = (SDL_FPoint){ center_x - half, edge_y };
    points[1] = (SDL_FPoint){ center_x + half, edge_y };
    points[2] = (SDL_FPoint){ center_x, edge_y - depth };

    sdl_touch_hidden_indicator_draw_triangle(points, fill, outline);
}

typedef enum touch_top_panel_indicator_edge {
    TOUCH_TOP_PANEL_INDICATOR_TOP = 0,
    TOUCH_TOP_PANEL_INDICATOR_RIGHT,
    TOUCH_TOP_PANEL_INDICATOR_BOTTOM,
    TOUCH_TOP_PANEL_INDICATOR_LEFT
} touch_top_panel_indicator_edge;

static float sdl_touch_top_panel_indicator_size_for_screen(
    const SDL_Rect* screen)
{
    float size;

    if (!screen)
        return 0.0f;

    size = (float)screen->h * 0.026f;
    return sdl_touch_pane_clampf(size, 22.0f, 40.0f);
}

static touch_top_panel_indicator_edge
sdl_touch_top_panel_indicator_edge_for_placement(enum pane_placement where)
{
    switch (where) {
    case PLACE_TOP_LEFT:
    case PLACE_TOP_CENTER:
    case PLACE_TOP_RIGHT:
        return TOUCH_TOP_PANEL_INDICATOR_TOP;
    case PLACE_RIGHT_CENTER:
        return TOUCH_TOP_PANEL_INDICATOR_RIGHT;
    case PLACE_LEFT_CENTER:
        return TOUCH_TOP_PANEL_INDICATOR_LEFT;
    case PLACE_BOTTOM_LEFT:
    case PLACE_BOTTOM_CENTER:
    case PLACE_BOTTOM_RIGHT:
    default:
        return TOUCH_TOP_PANEL_INDICATOR_BOTTOM;
    }
}

static void sdl_touch_top_panel_indicator_hit_rect(float cx, float cy,
    float hit, SDL_FRect* out_hit)
{
    if (!out_hit)
        return;

    *out_hit = (SDL_FRect){
        .x = cx - hit * 0.5f,
        .y = cy - hit * 0.5f,
        .w = hit,
        .h = hit,
    };
}

static bool sdl_touch_top_panel_indicator_geometry(bool open,
    SDL_FRect* out_hit, SDL_FPoint points[3])
{
    SDL_Rect screen;
    SDL_Rect anchor;
    SDL_FRect panel = { 0 };
    enum pane_placement where;
    touch_top_panel_indicator_edge edge;
    float size;
    float half;
    float depth;
    float hit;
    float cx;
    float cy;
    float edge_pos;
    bool panel_ready = false;

    if (out_hit)
        *out_hit = (SDL_FRect){ 0 };
    if (points) {
        points[0] = (SDL_FPoint){ 0.0f, 0.0f };
        points[1] = (SDL_FPoint){ 0.0f, 0.0f };
        points[2] = (SDL_FPoint){ 0.0f, 0.0f };
    }
    if (!config.touch_top_panel_arrows_visible)
        return false;

    if (!sdl_touch_top_panel_current_anchor(&screen, &anchor, &where))
        return false;
    if (open)
        panel_ready = sdl_touch_top_panel_compute_layout_for_anchor(&screen,
            &anchor, where, NULL, &panel);

    size = sdl_touch_top_panel_indicator_size_for_screen(&screen);
    if (size <= 0.0f)
        return false;
    half = size * 0.5f;
    depth = size * 0.72f;
    hit = size * 1.7f;
    if (hit < 38.0f)
        hit = 38.0f;

    edge = sdl_touch_top_panel_indicator_edge_for_placement(where);
    switch (edge) {
    case TOUCH_TOP_PANEL_INDICATOR_TOP:
        cx = panel_ready ? panel.x + panel.w * 0.5f
                         : (float)anchor.x + (float)anchor.w * 0.5f;
        edge_pos = panel_ready ? panel.y : (float)screen.y;
        cy = panel_ready ? edge_pos - depth * 0.5f : edge_pos + depth * 0.5f;
        if (points) {
            points[0] = (SDL_FPoint){ cx - half,
                panel_ready ? edge_pos : edge_pos + depth };
            points[1] = (SDL_FPoint){ cx + half,
                panel_ready ? edge_pos : edge_pos + depth };
            points[2] = (SDL_FPoint){ cx,
                panel_ready ? edge_pos - depth : edge_pos };
        }
        sdl_touch_top_panel_indicator_hit_rect(cx, cy, hit, out_hit);
        break;

    case TOUCH_TOP_PANEL_INDICATOR_RIGHT:
        cy = panel_ready ? panel.y + panel.h * 0.5f
                         : (float)anchor.y + (float)anchor.h * 0.5f;
        edge_pos = panel_ready ? panel.x + panel.w
                               : (float)(screen.x + screen.w - 1);
        cx = panel_ready ? edge_pos + depth * 0.5f
                         : edge_pos - depth * 0.5f;
        if (points) {
            points[0] = (SDL_FPoint){
                panel_ready ? edge_pos : edge_pos - depth, cy - half };
            points[1] = (SDL_FPoint){
                panel_ready ? edge_pos : edge_pos - depth, cy + half };
            points[2] = (SDL_FPoint){
                panel_ready ? edge_pos + depth : edge_pos, cy };
        }
        sdl_touch_top_panel_indicator_hit_rect(cx, cy, hit, out_hit);
        break;

    case TOUCH_TOP_PANEL_INDICATOR_LEFT:
        cy = panel_ready ? panel.y + panel.h * 0.5f
                         : (float)anchor.y + (float)anchor.h * 0.5f;
        edge_pos = panel_ready ? panel.x : (float)screen.x;
        cx = panel_ready ? edge_pos - depth * 0.5f
                         : edge_pos + depth * 0.5f;
        if (points) {
            points[0] = (SDL_FPoint){
                panel_ready ? edge_pos : edge_pos + depth, cy - half };
            points[1] = (SDL_FPoint){
                panel_ready ? edge_pos : edge_pos + depth, cy + half };
            points[2] = (SDL_FPoint){
                panel_ready ? edge_pos - depth : edge_pos, cy };
        }
        sdl_touch_top_panel_indicator_hit_rect(cx, cy, hit, out_hit);
        break;

    case TOUCH_TOP_PANEL_INDICATOR_BOTTOM:
    default:
        cx = panel_ready ? panel.x + panel.w * 0.5f
                         : (float)anchor.x + (float)anchor.w * 0.5f;
        edge_pos = panel_ready ? panel.y + panel.h
                               : (float)(screen.y + screen.h - 1);
        cy = panel_ready ? edge_pos + depth * 0.5f
                         : edge_pos - depth * 0.5f;
        if (points) {
            points[0] = (SDL_FPoint){ cx - half,
                panel_ready ? edge_pos : edge_pos - depth };
            points[1] = (SDL_FPoint){ cx + half,
                panel_ready ? edge_pos : edge_pos - depth };
            points[2] = (SDL_FPoint){ cx,
                panel_ready ? edge_pos + depth : edge_pos };
        }
        sdl_touch_top_panel_indicator_hit_rect(cx, cy, hit, out_hit);
        break;
    }

    return true;
}

static void sdl_touch_top_panel_indicator_render(bool open, SDL_Color fill,
    SDL_Color outline)
{
    SDL_FPoint points[3];

    if (!sdl_touch_top_panel_indicator_geometry(open, NULL, points))
        return;

    sdl_touch_hidden_indicator_draw_triangle(points, fill, outline);
}

void sdl_touch_hidden_indicator_render(void)
{
    SDL_Rect screen;
    SDL_Color fill = g_state.palette[TERM_YELLOW];
    SDL_Color outline = g_state.palette[TERM_WHITE];
    bool top_panel_indicator;
    bool proto_touch = sdl_touch_pane_proto_mode_active();

    if (!proto_touch && !sdl_touch_pane_is_config_enabled()
        && !sdl_touch_top_panel_layout_visible())
        return;

    top_panel_indicator = sdl_touch_top_panel_layout_visible();
    if (!top_panel_indicator)
        return;

    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return;

    fill.a = 224;
    outline.a = 255;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    if (top_panel_indicator)
        sdl_touch_top_panel_indicator_render(g_touch_top_panel_open, fill,
            outline);
}

bool sdl_touch_hidden_indicator_handle_pointer_down(float x, float y,
    bool touch)
{
    SDL_FRect top_hit;

    (void)touch;

    if (!sdl_touch_top_panel_layout_visible())
        return false;
    if (!sdl_main_screen_click_shortcuts_active())
        return false;

    if (sdl_touch_top_panel_indicator_geometry(
            g_touch_top_panel_open, &top_hit, NULL)
        && sdl_point_in_frect(&top_hit, x, y))
    {
        sdl_touch_top_panel_set_open(!g_touch_top_panel_open);
        return true;
    }

    return false;
}

bool sdl_touch_top_panel_pointer_claims_point(float x, float y)
{
    SDL_FRect top_hit;

    if (!sdl_touch_top_panel_layout_visible())
        return false;
    if (sdl_touch_top_panel_point_to_slot(x, y, NULL))
        return true;
    if (sdl_touch_top_panel_indicator_geometry(
            g_touch_top_panel_open, &top_hit, NULL)
        && sdl_point_in_frect(&top_hit, x, y))
    {
        return true;
    }

    return false;
}

bool sdl_touch_top_panel_handle_secondary_pointer(float x, float y)
{
    int slot = -1;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_touch_top_panel_point_to_slot(x, y, &slot))
        return false;
    if (slot < 0)
        return false;

    sdl_touch_top_panel_cancel_press();
    sdl_touch_top_panel_set_hover_slot(-1);
    sdl_touch_run_quick_access_picker(slot);
    return true;
}

void sdl_touch_zone_send(int zone, bool long_press)
{
    if (!sdl_touch_zone_controls_active())
        return;

    if (sdl_touch_zone_is_arrow(zone)) {
        int dir = sdl_touch_zone_arrow_dir(zone);

        sdl_gamepad_send_direction_mods(dir, false, long_press, false);
        return;
    }

    if (zone == TOUCH_ZONE_LEFT_Z) {
        sdl_touch_pane_send_binding(
            sdl_touch_zone_binding_for_center(zone, long_press),
            false, false);
        return;
    }

    if (zone == TOUCH_ZONE_RIGHT_SPACE) {
        sdl_touch_pane_send_binding(
            sdl_touch_zone_binding_for_center(zone, long_press),
            false, false);
        return;
    }

    if (sdl_touch_zone_corner_action_binding_index(zone, long_press) >= 0) {
        int binding = sdl_touch_zone_binding_for_corner_action(zone,
            long_press);

        if (sdl_touch_corner_action_apply_zoom_binding(binding))
            return;

        sdl_touch_pane_send_binding(binding, false, false);
    }
}

void sdl_touch_zone_cancel_press(void)
{
    g_touch_zone_press.active = false;
    g_touch_zone_press.finger_id = 0;
    g_touch_zone_press.zone = -1;
    g_touch_zone_press.start_x = 0.0f;
    g_touch_zone_press.start_y = 0.0f;
    g_touch_zone_press.start_time = 0;
}

bool sdl_touch_zone_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id)
{
    int zone = -1;

    if (!sdl_touch_zone_controls_active())
        return false;
    if (sdl_touch_movement_point_blocked_by_overlay(x, y))
        return false;
    if (sdl_main_view_point_is_player_grid(x, y))
        return false;
    if (!sdl_touch_zone_point_to_zone(x, y, &zone))
        return false;
    if (zone < 0)
        return false;

    sdl_menu_touch_cancel();
    sdl_menu_scroll_cancel();
    sdl_map_touch_cancel_press();
    sdl_pointer_attack_cancel_touch_press();
    sdl_touch_zone_cancel_press();
    g_touch_zone_press.active = true;
    g_touch_zone_press.finger_id = finger_id;
    g_touch_zone_press.zone = zone;
    g_touch_zone_press.start_x = x;
    g_touch_zone_press.start_y = y;
    g_touch_zone_press.start_time = SDL_GetTicksNS();
    return true;
}

bool sdl_touch_zone_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float threshold;
    float start_x;
    float start_y;

    if (!g_touch_zone_press.active
        || g_touch_zone_press.finger_id != finger_id)
    {
        return false;
    }

    dx = x - g_touch_zone_press.start_x;
    dy = y - g_touch_zone_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_touch_swipe_threshold_px();
    if (dx > threshold || dy > threshold) {
        start_x = g_touch_zone_press.start_x;
        start_y = g_touch_zone_press.start_y;
        sdl_touch_zone_cancel_press();
        if (sdl_touch_swipe_handle_pointer_down(start_x, start_y, finger_id))
            (void)sdl_touch_swipe_handle_pointer_motion(x, y, finger_id);
        return true;
    }

    return true;
}

bool sdl_touch_zone_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id)
{
    Uint64 press_time;
    bool long_press;
    int zone;
    int release_zone = -1;

    if (!g_touch_zone_press.active
        || g_touch_zone_press.finger_id != finger_id)
    {
        return false;
    }

    zone = g_touch_zone_press.zone;
    press_time = SDL_GetTicksNS() - g_touch_zone_press.start_time;
    long_press = (press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL);

    (void)sdl_touch_zone_point_to_zone(x, y, &release_zone);
    sdl_touch_zone_cancel_press();
    if (release_zone != zone)
        return true;

    sdl_touch_zone_send(zone, long_press);
    return true;
}

int sdl_touch_zone_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_touch_zone_press.active)
        return -1;

    elapsed = now_ns - g_touch_zone_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_touch_zone_flush_pending_press(Uint64 now_ns)
{
    int zone;

    if (!g_touch_zone_press.active)
        return false;
    if (!sdl_touch_zone_controls_active()) {
        sdl_touch_zone_cancel_press();
        return false;
    }
    if (now_ns - g_touch_zone_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    zone = g_touch_zone_press.zone;
    sdl_touch_zone_cancel_press();
    sdl_touch_zone_send(zone, true);
    return true;
}

static const struct pane_config* sdl_touch_top_panel_pane_config(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_OVERLAY_MENU)
            return &pane_config[i];
    }

    return NULL;
}

static bool sdl_touch_top_panel_pane_enabled(void)
{
    const struct pane_config* pc = sdl_touch_top_panel_pane_config();

    return pc ? pc->enabled : true;
}

static enum pane_placement sdl_touch_top_panel_pane_placement(void)
{
    const struct pane_config* pc = sdl_touch_top_panel_pane_config();

    if (pc && pane_type_allows_placement(PANE_OVERLAY_MENU, pc->where))
        return pc->where;
    return PLACE_BOTTOM_CENTER;
}

bool sdl_touch_top_panel_layout_visible(void)
{
    return sdl_touch_top_panel_pane_enabled()
        && (g_direct_touch_present || config.mouse_enabled)
        && sdl_mouse_gameplay_context_active()
        && !g_main_menu_overlay_active;
}

static void sdl_touch_top_panel_set_hover_slot(int slot)
{
    if (slot < 0 || slot >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        slot = -1;
    if (g_touch_top_panel_hover_slot == slot)
        return;

    g_touch_top_panel_hover_slot = slot;
    g_state.need_present = true;
}

void sdl_touch_top_panel_set_open(bool open)
{
    if (g_touch_top_panel_open == open)
        return;

    g_touch_top_panel_open = open;
    sdl_touch_top_panel_cancel_press();
    if (!open)
        sdl_touch_top_panel_set_hover_slot(-1);
    g_state.need_present = true;
}

float sdl_touch_top_panel_size_normalized(float size)
{
    int steps;

    if (size <= SDL_TOUCH_TOP_PANEL_SIZE_STRETCH)
        return SDL_TOUCH_TOP_PANEL_SIZE_STRETCH;
    if (size < SDL_TOUCH_TOP_PANEL_SIZE_MIN)
        size = SDL_TOUCH_TOP_PANEL_SIZE_MIN;
    if (size > SDL_TOUCH_TOP_PANEL_SIZE_MAX)
        size = SDL_TOUCH_TOP_PANEL_SIZE_MAX;

    steps = (int)(size / SDL_TOUCH_TOP_PANEL_SIZE_STEP + 0.5f);
    return (float)steps * SDL_TOUCH_TOP_PANEL_SIZE_STEP;
}

int sdl_touch_top_panel_columns_normalized(int columns)
{
    if (columns < SDL_TOUCH_TOP_PANEL_COLUMNS_MIN)
        return SDL_TOUCH_TOP_PANEL_COLUMNS_MIN;
    if (columns > SDL_TOUCH_TOP_PANEL_COLUMNS_MAX)
        return SDL_TOUCH_TOP_PANEL_COLUMNS_MAX;
    return columns;
}

int sdl_touch_top_panel_cell_count_normalized(int count)
{
    if (count < SDL_TOUCH_TOP_PANEL_CELL_COUNT_MIN)
        return SDL_TOUCH_TOP_PANEL_CELL_COUNT_MIN;
    if (count > SDL_TOUCH_TOP_PANEL_CELL_COUNT_MAX)
        return SDL_TOUCH_TOP_PANEL_CELL_COUNT_MAX;
    return count;
}

int sdl_touch_top_panel_rows_normalized(int rows)
{
    if (rows < SDL_TOUCH_TOP_PANEL_ROWS_MIN)
        return SDL_TOUCH_TOP_PANEL_ROWS_MIN;
    if (rows > SDL_TOUCH_TOP_PANEL_ROWS_MAX)
        return SDL_TOUCH_TOP_PANEL_ROWS_MAX;
    return rows;
}

static int sdl_touch_top_panel_configured_button_count(void)
{
    return sdl_touch_top_panel_cell_count_normalized(
        config.touch_top_panel_cell_count);
}

static int sdl_touch_top_panel_slot_for_display_index(int index,
    int configured_count)
{
    if (index < 0 || index >= configured_count)
        return -1;
    return index;
}

static int sdl_touch_top_panel_description_action_for_binding(int binding)
{
    if (!sdl_touch_thumb_description_open())
        return GAMEPAD_BIND_NONE;

    if (binding == INPUT_BIND_CONFIRM)
        binding = ' ';

    if (binding == 'x' && sdl_description_overlay_has_footer_action('x'))
        return 'x';
    if ((binding == ' ' || binding == 'g')
        && sdl_description_overlay_has_footer_action(' '))
    {
        return ' ';
    }

    return GAMEPAD_BIND_NONE;
}

static int sdl_touch_top_panel_description_action_for_slot(int slot)
{
    if (slot < 0 || slot >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;

    return sdl_touch_top_panel_description_action_for_binding(
        config.touch_top_panel_bindings[slot]);
}

static int sdl_touch_top_panel_visible_slots(int* slots, int max_slots)
{
    int configured_count = sdl_touch_top_panel_configured_button_count();
    int visible_count = 0;

    if (sdl_touch_thumb_description_open())
    {
        int description_slot = -1;
        int pickup_slot = -1;

        for (int i = 0; i < configured_count; i++) {
            int slot = sdl_touch_top_panel_slot_for_display_index(i,
                configured_count);
            int action = sdl_touch_top_panel_description_action_for_slot(slot);

            if (action == 'x' && description_slot < 0)
                description_slot = slot;
            else if (action == ' ' && pickup_slot < 0)
                pickup_slot = slot;
        }

        if (description_slot >= 0) {
            if (slots && visible_count < max_slots)
                slots[visible_count] = description_slot;
            visible_count++;
        }
        if (pickup_slot >= 0 && pickup_slot != description_slot) {
            if (slots && visible_count < max_slots)
                slots[visible_count] = pickup_slot;
            visible_count++;
        }

        return visible_count;
    }

    for (int i = 0; i < configured_count; i++) {
        int slot = sdl_touch_top_panel_slot_for_display_index(i,
            configured_count);

        if (slots && visible_count < max_slots)
            slots[visible_count] = slot;
        visible_count++;
    }

    return visible_count;
}

int sdl_touch_top_panel_visible_button_count(void)
{
    return sdl_touch_top_panel_visible_slots(NULL, 0);
}

bool sdl_touch_top_panel_current_anchor(SDL_Rect* out_screen,
    SDL_Rect* out_anchor, enum pane_placement* out_where)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    SDL_Rect anchor = { 0 };
    enum pane_placement where = sdl_touch_top_panel_pane_placement();

    if (!sdl_touch_top_panel_pane_enabled())
        return false;

    if (sdl_layout_matches_supporting_pane_visibility()
        && sdl_rect_has_area(&g_pane_rects[PANE_OVERLAY_MENU]))
    {
        anchor = g_pane_rects[PANE_OVERLAY_MENU];
    } else {
        SDL_Rect panes[PANE_MAX] = { 0 };

        sdl_compute_display_panes(panes);
        if (sdl_rect_has_area(&panes[PANE_OVERLAY_MENU]))
            anchor = panes[PANE_OVERLAY_MENU];
    }

    if (!sdl_rect_has_area(&screen))
        screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return false;
    if (!sdl_rect_has_area(&anchor))
        anchor = screen;

    if (out_screen)
        *out_screen = screen;
    if (out_anchor)
        *out_anchor = anchor;
    if (out_where)
        *out_where = where;
    return true;
}

static bool sdl_touch_top_panel_pane_shares_stack(enum pane_type pane,
    enum pane_placement where)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == pane && pane_config[i].enabled
            && pane_config[i].where == where)
        {
            return true;
        }
    }

    return false;
}

/* Return the full horizontal span which does not cross side overlay panes in
 * the Quick Access panel's vertical band.  Stretch uses the whole span. */
static float sdl_touch_top_panel_available_width(const SDL_Rect* screen,
    const SDL_Rect* anchor, enum pane_placement where, float panel_h,
    float gap, float* out_left, float* out_right)
{
    status_pane_layout status_layout;
    float band_top;
    float band_bottom;
    float left;
    float right;
    float center;
    float screen_w;
    float side_margin;
    SDL_Rect panes[4];
    int pane_count = 0;
    SDL_Rect r;
    SDL_FRect fr;

    if (!screen || !anchor || panel_h <= 0.0f)
        return 0.0f;

    screen_w = (float)screen->w;
    side_margin = sdl_touch_pane_clampf(screen_w * 0.02f, 6.0f, 18.0f);
    left = (float)screen->x + side_margin;
    right = (float)(screen->x + screen->w) - side_margin;

    band_top = sdl_overlay_panel_y(anchor, where, (int)(panel_h + 0.5f));
    band_bottom = band_top + panel_h;
    /* Small vertical slop so a pane that just abuts the band still counts. */
    band_top -= gap;
    band_bottom += gap;

    if (!sdl_touch_top_panel_pane_shares_stack(PANE_STATUS, where)
        && sdl_status_pane_layout(&status_layout))
    {
        r = (SDL_Rect){
            (int)status_layout.panel.x, (int)status_layout.panel.y,
            (int)status_layout.panel.w, (int)status_layout.panel.h
        };
        if (r.w > 0 && r.h > 0)
            panes[pane_count++] = r;
    }
    if (!sdl_touch_top_panel_pane_shares_stack(PANE_COMBAT, where)
        && sdl_combat_overlay_pane_current_rect(&r) && r.w > 0 && r.h > 0)
    {
        panes[pane_count++] = r;
    }
    if (!sdl_touch_top_panel_pane_shares_stack(PANE_ROLLS, where)
        && sdl_overlay_log_pane_current_rect(&r) && r.w > 0 && r.h > 0)
    {
        panes[pane_count++] = r;
    }
    if (!sdl_touch_top_panel_pane_shares_stack(PANE_DEPTH, where)
        && sdl_depth_menu_pane_current_rect(&fr)
        && fr.w > 0.0f && fr.h > 0.0f)
    {
        panes[pane_count++] = (SDL_Rect){ (int)fr.x, (int)fr.y,
            (int)fr.w, (int)fr.h };
    }
    /* Status & Depth adapts its own width around Quick Access.  Feeding it
     * back as a horizontal obstacle here would make the controls shrink or
     * slide sideways and introduces a circular layout dependency. */

    /* Classify obstructions by screen side.  The anchor is often the narrow
     * left overlay menu, so its centre cannot separate left-side status panes
     * from right-side log/depth panes when Stretch uses the whole screen. */
    center = (float)screen->x + (float)screen->w * 0.5f;

    for (int i = 0; i < pane_count; i++) {
        float p_left = (float)panes[i].x;
        float p_right = (float)(panes[i].x + panes[i].w);
        float p_top = (float)panes[i].y;
        float p_bottom = (float)(panes[i].y + panes[i].h);
        float p_mid = (p_left + p_right) * 0.5f;

        if (p_bottom <= band_top || p_top >= band_bottom)
            continue;
        if (p_mid <= center) {
            if (p_right + gap > left)
                left = p_right + gap;
        } else if (p_left - gap < right) {
            right = p_left - gap;
        }
    }

    if (out_left)
        *out_left = left;
    if (out_right)
        *out_right = right;
    return MAX(1.0f, right - left);
}

static void sdl_touch_top_panel_button_metrics_for_size(float size,
    float* out_button_size, float* out_gap)
{
    float icon_size;
    float button_size;
    float gap;

    if (size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH)
        size = SDL_TOUCH_TOP_PANEL_SIZE_DEFAULT;
    size = sdl_touch_top_panel_size_normalized(size);
    icon_size = (float)TILE_SIZE * size;

#if SIL_SDL_MOBILE_BUILD
    button_size = sdl_touch_pane_clampf(icon_size * 2.75f, 76.0f, 112.0f);
    if (size > SDL_TOUCH_TOP_PANEL_SIZE_DEFAULT) {
        button_size += (size - SDL_TOUCH_TOP_PANEL_SIZE_DEFAULT) * 12.0f;
    }
    gap = sdl_touch_pane_clampf(button_size * 0.12f, 8.0f, 18.0f);
#else
    float pad = sdl_touch_pane_clampf(icon_size * 0.18f, 6.0f, 14.0f);

    gap = sdl_touch_pane_clampf(icon_size * 0.18f, 6.0f, 14.0f);
    button_size = icon_size + pad * 2.0f;
#endif

    if (out_button_size)
        *out_button_size = button_size;
    if (out_gap)
        *out_gap = gap;
}

static bool sdl_touch_top_panel_vertical_placement(
    enum pane_placement where)
{
    return where == PLACE_LEFT_CENTER || where == PLACE_RIGHT_CENTER;
}

static bool sdl_touch_top_panel_top_placement(
    enum pane_placement where)
{
    return where == PLACE_TOP_LEFT || where == PLACE_TOP_CENTER
        || where == PLACE_TOP_RIGHT;
}

static void sdl_touch_top_panel_avoid_status_depth(
    enum pane_placement where, const SDL_Rect* screen, SDL_FRect* panel)
{
    enum pane_placement status_where = PLACE_BOTTOM_CENTER;
    SDL_FRect status_depth;
    float gap;
    float margin;
    bool horizontal_overlap;

    if (!screen || !panel)
        return;

    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].enabled
            && pane_config[i].pane == PANE_STATUS_DEPTH)
        {
            status_where = pane_config[i].where;
            break;
        }
    }
    if (!sdl_status_depth_pane_current_rect(&status_depth))
        return;

    horizontal_overlap = panel->x < status_depth.x + status_depth.w
        && panel->x + panel->w > status_depth.x;
    if (!horizontal_overlap)
        return;

    gap = (float)sdl_overlay_inner_gap_px();
    if (sdl_left_panel_pane_placement_is_bottom(where)
        && sdl_left_panel_pane_placement_is_bottom(status_where))
    {
        float limit = status_depth.y - gap;

        if (panel->y + panel->h > limit)
            panel->y = limit - panel->h;
    } else if (sdl_touch_top_panel_top_placement(where)
        && sdl_touch_top_panel_top_placement(status_where))
    {
        float limit = status_depth.y + status_depth.h + gap;

        if (panel->y < limit)
            panel->y = limit;
    }

    margin = (float)sdl_overlay_margin_px();
    panel->y = sdl_touch_pane_clampf(panel->y,
        (float)screen->y + margin,
        (float)(screen->y + screen->h) - panel->h - margin);
}

bool sdl_touch_top_panel_compute_layout_for_anchor(const SDL_Rect* screen,
    const SDL_Rect* anchor, enum pane_placement where,
    SDL_FRect* button_rects, SDL_FRect* out_panel)
{
    float screen_w;
    float side_margin;
    float max_panel_w;
    float max_panel_h;
    float gap;
    float panel_w;
    float panel_h;
    float button_size;
    float configured_size;
    SDL_FRect panel;
    int visible_slots[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int active_count;
    int columns;
    int rows;
    bool vertical;

    if (!screen || !anchor || !sdl_rect_has_area(screen)
        || !sdl_rect_has_area(anchor))
    {
        return false;
    }

    /* The configured pane rectangle is an alignment anchor, not a size cap.
     * Corner anchors can be only a few cells wide, which previously collapsed
     * Bottom Right Quick Access buttons almost to zero in portrait mode. */
    screen_w = (float)screen->w;
    active_count = sdl_touch_top_panel_visible_slots(visible_slots,
        SDL_TOUCH_TOP_PANEL_BUTTON_COUNT);
    if (active_count <= 0)
        return false;

    side_margin = sdl_touch_pane_clampf(screen_w * 0.02f, 6.0f, 18.0f);
    max_panel_w = screen_w - side_margin * 2.0f;
    max_panel_h = (float)screen->h - side_margin * 2.0f;
    vertical = sdl_touch_top_panel_vertical_placement(where);
    if (sdl_touch_thumb_description_open()) {
        rows = vertical ? active_count : 1;
        columns = vertical ? 1 : active_count;
    } else if (vertical) {
        rows = active_count;
        columns = 1;
    } else {
        rows = MIN(get_sdl_touch_top_panel_rows(), active_count);
        columns = (active_count + rows - 1) / rows;
    }
    if (columns < 1 || rows < 1 || active_count < 1)
        return false;

    configured_size = get_sdl_touch_top_panel_size();
    sdl_touch_top_panel_button_metrics_for_size(configured_size,
        &button_size, &gap);

    if (vertical) {
        float fit_size;

        if (max_panel_h <= gap * (float)(rows - 1))
            return false;
        fit_size = (max_panel_h - gap * (float)(rows - 1))
            / (float)rows;
        if (fit_size > max_panel_w)
            fit_size = max_panel_w;
        if (configured_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH
            || button_size > fit_size)
        {
            button_size = fit_size;
        }
        if (button_size <= 0.0f)
            return false;
    }

    /* The available span depends on the panel's vertical band.  A few passes
     * converge the stretch size while accounting for side panes intersecting
     * that band.  Fixed sizes remain fixed unless the window cannot contain
     * the requested columns. */
    for (int pass = 0; !vertical && pass < 3; pass++) {
        float available_w;
        float fit_size;

        panel_h = button_size * (float)rows
            + gap * (float)(rows - 1);
        available_w = sdl_touch_top_panel_available_width(screen, anchor,
            where, panel_h, gap, NULL, NULL);
        if (available_w > max_panel_w)
            available_w = max_panel_w;
        if (available_w <= gap * (float)(columns - 1))
            return false;

        fit_size = (available_w - gap * (float)(columns - 1))
            / (float)columns;
        if (configured_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH) {
            button_size = fit_size;
            gap = sdl_touch_pane_clampf(button_size * 0.10f, 6.0f, 18.0f);
        } else if (button_size > fit_size) {
            button_size = fit_size;
        }
    }
    if (button_size <= 0.0f)
        return false;

    if (!vertical
        && configured_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH)
    {
        float available_w;

        panel_h = button_size * (float)rows
            + gap * (float)(rows - 1);
        available_w = sdl_touch_top_panel_available_width(screen, anchor,
            where, panel_h, gap, NULL, NULL);
        if (available_w > max_panel_w)
            available_w = max_panel_w;
        if (available_w <= gap * (float)(columns - 1))
            return false;
        button_size = (available_w - gap * (float)(columns - 1))
            / (float)columns;
    }

    panel_w = button_size * (float)columns
        + gap * (float)(columns - 1);

    panel_h = button_size * (float)rows + gap * (float)(rows - 1);
    panel = sdl_overlay_panel_rect(anchor, where, (int)(panel_w + 0.5f),
        (int)(panel_h + 0.5f), screen);
    if (!vertical) {
        float clear_left;
        float clear_right;

        (void)sdl_touch_top_panel_available_width(screen, anchor, where,
            panel_h, gap, &clear_left, &clear_right);
        if (configured_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH) {
            panel.x = clear_left;
            panel.w = MAX(1.0f, clear_right - clear_left);
        } else if (panel.w <= clear_right - clear_left) {
            panel.x = sdl_touch_pane_clampf(panel.x, clear_left,
                clear_right - panel.w);
        }
    }
    sdl_touch_top_panel_avoid_status_depth(where, screen, &panel);
    panel_w = panel.w;
    panel_h = panel.h;

    if (out_panel)
        *out_panel = panel;

    if (button_rects) {
        float grid_h = button_size * (float)rows + gap * (float)(rows - 1);
        float grid_y = panel.y + (panel.h - grid_h) * 0.5f;

        for (int i = 0; i < SDL_TOUCH_TOP_PANEL_BUTTON_COUNT; i++)
            button_rects[i] = (SDL_FRect){ 0 };

        for (int i = 0; i < active_count; i++) {
            int slot = visible_slots[i];
            int row = i / columns;
            int col = i % columns;
            int row_count = active_count - row * columns;
            float row_w;
            float row_x;

            if (row_count > columns)
                row_count = columns;
            row_w = button_size * (float)row_count
                + gap * (float)(row_count - 1);
            row_x = panel.x + (panel.w - row_w) * 0.5f;

            button_rects[slot] = (SDL_FRect){
                .x = row_x + (button_size + gap) * (float)col,
                .y = grid_y + (button_size + gap) * (float)row,
                .w = button_size,
                .h = button_size,
            };
        }
    }

    return true;
}

bool sdl_touch_top_panel_compute_layout_for_screen(
    const SDL_Rect* screen, SDL_FRect* button_rects, SDL_FRect* out_panel)
{
    return sdl_touch_top_panel_compute_layout_for_anchor(screen, screen,
        PLACE_BOTTOM_CENTER, button_rects, out_panel);
}

bool sdl_touch_top_panel_point_to_slot(float x, float y, int* out_slot)
{
    const SDL_FRect* button_rects = g_touch_top_panel_cached_buttons;
    int visible_slots[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int active_count = sdl_touch_top_panel_visible_slots(visible_slots,
        SDL_TOUCH_TOP_PANEL_BUTTON_COUNT);

    if (out_slot)
        *out_slot = -1;
    if (!g_touch_top_panel_cached_layout_valid
        || g_touch_top_panel_cached_generation != g_sdl_present_generation)
    {
        if (!sdl_touch_top_panel_compute_layout(
                g_touch_top_panel_cached_buttons, NULL))
        {
            g_touch_top_panel_cached_layout_valid = false;
            return false;
        }
        g_touch_top_panel_cached_layout_valid = true;
        g_touch_top_panel_cached_generation = g_sdl_present_generation;
    }

    for (int i = 0; i < active_count; i++) {
        int slot = visible_slots[i];
        const SDL_FRect* rect = &button_rects[slot];

        if (x >= rect->x && x < rect->x + rect->w
            && y >= rect->y && y < rect->y + rect->h)
        {
            if (out_slot)
                *out_slot = slot;
            return true;
        }
    }

    return false;
}

int sdl_touch_top_panel_binding_for_slot(int slot, bool long_press)
{
    if (slot < 0 || slot >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;

    return long_press ? config.touch_top_panel_long_bindings[slot]
                      : config.touch_top_panel_bindings[slot];
}

static int sdl_touch_top_panel_display_binding_for_slot(int slot)
{
    int description_action =
        sdl_touch_top_panel_description_action_for_slot(slot);
    int binding = sdl_touch_top_panel_binding_for_slot(slot, false);

    if (description_action != GAMEPAD_BIND_NONE)
        return description_action;
    if (binding != GAMEPAD_BIND_NONE)
        return binding;

    return sdl_touch_top_panel_binding_for_slot(slot, true);
}

void sdl_touch_top_panel_label_for_slot(int slot, bool long_press,
    char* buf, size_t buflen)
{
    int binding;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (slot < 0 || slot >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return;

    binding = sdl_touch_top_panel_binding_for_slot(slot, long_press);
    if (binding == GAMEPAD_BIND_NONE) {
        SDL_strlcpy(buf, "Off", buflen);
        return;
    }

    {
        int description_action =
            sdl_touch_top_panel_description_action_for_binding(binding);

        if (description_action != GAMEPAD_BIND_NONE) {
            sdl_touch_context_label_for_binding(description_action, buf,
                buflen);
            return;
        }
    }

    if (binding == TOUCH_BIND_MAIN_MENU_KNOWLEDGE) {
        SDL_strlcpy(buf, "Known Lore", buflen);
        return;
    }
    if (binding == TOUCH_BIND_MAIN_MENU_HINTS_QUESTS) {
        SDL_strlcpy(buf, "Hints", buflen);
        return;
    }

    sdl_touch_context_label_for_binding(binding, buf, buflen);
}

static bool sdl_touch_top_panel_can_draw_tiles(void)
{
    return g_state.use_tiles && g_state.tileset;
}

static int sdl_touch_top_panel_player_action_kind_for_binding(int binding)
{
    switch (binding) {
    case 'z': return SDL_PLAYER_ACTION_WAIT;
    case 'u': return SDL_PLAYER_ACTION_USE;
    case 'S': return SDL_PLAYER_ACTION_STEALTH;
    case 's': return SDL_PLAYER_ACTION_SING;
    case 'X': return SDL_PLAYER_ACTION_EXCHANGE;
    case '-': return SDL_PLAYER_ACTION_FLETCH;
    case 'x': return SDL_PLAYER_ACTION_EXAMINE;
    case 'a': return SDL_PLAYER_ACTION_ACTIVATE;
    case 'p': return SDL_PLAYER_ACTION_HORN;
    case '\t': return SDL_PLAYER_ACTION_SHOOT;
    case 't':
    case KTRL('T'):
    case SDL_TOUCH_THUMB_BIND_QUICK_THROW:
    case SDL_TOUCH_THUMB_BIND_QUICK_THROW_TARGET:
        return SDL_PLAYER_ACTION_QUICK_THROW;
    case 'Z': return SDL_PLAYER_ACTION_REST;
    case KTRL('F'):
    case SDL_TOUCH_THUMB_BIND_CHANGE_QUIVER:
        return SDL_PLAYER_ACTION_SWAP_QUIVERS;
    case KTRL('A'): return SDL_PLAYER_ACTION_CHANGE_STAFF;
    case 'c': return SDL_PLAYER_ACTION_CLOSE_DOOR;
    case 'b': return SDL_PLAYER_ACTION_BASH_DOOR;
    default:
        return SDL_PLAYER_ACTION_NONE;
    }
}

static bool sdl_touch_top_panel_player_action_icon_for_binding(int binding,
    byte* out_attr, char* out_char, cptr* out_fallback)
{
    int kind = sdl_touch_top_panel_player_action_kind_for_binding(binding);

    /* While the floor Description is open, its x footer action is the
     * contextual Use/Wield verb, not the action that opens Description. */
    if (binding == 'x' && sdl_touch_thumb_description_open()) {
        if (out_attr)
            *out_attr = (byte)(TILE_FLAG | SDL_UI_SYMBOL_ROW);
        if (out_char)
            *out_char = (char)(TILE_FLAG | SDL_UI_SYMBOL_USE);
        if (out_fallback)
            *out_fallback = "Use";
        return true;
    }

    if (kind == SDL_PLAYER_ACTION_NONE)
        return false;

    sdl_player_action_menu_tile_for_kind(kind, out_attr, out_char);
    if (out_fallback)
        *out_fallback = sdl_player_action_menu_fallback_for_kind(kind);
    return true;
}

static bool sdl_touch_top_panel_tile_for_binding(int binding, byte* out_attr,
    char* out_char, cptr* out_fallback)
{
    byte row = 12;
    byte col = 10;
    cptr fallback = "?";
    bool has_tile = true;

    if (sdl_touch_top_panel_player_action_icon_for_binding(binding, out_attr,
            out_char, out_fallback))
    {
        return true;
    }

    /* Only Quick Access-only bindings belong here.  Actions shared with the
     * player wheel must use sdl_player_action_menu_tile_for_kind() above. */
    switch (binding) {
    case INPUT_BIND_CONFIRM:
    case ' ':
    case 'g':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_PICK; fallback = "Pick";
        break;
    case 'o':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_OPEN_DOOR; fallback = "Open";
        break;
    case 'D':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_DISARM; fallback = "D";
        break;
    case 'j':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_SUPPLY; fallback = "S";
        break;
    case '0':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_SMITHING; fallback = "Sm";
        break;
    case 'l':
    case 'L':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_VIEW; fallback = "V";
        break;
    case 'M':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_MAP; fallback = "Map";
        break;
    case TOUCH_BIND_MAIN_MENU_KNOWLEDGE:
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_LORE; fallback = "Lore";
        break;
    case TOUCH_BIND_MAIN_MENU_HINTS_QUESTS:
        row = 12; col = 10; fallback = "?";    /* question-mark tile */
        break;
    case 'y':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_ABILITIES; fallback = "Ab";
        break;
    case 'f':
    case 'F':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_RANGED_ATTACK; fallback = "F";
        break;
    case 'q':
        row = 3; col = 0; fallback = "!";      /* potion */
        break;
    case 'i':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_INVENTORY; fallback = "I";
        break;
    case 'e':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_EQUIPPED; fallback = "E";
        break;
    case 'h':
    case '@':
        row = SDL_UI_SYMBOL_ROW; col = SDL_UI_SYMBOL_CHARACTER; fallback = "Ch";
        break;
    case TOUCH_BIND_TOGGLE_TILES:
        has_tile = false; fallback = "@";
        break;
    case KTRL('Y'):
        has_tile = false; fallback = "Dbg";
        break;
    case 'J':
        has_tile = false; fallback = "Set";
        break;
    default:
        has_tile = false;
        fallback = "";
        break;
    }

    if (out_attr)
        *out_attr = (byte)(TILE_FLAG | row);
    if (out_char)
        *out_char = (char)(TILE_FLAG | col);
    if (out_fallback)
        *out_fallback = fallback;
    return has_tile;
}

static bool sdl_touch_top_panel_render_vector_icon(int binding,
    const SDL_FRect* rect, SDL_Color color)
{
    float x;
    float y;
    float w;
    float h;
    float cx;
    float cy;
    SDL_FRect box;

    if (!rect)
        return false;

    x = rect->x;
    y = rect->y;
    w = rect->w;
    h = rect->h;
    cx = x + w * 0.5f;
    cy = y + h * 0.5f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        color.a);

    switch (binding) {
    case 'j':
        box = (SDL_FRect){ x + w * 0.19f, y + h * 0.42f,
            w * 0.62f, h * 0.34f };
        SDL_RenderRect(g_state.renderer, &box);
        SDL_RenderLine(g_state.renderer, box.x, box.y,
            box.x + box.w, box.y);
        SDL_RenderLine(g_state.renderer, x + w * 0.32f, y + h * 0.36f,
            x + w * 0.68f, y + h * 0.36f);
        SDL_RenderLine(g_state.renderer, cx, box.y,
            cx, box.y + box.h);
        return true;
    case '0':
        box = (SDL_FRect){ x + w * 0.52f, y + h * 0.24f,
            w * 0.26f, h * 0.15f };
        SDL_RenderFillRect(g_state.renderer, &box);
        SDL_RenderLine(g_state.renderer, x + w * 0.30f, y + h * 0.72f,
            x + w * 0.62f, y + h * 0.34f);
        SDL_RenderLine(g_state.renderer, x + w * 0.34f, y + h * 0.74f,
            x + w * 0.66f, y + h * 0.36f);
        SDL_RenderLine(g_state.renderer, x + w * 0.48f, y + h * 0.32f,
            x + w * 0.82f, y + h * 0.45f);
        return true;
    case 'l':
    case 'M':
    case 'L':
        SDL_RenderLine(g_state.renderer, x + w * 0.16f, cy,
            cx, y + h * 0.34f);
        SDL_RenderLine(g_state.renderer, cx, y + h * 0.34f,
            x + w * 0.84f, cy);
        SDL_RenderLine(g_state.renderer, x + w * 0.84f, cy,
            cx, y + h * 0.66f);
        SDL_RenderLine(g_state.renderer, cx, y + h * 0.66f,
            x + w * 0.16f, cy);
        sdl_touch_round_draw_circle(cx, cy, w * 0.10f, color);
        return true;
    case '\t':
        SDL_RenderLine(g_state.renderer, cx, y + h * 0.18f,
            cx, y + h * 0.82f);
        SDL_RenderLine(g_state.renderer, x + w * 0.18f, cy,
            x + w * 0.82f, cy);
        SDL_RenderLine(g_state.renderer, x + w * 0.31f, y + h * 0.31f,
            x + w * 0.69f, y + h * 0.69f);
        SDL_RenderLine(g_state.renderer, x + w * 0.69f, y + h * 0.31f,
            x + w * 0.31f, y + h * 0.69f);
        return true;
    case 'm':
        SDL_RenderLine(g_state.renderer, x + w * 0.20f, y + h * 0.30f,
            x + w * 0.80f, y + h * 0.30f);
        SDL_RenderLine(g_state.renderer, x + w * 0.20f, cy,
            x + w * 0.80f, cy);
        SDL_RenderLine(g_state.renderer, x + w * 0.20f, y + h * 0.70f,
            x + w * 0.80f, y + h * 0.70f);
        return true;
    default:
        return false;
    }
}

static void sdl_touch_top_panel_render_icon(const SDL_FRect* button_rect,
    int binding, SDL_Color color, bool hover)
{
    SDL_FRect rect;
    byte tile_attr = 0;
    char tile_char = 0;
    cptr fallback = "";
    bool has_tile;
    float icon_size;
    float max_icon;
    float grow;
    float configured_size;

    if (!button_rect)
        return;

    configured_size = get_sdl_touch_top_panel_size();
#if SIL_SDL_MOBILE_BUILD
    icon_size = MIN(button_rect->w, button_rect->h) * 0.68f;
    max_icon = MIN(button_rect->w, button_rect->h)
        - sdl_touch_pane_clampf(button_rect->w * 0.14f, 8.0f, 18.0f);
#else
    icon_size = (configured_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH)
        ? MIN(button_rect->w, button_rect->h) * 0.68f
        : (float)TILE_SIZE * configured_size;
    max_icon = MIN(button_rect->w, button_rect->h)
        - sdl_touch_pane_clampf(button_rect->w * 0.18f, 6.0f, 14.0f);
#endif
    if (max_icon < 4.0f)
        max_icon = MIN(button_rect->w, button_rect->h);
    if (icon_size > max_icon)
        icon_size = max_icon;

    rect = (SDL_FRect){
        .x = button_rect->x + (button_rect->w - icon_size) * 0.5f,
        .y = button_rect->y + (button_rect->h - icon_size) * 0.5f,
        .w = icon_size,
        .h = icon_size,
    };
    grow = hover ? sdl_touch_pane_clampf(button_rect->w * 0.08f, 2.0f, 5.0f)
                 : 0.0f;
    rect.x -= grow;
    rect.y -= grow;
    rect.w += grow * 2.0f;
    rect.h += grow * 2.0f;

    has_tile = sdl_touch_top_panel_tile_for_binding(binding, &tile_attr,
        &tile_char, &fallback);
    if (has_tile && sdl_touch_top_panel_can_draw_tiles()) {
        SDL_SetTextureAlphaMod(g_state.tileset, hover ? 255 : 226);
        sdl_draw_tileset_sprite(tile_attr, tile_char, &rect, false);
        sdl_restore_tileset_mod();
        return;
    }

    if (sdl_touch_top_panel_render_vector_icon(binding, &rect, color))
        return;

    if (!fallback || !fallback[0]) {
        static char key_glyph[4];

        /* Any assignable command without a dedicated tile/label falls back to
         * its own key glyph (e.g. Open -> "o", Bash -> "b") rather than a bare
         * "?", so every Quick Access / thumb binding stays recognizable. */
        if (binding == GAMEPAD_BIND_NONE) {
            fallback = "";
        } else if (binding >= 1 && binding <= 26) {
            key_glyph[0] = '^';
            key_glyph[1] = (char)('A' + binding - 1);
            key_glyph[2] = '\0';
            fallback = key_glyph;
        } else if (binding > 0 && binding < 128 && SDL_isprint(binding)) {
            key_glyph[0] = (char)binding;
            key_glyph[1] = '\0';
            fallback = key_glyph;
        } else {
            fallback = "?";
        }
    }

    sdl_touch_pane_draw_button_text_scaled(&rect, NULL, fallback, color,
#if SIL_SDL_MOBILE_BUILD
        0.78f, 0.82f
#else
        0.74f, 0.80f
#endif
        );
}

static void sdl_touch_top_panel_description_for_binding(int binding,
    char* buf, size_t buflen)
{
    char label[48];
    int context_binding = (binding == INPUT_BIND_CONFIRM) ? ' ' : binding;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (touch_shortcut_context_action(context_binding,
            sdl_touch_thumb_description_open(), NULL, label, sizeof(label))) {
        strnfmt(buf, buflen, "%s.", label);
        return;
    }

    switch (binding) {
    case GAMEPAD_BIND_NONE:
        SDL_strlcpy(buf, "Unbound.", buflen);
        return;
    case TOUCH_BIND_TOP_PANEL_OPEN:
        SDL_strlcpy(buf, "Overlay menu: open the shortcut menu.", buflen);
        return;
    case TOUCH_BIND_TOP_PANEL_CLOSE:
        SDL_strlcpy(buf, "Overlay menu: close the shortcut menu.", buflen);
        return;
    case TOUCH_BIND_MAIN_MENU_KNOWLEDGE:
        SDL_strlcpy(buf, "Known Lore: open the lore browser.", buflen);
        return;
    case TOUCH_BIND_MAIN_MENU_HINTS_QUESTS:
        SDL_strlcpy(buf,
            "Hints & Quests: open saved hints, quest notes, and thrall requests.",
            buflen);
        return;
    case TOUCH_BIND_TOGGLE_TILES:
        strnfmt(buf, buflen, "%s.", get_sdl_tiles() ? "Change to ASCII"
                                                     : "Change to Tiles");
        return;
    case 'j':
        SDL_strlcpy(buf, "Supply: open supplies and carried resources.",
            buflen);
        return;
    case '0':
        SDL_strlcpy(buf, "Smithing: open the forge crafting screen.", buflen);
        return;
    case 'l':
        SDL_strlcpy(buf, "View: inspect the map, floor items, and nearby creatures.",
            buflen);
        return;
    case '\t':
        SDL_strlcpy(buf, "Weapon: change active melee or ranged weapon.", buflen);
        return;
    case 'y':
        SDL_strlcpy(buf, "Abilities: open the ability screen.", buflen);
        return;
    case 'M':
        SDL_strlcpy(buf, "Map: view the full dungeon map.", buflen);
        return;
    default:
        break;
    }

    label[0] = '\0';
    binding_action_short(binding, label, sizeof(label));
    if (label[0])
        strnfmt(buf, buflen, "%s.", label);
    else
        strnfmt(buf, buflen, "Action %d.", binding);
}

static void sdl_touch_top_panel_description_for_slot(int slot, char* buf,
    size_t buflen)
{
    int tap_binding;
    char tap_desc[192];

    if (!buf || !buflen)
        return;
    buf[0] = '\0';
    if (slot < 0 || slot >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return;

    tap_binding = sdl_touch_top_panel_binding_for_slot(slot, false);
    {
        int description_action =
            sdl_touch_top_panel_description_action_for_binding(tap_binding);

        if (description_action != GAMEPAD_BIND_NONE)
            tap_binding = description_action;
    }
    sdl_touch_top_panel_description_for_binding(tap_binding, tap_desc,
        sizeof(tap_desc));
    strnfmt(buf, buflen, "Tap: %s\nHold: edit this button.", tap_desc);
}

static void sdl_touch_top_panel_render_tooltip(const SDL_FRect* anchor,
    int slot)
{
    SDL_Rect screen;
    TTF_Font* font;
    SDL_Texture* texture;
    SDL_FRect box;
    SDL_FRect text_dst;
    SDL_Color text_color = g_state.palette[TERM_WHITE];
    SDL_Color border = g_state.palette[TERM_YELLOW];
    char description[384];
    float pad;
    float gap;
    float screen_margin;
    float max_box_w;
    float max_text_w;
    int font_px;
    int text_w = 0;
    int text_h = 0;

    if (!anchor)
        return;

    sdl_touch_top_panel_description_for_slot(slot, description,
        sizeof(description));
    if (!description[0])
        return;

    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return;

    font_px = sdl_object_tooltip_font_px();
    font = sdl_story_font_for_height_slot(font_px,
        STORY_FONT_SLOT_SECONDARY);
    if (!font)
        return;

    pad = sdl_touch_pane_clampf((float)font_px * 0.36f, 7.0f, 14.0f);
    gap = sdl_touch_pane_clampf((float)font_px * 0.28f, 6.0f, 12.0f);
    screen_margin = sdl_touch_pane_clampf(g_state.system_scale * 4.0f,
        4.0f, 10.0f);
    max_box_w = (float)screen.w - screen_margin * 2.0f;
    if (max_box_w > 420.0f)
        max_box_w = 420.0f;
    if (max_box_w <= pad * 2.0f)
        return;

    max_text_w = max_box_w - pad * 2.0f;
    texture = sdl_ui_wrapped_text_texture(font, description,
        MAX(1, (int)(max_text_w + 0.5f)), text_color, &text_w, &text_h);
    if (!texture)
        return;

    box.w = (float)text_w + pad * 2.0f;
    box.h = (float)text_h + pad * 2.0f;
    box.x = anchor->x + anchor->w * 0.5f - box.w * 0.5f;
    box.y = anchor->y - box.h - gap;
    if (box.y < (float)screen.y + screen_margin)
        box.y = anchor->y + anchor->h + gap;

    if (box.w + screen_margin * 2.0f <= (float)screen.w) {
        box.x = sdl_touch_pane_clampf(box.x,
            (float)screen.x + screen_margin,
            (float)(screen.x + screen.w) - box.w - screen_margin);
    } else {
        box.x = (float)screen.x + screen_margin;
    }
    if (box.h + screen_margin * 2.0f <= (float)screen.h) {
        box.y = sdl_touch_pane_clampf(box.y,
            (float)screen.y + screen_margin,
            (float)(screen.y + screen.h) - box.h - screen_margin);
    } else {
        box.y = (float)screen.y + screen_margin;
    }

    text_dst = (SDL_FRect){
        .x = box.x + pad,
        .y = box.y + pad,
        .w = (float)text_w,
        .h = (float)text_h,
    };

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 218);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
        166);
    SDL_RenderRect(g_state.renderer, &box);

    SDL_RenderTexture(g_state.renderer, texture, NULL, &text_dst);
}

void sdl_touch_top_panel_render_buttons(
    const SDL_FRect* button_rects)
{
    SDL_Color frame = g_state.palette[TERM_WHITE];
    SDL_Color muted = g_state.palette[TERM_SLATE];
    SDL_Color selected = g_state.palette[TERM_YELLOW];
    int visible_slots[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int active_count = sdl_touch_top_panel_visible_slots(visible_slots,
        SDL_TOUCH_TOP_PANEL_BUTTON_COUNT);
    int tooltip_slot = -1;
    const SDL_FRect* tooltip_rect = NULL;

    if (!button_rects)
        return;

    for (int i = 0; i < active_count; i++) {
        int slot = visible_slots[i];
        SDL_Color icon_color;
        SDL_Color border_color;
        SDL_FRect shadow;

        /* Slots dropped by the side-pane fit have a zeroed rect; skip them so
         * the visible row matches what compute_layout actually placed. */
        if (button_rects[slot].w <= 0.0f || button_rects[slot].h <= 0.0f)
            continue;

        shadow = button_rects[slot];
        int binding = sdl_touch_top_panel_display_binding_for_slot(slot);
        bool toggled = sdl_pointer_attack_binding_toggled(binding);
        bool hovered = slot == g_touch_top_panel_hover_slot;
        bool pressed = slot == g_touch_top_panel_pressed_slot;
        bool flashed = slot == g_touch_top_panel_flash_slot;
        bool active = hovered || pressed || flashed || toggled;

        shadow.x += 2.0f;
        shadow.y += 2.0f;
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 150);
        SDL_RenderFillRect(g_state.renderer, &shadow);

        if (binding == GAMEPAD_BIND_NONE) {
            SDL_SetRenderDrawColor(g_state.renderer, 26, 26, 26, 250);
            icon_color = muted;
            border_color = hovered ? selected : muted;
        } else if (active) {
            SDL_SetRenderDrawColor(g_state.renderer, 34, 34, 34, 252);
            icon_color = selected;
            border_color = selected;
        } else {
            SDL_SetRenderDrawColor(g_state.renderer, 34, 34, 34, 250);
            icon_color = frame;
            border_color = frame;
        }

        SDL_RenderFillRect(g_state.renderer, &button_rects[slot]);
        SDL_SetRenderDrawColor(g_state.renderer, border_color.r, border_color.g,
            border_color.b, 220);
        SDL_RenderRect(g_state.renderer, &button_rects[slot]);

        sdl_touch_top_panel_render_icon(&button_rects[slot], binding,
            icon_color, active);

        if (hovered || pressed) {
            tooltip_slot = slot;
            tooltip_rect = &button_rects[slot];
        }
    }

    if (tooltip_slot >= 0)
        sdl_touch_top_panel_render_tooltip(tooltip_rect, tooltip_slot);
}

void sdl_touch_top_panel_render(void)
{
    if (!sdl_touch_top_panel_layout_visible()) {
        g_touch_top_panel_cached_layout_valid = false;
        sdl_touch_top_panel_cancel_press();
        sdl_touch_top_panel_set_hover_slot(-1);
        return;
    }

    if (!sdl_touch_top_panel_compute_layout(g_touch_top_panel_cached_buttons,
            NULL))
    {
        g_touch_top_panel_cached_layout_valid = false;
        sdl_touch_top_panel_set_hover_slot(-1);
        return;
    }
    g_touch_top_panel_cached_layout_valid = true;
    g_touch_top_panel_cached_generation = g_sdl_present_generation;

    sdl_touch_top_panel_render_buttons(g_touch_top_panel_cached_buttons);

    if (g_touch_top_panel_flash_slot >= 0
        && SDL_GetTicksNS() >= g_touch_top_panel_flash_until)
    {
        g_touch_top_panel_flash_slot = -1;
        g_touch_top_panel_flash_until = 0;
    }
}

void sdl_touch_top_panel_send_slot(int slot, bool long_press)
{
    int binding = sdl_touch_top_panel_binding_for_slot(slot, long_press);

    if (!sdl_main_screen_click_shortcuts_active())
        return;

    /* A press or mouse hover displays the button description.  Clear it
     * before the selected command can open its own prompt or modal screen. */
    sdl_touch_top_panel_set_hover_slot(-1);

    if (long_press) {
        sdl_touch_run_quick_access_picker(slot);
        return;
    }

    sdl_touch_pane_send_binding(sdl_touch_context_binding(binding), false,
        false);
}

void sdl_touch_top_panel_cancel_press(void)
{
    if (!g_touch_top_panel_press.active && g_touch_top_panel_pressed_slot < 0)
        return;

    g_touch_top_panel_press.active = false;
    g_touch_top_panel_press.finger_id = 0;
    g_touch_top_panel_press.slot = -1;
    g_touch_top_panel_press.start_x = 0.0f;
    g_touch_top_panel_press.start_y = 0.0f;
    g_touch_top_panel_press.start_time = 0;
    g_touch_top_panel_pressed_slot = -1;
    g_state.need_present = true;
}

bool sdl_touch_top_panel_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id)
{
    int slot = -1;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_touch_top_panel_point_to_slot(x, y, &slot))
        return false;
    if (slot < 0)
        return false;

    sdl_touch_top_panel_cancel_press();
    sdl_touch_top_panel_set_hover_slot(slot);
    g_touch_top_panel_press.active = true;
    g_touch_top_panel_press.finger_id = finger_id;
    g_touch_top_panel_press.slot = slot;
    g_touch_top_panel_press.start_x = x;
    g_touch_top_panel_press.start_y = y;
    g_touch_top_panel_press.start_time = SDL_GetTicksNS();
    g_touch_top_panel_pressed_slot = slot;
    g_state.need_present = true;
    return true;
}

bool sdl_touch_top_panel_handle_pointer_motion(float x, float y, bool mouse,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float threshold;
    float start_x;
    float start_y;
    int slot = -1;

    if (!g_touch_top_panel_press.active
        || g_touch_top_panel_press.finger_id != finger_id)
    {
        /* Fingers do not hover.  Avoid running the complete top-panel/status
         * placement calculation for every Android movement sample when no
         * top-panel button owns that finger. */
        if (!mouse)
            return false;
        if (sdl_touch_top_panel_point_to_slot(x, y, &slot) && slot >= 0) {
            sdl_touch_top_panel_set_hover_slot(slot);
            return true;
        }
        sdl_touch_top_panel_set_hover_slot(-1);
        return false;
    }

    dx = x - g_touch_top_panel_press.start_x;
    dy = y - g_touch_top_panel_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_touch_swipe_threshold_px();
    if (dx > threshold || dy > threshold) {
        start_x = g_touch_top_panel_press.start_x;
        start_y = g_touch_top_panel_press.start_y;
        sdl_touch_top_panel_cancel_press();
        if (sdl_touch_swipe_handle_pointer_down(start_x, start_y, finger_id))
            (void)sdl_touch_swipe_handle_pointer_motion(x, y, finger_id);
        return true;
    }

    if (!sdl_touch_top_panel_point_to_slot(x, y, &slot)
        || slot != g_touch_top_panel_press.slot)
    {
        sdl_touch_top_panel_cancel_press();
        sdl_touch_top_panel_set_hover_slot(slot);
        return true;
    }

    sdl_touch_top_panel_set_hover_slot(slot);
    return true;
}

bool sdl_touch_top_panel_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id)
{
    Uint64 press_time;
    bool long_press;
    int slot;
    int release_slot = -1;

    if (!g_touch_top_panel_press.active
        || g_touch_top_panel_press.finger_id != finger_id)
    {
        return false;
    }

    slot = g_touch_top_panel_press.slot;
    press_time = SDL_GetTicksNS() - g_touch_top_panel_press.start_time;
    long_press = (press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL);
    (void)sdl_touch_top_panel_point_to_slot(x, y, &release_slot);
    sdl_touch_top_panel_cancel_press();
    sdl_touch_top_panel_set_hover_slot(release_slot);
    if (release_slot != slot)
        return true;

    sdl_touch_top_panel_send_slot(slot, long_press);
    g_touch_top_panel_flash_slot = slot;
    g_touch_top_panel_flash_until = SDL_GetTicksNS() + 150000000ULL;
    g_state.need_present = true;
    return true;
}

int sdl_touch_top_panel_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_touch_top_panel_press.active)
        return -1;
    if (!sdl_touch_top_panel_layout_visible())
        return 0;

    elapsed = now_ns - g_touch_top_panel_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_touch_top_panel_flush_pending_press(Uint64 now_ns)
{
    int slot;

    if (!g_touch_top_panel_press.active)
        return false;
    if (!sdl_touch_top_panel_layout_visible()
        || !sdl_main_screen_click_shortcuts_active())
    {
        sdl_touch_top_panel_cancel_press();
        return false;
    }
    if (now_ns - g_touch_top_panel_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    slot = g_touch_top_panel_press.slot;
    sdl_touch_top_panel_cancel_press();
    sdl_touch_top_panel_set_hover_slot(slot);
    sdl_touch_top_panel_send_slot(slot, true);
    g_touch_top_panel_flash_slot = slot;
    g_touch_top_panel_flash_until = SDL_GetTicksNS() + 150000000ULL;
    g_state.need_present = true;
    return true;
}
