/*
 * Question overlay: a titled panel with an optional wrapped description
 * block and selectable answer rows.  Local questions ("Bash the door?",
 * "Step on the trap?") anchor next to the map grid they are about; global
 * questions centre on the map view, matching the description overlay
 * chrome.  The game side fills it via sdl_question_menu_begin/add_entry/
 * finish and keeps running its own inkey loop; pointer input is fed back
 * through the ui_menu_click pending-choice mechanism, mirroring the song
 * menu.
 */

#include "angband.h"
#include "sdl/main-sdl-private.h"

typedef struct sdl_question_menu_layout_info {
    SDL_FRect panel;
    SDL_FRect title_row;
    SDL_FRect desc_rect;
    SDL_FRect rows[SDL_QUESTION_MENU_MAX_ENTRIES];
    float divider_y;
    float letter_w;
    float letter_gap;
    int font_px;
    int first_entry;
    int visible_count;
    bool has_title;
    bool has_desc;
    bool has_divider;
} sdl_question_menu_layout_info;

/* Pixel rect of a map cell on the main view, or false when it is off the
 * current panel.  Shared with the yes/no prompt anchoring. */
bool sdl_map_grid_cell_rect(int y, int x, SDL_FRect* out)
{
    int cell_cols = use_bigtile ? 2 : 1;
    int term_row;
    int term_col;

    if (!character_generated || !character_dungeon || !p_ptr
        || !g_views[PANE_MAIN].term_ready)
    {
        return false;
    }
    if (!panel_contains(y, x))
        return false;

    term_row = ROW_MAP + (y - p_ptr->wy);
    term_col = COL_MAP + (x - p_ptr->wx) * cell_cols;
    return sdl_main_cell_rect(term_col, term_row, cell_cols, 1, out);
}

static void sdl_question_menu_draw_text(TTF_Font* font, cptr text,
    SDL_Color color, float x, float y, float max_w, float row_h,
    bool center)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect src;
    SDL_FRect dst;
    float scale = 1.0f;

    if (!font || !text || !text[0] || max_w <= 0.0f || row_h <= 0.0f)
        return;

    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    if (surface->h > 0 && (float)surface->h > row_h * 0.94f)
        scale = (row_h * 0.94f) / (float)surface->h;

    src = (SDL_FRect){
        .x = 0.0f,
        .y = 0.0f,
        .w = (float)surface->w,
        .h = (float)surface->h,
    };
    dst = (SDL_FRect){
        .x = x,
        .y = y,
        .w = (float)surface->w * scale,
        .h = (float)surface->h * scale,
    };

    if (dst.w > max_w)
    {
        dst.w = max_w;
        src.w = max_w / scale;
        if (src.w > (float)surface->w)
            src.w = (float)surface->w;
    }
    if (center && dst.w < max_w)
        dst.x = x + (max_w - dst.w) * 0.5f;
    dst.y = y + (row_h - dst.h) * 0.5f;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, &src, &dst);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static float sdl_question_menu_text_width(TTF_Font* font, cptr text,
    int font_px)
{
    int width;

    if (!text || !text[0])
        return 0.0f;

    width = sdl_touch_pane_story_text_width(font, text);
    if (width > 0)
        return (float)width;

    return (float)strlen(text) * (float)font_px * 0.55f;
}

/* Height of the wrapped description block at the given width. */
static float sdl_question_menu_desc_height(TTF_Font* font, float wrap_w)
{
    SDL_Surface* surface;
    float h;

    if (!g_question_menu.desc[0] || !font || wrap_w <= 0.0f)
        return 0.0f;

    surface = sdl_object_tooltip_render_text_surface(font,
        g_question_menu.desc, (SDL_Color){ 255, 255, 255, 255 }, wrap_w);
    if (!surface)
        return 0.0f;

    h = (float)surface->h;
    SDL_DestroySurface(surface);
    return h;
}

