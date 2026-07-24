#include "angband.h"
#include "sdl/main-sdl-private.h"

#define SDL_NARRATIVE_BANNER_TRANSITION_FADE_MS 500

static Uint8 g_sdl_narrative_banner_alpha = SDL_ALPHA_OPAQUE;

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
        return "Caves";
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
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);
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
        /* Right-hand overlays sit flush with the pane edge.  Do this after
         * the general screen-margin clamp so the depth panel shares the same
         * right border as the overlay log. */
        if (pc->where == PLACE_TOP_RIGHT
            || pc->where == PLACE_RIGHT_CENTER
            || pc->where == PLACE_BOTTOM_RIGHT)
        {
            out->panel.x = (float)(anchor.x + anchor.w) - out->panel.w;
        }
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
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0,
        SDL_OVERLAY_LOG_PANE_ALPHA);
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
        zoom_out_text, 0.72f, 0.78f);
    sdl_touch_pane_draw_button_text_scaled(&layout.zoom_in, NULL, "+",
        zoom_in_text, 0.72f, 0.78f);
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
    int font_px = (int)(cell_h * 2.15f);

    if (font_px < 32)
        font_px = 32;
    if (font_px > 56)
        font_px = 56;
    if (screen_h < 480 && font_px > 44)
        font_px = 44;
    if (screen_h < 360 && font_px > 34)
        font_px = 34;
    if (screen_h < 260 && font_px > 28)
        font_px = 28;

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

static bool sdl_narrative_banner_pane_rect(
    const struct pane_config* pc, SDL_FRect* out)
{
    SDL_Rect rect;
    SDL_FRect frect;
    bool have_rect = false;

    if (out)
        *out = (SDL_FRect){ 0 };
    if (!pc || !out || !pc->enabled)
        return false;

    switch (pc->pane) {
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

    case PANE_STATUS_DEPTH:
        return sdl_status_depth_pane_current_rect(out);

    case PANE_LEFT_PANEL:
        if (!sdl_left_panel_pane_presentation_active())
            return false;
        break;

    case PANE_COMBAT:
        if (!sdl_combat_overlay_pane_current_rect(&rect))
            return false;
        have_rect = true;
        break;

    case PANE_ROLLS:
        if (!sdl_overlay_log_pane_current_rect(&rect))
            return false;
        have_rect = true;
        break;

    case PANE_OVERLAY_MENU:
        return sdl_touch_top_panel_compute_layout(NULL, out);

    case PANE_DESCRIPTION:
        return false;

    default:
        break;
    }

    if (pc->pane <= PANE_MAIN || pc->pane >= PANE_MAX)
        return false;
    if (!have_rect)
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

bool sdl_narrative_banner_top_center_pane_rect(
    const struct pane_config* pc, SDL_FRect* out)
{
    if (out)
        *out = (SDL_FRect){ 0 };
    if (!pc || !out || pc->where != PLACE_TOP_CENTER)
        return false;

    return sdl_narrative_banner_pane_rect(pc, out);
}

int sdl_narrative_banner_top_center_panes_bottom(void)
{
    float bottom = 0.0f;
    SDL_FRect menu_button;

    for (int i = 0; i < pane_config_count; i++) {
        SDL_FRect pane_rect;
        float pane_bottom;
        enum pane_placement where = pane_config[i].where;

        /*
         * Both orientations may place persistent HUD panes across the top of
         * the map.  Clear every live top stack vertically; trying to reserve a
         * wide top-right log horizontally can collapse the banner on a narrow
         * screen, and still lets a landscape banner cover the other panes.
         */
        if (where != PLACE_TOP_LEFT && where != PLACE_TOP_CENTER
            && where != PLACE_TOP_RIGHT)
        {
            continue;
        }
        if (!sdl_narrative_banner_pane_rect(&pane_config[i], &pane_rect))
        {
            continue;
        }

        pane_bottom = pane_rect.y + pane_rect.h;
        if (pane_bottom > bottom)
            bottom = pane_bottom;
    }

    /* The fixed Menu button is not part of pane_config. */
    if (sdl_main_menu_pane_button_rect(&menu_button)) {
        float menu_bottom = menu_button.y + menu_button.h;

        if (menu_bottom > bottom)
            bottom = menu_bottom;
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
    if (pane_bottom > rect_bottom - min_h)
        pane_bottom = rect_bottom - min_h;
    rect->y = pane_bottom;
    rect->h = rect_bottom - rect->y;
    if (rect->h < min_h)
        rect->h = min_h;
}

/*
 * Left pixel edge where the overlay log band begins, or 0 when no overlay
 * log pane is active.  Mirrors the band geometry painted in sdl-present.c so
 * the narrative banner can be clipped to stop before it rather than crossing
 * the translucent log on the right-hand side.
 */
int sdl_narrative_banner_overlay_log_left(void)
{
    const sdl_view* view = &g_views[PANE_ROLLS];
    int margin;
    int pad;

    if (!sdl_view_is_overlay_log_pane(view))
        return 0;
    if (view->cell_w <= 0 || view->cols <= 0)
        return 0;

    margin = pane_log_overlay_left_margin(view->cols);
    if (margin <= 0)
        return 0;

    pad = view->cell_w / 8;
    if (pad < 2)
        pad = 2;

    return view->rect.x + view->margin_x + margin * view->cell_w - pad;
}

void sdl_narrative_banner_apply_overlay_log_avoidance(SDL_Rect* rect)
{
    SDL_Rect log_rect;
    int log_left;
    int right_reserve;

    if (!rect || rect->w <= 0 || rect->h <= 0)
        return;
    if (!sdl_overlay_log_pane_current_rect(&log_rect))
        return;
    if (sdl_mobile_portrait_layout_active()
        && (log_rect.y + log_rect.h <= rect->y
            || log_rect.y >= rect->y + rect->h))
    {
        return;
    }

    log_left = log_rect.x;
    if (log_left <= rect->x || log_left >= rect->x + rect->w)
        return;

    /*
     * Reserve the band width on the right (where the overlay log sits) and an
     * equal amount on the left, so the banner keeps clear of the log yet stays
     * centered on the original midpoint of the map area.
     */
    right_reserve = rect->x + rect->w - log_left;
    rect->x += right_reserve;
    rect->w -= 2 * right_reserve;
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

    sdl_narrative_banner_apply_overlay_log_avoidance(&rect);

    if (rect.w <= 0 || rect.h <= 0)
        return false;

    *out = rect;
    return true;
}

int sdl_narrative_banner_font_px(const SDL_Rect* rect)
{
    int font_px;

    if (!rect || rect->h <= 0)
        return 0;

    /* Use the same font size as the message log pane. */
    font_px = sdl_effective_pane_cell_height_for_type(PANE_LOG);
    if (font_px < 8)
        font_px = 8;

    return font_px;
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
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);
    if (!font)
        return 0;

    return sdl_narrative_banner_wrap_lines(active_narrative_banner_text(),
        font, sdl_narrative_banner_max_text_w(&rect, font_px), lines,
        SDL_NARRATIVE_BANNER_MAX_LINES);
}

static bool sdl_narrative_banner_layout(SDL_FRect* out_panel,
    float* out_pad_x, float* out_pad_y, float* out_line_h,
    int* out_shown_lines,
    char lines[][SDL_NARRATIVE_BANNER_LINE_LEN], int max_lines,
    TTF_Font** out_font)
{
    SDL_Rect rect;
    TTF_Font* font;
    int font_px;
    int line_count;
    int shown_lines;
    int max_line_w = 0;
    float max_text_w;
    float pad_x;
    float pad_y;
    float line_h;
    float panel_w;
    float max_panel_w;
    float panel_h;
    float top_gap;
    SDL_FRect panel;
    int overlay_log_left;

    if (!sdl_narrative_banner_overlay_enabled())
        return false;
    if (!active_narrative_banner_visible() || character_icky > 0)
        return false;
    if (!sdl_narrative_banner_base_rect(&rect))
        return false;

    font_px = sdl_narrative_banner_font_px(&rect);
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);
    if (!font)
        return false;

    max_text_w = sdl_narrative_banner_max_text_w(&rect, font_px);
    line_count = sdl_narrative_banner_wrap_lines(active_narrative_banner_text(),
        font, max_text_w, lines, max_lines);
    if (line_count <= 0)
        return false;

    shown_lines = line_count;
    if (shown_lines <= 0)
        return false;

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
    max_panel_w = (float)rect.w - 8.0f;
    if (panel_w > max_panel_w)
        panel_w = max_panel_w;
    if (panel_w < 80.0f)
        panel_w = (float)rect.w;

    overlay_log_left = sdl_narrative_banner_overlay_log_left();
    if (overlay_log_left > rect.x
        && overlay_log_left <= rect.x + rect.w + 2
        && panel_w < max_panel_w)
    {
        /*
         * The safe rect already stops at the overlay log.  If the wrapped text
         * is narrower, widen the panel halfway into the remaining side space
         * so both left and right gaps shrink while the banner stays centered.
         */
        panel_w += (max_panel_w - panel_w) * 0.5f;
    }

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

    if (out_panel)
        *out_panel = panel;
    if (out_pad_x)
        *out_pad_x = pad_x;
    if (out_pad_y)
        *out_pad_y = pad_y;
    if (out_line_h)
        *out_line_h = line_h;
    if (out_shown_lines)
        *out_shown_lines = shown_lines;
    if (out_font)
        *out_font = font;

    return true;
}

