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
#if defined(SDL_PLATFORM_ANDROID)
#include <jni.h>
#endif

#if defined(__ANDROID__) || defined(SIL_IOS)
#define SIL_SDL_MOBILE_BUILD 1
#else
#define SIL_SDL_MOBILE_BUILD 0
#endif

#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
#define SIL_SDL_DESKTOP_HANDHELD_BUILD 1
#else
#define SIL_SDL_DESKTOP_HANDHELD_BUILD 0
#endif

#if SIL_SDL_MOBILE_BUILD || SIL_SDL_DESKTOP_HANDHELD_BUILD
#define SIL_SDL_HANDHELD_DEFAULTS_BUILD 1
#else
#define SIL_SDL_HANDHELD_DEFAULTS_BUILD 0
#endif

const char help_sdl[] = "SDL3";

static const char* const sdl_story_fallback_font = "lib/xtra/font/MarcellusSC-Regular.ttf";

enum {
    TILE_SIZE = 16,
    MAX_TERM_DATA = ANGBAND_TERM_MAX,
    MAX_STORY_FONT_CACHE = 32,
    MAX_MONO_FONT_CACHE = 48,
    TOUCH_PANE_LONG_PRESS_MS = 350,
    TOUCH_SWIPE_MIN_DISTANCE_PX = 24,
    TOUCH_SWIPE_MAX_DISTANCE_PX = 72,
    SDL_STARTUP_ISSUE_MAX = 1024,
};

typedef struct sdl_layout_recovery_result {
    bool mode_changed;
    bool scale_changed;
    int old_mode;
    int new_mode;
    int old_scale;
    int new_scale;
} sdl_layout_recovery_result;

// SDL configuration (loaded from INI file)
struct sdl_config config;
bool g_hide_left_panel = false;

// Sound configuration (loaded from sound.json)
struct sound_config g_sound_config;

// Configuration file path (needed for saving on exit)
char config_file_path[1024];

// Default pane configuration
static const struct pane_config default_pane_config[] = {
    // On the right
    {.pane = PANE_INVENTORY, .where = PLACE_RIGHT, .enabled = true},
    {.pane = PANE_WORN, .where = PLACE_RIGHT, .enabled = true},
    {.pane = PANE_INFO, .where = PLACE_RIGHT, .enabled = true, .rect.rows = 8},
    {.pane = PANE_TOUCH, .where = PLACE_DOUBLE_RIGHT, .enabled = false},
    // In the bottom
    {.pane = PANE_ROLLS, .where = PLACE_BOTTOM, .enabled = true, .rect.rows = 4},
    {.pane = PANE_LOG, .where = PLACE_BOTTOM, .enabled = true},
};
const int default_pane_config_count = sizeof(default_pane_config) / sizeof(struct pane_config);

// Active pane configuration (may be loaded from INI)
struct pane_config pane_config[MAX_PANE_CONFIGS];
int pane_config_count = 0;
static struct sdl_pane_profile g_pane_profiles[SDL_PANE_PROFILE_COUNT];

static void sdl_copy_pane_configs(struct pane_config* dest, int* dest_count,
    const struct pane_config* src, int src_count)
{
    int count = src_count;

    if (!dest || !dest_count)
        return;

    if (count < 0)
        count = 0;
    if (count > MAX_PANE_CONFIGS)
        count = MAX_PANE_CONFIGS;

    if (count > 0 && src)
        memcpy(dest, src, sizeof(struct pane_config) * count);

    if (count < MAX_PANE_CONFIGS)
        memset(dest + count, 0, sizeof(struct pane_config) * (MAX_PANE_CONFIGS - count));

    *dest_count = count;
}

static bool sdl_min_terminal_mode_is_valid(int mode)
{
    return (mode == SDL_MIN_TERMINAL_NORMAL || mode == SDL_MIN_TERMINAL_COMPACT);
}

static void sdl_store_active_pane_profile(int mode)
{
    if (!sdl_min_terminal_mode_is_valid(mode))
        return;
    if (mode >= SDL_PANE_PROFILE_COUNT)
        return;

    g_pane_profiles[mode].main_view_scale = config.main_view_scale;
    g_pane_profiles[mode].aux_view_font_size = config.aux_view_font_size;
    g_pane_profiles[mode].enable_right_panes = config.enable_right_panes;
    g_pane_profiles[mode].enable_bottom_panes = config.enable_bottom_panes;
    sdl_copy_pane_configs(g_pane_profiles[mode].pane_configs,
        &g_pane_profiles[mode].pane_count, pane_config, pane_config_count);
}

static void sdl_apply_stored_pane_profile(int mode)
{
    if (!sdl_min_terminal_mode_is_valid(mode))
        return;
    if (mode >= SDL_PANE_PROFILE_COUNT)
        return;

    config.main_view_scale = g_pane_profiles[mode].main_view_scale;
    config.aux_view_font_size = g_pane_profiles[mode].aux_view_font_size;
    config.enable_right_panes = g_pane_profiles[mode].enable_right_panes;
    config.enable_bottom_panes = g_pane_profiles[mode].enable_bottom_panes;
    sdl_copy_pane_configs(pane_config, &pane_config_count,
        g_pane_profiles[mode].pane_configs, g_pane_profiles[mode].pane_count);
}

static void sdl_seed_all_pane_profiles_from_active(void)
{
    for (int mode = 0; mode < SDL_PANE_PROFILE_COUNT; mode++)
        sdl_store_active_pane_profile(mode);
}

static void sdl_reset_config_to_resolution_defaults(int screen_width,
    int screen_height)
{
    bool matched_profile = sdl_config_set_defaults_for_resolution(&config, pane_config,
        &pane_config_count, MAX_PANE_CONFIGS, screen_width, screen_height);

    if (!matched_profile && pane_config_count == 0) {
        pane_config_count = default_pane_config_count;
        for (int i = 0; i < default_pane_config_count
            && i < MAX_PANE_CONFIGS; i++)
        {
            pane_config[i] = default_pane_config[i];
        }
    }

    sdl_seed_all_pane_profiles_from_active();
}

static int sdl_min_terminal_cols_for_mode(int mode)
{
    return (mode == SDL_MIN_TERMINAL_COMPACT) ? 50 : 80;
}

static int sdl_min_terminal_rows_for_mode(int mode)
{
    return (mode == SDL_MIN_TERMINAL_COMPACT) ? 18 : 24;
}

static int sdl_current_min_terminal_cols(void)
{
    return sdl_min_terminal_cols_for_mode(config.min_terminal_mode);
}

static int sdl_current_min_terminal_rows(void)
{
    return sdl_min_terminal_rows_for_mode(config.min_terminal_mode);
}

static const char* sdl_min_terminal_mode_name(int mode)
{
    return (mode == SDL_MIN_TERMINAL_COMPACT) ? "compact" : "normal";
}

typedef struct story_font_entry {
    int pixel_height;
    TTF_Font* font;
} story_font_entry;

typedef struct mono_font_atlas_entry {
    bool valid;
    char font_path[256];
    int cell_height;
    bool bold;
    bool italic;
    bool underline;
    bool strikethrough;
    int hinting;
    bool kerning;
    int outline;
    SDL_Texture* atlas;
} mono_font_atlas_entry;

typedef struct sdl_state {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* tileset;
    SDL_Color palette[16];
    SDL_Rect safe_area;
    float system_scale;
    int tileset_cols;
    bool need_present;
    bool use_tiles;
    
    // Custom fonts
    story_font_entry story_fonts[MAX_STORY_FONT_CACHE];
    int story_font_count;
    int story_font_depth;      // Nesting counter for story font enable/disable
    bool story_font_grid;      // Whether queued story text should snap to cell grid
    bool story_font_cache_valid;
    char story_font_cache_path[256];
    bool story_font_cache_bold;
    bool story_font_cache_italic;
    bool story_font_cache_underline;
    bool story_font_cache_strikethrough;
    int story_font_cache_hinting;
    bool story_font_cache_kerning;
    int story_font_cache_outline;
    mono_font_atlas_entry mono_font_atlases[MAX_MONO_FONT_CACHE];
    
} sdl_state;

typedef struct sdl_view {
    SDL_Rect rect;
    SDL_Texture* canvas;
    SDL_Texture* font_atlas;
    bool font_atlas_cached;
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

typedef struct touch_pane_slot_info {
    const char* slot_name;
    const char* default_label;
    int default_binding;
} touch_pane_slot_info;

static const touch_pane_slot_info g_touch_pane_slots[SDL_TOUCH_PANE_BUTTON_COUNT] = {
    { "Esc", "Esc", ESCAPE },
    { "Ctrl", "Ctrl", GAMEPAD_BIND_CTRL },
    { "Shift", "Shift", GAMEPAD_BIND_SHIFT },
    { "Worn", "Worn", 'e' },
    { "Inv", "Inv", 'i' },
    { "Supply", "Supply", 'j' },
    { "Use", "Use", 'u' },
    { "Sing", "Sing", 's' },
    { "Shoot", "Shoot", 'f' },
    { "Northwest", NULL, '7' },
    { "North", NULL, '8' },
    { "Northeast", NULL, '9' },
    { "West", NULL, '4' },
    { "Center", "pick", INPUT_BIND_CONFIRM },
    { "East", NULL, '6' },
    { "Southwest", NULL, '1' },
    { "South", NULL, '2' },
    { "Southeast", NULL, '3' },
    { "Staff", "View", 'l' },
    { "Desc", "Desc", 'x' },
    { "Drop", "Staff", 'a' },
    { "Map", "Map", 'M' },
    { "Hero", "Hero", 'h' },
    { "Ability", "Ability", '\t' },
};

enum {
    SDL_TOUCH_PANE_CENTER_SLOT = 13,
};

enum {
    MAX_GAMEPADS = 4,
    DPAD_DIAGONAL_WINDOW_MS = 100,
    SHOULDER_COMBO_WINDOW_MS = 150,
    /* Rebinding should listen immediately instead of dropping quick inputs. */
    GAMEPAD_CAPTURE_ARM_DELAY_MS = 0,
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

typedef struct touch_pane_press_state {
    bool active;
    bool mouse;
    SDL_FingerID finger_id;
    int panel;
    int slot;
    Uint64 start_time;
} touch_pane_press_state;

typedef struct touch_swipe_state {
    bool active;
    bool triggered;
    SDL_FingerID finger_id;
    float start_x;
    float start_y;
    float last_x;
    float last_y;
} touch_swipe_state;

sdl_state g_state;
sdl_view g_views[MAX_TERM_DATA];
static SDL_Rect g_pane_rects[PANE_MAX];
static bool g_active_side_panes = true;
static bool g_active_bottom_panes = true;
static bool g_supporting_panes_layout_visible = true;
static bool g_suppress_layout_refresh_present = false;
static bool g_skip_main_redraw_on_layout_refresh = false;
static gamepad_input_state g_gamepad_state;
static bool g_gamepad_auto_ui = false;
static int g_default_gamepad_button_bindings[SDL_GAMEPAD_BUTTON_COUNT];
static int g_default_gamepad_trigger_bindings[GAMEPAD_TRIGGER_COUNT];
static int g_default_gamepad_left_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_button_combo_bindings[GAMEPAD_MODIFIER_COUNT][SDL_GAMEPAD_BUTTON_COUNT];
static int g_default_gamepad_trigger_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_TRIGGER_COUNT];
static int g_default_gamepad_left_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_right_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_shoulder_combo_binding = GAMEPAD_BIND_NONE;
static bool g_default_gamepad_bindings_ready = false;
static int g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_BUTTON_COUNT];
static char g_default_touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_LABEL_LEN];
static bool g_default_touch_swipe_enabled = true;
static int g_default_touch_swipe_bindings[GAMEPAD_STICK_DIR_COUNT];
static bool g_default_touch_pane_bindings_ready = false;
static bool g_gamepad_capture_active = false;
static bool g_gamepad_capture_ready = false;
static Uint64 g_gamepad_capture_arm_time = 0;
static bool g_gamepad_capture_allow_modifier_combo = false;
static int g_gamepad_capture_modifier = GAMEPAD_BIND_NONE;
static int g_gamepad_capture_type = GAMEPAD_CAPTURE_BUTTON;
static int g_gamepad_capture_id = 0;
static int g_touch_pane_flash_slot = -1;
static Uint64 g_touch_pane_flash_until = 0;
static int g_touch_pane_pressed_slot = -1;
static bool g_touch_pane_second_panel = false;
static bool g_touch_pane_ctrl_toggle = false;
static bool g_touch_pane_reset_confirm_active = false;
static touch_pane_press_state g_touch_pane_press;
static touch_swipe_state g_touch_swipe;
static int g_auto_aux_main_cell_h_override = 0;

static sdl_view* sdl_view_from_term(term* t);
static void sdl_view_destroy(sdl_view* d);
void resize(const SDL_Rect* screen);
bool steamdeck_controls_active(void);
static bool sdl_rect_has_area(const SDL_Rect* rect);
static SDL_Rect sdl_get_window_pixel_rect(void);
static SDL_Rect sdl_window_rect_to_pixel_rect(const SDL_Rect* rect);
#if defined(SDL_PLATFORM_ANDROID)
static int sdl_android_sdk_int(JNIEnv* env);
static SDL_Rect sdl_get_android_display_cutout_rect(void);
#endif
static void sdl_refresh_safe_area(void);
static SDL_Rect sdl_get_layout_screen_rect(void);
static bool sdl_mobile_prefer_safe_edge_alignment(void);
static void sdl_resize_for_current_layout(void);
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
static void sdl_gamepad_send_key_raw(int key);
static void sdl_send_macro_key(int key, bool shift, bool ctrl, bool alt);
static int sdl_keymap_mode(void);
static char sdl_direction_char_for_key(int key);
static int sdl_direction_for_key_char(char ch);
static bool sdl_send_modified_direction_action(int dir, char dir_ch, bool shift, bool ctrl, bool alt,
    bool gui);
static bool sdl_try_send_modified_direction_key(int key, bool shift, bool ctrl, bool alt, bool gui);
static bool sdl_try_send_modified_direction_event(const SDL_KeyboardEvent* key_event);
static bool sdl_handle_global_layout_shortcut(const SDL_KeyboardEvent* key_event);
static void sdl_gamepad_apply_modifier(int binding, bool down);
static bool sdl_gamepad_shift_active(void);
static bool sdl_gamepad_ctrl_active(void);
static bool sdl_gamepad_alt_active(void);
static int sdl_gamepad_modifier_index(int binding);
static int sdl_gamepad_single_active_modifier(void);
static int sdl_gamepad_combo_binding_for_input(int modifier, int type, int id);
static void sdl_gamepad_clear_pending_dpad(void);
static void sdl_gamepad_set_pending_dpad(int dir);
static bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force);
static void sdl_gamepad_clear_pending_left_stick(void);
static void sdl_gamepad_set_pending_left_stick(int dir);
static bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force);
static void sdl_gamepad_clear_pending_shoulder(void);
static void sdl_gamepad_set_pending_shoulder(int button);
static bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force);
static bool sdl_gamepad_resolve_pending_shoulder_with_modifier(int binding);
static int sdl_gamepad_capture_binding_for_input(int type, int id);
static bool sdl_gamepad_capture_queue_input(int type, int id);
static int sdl_gamepad_pending_timeout_ms(Uint64 now_ns);
static void sdl_apply_story_font_state(bool active);
static void sdl_apply_story_grid_state(bool grid);
static void sdl_story_font_reset_state(void);
static void sdl_story_font_cache_clear(void);
static bool sdl_story_font_cache_matches_config(void);
static void sdl_story_font_cache_mark_config(void);
static void sdl_mono_font_cache_clear(void);
static SDL_Texture* sdl_acquire_mono_font_atlas(const char* font_path,
    int cell_height, bool* out_cached);
static TTF_Font* sdl_story_font_for_height(int pixel_height);
static TTF_Font* sdl_story_font_for_view(const sdl_view* d);
#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
static void sdl_ensure_default_pane_configs_present(bool enable_new_panes);
static void sdl_apply_mobile_default_pane_layout(const SDL_Rect* screen,
    bool has_controller);
#endif
static void sdl_ensure_touch_pane_config_present(void);
static bool sdl_touch_pane_panel_is_valid(int panel);
static int sdl_touch_pane_active_panel(void);
static int sdl_touch_pane_other_panel(int panel);
static int sdl_touch_pane_raw_binding_for_panel(int panel, int index);
static int sdl_touch_pane_effective_binding_for_panel(int panel, int index);
static bool sdl_touch_pane_point_to_slot(float x, float y, int* out_slot);
static bool sdl_touch_pane_compute_layout(const SDL_Rect* pane_rect, SDL_FRect* slot_rects);
static bool sdl_touch_pane_binding_is_direction(int binding);
static bool sdl_touch_pane_slot_uses_long_press(int slot, int binding);
static bool sdl_touch_pane_confirm_binding(int binding);
static bool sdl_touch_pane_main_panel_has_confirm_excluding(int skip_index);
static void sdl_touch_pane_ensure_main_panel_confirm(void);
static void sdl_touch_pane_begin_reset_confirm(void);
static void sdl_touch_pane_finish_reset_confirm(bool confirmed);
static void sdl_touch_pane_handle_reset_prompt_pointer(float x, float y);
static void sdl_touch_pane_draw_arrow(const SDL_FRect* rect, int binding, SDL_Color color);
static void sdl_touch_pane_draw_button_text(const SDL_FRect* rect, const char* name, const char* symbol,
    SDL_Color color);
static void sdl_touch_pane_binding_symbol(int binding, char* buf, size_t buflen);
static bool sdl_touch_pane_label_is_symbol_only(const char* label);
static bool sdl_touch_pane_should_hide_symbol(const char* name, const char* symbol);
static void sdl_touch_pane_render_reset_prompt(void);
static void sdl_touch_pane_default_label_for_panel_slot(int panel, int index, char* buf, size_t buflen);
static void sdl_touch_pane_base_label_for_slot(int panel, int index, char* buf, size_t buflen);
static void sdl_touch_pane_display_label_for_slot(int panel, int index, char* buf, size_t buflen);
static void sdl_touch_pane_render(void);
static bool sdl_touch_pane_handle_pointer_down(float x, float y, bool mouse, SDL_FingerID finger_id);
static void sdl_touch_pane_handle_pointer_up(bool mouse, SDL_FingerID finger_id);
static void sdl_touch_pane_cancel_press(void);
static int sdl_touch_pane_pending_timeout_ms(Uint64 now_ns);
static bool sdl_touch_pane_flush_pending_press(Uint64 now_ns);
static void sdl_touch_pane_send_slot(int panel, int index, bool long_press);
static void sdl_touch_pane_send_binding(int binding, bool second_panel, bool long_press);
static void sdl_touch_pane_load_default_bindings(void);
static bool sdl_touch_pane_is_config_enabled(void);
static bool sdl_is_log_pane_active(void);
static bool sdl_is_bottom_log_pane_active(void);
static void sdl_handle_log_pane_activation(bool log_was_active, bool bottom_log_was_active);
static int sdl_touch_swipe_index_for_keypad_dir(int dir);
static float sdl_touch_swipe_threshold_px(void);
static int sdl_touch_swipe_direction_for_delta(float dx, float dy, float threshold);
static void sdl_touch_swipe_cancel(void);
static bool sdl_touch_swipe_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
static bool sdl_touch_swipe_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
static void sdl_touch_swipe_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
int get_sdl_touch_pane_binding_for_panel(int panel, int index);
void set_sdl_touch_pane_binding_for_panel(int panel, int index, int binding);
int get_sdl_touch_pane_default_binding_for_panel(int panel, int index);
void get_sdl_touch_pane_button_label_for_panel(int panel, int index, char* buf, size_t buflen);
void set_sdl_touch_pane_button_label_for_panel(int panel, int index, cptr label);
void clear_sdl_touch_pane_button_label_for_panel(int panel, int index);
void get_sdl_touch_pane_panel_name(int panel, char* buf, size_t buflen);
void set_sdl_touch_pane_panel_name(int panel, cptr name);
static void sdl_update_cursor_visibility(void);
static void sdl_draw_pane_edges(const SDL_Rect* rect, bool draw_left,
    bool draw_top, bool draw_right, bool draw_bottom);
static bool sdl_should_show_supporting_panes(void);
static bool sdl_hide_supporting_panes_mode_effective(void);
static bool sdl_layout_matches_supporting_pane_visibility(void);
static void sdl_present_if_needed(sdl_view* d);
static int sdl_build_active_pane_config(struct pane_config* active, bool include_side,
    bool include_bottom, bool touch_only);
static int sdl_auto_aux_view_font_size(void);
static int sdl_resolve_aux_view_font_size(int requested_size);
static int sdl_effective_pane_font_size_for_config(const struct pane_config* pc);
static int sdl_effective_pane_font_size_for_type(enum pane_type type);
static void sdl_build_supporting_pane_metrics(const struct pane_config* configs,
    int count, int* cell_widths, int* cell_heights);
static bool sdl_prune_unusable_panes(struct pane_config* active,
    int active_count, SDL_Rect* panes, const int* cell_widths,
    const int* cell_heights);
static void sdl_place_active_panes(const SDL_Rect* screen, SDL_Rect* panes,
    bool include_side, bool include_bottom, bool touch_only);
static void sdl_compute_split_panes(const SDL_Rect* screen, SDL_Rect* panes);
static void sdl_compute_display_panes(SDL_Rect* panes);
static int sdl_max_scale_for_rect(const SDL_Rect* rect);
static int sdl_max_scale_for_rect_mode(const SDL_Rect* rect, int mode);
static int sdl_max_scale_for_window_mode(int mode);
static bool sdl_mode_scale_fits_window(const SDL_Rect* screen, int mode,
    int scale, int* cols, int* rows);
static void sdl_ensure_window_size_for_min_terminal(const SDL_Rect* screen,
    int* window_width, int* window_height);
static bool sdl_recover_layout_for_current_window(const char* reason,
    bool notify_user, sdl_layout_recovery_result* recovery);
static void sdl_format_layout_recovery_message(const char* reason,
    const sdl_layout_recovery_result* recovery, char* buf, size_t buflen);
static void sdl_append_issue_line(char* buf, size_t buflen, const char* line);
static void sdl_reset_config_to_resolution_defaults(int screen_width,
    int screen_height);
static bool sdl_prompt_reset_sdl_defaults(const char* issue_summary,
    int screen_width, int screen_height);
static bool sdl_is_desktop_handheld_resolution(int width, int height);
static int sdl_touch_pane_target_width_px(int pane_height_px);
static void sdl_apply_dynamic_auto_pane_sizes(struct pane_config* active,
    int active_count, const SDL_Rect* screen, const int* cell_widths,
    const int* cell_heights, int margin_px);
static void sdl_touch_pane_send_confirm_action(void);
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
static bool sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin);
static void sdl_load_story_fonts(void);
static TTF_Font* sdl_load_font_with_fallback(const char* font_path, int font_size, const char* fallback_path);
static void sdl_handle_renderer_reset(void);
void sdl_request_redraw(void);

static sdl_view* sdl_view_from_term(term* t)
{
    if (!t)
        return NULL;

    size_t idx = (size_t)(uintptr_t)t->data;
    if (idx >= MAX_TERM_DATA) {
        log_warn("sdl_view_from_term: invalid term index %zu (max %d)", idx, MAX_TERM_DATA - 1);
        return NULL;
    }

    return &g_views[idx];
}

static bool sdl_rect_has_area(const SDL_Rect* rect)
{
    return (rect && rect->w > 0 && rect->h > 0);
}