static bool sdl_question_menu_layout(sdl_question_menu_layout_info* out)
{
    SDL_Rect anchor;
    SDL_FRect anchor_cell;
    TTF_Font* story_font;
    TTF_Font* mono_font;
    int font_px;
    int margin;
    float pad_x;
    float pad_y;
    float row_h;
    float divider_gap;
    float text_w = 0.0f;
    float letter_w = 0.0f;
    float letter_gap;
    float content_w;
    float desc_h = 0.0f;
    float panel_w;
    float panel_h;
    float max_panel_w;
    float max_panel_h;
    float rows_top;
    bool anchored = false;

    if (!out)
        return false;
    *out = (sdl_question_menu_layout_info){ 0 };

    if (!g_question_menu.active || g_question_menu.count <= 0)
        return false;
    if (!sdl_overlay_pane_anchor_rect(PANE_DESCRIPTION, &anchor))
        return false;

    font_px = sdl_main_menu_pane_font_px();
#if SIL_SDL_MOBILE_BUILD
    font_px = (int)((float)font_px * 1.18f + 0.5f);
#endif
    story_font = sdl_story_font_for_height_slot(font_px,
        SDL_STORY_FONT_SLOT_MENU);
    if (!story_font)
        return false;
    mono_font = sdl_main_menu_mono_font_for_height(font_px);

    for (int i = 0; i < g_question_menu.count; i++)
    {
        const sdl_question_menu_entry_state* entry
            = &g_question_menu.entries[i];
        float w = sdl_question_menu_text_width(story_font, entry->text,
            font_px);

        if (w > text_w)
            text_w = w;
        if (entry->letter[0])
        {
            w = mono_font
                ? sdl_question_menu_text_width(mono_font, entry->letter,
                      font_px)
                : (float)strlen(entry->letter) * (float)font_px * 0.5f;
            if (w > letter_w)
                letter_w = w;
        }
    }

    out->has_title = (g_question_menu.title[0] != '\0');
    if (out->has_title)
    {
        float w = sdl_question_menu_text_width(story_font,
            g_question_menu.title, font_px);

        if (w > text_w + letter_w)
            text_w = w - letter_w;
    }

    margin = sdl_overlay_margin_px();
    pad_x = sdl_touch_pane_clampf((float)font_px * 0.68f, 10.0f, 18.0f);
    pad_y = sdl_touch_pane_clampf((float)font_px * 0.55f, 8.0f, 16.0f);
    row_h = (float)font_px * 1.24f;
    if (row_h < (float)font_px + 4.0f)
        row_h = (float)font_px + 4.0f;
    divider_gap = sdl_touch_pane_clampf((float)font_px * 0.3f, 3.0f, 8.0f);
    letter_gap = letter_w > 0.0f
        ? sdl_touch_pane_clampf((float)font_px * 0.38f, 5.0f, 10.0f)
        : 0.0f;

    content_w = letter_w + letter_gap + text_w;
    out->has_desc = (g_question_menu.desc[0] != '\0');
    if (out->has_desc)
    {
        /* Give descriptions a comfortable column without letting a short
         * option list force narrow wrapping. */
        float desc_w = sdl_question_menu_text_width(story_font,
            g_question_menu.desc, font_px);
        float desc_max = (float)font_px * 19.0f;

        if (desc_w > desc_max)
            desc_w = desc_max;
        if (desc_w > content_w)
            content_w = desc_w;
    }

    panel_w = content_w + pad_x * 2.0f;
    if (panel_w < (float)font_px * 9.0f)
        panel_w = (float)font_px * 9.0f;
    max_panel_w = (float)anchor.w - (float)margin * 2.0f;
    if (max_panel_w < 1.0f)
        max_panel_w = (float)anchor.w;
    if (panel_w > max_panel_w)
        panel_w = max_panel_w;

    max_panel_h = (float)anchor.h - (float)margin * 2.0f;
    if (max_panel_h < 1.0f)
        max_panel_h = (float)anchor.h;

    if (out->has_desc)
    {
        desc_h = sdl_question_menu_desc_height(story_font,
            panel_w - pad_x * 2.0f);

        if (desc_h > 0.0f)
        {
            int reserved_rows = g_question_menu.count;
            float reserved_h;

            if (reserved_rows > 3)
                reserved_rows = 3;

            reserved_h = pad_y * 2.0f
                + row_h * (float)reserved_rows
                + divider_gap
                + 4.0f;
            if (out->has_title)
                reserved_h += row_h + divider_gap;

            if (max_panel_h > reserved_h)
            {
                float max_desc_h = max_panel_h - reserved_h;

                if (desc_h > max_desc_h)
                    desc_h = max_desc_h;
            }
            else
            {
                desc_h = 0.0f;
            }
        }
    }

    panel_h = pad_y * 2.0f + row_h * (float)g_question_menu.count;
    if (out->has_title)
        panel_h += row_h + divider_gap;
    if (desc_h > 0.0f)
        panel_h += desc_h + divider_gap;
    if (panel_h > max_panel_h)
        panel_h = max_panel_h;

    /* Local questions sit next to the grid they are about; fall back to the
     * centred placement when the grid is off the visible map. */
    if (g_question_menu.has_anchor
        && sdl_map_grid_cell_rect(g_question_menu.anchor_y,
            g_question_menu.anchor_x, &anchor_cell))
    {
        const sdl_view* view = &g_views[PANE_MAIN];
        float view_x = (float)(view->rect.x + view->margin_x);
        float view_y = (float)(view->rect.y + view->margin_y);
        float view_w = (float)(sdl_main_view_visual_cols(view) * view->cell_w);
        float view_h = (float)(sdl_main_view_visual_rows(view) * view->cell_h);
        float gap = sdl_touch_pane_clampf(anchor_cell.h * 0.45f, 6.0f, 16.0f);

        /* Prefer below the cell, then above; centre horizontally on it. */
        out->panel.x = anchor_cell.x + anchor_cell.w * 0.5f - panel_w * 0.5f;
        out->panel.y = anchor_cell.y + anchor_cell.h + gap;
        if (out->panel.y + panel_h > view_y + view_h - gap)
            out->panel.y = anchor_cell.y - panel_h - gap;

        if (out->panel.x < view_x + gap)
            out->panel.x = view_x + gap;
        if (out->panel.x + panel_w > view_x + view_w - gap)
            out->panel.x = view_x + view_w - gap - panel_w;
        if (out->panel.y < view_y + gap)
            out->panel.y = view_y + gap;
        if (out->panel.y + panel_h > view_y + view_h - gap)
            out->panel.y = view_y + view_h - gap - panel_h;

        out->panel.w = panel_w;
        out->panel.h = panel_h;
        anchored = true;
    }

    if (!anchored)
    {
        out->panel = (SDL_FRect){
            .x = (float)anchor.x + ((float)anchor.w - panel_w) * 0.5f,
            .y = (float)anchor.y + ((float)anchor.h - panel_h) * 0.5f,
            .w = panel_w,
            .h = panel_h,
        };
    }

    out->font_px = font_px;
    out->letter_w = letter_w;
    out->letter_gap = letter_gap;

    rows_top = out->panel.y + pad_y;
    if (out->has_title)
    {
        out->title_row = (SDL_FRect){
            .x = out->panel.x + pad_x,
            .y = rows_top,
            .w = out->panel.w - pad_x * 2.0f,
            .h = row_h,
        };
        out->divider_y = rows_top + row_h + divider_gap * 0.5f;
        out->has_divider = true;
        rows_top += row_h + divider_gap;
    }

    if (desc_h > 0.0f)
    {
        out->desc_rect = (SDL_FRect){
            .x = out->panel.x + pad_x,
            .y = rows_top,
            .w = out->panel.w - pad_x * 2.0f,
            .h = desc_h,
        };
        rows_top += desc_h + divider_gap;
    }
    else
    {
        out->has_desc = false;
    }

    {
        float rows_bottom = out->panel.y + out->panel.h - pad_y;
        float rows_h = rows_bottom - rows_top;
        int visible_count = (int)((rows_h + 2.0f) / row_h);
        int highlight_index = 0;
        int first_entry = 0;

        if (visible_count < 1)
            visible_count = 1;
        if (visible_count > g_question_menu.count)
            visible_count = g_question_menu.count;

        for (int i = 0; i < g_question_menu.count; i++)
        {
            if (g_question_menu.entries[i].choice == g_question_menu.highlight)
            {
                highlight_index = i;
                break;
            }
        }

        if (g_question_menu.count > visible_count)
        {
            first_entry = highlight_index - visible_count / 2;
            if (first_entry < 0)
                first_entry = 0;
            if (first_entry + visible_count > g_question_menu.count)
                first_entry = g_question_menu.count - visible_count;
        }

        out->first_entry = first_entry;
        out->visible_count = visible_count;
    }

    for (int i = 0; i < g_question_menu.count; i++)
    {
        out->rows[i] = (SDL_FRect){
            .x = out->panel.x + pad_x,
            .y = rows_top + (float)(i - out->first_entry) * row_h,
            .w = out->panel.w - pad_x * 2.0f,
            .h = row_h,
        };
    }

    return true;
}

