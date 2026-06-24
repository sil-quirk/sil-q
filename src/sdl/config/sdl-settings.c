#include "angband.h"
#include "sdl/main-sdl-private.h"

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
    offset += (size_t)strnfmt(buf + offset, size - offset,
        "Left Panel Launch State: %s\n",
        config.left_panel_expanded_on_launch ? "Full" : "Compact");
    offset += (size_t)strnfmt(buf + offset, size - offset,
        "Left Panel Compact Mode: %s\n\n",
        (get_sdl_left_panel_compact_mode() == SDL_LEFT_PANEL_COMPACT_ROW)
            ? "Row" : "Column");
    
    // Pane configurations
    offset += (size_t)strnfmt(buf + offset, size - offset, "=== Pane Configuration ===\n");
    int support_count = 0;
    for (int i = 0; i < pane_config_count && i < MAX_PANE_CONFIGS; i++) {
        if (pane_config[i].pane != PANE_MAIN
            && pane_config[i].pane != PANE_MAIN_MENU)
        {
            support_count++;
        }
    }
    offset += (size_t)strnfmt(buf + offset, size - offset, "Configurable Panes: %d\n\n", support_count);

    for (int i = 0; i < pane_config_count && i < MAX_PANE_CONFIGS; i++) {
        const struct pane_config* pc = &pane_config[i];
        if (pc->pane == PANE_MAIN)
            continue;
        if (pc->pane == PANE_MAIN_MENU)
            continue;
        const char* type_str = "UNKNOWN";
        const char* where_str = pane_placement_display_name(pc->where);
        
        switch (pc->pane) {
            case PANE_MAIN: type_str = "MAIN"; break;
            case PANE_INVENTORY: type_str = "INVENTORY"; break;
            case PANE_SUPPLY: type_str = "SUPPLY"; break;
            case PANE_WORN: type_str = "WORN"; break;
            case PANE_ROLLS: type_str = "ROLLS"; break;
            case PANE_INFO: type_str = "INFO"; break;
            case PANE_CHARACTER: type_str = "CHARACTER"; break;
            case PANE_LOG: type_str = "LOG"; break;
            case PANE_MONSTERS: type_str = "MONSTERS"; break;
            case PANE_MAP: type_str = "MAP"; break;
            case PANE_TOUCH: type_str = "TOUCH"; break;
            case PANE_LEFT_PANEL: type_str = "LEFT_PANEL"; break;
            case PANE_STATUS: type_str = "STATUS"; break;
            case PANE_DEPTH: type_str = "DEPTH"; break;
            case PANE_DESCRIPTION: type_str = "DESCRIPTION"; break;
            case PANE_OVERLAY_MENU: type_str = "OVERLAY_MENU"; break;
            case PANE_COMBAT: type_str = "COMBAT"; break;
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

int get_sdl_effective_main_view_scale(void)
{
    return sdl_current_main_view_scale();
}

int get_sdl_min_main_view_scale(void)
{
    return sdl_main_view_scale_floor();
}

int get_sdl_min_terminal_mode(void)
{
    return config.min_terminal_mode;
}

void set_sdl_min_terminal_mode(int value)
{
    if (!sdl_min_terminal_mode_is_valid(value))
        return;
    if (config.min_terminal_mode == value)
        return;

    sdl_store_active_pane_profile(config.min_terminal_mode);
    config.min_terminal_mode = value;
    sdl_apply_stored_pane_profile(value);

    if (config.main_view_scale > get_sdl_max_scale())
        config.main_view_scale = get_sdl_max_scale();
    config.main_view_scale = sdl_clamp_main_view_scale_platform_bounds(
        config.main_view_scale, config.min_terminal_mode);
    g_main_view_zoom_scale = 0;
}

void set_sdl_main_view_scale(int value)
{
    int max_scale = get_sdl_max_scale();
    int min_scale = sdl_main_view_scale_floor();
    int platform_max = get_sdl_platform_max_main_view_scale();

    if (max_scale < SDL_MAIN_VIEW_MIN_SCALE)
        max_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale > platform_max)
        max_scale = platform_max;
    if (max_scale < min_scale)
        max_scale = min_scale;
    if (value < min_scale)
        value = min_scale;
    if (value <= max_scale) {
        config.main_view_scale = value;
        g_main_view_zoom_scale = 0;
#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
        g_mobile_first_start_auto_scale_pending = false;
#endif
    }
}

bool set_sdl_main_view_zoom_scale(int value)
{
    int min_scale = get_sdl_min_main_view_zoom_scale();
    int max_scale = get_sdl_max_main_view_zoom_scale();

    if (max_scale < SDL_MAIN_VIEW_MIN_SCALE)
        max_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (min_scale < sdl_main_view_scale_floor())
        min_scale = sdl_main_view_scale_floor();
    if (max_scale < min_scale)
        max_scale = min_scale;
    if (value < min_scale)
        value = min_scale;
    if (value > max_scale)
        value = max_scale;

    if (value == sdl_configured_main_view_scale())
        g_main_view_zoom_scale = 0;
    else
        g_main_view_zoom_scale = value;

    return true;
}

/*
 * Drop any active gameplay main-view zoom back to the configured default and
 * resize the terminal to match.  Called when a new session begins so a zoom
 * carried over from a previous session does not leave the grid too short --
 * notably when relaunching into Blitz from the in-game menu, which never passes
 * through the title screen (or a screen_save) that would otherwise reset it.
 */
void sdl_reset_main_view_zoom(void)
{
    if (g_main_view_zoom_scale == 0)
        return;

    g_main_view_zoom_scale = 0;
    sdl_apply_config_no_redraw();
}

void sdl_suspend_main_view_zoom_for_saved_screen(void)
{
    int saved_zoom = 0;
    bool keep_zoom = sdl_main_view_zoom_keep_for_saved_screen();

    if (g_main_view_zoom_suspended_depth
        < (int)N_ELEMENTS(g_main_view_zoom_suspended_stack))
    {
        if (g_main_view_zoom_suspended_depth == 0
            && g_main_view_zoom_scale > 0
            && !keep_zoom)
        {
            saved_zoom = g_main_view_zoom_scale;
        }

        g_main_view_zoom_suspended_stack[g_main_view_zoom_suspended_depth++] =
            saved_zoom;
    }

    if (saved_zoom <= 0)
        return;

    g_main_view_zoom_scale = 0;
    sdl_apply_config_no_redraw();
}

void sdl_resume_main_view_zoom_for_saved_screen(void)
{
    int saved_zoom;

    if (g_main_view_zoom_suspended_depth <= 0)
        return;

    saved_zoom =
        g_main_view_zoom_suspended_stack[--g_main_view_zoom_suspended_depth];
    g_main_view_zoom_suspended_stack[g_main_view_zoom_suspended_depth] = 0;

    if (saved_zoom <= 0)
        return;

    g_main_view_zoom_scale = saved_zoom;
    sdl_clamp_main_view_zoom_to_current_layout();
    sdl_apply_config_no_redraw();

    if (character_dungeon)
        Term_keypress(KTRL('R'));
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

int get_sdl_dice_roll_lock_ms(void)
{
    return config.dice_roll_lock_ms;
}

void set_sdl_dice_roll_lock_ms(int value)
{
    if (value < 0)
        value = 0;
    if (value > SDL_DICE_ROLL_TIMING_MAX_MS)
        value = SDL_DICE_ROLL_TIMING_MAX_MS;

    config.dice_roll_lock_ms = value;
}

int get_sdl_dice_roll_overlay_ms(void)
{
    return config.dice_roll_overlay_ms;
}

void set_sdl_dice_roll_overlay_ms(int value)
{
    if (value < 0)
        value = 0;
    if (value > SDL_DICE_ROLL_TIMING_MAX_MS)
        value = SDL_DICE_ROLL_TIMING_MAX_MS;

    config.dice_roll_overlay_ms = value;
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
    if (config.tiles == value && g_state.use_tiles == value)
        return;

    if (!g_state.renderer) {
        config.tiles = value;
        return;
    }

    if (!sdl_set_tiles_runtime(value))
        return;

    sdl_finish_tiles_mode_change();
}

void sdl_current_default_dimensions(int* out_w, int* out_h)
{
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();
    int width = window_pixels.w;
    int height = window_pixels.h;

    if (width <= 0 || height <= 0)
    {
        SDL_DisplayID primary = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* desktop_mode = primary
            ? SDL_GetDesktopDisplayMode(primary)
            : NULL;

        if (desktop_mode)
        {
            float density = (desktop_mode->pixel_density > 0.0f)
                ? desktop_mode->pixel_density
                : 1.0f;
            width = (int)(desktop_mode->w * density + 0.5f);
            height = (int)(desktop_mode->h * density + 0.5f);
        }
    }

    if (width <= 0)
        width = 1280;
    if (height <= 0)
        height = 720;

    if (out_w)
        *out_w = width;
    if (out_h)
        *out_h = height;
}

void sdl_show_interface_settings_reset_notice(void)
{
    SDL_MessageBoxButtonData button = {
        SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT
            | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
        0,
        "OK",
    };
    SDL_MessageBoxData messagebox = {
        .flags = SDL_MESSAGEBOX_INFORMATION,
        .window = g_state.window,
        .title = "Interface Settings Reset",
        .message = "Due to major interface changes, all interface settings were reset to defaults.",
        .numbuttons = 1,
        .buttons = &button,
        .colorScheme = NULL,
    };
    int button_id = 0;

    if (!g_state.window)
    {
        log_info("Due to major interface changes, all interface settings were reset to defaults.");
        return;
    }

    if (!SDL_ShowMessageBox(&messagebox, &button_id))
    {
        log_warn("SDL_ShowMessageBox failed for interface reset notice: %s",
            SDL_GetError());
    }
}

void sdl_reset_interface_settings_to_defaults_for_migration(void)
{
    static bool reset_done = false;
    int screen_w;
    int screen_h;
    bool old_fullscreen;
    bool old_tiles;
    bool desired_fullscreen;
    bool desired_tiles;

    if (reset_done)
    {
        sdl_config_load_app_options(config_file_path);
        return;
    }
    reset_done = true;

    sdl_current_default_dimensions(&screen_w, &screen_h);
    old_fullscreen = config.fullscreen;
    old_tiles = config.tiles;

    sdl_reset_config_to_resolution_defaults(screen_w, screen_h);
    sdl_ensure_default_pane_profiles_present(false);
    sdl_apply_screen_aspect_pane_default_profiles(screen_w, screen_h);
    sdl_apply_stored_pane_profile(config.min_terminal_mode);
    sdl_config_reset_app_options_to_defaults();

    desired_fullscreen = config.fullscreen;
    desired_tiles = config.tiles;
    config.fullscreen = old_fullscreen;
    config.tiles = old_tiles;

    set_sdl_fullscreen(desired_fullscreen);
    set_sdl_tiles(desired_tiles);

    g_hide_left_panel = false;
    g_left_panel_pane_expanded = config.left_panel_expanded_on_launch;
    sdl_ensure_touch_pane_config_present();
    sdl_touch_pane_ensure_main_panel_confirm();
    sdl_store_active_pane_profile(config.min_terminal_mode);

    if (g_state.window)
        sdl_apply_config();

    if (config_file_path[0] != '\0')
    {
        sdl_config_save(config_file_path, &config, g_pane_profiles,
            SDL_PANE_PROFILE_COUNT);
        log_info("Reset SDL/interface settings to defaults for 0.9.7 migration: %s",
            config_file_path);
    }

    sdl_show_interface_settings_reset_notice();
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

void set_sdl_pane_where(int index, int where)
{
    enum pane_placement placement = (enum pane_placement)where;
    bool is_touch_pane;
    bool is_overlay_menu;

    if (index < 0 || index >= pane_config_count)
        return;
    is_touch_pane = (pane_config[index].pane == PANE_TOUCH);
    is_overlay_menu = (pane_config[index].pane == PANE_OVERLAY_MENU);
    if (!pane_type_allows_placement(pane_config[index].pane, placement))
        placement = pane_first_allowed_placement(pane_config[index].pane);

    pane_config[index].where = placement;
    if (is_touch_pane) {
        sdl_touch_pane_cancel_press();
        sdl_touch_swipe_cancel();
    }
    if (is_overlay_menu)
        sdl_touch_top_panel_cancel_press();
}

bool get_sdl_pane_enabled(int index)
{
    if (index < 0 || index >= pane_config_count)
        return false;
    return pane_config[index].enabled;
}

/*
 * The canonical default config entry for the pane at a live index: the first
 * default entry of the same pane type.  Used by the per-row "Reset" buttons to
 * restore a pane's default placement/enabled/size.
 */
static int sdl_pane_default_config_index(int index)
{
    enum pane_type type;

    if (index < 0 || index >= pane_config_count)
        return -1;
    type = pane_config[index].pane;
    for (int i = 0; i < default_pane_config_count; i++) {
        if (default_pane_config[i].pane == type)
            return i;
    }
    return -1;
}

bool get_sdl_pane_default_enabled(int index)
{
    int di = sdl_pane_default_config_index(index);
    if (di < 0)
        return get_sdl_pane_enabled(index);
    return default_pane_config[di].enabled;
}

int get_sdl_pane_default_where(int index)
{
    int di = sdl_pane_default_config_index(index);
    if (di < 0)
        return get_sdl_pane_where(index);
    return (int)default_pane_config[di].where;
}

int get_sdl_pane_default_rows(int index)
{
    int di = sdl_pane_default_config_index(index);
    if (di < 0)
        return 0;
    return default_pane_config[di].rect.rows;
}

int get_sdl_pane_default_cols(int index)
{
    int di = sdl_pane_default_config_index(index);
    if (di < 0)
        return 0;
    return default_pane_config[di].rect.cols;
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

int sdl_pane_current_size(int index, bool want_rows)
{
    enum pane_type type;
    enum pane_placement where;
    int configured;

    if (index < 0 || index >= pane_config_count)
        return 0;

    type = pane_config[index].pane;
    if (type == PANE_LEFT_PANEL) {
        sdl_left_panel_metrics metrics;

        if (sdl_left_panel_metrics_for_view(&g_views[PANE_MAIN], &metrics))
            return want_rows ? metrics.panel_rows : metrics.content_cols;

        return 0;
    }
    if (type <= PANE_MAIN || type >= PANE_MAX)
        return 0;

    if ((int)type < MAX_TERM_DATA && g_views[type].term_ready
        && g_pane_rects[type].w > 0 && g_pane_rects[type].h > 0)
    {
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
    if (pane_config[index].pane == PANE_LEFT_PANEL) {
        pane_config[index].rect.rows = 0;
        return;
    }
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
    if (pane_config[index].pane == PANE_LEFT_PANEL) {
        pane_config[index].rect.cols = 0;
        return;
    }
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
    enum pane_type type;
    bool is_touch_pane;
    bool is_overlay_menu;

    if (index < 0 || index >= pane_config_count)
        return;
    if (pane_config[index].pane == PANE_LEFT_PANEL) {
        pane_config[index].enabled = true;
        return;
    }
    type = pane_config[index].pane;
    is_touch_pane = (type == PANE_TOUCH);
    is_overlay_menu = (type == PANE_OVERLAY_MENU);
    pane_config[index].enabled = enabled;
    if (enabled && (type == PANE_LOG || type == PANE_ROLLS)) {
        enum pane_type other = (type == PANE_LOG) ? PANE_ROLLS : PANE_LOG;

        for (int i = 0; i < pane_config_count; i++) {
            if (i != index && pane_config[i].pane == other)
                pane_config[i].enabled = false;
        }
    }
    if (is_touch_pane) {
        g_touch_pane_mobile_open = config.touch_pane_default_open;
        sdl_touch_pane_cancel_press();
        sdl_touch_swipe_cancel();
    }
    if (is_overlay_menu) {
        if (!enabled)
            sdl_touch_top_panel_set_open(false);
        sdl_touch_top_panel_cancel_press();
    }
}

bool sdl_pane_group_matches(enum pane_placement where, bool side)
{
    if (pane_placement_is_overlay(where))
        return false;

    return side ? pane_placement_is_side(where)
                : pane_placement_is_bottom(where);
}

bool sdl_default_pane_enabled_for_group(enum pane_type type, bool side,
    bool* found)
{
    if (found)
        *found = false;

    for (int i = 0; i < default_pane_config_count; i++) {
        if (default_pane_config[i].pane != type)
            continue;
        if (!sdl_pane_group_matches(default_pane_config[i].where, side))
            continue;

        if (found)
            *found = true;
        return default_pane_config[i].enabled;
    }

    return false;
}

void sdl_enable_default_panes_for_empty_group(bool side)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_TOUCH
            || pane_config[i].pane == PANE_LEFT_PANEL
            || pane_config[i].pane == PANE_STATUS
            || pane_config[i].pane == PANE_DEPTH
            || pane_config[i].pane == PANE_COMBAT
            || (pane_config[i].pane == PANE_ROLLS
                && pane_placement_is_overlay(pane_config[i].where))
            || pane_config[i].pane == PANE_DESCRIPTION
            || pane_config[i].pane == PANE_OVERLAY_MENU)
        {
            continue;
        }
        if (!sdl_pane_group_matches(pane_config[i].where, side))
            continue;
        if (pane_config[i].enabled)
            return;
    }

    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_TOUCH
            || pane_config[i].pane == PANE_LEFT_PANEL
            || pane_config[i].pane == PANE_STATUS
            || pane_config[i].pane == PANE_DEPTH
            || pane_config[i].pane == PANE_COMBAT
            || (pane_config[i].pane == PANE_ROLLS
                && pane_placement_is_overlay(pane_config[i].where))
            || pane_config[i].pane == PANE_DESCRIPTION
            || pane_config[i].pane == PANE_OVERLAY_MENU)
        {
            continue;
        }
        if (!sdl_pane_group_matches(pane_config[i].where, side))
            continue;
        if (sdl_default_pane_enabled_for_group(pane_config[i].pane, side, NULL))
            pane_config[i].enabled = true;
    }
}

