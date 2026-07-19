#include "angband.h"
#include "sdl/main-sdl-private.h"

int sdl_main_view_visual_cols(const sdl_view* view)
{
    int cols;

    if (!view || view->cell_w <= 0 || view->rect.w <= 0)
        return 0;

    cols = sdl_main_view_visual_cols_for_width(view->rect.w, view->cell_w);
    if (view->cols > 0 && cols > view->cols)
        cols = view->cols;
    if (cols < 0)
        cols = 0;

    return cols;
}

int sdl_main_view_visual_rows(const sdl_view* view)
{
    int rows;

    if (!view || view->cell_h <= 0 || view->rect.h <= 0)
        return 0;

    rows = view->rect.h / view->cell_h;
    if (view->rows > 0 && rows > view->rows)
        rows = view->rows;
    if (rows < 0)
        rows = 0;

    return rows;
}

bool sdl_term_get_size_hook(term* t, int* w, int* h)
{
    sdl_view* view = &g_views[PANE_MAIN];
    int local_w;
    int local_h;

    if (!t || !view->term_ready || t != &view->t)
        return false;

    local_w = t->wid;
    local_h = t->hgt;

    /*
     * The SDL main term may keep hidden source columns for the detached
     * left-panel pane.  When that pane is not being presented, only the
     * rendered cell grid is visible, so menus must size themselves to that.
     * Saved-screen overlays may temporarily present the pane as backdrop,
     * but their menu layout should still use the visible grid.
     */
    if (!sdl_left_panel_pane_presentation_active()
        || sdl_saved_screen_left_panel_pane_active())
    {
        int visual_cols = sdl_main_view_visual_cols(view);
        int visual_rows = sdl_main_view_visual_rows(view);

        if (visual_cols > 0 && visual_cols < local_w)
            local_w = visual_cols;
        if (visual_rows > 0 && visual_rows < local_h)
            local_h = visual_rows;
    }

    if (w)
        *w = local_w;
    if (h)
        *h = local_h;

    return true;
}

bool sdl_left_panel_pane_rect_for_view(const sdl_view* view,
    SDL_FRect* out_rect)
{
    sdl_left_panel_metrics metrics;

    if (out_rect)
        *out_rect = (SDL_FRect){ 0 };
    if (!sdl_left_panel_pane_presentation_active())
        return false;
    if (!view || !view->term_ready || !view->canvas)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0)
        return false;

    if (!sdl_left_panel_metrics_for_view(view, &metrics))
        return false;

    return sdl_left_panel_pane_rect_for_metrics(view, &metrics, out_rect);
}

void sdl_update_left_panel_pane_rect(void)
{
    SDL_FRect rect;

    if (g_hide_left_panel) {
        g_pane_rects[PANE_LEFT_PANEL] = (SDL_Rect){ 0 };
        return;
    }

    if (!sdl_left_panel_pane_rect_for_view(&g_views[PANE_MAIN], &rect)) {
        g_pane_rects[PANE_LEFT_PANEL] = (SDL_Rect){ 0 };
        return;
    }

    g_pane_rects[PANE_LEFT_PANEL] = (SDL_Rect){
        .x = (int)rect.x,
        .y = (int)rect.y,
        .w = (int)rect.w,
        .h = (int)rect.h,
    };
}

void sdl_merge_frect_bounds(const SDL_FRect* rect, bool* have,
    float* x1, float* y1, float* x2, float* y2)
{
    if (!rect || !have || !x1 || !y1 || !x2 || !y2
        || rect->w <= 0.0f || rect->h <= 0.0f)
    {
        return;
    }

    if (!*have) {
        *x1 = rect->x;
        *y1 = rect->y;
        *x2 = rect->x + rect->w;
        *y2 = rect->y + rect->h;
        *have = true;
        return;
    }

    if (rect->x < *x1) *x1 = rect->x;
    if (rect->y < *y1) *y1 = rect->y;
    if (rect->x + rect->w > *x2) *x2 = rect->x + rect->w;
    if (rect->y + rect->h > *y2) *y2 = rect->y + rect->h;
}

bool sdl_main_cell_rect(int col, int row, int cols, int rows,
    SDL_FRect* out)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int visual_cols;
    int visual_rows;
    float grid_x;
    float grid_y;

    if (!out)
        return false;
    *out = (SDL_FRect){ 0 };
    if (!view->term_ready || !view->canvas)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0
        || view->cols <= 0 || view->rows <= 0)
    {
        return false;
    }
    if (col < 0 || row < 0 || cols <= 0 || rows <= 0)
        return false;
    if (row >= view->rows || col >= view->cols)
        return false;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    if (visual_cols <= 0 || visual_rows <= 0)
        return false;

    grid_x = (float)(view->rect.x + view->margin_x);
    grid_y = (float)(view->rect.y + view->margin_y);

    if (col >= visual_cols)
        return false;
    if (cols > visual_cols - col)
        cols = visual_cols - col;
    if (rows > visual_rows - row)
        rows = visual_rows - row;
    if (cols <= 0 || rows <= 0)
        return false;

    *out = (SDL_FRect){
        .x = grid_x + (float)(col * view->cell_w),
        .y = grid_y + (float)(row * view->cell_h),
        .w = (float)(cols * view->cell_w),
        .h = (float)(rows * view->cell_h),
    };
    return true;
}

