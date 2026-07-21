#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "ui/menu-click.h"

bool sdl_gamepad_shift_active(void)
{
    return g_gamepad_state.shift_held > 0;
}

bool sdl_gamepad_ctrl_active(void)
{
    return g_gamepad_state.ctrl_held > 0;
}

bool sdl_gamepad_alt_active(void)
{
    return g_gamepad_state.alt_held > 0;
}

int sdl_gamepad_modifier_index(int binding)
{
    switch (binding) {
    case GAMEPAD_BIND_SHIFT:
        return GAMEPAD_MODIFIER_SHIFT;
    case GAMEPAD_BIND_CTRL:
        return GAMEPAD_MODIFIER_CTRL;
    case GAMEPAD_BIND_ALT:
        return GAMEPAD_MODIFIER_ALT;
    default:
        return -1;
    }
}

int sdl_gamepad_single_active_modifier(void)
{
    int active = GAMEPAD_BIND_NONE;

    if (sdl_gamepad_shift_active())
        active = GAMEPAD_BIND_SHIFT;
    if (sdl_gamepad_ctrl_active()) {
        if (active != GAMEPAD_BIND_NONE)
            return GAMEPAD_BIND_NONE;
        active = GAMEPAD_BIND_CTRL;
    }
    if (sdl_gamepad_alt_active()) {
        if (active != GAMEPAD_BIND_NONE)
            return GAMEPAD_BIND_NONE;
        active = GAMEPAD_BIND_ALT;
    }

    return active;
}

int sdl_gamepad_combo_binding_for_input(int modifier, int type, int id)
{
    int modifier_index = sdl_gamepad_modifier_index(modifier);

    if (modifier_index < 0)
        return GAMEPAD_BIND_NONE;

    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id >= 0 && id < SDL_GAMEPAD_BUTTON_COUNT)
            return config.gamepad_button_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        if (id >= 0 && id < GAMEPAD_TRIGGER_COUNT)
            return config.gamepad_trigger_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_left_stick_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_right_stick_combo_bindings[modifier_index][id];
        break;
    default:
        break;
    }

    return GAMEPAD_BIND_NONE;
}

void sdl_gamepad_mark_auto_ui(void)
{
    if (config.gamepad_auto_mode)
        g_gamepad_auto_ui = true;
}

void sdl_gamepad_apply_modifier(int binding, bool down)
{
    int delta = down ? 1 : -1;

    if (binding == GAMEPAD_BIND_SHIFT) {
        g_gamepad_state.shift_held += delta;
        if (g_gamepad_state.shift_held < 0)
            g_gamepad_state.shift_held = 0;
    } else if (binding == GAMEPAD_BIND_CTRL) {
        g_gamepad_state.ctrl_held += delta;
        if (g_gamepad_state.ctrl_held < 0)
            g_gamepad_state.ctrl_held = 0;
        sdl_pointer_attack_clear_hover();
        sdl_pointer_attack_clear_touch_selection();
        sdl_pointer_attack_cancel_touch_press();
        sdl_mouse_path_cancel();
        g_state.need_present = true;
    } else if (binding == GAMEPAD_BIND_ALT) {
        g_gamepad_state.alt_held += delta;
        if (g_gamepad_state.alt_held < 0)
            g_gamepad_state.alt_held = 0;
    }

    if (down)
        (void)sdl_gamepad_resolve_pending_shoulder_with_modifier(binding);
}

void sdl_send_macro_key(int key, bool shift, bool ctrl, bool alt)
{
    if (alt)
        return;

    if (ctrl)
    {
        if (SDL_isalpha(key))
            Term_keypress(KTRL(key));
        return;
    }

    if (shift)
    {
        int shifted = sdl_shifted_ascii_for_key(key);

        if (SDL_isalpha(key))
            key = SDL_toupper(key);
        else if (shifted)
            key = shifted;
    }

    if (key > 0 && key < 256)
        Term_keypress(key);
}

int sdl_keymap_mode(void)
{
    if (!hjkl_movement && !angband_keyset)
        return KEYMAP_MODE_SIL;
    if (hjkl_movement && !angband_keyset)
        return KEYMAP_MODE_SIL_HJKL;
    if (!hjkl_movement && angband_keyset)
        return KEYMAP_MODE_ANGBAND;
    return KEYMAP_MODE_ANGBAND_HJKL;
}

int sdl_shifted_ascii_for_key(int key)
{
    if (key < 0 || key >= 256)
        return 0;

    switch (key) {
    case '1': return '!';
    case '2': return '@';
    case '3': return '#';
    case '4': return '$';
    case '5': return '%';
    case '6': return '^';
    case '7': return '&';
    case '8': return '*';
    case '9': return '(';
    case '0': return ')';
    case '-': return '_';
    case '=': return '+';
    case ',': return '<';
    case '.': return '>';
    case '/': return '?';
    case '[': return '{';
    case ']': return '}';
    case ';': return ':';
    case '\'': return '"';
    case '\\': return '|';
    case '`': return '~';
    default: return 0;
    }
}

char sdl_direction_char_for_key(int key)
{
    switch (key) {
        case SDLK_UP:
        case SDLK_KP_8:
            return '8';
        case SDLK_DOWN:
        case SDLK_KP_2:
            return '2';
        case SDLK_LEFT:
        case SDLK_KP_4:
            return '4';
        case SDLK_RIGHT:
        case SDLK_KP_6:
            return '6';
        case SDLK_KP_1:
        case SDLK_END:
            return '1';
        case SDLK_KP_3:
        case SDLK_PAGEDOWN:
            return '3';
        case SDLK_KP_7:
        case SDLK_HOME:
            return '7';
        case SDLK_KP_9:
        case SDLK_PAGEUP:
            return '9';
        case SDLK_KP_5:
            return '5';
        default:
            break;
    }

    if (SDL_isprint(key) && key > 0 && key < 256)
        return (char)key;

    return 0;
}

int sdl_direction_for_key_char(char ch)
{
    int dir;
    int mode;
    cptr act;

    if (!ch)
        return 0;

    dir = target_dir(ch);
    if (dir)
        return dir;

    mode = sdl_keymap_mode();
    act = keymap_act[mode][(byte)ch];
    if (act && streq(act, "z"))
        return 5;

    return 0;
}

static u16b sdl_movement_modifiers_from_sdl(SDL_Keymod mod)
{
    u16b modifiers = 0;

    if (mod & SDL_KMOD_SHIFT)
        modifiers |= MOVEMENT_INPUT_MODIFIER_SHIFT;
    if (mod & SDL_KMOD_CTRL)
        modifiers |= MOVEMENT_INPUT_MODIFIER_CTRL;
    if (mod & SDL_KMOD_ALT)
        modifiers |= MOVEMENT_INPUT_MODIFIER_ALT;
    if (mod & SDL_KMOD_GUI)
        modifiers |= MOVEMENT_INPUT_MODIFIER_META;

    return modifiers;
}

/*
 * Movement is only submitted during live gameplay input: a real command
 * request, a direction prompt, or targeting. A non-NONE active context is the
 * primary signal, but that context can survive into a modal screen that was
 * not entered through a fresh command request. character_icky is set whenever
 * a screen is saved (options, inventory, knowledge, the movement-binding menu
 * itself, ...), so it reliably means "a modal is up, do not steal keys for the
 * player." Without this guard, presets that bind arrow/letter keys would eat
 * those keys inside menus instead of letting them navigate.
 */
static bool sdl_movement_input_is_live(void)
{
    return !character_icky
        && movement_input_active_context() != MOVEMENT_INPUT_CONTEXT_NONE;
}