bool get_sdl_enable_right_panes(void)
{
    return config.enable_right_panes;
}

void set_sdl_enable_right_panes(bool value)
{
    config.enable_right_panes = value;

    if (value)
        sdl_enable_default_panes_for_empty_group(true);
}

bool get_sdl_enable_bottom_panes(void)
{
    return config.enable_bottom_panes;
}

void set_sdl_enable_bottom_panes(bool value)
{
    config.enable_bottom_panes = value;

    if (value)
        sdl_enable_default_panes_for_empty_group(false);
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

void sdl_push_saved_screen_left_panel_pane(void)
{
    g_saved_screen_left_panel_pane_depth++;
    sdl_update_left_panel_pane_rect();
    g_state.need_present = true;
}

void sdl_pop_saved_screen_left_panel_pane(void)
{
    if (g_saved_screen_left_panel_pane_depth > 0)
        g_saved_screen_left_panel_pane_depth--;
    sdl_update_left_panel_pane_rect();
    g_state.need_present = true;
}

bool get_sdl_left_panel_expanded_on_launch(void)
{
    return config.left_panel_expanded_on_launch;
}

void set_sdl_left_panel_expanded_on_launch(bool value)
{
    config.left_panel_expanded_on_launch = value;
    sdl_left_panel_pane_set_expanded(value);
}

int get_sdl_left_panel_compact_mode(void)
{
    return sdl_left_panel_compact_mode_normalized(
        config.left_panel_compact_mode);
}

void set_sdl_left_panel_compact_mode(int mode)
{
    mode = sdl_left_panel_compact_mode_normalized(mode);
    if (config.left_panel_compact_mode == mode)
        return;

    config.left_panel_compact_mode = mode;
    sdl_update_left_panel_pane_rect();
    g_state.need_present = true;
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

void sdl_gamepad_load_default_bindings(void)
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

void sdl_mouse_load_default_settings(void)
{
    if (g_default_mouse_settings_ready)
        return;

    struct sdl_config defaults;
    sdl_config_set_defaults(&defaults);
    sdl_apply_startup_input_defaults_to_config(&defaults,
        g_startup_device_class);
    g_default_mouse_enabled = defaults.mouse_enabled;
    g_default_mouse_movement_mode = defaults.mouse_movement_mode;
    g_default_mouse_settings_ready = true;
}

void sdl_touch_pane_load_default_bindings(void)
{
    if (g_default_touch_pane_bindings_ready)
        return;

    struct sdl_config defaults;
    sdl_config_set_defaults(&defaults);
    sdl_apply_startup_input_defaults_to_config(&defaults,
        g_startup_device_class);
    memcpy(g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_MAIN], defaults.touch_pane_bindings,
        sizeof(defaults.touch_pane_bindings));
    memcpy(g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_SECOND], defaults.touch_pane_second_bindings,
        sizeof(defaults.touch_pane_second_bindings));
    memcpy(g_default_touch_pane_panel_names, defaults.touch_pane_panel_names,
        sizeof(g_default_touch_pane_panel_names));
    memcpy(g_default_touch_menu_command_enabled, defaults.touch_menu_command_enabled,
        sizeof(g_default_touch_menu_command_enabled));
    g_default_touch_profile = defaults.touch_profile;
    g_default_touch_pane_default_open = defaults.touch_pane_default_open;
    g_default_touch_pane_key_labels_visible =
        defaults.touch_pane_key_labels_visible;
    g_default_touch_pane_inventory_equipment_cycle =
        defaults.touch_pane_inventory_equipment_cycle;
    g_default_touch_movement_mode = defaults.touch_movement_mode;
    g_default_touch_round_movement_enabled =
        defaults.touch_round_movement_enabled;
    g_default_touch_zone_overlay_mode = defaults.touch_zone_overlay_mode;
    memcpy(g_default_touch_zone_center_bindings,
        defaults.touch_zone_center_bindings,
        sizeof(g_default_touch_zone_center_bindings));
    g_default_touch_corner_up_down_side =
        defaults.touch_corner_up_down_side;
    memcpy(g_default_touch_corner_action_bindings,
        defaults.touch_corner_action_bindings,
        sizeof(g_default_touch_corner_action_bindings));
    g_default_touch_top_panel_mode = defaults.touch_top_panel_mode;
    g_default_touch_top_panel_default_open =
        defaults.touch_top_panel_default_open;
    g_default_touch_top_panel_button_count =
        defaults.touch_top_panel_button_count;
    g_default_touch_top_panel_tile_scale =
        defaults.touch_top_panel_tile_scale;
    memcpy(g_default_touch_top_panel_bindings,
        defaults.touch_top_panel_bindings,
        sizeof(g_default_touch_top_panel_bindings));
    memcpy(g_default_touch_top_panel_long_bindings,
        defaults.touch_top_panel_long_bindings,
        sizeof(g_default_touch_top_panel_long_bindings));
    g_default_touch_thumb_enabled = defaults.touch_thumb_enabled;
    memcpy(g_default_touch_thumb_bindings,
        defaults.touch_thumb_bindings,
        sizeof(g_default_touch_thumb_bindings));
    memcpy(g_default_touch_thumb_long_bindings,
        defaults.touch_thumb_long_bindings,
        sizeof(g_default_touch_thumb_long_bindings));
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
    if (!config.gamepad_auto_mode)
        return false;

    return g_gamepad_auto_ui || (g_gamepad_state.pad_count > 0);
}

