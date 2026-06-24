#include "angband.h"
#include "sdl/main-sdl-private.h"

static u16b sdl_keyboard_capture_modifiers_from_sdl(SDL_Keymod mod)
{
    u16b modifiers = 0;

    if (mod & SDL_KMOD_SHIFT)
        modifiers |= MOVEMENT_INPUT_MODIFIER_SHIFT;
    if (mod & SDL_KMOD_CTRL)
        modifiers |= MOVEMENT_INPUT_MODIFIER_CTRL;
    if (mod & SDL_KMOD_ALT)
        modifiers |= MOVEMENT_INPUT_MODIFIER_ALT;
    if (mod & SDL_KMOD_GUI)
        modifiers |= MOVEMENT_INPUT_MODIFIER_META;

    return modifiers;
}

static bool sdl_keyboard_capture_scancode_is_modifier(SDL_Scancode scancode)
{
    switch (scancode)
    {
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_RGUI:
        return true;
    default:
        return false;
    }
}

static bool sdl_keyboard_capture_handle_keydown(
    const SDL_KeyboardEvent* key_event)
{
    if (!g_keyboard_capture_active || !key_event)
        return false;

    if (SDL_GetTicksNS() < g_keyboard_capture_arm_time)
        return true;

    if (sdl_keyboard_capture_scancode_is_modifier(key_event->scancode))
        return true;

    g_keyboard_capture_scancode = key_event->scancode;
    g_keyboard_capture_modifiers =
        sdl_keyboard_capture_modifiers_from_sdl(key_event->mod);
    g_keyboard_capture_ready = true;
    g_keyboard_capture_active = false;
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

void resize(const SDL_Rect* screen)
{
    Uint64 start_ns = SDL_GetTicksNS();
    log_warn("resize enter");
    SDL_Rect panes[PANE_MAX] = {0};
    bool show_supporting_panes = sdl_should_show_supporting_panes();
    bool touch_pane_hidden = sdl_touch_pane_hidden_mode_active();
    bool include_side = config.enable_right_panes;
    bool include_bottom = config.enable_bottom_panes;
    int main_view_scale = sdl_current_main_view_scale();
    int layout_main_view_scale = sdl_main_view_layout_scale();

    if (!show_supporting_panes)
    {
        sdl_place_active_panes(screen, panes, g_active_side_panes,
            g_active_bottom_panes, true);
        g_supporting_panes_layout_visible = false;
    }

    // Split the window according to enabled pane settings. Supporting panes
    // keep their own minimum sizes, but no longer get removed solely to
    // preserve a minimum main terminal size.
    if (show_supporting_panes) {
        int cell_w = layout_main_view_scale * TILE_SIZE / 2;
        int cell_h = layout_main_view_scale * TILE_SIZE;
        int min_main_cols = sdl_current_min_terminal_cols();
        int min_main_rows = sdl_current_min_terminal_rows();
        log_debug("Layout cell dimensions: %dx%d (scale=%d, TILE_SIZE=%d, render_scale=%d)",
            cell_w, cell_h, layout_main_view_scale, TILE_SIZE, main_view_scale);
        int cols;
        int rows;

        if (!include_side)
            log_info("side panes disabled by user setting");
        if (!include_bottom)
            log_info("bottom panes disabled by user setting");

        sdl_place_active_panes_fitting_main(screen, panes, include_side,
            include_bottom, false, &include_side, &include_bottom);

        cols = sdl_main_view_logical_cols_for_visual_cols(
            panes[PANE_MAIN].w / cell_w);
        rows = panes[PANE_MAIN].h / cell_h;
        log_debug("Main view: %dx%d pixels at (%d,%d) = %dx%d cells (configured minimum reference: %dx%d %s)",
            panes[PANE_MAIN].w, panes[PANE_MAIN].h,
            panes[PANE_MAIN].x, panes[PANE_MAIN].y,
            cols, rows,
            min_main_cols, min_main_rows,
            sdl_min_terminal_mode_name(config.min_terminal_mode));

        g_active_side_panes = include_side;
        g_active_bottom_panes = include_bottom;
        g_supporting_panes_layout_visible = true;
    }

    g_inventory_pane_layout_rows = -1;
    g_supply_pane_layout_rows = -1;
    if (show_supporting_panes && panes[PANE_INVENTORY].w > 0
        && panes[PANE_INVENTORY].h > 0)
    {
        g_inventory_pane_layout_rows = sdl_inventory_pane_desired_rows();
    }
    if (show_supporting_panes && panes[PANE_SUPPLY].w > 0
        && panes[PANE_SUPPLY].h > 0)
    {
        g_supply_pane_layout_rows = sdl_supply_pane_desired_rows();
    }

    for (int i = 0; i < PANE_MAX; i++) {
        const SDL_Rect* r = &panes[i];
        log_debug("pane %d is at (%d, %d) size %dx%d", i, r->x, r->y, r->w, r->h);
    }

    memcpy(g_pane_rects, panes, sizeof(g_pane_rects));
    g_touch_pane_hidden_layout_active = touch_pane_hidden;
    g_touch_pane_proto_layout_active = sdl_touch_pane_proto_mode_active();

    // Use configured monospace font or fall back to default
    const char* font_path = config.monospace_font[0] != '\0' 
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";

    for (int i = 1; i < MAX_TERM_DATA; i++) {
        // Always destroy the old pane to prevent its display in cases when we
        // have removed one of the bars or both of them due to the size
        // restrictions.
        sdl_view_destroy(&g_views[i]);
        if (panes[i].w > 0 && panes[i].h > 0) {
            if (!sdl_view_create(&g_views[i], panes[i], font_path,
                    sdl_effective_pane_font_size_for_type((enum pane_type)i), 0,
                    config.margin))
            {
                g_pane_rects[i] = (SDL_Rect){ 0 };
                continue;
            }
            sdl_view_link_term(&g_views[i], i);
        }
    }

    sdl_view_destroy(&g_views[0]);
    if (!sdl_view_create(&g_views[0], panes[PANE_MAIN], font_path, 0,
            main_view_scale, config.margin))
    {
        quit("could not create main view");
    }
    sdl_view_link_term(&g_views[0], 0);
    sdl_update_left_panel_pane_rect();
    sdl_mark_active_supporting_panes_dirty(panes);

    Term_activate(&g_views[0].t);

    /* Ensure the dungeon panel matches the resized main term before the
     * freshly-created canvas is presented. Corner-pane layouts present the
     * map through a shifted SDL projection, so leaving PU_PANEL for a later
     * game update can show one frame, or sometimes a whole idle screen, with
     * the old horizontal panel origin. */
    if (character_dungeon && p_ptr)
    {
        p_ptr->update |= PU_PANEL;
        p_ptr->redraw |= PR_MAP;
        if (character_generated && !character_icky && p_ptr->playing
            && !g_defer_resize_handle_stuff)
        {
            handle_stuff();
        }
    }

    // Don't strictly need this as `sdl_view_create` already sets this flag.
    g_state.need_present = true;
    sdl_update_cursor_visibility();
    log_debug("resize completed in %llu ms (render_scale=%d layout_scale=%d main=%dx%d)",
        (unsigned long long)((SDL_GetTicksNS() - start_ns) / 1000000ULL),
        main_view_scale, layout_main_view_scale, g_views[PANE_MAIN].cols,
        g_views[PANE_MAIN].rows);
}

/*
 * Handle renderer device/targets reset.
 * This can happen on NVIDIA when switching fullscreen modes,
 * when the driver resets, or after sleep/wake cycles.
 * We need to recreate all render targets (canvas textures).
 */
void sdl_handle_renderer_reset(void)
{
    const char* font_path = config.monospace_font[0] != '\0'
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";

    sdl_left_panel_canvas_destroy();
    sdl_mono_font_cache_clear();

    // Recreate all view canvases
    for (int i = 0; i < MAX_TERM_DATA; i++) {
        sdl_view* view = &g_views[i];
        if (!view->term_ready)
            continue;

        if (view->font_atlas && !view->font_atlas_cached)
            SDL_DestroyTexture(view->font_atlas);
        view->font_atlas = sdl_acquire_mono_font_atlas_cells_ex(font_path,
            view->cell_w, view->cell_h, &view->font_atlas_cached,
            &view->font_atlas_cell_w, &view->font_atlas_cell_h,
            &view->font_atlas_exact, true);
        if (view->font_atlas) {
            if (!view->font_atlas_exact)
                sdl_queue_mono_font_atlas_prewarm_cells(font_path,
                    view->cell_w, view->cell_h);
            SDL_SetTextureBlendMode(view->font_atlas, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(view->font_atlas, 255, 255, 255);
            SDL_SetTextureAlphaMod(view->font_atlas, 255);
        }

        // Destroy old canvas
        if (view->canvas) {
            SDL_DestroyTexture(view->canvas);
            view->canvas = NULL;
        }

        // Recreate canvas texture
        view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET,
                                         view->cols * view->cell_w,
                                         view->rows * view->cell_h);
        if (view->canvas) {
            Uint8 bg_alpha = sdl_view_background_alpha(view);

            SDL_SetTextureBlendMode(view->canvas,
                (bg_alpha < SDL_ALPHA_OPAQUE) ? SDL_BLENDMODE_BLEND
                                              : SDL_BLENDMODE_NONE);
            SDL_SetTextureScaleMode(view->canvas, SDL_SCALEMODE_NEAREST);
            SDL_SetRenderTarget(g_state.renderer, view->canvas);
            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, bg_alpha);
            SDL_RenderClear(g_state.renderer);
            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        } else {
            log_error("Failed to recreate canvas for view %d: %s", i, SDL_GetError());
        }
    }

    // Recreate tileset if using tiles
    if (g_state.use_tiles && g_state.tileset) {
        SDL_DestroyTexture(g_state.tileset);
        g_state.tileset = NULL;

        SDL_Surface* ts = IMG_Load("lib/xtra/graf/16x16.png");
        if (ts) {
            g_state.tileset = SDL_CreateTextureFromSurface(g_state.renderer, ts);
            if (g_state.tileset) {
                SDL_SetTextureScaleMode(g_state.tileset, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
            }
            SDL_DestroySurface(ts);
        }
    }

    // Force a full redraw
    g_state.need_present = true;
    Term_redraw();
    sdl_queue_main_view_scale_neighbors_prewarm("renderer reset");
}

#if SIL_SDL_MOBILE_BUILD
bool sdl_mobile_lifecycle_handle_event(const SDL_Event* ev)
{
    if (!ev)
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        if (!g_mobile_lifecycle_autosaved
            && mobile_autosave_game("will enter background"))
        {
            g_mobile_lifecycle_autosaved = true;
        }
        return true;

    case SDL_EVENT_DID_ENTER_BACKGROUND:
        if (!g_mobile_lifecycle_autosaved
            && mobile_autosave_game("did enter background"))
        {
            g_mobile_lifecycle_autosaved = true;
        }
        return true;

    case SDL_EVENT_TERMINATING:
        if (mobile_autosave_game("terminating"))
            g_mobile_lifecycle_autosaved = true;
        return true;

    case SDL_EVENT_WILL_ENTER_FOREGROUND:
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        g_mobile_lifecycle_autosaved = false;
        return true;

    case SDL_EVENT_LOW_MEMORY:
        log_warn("mobile lifecycle: low memory event received");
        return true;

    default:
        return false;
    }
}

