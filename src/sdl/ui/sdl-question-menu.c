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
    SDL_FRect close_rect;
    SDL_FRect rows[SDL_QUESTION_MENU_MAX_ENTRIES];
    SDL_FRect buttons[SDL_QUESTION_MENU_MAX_BUTTONS];
    float divider_y;
    float letter_w;
    float letter_gap;
    float row_h;
    int font_px;
    int first_entry;
    int visible_count;
    int button_count;
    bool has_title;
    bool has_desc;
    bool has_divider;
    bool close_button;
} sdl_question_menu_layout_info;

typedef struct sdl_question_menu_touch_state {
    bool active;
    bool dragged;
    bool close_pressed;
    SDL_FingerID finger_id;
    int choice;
    float start_x;
    float start_y;
    float last_y;
    float accum_y;
} sdl_question_menu_touch_state;

static sdl_question_menu_touch_state g_question_menu_touch;
static bool g_question_menu_touch_scrolled = false;

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
    SDL_Texture* texture;
    SDL_FRect src;
    SDL_FRect dst;
    float scale = 1.0f;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || max_w <= 0.0f || row_h <= 0.0f)
        return;

    texture = sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture)
        return;

    if (text_h > 0 && (float)text_h > row_h * 0.94f)
        scale = (row_h * 0.94f) / (float)text_h;

    src = (SDL_FRect){
        .x = 0.0f,
        .y = 0.0f,
        .w = (float)text_w,
        .h = (float)text_h,
    };
    dst = (SDL_FRect){
        .x = x,
        .y = y,
        .w = (float)text_w * scale,
        .h = (float)text_h * scale,
    };

    if (dst.w > max_w)
    {
        dst.w = max_w;
        src.w = max_w / scale;
        if (src.w > (float)text_w)
            src.w = (float)text_w;
    }
    if (center && dst.w < max_w)
        dst.x = x + (max_w - dst.w) * 0.5f;
    dst.y = y + (row_h - dst.h) * 0.5f;

    SDL_RenderTexture(g_state.renderer, texture, &src, &dst);
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
    int width = 0;
    int height = 0;
    int wrap_width;

    if (!g_question_menu.desc[0] || !font || wrap_w <= 0.0f)
        return 0.0f;

    wrap_width = MAX(1, (int)(wrap_w + 0.5f));
    if (!TTF_GetStringSizeWrapped(font, g_question_menu.desc, 0,
            wrap_width, &width, &height))
    {
        return 0.0f;
    }
    return (float)height;
}