void sdl_narrative_banner_draw_line(TTF_Font* font, cptr text,
    SDL_Color color, float left_x, float y, float max_w, float line_h)
{
    SDL_Texture* texture;
    SDL_FRect dst;
    float scale = 1.0f;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || max_w <= 0.0f || line_h <= 0.0f
        || color.a == 0)
    {
        return;
    }

    texture = sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture)
        return;

    if (text_w > 0 && (float)text_w > max_w)
        scale = max_w / (float)text_w;

    dst = (SDL_FRect){
        .w = (float)text_w * scale,
        .h = (float)text_h * scale,
    };
    dst.x = left_x;
    dst.y = y + (line_h - dst.h) * 0.5f;

    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
}

static Uint8 sdl_narrative_banner_scaled_alpha(Uint8 alpha)
{
    if (g_sdl_narrative_banner_alpha == 0 || alpha == 0)
        return 0;
    if (g_sdl_narrative_banner_alpha >= SDL_ALPHA_OPAQUE)
        return alpha;

    return (Uint8)(((int)alpha * (int)g_sdl_narrative_banner_alpha
        + SDL_ALPHA_OPAQUE / 2) / SDL_ALPHA_OPAQUE);
}

void sdl_narrative_banner_render(void)
{
    TTF_Font* font;
    char lines[SDL_NARRATIVE_BANNER_MAX_LINES][SDL_NARRATIVE_BANNER_LINE_LEN];
    int shown_lines;
    float pad_x;
    float pad_y;
    float line_h;
    SDL_FRect panel;
    SDL_Color text_color;
    Uint8 panel_alpha;
    Uint8 border_alpha;

    if (!sdl_narrative_banner_layout(&panel, &pad_x, &pad_y, &line_h,
            &shown_lines, lines, SDL_NARRATIVE_BANNER_MAX_LINES, &font))
    {
        return;
    }

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    panel_alpha = sdl_narrative_banner_scaled_alpha(212);
    border_alpha = sdl_narrative_banner_scaled_alpha(126);
    if (panel_alpha > 0) {
        SDL_SetRenderDrawColor(g_state.renderer, 4, 5, 7, panel_alpha);
        SDL_RenderFillRect(g_state.renderer, &panel);
    }
    if (border_alpha > 0) {
        SDL_SetRenderDrawColor(g_state.renderer, 255, 184, 94,
            border_alpha);
        SDL_RenderRect(g_state.renderer, &panel);
    }

    text_color = (SDL_Color){
        angband_color_table[TERM_ORANGE][1],
        angband_color_table[TERM_ORANGE][2],
        angband_color_table[TERM_ORANGE][3],
        sdl_narrative_banner_scaled_alpha(SDL_ALPHA_OPAQUE),
    };

    for (int i = 0; i < shown_lines; i++) {
        sdl_narrative_banner_draw_line(font, lines[i], text_color,
            panel.x + pad_x,
            panel.y + pad_y + line_h * (float)i,
            panel.w - pad_x * 2.0f, line_h);
    }
}

bool sdl_narrative_banner_handle_pointer(float x, float y)
{
    SDL_FRect panel;
    char lines[SDL_NARRATIVE_BANNER_MAX_LINES][SDL_NARRATIVE_BANNER_LINE_LEN];

    if (!sdl_narrative_banner_layout(&panel, NULL, NULL, NULL,
            NULL, lines, SDL_NARRATIVE_BANNER_MAX_LINES, NULL))
    {
        return false;
    }

    if (x < panel.x || x >= panel.x + panel.w
        || y < panel.y || y >= panel.y + panel.h)
    {
        return false;
    }

    (void)dismiss_active_narrative_banner();
    do_cmd_redraw();
    g_state.need_present = true;
    return true;
}

static void sdl_narrative_banner_present_frame(sdl_view* restore_view)
{
    g_state.need_present = true;
    if (sdl_render_current_window_frame()) {
        SDL_RenderPresent(g_state.renderer);
        g_state.need_present = false;
    }
    sdl_restore_render_target(restore_view);
}

