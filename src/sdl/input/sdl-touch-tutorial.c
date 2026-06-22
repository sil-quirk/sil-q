#include "angband.h"
#include "sdl/main-sdl-private.h"

float sdl_touch_tutorial_draw_text_line(cptr text, float x, float y,
    float max_w, int font_px, SDL_Color color, bool centered)
{
    TTF_Font* font;
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    float scale = 1.0f;

    if (!text || !text[0] || font_px <= 0)
        return 0.0f;

    font = sdl_story_font_for_height(font_px);
    if (!font)
        return 0.0f;

    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return 0.0f;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return 0.0f;
    }

    if (max_w > 0.0f && surface->w > 0 && (float)surface->w > max_w)
        scale = max_w / (float)surface->w;

    dst = (SDL_FRect){
        .x = centered ? x - (float)surface->w * scale * 0.5f : x,
        .y = y,
        .w = (float)surface->w * scale,
        .h = (float)surface->h * scale,
    };

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
    return dst.h;
}

int sdl_touch_tutorial_wrap_lines(cptr text, TTF_Font* font,
    float max_w, char lines[][SDL_TOUCH_TUTORIAL_LINE_LEN], int max_lines)
{
    const char* p;
    char current[SDL_TOUCH_TUTORIAL_LINE_LEN];
    int line_count = 0;
    bool truncated = false;

    if (!lines || max_lines <= 0)
        return 0;

    p = (text && text[0]) ? text : "";
    current[0] = '\0';

    while (*p) {
        char word[SDL_TOUCH_TUTORIAL_LINE_LEN];
        char candidate[SDL_TOUCH_TUTORIAL_LINE_LEN];
        size_t word_len = 0;

        while (*p && *p != '\n' && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        if (*p == '\n') {
            p++;
            if (current[0]) {
                if (line_count >= max_lines) {
                    truncated = true;
                    break;
                }
                SDL_strlcpy(lines[line_count++], current,
                    SDL_TOUCH_TUTORIAL_LINE_LEN);
                current[0] = '\0';
            }
            continue;
        }

        while (*p && *p != '\n' && !isspace((unsigned char)*p)) {
            if (word_len < sizeof(word) - 1)
                word[word_len++] = *p;
            p++;
        }
        word[word_len] = '\0';
        if (!word[0])
            continue;

        if (!current[0]) {
            SDL_strlcpy(current, word, sizeof(current));
            continue;
        }

        strnfmt(candidate, sizeof(candidate), "%s %s", current, word);
        if (max_w > 1.0f
            && sdl_touch_pane_story_text_width(font, candidate) > (int)max_w)
        {
            if (line_count >= max_lines) {
                truncated = true;
                break;
            }

            SDL_strlcpy(lines[line_count++], current,
                SDL_TOUCH_TUTORIAL_LINE_LEN);
            SDL_strlcpy(current, word, sizeof(current));

            if (line_count >= max_lines && *p) {
                truncated = true;
                break;
            }
        } else {
            SDL_strlcpy(current, candidate, sizeof(current));
        }
    }

    if (!truncated && current[0]) {
        if (line_count < max_lines)
            SDL_strlcpy(lines[line_count++], current,
                SDL_TOUCH_TUTORIAL_LINE_LEN);
        else
            truncated = true;
    }

    if (truncated && line_count > 0)
        sdl_touch_pane_append_ellipsis(lines[line_count - 1],
            SDL_TOUCH_TUTORIAL_LINE_LEN);

    return line_count;
}

float sdl_touch_tutorial_draw_wrapped(cptr text, float x, float y,
    float max_w, int font_px, SDL_Color color)
{
    TTF_Font* font;
    char lines[SDL_TOUCH_TUTORIAL_MAX_LINES][SDL_TOUCH_TUTORIAL_LINE_LEN];
    int line_count;
    float line_h;

    font = sdl_story_font_for_height(font_px);
    if (!font)
        return 0.0f;

    line_count = sdl_touch_tutorial_wrap_lines(text, font, max_w, lines,
        SDL_TOUCH_TUTORIAL_MAX_LINES);
    if (line_count <= 0)
        return 0.0f;

    line_h = (float)font_px * 1.28f;
    for (int i = 0; i < line_count; i++)
        (void)sdl_touch_tutorial_draw_text_line(lines[i], x,
            y + line_h * (float)i, max_w, font_px, color, false);

    return line_h * (float)line_count;
}

float sdl_touch_tutorial_draw_wrapped_centered(cptr text, float x,
    float y, float max_w, int font_px, SDL_Color color)
{
    TTF_Font* font;
    char lines[SDL_TOUCH_TUTORIAL_MAX_LINES][SDL_TOUCH_TUTORIAL_LINE_LEN];
    int line_count;
    float line_h;

    font = sdl_story_font_for_height(font_px);
    if (!font)
        return 0.0f;

    line_count = sdl_touch_tutorial_wrap_lines(text, font, max_w, lines,
        SDL_TOUCH_TUTORIAL_MAX_LINES);
    if (line_count <= 0)
        return 0.0f;

    line_h = (float)font_px * 1.30f;
    for (int i = 0; i < line_count; i++)
        (void)sdl_touch_tutorial_draw_text_line(lines[i], x,
            y + line_h * (float)i, max_w, font_px, color, true);

    return line_h * (float)line_count;
}

int sdl_touch_tutorial_line_count(cptr text, int font_px, float max_w)
{
    TTF_Font* font;
    char lines[SDL_TOUCH_TUTORIAL_MAX_LINES][SDL_TOUCH_TUTORIAL_LINE_LEN];

    if (!text || !text[0])
        return 0;

    font = sdl_story_font_for_height(font_px);
    if (!font)
        return 0;

    return sdl_touch_tutorial_wrap_lines(text, font, max_w, lines,
        SDL_TOUCH_TUTORIAL_MAX_LINES);
}

void sdl_touch_tutorial_draw_screen_dim(const SDL_Rect* screen,
    Uint8 alpha)
{
    if (!screen)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, alpha);
    SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
        .x = (float)screen->x,
        .y = (float)screen->y,
        .w = (float)screen->w,
        .h = (float)screen->h,
    });
}

float sdl_touch_tutorial_top_reserved_height(const SDL_Rect* screen)
{
    float reserved = 0.0f;
    SDL_FRect main_menu_rect;
    SDL_FRect depth_rect;

    if (!screen)
        return 0.0f;

    if (sdl_main_menu_pane_current_rect(&main_menu_rect)
        && main_menu_rect.h > reserved)
    {
        reserved = main_menu_rect.h;
    }
    if (sdl_depth_menu_pane_current_rect(&depth_rect))
    {
        if (depth_rect.h > reserved)
            reserved = depth_rect.h;
    }
    if (reserved > 0.0f) {
        return reserved + sdl_touch_pane_clampf(
            (float)screen->h * 0.010f, 6.0f, 12.0f);
    }

    if (Term && ROW_STATUS == 0) {
        const sdl_view* view = &g_views[PANE_MAIN];

        if (view->cell_h > 0)
            reserved = (float)view->cell_h;
        else
            reserved = sdl_touch_pane_clampf((float)screen->h * 0.055f,
                28.0f, 44.0f);

        reserved += sdl_touch_pane_clampf((float)screen->h * 0.010f,
            6.0f, 12.0f);
    }

    return reserved;
}

float sdl_touch_tutorial_default_header_y(const SDL_Rect* screen)
{
    if (!screen)
        return 0.0f;

    return (float)screen->y + sdl_touch_pane_clampf(
        (float)screen->h * 0.025f, 10.0f, 30.0f)
        + sdl_touch_tutorial_top_reserved_height(screen);
}

float sdl_touch_tutorial_draw_header_at(const SDL_Rect* screen,
    cptr title, cptr body, int page, int page_count, float y)
{
    SDL_Color title_color = g_state.palette[TERM_YELLOW];
    SDL_Color text_color = g_state.palette[TERM_L_WHITE];
    float x;
    float max_w;
    int title_px;
    int body_px;
    char page_buf[32];

    if (!screen)
        return 0.0f;

    x = (float)screen->x + (float)screen->w * 0.5f;
    max_w = (float)screen->w * 0.82f;
    title_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.052f,
        30.0f, 50.0f);
    body_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.032f,
        22.0f, 34.0f);

    y += sdl_touch_tutorial_draw_text_line(title, x, y, max_w, title_px,
        title_color, true);
    y += 5.0f;
    y += sdl_touch_tutorial_draw_wrapped_centered(body, x, y, max_w, body_px,
        text_color);

    strnfmt(page_buf, sizeof(page_buf), "%d/%d", page + 1, page_count);
    (void)sdl_touch_tutorial_draw_text_line(page_buf,
        (float)(screen->x + screen->w) - 18.0f,
        (float)screen->y + 10.0f
            + sdl_touch_tutorial_top_reserved_height(screen),
        80.0f, body_px, text_color, false);

    return y;
}

float sdl_touch_tutorial_draw_header(const SDL_Rect* screen, cptr title,
    cptr body, int page, int page_count)
{
    return sdl_touch_tutorial_draw_header_at(screen, title, body, page,
        page_count, sdl_touch_tutorial_default_header_y(screen));
}

static void sdl_touch_tutorial_prompt_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    if (!buf || buflen == 0)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

void sdl_touch_tutorial_draw_footer(const SDL_Rect* screen, bool mouse,
    bool single_page)
{
    SDL_Color text_color = g_state.palette[TERM_L_WHITE];
    int font_px;
    float line_h;
    float y;
    char advance_text[96];
    char page_text[96];

    if (!screen)
        return;

    font_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.030f,
        22.0f, 30.0f);
    line_h = (float)font_px * 1.30f;
    y = (float)(screen->y + screen->h)
        - sdl_touch_pane_clampf((float)screen->h * 0.090f, 54.0f, 78.0f);

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 185);
    SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
        .x = (float)screen->x,
        .y = y - 8.0f,
        .w = (float)screen->w,
        .h = (float)(screen->y + screen->h) - y + 8.0f,
    });

    if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char back_label[16];
        char prev_label[16];
        char next_label[16];

        sdl_touch_tutorial_prompt_label(steamdeck_confirm_key(), "A",
            confirm_label, sizeof(confirm_label));
        sdl_touch_tutorial_prompt_label(steamdeck_back_key(), "B",
            back_label, sizeof(back_label));
        sdl_touch_tutorial_prompt_label(steamdeck_prev_page_key(), "L1",
            prev_label, sizeof(prev_label));
        sdl_touch_tutorial_prompt_label(steamdeck_next_page_key(), "R1",
            next_label, sizeof(next_label));

        strnfmt(advance_text, sizeof(advance_text), "%s to %s",
            confirm_label, single_page ? "close" : "continue");
        strnfmt(page_text, sizeof(page_text),
            "%s/%s changes page   %s closes", prev_label, next_label,
            back_label);
    }
    else
    {
        SDL_strlcpy(advance_text,
            mouse
                ? (single_page ? "Click or Space to close"
                               : "Click or Space for next")
                : (single_page ? "Tap or Space to close"
                               : "Tap or Space for next"),
            sizeof(advance_text));
        SDL_strlcpy(page_text, "Left/Right changes page   Esc closes",
            sizeof(page_text));
    }

    (void)sdl_touch_tutorial_draw_text_line(
        advance_text,
        (float)screen->x + (float)screen->w * 0.5f, y,
        (float)screen->w * 0.90f, font_px, text_color, true);
    (void)sdl_touch_tutorial_draw_text_line(
        page_text,
        (float)screen->x + (float)screen->w * 0.5f, y + line_h,
        (float)screen->w * 0.90f, font_px, text_color, true);
}

bool sdl_touch_tutorial_cell_rect(int col, int row, int cols, int rows,
    SDL_FRect* out)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int visual_cols;
    int visual_rows;

    if (!out)
        return false;
    if (!view->term_ready || !view->canvas)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0)
        return false;
    if (view->cols <= 0 || view->rows <= 0)
        return false;
    if (col < 0 || row < 0 || cols <= 0 || rows <= 0)
        return false;
    if (col >= view->cols || row >= view->rows)
        return false;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    if (cols > view->cols - col)
        cols = view->cols - col;
    if (cols > visual_cols - MIN(col, visual_cols))
        cols = visual_cols - MIN(col, visual_cols);
    if (rows > visual_rows - row)
        rows = visual_rows - row;
    if (cols <= 0 || rows <= 0)
        return false;

    return sdl_main_cell_rect(col, row, cols, rows, out);
}

bool sdl_touch_tutorial_view_rect(enum pane_type pane, SDL_FRect* out)
{
    const sdl_view* view;

    if (!out)
        return false;
    if (pane < PANE_MAIN || pane >= PANE_MAX)
        return false;
    if ((int)pane >= MAX_TERM_DATA)
        return false;
    if (!sdl_should_show_supporting_panes())
        return false;

    view = &g_views[pane];
    if (!view->term_ready || !view->canvas)
        return false;
    if (!sdl_rect_has_area(&view->rect))
        return false;
    if (pane == PANE_ROLLS && sdl_view_is_overlay_log_pane(view)) {
        SDL_Rect band;

        if (!sdl_overlay_log_pane_current_rect(&band))
            return false;
        *out = (SDL_FRect){
            .x = (float)band.x,
            .y = (float)band.y,
            .w = (float)band.w,
            .h = (float)band.h,
        };
        return true;
    }

    *out = (SDL_FRect){
        .x = (float)view->rect.x,
        .y = (float)view->rect.y,
        .w = (float)view->rect.w,
        .h = (float)view->rect.h,
    };
    return true;
}