bool sdl_left_panel_source_cell_rect(int col, int row, int cols,
    int rows, SDL_FRect* out)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    sdl_left_panel_metrics metrics;
    SDL_FRect panel_rect;
    float panel_content_x;

    if (!out)
        return false;
    *out = (SDL_FRect){ 0 };
    if (!sdl_left_panel_pane_presentation_active())
        return false;
    if (!view->term_ready || !view->canvas)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0
        || view->cols <= 0 || view->rows <= 0)
    {
        return false;
    }
    if (col < 0 || row < 0 || cols <= 0 || rows <= 0)
        return false;
    if (!sdl_left_panel_metrics_for_view(view, &metrics))
        return false;
    if (!sdl_left_panel_pane_rect_for_metrics(view, &metrics, &panel_rect))
        return false;

    panel_content_x = panel_rect.x
        + (float)sdl_left_panel_content_x_for_metrics(&metrics);
    panel_rect.y += (float)metrics.top_padding_h;

    if (metrics.collapsed) {
        bool have = false;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float x2 = 0.0f;
        float y2 = 0.0f;
        int end_col = col + cols;
        int end_row = row + rows;

        if (end_col < col)
            end_col = LEFT_PANEL_CONTENT_WID;
        if (end_row < row)
            end_row = row + 1;

        for (int i = 0; i < metrics.compact_segment_count; i++) {
            int source_row = metrics.compact_source_rows[i];
            int c0;
            int c1;

            if (source_row < row || source_row >= end_row)
                continue;

            c0 = MAX(col, 0);
            c1 = MIN(end_col, metrics.compact_widths[i]);
            if (c1 > c0) {
                SDL_FRect r = {
                    .x = panel_content_x
                        + (float)((metrics.compact_output_cols[i] + c0)
                            * metrics.cell_w),
                    .y = panel_rect.y
                        + (float)(metrics.compact_output_rows[i]
                            * metrics.cell_h),
                    .w = (float)((c1 - c0) * metrics.cell_w),
                    .h = (float)metrics.cell_h,
                };
                sdl_merge_frect_bounds(&r, &have, &x1, &y1, &x2, &y2);
            }
        }

        if (!have)
            return false;

        *out = (SDL_FRect){ .x = x1, .y = y1, .w = x2 - x1, .h = y2 - y1 };
        return out->w > 0.0f && out->h > 0.0f;
    }

    if (col >= metrics.content_cols || row >= metrics.panel_rows)
        return false;
    if (cols > metrics.content_cols - col)
        cols = metrics.content_cols - col;
    if (cols <= 0 || rows <= 0)
        return false;

    {
        bool have = false;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float x2 = 0.0f;
        float y2 = 0.0f;
        int end_row = row + rows;

        if (end_row < row || end_row > metrics.panel_rows)
            end_row = metrics.panel_rows;

        for (int source_row = row; source_row < end_row; source_row++) {
            int output_row = sdl_left_panel_output_row_for_source_row(
                source_row);
            SDL_FRect r;

            if (output_row < 0)
                continue;
            if (output_row >= metrics.panel_rows)
                continue;

            r = (SDL_FRect){
                .x = panel_content_x + (float)(col * metrics.cell_w),
                .y = panel_rect.y + (float)(output_row * metrics.cell_h),
                .w = (float)(cols * metrics.cell_w),
                .h = (float)metrics.cell_h,
            };
            if (r.y + r.h > panel_rect.y + (float)metrics.panel_render_h)
                r.h = panel_rect.y + (float)metrics.panel_render_h - r.y;
            sdl_merge_frect_bounds(&r, &have, &x1, &y1, &x2, &y2);
        }

        if (!have)
            return false;

        *out = (SDL_FRect){ .x = x1, .y = y1, .w = x2 - x1, .h = y2 - y1 };
    }

    return out->w > 0.0f && out->h > 0.0f;
}

bool sdl_combat_overlay_cell_rect(int col, int source_row, int cols,
    int rows, SDL_FRect* out)
{
    SDL_Rect pane;
    int cell_h;
    int cell_w;
    int panel_cols;
    int panel_rows;
    int row_count;
    int start_row;
    int source_index = -1;

    if (!out)
        return false;
    *out = (SDL_FRect){ 0 };
    if (!sdl_combat_overlay_pane_content_rect(&pane))
        return false;

    cell_h = sdl_effective_pane_cell_height_for_type(PANE_COMBAT);
    if (cell_h < 1)
        cell_h = 1;
    cell_w = cell_h / 2;
    if (cell_w < 1)
        cell_w = 1;

    panel_cols = pane.w / cell_w;
    panel_rows = pane.h / cell_h;
    if (panel_cols > PANE_COMBAT_OVERLAY_COLS)
        panel_cols = PANE_COMBAT_OVERLAY_COLS;
    if (panel_cols <= 0 || panel_rows <= 0)
        return false;

    row_count = sdl_combat_overlay_visible_row_count(panel_rows);
    start_row = (panel_rows > row_count) ? panel_rows - row_count : 0;
    for (int i = 0; i < row_count; i++) {
        int row_at_index = -1;

        if (!sdl_combat_overlay_visible_source_row_at_index(i, panel_rows,
                &row_at_index))
        {
            continue;
        }
        if (row_at_index == source_row) {
            source_index = i;
            break;
        }
    }
    if (source_index < 0 || start_row + source_index >= panel_rows)
        return false;

    if (col < 0 || source_row < 0 || cols <= 0 || rows <= 0)
        return false;
    if (col >= panel_cols)
        return false;
    if (cols > panel_cols - col)
        cols = panel_cols - col;
    if (cols <= 0)
        return false;

    *out = (SDL_FRect){
        .x = (float)pane.x + (float)(col * cell_w),
        .y = (float)pane.y + (float)((start_row + source_index) * cell_h),
        .w = (float)(cols * cell_w),
        .h = (float)cell_h,
    };
    if (out->x + out->w > (float)(pane.x + pane.w))
        out->w = (float)(pane.x + pane.w) - out->x;
    if (out->y + out->h > (float)(pane.y + pane.h))
        out->h = (float)(pane.y + pane.h) - out->y;
    (void)rows;

    return out->w > 0.0f && out->h > 0.0f;
}

bool sdl_combat_overlay_point_to_cell(float x, float y, int* out_col,
    int* out_source_row)
{
    SDL_Rect pane;
    int cell_h;
    int cell_w;
    int panel_cols;
    int panel_rows;
    int row_count;
    int start_row;
    int local_col;
    int local_row;
    int row_index;
    int source_row = -1;

    if (!out_col || !out_source_row)
        return false;
    *out_col = 0;
    *out_source_row = 0;
    if (!sdl_combat_overlay_pane_content_rect(&pane))
        return false;
    if (x < (float)pane.x || y < (float)pane.y
        || x >= (float)(pane.x + pane.w)
        || y >= (float)(pane.y + pane.h))
    {
        return false;
    }

    cell_h = sdl_effective_pane_cell_height_for_type(PANE_COMBAT);
    if (cell_h < 1)
        cell_h = 1;
    cell_w = cell_h / 2;
    if (cell_w < 1)
        cell_w = 1;

    panel_cols = pane.w / cell_w;
    panel_rows = pane.h / cell_h;
    if (panel_cols > PANE_COMBAT_OVERLAY_COLS)
        panel_cols = PANE_COMBAT_OVERLAY_COLS;
    if (panel_cols <= 0 || panel_rows <= 0)
        return false;

    local_col = (int)((x - (float)pane.x) / (float)cell_w);
    local_row = (int)((y - (float)pane.y) / (float)cell_h);
    if (local_col < 0 || local_col >= panel_cols
        || local_row < 0 || local_row >= panel_rows)
    {
        return false;
    }

    row_count = sdl_combat_overlay_visible_row_count(panel_rows);
    start_row = (panel_rows > row_count) ? panel_rows - row_count : 0;
    row_index = local_row - start_row;
    if (row_index < 0 || row_index >= row_count)
        return false;
    if (!sdl_combat_overlay_visible_source_row_at_index(row_index,
            panel_rows, &source_row))
    {
        return false;
    }

    *out_col = local_col;
    *out_source_row = source_row;
    return true;
}

