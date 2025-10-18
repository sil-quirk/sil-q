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