bool sdl_menu_letters_enabled(void)
{
    if (g_startup_device_class != SDL_STARTUP_DEVICE_DESKTOP)
        return false;

    return !steamdeck_controls_active();
}

bool sdl_touch_only_device_active(void)
{
    return sdl_touch_only_mobile_device_active();
}

bool portable_controls_active(void)
{
#if defined(SIL_USE_LOCAL_DATA) || defined(__ANDROID__) || defined(SIL_IOS)
    /* Portable builds and mobile platforms accept touch-friendly confirm shortcuts.
     * Controller button labels are gated separately by steamdeck_controls_active(). */
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
        sdl_gamepad_clear_pending_confirm();
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
        sdl_touch_top_panel_cancel_press();
        sdl_touch_round_cancel_press();
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

static void sdl_gamepad_toggle_defaults(struct sdl_config* defaults)
{
    sdl_config_set_defaults(defaults);
    sdl_apply_startup_input_defaults_to_config(defaults,
        g_startup_device_class);
}

bool get_sdl_gamepad_default_enabled(void)
{
    struct sdl_config defaults;
    sdl_gamepad_toggle_defaults(&defaults);
    return defaults.gamepad_enabled;
}

bool get_sdl_gamepad_default_auto_mode(void)
{
    struct sdl_config defaults;
    sdl_gamepad_toggle_defaults(&defaults);
    return defaults.gamepad_auto_mode;
}

bool get_sdl_steamdeck_default_mode(void)
{
    struct sdl_config defaults;
    sdl_gamepad_toggle_defaults(&defaults);
    return defaults.steamdeck_mode;
}

bool get_sdl_steamdeck_default_inv_equip_same_button_cycle(void)
{
    struct sdl_config defaults;
    sdl_gamepad_toggle_defaults(&defaults);
    return defaults.steamdeck_inv_equip_same_button_cycle;
}

bool get_sdl_gamepad_default_use_dpad(void)
{
    struct sdl_config defaults;
    sdl_gamepad_toggle_defaults(&defaults);
    return defaults.gamepad_use_dpad;
}

bool get_sdl_gamepad_default_use_left_stick(void)
{
    struct sdl_config defaults;
    sdl_gamepad_toggle_defaults(&defaults);
    return defaults.gamepad_use_left_stick;
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

int sdl_touch_profile_normalized(int profile)
{
    if (profile >= SDL_TOUCH_PROFILE_TOUCH_PANE
        && profile < SDL_TOUCH_PROFILE_COUNT)
    {
        return profile;
    }

    return SDL_TOUCH_PROFILE_TOUCH_PANE;
}

int get_sdl_mouse_movement_mode(void)
{
    return sdl_mouse_movement_normalized_mode(config.mouse_movement_mode);
}

void set_sdl_mouse_movement_mode(int mode)
{
    config.mouse_movement_mode = sdl_mouse_movement_normalized_mode(mode);
    if (config.mouse_movement_mode == SDL_MOUSE_MOVEMENT_OFF)
        sdl_mouse_path_cancel();
}

int get_sdl_mouse_movement_default_mode(void)
{
    sdl_mouse_load_default_settings();
    return sdl_mouse_movement_normalized_mode(g_default_mouse_movement_mode);
}

bool get_sdl_mouse_enabled(void)
{
    return config.mouse_enabled;
}

void set_sdl_mouse_enabled(bool enabled)
{
    if (config.mouse_enabled == enabled)
        return;

    config.mouse_enabled = enabled;
    if (!enabled) {
        sdl_mouse_path_cancel();
        sdl_pointer_attack_clear_hover();
    }
    sdl_update_cursor_visibility();
}

bool get_sdl_mouse_default_enabled(void)
{
    sdl_mouse_load_default_settings();
    return g_default_mouse_enabled;
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

bool get_sdl_touch_pane_enabled(void)
{
    return sdl_touch_pane_is_config_enabled();
}

void set_sdl_touch_pane_enabled(bool value)
{
    sdl_ensure_touch_pane_config_present();

    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane != PANE_TOUCH)
            continue;

        if (pane_config[i].enabled == value)
            return;

        pane_config[i].enabled = value;
        g_touch_pane_mobile_open = config.touch_pane_default_open;
        sdl_touch_pane_cancel_press();
        sdl_touch_swipe_cancel();
        return;
    }
}

bool get_sdl_touch_pane_default_open(void)
{
    return config.touch_pane_default_open;
}

void set_sdl_touch_pane_default_open(bool value)
{
    config.touch_pane_default_open = value;
    if (!sdl_touch_pane_uses_mobile_toggle())
        return;

    if (g_touch_pane_mobile_open == value)
        return;

    g_touch_pane_mobile_open = value;
    if (value) {
        sdl_touch_zone_cancel_press();
    } else {
        sdl_touch_pane_cancel_press();
    }

    if (g_state.window) {
        sdl_resize_for_current_layout();
        sdl_touch_pane_refresh_after_layout_toggle();
    } else {
        g_state.need_present = true;
    }
}

bool get_sdl_touch_pane_default_open_default(void)
{
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_pane_default_open;
}

bool get_sdl_touch_pane_key_labels_visible(void)
{
    return config.touch_pane_key_labels_visible;
}

void set_sdl_touch_pane_key_labels_visible(bool value)
{
    config.touch_pane_key_labels_visible = value;
    g_state.need_present = true;
}

bool get_sdl_touch_pane_key_labels_default_visible(void)
{
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_pane_key_labels_visible;
}

bool get_sdl_touch_pane_inventory_equipment_cycle(void)
{
    return config.touch_pane_inventory_equipment_cycle;
}

void set_sdl_touch_pane_inventory_equipment_cycle(bool value)
{
    config.touch_pane_inventory_equipment_cycle = value;
}

bool get_sdl_touch_pane_inventory_equipment_default_cycle(void)
{
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_pane_inventory_equipment_cycle;
}

int get_sdl_touch_pane_placement(void)
{
    return sdl_touch_pane_is_left_placement()
        ? SDL_TOUCH_PANE_PLACEMENT_LEFT
        : SDL_TOUCH_PANE_PLACEMENT_RIGHT;
}

void set_sdl_touch_pane_placement(int placement)
{
    enum pane_placement where = (placement == SDL_TOUCH_PANE_PLACEMENT_LEFT)
        ? PLACE_DOUBLE_LEFT
        : PLACE_DOUBLE_RIGHT;

    sdl_ensure_touch_pane_config_present();

    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane != PANE_TOUCH)
            continue;

        if (pane_config[i].where == where)
            return;

        pane_config[i].where = where;
        sdl_touch_pane_cancel_press();
        sdl_touch_swipe_cancel();
        return;
    }
}

