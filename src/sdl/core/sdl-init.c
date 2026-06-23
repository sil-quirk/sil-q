#include "angband.h"
#include "sdl/main-sdl-private.h"

void sdl_quit_hook(cptr str)
{
    (void)str; // Unused parameter

#if SIL_SDL_MOBILE_BUILD
    sdl_mobile_lifecycle_unregister();
#endif
    
    // Shut down audio before tearing down SDL
    sdl_sound_shutdown();

    // Close any open gamepads
    sdl_gamepad_shutdown();

    // Release cached mouse route search buffers.
    sdl_mouse_path_search_free();
    
    // Clean up story fonts
    sdl_story_font_cache_clear();
    sdl_left_panel_canvas_destroy();
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
    g_term_get_size_hook = sdl_term_get_size_hook;

#if defined(__ANDROID__) && defined(SDL_HINT_ANDROID_TRAP_BACK_BUTTON)
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
#endif
    
    // Initialize SDL first to get display information
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        quit("could not init SDL");
    }
#if SIL_SDL_MOBILE_BUILD
    sdl_mobile_lifecycle_register();
#endif
    if (!TTF_Init()) {
        log_error("TTF_Init failed: %s", SDL_GetError());
        quit("could not init TTF");
    }
    sdl_refresh_direct_touch_present();
    log_info("Direct touch input: %s",
        g_direct_touch_present ? "present" : "not detected");
    sdl_log_mouse_devices();
    
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
    int profile_pixels_w = screen_pixels_w;
    int profile_pixels_h = screen_pixels_h;

#if defined(__ANDROID__) || defined(SIL_IOS)
    if (profile_pixels_w < profile_pixels_h) {
        int tmp = profile_pixels_w;

        profile_pixels_w = profile_pixels_h;
        profile_pixels_h = tmp;
    }
#endif
    
    log_info("primary display desktop mode: %dx%d @%.2fHz, pixel_density=%.2f",
             desktop_mode->w, desktop_mode->h, desktop_mode->refresh_rate, pixel_density);
    log_info("primary display physical resolution for defaults: %dx%d",
             screen_pixels_w, screen_pixels_h);
    sdl_store_platform_max_main_view_scales(profile_pixels_w,
        profile_pixels_h);
    
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

    startup_issue_summary[0] = '\0';

    if (config_exists) {
        // Config file exists - use generic defaults first, then load from file
        log_debug("Config file exists, loading from: %s", config_file_path);
        sdl_config_set_defaults(&config);

        pane_config_count = 0;
        memset(pane_config, 0, sizeof(pane_config));
        (void)sdl_normalize_unified_log_pane_config(pane_config,
            &pane_config_count, true);
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
        // Config file doesn't exist - use dynamic first-run defaults.
        log_debug("Config file not found, using dynamic defaults");
        sdl_reset_config_to_resolution_defaults(profile_pixels_w,
            profile_pixels_h);
        
        log_debug("After resolution defaults: scale=%d, default_aux_font=%d, margin=%d, fullscreen=%d, tiles=%d",
                  config.main_view_scale, config.aux_view_font_size, config.margin,
                  config.fullscreen, config.tiles);
    }

    sdl_normalize_unified_log_pane_profiles(true);
    sdl_ensure_default_pane_profiles_present(false);
    sdl_normalize_unified_log_pane_profiles(true);
    if (!config_exists)
        sdl_apply_screen_aspect_pane_default_profiles(profile_pixels_w,
            profile_pixels_h);
    sdl_apply_stored_pane_profile(config.min_terminal_mode);

#if defined(__ANDROID__) || defined(SIL_IOS)
    sdl_ensure_default_pane_configs_present(false);
    (void)sdl_normalize_unified_log_pane_config(pane_config,
        &pane_config_count, true);
    sdl_ensure_touch_pane_config_present();
#endif

    sdl_ensure_touch_pane_config_present();
    sdl_touch_pane_ensure_main_panel_confirm();

    /* Seed hidden-screen fallback layout from the configured pane groups
     * before the first resize/present, so startup screens do not briefly
     * show touch/right panes when that axis is disabled. */
    g_active_side_panes = config.enable_right_panes;
    g_active_bottom_panes = config.enable_bottom_panes;

    g_hide_left_panel = false;
    g_left_panel_pane_expanded = config.left_panel_expanded_on_launch;
    
    // Apply command-line overrides
    sdl_config_apply_cmdline(&config, argc, argv);
    log_debug("After command-line: scale=%d, default_aux_font=%d, margin=%d, fullscreen=%d, tiles=%d",
              config.main_view_scale, config.aux_view_font_size, config.margin,
              config.fullscreen, config.tiles);
    sdl_log_pane_sync_display_filter_from_config();

