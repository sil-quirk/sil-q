#include "angband.h"
#include "sdl/main-sdl-private.h"

bool sdl_mouse_path_has_pending_key(void)
{
    char ch = '\0';

    if (!Term)
        return false;

    return Term_inkey(&ch, false, false) == 0;
}

bool sdl_mouse_path_take_step_command(int* command, int* dir)
{
    int next_y;
    int next_x;
    int target_y;
    int target_x;
    int blocked_target_kind;
    bool attack_step;

    if (!command || !dir)
        return false;
    if (sdl_mouse_stuck_door_bash_take_command(command, dir))
        return true;
    if (!g_mouse_path.follow_active)
        return false;

    /* If the previous step tried to open a closed door and the door is
     * still closed with the player in the same spot, the open/lockpick
     * attempt failed.  Stop the path so we don't auto-retry the lockpick. */
    if (g_mouse_path.last_step_door_pending) {
        bool open_failed = (p_ptr->py == g_mouse_path.last_step_player_y
            && p_ptr->px == g_mouse_path.last_step_player_x
            && in_bounds(g_mouse_path.last_step_door_y,
                g_mouse_path.last_step_door_x)
            && cave_known_closed_door_bold(g_mouse_path.last_step_door_y,
                g_mouse_path.last_step_door_x));

        g_mouse_path.last_step_door_pending = false;
        if (open_failed) {
            sdl_mouse_path_cancel();
            return false;
        }
    }

    if (!sdl_mouse_gameplay_context_active()) {
        sdl_mouse_path_cancel();
        return false;
    }

    if (g_mouse_path.path_wake_pending) {
        (void)sdl_mouse_consume_wake_key();
        g_mouse_path.path_wake_pending = false;
    }

    if (sdl_mouse_path_has_pending_key()) {
        sdl_mouse_path_cancel();
        return false;
    }

    target_y = g_mouse_path.target_y;
    target_x = g_mouse_path.target_x;

    if (g_mouse_path.target_m_idx > 0) {
        monster_type* m_ptr = &mon_list[g_mouse_path.target_m_idx];

        if (!m_ptr->r_idx || !m_ptr->ml
            || !grid_info_is_available(m_ptr->fy, m_ptr->fx))
        {
            sdl_mouse_path_cancel();
            return false;
        }

        target_y = m_ptr->fy;
        target_x = m_ptr->fx;
    }

    if ((p_ptr->py == target_y) && (p_ptr->px == target_x)) {
        sdl_mouse_path_cancel();
        return false;
    }

    if (!sdl_mouse_path_compute(target_y, target_x)) {
        sdl_mouse_path_cancel();
        bell("Mouse path blocked.");
        return false;
    }

    blocked_target_kind = g_mouse_path.blocked_target_kind;
    if (g_mouse_path.path_len <= 0) {
        if (blocked_target_kind == SDL_MOUSE_PATH_BLOCKED_STUCK_DOOR) {
            int bash_dir = 0;

            if (sdl_mouse_stuck_door_bash_target(target_y, target_x,
                    &bash_dir)
                && get_check("Stuck door, do you want to bash it? "))
            {
                sdl_mouse_path_cancel();
                sdl_mouse_note_feature_for_action(target_y, target_x);
                *command = 'b';
                *dir = bash_dir;
                return true;
            }
        }
        else if (blocked_target_kind == SDL_MOUSE_PATH_BLOCKED_DANGER) {
            int danger_dir = motion_dir(p_ptr->py, p_ptr->px, target_y,
                target_x);

            if (danger_dir >= 1 && danger_dir <= 9 && danger_dir != 5
                && target_y == p_ptr->py + ddy[danger_dir]
                && target_x == p_ptr->px + ddx[danger_dir])
            {
                sdl_mouse_path_cancel();
                sdl_mouse_note_feature_for_action(target_y, target_x);
                *command = ';';
                *dir = danger_dir;
                return true;
            }
        }

        sdl_mouse_path_cancel();
        *command = ' ';
        *dir = 0;
        return blocked_target_kind != SDL_MOUSE_PATH_BLOCKED_NONE;
    }

    next_y = GRID_Y(g_mouse_path.path[0]);
    next_x = GRID_X(g_mouse_path.path[0]);
    attack_step = sdl_mouse_grid_has_visible_monster(next_y, next_x, NULL);
    if (cave_feat[next_y][next_x] == FEAT_CHASM)
        sdl_mouse_note_feature_for_action(next_y, next_x);

    *command = ';';
    *dir = motion_dir(p_ptr->py, p_ptr->px, next_y, next_x);
    if (*dir == 5) {
        sdl_mouse_path_cancel();
        return false;
    }

    if (attack_step)
        sdl_mouse_path_cancel();
    else if (cave_known_closed_door_bold(next_y, next_x)) {
        /* The walk command will trigger an open/lockpick attempt without
         * moving the player.  Remember this so we can detect a failed
         * attempt on the next iteration and stop retrying. */
        g_mouse_path.last_step_door_pending = true;
        g_mouse_path.last_step_door_y = next_y;
        g_mouse_path.last_step_door_x = next_x;
        g_mouse_path.last_step_player_y = p_ptr->py;
        g_mouse_path.last_step_player_x = p_ptr->px;
    }

    return true;
}

bool sdl_mouse_grid_has_visible_monster(int y, int x, int* out_m_idx)
{
    int m_idx;

    if (!in_bounds(y, x))
        return false;

    m_idx = cave_m_idx[y][x];
    if ((m_idx <= 0) || !mon_list[m_idx].ml)
        return false;

    if (out_m_idx)
        *out_m_idx = m_idx;
    return true;
}

bool sdl_mouse_monster_is_friendly(int m_idx)
{
    monster_type* m_ptr;
    monster_race* r_ptr;

    if (m_idx <= 0)
        return false;

    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx)
        return false;

    r_ptr = &r_info[m_ptr->r_idx];
    return (r_ptr->flags1 & (RF1_PEACEFUL)) != 0;
}

bool sdl_mouse_grid_has_marked_object(int y, int x, object_type** out_obj)
{
    s16b o_idx;

    if (!in_bounds(y, x) || !grid_info_is_available(y, x))
        return false;
    if (!(cave_floorlike_bold(y, x) || cave_feat[y][x] == FEAT_SUNLIGHT))
        return false;

    o_idx = cave_o_idx[y][x];
    if (!o_idx || !o_list[o_idx].marked)
        return false;

    if (out_obj)
        *out_obj = &o_list[o_idx];
    return true;
}

void sdl_object_tooltip_clear(void)
{
    if (!g_object_tooltip.active)
        return;

    g_object_tooltip.active = false;
    g_object_tooltip.touch = false;
    g_object_tooltip.term_cell = false;
    g_object_tooltip.character_panel_cell = false;
    g_object_tooltip.map_y = 0;
    g_object_tooltip.map_x = 0;
    g_object_tooltip.cell_col = 0;
    g_object_tooltip.cell_row = 0;
    g_object_tooltip.cell_cols = 0;
    g_object_tooltip.expires_at = 0;
    g_object_tooltip.text[0] = '\0';
    g_state.need_present = true;
}

void sdl_object_tooltip_append_part(char* buf, size_t buflen,
    cptr text)
{
    if (!buf || !buflen || !text || !text[0])
        return;

    if (buf[0])
        SDL_strlcat(buf, "; ", buflen);
    SDL_strlcat(buf, text, buflen);
}

