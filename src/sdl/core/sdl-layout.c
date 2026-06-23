#include "angband.h"
#include "sdl/main-sdl-private.h"

static term g_left_panel_source_term;
static bool g_left_panel_source_term_ready = false;
static Uint64 g_left_panel_source_generation = 1;
static Uint64 g_left_panel_source_rendered_generation = 0;
static int g_left_panel_source_w = 0;
static int g_left_panel_source_h = 0;

sdl_view* sdl_view_from_term(term* t)
{
    if (!t)
        return NULL;

    size_t idx = (size_t)(uintptr_t)t->data;
    if (idx >= MAX_TERM_DATA) {
        log_warn("sdl_view_from_term: invalid term index %zu (max %d)", idx, MAX_TERM_DATA - 1);
        return NULL;
    }

    return &g_views[idx];
}

enum pane_type sdl_view_pane_type(const sdl_view* view)
{
    if (!view)
        return PANE_MAX;

    for (int i = 0; i < MAX_TERM_DATA && i < PANE_MAX; i++) {
        if (view == &g_views[i])
            return (enum pane_type)i;
    }

    return PANE_MAX;
}

bool sdl_view_is_overlay_log_pane(const sdl_view* view)
{
    enum pane_type type = sdl_view_pane_type(view);

    if (type != PANE_ROLLS)
        return false;

    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane != PANE_ROLLS)
            continue;

        return pane_config[i].enabled
            && pane_placement_is_overlay(pane_config[i].where);
    }

    return false;
}

Uint8 sdl_view_background_alpha(const sdl_view* view)
{
    return sdl_view_is_overlay_log_pane(view)
        ? SDL_OVERLAY_LOG_PANE_ALPHA
        : SDL_ALPHA_OPAQUE;
}

bool sdl_rect_has_area(const SDL_Rect* rect)
{
    return (rect && rect->w > 0 && rect->h > 0);
}

SDL_Rect sdl_get_window_pixel_rect(void)
{
    SDL_Rect rect = { 0 };

    if (g_state.window)
        SDL_GetWindowSizeInPixels(g_state.window, &rect.w, &rect.h);

    return rect;
}

SDL_Rect sdl_window_rect_to_pixel_rect(const SDL_Rect* rect)
{
    SDL_Rect pixel_rect = { 0 };
    SDL_Rect pixel_window = sdl_get_window_pixel_rect();
    int window_w = 0;
    int window_h = 0;

    if (!rect)
        return pixel_rect;

    pixel_rect = *rect;

    if (!g_state.window || !sdl_rect_has_area(&pixel_window))
        return pixel_rect;

    SDL_GetWindowSize(g_state.window, &window_w, &window_h);
    if (window_w <= 0 || window_h <= 0)
        return pixel_rect;

    if (window_w != pixel_window.w || window_h != pixel_window.h) {
        float scale_x = (float)pixel_window.w / (float)window_w;
        float scale_y = (float)pixel_window.h / (float)window_h;
        int x1 = (int)SDL_ceilf((float)rect->x * scale_x);
        int y1 = (int)SDL_ceilf((float)rect->y * scale_y);
        int x2 = (int)SDL_floorf((float)(rect->x + rect->w) * scale_x);
        int y2 = (int)SDL_floorf((float)(rect->y + rect->h) * scale_y);

        pixel_rect.x = x1;
        pixel_rect.y = y1;
        pixel_rect.w = x2 - x1;
        pixel_rect.h = y2 - y1;
    }

    if (pixel_rect.x < 0)
        pixel_rect.x = 0;
    if (pixel_rect.y < 0)
        pixel_rect.y = 0;
    if (pixel_rect.x > pixel_window.w)
        pixel_rect.x = pixel_window.w;
    if (pixel_rect.y > pixel_window.h)
        pixel_rect.y = pixel_window.h;
    if (pixel_rect.x + pixel_rect.w > pixel_window.w)
        pixel_rect.w = pixel_window.w - pixel_rect.x;
    if (pixel_rect.y + pixel_rect.h > pixel_window.h)
        pixel_rect.h = pixel_window.h - pixel_rect.y;
    if (pixel_rect.w < 0)
        pixel_rect.w = 0;
    if (pixel_rect.h < 0)
        pixel_rect.h = 0;

    return pixel_rect;
}

#if defined(SDL_PLATFORM_ANDROID)
int sdl_android_sdk_int(JNIEnv* env)
{
    int sdk = 0;
    jclass version_class = NULL;
    jfieldID sdk_field = NULL;

    if (!env)
        return 0;

    version_class = (*env)->FindClass(env, "android/os/Build$VERSION");
    if (!version_class)
        return 0;

    sdk_field = (*env)->GetStaticFieldID(env, version_class, "SDK_INT", "I");
    if (sdk_field)
        sdk = (*env)->GetStaticIntField(env, version_class, sdk_field);

    (*env)->DeleteLocalRef(env, version_class);
    return sdk;
}

void sdl_android_clear_pending_exception(JNIEnv* env)
{
    if (env && (*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
}

bool sdl_android_has_controller_device(void)
{
    enum {
        ANDROID_SOURCE_GAMEPAD = 0x00000401,
        ANDROID_SOURCE_JOYSTICK = 0x01000010,
    };
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jclass input_device_class = NULL;
    jmethodID get_device_ids = NULL;
    jmethodID get_device = NULL;
    jmethodID get_sources = NULL;
    jintArray ids = NULL;
    jint* id_values = NULL;
    jsize count = 0;
    bool has_controller = false;

    if (!env)
        return false;

    input_device_class = (*env)->FindClass(env, "android/view/InputDevice");
    if (!input_device_class) {
        sdl_android_clear_pending_exception(env);
        return false;
    }

    get_device_ids = (*env)->GetStaticMethodID(env, input_device_class,
        "getDeviceIds", "()[I");
    get_device = (*env)->GetStaticMethodID(env, input_device_class,
        "getDevice", "(I)Landroid/view/InputDevice;");
    get_sources = (*env)->GetMethodID(env, input_device_class,
        "getSources", "()I");
    if (!get_device_ids || !get_device || !get_sources) {
        sdl_android_clear_pending_exception(env);
        goto cleanup;
    }

    ids = (jintArray)(*env)->CallStaticObjectMethod(env, input_device_class,
        get_device_ids);
    if ((*env)->ExceptionCheck(env) || !ids) {
        sdl_android_clear_pending_exception(env);
        goto cleanup;
    }

    count = (*env)->GetArrayLength(env, ids);
    id_values = (*env)->GetIntArrayElements(env, ids, NULL);
    if (!id_values) {
        sdl_android_clear_pending_exception(env);
        goto cleanup;
    }

    for (jsize i = 0; i < count; i++) {
        jobject device = (*env)->CallStaticObjectMethod(env, input_device_class,
            get_device, id_values[i]);
        jint sources;

        if ((*env)->ExceptionCheck(env)) {
            sdl_android_clear_pending_exception(env);
            continue;
        }
        if (!device)
            continue;

        sources = (*env)->CallIntMethod(env, device, get_sources);
        if ((*env)->ExceptionCheck(env)) {
            sdl_android_clear_pending_exception(env);
            (*env)->DeleteLocalRef(env, device);
            continue;
        }

        /* Phones commonly expose a virtual keyboard with SOURCE_DPAD.  That is
         * navigation input, not evidence of a handheld/gamepad profile. */
        if (((sources & ANDROID_SOURCE_GAMEPAD) == ANDROID_SOURCE_GAMEPAD)
            || ((sources & ANDROID_SOURCE_JOYSTICK) == ANDROID_SOURCE_JOYSTICK))
        {
            has_controller = true;
            (*env)->DeleteLocalRef(env, device);
            break;
        }

        (*env)->DeleteLocalRef(env, device);
    }

cleanup:
    if (id_values && ids)
        (*env)->ReleaseIntArrayElements(env, ids, id_values, JNI_ABORT);
    if (ids)
        (*env)->DeleteLocalRef(env, ids);
    if (input_device_class)
        (*env)->DeleteLocalRef(env, input_device_class);

    return has_controller;
}

SDL_Rect sdl_get_android_display_cutout_rect(void)
{
    SDL_Rect rect = sdl_get_window_pixel_rect();
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    jclass activity_class = NULL;
    jclass window_class = NULL;
    jclass view_class = NULL;
    jclass insets_class = NULL;
    jclass cutout_class = NULL;
    jobject window = NULL;
    jobject decor_view = NULL;
    jobject insets = NULL;
    jobject cutout = NULL;

    if (!env || !activity || !sdl_rect_has_area(&rect))
        goto cleanup;

    if (sdl_android_sdk_int(env) < 28)
        goto cleanup;

    activity_class = (*env)->GetObjectClass(env, activity);
    if (!activity_class)
        goto cleanup;

    {
        jmethodID get_window = (*env)->GetMethodID(env, activity_class, "getWindow",
            "()Landroid/view/Window;");
        if (!get_window)
            goto cleanup;
        window = (*env)->CallObjectMethod(env, activity, get_window);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }
        if (!window)
            goto cleanup;
    }

    window_class = (*env)->GetObjectClass(env, window);
    if (!window_class)
        goto cleanup;

    {
        jmethodID get_decor_view = (*env)->GetMethodID(env, window_class,
            "getDecorView", "()Landroid/view/View;");
        if (!get_decor_view)
            goto cleanup;
        decor_view = (*env)->CallObjectMethod(env, window, get_decor_view);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }
        if (!decor_view)
            goto cleanup;
    }

    view_class = (*env)->GetObjectClass(env, decor_view);
    if (!view_class)
        goto cleanup;

    {
        jmethodID get_root_window_insets = (*env)->GetMethodID(env, view_class,
            "getRootWindowInsets", "()Landroid/view/WindowInsets;");
        if (!get_root_window_insets)
            goto cleanup;
        insets = (*env)->CallObjectMethod(env, decor_view, get_root_window_insets);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }
        if (!insets)
            goto cleanup;
    }

    insets_class = (*env)->GetObjectClass(env, insets);
    if (!insets_class)
        goto cleanup;

    {
        jmethodID get_display_cutout = (*env)->GetMethodID(env, insets_class,
            "getDisplayCutout", "()Landroid/view/DisplayCutout;");
        if (!get_display_cutout)
            goto cleanup;
        cutout = (*env)->CallObjectMethod(env, insets, get_display_cutout);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }
        if (!cutout)
            goto cleanup;
    }

    cutout_class = (*env)->GetObjectClass(env, cutout);
    if (!cutout_class)
        goto cleanup;

    {
        jmethodID get_safe_inset_left = (*env)->GetMethodID(env, cutout_class,
            "getSafeInsetLeft", "()I");
        jmethodID get_safe_inset_top = (*env)->GetMethodID(env, cutout_class,
            "getSafeInsetTop", "()I");
        jmethodID get_safe_inset_right = (*env)->GetMethodID(env, cutout_class,
            "getSafeInsetRight", "()I");
        jmethodID get_safe_inset_bottom = (*env)->GetMethodID(env, cutout_class,
            "getSafeInsetBottom", "()I");
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;

        if (!get_safe_inset_left || !get_safe_inset_top
            || !get_safe_inset_right || !get_safe_inset_bottom)
            goto cleanup;

        left = (*env)->CallIntMethod(env, cutout, get_safe_inset_left);
        top = (*env)->CallIntMethod(env, cutout, get_safe_inset_top);
        right = (*env)->CallIntMethod(env, cutout, get_safe_inset_right);
        bottom = (*env)->CallIntMethod(env, cutout, get_safe_inset_bottom);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            goto cleanup;
        }

        if (left < 0)
            left = 0;
        if (top < 0)
            top = 0;
        if (right < 0)
            right = 0;
        if (bottom < 0)
            bottom = 0;
        if (left + right >= rect.w || top + bottom >= rect.h)
            goto cleanup;

        rect.x = left;
        rect.y = top;
        rect.w -= left + right;
        rect.h -= top + bottom;
    }

cleanup:
    if (env) {
        if (cutout_class)
            (*env)->DeleteLocalRef(env, cutout_class);
        if (cutout)
            (*env)->DeleteLocalRef(env, cutout);
        if (insets_class)
            (*env)->DeleteLocalRef(env, insets_class);
        if (insets)
            (*env)->DeleteLocalRef(env, insets);
        if (view_class)
            (*env)->DeleteLocalRef(env, view_class);
        if (decor_view)
            (*env)->DeleteLocalRef(env, decor_view);
        if (window_class)
            (*env)->DeleteLocalRef(env, window_class);
        if (window)
            (*env)->DeleteLocalRef(env, window);
        if (activity_class)
            (*env)->DeleteLocalRef(env, activity_class);
        if (activity)
            (*env)->DeleteLocalRef(env, activity);
    }

    return rect;
}
#endif

void sdl_refresh_safe_area(void)
{
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();
    SDL_Rect safe_area = window_pixels;

    if (!g_state.window || !sdl_rect_has_area(&window_pixels)) {
        g_state.safe_area = window_pixels;
        return;
    }

    {
        SDL_Rect window_units = { 0 };
        SDL_Rect safe_units = { 0 };

        SDL_GetWindowSize(g_state.window, &window_units.w, &window_units.h);
        if (window_units.w > 0 && window_units.h > 0
            && SDL_GetWindowSafeArea(g_state.window, &safe_units)
            && safe_units.x >= 0
            && safe_units.y >= 0
            && safe_units.w > 0
            && safe_units.h > 0
            && safe_units.x + safe_units.w <= window_units.w
            && safe_units.y + safe_units.h <= window_units.h)
        {
            safe_area = sdl_window_rect_to_pixel_rect(&safe_units);
            if (!sdl_rect_has_area(&safe_area))
                safe_area = window_pixels;
#if defined(SDL_PLATFORM_ANDROID)
            else if (!config.use_unsafe_area)
                safe_area = sdl_get_android_display_cutout_rect();
#elif defined(SIL_IOS)
            else if (!config.use_unsafe_area) {
                int li = 0, ri = 0, ti = 0, bi = 0;
                if (sdl_ios_get_safe_area_insets(g_state.window,
                        &li, &ri, &ti, &bi))
                {
                    SDL_Rect ios_units = {
                        li, ti,
                        window_units.w - li - ri,
                        window_units.h - ti - bi
                    };
                    if (ios_units.w > 0 && ios_units.h > 0) {
                        SDL_Rect ios_pixels = sdl_window_rect_to_pixel_rect(
                            &ios_units);
                        if (sdl_rect_has_area(&ios_pixels))
                            safe_area = ios_pixels;
                    }
                }
            }
#endif
        }
    }

    if (SDL_memcmp(&g_state.safe_area, &safe_area, sizeof(safe_area)) != 0) {
        log_info("SDL layout safe area updated to (%d,%d %dx%d)",
            safe_area.x, safe_area.y, safe_area.w, safe_area.h);
    }

    g_state.safe_area = safe_area;
}

SDL_Rect sdl_get_layout_screen_rect(void)
{
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();

    if (config.use_unsafe_area)
        return window_pixels;

    if (!sdl_rect_has_area(&g_state.safe_area))
        sdl_refresh_safe_area();

    if (sdl_rect_has_area(&g_state.safe_area)) {
        SDL_Rect safe = g_state.safe_area;

        if (safe.x < 0)
            safe.x = 0;
        if (safe.y < 0)
            safe.y = 0;
        if (safe.x > window_pixels.w)
            safe.x = window_pixels.w;
        if (safe.y > window_pixels.h)
            safe.y = window_pixels.h;
        if (safe.x + safe.w > window_pixels.w)
            safe.w = window_pixels.w - safe.x;
        if (safe.y + safe.h > window_pixels.h)
            safe.h = window_pixels.h - safe.y;

        if (sdl_rect_has_area(&safe))
            return safe;
    }

    return window_pixels;
}

bool sdl_mobile_prefer_safe_edge_alignment(void)
{
#if defined(__ANDROID__) || defined(SIL_IOS)
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();

    if (config.use_unsafe_area)
        return false;
    if (!sdl_rect_has_area(&g_state.safe_area) || !sdl_rect_has_area(&window_pixels))
        return false;

    return (g_state.safe_area.x != 0
        || g_state.safe_area.y != 0
        || g_state.safe_area.w != window_pixels.w
        || g_state.safe_area.h != window_pixels.h);
#else
    return false;
#endif
}

void sdl_resize_for_current_layout(void)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();

    if (!sdl_rect_has_area(&screen))
        return;

    resize(&screen);
}

/*
 * Synchronize the SDL palette from angband_color_table.
 * This allows runtime palette changes to propagate to SDL.
 */
void sdl_sync_palette(void)
{
    for (int i = 0; i < 16; i++) {
        g_state.palette[i].r = angband_color_table[i][1];
        g_state.palette[i].g = angband_color_table[i][2];
        g_state.palette[i].b = angband_color_table[i][3];
        g_state.palette[i].a = 255;
    }
}

void sdl_view_destroy(sdl_view* d)
{
    if (d->canvas) {
        SDL_DestroyTexture(d->canvas);
        d->canvas = NULL;
    }
    if (d->font_atlas) {
        if (!d->font_atlas_cached)
            SDL_DestroyTexture(d->font_atlas);
        d->font_atlas = NULL;
    }
    d->font_atlas_cached = false;
    d->font_atlas_exact = false;
    d->font_atlas_cell_w = 0;
    d->font_atlas_cell_h = 0;
}

void sdl_left_panel_canvas_destroy(void)
{
    if (g_left_panel_canvas) {
        SDL_DestroyTexture(g_left_panel_canvas);
        g_left_panel_canvas = NULL;
    }
    g_left_panel_canvas_w = 0;
    g_left_panel_canvas_h = 0;
    if (g_left_panel_source_term_ready) {
        term_nuke(&g_left_panel_source_term);
        g_left_panel_source_term_ready = false;
    }
    g_left_panel_source_w = 0;
    g_left_panel_source_h = 0;
    g_left_panel_source_rendered_generation = 0;
}