void sdl_touch_tutorial_clamp_box_to_screen(SDL_FRect* box,
    const SDL_Rect* screen, float margin)
{
    float min_x;
    float min_y;
    float max_x;
    float max_y;

    if (!box || !screen)
        return;

    min_x = (float)screen->x + margin;
    min_y = (float)screen->y + margin;
    max_x = (float)(screen->x + screen->w) - box->w - margin;
    max_y = (float)(screen->y + screen->h) - box->h - margin;

    if (max_x < min_x)
        max_x = min_x;
    if (max_y < min_y)
        max_y = min_y;

    if (box->x < min_x)
        box->x = min_x;
    if (box->x > max_x)
        box->x = max_x;
    if (box->y < min_y)
        box->y = min_y;
    if (box->y > max_y)
        box->y = max_y;
}

bool sdl_touch_tutorial_compact_layout(const SDL_Rect* screen)
{
    if (Term
        && ((Term->wid > 0 && Term->wid <= 50)
            || (Term->hgt > 0 && Term->hgt <= 18)))
    {
        return true;
    }

    return screen && (screen->w < 900 || screen->h < 560);
}

void sdl_touch_tutorial_draw_compact_zone_label(
    const SDL_Rect* screen, const SDL_FRect* zone, cptr label)
{
    SDL_Color text = g_state.palette[TERM_YELLOW];
    SDL_Color border = g_state.palette[TERM_L_WHITE];
    int font_px;
    float text_y;
    float max_w;

    if (!screen || !zone || !label || !label[0])
        return;
    if (zone->w <= 1.0f || zone->h <= 1.0f)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 18, 96, 156, 96);
    SDL_RenderFillRect(g_state.renderer, zone);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
        228);
    SDL_RenderRect(g_state.renderer, zone);

    font_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.032f,
        18.0f, 28.0f);
    text_y = zone->y + zone->h * 0.5f - (float)font_px * 0.60f;
    if (text_y < zone->y + 1.0f)
        text_y = zone->y + 1.0f;
    if (text_y + (float)font_px * 1.2f > zone->y + zone->h)
        text_y = zone->y + zone->h - (float)font_px * 1.2f;
    if (text_y < (float)screen->y)
        text_y = (float)screen->y;

    max_w = zone->w - 8.0f;
    if (max_w < 24.0f)
        max_w = zone->w;

    (void)sdl_touch_tutorial_draw_text_line(label,
        zone->x + zone->w * 0.5f, text_y, max_w, font_px, text, true);
}

void sdl_touch_tutorial_draw_compact_zone_legend(
    const SDL_Rect* screen, float min_y, const char* const* lines,
    int line_count, bool mouse)
{
    SDL_Color title_color = g_state.palette[TERM_YELLOW];
    SDL_Color text_color = g_state.palette[TERM_L_WHITE];
    SDL_FRect box;
    SDL_FRect shadow;
    float footer_top;
    float available_h;
    float pad;
    float line_h;
    float title_h;
    float w;
    float h;
    int font_px;
    int title_px;

    if (!screen || !lines || line_count <= 0)
        return;

    footer_top = (float)(screen->y + screen->h)
        - sdl_touch_pane_clampf((float)screen->h * 0.090f, 54.0f, 78.0f)
        - 10.0f;
    available_h = footer_top - min_y - 8.0f;
    if (available_h < 56.0f)
        available_h = (float)screen->h * 0.48f;

    font_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.030f,
        16.0f, 24.0f);
    pad = sdl_touch_pane_clampf((float)screen->h * 0.012f, 5.0f, 9.0f);

    for (;;) {
        title_px = font_px + 2;
        line_h = (float)font_px * 1.22f;
        title_h = (float)title_px * 1.22f;
        h = pad * 2.0f + title_h + 4.0f
            + line_h * (float)line_count;
        if (h <= available_h || font_px <= 14)
            break;
        font_px--;
    }

    w = (float)screen->w * 0.88f;
    if (w > 760.0f)
        w = 760.0f;
    if (w > (float)screen->w - pad * 2.0f)
        w = (float)screen->w - pad * 2.0f;
    if (w < 240.0f)
        w = (float)screen->w - pad * 2.0f;
    if (w <= 40.0f)
        return;

    box = (SDL_FRect){
        .x = (float)screen->x + ((float)screen->w - w) * 0.5f,
        .y = min_y + 8.0f,
        .w = w,
        .h = h,
    };
    if (box.y + box.h > footer_top)
        box.y = footer_top - box.h;
    sdl_touch_tutorial_clamp_box_to_screen(&box, screen, pad);

    shadow = box;
    shadow.x += 3.0f;
    shadow.y += 3.0f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 168);
    SDL_RenderFillRect(g_state.renderer, &shadow);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 224);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, title_color.r, title_color.g,
        title_color.b, 236);
    SDL_RenderRect(g_state.renderer, &box);

    {
        float y = box.y + pad;
        float text_w = box.w - pad * 2.0f;

        y += sdl_touch_tutorial_draw_text_line(
            mouse ? "Mouse shortcuts" : "Touch shortcuts",
            box.x + box.w * 0.5f, y, text_w, title_px, title_color, true);
        y += 4.0f;
        for (int i = 0; i < line_count; i++) {
            (void)sdl_touch_tutorial_draw_text_line(lines[i], box.x + pad, y,
                text_w, font_px, text_color, false);
            y += line_h;
        }
    }
}

enum {
    SDL_TOUCH_TUTORIAL_MAX_ZONE_CALLOUTS = 8
};

typedef struct sdl_touch_tutorial_zone_callout {
    SDL_FRect zone;
    cptr title;
    cptr detail;
} sdl_touch_tutorial_zone_callout;

void sdl_touch_tutorial_queue_zone_callout(
    sdl_touch_tutorial_zone_callout* callouts, int* count,
    const SDL_FRect* zone, cptr title, cptr detail)
{
    if (!callouts || !count || !zone)
        return;
    if (*count < 0 || *count >= SDL_TOUCH_TUTORIAL_MAX_ZONE_CALLOUTS)
        return;

    callouts[*count].zone = *zone;
    callouts[*count].title = title;
    callouts[*count].detail = detail;
    (*count)++;
}

void sdl_touch_tutorial_draw_zone_highlight(const SDL_FRect* zone)
{
    SDL_Color border_color = g_state.palette[TERM_L_WHITE];

    if (!zone || zone->w <= 1.0f || zone->h <= 1.0f)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 18, 96, 156, 82);
    SDL_RenderFillRect(g_state.renderer, zone);
    SDL_SetRenderDrawColor(g_state.renderer, border_color.r,
        border_color.g, border_color.b, 235);
    SDL_RenderRect(g_state.renderer, zone);
}

void sdl_touch_tutorial_draw_zone_prompt(const SDL_Rect* screen,
    const SDL_FRect* zone, cptr title, cptr detail, float min_y)
{
    SDL_Color title_color = g_state.palette[TERM_YELLOW];
    SDL_Color detail_color = g_state.palette[TERM_L_WHITE];
    SDL_FRect box;
    SDL_FRect shadow;
    float pad;
    float max_box_w;
    float box_w;
    float box_h;
    float screen_bottom;
    float text_w;
    int title_px;
    int detail_px;
    int detail_lines;

    if (!screen || !zone || zone->w <= 1.0f || zone->h <= 1.0f)
        return;

    title_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.042f,
        26.0f, 38.0f);
    detail_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.034f,
        22.0f, 32.0f);
    pad = sdl_touch_pane_clampf((float)screen->h * 0.016f, 10.0f, 18.0f);

    max_box_w = (float)screen->w - 2.0f * pad;
    if (max_box_w < 80.0f)
        return;

    box_w = (float)screen->w * 0.42f;
    if (box_w < 300.0f)
        box_w = 300.0f;
    if (box_w > 620.0f)
        box_w = 620.0f;
    if (box_w > max_box_w)
        box_w = max_box_w;
    text_w = box_w - pad * 2.0f;
    detail_lines = sdl_touch_tutorial_line_count(detail, detail_px, text_w);

    box_h = pad * 2.0f + (float)title_px * 1.25f;
    if (detail_lines > 0)
        box_h += 7.0f + (float)detail_lines * (float)detail_px * 1.30f;

    box.x = zone->x + zone->w * 0.5f - box_w * 0.5f;
    screen_bottom = (float)(screen->y + screen->h);
    if (zone->w >= box_w + pad * 2.0f && zone->h >= box_h + pad * 2.0f) {
        box.y = zone->y + zone->h * 0.5f - box_h * 0.5f;
    } else if (zone->y + zone->h + box_h + pad <= screen_bottom) {
        box.y = zone->y + zone->h + pad;
    } else {
        box.y = zone->y - box_h - pad;
    }
    if (min_y > (float)screen->y && box.y < min_y) {
        float max_y = screen_bottom - box_h - pad;

        box.y = (max_y >= min_y) ? min_y : max_y;
    }
    box.w = box_w;
    box.h = box_h;
    sdl_touch_tutorial_clamp_box_to_screen(&box, screen, pad);

    shadow = box;
    shadow.x += 3.0f;
    shadow.y += 3.0f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 168);
    SDL_RenderFillRect(g_state.renderer, &shadow);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 222);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, title_color.r, title_color.g,
        title_color.b, 242);
    SDL_RenderRect(g_state.renderer, &box);

    (void)sdl_touch_tutorial_draw_text_line(title,
        box.x + box.w * 0.5f, box.y + pad, text_w,
        title_px, title_color, true);
    if (detail_lines > 0) {
        (void)sdl_touch_tutorial_draw_wrapped_centered(detail,
            box.x + box.w * 0.5f,
            box.y + pad + (float)title_px * 1.34f,
            text_w, detail_px, detail_color);
    }
}

void sdl_touch_tutorial_draw_info_panel(const SDL_Rect* screen,
    float x, float y, float w, cptr title, cptr body)
{
    SDL_Color title_color = g_state.palette[TERM_YELLOW];
    SDL_Color text_color = g_state.palette[TERM_L_WHITE];
    SDL_FRect box;
    SDL_FRect shadow;
    float pad;
    float text_w;
    float h;
    int title_px;
    int body_px;
    int body_lines;

    if (!screen || !body || !body[0] || w <= 20.0f)
        return;

    pad = sdl_touch_pane_clampf((float)screen->h * 0.018f, 11.0f, 20.0f);
    title_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.040f,
        26.0f, 38.0f);
    body_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.034f,
        22.0f, 32.0f);
    text_w = w - pad * 2.0f;
    if (text_w <= 40.0f)
        return;

    body_lines = sdl_touch_tutorial_line_count(body, body_px, text_w);
    h = pad * 2.0f + (float)body_lines * (float)body_px * 1.30f;
    if (title && title[0])
        h += (float)title_px * 1.35f + 5.0f;

    box = (SDL_FRect){ .x = x, .y = y, .w = w, .h = h };
    sdl_touch_tutorial_clamp_box_to_screen(&box, screen, pad);
    shadow = box;
    shadow.x += 3.0f;
    shadow.y += 3.0f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 174);
    SDL_RenderFillRect(g_state.renderer, &shadow);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 224);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, title_color.r, title_color.g,
        title_color.b, 235);
    SDL_RenderRect(g_state.renderer, &box);

    y = box.y + pad;
    if (title && title[0]) {
        y += sdl_touch_tutorial_draw_text_line(title,
            box.x + box.w * 0.5f, y, text_w, title_px, title_color, true);
        y += 5.0f;
    }
    (void)sdl_touch_tutorial_draw_wrapped(body, box.x + pad, y, text_w,
        body_px, text_color);
}

