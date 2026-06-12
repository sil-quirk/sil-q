#include "angband.h"
#include "sdl/main-sdl-private.h"

void sdl_apply_story_font_state(bool active)
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

void sdl_apply_story_grid_state(bool grid)
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

void sdl_apply_story_slot_state(int slot)
{
    log_trace("Story slot state apply: slot=%d term=%p", slot, (void*)Term);
    for (int i = 0; i < MAX_TERM_DATA; i++)
    {
        if (g_views[i].term_ready)
        {
            g_views[i].t.story_font_slot = slot;
        }
    }
    if (Term)
        Term->story_font_slot = slot;
}

void sdl_story_font_reset_state(void)
{
    g_state.story_font_depth = 0;
    sdl_apply_story_font_state(false);
    g_state.story_font_grid = false;
    sdl_apply_story_grid_state(false);
    g_state.story_font_slot = 0;
    sdl_apply_story_slot_state(0);
    if (Term)
        Term->story_chunk_active = false;
    log_trace("Story font state hard reset");
}

void sdl_render_mono_text(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col)
{
    if (!d || !d->font_atlas || n <= 0)
        return;

    SDL_SetTextureColorMod(d->font_atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        int atlas_cell_w = (d->font_atlas_cell_w > 0)
            ? d->font_atlas_cell_w : d->cell_w;
        int atlas_cell_h = (d->font_atlas_cell_h > 0)
            ? d->font_atlas_cell_h : d->cell_h;
        SDL_FRect src = {
            (ch & 15) * atlas_cell_w,
            (ch >> 4) * atlas_cell_h,
            atlas_cell_w,
            atlas_cell_h,
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

void sdl_render_mono_utf8_glyph(TTF_Font* font, float cell_w,
    float cell_h, float origin_x, float origin_y, int x, int y,
    int cell_offset, int cell_span, const char* s, int len, SDL_Color col)
{
    char text_buf[8];
    SDL_Surface* text_surface;
    SDL_Texture* text_texture;

    if (!font || !s || len <= 0 || len >= (int)sizeof(text_buf)
        || cell_w <= 0.0f || cell_h <= 0.0f)
    {
        return;
    }

    if (cell_span < 1)
        cell_span = 1;

    memcpy(text_buf, s, (size_t)len);
    text_buf[len] = '\0';

    text_surface = TTF_RenderText_Blended(font, text_buf, 0, col);
    if (!text_surface)
        return;

    text_texture = SDL_CreateTextureFromSurface(g_state.renderer, text_surface);
    if (text_texture)
    {
        float surf_w_f = (float)text_surface->w;
        float surf_h_f = (float)text_surface->h;
        float scale = (surf_h_f > 0.0f) ? (cell_h / surf_h_f) : 1.0f;
        float render_w = surf_w_f * scale;
        float max_w = (float)cell_span * cell_w;
        SDL_FRect dst;

        if (render_w > max_w) {
            if (origin_x > 0.0f || g_left_panel_debug_dump_rows) {
                log_debug("left-panel utf8 glyph clamp: term_x=%d y=%d "
                    "offset=%d span=%d cell=%.1fx%.1f surf=%dx%d "
                    "scaled_w=%.1f max_w=%.1f first_byte=0x%02x",
                    x, y, cell_offset, cell_span, (double)cell_w,
                    (double)cell_h, text_surface->w, text_surface->h,
                    (double)render_w, (double)max_w,
                    (unsigned int)(unsigned char)s[0]);
            }
            render_w = max_w;
        }

        dst.x = origin_x + ((float)(x + cell_offset) * cell_w)
            + ((max_w - render_w) * 0.5f);
        dst.y = origin_y + (float)y * cell_h;
        dst.w = render_w;
        dst.h = cell_h;

        SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, text_texture, NULL, &dst);
        SDL_DestroyTexture(text_texture);
    }

    SDL_DestroySurface(text_surface);
}

void sdl_render_mono_utf8_text_cells_at(SDL_Texture* atlas,
    int atlas_cell_w, int atlas_cell_h, TTF_Font* font, float cell_w,
    float cell_h, float origin_x, float origin_y, int x, int y, int n,
    const char* s, SDL_Color col)
{
    int byte_pos = 0;
    int cell_offset = 0;

    if (!s || n <= 0 || cell_w <= 0.0f || cell_h <= 0.0f)
        return;

    if (atlas)
    {
        SDL_SetTextureColorMod(atlas, col.r, col.g, col.b);
        SDL_SetTextureAlphaMod(atlas, 255);
    }

    while (byte_pos < n && s[byte_pos] && cell_offset < n)
    {
        unsigned char ch = (unsigned char)s[byte_pos];
        int remaining = n - byte_pos;
        int char_len = utf8_sequence_len_n(s + byte_pos, remaining);
        int char_width;

        if (char_len <= 0)
            break;

        if (ch < 0x80 || char_len == 1)
        {
            if (atlas)
            {
                SDL_FRect src = {
                    (float)((ch & 15) * atlas_cell_w),
                    (float)((ch >> 4) * atlas_cell_h),
                    (float)atlas_cell_w,
                    (float)atlas_cell_h,
                };
                SDL_FRect dst = {
                    origin_x + ((float)(x + cell_offset) * cell_w),
                    origin_y + (float)y * cell_h,
                    cell_w,
                    cell_h,
                };

                if (use_graphics == GRAPHICS_PSEUDO && solid_walls
                    && (ch == '#' || ch == '%'))
                {
                    SDL_SetRenderDrawColor(g_state.renderer, col.r, col.g,
                        col.b, SDL_ALPHA_OPAQUE);
                    SDL_RenderFillRect(g_state.renderer, &dst);
                }

                SDL_RenderTexture(g_state.renderer, atlas, &src, &dst);
            }

            byte_pos++;
            cell_offset++;
            continue;
        }

        char_width = utf8_display_width_n(s + byte_pos, char_len);
        if (char_width <= 0)
        {
            int overlay_offset = (cell_offset > 0) ? (cell_offset - 1) : 0;
            sdl_render_mono_utf8_glyph(font, cell_w, cell_h, origin_x,
                origin_y, x, y, overlay_offset, 1, s + byte_pos, char_len,
                col);
        }
        else
        {
            sdl_render_mono_utf8_glyph(font, cell_w, cell_h, origin_x,
                origin_y, x, y, cell_offset, char_width, s + byte_pos,
                char_len, col);
            cell_offset += char_width;
        }

        byte_pos += char_len;
    }
}

void sdl_render_mono_utf8_text_cells(SDL_Texture* atlas,
    int atlas_cell_w, int atlas_cell_h, TTF_Font* font, float cell_w,
    float cell_h, float origin_x, int x, int y, int n, const char* s,
    SDL_Color col)
{
    sdl_render_mono_utf8_text_cells_at(atlas, atlas_cell_w, atlas_cell_h,
        font, cell_w, cell_h, origin_x, 0.0f, x, y, n, s, col);
}

static void sdl_render_texture_with_clip(SDL_Texture* texture,
    const SDL_FRect* dst, const SDL_FRect* clip)
{
    bool apply_clip;
    bool had_clip;
    SDL_Rect old_clip = { 0 };
    SDL_Rect clip_rect;

    if (!texture || !dst)
        return;

    apply_clip = clip && clip->w > 0.0f && clip->h > 0.0f
        && (dst->x < clip->x || dst->y < clip->y
            || dst->x + dst->w > clip->x + clip->w
            || dst->y + dst->h > clip->y + clip->h);
    if (!apply_clip) {
        SDL_RenderTexture(g_state.renderer, texture, NULL, dst);
        return;
    }

    had_clip = SDL_RenderClipEnabled(g_state.renderer);
    if (had_clip)
        SDL_GetRenderClipRect(g_state.renderer, &old_clip);

    clip_rect.x = (int)clip->x;
    clip_rect.y = (int)clip->y;
    clip_rect.w = (int)(clip->w + 0.5f);
    clip_rect.h = (int)(clip->h + 0.5f);
    if (clip_rect.w < 1)
        clip_rect.w = 1;
    if (clip_rect.h < 1)
        clip_rect.h = 1;

    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);
    SDL_RenderTexture(g_state.renderer, texture, NULL, dst);
    SDL_SetRenderClipRect(g_state.renderer, had_clip ? &old_clip : NULL);
}

void sdl_render_story_text_free(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col)
{
    if (!d || !font || !s || n <= 0)
        return;

    char text_buf[256];
    int len = (n < 255) ? n : 255;
    len = utf8_safe_prefix_len(s, len);
    if (len <= 0)
        return;
    memcpy(text_buf, s, len);
    text_buf[len] = '\0';

    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text_buf, 0, col);
    if (!text_surface)
        return;

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(g_state.renderer, text_surface);
    if (text_texture) {
        float cell_h_f = (float)d->cell_h;
        float surf_h_f = (float)text_surface->h;
        float scale = (surf_h_f > 0.0f) ? (cell_h_f / surf_h_f) : 1.0f;
        float max_w = (float)(n * d->cell_w);

        SDL_FRect dst = {
            (float)(x * d->cell_w),
            (float)(y * d->cell_h),
            (float)(text_surface->w) * scale,
            cell_h_f
        };
        SDL_FRect clip = {
            dst.x,
            dst.y,
            max_w,
            cell_h_f
        };

        SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);
        sdl_render_texture_with_clip(text_texture, &dst, &clip);
        SDL_DestroyTexture(text_texture);
    }

    SDL_DestroySurface(text_surface);
}