int sdl_overlay_margin_px(void)
{
    int margin_px = (int)(g_state.system_scale * config.margin);

    return (margin_px > 0) ? margin_px * 5 : 0;
}

int sdl_overlay_edge_gap_px(int area_px, int content_px)
{
    int margin_px = sdl_overlay_margin_px();
    int max_gap;

    if (margin_px <= 0 || area_px <= content_px)
        return 0;

    max_gap = (area_px - content_px) / 2;
    if (max_gap <= 0)
        return 0;

    return (margin_px < max_gap) ? margin_px : max_gap;
}

bool sdl_pane_default_enabled_on_migration(enum pane_type pane)
{
    return pane == PANE_SUPPLY || pane == PANE_LEFT_PANEL || pane == PANE_STATUS
        || pane == PANE_DEPTH || pane == PANE_COMBAT || pane == PANE_ROLLS
        || pane == PANE_LOG || pane == PANE_DESCRIPTION
        || pane == PANE_OVERLAY_MENU;
}

bool sdl_migrate_legacy_main_menu_depth_pane(
    struct pane_config* configs, int count)
{
    int legacy_idx = -1;
    bool has_depth = false;

    if (!configs || count <= 0)
        return false;

    for (int i = 0; i < count; i++) {
        if (configs[i].pane == PANE_DEPTH) {
            has_depth = true;
            break;
        }
        if (configs[i].pane == PANE_MAIN_MENU && legacy_idx < 0)
            legacy_idx = i;
    }

    if (has_depth || legacy_idx < 0)
        return false;

    configs[legacy_idx].pane = PANE_DEPTH;
    if (configs[legacy_idx].rect.rows < 4)
        configs[legacy_idx].rect.rows = 4;
    if (configs[legacy_idx].rect.cols < 12)
        configs[legacy_idx].rect.cols = 12;
    log_info("Migrated legacy MAIN_MENU pane config to DEPTH pane");
    return true;
}

static bool sdl_remove_obsolete_main_menu_pane_configs(
    struct pane_config* configs, int* config_count)
{
    int write = 0;
    bool changed = false;

    if (!configs || !config_count)
        return false;
    if (*config_count < 0)
        *config_count = 0;
    if (*config_count > MAX_PANE_CONFIGS)
        *config_count = MAX_PANE_CONFIGS;

    for (int read = 0; read < *config_count; read++) {
        if (configs[read].pane == PANE_MAIN_MENU) {
            changed = true;
            continue;
        }

        if (write != read)
            configs[write] = configs[read];
        write++;
    }

    if (changed) {
        memset(configs + write, 0,
            sizeof(struct pane_config) * (MAX_PANE_CONFIGS - write));
        *config_count = write;
        log_info("Removed obsolete MAIN_MENU pane config");
    }

    return changed;
}

bool sdl_ensure_default_pane_config_entries(struct pane_config* configs,
    int* config_count, bool enable_new_panes)
{
    bool changed = false;

    if (!configs || !config_count)
        return false;

    if (*config_count < 0)
        *config_count = 0;
    if (*config_count > MAX_PANE_CONFIGS)
        *config_count = MAX_PANE_CONFIGS;

    if (sdl_migrate_legacy_main_menu_depth_pane(configs, *config_count))
        changed = true;
    if (sdl_remove_obsolete_main_menu_pane_configs(configs, config_count))
        changed = true;

    for (int i = 0; i < default_pane_config_count; i++) {
        bool found = false;
        enum pane_type pane = default_pane_config[i].pane;

        for (int j = 0; j < *config_count; j++) {
            if (configs[j].pane == pane) {
                if (pane == PANE_LEFT_PANEL) {
                    if (!configs[j].enabled) {
                        configs[j].enabled = true;
                        changed = true;
                    }
                    if (configs[j].rect.rows != 0 || configs[j].rect.cols != 0) {
                        configs[j].rect.rows = 0;
                        configs[j].rect.cols = 0;
                        changed = true;
                    }
                }
                if (pane == PANE_DEPTH && configs[j].rect.rows < 4) {
                    configs[j].rect.rows = 4;
                    changed = true;
                }
                if (pane == PANE_COMBAT) {
                    if (configs[j].rect.rows < PANE_COMBAT_OVERLAY_ROWS) {
                        configs[j].rect.rows = PANE_COMBAT_OVERLAY_ROWS;
                        changed = true;
                    }
                    if (configs[j].rect.cols < PANE_COMBAT_OVERLAY_COLS) {
                        configs[j].rect.cols = PANE_COMBAT_OVERLAY_COLS;
                        changed = true;
                    }
                }
                found = true;
                break;
            }
        }

        if (found)
            continue;

        if (*config_count >= MAX_PANE_CONFIGS) {
            log_warn("Could not append pane %d; max pane count reached",
                pane);
            return changed;
        }

        configs[*config_count] = default_pane_config[i];
        configs[*config_count].enabled =
            sdl_pane_default_enabled_on_migration(pane)
                ? default_pane_config[i].enabled
                : (enable_new_panes && default_pane_config[i].enabled);
        (*config_count)++;
        changed = true;
    }

    return changed;
}

#if SIL_SDL_MOBILE_BUILD
void sdl_ensure_default_pane_configs_present(bool enable_new_panes)
{
    (void)sdl_ensure_default_pane_config_entries(pane_config,
        &pane_config_count, enable_new_panes);
}
#endif

void sdl_ensure_default_pane_profiles_present(bool enable_new_panes)
{
    for (int mode = 0; mode < SDL_PANE_PROFILE_COUNT; mode++) {
        (void)sdl_ensure_default_pane_config_entries(
            g_pane_profiles[mode].pane_configs,
            &g_pane_profiles[mode].pane_count, enable_new_panes);
    }
}

void sdl_ensure_touch_pane_config_present(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_TOUCH)
            return;
    }

    if (pane_config_count >= MAX_PANE_CONFIGS) {
        log_warn("Could not append touch pane config; max pane count reached");
        return;
    }

    pane_config[pane_config_count++] = (struct pane_config){
        .pane = PANE_TOUCH,
        .where = PLACE_DOUBLE_RIGHT,
        .enabled = false,
        .rect = { .rows = 0, .cols = 0 },
        .ratio = 0.0f,
    };
}

int sdl_left_panel_pane_config_index(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_LEFT_PANEL)
            return i;
    }

    return -1;
}

enum pane_placement sdl_left_panel_pane_placement(void)
{
    int index = sdl_left_panel_pane_config_index();
    enum pane_placement placement = PLACE_TOP_LEFT;

    if (index >= 0)
        placement = pane_config[index].where;
    if (!pane_type_allows_placement(PANE_LEFT_PANEL, placement))
        placement = PLACE_TOP_LEFT;

    return placement;
}

bool sdl_left_panel_pane_placement_is_right(enum pane_placement where)
{
    return where == PLACE_TOP_RIGHT || where == PLACE_RIGHT_CENTER
        || where == PLACE_BOTTOM_RIGHT;
}

bool sdl_left_panel_pane_placement_is_horizontal_center(
    enum pane_placement where)
{
    return where == PLACE_TOP_CENTER || where == PLACE_BOTTOM_CENTER;
}

bool sdl_left_panel_pane_placement_is_bottom(enum pane_placement where)
{
    return where == PLACE_BOTTOM_LEFT || where == PLACE_BOTTOM_CENTER
        || where == PLACE_BOTTOM_RIGHT;
}

bool sdl_left_panel_pane_placement_is_vertical_center(
    enum pane_placement where)
{
    return where == PLACE_LEFT_CENTER || where == PLACE_RIGHT_CENTER;
}

bool sdl_left_panel_pane_config_enabled(void)
{
    int index = sdl_left_panel_pane_config_index();

    if (index < 0)
        return true;
    if (pane_config[index].pane == PANE_LEFT_PANEL)
        return true;

    return pane_config[index].enabled;
}

bool sdl_left_panel_cell_has_visible_content(byte a, char c)
{
    if ((a == 255) && ((byte)c == 0xFF))
        return false;

    if (g_state.use_tiles && g_state.tileset
        && (a & TILE_FLAG) && (((byte)c) & TILE_FLAG))
    {
        return true;
    }

    return c != '\0' && c != ' ';
}

void sdl_left_panel_debug_log_cell_size(int content_cols,
    int initial_cell_w, int initial_cell_h, int final_cell_w,
    int final_cell_h, int visual_cols, int visual_w, int border_cols,
    int available_w, int max_cell_w)
{
    static bool have_last = false;
    static int last_content_cols;
    static int last_initial_cell_w;
    static int last_initial_cell_h;
    static int last_final_cell_w;
    static int last_final_cell_h;
    static int last_visual_cols;
    static int last_visual_w;
    static int last_border_cols;
    static int last_available_w;
    static int last_max_cell_w;

    if (have_last
        && last_content_cols == content_cols
        && last_initial_cell_w == initial_cell_w
        && last_initial_cell_h == initial_cell_h
        && last_final_cell_w == final_cell_w
        && last_final_cell_h == final_cell_h
        && last_visual_cols == visual_cols
        && last_visual_w == visual_w
        && last_border_cols == border_cols
        && last_available_w == available_w
        && last_max_cell_w == max_cell_w)
    {
        return;
    }

    have_last = true;
    last_content_cols = content_cols;
    last_initial_cell_w = initial_cell_w;
    last_initial_cell_h = initial_cell_h;
    last_final_cell_w = final_cell_w;
    last_final_cell_h = final_cell_h;
    last_visual_cols = visual_cols;
    last_visual_w = visual_w;
    last_border_cols = border_cols;
    last_available_w = available_w;
    last_max_cell_w = max_cell_w;

    log_debug("left-panel cell-size: content_cols=%d border_cols=%d "
        "initial_cell=%dx%d final_cell=%dx%d visual_cols=%d visual_w=%d "
        "available_w=%d max_cell_w=%d",
        content_cols, border_cols, initial_cell_w, initial_cell_h,
        final_cell_w, final_cell_h, visual_cols, visual_w, available_w,
        max_cell_w);
}

void sdl_left_panel_pane_cell_size_for_view(const sdl_view* view,
    int content_cols, int* out_cell_w, int* out_cell_h)
{
    int cell_h = sdl_effective_pane_cell_height_for_type(PANE_LEFT_PANEL);
    int cell_w = cell_h / 2;
    int initial_cell_h = cell_h;
    int initial_cell_w = cell_w;
    int visual_cols = 0;
    int visual_w = 0;
    int border_cols = sdl_left_panel_pane_has_border_columns() ? 2 : 0;
    int available_w = 0;
    int max_cell_w = 0;

    if (content_cols < 1)
        content_cols = LEFT_PANEL_CONTENT_WID;
    if (cell_h < 1)
        cell_h = 1;
    if (cell_w < 1)
        cell_w = 1;
    if (initial_cell_h < 1)
        initial_cell_h = 1;
    if (initial_cell_w < 1)
        initial_cell_w = 1;

    if (view && view->rect.w > 0 && view->cell_w > 0) {
        visual_cols = sdl_main_view_visual_cols_for_width(view->rect.w,
            view->cell_w);
        visual_w = visual_cols * view->cell_w;
        available_w = visual_w - 1;
        max_cell_w = (available_w > 0)
            ? available_w / (content_cols + border_cols)
            : 1;

        if (max_cell_w < 1)
            max_cell_w = 1;
        if (cell_w > max_cell_w) {
            cell_w = max_cell_w;
            cell_h = cell_w * 2;
            if (cell_h < 1)
                cell_h = 1;
        }
    }

    sdl_left_panel_debug_log_cell_size(content_cols, initial_cell_w,
        initial_cell_h, cell_w, cell_h, visual_cols, visual_w, border_cols,
        available_w, max_cell_w);

    if (out_cell_w)
        *out_cell_w = cell_w;
    if (out_cell_h)
        *out_cell_h = cell_h;
}

int sdl_left_panel_pane_rows_for_view(const sdl_view* view)
{
    bool compact = (view && view->rows > 0 && view->rows < 24);
    int row_song = compact ? 18 : 21;
    int row_cut = compact ? 17 : 20;
    int row_poisoned = compact ? 17 : 20;
    int row_resist = compact ? 15 : 17;
    int row_info = compact ? 15 : 17;
    int row_evn = compact ? 14 : 16;
    int rows = row_song + 2;

    if (sdl_left_panel_pane_collapsed())
        return SDL_LEFT_PANEL_COLLAPSED_ROWS;

    if (rows < row_cut + 1)
        rows = row_cut + 1;
    if (rows < row_poisoned + 1)
        rows = row_poisoned + 1;
    if (rows < row_resist + 1)
        rows = row_resist + 1;
    if (rows < row_info + 1)
        rows = row_info + 1;
    if (rows < row_evn + 1)
        rows = row_evn + 1;
    if (rows < ROW_MAP + 1)
        rows = ROW_MAP + 1;

    return rows;
}

void sdl_left_panel_debug_log_content_size(int term_w, int term_h,
    int scan_cols, int scan_rows, int measured_cols, int measured_rows)
{
    enum { LEFT_PANEL_CONTENT_LOG_CACHE = 8 };
    static int next_slot = 0;
    static bool have_last[LEFT_PANEL_CONTENT_LOG_CACHE];
    static int last_term_w[LEFT_PANEL_CONTENT_LOG_CACHE];
    static int last_term_h[LEFT_PANEL_CONTENT_LOG_CACHE];
    static int last_scan_cols[LEFT_PANEL_CONTENT_LOG_CACHE];
    static int last_scan_rows[LEFT_PANEL_CONTENT_LOG_CACHE];
    static int last_measured_cols[LEFT_PANEL_CONTENT_LOG_CACHE];
    static int last_measured_rows[LEFT_PANEL_CONTENT_LOG_CACHE];

    for (int i = 0; i < LEFT_PANEL_CONTENT_LOG_CACHE; i++) {
        if (have_last[i]
            && last_term_w[i] == term_w
            && last_term_h[i] == term_h
            && last_scan_cols[i] == scan_cols
            && last_scan_rows[i] == scan_rows
            && last_measured_cols[i] == measured_cols
            && last_measured_rows[i] == measured_rows)
        {
            return;
        }
    }

    have_last[next_slot] = true;
    last_term_w[next_slot] = term_w;
    last_term_h[next_slot] = term_h;
    last_scan_cols[next_slot] = scan_cols;
    last_scan_rows[next_slot] = scan_rows;
    last_measured_cols[next_slot] = measured_cols;
    last_measured_rows[next_slot] = measured_rows;
    next_slot = (next_slot + 1) % LEFT_PANEL_CONTENT_LOG_CACHE;

    log_trace("left-panel measured content: term=%dx%d scan=%dx%d "
        "measured=%dx%d",
        term_w, term_h, scan_cols, scan_rows, measured_cols, measured_rows);
}

bool sdl_left_panel_content_size_for_term(const term* t,
    int scan_rows, int* out_cols, int* out_rows)
{
    term_win* scr;
    int scan_cols;
    int max_col = -1;
    int max_row = -1;

    if (out_cols)
        *out_cols = 0;
    if (out_rows)
        *out_rows = 0;
    if (!t)
        return false;
    scr = t->scr;
    if (!scr || !scr->a || !scr->c)
        return false;

    scan_cols = MIN(LEFT_PANEL_CONTENT_WID, t->wid);
    if (scan_rows > t->hgt)
        scan_rows = t->hgt;

    if (scan_cols <= 0 || scan_rows <= 0)
        return false;

    for (int row = 0; row < scan_rows; row++) {
        if (!scr->a[row] || !scr->c[row])
            continue;

        for (int col = 0; col < scan_cols; col++) {
            byte a = scr->a[row][col];
            char c = scr->c[row][col];
            int visible_col;

            if (!sdl_left_panel_cell_has_visible_content(a, c))
                continue;

            visible_col = col;
            if (g_state.use_tiles && g_state.tileset && use_bigtile
                && (a & TILE_FLAG) && (((byte)c) & TILE_FLAG)
                && visible_col + 1 < scan_cols)
            {
                visible_col++;
            }

            if (visible_col > max_col)
                max_col = visible_col;
            if (row > max_row)
                max_row = row;
        }
    }

    if (max_col < 0 || max_row < 0)
        return false;

    if (out_cols)
        *out_cols = max_col + 1;
    if (out_rows)
        *out_rows = max_row + 1;
    sdl_left_panel_debug_log_content_size(t->wid, t->hgt, scan_cols,
        scan_rows, max_col + 1, max_row + 1);
    return true;
}

bool sdl_left_panel_render_source_term(const sdl_view* view,
    int source_rows, term* panel_term, int* out_source_w, int* out_source_h)
{
    term* old_term;
    bool old_source_active;
    int source_w;
    int source_h;

    if (out_source_w)
        *out_source_w = 0;
    if (out_source_h)
        *out_source_h = 0;
    if (!view || !view->term_ready || !panel_term || source_rows <= 0)
        return false;

    source_w = MAX(view->t.wid, LEFT_PANEL_WID);
    source_h = source_rows;
    if (source_h < 1)
        source_h = 1;
    /*
     * prt_frame_basic() picks its compact (<24 rows) vs normal sidebar layout
     * from this scratch term's own height, while every caller reads rows back
     * out using the ROW_* macros, which key off the *active* term's height
     * (SIL_UI_COMPACT_HEIGHT).  Force the scratch term to the active term's
     * compactness so a row index computed by the caller maps to the same row
     * drawn here.  Otherwise a short scratch term (e.g. the collapsed left
     * panel, which only needs a few rows) renders the compact layout while
     * non-compact indices are read, shifting every row -- melee/quiver/evasion
     * surface where HP/melee/archery belong, in both the combat overlay and the
     * compact left panel.
     */
    if (!SIL_UI_COMPACT_HEIGHT)
        source_h = MAX(source_h, 24);

    if (term_init(panel_term, source_w, source_h, 16) != 0)
        return false;

    old_term = Term;
    old_source_active = g_sdl_left_panel_pane_source_active;
    g_sdl_left_panel_pane_source_active = true;
    Term_activate(panel_term);
    prt_frame_basic();
    Term_activate(old_term);
    g_sdl_left_panel_pane_source_active = old_source_active;

    if (out_source_w)
        *out_source_w = source_w;
    if (out_source_h)
        *out_source_h = source_h;
    return true;
}