bool SDLCALL sdl_mobile_lifecycle_event_watch(void* userdata, SDL_Event* ev)
{
    (void)userdata;
    (void)sdl_mobile_lifecycle_handle_event(ev);
    return true;
}

void sdl_mobile_lifecycle_register(void)
{
    if (g_mobile_lifecycle_watch_registered)
        return;

    if (SDL_AddEventWatch(sdl_mobile_lifecycle_event_watch, NULL))
    {
        g_mobile_lifecycle_watch_registered = true;
        log_info("Registered mobile lifecycle autosave event watch");
    }
    else
    {
        log_warn("Failed to register mobile lifecycle event watch: %s",
            SDL_GetError());
    }
}

void sdl_mobile_lifecycle_unregister(void)
{
    if (!g_mobile_lifecycle_watch_registered)
        return;

    SDL_RemoveEventWatch(sdl_mobile_lifecycle_event_watch, NULL);
    g_mobile_lifecycle_watch_registered = false;
}
#endif

bool sdl_event_is_disabled_mouse_input(const SDL_Event* ev)
{
    if (!ev || config.mouse_enabled)
        return false;

    switch (ev->type) {
    case SDL_EVENT_MOUSE_MOTION:
        return ev->motion.which != SDL_TOUCH_MOUSEID;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return ev->button.which != SDL_TOUCH_MOUSEID;
    case SDL_EVENT_MOUSE_WHEEL:
        return ev->wheel.which != SDL_TOUCH_MOUSEID;
    default:
        return false;
    }
}

bool sdl_event_is_touch_mouse_input(const SDL_Event* ev)
{
    if (!ev)
        return false;

    switch (ev->type) {
    case SDL_EVENT_MOUSE_MOTION:
        return ev->motion.which == SDL_TOUCH_MOUSEID;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return ev->button.which == SDL_TOUCH_MOUSEID;
    case SDL_EVENT_MOUSE_WHEEL:
        return ev->wheel.which == SDL_TOUCH_MOUSEID;
    default:
        return false;
    }
}

bool sdl_ascii_contains_ci(cptr text, cptr needle)
{
    size_t needle_len;

    if (!text || !needle)
        return false;
    needle_len = strlen(needle);
    if (needle_len == 0)
        return true;

    for (const char* p = text; *p; p++) {
        size_t i;

        for (i = 0; i < needle_len; i++) {
            unsigned char a = (unsigned char)p[i];
            unsigned char b = (unsigned char)needle[i];

            if (!a)
                return false;
            if (tolower(a) != tolower(b))
                break;
        }
        if (i == needle_len)
            return true;
    }

    return false;
}

bool sdl_mouse_id_looks_like_touchscreen(SDL_MouseID id)
{
    const char* name;

    if (id == 0)
        return false;

    name = SDL_GetMouseNameForID(id);
    if (!name || !name[0])
        return false;

    if (sdl_ascii_contains_ci(name, "touchpad"))
        return false;

    return sdl_ascii_contains_ci(name, "touchscreen")
        || sdl_ascii_contains_ci(name, "touch screen")
        || sdl_ascii_contains_ci(name, "fts3528");
}

bool sdl_event_is_handheld_touch_fallback_input(const SDL_Event* ev)
{
    if (!ev || config.mouse_enabled)
        return false;
    if (g_startup_device_class != SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER
        && g_startup_device_class != SDL_STARTUP_DEVICE_DESKTOP_HANDHELD)
    {
        return false;
    }
    if (g_direct_touch_present)
        return false;

    switch (ev->type) {
    case SDL_EVENT_MOUSE_MOTION:
        if (sdl_mouse_id_looks_like_touchscreen(ev->motion.which))
            return true;
        /* Steam Deck Game Mode under Proton can expose touch as a generic
         * Wine HID mouse. Only route its motion while a fallback press is live. */
        return g_touch_mouse_fallback_active
            && g_touch_mouse_fallback_mouse_id == ev->motion.which;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (sdl_mouse_id_looks_like_touchscreen(ev->button.which))
            return true;
        if (ev->button.button != SDL_BUTTON_LEFT)
            return false;
        if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            return true;
        return g_touch_mouse_fallback_active
            && g_touch_mouse_fallback_mouse_id == ev->button.which;
    default:
        return false;
    }
}

void sdl_log_handheld_touch_mouse_fallback(SDL_MouseID id)
{
    const char* name;

    if (g_handheld_untagged_mouse_fallback_logged)
        return;
    if (sdl_mouse_id_looks_like_touchscreen(id))
        return;

    name = SDL_GetMouseNameForID(id);
    log_info("Desktop-handheld touch fallback: treating mouse id %u (%s) as touch input while mouse control is disabled",
        (unsigned)id, (name && name[0]) ? name : "unnamed");
    g_handheld_untagged_mouse_fallback_logged = true;
}

/* Keep touch-generated mouse events on the touch path when SDL has not
 * reported a direct touch device yet.  Steam Deck's touchscreen can also
 * arrive as untagged mouse input, so the desktop-handheld profile gets the
 * same fallback while mouse input is disabled. */
bool sdl_dispatch_touch_mouse_fallback(sdl_state* st,
    SDL_EventType type, SDL_WindowID window_id, Uint64 timestamp, float x,
    float y, float dx, float dy)
{
    SDL_Event touch_event;
    int window_w = 0;
    int window_h = 0;

    if (!g_state.window)
        return true;

    SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
    if (window_w <= 0 || window_h <= 0)
        return true;

    memset(&touch_event, 0, sizeof(touch_event));
    touch_event.type = type;
    touch_event.tfinger.type = type;
    touch_event.tfinger.timestamp = timestamp;
    touch_event.tfinger.touchID = SDL_MOUSE_TOUCHID;
    touch_event.tfinger.fingerID = TOUCH_MOUSE_FALLBACK_FINGER_ID;
    touch_event.tfinger.x = x / (float)window_w;
    touch_event.tfinger.y = y / (float)window_h;
    touch_event.tfinger.dx = dx / (float)window_w;
    touch_event.tfinger.dy = dy / (float)window_h;
    touch_event.tfinger.pressure = (type == SDL_EVENT_FINGER_UP) ? 0.0f : 1.0f;
    touch_event.tfinger.windowID = window_id;

    sdl_handle_event(st, &touch_event);
    return true;
}

