#include "angband.h"
#include "sdl/main-sdl-private.h"

typedef struct {
    bool valid;
    Uint64 content_hash;
    int content_w;
    int content_h;
} left_panel_render_cache;

static left_panel_render_cache g_left_panel_render_cache;

void sdl_touch_pane_render(void)
{
    SDL_FRect slot_rects[SDL_TOUCH_PANE_BUTTON_COUNT];
    SDL_FRect pane_rect;
    SDL_Color frame = g_state.palette[TERM_WHITE];
    SDL_Color muted = g_state.palette[TERM_SLATE];
    SDL_Color selected = g_state.palette[TERM_YELLOW];
    SDL_Rect pane;
    int panel = sdl_touch_pane_active_panel();

    if (!sdl_touch_pane_current_rect(&pane))
        return;

    if (!sdl_touch_pane_compute_layout(&pane, slot_rects))
        return;

    pane_rect = (SDL_FRect){
        .x = (float)pane.x,
        .y = (float)pane.y,
        .w = (float)pane.w,
        .h = (float)pane.h,
    };

    SDL_SetRenderDrawColor(g_state.renderer, 12, 12, 12, 255);
    SDL_RenderFillRect(g_state.renderer, &pane_rect);
    if (config.show_pane_borders)
        SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, 180);
    else
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderRect(g_state.renderer, &pane_rect);

    for (int visual_index = 0; visual_index < SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT; visual_index++) {
        SDL_Color text_color;
        SDL_Color border_color;
        SDL_FRect shadow;
        char label[64];
        char symbol[32];
        int i = sdl_touch_pane_visible_slot_at(visual_index);
        int binding;
        bool toggled;

        if (i < 0)
            continue;
        if (!sdl_touch_pane_slot_visible_in_current_mode(i))
            continue;

        binding = sdl_touch_pane_effective_binding_for_panel(panel, i);
        toggled = ((binding == GAMEPAD_BIND_SHIFT && g_touch_pane_second_panel)
            || (binding == GAMEPAD_BIND_CTRL && sdl_gamepad_ctrl_active())
            || sdl_pointer_attack_binding_toggled(binding));

        shadow = slot_rects[i];
        shadow.x += 2.0f;
        shadow.y += 2.0f;

        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(g_state.renderer, &shadow);

        if (binding == GAMEPAD_BIND_NONE) {
            SDL_SetRenderDrawColor(g_state.renderer, 26, 26, 26, 255);
            text_color = muted;
            border_color = muted;
        } else if (toggled) {
            SDL_SetRenderDrawColor(g_state.renderer, 34, 34, 34, 255);
            text_color = selected;
            border_color = selected;
        } else {
            SDL_SetRenderDrawColor(g_state.renderer, 34, 34, 34, 255);
            text_color = frame;
            border_color = frame;
        }

        SDL_RenderFillRect(g_state.renderer, &slot_rects[i]);
        SDL_SetRenderDrawColor(g_state.renderer, border_color.r, border_color.g, border_color.b, 220);
        SDL_RenderRect(g_state.renderer, &slot_rects[i]);

        if (sdl_touch_pane_binding_is_direction(binding)) {
            sdl_touch_pane_draw_arrow(&slot_rects[i], binding, text_color);
            continue;
        }

        sdl_touch_pane_display_label_for_slot(panel, i, label, sizeof(label));
        if (i == SDL_TOUCH_PANE_CENTER_SLOT
            && sdl_touch_pane_confirm_binding(binding))
        {
            if (!label[0])
                SDL_strlcpy(label, "Confirm", sizeof(label));
            if (streq(label, "Confirm"))
                SDL_strlcpy(symbol, "(pick)", sizeof(symbol));
            else
                symbol[0] = '\0';
        } else {
            sdl_touch_pane_binding_symbol(binding, symbol, sizeof(symbol));
            if (!config.touch_pane_key_labels_visible
                || sdl_touch_pane_should_hide_symbol(label, symbol))
            {
                symbol[0] = '\0';
            }
        }
        sdl_touch_pane_draw_button_text(&slot_rects[i], label, symbol, text_color);
    }

    if (g_touch_pane_flash_slot >= 0) {
        g_touch_pane_flash_slot = -1;
        g_touch_pane_flash_until = 0;
    }
}

void sdl_restore_render_target(sdl_view* d)
{
    if (d && d->canvas)
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
    else
        SDL_SetRenderTarget(g_state.renderer, NULL);
}

bool sdl_left_panel_ensure_canvas(int width, int height)
{
    int canvas_w;
    int canvas_h;

    if (width <= 0 || height <= 0)
        return false;

    /*
     * The expanded panel's measured height changes as transient status rows
     * appear and disappear. Reuse a canvas that is already large enough
     * instead of destroying and recreating a GPU texture every turn.
     */
    if (g_left_panel_canvas && g_left_panel_canvas_w >= width
        && g_left_panel_canvas_h >= height)
    {
        return true;
    }

    canvas_w = MAX(width, g_left_panel_canvas_w);
    canvas_h = MAX(height, g_left_panel_canvas_h);
    sdl_left_panel_canvas_destroy();
    g_left_panel_render_cache.valid = false;
    g_left_panel_canvas = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, canvas_w, canvas_h);
    if (!g_left_panel_canvas) {
        log_warn("Could not create left panel texture %dx%d: %s", canvas_w,
            canvas_h, SDL_GetError());
        return false;
    }

    SDL_SetTextureBlendMode(g_left_panel_canvas, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(g_left_panel_canvas, SDL_SCALEMODE_NEAREST);
    g_left_panel_canvas_w = canvas_w;
    g_left_panel_canvas_h = canvas_h;
    return true;
}

bool sdl_left_panel_cell_is_tile(byte a, char c)
{
    return g_state.use_tiles && g_state.tileset
        && (a & TILE_FLAG) && (((byte)c) & TILE_FLAG);
}

void sdl_left_panel_debug_make_row_text(const term_win* scr, int row,
    int source_col, int end_col, char* out, size_t out_size,
    int* out_visible_width)
{
    int len = 0;
    int visible_col = -1;

    if (out && out_size > 0)
        out[0] = '\0';
    if (out_visible_width)
        *out_visible_width = 0;
    if (!scr || !scr->a || !scr->c || row < 0 || !scr->a[row]
        || !scr->c[row] || !out || out_size == 0)
    {
        return;
    }

    for (int col = source_col; col < end_col
         && len < (int)out_size - 1; col++)
    {
        byte a = scr->a[row][col];
        char c = scr->c[row][col];
        unsigned char uc = (unsigned char)c;

        if (sdl_left_panel_cell_has_visible_content(a, c))
            visible_col = col;

        if ((a == 255) && (uc == 0xFF))
            out[len++] = '~';
        else if (uc == 0)
            out[len++] = ' ';
        else if (uc < 32 || uc > 126)
            out[len++] = '?';
        else
            out[len++] = (char)uc;
    }
    out[len] = '\0';

    if (out_visible_width && visible_col >= source_col)
        *out_visible_width = visible_col - source_col + 1;
}

void sdl_left_panel_debug_log_source_row(const term* t,
    const term_win* scr, int source_row, int source_col, int width,
    int end_col, int dest_col, int dest_row, float content_x, int cell_w,
    int cell_h)
{
    char text[LEFT_PANEL_CONTENT_WID + 1];
    int visible_width = 0;

    if (!g_left_panel_debug_dump_rows || !t || !scr)
        return;

    sdl_left_panel_debug_make_row_text(scr, source_row, source_col, end_col,
        text, sizeof(text), &visible_width);
    log_trace("left-panel row: src_row=%d dst_row=%d src_col=%d "
        "requested_w=%d end_col=%d visible_w=%d dest_col=%d cell=%dx%d "
        "content_x=%.1f text='%s'",
        source_row, dest_row, source_col, width, end_col, visible_width,
        dest_col, cell_w, cell_h, (double)content_x, text);
}

SDL_Color sdl_left_panel_background_color(void)
{
    SDL_Color color = { 0, 0, 0, 255 };

    return color;
}

