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
    SDL_HINT_QUEST_LABEL_LEN = 48,
    SDL_HINT_QUEST_PAGE_TURN_MS = 850
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
    bool horizontal;
    SDL_FingerID finger_id;
    int choice;
    float start_x;
    float start_y;
    float last_y;
} sdl_hint_quest_press;

typedef struct sdl_hint_quest_turn {
    bool pending;
    bool active;
    int dir;
    hint_quest_page to_page;
    Uint64 start_ns;
    SDL_Texture* from_texture;
    SDL_Texture* to_texture;
    SDL_FRect region;
} sdl_hint_quest_turn;

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
static sdl_hint_quest_turn g_hint_quest_turn;

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
    bool portrait;
    int px;

    if (!screen)
        return 44;
    short_side = (float)MIN(screen->w, screen->h);
    portrait = screen->h > screen->w;

    /* Match the readability-first Tale Statistics scale.  Portrait needs a
     * little more of the short side because the narrow measure makes the same
     * nominal type look smaller; long lists already scroll safely. */
    px = (int)(short_side * (portrait ? 0.108f : 0.090f) + 0.5f);
    if (px < 44)
        px = 44;
    if (px > 72)
        px = 72;
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
    out->title_px = (int)((float)out->body_px * 1.18f + 0.5f);
    out->line_h = (float)out->body_px * 1.22f;
    out->block_gap = sdl_touch_pane_clampf((float)out->body_px * 0.46f,
        8.0f, 22.0f);
    out->card_pad_x = sdl_touch_pane_clampf((float)out->body_px * 0.42f,
        10.0f, 24.0f);
    out->card_pad_y = sdl_touch_pane_clampf((float)out->body_px * 0.24f,
        6.0f, 14.0f);

    margin = sdl_touch_pane_clampf((float)MIN(screen.w, screen.h) * 0.024f,
        10.0f, 28.0f);
    panel_w = (float)screen.w - margin * 2.0f;
    {
        /* Larger prose needs a wider reading leaf as well as a larger font.
         * Keep the frame comfortably inside the safe layout rectangle while
         * giving wrapped quest and hint text useful line lengths. */
        float panel_max = (float)screen.w * (portrait ? 0.94f : 0.82f);

        if (!portrait && panel_max < 680.0f)
            panel_max = 680.0f;
        if (panel_w > panel_max)
            panel_w = panel_max;
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

    pad_x = sdl_touch_pane_clampf((float)out->body_px * 1.18f,
        22.0f, 54.0f);
    pad_y = sdl_touch_pane_clampf((float)out->body_px * 0.72f,
        14.0f, 34.0f);
    gap = sdl_touch_pane_clampf((float)out->body_px * 0.40f,
        8.0f, 18.0f);
    title_h = (float)out->title_px * 1.22f;
    if (g_hint_quest.show_tabs)
        tabs_h = MAX(44.0f, (float)out->body_px * 1.38f);
    section_h = g_hint_quest.section[0]
        ? (float)out->body_px * 1.26f : 0.0f;
    if (g_hint_quest.button_count > 0)
        footer_h = MAX(46.0f, (float)out->body_px * 1.56f);

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

static void sdl_hint_quest_page_turn_clear(void)
{
    if (g_hint_quest_turn.from_texture)
        SDL_DestroyTexture(g_hint_quest_turn.from_texture);
    if (g_hint_quest_turn.to_texture)
        SDL_DestroyTexture(g_hint_quest_turn.to_texture);
    memset(&g_hint_quest_turn, 0, sizeof(g_hint_quest_turn));
}

static SDL_Texture* sdl_hint_quest_capture_page(SDL_FRect* out_region)
{
    sdl_hint_quest_layout layout;
    SDL_Rect read_rect;
    SDL_Surface* surface;
    SDL_Texture* texture;

    if (!g_state.renderer || !sdl_hint_quest_layout_compute(&layout))
        return NULL;

    /* Draw the current semantic page into the backbuffer immediately, then
     * copy only the framed leaf.  The snapshot therefore has the same pixels,
     * wrapping and focus state as the live page that is about to turn. */
    sdl_hint_quest_menu_render();
    read_rect = (SDL_Rect){
        (int)layout.panel.x,
        (int)layout.panel.y,
        MAX(1, (int)(layout.panel.w + 0.5f)),
        MAX(1, (int)(layout.panel.h + 0.5f))
    };
    surface = SDL_RenderReadPixels(g_state.renderer, &read_rect);
    if (!surface)
    {
        log_warn("Hints book page snapshot failed: %s", SDL_GetError());
        return NULL;
    }
    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture)
    {
        log_warn("Hints book page texture failed: %s", SDL_GetError());
        return NULL;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    if (out_region)
    {
        *out_region = (SDL_FRect){
            (float)read_rect.x,
            (float)read_rect.y,
            (float)read_rect.w,
            (float)read_rect.h
        };
    }
    return texture;
}

static bool sdl_hint_quest_render_page_turn(void)
{
    Uint64 now;
    float t;
    float eased;
    float curl;
    SDL_Texture* background;
    SDL_Texture* leaf;

    if (!g_hint_quest_turn.active || !g_hint_quest_turn.from_texture
        || !g_hint_quest_turn.to_texture)
    {
        return false;
    }

    now = SDL_GetTicksNS();
    t = (float)((now - g_hint_quest_turn.start_ns) / 1000000ULL)
        / (float)SDL_HINT_QUEST_PAGE_TURN_MS;
    if (t >= 1.0f)
    {
        sdl_hint_quest_page_turn_clear();
        return false;
    }
    if (t < 0.0f)
        t = 0.0f;
    eased = (t < 0.5f)
        ? (4.0f * t * t * t)
        : (1.0f - SDL_powf(-2.0f * t + 2.0f, 3.0f) * 0.5f);

    if (g_hint_quest_turn.dir > 0)
    {
        background = g_hint_quest_turn.to_texture;
        leaf = g_hint_quest_turn.from_texture;
        curl = eased;
    }
    else
    {
        background = g_hint_quest_turn.from_texture;
        leaf = g_hint_quest_turn.to_texture;
        curl = 1.0f - eased;
    }

    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);
    SDL_RenderTexture(g_state.renderer, background, NULL,
        &g_hint_quest_turn.region);
    sdl_char_sheet_draw_curled_leaf(leaf, g_hint_quest_turn.region, curl);
    g_state.need_present = true;
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

static void sdl_hint_quest_draw_book_leaf(
    const sdl_hint_quest_layout* layout)
{
    SDL_FRect edge;
    float border;
    float inset;
    float hairline;

    if (!layout)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    border = MAX(2.0f, (float)layout->body_px * 0.075f);
    SDL_SetRenderDrawColor(g_state.renderer, 206, 196, 170, 210);
    edge = (SDL_FRect){ layout->panel.x, layout->panel.y,
        layout->panel.w, border };
    SDL_RenderFillRect(g_state.renderer, &edge);
    edge.y = layout->panel.y + layout->panel.h - border;
    SDL_RenderFillRect(g_state.renderer, &edge);
    edge = (SDL_FRect){ layout->panel.x, layout->panel.y,
        border, layout->panel.h };
    SDL_RenderFillRect(g_state.renderer, &edge);
    edge.x = layout->panel.x + layout->panel.w - border;
    SDL_RenderFillRect(g_state.renderer, &edge);

    inset = border * 2.5f;
    hairline = MAX(1.0f, border * 0.5f);
    SDL_SetRenderDrawColor(g_state.renderer, 150, 140, 120, 150);
    edge = (SDL_FRect){ layout->panel.x + inset,
        layout->panel.y + inset,
        layout->panel.w - inset * 2.0f, hairline };
    SDL_RenderFillRect(g_state.renderer, &edge);
    edge.y = layout->panel.y + layout->panel.h - inset - hairline;
    SDL_RenderFillRect(g_state.renderer, &edge);
    edge = (SDL_FRect){ layout->panel.x + inset,
        layout->panel.y + inset, hairline,
        layout->panel.h - inset * 2.0f };
    SDL_RenderFillRect(g_state.renderer, &edge);
    edge.x = layout->panel.x + layout->panel.w - inset - hairline;
    SDL_RenderFillRect(g_state.renderer, &edge);
}

static void sdl_hint_quest_draw_center_ornament(float center_x, float y,
    float width, float thickness)
{
    SDL_FRect line;
    float half = MAX(12.0f, width * 0.5f);
    float gap = MAX(6.0f, thickness * 4.0f);

    SDL_SetRenderDrawColor(g_state.renderer, 111, 81, 38, 210);
    line = (SDL_FRect){ center_x - half - gap, y, half, thickness };
    SDL_RenderFillRect(g_state.renderer, &line);
    line.x = center_x + gap;
    SDL_RenderFillRect(g_state.renderer, &line);
    line = (SDL_FRect){ center_x - thickness * 1.5f,
        y - thickness, thickness * 3.0f, thickness * 3.0f };
    SDL_RenderFillRect(g_state.renderer, &line);
}

static void sdl_hint_quest_draw_chrome(const sdl_hint_quest_layout* layout,
    TTF_Font* title_font, TTF_Font* body_font)
{
    static const char* const tab_labels[] = {
        "I. Hints", "II. Quests", "III. Thralls"
    };
    static const int tab_choices[] = {
        HINT_QUEST_CLICK_HINTS_TAB,
        HINT_QUEST_CLICK_QUESTS_TAB,
        HINT_QUEST_CLICK_THRALLS_TAB
    };
    SDL_Color gold = sdl_color_from_attr(TERM_YELLOW);
    SDL_Color parchment = sdl_color_from_attr(TERM_L_WHITE);
    float ornament_y;
    float ornament_w;
    float ornament_t;

    sdl_hint_quest_draw_book_leaf(layout);
    sdl_hint_quest_draw_text(title_font, g_hint_quest.title, gold,
        &layout->title, true, false);
    ornament_y = layout->title.y + layout->title.h
        - MAX(1.0f, (float)layout->body_px * 0.035f);
    ornament_w = layout->title.w * 0.15f;
    ornament_t = MAX(1.0f, (float)layout->body_px * 0.035f);
    sdl_hint_quest_draw_center_ornament(
        layout->title.x + layout->title.w * 0.5f,
        ornament_y, ornament_w, ornament_t);

    if (g_hint_quest.show_tabs)
    {
        SDL_FRect rule = {
            layout->tabs[0].x,
            layout->tabs[0].y + layout->tabs[0].h - ornament_t,
            layout->tabs[2].x + layout->tabs[2].w - layout->tabs[0].x,
            ornament_t
        };

        SDL_SetRenderDrawColor(g_state.renderer, 150, 140, 120, 70);
        SDL_RenderFillRect(g_state.renderer, &rule);
        for (int i = 0; i < 3; ++i)
        {
            bool active = g_hint_quest.page == (hint_quest_page)(i + 1);
            bool hover = g_hint_quest.hover_choice == tab_choices[i];
            SDL_Color text = (active || hover) ? gold : parchment;

            if (hover && !active)
            {
                SDL_FRect focus = layout->tabs[i];

                focus.x += layout->card_pad_x * 0.45f;
                focus.w -= layout->card_pad_x * 0.90f;
                focus.y += layout->card_pad_y * 0.30f;
                focus.h -= layout->card_pad_y * 0.60f;
                sdl_char_sheet_draw_focus_rect(focus, false);
                text = sdl_color_from_attr(TERM_DARK);
            }
            sdl_hint_quest_draw_text(body_font, tab_labels[i], text,
                &layout->tabs[i], true, false);
            if (active)
            {
                SDL_FRect underline = {
                    layout->tabs[i].x + layout->tabs[i].w * 0.24f,
                    layout->tabs[i].y + layout->tabs[i].h - ornament_t * 3.0f,
                    layout->tabs[i].w * 0.52f,
                    ornament_t * 1.35f
                };

                SDL_SetRenderDrawColor(g_state.renderer, gold.r, gold.g,
                    gold.b, 220);
                SDL_RenderFillRect(g_state.renderer, &underline);
            }
            sdl_hint_quest_add_hit(layout->tabs[i], tab_choices[i]);
        }
    }

    if (g_hint_quest.section[0])
    {
        sdl_hint_quest_draw_text(body_font, g_hint_quest.section, gold,
            &layout->section, false, false);
        SDL_SetRenderDrawColor(g_state.renderer, 150, 140, 120, 85);
        {
            SDL_FRect rule = {
                layout->section.x,
                layout->section.y + layout->section.h - ornament_t,
                layout->section.w,
                ornament_t
            };

            SDL_RenderFillRect(g_state.renderer, &rule);
        }
    }
}

static void sdl_hint_quest_draw_footer(const sdl_hint_quest_layout* layout,
    TTF_Font* font)
{
    float widths[SDL_HINT_QUEST_MAX_BUTTONS] = { 0 };
    float total = 0.0f;
    float gap;
    float x;
    float rule_h;
    SDL_Color gold = sdl_color_from_attr(TERM_YELLOW);

    if (g_hint_quest.button_count <= 0)
        return;
    rule_h = MAX(1.0f, (float)layout->body_px * 0.035f);
    {
        SDL_FRect rule = {
            layout->footer.x + layout->footer.w * 0.10f,
            layout->footer.y,
            layout->footer.w * 0.80f,
            rule_h
        };

        SDL_SetRenderDrawColor(g_state.renderer, 150, 140, 120, 90);
        SDL_RenderFillRect(g_state.renderer, &rule);
    }
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
        SDL_Color text = hover ? gold : sdl_color_from_attr(button->attr);

        if (hover)
        {
            SDL_FRect focus = rect;

            focus.x += layout->card_pad_x * 0.25f;
            focus.w -= layout->card_pad_x * 0.50f;
            focus.y += layout->card_pad_y * 0.45f;
            focus.h -= layout->card_pad_y * 0.60f;
            sdl_char_sheet_draw_focus_rect(focus, false);
            text = sdl_color_from_attr(TERM_DARK);
        }
        sdl_hint_quest_draw_text(font, button->label, text, &rect, true,
            false);
        if (!hover)
        {
            SDL_FRect underline = {
                rect.x + rect.w * 0.32f,
                rect.y + rect.h - rule_h * 2.6f,
                rect.w * 0.36f,
                rule_h
            };

            SDL_SetRenderDrawColor(g_state.renderer, gold.r, gold.g, gold.b,
                105);
            SDL_RenderFillRect(g_state.renderer, &underline);
        }
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

    if (sdl_hint_quest_render_page_turn())
        return;
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
    if (g_hint_quest.center_body && !g_hint_quest.show_tabs
        && total_h < layout.body.h)
    {
        content_offset = (layout.body.h - total_h) * 0.5f;
    }

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
            bool focused = selected || hover;
            SDL_FRect visible_hit;

            if (focused)
                sdl_char_sheet_draw_focus_rect(rect, selected);
            else
            {
                SDL_FRect rule = {
                    rect.x + layout.card_pad_x,
                    rect.y + rect.h - 1.0f,
                    MAX(1.0f, rect.w - layout.card_pad_x * 2.0f),
                    1.0f
                };

                SDL_SetRenderDrawColor(g_state.renderer, 150, 140, 120, 42);
                SDL_RenderFillRect(g_state.renderer, &rule);
            }
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
            bool focused = block->choice != 0
                && (block->choice == g_hint_quest.selected_choice
                    || block->choice == g_hint_quest.hover_choice);
            byte attr = focused ? TERM_DARK : block->attr;

            sdl_hint_quest_draw_text(body_font, block->text,
                sdl_color_from_attr(attr), &rect, false, true);
        }
    }
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    sdl_hint_quest_draw_footer(&layout, body_font);

    if (g_hint_quest.scroll_y > 0.5f)
    {
        SDL_FRect line = {
            layout.body.x + layout.body.w * 0.38f,
            layout.body.y,
            layout.body.w * 0.24f,
            MAX(1.0f, (float)layout.body_px * 0.04f)
        };

        SDL_SetRenderDrawColor(g_state.renderer, 224, 185, 92, 175);
        SDL_RenderFillRect(g_state.renderer, &line);
    }
    if (g_hint_quest.scroll_y + 0.5f < g_hint_quest.max_scroll_y)
    {
        float h = MAX(1.0f, (float)layout.body_px * 0.04f);
        SDL_FRect line = {
            layout.body.x + layout.body.w * 0.38f,
            layout.body.y + layout.body.h - h,
            layout.body.w * 0.24f,
            h
        };

        SDL_SetRenderDrawColor(g_state.renderer, 224, 185, 92, 175);
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
    if (!g_hint_quest.active)
        return;

    if (g_hint_quest_turn.pending
        && g_hint_quest.page == g_hint_quest_turn.to_page)
    {
        SDL_FRect destination_region;

        g_hint_quest_turn.to_texture = sdl_hint_quest_capture_page(
            &destination_region);
        if (g_hint_quest_turn.from_texture
            && g_hint_quest_turn.to_texture)
        {
            g_hint_quest_turn.region = destination_region;
            g_hint_quest_turn.pending = false;
            g_hint_quest_turn.active = true;
            g_hint_quest_turn.start_ns = SDL_GetTicksNS();
        }
        else
        {
            sdl_hint_quest_page_turn_clear();
        }
    }
    sdl_hint_quest_mark_dirty();
}

