#include "angband.h"
#include "main.h"
#include "z-term.h"
#include "log/log.h"
#include "pane.h"
#include "sdl-config.h"
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

const char help_sdl[] = "SDL3";

enum {
    TILE_SIZE = 16,
    MAX_TERM_DATA = 8,
    MAX_PANE_CONFIGS = 8,
};

// SDL configuration (loaded from INI file)
struct sdl_config config;

// Configuration file path (needed for saving on exit)
static char config_file_path[1024];

// Default pane configuration
static const struct pane_config default_pane_config[] = {
    // On the right
    {.pane = PANE_INVENTORY, .where = PLACE_RIGHT},
    {.pane = PANE_WORN, .where = PLACE_RIGHT},
    {.pane = PANE_INFO, .where = PLACE_RIGHT, .rect.rows = 8},
    // In the bottom
    {.pane = PANE_ROLLS, .where = PLACE_BOTTOM, .rect.rows = 4},
    {.pane = PANE_LOG, .where = PLACE_BOTTOM},
};
const int default_pane_config_count = sizeof(default_pane_config) / sizeof(struct pane_config);

// Active pane configuration (may be loaded from INI)
struct pane_config pane_config[MAX_PANE_CONFIGS];
int pane_config_count = 0;

typedef struct sdl_state {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* tileset;
    SDL_Color palette[16];
    float system_scale;
    int tileset_cols;
    bool need_present;
    bool use_tiles;
} sdl_state;

typedef struct sdl_view {
    SDL_Rect rect;
    SDL_Texture* canvas;
    SDL_Texture* font_atlas;
    int ttf_font_size;
    int cell_w;
    int cell_h;
    int cols;
    int rows;
    int margin_x;
    int margin_y;
    term t;
    bool term_ready;
} sdl_view;

sdl_state g_state;
sdl_view g_views[MAX_TERM_DATA];

static sdl_view* sdl_view_from_term(term* t);
static void sdl_view_destroy(sdl_view* d);
static void resize(const SDL_Rect* screen);
static void sdl_handle_event(sdl_state* st, const SDL_Event* ev);
static void sdl_quit_hook(cptr str);
static errr callback_sdl_xtra(int n, int v);
static void draw_cursor(int x, int y, bool big);
static errr callback_sdl_curs(int x, int y);
static errr callback_sdl_bigcurs(int x, int y);
static errr callback_sdl_wipe(int x, int y, int n);
static errr callback_sdl_text(int x, int y, int n, byte a, cptr s);
static errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp,
                       const byte* tap, const char* tcp);
static void callback_sdl_nuke();
static void callback_sdl_init(term* t);
static errr sdl_view_link_term(sdl_view* d, int term_index);
static SDL_Texture* sdl_load_ttf_font(const char* font_path, int font_size, int* actual_font_size);
static void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles);
static void sdl_window_set_position(int x, int y);
static void sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin);

static sdl_view* sdl_view_from_term(term* t)
{
    return &g_views[(size_t)t->data];
}

/*
 * Synchronize the SDL palette from angband_color_table.
 * This allows color customizations from .prf files to work.
 */
static void sdl_sync_palette(void)
{
    for (int i = 0; i < 16; i++) {
        g_state.palette[i].r = angband_color_table[i][1];
        g_state.palette[i].g = angband_color_table[i][2];
        g_state.palette[i].b = angband_color_table[i][3];
        g_state.palette[i].a = 255;
    }
}

static void sdl_view_destroy(sdl_view* d)
{
    if (d->canvas) {
        SDL_DestroyTexture(d->canvas);
        d->canvas = NULL;
    }
    if (d->font_atlas) {
        SDL_DestroyTexture(d->font_atlas);
        d->font_atlas = NULL;
    }
}