void sdl_left_panel_source_invalidate(void)
{
    g_left_panel_source_generation++;
    if (g_left_panel_source_generation == 0)
        g_left_panel_source_generation = 1;
}

const term* sdl_left_panel_source_term_for_view(const sdl_view* view,
    int source_rows)
{
    term* old_term;
    bool old_source_active;
    int source_w;
    int source_h;

    if (!view || !view->term_ready || source_rows <= 0)
        return NULL;

    source_w = MAX(view->t.wid, LEFT_PANEL_WID);
    /*
     * Keep one stable source size for the view. Compact metrics query several
     * individual rows before the pane render; sizing to each query would nuke
     * and recreate this term repeatedly in the same frame.
     */
    source_h = MAX(source_rows, MAX(view->t.hgt, 1));
    if (!SIL_UI_COMPACT_HEIGHT)
        source_h = MAX(source_h, 24);

    if (g_left_panel_source_term_ready
        && (g_left_panel_source_w != source_w
            || g_left_panel_source_h != source_h))
    {
        term_nuke(&g_left_panel_source_term);
        g_left_panel_source_term_ready = false;
        g_left_panel_source_rendered_generation = 0;
    }

    if (!g_left_panel_source_term_ready) {
        if (term_init(&g_left_panel_source_term, source_w, source_h, 16) != 0)
            return NULL;
        g_left_panel_source_term_ready = true;
        g_left_panel_source_w = source_w;
        g_left_panel_source_h = source_h;
    }

    if (g_left_panel_source_rendered_generation
        == g_left_panel_source_generation)
    {
        return &g_left_panel_source_term;
    }

    old_term = Term;
    old_source_active = g_sdl_left_panel_pane_source_active;
    g_sdl_left_panel_pane_source_active = true;
    Term_activate(&g_left_panel_source_term);
    Term_clear();
    prt_frame_basic();
    Term_activate(old_term);
    g_sdl_left_panel_pane_source_active = old_source_active;
    g_left_panel_source_rendered_generation =
        g_left_panel_source_generation;

    return &g_left_panel_source_term;
}

bool sdl_left_panel_content_size_for_scratch(const sdl_view* view,
    int source_rows, int* out_cols, int* out_rows);

bool sdl_left_panel_content_size_for_view(const sdl_view* view,
    int* out_cols, int* out_rows)
{
    int scan_rows;

    if (out_cols)
        *out_cols = 0;
    if (out_rows)
        *out_rows = 0;
    if (!view || !view->term_ready)
        return false;

    scan_rows = sdl_main_view_visual_rows(view);
    if (scan_rows <= 0 && view->rows > 0)
        scan_rows = view->rows;

    return sdl_left_panel_content_size_for_scratch(view, scan_rows,
        out_cols, out_rows);
}

bool sdl_left_panel_content_size_for_scratch(const sdl_view* view,
    int source_rows, int* out_cols, int* out_rows)
{
    term panel_term;
    int source_w;
    int source_h;
    bool measured = false;
    static bool have_last_log = false;
    static int last_source_w;
    static int last_source_h;
    static int last_cols;
    static int last_rows;

    if (out_cols)
        *out_cols = 0;
    if (out_rows)
        *out_rows = 0;
    if (!view || !view->term_ready || source_rows <= 0)
        return false;

    if (!sdl_left_panel_render_source_term(view, source_rows, &panel_term,
            &source_w, &source_h))
    {
        return false;
    }
    measured = sdl_left_panel_content_size_for_term(&panel_term, source_h,
        out_cols, out_rows);
    term_nuke(&panel_term);

    if (measured
        && (!have_last_log
            || last_source_w != source_w
            || last_source_h != source_h
            || last_cols != (out_cols ? *out_cols : 0)
            || last_rows != (out_rows ? *out_rows : 0)))
    {
        have_last_log = true;
        last_source_w = source_w;
        last_source_h = source_h;
        last_cols = out_cols ? *out_cols : 0;
        last_rows = out_rows ? *out_rows : 0;
        log_trace("left-panel scratch content: source=%dx%d measured=%dx%d",
            source_w, source_h, last_cols, last_rows);
    }

    return measured;
}

int sdl_left_panel_source_row_width_for_term(const term* t,
    int source_row)
{
    term_win* scr;
    int scan_cols;
    int max_col = -1;

    if (!t || source_row < 0)
        return 0;
    if (source_row >= t->hgt)
        return 0;

    scr = t->scr;
    if (!scr || !scr->a || !scr->c || !scr->a[source_row]
        || !scr->c[source_row])
    {
        return 0;
    }

    scan_cols = MIN(LEFT_PANEL_CONTENT_WID, t->wid);
    for (int col = 0; col < scan_cols; col++) {
        byte a = scr->a[source_row][col];
        char c = scr->c[source_row][col];
        int visible_col;

        if (!sdl_left_panel_cell_has_visible_content(a, c))
            continue;

        visible_col = col;
        if (g_state.use_tiles && g_state.tileset && use_bigtile
            && (a & TILE_FLAG) && (((byte)c) & TILE_FLAG)
            && visible_col + 1 < scan_cols)
        {
            visible_col++;
        }

        if (visible_col > max_col)
            max_col = visible_col;
    }

    return max_col + 1;
}

int sdl_left_panel_source_row_width_for_view(const sdl_view* view,
    int source_row)
{
    const term* source_term;

    if (!view || !view->term_ready || source_row < 0)
        return 0;

    source_term = sdl_left_panel_source_term_for_view(view, source_row + 1);
    if (!source_term || source_row >= source_term->hgt)
        return 0;

    return sdl_left_panel_source_row_width_for_term(source_term, source_row);
}

bool sdl_left_panel_compact_light_span_for_term(const term* t,
    const term_win* scr, sdl_left_panel_compact_light_span* out)
{
    int scan_cols;
    int icon_cols = 2;
    int text_start = -1;
    int text_end = -1;

    if (out)
        *out = (sdl_left_panel_compact_light_span){ 0 };
    if (!t || !scr || !inventory[INVEN_LITE].k_idx)
        return false;
    if (ROW_LIGHT < 0 || ROW_LIGHT >= t->hgt)
        return false;
    if (!scr->a || !scr->c || !scr->a[ROW_LIGHT] || !scr->c[ROW_LIGHT])
        return false;

    scan_cols = MIN(LEFT_PANEL_CONTENT_WID, t->wid);
    if (scan_cols <= icon_cols)
        return false;

    for (int col = icon_cols; col < scan_cols; col++) {
        byte a = scr->a[ROW_LIGHT][col];
        char c = scr->c[ROW_LIGHT][col];

        if (!sdl_left_panel_cell_has_visible_content(a, c))
            continue;

        if (text_start < 0)
            text_start = col;
        text_end = col;
    }

    if (text_start < 0 || text_end < text_start)
        return false;

    if (out) {
        out->icon_cols = icon_cols;
        out->text_start = text_start;
        out->text_width = text_end - text_start + 1;
        out->packed_width = icon_cols + 1 + out->text_width;
    }

    return true;
}

int sdl_left_panel_compact_source_row_width_for_view(
    const sdl_view* view, int source_row, bool row_mode)
{
    if (row_mode && source_row == ROW_LIGHT) {
        sdl_left_panel_compact_light_span span;
        const term* source_term;

        if (!view || !view->term_ready)
            return 0;

        source_term = sdl_left_panel_source_term_for_view(view,
            source_row + 1);
        if (source_term
            && sdl_left_panel_compact_light_span_for_term(source_term,
                source_term->scr, &span))
        {
            return span.packed_width;
        }
    }

    return sdl_left_panel_source_row_width_for_view(view, source_row);
}

void sdl_left_panel_compact_metrics_for_view(const sdl_view* view,
    sdl_left_panel_metrics* metrics)
{
    const int source_rows[] = { ROW_HP, ROW_SP, ROW_LIGHT };
    int next_col = 0;
    int max_cols = 0;
    bool row_mode;

    if (!metrics)
        return;

    row_mode = sdl_left_panel_compact_row_mode();
    metrics->collapsed = true;
    metrics->compact_row = row_mode;
    metrics->compact_segment_count = (int)N_ELEMENTS(source_rows);

    for (int i = 0; i < metrics->compact_segment_count; i++) {
        int width = sdl_left_panel_compact_source_row_width_for_view(view,
            source_rows[i], row_mode);

        if (width <= 0)
            width = LEFT_PANEL_CONTENT_WID;
        if (width > LEFT_PANEL_CONTENT_WID)
            width = LEFT_PANEL_CONTENT_WID;

        metrics->compact_source_rows[i] = source_rows[i];
        metrics->compact_widths[i] = width;
        if (row_mode) {
            metrics->compact_output_rows[i] = 0;
            metrics->compact_output_cols[i] = next_col;
            next_col += width + 1;
        } else {
            metrics->compact_output_rows[i] = i + 1;
            metrics->compact_output_cols[i] = 0;
            if (width > max_cols)
                max_cols = width;
        }
    }

    if (row_mode) {
        metrics->panel_rows = 1;
        metrics->content_cols = next_col > 0 ? next_col - 1 : 1;
    } else {
        metrics->panel_rows = metrics->compact_segment_count + 1;
        metrics->content_cols = max_cols > 0 ? max_cols : LEFT_PANEL_CONTENT_WID;
    }
}

static int sdl_left_panel_visible_row_count_for_source_rows(int source_rows)
{
    int visible_rows = 0;

    if (source_rows <= 0)
        return 0;

    for (int row = 0; row < source_rows; row++) {
        if (!sdl_left_panel_source_row_hidden_by_combat_overlay(row))
            visible_rows++;
    }

    return visible_rows;
}

bool sdl_left_panel_metrics_for_view(const sdl_view* view,
    sdl_left_panel_metrics* metrics)
{
    int cell_w;
    int cell_h;
    int content_cols = LEFT_PANEL_CONTENT_WID;
    int panel_rows;
    int render_rows;
    int panel_render_h;
    int bottom_border_h;
    int corner_h;
    int source_h;
    sdl_left_panel_metrics local_metrics = { 0 };

    if (metrics)
        *metrics = (sdl_left_panel_metrics){ 0 };
    if (!view || view->cell_w <= 0 || view->cell_h <= 0)
        return false;

    if (sdl_left_panel_pane_collapsed()) {
        sdl_left_panel_compact_metrics_for_view(view, &local_metrics);
        content_cols = local_metrics.content_cols;
        panel_rows = local_metrics.panel_rows;
    } else {
        const int natural_rows = sdl_left_panel_pane_rows_for_view(view);
        const term* source_term = sdl_left_panel_source_term_for_view(view,
            natural_rows);
        int measured_cols = 0;
        int measured_rows = 0;

        /*
         * Size the black pane to its rendered status content.  The retained
         * source term keeps this measurement cheap while still allowing the
         * pane to grow when a transient status row actually appears.
         */
        if (source_term
            && sdl_left_panel_content_size_for_term(source_term, natural_rows,
                &measured_cols, &measured_rows))
        {
            content_cols = measured_cols;
            panel_rows = measured_rows;
        } else {
            content_cols = LEFT_PANEL_CONTENT_WID;
            panel_rows = natural_rows;
        }
    }
    if (content_cols < 1)
        content_cols = 1;
    if (panel_rows < 1)
        panel_rows = 1;
    render_rows = sdl_left_panel_pane_collapsed()
        ? panel_rows
        : sdl_left_panel_visible_row_count_for_source_rows(panel_rows);
    if (render_rows < 1)
        render_rows = 1;

    sdl_left_panel_pane_cell_size_for_view(view, content_cols, &cell_w,
        &cell_h);
    if (cell_w < 1 || cell_h < 1)
        return false;

    source_h = sdl_main_view_visual_rows(view) * view->cell_h;
    if (source_h <= 0)
        return false;
    if (render_rows > 0) {
        int max_cell_h = source_h / (render_rows + 1);

        if (max_cell_h < 1)
            max_cell_h = 1;
        if (cell_h > max_cell_h) {
            cell_h = max_cell_h;
            cell_w = cell_h / 2;
            if (cell_w < 1)
                cell_w = 1;
        }
    }

    bottom_border_h = cell_h;
    panel_render_h = render_rows * cell_h;
    if (panel_render_h + bottom_border_h > source_h) {
        int available_content_h = source_h - bottom_border_h;

        if (available_content_h < cell_h) {
            bottom_border_h = 0;
            available_content_h = source_h;
        }
        render_rows = available_content_h / cell_h;
        if (render_rows < 1)
            render_rows = 1;
        panel_render_h = render_rows * cell_h;
        if (panel_render_h > source_h)
            panel_render_h = source_h;
        bottom_border_h = source_h - panel_render_h;
        if (bottom_border_h > cell_h)
            bottom_border_h = cell_h;
        if (bottom_border_h < 0)
            bottom_border_h = 0;
    }
    corner_h = panel_render_h + bottom_border_h;

    if (metrics) {
        *metrics = local_metrics;
        metrics->collapsed = sdl_left_panel_pane_collapsed();
        metrics->compact_row = metrics->collapsed
            && sdl_left_panel_compact_row_mode();
        metrics->cell_w = cell_w;
        metrics->cell_h = cell_h;
        metrics->content_cols = content_cols;
        metrics->content_w = content_cols * metrics->cell_w;
        metrics->separator_w = sdl_left_panel_pane_has_border_columns()
            ? metrics->cell_w
            : 0;
        metrics->total_w = metrics->content_w
            + (metrics->separator_w * 2);
        metrics->panel_rows = panel_rows;
        metrics->panel_render_h = panel_render_h;
        metrics->bottom_border_h = bottom_border_h;
        metrics->corner_h = corner_h;
        metrics->visual_rows = sdl_main_view_visual_rows(view);
        metrics->source_h = source_h;
    }

    return true;
}

int sdl_left_panel_content_x_for_metrics(
    const sdl_left_panel_metrics* metrics)
{
    if (!metrics || metrics->separator_w <= 0)
        return 0;

    return metrics->separator_w;
}

bool sdl_left_panel_pane_rect_for_metrics(const sdl_view* view,
    const sdl_left_panel_metrics* metrics, SDL_FRect* out_rect)
{
    enum pane_placement where;
    int visual_cols;
    int visual_rows;
    int visual_w;
    int visual_h;
    int edge_gap_x;
    int edge_gap_y;
    float x;
    float y;

    if (out_rect)
        *out_rect = (SDL_FRect){ 0 };
    if (!view || !metrics)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0)
        return false;
    if (metrics->total_w <= 0 || metrics->corner_h <= 0)
        return false;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    visual_w = visual_cols * view->cell_w;
    visual_h = visual_rows * view->cell_h;
    if (visual_w <= metrics->total_w || visual_h <= 0)
        return false;

    where = sdl_left_panel_pane_placement();
    x = (float)(view->rect.x + view->margin_x);
    y = (float)(view->rect.y + view->margin_y);
    edge_gap_x = sdl_overlay_edge_gap_px(visual_w, metrics->total_w);
    edge_gap_y = sdl_overlay_edge_gap_px(visual_h, metrics->corner_h);

    if (sdl_left_panel_pane_placement_is_right(where))
        x += (float)(visual_w - metrics->total_w - edge_gap_x);
    else if (sdl_left_panel_pane_placement_is_horizontal_center(where))
        x += (float)((visual_w - metrics->total_w) / 2);
    else
        x += (float)edge_gap_x;

    if (sdl_left_panel_pane_placement_is_bottom(where))
        y += (float)(visual_h - metrics->corner_h - edge_gap_y);
    else if (sdl_left_panel_pane_placement_is_vertical_center(where))
        y += (float)((visual_h - metrics->corner_h) / 2);
    else
        y += (float)edge_gap_y;

    if (out_rect) {
        *out_rect = (SDL_FRect){
            .x = x,
            .y = y,
            .w = (float)metrics->total_w,
            .h = (float)metrics->corner_h,
        };
    }

    return true;
}

bool sdl_left_panel_pane_layout_enabled(void)
{
    return !g_hide_left_panel
        && sdl_should_show_supporting_panes()
        && sdl_left_panel_pane_config_enabled();
}

bool sdl_saved_screen_left_panel_pane_active(void)
{
    return g_saved_screen_left_panel_pane_depth > 0;
}

bool sdl_left_panel_pane_presentation_active(void)
{
    return sdl_left_panel_pane_layout_enabled()
        && character_generated
        && character_dungeon
        && p_ptr
        && character_icky == 0
        /* Only present the styled left-panel pane during live gameplay.  Death
         * (is_dead), the post-death spectator view, and quit-to-title
         * (!playing) are not gameplay: the pane used to keep drawing its compact
         * fallback alone over a blank main view in those scenes (e.g. behind the
         * wizard "Die?" prompt).  Gating here also keeps the map's COL_MAP offset
         * consistent, since every consumer derives from this one predicate.  We
         * deliberately do NOT gate on p_ptr->leaving, so ordinary level
         * transitions keep the pane (and the map offset) steady instead of
         * snapping to the classic sidebar on every stair. */
        && p_ptr->playing
        && !p_ptr->is_dead
        && !death_spectator_active();
}