void sdl_render_mono_text_scaled(SDL_Texture* atlas, int atlas_cell_w,
    int atlas_cell_h, float cell_w, float cell_h, float origin_x,
    float origin_y, int x, int y, int n, const char* s, SDL_Color col)
{
    if (!atlas || atlas_cell_w <= 0 || atlas_cell_h <= 0
        || cell_w <= 0.0f || cell_h <= 0.0f || n <= 0 || !s)
    {
        return;
    }

    SDL_SetTextureColorMod(atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(atlas, 255);

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        SDL_FRect src = {
            (float)((ch & 15) * atlas_cell_w),
            (float)((ch >> 4) * atlas_cell_h),
            (float)atlas_cell_w,
            (float)atlas_cell_h,
        };
        SDL_FRect dst = {
            origin_x + ((float)x + (float)i) * cell_w,
            origin_y + (float)y * cell_h,
            cell_w,
            cell_h,
        };

        if (use_graphics == GRAPHICS_PSEUDO && solid_walls
            && (ch == '#' || ch == '%'))
        {
            SDL_SetRenderDrawColor(g_state.renderer, col.r, col.g, col.b,
                SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(g_state.renderer, &dst);
        }
        SDL_RenderTexture(g_state.renderer, atlas, &src, &dst);
    }
}

byte sdl_left_panel_pane_render_attr_for_cell(int source_col,
    int source_row, byte attr)
{
    int action;

    if (!sdl_left_panel_pane_presentation_active())
        return attr;
    if (source_col < 0 || source_col >= LEFT_PANEL_CONTENT_WID
        || source_row < 0)
    {
        return attr;
    }

    action = sdl_visible_character_panel_click_action_at_cell(source_col,
        source_row);
    /* Pass row -1 so the whole block highlights together: any cell whose
     * block action matches the selected action lights up, not just one row. */
    if (action != SDL_PANEL_CLICK_NONE
        && sdl_character_panel_touch_zone_selected(action, -1))
    {
        return TERM_YELLOW;
    }

    return attr;
}

static Uint64 sdl_left_panel_hash_mix(Uint64 hash, Uint64 value)
{
    hash ^= value;
    hash *= 1099511628211ULL;
    return hash;
}

static Uint64 sdl_left_panel_source_hash(const term* source_term,
    const sdl_left_panel_metrics* metrics, int source_rows)
{
    term_win* scr;
    Uint64 hash = 1469598103934665603ULL;
    int cols;
    int rows;

    if (!source_term || !metrics || !source_term->scr)
        return 0;

    scr = source_term->scr;
    cols = MIN(LEFT_PANEL_CONTENT_WID, source_term->wid);
    rows = MIN(source_rows, source_term->hgt);

    hash = sdl_left_panel_hash_mix(hash, (Uint64)metrics->collapsed);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)metrics->compact_row);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)metrics->cell_w);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)metrics->cell_h);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)metrics->content_cols);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)metrics->panel_rows);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)metrics->top_padding_h);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)metrics->bottom_padding_h);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)metrics->compact_segment_count);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)use_bigtile);
    hash = sdl_left_panel_hash_mix(hash, (Uint64)g_state.use_tiles);
    hash = sdl_left_panel_hash_mix(hash,
        sdl_mono_font_atlas_generation_for_cells(sdl_monospace_font_path(),
            metrics->cell_w, metrics->cell_h));
    hash = sdl_left_panel_hash_mix(hash,
        (Uint64)(op_ptr && styled_player_health_bar));
    hash = sdl_left_panel_hash_mix(hash,
        (Uint64)(op_ptr && styled_monster_health_bars));
    if (p_ptr && op_ptr && styled_player_health_bar) {
        hash = sdl_left_panel_hash_mix(hash, (Uint64)p_ptr->chp);
        hash = sdl_left_panel_hash_mix(hash, (Uint64)p_ptr->mhp);
    }

    for (int i = 0; i < metrics->compact_segment_count; i++) {
        hash = sdl_left_panel_hash_mix(hash,
            (Uint64)metrics->compact_source_rows[i]);
        hash = sdl_left_panel_hash_mix(hash,
            (Uint64)metrics->compact_widths[i]);
        hash = sdl_left_panel_hash_mix(hash,
            (Uint64)metrics->compact_output_cols[i]);
        hash = sdl_left_panel_hash_mix(hash,
            (Uint64)metrics->compact_output_rows[i]);
    }

    for (int row = 0; row < rows; row++) {
        if (!scr->a || !scr->c || !scr->a[row] || !scr->c[row])
            continue;

        for (int col = 0; col < cols; col++) {
            byte attr = sdl_left_panel_pane_render_attr_for_cell(col, row,
                scr->a[row][col]);
            byte ch = (byte)scr->c[row][col];
            byte terrain_attr = (scr->ta && scr->ta[row])
                ? scr->ta[row][col]
                : 0;
            byte terrain_ch = (scr->tc && scr->tc[row])
                ? (byte)scr->tc[row][col]
                : 0;
            byte health = (scr->health && scr->health[row])
                ? scr->health[row][col]
                : 0;

            hash = sdl_left_panel_hash_mix(hash, attr);
            hash = sdl_left_panel_hash_mix(hash, ch);
            hash = sdl_left_panel_hash_mix(hash, terrain_attr);
            hash = sdl_left_panel_hash_mix(hash, terrain_ch);
            hash = sdl_left_panel_hash_mix(hash, health);
        }
    }

    return hash;
}

/*
 * Render a continuous, pixel-accurate health bar on the row beneath the
 * player name in the styled left panel instead of the terminal health row.
 */
static bool sdl_render_left_panel_player_health_bar(int source_row,
    int source_col, int end_col, int dest_col, int dest_row, float content_x,
    float content_y, int cell_w, int cell_h)
{
    const int bar_col = COL_NAME;
    const int bar_cells = 12;
    byte fill_attr;
    SDL_Color fill_color;
    SDL_Color track_color;
    SDL_Color frame_color;
    SDL_FRect bar;
    SDL_FRect fill;
    float inset_x;
    float inset_y;
    float fraction;

    if (!p_ptr || !op_ptr || !styled_player_health_bar || p_ptr->mhp <= 0)
        return false;
    if (source_row != ROW_NAME + 1)
        return false;
    if (source_col > bar_col || end_col < bar_col + bar_cells)
        return false;

    fill_attr = health_attr(p_ptr->chp, p_ptr->mhp);
    fill_attr = sdl_left_panel_pane_render_attr_for_cell(bar_col, source_row,
        fill_attr);
    fill_color = sdl_color_from_attr(sdl_ui_text_fg_attr(fill_attr));
    track_color = sdl_color_from_attr(TERM_L_DARK);
    frame_color = sdl_color_from_attr(TERM_SLATE);

    inset_x = (float)cell_w * 0.12f;
    if (inset_x < 1.0f)
        inset_x = 1.0f;
    inset_y = (float)cell_h * 0.24f;
    if (inset_y < 1.0f)
        inset_y = 1.0f;

    bar.x = content_x
        + (float)(dest_col + bar_col - source_col) * (float)cell_w
        + inset_x;
    bar.y = content_y + (float)dest_row * (float)cell_h + inset_y;
    bar.w = (float)(bar_cells * cell_w) - 2.0f * inset_x;
    bar.h = (float)cell_h - 2.0f * inset_y;
    if (bar.w < 2.0f || bar.h < 2.0f)
        return false;

    SDL_SetRenderDrawColor(g_state.renderer, track_color.r, track_color.g,
        track_color.b, 255);
    SDL_RenderFillRect(g_state.renderer, &bar);

    fraction = (float)p_ptr->chp / (float)p_ptr->mhp;
    if (fraction < 0.0f)
        fraction = 0.0f;
    if (fraction > 1.0f)
        fraction = 1.0f;

    fill = bar;
    fill.x += 1.0f;
    fill.y += 1.0f;
    fill.w = (bar.w - 2.0f) * fraction;
    fill.h -= 2.0f;
    if (fill.w > 0.0f && fill.h > 0.0f) {
        if (fill.w < 1.0f)
            fill.w = 1.0f;
        SDL_SetRenderDrawColor(g_state.renderer, fill_color.r, fill_color.g,
            fill_color.b, 255);
        SDL_RenderFillRect(g_state.renderer, &fill);
    }

    SDL_SetRenderDrawColor(g_state.renderer, frame_color.r, frame_color.g,
        frame_color.b, 255);
    SDL_RenderRect(g_state.renderer, &bar);
    return true;
}