bool sdl_try_handle_touch_mouse_fallback_event(sdl_state* st,
    const SDL_Event* ev)
{
    bool explicit_touch_mouse = sdl_event_is_touch_mouse_input(ev);
    bool handheld_touch_fallback =
        sdl_event_is_handheld_touch_fallback_input(ev);

    if (!explicit_touch_mouse && !handheld_touch_fallback)
        return false;

    if (explicit_touch_mouse && g_direct_touch_present) {
        g_touch_mouse_fallback_active = false;
        g_touch_mouse_fallback_mouse_id = 0;
        return true;
    }

    switch (ev->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.button != SDL_BUTTON_LEFT)
            return true;

        g_touch_mouse_fallback_active = true;
        g_touch_mouse_fallback_mouse_id = ev->button.which;
        if (handheld_touch_fallback)
            sdl_log_handheld_touch_mouse_fallback(ev->button.which);
        return sdl_dispatch_touch_mouse_fallback(st, SDL_EVENT_FINGER_DOWN,
            ev->button.windowID, ev->button.timestamp, ev->button.x,
            ev->button.y, 0.0f, 0.0f);

    case SDL_EVENT_MOUSE_MOTION:
        if (!g_touch_mouse_fallback_active)
            return true;

        return sdl_dispatch_touch_mouse_fallback(st, SDL_EVENT_FINGER_MOTION,
            ev->motion.windowID, ev->motion.timestamp, ev->motion.x,
            ev->motion.y, ev->motion.xrel, ev->motion.yrel);

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev->button.button != SDL_BUTTON_LEFT)
            return true;
        if (!g_touch_mouse_fallback_active)
            return true;

        g_touch_mouse_fallback_active = false;
        g_touch_mouse_fallback_mouse_id = 0;
        return sdl_dispatch_touch_mouse_fallback(st, SDL_EVENT_FINGER_UP,
            ev->button.windowID, ev->button.timestamp, ev->button.x,
            ev->button.y, 0.0f, 0.0f);

    case SDL_EVENT_MOUSE_WHEEL:
        return true;

    default:
        return true;
    }
}

bool sdl_finger_event_to_render_coords(const SDL_TouchFingerEvent* finger,
    float* out_x, float* out_y)
{
    int window_w = 0;
    int window_h = 0;
    float x;
    float y;

    if (!finger || !out_x || !out_y || !g_state.window)
        return false;

    /*
     * Mouse fallback events are synthesized after mouse coordinates have
     * already been normalized to renderer space.  Preserve that round trip
     * instead of applying the window-to-render transform a second time.
     */
    if (finger->touchID == SDL_MOUSE_TOUCHID
        || finger->fingerID == TOUCH_MOUSE_FALLBACK_FINGER_ID)
    {
        SDL_Rect pixels = sdl_get_window_pixel_rect();

        if (!sdl_rect_has_area(&pixels))
            return false;
        *out_x = finger->x * (float)pixels.w;
        *out_y = finger->y * (float)pixels.h;
        return true;
    }

    SDL_GetWindowSize(g_state.window, &window_w, &window_h);
    if (window_w <= 0 || window_h <= 0)
        return false;

    x = finger->x * (float)window_w;
    y = finger->y * (float)window_h;
    if (g_state.renderer)
        SDL_RenderCoordinatesFromWindow(g_state.renderer, x, y, &x, &y);

    *out_x = x;
    *out_y = y;
    return true;
}

/* The renderer is created with SDL_WINDOW_HIGH_PIXEL_DENSITY and the rest of
 * the code consumes coordinates in renderer/pixel space.
 * SDL3 mouse events arrive in window-point space, so on Retina/HiDPI displays
 * the reported (x, y) is half (or 1/scale of) the rendered position. Convert
 * mouse motion/button/wheel coordinates to renderer space here so every
 * downstream handler sees pixel-space coordinates. Touch finger coordinates
 * are normalized (0..1) and converted by sdl_finger_event_to_render_coords(). */
void sdl_normalize_event_to_render_coords(SDL_Event* ev)
{
    if (!ev || !g_state.renderer)
        return;

    switch (ev->type) {
    case SDL_EVENT_MOUSE_MOTION: {
        float x = ev->motion.x;
        float y = ev->motion.y;
        SDL_RenderCoordinatesFromWindow(g_state.renderer, x, y, &x, &y);
        ev->motion.x = x;
        ev->motion.y = y;

        float ox = 0.0f;
        float oy = 0.0f;
        SDL_RenderCoordinatesFromWindow(g_state.renderer, 0.0f, 0.0f, &ox, &oy);
        float rx = ev->motion.xrel;
        float ry = ev->motion.yrel;
        SDL_RenderCoordinatesFromWindow(g_state.renderer, rx, ry, &rx, &ry);
        ev->motion.xrel = rx - ox;
        ev->motion.yrel = ry - oy;
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        float x = ev->button.x;
        float y = ev->button.y;
        SDL_RenderCoordinatesFromWindow(g_state.renderer, x, y, &x, &y);
        ev->button.x = x;
        ev->button.y = y;
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        float x = ev->wheel.mouse_x;
        float y = ev->wheel.mouse_y;
        SDL_RenderCoordinatesFromWindow(g_state.renderer, x, y, &x, &y);
        ev->wheel.mouse_x = x;
        ev->wheel.mouse_y = y;
        break;
    }
    default:
        break;
    }
}

static bool sdl_event_is_narrative_banner_input(const SDL_Event* ev)
{
    int deadzone;

    if (!ev)
        return false;

    switch (ev->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        return true;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        deadzone = config.gamepad_deadzone;
        if (deadzone < 0)
            deadzone = 0;
        return ABS(ev->gaxis.value) > deadzone;

    default:
        return false;
    }
}

static bool sdl_event_targets_touch_top_panel(const SDL_Event* ev)
{
    float x;
    float y;

    if (!ev)
        return false;

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        return sdl_touch_top_panel_pointer_claims_point(
            (float)ev->button.x, (float)ev->button.y);
    }

    if (ev->type == SDL_EVENT_FINGER_DOWN)
    {
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return false;
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return false;
        return sdl_touch_top_panel_pointer_claims_point(x, y);
    }

    return false;
}

static bool sdl_narrative_banner_consume_input_event(const SDL_Event* ev)
{
    if (!active_narrative_banner_consumes_input())
        return false;
    if (!sdl_event_is_narrative_banner_input(ev))
        return false;
    if (sdl_event_targets_touch_top_panel(ev))
        return false;

    clear_active_narrative_banner();
    do_cmd_redraw();
    g_state.need_present = true;
    return true;
}

static bool sdl_narrative_banner_handle_pointer_event(const SDL_Event* ev)
{
    float x;
    float y;

    if (!ev)
        return false;

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        if (sdl_touch_top_panel_pointer_claims_point(
                (float)ev->button.x, (float)ev->button.y))
        {
            return false;
        }
        return sdl_narrative_banner_handle_pointer(
            (float)ev->button.x, (float)ev->button.y);
    }

    if (ev->type == SDL_EVENT_FINGER_DOWN)
    {
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return false;
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return false;
        if (sdl_touch_top_panel_pointer_claims_point(x, y))
            return false;
        return sdl_narrative_banner_handle_pointer(x, y);
    }

    return false;
}

bool sdl_quit_transition_active(void)
{
    return character_generated
        && character_dungeon
        && p_ptr
        && character_icky == 0
        && !screen_saved_fullscreen_active()
        && !death_spectator_active()
        /* A modal yes/no prompt (get_check) genuinely needs the user's answer,
         * even though p_ptr->leaving is already set.  The clearest case is the
         * wizard/cheat "Die?" confirm: death sets both is_dead and leaving while
         * p_ptr->playing is still true, so without this guard the quit
         * transition swallows every click and keypress and the prompt can never
         * be answered. */
        && !g_touch_pane_yes_no_prompt_active
        && (p_ptr->leaving || !p_ptr->playing);
}

bool sdl_quit_transition_input_event(const SDL_Event* ev)
{
    if (!ev)
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        return true;
    default:
        return false;
    }
}

bool sdl_quit_transition_consume_event(const SDL_Event* ev)
{
    if (!sdl_quit_transition_active()
        || !sdl_quit_transition_input_event(ev))
    {
        return false;
    }

    sdl_main_menu_overlay_close();
    sdl_player_action_menu_cancel();
    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    ui_menu_click_clear_pending_hover();
    ui_menu_click_clear();
    g_main_menu_pane_hover = false;
    g_depth_pane_hover_action = SDL_DEPTH_PANE_HOVER_NONE;
    g_state.need_present = true;

    return true;
}

