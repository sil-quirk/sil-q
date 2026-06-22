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
    if (sdl_grid_question_take_command(command, dir))
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

    Uint64 _tsc0 = SDL_GetTicksNS();
    bool _tsc_ok = sdl_mouse_path_compute(target_y, target_x);
    {
        Uint64 _tsc_ms = (SDL_GetTicksNS() - _tsc0) / 1000000ULL;
        if (_tsc_ms >= 150)
            log_warn("[INPUTLAG] take_step compute %llu ms (path_len=%d)",
                (unsigned long long)_tsc_ms, g_mouse_path.path_len);
    }
    if (!_tsc_ok) {
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
                && get_check_near(target_y, target_x,
                    "Stuck door, do you want to bash it? "))
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

/*
 * When set, a touch tooltip shown while the flag is active does not auto-expire
 * after SDL_OBJECT_TOOLTIP_TOUCH_MS; it stays up until the next press, matching
 * the persistent right-click popup on the mouse.  Scoped tightly around the
 * long-press popup via sdl_object_tooltip_begin/end_persistent.
 */
static bool g_object_tooltip_persistent_scope = false;

void sdl_object_tooltip_begin_persistent(void)
{
    g_object_tooltip_persistent_scope = true;
}

void sdl_object_tooltip_end_persistent(void)
{
    g_object_tooltip_persistent_scope = false;
}

/* Expiry time for a freshly shown tooltip: touch tooltips auto-expire unless
 * shown inside a persistent scope; mouse tooltips never auto-expire. */
static Uint64 sdl_object_tooltip_expiry(bool touch)
{
    if (touch && !g_object_tooltip_persistent_scope)
        return SDL_GetTicksNS()
            + (Uint64)SDL_OBJECT_TOOLTIP_TOUCH_MS * 1000000ULL;

    return 0;
}

void sdl_object_tooltip_clear(void)
{
    if (!g_object_tooltip.active)
        return;

    g_object_tooltip.active = false;
    g_object_tooltip.touch = false;
    g_object_tooltip.term_cell = false;
    g_object_tooltip.character_panel_cell = false;
    g_object_tooltip.screen_rect = false;
    g_object_tooltip.map_y = 0;
    g_object_tooltip.map_x = 0;
    g_object_tooltip.cell_col = 0;
    g_object_tooltip.cell_row = 0;
    g_object_tooltip.cell_cols = 0;
    g_object_tooltip.rect = (SDL_FRect){ 0 };
    g_object_tooltip.expires_at = 0;
    g_object_tooltip.persistent = false;
    g_object_tooltip.text[0] = '\0';
    g_state.need_present = true;
}

/*
 * Dismiss a persistent touch popup on the next press, the way moving the mouse
 * dismisses the hovered right-click popup.  Returns true if one was cleared.
 */
bool sdl_object_tooltip_dismiss_persistent_on_press(void)
{
    if (!g_object_tooltip.active || !g_object_tooltip.persistent)
        return false;

    sdl_object_tooltip_clear();
    return true;
}

/* Append text to buf, recording the attr of each appended byte in the
 * parallel attrs array (when provided; it must hold at least buflen
 * entries). */
static void sdl_object_tooltip_append_text(char* buf, size_t buflen,
    byte* attrs, cptr text, byte attr)
{
    size_t old_len;
    size_t new_len;

    if (!buf || !buflen || !text || !text[0])
        return;

    old_len = strlen(buf);
    SDL_strlcat(buf, text, buflen);

    if (!attrs)
        return;
    new_len = strlen(buf);
    for (size_t i = old_len; i < new_len; i++)
        attrs[i] = attr;
}

void sdl_object_tooltip_append_part(char* buf, size_t buflen,
    byte* attrs, cptr text, byte attr)
{
    if (!buf || !buflen || !text || !text[0])
        return;

    if (buf[0])
        sdl_object_tooltip_append_text(buf, buflen, attrs, "; ", TERM_WHITE);
    sdl_object_tooltip_append_text(buf, buflen, attrs, text, attr);
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

    /* A trap the player has rewired is shown as such. */
    if ((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL)
        && cave_rewired[y][x])
    {
        static char rewired_buf[80];
        strnfmt(rewired_buf, sizeof(rewired_buf), "%s (rewired)", name);
        name = rewired_buf;
    }

    if (out_name)
        *out_name = name;
    return true;
}

void sdl_object_tooltip_monster_morale_text(monster_type* m_ptr,
    char* out, size_t out_len, byte* out_attr)
{
    int color = TERM_WHITE;
    int morale_num;

    if (out_attr)
        *out_attr = TERM_WHITE;
    if (!out || !out_len)
        return;

    out[0] = '\0';
    if (!m_ptr)
        return;

    if (get_alertness_text(m_ptr, (int)out_len, out, &color)) {
        if (out_attr)
            *out_attr = (byte)color;
        return;
    }

    morale_num = (m_ptr->morale >= 0) ? ((m_ptr->morale + 9) / 10)
                                      : (m_ptr->morale / 10);
    strnfmt(out, out_len, "%d", morale_num);
}

