#pragma once

#include <stdbool.h>
#include <SDL3/SDL_gamepad.h>
#include "pane.h"
#include "support/movement-input.h"

#define GAMEPAD_TRIGGER_COUNT 2
#define GAMEPAD_STICK_DIR_COUNT 4
#define TOUCH_SWIPE_DIR_COUNT 4
#define GAMEPAD_MODIFIER_COUNT 3
#define SDL_KEYMAP_MODE_COUNT 4
#define SDL_KEYMAP_KEY_COUNT 256
#define SDL_KEYMAP_ACTION_LEN 16
#define SDL_MOVEMENT_BINDING_MAX 96

#define GAMEPAD_MODIFIER_SHIFT 0
#define GAMEPAD_MODIFIER_CTRL 1
#define GAMEPAD_MODIFIER_ALT 2

#define GAMEPAD_STICK_DIR_UP 0
#define GAMEPAD_STICK_DIR_DOWN 1
#define GAMEPAD_STICK_DIR_LEFT 2
#define GAMEPAD_STICK_DIR_RIGHT 3

#define TOUCH_SWIPE_DIR_UP 0
#define TOUCH_SWIPE_DIR_DOWN 1
#define TOUCH_SWIPE_DIR_LEFT 2
#define TOUCH_SWIPE_DIR_RIGHT 3

#define SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT 0
#define SDL_TOUCH_MENU_CATEGORY_SUPPLY 1
#define SDL_TOUCH_MENU_CATEGORY_OTHER 2
#define SDL_TOUCH_MENU_CATEGORY_COUNT 3

#define SDL_TOUCH_PROFILE_TOUCH_PANE 0
#define SDL_TOUCH_PROFILE_CORNERS 1
#define SDL_TOUCH_PROFILE_ROUND_WHEEL 2
#define SDL_TOUCH_PROFILE_COUNT 3

#define SDL_PORTRAIT_OVERLAY_LOG_PANE_DEFAULT_ROWS 7

#define SDL_TOUCH_MOVEMENT_ON 0
#define SDL_TOUCH_MOVEMENT_OFF 1
#define SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY 2

#define SDL_MOUSE_MOVEMENT_ON 0
#define SDL_MOUSE_MOVEMENT_OFF 1
#define SDL_MOUSE_MOVEMENT_RIGHT_ONLY 2

#define SDL_MOVEMENT_PRESET_NONE 0
#define SDL_MOVEMENT_PRESET_MODERN_ARROWS 1
#define SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC 2
#define SDL_MOVEMENT_PRESET_VI_KEYS 3
#define SDL_MOVEMENT_PRESET_CLASSIC_SIL 4

#define GAMEPAD_BIND_NONE -1
#define GAMEPAD_BIND_SHIFT -2
#define GAMEPAD_BIND_CTRL -3
#define GAMEPAD_BIND_ALT -4
#define INPUT_BIND_CONFIRM -5
#define TOUCH_PANE_BIND_INHERIT -6
#define TOUCH_BIND_TOP_PANEL_OPEN -7
#define TOUCH_BIND_TOP_PANEL_CLOSE -8
#define TOUCH_BIND_MAIN_MENU_KNOWLEDGE -9
#define TOUCH_BIND_MAIN_MENU_HINTS_QUESTS -10
#define TOUCH_BIND_TOGGLE_TILES -11