void sdl_hint_quest_menu_prepare_page_turn(hint_quest_page next_page)
{
    int direction = (next_page > g_hint_quest.page) ? 1 : -1;

    sdl_hint_quest_menu_prepare_leaf_turn(next_page, direction);
}

void sdl_hint_quest_menu_prepare_leaf_turn(hint_quest_page next_page,
    int direction)
{
    SDL_FRect source_region;

    if (!g_hint_quest.active || next_page == HINT_QUEST_PAGE_EXIT
        || direction == 0)
    {
        return;
    }

    sdl_hint_quest_page_turn_clear();
    g_hint_quest_turn.from_texture = sdl_hint_quest_capture_page(
        &source_region);
    if (!g_hint_quest_turn.from_texture)
        return;

    g_hint_quest_turn.pending = true;
    g_hint_quest_turn.dir = (direction > 0) ? 1 : -1;
    g_hint_quest_turn.to_page = next_page;
    g_hint_quest_turn.region = source_region;
}

void sdl_hint_quest_menu_hide(void)
{
    if (!g_hint_quest.active && !g_hint_quest_turn.pending
        && !g_hint_quest_turn.active)
    {
        return;
    }

    sdl_hint_quest_page_turn_clear();
    memset(&g_hint_quest, 0, sizeof(g_hint_quest));
    memset(&g_hint_quest_press, 0, sizeof(g_hint_quest_press));
    sdl_hint_quest_mark_dirty();
}

