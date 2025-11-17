#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "fs/io_sdl.h"
#include "log/log.h"
#include "main.h"
#include "z-term.h"
#include "pane.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
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
char config_file_path[1024];

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
    
    // Custom fonts
    TTF_Font* story_font;      // Non-monospace font for story/narrative text
    int story_font_depth;      // Nesting counter for story font enable/disable
    bool story_font_grid;      // Whether queued story text should snap to cell grid
    
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
static void sdl_apply_story_font_state(bool active);
static void sdl_apply_story_grid_state(bool grid);
static void sdl_story_font_reset_state(void);
static void sdl_render_mono_text(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col);
static void sdl_render_story_text_free(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col);
static void sdl_render_story_text_grid(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col);
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
static void sdl_load_story_fonts(void);
static TTF_Font* sdl_load_font_with_fallback(const char* font_path, int font_size, const char* fallback_path);

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

    // Use configured monospace font or fall back to default
    const char* font_path = config.monospace_font[0] != '\0' 
        ? config.monospace_font 
        : "lib/xtra/font/InputMono-Bold.ttf";

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
                    key = '1';
                    break;
                case SDLK_KP_3:
                    key = '3';
                    break;
                case SDLK_KP_7:
                    key = '7';
                    break;
                case SDLK_KP_9:
                    key = '9';
                    break;
                case SDLK_KP_5:
                    key = '5';
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
    case TERM_XTRA_SOUND:
        sdl_sound_handle(v);
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

    // Check if any character in this chunk should use story font
    // First check the global chunk flag (for whole-line story rendering)
    bool chunk_story_font = (Term && Term->story_chunk_active && g_state.story_font);
    
    // Also check per-character story font flags
    if (!chunk_story_font && Term && Term->scr && g_state.story_font) {
        // Check if ANY character in this chunk (from x to x+n) has the story font flag
        // story is a byte** (2D array), so we need story[y] which gives us byte* for that row
        if (y >= 0 && y < Term->hgt && Term->scr->story && Term->scr->story[y]) {
            // Check all characters in the chunk, not just the first one
            for (int i = 0; i < n && (x + i) < Term->wid; i++) {
                if (Term->scr->story[y][x + i]) {
                    chunk_story_font = true;
                    log_debug("callback_sdl_text: Using story font based on per-char flag at y=%d x=%d (chunk starts at x=%d)", 
                              y, x + i, x);
                    break;
                }
            }
        }
    }
    
    bool story_mode = (chunk_story_font && g_state.story_font);

    if (!story_mode) {
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
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    } else {
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    /* Use extended color table to support shaded colors (indices 0-255) */
    SDL_Color col;
    col.r = angband_color_table[a][1];
    col.g = angband_color_table[a][2];
    col.b = angband_color_table[a][3];
    col.a = 255;
    
    // Special logging for line 0 (top description line in unified look)
    if (y == 0) {
        log_debug("callback_sdl_text ROW 0: x=%d n=%d chunk_story=%d text='%.*s'", 
                  x, n, chunk_story_font, n, s);
    }
    
    // Special logging for the shooting row (y=1 when 0-indexed, or the second row)
    if (y == 1 || y == 2) {
        log_debug("callback_sdl_text ROW %d: chunk_story=%d chunk_active=%d",
                  y, chunk_story_font,
                  (Term && Term->story_chunk_active) ? 1 : 0);
    }
    
    log_trace("callback_sdl_text: chunk_story_font=%s term=%p chunk_flag=%s depth=%d font=%p",
              chunk_story_font ? "true" : "false",
              (void*)Term,
              (Term && Term->story_chunk_active) ? "true" : "false",
              g_state.story_font_depth,
              (void*)g_state.story_font);

    byte* story_row = NULL;
    char* row_chars = NULL;
    byte* row_attr = NULL;
    if (Term && Term->scr && y >= 0 && y < Term->hgt) {
        if (Term->scr->story)
            story_row = Term->scr->story[y];
        if (Term->scr->c)
            row_chars = Term->scr->c[y];
        if (Term->scr->a)
            row_attr = Term->scr->a[y];
    }

    if (story_mode) {
        if (story_row) {
            int offset = 0;
            while (offset < n && (x + offset) < Term->wid) {
                int term_col = x + offset;
                byte flags = story_row[term_col];
                bool use_story = (flags & STORY_FLAG_USE) != 0;
                bool grid_align = (flags & STORY_FLAG_CELL_ALIGN) != 0;

                int chunk_remaining = n - offset;
                int chunk_run = 1;
                while ((chunk_run < chunk_remaining) && (term_col + chunk_run) < Term->wid) {
                    byte next_flags = story_row[term_col + chunk_run];
                    bool next_story = (next_flags & STORY_FLAG_USE) != 0;
                    bool next_grid = (next_flags & STORY_FLAG_CELL_ALIGN) != 0;
                    if (next_story != use_story)
                        break;
                    if (next_grid != grid_align)
                        break;
                    if (row_attr && row_attr[term_col + chunk_run] != a)
                        break;
                    chunk_run++;
                }

                bool can_extend_story = use_story && row_chars;
                int render_col = term_col;
                int render_end = term_col + chunk_run;

                if (can_extend_story) {
                    while (render_col > 0) {
                        byte prev_flags = story_row[render_col - 1];
                        bool prev_story = (prev_flags & STORY_FLAG_USE) != 0;
                        bool prev_grid = (prev_flags & STORY_FLAG_CELL_ALIGN) != 0;
                        if (!prev_story || prev_grid != grid_align)
                            break;
                        if (row_attr && row_attr[render_col - 1] != a)
                            break;
                        render_col--;
                    }
                    while (render_end < Term->wid) {
                        byte next_flags = story_row[render_end];
                        bool next_story = (next_flags & STORY_FLAG_USE) != 0;
                        bool next_grid = (next_flags & STORY_FLAG_CELL_ALIGN) != 0;
                        if (!next_story || next_grid != grid_align)
                            break;
                        if (row_attr && row_attr[render_end] != a)
                            break;
                        render_end++;
                    }
                }

                int render_run = render_end - render_col;
                const char* render_text = (can_extend_story && row_chars) ? (row_chars + render_col) : (s + offset);

                SDL_FRect clear_rect = {
                    (float)(render_col * d->cell_w),
                    (float)(y * d->cell_h),
                    (float)(render_run * d->cell_w),
                    (float)d->cell_h
                };
                SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
                SDL_RenderFillRect(g_state.renderer, &clear_rect);

                if (use_story) {
                    if (grid_align)
                        sdl_render_story_text_grid(d, render_col, y, render_run, render_text, col);
                    else
                        sdl_render_story_text_free(d, render_col, y, render_run, render_text, col);
                } else {
                    sdl_render_mono_text(d, render_col, y, render_run, render_text, col);
                }

                offset += chunk_run;
            }
        } else {
            SDL_FRect clear_rect = {
                (float)(x * d->cell_w),
                (float)(y * d->cell_h),
                (float)(n * d->cell_w),
                (float)d->cell_h
            };
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(g_state.renderer, &clear_rect);
            sdl_render_story_text_free(d, x, y, n, s, col);
        }
    } else {
        if (y == 1 || y == 2) {
            log_debug("callback_sdl_text: USING MONO FONT for row %d: '%.30s'", y, s);
        }
        sdl_render_mono_text(d, x, y, n, s, col);
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

// Helper to apply font rendering settings to a TTF_Font
static void sdl_apply_font_settings(TTF_Font* font, bool is_story_font)
{
    // Select settings based on font type
    bool bold = is_story_font ? config.story_bold : config.mono_bold;
    bool italic = is_story_font ? config.story_italic : config.mono_italic;
    bool underline = is_story_font ? config.story_underline : config.mono_underline;
    bool strikethrough = is_story_font ? config.story_strikethrough : config.mono_strikethrough;
    int hinting = is_story_font ? config.story_hinting : config.mono_hinting;
    bool kerning = is_story_font ? config.story_kerning : config.mono_kerning;
    int outline = is_story_font ? config.story_outline : config.mono_outline;
    
    // Apply font style settings
    int style = TTF_STYLE_NORMAL;
    if (bold) style |= TTF_STYLE_BOLD;
    if (italic) style |= TTF_STYLE_ITALIC;
    if (underline) style |= TTF_STYLE_UNDERLINE;
    if (strikethrough) style |= TTF_STYLE_STRIKETHROUGH;
    if (style != TTF_STYLE_NORMAL) {
        TTF_SetFontStyle(font, style);
        log_debug("Applied %s font style: %d (bold=%d, italic=%d, underline=%d, strikethrough=%d)",
                 is_story_font ? "story" : "mono", style, bold, italic, underline, strikethrough);
    }
    
    // Apply hinting
    TTF_SetFontHinting(font, hinting);
    log_debug("Applied %s font hinting: %d", is_story_font ? "story" : "mono", hinting);
    
    // Apply kerning
    TTF_SetFontKerning(font, kerning);
    
    // Apply outline
    if (outline > 0) {
        TTF_SetFontOutline(font, outline);
        log_debug("Applied %s font outline: %d", is_story_font ? "story" : "mono", outline);
    }
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
            
            // Apply monospace font settings
            sdl_apply_font_settings(font, false);
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

/*
 * Load a TTF font with fallback to default if not found
 */
static TTF_Font* sdl_load_font_with_fallback(const char* font_path, int font_size, const char* fallback_path)
{
    TTF_Font* font = NULL;
    
    // Try to load the specified font if provided
    if (font_path && font_path[0] != '\0') {
        font = TTF_OpenFont(font_path, font_size);
        if (font) {
            log_debug("Loaded custom font: %s at size %d", font_path, font_size);
            // Apply story font settings
            sdl_apply_font_settings(font, true);
            return font;
        } else {
            log_warn("Failed to load custom font '%s': %s", font_path, SDL_GetError());
        }
    }
    
    // Fall back to default font
    font = TTF_OpenFont(fallback_path, font_size);
    if (font) {
        log_debug("Using fallback font: %s at size %d", fallback_path, font_size);
        // Apply story font settings to fallback too
        sdl_apply_font_settings(font, true);
    } else {
        log_error("Failed to load fallback font '%s': %s", fallback_path, SDL_GetError());
    }
    
    return font;
}

/*
 * Load story font from configuration
 * If no story font is set in the SDL settings (or the SDL JSON is missing),
 * fall back to MarcellusSC-Regular.ttf located in lib/xtra/font.
 */
static void sdl_load_story_fonts(void)
{
    const char* default_font = "lib/xtra/font/MarcellusSC-Regular.ttf";
    /* Use the auxiliary view font size scaled by the system scale so the
     * story font matches the monospace/aux view appearance. Fall back to
     * 32 if aux_view_font_size is invalid. */
    int story_font_size = (config.aux_view_font_size > 0)
                           ? g_state.system_scale * config.aux_view_font_size
                           : 32;
    
    log_info("Loading story font...");
    log_debug("Story font config: '%s'", config.story_font[0] != '\0' ? config.story_font : "(not set)");
    
    // Load story font (or fallback to default)
    g_state.story_font = sdl_load_font_with_fallback(
        config.story_font[0] != '\0' ? config.story_font : NULL,
        story_font_size,
        default_font
    );
    
    // Initialize flag to false
    g_state.story_font_depth = 0;
    if (Term) Term->story_font_active = false;
    
    log_info("Story font loaded successfully");
}

/*
 * Enable story font mode - subsequent text will use custom font
 */
void sdl_story_font_enable(void)
{
    g_state.story_font_depth++;
    if (g_state.story_font_depth == 1)
        sdl_apply_story_font_state(true);
    log_debug("Story font ENABLED (depth=%d)", g_state.story_font_depth);
}

/*
 * Disable story font mode - subsequent text will use monospace font
 */
void sdl_story_font_disable(void)
{
    if (g_state.story_font_depth > 0)
        g_state.story_font_depth--;
    bool active = (g_state.story_font_depth > 0);
    sdl_apply_story_font_state(active);
    if (!active)
        sdl_story_font_set_grid(false);
    log_debug("Story font DISABLED (depth=%d)", g_state.story_font_depth);
}

/*
 * Check if story font is currently enabled
 */
bool sdl_is_story_font_enabled(void)
{
    return (Term && Term->story_font_active);
}

void sdl_story_font_set_grid(bool grid)
{
    if (g_state.story_font_grid == grid)
        return;
    g_state.story_font_grid = grid;
    log_trace("Story font grid %s", grid ? "ENABLED" : "DISABLED");
    sdl_apply_story_grid_state(grid);
}

bool sdl_is_story_font_grid(void)
{
    return (Term && Term->story_font_grid);
}

void sdl_story_font_reset(void)
{
    sdl_story_font_reset_state();
}

/*
 * Get the pixel width of text when rendered with the story font.
 * Returns 0 if story font is not available.
 * IMPORTANT: This returns the width AFTER scaling to match cell height.
 */
int sdl_story_font_text_width(cptr text, int len)
{
    if (!g_state.story_font || !text) {
        return 0;
    }
    
    /* Measure the text width using SDL_ttf (unscaled) */
    int w = 0;
    TTF_MeasureString(g_state.story_font, text, len, 0, &w, NULL);
    
    /* Apply the same scaling that's used when rendering */
    if (g_views[0].term_ready && g_state.story_font) {
        /* Get font metrics to determine rendered height */
        int font_h = TTF_GetFontHeight(g_state.story_font);
        if (font_h > 0) {
            /* Calculate scaling factor (same as in callback_sdl_text) */
            float cell_h_f = (float)g_views[0].cell_h;
            float surf_h_f = (float)font_h;
            float scale = cell_h_f / surf_h_f;
            
            /* Apply scaling to width */
            w = (int)((float)w * scale);
        }
    }
    
    return w;
}

/*
 * Get the cell width in pixels for the main terminal view.
 * This is used to convert terminal columns to pixel width.
 */
int sdl_get_cell_width(void)
{
    if (g_views[0].term_ready) {
        return g_views[0].cell_w;
    }
    return 8; /* fallback */
}

// Quit hook to save window configuration on exit
static void sdl_quit_hook(cptr str)
{
    (void)str; // Unused parameter
    
    // Shut down audio before tearing down SDL
    sdl_sound_shutdown();
    
    // Clean up story font
    if (g_state.story_font) {
        TTF_CloseFont(g_state.story_font);
        g_state.story_font = NULL;
    }
    
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
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
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
    // Physical resolution = logical × pixel_density
    float pixel_density = desktop_mode->pixel_density;
    
    // Calculate physical pixel dimensions for resolution profile matching
    // On macOS Retina: 1440×900 logical × 2.0 density = 2560×1600 physical
    // On Windows/Linux (no scaling): 1920×1080 logical × 1.0 density = 1920×1080 physical
    int screen_pixels_w = (int)(desktop_mode->w * pixel_density + 0.5f);
    int screen_pixels_h = (int)(desktop_mode->h * pixel_density + 0.5f);
    
    log_info("primary display desktop mode: %dx%d @%.2fHz, pixel_density=%.2f",
             desktop_mode->w, desktop_mode->h, desktop_mode->refresh_rate, pixel_density);
    log_info("primary display physical resolution for defaults: %dx%d",
             screen_pixels_w, screen_pixels_h);
    
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

    if (config_exists) {
        // Config file exists - use generic defaults first, then load from file
        log_debug("Config file exists, loading from: %s", config_file_path);
        sdl_config_set_defaults(&config);
        
        // Copy default pane configuration
        pane_config_count = default_pane_config_count;
        for (int i = 0; i < default_pane_config_count && i < MAX_PANE_CONFIGS; i++) {
            pane_config[i] = default_pane_config[i];
        }
        
        sdl_config_load(config_file_path, &config, pane_config, &pane_config_count, MAX_PANE_CONFIGS);
        
        // Apply sound setting to global variable
        use_sound = config.sound_enabled;
        
        log_debug("After loading JSON: scale=%d, font=%d, margin=%d, fullscreen=%d, tiles=%d, sound=%d",
                  config.main_view_scale, config.aux_view_font_size, config.margin,
                  config.fullscreen, config.tiles, config.sound_enabled);
    } else {
        // Config file doesn't exist - use resolution-based defaults
        log_debug("Config file not found, using resolution-based defaults");
        sdl_config_set_defaults_for_resolution(&config, pane_config, &pane_config_count,
                                               MAX_PANE_CONFIGS, screen_pixels_w, screen_pixels_h);
        
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
    
    sdl_window_create(window_width, window_height, config.fullscreen, config.tiles);
    
    // Set window position for windowed mode
    if (!config.fullscreen && config.window_x >= 0 && config.window_y >= 0) {
        sdl_window_set_position(config.window_x, config.window_y);
    }
    
    // Load story and banner fonts
    sdl_load_story_fonts();

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


static void sdl_apply_story_font_state(bool active)
{
    log_trace("Story font state apply: active=%s depth=%d term=%p",
              active ? "true" : "false", g_state.story_font_depth, (void*)Term);
    for (int i = 0; i < MAX_TERM_DATA; i++)
    {
        if (g_views[i].term_ready)
        {
            g_views[i].t.story_font_active = active;
        }
    }
    if (Term)
        Term->story_font_active = active;
}

static void sdl_apply_story_grid_state(bool grid)
{
    log_trace("Story grid state apply: grid=%s term=%p",
              grid ? "true" : "false", (void*)Term);
    for (int i = 0; i < MAX_TERM_DATA; i++)
    {
        if (g_views[i].term_ready)
        {
            g_views[i].t.story_font_grid = grid;
        }
    }
    if (Term)
        Term->story_font_grid = grid;
}

static void sdl_story_font_reset_state(void)
{
    g_state.story_font_depth = 0;
    sdl_apply_story_font_state(false);
    g_state.story_font_grid = false;
    sdl_apply_story_grid_state(false);
    if (Term)
        Term->story_chunk_active = false;
    log_trace("Story font state hard reset");
}

static void sdl_render_mono_text(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col)
{
    if (!d || !d->font_atlas || n <= 0)
        return;

    SDL_SetTextureColorMod(d->font_atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        SDL_FRect src = {
            (ch & 15) * d->cell_w,
            (ch >> 4) * d->cell_h,
            d->cell_w,
            d->cell_h,
        };
        SDL_FRect dst = {
            (x + i) * d->cell_w,
            y * d->cell_h,
            d->cell_w,
            d->cell_h
        };
        if (use_graphics == GRAPHICS_PSEUDO && solid_walls && (ch == '#' || ch == '%')) {
            SDL_SetRenderDrawColor(g_state.renderer, col.r, col.g, col.b, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(g_state.renderer, &dst);
        }
        SDL_RenderTexture(g_state.renderer, d->font_atlas, &src, &dst);
    }
}

static void sdl_render_story_text_free(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col)
{
    if (!d || !g_state.story_font || n <= 0)
        return;

    char text_buf[256];
    int len = (n < 255) ? n : 255;
    memcpy(text_buf, s, len);
    text_buf[len] = '\0';

    SDL_Surface* text_surface = TTF_RenderText_Blended(g_state.story_font, text_buf, 0, col);
    if (!text_surface)
        return;

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(g_state.renderer, text_surface);
    if (text_texture) {
        float cell_h_f = (float)d->cell_h;
        float surf_h_f = (float)text_surface->h;
        float scale = (surf_h_f > 0.0f) ? (cell_h_f / surf_h_f) : 1.0f;

        SDL_FRect dst = {
            (float)(x * d->cell_w),
            (float)(y * d->cell_h),
            (float)(text_surface->w) * scale,
            cell_h_f
        };

        float max_w = (float)(n * d->cell_w);
        if (dst.w > max_w) dst.w = max_w;

        SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, text_texture, NULL, &dst);
        SDL_DestroyTexture(text_texture);
    }

    SDL_DestroySurface(text_surface);
}

static void sdl_render_story_text_grid(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col)
{
    if (!d || !g_state.story_font || n <= 0)
        return;

    float cell_w_f = (float)d->cell_w;
    float cell_h_f = (float)d->cell_h;

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (!ch || ch == ' ')
            continue;

        char glyph_text[2] = { (char)ch, '\0' };
        SDL_Surface* glyph_surface = TTF_RenderText_Blended(g_state.story_font, glyph_text, 0, col);
        if (!glyph_surface)
            continue;

        SDL_Texture* glyph_texture = SDL_CreateTextureFromSurface(g_state.renderer, glyph_surface);
        if (glyph_texture) {
            float surf_w = (float)glyph_surface->w;
            float surf_h = (float)glyph_surface->h;
            float scale = (surf_h > 0.0f) ? (cell_h_f / surf_h) : 1.0f;
            float scaled_w = surf_w * scale;
            float dst_w = scaled_w;
            float offset_x = 0.0f;

            if (scaled_w > cell_w_f) {
                dst_w = cell_w_f;
                scale = (surf_w > 0.0f) ? (dst_w / surf_w) : 1.0f;
            } else {
                offset_x = (cell_w_f - scaled_w) * 0.5f;
            }

            SDL_FRect dst = {
                (float)((x + i) * d->cell_w) + offset_x,
                (float)(y * d->cell_h),
                dst_w,
                cell_h_f
            };

            SDL_SetTextureBlendMode(glyph_texture, SDL_BLENDMODE_BLEND);
            SDL_RenderTexture(g_state.renderer, glyph_texture, NULL, &dst);
            SDL_DestroyTexture(glyph_texture);
        }

        SDL_DestroySurface(glyph_surface);
    }
}


