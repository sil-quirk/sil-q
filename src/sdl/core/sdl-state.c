#include "angband.h"
#include "sdl/main-sdl-private.h"

bool sdl_key_is_escape_or_back(SDL_Keycode key)
{
    if (key == SDLK_ESCAPE)
        return true;
#ifdef SDLK_AC_BACK
    if (key == SDLK_AC_BACK)
        return true;
#endif

    return false;
}

const char help_sdl[] = "SDL3";

const char* const sdl_story_fallback_font = "lib/xtra/font/MarcellusSC-Regular.ttf";
const char* const sdl_story_fallback_font2 = "lib/xtra/font/MarcellusSC-Regular.ttf";

// SDL configuration (loaded from INI file)
struct sdl_config config;
bool g_hide_left_panel = false;
bool g_sdl_left_panel_pane_source_active = false;
bool g_left_panel_pane_expanded = false;
int g_saved_screen_left_panel_pane_depth = 0;
bool g_last_main_cell_hit_left_panel = false;

// Sound configuration (loaded from sound.json)
struct sound_config g_sound_config;

// Configuration file path (needed for saving on exit)
char config_file_path[1024];

// Default pane configuration
const struct pane_config default_pane_config[] = {
    {.pane = PANE_LEFT_PANEL, .where = PLACE_TOP_LEFT, .enabled = true,
        .rect.cols = 0},
    {.pane = PANE_COMBAT, .where = PLACE_BOTTOM_LEFT, .enabled = true,
        .rect.rows = PANE_COMBAT_OVERLAY_ROWS,
        .rect.cols = PANE_COMBAT_OVERLAY_COLS},
#if SIL_SDL_MOBILE_BUILD
    {.pane = PANE_DEPTH, .where = PLACE_BOTTOM_RIGHT, .enabled = false,
#else
    {.pane = PANE_DEPTH, .where = PLACE_BOTTOM_RIGHT, .enabled = true,
#endif
        .rect.rows = 4, .rect.cols = 12},
    {.pane = PANE_ROLLS, .where = PLACE_TOP_RIGHT, .enabled = true,
        .rect.rows = SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS},
#if SIL_SDL_MOBILE_BUILD
    {.pane = PANE_STATUS, .where = PLACE_TOP_CENTER, .enabled = false,
#else
    {.pane = PANE_STATUS, .where = PLACE_TOP_CENTER, .enabled = true,
#endif
        .rect.rows = 1, .rect.cols = 24},
#if SIL_SDL_MOBILE_BUILD
    {.pane = PANE_STATUS_DEPTH, .where = PLACE_BOTTOM_RIGHT, .enabled = true,
#else
    {.pane = PANE_STATUS_DEPTH, .where = PLACE_BOTTOM_RIGHT, .enabled = false,
#endif
        .rect.rows = 1, .rect.cols = 24},
    {.pane = PANE_DESCRIPTION, .where = PLACE_BOTTOM_CENTER, .enabled = true,
        .rect.rows = 80, .rect.cols = 160},
    {.pane = PANE_OVERLAY_MENU, .where = PLACE_BOTTOM_CENTER, .enabled = true,
        .rect.rows = 1, .rect.cols = 4},
    // On the right
    {.pane = PANE_INVENTORY, .where = PLACE_RIGHT, .enabled = true},
    {.pane = PANE_SUPPLY, .where = PLACE_RIGHT, .enabled = true,
        .rect.rows = 6},
    {.pane = PANE_WORN, .where = PLACE_RIGHT, .enabled = true},
    {.pane = PANE_INFO, .where = PLACE_RIGHT, .enabled = true, .rect.rows = 8},
    {.pane = PANE_CHARACTER, .where = PLACE_RIGHT, .enabled = false},
    {.pane = PANE_MAP, .where = PLACE_RIGHT, .enabled = false, .rect.rows = 12},
    {.pane = PANE_TOUCH, .where = PLACE_DOUBLE_RIGHT, .enabled = false},
    {.pane = PANE_LOG, .where = PLACE_BOTTOM, .enabled = true,
        .rect.rows = SDL_LOG_PANE_DEFAULT_ROWS},
};
const int default_pane_config_count = sizeof(default_pane_config) / sizeof(struct pane_config);

// Active pane configuration (may be loaded from INI)
struct pane_config pane_config[MAX_PANE_CONFIGS];
int pane_config_count = 0;
struct sdl_pane_profile g_pane_profiles[SDL_PANE_PROFILE_COUNT];
int g_platform_max_main_view_scale[SDL_MIN_TERMINAL_MODE_COUNT] = {
    SDL_MAIN_VIEW_PREFERRED_MIN_SCALE, SDL_MAIN_VIEW_PREFERRED_MIN_SCALE
};
sdl_startup_device_class g_startup_device_class =
    SDL_STARTUP_DEVICE_DESKTOP;
bool g_touch_tutorial_requested_from_settings = false;
bool g_mouse_tutorial_requested_from_settings = false;
bool g_character_wheel_tutorial_requested_from_settings = false;
bool g_zones_tutorial_requested_from_settings = false;

void sdl_copy_pane_configs(struct pane_config* dest, int* dest_count,
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

bool sdl_min_terminal_mode_is_valid(int mode)
{
    return (mode == SDL_MIN_TERMINAL_NORMAL || mode == SDL_MIN_TERMINAL_COMPACT);
}

int sdl_main_view_scale_floor_for_mode(int mode)
{
    int platform_max;

    if (!sdl_min_terminal_mode_is_valid(mode))
        mode = SDL_MIN_TERMINAL_NORMAL;
    if (mode >= SDL_MIN_TERMINAL_MODE_COUNT)
        mode = SDL_MIN_TERMINAL_NORMAL;

    platform_max = g_platform_max_main_view_scale[mode];
    if (platform_max <= SDL_MAIN_VIEW_MIN_SCALE)
        return SDL_MAIN_VIEW_MIN_SCALE;

    return SDL_MAIN_VIEW_PREFERRED_MIN_SCALE;
}

int sdl_platform_max_main_view_scale_for_mode(int mode)
{
    int scale;

    if (!sdl_min_terminal_mode_is_valid(mode))
        mode = SDL_MIN_TERMINAL_NORMAL;
    if (mode >= SDL_MIN_TERMINAL_MODE_COUNT)
        mode = SDL_MIN_TERMINAL_NORMAL;

    scale = g_platform_max_main_view_scale[mode];
    if (scale < SDL_MAIN_VIEW_MIN_SCALE)
        scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (scale > SDL_MAIN_VIEW_MAX_SCALE)
        scale = SDL_MAIN_VIEW_MAX_SCALE;

    return scale;
}

int sdl_main_view_scale_floor(void)
{
    return sdl_main_view_scale_floor_for_mode(config.min_terminal_mode);
}

int sdl_clamp_main_view_scale_floor(int scale, int mode)
{
    int floor = sdl_main_view_scale_floor_for_mode(mode);

    return (scale < floor) ? floor : scale;
}

int sdl_clamp_main_view_scale_platform_bounds(int scale, int mode)
{
    int floor = sdl_main_view_scale_floor_for_mode(mode);
    int max_scale = sdl_platform_max_main_view_scale_for_mode(mode);

    if (floor > max_scale)
        floor = max_scale;
    if (scale < floor)
        scale = floor;
    if (scale > max_scale)
        scale = max_scale;

    return scale;
}

static int sdl_pane_profile_index_for_mode(int mode)
{
    if (!sdl_min_terminal_mode_is_valid(mode))
        return -1;

#if SIL_SDL_MOBILE_BUILD
    return SDL_PANE_PROFILE_INDEX(config.mobile_portrait_mode
            ? SDL_PANE_ORIENTATION_PORTRAIT
            : SDL_PANE_ORIENTATION_LANDSCAPE,
        mode);
#else
    return SDL_PANE_PROFILE_INDEX(SDL_PANE_ORIENTATION_LANDSCAPE, mode);
#endif
}

void sdl_log_pane_sync_display_filter_from_config(void);

static void sdl_capture_pane_profile(struct sdl_pane_profile* profile)
{
    if (!profile)
        return;

    profile->main_view_scale = config.main_view_scale;
    profile->aux_view_font_size = config.aux_view_font_size;
    profile->enable_right_panes = config.enable_right_panes;
    profile->enable_bottom_panes = config.enable_bottom_panes;
    profile->left_overlays_touch_screen_edge =
        config.left_overlays_touch_screen_edge;
    profile->show_overlay_log_border = config.show_overlay_log_border;
    profile->show_main_menu_button = config.show_main_menu_button;
    profile->left_panel_expanded_on_launch =
        config.left_panel_expanded_on_launch;
    profile->left_panel_compact_mode = config.left_panel_compact_mode;
    profile->log_pane_display_filter = config.log_pane_display_filter;
    profile->dice_roll_lock_ms = config.dice_roll_lock_ms;
    profile->dice_roll_overlay_ms = config.dice_roll_overlay_ms;
    profile->popup_notification_ms = config.popup_notification_ms;
    profile->touch_top_panel_arrows_visible =
        config.touch_top_panel_arrows_visible;
    profile->touch_top_panel_default_open =
        config.touch_top_panel_default_open;
    profile->touch_top_panel_cell_count = config.touch_top_panel_cell_count;
    profile->touch_top_panel_rows = config.touch_top_panel_rows;
    profile->touch_top_panel_size = config.touch_top_panel_size;
    memcpy(profile->touch_top_panel_bindings,
        config.touch_top_panel_bindings,
        sizeof(profile->touch_top_panel_bindings));
    memcpy(profile->touch_top_panel_long_bindings,
        config.touch_top_panel_long_bindings,
        sizeof(profile->touch_top_panel_long_bindings));
    sdl_copy_pane_configs(profile->pane_configs,
        &profile->pane_count, pane_config, pane_config_count);
}

void sdl_store_active_pane_profile(int mode)
{
    int profile_index = sdl_pane_profile_index_for_mode(mode);

    if (profile_index < 0 || profile_index >= SDL_PANE_PROFILE_COUNT)
        return;

    sdl_capture_pane_profile(&g_pane_profiles[profile_index]);

#if SIL_SDL_MOBILE_BUILD
    /* Orientation profiles own pane and menu layout, but the configured main
     * view scale is one display setting.  Keep it continuous when rotating. */
    {
        int other_orientation = config.mobile_portrait_mode
            ? SDL_PANE_ORIENTATION_LANDSCAPE
            : SDL_PANE_ORIENTATION_PORTRAIT;
        int other_index = SDL_PANE_PROFILE_INDEX(other_orientation, mode);

        g_pane_profiles[other_index].main_view_scale = config.main_view_scale;
    }
#endif
}

void sdl_apply_stored_pane_profile(int mode)
{
    int profile_index = sdl_pane_profile_index_for_mode(mode);
    const struct sdl_pane_profile* profile;

    if (!sdl_min_terminal_mode_is_valid(mode))
        return;
    if (profile_index < 0 || profile_index >= SDL_PANE_PROFILE_COUNT)
        return;

    profile = &g_pane_profiles[profile_index];

    config.main_view_scale = profile->main_view_scale;
    config.main_view_scale = sdl_clamp_main_view_scale_platform_bounds(
        config.main_view_scale, mode);
    config.aux_view_font_size = profile->aux_view_font_size;
    config.enable_right_panes = profile->enable_right_panes;
    config.enable_bottom_panes = profile->enable_bottom_panes;
    config.left_overlays_touch_screen_edge =
        profile->left_overlays_touch_screen_edge;
    config.show_overlay_log_border = profile->show_overlay_log_border;
    config.show_main_menu_button = profile->show_main_menu_button;
    config.left_panel_expanded_on_launch =
        profile->left_panel_expanded_on_launch;
    config.left_panel_compact_mode = profile->left_panel_compact_mode;
    config.log_pane_display_filter = profile->log_pane_display_filter;
    config.dice_roll_lock_ms = profile->dice_roll_lock_ms;
    config.dice_roll_overlay_ms = profile->dice_roll_overlay_ms;
    config.popup_notification_ms = profile->popup_notification_ms;
    config.touch_top_panel_arrows_visible =
        profile->touch_top_panel_arrows_visible;
    config.touch_top_panel_default_open =
        profile->touch_top_panel_default_open;
    config.touch_top_panel_cell_count = profile->touch_top_panel_cell_count;
    config.touch_top_panel_rows = profile->touch_top_panel_rows;
    config.touch_top_panel_size = profile->touch_top_panel_size;
    memcpy(config.touch_top_panel_bindings,
        profile->touch_top_panel_bindings,
        sizeof(config.touch_top_panel_bindings));
    memcpy(config.touch_top_panel_long_bindings,
        profile->touch_top_panel_long_bindings,
        sizeof(config.touch_top_panel_long_bindings));
    sdl_log_pane_sync_display_filter_from_config();
    g_left_panel_pane_expanded = config.left_panel_expanded_on_launch;
    g_touch_top_panel_open = !config.touch_top_panel_arrows_visible
        || config.touch_top_panel_default_open;
    sdl_copy_pane_configs(pane_config, &pane_config_count,
        profile->pane_configs, profile->pane_count);
    if (sdl_ensure_main_menu_access())
        sdl_capture_pane_profile(&g_pane_profiles[profile_index]);
}

void sdl_seed_all_pane_profiles_from_active(void)
{
    struct sdl_pane_profile base;

    memset(&base, 0, sizeof(base));
    sdl_capture_pane_profile(&base);

    for (int mode = 0; mode < SDL_MIN_TERMINAL_MODE_COUNT; mode++) {
        int landscape = SDL_PANE_PROFILE_INDEX(
            SDL_PANE_ORIENTATION_LANDSCAPE, mode);
        int portrait = SDL_PANE_PROFILE_INDEX(
            SDL_PANE_ORIENTATION_PORTRAIT, mode);

        g_pane_profiles[landscape] = base;
        g_pane_profiles[landscape].touch_top_panel_size =
            (float)g_pane_profiles[landscape].main_view_scale;
        g_pane_profiles[portrait] = base;
        sdl_pane_profile_apply_portrait_defaults(
            &g_pane_profiles[portrait]);
    }
}

int sdl_pane_config_index_in_array(const struct pane_config* configs,
    int count, enum pane_type pane)
{
    if (!configs)
        return -1;

    for (int i = 0; i < count; i++) {
        if (configs[i].pane == pane)
            return i;
    }

    return -1;
}

bool sdl_normalize_unified_log_pane_config(struct pane_config* configs,
    int* config_count, bool enable_added_log)
{
    int log_idx;
    int rolls_idx;
    bool originally_had_log;
    bool originally_had_rolls;
    bool log_added = false;
    bool rolls_added = false;
    bool changed = false;

    if (!configs || !config_count)
        return false;

    if (*config_count < 0)
        *config_count = 0;
    if (*config_count > MAX_PANE_CONFIGS)
        *config_count = MAX_PANE_CONFIGS;

    log_idx = sdl_pane_config_index_in_array(configs, *config_count, PANE_LOG);
    rolls_idx = sdl_pane_config_index_in_array(configs, *config_count,
        PANE_ROLLS);
    originally_had_log = (log_idx >= 0);
    originally_had_rolls = (rolls_idx >= 0);

    if (rolls_idx < 0 && *config_count < MAX_PANE_CONFIGS) {
        rolls_idx = *config_count;
        configs[(*config_count)++] = (struct pane_config){
            .pane = PANE_ROLLS,
            .where = PLACE_TOP_RIGHT,
            .enabled = enable_added_log && !originally_had_log,
            .rect = { .rows = SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS, .cols = 0 },
            .font_size = 0,
            .ratio = 0.0f,
        };
        rolls_added = true;
        changed = true;
    }

    if (log_idx < 0 && *config_count < MAX_PANE_CONFIGS) {
        log_idx = *config_count;
        configs[(*config_count)++] = (struct pane_config){
            .pane = PANE_LOG,
            .where = PLACE_BOTTOM,
            .enabled = enable_added_log && !originally_had_rolls,
            .rect = { .rows = SDL_LOG_PANE_DEFAULT_ROWS, .cols = 0 },
            .font_size = 0,
            .ratio = 0.0f,
        };
        log_added = true;
        changed = true;
    }

    if (log_idx >= 0) {
        struct pane_config* log = &configs[log_idx];

        if (pane_placement_is_overlay(log->where)) {
            if (rolls_idx >= 0) {
                struct pane_config* rolls = &configs[rolls_idx];

                rolls->where = log->where;
                rolls->enabled = log->enabled;
                rolls->rect.rows = log->rect.rows > 0
                    ? log->rect.rows : SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS;
                rolls->rect.cols = 0;
                rolls->font_size = log->font_size;
                rolls->ratio = 0.0f;
            }
            log->where = PLACE_BOTTOM;
            log->enabled = false;
            log->rect.rows = SDL_LOG_PANE_DEFAULT_ROWS;
            log->rect.cols = 0;
            log->ratio = 0.0f;
            changed = true;
        }
        if (log->where == 0 || !pane_type_allows_placement(PANE_LOG, log->where)) {
            log->where = PLACE_BOTTOM;
            changed = true;
        }
        if (pane_placement_is_bottom(log->where) && log->rect.rows <= 0)
        {
            log->rect.rows = SDL_LOG_PANE_DEFAULT_ROWS;
            changed = true;
        }
    }

    if (rolls_idx >= 0) {
        struct pane_config* rolls = &configs[rolls_idx];
        bool migrate_rows_to_overlay_default = pane_placement_is_bottom(
            rolls->where);

        if (rolls->where == 0
            || !pane_type_allows_placement(PANE_ROLLS, rolls->where)
            || pane_placement_is_bottom(rolls->where))
        {
            rolls->where = PLACE_TOP_RIGHT;
            migrate_rows_to_overlay_default = true;
            changed = true;
        }
        if (pane_placement_is_overlay(rolls->where)
            && migrate_rows_to_overlay_default
            && rolls->rect.rows != SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS)
        {
            rolls->rect.rows = SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS;
            changed = true;
        }
        if (pane_placement_is_overlay(rolls->where)
            && rolls->rect.rows <= 0)
        {
            rolls->rect.rows = SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS;
            changed = true;
        }
        if (pane_placement_is_overlay(rolls->where)
            && rolls->rect.cols != 0)
        {
            rolls->rect.cols = 0;
            changed = true;
        }
        /* Existing log pane choices keep their stored enabled state; a
         * compatibility sibling added above starts off unless there was no log
         * pane at all, so deliberate disables survive reloads. */
    }

    if (log_idx >= 0 && rolls_idx >= 0
        && configs[log_idx].enabled && configs[rolls_idx].enabled)
    {
        if (rolls_added && !log_added)
            configs[rolls_idx].enabled = false;
        else if (log_added && !rolls_added)
            configs[log_idx].enabled = false;
        else if (log_idx < rolls_idx)
            configs[rolls_idx].enabled = false;
        else
            configs[log_idx].enabled = false;
        changed = true;
    }

    return changed;
}

void sdl_normalize_unified_log_pane_profiles(bool enable_added_log)
{
    for (int mode = 0; mode < SDL_PANE_PROFILE_COUNT; mode++) {
        (void)sdl_normalize_unified_log_pane_config(
            g_pane_profiles[mode].pane_configs,
            &g_pane_profiles[mode].pane_count, enable_added_log);
    }
}

enum pane_placement sdl_default_status_pane_placement(
    const struct pane_config* configs, int config_count)
{
    int rolls_idx = sdl_pane_config_index_in_array(configs, config_count,
        PANE_ROLLS);
    int log_idx = sdl_pane_config_index_in_array(configs, config_count,
        PANE_LOG);

    /* Keep the status readout away from the depth readout in the top-right.
     * The overlay log owns that corner on wide layouts, so status sits at the
     * top centre; with the desktop bottom log, status uses bottom-right. */
    if (rolls_idx >= 0 && configs[rolls_idx].enabled
        && configs[rolls_idx].where == PLACE_TOP_RIGHT)
    {
        return PLACE_TOP_CENTER;
    }
    if (log_idx >= 0 && configs[log_idx].enabled
        && pane_placement_is_bottom(configs[log_idx].where))
    {
        return PLACE_BOTTOM_RIGHT;
    }

    return PLACE_TOP_CENTER;
}

int sdl_default_main_scale_for_screen_size(int screen_width,
    int screen_height, int mode)
{
    int min_cols = (mode == SDL_MIN_TERMINAL_COMPACT) ? 50 : 80;
    int min_rows = (mode == SDL_MIN_TERMINAL_COMPACT) ? 18 : 24;
    int max_scale_w;
    int max_scale_h;
    int max_scale;

    if (screen_width <= 0 || screen_height <= 0)
        return SDL_MAIN_VIEW_MIN_SCALE;

    max_scale_w = (screen_width / min_cols) * 2 / TILE_SIZE;
    max_scale_h = screen_height / min_rows / TILE_SIZE;
    max_scale = (max_scale_w < max_scale_h) ? max_scale_w : max_scale_h;

    if (max_scale < SDL_MAIN_VIEW_MIN_SCALE)
        max_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale > SDL_MAIN_VIEW_MAX_SCALE)
        max_scale = SDL_MAIN_VIEW_MAX_SCALE;

    return max_scale;
}

void sdl_store_platform_max_main_view_scales(int screen_width,
    int screen_height)
{
    for (int mode = 0; mode < SDL_MIN_TERMINAL_MODE_COUNT; mode++)
    {
        g_platform_max_main_view_scale[mode] =
            sdl_default_main_scale_for_screen_size(screen_width, screen_height,
                mode);
    }

    log_info("Platform max main view scale: normal=%d compact=%d",
        g_platform_max_main_view_scale[SDL_MIN_TERMINAL_NORMAL],
        g_platform_max_main_view_scale[SDL_MIN_TERMINAL_COMPACT]);
}

void sdl_refresh_platform_max_main_view_scales_for_current_layout(
    const char* reason)
{
#if SIL_SDL_MOBILE_BUILD
    SDL_Rect window_pixels;
    int screen_w;
    int screen_h;
    int old_normal;
    int old_compact;

    if (!g_state.window)
        return;

    window_pixels = sdl_get_window_pixel_rect();
    if (!sdl_rect_has_area(&window_pixels))
        return;

    /* Platform max is a device capability cap, not a fit-to-layout limit:
     * use full window pixels in landscape orientation so portrait windows
     * and safe-area insets cannot lower it below what the device supports.
     * Fitting the view to the actual layout is enforced separately by the
     * zoom clamps. */
    screen_w = window_pixels.w;
    screen_h = window_pixels.h;
    if (screen_w < screen_h) {
        int tmp = screen_w;

        screen_w = screen_h;
        screen_h = tmp;
    }

    old_normal = g_platform_max_main_view_scale[SDL_MIN_TERMINAL_NORMAL];
    old_compact = g_platform_max_main_view_scale[SDL_MIN_TERMINAL_COMPACT];
    for (int mode = 0; mode < SDL_MIN_TERMINAL_MODE_COUNT; mode++)
    {
        g_platform_max_main_view_scale[mode] =
            sdl_default_main_scale_for_screen_size(screen_w, screen_h, mode);
    }

    if (old_normal != g_platform_max_main_view_scale[SDL_MIN_TERMINAL_NORMAL]
        || old_compact !=
            g_platform_max_main_view_scale[SDL_MIN_TERMINAL_COMPACT])
    {
        log_info("%s: platform max main view scale refreshed for window "
                 "%dx%d: normal=%d->%d compact=%d->%d",
            reason ? reason : "layout",
            screen_w, screen_h,
            old_normal,
            g_platform_max_main_view_scale[SDL_MIN_TERMINAL_NORMAL],
            old_compact,
            g_platform_max_main_view_scale[SDL_MIN_TERMINAL_COMPACT]);
    }
#else
    (void)reason;
#endif
}

void sdl_reset_config_to_resolution_defaults(int screen_width,
    int screen_height)
{
    int default_main_scales[SDL_MIN_TERMINAL_MODE_COUNT];

#if SIL_SDL_MOBILE_BUILD
    /* A phone's current window can be portrait, but main view scale is a
     * shared display setting across orientations.  Derive defaults from the
     * device's landscape-sized bounds so resetting in portrait does not pick
     * a smaller scale. */
    if (screen_width < screen_height) {
        int tmp = screen_width;

        screen_width = screen_height;
        screen_height = tmp;
    }
#endif
    for (int mode = 0; mode < SDL_MIN_TERMINAL_MODE_COUNT; mode++) {
        default_main_scales[mode] =
            sdl_default_main_scale_for_screen_size(screen_width,
                screen_height, mode);
    }

    (void)sdl_config_set_defaults_for_resolution(&config, pane_config,
        &pane_config_count, MAX_PANE_CONFIGS, screen_width, screen_height);

    config.main_view_scale = default_main_scales[config.min_terminal_mode];
    config.aux_view_font_size = 0;
    config.enable_right_panes = false;
#if SIL_SDL_MOBILE_BUILD
    config.enable_bottom_panes = false;
#else
    config.enable_bottom_panes = true;
#endif

    (void)sdl_normalize_unified_log_pane_config(pane_config,
        &pane_config_count, true);
    sdl_seed_all_pane_profiles_from_active();
    for (int mode = 0; mode < SDL_MIN_TERMINAL_MODE_COUNT; mode++) {
        int landscape = SDL_PANE_PROFILE_INDEX(
            SDL_PANE_ORIENTATION_LANDSCAPE, mode);
        int portrait = SDL_PANE_PROFILE_INDEX(
            SDL_PANE_ORIENTATION_PORTRAIT, mode);

        g_pane_profiles[landscape].main_view_scale =
            default_main_scales[mode];
        g_pane_profiles[landscape].touch_top_panel_size =
            (float)default_main_scales[mode];
        g_pane_profiles[portrait].main_view_scale =
            default_main_scales[mode];
    }
    sdl_log_pane_sync_display_filter_from_config();

    log_info("Default main view scales: normal=%d compact=%d for %dx%d; active=%d (%s), selected without side panes; default log pane rows=%d",
        default_main_scales[SDL_MIN_TERMINAL_NORMAL],
        default_main_scales[SDL_MIN_TERMINAL_COMPACT],
        screen_width, screen_height,
        config.main_view_scale,
        (config.min_terminal_mode == SDL_MIN_TERMINAL_COMPACT) ? "compact" : "normal",
        SDL_LOG_PANE_DEFAULT_ROWS);
}

int sdl_min_terminal_cols_for_mode(int mode)
{
    return (mode == SDL_MIN_TERMINAL_COMPACT) ? 50 : 80;
}

int sdl_min_terminal_rows_for_mode(int mode)
{
    return (mode == SDL_MIN_TERMINAL_COMPACT) ? 18 : 24;
}

int sdl_current_min_terminal_cols(void)
{
    return sdl_min_terminal_cols_for_mode(config.min_terminal_mode);
}

int sdl_current_min_terminal_rows(void)
{
    return sdl_min_terminal_rows_for_mode(config.min_terminal_mode);
}

const char* sdl_min_terminal_mode_name(int mode)
{
    return (mode == SDL_MIN_TERMINAL_COMPACT) ? "compact" : "normal";
}


const touch_pane_slot_info g_touch_pane_slots[SDL_TOUCH_PANE_BUTTON_COUNT] = {
    { "Esc", "Esc", ESCAPE },
    { "Stealth", "Stealth", 'S' },
    { "2nd Panel", "2nd Panel", GAMEPAD_BIND_SHIFT },
    { "Char", "Char", 'h' },
    { "Inv", "Inv", 'i' },
    { "Supply", "Supply", 'j' },
    { "Use", "Use", 'u' },
    { "Sing", "Sing", 's' },
    { "Shoot", "Shoot", 'f' },
    { "Northwest", NULL, '7' },
    { "North", NULL, '8' },
    { "Northeast", NULL, '9' },
    { "West", NULL, '4' },
    { "Center", "Confirm", INPUT_BIND_CONFIRM },
    { "East", NULL, '6' },
    { "Southwest", NULL, '1' },
    { "South", NULL, '2' },
    { "Southeast", NULL, '3' },
    { "Staff", "View", 'l' },
    { "Desc", "Desc", 'x' },
    { "Drop", "Staff", 'a' },
    { "Map", "Map", 'M' },
    { "Hero", "Hero", 'h' },
    { "Ability", "Ability", 'y' },
};

const int g_touch_pane_visible_slots[SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT] = {
    0, 1, 2,
    6, 7, 8,
    9, 10, 11,
    12, 13, 14,
    15, 16, 17,
    3, 4, 5,
    18, 19, 20,
};

int sdl_touch_pane_visible_slot_at(int visible_index)
{
    if (visible_index < 0 || visible_index >= SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT)
        return -1;

    return g_touch_pane_visible_slots[visible_index];
}


sdl_state g_state;
sdl_view g_views[MAX_TERM_DATA];

mono_font_prewarm_request
    g_mono_font_prewarm_queue[SDL_MONO_FONT_PREWARM_QUEUE_MAX];
int g_mono_font_prewarm_count = 0;
mono_font_prewarm_job g_mono_font_prewarm_job;
Uint64 g_mono_font_atlas_generation = 1;
Uint64 g_story_font_generation = 1;
Uint64 g_sdl_present_generation = 1;
SDL_Rect g_pane_rects[PANE_MAX];
SDL_Texture* g_left_panel_canvas = NULL;
int g_left_panel_canvas_w = 0;
int g_left_panel_canvas_h = 0;
bool g_left_panel_debug_dump_rows = false;
bool g_active_side_panes = true;
bool g_active_bottom_panes = true;
int g_description_overlay_main_anchor_depth = 0;
int g_description_overlay_full_main_anchor_depth = 0;
bool g_supporting_panes_layout_visible = true;
int g_inventory_pane_layout_rows = -1;
int g_supply_pane_layout_rows = -1;
bool g_touch_pane_hidden_layout_active = false;
bool g_touch_pane_proto_layout_active = false;
bool g_suppress_layout_refresh_present = false;
bool g_skip_main_redraw_on_layout_refresh = false;
bool g_defer_resize_handle_stuff = false;
bool g_touch_tutorial_suppress_runtime_top_panel = false;
gamepad_input_state g_gamepad_state;
bool g_gamepad_auto_ui = false;
int g_default_gamepad_button_bindings[SDL_GAMEPAD_BUTTON_COUNT];
int g_default_gamepad_trigger_bindings[GAMEPAD_TRIGGER_COUNT];
int g_default_gamepad_left_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
int g_default_gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
int g_default_gamepad_button_combo_bindings[GAMEPAD_MODIFIER_COUNT][SDL_GAMEPAD_BUTTON_COUNT];
int g_default_gamepad_trigger_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_TRIGGER_COUNT];
int g_default_gamepad_left_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
int g_default_gamepad_right_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
int g_default_gamepad_shoulder_combo_binding = GAMEPAD_BIND_NONE;
bool g_default_gamepad_bindings_ready = false;
bool g_default_mouse_enabled = true;
int g_default_mouse_movement_mode = SDL_MOUSE_MOVEMENT_ON;
bool g_default_mouse_tile_pointer = false;
bool g_default_mouse_settings_ready = false;
int g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_BUTTON_COUNT];
char g_default_touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_LABEL_LEN];
bool g_default_touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_COUNT];
int g_default_touch_profile = SDL_TOUCH_PROFILE_TOUCH_PANE;
bool g_default_touch_pane_default_open = true;
bool g_default_touch_pane_key_labels_visible = false;
bool g_default_touch_pane_inventory_equipment_cycle = true;
int g_default_touch_movement_mode = SDL_TOUCH_MOVEMENT_ON;
int g_default_touch_zone_overlay_mode = SDL_TOUCH_ZONE_OVERLAY_MARKERS;
bool g_default_touch_round_movement_enabled = false;
int g_default_touch_zone_center_bindings[SDL_TOUCH_ZONE_CENTER_BINDING_COUNT];
int g_default_touch_corner_up_down_side = SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
int g_default_touch_corner_action_bindings[SDL_TOUCH_CORNER_ACTION_BINDING_COUNT];
bool g_default_touch_top_panel_arrows_visible = true;
bool g_default_touch_top_panel_default_open = false;
float g_default_touch_top_panel_size = SDL_TOUCH_TOP_PANEL_SIZE_DEFAULT;
int g_default_touch_top_panel_cell_count = SDL_TOUCH_TOP_PANEL_CELL_COUNT_DEFAULT;
int g_default_touch_top_panel_rows = SDL_TOUCH_TOP_PANEL_ROWS_DEFAULT;
int g_default_touch_top_panel_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
int g_default_touch_top_panel_long_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
bool g_default_touch_thumb_enabled = true;
int g_default_touch_thumb_bindings[SDL_TOUCH_THUMB_BUTTON_COUNT];
int g_default_touch_thumb_long_bindings[SDL_TOUCH_THUMB_BUTTON_COUNT];
bool g_default_touch_swipe_enabled = true;
int g_default_touch_swipe_bindings[TOUCH_SWIPE_DIR_COUNT];
bool g_default_touch_pane_bindings_ready = false;
bool g_gamepad_capture_active = false;
bool g_gamepad_capture_ready = false;
Uint64 g_gamepad_capture_arm_time = 0;
bool g_gamepad_capture_allow_modifier_combo = false;
int g_gamepad_capture_modifier = GAMEPAD_BIND_NONE;
int g_gamepad_capture_type = GAMEPAD_CAPTURE_BUTTON;
int g_gamepad_capture_id = 0;
bool g_keyboard_capture_active = false;
bool g_keyboard_capture_ready = false;
Uint64 g_keyboard_capture_arm_time = 0;
SDL_Scancode g_keyboard_capture_scancode = SDL_SCANCODE_UNKNOWN;
u16b g_keyboard_capture_modifiers = 0;
int g_touch_pane_flash_slot = -1;
Uint64 g_touch_pane_flash_until = 0;
int g_touch_pane_pressed_slot = -1;
bool g_touch_pane_second_panel = false;
bool g_touch_pane_ctrl_toggle = false;
bool g_touch_pane_reset_confirm_active = false;
bool g_touch_pane_yes_no_prompt_active = false;
char g_touch_pane_yes_no_prompt_text[SDL_TOUCH_YES_NO_LINE_LEN];
sdl_touch_yes_no_prompt_placement g_touch_pane_yes_no_prompt_placement =
    SDL_TOUCH_YES_NO_PLACEMENT_CENTER;
