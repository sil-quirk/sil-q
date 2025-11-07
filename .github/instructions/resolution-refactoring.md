# Resolution Profile System - Before vs After Refactoring

## Code Comparison

### BEFORE: Hard-coded if/else chains (~170 lines of repetitive code)

```c
void sdl_config_set_defaults_for_resolution(...)
{
    sdl_config_set_defaults(config);
    log_info("Setting resolution-specific defaults for %dx%d", screen_width, screen_height);
    
    if (screen_width == 2880 && screen_height == 1800) {
        log_info("Detected 2880x1800 resolution - applying optimized defaults");
        config->main_view_scale = 3;
        config->aux_view_font_size = 18;
        config->margin = 4;
        config->fullscreen = false;
        config->tiles = true;
        config->window_x = 100;
        config->window_y = 100;
        config->window_width = 2779;
        config->window_height = 1466;
        
        *pane_count = 5;
        if (max_panes < 5) {
            log_warn("max_panes (%d) too small for default config, truncating", max_panes);
            *pane_count = max_panes;
        }
        
        if (*pane_count > 0) {
            pane_configs[0].pane = PANE_INVENTORY;
            pane_configs[0].where = PLACE_RIGHT;
            pane_configs[0].rect.rows = 22;
            pane_configs[0].rect.cols = 50;
            pane_configs[0].ratio = 0.0f;
        }
        // ... 4 more panes, each 7 lines of code ...
    }
    else if (screen_width == 2560 && screen_height == 1600) {
        // ... entire block repeated for 2560x1600 (another ~60 lines) ...
    }
    else {
        log_info("Using generic defaults for %dx%d resolution", screen_width, screen_height);
    }
}
```

**Problems:**
- ❌ Adding new resolution = copy/paste ~60 lines
- ❌ Risk of typos when modifying pane configs
- ❌ Hard to see what's different between resolutions
- ❌ Function grows linearly with each resolution
- ❌ No compile-time validation of structure

---

### AFTER: Data-driven with static profiles (~30 lines total, plus ~20 lines per profile)

```c
// Data structure (defined once)
struct resolution_profile {
    int width, height;
    const char* name;
    int main_view_scale, aux_view_font_size, margin;
    bool fullscreen, tiles;
    int window_x, window_y, window_width, window_height;
    int pane_count;
    struct {
        enum pane_type type;
        enum pane_placement where;
        int rows, cols;
    } panes[8];
};

// Data array - add new resolutions here!
static const struct resolution_profile resolution_profiles[] = {
    {
        .width = 2880, .height = 1800,
        .name = "2880x1800 (Retina 15\")",
        .main_view_scale = 3,
        .aux_view_font_size = 18,
        .margin = 4,
        .fullscreen = false,
        .tiles = true,
        .window_x = 100, .window_y = 100,
        .window_width = 2779, .window_height = 1466,
        .pane_count = 5,
        .panes = {
            { PANE_INVENTORY, PLACE_RIGHT,  22, 50 },
            { PANE_WORN,      PLACE_RIGHT,  17, 0  },
            { PANE_INFO,      PLACE_RIGHT,  8,  0  },
            { PANE_ROLLS,     PLACE_BOTTOM, 4,  0  },
            { PANE_LOG,       PLACE_BOTTOM, 0,  0  }
        }
    },
    {
        .width = 2560, .height = 1600,
        .name = "2560x1600 (Retina 13\")",
        .main_view_scale = 3,
        .aux_view_font_size = 18,
        .margin = 4,
        .fullscreen = false,
        .tiles = true,
        .window_x = 100, .window_y = 100,
        .window_width = 2459, .window_height = 1266,
        .pane_count = 5,
        .panes = {
            { PANE_INVENTORY, PLACE_RIGHT,  22, 50 },
            { PANE_WORN,      PLACE_RIGHT,  17, 0  },
            { PANE_INFO,      PLACE_RIGHT,  8,  0  },
            { PANE_ROLLS,     PLACE_BOTTOM, 4,  0  },
            { PANE_LOG,       PLACE_BOTTOM, 0,  0  }
        }
    }
    // Add more resolutions here - no function changes needed!
};

// Simple lookup function (stays the same forever)
void sdl_config_set_defaults_for_resolution(...)
{
    sdl_config_set_defaults(config);
    *pane_count = 0;
    
    // Find matching profile
    const struct resolution_profile* profile = NULL;
    for (size_t i = 0; i < NUM_RESOLUTION_PROFILES; i++) {
        if (resolution_profiles[i].width == screen_width && 
            resolution_profiles[i].height == screen_height) {
            profile = &resolution_profiles[i];
            break;
        }
    }
    
    if (profile) {
        // Apply all settings from profile
        log_info("Detected %s - applying optimized defaults", profile->name);
        config->main_view_scale = profile->main_view_scale;
        config->aux_view_font_size = profile->aux_view_font_size;
        // ... (9 more simple assignments)
        
        // Apply pane configs
        *pane_count = profile->pane_count;
        for (int i = 0; i < *pane_count; i++) {
            pane_configs[i].pane = profile->panes[i].type;
            pane_configs[i].where = profile->panes[i].where;
            pane_configs[i].rect.rows = profile->panes[i].rows;
            pane_configs[i].rect.cols = profile->panes[i].cols;
            pane_configs[i].ratio = 0.0f;
        }
    } else {
        log_info("Using generic defaults for %dx%d", screen_width, screen_height);
    }
}
```