#define SDL_TOUCH_PANE_BUTTON_COLS 3
#define SDL_TOUCH_PANE_BUTTON_ROWS 8
#define SDL_TOUCH_PANE_BUTTON_COUNT (SDL_TOUCH_PANE_BUTTON_COLS * SDL_TOUCH_PANE_BUTTON_ROWS)
#define SDL_TOUCH_PANE_VISIBLE_BUTTON_ROWS 7
#define SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT (SDL_TOUCH_PANE_BUTTON_COLS * SDL_TOUCH_PANE_VISIBLE_BUTTON_ROWS)
#define SDL_TOUCH_PANE_LABEL_LEN 24
#define SDL_TOUCH_PANE_PANEL_COUNT 2
#define SDL_TOUCH_PANE_PANEL_MAIN 0
#define SDL_TOUCH_PANE_PANEL_SECOND 1
#define SDL_TOUCH_PANE_PLACEMENT_LEFT 0
#define SDL_TOUCH_PANE_PLACEMENT_RIGHT 1
#define SDL_TOUCH_ZONE_OVERLAY_OFF 0
#define SDL_TOUCH_ZONE_OVERLAY_MARKERS 1
#define SDL_TOUCH_ZONE_OVERLAY_BORDERS 2
#define SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS 3
#define SDL_TOUCH_ZONE_OVERLAY_COUNT 4
#define SDL_TOUCH_ZONE_CENTER_LEFT_TAP 0
#define SDL_TOUCH_ZONE_CENTER_LEFT_LONG_TAP 1
#define SDL_TOUCH_ZONE_CENTER_RIGHT_TAP 2
#define SDL_TOUCH_ZONE_CENTER_RIGHT_LONG_TAP 3
#define SDL_TOUCH_ZONE_CENTER_BINDING_COUNT 4
#define SDL_TOUCH_CORNER_UP_DOWN_LEFT 0
#define SDL_TOUCH_CORNER_UP_DOWN_RIGHT 1
#define SDL_TOUCH_CORNER_ACTION_TOP_TAP 0
#define SDL_TOUCH_CORNER_ACTION_TOP_LONG_TAP 1
#define SDL_TOUCH_CORNER_ACTION_BOTTOM_TAP 2
#define SDL_TOUCH_CORNER_ACTION_BOTTOM_LONG_TAP 3
#define SDL_TOUCH_CORNER_ACTION_BINDING_COUNT 4
#define SDL_TOUCH_TOP_PANEL_BUTTON_COUNT 16
#define SDL_TOUCH_TOP_PANEL_CELL_COUNT_MIN 0
#define SDL_TOUCH_TOP_PANEL_CELL_COUNT_MAX SDL_TOUCH_TOP_PANEL_BUTTON_COUNT
#define SDL_TOUCH_TOP_PANEL_CELL_COUNT_DEFAULT 8
#define SDL_TOUCH_TOP_PANEL_COLUMNS_MIN 0
#define SDL_TOUCH_TOP_PANEL_COLUMNS_MAX SDL_TOUCH_TOP_PANEL_BUTTON_COUNT
#define SDL_TOUCH_TOP_PANEL_ROWS_MIN 1
#define SDL_TOUCH_TOP_PANEL_ROWS_MAX 2
#define SDL_TOUCH_TOP_PANEL_ROWS_DEFAULT 1
#define SDL_TOUCH_TOP_PANEL_SIZE_STRETCH 0.0f
#define SDL_TOUCH_TOP_PANEL_SIZE_MIN 1.0f
#define SDL_TOUCH_TOP_PANEL_SIZE_MAX 8.0f
#define SDL_TOUCH_TOP_PANEL_SIZE_STEP 0.25f
#if defined(__ANDROID__) || defined(SIL_IOS)
#define SDL_TOUCH_TOP_PANEL_SIZE_DEFAULT 3.0f
#else
#define SDL_TOUCH_TOP_PANEL_SIZE_DEFAULT 6.0f
#endif
#define SDL_TOUCH_THUMB_BUTTON_COUNT 2
#define SDL_MIN_TERMINAL_MODE_COUNT 2
#define SDL_PANE_ORIENTATION_COUNT 2
#define SDL_PANE_PROFILE_COUNT \
    (SDL_MIN_TERMINAL_MODE_COUNT * SDL_PANE_ORIENTATION_COUNT)
#define SDL_LEFT_PANEL_COMPACT_COLUMN 0
#define SDL_LEFT_PANEL_COMPACT_ROW 1
#define SDL_LEFT_PANEL_COMPACT_COUNT 2
#define SDL_MAIN_VIEW_MIN_SCALE 1
#define SDL_MAIN_VIEW_PREFERRED_MIN_SCALE 2
#define SDL_MAIN_VIEW_MAX_SCALE 20
#define SDL_TERMINAL_MENU_SCALE_OFFSET_MIN \
    (SDL_MAIN_VIEW_MIN_SCALE - SDL_MAIN_VIEW_MAX_SCALE)
#define SDL_TERMINAL_MENU_SCALE_OFFSET_DEFAULT (-1)
#define SDL_TERMINAL_MENU_SCALE_OFFSET_MAX 0
#define SDL_MOBILE_STARTING_ZOOM_OFFSET_MIN 0
#define SDL_MOBILE_STARTING_ZOOM_OFFSET_DEFAULT 2
#define SDL_MOBILE_STARTING_ZOOM_OFFSET_MAX \
    (SDL_MAIN_VIEW_MAX_SCALE - SDL_MAIN_VIEW_MIN_SCALE)
