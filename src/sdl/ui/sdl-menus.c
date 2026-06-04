#include "angband.h"
#include "sdl/main-sdl-private.h"

cptr sdl_depth_menu_partition_label(void)
{
    if (!p_ptr || !character_dungeon)
        return "";

    switch (level_partition_kind_for_point(p_ptr->py, p_ptr->px))
    {
    case LEVEL_PART_ROOMY:
        return "Room";
    case LEVEL_PART_RUINED:
        return "Ruin";
    case LEVEL_PART_CAVEY:
        return "Cave";
    case LEVEL_PART_BIG_CAVE:
        return "Big Cave";
    case LEVEL_PART_LABYRINTH:
        return "Labyrinth";
    case LEVEL_PART_CHASM:
        return "Chasm";
    default:
        return "";
    }
}

void sdl_depth_menu_pane_label(char* out, size_t out_sz)
{
    cptr partition;
    char depth[24];

    if (!out || out_sz == 0)
        return;
    out[0] = '\0';

    if (!p_ptr)
        return;

    if (!p_ptr->depth)
        SDL_strlcpy(depth, "Surface", sizeof(depth));
    else
        strnfmt(depth, sizeof(depth), "%d ft", p_ptr->depth * 50);

    partition = sdl_depth_menu_partition_label();
    if (partition && partition[0] && p_ptr->depth)
        strnfmt(out, out_sz, "%s - %s", partition, depth);
    else
        SDL_strlcpy(out, depth, out_sz);
}

typedef struct depth_pane_layout {
    SDL_FRect panel;
    SDL_FRect label;
    SDL_FRect zoom_out;
    SDL_FRect zoom_in;
} depth_pane_layout;

int sdl_depth_menu_pane_font_px(void)
{
    int font_px;

    font_px = sdl_effective_pane_cell_height_for_type(PANE_DEPTH);
    if (font_px < 8)
        font_px = 8;

    return font_px;
}

bool sdl_depth_menu_pane_layout(depth_pane_layout* out)
{
    const struct pane_config* pc = sdl_depth_pane_config();
    SDL_Rect screen;
    SDL_Rect anchor;
    char label[64];
    TTF_Font* font;
    int font_px;
    int text_w;
    float pad_x;
    float pad_y;
    float panel_w;
    float panel_h;
    float row_h;
    float gap;
    float max_w;

    if (out)
        *out = (depth_pane_layout){ 0 };

    if (!pc || !pc->enabled)
        return false;
    if (!character_generated || !character_dungeon || character_icky)
        return false;
    if (!p_ptr || screen_saved_fullscreen_active())
        return false;

    sdl_depth_menu_pane_label(label, sizeof(label));
    if (!label[0])
        return false;

    screen = sdl_get_layout_screen_rect();
    if (screen.w <= 0 || screen.h <= 0)
        return false;
    if (!sdl_overlay_pane_anchor_rect(PANE_DEPTH, &anchor))
        return false;

    font_px = sdl_depth_menu_pane_font_px();
    font = sdl_story_font_for_height(font_px);
    text_w = font ? sdl_touch_pane_story_text_width(font, label)
                  : (int)strlen(label) * font_px / 2;
    if (text_w < 1)
        return false;

    pad_x = (float)font_px * 0.82f;
    pad_y = (float)font_px * 0.34f;
    if (pad_x < 9.0f)
        pad_x = 9.0f;
    if (pad_y < 4.0f)
        pad_y = 4.0f;
    panel_w = (float)text_w + pad_x * 2.0f;
    row_h = (float)font_px * 1.16f + pad_y * 2.0f;
    if (row_h < (float)font_px * 1.45f)
        row_h = (float)font_px * 1.45f;
    gap = 1.0f;
    panel_h = row_h * 2.0f + gap;
    max_w = (float)screen.w * 0.72f;
    if (max_w < 96.0f)
        max_w = (float)screen.w;
    if (panel_w < (float)font_px * 8.5f)
        panel_w = (float)font_px * 8.5f;
    if (panel_w > max_w)
        panel_w = max_w;
    if (panel_w > (float)screen.w)
        panel_w = (float)screen.w;
    if (panel_h > (float)screen.h * 0.32f) {
        panel_h = (float)screen.h * 0.32f;
        row_h = (panel_h - gap) * 0.5f;
        if (row_h < 1.0f)
            row_h = 1.0f;
    }

    if (out)
    {
        float button_w;

        out->panel = sdl_overlay_panel_rect(&anchor, pc->where,
            (int)panel_w, (int)panel_h, &screen);
        out->label = (SDL_FRect){
            .x = out->panel.x,
            .y = out->panel.y,
            .w = out->panel.w,
            .h = row_h,
        };
        button_w = (out->panel.w - gap) * 0.5f;
        if (button_w < 1.0f)
            button_w = out->panel.w * 0.5f;
        out->zoom_out = (SDL_FRect){
            .x = out->panel.x,
            .y = out->panel.y + row_h + gap,
            .w = button_w,
            .h = row_h,
        };
        out->zoom_in = (SDL_FRect){
            .x = out->zoom_out.x + out->zoom_out.w + gap,
            .y = out->zoom_out.y,
            .w = out->panel.x + out->panel.w
                - (out->zoom_out.x + out->zoom_out.w + gap),
            .h = row_h,
        };
        if (out->zoom_in.w < 1.0f)
            out->zoom_in.w = button_w;
    }

    return true;
}

bool sdl_depth_menu_pane_current_rect(SDL_FRect* out)
{
    depth_pane_layout layout;

    if (out)
        *out = (SDL_FRect){ 0 };
    if (!sdl_depth_menu_pane_layout(&layout))
        return false;
    if (out)
        *out = layout.panel;
    return true;
}

void sdl_depth_menu_pane_render(void)
{
    depth_pane_layout layout;
    SDL_FRect shadow;
    SDL_FRect divider;
    SDL_FRect center_divider;
    char label[64];
    SDL_Color label_text;
    SDL_Color zoom_out_text;
    SDL_Color zoom_in_text;

    if (!sdl_depth_menu_pane_layout(&layout))
        return;

    sdl_depth_menu_pane_label(label, sizeof(label));
    if (!label[0])
        return;

    label_text = g_state.palette[
        g_depth_pane_hover_action == SDL_DEPTH_PANE_HOVER_LABEL
            ? TERM_YELLOW : TERM_L_WHITE];
    zoom_out_text = g_state.palette[
        g_depth_pane_hover_action == SDL_DEPTH_PANE_HOVER_ZOOM_OUT
            ? TERM_YELLOW : TERM_L_WHITE];
    zoom_in_text = g_state.palette[
        g_depth_pane_hover_action == SDL_DEPTH_PANE_HOVER_ZOOM_IN
            ? TERM_YELLOW : TERM_L_WHITE];

    shadow = layout.panel;
    shadow.x += 2.0f;
    shadow.y += 2.0f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 126);
    SDL_RenderFillRect(g_state.renderer, &shadow);
    if (g_depth_pane_hover_action != SDL_DEPTH_PANE_HOVER_NONE)
        SDL_SetRenderDrawColor(g_state.renderer, 22, 24, 18, 238);
    else
        SDL_SetRenderDrawColor(g_state.renderer, 8, 10, 12, 226);
    SDL_RenderFillRect(g_state.renderer, &layout.panel);

    if (g_depth_pane_hover_action == SDL_DEPTH_PANE_HOVER_ZOOM_OUT) {
        SDL_SetRenderDrawColor(g_state.renderer, 34, 38, 34, 210);
        SDL_RenderFillRect(g_state.renderer, &layout.zoom_out);
    }
    else if (g_depth_pane_hover_action == SDL_DEPTH_PANE_HOVER_ZOOM_IN) {
        SDL_SetRenderDrawColor(g_state.renderer, 34, 38, 34, 210);
        SDL_RenderFillRect(g_state.renderer, &layout.zoom_in);
    }

    divider = (SDL_FRect){
        .x = layout.panel.x,
        .y = layout.zoom_out.y - 1.0f,
        .w = layout.panel.w,
        .h = 1.0f,
    };
    center_divider = (SDL_FRect){
        .x = layout.zoom_in.x - 1.0f,
        .y = layout.zoom_out.y,
        .w = 1.0f,
        .h = layout.zoom_out.h,
    };
    SDL_SetRenderDrawColor(g_state.renderer, 95, 105, 112, 104);
    SDL_RenderFillRect(g_state.renderer, &divider);
    SDL_RenderFillRect(g_state.renderer, &center_divider);

    sdl_touch_pane_draw_button_text_scaled(&layout.label, NULL, label,
        label_text, 0.54f,
        0.62f);
    sdl_touch_pane_draw_button_text_scaled(&layout.zoom_out, NULL, "-",
        zoom_out_text, 0.42f, 0.58f);
    sdl_touch_pane_draw_button_text_scaled(&layout.zoom_in, NULL, "+",
        zoom_in_text, 0.42f, 0.58f);
}

