#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "fs/io_sdl.h"
#include "log/log.h"
#include "main.h"
#include "z-term.h"
#include "pane.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include "sound-config.h"
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

const char help_sdl[] = "SDL3";

static const char* const sdl_story_fallback_font = "lib/xtra/font/MarcellusSC-Regular.ttf";

enum {
    TILE_SIZE = 16,
    MAX_TERM_DATA = 8,
    MAX_STORY_FONT_CACHE = 4,
    MAX_PANE_CONFIGS = 8,
};

// SDL configuration (loaded from INI file)
struct sdl_config config;

// Sound configuration (loaded from sound.json)
struct sound_config g_sound_config;

// Configuration file path (needed for saving on exit)
char config_file_path[1024];

// Default pane configuration
static const struct pane_config default_pane_config[] = {
    // On the right
    {.pane = PANE_INVENTORY, .where = PLACE_RIGHT},
    {.pane = PANE_WORN, .where = PLACE_RIGHT},
    {.pane = PANE_INFO, .where = PLACE_RIGHT, .rect.rows = 8},
    // In the bottom
    {.pane = PANE_ROLLS, .where = PLACE_BOTTOM, .rect.rows = 4},
    {.pane = PANE_LOG, .where = PLACE_BOTTOM},
};
const int default_pane_config_count = sizeof(default_pane_config) / sizeof(struct pane_config);

// Active pane configuration (may be loaded from INI)
struct pane_config pane_config[MAX_PANE_CONFIGS];
int pane_config_count = 0;

typedef struct story_font_entry {
    int pixel_height;
    TTF_Font* font;
} story_font_entry;

typedef struct sdl_state {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* tileset;
    SDL_Color palette[16];
    float system_scale;
    int tileset_cols;
    bool need_present;
    bool use_tiles;
    
    // Custom fonts
    story_font_entry story_fonts[MAX_STORY_FONT_CACHE];
    int story_font_count;
    int story_font_depth;      // Nesting counter for story font enable/disable
    bool story_font_grid;      // Whether queued story text should snap to cell grid
    
} sdl_state;

typedef struct sdl_view {
    SDL_Rect rect;
    SDL_Texture* canvas;
    SDL_Texture* font_atlas;
    int ttf_font_size;
    int cell_w;
    int cell_h;
    int cols;
    int rows;
    int margin_x;
    int margin_y;
    term t;
    bool term_ready;
} sdl_view;

enum {
    MAX_GAMEPADS = 4,
    DPAD_DIAGONAL_WINDOW_MS = 100,
    SHOULDER_COMBO_WINDOW_MS = 150,
};

typedef struct gamepad_entry {
    SDL_JoystickID id;
    SDL_Gamepad* pad;
} gamepad_entry;

typedef struct gamepad_input_state {
    gamepad_entry pads[MAX_GAMEPADS];
    int pad_count;
    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;
    int dpad_dir;
    bool dpad_pending;
    int dpad_pending_dir;
    Uint64 dpad_pending_time;
    bool dpad_pending_shift;
    bool dpad_pending_ctrl;
    bool dpad_pending_alt;
    Sint16 left_x;
    Sint16 left_y;
    int left_dir;
    int left_bind_dir;
    bool left_pending;
    int left_pending_dir;
    Uint64 left_pending_time;
    bool left_pending_shift;
    bool left_pending_ctrl;
    bool left_pending_alt;
    Sint16 right_x;
    Sint16 right_y;
    int right_dir;
    bool left_shoulder_down;
    bool right_shoulder_down;
    bool shoulder_pending;
    int shoulder_pending_button;
    Uint64 shoulder_pending_time;
    bool left_trigger_down;
    bool right_trigger_down;
    int shift_held;
    int ctrl_held;
    int alt_held;
} gamepad_input_state;

sdl_state g_state;
sdl_view g_views[MAX_TERM_DATA];
static gamepad_input_state g_gamepad_state;
static bool g_gamepad_auto_ui = false;
static int g_default_gamepad_button_bindings[SDL_GAMEPAD_BUTTON_COUNT];
static int g_default_gamepad_trigger_bindings[GAMEPAD_TRIGGER_COUNT];
static int g_default_gamepad_left_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_shoulder_combo_binding = GAMEPAD_BIND_NONE;
static bool g_default_gamepad_bindings_ready = false;
static bool g_gamepad_capture_active = false;
static bool g_gamepad_capture_ready = false;
static int g_gamepad_capture_type = GAMEPAD_CAPTURE_BUTTON;
static int g_gamepad_capture_id = 0;

static sdl_view* sdl_view_from_term(term* t);
static void sdl_view_destroy(sdl_view* d);
void resize(const SDL_Rect* screen);
bool steamdeck_controls_active(void);
static void sdl_handle_event(sdl_state* st, const SDL_Event* ev);
static void sdl_quit_hook(cptr str);
static errr callback_sdl_xtra(int n, int v);
static void sdl_gamepad_init(void);
static void sdl_gamepad_shutdown(void);
static void sdl_gamepad_handle_button(const SDL_GamepadButtonEvent* ev);
static void sdl_gamepad_handle_axis(const SDL_GamepadAxisEvent* ev);
static void sdl_gamepad_handle_device(const SDL_GamepadDeviceEvent* ev);
static void sdl_gamepad_open(SDL_JoystickID id);
static void sdl_gamepad_close(SDL_JoystickID id);
static void sdl_gamepad_mark_auto_ui(void);
static int sdl_gamepad_axis_to_dir(Sint16 x, Sint16 y, int deadzone);
static int sdl_gamepad_axis_to_cardinal_dir(Sint16 x, Sint16 y, int deadzone);
static void sdl_gamepad_send_direction(int dir);
static void sdl_gamepad_send_direction_mods(int dir, bool shift, bool ctrl, bool alt);
static void sdl_gamepad_send_key(int key, bool use_macro_mods);
static void sdl_gamepad_send_macro_key(int key, bool shift, bool ctrl, bool alt);
static void sdl_gamepad_apply_modifier(int binding, bool down);
static bool sdl_gamepad_shift_active(void);
static bool sdl_gamepad_ctrl_active(void);
static bool sdl_gamepad_alt_active(void);
static void sdl_gamepad_clear_pending_dpad(void);
static void sdl_gamepad_set_pending_dpad(int dir);
static bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force);
static void sdl_gamepad_clear_pending_left_stick(void);
static void sdl_gamepad_set_pending_left_stick(int dir);
static bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force);
static void sdl_gamepad_clear_pending_shoulder(void);
static void sdl_gamepad_set_pending_shoulder(int button);
static bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force);
static int sdl_gamepad_pending_timeout_ms(Uint64 now_ns);
static void sdl_apply_story_font_state(bool active);
static void sdl_apply_story_grid_state(bool grid);
static void sdl_story_font_reset_state(void);
static void sdl_story_font_cache_clear(void);
static TTF_Font* sdl_story_font_for_height(int pixel_height);
static TTF_Font* sdl_story_font_for_view(const sdl_view* d);
static void sdl_render_mono_text(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col);
static void sdl_render_story_text_free(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col);
static void sdl_render_story_text_grid(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col);
static int sdl_render_story_text_free_px(sdl_view* d, TTF_Font* font, float x_px, int y, const char* s, int n,
    SDL_Color col,
    float max_w_px);
static void sdl_render_story_row_packed(sdl_view* d, TTF_Font* font, int y, const byte* story_row,
    const char* row_chars, const byte* row_attr);
static void draw_cursor(int x, int y, bool big);
static errr callback_sdl_curs(int x, int y);
static errr callback_sdl_bigcurs(int x, int y);
static errr callback_sdl_wipe(int x, int y, int n);
static errr callback_sdl_text(int x, int y, int n, byte a, cptr s);
static errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp,
                       const byte* tap, const char* tcp);
static void callback_sdl_nuke();
static void callback_sdl_init(term* t);
static errr sdl_view_link_term(sdl_view* d, int term_index);
static SDL_Texture* sdl_load_ttf_font(const char* font_path, int font_size, int* actual_font_size);
static void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles);
static void sdl_window_set_position(int x, int y);
static void sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin);
static void sdl_load_story_fonts(void);
static TTF_Font* sdl_load_font_with_fallback(const char* font_path, int font_size, const char* fallback_path);

static sdl_view* sdl_view_from_term(term* t)
{
    return &g_views[(size_t)t->data];
}

/*
 * Synchronize the SDL palette from angband_color_table.
 * This allows color customizations from .prf files to work.
 */
static void sdl_sync_palette(void)
{
    for (int i = 0; i < 16; i++) {
        g_state.palette[i].r = angband_color_table[i][1];
        g_state.palette[i].g = angband_color_table[i][2];
        g_state.palette[i].b = angband_color_table[i][3];
        g_state.palette[i].a = 255;
    }
}

static void sdl_view_destroy(sdl_view* d)
{
    if (d->canvas) {
        SDL_DestroyTexture(d->canvas);
        d->canvas = NULL;
    }
    if (d->font_atlas) {
        SDL_DestroyTexture(d->font_atlas);
        d->font_atlas = NULL;
    }
}

static bool sdl_gamepad_shift_active(void)
{
    return g_gamepad_state.shift_held > 0;
}

static bool sdl_gamepad_ctrl_active(void)
{
    return g_gamepad_state.ctrl_held > 0;
}

static bool sdl_gamepad_alt_active(void)
{
    return g_gamepad_state.alt_held > 0;
}

static void sdl_gamepad_mark_auto_ui(void)
{
    if (config.gamepad_auto_mode)
        g_gamepad_auto_ui = true;
}

static void sdl_gamepad_apply_modifier(int binding, bool down)
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
    } else if (binding == GAMEPAD_BIND_ALT) {
        g_gamepad_state.alt_held += delta;
        if (g_gamepad_state.alt_held < 0)
            g_gamepad_state.alt_held = 0;
    }
}

static void sdl_gamepad_send_macro_key(int key, bool shift, bool ctrl, bool alt)
{
    Term_keypress(31);
    if (ctrl)
        Term_keypress('C');
    if (shift)
        Term_keypress('S');
    if (alt)
        Term_keypress('A');
    Term_keypress('x');
    Term_keypress(hexsym[(key / 16) & 0x0F]);
    Term_keypress(hexsym[key % 16]);
    Term_keypress(13);
    log_debug("send gamepad macro key=%d ^_%s%s%sx%x%x\r",
        key, ctrl ? "C" : "", shift ? "S" : "", alt ? "A" : "", key / 16, key % 16);
}

static void sdl_gamepad_send_key(int key, bool use_macro_mods)
{
    bool shift = sdl_gamepad_shift_active();
    bool ctrl = sdl_gamepad_ctrl_active();
    bool alt = sdl_gamepad_alt_active();

    if (use_macro_mods && (shift || ctrl || alt)) {
        sdl_gamepad_send_macro_key(key, shift, ctrl, alt);
        return;
    }

    if (SDL_isprint(key)) {
        if (ctrl && !alt && SDL_isalpha(key)) {
            Term_keypress(KTRL(key));
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
        sdl_gamepad_send_macro_key(key, shift, ctrl, alt);
    } else {
        Term_keypress(key);
    }
}

static void sdl_gamepad_send_shoulder_combo(void)
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

static void sdl_gamepad_send_direction_mods(int dir, bool shift, bool ctrl, bool alt)
{
    if (dir < 1 || dir > 9)
        return;

    if (shift || ctrl || alt) {
        sdl_gamepad_send_macro_key('0' + dir, shift, ctrl, alt);
    } else {
        Term_keypress('0' + dir);
    }
}

static int sdl_gamepad_axis_to_dir(Sint16 x, Sint16 y, int deadzone)
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

static int sdl_gamepad_axis_to_cardinal_dir(Sint16 x, Sint16 y, int deadzone)
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

static void sdl_gamepad_send_direction(int dir)
{
    sdl_gamepad_send_direction_mods(dir, sdl_gamepad_shift_active(),
        sdl_gamepad_ctrl_active(), sdl_gamepad_alt_active());
}

static void sdl_gamepad_clear_pending_dpad(void)
{
    g_gamepad_state.dpad_pending = false;
    g_gamepad_state.dpad_pending_dir = 0;
    g_gamepad_state.dpad_pending_time = 0;
    g_gamepad_state.dpad_pending_shift = false;
    g_gamepad_state.dpad_pending_ctrl = false;
    g_gamepad_state.dpad_pending_alt = false;
}

static void sdl_gamepad_set_pending_dpad(int dir)
{
    g_gamepad_state.dpad_pending = true;
    g_gamepad_state.dpad_pending_dir = dir;
    g_gamepad_state.dpad_pending_time = SDL_GetTicksNS();
    g_gamepad_state.dpad_pending_shift = sdl_gamepad_shift_active();
    g_gamepad_state.dpad_pending_ctrl = sdl_gamepad_ctrl_active();
    g_gamepad_state.dpad_pending_alt = sdl_gamepad_alt_active();
}

static bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force)
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

