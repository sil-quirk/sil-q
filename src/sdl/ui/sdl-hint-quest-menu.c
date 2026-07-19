/*
 * Native Hints & Quests surface.  The game side supplies semantic blocks and
 * actions; this file owns all presentation geometry, text measurement,
 * wrapping, scrolling and pointer hit-testing in renderer pixels.
 */

#include "angband.h"
#include "sdl/main-sdl-private.h"

enum {
    SDL_HINT_QUEST_MAX_BLOCKS = 384,
    SDL_HINT_QUEST_MAX_BUTTONS = 5,
    SDL_HINT_QUEST_MAX_HITS = 512,
    SDL_HINT_QUEST_TEXT_LEN = 768,
    SDL_HINT_QUEST_LABEL_LEN = 48
};

typedef struct sdl_hint_quest_block {
    char text[SDL_HINT_QUEST_TEXT_LEN];
    byte attr;
    int indent;
    int choice;
} sdl_hint_quest_block;

typedef struct sdl_hint_quest_button {
    char label[SDL_HINT_QUEST_LABEL_LEN];
    byte attr;
    int choice;
} sdl_hint_quest_button;

typedef struct sdl_hint_quest_hit {
    SDL_FRect rect;
    int choice;
} sdl_hint_quest_hit;

typedef struct sdl_hint_quest_state {
    bool active;
    bool show_tabs;
    bool center_body;
    hint_quest_page page;
    int selected_choice;
    int hover_choice;
    int block_count;
    int button_count;
    int hit_count;
    int last_selected_choice;
    float scroll_y;
    float max_scroll_y;
    SDL_FRect body_rect;
    char title[SDL_HINT_QUEST_LABEL_LEN];
    char section[SDL_HINT_QUEST_LABEL_LEN];
    sdl_hint_quest_block blocks[SDL_HINT_QUEST_MAX_BLOCKS];
    sdl_hint_quest_button buttons[SDL_HINT_QUEST_MAX_BUTTONS];
    sdl_hint_quest_hit hits[SDL_HINT_QUEST_MAX_HITS];
} sdl_hint_quest_state;

typedef struct sdl_hint_quest_press {
    bool active;
    bool touch;
    bool dragged;
    SDL_FingerID finger_id;
    int choice;
    float start_x;
    float start_y;
    float last_y;
} sdl_hint_quest_press;

typedef struct sdl_hint_quest_layout {
    SDL_FRect panel;
    SDL_FRect title;
    SDL_FRect tabs[3];
    SDL_FRect section;
    SDL_FRect body;
    SDL_FRect footer;
    int body_px;
    int title_px;
    float line_h;
    float block_gap;
    float card_pad_x;
    float card_pad_y;
} sdl_hint_quest_layout;

static sdl_hint_quest_state g_hint_quest;
static sdl_hint_quest_press g_hint_quest_press;

static void sdl_hint_quest_mark_dirty(void)
{
    g_state.need_present = true;
}

static void sdl_hint_quest_add_hit(SDL_FRect rect, int choice)
{
    sdl_hint_quest_hit* hit;

    if (choice == 0 || g_hint_quest.hit_count >= SDL_HINT_QUEST_MAX_HITS)
        return;
    if (rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    hit = &g_hint_quest.hits[g_hint_quest.hit_count++];
    hit->rect = rect;
    hit->choice = choice;
}

static int sdl_hint_quest_choice_at(float x, float y)
{
    for (int i = g_hint_quest.hit_count - 1; i >= 0; --i)
    {
        if (sdl_point_in_frect(&g_hint_quest.hits[i].rect, x, y))
            return g_hint_quest.hits[i].choice;
    }

    return 0;
}

static bool sdl_hint_quest_intersect_rect(SDL_FRect a, SDL_FRect b,
    SDL_FRect* out)
{
    float left = MAX(a.x, b.x);
    float top = MAX(a.y, b.y);
    float right = MIN(a.x + a.w, b.x + b.w);
    float bottom = MIN(a.y + a.h, b.y + b.h);

    if (!out || right <= left || bottom <= top)
        return false;
    *out = (SDL_FRect){ left, top, right - left, bottom - top };
    return true;
}

static void sdl_hint_quest_draw_text(TTF_Font* font, cptr text,
    SDL_Color color, const SDL_FRect* rect, bool center, bool wrapped)
{
    SDL_Texture* texture;
    SDL_FRect dst;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || !rect || rect->w <= 0.0f
        || rect->h <= 0.0f)
    {
        return;
    }

    texture = wrapped
        ? sdl_ui_wrapped_text_texture(font, text,
              MAX(1, (int)(rect->w + 0.5f)), color, &text_w, &text_h)
        : sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture || text_w <= 0 || text_h <= 0)
        return;

    dst = (SDL_FRect){
        .x = rect->x,
        .y = rect->y + (rect->h - (float)text_h) * 0.5f,
        .w = (float)text_w,
        .h = (float)text_h
    };
    if (dst.w > rect->w)
        dst.w = rect->w;
    if (wrapped)
        dst.y = rect->y;
    if (center && dst.w < rect->w)
        dst.x += (rect->w - dst.w) * 0.5f;

    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
}