int sdl_depth_menu_pane_hit_action(float x, float y,
    SDL_FRect* out_rect)
{
    depth_pane_layout layout;

    if (!sdl_main_screen_click_shortcuts_active())
        return SDL_DEPTH_PANE_HOVER_NONE;
    if (!sdl_depth_menu_pane_layout(&layout))
        return SDL_DEPTH_PANE_HOVER_NONE;
    if (out_rect)
        *out_rect = layout.panel;

    if (x < layout.panel.x || x >= layout.panel.x + layout.panel.w
        || y < layout.panel.y || y >= layout.panel.y + layout.panel.h)
    {
        return SDL_DEPTH_PANE_HOVER_NONE;
    }
    if (x >= layout.zoom_out.x && x < layout.zoom_out.x + layout.zoom_out.w
        && y >= layout.zoom_out.y && y < layout.zoom_out.y + layout.zoom_out.h)
    {
        return SDL_DEPTH_PANE_HOVER_ZOOM_OUT;
    }
    if (x >= layout.zoom_in.x && x < layout.zoom_in.x + layout.zoom_in.w
        && y >= layout.zoom_in.y && y < layout.zoom_in.y + layout.zoom_in.h)
    {
        return SDL_DEPTH_PANE_HOVER_ZOOM_IN;
    }
    return SDL_DEPTH_PANE_HOVER_LABEL;
}

bool sdl_depth_menu_pane_handle_hover_pointer(float x, float y)
{
    int action = sdl_depth_menu_pane_hit_action(x, y, NULL);

    if (g_depth_pane_hover_action != action)
    {
        g_depth_pane_hover_action = action;
        g_state.need_present = true;
    }

    if (action != SDL_DEPTH_PANE_HOVER_NONE)
        sdl_pointer_attack_set_panel_hover_mode(SDL_POINTER_ATTACK_NONE);

    return action != SDL_DEPTH_PANE_HOVER_NONE;
}

bool sdl_depth_menu_pane_handle_pointer(float x, float y)
{
    int action = sdl_depth_menu_pane_hit_action(x, y, NULL);

    if (action == SDL_DEPTH_PANE_HOVER_NONE)
        return false;

    g_depth_pane_hover_action = SDL_DEPTH_PANE_HOVER_NONE;
    g_state.need_present = true;
    if (action == SDL_DEPTH_PANE_HOVER_ZOOM_OUT)
        return sdl_main_screen_adjust_main_view_scale(-1);
    if (action == SDL_DEPTH_PANE_HOVER_ZOOM_IN)
        return sdl_main_screen_adjust_main_view_scale(1);

    sdl_enqueue_bypassed_command('M');
    return true;
}

int sdl_touch_pane_yes_no_prompt_font_px(float cell_h, int screen_h)
{
    int font_px = (int)(cell_h * 1.35f);

    if (font_px < 20)
        font_px = 20;
    if (font_px > 30)
        font_px = 30;
    if (screen_h < 360 && font_px > 24)
        font_px = 24;
    if (screen_h < 260 && font_px > 20)
        font_px = 20;

    return font_px;
}

void sdl_touch_pane_append_ellipsis(char* line, size_t line_size)
{
    size_t len;

    if (!line || line_size < 4)
        return;

    len = strlen(line);
    while (len > 0 && isspace((unsigned char)line[len - 1]))
        line[--len] = '\0';

    if (len <= line_size - 4) {
        SDL_strlcat(line, "...", line_size);
    } else {
        line[line_size - 4] = '.';
        line[line_size - 3] = '.';
        line[line_size - 2] = '.';
        line[line_size - 1] = '\0';
    }
}

bool sdl_narrative_banner_overlay_enabled(void)
{
    return g_state.window && g_state.renderer
        && g_views[PANE_MAIN].term_ready;
}

bool sdl_narrative_banner_top_center_pane_rect(
    const struct pane_config* pc, SDL_FRect* out)
{
    SDL_Rect rect;
    SDL_FRect frect;

    if (out)
        *out = (SDL_FRect){ 0 };
    if (!pc || !out || !pc->enabled || pc->where != PLACE_TOP_CENTER)
        return false;

    switch (pc->pane) {
    case PANE_MAIN_MENU:
        return sdl_main_menu_pane_current_rect(out);

    case PANE_DEPTH:
        return sdl_depth_menu_pane_current_rect(out);

    case PANE_STATUS:
    {
        status_pane_layout layout;

        if (!sdl_status_pane_layout(&layout))
            return false;
        *out = layout.panel;
        return out->w > 0.0f && out->h > 0.0f;
    }

    case PANE_LEFT_PANEL:
        if (!sdl_left_panel_pane_presentation_active())
            return false;
        break;

    case PANE_DESCRIPTION:
        return false;

    default:
        break;
    }

    if (pc->pane <= PANE_MAIN || pc->pane >= PANE_MAX)
        return false;
    rect = g_pane_rects[pc->pane];
    if (!sdl_rect_has_area(&rect))
        return false;

    frect = (SDL_FRect){
        .x = (float)rect.x,
        .y = (float)rect.y,
        .w = (float)rect.w,
        .h = (float)rect.h,
    };
    if (frect.w <= 0.0f || frect.h <= 0.0f)
        return false;

    *out = frect;
    return true;
}

int sdl_narrative_banner_top_center_panes_bottom(void)
{
    float bottom = 0.0f;

    for (int i = 0; i < pane_config_count; i++) {
        SDL_FRect pane_rect;
        float pane_bottom;

        if (!sdl_narrative_banner_top_center_pane_rect(&pane_config[i],
                &pane_rect))
        {
            continue;
        }

        pane_bottom = pane_rect.y + pane_rect.h;
        if (pane_bottom > bottom)
            bottom = pane_bottom;
    }

    return (int)(bottom + 0.5f);
}