bool sdl_left_panel_pane_renders_character_panel(void)
{
    return sdl_left_panel_pane_presentation_active();
}

bool sdl_combat_overlay_pane_presentation_active(void)
{
    return sdl_should_show_supporting_panes()
        && sdl_layout_matches_supporting_pane_visibility()
        && character_generated
        && character_dungeon
        && p_ptr
        && character_icky == 0
        && p_ptr->playing
        && !p_ptr->is_dead
        && !death_spectator_active();
}

bool sdl_combat_overlay_melee_uses_offhand_row(void)
{
    return inventory
        && (ROW_MEL - 1) != ROW_LIGHT
        && inventory[INVEN_ARM].k_idx
        && inventory[INVEN_ARM].tval != TV_SHIELD;
}

int sdl_combat_overlay_source_row_count(void)
{
    return sdl_combat_overlay_melee_uses_offhand_row() ? 4 : 3;
}

bool sdl_combat_overlay_source_row_at_index(int index, int* out_row)
{
    int rows[4];
    int count = 0;

    if (out_row)
        *out_row = -1;
    if (index < 0)
        return false;

    if (sdl_combat_overlay_melee_uses_offhand_row())
        rows[count++] = ROW_MEL - 1;
    rows[count++] = ROW_MEL;
    rows[count++] = ROW_ARC;
    rows[count++] = ROW_QUIVER;

    if (index >= count)
        return false;

    if (out_row)
        *out_row = rows[index];
    return true;
}

int sdl_combat_overlay_visible_row_count(int panel_rows)
{
    int source_count = sdl_combat_overlay_source_row_count();

    if (panel_rows < 0)
        panel_rows = 0;
    return (panel_rows < source_count) ? panel_rows : source_count;
}

bool sdl_combat_overlay_visible_source_row_at_index(int index,
    int panel_rows, int* out_row)
{
    int source_count = sdl_combat_overlay_source_row_count();
    int visible_count = sdl_combat_overlay_visible_row_count(panel_rows);
    int source_index = index;

    if (out_row)
        *out_row = -1;
    if (index < 0 || index >= visible_count)
        return false;

    /*
     * If the overlay is clipped to three rows, keep the primary combat rows
     * visible and drop the optional offhand row first.
     */
    if (panel_rows < source_count
        && sdl_combat_overlay_melee_uses_offhand_row())
    {
        source_index++;
    }

    return sdl_combat_overlay_source_row_at_index(source_index, out_row);
}

bool sdl_combat_overlay_source_row_visible(int source_row)
{
    SDL_Rect pane;
    int cell_h;
    int panel_rows;
    int visible_count;

    if (source_row < 0)
        return false;
    if (!sdl_combat_overlay_pane_content_rect(&pane))
        return false;

    cell_h = sdl_effective_pane_cell_height_for_type(PANE_COMBAT);
    if (cell_h < 1)
        cell_h = 1;
    panel_rows = pane.h / cell_h;
    if (panel_rows <= 0)
        return false;

    visible_count = sdl_combat_overlay_visible_row_count(panel_rows);
    for (int i = 0; i < visible_count; i++) {
        int row_at_index = -1;

        if (!sdl_combat_overlay_visible_source_row_at_index(i, panel_rows,
                &row_at_index))
        {
            continue;
        }
        if (row_at_index == source_row)
            return true;
    }

    return false;
}

bool sdl_left_panel_source_row_hidden_by_combat_overlay(int source_row)
{
    return sdl_combat_overlay_source_row_visible(source_row);
}

int sdl_left_panel_output_row_for_source_row(int source_row)
{
    int output_row = 0;

    if (source_row < 0)
        return -1;
    if (sdl_left_panel_source_row_hidden_by_combat_overlay(source_row))
        return -1;

    for (int row = 0; row < source_row; row++) {
        if (!sdl_left_panel_source_row_hidden_by_combat_overlay(row))
            output_row++;
    }

    return output_row;
}

int sdl_left_panel_source_row_for_output_row(int output_row)
{
    int visible_row = 0;

    if (output_row < 0)
        return -1;

    for (int source_row = 0; source_row < ROW_EVN + 16; source_row++) {
        if (sdl_left_panel_source_row_hidden_by_combat_overlay(source_row))
            continue;
        if (visible_row == output_row)
            return source_row;
        visible_row++;
    }

    return -1;
}

/* Map a window-pixel rect to the map cells it covers on the main view.
 * Returns false when the rect misses the map grid entirely.  The result is
 * in map cells (the units verify_panel scrolls in), not term cells: the map
 * origin sits at ROW_MAP/COL_MAP less the columns the view already hides,
 * and bigtile map cells span two grid columns (see
 * sdl_main_view_point_to_map). */
static bool sdl_main_view_map_cell_coverage(const sdl_view* view,
    const SDL_FRect* rect, int* start_col, int* cols, int* start_row,
    int* rows)
{
    int visual_cols;
    int visual_rows;
    int first_col;
    int last_col;
    int first_row;
    int last_row;
    int col_origin;
    float grid_x;
    float grid_y;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    if (visual_cols <= 0 || visual_rows <= 0)
        return false;

    grid_x = (float)(view->rect.x + view->margin_x);
    grid_y = (float)(view->rect.y + view->margin_y);
    first_col = (int)SDL_floorf((rect->x - grid_x) / (float)view->cell_w);
    last_col = (int)SDL_ceilf(
        (rect->x + rect->w - grid_x) / (float)view->cell_w);
    first_row = (int)SDL_floorf((rect->y - grid_y) / (float)view->cell_h);
    last_row = (int)SDL_ceilf(
        (rect->y + rect->h - grid_y) / (float)view->cell_h);

    if (first_col < 0)
        first_col = 0;
    if (last_col > visual_cols)
        last_col = visual_cols;
    if (first_row < 0)
        first_row = 0;
    if (last_row > visual_rows)
        last_row = visual_rows;
    if (last_col <= first_col || last_row <= first_row)
        return false;

    col_origin = COL_MAP - sdl_main_view_visible_col0();
    first_col -= col_origin;
    last_col -= col_origin;
    first_row -= ROW_MAP;
    last_row -= ROW_MAP;
    if (use_bigtile) {
        first_col = first_col / 2;
        last_col = (last_col + 1) / 2;
    }

    if (first_col < 0)
        first_col = 0;
    if (first_row < 0)
        first_row = 0;
    if (last_col <= first_col || last_row <= first_row)
        return false;

    if (start_col)
        *start_col = first_col;
    if (cols)
        *cols = last_col - first_col;
    if (start_row)
        *start_row = first_row;
    if (rows)
        *rows = last_row - first_row;

    return true;
}

bool sdl_left_panel_pane_map_coverage(int* start_col, int* cols,
    int* start_row, int* rows)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    sdl_left_panel_metrics metrics;
    SDL_FRect rect;

    if (start_col)
        *start_col = 0;
    if (cols)
        *cols = 0;
    if (start_row)
        *start_row = 0;
    if (rows)
        *rows = 0;

    if (!sdl_left_panel_pane_presentation_active())
        return false;
    if (!view->term_ready || view->cell_w <= 0 || view->cell_h <= 0)
        return false;
    if (!sdl_left_panel_metrics_for_view(view, &metrics))
        return false;
    if (!sdl_left_panel_pane_rect_for_metrics(view, &metrics, &rect))
        return false;

    return sdl_main_view_map_cell_coverage(view, &rect, start_col, cols,
        start_row, rows);
}

bool sdl_combat_overlay_pane_map_coverage(int* start_col, int* cols,
    int* start_row, int* rows)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    SDL_Rect pane_rect;
    SDL_FRect rect;

    if (start_col)
        *start_col = 0;
    if (cols)
        *cols = 0;
    if (start_row)
        *start_row = 0;
    if (rows)
        *rows = 0;

    if (!sdl_combat_overlay_pane_current_rect(&pane_rect))
        return false;
    if (!view->term_ready || view->cell_w <= 0 || view->cell_h <= 0)
        return false;

    rect = (SDL_FRect){
        .x = (float)pane_rect.x,
        .y = (float)pane_rect.y,
        .w = (float)pane_rect.w,
        .h = (float)pane_rect.h,
    };

    return sdl_main_view_map_cell_coverage(view, &rect, start_col, cols,
        start_row, rows);
}

bool sdl_overlay_log_pane_map_coverage(int* start_col, int* cols,
    int* start_row, int* rows)
{
    const sdl_view* main_view = &g_views[PANE_MAIN];
    const sdl_view* view = &g_views[PANE_ROLLS];
    SDL_FRect band;
    float blit_off;
    int pad;

    if (start_col)
        *start_col = 0;
    if (cols)
        *cols = 0;
    if (start_row)
        *start_row = 0;
    if (rows)
        *rows = 0;

    if (!sdl_view_is_overlay_log_pane(view))
        return false;
    if (!sdl_should_show_supporting_panes())
        return false;
    if (!view->canvas || view->cols <= 0 || view->rows <= 0
        || view->cell_w <= 0 || view->cell_h <= 0)
    {
        return false;
    }
    if (!main_view->term_ready || main_view->cell_w <= 0
        || main_view->cell_h <= 0)
    {
        return false;
    }

    /* Mirror the present-time blit: only the band past the transparent left
     * margin carries the translucent panel and actually covers the map. */
    pad = view->cell_w / 8;
    if (pad < 2)
        pad = 2;
    blit_off = (float)(pane_log_overlay_left_margin(view->cols) * view->cell_w
        - pad);
    if (blit_off < 0.0f)
        blit_off = 0.0f;

    band.x = (float)(view->rect.x + view->margin_x) + blit_off;
    band.w = (float)(view->cols * view->cell_w) - blit_off;
    band.y = (float)view->rect.y;
    band.h = (float)view->rect.h;
    if (band.w <= 0.0f || band.h <= 0.0f)
        return false;

    return sdl_main_view_map_cell_coverage(main_view, &band, start_col, cols,
        start_row, rows);
}

/*
 * Pixel rectangle of the overlay log's *visible* band (the translucent
 * right-hand strip past the transparent left margin).  Mirrors the present-time
 * blit so pointer hit-testing matches exactly what the player sees, and so taps
 * on the transparent margin still fall through to the map behind it.
 */
bool sdl_overlay_log_pane_current_rect(SDL_Rect* out_rect)
{
    const sdl_view* view = &g_views[PANE_ROLLS];
    float blit_off;
    int pad;
    int x;
    int w;

    if (out_rect)
        *out_rect = (SDL_Rect){ 0 };

    if (!sdl_view_is_overlay_log_pane(view))
        return false;
    if (!sdl_should_show_supporting_panes())
        return false;
    if (!view->canvas || view->cols <= 0 || view->cell_w <= 0
        || view->rect.w <= 0 || view->rect.h <= 0)
    {
        return false;
    }

    pad = view->cell_w / 8;
    if (pad < 2)
        pad = 2;
    blit_off = (float)(pane_log_overlay_left_margin(view->cols) * view->cell_w
        - pad);
    if (blit_off < 0.0f)
        blit_off = 0.0f;

    x = view->rect.x + view->margin_x + (int)blit_off;
    w = view->cols * view->cell_w - (int)blit_off;
    if (w <= 0)
        return false;

    if (out_rect) {
        out_rect->x = x;
        out_rect->y = view->rect.y;
        out_rect->w = w;
        out_rect->h = view->rect.h;
    }
    return true;
}

bool sdl_left_panel_pane_runtime_active(void)
{
    return sdl_left_panel_pane_presentation_active()
        && p_ptr->playing
        && !p_ptr->leaving
        && !p_ptr->is_dead
        && !death_spectator_active();
}

bool sdl_left_panel_pane_collapsed(void)
{
    return !g_left_panel_pane_expanded;
}

int sdl_left_panel_compact_mode_normalized(int mode)
{
    return (mode == SDL_LEFT_PANEL_COMPACT_ROW)
        ? SDL_LEFT_PANEL_COMPACT_ROW
        : SDL_LEFT_PANEL_COMPACT_COLUMN;
}

bool sdl_left_panel_compact_row_mode(void)
{
    return sdl_left_panel_compact_mode_normalized(
        config.left_panel_compact_mode) == SDL_LEFT_PANEL_COMPACT_ROW;
}

bool sdl_left_panel_pane_has_border_columns(void)
{
    return !sdl_left_panel_pane_collapsed()
        || !sdl_left_panel_compact_row_mode();
}

int sdl_main_view_visual_cols_for_width(int width_px, int cell_w)
{
    int cols;

    if (width_px <= 0 || cell_w <= 0)
        return 0;

    cols = width_px / cell_w;
    if (cols < 0)
        cols = 0;

    return cols;
}

int sdl_main_view_logical_cols_for_visual_cols(int visual_cols)
{
    if (visual_cols <= 0)
        return 0;

    if (!sdl_left_panel_pane_layout_enabled())
        return visual_cols;

    return COL_MAP + visual_cols;
}

const char* sdl_startup_device_class_name(sdl_startup_device_class device)
{
    switch (device) {
    case SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER:
        return "desktop-controller";
    case SDL_STARTUP_DEVICE_DESKTOP_HANDHELD:
        return "desktop-handheld";
    case SDL_STARTUP_DEVICE_ANDROID_HANDHELD:
        return "android-handheld";
    case SDL_STARTUP_DEVICE_MOBILE_TOUCH:
        return "mobile";
    case SDL_STARTUP_DEVICE_DESKTOP:
    default:
        return "desktop";
    }
}

bool sdl_startup_device_class_uses_controller_ui(
    sdl_startup_device_class device)
{
    return device == SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER
        || device == SDL_STARTUP_DEVICE_DESKTOP_HANDHELD
        || device == SDL_STARTUP_DEVICE_ANDROID_HANDHELD;
}

void sdl_apply_startup_input_defaults_to_config(
    struct sdl_config* target, sdl_startup_device_class device)
{
    bool controller_ui = sdl_startup_device_class_uses_controller_ui(device);
    bool mobile_touch = (device == SDL_STARTUP_DEVICE_MOBILE_TOUCH);

    if (!target)
        return;

    target->gamepad_enabled = controller_ui;
    target->gamepad_auto_mode = controller_ui;
    target->steamdeck_mode = controller_ui;
    target->mouse_enabled = !controller_ui
        && (device == SDL_STARTUP_DEVICE_DESKTOP);

    if (mobile_touch) {
        target->touch_profile = SDL_TOUCH_PROFILE_ROUND_WHEEL;
        target->touch_pane_default_open = false;
        for (int i = 0; i < SDL_TOUCH_MENU_CATEGORY_COUNT; i++)
            target->touch_menu_command_enabled[i] = true;
        target->touch_movement_mode = SDL_TOUCH_MOVEMENT_ON;
        target->touch_round_movement_enabled = true;
        target->touch_zone_overlay_mode = SDL_TOUCH_ZONE_OVERLAY_OFF;
        target->touch_top_panel_mode = SDL_TOUCH_TOP_PANEL_MODE_LONG;
        target->touch_top_panel_button_count =
            SDL_TOUCH_TOP_PANEL_BUTTON_COUNT;
        target->touch_top_panel_default_open = true;
        target->touch_top_panel_tile_scale =
            SDL_TOUCH_TOP_PANEL_TILE_SCALE_DEFAULT;
        target->touch_swipe_enabled = true;
    }
}

void sdl_set_touch_pane_config_enabled(bool enabled)
{
    sdl_ensure_touch_pane_config_present();

    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane != PANE_TOUCH)
            continue;

        pane_config[i].where = PLACE_DOUBLE_RIGHT;
        pane_config[i].enabled = enabled;
        pane_config[i].rect.rows = 0;
        pane_config[i].rect.cols = 0;
        pane_config[i].font_size = 0;
        pane_config[i].ratio = 0.0f;
        return;
    }
}

void sdl_apply_first_start_device_defaults(
    sdl_startup_device_class device)
{
    bool touch_pane_enabled = (device == SDL_STARTUP_DEVICE_MOBILE_TOUCH);

    sdl_apply_startup_input_defaults_to_config(&config, device);
    sdl_set_touch_pane_config_enabled(touch_pane_enabled);

    log_info("First-start device defaults: profile=%s controller_input=%s "
             "controller_ui=%s mouse=%s touch_pane=%s menu_letters=%s",
        sdl_startup_device_class_name(device),
        config.gamepad_enabled ? "on" : "off",
        config.steamdeck_mode ? "on" : "off",
        config.mouse_enabled ? "on" : "off",
        touch_pane_enabled ? "on" : "off",
        (device == SDL_STARTUP_DEVICE_DESKTOP) ? "on" : "off");
}

sdl_startup_device_class sdl_detect_startup_device_class(
    int screen_width, int screen_height)
{
    bool has_gamepad = (g_gamepad_state.pad_count > 0);

#if defined(__ANDROID__)
    bool android_has_controller = false;

    (void)screen_width;
    (void)screen_height;

#if defined(SDL_PLATFORM_ANDROID)
    android_has_controller = sdl_android_has_controller_device();
    if (android_has_controller && !has_gamepad) {
        log_info("Android InputDevice reports a controller before SDL has an opened gamepad");
    }
#endif

    return (has_gamepad || android_has_controller)
        ? SDL_STARTUP_DEVICE_ANDROID_HANDHELD
        : SDL_STARTUP_DEVICE_MOBILE_TOUCH;
#elif defined(SIL_IOS)
    (void)has_gamepad;
    return SDL_STARTUP_DEVICE_MOBILE_TOUCH;
#endif

    (void)has_gamepad;
    (void)screen_width;
    (void)screen_height;
    return SDL_STARTUP_DEVICE_DESKTOP;
}

#if SIL_SDL_HANDHELD_DEFAULTS_BUILD
#if 0
/* Legacy handheld/mobile pane presets. First-run layouts no longer enable
 * side or bottom pane presets on any platform. */