bool sdl_main_view_point_to_cell(float x, float y, int* out_col, int* out_row)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    float grid_x;
    float grid_y;
    float grid_w;
    float grid_h;
    int visual_cols;
    int visual_rows;

    if (!out_col || !out_row)
        return false;
    g_last_main_cell_hit_left_panel = false;
    if (!view->term_ready || !view->canvas)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0 || view->cols <= 0 || view->rows <= 0)
        return false;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    if (visual_cols <= 0 || visual_rows <= 0)
        return false;

    grid_x = (float)(view->rect.x + view->margin_x);
    grid_y = (float)(view->rect.y + view->margin_y);
    grid_w = (float)(visual_cols * view->cell_w);
    grid_h = (float)(visual_rows * view->cell_h);

    if (x < grid_x || y < grid_y || x >= grid_x + grid_w || y >= grid_y + grid_h)
        return false;

    if (sdl_left_panel_pane_presentation_active()) {
        sdl_left_panel_metrics metrics;
        float local_x = x - grid_x;
        float local_y = y - grid_y;
        SDL_FRect panel_rect;
        float panel_local_x;
        float panel_local_y;
        float panel_content_x;
        float panel_content_right;
        int row;

        if (!sdl_left_panel_metrics_for_view(view, &metrics))
            return false;
        if (grid_w <= (float)metrics.total_w)
            return false;
        if (!sdl_left_panel_pane_rect_for_metrics(view, &metrics,
                &panel_rect))
        {
            return false;
        }

        if (x >= panel_rect.x && x < panel_rect.x + panel_rect.w
            && y >= panel_rect.y && y < panel_rect.y + panel_rect.h)
        {
            panel_local_x = x - panel_rect.x;
            panel_local_y = y - panel_rect.y
                - (float)metrics.top_padding_h;
            if (metrics.cell_w <= 0 || metrics.cell_h <= 0)
                return false;
            if (panel_local_y < 0.0f
                || panel_local_y >= (float)metrics.panel_render_h)
            {
                return false;
            }

            panel_content_x =
                (float)sdl_left_panel_content_x_for_metrics(&metrics);
            panel_content_right = panel_content_x
                + (float)metrics.content_w;
            if (panel_local_x < panel_content_x
                || panel_local_x >= panel_content_right)
            {
                *out_col = LEFT_PANEL_CONTENT_WID;
                *out_row = (int)(panel_local_y / (float)metrics.cell_h);
                if (!metrics.collapsed)
                    *out_row = sdl_left_panel_source_row_for_output_row(
                        *out_row);
                return (*out_row >= 0 && *out_row < metrics.panel_rows);
            }

            panel_local_x -= panel_content_x;
            if (panel_local_x < 0.0f
                || panel_local_x >= (float)metrics.content_w)
            {
                return false;
            }

            *out_col = (int)(panel_local_x / (float)metrics.cell_w);
            *out_row = (int)(panel_local_y / (float)metrics.cell_h);
            if (*out_col < 0 || *out_col >= metrics.content_cols
                || *out_row < 0 || *out_row >= metrics.panel_rows)
            {
                return false;
            }

            if (metrics.collapsed) {
                for (int i = 0; i < metrics.compact_segment_count; i++) {
                    int seg_col = metrics.compact_output_cols[i];
                    int seg_row = metrics.compact_output_rows[i];

                    if (*out_row != seg_row)
                        continue;
                    if (*out_col < seg_col
                        || *out_col >= seg_col + metrics.compact_widths[i])
                    {
                        continue;
                    }

                    *out_col -= seg_col;
                    *out_row = metrics.compact_source_rows[i];
                    g_last_main_cell_hit_left_panel = true;
                    return true;
                }

                return false;
            }

            *out_row = sdl_left_panel_source_row_for_output_row(*out_row);
            if (*out_row < 0 || *out_row >= metrics.panel_rows)
                return false;

            g_last_main_cell_hit_left_panel = true;
            return true;
        }

        row = (int)(local_y / (float)view->cell_h);
        *out_col = COL_MAP + (int)(local_x / (float)view->cell_w);
        *out_row = row;
        return (*out_col >= COL_MAP && *out_col < view->cols
            && *out_row >= 0 && *out_row < view->rows);
    }

    *out_col = (int)((x - grid_x) / (float)view->cell_w);
    *out_row = (int)((y - grid_y) / (float)view->cell_h);
    return (*out_col >= 0 && *out_col < visual_cols
        && *out_row >= 0 && *out_row < visual_rows);
}

bool sdl_mouse_gameplay_context_active(void)
{
    return character_generated
        && character_dungeon
        && p_ptr
        && p_ptr->playing
        && !p_ptr->leaving
        && !p_ptr->is_dead
        && character_icky == 0
        && !ui_menu_click_is_active()
        && g_views[PANE_MAIN].term_ready;
}

bool sdl_main_view_point_to_map(float x, float y, int* out_y, int* out_x)
{
    int col = 0;
    int row = 0;
    int map_row;
    int map_col;
    int map_y;
    int map_x;

    if (!out_y || !out_x)
        return false;
    if (!sdl_mouse_gameplay_context_active())
        return false;
    /* A point over any pane floating on top of the map (combat overlay, log
     * band, status/depth overlays, side map, touch pad, description popup) is
     * not a map cell: suppress hover popups and mouse/touch pathfinding there
     * so they act on the visible pane, not the cell hidden behind it. */
    if (sdl_main_screen_point_over_overlay_pane(x, y))
        return false;
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (sdl_main_screen_cell_hits_character_panel(col, row))
        return false;
    if (row < ROW_MAP || col < COL_MAP)
        return false;

    map_row = row - ROW_MAP;
    map_col = col - COL_MAP;
    if (use_bigtile)
        map_col /= 2;

    if (map_row < 0 || map_col < 0 || map_row >= SCREEN_HGT || map_col >= SCREEN_WID)
        return false;

    map_y = p_ptr->wy + map_row;
    map_x = p_ptr->wx + map_col;
    if (!in_bounds(map_y, map_x))
        return false;

    *out_y = map_y;
    *out_x = map_x;
    return true;
}

bool sdl_main_view_point_hits_outer_cell(float x, float y)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int col = 0;
    int row = 0;

    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;

    return row == 0 || col == 0
        || row == view->rows - 1 || col == view->cols - 1;
}

bool sdl_main_view_point_to_look_map(float x, float y, int* out_y,
    int* out_x)
{
    int col = 0;
    int row = 0;
    int map_row;
    int map_col;
    int map_y;
    int map_x;

    if (!out_y || !out_x)
        return false;
    if (!character_generated || !character_dungeon || !p_ptr
        || !g_views[PANE_MAIN].term_ready)
    {
        return false;
    }
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (sdl_main_screen_cell_hits_character_panel(col, row))
        return false;
    if (row < ROW_MAP || col < COL_MAP)
        return false;

    map_row = row - ROW_MAP;
    map_col = col - COL_MAP;
    if (use_bigtile)
        map_col /= 2;

    if (map_row < 0 || map_col < 0 || map_row >= SCREEN_HGT
        || map_col >= SCREEN_WID)
    {
        return false;
    }

    map_y = p_ptr->wy + map_row;
    map_x = p_ptr->wx + map_col;
    if (!in_bounds(map_y, map_x))
        return false;

    *out_y = map_y;
    *out_x = map_x;
    return true;
}