void sdl_narrative_banner_apply_top_center_avoidance(SDL_Rect* rect,
    int min_h)
{
    int pane_bottom;
    int rect_bottom;

    if (!rect || rect->w <= 0 || rect->h <= 0)
        return;
    if (min_h < 1)
        min_h = 1;

    pane_bottom = sdl_narrative_banner_top_center_panes_bottom();
    if (pane_bottom <= rect->y)
        return;

    rect_bottom = rect->y + rect->h;
    rect->y = pane_bottom;
    rect->h = rect_bottom - rect->y;
    if (rect->h < min_h)
        rect->h = min_h;
}

bool sdl_narrative_banner_base_rect(SDL_Rect* out)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    SDL_Rect rect;

    if (!out)
        return false;

    if (view->term_ready && view->cell_w > 0 && view->cell_h > 0
        && view->cols > 0 && view->rows > 0)
    {
        int reserved_top = view->cell_h * ROW_MAP;

        rect = (SDL_Rect){
            .x = view->rect.x + view->margin_x,
            .y = view->rect.y + view->margin_y + reserved_top,
            .w = sdl_main_view_visual_cols(view) * view->cell_w,
            .h = sdl_main_view_visual_rows(view) * view->cell_h
                - reserved_top,
        };
        if (rect.h < view->cell_h)
            rect.h = view->cell_h;
        sdl_narrative_banner_apply_top_center_avoidance(&rect,
            view->cell_h);
    }
    else
    {
        rect = sdl_get_layout_screen_rect();
        sdl_narrative_banner_apply_top_center_avoidance(&rect, 1);
    }

    if (rect.w <= 0 || rect.h <= 0)
        return false;

    *out = rect;
    return true;
}

int sdl_narrative_banner_font_px(const SDL_Rect* rect)
{
    float font_px;

    if (!rect || rect->h <= 0)
        return 0;

    font_px = (float)rect->h * 0.040f;
    font_px = sdl_touch_pane_clampf(font_px, 18.0f, 38.0f);
    if (rect->h < 420 && font_px > 24.0f)
        font_px = 24.0f;
    if (rect->h < 300 && font_px > 20.0f)
        font_px = 20.0f;

    return (int)(font_px + 0.5f);
}

float sdl_narrative_banner_line_h(TTF_Font* font, int font_px)
{
    float line_h;

    line_h = font ? (float)TTF_GetFontHeight(font) : (float)font_px;
    if (line_h < (float)font_px)
        line_h = (float)font_px;
    return line_h * 1.08f;
}

float sdl_narrative_banner_max_text_w(const SDL_Rect* rect, int font_px)
{
    float max_w;
    float cap_w;

    if (!rect || rect->w <= 0)
        return 0.0f;

    max_w = (float)rect->w * 0.76f;
    cap_w = (float)font_px * 50.0f;
    if (max_w > cap_w)
        max_w = cap_w;
    if (max_w > (float)rect->w - 12.0f)
        max_w = (float)rect->w - 12.0f;
    if (max_w < 80.0f)
        max_w = (float)rect->w;

    return max_w;
}

int sdl_narrative_banner_wrap_lines(cptr text, TTF_Font* font,
    float max_w, char lines[][SDL_NARRATIVE_BANNER_LINE_LEN], int max_lines)
{
    const char* p;
    char current[SDL_NARRATIVE_BANNER_LINE_LEN];
    int line_count = 0;
    bool truncated = false;

    if (!lines || max_lines <= 0)
        return 0;

    p = (text && text[0]) ? text : "";
    current[0] = '\0';

    while (*p) {
        char word[SDL_NARRATIVE_BANNER_LINE_LEN];
        char candidate[SDL_NARRATIVE_BANNER_LINE_LEN];
        size_t word_len = 0;

        while (*p && *p != '\n' && isspace((unsigned char)*p))
            p++;

        if (*p == '\n') {
            p++;
            if (current[0]) {
                if (line_count >= max_lines) {
                    truncated = true;
                    break;
                }
                SDL_strlcpy(lines[line_count++], current,
                    SDL_NARRATIVE_BANNER_LINE_LEN);
                current[0] = '\0';
            }
            continue;
        }

        if (!*p)
            break;

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
                SDL_NARRATIVE_BANNER_LINE_LEN);
            SDL_strlcpy(current, word, sizeof(current));
        }
        else
        {
            SDL_strlcpy(current, candidate, sizeof(current));
        }
    }

    if (!truncated && current[0]) {
        if (line_count < max_lines)
            SDL_strlcpy(lines[line_count++], current,
                SDL_NARRATIVE_BANNER_LINE_LEN);
        else
            truncated = true;
    }

    if (truncated && line_count > 0)
        sdl_touch_pane_append_ellipsis(lines[line_count - 1],
            SDL_NARRATIVE_BANNER_LINE_LEN);

    return line_count;
}

int sdl_narrative_banner_line_count(void)
{
    SDL_Rect rect;
    int font_px;
    TTF_Font* font;
    char lines[SDL_NARRATIVE_BANNER_MAX_LINES][SDL_NARRATIVE_BANNER_LINE_LEN];

    if (!active_narrative_banner_visible())
        return 0;
    if (!sdl_narrative_banner_base_rect(&rect))
        return 0;

    font_px = sdl_narrative_banner_font_px(&rect);
    font = sdl_story_font_for_height(font_px);
    if (!font)
        return 0;

    return sdl_narrative_banner_wrap_lines(active_narrative_banner_text(),
        font, sdl_narrative_banner_max_text_w(&rect, font_px), lines,
        SDL_NARRATIVE_BANNER_MAX_LINES);
}

void sdl_narrative_banner_draw_line(TTF_Font* font, cptr text,
    SDL_Color color, float center_x, float y, float max_w, float line_h)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    float scale = 1.0f;

    if (!font || !text || !text[0] || max_w <= 0.0f || line_h <= 0.0f)
        return;

    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }

    if (surface->w > 0 && (float)surface->w > max_w)
        scale = max_w / (float)surface->w;

    dst = (SDL_FRect){
        .w = (float)surface->w * scale,
        .h = (float)surface->h * scale,
    };
    dst.x = center_x - dst.w * 0.5f;
    dst.y = y + (line_h - dst.h) * 0.5f;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