static void resize(const SDL_Rect* screen)
{
    log_warn("resize enter");
    SDL_Rect panes[MAX_TERM_DATA] = {0};
    place_panes(pane_config, pane_config_count, panes, screen,
        g_state.system_scale * config.aux_view_font_size / 2,
        g_state.system_scale * config.aux_view_font_size,
        g_state.system_scale * config.margin);
    for (int i = 0; i < PANE_MAX; i++) {
        const SDL_Rect* r = &panes[i];
        log_debug("pane %d is at (%d, %d) size %dx%d", i, r->x, r->y, r->w, r->h);
    }

    // Check whether after splitting the window the main view is larger than
    // 80x25. If it isn't, remove panes along the corresponding axis (or axes).
    {
        int cell_w = config.main_view_scale * TILE_SIZE / 2;
        int cell_h = config.main_view_scale * TILE_SIZE;
        log_debug("Cell dimensions: %dx%d (scale=%d, TILE_SIZE=%d)", cell_w, cell_h, config.main_view_scale, TILE_SIZE);
        // panes are already in window coordinate space, no need to multiply by system_scale
        int cols = panes[PANE_MAIN].w / cell_w;
        int rows = panes[PANE_MAIN].h / cell_h;
        log_debug("Main view: %dx%d pixels = %dx%d cells (minimum required: 80x25)", 
                  panes[PANE_MAIN].w, panes[PANE_MAIN].h, cols, rows);
        if (cols < 80) {
            log_warn("main view too small, %d cols < 80 — removing right panes", cols);
            log_debug("Before removing right panes: main view width = %d", panes[PANE_MAIN].w);
            for (int i = 0; i < pane_config_count; i++) {
                if (pane_config[i].where == PLACE_RIGHT) {
                    log_debug("Removing pane %d (type=%d) from right", i, pane_config[i].pane);
                    panes[pane_config[i].pane].w = 0;
                }
            }
            panes[PANE_MAIN].w = screen->w;
            log_debug("After removing right panes: main view width = %d, cols = %d", 
                      panes[PANE_MAIN].w, panes[PANE_MAIN].w / cell_w);
        }
        if (rows < 25) {
            log_warn("main view too small, %d rows < 25 — removing bottom panes", rows);
            log_debug("Before removing bottom panes: main view height = %d", panes[PANE_MAIN].h);
            for (int i = 0; i < pane_config_count; i++) {
                if (pane_config[i].where == PLACE_BOTTOM) {
                    log_debug("Removing pane %d (type=%d) from bottom", i, pane_config[i].pane);
                    panes[pane_config[i].pane].w = 0;
                }
            }
            panes[PANE_MAIN].h = screen->h;
            log_debug("After removing bottom panes: main view height = %d, rows = %d",
                      panes[PANE_MAIN].h, panes[PANE_MAIN].h / cell_h);
        }
    }

    const char font_path[] = "lib/xtra/font/InputMono-Bold.ttf";

    for (int i = 1; i < MAX_TERM_DATA; i++) {
        // Always destroy the old pane to prevent its display in cases when we
        // have removed one of the bars or both of them due to the size
        // restrictions.
        sdl_view_destroy(&g_views[i]);
        if (panes[i].w) {
            sdl_view_create(&g_views[i], panes[i], font_path, config.aux_view_font_size, 0, config.margin);
            sdl_view_link_term(&g_views[i], i);
        }
    }

    sdl_view_destroy(&g_views[0]);
    sdl_view_create(&g_views[0], panes[PANE_MAIN], font_path, 0, config.main_view_scale, config.margin);
    sdl_view_link_term(&g_views[0], 0);

    Term_activate(&g_views[0].t);
    // Don't strictly need this as `sdl_view_create` already sets this flag.
    g_state.need_present = true;
}