void sdl_narrative_banner_show(bool line_delay, bool fast_fade)
{
    sdl_view* restore_view;

    if (!sdl_narrative_banner_overlay_enabled())
        return;
    if (!active_narrative_banner_visible())
        return;

    /* Banner presentation is an input boundary.  Do not let a touch press
     * armed while the previous command was being collected survive into the
     * banner and age into a Ctrl+direction gesture. */
    sdl_touch_cancel_all_inputs();

    restore_view = sdl_view_from_term(Term);
    if (!restore_view)
        restore_view = &g_views[PANE_MAIN];

    if (!line_delay) {
        g_sdl_narrative_banner_alpha = SDL_ALPHA_OPAQUE;
        sdl_narrative_banner_present_frame(restore_view);
        return;
    }

    Uint64 start_ns = SDL_GetTicksNS();
    int fade_ms = fast_fade ? SDL_NARRATIVE_BANNER_TRANSITION_FADE_MS
                            : SDL_NARRATIVE_BANNER_FADE_MS;
    Uint64 fade_ns = (Uint64)fade_ms * 1000000ULL;

    for (;;) {
        Uint64 elapsed_ns = SDL_GetTicksNS() - start_ns;

        if (elapsed_ns >= fade_ns) {
            g_sdl_narrative_banner_alpha = SDL_ALPHA_OPAQUE;
        } else {
            g_sdl_narrative_banner_alpha =
                (Uint8)((elapsed_ns * SDL_ALPHA_OPAQUE + fade_ns / 2)
                    / fade_ns);
        }

        sdl_narrative_banner_present_frame(restore_view);
        if (g_sdl_narrative_banner_alpha >= SDL_ALPHA_OPAQUE)
            break;

        SDL_Delay(SDL_NARRATIVE_BANNER_FADE_FRAME_MS);
    }

    g_sdl_narrative_banner_alpha = SDL_ALPHA_OPAQUE;
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
    prompt_font = sdl_story_font_for_height_slot(prompt_font_px, SDL_STORY_FONT_SLOT_MENU);
    prompt_text_w = sdl_touch_pane_story_text_width(prompt_font, prompt_text);

    margin = sdl_touch_pane_clampf(cell_h * 1.45f, 24.0f, 46.0f);
    button_gap = sdl_touch_pane_clampf(cell_w * 1.90f, 18.0f, 34.0f);
    row_gap = sdl_touch_pane_clampf(cell_h * 1.10f, 18.0f, 34.0f);
    button_w = sdl_touch_pane_clampf(cell_w * 13.50f, 136.0f, 220.0f);
    button_h = sdl_touch_pane_clampf(cell_h * 3.80f, 72.0f, 112.0f);

    max_panel_w = (float)screen.w * 0.94f;
    if (max_panel_w > cell_w * 82.0f)
        max_panel_w = cell_w * 82.0f;
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
    prompt_line_h = (float)prompt_font_px * 1.42f;
    if (prompt_line_h < 24.0f)
        prompt_line_h = 24.0f;
    prompt_h = prompt_line_h * (float)prompt_line_count;

    /* Let the panel grow to fit every wrapped line at the fixed font size.
     * Only as a last resort (a prompt taller than the whole screen) do we
     * compress the text block; normally we add rows instead of shrinking. */
    max_panel_h = (float)screen.h - 8.0f;
    if (max_panel_h <= 0.0f)
        max_panel_h = (float)screen.h;
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

    /* Local questions spawn next to the map grid they are about (below the
     * cell, else above), so the player can see the object being asked
     * about; fall back to the centred placement when it is off-screen. */
    if (g_touch_pane_yes_no_prompt_anchor_active)
    {
        SDL_FRect anchor_cell;

        if (sdl_map_grid_cell_rect(g_touch_pane_yes_no_prompt_anchor_y,
                g_touch_pane_yes_no_prompt_anchor_x, &anchor_cell))
        {
            float gap = sdl_touch_pane_clampf(cell_h * 0.45f, 6.0f, 16.0f);

            x = anchor_cell.x + anchor_cell.w * 0.5f - panel_w * 0.5f;
            y = anchor_cell.y + anchor_cell.h + gap;
            if (y + panel_h > (float)screen.y + (float)screen.h - gap)
                y = anchor_cell.y - panel_h - gap;

            if (x + panel_w > (float)screen.x + (float)screen.w - gap)
                x = (float)screen.x + (float)screen.w - gap - panel_w;
            if (y + panel_h > (float)screen.y + (float)screen.h - gap)
                y = (float)screen.y + (float)screen.h - gap - panel_h;
        }
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
    g_touch_pane_yes_no_prompt_anchor_active = false;
    g_touch_pane_yes_no_prompt_anchor_y = 0;
    g_touch_pane_yes_no_prompt_anchor_x = 0;
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
    int name_w = 0;
    int name_h_px = 0;
    int symbol_w = 0;
    int symbol_h_px = 0;

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

    /* Resolve both fonts before borrowing either cached texture: loading the
     * second font may evict an older story-font entry. */
    if (have_name)
        name_font = sdl_story_font_for_height_slot(name_font_px,
            SDL_STORY_FONT_SLOT_MENU);
    if (have_symbol)
        symbol_font = sdl_story_font_for_height_slot(symbol_font_px,
            SDL_STORY_FONT_SLOT_MENU);
    if (name_font)
        name_texture = sdl_ui_text_texture(name_font, name, color,
            &name_w, &name_h_px);
    if (symbol_font)
        symbol_texture = sdl_ui_text_texture(symbol_font, symbol, color,
            &symbol_w, &symbol_h_px);

    if (!name_texture && !symbol_texture)
        return;

    gap = rect->h * 0.03f;
    if (gap < 2.0f)
        gap = 2.0f;

    if (name_texture && symbol_texture) {
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

        name_scale_w = (name_w > 0) ? (name_max_w / (float)name_w) : 1.0f;
        name_scale_h = (name_h_px > 0) ? (name_max_h / (float)name_h_px) : 1.0f;
        name_scale = (name_scale_w < name_scale_h) ? name_scale_w : name_scale_h;
        if (name_scale > 1.0f)
            name_scale = 1.0f;

        symbol_scale_w = (symbol_w > 0) ? (symbol_max_w / (float)symbol_w) : 1.0f;
        symbol_scale_h = (symbol_h_px > 0) ? (symbol_max_h / (float)symbol_h_px) : 1.0f;
        symbol_scale = (symbol_scale_w < symbol_scale_h) ? symbol_scale_w : symbol_scale_h;
        if (symbol_scale > 1.0f)
            symbol_scale = 1.0f;

        total_h = (float)name_h_px * name_scale + gap
            + (float)symbol_h_px * symbol_scale;
        scale = 1.0f;
        if (total_h > avail_h && total_h > 0.0f)
            scale = avail_h / total_h;

        name_scale *= scale;
        symbol_scale *= scale;
        name_h = (float)name_h_px * name_scale;
        symbol_h = (float)symbol_h_px * symbol_scale;
        start_y = rect->y + (rect->h - (name_h + gap + symbol_h)) * 0.5f;

        name_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)name_w * name_scale) * 0.5f,
            .y = start_y,
            .w = (float)name_w * name_scale,
            .h = name_h,
        };
        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)symbol_w * symbol_scale) * 0.5f,
            .y = start_y + name_h + gap,
            .w = (float)symbol_w * symbol_scale,
            .h = symbol_h,
        };

        SDL_RenderTexture(g_state.renderer, name_texture, NULL, &name_dst);
        SDL_RenderTexture(g_state.renderer, symbol_texture, NULL, &symbol_dst);
    } else {
        SDL_Texture* only_texture = name_texture ? name_texture : symbol_texture;
        int only_w = name_texture ? name_w : symbol_w;
        int only_h = name_texture ? name_h_px : symbol_h_px;
        float max_w = rect->w * 0.82f;
        float max_h = rect->h
            * ((!have_name && have_symbol) ? single_text_height_ratio : 0.38f);
        float scale_w = (only_w > 0) ? (max_w / (float)only_w) : 1.0f;
        float scale_h = (only_h > 0) ? (max_h / (float)only_h) : 1.0f;
        float scale = (scale_w < scale_h) ? scale_w : scale_h;

        if (scale > 1.0f)
            scale = 1.0f;

        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)only_w * scale) * 0.5f,
            .y = rect->y + (rect->h - (float)only_h * scale) * 0.5f,
            .w = (float)only_w * scale,
            .h = (float)only_h * scale,
        };

        SDL_RenderTexture(g_state.renderer, only_texture, NULL, &symbol_dst);
    }
}

void sdl_touch_pane_draw_button_text(const SDL_FRect* rect, const char* name, const char* symbol,
    SDL_Color color)
{
    sdl_touch_pane_draw_button_text_scaled(rect, name, symbol, color, 0.28f, 0.38f);
}

void sdl_touch_pane_draw_button_text_px(const SDL_FRect* rect,
    const char* name, const char* symbol, SDL_Color color, int name_px,
    int symbol_px)
{
    SDL_Texture* name_texture = NULL;
    SDL_Texture* symbol_texture = NULL;
    TTF_Font* name_font = NULL;
    TTF_Font* symbol_font = NULL;
    bool have_name;
    bool have_symbol;
    float gap;
    SDL_FRect name_dst;
    SDL_FRect symbol_dst;
    int name_w = 0;
    int name_h_px = 0;
    int symbol_w = 0;
    int symbol_h_px = 0;

    if (!rect)
        return;

    have_name = (name && name[0]);
    have_symbol = (symbol && symbol[0]);
    if (!have_name && !have_symbol)
        return;

    if (name_px < 8)
        name_px = 8;
    if (symbol_px < 8)
        symbol_px = 8;

    if (have_name)
        name_font = sdl_story_font_for_height_slot(name_px,
            SDL_STORY_FONT_SLOT_MENU);
    if (have_symbol)
        symbol_font = sdl_story_font_for_height_slot(symbol_px,
            SDL_STORY_FONT_SLOT_MENU);
    if (name_font)
        name_texture = sdl_ui_text_texture(name_font, name, color,
            &name_w, &name_h_px);
    if (symbol_font)
        symbol_texture = sdl_ui_text_texture(symbol_font, symbol, color,
            &symbol_w, &symbol_h_px);

    if (!name_texture && !symbol_texture)
        return;

    gap = rect->h * 0.03f;
    if (gap < 2.0f)
        gap = 2.0f;

    if (name_texture && symbol_texture) {
        float max_w = rect->w * 0.92f;
        float avail_h = rect->h - 4.0f;
        float name_scale = 1.0f;
        float symbol_scale = 1.0f;
        float total_h;
        float scale;
        float name_h;
        float symbol_h;
        float start_y;

        if (name_w > 0 && (float)name_w > max_w)
            name_scale = max_w / (float)name_w;
        if (symbol_w > 0 && (float)symbol_w > max_w)
            symbol_scale = max_w / (float)symbol_w;

        total_h = (float)name_h_px * name_scale + gap
            + (float)symbol_h_px * symbol_scale;
        scale = 1.0f;
        if (avail_h > 0.0f && total_h > avail_h)
            scale = avail_h / total_h;

        name_scale *= scale;
        symbol_scale *= scale;
        name_h = (float)name_h_px * name_scale;
        symbol_h = (float)symbol_h_px * symbol_scale;
        start_y = rect->y + (rect->h - (name_h + gap + symbol_h)) * 0.5f;

        name_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)name_w * name_scale) * 0.5f,
            .y = start_y,
            .w = (float)name_w * name_scale,
            .h = name_h,
        };
        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)symbol_w * symbol_scale) * 0.5f,
            .y = start_y + name_h + gap,
            .w = (float)symbol_w * symbol_scale,
            .h = symbol_h,
        };

        SDL_RenderTexture(g_state.renderer, name_texture, NULL, &name_dst);
        SDL_RenderTexture(g_state.renderer, symbol_texture, NULL, &symbol_dst);
    } else {
        SDL_Texture* only_texture = name_texture ? name_texture : symbol_texture;
        int only_w = name_texture ? name_w : symbol_w;
        int only_h = name_texture ? name_h_px : symbol_h_px;
        float max_w = rect->w * 0.92f;
        float max_h = rect->h - 4.0f;
        float scale_w = (only_w > 0) ? (max_w / (float)only_w) : 1.0f;
        float scale_h = (only_h > 0) ? (max_h / (float)only_h) : 1.0f;
        float scale = (scale_w < scale_h) ? scale_w : scale_h;

        if (scale > 1.0f)
            scale = 1.0f;

        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)only_w * scale) * 0.5f,
            .y = rect->y + (rect->h - (float)only_h * scale) * 0.5f,
            .w = (float)only_w * scale,
            .h = (float)only_h * scale,
        };

        SDL_RenderTexture(g_state.renderer, only_texture, NULL, &symbol_dst);
    }
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

    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);
    if (!font)
        return;

    line_count = sdl_touch_pane_wrap_prompt_lines(text, font, rect->w,
        lines, SDL_TOUCH_YES_NO_MAX_LINES);
    if (line_count <= 0)
        return;

    line_h = (float)font_px * 1.42f;
    if (line_h < 24.0f)
        line_h = 24.0f;
    if (line_h * (float)line_count > rect->h)
        line_h = rect->h / (float)line_count;
    if (line_h < 10.0f)
        line_h = 10.0f;

    total_h = line_h * (float)line_count;
    start_y = rect->y + (rect->h - total_h) * 0.5f;
    if (start_y < rect->y)
        start_y = rect->y;

    for (int i = 0; i < line_count; i++) {
        SDL_Texture* texture;
        float max_w;
        float max_h;
        float scale_w;
        float scale_h;
        float scale;
        SDL_FRect dst;
        int text_w = 0;
        int text_h = 0;

        if (!lines[i][0])
            continue;

        texture = sdl_ui_text_texture(font, lines[i], color, &text_w,
            &text_h);
        if (!texture)
            continue;

        max_w = rect->w;
        max_h = line_h;
        scale_w = (text_w > 0) ? (max_w / (float)text_w) : 1.0f;
        scale_h = (text_h > 0) ? (max_h / (float)text_h) : 1.0f;
        scale = (scale_w < scale_h) ? scale_w : scale_h;
        if (scale > 1.0f)
            scale = 1.0f;

        dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)text_w * scale) * 0.5f,
            .y = start_y + (float)i * line_h
                + (line_h - (float)text_h * scale) * 0.5f,
            .w = (float)text_w * scale,
            .h = (float)text_h * scale,
        };

        SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
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
    SDL_Color text = g_state.palette[TERM_WHITE];
    SDL_Color button_text = (SDL_Color){ 0, 0, 0, 255 };
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
    yes_fill = yes_highlight ? (SDL_Color){ 245, 245, 245, 255 }
                             : (SDL_Color){ 156, 156, 156, 238 };
    no_fill = no_highlight ? (SDL_Color){ 245, 245, 245, 255 }
                           : (SDL_Color){ 156, 156, 156, 238 };
    yes_border = yes_highlight ? (SDL_Color){ 0, 0, 0, 255 }
                               : (SDL_Color){ 28, 28, 28, 230 };
    no_border = no_highlight ? (SDL_Color){ 0, 0, 0, 255 }
                             : (SDL_Color){ 28, 28, 28, 230 };

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
    sdl_touch_pane_draw_button_text_scaled(&yes_rect, NULL, "Yes",
        button_text, 0.44f, 0.58f);
    sdl_touch_pane_draw_button_text_scaled(&no_rect, NULL, "No", button_text,
        0.44f, 0.58f);
}