/* While the yes/no confirmation popup is active it must behave as a true modal:
 * give it first claim on mouse input so it stays clickable even when another
 * full-screen surface (the character sheet at birth, the welcome screen) is up
 * and the normal map-screen pointer routing would otherwise consume the click.
 * Keyboard events are deliberately left to fall through so y/n/ESC still work. */
bool sdl_yes_no_prompt_handle_modal_event(const SDL_Event* ev)
{
    float x;
    float y;

    if (!ev || !g_touch_pane_yes_no_prompt_active)
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        if (ev->motion.which == SDL_TOUCH_MOUSEID)
            return false;
        return sdl_touch_pane_handle_yes_no_prompt_hover(
            (float)ev->motion.x, (float)ev->motion.y);
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.button != SDL_BUTTON_LEFT
            || ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        return sdl_touch_pane_handle_yes_no_prompt_pointer(
            (float)ev->button.x, (float)ev->button.y);
    case SDL_EVENT_FINGER_DOWN:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        sdl_note_touch_event_device(ev->tfinger.touchID);
        if (sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return sdl_touch_pane_handle_yes_no_prompt_pointer(x, y);
        return true;
    case SDL_EVENT_FINGER_MOTION:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        if (sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return sdl_touch_pane_handle_yes_no_prompt_hover(x, y);
        return true;
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        return true;
    default:
        return false;
    }
}

/*
 * While an interactive question overlay (the in-menu value picker) is up it is
 * modal: it must own every pointer/touch event so input never leaks to the
 * screen behind it (e.g. the settings menu it was opened from, whose own
 * pointer handler would otherwise keep moving its selection).  In-panel motion
 * and clicks drive the picker; a click/tap outside the panel cancels it, the
 * same as the keyboard Escape path.  Returns true when the event was a
 * pointer/touch event and was consumed; keyboard and gamepad events return
 * false so they still reach inkey() through the normal handlers below.
 */
static bool sdl_question_overlay_consume_pointer(const SDL_Event* ev)
{
    float x;
    float y;

    switch (ev->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        if (ev->motion.which != SDL_TOUCH_MOUSEID)
            sdl_question_menu_handle_hover_pointer((float)ev->motion.x,
                (float)ev->motion.y);
        return true;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.which != SDL_TOUCH_MOUSEID)
        {
            int action = (ev->button.button == SDL_BUTTON_RIGHT)
                ? UI_MENU_CLICK_SECONDARY
                : UI_MENU_CLICK_PRIMARY;

            if (!sdl_question_menu_handle_pointer((float)ev->button.x,
                    (float)ev->button.y, action))
            {
                Term_keypress(ESCAPE);
            }
        }
        return true;

    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return true;

    case SDL_EVENT_FINGER_DOWN:
        if (ev->tfinger.windowID == SDL_GetWindowID(g_state.window))
        {
            sdl_note_touch_event_device(ev->tfinger.touchID);
            if (sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y)
                && !sdl_question_menu_handle_pointer(x, y,
                       UI_MENU_CLICK_PRIMARY))
            {
                Term_keypress(ESCAPE);
            }
        }
        return true;

    case SDL_EVENT_FINGER_MOTION:
        if (ev->tfinger.windowID == SDL_GetWindowID(g_state.window)
            && sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
        {
            sdl_question_menu_handle_hover_pointer(x, y);
        }
        return true;

    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        return true;

    default:
        return false;
    }
}