struct pane_config* sdl_find_pane_config_entry(struct pane_config* configs,
    int count, enum pane_type pane)
{
    if (!configs)
        return NULL;

    for (int i = 0; i < count; i++) {
        if (configs[i].pane == pane)
            return &configs[i];
    }

    return NULL;
}

void sdl_mobile_reset_default_pane_configs(struct pane_config* configs,
    int count)
{
    struct pane_config* pc;

    if (!configs)
        return;

    for (int i = 0; i < count; i++) {
        switch (configs[i].pane) {
        case PANE_INVENTORY:
            configs[i].where = PLACE_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = 22;
            configs[i].rect.cols = 40;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_WORN:
            configs[i].where = PLACE_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = 17;
            configs[i].rect.cols = 40;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_INFO:
            configs[i].where = PLACE_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = 0;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_MAP:
            configs[i].where = PLACE_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = 12;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_TOUCH:
            configs[i].where = PLACE_DOUBLE_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = 0;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_LEFT_PANEL:
            configs[i].where = PLACE_TOP_LEFT;
            configs[i].enabled = true;
            configs[i].rect.rows = 0;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_STATUS:
            configs[i].where = PLACE_BOTTOM_RIGHT;
            configs[i].enabled = true;
            configs[i].rect.rows = 1;
            configs[i].rect.cols = 24;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_DEPTH:
            configs[i].where = PLACE_TOP_RIGHT;
            configs[i].enabled = true;
            configs[i].rect.rows = 4;
            configs[i].rect.cols = 12;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_COMBAT:
            configs[i].where = PLACE_BOTTOM_LEFT;
            configs[i].enabled = true;
            configs[i].rect.rows = PANE_COMBAT_OVERLAY_ROWS;
            configs[i].rect.cols = PANE_COMBAT_OVERLAY_COLS;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_OVERLAY_MENU:
            configs[i].where = PLACE_BOTTOM_CENTER;
            configs[i].enabled = true;
            configs[i].rect.rows = 1;
            configs[i].rect.cols = 4;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_MAIN_MENU:
            configs[i].where = PLACE_TOP_CENTER;
            configs[i].enabled = false;
            configs[i].rect.rows = 0;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_LOG:
            configs[i].where = PLACE_BOTTOM;
            configs[i].enabled = false;
            configs[i].rect.rows = 0;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        case PANE_ROLLS:
            configs[i].where = PLACE_TOP_RIGHT;
            configs[i].enabled = false;
            configs[i].rect.rows = SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS;
            configs[i].rect.cols = 0;
            configs[i].font_size = 0;
            configs[i].ratio = 0.0f;
            break;

        default:
            break;
        }
    }

    pc = sdl_find_pane_config_entry(configs, count, PANE_TOUCH);
    if (pc)
        pc->where = PLACE_DOUBLE_RIGHT;
}

void sdl_mobile_set_touch_pane_enabled(struct pane_config* configs,
    int count, bool enabled)
{
    struct pane_config* pc = sdl_find_pane_config_entry(configs, count,
        PANE_TOUCH);

    if (!pc)
        return;

    pc->where = PLACE_DOUBLE_RIGHT;
    pc->enabled = enabled;
    pc->rect.rows = 0;
    pc->rect.cols = 0;
    pc->font_size = 0;
    pc->ratio = 0.0f;
}

void sdl_mobile_disable_bottom_panes(struct pane_config* configs,
    int count)
{
    struct pane_config* rolls = sdl_find_pane_config_entry(configs, count,
        PANE_ROLLS);
    struct pane_config* log = sdl_find_pane_config_entry(configs, count,
        PANE_LOG);

    if (rolls) {
        rolls->enabled = false;
        rolls->where = PLACE_TOP_RIGHT;
        rolls->rect.rows = SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS;
        rolls->rect.cols = 0;
        rolls->font_size = 0;
        rolls->ratio = 0.0f;
    }

    if (log) {
        log->enabled = false;
        log->where = PLACE_BOTTOM;
        log->rect.rows = 0;
        log->rect.cols = 0;
        log->font_size = 0;
        log->ratio = 0.0f;
    }
}

void sdl_mobile_configure_bottom_wide(struct pane_config* configs,
    int count, int rows)
{
    struct pane_config* log;

    sdl_mobile_disable_bottom_panes(configs, count);
    if (rows <= 0)
        return;

    log = sdl_find_pane_config_entry(configs, count, PANE_LOG);

    if (log) {
        log->enabled = true;
        log->where = PLACE_BOTTOM;
        log->rect.rows = rows;
        log->rect.cols = 0;
    }
}

void sdl_mobile_configure_bottom_narrow(struct pane_config* configs,
    int count, int total_rows)
{
    struct pane_config* log;

    sdl_mobile_disable_bottom_panes(configs, count);
    if (total_rows <= 0)
        return;

    log = sdl_find_pane_config_entry(configs, count, PANE_LOG);

    if (log) {
        log->enabled = true;
        log->where = PLACE_BOTTOM;
        log->rect.rows = total_rows;
        log->rect.cols = 0;
    }
}

void sdl_mobile_set_right_panes(struct pane_config* configs, int count,
    bool inventory_enabled, bool worn_enabled)
{
    struct pane_config* inventory = sdl_find_pane_config_entry(configs, count,
        PANE_INVENTORY);
    struct pane_config* worn = sdl_find_pane_config_entry(configs, count,
        PANE_WORN);
    struct pane_config* info = sdl_find_pane_config_entry(configs, count,
        PANE_INFO);

    if (inventory) {
        inventory->where = PLACE_RIGHT;
        inventory->enabled = inventory_enabled;
        inventory->rect.rows = 22;
        inventory->rect.cols = 40;
        inventory->font_size = 0;
        inventory->ratio = 0.0f;
    }

    if (worn) {
        worn->where = PLACE_RIGHT;
        worn->enabled = worn_enabled;
        worn->rect.rows = 17;
        worn->rect.cols = 40;
        worn->font_size = 0;
        worn->ratio = 0.0f;
    }

    if (info) {
        info->where = PLACE_RIGHT;
        info->enabled = false;
        info->rect.rows = 0;
        info->rect.cols = 0;
        info->font_size = 0;
        info->ratio = 0.0f;
    }
}
#endif

int sdl_mobile_pane_cols(const SDL_Rect* panes, const int* cell_widths,
    enum pane_type type)
{
    if (!panes || !cell_widths || type < PANE_MAIN || type >= PANE_MAX)
        return 0;
    if (panes[type].w <= 0 || cell_widths[type] <= 0)
        return 0;

    return panes[type].w / cell_widths[type];
}

int sdl_mobile_pane_rows(const SDL_Rect* panes, const int* cell_heights,
    enum pane_type type)
{
    if (!panes || !cell_heights || type < PANE_MAIN || type >= PANE_MAX)
        return 0;
    if (panes[type].h <= 0 || cell_heights[type] <= 0)
        return 0;

    return panes[type].h / cell_heights[type];
}

bool sdl_mobile_enabled_panes_fit(const struct pane_config* configs,
    int count, const SDL_Rect* panes, const int* cell_widths,
    const int* cell_heights)
{
    if (!configs || !panes || !cell_widths || !cell_heights)
        return false;

    for (int i = 0; i < count; i++) {
        int cols;
        int rows;
        int min_cols;
        int min_rows;

        if (!configs[i].enabled)
            continue;
        if (configs[i].pane <= PANE_MAIN || configs[i].pane >= PANE_MAX)
            continue;
        if (configs[i].pane == PANE_LEFT_PANEL
            || configs[i].pane == PANE_COMBAT)
        {
            continue;
        }

        cols = sdl_mobile_pane_cols(panes, cell_widths, configs[i].pane);
        rows = sdl_mobile_pane_rows(panes, cell_heights, configs[i].pane);
        if (pane_placement_is_side(configs[i].where)) {
            min_cols = pane_primary_min_cells(configs[i].pane, configs[i].where);
            min_rows = pane_secondary_min_cells(configs[i].pane, configs[i].where);
        } else {
            min_cols = pane_secondary_min_cells(configs[i].pane, configs[i].where);
            min_rows = pane_primary_min_cells(configs[i].pane, configs[i].where);
        }

        if (cols < min_cols || rows < min_rows)
            return false;
    }

    return true;
}

bool sdl_mobile_layout_fits(const SDL_Rect* screen, int scale,
    const struct pane_config* configs, int count, SDL_Rect* out_panes,
    int* out_cell_widths, int* out_cell_heights, int* out_main_cols,
    int* out_main_rows)
{
    struct pane_config active[MAX_PANE_CONFIGS] = { 0 };
    SDL_Rect panes[PANE_MAX] = { 0 };
    int cell_widths[PANE_MAX] = { 0 };
    int cell_heights[PANE_MAX] = { 0 };
    int margin_px;
    int saved_scale;
    int saved_layout_scale_override;
    int active_count;
    int main_cols;
    int main_rows;
    bool fits;

    if (!screen || !configs || count < 0)
        return false;

    if (count > MAX_PANE_CONFIGS)
        count = MAX_PANE_CONFIGS;

    active_count = 0;
    for (int i = 0; i < count && active_count < MAX_PANE_CONFIGS; i++) {
        if (configs[i].pane == PANE_LEFT_PANEL
            || configs[i].pane == PANE_COMBAT)
        {
            continue;
        }
        active[active_count++] = configs[i];
    }

    saved_scale = config.main_view_scale;
    saved_layout_scale_override = g_main_view_layout_scale_override;
    config.main_view_scale = scale;
    g_main_view_layout_scale_override = scale;

    sdl_build_supporting_pane_metrics(active, active_count, cell_widths,
        cell_heights);
    margin_px = (int)(g_state.system_scale * config.margin);
    sdl_apply_dynamic_auto_pane_sizes(active, active_count, screen, cell_widths,
        cell_heights, margin_px);
    place_panes(active, active_count, panes, screen, cell_widths, cell_heights,
        margin_px);

    main_cols = sdl_mobile_pane_cols(panes, cell_widths, PANE_MAIN);
    main_rows = sdl_mobile_pane_rows(panes, cell_heights, PANE_MAIN);
    fits = sdl_mobile_enabled_panes_fit(active, active_count, panes,
        cell_widths, cell_heights);

    config.main_view_scale = saved_scale;
    g_main_view_layout_scale_override = saved_layout_scale_override;

    if (out_panes)
        memcpy(out_panes, panes, sizeof(panes));
    if (out_cell_widths)
        memcpy(out_cell_widths, cell_widths, sizeof(cell_widths));
    if (out_cell_heights)
        memcpy(out_cell_heights, cell_heights, sizeof(cell_heights));
    if (out_main_cols)
        *out_main_cols = main_cols;
    if (out_main_rows)
        *out_main_rows = main_rows;

    return fits;
}

#if 0
int sdl_mobile_select_default_scale(const SDL_Rect* screen,
    const struct pane_config* configs, int count)
{
    int max_scale;

    if (!screen)
        return 1;

    max_scale = sdl_max_scale_for_rect(screen);
    for (int scale = max_scale; scale >= 1; scale--) {
        if (sdl_mobile_layout_fits(screen, scale, configs, count, NULL, NULL,
                NULL, NULL, NULL))
            return scale;
    }

    return 1;
}

void sdl_apply_mobile_default_pane_layout(const SDL_Rect* screen,
    bool has_controller)
{
    struct pane_config selected[MAX_PANE_CONFIGS] = { 0 };
    struct pane_config candidate[MAX_PANE_CONFIGS] = { 0 };
    SDL_Rect panes[PANE_MAX] = { 0 };
    int cell_widths[PANE_MAX] = { 0 };
    int cell_heights[PANE_MAX] = { 0 };
    bool touch_enabled;
    bool have_bottom = false;
    bool inventory_enabled = false;
    bool worn_enabled = false;
    bool allow_right_panes = has_controller;
    int bottom_rows = 0;
    int final_main_cols = 0;
    int final_main_rows = 0;

    if (!screen)
        return;

    config.min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;

    memcpy(selected, pane_config, sizeof(selected));
    sdl_mobile_reset_default_pane_configs(selected, pane_config_count);

    touch_enabled = !has_controller;
    sdl_mobile_set_touch_pane_enabled(selected, pane_config_count,
        touch_enabled);

    config.main_view_scale = sdl_mobile_select_default_scale(screen, selected,
        pane_config_count);

    for (int rows = 4; rows >= 1; rows--) {
        memcpy(candidate, selected, sizeof(candidate));
        sdl_mobile_configure_bottom_wide(candidate, pane_config_count, rows);

        if (!sdl_mobile_layout_fits(screen, config.main_view_scale, candidate,
                pane_config_count, panes, cell_widths, cell_heights, NULL,
                NULL))
            continue;
        if (sdl_mobile_pane_cols(panes, cell_widths, PANE_LOG) < 50)
            continue;

        memcpy(selected, candidate, sizeof(selected));
        have_bottom = true;
        bottom_rows = rows;
        break;
    }

    if (!have_bottom) {
        for (int total_rows = 7; total_rows >= 1; total_rows--) {
            memcpy(candidate, selected, sizeof(candidate));
            sdl_mobile_configure_bottom_narrow(candidate, pane_config_count,
                total_rows);

            if (!sdl_mobile_layout_fits(screen, config.main_view_scale,
                    candidate, pane_config_count, NULL, NULL, NULL, NULL,
                    NULL))
                continue;

            memcpy(selected, candidate, sizeof(selected));
            have_bottom = true;
            bottom_rows = total_rows;
            break;
        }
    }

    if (allow_right_panes) {
        memcpy(candidate, selected, sizeof(candidate));
        sdl_mobile_set_right_panes(candidate, pane_config_count, true, false);
        if (sdl_mobile_layout_fits(screen, config.main_view_scale, candidate,
                pane_config_count, NULL, NULL, NULL, NULL, NULL))
        {
            memcpy(selected, candidate, sizeof(selected));
            inventory_enabled = true;
        }

        if (inventory_enabled) {
            memcpy(candidate, selected, sizeof(candidate));
            sdl_mobile_set_right_panes(candidate, pane_config_count, true, true);
            if (sdl_mobile_layout_fits(screen, config.main_view_scale, candidate,
                    pane_config_count, NULL, NULL, NULL, NULL, NULL))
            {
                memcpy(selected, candidate, sizeof(selected));
                worn_enabled = true;
            }
        }
    }

    memcpy(pane_config, selected, sizeof(selected));
    config.enable_bottom_panes = have_bottom;
    config.enable_right_panes = (inventory_enabled || worn_enabled);

    sdl_mobile_layout_fits(screen, config.main_view_scale, pane_config,
        pane_config_count, panes, cell_widths, cell_heights, &final_main_cols,
        &final_main_rows);

    log_info("Mobile default pane layout: controller=%s touch=%s scale=%d main=%dx%d",
        has_controller ? "yes" : "no",
        touch_enabled ? "on" : "off",
        config.main_view_scale, final_main_cols, final_main_rows);
    if (have_bottom) {
        log_info("Mobile default bottom pane: combined log, %d row%s",
            bottom_rows, (bottom_rows == 1) ? "" : "s");
    } else {
        log_info("Mobile default bottom pane: off");
    }
    log_info("Mobile default right panes: inventory=%s worn=%s",
        inventory_enabled ? "on" : "off",
        worn_enabled ? "on" : "off");
}
#endif

bool sdl_mobile_maybe_apply_first_start_auto_scale(const char* reason)
{
    SDL_Rect screen;
    int old_scale;
    int max_scale;

    if (!g_mobile_first_start_auto_scale_pending)
        return false;

    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return false;

    old_scale = config.main_view_scale;
    max_scale = sdl_max_scale_for_rect(&screen);
    for (int scale = max_scale; scale > old_scale; scale--) {
        if (!sdl_mobile_layout_fits(&screen, scale, pane_config,
                pane_config_count, NULL, NULL, NULL, NULL, NULL))
            continue;

        config.main_view_scale = scale;
        g_mobile_first_start_auto_scale_pending = false;
        log_info("%s: raised first-start mobile main_view_scale from %d to %d after window layout settled",
            reason ? reason : "startup",
            old_scale, config.main_view_scale);
        return true;
    }

    return false;
}
#endif

bool sdl_touch_pane_is_config_enabled(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_TOUCH)
            return pane_config[i].enabled;
    }
    return false;
}

bool sdl_touch_pane_is_left_placement(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane != PANE_TOUCH)
            continue;

        return pane_config[i].where == PLACE_DOUBLE_LEFT
            || pane_config[i].where == PLACE_LEFT;
    }

    return false;
}

bool sdl_touch_only_mobile_device_active(void)
{
#if SIL_SDL_MOBILE_BUILD
    return g_direct_touch_present
        && g_startup_device_class == SDL_STARTUP_DEVICE_MOBILE_TOUCH
        && !steamdeck_controls_active();
#else
    return false;
#endif
}

bool sdl_touch_tutorial_device_available(void)
{
    return g_direct_touch_present;
}

bool sdl_touch_tutorial_full_mode(void)
{
    return sdl_touch_only_mobile_device_active();
}

bool sdl_touch_pane_uses_mobile_toggle(void)
{
    return sdl_touch_only_mobile_device_active()
        && sdl_touch_pane_is_config_enabled();
}

bool sdl_touch_pane_mobile_layout_open(void)
{
    if (!sdl_touch_pane_uses_mobile_toggle())
        return true;

    return g_touch_pane_mobile_open;
}

bool sdl_touch_pane_hidden_mode_active(void)
{
    return screen_startup_touch_pane_hidden_active()
        || screen_touch_pane_hidden_active();
}

bool sdl_touch_pane_proto_mode_active(void)
{
    return screen_touch_pane_proto_active()
        && sdl_touch_only_mobile_device_active();
}