bool sdl_object_tooltip_format_grid(int y, int x, char* out,
    size_t out_len, byte* attrs)
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
        byte morale_attr = TERM_WHITE;

        monster_desc(m_name, sizeof(m_name), m_ptr, 0x08);
        sdl_object_tooltip_monster_morale_text(m_ptr, morale_text,
            sizeof(morale_text), &morale_attr);
        sdl_object_tooltip_append_part(buf, buflen, attrs, m_name,
            TERM_WHITE);
        if (m_ptr->maxhp > 0) {
            monster_health_bar_text(m_ptr, hp_bar, sizeof(hp_bar), 8);
            sdl_object_tooltip_append_text(buf, buflen, attrs, " [HP: ",
                TERM_WHITE);
            sdl_object_tooltip_append_text(buf, buflen, attrs,
                hp_bar[0] ? hp_bar : "-",
                health_attr(m_ptr->hp, m_ptr->maxhp));
            if (morale_text[0]) {
                sdl_object_tooltip_append_text(buf, buflen, attrs,
                    ", Morale: ", TERM_WHITE);
                sdl_object_tooltip_append_text(buf, buflen, attrs,
                    morale_text, morale_attr);
            }
            sdl_object_tooltip_append_text(buf, buflen, attrs, "]",
                TERM_WHITE);
        } else if (morale_text[0]) {
            sdl_object_tooltip_append_text(buf, buflen, attrs, " [Morale: ",
                TERM_WHITE);
            sdl_object_tooltip_append_text(buf, buflen, attrs, morale_text,
                morale_attr);
            sdl_object_tooltip_append_text(buf, buflen, attrs, "]",
                TERM_WHITE);
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
            sdl_object_tooltip_append_part(buf, buflen, attrs, o_name,
                TERM_WHITE);
        }

        if (object_count > 2) {
            char more[32];

            strnfmt(more, sizeof(more), "+%d more", object_count - 2);
            sdl_object_tooltip_append_part(buf, buflen, attrs, more,
                TERM_WHITE);
        }
    }

    if (sdl_object_tooltip_feature_name(y, x, &feature_name))
        sdl_object_tooltip_append_part(buf, buflen, attrs, feature_name,
            TERM_WHITE);

    return buf[0] != '\0';
}

bool sdl_object_tooltip_show_grid(int map_y, int map_x, bool touch)
{
    char name[sizeof(g_object_tooltip.text)];
    Uint64 expires_at = sdl_object_tooltip_expiry(touch);
    bool persistent = touch && expires_at == 0;

    if (!sdl_object_tooltip_format_grid(map_y, map_x, name, sizeof(name),
            NULL))
    {
        sdl_object_tooltip_clear();
        return false;
    }

    if (g_object_tooltip.active
        && !g_object_tooltip.term_cell
        && !g_object_tooltip.screen_rect
        && g_object_tooltip.touch == touch
        && g_object_tooltip.map_y == map_y
        && g_object_tooltip.map_x == map_x
        && SDL_strcmp(g_object_tooltip.text, name) == 0)
    {
        if (touch) {
            g_object_tooltip.expires_at = expires_at;
            g_object_tooltip.persistent = persistent;
        }
        return true;
    }

    g_object_tooltip.active = true;
    g_object_tooltip.touch = touch;
    g_object_tooltip.term_cell = false;
    g_object_tooltip.character_panel_cell = false;
    g_object_tooltip.screen_rect = false;
    g_object_tooltip.map_y = map_y;
    g_object_tooltip.map_x = map_x;
    g_object_tooltip.cell_col = 0;
    g_object_tooltip.cell_row = 0;
    g_object_tooltip.cell_cols = 0;
    g_object_tooltip.rect = (SDL_FRect){ 0 };
    g_object_tooltip.expires_at = expires_at;
    g_object_tooltip.persistent = persistent;
    SDL_strlcpy(g_object_tooltip.text, name, sizeof(g_object_tooltip.text));
    g_state.need_present = true;
    return true;
}