int sdl_render_story_text_free_px(sdl_view* d, TTF_Font* font, float x_px, int y, const char* s, int n,
    SDL_Color col, float max_w_px)
{
    if (!d || !font || !s || n <= 0)
        return 0;

    char text_buf[256];
    int len = (n < 255) ? n : 255;
    len = utf8_safe_prefix_len(s, len);
    if (len <= 0)
        return 0;
    for (int i = 0; i < len; i++)
    {
        unsigned char ch = (unsigned char)s[i];
        text_buf[i] = (ch ? (char)ch : ' ');
    }
    text_buf[len] = '\0';

    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text_buf, 0, col);
    if (!text_surface)
        return 0;

    int adv_w_unscaled = 0;
    TTF_MeasureString(font, text_buf, len, 0, &adv_w_unscaled, NULL);

    float cell_h_f = (float)d->cell_h;
    float surf_h_f = (float)text_surface->h;
    float scale = (surf_h_f > 0.0f) ? (cell_h_f / surf_h_f) : 1.0f;
    float advance_w = (float)adv_w_unscaled * scale;
    float render_w = (float)text_surface->w * scale;

    if (max_w_px > 0.0f && advance_w > max_w_px)
        advance_w = max_w_px;

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(g_state.renderer, text_surface);
    if (text_texture)
    {
        SDL_FRect dst = {
            x_px,
            (float)(y * d->cell_h),
            render_w,
            cell_h_f
        };
        SDL_FRect clip = {
            x_px,
            (float)(y * d->cell_h),
            max_w_px,
            cell_h_f
        };

        SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);
        sdl_render_texture_with_clip(text_texture, &dst,
            max_w_px > 0.0f ? &clip : NULL);
        SDL_DestroyTexture(text_texture);
    }

    SDL_DestroySurface(text_surface);
    return (int)advance_w;
}