static int sdl_hint_quest_body_px(const SDL_Rect* screen)
{
    float short_side;
    int px;

    if (!screen)
        return 22;
    short_side = (float)MIN(screen->w, screen->h);
    px = (int)(short_side * 0.042f + 0.5f);
    if (px < 18)
        px = 18;
    if (px > 36)
        px = 36;
    return px;
}

static bool sdl_hint_quest_layout_compute(sdl_hint_quest_layout* out)
{
    SDL_Rect screen;
    bool portrait;
    float margin;
    float panel_w;
    float panel_h;
    float pad_x;
    float pad_y;
    float title_h;
    float tabs_h = 0.0f;
    float section_h;
    float footer_h = 0.0f;
    float gap;
    float cursor_y;

    if (!out || !g_hint_quest.active)
        return false;
    *out = (sdl_hint_quest_layout){ 0 };

    screen = sdl_get_layout_screen_rect();
    if (screen.w <= 0 || screen.h <= 0)
        return false;
    portrait = screen.h > screen.w;
    out->body_px = sdl_hint_quest_body_px(&screen);
    out->title_px = (int)((float)out->body_px * 1.24f + 0.5f);
    out->line_h = (float)out->body_px * 1.24f;
    out->block_gap = sdl_touch_pane_clampf((float)out->body_px * 0.34f,
        6.0f, 14.0f);
    out->card_pad_x = sdl_touch_pane_clampf((float)out->body_px * 0.58f,
        10.0f, 22.0f);
    out->card_pad_y = sdl_touch_pane_clampf((float)out->body_px * 0.32f,
        6.0f, 13.0f);

    margin = sdl_touch_pane_clampf((float)MIN(screen.w, screen.h) * 0.032f,
        12.0f, 36.0f);
    panel_w = (float)screen.w - margin * 2.0f;
    if (!portrait)
    {
        float landscape_max = (float)screen.w * 0.76f;

        if (landscape_max < 720.0f)
            landscape_max = 720.0f;
        if (panel_w > landscape_max)
            panel_w = landscape_max;
    }
    if (panel_w < 1.0f)
        panel_w = (float)screen.w;
    panel_h = (float)screen.h - margin * 2.0f;
    if (panel_h < 1.0f)
        panel_h = (float)screen.h;

    out->panel = (SDL_FRect){
        .x = (float)screen.x + ((float)screen.w - panel_w) * 0.5f,
        .y = (float)screen.y + ((float)screen.h - panel_h) * 0.5f,
        .w = panel_w,
        .h = panel_h
    };

    pad_x = sdl_touch_pane_clampf((float)out->body_px * 0.85f,
        16.0f, 34.0f);
    pad_y = sdl_touch_pane_clampf((float)out->body_px * 0.60f,
        12.0f, 24.0f);
    gap = sdl_touch_pane_clampf((float)out->body_px * 0.48f,
        8.0f, 18.0f);
    title_h = (float)out->title_px * 1.28f;
    if (g_hint_quest.show_tabs)
        tabs_h = MAX(46.0f, (float)out->body_px * 1.82f);
    section_h = g_hint_quest.section[0]
        ? (float)out->body_px * 1.42f : 0.0f;
    if (g_hint_quest.button_count > 0)
        footer_h = MAX(48.0f, (float)out->body_px * 1.90f);

    cursor_y = out->panel.y + pad_y;
    out->title = (SDL_FRect){
        .x = out->panel.x + pad_x,
        .y = cursor_y,
        .w = out->panel.w - pad_x * 2.0f,
        .h = title_h
    };
    cursor_y += title_h;

    if (g_hint_quest.show_tabs)
    {
        float tab_gap = sdl_touch_pane_clampf((float)out->body_px * 0.36f,
            6.0f, 14.0f);
        float tab_w = (out->title.w - tab_gap * 2.0f) / 3.0f;

        cursor_y += gap * 0.42f;
        for (int i = 0; i < 3; ++i)
        {
            out->tabs[i] = (SDL_FRect){
                .x = out->title.x + (tab_w + tab_gap) * (float)i,
                .y = cursor_y,
                .w = tab_w,
                .h = tabs_h
            };
        }
        cursor_y += tabs_h + gap;
    }
    else
    {
        cursor_y += gap * 0.55f;
    }

    if (section_h > 0.0f)
    {
        out->section = (SDL_FRect){
            .x = out->title.x,
            .y = cursor_y,
            .w = out->title.w,
            .h = section_h
        };
        cursor_y += section_h + gap * 0.55f;
    }

    out->footer = (SDL_FRect){
        .x = out->title.x,
        .y = out->panel.y + out->panel.h - pad_y - footer_h,
        .w = out->title.w,
        .h = footer_h
    };
    out->body = (SDL_FRect){
        .x = out->title.x,
        .y = cursor_y,
        .w = out->title.w,
        .h = out->footer.y - gap * 0.55f - cursor_y
    };
    if (footer_h <= 0.0f)
        out->body.h = out->panel.y + out->panel.h - pad_y - cursor_y;
    if (out->body.h < 1.0f)
        out->body.h = 1.0f;

    return true;
}