void sdl_unified_look_set_map_hover_enabled(bool enabled)
{
    g_unified_look_map_hover_enabled = enabled;
    sdl_unified_look_clear_map_hover();
}

void sdl_unified_look_set_active(bool active)
{
    g_unified_look_active = active;
    if (!active)
    {
        sdl_unified_look_prompt_clear();
        sdl_unified_look_sidebar_clear();
        sdl_object_tooltip_clear();
        sdl_unified_look_set_map_hover_enabled(false);
    }
    else
    {
        sdl_unified_look_clear_map_hover();
    }
}

bool sdl_unified_look_pointer_input_active(void)
{
    return g_unified_look_active && g_unified_look_map_hover_enabled;
}

void sdl_unified_look_cancel_map_drag(void)
{
    g_unified_look_map_drag.active = false;
    g_unified_look_map_drag.mouse = false;
    g_unified_look_map_drag.dragged = false;
    g_unified_look_map_drag.pinch_active = false;
    g_unified_look_map_drag.finger_id = 0;
    g_unified_look_map_drag.pinch_finger_id = 0;
    g_unified_look_map_drag.start_x = 0.0f;
    g_unified_look_map_drag.start_y = 0.0f;
    g_unified_look_map_drag.last_x = 0.0f;
    g_unified_look_map_drag.last_y = 0.0f;
    g_unified_look_map_drag.accum_x = 0.0f;
    g_unified_look_map_drag.accum_y = 0.0f;
    g_unified_look_map_drag.pinch_x = 0.0f;
    g_unified_look_map_drag.pinch_y = 0.0f;
    g_unified_look_map_drag.pinch_start_distance = 0.0f;
    g_unified_look_map_drag.pinch_start_scale = 0;
}

void sdl_unified_look_clear_map_hover(void)
{
    g_unified_look_map_hover_pending = false;
    g_unified_look_map_hover_wake_pending = false;
    g_unified_look_map_hover_y = -1;
    g_unified_look_map_hover_x = -1;
    g_unified_look_map_describe_pending = false;
    g_unified_look_map_describe_y = -1;
    g_unified_look_map_describe_x = -1;
    g_unified_look_map_target_pending = false;
    g_unified_look_map_target_y = -1;
    g_unified_look_map_target_x = -1;
    g_unified_look_map_pan_pending = false;
    g_unified_look_map_pan_wake_pending = false;
    g_unified_look_map_pan_dy = 0;
    g_unified_look_map_pan_dx = 0;
    g_unified_look_main_zoom_pending = false;
    g_unified_look_main_zoom_wake_pending = false;
    g_unified_look_main_zoom_scale = 0;
    sdl_unified_look_cancel_map_drag();
}

bool sdl_unified_look_take_map_hover(int* y, int* x)
{
    if (!g_unified_look_map_hover_pending)
        return false;

    if (y)
        *y = g_unified_look_map_hover_y;
    if (x)
        *x = g_unified_look_map_hover_x;

    g_unified_look_map_hover_pending = false;
    g_unified_look_map_hover_wake_pending = false;
    return true;
}

bool sdl_unified_look_take_map_describe(int* y, int* x)
{
    if (!g_unified_look_map_describe_pending)
        return false;

    if (y)
        *y = g_unified_look_map_describe_y;
    if (x)
        *x = g_unified_look_map_describe_x;

    g_unified_look_map_describe_pending = false;
    return true;
}

bool sdl_unified_look_take_map_target(int* y, int* x)
{
    if (!g_unified_look_map_target_pending)
        return false;

    if (y)
        *y = g_unified_look_map_target_y;
    if (x)
        *x = g_unified_look_map_target_x;

    g_unified_look_map_target_pending = false;
    return true;
}

bool sdl_unified_look_take_map_pan(int* dy, int* dx)
{
    if (!g_unified_look_map_pan_pending)
        return false;

    if (dy)
        *dy = g_unified_look_map_pan_dy;
    if (dx)
        *dx = g_unified_look_map_pan_dx;

    g_unified_look_map_pan_pending = false;
    g_unified_look_map_pan_wake_pending = false;
    g_unified_look_map_pan_dy = 0;
    g_unified_look_map_pan_dx = 0;
    return true;
}

bool sdl_unified_look_take_main_zoom(int* scale)
{
    if (!g_unified_look_main_zoom_pending)
        return false;

    if (scale)
        *scale = g_unified_look_main_zoom_scale;

    g_unified_look_main_zoom_pending = false;
    g_unified_look_main_zoom_wake_pending = false;
    g_unified_look_main_zoom_scale = 0;
    return true;
}

bool sdl_unified_look_point_to_drag_map(float x, float y)
{
    int col = 0;
    int row = 0;
    int map_y = 0;
    int map_x = 0;

    if (!sdl_unified_look_pointer_input_active())
        return false;
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (ui_menu_click_has_cell(col, row))
        return false;

    return sdl_main_view_point_to_look_map(x, y, &map_y, &map_x);
}

void sdl_unified_look_queue_map_pan(int dy, int dx)
{
    if (dy == 0 && dx == 0)
        return;

    g_unified_look_map_pan_dy += dy;
    g_unified_look_map_pan_dx += dx;
    g_unified_look_map_pan_pending = true;

    if (!g_unified_look_map_pan_wake_pending)
    {
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        g_unified_look_map_pan_wake_pending = true;
    }
}

int sdl_unified_look_effective_zoom_scale(void)
{
    return g_unified_look_main_zoom_pending
        ? g_unified_look_main_zoom_scale
        : get_sdl_effective_main_view_scale();
}

int sdl_unified_look_clamp_zoom_scale(int scale)
{
    int min_scale = get_sdl_min_main_view_zoom_scale();
    int max_scale = get_sdl_max_main_view_zoom_scale();

    if (min_scale < SDL_MAIN_VIEW_MIN_SCALE)
        min_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale < SDL_MAIN_VIEW_MIN_SCALE)
        max_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale < min_scale)
        max_scale = min_scale;
    if (scale < min_scale)
        scale = min_scale;
    if (scale > max_scale)
        scale = max_scale;

    return scale;
}

void sdl_unified_look_queue_main_zoom_target(int scale)
{
    scale = sdl_unified_look_clamp_zoom_scale(scale);
    if (scale == sdl_unified_look_effective_zoom_scale())
        return;

    g_unified_look_main_zoom_scale = scale;
    g_unified_look_main_zoom_pending = true;

    if (!g_unified_look_main_zoom_wake_pending)
    {
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        g_unified_look_main_zoom_wake_pending = true;
    }
}

void sdl_unified_look_queue_main_zoom_delta(int delta)
{
    if (delta == 0)
        return;

    sdl_unified_look_queue_main_zoom_target(
        sdl_unified_look_effective_zoom_scale() + delta);
}

float sdl_unified_look_pinch_distance(void)
{
    float dx = g_unified_look_map_drag.last_x
        - g_unified_look_map_drag.pinch_x;
    float dy = g_unified_look_map_drag.last_y
        - g_unified_look_map_drag.pinch_y;

    return SDL_sqrtf(dx * dx + dy * dy);
}