static int sdl_unified_look_log_text_font_px(void)
{
    const sdl_view* log_view = &g_views[PANE_LOG];
    const sdl_view* rolls_view = &g_views[PANE_ROLLS];
    int font_px = 0;

    if (log_view->term_ready && log_view->cell_h > 0)
        font_px = log_view->cell_h;
    else if (sdl_view_is_overlay_log_pane(rolls_view)
        && rolls_view->cell_h > 0)
    {
        font_px = rolls_view->cell_h;
    }
    else
    {
        font_px = sdl_effective_pane_cell_height_for_type(PANE_LOG);
    }

    if (font_px < 8)
        font_px = 8;

    return font_px;
}

static void sdl_unified_look_draw_centered_text(TTF_Font* font, cptr text,
    SDL_Color color, const SDL_FRect* rect)
{
    SDL_Texture* texture;
    SDL_FRect dst;
    float scale = 1.0f;
    float max_w;
    float max_h;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || !rect
        || rect->w <= 0.0f || rect->h <= 0.0f)
    {
        return;
    }

    texture = sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture)
        return;

    max_w = rect->w - 6.0f;
    max_h = rect->h - 4.0f;
    if (max_w < 1.0f)
        max_w = rect->w;
    if (max_h < 1.0f)
        max_h = rect->h;

    if (text_w > 0 && (float)text_w > max_w)
        scale = max_w / (float)text_w;
    if (text_h > 0 && (float)text_h * scale > max_h)
        scale = max_h / (float)text_h;

    dst.w = (float)text_w * scale;
    dst.h = (float)text_h * scale;
    dst.x = rect->x + (rect->w - dst.w) * 0.5f;
    dst.y = rect->y + (rect->h - dst.h) * 0.5f;

    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
}