void sdl_narrative_banner_render(void)
{
    SDL_Rect rect;
    TTF_Font* font;
    char lines[SDL_NARRATIVE_BANNER_MAX_LINES][SDL_NARRATIVE_BANNER_LINE_LEN];
    int font_px;
    int line_count;
    int shown_lines;
    int max_line_w = 0;
    float max_text_w;
    float pad_x;
    float pad_y;
    float line_h;
    float panel_w;
    float panel_h;
    float top_gap;
    SDL_FRect panel;
    SDL_Color text_color;

    if (!sdl_narrative_banner_overlay_enabled())
        return;
    if (!active_narrative_banner_visible() || character_icky > 0)
        return;
    if (!sdl_narrative_banner_base_rect(&rect))
        return;

    font_px = sdl_narrative_banner_font_px(&rect);
    font = sdl_story_font_for_height(font_px);
    if (!font)
        return;

    max_text_w = sdl_narrative_banner_max_text_w(&rect, font_px);
    line_count = sdl_narrative_banner_wrap_lines(active_narrative_banner_text(),
        font, max_text_w, lines, SDL_NARRATIVE_BANNER_MAX_LINES);
    if (line_count <= 0)
        return;

    shown_lines = line_count;
    if (g_narrative_banner_line_limit > 0
        && shown_lines > g_narrative_banner_line_limit)
    {
        shown_lines = g_narrative_banner_line_limit;
    }
    if (shown_lines <= 0)
        return;

    for (int i = 0; i < shown_lines; i++) {
        int width = sdl_touch_pane_story_text_width(font, lines[i]);
        if (width > max_line_w)
            max_line_w = width;
    }

    pad_x = sdl_touch_pane_clampf((float)font_px * 0.78f, 12.0f, 28.0f);
    pad_y = sdl_touch_pane_clampf((float)font_px * 0.42f, 7.0f, 17.0f);
    line_h = sdl_narrative_banner_line_h(font, font_px);

    panel_w = (float)max_line_w + pad_x * 2.0f;
    if (panel_w > max_text_w + pad_x * 2.0f)
        panel_w = max_text_w + pad_x * 2.0f;
    if (panel_w < (float)font_px * 10.0f)
        panel_w = (float)font_px * 10.0f;
    if (panel_w > (float)rect.w - 8.0f)
        panel_w = (float)rect.w - 8.0f;
    if (panel_w < 80.0f)
        panel_w = (float)rect.w;

    panel_h = pad_y * 2.0f + line_h * (float)shown_lines;
    top_gap = sdl_touch_pane_clampf((float)rect.h * 0.018f, 5.0f, 18.0f);

    panel = (SDL_FRect){
        .x = (float)rect.x + ((float)rect.w - panel_w) * 0.5f,
        .y = (float)rect.y + top_gap,
        .w = panel_w,
        .h = panel_h,
    };
    if (panel.x < (float)rect.x + 4.0f)
        panel.x = (float)rect.x + 4.0f;
    if (panel.x + panel.w > (float)(rect.x + rect.w) - 4.0f)
        panel.x = (float)(rect.x + rect.w) - 4.0f - panel.w;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 4, 5, 7, 212);
    SDL_RenderFillRect(g_state.renderer, &panel);
    SDL_SetRenderDrawColor(g_state.renderer, 255, 184, 94, 126);
    SDL_RenderRect(g_state.renderer, &panel);

    text_color = (SDL_Color){
        angband_color_table[TERM_ORANGE][1],
        angband_color_table[TERM_ORANGE][2],
        angband_color_table[TERM_ORANGE][3],
        255,
    };

    for (int i = 0; i < shown_lines; i++) {
        sdl_narrative_banner_draw_line(font, lines[i], text_color,
            panel.x + panel.w * 0.5f,
            panel.y + pad_y + line_h * (float)i,
            panel.w - pad_x * 2.0f, line_h);
    }
}

void sdl_narrative_banner_show(bool line_delay)
{
    sdl_view* restore_view;
    int line_count = 1;

    if (!sdl_narrative_banner_overlay_enabled())
        return;
    if (!active_narrative_banner_visible())
        return;

    restore_view = sdl_view_from_term(Term);
    if (!restore_view)
        restore_view = &g_views[PANE_MAIN];

    if (line_delay) {
        line_count = sdl_narrative_banner_line_count();
        if (line_count < 1)
            line_count = 1;
    }

    for (int i = 1; i <= line_count; i++) {
        g_narrative_banner_line_limit = line_delay ? i : 0;
        g_state.need_present = true;
        if (sdl_render_current_window_frame()) {
            SDL_RenderPresent(g_state.renderer);
            g_state.need_present = false;
        }
        sdl_restore_render_target(restore_view);

        if (line_delay && i < line_count)
            SDL_Delay(800);
    }

    g_narrative_banner_line_limit = 0;
}

int sdl_touch_pane_wrap_prompt_lines(cptr text, TTF_Font* font,
    float max_w, char lines[][SDL_TOUCH_YES_NO_LINE_LEN], int max_lines)
{
    const char* p;
    char current[SDL_TOUCH_YES_NO_LINE_LEN];
    int line_count = 0;
    bool truncated = false;

    if (!lines || max_lines <= 0)
        return 0;

    p = (text && text[0]) ? text : "Are you sure?";
    current[0] = '\0';

    while (*p) {
        char word[SDL_TOUCH_YES_NO_LINE_LEN];
        char candidate[SDL_TOUCH_YES_NO_LINE_LEN];
        size_t word_len = 0;

        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;

        while (*p && !isspace((unsigned char)*p)) {
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
                SDL_TOUCH_YES_NO_LINE_LEN);
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
        if (line_count < max_lines) {
            SDL_strlcpy(lines[line_count++], current,
                SDL_TOUCH_YES_NO_LINE_LEN);
        } else {
            truncated = true;
        }
    }

    if (line_count <= 0) {
        SDL_strlcpy(lines[0], "Are you sure?", SDL_TOUCH_YES_NO_LINE_LEN);
        line_count = 1;
    }

    if (truncated)
        sdl_touch_pane_append_ellipsis(lines[line_count - 1],
            SDL_TOUCH_YES_NO_LINE_LEN);

    return line_count;
}