static bool sdl_submit_movement_command(
    const movement_input_command* command)
{
    if (!command || !sdl_movement_input_is_live())
        return false;
    if (!movement_input_submit_command(command))
        return false;

    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

static bool sdl_submit_legacy_keypad_movement(u16b action, int dir)
{
    movement_input_command command;
    u16b direction = MOVEMENT_INPUT_DIRECTION_NONE;
    u16b context = movement_input_active_context();

    if (!sdl_movement_input_is_live())
        return false;
    if (movement_input_action_is_directional(action))
    {
        if (!movement_input_direction_from_legacy_keypad(dir, &direction))
            return false;
    }

    movement_input_command_clear(&command);
    command.context = context;
    command.action = action;
    command.direction = direction;

    return sdl_submit_movement_command(&command);
}

bool sdl_try_send_movement_event(const SDL_KeyboardEvent* key_event)
{
    movement_input_command command;
    u16b context;
    u16b modifiers;
    u32b trigger;
    u32b trigger_aux;

    if (!key_event)
        return false;

    if (!sdl_movement_input_is_live())
        return false;
    context = movement_input_active_context();

    modifiers = sdl_movement_modifiers_from_sdl(key_event->mod);
    trigger = (u32b)key_event->scancode;
    trigger_aux = (u32b)SDL_GetKeyFromScancode(key_event->scancode,
        SDL_KMOD_NONE, false);

    if (!sdl_config_resolve_movement_binding(&config, context, trigger,
            trigger_aux, modifiers, &command))
    {
        return false;
    }

    return sdl_submit_movement_command(&command);
}

/*
 * Letter-based movement presets take over some letters' normal commands while
 * in the dungeon. That shadows both lowercase commands (w = wield, s = sing)
 * and Shift/capital commands (S = stealth, D = disarm, ...). Alt is the one
 * free modifier, so:
 *   Alt+<letter>       -> the lowercase command (Alt+w = wield, Alt+s = sing)
 *   Alt+Shift+<letter> -> the capital command  (Alt+Shift+s = stealth)
 * Only fires when that letter has a plain movement binding, so
 * Classic/Arrows presets are unaffected and the Alt layout shortcuts
 * (Alt+a/i/l) keep working for unshadowed letters.
 */
bool sdl_try_send_shadowed_command_event(const SDL_KeyboardEvent* key_event)
{
    SDL_Keycode base;
    char command;

    if (!key_event || !character_dungeon || character_icky)
        return false;

    if (!(key_event->mod & SDL_KMOD_ALT)
        || (key_event->mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)))
    {
        return false;
    }

    if (!sdl_config_scancode_is_plain_movement_letter(&config,
            (u32b)key_event->scancode))
    {
        return false;
    }

    base = SDL_GetKeyFromScancode(key_event->scancode, SDL_KMOD_NONE, false);
    if (base < 'a' || base > 'z')
        return false;

    /* Shift selects the capital command for that key. */
    command = (key_event->mod & SDL_KMOD_SHIFT)
        ? (char)SDL_toupper(base)
        : (char)base;

    /* Issue the underlying letter command (movement only steals the bare key,
     * so feeding the letter through the Term queue runs its real command). */
    Term_keypress(command);
    return true;
}

/*
 * WASD-grid-only extra command keys. The grid shadows several command letters,
 * so the free letters n/v/k are offered as command keys for that preset only:
 *   n = sing, v = examine, k = activate staff, Shift+N = toggle stealth.
 * (Other presets leave n/v/k alone; in particular Vi uses n/k for movement and
 * keeps the normal Shift+S for stealth.)
 */
bool sdl_try_send_preset_command_alias(const SDL_KeyboardEvent* key_event)
{
    char command;

    if (!key_event || !character_dungeon || character_icky)
        return false;
    if (key_event->mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))
        return false;
    if (config.movement_keyboard_preset != SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC)
        return false;

    if (key_event->mod & SDL_KMOD_SHIFT)
    {
        /* Capital alias for the command shadowed by the WASD-grid bindings. */
        if (key_event->scancode != SDL_SCANCODE_N)
            return false;
        command = 'S'; /* toggle stealth */
    }
    else
    {
        switch (key_event->scancode)
        {
        case SDL_SCANCODE_N:
            command = 's'; /* sing */
            break;
        case SDL_SCANCODE_V:
            command = 'x'; /* examine */
            break;
        case SDL_SCANCODE_K:
            command = 'a'; /* activate staff */
            break;
        default:
            return false;
        }
    }

    Term_keypress(command);
    return true;
}

bool sdl_send_modified_direction_action(int dir, char dir_ch, bool shift, bool ctrl, bool alt,
    bool gui)
{
    bool control = ctrl || gui;
    int mod_count = (shift ? 1 : 0) + (control ? 1 : 0) + (alt ? 1 : 0);
    char action_key;
    char follow_key;

    if (dir < 1 || dir > 9 || mod_count != 1)
        return false;

    if (alt) {
        action_key = 'f';
        follow_key = (dir == 5) ? 'f' : dir_ch;
    } else if (control) {
        if (sdl_submit_legacy_keypad_movement(
                MOVEMENT_INPUT_ACTION_INTERACT_DIR, dir))
        {
            return true;
        }
        action_key = '/';
        follow_key = (dir == 5) ? '5' : dir_ch;
    } else {
        if (sdl_submit_legacy_keypad_movement(
                (dir == 5) ? MOVEMENT_INPUT_ACTION_REST
                           : MOVEMENT_INPUT_ACTION_RUN_DIR,
                dir))
        {
            return true;
        }
        action_key = '.';
        follow_key = (dir == 5) ? '5' : dir_ch;
    }

    if (!follow_key)
        follow_key = (char)('0' + dir);

    /* Bypass keymaps for the action key itself, but keep the bound direction key. */
    Term_keypress('\\');
    Term_keypress(action_key);
    Term_keypress(follow_key);
    return true;
}

bool sdl_try_send_modified_direction_key(int key, bool shift, bool ctrl, bool alt, bool gui)
{
    char dir_ch = sdl_direction_char_for_key(key);
    int dir = sdl_direction_for_key_char(dir_ch);

    if (!dir)
        return false;

    return sdl_send_modified_direction_action(dir, dir_ch, shift, ctrl, alt, gui);
}

bool sdl_try_send_modified_direction_event(const SDL_KeyboardEvent* key_event)
{
    bool shift;
    bool alt;
    bool ctrl;
    bool gui;
    SDL_Keycode base_key;
    int shifted_ascii;

    if (!key_event)
        return false;

    shift = key_event->mod & SDL_KMOD_SHIFT;
    alt = key_event->mod & SDL_KMOD_ALT;
    ctrl = key_event->mod & SDL_KMOD_CTRL;
    gui = key_event->mod & SDL_KMOD_GUI;

    base_key = SDL_GetKeyFromScancode(key_event->scancode, SDL_KMOD_NONE, false);

    /* Shifted punctuation is normally a distinct command (<, >, *, ?).
     * Do not reinterpret it as Shift plus the unshifted key's keymap action. */
    shifted_ascii = sdl_shifted_ascii_for_key(key_event->key);
    if (!shifted_ascii && base_key != key_event->key)
        shifted_ascii = sdl_shifted_ascii_for_key(base_key);
    if (shift && !ctrl && !alt && !gui && shifted_ascii)
        return false;

    if (sdl_try_send_modified_direction_key(key_event->key, shift, ctrl, alt, gui))
        return true;

    if (base_key != key_event->key
        && sdl_try_send_modified_direction_key(base_key, shift, ctrl, alt, gui))
    {
        return true;
    }

    return false;
}

bool sdl_handle_jewelry_preset_shortcut(
    const SDL_KeyboardEvent* key_event)
{
    SDL_Keycode key;

    if (!key_event || !character_dungeon)
        return false;

    if (!(key_event->mod & SDL_KMOD_ALT)
        || (key_event->mod & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_GUI)))
    {
        return false;
    }

    key = SDL_GetKeyFromScancode(key_event->scancode, SDL_KMOD_NONE, false);
    if (key < '1' || key > ('0' + JEWELRY_PRESET_MAX))
        return false;

    Term_keypress('\\');
    Term_keypress('J');
    Term_keypress(key);
    return true;
}