int sdl_touch_movement_normalized_mode(int mode)
{
    if (mode == SDL_TOUCH_MOVEMENT_OFF
        || mode == SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY)
    {
        return mode;
    }

    return SDL_TOUCH_MOVEMENT_ON;
}

int sdl_touch_menu_category_normalized(int category)
{
    if (category < 0 || category >= SDL_TOUCH_MENU_CATEGORY_COUNT)
        return SDL_TOUCH_MENU_CATEGORY_OTHER;

    return category;
}

bool get_sdl_touch_menu_commands_enabled(int category)
{
    category = sdl_touch_menu_category_normalized(category);
    return config.touch_menu_command_enabled[category];
}

void set_sdl_touch_menu_commands_enabled(int category, bool value)
{
    category = sdl_touch_menu_category_normalized(category);
    config.touch_menu_command_enabled[category] = value;
    if (!value)
        sdl_menu_touch_cancel();
}

bool get_sdl_touch_menu_commands_default_enabled(int category)
{
    category = sdl_touch_menu_category_normalized(category);
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_menu_command_enabled[category];
}

int get_sdl_touch_profile(void)
{
    return sdl_touch_profile_normalized(config.touch_profile);
}

void set_sdl_touch_profile(int profile)
{
    config.touch_profile = sdl_touch_profile_normalized(profile);
}

int get_sdl_touch_profile_default(void)
{
    sdl_touch_pane_load_default_bindings();
    return sdl_touch_profile_normalized(g_default_touch_profile);
}

void sdl_touch_cancel_all_inputs(void)
{
    sdl_touch_pane_cancel_press();
    sdl_touch_swipe_cancel();
    sdl_touch_zone_cancel_press();
    sdl_touch_top_panel_cancel_press();
    sdl_touch_round_cancel_press();
    sdl_character_panel_cancel_press();
    sdl_map_touch_cancel_press();
    sdl_main_map_cancel_drag();
    sdl_pointer_attack_cancel_touch_press();
}

