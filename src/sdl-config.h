#pragma once

#include <stdbool.h>
#include "pane.h"

// SDL-specific configuration structure
struct sdl_config {
    int main_view_scale;
    int aux_view_font_size;
    int margin;
    bool fullscreen;
    bool tiles;
    
    // Window position and size for windowed mode
    int window_x;
    int window_y;
    int window_width;
    int window_height;
    
    // Custom fonts
    char story_font[256];      // Font for story/narrative text (non-monospace, e.g., "lib/xtra/font/Story.ttf")
    char monospace_font[256];  // Font for regular game text (monospace, default: InputMono-Bold.ttf)
    
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
};

// Load SDL configuration from JSON file
void sdl_config_load(const char* filename, struct sdl_config* config, 
                     struct pane_config* pane_configs, int* pane_count, int max_panes);

// Save SDL configuration to JSON file
void sdl_config_save(const char* filename, const struct sdl_config* config,
                     const struct pane_config* pane_configs, int pane_count);

// Set default configuration values
void sdl_config_set_defaults(struct sdl_config* config);

// Set default configuration values based on screen resolution
void sdl_config_set_defaults_for_resolution(struct sdl_config* config, 
                                            struct pane_config* pane_configs,
                                            int* pane_count,
                                            int max_panes,
                                            int screen_width,
                                            int screen_height);

// Apply command-line arguments to configuration
void sdl_config_apply_cmdline(struct sdl_config* config, int argc, char** argv);