static float sdl_hint_quest_block_height(TTF_Font* font,
    const sdl_hint_quest_layout* layout, const sdl_hint_quest_block* block,
    float* out_text_w)
{
    float indent_px;
    float text_w;
    int measured_w = 0;
    int measured_h = 0;

    if (out_text_w)
        *out_text_w = 1.0f;
    if (!font || !layout || !block)
        return 1.0f;
    if (!block->text[0])
        return layout->line_h * 0.52f;

    indent_px = (float)MAX(0, block->indent) * (float)layout->body_px * 0.72f;
    text_w = layout->body.w - indent_px;
    if (block->choice != 0)
        text_w -= layout->card_pad_x * 2.0f;
    if (text_w < 1.0f)
        text_w = 1.0f;
    (void)sdl_ui_wrapped_text_texture(font, block->text,
        MAX(1, (int)(text_w + 0.5f)), sdl_color_from_attr(block->attr),
        &measured_w, &measured_h);
    if (measured_h < 1)
        measured_h = (int)layout->line_h;
    if (out_text_w)
        *out_text_w = text_w;

    return (float)measured_h
        + ((block->choice != 0) ? layout->card_pad_y * 2.0f : 0.0f);
}

static void sdl_hint_quest_draw_chrome(const sdl_hint_quest_layout* layout,
    TTF_Font* title_font, TTF_Font* body_font)
{
    static const char* const tab_labels[] = { "Hints", "Quests", "Thralls" };
    static const int tab_choices[] = {
        HINT_QUEST_CLICK_HINTS_TAB,
        HINT_QUEST_CLICK_QUESTS_TAB,
        HINT_QUEST_CLICK_THRALLS_TAB
    };
    SDL_Color border = sdl_color_from_attr(TERM_SLATE);
    SDL_Color title = sdl_color_from_attr(TERM_L_WHITE);
    SDL_Color gold = sdl_color_from_attr(TERM_YELLOW);

    border.a = 90;
    SDL_SetRenderDrawColor(g_state.renderer, 4, 5, 8, 248);
    SDL_RenderFillRect(g_state.renderer, &layout->panel);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
        border.a);
    SDL_RenderRect(g_state.renderer, &layout->panel);
    sdl_hint_quest_draw_text(title_font, g_hint_quest.title, title,
        &layout->title, false, false);

    if (g_hint_quest.show_tabs)
    {
        for (int i = 0; i < 3; ++i)
        {
            bool active = g_hint_quest.page == (hint_quest_page)(i + 1);
            bool hover = g_hint_quest.hover_choice == tab_choices[i];
            SDL_Color fill = active
                ? sdl_color_from_attr(TERM_YELLOW)
                : sdl_color_from_attr(TERM_L_DARK);
            SDL_Color text = active ? sdl_color_from_attr(TERM_DARK) : gold;

            fill.a = active ? 205 : (hover ? 105 : 46);
            SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g, fill.b,
                fill.a);
            SDL_RenderFillRect(g_state.renderer, &layout->tabs[i]);
            SDL_SetRenderDrawColor(g_state.renderer, gold.r, gold.g, gold.b,
                active || hover ? 210 : 95);
            SDL_RenderRect(g_state.renderer, &layout->tabs[i]);
            sdl_hint_quest_draw_text(body_font, tab_labels[i], text,
                &layout->tabs[i], true, false);
            sdl_hint_quest_add_hit(layout->tabs[i], tab_choices[i]);
        }
    }

    if (g_hint_quest.section[0])
    {
        sdl_hint_quest_draw_text(body_font, g_hint_quest.section, gold,
            &layout->section, false, false);
    }
}