static void sdl_gamepad_clear_pending_left_stick(void)
{
    g_gamepad_state.left_pending = false;
    g_gamepad_state.left_pending_dir = 0;
    g_gamepad_state.left_pending_time = 0;
    g_gamepad_state.left_pending_shift = false;
    g_gamepad_state.left_pending_ctrl = false;
    g_gamepad_state.left_pending_alt = false;
}

static void sdl_gamepad_set_pending_left_stick(int dir)
{
    g_gamepad_state.left_pending = true;
    g_gamepad_state.left_pending_dir = dir;
    g_gamepad_state.left_pending_time = SDL_GetTicksNS();
    g_gamepad_state.left_pending_shift = sdl_gamepad_shift_active();
    g_gamepad_state.left_pending_ctrl = sdl_gamepad_ctrl_active();
    g_gamepad_state.left_pending_alt = sdl_gamepad_alt_active();
}

static bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force)
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

static void sdl_gamepad_clear_pending_shoulder(void)
{
    g_gamepad_state.shoulder_pending = false;
    g_gamepad_state.shoulder_pending_button = 0;
    g_gamepad_state.shoulder_pending_time = 0;
}

static void sdl_gamepad_set_pending_shoulder(int button)
{
    g_gamepad_state.shoulder_pending = true;
    g_gamepad_state.shoulder_pending_button = button;
    g_gamepad_state.shoulder_pending_time = SDL_GetTicksNS();
}

static bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force)
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

static int sdl_gamepad_pending_timeout_ms(Uint64 now_ns)
{
    int dpad_timeout = -1;
    int left_timeout = -1;
    int shoulder_timeout = -1;

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

    if (dpad_timeout < 0 && left_timeout < 0)
        return shoulder_timeout;
    if (dpad_timeout < 0 && shoulder_timeout < 0)
        return left_timeout;
    if (left_timeout < 0 && shoulder_timeout < 0)
        return dpad_timeout;
    if (dpad_timeout < 0)
        return (left_timeout < shoulder_timeout) ? left_timeout : shoulder_timeout;
    if (left_timeout < 0)
        return (dpad_timeout < shoulder_timeout) ? dpad_timeout : shoulder_timeout;
    if (shoulder_timeout < 0)
        return (dpad_timeout < left_timeout) ? dpad_timeout : left_timeout;
    {
        int min = dpad_timeout;
        if (left_timeout < min)
            min = left_timeout;
        if (shoulder_timeout < min)
            min = shoulder_timeout;
        return min;
    }
}

static const char* sdl_gamepad_button_label(int button)
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

static const char* sdl_gamepad_button_short_label(int button)
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

static const char* sdl_gamepad_trigger_label(int index)
{
    if (index == 0)
        return "L2 (Left Trigger)";
    if (index == 1)
        return "R2 (Right Trigger)";
    return "Unknown Trigger";
}

static const char* sdl_gamepad_trigger_short_label(int index)
{
    if (index == 0)
        return "L2";
    if (index == 1)
        return "R2";
    return "?";
}

static const char* sdl_gamepad_stick_dir_label(int type, int dir, bool short_label)
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

static void sdl_gamepad_binding_label_ex(int type, int id, char* buf, size_t buflen, bool short_label)
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

static int sdl_gamepad_action_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (config.gamepad_button_bindings[i] == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (config.gamepad_trigger_bindings[i] == binding) {
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
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (config.gamepad_right_stick_bindings[i] == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    if (config.gamepad_shoulder_combo_binding == binding) {
        if (count == 0 && out_type && out_id) {
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
            *out_id = 0;
        }
        count++;
    }

    return count;
}

static void sdl_gamepad_action_binding_label_ex(int binding, char* buf, size_t buflen, bool short_label)
{
    if (!buf || !buflen)
        return;

    int type = 0;
    int id = 0;
    int count = sdl_gamepad_action_binding_count(binding, &type, &id);
    if (count <= 0) {
        SDL_strlcpy(buf, "(unbound)", buflen);
    } else if (count == 1) {
        sdl_gamepad_binding_label_ex(type, id, buf, buflen, short_label);
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

static void sdl_gamepad_handle_button(const SDL_GamepadButtonEvent* ev)
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
        bool shoulder_button = (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
            || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        if (shoulder_button) {
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
                g_gamepad_capture_type = GAMEPAD_CAPTURE_BUTTON;
                g_gamepad_capture_id = (int)button;
                g_gamepad_capture_ready = true;
                g_gamepad_capture_active = false;
            }
            return;
        }

        if (down) {
            bool dpad_button = (button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN
                || button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            if (!dpad_button || !config.gamepad_use_dpad) {
                if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_BUTTON;
                    g_gamepad_capture_id = (int)button;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                }
            }
        }
        return;
    }

    if (!config.gamepad_enabled)
        return;

    sdl_gamepad_mark_auto_ui();

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

    int binding = config.gamepad_button_bindings[button];
    if (binding == GAMEPAD_BIND_NONE)
        return;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, down);
        return;
    }

    if (down)
        sdl_gamepad_send_key(binding, false);
}