int sdl_unified_look_zoom_delta_for_pinch_ratio(float ratio)
{
    int delta = 0;
    float threshold = 1.20f;

    if (ratio <= 0.0f)
        return 0;

    while (ratio >= threshold && delta < 20)
    {
        delta++;
        threshold *= 1.25f;
    }

    threshold = 1.20f;
    while (ratio <= 1.0f / threshold && delta > -20)
    {
        delta--;
        threshold *= 1.25f;
    }

    return delta;
}

void sdl_unified_look_update_map_pinch(void)
{
    float distance;
    float ratio;
    int delta;

    if (!g_unified_look_map_drag.pinch_active)
        return;
    if (g_unified_look_map_drag.pinch_start_distance < 8.0f)
        return;

    distance = sdl_unified_look_pinch_distance();
    ratio = distance / g_unified_look_map_drag.pinch_start_distance;
    delta = sdl_unified_look_zoom_delta_for_pinch_ratio(ratio);
    sdl_unified_look_queue_main_zoom_target(
        g_unified_look_map_drag.pinch_start_scale + delta);
}

bool sdl_unified_look_begin_map_pinch(float x, float y,
    SDL_FingerID finger_id)
{
    if (!g_unified_look_map_drag.active
        || g_unified_look_map_drag.mouse
        || g_unified_look_map_drag.finger_id == finger_id)
    {
        return false;
    }
    if (g_unified_look_map_drag.pinch_active)
        return true;

    g_unified_look_map_drag.pinch_active = true;
    g_unified_look_map_drag.dragged = true;
    g_unified_look_map_drag.pinch_finger_id = finger_id;
    g_unified_look_map_drag.pinch_x = x;
    g_unified_look_map_drag.pinch_y = y;
    g_unified_look_map_drag.pinch_start_distance =
        sdl_unified_look_pinch_distance();
    g_unified_look_map_drag.pinch_start_scale =
        sdl_unified_look_effective_zoom_scale();
    g_unified_look_map_drag.accum_x = 0.0f;
    g_unified_look_map_drag.accum_y = 0.0f;
    return true;
}

/* Smooth devices such as macOS touchpads report fractional wheel deltas.
 * Accumulate and rate-limit them into deliberate menu/zoom steps instead of
 * treating each wheel event as one click. */
void sdl_wheel_step_state_reset(sdl_wheel_step_state* state)
{
    if (!state)
        return;

    state->accum_x = 0.0f;
    state->accum_y = 0.0f;
    state->last_step_x_timestamp = 0;
    state->last_step_y_timestamp = 0;
    state->smooth_x = false;
    state->smooth_y = false;
}

void sdl_wheel_step_state_prepare(sdl_wheel_step_state* state,
    const SDL_MouseWheelEvent* wheel)
{
    if (!state || !wheel)
        return;

    if (state->last_timestamp != 0
        && (state->last_mouse != wheel->which
            || wheel->timestamp < state->last_timestamp
            || wheel->timestamp - state->last_timestamp > SDL_WHEEL_IDLE_RESET_NS))
    {
        sdl_wheel_step_state_reset(state);
    }

    state->last_timestamp = wheel->timestamp;
    state->last_mouse = wheel->which;
}

bool sdl_wheel_axis_value_looks_smooth(float value)
{
    float abs_value;
    float whole;

    if (!(value > 0.0f || value < 0.0f))
        return false;

    abs_value = SDL_fabsf(value);
    if (abs_value < SDL_WHEEL_DISCRETE_STEP_UNITS)
        return true;

    whole = SDL_floorf(abs_value);
    return SDL_fabsf(abs_value - whole) > SDL_WHEEL_ACCUM_EPSILON;
}

void sdl_wheel_clamp_accum(float* accum, float step_units)
{
    if (!accum)
        return;

    if (*accum >= step_units)
        *accum = step_units - SDL_WHEEL_ACCUM_EPSILON;
    else if (*accum <= -step_units)
        *accum = -step_units + SDL_WHEEL_ACCUM_EPSILON;
}

int sdl_wheel_consume_axis_value(sdl_wheel_step_state* state,
    const SDL_MouseWheelEvent* wheel, float* accum, float value,
    bool* smooth_axis, Uint64* last_step_timestamp)
{
    int steps = 0;
    bool smooth;
    float step_units;
    int max_steps;

    if (!state || !wheel || !accum || !smooth_axis || !last_step_timestamp)
        return 0;
    if (!(value > 0.0f || value < 0.0f))
        return 0;

    sdl_wheel_step_state_prepare(state, wheel);
    if (sdl_wheel_axis_value_looks_smooth(value))
        *smooth_axis = true;

    smooth = *smooth_axis;
    step_units = smooth
        ? SDL_WHEEL_SMOOTH_STEP_UNITS
        : SDL_WHEEL_DISCRETE_STEP_UNITS;
    max_steps = smooth
        ? SDL_WHEEL_SMOOTH_MAX_STEPS_PER_EVENT
        : SDL_WHEEL_DISCRETE_MAX_STEPS_PER_EVENT;

    if ((*accum > 0.0f && value < 0.0f)
        || (*accum < 0.0f && value > 0.0f))
    {
        *accum = 0.0f;
        *last_step_timestamp = 0;
    }

    *accum += value;

    if (smooth && *last_step_timestamp != 0
        && wheel->timestamp >= *last_step_timestamp
        && wheel->timestamp - *last_step_timestamp < SDL_WHEEL_SMOOTH_MIN_STEP_NS)
    {
        sdl_wheel_clamp_accum(accum, step_units);
        return 0;
    }

    while (*accum >= step_units && steps < max_steps)
    {
        steps++;
        *accum -= step_units;
    }

    while (*accum <= -step_units && steps > -max_steps)
    {
        steps--;
        *accum += step_units;
    }

    if (steps != 0 && smooth)
        *last_step_timestamp = wheel->timestamp;

    sdl_wheel_clamp_accum(accum, step_units);

    return steps;
}

int sdl_wheel_step_state_consume_axis(sdl_wheel_step_state* state,
    const SDL_MouseWheelEvent* wheel, bool vertical)
{
    if (!state)
        return 0;

    if (vertical)
        return sdl_wheel_consume_axis_value(state, wheel, &state->accum_y,
            wheel ? wheel->y : 0.0f, &state->smooth_y,
            &state->last_step_y_timestamp);

    return sdl_wheel_consume_axis_value(state, wheel, &state->accum_x,
        wheel ? wheel->x : 0.0f, &state->smooth_x,
        &state->last_step_x_timestamp);
}