bool sdl_handle_global_layout_shortcut(const SDL_KeyboardEvent* key_event)
{
    SDL_Keycode key;

    if (!key_event)
        return false;

    if (!(key_event->mod & SDL_KMOD_ALT))
        return false;

    key = key_event->key;

    if (key == '+' || key == '=' || key == SDLK_KP_PLUS) {
        (void)sdl_main_screen_adjust_main_view_scale(1);
        return true;
    }

    if (key == '-' || key == SDLK_KP_MINUS) {
        (void)sdl_main_screen_adjust_main_view_scale(-1);
        return true;
    }

    if (key == 'i' || key == 'I') {
        bool enabled = get_sdl_enable_right_panes();

        if (key_event->repeat)
            return true;

        set_sdl_enable_right_panes(!enabled);
        sdl_apply_config();
        if (character_dungeon)
            Term_keypress(KTRL('R'));
        return true;
    }

    if (key == 'l' || key == 'L') {
        bool enabled = get_sdl_enable_bottom_panes();

        if (key_event->repeat)
            return true;

        set_sdl_enable_bottom_panes(!enabled);
        sdl_apply_config();
        if (character_dungeon)
            Term_keypress(KTRL('R'));
        return true;
    }

    if ((key == 'a' || key == 'A')
        && !(key_event->mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)))
    {
        bool old_tiles = get_sdl_tiles();

        if (key_event->repeat)
            return true;

        set_sdl_tiles(!old_tiles);
        return true;
    }

    return false;
}

void sdl_gamepad_send_key(int key, bool use_macro_mods)
{
    bool shift = sdl_gamepad_shift_active();
    bool ctrl = sdl_gamepad_ctrl_active();
    bool alt = sdl_gamepad_alt_active();

    if (use_macro_mods && (shift || ctrl || alt)) {
        sdl_send_macro_key(key, shift, ctrl, alt);
        return;
    }

    if (SDL_isprint(key)) {
        if (ctrl && !alt && SDL_isalpha(key)) {
            Term_keypress(KTRL(key));
            return;
        }

        if (ctrl || alt) {
            sdl_send_macro_key(key, shift, ctrl, alt);
            return;
        }

        if (shift) {
            if (SDL_isalpha(key)) {
                key = SDL_toupper(key);
            } else {
                const char shifted[256] = {
                    ['1'] = '!', ['2'] = '@', ['3'] = '#', ['4'] = '$', ['5'] = '%',
                    ['6'] = '^', ['7'] = '&', ['8'] = '*', ['9'] = '(', ['0'] = ')',
                    ['-'] = '_', ['='] = '+',
                    [','] = '<', ['.'] = '>', ['/'] = '?',
                    ['['] = '{', [']'] = '}',
                    [';'] = ':', ['\''] = '"', ['\\'] = '|',
                    ['`'] = '~',
                };
                if (shifted[key])
                    key = shifted[key];
            }
        }

        Term_keypress(key);
        return;
    }

    if (shift || ctrl || alt) {
        sdl_send_macro_key(key, shift, ctrl, alt);
    } else {
        Term_keypress(key);
    }
}

void sdl_gamepad_send_key_raw(int key)
{
    Term_keypress(key);
}

void sdl_gamepad_send_shoulder_combo(void)
{
    int binding = config.gamepad_shoulder_combo_binding;
    if (binding == GAMEPAD_BIND_NONE)
        return;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, true);
        sdl_gamepad_apply_modifier(binding, false);
        return;
    }

    sdl_gamepad_send_key(binding, false);
}

void sdl_gamepad_send_direction_mods(int dir, bool shift, bool ctrl, bool alt)
{
    if (dir < 1 || dir > 9)
        return;

    if (!shift && !ctrl && !alt)
    {
        if (sdl_submit_legacy_keypad_movement(
                (dir == 5) ? MOVEMENT_INPUT_ACTION_WAIT
                           : MOVEMENT_INPUT_ACTION_MOVE_DIR,
                dir))
        {
            return;
        }
    }

    if (shift && !ctrl && !alt)
    {
        if (sdl_submit_legacy_keypad_movement(
                (dir == 5) ? MOVEMENT_INPUT_ACTION_REST
                           : MOVEMENT_INPUT_ACTION_RUN_DIR,
                dir))
        {
            return;
        }
    }

    if (ctrl && !shift && !alt)
    {
        if (sdl_submit_legacy_keypad_movement(
                MOVEMENT_INPUT_ACTION_INTERACT_DIR, dir))
        {
            return;
        }
    }

    if (sdl_send_modified_direction_action(dir, (char)('0' + dir), shift, ctrl, alt, false))
        return;

    if (shift || ctrl || alt) {
        sdl_send_macro_key('0' + dir, shift, ctrl, alt);
    } else {
        Term_keypress('0' + dir);
    }
}

int sdl_gamepad_axis_to_dir(Sint16 x, Sint16 y, int deadzone)
{
    int dx = 0;
    int dy = 0;

    if (x > deadzone)
        dx = 1;
    else if (x < -deadzone)
        dx = -1;

    if (y > deadzone)
        dy = 1;
    else if (y < -deadzone)
        dy = -1;

    if (dx == 0 && dy == 0)
        return 0;

    if (dy < 0) {
        if (dx < 0) return 7;
        if (dx > 0) return 9;
        return 8;
    }
    if (dy > 0) {
        if (dx < 0) return 1;
        if (dx > 0) return 3;
        return 2;
    }
    if (dx < 0) return 4;
    if (dx > 0) return 6;
    return 0;
}

int sdl_gamepad_axis_to_cardinal_dir(Sint16 x, Sint16 y, int deadzone)
{
    int abs_x = abs(x);
    int abs_y = abs(y);

    if (abs_x < deadzone && abs_y < deadzone)
        return -1;

    if (abs_x >= abs_y) {
        return (x >= 0) ? GAMEPAD_STICK_DIR_RIGHT : GAMEPAD_STICK_DIR_LEFT;
    }

    return (y >= 0) ? GAMEPAD_STICK_DIR_DOWN : GAMEPAD_STICK_DIR_UP;
}

void sdl_gamepad_send_direction(int dir)
{
    sdl_gamepad_send_direction_mods(dir, sdl_gamepad_shift_active(),
        sdl_gamepad_ctrl_active(), sdl_gamepad_alt_active());
}

void sdl_gamepad_clear_pending_dpad(void)
{
    g_gamepad_state.dpad_pending = false;
    g_gamepad_state.dpad_pending_dir = 0;
    g_gamepad_state.dpad_pending_time = 0;
    g_gamepad_state.dpad_pending_shift = false;
    g_gamepad_state.dpad_pending_ctrl = false;
    g_gamepad_state.dpad_pending_alt = false;
}

void sdl_gamepad_set_pending_dpad(int dir)
{
    g_gamepad_state.dpad_pending = true;
    g_gamepad_state.dpad_pending_dir = dir;
    g_gamepad_state.dpad_pending_time = SDL_GetTicksNS();
    g_gamepad_state.dpad_pending_shift = sdl_gamepad_shift_active();
    g_gamepad_state.dpad_pending_ctrl = sdl_gamepad_ctrl_active();
    g_gamepad_state.dpad_pending_alt = sdl_gamepad_alt_active();
}

bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force)
{
    if (!g_gamepad_state.dpad_pending)
        return false;
    if (!config.gamepad_enabled || !config.gamepad_use_dpad) {
        sdl_gamepad_clear_pending_dpad();
        return false;
    }

    Uint64 window_ns = (Uint64)DPAD_DIAGONAL_WINDOW_MS * 1000000ULL;
    if (!force && now_ns - g_gamepad_state.dpad_pending_time < window_ns)
        return false;

    sdl_gamepad_send_direction_mods(g_gamepad_state.dpad_pending_dir,
        g_gamepad_state.dpad_pending_shift, g_gamepad_state.dpad_pending_ctrl,
        g_gamepad_state.dpad_pending_alt);
    sdl_gamepad_clear_pending_dpad();
    return true;
}