static void sdl_gamepad_handle_axis(const SDL_GamepadAxisEvent* ev)
{
    if (!ev)
        return;

    if (g_gamepad_capture_active) {
        if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX || ev->axis == SDL_GAMEPAD_AXIS_LEFTY) {
            if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX)
                g_gamepad_state.left_x = ev->value;
            else
                g_gamepad_state.left_y = ev->value;

            if (!config.gamepad_use_left_stick) {
                int deadzone = config.gamepad_deadzone;
                if (deadzone < 0)
                    deadzone = 0;
                int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
                if (dir >= 0) {
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_LEFT_STICK;
                    g_gamepad_capture_id = dir;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
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
            if (dir >= 0) {
                g_gamepad_capture_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                g_gamepad_capture_id = dir;
                g_gamepad_capture_ready = true;
                g_gamepad_capture_active = false;
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
                if (pressed && !was_down) {
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_TRIGGER;
                    g_gamepad_capture_id = 0;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                }
            } else {
                bool was_down = g_gamepad_state.right_trigger_down;
                g_gamepad_state.right_trigger_down = pressed;
                if (pressed && !was_down) {
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_TRIGGER;
                    g_gamepad_capture_id = 1;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                }
            }
        }
        return;
    }

    if (!config.gamepad_enabled)
        return;

    sdl_gamepad_mark_auto_ui();

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
                    int binding = config.gamepad_left_stick_bindings[dir];
                    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
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
                int binding = config.gamepad_right_stick_bindings[dir];
                if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
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
                if (binding != GAMEPAD_BIND_NONE) {
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
                if (binding != GAMEPAD_BIND_NONE) {
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

static void sdl_gamepad_open(SDL_JoystickID id)
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

static void sdl_gamepad_close(SDL_JoystickID id)
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

static void sdl_gamepad_handle_device(const SDL_GamepadDeviceEvent* ev)
{
    if (!ev)
        return;

    if (ev->type == SDL_EVENT_GAMEPAD_ADDED) {
        sdl_gamepad_open(ev->which);
    } else if (ev->type == SDL_EVENT_GAMEPAD_REMOVED) {
        sdl_gamepad_close(ev->which);
    }
}

static void sdl_gamepad_init(void)
{
    SDL_SetGamepadEventsEnabled(true);
    g_gamepad_state.left_bind_dir = -1;
    g_gamepad_state.right_dir = -1;
    sdl_gamepad_clear_pending_shoulder();

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) {
        log_warn("SDL_GetGamepads failed: %s", SDL_GetError());
        return;
    }

    for (int i = 0; i < count; i++) {
        sdl_gamepad_open(ids[i]);
    }
    SDL_free(ids);
}

static void sdl_gamepad_shutdown(void)
{
    while (g_gamepad_state.pad_count > 0) {
        SDL_JoystickID id = g_gamepad_state.pads[g_gamepad_state.pad_count - 1].id;
        sdl_gamepad_close(id);
    }
}

void resize(const SDL_Rect* screen)
{
    log_warn("resize enter");
    SDL_Rect panes[MAX_TERM_DATA] = {0};
    place_panes(pane_config, pane_config_count, panes, screen,
        g_state.system_scale * config.aux_view_font_size / 2,
        g_state.system_scale * config.aux_view_font_size,
        g_state.system_scale * config.margin);
    for (int i = 0; i < PANE_MAX; i++) {
        const SDL_Rect* r = &panes[i];
        log_debug("pane %d is at (%d, %d) size %dx%d", i, r->x, r->y, r->w, r->h);
    }

    // Check whether after splitting the window the main view is larger than
    // 80x24. If it isn't, remove panes along the corresponding axis (or axes).
    // Also remove panes if user has disabled them via settings.
    {
        int cell_w = config.main_view_scale * TILE_SIZE / 2;
        int cell_h = config.main_view_scale * TILE_SIZE;
        log_debug("Cell dimensions: %dx%d (scale=%d, TILE_SIZE=%d)", cell_w, cell_h, config.main_view_scale, TILE_SIZE);
        // panes are already in window coordinate space, no need to multiply by system_scale
        int cols = panes[PANE_MAIN].w / cell_w;
        int rows = panes[PANE_MAIN].h / cell_h;
        log_debug("Main view: %dx%d pixels = %dx%d cells (minimum required: 80x24)", 
                  panes[PANE_MAIN].w, panes[PANE_MAIN].h, cols, rows);
        if (cols < 80 || !config.enable_right_panes) {
            if (cols < 80) {
                log_warn("main view too small, %d cols < 80 — removing right panes", cols);
            } else {
                log_info("right panes disabled by user setting");
            }
            log_debug("Before removing right panes: main view width = %d", panes[PANE_MAIN].w);
            for (int i = 0; i < pane_config_count; i++) {
                if (pane_config[i].where == PLACE_RIGHT) {
                    log_debug("Removing pane %d (type=%d) from right", i, pane_config[i].pane);
                    panes[pane_config[i].pane].w = 0;
                }
            }
            panes[PANE_MAIN].w = screen->w;
            log_debug("After removing right panes: main view width = %d, cols = %d", 
                      panes[PANE_MAIN].w, panes[PANE_MAIN].w / cell_w);
        }
        if (rows < 24 || !config.enable_bottom_panes) {
            if (rows < 24) {
                log_warn("main view too small, %d rows < 24 — removing bottom panes", rows);
            } else {
                log_info("bottom panes disabled by user setting");
            }
            log_debug("Before removing bottom panes: main view height = %d", panes[PANE_MAIN].h);
            for (int i = 0; i < pane_config_count; i++) {
                if (pane_config[i].where == PLACE_BOTTOM) {
                    log_debug("Removing pane %d (type=%d) from bottom", i, pane_config[i].pane);
                    panes[pane_config[i].pane].w = 0;
                }
            }
            panes[PANE_MAIN].h = screen->h;
            log_debug("After removing bottom panes: main view height = %d, rows = %d",
                      panes[PANE_MAIN].h, panes[PANE_MAIN].h / cell_h);
        }
    }

    // Use configured monospace font or fall back to default
    const char* font_path = config.monospace_font[0] != '\0' 
        ? config.monospace_font 
        : "lib/xtra/font/InputMono-Bold.ttf";

    for (int i = 1; i < MAX_TERM_DATA; i++) {
        // Always destroy the old pane to prevent its display in cases when we
        // have removed one of the bars or both of them due to the size
        // restrictions.
        sdl_view_destroy(&g_views[i]);
        if (panes[i].w) {
            sdl_view_create(&g_views[i], panes[i], font_path, config.aux_view_font_size, 0, config.margin);
            sdl_view_link_term(&g_views[i], i);
        }
    }

    sdl_view_destroy(&g_views[0]);
    sdl_view_create(&g_views[0], panes[PANE_MAIN], font_path, 0, config.main_view_scale, config.margin);
    sdl_view_link_term(&g_views[0], 0);

    Term_activate(&g_views[0].t);
    // Don't strictly need this as `sdl_view_create` already sets this flag.
    g_state.need_present = true;
}

static void sdl_handle_event(sdl_state* st, const SDL_Event* ev)
{
    (void)st;
    if (ev->type == SDL_EVENT_QUIT) {
        Term_keypress(27); // ESC or define a quit signal
    } else if (ev->type == SDL_EVENT_KEY_DOWN) {
        int key = ev->key.key;
        // Ignore bare modifiers.
        if (key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
            key == SDLK_LALT || key == SDLK_RALT ||
            key == SDLK_LCTRL || key == SDLK_RCTRL ||
            key == SDLK_LGUI || key == SDLK_RGUI)
        {
            return;
        }

        // Handle Alt key combinations for pane settings directly
        bool alt = ev->key.mod & SDL_KMOD_ALT;
        if (alt && !character_dungeon) {
            // Only allow these in the dungeon, not in menus
            return;
        }
        
        if (alt) {
            bool handled = false;
            
            // Alt++ or Alt+= : Increase main view scale
            if (key == '+' || key == '=') {
                int current_scale = get_sdl_main_view_scale();
                int max_scale = get_sdl_max_scale();
                if (current_scale < max_scale) {
                    set_sdl_main_view_scale(current_scale + 1);
                    sdl_apply_config();
                    Term_keypress(KTRL('R')); // Trigger redraw
                }
                handled = true;
            }
            // Alt+- : Decrease main view scale
            else if (key == '-') {
                int current_scale = get_sdl_main_view_scale();
                if (current_scale > 1) {
                    set_sdl_main_view_scale(current_scale - 1);
                    sdl_apply_config();
                    Term_keypress(KTRL('R')); // Trigger redraw
                }
                handled = true;
            }
            // Alt+I : Toggle right panes
            else if (key == 'i' || key == 'I') {
                bool enabled = get_sdl_enable_right_panes();
                set_sdl_enable_right_panes(!enabled);
                sdl_apply_config();
                Term_keypress(KTRL('R')); // Trigger redraw
                handled = true;
            }
            // Alt+L : Toggle bottom panes
            else if (key == 'l' || key == 'L') {
                bool enabled = get_sdl_enable_bottom_panes();
                set_sdl_enable_bottom_panes(!enabled);
                sdl_apply_config();
                Term_keypress(KTRL('R')); // Trigger redraw
                handled = true;
            }
            
            if (handled) {
                return;
            }
        }

        if (SDL_isprint(ev->key.key)) {
            /* If Ctrl+letter (no Alt/GUI), send the corresponding control char
             * (so Ctrl-A -> ASCII 1) to preserve traditional control bindings
             * like Ctrl-A for the debug menu. For other modifier combinations
             * or non-alpha printables, keep existing behavior. */
            bool ctrl = ev->key.mod & SDL_KMOD_CTRL;
            bool alt = ev->key.mod & SDL_KMOD_ALT;
            bool gui = ev->key.mod & SDL_KMOD_GUI;
            if (ctrl && !alt && !gui && SDL_isalpha(key)) {
                /* Map to control character */
                Term_keypress(KTRL(key));
            } else {
                if (ev->key.mod & SDL_KMOD_SHIFT) {
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
            }
        } else {
            bool shift = ev->key.mod & SDL_KMOD_SHIFT;
            bool alt = ev->key.mod & SDL_KMOD_ALT;
            bool ctrl = ev->key.mod & SDL_KMOD_CTRL;
            bool gui = ev->key.mod & SDL_KMOD_GUI;
            bool mod = shift || alt || ctrl || gui;
            switch (key) {
                case SDLK_UP:
                case SDLK_KP_8:
                    key = '8';
                    break;
                case SDLK_DOWN:
                case SDLK_KP_2:
                    key = '2';
                    break;
                case SDLK_LEFT:
                case SDLK_KP_4:
                    key = '4';
                    break;
                case SDLK_RIGHT:
                case SDLK_KP_6:
                    key = '6';
                    break;
                case SDLK_KP_1:
                    key = '1';
                    break;
                case SDLK_KP_3:
                    key = '3';
                    break;
                case SDLK_KP_7:
                    key = '7';
                    break;
                case SDLK_KP_9:
                    key = '9';
                    break;
                case SDLK_KP_5:
                    key = '5';
                    break;
            }
            if (mod) {
                /* Begin the macro trigger */
                Term_keypress(31);
                /* Send the modifiers */
                if (ctrl || gui)
                    Term_keypress('C');
                if (shift)
                    Term_keypress('S');
                if (alt)
                    Term_keypress('A');
                /* Introduce the scan code */
                Term_keypress('x');
                /* Encode the hexidecimal scan code */
                Term_keypress(hexsym[key / 16]);
                Term_keypress(hexsym[key % 16]);
                /* End the macro trigger */
                Term_keypress(13);
                log_debug("send macro key=%d ^_%s%s%sx%x%x\r", key, (ctrl || gui) ? "C" : "",
                    shift ? "S" : "", alt ? "A" : "", key / 16, key % 16);
            } else {
                Term_keypress(key);
            }
        }
    } else if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        sdl_gamepad_handle_button(&ev->gbutton);
    } else if (ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        sdl_gamepad_handle_axis(&ev->gaxis);
    } else if (ev->type == SDL_EVENT_GAMEPAD_ADDED || ev->type == SDL_EVENT_GAMEPAD_REMOVED
        || ev->type == SDL_EVENT_GAMEPAD_REMAPPED) {
        sdl_gamepad_handle_device(&ev->gdevice);
    } else if (ev->type == SDL_EVENT_WINDOW_RESIZED) {
        log_debug("window resized to %dx%d", ev->window.data1, ev->window.data2);
        SDL_Rect window = { 0 };
        SDL_GetWindowSizeInPixels(g_state.window, &window.w, &window.h);
        log_debug("new window size in pixels %dx%d", window.w, window.h);
        // SDL_Rect window = {.w = ev->window.data1, .h = ev->window.data2};
        resize(&window);
    } else if (ev->type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED ||
        ev->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
        ev->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {

        float scale = SDL_GetWindowDisplayScale(g_state.window);
        if (scale != g_state.system_scale) {
            log_info("new system scale is %g", scale);
            g_state.system_scale = scale;
            sdl_load_story_fonts();
            SDL_Rect window = { 0 };
            SDL_GetWindowSizeInPixels(g_state.window, &window.w, &window.h);
            log_debug("window size in pixels %dx%d", window.w, window.h);
            resize(&window);
        }
    }
}

static errr callback_sdl_xtra(int n, int v)
{
    sdl_view* d = sdl_view_from_term(Term);
    switch (n) {
    case TERM_XTRA_EVENT: {
        SDL_Event ev;
        if (v) {
            sdl_music_update(); /* Update music before waiting */
            Uint64 now_ns = SDL_GetTicksNS();
            int timeout_ms = sdl_gamepad_pending_timeout_ms(now_ns);
            if (timeout_ms >= 0) {
                if (SDL_WaitEventTimeout(&ev, timeout_ms))
                    sdl_handle_event(&g_state, &ev);
            } else {
                if (SDL_WaitEvent(&ev))
                    sdl_handle_event(&g_state, &ev);
            }
            Uint64 flush_ns = SDL_GetTicksNS();
            sdl_gamepad_flush_pending_dpad(flush_ns, false);
            sdl_gamepad_flush_pending_left_stick(flush_ns, false);
            sdl_gamepad_flush_pending_shoulder(flush_ns, false);
            sdl_music_update(); /* Update music after handling event */
        } else {
            /* Non-blocking scan so animation loops (intro fades, etc.) keep running */
            bool handled = false;
            sdl_music_update(); /* Update music streams */
            while (SDL_PollEvent(&ev)) {
                handled = true;
                sdl_handle_event(&g_state, &ev);
            }
            Uint64 flush_ns = SDL_GetTicksNS();
            sdl_gamepad_flush_pending_dpad(flush_ns, false);
            sdl_gamepad_flush_pending_left_stick(flush_ns, false);
            sdl_gamepad_flush_pending_shoulder(flush_ns, false);

            /* Avoid pegging a CPU core when we're repeatedly asked to poll */
            if (!handled)
                SDL_Delay(1);
        }
        return 0;
    }
    case TERM_XTRA_FLUSH:
        // Flush pending input events (drain queue)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                sdl_handle_event(&g_state, &ev);
        }
        return 0;
    case TERM_XTRA_CLEAR:
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
        g_state.need_present = true;
        return 0;
    case TERM_XTRA_FRESH:
        if (g_state.need_present) {
            SDL_SetRenderTarget(g_state.renderer, NULL);
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_state.renderer);
            // Render all view canvases to the window
            int active_views = 0;
            for (int i = 0; i < MAX_TERM_DATA; i++) {
                if (g_views[i].canvas)
                    active_views++;
            }
            for (int i = 0; i < MAX_TERM_DATA; i++) {
                sdl_view* view = &g_views[i];
                if (!view->canvas)
                    continue;
                // rect and margin are already in window coordinates, no scaling needed
                SDL_RenderTexture(g_state.renderer, view->canvas, NULL, &(SDL_FRect){
                    .x = view->rect.x + view->margin_x,
                    .y = view->rect.y + view->margin_y,
                    .w = view->canvas->w,
                    .h = view->canvas->h,
                });
                if (active_views > 1) {
                    SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 128);
                    SDL_FRect frame = {
                        .x = view->rect.x,
                        .y = view->rect.y,
                        .w = view->rect.w,
                        .h = view->rect.h,
                    };
                    SDL_RenderRect(g_state.renderer, &frame);
                }
            }
            SDL_RenderPresent(g_state.renderer);
            SDL_FlushRenderer(g_state.renderer);
            SDL_SetRenderTarget(g_state.renderer, d->canvas);
            g_state.need_present = false;
        }
        return 0;
    case TERM_XTRA_DELAY: {
        /* Break delay into chunks and process events to keep app responsive */
        Uint32 total_delay = (Uint32)v;
        Uint32 chunk = 20; /* Process events every 20ms */
        
        while (total_delay > 0) {
            Uint32 this_delay = (total_delay < chunk) ? total_delay : chunk;
            SDL_Delay(this_delay);
            total_delay -= this_delay;
            
            /* Update music streams */
            sdl_music_update();
            
            /* Process pending events to prevent "Not Responding" status */
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                sdl_handle_event(&g_state, &ev);
            }
        }
        return 0;
    }
    case TERM_XTRA_REACT:
        /* React to global setting changes (graphics mode, colors, etc.) */
        log_debug("TERM_XTRA_REACT received (tiles_mode=%d use_graphics=%d arg_graphics=%d)",
                  g_state.use_tiles, use_graphics, arg_graphics);
        /* Reload colors from angband_color_table (may have been changed by .prf files) */
        sdl_sync_palette();
        reset_visuals(true);
        return 0;
    default:
        return 0;
    }
}

static void draw_cursor(int x, int y, bool big)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d->canvas)
        return;
    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderDrawColor(g_state.renderer, 0, 255, 255, 255);
    SDL_RenderRect(g_state.renderer, &r);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
}

static errr callback_sdl_curs(int x, int y)
{
    draw_cursor(x, y, false);
    return 0;
}

static errr callback_sdl_bigcurs(int x, int y)
{
    draw_cursor(x, y, true);
    return 0;
}

static errr callback_sdl_wipe(int x, int y, int n)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d->canvas)
        return 0;
    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(g_state.renderer, &r);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
    return 0;
}