bool sdl_touch_pane_yes_no_prompt_layout(SDL_FRect* panel_rect,
    SDL_FRect* prompt_rect, SDL_FRect* yes_rect, SDL_FRect* no_rect)
{
    SDL_Rect screen;
    const sdl_view* view = &g_views[PANE_MAIN];
    float cell_w = 8.0f;
    float cell_h = 16.0f;
    TTF_Font* prompt_font;
    cptr prompt_text;
    char prompt_lines[SDL_TOUCH_YES_NO_MAX_LINES][SDL_TOUCH_YES_NO_LINE_LEN];
    int prompt_font_px;
    int prompt_line_count;
    int prompt_text_w;
    float margin;
    float button_gap;
    float row_gap;
    float button_w;
    float button_h;
    float buttons_w;
    float min_panel_w;
    float panel_w;
    float panel_h;
    float max_panel_w;
    float max_panel_h;
    float prompt_h;
    float prompt_line_h;
    float prompt_w;
    float x;
    float y;
    float button_y;
    float button_x;

    if (!g_touch_pane_yes_no_prompt_active || !yes_rect || !no_rect)
        return false;

    if (view->term_ready && view->cell_w > 0 && view->cell_h > 0
        && view->cols > 0 && view->rows > 0)
    {
        screen = (SDL_Rect){
            .x = view->rect.x + view->margin_x,
            .y = view->rect.y + view->margin_y,
            .w = sdl_main_view_visual_cols(view) * view->cell_w,
            .h = sdl_main_view_visual_rows(view) * view->cell_h,
        };
        cell_w = (float)view->cell_w;
        cell_h = (float)view->cell_h;
    }
    else
    {
        screen = sdl_get_layout_screen_rect();
    }

    if (screen.w <= 0 || screen.h <= 0)
        return false;

    prompt_text = g_touch_pane_yes_no_prompt_text[0]
        ? g_touch_pane_yes_no_prompt_text
        : "Are you sure?";
    prompt_font_px = sdl_touch_pane_yes_no_prompt_font_px(cell_h, screen.h);
    prompt_font = sdl_story_font_for_height(prompt_font_px);
    prompt_text_w = sdl_touch_pane_story_text_width(prompt_font, prompt_text);

    margin = sdl_touch_pane_clampf(cell_h * 1.20f, 20.0f, 34.0f);
    button_gap = sdl_touch_pane_clampf(cell_w * 1.50f, 14.0f, 26.0f);
    row_gap = sdl_touch_pane_clampf(cell_h * 0.90f, 15.0f, 25.0f);
    button_w = sdl_touch_pane_clampf(cell_w * 10.20f, 104.0f, 148.0f);
    button_h = sdl_touch_pane_clampf(cell_h * 2.95f, 58.0f, 76.0f);

    max_panel_w = (float)screen.w * 0.90f;
    if (max_panel_w > cell_w * 70.0f)
        max_panel_w = cell_w * 70.0f;
    if (max_panel_w < 272.0f)
        max_panel_w = (float)screen.w - 8.0f;
    if (max_panel_w > (float)screen.w - 8.0f)
        max_panel_w = (float)screen.w - 8.0f;
    if (max_panel_w < 96.0f)
        max_panel_w = (float)screen.w;

    buttons_w = button_w * 2.0f + button_gap;
    min_panel_w = buttons_w + margin * 2.0f;
    if (min_panel_w > max_panel_w) {
        button_w = (max_panel_w - button_gap - margin * 2.0f) * 0.5f;
        if (button_w < 44.0f) {
            margin = 8.0f;
            button_gap = 8.0f;
            button_w = (max_panel_w - button_gap - margin * 2.0f) * 0.5f;
        }
        if (button_w < 36.0f)
            button_w = 36.0f;
        buttons_w = button_w * 2.0f + button_gap;
        min_panel_w = buttons_w + margin * 2.0f;
    }

    panel_w = (prompt_text_w > 0)
        ? (float)prompt_text_w + margin * 2.60f
        : min_panel_w;
    panel_w = sdl_touch_pane_clampf(panel_w, min_panel_w, max_panel_w);
    prompt_w = panel_w - margin * 2.0f;
    if (prompt_w < 40.0f)
        prompt_w = 40.0f;

    prompt_line_count = sdl_touch_pane_wrap_prompt_lines(prompt_text, prompt_font,
        prompt_w, prompt_lines, SDL_TOUCH_YES_NO_MAX_LINES);
    prompt_line_h = (float)prompt_font_px * 1.28f;
    if (prompt_line_h < 19.0f)
        prompt_line_h = 19.0f;
    prompt_h = prompt_line_h * (float)prompt_line_count;

    max_panel_h = (float)screen.h * 0.78f;
    panel_h = margin * 2.0f + prompt_h + row_gap + button_h;
    if (panel_h > max_panel_h && max_panel_h > 0.0f) {
        float available_prompt_h = max_panel_h
            - (margin * 2.0f + row_gap + button_h);
        if (available_prompt_h >= prompt_line_h)
            prompt_h = available_prompt_h;
        panel_h = margin * 2.0f + prompt_h + row_gap + button_h;
    }

    if (panel_w > (float)screen.w)
        panel_w = (float)screen.w;
    if (panel_h > (float)screen.h)
        panel_h = (float)screen.h;

    x = (float)screen.x + ((float)screen.w - panel_w) * 0.5f;
    if (g_touch_pane_yes_no_prompt_placement
        == SDL_TOUCH_YES_NO_PLACEMENT_LOWER)
    {
        float bottom_margin = sdl_touch_pane_clampf(cell_h * 0.55f, 8.0f,
            18.0f);
        y = (float)screen.y + (float)screen.h - panel_h - bottom_margin;
    }
    else
    {
        y = (float)screen.y + ((float)screen.h - panel_h) * 0.5f;
    }
    if (x < (float)screen.x)
        x = (float)screen.x;
    if (y < (float)screen.y)
        y = (float)screen.y;

    buttons_w = button_w * 2.0f + button_gap;
    button_x = x + (panel_w - buttons_w) * 0.5f;
    button_y = y + panel_h - margin - button_h;

    if (panel_rect)
    {
        *panel_rect = (SDL_FRect){ .x = x, .y = y, .w = panel_w, .h = panel_h };
    }
    if (prompt_rect)
    {
        *prompt_rect = (SDL_FRect){
            .x = x + margin,
            .y = y + margin,
            .w = panel_w - margin * 2.0f,
            .h = prompt_h,
        };
    }

    *yes_rect = (SDL_FRect){
        .x = button_x,
        .y = button_y,
        .w = button_w,
        .h = button_h,
    };
    *no_rect = (SDL_FRect){
        .x = button_x + button_w + button_gap,
        .y = button_y,
        .w = button_w,
        .h = button_h,
    };

    return true;
}

bool sdl_touch_pane_point_to_slot(float x, float y, int* out_slot)
{
    SDL_FRect slot_rects[SDL_TOUCH_PANE_BUTTON_COUNT];
    SDL_Rect pane;

    if (out_slot)
        *out_slot = -1;

    if (!sdl_touch_pane_current_rect(&pane))
        return false;

    if (!sdl_touch_pane_compute_layout(&pane, slot_rects))
        return false;

    for (int i = 0; i < SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT; i++) {
        int slot = sdl_touch_pane_visible_slot_at(i);
        const SDL_FRect* rect;

        if (slot < 0)
            continue;
        if (!sdl_touch_pane_slot_visible_in_current_mode(slot))
            continue;

        rect = &slot_rects[slot];
        if (x >= rect->x && x < rect->x + rect->w && y >= rect->y && y < rect->y + rect->h) {
            if (out_slot)
                *out_slot = slot;
            return true;
        }
    }

    if (x >= (float)pane.x && x < (float)(pane.x + pane.w)
        && y >= (float)pane.y && y < (float)(pane.y + pane.h))
    {
        return true;
    }

    return false;
}

bool sdl_touch_pane_yes_no_prompt_hit(float x, float y,
    sdl_touch_yes_no_prompt_hover* button)
{
    SDL_FRect yes_rect;
    SDL_FRect no_rect;

    if (button)
        *button = SDL_TOUCH_YES_NO_HOVER_NONE;

    if (!sdl_touch_pane_yes_no_prompt_layout(NULL, NULL, &yes_rect, &no_rect))
        return false;

    if (x >= yes_rect.x && x < yes_rect.x + yes_rect.w
        && y >= yes_rect.y && y < yes_rect.y + yes_rect.h)
    {
        if (button)
            *button = SDL_TOUCH_YES_NO_HOVER_YES;
        return true;
    }

    if (x >= no_rect.x && x < no_rect.x + no_rect.w
        && y >= no_rect.y && y < no_rect.y + no_rect.h)
    {
        if (button)
            *button = SDL_TOUCH_YES_NO_HOVER_NO;
        return true;
    }

    return true;
}

void sdl_touch_pane_clear_yes_no_prompt(void)
{
    g_touch_pane_yes_no_prompt_active = false;
    g_touch_pane_yes_no_prompt_text[0] = '\0';
    g_touch_pane_yes_no_prompt_placement = SDL_TOUCH_YES_NO_PLACEMENT_CENTER;
    g_touch_pane_yes_no_prompt_hover = SDL_TOUCH_YES_NO_HOVER_NONE;
    g_state.need_present = true;
}

bool sdl_touch_pane_handle_yes_no_prompt_hover(float x, float y)
{
    sdl_touch_yes_no_prompt_hover button = SDL_TOUCH_YES_NO_HOVER_NONE;

    if (!sdl_touch_pane_yes_no_prompt_hit(x, y, &button))
        return false;

    if (g_touch_pane_yes_no_prompt_hover != button)
    {
        g_touch_pane_yes_no_prompt_hover = button;
        g_state.need_present = true;
    }

    return true;
}