void sdl_gamepad_clear_pending_left_stick(void)
{
    g_gamepad_state.left_pending = false;
    g_gamepad_state.left_pending_dir = 0;
    g_gamepad_state.left_pending_time = 0;
    g_gamepad_state.left_pending_shift = false;
    g_gamepad_state.left_pending_ctrl = false;
    g_gamepad_state.left_pending_alt = false;
}

void sdl_gamepad_set_pending_left_stick(int dir)
{
    g_gamepad_state.left_pending = true;
    g_gamepad_state.left_pending_dir = dir;
    g_gamepad_state.left_pending_time = SDL_GetTicksNS();
    g_gamepad_state.left_pending_shift = sdl_gamepad_shift_active();
    g_gamepad_state.left_pending_ctrl = sdl_gamepad_ctrl_active();
    g_gamepad_state.left_pending_alt = sdl_gamepad_alt_active();
}

bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force)
{
    if (!g_gamepad_state.left_pending)
        return false;
    if (!config.gamepad_enabled || !config.gamepad_use_left_stick) {
        sdl_gamepad_clear_pending_left_stick();
        return false;
    }

    Uint64 window_ns = (Uint64)DPAD_DIAGONAL_WINDOW_MS * 1000000ULL;
    if (!force && now_ns - g_gamepad_state.left_pending_time < window_ns)
        return false;

    sdl_gamepad_send_direction_mods(g_gamepad_state.left_pending_dir,
        g_gamepad_state.left_pending_shift, g_gamepad_state.left_pending_ctrl,
        g_gamepad_state.left_pending_alt);
    sdl_gamepad_clear_pending_left_stick();
    return true;
}

void sdl_gamepad_clear_pending_confirm(void)
{
    g_gamepad_state.confirm_pending = false;
    g_gamepad_state.confirm_pending_button = 0;
    g_gamepad_state.confirm_pending_binding = GAMEPAD_BIND_NONE;
    g_gamepad_state.confirm_pending_time = 0;
    g_gamepad_state.confirm_long_triggered = false;
}

bool sdl_gamepad_confirm_long_press_available(int binding)
{
    if (!config.gamepad_enabled)
        return false;
    if (!sdl_gamepad_action_is_confirm(binding))
        return false;
    if (g_player_action_menu.active || g_player_exchange_target.active)
        return false;
    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (sdl_gamepad_single_active_modifier() != GAMEPAD_BIND_NONE)
        return false;

    return true;
}

bool sdl_touch_top_panel_compute_layout(SDL_FRect* button_rects,
    SDL_FRect* out_panel)
{
    SDL_Rect screen;
    SDL_Rect anchor;
    enum pane_placement where;

    if (!sdl_touch_top_panel_layout_visible())
        return false;
    if (config.touch_top_panel_arrows_visible && !g_touch_top_panel_open)
        return false;

    if (!sdl_touch_top_panel_current_anchor(&screen, &anchor, &where))
        return false;
    return sdl_touch_top_panel_compute_layout_for_anchor(&screen, &anchor,
        where, button_rects, out_panel);
}

/* Geometry-only layout for the tutorial: the quick access panel is taught even
 * when it is collapsed (g_touch_top_panel_open == false) or gameplay shortcuts
 * are inactive, so the runtime open/visible gates are skipped on purpose. */
bool sdl_touch_top_panel_compute_layout_for_display(SDL_FRect* button_rects,
    SDL_FRect* out_panel)
{
    SDL_Rect screen;
    SDL_Rect anchor;
    enum pane_placement where;

    if (!sdl_touch_top_panel_current_anchor(&screen, &anchor, &where))
        return false;
    return sdl_touch_top_panel_compute_layout_for_anchor(&screen, &anchor,
        where, button_rects, out_panel);
}

bool sdl_gamepad_handle_confirm_long_press_button(
    int button, int binding, bool down)
{
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return false;

    if (g_gamepad_state.confirm_pending
        && g_gamepad_state.confirm_pending_button == button)
    {
        if (down)
            return true;

        if (!g_gamepad_state.confirm_long_triggered)
            sdl_gamepad_send_key_raw(
                g_gamepad_state.confirm_pending_binding);

        sdl_gamepad_clear_pending_confirm();
        return true;
    }

    if (!down)
        return false;

    if (!sdl_gamepad_confirm_long_press_available(binding))
        return false;

    g_gamepad_state.confirm_pending = true;
    g_gamepad_state.confirm_pending_button = button;
    g_gamepad_state.confirm_pending_binding = binding;
    g_gamepad_state.confirm_pending_time = SDL_GetTicksNS();
    g_gamepad_state.confirm_long_triggered = false;
    return true;
}

int sdl_gamepad_pending_confirm_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_gamepad_state.confirm_pending)
        return -1;
    if (g_gamepad_state.confirm_long_triggered)
        return -1;
    if (!config.gamepad_enabled) {
        sdl_gamepad_clear_pending_confirm();
        return -1;
    }

    elapsed = now_ns - g_gamepad_state.confirm_pending_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_gamepad_flush_pending_confirm(Uint64 now_ns)
{
    if (!g_gamepad_state.confirm_pending)
        return false;
    if (g_gamepad_state.confirm_long_triggered)
        return false;
    if (!sdl_gamepad_confirm_long_press_available(
            g_gamepad_state.confirm_pending_binding))
    {
        return false;
    }
    if (now_ns - g_gamepad_state.confirm_pending_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    if (!sdl_player_action_menu_open())
        return false;

    g_gamepad_state.confirm_long_triggered = true;
    sdl_player_action_menu_select_default();
    return true;
}

void sdl_gamepad_clear_pending_shoulder(void)
{
    g_gamepad_state.shoulder_pending = false;
    g_gamepad_state.shoulder_pending_button = 0;
    g_gamepad_state.shoulder_pending_time = 0;
}

void sdl_gamepad_set_pending_shoulder(int button)
{
    g_gamepad_state.shoulder_pending = true;
    g_gamepad_state.shoulder_pending_button = button;
    g_gamepad_state.shoulder_pending_time = SDL_GetTicksNS();
}

bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force)
{
    if (!g_gamepad_state.shoulder_pending)
        return false;
    if (!config.gamepad_enabled) {
        sdl_gamepad_clear_pending_shoulder();
        return false;
    }

    Uint64 window_ns = (Uint64)SHOULDER_COMBO_WINDOW_MS * 1000000ULL;
    if (!force && now_ns - g_gamepad_state.shoulder_pending_time < window_ns)
        return false;

    int button = g_gamepad_state.shoulder_pending_button;
    sdl_gamepad_clear_pending_shoulder();

    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return true;

    int binding = config.gamepad_button_bindings[button];
    if (binding == GAMEPAD_BIND_NONE)
        return true;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, true);
    } else {
        sdl_gamepad_send_key(binding, false);
    }

    return true;
}

bool sdl_gamepad_resolve_pending_shoulder_with_modifier(int binding)
{
    int button;
    int combo_binding;

    if (!g_gamepad_state.shoulder_pending)
        return false;
    if (!config.gamepad_enabled || !steamdeck_controls_active())
        return false;
    if (g_gamepad_capture_active)
        return false;
    if (binding != GAMEPAD_BIND_SHIFT && binding != GAMEPAD_BIND_CTRL
        && binding != GAMEPAD_BIND_ALT)
        return false;

    button = g_gamepad_state.shoulder_pending_button;
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return false;

    combo_binding = sdl_gamepad_combo_binding_for_input(binding,
        GAMEPAD_CAPTURE_BUTTON, button);
    if (combo_binding == GAMEPAD_BIND_NONE)
        return false;

    sdl_gamepad_clear_pending_shoulder();
    sdl_gamepad_send_key_raw(combo_binding);
    return true;
}