bool sdl_object_tooltip_feature_name(int y, int x, cptr* out_name)
{
    int feat;
    cptr name;

    if (out_name)
        *out_name = NULL;
    if (!in_bounds(y, x))
        return false;
    if (!sdl_mouse_feature_known_for_action(y, x))
        return false;
    if ((cave_feat[y][x] >= FEAT_TRAP_HEAD)
        && (cave_feat[y][x] <= FEAT_TRAP_TAIL)
        && (cave_info[y][x] & CAVE_HIDDEN))
    {
        return false;
    }

    feat = f_info[cave_feat[y][x]].mimic;
    if (feat == FEAT_NONE || feat == FEAT_FLOOR || feat == FEAT_RAGE_FLOOR)
        return false;

    if ((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL)
        && (cave_info[y][x] & CAVE_HIDDEN))
    {
        return false;
    }

    if (feat == FEAT_OPEN)
        name = "open door";
    else if (feat == FEAT_BROKEN)
        name = "broken door";
    else if (feat == FEAT_LESS)
        name = "up staircase";
    else if (feat == FEAT_MORE)
        name = "down staircase";
    else if (feat == FEAT_LESS_SHAFT)
        name = "up shaft";
    else if (feat == FEAT_MORE_SHAFT)
        name = "down shaft";
    else if (feat == FEAT_SUNLIGHT)
        name = "patch of sunlight";
    else
        name = f_name + f_info[feat].name;

    if (!name || !name[0])
        return false;

    if (out_name)
        *out_name = name;
    return true;
}

void sdl_object_tooltip_monster_morale_text(monster_type* m_ptr,
    char* out, size_t out_len)
{
    int color = TERM_WHITE;
    int morale_num;

    if (!out || !out_len)
        return;

    out[0] = '\0';
    if (!m_ptr)
        return;

    if (get_alertness_text(m_ptr, (int)out_len, out, &color))
        return;

    morale_num = (m_ptr->morale >= 0) ? ((m_ptr->morale + 9) / 10)
                                      : (m_ptr->morale / 10);
    strnfmt(out, out_len, "%d", morale_num);
}

bool sdl_object_tooltip_format_grid(int y, int x, char* out,
    size_t out_len)
{
    char local[sizeof(g_object_tooltip.text)];
    char* buf = out ? out : local;
    size_t buflen = out ? out_len : sizeof(local);
    int m_idx = 0;
    int object_count = 0;
    cptr feature_name = NULL;

    if (!buf || !buflen)
        return false;

    buf[0] = '\0';
    if (!in_bounds(y, x) || !grid_info_is_available(y, x))
        return false;

    if (sdl_mouse_grid_has_visible_monster(y, x, &m_idx)) {
        monster_type* m_ptr = &mon_list[m_idx];
        char m_name[80];
        char hp_bar[10];
        char morale_text[24];
        char m_label[144];

        monster_desc(m_name, sizeof(m_name), m_ptr, 0x08);
        sdl_object_tooltip_monster_morale_text(m_ptr, morale_text,
            sizeof(morale_text));
        if (m_ptr->maxhp > 0) {
            monster_health_bar_text(m_ptr, hp_bar, sizeof(hp_bar), 8);
            if (morale_text[0])
                strnfmt(m_label, sizeof(m_label), "%s [HP: %s, Morale: %s]",
                    m_name, hp_bar[0] ? hp_bar : "-", morale_text);
            else
                strnfmt(m_label, sizeof(m_label), "%s [HP: %s]", m_name,
                    hp_bar[0] ? hp_bar : "-");
            sdl_object_tooltip_append_part(buf, buflen, m_label);
        } else {
            if (morale_text[0]) {
                strnfmt(m_label, sizeof(m_label), "%s [Morale: %s]", m_name,
                    morale_text);
                sdl_object_tooltip_append_part(buf, buflen, m_label);
            } else {
                sdl_object_tooltip_append_part(buf, buflen, m_name);
            }
        }
    }

    if (cave_floorlike_bold(y, x) || cave_feat[y][x] == FEAT_SUNLIGHT) {
        object_type* o_ptr;

        for (o_ptr = get_first_object(y, x); o_ptr;
             o_ptr = get_next_object(o_ptr))
        {
            char o_name[80];

            if (!o_ptr->k_idx || !o_ptr->marked)
                continue;

            object_count++;
            if (object_count > 2)
                continue;

            object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
            sdl_object_tooltip_append_part(buf, buflen, o_name);
        }

        if (object_count > 2) {
            char more[32];

            strnfmt(more, sizeof(more), "+%d more", object_count - 2);
            sdl_object_tooltip_append_part(buf, buflen, more);
        }
    }

    if (sdl_object_tooltip_feature_name(y, x, &feature_name))
        sdl_object_tooltip_append_part(buf, buflen, feature_name);

    return buf[0] != '\0';
}

bool sdl_object_tooltip_show_grid(int map_y, int map_x, bool touch)
{
    char name[sizeof(g_object_tooltip.text)];
    Uint64 expires_at = 0;

    if (!sdl_object_tooltip_format_grid(map_y, map_x, name, sizeof(name))) {
        sdl_object_tooltip_clear();
        return false;
    }

    if (touch)
        expires_at = SDL_GetTicksNS()
            + (Uint64)SDL_OBJECT_TOOLTIP_TOUCH_MS * 1000000ULL;

    if (g_object_tooltip.active
        && !g_object_tooltip.term_cell
        && g_object_tooltip.touch == touch
        && g_object_tooltip.map_y == map_y
        && g_object_tooltip.map_x == map_x
        && SDL_strcmp(g_object_tooltip.text, name) == 0)
    {
        if (touch)
            g_object_tooltip.expires_at = expires_at;
        return true;
    }

    g_object_tooltip.active = true;
    g_object_tooltip.touch = touch;
    g_object_tooltip.term_cell = false;
    g_object_tooltip.character_panel_cell = false;
    g_object_tooltip.map_y = map_y;
    g_object_tooltip.map_x = map_x;
    g_object_tooltip.cell_col = 0;
    g_object_tooltip.cell_row = 0;
    g_object_tooltip.cell_cols = 0;
    g_object_tooltip.expires_at = expires_at;
    SDL_strlcpy(g_object_tooltip.text, name, sizeof(g_object_tooltip.text));
    g_state.need_present = true;
    return true;
}

bool sdl_object_tooltip_show_text_at_cell_ex(int col, int row, int cols,
    cptr text, bool touch, bool character_panel_cell)
{
    Uint64 expires_at = 0;

    if (!text || !text[0] || col < 0 || row < 0) {
        sdl_object_tooltip_clear();
        return false;
    }

    if (cols < 1)
        cols = 1;

    if (touch)
        expires_at = SDL_GetTicksNS()
            + (Uint64)SDL_OBJECT_TOOLTIP_TOUCH_MS * 1000000ULL;

    if (g_object_tooltip.active
        && g_object_tooltip.term_cell
        && g_object_tooltip.character_panel_cell == character_panel_cell
        && g_object_tooltip.touch == touch
        && g_object_tooltip.cell_col == col
        && g_object_tooltip.cell_row == row
        && g_object_tooltip.cell_cols == cols
        && SDL_strcmp(g_object_tooltip.text, text) == 0)
    {
        if (touch)
            g_object_tooltip.expires_at = expires_at;
        return true;
    }

    g_object_tooltip.active = true;
    g_object_tooltip.touch = touch;
    g_object_tooltip.term_cell = true;
    g_object_tooltip.character_panel_cell = character_panel_cell;
    g_object_tooltip.map_y = 0;
    g_object_tooltip.map_x = 0;
    g_object_tooltip.cell_col = col;
    g_object_tooltip.cell_row = row;
    g_object_tooltip.cell_cols = cols;
    g_object_tooltip.expires_at = expires_at;
    SDL_strlcpy(g_object_tooltip.text, text, sizeof(g_object_tooltip.text));
    g_state.need_present = true;
    return true;
}