static SDL_Rect sdl_get_window_pixel_rect(void)
{
    SDL_Rect rect = { 0 };

    if (g_state.window)
        SDL_GetWindowSizeInPixels(g_state.window, &rect.w, &rect.h);

    return rect;
}

static SDL_Rect sdl_window_rect_to_pixel_rect(const SDL_Rect* rect)
{
    SDL_Rect pixel_rect = { 0 };
    SDL_Rect pixel_window = sdl_get_window_pixel_rect();
    int window_w = 0;
    int window_h = 0;

    if (!rect)
        return pixel_rect;

    pixel_rect = *rect;

    if (!g_state.window || !sdl_rect_has_area(&pixel_window))
        return pixel_rect;

    SDL_GetWindowSize(g_state.window, &window_w, &window_h);
    if (window_w <= 0 || window_h <= 0)
        return pixel_rect;

    if (window_w != pixel_window.w || window_h != pixel_window.h) {
        float scale_x = (float)pixel_window.w / (float)window_w;
        float scale_y = (float)pixel_window.h / (float)window_h;
        int x1 = (int)SDL_ceilf((float)rect->x * scale_x);
        int y1 = (int)SDL_ceilf((float)rect->y * scale_y);
        int x2 = (int)SDL_floorf((float)(rect->x + rect->w) * scale_x);
        int y2 = (int)SDL_floorf((float)(rect->y + rect->h) * scale_y);

        pixel_rect.x = x1;
        pixel_rect.y = y1;
        pixel_rect.w = x2 - x1;
        pixel_rect.h = y2 - y1;
    }

    if (pixel_rect.x < 0)
        pixel_rect.x = 0;
    if (pixel_rect.y < 0)
        pixel_rect.y = 0;
    if (pixel_rect.x > pixel_window.w)
        pixel_rect.x = pixel_window.w;
    if (pixel_rect.y > pixel_window.h)
        pixel_rect.y = pixel_window.h;
    if (pixel_rect.x + pixel_rect.w > pixel_window.w)
        pixel_rect.w = pixel_window.w - pixel_rect.x;
    if (pixel_rect.y + pixel_rect.h > pixel_window.h)
        pixel_rect.h = pixel_window.h - pixel_rect.y;
    if (pixel_rect.w < 0)
        pixel_rect.w = 0;
    if (pixel_rect.h < 0)
        pixel_rect.h = 0;

    return pixel_rect;
}

#if defined(SDL_PLATFORM_ANDROID)
static int sdl_android_sdk_int(JNIEnv* env)
{
    int sdk = 0;
    jclass version_class = NULL;
    jfieldID sdk_field = NULL;

    if (!env)
        return 0;

    version_class = (*env)->FindClass(env, "android/os/Build$VERSION");
    if (!version_class)
        return 0;

    sdk_field = (*env)->GetStaticFieldID(env, version_class, "SDK_INT", "I");
    if (sdk_field)
        sdk = (*env)->GetStaticIntField(env, version_class, sdk_field);

    (*env)->DeleteLocalRef(env, version_class);
    return sdk;
}

static SDL_Rect sdl_get_android_display_cutout_rect(void)
{
    SDL_Rect rect = sdl_get_window_pixel_rect();
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    jclass activity_class = NULL;
    jclass window_class = NULL;
    jclass view_class = NULL;
    jclass insets_class = NULL;
    jclass cutout_class = NULL;
    jobject window = NULL;
    jobject decor_view = NULL;
    jobject insets = NULL;
    jobject cutout = NULL;

    if (!env || !activity || !sdl_rect_has_area(&rect))
        goto cleanup;

    if (sdl_android_sdk_int(env) < 28)
        goto cleanup;

    activity_class = (*env)->GetObjectClass(env, activity);
    if (!activity_class)
        goto cleanup;

    {
        jmethodID get_window = (*env)->GetMethodID(env, activity_class, "getWindow",
            "()Landroid/view/Window;");
        if (!get_window)
            goto cleanup;
        window = (*env)->CallObjectMethod(env, activity, get_window);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }
        if (!window)
            goto cleanup;
    }

    window_class = (*env)->GetObjectClass(env, window);
    if (!window_class)
        goto cleanup;

    {
        jmethodID get_decor_view = (*env)->GetMethodID(env, window_class,
            "getDecorView", "()Landroid/view/View;");
        if (!get_decor_view)
            goto cleanup;
        decor_view = (*env)->CallObjectMethod(env, window, get_decor_view);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }
        if (!decor_view)
            goto cleanup;
    }

    view_class = (*env)->GetObjectClass(env, decor_view);
    if (!view_class)
        goto cleanup;

    {
        jmethodID get_root_window_insets = (*env)->GetMethodID(env, view_class,
            "getRootWindowInsets", "()Landroid/view/WindowInsets;");
        if (!get_root_window_insets)
            goto cleanup;
        insets = (*env)->CallObjectMethod(env, decor_view, get_root_window_insets);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }
        if (!insets)
            goto cleanup;
    }

    insets_class = (*env)->GetObjectClass(env, insets);
    if (!insets_class)
        goto cleanup;

    {
        jmethodID get_display_cutout = (*env)->GetMethodID(env, insets_class,
            "getDisplayCutout", "()Landroid/view/DisplayCutout;");
        if (!get_display_cutout)
            goto cleanup;
        cutout = (*env)->CallObjectMethod(env, insets, get_display_cutout);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }
        if (!cutout)
            goto cleanup;
    }

    cutout_class = (*env)->GetObjectClass(env, cutout);
    if (!cutout_class)
        goto cleanup;

    {
        jmethodID get_safe_inset_left = (*env)->GetMethodID(env, cutout_class,
            "getSafeInsetLeft", "()I");
        jmethodID get_safe_inset_top = (*env)->GetMethodID(env, cutout_class,
            "getSafeInsetTop", "()I");
        jmethodID get_safe_inset_right = (*env)->GetMethodID(env, cutout_class,
            "getSafeInsetRight", "()I");
        jmethodID get_safe_inset_bottom = (*env)->GetMethodID(env, cutout_class,
            "getSafeInsetBottom", "()I");
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;

        if (!get_safe_inset_left || !get_safe_inset_top
            || !get_safe_inset_right || !get_safe_inset_bottom)
            goto cleanup;

        left = (*env)->CallIntMethod(env, cutout, get_safe_inset_left);
        top = (*env)->CallIntMethod(env, cutout, get_safe_inset_top);
        right = (*env)->CallIntMethod(env, cutout, get_safe_inset_right);
        bottom = (*env)->CallIntMethod(env, cutout, get_safe_inset_bottom);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }

        if (left < 0)
            left = 0;
        if (top < 0)
            top = 0;
        if (right < 0)
            right = 0;
        if (bottom < 0)
            bottom = 0;
        if (left + right >= rect.w || top + bottom >= rect.h)
            goto cleanup;

        rect.x = left;
        rect.y = top;
        rect.w -= left + right;
        rect.h -= top + bottom;
    }

cleanup:
    if (env) {
        if (cutout_class)
            (*env)->DeleteLocalRef(env, cutout_class);
        if (cutout)
            (*env)->DeleteLocalRef(env, cutout);
        if (insets_class)
            (*env)->DeleteLocalRef(env, insets_class);
        if (insets)
            (*env)->DeleteLocalRef(env, insets);
        if (view_class)
            (*env)->DeleteLocalRef(env, view_class);
        if (decor_view)
            (*env)->DeleteLocalRef(env, decor_view);
        if (window_class)
            (*env)->DeleteLocalRef(env, window_class);
        if (window)
            (*env)->DeleteLocalRef(env, window);
        if (activity_class)
            (*env)->DeleteLocalRef(env, activity_class);
        if (activity)
            (*env)->DeleteLocalRef(env, activity);
    }

    return rect;
}
#endif

static void sdl_refresh_safe_area(void)
{
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();
    SDL_Rect safe_area = window_pixels;

    if (!g_state.window || !sdl_rect_has_area(&window_pixels)) {
        g_state.safe_area = window_pixels;
        return;
    }

    {
        SDL_Rect window_units = { 0 };
        SDL_Rect safe_units = { 0 };

        SDL_GetWindowSize(g_state.window, &window_units.w, &window_units.h);
        if (window_units.w > 0 && window_units.h > 0
            && SDL_GetWindowSafeArea(g_state.window, &safe_units)
            && safe_units.x >= 0
            && safe_units.y >= 0
            && safe_units.w > 0
            && safe_units.h > 0
            && safe_units.x + safe_units.w <= window_units.w
            && safe_units.y + safe_units.h <= window_units.h)
        {
            safe_area = sdl_window_rect_to_pixel_rect(&safe_units);
            if (!sdl_rect_has_area(&safe_area))
                safe_area = window_pixels;
#if defined(SDL_PLATFORM_ANDROID)
            else if (!config.use_unsafe_area)
                safe_area = sdl_get_android_display_cutout_rect();
#endif
        }
    }

    if (SDL_memcmp(&g_state.safe_area, &safe_area, sizeof(safe_area)) != 0) {
        log_info("SDL layout safe area updated to (%d,%d %dx%d)",
            safe_area.x, safe_area.y, safe_area.w, safe_area.h);
    }

    g_state.safe_area = safe_area;
}

static SDL_Rect sdl_get_layout_screen_rect(void)
{
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();

    if (config.use_unsafe_area)
        return window_pixels;

    if (!sdl_rect_has_area(&g_state.safe_area))
        sdl_refresh_safe_area();

    if (sdl_rect_has_area(&g_state.safe_area)) {
        SDL_Rect safe = g_state.safe_area;

        if (safe.x < 0)
            safe.x = 0;
        if (safe.y < 0)
            safe.y = 0;
        if (safe.x > window_pixels.w)
            safe.x = window_pixels.w;
        if (safe.y > window_pixels.h)
            safe.y = window_pixels.h;
        if (safe.x + safe.w > window_pixels.w)
            safe.w = window_pixels.w - safe.x;
        if (safe.y + safe.h > window_pixels.h)
            safe.h = window_pixels.h - safe.y;

        if (sdl_rect_has_area(&safe))
            return safe;
    }

    return window_pixels;
}

static bool sdl_mobile_prefer_safe_edge_alignment(void)
{
#if defined(__ANDROID__) || defined(SIL_IOS)
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();

    if (config.use_unsafe_area)
        return false;
    if (!sdl_rect_has_area(&g_state.safe_area) || !sdl_rect_has_area(&window_pixels))
        return false;

    return (g_state.safe_area.x != 0
        || g_state.safe_area.y != 0
        || g_state.safe_area.w != window_pixels.w
        || g_state.safe_area.h != window_pixels.h);
#else
    return false;
#endif
}

static void sdl_resize_for_current_layout(void)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();

    if (!sdl_rect_has_area(&screen))
        return;

    resize(&screen);
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
        if (!d->font_atlas_cached)
            SDL_DestroyTexture(d->font_atlas);
        d->font_atlas = NULL;
    }
    d->font_atlas_cached = false;
}

#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
static void sdl_ensure_default_pane_configs_present(bool enable_new_panes)
{
    for (int i = 0; i < default_pane_config_count; i++) {
        bool found = false;

        if (default_pane_config[i].pane == PANE_TOUCH)
            continue;

        for (int j = 0; j < pane_config_count; j++) {
            if (pane_config[j].pane == default_pane_config[i].pane) {
                found = true;
                break;
            }
        }

        if (found)
            continue;

        if (pane_config_count >= MAX_PANE_CONFIGS) {
            log_warn("Could not append pane %d; max pane count reached",
                default_pane_config[i].pane);
            return;
        }

        pane_config[pane_config_count] = default_pane_config[i];
        pane_config[pane_config_count].enabled = enable_new_panes;
        pane_config_count++;
    }
}
#endif

static void sdl_ensure_touch_pane_config_present(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_TOUCH)
            return;
    }

    if (pane_config_count >= MAX_PANE_CONFIGS) {
        log_warn("Could not append touch pane config; max pane count reached");
        return;
    }

    pane_config[pane_config_count++] = (struct pane_config){
        .pane = PANE_TOUCH,
        .where = PLACE_DOUBLE_RIGHT,
        .enabled = false,
        .rect = { .rows = 0, .cols = 0 },
        .ratio = 0.0f,
    };
}

#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
static struct pane_config* sdl_find_pane_config_entry(struct pane_config* configs,
    int count, enum pane_type pane)
{
    if (!configs)
        return NULL;

    for (int i = 0; i < count; i++) {
        if (configs[i].pane == pane)
            return &configs[i];
    }

    return NULL;
}

static void sdl_mobile_reset_default_pane_configs(struct pane_config* configs,
    int count)
{
    struct pane_config* pc;

    if (!configs)
        return;

    for (int i = 0; i < count; i++) {
        switch (configs[i].pane) {
        case PANE_INVENTORY:
            configs[i].where = PLACE_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = 22;
            configs[i].rect.cols = 40;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_WORN:
            configs[i].where = PLACE_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = 17;
            configs[i].rect.cols = 40;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_INFO:
            configs[i].where = PLACE_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = 0;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_TOUCH:
            configs[i].where = PLACE_DOUBLE_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = 0;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_ROLLS:
        case PANE_LOG:
            configs[i].where = PLACE_BOTTOM;
            configs[i].enabled = false;
            configs[i].rect.rows = 0;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        default:
            break;
        }
    }

    pc = sdl_find_pane_config_entry(configs, count, PANE_TOUCH);
    if (pc)
        pc->where = PLACE_DOUBLE_RIGHT;
}

static void sdl_mobile_set_touch_pane_enabled(struct pane_config* configs,
    int count, bool enabled)
{
    struct pane_config* pc = sdl_find_pane_config_entry(configs, count,
        PANE_TOUCH);

    if (!pc)
        return;

    pc->where = PLACE_DOUBLE_RIGHT;
    pc->enabled = enabled;
    pc->rect.rows = 0;
    pc->rect.cols = 0;
    pc->font_size = 0;
    pc->ratio = 0.0f;
}

static void sdl_mobile_disable_bottom_panes(struct pane_config* configs,
    int count)
{
    struct pane_config* rolls = sdl_find_pane_config_entry(configs, count,
        PANE_ROLLS);
    struct pane_config* log = sdl_find_pane_config_entry(configs, count,
        PANE_LOG);

    if (rolls) {
        rolls->enabled = false;
        rolls->where = PLACE_BOTTOM;
        rolls->rect.rows = 0;
        rolls->rect.cols = 0;
        rolls->font_size = 0;
        rolls->ratio = 0.0f;
    }

    if (log) {
        log->enabled = false;
        log->where = PLACE_BOTTOM;
        log->rect.rows = 0;
        log->rect.cols = 0;
        log->font_size = 0;
        log->ratio = 0.0f;
    }
}

static void sdl_mobile_configure_bottom_wide(struct pane_config* configs,
    int count, int rows)
{
    struct pane_config* rolls;
    struct pane_config* log;

    sdl_mobile_disable_bottom_panes(configs, count);
    if (rows <= 0)
        return;

    rolls = sdl_find_pane_config_entry(configs, count, PANE_ROLLS);
    log = sdl_find_pane_config_entry(configs, count, PANE_LOG);

    if (rolls) {
        rolls->enabled = true;
        rolls->where = PLACE_BOTTOM;
        rolls->rect.rows = rows;
        rolls->rect.cols = 65;
    }

    if (log) {
        log->enabled = true;
        log->where = PLACE_BOTTOM;
        log->rect.rows = rows;
        log->rect.cols = 0;
    }
}

static void sdl_mobile_configure_bottom_narrow(struct pane_config* configs,
    int count, int total_rows)
{
    struct pane_config* rolls;
    struct pane_config* log;
    int log_rows = 0;
    int rolls_rows = 0;

    sdl_mobile_disable_bottom_panes(configs, count);
    if (total_rows <= 0)
        return;

    /* Narrow mobile layouts prioritize combat rolls until there is enough
     * height to stack a message log strip above them. */
    switch (total_rows) {
    case 1:
    case 2:
    case 3:
        rolls_rows = total_rows;
        break;

    case 4:
        log_rows = 2;
        rolls_rows = 2;
        break;

    case 5:
        log_rows = 2;
        rolls_rows = 3;
        break;

    case 6:
        log_rows = 2;
        rolls_rows = 4;
        break;

    default:
        log_rows = 3;
        rolls_rows = 4;
        break;
    }

    rolls = sdl_find_pane_config_entry(configs, count, PANE_ROLLS);
    log = sdl_find_pane_config_entry(configs, count, PANE_LOG);

    if (log_rows > 0 && log) {
        log->enabled = true;
        log->where = PLACE_BOTTOM;
        log->rect.rows = log_rows;
        log->rect.cols = 0;
    }

    if (rolls_rows > 0 && rolls) {
        rolls->enabled = true;
        rolls->where = (log_rows > 0) ? PLACE_DOUBLE_BOTTOM : PLACE_BOTTOM;
        rolls->rect.rows = rolls_rows;
        rolls->rect.cols = 0;
    }
}

static void sdl_mobile_set_right_panes(struct pane_config* configs, int count,
    bool inventory_enabled, bool worn_enabled)
{
    struct pane_config* inventory = sdl_find_pane_config_entry(configs, count,
        PANE_INVENTORY);
    struct pane_config* worn = sdl_find_pane_config_entry(configs, count,
        PANE_WORN);
    struct pane_config* info = sdl_find_pane_config_entry(configs, count,
        PANE_INFO);

    if (inventory) {
        inventory->where = PLACE_RIGHT;
        inventory->enabled = inventory_enabled;
        inventory->rect.rows = 22;
        inventory->rect.cols = 40;
        inventory->font_size = 0;
        inventory->ratio = 0.0f;
    }

    if (worn) {
        worn->where = PLACE_RIGHT;
        worn->enabled = worn_enabled;
        worn->rect.rows = 17;
        worn->rect.cols = 40;
        worn->font_size = 0;
        worn->ratio = 0.0f;
    }

    if (info) {
        info->where = PLACE_RIGHT;
        info->enabled = false;
        info->rect.rows = 0;
        info->rect.cols = 0;
        info->font_size = 0;
        info->ratio = 0.0f;
    }
}

static int sdl_mobile_pane_cols(const SDL_Rect* panes, const int* cell_widths,
    enum pane_type type)
{
    if (!panes || !cell_widths || type < PANE_MAIN || type >= PANE_MAX)
        return 0;
    if (panes[type].w <= 0 || cell_widths[type] <= 0)
        return 0;

    return panes[type].w / cell_widths[type];
}

static int sdl_mobile_pane_rows(const SDL_Rect* panes, const int* cell_heights,
    enum pane_type type)
{
    if (!panes || !cell_heights || type < PANE_MAIN || type >= PANE_MAX)
        return 0;
    if (panes[type].h <= 0 || cell_heights[type] <= 0)
        return 0;

    return panes[type].h / cell_heights[type];
}

static bool sdl_mobile_enabled_panes_fit(const struct pane_config* configs,
    int count, const SDL_Rect* panes, const int* cell_widths,
    const int* cell_heights)
{
    if (!configs || !panes || !cell_widths || !cell_heights)
        return false;

    for (int i = 0; i < count; i++) {
        int cols;
        int rows;
        int min_cols;
        int min_rows;

        if (!configs[i].enabled)
            continue;
        if (configs[i].pane <= PANE_MAIN || configs[i].pane >= PANE_MAX)
            continue;

        cols = sdl_mobile_pane_cols(panes, cell_widths, configs[i].pane);
        rows = sdl_mobile_pane_rows(panes, cell_heights, configs[i].pane);
        if (pane_placement_is_side(configs[i].where)) {
            min_cols = pane_primary_min_cells(configs[i].pane, configs[i].where);
            min_rows = pane_secondary_min_cells(configs[i].pane, configs[i].where);
        } else {
            min_cols = pane_secondary_min_cells(configs[i].pane, configs[i].where);
            min_rows = pane_primary_min_cells(configs[i].pane, configs[i].where);
        }

        if (cols < min_cols || rows < min_rows)
            return false;
    }

    return true;
}

static bool sdl_mobile_layout_fits(const SDL_Rect* screen, int scale,
    const struct pane_config* configs, int count, SDL_Rect* out_panes,
    int* out_cell_widths, int* out_cell_heights, int* out_main_cols,
    int* out_main_rows)
{
    struct pane_config active[MAX_PANE_CONFIGS] = { 0 };
    SDL_Rect panes[PANE_MAX] = { 0 };
    int cell_widths[PANE_MAX] = { 0 };
    int cell_heights[PANE_MAX] = { 0 };
    int margin_px;
    int saved_scale;
    int main_cols;
    int main_rows;
    bool fits;

    if (!screen || !configs || count < 0)
        return false;

    if (count > MAX_PANE_CONFIGS)
        count = MAX_PANE_CONFIGS;

    memcpy(active, configs, sizeof(struct pane_config) * count);

    saved_scale = config.main_view_scale;
    config.main_view_scale = scale;

    sdl_build_supporting_pane_metrics(active, count, cell_widths, cell_heights);
    margin_px = (int)(g_state.system_scale * config.margin);
    sdl_apply_dynamic_auto_pane_sizes(active, count, screen, cell_widths,
        cell_heights, margin_px);
    place_panes(active, count, panes, screen, cell_widths, cell_heights,
        margin_px);

    main_cols = sdl_mobile_pane_cols(panes, cell_widths, PANE_MAIN);
    main_rows = sdl_mobile_pane_rows(panes, cell_heights, PANE_MAIN);
    fits = (main_cols >= sdl_current_min_terminal_cols()
        && main_rows >= sdl_current_min_terminal_rows()
        && sdl_mobile_enabled_panes_fit(active, count, panes, cell_widths,
            cell_heights));

    config.main_view_scale = saved_scale;

    if (out_panes)
        memcpy(out_panes, panes, sizeof(panes));
    if (out_cell_widths)
        memcpy(out_cell_widths, cell_widths, sizeof(cell_widths));
    if (out_cell_heights)
        memcpy(out_cell_heights, cell_heights, sizeof(cell_heights));
    if (out_main_cols)
        *out_main_cols = main_cols;
    if (out_main_rows)
        *out_main_rows = main_rows;

    return fits;
}

static int sdl_mobile_select_default_scale(const SDL_Rect* screen,
    const struct pane_config* configs, int count)
{
    int max_scale;

    if (!screen)
        return 1;

    max_scale = sdl_max_scale_for_rect(screen);
    for (int scale = max_scale; scale >= 1; scale--) {
        if (sdl_mobile_layout_fits(screen, scale, configs, count, NULL, NULL,
                NULL, NULL, NULL))
            return scale;
    }

    return 1;
}