static void sdl_handle_event(sdl_state* st, const SDL_Event* ev)
{
    if (ev->type == SDL_EVENT_QUIT) {
        Term_keypress(27); // ESC or define a quit signal
    } else if (ev->type == SDL_EVENT_KEY_DOWN) {
        int key = ev->key.key;
        // Ignore bare modifiers.
        if (key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
            key == SDLK_LALT || key == SDLK_RALT ||
            key == SDLK_LCTRL || key == SDLK_RCTRL ||
            key == SDLK_LGUI || key == SDLK_RGUI)
        {
            return;
        }

        if (SDL_isprint(ev->key.key)) {
            /* If Ctrl+letter (no Alt/GUI), send the corresponding control char
             * (so Ctrl-A -> ASCII 1) to preserve traditional control bindings
             * like Ctrl-A for the debug menu. For other modifier combinations
             * or non-alpha printables, keep existing behavior. */
            bool ctrl = ev->key.mod & SDL_KMOD_CTRL;
            bool alt = ev->key.mod & SDL_KMOD_ALT;
            bool gui = ev->key.mod & SDL_KMOD_GUI;
            if (ctrl && !alt && !gui && SDL_isalpha(key)) {
                /* Map to control character */
                Term_keypress(KTRL(key));
            } else {
                if (ev->key.mod & SDL_KMOD_SHIFT) {
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
                    key = '8';
                    break;
                case SDLK_DOWN:
                    key = '2';
                    break;
                case SDLK_LEFT:
                    key = '4';
                    break;
                case SDLK_RIGHT:
                    key = '6';
                    break;
            }
            if (mod) {
                /* Begin the macro trigger */
                Term_keypress(31);
                /* Send the modifiers */
                if (ctrl || gui)
                    Term_keypress('C');
                if (shift)
                    Term_keypress('S');
                if (alt)
                    Term_keypress('A');
                /* Introduce the scan code */
                Term_keypress('x');
                /* Encode the hexidecimal scan code */
                Term_keypress(hexsym[key / 16]);
                Term_keypress(hexsym[key % 16]);
                /* End the macro trigger */
                Term_keypress(13);
                log_debug("send macro key=%d ^_%s%s%sx%x%x\r", key, (ctrl || gui) ? "C" : "",
                    shift ? "S" : "", alt ? "A" : "", key / 16, key % 16);
            } else {
                Term_keypress(key);
            }
        }
    } else if (ev->type == SDL_EVENT_WINDOW_RESIZED) {
        log_debug("window resized to %dx%d", ev->window.data1, ev->window.data2);
        SDL_Rect window = { 0 };
        SDL_GetWindowSizeInPixels(g_state.window, &window.w, &window.h);
        log_debug("new window size in pixels %dx%d", window.w, window.h);
        // SDL_Rect window = {.w = ev->window.data1, .h = ev->window.data2};
        resize(&window);
    } else if (ev->type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED ||
        ev->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
        ev->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {

        float scale = SDL_GetWindowDisplayScale(g_state.window);
        if (scale != g_state.system_scale) {
            log_info("new system scale is %g", scale);
            g_state.system_scale = scale;
            SDL_Rect window = { 0 };
            SDL_GetWindowSizeInPixels(g_state.window, &window.w, &window.h);
            log_debug("window size in pixels %dx%d", window.w, window.h);
            resize(&window);
        }
    }
}

static errr callback_sdl_xtra(int n, int v)
{
    sdl_view* d = sdl_view_from_term(Term);
    switch (n) {
    case TERM_XTRA_EVENT: {
        SDL_Event ev;
        if (v) {
            if (SDL_WaitEvent(&ev))
                sdl_handle_event(&g_state, &ev);
        } else {
            /* Non-blocking scan so animation loops (intro fades, etc.) keep running */
            bool handled = false;
            while (SDL_PollEvent(&ev)) {
                handled = true;
                sdl_handle_event(&g_state, &ev);
            }

            /* Avoid pegging a CPU core when we're repeatedly asked to poll */
            if (!handled)
                SDL_Delay(1);
        }
        return 0;
    }
    case TERM_XTRA_FLUSH:
        // Flush pending input events (drain queue)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                sdl_handle_event(&g_state, &ev);
        }
        return 0;
    case TERM_XTRA_CLEAR:
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
        g_state.need_present = true;
        return 0;
    case TERM_XTRA_FRESH:
        if (g_state.need_present) {
            SDL_SetRenderTarget(g_state.renderer, NULL);
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_state.renderer);
            // Render all view canvases to the window
            for (int i = 0; i < MAX_TERM_DATA; i++) {
                sdl_view* view = &g_views[i];
                if (!view->canvas)
                    continue;
                // rect and margin are already in window coordinates, no scaling needed
                SDL_RenderTexture(g_state.renderer, view->canvas, NULL, &(SDL_FRect){
                    .x = view->rect.x + view->margin_x,
                    .y = view->rect.y + view->margin_y,
                    .w = view->canvas->w,
                    .h = view->canvas->h,
                });
                SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 128);
                SDL_FRect frame = {
                    .x = view->rect.x,
                    .y = view->rect.y,
                    .w = view->rect.w,
                    .h = view->rect.h,
                };
                SDL_RenderRect(g_state.renderer, &frame);
            }
            SDL_RenderPresent(g_state.renderer);
            SDL_FlushRenderer(g_state.renderer);
            SDL_SetRenderTarget(g_state.renderer, d->canvas);
            g_state.need_present = false;
        }
        return 0;
    case TERM_XTRA_DELAY: {
        /* Break delay into chunks and process events to keep app responsive */
        Uint32 total_delay = (Uint32)v;
        Uint32 chunk = 20; /* Process events every 20ms */
        
        while (total_delay > 0) {
            Uint32 this_delay = (total_delay < chunk) ? total_delay : chunk;
            SDL_Delay(this_delay);
            total_delay -= this_delay;
            
            /* Process pending events to prevent "Not Responding" status */
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                sdl_handle_event(&g_state, &ev);
            }
        }
        return 0;
    }
    case TERM_XTRA_REACT:
        /* React to global setting changes (graphics mode, colors, etc.) */
        log_debug("TERM_XTRA_REACT received (tiles_mode=%d use_graphics=%d arg_graphics=%d)",
                  g_state.use_tiles, use_graphics, arg_graphics);
        /* Reload colors from angband_color_table (may have been changed by .prf files) */
        sdl_sync_palette();
        reset_visuals(true);
        return 0;
    default:
        return 0;
    }
}