bool sdl_object_tooltip_show_text_at_cell(int col, int row, int cols,
    cptr text, bool touch)
{
    return sdl_object_tooltip_show_text_at_cell_ex(col, row, cols, text,
        touch, false);
}

bool sdl_object_tooltip_show_character_panel_text_at_cell(int col,
    int row, int cols, cptr text, bool touch)
{
    return sdl_object_tooltip_show_text_at_cell_ex(col, row, cols, text,
        touch, true);
}

bool sdl_hover_tooltip_show_text(int col, int row, int cols, cptr text,
    bool touch)
{
    return sdl_object_tooltip_show_text_at_cell(col, row, cols, text, touch);
}

void sdl_hover_tooltip_clear(void)
{
    sdl_object_tooltip_clear();
}

int sdl_object_tooltip_font_px(void)
{
    int font_size = sdl_auto_font_size_from_main(1, 2);
    int font_px = sdl_aux_cell_height_for_font_size(font_size);

    return (font_px > 0) ? font_px : SDL_OBJECT_TOOLTIP_FONT_SIZE;
}

SDL_Surface* sdl_object_tooltip_render_text_surface(TTF_Font* font,
    cptr text, SDL_Color color, float max_text_w)
{
    SDL_Surface* surface;
    int wrap_w;

    if (!font || !text || !text[0])
        return NULL;

    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return NULL;
    if (max_text_w <= 0.0f || (float)surface->w <= max_text_w)
        return surface;

    SDL_DestroySurface(surface);
    wrap_w = (int)(max_text_w + 0.5f);
    if (wrap_w < 1)
        wrap_w = 1;

    return TTF_RenderText_Blended_Wrapped(font, text, 0, color, wrap_w);
}

bool sdl_object_tooltip_pointer_hits_term_cell(float x, float y)
{
    int col = 0;
    int row = 0;

    if (!g_object_tooltip.active || !g_object_tooltip.term_cell)
        return false;
    if (g_object_tooltip.character_panel_cell) {
        if (!sdl_main_view_point_to_cell(x, y, &col, &row)
            || !g_last_main_cell_hit_left_panel)
        {
            return false;
        }
    } else if (!sdl_main_view_point_to_cell(x, y, &col, &row)) {
        return false;
    }
    if (row != g_object_tooltip.cell_row)
        return false;

    return col >= g_object_tooltip.cell_col
        && col < g_object_tooltip.cell_col + g_object_tooltip.cell_cols;
}

void sdl_object_tooltip_handle_mouse_motion(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (g_object_tooltip.active && g_object_tooltip.term_cell) {
        if (sdl_object_tooltip_pointer_hits_term_cell(x, y))
            return;
        sdl_object_tooltip_clear();
    }

    if (g_unified_look_active || g_pointer_aim.active
        || g_player_action_menu.active || g_player_exchange_target.active)
    {
        sdl_object_tooltip_clear();
        return;
    }
    if (!sdl_main_screen_click_shortcuts_active()
        || !sdl_main_view_point_to_map(x, y, &map_y, &map_x))
    {
        sdl_object_tooltip_clear();
        return;
    }

    (void)sdl_object_tooltip_show_grid(map_y, map_x, false);
}

int sdl_object_tooltip_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 remaining;

    if (!g_object_tooltip.active || !g_object_tooltip.touch
        || !g_object_tooltip.expires_at)
    {
        return -1;
    }

    if (now_ns >= g_object_tooltip.expires_at)
        return 0;

    remaining = g_object_tooltip.expires_at - now_ns;
    return (int)((remaining + 999999ULL) / 1000000ULL);
}

bool sdl_object_tooltip_flush_expired(Uint64 now_ns)
{
    if (!g_object_tooltip.active || !g_object_tooltip.touch
        || !g_object_tooltip.expires_at || now_ns < g_object_tooltip.expires_at)
    {
        return false;
    }

    sdl_object_tooltip_clear();
    return true;
}

void sdl_object_tooltip_render(void)
{
    char current[sizeof(g_object_tooltip.text)];
    SDL_FRect cell_rect;
    SDL_Rect screen;
    TTF_Font* font;
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect box;
    SDL_FRect text_dst;
    SDL_Color text_color = g_state.palette[TERM_WHITE];
    float pad;
    float gap;
    float screen_margin;
    float max_box_w;
    float max_text_w;
    int font_px;

    if (!g_object_tooltip.active)
        return;
    if (g_object_tooltip.touch && g_object_tooltip.expires_at
        && SDL_GetTicksNS() >= g_object_tooltip.expires_at)
    {
        sdl_object_tooltip_clear();
        return;
    }
    if (g_object_tooltip.term_cell) {
        if (!g_object_tooltip.text[0]
            || !(g_object_tooltip.character_panel_cell
                ? sdl_left_panel_source_cell_rect(g_object_tooltip.cell_col,
                    g_object_tooltip.cell_row, g_object_tooltip.cell_cols, 1,
                    &cell_rect)
                : sdl_main_cell_rect(g_object_tooltip.cell_col,
                    g_object_tooltip.cell_row, g_object_tooltip.cell_cols, 1,
                    &cell_rect)))
        {
            sdl_object_tooltip_clear();
            return;
        }
    } else {
        if (!sdl_mouse_gameplay_context_active()
            || !sdl_object_tooltip_format_grid(g_object_tooltip.map_y,
                g_object_tooltip.map_x, current, sizeof(current))
            || !sdl_player_map_rect(g_object_tooltip.map_y,
                g_object_tooltip.map_x, &cell_rect))
        {
            sdl_object_tooltip_clear();
            return;
        }
        if (SDL_strcmp(g_object_tooltip.text, current) != 0)
            SDL_strlcpy(g_object_tooltip.text, current,
                sizeof(g_object_tooltip.text));
    }

    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return;

    font_px = sdl_object_tooltip_font_px();
    font = sdl_story_font_for_height(font_px);
    if (!font)
        return;

    pad = sdl_touch_pane_clampf((float)font_px * 0.36f, 7.0f, 14.0f);
    gap = sdl_touch_pane_clampf((float)font_px * 0.28f, 6.0f, 12.0f);
    screen_margin = sdl_touch_pane_clampf(g_state.system_scale * 4.0f,
        4.0f, 10.0f);
    max_box_w = (float)screen.w - screen_margin * 2.0f;
    if (max_box_w <= pad * 2.0f)
        return;

    max_text_w = max_box_w - pad * 2.0f;
    surface = sdl_object_tooltip_render_text_surface(font,
        g_object_tooltip.text, text_color, max_text_w);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }

    box.w = (float)surface->w + pad * 2.0f;
    box.h = (float)surface->h + pad * 2.0f;
    box.x = cell_rect.x + (cell_rect.w - box.w) * 0.5f;
    box.y = cell_rect.y - box.h - gap;

    if (box.y < (float)screen.y + screen_margin)
        box.y = cell_rect.y + cell_rect.h + gap;
    if (box.w + screen_margin * 2.0f <= (float)screen.w) {
        box.x = sdl_touch_pane_clampf(box.x,
            (float)screen.x + screen_margin,
            (float)(screen.x + screen.w) - box.w - screen_margin);
    } else {
        box.x = (float)screen.x + screen_margin;
    }
    if (box.h + screen_margin * 2.0f <= (float)screen.h) {
        box.y = sdl_touch_pane_clampf(box.y,
            (float)screen.y + screen_margin,
            (float)(screen.y + screen.h) - box.h - screen_margin);
    } else {
        box.y = (float)screen.y + screen_margin;
    }

    text_dst = (SDL_FRect){
        .x = box.x + pad,
        .y = box.y + pad,
        .w = (float)surface->w,
        .h = (float)surface->h,
    };

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 210);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 130);
    SDL_RenderRect(g_state.renderer, &box);

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &text_dst);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

