#include "angband.h"
#include "main.h"
#include "z-term.h"
#include "z-virt.h"
#include "log/log.h"
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

/* Forward declarations so full-frame repaint can call these before their definitions */
static errr sdl3_text(int x, int y, int n, byte a, cptr s);
static errr sdl3_pict(int x, int y, int n, const byte* ap, const char* cp,
                      const byte* tap, const char* tcp);

const char help_sdl[] = "SDL";

typedef enum {
    FONTMODE_TTF,
    FONTMODE_BITMAP
} font_mode_t;

static font_mode_t g_font_mode = FONTMODE_TTF;
static int g_scale = 1;
static int g_bitmap_cell_w = 8;
static int g_bitmap_cell_h = 16;
static int g_atlas_inset = 3;
static bool g_tiles_mode = false; /* set by --tiles CLI switch */
static bool g_tiles_always = false; /* --tiles-always forces always_pict (draw every cell as pict) */
static bool g_deferred_tiles_react = false; /* defer reset_visuals until core finishes init */

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* canvas;
    SDL_Texture* font_atlas; // kept for atlas path if needed
    SDL_Texture* glyph_cache[256]; // cached monochrome glyph textures (one per ASCII code)
    int glyph_w[256];
    int glyph_h[256];
    /* Tileset support */
    SDL_Texture* tileset;
    int tile_w, tile_h;
    int tileset_cols;
    bool use_tiles;
    /* Font / cell configuration */
    TTF_Font* font;
    int cell_w, cell_h; // size of a text cell
    int cols, rows; // term size
    SDL_Color palette[16]; // map your attr values to SDL colors
    bool request_quit;
    bool need_present;
} sdl3_term_t;

static sdl3_term_t* td(term* t)
{
    return (sdl3_term_t*)t->data;
}