static errr callback_sdl_text(int x, int y, int n, byte a, cptr s)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas)
        return 0;
    SDL_SetRenderTarget(g_state.renderer, d->canvas);

    TTF_Font* story_font = sdl_story_font_for_view(d);

    // Check if any character in this chunk should use story font
    // First check the global chunk flag (for whole-line story rendering)
    bool chunk_story_font = (Term && Term->story_chunk_active && story_font);
    
    // Also check per-character story font flags
    if (!chunk_story_font && Term && Term->scr && story_font) {
        // Check if ANY character in this chunk (from x to x+n) has the story font flag
        // story is a byte** (2D array), so we need story[y] which gives us byte* for that row
        if (y >= 0 && y < Term->hgt && Term->scr->story && Term->scr->story[y]) {
            // Check all characters in the chunk, not just the first one
            for (int i = 0; i < n && (x + i) < Term->wid; i++) {
                if (Term->scr->story[y][x + i]) {
                    chunk_story_font = true;
                    log_debug("callback_sdl_text: Using story font based on per-char flag at y=%d x=%d (chunk starts at x=%d)", 
                              y, x + i, x);
                    break;
                }
            }
        }
    }
    
    bool story_mode = (chunk_story_font && story_font);

    if (!story_mode) {
        // Clear destination cell span so shorter/narrower glyphs don't leave leftovers
        SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
        SDL_SetRenderClipRect(g_state.renderer, &clip);
        SDL_FRect bg = {
            (float)(x * d->cell_w),
            (float)(y * d->cell_h),
            (float)(n * d->cell_w),
            (float)(d->cell_h)
        };
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(g_state.renderer, &bg);
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    } else {
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    /* Use extended color table to support shaded colors (indices 0-255) */
    SDL_Color col;
    col.r = angband_color_table[a][1];
    col.g = angband_color_table[a][2];
    col.b = angband_color_table[a][3];
    col.a = 255;
    
    // Special logging for line 0 (top description line in unified look)
    if (y == 0) {
        log_debug("callback_sdl_text ROW 0: x=%d n=%d chunk_story=%d text='%.*s'", 
                  x, n, chunk_story_font, n, s);
    }
    
    // Special logging for the shooting row (y=1 when 0-indexed, or the second row)
    if (y == 1 || y == 2) {
        log_debug("callback_sdl_text ROW %d: chunk_story=%d chunk_active=%d",
                  y, chunk_story_font,
                  (Term && Term->story_chunk_active) ? 1 : 0);
    }
    
    log_trace("callback_sdl_text: chunk_story_font=%s term=%p chunk_flag=%s depth=%d font=%p",
              chunk_story_font ? "true" : "false",
              (void*)Term,
              (Term && Term->story_chunk_active) ? "true" : "false",
              g_state.story_font_depth,
              (void*)story_font);

    byte* story_row = NULL;
    char* row_chars = NULL;
    byte* row_attr = NULL;
    if (Term && Term->scr && y >= 0 && y < Term->hgt) {
        if (Term->scr->story)
            story_row = Term->scr->story[y];
        if (Term->scr->c)
            row_chars = Term->scr->c[y];
        if (Term->scr->a)
            row_attr = Term->scr->a[y];
    }

    if (story_mode) {
        if (story_row) {
            /* Story "free" text must be pixel-packed across color runs.
             * Rendering per-cell or per-run leaves large gaps with proportional fonts. */
            if (row_chars && row_attr)
            {
                sdl_render_story_row_packed(d, story_font, y, story_row, row_chars, row_attr);
                g_state.need_present = true;
                return 0;
            }

            int offset = 0;
            while (offset < n && (x + offset) < Term->wid) {
                int term_col = x + offset;
                byte flags = story_row[term_col];
                bool use_story = (flags & STORY_FLAG_USE) != 0;
                bool grid_align = (flags & STORY_FLAG_CELL_ALIGN) != 0;

                int chunk_remaining = n - offset;
                int chunk_run = 1;
                while ((chunk_run < chunk_remaining) && (term_col + chunk_run) < Term->wid) {
                    byte next_flags = story_row[term_col + chunk_run];
                    bool next_story = (next_flags & STORY_FLAG_USE) != 0;
                    bool next_grid = (next_flags & STORY_FLAG_CELL_ALIGN) != 0;
                    if (next_story != use_story)
                        break;
                    if (next_grid != grid_align)
                        break;
                    if (row_attr && row_attr[term_col + chunk_run] != a)
                        break;
                    chunk_run++;
                }

                bool can_extend_story = use_story && row_chars;
                int render_col = term_col;
                int render_end = term_col + chunk_run;

                if (can_extend_story) {
                    while (render_col > 0) {
                        byte prev_flags = story_row[render_col - 1];
                        bool prev_story = (prev_flags & STORY_FLAG_USE) != 0;
                        bool prev_grid = (prev_flags & STORY_FLAG_CELL_ALIGN) != 0;
                        if (!prev_story || prev_grid != grid_align)
                            break;
                        if (row_attr && row_attr[render_col - 1] != a)
                            break;
                        render_col--;
                    }
                    while (render_end < Term->wid) {
                        byte next_flags = story_row[render_end];
                        bool next_story = (next_flags & STORY_FLAG_USE) != 0;
                        bool next_grid = (next_flags & STORY_FLAG_CELL_ALIGN) != 0;
                        if (!next_story || next_grid != grid_align)
                            break;
                        if (row_attr && row_attr[render_end] != a)
                            break;
                        render_end++;
                    }
                }

                int render_run = render_end - render_col;
                const char* render_text = (can_extend_story && row_chars) ? (row_chars + render_col) : (s + offset);

                SDL_FRect clear_rect = {
                    (float)(render_col * d->cell_w),
                    (float)(y * d->cell_h),
                    (float)(render_run * d->cell_w),
                    (float)d->cell_h
                };
                SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
                SDL_RenderFillRect(g_state.renderer, &clear_rect);

                if (use_story) {
                    if (grid_align)
                        sdl_render_story_text_grid(d, story_font, render_col, y, render_run, render_text, col);
                    else
                        sdl_render_story_text_free(d, story_font, render_col, y, render_run, render_text, col);
                } else {
                    sdl_render_mono_text(d, render_col, y, render_run, render_text, col);
                }

                offset += chunk_run;
            }
        } else {
            SDL_FRect clear_rect = {
                (float)(x * d->cell_w),
                (float)(y * d->cell_h),
                (float)(n * d->cell_w),
                (float)d->cell_h
            };
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(g_state.renderer, &clear_rect);
            sdl_render_story_text_free(d, story_font, x, y, n, s, col);
        }
    } else {
        if (y == 1 || y == 2) {
            log_debug("callback_sdl_text: USING MONO FONT for row %d: '%.30s'", y, s);
        }
        sdl_render_mono_text(d, x, y, n, s, col);
    }

    g_state.need_present = true;
    return 0;
}

static errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp,
                       const byte* tap, const char* tcp)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas)
        return 0;
    log_trace("sdl3_pict stripe start: y=%d x=%d n=%d", y, x, n);

    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_SetRenderClipRect(g_state.renderer, &(SDL_Rect){
        x * d->cell_w,
        y * d->cell_h,
        n * d->cell_w * (use_bigtile + 1),
        d->cell_h,
    });

    SDL_FRect src = {
        .w = TILE_SIZE,
        .h = TILE_SIZE,
    };

    SDL_FRect dst = {
        x * d->cell_w,
        y * d->cell_h,
        d->cell_w * (use_bigtile + 1),
        d->cell_h,
    };

    for (int i = 0; i < n; ++i, dst.x += dst.w) {
        byte a = ap[i];
        char c = cp[i];

        bool glow = a & GRAPHICS_GLOW_MASK;
        bool alert = c & GRAPHICS_ALERT_MASK;

        /* Unconditionally clear the full (possibly 2-cell) destination area to avoid ghosting */
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(g_state.renderer, &dst);

        /* Draw terrain underlay ALWAYS */
        src.x = (tcp[i] & 0x3F) * TILE_SIZE;
        src.y = (tap[i] & 0x3F) * TILE_SIZE;
        SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);

        /* Overlays (glow / alert) */
        if (glow) {
            src.x = (0x7F & misc_to_char[ICON_GLOW]) * TILE_SIZE;
            src.y = (0x7F & misc_to_attr[ICON_GLOW]) * TILE_SIZE;
            SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
        }

        /* Draw base tile */
        src.x = (c & 0x3F) * TILE_SIZE;
        src.y = (a & 0x3F) * TILE_SIZE;
        SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);

        if (alert) {
            src.x = (0x7F & misc_to_char[ICON_ALERT]) * TILE_SIZE;
            src.y = (0x7F & misc_to_attr[ICON_ALERT]) * TILE_SIZE;
            SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
    return 0;
}

static void callback_sdl_nuke() {
    log_debug("sdl3_term_nuke");
    sdl_view* d = sdl_view_from_term(Term);
    if (!d)
        return;

    // if (d->font) {
    //     TTF_CloseFont(d->font);
    //     d->font = NULL;
    // }
    // for (int i = 0; i < 256; ++i) {
    //     if (d->glyph_cache[i]) {
    //         SDL_DestroyTexture(d->glyph_cache[i]);
    //         d->glyph_cache[i] = NULL;
    //     }
    // }
    if (d->font_atlas)
        SDL_DestroyTexture(d->font_atlas);
    // if (d->tileset)
    //     SDL_DestroyTexture(d->tileset);
    if (d->canvas)
        SDL_DestroyTexture(d->canvas);
    // if (d->renderer)
    //     SDL_DestroyRenderer(d->renderer);
    // if (d->window)
    //     SDL_DestroyWindow(d->window);
}

static void callback_sdl_init(term* t)
{
    (void)t;
}

static errr sdl_view_link_term(sdl_view* d, int term_index)
{
    term* t = &d->t;
    if (d->term_ready) {
        term* old = Term;
        Term_activate(t);
        Term_resize(d->cols, d->rows);
        Term_redraw();
        Term_activate(old);
        return 0;
    }
    term_init(t, d->cols, d->rows, 256);
    t->soft_cursor = true;
    t->higher_pict = g_state.use_tiles;
    t->never_frosh = true;
    t->init_hook = callback_sdl_init;
    t->nuke_hook = callback_sdl_nuke;
    t->xtra_hook = callback_sdl_xtra;
    t->curs_hook = callback_sdl_curs;
    t->bigcurs_hook = callback_sdl_bigcurs;
    t->wipe_hook = callback_sdl_wipe;
    t->text_hook = callback_sdl_text;
    if (g_state.use_tiles)
        t->pict_hook = callback_sdl_pict;
    size_t* view_index = (size_t*)&t->data;
    *view_index = term_index;
    angband_term[term_index] = t;
    d->term_ready = true;
    return 0;
}

// Helper to apply font rendering settings to a TTF_Font
static void sdl_apply_font_settings(TTF_Font* font, bool is_story_font)
{
    // Select settings based on font type
    bool bold = is_story_font ? config.story_bold : config.mono_bold;
    bool italic = is_story_font ? config.story_italic : config.mono_italic;
    bool underline = is_story_font ? config.story_underline : config.mono_underline;
    bool strikethrough = is_story_font ? config.story_strikethrough : config.mono_strikethrough;
    int hinting = is_story_font ? config.story_hinting : config.mono_hinting;
    bool kerning = is_story_font ? config.story_kerning : config.mono_kerning;
    int outline = is_story_font ? config.story_outline : config.mono_outline;
    
    // Apply font style settings
    int style = TTF_STYLE_NORMAL;
    if (bold) style |= TTF_STYLE_BOLD;
    if (italic) style |= TTF_STYLE_ITALIC;
    if (underline) style |= TTF_STYLE_UNDERLINE;
    if (strikethrough) style |= TTF_STYLE_STRIKETHROUGH;
    if (style != TTF_STYLE_NORMAL) {
        TTF_SetFontStyle(font, style);
        log_debug("Applied %s font style: %d (bold=%d, italic=%d, underline=%d, strikethrough=%d)",
                 is_story_font ? "story" : "mono", style, bold, italic, underline, strikethrough);
    }
    
    // Apply hinting
    TTF_SetFontHinting(font, hinting);
    log_debug("Applied %s font hinting: %d", is_story_font ? "story" : "mono", hinting);
    
    // Apply kerning
    TTF_SetFontKerning(font, kerning);
    
    // Apply outline
    if (outline > 0) {
        TTF_SetFontOutline(font, outline);
        log_debug("Applied %s font outline: %d", is_story_font ? "story" : "mono", outline);
    }
}