int sdl_description_overlay_font_px(void)
{
    int font_px = get_sdl_terminal_menu_scale() * TILE_SIZE;

    if (font_px < 8)
        font_px = 8;

    return font_px;
}

int sdl_description_overlay_max_cols(void)
{
    SDL_Rect anchor;
    int font_px;
    int cell_w;
    int margin;
    int pad_x;
    int max_panel_w;
    int max_cols;

    if (!sdl_overlay_pane_anchor_rect(PANE_DESCRIPTION, &anchor))
        return 80;

    font_px = sdl_description_overlay_font_px();
    cell_w = font_px / 2;
    if (cell_w < 1)
        cell_w = 1;

    margin = sdl_overlay_margin_px();
    pad_x = cell_w;
    max_panel_w = anchor.w - margin * 2;
    if (max_panel_w <= pad_x * 2)
        return 80;

    max_cols = (max_panel_w - pad_x * 2) / cell_w;
    if (max_cols < 20)
        return 20;

    return max_cols;
}

SDL_Color sdl_description_overlay_attr_color(byte attr)
{
    SDL_Color col = {
        angband_color_table[attr][1],
        angband_color_table[attr][2],
        angband_color_table[attr][3],
        255
    };

    return col;
}

cptr sdl_description_overlay_footer_text(
    const description_overlay_state* overlay)
{
    if (overlay && overlay->footer_text[0])
        return overlay->footer_text;

    return "Esc close  Space page  Arrows scroll";
}

void sdl_description_overlay_set_footer(cptr text, bool always)
{
    bool changed;

    g_description_overlay.footer_always = always;

    if (text && text[0])
    {
        changed = !streq(g_description_overlay.footer_text, text);
        SDL_strlcpy(g_description_overlay.footer_text, text,
            sizeof(g_description_overlay.footer_text));
        if (changed)
            g_description_overlay.footer_hover_key = 0;
    }
    else
    {
        g_description_overlay.footer_hover_key = 0;
        g_description_overlay.footer_text[0] = '\0';
    }
}

void sdl_description_overlay_clear_footer_actions(void)
{
    g_description_overlay.footer_action_count = 0;
    for (int i = 0; i < SDL_DESCRIPTION_OVERLAY_MAX_ACTIONS; i++)
    {
        g_description_overlay.footer_actions[i].key = 0;
        g_description_overlay.footer_actions[i].token[0] = '\0';
    }
}

void sdl_description_overlay_add_footer_action(int key, cptr token)
{
    description_overlay_action* action;

    if (!token || !token[0])
        return;
    if (g_description_overlay.footer_action_count
        >= SDL_DESCRIPTION_OVERLAY_MAX_ACTIONS)
    {
        return;
    }

    action =
        &g_description_overlay.footer_actions[
            g_description_overlay.footer_action_count++];
    action->key = key;
    SDL_strlcpy(action->token, token, sizeof(action->token));
}

void sdl_description_overlay_clear_avoid(void)
{
    bool changed = g_description_overlay.avoid_active;

    g_description_overlay.avoid_active = false;
    g_description_overlay.avoid_term_col = 0;
    g_description_overlay.avoid_term_row = 0;
    g_description_overlay.avoid_term_wid = 0;
    g_description_overlay.avoid_term_hgt = 0;

    if (changed && g_description_overlay.active)
    {
        g_state.need_present = true;
    }
}

void sdl_description_overlay_set_avoid_term_rect(int col, int row, int wid,
    int hgt)
{
    bool changed;

    if (col < 0 || row < 0 || wid <= 0 || hgt <= 0)
    {
        sdl_description_overlay_clear_avoid();
        return;
    }

    changed = !g_description_overlay.avoid_active
        || g_description_overlay.avoid_term_col != col
        || g_description_overlay.avoid_term_row != row
        || g_description_overlay.avoid_term_wid != wid
        || g_description_overlay.avoid_term_hgt != hgt;

    g_description_overlay.avoid_active = true;
    g_description_overlay.avoid_term_col = col;
    g_description_overlay.avoid_term_row = row;
    g_description_overlay.avoid_term_wid = wid;
    g_description_overlay.avoid_term_hgt = hgt;

    if (changed && g_description_overlay.active)
    {
        g_state.need_present = true;
    }
}

bool sdl_description_overlay_token_matches_hover(
    const description_overlay_state* overlay, cptr text, int col)
{
    if (!overlay || !text || overlay->footer_hover_key == 0)
        return false;

    for (int i = 0; i < overlay->footer_action_count; i++)
    {
        const description_overlay_action* action = &overlay->footer_actions[i];
        cptr match;
        int start;
        int end;

        if (action->key != overlay->footer_hover_key || !action->token[0])
            continue;

        match = strstr(text, action->token);
        if (!match)
            continue;

        start = (int)(match - text);
        end = start + (int)strlen(action->token);
        if (col >= start && col < end)
            return true;
    }

    return false;
}

bool sdl_description_overlay_avoid_rect(SDL_FRect* out)
{
    const description_overlay_state* overlay = &g_description_overlay;

    if (!out)
        return false;
    *out = (SDL_FRect){ 0 };
    if (!overlay->avoid_active)
        return false;

    return sdl_main_cell_rect(overlay->avoid_term_col,
        overlay->avoid_term_row, overlay->avoid_term_wid,
        overlay->avoid_term_hgt, out);
}

bool sdl_description_overlay_rects_intersect(
    const SDL_FRect* a, const SDL_FRect* b)
{
    return a && b && a->x < b->x + b->w && a->x + a->w > b->x
        && a->y < b->y + b->h && a->y + a->h > b->y;
}

int sdl_description_overlay_rows_for_panel_space(float available_h,
    int pad_y, int cell_h, bool footer)
{
    int rows;

    if (available_h <= (float)(pad_y * 2) || cell_h <= 0)
        return 0;

    rows = ((int)available_h - pad_y * 2) / cell_h;
    if (footer)
        rows--;
    if (rows < 0)
        rows = 0;

    return rows;
}