**Benefits:**
- ✅ Adding new resolution = add one array entry (~20 lines)
- ✅ Compiler validates structure at compile-time
- ✅ Easy to compare different resolutions side-by-side
- ✅ Function size stays constant
- ✅ Self-documenting with named fields
- ✅ Can iterate over profiles programmatically if needed

---

## Adding a New Resolution

### Before
1. Find the function in sdl-config.c
2. Add new `else if` block
3. Copy/paste 60+ lines
4. Change resolution check
5. Manually update each setting
6. Manually update each pane config (7 lines × 5 panes = 35 lines)
7. Test and debug typos

**Total: ~70 lines added to function**

### After
1. Open sdl-config.c
2. Copy existing profile block
3. Update values in array entry
4. Done!

**Total: ~20 lines added to data array**

---

## Example: Adding 1920x1080

### Before (would add ~70 lines to function)
```c
else if (screen_width == 1920 && screen_height == 1080) {
    log_info("Detected 1920x1080 resolution - applying optimized defaults");
    config->main_view_scale = 2;
    config->aux_view_font_size = 16;
    config->margin = 4;
    config->fullscreen = false;
    config->tiles = true;
    config->window_x = 100;
    config->window_y = 100;
    config->window_width = 1820;
    config->window_height = 980;
    
    *pane_count = 5;
    if (max_panes < 5) {
        log_warn("max_panes (%d) too small for default config, truncating", max_panes);
        *pane_count = max_panes;
    }
    
    if (*pane_count > 0) {
        pane_configs[0].pane = PANE_INVENTORY;
        // ... 6 more lines ...
    }
    if (*pane_count > 1) {
        // ... 7 more lines ...
    }
    // ... 3 more panes ...
}
```

### After (just add to array)
```c
{
    .width = 1920, .height = 1080,
    .name = "1920x1080 (Full HD)",
    .main_view_scale = 2,
    .aux_view_font_size = 16,
    .margin = 4,
    .fullscreen = false,
    .tiles = true,
    .window_x = 100, .window_y = 100,
    .window_width = 1820, .window_height = 980,
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

**70% less code. No function changes. Compiler-validated. Self-documenting.**

---

## Summary

| Aspect | Before | After |
|--------|--------|-------|
| Lines per resolution | ~70 | ~20 |
| Function growth | Linear | Constant |
| Code location | Mixed in logic | Separate data |
| Type safety | Manual | Compiler-enforced |
| Readability | Scattered | Tabular |
| Error-prone | Yes (copy/paste) | No (structured) |
| Maintenance | Touch function | Touch data only |

The refactoring reduces code by 70% while improving maintainability, safety, and clarity.