int sdl_gamepad_pending_timeout_ms(Uint64 now_ns)
{
    int dpad_timeout = -1;
    int left_timeout = -1;
    int shoulder_timeout = -1;
    int confirm_timeout = sdl_gamepad_pending_confirm_timeout_ms(now_ns);
    int best = -1;

    if (g_gamepad_state.dpad_pending && config.gamepad_enabled && config.gamepad_use_dpad) {
        Uint64 window_ns = (Uint64)DPAD_DIAGONAL_WINDOW_MS * 1000000ULL;
        Uint64 elapsed = now_ns - g_gamepad_state.dpad_pending_time;
        if (elapsed >= window_ns) {
            dpad_timeout = 0;
        } else {
            Uint64 remaining_ns = window_ns - elapsed;
            dpad_timeout = (int)(remaining_ns / 1000000ULL);
            if (dpad_timeout < 1)
                dpad_timeout = 1;
        }
    }

    if (g_gamepad_state.left_pending && config.gamepad_enabled && config.gamepad_use_left_stick) {
        Uint64 window_ns = (Uint64)DPAD_DIAGONAL_WINDOW_MS * 1000000ULL;
        Uint64 elapsed = now_ns - g_gamepad_state.left_pending_time;
        if (elapsed >= window_ns) {
            left_timeout = 0;
        } else {
            Uint64 remaining_ns = window_ns - elapsed;
            left_timeout = (int)(remaining_ns / 1000000ULL);
            if (left_timeout < 1)
                left_timeout = 1;
        }
    }

    if (g_gamepad_state.shoulder_pending && config.gamepad_enabled && steamdeck_controls_active()) {
        Uint64 window_ns = (Uint64)SHOULDER_COMBO_WINDOW_MS * 1000000ULL;
        Uint64 elapsed = now_ns - g_gamepad_state.shoulder_pending_time;
        if (elapsed >= window_ns) {
            shoulder_timeout = 0;
        } else {
            Uint64 remaining_ns = window_ns - elapsed;
            shoulder_timeout = (int)(remaining_ns / 1000000ULL);
            if (shoulder_timeout < 1)
                shoulder_timeout = 1;
        }
    }

    if (dpad_timeout >= 0)
        best = dpad_timeout;
    if (left_timeout >= 0 && (best < 0 || left_timeout < best))
        best = left_timeout;
    if (shoulder_timeout >= 0 && (best < 0 || shoulder_timeout < best))
        best = shoulder_timeout;
    if (confirm_timeout >= 0 && (best < 0 || confirm_timeout < best))
        best = confirm_timeout;

    return best;
}

const char* sdl_gamepad_button_label(int button)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return "A (South)";
    case SDL_GAMEPAD_BUTTON_EAST: return "B (East)";
    case SDL_GAMEPAD_BUTTON_WEST: return "X (West)";
    case SDL_GAMEPAD_BUTTON_NORTH: return "Y (North)";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1 (Left Shoulder)";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1 (Right Shoulder)";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4 (Left Paddle 1)";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5 (Left Paddle 2)";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4 (Right Paddle 1)";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5 (Right Paddle 2)";
    case SDL_GAMEPAD_BUTTON_START: return "Start (Menu)";
    case SDL_GAMEPAD_BUTTON_BACK: return "Back (View)";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "Left Stick Click";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "Right Stick Click";
    case SDL_GAMEPAD_BUTTON_GUIDE: return "Guide (Steam)";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad Click";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
    case SDL_GAMEPAD_BUTTON_MISC1: return "Misc1";
    case SDL_GAMEPAD_BUTTON_MISC2: return "Misc2";
    case SDL_GAMEPAD_BUTTON_MISC3: return "Misc3";
    case SDL_GAMEPAD_BUTTON_MISC4: return "Misc4";
    case SDL_GAMEPAD_BUTTON_MISC5: return "Misc5";
    case SDL_GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "Unknown Button";
    }
}

const char* sdl_gamepad_button_short_label(int button)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return "A";
    case SDL_GAMEPAD_BUTTON_EAST: return "B";
    case SDL_GAMEPAD_BUTTON_WEST: return "X";
    case SDL_GAMEPAD_BUTTON_NORTH: return "Y";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5";
    case SDL_GAMEPAD_BUTTON_START: return "Start";
    case SDL_GAMEPAD_BUTTON_BACK: return "Back";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "L3";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "R3";
    case SDL_GAMEPAD_BUTTON_GUIDE: return "Guide";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-Up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-Down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-Left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-Right";
    case SDL_GAMEPAD_BUTTON_MISC1: return "Misc1";
    case SDL_GAMEPAD_BUTTON_MISC2: return "Misc2";
    case SDL_GAMEPAD_BUTTON_MISC3: return "Misc3";
    case SDL_GAMEPAD_BUTTON_MISC4: return "Misc4";
    case SDL_GAMEPAD_BUTTON_MISC5: return "Misc5";
    case SDL_GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "?";
    }
}

const char* sdl_gamepad_trigger_label(int index)
{
    if (index == 0)
        return "L2 (Left Trigger)";
    if (index == 1)
        return "R2 (Right Trigger)";
    return "Unknown Trigger";
}

const char* sdl_gamepad_trigger_short_label(int index)
{
    if (index == 0)
        return "L2";
    if (index == 1)
        return "R2";
    return "?";
}

const char* sdl_gamepad_stick_dir_label(int type, int dir, bool short_label)
{
    const char* stick = (type == GAMEPAD_CAPTURE_RIGHT_STICK) ? "Right Stick" : "Left Stick";
    const char* stick_short = (type == GAMEPAD_CAPTURE_RIGHT_STICK) ? "RS" : "LS";
    const char* dir_label = "";
    const char* dir_short = "";

    switch (dir) {
    case GAMEPAD_STICK_DIR_UP: dir_label = "Up"; dir_short = "Up"; break;
    case GAMEPAD_STICK_DIR_DOWN: dir_label = "Down"; dir_short = "Down"; break;
    case GAMEPAD_STICK_DIR_LEFT: dir_label = "Left"; dir_short = "Left"; break;
    case GAMEPAD_STICK_DIR_RIGHT: dir_label = "Right"; dir_short = "Right"; break;
    default: return short_label ? "?" : "Unknown Stick";
    }

    if (short_label)
        return format("%s %s", stick_short, dir_short);
    return format("%s %s", stick, dir_label);
}

void sdl_gamepad_binding_label_ex(int type, int id, char* buf, size_t buflen, bool short_label)
{
    if (!buf || !buflen)
        return;

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        SDL_strlcpy(buf, short_label ? sdl_gamepad_button_short_label(id)
                                     : sdl_gamepad_button_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        SDL_strlcpy(buf, short_label ? sdl_gamepad_trigger_short_label(id)
                                     : sdl_gamepad_trigger_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK || type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        SDL_strlcpy(buf, sdl_gamepad_stick_dir_label(type, id, short_label), buflen);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        SDL_strlcpy(buf, short_label ? "L1+R1" : "L1+R1 Combo", buflen);
    } else {
        SDL_strlcpy(buf, "(unknown)", buflen);
    }
}

bool sdl_gamepad_action_is_confirm(int binding)
{
    return (binding == INPUT_BIND_CONFIRM || binding == ' ' || binding == '\r');
}

bool sdl_gamepad_action_binding_equals(int lhs, int rhs)
{
    if (sdl_gamepad_action_is_confirm(lhs) && sdl_gamepad_action_is_confirm(rhs))
        return true;

    return lhs == rhs;
}

int sdl_gamepad_direct_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_button_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_trigger_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_left_stick_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_right_stick_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    if (sdl_gamepad_action_binding_equals(config.gamepad_shoulder_combo_binding, binding)) {
        if (count == 0 && out_type && out_id) {
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
            *out_id = 0;
        }
        count++;
    }

    return count;
}

int sdl_gamepad_physical_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_button_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_trigger_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (config.gamepad_left_stick_bindings[i] == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
                *out_id = i;
            }
            count++;
        }
        if (config.gamepad_right_stick_bindings[i] == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    return count;
}