bool sdl_object_tooltip_show_text_at_cell_ex(int col, int row, int cols,
    cptr text, bool touch, bool character_panel_cell)
{
    Uint64 expires_at = sdl_object_tooltip_expiry(touch);
    bool persistent = touch && expires_at == 0;

    if (!text || !text[0] || col < 0 || row < 0) {
        sdl_object_tooltip_clear();
        return false;
    }

    if (cols < 1)
        cols = 1;

    if (g_object_tooltip.active
        && g_object_tooltip.term_cell
        && g_object_tooltip.character_panel_cell == character_panel_cell
        && !g_object_tooltip.screen_rect
        && g_object_tooltip.touch == touch
        && g_object_tooltip.cell_col == col
        && g_object_tooltip.cell_row == row
        && g_object_tooltip.cell_cols == cols
        && SDL_strcmp(g_object_tooltip.text, text) == 0)
    {
        if (touch) {
            g_object_tooltip.expires_at = expires_at;
            g_object_tooltip.persistent = persistent;
        }
        return true;
    }

    g_object_tooltip.active = true;
    g_object_tooltip.touch = touch;
    g_object_tooltip.term_cell = true;
    g_object_tooltip.character_panel_cell = character_panel_cell;
    g_object_tooltip.screen_rect = false;
    g_object_tooltip.map_y = 0;
    g_object_tooltip.map_x = 0;
    g_object_tooltip.cell_col = col;
    g_object_tooltip.cell_row = row;
    g_object_tooltip.cell_cols = cols;
    g_object_tooltip.rect = (SDL_FRect){ 0 };
    g_object_tooltip.expires_at = expires_at;
    g_object_tooltip.persistent = persistent;
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

bool sdl_object_tooltip_show_text_at_rect(const SDL_FRect* rect, cptr text,
    bool touch)
{
    Uint64 expires_at = sdl_object_tooltip_expiry(touch);
    bool persistent = touch && expires_at == 0;

    if (!text || !text[0] || !rect || rect->w <= 0.0f || rect->h <= 0.0f) {
        sdl_object_tooltip_clear();
        return false;
    }

    if (g_object_tooltip.active
        && g_object_tooltip.screen_rect
        && g_object_tooltip.touch == touch
        && g_object_tooltip.rect.x == rect->x
        && g_object_tooltip.rect.y == rect->y
        && g_object_tooltip.rect.w == rect->w
        && g_object_tooltip.rect.h == rect->h
        && SDL_strcmp(g_object_tooltip.text, text) == 0)
    {
        if (touch) {
            g_object_tooltip.expires_at = expires_at;
            g_object_tooltip.persistent = persistent;
        }
        return true;
    }

    g_object_tooltip.active = true;
    g_object_tooltip.touch = touch;
    g_object_tooltip.term_cell = false;
    g_object_tooltip.character_panel_cell = false;
    g_object_tooltip.screen_rect = true;
    g_object_tooltip.map_y = 0;
    g_object_tooltip.map_x = 0;
    g_object_tooltip.cell_col = 0;
    g_object_tooltip.cell_row = 0;
    g_object_tooltip.cell_cols = 0;
    g_object_tooltip.rect = *rect;
    g_object_tooltip.expires_at = expires_at;
    g_object_tooltip.persistent = persistent;
    SDL_strlcpy(g_object_tooltip.text, text, sizeof(g_object_tooltip.text));
    g_state.need_present = true;
    return true;
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
#if SIL_SDL_MOBILE_BUILD
    /* Mobile: enlarge tooltip/character-wheel-description font (3/4 -> 4/5). */
    int font_size = sdl_auto_font_size_from_main(4, 5);
#else
    int font_size = sdl_auto_font_size_from_main(3, 4);
#endif
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

    if (!g_object_tooltip.active)
        return false;
    if (g_object_tooltip.screen_rect)
        return x >= g_object_tooltip.rect.x
            && y >= g_object_tooltip.rect.y
            && x < g_object_tooltip.rect.x + g_object_tooltip.rect.w
            && y < g_object_tooltip.rect.y + g_object_tooltip.rect.h;
    if (!g_object_tooltip.term_cell)
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

    if (g_object_tooltip.active
        && (g_object_tooltip.term_cell || g_object_tooltip.screen_rect))
    {
        if (sdl_object_tooltip_pointer_hits_term_cell(x, y))
            return;
        sdl_object_tooltip_clear();
    }

    if (g_unified_look_active)
        return;

    /* The aim-select loop drives the tooltip from game code; mouse motion
     * must not clear or retarget it here. */
    if (g_pointer_aim.active && g_pointer_aim.select_mode)
        return;

    if (g_pointer_aim.active
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

/* A colored, word-wrapped slice of the tooltip text: byte range, color and
 * laid-out position (line index plus pixel offset within the line). */
typedef struct object_tooltip_text_run {
    int start;
    int len;
    byte attr;
    int line;
    float x;
} object_tooltip_text_run;

enum { SDL_OBJECT_TOOLTIP_MAX_RUNS = 128 };

static float sdl_object_tooltip_measure_text(TTF_Font* font, cptr text,
    int len)
{
    int adv = 0;

    if (!font || !text || len <= 0)
        return 0.0f;

    len = utf8_safe_prefix_len(text, len);
    if (len <= 0)
        return 0.0f;

    TTF_MeasureString(font, text, (size_t)len, 0, &adv, NULL);
    return (float)adv;
}

/*
 * Split tooltip text into same-color runs, word-wrapped to max_text_w.
 * Returns the run count; *out_lines and *out_max_w receive the line count
 * and the widest laid-out line in pixels.
 */
static int sdl_object_tooltip_layout_runs(TTF_Font* font, cptr text,
    const byte* attrs, float max_text_w, object_tooltip_text_run* runs,
    int max_runs, int* out_lines, float* out_max_w)
{
    int run_count = 0;
    int line = 0;
    float line_w = 0.0f;
    float max_w = 0.0f;
    int len = (int)strlen(text);
    int pos = 0;

    while (pos < len && run_count < max_runs) {
        bool is_space = (text[pos] == ' ');
        int tok_start = pos;
        int tok_len;
        float tok_w;
        int sub;

        while (pos < len && (text[pos] == ' ') == is_space)
            pos++;
        tok_len = pos - tok_start;

        /* Drop spaces at the start of a wrapped line. */
        if (is_space && line_w <= 0.0f && line > 0)
            continue;

        tok_w = sdl_object_tooltip_measure_text(font, text + tok_start,
            tok_len);
        if (!is_space && line_w > 0.0f && line_w + tok_w > max_text_w) {
            line++;
            line_w = 0.0f;
        }

        /* Hard-break a single token wider than the box. */
        if (!is_space && tok_w > max_text_w) {
            int off = 0;

            while (off < tok_len && run_count < max_runs) {
                size_t fit_len = 0;
                int fit_w = 0;

                if (!TTF_MeasureString(font, text + tok_start + off,
                        (size_t)(tok_len - off), (int)max_text_w, &fit_w,
                        &fit_len)
                    || fit_len < 1)
                {
                    fit_len = (size_t)(tok_len - off);
                    fit_w = (int)tok_w;
                }

                runs[run_count++] = (object_tooltip_text_run){
                    .start = tok_start + off,
                    .len = (int)fit_len,
                    .attr = attrs[tok_start + off],
                    .line = line,
                    .x = 0.0f,
                };
                line_w = (float)fit_w;
                if (line_w > max_w)
                    max_w = line_w;
                off += (int)fit_len;
                if (off < tok_len) {
                    line++;
                    line_w = 0.0f;
                }
            }
            continue;
        }

        /* Emit the token as one run per color, merging with the previous
         * run when contiguous and same-colored. */
        sub = tok_start;
        while (sub < tok_start + tok_len) {
            byte attr = attrs[sub];
            int sub_end = sub + 1;
            float sub_w;

            while (sub_end < tok_start + tok_len && attrs[sub_end] == attr)
                sub_end++;

            sub_w = sdl_object_tooltip_measure_text(font, text + sub,
                sub_end - sub);
            if (run_count > 0
                && runs[run_count - 1].line == line
                && runs[run_count - 1].attr == attr
                && runs[run_count - 1].start + runs[run_count - 1].len == sub)
            {
                runs[run_count - 1].len += sub_end - sub;
            } else {
                if (run_count >= max_runs)
                    break;
                runs[run_count++] = (object_tooltip_text_run){
                    .start = sub,
                    .len = sub_end - sub,
                    .attr = attr,
                    .line = line,
                    .x = line_w,
                };
            }
            line_w += sub_w;
            sub = sub_end;
        }
        if (line_w > max_w)
            max_w = line_w;
    }

    if (out_lines)
        *out_lines = line + 1;
    if (out_max_w)
        *out_max_w = max_w;
    return run_count;
}

static float sdl_object_tooltip_clamp_box_y(float y, float box_h,
    const SDL_Rect* screen, float screen_margin)
{
    if (box_h + screen_margin * 2.0f <= (float)screen->h)
        return sdl_touch_pane_clampf(y, (float)screen->y + screen_margin,
            (float)(screen->y + screen->h) - box_h - screen_margin);

    return (float)screen->y + screen_margin;
}

void sdl_object_tooltip_render(void)
{
    char current[sizeof(g_object_tooltip.text)];
    byte attrs[sizeof(g_object_tooltip.text)];
    object_tooltip_text_run runs[SDL_OBJECT_TOOLTIP_MAX_RUNS];
    int run_count;
    int line_count = 1;
    float text_w = 0.0f;
    SDL_FRect cell_rect;
    SDL_Rect screen;
    TTF_Font* font;
    SDL_FRect box;
    SDL_FRect player_rect;
    float pad;
    float gap;
    float screen_margin;
    float max_box_w;
    float max_text_w;
    float above_y;
    float below_y;
    bool prefer_above;
    int font_px;
    int line_h;

    if (!g_object_tooltip.active)
        return;
    if (g_object_tooltip.touch && g_object_tooltip.expires_at
        && SDL_GetTicksNS() >= g_object_tooltip.expires_at)
    {
        sdl_object_tooltip_clear();
        return;
    }

    SDL_memset(attrs, TERM_WHITE, sizeof(attrs));
    if (g_object_tooltip.screen_rect) {
        if (!g_object_tooltip.text[0]
            || g_object_tooltip.rect.w <= 0.0f
            || g_object_tooltip.rect.h <= 0.0f)
        {
            sdl_object_tooltip_clear();
            return;
        }
        cell_rect = g_object_tooltip.rect;
    } else if (g_object_tooltip.term_cell) {
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
                g_object_tooltip.map_x, current, sizeof(current), attrs)
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
    font = sdl_story_font_for_height_slot(font_px,
        STORY_FONT_SLOT_SECONDARY);
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
    run_count = sdl_object_tooltip_layout_runs(font, g_object_tooltip.text,
        attrs, max_text_w, runs, SDL_OBJECT_TOOLTIP_MAX_RUNS, &line_count,
        &text_w);
    if (run_count < 1 || text_w < 1.0f)
        return;

    line_h = TTF_GetFontHeight(font);
    if (line_h < 1)
        line_h = font_px;

    box.w = text_w + pad * 2.0f;
    box.h = (float)(line_count * line_h) + pad * 2.0f;
    above_y = cell_rect.y - box.h - gap;
    below_y = cell_rect.y + cell_rect.h + gap;
    prefer_above = (above_y >= (float)screen.y + screen_margin);

    box.x = cell_rect.x + (cell_rect.w - box.w) * 0.5f;
    if (box.w + screen_margin * 2.0f <= (float)screen.w) {
        box.x = sdl_touch_pane_clampf(box.x,
            (float)screen.x + screen_margin,
            (float)(screen.x + screen.w) - box.w - screen_margin);
    } else {
        box.x = (float)screen.x + screen_margin;
    }
    box.y = sdl_object_tooltip_clamp_box_y(prefer_above ? above_y : below_y,
        box.h, &screen, screen_margin);

    /* Keep the popup off the player's tile: flip to the other side of the
     * hovered cell, or dodge sideways when both sides would cover it. */
    if (!g_object_tooltip.term_cell
        && sdl_player_map_rect(p_ptr->py, p_ptr->px, &player_rect)
        && sdl_description_overlay_rects_intersect(&box, &player_rect))
    {
        SDL_FRect alt = box;

        alt.y = sdl_object_tooltip_clamp_box_y(
            prefer_above ? below_y : above_y, box.h, &screen, screen_margin);
        if (!sdl_description_overlay_rects_intersect(&alt, &player_rect)) {
            box.y = alt.y;
        } else {
            float right_x = player_rect.x + player_rect.w + gap;
            float left_x = player_rect.x - box.w - gap;
            bool right_ok = right_x + box.w + screen_margin
                <= (float)(screen.x + screen.w);
            bool left_ok = left_x >= (float)screen.x + screen_margin;
            float d_right = right_x - box.x;
            float d_left = box.x - left_x;

            if (d_right < 0.0f)
                d_right = -d_right;
            if (d_left < 0.0f)
                d_left = -d_left;
            if (right_ok && (!left_ok || d_right <= d_left))
                box.x = right_x;
            else if (left_ok)
                box.x = left_x;
        }
    }

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 210);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 130);
    SDL_RenderRect(g_state.renderer, &box);

    for (int i = 0; i < run_count; i++) {
        const object_tooltip_text_run* run = &runs[i];
        char run_text[sizeof(g_object_tooltip.text)];
        SDL_Surface* surface;
        SDL_Texture* texture;
        SDL_FRect text_dst;
        int copy_len = run->len;

        if (copy_len >= (int)sizeof(run_text))
            copy_len = (int)sizeof(run_text) - 1;
        SDL_memcpy(run_text, g_object_tooltip.text + run->start,
            (size_t)copy_len);
        run_text[copy_len] = '\0';

        surface = TTF_RenderText_Blended(font, run_text, 0,
            g_state.palette[run->attr]);
        if (!surface)
            continue;

        texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
        if (!texture) {
            SDL_DestroySurface(surface);
            continue;
        }

        text_dst = (SDL_FRect){
            .x = box.x + pad + run->x,
            .y = box.y + pad + (float)(run->line * line_h),
            .w = (float)surface->w,
            .h = (float)surface->h,
        };
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, texture, NULL, &text_dst);

        SDL_DestroyTexture(texture);
        SDL_DestroySurface(surface);
    }
}