typedef struct sdl_unified_look_prompt_layout_info {
    SDL_FRect area;
    SDL_FRect panel;
    SDL_FRect buttons[SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
    int count;
    int variant;
    int font_px;
} sdl_unified_look_prompt_layout_info;

static cptr sdl_unified_look_prompt_label(
    const sdl_unified_look_prompt_button_state* button, int variant)
{
    if (!button)
        return "";
    if (variant < 0)
        variant = 0;
    if (variant >= SDL_UNIFIED_LOOK_PROMPT_LABEL_VARIANTS)
        variant = SDL_UNIFIED_LOOK_PROMPT_LABEL_VARIANTS - 1;

    for (int i = variant; i < SDL_UNIFIED_LOOK_PROMPT_LABEL_VARIANTS; i++)
    {
        if (button->labels[i][0])
            return button->labels[i];
    }
    for (int i = variant - 1; i >= 0; i--)
    {
        if (button->labels[i][0])
            return button->labels[i];
    }

    return "";
}

static int sdl_unified_look_prompt_text_width(TTF_Font* font, cptr text,
    int font_px)
{
    int width = sdl_touch_pane_story_text_width(font, text);

    if (width > 0)
        return width;
    if (!text || !text[0])
        return 0;

    return (int)((float)strlen(text) * (float)font_px * 0.55f);
}

static bool sdl_unified_look_prompt_try_layout(
    sdl_unified_look_prompt_layout_info* out, int variant, TTF_Font* font,
    const SDL_FRect* area, int anchor_row, float cell_h, float screen_margin,
    float panel_pad, float row_gap, float button_gap, float button_pad_x,
    float row_h, float min_button_w, int max_rows)
{
    float available_w;
    float max_panel_h;
    float button_w[SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
    float row_width[SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
    int row_start[SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
    int row_end[SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
    int row_count = 1;
    int count;
    float current_w = 0.0f;
    float max_row_w = 0.0f;
    float panel_w;
    float panel_h;
    float anchor_bottom;
    float y;

    if (!out || !font || !area)
        return false;
    count = g_unified_look_prompt.count;
    if (count <= 0)
        return false;
    if (count > SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS)
        count = SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS;
    if (max_rows < 1)
        max_rows = 1;
    if (max_rows > SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS)
        max_rows = SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS;

    available_w = area->w - (screen_margin * 2.0f) - (panel_pad * 2.0f);
    if (available_w <= 20.0f)
        return false;

    row_start[0] = 0;
    for (int i = 0; i < count; i++)
    {
        cptr label = sdl_unified_look_prompt_label(
            &g_unified_look_prompt.buttons[i], variant);
        float width = (float)sdl_unified_look_prompt_text_width(font, label,
            out->font_px) + button_pad_x * 2.0f;

        if (width < min_button_w)
            width = min_button_w;
        if (width > available_w)
            width = available_w;
        button_w[i] = width;

        if (current_w > 0.0f
            && current_w + button_gap + width > available_w)
        {
            row_width[row_count - 1] = current_w;
            row_end[row_count - 1] = i;
            row_count++;
            if (row_count > max_rows)
                return false;
            row_start[row_count - 1] = i;
            current_w = 0.0f;
        }

        if (current_w > 0.0f)
            current_w += button_gap;
        current_w += width;
    }

    row_width[row_count - 1] = current_w;
    row_end[row_count - 1] = count;

    for (int row = 0; row < row_count; row++)
    {
        if (row_width[row] > max_row_w)
            max_row_w = row_width[row];
    }

    panel_w = max_row_w + panel_pad * 2.0f;
    panel_h = panel_pad * 2.0f + row_h * (float)row_count
        + row_gap * (float)(row_count - 1);
    max_panel_h = area->h - screen_margin * 2.0f;
    if (panel_h > max_panel_h && max_panel_h > 20.0f)
    {
        row_h = (max_panel_h - panel_pad * 2.0f
            - row_gap * (float)(row_count - 1)) / (float)row_count;
        if (row_h < 18.0f)
            return false;
        panel_h = max_panel_h;
    }

    out->panel = (SDL_FRect){
        .x = area->x + (area->w - panel_w) * 0.5f,
        .w = panel_w,
        .h = panel_h,
    };

    anchor_bottom = area->y + area->h - screen_margin;
    if (anchor_row >= 0 && cell_h > 0.0f)
    {
        float row_bottom = area->y + ((float)anchor_row + 1.0f) * cell_h;

        if (row_bottom > area->y + screen_margin)
            anchor_bottom = row_bottom;
        if (anchor_bottom > area->y + area->h - screen_margin)
            anchor_bottom = area->y + area->h - screen_margin;
    }
    out->panel.y = anchor_bottom - panel_h;
    if (out->panel.y < area->y + screen_margin)
        out->panel.y = area->y + screen_margin;
    if (out->panel.y + out->panel.h > area->y + area->h - screen_margin)
        out->panel.y = area->y + area->h - screen_margin - out->panel.h;

    for (int row = 0; row < row_count; row++)
    {
        float x = out->panel.x + (out->panel.w - row_width[row]) * 0.5f;

        y = out->panel.y + panel_pad
            + (float)row * (row_h + row_gap);
        for (int i = row_start[row]; i < row_end[row]; i++)
        {
            out->buttons[i] = (SDL_FRect){
                .x = x,
                .y = y,
                .w = button_w[i],
                .h = row_h,
            };
            x += button_w[i] + button_gap;
        }
    }

    out->count = count;
    out->variant = variant;
    return true;
}

static bool sdl_unified_look_prompt_layout(
    sdl_unified_look_prompt_layout_info* out)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int visual_cols;
    int visual_rows;
    float cell_w;
    float cell_h;
    float screen_margin;
    float panel_pad;
    float row_gap;
    float button_gap;
    float button_pad_x;
    float row_h;
    float min_button_w;
    int base_font_px;
    int min_font_px;
    TTF_Font* font;
    SDL_FRect area;
    sdl_unified_look_prompt_layout_info best_layout = { 0 };
    bool found_layout = false;

    if (!out)
        return false;
    *out = (sdl_unified_look_prompt_layout_info){ 0 };

    if (!g_unified_look_prompt.active || g_unified_look_prompt.count <= 0)
        return false;
    if (!view->term_ready || !view->canvas)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0)
        return false;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    if (visual_cols <= 0 || visual_rows <= 0)
        return false;

    cell_w = (float)view->cell_w;
    cell_h = (float)view->cell_h;
    out->area = (SDL_FRect){
        .x = (float)(view->rect.x + view->margin_x),
        .y = (float)(view->rect.y + view->margin_y),
        .w = (float)visual_cols * cell_w,
        .h = (float)visual_rows * cell_h,
    };
    if (out->area.w <= 0.0f || out->area.h <= 0.0f)
        return false;
    area = out->area;

    screen_margin = sdl_touch_pane_clampf(cell_w * 0.65f, 5.0f, 14.0f);
    base_font_px = sdl_unified_look_log_text_font_px();
    min_font_px = 5;
    if (base_font_px < min_font_px)
        base_font_px = min_font_px;

    {
        int low_px = min_font_px;
        int high_px = base_font_px;

        while (low_px <= high_px) {
            int font_px = low_px + (high_px - low_px) / 2;
            sdl_unified_look_prompt_layout_info candidate = {
                .area = area,
                .font_px = font_px,
            };
            bool fits = false;

            font = sdl_story_font_for_height_slot(font_px,
                SDL_STORY_FONT_SLOT_LOG);
            if (!font) {
                high_px = font_px - 1;
                continue;
            }

            panel_pad = sdl_touch_pane_clampf((float)font_px * 0.45f,
                4.0f, 12.0f);
            row_gap = 0.0f;
            button_gap = sdl_touch_pane_clampf((float)font_px * 0.42f,
                2.0f, 10.0f);
            button_pad_x = sdl_touch_pane_clampf((float)font_px * 0.70f,
                4.0f, 16.0f);
            row_h = (float)font_px * 1.55f;
            if (row_h < (float)font_px + 7.0f)
                row_h = (float)font_px + 7.0f;
            min_button_w = sdl_touch_pane_clampf((float)font_px * 2.2f,
                14.0f, 58.0f);

            for (int variant = 0;
                 variant < SDL_UNIFIED_LOOK_PROMPT_LABEL_VARIANTS; variant++)
            {
                candidate = (sdl_unified_look_prompt_layout_info){
                    .area = area,
                    .font_px = font_px,
                };
                if (sdl_unified_look_prompt_try_layout(&candidate, variant,
                        font, &area, g_unified_look_prompt.anchor_row, cell_h,
                        screen_margin, panel_pad, row_gap, button_gap,
                        button_pad_x, row_h, min_button_w, 1))
                {
                    fits = true;
                    break;
                }
            }
            if (fits) {
                best_layout = candidate;
                found_layout = true;
                low_px = font_px + 1;
            } else {
                high_px = font_px - 1;
            }
        }
    }

    if (found_layout) {
        *out = best_layout;
        return true;
    }

    out->area = area;
    out->font_px = min_font_px;
    font = sdl_story_font_for_height_slot(min_font_px,
        SDL_STORY_FONT_SLOT_LOG);
    if (!font)
        return false;

    panel_pad = 4.0f;
    row_gap = 0.0f;
    button_gap = 2.0f;
    button_pad_x = 4.0f;
    row_h = (float)min_font_px + 7.0f;
    min_button_w = 0.0f;

    return sdl_unified_look_prompt_try_layout(out,
        SDL_UNIFIED_LOOK_PROMPT_LABEL_VARIANTS - 1, font, &out->area,
        g_unified_look_prompt.anchor_row, cell_h, screen_margin, panel_pad,
        row_gap, button_gap, button_pad_x, row_h, min_button_w, 1);
}

void sdl_unified_look_prompt_clear(void)
{
    if (g_unified_look_prompt.active || g_unified_look_prompt.count > 0)
        g_state.need_present = true;

    memset(&g_unified_look_prompt, 0, sizeof(g_unified_look_prompt));
    g_unified_look_prompt.anchor_row = -1;
}

void sdl_unified_look_prompt_begin(int anchor_row)
{
    g_unified_look_prompt.active = true;
    g_unified_look_prompt.anchor_row = anchor_row;
    g_unified_look_prompt.count = 0;
    g_state.need_present = true;
}

static void sdl_unified_look_prompt_copy_label(char* dst, cptr src)
{
    if (!dst)
        return;

    SDL_strlcpy(dst, src ? src : "", SDL_UNIFIED_LOOK_PROMPT_LABEL_LEN);
}

void sdl_unified_look_prompt_add(int choice, cptr full, cptr medium,
    cptr compact, cptr tiny)
{
    sdl_unified_look_prompt_button_state* button;

    if (!g_unified_look_prompt.active)
        return;
    if (g_unified_look_prompt.count >= SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS)
        return;
    if ((!full || !full[0]) && (!medium || !medium[0])
        && (!compact || !compact[0]) && (!tiny || !tiny[0]))
    {
        return;
    }

    button = &g_unified_look_prompt.buttons[g_unified_look_prompt.count++];
    memset(button, 0, sizeof(*button));
    button->choice = choice;
    sdl_unified_look_prompt_copy_label(button->labels[0], full);
    sdl_unified_look_prompt_copy_label(button->labels[1], medium);
    sdl_unified_look_prompt_copy_label(button->labels[2], compact);
    sdl_unified_look_prompt_copy_label(button->labels[3], tiny);
    g_state.need_present = true;
}

void sdl_unified_look_prompt_finish(void)
{
    if (!g_unified_look_prompt.active || g_unified_look_prompt.count <= 0)
    {
        sdl_unified_look_prompt_clear();
        return;
    }

    g_state.need_present = true;
}

void sdl_unified_look_prompt_render(void)
{
    sdl_unified_look_prompt_layout_info layout;
    SDL_FRect shadow;
    SDL_Color frame = g_state.palette[TERM_SLATE];
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    SDL_Color text = g_state.palette[TERM_WHITE];
    TTF_Font* font;
    int hover_choice = 0;
    bool has_hover_choice;

    if (!sdl_unified_look_prompt_layout(&layout))
        return;

    font = sdl_story_font_for_height_slot(layout.font_px,
        SDL_STORY_FONT_SLOT_LOG);
    if (!font)
        return;

    has_hover_choice = ui_menu_click_get_hover_choice(&hover_choice);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    shadow = layout.panel;
    shadow.x += 3.0f;
    shadow.y += 3.0f;
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 125);
    SDL_RenderFillRect(g_state.renderer, &shadow);

    SDL_SetRenderDrawColor(g_state.renderer, 13, 15, 17, 232);
    SDL_RenderFillRect(g_state.renderer, &layout.panel);
    SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, 178);
    SDL_RenderRect(g_state.renderer, &layout.panel);

    for (int i = 0; i < layout.count; i++)
    {
        const sdl_unified_look_prompt_button_state* button =
            &g_unified_look_prompt.buttons[i];
        cptr label = sdl_unified_look_prompt_label(button, layout.variant);
        bool selected = has_hover_choice && hover_choice == button->choice;
        SDL_Color border = selected ? accent : frame;

        if (selected)
            SDL_SetRenderDrawColor(g_state.renderer, 37, 47, 63, 238);
        else
            SDL_SetRenderDrawColor(g_state.renderer, 25, 27, 29, 225);
        SDL_RenderFillRect(g_state.renderer, &layout.buttons[i]);

        SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
            selected ? 235 : 190);
        SDL_RenderRect(g_state.renderer, &layout.buttons[i]);

        sdl_unified_look_draw_centered_text(font, label, text,
            &layout.buttons[i]);
    }
}