void sdl_touch_tutorial_draw_main_screen_zones_compact(
    const SDL_Rect* screen, float header_bottom, bool mouse)
{
    SDL_FRect rect;
    SDL_Rect pane_rect;
    int term_h;
    int map_cols;
    int panel_rows;
    const char* legend_lines[8];
    int legend_n = 0;
    bool supporting_pane_seen = false;

    if (!screen || !Term)
        return;

    term_h = Term->hgt;
    map_cols = SCREEN_WID * (use_bigtile ? 2 : 1);

    if (sdl_main_menu_pane_current_rect(&rect)) {
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Menu");
        legend_lines[legend_n++] = mouse
            ? "Menu: click for the main menu."
            : "Menu: tap for the main menu.";
    }

    if (sdl_depth_menu_pane_current_rect(&rect)) {
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Depth");
        legend_lines[legend_n++] = mouse
            ? "Depth: click for the map; use +/- for temporary zoom."
            : "Depth: tap for the map; use +/- for temporary zoom.";
    }

    panel_rows = term_h - ROW_MAP;
    if (ROW_STATUS > ROW_MAP)
        panel_rows = ROW_STATUS - ROW_MAP;
    if (!get_sdl_hide_left_panel() && COL_MAP > 0 && panel_rows > 0
        && sdl_touch_tutorial_cell_rect(0, ROW_MAP, COL_MAP,
            panel_rows, &rect))
    {
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Character");
        legend_lines[legend_n++] = mouse
            ? "Character: click sidebar rows for matching sheets."
            : "Character: tap sidebar rows for matching sheets.";
    }

    if (SCREEN_HGT > 0 && map_cols > 0
        && sdl_touch_tutorial_cell_rect(COL_MAP, ROW_MAP, map_cols,
            SCREEN_HGT, &rect))
    {
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect,
            "Map / Player");
        legend_lines[legend_n++] = mouse
            ? "Map/player: left-click to path or target; right-click for actions."
            : "Map/player: tap to path or target; hold for actions.";
    }

    if (sdl_combat_overlay_pane_current_rect(&pane_rect)) {
        rect = (SDL_FRect){ (float)pane_rect.x, (float)pane_rect.y,
            (float)pane_rect.w, (float)pane_rect.h };
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Combat");
    }
    {
        SDL_FRect qa_rects[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];

        if (sdl_touch_top_panel_compute_layout_for_display(qa_rects, &rect)) {
            sdl_touch_top_panel_render_buttons(qa_rects);
            sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Quick");
        }
    }
    if (sdl_status_pane_current_rect(&pane_rect, NULL)) {
        rect = (SDL_FRect){ (float)pane_rect.x, (float)pane_rect.y,
            (float)pane_rect.w, (float)pane_rect.h };
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Status");
    }
    if (sdl_touch_tutorial_view_rect(PANE_ROLLS, &rect))
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Rolls");
    if (legend_n < (int)N_ELEMENTS(legend_lines)) {
        legend_lines[legend_n++] = mouse
            ? "Overlays: click combat, quick access, status, depth, or rolls."
            : "Overlays: tap combat, quick access, status, depth, or rolls.";
    }

    if (sdl_touch_tutorial_view_rect(PANE_INVENTORY, &rect)) {
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Inventory");
        supporting_pane_seen = true;
    }
    if (sdl_touch_tutorial_view_rect(PANE_WORN, &rect)) {
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Equipment");
        supporting_pane_seen = true;
    }
    if (sdl_touch_tutorial_view_rect(PANE_LOG, &rect)) {
        sdl_touch_tutorial_draw_compact_zone_label(screen, &rect, "Messages");
        supporting_pane_seen = true;
    }
    if (supporting_pane_seen && legend_n < (int)N_ELEMENTS(legend_lines)) {
        legend_lines[legend_n++] = mouse
            ? "Panes: click inventory, equipment, or messages panes."
            : "Panes: tap inventory, equipment, or messages panes.";
    }

    sdl_touch_tutorial_draw_compact_zone_legend(screen, header_bottom,
        legend_lines, legend_n, mouse);
}

void sdl_touch_tutorial_draw_main_screen_zones(
    const SDL_Rect* screen, bool mouse, float min_callout_y)
{
    sdl_touch_tutorial_zone_callout callouts[SDL_TOUCH_TUTORIAL_MAX_ZONE_CALLOUTS];
    SDL_FRect rect;
    SDL_Rect pane_rect;
    int callout_count = 0;
    int term_h;
    int map_cols;
    int panel_rows;

    if (!screen || !Term)
        return;

    term_h = Term->hgt;
    map_cols = SCREEN_WID * (use_bigtile ? 2 : 1);

    if (sdl_main_menu_pane_current_rect(&rect)) {
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Main menu",
            mouse
                ? "Click: open the main menu."
                : "Tap: open the main touch menu.");
    }

    if (sdl_depth_menu_pane_current_rect(&rect)) {
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Depth pane",
            mouse
                ? "Click depth: open the map.\n+/-: change temporary zoom."
                : "Tap depth: open the map.\n+/-: change temporary zoom.");
    }

    panel_rows = term_h - ROW_MAP;
    if (ROW_STATUS > ROW_MAP)
        panel_rows = ROW_STATUS - ROW_MAP;
    if (!get_sdl_hide_left_panel() && COL_MAP > 0 && panel_rows > 0
        && sdl_touch_tutorial_cell_rect(0, ROW_MAP, COL_MAP,
            panel_rows, &rect))
    {
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Character panel",
            mouse
                ? "Click rows: character, skills, abilities, song, supplies, inventory, and attack-mode shortcuts.\nUse when: you want the relevant screen without opening the full menu."
                : "Tap rows: character, skills, abilities, song, supplies, inventory, and attack-mode shortcuts.\nUse when: you want the relevant screen without opening the full menu.");
    }

    if (SCREEN_HGT > 0 && map_cols > 0
        && sdl_touch_tutorial_cell_rect(COL_MAP, ROW_MAP, map_cols,
            SCREEN_HGT, &rect))
    {
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Map / player",
            mouse
                ? "Left-click: path to an explored or open square, or select a target.\nRight-click: open contextual actions, look, or special movement choices.\nMouse Movement: choose On, Off, or Right click only in Mouse Input."
                : "Tap: path to an explored or open square, or select a target.\nHold/right-click: open contextual actions, look, or special movement choices.\nPlayer square: action wheel; Use/Desc act on the floor item, hold/right-click them for full item menus.");
    }

    if (sdl_combat_overlay_pane_current_rect(&pane_rect)) {
        rect = (SDL_FRect){ (float)pane_rect.x, (float)pane_rect.y,
            (float)pane_rect.w, (float)pane_rect.h };
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Combat overlay",
            mouse
                ? "Click an attack row to choose the matching attack mode.\nDefault placement: lower-left corner."
                : "Tap an attack row to choose the matching attack mode.\nDefault placement: lower-left corner.");
    }
    {
        SDL_FRect qa_rects[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];

        if (sdl_touch_top_panel_compute_layout_for_display(qa_rects, &rect)) {
            sdl_touch_top_panel_render_buttons(qa_rects);
            sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
                "Quick access",
                mouse
                    ? "Click an icon for its command; right-click for its alternate.\nDefault placement: bottom center. Item descriptions open above this anchor. Edit buttons in Touch Settings."
                    : "Tap an icon for its command; hold for its alternate.\nDefault placement: bottom center. Item descriptions open above this anchor. Edit buttons in Touch Settings.");
        }
    }
    if (sdl_status_pane_current_rect(&pane_rect, NULL)) {
        rect = (SDL_FRect){ (float)pane_rect.x, (float)pane_rect.y,
            (float)pane_rect.w, (float)pane_rect.h };
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Status overlay",
            "Shows temporary conditions and remaining durations.\nDefault placement: lower-right corner.");
    }
    if (sdl_touch_tutorial_view_rect(PANE_ROLLS, &rect)) {
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Roll log overlay",
            mouse
                ? "Click: open combat history.\nDefault placement: upper-right, below the depth control."
                : "Tap: open combat history.\nDefault placement: upper-right, below the depth control.");
    }

    if (sdl_touch_tutorial_view_rect(PANE_INVENTORY, &rect)) {
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Inventory pane",
            mouse
                ? "Click: open inventory.\nUse for: inspecting, using, dropping, or managing carried items without going through the main menu."
                : "Tap: open inventory.\nUse for: inspecting, using, dropping, or managing carried items without going through the main menu.");
    }
    if (sdl_touch_tutorial_view_rect(PANE_WORN, &rect)) {
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Equipment pane",
            mouse
                ? "Click: open equipment.\nUse for: inspecting worn gear, comparing equipment, taking items off, and checking current loadout."
                : "Tap: open equipment.\nUse for: inspecting worn gear, comparing equipment, taking items off, and checking current loadout.");
    }
    if (sdl_touch_tutorial_view_rect(PANE_LOG, &rect)) {
        sdl_touch_tutorial_queue_zone_callout(callouts, &callout_count, &rect,
            "Messages pane",
            mouse
                ? "Click: review recent log entries.\nUse after: combat rounds, warnings, sounds, prompts, or long automatic actions."
                : "Tap: review recent log entries.\nUse after: combat rounds, warnings, sounds, prompts, or long automatic actions.");
    }
    for (int i = 0; i < callout_count; i++)
        sdl_touch_tutorial_draw_zone_highlight(&callouts[i].zone);
    for (int i = 0; i < callout_count; i++) {
        sdl_touch_tutorial_draw_zone_prompt(screen, &callouts[i].zone,
            callouts[i].title, callouts[i].detail, min_callout_y);
    }
}

void sdl_touch_tutorial_draw_zones_page(const SDL_Rect* screen,
    int page, int page_count, bool mouse)
{
    bool compact = sdl_touch_tutorial_compact_layout(screen);
    float header_bottom;

    sdl_touch_tutorial_draw_screen_dim(screen, 112);
    header_bottom = sdl_touch_tutorial_draw_header(screen,
        mouse ? "Main Screen Mouse Controls" : "Default Touch Layout",
        mouse
            ? (compact
                ? "Click highlighted regions to open views and menus. Left-click the map to move; right-click for actions."
                : "Click highlighted regions to open views and menus. Left-click the map to move; right-click for actions. Mouse Movement can be changed any time in Options > Input Options > Mouse Input.")
            : (compact
                ? "Tap highlighted regions to open views. The fixed overlays use the placements shown here."
                : "Tap highlighted regions to open views. Combat is lower left, quick access bottom center, status lower right, and depth/rolls upper right."),
        page, page_count);

    if (compact)
        sdl_touch_tutorial_draw_main_screen_zones_compact(screen,
            header_bottom, mouse);
    else
        sdl_touch_tutorial_draw_main_screen_zones(screen, mouse,
            header_bottom + sdl_touch_pane_clampf((float)screen->h * 0.018f,
                10.0f, 20.0f));

    sdl_touch_tutorial_draw_footer(screen, mouse, page_count == 1);
}

void sdl_touch_tutorial_draw_overlay_menu(const SDL_Rect* screen)
{
    SDL_FRect button_rects[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];

    if (!screen)
        return;

    if (!sdl_touch_top_panel_compute_layout_for_display(button_rects, NULL))
        return;

    sdl_touch_top_panel_render_buttons(button_rects);
}

void sdl_touch_tutorial_draw_pane_page(const SDL_Rect* screen, int page,
    int page_count)
{
    SDL_Rect pane;
    SDL_FRect slot_rects[SDL_TOUCH_PANE_BUTTON_COUNT];
    SDL_Color text = g_state.palette[TERM_YELLOW];
    SDL_Color border = g_state.palette[TERM_L_WHITE];
    bool have_pane = false;

    sdl_touch_tutorial_draw_screen_dim(screen, 128);
    sdl_touch_tutorial_draw_header(screen,
        "Preset: Touch pane + touch screen",
        "Visible command pad. Tap buttons for actions; hold for alternates. Change presets any time in Touch Settings.",
        page, page_count);

    have_pane = sdl_touch_pane_current_rect(&pane)
        && sdl_touch_pane_compute_layout(&pane, slot_rects);
    if (have_pane) {
        int panel = sdl_touch_pane_active_panel();

        for (int visual_index = 0; visual_index < SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT;
             visual_index++)
        {
            int slot = sdl_touch_pane_visible_slot_at(visual_index);
            int binding;
            char label[SDL_TOUCH_PANE_LABEL_LEN];
            char symbol[32];

            if (slot < 0)
                continue;

            binding = sdl_touch_pane_effective_binding_for_panel(panel, slot);
            sdl_touch_pane_display_label_for_slot(panel, slot, label,
                sizeof(label));
            if (slot == SDL_TOUCH_PANE_CENTER_SLOT
                && sdl_touch_pane_confirm_binding(binding))
            {
                SDL_strlcpy(label, "Confirm", sizeof(label));
                SDL_strlcpy(symbol, "(pick)", sizeof(symbol));
            } else {
                sdl_touch_pane_binding_symbol(binding, symbol, sizeof(symbol));
                if (!config.touch_pane_key_labels_visible
                    || sdl_touch_pane_should_hide_symbol(label, symbol))
                {
                    symbol[0] = '\0';
                }
            }

            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_state.renderer, 20, 78, 130, 96);
            SDL_RenderFillRect(g_state.renderer, &slot_rects[slot]);
            SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g,
                border.b, 230);
            SDL_RenderRect(g_state.renderer, &slot_rects[slot]);

            if (sdl_touch_pane_binding_is_direction(binding))
                sdl_touch_pane_draw_arrow(&slot_rects[slot], binding, text);
            else
                sdl_touch_pane_draw_button_text_scaled(&slot_rects[slot],
                    label, symbol, text, 0.30f, 0.40f);
        }

        {
            float panel_x;
            float panel_w;
            float margin = sdl_touch_pane_clampf((float)screen->w * 0.025f,
                18.0f, 36.0f);
            cptr body =
                "Tap: use the command printed on the button. Tap 2nd Panel to swap panes, then tap it again to return.\nSecond panel: Esc -> Ctrl, Stealth -> Exchange, Inv -> Equip, Supply -> Fletch, View -> Map, Sing -> Smith, Char -> Ability, Desc -> Quaff.\nChar opens character details. Supply opens supplies. Shoot fires with the f key.\nConfirm (pick): confirms prompts, picks up, enters, or waits depending on context.\nDirection buttons: step in one of eight directions; long-touch movement uses the active profile's alternate movement behavior.\nPresets: change this layout any time in Options > Input Options > Touch Settings.";

            if (pane.x < screen->x + screen->w / 2) {
                panel_x = (float)(pane.x + pane.w) + margin;
                panel_w = (float)(screen->x + screen->w) - panel_x - margin;
            } else {
                panel_x = (float)screen->x + margin;
                panel_w = (float)pane.x - panel_x - margin;
            }
            if (panel_w < (float)screen->w * 0.38f) {
                panel_x = (float)screen->x + margin;
                panel_w = (float)screen->w * 0.58f;
            }

            sdl_touch_tutorial_draw_info_panel(screen, panel_x,
                (float)screen->y + (float)screen->h * 0.28f, panel_w,
                "Touch pane buttons", body);
        }
    } else {
        SDL_Color body = g_state.palette[TERM_L_WHITE];
        float max_w = (float)screen->w * 0.70f;
        float x = (float)screen->x + (float)screen->w * 0.15f;
        float y = (float)screen->y + (float)screen->h * 0.38f;
        int font_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.038f,
            22.0f, 32.0f);

        (void)sdl_touch_tutorial_draw_wrapped(
            "The touch pane is currently hidden or disabled. Choose the Touch pane + touch screen profile to show it by default.",
            x, y, max_w, font_px, body);
    }

    sdl_touch_tutorial_draw_footer(screen, false, page_count == 1);
}