static void sdl_apply_mobile_default_pane_layout(const SDL_Rect* screen,
    bool has_controller)
{
    struct pane_config selected[MAX_PANE_CONFIGS] = { 0 };
    struct pane_config candidate[MAX_PANE_CONFIGS] = { 0 };
    SDL_Rect panes[PANE_MAX] = { 0 };
    int cell_widths[PANE_MAX] = { 0 };
    int cell_heights[PANE_MAX] = { 0 };
    bool touch_enabled;
    bool have_bottom = false;
    bool wide_bottom = false;
    bool inventory_enabled = false;
    bool worn_enabled = false;
    int bottom_rows = 0;
    int final_main_cols = 0;
    int final_main_rows = 0;

    if (!screen)
        return;

    config.min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;

    memcpy(selected, pane_config, sizeof(selected));
    sdl_mobile_reset_default_pane_configs(selected, pane_config_count);

    touch_enabled = !has_controller;
    sdl_mobile_set_touch_pane_enabled(selected, pane_config_count,
        touch_enabled);

    config.main_view_scale = sdl_mobile_select_default_scale(screen, selected,
        pane_config_count);

    for (int rows = 4; rows >= 1; rows--) {
        memcpy(candidate, selected, sizeof(candidate));
        sdl_mobile_configure_bottom_wide(candidate, pane_config_count, rows);

        if (!sdl_mobile_layout_fits(screen, config.main_view_scale, candidate,
                pane_config_count, panes, cell_widths, cell_heights, NULL,
                NULL))
            continue;
        if (sdl_mobile_pane_cols(panes, cell_widths, PANE_ROLLS) < 65)
            continue;
        if (sdl_mobile_pane_cols(panes, cell_widths, PANE_LOG) < 50)
            continue;

        memcpy(selected, candidate, sizeof(selected));
        have_bottom = true;
        wide_bottom = true;
        bottom_rows = rows;
        break;
    }

    if (!have_bottom) {
        for (int total_rows = 7; total_rows >= 1; total_rows--) {
            memcpy(candidate, selected, sizeof(candidate));
            sdl_mobile_configure_bottom_narrow(candidate, pane_config_count,
                total_rows);

            if (!sdl_mobile_layout_fits(screen, config.main_view_scale,
                    candidate, pane_config_count, NULL, NULL, NULL, NULL,
                    NULL))
                continue;

            memcpy(selected, candidate, sizeof(selected));
            have_bottom = true;
            bottom_rows = total_rows;
            break;
        }
    }

    memcpy(candidate, selected, sizeof(candidate));
    sdl_mobile_set_right_panes(candidate, pane_config_count, true, false);
    if (sdl_mobile_layout_fits(screen, config.main_view_scale, candidate,
            pane_config_count, NULL, NULL, NULL, NULL, NULL))
    {
        memcpy(selected, candidate, sizeof(selected));
        inventory_enabled = true;
    }

    if (inventory_enabled) {
        memcpy(candidate, selected, sizeof(candidate));
        sdl_mobile_set_right_panes(candidate, pane_config_count, true, true);
        if (sdl_mobile_layout_fits(screen, config.main_view_scale, candidate,
                pane_config_count, NULL, NULL, NULL, NULL, NULL))
        {
            memcpy(selected, candidate, sizeof(selected));
            worn_enabled = true;
        }
    }

    memcpy(pane_config, selected, sizeof(selected));
    config.enable_bottom_panes = have_bottom;
    config.enable_right_panes = (touch_enabled || inventory_enabled
        || worn_enabled);

    sdl_mobile_layout_fits(screen, config.main_view_scale, pane_config,
        pane_config_count, panes, cell_widths, cell_heights, &final_main_cols,
        &final_main_rows);

    log_info("Handheld default pane layout: controller=%s touch=%s scale=%d main=%dx%d",
        has_controller ? "yes" : "no",
        touch_enabled ? "on" : "off",
        config.main_view_scale, final_main_cols, final_main_rows);
    if (have_bottom) {
        log_info("Handheld default bottom panes: %s layout, %d row%s",
            wide_bottom ? "split" : "stacked",
            bottom_rows, (bottom_rows == 1) ? "" : "s");
    } else {
        log_info("Handheld default bottom panes: off");
    }
    log_info("Handheld default right panes: inventory=%s worn=%s",
        inventory_enabled ? "on" : "off",
        worn_enabled ? "on" : "off");
}
#endif

static bool sdl_touch_pane_is_config_enabled(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_TOUCH)
            return pane_config[i].enabled;
    }
    return false;
}

static bool sdl_touch_pane_panel_is_valid(int panel)
{
    return (panel >= 0 && panel < SDL_TOUCH_PANE_PANEL_COUNT);
}

static int sdl_touch_pane_active_panel(void)
{
    return g_touch_pane_second_panel ? SDL_TOUCH_PANE_PANEL_SECOND : SDL_TOUCH_PANE_PANEL_MAIN;
}

static int sdl_touch_pane_other_panel(int panel)
{
    return (panel == SDL_TOUCH_PANE_PANEL_SECOND) ? SDL_TOUCH_PANE_PANEL_MAIN
                                                  : SDL_TOUCH_PANE_PANEL_SECOND;
}

static int sdl_touch_pane_raw_binding_for_panel(int panel, int index)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return GAMEPAD_BIND_NONE;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        return config.touch_pane_second_bindings[index];

    return config.touch_pane_bindings[index];
}

static int sdl_touch_pane_effective_binding_for_panel(int panel, int index)
{
    int binding;

    binding = sdl_touch_pane_raw_binding_for_panel(panel, index);
    if (panel == SDL_TOUCH_PANE_PANEL_SECOND && binding == TOUCH_PANE_BIND_INHERIT)
        return config.touch_pane_bindings[index];

    return binding;
}

static void sdl_update_cursor_visibility(void)
{
    bool show_cursor = true;
    bool show_supporting_panes = sdl_should_show_supporting_panes();

    if (config.fullscreen) {
        SDL_Rect panes[PANE_MAX];
        const SDL_Rect* touch_pane;

        if (show_supporting_panes || sdl_layout_matches_supporting_pane_visibility()) {
            touch_pane = &g_pane_rects[PANE_TOUCH];
        } else {
            sdl_compute_display_panes(panes);
            touch_pane = &panes[PANE_TOUCH];
        }

        show_cursor = (touch_pane->w > 0 && sdl_touch_pane_is_config_enabled());
    }

    if (show_cursor)
        SDL_ShowCursor();
    else
        SDL_HideCursor();
}

static int sdl_build_active_pane_config(struct pane_config* active, bool include_side,
    bool include_bottom, bool touch_only)
{
    int active_count = 0;

    for (int i = 0; i < pane_config_count && active_count < MAX_PANE_CONFIGS; i++) {
        enum pane_placement where = pane_config[i].where;
        bool is_touch_pane = (pane_config[i].pane == PANE_TOUCH);

        if (touch_only && !is_touch_pane)
            continue;
        if (!is_touch_pane) {
            if (pane_placement_is_side(where) && !include_side)
                continue;
            if (pane_placement_is_bottom(where) && !include_bottom)
                continue;
        }

        active[active_count++] = pane_config[i];
    }

    return active_count;
}

static int sdl_auto_aux_view_font_size(void)
{
    float system_scale = (g_state.system_scale > 0.0f) ? g_state.system_scale : 1.0f;
    int main_cell_h_px = config.main_view_scale * TILE_SIZE;
    int main_font_size;
    int size;

    if (g_auto_aux_main_cell_h_override > 0)
        main_cell_h_px = g_auto_aux_main_cell_h_override;
    else if (g_views[0].term_ready && g_views[0].cell_h > 0)
        main_cell_h_px = g_views[0].cell_h;

    main_font_size = (int)((float)main_cell_h_px / system_scale + 0.5f);
    size = (main_font_size * 3 + 3) / 4;

    if (size >= main_font_size && main_font_size > 8)
        size = main_font_size - 1;

    if (size < 8)
        size = 8;
    if (size > 48)
        size = 48;

    return size;
}

static int sdl_resolve_aux_view_font_size(int requested_size)
{
    int size = requested_size;

    if (size <= 0)
        size = sdl_auto_aux_view_font_size();
    if (size < 8)
        size = 8;
    if (size > 48)
        size = 48;

    return size;
}

static int sdl_effective_pane_font_size_for_config(const struct pane_config* pc)
{
    if (pc && pc->font_size > 0)
        return sdl_resolve_aux_view_font_size(pc->font_size);

    return sdl_resolve_aux_view_font_size(config.aux_view_font_size);
}

static int sdl_effective_pane_font_size_for_type(enum pane_type type)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == type)
            return sdl_effective_pane_font_size_for_config(&pane_config[i]);
    }

    return sdl_resolve_aux_view_font_size(config.aux_view_font_size);
}

static void sdl_build_supporting_pane_metrics(const struct pane_config* configs,
    int count, int* cell_widths, int* cell_heights)
{
    int default_font_size = sdl_resolve_aux_view_font_size(config.aux_view_font_size);
    int default_cell_h = (int)(g_state.system_scale * default_font_size);
    int default_cell_w;

    if (!cell_widths || !cell_heights)
        return;

    if (default_cell_h < 1)
        default_cell_h = 1;
    default_cell_w = default_cell_h / 2;
    if (default_cell_w < 1)
        default_cell_w = 1;

    for (int i = 0; i < PANE_MAX; i++) {
        cell_widths[i] = default_cell_w;
        cell_heights[i] = default_cell_h;
    }

    cell_widths[PANE_MAIN] = config.main_view_scale * TILE_SIZE / 2;
    cell_heights[PANE_MAIN] = config.main_view_scale * TILE_SIZE;

    for (int i = 0; i < count; i++) {
        enum pane_type type = configs[i].pane;
        int font_size;
        int cell_h;

        if (type <= PANE_MAIN || type >= PANE_MAX)
            continue;

        font_size = sdl_effective_pane_font_size_for_config(&configs[i]);
        cell_h = (int)(g_state.system_scale * font_size);
        if (cell_h < 1)
            cell_h = 1;
        cell_heights[type] = cell_h;
        cell_widths[type] = cell_h / 2;
        if (cell_widths[type] < 1)
            cell_widths[type] = 1;
    }
}

static bool sdl_prune_unusable_panes(struct pane_config* active,
    int active_count, SDL_Rect* panes, const int* cell_widths,
    const int* cell_heights)
{
    bool pruned = false;

    for (int i = 0; i < active_count; i++) {
        struct pane_config* pc = &active[i];
        enum pane_type type = pc->pane;
        SDL_Rect* rect;
        int cell_w;
        int cell_h;
        int cols;
        int rows;
        int min_cols;
        int min_rows;

        if (!pc->enabled)
            continue;
        if (type <= PANE_MAIN || type >= PANE_MAX)
            continue;

        rect = &panes[type];
        if (rect->w <= 0 || rect->h <= 0)
            continue;

        cell_w = cell_widths[type];
        cell_h = cell_heights[type];
        if (cell_w <= 0 || cell_h <= 0) {
            pc->enabled = false;
            *rect = (SDL_Rect){ 0 };
            pruned = true;
            continue;
        }

        cols = rect->w / cell_w;
        rows = rect->h / cell_h;
        if (pane_placement_is_side(pc->where)) {
            min_cols = pane_primary_min_cells(type, pc->where);
            min_rows = pane_secondary_min_cells(type, pc->where);
        } else {
            min_cols = pane_secondary_min_cells(type, pc->where);
            min_rows = pane_primary_min_cells(type, pc->where);
        }

        if (cols >= min_cols && rows >= min_rows)
            continue;

        log_info("Skipping pane %d at (%d,%d) size %dx%d: fits %dx%d cells, needs %dx%d",
            type, rect->x, rect->y, rect->w, rect->h, cols, rows, min_cols, min_rows);
        pc->enabled = false;
        *rect = (SDL_Rect){ 0 };
        pruned = true;
    }

    return pruned;
}

static int sdl_touch_pane_target_width_px(int pane_height_px)
{
    const int numerator = 40 * SDL_TOUCH_PANE_BUTTON_COLS;
    const int denominator = 39 * SDL_TOUCH_PANE_BUTTON_ROWS
        + SDL_TOUCH_PANE_BUTTON_COLS;

    if (pane_height_px <= 0)
        return 0;

    return (pane_height_px * numerator + denominator - 1) / denominator;
}

static void sdl_apply_dynamic_auto_pane_sizes(struct pane_config* active,
    int active_count, const SDL_Rect* screen, const int* cell_widths,
    const int* cell_heights, int margin_px)
{
    SDL_Rect temp_panes[PANE_MAX] = { 0 };
    int touch_idx = -1;
    int min_touch_cols;
    int min_touch_width_px;
    int desired_touch_px;
    int desired_touch_cols;
    int max_touch_px;
    int max_touch_cols;
    int min_main_width_px;

    if (!active || active_count <= 0 || !screen || !cell_widths || !cell_heights)
        return;

    for (int i = 0; i < active_count; i++) {
        if (!active[i].enabled)
            continue;
        if (active[i].pane != PANE_TOUCH)
            continue;
        if (!pane_placement_is_side(active[i].where))
            continue;
        if (active[i].rect.cols > 0)
            continue;

        touch_idx = i;
        break;
    }

    if (touch_idx < 0)
        return;

    place_panes(active, active_count, temp_panes, screen, cell_widths,
        cell_heights,
        margin_px);

    if (temp_panes[PANE_TOUCH].w <= 0 || temp_panes[PANE_TOUCH].h <= 0)
        return;

    min_touch_cols = pane_primary_min_cells(PANE_TOUCH, active[touch_idx].where);
    min_touch_width_px = min_touch_cols * cell_widths[PANE_TOUCH] + margin_px;
    desired_touch_px = sdl_touch_pane_target_width_px(temp_panes[PANE_TOUCH].h);
    if (desired_touch_px < min_touch_width_px)
        desired_touch_px = min_touch_width_px;

    desired_touch_cols = (desired_touch_px > margin_px)
        ? ((desired_touch_px - margin_px + cell_widths[PANE_TOUCH] - 1)
            / cell_widths[PANE_TOUCH])
        : min_touch_cols;
    if (desired_touch_cols < min_touch_cols)
        desired_touch_cols = min_touch_cols;

    min_main_width_px = sdl_current_min_terminal_cols() * cell_widths[PANE_MAIN];
    max_touch_px = temp_panes[PANE_MAIN].w + temp_panes[PANE_TOUCH].w
        - min_main_width_px;

    if (max_touch_px >= min_touch_width_px) {
        max_touch_cols = (max_touch_px > margin_px)
            ? ((max_touch_px - margin_px) / cell_widths[PANE_TOUCH])
            : 0;
        if (max_touch_cols < min_touch_cols)
            max_touch_cols = min_touch_cols;
        if (desired_touch_cols > max_touch_cols)
            desired_touch_cols = max_touch_cols;
    }

    active[touch_idx].rect.cols = desired_touch_cols;
}

static void sdl_place_active_panes(const SDL_Rect* screen, SDL_Rect* panes,
    bool include_side, bool include_bottom, bool touch_only)
{
    struct pane_config active[MAX_PANE_CONFIGS] = { 0 };
    int active_count;
    int cell_widths[PANE_MAX] = { 0 };
    int cell_heights[PANE_MAX] = { 0 };
    int margin_px;

    if (!screen || !panes)
        return;

    memset(panes, 0, sizeof(SDL_Rect) * PANE_MAX);

    margin_px = (int)(g_state.system_scale * config.margin);
    active_count = sdl_build_active_pane_config(active, include_side,
        include_bottom, touch_only);
    sdl_build_supporting_pane_metrics(active, active_count, cell_widths,
        cell_heights);
    sdl_apply_dynamic_auto_pane_sizes(active, active_count, screen, cell_widths,
        cell_heights, margin_px);

    for (int attempt = 0; attempt <= active_count; attempt++) {
        place_panes(active, active_count, panes, screen, cell_widths,
            cell_heights, margin_px);
        if (!sdl_prune_unusable_panes(active, active_count, panes, cell_widths,
                cell_heights))
            break;
        memset(panes, 0, sizeof(SDL_Rect) * PANE_MAX);
    }
}

static void sdl_compute_split_panes(const SDL_Rect* screen, SDL_Rect* panes)
{
    sdl_place_active_panes(screen, panes, config.enable_right_panes,
        config.enable_bottom_panes, false);
}

static bool sdl_hide_supporting_panes_mode_effective(void)
{
    /* Startup hidden mode must be able to take effect before persistent
     * options have been loaded into op_ptr. */
    if (!screen_startup_supporting_panes_hidden_active()
        && op_ptr && !op_ptr->opt[OPT_hide_supporting_panes_fullscreen])
        return false;

    for (int i = 0; i < pane_config_count; i++) {
        enum pane_placement where = pane_config[i].where;

        if (!pane_config[i].enabled)
            continue;
        if (pane_config[i].pane == PANE_MAIN || pane_config[i].pane == PANE_TOUCH)
            continue;
        if (pane_placement_is_side(where) && !g_active_side_panes)
            continue;
        if (pane_placement_is_bottom(where) && !g_active_bottom_panes)
            continue;

        return true;
    }

    return false;
}

static void sdl_draw_pane_edges(const SDL_Rect* rect, bool draw_left,
    bool draw_top, bool draw_right, bool draw_bottom)
{
    int x1;
    int y1;
    int x2;
    int y2;

    if (!rect || rect->w <= 0 || rect->h <= 0)
        return;

    x1 = rect->x;
    y1 = rect->y;
    x2 = rect->x + rect->w - 1;
    y2 = rect->y + rect->h - 1;

    if (x2 < x1 || y2 < y1)
        return;

    if (draw_top)
        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = (float)x1,
            .y = (float)y1,
            .w = (float)(x2 - x1 + 1),
            .h = 1.0f,
        });
    if (draw_bottom)
        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = (float)x1,
            .y = (float)y2,
            .w = (float)(x2 - x1 + 1),
            .h = 1.0f,
        });

    {
        int top_offset = draw_top ? 1 : 0;
        int bottom_offset = draw_bottom ? 1 : 0;
        int edge_height = y2 - y1 + 1 - top_offset - bottom_offset;

        if (edge_height > 0 && draw_left)
            SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
                .x = (float)x1,
                .y = (float)(y1 + top_offset),
                .w = 1.0f,
                .h = (float)edge_height,
            });
        if (edge_height > 0 && draw_right)
            SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
                .x = (float)x2,
                .y = (float)(y1 + top_offset),
                .w = 1.0f,
                .h = (float)edge_height,
            });
    }
}

static bool sdl_should_show_supporting_panes(void)
{
    if (!sdl_hide_supporting_panes_mode_effective())
        return true;
    if (screen_startup_supporting_panes_hidden_active())
        return false;
    if (screen_supporting_panes_hidden_active())
        return false;
    if (screen_saved_fullscreen_active())
        return false;

    return true;
}

static bool sdl_layout_matches_supporting_pane_visibility(void)
{
    return (g_supporting_panes_layout_visible == sdl_should_show_supporting_panes());
}

static void sdl_compute_display_panes(SDL_Rect* panes)
{
    SDL_Rect screen;

    if (!panes)
        return;

    if (!g_state.window || sdl_should_show_supporting_panes()) {
        memcpy(panes, g_pane_rects, sizeof(g_pane_rects));
        return;
    }

    screen = sdl_get_layout_screen_rect();
    sdl_place_active_panes(&screen, panes, g_active_side_panes,
        g_active_bottom_panes, true);
}

static int sdl_max_scale_for_rect_mode(const SDL_Rect* rect, int mode)
{
    int min_cols;
    int min_rows;
    int max_scale_w;
    int max_scale_h;
    int max_scale;

    if (!rect)
        return 1;

    min_cols = sdl_min_terminal_cols_for_mode(mode);
    min_rows = sdl_min_terminal_rows_for_mode(mode);
    max_scale_w = (rect->w / min_cols) * 2 / TILE_SIZE;
    max_scale_h = rect->h / min_rows / TILE_SIZE;
    max_scale = (max_scale_w < max_scale_h) ? max_scale_w : max_scale_h;

    if (max_scale < 1)
        max_scale = 1;
    if (max_scale > 20)
        max_scale = 20;

    return max_scale;
}

static int sdl_max_scale_for_rect(const SDL_Rect* rect)
{
    return sdl_max_scale_for_rect_mode(rect, config.min_terminal_mode);
}

static int sdl_max_scale_for_window_mode(int mode)
{
    struct sdl_config saved_config = config;
    struct pane_config saved_panes[MAX_PANE_CONFIGS];
    SDL_Rect screen;
    SDL_Rect panes[PANE_MAX];
    int saved_pane_count = pane_config_count;
    int max_scale = 1;

    memcpy(saved_panes, pane_config, sizeof(saved_panes));

    config.min_terminal_mode = mode;
    sdl_apply_stored_pane_profile(mode);
    config.min_terminal_mode = mode;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    if (sdl_rect_has_area(&screen)) {
        sdl_compute_split_panes(&screen, panes);
        max_scale = sdl_max_scale_for_rect_mode(&panes[PANE_MAIN], mode);
    }

    config = saved_config;
    pane_config_count = saved_pane_count;
    memcpy(pane_config, saved_panes, sizeof(saved_panes));

    return max_scale;
}

static bool sdl_mode_scale_fits_window(const SDL_Rect* screen, int mode,
    int scale, int* cols, int* rows)
{
    struct sdl_config saved_config = config;
    struct pane_config saved_panes[MAX_PANE_CONFIGS];
    SDL_Rect panes[PANE_MAX];
    int saved_pane_count = pane_config_count;
    int local_cols = 0;
    int local_rows = 0;
    bool fits = false;

    if (!screen || !sdl_rect_has_area(screen) || scale <= 0)
        return false;

    memcpy(saved_panes, pane_config, sizeof(saved_panes));

    config.min_terminal_mode = mode;
    sdl_apply_stored_pane_profile(mode);
    config.min_terminal_mode = mode;
    config.main_view_scale = scale;
    sdl_compute_split_panes(screen, panes);

    local_cols = panes[PANE_MAIN].w / (scale * TILE_SIZE / 2);
    local_rows = panes[PANE_MAIN].h / (scale * TILE_SIZE);
    fits = (local_cols >= sdl_min_terminal_cols_for_mode(mode)
        && local_rows >= sdl_min_terminal_rows_for_mode(mode));

    config = saved_config;
    pane_config_count = saved_pane_count;
    memcpy(pane_config, saved_panes, sizeof(saved_panes));

    if (cols)
        *cols = local_cols;
    if (rows)
        *rows = local_rows;

    return fits;
}

static void sdl_ensure_window_size_for_min_terminal(const SDL_Rect* screen,
    int* window_width, int* window_height)
{
    int min_width;
    int min_height;

    if (!screen || !window_width || !window_height || config.fullscreen)
        return;

    min_width = sdl_current_min_terminal_cols() * (TILE_SIZE / 2);
    min_height = sdl_current_min_terminal_rows() * TILE_SIZE;

    if (min_width < 1)
        min_width = 1;
    if (min_height < 1)
        min_height = 1;

    if (screen->w > 0 && min_width > screen->w)
        min_width = screen->w;
    if (screen->h > 0 && min_height > screen->h)
        min_height = screen->h;

    if (*window_width < min_width) {
        log_info("Increasing initial window width from %d to %d to fit minimum terminal %dx%d (%s)",
            *window_width, min_width,
            sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows(),
            sdl_min_terminal_mode_name(config.min_terminal_mode));
        *window_width = min_width;
    }

    if (*window_height < min_height) {
        log_info("Increasing initial window height from %d to %d to fit minimum terminal %dx%d (%s)",
            *window_height, min_height,
            sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows(),
            sdl_min_terminal_mode_name(config.min_terminal_mode));
        *window_height = min_height;
    }
}