void sdl_touch_apply_profile(int profile)
{
    bool pane_enabled = true;
    int pane_placement = SDL_TOUCH_PANE_PLACEMENT_RIGHT;
    bool pane_default_open = true;
    bool top_panel_default_open = false;
    int top_panel_mode = SDL_TOUCH_TOP_PANEL_MODE_SHORT;
    int top_panel_button_count = SDL_TOUCH_TOP_PANEL_BUTTON_COUNT_DEFAULT;
    int movement_mode = SDL_TOUCH_MOVEMENT_ON;
    bool round_enabled = false;
    int zone_overlay_mode = SDL_TOUCH_ZONE_OVERLAY_MARKERS;

    profile = sdl_touch_profile_normalized(profile);

    switch (profile) {
    case SDL_TOUCH_PROFILE_CORNERS:
        pane_default_open = false;
        top_panel_default_open = true;
        zone_overlay_mode = SDL_TOUCH_ZONE_OVERLAY_MARKERS;
        break;
    case SDL_TOUCH_PROFILE_ROUND_WHEEL:
        pane_enabled = sdl_touch_only_mobile_device_active();
        pane_default_open = false;
        top_panel_default_open = true;
        top_panel_mode = SDL_TOUCH_TOP_PANEL_MODE_LONG;
        top_panel_button_count = SDL_TOUCH_TOP_PANEL_BUTTON_COUNT;
        movement_mode = SDL_TOUCH_MOVEMENT_ON;
        round_enabled = true;
        zone_overlay_mode = SDL_TOUCH_ZONE_OVERLAY_OFF;
        break;
    case SDL_TOUCH_PROFILE_TOUCH_PANE:
    default:
        pane_default_open = true;
        top_panel_default_open = false;
        zone_overlay_mode = SDL_TOUCH_ZONE_OVERLAY_MARKERS;
        break;
    }

    config.touch_profile = profile;
    for (int i = 0; i < SDL_TOUCH_MENU_CATEGORY_COUNT; i++)
        config.touch_menu_command_enabled[i] = true;
    config.touch_pane_default_open = pane_default_open;
    config.touch_top_panel_mode = top_panel_mode;
    config.touch_top_panel_button_count =
        sdl_touch_top_panel_button_count_normalized(top_panel_button_count);
    config.touch_top_panel_default_open = top_panel_default_open;
    config.touch_movement_mode = sdl_touch_movement_normalized_mode(movement_mode);
    config.touch_round_movement_enabled = round_enabled;
    config.touch_zone_overlay_mode =
        sdl_touch_zone_overlay_mode_normalized(zone_overlay_mode);
    config.touch_corner_up_down_side = SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
    config.touch_swipe_enabled = true;

    set_sdl_touch_pane_enabled(pane_enabled);
    set_sdl_touch_pane_placement(pane_placement);
    g_touch_pane_mobile_open = pane_default_open;
    g_touch_top_panel_open = top_panel_default_open;
    sdl_touch_cancel_all_inputs();

    if (g_state.window)
        sdl_apply_config();
    else
        g_state.need_present = true;
}

int get_sdl_touch_movement_mode(void)
{
    return sdl_touch_movement_normalized_mode(config.touch_movement_mode);
}

void set_sdl_touch_movement_mode(int mode)
{
    config.touch_movement_mode = sdl_touch_movement_normalized_mode(mode);
    sdl_map_touch_cancel_press();
}

int get_sdl_touch_movement_default_mode(void)
{
    sdl_touch_pane_load_default_bindings();
    return sdl_touch_movement_normalized_mode(g_default_touch_movement_mode);
}

bool get_sdl_touch_round_movement_enabled(void)
{
    return config.touch_round_movement_enabled;
}

void set_sdl_touch_round_movement_enabled(bool value)
{
    if (config.touch_round_movement_enabled == value)
        return;

    config.touch_round_movement_enabled = value;
    sdl_touch_round_cancel_press();
    if (value) {
        sdl_touch_zone_cancel_press();
        sdl_touch_top_panel_cancel_press();
        sdl_touch_swipe_cancel();
        sdl_map_touch_cancel_press();
        sdl_pointer_attack_cancel_touch_press();
    }
    if (sdl_touch_pane_uses_mobile_toggle() && g_state.window) {
        sdl_resize_for_current_layout();
        sdl_touch_pane_refresh_after_layout_toggle();
        return;
    }
    g_state.need_present = true;
}

bool get_sdl_touch_round_movement_default_enabled(void)
{
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_round_movement_enabled;
}

int get_sdl_touch_zone_overlay_mode(void)
{
    return sdl_touch_zone_overlay_mode_normalized(
        config.touch_zone_overlay_mode);
}

void set_sdl_touch_zone_overlay_mode(int mode)
{
    config.touch_zone_overlay_mode =
        sdl_touch_zone_overlay_mode_normalized(mode);
    g_state.need_present = true;
}

int get_sdl_touch_zone_overlay_default_mode(void)
{
    sdl_touch_pane_load_default_bindings();
    return sdl_touch_zone_overlay_mode_normalized(
        g_default_touch_zone_overlay_mode);
}