void sdl_touch_tutorial_draw_movement_page(const SDL_Rect* screen,
    int page, int page_count)
{
    SDL_Color text = g_state.palette[TERM_L_WHITE];
    SDL_Color zone_text = g_state.palette[TERM_YELLOW];
    SDL_Color border = g_state.palette[TERM_L_WHITE];
    SDL_FRect zone_rects[TOUCH_ZONE_COUNT];
    float x;
    float y;
    float max_w;
    int font_px;

    sdl_touch_tutorial_draw_screen_dim(screen, 142);
    sdl_touch_tutorial_draw_overlay_menu(screen);
    sdl_touch_tutorial_draw_header(screen,
        "Preset: Corners + quick access",
        "Pane hidden. Side corner zones handle movement and fast commands. Change presets any time in Touch Settings.",
        page, page_count);

    if (sdl_touch_zone_compute_layout_for_screen(screen, zone_rects)) {
        for (int i = 0; i < TOUCH_ZONE_COUNT; i++) {
            char name[32];
            char symbol[32];

            sdl_touch_zone_button_label(i, name, sizeof(name), symbol,
                sizeof(symbol));
            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_state.renderer, 18, 96, 156, 92);
            SDL_RenderFillRect(g_state.renderer, &zone_rects[i]);
            SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g,
                border.b, 232);
            SDL_RenderRect(g_state.renderer, &zone_rects[i]);
            sdl_touch_pane_draw_button_text_scaled(&zone_rects[i], name,
                symbol, zone_text, 0.24f, 0.32f);
        }
        sdl_touch_tutorial_draw_info_panel(screen,
            (float)screen->x + (float)screen->w * 0.27f,
            (float)screen->y + (float)screen->h * 0.56f,
            (float)screen->w * 0.46f, "Corners preset",
            "Tap arrows: step in the shown direction.\nThe top and bottom non-arrow buttons use configurable commands.\nHold center blocks or command buttons for alternate bindings.\nSwipe edge: reveal or hide touch controls.\nChange preset and corner side in Touch Settings.");
    } else {
        x = (float)screen->x + (float)screen->w * 0.14f;
        y = (float)screen->y + (float)screen->h * 0.34f;
        max_w = (float)screen->w * 0.72f;
        font_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.038f,
            22.0f, 32.0f);

        (void)sdl_touch_tutorial_draw_wrapped(
            "Tap the side corner arrows to move. The non-arrow top and bottom buttons are configurable commands.",
            x, y, max_w, font_px, text);
    }

    sdl_touch_tutorial_draw_footer(screen, false, page_count == 1);
}

void sdl_touch_tutorial_draw_buttonwheel_page(const SDL_Rect* screen,
    int page, int page_count)
{
    SDL_Color wheel = g_state.palette[TERM_YELLOW];
    SDL_Color inner_wheel = g_state.palette[TERM_L_BLUE];
    float cx;
    float cy;
    float radius;
    float inner_radius;
    float header_bottom;
    float margin;
    float panel_x;
    float panel_w;
    float panel_y;
    bool have_wheel;

    sdl_touch_tutorial_draw_screen_dim(screen, 150);
    sdl_touch_tutorial_draw_overlay_menu(screen);
    header_bottom = sdl_touch_tutorial_draw_header(screen,
        "Button Wheel + Quick Access",
        "The wheel uses the open right-side lane between the upper and lower overlays. Quick access stays at bottom center.",
        page, page_count);

    have_wheel = sdl_touch_round_compute_layout(&cx, &cy, &radius,
        &inner_radius, NULL);
    if (!have_wheel) {
        radius = sdl_touch_pane_clampf((float)screen->h * 0.115f,
            58.0f, 122.0f);
        inner_radius = radius * 0.58f;
        cx = (float)screen->x + (float)screen->w * 0.78f;
        cy = header_bottom + radius + sdl_touch_pane_clampf(
            (float)screen->h * 0.12f, 52.0f, 84.0f);
    }

    wheel.a = 248;
    inner_wheel.a = 210;
    sdl_touch_round_draw_circle(cx, cy, radius, wheel);
    sdl_touch_round_draw_circle(cx, cy, inner_radius, inner_wheel);
    sdl_touch_round_draw_sector_lines(cx, cy, inner_radius, radius, wheel);
    {
        static const int dirs[] = { 7, 8, 9, 4, 6, 1, 2, 3 };
        float mid = (inner_radius + radius) * 0.5f;
        float button_size = (radius - inner_radius) * 1.14f;

        if (button_size < 28.0f)
            button_size = 28.0f;
        for (int i = 0; i < (int)N_ELEMENTS(dirs); i++) {
            int dir = dirs[i];
            float ux = (float)ddx[dir];
            float uy = (float)ddy[dir];
            float len = SDL_sqrtf(ux * ux + uy * uy);
            SDL_FRect arrow_rect;

            if (len <= 0.0f)
                continue;
            ux /= len;
            uy /= len;
            arrow_rect = (SDL_FRect){
                .x = cx + ux * mid - button_size * 0.5f,
                .y = cy + uy * mid - button_size * 0.5f,
                .w = button_size,
                .h = button_size,
            };
            sdl_touch_pane_draw_arrow(&arrow_rect, '0' + dir, wheel);
        }
    }

    margin = sdl_touch_pane_clampf((float)screen->w * 0.025f,
        18.0f, 36.0f);
    if (cx >= (float)screen->x + (float)screen->w * 0.5f) {
        panel_x = (float)screen->x + margin;
        panel_w = cx - radius - panel_x - margin;
    } else {
        panel_x = cx + radius + margin;
        panel_w = (float)(screen->x + screen->w) - panel_x - margin;
    }
    if (panel_w < 260.0f) {
        panel_x = (float)screen->x + margin;
        panel_w = (float)screen->w * 0.46f;
    }
    panel_y = header_bottom + sdl_touch_pane_clampf(
        (float)screen->h * 0.035f, 18.0f, 32.0f);

    sdl_touch_tutorial_draw_info_panel(screen,
        panel_x, panel_y, panel_w, "Button wheel controls",
        "Outer arrows: tap a direction to step.\nInner wheel: press and drag toward a direction, then release.\nCenter: tap to repeat the last direction.\nSwipe edge: reveal or hide the touch pane.\nQuick access: tap a button for its command; hold it for the long-touch command.\nDescription cards open above the bottom-center quick-access overlay.");

    sdl_touch_tutorial_draw_footer(screen, false, page_count == 1);
}

int sdl_touch_tutorial_wait_action(Uint64 accept_after_ns)
{
    SDL_Event ev;
    bool left_mouse_pressed = false;
    bool right_mouse_pressed = false;
    bool finger_pressed = false;
    SDL_FingerID active_finger = 0;

    for (;;) {
        Uint64 now_ns;

        sdl_music_update();
        if (!SDL_WaitEvent(&ev))
            continue;
        now_ns = SDL_GetTicksNS();

        if (sdl_sound_try_handle_event(&ev))
            continue;
#if SIL_SDL_MOBILE_BUILD
        if (sdl_mobile_lifecycle_handle_event(&ev))
            continue;
#endif

        if (ev.type == SDL_EVENT_QUIT) {
            Term_keypress(ESCAPE);
            return 2;
        }

        if (ev.type == SDL_EVENT_KEY_DOWN) {
            SDL_Keycode key = ev.key.key;

            if (now_ns < accept_after_ns)
                continue;
            if (sdl_key_is_escape_or_back(key) || key == 'q' || key == 'Q')
                return 2;
            if (key == SDLK_LEFT || key == SDLK_BACKSPACE
                || key == SDLK_PAGEUP)
            {
                return -1;
            }
            if (key == SDLK_RIGHT || key == SDLK_SPACE
                || key == SDLK_RETURN || key == SDLK_KP_ENTER
                || key == SDLK_PAGEDOWN)
            {
                return 1;
            }
            continue;
        }

        if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (now_ns < accept_after_ns)
                continue;
            if (ev.button.button == SDL_BUTTON_LEFT)
                left_mouse_pressed = true;
            else if (ev.button.button == SDL_BUTTON_RIGHT)
                right_mouse_pressed = true;
            continue;
        }

        if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (now_ns < accept_after_ns)
                continue;
            if (ev.button.button == SDL_BUTTON_LEFT && left_mouse_pressed)
                return 1;
            if (ev.button.button == SDL_BUTTON_RIGHT && right_mouse_pressed)
                return 2;
            continue;
        }

        if (ev.type == SDL_EVENT_FINGER_DOWN) {
            sdl_note_touch_event_device(ev.tfinger.touchID);
            if (now_ns < accept_after_ns)
                continue;
            finger_pressed = true;
            active_finger = ev.tfinger.fingerID;
            continue;
        }

        if (ev.type == SDL_EVENT_FINGER_UP) {
            sdl_note_touch_event_device(ev.tfinger.touchID);
            if (now_ns < accept_after_ns)
                continue;
            if (finger_pressed && ev.tfinger.fingerID == active_finger)
                return 1;
            continue;
        }

        if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            if (now_ns < accept_after_ns)
                continue;
            switch (ev.gbutton.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH:
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
                return 1;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
                return -1;
            case SDL_GAMEPAD_BUTTON_EAST:
            case SDL_GAMEPAD_BUTTON_BACK:
            case SDL_GAMEPAD_BUTTON_START:
                return 2;
            default:
                break;
            }
            continue;
        }

        if (ev.type == SDL_EVENT_WINDOW_RESIZED
            || ev.type == SDL_EVENT_WINDOW_SAFE_AREA_CHANGED
            || ev.type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED
            || ev.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED
            || ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
            || ev.type == SDL_EVENT_RENDER_DEVICE_RESET
            || ev.type == SDL_EVENT_RENDER_TARGETS_RESET
            || ev.type == SDL_EVENT_WINDOW_RESTORED
            || ev.type == SDL_EVENT_WINDOW_EXPOSED)
        {
            sdl_handle_event(&g_state, &ev);
            return 0;
        }
    }
}

void sdl_touch_tutorial_draw_page(int page, bool full, int page_count,
    bool mouse)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    bool old_suppress_top_panel;
    bool rendered;

    if (!sdl_rect_has_area(&screen))
        return;

    old_suppress_top_panel = g_touch_tutorial_suppress_runtime_top_panel;
    g_touch_tutorial_suppress_runtime_top_panel = true;
    rendered = sdl_render_current_window_frame();
    g_touch_tutorial_suppress_runtime_top_panel = old_suppress_top_panel;
    if (!rendered)
        return;

    if (!full) {
        sdl_touch_tutorial_draw_zones_page(&screen, 0, 1, mouse);
        return;
    }

    switch (page) {
    case 0:
        sdl_touch_tutorial_draw_zones_page(&screen, page, page_count, false);
        break;
    case 1:
    default:
        sdl_touch_tutorial_draw_buttonwheel_page(&screen, page, page_count);
        break;
    }
}

void sdl_touch_tutorial_prepare_snapshot(void)
{
    term* old = Term;

    if (p_ptr && p_ptr->playing && character_dungeon && character_icky == 0
        && g_views[PANE_MAIN].term_ready)
    {
        Term_activate(&g_views[PANE_MAIN].t);
        do_cmd_redraw();
        if (old && old != Term)
            Term_activate(old);
        return;
    }

    if (Term)
        Term_fresh();
}

void sdl_touch_tutorial_run(bool full, bool mouse)
{
    sdl_view* d;
    int page = 0;
    int page_count = full ? 2 : 1;
    bool done = false;
    Uint64 accept_after_ns;

    if (!g_state.window || !g_state.renderer)
        return;

    sdl_touch_tutorial_prepare_snapshot();
    d = sdl_view_from_term(Term);
    sdl_touch_cancel_all_inputs();
    accept_after_ns = SDL_GetTicksNS() + 250000000ULL;

    while (!done) {
        int action;

        if (page < 0)
            page = 0;
        if (page >= page_count)
            page = page_count - 1;

        sdl_touch_tutorial_draw_page(page, full, page_count, mouse);
        SDL_RenderPresent(g_state.renderer);
        sdl_restore_render_target(d);

        action = sdl_touch_tutorial_wait_action(accept_after_ns);
        if (action == 2) {
            done = true;
        } else if (action > 0) {
            if (page + 1 >= page_count)
                done = true;
            else
                page++;
            accept_after_ns = SDL_GetTicksNS() + 90000000ULL;
        } else if (action < 0) {
            if (page > 0)
                page--;
            accept_after_ns = SDL_GetTicksNS() + 90000000ULL;
        }
    }

    if (sdl_render_current_window_frame()) {
        SDL_RenderPresent(g_state.renderer);
        sdl_restore_render_target(d);
    }
    g_state.need_present = false;
    sdl_touch_cancel_all_inputs();
}

/* ------------------------------------------------------------------------
 * First-run character-creation coach
 *
 * Rather than separate text pages, the first-time tutorial draws a short set of
 * callouts directly over the real birth screens as the player reaches them
 * (selection -> attributes -> skills) and over the live character sheet.  The
 * underlying screen stays visible (lightly dimmed) so the guidance points at
 * what the player is actually looking at.  No prose - just short labels.
 * ------------------------------------------------------------------------ */

static bool g_birth_coach_ack[BIRTH_COACH_STAGE_MAX];