int sdl_wheel_step_state_consume_primary_axis(
    sdl_wheel_step_state* state, const SDL_MouseWheelEvent* wheel)
{
    float abs_x;
    float abs_y;

    if (!state || !wheel)
        return 0;

    abs_x = SDL_fabsf(wheel->x);
    abs_y = SDL_fabsf(wheel->y);

    if (abs_y == 0.0f && abs_x == 0.0f)
        return 0;

    return sdl_wheel_step_state_consume_axis(state, wheel,
        abs_y >= abs_x);
}

bool sdl_unified_look_handle_map_zoom_wheel(
    const SDL_MouseWheelEvent* wheel)
{
    static sdl_wheel_step_state wheel_state;
    int steps;

    if (!wheel)
        return false;
    if (!sdl_unified_look_pointer_input_active())
        return false;
    if (wheel->which == SDL_TOUCH_MOUSEID)
        return true;
    if (!sdl_unified_look_point_to_drag_map(wheel->mouse_x, wheel->mouse_y))
        return false;

    steps = sdl_wheel_step_state_consume_primary_axis(&wheel_state, wheel);
    if (steps != 0)
        sdl_unified_look_queue_main_zoom_delta(steps);

    return true;
}

int sdl_main_screen_clamp_main_view_scale(int scale)
{
    int min_scale = get_sdl_min_main_view_zoom_scale();
    int max_scale = get_sdl_max_main_view_zoom_scale();

    if (min_scale < SDL_MAIN_VIEW_MIN_SCALE)
        min_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale < SDL_MAIN_VIEW_MIN_SCALE)
        max_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale < min_scale)
        max_scale = min_scale;
    if (scale < min_scale)
        scale = min_scale;
    if (scale > max_scale)
        scale = max_scale;

    return scale;
}

bool sdl_main_screen_set_main_view_scale_target(int scale)
{
    Uint64 start_ns = SDL_GetTicksNS();
    int old_scale = get_sdl_effective_main_view_scale();

    scale = sdl_main_screen_clamp_main_view_scale(scale);
    if (scale == old_scale)
        return true;

    set_sdl_main_view_zoom_scale(scale);
    if (get_sdl_effective_main_view_scale() == old_scale)
        return true;

    sdl_apply_runtime_zoom();
    if (character_dungeon && p_ptr) {
        (void)modify_panel(p_ptr->py - SCREEN_HGT / 2,
            p_ptr->px - SCREEN_WID / 2);
        Term_keypress(KTRL('R'));
    }

    log_debug("main scale target change %d->%d completed in %llu ms",
        old_scale, get_sdl_effective_main_view_scale(),
        (unsigned long long)((SDL_GetTicksNS() - start_ns) / 1000000ULL));

    return true;
}

bool sdl_main_screen_adjust_main_view_scale(int delta)
{
    if (delta == 0)
        return true;

    return sdl_main_screen_set_main_view_scale_target(
        get_sdl_effective_main_view_scale() + delta);
}

bool sdl_main_map_point_to_drag_map(float x, float y)
{
    int col = 0;
    int row = 0;
    int map_y = 0;
    int map_x = 0;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (ui_menu_click_has_cell(col, row))
        return false;

    return sdl_main_view_point_to_map(x, y, &map_y, &map_x);
}

bool sdl_main_map_apply_pan(int pan_dy, int pan_dx)
{
    int new_wy;
    int new_wx;

    if (!p_ptr || (pan_dy == 0 && pan_dx == 0))
        return false;

    new_wy = p_ptr->wy + pan_dy;
    new_wx = p_ptr->wx + pan_dx;
    if (!modify_panel(new_wy, new_wx))
        return true;

    sdl_mouse_path_cancel();
    sdl_object_tooltip_clear();
    handle_stuff();
    if (Term)
        Term_fresh();
    g_state.need_present = true;
    return true;
}

/*
 * SDL can deliver several finger-motion samples in one poll batch.  Applying
 * each sample immediately redraws the full map and, with V-sync enabled,
 * potentially presents it before the next sample is handled.  Accumulate the
 * logical cell movement and apply the final panel position once the batch has
 * been drained.
 */
static int g_main_map_pending_pan_dy;
static int g_main_map_pending_pan_dx;

static void sdl_main_map_queue_pan(int pan_dy, int pan_dx)
{
    g_main_map_pending_pan_dy += pan_dy;
    g_main_map_pending_pan_dx += pan_dx;
}

void sdl_main_map_flush_pending_pan(void)
{
    int pan_dy = g_main_map_pending_pan_dy;
    int pan_dx = g_main_map_pending_pan_dx;

    g_main_map_pending_pan_dy = 0;
    g_main_map_pending_pan_dx = 0;

    if (pan_dy || pan_dx)
        (void)sdl_main_map_apply_pan(pan_dy, pan_dx);
}

void sdl_main_map_cancel_drag(void)
{
    g_main_map_drag.active = false;
    g_main_map_drag.mouse = false;
    g_main_map_drag.dragged = false;
    g_main_map_drag.pinch_active = false;
    g_main_map_drag.finger_id = 0;
    g_main_map_drag.pinch_finger_id = 0;
    g_main_map_drag.start_x = 0.0f;
    g_main_map_drag.start_y = 0.0f;
    g_main_map_drag.last_x = 0.0f;
    g_main_map_drag.last_y = 0.0f;
    g_main_map_drag.accum_x = 0.0f;
    g_main_map_drag.accum_y = 0.0f;
    g_main_map_drag.pinch_x = 0.0f;
    g_main_map_drag.pinch_y = 0.0f;
    g_main_map_drag.pinch_start_distance = 0.0f;
    g_main_map_drag.pinch_start_scale = 0;
}

void sdl_main_map_mark_dragged(void)
{
    if (g_main_map_drag.dragged)
        return;

    g_main_map_drag.dragged = true;
    sdl_map_touch_cancel_press();
    sdl_mouse_path_cancel();
    sdl_object_tooltip_clear();
}

float sdl_main_map_pinch_distance(void)
{
    float dx = g_main_map_drag.last_x - g_main_map_drag.pinch_x;
    float dy = g_main_map_drag.last_y - g_main_map_drag.pinch_y;

    return SDL_sqrtf(dx * dx + dy * dy);
}

int sdl_main_map_zoom_delta_for_pinch_ratio(float ratio)
{
    int delta = 0;
    float threshold = 1.20f;

    if (ratio <= 0.0f)
        return 0;

    while (ratio >= threshold && delta < 20)
    {
        delta++;
        threshold *= 1.25f;
    }

    threshold = 1.20f;
    while (ratio <= 1.0f / threshold && delta > -20)
    {
        delta--;
        threshold *= 1.25f;
    }

    return delta;
}

void sdl_main_map_update_pinch(void)
{
    float distance;
    float ratio;
    int delta;

    if (!g_main_map_drag.pinch_active)
        return;
    if (g_main_map_drag.pinch_start_distance < 8.0f)
        return;

    distance = sdl_main_map_pinch_distance();
    ratio = distance / g_main_map_drag.pinch_start_distance;
    delta = sdl_main_map_zoom_delta_for_pinch_ratio(ratio);
    (void)sdl_main_screen_set_main_view_scale_target(
        g_main_map_drag.pinch_start_scale + delta);
}