static void sdl_story_font_cache_clear(void)
{
    for (int i = 0; i < g_state.story_font_count; i++) {
        if (g_state.story_fonts[i].font) {
            TTF_CloseFont(g_state.story_fonts[i].font);
            g_state.story_fonts[i].font = NULL;
        }
        g_state.story_fonts[i].pixel_height = 0;
    }
    g_state.story_font_count = 0;
}

static TTF_Font* sdl_story_font_for_height(int pixel_height)
{
    if (pixel_height <= 0)
        return NULL;

    for (int i = 0; i < g_state.story_font_count; i++) {
        if (g_state.story_fonts[i].pixel_height == pixel_height)
            return g_state.story_fonts[i].font;
    }

    if (g_state.story_font_count >= MAX_STORY_FONT_CACHE) {
        log_warn("Story font cache full; reusing size %d", g_state.story_fonts[0].pixel_height);
        return g_state.story_fonts[0].font;
    }

    const char* font_path = (config.story_font[0] != '\0') ? config.story_font : NULL;
    TTF_Font* font = sdl_load_font_with_fallback(font_path, pixel_height, sdl_story_fallback_font);
    if (!font)
        return NULL;

    g_state.story_fonts[g_state.story_font_count].pixel_height = pixel_height;
    g_state.story_fonts[g_state.story_font_count].font = font;
    g_state.story_font_count++;
    return font;
}

static TTF_Font* sdl_story_font_for_view(const sdl_view* d)
{
    if (!d)
        return NULL;
    return sdl_story_font_for_height(d->cell_h);
}

// Loads TTF font with given size. Attempts to fit the font into a cell assming
// 1:2 aspect ratio. The font size is expected to take into account any scaling,
// either HiDPI or user. So on a HiDPI screen to use font size 12, this function
// would expect 24 given scaling factor of 2.0.
static SDL_Texture* sdl_load_ttf_font(const char* font_path, int font_size, int* actual_font_size)
{
    int cell_height = font_size;
    int cell_width = font_size / 2;
    int min_size = font_size / 2;
    TTF_Font* font = NULL;
    for (; font_size >= min_size; font_size--) {
        log_trace("trying TTF font size %d", font_size);
        if (font == NULL) {
            font = TTF_OpenFont(font_path, font_size);
            if (!font) {
                log_error("TTF_OpenFont failed: %s", SDL_GetError());
                quit("could not load TTF font");
            }
            
            // Apply monospace font settings
            sdl_apply_font_settings(font, false);
        }
        int measured_w = 0;
        TTF_MeasureString(font, "M", 1, 0, &measured_w, NULL);
        log_trace("font size %d, measured_w %d", font_size, measured_w);
        if (measured_w <= cell_width) {
            log_debug("chose TTF font size %d, em width %d", font_size, measured_w);
            break;
        }
        TTF_CloseFont(font);
        font = NULL;
    }
    if (!font) {
        log_error("could not find suitable font size");
        quit("could not find suitable font size");
    }
    // Build TTF font atlas.
    SDL_Texture* font_atlas = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 16 * cell_width, 16 * cell_height);
    if (!font_atlas) {
        log_error("SDL_CreateTexture failed: %s", SDL_GetError());
        quit("could not create TTF glyph cache");
    }
    SDL_SetRenderTarget(g_state.renderer, font_atlas);
    SDL_Color white = (SDL_Color){255, 255, 255, 255};
    SDL_FRect dst = {
        .w = cell_width,
        .h = cell_height,
    };
    for (Uint32 ch = 0; ch < 256; ch++) {
        SDL_Surface* gsurf = TTF_RenderGlyph_Blended(font, ch, white);
        if (!gsurf) {
            // Dumb method of comparing errors using string comparison.
            // Apparently SDL doesn't have error codes, only this.
            const char* error = SDL_GetError();
            if (!SDL_strcmp(error, "Text has zero width")) {
                continue;
            }
            log_error("could not render `%c` character: %s", ch, error);
            quit("could not render TTF character");
        }
        SDL_Texture* gtex = SDL_CreateTextureFromSurface(g_state.renderer, gsurf);
        SDL_DestroySurface(gsurf);
        if (!gtex) {
            log_error("prepare_glyph: could not create texture from surface: %s", SDL_GetError());
            quit("could not create SDL texture");
        }
        SDL_SetTextureBlendMode(gtex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(gtex, SDL_SCALEMODE_LINEAR);
        dst.x = cell_width * (ch % 16);
        dst.y = cell_height * (ch >> 4);
        SDL_RenderTexture(g_state.renderer, gtex, NULL, &dst);
    }
    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetTextureScaleMode(font_atlas, SDL_SCALEMODE_LINEAR);
    TTF_CloseFont(font);
    if (actual_font_size)
        *actual_font_size = font_size;
    return font_atlas;
}

static void sdl_window_set_position(int x, int y)
{
    if (g_state.window && x >= 0 && y >= 0) {
        SDL_SetWindowPosition(g_state.window, x, y);
        log_debug("Window position set to (%d, %d)", x, y);
    }
}