bool sdl_unified_look_prompt_contains_point(float x, float y)
{
    sdl_unified_look_prompt_layout_info layout;

    if (!g_unified_look_prompt.active)
        return false;
    if (!sdl_unified_look_prompt_layout(&layout))
        return false;

    return sdl_point_in_frect(&layout.panel, x, y);
}

static bool sdl_unified_look_prompt_choice_at(float x, float y,
    int* out_choice, bool* out_in_panel)
{
    sdl_unified_look_prompt_layout_info layout;

    if (out_choice)
        *out_choice = 0;
    if (out_in_panel)
        *out_in_panel = false;

    if (!sdl_unified_look_prompt_layout(&layout))
        return false;
    if (!sdl_point_in_frect(&layout.panel, x, y))
        return false;

    if (out_in_panel)
        *out_in_panel = true;

    for (int i = 0; i < layout.count; i++)
    {
        if (sdl_point_in_frect(&layout.buttons[i], x, y))
        {
            if (out_choice)
                *out_choice = g_unified_look_prompt.buttons[i].choice;
            return true;
        }
    }

    return false;
}

static void sdl_unified_look_prompt_clear_hover_if_needed(void)
{
    bool wake = false;

    if (ui_menu_click_clear_hover(&wake))
    {
        g_state.need_present = true;
        if (wake)
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    }
}

bool sdl_unified_look_prompt_handle_pointer(float x, float y, int action)
{
    int choice = 0;
    bool in_panel = false;
    bool wake = false;

    if (!g_unified_look_prompt.active)
        return false;
    if (!sdl_unified_look_prompt_choice_at(x, y, &choice, &in_panel))
    {
        if (in_panel)
        {
            sdl_unified_look_prompt_clear_hover_if_needed();
            sdl_unified_look_clear_map_hover();
            sdl_object_tooltip_clear();
            return true;
        }
        return false;
    }

    if (!ui_menu_click_handle_choice_action(choice, action, &wake))
        return true;

    sdl_unified_look_clear_map_hover();
    sdl_object_tooltip_clear();
    g_state.need_present = true;
    Term_keypress((action == UI_MENU_CLICK_SECONDARY)
        ? UI_MENU_CLICK_WAKE_KEY
        : '\r');
    (void)wake;
    return true;
}

bool sdl_unified_look_prompt_handle_hover_pointer(float x, float y)
{
    int choice = 0;
    bool in_panel = false;
    bool wake = false;

    if (!g_unified_look_prompt.active)
        return false;
    if (!sdl_unified_look_prompt_choice_at(x, y, &choice, &in_panel))
    {
        if (in_panel)
        {
            sdl_unified_look_prompt_clear_hover_if_needed();
            sdl_unified_look_clear_map_hover();
            sdl_object_tooltip_clear();
            return true;
        }
        return false;
    }

    if (!ui_menu_click_handle_choice_action(choice, UI_MENU_CLICK_HOVER, &wake))
        return true;

    sdl_unified_look_clear_map_hover();
    sdl_object_tooltip_clear();
    g_state.need_present = true;
    if (wake)
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

typedef struct sdl_unified_look_sidebar_layout_info {
    SDL_FRect area;
    SDL_FRect panel;
    SDL_FRect rows[SDL_UNIFIED_LOOK_SIDEBAR_MAX_ITEMS];
    int item_indices[SDL_UNIFIED_LOOK_SIDEBAR_MAX_ITEMS];
    int visible_count;
    int top_item;
    int font_px;
    float row_h;
    float pad_x;
    float pad_y;
    float symbol_w;
    float tile_size;
} sdl_unified_look_sidebar_layout_info;

static SDL_Color sdl_unified_look_sidebar_attr_color(byte attr)
{
    SDL_Color color = g_state.palette[attr];

    color.a = 255;
    return color;
}

static void sdl_unified_look_sidebar_draw_clipped_text(TTF_Font* font,
    cptr text, SDL_Color color, float x, float y, float max_w, float row_h)
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
    dst.y = y + (row_h - dst.h) * 0.5f;

    SDL_RenderTexture(g_state.renderer, texture, &src, &dst);
}

static float sdl_unified_look_sidebar_text_width(TTF_Font* font, cptr text,
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

static int sdl_unified_look_sidebar_selected_item_index(void)
{
    if (!g_unified_look_sidebar.has_selection)
        return -1;

    for (int i = 0; i < g_unified_look_sidebar.count; i++)
    {
        const sdl_unified_look_sidebar_item_state* item =
            &g_unified_look_sidebar.items[i];

        if (item->kind == SDL_UNIFIED_LOOK_SIDEBAR_ITEM_ENTRY
            && item->choice == g_unified_look_sidebar.selected_choice)
        {
            return i;
        }
    }

    return -1;
}

static bool sdl_unified_look_sidebar_layout(
    sdl_unified_look_sidebar_layout_info* out)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int visual_cols;
    int visual_rows;
    float cell_w;
    float cell_h;
    float margin;
    float max_panel_w;
    float max_panel_h;
    float measured_w = 0.0f;
    float panel_w;
    float panel_h;
    int visible_capacity;
    int selected_item;
    int top_item = 0;
    TTF_Font* font;

    if (!out)
        return false;
    *out = (sdl_unified_look_sidebar_layout_info){ 0 };

    if (!g_unified_look_sidebar.active || g_unified_look_sidebar.count <= 0)
        return false;
    if (!view->term_ready || !view->canvas)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0)
        return false;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    if (visual_cols <= 0 || visual_rows <= 0)
        return false;

    cell_w = (float)view->cell_w;
    cell_h = (float)view->cell_h;
    out->area = (SDL_FRect){
        .x = (float)(view->rect.x + view->margin_x),
        .y = (float)(view->rect.y + view->margin_y),
        .w = (float)visual_cols * cell_w,
        .h = (float)visual_rows * cell_h,
    };
    if (out->area.w <= 0.0f || out->area.h <= 0.0f)
        return false;

    margin = sdl_touch_pane_clampf(MIN(cell_w, cell_h) * 0.55f, 8.0f,
        18.0f);
    max_panel_w = out->area.w - margin * 2.0f;
    if (max_panel_w < 120.0f)
        return false;
    max_panel_h = out->area.h - margin * 2.0f;

    out->font_px = sdl_unified_look_log_text_font_px();
    font = sdl_story_font_for_height_slot(out->font_px,
        SDL_STORY_FONT_SLOT_LOG);
    if (!font)
        return false;

    out->row_h = (float)out->font_px * 1.28f;
    if (out->row_h < (float)out->font_px + 4.0f)
        out->row_h = (float)out->font_px + 4.0f;
    out->pad_x = sdl_touch_pane_clampf((float)out->font_px * 0.68f, 8.0f,
        14.0f);
    out->pad_y = sdl_touch_pane_clampf((float)out->font_px * 0.62f, 8.0f,
        14.0f);
    out->symbol_w = sdl_touch_pane_clampf((float)out->font_px * 1.35f,
        18.0f, 28.0f);

    /* A graphical entry uses the existing row height with a two-pixel inset
     * on every side.  Reserve the full row-height icon column plus a
     * font-relative gap so the following text never shares pixels with the
     * tile. */
    if (g_state.use_tiles && g_state.tileset)
    {
        float symbol_gap = sdl_touch_pane_clampf(
            (float)out->font_px * 0.25f, 4.0f, 8.0f);

        out->tile_size = MAX(1.0f, out->row_h - 4.0f);
        out->symbol_w = out->row_h + symbol_gap;
    }

    if (max_panel_h < out->row_h + out->pad_y * 2.0f)
        return false;

    visible_capacity = (int)((max_panel_h - out->pad_y * 2.0f)
        / out->row_h);
    if (visible_capacity < 1)
        return false;
    if (visible_capacity > SDL_UNIFIED_LOOK_SIDEBAR_MAX_ITEMS)
        visible_capacity = SDL_UNIFIED_LOOK_SIDEBAR_MAX_ITEMS;
    if (visible_capacity > g_unified_look_sidebar.count)
        visible_capacity = g_unified_look_sidebar.count;

    selected_item = sdl_unified_look_sidebar_selected_item_index();
    if (selected_item >= 0)
    {
        top_item = selected_item - visible_capacity / 2;
        if (top_item < 0)
            top_item = 0;
        if (top_item + visible_capacity > g_unified_look_sidebar.count)
            top_item = g_unified_look_sidebar.count - visible_capacity;
        if (top_item < 0)
            top_item = 0;
    }

    for (int i = 0; i < visible_capacity
         && top_item + i < g_unified_look_sidebar.count; i++)
    {
        const sdl_unified_look_sidebar_item_state* item =
            &g_unified_look_sidebar.items[top_item + i];
        float row_w;

        row_w = sdl_unified_look_sidebar_text_width(font, item->text,
            out->font_px);
        if (item->health_m_idx > 0 && item->health_len > 0)
        {
            float placeholder_w = sdl_unified_look_sidebar_text_width(font,
                "--------", out->font_px);
            float health_w = MAX(placeholder_w,
                (float)out->font_px * 3.2f);

            row_w += health_w - placeholder_w;
        }
        if (item->kind == SDL_UNIFIED_LOOK_SIDEBAR_ITEM_ENTRY)
        {
            float symbol_w = sdl_unified_look_sidebar_text_width(font,
                item->symbol, out->font_px);

            if (symbol_w > out->symbol_w)
                row_w += symbol_w;
            else
                row_w += out->symbol_w;
        }

        if (row_w > measured_w)
            measured_w = row_w;
    }

    panel_w = measured_w + out->pad_x * 2.0f;
    if (panel_w < out->pad_x * 2.0f + (float)out->font_px * 8.0f)
        panel_w = out->pad_x * 2.0f + (float)out->font_px * 8.0f;
    if (panel_w > max_panel_w)
        panel_w = max_panel_w;

    panel_h = out->pad_y * 2.0f + out->row_h * (float)visible_capacity;
    if (panel_h > max_panel_h)
        panel_h = max_panel_h;

    out->panel = (SDL_FRect){
        .x = out->area.x + margin,
        .y = out->area.y + margin,
        .w = panel_w,
        .h = panel_h,
    };

    out->top_item = top_item;
    for (int i = 0; i < visible_capacity
         && top_item + i < g_unified_look_sidebar.count; i++)
    {
        out->item_indices[out->visible_count] = top_item + i;
        out->rows[out->visible_count] = (SDL_FRect){
            .x = out->panel.x + out->pad_x,
            .y = out->panel.y + out->pad_y
                + (float)i * out->row_h,
            .w = out->panel.w - out->pad_x * 2.0f,
            .h = out->row_h,
        };
        out->visible_count++;
    }

    return out->visible_count > 0;
}