static int sdl_description_overlay_font_px_for_story(bool story)
{
    int font_px = get_sdl_terminal_menu_scale() * TILE_SIZE;

    if (story)
        font_px = (font_px * 7) / 5;

    if (font_px < 8)
        font_px = 8;

    return font_px;
}

int sdl_description_overlay_font_px(void)
{
    return sdl_description_overlay_font_px_for_story(false);
}

static int sdl_description_overlay_story_font_px(void)
{
    return sdl_description_overlay_font_px_for_story(true);
}

static int sdl_description_overlay_cell_w_for_font_px(int font_px)
{
    int cell_w = font_px / 2;

    return (cell_w > 0) ? cell_w : 1;
}

static int sdl_description_overlay_anchor_max_cols_for_font_px(int font_px)
{
    SDL_Rect anchor;
    int cell_w;
    int margin;
    int pad_x;
    int max_panel_w;
    int max_cols;

    if (!sdl_overlay_pane_anchor_rect(PANE_DESCRIPTION, &anchor))
        return 80;

    cell_w = sdl_description_overlay_cell_w_for_font_px(font_px);
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

int sdl_description_overlay_max_cols(void)
{
    int max_cols = sdl_description_overlay_anchor_max_cols_for_font_px(
        sdl_description_overlay_font_px());

    /*
     * Match the historical item description body width, which left a small
     * allowance inside the description pane instead of filling every column.
     */
    if (max_cols > 24)
        max_cols -= 4;

    return max_cols;
}

/* Pixel width available for text inside the description overlay panel.  Used to
 * wrap proportional (story-font) descriptions so they fill the panel instead of
 * breaking at the monospace column budget. */
int sdl_description_overlay_text_px(void)
{
    int font_px = sdl_description_overlay_font_px();
    int cell_w = sdl_description_overlay_cell_w_for_font_px(font_px);
    int cols = sdl_description_overlay_max_cols();

    if (text_out_wrap > 0 && text_out_wrap < cols)
        cols = text_out_wrap;

    return cols * cell_w;
}

/* Proportional width, in pixels, of a text run rendered in the overlay's story
 * font for the given slot at the overlay cell height (matches the scaling used
 * by sdl_description_overlay_render_story_run). */
int sdl_description_overlay_story_text_width(cptr text, int len, int slot)
{
    int font_px;
    int cell_h;
    int adv = 0;
    int font_h;
    TTF_Font* font;

    if (!text || len <= 0)
        return 0;

    len = utf8_safe_prefix_len(text, len);
    if (len <= 0)
        return 0;

    font_px = sdl_description_overlay_story_font_px();
    cell_h = font_px;
    font = sdl_story_font_for_height_slot(cell_h, slot);
    if (!font)
        return 0;

    TTF_MeasureString(font, text, len, 0, &adv, NULL);

    font_h = TTF_GetFontHeight(font);
    if (font_h > 0)
        adv = (int)((float)adv * ((float)cell_h / (float)font_h));

    return adv;
}

static bool sdl_description_overlay_has_story(
    const description_overlay_state* overlay)
{
    if (!overlay || !overlay->story || overlay->width <= 0
        || overlay->height <= 0)
    {
        return false;
    }

    for (int row = 0; row < overlay->height; row++)
    {
        for (int col = 0; col < overlay->width; col++)
        {
            if (overlay->story[row * overlay->width + col] & STORY_FLAG_USE)
                return true;
        }
    }

    return false;
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

    return "Esc close  Space page  Dir scroll";
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
    bool has_story;
    int font_px;
    int mono_cell_w;
    int cell_w;
    int cell_h;
    int margin;
    int pad_x;
    int pad_y;
    int max_panel_w;
    int max_panel_h;
    int max_rows_no_footer;
    int max_rows;
    int max_cols;
    int target_cols;
    int footer_cols = 0;
    int text_px;
    int visible_cols;
    int visible_rows;
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

    has_story = sdl_description_overlay_has_story(overlay);
    font_px = sdl_description_overlay_font_px_for_story(has_story);
    cell_h = font_px;
    cell_w = sdl_description_overlay_cell_w_for_font_px(font_px);
    mono_cell_w = sdl_description_overlay_cell_w_for_font_px(
        sdl_description_overlay_font_px());

    margin = sdl_overlay_margin_px();
    pad_x = mono_cell_w;
    pad_y = cell_h / 2;
    if (pad_y < 2)
        pad_y = 2;

    /* Reserve the left strip the thumb buttons occupy so the popup is sized and
     * centred in the space to their right and never crosses them. */
    {
        SDL_FRect thumb_rects[SDL_TOUCH_THUMB_BUTTON_COUNT];

        if (sdl_touch_thumb_compute_rects(thumb_rects)) {
            float right = 0.0f;

            for (int i = 0; i < SDL_TOUCH_THUMB_BUTTON_COUNT; i++) {
                float edge = thumb_rects[i].x + thumb_rects[i].w;

                if (thumb_rects[i].w > 0.0f && edge > right)
                    right = edge;
            }
            if (right > 0.0f) {
                int reserve = (int)right + margin - anchor.x;

                if (reserve > 0 && reserve < anchor.w) {
                    anchor.x += reserve;
                    anchor.w -= reserve;
                }
            }
        }
    }

    max_panel_w = anchor.w - margin * 2;
    max_panel_h = anchor.h - margin * 2;
    if (max_panel_w <= pad_x * 2 || max_panel_h <= pad_y * 2 + cell_h)
        return false;

    max_cols = (max_panel_w - pad_x * 2) / cell_w;
    max_rows_no_footer = (max_panel_h - pad_y * 2) / cell_h;
    if (max_cols < 1 || max_rows_no_footer < 1)
        return false;

    target_cols = (overlay->target_cols > 0) ? overlay->target_cols
                                             : overlay->width;
    if (target_cols < 1)
        target_cols = 1;

    if (overlay->interactive)
        footer_cols = (int)strlen(sdl_description_overlay_footer_text(overlay));

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

        if (has_story)
        {
            text_px = target_cols * mono_cell_w;
            visible_cols = (text_px + cell_w - 1) / cell_w;
        }
        else
        {
            visible_cols = target_cols;
            text_px = visible_cols * cell_w;
        }

        if (visible_cols < footer_cols)
        {
            visible_cols = footer_cols;
            text_px = visible_cols * cell_w;
        }
        if (visible_cols > max_cols)
        {
            visible_cols = max_cols;
            text_px = visible_cols * cell_w;
        }
        if (visible_cols < 1)
        {
            visible_cols = 1;
            text_px = cell_w;
        }

        panel_w = (float)(text_px + pad_x * 2);
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

bool sdl_description_overlay_contains_point(float x, float y)
{
    description_overlay_layout layout;

    if (!sdl_description_overlay_layout(&layout))
        return false;

    return sdl_point_in_frect(&layout.panel, x, y);
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

/*
 * Advance width, in pixels, of a story-font run at the overlay cell height.
 * Mirrors the advance computed by sdl_description_overlay_render_story_run so
 * hit-testing and layout agree with what is actually drawn.
 */
static float sdl_description_overlay_run_advance(TTF_Font* font,
    float cell_h, const char* s, int n)
{
    int adv = 0;
    int font_h;

    if (!font || !s || n <= 0)
        return 0.0f;

    n = utf8_safe_prefix_len(s, n);
    if (n <= 0)
        return 0.0f;

    TTF_MeasureString(font, s, n, 0, &adv, NULL);
    font_h = TTF_GetFontHeight(font);
    if (font_h > 0)
        return (float)adv * (cell_h / (float)font_h);

    return (float)adv;
}

/*
 * Render a run of story-font glyphs inside the description overlay, scaled to
 * the overlay cell height.  Returns the advance width in pixels so the caller
 * can keep flowing proportional text.  The active render clip (the panel rect)
 * keeps glyphs from spilling past the panel edges.
 */
static float sdl_description_overlay_render_story_run(TTF_Font* font,
    float x_px, float y_px, float cell_h, const char* s, int n, byte attr)
{
    char text_buf[256];
    int len;
    int adv_unscaled = 0;
    float scale;
    float advance_w;
    float render_w;
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_Color col;

    if (!font || !s || n <= 0)
        return 0.0f;

    len = (n < 255) ? n : 255;
    len = utf8_safe_prefix_len(s, len);
    if (len <= 0)
        return 0.0f;
    for (int i = 0; i < len; i++)
    {
        unsigned char ch = (unsigned char)s[i];
        text_buf[i] = ch ? (char)ch : ' ';
    }
    text_buf[len] = '\0';

    col = sdl_description_overlay_attr_color(attr);
    surface = TTF_RenderText_Blended(font, text_buf, 0, col);
    if (!surface)
        return 0.0f;

    TTF_MeasureString(font, text_buf, len, 0, &adv_unscaled, NULL);
    scale = (surface->h > 0) ? (cell_h / (float)surface->h) : 1.0f;
    render_w = (float)surface->w * scale;

    /* Advance must match sdl_description_overlay_story_text_width (which scales
     * by the font height) so wrapping and rendering agree on line width. */
    {
        int font_h = TTF_GetFontHeight(font);
        float adv_scale = (font_h > 0) ? (cell_h / (float)font_h) : scale;
        advance_w = (float)adv_unscaled * adv_scale;
    }

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (texture)
    {
        SDL_FRect dst = { x_px, y_px, render_w, cell_h };

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }

    SDL_DestroySurface(surface);
    return advance_w;
}

/*
 * Render one captured overlay row.  Story-font cells (flagged during capture)
 * are drawn proportionally with the matching story slot; plain cells fall back
 * to the monospace atlas.  Returns nothing; positions are relative to the
 * laid-out panel.
 */
static void sdl_description_overlay_render_row(
    const description_overlay_state* overlay,
    const description_overlay_layout* layout, SDL_Texture* atlas,
    int atlas_cell_w, int atlas_cell_h, int src_row, float row_y)
{
    /* Process every captured column: proportional story lines span more columns
     * than the panel's monospace budget, and the active panel clip keeps any
     * overflow inside the box. */
    int limit = overlay->width;

    int col = 0;
    while (col < limit)
    {
        int idx = src_row * overlay->width + col;
        byte sflag = overlay->story ? overlay->story[idx] : 0;

        if (!(sflag & STORY_FLAG_USE))
        {
            sdl_description_overlay_render_char(atlas, atlas_cell_w,
                atlas_cell_h, (float)layout->cell_w, (float)layout->cell_h,
                layout->text_x + (float)col * (float)layout->cell_w, row_y,
                overlay->attrs[idx], overlay->chars[idx]);
            col++;
            continue;
        }

        /* Gather a contiguous story region sharing slot and alignment mode. */
        bool slot2 = (sflag & STORY_FLAG_SLOT2) != 0;
        bool grid_align = (sflag & STORY_FLAG_CELL_ALIGN) != 0;
        int region_start = col;
        while (col < limit)
        {
            byte f = overlay->story[src_row * overlay->width + col];
            if (!(f & STORY_FLAG_USE))
                break;
            if (((f & STORY_FLAG_SLOT2) != 0) != slot2)
                break;
            if (((f & STORY_FLAG_CELL_ALIGN) != 0) != grid_align)
                break;
            col++;
        }
        int region_end = col;

        TTF_Font* font = sdl_story_font_for_height_slot(layout->cell_h,
            slot2 ? STORY_FONT_SLOT_SECONDARY : STORY_FONT_SLOT_DEFAULT);
        if (!font)
        {
            /* No story font available: fall back to the mono atlas. */
            for (int c = region_start; c < region_end; c++)
            {
                int i = src_row * overlay->width + c;
                sdl_description_overlay_render_char(atlas, atlas_cell_w,
                    atlas_cell_h, (float)layout->cell_w,
                    (float)layout->cell_h,
                    layout->text_x + (float)c * (float)layout->cell_w, row_y,
                    overlay->attrs[i], overlay->chars[i]);
            }
            continue;
        }

        if (grid_align)
        {
            /* Snap each glyph to its cell so column alignment is preserved. */
            for (int c = region_start; c < region_end; c++)
            {
                int i = src_row * overlay->width + c;
                sdl_description_overlay_render_story_run(font,
                    layout->text_x + (float)c * (float)layout->cell_w, row_y,
                    (float)layout->cell_h, overlay->chars + i, 1,
                    overlay->attrs[i]);
            }
            continue;
        }

        /* Free region: pack same-attr segments by measured pixel width. */
        float px_cursor = layout->text_x
            + (float)region_start * (float)layout->cell_w;
        int seg = region_start;
        while (seg < region_end)
        {
            byte seg_attr = overlay->attrs[src_row * overlay->width + seg];
            int seg_end = seg + 1;
            while (seg_end < region_end
                && overlay->attrs[src_row * overlay->width + seg_end] == seg_attr)
            {
                seg_end++;
            }

            int seg_idx = src_row * overlay->width + seg;
            px_cursor += sdl_description_overlay_render_story_run(font,
                px_cursor, row_y, (float)layout->cell_h,
                overlay->chars + seg_idx, seg_end - seg, seg_attr);
            seg = seg_end;
        }
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

        sdl_description_overlay_render_row(overlay, &layout, atlas,
            atlas_cell_w, atlas_cell_h, src_row,
            layout.text_y + (float)row * (float)layout.cell_h);
    }

    if (layout.footer)
    {
        char scroll_buf[32];
        cptr footer_text = sdl_description_overlay_footer_text(overlay);
        /* The command row uses the secondary story font (storyfont2), matching
         * the menu/log UI; glyphs are snapped per-cell so footer hover/column
         * hit-testing stays aligned with the monospace layout. */
        TTF_Font* footer_font = sdl_story_font_for_height_slot(
            layout.cell_h, STORY_FONT_SLOT_SECONDARY);

        if (overlay->interactive)
        {
            int len = (int)strlen(footer_text);

            if (len > layout.visible_cols)
                len = layout.visible_cols;

            if (footer_font)
            {
                /* Proportional: draw contiguous runs that share a hover state
                 * as whole words, advancing by the measured pixel width. */
                float px = layout.text_x;
                int col = 0;

                while (col < len)
                {
                    bool hot = sdl_description_overlay_token_matches_hover(
                        overlay, footer_text, col);
                    int run_end = col + 1;

                    while (run_end < len
                        && sdl_description_overlay_token_matches_hover(
                               overlay, footer_text, run_end) == hot)
                    {
                        run_end++;
                    }

                    px += sdl_description_overlay_render_story_run(footer_font,
                        px, layout.footer_y, (float)layout.cell_h,
                        footer_text + col, run_end - col,
                        hot ? TERM_L_BLUE : TERM_SLATE);
                    col = run_end;
                }
            }
            else
            {
                for (int col = 0; col < len; col++)
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
        }
        if (layout.max_scroll > 0)
        {
            strnfmt(scroll_buf, sizeof(scroll_buf), "%d/%d",
                layout.scroll + 1, overlay->height);

            if (footer_font)
            {
                /* Right-align by the run's measured proportional width. */
                float w = sdl_description_overlay_run_advance(footer_font,
                    (float)layout.cell_h, scroll_buf,
                    (int)strlen(scroll_buf));
                float right = layout.text_x
                    + (float)layout.visible_cols * (float)layout.cell_w;
                float x = right - w;

                if (x < layout.text_x)
                    x = layout.text_x;
                sdl_description_overlay_render_story_run(footer_font, x,
                    layout.footer_y, (float)layout.cell_h, scroll_buf,
                    (int)strlen(scroll_buf), TERM_SLATE);
            }
            else
            {
                int scroll_col =
                    layout.visible_cols - (int)strlen(scroll_buf);
                if (scroll_col < 0)
                    scroll_col = 0;
                sdl_description_overlay_render_text(atlas, atlas_cell_w,
                    atlas_cell_h, scroll_buf,
                    layout.text_x + (float)scroll_col * (float)layout.cell_w,
                    layout.footer_y, (float)layout.cell_w,
                    (float)layout.cell_h, TERM_SLATE);
            }
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

    footer_text = sdl_description_overlay_footer_text(overlay);

    {
        /* The command row is drawn in the proportional secondary story font,
         * so map the pointer to a character by accumulating glyph advances
         * (matching the render path).  Fall back to monospace columns when no
         * story font is available. */
        TTF_Font* footer_font = sdl_story_font_for_height_slot(
            layout.cell_h, STORY_FONT_SLOT_SECONDARY);

        if (footer_font)
        {
            int len = (int)strlen(footer_text);
            float relx = x - layout.text_x;

            if (len > layout.visible_cols)
                len = layout.visible_cols;

            /* Accumulate by prefix width from the string start so kerning is
             * accounted for the same way the render measures whole runs. */
            col = -1;
            for (int i = 0; i < len; i++)
            {
                float end_px = sdl_description_overlay_run_advance(footer_font,
                    (float)layout.cell_h, footer_text, i + 1);
                if (relx < end_px)
                {
                    col = i;
                    break;
                }
            }
            if (col < 0)
                return 0;
        }
        else
        {
            col = (int)((x - layout.text_x) / (float)layout.cell_w);
            if (col < 0 || col >= layout.visible_cols)
                return 0;
        }
    }

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
        || sdl_object_tooltip_format_grid(y, x, NULL, 0, NULL);
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

    if (sdl_touch_movement_point_blocked_by_overlay(x, y))
        return false;
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
    tooltip_target = sdl_object_tooltip_format_grid(map_y, map_x, NULL, 0,
        NULL);
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

        if (sdl_object_tooltip_format_grid(y, x, desc, sizeof(desc), NULL)) {
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

    /* Draw only the already-computed path.  This is the per-frame render path,
     * so it must never run the (heavy, on large levels) search itself.  The old
     * code recomputed here whenever the path was invalid or the player had
     * moved -- which, for an unreachable/far hover target, re-ran the full A*
     * every presented frame and was the in-battle "slow redraw"/left-panel
     * churn.  The hover (handle_motion) and follow (take_step) paths keep
     * g_mouse_path fresh; if it is not valid here, simply skip drawing. */
    if (!g_mouse_path.path_valid)
        return;

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