static int sdl_render_left_panel_monster_health_bar(term_win* scr,
    int source_row, int col, int end_col, int out_col, int dest_row,
    float content_x, float content_y, int cell_w, int cell_h,
    SDL_Texture* font_atlas, int atlas_cell_w, int atlas_cell_h)
{
    byte level;
    byte fill_attr = TERM_L_GREEN;
    bool confused = false;
    bool stunned = false;
    int bar_end;
    SDL_FRect bar;

    if (!styled_monster_health_bars || !scr || !scr->health
        || !scr->health[source_row])
    {
        return col;
    }

    level = scr->health[source_row][col];
    if (level == 0)
        return col;

    bar_end = col + 1;
    while (bar_end < end_col
        && scr->health[source_row][bar_end] == level)
    {
        bar_end++;
    }

    for (int c = col; c < bar_end; c++)
    {
        char ch = scr->c[source_row][c];

        if (ch != '-' && ch != ' ')
        {
            fill_attr = sdl_left_panel_pane_render_attr_for_cell(c,
                source_row, scr->a[source_row][c]);
        }
        if (ch == 'c')
            confused = true;
        else if (ch == 's')
            stunned = true;
    }

    bar = (SDL_FRect){
        .x = content_x + (float)out_col * (float)cell_w
            + MAX(1.0f, (float)cell_w * 0.12f),
        .y = content_y + (float)dest_row * (float)cell_h
            + MAX(1.0f, (float)cell_h * 0.24f),
        .w = (float)((bar_end - col) * cell_w)
            - 2.0f * MAX(1.0f, (float)cell_w * 0.12f),
        .h = (float)cell_h
            - 2.0f * MAX(1.0f, (float)cell_h * 0.24f),
    };
    sdl_render_health_bar_rect(&bar, level, fill_attr);

    if (confused || stunned)
    {
        char status[3];
        int len = 0;
        int status_col;
        SDL_Color text_col = sdl_color_from_attr(TERM_WHITE);

        if (confused)
            status[len++] = 'c';
        if (stunned)
            status[len++] = 's';
        status[len] = '\0';
        status_col = out_col + MAX(0, ((bar_end - col) - len) / 2);
        sdl_render_mono_text_scaled(font_atlas, atlas_cell_w, atlas_cell_h,
            (float)cell_w, (float)cell_h, content_x, content_y, status_col,
            dest_row, len, status, text_col);
    }

    return bar_end;
}

void sdl_render_left_panel_source_row_cells(const sdl_view* view,
    const term* source_term, term_win* scr, int source_row, int source_col,
    int width, int dest_col, int dest_row, float content_x, float content_y,
    int cell_w, int cell_h, SDL_Texture* font_atlas, int atlas_cell_w, int atlas_cell_h,
    TTF_Font* mono_font)
{
    const term* t;
    int col;
    int end_col;

    if (!view || !scr || !font_atlas || cell_w <= 0 || cell_h <= 0)
        return;
    if (source_row < 0 || source_col < 0 || width <= 0)
        return;

    t = source_term ? source_term : &view->t;
    if (source_row >= t->hgt)
        return;
    if (!scr->a || !scr->c || !scr->a[source_row] || !scr->c[source_row])
        return;

    end_col = source_col + width;
    if (end_col > t->wid)
        end_col = t->wid;
    if (end_col > LEFT_PANEL_CONTENT_WID)
        end_col = LEFT_PANEL_CONTENT_WID;
    if (end_col <= source_col)
        return;

    sdl_left_panel_debug_log_source_row(t, scr, source_row, source_col,
        width, end_col, dest_col, dest_row, content_x, cell_w, cell_h);

    if (sdl_render_left_panel_player_health_bar(source_row, source_col,
            end_col, dest_col, dest_row, content_x, content_y, cell_w,
            cell_h))
    {
        return;
    }

    col = source_col;
    while (col < end_col) {
        int out_col = dest_col + (col - source_col);
        byte a = scr->a[source_row][col];
        char c = scr->c[source_row][col];
        byte ta = (scr->ta && scr->ta[source_row])
            ? scr->ta[source_row][col]
            : 0;
        char tc = (scr->tc && scr->tc[source_row])
            ? scr->tc[source_row][col]
            : 0;

        if (scr->health && scr->health[source_row]
            && scr->health[source_row][col] != 0
            && styled_monster_health_bars)
        {
            col = sdl_render_left_panel_monster_health_bar(scr, source_row,
                col, end_col, out_col, dest_row, content_x, content_y,
                cell_w, cell_h, font_atlas, atlas_cell_w, atlas_cell_h);
            continue;
        }

        if ((a == 255) && ((byte)c == 0xFF)) {
            col++;
            continue;
        }

        if (sdl_left_panel_cell_is_tile(a, c)) {
            int tile_cols = use_bigtile ? 2 : 1;
            if (col + tile_cols > end_col)
                tile_cols = end_col - col;
            if (tile_cols < 1)
                tile_cols = 1;

            SDL_FRect dst = {
                content_x + (float)(out_col * cell_w),
                content_y + (float)(dest_row * cell_h),
                (float)(tile_cols * cell_w),
                (float)cell_h,
            };

            sdl_draw_map_tile_layers_at(-1, -1, a, c, ta, tc, &dst);
            col += tile_cols;
        } else {
            char buf[LEFT_PANEL_CONTENT_WID + 1];
            int start = col;
            int start_out = out_col;
            int len = 0;
            SDL_Color text_col;

            a = sdl_left_panel_pane_render_attr_for_cell(col, source_row, a);
            text_col = sdl_color_from_attr(sdl_ui_text_fg_attr(a));

            while (col < end_col && len < LEFT_PANEL_CONTENT_WID) {
                byte run_a = scr->a[source_row][col];
                char run_c = scr->c[source_row][col];

                if ((run_a == 255) && ((byte)run_c == 0xFF))
                    break;
                if (sdl_left_panel_cell_is_tile(run_a, run_c))
                    break;
                run_a = sdl_left_panel_pane_render_attr_for_cell(col,
                    source_row, run_a);
                if (run_a != a)
                    break;

                buf[len++] = run_c ? run_c : ' ';
                col++;
            }
            buf[len] = '\0';
            if (len <= 0) {
                if (col == start)
                    col++;
                continue;
            }

            if (a >= TERM_UI_SELECTED) {
                SDL_Color bg = sdl_color_from_attr(sdl_ui_text_bg_attr(a));
                SDL_FRect bg_rect = {
                    content_x + (float)(start_out * cell_w),
                    content_y + (float)(dest_row * cell_h),
                    (float)(len * cell_w),
                    (float)cell_h,
                };

                SDL_SetRenderDrawColor(g_state.renderer, bg.r, bg.g, bg.b,
                    255);
                SDL_RenderFillRect(g_state.renderer, &bg_rect);
            }

            if (mono_font && utf8_has_non_ascii_n(buf, len)) {
                sdl_render_mono_utf8_text_cells_at(font_atlas, atlas_cell_w,
                    atlas_cell_h, mono_font, (float)cell_w, (float)cell_h,
                    content_x, content_y, start_out, dest_row, len, buf,
                    text_col);
            } else {
                sdl_render_mono_text_scaled(font_atlas, atlas_cell_w,
                    atlas_cell_h, (float)cell_w, (float)cell_h, content_x,
                    content_y, start_out, dest_row, len, buf, text_col);
            }
        }
    }
}