/*
 * A single coach step: a detailed callout pinned to one real on-screen block.
 * SHEET steps locate their block by panel heading; the birth/selection stages
 * locate theirs from the interactive-row hit rectangles.
 */
typedef struct birth_coach_step {
    cptr heading;   /* non-NULL: locate the block by panel heading */
    int  hit_lo;    /* heading == NULL: union the hits in [hit_lo, hit_hi] */
    int  hit_hi;
    cptr title;
    cptr body;      /* newline-separated callout lines */
} birth_coach_step;

/*
 * The live character sheet is toured block by block, in reading order.  Only
 * the blocks actually on screen are shown (e.g. "Combat" splits off "Vitals"
 * only on very wide layouts).
 */
static const birth_coach_step birth_coach_sheet_steps[] = {
    { "Vitals", 0, 0, "Vitals",
        "Your live status at a glance.\n"
        "Exp: spent / earned - the pool you spend on skills and abilities.\n"
        "Burden: weight carried / the most you can bear before slowing.\n"
        "Depth c/m: current depth / the shallowest you may climb back to.\n"
        "Health and Voice: your hit points and song points.\n"
        "Melee & Bows show (to-hit, damage); Armor shows [evasion, protection]." },
    { "Combat", 0, 0, "Combat numbers",
        "Your offence and defence, pulled out on wide screens.\n"
        "Melee / Bows: (to-hit bonus, damage dice).\n"
        "Armor: [evasion, protection] - dodge first, then soak.\n"
        "See the two Combat steps at the end for how these are used." },
    { "Traits", 0, 0, "Traits",
        "Innate strengths and flaws from your hero and house.\n"
        "++ mastery and + affinity make a skill cheaper and stronger.\n"
        "- and -- are penalties; UNIQUE marks a special power.\n"
        "Curses such as Doom of Mandos are shown in umber.\n"
        "Lean into your affinities and play around your curses." },
    { "Attributes", 0, 0, "Attributes",
        "Str, Dex, Con, Gra - the roots every skill grows from.\n"
        "Str: melee damage dice and carrying capacity.\n"
        "Dex: feeds melee, evasion, archery and stealth.\n"
        "Con: your hit points and resilience.\n"
        "Gra: feeds will, perception, song, smithing and voice.\n"
        "Read each as Current = Base +equip +misc -drain." },
    { "Skills", 0, 0, "Skills",
        "What you train by spending experience.\n"
        "Total = Base +stat +equip +misc.\n"
        "Melee / Archery: chance to hit.  Evasion: avoid being hit.\n"
        "Stealth / Perception: stay unseen and notice things.\n"
        "Will resists fear & magic; Smithing forges; Song sings powers.\n"
        "Click a skill, or press i, to raise it." },
    { "Skills", 0, 0, "Combat: attack & evasion",
        "Whether a blow lands is one opposed roll:\n"
        "  you: 1d20 + Melee   vs   them: 1d20 + Evasion.\n"
        "The higher total wins; a tie misses.\n"
        "Evasion is active dodging, so it is reduced when you are\n"
        "surrounded - fight in doorways and corridors to keep it.\n"
        "Archery uses the same roll, your Archery vs their Evasion.\n"
        "Beat their roll by a wide margin to land a critical hit,\n"
        "which rolls extra damage dice." },
    { "Skills", 0, 0, "Combat: damage & armour",
        "Damage is rolled only after a hit connects:\n"
        "  damage dice = weapon dice + Strength (capped by weapon weight).\n"
        "A foe's armour is shown as [Evasion, Protection].\n"
        "Protection rolls a value within that range each blow and is\n"
        "subtracted from your damage - only the excess wounds them.\n"
        "So heavy armour can shrug off small hits entirely;\n"
        "criticals (extra dice) are how you punch through it.\n"
        "Your own Armor line works the same way against their attacks." },
};

static const birth_coach_step birth_coach_select_step = {
    NULL, 0, 8999, "Choose your hero",
    "The screen shows description, traits and a power rating.\n"
    "A higher power rating means an easier start - ideal when you are new.\n"
    "Pick the hero whose strengths match the run you want."
};

static const birth_coach_step birth_coach_stats_step = {
    NULL, 0, 999, "Assign attributes",
    "Spend your points across Str, Dex, Con and Gra.\n"
    "Str: melee dice & capacity.  Dex: melee/evasion/archery/stealth.\n"
    "Con: hit points.  Gra: will/perception/song/smithing & voice.\n"
    "Cost = price of the next point; Points Left = your budget.\n"
    "Every point ripples into the skills shown alongside."
};

static const birth_coach_step birth_coach_skills_step = {
    NULL, 0, 999, "Buy skills",
    "Spend experience on the eight skills.\n"
    "Total = Base +stat +equip +misc.\n"
    "Base also sets how dear abilities are to buy later.\n"
    "Cost climbs the higher the skill; Points Left = your experience."
};

static cptr birth_coach_body_for_step(const birth_coach_step* step, char* buf,
    size_t buflen)
{
    char confirm_label[16];
    char back_label[16];

    if (!step)
        return "";
    if (!buf || buflen == 0)
        return step->body ? step->body : "";

    buf[0] = '\0';

    if (step == &birth_coach_select_step)
    {
        if (sdl_touch_only_device_active())
        {
            SDL_strlcpy(buf,
                "Swipe or tap the side arrows to browse heroes.\n"
                "Tap the hero name or Choose to confirm; Back returns to peoples.\n"
                "The screen shows description, traits and a power rating.\n"
                "A higher power rating means an easier start - ideal when you are new.\n"
                "Pick the hero whose strengths match the run you want.",
                buflen);
        }
        else if (steamdeck_controls_active())
        {
            sdl_touch_tutorial_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            sdl_touch_tutorial_prompt_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            strnfmt(buf, buflen,
                "D-pad/left stick Up/Down moves the highlight.\n"
                "Right or %s confirms; Left or %s returns to peoples.\n"
                "The screen shows description, traits and a power rating.\n"
                "A higher power rating means an easier start - ideal when you are new.\n"
                "Pick the hero whose strengths match the run you want.",
                confirm_label, back_label);
        }
        else
        {
            SDL_strlcpy(buf,
                "Use Up/Down to move the highlight, or click a hero to highlight it.\n"
                "Enter confirms; Esc returns to peoples.\n"
                "Click the highlighted hero again to confirm.\n"
                "The screen shows description, traits and a power rating.\n"
                "A higher power rating means an easier start - ideal when you are new.\n"
                "Pick the hero whose strengths match the run you want.",
                buflen);
        }
        return buf;
    }

    if (step == &birth_coach_stats_step)
    {
        if (sdl_touch_only_device_active())
        {
            SDL_strlcpy(buf,
                "Spend your points across Str, Dex, Con and Gra.\n"
                "Tap a stat to select it; tap it again to raise it.\n"
                "Long-tap a stat to lower it. Confirm accepts; Back returns to heroes.\n"
                "Str: melee dice & capacity.  Dex: melee/evasion/archery/stealth.\n"
                "Con: hit points.  Gra: will/perception/song/smithing & voice.\n"
                "Cost = price of the next point; Points Left = your budget.",
                buflen);
        }
        else if (steamdeck_controls_active())
        {
            sdl_touch_tutorial_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            sdl_touch_tutorial_prompt_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            strnfmt(buf, buflen,
                "Spend your points across Str, Dex, Con and Gra.\n"
                "D-pad Up/Down picks a stat; Left/Right lowers or raises it.\n"
                "%s accepts; %s returns to heroes.\n"
                "Str: melee dice & capacity.  Dex: melee/evasion/archery/stealth.\n"
                "Con: hit points.  Gra: will/perception/song/smithing & voice.\n"
                "Cost = price of the next point; Points Left = your budget.",
                confirm_label, back_label);
        }
        else
        {
            SDL_strlcpy(buf,
                "Spend your points across Str, Dex, Con and Gra.\n"
                "Up/Down picks a stat; Left/Right lowers or raises it.\n"
                "Enter accepts; Esc returns to heroes.\n"
                "Click a stat to select it; click again to raise; right-click lowers.\n"
                "Str: melee dice & capacity.  Dex: melee/evasion/archery/stealth.\n"
                "Con: hit points.  Gra: will/perception/song/smithing & voice.\n"
                "Cost = price of the next point; Points Left = your budget.",
                buflen);
        }
        return buf;
    }

    if (step == &birth_coach_skills_step)
    {
        if (sdl_touch_only_device_active())
        {
            SDL_strlcpy(buf,
                "Spend experience on the eight skills.\n"
                "Tap a skill to select it; tap it again to raise it.\n"
                "Long-tap a skill to lower it. Confirm accepts; Back returns to attributes.\n"
                "Total = Base +stat +equip +misc.\n"
                "Base also sets how dear abilities are to buy later.\n"
                "Cost climbs the higher the skill; Points Left = your experience.",
                buflen);
        }
        else if (steamdeck_controls_active())
        {
            sdl_touch_tutorial_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            sdl_touch_tutorial_prompt_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            strnfmt(buf, buflen,
                "Spend experience on the eight skills.\n"
                "D-pad Up/Down picks a skill; Left/Right lowers or raises it.\n"
                "%s accepts; %s returns to attributes.\n"
                "Total = Base +stat +equip +misc.\n"
                "Base also sets how dear abilities are to buy later.\n"
                "Cost climbs the higher the skill; Points Left = your experience.",
                confirm_label, back_label);
        }
        else
        {
            SDL_strlcpy(buf,
                "Spend experience on the eight skills.\n"
                "Up/Down picks a skill; Left/Right lowers or raises it.\n"
                "Enter accepts; Esc returns to attributes.\n"
                "Click a skill to select it; click again to raise; right-click lowers.\n"
                "Total = Base +stat +equip +misc.\n"
                "Base also sets how dear abilities are to buy later.\n"
                "Cost climbs the higher the skill; Points Left = your experience.",
                buflen);
        }
        return buf;
    }

    if (step->heading && streq(step->heading, "Skills")
        && step->title && streq(step->title, "Skills"))
    {
        if (sdl_touch_only_device_active())
        {
            SDL_strlcpy(buf,
                "What you train by spending experience.\n"
                "Total = Base +stat +equip +misc.\n"
                "Melee / Archery: chance to hit.  Evasion: avoid being hit.\n"
                "Stealth / Perception: stay unseen and notice things.\n"
                "Will resists fear & magic; Smithing forges; Song sings powers.\n"
                "Tap a skill once to focus it, then tap again or tap Increase to raise it.",
                buflen);
        }
        else if (steamdeck_controls_active())
        {
            sdl_touch_tutorial_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            strnfmt(buf, buflen,
                "What you train by spending experience.\n"
                "Total = Base +stat +equip +misc.\n"
                "Melee / Archery: chance to hit.  Evasion: avoid being hit.\n"
                "Stealth / Perception: stay unseen and notice things.\n"
                "Will resists fear & magic; Smithing forges; Song sings powers.\n"
                "D-pad or left stick moves focus; %s raises the focused skill.",
                confirm_label);
        }
        else
        {
            SDL_strlcpy(buf,
                "What you train by spending experience.\n"
                "Total = Base +stat +equip +misc.\n"
                "Melee / Archery: chance to hit.  Evasion: avoid being hit.\n"
                "Stealth / Perception: stay unseen and notice things.\n"
                "Will resists fear & magic; Smithing forges; Song sings powers.\n"
                "Click a skill twice, or press i/Space, to raise skills.",
                buflen);
        }
        return buf;
    }

    return step->body ? step->body : "";
}

/*
 * Gather the steps to show for a stage.  The live sheet contributes one step per
 * block that is actually on screen (located by panel heading); the birth and
 * selection stages are a single step.  Returns the number written to out[].
 */
static int birth_coach_collect_steps(int stage, const birth_coach_step** out,
    int max)
{
    int n = 0;

    if (!out || max <= 0)
        return 0;

    switch (stage) {
    case BIRTH_COACH_SHEET:
        for (size_t i = 0;
            i < N_ELEMENTS(birth_coach_sheet_steps) && n < max; i++)
        {
            SDL_FRect r;

            if (sdl_char_sheet_panel_rect(birth_coach_sheet_steps[i].heading,
                    &r))
                out[n++] = &birth_coach_sheet_steps[i];
        }
        break;
    case BIRTH_COACH_SELECT:
        out[n++] = &birth_coach_select_step;
        break;
    case BIRTH_COACH_STATS:
        out[n++] = &birth_coach_stats_step;
        break;
    case BIRTH_COACH_SKILLS:
        out[n++] = &birth_coach_skills_step;
        break;
    default:
        break;
    }

    return n;
}

/*
 * Union of the clickable-hit rectangles whose choice falls in [lo, hi].  This is
 * how the coach finds the real on-screen widgets to point at: the character
 * sheet registers a hit for every interactive row (attribute / skill / item /
 * hero) while sdl_render_current_window_frame() draws the live screen.
 */
static bool birth_coach_zone_from_hits(int lo, int hi, SDL_FRect* out)
{
    bool found = false;
    SDL_FRect box = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (!out)
        return false;

    for (int i = 0; i < g_sdl_character_sheet_screen.hit_count; i++) {
        const sdl_character_sheet_hit* h =
            &g_sdl_character_sheet_screen.hits[i];

        if (h->choice < lo || h->choice > hi)
            continue;
        if (h->rect.w <= 0.0f || h->rect.h <= 0.0f)
            continue;

        if (!found) {
            box = h->rect;
            found = true;
        } else {
            float right = MAX(box.x + box.w, h->rect.x + h->rect.w);
            float bottom = MAX(box.y + box.h, h->rect.y + h->rect.h);

            box.x = MIN(box.x, h->rect.x);
            box.y = MIN(box.y, h->rect.y);
            box.w = right - box.x;
            box.h = bottom - box.y;
        }
    }

    if (found)
        *out = box;
    return found;
}

