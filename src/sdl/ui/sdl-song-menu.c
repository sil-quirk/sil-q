/*
 * Song selection overlay: a small centered panel (matching the description
 * overlay chrome) listing the player's songs of power.  The game side fills
 * it from do_cmd_change_song via sdl_song_menu_begin/add_entry/finish and
 * keeps running its own inkey loop; pointer input is fed back through the
 * ui_menu_click pending-choice mechanism, mirroring the unified look
 * sidebar.
 */

#include "angband.h"
#include "sdl/main-sdl-private.h"

typedef struct sdl_song_menu_layout_info {
    SDL_FRect panel;
    SDL_FRect title_row;
    SDL_FRect rows[SDL_SONG_MENU_MAX_ENTRIES];
    float divider_y;
    float letter_w;
    float letter_gap;
    int font_px;
    bool has_title;
    bool has_divider;
} sdl_song_menu_layout_info;

static void sdl_song_menu_draw_text(TTF_Font* font, cptr text,
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

static float sdl_song_menu_text_width(TTF_Font* font, cptr text,
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

static bool sdl_song_menu_layout(sdl_song_menu_layout_info* out)
{
    SDL_Rect anchor;
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
    float panel_w;
    float panel_h;
    float max_panel_w;
    float max_panel_h;
    float rows_top;

    if (!out)
        return false;
    *out = (sdl_song_menu_layout_info){ 0 };

    if (!g_song_menu.active || g_song_menu.count <= 0)
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

    for (int i = 0; i < g_song_menu.count; i++)
    {
        const sdl_song_menu_entry_state* entry = &g_song_menu.entries[i];
        float w = sdl_song_menu_text_width(story_font, entry->text,
            font_px);

        if (w > text_w)
            text_w = w;
        if (entry->letter[0])
        {
            w = mono_font
                ? sdl_song_menu_text_width(mono_font, entry->letter,
                      font_px)
                : (float)strlen(entry->letter) * (float)font_px * 0.5f;
            if (w > letter_w)
                letter_w = w;
        }
    }

    out->has_title = (g_song_menu.title[0] != '\0');
    if (out->has_title)
    {
        float w = sdl_song_menu_text_width(story_font, g_song_menu.title,
            font_px);

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
    panel_w = content_w + pad_x * 2.0f;
    if (panel_w < (float)font_px * 9.0f)
        panel_w = (float)font_px * 9.0f;
    max_panel_w = (float)anchor.w - (float)margin * 2.0f;
    if (max_panel_w < 1.0f)
        max_panel_w = (float)anchor.w;
    if (panel_w > max_panel_w)
        panel_w = max_panel_w;

    panel_h = pad_y * 2.0f + row_h * (float)g_song_menu.count;
    if (out->has_title)
        panel_h += row_h + divider_gap;
    max_panel_h = (float)anchor.h - (float)margin * 2.0f;
    if (max_panel_h < 1.0f)
        max_panel_h = (float)anchor.h;
    if (panel_h > max_panel_h)
        panel_h = max_panel_h;

    out->panel = (SDL_FRect){
        .x = (float)anchor.x + ((float)anchor.w - panel_w) * 0.5f,
        .y = (float)anchor.y + ((float)anchor.h - panel_h) * 0.5f,
        .w = panel_w,
        .h = panel_h,
    };
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

    for (int i = 0; i < g_song_menu.count; i++)
    {
        out->rows[i] = (SDL_FRect){
            .x = out->panel.x + pad_x,
            .y = rows_top + (float)i * row_h,
            .w = out->panel.w - pad_x * 2.0f,
            .h = row_h,
        };
    }

    return true;
}

void sdl_song_menu_clear(void)
{
    if (g_song_menu.active || g_song_menu.count > 0)
        g_state.need_present = true;

    memset(&g_song_menu, 0, sizeof(g_song_menu));
    g_song_menu.highlight = -1;
}

void sdl_song_menu_begin(cptr title)
{
    memset(&g_song_menu, 0, sizeof(g_song_menu));
    g_song_menu.active = true;
    g_song_menu.highlight = -1;
    if (title)
        SDL_strlcpy(g_song_menu.title, title, sizeof(g_song_menu.title));
    g_state.need_present = true;
}

void sdl_song_menu_add_entry(int choice, cptr letter, cptr text, byte attr)
{
    sdl_song_menu_entry_state* entry;

    if (!g_song_menu.active)
        return;
    if (g_song_menu.count >= SDL_SONG_MENU_MAX_ENTRIES)
        return;
    if (!text || !text[0])
        return;

    entry = &g_song_menu.entries[g_song_menu.count++];
    memset(entry, 0, sizeof(*entry));
    entry->choice = choice;
    entry->text_attr = attr;
    SDL_strlcpy(entry->letter, letter ? letter : "", sizeof(entry->letter));
    SDL_strlcpy(entry->text, text, sizeof(entry->text));
    g_state.need_present = true;
}

/* Information-only line: rendered like an entry but never hit-tested. */
void sdl_song_menu_add_text(cptr text, byte attr)
{
    sdl_song_menu_add_entry(-1, "", text, attr);
}

void sdl_song_menu_set_highlight(int choice)
{
    if (!g_song_menu.active)
        return;
    if (g_song_menu.highlight == choice)
        return;

    g_song_menu.highlight = choice;
    g_state.need_present = true;
}

void sdl_song_menu_finish(void)
{
    if (!g_song_menu.active || g_song_menu.count <= 0)
    {
        sdl_song_menu_clear();
        return;
    }

    g_state.need_present = true;
}

void sdl_song_menu_render(void)
{
    sdl_song_menu_layout_info layout;
    SDL_FRect shadow;
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    TTF_Font* story_font;
    TTF_Font* mono_font;
    SDL_Rect clip;
    int hover_choice = 0;
    bool has_hover_choice;

    if (!sdl_song_menu_layout(&layout))
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
        sdl_song_menu_draw_text(story_font, g_song_menu.title,
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

    for (int i = 0; i < g_song_menu.count; i++)
    {
        const sdl_song_menu_entry_state* entry = &g_song_menu.entries[i];
        SDL_FRect row = layout.rows[i];
        bool selectable = (entry->choice >= 0);
        bool selected = selectable && entry->choice == g_song_menu.highlight;
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
            sdl_song_menu_draw_text(mono_font, entry->letter, letter_color,
                row.x, row.y, layout.letter_w, row.h, false);
        }
        sdl_song_menu_draw_text(story_font, entry->text, text_color,
            text_x, row.y, text_w, row.h, false);
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
}

static bool sdl_song_menu_choice_at(float x, float y, int* out_choice,
    bool* out_in_panel)
{
    sdl_song_menu_layout_info layout;

    if (out_choice)
        *out_choice = -1;
    if (out_in_panel)
        *out_in_panel = false;

    if (!sdl_song_menu_layout(&layout))
        return false;
    if (!sdl_point_in_frect(&layout.panel, x, y))
        return false;

    if (out_in_panel)
        *out_in_panel = true;

    for (int i = 0; i < g_song_menu.count; i++)
    {
        if (g_song_menu.entries[i].choice < 0)
            continue;
        if (sdl_point_in_frect(&layout.rows[i], x, y))
        {
            if (out_choice)
                *out_choice = g_song_menu.entries[i].choice;
            return true;
        }
    }

    return false;
}

bool sdl_song_menu_handle_pointer(float x, float y, int action)
{
    int choice = -1;
    bool in_panel = false;
    bool wake = false;

    if (!g_song_menu.active)
        return false;
    if (!sdl_song_menu_choice_at(x, y, &choice, &in_panel))
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

bool sdl_song_menu_handle_hover_pointer(float x, float y)
{
    int choice = -1;
    bool in_panel = false;
    bool wake = false;

    if (!g_song_menu.active)
        return false;
    if (!sdl_song_menu_choice_at(x, y, &choice, &in_panel))
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