#define SDL_DICE_ROLL_LOCK_DEFAULT_MS 2000
#define SDL_DICE_ROLL_OVERLAY_DEFAULT_MS 2500
#define SDL_DICE_ROLL_TIMING_MAX_MS 10000
#define SDL_POPUP_NOTIFICATION_DEFAULT_MS 2000
#define SDL_POPUP_NOTIFICATION_MAX_MS 10000
#define SDL_CAMERA_CENTER_CLEARANCE_MIN 1
#define SDL_CAMERA_CENTER_CLEARANCE_MAX 20
#if defined(__ANDROID__) || defined(SIL_IOS)
#define SDL_CAMERA_CENTER_CLEARANCE_DEFAULT 3
#else
#define SDL_CAMERA_CENTER_CLEARANCE_DEFAULT 5
#endif

enum sdl_min_terminal_mode {
    SDL_MIN_TERMINAL_NORMAL = 0,
    SDL_MIN_TERMINAL_COMPACT = 1,
};

enum sdl_pane_orientation {
    SDL_PANE_ORIENTATION_LANDSCAPE = 0,
    SDL_PANE_ORIENTATION_PORTRAIT = 1,
};

#define SDL_PANE_PROFILE_INDEX(ORIENTATION, MODE) \
    ((ORIENTATION) * SDL_MIN_TERMINAL_MODE_COUNT + (MODE))

enum sdl_config_load_status {
    SDL_CONFIG_LOAD_OK = 0,
    SDL_CONFIG_LOAD_READ_FAILED,
    SDL_CONFIG_LOAD_PARSE_FAILED,
};

/* Records whether device-dependent touch defaults were explicitly present in
 * the loaded file.  Older builds wrote only a partial touchControl object, so
 * startup needs to distinguish "missing" from a deliberate false value. */
struct sdl_config_load_info {
    bool touch_profile_present;
    bool touch_round_movement_present;
};

struct sdl_pane_profile {
    int main_view_scale;
    int aux_view_font_size;
    bool enable_right_panes;
    bool enable_bottom_panes;
    bool left_overlays_touch_screen_edge;
    bool show_overlay_log_border;
    bool show_main_menu_button;
    bool left_panel_expanded_on_launch;
    int left_panel_compact_mode;
    int log_pane_display_filter;
    int dice_roll_lock_ms;
    int dice_roll_overlay_ms;
    int popup_notification_ms;
    bool touch_top_panel_arrows_visible;
    bool touch_top_panel_default_open;
    int touch_top_panel_cell_count;
    int touch_top_panel_rows;
    float touch_top_panel_size;
    int touch_top_panel_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int touch_top_panel_long_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int pane_count;
    struct pane_config pane_configs[MAX_PANE_CONFIGS];
};

// SDL-specific configuration structure
struct sdl_config {
    int main_view_scale;
    // Scale steps relative to the largest terminal-menu scale that fits.
    int terminal_menu_scale_offset;
    // Extra scale steps applied when mobile gameplay first appears.
    int mobile_starting_zoom_offset;
    // On mobile, request a real portrait device orientation.  Landscape
    // remains the default for existing desktop and handheld layouts.
    bool mobile_portrait_mode;
    // Default supporting-pane font size. Zero means auto from the configured
    // main view scale, independent of temporary main-map zoom.
    int aux_view_font_size;
    int margin;
    bool fullscreen;
    bool tiles;
    bool use_unsafe_area;
    bool enable_right_panes;
    bool enable_bottom_panes;
    bool show_pane_borders;
    bool left_overlays_touch_screen_edge;
    bool show_overlay_log_border;
    bool left_panel_expanded_on_launch;
    int left_panel_compact_mode;
    int min_terminal_mode;
    int log_pane_display_filter;
    int dice_roll_lock_ms;
    int dice_roll_overlay_ms;
    int popup_notification_ms;
    bool show_main_menu_button;
    int camera_center_clearance;
    
    // Window position and size for windowed mode
    int window_x;
    int window_y;
    int window_width;
    int window_height;
    
    // Custom fonts
    char story_font[256];      // Font for story/narrative text (non-monospace, e.g., "lib/xtra/font/Story.ttf")
    char story_font2[256];     // Second story font, used by select UI contexts (e.g. menus, log, quest book)
    char monospace_font[256];  // Font for regular game text (monospace, default: VictorMono-Medium.ttf)
    
