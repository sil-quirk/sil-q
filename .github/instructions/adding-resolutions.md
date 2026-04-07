# Adding New Resolution Profiles

## Quick Guide

To add support for a new screen resolution, you only need to edit **one file** and add **one array entry**.

### File to Edit
`src/sdl-config.c`

### Where to Add
Find the `resolution_profiles[]` array near the top of the file (around line 40). Add a new entry following this template:

```c
{
    .width = 1920,                      // Screen width in pixels
    .height = 1080,                     // Screen height in pixels
    .name = "1920x1080 (Full HD)",     // Descriptive name for logs
    
    // Resolution-specific settings only
    .main_view_scale = 2,               // Tile/font scale (1-3 typical)
    .aux_view_font_size = 16,           // Font size for aux panes
    
    // Pane layout (up to 8 panes supported)
    .pane_count = 5,                    // Number of panes
    .panes = {
        { PANE_INVENTORY, PLACE_RIGHT,  18, 40 },  // Type, placement, rows, cols
        { PANE_WORN,      PLACE_RIGHT,  14, 0  },  // 0 = auto-calculate
        { PANE_INFO,      PLACE_RIGHT,  6,  0  },
        { PANE_ROLLS,     PLACE_BOTTOM, 3,  0  },
        { PANE_LOG,       PLACE_BOTTOM, 0,  0  }
    }
}
```

**Note:** Common settings like `margin=4`, `fullscreen=true`, and `tiles=true` are set automatically by `sdl_config_set_defaults()` and don't need to be specified here.

### Available Pane Types
- `PANE_MAIN` - Main game view (don't include in panes array)
- `PANE_INVENTORY` - Inventory list
- `PANE_WORN` - Equipment/worn items
- `PANE_INFO` - Character info
- `PANE_ROLLS` - Combat rolls display
- `PANE_LOG` - Message log
- `PANE_CHARACTER` - Character sheet
- `PANE_MONSTERS` - Monster list

### Pane Placement
- `PLACE_RIGHT` - Right sidebar
- `PLACE_BOTTOM` - Bottom panel

### Tips
1. **Start with an existing profile** that's close to your target resolution
2. **Scale factor**: Higher resolutions usually benefit from higher scale (2-3)
3. **Only specify resolution-specific values** - common defaults (margin, fullscreen, tiles) are automatic
4. **Pane rows**: Adjust based on vertical space available
5. **Pane cols = 0**: Auto-calculate width based on content

### Example: Adding Common Resolutions

**1920x1080 (Full HD)**
```c
{
    .width = 1920, .height = 1080,
    .name = "1920x1080 (Full HD)",
    .main_view_scale = 2,
    .aux_view_font_size = 16,
    .pane_count = 5,
    .panes = {
        { PANE_INVENTORY, PLACE_RIGHT,  18, 40 },
        { PANE_WORN,      PLACE_RIGHT,  14, 0  },
        { PANE_INFO,      PLACE_RIGHT,  6,  0  },
        { PANE_ROLLS,     PLACE_BOTTOM, 3,  0  },
        { PANE_LOG,       PLACE_BOTTOM, 0,  0  }
    }
}
```

**3840x2160 (4K)**
```c
{
    .width = 3840, .height = 2160,
    .name = "3840x2160 (4K)",
    .main_view_scale = 3,
    .aux_view_font_size = 20,
    .pane_count = 5,
    .panes = {
        { PANE_INVENTORY, PLACE_RIGHT,  28, 60 },
        { PANE_WORN,      PLACE_RIGHT,  20, 0  },
        { PANE_INFO,      PLACE_RIGHT,  10, 0  },
        { PANE_ROLLS,     PLACE_BOTTOM, 5,  0  },
        { PANE_LOG,       PLACE_BOTTOM, 0,  0  }
    }
}
```

**1366x768 (Common Laptop)**
```c
{
    .width = 1366, .height = 768,
    .name = "1366x768 (Laptop)",
    .main_view_scale = 1,
    .aux_view_font_size = 14,
    .pane_count = 5,
    .panes = {
        { PANE_INVENTORY, PLACE_RIGHT,  14, 35 },
        { PANE_WORN,      PLACE_RIGHT,  11, 0  },
        { PANE_INFO,      PLACE_RIGHT,  5,  0  },
        { PANE_ROLLS,     PLACE_BOTTOM, 2,  0  },
        { PANE_LOG,       PLACE_BOTTOM, 0,  0  }
    }
}
```

### Testing Your Profile
1. Build the game: `build-cmake.bat`
2. Delete `sil_sdl.json` from your Sil-More user folder (for example `%USERPROFILE%\sil-more\sil_sdl.json` on Windows or `~/sil-more/sil_sdl.json` on macOS/Linux)
3. Run the game on the target resolution
4. Check `log.txt` for: `Detected [your resolution name] - applying optimized defaults`
5. Verify the layout looks good
6. Fine-tune the values and rebuild if needed

### That's It!
No function changes, no complex logic - just data. The lookup function automatically finds and applies your profile based on screen resolution.