void sdl_handle_event(sdl_state* st, SDL_Event* ev)
{
    (void)st;
    sdl_normalize_event_to_render_coords(ev);
    if (sdl_sound_try_handle_event(ev)) {
        return;
    }
#if SIL_SDL_MOBILE_BUILD
    if (sdl_mobile_lifecycle_handle_event(ev)) {
        return;
    }
#endif
    if (sdl_try_handle_touch_mouse_fallback_event(st, ev))
        return;
    if (sdl_event_is_disabled_mouse_input(ev))
        return;

    if (ev->type == SDL_EVENT_QUIT) {
        Term_keypress(27); // ESC or define a quit signal
    } else if (sdl_quit_transition_consume_event(ev)) {
        return;
    } else if (sdl_question_menu_blocks_input()) {
        switch (ev->type)
        {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_CANCELED:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            return;
        default:
            break;
        }
    } else if (g_touch_pane_reset_confirm_active) {
        if (ev->type == SDL_EVENT_KEY_DOWN) {
            if (sdl_key_is_escape_or_back(ev->key.key)) {
                sdl_touch_pane_finish_reset_confirm(false);
            } else if (ev->key.key == SDLK_RETURN || ev->key.key == SDLK_KP_ENTER
                || ev->key.key == SDLK_SPACE)
            {
                sdl_touch_pane_finish_reset_confirm(true);
            }
            return;
        } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (ev->button.button == SDL_BUTTON_LEFT && ev->button.which != SDL_TOUCH_MOUSEID)
                sdl_touch_pane_handle_reset_prompt_pointer((float)ev->button.x, (float)ev->button.y);
            return;
        } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP) {
            return;
        } else if (ev->type == SDL_EVENT_FINGER_DOWN) {
            float x;
            float y;

            if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
                return;
            sdl_note_touch_event_device(ev->tfinger.touchID);
            if (sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
                sdl_touch_pane_handle_reset_prompt_pointer(x, y);
            return;
        } else if (ev->type == SDL_EVENT_FINGER_UP || ev->type == SDL_EVENT_FINGER_CANCELED) {
            return;
        }
    } else if (sdl_yes_no_prompt_handle_modal_event(ev)) {
        return;
    } else if (sdl_main_menu_overlay_handle_event(ev)) {
        return;
    } else if (g_minimap.active && sdl_minimap_handle_event(ev)) {
        return;
    } else if (sdl_narrative_banner_consume_input_event(ev)) {
        return;
    } else if (sdl_narrative_banner_handle_pointer_event(ev)) {
        return;
    } else if (sdl_screen_back_gesture_handle_event(ev)) {
        return;
    } else if (sdl_question_menu_captures_pointer()
        && sdl_question_overlay_consume_pointer(ev)) {
        return;
    } else if (sdl_character_sheet_screen_handle_pointer_event(ev)) {
        return;
    } else if (ev->type == SDL_EVENT_MOUSE_MOTION) {
        if (ev->motion.which == SDL_TOUCH_MOUSEID)
            return;
        if (sdl_welcome_screen_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        sdl_object_tooltip_handle_mouse_motion((float)ev->motion.x,
            (float)ev->motion.y);
        if (g_touch_pane_yes_no_prompt_active
            && sdl_touch_pane_handle_yes_no_prompt_hover(
                (float)ev->motion.x, (float)ev->motion.y))
        {
            return;
        }
        if (sdl_description_overlay_handle_footer_hover((float)ev->motion.x,
                (float)ev->motion.y))
        {
            return;
        }
        if (sdl_log_pane_menu_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y, 0, true))
        {
            return;
        }
        if (sdl_side_pane_menu_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y, 0, true))
        {
            return;
        }
        if (sdl_pane_layout_drag_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        if (sdl_pointer_aim_handle_motion((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        if (sdl_player_action_menu_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y, 0, true))
        {
            return;
        }
        if (sdl_player_exchange_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y, 0, true))
        {
            return;
        }
        if (sdl_unified_look_handle_map_drag_motion((float)ev->motion.x,
            (float)ev->motion.y, true, 0))
        {
            return;
        }
        if (sdl_main_map_handle_drag_motion((float)ev->motion.x,
            (float)ev->motion.y, true, 0))
        {
            return;
        }
        if (sdl_side_map_pane_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y, true, 0))
        {
            return;
        }
        if (sdl_menu_touch_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y, 0, true))
        {
            return;
        }
        if (sdl_unified_look_prompt_handle_hover_pointer(
            (float)ev->motion.x, (float)ev->motion.y))
        {
            return;
        }
        if (sdl_unified_look_sidebar_handle_hover_pointer(
            (float)ev->motion.x, (float)ev->motion.y))
        {
            return;
        }
        if (sdl_song_menu_handle_hover_pointer((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        if (sdl_question_menu_handle_hover_pointer((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        if (sdl_character_panel_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y, true, 0))
        {
            return;
        }
        if (sdl_main_screen_handle_menu_hover_pointer((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        if (sdl_main_menu_pane_handle_hover_pointer((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        if (sdl_depth_menu_pane_handle_hover_pointer((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        if (sdl_main_screen_handle_status_line_hover_pointer(
            (float)ev->motion.x, (float)ev->motion.y))
        {
            return;
        }
        if (sdl_main_screen_handle_character_panel_hover_pointer(
            (float)ev->motion.x, (float)ev->motion.y))
        {
            return;
        }
        if (sdl_touch_pane_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y, true, 0))
        {
            return;
        }
        if (sdl_touch_top_panel_handle_pointer_motion((float)ev->motion.x,
            (float)ev->motion.y, 0))
        {
            return;
        }
        if (sdl_unified_look_handle_map_hover_pointer((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        if (sdl_pointer_attack_handle_motion((float)ev->motion.x,
            (float)ev->motion.y))
        {
            return;
        }
        sdl_mouse_path_handle_motion((float)ev->motion.x, (float)ev->motion.y);
    } else if (ev->type == SDL_EVENT_MOUSE_WHEEL) {
        if (sdl_unified_look_handle_map_zoom_wheel(&ev->wheel))
            return;
        if (sdl_main_map_handle_zoom_wheel(&ev->wheel))
            return;
        if (sdl_side_map_pane_handle_mouse_wheel(&ev->wheel))
            return;
        if (sdl_description_overlay_handle_mouse_wheel(&ev->wheel))
            return;
        if (sdl_menu_scroll_handle_mouse_wheel(&ev->wheel))
            return;
    } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev->button.which != SDL_TOUCH_MOUSEID
            && sdl_log_pane_menu_handle_pointer_down((float)ev->button.x,
                (float)ev->button.y, 0, true))
        {
            return;
        }
        if (ev->button.which != SDL_TOUCH_MOUSEID
            && sdl_side_pane_menu_handle_pointer_down((float)ev->button.x,
                (float)ev->button.y, 0, true))
        {
            return;
        }
        if (ev->button.button == SDL_BUTTON_LEFT) {
            if (ev->button.which == SDL_TOUCH_MOUSEID)
                return;
            if (sdl_touch_exit_button_handle_pointer((float)ev->button.x,
                    (float)ev->button.y))
            {
                return;
            }
            if (sdl_pointer_activate_welcome_screen_at((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_touch_pane_handle_yes_no_prompt_pointer((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (ui_key_wait_dismiss_is_active()
                && sdl_pointer_dismiss_any_key_prompt())
            {
                return;
            }
            if (sdl_description_overlay_handle_footer_pointer(
                    (float)ev->button.x, (float)ev->button.y))
            {
                return;
            }
            if (sdl_pointer_aim_handle_left_click((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_main_menu_pane_handle_pointer((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_depth_menu_pane_handle_pointer((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_player_action_menu_handle_pointer_down(
                (float)ev->button.x, (float)ev->button.y, 0, true, false))
            {
                return;
            }
            if (sdl_player_exchange_handle_pointer_down(
                (float)ev->button.x, (float)ev->button.y, 0, true))
            {
                return;
            }
            if (sdl_unified_look_prompt_handle_pointer(
                    (float)ev->button.x, (float)ev->button.y,
                    UI_MENU_CLICK_PRIMARY))
            {
                return;
            }
            if (sdl_unified_look_sidebar_handle_pointer(
                    (float)ev->button.x, (float)ev->button.y,
                    UI_MENU_CLICK_PRIMARY))
            {
                return;
            }
            if (sdl_song_menu_handle_pointer((float)ev->button.x,
                    (float)ev->button.y, UI_MENU_CLICK_PRIMARY))
            {
                return;
            }
            if (sdl_question_menu_handle_pointer((float)ev->button.x,
                    (float)ev->button.y, UI_MENU_CLICK_PRIMARY))
            {
                return;
            }
            if (sdl_touch_hidden_indicator_handle_pointer_down(
                    (float)ev->button.x, (float)ev->button.y, false))
            {
                return;
            }
            if (sdl_touch_top_panel_handle_pointer_down(
                    (float)ev->button.x, (float)ev->button.y, 0))
            {
                return;
            }
            if (sdl_unified_look_handle_map_drag_down(
                    (float)ev->button.x, (float)ev->button.y, true, 0))
            {
                return;
            }
            if (sdl_main_map_handle_drag_down(
                    (float)ev->button.x, (float)ev->button.y, true, 0))
            {
                return;
            }
            if (sdl_side_map_pane_handle_pointer_down(
                    (float)ev->button.x, (float)ev->button.y, true, 0))
            {
                return;
            }
            if (sdl_unified_look_handle_map_target_pointer(
                    (float)ev->button.x, (float)ev->button.y))
            {
                return;
            }
            if (sdl_main_screen_click_shortcuts_active()) {
                int map_y = 0;
                int map_x = 0;

                if (sdl_main_view_point_to_map((float)ev->button.x,
                        (float)ev->button.y, &map_y, &map_x)
                    && sdl_mouse_stuck_door_handle_left_click(map_y, map_x))
                {
                    return;
                }
            }
            if (sdl_menu_touch_handle_pointer_down((float)ev->button.x,
                (float)ev->button.y, 0, true))
            {
                return;
            }
            if (sdl_menu_scroll_handle_mouse_button((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_main_screen_handle_menu_outside_pointer((float)ev->button.x,
                (float)ev->button.y, true))
            {
                return;
            }
            if (sdl_touch_pane_handle_pointer_down((float)ev->button.x, (float)ev->button.y,
                true, 0))
            {
                return;
            }
            if (sdl_unified_look_handle_map_hover_pointer((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_main_screen_handle_status_line_pointer((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_character_panel_handle_pointer_down((float)ev->button.x,
                (float)ev->button.y, true, 0))
            {
                return;
            }
            if (sdl_pane_layout_drag_handle_pointer_down((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_main_screen_handle_supporting_pane_pointer((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_pointer_attack_handle_left_click((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_mouse_path_handle_left_click((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
        }
        else if (ev->button.button == SDL_BUTTON_RIGHT) {
            if (ev->button.which == SDL_TOUCH_MOUSEID)
                return;
            if (g_player_action_menu.active) {
                (void)sdl_player_action_menu_handle_pointer_down(
                    (float)ev->button.x, (float)ev->button.y, 0, true, true);
                return;
            }
            if (g_player_exchange_target.active) {
                sdl_player_exchange_cancel();
                return;
            }
            if (sdl_log_pane_menu_open_from_pointer((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_side_pane_menu_open_from_pointer((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_unified_look_prompt_handle_pointer(
                    (float)ev->button.x, (float)ev->button.y,
                    UI_MENU_CLICK_SECONDARY))
            {
                return;
            }
            if (sdl_unified_look_sidebar_handle_pointer(
                    (float)ev->button.x, (float)ev->button.y,
                    UI_MENU_CLICK_SECONDARY))
            {
                return;
            }
            if (sdl_song_menu_handle_pointer((float)ev->button.x,
                    (float)ev->button.y, UI_MENU_CLICK_SECONDARY))
            {
                return;
            }
            if (sdl_question_menu_handle_pointer((float)ev->button.x,
                    (float)ev->button.y, UI_MENU_CLICK_SECONDARY))
            {
                return;
            }
            if (sdl_main_screen_handle_character_panel_secondary_pointer(
                (float)ev->button.x, (float)ev->button.y))
            {
                return;
            }
            if (sdl_unified_look_handle_map_describe_pointer(
                    (float)ev->button.x, (float)ev->button.y))
            {
                return;
            }
            if (sdl_main_screen_menu_pointer_hits_cell((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_main_screen_handle_menu_outside_pointer((float)ev->button.x,
                (float)ev->button.y, false))
            {
                return;
            }
            if (ui_key_wait_dismiss_is_active()
                && sdl_pointer_dismiss_any_key_prompt())
            {
                return;
            }
            if (sdl_menu_scroll_handle_mouse_button((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_mouse_path_handle_right_click((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_mouse_recall_handle_right_click_if_available(
                    (float)ev->button.x, (float)ev->button.y))
            {
                return;
            }
            if (sdl_mouse_path_handle_right_movement_click((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_mouse_recall_handle_right_click((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
        }
        if (ev->button.which != SDL_TOUCH_MOUSEID
            && sdl_pointer_dismiss_any_key_prompt())
        {
            return;
        }
    } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (ev->button.which != SDL_TOUCH_MOUSEID
            && sdl_log_pane_menu_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, 0, true))
        {
            return;
        }
        if (ev->button.which != SDL_TOUCH_MOUSEID
            && sdl_side_pane_menu_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, 0, true))
        {
            return;
        }
        if (ev->button.button == SDL_BUTTON_LEFT) {
            if (ev->button.which == SDL_TOUCH_MOUSEID)
                return;
            if (sdl_pane_layout_drag_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y))
            {
                return;
            }
            if (sdl_pointer_aim_handle_left_release((float)ev->button.x,
                    (float)ev->button.y))
            {
                return;
            }
            if (sdl_unified_look_handle_map_drag_up((float)ev->button.x,
                (float)ev->button.y, true, 0))
            {
                return;
            }
            if (sdl_player_action_menu_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, 0, true, false))
            {
                return;
            }
            if (sdl_player_exchange_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, 0, true))
            {
                return;
            }
            if (sdl_touch_top_panel_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, 0))
            {
                return;
            }
            if (sdl_main_map_handle_drag_up((float)ev->button.x,
                (float)ev->button.y, true, 0))
            {
                return;
            }
            if (sdl_side_map_pane_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, true, 0))
            {
                return;
            }
            if (sdl_menu_touch_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, 0, true))
            {
                return;
            }
            if (sdl_character_panel_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, true, 0))
            {
                return;
            }
            sdl_touch_pane_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, true, 0);
        }
        else if (ev->button.button == SDL_BUTTON_RIGHT) {
            if (ev->button.which == SDL_TOUCH_MOUSEID)
                return;
            if (sdl_player_action_menu_handle_pointer_up((float)ev->button.x,
                (float)ev->button.y, 0, true, true))
            {
                return;
            }
            if (sdl_main_screen_handle_menu_text_pointer((float)ev->button.x,
                (float)ev->button.y, UI_MENU_CLICK_SECONDARY))
            {
                return;
            }
        }
    } else if (ev->type == SDL_EVENT_FINGER_DOWN) {
        float x;
        float y;

        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return;
        sdl_note_touch_event_device(ev->tfinger.touchID);
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return;
        /* A persistent long-press popup stays up until the next press; dismiss
         * it here so any touch clears it, mirroring the mouse right-click. */
        (void)sdl_object_tooltip_dismiss_persistent_on_press();
        if (sdl_touch_exit_button_handle_pointer(x, y))
            return;
        if (sdl_log_pane_menu_handle_pointer_down(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_side_pane_menu_handle_pointer_down(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_log_pane_menu_handle_long_press_down(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_side_pane_menu_handle_long_press_down(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_welcome_touch_handle_pointer_down(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_touch_pane_handle_yes_no_prompt_pointer(x, y))
            return;
        if (ui_key_wait_dismiss_is_active()
            && sdl_pointer_dismiss_any_key_prompt())
        {
            return;
        }
        if (sdl_description_overlay_handle_footer_pointer(x, y))
            return;
        if (sdl_pointer_aim_handle_touch_down(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_main_menu_pane_handle_pointer(x, y))
            return;
        if (sdl_depth_menu_pane_handle_pointer(x, y))
            return;
        if ((ev->tfinger.touchID == SDL_MOUSE_TOUCHID
                || ev->tfinger.fingerID == TOUCH_MOUSE_FALLBACK_FINGER_ID)
            && sdl_main_screen_click_shortcuts_active())
        {
            int map_y = 0;
            int map_x = 0;

            if (sdl_main_view_point_to_map(x, y, &map_y, &map_x)
                && sdl_mouse_stuck_door_handle_left_click(map_y, map_x))
            {
                return;
            }
        }
        if (sdl_player_action_menu_handle_pointer_down(x, y,
            ev->tfinger.fingerID, false, false))
        {
            return;
        }
        if (sdl_player_exchange_handle_pointer_down(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_unified_look_prompt_handle_pointer(x, y,
            UI_MENU_CLICK_PRIMARY))
        {
            return;
        }
        if (sdl_unified_look_sidebar_handle_pointer(x, y,
            UI_MENU_CLICK_PRIMARY))
        {
            return;
        }
        if (sdl_song_menu_handle_pointer(x, y, UI_MENU_CLICK_PRIMARY))
            return;
        if (sdl_question_menu_handle_pointer(x, y, UI_MENU_CLICK_PRIMARY))
            return;
        if (sdl_touch_hidden_indicator_handle_pointer_down(x, y, true))
            return;
        if (sdl_main_screen_menu_pointer_hits_cell(x, y)
            && sdl_menu_touch_handle_pointer_down(x, y,
                ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_unified_look_handle_map_drag_down(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_side_map_pane_handle_pointer_down(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        /* Claim thumb-button taps before the "tap away to cancel" handler, so a
         * tap on a thumb button while an item description popup is open fires
         * the button instead of closing the popup. */
        if (sdl_touch_thumb_handle_pointer_down(x, y, false,
                ev->tfinger.fingerID))
        {
            return;
        }
        if (ui_menu_click_outside_cancel_enabled()
            && sdl_menu_touch_handle_pointer_down(x, y,
                ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_touch_round_layer_controls_active()) {
            bool round_point_excluded;

            if (sdl_touch_pane_handle_pointer_down(x, y, false,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_touch_top_panel_handle_pointer_down(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_main_screen_handle_supporting_pane_pointer(x, y))
                return;
            round_point_excluded = sdl_touch_round_point_excluded(x, y);
            if (round_point_excluded) {
                if (sdl_main_screen_handle_status_line_pointer(x, y))
                    return;
                if (sdl_character_panel_handle_pointer_down(x, y, false,
                        ev->tfinger.fingerID))
                {
                    return;
                }
                if (sdl_pointer_attack_handle_touch_down(x, y,
                        ev->tfinger.fingerID))
                {
                    return;
                }
                if (sdl_main_map_handle_drag_down(x, y, false,
                        ev->tfinger.fingerID))
                {
                    return;
                }
                if (sdl_map_touch_handle_pointer_down(x, y,
                        ev->tfinger.fingerID))
                {
                    return;
                }
                if (sdl_touch_swipe_handle_pointer_down(x, y,
                        ev->tfinger.fingerID))
                {
                    return;
                }
                return;
            }
            if (sdl_touch_round_handle_pointer_down(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            return;
        }
        if (sdl_main_screen_handle_corner_exempt_status_pointer(x, y))
            return;
        if (sdl_touch_zone_handle_pointer_down(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_menu_touch_handle_pointer_down(x, y, ev->tfinger.fingerID,
            false))
            return;
        if (sdl_main_screen_handle_menu_outside_pointer(x, y, true))
            return;
        if (ui_scroll_area_get_tap_key()
            && sdl_menu_scroll_handle_pointer_down(x, y, ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_touch_pane_handle_pointer_down(x, y, false, ev->tfinger.fingerID))
            return;
        if (sdl_touch_top_panel_handle_pointer_down(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_menu_scroll_handle_pointer_down(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_main_screen_handle_status_line_pointer(x, y))
            return;
        if (sdl_character_panel_handle_pointer_down(x, y, false,
                ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_main_screen_handle_supporting_pane_pointer(x, y))
            return;
        if (sdl_pointer_dismiss_any_key_prompt())
            return;
        if (sdl_pointer_attack_handle_touch_down(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_main_map_handle_drag_down(x, y, false, ev->tfinger.fingerID))
            return;
        if (sdl_map_touch_handle_pointer_down(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_touch_swipe_handle_pointer_down(x, y, ev->tfinger.fingerID))
        {
            return;
        }
    } else if (ev->type == SDL_EVENT_FINGER_MOTION) {
        float x;
        float y;

        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return;
        sdl_note_touch_event_device(ev->tfinger.touchID);
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return;
        if (sdl_log_pane_menu_handle_long_press_motion(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_side_pane_menu_handle_long_press_motion(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_log_pane_menu_handle_pointer_motion(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_side_pane_menu_handle_pointer_motion(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_welcome_touch_handle_pointer_motion(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_player_action_menu_handle_pointer_motion(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_player_exchange_handle_pointer_motion(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_unified_look_handle_map_drag_motion(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_main_map_handle_drag_motion(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_side_map_pane_handle_pointer_motion(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_pointer_aim_handle_touch_motion(x, y, ev->tfinger.fingerID))
            return;
        if (g_touch_thumb_press.active
            && !g_touch_thumb_press.mouse
            && g_touch_thumb_press.finger_id == ev->tfinger.fingerID)
        {
            (void)sdl_touch_thumb_handle_pointer_motion(x, y, false,
                ev->tfinger.fingerID);
            return;
        }
        if (sdl_touch_round_layer_controls_active()
            || (g_touch_round_press.active
                && g_touch_round_press.finger_id == ev->tfinger.fingerID))
        {
            if (g_touch_pane_press.active
                && !g_touch_pane_press.mouse
                && g_touch_pane_press.finger_id == ev->tfinger.fingerID)
            {
                (void)sdl_touch_pane_handle_pointer_motion(x, y, false,
                    ev->tfinger.fingerID);
                return;
            }
            if (sdl_touch_top_panel_handle_pointer_motion(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_character_panel_handle_pointer_motion(x, y, false,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_pointer_attack_handle_touch_motion(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_map_touch_handle_pointer_motion(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_touch_swipe_handle_pointer_motion(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_touch_round_handle_pointer_motion(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            return;
        }
        if (sdl_menu_touch_handle_pointer_motion(x, y, ev->tfinger.fingerID,
            false))
            return;
        if (sdl_character_panel_handle_pointer_motion(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_touch_pane_handle_pointer_motion(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_touch_top_panel_handle_pointer_motion(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_touch_zone_handle_pointer_motion(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_pointer_attack_handle_touch_motion(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_map_touch_handle_pointer_motion(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_menu_scroll_handle_pointer_motion(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_touch_swipe_handle_pointer_motion(x, y, ev->tfinger.fingerID))
            return;
    } else if (ev->type == SDL_EVENT_FINGER_UP) {
        float x;
        float y;

        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return;
        sdl_note_touch_event_device(ev->tfinger.touchID);
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return;
        if (sdl_log_pane_menu_handle_long_press_up(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_side_pane_menu_handle_long_press_up(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_log_pane_menu_handle_pointer_up(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_side_pane_menu_handle_pointer_up(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_welcome_touch_handle_pointer_up(x, y,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_player_action_menu_handle_pointer_up(x, y,
            ev->tfinger.fingerID, false, false))
        {
            return;
        }
        if (sdl_player_exchange_handle_pointer_up(x, y,
            ev->tfinger.fingerID, false))
        {
            return;
        }
        if (sdl_unified_look_handle_map_drag_up(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_main_map_handle_drag_up(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_side_map_pane_handle_pointer_up(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_pointer_aim_handle_touch_up(x, y, ev->tfinger.fingerID))
            return;
        if (g_touch_thumb_press.active
            && !g_touch_thumb_press.mouse
            && g_touch_thumb_press.finger_id == ev->tfinger.fingerID)
        {
            (void)sdl_touch_thumb_handle_pointer_up(x, y, false,
                ev->tfinger.fingerID);
            return;
        }
        if (sdl_touch_round_layer_controls_active()
            || (g_touch_round_press.active
                && g_touch_round_press.finger_id == ev->tfinger.fingerID))
        {
            if (g_touch_pane_press.active
                && !g_touch_pane_press.mouse
                && g_touch_pane_press.finger_id == ev->tfinger.fingerID)
            {
                sdl_touch_pane_handle_pointer_up(x, y, false,
                    ev->tfinger.fingerID);
                return;
            }
            if (sdl_touch_top_panel_handle_pointer_up(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_character_panel_handle_pointer_up(x, y, false,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_pointer_attack_handle_touch_up(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (sdl_map_touch_handle_pointer_up(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            if (g_touch_swipe.active
                && g_touch_swipe.finger_id == ev->tfinger.fingerID)
            {
                sdl_touch_swipe_handle_pointer_up(x, y,
                    ev->tfinger.fingerID);
                return;
            }
            if (sdl_touch_round_handle_pointer_up(x, y,
                    ev->tfinger.fingerID))
            {
                return;
            }
            return;
        }
        if (sdl_menu_touch_handle_pointer_up(x, y, ev->tfinger.fingerID,
            false))
            return;
        if (sdl_character_panel_handle_pointer_up(x, y, false,
            ev->tfinger.fingerID))
        {
            return;
        }
        if (sdl_touch_top_panel_handle_pointer_up(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_touch_zone_handle_pointer_up(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_menu_scroll_handle_pointer_up(ev->tfinger.fingerID))
            return;
        if (sdl_pointer_attack_handle_touch_up(x, y, ev->tfinger.fingerID))
            return;
        if (sdl_map_touch_handle_pointer_up(x, y, ev->tfinger.fingerID))
            return;
        sdl_touch_swipe_handle_pointer_up(x, y, ev->tfinger.fingerID);
        sdl_touch_pane_handle_pointer_up(x, y, false, ev->tfinger.fingerID);
    } else if (ev->type == SDL_EVENT_FINGER_CANCELED) {
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return;
        sdl_note_touch_event_device(ev->tfinger.touchID);
        if (g_touch_swipe.active && g_touch_swipe.finger_id == ev->tfinger.fingerID)
            sdl_touch_swipe_cancel();
        if (g_welcome_touch_press.active
            && g_welcome_touch_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_welcome_touch_cancel_press();
        }
        if (g_touch_pane_press.active && !g_touch_pane_press.mouse
            && g_touch_pane_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_touch_pane_cancel_press();
        }
        if (g_touch_thumb_press.active && !g_touch_thumb_press.mouse
            && g_touch_thumb_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_touch_thumb_cancel_press();
        }
        if (g_menu_touch_press.active
            && g_menu_touch_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_menu_touch_cancel();
        }
        if (g_character_panel_press.active
            && !g_character_panel_press.mouse
            && g_character_panel_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_character_panel_cancel_press();
        }
        if (g_menu_scroll_drag.active
            && g_menu_scroll_drag.finger_id == ev->tfinger.fingerID)
        {
            sdl_menu_scroll_cancel();
        }
        if (g_map_touch_press.active
            && g_map_touch_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_mouse_path_cancel();
        }
        if (g_unified_look_map_drag.active
            && !g_unified_look_map_drag.mouse
            && g_unified_look_map_drag.finger_id == ev->tfinger.fingerID)
        {
            sdl_unified_look_cancel_map_drag();
        }
        if (g_main_map_drag.active
            && !g_main_map_drag.mouse
            && (g_main_map_drag.finger_id == ev->tfinger.fingerID
                || (g_main_map_drag.pinch_active
                    && g_main_map_drag.pinch_finger_id == ev->tfinger.fingerID)))
        {
            sdl_main_map_cancel_drag();
        }
        sdl_side_map_pane_cancel_pointer(ev->tfinger.fingerID, false);
        sdl_side_pane_menu_cancel_long_press(ev->tfinger.fingerID);
        if (g_side_pane_menu.press_active
            && !g_side_pane_menu.press_mouse
            && g_side_pane_menu.press_finger_id == ev->tfinger.fingerID)
        {
            g_side_pane_menu.press_active = false;
            g_side_pane_menu.press_index = -1;
            g_state.need_present = true;
        }
        sdl_log_pane_menu_cancel_long_press(ev->tfinger.fingerID);
        if (g_log_pane_menu.press_active
            && !g_log_pane_menu.press_mouse
            && g_log_pane_menu.press_finger_id == ev->tfinger.fingerID)
        {
            g_log_pane_menu.press_active = false;
            g_log_pane_menu.press_index = -1;
            g_state.need_present = true;
        }
        if (g_player_action_menu.press_active
            && !g_player_action_menu.press_mouse
            && g_player_action_menu.press_finger_id == ev->tfinger.fingerID)
        {
            sdl_player_action_menu_cancel_press();
        }
        if (g_player_exchange_target.press_active
            && !g_player_exchange_target.press_mouse
            && g_player_exchange_target.press_finger_id == ev->tfinger.fingerID)
        {
            sdl_player_exchange_cancel_press();
        }
        if (g_pointer_aim.touch_press_active
            && g_pointer_aim.touch_finger_id == ev->tfinger.fingerID)
        {
            sdl_pointer_aim_cancel_touch_press();
        }
        if (g_pointer_attack.touch_press_active
            && g_pointer_attack.touch_finger_id == ev->tfinger.fingerID)
        {
            sdl_pointer_attack_cancel_touch_press();
            sdl_pointer_attack_clear_hover();
        }
        if (g_touch_zone_press.active
            && g_touch_zone_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_touch_zone_cancel_press();
        }
        if (g_touch_top_panel_press.active
            && g_touch_top_panel_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_touch_top_panel_cancel_press();
        }
        if (g_touch_round_press.active
            && g_touch_round_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_touch_round_cancel_press();
        }
    } else if (ev->type == SDL_EVENT_KEY_DOWN) {
        int key = ev->key.key;
        if (ev->common.timestamp) {
            Uint64 now_ns = SDL_GetTicksNS();

            if (now_ns > ev->common.timestamp) {
                Uint64 queued_ms =
                    (now_ns - ev->common.timestamp) / 1000000ULL;

                if (queued_ms >= 50) {
                    log_warn("[SLOWINPUT] key waited %llu ms in SDL queue",
                        (unsigned long long)queued_ms);
                }
            }
        }

        if (sdl_keyboard_capture_handle_keydown(&ev->key))
            return;

        // Ignore bare modifiers.
        if (key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
            key == SDLK_LALT || key == SDLK_RALT ||
            key == SDLK_LCTRL || key == SDLK_RCTRL ||
            key == SDLK_LGUI || key == SDLK_RGUI)
        {
            if (key == SDLK_LCTRL || key == SDLK_RCTRL) {
                sdl_pointer_attack_clear_hover();
                sdl_pointer_attack_clear_touch_selection();
                sdl_pointer_attack_cancel_touch_press();
                sdl_mouse_path_cancel();
                g_state.need_present = true;
            }
            return;
        }

        if (g_log_pane_menu.active) {
            sdl_log_pane_menu_cancel();
            if (sdl_key_is_escape_or_back(key))
                return;
        }

        if (g_side_pane_menu.active) {
            sdl_side_pane_menu_cancel();
            if (sdl_key_is_escape_or_back(key))
                return;
        }

        if (g_player_action_menu.active || g_player_exchange_target.active) {
            if (sdl_key_is_escape_or_back(key)) {
                sdl_player_action_menu_cancel();
                sdl_player_exchange_cancel();
                return;
            }

            sdl_player_action_menu_cancel();
            sdl_player_exchange_cancel();
        }

        if (sdl_key_is_escape_or_back(key)) {
            Term_keypress(ESCAPE);
            return;
        }

        /* For letter-based movement presets, Alt+<movement letter> issues that
         * letter's normal command. Runs before the Alt layout shortcuts so a
         * shadowed letter (e.g. Alt+a = activate staff in WASD) wins; unshadowed
         * letters still reach the layout shortcuts below. */
        if (sdl_try_send_shadowed_command_event(&ev->key))
            return;

        /* Handle SDL layout shortcuts before menu/game input routing so they
         * work from the initial menu onward. */
        if (sdl_handle_global_layout_shortcut(&ev->key))
            return;

        // Keep other Alt-based key handling limited to the dungeon.
        bool alt = ev->key.mod & SDL_KMOD_ALT;
        if (alt && !character_dungeon)
            return;

        if (character_dungeon) {
            if (sdl_handle_jewelry_preset_shortcut(&ev->key))
                return;
            if (sdl_try_send_movement_event(&ev->key))
                return;
            if (sdl_try_send_preset_command_alias(&ev->key))
                return;
            if (sdl_try_send_modified_direction_event(&ev->key))
                return;
        }

        if (SDL_isprint(ev->key.key)) {
            /* If Ctrl+letter (no Alt/GUI), send the corresponding control char
             * (so Ctrl-A -> ASCII 1) to preserve traditional control bindings
             * like Ctrl-A for staff swapping. Unsupported modified printable
             * keys are ignored by the legacy byte bridge. */
            bool shift = ev->key.mod & SDL_KMOD_SHIFT;
            bool ctrl = ev->key.mod & SDL_KMOD_CTRL;
            bool alt = ev->key.mod & SDL_KMOD_ALT;
            bool gui = ev->key.mod & SDL_KMOD_GUI;
            if (ctrl && !alt && !gui && SDL_isalpha(key)) {
                /* Map to control character */
                Term_keypress(KTRL(key));
            } else if (ctrl || alt || gui) {
                sdl_send_macro_key(key, shift, ctrl || gui, alt);
            } else {
                if (shift) {
                    if (SDL_isalpha(key)) {
                        key = SDL_toupper(key);
                    } else {
                        const char shifted[256] = {
                            ['1'] = '!', ['2'] = '@', ['3'] = '#', ['4'] = '$', ['5'] = '%',
                            ['6'] = '^', ['7'] = '&', ['8'] = '*', ['9'] = '(', ['0'] = ')',
                            ['-'] = '_', ['='] = '+',
                            [','] = '<', ['.'] = '>', ['/'] = '?',
                            ['['] = '{', [']'] = '}',
                            [';'] = ':', ['\''] = '"', ['\\'] = '|',
                            ['`'] = '~',
                        };
                        if (shifted[key])
                            key = shifted[key];
                    }
                }
                Term_keypress(key);
            }
        } else {
            bool shift = ev->key.mod & SDL_KMOD_SHIFT;
            bool alt = ev->key.mod & SDL_KMOD_ALT;
            bool ctrl = ev->key.mod & SDL_KMOD_CTRL;
            bool gui = ev->key.mod & SDL_KMOD_GUI;
            bool mod = shift || alt || ctrl || gui;
            switch (key) {
                case SDLK_UP:
                case SDLK_KP_8:
                    key = '8';
                    break;
                case SDLK_DOWN:
                case SDLK_KP_2:
                    key = '2';
                    break;
                case SDLK_LEFT:
                case SDLK_KP_4:
                    key = '4';
                    break;
                case SDLK_RIGHT:
                case SDLK_KP_6:
                    key = '6';
                    break;
                case SDLK_KP_1:
                case SDLK_END:
                    key = '1';
                    break;
                case SDLK_KP_3:
                case SDLK_PAGEDOWN:
                    key = '3';
                    break;
                case SDLK_KP_7:
                case SDLK_HOME:
                    key = '7';
                    break;
                case SDLK_KP_9:
                case SDLK_PAGEUP:
                    key = '9';
                    break;
                case SDLK_KP_5:
                    key = '5';
                    break;
            }
            if (key <= 0 || key >= 256)
                return;
            if (mod) {
                sdl_send_macro_key(key, shift, ctrl || gui, alt);
            } else {
                Term_keypress(key);
            }
        }
    } else if (ev->type == SDL_EVENT_KEY_UP) {
        int key = ev->key.key;

        if (key == SDLK_LCTRL || key == SDLK_RCTRL) {
            sdl_pointer_attack_clear_hover();
            sdl_pointer_attack_clear_touch_selection();
            sdl_pointer_attack_cancel_touch_press();
            sdl_mouse_path_cancel();
            g_state.need_present = true;
        }
    } else if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        sdl_gamepad_handle_button(&ev->gbutton);
    } else if (ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        sdl_gamepad_handle_axis(&ev->gaxis);
    } else if (ev->type == SDL_EVENT_GAMEPAD_ADDED || ev->type == SDL_EVENT_GAMEPAD_REMOVED
        || ev->type == SDL_EVENT_GAMEPAD_REMAPPED) {
        sdl_gamepad_handle_device(&ev->gdevice);
    } else if (ev->type == SDL_EVENT_WINDOW_RESIZED
        || ev->type == SDL_EVENT_WINDOW_SAFE_AREA_CHANGED) {
        log_debug("window resized to %dx%d", ev->window.data1, ev->window.data2);
        sdl_refresh_safe_area();
        sdl_refresh_platform_max_main_view_scales_for_current_layout(
            "window resize");
        (void)sdl_recover_layout_for_current_window("window resize", true, NULL);
        sdl_clamp_main_view_zoom_to_current_layout();
#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
        (void)sdl_mobile_maybe_apply_first_start_auto_scale("window resize");
#endif
        {
            SDL_Rect screen = sdl_get_layout_screen_rect();

            log_debug("new layout size %dx%d at (%d,%d)",
                screen.w, screen.h, screen.x, screen.y);
            resize(&screen);
        }
    } else if (ev->type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED ||
        ev->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
        ev->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {

        float scale = SDL_GetWindowDisplayScale(g_state.window);
        bool scale_changed = (scale != g_state.system_scale);

        if (scale_changed) {
            log_info("new system scale is %g", scale);
            g_state.system_scale = scale;
            sdl_load_story_fonts();
        }

        sdl_refresh_safe_area();
        sdl_refresh_platform_max_main_view_scales_for_current_layout(
            "display scale change");
        (void)sdl_recover_layout_for_current_window("display scale change",
            true, NULL);
        sdl_clamp_main_view_zoom_to_current_layout();
#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
        (void)sdl_mobile_maybe_apply_first_start_auto_scale("display scale change");
#endif
        {
            SDL_Rect screen = sdl_get_layout_screen_rect();

            log_debug("window pixel/display update layout=%dx%d at (%d,%d) (scale_changed=%d)",
                screen.w, screen.h, screen.x, screen.y, scale_changed ? 1 : 0);
            resize(&screen);
        }
    }
    // Handle GPU reset events (commonly triggered by NVIDIA drivers on mode switches,
    // driver updates, or sleep/wake cycles)
    else if (ev->type == SDL_EVENT_RENDER_DEVICE_RESET ||
             ev->type == SDL_EVENT_RENDER_TARGETS_RESET) {
        log_warn("Renderer device/targets reset detected - recreating textures");
        sdl_handle_renderer_reset();
    }
    // Handle window restored (after minimize/alt-tab on some systems)
    else if (ev->type == SDL_EVENT_WINDOW_RESTORED ||
             ev->type == SDL_EVENT_WINDOW_EXPOSED) {
        log_debug("Window restored/exposed - forcing redraw");
        g_state.need_present = true;
        Term_redraw();
    }
}
