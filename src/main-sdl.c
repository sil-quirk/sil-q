#include "angband.h"
#include "main.h"
#include "z-term.h"
#include "log/log.h"
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

const char help_sdl[] = "SDL3";

typedef enum {
    FONTMODE_TTF,
    FONTMODE_BITMAP
} font_mode_t;

enum {
    GLYPH_WIDTH = 8,
    GLYPH_HEIGHT = 16,
    TILE_WIDTH = 16,
    TILE_HEIGHT = 16,
    MAX_TERM_DATA = 8,
};

static font_mode_t g_font_mode = FONTMODE_BITMAP;
static int g_scale = 1;
static bool g_tiles_mode = true;
static bool g_fullscreen = true;
static SDL_Color g_palette[16];

typedef struct {
    SDL_Window* window; // shared
    SDL_Renderer* renderer; // shared?
    SDL_Texture* canvas; // individual
    SDL_Texture* font_atlas; // individual
    SDL_Texture* tileset; // shared
    float dpi_scale; // shared
    // Need to store these because these can vary between TTF and bitmap.
    int glyph_w;
    int glyph_h;
    int tile_w; // individual
    int tile_h;
    int tileset_cols;
    int ttf_font_size; // individual
    int cell_w; // individual
    int cell_h;
    int cols; // individual
    int rows;
    int margin_x; // individual, only applicable to the main window
    int margin_y;
    bool need_present; // shared
    bool use_tiles; // shared
    term t; // individual
} sdl_window;

typedef struct sdl_state {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* tileset;
    int tileset_cols;
    bool need_present;
    bool use_tiles;
} sdl_state;

typedef struct sdl_view {
    SDL_Rect rect;
    SDL_Texture* canvas;
    SDL_Texture* font_atlas;
    int tile_w;
    int tile_h;
    int ttf_font_size;
    int cell_w;
    int cell_h;
    int cols;
    int rows;
    int margin_x;
    int margin_y;
    term t;
} sdl_view;

sdl_state g_state;
sdl_view g_views[MAX_TERM_DATA];
sdl_window windows[MAX_TERM_DATA];

static void sdl_window_load_ttf_font(sdl_window* d, int window_width);

static sdl_window* sdl_window_from_term(term* t)
{
    return (sdl_window*)t->data;
}

static void sdl_window_handle_event(sdl_window* d, const SDL_Event* ev)
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
        int cols = d->dpi_scale * ev->window.data1 / d->cell_w;
        int rows = d->dpi_scale * ev->window.data2 / d->cell_h;
        if (cols != d->cols || rows != d->rows) {
            bool recalc = false;
            if ((d->cols >= 80 && cols < 80) || (d->rows >= 25 && rows < 25)) {
                log_debug("window too small, downscale");
                g_scale = 1;
                recalc = true;
            } else if (g_scale == 1 && cols >= 2 * 80 && rows >= 2 * 25) {
                log_debug("window big enough, upscale");
                g_scale = 2;
                recalc = true;
            }
            if (recalc) {
                d->cell_h = d->dpi_scale * g_scale * GLYPH_HEIGHT;
                d->cell_w = d->dpi_scale * g_scale * GLYPH_WIDTH;
                sdl_window_load_ttf_font(d, d->cols * d->cell_w);
            }
            cols = d->dpi_scale * ev->window.data1 / d->cell_w;
            rows = d->dpi_scale * ev->window.data2 / d->cell_h;

            log_debug("resized from %dx%d to %dx%d", d->rows, d->cols, rows, cols);
            d->cols = cols;
            d->rows = rows;
            Term_activate(&d->t);
            Term_resize(cols, rows);
            // Term_xtra(TERM_XTRA_FRESH, 0);

            // Recreate canvas for new logical size
            if (d->canvas)
                SDL_DestroyTexture(d->canvas);
            d->canvas = SDL_CreateTexture(d->renderer,
                SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                d->cols * d->cell_w, d->rows * d->cell_h);
            if (d->canvas) {
                SDL_SetTextureBlendMode(d->canvas, SDL_BLENDMODE_NONE);
                SDL_SetTextureScaleMode(d->canvas, SDL_SCALEMODE_NEAREST);
                SDL_SetRenderTarget(d->renderer, d->canvas);
                SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
                SDL_RenderClear(d->renderer);
            } else {
                log_error("Failed to recreate canvas: %s", SDL_GetError());
            }
            d->need_present = true;
        }
    }
}