void sdl_question_menu_clear(void)
{
    if (g_question_menu.active || g_question_menu.count > 0)
        g_state.need_present = true;

    memset(&g_question_menu, 0, sizeof(g_question_menu));
    g_question_menu.highlight = -1;
}

void sdl_question_menu_begin(cptr title)
{
    memset(&g_question_menu, 0, sizeof(g_question_menu));
    g_question_menu.active = true;
    g_question_menu.highlight = -1;
    if (title)
        SDL_strlcpy(g_question_menu.title, title,
            sizeof(g_question_menu.title));
    g_state.need_present = true;
}

void sdl_question_menu_set_anchor_grid(int y, int x)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.has_anchor = true;
    g_question_menu.anchor_y = y;
    g_question_menu.anchor_x = x;
    g_state.need_present = true;
}

void sdl_question_menu_set_desc(cptr text)
{
    if (!g_question_menu.active)
        return;

    SDL_strlcpy(g_question_menu.desc, text ? text : "",
        sizeof(g_question_menu.desc));
    g_state.need_present = true;
}

void sdl_question_menu_add_entry(int choice, cptr letter, cptr text,
    byte attr)
{
    sdl_question_menu_entry_state* entry;

    if (!g_question_menu.active)
        return;
    if (g_question_menu.count >= SDL_QUESTION_MENU_MAX_ENTRIES)
        return;
    if (!text || !text[0])
        return;

    entry = &g_question_menu.entries[g_question_menu.count++];
    memset(entry, 0, sizeof(*entry));
    entry->choice = choice;
    entry->text_attr = attr;
    SDL_strlcpy(entry->letter, letter ? letter : "", sizeof(entry->letter));
    SDL_strlcpy(entry->text, text, sizeof(entry->text));
    g_state.need_present = true;
}