void sdl_unified_look_sidebar_clear(void)
{
    if (g_unified_look_sidebar.active || g_unified_look_sidebar.count > 0)
        g_state.need_present = true;

    memset(&g_unified_look_sidebar, 0, sizeof(g_unified_look_sidebar));
    g_unified_look_sidebar.selected_choice = -1;
}

void sdl_unified_look_sidebar_begin(bool compact, bool has_selection,
    int selected_choice)
{
    memset(&g_unified_look_sidebar, 0, sizeof(g_unified_look_sidebar));
    g_unified_look_sidebar.active = true;
    g_unified_look_sidebar.compact = compact;
    g_unified_look_sidebar.has_selection = has_selection;
    g_unified_look_sidebar.selected_choice = selected_choice;
    g_state.need_present = true;
}

void sdl_unified_look_sidebar_add_header(cptr text)
{
    sdl_unified_look_sidebar_item_state* item;

    if (!g_unified_look_sidebar.active)
        return;
    if (g_unified_look_sidebar.count >= SDL_UNIFIED_LOOK_SIDEBAR_MAX_ITEMS)
        return;
    if (!text || !text[0])
        return;

    item = &g_unified_look_sidebar.items[g_unified_look_sidebar.count++];
    memset(item, 0, sizeof(*item));
    item->kind = SDL_UNIFIED_LOOK_SIDEBAR_ITEM_HEADER;
    item->choice = -1;
    item->text_attr = TERM_WHITE;
    SDL_strlcpy(item->text, text, sizeof(item->text));
    g_state.need_present = true;
}

void sdl_unified_look_sidebar_add_entry(int choice, int entity_type, int y,
    int x, byte symbol_attr, byte text_attr, cptr symbol, cptr text)
{
    sdl_unified_look_sidebar_item_state* item;

    if (!g_unified_look_sidebar.active)
        return;
    if (g_unified_look_sidebar.count >= SDL_UNIFIED_LOOK_SIDEBAR_MAX_ITEMS)
        return;
    if (!text || !text[0])
        return;

    item = &g_unified_look_sidebar.items[g_unified_look_sidebar.count++];
    memset(item, 0, sizeof(*item));
    item->kind = SDL_UNIFIED_LOOK_SIDEBAR_ITEM_ENTRY;
    item->choice = choice;
    item->entity_type = entity_type;
    item->y = y;
    item->x = x;
    item->symbol_attr = symbol_attr;
    item->text_attr = text_attr;
    SDL_strlcpy(item->symbol, symbol ? symbol : "", sizeof(item->symbol));
    SDL_strlcpy(item->text, text, sizeof(item->text));
    if (styled_monster_health_bars && entity_type == 1
        && in_bounds(y, x) && cave_m_idx[y][x] > 0
        && monster_health_bar_allowed(&mon_list[cave_m_idx[y][x]]))
    {
        cptr marker = strstr(item->text, "--------");

        if (marker)
        {
            item->health_m_idx = cave_m_idx[y][x];
            item->health_offset = (byte)MIN(marker - item->text, 255);
            item->health_len = 8;
        }
    }
    g_state.need_present = true;
}

void sdl_unified_look_sidebar_finish(void)
{
    if (!g_unified_look_sidebar.active || g_unified_look_sidebar.count <= 0)
    {
        sdl_unified_look_sidebar_clear();
        return;
    }

    g_state.need_present = true;
}