int sdl_gamepad_combo_action_binding_count(int binding, int* out_modifier_type,
    int* out_modifier_id, int* out_type, int* out_id)
{
    static const int modifiers[] = {
        GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL,
        GAMEPAD_BIND_ALT,
    };
    static const int types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
        GAMEPAD_CAPTURE_LEFT_STICK,
        GAMEPAD_CAPTURE_RIGHT_STICK,
    };
    int total = 0;

    for (int mi = 0; mi < (int)N_ELEMENTS(modifiers); mi++) {
        int mod_type = 0;
        int mod_id = 0;
        int mod_count = sdl_gamepad_physical_binding_count(modifiers[mi], &mod_type,
            &mod_id);

        if (mod_count <= 0)
            continue;

        for (int ti = 0; ti < (int)N_ELEMENTS(types); ti++) {
            int count = 0;

            if (types[ti] == GAMEPAD_CAPTURE_BUTTON)
                count = SDL_GAMEPAD_BUTTON_COUNT;
            else if (types[ti] == GAMEPAD_CAPTURE_TRIGGER)
                count = GAMEPAD_TRIGGER_COUNT;
            else
                count = GAMEPAD_STICK_DIR_COUNT;

            for (int id = 0; id < count; id++) {
                if (!sdl_gamepad_action_binding_equals(
                        sdl_gamepad_combo_binding_for_input(modifiers[mi], types[ti], id),
                        binding))
                    continue;

                if (total == 0) {
                    if (out_modifier_type)
                        *out_modifier_type = mod_type;
                    if (out_modifier_id)
                        *out_modifier_id = mod_id;
                    if (out_type)
                        *out_type = types[ti];
                    if (out_id)
                        *out_id = id;
                }

                total += mod_count;
            }
        }
    }

    return total;
}

void sdl_gamepad_action_binding_label_ex(int binding, char* buf, size_t buflen, bool short_label)
{
    if (!buf || !buflen)
        return;

    int type = 0;
    int id = 0;
    int mod_type = 0;
    int mod_id = 0;
    int direct_count = sdl_gamepad_direct_binding_count(binding, &type, &id);
    int combo_count = sdl_gamepad_combo_action_binding_count(binding, &mod_type,
        &mod_id, &type, &id);
    int count = direct_count + combo_count;

    if (count <= 0) {
        SDL_strlcpy(buf, "(unbound)", buflen);
    } else if (count == 1 && direct_count == 1) {
        sdl_gamepad_binding_label_ex(type, id, buf, buflen, short_label);
    } else if (count == 1) {
        char mod_buf[32];
        char base_buf[32];
        sdl_gamepad_binding_label_ex(mod_type, mod_id, mod_buf, sizeof(mod_buf),
            short_label);
        sdl_gamepad_binding_label_ex(type, id, base_buf, sizeof(base_buf),
            short_label);
        strnfmt(buf, buflen, "%s+%s", mod_buf, base_buf);
    } else {
        SDL_strlcpy(buf, "Multiple", buflen);
    }
}

void sdl_gamepad_action_binding_label(int binding, char* buf, size_t buflen)
{
    sdl_gamepad_action_binding_label_ex(binding, buf, buflen, false);
}

void sdl_gamepad_action_binding_short_label(int binding, char* buf, size_t buflen)
{
    sdl_gamepad_action_binding_label_ex(binding, buf, buflen, true);
}

int sdl_gamepad_capture_binding_for_input(int type, int id)
{
    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id >= 0 && id < SDL_GAMEPAD_BUTTON_COUNT)
            return config.gamepad_button_bindings[id];
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        if (id >= 0 && id < GAMEPAD_TRIGGER_COUNT)
            return config.gamepad_trigger_bindings[id];
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_left_stick_bindings[id];
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_right_stick_bindings[id];
        break;
    default:
        break;
    }

    return GAMEPAD_BIND_NONE;
}

bool sdl_gamepad_capture_queue_input(int type, int id)
{
    int binding = sdl_gamepad_capture_binding_for_input(type, id);

    if (g_gamepad_capture_allow_modifier_combo
        && (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL
            || binding == GAMEPAD_BIND_ALT)) {
        if (g_gamepad_capture_modifier != GAMEPAD_BIND_NONE)
            return false;
        g_gamepad_capture_modifier = binding;
        return false;
    }

    g_gamepad_capture_type = type;
    g_gamepad_capture_id = id;
    g_gamepad_capture_ready = true;
    g_gamepad_capture_active = false;
    return true;
}

/* Controller UI menu helpers - return key bindings for menu actions */
int steamdeck_back_key(void)
{
    /* B button (EAST) - for back/quit in menus */
    return get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_EAST);
}

int steamdeck_confirm_key(void)
{
    /* A button (SOUTH) - for confirm/ok in menus */
    return get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_SOUTH);
}

int steamdeck_prev_page_key(void)
{
    /* L1 button (LEFT_SHOULDER) - for previous page/tab in menus */
    return get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
}

int steamdeck_next_page_key(void)
{
    /* R1 button (RIGHT_SHOULDER) - for next page/tab in menus */
    return get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
}

int steamdeck_menu_key(int key, int prev_page_key, int next_page_key)
{
    if (!steamdeck_controls_active())
        return key;

    if (key == steamdeck_back_key())
        return ESCAPE;
    if (key == steamdeck_confirm_key())
        return '\r';
    if (prev_page_key && key == steamdeck_prev_page_key())
        return prev_page_key;
    if (next_page_key && key == steamdeck_next_page_key())
        return next_page_key;

    return key;
}

int steamdeck_info_key(void)
{
    /* RS Right - for info/recall in menus */
    return get_sdl_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_RIGHT);
}

int steamdeck_alt_action_key(void)
{
    /* X button (WEST) - for alternate action in menus */
    return get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_WEST);
}

int steamdeck_secondary_key(void)
{
    /* Y button (NORTH) - for secondary action in menus */
    return get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_NORTH);
}