/* Locate the on-screen rectangle a step points at (panel heading, or hits). */
static bool birth_coach_step_zone(const birth_coach_step* step, SDL_FRect* out)
{
    if (!step || !out)
        return false;
    if (step->heading)
        return sdl_char_sheet_panel_rect(step->heading, out);
    return birth_coach_zone_from_hits(step->hit_lo, step->hit_hi, out);
}

/*
 * The coach's own callout box: wider than the shared zone-prompt, left-aligned
 * body text, an auto-shrinking font and no 14-line cap, so detailed multi-line
 * explanations fit without being truncated.  Positioned beside the highlighted
 * block when one is given, otherwise centred under the title.
 */
static void birth_coach_draw_callout(const SDL_Rect* screen,
    const SDL_FRect* zone, cptr title, cptr body, float min_y)
{
    SDL_Color title_color = g_state.palette[TERM_YELLOW];
    SDL_Color text_color = g_state.palette[TERM_L_WHITE];
    char lines[48][SDL_TOUCH_TUTORIAL_LINE_LEN];
    SDL_FRect box;
    SDL_FRect shadow;
    TTF_Font* font = NULL;
    float pad = sdl_touch_pane_clampf((float)screen->h * 0.017f, 12.0f, 22.0f);
    float footer_top = (float)(screen->y + screen->h)
        - sdl_touch_pane_clampf((float)screen->h * 0.090f, 54.0f, 78.0f) - pad;
    float box_w;
    float text_w;
    float box_h = 0.0f;
    float line_h = 1.0f;
    float title_h;
    float avail_h;
    float y;
    int title_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.050f,
        28.0f, 44.0f);
    int detail_px = (int)sdl_touch_pane_clampf((float)screen->h * 0.038f,
        22.0f, 36.0f);
    int n = 0;

    box_w = (float)screen->w * 0.64f;
    if (box_w > 1160.0f)
        box_w = 1160.0f;
    if (box_w > (float)screen->w - pad * 2.0f)
        box_w = (float)screen->w - pad * 2.0f;
    if (box_w < 340.0f)
        box_w = (float)screen->w - pad * 2.0f;
    text_w = box_w - pad * 2.0f;
    if (text_w < 40.0f)
        return;

    avail_h = footer_top - min_y;
    if (avail_h < (float)screen->h * 0.40f)
        avail_h = (float)screen->h * 0.82f;

    /* Shrink the body font until the wrapped text fits the vertical budget. */
    for (;;) {
        font = sdl_story_font_for_height(detail_px);
        n = sdl_touch_tutorial_wrap_lines(body, font, text_w, lines,
            (int)N_ELEMENTS(lines));
        line_h = (float)detail_px * 1.30f;
        title_h = (float)title_px * 1.25f;
        box_h = pad * 2.0f + title_h + 6.0f + (float)n * line_h;
        if (box_h <= avail_h || detail_px <= 18)
            break;
        detail_px--;
        if (title_px > detail_px + 8)
            title_px--;
    }

    /* Place beside the block: below it if there is room, else above, else at the
     * top of the free area; centred when no block is supplied. */
    if (zone && zone->w > 1.0f && zone->h > 1.0f) {
        box.x = zone->x + zone->w * 0.5f - box_w * 0.5f;
        if (zone->y + zone->h + box_h + pad <= footer_top)
            box.y = zone->y + zone->h + pad;
        else if (zone->y - box_h - pad >= min_y)
            box.y = zone->y - box_h - pad;
        else
            box.y = min_y;
    } else {
        box.x = (float)screen->x + ((float)screen->w - box_w) * 0.5f;
        box.y = min_y;
    }
    box.w = box_w;
    box.h = box_h;
    sdl_touch_tutorial_clamp_box_to_screen(&box, screen, pad);

    shadow = box;
    shadow.x += 3.0f;
    shadow.y += 3.0f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 170);
    SDL_RenderFillRect(g_state.renderer, &shadow);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 230);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, title_color.r, title_color.g,
        title_color.b, 242);
    SDL_RenderRect(g_state.renderer, &box);

    y = box.y + pad;
    (void)sdl_touch_tutorial_draw_text_line(title, box.x + box.w * 0.5f, y,
        text_w, title_px, title_color, true);
    y += title_h + 6.0f;
    for (int i = 0; i < n; i++) {
        (void)sdl_touch_tutorial_draw_text_line(lines[i], box.x + pad, y,
            text_w, detail_px, text_color, false);
        y += line_h;
    }
}

/* Draw one step: dim the screen, frame the target block, and pin a detailed
 * callout to it.  Falls back to a centred callout if the block is not found. */
static void birth_coach_draw_step(const SDL_Rect* screen, bool mouse,
    bool single_page, const SDL_FRect* zone, cptr title, cptr body)
{
    float min_y;

    if (!screen)
        return;

    sdl_touch_tutorial_draw_screen_dim(screen, 150);

    /* Keep callouts clear of the screen's own title banner at the very top. */
    min_y = (float)screen->y + sdl_touch_pane_clampf((float)screen->h * 0.12f,
        40.0f, 120.0f);

    if (zone && zone->w > 1.0f && zone->h > 1.0f) {
        SDL_FRect z = *zone;
        float pad = sdl_touch_pane_clampf((float)screen->h * 0.010f, 4.0f, 10.0f);

        z.x -= pad;
        z.y -= pad;
        z.w += pad * 2.0f;
        z.h += pad * 2.0f;
        sdl_touch_tutorial_clamp_box_to_screen(&z, screen, 2.0f);

        sdl_touch_tutorial_draw_zone_highlight(&z);
        birth_coach_draw_callout(screen, &z, title, body, min_y);
    } else {
        birth_coach_draw_callout(screen, NULL, title, body, min_y);
    }

    sdl_touch_tutorial_draw_footer(screen, mouse, single_page);
}

static void birth_coach_run_overlay(int stage)
{
    sdl_view* d;
    Uint64 accept_after_ns;
    bool mouse;
    int idx = 0;

    if (!g_state.window || !g_state.renderer)
        return;

    mouse = !sdl_touch_tutorial_device_available();
    sdl_touch_cancel_all_inputs();
    d = sdl_view_from_term(Term);
    /* Brief guard so the tap/click that opened the coach does not dismiss it. */
    accept_after_ns = SDL_GetTicksNS() + 250000000ULL;

    for (;;) {
        SDL_Rect screen = sdl_get_layout_screen_rect();
        const birth_coach_step* steps[8];
        SDL_FRect zone = { 0.0f, 0.0f, 0.0f, 0.0f };
        char title[80];
        char body[2048];
        cptr body_text;
        bool have_zone;
        int count;
        int action;

        if (!sdl_rect_has_area(&screen))
            break;
        /* Draw the live screen first: this also re-registers the panel/hit
         * rectangles the coach points at. */
        if (!sdl_render_current_window_frame())
            break;

        count = birth_coach_collect_steps(stage, steps,
            (int)N_ELEMENTS(steps));
        if (count <= 0)
            break;
        if (idx >= count)
            idx = count - 1;
        if (idx < 0)
            idx = 0;

        have_zone = birth_coach_step_zone(steps[idx], &zone);

        if (count > 1)
            strnfmt(title, sizeof(title), "%s  (%d/%d)", steps[idx]->title,
                idx + 1, count);
        else
            SDL_strlcpy(title, steps[idx]->title, sizeof(title));

        body_text = birth_coach_body_for_step(steps[idx], body,
            sizeof(body));
        birth_coach_draw_step(&screen, mouse, count == 1,
            have_zone ? &zone : NULL, title, body_text);
        SDL_RenderPresent(g_state.renderer);
        sdl_restore_render_target(d);

        action = sdl_touch_tutorial_wait_action(accept_after_ns);
        /* Re-arm the guard so a held key/tap does not skip several steps. */
        accept_after_ns = SDL_GetTicksNS() + 90000000ULL;

        if (action == 2) {
            break;                       /* Esc / quit closes the tour */
        } else if (action > 0) {
            if (idx + 1 >= count)
                break;                   /* past the last step -> close */
            idx++;
        } else if (action < 0) {
            if (idx > 0)
                idx--;                   /* page back */
        }
        /* action == 0: window resize or redraw request -> draw again */
    }

    /* Restore the underlying screen without the overlay. */
    if (sdl_render_current_window_frame()) {
        SDL_RenderPresent(g_state.renderer);
        sdl_restore_render_target(d);
    }
    g_state.need_present = false;
    sdl_touch_cancel_all_inputs();
}

void birth_coach_show(int stage)
{
    birth_coach_run_overlay(stage);
}

void birth_coach_show_once(int stage)
{
    if (stage < 0 || stage >= BIRTH_COACH_STAGE_MAX)
        return;
    if (g_birth_coach_ack[stage])
        return;
    /* Only first-time players (empty score file) get the guided coach. */
    if (!highscore_is_empty())
        return;

    g_birth_coach_ack[stage] = true;
    birth_coach_run_overlay(stage);
}

enum {
    SDL_TOUCH_TUTORIAL_CHOICE_TOUCH_PANE = 0,
    SDL_TOUCH_TUTORIAL_CHOICE_CORNERS,
    SDL_TOUCH_TUTORIAL_CHOICE_ROUND_WHEEL,
    SDL_TOUCH_TUTORIAL_CHOICE_REPLAY,
    SDL_TOUCH_TUTORIAL_CHOICE_COUNT,
    SDL_TOUCH_TUTORIAL_CHOICE_CANCEL = -1
};

typedef struct sdl_touch_tutorial_choice {
    int result;
    cptr title;
    cptr body;
} sdl_touch_tutorial_choice;

const sdl_touch_tutorial_choice sdl_touch_tutorial_choices[] = {
    {
        SDL_TOUCH_PROFILE_TOUCH_PANE,
        "Touch pane + touch screen",
        "Visible command pad with movement and common actions on screen. Best for phones and first touch games."
    },
    {
        SDL_TOUCH_PROFILE_CORNERS,
        "Corners + quick access",
        "Side corner movement zones and a short quick-access command pane. Best when you want more map space."
    },
    {
        SDL_TOUCH_PROFILE_ROUND_WHEEL,
        "Button wheel + quick access",
        "Outer movement buttons with an inner drag wheel and a longer quick-access command pane. Best for one-thumb movement once you know the layout."
    },
    {
        SDL_TOUCH_TUTORIAL_CHOICE_REPLAY,
        "Start tutorial again",
        "Replay the touch tutorial before choosing a preset."
    },
};

int sdl_touch_tutorial_current_choice_index(void)
{
    int profile = get_sdl_touch_profile();

    for (int i = 0; i < (int)N_ELEMENTS(sdl_touch_tutorial_choices); i++) {
        if (sdl_touch_tutorial_choices[i].result == profile)
            return i;
    }

    return SDL_TOUCH_TUTORIAL_CHOICE_TOUCH_PANE;
}

void sdl_touch_tutorial_choice_layout(const SDL_Rect* screen,
    SDL_FRect choice_rects[SDL_TOUCH_TUTORIAL_CHOICE_COUNT])
{
    bool grid;
    float margin;
    float max_w;
    float x;
    float top;
    float bottom;
    float gap;
    float card_w;
    float card_h;

    if (!screen || !choice_rects)
        return;

    margin = sdl_touch_pane_clampf((float)screen->w * 0.045f,
        18.0f, 54.0f);
    max_w = (float)screen->w - margin * 2.0f;
    if (max_w > 980.0f)
        max_w = 980.0f;
    x = (float)screen->x + ((float)screen->w - max_w) * 0.5f;

    top = (float)screen->y + sdl_touch_pane_clampf(
        (float)screen->h * 0.245f, 118.0f, 190.0f)
        + sdl_touch_tutorial_top_reserved_height(screen);
    bottom = (float)(screen->y + screen->h)
        - sdl_touch_pane_clampf((float)screen->h * 0.115f, 62.0f, 92.0f);
    if (bottom <= top + 80.0f)
        bottom = (float)(screen->y + screen->h) - 46.0f;
    if (bottom <= top + 80.0f)
        top = (float)screen->y + 88.0f;

    gap = sdl_touch_pane_clampf((float)screen->h * 0.018f, 8.0f, 16.0f);
    grid = (screen->w >= 760 && screen->h >= 430);

    if (grid) {
        card_w = (max_w - gap) * 0.5f;
        card_h = (bottom - top - gap) * 0.5f;
        for (int i = 0; i < SDL_TOUCH_TUTORIAL_CHOICE_COUNT; i++) {
            int row = i / 2;
            int col = i % 2;

            choice_rects[i] = (SDL_FRect){
                .x = x + (card_w + gap) * (float)col,
                .y = top + (card_h + gap) * (float)row,
                .w = card_w,
                .h = card_h,
            };
        }
    } else {
        card_w = max_w;
        card_h = (bottom - top
            - gap * (float)(SDL_TOUCH_TUTORIAL_CHOICE_COUNT - 1))
            / (float)SDL_TOUCH_TUTORIAL_CHOICE_COUNT;

        for (int i = 0; i < SDL_TOUCH_TUTORIAL_CHOICE_COUNT; i++) {
            choice_rects[i] = (SDL_FRect){
                .x = x,
                .y = top + (card_h + gap) * (float)i,
                .w = card_w,
                .h = card_h,
            };
        }
    }
}