static void handle_event(sdl3_term_t* d, const SDL_Event* ev)
{
    if (ev->type == SDL_EVENT_QUIT) {
        d->request_quit = true;
        Term_keypress(27); // ESC or define a quit signal
    } else if (ev->type == SDL_EVENT_KEY_UP) {
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
        } else {
            bool arrow = false;
            switch (key) {
                case SDLK_UP:
                    arrow = true;
                    key = '8';
                    break;
                case SDLK_DOWN:
                    arrow = true;
                    key = '2';
                    break;
                case SDLK_LEFT:
                    arrow = true;
                    key = '4';
                    break;
                case SDLK_RIGHT:
                    arrow = true;
                    key = '6';
                    break;
            }
            if (arrow && (ev->key.mod & SDL_KMOD_SHIFT))
                Term_keypress('.');
            if (ev->key.mod & SDL_KMOD_GUI)
                key = KTRL(key);
        }

        Term_keypress(key);
    } else if (ev->type == SDL_EVENT_WINDOW_RESIZED) {
        // With logical presentation, compute cols/rows from the renderer's logical size
        int lw = 0, lh = 0;
        SDL_RendererLogicalPresentation mode;
        SDL_GetRenderLogicalPresentation(d->renderer, &lw, &lh, &mode);
        if (lw > 0 && lh > 0) {
            int cols = lw / d->cell_w;
            int rows = lh / d->cell_h;
            if (cols != d->cols || rows != d->rows) {
                d->cols = cols;
                d->rows = rows;
                Term_resize(cols, rows);

                // Recreate canvas for new logical size
                if (d->canvas) SDL_DestroyTexture(d->canvas);
                d->canvas = SDL_CreateTexture(d->renderer, SDL_PIXELFORMAT_RGBA8888,
                                              SDL_TEXTUREACCESS_TARGET, lw, lh);
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
}

static errr sdl3_xtra(int n, int v)
{
    sdl3_term_t* d = td(Term);
    switch (n) {
    case TERM_XTRA_EVENT: {
        int wait_ms = v;
        SDL_Event ev;

        if (wait_ms > 0) {
            // Wait for an event with timeout
            if (SDL_WaitEventTimeout(&ev, wait_ms))
                handle_event(d, &ev);
        } else {
            // Poll for events without waiting
            while (SDL_PollEvent(&ev))
                handle_event(d, &ev);
        }
        return 0;
    }
    case TERM_XTRA_FLUSH:
        // Flush pending input events (drain queue)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {}
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
            SDL_RenderTexture(d->renderer, d->canvas, NULL, NULL);
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
        log_debug("TERM_XTRA_REACT received (tiles_mode=%d use_graphics=%d arg_graphics=%d deferred=%d)",
                  g_tiles_mode, use_graphics, arg_graphics, g_deferred_tiles_react);
        reset_visuals(true);
        return 0;
    default:
        return 0;
    }
}

static errr sdl3_curs(int x, int y) {
    sdl3_term_t* d = td(Term);
    SDL_SetRenderTarget(d->renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, d->cell_w, d->cell_h };
    SDL_SetRenderClipRect(d->renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, d->cell_w, d->cell_h };
    SDL_SetRenderDrawColor(d->renderer, 255, 255, 255, 80);
    SDL_RenderFillRect(d->renderer, &r);
    SDL_SetRenderClipRect(d->renderer, NULL);
    d->need_present = true;
    return 0;
}

static errr sdl3_wipe(int x, int y, int n) {
    sdl3_term_t* d = td(Term);
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

static void prepare_glyph(sdl3_term_t* d, Uint32 ch)
{
    // Create a white glyph texture once and cache it
    SDL_Color white = (SDL_Color){255, 255, 255, 255};
    SDL_Surface* gsurf = TTF_RenderGlyph_Blended(d->font, ch, white);
    if (!gsurf) {
        log_error("prepare_glyph: could not render `%c` character: %s", ch, SDL_GetError());
        quit("could not render TTF character");
    }
    int gw = gsurf->w;
    int gh = gsurf->h;
    SDL_Texture* gtex = SDL_CreateTextureFromSurface(d->renderer, gsurf);
    SDL_DestroySurface(gsurf);

    if (!gtex) {
        log_error("prepare_glyph: could not create texture from surface: %s", SDL_GetError());
        quit("could not create SDL texture");
    }

    SDL_SetTextureBlendMode(gtex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(gtex, SDL_SCALEMODE_NEAREST);

    d->glyph_cache[ch] = gtex;
    d->glyph_w[ch] = gw;
    d->glyph_h[ch] = gh;
}

/* Definition (after forward declaration above) */
static errr sdl3_text(int x, int y, int n, byte a, cptr s) {
    sdl3_term_t* d = td(Term);
    if (!d) return 0;
    if (g_font_mode == FONTMODE_TTF && !d->font) return 0;
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

    SDL_Color col = d->palette[a % 16];

    if (g_font_mode == FONTMODE_BITMAP) {
        // Ensure nearest sampling and identity modulation for crisp output
        SDL_SetTextureColorMod(d->font_atlas, col.r, col.g, col.b);
        SDL_SetTextureAlphaMod(d->font_atlas, 255);
    }

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (g_font_mode == FONTMODE_TTF) {
            if (!d->glyph_cache[ch])
                prepare_glyph(d, ch);
            SDL_FRect dst = {
                (float)((x + i) * d->cell_w),
                (float)(y * d->cell_h),
                (float)d->glyph_w[ch],
                (float)d->glyph_h[ch]
            };
            if (use_graphics == GRAPHICS_PSEUDO && (ch == '#' || ch == '%')) {
                SDL_SetRenderDrawColor(d->renderer, col.r, col.g, col.b, SDL_ALPHA_OPAQUE);
                SDL_RenderFillRect(d->renderer, &dst);
            } else {
                SDL_Texture* glyph = d->glyph_cache[ch];
                SDL_SetTextureColorMod(glyph, col.r, col.g, col.b);
                SDL_SetTextureAlphaMod(glyph, 255);
                int dy = (d->cell_h - d->glyph_h[ch]);
                dy /= 2;
                dst.y += dy;
                SDL_RenderTexture(d->renderer, glyph, NULL, &dst);
            }
        } else {
            SDL_FRect dst = {
                (float)((x + i) * d->cell_w),
                (float)(y * d->cell_h),
                (float)d->cell_w,
                (float)d->cell_h
            };
            if (use_graphics == GRAPHICS_PSEUDO && (ch == '#' || ch == '%')) {
                SDL_SetRenderDrawColor(d->renderer, col.r, col.g, col.b, SDL_ALPHA_OPAQUE);
                SDL_RenderFillRect(d->renderer, &dst);
            } else {
                const int glyph_w = 8;   // source glyph width in atlas
                const int glyph_h = 16;  // source glyph height in atlas
                SDL_FRect src = {
                    (float)((ch % 16) * 16 + g_atlas_inset),
                    (float)((ch / 16) * glyph_h),
                    (float)glyph_w,
                    (float)glyph_h
                };
                SDL_RenderTexture(d->renderer, d->font_atlas, &src, &dst);
            }
        }
    }

    d->need_present = true;
    return 0;
}

/* Definition (after forward declaration above) */
static errr sdl3_pict(int x, int y, int n, const byte* ap, const char* cp,
                       const byte* tap, const char* tcp)
{
    sdl3_term_t* d = td(Term);
    if (!d)
        return 0;
    log_trace("sdl3_pict stripe start: y=%d x=%d n=%d tileset=%p use_tiles=%d higher_pict=%d always_pict=%d",
              y, x, n, (void*)d->tileset, d->use_tiles, Term->higher_pict, Term->always_pict);

    /* If tileset not active, fallback to text rendering */
    if (!d->use_tiles || !d->tileset) {
        log_trace("sdl3_pict fallback to text (no tiles) stripe y=%d", y);
        for (int i = 0; i < n; ++i) {
            byte a = ap ? ap[i] : 0;
            char c = cp ? cp[i] : ' ';
            sdl3_text(x + i, y, 1, a, &c);
        }
        return 0;
    }

    SDL_SetRenderTarget(d->renderer, d->canvas);

    for (int i = 0; i < n; ++i) {
        byte a = ap[i];
        char c = cp[i];

        /* Skip placeholder (second half of bigtile) outright */
        if (a == 255 && (unsigned char)c == 0xFF) {
            continue;
        }

        /* Detect bigtile pair (current + placeholder) */
        bool have_pair = false;
        if (use_bigtile && (i + 1 < n) &&
            ap[i + 1] == 255 && (unsigned char)cp[i + 1] == 0xFF) {
            have_pair = true;
        }

        /* Non-graphical cell -> fallback to text */
        if ((a & 0x80) == 0 || (c & 0x80) == 0) {
            sdl3_text(x + i, y, 1, (byte)(a & 0x7F), &cp[i]);
            continue;
        }

        int row = a & 0x3F;
        int col = c & 0x3F;

        bool glow = a & GRAPHICS_GLOW_MASK;
        bool alert = c & GRAPHICS_ALERT_MASK;

        // if (row < 0 || row >= 64 || col < 0 || col >= d->tileset_cols) {
        //     sdl3_text(x + i, y, 1, (byte)(a & 0x7F), &cp[i]);
        //     continue;
        // }

        /* Underlay (terrain) selection */
        int trow = row;
        int tcol = col;
        if (tap && tcp) {
            byte ta = tap[i];
            char tc = tcp[i];
            if ((ta & 0x80) && (tc & 0x80)) {
                trow = ta & 0x3F;
                tcol = tc & 0x3F;
            }
        }

        float dst_x = (float)((x + i) * d->cell_w);
        float dst_y = (float)(y * d->cell_h);
        float dst_w = (float)(use_bigtile ? (2 * d->cell_w) : d->cell_w);
        float dst_h = (float)d->cell_h;

        /* Unconditionally clear the full (possibly 2-cell) destination area to avoid ghosting */
        SDL_FRect clear_rect = { dst_x, dst_y, dst_w, dst_h };
        SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(d->renderer, &clear_rect);

        /* Draw terrain underlay ALWAYS */
        SDL_FRect src_t = {
            (float)(tcol * d->tile_w),
            (float)(trow * d->tile_h),
            (float)d->tile_w,
            (float)d->tile_h
        };
        SDL_RenderTexture(d->renderer, d->tileset, &src_t, &clear_rect);

        /* Draw base tile */
        SDL_FRect src = {
            (float)(col * d->tile_w),
            (float)(row * d->tile_h),
            (float)d->tile_w,
            (float)d->tile_h
        };
        SDL_RenderTexture(d->renderer, d->tileset, &src, &clear_rect);

        /* Overlays (glow / alert) */
        if (glow) {
            SDL_SetRenderDrawBlendMode(d->renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(d->renderer, 255, 255, 0, 70);
            SDL_RenderFillRect(d->renderer, &clear_rect);
        }
        if (alert) {
            SDL_SetRenderDrawBlendMode(d->renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(d->renderer, 255, 0, 0, 90);
            SDL_RenderFillRect(d->renderer, &clear_rect);
        }

        log_trace("tile draw merged=%d row=%d col=%d under=%d/%d glow=%d alert=%d screen=(%.0f,%.0f)",
                  have_pair, row, col, trow, tcol, glow, alert, dst_x, dst_y);

        if (have_pair) {
            ++i; /* consume placeholder partner */
        }
    }

    d->need_present = true;
    return 0;
}

static void sdl3_term_nuke() {
    log_debug("sdl3_term_nuke");
    sdl3_term_t* d = td(Term);
    if (!d)
        return;

    // Stop text input
    // if (d->window) {
    //     SDL_StopTextInput(d->window);
    // }
    if (d->font) {
        TTF_CloseFont(d->font);
        d->font = NULL;
    }
    for (int i = 0; i < 256; ++i) {
        if (d->glyph_cache[i]) {
            SDL_DestroyTexture(d->glyph_cache[i]);
            d->glyph_cache[i] = NULL;
        }
    }
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
    KILL(d);
}

static void sdl3_term_init(term* t) {
    log_debug("sdl3_term_init");
    sdl3_term_t* d;
    MAKE(d, sdl3_term_t);
    WIPE(d, sdl3_term_t);
    t->data = d;

    /* If tiles mode is active, set the cell size to the tile size, but still initialize
       the chosen font mode so that UI text (messages, menus) renders correctly. */
    if (g_tiles_mode) {
        /* Use 8x16 text cells; each 16x16 tile spans two cells horizontally (bigtile). */
        d->cell_w = g_scale * 8;
        d->cell_h = g_scale * 16;
        log_info("Tiles mode: text cell size %dx%d (tiles 16x16 via bigtile)", d->cell_w, d->cell_h);
    }

    /* Initialize font/bitmap mode independently of tiles */
    if (g_font_mode == FONTMODE_TTF) {
        int cell_height = 16;
        if (d->font == NULL) {
            TTF_Init();
            d->font = TTF_OpenFont("/Users/ilya/Library/Fonts/InputMono-Bold.ttf", cell_height * g_scale);
            if (!d->font) {
                log_error("TTF_OpenFont failed: %s", SDL_GetError());
                quit("could not load TTF font");
            }
        }
        int measured_w = 0;
        TTF_MeasureString(d->font, "M", 1, 0, &measured_w, NULL);
        if (!g_tiles_mode) {
            d->cell_w = measured_w;
            d->cell_h = cell_height * g_scale;
        }
    } else if (g_font_mode == FONTMODE_BITMAP) {
        if (!g_tiles_mode) {
            d->cell_w = g_bitmap_cell_w;
            d->cell_h = g_bitmap_cell_h;
        }
    } else {
        /* Fallback: do not abort in tiles mode; just default to bitmap cell metrics */
        if (!g_tiles_mode) {
            log_error("Unknown font mode; defaulting to 8x16 cell");
            d->cell_w = 8;
            d->cell_h = 16;
        }
    }

    d->cols = t->wid;
    d->rows = t->hgt;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    if (!SDL_CreateWindowAndRenderer("Sil-more SDL3", d->cols * d->cell_w, d->rows * d->cell_h,
        SDL_WINDOW_HIGH_PIXEL_DENSITY, &d->window, &d->renderer)) {
        log_error("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
    }

    // Ensure predictable alpha blending (cursor/text)
    SDL_SetRenderDrawBlendMode(d->renderer, SDL_BLENDMODE_BLEND);
    // Force integer-scaled presentation to avoid fractional pixel sampling
    SDL_SetRenderLogicalPresentation(
        d->renderer,
        d->cols * d->cell_w,
        d->rows * d->cell_h,
        SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
    );
    // Auto-detect device scale (HiDPI) and reopen TTF font at scaled size if user didn't set one
    if (g_font_mode == FONTMODE_TTF && g_scale == 1) {
        int lw = 0, lh = 0;
        SDL_RendererLogicalPresentation mode;
        SDL_GetRenderLogicalPresentation(d->renderer, &lw, &lh, &mode);
        int outw = 0, outh = 0;
        SDL_GetRenderOutputSize(d->renderer, &outw, &outh);
        if (lw > 0 && lh > 0 && outw > 0 && outh > 0) {
            log_debug("lw=%d lh=%d outw=%d outh=%d", lw, lh, outw, outh);
            int sx = outw / lw, sy = outh / lh;
            int s = (sx < sy ? sx : sy);
            if (s > 1) {
                log_info("setting font scale to %d", s);
                g_scale = s;
                if (d->font) {
                    TTF_CloseFont(d->font);
                }
                int base_h = d->cell_h;
                d->font = TTF_OpenFont("/Users/ilya/Library/Fonts/InputMono-Bold.ttf", base_h * g_scale);
                TTF_MeasureString(d->font, "M", 1, 0, &d->cell_w, NULL);
                d->cell_h = base_h * g_scale;
                // Update logical presentation to reflect new cell dimensions after scaling
                SDL_SetRenderLogicalPresentation(
                    d->renderer,
                    d->cols * d->cell_w,
                    d->rows * d->cell_h,
                    SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                );
            }
        }
    }

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

    if (g_font_mode == FONTMODE_BITMAP) {
        SDL_Surface* font_surface = IMG_Load("font_16.png");
        if (font_surface) {
            log_debug("font loaded");
            d->font_atlas = SDL_CreateTextureFromSurface(d->renderer, font_surface);
            SDL_DestroySurface(font_surface);
            SDL_SetTextureScaleMode(d->font_atlas, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(d->font_atlas, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(d->font_atlas, 255, 255, 255);
            SDL_SetTextureAlphaMod(d->font_atlas, 255);
            // Make atlas draw crisply
        } else {
            log_error("IMG_Load failed: %s", SDL_GetError());
            quit("could not load bitmap font");
        }
    }

    // Load tileset if requested
    if (g_tiles_mode) {
        log_debug("preparing tileset");
        d->use_tiles = true;
        d->tile_w = 16;
        d->tile_h = 16;
        SDL_Surface* ts = IMG_Load("lib/xtra/graf/16x16_microchasm.png");
        if (ts) {
            log_debug("tileset loaded");
            int tw = ts->w;
            d->tileset = SDL_CreateTextureFromSurface(d->renderer, ts);
            SDL_DestroySurface(ts);
            if (!d->tileset) {
                log_error("Failed to create tileset texture: %s", SDL_GetError());
                d->use_tiles = false;
            } else {
                SDL_SetTextureScaleMode(d->tileset, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(d->tileset, SDL_BLENDMODE_BLEND);
                d->tileset_cols = (d->tile_w > 0) ? (tw / d->tile_w) : 0;
            }
        } else {
            log_error("Failed to load tileset PNG: %s", SDL_GetError());
            d->use_tiles = false;
        }
    }
    // Enable text input for proper keyboard handling
    // SDL_StartTextInput(d->window);

    // Initialize palette mapping from term attrs to SDL_Color.
    #define RGB(_r,_g,_b) (SDL_Color){.r = (_r), .g = (_g), .b = (_b)}
    d->palette[0] = RGB(0, 0, 0);
    d->palette[1] = RGB(255, 255, 255);
    d->palette[2] = RGB(127, 127, 127);
    d->palette[3] = RGB(255, 186, 0); // orange
    d->palette[4] = RGB(255, 39, 23); // red
    d->palette[5] = RGB(0, 255, 0); // green
    d->palette[6] = RGB(0, 0, 255); // blue
    d->palette[7] = RGB(96, 78, 69); // umber
    d->palette[8] = RGB(80, 80, 80); // dark grey
    d->palette[9] = RGB(192, 192, 192); // light grey
    d->palette[10] = RGB(255, 73, 255); // purple
    d->palette[11] = RGB(252, 236, 0); // yellow
    d->palette[12] = RGB(255, 39, 23); // light red
    d->palette[13] = RGB(0, 255, 0); // light green
    d->palette[14] = RGB(61, 132, 255); // light blue
    d->palette[15] = RGB(231, 185, 3); // light umber
    #undef RGB
}

errr term_open_sdl3(int w, int h, term** out) {
    log_debug("%dx%d", w, h);
    term* t;
    MAKE(t, term);
    WIPE(t, term);

    term_init(t, w, h, 256); // k=key queue size
    t->soft_cursor = true;
    t->higher_pict = (g_tiles_mode ? true : false);
    if (g_tiles_mode && g_tiles_always) {
        t->always_pict = true;
    }
    t->never_frosh = true;
    t->init_hook = sdl3_term_init;
    t->nuke_hook = sdl3_term_nuke;
    t->xtra_hook = sdl3_xtra;
    t->curs_hook = sdl3_curs;
    t->bigcurs_hook = sdl3_curs;
    t->wipe_hook = sdl3_wipe;
    t->text_hook = sdl3_text;
    if (g_tiles_mode) {
        log_debug("setting pict hook (higher_pict=%d always_pict=%d)", t->higher_pict, t->always_pict);
        t->pict_hook = sdl3_pict;
    }

    angband_term[0] = t;
    Term_activate(t);
    *out = t;
    return 0;
}

errr init_sdl(int argc, char **argv)
{
    // CLI switches:
    //   --sdl-bitmap           use bitmap atlas (ascii_table_16x16.png)
    //   --sdl-ttf              use TTF rendering (default)
    //   --cell WxH             bitmap cell size (default 8x16)
    //   --atlas-inset N        left inset per atlas cell (default 0)
    //   --scale N              integer scale factor for bitmap cell size (default 1)
    //   --ttf-scale N          integer HiDPI scale for TTF (overrides auto-detect)
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--sdl-bitmap")) {
            g_font_mode = FONTMODE_BITMAP;
        } else if (!strcmp(argv[i], "--sdl-ttf")) {
            g_font_mode = FONTMODE_TTF;
        } else if (!strcmp(argv[i], "--scale") && i + 1 < argc) {
            const char* scale_str = argv[++i];
            double scale = SDL_atof(scale_str);
            if (scale <= 0)
                quit("wrong scale value, must be >= 1");
            g_bitmap_cell_w *= scale;
            g_bitmap_cell_h *= scale;
            g_scale = scale;
        } else if (!strcmp(argv[i], "--ttf-scale") && i + 1 < argc) {
            int s = SDL_atoi(argv[++i]);
            if (s < 1)
                quit("wrong ttf-scale value, must be >= 1");
            g_scale = s;
        } else if (!strcmp(argv[i], "--tiles")) {
            g_tiles_mode = true;
        } else if (!strcmp(argv[i], "--tiles-always")) {
            g_tiles_mode = true;
            g_tiles_always = true;
        }
    }

    log_debug("init_sdl");
    ANGBAND_SYS = "sdl";
    if (g_tiles_mode) {
        ANGBAND_GRAF = "new";
        arg_graphics = GRAPHICS_MICROCHASM;
        use_graphics = GRAPHICS_MICROCHASM;
        g_font_mode = FONTMODE_BITMAP;
        use_bigtile = true;
    } else {
        ANGBAND_GRAF = "old";
        arg_graphics = GRAPHICS_PSEUDO;
        use_graphics = GRAPHICS_PSEUDO;
    }
    errr e = term_open_sdl3(96, 30, &Term);
    if (g_tiles_mode) {
        /* Defer visuals reset until core triggers TERM_XTRA_REACT (data tables allocated) */
        g_deferred_tiles_react = true;
    }
    log_debug("init_sdl: SDL term opened (tiles_mode=%d higher_pict=%d always_pict=%d)",
              g_tiles_mode, Term->higher_pict, Term->always_pict);
    return e;
}