bool sdl_story_cell_is_text(byte a, char c)
{
    unsigned char uc = (unsigned char)c;

    /* High-bit attr/char pairs are tiles and are handled by pict hook. */
    if ((a & 0x80) && (uc & 0x80))
        return false;

    /* Bigtile second cell. */
    if (a == 255 && uc == 0xFF)
        return false;

    return true;
}

byte sdl_ui_text_fg_attr(byte attr)
{
    return (attr >= TERM_UI_SELECTED) ? TERM_DARK : attr;
}

byte sdl_ui_text_bg_attr(byte attr)
{
    return (attr >= TERM_UI_SELECTED)
        ? (byte)(attr - TERM_UI_SELECTED)
        : TERM_DARK;
}

SDL_Color sdl_color_from_attr(byte attr)
{
    SDL_Color col = {
        angband_color_table[attr][1],
        angband_color_table[attr][2],
        angband_color_table[attr][3],
        255
    };

    return col;
}

void sdl_fill_cell_span_with_attr(sdl_view* d, int x, int y, int n,
    byte attr)
{
    if (!d || n <= 0)
        return;

    SDL_Color bg = sdl_color_from_attr(sdl_ui_text_bg_attr(attr));
    SDL_FRect clear_rect = {
        x * (float)d->cell_w,
        y * (float)d->cell_h,
        n * (float)d->cell_w,
        (float)d->cell_h
    };

    bg.a = (attr >= TERM_UI_SELECTED) ? 255 : sdl_view_background_alpha(d);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_state.renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRect(g_state.renderer, &clear_rect);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
}

/* Width, in pixels, the secondary story font needs for a text segment when
 * scaled to the term cell height (matches sdl_render_story_text_free_px). */