static void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles)
{
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    if (!SDL_CreateWindowAndRenderer("Sil-more SDL3", window_width, window_height,
            flags, &g_state.window, &g_state.renderer))
    {
        log_error("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        quit("could not create SDL window");
    }

    if (fullscreen)
        SDL_HideCursor();

    g_state.system_scale = SDL_GetWindowDisplayScale(g_state.window);
    log_debug("window scale is %g", g_state.system_scale);

    // Ensure predictable alpha blending (cursor/text)
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    g_state.use_tiles = use_tiles;
    if (g_state.use_tiles) {
        log_debug("preparing tileset");
        g_state.use_tiles = true;
        // d->tile_w = 2 * GLYPH_WIDTH;
        // d->tile_h = GLYPH_HEIGHT;
        SDL_Surface* ts = IMG_Load("lib/xtra/graf/16x16.png");
        if (ts) {
            log_debug("tileset loaded");
            g_state.tileset = SDL_CreateTextureFromSurface(g_state.renderer, ts);
            SDL_DestroySurface(ts);
            if (!g_state.tileset) {
                log_error("Failed to create tileset texture: %s", SDL_GetError());
                quit("could not create tileset texture");
            } else {
                SDL_SetTextureScaleMode(g_state.tileset, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
                g_state.tileset_cols = ts->w / TILE_SIZE;
            }
        } else {
            log_error("Failed to load tileset PNG: %s", SDL_GetError());
            quit("could not load tileset");
        }
    }
}

static void sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin)
{
    log_debug("view rect=(%d %d %d %d)", rect.x, rect.y, rect.w, rect.h);

    if (scale) {
        // Integer scaling mode.
        d->cell_w = scale * TILE_SIZE / 2;
        d->cell_h = scale * TILE_SIZE;
    } else if (font_size) {
        // Non-integer scaling mode.
        d->cell_h = g_state.system_scale * font_size;
        d->cell_w = d->cell_h / 2;
    } else {
        quit("sdl_view_create: font_size and scale cannot both be zero");
    }

    d->font_atlas = sdl_load_ttf_font(font_path, d->cell_h, NULL);
    SDL_SetTextureBlendMode(d->font_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(d->font_atlas, 255, 255, 255);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    d->rect = rect;
    d->cols = rect.w / d->cell_w;
    d->rows = rect.h / d->cell_h;
    d->margin_x = (rect.w - d->cols * d->cell_w) / 2;
    if (d->margin_x < margin)
        d->margin_x = margin;
    d->margin_y = (rect.h - d->rows * d->cell_h) / 2;
    if (d->margin_y < margin)
        d->margin_y = margin;
    log_debug("view cols=%d rows=%d cell=(%d, %d) margin=(%d, %d)",
        d->cols, d->rows, d->cell_w, d->cell_h,
        d->margin_x, d->margin_y);

    // Create a persistent offscreen canvas to render into.
    d->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_TARGET,
                                  d->cols * d->cell_w, d->rows * d->cell_h);
    if (d->canvas) {
        log_debug("view canvas %dx%d", d->canvas->w, d->canvas->h);
        SDL_SetTextureBlendMode(d->canvas, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(d->canvas, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
        g_state.need_present = true;
    } else {
        log_error("Create canvas failed: %s", SDL_GetError());
        quit("could not create canvas");
    }
}

/*
 * Load a TTF font with fallback to default if not found
 */
static TTF_Font* sdl_load_font_with_fallback(const char* font_path, int font_size, const char* fallback_path)
{
    TTF_Font* font = NULL;
    
    // Try to load the specified font if provided
    if (font_path && font_path[0] != '\0') {
        font = TTF_OpenFont(font_path, font_size);
        if (font) {
            log_debug("Loaded custom font: %s at size %d", font_path, font_size);
            // Apply story font settings
            sdl_apply_font_settings(font, true);
            return font;
        } else {
            log_warn("Failed to load custom font '%s': %s", font_path, SDL_GetError());
        }
    }
    
    // Fall back to default font
    font = TTF_OpenFont(fallback_path, font_size);
    if (font) {
        log_debug("Using fallback font: %s at size %d", fallback_path, font_size);
        // Apply story font settings to fallback too
        sdl_apply_font_settings(font, true);
    } else {
        log_error("Failed to load fallback font '%s': %s", fallback_path, SDL_GetError());
    }
    
    return font;
}

/*
 * Load story fonts from configuration for main/aux view sizes.
 * If no story font is set in the SDL settings (or the SDL JSON is missing),
 * fall back to MarcellusSC-Regular.ttf located in lib/xtra/font.
 */
static void sdl_load_story_fonts(void)
{
    int main_cell_h = config.main_view_scale * TILE_SIZE;
    int aux_cell_h = (config.aux_view_font_size > 0)
        ? (int)(g_state.system_scale * config.aux_view_font_size)
        : main_cell_h;
    
    log_info("Loading story fonts...");
    log_debug("Story font config: '%s'", config.story_font[0] != '\0' ? config.story_font : "(not set)");
    log_debug("Story font sizes: main=%d aux=%d", main_cell_h, aux_cell_h);
    
    sdl_story_font_cache_clear();
    (void)sdl_story_font_for_height(main_cell_h);
    (void)sdl_story_font_for_height(aux_cell_h);
    
    // Initialize flag to false
    g_state.story_font_depth = 0;
    if (Term) Term->story_font_active = false;
    
    log_info("Story fonts loaded successfully");
}

/*
 * Enable story font mode - subsequent text will use custom font
 */
void sdl_story_font_enable(void)
{
    g_state.story_font_depth++;
    if (g_state.story_font_depth == 1)
        sdl_apply_story_font_state(true);
    log_debug("Story font ENABLED (depth=%d)", g_state.story_font_depth);
}

/*
 * Disable story font mode - subsequent text will use monospace font
 */
void sdl_story_font_disable(void)
{
    if (g_state.story_font_depth > 0)
        g_state.story_font_depth--;
    bool active = (g_state.story_font_depth > 0);
    sdl_apply_story_font_state(active);
    if (!active)
        sdl_story_font_set_grid(false);
    log_debug("Story font DISABLED (depth=%d)", g_state.story_font_depth);
}

/*
 * Check if story font is currently enabled
 */
bool sdl_is_story_font_enabled(void)
{
    return (Term && Term->story_font_active);
}

void sdl_story_font_set_grid(bool grid)
{
    if (g_state.story_font_grid == grid)
        return;
    g_state.story_font_grid = grid;
    log_trace("Story font grid %s", grid ? "ENABLED" : "DISABLED");
    sdl_apply_story_grid_state(grid);
}

bool sdl_is_story_font_grid(void)
{
    return (Term && Term->story_font_grid);
}

void sdl_story_font_reset(void)
{
    sdl_story_font_reset_state();
}

/*
 * Get the pixel width of text when rendered with the story font.
 * Returns 0 if story font is not available.
 * IMPORTANT: This returns the width AFTER scaling to match cell height.
 */
int sdl_story_font_text_width(cptr text, int len)
{
    if (!text)
        return 0;

    sdl_view* d = NULL;
    if (Term)
        d = sdl_view_from_term(Term);
    if (!d || !d->term_ready) {
        if (g_views[0].term_ready)
            d = &g_views[0];
    }
    if (!d)
        return 0;

    TTF_Font* font = sdl_story_font_for_view(d);
    if (!font)
        return 0;

    /* Measure the text width using SDL_ttf (unscaled) */
    int w = 0;
    TTF_MeasureString(font, text, len, 0, &w, NULL);

    /* Apply the same scaling that's used when rendering */
    int font_h = TTF_GetFontHeight(font);
    if (font_h > 0) {
        float cell_h_f = (float)d->cell_h;
        float surf_h_f = (float)font_h;
        float scale = cell_h_f / surf_h_f;
        w = (int)((float)w * scale);
    }

    return w;
}

/*
 * Get the cell width in pixels for the main terminal view.
 * This is used to convert terminal columns to pixel width.
 */
int sdl_get_cell_width(void)
{
    if (g_views[0].term_ready) {
        return g_views[0].cell_w;
    }
    return 8; /* fallback */
}

// Quit hook to save window configuration on exit
static void sdl_quit_hook(cptr str)
{
    (void)str; // Unused parameter
    
    // Shut down audio before tearing down SDL
    sdl_sound_shutdown();

    // Close any open gamepads
    sdl_gamepad_shutdown();
    
    // Clean up story fonts
    sdl_story_font_cache_clear();
    
    // Only save if we have a valid window and config file path
    if (g_state.window && config_file_path[0] != '\0') {
        // Get current window position and size if not in fullscreen
        if (!config.fullscreen) {
            SDL_GetWindowPosition(g_state.window, &config.window_x, &config.window_y);
            SDL_GetWindowSize(g_state.window, &config.window_width, &config.window_height);
            log_debug("Saving window position (%d, %d) and size (%dx%d)",
                     config.window_x, config.window_y, config.window_width, config.window_height);
        }
        
        // Save configuration
        sdl_config_save(config_file_path, &config, pane_config, pane_config_count);
    }
}


errr init_sdl(int argc, char **argv)
{
    log_debug("init_sdl starting");
    
    // Initialize SDL first to get display information
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        quit("could not init SDL");
    }
    if (!TTF_Init()) {
        log_error("TTF_Init failed: %s", SDL_GetError());
        quit("could not init TTF");
    }
    
    // Get primary display information
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (!primary) {
        log_error("SDL_GetPrimaryDisplay failed: %s", SDL_GetError());
        quit("could not get primary display ID");
    }
    
    // Get display bounds for window sizing (uses logical coordinates)
    SDL_Rect screen;
    if (!SDL_GetDisplayBounds(primary, &screen)) {
        log_error("SDL_GetDisplayBounds failed: %s", SDL_GetError());
        quit("could not get primary display bounds");
    }
    log_info("primary display bounds (logical): %dx%d at (%d,%d)",
             screen.w, screen.h, screen.x, screen.y);
    
    // Get the desktop display mode - this contains the pixel_density field we need
    const SDL_DisplayMode* desktop_mode = SDL_GetDesktopDisplayMode(primary);
    if (!desktop_mode) {
        log_error("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
        quit("could not get desktop display mode");
    }
    
    // SDL_DisplayMode contains:
    // - w, h: logical resolution (points on macOS, pixels on Windows/Linux without scaling)
    // - pixel_density: scale factor (e.g., 2.0 on Retina displays, 1.0 otherwise)
    // Physical resolution = logical × pixel_density
    float pixel_density = desktop_mode->pixel_density;
    
    // Calculate physical pixel dimensions for resolution profile matching
    // On macOS Retina: 1440×900 logical × 2.0 density = 2560×1600 physical
    // On Windows/Linux (no scaling): 1920×1080 logical × 1.0 density = 1920×1080 physical
    int screen_pixels_w = (int)(desktop_mode->w * pixel_density + 0.5f);
    int screen_pixels_h = (int)(desktop_mode->h * pixel_density + 0.5f);
    
    log_info("primary display desktop mode: %dx%d @%.2fHz, pixel_density=%.2f",
             desktop_mode->w, desktop_mode->h, desktop_mode->refresh_rate, pixel_density);
    log_info("primary display physical resolution for defaults: %dx%d",
             screen_pixels_w, screen_pixels_h);
    
    // Save config file path for later use on exit
    char config_file[1024];
    if (ANGBAND_DIR_USER && ANGBAND_DIR_USER[0])
        path_build(config_file, sizeof(config_file), ANGBAND_DIR_USER, "sil_sdl.json");
    else
        SDL_strlcpy(config_file, "sil_sdl.json", sizeof(config_file));
    SDL_strlcpy(config_file_path, config_file, sizeof(config_file_path));
    
    // Register quit hook to save configuration on exit
    log_register_quit_hook(sdl_quit_hook);
    
    // Check if config file exists
    bool config_exists = SDL_GetPathInfo(config_file_path, NULL);

    if (config_exists) {
        // Config file exists - use generic defaults first, then load from file
        log_debug("Config file exists, loading from: %s", config_file_path);
        sdl_config_set_defaults(&config);
        
        // Copy default pane configuration
        pane_config_count = default_pane_config_count;
        for (int i = 0; i < default_pane_config_count && i < MAX_PANE_CONFIGS; i++) {
            pane_config[i] = default_pane_config[i];
        }
        
        sdl_config_load(config_file_path, &config, pane_config, &pane_config_count, MAX_PANE_CONFIGS);
        
        // Load sound configuration from sound.json
        // For local builds: read from lib/pref (ANGBAND_DIR_PREF)
        // For standard builds: read from user folder (ANGBAND_DIR_USER)
        char sound_config_path[1024];
#ifdef SIL_USE_LOCAL_DATA
        if (ANGBAND_DIR_PREF && ANGBAND_DIR_PREF[0])
            path_build(sound_config_path, sizeof(sound_config_path), ANGBAND_DIR_PREF, "sound.json");
        else
            SDL_strlcpy(sound_config_path, "sound.json", sizeof(sound_config_path));
#else
        if (ANGBAND_DIR_USER && ANGBAND_DIR_USER[0])
            path_build(sound_config_path, sizeof(sound_config_path), ANGBAND_DIR_USER, "sound.json");
        else
            SDL_strlcpy(sound_config_path, "sound.json", sizeof(sound_config_path));
#endif
        sound_config_load(sound_config_path, &g_sound_config);
        
        // Apply sound setting to global variable
        use_sound = g_sound_config.enabled;
        
        log_debug("After loading JSON: scale=%d, font=%d, margin=%d, fullscreen=%d, tiles=%d, sound=%d",
                  config.main_view_scale, config.aux_view_font_size, config.margin,
                  config.fullscreen, config.tiles, g_sound_config.enabled);
    } else {
        // Config file doesn't exist - use resolution-based defaults
        log_debug("Config file not found, using resolution-based defaults");
        sdl_config_set_defaults_for_resolution(&config, pane_config, &pane_config_count,
                                               MAX_PANE_CONFIGS, screen_pixels_w, screen_pixels_h);
        
        // If no resolution-specific config was found, use default pane config
        if (pane_config_count == 0) {
            pane_config_count = default_pane_config_count;
            for (int i = 0; i < default_pane_config_count && i < MAX_PANE_CONFIGS; i++) {
                pane_config[i] = default_pane_config[i];
            }
        }
        
        log_debug("After resolution defaults: scale=%d, font=%d, margin=%d, fullscreen=%d, tiles=%d",
                  config.main_view_scale, config.aux_view_font_size, config.margin,
                  config.fullscreen, config.tiles);
    }
    
    // Apply command-line overrides
    sdl_config_apply_cmdline(&config, argc, argv);
    log_debug("After command-line: scale=%d, font=%d, margin=%d, fullscreen=%d, tiles=%d",
              config.main_view_scale, config.aux_view_font_size, config.margin,
              config.fullscreen, config.tiles);
    
    // Validate configuration
    if (config.main_view_scale <= 0) {
        log_warn("Invalid main_view_scale %d, using 1", config.main_view_scale);
        config.main_view_scale = 1;
    }
    if (config.aux_view_font_size <= 0) {
        log_warn("Invalid aux_view_font_size %d, using 18", config.aux_view_font_size);
        config.aux_view_font_size = 18;
    }
    if (config.margin < 0) {
        log_warn("Invalid margin %d, using 0", config.margin);
        config.margin = 0;
    }
    if (config.gamepad_deadzone < 0) {
        log_warn("Invalid gamepad_deadzone %d, using 0", config.gamepad_deadzone);
        config.gamepad_deadzone = 0;
    } else if (config.gamepad_deadzone > SDL_JOYSTICK_AXIS_MAX) {
        log_warn("Invalid gamepad_deadzone %d, clamping to %d", config.gamepad_deadzone, SDL_JOYSTICK_AXIS_MAX);
        config.gamepad_deadzone = SDL_JOYSTICK_AXIS_MAX;
    }
    if (config.gamepad_trigger_threshold < 0) {
        log_warn("Invalid gamepad_trigger_threshold %d, using 0", config.gamepad_trigger_threshold);
        config.gamepad_trigger_threshold = 0;
    } else if (config.gamepad_trigger_threshold > SDL_JOYSTICK_AXIS_MAX) {
        log_warn("Invalid gamepad_trigger_threshold %d, clamping to %d", config.gamepad_trigger_threshold,
            SDL_JOYSTICK_AXIS_MAX);
        config.gamepad_trigger_threshold = SDL_JOYSTICK_AXIS_MAX;
    }

    sdl_gamepad_init();
    
    log_info("SDL Configuration:");
    log_info("  Main view scale: %d", config.main_view_scale);
    log_info("  Aux view font size: %d", config.aux_view_font_size);
    log_info("  Margin: %d", config.margin);
    log_info("  Fullscreen: %s", config.fullscreen ? "true" : "false");
    log_info("  Tiles: %s", config.tiles ? "true" : "false");
    log_info("  Pane configurations: %d", pane_config_count);

    // Initialize palette from angband_color_table (supports .prf file customization)
    sdl_sync_palette();

    // Prepare sound registry and audio playback
    sdl_sound_reload();
    if (!sdl_sound_initialize()) {
        log_info("Sound subsystem not initialized; continuing without audio output");
    }

    // Use full display size for fullscreen, reasonable default for windowed mode
    int window_width, window_height;
    if (config.fullscreen) {
        window_width = screen.w;
        window_height = screen.h;
    } else {
        // Use saved dimensions if valid, otherwise default to 3/4 of screen size
        if (config.window_width > 0 && config.window_height > 0) {
            window_width = config.window_width;
            window_height = config.window_height;
            log_debug("Using saved window size: %dx%d", window_width, window_height);
        } else {
            window_width = screen.w * 3 / 4;
            window_height = screen.h * 3 / 4;
            log_debug("Using default window size: %dx%d", window_width, window_height);
        }
    }
    
    sdl_window_create(window_width, window_height, config.fullscreen, config.tiles);
    
    // Set window position for windowed mode
    if (!config.fullscreen && config.window_x >= 0 && config.window_y >= 0) {
        sdl_window_set_position(config.window_x, config.window_y);
    }
    
    // Load story and banner fonts
    sdl_load_story_fonts();

    ANGBAND_SYS = "sdl";
    if (config.tiles) {
        ANGBAND_GRAF = "new";
        arg_graphics = GRAPHICS_MICROCHASM;
        use_graphics = GRAPHICS_MICROCHASM;
        use_bigtile = true;
    } else {
        ANGBAND_GRAF = "old";
        arg_graphics = GRAPHICS_PSEUDO;
        use_graphics = GRAPHICS_PSEUDO;
    }

    SDL_Rect window = { 0 };
    SDL_GetWindowSizeInPixels(g_state.window, &window.w, &window.h);
    log_debug("window pixel size %dx%d", window.w, window.h);
    resize(&window);

    log_debug("init_sdl: SDL term opened (tiles_mode=%d higher_pict=%d always_pict=%d)",
            config.tiles, Term->higher_pict, Term->always_pict);
    
    return 0;
}

/*
 * Get SDL configuration info as formatted string
 * Called from cmd4.c for the pane settings menu
 */
void get_sdl_config_info(char* buf, size_t size)
{
    size_t offset = 0;
    
    // SDL settings
    offset += (size_t)strnfmt(buf + offset, size - offset, "=== SDL Settings ===\n");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Main View Scale: %d\n", config.main_view_scale);
    offset += (size_t)strnfmt(buf + offset, size - offset, "Aux View Font Size: %d\n", config.aux_view_font_size);
    offset += (size_t)strnfmt(buf + offset, size - offset, "Margin: %d\n", config.margin);
    offset += (size_t)strnfmt(buf + offset, size - offset, "Fullscreen: %s\n", config.fullscreen ? "Yes" : "No");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Tiles: %s\n\n", config.tiles ? "Yes" : "No");
    
    // Pane configurations
    offset += (size_t)strnfmt(buf + offset, size - offset, "=== Pane Configuration ===\n");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Total Panes: %d\n\n", pane_config_count);
    
    for (int i = 0; i < pane_config_count && i < MAX_PANE_CONFIGS; i++) {
        const struct pane_config* pc = &pane_config[i];
        const char* type_str = "UNKNOWN";
        const char* where_str = (pc->where == PLACE_BOTTOM) ? "BOTTOM" : "RIGHT";
        
        switch (pc->pane) {
            case PANE_MAIN: type_str = "MAIN"; break;
            case PANE_INVENTORY: type_str = "INVENTORY"; break;
            case PANE_WORN: type_str = "WORN"; break;
            case PANE_ROLLS: type_str = "ROLLS"; break;
            case PANE_INFO: type_str = "INFO"; break;
            case PANE_CHARACTER: type_str = "CHARACTER"; break;
            case PANE_LOG: type_str = "LOG"; break;
            case PANE_MONSTERS: type_str = "MONSTERS"; break;
            default: break;
        }
        
        offset += (size_t)strnfmt(buf + offset, size - offset, "Pane %d: %s\n", i + 1, type_str);
        offset += (size_t)strnfmt(buf + offset, size - offset, "  Placement: %s\n", where_str);
        if (pc->rect.rows > 0)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Rows: %d\n", pc->rect.rows);
        if (pc->rect.cols > 0)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Cols: %d\n", pc->rect.cols);
        if (pc->ratio > 0.0f)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Ratio: %.2f\n", pc->ratio);
        offset += (size_t)strnfmt(buf + offset, size - offset, "\n");
    }
    
    offset += (size_t)strnfmt(buf + offset, size - offset, "\nConfiguration file: %s\n", config_file_path);
}

/*
 * Save current pane configuration to JSON file
 * Returns TRUE on success, FALSE on failure
 */
bool save_pane_config_to_json(void)
{
    sdl_config_save(config_file_path, &config, pane_config, pane_config_count);
    log_info("Pane configuration saved to: %s", config_file_path);
    return true;
}

cptr get_sdl_config_path(void)
{
    return config_file_path;
}

/*
 * Accessor functions for SDL configuration values
 * These allow the options menu to read and modify settings
 */
int get_sdl_main_view_scale(void)
{
    return config.main_view_scale;
}

void set_sdl_main_view_scale(int value)
{
    int max_scale = get_sdl_max_scale();
    if (value > 0 && value <= max_scale)
        config.main_view_scale = value;
}

int get_sdl_aux_view_font_size(void)
{
    return config.aux_view_font_size;
}

void set_sdl_aux_view_font_size(int value)
{
    if (value >= 8 && value <= 48)
        config.aux_view_font_size = value;
}

int get_sdl_margin(void)
{
    return config.margin;
}

void set_sdl_margin(int value)
{
    if (value >= 0 && value <= 20)
        config.margin = value;
}

bool get_sdl_fullscreen(void)
{
    return config.fullscreen;
}

void set_sdl_fullscreen(bool value)
{
    config.fullscreen = value;
}

bool get_sdl_tiles(void)
{
    return config.tiles;
}

void set_sdl_tiles(bool value)
{
    config.tiles = value;
}

int get_pane_config_count(void)
{
    return pane_config_count;
}

bool get_sdl_enable_right_panes(void)
{
    return config.enable_right_panes;
}

void set_sdl_enable_right_panes(bool value)
{
    config.enable_right_panes = value;
}

bool get_sdl_enable_bottom_panes(void)
{
    return config.enable_bottom_panes;
}

void set_sdl_enable_bottom_panes(bool value)
{
    config.enable_bottom_panes = value;
}

static void sdl_gamepad_load_default_bindings(void)
{
    if (g_default_gamepad_bindings_ready)
        return;

    struct sdl_config defaults;
    sdl_config_set_defaults(&defaults);
    memcpy(g_default_gamepad_button_bindings, defaults.gamepad_button_bindings,
        sizeof(g_default_gamepad_button_bindings));
    memcpy(g_default_gamepad_trigger_bindings, defaults.gamepad_trigger_bindings,
        sizeof(g_default_gamepad_trigger_bindings));
    memcpy(g_default_gamepad_left_stick_bindings, defaults.gamepad_left_stick_bindings,
        sizeof(g_default_gamepad_left_stick_bindings));
    memcpy(g_default_gamepad_right_stick_bindings, defaults.gamepad_right_stick_bindings,
        sizeof(g_default_gamepad_right_stick_bindings));
    g_default_gamepad_shoulder_combo_binding = defaults.gamepad_shoulder_combo_binding;
    g_default_gamepad_bindings_ready = true;
}

bool steamdeck_controls_active(void)
{
    if (config.steamdeck_mode)
        return true;
    if (!config.gamepad_enabled)
        return false;
    return (config.gamepad_auto_mode && g_gamepad_auto_ui);
}

bool get_sdl_gamepad_enabled(void)
{
    return config.gamepad_enabled;
}

void set_sdl_gamepad_enabled(bool value)
{
    config.gamepad_enabled = value;
    if (!value) {
        g_gamepad_state.dpad_up = false;
        g_gamepad_state.dpad_down = false;
        g_gamepad_state.dpad_left = false;
        g_gamepad_state.dpad_right = false;
        g_gamepad_state.dpad_dir = 0;
        sdl_gamepad_clear_pending_dpad();
        g_gamepad_state.left_x = 0;
        g_gamepad_state.left_y = 0;
        g_gamepad_state.left_dir = 0;
        g_gamepad_state.left_bind_dir = -1;
        sdl_gamepad_clear_pending_left_stick();
        g_gamepad_state.right_x = 0;
        g_gamepad_state.right_y = 0;
        g_gamepad_state.right_dir = -1;
        sdl_gamepad_clear_pending_shoulder();
        g_gamepad_state.left_trigger_down = false;
        g_gamepad_state.right_trigger_down = false;
        g_gamepad_state.shift_held = 0;
        g_gamepad_state.ctrl_held = 0;
        g_gamepad_state.alt_held = 0;
    }
}

bool get_sdl_gamepad_auto_mode(void)
{
    return config.gamepad_auto_mode;
}

void set_sdl_gamepad_auto_mode(bool value)
{
    config.gamepad_auto_mode = value;
}

bool get_sdl_steamdeck_mode(void)
{
    return config.steamdeck_mode;
}

void set_sdl_steamdeck_mode(bool value)
{
    config.steamdeck_mode = value;
}

bool get_sdl_gamepad_use_dpad(void)
{
    return config.gamepad_use_dpad;
}

void set_sdl_gamepad_use_dpad(bool value)
{
    config.gamepad_use_dpad = value;
    if (value) {
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_DPAD_UP] = GAMEPAD_BIND_NONE;
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_DPAD_DOWN] = GAMEPAD_BIND_NONE;
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_DPAD_LEFT] = GAMEPAD_BIND_NONE;
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = GAMEPAD_BIND_NONE;
    } else {
        g_gamepad_state.dpad_up = false;
        g_gamepad_state.dpad_down = false;
        g_gamepad_state.dpad_left = false;
        g_gamepad_state.dpad_right = false;
        g_gamepad_state.dpad_dir = 0;
        sdl_gamepad_clear_pending_dpad();
    }
}