bool sdl_touch_pane_proto_slot_allowed(int slot)
{
    switch (slot) {
    case SDL_TOUCH_PANE_ESC_SLOT:
    case SDL_TOUCH_PANE_NORTH_SLOT:
    case SDL_TOUCH_PANE_WEST_SLOT:
    case SDL_TOUCH_PANE_CENTER_SLOT:
    case SDL_TOUCH_PANE_EAST_SLOT:
    case SDL_TOUCH_PANE_SOUTH_SLOT:
        return true;
    default:
        return false;
    }
}

bool sdl_touch_pane_slot_visible_in_current_mode(int slot)
{
    return !sdl_touch_pane_proto_mode_active()
        || sdl_touch_pane_proto_slot_allowed(slot);
}

int sdl_touch_pane_proto_binding_for_slot(int slot)
{
    switch (slot) {
    case SDL_TOUCH_PANE_ESC_SLOT: return ESCAPE;
    case SDL_TOUCH_PANE_NORTH_SLOT: return '8';
    case SDL_TOUCH_PANE_WEST_SLOT: return '4';
    case SDL_TOUCH_PANE_CENTER_SLOT: return INPUT_BIND_CONFIRM;
    case SDL_TOUCH_PANE_EAST_SLOT: return '6';
    case SDL_TOUCH_PANE_SOUTH_SLOT: return '2';
    default: return GAMEPAD_BIND_NONE;
    }
}

void sdl_touch_pane_refresh_after_layout_toggle(void)
{
    if (p_ptr && character_generated && !character_icky) {
        p_ptr->update |= PU_PANEL;
        p_ptr->redraw |= PR_BASIC | PR_EXTRA | PR_MAP | PR_DEPTH;
        handle_stuff();
        if (Term)
            Term_fresh();
        g_state.need_present = true;
        return;
    }

    sdl_request_redraw();
}

void sdl_touch_pane_set_mobile_open(bool open)
{
    if (!sdl_touch_pane_uses_mobile_toggle())
        return;
    if (g_touch_pane_mobile_open == open)
        return;

    g_touch_pane_mobile_open = open;
    if (!open) {
        sdl_touch_pane_cancel_press();
    } else {
        sdl_touch_zone_cancel_press();
        sdl_touch_top_panel_cancel_press();
    }

    if (g_state.window) {
        sdl_resize_for_current_layout();
        sdl_touch_pane_refresh_after_layout_toggle();
    } else {
        g_state.need_present = true;
    }
}

bool sdl_touch_pane_panel_is_valid(int panel)
{
    return (panel >= 0 && panel < SDL_TOUCH_PANE_PANEL_COUNT);
}

int sdl_touch_pane_active_panel(void)
{
    return g_touch_pane_second_panel ? SDL_TOUCH_PANE_PANEL_SECOND : SDL_TOUCH_PANE_PANEL_MAIN;
}

int sdl_touch_pane_other_panel(int panel)
{
    return (panel == SDL_TOUCH_PANE_PANEL_SECOND) ? SDL_TOUCH_PANE_PANEL_MAIN
                                                  : SDL_TOUCH_PANE_PANEL_SECOND;
}

int sdl_touch_pane_raw_binding_for_panel(int panel, int index)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return GAMEPAD_BIND_NONE;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        return config.touch_pane_second_bindings[index];

    return config.touch_pane_bindings[index];
}

int sdl_touch_pane_effective_binding_for_panel(int panel, int index)
{
    int binding;

    if (sdl_touch_pane_proto_mode_active())
        return sdl_touch_pane_proto_binding_for_slot(index);

    binding = sdl_touch_pane_raw_binding_for_panel(panel, index);
    if (panel == SDL_TOUCH_PANE_PANEL_SECOND && binding == TOUCH_PANE_BIND_INHERIT)
        return config.touch_pane_bindings[index];

    return binding;
}

bool sdl_query_direct_touch_present(void)
{
#if SIL_SDL_MOBILE_BUILD
    return true;
#elif defined(SDL_PLATFORM_WINDOWS)
    int digitizer = GetSystemMetrics(SM_DIGITIZER);
    bool ready = (digitizer & NID_READY) != 0;
    bool has_touch = (digitizer & (NID_INTEGRATED_TOUCH | NID_EXTERNAL_TOUCH)) != 0;

    log_debug("Windows digitizer flags: 0x%x", digitizer);
    return ready && has_touch;
#else
    SDL_TouchID* devices;
    int count = 0;
    bool present = false;

    devices = SDL_GetTouchDevices(&count);
    if (!devices)
        return false;

    for (int i = 0; i < count; i++) {
        if (devices[i] == SDL_MOUSE_TOUCHID || devices[i] == SDL_PEN_TOUCHID)
            continue;
        if (SDL_GetTouchDeviceType(devices[i]) == SDL_TOUCH_DEVICE_DIRECT) {
            present = true;
            break;
        }
    }

    SDL_free(devices);
    return present;
#endif
}

bool sdl_refresh_direct_touch_present(void)
{
    bool present = sdl_query_direct_touch_present();
    bool changed = (g_direct_touch_present != present);

    g_direct_touch_present = present;
    return changed;
}

void sdl_note_touch_event_device(SDL_TouchID touch_id)
{
    if (g_direct_touch_present)
        return;
    if (touch_id == SDL_MOUSE_TOUCHID || touch_id == SDL_PEN_TOUCHID)
        return;
    if (SDL_GetTouchDeviceType(touch_id) != SDL_TOUCH_DEVICE_DIRECT)
        return;

    g_direct_touch_present = true;
    g_touch_mouse_fallback_active = false;
    sdl_update_cursor_visibility();
    sdl_refresh_supporting_panes_layout();
}

void sdl_update_cursor_visibility(void)
{
    bool show_cursor = config.mouse_enabled
        && !(config.fullscreen && g_direct_touch_present);

    if (show_cursor)
        SDL_ShowCursor();
    else
        SDL_HideCursor();
}

void sdl_log_mouse_devices(void)
{
    int count = 0;
    SDL_MouseID* mice = SDL_GetMice(&count);

    if (!mice) {
        log_debug("SDL_GetMice failed: %s", SDL_GetError());
        return;
    }

    log_info("SDL_GetMice returned %d mouse device%s",
        count, (count == 1) ? "" : "s");
    for (int i = 0; i < count; i++) {
        const char* name = SDL_GetMouseNameForID(mice[i]);

        log_info("Mouse device id %u (%s)",
            (unsigned)mice[i], (name && name[0]) ? name : "unnamed");
    }

    SDL_free(mice);
}

int sdl_build_active_pane_config(struct pane_config* active, bool include_side,
    bool include_bottom, bool touch_only)
{
    int active_count = 0;
    bool proto_touch = sdl_touch_pane_proto_mode_active();

    if (proto_touch)
        sdl_ensure_touch_pane_config_present();

    for (int i = 0; i < pane_config_count && active_count < MAX_PANE_CONFIGS; i++) {
        enum pane_placement where = pane_config[i].where;
        bool is_touch_pane = (pane_config[i].pane == PANE_TOUCH);
        bool is_status_pane = (pane_config[i].pane == PANE_STATUS);
        bool is_left_panel = (pane_config[i].pane == PANE_LEFT_PANEL);
        bool is_depth_pane = (pane_config[i].pane == PANE_DEPTH);
        bool is_combat_pane = (pane_config[i].pane == PANE_COMBAT);
        bool is_description_pane = (pane_config[i].pane == PANE_DESCRIPTION);
        bool is_overlay_menu_pane =
            (pane_config[i].pane == PANE_OVERLAY_MENU);
        bool is_overlay_log_pane = (pane_config[i].pane == PANE_ROLLS)
            && pane_placement_is_overlay(where);

        if (pane_config[i].pane == PANE_MAIN_MENU)
            continue;
        if (touch_only && !is_touch_pane && !is_status_pane
            && !is_left_panel && !is_depth_pane
            && !is_combat_pane
            && !is_description_pane && !is_overlay_menu_pane
            && !is_overlay_log_pane)
        {
            continue;
        }
        if (is_touch_pane && !proto_touch && sdl_touch_pane_hidden_mode_active())
            continue;
        if (is_touch_pane && !proto_touch && !sdl_touch_pane_mobile_layout_open())
            continue;
        if (!is_touch_pane && !is_status_pane && !is_left_panel
            && !is_depth_pane && !is_combat_pane
            && !is_description_pane && !is_overlay_menu_pane
            && !is_overlay_log_pane)
        {
            if (pane_placement_is_side(where) && !include_side)
                continue;
            if (pane_placement_is_bottom(where) && !include_bottom)
                continue;
        }

        active[active_count] = pane_config[i];
        if (is_touch_pane && proto_touch) {
            active[active_count].enabled = true;
            if (!pane_placement_is_side(active[active_count].where))
                active[active_count].where = PLACE_DOUBLE_RIGHT;
        }
        active_count++;
    }

    return active_count;
}

int sdl_configured_main_view_scale(void)
{
    int scale = config.main_view_scale;

    return sdl_clamp_main_view_scale_platform_bounds(scale,
        config.min_terminal_mode);
}

int sdl_main_view_layout_scale(void)
{
    int scale = g_main_view_layout_scale_override;

    if (scale <= 0)
        scale = g_terminal_menu_scale_override;
    if (scale <= 0)
        scale = sdl_configured_main_view_scale();

    return sdl_clamp_main_view_scale_platform_bounds(scale,
        config.min_terminal_mode);
}

bool sdl_main_view_zoom_keep_for_saved_screen(void)
{
    return g_unified_look_active;
}

int sdl_current_main_view_scale(void)
{
    int scale;

    if (g_main_view_layout_scale_override > 0)
        scale = g_main_view_layout_scale_override;
    else if (g_terminal_menu_scale_override > 0)
        scale = g_terminal_menu_scale_override;
    else if (g_main_view_zoom_scale > 0) {
        scale = g_main_view_zoom_scale;
        if (scale < sdl_main_view_scale_floor())
            scale = sdl_main_view_scale_floor();
        if (scale > SDL_MAIN_VIEW_MAX_SCALE)
            scale = SDL_MAIN_VIEW_MAX_SCALE;
        return scale;
    } else {
        scale = config.main_view_scale;
    }

    return sdl_clamp_main_view_scale_platform_bounds(scale,
        config.min_terminal_mode);
}

int sdl_auto_font_size_from_main(int numerator, int denominator)
{
    float system_scale = (g_state.system_scale > 0.0f) ? g_state.system_scale : 1.0f;
    int main_cell_h_px = sdl_configured_main_view_scale() * TILE_SIZE;
    int main_font_size;
    int size;

    if (g_auto_aux_main_cell_h_override > 0)
        main_cell_h_px = g_auto_aux_main_cell_h_override;

    main_font_size = (int)((float)main_cell_h_px / system_scale + 0.5f);
    size = (main_font_size * numerator + denominator - 1) / denominator;

    if (size >= main_font_size && main_font_size > 8)
        size = main_font_size - 1;

    if (size < 8)
        size = 8;
    if (size > 48)
        size = 48;

    return size;
}

int sdl_auto_aux_view_font_size(void)
{
    return sdl_auto_font_size_from_main(2, 3);
}

int sdl_auto_pane_font_size(enum pane_type type)
{
    if (type == PANE_STATUS)
        return sdl_auto_font_size_from_main(1, 2);
    if (type == PANE_LEFT_PANEL || type == PANE_COMBAT || type == PANE_LOG)
#if SIL_SDL_MOBILE_BUILD
        /* Mobile: enlarge left-panel/combat/log fonts one notch (3/4 -> 4/5). */
        return sdl_auto_font_size_from_main(4, 5);
#else
        return sdl_auto_font_size_from_main(3, 4);
#endif

    return sdl_auto_aux_view_font_size();
}

int sdl_resolve_aux_view_font_size(int requested_size)
{
    int size = requested_size;

    if (size <= 0)
        size = sdl_auto_aux_view_font_size();
    if (size < 8)
        size = 8;
    if (size > 48)
        size = 48;

    return size;
}

int sdl_resolve_pane_font_size(enum pane_type type, int requested_size)
{
    int size = requested_size;

    if (size <= 0)
        size = sdl_auto_pane_font_size(type);
    if (size < 8)
        size = 8;
    if (size > 48)
        size = 48;

    return size;
}

int sdl_effective_pane_font_size_for_config(const struct pane_config* pc)
{
    if (pc && pc->font_size > 0)
        return sdl_resolve_aux_view_font_size(pc->font_size);
    if (pc)
        return sdl_resolve_pane_font_size(pc->pane, config.aux_view_font_size);

    return sdl_resolve_aux_view_font_size(config.aux_view_font_size);
}

int sdl_effective_pane_font_size_for_type(enum pane_type type)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == type)
            return sdl_effective_pane_font_size_for_config(&pane_config[i]);
    }

    return sdl_resolve_pane_font_size(type, config.aux_view_font_size);
}

int sdl_aux_cell_height_for_font_size(int font_size)
{
    int cell_h = (int)(g_state.system_scale * (float)font_size + 0.5f);

    if (cell_h < 1)
        cell_h = 1;

    return cell_h;
}

int sdl_effective_pane_cell_height_for_config(const struct pane_config* pc)
{
    return sdl_aux_cell_height_for_font_size(
        sdl_effective_pane_font_size_for_config(pc));
}

int sdl_effective_pane_cell_height_for_type(enum pane_type type)
{
    return sdl_aux_cell_height_for_font_size(
        sdl_effective_pane_font_size_for_type(type));
}

int sdl_supporting_pane_cell_width(enum pane_type type,
    enum pane_placement where, int cell_h)
{
    int cell_w = cell_h / 2;

    (void)type;
    (void)where;

    if (cell_w < 1)
        cell_w = 1;

    return cell_w;
}

void sdl_build_supporting_pane_metrics(const struct pane_config* configs,
    int count, int* cell_widths, int* cell_heights)
{
    int default_font_size = sdl_resolve_aux_view_font_size(config.aux_view_font_size);
    int default_cell_h = sdl_aux_cell_height_for_font_size(default_font_size);
    int default_cell_w;

    if (!cell_widths || !cell_heights)
        return;

    if (default_cell_h < 1)
        default_cell_h = 1;
    default_cell_w = default_cell_h / 2;
    if (default_cell_w < 1)
        default_cell_w = 1;

    for (int i = 0; i < PANE_MAX; i++) {
        cell_widths[i] = default_cell_w;
        cell_heights[i] = default_cell_h;
    }

    {
        /* Pane placement is part of the persistent layout.  Runtime zoom only
         * changes how the main term is rendered inside that rectangle. */
        int main_scale = sdl_main_view_layout_scale();

        cell_widths[PANE_MAIN] = main_scale * TILE_SIZE / 2;
        cell_heights[PANE_MAIN] = main_scale * TILE_SIZE;
    }

    for (int i = 0; i < count; i++) {
        enum pane_type type = configs[i].pane;
        int cell_h;

        if (type <= PANE_MAIN || type >= PANE_MAX)
            continue;

        cell_h = sdl_effective_pane_cell_height_for_config(&configs[i]);
        cell_heights[type] = cell_h;
        cell_widths[type] = sdl_supporting_pane_cell_width(type,
            configs[i].where, cell_h);
    }
}

bool sdl_prune_unusable_panes(struct pane_config* active,
    int active_count, SDL_Rect* panes, const int* cell_widths,
    const int* cell_heights)
{
    bool pruned = false;

    for (int i = 0; i < active_count; i++) {
        struct pane_config* pc = &active[i];
        enum pane_type type = pc->pane;
        SDL_Rect* rect;
        int cell_w;
        int cell_h;
        int cols;
        int rows;
        int min_cols;
        int min_rows;

        if (!pc->enabled)
            continue;
        if (type <= PANE_MAIN || type >= PANE_MAX)
            continue;

        rect = &panes[type];
        if (rect->w <= 0 || rect->h <= 0)
            continue;

        cell_w = cell_widths[type];
        cell_h = cell_heights[type];
        if (cell_w <= 0 || cell_h <= 0) {
            pc->enabled = false;
            *rect = (SDL_Rect){ 0 };
            pruned = true;
            continue;
        }

        cols = rect->w / cell_w;
        rows = rect->h / cell_h;
        if (pane_placement_is_side(pc->where)) {
            min_cols = pane_primary_min_cells(type, pc->where);
            min_rows = pane_secondary_min_cells(type, pc->where);
        } else {
            min_cols = pane_secondary_min_cells(type, pc->where);
            min_rows = pane_primary_min_cells(type, pc->where);
        }
        if (type == PANE_COMBAT && min_rows > PANE_COMBAT_OVERLAY_MIN_ROWS)
            min_rows = PANE_COMBAT_OVERLAY_MIN_ROWS;

        if (cols >= min_cols && rows >= min_rows)
            continue;

        log_info("Skipping pane %d at (%d,%d) size %dx%d: fits %dx%d cells, needs %dx%d",
            type, rect->x, rect->y, rect->w, rect->h, cols, rows, min_cols, min_rows);
        pc->enabled = false;
        *rect = (SDL_Rect){ 0 };
        pruned = true;
    }

    return pruned;
}

int sdl_inventory_pane_desired_rows(void)
{
    int rows = 0;

    if (!character_generated || !p_ptr || !inventory)
        return -1;

    for (int i = 0; i < INVEN_PACK; i++) {
        if (inventory[i].k_idx)
            rows = i + 1;
    }

    if (p_ptr->inven_cnt > rows)
        rows = p_ptr->inven_cnt;
    if (rows < 1)
        rows = 1;
    if (rows > INVEN_PACK)
        rows = INVEN_PACK;

    return rows;
}