static float sdl_story_seg_width_px(sdl_view* d, TTF_Font* font, const char* s,
    int n)
{
    char buf[256];
    int len;
    int adv_unscaled = 0;
    int font_h;

    if (!d || !font || !s || n <= 0)
        return 0.0f;

    len = (n < 255) ? n : 255;
    len = utf8_safe_prefix_len(s, len);
    if (len <= 0)
        return 0.0f;
    for (int i = 0; i < len; i++)
    {
        unsigned char ch = (unsigned char)s[i];
        buf[i] = (ch ? (char)ch : ' ');
    }
    buf[len] = '\0';

    if (!TTF_MeasureString(font, buf, len, 0, &adv_unscaled, NULL))
        return 0.0f;

    font_h = TTF_GetFontHeight(font);
    if (font_h < 1)
        font_h = d->cell_h;
    return (float)adv_unscaled * ((float)d->cell_h / (float)font_h);
}

/*
 * Word-wrap a message to the overlay log's visible band, measured in pixels
 * with the proportional secondary story font (so it wraps where the text
 * actually reaches the band edge, not by cell count).  Fills out_off/out_len
 * with each segment's byte offset and length and returns the segment count.
 */
int sdl_overlay_log_wrap(const char* msg, int max_segs, int* out_off,
    int* out_len)
{
    sdl_view* d = sdl_view_from_term(Term);
    TTF_Font* font = d ? sdl_story_font_for_view_slot(d,
        STORY_FONT_SLOT_SECONDARY) : NULL;
    int len = msg ? (int)strlen(msg) : 0;
    int wid = Term ? Term->wid : 0;
    int margin;
    float band_px;
    int n = 0;
    int pos = 0;

    if (len <= 0 || max_segs <= 0 || !out_off || !out_len || !d)
        return 0;

    margin = (d && sdl_view_is_overlay_log_pane(d))
        ? pane_log_overlay_left_margin(wid) : 0;
    band_px = (float)(wid - margin) * (float)d->cell_w
        - (float)d->cell_w * 0.25f - SDL_OVERLAY_LOG_TEXT_LEFT_PAD(d);

    if (!font || band_px <= 0.0f)
    {
        out_off[0] = 0;
        out_len[0] = len;
        return 1;
    }

    while (pos < len && n < max_segs)
    {
        int remain = len - pos;
        int limit = remain;
        int last_fit = 0;
        int last_space = -1;
        int take;

        if (limit >= 96)
            limit = 95;

        for (int k = 1; k <= limit; k++)
        {
            float w = sdl_story_seg_width_px(d, font, msg + pos, k);

            if (w > band_px && last_fit > 0)
                break;

            last_fit = k;
            if (msg[pos + k - 1] == ' ')
                last_space = k;

            if (w > band_px)
                break;
        }

        take = (last_fit > 0) ? last_fit : 1;

        if (take < remain)
        {
            if (last_space > 0)
                take = last_space;
        }

        out_off[n] = pos;
        out_len[n] = take;
        n++;

        pos += take;
        while (pos < len && msg[pos] == ' ')
            pos++;
    }

    return n;
}

static void sdl_render_overlay_log_message_row_px(sdl_view* d, int y,
    byte attr, cptr text)
{
    const int wid = Term->wid;
    TTF_Font* font = sdl_story_font_for_view_slot(d,
        STORY_FONT_SLOT_SECONDARY);
    int margin;
    float px;
    float right_edge;
    float max_w;
    SDL_Color col;

    sdl_fill_cell_span_with_attr(d, 0, y, wid, TERM_DARK);

    if (!font || !text || !text[0])
        return;

    margin = sdl_view_is_overlay_log_pane(d)
        ? pane_log_overlay_left_margin(wid) : 0;
    px = (float)margin * (float)d->cell_w + SDL_OVERLAY_LOG_TEXT_LEFT_PAD(d);
    right_edge = (float)wid * (float)d->cell_w;
    max_w = right_edge - px - (float)d->cell_w * 0.25f;
    if (max_w <= 0.0f)
        return;

    col = sdl_color_from_attr(sdl_ui_text_fg_attr(attr));
    sdl_render_story_text_free_px(d, font, px, y, text, (int)strlen(text),
        col, max_w);
}

/*
 * Render a combat-roll row with proportional pixel layout.
 *
 * The portable layer marks these rows with STORY_FLAG_PIXEL_PACK and stashes
 * the full line as semantic tokens (text runs + tiles) in a side buffer, so the
 * line is never truncated by the panel's cell width.  Here we re-pack those
 * tokens tightly in pixels -- text measured with the secondary story font,
 * tiles drawn inline -- left-aligned to the band so it starts where the
 * messages start.
 */