bool sdl_description_overlay_fit_around_avoid(
    const SDL_Rect* anchor, int margin, int pad_y, int cell_h, bool footer,
    float panel_x, float panel_w, int* visible_rows, float* panel_h,
    float* panel_y)
{
    SDL_FRect avoid;
    SDL_FRect panel;

    if (!anchor || !visible_rows || !panel_h || !panel_y)
        return false;
    if (g_description_overlay.interactive
        || !g_description_overlay.avoid_active)
    {
        return true;
    }

    panel = (SDL_FRect){
        .x = panel_x,
        .y = *panel_y,
        .w = panel_w,
        .h = *panel_h,
    };

    if (!sdl_description_overlay_avoid_rect(&avoid)
        || !sdl_description_overlay_rects_intersect(&panel, &avoid))
    {
        return true;
    }

    {
        float min_y = (float)(anchor->y + margin);
        float max_y = (float)(anchor->y + anchor->h - margin);
        float gap = (float)(cell_h / 4);
        float above_h;
        float below_h;
        int above_rows;
        int below_rows;
        int side_rows;
        bool place_below;

        if (gap < 2.0f)
            gap = 2.0f;

        above_h = avoid.y - gap - min_y;
        below_h = max_y - (avoid.y + avoid.h + gap);
        above_rows = sdl_description_overlay_rows_for_panel_space(
            above_h, pad_y, cell_h, footer);
        below_rows = sdl_description_overlay_rows_for_panel_space(
            below_h, pad_y, cell_h, footer);

        if (below_rows >= *visible_rows && above_rows < *visible_rows)
            place_below = true;
        else if (above_rows >= *visible_rows && below_rows < *visible_rows)
            place_below = false;
        else
            place_below = (below_rows >= above_rows);

        side_rows = place_below ? below_rows : above_rows;
        if (side_rows < 1)
            return false;

        if (*visible_rows > side_rows)
            *visible_rows = side_rows;
        *panel_h = (float)((*visible_rows + (footer ? 1 : 0)) * cell_h
            + pad_y * 2);
        *panel_y = place_below ? (avoid.y + avoid.h + gap)
                               : (avoid.y - gap - *panel_h);
    }

    return true;
}

bool sdl_description_overlay_layout(description_overlay_layout* out)
{
    SDL_Rect anchor;
    const description_overlay_state* overlay = &g_description_overlay;
    cptr footer_text = sdl_description_overlay_footer_text(overlay);
    int font_px;
    int cell_w;
    int cell_h;
    int margin;
    int pad_x;
    int pad_y;
    int max_panel_w;
    int max_panel_h;
    int max_cols;
    int max_rows_no_footer;
    int max_rows;
    int visible_cols;
    int visible_rows;
    int footer_cols = 0;
    bool footer;
    bool footer_forced;
    float panel_w;
    float panel_h;
    float panel_x;
    float panel_y;

    if (out)
        *out = (description_overlay_layout){ 0 };
    if (!out || !overlay->active || !overlay->attrs || !overlay->chars
        || overlay->width <= 0 || overlay->height <= 0)
    {
        return false;
    }

    if (!sdl_overlay_pane_anchor_rect(PANE_DESCRIPTION, &anchor))
        return false;

    font_px = sdl_description_overlay_font_px();
    cell_h = font_px;
    cell_w = font_px / 2;
    if (cell_w < 1)
        cell_w = 1;

    margin = sdl_overlay_margin_px();
    pad_x = cell_w;
    pad_y = cell_h / 2;
    if (pad_y < 2)
        pad_y = 2;

    max_panel_w = anchor.w - margin * 2;
    max_panel_h = anchor.h - margin * 2;
    if (max_panel_w <= pad_x * 2 || max_panel_h <= pad_y * 2 + cell_h)
        return false;

    max_cols = (max_panel_w - pad_x * 2) / cell_w;
    max_rows_no_footer = (max_panel_h - pad_y * 2) / cell_h;
    if (max_cols < 1 || max_rows_no_footer < 1)
        return false;

    footer_forced = overlay->interactive
        && (overlay->footer_always || overlay->footer_text[0]);
    footer = footer_forced
        || (overlay->interactive && overlay->height > max_rows_no_footer);

    for (int pass = 0; pass < 2; pass++)
    {
        max_rows = max_rows_no_footer - (footer ? 1 : 0);
        if (max_rows < 1)
            max_rows = 1;

        visible_rows = overlay->height;
        if (visible_rows > max_rows)
            visible_rows = max_rows;

        footer_cols = footer ? (int)strlen(footer_text) : 0;
        visible_cols = overlay->width;
        if (visible_cols < footer_cols)
            visible_cols = footer_cols;
        if (visible_cols > max_cols)
            visible_cols = max_cols;
        if (visible_cols < 1)
            visible_cols = 1;

        panel_w = (float)(visible_cols * cell_w + pad_x * 2);
        panel_h = (float)((visible_rows + (footer ? 1 : 0)) * cell_h
            + pad_y * 2);
        panel_x = (float)anchor.x + ((float)anchor.w - panel_w) * 0.5f;
        panel_y = (float)anchor.y + ((float)anchor.h - panel_h) * 0.5f;

        if (!sdl_description_overlay_fit_around_avoid(&anchor, margin,
                pad_y, cell_h, footer, panel_x, panel_w, &visible_rows,
                &panel_h, &panel_y))
        {
            return false;
        }

        if (!footer && visible_rows < overlay->height)
        {
            footer = true;
            continue;
        }

        break;
    }

    out->cell_w = cell_w;
    out->cell_h = cell_h;
    out->visible_cols = visible_cols;
    out->visible_rows = visible_rows;
    out->footer = footer;
    out->max_scroll = MAX(0, overlay->height - visible_rows);
    out->scroll = overlay->scroll;
    if (out->scroll < 0)
        out->scroll = 0;
    if (out->scroll > out->max_scroll)
        out->scroll = out->max_scroll;

    out->panel = (SDL_FRect){
        .x = panel_x,
        .y = panel_y,
        .w = panel_w,
        .h = panel_h,
    };
    out->text_x = out->panel.x + (float)pad_x;
    out->text_y = out->panel.y + (float)pad_y;
    out->footer_y = out->text_y + (float)(visible_rows * cell_h);

    return true;
}

bool sdl_description_overlay_scroll_to_layout(
    const description_overlay_layout* layout, int scroll)
{
    int clamped;

    if (!layout || !g_description_overlay.active || layout->max_scroll <= 0)
        return false;

    clamped = scroll;
    if (clamped < 0)
        clamped = 0;
    if (clamped > layout->max_scroll)
        clamped = layout->max_scroll;

    if (clamped == g_description_overlay.scroll)
        return true;

    g_description_overlay.scroll = clamped;
    g_state.need_present = true;
    sdl_present_if_needed(&g_views[PANE_MAIN]);
    return true;
}

bool sdl_description_overlay_scroll_by(int rows)
{
    description_overlay_layout layout;

    if (rows == 0)
        return false;
    if (!sdl_description_overlay_layout(&layout))
        return false;

    return sdl_description_overlay_scroll_to_layout(&layout,
        layout.scroll + rows);
}

bool sdl_description_overlay_scroll_page(int direction)
{
    description_overlay_layout layout;
    int rows;

    if (direction == 0)
        return false;
    if (!sdl_description_overlay_layout(&layout))
        return false;

    rows = layout.visible_rows;
    if (rows < 1)
        rows = 1;
    return sdl_description_overlay_scroll_to_layout(&layout,
        layout.scroll + ((direction > 0) ? rows : -rows));
}