static void sdl_format_layout_recovery_message(const char* reason,
    const sdl_layout_recovery_result* recovery, char* buf, size_t buflen)
{
    const char* prefix = "Layout recovery";

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!recovery)
        return;

    if (reason && reason[0]) {
        if (streq(reason, "startup"))
            prefix = "At startup";
        else if (streq(reason, "window resize"))
            prefix = "Window resize";
        else if (streq(reason, "display scale change"))
            prefix = "Display scale change";
        else if (streq(reason, "fullscreen change"))
            prefix = "Fullscreen change";
        else if (streq(reason, "settings change"))
            prefix = "Settings change";
        else
            prefix = reason;
    }

    if (recovery->mode_changed && recovery->scale_changed) {
        strnfmt(buf, buflen,
            "%s: switched to %s terminal layout and reduced main view scale from %d to %d to keep the window usable.",
            prefix, sdl_min_terminal_mode_name(recovery->new_mode),
            recovery->old_scale, recovery->new_scale);
    } else if (recovery->mode_changed) {
        strnfmt(buf, buflen,
            "%s: switched from %s to %s terminal layout to fit the current window.",
            prefix, sdl_min_terminal_mode_name(recovery->old_mode),
            sdl_min_terminal_mode_name(recovery->new_mode));
    } else if (recovery->scale_changed) {
        strnfmt(buf, buflen,
            "%s: reduced main view scale from %d to %d to keep the %s terminal visible.",
            prefix, recovery->old_scale, recovery->new_scale,
            sdl_min_terminal_mode_name(recovery->new_mode));
    }
}

static void sdl_append_issue_line(char* buf, size_t buflen, const char* line)
{
    if (!buf || !buflen || !line || !line[0])
        return;

    if (buf[0])
        SDL_strlcat(buf, "\n", buflen);
    SDL_strlcat(buf, line, buflen);
}

static bool sdl_recover_layout_for_current_window(const char* reason,
    bool notify_user, sdl_layout_recovery_result* recovery)
{
    SDL_Rect screen;
    sdl_layout_recovery_result local = {
        .mode_changed = false,
        .scale_changed = false,
        .old_mode = config.min_terminal_mode,
        .new_mode = config.min_terminal_mode,
        .old_scale = config.main_view_scale,
        .new_scale = config.main_view_scale,
    };
    char notice[256];

    if (!g_state.window)
        return false;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return false;

    if (config.min_terminal_mode == SDL_MIN_TERMINAL_NORMAL
        && !sdl_mode_scale_fits_window(&screen, SDL_MIN_TERMINAL_NORMAL,
            config.main_view_scale, NULL, NULL))
    {
        log_info("%s: normal minimum terminal no longer fits; activating compact layout",
            reason ? reason : "layout change");
        set_sdl_min_terminal_mode(SDL_MIN_TERMINAL_COMPACT);
        local.mode_changed = true;
        local.new_mode = config.min_terminal_mode;
        local.new_scale = config.main_view_scale;
    }

    if (!sdl_mode_scale_fits_window(&screen, config.min_terminal_mode,
            config.main_view_scale, NULL, NULL))
    {
        int max_scale = sdl_max_scale_for_window_mode(config.min_terminal_mode);

        if (config.main_view_scale > max_scale) {
            log_info("%s: clamping main_view_scale from %d to %d for %s minimum terminal",
                reason ? reason : "layout change",
                config.main_view_scale, max_scale,
                sdl_min_terminal_mode_name(config.min_terminal_mode));
            config.main_view_scale = max_scale;
            local.scale_changed = true;
            local.new_scale = config.main_view_scale;
        }
    }

    if (recovery)
        *recovery = local;

    if (!(local.mode_changed || local.scale_changed))
        return false;

    if (notify_user && Term) {
        sdl_format_layout_recovery_message(reason, &local, notice,
            sizeof(notice));
        if (notice[0])
            msg_print(notice);
    }

    return true;
}

static bool sdl_prompt_reset_sdl_defaults(const char* issue_summary,
    int screen_width, int screen_height)
{
    enum {
        SDL_STARTUP_KEEP_RECOVERED = 0,
        SDL_STARTUP_LOAD_DEFAULTS = 1,
    };
    SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
            SDL_STARTUP_KEEP_RECOVERED, "Keep Recovered Settings" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
            SDL_STARTUP_LOAD_DEFAULTS, "Load Defaults" },
    };
    char message[SDL_STARTUP_ISSUE_MAX + 256];
    SDL_MessageBoxData messagebox = {
        .flags = SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT,
        .window = g_state.window,
        .title = "SDL Config Recovery",
        .message = message,
        .numbuttons = (int)(sizeof(buttons) / sizeof(buttons[0])),
        .buttons = buttons,
        .colorScheme = NULL,
    };
    int button_id = SDL_STARTUP_KEEP_RECOVERED;

    if (!issue_summary || !issue_summary[0])
        return false;

    strnfmt(message, sizeof(message),
        "Sil-more adjusted your SDL settings so the game can start:\n\n%s\n\nLoad default SDL settings now? You can keep the recovered settings and change them later from SDL Pane Settings.",
        issue_summary);

    if (!SDL_ShowMessageBox(&messagebox, &button_id)) {
        log_warn("SDL_ShowMessageBox failed during startup recovery prompt: %s",
            SDL_GetError());
        return false;
    }

    if (button_id != SDL_STARTUP_LOAD_DEFAULTS)
        return false;

    sdl_reset_config_to_resolution_defaults(screen_width, screen_height);
    sdl_config_save(config_file_path, &config, g_pane_profiles,
        SDL_PANE_PROFILE_COUNT);
    log_info("Startup recovery: reset SDL config to defaults at %s",
        config_file_path);
    return true;
}

static bool sdl_resolution_matches_pair(int width, int height, int native_w,
    int native_h)
{
    return ((width == native_w && height == native_h)
        || (width == native_h && height == native_w));
}

static bool sdl_is_desktop_handheld_resolution(int width, int height)
{
#if SIL_SDL_DESKTOP_HANDHELD_BUILD
    /* Native panel sizes for current Windows/Linux handhelds, plus common
     * handheld performance-mode targets. Check both orientations. */
    return sdl_resolution_matches_pair(width, height, 1280, 720)
        || sdl_resolution_matches_pair(width, height, 1280, 800)
        || sdl_resolution_matches_pair(width, height, 1920, 1080)
        || sdl_resolution_matches_pair(width, height, 1920, 1200)
        || sdl_resolution_matches_pair(width, height, 2560, 1600);
#else
    (void)width;
    (void)height;
    return false;
#endif
}

static bool sdl_touch_pane_binding_is_direction(int binding)
{
    switch (binding) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '6':
    case '7':
    case '8':
    case '9':
        return true;
    default:
        return false;
    }
}

static bool sdl_touch_pane_slot_uses_long_press(int slot, int binding)
{
    return (slot == 0)
        || sdl_touch_pane_binding_is_direction(binding)
        || (binding == 'z')
        || sdl_touch_pane_confirm_binding(binding);
}

static bool sdl_touch_pane_confirm_binding(int binding)
{
    return (binding == INPUT_BIND_CONFIRM || binding == ' ' || binding == '\r');
}

static bool sdl_touch_pane_main_panel_has_confirm_excluding(int skip_index)
{
    for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++) {
        if (i == skip_index)
            continue;

        if (sdl_touch_pane_confirm_binding(config.touch_pane_bindings[i]))
            return true;
    }

    return false;
}

static void sdl_touch_pane_ensure_main_panel_confirm(void)
{
    if (sdl_touch_pane_main_panel_has_confirm_excluding(-1))
        return;

    if (config.touch_pane_bindings[SDL_TOUCH_PANE_CENTER_SLOT] != INPUT_BIND_CONFIRM)
        log_warn("Touch pane main panel had no Pick/Confirm binding; restoring center button");

    config.touch_pane_bindings[SDL_TOUCH_PANE_CENTER_SLOT] = INPUT_BIND_CONFIRM;
    clear_sdl_touch_pane_button_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN,
        SDL_TOUCH_PANE_CENTER_SLOT);
}

static void sdl_touch_pane_begin_reset_confirm(void)
{
    g_touch_pane_reset_confirm_active = true;
    g_state.need_present = true;
}

static void sdl_touch_pane_finish_reset_confirm(bool confirmed)
{
    g_touch_pane_reset_confirm_active = false;

    if (confirmed) {
        sdl_touch_pane_reset_bindings_to_default();
        msg_print("Touch controls reset to defaults.");
    }

    g_state.need_present = true;
}

static void sdl_touch_pane_handle_reset_prompt_pointer(float x, float y)
{
    int slot = -1;
    int panel;
    int binding;

    if (!sdl_touch_pane_point_to_slot(x, y, &slot))
        return;
    if (slot < 0)
        return;

    panel = sdl_touch_pane_active_panel();
    binding = sdl_touch_pane_effective_binding_for_panel(panel, slot);

    if (slot == 0 || binding == ESCAPE) {
        sdl_touch_pane_finish_reset_confirm(false);
    } else if (sdl_touch_pane_confirm_binding(binding)) {
        sdl_touch_pane_finish_reset_confirm(true);
    }
}

static bool sdl_touch_pane_compute_layout(const SDL_Rect* pane_rect, SDL_FRect* slot_rects)
{
    float gap;
    float usable_w;
    float usable_h;
    float button_from_w;
    float button_from_h;
    float button_size;
    float grid_w;
    float grid_h;
    float start_x;
    float start_y;

    if (!pane_rect || !slot_rects || pane_rect->w <= 0 || pane_rect->h <= 0)
        return false;

    gap = (float)((pane_rect->w < pane_rect->h) ? pane_rect->w : pane_rect->h) / 40.0f;
    if (gap < 4.0f)
        gap = 4.0f;
    if (gap > 12.0f)
        gap = 12.0f;

    usable_w = (float)pane_rect->w - gap * 2.0f;
    usable_h = (float)pane_rect->h - gap * 2.0f;
    button_from_w = (usable_w - gap * (SDL_TOUCH_PANE_BUTTON_COLS - 1)) / SDL_TOUCH_PANE_BUTTON_COLS;
    button_from_h = (usable_h - gap * (SDL_TOUCH_PANE_BUTTON_ROWS - 1)) / SDL_TOUCH_PANE_BUTTON_ROWS;
    button_size = (button_from_w < button_from_h) ? button_from_w : button_from_h;
    if (button_size < 12.0f)
        return false;

    grid_w = button_size * SDL_TOUCH_PANE_BUTTON_COLS + gap * (SDL_TOUCH_PANE_BUTTON_COLS - 1);
    grid_h = button_size * SDL_TOUCH_PANE_BUTTON_ROWS + gap * (SDL_TOUCH_PANE_BUTTON_ROWS - 1);
    start_x = (float)pane_rect->x + ((float)pane_rect->w - grid_w) * 0.5f;
    start_y = sdl_mobile_prefer_safe_edge_alignment()
        ? ((float)pane_rect->y + gap)
        : ((float)pane_rect->y + ((float)pane_rect->h - grid_h) * 0.5f);

    for (int row = 0; row < SDL_TOUCH_PANE_BUTTON_ROWS; row++) {
        for (int col = 0; col < SDL_TOUCH_PANE_BUTTON_COLS; col++) {
            int idx = row * SDL_TOUCH_PANE_BUTTON_COLS + col;
            slot_rects[idx] = (SDL_FRect){
                .x = start_x + col * (button_size + gap),
                .y = start_y + row * (button_size + gap),
                .w = button_size,
                .h = button_size,
            };
        }
    }

    return true;
}

static bool sdl_touch_pane_point_to_slot(float x, float y, int* out_slot)
{
    SDL_FRect slot_rects[SDL_TOUCH_PANE_BUTTON_COUNT];
    SDL_Rect panes[PANE_MAX];
    const SDL_Rect* pane;
    bool show_supporting_panes = sdl_should_show_supporting_panes();

    if (out_slot)
        *out_slot = -1;

    if (show_supporting_panes || sdl_layout_matches_supporting_pane_visibility()) {
        pane = &g_pane_rects[PANE_TOUCH];
    } else {
        sdl_compute_display_panes(panes);
        pane = &panes[PANE_TOUCH];
    }

    if (pane->w <= 0 || pane->h <= 0)
        return false;

    if (!sdl_touch_pane_compute_layout(pane, slot_rects))
        return false;

    for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++) {
        const SDL_FRect* rect = &slot_rects[i];
        if (x >= rect->x && x < rect->x + rect->w && y >= rect->y && y < rect->y + rect->h) {
            if (out_slot)
                *out_slot = i;
            return true;
        }
    }

    if (x >= pane->x && x < pane->x + pane->w && y >= pane->y && y < pane->y + pane->h)
        return true;

    return false;
}

static void sdl_touch_pane_draw_arrow(const SDL_FRect* rect, int binding, SDL_Color color)
{
    float dx = 0.0f;
    float dy = 0.0f;
    float px = 0.0f;
    float py = 0.0f;
    float cx;
    float cy;
    float body_len;
    float head_len;
    float tail_x;
    float tail_y;
    float tip_x;
    float tip_y;

    if (!rect)
        return;

    switch (binding) {
    case '7': dx = -0.70710677f; dy = -0.70710677f; break;
    case '8': dx = 0.0f; dy = -1.0f; break;
    case '9': dx = 0.70710677f; dy = -0.70710677f; break;
    case '4': dx = -1.0f; dy = 0.0f; break;
    case '6': dx = 1.0f; dy = 0.0f; break;
    case '1': dx = -0.70710677f; dy = 0.70710677f; break;
    case '2': dx = 0.0f; dy = 1.0f; break;
    case '3': dx = 0.70710677f; dy = 0.70710677f; break;
    default:
        return;
    }

    px = -dy;
    py = dx;
    cx = rect->x + rect->w * 0.5f;
    cy = rect->y + rect->h * 0.5f;
    body_len = ((rect->w < rect->h) ? rect->w : rect->h) * 0.28f;
    head_len = ((rect->w < rect->h) ? rect->w : rect->h) * 0.16f;
    tail_x = cx - dx * body_len;
    tail_y = cy - dy * body_len;
    tip_x = cx + dx * body_len;
    tip_y = cy + dy * body_len;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(g_state.renderer, tail_x, tail_y, tip_x, tip_y);
    SDL_RenderLine(g_state.renderer, tip_x, tip_y,
        tip_x - dx * head_len + px * head_len * 0.55f,
        tip_y - dy * head_len + py * head_len * 0.55f);
    SDL_RenderLine(g_state.renderer, tip_x, tip_y,
        tip_x - dx * head_len - px * head_len * 0.55f,
        tip_y - dy * head_len - py * head_len * 0.55f);
}

static void sdl_touch_pane_draw_button_text(const SDL_FRect* rect, const char* name, const char* symbol,
    SDL_Color color)
{
    SDL_Surface* name_surface = NULL;
    SDL_Surface* symbol_surface = NULL;
    SDL_Texture* name_texture = NULL;
    SDL_Texture* symbol_texture = NULL;
    TTF_Font* name_font = NULL;
    TTF_Font* symbol_font = NULL;
    bool have_name;
    bool have_symbol;
    float name_max_w;
    float name_max_h;
    float symbol_max_w;
    float symbol_max_h;
    float gap;
    SDL_FRect name_dst;
    SDL_FRect symbol_dst;
    int name_font_px;
    int symbol_font_px;

    if (!rect)
        return;

    have_name = (name && name[0]);
    have_symbol = (symbol && symbol[0]);
    if (!have_name && !have_symbol)
        return;

    name_font_px = (int)(rect->h * 0.18f);
    if (name_font_px < 10)
        name_font_px = 10;

    symbol_font_px = (int)(rect->h * (have_name ? 0.22f : 0.28f));
    if (symbol_font_px < 12)
        symbol_font_px = 12;

    if (have_name) {
        name_font = sdl_story_font_for_height(name_font_px);
        if (name_font)
            name_surface = TTF_RenderText_Blended(name_font, name, 0, color);
    }

    if (have_symbol) {
        symbol_font = sdl_story_font_for_height(symbol_font_px);
        if (symbol_font)
            symbol_surface = TTF_RenderText_Blended(symbol_font, symbol, 0, color);
    }

    if (!name_surface && !symbol_surface)
        return;

    if (name_surface)
        name_texture = SDL_CreateTextureFromSurface(g_state.renderer, name_surface);
    if (symbol_surface)
        symbol_texture = SDL_CreateTextureFromSurface(g_state.renderer, symbol_surface);

    if (name_surface && !name_texture) {
        SDL_DestroySurface(name_surface);
        name_surface = NULL;
    }
    if (symbol_surface && !symbol_texture) {
        SDL_DestroySurface(symbol_surface);
        symbol_surface = NULL;
    }

    if (!name_surface && !symbol_surface)
        return;

    gap = rect->h * 0.03f;
    if (gap < 2.0f)
        gap = 2.0f;

    if (name_surface && symbol_surface) {
        float total_h;
        float avail_h;
        float name_scale_w;
        float name_scale_h;
        float name_scale;
        float symbol_scale_w;
        float symbol_scale_h;
        float symbol_scale;
        float scale;
        float name_h;
        float symbol_h;
        float start_y;

        name_max_w = rect->w * 0.82f;
        symbol_max_w = rect->w * 0.82f;
        avail_h = rect->h * 0.68f;
        name_max_h = rect->h * 0.22f;
        symbol_max_h = rect->h * 0.30f;

        name_scale_w = (name_surface->w > 0) ? (name_max_w / (float)name_surface->w) : 1.0f;
        name_scale_h = (name_surface->h > 0) ? (name_max_h / (float)name_surface->h) : 1.0f;
        name_scale = (name_scale_w < name_scale_h) ? name_scale_w : name_scale_h;
        if (name_scale > 1.0f)
            name_scale = 1.0f;

        symbol_scale_w = (symbol_surface->w > 0) ? (symbol_max_w / (float)symbol_surface->w) : 1.0f;
        symbol_scale_h = (symbol_surface->h > 0) ? (symbol_max_h / (float)symbol_surface->h) : 1.0f;
        symbol_scale = (symbol_scale_w < symbol_scale_h) ? symbol_scale_w : symbol_scale_h;
        if (symbol_scale > 1.0f)
            symbol_scale = 1.0f;

        total_h = (float)name_surface->h * name_scale + gap + (float)symbol_surface->h * symbol_scale;
        scale = 1.0f;
        if (total_h > avail_h && total_h > 0.0f)
            scale = avail_h / total_h;

        name_scale *= scale;
        symbol_scale *= scale;
        name_h = (float)name_surface->h * name_scale;
        symbol_h = (float)symbol_surface->h * symbol_scale;
        start_y = rect->y + (rect->h - (name_h + gap + symbol_h)) * 0.5f;

        name_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)name_surface->w * name_scale) * 0.5f,
            .y = start_y,
            .w = (float)name_surface->w * name_scale,
            .h = name_h,
        };
        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)symbol_surface->w * symbol_scale) * 0.5f,
            .y = start_y + name_h + gap,
            .w = (float)symbol_surface->w * symbol_scale,
            .h = symbol_h,
        };

        SDL_SetTextureBlendMode(name_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureBlendMode(symbol_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, name_texture, NULL, &name_dst);
        SDL_RenderTexture(g_state.renderer, symbol_texture, NULL, &symbol_dst);
    } else {
        SDL_Surface* only_surface = name_surface ? name_surface : symbol_surface;
        SDL_Texture* only_texture = name_surface ? name_texture : symbol_texture;
        float max_w = rect->w * 0.82f;
        float max_h = rect->h * 0.38f;
        float scale_w = (only_surface->w > 0) ? (max_w / (float)only_surface->w) : 1.0f;
        float scale_h = (only_surface->h > 0) ? (max_h / (float)only_surface->h) : 1.0f;
        float scale = (scale_w < scale_h) ? scale_w : scale_h;

        if (scale > 1.0f)
            scale = 1.0f;

        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)only_surface->w * scale) * 0.5f,
            .y = rect->y + (rect->h - (float)only_surface->h * scale) * 0.5f,
            .w = (float)only_surface->w * scale,
            .h = (float)only_surface->h * scale,
        };

        SDL_SetTextureBlendMode(only_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, only_texture, NULL, &symbol_dst);
    }

    if (name_texture)
        SDL_DestroyTexture(name_texture);
    if (symbol_texture)
        SDL_DestroyTexture(symbol_texture);
    if (name_surface)
        SDL_DestroySurface(name_surface);
    if (symbol_surface)
        SDL_DestroySurface(symbol_surface);
}

static void sdl_touch_pane_binding_symbol(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    switch (binding) {
    case GAMEPAD_BIND_NONE:
        return;
    case GAMEPAD_BIND_SHIFT:
        SDL_strlcpy(buf, "Shift", buflen);
        return;
    case GAMEPAD_BIND_CTRL:
        SDL_strlcpy(buf, "Ctrl", buflen);
        return;
    case GAMEPAD_BIND_ALT:
        SDL_strlcpy(buf, "Alt", buflen);
        return;
    case INPUT_BIND_CONFIRM:
    case ' ':
        SDL_strlcpy(buf, "Space", buflen);
        return;
    case '\r':
        SDL_strlcpy(buf, "Enter", buflen);
        return;
    case '\t':
        SDL_strlcpy(buf, "Tab", buflen);
        return;
    case ESCAPE:
        SDL_strlcpy(buf, "Esc", buflen);
        return;
    default:
        break;
    }

    if (binding >= 32 && binding <= 126) {
        buf[0] = (char)binding;
        buf[1] = '\0';
    }
}

static bool sdl_touch_pane_label_is_symbol_only(const char* label)
{
    return (label && label[0] == '\x01' && label[1] == '\0');
}

static bool sdl_touch_pane_should_hide_symbol(const char* name, const char* symbol)
{
    if (!name || !name[0] || !symbol || !symbol[0])
        return false;

    return (SDL_strcasecmp(name, symbol) == 0);
}

static void sdl_touch_pane_render_reset_prompt(void)
{
    SDL_Rect screen;
    SDL_FRect rect;
    SDL_Color frame = g_state.palette[TERM_L_BLUE];
    SDL_Color text = g_state.palette[TERM_WHITE];

    if (!g_touch_pane_reset_confirm_active)
        return;

    screen = sdl_get_layout_screen_rect();
    if (screen.w <= 0 || screen.h <= 0)
        return;

    rect = (SDL_FRect){
        .x = screen.x + screen.w * 0.10f,
        .y = screen.y + screen.h * 0.04f,
        .w = screen.w * 0.80f,
        .h = (screen.h < 600) ? 54.0f : 68.0f,
    };

    SDL_SetRenderDrawColor(g_state.renderer, 10, 10, 10, 235);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, 220);
    SDL_RenderRect(g_state.renderer, &rect);
    sdl_touch_pane_draw_button_text(&rect, "Reset touch controls to defaults?",
        "Confirm: Pick/Enter. Cancel: Esc.", text);
}

static void sdl_touch_pane_default_label_for_panel_slot(int panel, int index, char* buf, size_t buflen)
{
    int raw_binding;
    int binding;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    raw_binding = sdl_touch_pane_raw_binding_for_panel(panel, index);
    binding = sdl_touch_pane_effective_binding_for_panel(panel, index);

    if (raw_binding == TOUCH_PANE_BIND_INHERIT)
        return;

    if (binding == GAMEPAD_BIND_NONE) {
        SDL_strlcpy(buf, "Off", buflen);
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        int other_panel = sdl_touch_pane_other_panel(panel);

        sdl_touch_pane_load_default_bindings();
        if (config.touch_pane_panel_names[other_panel][0]) {
            SDL_strlcpy(buf, config.touch_pane_panel_names[other_panel], buflen);
        } else {
            SDL_strlcpy(buf, g_default_touch_pane_panel_names[other_panel], buflen);
        }
        return;
    }

    if (panel == SDL_TOUCH_PANE_PANEL_MAIN
        && binding == g_touch_pane_slots[index].default_binding
        && g_touch_pane_slots[index].default_label
        && g_touch_pane_slots[index].default_label[0]) {
        SDL_strlcpy(buf, g_touch_pane_slots[index].default_label, buflen);
        return;
    }

    if (binding == INPUT_BIND_CONFIRM) {
        SDL_strlcpy(buf, "Confirm", buflen);
        return;
    }

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND) {
        switch (index) {
        case 3:
            if (binding == '0') {
                SDL_strlcpy(buf, "Smith", buflen);
                return;
            }
            break;
        case 18:
            if (binding == 'L') {
                SDL_strlcpy(buf, "AltView", buflen);
                return;
            }
            break;
        case 7:
            if (binding == 'S') {
                SDL_strlcpy(buf, "Stealth", buflen);
                return;
            }
            break;
        case 8:
            if (binding == 'F') {
                SDL_strlcpy(buf, "Shoot 2", buflen);
                return;
            }
            break;
        case 19:
            if (binding == 'X') {
                SDL_strlcpy(buf, "Exch", buflen);
                return;
            }
            break;
        case 20:
            if (binding == 'p') {
                SDL_strlcpy(buf, "Play", buflen);
                return;
            }
            break;
        default:
            break;
        }
    }

    binding_action_short(binding, buf, buflen);
}