static void sdl_hint_quest_draw_footer(const sdl_hint_quest_layout* layout,
    TTF_Font* font)
{
    float widths[SDL_HINT_QUEST_MAX_BUTTONS] = { 0 };
    float total = 0.0f;
    float gap;
    float x;

    if (g_hint_quest.button_count <= 0)
        return;
    gap = sdl_touch_pane_clampf((float)layout->body_px * 0.42f,
        7.0f, 16.0f);
    for (int i = 0; i < g_hint_quest.button_count; ++i)
    {
        int text_w = sdl_touch_pane_story_text_width(font,
            g_hint_quest.buttons[i].label);

        widths[i] = MAX(layout->footer.h * 1.65f,
            (float)text_w + (float)layout->body_px * 1.55f);
        total += widths[i];
        if (i + 1 < g_hint_quest.button_count)
            total += gap;
    }
    if (total > layout->footer.w)
    {
        float equal = (layout->footer.w
            - gap * (float)(g_hint_quest.button_count - 1))
            / (float)g_hint_quest.button_count;

        for (int i = 0; i < g_hint_quest.button_count; ++i)
            widths[i] = MAX(1.0f, equal);
        total = layout->footer.w;
    }

    x = layout->footer.x + (layout->footer.w - total) * 0.5f;
    for (int i = 0; i < g_hint_quest.button_count; ++i)
    {
        const sdl_hint_quest_button* button = &g_hint_quest.buttons[i];
        SDL_FRect rect = {
            .x = x,
            .y = layout->footer.y,
            .w = widths[i],
            .h = layout->footer.h
        };
        bool hover = g_hint_quest.hover_choice == button->choice;
        SDL_Color fill = sdl_color_from_attr(hover
            ? TERM_L_BLUE : TERM_L_DARK);
        SDL_Color border = sdl_color_from_attr(hover
            ? TERM_L_BLUE : TERM_SLATE);
        SDL_Color text = sdl_color_from_attr(button->attr);

        fill.a = hover ? 105 : 58;
        border.a = hover ? 220 : 120;
        SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g, fill.b,
            fill.a);
        SDL_RenderFillRect(g_state.renderer, &rect);
        SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
            border.a);
        SDL_RenderRect(g_state.renderer, &rect);
        sdl_hint_quest_draw_text(font, button->label, text, &rect, true,
            false);
        sdl_hint_quest_add_hit(rect, button->choice);
        x += widths[i] + gap;
    }
}