void sdl_left_panel_debug_log_frame(const sdl_view* view,
    const sdl_left_panel_metrics* metrics, const SDL_FRect* dst_left,
    int visual_cols, int visual_rows, int source_h, int visual_w,
    int canvas_w)
{
    static bool have_last = false;
    static int last_view_cell_w;
    static int last_view_cell_h;
    static int last_cols;
    static int last_rows;
    static int last_visual_cols;
    static int last_visual_rows;
    static int last_source_h;
    static int last_visual_w;
    static int last_canvas_w;
    static int last_cell_w;
    static int last_cell_h;
    static int last_content_cols;
    static int last_content_w;
    static int last_separator_w;
    static int last_total_w;
    static int last_panel_rows;
    static int last_panel_render_h;
    static int last_top_padding_h;
    static int last_bottom_padding_h;
    static int last_corner_h;
    static int last_dst_x;
    static int last_dst_y;
    static int last_dst_w;
    static int last_dst_h;
    static int last_collapsed;
    int dst_x;
    int dst_y;
    int dst_w;
    int dst_h;
    bool changed;

    if (!view || !metrics || !dst_left)
        return;

    dst_x = (int)(dst_left->x + 0.5f);
    dst_y = (int)(dst_left->y + 0.5f);
    dst_w = (int)(dst_left->w + 0.5f);
    dst_h = (int)(dst_left->h + 0.5f);

    changed = !have_last
        || last_view_cell_w != view->cell_w
        || last_view_cell_h != view->cell_h
        || last_cols != view->cols
        || last_rows != view->rows
        || last_visual_cols != visual_cols
        || last_visual_rows != visual_rows
        || last_source_h != source_h
        || last_visual_w != visual_w
        || last_canvas_w != canvas_w
        || last_cell_w != metrics->cell_w
        || last_cell_h != metrics->cell_h
        || last_content_cols != metrics->content_cols
        || last_content_w != metrics->content_w
        || last_separator_w != metrics->separator_w
        || last_total_w != metrics->total_w
        || last_panel_rows != metrics->panel_rows
        || last_panel_render_h != metrics->panel_render_h
        || last_top_padding_h != metrics->top_padding_h
        || last_bottom_padding_h != metrics->bottom_padding_h
        || last_corner_h != metrics->corner_h
        || last_dst_x != dst_x
        || last_dst_y != dst_y
        || last_dst_w != dst_w
        || last_dst_h != dst_h
        || last_collapsed != (metrics->collapsed ? 1 : 0);

    if (!changed)
        return;

    have_last = true;
    last_view_cell_w = view->cell_w;
    last_view_cell_h = view->cell_h;
    last_cols = view->cols;
    last_rows = view->rows;
    last_visual_cols = visual_cols;
    last_visual_rows = visual_rows;
    last_source_h = source_h;
    last_visual_w = visual_w;
    last_canvas_w = canvas_w;
    last_cell_w = metrics->cell_w;
    last_cell_h = metrics->cell_h;
    last_content_cols = metrics->content_cols;
    last_content_w = metrics->content_w;
    last_separator_w = metrics->separator_w;
    last_total_w = metrics->total_w;
    last_panel_rows = metrics->panel_rows;
    last_panel_render_h = metrics->panel_render_h;
    last_top_padding_h = metrics->top_padding_h;
    last_bottom_padding_h = metrics->bottom_padding_h;
    last_corner_h = metrics->corner_h;
    last_dst_x = dst_x;
    last_dst_y = dst_y;
    last_dst_w = dst_w;
    last_dst_h = dst_h;
    last_collapsed = metrics->collapsed ? 1 : 0;
    log_trace("left-panel frame: collapsed=%d compact_row=%d "
        "term=%dx%d visual=%dx%d main_cell=%dx%d pane_cell=%dx%d "
        "source_h=%d visual_w=%d canvas_w=%d content_cols=%d content_w=%d "
        "separator_w=%d total_w=%d panel_rows=%d panel_h=%d "
        "padding_h=%d/%d "
        "corner_h=%d dst=(%d,%d %dx%d) use_tiles=%d use_bigtile=%d",
        metrics->collapsed ? 1 : 0, metrics->compact_row ? 1 : 0,
        view->cols, view->rows, visual_cols, visual_rows, view->cell_w,
        view->cell_h, metrics->cell_w, metrics->cell_h, source_h,
        visual_w, canvas_w, metrics->content_cols, metrics->content_w,
        metrics->separator_w, metrics->total_w, metrics->panel_rows,
        metrics->panel_render_h, metrics->top_padding_h,
        metrics->bottom_padding_h,
        metrics->corner_h,
        dst_x, dst_y, dst_w, dst_h, g_state.use_tiles ? 1 : 0,
        use_bigtile ? 1 : 0);
}

static int sdl_left_panel_source_rows_for_render(
    const sdl_view* view, const sdl_left_panel_metrics* metrics)
{
    int source_rows = sdl_left_panel_pane_rows_for_view(view);

    if (!metrics)
        return source_rows;

    if (source_rows < metrics->panel_rows)
        source_rows = metrics->panel_rows;

    if (metrics->collapsed) {
        for (int i = 0; i < metrics->compact_segment_count; i++) {
            int row_count = metrics->compact_source_rows[i] + 1;

            if (source_rows < row_count)
                source_rows = row_count;
        }
    }

    return source_rows;
}