static void draw_cursor(int x, int y, bool big)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d->canvas)
        return;
    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderDrawColor(g_state.renderer, 0, 255, 255, 255);
    SDL_RenderRect(g_state.renderer, &r);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
}

static errr callback_sdl_curs(int x, int y)
{
    draw_cursor(x, y, false);
    return 0;
}

static errr callback_sdl_bigcurs(int x, int y)
{
    draw_cursor(x, y, true);
    return 0;
}

static errr callback_sdl_wipe(int x, int y, int n)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d->canvas)
        return 0;
    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(g_state.renderer, &r);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
    return 0;
}

static errr callback_sdl_text(int x, int y, int n, byte a, cptr s)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas)
        return 0;
    SDL_SetRenderTarget(g_state.renderer, d->canvas);

    // Clear destination cell span so shorter/narrower glyphs don't leave leftovers
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_FRect bg = {
        (float)(x * d->cell_w),
        (float)(y * d->cell_h),
        (float)(n * d->cell_w),
        (float)(d->cell_h)
    };
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(g_state.renderer, &bg);

    SDL_Color col = g_state.palette[a % 16];
    SDL_SetTextureColorMod(d->font_atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        int glyph_width = d->cell_w;
        int glyph_height = d->cell_h;
        SDL_FRect src = {
            (ch & 15) * glyph_width,
            (ch >> 4) * glyph_height,
            glyph_width,
            glyph_height,
        };
        SDL_FRect dst = {
            (x + i) * d->cell_w,
            y * d->cell_h,
            d->cell_w,
            d->cell_h
        };
        if (use_graphics == GRAPHICS_PSEUDO && (ch == '#' || ch == '%')) {
            SDL_SetRenderDrawColor(g_state.renderer, col.r, col.g, col.b, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(g_state.renderer, &dst);
        }
        SDL_RenderTexture(g_state.renderer, d->font_atlas, &src, &dst);
    }

    g_state.need_present = true;
    return 0;
}

static errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp,
                       const byte* tap, const char* tcp)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas)
        return 0;
    log_trace("sdl3_pict stripe start: y=%d x=%d n=%d", y, x, n);

    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_SetRenderClipRect(g_state.renderer, &(SDL_Rect){
        x * d->cell_w,
        y * d->cell_h,
        n * d->cell_w * (use_bigtile + 1),
        d->cell_h,
    });

    SDL_FRect src = {
        .w = TILE_SIZE,
        .h = TILE_SIZE,
    };

    SDL_FRect dst = {
        x * d->cell_w,
        y * d->cell_h,
        d->cell_w * (use_bigtile + 1),
        d->cell_h,
    };

    for (int i = 0; i < n; ++i, dst.x += dst.w) {
        byte a = ap[i];
        char c = cp[i];

        bool glow = a & GRAPHICS_GLOW_MASK;
        bool alert = c & GRAPHICS_ALERT_MASK;

        /* Unconditionally clear the full (possibly 2-cell) destination area to avoid ghosting */
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(g_state.renderer, &dst);

        /* Draw terrain underlay ALWAYS */
        src.x = (tcp[i] & 0x3F) * TILE_SIZE;
        src.y = (tap[i] & 0x3F) * TILE_SIZE;
        SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);

        /* Overlays (glow / alert) */
        if (glow) {
            src.x = (0x7F & misc_to_char[ICON_GLOW]) * TILE_SIZE;
            src.y = (0x7F & misc_to_attr[ICON_GLOW]) * TILE_SIZE;
            SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
        }

        /* Draw base tile */
        src.x = (c & 0x3F) * TILE_SIZE;
        src.y = (a & 0x3F) * TILE_SIZE;
        SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);

        if (alert) {
            src.x = (0x7F & misc_to_char[ICON_ALERT]) * TILE_SIZE;
            src.y = (0x7F & misc_to_attr[ICON_ALERT]) * TILE_SIZE;
            SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
    return 0;
}