bool sdl_inventory_pane_dynamic_configured(void)
{
    if (!character_generated || !p_ptr || !inventory)
        return false;
    if (!config.enable_right_panes)
        return false;

    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane != PANE_INVENTORY)
            continue;
        if (!pane_config[i].enabled)
            continue;
        if (!pane_placement_is_side(pane_config[i].where))
            continue;

        return true;
    }

    return false;
}

void sdl_apply_dynamic_inventory_pane_size(struct pane_config* active,
    int active_count)
{
    int rows = sdl_inventory_pane_desired_rows();

    if (rows < 1)
        return;

    for (int i = 0; i < active_count; i++) {
        if (!active[i].enabled)
            continue;
        if (active[i].pane != PANE_INVENTORY)
            continue;
        if (!pane_placement_is_side(active[i].where))
            continue;

        active[i].rect.rows = rows;
    }
}

int sdl_supply_pane_desired_rows(void)
{
    int rows;

    if (!character_generated || !p_ptr)
        return -1;

    rows = supplies_entry_count() + 1;
    if (rows < 2)
        rows = 2;
    if (rows > 27)
        rows = 27;

    return rows;
}

bool sdl_supply_pane_dynamic_configured(void)
{
    if (!character_generated || !p_ptr)
        return false;
    if (!config.enable_right_panes)
        return false;

    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane != PANE_SUPPLY)
            continue;
        if (!pane_config[i].enabled)
            continue;
        if (!pane_placement_is_side(pane_config[i].where))
            continue;

        return true;
    }

    return false;
}

void sdl_apply_dynamic_supply_pane_size(struct pane_config* active,
    int active_count)
{
    int rows = sdl_supply_pane_desired_rows();

    if (rows < 1)
        return;

    for (int i = 0; i < active_count; i++) {
        if (!active[i].enabled)
            continue;
        if (active[i].pane != PANE_SUPPLY)
            continue;
        if (!pane_placement_is_side(active[i].where))
            continue;

        active[i].rect.rows = rows;
    }
}

int sdl_touch_pane_target_width_px(int pane_height_px)
{
    const int numerator = 40 * SDL_TOUCH_PANE_BUTTON_COLS;
    const int denominator = 39 * SDL_TOUCH_PANE_VISIBLE_BUTTON_ROWS
        + SDL_TOUCH_PANE_BUTTON_COLS;

    if (pane_height_px <= 0)
        return 0;

    return (pane_height_px * numerator + denominator - 1) / denominator;
}

void sdl_apply_dynamic_auto_pane_sizes(struct pane_config* active,
    int active_count, const SDL_Rect* screen, const int* cell_widths,
    const int* cell_heights, int margin_px)
{
    SDL_Rect temp_panes[PANE_MAX] = { 0 };
    int touch_idx = -1;
    int min_touch_cols;
    int min_touch_width_px;
    int desired_touch_px;
    int desired_touch_cols;
    int max_touch_px;
    int max_touch_cols;
    int min_main_width_px;

    if (!active || active_count <= 0 || !screen || !cell_widths || !cell_heights)
        return;

    sdl_apply_dynamic_inventory_pane_size(active, active_count);
    sdl_apply_dynamic_supply_pane_size(active, active_count);

    for (int i = 0; i < active_count; i++) {
        if (!active[i].enabled)
            continue;
        if (active[i].pane != PANE_TOUCH)
            continue;
        if (!pane_placement_is_side(active[i].where))
            continue;
        if (active[i].rect.cols > 0)
            continue;

        touch_idx = i;
        break;
    }

    if (touch_idx < 0)
        return;

    place_panes(active, active_count, temp_panes, screen, cell_widths,
        cell_heights,
        margin_px);

    if (temp_panes[PANE_TOUCH].w <= 0 || temp_panes[PANE_TOUCH].h <= 0)
        return;

    min_touch_cols = pane_primary_min_cells(PANE_TOUCH, active[touch_idx].where);
    min_touch_width_px = min_touch_cols * cell_widths[PANE_TOUCH] + margin_px;
    desired_touch_px = sdl_touch_pane_target_width_px(temp_panes[PANE_TOUCH].h);
    if (desired_touch_px < min_touch_width_px)
        desired_touch_px = min_touch_width_px;

    desired_touch_cols = (desired_touch_px > margin_px)
        ? ((desired_touch_px - margin_px + cell_widths[PANE_TOUCH] - 1)
            / cell_widths[PANE_TOUCH])
        : min_touch_cols;
    if (desired_touch_cols < min_touch_cols)
        desired_touch_cols = min_touch_cols;

    min_main_width_px = sdl_current_min_terminal_cols() * cell_widths[PANE_MAIN];
    max_touch_px = temp_panes[PANE_MAIN].w + temp_panes[PANE_TOUCH].w
        - min_main_width_px;

    if (max_touch_px >= min_touch_width_px) {
        max_touch_cols = (max_touch_px > margin_px)
            ? ((max_touch_px - margin_px) / cell_widths[PANE_TOUCH])
            : 0;
        if (max_touch_cols < min_touch_cols)
            max_touch_cols = min_touch_cols;
        if (desired_touch_cols > max_touch_cols)
            desired_touch_cols = max_touch_cols;
    }

    active[touch_idx].rect.cols = desired_touch_cols;
}

void sdl_place_active_panes(const SDL_Rect* screen, SDL_Rect* panes,
    bool include_side, bool include_bottom, bool touch_only)
{
    struct pane_config active[MAX_PANE_CONFIGS] = { 0 };
    int active_count;
    int cell_widths[PANE_MAX] = { 0 };
    int cell_heights[PANE_MAX] = { 0 };
    int margin_px;

    if (!screen || !panes)
        return;

    memset(panes, 0, sizeof(SDL_Rect) * PANE_MAX);

    margin_px = (int)(g_state.system_scale * config.margin);
    active_count = sdl_build_active_pane_config(active, include_side,
        include_bottom, touch_only);
    sdl_build_supporting_pane_metrics(active, active_count, cell_widths,
        cell_heights);
    sdl_apply_dynamic_auto_pane_sizes(active, active_count, screen, cell_widths,
        cell_heights, margin_px);

    for (int attempt = 0; attempt <= active_count; attempt++) {
        place_panes(active, active_count, panes, screen, cell_widths,
            cell_heights, margin_px);
        if (!sdl_prune_unusable_panes(active, active_count, panes, cell_widths,
                cell_heights))
            break;
        memset(panes, 0, sizeof(SDL_Rect) * PANE_MAX);
    }
}

bool sdl_active_group_has_visible(const struct pane_config* active,
    int active_count, const SDL_Rect* panes, bool side)
{
    for (int i = 0; i < active_count; i++) {
        SDL_Rect rect;
        bool matches;

        if (!active[i].enabled)
            continue;
        if (pane_placement_is_overlay(active[i].where))
            continue;
        matches = side ? pane_placement_is_side(active[i].where)
                       : pane_placement_is_bottom(active[i].where);
        if (!matches)
            continue;

        rect = panes[active[i].pane];
        if (rect.w > 0 && rect.h > 0)
            return true;
    }

    return false;
}

void sdl_place_active_panes_fitting_main(const SDL_Rect* screen,
    SDL_Rect* panes, bool include_side, bool include_bottom, bool touch_only,
    bool* out_side, bool* out_bottom)
{
    struct pane_config base[MAX_PANE_CONFIGS] = { 0 };
    struct pane_config active[MAX_PANE_CONFIGS] = { 0 };
    int active_count;
    int cell_widths[PANE_MAX] = { 0 };
    int cell_heights[PANE_MAX] = { 0 };
    int margin_px;

    if (out_side)
        *out_side = false;
    if (out_bottom)
        *out_bottom = false;
    if (!screen || !panes)
        return;

    memset(panes, 0, sizeof(SDL_Rect) * PANE_MAX);

    margin_px = (int)(g_state.system_scale * config.margin);
    active_count = sdl_build_active_pane_config(base, include_side,
        include_bottom, touch_only);
    memcpy(active, base, sizeof(struct pane_config) * active_count);
    sdl_build_supporting_pane_metrics(active, active_count, cell_widths,
        cell_heights);
    sdl_apply_dynamic_auto_pane_sizes(active, active_count, screen,
        cell_widths, cell_heights, margin_px);

    for (int attempt = 0; attempt <= active_count; attempt++) {
        place_panes(active, active_count, panes, screen, cell_widths,
            cell_heights, margin_px);
        if (!sdl_prune_unusable_panes(active, active_count, panes,
                cell_widths, cell_heights))
        {
            break;
        }
        memset(panes, 0, sizeof(SDL_Rect) * PANE_MAX);
    }

    if (out_side)
        *out_side = sdl_active_group_has_visible(active, active_count, panes,
            true);
    if (out_bottom)
        *out_bottom = sdl_active_group_has_visible(active, active_count, panes,
            false);
}

int sdl_bottom_pane_group_rows_for_minimum(const SDL_Rect* panes,
    enum pane_placement where)
{
    int rows = 0;

    if (!panes)
        return 0;

    for (int i = 0; i < pane_config_count; i++) {
        const struct pane_config* pc = &pane_config[i];
        const SDL_Rect* rect;
        int cell_h;
        int pane_rows;

        if (!pc->enabled || pc->where != where)
            continue;
        if (pc->pane <= PANE_MAIN || pc->pane >= PANE_MAX)
            continue;

        rect = &panes[pc->pane];
        if (rect->w <= 0 || rect->h <= 0)
            continue;

        cell_h = sdl_effective_pane_cell_height_for_config(pc);
        if (cell_h <= 0)
            continue;

        pane_rows = rect->h / cell_h;
        if (pane_rows > rows)
            rows = pane_rows;
    }

    return rows;
}

int sdl_bottom_pane_rows_for_minimum(const SDL_Rect* panes)
{
    return sdl_bottom_pane_group_rows_for_minimum(panes, PLACE_DOUBLE_BOTTOM)
        + sdl_bottom_pane_group_rows_for_minimum(panes, PLACE_BOTTOM);
}

void sdl_compute_pruned_split_panes_for_mode_ex(const SDL_Rect* screen,
    int mode, int scale, bool aux_follows_candidate_scale, SDL_Rect* panes,
    bool* out_side, bool* out_bottom, int* out_cols, int* out_rows,
    int* out_rows_with_bottom)
{
    SDL_Rect local_panes[PANE_MAX] = { 0 };
    SDL_Rect* target_panes = panes ? panes : local_panes;
    int saved_mode = config.min_terminal_mode;
    int saved_scale = config.main_view_scale;
    int saved_layout_scale_override = g_main_view_layout_scale_override;
    int saved_aux_override = g_auto_aux_main_cell_h_override;
    bool include_side = config.enable_right_panes;
    bool include_bottom = config.enable_bottom_panes;
    int layout_scale;
    int cell_w;
    int cell_h;
    int cols = 0;
    int rows = 0;

    if (panes)
        memset(panes, 0, sizeof(SDL_Rect) * PANE_MAX);
    if (out_side)
        *out_side = include_side;
    if (out_bottom)
        *out_bottom = include_bottom;
    if (out_cols)
        *out_cols = 0;
    if (out_rows)
        *out_rows = 0;
    if (out_rows_with_bottom)
        *out_rows_with_bottom = 0;

    if (!screen || !sdl_rect_has_area(screen) || scale <= 0)
        return;
    if (!sdl_min_terminal_mode_is_valid(mode))
        return;

    /* Temporary zoom changes the rendered main view scale, but pane
     * visibility/layout still follows the saved main scale. */
    layout_scale = aux_follows_candidate_scale
        ? scale
        : sdl_configured_main_view_scale();
    config.min_terminal_mode = mode;
    g_main_view_layout_scale_override = layout_scale;
    if (aux_follows_candidate_scale) {
        config.main_view_scale = scale;
        g_auto_aux_main_cell_h_override = scale * TILE_SIZE;
    } else {
        g_auto_aux_main_cell_h_override = layout_scale * TILE_SIZE;
    }

    cell_w = scale * TILE_SIZE / 2;
    cell_h = scale * TILE_SIZE;
    sdl_place_active_panes_fitting_main(screen, target_panes, include_side,
        include_bottom, false, &include_side, &include_bottom);
    cols = (cell_w > 0)
        ? sdl_main_view_logical_cols_for_visual_cols(
            target_panes[PANE_MAIN].w / cell_w)
        : 0;
    rows = (cell_h > 0) ? target_panes[PANE_MAIN].h / cell_h : 0;
    if (out_rows_with_bottom)
        *out_rows_with_bottom = rows
            + sdl_bottom_pane_rows_for_minimum(target_panes);

    config.min_terminal_mode = saved_mode;
    config.main_view_scale = saved_scale;
    g_main_view_layout_scale_override = saved_layout_scale_override;
    g_auto_aux_main_cell_h_override = saved_aux_override;

    if (out_side)
        *out_side = include_side;
    if (out_bottom)
        *out_bottom = include_bottom;
    if (out_cols)
        *out_cols = cols;
    if (out_rows)
        *out_rows = rows;
}

void sdl_mark_active_supporting_panes_dirty(const SDL_Rect* panes)
{
    u32b flags = 0L;

    if (!panes || !p_ptr || !op_ptr)
        return;

    for (int i = 1; i < ANGBAND_TERM_MAX && i < PANE_MAX; i++) {
        if (panes[i].w <= 0 || panes[i].h <= 0)
            continue;
        if (!angband_term[i])
            continue;

        flags |= op_ptr->window_flag[i];
    }

    if (flags)
        p_ptr->window |= flags;
}

bool sdl_hide_supporting_panes_mode_effective(void)
{
    bool explicit_hide = screen_startup_supporting_panes_hidden_active()
        || screen_supporting_panes_hidden_active();

    /* Startup hidden mode must be able to take effect before persistent
     * options have been loaded into op_ptr. */
    if (explicit_hide)
        return true;

    if (!explicit_hide && op_ptr
        && !op_ptr->opt[OPT_hide_supporting_panes_fullscreen])
        return false;

    for (int i = 0; i < pane_config_count; i++) {
        enum pane_placement where = pane_config[i].where;

        if (!pane_config[i].enabled)
            continue;
        if (pane_config[i].pane == PANE_MAIN
            || pane_config[i].pane == PANE_TOUCH
            || pane_config[i].pane == PANE_STATUS
            || pane_config[i].pane == PANE_DEPTH
            || pane_config[i].pane == PANE_COMBAT
            || pane_config[i].pane == PANE_MAIN_MENU
            || pane_config[i].pane == PANE_DESCRIPTION
            || pane_config[i].pane == PANE_OVERLAY_MENU)
        {
            continue;
        }
        if (pane_placement_is_side(where) && !g_active_side_panes)
            continue;
        if (pane_placement_is_bottom(where) && !g_active_bottom_panes)
            continue;

        return true;
    }

    return false;
}

void sdl_draw_pane_edges(const SDL_Rect* rect, bool draw_left,
    bool draw_top, bool draw_right, bool draw_bottom)
{
    int x1;
    int y1;
    int x2;
    int y2;

    if (!rect || rect->w <= 0 || rect->h <= 0)
        return;

    x1 = rect->x;
    y1 = rect->y;
    x2 = rect->x + rect->w - 1;
    y2 = rect->y + rect->h - 1;

    if (x2 < x1 || y2 < y1)
        return;

    if (draw_top)
        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = (float)x1,
            .y = (float)y1,
            .w = (float)(x2 - x1 + 1),
            .h = 1.0f,
        });
    if (draw_bottom)
        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = (float)x1,
            .y = (float)y2,
            .w = (float)(x2 - x1 + 1),
            .h = 1.0f,
        });

    {
        int top_offset = draw_top ? 1 : 0;
        int bottom_offset = draw_bottom ? 1 : 0;
        int edge_height = y2 - y1 + 1 - top_offset - bottom_offset;

        if (edge_height > 0 && draw_left)
            SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
                .x = (float)x1,
                .y = (float)(y1 + top_offset),
                .w = 1.0f,
                .h = (float)edge_height,
            });
        if (edge_height > 0 && draw_right)
            SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
                .x = (float)x2,
                .y = (float)(y1 + top_offset),
                .w = 1.0f,
                .h = (float)edge_height,
            });
    }
}

bool sdl_should_show_supporting_panes(void)
{
    if (!sdl_hide_supporting_panes_mode_effective())
        return true;
    if (screen_startup_supporting_panes_hidden_active())
        return false;
    if (screen_supporting_panes_hidden_active())
        return false;
    if (screen_saved_fullscreen_active())
        return false;

    return true;
}

bool sdl_layout_matches_supporting_pane_visibility(void)
{
    bool show_supporting_panes = sdl_should_show_supporting_panes();

    if (g_supporting_panes_layout_visible != show_supporting_panes)
        return false;
    if (g_touch_pane_hidden_layout_active != sdl_touch_pane_hidden_mode_active())
        return false;
    if (g_touch_pane_proto_layout_active != sdl_touch_pane_proto_mode_active())
        return false;
    if (show_supporting_panes && sdl_inventory_pane_dynamic_configured()) {
        int rows = sdl_inventory_pane_desired_rows();

        if (rows > 0 && g_inventory_pane_layout_rows != rows)
            return false;
    }
    if (show_supporting_panes && sdl_supply_pane_dynamic_configured()) {
        int rows = sdl_supply_pane_desired_rows();

        if (rows > 0 && g_supply_pane_layout_rows != rows)
            return false;
    }

    return true;
}

void sdl_compute_display_panes(SDL_Rect* panes)
{
    SDL_Rect screen;

    if (!panes)
        return;

    if (!g_state.window || sdl_should_show_supporting_panes()) {
        memcpy(panes, g_pane_rects, sizeof(g_pane_rects));
        return;
    }

    screen = sdl_get_layout_screen_rect();
    sdl_place_active_panes(&screen, panes, g_active_side_panes,
        g_active_bottom_panes, true);
}