sdl_touch_yes_no_prompt_hover g_touch_pane_yes_no_prompt_hover =
    SDL_TOUCH_YES_NO_HOVER_NONE;
bool g_touch_pane_yes_no_prompt_anchor_active = false;
int g_touch_pane_yes_no_prompt_anchor_y = 0;
int g_touch_pane_yes_no_prompt_anchor_x = 0;
bool g_touch_pane_mobile_open = true;
touch_pane_press_state g_touch_pane_press;
touch_pane_press_state g_touch_thumb_press;
int g_touch_thumb_flash_button = -1;
Uint64 g_touch_thumb_flash_until = 0;
int g_touch_thumb_pressed_button = -1;
bool g_touch_mouse_fallback_active = false;
SDL_MouseID g_touch_mouse_fallback_mouse_id = 0;
bool g_handheld_untagged_mouse_fallback_logged = false;
touch_swipe_state g_touch_swipe;
welcome_touch_press_state g_welcome_touch_press;
sdl_welcome_screen_state g_sdl_welcome_screen = {
    .mode = SDL_WELCOME_SCREEN_HIDDEN,
    .intro_style = INTRO_STYLE_FLAME
};
sdl_character_sheet_screen_state g_sdl_character_sheet_screen = {
    .context = SDL_CHARACTER_SHEET_HIDDEN,
    .focus_choice = -1,
    .selected_index = -1,
    .hover_choice = SDL_CHAR_SHEET_NO_HOVER
};
touch_zone_press_state g_touch_zone_press;
touch_top_panel_press_state g_touch_top_panel_press;
touch_round_press_state g_touch_round_press;
int g_touch_round_last_dir = 0;
bool g_player_tile_facing_right = false;
bool g_touch_top_panel_open = true;
int g_touch_top_panel_pressed_slot = -1;
int g_touch_top_panel_hover_slot = -1;
int g_touch_top_panel_flash_slot = -1;
Uint64 g_touch_top_panel_flash_until = 0;
menu_touch_press_state g_menu_touch_press;
character_panel_press_state g_character_panel_press;
int g_screen_back_gesture_depth = 0;
bool g_screen_back_right_button_pending = false;
screen_back_touch_press_state g_screen_back_touch_press;
bool g_screen_back_suppress_touch_up = false;
SDL_FingerID g_screen_back_suppress_touch_finger_id = 0;
menu_scroll_drag_state g_menu_scroll_drag;
pane_layout_drag_state g_pane_layout_drag;
side_pane_menu_state g_side_pane_menu = {
    .hover_index = -1,
    .press_index = -1,
    .long_press_pane = PANE_MAIN,
};
log_pane_menu_state g_log_pane_menu = {
    .hover_index = -1,
    .press_index = -1,
    .target_pane = PANE_MAIN,
    .long_press_pane = PANE_MAIN,
};
int g_log_pane_display_filters[PANE_MAX] = {
    [PANE_LOG] = LOG_HISTORY_FILTER_ALL,
    [PANE_ROLLS] = LOG_HISTORY_FILTER_ALL,
};
bool g_log_pane_display_pending = false;
enum pane_type g_log_pane_pending_pane = PANE_MAIN;
int g_log_pane_pending_filter = LOG_HISTORY_FILTER_ALL;
map_touch_press_state g_map_touch_press;
player_action_menu_state g_player_action_menu;
player_exchange_target_state g_player_exchange_target;
bool g_map_touch_selected = false;
int g_map_touch_selected_y = 0;
int g_map_touch_selected_x = 0;
minimap_state g_minimap;
side_map_pane_state g_side_map_pane = {
    .default_zoom_pending = true,
    .last_depth = -32768
};
bool g_minimap_pending_focus_active = false;
int g_minimap_pending_focus_y = -1;
int g_minimap_pending_focus_x = -1;
pointer_attack_state g_pointer_attack = {
    .mode = SDL_POINTER_ATTACK_MELEE
};
pointer_aim_state g_pointer_aim;
mouse_path_state g_mouse_path;
object_tooltip_state g_object_tooltip;
description_overlay_state g_description_overlay;
main_map_drag_state g_main_map_drag;
bool g_unified_look_active = false;
sdl_unified_look_prompt_state g_unified_look_prompt;
sdl_unified_look_sidebar_state g_unified_look_sidebar;
sdl_song_menu_state g_song_menu = { .highlight = -1 };
sdl_question_menu_state g_question_menu = { .highlight = -1 };
bool g_unified_look_map_hover_enabled = false;
bool g_unified_look_map_hover_pending = false;
bool g_unified_look_map_hover_wake_pending = false;
int g_unified_look_map_hover_y = 0;
int g_unified_look_map_hover_x = 0;
bool g_unified_look_map_describe_pending = false;
int g_unified_look_map_describe_y = 0;
int g_unified_look_map_describe_x = 0;
bool g_unified_look_map_target_pending = false;
int g_unified_look_map_target_y = 0;
int g_unified_look_map_target_x = 0;
unified_look_map_drag_state g_unified_look_map_drag;
bool g_unified_look_map_pan_pending = false;
bool g_unified_look_map_pan_wake_pending = false;
int g_unified_look_map_pan_dy = 0;
int g_unified_look_map_pan_dx = 0;
bool g_unified_look_main_zoom_pending = false;
bool g_unified_look_main_zoom_wake_pending = false;
int g_unified_look_main_zoom_scale = 0;
int g_main_screen_status_selected_action = SDL_STATUS_CLICK_NONE;
int g_main_screen_status_selected_col = -1;
int g_main_screen_panel_selected_action = SDL_PANEL_CLICK_NONE;
int g_main_screen_panel_selected_row = -1;
bool g_main_menu_pane_hover = false;
bool g_main_menu_overlay_active = false;
int g_main_menu_overlay_highlight = MAIN_MENU_CHARACTER;
int g_main_menu_overlay_hover_choice = 0;
int g_main_menu_overlay_first_choice = 1;
int g_main_menu_overlay_left_stick_dir = 0;
int g_main_menu_overlay_right_stick_dir = 0;
int g_depth_pane_hover_action = 0;
u16b g_mouse_path_reverse[SDL_MOUSE_PATH_MAX_GRIDS];
mouse_path_search_state g_mouse_path_search;
const byte g_mouse_path_route_dirs[SDL_MOUSE_PATH_ROUTE_DIRS] =
    { 1, 2, 3, 4, 6, 7, 8, 9 };
bool g_sdl_blocking_key_wait = false;
bool g_direct_touch_present = SIL_SDL_MOBILE_BUILD ? true : false;
int g_main_view_zoom_scale = 0;
int g_main_view_layout_scale_override = 0;
int g_main_view_zoom_suspended_stack[16];
int g_main_view_zoom_suspended_depth = 0;
int g_terminal_menu_scale_override = 0;
int g_terminal_menu_scale_stack[16];
int g_terminal_menu_scale_depth = 0;
int g_auto_aux_main_cell_h_override = 0;
#if SIL_SDL_MOBILE_BUILD
bool g_mobile_lifecycle_watch_registered = false;
bool g_mobile_lifecycle_autosaved = false;
#endif
#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
bool g_mobile_first_start_auto_scale_pending = false;
#endif