bool sdl_description_overlay_handle_mouse_wheel(
    const SDL_MouseWheelEvent* wheel)
{
    static sdl_wheel_step_state wheel_state;
    description_overlay_layout layout;
    int steps;

    if (!wheel || wheel->which == SDL_TOUCH_MOUSEID)
        return false;
    if (!sdl_description_overlay_layout(&layout) || layout.max_scroll <= 0)
        return false;
    if (g_description_overlay.interactive
        && (wheel->mouse_x < layout.panel.x
            || wheel->mouse_x >= layout.panel.x + layout.panel.w
            || wheel->mouse_y < layout.panel.y
            || wheel->mouse_y >= layout.panel.y + layout.panel.h))
    {
        return false;
    }

    steps = sdl_wheel_step_state_consume_primary_axis(&wheel_state, wheel);
    if (steps == 0)
        return true;

    return sdl_description_overlay_scroll_to_layout(&layout,
        layout.scroll - steps);
}

void sdl_description_overlay_render_char(SDL_Texture* atlas,
    int atlas_cell_w, int atlas_cell_h, float cell_w, float cell_h,
    float x, float y, byte attr, char ch)
{
    unsigned char uch = (unsigned char)ch;
    SDL_FRect src;
    SDL_FRect dst;
    SDL_Color col;

    if (!atlas || uch < 32 || uch == 127)
        return;

    col = sdl_description_overlay_attr_color(attr);
    SDL_SetTextureColorMod(atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(atlas, 255);

    src = (SDL_FRect){
        .x = (float)((uch & 15) * atlas_cell_w),
        .y = (float)((uch >> 4) * atlas_cell_h),
        .w = (float)atlas_cell_w,
        .h = (float)atlas_cell_h,
    };
    dst = (SDL_FRect){
        .x = x,
        .y = y,
        .w = cell_w,
        .h = cell_h,
    };

    SDL_RenderTexture(g_state.renderer, atlas, &src, &dst);
}

void sdl_description_overlay_render_text(SDL_Texture* atlas,
    int atlas_cell_w, int atlas_cell_h, const char* text, float x, float y,
    float cell_w, float cell_h, byte attr)
{
    if (!text)
        return;

    for (int i = 0; text[i]; i++)
    {
        sdl_description_overlay_render_char(atlas, atlas_cell_w,
            atlas_cell_h, cell_w, cell_h, x + (float)i * cell_w, y, attr,
            text[i]);
    }
}

void sdl_description_overlay_render(void)
{
    description_overlay_layout layout;
    const description_overlay_state* overlay = &g_description_overlay;
    const char* font_path = config.monospace_font[0] != '\0'
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";
    SDL_Texture* atlas;
    int atlas_cell_w = 0;
    int atlas_cell_h = 0;
    bool cached = false;
    bool exact = false;
    SDL_Rect clip;

    if (!sdl_description_overlay_layout(&layout))
        return;

    atlas = sdl_acquire_mono_font_atlas_cells_ex(font_path, layout.cell_w,
        layout.cell_h, &cached, &atlas_cell_w, &atlas_cell_h, &exact, true);
    if (!atlas)
        return;

    (void)cached;
    (void)exact;

    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
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

    for (int row = 0; row < layout.visible_rows; row++)
    {
        int src_row = row + layout.scroll;

        if (src_row >= overlay->height)
            break;

        for (int col = 0; col < layout.visible_cols; col++)
        {
            int idx;

            if (col >= overlay->width)
                continue;

            idx = src_row * overlay->width + col;
            sdl_description_overlay_render_char(atlas, atlas_cell_w,
                atlas_cell_h, (float)layout.cell_w, (float)layout.cell_h,
                layout.text_x + (float)col * (float)layout.cell_w,
                layout.text_y + (float)row * (float)layout.cell_h,
                overlay->attrs[idx], overlay->chars[idx]);
        }
    }

    if (layout.footer)
    {
        char scroll_buf[32];
        cptr footer_text = sdl_description_overlay_footer_text(overlay);

        if (overlay->interactive)
        {
            for (int col = 0; footer_text[col] && col < layout.visible_cols;
                 col++)
            {
                byte attr = sdl_description_overlay_token_matches_hover(
                    overlay, footer_text, col)
                    ? TERM_L_BLUE
                    : TERM_SLATE;

                sdl_description_overlay_render_char(atlas, atlas_cell_w,
                    atlas_cell_h, (float)layout.cell_w,
                    (float)layout.cell_h,
                    layout.text_x + (float)col * (float)layout.cell_w,
                    layout.footer_y, attr, footer_text[col]);
            }
        }
        if (layout.max_scroll > 0)
        {
            strnfmt(scroll_buf, sizeof(scroll_buf), "%d/%d",
                layout.scroll + 1, overlay->height);
            int scroll_col = layout.visible_cols - (int)strlen(scroll_buf);
            if (scroll_col < 0)
                scroll_col = 0;
            sdl_description_overlay_render_text(atlas, atlas_cell_w,
                atlas_cell_h, scroll_buf,
                layout.text_x + (float)scroll_col * (float)layout.cell_w,
                layout.footer_y, (float)layout.cell_w, (float)layout.cell_h,
                TERM_SLATE);
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);

    if (!cached)
        SDL_DestroyTexture(atlas);
}

int sdl_description_overlay_footer_action_at(float x, float y)
{
    description_overlay_layout layout;
    const description_overlay_state* overlay = &g_description_overlay;
    cptr footer_text;
    int col;

    if (!overlay->active || !overlay->interactive
        || overlay->footer_action_count <= 0)
    {
        return 0;
    }

    if (!sdl_description_overlay_layout(&layout) || !layout.footer)
        return 0;

    if (x < layout.text_x || y < layout.footer_y
        || y >= layout.footer_y + (float)layout.cell_h)
    {
        return 0;
    }

    col = (int)((x - layout.text_x) / (float)layout.cell_w);
    if (col < 0 || col >= layout.visible_cols)
        return 0;

    footer_text = sdl_description_overlay_footer_text(overlay);
    for (int i = 0; i < overlay->footer_action_count; i++)
    {
        const description_overlay_action* action = &overlay->footer_actions[i];
        cptr match;
        int start;
        int end;

        if (!action->token[0])
            continue;

        match = strstr(footer_text, action->token);
        if (!match)
            continue;

        start = (int)(match - footer_text);
        end = start + (int)strlen(action->token);
        if (col >= start && col < end)
            return action->key;
    }

    return 0;
}

bool sdl_description_overlay_handle_footer_hover(float x, float y)
{
    int key = sdl_description_overlay_footer_action_at(x, y);

    if (!g_description_overlay.active || !g_description_overlay.interactive
        || g_description_overlay.footer_action_count <= 0)
    {
        return false;
    }

    if (key == 0)
    {
        if (g_description_overlay.footer_hover_key != 0)
        {
            g_description_overlay.footer_hover_key = 0;
            g_state.need_present = true;
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
        return false;
    }

    if (g_description_overlay.footer_hover_key != key)
    {
        g_description_overlay.footer_hover_key = key;
        g_state.need_present = true;
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    }

    return true;
}

bool sdl_description_overlay_handle_footer_pointer(float x, float y)
{
    int key = sdl_description_overlay_footer_action_at(x, y);

    if (key == 0)
        return false;

    g_description_overlay.footer_hover_key = key;
    g_state.need_present = true;
    Term_keypress(key);
    return true;
}

bool sdl_unified_look_handle_map_describe_pointer(float x, float y)
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
    if (!sdl_main_view_point_to_look_map(x, y, &map_y, &map_x))
        return false;
    if (!sdl_mouse_grid_has_visible_monster(map_y, map_x, NULL)
        && !sdl_mouse_grid_has_marked_object(map_y, map_x, NULL))
    {
        return false;
    }

    g_unified_look_map_describe_pending = true;
    g_unified_look_map_describe_y = map_y;
    g_unified_look_map_describe_x = map_x;
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_unified_look_handle_map_target_pointer(float x, float y)
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
    if (!sdl_main_view_point_to_look_map(x, y, &map_y, &map_x))
        return false;
    if (!sdl_mouse_grid_has_visible_monster(map_y, map_x, NULL))
        return false;

    g_unified_look_map_target_pending = true;
    g_unified_look_map_target_y = map_y;
    g_unified_look_map_target_x = map_x;
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_mouse_grid_has_marked_searched_skeleton(int y, int x,
    object_type** out_obj)
{
    object_type* o_ptr;

    if (!in_bounds(y, x) || !grid_info_is_available(y, x))
        return false;
    if (!(cave_floorlike_bold(y, x) || cave_feat[y][x] == FEAT_SUNLIGHT))
        return false;

    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        if (!o_ptr->marked)
            continue;
        if (!object_is_searched_skeleton(o_ptr))
            continue;

        if (out_obj)
            *out_obj = o_ptr;
        return true;
    }

    return false;
}

bool sdl_mouse_grid_has_recallable_content(int y, int x)
{
    object_type* o_ptr = NULL;

    return sdl_mouse_grid_has_visible_monster(y, x, NULL)
        || sdl_mouse_grid_has_marked_searched_skeleton(y, x, &o_ptr)
        || sdl_mouse_grid_has_marked_object(y, x, &o_ptr);
}

bool sdl_mouse_grid_has_describable_content(int y, int x)
{
    return sdl_mouse_grid_has_recallable_content(y, x)
        || sdl_object_tooltip_format_grid(y, x, NULL, 0);
}

void sdl_mouse_recall_object(object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return;

    if (o_ptr->tval == TV_NOTE) {
        note_info_screen(o_ptr);
        return;
    }

    (void)player_try_identify_smithing_object_on_examine(o_ptr, false);

    if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL) {
        int slot = wield_slot(o_ptr);
        const object_type* compare_objects[2];
        const char* compare_headings[2];
        char selected_heading[32];
        char equipped_heading[32];

        strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
        strnfmt(equipped_heading, sizeof(equipped_heading), "%s", mention_use(slot));

        compare_objects[0] = o_ptr;
        compare_headings[0] = selected_heading;
        compare_objects[1] = inventory[slot].k_idx ? &inventory[slot] : NULL;
        compare_headings[1] = equipped_heading;

        object_info_screen_multi(compare_objects, compare_headings, 2);
    } else {
        object_info_screen(o_ptr);
    }
}

bool sdl_mouse_recall_handle_right_click(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_main_view_point_to_look_map(x, y, &map_y, &map_x))
        return false;

    g_mouse_path.recall_pending = true;
    g_mouse_path.recall_y = map_y;
    g_mouse_path.recall_x = map_x;
    sdl_mouse_path_cancel();
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_mouse_recall_handle_right_click_if_available(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_main_view_point_to_look_map(x, y, &map_y, &map_x))
        return false;
    if (!sdl_mouse_grid_has_describable_content(map_y, map_x))
        return false;

    g_mouse_path.recall_pending = true;
    g_mouse_path.recall_y = map_y;
    g_mouse_path.recall_x = map_x;
    sdl_mouse_path_cancel();
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

void sdl_map_touch_cancel_press(void)
{
    g_map_touch_press.active = false;
    g_map_touch_press.repeat_target = false;
    g_map_touch_press.finger_id = 0;
    g_map_touch_press.map_y = 0;
    g_map_touch_press.map_x = 0;
    g_map_touch_press.start_x = 0.0f;
    g_map_touch_press.start_y = 0.0f;
    g_map_touch_press.start_time = 0;
}

bool sdl_map_touch_is_same_selected_target(int map_y, int map_x)
{
    return g_map_touch_selected
        && g_mouse_path.hover_visible
        && g_mouse_path.path_valid
        && !g_mouse_path.follow_active
        && g_map_touch_selected_y == map_y
        && g_map_touch_selected_x == map_x
        && g_mouse_path.target_y == map_y
        && g_mouse_path.target_x == map_x;
}

bool sdl_map_touch_handle_pointer_down(float x, float y, SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;
    bool feature_action_target;
    bool tooltip_target;

    if (sdl_touch_round_layer_controls_active()
        && !sdl_touch_round_point_excluded(x, y))
    {
        return false;
    }
    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x))
        return false;
    feature_action_target = sdl_mouse_feature_action_for_grid(map_y, map_x,
        NULL, NULL);
    tooltip_target = sdl_object_tooltip_format_grid(map_y, map_x, NULL, 0);
    if (config.touch_movement_mode == SDL_TOUCH_MOVEMENT_OFF
        && (map_y != p_ptr->py || map_x != p_ptr->px)
        && !feature_action_target
        && !tooltip_target)
    {
        return false;
    }

    if (!tooltip_target)
        sdl_object_tooltip_clear();

    if (map_y == p_ptr->py && map_x == p_ptr->px)
        sdl_mouse_path_cancel();

    sdl_map_touch_cancel_press();
    g_map_touch_press.active = true;
    g_map_touch_press.repeat_target =
        sdl_map_touch_is_same_selected_target(map_y, map_x);
    g_map_touch_press.finger_id = finger_id;
    g_map_touch_press.map_y = map_y;
    g_map_touch_press.map_x = map_x;
    g_map_touch_press.start_x = x;
    g_map_touch_press.start_y = y;
    g_map_touch_press.start_time = SDL_GetTicksNS();

    if ((map_y != p_ptr->py || map_x != p_ptr->px)
        && config.touch_movement_mode != SDL_TOUCH_MOVEMENT_OFF)
    {
        (void)sdl_mouse_path_select_grid(map_y, map_x, false);
    }
    return true;
}