#if defined(__ANDROID__) || defined(SIL_IOS)
    if (config_exists) {
        int mobile_min_cols = sdl_current_min_terminal_cols();
        int mobile_min_rows = sdl_current_min_terminal_rows();
        int mobile_max_scale_w = (profile_pixels_w / mobile_min_cols) * 2 / TILE_SIZE;
        int mobile_max_scale_h = profile_pixels_h / mobile_min_rows / TILE_SIZE;
        int mobile_max_scale = mobile_max_scale_w;

        if (mobile_max_scale_h < mobile_max_scale)
            mobile_max_scale = mobile_max_scale_h;
        if (mobile_max_scale < SDL_MAIN_VIEW_MIN_SCALE)
            mobile_max_scale = SDL_MAIN_VIEW_MIN_SCALE;

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
    {
        int min_main_view_scale = sdl_main_view_scale_floor();
        int max_main_view_scale = get_sdl_platform_max_main_view_scale();

        if (max_main_view_scale < SDL_MAIN_VIEW_MIN_SCALE)
            max_main_view_scale = SDL_MAIN_VIEW_MIN_SCALE;
        if (min_main_view_scale > max_main_view_scale)
            min_main_view_scale = max_main_view_scale;

        if (config.main_view_scale < min_main_view_scale) {
            log_warn("Invalid main_view_scale %d, using %d",
                config.main_view_scale, min_main_view_scale);
            if (config_exists) {
                sdl_append_issue_line(startup_issue_summary,
                    sizeof(startup_issue_summary),
                    "The saved main view scale was below the supported minimum and was raised.");
            }
            config.main_view_scale = min_main_view_scale;
        } else if (config.main_view_scale > max_main_view_scale) {
            log_warn("Invalid main_view_scale %d, using %d",
                config.main_view_scale, max_main_view_scale);
            if (config_exists) {
                sdl_append_issue_line(startup_issue_summary,
                    sizeof(startup_issue_summary),
                    "The saved main view scale was too large for this display and was reduced.");
            }
            config.main_view_scale = max_main_view_scale;
        }
    }
    config.left_panel_compact_mode = sdl_left_panel_compact_mode_normalized(
        config.left_panel_compact_mode);
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

    g_startup_device_class = sdl_detect_startup_device_class(screen_pixels_w,
        screen_pixels_h);
#if SIL_SDL_DESKTOP_HANDHELD_BUILD
    if (!config_exists && g_gamepad_state.pad_count > 0
        && g_startup_device_class == SDL_STARTUP_DEVICE_DESKTOP)
    {
        g_startup_device_class =
            sdl_prompt_desktop_startup_input_device(screen_pixels_w,
                screen_pixels_h);
    }
#endif
    log_info("Startup device profile: %s (%dx%d, %d gamepad%s detected%s)",
        sdl_startup_device_class_name(g_startup_device_class),
        screen_pixels_w, screen_pixels_h,
        g_gamepad_state.pad_count,
        (g_gamepad_state.pad_count == 1) ? "" : "s",
        config_exists ? ", loaded config" : ", first start");

    if (!config_exists) {
        sdl_apply_first_start_device_defaults(g_startup_device_class);
    }
    if (sdl_touch_only_mobile_device_active())
        sdl_touch_apply_profile(SDL_TOUCH_PROFILE_ROUND_WHEEL);

    g_touch_pane_mobile_open = config.touch_pane_default_open;
    g_touch_top_panel_open = config.touch_top_panel_default_open;
    
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
    log_info("  Palette preset: %s",
        config.palette_preset[0] ? config.palette_preset : "classic");
    log_info("  Pane configurations: %d", pane_config_count);

    ui_colors_load_palette_presets();
    if (!ui_colors_apply_palette_preset(config.palette_preset))
        ui_colors_apply_palette_preset("classic");
    SDL_strlcpy(config.palette_preset, ui_colors_current_palette_preset(),
        sizeof(config.palette_preset));

    // Initialize palette from the selected preset.
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
    sdl_refresh_platform_max_main_view_scales_for_current_layout(
        "startup window layout");
    if (!config_exists) {
        (void)sdl_apply_default_main_scale_for_layout("startup");
    } else {
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

    // Load story and banner fonts
    sdl_load_story_fonts();

    ANGBAND_SYS = "sdl";
    (void)sdl_set_tiles_runtime(config.tiles);

    sdl_refresh_safe_area();
    {
        SDL_Rect screen = sdl_get_layout_screen_rect();

        log_debug("window layout size %dx%d at (%d,%d)",
            screen.w, screen.h, screen.x, screen.y);
        resize(&screen);
    }
    sdl_queue_main_view_scale_neighbors_prewarm("startup");

    if (config_exists && startup_issue_summary[0]) {
        bool old_fullscreen = config.fullscreen;

        if (sdl_prompt_reset_sdl_defaults(startup_issue_summary,
                profile_pixels_w, profile_pixels_h))
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