void sdl_gamepad_handle_button(const SDL_GamepadButtonEvent* ev)
{
    if (!ev)
        return;

    SDL_GamepadButton button = (SDL_GamepadButton)ev->button;
    bool down = ev->down;

    if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
        g_gamepad_state.left_shoulder_down = down;
    } else if (button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
        g_gamepad_state.right_shoulder_down = down;
    }

    if (g_gamepad_capture_active) {
        bool capture_armed = (SDL_GetTicksNS() >= g_gamepad_capture_arm_time);
        bool shoulder_button = (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
            || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        if (shoulder_button) {
            if (!capture_armed)
                return;

            if (g_gamepad_capture_allow_modifier_combo && down) {
                int binding = sdl_gamepad_capture_binding_for_input(
                    GAMEPAD_CAPTURE_BUTTON, (int)button);
                if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL
                    || binding == GAMEPAD_BIND_ALT) {
                    (void)sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON,
                        (int)button);
                    return;
                }
            }

            if (g_gamepad_capture_modifier != GAMEPAD_BIND_NONE) {
                if (down)
                    (void)sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON,
                        (int)button);
                return;
            }

            if (down) {
                if (g_gamepad_state.shoulder_pending &&
                    g_gamepad_state.shoulder_pending_button != (int)button) {
                    sdl_gamepad_clear_pending_shoulder();
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
                    g_gamepad_capture_id = 0;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                } else {
                    g_gamepad_state.shoulder_pending = true;
                    g_gamepad_state.shoulder_pending_button = (int)button;
                    g_gamepad_state.shoulder_pending_time = SDL_GetTicksNS();
                }
            } else if (g_gamepad_state.shoulder_pending &&
                       g_gamepad_state.shoulder_pending_button == (int)button) {
                sdl_gamepad_clear_pending_shoulder();
                (void)sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON,
                    (int)button);
            }
            return;
        }

        if (!capture_armed)
            return;

        if (down) {
            bool dpad_button = (button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN
                || button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            if (!dpad_button || !config.gamepad_use_dpad) {
                if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
                    (void)sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON,
                        (int)button);
                }
            }
        }
        return;
    }

    if (!config.gamepad_enabled)
        return;

    sdl_gamepad_mark_auto_ui();

    if (sdl_minimap_handle_gamepad_button(button, down))
        return;

    if (sdl_welcome_screen_handle_gamepad_button(button, down))
        return;

    if (!down && g_gamepad_state.confirm_pending
        && g_gamepad_state.confirm_pending_button == (int)button
        && sdl_gamepad_handle_confirm_long_press_button(
            (int)button, GAMEPAD_BIND_NONE, down))
    {
        return;
    }

    if (sdl_player_exchange_handle_gamepad_button(button, down))
        return;

    if (sdl_player_action_menu_handle_gamepad_button(button, down))
        return;

    if (config.gamepad_use_dpad &&
        (button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN ||
            button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
    {
        switch (button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP: g_gamepad_state.dpad_up = down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN: g_gamepad_state.dpad_down = down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT: g_gamepad_state.dpad_left = down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: g_gamepad_state.dpad_right = down; break;
            default: break;
        }

        int dx = 0;
        int dy = 0;
        if (g_gamepad_state.dpad_left) dx--;
        if (g_gamepad_state.dpad_right) dx++;
        if (g_gamepad_state.dpad_up) dy--;
        if (g_gamepad_state.dpad_down) dy++;

        int dir = 0;
        bool diagonal = false;
        if (dx || dy) {
            if (dy < 0) {
                dir = (dx < 0) ? 7 : (dx > 0) ? 9 : 8;
            } else if (dy > 0) {
                dir = (dx < 0) ? 1 : (dx > 0) ? 3 : 2;
            } else {
                dir = (dx < 0) ? 4 : (dx > 0) ? 6 : 0;
            }
            diagonal = (dx != 0 && dy != 0);
        }

        if (dir != g_gamepad_state.dpad_dir) {
            g_gamepad_state.dpad_dir = dir;

            if (!down)
                return;

            if (dir == 0) {
                /* Keep pending to allow quick taps to resolve. */
            } else if (diagonal) {
                sdl_gamepad_clear_pending_dpad();
                sdl_gamepad_send_direction(dir);
            } else {
                if (g_gamepad_state.dpad_pending)
                    sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true);
                sdl_gamepad_set_pending_dpad(dir);
            }
        }
        return;
    }

    if (steamdeck_controls_active() &&
        (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
    {
        int active_modifier = sdl_gamepad_single_active_modifier();
        int combo_binding = GAMEPAD_BIND_NONE;

        if (down && active_modifier != GAMEPAD_BIND_NONE) {
            combo_binding = sdl_gamepad_combo_binding_for_input(active_modifier,
                GAMEPAD_CAPTURE_BUTTON, (int)button);
            if (combo_binding != GAMEPAD_BIND_NONE) {
                sdl_gamepad_send_key_raw(combo_binding);
                return;
            }
        }

        if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
            int binding = config.gamepad_button_bindings[button];
            if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                sdl_gamepad_apply_modifier(binding, down);
                return;
            }
        }

        if (down) {
            if (g_gamepad_state.shoulder_pending &&
                g_gamepad_state.shoulder_pending_button != (int)button) {
                sdl_gamepad_clear_pending_shoulder();
                sdl_gamepad_send_shoulder_combo();
            } else {
                sdl_gamepad_set_pending_shoulder((int)button);
            }
        } else if (g_gamepad_state.shoulder_pending &&
                   g_gamepad_state.shoulder_pending_button == (int)button) {
            sdl_gamepad_flush_pending_shoulder(SDL_GetTicksNS(), true);
        }
        return;
    }

    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return;

    if (down) {
        int active_modifier = sdl_gamepad_single_active_modifier();
        int combo_binding = GAMEPAD_BIND_NONE;

        if (active_modifier != GAMEPAD_BIND_NONE) {
            combo_binding = sdl_gamepad_combo_binding_for_input(active_modifier,
                GAMEPAD_CAPTURE_BUTTON, (int)button);
            if (combo_binding != GAMEPAD_BIND_NONE) {
                sdl_gamepad_send_key_raw(combo_binding);
                return;
            }
        }
    }

    int binding = config.gamepad_button_bindings[button];
    if (sdl_gamepad_handle_confirm_long_press_button((int)button, binding, down))
        return;

    if (binding == GAMEPAD_BIND_NONE)
        return;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, down);
        return;
    }

    if (down)
        sdl_gamepad_send_key(binding, false);
}