static void sdl_render_combat_roll_row_px(sdl_view* d, int y)
{
    const int wid = Term->wid;
    const float cell_w_f = (float)d->cell_w;
    const float cell_h_f = (float)d->cell_h;
    TTF_Font* font = sdl_story_font_for_view_slot(d, STORY_FONT_SLOT_SECONDARY);
    const combat_roll_token* toks = NULL;
    int ntok = pane_log_combat_row_tokens(y, &toks);
    int tile_cols = (use_bigtile && !graphics_are_ascii()) ? 2 : 1;
    float tile_w = (float)tile_cols * cell_w_f;
    int margin;
    float right_edge;
    float px;

    /* Clear the row's whole footprint so re-packed pixels leave no stragglers. */
    sdl_fill_cell_span_with_attr(d, 0, y, wid, TERM_DARK);

    if (!font || ntok <= 0 || !toks)
        return;

    /* Left-align the packed line to the visible band. */
    margin = sdl_view_is_overlay_log_pane(d)
        ? pane_log_overlay_left_margin(wid) : 0;
    px = (float)margin * cell_w_f + SDL_OVERLAY_LOG_TEXT_LEFT_PAD(d);
    right_edge = (float)wid * cell_w_f;

    for (int i = 0; i < ntok && px < right_edge; i++)
    {
        const combat_roll_token* t = &toks[i];

        if (t->is_tile)
        {
            SDL_FRect dst = { px, (float)y * cell_h_f, tile_w, cell_h_f };

            sdl_draw_map_tile_layers_at(-1, -1, t->attr, t->tile_char, 0, 0,
                &dst);
            px += tile_w;
        }
        else
        {
            int len = (int)strlen(t->text);
            SDL_Color col;

            if (len <= 0)
                continue;
            col = sdl_color_from_attr(sdl_ui_text_fg_attr(t->attr));
            sdl_render_story_text_free_px(d, font, px, y, t->text, len, col,
                right_edge - px);
            px += sdl_story_seg_width_px(d, font, t->text, len);
        }
    }
}