void sdl_unified_look_sidebar_render(void)
{
    sdl_unified_look_sidebar_layout_info layout;
    SDL_FRect shadow;
    SDL_Color frame = g_state.palette[TERM_SLATE];
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    SDL_Color header = g_state.palette[TERM_WHITE];
    TTF_Font* font;
    int hover_choice = 0;
    bool has_hover_choice;

    if (!sdl_unified_look_sidebar_layout(&layout))
        return;

    font = sdl_story_font_for_height_slot(layout.font_px,
        SDL_STORY_FONT_SLOT_LOG);
    if (!font)
        return;

    has_hover_choice = ui_menu_click_get_hover_choice(&hover_choice);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    shadow = layout.panel;
    shadow.x += 3.0f;
    shadow.y += 3.0f;
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 118);
    SDL_RenderFillRect(g_state.renderer, &shadow);

    SDL_SetRenderDrawColor(g_state.renderer, 10, 12, 14, 218);
    SDL_RenderFillRect(g_state.renderer, &layout.panel);
    SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, 150);
    SDL_RenderRect(g_state.renderer, &layout.panel);

    header.a = 235;
    for (int i = 0; i < layout.visible_count; i++)
    {
        int item_index = layout.item_indices[i];
        const sdl_unified_look_sidebar_item_state* item =
            &g_unified_look_sidebar.items[item_index];
        SDL_FRect row = layout.rows[i];

        if (item->kind == SDL_UNIFIED_LOOK_SIDEBAR_ITEM_HEADER)
        {
            SDL_Color color = header;

            color.a = 230;
            sdl_unified_look_sidebar_draw_clipped_text(font, item->text,
                color, row.x, row.y, row.w, row.h);
            continue;
        }

        if (item->kind == SDL_UNIFIED_LOOK_SIDEBAR_ITEM_ENTRY)
        {
            bool selected = g_unified_look_sidebar.has_selection
                && item->choice == g_unified_look_sidebar.selected_choice;
            bool hovered = has_hover_choice && hover_choice == item->choice;
            SDL_Color symbol_color =
                sdl_unified_look_sidebar_attr_color(item->symbol_attr);
            SDL_Color text_color = selected || hovered
                ? accent
                : sdl_unified_look_sidebar_attr_color(item->text_attr);
            float symbol_x = row.x;
            float text_x = row.x + layout.symbol_w;
            float text_w = row.w - layout.symbol_w;

            if (selected || hovered)
            {
                SDL_SetRenderDrawColor(g_state.renderer, 36, 47, 62,
                    selected ? 226 : 184);
                SDL_RenderFillRect(g_state.renderer, &row);
                SDL_SetRenderDrawColor(g_state.renderer, accent.r, accent.g,
                    accent.b, selected ? 225 : 168);
                SDL_RenderRect(g_state.renderer, &row);
            }

            /* In tile mode the symbol carries the tile attr/char (high bit
             * set); render the actual tile rather than the TTF glyph, which
             * cannot draw it. */
            if (g_state.use_tiles && g_state.tileset
                && (item->symbol_attr & 0x80)
                && ((byte)item->symbol[0] & 0x80))
            {
                SDL_FRect tile_dst;

                tile_dst = (SDL_FRect){
                    .x = symbol_x + (row.h - layout.tile_size) * 0.5f,
                    .y = row.y + (row.h - layout.tile_size) * 0.5f,
                    .w = layout.tile_size,
                    .h = layout.tile_size,
                };

                SDL_SetTextureAlphaMod(g_state.tileset, 255);
                sdl_draw_tileset_sprite((byte)item->symbol_attr,
                    item->symbol[0], &tile_dst, false);
                sdl_restore_tileset_mod();
            }
            else
            {
                sdl_unified_look_sidebar_draw_clipped_text(font, item->symbol,
                    symbol_color, symbol_x, row.y, layout.symbol_w, row.h);
            }
            if (item->health_m_idx > 0
                && item->health_m_idx < mon_max
                && item->health_len > 0
                && mon_list[item->health_m_idx].r_idx)
            {
                const monster_type* m_ptr =
                    &mon_list[item->health_m_idx];
                char prefix[SDL_UNIFIED_LOOK_SIDEBAR_TEXT_LEN];
                char suffix[SDL_UNIFIED_LOOK_SIDEBAR_TEXT_LEN];
                char status[3];
                int prefix_len = MIN(item->health_offset,
                    (int)sizeof(prefix) - 1);
                int suffix_offset = item->health_offset + item->health_len;
                int status_len = 0;
                float prefix_w;
                float bar_w;
                float suffix_x;
                long scaled;
                SDL_FRect bar;

                SDL_memcpy(prefix, item->text, (size_t)prefix_len);
                prefix[prefix_len] = '\0';
                SDL_strlcpy(suffix,
                    (suffix_offset < (int)strlen(item->text))
                        ? item->text + suffix_offset : "",
                    sizeof(suffix));

                prefix_w = sdl_unified_look_sidebar_text_width(font, prefix,
                    layout.font_px);
                bar_w = sdl_unified_look_sidebar_text_width(font,
                    "--------", layout.font_px);
                if (bar_w < (float)layout.font_px * 3.2f)
                    bar_w = (float)layout.font_px * 3.2f;
                if (prefix_w + bar_w > text_w)
                    bar_w = MAX(8.0f, text_w - prefix_w);

                sdl_unified_look_sidebar_draw_clipped_text(font, prefix,
                    text_color, text_x, row.y, text_w, row.h);

                scaled = ((long)m_ptr->hp * 255L
                    + (long)m_ptr->maxhp - 1L) / (long)m_ptr->maxhp;
                if (scaled < 1L)
                    scaled = 1L;
                if (scaled > 255L)
                    scaled = 255L;
                bar = (SDL_FRect){
                    .x = text_x + prefix_w,
                    .y = row.y + row.h * 0.28f,
                    .w = bar_w,
                    .h = MAX(3.0f, row.h * 0.44f),
                };
                sdl_render_health_bar_rect(&bar, (byte)scaled,
                    health_attr(m_ptr->hp, m_ptr->maxhp));

                if (m_ptr->confused)
                    status[status_len++] = 'c';
                if (m_ptr->stunned)
                    status[status_len++] = 's';
                status[status_len] = '\0';
                if (status_len > 0)
                {
                    float status_w = sdl_unified_look_sidebar_text_width(
                        font, status, layout.font_px);

                    sdl_unified_look_sidebar_draw_clipped_text(font, status,
                        sdl_unified_look_sidebar_attr_color(TERM_WHITE),
                        bar.x + MAX(0.0f, (bar.w - status_w) * 0.5f), row.y,
                        bar.w, row.h);
                }

                suffix_x = bar.x + bar.w;
                if (suffix_x < text_x + text_w)
                {
                    sdl_unified_look_sidebar_draw_clipped_text(font, suffix,
                        text_color, suffix_x, row.y,
                        text_x + text_w - suffix_x, row.h);
                }
            }
            else
            {
                sdl_unified_look_sidebar_draw_clipped_text(font, item->text,
                    text_color, text_x, row.y, text_w, row.h);
            }
        }
    }
}

static bool sdl_unified_look_sidebar_choice_at(float x, float y,
    int* out_choice, bool* out_in_panel)
{
    sdl_unified_look_sidebar_layout_info layout;

    if (out_choice)
        *out_choice = 0;
    if (out_in_panel)
        *out_in_panel = false;

    if (!sdl_unified_look_sidebar_layout(&layout))
        return false;
    if (!sdl_point_in_frect(&layout.panel, x, y))
        return false;

    if (out_in_panel)
        *out_in_panel = true;

    for (int i = 0; i < layout.visible_count; i++)
    {
        int item_index = layout.item_indices[i];
        const sdl_unified_look_sidebar_item_state* item =
            &g_unified_look_sidebar.items[item_index];

        if (item->kind != SDL_UNIFIED_LOOK_SIDEBAR_ITEM_ENTRY)
            continue;
        if (sdl_point_in_frect(&layout.rows[i], x, y))
        {
            if (out_choice)
                *out_choice = item->choice;
            return true;
        }
    }

    return false;
}

bool sdl_unified_look_sidebar_handle_pointer(float x, float y, int action)
{
    int choice = 0;
    bool in_panel = false;
    bool wake = false;

    if (!g_unified_look_sidebar.active)
        return false;
    if (!sdl_unified_look_sidebar_choice_at(x, y, &choice, &in_panel))
    {
        if (in_panel)
        {
            sdl_unified_look_prompt_clear_hover_if_needed();
            sdl_unified_look_clear_map_hover();
            sdl_object_tooltip_clear();
            return true;
        }
        return false;
    }

    if (!ui_menu_click_handle_choice_action(choice, action, &wake))
        return true;

    sdl_unified_look_clear_map_hover();
    g_state.need_present = true;
    Term_keypress((action == UI_MENU_CLICK_SECONDARY)
        ? UI_MENU_CLICK_WAKE_KEY
        : '\r');
    (void)wake;
    return true;
}

bool sdl_unified_look_sidebar_handle_hover_pointer(float x, float y)
{
    int choice = 0;
    bool in_panel = false;
    bool wake = false;

    if (!g_unified_look_sidebar.active)
        return false;
    if (!sdl_unified_look_sidebar_choice_at(x, y, &choice, &in_panel))
    {
        if (in_panel)
        {
            sdl_unified_look_prompt_clear_hover_if_needed();
            sdl_unified_look_clear_map_hover();
            sdl_object_tooltip_clear();
            return true;
        }
        return false;
    }

    if (!ui_menu_click_handle_choice_action(choice, UI_MENU_CLICK_HOVER, &wake))
        return true;

    sdl_unified_look_clear_map_hover();
    g_state.need_present = true;
    if (wake)
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
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
                SDL_strlcpy(buf, "Weapon", buflen);
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
    int binding;
    int raw_binding;
    const char* custom_label;
    char context_label[SDL_TOUCH_PANE_LABEL_LEN];

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    sdl_touch_pane_base_label_for_slot(panel, index, buf, buflen);

    if (!sdl_touch_pane_panel_is_valid(panel)
        || index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
    {
        return;
    }

    raw_binding = sdl_touch_pane_raw_binding_for_panel(panel, index);
    custom_label = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? config.touch_pane_second_labels[index]
        : config.touch_pane_labels[index];
    if (custom_label[0])
        return;
    if (panel == SDL_TOUCH_PANE_PANEL_SECOND
        && raw_binding == TOUCH_PANE_BIND_INHERIT
        && config.touch_pane_labels[index][0])
    {
        return;
    }

    binding = sdl_touch_pane_effective_binding_for_panel(panel, index);
    if (binding == INPUT_BIND_CONFIRM)
        binding = ' ';
    if (touch_shortcut_context_action(binding,
            g_description_overlay.active && g_description_overlay.interactive,
            NULL, context_label, sizeof(context_label)))
    {
        SDL_strlcpy(buf, context_label, buflen);
    }
}