static bool sdl_question_menu_close_button_enabled(void)
{
    return g_question_menu.active && !g_question_menu.blocking_input;
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
    float title_w = 0.0f;
    float letter_w = 0.0f;
    float letter_gap;
    float close_reserve = 0.0f;
    float button_widths[SDL_QUESTION_MENU_MAX_BUTTONS] = { 0 };
    float button_gap = 0.0f;
    float button_total_w = 0.0f;
    float button_section_h = 0.0f;
    int button_count;
    float content_w;
    float desc_h = 0.0f;
    float panel_w;
    float panel_h;
    float max_panel_w;
    float max_panel_h;
    float rows_top;
    bool anchored = false;
    bool close_button;
    bool header_row;

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
        title_w = sdl_question_menu_text_width(story_font,
            g_question_menu.title, font_px);
    }

    margin = sdl_overlay_margin_px();
    pad_x = sdl_touch_pane_clampf((float)font_px * 0.68f, 10.0f, 18.0f);
    pad_y = sdl_touch_pane_clampf((float)font_px * 0.55f, 8.0f, 16.0f);
    row_h = (float)font_px * 1.24f;
    if (row_h < (float)font_px + 4.0f)
        row_h = (float)font_px + 4.0f;
    divider_gap = sdl_touch_pane_clampf((float)font_px * 0.3f, 3.0f, 8.0f);
    button_gap = sdl_touch_pane_clampf((float)font_px * 0.42f, 6.0f, 14.0f);
    letter_gap = letter_w > 0.0f
        ? sdl_touch_pane_clampf((float)font_px * 0.38f, 5.0f, 10.0f)
        : 0.0f;
    close_button = sdl_question_menu_close_button_enabled();
    if (close_button)
    {
        close_reserve = row_h
            + sdl_touch_pane_clampf((float)font_px * 0.38f, 5.0f,
                10.0f);
    }
    header_row = out->has_title || close_button;
    button_count = g_question_menu.button_count;
    if (button_count < 0)
        button_count = 0;
    if (button_count > SDL_QUESTION_MENU_MAX_BUTTONS)
        button_count = SDL_QUESTION_MENU_MAX_BUTTONS;
    out->button_count = button_count;
    if (button_count > 0)
    {
        float button_pad_x = sdl_touch_pane_clampf(row_h * 0.58f,
            12.0f, 24.0f);

        button_section_h = row_h + divider_gap;
        for (int i = 0; i < button_count; i++)
        {
            float w = sdl_question_menu_text_width(story_font,
                g_question_menu.buttons[i].text, font_px)
                + button_pad_x * 2.0f;

            if (w < row_h * 2.1f)
                w = row_h * 2.1f;
            button_widths[i] = w;
            button_total_w += w;
            if (i + 1 < button_count)
                button_total_w += button_gap;
        }
    }

    content_w = letter_w + letter_gap + text_w;
    if (out->has_title && title_w + close_reserve > content_w)
        content_w = title_w + close_reserve;
    if (button_total_w > content_w)
        content_w = button_total_w;
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
            else if (close_button)
                reserved_h += row_h + divider_gap;
            reserved_h += button_section_h;

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
    if (header_row)
        panel_h += row_h + divider_gap;
    if (desc_h > 0.0f)
        panel_h += desc_h + divider_gap;
    panel_h += button_section_h;
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
    out->row_h = row_h;
    out->close_button = close_button;
    if (out->close_button)
    {
        float close_size = row_h;

        out->close_rect = (SDL_FRect){
            .x = out->panel.x + out->panel.w - pad_x - close_size,
            .y = out->panel.y + pad_y,
            .w = close_size,
            .h = close_size,
        };
    }

    rows_top = out->panel.y + pad_y;
    if (out->has_title)
    {
        out->title_row = (SDL_FRect){
            .x = out->panel.x + pad_x,
            .y = rows_top,
            .w = out->panel.w - pad_x * 2.0f - close_reserve,
            .h = row_h,
        };
        if (out->title_row.w < 1.0f)
            out->title_row.w = 1.0f;
        out->divider_y = rows_top + row_h + divider_gap * 0.5f;
        out->has_divider = true;
        rows_top += row_h + divider_gap;
    }
    else if (out->close_button)
    {
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
        float rows_bottom = out->panel.y + out->panel.h - pad_y
            - button_section_h;
        float rows_h = rows_bottom - rows_top;
        int visible_count = (int)((rows_h + 2.0f) / row_h);
        int highlight_index = 0;
        int first_entry = 0;
        int max_first_entry = 0;

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

        max_first_entry = g_question_menu.count - visible_count;
        if (max_first_entry < 0)
            max_first_entry = 0;

        if (g_question_menu.scroll_offset_ptr)
        {
            first_entry = *g_question_menu.scroll_offset_ptr;
            if (first_entry < 0)
                first_entry = 0;
            if (first_entry > max_first_entry)
                first_entry = max_first_entry;

            if (g_question_menu.scroll_follow_highlight)
            {
                if (highlight_index < first_entry)
                    first_entry = highlight_index;
                else if (highlight_index >= first_entry + visible_count)
                    first_entry = highlight_index - visible_count + 1;

                if (first_entry < 0)
                    first_entry = 0;
                if (first_entry > max_first_entry)
                    first_entry = max_first_entry;
            }

            *g_question_menu.scroll_offset_ptr = first_entry;
        }
        else if (g_question_menu.count > visible_count)
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

    if (button_count > 0)
    {
        float button_area_w = out->panel.w - pad_x * 2.0f;
        float y = out->panel.y + out->panel.h - pad_y - row_h;
        float total_w = button_total_w;
        float x;

        if (button_area_w < 1.0f)
            button_area_w = 1.0f;
        if (total_w > button_area_w)
        {
            float fit_w = (button_area_w
                - button_gap * (float)(button_count - 1))
                / (float)button_count;

            if (fit_w < 1.0f)
                fit_w = button_area_w / (float)button_count;
            total_w = 0.0f;
            for (int i = 0; i < button_count; i++)
            {
                button_widths[i] = fit_w;
                total_w += fit_w;
                if (i + 1 < button_count)
                    total_w += button_gap;
            }
        }

        x = out->panel.x + pad_x + (button_area_w - total_w) * 0.5f;
        if (x < out->panel.x + pad_x)
            x = out->panel.x + pad_x;

        for (int i = 0; i < button_count; i++)
        {
            out->buttons[i] = (SDL_FRect){
                .x = x,
                .y = y,
                .w = button_widths[i],
                .h = row_h,
            };
            x += button_widths[i] + button_gap;
        }
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

void sdl_question_menu_cancel_touch(void)
{
    if (g_question_menu_touch.close_pressed && g_question_menu.close_hover)
    {
        g_question_menu.close_hover = false;
        g_state.need_present = true;
    }

    g_question_menu_touch = (sdl_question_menu_touch_state){ 0 };
    g_question_menu_touch.choice = -1;
}

static void sdl_question_menu_render_close_button(
    const sdl_question_menu_layout_info* layout, bool hover)
{
    SDL_FRect rect;
    SDL_Color icon = hover ? (SDL_Color){ 125, 185, 255, 255 }
                           : (SDL_Color){ 220, 224, 232, 235 };
    SDL_Color outline = hover ? (SDL_Color){ 125, 185, 255, 220 }
                              : (SDL_Color){ 210, 216, 226, 150 };
    float stroke;
    float pad;
    int repeats;

    if (!layout || !layout->close_button)
        return;

    rect = layout->close_rect;
    if (rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, hover ? 26 : 12,
        hover ? 38 : 18, hover ? 58 : 24, hover ? 245 : 220);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, outline.r, outline.g,
        outline.b, outline.a);
    SDL_RenderRect(g_state.renderer, &rect);

    stroke = rect.w / 11.0f;
    if (stroke < 1.0f)
        stroke = 1.0f;
    if (stroke > 4.0f)
        stroke = 4.0f;
    pad = rect.w * 0.33f;
    repeats = (int)stroke;
    if (repeats < 1)
        repeats = 1;

    SDL_SetRenderDrawColor(g_state.renderer, icon.r, icon.g, icon.b, icon.a);
    for (int i = 0; i < repeats; i++)
    {
        float offset = (float)i - ((float)repeats - 1.0f) * 0.5f;

        SDL_RenderLine(g_state.renderer, rect.x + pad,
            rect.y + pad + offset, rect.x + rect.w - pad,
            rect.y + rect.h - pad + offset);
        SDL_RenderLine(g_state.renderer, rect.x + pad,
            rect.y + rect.h - pad + offset, rect.x + rect.w - pad,
            rect.y + pad + offset);
    }
}

void sdl_question_menu_clear(void)
{
    if (g_question_menu.active || g_question_menu.count > 0)
        g_state.need_present = true;

    sdl_question_menu_cancel_touch();
    g_question_menu_touch_scrolled = false;
    memset(&g_question_menu, 0, sizeof(g_question_menu));
    g_question_menu.highlight = -1;
}

void sdl_question_menu_clear_nonblocking(void)
{
    if (g_question_menu.active && g_question_menu.nonblocking)
        sdl_question_menu_clear();
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

void sdl_question_menu_add_button(int choice, cptr text, byte attr)
{
    sdl_question_menu_button_state* button;

    if (!g_question_menu.active)
        return;
    if (g_question_menu.button_count >= SDL_QUESTION_MENU_MAX_BUTTONS)
        return;
    if (!text || !text[0])
        return;

    button = &g_question_menu.buttons[g_question_menu.button_count++];
    memset(button, 0, sizeof(*button));
    button->choice = choice;
    button->text_attr = attr;
    SDL_strlcpy(button->text, text, sizeof(button->text));
    g_state.need_present = true;
}

/* Information-only line: rendered like an entry but never hit-tested. */
void sdl_question_menu_add_text(cptr text, byte attr)
{
    sdl_question_menu_add_entry(-1, "", text, attr);
}

void sdl_question_menu_set_scroll_offset_target(int* offset,
    bool follow_highlight)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.scroll_offset_ptr = offset;
    g_question_menu.scroll_follow_highlight = follow_highlight;
}

bool sdl_question_menu_take_touch_scrolled(void)
{
    bool scrolled = g_question_menu_touch_scrolled;

    g_question_menu_touch_scrolled = false;
    return scrolled;
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
        int wrap_w = MAX(1, (int)(layout.desc_rect.w + 0.5f));
        int text_w = 0;
        int text_h = 0;
        SDL_Texture* texture = sdl_ui_wrapped_text_texture(story_font,
            g_question_menu.desc, wrap_w, g_state.palette[TERM_L_WHITE],
            &text_w, &text_h);

        if (texture)
        {
            SDL_FRect src = (SDL_FRect){
                .x = 0.0f,
                .y = 0.0f,
                .w = (float)text_w,
                .h = (float)text_h,
            };
            SDL_FRect dst = (SDL_FRect){
                .x = layout.desc_rect.x,
                .y = layout.desc_rect.y,
                .w = (float)text_w,
                .h = (float)text_h,
            };

            if (dst.w > layout.desc_rect.w)
                dst.w = layout.desc_rect.w;
            if (dst.h > layout.desc_rect.h)
            {
                dst.h = layout.desc_rect.h;
                src.h = layout.desc_rect.h;
            }
            SDL_RenderTexture(g_state.renderer, texture, &src, &dst);
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

    for (int i = 0; i < layout.button_count; i++)
    {
        const sdl_question_menu_button_state* button =
            &g_question_menu.buttons[i];
        SDL_FRect rect = layout.buttons[i];
        bool hovered = has_hover_choice && hover_choice == button->choice;
        SDL_Color fill = hovered ? (SDL_Color){ 245, 245, 245, 255 }
                                 : (SDL_Color){ 116, 116, 116, 214 };
        SDL_Color border = hovered ? (SDL_Color){ 0, 0, 0, 255 }
                                   : (SDL_Color){ 28, 28, 28, 224 };
        SDL_Color text = hovered ? g_state.palette[TERM_DARK]
                                 : g_state.palette[button->text_attr];

        SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g, fill.b,
            fill.a);
        SDL_RenderFillRect(g_state.renderer, &rect);
        SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g,
            border.b, border.a);
        SDL_RenderRect(g_state.renderer, &rect);
        sdl_question_menu_draw_text(story_font, button->text, text,
            rect.x + rect.w * 0.08f, rect.y, rect.w * 0.84f, rect.h, true);
    }

    sdl_question_menu_render_close_button(&layout,
        g_question_menu.close_hover);

    SDL_SetRenderClipRect(g_state.renderer, NULL);
}