static errr callback_sdl_xtra(int n, int v)
{
    sdl_window* d = sdl_window_from_term(Term);
    switch (n) {
    case TERM_XTRA_EVENT: {
        int wait_ms = v;
        SDL_Event ev;

        if (wait_ms > 0) {
            // Wait for an event with timeout
            if (SDL_WaitEventTimeout(&ev, wait_ms))
                sdl_window_handle_event(d, &ev);
        } else {
            // Poll for events without waiting
            if (SDL_PollEvent(&ev))
                sdl_window_handle_event(d, &ev);
        }
        return 0;
    }
    case TERM_XTRA_FLUSH:
        // Flush pending input events (drain queue)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                sdl_window_handle_event(d, &ev);
        }
        return 0;
    case TERM_XTRA_CLEAR:
        SDL_SetRenderTarget(d->renderer, d->canvas);
        SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
        SDL_RenderClear(d->renderer);
        d->need_present = true;
        return 0;
    case TERM_XTRA_FRESH:
        if (d->need_present) {
            SDL_SetRenderTarget(d->renderer, NULL);
            SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
            SDL_RenderClear(d->renderer);
            SDL_RenderTexture(d->renderer, d->canvas, NULL, &(SDL_FRect){
                .x = d->margin_x,
                .y = d->margin_y,
                .w = d->canvas->w,
                .h = d->canvas->h,
            });
            SDL_RenderPresent(d->renderer);
            SDL_FlushRenderer(d->renderer);
            d->need_present = false;
            SDL_SetRenderTarget(d->renderer, d->canvas);
        }
        return 0;
    case TERM_XTRA_DELAY:
        SDL_Delay((Uint32)v);
        return 0;
    case TERM_XTRA_REACT:
        /* React to global setting changes (graphics mode, colors, etc.) */
        log_debug("TERM_XTRA_REACT received (tiles_mode=%d use_graphics=%d arg_graphics=%d)",
                  g_tiles_mode, use_graphics, arg_graphics);
        reset_visuals(true);
        return 0;
    default:
        return 0;
    }
}