bool get_sdl_gamepad_use_left_stick(void)
{
    return config.gamepad_use_left_stick;
}

void set_sdl_gamepad_use_left_stick(bool value)
{
    config.gamepad_use_left_stick = value;
    if (value) {
        if (g_gamepad_state.left_bind_dir >= 0 && g_gamepad_state.left_bind_dir < GAMEPAD_STICK_DIR_COUNT) {
            int binding = config.gamepad_left_stick_bindings[g_gamepad_state.left_bind_dir];
            if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                sdl_gamepad_apply_modifier(binding, false);
            }
        }
        g_gamepad_state.left_bind_dir = -1;
        for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
            config.gamepad_left_stick_bindings[i] = GAMEPAD_BIND_NONE;
        }
    } else {
        g_gamepad_state.left_x = 0;
        g_gamepad_state.left_y = 0;
        g_gamepad_state.left_dir = 0;
        g_gamepad_state.left_bind_dir = -1;
        sdl_gamepad_clear_pending_left_stick();
    }
}

int get_sdl_gamepad_button_binding(int button)
{
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.gamepad_button_bindings[button];
}

void set_sdl_gamepad_button_binding(int button, int binding)
{
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return;
    config.gamepad_button_bindings[button] = binding;
}

int get_sdl_gamepad_trigger_binding(int index)
{
    if (index < 0 || index >= GAMEPAD_TRIGGER_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.gamepad_trigger_bindings[index];
}

void set_sdl_gamepad_trigger_binding(int index, int binding)
{
    if (index < 0 || index >= GAMEPAD_TRIGGER_COUNT)
        return;
    config.gamepad_trigger_bindings[index] = binding;
}

int get_sdl_gamepad_left_stick_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.gamepad_left_stick_bindings[dir];
}

void set_sdl_gamepad_left_stick_binding(int dir, int binding)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return;
    config.gamepad_left_stick_bindings[dir] = binding;
}

int get_sdl_gamepad_right_stick_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.gamepad_right_stick_bindings[dir];
}

void set_sdl_gamepad_right_stick_binding(int dir, int binding)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return;
    config.gamepad_right_stick_bindings[dir] = binding;
}

int get_sdl_gamepad_shoulder_combo_binding(void)
{
    return config.gamepad_shoulder_combo_binding;
}

void set_sdl_gamepad_shoulder_combo_binding(int binding)
{
    config.gamepad_shoulder_combo_binding = binding;
}

int get_sdl_gamepad_default_button_binding(int button)
{
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_button_bindings[button];
}

int get_sdl_gamepad_default_trigger_binding(int index)
{
    if (index < 0 || index >= GAMEPAD_TRIGGER_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_trigger_bindings[index];
}

int get_sdl_gamepad_default_left_stick_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_left_stick_bindings[dir];
}

int get_sdl_gamepad_default_right_stick_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_right_stick_bindings[dir];
}

int get_sdl_gamepad_default_shoulder_combo_binding(void)
{
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_shoulder_combo_binding;
}

void sdl_gamepad_reset_bindings_to_default(void)
{
    sdl_config_set_default_gamepad_bindings(&config);
}

bool sdl_gamepad_capture_begin(void)
{
    g_gamepad_capture_ready = false;
    g_gamepad_capture_active = (g_gamepad_state.pad_count > 0);
    return g_gamepad_capture_active;
}

void sdl_gamepad_capture_cancel(void)
{
    g_gamepad_capture_active = false;
    g_gamepad_capture_ready = false;
    sdl_gamepad_clear_pending_shoulder();
}

bool sdl_gamepad_capture_poll(int* out_type, int* out_id)
{
    if (!g_gamepad_capture_ready)
        return false;

    if (out_type)
        *out_type = g_gamepad_capture_type;
    if (out_id)
        *out_id = g_gamepad_capture_id;

    g_gamepad_capture_ready = false;
    g_gamepad_capture_active = false;
    sdl_gamepad_clear_pending_shoulder();
    return true;
}

/*
 * Calculate the maximum scale that allows 80x24 main view.
 * Returns the maximum scale value that should be allowed.
 */