int sdl_max_scale_for_rect_mode(const SDL_Rect* rect, int mode)
{
    int min_cols;
    int min_rows;
    int max_scale_w;
    int max_scale_h;
    int max_scale;

    if (!rect)
        return SDL_MAIN_VIEW_MIN_SCALE;

    min_cols = sdl_min_terminal_cols_for_mode(mode);
    min_rows = sdl_min_terminal_rows_for_mode(mode);
    max_scale_w = (rect->w / min_cols) * 2 / TILE_SIZE;
    max_scale_h = rect->h / min_rows / TILE_SIZE;
    max_scale = (max_scale_w < max_scale_h) ? max_scale_w : max_scale_h;

    if (max_scale < SDL_MAIN_VIEW_MIN_SCALE)
        max_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale > SDL_MAIN_VIEW_MAX_SCALE)
        max_scale = SDL_MAIN_VIEW_MAX_SCALE;

    return max_scale;
}

int sdl_max_scale_for_rect(const SDL_Rect* rect)
{
    return sdl_max_scale_for_rect_mode(rect, config.min_terminal_mode);
}

bool sdl_apply_default_main_scale_for_layout(const char* reason)
{
    SDL_Rect screen;
    int old_scale;
    int new_scale;

    if (!g_state.window)
        return false;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return false;

    old_scale = config.main_view_scale;
    new_scale = sdl_max_scale_for_layout(&screen, config.min_terminal_mode);
    new_scale = sdl_clamp_main_view_scale_floor(new_scale,
        config.min_terminal_mode);

    config.main_view_scale = new_scale;
    g_main_view_zoom_scale = 0;

    if (old_scale != new_scale) {
        log_info("%s: default main_view_scale changed from %d to %d for layout %dx%d without supporting panes",
            reason ? reason : "startup", old_scale, new_scale,
            screen.w, screen.h);
        return true;
    }

    log_info("%s: default main_view_scale %d fits layout %dx%d without supporting panes",
        reason ? reason : "startup", new_scale, screen.w, screen.h);
    return false;
}

int sdl_max_scale_for_layout(const SDL_Rect* screen, int mode)
{
    int min_cols;
    int min_rows;

    if (!screen || !sdl_rect_has_area(screen))
        return SDL_MAIN_VIEW_MIN_SCALE;
    if (!sdl_min_terminal_mode_is_valid(mode))
        return SDL_MAIN_VIEW_MIN_SCALE;

    min_cols = sdl_min_terminal_cols_for_mode(mode);
    min_rows = sdl_min_terminal_rows_for_mode(mode);

    for (int scale = SDL_MAIN_VIEW_MAX_SCALE;
         scale >= SDL_MAIN_VIEW_MIN_SCALE; scale--) {
        int cols = 0;
        int rows = 0;
        int rows_with_bottom = 0;

        sdl_compute_pruned_split_panes_for_mode_ex(screen, mode, scale, true,
            NULL, NULL, NULL, &cols, &rows, &rows_with_bottom);
        if (cols >= min_cols && rows_with_bottom >= min_rows)
            return scale;
    }

    return SDL_MAIN_VIEW_MIN_SCALE;
}


int sdl_main_view_terminal_cols_for_map_squares(int map_cols)
{
    int tile_cols = use_bigtile ? 2 : 1;

    if (map_cols < 1)
        map_cols = 1;

    return COL_MAP + 1 + map_cols * tile_cols;
}

int sdl_main_view_terminal_rows_for_map_squares(int map_rows)
{
    if (map_rows < 1)
        map_rows = 1;

    return map_rows;
}

int sdl_main_view_map_cols_for_terminal_cols(int cols)
{
    int tile_cols = use_bigtile ? 2 : 1;
    int map_cols = (cols - COL_MAP - 1) / tile_cols;

    return (map_cols > 0) ? map_cols : 0;
}

int sdl_main_view_map_rows_for_terminal_rows(int rows)
{
    int map_rows = rows;

    return (map_rows > 0) ? map_rows : 0;
}

int sdl_max_main_view_zoom_scale_for_layout(const SDL_Rect* screen,
    int mode)
{
    if (!screen || !sdl_rect_has_area(screen))
        return SDL_MAIN_VIEW_MIN_SCALE;
    if (!sdl_min_terminal_mode_is_valid(mode))
        return SDL_MAIN_VIEW_MIN_SCALE;

    for (int scale = SDL_MAIN_VIEW_MAX_SCALE;
         scale >= SDL_MAIN_VIEW_MIN_SCALE; scale--) {
        int cols = 0;
        int rows = 0;
        int map_cols;
        int map_rows;

        sdl_compute_pruned_split_panes_for_mode_ex(screen, mode, scale, false,
            NULL, NULL, NULL, &cols, &rows, NULL);
        map_cols = sdl_main_view_map_cols_for_terminal_cols(cols);
        map_rows = sdl_main_view_map_rows_for_terminal_rows(rows);
        if (map_cols >= SDL_MAIN_VIEW_ZOOM_MIN_MAP_SQUARES
            || map_rows >= SDL_MAIN_VIEW_ZOOM_MIN_MAP_SQUARES)
        {
            return scale;
        }
    }

    return SDL_MAIN_VIEW_MIN_SCALE;
}

int sdl_min_main_view_zoom_scale_for_layout(const SDL_Rect* screen,
    int mode)
{
    int floor = sdl_main_view_scale_floor_for_mode(mode);

    if (!screen || !sdl_rect_has_area(screen))
        return floor;
    if (!sdl_min_terminal_mode_is_valid(mode))
        return floor;

    return floor;
}

int sdl_max_scale_for_window_mode(int mode)
{
    struct sdl_config saved_config = config;
    struct pane_config saved_panes[MAX_PANE_CONFIGS];
    SDL_Rect screen;
    int saved_pane_count = pane_config_count;
    int max_scale = SDL_MAIN_VIEW_MIN_SCALE;

    memcpy(saved_panes, pane_config, sizeof(saved_panes));

    config.min_terminal_mode = mode;
    sdl_apply_stored_pane_profile(mode);
    config.min_terminal_mode = mode;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    if (sdl_rect_has_area(&screen))
        max_scale = sdl_max_scale_for_layout(&screen, mode);

    config = saved_config;
    pane_config_count = saved_pane_count;
    memcpy(pane_config, saved_panes, sizeof(saved_panes));

    return max_scale;
}

bool sdl_mode_scale_fits_window(const SDL_Rect* screen, int mode,
    int scale, int* cols, int* rows)
{
    struct sdl_config saved_config = config;
    struct pane_config saved_panes[MAX_PANE_CONFIGS];
    SDL_Rect panes[PANE_MAX];
    int saved_pane_count = pane_config_count;
    int local_cols = 0;
    int local_rows = 0;
    int local_rows_with_bottom = 0;
    bool fits = false;

    if (!screen || !sdl_rect_has_area(screen) || scale <= 0)
        return false;

    memcpy(saved_panes, pane_config, sizeof(saved_panes));

    config.min_terminal_mode = mode;
    sdl_apply_stored_pane_profile(mode);
    config.min_terminal_mode = mode;
    sdl_compute_pruned_split_panes_for_mode_ex(screen, mode, scale, true,
        panes, NULL, NULL, &local_cols, &local_rows, &local_rows_with_bottom);
    fits = (local_cols >= sdl_min_terminal_cols_for_mode(mode)
        && local_rows_with_bottom >= sdl_min_terminal_rows_for_mode(mode));

    config = saved_config;
    pane_config_count = saved_pane_count;
    memcpy(pane_config, saved_panes, sizeof(saved_panes));

    if (cols)
        *cols = local_cols;
    if (rows)
        *rows = local_rows;

    return fits;
}

void sdl_ensure_window_size_for_min_terminal(const SDL_Rect* screen,
    int* window_width, int* window_height)
{
    int min_width;
    int min_height;
    int min_scale;

    if (!screen || !window_width || !window_height || config.fullscreen)
        return;

    min_scale = sdl_main_view_scale_floor();
    min_width = sdl_current_min_terminal_cols()
        * (min_scale * TILE_SIZE / 2);
    min_height = sdl_current_min_terminal_rows()
        * (min_scale * TILE_SIZE);

    if (min_width < 1)
        min_width = 1;
    if (min_height < 1)
        min_height = 1;

    if (screen->w > 0 && min_width > screen->w)
        min_width = screen->w;
    if (screen->h > 0 && min_height > screen->h)
        min_height = screen->h;

    if (*window_width < min_width) {
        log_info("Increasing initial window width from %d to %d to fit minimum terminal %dx%d (%s)",
            *window_width, min_width,
            sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows(),
            sdl_min_terminal_mode_name(config.min_terminal_mode));
        *window_width = min_width;
    }

    if (*window_height < min_height) {
        log_info("Increasing initial window height from %d to %d to fit minimum terminal %dx%d (%s)",
            *window_height, min_height,
            sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows(),
            sdl_min_terminal_mode_name(config.min_terminal_mode));
        *window_height = min_height;
    }
}

void sdl_format_layout_recovery_message(const char* reason,
    const sdl_layout_recovery_result* recovery, char* buf, size_t buflen)
{
    const char* prefix = "Layout recovery";

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!recovery)
        return;

    if (reason && reason[0]) {
        if (streq(reason, "startup"))
            prefix = "At startup";
        else if (streq(reason, "window resize"))
            prefix = "Window resize";
        else if (streq(reason, "display scale change"))
            prefix = "Display scale change";
        else if (streq(reason, "fullscreen change"))
            prefix = "Fullscreen change";
        else if (streq(reason, "settings change"))
            prefix = "Settings change";
        else
            prefix = reason;
    }

    if (recovery->mode_changed && recovery->scale_changed) {
        strnfmt(buf, buflen,
            "%s: switched to %s terminal layout and reduced main view scale from %d to %d to keep the window usable.",
            prefix, sdl_min_terminal_mode_name(recovery->new_mode),
            recovery->old_scale, recovery->new_scale);
    } else if (recovery->mode_changed) {
        strnfmt(buf, buflen,
            "%s: switched from %s to %s terminal layout to fit the current window.",
            prefix, sdl_min_terminal_mode_name(recovery->old_mode),
            sdl_min_terminal_mode_name(recovery->new_mode));
    } else if (recovery->scale_changed) {
        strnfmt(buf, buflen,
            "%s: reduced main view scale from %d to %d to keep the %s terminal visible.",
            prefix, recovery->old_scale, recovery->new_scale,
            sdl_min_terminal_mode_name(recovery->new_mode));
    }
}

void sdl_append_issue_line(char* buf, size_t buflen, const char* line)
{
    if (!buf || !buflen || !line || !line[0])
        return;

    if (buf[0])
        SDL_strlcat(buf, "\n", buflen);
    SDL_strlcat(buf, line, buflen);
}

bool sdl_recover_layout_for_current_window(const char* reason,
    bool notify_user, sdl_layout_recovery_result* recovery)
{
    SDL_Rect screen;
    sdl_layout_recovery_result local = {
        .mode_changed = false,
        .scale_changed = false,
        .old_mode = config.min_terminal_mode,
        .new_mode = config.min_terminal_mode,
        .old_scale = config.main_view_scale,
        .new_scale = config.main_view_scale,
    };
    char notice[256];

    if (!g_state.window)
        return false;

    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return false;

    if (config.min_terminal_mode == SDL_MIN_TERMINAL_NORMAL
        && !sdl_mode_scale_fits_window(&screen, SDL_MIN_TERMINAL_NORMAL,
            config.main_view_scale, NULL, NULL))
    {
        log_info("%s: normal minimum terminal no longer fits; activating compact layout",
            reason ? reason : "layout change");
        set_sdl_min_terminal_mode(SDL_MIN_TERMINAL_COMPACT);
        local.mode_changed = true;
        local.new_mode = config.min_terminal_mode;
        local.new_scale = config.main_view_scale;
    }

    if (!sdl_mode_scale_fits_window(&screen, config.min_terminal_mode,
            config.main_view_scale, NULL, NULL))
    {
        int max_scale = sdl_max_scale_for_window_mode(config.min_terminal_mode);
        int min_scale = sdl_main_view_scale_floor();

        if (max_scale < min_scale)
            max_scale = min_scale;

        if (config.main_view_scale > max_scale) {
            log_info("%s: clamping main_view_scale from %d to %d for %s minimum terminal",
                reason ? reason : "layout change",
                config.main_view_scale, max_scale,
                sdl_min_terminal_mode_name(config.min_terminal_mode));
            config.main_view_scale = max_scale;
            local.scale_changed = true;
            local.new_scale = config.main_view_scale;
        }
    }

    if (recovery)
        *recovery = local;

    if (!(local.mode_changed || local.scale_changed))
        return false;

    if (notify_user && Term) {
        sdl_format_layout_recovery_message(reason, &local, notice,
            sizeof(notice));
        if (notice[0])
            msg_print(notice);
    }

    return true;
}

bool sdl_prompt_reset_sdl_defaults(const char* issue_summary,
    int screen_width, int screen_height)
{
    enum {
        SDL_STARTUP_KEEP_RECOVERED = 0,
        SDL_STARTUP_LOAD_DEFAULTS = 1,
    };
    SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
            SDL_STARTUP_KEEP_RECOVERED, "Keep Recovered Settings" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
            SDL_STARTUP_LOAD_DEFAULTS, "Load Defaults" },
    };
    char message[SDL_STARTUP_ISSUE_MAX + 256];
    SDL_MessageBoxData messagebox = {
        .flags = SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT,
        .window = g_state.window,
        .title = "SDL Config Recovery",
        .message = message,
        .numbuttons = (int)(sizeof(buttons) / sizeof(buttons[0])),
        .buttons = buttons,
        .colorScheme = NULL,
    };
    int button_id = SDL_STARTUP_KEEP_RECOVERED;

    if (!issue_summary || !issue_summary[0])
        return false;

    strnfmt(message, sizeof(message),
        "Sil-more adjusted your SDL settings so the game can start:\n\n%s\n\nLoad default SDL settings now? You can keep the recovered settings and change them later from SDL Pane Settings.",
        issue_summary);

    if (!SDL_ShowMessageBox(&messagebox, &button_id)) {
        log_warn("SDL_ShowMessageBox failed during startup recovery prompt: %s",
            SDL_GetError());
        return false;
    }

    if (button_id != SDL_STARTUP_LOAD_DEFAULTS)
        return false;

    sdl_reset_config_to_resolution_defaults(screen_width, screen_height);
    sdl_ensure_default_pane_profiles_present(false);
    sdl_apply_screen_aspect_pane_default_profiles(screen_width, screen_height);
    sdl_apply_stored_pane_profile(config.min_terminal_mode);
    sdl_ensure_touch_pane_config_present();
    sdl_touch_pane_ensure_main_panel_confirm();
    sdl_config_save(config_file_path, &config, g_pane_profiles,
        SDL_PANE_PROFILE_COUNT);
    log_info("Startup recovery: reset SDL config to defaults at %s",
        config_file_path);
    return true;
}

#if SIL_SDL_DESKTOP_HANDHELD_BUILD
bool sdl_resolution_matches_pair(int width, int height, int native_w,
    int native_h)
{
    return ((width == native_w && height == native_h)
        || (width == native_h && height == native_w));
}

bool sdl_is_desktop_handheld_resolution(int width, int height)
{
    /* Native panel sizes for current Windows/Linux handhelds, plus common
     * handheld performance-mode targets. Check both orientations. */
    return sdl_resolution_matches_pair(width, height, 1280, 720)
        || sdl_resolution_matches_pair(width, height, 1280, 800)
        || sdl_resolution_matches_pair(width, height, 1920, 1080)
        || sdl_resolution_matches_pair(width, height, 1920, 1200)
        || sdl_resolution_matches_pair(width, height, 2560, 1600);
}

sdl_startup_device_class sdl_prompt_desktop_startup_input_device(
    int screen_width, int screen_height)
{
    enum {
        SDL_STARTUP_INPUT_MOUSE_KEYBOARD = 0,
        SDL_STARTUP_INPUT_CONTROLLER = 1,
    };
    bool handheld_resolution =
        sdl_is_desktop_handheld_resolution(screen_width, screen_height);
    SDL_MessageBoxButtonData buttons[2];
    SDL_MessageBoxData messagebox = {
        .flags = SDL_MESSAGEBOX_INFORMATION
            | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT,
        .window = g_state.window,
        .title = "Choose Input",
        .message = "A controller is connected.\n\nUse controller input or mouse + keyboard input?\n\nController is recommended for handhelds. Mouse + keyboard is recommended for everything else.",
        .numbuttons = 2,
        .buttons = buttons,
        .colorScheme = NULL,
    };
    int button_id = handheld_resolution
        ? SDL_STARTUP_INPUT_CONTROLLER
        : SDL_STARTUP_INPUT_MOUSE_KEYBOARD;

    buttons[0] = (SDL_MessageBoxButtonData){
        handheld_resolution ? SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT
                            : 0,
        SDL_STARTUP_INPUT_CONTROLLER,
        "Controller",
    };
    buttons[1] = (SDL_MessageBoxButtonData){
        handheld_resolution ? SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT
                            : (SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT
                                | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT),
        SDL_STARTUP_INPUT_MOUSE_KEYBOARD,
        "Mouse + Keyboard",
    };

    if (!SDL_ShowMessageBox(&messagebox, &button_id)) {
        log_warn("SDL_ShowMessageBox failed during input selection: %s",
            SDL_GetError());
        return handheld_resolution
            ? SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER
            : SDL_STARTUP_DEVICE_DESKTOP;
    }

    if (button_id == SDL_STARTUP_INPUT_CONTROLLER)
        return SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER;

    return SDL_STARTUP_DEVICE_DESKTOP;
}
#endif