static bool sdl_question_menu_close_button_at(float x, float y)
{
    sdl_question_menu_layout_info layout;
    SDL_FRect hit;

    if (!sdl_question_menu_close_button_enabled())
        return false;

    if (!sdl_question_menu_layout(&layout) || !layout.close_button)
        return false;

    hit = layout.close_rect;
    hit.x -= hit.w * 0.75f;
    hit.y = layout.panel.y;
    hit.w = layout.panel.x + layout.panel.w - hit.x;
    hit.h = layout.close_rect.y + layout.close_rect.h - layout.panel.y;
    if (hit.h < layout.close_rect.h)
        hit.h = layout.close_rect.h;

    return sdl_point_in_frect(&hit, x, y);
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

    for (int i = 0; i < layout.button_count; i++)
    {
        if (sdl_point_in_frect(&layout.buttons[i], x, y))
        {
            if (out_choice)
                *out_choice = g_question_menu.buttons[i].choice;
            return true;
        }
    }

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

static int sdl_question_menu_scroll_max(
    const sdl_question_menu_layout_info* layout)
{
    int max_first_entry;

    if (!layout)
        return 0;

    max_first_entry = g_question_menu.count - layout->visible_count;
    return (max_first_entry > 0) ? max_first_entry : 0;
}

static bool sdl_question_menu_scroll_offset_by(
    const sdl_question_menu_layout_info* layout, int delta)
{
    int value;
    int clamped;
    int max_first_entry;

    if (!g_question_menu.scroll_offset_ptr || delta == 0)
        return false;

    max_first_entry = sdl_question_menu_scroll_max(layout);
    value = *g_question_menu.scroll_offset_ptr;
    clamped = value + delta;
    if (clamped < 0)
        clamped = 0;
    if (clamped > max_first_entry)
        clamped = max_first_entry;

    if (clamped == value)
        return false;

    *g_question_menu.scroll_offset_ptr = clamped;
    g_question_menu_touch_scrolled = true;
    g_state.need_present = true;
    return true;
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
    if (sdl_question_menu_close_button_at(x, y))
    {
        g_question_menu.close_hover = true;
        g_state.need_present = true;

        if (g_question_menu.nonblocking)
            sdl_question_menu_clear();
        else
            Term_keypress(ESCAPE);

        return true;
    }
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

bool sdl_question_menu_handle_touch_down(float x, float y,
    SDL_FingerID finger_id)
{
    int choice = -1;
    bool in_panel = false;

    if (!g_question_menu.active)
        return false;
    if (g_question_menu.blocking_input)
        return true;

    sdl_question_menu_cancel_touch();

    if (sdl_question_menu_close_button_at(x, y))
    {
        g_question_menu_touch.active = true;
        g_question_menu_touch.close_pressed = true;
        g_question_menu_touch.finger_id = finger_id;
        g_question_menu_touch.choice = -1;
        g_question_menu_touch.start_x = x;
        g_question_menu_touch.start_y = y;
        g_question_menu_touch.last_y = y;
        g_question_menu.close_hover = true;
        g_state.need_present = true;
        return true;
    }

    if (g_question_menu.nonblocking)
        return false;
    if (!sdl_question_menu_choice_at(x, y, &choice, &in_panel))
        return in_panel;

    g_question_menu_touch.active = true;
    g_question_menu_touch.finger_id = finger_id;
    g_question_menu_touch.choice = choice;
    g_question_menu_touch.start_x = x;
    g_question_menu_touch.start_y = y;
    g_question_menu_touch.last_y = y;
    g_question_menu_touch.accum_y = 0.0f;
    return true;
}

bool sdl_question_menu_handle_touch_motion(float x, float y,
    SDL_FingerID finger_id)
{
    sdl_question_menu_layout_info layout;
    float dx;
    float dy;
    float total_dy;
    float row_h;
    bool changed = false;

    if (!g_question_menu.active)
        return false;
    if (g_question_menu.blocking_input)
        return true;
    if (!g_question_menu_touch.active
        || g_question_menu_touch.finger_id != finger_id)
    {
        return true;
    }

    dx = x - g_question_menu_touch.start_x;
    if (dx < 0.0f)
        dx = -dx;
    dy = y - g_question_menu_touch.last_y;
    g_question_menu_touch.last_y = y;
    g_question_menu_touch.accum_y += dy;
    total_dy = y - g_question_menu_touch.start_y;
    if (total_dy < 0.0f)
        total_dy = -total_dy;

    if (dx > sdl_touch_swipe_threshold_px()
        || total_dy > sdl_touch_swipe_threshold_px())
    {
        g_question_menu_touch.dragged = true;
    }

    if (g_question_menu_touch.close_pressed)
    {
        bool close_hit = sdl_question_menu_close_button_at(x, y);

        if (g_question_menu.close_hover != close_hit)
        {
            g_question_menu.close_hover = close_hit;
            g_state.need_present = true;
        }
        return true;
    }

    if (!g_question_menu_touch.dragged
        || !g_question_menu.scroll_offset_ptr
        || !sdl_question_menu_layout(&layout))
    {
        return true;
    }

    row_h = layout.row_h;
    if (row_h <= 1.0f)
        row_h = 1.0f;

    while (g_question_menu_touch.accum_y >= row_h)
    {
        changed |= sdl_question_menu_scroll_offset_by(&layout, -1);
        g_question_menu_touch.accum_y -= row_h;
    }
    while (g_question_menu_touch.accum_y <= -row_h)
    {
        changed |= sdl_question_menu_scroll_offset_by(&layout, 1);
        g_question_menu_touch.accum_y += row_h;
    }

    if (changed)
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);

    return true;
}

bool sdl_question_menu_handle_touch_up(float x, float y,
    SDL_FingerID finger_id)
{
    int press_choice;
    int release_choice = -1;
    bool in_panel = false;
    bool close_pressed;
    bool dragged;
    bool wake = false;

    if (!g_question_menu_touch.active
        || g_question_menu_touch.finger_id != finger_id)
    {
        return false;
    }

    press_choice = g_question_menu_touch.choice;
    close_pressed = g_question_menu_touch.close_pressed;
    dragged = g_question_menu_touch.dragged;
    sdl_question_menu_cancel_touch();

    if (dragged)
        return true;

    if (close_pressed)
    {
        if (sdl_question_menu_close_button_at(x, y))
            Term_keypress(ESCAPE);
        return true;
    }

    if (!sdl_question_menu_choice_at(x, y, &release_choice, &in_panel)
        || release_choice != press_choice)
    {
        return true;
    }

    if (!ui_menu_click_handle_choice_action(release_choice,
            UI_MENU_CLICK_PRIMARY, &wake))
    {
        return true;
    }

    g_state.need_present = true;
    Term_keypress('\r');
    (void)wake;
    return true;
}

bool sdl_question_menu_handle_hover_pointer(float x, float y)
{
    int choice = -1;
    bool in_panel = false;
    bool wake = false;
    bool close_hit;

    if (!g_question_menu.active)
        return false;
    if (g_question_menu.blocking_input)
        return true;
    close_hit = sdl_question_menu_close_button_at(x, y);
    if (close_hit)
    {
        if (ui_menu_click_clear_hover(&wake) && wake
            && !g_question_menu.nonblocking)
        {
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
        if (!g_question_menu.close_hover)
        {
            g_question_menu.close_hover = true;
            g_state.need_present = true;
            if (!g_question_menu.nonblocking)
                Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
        return true;
    }
    if (g_question_menu.close_hover)
    {
        g_question_menu.close_hover = false;
        g_state.need_present = true;
        if (!g_question_menu.nonblocking)
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    }
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