void sdl_hint_quest_menu_render(void)
{
    sdl_hint_quest_layout layout;
    TTF_Font* body_font;
    TTF_Font* title_font;
    float block_y[SDL_HINT_QUEST_MAX_BLOCKS];
    float block_h[SDL_HINT_QUEST_MAX_BLOCKS];
    float total_h = 0.0f;
    float content_offset = 0.0f;
    float selected_top = -1.0f;
    float selected_bottom = -1.0f;
    SDL_Rect clip;

    if (!sdl_hint_quest_layout_compute(&layout))
        return;
    body_font = sdl_story_font_for_height_slot(layout.body_px,
        SDL_STORY_FONT_SLOT_NARRATIVE);
    title_font = sdl_story_font_for_height_slot(layout.title_px,
        SDL_STORY_FONT_SLOT_MENU);
    if (!body_font || !title_font)
        return;

    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);
    g_hint_quest.hit_count = 0;
    sdl_hint_quest_draw_chrome(&layout, title_font, body_font);

    for (int i = 0; i < g_hint_quest.block_count; ++i)
    {
        block_y[i] = total_h;
        block_h[i] = sdl_hint_quest_block_height(body_font, &layout,
            &g_hint_quest.blocks[i], NULL);
        if (g_hint_quest.blocks[i].choice == g_hint_quest.selected_choice)
        {
            if (selected_top < 0.0f)
                selected_top = total_h;
            selected_bottom = total_h + block_h[i];
        }
        total_h += block_h[i];
        if (i + 1 < g_hint_quest.block_count)
            total_h += layout.block_gap;
    }

    g_hint_quest.max_scroll_y = MAX(0.0f, total_h - layout.body.h);
    if (g_hint_quest.selected_choice != g_hint_quest.last_selected_choice
        && selected_top >= 0.0f)
    {
        if (selected_top < g_hint_quest.scroll_y)
            g_hint_quest.scroll_y = selected_top;
        else if (selected_bottom > g_hint_quest.scroll_y + layout.body.h)
            g_hint_quest.scroll_y = selected_bottom - layout.body.h;
        g_hint_quest.last_selected_choice = g_hint_quest.selected_choice;
    }
    g_hint_quest.scroll_y = sdl_touch_pane_clampf(g_hint_quest.scroll_y,
        0.0f, g_hint_quest.max_scroll_y);
    if (g_hint_quest.center_body && total_h < layout.body.h)
        content_offset = (layout.body.h - total_h) * 0.5f;

    g_hint_quest.body_rect = layout.body;
    clip = (SDL_Rect){
        .x = (int)layout.body.x,
        .y = (int)layout.body.y,
        .w = MAX(1, (int)(layout.body.w + 0.5f)),
        .h = MAX(1, (int)(layout.body.h + 0.5f))
    };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    for (int i = 0; i < g_hint_quest.block_count; ++i)
    {
        const sdl_hint_quest_block* block = &g_hint_quest.blocks[i];
        float y = layout.body.y + content_offset + block_y[i]
            - g_hint_quest.scroll_y;
        float indent_px = (float)MAX(0, block->indent)
            * (float)layout.body_px * 0.72f;
        SDL_FRect rect = {
            .x = layout.body.x + indent_px,
            .y = y,
            .w = layout.body.w - indent_px,
            .h = block_h[i]
        };

        if (rect.y + rect.h <= layout.body.y
            || rect.y >= layout.body.y + layout.body.h)
        {
            continue;
        }
        if (block->choice != 0)
        {
            bool selected = block->choice == g_hint_quest.selected_choice;
            bool hover = block->choice == g_hint_quest.hover_choice;
            SDL_Color fill = sdl_color_from_attr(selected
                ? TERM_L_BLUE : TERM_L_DARK);
            SDL_Color accent = sdl_color_from_attr(selected
                ? TERM_L_BLUE : TERM_SLATE);
            SDL_FRect accent_rect = rect;
            SDL_FRect visible_hit;

            fill.a = selected ? 92 : (hover ? 70 : 34);
            accent.a = selected ? 235 : (hover ? 160 : 70);
            SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g, fill.b,
                fill.a);
            SDL_RenderFillRect(g_state.renderer, &rect);
            accent_rect.w = MAX(3.0f, (float)layout.body_px * 0.16f);
            SDL_SetRenderDrawColor(g_state.renderer, accent.r, accent.g,
                accent.b, accent.a);
            SDL_RenderFillRect(g_state.renderer, &accent_rect);
            if (sdl_hint_quest_intersect_rect(rect, layout.body,
                    &visible_hit))
            {
                sdl_hint_quest_add_hit(visible_hit, block->choice);
            }

            rect.x += layout.card_pad_x;
            rect.y += layout.card_pad_y;
            rect.w -= layout.card_pad_x * 2.0f;
            rect.h -= layout.card_pad_y * 2.0f;
        }

        if (block->text[0])
        {
            sdl_hint_quest_draw_text(body_font, block->text,
                sdl_color_from_attr(block->attr), &rect, false, true);
        }
    }
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    sdl_hint_quest_draw_footer(&layout, body_font);

    if (g_hint_quest.scroll_y > 0.5f)
    {
        SDL_Color c = sdl_color_from_attr(TERM_SLATE);
        SDL_FRect line = { layout.body.x, layout.body.y,
            layout.body.w, 2.0f };

        SDL_SetRenderDrawColor(g_state.renderer, c.r, c.g, c.b, 120);
        SDL_RenderFillRect(g_state.renderer, &line);
    }
    if (g_hint_quest.scroll_y + 0.5f < g_hint_quest.max_scroll_y)
    {
        SDL_Color c = sdl_color_from_attr(TERM_SLATE);
        SDL_FRect line = { layout.body.x,
            layout.body.y + layout.body.h - 2.0f, layout.body.w, 2.0f };

        SDL_SetRenderDrawColor(g_state.renderer, c.r, c.g, c.b, 120);
        SDL_RenderFillRect(g_state.renderer, &line);
    }
}