static void sdl_touch_pane_base_label_for_slot(int panel, int index, char* buf, size_t buflen)
{
    int raw_binding;
    const char* custom_label = NULL;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    raw_binding = sdl_touch_pane_raw_binding_for_panel(panel, index);
    custom_label = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? config.touch_pane_second_labels[index]
        : config.touch_pane_labels[index];

    if (sdl_touch_pane_label_is_symbol_only(custom_label)) {
        sdl_touch_pane_binding_symbol(sdl_touch_pane_effective_binding_for_panel(panel, index),
            buf, buflen);
        return;
    }

    if (custom_label[0]) {
        SDL_strlcpy(buf, custom_label, buflen);
        return;
    }

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND && raw_binding == TOUCH_PANE_BIND_INHERIT) {
        sdl_touch_pane_base_label_for_slot(SDL_TOUCH_PANE_PANEL_MAIN, index, buf, buflen);
        return;
    }

    sdl_touch_pane_default_label_for_panel_slot(panel, index, buf, buflen);
}

static void sdl_touch_pane_display_label_for_slot(int panel, int index, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    sdl_touch_pane_base_label_for_slot(panel, index, buf, buflen);
}

static void sdl_touch_pane_send_confirm_action(void)
{
    if (character_dungeon) {
        Term_keypress(' ');
        return;
    }

    Term_keypress('\r');
}

static void sdl_touch_pane_send_binding(int binding, bool second_panel, bool long_press)
{
    if (binding == GAMEPAD_BIND_NONE)
        return;

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

    sdl_gamepad_send_key(binding, false);
}

static void sdl_touch_pane_send_slot(int panel, int index, bool long_press)
{
    int binding;

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    binding = sdl_touch_pane_effective_binding_for_panel(panel, index);
    sdl_touch_pane_send_binding(binding, panel == SDL_TOUCH_PANE_PANEL_SECOND,
        long_press);
}

static int sdl_touch_swipe_index_for_keypad_dir(int dir)
{
    switch (dir) {
    case 8:
        return GAMEPAD_STICK_DIR_UP;
    case 2:
        return GAMEPAD_STICK_DIR_DOWN;
    case 4:
        return GAMEPAD_STICK_DIR_LEFT;
    case 6:
        return GAMEPAD_STICK_DIR_RIGHT;
    default:
        return -1;
    }
}

static float sdl_touch_swipe_threshold_px(void)
{
    int cell_px = (g_views[0].cell_w > g_views[0].cell_h) ? g_views[0].cell_w : g_views[0].cell_h;
    float threshold = (float)cell_px * 0.75f;

    if (threshold < TOUCH_SWIPE_MIN_DISTANCE_PX)
        threshold = TOUCH_SWIPE_MIN_DISTANCE_PX;
    if (threshold > TOUCH_SWIPE_MAX_DISTANCE_PX)
        threshold = TOUCH_SWIPE_MAX_DISTANCE_PX;

    return threshold;
}

static int sdl_touch_swipe_direction_for_delta(float dx, float dy, float threshold)
{
    float abs_x = (dx >= 0.0f) ? dx : -dx;
    float abs_y = (dy >= 0.0f) ? dy : -dy;

    if (abs_x < threshold && abs_y < threshold)
        return 0;

    if (abs_x >= abs_y)
        return (dx >= 0.0f) ? 6 : 4;

    return (dy >= 0.0f) ? 2 : 8;
}

static void sdl_touch_swipe_cancel(void)
{
    g_touch_swipe.active = false;
    g_touch_swipe.triggered = false;
    g_touch_swipe.finger_id = 0;
    g_touch_swipe.start_x = 0.0f;
    g_touch_swipe.start_y = 0.0f;
    g_touch_swipe.last_x = 0.0f;
    g_touch_swipe.last_y = 0.0f;
}

static bool sdl_touch_swipe_handle_pointer_down(float x, float y, SDL_FingerID finger_id)
{
    int slot = -1;

    if (!config.touch_swipe_enabled)
        return false;
    if (sdl_touch_pane_is_config_enabled() && sdl_touch_pane_point_to_slot(x, y, &slot))
        return false;

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

static bool sdl_touch_swipe_handle_pointer_motion(float x, float y, SDL_FingerID finger_id)
{
    int dir;
    int binding;
    int swipe_index;
    float dx;
    float dy;

    if (!g_touch_swipe.active || g_touch_swipe.finger_id != finger_id)
        return false;

    g_touch_swipe.last_x = x;
    g_touch_swipe.last_y = y;

    if (g_touch_swipe.triggered)
        return true;

    dx = x - g_touch_swipe.start_x;
    dy = y - g_touch_swipe.start_y;
    dir = sdl_touch_swipe_direction_for_delta(dx, dy, sdl_touch_swipe_threshold_px());
    if (!dir)
        return true;

    swipe_index = sdl_touch_swipe_index_for_keypad_dir(dir);
    if (swipe_index < 0)
        return true;

    binding = config.touch_swipe_bindings[swipe_index];
    if (binding != GAMEPAD_BIND_NONE)
        sdl_touch_pane_send_binding(binding, false, false);
    g_touch_swipe.triggered = true;
    return true;
}

static void sdl_touch_swipe_handle_pointer_up(float x, float y, SDL_FingerID finger_id)
{
    if (!sdl_touch_swipe_handle_pointer_motion(x, y, finger_id))
        return;

    sdl_touch_swipe_cancel();
}

static void sdl_touch_pane_cancel_press(void)
{
    if (!g_touch_pane_press.active && g_touch_pane_pressed_slot < 0)
        return;

    g_touch_pane_press.active = false;
    g_touch_pane_pressed_slot = -1;
    g_state.need_present = true;
}

static int sdl_touch_pane_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_touch_pane_press.active)
        return -1;

    elapsed = now_ns - g_touch_pane_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