bool sdl_touch_pane_handle_yes_no_prompt_pointer(float x, float y)
{
    sdl_touch_yes_no_prompt_hover button = SDL_TOUCH_YES_NO_HOVER_NONE;

    if (!sdl_touch_pane_yes_no_prompt_hit(x, y, &button))
        return false;

    g_touch_pane_yes_no_prompt_hover = button;

    if (button == SDL_TOUCH_YES_NO_HOVER_YES)
    {
        sdl_touch_pane_clear_yes_no_prompt();
        Term_keypress('y');
        return true;
    }

    if (button == SDL_TOUCH_YES_NO_HOVER_NO)
    {
        sdl_touch_pane_clear_yes_no_prompt();
        Term_keypress('n');
        return true;
    }

    /* The yes/no prompt is modal; ignore clicks outside its buttons. */
    return true;
}

void sdl_touch_pane_draw_arrow(const SDL_FRect* rect, int binding, SDL_Color color)
{
    float dx = 0.0f;
    float dy = 0.0f;
    float px = 0.0f;
    float py = 0.0f;
    float cx;
    float cy;
    float body_len;
    float head_len;
    float tail_x;
    float tail_y;
    float tip_x;
    float tip_y;

    if (!rect)
        return;

    switch (binding) {
    case '7': dx = -0.70710677f; dy = -0.70710677f; break;
    case '8': dx = 0.0f; dy = -1.0f; break;
    case '9': dx = 0.70710677f; dy = -0.70710677f; break;
    case '4': dx = -1.0f; dy = 0.0f; break;
    case '6': dx = 1.0f; dy = 0.0f; break;
    case '1': dx = -0.70710677f; dy = 0.70710677f; break;
    case '2': dx = 0.0f; dy = 1.0f; break;
    case '3': dx = 0.70710677f; dy = 0.70710677f; break;
    default:
        return;
    }

    px = -dy;
    py = dx;
    cx = rect->x + rect->w * 0.5f;
    cy = rect->y + rect->h * 0.5f;
    body_len = ((rect->w < rect->h) ? rect->w : rect->h) * 0.28f;
    head_len = ((rect->w < rect->h) ? rect->w : rect->h) * 0.16f;
    tail_x = cx - dx * body_len;
    tail_y = cy - dy * body_len;
    tip_x = cx + dx * body_len;
    tip_y = cy + dy * body_len;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(g_state.renderer, tail_x, tail_y, tip_x, tip_y);
    SDL_RenderLine(g_state.renderer, tip_x, tip_y,
        tip_x - dx * head_len + px * head_len * 0.55f,
        tip_y - dy * head_len + py * head_len * 0.55f);
    SDL_RenderLine(g_state.renderer, tip_x, tip_y,
        tip_x - dx * head_len - px * head_len * 0.55f,
        tip_y - dy * head_len - py * head_len * 0.55f);
}

void sdl_touch_pane_draw_button_text_scaled(const SDL_FRect* rect, const char* name,
    const char* symbol, SDL_Color color, float single_text_font_ratio,
    float single_text_height_ratio)
{
    SDL_Surface* name_surface = NULL;
    SDL_Surface* symbol_surface = NULL;
    SDL_Texture* name_texture = NULL;
    SDL_Texture* symbol_texture = NULL;
    TTF_Font* name_font = NULL;
    TTF_Font* symbol_font = NULL;
    bool have_name;
    bool have_symbol;
    float name_max_w;
    float name_max_h;
    float symbol_max_w;
    float symbol_max_h;
    float gap;
    SDL_FRect name_dst;
    SDL_FRect symbol_dst;
    int name_font_px;
    int symbol_font_px;

    if (!rect)
        return;

    have_name = (name && name[0]);
    have_symbol = (symbol && symbol[0]);
    if (!have_name && !have_symbol)
        return;

    if (single_text_font_ratio <= 0.0f)
        single_text_font_ratio = 0.28f;
    if (single_text_height_ratio <= 0.0f)
        single_text_height_ratio = 0.38f;

    name_font_px = (int)(rect->h * 0.18f);
    if (name_font_px < 10)
        name_font_px = 10;

    symbol_font_px =
        (int)(rect->h * (have_name ? 0.22f : single_text_font_ratio));
    if (symbol_font_px < 12)
        symbol_font_px = 12;

    if (have_name) {
        name_font = sdl_story_font_for_height(name_font_px);
        if (name_font)
            name_surface = TTF_RenderText_Blended(name_font, name, 0, color);
    }

    if (have_symbol) {
        symbol_font = sdl_story_font_for_height(symbol_font_px);
        if (symbol_font)
            symbol_surface = TTF_RenderText_Blended(symbol_font, symbol, 0, color);
    }

    if (!name_surface && !symbol_surface)
        return;

    if (name_surface)
        name_texture = SDL_CreateTextureFromSurface(g_state.renderer, name_surface);
    if (symbol_surface)
        symbol_texture = SDL_CreateTextureFromSurface(g_state.renderer, symbol_surface);

    if (name_surface && !name_texture) {
        SDL_DestroySurface(name_surface);
        name_surface = NULL;
    }
    if (symbol_surface && !symbol_texture) {
        SDL_DestroySurface(symbol_surface);
        symbol_surface = NULL;
    }

    if (!name_surface && !symbol_surface)
        return;

    gap = rect->h * 0.03f;
    if (gap < 2.0f)
        gap = 2.0f;

    if (name_surface && symbol_surface) {
        float total_h;
        float avail_h;
        float name_scale_w;
        float name_scale_h;
        float name_scale;
        float symbol_scale_w;
        float symbol_scale_h;
        float symbol_scale;
        float scale;
        float name_h;
        float symbol_h;
        float start_y;

        name_max_w = rect->w * 0.82f;
        symbol_max_w = rect->w * 0.82f;
        avail_h = rect->h * 0.68f;
        name_max_h = rect->h * 0.22f;
        symbol_max_h = rect->h * 0.30f;

        name_scale_w = (name_surface->w > 0) ? (name_max_w / (float)name_surface->w) : 1.0f;
        name_scale_h = (name_surface->h > 0) ? (name_max_h / (float)name_surface->h) : 1.0f;
        name_scale = (name_scale_w < name_scale_h) ? name_scale_w : name_scale_h;
        if (name_scale > 1.0f)
            name_scale = 1.0f;

        symbol_scale_w = (symbol_surface->w > 0) ? (symbol_max_w / (float)symbol_surface->w) : 1.0f;
        symbol_scale_h = (symbol_surface->h > 0) ? (symbol_max_h / (float)symbol_surface->h) : 1.0f;
        symbol_scale = (symbol_scale_w < symbol_scale_h) ? symbol_scale_w : symbol_scale_h;
        if (symbol_scale > 1.0f)
            symbol_scale = 1.0f;

        total_h = (float)name_surface->h * name_scale + gap + (float)symbol_surface->h * symbol_scale;
        scale = 1.0f;
        if (total_h > avail_h && total_h > 0.0f)
            scale = avail_h / total_h;

        name_scale *= scale;
        symbol_scale *= scale;
        name_h = (float)name_surface->h * name_scale;
        symbol_h = (float)symbol_surface->h * symbol_scale;
        start_y = rect->y + (rect->h - (name_h + gap + symbol_h)) * 0.5f;

        name_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)name_surface->w * name_scale) * 0.5f,
            .y = start_y,
            .w = (float)name_surface->w * name_scale,
            .h = name_h,
        };
        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)symbol_surface->w * symbol_scale) * 0.5f,
            .y = start_y + name_h + gap,
            .w = (float)symbol_surface->w * symbol_scale,
            .h = symbol_h,
        };

        SDL_SetTextureBlendMode(name_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureBlendMode(symbol_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, name_texture, NULL, &name_dst);
        SDL_RenderTexture(g_state.renderer, symbol_texture, NULL, &symbol_dst);
    } else {
        SDL_Surface* only_surface = name_surface ? name_surface : symbol_surface;
        SDL_Texture* only_texture = name_surface ? name_texture : symbol_texture;
        float max_w = rect->w * 0.82f;
        float max_h = rect->h
            * ((!have_name && have_symbol) ? single_text_height_ratio : 0.38f);
        float scale_w = (only_surface->w > 0) ? (max_w / (float)only_surface->w) : 1.0f;
        float scale_h = (only_surface->h > 0) ? (max_h / (float)only_surface->h) : 1.0f;
        float scale = (scale_w < scale_h) ? scale_w : scale_h;

        if (scale > 1.0f)
            scale = 1.0f;

        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)only_surface->w * scale) * 0.5f,
            .y = rect->y + (rect->h - (float)only_surface->h * scale) * 0.5f,
            .w = (float)only_surface->w * scale,
            .h = (float)only_surface->h * scale,
        };

        SDL_SetTextureBlendMode(only_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, only_texture, NULL, &symbol_dst);
    }

    if (name_texture)
        SDL_DestroyTexture(name_texture);
    if (symbol_texture)
        SDL_DestroyTexture(symbol_texture);
    if (name_surface)
        SDL_DestroySurface(name_surface);
    if (symbol_surface)
        SDL_DestroySurface(symbol_surface);
}

