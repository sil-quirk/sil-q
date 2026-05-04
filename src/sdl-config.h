#pragma once

#include <stdbool.h>
#include <SDL3/SDL_gamepad.h>
#include "pane.h"

#define GAMEPAD_TRIGGER_COUNT 2
#define GAMEPAD_STICK_DIR_COUNT 4
#define TOUCH_SWIPE_DIR_COUNT 4
#define GAMEPAD_MODIFIER_COUNT 3

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

#define SDL_TOUCH_MOVEMENT_ON 0
#define SDL_TOUCH_MOVEMENT_OFF 1
#define SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY 2

#define SDL_MOUSE_MOVEMENT_ON 0
#define SDL_MOUSE_MOVEMENT_OFF 1
#define SDL_MOUSE_MOVEMENT_RIGHT_ONLY 2

#define GAMEPAD_BIND_NONE -1
#define GAMEPAD_BIND_SHIFT -2
#define GAMEPAD_BIND_CTRL -3
#define GAMEPAD_BIND_ALT -4
#define INPUT_BIND_CONFIRM -5
#define TOUCH_PANE_BIND_INHERIT -6
#define TOUCH_BIND_TOP_PANEL_OPEN -7
#define TOUCH_BIND_TOP_PANEL_CLOSE -8

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
#define SDL_TOUCH_TOP_PANEL_MODE_SHORT 0
#define SDL_TOUCH_TOP_PANEL_MODE_LONG 1
#define SDL_TOUCH_TOP_PANEL_MODE_COUNT 2
#define SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT 4
#define SDL_TOUCH_TOP_PANEL_BUTTON_COUNT 6
#define SDL_PANE_PROFILE_COUNT 2

enum sdl_min_terminal_mode {
    SDL_MIN_TERMINAL_NORMAL = 0,
    SDL_MIN_TERMINAL_COMPACT = 1,
};

enum sdl_config_load_status {
    SDL_CONFIG_LOAD_OK = 0,
    SDL_CONFIG_LOAD_READ_FAILED,
    SDL_CONFIG_LOAD_PARSE_FAILED,
};

struct sdl_pane_profile {
    int main_view_scale;
    int aux_view_font_size;
    bool enable_right_panes;
    bool enable_bottom_panes;
    int pane_count;
    struct pane_config pane_configs[MAX_PANE_CONFIGS];
};

// SDL-specific configuration structure
struct sdl_config {
    int main_view_scale;
    // Default supporting-pane font size. Zero means auto from the main pane's
    // visible font/cell height.
    int aux_view_font_size;
    int margin;
    bool fullscreen;
    bool tiles;
    bool use_unsafe_area;
    bool enable_right_panes;
    bool enable_bottom_panes;
    bool show_pane_borders;
    bool hide_left_panel;
    int hidden_left_panel_mode;
    int min_terminal_mode;
    
    // Window position and size for windowed mode
    int window_x;
    int window_y;
    int window_width;
    int window_height;
    
    // Custom fonts
    char story_font[256];      // Font for story/narrative text (non-monospace, e.g., "lib/xtra/font/Story.ttf")
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
    int touch_top_panel_mode;
    bool touch_top_panel_default_open;
    int touch_top_panel_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int touch_top_panel_long_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    bool touch_swipe_enabled;
    int touch_swipe_bindings[TOUCH_SWIPE_DIR_COUNT];
};

// Load SDL configuration from JSON file
enum sdl_config_load_status sdl_config_load(const char* filename,
    struct sdl_config* config, struct sdl_pane_profile* pane_profiles,
    int profile_count);

// Save SDL configuration to JSON file
void sdl_config_save(const char* filename, const struct sdl_config* config,
                     const struct sdl_pane_profile* pane_profiles, int profile_count);

// Set default configuration values
void sdl_config_set_defaults(struct sdl_config* config);

// Set default gamepad bindings (does not touch other fields)
void sdl_config_set_default_gamepad_bindings(struct sdl_config* config);

// Set default touch pane bindings (does not touch other fields)
void sdl_config_set_default_touch_pane_bindings(struct sdl_config* config);

// Clear custom touch pane labels (does not touch other fields)
void sdl_config_clear_touch_pane_labels(struct sdl_config* config);

// Set default configuration values based on screen resolution
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
bool sdl_config_should_force_intro_flame(void);
void sdl_config_mark_intro_seen(void);
bool sdl_config_touch_tutorial_seen(void);
void sdl_config_mark_touch_tutorial_seen(void);
bool sdl_config_mouse_tutorial_seen(void);
void sdl_config_mark_mouse_tutorial_seen(void);
bool option_is_app_persistent(int opt);