bool sdl_hint_quest_menu_active(void)
{
    return g_hint_quest.active;
}

int sdl_hint_quest_menu_pending_timeout_ms(Uint64 now_ns)
{
    (void)now_ns;
    return g_hint_quest_turn.active ? 16 : -1;
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

static void sdl_hint_quest_handle_horizontal_swipe(float delta_x)
{
    hint_quest_page next;
    int choice;

    if (SDL_fabsf(delta_x) <= sdl_touch_swipe_threshold_px())
        return;

    /* Inner leaves turn back into their chapter with a rightward swipe. */
    if (g_hint_quest.center_body)
    {
        if (delta_x > 0.0f)
            Term_keypress(ESCAPE);
        return;
    }
    if (!g_hint_quest.show_tabs)
        return;

    if (delta_x < 0.0f)
    {
        next = (g_hint_quest.page == HINT_QUEST_PAGE_THRALLS)
            ? HINT_QUEST_PAGE_HINTS
            : (hint_quest_page)(g_hint_quest.page + 1);
    }
    else
    {
        next = (g_hint_quest.page == HINT_QUEST_PAGE_HINTS)
            ? HINT_QUEST_PAGE_THRALLS
            : (hint_quest_page)(g_hint_quest.page - 1);
    }

    choice = (next == HINT_QUEST_PAGE_HINTS)
        ? HINT_QUEST_CLICK_HINTS_TAB
        : ((next == HINT_QUEST_PAGE_QUESTS)
            ? HINT_QUEST_CLICK_QUESTS_TAB
            : HINT_QUEST_CLICK_THRALLS_TAB);
    sdl_hint_quest_activate_choice(choice, UI_MENU_CLICK_PRIMARY);
}

bool sdl_hint_quest_menu_handle_event(const SDL_Event* ev)
{
    float x = 0.0f;
    float y = 0.0f;

    if (!ev || !g_hint_quest.active)
        return false;

    if (g_hint_quest_turn.active)
    {
        switch (ev->type)
        {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            return true;
        default:
            break;
        }
    }

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
        if (!g_hint_quest_press.dragged
            && (SDL_fabsf(y - g_hint_quest_press.start_y)
                    > sdl_touch_swipe_threshold_px()
                || SDL_fabsf(x - g_hint_quest_press.start_x)
                    > sdl_touch_swipe_threshold_px()))
        {
            g_hint_quest_press.dragged = true;
            g_hint_quest_press.horizontal =
                SDL_fabsf(x - g_hint_quest_press.start_x)
                    > SDL_fabsf(y - g_hint_quest_press.start_y);
        }
        if (g_hint_quest_press.dragged && !g_hint_quest_press.horizontal)
            sdl_hint_quest_scroll_by(g_hint_quest_press.last_y - y);
        g_hint_quest_press.last_y = y;
        return true;

    case SDL_EVENT_FINGER_UP:
        if (!g_hint_quest_press.active
            || g_hint_quest_press.finger_id != ev->tfinger.fingerID)
        {
            return true;
        }
        if (sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
        {
            if (g_hint_quest_press.dragged
                && g_hint_quest_press.horizontal)
            {
                sdl_hint_quest_handle_horizontal_swipe(
                    x - g_hint_quest_press.start_x);
            }
            else if (!g_hint_quest_press.dragged)
            {
                int choice = sdl_hint_quest_choice_at(x, y);

                if (choice == g_hint_quest_press.choice)
                    sdl_hint_quest_activate_choice(choice,
                        UI_MENU_CLICK_PRIMARY);
            }
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