static void callback_sdl_nuke() {
    log_debug("sdl3_term_nuke");
    sdl_view* d = sdl_view_from_term(Term);
    if (!d)
        return;

    // if (d->font) {
    //     TTF_CloseFont(d->font);
    //     d->font = NULL;
    // }
    // for (int i = 0; i < 256; ++i) {
    //     if (d->glyph_cache[i]) {
    //         SDL_DestroyTexture(d->glyph_cache[i]);
    //         d->glyph_cache[i] = NULL;
    //     }
    // }
    if (d->font_atlas)
        SDL_DestroyTexture(d->font_atlas);
    // if (d->tileset)
    //     SDL_DestroyTexture(d->tileset);
    if (d->canvas)
        SDL_DestroyTexture(d->canvas);
    // if (d->renderer)
    //     SDL_DestroyRenderer(d->renderer);
    // if (d->window)
    //     SDL_DestroyWindow(d->window);
}

static void callback_sdl_init(term* t)
{
    (void)t;
}

static errr sdl_view_link_term(sdl_view* d, int term_index)
{
    term* t = &d->t;
    if (d->term_ready) {
        term* old = Term;
        Term_activate(t);
        Term_resize(d->cols, d->rows);
        Term_redraw();
        Term_activate(old);
        return 0;
    }
    term_init(t, d->cols, d->rows, 256);
    t->soft_cursor = true;
    t->higher_pict = g_state.use_tiles;
    t->never_frosh = true;
    t->init_hook = callback_sdl_init;
    t->nuke_hook = callback_sdl_nuke;
    t->xtra_hook = callback_sdl_xtra;
    t->curs_hook = callback_sdl_curs;
    t->bigcurs_hook = callback_sdl_bigcurs;
    t->wipe_hook = callback_sdl_wipe;
    t->text_hook = callback_sdl_text;
    if (g_state.use_tiles)
        t->pict_hook = callback_sdl_pict;
    size_t* view_index = (size_t*)&t->data;
    *view_index = term_index;
    angband_term[term_index] = t;
    d->term_ready = true;
    return 0;
}

// Loads TTF font with given size. Attempts to fit the font into a cell assming
// 1:2 aspect ratio. The font size is expected to take into account any scaling,
// either HiDPI or user. So on a HiDPI screen to use font size 12, this function
// would expect 24 given scaling factor of 2.0.
static SDL_Texture* sdl_load_ttf_font(const char* font_path, int font_size, int* actual_font_size)
{
    int cell_height = font_size;
    int cell_width = font_size / 2;
    int min_size = font_size / 2;
    TTF_Font* font = NULL;
    for (; font_size >= min_size; font_size--) {
        log_trace("trying TTF font size %d", font_size);
        if (font == NULL) {
            font = TTF_OpenFont(font_path, font_size);
            if (!font) {
                log_error("TTF_OpenFont failed: %s", SDL_GetError());
                quit("could not load TTF font");
            }
        }
        int measured_w = 0;
        TTF_MeasureString(font, "M", 1, 0, &measured_w, NULL);
        log_trace("font size %d, measured_w %d", font_size, measured_w);
        if (measured_w <= cell_width) {
            log_debug("chose TTF font size %d, em width %d", font_size, measured_w);
            break;
        }
        TTF_CloseFont(font);
        font = NULL;
    }
    if (!font) {
        log_error("could not find suitable font size");
        quit("could not find suitable font size");
    }
    // Build TTF font atlas.
    SDL_Texture* font_atlas = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 16 * cell_width, 16 * cell_height);
    if (!font_atlas) {
        log_error("SDL_CreateTexture failed: %s", SDL_GetError());
        quit("could not create TTF glyph cache");
    }
    SDL_SetRenderTarget(g_state.renderer, font_atlas);
    SDL_Color white = (SDL_Color){255, 255, 255, 255};
    SDL_FRect dst = {
        .w = cell_width,
        .h = cell_height,
    };
    for (Uint32 ch = 0; ch < 256; ch++) {
        SDL_Surface* gsurf = TTF_RenderGlyph_Blended(font, ch, white);
        if (!gsurf) {
            // Dumb method of comparing errors using string comparison.
            // Apparently SDL doesn't have error codes, only this.
            const char* error = SDL_GetError();
            if (!SDL_strcmp(error, "Text has zero width")) {
                continue;
            }
            log_error("could not render `%c` character: %s", ch, error);
            quit("could not render TTF character");
        }
        SDL_Texture* gtex = SDL_CreateTextureFromSurface(g_state.renderer, gsurf);
        SDL_DestroySurface(gsurf);
        if (!gtex) {
            log_error("prepare_glyph: could not create texture from surface: %s", SDL_GetError());
            quit("could not create SDL texture");
        }
        SDL_SetTextureBlendMode(gtex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(gtex, SDL_SCALEMODE_LINEAR);
        dst.x = cell_width * (ch % 16);
        dst.y = cell_height * (ch >> 4);
        SDL_RenderTexture(g_state.renderer, gtex, NULL, &dst);
    }
    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetTextureScaleMode(font_atlas, SDL_SCALEMODE_LINEAR);
    TTF_CloseFont(font);
    if (actual_font_size)
        *actual_font_size = font_size;
    return font_atlas;
}