static bool sdl_touch_pane_flush_pending_press(Uint64 now_ns)
{
    int slot;

    if (!g_touch_pane_press.active)
        return false;
    if (now_ns - g_touch_pane_press.start_time < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return false;

    slot = g_touch_pane_press.slot;
    sdl_touch_pane_cancel_press();
    if (slot == 0) {
        sdl_touch_pane_begin_reset_confirm();
    } else {
        int panel = g_touch_pane_press.panel;
        sdl_touch_pane_send_slot(panel, slot, true);
        g_touch_pane_flash_slot = slot;
        g_touch_pane_flash_until = SDL_GetTicksNS() + 150000000ULL;
        g_state.need_present = true;
    }
    return true;
}

static bool sdl_touch_pane_handle_pointer_down(float x, float y, bool mouse, SDL_FingerID finger_id)
{
    int slot = -1;
    int panel;
    int binding;

    if (!sdl_touch_pane_point_to_slot(x, y, &slot))
        return false;

    if (slot < 0)
        return true;

    panel = sdl_touch_pane_active_panel();
    binding = sdl_touch_pane_effective_binding_for_panel(panel, slot);

    if (sdl_touch_pane_slot_uses_long_press(slot, binding)) {
        sdl_touch_pane_cancel_press();
        g_touch_pane_press.active = true;
        g_touch_pane_press.mouse = mouse;
        g_touch_pane_press.finger_id = finger_id;
        g_touch_pane_press.panel = panel;
        g_touch_pane_press.slot = slot;
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

static void sdl_touch_pane_handle_pointer_up(bool mouse, SDL_FingerID finger_id)
{
    Uint64 press_time;
    bool ctrl_direction_override;
    int slot;
    int panel;

    if (!g_touch_pane_press.active)
        return;
    if (g_touch_pane_press.mouse != mouse)
        return;
    if (!mouse && g_touch_pane_press.finger_id != finger_id)
        return;

    press_time = SDL_GetTicksNS() - g_touch_pane_press.start_time;
    ctrl_direction_override = (press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL);
    slot = g_touch_pane_press.slot;
    panel = g_touch_pane_press.panel;
    sdl_touch_pane_cancel_press();
    if (slot == 0 && ctrl_direction_override) {
        sdl_touch_pane_begin_reset_confirm();
    } else {
        sdl_touch_pane_send_slot(panel, slot, ctrl_direction_override);
        g_touch_pane_flash_slot = slot;
        g_touch_pane_flash_until = SDL_GetTicksNS() + 150000000ULL;
        g_state.need_present = true;
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

static int sdl_gamepad_modifier_index(int binding)
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

static int sdl_gamepad_single_active_modifier(void)
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

static int sdl_gamepad_combo_binding_for_input(int modifier, int type, int id)
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

    if (down)
        (void)sdl_gamepad_resolve_pending_shoulder_with_modifier(binding);
}

static void sdl_send_macro_key(int key, bool shift, bool ctrl, bool alt)
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
    log_debug("send macro key=%d ^_%s%s%sx%x%x\r",
        key, ctrl ? "C" : "", shift ? "S" : "", alt ? "A" : "", key / 16, key % 16);
}

static int sdl_keymap_mode(void)
{
    if (!hjkl_movement && !angband_keyset)
        return KEYMAP_MODE_SIL;
    if (hjkl_movement && !angband_keyset)
        return KEYMAP_MODE_SIL_HJKL;
    if (!hjkl_movement && angband_keyset)
        return KEYMAP_MODE_ANGBAND;
    return KEYMAP_MODE_ANGBAND_HJKL;
}

static char sdl_direction_char_for_key(int key)
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

static int sdl_direction_for_key_char(char ch)
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

static bool sdl_send_modified_direction_action(int dir, char dir_ch, bool shift, bool ctrl, bool alt,
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
        action_key = '/';
        follow_key = (dir == 5) ? '5' : dir_ch;
    } else {
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

static bool sdl_try_send_modified_direction_key(int key, bool shift, bool ctrl, bool alt, bool gui)
{
    char dir_ch = sdl_direction_char_for_key(key);
    int dir = sdl_direction_for_key_char(dir_ch);

    if (!dir)
        return false;

    return sdl_send_modified_direction_action(dir, dir_ch, shift, ctrl, alt, gui);
}

static bool sdl_try_send_modified_direction_event(const SDL_KeyboardEvent* key_event)
{
    bool shift;
    bool alt;
    bool ctrl;
    bool gui;
    SDL_Keycode base_key;

    if (!key_event)
        return false;

    shift = key_event->mod & SDL_KMOD_SHIFT;
    alt = key_event->mod & SDL_KMOD_ALT;
    ctrl = key_event->mod & SDL_KMOD_CTRL;
    gui = key_event->mod & SDL_KMOD_GUI;

    if (sdl_try_send_modified_direction_key(key_event->key, shift, ctrl, alt, gui))
        return true;

    base_key = SDL_GetKeyFromScancode(key_event->scancode, SDL_KMOD_NONE, false);
    if (base_key != key_event->key
        && sdl_try_send_modified_direction_key(base_key, shift, ctrl, alt, gui))
    {
        return true;
    }

    return false;
}

static bool sdl_handle_global_layout_shortcut(const SDL_KeyboardEvent* key_event)
{
    SDL_Keycode key;

    if (!key_event)
        return false;

    if (!(key_event->mod & SDL_KMOD_ALT))
        return false;

    key = key_event->key;

    if (key == '+' || key == '=' || key == SDLK_KP_PLUS) {
        int current_scale = get_sdl_main_view_scale();
        int max_scale = get_sdl_max_scale();

        if (current_scale < max_scale) {
            set_sdl_main_view_scale(current_scale + 1);
            sdl_apply_config();
            if (character_dungeon)
                Term_keypress(KTRL('R'));
        }
        return true;
    }

    if (key == '-' || key == SDLK_KP_MINUS) {
        int current_scale = get_sdl_main_view_scale();

        if (current_scale > 1) {
            set_sdl_main_view_scale(current_scale - 1);
            sdl_apply_config();
            if (character_dungeon)
                Term_keypress(KTRL('R'));
        }
        return true;
    }

    if (key == 'i' || key == 'I') {
        bool enabled = get_sdl_enable_right_panes();

        set_sdl_enable_right_panes(!enabled);
        sdl_apply_config();
        if (character_dungeon)
            Term_keypress(KTRL('R'));
        return true;
    }

    if (key == 'l' || key == 'L') {
        bool enabled = get_sdl_enable_bottom_panes();

        set_sdl_enable_bottom_panes(!enabled);
        sdl_apply_config();
        if (character_dungeon)
            Term_keypress(KTRL('R'));
        return true;
    }

    if (key == 'p' || key == 'P') {
        bool hidden = get_sdl_hide_left_panel();

        set_sdl_hide_left_panel(!hidden);
        sdl_request_redraw();
        save_pane_config_to_json();
        if (character_dungeon)
            Term_keypress(KTRL('R'));
        return true;
    }

    return false;
}

static void sdl_gamepad_send_key(int key, bool use_macro_mods)
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

static void sdl_gamepad_send_key_raw(int key)
{
    Term_keypress(key);
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

    if (sdl_send_modified_direction_action(dir, (char)('0' + dir), shift, ctrl, alt, false))
        return;

    if (shift || ctrl || alt) {
        sdl_send_macro_key('0' + dir, shift, ctrl, alt);
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

static bool sdl_gamepad_resolve_pending_shoulder_with_modifier(int binding)
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

static bool sdl_gamepad_action_is_confirm(int binding)
{
    return (binding == INPUT_BIND_CONFIRM || binding == ' ');
}

static bool sdl_gamepad_action_binding_equals(int lhs, int rhs)
{
    if (sdl_gamepad_action_is_confirm(lhs) && sdl_gamepad_action_is_confirm(rhs))
        return true;

    return lhs == rhs;
}

static int sdl_gamepad_direct_binding_count(int binding, int* out_type, int* out_id)
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

static int sdl_gamepad_physical_binding_count(int binding, int* out_type, int* out_id)
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

static int sdl_gamepad_combo_action_binding_count(int binding, int* out_modifier_type,
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

static void sdl_gamepad_action_binding_label_ex(int binding, char* buf, size_t buflen, bool short_label)
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

static int sdl_gamepad_capture_binding_for_input(int type, int id)
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

static bool sdl_gamepad_capture_queue_input(int type, int id)
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
    SDL_Rect panes[PANE_MAX] = {0};
    bool show_supporting_panes = sdl_should_show_supporting_panes();
    bool include_side = config.enable_right_panes;
    bool include_bottom = config.enable_bottom_panes;

    if (!show_supporting_panes)
    {
        sdl_place_active_panes(screen, panes, g_active_side_panes,
            g_active_bottom_panes, true);
        g_supporting_panes_layout_visible = false;
    }

    // Check whether after splitting the window the main view meets minimum size.
    // If it doesn't, remove panes along the corresponding axis (or axes).
    // Also remove panes if user has disabled them via settings.
    if (show_supporting_panes) {
        int cell_w = config.main_view_scale * TILE_SIZE / 2;
        int cell_h = config.main_view_scale * TILE_SIZE;
        int min_main_cols = sdl_current_min_terminal_cols();
        int min_main_rows = sdl_current_min_terminal_rows();
        log_debug("Cell dimensions: %dx%d (scale=%d, TILE_SIZE=%d)", cell_w, cell_h, config.main_view_scale, TILE_SIZE);
        int cols;
        int rows;

        if (!include_side)
            log_info("side panes disabled by user setting");
        if (!include_bottom)
            log_info("bottom panes disabled by user setting");

        sdl_place_active_panes(screen, panes, include_side, include_bottom,
            false);

        for (;;) {
            cols = panes[PANE_MAIN].w / cell_w;
            rows = panes[PANE_MAIN].h / cell_h;
            log_debug("Main view: %dx%d pixels at (%d,%d) = %dx%d cells (minimum required: %dx%d %s)",
                panes[PANE_MAIN].w, panes[PANE_MAIN].h,
                panes[PANE_MAIN].x, panes[PANE_MAIN].y,
                cols, rows,
                min_main_cols, min_main_rows,
                sdl_min_terminal_mode_name(config.min_terminal_mode));

            if (include_side && cols < min_main_cols) {
                log_warn("main view too small, %d cols < %d; removing side panes",
                    cols, min_main_cols);
                include_side = false;
                sdl_place_active_panes(screen, panes, include_side,
                    include_bottom, false);
                continue;
            }

            if (include_bottom && rows < min_main_rows) {
                log_warn("main view too small, %d rows < %d; removing bottom panes",
                    rows, min_main_rows);
                include_bottom = false;
                sdl_place_active_panes(screen, panes, include_side,
                    include_bottom, false);
                continue;
            }

            break;
        }

        g_active_side_panes = include_side;
        g_active_bottom_panes = include_bottom;
        g_supporting_panes_layout_visible = true;
    }

    for (int i = 0; i < PANE_MAX; i++) {
        const SDL_Rect* r = &panes[i];
        log_debug("pane %d is at (%d, %d) size %dx%d", i, r->x, r->y, r->w, r->h);
    }

    memcpy(g_pane_rects, panes, sizeof(g_pane_rects));

    // Use configured monospace font or fall back to default
    const char* font_path = config.monospace_font[0] != '\0' 
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";

    for (int i = 1; i < MAX_TERM_DATA; i++) {
        // Always destroy the old pane to prevent its display in cases when we
        // have removed one of the bars or both of them due to the size
        // restrictions.
        sdl_view_destroy(&g_views[i]);
        if (panes[i].w > 0 && panes[i].h > 0) {
            if (!sdl_view_create(&g_views[i], panes[i], font_path,
                    sdl_effective_pane_font_size_for_type((enum pane_type)i), 0,
                    config.margin))
            {
                g_pane_rects[i] = (SDL_Rect){ 0 };
                continue;
            }
            sdl_view_link_term(&g_views[i], i);
        }
    }

    sdl_view_destroy(&g_views[0]);
    if (!sdl_view_create(&g_views[0], panes[PANE_MAIN], font_path, 0,
            config.main_view_scale, config.margin))
    {
        quit("could not create main view");
    }
    sdl_view_link_term(&g_views[0], 0);

    Term_activate(&g_views[0].t);

    /* Ensure the dungeon panel still contains the player after a resize/scale
     * change. Otherwise the player can end up off-screen until something else
     * triggers PU_PANEL. */
    if (character_dungeon && p_ptr)
    {
        p_ptr->update |= PU_PANEL;
        p_ptr->redraw |= PR_MAP;
    }

    // Don't strictly need this as `sdl_view_create` already sets this flag.
    g_state.need_present = true;
    sdl_update_cursor_visibility();
}

/*
 * Handle renderer device/targets reset.
 * This can happen on NVIDIA when switching fullscreen modes,
 * when the driver resets, or after sleep/wake cycles.
 * We need to recreate all render targets (canvas textures).
 */
static void sdl_handle_renderer_reset(void)
{
    const char* font_path = config.monospace_font[0] != '\0'
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";

    sdl_mono_font_cache_clear();

    // Recreate all view canvases
    for (int i = 0; i < MAX_TERM_DATA; i++) {
        sdl_view* view = &g_views[i];
        if (!view->term_ready)
            continue;

        if (view->font_atlas && !view->font_atlas_cached)
            SDL_DestroyTexture(view->font_atlas);
        view->font_atlas = sdl_acquire_mono_font_atlas(font_path, view->cell_h,
            &view->font_atlas_cached);
        if (view->font_atlas) {
            SDL_SetTextureBlendMode(view->font_atlas, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(view->font_atlas, 255, 255, 255);
            SDL_SetTextureAlphaMod(view->font_atlas, 255);
        }

        // Destroy old canvas
        if (view->canvas) {
            SDL_DestroyTexture(view->canvas);
            view->canvas = NULL;
        }

        // Recreate canvas texture
        view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET,
                                         view->cols * view->cell_w,
                                         view->rows * view->cell_h);
        if (view->canvas) {
            SDL_SetTextureBlendMode(view->canvas, SDL_BLENDMODE_NONE);
            SDL_SetTextureScaleMode(view->canvas, SDL_SCALEMODE_NEAREST);
            SDL_SetRenderTarget(g_state.renderer, view->canvas);
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_state.renderer);
        } else {
            log_error("Failed to recreate canvas for view %d: %s", i, SDL_GetError());
        }
    }

    // Recreate tileset if using tiles
    if (g_state.use_tiles && g_state.tileset) {
        SDL_DestroyTexture(g_state.tileset);
        g_state.tileset = NULL;

        SDL_Surface* ts = IMG_Load("lib/xtra/graf/16x16.png");
        if (ts) {
            g_state.tileset = SDL_CreateTextureFromSurface(g_state.renderer, ts);
            if (g_state.tileset) {
                SDL_SetTextureScaleMode(g_state.tileset, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
            }
            SDL_DestroySurface(ts);
        }
    }

    // Force a full redraw
    g_state.need_present = true;
    Term_redraw();
}

static void sdl_handle_event(sdl_state* st, const SDL_Event* ev)
{
    (void)st;
    if (sdl_sound_try_handle_event(ev)) {
        return;
    }
    if (ev->type == SDL_EVENT_QUIT) {
        Term_keypress(27); // ESC or define a quit signal
    } else if (g_touch_pane_reset_confirm_active) {
        if (ev->type == SDL_EVENT_KEY_DOWN) {
            if (ev->key.key == SDLK_ESCAPE) {
                sdl_touch_pane_finish_reset_confirm(false);
            } else if (ev->key.key == SDLK_RETURN || ev->key.key == SDLK_KP_ENTER
                || ev->key.key == SDLK_SPACE)
            {
                sdl_touch_pane_finish_reset_confirm(true);
            }
            return;
        } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (ev->button.button == SDL_BUTTON_LEFT && ev->button.which != SDL_TOUCH_MOUSEID)
                sdl_touch_pane_handle_reset_prompt_pointer((float)ev->button.x, (float)ev->button.y);
            return;
        } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP) {
            return;
        } else if (ev->type == SDL_EVENT_FINGER_DOWN) {
            int window_w = 0;
            int window_h = 0;

            if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
                return;
            SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
            sdl_touch_pane_handle_reset_prompt_pointer(ev->tfinger.x * (float)window_w,
                ev->tfinger.y * (float)window_h);
            return;
        } else if (ev->type == SDL_EVENT_FINGER_UP || ev->type == SDL_EVENT_FINGER_CANCELED) {
            return;
        }
    } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            if (ev->button.which == SDL_TOUCH_MOUSEID)
                return;
            if (sdl_touch_pane_handle_pointer_down((float)ev->button.x, (float)ev->button.y,
                true, 0))
            {
                return;
            }
        }
    } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            if (ev->button.which == SDL_TOUCH_MOUSEID)
                return;
            sdl_touch_pane_handle_pointer_up(true, 0);
        }
    } else if (ev->type == SDL_EVENT_FINGER_DOWN) {
        int window_w = 0;
        int window_h = 0;
        float x;
        float y;

        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return;

        SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
        x = ev->tfinger.x * (float)window_w;
        y = ev->tfinger.y * (float)window_h;
        if (sdl_touch_pane_handle_pointer_down(x, y, false, ev->tfinger.fingerID))
            return;
        if (sdl_touch_swipe_handle_pointer_down(x, y, ev->tfinger.fingerID))
            return;
    } else if (ev->type == SDL_EVENT_FINGER_MOTION) {
        int window_w = 0;
        int window_h = 0;
        float x;
        float y;

        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return;

        SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
        x = ev->tfinger.x * (float)window_w;
        y = ev->tfinger.y * (float)window_h;
        if (sdl_touch_swipe_handle_pointer_motion(x, y, ev->tfinger.fingerID))
            return;
    } else if (ev->type == SDL_EVENT_FINGER_UP) {
        int window_w = 0;
        int window_h = 0;
        float x;
        float y;

        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return;
        SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
        x = ev->tfinger.x * (float)window_w;
        y = ev->tfinger.y * (float)window_h;
        sdl_touch_swipe_handle_pointer_up(x, y, ev->tfinger.fingerID);
        sdl_touch_pane_handle_pointer_up(false, ev->tfinger.fingerID);
    } else if (ev->type == SDL_EVENT_FINGER_CANCELED) {
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return;
        if (g_touch_swipe.active && g_touch_swipe.finger_id == ev->tfinger.fingerID)
            sdl_touch_swipe_cancel();
        if (g_touch_pane_press.active && !g_touch_pane_press.mouse
            && g_touch_pane_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_touch_pane_cancel_press();
        }
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

        /* Handle SDL layout shortcuts before menu/game input routing so they
         * work from the initial menu onward. */
        if (sdl_handle_global_layout_shortcut(&ev->key))
            return;

        // Keep other Alt-based key handling limited to the dungeon.
        bool alt = ev->key.mod & SDL_KMOD_ALT;
        if (alt && !character_dungeon)
            return;

        if (character_dungeon) {
            if (sdl_try_send_modified_direction_event(&ev->key))
                return;
        }

        if (SDL_isprint(ev->key.key)) {
            /* If Ctrl+letter (no Alt/GUI), send the corresponding control char
             * (so Ctrl-A -> ASCII 1) to preserve traditional control bindings
             * like Ctrl-A for staff swapping. Other printable keys with
             * Ctrl/Alt/GUI use macro triggers so pref bindings can match. */
            bool shift = ev->key.mod & SDL_KMOD_SHIFT;
            bool ctrl = ev->key.mod & SDL_KMOD_CTRL;
            bool alt = ev->key.mod & SDL_KMOD_ALT;
            bool gui = ev->key.mod & SDL_KMOD_GUI;
            if (ctrl && !alt && !gui && SDL_isalpha(key)) {
                /* Map to control character */
                Term_keypress(KTRL(key));
            } else if (ctrl || alt || gui) {
                sdl_send_macro_key(key, shift, ctrl || gui, alt);
            } else {
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
                case SDLK_END:
                    key = '1';
                    break;
                case SDLK_KP_3:
                case SDLK_PAGEDOWN:
                    key = '3';
                    break;
                case SDLK_KP_7:
                case SDLK_HOME:
                    key = '7';
                    break;
                case SDLK_KP_9:
                case SDLK_PAGEUP:
                    key = '9';
                    break;
                case SDLK_KP_5:
                    key = '5';
                    break;
            }
            if (mod) {
                sdl_send_macro_key(key, shift, ctrl || gui, alt);
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
    } else if (ev->type == SDL_EVENT_WINDOW_RESIZED
        || ev->type == SDL_EVENT_WINDOW_SAFE_AREA_CHANGED) {
        log_debug("window resized to %dx%d", ev->window.data1, ev->window.data2);
        sdl_refresh_safe_area();
        (void)sdl_recover_layout_for_current_window("window resize", true, NULL);
        {
            SDL_Rect screen = sdl_get_layout_screen_rect();

            log_debug("new layout size %dx%d at (%d,%d)",
                screen.w, screen.h, screen.x, screen.y);
            resize(&screen);
        }
    } else if (ev->type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED ||
        ev->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
        ev->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {

        float scale = SDL_GetWindowDisplayScale(g_state.window);
        bool scale_changed = (scale != g_state.system_scale);

        if (scale_changed) {
            log_info("new system scale is %g", scale);
            g_state.system_scale = scale;
            sdl_load_story_fonts();
        }

        sdl_refresh_safe_area();
        (void)sdl_recover_layout_for_current_window("display scale change",
            true, NULL);
        {
            SDL_Rect screen = sdl_get_layout_screen_rect();

            log_debug("window pixel/display update layout=%dx%d at (%d,%d) (scale_changed=%d)",
                screen.w, screen.h, screen.x, screen.y, scale_changed ? 1 : 0);
            resize(&screen);
        }
    }
    // Handle GPU reset events (commonly triggered by NVIDIA drivers on mode switches,
    // driver updates, or sleep/wake cycles)
    else if (ev->type == SDL_EVENT_RENDER_DEVICE_RESET ||
             ev->type == SDL_EVENT_RENDER_TARGETS_RESET) {
        log_warn("Renderer device/targets reset detected - recreating textures");
        sdl_handle_renderer_reset();
    }
    // Handle window restored (after minimize/alt-tab on some systems)
    else if (ev->type == SDL_EVENT_WINDOW_RESTORED ||
             ev->type == SDL_EVENT_WINDOW_EXPOSED) {
        log_debug("Window restored/exposed - forcing redraw");
        g_state.need_present = true;
        Term_redraw();
    }
}

static void sdl_touch_pane_render(void)
{
    SDL_FRect slot_rects[SDL_TOUCH_PANE_BUTTON_COUNT];
    SDL_FRect pane_rect;
    SDL_Color frame = g_state.palette[TERM_WHITE];
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    SDL_Color muted = g_state.palette[TERM_SLATE];
    SDL_Rect panes[PANE_MAX];
    const SDL_Rect* pane;
    int panel = sdl_touch_pane_active_panel();
    bool show_supporting_panes = sdl_should_show_supporting_panes();

    if (show_supporting_panes || sdl_layout_matches_supporting_pane_visibility()) {
        pane = &g_pane_rects[PANE_TOUCH];
    } else {
        sdl_compute_display_panes(panes);
        pane = &panes[PANE_TOUCH];
    }

    if (pane->w <= 0 || pane->h <= 0)
        return;

    if (!sdl_touch_pane_compute_layout(pane, slot_rects))
        return;

    pane_rect = (SDL_FRect){
        .x = (float)pane->x,
        .y = (float)pane->y,
        .w = (float)pane->w,
        .h = (float)pane->h,
    };

    SDL_SetRenderDrawColor(g_state.renderer, 12, 12, 12, 255);
    SDL_RenderFillRect(g_state.renderer, &pane_rect);
    if (config.show_pane_borders)
        SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, 180);
    else
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderRect(g_state.renderer, &pane_rect);

    for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++) {
        SDL_Color text_color;
        SDL_Color border_color;
        SDL_FRect shadow;
        char label[64];
        char symbol[32];
        int binding = sdl_touch_pane_effective_binding_for_panel(panel, i);
        bool flashed = (i == g_touch_pane_flash_slot);
        bool pressed = (i == g_touch_pane_pressed_slot);
        bool toggled = ((binding == GAMEPAD_BIND_SHIFT && g_touch_pane_second_panel)
            || (binding == GAMEPAD_BIND_CTRL && sdl_gamepad_ctrl_active()));

        shadow = slot_rects[i];
        shadow.x += 2.0f;
        shadow.y += 2.0f;

        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(g_state.renderer, &shadow);

        if (binding == GAMEPAD_BIND_NONE) {
            SDL_SetRenderDrawColor(g_state.renderer, 26, 26, 26, 255);
            text_color = muted;
            border_color = muted;
        } else if (toggled) {
            SDL_SetRenderDrawColor(g_state.renderer, 48, 58, 44, 255);
            text_color = accent;
            border_color = accent;
        } else if (pressed) {
            SDL_SetRenderDrawColor(g_state.renderer, 54, 66, 86, 255);
            text_color = accent;
            border_color = accent;
        } else if (flashed) {
            SDL_SetRenderDrawColor(g_state.renderer, 54, 66, 86, 255);
            text_color = accent;
            border_color = accent;
        } else {
            SDL_SetRenderDrawColor(g_state.renderer, 34, 34, 34, 255);
            text_color = frame;
            border_color = frame;
        }

        SDL_RenderFillRect(g_state.renderer, &slot_rects[i]);
        SDL_SetRenderDrawColor(g_state.renderer, border_color.r, border_color.g, border_color.b, 220);
        SDL_RenderRect(g_state.renderer, &slot_rects[i]);

        if (sdl_touch_pane_binding_is_direction(binding)) {
            sdl_touch_pane_draw_arrow(&slot_rects[i], binding, text_color);
            continue;
        }

        sdl_touch_pane_display_label_for_slot(panel, i, label, sizeof(label));
        sdl_touch_pane_binding_symbol(binding, symbol, sizeof(symbol));
        if (sdl_touch_pane_should_hide_symbol(label, symbol))
            symbol[0] = '\0';
        sdl_touch_pane_draw_button_text(&slot_rects[i], label, symbol, text_color);
    }

    if (g_touch_pane_flash_slot >= 0) {
        g_touch_pane_flash_slot = -1;
        g_touch_pane_flash_until = 0;
    }
}

static void sdl_present_if_needed(sdl_view* d)
{
    bool show_supporting_panes;
    bool layout_matches;
    SDL_Rect layout_screen;
    int visible_views = 0;

    if (!g_state.need_present)
        return;
    if (g_suppress_layout_refresh_present)
        return;

    show_supporting_panes = sdl_should_show_supporting_panes();
    layout_matches = sdl_layout_matches_supporting_pane_visibility();
    if (!layout_matches)
        return;

    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);
    layout_screen = sdl_get_layout_screen_rect();

    for (int i = 0; i < MAX_TERM_DATA; i++) {
        if (!g_views[i].canvas)
            continue;
        if (!show_supporting_panes && i != PANE_MAIN && i != PANE_TOUCH)
            continue;
        visible_views++;
    }

    for (int i = 0; i < MAX_TERM_DATA; i++) {
        sdl_view* view = &g_views[i];
        float dst_w;
        float dst_h;

        if (!view->canvas)
            continue;
        if (view->cols <= 0 || view->rows <= 0 || view->cell_w <= 0 || view->cell_h <= 0)
            continue;
        if (!show_supporting_panes && i != PANE_MAIN)
            continue;

        dst_w = (float)(view->cols * view->cell_w);
        dst_h = (float)(view->rows * view->cell_h);
        if (dst_w <= 0.0f || dst_h <= 0.0f)
            continue;

        SDL_RenderTexture(g_state.renderer, view->canvas, NULL, &(SDL_FRect){
            .x = (float)(view->rect.x + view->margin_x),
            .y = (float)(view->rect.y + view->margin_y),
            .w = dst_w,
            .h = dst_h,
        });
    }

    if (visible_views > 1) {
        if (config.show_pane_borders)
            SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 128);
        else
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);

        for (int i = 0; i < MAX_TERM_DATA; i++) {
            sdl_view* view = &g_views[i];

            if (!view->canvas)
                continue;
            if (!show_supporting_panes && i != PANE_MAIN && i != PANE_TOUCH)
                continue;

            if (i == PANE_TOUCH)
                continue;

            /* Draw only internal leading edges.  This keeps separators
             * between panes without painting a frame around the window. */
            sdl_draw_pane_edges(&view->rect,
                (layout_screen.w > 0 && view->rect.x > layout_screen.x),
                (layout_screen.h > 0 && view->rect.y > layout_screen.y),
                false,
                false);
        }
    }

    sdl_touch_pane_render();
    sdl_touch_pane_render_reset_prompt();
    SDL_RenderPresent(g_state.renderer);

    if (d && d->canvas)
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
    else
        SDL_SetRenderTarget(g_state.renderer, NULL);

    g_state.need_present = false;
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
            int touch_timeout_ms = sdl_touch_pane_pending_timeout_ms(now_ns);
            if (timeout_ms < 0 || (touch_timeout_ms >= 0 && touch_timeout_ms < timeout_ms))
                timeout_ms = touch_timeout_ms;
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
            sdl_touch_pane_flush_pending_press(flush_ns);
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
            sdl_touch_pane_flush_pending_press(flush_ns);

            /* Avoid pegging a CPU core when we're repeatedly asked to poll */
            if (!handled)
                SDL_Delay(1);
        }
        sdl_present_if_needed(d);
        return 0;
    }
    case TERM_XTRA_FLUSH:
        // Flush pending input events (drain queue)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                sdl_handle_event(&g_state, &ev);
        }
        sdl_touch_pane_flush_pending_press(SDL_GetTicksNS());
        sdl_present_if_needed(d);
        return 0;
    case TERM_XTRA_CLEAR:
        if (!d || !d->canvas)
            return 0;
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
        g_state.need_present = true;
        return 0;
    case TERM_XTRA_FRESH:
        sdl_present_if_needed(d);
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
            sdl_touch_pane_flush_pending_press(SDL_GetTicksNS());
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
    if (!d || !d->canvas)
        return;
    if (!Term)
        return;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
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
    if (!d || !d->canvas || !Term || n <= 0)
        return 0;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return 0;
    if (x + n > Term->wid)
        n = Term->wid - x;
    if (n <= 0)
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
    if (!d || !d->canvas || !Term || !s || n <= 0)
        return 0;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return 0;
    if (x + n > Term->wid)
        n = Term->wid - x;
    if (n <= 0)
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
                    log_trace("callback_sdl_text: Using story font based on per-char flag at y=%d x=%d (chunk starts at x=%d)",
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
        log_trace("callback_sdl_text ROW 0: x=%d n=%d chunk_story=%d text='%.*s'",
                  x, n, chunk_story_font, n, s);
    }
    
    // Special logging for the shooting row (y=1 when 0-indexed, or the second row)
    if (y == 1 || y == 2) {
        log_trace("callback_sdl_text ROW %d: chunk_story=%d chunk_active=%d",
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
            log_trace("callback_sdl_text: USING MONO FONT for row %d: '%.30s'", y, s);
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
    if (!d || !d->canvas || !Term || !ap || !cp || !tap || !tcp || n <= 0)
        return 0;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return 0;
    if (x + n > Term->wid)
        n = Term->wid - x;
    if (n <= 0)
        return 0;
    //log_trace("sdl3_pict stripe start: y=%d x=%d n=%d", y, x, n);

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
        bool seen = tcp[i] & GRAPHICS_SEEN_MASK;
        bool sleep = tap[i] & GRAPHICS_SLEEP_MASK;

        /* Unconditionally clear the full (possibly 2-cell) destination area to avoid ghosting */
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(g_state.renderer, &dst);

        /* Draw terrain underlay ALWAYS */
        src.x = (tcp[i] & 0x3F) * TILE_SIZE;
        src.y = (tap[i] & 0x3F) * TILE_SIZE;
        SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);

        /* Traps, stairs, shafts, forges, and sunlight are drawn as a middle layer (floor ->
         * feature -> monster). When a visible creature is standing on a visible feature,
         * inject the feature tile between the terrain underlay and the creature tile. */
        if (Term == term_screen) {
            int term_x = x + (i * (use_bigtile + 1));
            if (y >= ROW_MAP && term_x >= COL_MAP) {
                int map_y = y - ROW_MAP;
                int map_x = term_x - COL_MAP;
                if (use_bigtile)
                    map_x /= 2;

                int dy = p_ptr->wy + map_y;
                int dx = p_ptr->wx + map_x;

                if ((dy >= 0) && (dx >= 0) && (dy < p_ptr->cur_map_hgt)
                    && (dx < p_ptr->cur_map_wid)) {
                    u16b info = cave_info[dy][dx];
                    bool hide_square = (!p_ptr->is_dead)
                        && (p_ptr->rage || g_labyrinth_view_active)
                        && !(info & (CAVE_SEEN));

                    if (!hide_square) {
                        s16b m_idx = cave_m_idx[dy][dx];
                        bool creature_visible = (m_idx < 0)
                            || ((m_idx > 0) && mon_list[m_idx].ml);

                        if (creature_visible && (info & (CAVE_MARK))) {
                            byte feat = cave_feat[dy][dx];
                            feat = f_info[feat].mimic;

                            if (((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL)) ||
                                ((feat >= FEAT_STAIR_HEAD) && (feat <= FEAT_STAIR_TAIL)) ||
                                ((feat >= FEAT_FORGE_HEAD) && (feat <= FEAT_FORGE_TAIL)) ||
                                (feat == FEAT_SUNLIGHT)) {
                                feature_type* f_ptr = &f_info[feat];
                                byte feat_a = f_ptr->x_attr;
                                char feat_c = f_ptr->x_char;

                                if ((use_graphics == GRAPHICS_MICROCHASM)
                                    && feat_supports_lighting(feat)) {
                                    bool is_dark = p_ptr->blind
                                        || ((cave_light[dy][dx] <= 0)
                                            && !(info & (CAVE_GLOW)));
                                    if (is_dark || !(info & (CAVE_SEEN))) {
                                        feat_c += 1;
                                    }
                                }

                                src.x = ((byte)feat_c & 0x3F) * TILE_SIZE;
                                src.y = (feat_a & 0x3F) * TILE_SIZE;
                                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
                            }
                        }

                        /* Keep the floor item visible beneath the player tile. */
                        if (m_idx < 0) {
                            byte feat = cave_feat[dy][dx];

                            if ((feat == FEAT_FLOOR) || (feat == FEAT_SUNLIGHT)) {
                                object_type* o_ptr;

                                for (o_ptr = get_first_object(dy, dx); o_ptr;
                                     o_ptr = get_next_object(o_ptr)) {
                                    if (o_ptr->marked) {
                                        byte obj_a = object_attr(o_ptr);
                                        byte obj_c = (byte)object_char(o_ptr);

                                        if ((obj_a & TILE_FLAG) && (obj_c & TILE_FLAG)) {
                                            src.x = (obj_c & 0x3F) * TILE_SIZE;
                                            src.y = (obj_a & 0x3F) * TILE_SIZE;
                                            SDL_RenderTexture(g_state.renderer,
                                                g_state.tileset, &src, &dst);
                                        }

                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        /* Overlays (glow / alert) */
        if (glow) {
            byte icon_a = misc_to_attr[ICON_GLOW];
            byte icon_c = (byte)misc_to_char[ICON_GLOW];
            if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG)) {
                src.x = (icon_c & 0x7F) * TILE_SIZE;
                src.y = (icon_a & 0x7F) * TILE_SIZE;
                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
            }
        }

        /* Draw base tile */
        src.x = (c & 0x3F) * TILE_SIZE;
        src.y = (a & 0x3F) * TILE_SIZE;
        SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);

        if (sleep) {
            byte icon_a = misc_to_attr[ICON_SLEEPING];
            byte icon_c = (byte)misc_to_char[ICON_SLEEPING];
            if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG)) {
                src.x = (icon_c & 0x7F) * TILE_SIZE;
                src.y = (icon_a & 0x7F) * TILE_SIZE;
                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
            }
        }

        if (seen) {
            byte icon_a = misc_to_attr[ICON_MONSTER_SEES_PLAYER];
            byte icon_c = (byte)misc_to_char[ICON_MONSTER_SEES_PLAYER];
            if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG)) {
                src.x = (icon_c & 0x7F) * TILE_SIZE;
                src.y = (icon_a & 0x7F) * TILE_SIZE;
                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
            }
        }

        if (alert) {
            byte icon_a = misc_to_attr[ICON_ALERT];
            byte icon_c = (byte)misc_to_char[ICON_ALERT];
            if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG)) {
                src.x = (icon_c & 0x7F) * TILE_SIZE;
                src.y = (icon_a & 0x7F) * TILE_SIZE;
                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
            }
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
    d->font_atlas = NULL;
    // if (d->tileset)
    //     SDL_DestroyTexture(d->tileset);
    if (d->canvas)
        SDL_DestroyTexture(d->canvas);
    d->canvas = NULL;
    d->font_atlas_cached = false;
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
        if (!(g_skip_main_redraw_on_layout_refresh && term_index == PANE_MAIN))
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
    t->data = (void*)(uintptr_t)term_index;
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

static void sdl_mono_font_cache_clear(void)
{
    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        mono_font_atlas_entry* entry = &g_state.mono_font_atlases[i];

        if (entry->atlas) {
            SDL_DestroyTexture(entry->atlas);
            entry->atlas = NULL;
        }

        entry->valid = false;
        entry->font_path[0] = '\0';
        entry->cell_height = 0;
        entry->bold = false;
        entry->italic = false;
        entry->underline = false;
        entry->strikethrough = false;
        entry->hinting = 0;
        entry->kerning = false;
        entry->outline = 0;
    }
}

static SDL_Texture* sdl_acquire_mono_font_atlas(const char* font_path, int cell_height,
    bool* out_cached)
{
    mono_font_atlas_entry* free_entry = NULL;

    if (out_cached)
        *out_cached = false;

    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        mono_font_atlas_entry* entry = &g_state.mono_font_atlases[i];

        if (!entry->valid) {
            if (!free_entry)
                free_entry = entry;
            continue;
        }

        if (entry->cell_height != cell_height)
            continue;
        if (entry->bold != config.mono_bold
            || entry->italic != config.mono_italic
            || entry->underline != config.mono_underline
            || entry->strikethrough != config.mono_strikethrough
            || entry->hinting != config.mono_hinting
            || entry->kerning != config.mono_kerning
            || entry->outline != config.mono_outline)
            continue;
        if (!streq(entry->font_path, font_path))
            continue;

        if (out_cached)
            *out_cached = true;
        return entry->atlas;
    }

    if (!free_entry) {
        log_warn("Monospace font atlas cache full; creating uncached atlas for %s at cell height %d",
            font_path, cell_height);
        return sdl_load_ttf_font(font_path, cell_height, NULL);
    }

    free_entry->atlas = sdl_load_ttf_font(font_path, cell_height, NULL);
    if (!free_entry->atlas)
        return NULL;

    free_entry->valid = true;
    SDL_strlcpy(free_entry->font_path, font_path, sizeof(free_entry->font_path));
    free_entry->cell_height = cell_height;
    free_entry->bold = config.mono_bold;
    free_entry->italic = config.mono_italic;
    free_entry->underline = config.mono_underline;
    free_entry->strikethrough = config.mono_strikethrough;
    free_entry->hinting = config.mono_hinting;
    free_entry->kerning = config.mono_kerning;
    free_entry->outline = config.mono_outline;

    if (out_cached)
        *out_cached = true;

    return free_entry->atlas;
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
    g_state.story_font_cache_valid = false;
    g_state.story_font_cache_path[0] = '\0';
}

static bool sdl_story_font_cache_matches_config(void)
{
    const char* font_path = (config.story_font[0] != '\0') ? config.story_font : "";

    if (!g_state.story_font_cache_valid)
        return false;

    return streq(g_state.story_font_cache_path, font_path)
        && g_state.story_font_cache_bold == config.story_bold
        && g_state.story_font_cache_italic == config.story_italic
        && g_state.story_font_cache_underline == config.story_underline
        && g_state.story_font_cache_strikethrough == config.story_strikethrough
        && g_state.story_font_cache_hinting == config.story_hinting
        && g_state.story_font_cache_kerning == config.story_kerning
        && g_state.story_font_cache_outline == config.story_outline;
}