void sdl_hint_quest_menu_begin(hint_quest_page page, cptr title,
    cptr section, bool show_tabs, bool center_body, int selected_choice)
{
    bool preserve = g_hint_quest.active && g_hint_quest.page == page
        && g_hint_quest.show_tabs == show_tabs
        && g_hint_quest.center_body == center_body;
    float old_scroll = preserve ? g_hint_quest.scroll_y : 0.0f;
    int old_hover = preserve ? g_hint_quest.hover_choice : 0;
    int old_last_selected = preserve
        ? g_hint_quest.last_selected_choice : selected_choice;

    memset(&g_hint_quest, 0, sizeof(g_hint_quest));
    g_hint_quest.active = true;
    g_hint_quest.page = page;
    g_hint_quest.show_tabs = show_tabs;
    g_hint_quest.center_body = center_body;
    g_hint_quest.selected_choice = selected_choice;
    g_hint_quest.last_selected_choice = old_last_selected;
    g_hint_quest.scroll_y = old_scroll;
    g_hint_quest.hover_choice = old_hover;
    SDL_strlcpy(g_hint_quest.title, title ? title : "Hints & Quests",
        sizeof(g_hint_quest.title));
    SDL_strlcpy(g_hint_quest.section, section ? section : "",
        sizeof(g_hint_quest.section));
    sdl_hint_quest_mark_dirty();
}

void sdl_hint_quest_menu_add_block(cptr text, byte attr, int indent,
    int choice)
{
    sdl_hint_quest_block* block;

    if (!g_hint_quest.active
        || g_hint_quest.block_count >= SDL_HINT_QUEST_MAX_BLOCKS)
    {
        return;
    }
    block = &g_hint_quest.blocks[g_hint_quest.block_count++];
    memset(block, 0, sizeof(*block));
    SDL_strlcpy(block->text, text ? text : "", sizeof(block->text));
    block->attr = attr;
    block->indent = MAX(0, indent);
    block->choice = choice;
}

void sdl_hint_quest_menu_add_button(int choice, cptr label, byte attr)
{
    sdl_hint_quest_button* button;

    if (!g_hint_quest.active || !label || !label[0]
        || g_hint_quest.button_count >= SDL_HINT_QUEST_MAX_BUTTONS)
    {
        return;
    }
    button = &g_hint_quest.buttons[g_hint_quest.button_count++];
    memset(button, 0, sizeof(*button));
    SDL_strlcpy(button->label, label, sizeof(button->label));
    button->attr = attr;
    button->choice = choice;
}

void sdl_hint_quest_menu_finish(void)
{
    if (g_hint_quest.active)
        sdl_hint_quest_mark_dirty();
}

void sdl_hint_quest_menu_hide(void)
{
    if (!g_hint_quest.active)
        return;

    memset(&g_hint_quest, 0, sizeof(g_hint_quest));
    memset(&g_hint_quest_press, 0, sizeof(g_hint_quest_press));
    sdl_hint_quest_mark_dirty();
}

bool sdl_hint_quest_menu_active(void)
{
    return g_hint_quest.active;
}