void sdl_gamepad_handle_axis(const SDL_GamepadAxisEvent* ev)
{
    if (!ev)
        return;

    if (g_gamepad_capture_active) {
        bool capture_armed = (SDL_GetTicksNS() >= g_gamepad_capture_arm_time);
        if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX || ev->axis == SDL_GAMEPAD_AXIS_LEFTY) {
            if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX)
                g_gamepad_state.left_x = ev->value;
            else
                g_gamepad_state.left_y = ev->value;

            if (!capture_armed)
                return;

            if (!config.gamepad_use_left_stick) {
                int deadzone = config.gamepad_deadzone;
                if (deadzone < 0)
                    deadzone = 0;
                int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
                if (dir >= 0) {
                    (void)sdl_gamepad_capture_queue_input(
                        GAMEPAD_CAPTURE_LEFT_STICK, dir);
                }
            }
            return;
        }

        if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX || ev->axis == SDL_GAMEPAD_AXIS_RIGHTY) {
            if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX)
                g_gamepad_state.right_x = ev->value;
            else
                g_gamepad_state.right_y = ev->value;

            if (!capture_armed)
                return;

            int deadzone = config.gamepad_deadzone;
            if (deadzone < 0)
                deadzone = 0;
            int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.right_x, g_gamepad_state.right_y, deadzone);
            if (dir >= 0) {
                (void)sdl_gamepad_capture_queue_input(
                    GAMEPAD_CAPTURE_RIGHT_STICK, dir);
            }
            return;
        }

        if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            int threshold = config.gamepad_trigger_threshold;
            if (threshold < 0)
                threshold = 0;
            bool pressed = (ev->value >= threshold);

            if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
                bool was_down = g_gamepad_state.left_trigger_down;
                g_gamepad_state.left_trigger_down = pressed;
                if (capture_armed && pressed && !was_down) {
                    (void)sdl_gamepad_capture_queue_input(
                        GAMEPAD_CAPTURE_TRIGGER, 0);
                }
            } else {
                bool was_down = g_gamepad_state.right_trigger_down;
                g_gamepad_state.right_trigger_down = pressed;
                if (capture_armed && pressed && !was_down) {
                    (void)sdl_gamepad_capture_queue_input(
                        GAMEPAD_CAPTURE_TRIGGER, 1);
                }
            }
        }
        return;
    }

    if (!config.gamepad_enabled)
        return;

    sdl_gamepad_mark_auto_ui();

    if (sdl_minimap_handle_gamepad_axis(ev))
        return;

    if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX || ev->axis == SDL_GAMEPAD_AXIS_LEFTY) {
        if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX)
            g_gamepad_state.left_x = ev->value;
        else
            g_gamepad_state.left_y = ev->value;

        int deadzone = config.gamepad_deadzone;
        if (deadzone < 0)
            deadzone = 0;

        if (config.gamepad_use_left_stick) {
            int dir = sdl_gamepad_axis_to_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
            int prev_dir = g_gamepad_state.left_dir;
            if (dir != prev_dir) {
                g_gamepad_state.left_dir = dir;
                if (dir == 0) {
                    /* Keep pending to allow quick taps to resolve. */
                } else if (dir == 1 || dir == 3 || dir == 7 || dir == 9) {
                    sdl_gamepad_clear_pending_left_stick();
                    sdl_gamepad_send_direction(dir);
                } else {
                    if (prev_dir == 1 || prev_dir == 3 || prev_dir == 7 || prev_dir == 9) {
                        sdl_gamepad_clear_pending_left_stick();
                        return;
                    }
                    if (g_gamepad_state.left_pending)
                        sdl_gamepad_flush_pending_left_stick(SDL_GetTicksNS(), true);
                    sdl_gamepad_set_pending_left_stick(dir);
                }
            }
        } else {
            int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
            int prev_dir = g_gamepad_state.left_bind_dir;
            if (dir != prev_dir) {
                if (prev_dir >= 0 && prev_dir < GAMEPAD_STICK_DIR_COUNT) {
                    int prev_binding = config.gamepad_left_stick_bindings[prev_dir];
                    if (prev_binding == GAMEPAD_BIND_SHIFT || prev_binding == GAMEPAD_BIND_CTRL || prev_binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(prev_binding, false);
                    }
                }

                g_gamepad_state.left_bind_dir = dir;

                if (dir >= 0 && dir < GAMEPAD_STICK_DIR_COUNT) {
                    int active_modifier = sdl_gamepad_single_active_modifier();
                    int binding = config.gamepad_left_stick_bindings[dir];
                    int combo_binding = GAMEPAD_BIND_NONE;

                    if (active_modifier != GAMEPAD_BIND_NONE) {
                        combo_binding = sdl_gamepad_combo_binding_for_input(
                            active_modifier, GAMEPAD_CAPTURE_LEFT_STICK, dir);
                    }

                    if (combo_binding != GAMEPAD_BIND_NONE) {
                        sdl_gamepad_send_key_raw(combo_binding);
                    } else if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(binding, true);
                    } else if (binding != GAMEPAD_BIND_NONE) {
                        sdl_gamepad_send_key(binding, false);
                    }
                }
            }
        }
        return;
    }

    if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX || ev->axis == SDL_GAMEPAD_AXIS_RIGHTY) {
        if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX)
            g_gamepad_state.right_x = ev->value;
        else
            g_gamepad_state.right_y = ev->value;

        int deadzone = config.gamepad_deadzone;
        if (deadzone < 0)
            deadzone = 0;
        int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.right_x, g_gamepad_state.right_y, deadzone);
        int prev_dir = g_gamepad_state.right_dir;
        if (dir != prev_dir) {
            if (prev_dir >= 0 && prev_dir < GAMEPAD_STICK_DIR_COUNT) {
                int prev_binding = config.gamepad_right_stick_bindings[prev_dir];
                if (prev_binding == GAMEPAD_BIND_SHIFT || prev_binding == GAMEPAD_BIND_CTRL || prev_binding == GAMEPAD_BIND_ALT) {
                    sdl_gamepad_apply_modifier(prev_binding, false);
                }
            }

            g_gamepad_state.right_dir = dir;

            if (dir >= 0 && dir < GAMEPAD_STICK_DIR_COUNT) {
                int active_modifier = sdl_gamepad_single_active_modifier();
                int binding = config.gamepad_right_stick_bindings[dir];
                int combo_binding = GAMEPAD_BIND_NONE;

                if (active_modifier != GAMEPAD_BIND_NONE) {
                    combo_binding = sdl_gamepad_combo_binding_for_input(
                        active_modifier, GAMEPAD_CAPTURE_RIGHT_STICK, dir);
                }

                if (combo_binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key_raw(combo_binding);
                } else if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                    sdl_gamepad_apply_modifier(binding, true);
                } else if (binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key(binding, false);
                }
            }
        }
        return;
    }

    if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        int threshold = config.gamepad_trigger_threshold;
        if (threshold < 0)
            threshold = 0;
        bool pressed = (ev->value >= threshold);

        if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            if (pressed != g_gamepad_state.left_trigger_down) {
                g_gamepad_state.left_trigger_down = pressed;
                int binding = config.gamepad_trigger_bindings[0];
                int combo_binding = GAMEPAD_BIND_NONE;

                if (pressed) {
                    int active_modifier = sdl_gamepad_single_active_modifier();
                    if (active_modifier != GAMEPAD_BIND_NONE) {
                        combo_binding = sdl_gamepad_combo_binding_for_input(
                            active_modifier, GAMEPAD_CAPTURE_TRIGGER, 0);
                    }
                }

                if (combo_binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key_raw(combo_binding);
                } else if (binding != GAMEPAD_BIND_NONE) {
                    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(binding, pressed);
                    } else if (pressed) {
                        sdl_gamepad_send_key(binding, false);
                    }
                }
            }
        } else {
            if (pressed != g_gamepad_state.right_trigger_down) {
                g_gamepad_state.right_trigger_down = pressed;
                int binding = config.gamepad_trigger_bindings[1];
                int combo_binding = GAMEPAD_BIND_NONE;

                if (pressed) {
                    int active_modifier = sdl_gamepad_single_active_modifier();
                    if (active_modifier != GAMEPAD_BIND_NONE) {
                        combo_binding = sdl_gamepad_combo_binding_for_input(
                            active_modifier, GAMEPAD_CAPTURE_TRIGGER, 1);
                    }
                }

                if (combo_binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key_raw(combo_binding);
                } else if (binding != GAMEPAD_BIND_NONE) {
                    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(binding, pressed);
                    } else if (pressed) {
                        sdl_gamepad_send_key(binding, false);
                    }
                }
            }
        }
    }
}

void sdl_gamepad_open(SDL_JoystickID id)
{
    if (!SDL_IsGamepad(id)) {
        log_debug("Ignoring non-gamepad device id %d", (int)id);
        return;
    }

    if (SDL_GetGamepadFromID(id)) {
        return;
    }

    SDL_Gamepad* pad = SDL_OpenGamepad(id);
    if (!pad) {
        log_warn("Failed to open gamepad id %d: %s", (int)id, SDL_GetError());
        return;
    }

    if (g_gamepad_state.pad_count >= MAX_GAMEPADS) {
        SDL_CloseGamepad(pad);
        log_warn("Gamepad list full, closing id %d", (int)id);
        return;
    }

    g_gamepad_state.pads[g_gamepad_state.pad_count].id = id;
    g_gamepad_state.pads[g_gamepad_state.pad_count].pad = pad;
    g_gamepad_state.pad_count++;

    log_info("Gamepad opened id %d (%s)", (int)id, SDL_GetGamepadName(pad));
    sdl_gamepad_mark_auto_ui();
}

void sdl_gamepad_close(SDL_JoystickID id)
{
    for (int i = 0; i < g_gamepad_state.pad_count; i++) {
        if (g_gamepad_state.pads[i].id == id) {
            SDL_CloseGamepad(g_gamepad_state.pads[i].pad);
            g_gamepad_state.pads[i] = g_gamepad_state.pads[g_gamepad_state.pad_count - 1];
            g_gamepad_state.pad_count--;
            log_info("Gamepad closed id %d", (int)id);
            break;
        }
    }
}

void sdl_gamepad_handle_device(const SDL_GamepadDeviceEvent* ev)
{
    if (!ev)
        return;

    if (ev->type == SDL_EVENT_GAMEPAD_ADDED) {
        sdl_gamepad_open(ev->which);
    } else if (ev->type == SDL_EVENT_GAMEPAD_REMOVED) {
        sdl_gamepad_close(ev->which);
    }
}

void sdl_gamepad_init(void)
{
    SDL_SetGamepadEventsEnabled(true);
    g_gamepad_state.left_bind_dir = -1;
    g_gamepad_state.right_dir = -1;
    sdl_gamepad_clear_pending_shoulder();
    SDL_UpdateGamepads();
    SDL_PumpEvents();

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) {
        log_warn("SDL_GetGamepads failed: %s", SDL_GetError());
        return;
    }

    log_info("SDL_GetGamepads returned %d gamepad%s",
        count, (count == 1) ? "" : "s");
#if defined(SDL_PLATFORM_ANDROID)
    if (count == 0 && sdl_android_has_controller_device()) {
        log_warn("Android InputDevice reports a controller, but SDL_GetGamepads returned none at startup");
    }
#endif

    for (int i = 0; i < count; i++) {
        sdl_gamepad_open(ids[i]);
    }
    SDL_free(ids);
}

void sdl_gamepad_shutdown(void)
{
    while (g_gamepad_state.pad_count > 0) {
        SDL_JoystickID id = g_gamepad_state.pads[g_gamepad_state.pad_count - 1].id;
        sdl_gamepad_close(id);
    }
}