static bool sdl_render_left_panel_pane_from_cells_with_metrics(
    const sdl_view* view, const SDL_FRect* dst_left,
    const sdl_left_panel_metrics* prepared_metrics)
{
    term_win* scr;
    const char* font_path;
    SDL_Texture* old_target;
    SDL_Texture* font_atlas;
    TTF_Font* mono_font;
    SDL_Rect old_clip;
    bool had_clip;
    bool font_cached = false;
    bool font_exact = false;
    sdl_left_panel_metrics metrics;
    int canvas_w;
    int canvas_h;
    int atlas_cell_w;
    int atlas_cell_h;
    float content_x;
    float content_y;
    SDL_FRect canvas_src;
    const term* source_term;
    int source_rows;
    Uint64 source_hash = 0;

    if (!view || !dst_left || dst_left->w <= 0.0f || dst_left->h <= 0.0f)
        return false;
    if (!view->term_ready || view->cell_w <= 0 || view->cell_h <= 0)
        return false;
    if (prepared_metrics)
        metrics = *prepared_metrics;
    else if (!sdl_left_panel_metrics_for_view(view, &metrics))
        return false;
    if (metrics.content_w <= 0 || metrics.total_w <= 0
        || metrics.corner_h <= 0
        || metrics.cell_w <= 0 || metrics.cell_h <= 0)
    {
        return false;
    }
    content_x = (float)sdl_left_panel_content_x_for_metrics(&metrics);
    content_y = (float)metrics.top_padding_h;

    canvas_w = metrics.total_w;
    canvas_h = metrics.corner_h;
    if (!sdl_left_panel_ensure_canvas(canvas_w, canvas_h))
        return false;

    source_rows = sdl_left_panel_source_rows_for_render(view, &metrics);
    source_term = sdl_left_panel_source_term_for_view(view, source_rows);
    if (!source_term)
        return false;
    scr = source_term->scr;
    source_hash = sdl_left_panel_source_hash(source_term, &metrics,
        source_rows);

    canvas_src = (SDL_FRect){
        .x = 0.0f,
        .y = 0.0f,
        .w = (float)canvas_w,
        .h = (float)canvas_h,
    };
    if (g_left_panel_render_cache.valid
        && g_left_panel_render_cache.content_hash == source_hash
        && g_left_panel_render_cache.content_w == canvas_w
        && g_left_panel_render_cache.content_h == canvas_h)
    {
        SDL_RenderTexture(g_state.renderer, g_left_panel_canvas, &canvas_src,
            dst_left);
        return true;
    }

    atlas_cell_w = metrics.cell_w;
    atlas_cell_h = metrics.cell_h;
    if (atlas_cell_w < 1) atlas_cell_w = 1;
    if (atlas_cell_h < 1) atlas_cell_h = 1;

    font_path = config.monospace_font[0] != '\0'
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";
    font_atlas = sdl_acquire_mono_font_atlas_cells_ex(font_path,
        atlas_cell_w, atlas_cell_h, &font_cached, &atlas_cell_w,
        &atlas_cell_h, &font_exact, true);
    if (!font_atlas) {
        return false;
    }
    if (!font_exact)
        sdl_queue_mono_font_atlas_prewarm_cells(font_path, metrics.cell_w,
            metrics.cell_h);
    mono_font = sdl_acquire_mono_font_cells(font_path, atlas_cell_w,
        atlas_cell_h);

    old_target = SDL_GetRenderTarget(g_state.renderer);
    had_clip = SDL_RenderClipEnabled(g_state.renderer);
    if (had_clip)
        SDL_GetRenderClipRect(g_state.renderer, &old_clip);

    SDL_SetRenderTarget(g_state.renderer, g_left_panel_canvas);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    {
        SDL_Color bg = sdl_left_panel_background_color();
        SDL_SetRenderDrawColor(g_state.renderer, bg.r, bg.g, bg.b, bg.a);
    }
    SDL_RenderClear(g_state.renderer);

    if (metrics.collapsed) {
        for (int i = 0; i < metrics.compact_segment_count; i++) {
            int source_row = metrics.compact_source_rows[i];
            int output_col = metrics.compact_output_cols[i];
            int output_row = metrics.compact_output_rows[i];

            if (metrics.compact_row && source_row == ROW_LIGHT) {
                sdl_left_panel_compact_light_span span;

                if (sdl_left_panel_compact_light_span_for_term(source_term,
                        scr, &span))
                {
                    sdl_render_left_panel_source_row_cells(view, source_term,
                        scr, source_row, 0, span.icon_cols, output_col,
                        output_row, content_x, content_y, metrics.cell_w,
                        metrics.cell_h, font_atlas, atlas_cell_w,
                        atlas_cell_h, mono_font);
                    sdl_render_left_panel_source_row_cells(view, source_term,
                        scr, source_row, span.text_start, span.text_width,
                        output_col + span.icon_cols + 1, output_row,
                        content_x, content_y, metrics.cell_w, metrics.cell_h,
                        font_atlas, atlas_cell_w, atlas_cell_h, mono_font);
                    continue;
                }
            }

            sdl_render_left_panel_source_row_cells(view, source_term, scr,
                source_row, 0, metrics.compact_widths[i], output_col,
                output_row, content_x, content_y, metrics.cell_w,
                metrics.cell_h,
                font_atlas, atlas_cell_w, atlas_cell_h, mono_font);
        }
    } else {
        int output_row = 0;

        for (int source_row = 0; source_row < metrics.panel_rows; source_row++) {
            if (sdl_left_panel_source_row_hidden(source_row))
                continue;
            if (output_row * metrics.cell_h >= metrics.panel_render_h)
                break;
            sdl_render_left_panel_source_row_cells(view, source_term, scr,
                source_row, 0, metrics.content_cols, 0, output_row, content_x,
                content_y, metrics.cell_w,
                metrics.cell_h, font_atlas, atlas_cell_w, atlas_cell_h,
                mono_font);
            output_row++;
        }
    }
    g_left_panel_debug_dump_rows = false;

    SDL_SetRenderTarget(g_state.renderer, old_target);
    SDL_SetRenderClipRect(g_state.renderer, had_clip ? &old_clip : NULL);
    SDL_RenderTexture(g_state.renderer, g_left_panel_canvas, &canvas_src,
        dst_left);
    if (!font_cached)
        SDL_DestroyTexture(font_atlas);
    g_left_panel_render_cache.valid = true;
    g_left_panel_render_cache.content_hash = source_hash;
    g_left_panel_render_cache.content_w = canvas_w;
    g_left_panel_render_cache.content_h = canvas_h;

    return true;
}

bool sdl_render_left_panel_pane_from_cells(const sdl_view* view,
    const SDL_FRect* dst_left)
{
    return sdl_render_left_panel_pane_from_cells_with_metrics(view, dst_left,
        NULL);
}

void sdl_combat_overlay_pane_render(void)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    SDL_Rect pane;
    SDL_Rect content;
    SDL_FRect pane_rect;
    const char* font_path;
    SDL_Texture* font_atlas;
    TTF_Font* mono_font;
    term_win* scr;
    const term* source_term;
    SDL_Rect old_clip;
    bool had_clip;
    bool font_cached = false;
    bool font_exact = false;
    int cell_h;
    int cell_w;
    int atlas_cell_w;
    int atlas_cell_h;
    int panel_cols;
    int panel_rows;
    int row_count;
    int start_row;
    int source_rows;

    if (!sdl_combat_overlay_pane_current_rect(&pane))
        return;
    if (!sdl_combat_overlay_pane_content_rect(&content))
        return;
    if (!view->term_ready || view->cell_w <= 0 || view->cell_h <= 0)
        return;

    cell_h = sdl_effective_pane_cell_height_for_type(PANE_COMBAT);
    if (cell_h < 1)
        cell_h = 1;
    cell_w = cell_h / 2;
    if (cell_w < 1)
        cell_w = 1;

    panel_cols = content.w / cell_w;
    panel_rows = content.h / cell_h;
    if (panel_cols > PANE_COMBAT_OVERLAY_COLS)
        panel_cols = PANE_COMBAT_OVERLAY_COLS;
    if (panel_cols <= 0 || panel_rows <= 0)
        return;

    row_count = sdl_combat_overlay_visible_row_count(panel_rows);
    start_row = 0;
    if (row_count <= 0 || start_row >= panel_rows)
        return;

    atlas_cell_w = cell_w;
    atlas_cell_h = cell_h;
    font_path = config.monospace_font[0] != '\0'
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";
    font_atlas = sdl_acquire_mono_font_atlas_cells_ex(font_path,
        atlas_cell_w, atlas_cell_h, &font_cached, &atlas_cell_w,
        &atlas_cell_h, &font_exact, true);
    if (!font_atlas)
        return;
    if (!font_exact)
        sdl_queue_mono_font_atlas_prewarm_cells(font_path, cell_w, cell_h);
    mono_font = sdl_acquire_mono_font_cells(font_path, atlas_cell_w,
        atlas_cell_h);

    /* Share the retained status source with the left pane. */
    source_rows = ROW_QUIVER + 1;
    source_term = sdl_left_panel_source_term_for_view(view, source_rows);
    if (!source_term) {
        if (!font_cached)
            SDL_DestroyTexture(font_atlas);
        return;
    }
    scr = source_term->scr;

    had_clip = SDL_RenderClipEnabled(g_state.renderer);
    if (had_clip)
        SDL_GetRenderClipRect(g_state.renderer, &old_clip);
    SDL_SetRenderClipRect(g_state.renderer, &pane);

    pane_rect = (SDL_FRect){
        .x = (float)pane.x,
        .y = (float)pane.y,
        .w = (float)pane.w,
        .h = (float)pane.h,
    };
    {
        SDL_Color bg = sdl_left_panel_background_color();

        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_state.renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(g_state.renderer, &pane_rect);
    }

    for (int i = 0; i < row_count; i++) {
        int source_row = -1;
        int dest_row = start_row + i;

        if (dest_row >= panel_rows)
            break;
        if (!sdl_combat_overlay_visible_source_row_at_index(i, panel_rows,
                &source_row))
        {
            continue;
        }

        sdl_render_left_panel_source_row_cells(view, source_term, scr,
            source_row, 0, panel_cols, 0, dest_row, (float)content.x,
            (float)content.y, cell_w, cell_h, font_atlas, atlas_cell_w,
            atlas_cell_h, mono_font);
    }

    SDL_SetRenderClipRect(g_state.renderer, had_clip ? &old_clip : NULL);
    if (!font_cached)
        SDL_DestroyTexture(font_atlas);
}