    // Monospace font rendering options
    bool mono_bold;            // Apply bold style to monospace font
    bool mono_italic;          // Apply italic style to monospace font
    bool mono_underline;       // Apply underline style to monospace font
    bool mono_strikethrough;   // Apply strikethrough style to monospace font
    int mono_hinting;          // TTF hinting mode: 0=normal, 1=light, 2=mono, 3=none, 4=light_subpixel
    bool mono_kerning;         // Enable kerning (default: true)
    int mono_outline;          // Outline width in pixels (0=none)
    
    // Story font rendering options
    bool story_bold;           // Apply bold style to story font
    bool story_italic;         // Apply italic style to story font
    bool story_underline;      // Apply underline style to story font
    bool story_strikethrough;  // Apply strikethrough style to story font
    int story_hinting;         // TTF hinting mode: 0=normal, 1=light, 2=mono, 3=none, 4=light_subpixel
    bool story_kerning;        // Enable kerning (default: true)
    int story_outline;         // Outline width in pixels (0=none)

    // Second story font rendering options (slot 1)
    bool story2_bold;          // Apply bold style to second story font
    bool story2_italic;        // Apply italic style to second story font
    bool story2_underline;     // Apply underline style to second story font
    bool story2_strikethrough; // Apply strikethrough style to second story font
    int story2_hinting;        // TTF hinting mode: 0=normal, 1=light, 2=mono, 3=none, 4=light_subpixel
    bool story2_kerning;       // Enable kerning (default: true)
    int story2_outline;        // Outline width in pixels (0=none)

    // Palette and keyboard keymaps persisted in sil_sdl.json.
    char palette_preset[64];
    char keymap_actions[SDL_KEYMAP_MODE_COUNT][SDL_KEYMAP_KEY_COUNT]
        [SDL_KEYMAP_ACTION_LEN];
    bool movement_keyboard_present;
    u16b movement_keyboard_preset;
    u16b movement_binding_count;
    movement_input_binding movement_bindings[SDL_MOVEMENT_BINDING_MAX];

    // Gamepad/controller settings
    bool gamepad_enabled;                 // Enable gamepad input
    bool gamepad_auto_mode;               // Auto-enable controller UI when gamepad is present/used
    bool steamdeck_mode;                  // Controller UI mode setting
    bool steamdeck_inv_equip_same_button_cycle; // In controller UI, pressing inventory/equipment again cycles to the other menu
    bool gamepad_use_dpad;                // Use d-pad for movement
    bool gamepad_use_left_stick;          // Use left stick for movement
    int gamepad_deadzone;                 // Deadzone for analog sticks
    int gamepad_trigger_threshold;        // Threshold to treat triggers as pressed
    int gamepad_button_bindings[SDL_GAMEPAD_BUTTON_COUNT];
    int gamepad_trigger_bindings[GAMEPAD_TRIGGER_COUNT];
    int gamepad_left_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
    int gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
    int gamepad_button_combo_bindings[GAMEPAD_MODIFIER_COUNT][SDL_GAMEPAD_BUTTON_COUNT];
    int gamepad_trigger_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_TRIGGER_COUNT];
    int gamepad_left_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
    int gamepad_right_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
    int gamepad_shoulder_combo_binding;   // Binding for L1+R1 combo action
    bool mouse_enabled;
    int mouse_movement_mode;
    bool mouse_tile_pointer;
    int touch_profile;
    bool touch_pane_default_open;
    bool touch_pane_key_labels_visible;
    bool touch_pane_inventory_equipment_cycle;
    int touch_pane_bindings[SDL_TOUCH_PANE_BUTTON_COUNT];
    char touch_pane_labels[SDL_TOUCH_PANE_BUTTON_COUNT][SDL_TOUCH_PANE_LABEL_LEN];
    int touch_pane_second_bindings[SDL_TOUCH_PANE_BUTTON_COUNT];
    char touch_pane_second_labels[SDL_TOUCH_PANE_BUTTON_COUNT][SDL_TOUCH_PANE_LABEL_LEN];
    char touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_LABEL_LEN];
    bool touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_COUNT];
    int touch_movement_mode;
    bool touch_round_movement_enabled;
    int touch_zone_overlay_mode;
    int touch_zone_center_bindings[SDL_TOUCH_ZONE_CENTER_BINDING_COUNT];
    int touch_corner_up_down_side;
    int touch_corner_action_bindings[SDL_TOUCH_CORNER_ACTION_BINDING_COUNT];
    bool touch_top_panel_arrows_visible;
    bool touch_top_panel_default_open;
    float touch_top_panel_size;
    int touch_top_panel_cell_count;
    int touch_top_panel_rows;
    int touch_top_panel_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int touch_top_panel_long_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    bool touch_thumb_enabled;
    int touch_thumb_bindings[SDL_TOUCH_THUMB_BUTTON_COUNT];
    int touch_thumb_long_bindings[SDL_TOUCH_THUMB_BUTTON_COUNT];
    bool touch_swipe_enabled;
    int touch_swipe_bindings[TOUCH_SWIPE_DIR_COUNT];
};