bool sdl_main_map_begin_pinch(float x, float y,
    SDL_FingerID finger_id)
{
    if (!g_main_map_drag.active
        || g_main_map_drag.mouse
        || g_main_map_drag.finger_id == finger_id)
    {
        return false;
    }
    if (g_main_map_drag.pinch_active)
        return true;

    g_main_map_drag.pinch_active = true;
    g_main_map_drag.pinch_finger_id = finger_id;
    g_main_map_drag.pinch_x = x;
    g_main_map_drag.pinch_y = y;
    g_main_map_drag.pinch_start_distance = sdl_main_map_pinch_distance();
    g_main_map_drag.pinch_start_scale = get_sdl_effective_main_view_scale();
    g_main_map_drag.accum_x = 0.0f;
    g_main_map_drag.accum_y = 0.0f;
    sdl_main_map_mark_dragged();
    return true;
}

bool sdl_main_map_handle_drag_down(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;
    SDL_Rect log_band;

    if (g_player_action_menu.active || g_player_exchange_target.active)
        return false;
    if (!mouse && sdl_touch_movement_point_blocked_by_overlay(x, y))
        return false;
    /* The overlay log sits on top of the map; a press on its visible band must
     * open the log rather than start a map drag/move.  Unlike the touch-only
     * overlay block above, this also guards mouse clicks, which reach the map
     * drag handler before the supporting-pane handler. */
    if (sdl_overlay_log_pane_current_rect(&log_band)
        && sdl_point_in_rect(&log_band, x, y))
    {
        return false;
    }
    if (!sdl_main_map_point_to_drag_map(x, y))
        return false;
    if (!mouse && sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        && map_y == p_ptr->py && map_x == p_ptr->px)
    {
        return false;
    }

    if (!mouse && g_touch_swipe.active
        && g_touch_swipe.finger_id == finger_id)
    {
        return false;
    }

    if (!mouse && sdl_main_map_begin_pinch(x, y, finger_id))
        return true;

    sdl_main_map_cancel_drag();
    g_main_map_drag.active = true;
    g_main_map_drag.mouse = mouse;
    g_main_map_drag.dragged = false;
    g_main_map_drag.finger_id = finger_id;
    g_main_map_drag.start_x = x;
    g_main_map_drag.start_y = y;
    g_main_map_drag.last_x = x;
    g_main_map_drag.last_y = y;
    g_main_map_drag.accum_x = 0.0f;
    g_main_map_drag.accum_y = 0.0f;

    if (!mouse)
        (void)sdl_map_touch_handle_pointer_down(x, y, finger_id);

    return true;
}

bool sdl_main_map_handle_drag_motion(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    float cell_w;
    float cell_h;
    float dx;
    float dy;
    float total_dx;
    float total_dy;
    int pan_dy = 0;
    int pan_dx = 0;

    if (!g_main_map_drag.active || g_main_map_drag.mouse != mouse)
        return false;
    if (!mouse && g_main_map_drag.finger_id != finger_id)
    {
        if (g_main_map_drag.pinch_active
            && g_main_map_drag.pinch_finger_id == finger_id)
        {
            g_main_map_drag.pinch_x = x;
            g_main_map_drag.pinch_y = y;
            sdl_main_map_update_pinch();
            return true;
        }
        return false;
    }

    if (!mouse && g_main_map_drag.pinch_active)
    {
        g_main_map_drag.last_x = x;
        g_main_map_drag.last_y = y;
        sdl_main_map_update_pinch();
        return true;
    }

    dx = x - g_main_map_drag.last_x;
    dy = y - g_main_map_drag.last_y;
    g_main_map_drag.last_x = x;
    g_main_map_drag.last_y = y;
    g_main_map_drag.accum_x += dx;
    g_main_map_drag.accum_y += dy;

    total_dx = x - g_main_map_drag.start_x;
    total_dy = y - g_main_map_drag.start_y;
    if (total_dx < 0.0f)
        total_dx = -total_dx;
    if (total_dy < 0.0f)
        total_dy = -total_dy;
    if (total_dx > sdl_touch_swipe_threshold_px()
        || total_dy > sdl_touch_swipe_threshold_px())
    {
        sdl_main_map_mark_dragged();
    }

    cell_w = (view->term_ready && view->cell_w > 0)
        ? (float)view->cell_w : 1.0f;
    cell_h = (view->term_ready && view->cell_h > 0)
        ? (float)view->cell_h : 1.0f;
    if (use_bigtile)
        cell_w *= 2.0f;

    while (g_main_map_drag.accum_y >= cell_h)
    {
        pan_dy--;
        g_main_map_drag.accum_y -= cell_h;
    }
    while (g_main_map_drag.accum_y <= -cell_h)
    {
        pan_dy++;
        g_main_map_drag.accum_y += cell_h;
    }
    while (g_main_map_drag.accum_x >= cell_w)
    {
        pan_dx--;
        g_main_map_drag.accum_x -= cell_w;
    }
    while (g_main_map_drag.accum_x <= -cell_w)
    {
        pan_dx++;
        g_main_map_drag.accum_x += cell_w;
    }

    if (pan_dy || pan_dx)
    {
        sdl_main_map_mark_dragged();
        sdl_main_map_queue_pan(pan_dy, pan_dx);
    }

    return true;
}

bool sdl_main_map_handle_mouse_release_click(float x, float y)
{
    int press_y = 0;
    int press_x = 0;
    int release_y = 0;
    int release_x = 0;

    if (!sdl_main_view_point_to_map(g_main_map_drag.start_x,
            g_main_map_drag.start_y, &press_y, &press_x))
    {
        return true;
    }
    if (!sdl_main_view_point_to_map(x, y, &release_y, &release_x))
        return true;
    if (release_y != press_y || release_x != press_x)
        return true;

    if (sdl_pointer_attack_handle_left_click(x, y))
        return true;
    if (sdl_mouse_path_handle_left_click(x, y))
        return true;

    return true;
}

bool sdl_main_map_handle_drag_up(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    bool dragged;

    if (!g_main_map_drag.active || g_main_map_drag.mouse != mouse)
        return false;
    if (!mouse && g_main_map_drag.finger_id != finger_id)
    {
        if (g_main_map_drag.pinch_active
            && g_main_map_drag.pinch_finger_id == finger_id)
        {
            sdl_main_map_cancel_drag();
            return true;
        }
        return false;
    }

    if (!mouse && g_main_map_drag.pinch_active)
    {
        sdl_main_map_cancel_drag();
        return true;
    }

    dragged = g_main_map_drag.dragged;
    if (mouse)
    {
        if (!dragged)
        {
            bool handled = sdl_main_map_handle_mouse_release_click(x, y);

            sdl_main_map_cancel_drag();
            return handled;
        }

        sdl_main_map_cancel_drag();
        return true;
    }

    sdl_main_map_cancel_drag();
    if (dragged)
        return true;
    if (!g_map_touch_press.active)
        return true;

    return false;
}

