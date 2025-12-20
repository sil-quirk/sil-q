#include "angband.h"
#include "externs.h"
#include "sdl-config.h"
#include "log/log.h"
#include "pane.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JSON-based configuration system using cJSON library

// Resolution-specific default configuration profile
// Only includes values that differ per resolution
struct resolution_profile {
    int width;
    int height;
    const char* name;
    
    // Resolution-specific SDL settings
    int main_view_scale;
    int aux_view_font_size;
    
    // Pane configurations (up to 8 panes)
    int pane_count;
    struct {
        enum pane_type type;
        enum pane_placement where;
        int rows;
        int cols;
    } panes[8];
};

// Resolution profiles database - add new resolutions here!
// 
// Only resolution-specific values are stored here.
// Common defaults (margin=4, fullscreen=true, tiles=true) are set in sdl_config_set_defaults()
// 
// LAYOUT CALCULATION LOGIC:
// 1. Minimum main terminal: 40x24 tiles (in tile mode with 16x16 base tile size)
// 2. Try maximum scale (up to 4) that fits: scale 4 = 2560x1536, scale 3 = 1920x1152, scale 2 = 1280x768, scale 1 = 640x384
// 3. Aux view font size: proportional to scale (scale 4 = 24px, scale 3 = 18px, scale 2 = 16px, scale 1 = 9px)
// 4. Right pane: if we can fit ≥40 columns (using ~0.6*font_size char width), add right pane
//    - Right pane contains: Inventory (22 rows), Worn (17 rows), Info (remaining, rows=0 means auto)
//    - Right pane width: 40-50 columns depending on available space
// 5. Bottom pane: if we can fit ≥1 row below main terminal, add bottom pane
//    - Bottom pane contains: Rolls (half) and Log (half), rows=0 on second pane means auto-split
//    - Maximum 4 rows for bottom pane
// 6. Main terminal expands to use all remaining space
//
// To add a new resolution:
// 1. Copy an existing profile block
// 2. Update width, height, name
// 3. Adjust main_view_scale, aux_view_font_size, and pane layout following the logic above
// 4. That's it! The function will automatically pick it up.
//
static const struct resolution_profile resolution_profiles[] = {
    // 800x600 (SVGA)
    { .width = 800, .height = 600, .name = "800x600 (SVGA)", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1024x768 (XGA)
    { .width = 1024, .height = 768, .name = "1024x768 (XGA)", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 40 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1152x864
    { .width = 1152, .height = 864, .name = "1152x864", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1280x720 (HD 720p)
    { .width = 1280, .height = 720, .name = "1280x720 (HD 720p)", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1280x768
    { .width = 1280, .height = 768, .name = "1280x768", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 0, .panes = {} },
    
    // 1280x800 (WXGA)
    { .width = 1280, .height = 800, .name = "1280x800 (WXGA)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 2, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1280x1024 (SXGA)
    { .width = 1280, .height = 1024, .name = "1280x1024 (SXGA)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1360x768
    { .width = 1360, .height = 768, .name = "1360x768", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 0, .panes = {} },
    
    // 1366x768 (HD)
    { .width = 1366, .height = 768, .name = "1366x768 (HD)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 0, .panes = {} },
    
    // 1400x1050
    { .width = 1400, .height = 1050, .name = "1400x1050", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1440x900
    { .width = 1440, .height = 900, .name = "1440x900", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1536x864
    { .width = 1536, .height = 864, .name = "1536x864", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1600x900 (HD+)
    { .width = 1600, .height = 900, .name = "1600x900 (HD+)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1600x1200 (UXGA)
    { .width = 1600, .height = 1200, .name = "1600x1200 (UXGA)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1680x1050
    { .width = 1680, .height = 1050, .name = "1680x1050", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 40 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1920x1080 (Full HD)
    { .width = 1920, .height = 1080, .name = "1920x1080 (Full HD)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1920x1200 (WUXGA)
    { .width = 1920, .height = 1200, .name = "1920x1200 (WUXGA)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 2, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2048x1152
    { .width = 2048, .height = 1152, .name = "2048x1152", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 0, .panes = {} },
    
    // 2256x1504 (Surface Laptop 13.5")
    { .width = 2256, .height = 1504, .name = "2256x1504 (Surface Laptop 13.5\")", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2160x1440
    { .width = 2160, .height = 1440, .name = "2160x1440", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2304x1440 (LG UltraFine scaled)
    { .width = 2304, .height = 1440, .name = "2304x1440 (LG UltraFine scaled)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2520x1680 (MacBook Air 13" M2/M3)
    { .width = 2520, .height = 1680, .name = "2520x1680 (MacBook Air 13\" M2/M3)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1080 (Ultrawide)
    { .width = 2560, .height = 1080, .name = "2560x1080 (Ultrawide)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1440 (QHD)
    { .width = 2560, .height = 1440, .name = "2560x1440 (QHD)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1600 (MacBook 13")
    { .width = 2560, .height = 1600, .name = "2560x1600 (MacBook 13\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1700 (Dell XPS 17")
    { .width = 2560, .height = 1700, .name = "2560x1700 (Dell XPS 17\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2736x1824 (Surface Book)
    { .width = 2736, .height = 1824, .name = "2736x1824 (Surface Book)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2880x1620
    { .width = 2880, .height = 1620, .name = "2880x1620", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2880x1800 (MacBook 15")
    { .width = 2880, .height = 1800, .name = "2880x1800 (MacBook 15\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2880x1920 (Surface Laptop 15")
    { .width = 2880, .height = 1920, .name = "2880x1920 (Surface Laptop 15\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3000x2000 (Surface Laptop)
    { .width = 3000, .height = 2000, .name = "3000x2000 (Surface Laptop)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3024x1964 (MacBook Pro 14" base)
    { .width = 3024, .height = 1964, .name = "3024x1964 (MacBook Pro 14\" base)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3072x1920 (MacBook Pro 16")
    { .width = 3072, .height = 1920, .name = "3072x1920 (MacBook Pro 16\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3200x1800
    { .width = 3200, .height = 1800, .name = "3200x1800", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3240x2160
    { .width = 3240, .height = 2160, .name = "3240x2160", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3440x1440 (Ultrawide QHD)
    { .width = 3440, .height = 1440, .name = "3440x1440 (Ultrawide QHD)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3456x2234 (MacBook Pro 14")
    { .width = 3456, .height = 2234, .name = "3456x2234 (MacBook Pro 14\")", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1080 (Super Ultrawide)
    { .width = 3840, .height = 1080, .name = "3840x1080 (Super Ultrawide)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1200
    { .width = 3840, .height = 1200, .name = "3840x1200", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1440
    { .width = 3840, .height = 1440, .name = "3840x1440", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1600
    { .width = 3840, .height = 1600, .name = "3840x1600", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x2160 (4K UHD)
    { .width = 3840, .height = 2160, .name = "3840x2160 (4K UHD)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x2400 (Dell UltraSharp)
    { .width = 3840, .height = 2400, .name = "3840x2400 (Dell UltraSharp)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 4096x2160 (DCI 4K)
    { .width = 4096, .height = 2160, .name = "4096x2160 (DCI 4K)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 4480x1440
    { .width = 4480, .height = 1440, .name = "4480x1440", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 5120x1440 (Super Ultrawide)
    { .width = 5120, .height = 1440, .name = "5120x1440 (Super Ultrawide)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 5120x2160 (5K Ultrawide)
    { .width = 5120, .height = 2160, .name = "5120x2160 (5K Ultrawide)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 5120x2880 (5K)
    { .width = 5120, .height = 2880, .name = "5120x2880 (5K)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 6016x3384 (6K)
    { .width = 6016, .height = 3384, .name = "6016x3384 (6K)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 7680x4320 (8K UHD)
    { .width = 7680, .height = 4320, .name = "7680x4320 (8K UHD)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } }
};

#define NUM_RESOLUTION_PROFILES (sizeof(resolution_profiles) / sizeof(resolution_profiles[0]))

static const char* pane_type_to_string(enum pane_type type)
{
    switch (type) {
        case PANE_MAIN: return "MAIN";
        case PANE_INVENTORY: return "INVENTORY";
        case PANE_WORN: return "WORN";
        case PANE_ROLLS: return "ROLLS";
        case PANE_INFO: return "INFO";
        case PANE_CHARACTER: return "CHARACTER";
        case PANE_LOG: return "LOG";
        case PANE_MONSTERS: return "MONSTERS";
        default: return "MAIN";
    }
}

static enum pane_type parse_pane_type(const char* value)
{
    if (!value)
        return PANE_MAIN;
    if (strcmp(value, "MAIN") == 0) return PANE_MAIN;
    if (strcmp(value, "INVENTORY") == 0) return PANE_INVENTORY;
    if (strcmp(value, "WORN") == 0) return PANE_WORN;
    if (strcmp(value, "ROLLS") == 0) return PANE_ROLLS;
    if (strcmp(value, "INFO") == 0) return PANE_INFO;
    if (strcmp(value, "CHARACTER") == 0) return PANE_CHARACTER;
    if (strcmp(value, "LOG") == 0) return PANE_LOG;
    if (strcmp(value, "MONSTERS") == 0) return PANE_MONSTERS;
    return PANE_MAIN;
}

static const char* pane_placement_to_string(enum pane_placement where)
{
    switch (where) {
        case PLACE_BOTTOM: return "BOTTOM";
        case PLACE_RIGHT: return "RIGHT";
        default: return "RIGHT";
    }
}

static enum pane_placement parse_pane_placement(const char* value)
{
    if (!value)
        return PLACE_RIGHT;
    if (strcmp(value, "BOTTOM") == 0) return PLACE_BOTTOM;
    if (strcmp(value, "RIGHT") == 0) return PLACE_RIGHT;
    return PLACE_RIGHT;
}

static char* read_file_contents(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f) {
        log_debug("Could not open JSON file: %s", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        log_error("Failed to allocate memory for JSON file");
        return NULL;
    }
    
    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    fclose(f);
    
    return content;
}

void sdl_config_load(const char* filename, struct sdl_config* config, 
                     struct pane_config* pane_configs, int* pane_count, int max_panes)
{
    log_info("Loading SDL configuration from: %s", filename);
    
    char* content = read_file_contents(filename);
    if (!content) {
        log_debug("Failed to read config file, using defaults");
        return;
    }
    
    log_debug("Config file content length: %zu bytes", strlen(content));
    
    cJSON* root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            log_error("JSON parse error before: %s", error_ptr);
        } else {
            log_error("JSON parse error (no error pointer available)");
        }
        return;
    }
    
    log_debug("JSON parsed successfully");
    
    // Parse SDL settings
    cJSON* sdl = cJSON_GetObjectItemCaseSensitive(root, "sdl");
    if (cJSON_IsObject(sdl)) {
        log_debug("Found 'sdl' object in JSON");
        cJSON* item;
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "mainViewScale");
        if (cJSON_IsNumber(item)) {
            config->main_view_scale = item->valueint;
            log_debug("Loaded mainViewScale: %d", config->main_view_scale);
        } else {
            log_warn("mainViewScale not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "auxViewFontSize");
        if (cJSON_IsNumber(item)) {
            config->aux_view_font_size = item->valueint;
            log_debug("Loaded auxViewFontSize: %d", config->aux_view_font_size);
        } else {
            log_warn("auxViewFontSize not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "margin");
        if (cJSON_IsNumber(item)) {
            config->margin = item->valueint;
            log_debug("Loaded margin: %d", config->margin);
        } else {
            log_warn("margin not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "fullscreen");
        if (cJSON_IsBool(item)) {
            config->fullscreen = cJSON_IsTrue(item);
            log_debug("Loaded fullscreen: %s", config->fullscreen ? "true" : "false");
        } else {
            log_warn("fullscreen not found or not a boolean");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "tiles");
        if (cJSON_IsBool(item)) {
            config->tiles = cJSON_IsTrue(item);
            log_debug("Loaded tiles: %s", config->tiles ? "true" : "false");
        } else {
            log_warn("tiles not found or not a boolean");
        }
        
        // Window position and size for windowed mode
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowX");
        if (cJSON_IsNumber(item)) {
            config->window_x = item->valueint;
            log_debug("Loaded windowX: %d", config->window_x);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowY");
        if (cJSON_IsNumber(item)) {
            config->window_y = item->valueint;
            log_debug("Loaded windowY: %d", config->window_y);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowWidth");
        if (cJSON_IsNumber(item)) {
            config->window_width = item->valueint;
            log_debug("Loaded windowWidth: %d", config->window_width);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowHeight");
        if (cJSON_IsNumber(item)) {
            config->window_height = item->valueint;
            log_debug("Loaded windowHeight: %d", config->window_height);
        }
        
        // Custom fonts
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyFont");
        if (cJSON_IsString(item)) {
            SDL_strlcpy(config->story_font, item->valuestring, sizeof(config->story_font));
            log_debug("Loaded storyFont: %s", config->story_font);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monospaceFont");
        if (cJSON_IsString(item)) {
            SDL_strlcpy(config->monospace_font, item->valuestring, sizeof(config->monospace_font));
            log_debug("Loaded monospaceFont: %s", config->monospace_font);
        }
        
        // Monospace font rendering options (with backward compatibility)
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoBold");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontBold");
        if (cJSON_IsBool(item)) {
            config->mono_bold = cJSON_IsTrue(item);
            log_debug("Loaded monoBold: %s", config->mono_bold ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoItalic");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontItalic");
        if (cJSON_IsBool(item)) {
            config->mono_italic = cJSON_IsTrue(item);
            log_debug("Loaded monoItalic: %s", config->mono_italic ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoUnderline");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontUnderline");
        if (cJSON_IsBool(item)) {
            config->mono_underline = cJSON_IsTrue(item);
            log_debug("Loaded monoUnderline: %s", config->mono_underline ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoStrikethrough");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontStrikethrough");
        if (cJSON_IsBool(item)) {
            config->mono_strikethrough = cJSON_IsTrue(item);
            log_debug("Loaded monoStrikethrough: %s", config->mono_strikethrough ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoHinting");
        if (!cJSON_IsNumber(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontHinting");
        if (cJSON_IsNumber(item)) {
            config->mono_hinting = item->valueint;
            log_debug("Loaded monoHinting: %d", config->mono_hinting);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoKerning");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontKerning");
        if (cJSON_IsBool(item)) {
            config->mono_kerning = cJSON_IsTrue(item);
            log_debug("Loaded monoKerning: %s", config->mono_kerning ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoOutline");
        if (!cJSON_IsNumber(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontOutline");
        if (cJSON_IsNumber(item)) {
            config->mono_outline = item->valueint;
            log_debug("Loaded monoOutline: %d", config->mono_outline);
        }
        
        // Story font rendering options
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyBold");
        if (cJSON_IsBool(item)) {
            config->story_bold = cJSON_IsTrue(item);
            log_debug("Loaded storyBold: %s", config->story_bold ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyItalic");
        if (cJSON_IsBool(item)) {
            config->story_italic = cJSON_IsTrue(item);
            log_debug("Loaded storyItalic: %s", config->story_italic ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyUnderline");
        if (cJSON_IsBool(item)) {
            config->story_underline = cJSON_IsTrue(item);
            log_debug("Loaded storyUnderline: %s", config->story_underline ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyStrikethrough");
        if (cJSON_IsBool(item)) {
            config->story_strikethrough = cJSON_IsTrue(item);
            log_debug("Loaded storyStrikethrough: %s", config->story_strikethrough ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyHinting");
        if (cJSON_IsNumber(item)) {
            config->story_hinting = item->valueint;
            log_debug("Loaded storyHinting: %d", config->story_hinting);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyKerning");
        if (cJSON_IsBool(item)) {
            config->story_kerning = cJSON_IsTrue(item);
            log_debug("Loaded storyKerning: %s", config->story_kerning ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyOutline");
        if (cJSON_IsNumber(item)) {
            config->story_outline = item->valueint;
            log_debug("Loaded storyOutline: %d", config->story_outline);
        }
    } else {
        log_warn("'sdl' object not found in JSON");
    }
    
    // Parse pane configurations
    cJSON* panes = cJSON_GetObjectItemCaseSensitive(root, "panes");
    if (cJSON_IsArray(panes)) {
        int count = 0;
        cJSON* pane_item = NULL;
        int array_size = cJSON_GetArraySize(panes);
        log_debug("Found 'panes' array with %d items", array_size);
        
        cJSON_ArrayForEach(pane_item, panes) {
            if (count >= max_panes) {
                log_warn("Too many panes in config, maximum is %d", max_panes);
                break;
            }
            
            struct pane_config* pc = &pane_configs[count];
            
            cJSON* type = cJSON_GetObjectItemCaseSensitive(pane_item, "type");
            if (cJSON_IsString(type)) {
                pc->pane = parse_pane_type(type->valuestring);
                log_debug("Pane %d: type=%s", count, type->valuestring);
            }
            
            cJSON* where = cJSON_GetObjectItemCaseSensitive(pane_item, "where");
            if (cJSON_IsString(where)) {
                pc->where = parse_pane_placement(where->valuestring);
                log_debug("Pane %d: where=%s", count, where->valuestring);
            }
            
            cJSON* rows = cJSON_GetObjectItemCaseSensitive(pane_item, "rows");
            if (cJSON_IsNumber(rows)) {
                pc->rect.rows = rows->valueint;
                log_debug("Pane %d: rows=%d", count, pc->rect.rows);
            }
            
            cJSON* cols = cJSON_GetObjectItemCaseSensitive(pane_item, "cols");
            if (cJSON_IsNumber(cols)) {
                pc->rect.cols = cols->valueint;
                log_debug("Pane %d: cols=%d", count, pc->rect.cols);
            }
            
            cJSON* ratio = cJSON_GetObjectItemCaseSensitive(pane_item, "ratio");
            if (cJSON_IsNumber(ratio)) {
                pc->ratio = (float)ratio->valuedouble;
                log_debug("Pane %d: ratio=%.2f", count, pc->ratio);
            }
            
            count++;
        }
        
        *pane_count = count;
        log_debug("Parsed %d panes from JSON", count);
    } else {
        log_warn("'panes' array not found in JSON");
    }

    // Parse gamepad settings
    cJSON* gamepad = cJSON_GetObjectItemCaseSensitive(root, "gamepad");
    if (cJSON_IsObject(gamepad)) {
        cJSON* item;

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "enabled");
        if (cJSON_IsBool(item)) {
            config->gamepad_enabled = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.enabled: %s", config->gamepad_enabled ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "autoMode");
        if (cJSON_IsBool(item)) {
            config->gamepad_auto_mode = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.autoMode: %s", config->gamepad_auto_mode ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "steamdeckMode");
        if (cJSON_IsBool(item)) {
            config->steamdeck_mode = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.steamdeckMode: %s", config->steamdeck_mode ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "useDpad");
        if (cJSON_IsBool(item)) {
            config->gamepad_use_dpad = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.useDpad: %s", config->gamepad_use_dpad ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "useLeftStick");
        if (cJSON_IsBool(item)) {
            config->gamepad_use_left_stick = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.useLeftStick: %s", config->gamepad_use_left_stick ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "deadzone");
        if (cJSON_IsNumber(item)) {
            config->gamepad_deadzone = item->valueint;
            log_debug("Loaded gamepad.deadzone: %d", config->gamepad_deadzone);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "triggerThreshold");
        if (cJSON_IsNumber(item)) {
            config->gamepad_trigger_threshold = item->valueint;
            log_debug("Loaded gamepad.triggerThreshold: %d", config->gamepad_trigger_threshold);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "buttonBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    config->gamepad_button_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.buttonBindings (%d entries)", count);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "triggerBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < GAMEPAD_TRIGGER_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    config->gamepad_trigger_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.triggerBindings (%d entries)", count);
        }
    } else {
        log_warn("'gamepad' object not found in JSON");
    }
    
    cJSON_Delete(root);
    log_debug("Configuration loading complete. Total panes: %d", *pane_count);
}

void sdl_config_save(const char* filename, const struct sdl_config* config,
                     const struct pane_config* pane_configs, int pane_count)
{
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        log_error("Failed to create JSON root object");
        return;
    }
    
    // Create SDL settings object
    cJSON* sdl = cJSON_CreateObject();
    if (!sdl) {
        cJSON_Delete(root);
        log_error("Failed to create SDL settings object");
        return;
    }
    
    cJSON_AddNumberToObject(sdl, "mainViewScale", config->main_view_scale);
    cJSON_AddNumberToObject(sdl, "auxViewFontSize", config->aux_view_font_size);
    cJSON_AddNumberToObject(sdl, "margin", config->margin);
    cJSON_AddBoolToObject(sdl, "fullscreen", config->fullscreen);
    cJSON_AddBoolToObject(sdl, "tiles", config->tiles);
    
    // Save window position and size for windowed mode
    cJSON_AddNumberToObject(sdl, "windowX", config->window_x);
    cJSON_AddNumberToObject(sdl, "windowY", config->window_y);
    cJSON_AddNumberToObject(sdl, "windowWidth", config->window_width);
    cJSON_AddNumberToObject(sdl, "windowHeight", config->window_height);
    
    // Save custom fonts
    cJSON_AddStringToObject(sdl, "storyFont", config->story_font);
    cJSON_AddStringToObject(sdl, "monospaceFont", config->monospace_font);
    
    // Save monospace font rendering options
    cJSON_AddBoolToObject(sdl, "monoBold", config->mono_bold);
    cJSON_AddBoolToObject(sdl, "monoItalic", config->mono_italic);
    cJSON_AddBoolToObject(sdl, "monoUnderline", config->mono_underline);
    cJSON_AddBoolToObject(sdl, "monoStrikethrough", config->mono_strikethrough);
    cJSON_AddNumberToObject(sdl, "monoHinting", config->mono_hinting);
    cJSON_AddBoolToObject(sdl, "monoKerning", config->mono_kerning);
    cJSON_AddNumberToObject(sdl, "monoOutline", config->mono_outline);
    
    // Save story font rendering options
    cJSON_AddBoolToObject(sdl, "storyBold", config->story_bold);
    cJSON_AddBoolToObject(sdl, "storyItalic", config->story_italic);
    cJSON_AddBoolToObject(sdl, "storyUnderline", config->story_underline);
    cJSON_AddBoolToObject(sdl, "storyStrikethrough", config->story_strikethrough);
    cJSON_AddNumberToObject(sdl, "storyHinting", config->story_hinting);
    cJSON_AddBoolToObject(sdl, "storyKerning", config->story_kerning);
    cJSON_AddNumberToObject(sdl, "storyOutline", config->story_outline);
    
    cJSON_AddItemToObject(root, "sdl", sdl);
    
    // Create panes array
    cJSON* panes = cJSON_CreateArray();
    if (!panes) {
        cJSON_Delete(root);
        log_error("Failed to create panes array");
        return;
    }
    
    for (int i = 0; i < pane_count; i++) {
        const struct pane_config* pc = &pane_configs[i];
        
        cJSON* pane = cJSON_CreateObject();
        if (!pane) {
            continue;
        }
        
        cJSON_AddStringToObject(pane, "type", pane_type_to_string(pc->pane));
        cJSON_AddStringToObject(pane, "where", pane_placement_to_string(pc->where));
        
        if (pc->rect.rows > 0) {
            cJSON_AddNumberToObject(pane, "rows", pc->rect.rows);
        }
        
        if (pc->rect.cols > 0) {
            cJSON_AddNumberToObject(pane, "cols", pc->rect.cols);
        }
        
        if (pc->ratio > 0.0f) {
            cJSON_AddNumberToObject(pane, "ratio", pc->ratio);
        }
        
        cJSON_AddItemToArray(panes, pane);
    }
    
    cJSON_AddItemToObject(root, "panes", panes);

    // Create gamepad settings object
    {
        cJSON* gamepad = cJSON_CreateObject();
        if (gamepad) {
            cJSON* bindings = NULL;
            cJSON* triggers = NULL;

            cJSON_AddBoolToObject(gamepad, "enabled", config->gamepad_enabled);
            cJSON_AddBoolToObject(gamepad, "autoMode", config->gamepad_auto_mode);
            cJSON_AddBoolToObject(gamepad, "steamdeckMode", config->steamdeck_mode);
            cJSON_AddBoolToObject(gamepad, "useDpad", config->gamepad_use_dpad);
            cJSON_AddBoolToObject(gamepad, "useLeftStick", config->gamepad_use_left_stick);
            cJSON_AddNumberToObject(gamepad, "deadzone", config->gamepad_deadzone);
            cJSON_AddNumberToObject(gamepad, "triggerThreshold", config->gamepad_trigger_threshold);

            bindings = cJSON_CreateArray();
            if (bindings) {
                for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
                    cJSON_AddItemToArray(bindings, cJSON_CreateNumber(config->gamepad_button_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "buttonBindings", bindings);
            }

            triggers = cJSON_CreateArray();
            if (triggers) {
                for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
                    cJSON_AddItemToArray(triggers, cJSON_CreateNumber(config->gamepad_trigger_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "triggerBindings", triggers);
            }

            cJSON_AddItemToObject(root, "gamepad", gamepad);
        }
    }
    
    // Print to string and write to file
    char* json_string = cJSON_Print(root);
    if (!json_string) {
        cJSON_Delete(root);
        log_error("Failed to print JSON");
        return;
    }
    
    FILE* f = fopen(filename, "w");
    if (!f) {
        log_error("Could not write JSON file: %s", filename);
        cJSON_free(json_string);
        cJSON_Delete(root);
        return;
    }
    
    fprintf(f, "%s\n", json_string);
    fclose(f);
    cJSON_free(json_string);
    cJSON_Delete(root);
    
    log_info("Saved SDL configuration to: %s", filename);
}

void sdl_config_set_default_gamepad_bindings(struct sdl_config* config)
{
    if (!config)
        return;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        config->gamepad_button_bindings[i] = GAMEPAD_BIND_NONE;
    }
    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        config->gamepad_trigger_bindings[i] = GAMEPAD_BIND_NONE;
    }

    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_SOUTH] = ' ';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST] = 'f';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_WEST] = 'u';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_NORTH] = 's';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = 'e';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'i';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_START] = '\r';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_BACK] = ESCAPE;
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_PADDLE1] = 'l';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_PADDLE2] = '\t';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1] = 'x';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2] = 'h';

    config->gamepad_trigger_bindings[0] = GAMEPAD_BIND_SHIFT;
    config->gamepad_trigger_bindings[1] = GAMEPAD_BIND_CTRL;
}

void sdl_config_set_defaults(struct sdl_config* config)
{
    config->main_view_scale = 1;
    config->aux_view_font_size = 18;
    config->margin = 4;
    config->fullscreen = true;
    config->tiles = true;
    config->enable_right_panes = true;
    config->enable_bottom_panes = true;
    
    // Default window position and size (will be overridden by actual screen size)
    config->window_x = -1;  // -1 means centered
    config->window_y = -1;  // -1 means centered
    config->window_width = 0;  // 0 means use default calculation
    config->window_height = 0; // 0 means use default calculation
    
    // Default fonts
    SDL_strlcpy(config->story_font, "lib/xtra/font/Cinzel-Medium.ttf", sizeof(config->story_font));
    SDL_strlcpy(config->monospace_font, "lib/xtra/font/VictorMono-Medium.ttf", sizeof(config->monospace_font));
    
    // Default monospace font rendering options
    config->mono_bold = false;
    config->mono_italic = false;
    config->mono_underline = false;
    config->mono_strikethrough = false;
    config->mono_hinting = 0;  // TTF_HINTING_NORMAL
    config->mono_kerning = true;
    config->mono_outline = 0;
    
    // Default story font rendering options
    config->story_bold = false;
    config->story_italic = false;
    config->story_underline = false;
    config->story_strikethrough = false;
    config->story_hinting = 0;  // TTF_HINTING_NORMAL
    config->story_kerning = true;
    config->story_outline = 0;

    // Default gamepad settings
    config->gamepad_enabled = true;
    config->gamepad_auto_mode = true;
    config->steamdeck_mode = false;
    config->gamepad_use_dpad = true;
    config->gamepad_use_left_stick = true;
    config->gamepad_deadzone = 12000;
    config->gamepad_trigger_threshold = 16000;
    sdl_config_set_default_gamepad_bindings(config);
}

void sdl_config_set_defaults_for_resolution(struct sdl_config* config, 
                                            struct pane_config* pane_configs,
                                            int* pane_count,
                                            int max_panes,
                                            int screen_width,
                                            int screen_height)
{
    // Start with base defaults
    sdl_config_set_defaults(config);
    *pane_count = 0;
    
    log_info("Setting resolution-specific defaults for %dx%d", screen_width, screen_height);
    
    // Search for matching resolution profile
    const struct resolution_profile* profile = NULL;
    for (size_t i = 0; i < NUM_RESOLUTION_PROFILES; i++) {
        if (resolution_profiles[i].width == screen_width && 
            resolution_profiles[i].height == screen_height) {
            profile = &resolution_profiles[i];
            break;
        }
    }
    
    if (profile) {
        // Apply resolution-specific settings
        log_info("Detected %s resolution - applying optimized defaults", profile->name);
        
        config->main_view_scale = profile->main_view_scale;
        config->aux_view_font_size = profile->aux_view_font_size;
        // Note: margin, fullscreen, tiles, and window position/size use base defaults
        
        // Apply pane configuration
        *pane_count = profile->pane_count;
        if (*pane_count > max_panes) {
            log_warn("Profile has %d panes but max_panes is %d, truncating", 
                     *pane_count, max_panes);
            *pane_count = max_panes;
        }
        
        for (int i = 0; i < *pane_count; i++) {
            pane_configs[i].pane = profile->panes[i].type;
            pane_configs[i].where = profile->panes[i].where;
            pane_configs[i].rect.rows = profile->panes[i].rows;
            pane_configs[i].rect.cols = profile->panes[i].cols;
            pane_configs[i].ratio = 0.0f;
        }
    } else {
        // Unknown resolution - use generic defaults
        log_info("Using generic defaults for %dx%d resolution", screen_width, screen_height);
        // The config already has base defaults from sdl_config_set_defaults()
        // pane_count is 0, so default_pane_config will be used by caller
    }
}

void sdl_config_apply_cmdline(struct sdl_config* config, int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--scale") == 0) {
            if (argc > i + 1) {
                const char* scale_str = argv[++i];
                int scale = atoi(scale_str);
                if (scale > 0) {
                    config->main_view_scale = scale;
                    log_info("Command line: main view scale set to %d", scale);
                }
            }
        } else if (strcmp(argv[i], "--ascii") == 0) {
            config->tiles = false;
            log_info("Command line: ASCII mode enabled");
        } else if (strcmp(argv[i], "--windowed") == 0) {
            config->fullscreen = false;
            log_info("Command line: windowed mode enabled");
        } else if (strcmp(argv[i], "--fullscreen") == 0) {
            config->fullscreen = true;
            log_info("Command line: fullscreen mode enabled");
        } else if (strcmp(argv[i], "--tiles") == 0) {
            config->tiles = true;
            log_info("Command line: tiles mode enabled");
        } else if (strcmp(argv[i], "--font-size") == 0) {
            if (argc > i + 1) {
                const char* size_str = argv[++i];
                int size = atoi(size_str);
                if (size > 0) {
                    config->aux_view_font_size = size;
                    log_info("Command line: auxiliary view font size set to %d", size);
                }
            }
        } else if (strcmp(argv[i], "--margin") == 0) {
            if (argc > i + 1) {
                const char* margin_str = argv[++i];
                int margin = atoi(margin_str);
                if (margin >= 0) {
                    config->margin = margin;
                    log_info("Command line: margin set to %d", margin);
                }
            }
        }
    }
}