bool sdl_map_touch_handle_pointer_motion(float x, float y, SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;
    float dx;
    float dy;

    if (!g_map_touch_press.active || g_map_touch_press.finger_id != finger_id)
        return false;

    dx = x - g_map_touch_press.start_x;
    dy = y - g_map_touch_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        || dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        float start_x = g_map_touch_press.start_x;
        float start_y = g_map_touch_press.start_y;

        sdl_mouse_path_cancel();
        if (sdl_touch_swipe_handle_pointer_down(start_x, start_y, finger_id))
            (void)sdl_touch_swipe_handle_pointer_motion(x, y, finger_id);
        return true;
    }

    return true;
}

bool sdl_map_touch_handle_pointer_up(float x, float y, SDL_FingerID finger_id)
{
    int map_y = 0;
    int map_x = 0;
    bool repeat_target;
    Uint64 now_ns;
    float dx;
    float dy;

    if (!g_map_touch_press.active || g_map_touch_press.finger_id != finger_id)
        return false;

    dx = x - g_map_touch_press.start_x;
    dy = y - g_map_touch_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;
    if (dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        sdl_mouse_path_cancel();
        return true;
    }

    now_ns = SDL_GetTicksNS();
    if (now_ns - g_map_touch_press.start_time
        >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        (void)sdl_map_touch_flush_pending_press(now_ns);
        return true;
    }

    repeat_target = g_map_touch_press.repeat_target;
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        || map_y != g_map_touch_press.map_y
        || map_x != g_map_touch_press.map_x)
    {
        sdl_mouse_path_cancel();
        return true;
    }

    sdl_map_touch_cancel_press();
    if (map_y == p_ptr->py && map_x == p_ptr->px)
    {
        sdl_object_tooltip_clear();
        sdl_player_confirm_at_player();
        return true;
    }

    (void)sdl_object_tooltip_show_grid(map_y, map_x, true);

    if (config.touch_movement_mode == SDL_TOUCH_MOVEMENT_OFF)
        return true;

    if (config.touch_movement_mode == SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY)
        return true;

    if (repeat_target) {
        (void)sdl_mouse_path_start_follow_grid(map_y, map_x);
        return true;
    }

    if (sdl_mouse_path_select_grid(map_y, map_x, true)) {
        g_map_touch_selected = true;
        g_map_touch_selected_y = map_y;
        g_map_touch_selected_x = map_x;
    } else {
        g_map_touch_selected = false;
    }

    return true;
}