void sdl_touch_pane_draw_button_text(const SDL_FRect* rect, const char* name, const char* symbol,
    SDL_Color color)
{
    sdl_touch_pane_draw_button_text_scaled(rect, name, symbol, color, 0.28f, 0.38f);
}

void sdl_touch_pane_draw_wrapped_prompt(const SDL_FRect* rect,
    cptr text, SDL_Color color, int font_px)
{
    TTF_Font* font;
    char lines[SDL_TOUCH_YES_NO_MAX_LINES][SDL_TOUCH_YES_NO_LINE_LEN];
    int line_count;
    float line_h;
    float total_h;
    float start_y;

    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    font = sdl_story_font_for_height(font_px);
    if (!font)
        return;

    line_count = sdl_touch_pane_wrap_prompt_lines(text, font, rect->w,
        lines, SDL_TOUCH_YES_NO_MAX_LINES);
    if (line_count <= 0)
        return;

    line_h = (float)font_px * 1.28f;
    if (line_h < 19.0f)
        line_h = 19.0f;
    if (line_h * (float)line_count > rect->h)
        line_h = rect->h / (float)line_count;
    if (line_h < 10.0f)
        line_h = 10.0f;

    total_h = line_h * (float)line_count;
    start_y = rect->y + (rect->h - total_h) * 0.5f;
    if (start_y < rect->y)
        start_y = rect->y;

    for (int i = 0; i < line_count; i++) {
        SDL_Surface* surface;
        SDL_Texture* texture;
        float max_w;
        float max_h;
        float scale_w;
        float scale_h;
        float scale;
        SDL_FRect dst;

        if (!lines[i][0])
            continue;

        surface = TTF_RenderText_Blended(font, lines[i], 0, color);
        if (!surface)
            continue;

        texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
        if (!texture) {
            SDL_DestroySurface(surface);
            continue;
        }

        max_w = rect->w * 0.98f;
        max_h = line_h * 0.86f;
        scale_w = (surface->w > 0) ? (max_w / (float)surface->w) : 1.0f;
        scale_h = (surface->h > 0) ? (max_h / (float)surface->h) : 1.0f;
        scale = (scale_w < scale_h) ? scale_w : scale_h;
        if (scale > 1.0f)
            scale = 1.0f;

        dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)surface->w * scale) * 0.5f,
            .y = start_y + (float)i * line_h
                + (line_h - (float)surface->h * scale) * 0.5f,
            .w = (float)surface->w * scale,
            .h = (float)surface->h * scale,
        };

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
        SDL_DestroySurface(surface);
    }
}

void sdl_touch_pane_binding_symbol(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    switch (binding) {
    case GAMEPAD_BIND_NONE:
        return;
    case GAMEPAD_BIND_SHIFT:
        SDL_strlcpy(buf, "Shift", buflen);
        return;
    case GAMEPAD_BIND_CTRL:
        SDL_strlcpy(buf, "Ctrl", buflen);
        return;
    case GAMEPAD_BIND_ALT:
        SDL_strlcpy(buf, "Alt", buflen);
        return;
    case INPUT_BIND_CONFIRM:
    case ' ':
        SDL_strlcpy(buf, "Space", buflen);
        return;
    case '\r':
        SDL_strlcpy(buf, "Enter", buflen);
        return;
    case '\t':
        SDL_strlcpy(buf, "Tab", buflen);
        return;
    case ESCAPE:
        SDL_strlcpy(buf, "Esc", buflen);
        return;
    default:
        break;
    }

    if (binding >= 32 && binding <= 126) {
        buf[0] = (char)binding;
        buf[1] = '\0';
    }
}

bool sdl_touch_pane_label_is_symbol_only(const char* label)
{
    return (label && label[0] == '\x01' && label[1] == '\0');
}

bool sdl_touch_pane_should_hide_symbol(const char* name, const char* symbol)
{
    if (!name || !name[0] || !symbol || !symbol[0])
        return false;

    return (SDL_strcasecmp(name, symbol) == 0);
}

void sdl_touch_pane_render_reset_prompt(void)
{
    SDL_Rect screen;
    SDL_FRect rect;
    SDL_Color frame = g_state.palette[TERM_L_BLUE];
    SDL_Color text = g_state.palette[TERM_WHITE];

    if (!g_touch_pane_reset_confirm_active)
        return;

    screen = sdl_get_layout_screen_rect();
    if (screen.w <= 0 || screen.h <= 0)
        return;

    rect = (SDL_FRect){
        .x = screen.x + screen.w * 0.10f,
        .y = screen.y + screen.h * 0.04f,
        .w = screen.w * 0.80f,
        .h = (screen.h < 600) ? 54.0f : 68.0f,
    };

    SDL_SetRenderDrawColor(g_state.renderer, 10, 10, 10, 235);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, 220);
    SDL_RenderRect(g_state.renderer, &rect);
    sdl_touch_pane_draw_button_text(&rect, "Reset touch controls to defaults?",
        "Confirm: Pick/Enter. Cancel: Esc.", text);
}