bool sdl_main_map_handle_zoom_wheel(const SDL_MouseWheelEvent* wheel)
{
    static sdl_wheel_step_state wheel_state;
    int steps;

    if (!wheel)
        return false;
    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (wheel->which == SDL_TOUCH_MOUSEID)
        return true;
    if (!sdl_main_map_point_to_drag_map(wheel->mouse_x, wheel->mouse_y))
        return false;

    steps = sdl_wheel_step_state_consume_primary_axis(&wheel_state, wheel);
    if (steps != 0)
        (void)sdl_main_screen_adjust_main_view_scale(steps);

    return true;
}

bool sdl_unified_look_handle_map_drag_down(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    if (!sdl_unified_look_point_to_drag_map(x, y))
        return false;

    if (!mouse && sdl_unified_look_begin_map_pinch(x, y, finger_id))
        return true;

    sdl_unified_look_cancel_map_drag();
    g_unified_look_map_drag.active = true;
    g_unified_look_map_drag.mouse = mouse;
    g_unified_look_map_drag.dragged = false;
    g_unified_look_map_drag.finger_id = finger_id;
    g_unified_look_map_drag.start_x = x;
    g_unified_look_map_drag.start_y = y;
    g_unified_look_map_drag.last_x = x;
    g_unified_look_map_drag.last_y = y;
    g_unified_look_map_drag.accum_x = 0.0f;
    g_unified_look_map_drag.accum_y = 0.0f;
    return true;
}

bool sdl_unified_look_handle_map_drag_motion(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    float cell_w;
    float cell_h;
    float dx;
    float dy;
    float total_dx;
    float total_dy;
    int pan_dy = 0;
    int pan_dx = 0;

    if (!g_unified_look_map_drag.active
        || g_unified_look_map_drag.mouse != mouse)
    {
        return false;
    }
    if (!mouse && g_unified_look_map_drag.finger_id != finger_id)
    {
        if (g_unified_look_map_drag.pinch_active
            && g_unified_look_map_drag.pinch_finger_id == finger_id)
        {
            g_unified_look_map_drag.pinch_x = x;
            g_unified_look_map_drag.pinch_y = y;
            sdl_unified_look_update_map_pinch();
            return true;
        }
        return false;
    }

    if (!mouse && g_unified_look_map_drag.pinch_active)
    {
        g_unified_look_map_drag.last_x = x;
        g_unified_look_map_drag.last_y = y;
        sdl_unified_look_update_map_pinch();
        return true;
    }

    dx = x - g_unified_look_map_drag.last_x;
    dy = y - g_unified_look_map_drag.last_y;
    g_unified_look_map_drag.last_x = x;
    g_unified_look_map_drag.last_y = y;
    g_unified_look_map_drag.accum_x += dx;
    g_unified_look_map_drag.accum_y += dy;

    total_dx = x - g_unified_look_map_drag.start_x;
    total_dy = y - g_unified_look_map_drag.start_y;
    if (total_dx < 0.0f)
        total_dx = -total_dx;
    if (total_dy < 0.0f)
        total_dy = -total_dy;
    if (total_dx > sdl_touch_swipe_threshold_px()
        || total_dy > sdl_touch_swipe_threshold_px())
    {
        g_unified_look_map_drag.dragged = true;
    }

    cell_w = (view->term_ready && view->cell_w > 0)
        ? (float)view->cell_w : 1.0f;
    cell_h = (view->term_ready && view->cell_h > 0)
        ? (float)view->cell_h : 1.0f;
    if (use_bigtile)
        cell_w *= 2.0f;

    while (g_unified_look_map_drag.accum_y >= cell_h)
    {
        pan_dy--;
        g_unified_look_map_drag.accum_y -= cell_h;
    }
    while (g_unified_look_map_drag.accum_y <= -cell_h)
    {
        pan_dy++;
        g_unified_look_map_drag.accum_y += cell_h;
    }
    while (g_unified_look_map_drag.accum_x >= cell_w)
    {
        pan_dx--;
        g_unified_look_map_drag.accum_x -= cell_w;
    }
    while (g_unified_look_map_drag.accum_x <= -cell_w)
    {
        pan_dx++;
        g_unified_look_map_drag.accum_x += cell_w;
    }

    if (pan_dy || pan_dx)
    {
        g_unified_look_map_drag.dragged = true;
        sdl_unified_look_queue_map_pan(pan_dy, pan_dx);
    }

    return true;
}

bool sdl_unified_look_handle_map_drag_up(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    bool dragged;

    if (!g_unified_look_map_drag.active
        || g_unified_look_map_drag.mouse != mouse)
    {
        return false;
    }
    if (!mouse && g_unified_look_map_drag.finger_id != finger_id)
    {
        if (g_unified_look_map_drag.pinch_active
            && g_unified_look_map_drag.pinch_finger_id == finger_id)
        {
            sdl_unified_look_cancel_map_drag();
            return true;
        }
        return false;
    }

    if (!mouse && g_unified_look_map_drag.pinch_active)
    {
        sdl_unified_look_cancel_map_drag();
        return true;
    }

    dragged = g_unified_look_map_drag.dragged;
    sdl_unified_look_cancel_map_drag();
    if (dragged)
        return true;

    if (mouse)
    {
        if (sdl_unified_look_handle_map_target_pointer(x, y)
            || sdl_unified_look_handle_map_hover_pointer(x, y))
        {
            return true;
        }
        return true;
    }

    (void)sdl_unified_look_handle_map_hover_pointer(x, y);
    return true;
}

bool sdl_unified_look_handle_map_hover_pointer(float x, float y)
{
    int col = 0;
    int row = 0;
    int map_y = 0;
    int map_x = 0;

    if (!sdl_unified_look_pointer_input_active())
        return false;
    /* The Quick Access panel overlays the bottom of the map; its buttons have
     * their own hover tooltips, so don't also raise a map look-hover there. */
    if (sdl_touch_top_panel_pointer_claims_point(x, y))
    {
        sdl_unified_look_clear_map_hover();
        sdl_object_tooltip_clear();
        return false;
    }
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
    {
        sdl_object_tooltip_clear();
        return false;
    }
    if (ui_menu_click_has_cell(col, row))
    {
        sdl_object_tooltip_clear();
        return false;
    }
    if (!sdl_main_view_point_to_look_map(x, y, &map_y, &map_x))
    {
        sdl_object_tooltip_clear();
        return false;
    }

    if (!ui_menu_click_clear_pending_hover()
        && !g_unified_look_map_hover_pending
        && g_unified_look_map_hover_y == map_y
        && g_unified_look_map_hover_x == map_x)
    {
        return true;
    }

    g_unified_look_map_hover_pending = true;
    g_unified_look_map_hover_y = map_y;
    g_unified_look_map_hover_x = map_x;

    if (!g_unified_look_map_hover_wake_pending)
    {
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        g_unified_look_map_hover_wake_pending = true;
    }

    return true;
}