static void sdl_window_set_position(int x, int y)
{
    if (g_state.window && x >= 0 && y >= 0) {
        SDL_SetWindowPosition(g_state.window, x, y);
        log_debug("Window position set to (%d, %d)", x, y);
    }
}

static void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles)
{
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    if (!SDL_CreateWindowAndRenderer("Sil-more SDL3", window_width, window_height,
            flags, &g_state.window, &g_state.renderer))
    {
        log_error("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        quit("could not create SDL window");
    }

    if (fullscreen)
        SDL_HideCursor();

    g_state.system_scale = SDL_GetWindowDisplayScale(g_state.window);
    log_debug("window scale is %g", g_state.system_scale);

    // Ensure predictable alpha blending (cursor/text)
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    g_state.use_tiles = use_tiles;
    if (g_state.use_tiles) {
        log_debug("preparing tileset");
        g_state.use_tiles = true;
        // d->tile_w = 2 * GLYPH_WIDTH;
        // d->tile_h = GLYPH_HEIGHT;
        SDL_Surface* ts = IMG_Load("lib/xtra/graf/16x16.png");
        if (ts) {
            log_debug("tileset loaded");
            g_state.tileset = SDL_CreateTextureFromSurface(g_state.renderer, ts);
            SDL_DestroySurface(ts);
            if (!g_state.tileset) {
                log_error("Failed to create tileset texture: %s", SDL_GetError());
                quit("could not create tileset texture");
            } else {
                SDL_SetTextureScaleMode(g_state.tileset, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
                g_state.tileset_cols = ts->w / TILE_SIZE;
            }
        } else {
            log_error("Failed to load tileset PNG: %s", SDL_GetError());
            quit("could not load tileset");
        }
    }
}

static void sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin)
{
    log_debug("view rect=(%d %d %d %d)", rect.x, rect.y, rect.w, rect.h);

    if (scale) {
        // Integer scaling mode.
        d->cell_w = scale * TILE_SIZE / 2;
        d->cell_h = scale * TILE_SIZE;
    } else if (font_size) {
        // Non-integer scaling mode.
        d->cell_h = g_state.system_scale * font_size;
        d->cell_w = d->cell_h / 2;
    } else {
        quit("sdl_view_create: font_size and scale cannot both be zero");
    }

    d->font_atlas = sdl_load_ttf_font(font_path, d->cell_h, NULL);
    SDL_SetTextureBlendMode(d->font_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(d->font_atlas, 255, 255, 255);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    d->rect = rect;
    d->cols = rect.w / d->cell_w;
    d->rows = rect.h / d->cell_h;
    d->margin_x = (rect.w - d->cols * d->cell_w) / 2;
    if (d->margin_x < margin)
        d->margin_x = margin;
    d->margin_y = (rect.h - d->rows * d->cell_h) / 2;
    if (d->margin_y < margin)
        d->margin_y = margin;
    log_debug("view cols=%d rows=%d cell=(%d, %d) margin=(%d, %d)",
        d->cols, d->rows, d->cell_w, d->cell_h,
        d->margin_x, d->margin_y);

    // Create a persistent offscreen canvas to render into.
    d->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_TARGET,
                                  d->cols * d->cell_w, d->rows * d->cell_h);
    if (d->canvas) {
        log_debug("view canvas %dx%d", d->canvas->w, d->canvas->h);
        SDL_SetTextureBlendMode(d->canvas, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(d->canvas, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
        g_state.need_present = true;
    } else {
        log_error("Create canvas failed: %s", SDL_GetError());
        quit("could not create canvas");
    }
}

// Quit hook to save window configuration on exit
static void sdl_quit_hook(cptr str)
{
    (void)str; // Unused parameter
    
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
        sdl_config_save(config_file_path, &config, pane_config, pane_config_count);
    }
}


errr init_sdl(int argc, char **argv)
{
    log_debug("init_sdl starting");
    
    // Initialize SDL first to get display information
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        quit("could not init SDL");
    }
    if (!TTF_Init()) {
        log_error("TTF_Init failed: %s", SDL_GetError());
        quit("could not init TTF");
    }
    
    // Get primary display information
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (!primary) {
        log_error("SDL_GetPrimaryDisplay failed: %s", SDL_GetError());
        quit("could not get primary display ID");
    }
    SDL_Rect screen;
    if (!SDL_GetDisplayBounds(primary, &screen)) {
        log_error("SDL_GetDisplayBounds failed: %s", SDL_GetError());
        quit("could not get primary display bounds");
    }
    
    log_info("primary display: %d %d %d %d", screen.x, screen.y, screen.w, screen.h);
    
    // Save config file path for later use on exit
    const char* config_file = "sil_sdl.json";
    my_strcpy(config_file_path, config_file, sizeof(config_file_path));
    
    // Register quit hook to save configuration on exit
    quit_aux = sdl_quit_hook;
    
    // Check if config file exists
    FILE* test_file = fopen(config_file, "rb");
    bool config_exists = (test_file != NULL);
    if (test_file) {
        fclose(test_file);
    }
    
    if (config_exists) {
        // Config file exists - use generic defaults first, then load from file
        log_debug("Config file exists, loading from: %s", config_file);
        sdl_config_set_defaults(&config);
        
        // Copy default pane configuration
        pane_config_count = default_pane_config_count;
        for (int i = 0; i < default_pane_config_count && i < MAX_PANE_CONFIGS; i++) {
            pane_config[i] = default_pane_config[i];
        }
        
        sdl_config_load(config_file, &config, pane_config, &pane_config_count, MAX_PANE_CONFIGS);
        log_debug("After loading JSON: scale=%d, font=%d, margin=%d, fullscreen=%d, tiles=%d",
                  config.main_view_scale, config.aux_view_font_size, config.margin,
                  config.fullscreen, config.tiles);
    } else {
        // Config file doesn't exist - use resolution-based defaults
        log_debug("Config file not found, using resolution-based defaults");
        sdl_config_set_defaults_for_resolution(&config, pane_config, &pane_config_count,
                                               MAX_PANE_CONFIGS, screen.w, screen.h);
        
        // If no resolution-specific config was found, use default pane config
        if (pane_config_count == 0) {
            pane_config_count = default_pane_config_count;
            for (int i = 0; i < default_pane_config_count && i < MAX_PANE_CONFIGS; i++) {
                pane_config[i] = default_pane_config[i];
            }
        }
        
        log_debug("After resolution defaults: scale=%d, font=%d, margin=%d, fullscreen=%d, tiles=%d",
                  config.main_view_scale, config.aux_view_font_size, config.margin,
                  config.fullscreen, config.tiles);
    }
    
    // Apply command-line overrides
    sdl_config_apply_cmdline(&config, argc, argv);
    log_debug("After command-line: scale=%d, font=%d, margin=%d, fullscreen=%d, tiles=%d",
              config.main_view_scale, config.aux_view_font_size, config.margin,
              config.fullscreen, config.tiles);
    
    // Validate configuration
    if (config.main_view_scale <= 0) {
        log_warn("Invalid main_view_scale %d, using 1", config.main_view_scale);
        config.main_view_scale = 1;
    }
    if (config.aux_view_font_size <= 0) {
        log_warn("Invalid aux_view_font_size %d, using 18", config.aux_view_font_size);
        config.aux_view_font_size = 18;
    }
    if (config.margin < 0) {
        log_warn("Invalid margin %d, using 0", config.margin);
        config.margin = 0;
    }
    
    log_info("SDL Configuration:");
    log_info("  Main view scale: %d", config.main_view_scale);
    log_info("  Aux view font size: %d", config.aux_view_font_size);
    log_info("  Margin: %d", config.margin);
    log_info("  Fullscreen: %s", config.fullscreen ? "true" : "false");
    log_info("  Tiles: %s", config.tiles ? "true" : "false");
    log_info("  Pane configurations: %d", pane_config_count);

    // Initialize palette from angband_color_table (supports .prf file customization)
    sdl_sync_palette();

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
    
    sdl_window_create(window_width, window_height, config.fullscreen, config.tiles);
    
    // Set window position for windowed mode
    if (!config.fullscreen && config.window_x >= 0 && config.window_y >= 0) {
        sdl_window_set_position(config.window_x, config.window_y);
    }

    ANGBAND_SYS = "sdl";
    if (config.tiles) {
        ANGBAND_GRAF = "new";
        arg_graphics = GRAPHICS_MICROCHASM;
        use_graphics = GRAPHICS_MICROCHASM;
        use_bigtile = true;
    } else {
        ANGBAND_GRAF = "old";
        arg_graphics = GRAPHICS_PSEUDO;
        use_graphics = GRAPHICS_PSEUDO;
    }

    SDL_Rect window = { 0 };
    SDL_GetWindowSizeInPixels(g_state.window, &window.w, &window.h);
    log_debug("window pixel size %dx%d", window.w, window.h);
    resize(&window);

    log_debug("init_sdl: SDL term opened (tiles_mode=%d higher_pict=%d always_pict=%d)",
            config.tiles, Term->higher_pict, Term->always_pict);
    
    return 0;
}

/*
 * Get SDL configuration info as formatted string
 * Called from cmd4.c for the pane settings menu
 */
void get_sdl_config_info(char* buf, size_t size)
{
    size_t offset = 0;
    
    // SDL settings
    offset += (size_t)strnfmt(buf + offset, size - offset, "=== SDL Settings ===\n");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Main View Scale: %d\n", config.main_view_scale);
    offset += (size_t)strnfmt(buf + offset, size - offset, "Aux View Font Size: %d\n", config.aux_view_font_size);
    offset += (size_t)strnfmt(buf + offset, size - offset, "Margin: %d\n", config.margin);
    offset += (size_t)strnfmt(buf + offset, size - offset, "Fullscreen: %s\n", config.fullscreen ? "Yes" : "No");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Tiles: %s\n\n", config.tiles ? "Yes" : "No");
    
    // Pane configurations
    offset += (size_t)strnfmt(buf + offset, size - offset, "=== Pane Configuration ===\n");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Total Panes: %d\n\n", pane_config_count);
    
    for (int i = 0; i < pane_config_count && i < MAX_PANE_CONFIGS; i++) {
        const struct pane_config* pc = &pane_config[i];
        const char* type_str = "UNKNOWN";
        const char* where_str = (pc->where == PLACE_BOTTOM) ? "BOTTOM" : "RIGHT";
        
        switch (pc->pane) {
            case PANE_MAIN: type_str = "MAIN"; break;
            case PANE_INVENTORY: type_str = "INVENTORY"; break;
            case PANE_WORN: type_str = "WORN"; break;
            case PANE_ROLLS: type_str = "ROLLS"; break;
            case PANE_INFO: type_str = "INFO"; break;
            case PANE_CHARACTER: type_str = "CHARACTER"; break;
            case PANE_LOG: type_str = "LOG"; break;
            case PANE_MONSTERS: type_str = "MONSTERS"; break;
            default: break;
        }
        
        offset += (size_t)strnfmt(buf + offset, size - offset, "Pane %d: %s\n", i + 1, type_str);
        offset += (size_t)strnfmt(buf + offset, size - offset, "  Placement: %s\n", where_str);
        if (pc->rect.rows > 0)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Rows: %d\n", pc->rect.rows);
        if (pc->rect.cols > 0)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Cols: %d\n", pc->rect.cols);
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
    sdl_config_save(config_file_path, &config, pane_config, pane_config_count);
    log_info("Pane configuration saved to: %s", config_file_path);
    return true;
}

/*
 * Accessor functions for SDL configuration values
 * These allow the options menu to read and modify settings
 */
int get_sdl_main_view_scale(void)
{
    return config.main_view_scale;
}

void set_sdl_main_view_scale(int value)
{
    if (value > 0 && value <= 10)
        config.main_view_scale = value;
}

int get_sdl_aux_view_font_size(void)
{
    return config.aux_view_font_size;
}

void set_sdl_aux_view_font_size(int value)
{
    if (value >= 8 && value <= 48)
        config.aux_view_font_size = value;
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
    config.fullscreen = value;
}

bool get_sdl_tiles(void)
{
    return config.tiles;
}

void set_sdl_tiles(bool value)
{
    config.tiles = value;
}

int get_pane_config_count(void)
{
    return pane_config_count;
}