void sdl_touch_pane_render_yes_no_prompt(void)
{
    SDL_Rect screen;
    const sdl_view* view = &g_views[PANE_MAIN];
    SDL_FRect panel_rect;
    SDL_FRect prompt_rect;
    SDL_FRect yes_rect;
    SDL_FRect no_rect;
    SDL_FRect shadow;
    float cell_h = 16.0f;
    int prompt_font_px;
    SDL_Color frame = g_state.palette[TERM_SLATE];
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    SDL_Color muted = g_state.palette[TERM_SLATE];
    SDL_Color text = g_state.palette[TERM_WHITE];
    bool yes_highlight;
    bool no_highlight;
    SDL_Color yes_fill;
    SDL_Color no_fill;
    SDL_Color yes_border;
    SDL_Color no_border;
    cptr prompt_text = g_touch_pane_yes_no_prompt_text[0]
        ? g_touch_pane_yes_no_prompt_text
        : "Are you sure?";

    if (!sdl_touch_pane_yes_no_prompt_layout(&panel_rect, &prompt_rect,
        &yes_rect, &no_rect))
    {
        return;
    }

    if (view->term_ready && view->cell_h > 0 && view->rows > 0) {
        screen = (SDL_Rect){
            .h = sdl_main_view_visual_rows(view) * view->cell_h
        };
        cell_h = (float)view->cell_h;
    } else {
        screen = sdl_get_layout_screen_rect();
    }
    prompt_font_px = sdl_touch_pane_yes_no_prompt_font_px(cell_h, screen.h);
    yes_highlight =
        (g_touch_pane_yes_no_prompt_hover != SDL_TOUCH_YES_NO_HOVER_NO);
    no_highlight =
        (g_touch_pane_yes_no_prompt_hover == SDL_TOUCH_YES_NO_HOVER_NO);
    yes_fill = yes_highlight ? (SDL_Color){ 37, 47, 63, 255 }
                             : (SDL_Color){ 31, 31, 31, 255 };
    no_fill = no_highlight ? (SDL_Color){ 37, 47, 63, 255 }
                           : (SDL_Color){ 31, 31, 31, 255 };
    yes_border = yes_highlight ? accent : muted;
    no_border = no_highlight ? accent : muted;

    /* Composite explicitly: this overlay draws straight to the window after a
     * chain of other renderers, so the blend mode must not be left to chance or
     * the translucent shadow/panel can paint over the dungeon incorrectly. */
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    shadow = panel_rect;
    shadow.x += 3.0f;
    shadow.y += 3.0f;
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 145);
    SDL_RenderFillRect(g_state.renderer, &shadow);

    SDL_SetRenderDrawColor(g_state.renderer, 18, 18, 18, 248);
    SDL_RenderFillRect(g_state.renderer, &panel_rect);
    SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, 190);
    SDL_RenderRect(g_state.renderer, &panel_rect);

    SDL_SetRenderDrawColor(g_state.renderer, yes_fill.r, yes_fill.g,
        yes_fill.b, yes_fill.a);
    SDL_RenderFillRect(g_state.renderer, &yes_rect);
    SDL_SetRenderDrawColor(g_state.renderer, no_fill.r, no_fill.g,
        no_fill.b, no_fill.a);
    SDL_RenderFillRect(g_state.renderer, &no_rect);

    SDL_SetRenderDrawColor(g_state.renderer, yes_border.r, yes_border.g,
        yes_border.b, yes_highlight ? 235 : 220);
    SDL_RenderRect(g_state.renderer, &yes_rect);
    SDL_SetRenderDrawColor(g_state.renderer, no_border.r, no_border.g,
        no_border.b, no_highlight ? 235 : 220);
    SDL_RenderRect(g_state.renderer, &no_rect);

    sdl_touch_pane_draw_wrapped_prompt(&prompt_rect, prompt_text, text,
        prompt_font_px);
    sdl_touch_pane_draw_button_text_scaled(&yes_rect, NULL, "Yes", text,
        0.36f, 0.46f);
    sdl_touch_pane_draw_button_text_scaled(&no_rect, NULL, "No", text,
        0.36f, 0.46f);
}

void sdl_touch_pane_default_label_for_panel_slot(int panel, int index, char* buf, size_t buflen)
{
    int raw_binding;
    int binding;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    raw_binding = sdl_touch_pane_raw_binding_for_panel(panel, index);
    binding = sdl_touch_pane_effective_binding_for_panel(panel, index);

    if (raw_binding == TOUCH_PANE_BIND_INHERIT)
        return;

    if (binding == GAMEPAD_BIND_NONE) {
        SDL_strlcpy(buf, "Off", buflen);
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        int other_panel = sdl_touch_pane_other_panel(panel);

        sdl_touch_pane_load_default_bindings();
        if (config.touch_pane_panel_names[other_panel][0]) {
            SDL_strlcpy(buf, config.touch_pane_panel_names[other_panel], buflen);
        } else {
            SDL_strlcpy(buf, g_default_touch_pane_panel_names[other_panel], buflen);
        }
        return;
    }

    if (panel == SDL_TOUCH_PANE_PANEL_MAIN
        && binding == g_touch_pane_slots[index].default_binding
        && g_touch_pane_slots[index].default_label
        && g_touch_pane_slots[index].default_label[0]) {
        SDL_strlcpy(buf, g_touch_pane_slots[index].default_label, buflen);
        return;
    }

    if (binding == INPUT_BIND_CONFIRM) {
        SDL_strlcpy(buf, "Confirm", buflen);
        return;
    }

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND) {
        switch (index) {
        case 1:
            if (binding == 'X') {
                SDL_strlcpy(buf, "Exchange", buflen);
                return;
            }
            break;
        case 3:
            if (binding == '\t') {
                SDL_strlcpy(buf, "Ability", buflen);
                return;
            }
            break;
        case 4:
            if (binding == 'e') {
                SDL_strlcpy(buf, "Equip", buflen);
                return;
            }
            break;
        case 5:
            if (binding == '-') {
                SDL_strlcpy(buf, "Fletch", buflen);
                return;
            }
            break;
        case 7:
            if (binding == '0') {
                SDL_strlcpy(buf, "Smith", buflen);
                return;
            }
            break;
        case 8:
            if (binding == 'F') {
                SDL_strlcpy(buf, "Shoot 2", buflen);
                return;
            }
            break;
        case 18:
            if (binding == 'M') {
                SDL_strlcpy(buf, "Map", buflen);
                return;
            }
            break;
        case 19:
            if (binding == 'q') {
                SDL_strlcpy(buf, "Quaff", buflen);
                return;
            }
            if (binding == 'X') {
                SDL_strlcpy(buf, "Exch", buflen);
                return;
            }
            break;
        case 20:
            if (binding == 'p') {
                SDL_strlcpy(buf, "Play", buflen);
                return;
            }
            break;
        default:
            break;
        }
    }

    binding_action_short(binding, buf, buflen);
}

void sdl_touch_pane_proto_label_for_slot(int index, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    switch (index) {
    case SDL_TOUCH_PANE_ESC_SLOT:
        SDL_strlcpy(buf, "Esc", buflen);
        break;
    case SDL_TOUCH_PANE_CENTER_SLOT:
        SDL_strlcpy(buf, "Confirm", buflen);
        break;
    default:
        break;
    }
}

void sdl_touch_pane_base_label_for_slot(int panel, int index, char* buf, size_t buflen)
{
    int raw_binding;
    const char* custom_label = NULL;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    if (sdl_touch_pane_proto_mode_active()) {
        sdl_touch_pane_proto_label_for_slot(index, buf, buflen);
        return;
    }

    raw_binding = sdl_touch_pane_raw_binding_for_panel(panel, index);
    custom_label = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? config.touch_pane_second_labels[index]
        : config.touch_pane_labels[index];

    if (sdl_touch_pane_label_is_symbol_only(custom_label)) {
        sdl_touch_pane_binding_symbol(sdl_touch_pane_effective_binding_for_panel(panel, index),
            buf, buflen);
        return;
    }

    if (custom_label[0]) {
        SDL_strlcpy(buf, custom_label, buflen);
        return;
    }

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND && raw_binding == TOUCH_PANE_BIND_INHERIT) {
        sdl_touch_pane_base_label_for_slot(SDL_TOUCH_PANE_PANEL_MAIN, index, buf, buflen);
        return;
    }

    sdl_touch_pane_default_label_for_panel_slot(panel, index, buf, buflen);
}

void sdl_touch_pane_display_label_for_slot(int panel, int index, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    sdl_touch_pane_base_label_for_slot(panel, index, buf, buflen);
}