void sdl_render_story_row_packed(sdl_view* d, TTF_Font* font, int y, const byte* story_row,
    const char* row_chars, const byte* row_attr)
{
    if (!d || !font || !Term || !story_row || !row_chars || !row_attr)
        return;

    const int wid = Term->wid;
    const float cell_w_f = (float)d->cell_w;

    /* Overlay message/combat rows may carry side-buffered pixel layouts. */
    if (Term->scr && Term->scr->story && y >= 0 && y < Term->hgt)
    {
        for (int x = 0; x < wid; x++)
        {
            if (story_row[x] & STORY_FLAG_PIXEL_PACK)
            {
                cptr msg = NULL;
                byte attr = TERM_WHITE;

                if (pane_log_overlay_message_row(y, &msg, &attr))
                {
                    sdl_render_overlay_log_message_row_px(d, y, attr, msg);
                    return;
                }

                if (pane_log_combat_row_tokens(y, NULL) > 0)
                {
                    sdl_render_combat_roll_row_px(d, y);
                    return;
                }

                break;
            }
        }
    }

    int x = 0;
    while (x < wid)
    {
        /* Skip non-text cells (tiles) entirely to avoid clearing/overdrawing them. */
        if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
        {
            x++;
            continue;
        }

        byte flags = story_row[x];
        bool use_story = (flags & STORY_FLAG_USE) != 0;
        bool grid_align = (flags & STORY_FLAG_CELL_ALIGN) != 0;
        bool slot2 = (flags & STORY_FLAG_SLOT2) != 0;
        byte attr = row_attr[x];
        TTF_Font* run_font = slot2
            ? sdl_story_font_for_view_slot(d, STORY_FONT_SLOT_SECONDARY) : font;

        if (!run_font)
            run_font = font;

        int run_start = x;

        if (!use_story)
        {
            while (x < wid)
            {
                if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
                    break;
                byte f = story_row[x];
                if ((f & STORY_FLAG_USE) != 0)
                    break;
                if (row_attr[x] != attr)
                    break;
                x++;
            }

            int run_len = x - run_start;
            SDL_Color col = sdl_color_from_attr(sdl_ui_text_fg_attr(attr));
            sdl_fill_cell_span_with_attr(d, run_start, y, run_len, attr);
            if (utf8_has_non_ascii_n(row_chars + run_start, run_len))
            {
                const char* font_path = config.monospace_font[0] != '\0'
                    ? config.monospace_font
                    : "lib/xtra/font/VictorMono-Medium.ttf";
                TTF_Font* mono_font = sdl_acquire_mono_font_cells(font_path,
                    d->cell_w, d->cell_h);

                if (mono_font)
                    sdl_render_mono_utf8_text_cells(d->font_atlas,
                        (d->font_atlas_cell_w > 0) ? d->font_atlas_cell_w : d->cell_w,
                        (d->font_atlas_cell_h > 0) ? d->font_atlas_cell_h : d->cell_h,
                        mono_font, (float)d->cell_w,
                        (float)d->cell_h, 0.0f, run_start, y, run_len,
                        row_chars + run_start, col);
                else
                    sdl_render_mono_text(d, run_start, y, run_len,
                        row_chars + run_start, col);
            }
            else
                sdl_render_mono_text(d, run_start, y, run_len, row_chars + run_start, col);
            continue;
        }

        if (grid_align)
        {
            while (x < wid)
            {
                if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
                    break;
                byte f = story_row[x];
                if ((f & STORY_FLAG_USE) == 0)
                    break;
                if ((f & STORY_FLAG_CELL_ALIGN) == 0)
                    break;
                if (((f & STORY_FLAG_SLOT2) != 0) != slot2)
                    break;
                if (row_attr[x] != attr)
                    break;
                x++;
            }

            int run_len = x - run_start;
            SDL_Color col = sdl_color_from_attr(sdl_ui_text_fg_attr(attr));
            sdl_fill_cell_span_with_attr(d, run_start, y, run_len, attr);
            sdl_render_story_text_grid(d, run_font, run_start, y, run_len, row_chars + run_start, col);
            continue;
        }

        /* Free story region: pack segments by measured pixel width (including spaces). */
        while (x < wid)
        {
            if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
                break;
            byte f = story_row[x];
            if ((f & STORY_FLAG_USE) == 0)
                break;
            if ((f & STORY_FLAG_CELL_ALIGN) != 0)
                break;
            if (((f & STORY_FLAG_SLOT2) != 0) != slot2)
                break;
            x++;
        }

        int region_start = run_start;
        int region_end = x;
        if (region_end <= region_start)
            continue;

        int bg = region_start;
        while (bg < region_end)
        {
            byte bg_attr = row_attr[bg];
            int bg_end = bg + 1;

            while (bg_end < region_end && row_attr[bg_end] == bg_attr)
                bg_end++;

            sdl_fill_cell_span_with_attr(d, bg, y, bg_end - bg, bg_attr);
            bg = bg_end;
        }

        float px_cursor = region_start * cell_w_f;
        float px_end = region_end * cell_w_f;

        int seg = region_start;
        while (seg < region_end)
        {
            if (!sdl_story_cell_is_text(row_attr[seg], row_chars[seg]))
            {
                seg++;
                continue;
            }

            byte seg_attr = row_attr[seg];
            int seg_end = seg + 1;
            while (seg_end < region_end && sdl_story_cell_is_text(row_attr[seg_end], row_chars[seg_end])
                && row_attr[seg_end] == seg_attr)
            {
                seg_end++;
            }

            int seg_len = seg_end - seg;
            SDL_Color seg_col =
                sdl_color_from_attr(sdl_ui_text_fg_attr(seg_attr));

            float remaining = px_end - px_cursor;
            if (remaining <= 0.0f)
                break;

            int consumed = sdl_render_story_text_free_px(d, run_font, px_cursor, y, row_chars + seg, seg_len, seg_col,
                remaining);
            if (consumed <= 0)
                break;

            px_cursor += (float)consumed;
            seg = seg_end;
        }
    }
}

void sdl_render_story_text_grid(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col)
{
    if (!d || !font || !s || n <= 0)
        return;

    if (utf8_has_non_ascii_n(s, n))
    {
        sdl_render_story_text_free(d, font, x, y, n, s, col);
        return;
    }

    float cell_w_f = (float)d->cell_w;
    float cell_h_f = (float)d->cell_h;

    for (int i = 0; i < n; i++) {
        Uint32 ch = (unsigned char)s[i];
        if (!ch || ch == ' ')
            continue;

        SDL_Surface* glyph_surface = TTF_RenderGlyph_Blended(font, ch, col);
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