static void sdl_story_font_cache_mark_config(void)
{
    const char* font_path = (config.story_font[0] != '\0') ? config.story_font : "";

    g_state.story_font_cache_valid = true;
    SDL_strlcpy(g_state.story_font_cache_path, font_path,
        sizeof(g_state.story_font_cache_path));
    g_state.story_font_cache_bold = config.story_bold;
    g_state.story_font_cache_italic = config.story_italic;
    g_state.story_font_cache_underline = config.story_underline;
    g_state.story_font_cache_strikethrough = config.story_strikethrough;
    g_state.story_font_cache_hinting = config.story_hinting;
    g_state.story_font_cache_kerning = config.story_kerning;
    g_state.story_font_cache_outline = config.story_outline;
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
    SDL_Texture* previous_target = NULL;
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
    previous_target = SDL_GetRenderTarget(g_state.renderer);
    SDL_SetTextureBlendMode(font_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(g_state.renderer, font_atlas);
    /* Fresh target textures are not guaranteed to start cleared on mobile GPUs. */
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 0);
    SDL_RenderClear(g_state.renderer);
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
        SDL_DestroyTexture(gtex);
    }
    SDL_SetRenderTarget(g_state.renderer, previous_target);
    /* Sample atlas cells without filtering to avoid cross-glyph bleed. */
    SDL_SetTextureScaleMode(font_atlas, SDL_SCALEMODE_NEAREST);
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

    // Enable V-SYNC for consistent frame timing across GPU vendors.
    // Without V-SYNC, NVIDIA drivers may buffer commands and return immediately
    // from SDL_RenderPresent(), causing timing-dependent rendering issues.
    if (!SDL_SetRenderVSync(g_state.renderer, 1)) {
        log_warn("Failed to enable V-SYNC: %s", SDL_GetError());
    }

    g_state.system_scale = SDL_GetWindowDisplayScale(g_state.window);
    log_debug("window scale is %g", g_state.system_scale);
    sdl_refresh_safe_area();

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
            int tileset_width = ts->w;
            g_state.tileset = SDL_CreateTextureFromSurface(g_state.renderer, ts);
            SDL_DestroySurface(ts);
            if (!g_state.tileset) {
                log_error("Failed to create tileset texture: %s", SDL_GetError());
                quit("could not create tileset texture");
            } else {
                SDL_SetTextureScaleMode(g_state.tileset, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
                g_state.tileset_cols = tileset_width / TILE_SIZE;
            }
        } else {
            log_error("Failed to load tileset PNG: %s", SDL_GetError());
            quit("could not load tileset");
        }
    }
}

static bool sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin)
{
    log_debug("view rect=(%d %d %d %d)", rect.x, rect.y, rect.w, rect.h);

    if (scale) {
        // Integer scaling mode.
#if defined(__ANDROID__) || defined(SIL_IOS)
        int requested_scale = scale;
        int min_cols = sdl_current_min_terminal_cols();
        int min_rows = sdl_current_min_terminal_rows();
        int max_scale_for_min_cols = (rect.w / min_cols) * 2 / TILE_SIZE;
        int max_scale_for_min_rows = rect.h / min_rows / TILE_SIZE;
        int effective_scale = requested_scale;

        if (max_scale_for_min_cols < 1)
            max_scale_for_min_cols = 1;
        if (max_scale_for_min_rows < 1)
            max_scale_for_min_rows = 1;

        int max_scale_for_min_size = max_scale_for_min_cols;
        if (max_scale_for_min_rows < max_scale_for_min_size)
            max_scale_for_min_size = max_scale_for_min_rows;

        if (effective_scale > max_scale_for_min_size)
            effective_scale = max_scale_for_min_size;

        d->cell_w = effective_scale * TILE_SIZE / 2;
        d->cell_h = effective_scale * TILE_SIZE;

        if (effective_scale != requested_scale) {
            log_info("Mobile main view scale clamped from %d to %d to keep >=%dx%d (%s)",
                     requested_scale, effective_scale,
                     min_cols, min_rows, sdl_min_terminal_mode_name(config.min_terminal_mode));
        }
#else
        d->cell_w = scale * TILE_SIZE / 2;
        d->cell_h = scale * TILE_SIZE;
#endif
    } else if (font_size) {
        // Non-integer scaling mode.
        d->cell_h = g_state.system_scale * font_size;
        d->cell_w = d->cell_h / 2;
    } else {
        quit("sdl_view_create: font_size and scale cannot both be zero");
    }

    d->rect = rect;
    d->cols = rect.w / d->cell_w;
    d->rows = rect.h / d->cell_h;
#if defined(__ANDROID__) || defined(SIL_IOS)
    if (scale) {
        log_info("Mobile main view: scale=%d cell=(%d,%d) cols=%d rows=%d (min=%dx%d %s)",
                 d->cell_h / TILE_SIZE, d->cell_w, d->cell_h, d->cols, d->rows,
                 sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows(),
                 sdl_min_terminal_mode_name(config.min_terminal_mode));
    }
#endif
    /* Center the cell grid inside the view rect.
     * Do not force a minimum margin larger than the available slack; that can
     * shift the term off-center and even partially off-screen. */
    (void)margin;
    d->margin_x = (rect.w - d->cols * d->cell_w) / 2;
    if (d->margin_x < 0)
        d->margin_x = 0;
    d->margin_y = sdl_mobile_prefer_safe_edge_alignment()
        ? 0
        : (rect.h - d->rows * d->cell_h) / 2;
    if (d->margin_y < 0)
        d->margin_y = 0;
    log_debug("view cols=%d rows=%d cell=(%d, %d) margin=(%d, %d)",
        d->cols, d->rows, d->cell_w, d->cell_h,
        d->margin_x, d->margin_y);

    if (d->cols <= 0 || d->rows <= 0) {
        log_warn("Skipping view creation for rect=(%d %d %d %d): fits %dx%d cells at (%d,%d)",
            rect.x, rect.y, rect.w, rect.h, d->cols, d->rows, d->cell_w, d->cell_h);
        return false;
    }

    d->font_atlas = sdl_acquire_mono_font_atlas(font_path, d->cell_h,
        &d->font_atlas_cached);
    if (!d->font_atlas) {
        log_error("Failed to acquire font atlas for rect=(%d %d %d %d)", rect.x,
            rect.y, rect.w, rect.h);
        quit("could not create font atlas");
    }
    SDL_SetTextureBlendMode(d->font_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(d->font_atlas, 255, 255, 255);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

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

    return true;
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
    int pane_cell_widths[PANE_MAX] = { 0 };
    int pane_cell_heights[PANE_MAX] = { 0 };
    
    log_info("Loading story fonts...");
    log_debug("Story font config: '%s'", config.story_font[0] != '\0' ? config.story_font : "(not set)");
    log_debug("Story font sizes: main=%d default-aux=%d", main_cell_h,
        (int)(g_state.system_scale * sdl_resolve_aux_view_font_size(config.aux_view_font_size)));
    
    if (!sdl_story_font_cache_matches_config()) {
        sdl_story_font_cache_clear();
        sdl_story_font_cache_mark_config();
    }

    (void)sdl_story_font_for_height(main_cell_h);
    sdl_build_supporting_pane_metrics(pane_config, pane_config_count,
        pane_cell_widths, pane_cell_heights);
    for (int i = 1; i < PANE_MAX; i++) {
        if (pane_cell_heights[i] > 0)
            (void)sdl_story_font_for_height(pane_cell_heights[i]);
    }
    
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
    sdl_mono_font_cache_clear();
    
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
        sdl_store_active_pane_profile(config.min_terminal_mode);
        sdl_config_save(config_file_path, &config, g_pane_profiles, SDL_PANE_PROFILE_COUNT);
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
    // Physical resolution = logical x pixel_density
    float pixel_density = desktop_mode->pixel_density;
    
    // Calculate physical pixel dimensions for resolution profile matching
    // On macOS Retina: 1440x900 logical x 2.0 density = 2560x1600 physical
    // On Windows/Linux (no scaling): 1920x1080 logical x 1.0 density = 1920x1080 physical
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
    enum sdl_config_load_status config_load_status = SDL_CONFIG_LOAD_OK;
    char startup_issue_summary[SDL_STARTUP_ISSUE_MAX];
    bool desktop_handheld_first_start = false;

    startup_issue_summary[0] = '\0';

    if (config_exists) {
        // Config file exists - use generic defaults first, then load from file
        log_debug("Config file exists, loading from: %s", config_file_path);
        sdl_config_set_defaults(&config);
        
        // Copy default pane configuration
        pane_config_count = default_pane_config_count;
        for (int i = 0; i < default_pane_config_count && i < MAX_PANE_CONFIGS; i++) {
            pane_config[i] = default_pane_config[i];
        }
        sdl_seed_all_pane_profiles_from_active();
        config_load_status = sdl_config_load(config_file_path, &config,
            g_pane_profiles, SDL_PANE_PROFILE_COUNT);
        sdl_apply_stored_pane_profile(config.min_terminal_mode);

        if (config_load_status == SDL_CONFIG_LOAD_READ_FAILED) {
            sdl_append_issue_line(startup_issue_summary,
                sizeof(startup_issue_summary),
                "The SDL config file could not be read, so the game is using recovered settings.");
        } else if (config_load_status == SDL_CONFIG_LOAD_PARSE_FAILED) {
            sdl_append_issue_line(startup_issue_summary,
                sizeof(startup_issue_summary),
                "The SDL config file could not be parsed, so the game is using recovered settings.");
        }
        
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
        
        log_debug("After loading JSON: scale=%d, default_aux_font=%d, margin=%d, fullscreen=%d, tiles=%d, sound=%d",
                  config.main_view_scale, config.aux_view_font_size, config.margin,
                  config.fullscreen, config.tiles, g_sound_config.enabled);
    } else {
        // Config file doesn't exist - use resolution-based defaults
        log_debug("Config file not found, using resolution-based defaults");
        sdl_reset_config_to_resolution_defaults(screen_pixels_w, screen_pixels_h);
        
        log_debug("After resolution defaults: scale=%d, default_aux_font=%d, margin=%d, fullscreen=%d, tiles=%d",
                  config.main_view_scale, config.aux_view_font_size, config.margin,
                  config.fullscreen, config.tiles);
    }

#if defined(__ANDROID__) || defined(SIL_IOS)
    sdl_ensure_default_pane_configs_present(false);
    sdl_ensure_touch_pane_config_present();
#endif

    sdl_ensure_touch_pane_config_present();
    sdl_touch_pane_ensure_main_panel_confirm();

    /* Seed hidden-screen fallback layout from the configured pane groups
     * before the first resize/present, so startup screens do not briefly
     * show touch/right panes when that axis is disabled. */
    g_active_side_panes = config.enable_right_panes;
    g_active_bottom_panes = config.enable_bottom_panes;

    g_hide_left_panel = config.hide_left_panel;
    
    // Apply command-line overrides
    sdl_config_apply_cmdline(&config, argc, argv);
    log_debug("After command-line: scale=%d, default_aux_font=%d, margin=%d, fullscreen=%d, tiles=%d",
              config.main_view_scale, config.aux_view_font_size, config.margin,
              config.fullscreen, config.tiles);

#if defined(__ANDROID__) || defined(SIL_IOS)
    if (config_exists) {
        int mobile_min_cols = sdl_current_min_terminal_cols();
        int mobile_min_rows = sdl_current_min_terminal_rows();
        int mobile_max_scale_w = (screen_pixels_w / mobile_min_cols) * 2 / TILE_SIZE;
        int mobile_max_scale_h = screen_pixels_h / mobile_min_rows / TILE_SIZE;
        int mobile_max_scale = mobile_max_scale_w;

        if (mobile_max_scale_h < mobile_max_scale)
            mobile_max_scale = mobile_max_scale_h;
        if (mobile_max_scale < 1)
            mobile_max_scale = 1;

        if (config.main_view_scale > mobile_max_scale) {
            log_info("Mobile main_view_scale clamped from %d to %d to keep >=%dx%d (%s)",
                     config.main_view_scale, mobile_max_scale,
                     mobile_min_cols, mobile_min_rows,
                     sdl_min_terminal_mode_name(config.min_terminal_mode));
            sdl_append_issue_line(startup_issue_summary,
                sizeof(startup_issue_summary),
                "The saved main view scale was too large for the mobile display and was reduced.");
            config.main_view_scale = mobile_max_scale;
        }
    }
#endif
    
    // Validate configuration
    if (config.main_view_scale <= 0) {
        log_warn("Invalid main_view_scale %d, using 1", config.main_view_scale);
        if (config_exists) {
            sdl_append_issue_line(startup_issue_summary,
                sizeof(startup_issue_summary),
                "The saved main view scale was invalid and was reset to 1.");
        }
        config.main_view_scale = 1;
    }
    if (config.aux_view_font_size < 0) {
        log_warn("Invalid aux_view_font_size %d, using auto", config.aux_view_font_size);
        if (config_exists) {
            sdl_append_issue_line(startup_issue_summary,
                sizeof(startup_issue_summary),
                "The saved auxiliary font size was invalid and was reset to auto.");
        }
        config.aux_view_font_size = 0;
    } else if (config.aux_view_font_size > 48) {
        log_warn("Invalid aux_view_font_size %d, clamping to 48", config.aux_view_font_size);
        if (config_exists) {
            sdl_append_issue_line(startup_issue_summary,
                sizeof(startup_issue_summary),
                "The saved auxiliary font size was too large and was clamped.");
        }
        config.aux_view_font_size = 48;
    }
    if (config.margin < 0) {
        log_warn("Invalid margin %d, using 0", config.margin);
        if (config_exists) {
            sdl_append_issue_line(startup_issue_summary,
                sizeof(startup_issue_summary),
                "The saved window margin was invalid and was reset.");
        }
        config.margin = 0;
    }
    if (!sdl_min_terminal_mode_is_valid(config.min_terminal_mode)) {
#if defined(__ANDROID__) || defined(SIL_IOS)
        log_warn("Invalid min_terminal_mode %d, using compact", config.min_terminal_mode);
        config.min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
#else
        log_warn("Invalid min_terminal_mode %d, using normal", config.min_terminal_mode);
        config.min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
#endif
        if (config_exists) {
            sdl_append_issue_line(startup_issue_summary,
                sizeof(startup_issue_summary),
                "The saved minimum terminal mode was invalid and was reset.");
        }
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

    if (!config_exists) {
#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
        bool has_gamepad = (g_gamepad_state.pad_count > 0);

#if SIL_SDL_MOBILE_BUILD
        config.steamdeck_mode = has_gamepad;
        log_info("Mobile first-start controller UI mode set to %s (%d gamepad%s detected)",
            config.steamdeck_mode ? "on" : "off",
            g_gamepad_state.pad_count,
            (g_gamepad_state.pad_count == 1) ? "" : "s");
#elif SIL_SDL_DESKTOP_HANDHELD_BUILD
        desktop_handheld_first_start = has_gamepad
            && sdl_is_desktop_handheld_resolution(screen_pixels_w,
                screen_pixels_h);
        if (desktop_handheld_first_start) {
            config.steamdeck_mode = true;
            config.min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
        }
        log_info("Desktop handheld first-start mode %s (%dx%d, %d gamepad%s detected)",
            desktop_handheld_first_start ? "enabled" : "not enabled",
            screen_pixels_w, screen_pixels_h,
            g_gamepad_state.pad_count,
            (g_gamepad_state.pad_count == 1) ? "" : "s");
#endif
#endif
    }
    
    log_info("SDL Configuration:");
    log_info("  Main view scale: %d", config.main_view_scale);
    if (config.aux_view_font_size > 0)
        log_info("  Default aux view font size: %d", config.aux_view_font_size);
    else
        log_info("  Default aux view font size: auto (%d)", sdl_auto_aux_view_font_size());
    log_info("  Margin: %d", config.margin);
    log_info("  Fullscreen: %s", config.fullscreen ? "true" : "false");
    log_info("  Tiles: %s", config.tiles ? "true" : "false");
    log_info("  Use unsafe area: %s", config.use_unsafe_area ? "true" : "false");
    log_info("  Minimum terminal size: %s (%dx%d)",
             sdl_min_terminal_mode_name(config.min_terminal_mode),
             sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows());
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

    sdl_ensure_window_size_for_min_terminal(&screen, &window_width, &window_height);
    
    sdl_window_create(window_width, window_height, config.fullscreen, config.tiles);

    sdl_refresh_safe_area();
    if (config_exists) {
        sdl_layout_recovery_result startup_recovery;
        char recovery_note[256];

        if (sdl_recover_layout_for_current_window("startup", false,
                &startup_recovery))
        {
            sdl_format_layout_recovery_message("startup", &startup_recovery,
                recovery_note, sizeof(recovery_note));
            sdl_append_issue_line(startup_issue_summary,
                sizeof(startup_issue_summary), recovery_note);
        }
    }
    
    // Set window position for windowed mode
    if (!config.fullscreen && config.window_x >= 0 && config.window_y >= 0) {
        sdl_window_set_position(config.window_x, config.window_y);
    }

#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
    if (!config_exists && (SIL_SDL_MOBILE_BUILD || desktop_handheld_first_start)) {
        SDL_Rect handheld_screen;
        bool handheld_has_controller = (g_gamepad_state.pad_count > 0);

#if SIL_SDL_DESKTOP_HANDHELD_BUILD
        if (desktop_handheld_first_start)
            handheld_has_controller = true;
#endif
        sdl_ensure_default_pane_configs_present(false);
        sdl_refresh_safe_area();
        handheld_screen = sdl_get_layout_screen_rect();
        sdl_apply_mobile_default_pane_layout(&handheld_screen,
            handheld_has_controller);
        g_active_side_panes = config.enable_right_panes;
        g_active_bottom_panes = config.enable_bottom_panes;
    }
#endif
    
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

    sdl_refresh_safe_area();
    {
        SDL_Rect screen = sdl_get_layout_screen_rect();

        log_debug("window layout size %dx%d at (%d,%d)",
            screen.w, screen.h, screen.x, screen.y);
        resize(&screen);
    }

    if (config_exists && startup_issue_summary[0]) {
        bool old_fullscreen = config.fullscreen;

        if (sdl_prompt_reset_sdl_defaults(startup_issue_summary, screen_pixels_w,
                screen_pixels_h))
        {
            if (old_fullscreen != config.fullscreen)
                set_sdl_fullscreen(config.fullscreen);
            else
                sdl_apply_config();
        }
    }

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
    offset += (size_t)strnfmt(buf + offset, size - offset, "Minimum Terminal Size: %s (%dx%d)\n",
        sdl_min_terminal_mode_name(config.min_terminal_mode),
        sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows());
    if (config.aux_view_font_size > 0)
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Default Aux View Font Size: %d\n", config.aux_view_font_size);
    else
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Default Aux View Font Size: auto (%d)\n", sdl_auto_aux_view_font_size());
    offset += (size_t)strnfmt(buf + offset, size - offset, "Margin: %d\n", config.margin);
    offset += (size_t)strnfmt(buf + offset, size - offset, "Fullscreen: %s\n", config.fullscreen ? "Yes" : "No");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Tiles: %s\n", config.tiles ? "Yes" : "No");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Use Unsafe Area: %s\n",
        config.use_unsafe_area ? "Yes" : "No");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Pane Borders: %s\n",
        config.show_pane_borders ? "White" : "Black");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Hide Left Panel: %s\n\n",
        config.hide_left_panel ? "Yes" : "No");
    
    // Pane configurations (supporting panes only)
    offset += (size_t)strnfmt(buf + offset, size - offset, "=== Pane Configuration (Supporting Panes) ===\n");
    int support_count = 0;
    for (int i = 0; i < pane_config_count && i < MAX_PANE_CONFIGS; i++) {
        if (pane_config[i].pane != PANE_MAIN)
            support_count++;
    }
    offset += (size_t)strnfmt(buf + offset, size - offset, "Supporting Panes: %d\n\n", support_count);

    for (int i = 0; i < pane_config_count && i < MAX_PANE_CONFIGS; i++) {
        const struct pane_config* pc = &pane_config[i];
        if (pc->pane == PANE_MAIN)
            continue;
        const char* type_str = "UNKNOWN";
        const char* where_str = pane_placement_name(pc->where);
        
        switch (pc->pane) {
            case PANE_MAIN: type_str = "MAIN"; break;
            case PANE_INVENTORY: type_str = "INVENTORY"; break;
            case PANE_WORN: type_str = "WORN"; break;
            case PANE_ROLLS: type_str = "ROLLS"; break;
            case PANE_INFO: type_str = "INFO"; break;
            case PANE_CHARACTER: type_str = "CHARACTER"; break;
            case PANE_LOG: type_str = "LOG"; break;
            case PANE_MONSTERS: type_str = "MONSTERS"; break;
            case PANE_TOUCH: type_str = "TOUCH"; break;
            default: break;
        }
        
        offset += (size_t)strnfmt(buf + offset, size - offset, "Pane %d: %s\n", i + 1, type_str);
        offset += (size_t)strnfmt(buf + offset, size - offset, "  Placement: %s\n", where_str);
        offset += (size_t)strnfmt(buf + offset, size - offset, "  Enabled: %s\n", pc->enabled ? "yes" : "no");
        if (pc->rect.rows > 0)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Rows: %d\n", pc->rect.rows);
        if (pc->rect.cols > 0)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Cols: %d\n", pc->rect.cols);
        if (pc->font_size > 0)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Font Size: %d\n", pc->font_size);
        else
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Font Size: auto (%d)\n",
                sdl_effective_pane_font_size_for_config(pc));
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
    sdl_store_active_pane_profile(config.min_terminal_mode);
    sdl_config_save(config_file_path, &config, g_pane_profiles, SDL_PANE_PROFILE_COUNT);
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

int get_sdl_min_terminal_mode(void)
{
    return config.min_terminal_mode;
}