void sdl_touch_tutorial_draw_choice_card(const SDL_FRect* rect,
    const sdl_touch_tutorial_choice* choice, int index, bool highlighted,
    bool current)
{
    SDL_Color title_color = g_state.palette[TERM_YELLOW];
    SDL_Color body_color = g_state.palette[TERM_L_WHITE];
    SDL_Color border_color = highlighted
        ? g_state.palette[TERM_L_BLUE]
        : (current ? g_state.palette[TERM_YELLOW]
                   : g_state.palette[TERM_L_WHITE]);
    SDL_FRect shadow;
    float pad;
    float text_x;
    float text_w;
    float y;
    int title_px;
    int body_px;
    char title[96];

    if (!rect || !choice || rect->w <= 1.0f || rect->h <= 1.0f)
        return;

    pad = sdl_touch_pane_clampf(rect->h * 0.13f, 8.0f, 17.0f);
    title_px = (int)sdl_touch_pane_clampf(rect->h * 0.205f, 17.0f, 28.0f);
    body_px = (int)sdl_touch_pane_clampf(rect->h * 0.148f, 13.0f, 21.0f);

    shadow = *rect;
    shadow.x += 3.0f;
    shadow.y += 3.0f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 150);
    SDL_RenderFillRect(g_state.renderer, &shadow);
    if (highlighted)
        SDL_SetRenderDrawColor(g_state.renderer, 24, 84, 138, 218);
    else
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 222);
    SDL_RenderFillRect(g_state.renderer, rect);
    SDL_SetRenderDrawColor(g_state.renderer, border_color.r, border_color.g,
        border_color.b, 238);
    SDL_RenderRect(g_state.renderer, rect);

    if (current && choice->result >= 0)
        strnfmt(title, sizeof(title), "%d. %s (Current)", index + 1,
            choice->title);
    else
        strnfmt(title, sizeof(title), "%d. %s", index + 1, choice->title);
    text_x = rect->x + pad;
    text_w = rect->w - pad * 2.0f;
    y = rect->y + pad;

    (void)sdl_touch_tutorial_draw_text_line(title, text_x, y, text_w,
        title_px, title_color, false);

    if (rect->h >= pad * 2.0f + (float)title_px * 1.25f
            + (float)body_px * 1.55f)
    {
        y += (float)title_px * 1.35f;
        (void)sdl_touch_tutorial_draw_wrapped(choice->body, text_x, y,
            text_w, body_px, body_color);
    }
}

bool sdl_touch_tutorial_draw_profile_choice_screen(int highlighted,
    SDL_FRect choice_rects[SDL_TOUCH_TUTORIAL_CHOICE_COUNT])
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    SDL_Color title_color = g_state.palette[TERM_YELLOW];
    SDL_Color text_color = g_state.palette[TERM_L_WHITE];
    sdl_view* d = sdl_view_from_term(Term);
    bool old_suppress_top_panel;
    bool rendered;
    float x;
    float y;
    float max_w;
    int title_px;
    int body_px;
    int footer_px;
    int current_index;

    if (!sdl_rect_has_area(&screen))
        return false;

    old_suppress_top_panel = g_touch_tutorial_suppress_runtime_top_panel;
    g_touch_tutorial_suppress_runtime_top_panel = true;
    rendered = sdl_render_current_window_frame();
    g_touch_tutorial_suppress_runtime_top_panel = old_suppress_top_panel;
    if (!rendered)
        return false;

    sdl_touch_tutorial_draw_screen_dim(&screen, 172);
    x = (float)screen.x + (float)screen.w * 0.5f;
    y = sdl_touch_tutorial_default_header_y(&screen);
    max_w = (float)screen.w * 0.86f;
    title_px = (int)sdl_touch_pane_clampf((float)screen.h * 0.050f,
        28.0f, 46.0f);
    body_px = (int)sdl_touch_pane_clampf((float)screen.h * 0.030f,
        18.0f, 28.0f);
    footer_px = (int)sdl_touch_pane_clampf((float)screen.h * 0.028f,
        16.0f, 24.0f);

    y += sdl_touch_tutorial_draw_text_line("Choose Touch Preset", x, y,
        max_w, title_px, title_color, true);
    y += 5.0f;
    (void)sdl_touch_tutorial_draw_wrapped_centered(
        "Pick the control layout to use now, or replay the tutorial before choosing.",
        x, y, max_w, body_px, text_color);

    sdl_touch_tutorial_choice_layout(&screen, choice_rects);
    current_index = sdl_touch_tutorial_current_choice_index();
    for (int i = 0; i < (int)N_ELEMENTS(sdl_touch_tutorial_choices); i++) {
        sdl_touch_tutorial_draw_choice_card(&choice_rects[i],
            &sdl_touch_tutorial_choices[i], i, highlighted == i,
            current_index == i);
    }

    y = (float)(screen.y + screen.h)
        - sdl_touch_pane_clampf((float)screen.h * 0.076f, 42.0f, 62.0f);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 185);
    SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
        .x = (float)screen.x,
        .y = y - 8.0f,
        .w = (float)screen.w,
        .h = (float)(screen.y + screen.h) - y + 8.0f,
    });
    (void)sdl_touch_tutorial_draw_text_line(
        "Tap/click a choice   Up/Down selects   Enter applies   Esc keeps current",
        x, y, (float)screen.w * 0.92f, footer_px, text_color, true);

    SDL_RenderPresent(g_state.renderer);
    sdl_restore_render_target(d);
    g_state.need_present = false;
    return true;
}

int sdl_touch_tutorial_choice_hit(
    const SDL_FRect choice_rects[SDL_TOUCH_TUTORIAL_CHOICE_COUNT],
    float x, float y)
{
    if (!choice_rects)
        return -1;

    for (int i = 0; i < SDL_TOUCH_TUTORIAL_CHOICE_COUNT; i++) {
        if (sdl_point_in_frect(&choice_rects[i], x, y))
            return i;
    }

    return -1;
}

int sdl_touch_tutorial_choose_profile(void)
{
    SDL_Event ev;
    SDL_FRect choice_rects[SDL_TOUCH_TUTORIAL_CHOICE_COUNT] = { 0 };
    int highlighted = sdl_touch_tutorial_current_choice_index();
    int pressed_choice = -1;
    SDL_FingerID active_finger = 0;
    bool finger_pressed = false;
    bool mouse_pressed = false;
    bool redraw = true;
    Uint64 accept_after_ns = SDL_GetTicksNS() + 250000000ULL;

    sdl_touch_cancel_all_inputs();

    for (;;) {
        Uint64 now_ns;

        if (redraw) {
            if (!sdl_touch_tutorial_draw_profile_choice_screen(highlighted,
                    choice_rects))
            {
                return SDL_TOUCH_TUTORIAL_CHOICE_CANCEL;
            }
            redraw = false;
        }

        sdl_music_update();
        if (!SDL_WaitEvent(&ev))
            continue;
        now_ns = SDL_GetTicksNS();

        if (sdl_sound_try_handle_event(&ev))
            continue;
#if SIL_SDL_MOBILE_BUILD
        if (sdl_mobile_lifecycle_handle_event(&ev))
            continue;
#endif

        if (ev.type == SDL_EVENT_QUIT) {
            Term_keypress(ESCAPE);
            return SDL_TOUCH_TUTORIAL_CHOICE_CANCEL;
        }

        if (ev.type == SDL_EVENT_KEY_DOWN) {
            SDL_Keycode key = ev.key.key;

            if (now_ns < accept_after_ns)
                continue;
            if (sdl_key_is_escape_or_back(key) || key == 'q' || key == 'Q')
                return SDL_TOUCH_TUTORIAL_CHOICE_CANCEL;
            if (key == SDLK_UP || key == SDLK_LEFT
                || key == SDLK_PAGEUP || key == SDLK_KP_8
                || key == SDLK_KP_4)
            {
                highlighted = (SDL_TOUCH_TUTORIAL_CHOICE_COUNT
                    + highlighted - 1) % SDL_TOUCH_TUTORIAL_CHOICE_COUNT;
                redraw = true;
                continue;
            }
            if (key == SDLK_DOWN || key == SDLK_RIGHT
                || key == SDLK_PAGEDOWN || key == SDLK_KP_2
                || key == SDLK_KP_6)
            {
                highlighted = (highlighted + 1)
                    % SDL_TOUCH_TUTORIAL_CHOICE_COUNT;
                redraw = true;
                continue;
            }
            if (key == SDLK_SPACE || key == SDLK_RETURN
                || key == SDLK_KP_ENTER)
            {
                return sdl_touch_tutorial_choices[highlighted].result;
            }
            if (key == '1')
                return sdl_touch_tutorial_choices[0].result;
            if (key == '2')
                return sdl_touch_tutorial_choices[1].result;
            if (key == '3')
                return sdl_touch_tutorial_choices[2].result;
            if (key == '4' || key == 'r' || key == 'R')
            {
                return SDL_TOUCH_TUTORIAL_CHOICE_REPLAY;
            }
            continue;
        }

        if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            int hit;

            if (now_ns < accept_after_ns)
                continue;
            if (ev.button.windowID != SDL_GetWindowID(g_state.window))
                continue;
            if (ev.button.button != SDL_BUTTON_LEFT)
                continue;

            hit = sdl_touch_tutorial_choice_hit(choice_rects,
                ev.button.x, ev.button.y);
            if (hit >= 0) {
                pressed_choice = hit;
                highlighted = hit;
                mouse_pressed = true;
                redraw = true;
            }
            continue;
        }

        if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            int hit;

            if (now_ns < accept_after_ns)
                continue;
            if (ev.button.windowID != SDL_GetWindowID(g_state.window))
                continue;
            if (ev.button.button != SDL_BUTTON_LEFT)
                continue;

            hit = sdl_touch_tutorial_choice_hit(choice_rects,
                ev.button.x, ev.button.y);
            if (mouse_pressed && hit >= 0 && hit == pressed_choice)
                return sdl_touch_tutorial_choices[hit].result;
            mouse_pressed = false;
            pressed_choice = -1;
            continue;
        }

        if (ev.type == SDL_EVENT_FINGER_DOWN) {
            float x;
            float y;
            int hit;

            if (ev.tfinger.windowID != SDL_GetWindowID(g_state.window))
                continue;
            sdl_note_touch_event_device(ev.tfinger.touchID);
            if (now_ns < accept_after_ns)
                continue;
            if (!sdl_finger_event_to_render_coords(&ev.tfinger, &x, &y))
                continue;
            hit = sdl_touch_tutorial_choice_hit(choice_rects, x, y);
            if (hit >= 0) {
                pressed_choice = hit;
                highlighted = hit;
                active_finger = ev.tfinger.fingerID;
                finger_pressed = true;
                redraw = true;
            }
            continue;
        }

        if (ev.type == SDL_EVENT_FINGER_MOTION) {
            float x;
            float y;
            int hit;

            if (ev.tfinger.windowID != SDL_GetWindowID(g_state.window))
                continue;
            sdl_note_touch_event_device(ev.tfinger.touchID);
            if (!finger_pressed
                || ev.tfinger.fingerID != active_finger)
            {
                continue;
            }

            if (!sdl_finger_event_to_render_coords(&ev.tfinger, &x, &y))
                continue;
            hit = sdl_touch_tutorial_choice_hit(choice_rects, x, y);
            if (hit != highlighted && hit >= 0) {
                highlighted = hit;
                redraw = true;
            }
            continue;
        }

        if (ev.type == SDL_EVENT_FINGER_UP) {
            float x;
            float y;
            int hit;

            if (ev.tfinger.windowID != SDL_GetWindowID(g_state.window))
                continue;
            sdl_note_touch_event_device(ev.tfinger.touchID);
            if (now_ns < accept_after_ns)
                continue;
            if (!finger_pressed || ev.tfinger.fingerID != active_finger)
                continue;

            if (!sdl_finger_event_to_render_coords(&ev.tfinger, &x, &y))
                continue;
            hit = sdl_touch_tutorial_choice_hit(choice_rects, x, y);
            if (hit >= 0 && hit == pressed_choice)
                return sdl_touch_tutorial_choices[hit].result;
            finger_pressed = false;
            pressed_choice = -1;
            continue;
        }

        if (ev.type == SDL_EVENT_FINGER_CANCELED) {
            if (ev.tfinger.windowID != SDL_GetWindowID(g_state.window))
                continue;
            sdl_note_touch_event_device(ev.tfinger.touchID);
            finger_pressed = false;
            pressed_choice = -1;
            continue;
        }

        if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            if (now_ns < accept_after_ns)
                continue;
            switch (ev.gbutton.button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP:
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
                highlighted = (SDL_TOUCH_TUTORIAL_CHOICE_COUNT
                    + highlighted - 1) % SDL_TOUCH_TUTORIAL_CHOICE_COUNT;
                redraw = true;
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
                highlighted = (highlighted + 1)
                    % SDL_TOUCH_TUTORIAL_CHOICE_COUNT;
                redraw = true;
                break;
            case SDL_GAMEPAD_BUTTON_SOUTH:
            case SDL_GAMEPAD_BUTTON_START:
                return sdl_touch_tutorial_choices[highlighted].result;
            case SDL_GAMEPAD_BUTTON_EAST:
            case SDL_GAMEPAD_BUTTON_BACK:
                return SDL_TOUCH_TUTORIAL_CHOICE_CANCEL;
            default:
                break;
            }
            continue;
        }

        if (ev.type == SDL_EVENT_WINDOW_RESIZED
            || ev.type == SDL_EVENT_WINDOW_SAFE_AREA_CHANGED
            || ev.type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED
            || ev.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED
            || ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
            || ev.type == SDL_EVENT_RENDER_DEVICE_RESET
            || ev.type == SDL_EVENT_RENDER_TARGETS_RESET
            || ev.type == SDL_EVENT_WINDOW_RESTORED
            || ev.type == SDL_EVENT_WINDOW_EXPOSED)
        {
            sdl_handle_event(&g_state, &ev);
            redraw = true;
            continue;
        }
    }
}