int get_sdl_touch_zone_center_binding(int index)
{
    if (index < 0 || index >= SDL_TOUCH_ZONE_CENTER_BINDING_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.touch_zone_center_bindings[index];
}

void set_sdl_touch_zone_center_binding(int index, int binding)
{
    if (index < 0 || index >= SDL_TOUCH_ZONE_CENTER_BINDING_COUNT)
        return;
    config.touch_zone_center_bindings[index] = binding;
    g_state.need_present = true;
}

int get_sdl_touch_zone_center_default_binding(int index)
{
    if (index < 0 || index >= SDL_TOUCH_ZONE_CENTER_BINDING_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_zone_center_bindings[index];
}

int get_sdl_touch_corner_up_down_side(void)
{
    return sdl_touch_corner_up_down_side_normalized(
        config.touch_corner_up_down_side);
}

void set_sdl_touch_corner_up_down_side(int side)
{
    side = sdl_touch_corner_up_down_side_normalized(side);
    if (config.touch_corner_up_down_side == side)
        return;

    config.touch_corner_up_down_side = side;
    sdl_touch_zone_cancel_press();
    g_state.need_present = true;
}

int get_sdl_touch_corner_up_down_default_side(void)
{
    sdl_touch_pane_load_default_bindings();
    return sdl_touch_corner_up_down_side_normalized(
        g_default_touch_corner_up_down_side);
}

int get_sdl_touch_corner_action_binding(int index)
{
    if (index < 0 || index >= SDL_TOUCH_CORNER_ACTION_BINDING_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.touch_corner_action_bindings[index];
}

void set_sdl_touch_corner_action_binding(int index, int binding)
{
    if (index < 0 || index >= SDL_TOUCH_CORNER_ACTION_BINDING_COUNT)
        return;
    config.touch_corner_action_bindings[index] = binding;
    g_state.need_present = true;
}

int get_sdl_touch_corner_action_default_binding(int index)
{
    if (index < 0 || index >= SDL_TOUCH_CORNER_ACTION_BINDING_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_corner_action_bindings[index];
}

int get_sdl_touch_top_panel_mode(void)
{
    return sdl_touch_top_panel_mode_normalized(config.touch_top_panel_mode);
}

void set_sdl_touch_top_panel_mode(int mode)
{
    mode = sdl_touch_top_panel_mode_normalized(mode);
    if (config.touch_top_panel_mode == mode)
        return;

    config.touch_top_panel_mode = mode;
    config.touch_top_panel_button_count =
        (mode == SDL_TOUCH_TOP_PANEL_MODE_LONG)
            ? SDL_TOUCH_TOP_PANEL_BUTTON_COUNT
            : SDL_TOUCH_TOP_PANEL_BUTTON_COUNT_DEFAULT;
    sdl_touch_top_panel_cancel_press();
    g_state.need_present = true;
}

int get_sdl_touch_top_panel_default_mode(void)
{
    sdl_touch_pane_load_default_bindings();
    return sdl_touch_top_panel_mode_normalized(
        g_default_touch_top_panel_mode);
}

bool get_sdl_touch_top_panel_default_open(void)
{
    return config.touch_top_panel_default_open;
}

void set_sdl_touch_top_panel_default_open(bool value)
{
    config.touch_top_panel_default_open = value;
    sdl_touch_top_panel_set_open(value);
}

bool get_sdl_touch_top_panel_default_open_default(void)
{
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_top_panel_default_open;
}

int get_sdl_touch_top_panel_button_count(void)
{
    return sdl_touch_top_panel_button_count_normalized(
        config.touch_top_panel_button_count);
}

void set_sdl_touch_top_panel_button_count(int count)
{
    count = sdl_touch_top_panel_button_count_normalized(count);
    if (config.touch_top_panel_button_count == count)
        return;

    config.touch_top_panel_button_count = count;
    config.touch_top_panel_mode =
        (count > SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT)
            ? SDL_TOUCH_TOP_PANEL_MODE_LONG
            : SDL_TOUCH_TOP_PANEL_MODE_SHORT;
    sdl_touch_top_panel_cancel_press();
    g_state.need_present = true;
}

int get_sdl_touch_top_panel_default_button_count(void)
{
    sdl_touch_pane_load_default_bindings();
    return sdl_touch_top_panel_button_count_normalized(
        g_default_touch_top_panel_button_count);
}

int get_sdl_touch_top_panel_tile_scale(void)
{
    return sdl_touch_top_panel_tile_scale_normalized(
        config.touch_top_panel_tile_scale);
}

void set_sdl_touch_top_panel_tile_scale(int scale)
{
    scale = sdl_touch_top_panel_tile_scale_normalized(scale);
    if (config.touch_top_panel_tile_scale == scale)
        return;

    config.touch_top_panel_tile_scale = scale;
    sdl_touch_top_panel_cancel_press();
    g_state.need_present = true;
}

int get_sdl_touch_top_panel_default_tile_scale(void)
{
    sdl_touch_pane_load_default_bindings();
    return sdl_touch_top_panel_tile_scale_normalized(
        g_default_touch_top_panel_tile_scale);
}

int get_sdl_touch_top_panel_binding(int index, bool long_press)
{
    if (index < 0 || index >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    return long_press ? config.touch_top_panel_long_bindings[index]
                      : config.touch_top_panel_bindings[index];
}

void set_sdl_touch_top_panel_binding(int index, bool long_press, int binding)
{
    if (index < 0 || index >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return;

    if (long_press)
        config.touch_top_panel_long_bindings[index] = binding;
    else
        config.touch_top_panel_bindings[index] = binding;
    g_state.need_present = true;
}

int get_sdl_touch_top_panel_default_binding(int index, bool long_press)
{
    if (index < 0 || index >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_touch_pane_load_default_bindings();
    return long_press ? g_default_touch_top_panel_long_bindings[index]
                      : g_default_touch_top_panel_bindings[index];
}

bool get_sdl_touch_thumb_enabled(void)
{
    return config.touch_thumb_enabled;
}

void set_sdl_touch_thumb_enabled(bool value)
{
    if (config.touch_thumb_enabled == value)
        return;
    config.touch_thumb_enabled = value;
    if (!value)
        sdl_touch_thumb_cancel_press();
    g_state.need_present = true;
}

bool get_sdl_touch_thumb_default_enabled(void)
{
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_thumb_enabled;
}

int get_sdl_touch_thumb_binding(int index, bool long_press)
{
    if (index < 0 || index >= SDL_TOUCH_THUMB_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    return long_press ? config.touch_thumb_long_bindings[index]
                      : config.touch_thumb_bindings[index];
}

void set_sdl_touch_thumb_binding(int index, bool long_press, int binding)
{
    if (index < 0 || index >= SDL_TOUCH_THUMB_BUTTON_COUNT)
        return;

    if (long_press)
        config.touch_thumb_long_bindings[index] = binding;
    else
        config.touch_thumb_bindings[index] = binding;
    g_state.need_present = true;
}

int get_sdl_touch_thumb_default_binding(int index, bool long_press)
{
    if (index < 0 || index >= SDL_TOUCH_THUMB_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_touch_pane_load_default_bindings();
    return long_press ? g_default_touch_thumb_long_bindings[index]
                      : g_default_touch_thumb_bindings[index];
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
    if (dir < 0 || dir >= TOUCH_SWIPE_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.touch_swipe_bindings[dir];
}

void set_sdl_touch_swipe_binding(int dir, int binding)
{
    if (dir < 0 || dir >= TOUCH_SWIPE_DIR_COUNT)
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
    if (dir < 0 || dir >= TOUCH_SWIPE_DIR_COUNT)
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
    g_touch_pane_mobile_open = sdl_touch_pane_uses_mobile_toggle();
    sdl_touch_pane_cancel_press();
    sdl_touch_swipe_cancel();
    sdl_touch_zone_cancel_press();
    sdl_touch_top_panel_cancel_press();
    sdl_touch_round_cancel_press();
    sdl_config_set_default_touch_pane_bindings(&config);
    sdl_config_clear_touch_pane_labels(&config);
    g_touch_pane_mobile_open = config.touch_pane_default_open;
    g_touch_top_panel_open = config.touch_top_panel_default_open;
    sdl_touch_pane_ensure_main_panel_confirm();
}

void sdl_touch_pane_begin_yes_no_prompt_impl(cptr prompt,
    sdl_touch_yes_no_prompt_placement placement)
{
    size_t len;

    SDL_strlcpy(g_touch_pane_yes_no_prompt_text,
        (prompt && prompt[0]) ? prompt : "Are you sure?",
        sizeof(g_touch_pane_yes_no_prompt_text));
    len = strlen(g_touch_pane_yes_no_prompt_text);
    while (len > 0
        && isspace((unsigned char)g_touch_pane_yes_no_prompt_text[len - 1]))
    {
        g_touch_pane_yes_no_prompt_text[--len] = '\0';
    }

    sdl_touch_pane_cancel_press();
    sdl_touch_swipe_cancel();
    sdl_touch_zone_cancel_press();
    sdl_touch_top_panel_cancel_press();
    sdl_touch_round_cancel_press();
    sdl_menu_touch_cancel();
    g_touch_pane_yes_no_prompt_placement = placement;
    g_touch_pane_yes_no_prompt_hover = SDL_TOUCH_YES_NO_HOVER_NONE;
    g_touch_pane_yes_no_prompt_anchor_active = false;
    g_touch_pane_yes_no_prompt_anchor_y = 0;
    g_touch_pane_yes_no_prompt_anchor_x = 0;
    g_touch_pane_yes_no_prompt_active = true;
    g_state.need_present = true;
}

void sdl_touch_pane_begin_yes_no_prompt(cptr prompt)
{
    sdl_touch_pane_begin_yes_no_prompt_impl(prompt,
        SDL_TOUCH_YES_NO_PLACEMENT_CENTER);
}

void sdl_touch_pane_begin_yes_no_prompt_lower(cptr prompt)
{
    sdl_touch_pane_begin_yes_no_prompt_impl(prompt,
        SDL_TOUCH_YES_NO_PLACEMENT_LOWER);
}

/* Local yes/no question: same modal panel, but spawned next to the map
 * grid the question is about (the trap, door, stair, ...) instead of the
 * screen centre. */
void sdl_touch_pane_begin_yes_no_prompt_near(cptr prompt, int map_y,
    int map_x)
{
    sdl_touch_pane_begin_yes_no_prompt_impl(prompt,
        SDL_TOUCH_YES_NO_PLACEMENT_CENTER);
    g_touch_pane_yes_no_prompt_anchor_active = true;
    g_touch_pane_yes_no_prompt_anchor_y = map_y;
    g_touch_pane_yes_no_prompt_anchor_x = map_x;
}

void sdl_touch_pane_end_yes_no_prompt(void)
{
    if (!g_touch_pane_yes_no_prompt_active)
        return;

    sdl_touch_pane_clear_yes_no_prompt();
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

bool sdl_keyboard_capture_begin(void)
{
    g_keyboard_capture_ready = false;
    g_keyboard_capture_active = true;
    g_keyboard_capture_scancode = SDL_SCANCODE_UNKNOWN;
    g_keyboard_capture_modifiers = 0;
    g_keyboard_capture_arm_time = SDL_GetTicksNS()
        + ((Uint64)GAMEPAD_CAPTURE_ARM_DELAY_MS * 1000000ULL);
    return true;
}

void sdl_keyboard_capture_cancel(void)
{
    g_keyboard_capture_active = false;
    g_keyboard_capture_ready = false;
    g_keyboard_capture_scancode = SDL_SCANCODE_UNKNOWN;
    g_keyboard_capture_modifiers = 0;
    g_keyboard_capture_arm_time = 0;
}

bool sdl_keyboard_capture_poll(SDL_Scancode* out_scancode,
    u16b* out_modifiers)
{
    if (!g_keyboard_capture_ready)
        return false;

    if (out_scancode)
        *out_scancode = g_keyboard_capture_scancode;
    if (out_modifiers)
        *out_modifiers = g_keyboard_capture_modifiers;

    g_keyboard_capture_ready = false;
    g_keyboard_capture_active = false;
    g_keyboard_capture_scancode = SDL_SCANCODE_UNKNOWN;
    g_keyboard_capture_modifiers = 0;
    g_keyboard_capture_arm_time = 0;
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
    int max_scale;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    max_scale = sdl_max_scale_for_layout(&screen, config.min_terminal_mode);
    if (max_scale < sdl_main_view_scale_floor())
        max_scale = sdl_main_view_scale_floor();

    log_debug("get_sdl_max_scale: layout=(%d,%d %dx%d) min=%dx%d (%s) max_scale=%d",
              screen.x, screen.y, screen.w, screen.h,
              sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows(),
              sdl_min_terminal_mode_name(config.min_terminal_mode), max_scale);
    
    return max_scale;
}

int get_sdl_max_main_view_zoom_scale(void)
{
    if (!g_state.window) {
        return 10;
    }

    SDL_Rect screen;
    int max_scale;
    int min_cols;
    int min_rows;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    max_scale = sdl_max_main_view_zoom_scale_for_layout(&screen,
        config.min_terminal_mode);
    min_cols = sdl_main_view_terminal_cols_for_map_squares(
        SDL_MAIN_VIEW_ZOOM_MIN_MAP_SQUARES);
    min_rows = sdl_main_view_terminal_rows_for_map_squares(
        SDL_MAIN_VIEW_ZOOM_MIN_MAP_SQUARES);

    log_debug("get_sdl_max_main_view_zoom_scale: layout=(%d,%d %dx%d) min_visible_axis=%d map squares term_min=%dx%d max_scale=%d",
              screen.x, screen.y, screen.w, screen.h,
              SDL_MAIN_VIEW_ZOOM_MIN_MAP_SQUARES, min_cols, min_rows,
              max_scale);

    return max_scale;
}

int get_sdl_min_main_view_zoom_scale(void)
{
    if (!g_state.window)
        return sdl_main_view_scale_floor();

    SDL_Rect screen;
    int min_scale;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    min_scale = sdl_min_main_view_zoom_scale_for_layout(&screen,
        config.min_terminal_mode);

    log_debug("get_sdl_min_main_view_zoom_scale: layout=(%d,%d %dx%d) min_scale=%d",
              screen.x, screen.y, screen.w, screen.h, min_scale);

    return min_scale;
}

void sdl_clamp_main_view_zoom_to_current_layout(void)
{
    int min_scale;
    int max_scale;
    int configured_scale;

    if (g_main_view_zoom_scale <= 0)
        return;

    min_scale = get_sdl_min_main_view_zoom_scale();
    max_scale = get_sdl_max_main_view_zoom_scale();
    if (min_scale < SDL_MAIN_VIEW_MIN_SCALE)
        min_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale < SDL_MAIN_VIEW_MIN_SCALE)
        max_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale < min_scale)
        max_scale = min_scale;

    if (g_main_view_zoom_scale < min_scale)
        g_main_view_zoom_scale = min_scale;
    if (g_main_view_zoom_scale > max_scale)
        g_main_view_zoom_scale = max_scale;

    configured_scale = sdl_configured_main_view_scale();
    if (g_main_view_zoom_scale == configured_scale)
        g_main_view_zoom_scale = 0;
}

void sdl_refresh_supporting_panes_layout(void)
{
    SDL_Rect screen;

    if (!g_state.window)
        return;
    if (sdl_layout_matches_supporting_pane_visibility())
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

void sdl_refresh_supporting_panes_layout_deferred(void)
{
    bool old_defer = g_defer_resize_handle_stuff;

    g_defer_resize_handle_stuff = true;
    sdl_refresh_supporting_panes_layout();
    g_defer_resize_handle_stuff = old_defer;
}

/*
 * Apply current SDL configuration by triggering a resize.
 * This makes changes to scale, font size, margin, etc. take effect immediately.
 */
void sdl_apply_runtime_zoom(void)
{
    Uint64 start_ns = SDL_GetTicksNS();
    Uint64 story_ns;
    Uint64 resize_ns;
    int story_depth;
    bool global_story_active;
    bool global_story_grid;
    bool local_story_active = false;
    bool local_story_grid = false;
    int local_story_term_index = -1;

    if (!g_state.window) {
        log_warn("sdl_apply_runtime_zoom: no window, skipping");
        return;
    }

    story_depth = g_state.story_font_depth;
    global_story_active = (story_depth > 0);
    global_story_grid = g_state.story_font_grid;
    if (!global_story_active && Term
        && (Term->story_font_active || Term->story_font_grid))
    {
        size_t term_index = (size_t)(uintptr_t)Term->data;
        if (term_index < MAX_TERM_DATA) {
            local_story_term_index = (int)term_index;
            local_story_active = Term->story_font_active;
            local_story_grid = Term->story_font_grid;
        }
    }

    sdl_refresh_safe_area();
    sdl_clamp_main_view_zoom_to_current_layout();
    g_auto_aux_main_cell_h_override = config.main_view_scale * TILE_SIZE;
    story_ns = SDL_GetTicksNS();
    sdl_load_story_fonts();
    story_ns = SDL_GetTicksNS() - story_ns;
    resize_ns = SDL_GetTicksNS();
    sdl_resize_for_current_layout();
    resize_ns = SDL_GetTicksNS() - resize_ns;
    g_auto_aux_main_cell_h_override = 0;
    sdl_queue_main_view_scale_neighbors_prewarm("runtime zoom");

    if (global_story_active || global_story_grid) {
        g_state.story_font_depth = global_story_active ? story_depth : 0;
        sdl_apply_story_font_state(global_story_active);
        g_state.story_font_grid = global_story_grid;
        sdl_apply_story_grid_state(global_story_grid);
    } else if (local_story_term_index >= 0) {
        term* local_term = &g_views[local_story_term_index].t;
        local_term->story_font_active = local_story_active;
        local_term->story_font_grid = local_story_grid;
    }

    sdl_request_redraw();
    log_debug("sdl_apply_runtime_zoom completed in %llu ms (story=%llu ms resize=%llu ms configured=%d effective=%d)",
        (unsigned long long)((SDL_GetTicksNS() - start_ns) / 1000000ULL),
        (unsigned long long)(story_ns / 1000000ULL),
        (unsigned long long)(resize_ns / 1000000ULL),
        config.main_view_scale, sdl_current_main_view_scale());
}

void sdl_apply_config_impl(bool request_redraw)
{
    Uint64 start_ns = SDL_GetTicksNS();
    Uint64 recovery_ns;
    Uint64 story_ns;
    Uint64 resize_ns;
    int story_depth;
    bool global_story_active;
    bool global_story_grid;
    bool local_story_active = false;
    bool local_story_grid = false;
    int local_story_term_index = -1;

    if (!g_state.window) {
        log_warn("sdl_apply_config: no window, skipping");
        return;
    }

    story_depth = g_state.story_font_depth;
    global_story_active = (story_depth > 0);
    global_story_grid = g_state.story_font_grid;
    if (!global_story_active && Term
        && (Term->story_font_active || Term->story_font_grid))
    {
        size_t term_index = (size_t)(uintptr_t)Term->data;
        if (term_index < MAX_TERM_DATA)
        {
            local_story_term_index = (int)term_index;
            local_story_active = Term->story_font_active;
            local_story_grid = Term->story_font_grid;
        }
    }

    recovery_ns = SDL_GetTicksNS();
    (void)sdl_recover_layout_for_current_window("settings change", true, NULL);
    recovery_ns = SDL_GetTicksNS() - recovery_ns;
    sdl_refresh_safe_area();
    sdl_clamp_main_view_zoom_to_current_layout();
    g_auto_aux_main_cell_h_override = config.main_view_scale * TILE_SIZE;
    story_ns = SDL_GetTicksNS();
    sdl_load_story_fonts();
    story_ns = SDL_GetTicksNS() - story_ns;
    resize_ns = SDL_GetTicksNS();
    sdl_resize_for_current_layout();
    resize_ns = SDL_GetTicksNS() - resize_ns;
    g_auto_aux_main_cell_h_override = 0;
    sdl_queue_main_view_scale_neighbors_prewarm("settings change");

    if (global_story_active || global_story_grid) {
        g_state.story_font_depth = global_story_active ? story_depth : 0;
        sdl_apply_story_font_state(global_story_active);
        g_state.story_font_grid = global_story_grid;
        sdl_apply_story_grid_state(global_story_grid);
    } else if (local_story_term_index >= 0) {
        term* local_term = &g_views[local_story_term_index].t;
        local_term->story_font_active = local_story_active;
        local_term->story_font_grid = local_story_grid;
    }
    
    if (request_redraw)
        sdl_request_redraw();
    else
        g_state.need_present = true;

    log_debug("sdl_apply_config completed in %llu ms (recover=%llu ms story=%llu ms resize=%llu ms redraw=%d)",
        (unsigned long long)((SDL_GetTicksNS() - start_ns) / 1000000ULL),
        (unsigned long long)(recovery_ns / 1000000ULL),
        (unsigned long long)(story_ns / 1000000ULL),
        (unsigned long long)(resize_ns / 1000000ULL),
        request_redraw ? 1 : 0);
}

void sdl_apply_config(void)
{
    sdl_apply_config_impl(true);
}

void sdl_apply_config_no_redraw(void)
{
    sdl_apply_config_impl(false);
}

bool sdl_prepare_first_gameplay_main_view_zoom(int delta)
{
#if SIL_SDL_MOBILE_BUILD
    Uint64 start_ns;
    Uint64 story_ns;
    Uint64 resize_ns;
    int old_scale;
    int target_scale;
    bool old_skip_main_redraw;
    bool old_defer_resize_handle_stuff;

    if (delta == 0)
        return false;
    if (!g_state.window) {
        log_warn("sdl_prepare_first_gameplay_main_view_zoom: no window, skipping");
        return false;
    }

    old_scale = get_sdl_effective_main_view_scale();
    target_scale = sdl_main_screen_clamp_main_view_scale(old_scale + delta);
    if (target_scale == old_scale)
        return false;

    start_ns = SDL_GetTicksNS();
    set_sdl_main_view_zoom_scale(target_scale);
    sdl_refresh_safe_area();
    sdl_clamp_main_view_zoom_to_current_layout();
    target_scale = get_sdl_effective_main_view_scale();
    if (target_scale == old_scale)
        return false;

    g_auto_aux_main_cell_h_override = config.main_view_scale * TILE_SIZE;
    story_ns = SDL_GetTicksNS();
    sdl_load_story_fonts();
    story_ns = SDL_GetTicksNS() - story_ns;

    old_skip_main_redraw = g_skip_main_redraw_on_layout_refresh;
    old_defer_resize_handle_stuff = g_defer_resize_handle_stuff;
    g_skip_main_redraw_on_layout_refresh = true;
    g_defer_resize_handle_stuff = true;
    resize_ns = SDL_GetTicksNS();
    sdl_resize_for_current_layout();
    resize_ns = SDL_GetTicksNS() - resize_ns;
    g_defer_resize_handle_stuff = old_defer_resize_handle_stuff;
    g_skip_main_redraw_on_layout_refresh = old_skip_main_redraw;
    g_auto_aux_main_cell_h_override = 0;

    g_state.need_present = false;
    sdl_queue_main_view_scale_neighbors_prewarm("first gameplay zoom");

    log_info("Mobile first gameplay zoom prepared before redraw: %d -> %d "
             "in %llu ms (story=%llu ms resize=%llu ms)",
        old_scale, target_scale,
        (unsigned long long)((SDL_GetTicksNS() - start_ns) / 1000000ULL),
        (unsigned long long)(story_ns / 1000000ULL),
        (unsigned long long)(resize_ns / 1000000ULL));

    return true;
#else
    (void)delta;
    return false;
#endif
}

int get_sdl_platform_max_main_view_scale(void)
{
#if SIL_SDL_MOBILE_BUILD
    sdl_refresh_platform_max_main_view_scales_for_current_layout(
        "platform max query");
#endif
    return sdl_platform_max_main_view_scale_for_mode(config.min_terminal_mode);
}

int get_sdl_terminal_menu_scale(void)
{
#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
    int menu_mode = config.min_terminal_mode;
#else
    int menu_mode = SDL_MIN_TERMINAL_NORMAL;
#endif
    SDL_Rect screen;
    int max_scale = SDL_MAIN_VIEW_MIN_SCALE;
    bool supporting_panes_hidden = !sdl_should_show_supporting_panes();
    int platform_max = sdl_platform_max_main_view_scale_for_mode(menu_mode);
    int compact_platform_max =
        sdl_platform_max_main_view_scale_for_mode(SDL_MIN_TERMINAL_COMPACT);
    int min_scale = sdl_main_view_scale_floor_for_mode(menu_mode);
    int scale;

    if (supporting_panes_hidden) {
        sdl_refresh_safe_area();
        screen = sdl_get_layout_screen_rect();
        if (sdl_rect_has_area(&screen))
            max_scale = sdl_max_scale_for_rect_mode(&screen, menu_mode);
    } else {
        max_scale = sdl_max_scale_for_window_mode(menu_mode);
    }

    if (max_scale <= SDL_MAIN_VIEW_MIN_SCALE)
        scale = SDL_MAIN_VIEW_MIN_SCALE;
    else
    {
        /* Desktop/full-screen menus should keep one step below the normal
         * terminal maximum, even when gameplay is in compact mode. */
        scale = max_scale - 1;
        if (scale < min_scale)
            scale = min_scale;
    }

    log_debug("terminal menu scale: mode=%s hidden_supporting_panes=%d "
              "layout_max=%d platform_max=%d compact_platform_max=%d "
              "target=%d",
        sdl_min_terminal_mode_name(menu_mode),
        supporting_panes_hidden ? 1 : 0, max_scale, platform_max,
        compact_platform_max, scale);

    return scale;
}

void sdl_push_terminal_menu_scale(void)
{
    int old_scale = g_terminal_menu_scale_override;
    int target_scale = get_sdl_terminal_menu_scale();

    if (g_terminal_menu_scale_depth
        >= (int)N_ELEMENTS(g_terminal_menu_scale_stack))
    {
        log_warn("terminal menu scale stack is full");
        return;
    }

    g_terminal_menu_scale_stack[g_terminal_menu_scale_depth++] = old_scale;

    g_terminal_menu_scale_override = target_scale;
    if (old_scale != target_scale)
        sdl_apply_config_no_redraw();
}

void sdl_pop_terminal_menu_scale(void)
{
    int old_scale = g_terminal_menu_scale_override;
    int restored_scale = 0;

    if (g_terminal_menu_scale_depth > 0)
    {
        restored_scale =
            g_terminal_menu_scale_stack[--g_terminal_menu_scale_depth];
        g_terminal_menu_scale_stack[g_terminal_menu_scale_depth] = 0;
    }

    g_terminal_menu_scale_override = restored_scale;
    if (old_scale != restored_scale)
        sdl_apply_config_no_redraw();
}

bool sdl_description_overlay_present(const byte* attrs, const char* chars,
    const byte* tattrs, const char* tchars, const byte* story, int width,
    int height, int target_cols, int scroll, bool interactive,
    int* out_visible_rows, int* out_max_scroll)
{
    description_overlay_layout layout;

    if (out_visible_rows)
        *out_visible_rows = 1;
    if (out_max_scroll)
        *out_max_scroll = 0;

    if (!attrs || !chars || width <= 0 || height <= 0)
    {
        sdl_description_overlay_clear();
        return false;
    }

    g_description_overlay.active = true;
    g_description_overlay.interactive = interactive;
    g_description_overlay.attrs = attrs;
    g_description_overlay.chars = chars;
    g_description_overlay.tattrs = tattrs;
    g_description_overlay.tchars = tchars;
    g_description_overlay.story = story;
    g_description_overlay.width = width;
    g_description_overlay.height = height;
    g_description_overlay.target_cols = target_cols;
    g_description_overlay.scroll = scroll;
    if (interactive)
    {
        g_description_overlay.avoid_active = false;
        g_description_overlay.avoid_term_col = 0;
        g_description_overlay.avoid_term_row = 0;
        g_description_overlay.avoid_term_wid = 0;
        g_description_overlay.avoid_term_hgt = 0;
    }

    if (!sdl_description_overlay_layout(&layout))
    {
        g_description_overlay = (description_overlay_state){ 0 };
        g_state.need_present = true;
        sdl_present_if_needed(&g_views[PANE_MAIN]);
        return false;
    }

    g_description_overlay.scroll = layout.scroll;
    if (out_visible_rows)
        *out_visible_rows = layout.visible_rows;
    if (out_max_scroll)
        *out_max_scroll = layout.max_scroll;

    g_state.need_present = true;
    sdl_present_if_needed(&g_views[PANE_MAIN]);
    return true;
}

void sdl_description_overlay_clear(void)
{
    bool was_active = g_description_overlay.active;

    if (!g_description_overlay.active && !g_description_overlay.avoid_active)
        return;

    g_description_overlay = (description_overlay_state){ 0 };
    if (was_active)
    {
        g_state.need_present = true;
        sdl_present_if_needed(&g_views[PANE_MAIN]);
    }
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