static bool sdl_render_main_view_with_left_panel_metrics(const sdl_view* view,
    const sdl_left_panel_metrics* prepared_metrics)
{
    bool render_styled_left_panel = sdl_left_panel_pane_presentation_active();
    bool skip_terminal_left_panel =
        g_description_overlay.active && g_description_overlay.interactive
        && sdl_left_panel_pane_layout_enabled();
    int visual_cols;
    int visual_rows;
    int source_h;
    int visual_w;
    int canvas_w;
    sdl_left_panel_metrics metrics;
    SDL_FRect src_map;
    SDL_FRect src_left;
    SDL_FRect dst_map;
    SDL_FRect dst_left;
    float grid_x;
    float grid_y;

    if (!render_styled_left_panel && !skip_terminal_left_panel)
        return false;
    if (!view || !view->canvas || !view->term_ready)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0)
        return false;
    if (prepared_metrics)
        metrics = *prepared_metrics;
    else if (!sdl_left_panel_metrics_for_view(view, &metrics))
        return false;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    source_h = visual_rows * view->cell_h;
    visual_w = visual_cols * view->cell_w;
    canvas_w = view->cols * view->cell_w;
    if (visual_w <= metrics.total_w || visual_rows <= 0
        || metrics.corner_h <= 0)
    {
        return false;
    }
    if (canvas_w <= COL_MAP * view->cell_w)
        return false;

    grid_x = (float)(view->rect.x + view->margin_x);
    grid_y = (float)(view->rect.y + view->margin_y);
    if (render_styled_left_panel)
    {
        if (!sdl_left_panel_pane_rect_for_metrics(view, &metrics, &dst_left))
            return false;
        sdl_left_panel_debug_log_frame(view, &metrics, &dst_left, visual_cols,
            visual_rows, source_h, visual_w, canvas_w);
    }

    src_map = (SDL_FRect){
        .x = (float)(COL_MAP * view->cell_w),
        .y = 0.0f,
        .w = (float)visual_w,
        .h = (float)source_h,
    };
    if (src_map.x + src_map.w > (float)canvas_w)
        src_map.w = (float)canvas_w - src_map.x;
    if (src_map.w <= 0.0f || src_map.h <= 0.0f)
        return false;

    dst_map = (SDL_FRect){
        .x = grid_x,
        .y = grid_y,
        .w = src_map.w,
        .h = src_map.h,
    };
    SDL_RenderTexture(g_state.renderer, view->canvas, &src_map, &dst_map);

    if (!render_styled_left_panel)
        return true;

    src_left = (SDL_FRect){
        .x = 0.0f,
        .y = 0.0f,
        .w = (float)(COL_MAP * view->cell_w),
        .h = (float)MIN(source_h, metrics.corner_h),
    };

    if (!sdl_left_panel_pane_runtime_active()
        || !sdl_render_left_panel_pane_from_cells_with_metrics(view, &dst_left,
            &metrics))
    {
        SDL_Color bg = sdl_left_panel_background_color();
        SDL_FRect src_content = src_left;
        SDL_FRect dst_content = dst_left;

        if (g_description_overlay.active && g_description_overlay.interactive)
            return true;

        SDL_SetRenderDrawColor(g_state.renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(g_state.renderer, &dst_left);

        dst_content.x +=
            (float)sdl_left_panel_content_x_for_metrics(&metrics);
        dst_content.y += (float)metrics.top_padding_h;
        dst_content.w = (float)metrics.content_w;
        dst_content.h = (float)metrics.panel_render_h;
        if (!metrics.collapsed) {
            float source_top = (float)(ROW_NAME * view->cell_h);

            src_content.y += source_top;
            src_content.h -= source_top;
        }
        if (dst_content.y + dst_content.h > dst_left.y + dst_left.h)
            dst_content.h = (dst_left.y + dst_left.h) - dst_content.y;
        if (src_content.h > dst_content.h)
            src_content.h = dst_content.h;
        if (dst_content.w > 0.0f && dst_content.h > 0.0f
            && src_content.w > 0.0f && src_content.h > 0.0f)
        {
            SDL_RenderTexture(g_state.renderer, view->canvas, &src_content,
                &dst_content);
        }
    }

    return true;
}

bool sdl_render_main_view_with_left_panel(const sdl_view* view)
{
    return sdl_render_main_view_with_left_panel_metrics(view, NULL);
}

bool sdl_saved_screen_cell_changed(const term_win* scr,
    const term_win* mem, int x, int y)
{
    if (!scr || !mem || x < 0 || y < 0)
        return false;
    if (!scr->a || !scr->c || !mem->a || !mem->c)
        return false;
    if (!scr->a[y] || !scr->c[y] || !mem->a[y] || !mem->c[y])
        return false;

    if (scr->a[y][x] != mem->a[y][x])
        return true;
    if (scr->c[y][x] != mem->c[y][x])
        return true;
    if (scr->ta && mem->ta && scr->ta[y] && mem->ta[y]
        && scr->ta[y][x] != mem->ta[y][x])
    {
        return true;
    }
    if (scr->tc && mem->tc && scr->tc[y] && mem->tc[y]
        && scr->tc[y][x] != mem->tc[y][x])
    {
        return true;
    }
    if (scr->story && mem->story && scr->story[y] && mem->story[y]
        && scr->story[y][x] != mem->story[y][x])
    {
        return true;
    }
    if (scr->health && mem->health && scr->health[y] && mem->health[y]
        && scr->health[y][x] != mem->health[y][x])
    {
        return true;
    }

    return false;
}