void sdl_touch_tutorial_save_profile_choice(int profile)
{
    if (profile < 0)
        return;

    sdl_touch_apply_profile(profile);
    if (config_file_path[0] != '\0') {
        sdl_store_active_pane_profile(config.min_terminal_mode);
        sdl_config_save(config_file_path, &config, g_pane_profiles,
            SDL_PANE_PROFILE_COUNT);
    }
}

void sdl_touch_tutorial_run_fixed(void)
{
    bool full = sdl_touch_tutorial_full_mode();

    if (sdl_touch_only_mobile_device_active())
        sdl_touch_apply_profile(SDL_TOUCH_PROFILE_ROUND_WHEEL);
    sdl_touch_tutorial_run(full, false);
}

void sdl_touch_mark_tutorial_seen_and_save(void)
{
    if (sdl_config_touch_tutorial_seen())
        return;

    sdl_config_mark_touch_tutorial_seen();
    if (config_file_path[0] != '\0') {
        sdl_store_active_pane_profile(config.min_terminal_mode);
        sdl_config_save(config_file_path, &config, g_pane_profiles,
            SDL_PANE_PROFILE_COUNT);
    }
}

void sdl_mouse_mark_tutorial_seen_and_save(void)
{
    if (sdl_config_mouse_tutorial_seen())
        return;

    sdl_config_mark_mouse_tutorial_seen();
    if (config_file_path[0] != '\0') {
        sdl_store_active_pane_profile(config.min_terminal_mode);
        sdl_config_save(config_file_path, &config, g_pane_profiles,
            SDL_PANE_PROFILE_COUNT);
    }
}

/* ------------------------------------------------------------------------
 * First-run character action-wheel coach
 *
 * Opens the live player action wheel (the radial menu centred on the player)
 * and pins one detailed callout to it, with the open / navigate / activate
 * instructions matched to the active input device.  Shown once on the first
 * game start, like the other first-run tutorials.
 * ------------------------------------------------------------------------ */

enum {
    SDL_WHEEL_COACH_INPUT_MOUSE = 0,
    SDL_WHEEL_COACH_INPUT_TOUCH,
    SDL_WHEEL_COACH_INPUT_CONTROLLER
};

static int sdl_character_wheel_coach_input(void)
{
    if (sdl_touch_only_device_active())
        return SDL_WHEEL_COACH_INPUT_TOUCH;
    if (steamdeck_controls_active())
        return SDL_WHEEL_COACH_INPUT_CONTROLLER;
    return SDL_WHEEL_COACH_INPUT_MOUSE;
}

static cptr sdl_character_wheel_coach_body(int input, char* buf, size_t buflen)
{
    char confirm_label[16];
    char back_label[16];

    switch (input) {
    case SDL_WHEEL_COACH_INPUT_TOUCH:
        SDL_strlcpy(buf,
            "Everything you can do while standing on your square - wait, use an "
            "item, ready your bow, sing, and more - lives on this wheel.\n"
            "Open it: tap your own square on the map.\n"
            "Choose: drag to a wedge and lift your finger to run that action.\n"
            "Second action: a wedge's outer ring holds a related action.\n"
            "Close: tap the centre, or tap outside the wheel.",
            buflen);
        return buf;
    case SDL_WHEEL_COACH_INPUT_CONTROLLER:
        sdl_touch_tutorial_prompt_label(steamdeck_confirm_key(), "A",
            confirm_label, sizeof(confirm_label));
        sdl_touch_tutorial_prompt_label(steamdeck_back_key(), "B",
            back_label, sizeof(back_label));
        strnfmt(buf, buflen,
            "Everything you can do while standing on your square - wait, use an "
            "item, ready your bow, sing, and more - lives on this wheel.\n"
            "Open it: press and hold %s while standing still.\n"
            "Choose: D-pad Left/Right turns the ring; Up/Down reaches the outer "
            "ring of second actions.\n"
            "Run it: press %s on the highlighted wedge.\n"
            "Close: press %s or Start.",
            confirm_label, confirm_label, back_label);
        return buf;
    default:
        SDL_strlcpy(buf,
            "Everything you can do while standing on your square - wait, use an "
            "item, ready your bow, sing, and more - lives on this wheel.\n"
            "Open it: right-click your own square on the map.\n"
            "Choose: move the cursor to a wedge and left-click to run that action.\n"
            "Second action: a wedge's outer ring holds a related action.\n"
            "Close: right-click again, or press Esc.",
            buflen);
        return buf;
    }
}

/* Returns true once the wheel was actually opened and the coach shown. */
static bool sdl_character_wheel_coach_run(void)
{
    sdl_view* d;
    Uint64 accept_after_ns;
    int input;
    bool shown = false;

    if (!g_state.window || !g_state.renderer)
        return false;
    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_player_action_menu_open())
        return false;

    input = sdl_character_wheel_coach_input();
    d = sdl_view_from_term(Term);
    sdl_touch_cancel_all_inputs();
    /* Brief guard so the tap/click that began the turn does not dismiss it. */
    accept_after_ns = SDL_GetTicksNS() + 250000000ULL;

    for (;;) {
        SDL_Rect screen = sdl_get_layout_screen_rect();
        player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
        SDL_FRect zone = { 0.0f, 0.0f, 0.0f, 0.0f };
        char body[1024];
        cptr body_text;
        bool have_zone;
        float min_y;
        int count = 0;
        int action;

        if (!sdl_rect_has_area(&screen))
            break;
        /* Draw the live screen first; this also draws the open wheel. */
        if (!sdl_render_current_window_frame())
            break;

        /* Dim, then redraw the wheel on top so it stays bright and readable. */
        sdl_touch_tutorial_draw_screen_dim(&screen, 158);
        sdl_player_action_menu_render();

        have_zone = sdl_player_action_menu_layout(entries, &count)
            && count > 0;
        if (have_zone) {
            float r = entries[0].outer_radius;

            zone = (SDL_FRect){
                .x = entries[0].center_x - r,
                .y = entries[0].center_y - r,
                .w = r * 2.0f,
                .h = r * 2.0f,
            };
        }

        min_y = (float)screen.y + sdl_touch_pane_clampf(
            (float)screen.h * 0.12f, 40.0f, 120.0f);
        body_text = sdl_character_wheel_coach_body(input, body, sizeof(body));
        birth_coach_draw_callout(&screen, have_zone ? &zone : NULL,
            "Action wheel", body_text, min_y);
        sdl_touch_tutorial_draw_footer(&screen,
            input == SDL_WHEEL_COACH_INPUT_MOUSE, true);

        SDL_RenderPresent(g_state.renderer);
        sdl_restore_render_target(d);
        shown = true;

        action = sdl_touch_tutorial_wait_action(accept_after_ns);
        /* Re-arm the guard so a held key/tap does not skip ahead. */
        accept_after_ns = SDL_GetTicksNS() + 90000000ULL;
        if (action != 0)
            break;       /* any confirm/back/cancel closes this single page */
        /* action == 0: window resize or redraw request -> draw again */
    }

    sdl_player_action_menu_cancel();
    if (sdl_render_current_window_frame()) {
        SDL_RenderPresent(g_state.renderer);
        sdl_restore_render_target(d);
    }
    g_state.need_present = false;
    sdl_touch_cancel_all_inputs();
    return shown;
}

void sdl_character_wheel_mark_tutorial_seen_and_save(void)
{
    if (sdl_config_character_wheel_tutorial_seen())
        return;

    sdl_config_mark_character_wheel_tutorial_seen();
    if (config_file_path[0] != '\0') {
        sdl_store_active_pane_profile(config.min_terminal_mode);
        sdl_config_save(config_file_path, &config, g_pane_profiles,
            SDL_PANE_PROFILE_COUNT);
    }
}

void sdl_character_wheel_maybe_show_first_game_tutorial(void)
{
    if (sdl_config_character_wheel_tutorial_seen())
        return;
    /* The wheel can only be opened on the live main screen; wait for it. */
    if (!sdl_main_screen_click_shortcuts_active())
        return;

    if (!sdl_character_wheel_coach_run())
        return;            /* could not open the wheel yet; retry next frame */

    sdl_character_wheel_mark_tutorial_seen_and_save();
}

void sdl_touch_request_tutorial_from_settings(void)
{
    g_touch_tutorial_requested_from_settings = true;
}

void sdl_mouse_request_tutorial_from_settings(void)
{
    g_mouse_tutorial_requested_from_settings = true;
}

void sdl_character_wheel_request_tutorial_from_settings(void)
{
    g_character_wheel_tutorial_requested_from_settings = true;
}

void sdl_zones_request_tutorial_from_settings(void)
{
    g_zones_tutorial_requested_from_settings = true;
}

bool sdl_zones_settings_tutorial_requested(void)
{
    return g_zones_tutorial_requested_from_settings;
}

/* The zones overview ("Default Touch Layout" / "Main Screen Mouse Controls")
 * shown as a single page, on demand from Input Options.  Mouse language is used
 * off mobile, tap language on mobile. */
void sdl_zones_show_tutorial(void)
{
    sdl_touch_tutorial_run(false, !sdl_touch_tutorial_full_mode());
}

/* Show the requested zones tutorial immediately from a clean command context
 * (the Options command, after screen_load).  Unlike the deferred Term_xtra
 * path, the full-screen tutorial renders reliably here. */
void sdl_zones_show_requested_tutorial(void)
{
    if (!g_zones_tutorial_requested_from_settings)
        return;

    g_zones_tutorial_requested_from_settings = false;
    sdl_zones_show_tutorial();
}

bool sdl_touch_settings_tutorial_requested(void)
{
    return g_touch_tutorial_requested_from_settings;
}

bool sdl_mouse_settings_tutorial_requested(void)
{
    return g_mouse_tutorial_requested_from_settings;
}

bool sdl_character_wheel_settings_tutorial_requested(void)
{
    return g_character_wheel_tutorial_requested_from_settings;
}

void sdl_touch_show_requested_tutorial(void)
{
    if (!g_touch_tutorial_requested_from_settings)
        return;

    g_touch_tutorial_requested_from_settings = false;
    sdl_touch_show_tutorial();
}

void sdl_mouse_show_requested_tutorial(void)
{
    if (!g_mouse_tutorial_requested_from_settings)
        return;

    g_mouse_tutorial_requested_from_settings = false;
    sdl_mouse_show_tutorial();
}

void sdl_touch_tutorial_maybe_show_deferred(void)
{
    if (!g_touch_tutorial_requested_from_settings)
        return;
    if (!sdl_main_screen_click_shortcuts_active())
        return;

    g_touch_tutorial_requested_from_settings = false;
    sdl_touch_show_tutorial();
}

void sdl_mouse_tutorial_maybe_show_deferred(void)
{
    if (!g_mouse_tutorial_requested_from_settings)
        return;
    if (!sdl_main_screen_click_shortcuts_active())
        return;

    g_mouse_tutorial_requested_from_settings = false;
    sdl_mouse_show_tutorial();
}

static void sdl_character_wheel_tutorial_maybe_show_deferred(void)
{
    if (!g_character_wheel_tutorial_requested_from_settings)
        return;
    if (!sdl_main_screen_click_shortcuts_active())
        return;
    if (!sdl_character_wheel_coach_run())
        return;

    g_character_wheel_tutorial_requested_from_settings = false;
    sdl_character_wheel_mark_tutorial_seen_and_save();
}

void sdl_zones_tutorial_maybe_show_deferred(void)
{
    if (!g_zones_tutorial_requested_from_settings)
        return;
    if (!sdl_main_screen_click_shortcuts_active())
        return;

    g_zones_tutorial_requested_from_settings = false;
    sdl_zones_show_tutorial();
}

void sdl_input_tutorial_maybe_show_deferred(void)
{
    sdl_touch_tutorial_maybe_show_deferred();
    sdl_mouse_tutorial_maybe_show_deferred();
    /* Zones replay is shown directly from the Options command (clean context);
     * it deliberately does not use this deferred Term_xtra path. */
    sdl_character_wheel_tutorial_maybe_show_deferred();
    sdl_character_wheel_maybe_show_first_game_tutorial();
}

void sdl_touch_show_tutorial(void)
{
    sdl_touch_tutorial_run_fixed();
    sdl_touch_mark_tutorial_seen_and_save();
}

void sdl_mouse_show_tutorial(void)
{
    sdl_touch_tutorial_run(false, true);
    sdl_mouse_mark_tutorial_seen_and_save();
}

void sdl_touch_maybe_show_first_game_tutorial(void)
{
    if (!sdl_touch_tutorial_device_available())
        return;
    if (sdl_config_touch_tutorial_seen())
        return;

    sdl_touch_tutorial_run_fixed();
    sdl_touch_mark_tutorial_seen_and_save();
}

void sdl_mouse_maybe_show_first_game_tutorial(void)
{
    if (!config.mouse_enabled)
        return;
    if (sdl_config_mouse_tutorial_seen())
        return;

    sdl_touch_tutorial_run(false, true);
    sdl_mouse_mark_tutorial_seen_and_save();
}