void set_sdl_min_terminal_mode(int value)
{
    bool log_was_active = sdl_is_log_pane_active();
    bool bottom_log_was_active = sdl_is_bottom_log_pane_active();

    if (!sdl_min_terminal_mode_is_valid(value))
        return;
    if (config.min_terminal_mode == value)
        return;

    sdl_store_active_pane_profile(config.min_terminal_mode);
    config.min_terminal_mode = value;
    sdl_apply_stored_pane_profile(value);
    sdl_handle_log_pane_activation(log_was_active, bottom_log_was_active);

    if (config.main_view_scale > get_sdl_max_scale())
        config.main_view_scale = get_sdl_max_scale();
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

int get_sdl_effective_aux_view_font_size(void)
{
    return sdl_resolve_aux_view_font_size(config.aux_view_font_size);
}

void set_sdl_aux_view_font_size(int value)
{
    if (value == 0 || (value >= 8 && value <= 48))
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
    if (config.fullscreen == value)
        return;

    config.fullscreen = value;

    // Apply fullscreen change immediately if window exists
    if (g_state.window) {
        if (value) {
            // Going to fullscreen - save current windowed position/size for later restoration
            SDL_GetWindowPosition(g_state.window, &config.window_x, &config.window_y);
            SDL_GetWindowSize(g_state.window, &config.window_width, &config.window_height);
            log_debug("Saving windowed position (%d, %d) and size (%dx%d) before fullscreen",
                     config.window_x, config.window_y, config.window_width, config.window_height);

            if (!SDL_SetWindowFullscreen(g_state.window, true)) {
                log_error("Failed to enter fullscreen: %s", SDL_GetError());
                config.fullscreen = false; // Revert on failure
                return;
            }
            log_info("Entered fullscreen mode");
        } else {
            // Going to windowed
            if (!SDL_SetWindowFullscreen(g_state.window, false)) {
                log_error("Failed to exit fullscreen: %s", SDL_GetError());
                config.fullscreen = true; // Revert on failure
                return;
            }

            // Restore saved window position and size
            if (config.window_width > 0 && config.window_height > 0) {
                SDL_SetWindowSize(g_state.window, config.window_width, config.window_height);
                if (config.window_x >= 0 && config.window_y >= 0) {
                    SDL_SetWindowPosition(g_state.window, config.window_x, config.window_y);
                }
                log_debug("Restored windowed position (%d, %d) and size (%dx%d)",
                         config.window_x, config.window_y, config.window_width, config.window_height);
            }
            log_info("Exited fullscreen mode");
        }

        // Force a resize event to recalculate layouts
        sdl_refresh_safe_area();
        (void)sdl_recover_layout_for_current_window("fullscreen change",
            true, NULL);
        sdl_load_story_fonts();
        sdl_resize_for_current_layout();
        sdl_update_cursor_visibility();

        // Redraw everything
        sdl_request_redraw();
    }
}

bool get_sdl_tiles(void)
{
    return config.tiles;
}

bool get_sdl_use_unsafe_area(void)
{
    return config.use_unsafe_area;
}

void set_sdl_use_unsafe_area(bool value)
{
    if (config.use_unsafe_area == value)
        return;

    config.use_unsafe_area = value;
    sdl_apply_config();
}

void set_sdl_tiles(bool value)
{
    config.tiles = value;
}

int get_pane_config_count(void)
{
    return pane_config_count;
}

/*
 * Accessors for the active pane configuration.
 * These are used by the interactive pane settings menu (cmd4.c).
 */
int get_sdl_pane_type(int index)
{
    if (index < 0 || index >= pane_config_count)
        return -1;
    return (int)pane_config[index].pane;
}

int get_sdl_pane_where(int index)
{
    if (index < 0 || index >= pane_config_count)
        return 0;
    return (int)pane_config[index].where;
}

static bool sdl_is_pane_group_enabled(enum pane_placement where)
{
    if (pane_placement_is_side(where))
        return config.enable_right_panes;
    if (pane_placement_is_bottom(where))
        return config.enable_bottom_panes;

    return false;
}

static bool sdl_is_log_pane_active(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane != PANE_LOG)
            continue;
        if (!pane_config[i].enabled)
            continue;
        if (!sdl_is_pane_group_enabled(pane_config[i].where))
            continue;

        return true;
    }

    return false;
}

static bool sdl_is_bottom_log_pane_active(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane != PANE_LOG)
            continue;
        if (!pane_config[i].enabled)
            continue;
        if (!pane_placement_is_bottom(pane_config[i].where))
            continue;
        if (!sdl_is_pane_group_enabled(pane_config[i].where))
            continue;

        return true;
    }

    return false;
}

static void sdl_disable_main_combat_rolls_for_log_pane(void)
{
    if (!op_ptr || op_ptr->main_combat_rolls == 0)
        return;

    op_ptr->main_combat_rolls = 0;

    if (Term)
        clear_main_combat_rolls_area();
    if (p_ptr)
        p_ptr->redraw |= PR_MAP;
}

static void sdl_enable_top_status_line_for_bottom_log_pane(void)
{
    if (!op_ptr || op_ptr->opt[OPT_top_status_line])
        return;

    if (!sdl_is_bottom_log_pane_active())
        return;

    op_ptr->opt[OPT_top_status_line] = true;

    if (p_ptr) {
        p_ptr->update |= PU_PANEL;
        p_ptr->redraw |= (PR_MAP | PR_EXTRA | PR_DEPTH);
    }
}

static void sdl_handle_log_pane_activation(bool log_was_active,
    bool bottom_log_was_active)
{
    if (!log_was_active && sdl_is_log_pane_active())
        sdl_disable_main_combat_rolls_for_log_pane();

    if (!bottom_log_was_active)
        sdl_enable_top_status_line_for_bottom_log_pane();
}

void set_sdl_pane_where(int index, int where)
{
    enum pane_placement placement = (enum pane_placement)where;
    bool log_was_active = sdl_is_log_pane_active();
    bool bottom_log_was_active = sdl_is_bottom_log_pane_active();

    if (index < 0 || index >= pane_config_count)
        return;
    if (!pane_type_allows_placement(pane_config[index].pane, placement))
        placement = pane_first_allowed_placement(pane_config[index].pane);

    pane_config[index].where = placement;
    sdl_handle_log_pane_activation(log_was_active, bottom_log_was_active);
}

bool get_sdl_pane_enabled(int index)
{
    if (index < 0 || index >= pane_config_count)
        return false;
    return pane_config[index].enabled;
}

int get_sdl_pane_rows(int index)
{
    if (index < 0 || index >= pane_config_count)
        return 0;
    return pane_config[index].rect.rows;
}

int get_sdl_pane_cols(int index)
{
    if (index < 0 || index >= pane_config_count)
        return 0;
    return pane_config[index].rect.cols;
}

int get_sdl_pane_font_size(int index)
{
    if (index < 0 || index >= pane_config_count)
        return 0;
    return pane_config[index].font_size;
}

int get_sdl_pane_effective_font_size(int index)
{
    if (index < 0 || index >= pane_config_count)
        return sdl_resolve_aux_view_font_size(config.aux_view_font_size);

    return sdl_effective_pane_font_size_for_config(&pane_config[index]);
}

static int sdl_pane_current_size(int index, bool want_rows)
{
    enum pane_type type;
    enum pane_placement where;
    int configured;

    if (index < 0 || index >= pane_config_count)
        return 0;

    type = pane_config[index].pane;
    if (type <= PANE_MAIN || type >= PANE_MAX)
        return 0;

    if (g_views[type].term_ready && g_pane_rects[type].w > 0 && g_pane_rects[type].h > 0) {
        int live = want_rows ? g_views[type].rows : g_views[type].cols;
        if (live > 0)
            return live;
    }

    configured = want_rows ? pane_config[index].rect.rows : pane_config[index].rect.cols;
    if (configured > 0)
        return configured;

    where = pane_config[index].where;
    if (want_rows) {
        return pane_placement_is_side(where)
            ? pane_secondary_min_cells(type, where)
            : pane_primary_min_cells(type, where);
    }

    return pane_placement_is_side(where)
        ? pane_primary_min_cells(type, where)
        : pane_secondary_min_cells(type, where);
}

int get_sdl_pane_current_rows(int index)
{
    return sdl_pane_current_size(index, true);
}

int get_sdl_pane_current_cols(int index)
{
    return sdl_pane_current_size(index, false);
}

void set_sdl_pane_rows(int index, int rows)
{
    if (index < 0 || index >= pane_config_count)
        return;
    if (rows < 0)
        rows = 0;
    if (rows > 200)
        rows = 200;
    pane_config[index].rect.rows = rows;
}

void set_sdl_pane_cols(int index, int cols)
{
    if (index < 0 || index >= pane_config_count)
        return;
    if (cols < 0)
        cols = 0;
    if (cols > 200)
        cols = 200;
    pane_config[index].rect.cols = cols;
}

void set_sdl_pane_font_size(int index, int font_size)
{
    if (index < 0 || index >= pane_config_count)
        return;
    if (font_size < 0)
        font_size = 0;
    if (font_size > 0 && font_size < 8)
        font_size = 8;
    if (font_size > 48)
        font_size = 48;
    pane_config[index].font_size = font_size;
}

void set_sdl_pane_enabled(int index, bool enabled)
{
    bool log_was_active = sdl_is_log_pane_active();
    bool bottom_log_was_active = sdl_is_bottom_log_pane_active();

    if (index < 0 || index >= pane_config_count)
        return;
    pane_config[index].enabled = enabled;
    sdl_handle_log_pane_activation(log_was_active, bottom_log_was_active);
}

bool get_sdl_enable_right_panes(void)
{
    return config.enable_right_panes;
}

void set_sdl_enable_right_panes(bool value)
{
    bool log_was_active = sdl_is_log_pane_active();
    bool bottom_log_was_active = sdl_is_bottom_log_pane_active();

    config.enable_right_panes = value;

    if (value) {
        for (int i = 0; i < pane_config_count; i++) {
            if (pane_placement_is_side(pane_config[i].where))
                pane_config[i].enabled = true;
        }
    }

    sdl_handle_log_pane_activation(log_was_active, bottom_log_was_active);
}

bool get_sdl_enable_bottom_panes(void)
{
    return config.enable_bottom_panes;
}

void set_sdl_enable_bottom_panes(bool value)
{
    bool log_was_active = sdl_is_log_pane_active();
    bool bottom_log_was_active = sdl_is_bottom_log_pane_active();

    config.enable_bottom_panes = value;

    /* On mobile, the default layout may keep the bottom-pane configs around
     * but disabled until the user explicitly turns the split on. Re-enable any
     * bottom-placed panes when the group is turned back on so the toggle does
     * what its label says. */
    if (value) {
        for (int i = 0; i < pane_config_count; i++) {
            if (pane_placement_is_bottom(pane_config[i].where))
                pane_config[i].enabled = true;
        }
    }

    sdl_handle_log_pane_activation(log_was_active, bottom_log_was_active);
}

bool get_sdl_show_pane_borders(void)
{
    return config.show_pane_borders;
}

void set_sdl_show_pane_borders(bool value)
{
    config.show_pane_borders = value;
}

bool get_sdl_hide_left_panel(void)
{
    return g_hide_left_panel;
}

void set_sdl_hide_left_panel(bool value)
{
    g_hide_left_panel = value;
    config.hide_left_panel = value;
}

int get_sdl_hidden_left_panel_mode(void)
{
    return config.hidden_left_panel_mode;
}

void set_sdl_hidden_left_panel_mode(int value)
{
    if (value != HIDDEN_LEFT_PANEL_TOPLINE)
        value = HIDDEN_LEFT_PANEL_TOP_LEFT;

    config.hidden_left_panel_mode = value;
}

/* Intro style: -1 = random (INTRO_STYLE_RANDOM), 0-4 = fixed variant. */
int get_sdl_intro_style(void)
{
    if (!op_ptr) return 0;
    return (op_ptr->intro_style == INTRO_STYLE_RANDOM)
        ? -1
        : (int)op_ptr->intro_style;
}

void set_sdl_intro_style(int style)
{
    if (!op_ptr) return;
    op_ptr->intro_style = (style == -1)
        ? INTRO_STYLE_RANDOM
        : (byte)(style < 0 ? 0 : style > 4 ? 4 : style);
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
    memcpy(g_default_gamepad_button_combo_bindings, defaults.gamepad_button_combo_bindings,
        sizeof(g_default_gamepad_button_combo_bindings));
    memcpy(g_default_gamepad_trigger_combo_bindings, defaults.gamepad_trigger_combo_bindings,
        sizeof(g_default_gamepad_trigger_combo_bindings));
    memcpy(g_default_gamepad_left_stick_combo_bindings, defaults.gamepad_left_stick_combo_bindings,
        sizeof(g_default_gamepad_left_stick_combo_bindings));
    memcpy(g_default_gamepad_right_stick_combo_bindings, defaults.gamepad_right_stick_combo_bindings,
        sizeof(g_default_gamepad_right_stick_combo_bindings));
    g_default_gamepad_shoulder_combo_binding = defaults.gamepad_shoulder_combo_binding;
    g_default_gamepad_bindings_ready = true;
}

static void sdl_touch_pane_load_default_bindings(void)
{
    if (g_default_touch_pane_bindings_ready)
        return;

    struct sdl_config defaults;
    sdl_config_set_defaults(&defaults);
    memcpy(g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_MAIN], defaults.touch_pane_bindings,
        sizeof(defaults.touch_pane_bindings));
    memcpy(g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_SECOND], defaults.touch_pane_second_bindings,
        sizeof(defaults.touch_pane_second_bindings));
    memcpy(g_default_touch_pane_panel_names, defaults.touch_pane_panel_names,
        sizeof(g_default_touch_pane_panel_names));
    g_default_touch_swipe_enabled = defaults.touch_swipe_enabled;
    memcpy(g_default_touch_swipe_bindings, defaults.touch_swipe_bindings,
        sizeof(g_default_touch_swipe_bindings));
    g_default_touch_pane_bindings_ready = true;
}

bool steamdeck_controls_active(void)
{
    if (config.steamdeck_mode)
        return true;
    if (!config.gamepad_enabled)
        return false;
    return (config.gamepad_auto_mode && g_gamepad_auto_ui);
}

bool portable_controls_active(void)
{
#if defined(SIL_USE_LOCAL_DATA) || defined(__ANDROID__) || defined(SIL_IOS)
    /* Portable builds and mobile platforms use the controller-style menu shortcuts. */
    return true;
#else
    return steamdeck_controls_active();
#endif
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
        g_touch_pane_second_panel = false;
        g_touch_pane_ctrl_toggle = false;
        sdl_touch_pane_cancel_press();
        sdl_touch_swipe_cancel();
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

bool get_sdl_steamdeck_inv_equip_same_button_cycle(void)
{
    return config.steamdeck_inv_equip_same_button_cycle;
}

void set_sdl_steamdeck_inv_equip_same_button_cycle(bool value)
{
    config.steamdeck_inv_equip_same_button_cycle = value;
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

int get_sdl_gamepad_combo_binding(int modifier, int type, int id)
{
    return sdl_gamepad_combo_binding_for_input(modifier, type, id);
}

void set_sdl_gamepad_combo_binding(int modifier, int type, int id, int binding)
{
    int modifier_index = sdl_gamepad_modifier_index(modifier);

    if (modifier_index < 0)
        return;

    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id >= 0 && id < SDL_GAMEPAD_BUTTON_COUNT)
            config.gamepad_button_combo_bindings[modifier_index][id] = binding;
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        if (id >= 0 && id < GAMEPAD_TRIGGER_COUNT)
            config.gamepad_trigger_combo_bindings[modifier_index][id] = binding;
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            config.gamepad_left_stick_combo_bindings[modifier_index][id] = binding;
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            config.gamepad_right_stick_combo_bindings[modifier_index][id] = binding;
        break;
    default:
        break;
    }
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

int get_sdl_gamepad_default_combo_binding(int modifier, int type, int id)
{
    int modifier_index = sdl_gamepad_modifier_index(modifier);

    if (modifier_index < 0)
        return GAMEPAD_BIND_NONE;

    sdl_gamepad_load_default_bindings();

    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id >= 0 && id < SDL_GAMEPAD_BUTTON_COUNT)
            return g_default_gamepad_button_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        if (id >= 0 && id < GAMEPAD_TRIGGER_COUNT)
            return g_default_gamepad_trigger_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return g_default_gamepad_left_stick_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return g_default_gamepad_right_stick_combo_bindings[modifier_index][id];
        break;
    default:
        break;
    }

    return GAMEPAD_BIND_NONE;
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

int get_sdl_touch_pane_binding(int index)
{
    return get_sdl_touch_pane_binding_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index);
}

int get_sdl_touch_pane_binding_for_panel(int panel, int index)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return GAMEPAD_BIND_NONE;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    return sdl_touch_pane_raw_binding_for_panel(panel, index);
}

void set_sdl_touch_pane_binding(int index, int binding)
{
    set_sdl_touch_pane_binding_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index, binding);
}

void set_sdl_touch_pane_binding_for_panel(int panel, int index, int binding)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;
    if (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        config.touch_pane_second_bindings[index] = binding;
    else
        config.touch_pane_bindings[index] = binding;

    sdl_touch_pane_ensure_main_panel_confirm();
}

int get_sdl_touch_pane_default_binding(int index)
{
    return get_sdl_touch_pane_default_binding_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index);
}

int get_sdl_touch_pane_default_binding_for_panel(int panel, int index)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return GAMEPAD_BIND_NONE;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_pane_bindings[panel][index];
}

bool get_sdl_touch_swipe_enabled(void)
{
    return config.touch_swipe_enabled;
}

void set_sdl_touch_swipe_enabled(bool value)
{
    config.touch_swipe_enabled = value;
    if (!value)
        sdl_touch_swipe_cancel();
}

int get_sdl_touch_swipe_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.touch_swipe_bindings[dir];
}

void set_sdl_touch_swipe_binding(int dir, int binding)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return;
    config.touch_swipe_bindings[dir] = binding;
}

bool get_sdl_touch_swipe_default_enabled(void)
{
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_swipe_enabled;
}

int get_sdl_touch_swipe_default_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_swipe_bindings[dir];
}

void sdl_touch_pane_reset_bindings_to_default(void)
{
    if (g_touch_pane_ctrl_toggle) {
        g_touch_pane_ctrl_toggle = false;
        sdl_gamepad_apply_modifier(GAMEPAD_BIND_CTRL, false);
    }

    g_touch_pane_second_panel = false;
    sdl_touch_pane_cancel_press();
    sdl_touch_swipe_cancel();
    sdl_config_set_default_touch_pane_bindings(&config);
    sdl_config_clear_touch_pane_labels(&config);
    sdl_touch_pane_ensure_main_panel_confirm();
}

cptr get_sdl_touch_pane_slot_name(int index)
{
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return "";
    return g_touch_pane_slots[index].slot_name;
}

void get_sdl_touch_pane_button_label(int index, char* buf, size_t buflen)
{
    get_sdl_touch_pane_button_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index, buf, buflen);
}

void set_sdl_touch_pane_button_label(int index, cptr label)
{
    set_sdl_touch_pane_button_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index, label);
}

void clear_sdl_touch_pane_button_label(int index)
{
    clear_sdl_touch_pane_button_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index);
}

void get_sdl_touch_pane_button_label_for_panel(int panel, int index, char* buf, size_t buflen)
{
    if (!sdl_touch_pane_panel_is_valid(panel)) {
        if (buf && buflen)
            buf[0] = '\0';
        return;
    }

    sdl_touch_pane_base_label_for_slot(panel, index, buf, buflen);
}

void set_sdl_touch_pane_button_label_for_panel(int panel, int index, cptr label)
{
    char (*labels)[SDL_TOUCH_PANE_LABEL_LEN];

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    labels = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? config.touch_pane_second_labels
        : config.touch_pane_labels;

    if (!label || !label[0]) {
        labels[index][0] = '\x01';
        labels[index][1] = '\0';
        return;
    }

    SDL_strlcpy(labels[index], label, SDL_TOUCH_PANE_LABEL_LEN);
}

void clear_sdl_touch_pane_button_label_for_panel(int panel, int index)
{
    char (*labels)[SDL_TOUCH_PANE_LABEL_LEN];

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    labels = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? config.touch_pane_second_labels
        : config.touch_pane_labels;
    labels[index][0] = '\0';
}

void get_sdl_touch_pane_panel_name(int panel, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;

    sdl_touch_pane_load_default_bindings();
    if (config.touch_pane_panel_names[panel][0]) {
        SDL_strlcpy(buf, config.touch_pane_panel_names[panel], buflen);
    } else {
        SDL_strlcpy(buf, g_default_touch_pane_panel_names[panel], buflen);
    }
}

void set_sdl_touch_pane_panel_name(int panel, cptr name)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return;

    if (!name || !name[0]) {
        config.touch_pane_panel_names[panel][0] = '\0';
        return;
    }

    SDL_strlcpy(config.touch_pane_panel_names[panel], name,
        sizeof(config.touch_pane_panel_names[panel]));
}

bool sdl_gamepad_capture_begin(bool allow_modifier_combo)
{
    g_gamepad_capture_ready = false;
    g_gamepad_capture_active = (g_gamepad_state.pad_count > 0);
    g_gamepad_capture_allow_modifier_combo = allow_modifier_combo;
    g_gamepad_capture_modifier = GAMEPAD_BIND_NONE;
    g_gamepad_capture_arm_time = SDL_GetTicksNS()
        + ((Uint64)GAMEPAD_CAPTURE_ARM_DELAY_MS * 1000000ULL);
    sdl_gamepad_clear_pending_shoulder();
    return g_gamepad_capture_active;
}

void sdl_gamepad_capture_cancel(void)
{
    g_gamepad_capture_active = false;
    g_gamepad_capture_ready = false;
    g_gamepad_capture_allow_modifier_combo = false;
    g_gamepad_capture_modifier = GAMEPAD_BIND_NONE;
    g_gamepad_capture_arm_time = 0;
    sdl_gamepad_clear_pending_shoulder();
}

bool sdl_gamepad_capture_poll(int* out_type, int* out_id, int* out_modifier)
{
    if (!g_gamepad_capture_ready)
        return false;

    if (out_type)
        *out_type = g_gamepad_capture_type;
    if (out_id)
        *out_id = g_gamepad_capture_id;
    if (out_modifier)
        *out_modifier = g_gamepad_capture_modifier;

    g_gamepad_capture_ready = false;
    g_gamepad_capture_active = false;
    g_gamepad_capture_allow_modifier_combo = false;
    g_gamepad_capture_modifier = GAMEPAD_BIND_NONE;
    g_gamepad_capture_arm_time = 0;
    sdl_gamepad_clear_pending_shoulder();
    return true;
}

/*
 * Calculate the maximum scale for the current window.
 * This keeps at least the configured minimum terminal size visible in the
 * current window.
 */
int get_sdl_max_scale(void)
{
    if (!g_state.window) {
        return 10; // fallback if window not initialized
    }
    
    SDL_Rect screen;
    SDL_Rect panes[PANE_MAX];
    int max_scale;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    sdl_compute_split_panes(&screen, panes);
    max_scale = sdl_max_scale_for_rect(&panes[PANE_MAIN]);

    log_debug("get_sdl_max_scale: layout=(%d,%d %dx%d) main=%dx%d min=%dx%d (%s) max_scale=%d",
              screen.x, screen.y, screen.w, screen.h,
              panes[PANE_MAIN].w, panes[PANE_MAIN].h,
              sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows(),
              sdl_min_terminal_mode_name(config.min_terminal_mode), max_scale);
    
    return max_scale;
}

void sdl_refresh_supporting_panes_layout(void)
{
    SDL_Rect screen;
    bool target_show_supporting_panes;

    if (!g_state.window)
        return;
    target_show_supporting_panes = sdl_should_show_supporting_panes();
    if (g_supporting_panes_layout_visible == target_show_supporting_panes)
        return;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    /* The caller will either redraw the destination scene or restore a saved
     * main-term buffer immediately after the layout change. Redrawing the old
     * outgoing main contents here is what produces the visible "flash" frame. */
    g_skip_main_redraw_on_layout_refresh = true;
    g_suppress_layout_refresh_present = true;
    resize(&screen);
    g_suppress_layout_refresh_present = false;
    if (g_skip_main_redraw_on_layout_refresh)
        g_state.need_present = false;
    g_skip_main_redraw_on_layout_refresh = false;
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

    (void)sdl_recover_layout_for_current_window("settings change", true, NULL);
    sdl_refresh_safe_area();
    g_auto_aux_main_cell_h_override = config.main_view_scale * TILE_SIZE;
    sdl_load_story_fonts();
    sdl_resize_for_current_layout();
    g_auto_aux_main_cell_h_override = 0;
    
    // Redraw the screen to prevent black empty spaces
    sdl_request_redraw();
}

void sdl_request_redraw(void)
{
    if (!g_state.window) {
        log_warn("sdl_request_redraw: no window, skipping");
        return;
    }

    g_state.need_present = true;
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
        Uint32 ch = (unsigned char)s[i];
        if (!ch || ch == ' ')
            continue;

        SDL_Surface* glyph_surface = TTF_RenderGlyph_Blended(font, ch, col);
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