/* Information-only line: rendered like an entry but never hit-tested. */
void sdl_question_menu_add_text(cptr text, byte attr)
{
    sdl_question_menu_add_entry(-1, "", text, attr);
}

void sdl_question_menu_set_highlight(int choice)
{
    if (!g_question_menu.active)
        return;
    if (g_question_menu.highlight == choice)
        return;

    g_question_menu.highlight = choice;
    g_state.need_present = true;
}

void sdl_question_menu_finish(void)
{
    if (!g_question_menu.active || g_question_menu.count <= 0)
    {
        sdl_question_menu_clear();
        return;
    }

    g_state.need_present = true;
}

void sdl_question_menu_set_blocking_input(bool blocking)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.blocking_input = blocking;
}

bool sdl_question_menu_blocks_input(void)
{
    return g_question_menu.active && g_question_menu.blocking_input;
}

/*
 * True only for an interactive overlay such as the in-menu value picker.  A
 * blocking_input popup swallows every event (handled separately) and a
 * nonblocking one deliberately lets input pass through, so neither should
 * make the overlay capture pointer/touch input.
 */
bool sdl_question_menu_captures_pointer(void)
{
    return g_question_menu.active
        && !g_question_menu.blocking_input
        && !g_question_menu.nonblocking;
}

void sdl_question_menu_set_nonblocking(bool nonblocking)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.nonblocking = nonblocking;
}