int sdl_map_touch_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_map_touch_press.active)
        return -1;

    elapsed = now_ns - g_map_touch_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_map_touch_flush_pending_press(Uint64 now_ns)
{
    float x;
    float y;
    int map_y;
    int map_x;

    if (!g_map_touch_press.active)
        return false;
    if (now_ns - g_map_touch_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    x = g_map_touch_press.start_x;
    y = g_map_touch_press.start_y;
    map_y = g_map_touch_press.map_y;
    map_x = g_map_touch_press.map_x;
    sdl_map_touch_cancel_press();
    g_map_touch_selected = false;

    if (map_y == p_ptr->py && map_x == p_ptr->px)
        return sdl_player_action_menu_open();

    if (sdl_mouse_feature_action_queue_grid(map_y, map_x))
        return true;

    if (sdl_mouse_grid_has_describable_content(map_y, map_x))
    {
        if (!sdl_mouse_recall_handle_right_click(x, y))
            sdl_mouse_path_cancel();
        return true;
    }

    if (config.touch_movement_mode == SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY)
        return sdl_mouse_path_start_follow_grid(map_y, map_x);

    if (!sdl_mouse_recall_handle_right_click(x, y))
        sdl_mouse_path_cancel();

    return true;
}

bool sdl_mouse_recall_process_pending(void)
{
    int y;
    int x;
    int m_idx = 0;
    object_type* o_ptr = NULL;

    if (!g_mouse_path.recall_pending)
        return false;

    y = g_mouse_path.recall_y;
    x = g_mouse_path.recall_x;
    g_mouse_path.recall_pending = false;
    (void)sdl_mouse_consume_wake_key();

    if (!character_generated || !character_dungeon || !p_ptr
        || p_ptr->is_dead || death_spectator_active() || character_icky != 0
        || ui_menu_click_is_active())
    {
        return true;
    }
    if (!in_bounds(y, x))
        return true;

    if (sdl_mouse_grid_has_visible_monster(y, x, &m_idx)) {
        monster_type* m_ptr = &mon_list[m_idx];

        monster_race_track(m_ptr->r_idx);
        health_track(m_idx);
        handle_stuff();

        screen_save();
        if (!screen_roff(m_ptr->r_idx, m_ptr))
            (void)inkey();
        screen_load();
        return true;
    }

    if (sdl_mouse_grid_has_marked_searched_skeleton(y, x, &o_ptr)) {
        char tip[160];

        if (hint_messages_short_tip_for_source(y, x, tip, sizeof(tip))) {
            do_cmd_redraw();
            sdl_minimap_focus(y, x);
            do_cmd_view_map();
            return true;
        }
    }

    if (sdl_mouse_grid_has_marked_object(y, x, &o_ptr)) {
        sdl_mouse_recall_object(o_ptr);
        return true;
    }

    {
        char desc[sizeof(g_object_tooltip.text)];

        if (sdl_object_tooltip_format_grid(y, x, desc, sizeof(desc))) {
            msg_format("You see %s.", desc);
            return true;
        }
    }

    bell("Nothing to recall.");
    return true;
}

void sdl_mouse_path_render(void)
{
    int cell_cols = use_bigtile ? 2 : 1;
    SDL_Color path_color = g_state.palette[TERM_L_BLUE];
    bool blocked_target;
    SDL_Color target_color;

    if (!sdl_mouse_gameplay_context_active())
        return;
    if (!g_mouse_path.follow_active && !g_mouse_path.hover_visible)
        return;

    if (!g_mouse_path.path_valid
        || g_mouse_path.source_y != p_ptr->py
        || g_mouse_path.source_x != p_ptr->px)
    {
        if (!sdl_mouse_path_compute(g_mouse_path.target_y, g_mouse_path.target_x))
            return;
    }

    blocked_target = g_mouse_path.blocked_target_kind
        != SDL_MOUSE_PATH_BLOCKED_NONE;
    target_color = g_state.palette[
        sdl_mouse_monster_is_friendly(g_mouse_path.target_m_idx) ? TERM_L_GREEN
        : (g_mouse_path.target_m_idx > 0 || blocked_target)      ? TERM_L_RED
                                                                 : TERM_YELLOW];

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < g_mouse_path.path_len; i++) {
        int y = GRID_Y(g_mouse_path.path[i]);
        int x = GRID_X(g_mouse_path.path[i]);
        int term_row;
        int term_col;
        SDL_FRect rect;
        SDL_Color color = (!blocked_target && i == g_mouse_path.path_len - 1)
            ? target_color
            : path_color;

        if (!panel_contains(y, x))
            continue;

        term_row = ROW_MAP + (y - p_ptr->wy);
        term_col = COL_MAP + (x - p_ptr->wx) * cell_cols;
        if (!sdl_main_cell_rect(term_col, term_row, cell_cols, 1, &rect))
            continue;

        SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
            (i == g_mouse_path.path_len - 1) ? 92 : 56);
        SDL_RenderFillRect(g_state.renderer, &rect);

        SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, 190);
        SDL_RenderRect(g_state.renderer, &rect);
    }

    if (blocked_target && panel_contains(g_mouse_path.target_y,
            g_mouse_path.target_x))
    {
        int term_row = ROW_MAP + (g_mouse_path.target_y - p_ptr->wy);
        int term_col = COL_MAP + (g_mouse_path.target_x - p_ptr->wx)
            * cell_cols;
        SDL_FRect rect;

        if (!sdl_main_cell_rect(term_col, term_row, cell_cols, 1, &rect))
            return;

        SDL_SetRenderDrawColor(g_state.renderer, target_color.r,
            target_color.g, target_color.b, 92);
        SDL_RenderFillRect(g_state.renderer, &rect);

        SDL_SetRenderDrawColor(g_state.renderer, target_color.r,
            target_color.g, target_color.b, 190);
        SDL_RenderRect(g_state.renderer, &rect);
    }
}