extern struct sdl_config config;

// Load SDL configuration from JSON file
enum sdl_config_load_status sdl_config_load(const char* filename,
    struct sdl_config* config, struct sdl_pane_profile* pane_profiles,
    int profile_count, struct sdl_config_load_info* load_info);

// Save SDL configuration to JSON file
void sdl_config_save(const char* filename, const struct sdl_config* config,
                     const struct sdl_pane_profile* pane_profiles, int profile_count);

/* Seed a profile with the portrait HUD defaults formerly imposed at render
 * time.  Once seeded, the values remain ordinary editable pane settings. */
void sdl_pane_config_apply_portrait_default(struct pane_config* pane);
void sdl_pane_profile_apply_portrait_defaults(
    struct sdl_pane_profile* profile);

// Set default configuration values
void sdl_config_set_defaults(struct sdl_config* config);
void sdl_config_apply_keyboard_keymaps(const struct sdl_config* config);

// Keyboard movement bindings persisted in sil_sdl.json.
const char* sdl_config_movement_preset_label(u16b preset_id);
u16b sdl_config_next_movement_preset(u16b preset_id);
void sdl_config_clear_movement_bindings(struct sdl_config* config);
void sdl_config_set_default_movement_bindings(struct sdl_config* config,
    u16b preset_id);
bool sdl_config_append_movement_binding(struct sdl_config* config,
    const movement_input_binding* binding);
bool sdl_config_set_movement_binding(struct sdl_config* config, u16b action,
    u16b direction, const movement_input_binding* binding);
bool sdl_config_resolve_movement_binding(const struct sdl_config* config,
    u16b context, u32b trigger, u32b trigger_aux, u16b modifiers,
    movement_input_command* out_command);
// True when a movement preset binds this letter scancode to a plain (no
// modifier) movement action, shadowing that letter's normal command.
bool sdl_config_scancode_is_plain_movement_letter(
    const struct sdl_config* config, u32b scancode);

// Set default gamepad bindings (does not touch other fields)
void sdl_config_set_default_gamepad_bindings(struct sdl_config* config);

// Set default touch pane bindings (does not touch other fields)
void sdl_config_set_default_touch_pane_bindings(struct sdl_config* config);

// Clear custom touch pane labels (does not touch other fields)
void sdl_config_clear_touch_pane_labels(struct sdl_config* config);

// Set first-run configuration values. Resolution presets are not applied.
bool sdl_config_set_defaults_for_resolution(struct sdl_config* config,
                                            struct pane_config* pane_configs,
                                            int* pane_count,
                                            int max_panes,
                                            int screen_width,
                                            int screen_height);

// Apply command-line arguments to configuration
void sdl_config_apply_cmdline(struct sdl_config* config, int argc, char** argv);

// Load/save app-wide game options from/to the SDL JSON config file.
void sdl_config_load_app_options(const char* filename);
void sdl_config_reset_app_options_to_defaults(void);
bool sdl_config_should_force_intro_flame(void);
void sdl_config_mark_intro_seen(void);
bool sdl_config_touch_tutorial_seen(void);
void sdl_config_mark_touch_tutorial_seen(void);
bool sdl_config_mouse_tutorial_seen(void);
void sdl_config_mark_mouse_tutorial_seen(void);
bool sdl_config_character_wheel_tutorial_seen(void);
void sdl_config_mark_character_wheel_tutorial_seen(void);
bool sdl_config_keyboard_preset_prompt_seen(void);
void sdl_config_mark_keyboard_preset_prompt_seen(void);
bool option_is_app_persistent(int opt);