void sdl_question_menu_set_timeout_ms(int ms)
{
    if (!g_question_menu.active)
        return;

    if (ms <= 0)
    {
        g_question_menu.expires_at_ns = 0;
        sdl_question_menu_clear();
        return;
    }

    g_question_menu.expires_at_ns =
        SDL_GetTicksNS() + (Uint64)ms * 1000000ULL;
}

int sdl_question_menu_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 remaining_ns;

    if (!g_question_menu.active || !g_question_menu.expires_at_ns)
        return -1;
    if (now_ns >= g_question_menu.expires_at_ns)
        return 0;

    remaining_ns = g_question_menu.expires_at_ns - now_ns;
    return (int)((remaining_ns + 999999ULL) / 1000000ULL);
}

bool sdl_question_menu_flush_expired(Uint64 now_ns)
{
    if (!g_question_menu.active || !g_question_menu.expires_at_ns)
        return false;
    if (now_ns < g_question_menu.expires_at_ns)
        return false;

    sdl_question_menu_clear();
    return true;
}

void sdl_question_menu_render(void)
{
    sdl_question_menu_layout_info layout;
    SDL_FRect shadow;
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    TTF_Font* story_font;
    TTF_Font* mono_font;
    SDL_Rect clip;
    int hover_choice = 0;
    bool has_hover_choice;

    if (sdl_question_menu_flush_expired(SDL_GetTicksNS()))
        return;

    if (!sdl_question_menu_layout(&layout))
        return;

    story_font = sdl_story_font_for_height_slot(layout.font_px,
        SDL_STORY_FONT_SLOT_MENU);
    if (!story_font)
        return;
    mono_font = sdl_main_menu_mono_font_for_height(layout.font_px);

    has_hover_choice = ui_menu_click_get_hover_choice(&hover_choice);

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    shadow = layout.panel;
    shadow.x += 3.0f;
    shadow.y += 3.0f;
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 118);
    SDL_RenderFillRect(g_state.renderer, &shadow);

    /* Match the description overlay chrome. */
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 232);
    SDL_RenderFillRect(g_state.renderer, &layout.panel);
    SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 120);
    SDL_RenderRect(g_state.renderer, &layout.panel);

    clip = (SDL_Rect){
        .x = (int)layout.panel.x,
        .y = (int)layout.panel.y,
        .w = (int)(layout.panel.w + 1.0f),
        .h = (int)(layout.panel.h + 1.0f),
    };
    SDL_SetRenderClipRect(g_state.renderer, &clip);

    if (layout.has_title)
    {
        sdl_question_menu_draw_text(story_font, g_question_menu.title,
            g_state.palette[TERM_WHITE], layout.title_row.x,
            layout.title_row.y, layout.title_row.w, layout.title_row.h,
            true);
    }
    if (layout.has_divider)
    {
        SDL_FRect divider = (SDL_FRect){
            .x = layout.panel.x,
            .y = layout.divider_y,
            .w = layout.panel.w,
            .h = 1.0f,
        };

        SDL_SetRenderDrawColor(g_state.renderer, 95, 105, 112, 104);
        SDL_RenderFillRect(g_state.renderer, &divider);
    }

    if (layout.has_desc)
    {
        SDL_Surface* surface = sdl_object_tooltip_render_text_surface(
            story_font, g_question_menu.desc,
            g_state.palette[TERM_L_WHITE], layout.desc_rect.w);

        if (surface)
        {
            SDL_Texture* texture
                = SDL_CreateTextureFromSurface(g_state.renderer, surface);

            if (texture)
            {
                SDL_FRect src = (SDL_FRect){
                    .x = 0.0f,
                    .y = 0.0f,
                    .w = (float)surface->w,
                    .h = (float)surface->h,
                };
                SDL_FRect dst = (SDL_FRect){
                    .x = layout.desc_rect.x,
                    .y = layout.desc_rect.y,
                    .w = (float)surface->w,
                    .h = (float)surface->h,
                };

                if (dst.w > layout.desc_rect.w)
                    dst.w = layout.desc_rect.w;
                if (dst.h > layout.desc_rect.h)
                {
                    dst.h = layout.desc_rect.h;
                    src.h = layout.desc_rect.h;
                }
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                SDL_RenderTexture(g_state.renderer, texture, &src, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_DestroySurface(surface);
        }
    }

    for (int i = layout.first_entry;
         i < g_question_menu.count
             && i < layout.first_entry + layout.visible_count;
         i++)
    {
        const sdl_question_menu_entry_state* entry
            = &g_question_menu.entries[i];
        SDL_FRect row = layout.rows[i];
        bool selectable = (entry->choice >= 0);
        bool selected
            = selectable && entry->choice == g_question_menu.highlight;
        bool hovered = selectable && has_hover_choice
            && hover_choice == entry->choice;
        SDL_Color letter_color = g_state.palette[
            selected || hovered ? TERM_L_BLUE : TERM_SLATE];
        SDL_Color text_color = selected || hovered
            ? accent
            : g_state.palette[entry->text_attr];
        float text_x = row.x + layout.letter_w + layout.letter_gap;
        float text_w = row.w - layout.letter_w - layout.letter_gap;

        if (selected || hovered)
        {
            SDL_SetRenderDrawColor(g_state.renderer, 36, 47, 62,
                selected ? 226 : 184);
            SDL_RenderFillRect(g_state.renderer, &row);
            SDL_SetRenderDrawColor(g_state.renderer, accent.r, accent.g,
                accent.b, selected ? 225 : 168);
            SDL_RenderRect(g_state.renderer, &row);
        }

        if (entry->letter[0] && layout.letter_w > 0.0f && mono_font)
        {
            sdl_question_menu_draw_text(mono_font, entry->letter,
                letter_color, row.x, row.y, layout.letter_w, row.h, false);
        }
        sdl_question_menu_draw_text(story_font, entry->text, text_color,
            text_x, row.y, text_w, row.h, false);
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
}

static bool sdl_question_menu_choice_at(float x, float y, int* out_choice,
    bool* out_in_panel)
{
    sdl_question_menu_layout_info layout;

    if (out_choice)
        *out_choice = -1;
    if (out_in_panel)
        *out_in_panel = false;

    if (!sdl_question_menu_layout(&layout))
        return false;
    if (!sdl_point_in_frect(&layout.panel, x, y))
        return false;

    if (out_in_panel)
        *out_in_panel = true;

    for (int i = layout.first_entry;
         i < g_question_menu.count
             && i < layout.first_entry + layout.visible_count;
         i++)
    {
        if (g_question_menu.entries[i].choice < 0)
            continue;
        if (sdl_point_in_frect(&layout.rows[i], x, y))
        {
            if (out_choice)
                *out_choice = g_question_menu.entries[i].choice;
            return true;
        }
    }

    return false;
}

bool sdl_question_menu_handle_pointer(float x, float y, int action)
{
    int choice = -1;
    bool in_panel = false;
    bool wake = false;

    if (!g_question_menu.active)
        return false;
    if (g_question_menu.blocking_input)
        return true;
    if (g_question_menu.nonblocking)
        return false;
    if (!sdl_question_menu_choice_at(x, y, &choice, &in_panel))
        return in_panel;

    if (!ui_menu_click_handle_choice_action(choice, action, &wake))
        return true;

    g_state.need_present = true;
    Term_keypress((action == UI_MENU_CLICK_SECONDARY)
        ? UI_MENU_CLICK_WAKE_KEY
        : '\r');
    (void)wake;
    return true;
}

bool sdl_question_menu_handle_hover_pointer(float x, float y)
{
    int choice = -1;
    bool in_panel = false;
    bool wake = false;

    if (!g_question_menu.active)
        return false;
    if (g_question_menu.blocking_input)
        return true;
    if (g_question_menu.nonblocking)
        return false;
    if (!sdl_question_menu_choice_at(x, y, &choice, &in_panel))
    {
        if (in_panel)
        {
            if (ui_menu_click_clear_hover(&wake) && wake)
                Term_keypress(UI_MENU_CLICK_WAKE_KEY);
            return true;
        }
        return false;
    }

    if (!ui_menu_click_handle_choice_action(choice, UI_MENU_CLICK_HOVER,
            &wake))
    {
        return true;
    }

    g_state.need_present = true;
    if (wake)
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}