void sdl_redraw_saved_screen_overlay_cells(const sdl_view* view,
    const SDL_FRect* clip_rect)
{
    const term* t;
    term_win* scr;
    term_win* mem;
    int cols;
    int rows;
    int visual_cols;
    int visual_rows;
    float grid_x;
    float grid_y;
    bool had_clip;
    SDL_Rect old_clip;
    SDL_Rect clip;

    if (!view || !clip_rect || !view->canvas)
        return;
    if (view->cell_w <= 0 || view->cell_h <= 0)
        return;

    t = &view->t;
    scr = t->scr;
    mem = t->mem;
    if (!scr || !mem)
        return;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    cols = MIN(COL_MAP, visual_cols);
    rows = MIN(t->hgt, visual_rows);
    if (cols <= 0 || rows <= 0)
        return;

    grid_x = (float)(view->rect.x + view->margin_x);
    grid_y = (float)(view->rect.y + view->margin_y);

    had_clip = SDL_RenderClipEnabled(g_state.renderer);
    if (had_clip)
        SDL_GetRenderClipRect(g_state.renderer, &old_clip);
    clip = (SDL_Rect){
        .x = (int)clip_rect->x,
        .y = (int)clip_rect->y,
        .w = (int)(clip_rect->w + 0.5f),
        .h = (int)(clip_rect->h + 0.5f),
    };
    SDL_SetRenderClipRect(g_state.renderer, &clip);

    for (int y = 0; y < rows; y++)
    {
        int x = 0;

        while (x < cols)
        {
            int start;
            int len;
            SDL_FRect src;
            SDL_FRect dst;

            while (x < cols && !sdl_saved_screen_cell_changed(scr, mem, x, y))
                x++;
            if (x >= cols)
                break;

            start = x;
            while (x < cols && sdl_saved_screen_cell_changed(scr, mem, x, y))
                x++;
            len = x - start;

            src = (SDL_FRect){
                .x = (float)(start * view->cell_w),
                .y = (float)(y * view->cell_h),
                .w = (float)(len * view->cell_w),
                .h = (float)view->cell_h,
            };
            dst = (SDL_FRect){
                .x = grid_x + src.x,
                .y = grid_y + src.y,
                .w = src.w,
                .h = src.h,
            };
            SDL_RenderTexture(g_state.renderer, view->canvas, &src, &dst);
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, had_clip ? &old_clip : NULL);
}

bool sdl_render_saved_screen_left_panel_backdrop(const sdl_view* view)
{
    sdl_left_panel_metrics metrics;
    SDL_FRect pane_rect;
    SDL_FRect left_rect;
    SDL_FRect map_src;
    int visual_cols;
    int visual_rows;
    int visual_w;
    int visual_h;
    int canvas_w;
    float grid_x;
    float grid_y;

    if (!sdl_saved_screen_left_panel_pane_active())
        return false;
    if (!sdl_left_panel_pane_layout_enabled())
        return false;
    if (!view || !view->canvas || !view->term_ready)
        return false;
    if (!view->t.mem || !view->t.scr)
        return false;
    if (view->cell_w <= 0 || view->cell_h <= 0)
        return false;
    if (!sdl_left_panel_metrics_for_view(view, &metrics))
        return false;

    visual_cols = sdl_main_view_visual_cols(view);
    visual_rows = sdl_main_view_visual_rows(view);
    visual_w = visual_cols * view->cell_w;
    visual_h = visual_rows * view->cell_h;
    canvas_w = view->cols * view->cell_w;
    if (visual_w <= 0 || visual_h <= 0 || canvas_w <= 0)
        return false;

    grid_x = (float)(view->rect.x + view->margin_x);
    grid_y = (float)(view->rect.y + view->margin_y);
    left_rect = (SDL_FRect){
        .x = grid_x,
        .y = grid_y,
        .w = (float)MIN(COL_MAP * view->cell_w, visual_w),
        .h = (float)visual_h,
    };
    if (left_rect.w <= 0.0f || left_rect.h <= 0.0f)
        return false;

    map_src = (SDL_FRect){
        .x = (float)(COL_MAP * view->cell_w),
        .y = 0.0f,
        .w = left_rect.w,
        .h = left_rect.h,
    };
    if (map_src.x + map_src.w > (float)canvas_w)
        map_src.w = (float)canvas_w - map_src.x;

    if (map_src.w > 0.0f && map_src.h > 0.0f)
    {
        SDL_FRect map_dst = left_rect;

        map_dst.w = map_src.w;
        SDL_RenderTexture(g_state.renderer, view->canvas, &map_src, &map_dst);
    }
    else
    {
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(g_state.renderer, &left_rect);
    }

    if (sdl_left_panel_pane_rect_for_metrics(view, &metrics, &pane_rect))
    {
        if (pane_rect.x < left_rect.x)
            pane_rect.x = left_rect.x;
        if (pane_rect.y < left_rect.y)
            pane_rect.y = left_rect.y;
        if (pane_rect.x + pane_rect.w > left_rect.x + left_rect.w)
            pane_rect.w = left_rect.x + left_rect.w - pane_rect.x;
        if (pane_rect.y + pane_rect.h > left_rect.y + left_rect.h)
            pane_rect.h = left_rect.y + left_rect.h - pane_rect.y;
        if (pane_rect.w > 0.0f && pane_rect.h > 0.0f)
            (void)sdl_render_left_panel_pane_from_cells(view, &pane_rect);
    }

    sdl_redraw_saved_screen_overlay_cells(view, &left_rect);
    return true;
}

bool sdl_render_current_window_frame(void)
{
    bool show_supporting_panes;
    bool hide_main_menu_supporting_panes;
    bool hide_main_menu_overlays;
    bool layout_matches;
    SDL_Rect layout_screen;
    SDL_Rect side_map_rect;
    bool side_map_visible;
    bool have_left_panel_metrics = false;
    sdl_left_panel_metrics left_panel_metrics;
    SDL_FRect left_panel_rect;
    int visible_views = 0;

    g_sdl_present_generation++;
    if (g_sdl_present_generation == 0)
        g_sdl_present_generation = 1;

    if (g_suppress_layout_refresh_present)
        return false;

    if (sdl_pause_text_screen_active()) {
        sdl_pause_text_screen_render();
        return true;
    }

    if (sdl_tale_screen_active()) {
        sdl_tale_screen_render();
        return true;
    }

    if (sdl_poetry_screen_active()) {
        sdl_poetry_screen_render();
        return true;
    }

    if (sdl_halls_screen_active()) {
        sdl_halls_screen_render();
        return true;
    }

    if (sdl_hint_quest_menu_active()) {
        sdl_hint_quest_menu_render();
        return true;
    }

    if (sdl_welcome_screen_active()) {
        sdl_welcome_screen_render();
        /* The yes/no confirm is modal and must stay visible above any
         * full-screen surface (see sdl_yes_no_prompt_handle_modal_event). */
        sdl_touch_pane_render_yes_no_prompt();
        return true;
    }

    if (sdl_character_sheet_screen_active()) {
        sdl_character_sheet_screen_render();
        sdl_touch_pane_render_yes_no_prompt();
        /* In-menu value pickers (ui_question_ask_overlay) draw on top of the
         * settings/character-sheet screen without repainting it, so render the
         * question overlay here too; otherwise the early return below skips it
         * and the picker is hidden behind the menu it was opened from. */
        sdl_question_menu_render();
        return true;
    }

    show_supporting_panes = sdl_should_show_supporting_panes();
    hide_main_menu_supporting_panes =
        sdl_main_menu_overlay_hides_supporting_panes();
    hide_main_menu_overlays = g_main_menu_overlay_active;
    layout_matches = sdl_layout_matches_supporting_pane_visibility();
    if (!layout_matches)
        return false;
    if (hide_main_menu_supporting_panes)
        show_supporting_panes = false;

    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);
    layout_screen = sdl_get_layout_screen_rect();
    if ((sdl_left_panel_pane_presentation_active()
            || (g_description_overlay.active
                && g_description_overlay.interactive
                && sdl_left_panel_pane_layout_enabled()))
        && sdl_left_panel_metrics_for_view(&g_views[PANE_MAIN],
            &left_panel_metrics))
    {
        have_left_panel_metrics = true;
    }
    if (sdl_left_panel_pane_presentation_active()
        && have_left_panel_metrics
        && sdl_left_panel_pane_rect_for_metrics(&g_views[PANE_MAIN],
            &left_panel_metrics, &left_panel_rect))
    {
        g_pane_rects[PANE_LEFT_PANEL] = (SDL_Rect){
            .x = (int)left_panel_rect.x,
            .y = (int)left_panel_rect.y,
            .w = (int)left_panel_rect.w,
            .h = (int)left_panel_rect.h,
        };
    } else {
        g_pane_rects[PANE_LEFT_PANEL] = (SDL_Rect){ 0 };
    }
    sdl_apply_top_right_overlay_offset();
    side_map_visible = !hide_main_menu_overlays
        && !hide_main_menu_supporting_panes
        && sdl_side_map_pane_current_rect(&side_map_rect);

    for (int i = 0; i < MAX_TERM_DATA; i++) {
        if (!g_views[i].canvas)
            continue;
        if (!show_supporting_panes && i != PANE_MAIN && i != PANE_TOUCH)
            continue;
        visible_views++;
    }
    if (show_supporting_panes && sdl_left_panel_pane_presentation_active()
        && sdl_rect_has_area(&g_pane_rects[PANE_LEFT_PANEL]))
    {
        visible_views++;
    }
    if (side_map_visible)
        visible_views++;

    for (int i = 0; i < MAX_TERM_DATA; i++) {
        sdl_view* view = &g_views[i];
        float dst_w;
        float dst_h;
        int visual_cols;
        int visual_rows;

        if (!view->canvas)
            continue;
        if (view->cols <= 0 || view->rows <= 0 || view->cell_w <= 0 || view->cell_h <= 0)
            continue;
        if (!show_supporting_panes && i != PANE_MAIN)
            continue;
        if (hide_main_menu_overlays && i == PANE_ROLLS
            && sdl_view_is_overlay_log_pane(view))
        {
            continue;
        }
        /* Hide the message log(s) while an interactive item description popup is
         * open, so it doesn't show through/around the popup. */
        if (g_description_overlay.active && g_description_overlay.interactive
            && (i == PANE_LOG
                || (i == PANE_ROLLS && sdl_view_is_overlay_log_pane(view))))
        {
            continue;
        }
        if (i == PANE_MAIN && show_supporting_panes
            && sdl_render_main_view_with_left_panel_metrics(view,
                have_left_panel_metrics ? &left_panel_metrics : NULL))
            continue;

        visual_cols = (i == PANE_MAIN)
            ? sdl_main_view_visual_cols(view)
            : view->cols;
        visual_rows = (i == PANE_MAIN)
            ? sdl_main_view_visual_rows(view)
            : view->rows;
        dst_w = (float)(visual_cols * view->cell_w);
        dst_h = (float)(visual_rows * view->cell_h);
        if (dst_w <= 0.0f || dst_h <= 0.0f)
            continue;

        /*
         * The overlay log only shows a narrow right-hand band; blit just that
         * sub-rectangle so the transparent left margin reveals the map behind
         * it instead of the pane's translucent background.  Keep a few pixels
         * of panel before the text so it does not touch the left border.
         */
        float src_x = 0.0f;
        float blit_off = 0.0f;
        bool overlay_log = (i == PANE_ROLLS
            && sdl_view_is_overlay_log_pane(view));

        if (overlay_log)
        {
            int pad = view->cell_w / 8;

            if (pad < 2)
                pad = 2;
            blit_off = (float)(pane_log_overlay_left_margin(view->cols)
                * view->cell_w - pad);
            if (blit_off < 0.0f)
                blit_off = 0.0f;
            src_x = blit_off;
        }

        float band_x = (float)(view->rect.x + view->margin_x) + blit_off;
        float band_w = dst_w - blit_off;
        float src_y = 0.0f;
        float src_h = (float)(visual_rows * view->cell_h);
        float dst_y = (float)(view->rect.y + view->margin_y);
        float blit_h = dst_h;

        if (overlay_log)
        {
            /* Bottom-anchor the content above its single panel-backed bottom
             * margin.  The pane allocation reserves no matching top margin,
             * so the first configured row begins at the top of the band. */
            int bottom_margin =
                pane_log_overlay_vertical_margin_px(view->cell_h);
            float band_top = (float)view->rect.y;
            float band_bottom = (float)(view->rect.y + view->rect.h);

            dst_y = band_bottom - (float)bottom_margin - src_h;
            blit_h = src_h;
            if (dst_y < band_top)
            {
                float clip = band_top - dst_y;

                src_y += clip;
                src_h -= clip;
                blit_h -= clip;
                dst_y = band_top;
            }

            SDL_Color bg = sdl_color_from_attr(TERM_DARK);

            bg.a = sdl_view_background_alpha(view);
            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_state.renderer, bg.r, bg.g, bg.b, bg.a);

            if (dst_y > band_top)
                SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
                    .x = band_x, .y = band_top,
                    .w = band_w, .h = dst_y - band_top });

            float content_bottom = dst_y + blit_h;

            if (content_bottom < band_bottom)
                SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
                    .x = band_x, .y = content_bottom,
                    .w = band_w, .h = band_bottom - content_bottom });
        }

        SDL_RenderTexture(g_state.renderer, view->canvas, &(SDL_FRect){
            .x = src_x,
            .y = src_y,
            .w = (float)(visual_cols * view->cell_w) - blit_off,
            .h = src_h,
        }, &(SDL_FRect){
            .x = band_x,
            .y = dst_y,
            .w = band_w,
            .h = blit_h,
        });

        if (i == PANE_MAIN)
            (void)sdl_render_saved_screen_left_panel_backdrop(view);
    }

    if (!hide_main_menu_overlays) {
        sdl_side_map_pane_render();
        sdl_pointer_aim_render();
        sdl_pointer_attack_render();
        sdl_mouse_path_render();
        sdl_combat_overlay_pane_render();
        sdl_status_pane_render();
        sdl_status_depth_pane_render();
        sdl_popup_notification_render();
        sdl_depth_menu_pane_render();
        sdl_narrative_banner_render();
        sdl_object_tooltip_render();
        sdl_player_exchange_render();
        sdl_player_action_menu_render();
        sdl_touch_round_render();
    }

    if (visible_views > 1) {
        for (int i = 0; i < MAX_TERM_DATA; i++) {
            sdl_view* view = &g_views[i];
            bool overlay_log;

            if (!view->canvas)
                continue;
            if (!show_supporting_panes && i != PANE_MAIN && i != PANE_TOUCH)
                continue;
            if (hide_main_menu_overlays && i == PANE_ROLLS
                && sdl_view_is_overlay_log_pane(view))
            {
                continue;
            }

            if (i == PANE_TOUCH)
                continue;
            /* Mirror the blit loop: keep the log's border hidden too while an
             * interactive item description popup is open. */
            if (g_description_overlay.active && g_description_overlay.interactive
                && (i == PANE_LOG
                    || (i == PANE_ROLLS && sdl_view_is_overlay_log_pane(view))))
            {
                continue;
            }

            overlay_log = i == PANE_ROLLS
                && sdl_view_is_overlay_log_pane(view);
            if (overlay_log && !get_sdl_show_overlay_log_border())
                continue;

            if (overlay_log || config.show_pane_borders)
                SDL_SetRenderDrawColor(g_state.renderer,
                    255, 255, 255, 128);
            else
                SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);

            /* Draw only internal leading edges.  This keeps separators
             * between panes without painting a frame around the window. */
            SDL_Rect edge_rect = view->rect;
            bool draw_left =
                (layout_screen.w > 0 && view->rect.x > layout_screen.x);
            bool draw_bottom = false;

            /* The overlay log only paints a narrow right-hand band, so inset
             * its border to where that band (and the messages) begin, and close
             * it off with a bottom edge. */
            if (overlay_log)
            {
                int pad = view->cell_w / 8;
                int inset;

                if (pad < 2)
                    pad = 2;
                inset = view->margin_x
                    + pane_log_overlay_left_margin(view->cols) * view->cell_w
                    - pad;

                if (inset > 0 && inset < view->rect.w)
                {
                    edge_rect.x += inset;
                    edge_rect.w -= inset;
                    draw_left = true;
                }
                draw_bottom = true;
            }

            sdl_draw_pane_edges(&edge_rect,
                draw_left,
                (layout_screen.h > 0 && view->rect.y > layout_screen.y),
                false,
                draw_bottom);
        }

        if (side_map_visible) {
            if (config.show_pane_borders)
                SDL_SetRenderDrawColor(g_state.renderer,
                    255, 255, 255, 128);
            else
                SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            sdl_draw_pane_edges(&side_map_rect,
                (layout_screen.w > 0 && side_map_rect.x > layout_screen.x),
                (layout_screen.h > 0 && side_map_rect.y > layout_screen.y),
                false,
                false);
        }

    }

    sdl_touch_pane_render();
    if (!hide_main_menu_overlays)
        sdl_touch_zone_render_markers();
    if (!g_touch_tutorial_suppress_runtime_top_panel)
        sdl_touch_top_panel_render();
    sdl_touch_hidden_indicator_render();
    /* The Main Menu is the topmost gameplay pane.  Draw it after pane borders
     * and touch controls so neither can obscure its panel or labels. */
    sdl_main_menu_pane_render();
    sdl_touch_pane_render_reset_prompt();
    sdl_touch_pane_render_yes_no_prompt();
    sdl_log_pane_menu_render();
    sdl_side_pane_menu_render();
    sdl_unified_look_sidebar_render();
    sdl_unified_look_prompt_render();
    sdl_song_menu_render();
    sdl_question_menu_render();
    sdl_description_overlay_render();
    /* Drawn after the description popup (and its dimming backdrop) so the thumb
     * buttons stay on top and visible while a description is open. */
    if (!hide_main_menu_overlays)
        sdl_touch_thumb_render();
    sdl_touch_exit_button_render();

    return true;
}