static void sdl_hint_quest_set_hover(int choice)
{
    bool wake = false;

    if (choice == g_hint_quest.hover_choice)
        return;
    g_hint_quest.hover_choice = choice;
    if (choice != 0)
        (void)ui_menu_click_handle_choice_action(choice,
            UI_MENU_CLICK_HOVER, &wake);
    else
        (void)ui_menu_click_clear_hover(&wake);
    sdl_hint_quest_mark_dirty();
    if (wake)
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
}

static void sdl_hint_quest_activate_choice(int choice, int action)
{
    bool wake = false;

    if (choice == 0)
        return;
    if (!ui_menu_click_handle_choice_action(choice, action, &wake))
        return;
    sdl_hint_quest_mark_dirty();
    Term_keypress((action == UI_MENU_CLICK_SECONDARY)
        ? UI_MENU_CLICK_WAKE_KEY : '\r');
    (void)wake;
}

static void sdl_hint_quest_scroll_by(float delta)
{
    float next = sdl_touch_pane_clampf(g_hint_quest.scroll_y + delta,
        0.0f, g_hint_quest.max_scroll_y);

    if (next == g_hint_quest.scroll_y)
        return;
    g_hint_quest.scroll_y = next;
    sdl_hint_quest_mark_dirty();
}

bool sdl_hint_quest_menu_handle_event(const SDL_Event* ev)
{
    float x = 0.0f;
    float y = 0.0f;

    if (!ev || !g_hint_quest.active)
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        if (ev->motion.which != SDL_TOUCH_MOUSEID)
            sdl_hint_quest_set_hover(sdl_hint_quest_choice_at(
                (float)ev->motion.x, (float)ev->motion.y));
        return true;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.which != SDL_TOUCH_MOUSEID)
        {
            int choice = sdl_hint_quest_choice_at((float)ev->button.x,
                (float)ev->button.y);

            if (ev->button.button == SDL_BUTTON_RIGHT)
                Term_keypress(ESCAPE);
            else if (ev->button.button == SDL_BUTTON_LEFT)
                sdl_hint_quest_activate_choice(choice,
                    UI_MENU_CLICK_PRIMARY);
        }
        return true;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        return true;

    case SDL_EVENT_MOUSE_WHEEL:
    {
        SDL_Rect screen = sdl_get_layout_screen_rect();

        sdl_hint_quest_scroll_by(-(float)ev->wheel.y * 3.0f
            * (float)sdl_hint_quest_body_px(&screen));
        return true;
    }

    case SDL_EVENT_FINGER_DOWN:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        sdl_note_touch_event_device(ev->tfinger.touchID);
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return true;
        g_hint_quest_press = (sdl_hint_quest_press){
            .active = true,
            .touch = true,
            .dragged = false,
            .finger_id = ev->tfinger.fingerID,
            .choice = sdl_hint_quest_choice_at(x, y),
            .start_x = x,
            .start_y = y,
            .last_y = y
        };
        return true;

    case SDL_EVENT_FINGER_MOTION:
        if (!g_hint_quest_press.active
            || g_hint_quest_press.finger_id != ev->tfinger.fingerID)
        {
            return true;
        }
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return true;
        if (SDL_fabsf(y - g_hint_quest_press.start_y)
                > sdl_touch_swipe_threshold_px()
            || SDL_fabsf(x - g_hint_quest_press.start_x)
                > sdl_touch_swipe_threshold_px())
        {
            g_hint_quest_press.dragged = true;
        }
        if (g_hint_quest_press.dragged)
            sdl_hint_quest_scroll_by(g_hint_quest_press.last_y - y);
        g_hint_quest_press.last_y = y;
        return true;

    case SDL_EVENT_FINGER_UP:
        if (!g_hint_quest_press.active
            || g_hint_quest_press.finger_id != ev->tfinger.fingerID)
        {
            return true;
        }
        if (sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y)
            && !g_hint_quest_press.dragged)
        {
            int choice = sdl_hint_quest_choice_at(x, y);

            if (choice == g_hint_quest_press.choice)
                sdl_hint_quest_activate_choice(choice,
                    UI_MENU_CLICK_PRIMARY);
        }
        memset(&g_hint_quest_press, 0, sizeof(g_hint_quest_press));
        return true;

    case SDL_EVENT_FINGER_CANCELED:
        memset(&g_hint_quest_press, 0, sizeof(g_hint_quest_press));
        return true;

    default:
        return false;
    }
}