int get_sdl_max_scale(void)
{
    if (!g_state.window) {
        return 10; // fallback if window not initialized
    }
    
    int w, h;
    SDL_GetWindowSize(g_state.window, &w, &h);
    
    // Calculate max scale based on 80 columns and 24 rows minimum
    // cell_w = scale * TILE_SIZE / 2, so scale = cell_w * 2 / TILE_SIZE
    // cell_h = scale * TILE_SIZE, so scale = cell_h / TILE_SIZE
    int max_scale_w = (w / 80) * 2 / TILE_SIZE;
    int max_scale_h = h / 24 / TILE_SIZE;
    
    int max_scale = (max_scale_w < max_scale_h) ? max_scale_w : max_scale_h;
    
    // Ensure at least 1, and cap at a reasonable maximum
    if (max_scale < 1) max_scale = 1;
    if (max_scale > 20) max_scale = 20;
    
    log_debug("get_sdl_max_scale: window=%dx%d, max_scale_w=%d, max_scale_h=%d, max_scale=%d",
              w, h, max_scale_w, max_scale_h, max_scale);
    
    return max_scale;
}

/*
 * Apply current SDL configuration by triggering a resize.
 * This makes changes to scale, font size, margin, etc. take effect immediately.
 */
void sdl_apply_config(void)
{
    if (!g_state.window) {
        log_warn("sdl_apply_config: no window, skipping");
        return;
    }
    
    int w, h;
    SDL_GetWindowSize(g_state.window, &w, &h);
    SDL_Rect screen = { 0, 0, w, h };
    sdl_load_story_fonts();
    resize(&screen);
    
    // Redraw the screen to prevent black empty spaces
    Term_redraw();
}


static void sdl_apply_story_font_state(bool active)
{
    log_trace("Story font state apply: active=%s depth=%d term=%p",
              active ? "true" : "false", g_state.story_font_depth, (void*)Term);
    for (int i = 0; i < MAX_TERM_DATA; i++)
    {
        if (g_views[i].term_ready)
        {
            g_views[i].t.story_font_active = active;
        }
    }
    if (Term)
        Term->story_font_active = active;
}

static void sdl_apply_story_grid_state(bool grid)
{
    log_trace("Story grid state apply: grid=%s term=%p",
              grid ? "true" : "false", (void*)Term);
    for (int i = 0; i < MAX_TERM_DATA; i++)
    {
        if (g_views[i].term_ready)
        {
            g_views[i].t.story_font_grid = grid;
        }
    }
    if (Term)
        Term->story_font_grid = grid;
}

static void sdl_story_font_reset_state(void)
{
    g_state.story_font_depth = 0;
    sdl_apply_story_font_state(false);
    g_state.story_font_grid = false;
    sdl_apply_story_grid_state(false);
    if (Term)
        Term->story_chunk_active = false;
    log_trace("Story font state hard reset");
}

static void sdl_render_mono_text(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col)
{
    if (!d || !d->font_atlas || n <= 0)
        return;

    SDL_SetTextureColorMod(d->font_atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        SDL_FRect src = {
            (ch & 15) * d->cell_w,
            (ch >> 4) * d->cell_h,
            d->cell_w,
            d->cell_h,
        };
        SDL_FRect dst = {
            (x + i) * d->cell_w,
            y * d->cell_h,
            d->cell_w,
            d->cell_h
        };
        if (use_graphics == GRAPHICS_PSEUDO && solid_walls && (ch == '#' || ch == '%')) {
            SDL_SetRenderDrawColor(g_state.renderer, col.r, col.g, col.b, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(g_state.renderer, &dst);
        }
        SDL_RenderTexture(g_state.renderer, d->font_atlas, &src, &dst);
    }
}

static void sdl_render_story_text_free(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col)
{
    if (!d || !font || n <= 0)
        return;

    char text_buf[256];
    int len = (n < 255) ? n : 255;
    memcpy(text_buf, s, len);
    text_buf[len] = '\0';

    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text_buf, 0, col);
    if (!text_surface)
        return;

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(g_state.renderer, text_surface);
    if (text_texture) {
        float cell_h_f = (float)d->cell_h;
        float surf_h_f = (float)text_surface->h;
        float scale = (surf_h_f > 0.0f) ? (cell_h_f / surf_h_f) : 1.0f;

        SDL_FRect dst = {
            (float)(x * d->cell_w),
            (float)(y * d->cell_h),
            (float)(text_surface->w) * scale,
            cell_h_f
        };

        float max_w = (float)(n * d->cell_w);
        if (dst.w > max_w) dst.w = max_w;

        SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, text_texture, NULL, &dst);
        SDL_DestroyTexture(text_texture);
    }

    SDL_DestroySurface(text_surface);
}

static int sdl_render_story_text_free_px(sdl_view* d, TTF_Font* font, float x_px, int y, const char* s, int n,
    SDL_Color col, float max_w_px)
{
    if (!d || !font || !s || n <= 0)
        return 0;

    char text_buf[256];
    int len = (n < 255) ? n : 255;
    for (int i = 0; i < len; i++)
    {
        unsigned char ch = (unsigned char)s[i];
        text_buf[i] = (ch ? (char)ch : ' ');
    }
    text_buf[len] = '\0';

    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text_buf, 0, col);
    if (!text_surface)
        return 0;

    int adv_w_unscaled = 0;
    TTF_MeasureString(font, text_buf, len, 0, &adv_w_unscaled, NULL);

    float cell_h_f = (float)d->cell_h;
    float surf_h_f = (float)text_surface->h;
    float scale = (surf_h_f > 0.0f) ? (cell_h_f / surf_h_f) : 1.0f;
    float advance_w = (float)adv_w_unscaled * scale;
    float render_w = (float)text_surface->w * scale;

    if (max_w_px > 0.0f && render_w > max_w_px)
        render_w = max_w_px;
    if (max_w_px > 0.0f && advance_w > max_w_px)
        advance_w = max_w_px;

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(g_state.renderer, text_surface);
    if (text_texture)
    {
        SDL_FRect dst = {
            x_px,
            (float)(y * d->cell_h),
            render_w,
            cell_h_f
        };

        SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, text_texture, NULL, &dst);
        SDL_DestroyTexture(text_texture);
    }

    SDL_DestroySurface(text_surface);
    return (int)advance_w;
}

static bool sdl_story_cell_is_text(byte a, char c)
{
    unsigned char uc = (unsigned char)c;

    /* High-bit attr/char pairs are tiles and are handled by pict hook. */
    if ((a & 0x80) && (uc & 0x80))
        return false;

    /* Bigtile second cell. */
    if (a == 255 && uc == 0xFF)
        return false;

    return true;
}

static void sdl_render_story_row_packed(sdl_view* d, TTF_Font* font, int y, const byte* story_row,
    const char* row_chars, const byte* row_attr)
{
    if (!d || !font || !Term || !story_row || !row_chars || !row_attr)
        return;

    const int wid = Term->wid;
    const float cell_w_f = (float)d->cell_w;
    const float cell_h_f = (float)d->cell_h;

    int x = 0;
    while (x < wid)
    {
        /* Skip non-text cells (tiles) entirely to avoid clearing/overdrawing them. */
        if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
        {
            x++;
            continue;
        }

        byte flags = story_row[x];
        bool use_story = (flags & STORY_FLAG_USE) != 0;
        bool grid_align = (flags & STORY_FLAG_CELL_ALIGN) != 0;
        byte attr = row_attr[x];

        int run_start = x;

        if (!use_story)
        {
            while (x < wid)
            {
                if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
                    break;
                byte f = story_row[x];
                if ((f & STORY_FLAG_USE) != 0)
                    break;
                if (row_attr[x] != attr)
                    break;
                x++;
            }

            int run_len = x - run_start;
            SDL_FRect clear_rect = { run_start * cell_w_f, y * cell_h_f, run_len * cell_w_f, cell_h_f };
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(g_state.renderer, &clear_rect);

            SDL_Color col = {
                angband_color_table[attr][1],
                angband_color_table[attr][2],
                angband_color_table[attr][3],
                255
            };
            sdl_render_mono_text(d, run_start, y, run_len, row_chars + run_start, col);
            continue;
        }

        if (grid_align)
        {
            while (x < wid)
            {
                if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
                    break;
                byte f = story_row[x];
                if ((f & STORY_FLAG_USE) == 0)
                    break;
                if ((f & STORY_FLAG_CELL_ALIGN) == 0)
                    break;
                if (row_attr[x] != attr)
                    break;
                x++;
            }

            int run_len = x - run_start;
            SDL_FRect clear_rect = { run_start * cell_w_f, y * cell_h_f, run_len * cell_w_f, cell_h_f };
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(g_state.renderer, &clear_rect);

            SDL_Color col = {
                angband_color_table[attr][1],
                angband_color_table[attr][2],
                angband_color_table[attr][3],
                255
            };
            sdl_render_story_text_grid(d, font, run_start, y, run_len, row_chars + run_start, col);
            continue;
        }

        /* Free story region: pack segments by measured pixel width (including spaces). */
        while (x < wid)
        {
            if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
                break;
            byte f = story_row[x];
            if ((f & STORY_FLAG_USE) == 0)
                break;
            if ((f & STORY_FLAG_CELL_ALIGN) != 0)
                break;
            x++;
        }

        int region_start = run_start;
        int region_end = x;
        if (region_end <= region_start)
            continue;

        SDL_FRect clear_rect = { region_start * cell_w_f, y * cell_h_f, (region_end - region_start) * cell_w_f,
            cell_h_f };
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(g_state.renderer, &clear_rect);

        float px_cursor = region_start * cell_w_f;
        float px_end = region_end * cell_w_f;

        int seg = region_start;
        while (seg < region_end)
        {
            if (!sdl_story_cell_is_text(row_attr[seg], row_chars[seg]))
            {
                seg++;
                continue;
            }

            byte seg_attr = row_attr[seg];
            int seg_end = seg + 1;
            while (seg_end < region_end && sdl_story_cell_is_text(row_attr[seg_end], row_chars[seg_end])
                && row_attr[seg_end] == seg_attr)
            {
                seg_end++;
            }

            int seg_len = seg_end - seg;
            SDL_Color seg_col = {
                angband_color_table[seg_attr][1],
                angband_color_table[seg_attr][2],
                angband_color_table[seg_attr][3],
                255
            };

            float remaining = px_end - px_cursor;
            if (remaining <= 0.0f)
                break;

            int consumed = sdl_render_story_text_free_px(d, font, px_cursor, y, row_chars + seg, seg_len, seg_col,
                remaining);
            if (consumed <= 0)
                break;

            px_cursor += (float)consumed;
            seg = seg_end;
        }
    }
}

static void sdl_render_story_text_grid(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col)
{
    if (!d || !font || n <= 0)
        return;

    float cell_w_f = (float)d->cell_w;
    float cell_h_f = (float)d->cell_h;

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (!ch || ch == ' ')
            continue;

        char glyph_text[2] = { (char)ch, '\0' };
        SDL_Surface* glyph_surface = TTF_RenderText_Blended(font, glyph_text, 0, col);
        if (!glyph_surface)
            continue;

        SDL_Texture* glyph_texture = SDL_CreateTextureFromSurface(g_state.renderer, glyph_surface);
        if (glyph_texture) {
            float surf_w = (float)glyph_surface->w;
            float surf_h = (float)glyph_surface->h;
            float scale = (surf_h > 0.0f) ? (cell_h_f / surf_h) : 1.0f;
            float scaled_w = surf_w * scale;
            float dst_w = scaled_w;
            float offset_x = 0.0f;

            if (scaled_w > cell_w_f) {
                dst_w = cell_w_f;
                scale = (surf_w > 0.0f) ? (dst_w / surf_w) : 1.0f;
            } else {
                offset_x = (cell_w_f - scaled_w) * 0.5f;
            }

            SDL_FRect dst = {
                (float)((x + i) * d->cell_w) + offset_x,
                (float)(y * d->cell_h),
                dst_w,
                cell_h_f
            };

            SDL_SetTextureBlendMode(glyph_texture, SDL_BLENDMODE_BLEND);
            SDL_RenderTexture(g_state.renderer, glyph_texture, NULL, &dst);
            SDL_DestroyTexture(glyph_texture);
        }

        SDL_DestroySurface(glyph_surface);
    }
}