/* When set, the renderer keeps showing the last presented frame.  Used while
 * the program is quitting after the Halls of Mandos score screen so the
 * restored dungeon view is never flashed on screen just before the window
 * closes. */
bool g_sdl_present_suppressed = false;
static int g_sdl_present_batch_depth = 0;

void sdl_set_present_suppressed(bool suppressed)
{
    g_sdl_present_suppressed = suppressed;
}

void sdl_present_batch_begin(void)
{
    g_sdl_present_batch_depth++;
}

void sdl_present_batch_end(void)
{
    sdl_view* view;

    if (g_sdl_present_batch_depth <= 0)
        return;

    g_sdl_present_batch_depth--;
    if (g_sdl_present_batch_depth > 0)
        return;

    view = sdl_view_from_term(Term);
    if (!view)
        view = &g_views[PANE_MAIN];
    sdl_present_if_needed(view);
}

void sdl_present_if_needed(sdl_view* d)
{
    if (g_sdl_present_suppressed)
        return;

    if (g_sdl_present_batch_depth > 0)
        return;

    if (!g_state.need_present)
        return;

    /* Renderers for animated UI may request the following frame.  Consume the
     * current request before rendering so that request is not cleared after
     * the page-curl/notification render has re-armed it. */
    g_state.need_present = false;
    if (!sdl_render_current_window_frame()) {
        g_state.need_present = true;
        return;
    }

    SDL_RenderPresent(g_state.renderer);
    sdl_restore_render_target(d);
}