static void draw_cursor(int x, int y, bool big)
{
    sdl_window* d = sdl_window_from_term(Term);
    SDL_SetRenderTarget(d->renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderClipRect(d->renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderDrawColor(d->renderer, 0, 255, 255, 127);
    SDL_RenderRect(d->renderer, &r);
    SDL_SetRenderClipRect(d->renderer, NULL);
    d->need_present = true;
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
    sdl_window* d = sdl_window_from_term(Term);
    SDL_SetRenderTarget(d->renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderClipRect(d->renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(d->renderer, &r);
    SDL_SetRenderClipRect(d->renderer, NULL);
    d->need_present = true;
    return 0;
}

static errr callback_sdl_text(int x, int y, int n, byte a, cptr s)
{
    sdl_window* d = sdl_window_from_term(Term);
    if (!d)
        return 0;
    SDL_SetRenderTarget(d->renderer, d->canvas);

    // Clear destination cell span so shorter/narrower glyphs don't leave leftovers
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderClipRect(d->renderer, &clip);
    SDL_FRect bg = {
        (float)(x * d->cell_w),
        (float)(y * d->cell_h),
        (float)(n * d->cell_w),
        (float)(d->cell_h)
    };
    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(d->renderer, &bg);

    SDL_Color col = g_palette[a % 16];
    SDL_SetTextureColorMod(d->font_atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        int glyph_width = g_font_mode == FONTMODE_TTF ? d->cell_w : GLYPH_WIDTH;
        int glyph_height = g_font_mode == FONTMODE_TTF ? d->cell_h : GLYPH_HEIGHT;
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
            SDL_SetRenderDrawColor(d->renderer, col.r, col.g, col.b, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(d->renderer, &dst);
        }
        SDL_RenderTexture(d->renderer, d->font_atlas, &src, &dst);
        // if (g_font_mode == FONTMODE_TTF) {
        //     int dy = (d->cell_h - glyph->h);
        //     dy /= 2;
        //     dst.y += dy;
        // }
    }

    d->need_present = true;
    return 0;
}

static errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp,
                       const byte* tap, const char* tcp)
{
    sdl_window* d = sdl_window_from_term(Term);
    if (!d)
        return 0;
    log_trace("sdl3_pict stripe start: y=%d x=%d n=%d", y, x, n);

    SDL_SetRenderClipRect(d->renderer, &(SDL_Rect){
        x * d->cell_w,
        y * d->cell_h,
        n * d->cell_w * (use_bigtile + 1),
        d->cell_h,
    });

    SDL_SetRenderTarget(d->renderer, d->canvas);

    SDL_FRect src = {
        .w = d->tile_w,
        .h = d->tile_h,
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

        int row = a & 0x3F;
        int col = c & 0x3F;

        bool glow = a & GRAPHICS_GLOW_MASK;
        bool alert = c & GRAPHICS_ALERT_MASK;

        /* Unconditionally clear the full (possibly 2-cell) destination area to avoid ghosting */
        SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(d->renderer, &dst);

        /* Draw terrain underlay ALWAYS */
        src.x = (tcp[i] & 0x3F) * d->tile_w;
        src.y = (tap[i] & 0x3F) * d->tile_h;
        SDL_RenderTexture(d->renderer, d->tileset, &src, &dst);

        /* Draw base tile */
        src.x = col * d->tile_w;
        src.y = row * d->tile_h;
        SDL_RenderTexture(d->renderer, d->tileset, &src, &dst);

        /* Overlays (glow / alert) */
        if (glow) {
            src.x = (0x7F & misc_to_char[ICON_GLOW]) * d->tile_w;
            src.y = (0x7F & misc_to_attr[ICON_GLOW]) * d->tile_h;
            SDL_RenderTexture(d->renderer, d->tileset, &src, &dst);
        }
        if (alert) {
            src.x = (0x7F & misc_to_char[ICON_ALERT]) * d->tile_w;
            src.y = (0x7F & misc_to_attr[ICON_ALERT]) * d->tile_h;
            SDL_RenderTexture(d->renderer, d->tileset, &src, &dst);
        }
    }

    SDL_SetRenderClipRect(d->renderer, NULL);
    d->need_present = true;
    return 0;
}

static void callback_sdl_nuke() {
    log_debug("sdl3_term_nuke");
    sdl_window* d = sdl_window_from_term(Term);
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
    if (d->tileset)
        SDL_DestroyTexture(d->tileset);
    if (d->canvas)
        SDL_DestroyTexture(d->canvas);
    if (d->renderer)
        SDL_DestroyRenderer(d->renderer);
    if (d->window)
        SDL_DestroyWindow(d->window);
}

static void callback_sdl_init(term* t)
{
    (void)t;
}

static errr sdl_window_link_term(sdl_window* d, int term_index)
{
    term* t = &d->t;
    term_init(t, d->cols, d->rows, 256);
    t->soft_cursor = true;
    t->higher_pict = g_tiles_mode;
    t->never_frosh = true;
    t->init_hook = callback_sdl_init;
    t->nuke_hook = callback_sdl_nuke;
    t->xtra_hook = callback_sdl_xtra;
    t->curs_hook = callback_sdl_curs;
    t->bigcurs_hook = callback_sdl_bigcurs;
    t->wipe_hook = callback_sdl_wipe;
    t->text_hook = callback_sdl_text;
    if (g_tiles_mode)
        t->pict_hook = callback_sdl_pict;
    t->data = d;
    angband_term[term_index] = t;
    return 0;
}

static void sdl_window_load_ttf_font(sdl_window* d, int window_width)
{
    // Find suitable font size so that it fits into GLYPH_WIDTHxGLYPH_HEIGHT
    // cell. We don't need to do this for ASCII mode because we are not
    // restricted to certain cell width, and can use any.
    int max_size = d->cell_h;
    int min_size = max_size / 2;
    TTF_Font* font = NULL;
    for (d->ttf_font_size = max_size; d->ttf_font_size >= min_size; d->ttf_font_size--) {
        log_debug("trying TTF font size %d", d->ttf_font_size);
        if (font == NULL) {
            font = TTF_OpenFont("lib/xtra/font/InputMono-Bold.ttf", d->ttf_font_size);
            if (!font) {
                log_error("TTF_OpenFont failed: %s", SDL_GetError());
                quit("could not load TTF font");
            }
        }
        int measured_w = 0;
        TTF_MeasureString(font, "M", 1, 0, &measured_w, NULL);
        if (g_tiles_mode) {
            if (measured_w <= d->cell_w) {
                log_debug("chose TTF font size %d, em width %d", d->ttf_font_size, measured_w);
                break;
            }
        } else {
            int cols = d->dpi_scale * window_width / measured_w;
            if (cols >= 80) {
                log_debug("adjust cell size from %d to %d", d->cell_w, measured_w);
                d->cell_w = measured_w;
                break;
            }
        }
        TTF_CloseFont(font);
        font = NULL;
    }
    if (!font) {
        log_error("could not find suitable font size");
        quit("could not find suitable font size");
    }
    // Build TTF font atlas.
    d->font_atlas = SDL_CreateTexture(d->renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 16 * d->cell_w, 16 * d->cell_h);
    if (!d->font_atlas) {
        log_error("SDL_CreateTexture failed: %s", SDL_GetError());
        quit("could not create TTF glyph cache");
    }
    SDL_SetRenderTarget(d->renderer, d->font_atlas);
    SDL_Color white = (SDL_Color){255, 255, 255, 255};
    SDL_FRect dst = {
        .w = d->cell_w,
        .h = d->cell_h,
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
        SDL_Texture* gtex = SDL_CreateTextureFromSurface(d->renderer, gsurf);
        SDL_DestroySurface(gsurf);
        if (!gtex) {
            log_error("prepare_glyph: could not create texture from surface: %s", SDL_GetError());
            quit("could not create SDL texture");
        }
        SDL_SetTextureBlendMode(gtex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(gtex, SDL_SCALEMODE_LINEAR);
        dst.x = d->cell_w * (ch % 16);
        dst.y = d->cell_h * (ch >> 4);
        SDL_RenderTexture(d->renderer, gtex, NULL, &dst);
    }
    SDL_SetRenderTarget(d->renderer, NULL);
    SDL_SetTextureScaleMode(d->font_atlas, SDL_SCALEMODE_LINEAR);
    TTF_CloseFont(font);
}

static void sdl_window_create(sdl_window* d, int scale, int window_width, int window_height, bool fullscreen)
{
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
    if (fullscreen) {
        d->margin_x = (window_width % GLYPH_WIDTH) / 2;
        d->margin_y = (window_height % GLYPH_HEIGHT) / 2;
        log_debug("margin_x=%d margin_y=%d", d->margin_x, d->margin_y);
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (!SDL_CreateWindowAndRenderer("Sil-more SDL3", window_width, window_height,
            flags, &d->window, &d->renderer))
    {
        log_error("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        quit("could not create SDL window");
    }
    if (fullscreen)
        SDL_HideCursor();

    // Ensure predictable alpha blending (cursor/text)
    SDL_SetRenderDrawBlendMode(d->renderer, SDL_BLENDMODE_BLEND);

    float dpi_scale = SDL_GetWindowDisplayScale(d->window);
    d->dpi_scale = dpi_scale;
    d->cell_h = dpi_scale * scale * GLYPH_HEIGHT;
    d->cell_w = dpi_scale * scale * GLYPH_WIDTH;

    if (g_font_mode == FONTMODE_TTF) {
        sdl_window_load_ttf_font(d, window_width);
    } else {
        SDL_Surface* font_surface = IMG_Load("lib/xtra/font/font_8x16.png");
        if (!font_surface) {
            log_error("IMG_Load failed: %s", SDL_GetError());
            quit("could not load bitmap font");
        }
        log_debug("bitmap font loaded");
        d->font_atlas = SDL_CreateTextureFromSurface(d->renderer, font_surface);
        if (!d->font_atlas) {
            log_error("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
            quit("could not create bitmap font atlas");
        }
        SDL_DestroySurface(font_surface);
        // Make atlas draw crisply
        SDL_SetTextureScaleMode(d->font_atlas, SDL_SCALEMODE_NEAREST);
    }

    SDL_SetTextureBlendMode(d->font_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(d->font_atlas, 255, 255, 255);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    d->cols = dpi_scale * window_width / d->cell_w;
    d->rows = dpi_scale * window_height / d->cell_h;
    d->margin_x = (dpi_scale * window_width - d->cols * d->cell_w) / 2;
    d->margin_y = (dpi_scale * window_height - d->rows * d->cell_h) / 2;

    // Create a persistent offscreen canvas to render into
    d->canvas = SDL_CreateTexture(d->renderer, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_TARGET,
                                  d->cols * d->cell_w, d->rows * d->cell_h);
    if (d->canvas) {
        SDL_SetTextureBlendMode(d->canvas, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(d->canvas, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderTarget(d->renderer, d->canvas);
        SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
        SDL_RenderClear(d->renderer);
        d->need_present = true;
    } else {
        log_error("Create canvas failed: %s", SDL_GetError());
        quit("could not create canvas");
    }

    // TODO: load tileset only once and reuse it for all windows?
    if (g_tiles_mode) {
        log_debug("preparing tileset");
        d->use_tiles = true;
        d->tile_w = 2 * GLYPH_WIDTH;
        d->tile_h = GLYPH_HEIGHT;
        SDL_Surface* ts = IMG_Load("lib/xtra/graf/16x16_new.png");
        if (ts) {
            log_debug("tileset loaded");
            int tw = ts->w;
            d->tileset = SDL_CreateTextureFromSurface(d->renderer, ts);
            SDL_DestroySurface(ts);
            if (!d->tileset) {
                log_error("Failed to create tileset texture: %s", SDL_GetError());
                quit("could not create tileset texture");
            } else {
                SDL_SetTextureScaleMode(d->tileset, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(d->tileset, SDL_BLENDMODE_BLEND);
                d->tileset_cols = (d->tile_w > 0) ? (tw / d->tile_w) : 0;
            }
        } else {
            log_error("Failed to load tileset PNG: %s", SDL_GetError());
            quit("could not load tileset");
        }
    }
}

errr init_sdl(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--font")) {
            if (argc > i + 1) {
                const char* font_mode = argv[++i];
                if (!strcmp(font_mode, "ttf")) {
                    g_font_mode = FONTMODE_TTF;
                } else if (!strcmp(font_mode, "bitmap")) {
                        g_font_mode = FONTMODE_BITMAP;
                } else {
                    log_error("unknown font mode `%s`", font_mode);
                    quit("unknown font mode");
                }
            } else {
                log_error("--font requires an argument");
                quit("--font requires an argument");
            }
        } else if (!strcmp(argv[i], "--scale")) {
            if (argc > i + 1) {
                const char* scale_str = argv[++i];
                double scale = SDL_atof(scale_str);
                if (scale <= 0)
                    quit("wrong scale value, must be >= 1");
                g_scale = scale;
            } else {
                log_error("--scale requires an argument");
                quit("--scale requires an argument");
            }
        } else if (!strcmp(argv[i], "--ascii")) {
            g_tiles_mode = false;
        } else if (!strcmp(argv[i], "--windowed")) {
            g_fullscreen = false;
        } else {
            log_error("unrecognised command line switch `%s`", argv[i]);
            quit("unrecognised command line switch");
        }
    }

    log_debug("init_sdl");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        quit("could not init SDL");
    }
    if (g_font_mode == FONTMODE_TTF && !TTF_Init()) {
        log_error("TTF_Init failed: %s", SDL_GetError());
        quit("could not init TTF");
    }
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

    // Initialize palette mapping from term attrs to SDL_Color.
    #define RGB(_r,_g,_b) (SDL_Color){.r = (_r), .g = (_g), .b = (_b)}
    g_palette[0] = RGB(0, 0, 0);
    g_palette[1] = RGB(255, 255, 255);
    g_palette[2] = RGB(127, 127, 127);
    g_palette[3] = RGB(255, 186, 0); // orange
    g_palette[4] = RGB(255, 39, 23); // red
    g_palette[5] = RGB(0, 255, 0); // green
    g_palette[6] = RGB(0, 0, 255); // blue
    g_palette[7] = RGB(96, 78, 69); // umber
    g_palette[8] = RGB(80, 80, 80); // dark grey
    g_palette[9] = RGB(192, 192, 192); // light grey
    g_palette[10] = RGB(255, 73, 255); // purple
    g_palette[11] = RGB(252, 236, 0); // yellow
    g_palette[12] = RGB(255, 39, 23); // light red
    g_palette[13] = RGB(0, 255, 0); // light green
    g_palette[14] = RGB(61, 132, 255); // light blue
    g_palette[15] = RGB(231, 185, 3); // light umber
    #undef RGB

    // Create all windows.
    sdl_window_create(&windows[0], g_scale, screen.w, screen.h, g_fullscreen);
    sdl_window_link_term(&windows[0], 0);
    Term_activate(&windows[0].t); // TODO: need to call it somewhere!

    // for (int i = 1; i < MAX_TERM_DATA; i++) {
    //     sdl_window_create(&windows[i], 1, screen.w / 4, screen.h / 4, false);
    //     sdl_window_link_term(&windows[i], i);
    // }

    ANGBAND_SYS = "sdl";
    if (g_tiles_mode) {
        ANGBAND_GRAF = "new";
        arg_graphics = GRAPHICS_MICROCHASM;
        use_graphics = GRAPHICS_MICROCHASM;
        use_bigtile = true;
    } else {
        ANGBAND_GRAF = "old";
        arg_graphics = GRAPHICS_PSEUDO;
        use_graphics = GRAPHICS_PSEUDO;
    }

    log_debug("init_sdl: SDL term opened (tiles_mode=%d higher_pict=%d always_pict=%d)",
              g_tiles_mode, Term->higher_pict, Term->always_pict);
    return 0;
}
