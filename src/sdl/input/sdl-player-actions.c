#include "angband.h"
#include "sdl/main-sdl-private.h"

bool sdl_player_map_rect(int y, int x, SDL_FRect* out_rect)
{
    int cell_cols = use_bigtile ? 2 : 1;
    int term_row;
    int term_col;

    if (!out_rect || !p_ptr)
        return false;
    if (!panel_contains(y, x))
        return false;

    term_row = ROW_MAP + (y - p_ptr->wy);
    term_col = COL_MAP + (x - p_ptr->wx) * cell_cols;
    return sdl_main_cell_rect(term_col, term_row, cell_cols, 1, out_rect);
}

bool sdl_point_in_frect(const SDL_FRect* rect, float x, float y)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return false;

    return x >= rect->x && x < rect->x + rect->w
        && y >= rect->y && y < rect->y + rect->h;
}

void sdl_player_confirm_at_player(void)
{
    sdl_player_action_menu_cancel();
    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    sdl_touch_pane_send_confirm_action();
}

bool sdl_main_view_point_is_player_grid(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x))
        return false;

    return map_y == p_ptr->py && map_x == p_ptr->px;
}

bool sdl_player_has_equipped_staff(void)
{
    return inventory[INVEN_STAFF].k_idx
        && inventory[INVEN_STAFF].tval == TV_STAFF;
}

bool sdl_player_has_equipped_horn(void)
{
    return inventory[INVEN_HORN].k_idx
        && inventory[INVEN_HORN].tval == TV_HORN;
}

bool sdl_player_has_singable_song(void)
{
    if (!p_ptr)
        return false;

    for (int i = 0; i < SNG_MAX; i++) {
        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
            continue;
        if (p_ptr->active_ability[S_SNG][i])
            return true;
    }

    return false;
}

#define SDL_PLAYER_ACTION_MENU_PI 3.14159265f

enum {
    SDL_PLAYER_ACTION_MENU_SECTOR_SEGMENTS = 12
};

static cptr sdl_player_action_menu_description_for_kind(int kind)
{
    switch (kind) {
    case SDL_PLAYER_ACTION_WAIT:
        return "Wait: pass one turn; long press rests until disturbed.";
    case SDL_PLAYER_ACTION_USE:
        return "Use: use an item, or use the floor item under you.";
    case SDL_PLAYER_ACTION_STEALTH:
        return "Stealth: toggle stealth mode.";
    case SDL_PLAYER_ACTION_SING:
        return "Sing: choose or stop a song.";
    case SDL_PLAYER_ACTION_EXCHANGE:
        return "Exchange: swap places with an adjacent monster.";
    case SDL_PLAYER_ACTION_FLETCH:
        return "Fletch: make arrows from available materials.";
    case SDL_PLAYER_ACTION_EXAMINE:
        return "Examine: look at your square or inspect the floor item.";
    case SDL_PLAYER_ACTION_ACTIVATE:
        return "Staff: activate your equipped staff.";
    case SDL_PLAYER_ACTION_HORN:
        return "Horn: blow your equipped horn.";
    default:
        return "";
    }
}

static cptr sdl_player_action_menu_fallback_for_kind(int kind)
{
    switch (kind) {
    case SDL_PLAYER_ACTION_WAIT: return "Z";
    case SDL_PLAYER_ACTION_USE: return "U";
    case SDL_PLAYER_ACTION_STEALTH: return "S";
    case SDL_PLAYER_ACTION_SING: return "Song";
    case SDL_PLAYER_ACTION_EXCHANGE: return "X";
    case SDL_PLAYER_ACTION_FLETCH: return "-";
    case SDL_PLAYER_ACTION_EXAMINE: return "?";
    case SDL_PLAYER_ACTION_ACTIVATE: return "Staff";
    case SDL_PLAYER_ACTION_HORN: return "Horn";
    default: return "";
    }
}

static void sdl_player_action_menu_tile_for_kind(int kind, byte* out_attr,
    char* out_char)
{
    byte row = 12;
    byte col = 10;

    switch (kind) {
    case SDL_PLAYER_ACTION_WAIT:
        row = 19; col = 8;  /* sleep icon */
        break;
    case SDL_PLAYER_ACTION_USE:
        row = 5; col = 8;   /* flask/oil, a common useable item */
        break;
    case SDL_PLAYER_ACTION_STEALTH:
        row = 5; col = 3;   /* shadow cloak */
        break;
    case SDL_PLAYER_ACTION_SING:
        row = 11; col = 27; /* amulet, for voice/song choices */
        break;
    case SDL_PLAYER_ACTION_EXCHANGE:
        row = 12; col = 30; /* seen/target icon */
        break;
    case SDL_PLAYER_ACTION_FLETCH:
        row = 5; col = 27;  /* arrows */
        break;
    case SDL_PLAYER_ACTION_EXAMINE:
        row = 12; col = 10; /* question mark */
        break;
    case SDL_PLAYER_ACTION_ACTIVATE:
        row = 6; col = 8;   /* quarterstaff */
        break;
    case SDL_PLAYER_ACTION_HORN:
        row = 4; col = 2;   /* horn-shaped atlas tile */
        break;
    default:
        break;
    }

    if (out_attr)
        *out_attr = (byte)(TILE_FLAG | row);
    if (out_char)
        *out_char = (char)(TILE_FLAG | col);
}

void sdl_player_action_menu_add_entry(player_action_menu_entry* entries,
    int* count, int kind, int command, cptr label)
{
    player_action_menu_entry* entry;

    if (!entries || !count || *count >= SDL_PLAYER_ACTION_MAX)
        return;

    entry = &entries[*count];
    entry->kind = kind;
    entry->command = command;
    entry->label = label;
    entry->description = sdl_player_action_menu_description_for_kind(kind);
    entry->fallback = sdl_player_action_menu_fallback_for_kind(kind);
    sdl_player_action_menu_tile_for_kind(kind, &entry->tile_attr,
        &entry->tile_char);
    entry->rect = (SDL_FRect) { 0 };
    entry->center_x = 0.0f;
    entry->center_y = 0.0f;
    entry->inner_radius = 0.0f;
    entry->outer_radius = 0.0f;
    entry->start_angle = 0.0f;
    entry->end_angle = 0.0f;
    (*count)++;
}

int sdl_player_action_menu_collect(player_action_menu_entry* entries)
{
    int count = 0;

    sdl_player_action_menu_add_entry(entries, &count, SDL_PLAYER_ACTION_WAIT,
        'z', "Wait");
    sdl_player_action_menu_add_entry(entries, &count, SDL_PLAYER_ACTION_USE,
        'u', "Use");
    sdl_player_action_menu_add_entry(entries, &count,
        SDL_PLAYER_ACTION_STEALTH, 'S', "Stealth");
    if (sdl_player_has_singable_song()) {
        sdl_player_action_menu_add_entry(entries, &count, SDL_PLAYER_ACTION_SING,
            's', "Sing");
    }
    if (p_ptr && p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES]) {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_EXCHANGE, 'X', "Xchg");
    }
    if (p_ptr && p_ptr->active_ability[S_ARC][ARC_FLETCHERY]) {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_FLETCH, '-', "Fletch");
    }
    sdl_player_action_menu_add_entry(entries, &count,
        SDL_PLAYER_ACTION_EXAMINE, 'x', "Desc");
    if (sdl_player_has_equipped_staff()) {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_ACTIVATE, 'a', "Staff");
    }
    if (sdl_player_has_equipped_horn()) {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_HORN, 'p', "Horn");
    }

    return count;
}

bool sdl_player_has_floor_item_underfoot(void)
{
    int floor_list[MAX_FLOOR_STACK];

    if (!p_ptr)
        return false;

    return scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px,
        0x00) > 0;
}

bool sdl_player_action_menu_kind_supports_secondary(int kind)
{
    return kind == SDL_PLAYER_ACTION_WAIT || kind == SDL_PLAYER_ACTION_USE
        || kind == SDL_PLAYER_ACTION_EXAMINE;
}

int sdl_player_action_menu_default_kind(void)
{
    player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = sdl_player_action_menu_collect(entries);

    if (count <= 0)
        return SDL_PLAYER_ACTION_NONE;

    for (int i = 0; i < count; i++) {
        if (entries[i].kind == SDL_PLAYER_ACTION_USE)
            return entries[i].kind;
    }

    return entries[0].kind;
}

int sdl_player_action_menu_hover_index(
    player_action_menu_entry* entries, int count)
{
    if (!entries || count <= 0)
        return -1;

    for (int i = 0; i < count; i++) {
        if (entries[i].kind == g_player_action_menu.hover_kind)
            return i;
    }

    return -1;
}

void sdl_player_action_menu_select_default(void)
{
    int kind = sdl_player_action_menu_default_kind();

    if (kind == SDL_PLAYER_ACTION_NONE)
        return;

    if (g_player_action_menu.hover_kind != kind) {
        g_player_action_menu.hover_kind = kind;
        g_state.need_present = true;
    }
}

void sdl_player_action_menu_move_hover(int delta)
{
    player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = sdl_player_action_menu_collect(entries);
    int index;

    if (count <= 0)
        return;

    index = sdl_player_action_menu_hover_index(entries, count);
    if (index < 0) {
        index = (delta < 0) ? count - 1 : 0;
    } else {
        index = (index + delta) % count;
        if (index < 0)
            index += count;
    }

    if (g_player_action_menu.hover_kind != entries[index].kind) {
        g_player_action_menu.hover_kind = entries[index].kind;
        g_state.need_present = true;
    }
}

void sdl_player_action_menu_activate_hover(void)
{
    if (g_player_action_menu.hover_kind == SDL_PLAYER_ACTION_NONE)
        sdl_player_action_menu_select_default();

    if (g_player_action_menu.hover_kind != SDL_PLAYER_ACTION_NONE)
        sdl_player_action_menu_activate_kind(g_player_action_menu.hover_kind,
            false);
}

void sdl_player_action_menu_start_gamepad_press(int button, int kind)
{
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return;
    if (!sdl_player_action_menu_kind_supports_secondary(kind))
        return;

    sdl_player_action_menu_cancel_press();
    g_player_action_menu.press_active = true;
    g_player_action_menu.press_mouse = false;
    g_player_action_menu.press_gamepad = true;
    g_player_action_menu.press_secondary = false;
    g_player_action_menu.press_finger_id = 0;
    g_player_action_menu.press_button = button;
    g_player_action_menu.press_kind = kind;
    g_player_action_menu.press_start_x = 0.0f;
    g_player_action_menu.press_start_y = 0.0f;
    g_player_action_menu.press_start_time = SDL_GetTicksNS();
    g_state.need_present = true;
}

bool sdl_player_action_menu_handle_gamepad_confirm(int button, bool down)
{
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return false;

    if (g_player_action_menu.press_active
        && g_player_action_menu.press_gamepad)
    {
        int kind = g_player_action_menu.press_kind;
        bool activate_secondary = false;

        if (g_player_action_menu.press_button != button)
            return true;
        if (down)
            return true;

        if (sdl_player_action_menu_kind_supports_secondary(kind)
            && g_player_action_menu.press_start_time
            && SDL_GetTicksNS() - g_player_action_menu.press_start_time
                >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        {
            activate_secondary = true;
        }

        sdl_player_action_menu_cancel_press();
        if (kind != SDL_PLAYER_ACTION_NONE)
            sdl_player_action_menu_activate_kind(kind, activate_secondary);
        return true;
    }

    if (!down)
        return true;

    if (g_player_action_menu.hover_kind == SDL_PLAYER_ACTION_NONE)
        sdl_player_action_menu_select_default();

    if (sdl_player_action_menu_kind_supports_secondary(
            g_player_action_menu.hover_kind))
    {
        sdl_player_action_menu_start_gamepad_press(button,
            g_player_action_menu.hover_kind);
    } else {
        sdl_player_action_menu_activate_hover();
    }

    return true;
}

void sdl_player_action_menu_slot_offset(int slot, int count,
    float* out_x, float* out_y)
{
    static const float offsets[SDL_PLAYER_ACTION_MAX + 1]
        [SDL_PLAYER_ACTION_MAX][2] = {
            { { 0.0f, 0.0f } },
            { { 0.0f, -1.0f } },
            { { -0.78f, -0.78f }, { 0.78f, -0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f } },
            { { -0.78f, -0.78f }, { 0.78f, -0.78f },
              { 0.78f, 0.78f }, { -0.78f, 0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 0.78f, 0.78f },
              { -0.78f, 0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 0.78f, 0.78f },
              { 0.0f, 1.0f }, { -0.78f, 0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 1.0f, 0.0f },
              { 0.78f, 0.78f }, { -0.78f, 0.78f },
              { -1.0f, 0.0f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 1.0f, 0.0f },
              { 0.78f, 0.78f }, { 0.0f, 1.0f },
              { -0.78f, 0.78f }, { -1.0f, 0.0f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 1.0f, -0.34f },
              { 1.0f, 0.34f }, { 0.78f, 0.78f },
              { 0.0f, 1.0f }, { -0.78f, 0.78f },
              { -1.0f, 0.0f } },
        };
    float x = 0.0f;
    float y = -1.0f;

    if (count < 1)
        count = 1;
    if (count > SDL_PLAYER_ACTION_MAX)
        count = SDL_PLAYER_ACTION_MAX;
    if (slot >= 0 && slot < count) {
        x = offsets[count][slot][0];
        y = offsets[count][slot][1];
    }

    if (out_x)
        *out_x = x;
    if (out_y)
        *out_y = y;
}

bool sdl_player_action_menu_layout(player_action_menu_entry* entries,
    int* out_count)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    SDL_FRect player_rect;
    SDL_FRect bounds;
    float center_x;
    float center_y;
    float outer_radius;
    float inner_radius;
    float icon_size;
    float icon_radius;
    float max_outer;
    float step;
    float first_start;
    int count;

    if (!p_ptr || !entries || !out_count)
        return false;
    if (!sdl_player_map_rect(p_ptr->py, p_ptr->px, &player_rect))
        return false;

    bounds = (SDL_FRect) {
        .x = (float)(view->rect.x + view->margin_x),
        .y = (float)(view->rect.y + view->margin_y),
        .w = (float)(sdl_main_view_visual_cols(view) * view->cell_w),
        .h = (float)(sdl_main_view_visual_rows(view) * view->cell_h),
    };
    if (bounds.w <= 44.0f || bounds.h <= 44.0f)
        return false;

    count = sdl_player_action_menu_collect(entries);
    if (count <= 0)
        return false;

    max_outer = ((bounds.w < bounds.h) ? bounds.w : bounds.h) * 0.5f - 8.0f;
    outer_radius = sdl_touch_pane_clampf((float)view->cell_h * 5.1f,
        96.0f, 176.0f);
    if (outer_radius > max_outer)
        outer_radius = max_outer;
    if (outer_radius < 44.0f)
        return false;

    center_x = player_rect.x + player_rect.w * 0.5f;
    center_y = player_rect.y + player_rect.h * 0.5f;
    center_x = sdl_touch_pane_clampf(center_x, bounds.x + outer_radius,
        bounds.x + bounds.w - outer_radius);
    center_y = sdl_touch_pane_clampf(center_y, bounds.y + outer_radius,
        bounds.y + bounds.h - outer_radius);

    inner_radius = outer_radius * 0.36f;
    if (inner_radius < 30.0f)
        inner_radius = 30.0f;
    if (inner_radius > outer_radius - 34.0f)
        inner_radius = outer_radius - 34.0f;

    icon_size = sdl_touch_pane_clampf((outer_radius - inner_radius) * 0.52f,
        26.0f, 56.0f);
    icon_radius = inner_radius + (outer_radius - inner_radius) * 0.58f;
    step = (SDL_PLAYER_ACTION_MENU_PI * 2.0f) / (float)count;
    first_start = -SDL_PLAYER_ACTION_MENU_PI * 0.5f - step * 0.5f;

    for (int i = 0; i < count; i++) {
        float start = first_start + (float)i * step;
        float end = start + step;
        float mid = (start + end) * 0.5f;
        float icon_x = center_x + SDL_cosf(mid) * icon_radius;
        float icon_y = center_y + SDL_sinf(mid) * icon_radius;
        SDL_FRect rect;

        rect.w = icon_size;
        rect.h = icon_size;
        rect.x = icon_x - icon_size * 0.5f;
        rect.y = icon_y - icon_size * 0.5f;
        entries[i].rect = rect;
        entries[i].center_x = center_x;
        entries[i].center_y = center_y;
        entries[i].inner_radius = inner_radius;
        entries[i].outer_radius = outer_radius;
        entries[i].start_angle = start;
        entries[i].end_angle = end;
    }

    *out_count = count;
    return true;
}

int sdl_player_action_menu_kind_at(float x, float y)
{
    player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = 0;
    float dx;
    float dy;
    float dist;
    float angle;
    float step;
    float first_start;
    int index;

    if (!sdl_player_action_menu_layout(entries, &count))
        return SDL_PLAYER_ACTION_NONE;
    if (count <= 0)
        return SDL_PLAYER_ACTION_NONE;

    dx = x - entries[0].center_x;
    dy = y - entries[0].center_y;
    dist = SDL_sqrtf(dx * dx + dy * dy);
    if (dist < entries[0].inner_radius || dist > entries[0].outer_radius)
        return SDL_PLAYER_ACTION_NONE;

    angle = SDL_atan2f(dy, dx);
    step = (SDL_PLAYER_ACTION_MENU_PI * 2.0f) / (float)count;
    first_start = entries[0].start_angle;
    while (angle < first_start)
        angle += SDL_PLAYER_ACTION_MENU_PI * 2.0f;
    index = (int)((angle - first_start) / step);
    if (index < 0 || index >= count)
        return SDL_PLAYER_ACTION_NONE;

    return entries[index].kind;
}

void sdl_player_action_menu_cancel_press(void)
{
    g_player_action_menu.press_active = false;
    g_player_action_menu.press_mouse = false;
    g_player_action_menu.press_gamepad = false;
    g_player_action_menu.press_secondary = false;
    g_player_action_menu.press_finger_id = 0;
    g_player_action_menu.press_button = -1;
    g_player_action_menu.press_kind = SDL_PLAYER_ACTION_NONE;
    g_player_action_menu.press_start_x = 0.0f;
    g_player_action_menu.press_start_y = 0.0f;
    g_player_action_menu.press_start_time = 0;
}

void sdl_player_action_menu_cancel(void)
{
    bool was_active = g_player_action_menu.active;

    g_player_action_menu.active = false;
    g_player_action_menu.hover_kind = SDL_PLAYER_ACTION_NONE;
    sdl_player_action_menu_cancel_press();

    if (was_active)
        g_state.need_present = true;
}

bool sdl_player_exchange_target_valid(int y, int x, int* out_m_idx)
{
    int m_idx = 0;
    monster_type* m_ptr;
    monster_race* r_ptr;

    if (out_m_idx)
        *out_m_idx = 0;
    if (!p_ptr || !p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
        return false;
    if (distance(p_ptr->py, p_ptr->px, y, x) != 1)
        return false;
    if (!sdl_mouse_grid_has_visible_monster(y, x, &m_idx))
        return false;

    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx)
        return false;
    r_ptr = &r_info[m_ptr->r_idx];
    if (r_ptr->flags1 & (RF1_NEVER_MOVE | RF1_HIDDEN_MOVE))
        return false;

    if (out_m_idx)
        *out_m_idx = m_idx;
    return true;
}

bool sdl_player_exchange_has_any_target(void)
{
    for (int d = 1; d <= 9; d++) {
        int y;
        int x;

        if (d == 5)
            continue;
        y = p_ptr->py + ddy[d];
        x = p_ptr->px + ddx[d];
        if (sdl_player_exchange_target_valid(y, x, NULL))
            return true;
    }

    return false;
}

int sdl_player_exchange_collect_targets(
    player_exchange_target_entry* entries, int max_entries)
{
    static const int dir_order[SDL_PLAYER_EXCHANGE_MAX_TARGETS] =
        { 8, 9, 6, 3, 2, 1, 4, 7 };
    int count = 0;

    if (!p_ptr || !entries || max_entries <= 0)
        return 0;

    for (int i = 0; i < SDL_PLAYER_EXCHANGE_MAX_TARGETS
         && count < max_entries; i++)
    {
        int dir = dir_order[i];
        int y = p_ptr->py + ddy[dir];
        int x = p_ptr->px + ddx[dir];
        int m_idx = 0;

        if (!sdl_player_exchange_target_valid(y, x, &m_idx))
            continue;

        entries[count].y = y;
        entries[count].x = x;
        entries[count].m_idx = m_idx;
        count++;
    }

    return count;
}

int sdl_player_exchange_target_index(
    const player_exchange_target_entry* entries, int count, int y, int x,
    int m_idx)
{
    if (!entries || count <= 0)
        return -1;

    for (int i = 0; i < count; i++) {
        if (entries[i].y == y && entries[i].x == x
            && entries[i].m_idx == m_idx)
        {
            return i;
        }
    }

    return -1;
}

void sdl_player_exchange_set_hover(
    const player_exchange_target_entry* entry)
{
    if (!entry)
        return;

    if (!g_player_exchange_target.hover_active
        || g_player_exchange_target.hover_y != entry->y
        || g_player_exchange_target.hover_x != entry->x
        || g_player_exchange_target.hover_m_idx != entry->m_idx)
    {
        g_player_exchange_target.hover_active = true;
        g_player_exchange_target.hover_y = entry->y;
        g_player_exchange_target.hover_x = entry->x;
        g_player_exchange_target.hover_m_idx = entry->m_idx;
        g_state.need_present = true;
    }
}

void sdl_player_exchange_select_default(void)
{
    player_exchange_target_entry entries[SDL_PLAYER_EXCHANGE_MAX_TARGETS];
    int count = sdl_player_exchange_collect_targets(entries,
        SDL_PLAYER_EXCHANGE_MAX_TARGETS);

    if (count > 0)
        sdl_player_exchange_set_hover(&entries[0]);
}

void sdl_player_exchange_cancel_press(void)
{
    g_player_exchange_target.press_active = false;
    g_player_exchange_target.press_mouse = false;
    g_player_exchange_target.press_finger_id = 0;
    g_player_exchange_target.press_y = 0;
    g_player_exchange_target.press_x = 0;
    g_player_exchange_target.press_start_x = 0.0f;
    g_player_exchange_target.press_start_y = 0.0f;
}

void sdl_player_exchange_cancel(void)
{
    bool was_active = g_player_exchange_target.active;

    g_player_exchange_target.active = false;
    g_player_exchange_target.hover_active = false;
    g_player_exchange_target.hover_y = 0;
    g_player_exchange_target.hover_x = 0;
    g_player_exchange_target.hover_m_idx = 0;
    g_player_exchange_target.selected = false;
    g_player_exchange_target.selected_y = 0;
    g_player_exchange_target.selected_x = 0;
    g_player_exchange_target.selected_m_idx = 0;
    sdl_player_exchange_cancel_press();

    if (was_active)
        g_state.need_present = true;
}

bool sdl_player_exchange_context_active(void)
{
    return g_player_exchange_target.active
        && character_generated
        && character_dungeon
        && p_ptr
        && character_icky == 0
        && !ui_menu_click_is_active()
        && g_views[PANE_MAIN].term_ready;
}

bool sdl_player_exchange_begin(bool report_no_target)
{
    if (!p_ptr || !p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
        return false;
    if (!sdl_player_exchange_has_any_target()) {
        if (report_no_target)
            bell("No adjacent exchange target.");
        return false;
    }

    sdl_player_action_menu_cancel();
    sdl_mouse_path_cancel();
    memset(&g_player_exchange_target, 0, sizeof(g_player_exchange_target));
    g_gamepad_state.dpad_up = false;
    g_gamepad_state.dpad_down = false;
    g_gamepad_state.dpad_left = false;
    g_gamepad_state.dpad_right = false;
    g_gamepad_state.dpad_dir = 0;
    sdl_gamepad_clear_pending_dpad();
    sdl_gamepad_clear_pending_left_stick();
    g_player_exchange_target.active = true;
    sdl_player_exchange_select_default();
    g_state.need_present = true;
    return true;
}

void sdl_player_exchange_begin_direction_prompt(void)
{
    (void)sdl_player_exchange_begin(false);
}

void sdl_player_exchange_cancel_direction_prompt(void)
{
    sdl_player_exchange_cancel();
}

void sdl_player_exchange_update_hover(float x, float y)
{
    int map_y = 0;
    int map_x = 0;
    int m_idx = 0;
    bool hover_active = false;

    if (sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        && sdl_player_exchange_target_valid(map_y, map_x, &m_idx))
    {
        hover_active = true;
    }

    if (g_player_exchange_target.hover_active != hover_active
        || g_player_exchange_target.hover_y != map_y
        || g_player_exchange_target.hover_x != map_x
        || g_player_exchange_target.hover_m_idx != m_idx)
    {
        g_player_exchange_target.hover_active = hover_active;
        g_player_exchange_target.hover_y = hover_active ? map_y : 0;
        g_player_exchange_target.hover_x = hover_active ? map_x : 0;
        g_player_exchange_target.hover_m_idx = hover_active ? m_idx : 0;
        g_state.need_present = true;
    }
}

bool sdl_player_exchange_same_selection(int y, int x, int m_idx)
{
    return g_player_exchange_target.selected
        && g_player_exchange_target.selected_y == y
        && g_player_exchange_target.selected_x == x
        && g_player_exchange_target.selected_m_idx == m_idx;
}

void sdl_player_exchange_execute(int y, int x)
{
    int dir = motion_dir(p_ptr->py, p_ptr->px, y, x);

    if (dir == 5)
        return;

    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    if (!p_ptr || p_ptr->command_cmd != 'X')
        sdl_enqueue_bypassed_command('X');
    Term_keypress('0' + dir);
}

void sdl_player_exchange_select_or_execute(int y, int x, int m_idx)
{
    if (sdl_player_exchange_same_selection(y, x, m_idx)) {
        sdl_player_exchange_execute(y, x);
        return;
    }

    g_player_exchange_target.selected = true;
    g_player_exchange_target.selected_y = y;
    g_player_exchange_target.selected_x = x;
    g_player_exchange_target.selected_m_idx = m_idx;
    g_state.need_present = true;
}

void sdl_player_exchange_move_hover(int delta)
{
    player_exchange_target_entry entries[SDL_PLAYER_EXCHANGE_MAX_TARGETS];
    int count = sdl_player_exchange_collect_targets(entries,
        SDL_PLAYER_EXCHANGE_MAX_TARGETS);
    int index = -1;

    if (count <= 0)
        return;

    if (g_player_exchange_target.hover_active) {
        index = sdl_player_exchange_target_index(entries, count,
            g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x,
            g_player_exchange_target.hover_m_idx);
    }
    if (index < 0 && g_player_exchange_target.selected) {
        index = sdl_player_exchange_target_index(entries, count,
            g_player_exchange_target.selected_y,
            g_player_exchange_target.selected_x,
            g_player_exchange_target.selected_m_idx);
    }

    if (index < 0) {
        index = (delta < 0) ? count - 1 : 0;
    } else {
        index = (index + delta) % count;
        if (index < 0)
            index += count;
    }

    sdl_player_exchange_set_hover(&entries[index]);
}

void sdl_player_exchange_activate_hover(void)
{
    int m_idx = 0;

    if (g_player_exchange_target.hover_active
        && sdl_player_exchange_target_valid(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x, &m_idx))
    {
        sdl_player_exchange_execute(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x);
        return;
    }

    if (g_player_exchange_target.selected
        && sdl_player_exchange_target_valid(g_player_exchange_target.selected_y,
            g_player_exchange_target.selected_x, &m_idx))
    {
        sdl_player_exchange_execute(g_player_exchange_target.selected_y,
            g_player_exchange_target.selected_x);
        return;
    }

    sdl_player_exchange_select_default();
    if (g_player_exchange_target.hover_active
        && sdl_player_exchange_target_valid(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x, &m_idx))
    {
        sdl_player_exchange_execute(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x);
        return;
    }

    bell("Choose an adjacent monster.");
}

void sdl_player_action_menu_activate_kind(int kind, bool secondary)
{
    int command = 0;
    bool select_floor = false;

    switch (kind) {
    case SDL_PLAYER_ACTION_EXCHANGE:
        (void)sdl_player_exchange_begin(true);
        return;
    case SDL_PLAYER_ACTION_WAIT:
        command = secondary ? 'Z' : 'z';
        break;
    case SDL_PLAYER_ACTION_USE:
        command = 'u';
        select_floor = !secondary && sdl_player_has_floor_item_underfoot();
        break;
    case SDL_PLAYER_ACTION_STEALTH:
        command = 'S';
        break;
    case SDL_PLAYER_ACTION_SING:
        command = 's';
        break;
    case SDL_PLAYER_ACTION_FLETCH:
        command = '-';
        break;
    case SDL_PLAYER_ACTION_EXAMINE:
        command = 'x';
        select_floor = !secondary && sdl_player_has_floor_item_underfoot();
        break;
    case SDL_PLAYER_ACTION_ACTIVATE:
        command = 'a';
        break;
    case SDL_PLAYER_ACTION_HORN:
        command = 'p';
        break;
    default:
        return;
    }

    sdl_player_action_menu_cancel();
    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    sdl_enqueue_bypassed_command(command);
    if (select_floor)
        Term_keypress('-');
}

bool sdl_player_action_menu_open(void)
{
    if (!sdl_main_screen_click_shortcuts_active())
        return false;

    sdl_player_exchange_cancel();
    sdl_mouse_path_cancel();
    sdl_player_action_menu_cancel_press();
    g_gamepad_state.dpad_up = false;
    g_gamepad_state.dpad_down = false;
    g_gamepad_state.dpad_left = false;
    g_gamepad_state.dpad_right = false;
    g_gamepad_state.dpad_dir = 0;
    sdl_gamepad_clear_pending_dpad();
    sdl_gamepad_clear_pending_left_stick();
    g_player_action_menu.active = true;
    g_player_action_menu.hover_kind = SDL_PLAYER_ACTION_NONE;
    g_state.need_present = true;
    return true;
}

bool sdl_player_action_menu_handle_gamepad_button(
    SDL_GamepadButton button, bool down)
{
    if (!g_player_action_menu.active)
        return false;

    if (!sdl_main_screen_click_shortcuts_active()) {
        sdl_player_action_menu_cancel();
        return true;
    }

    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        if (down) {
            if (g_player_action_menu.press_active
                && g_player_action_menu.press_gamepad)
            {
                sdl_player_action_menu_cancel_press();
            }
            sdl_player_action_menu_move_hover(-1);
        }
        return true;

    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        if (down) {
            if (g_player_action_menu.press_active
                && g_player_action_menu.press_gamepad)
            {
                sdl_player_action_menu_cancel_press();
            }
            sdl_player_action_menu_move_hover(1);
        }
        return true;

    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_START:
    case SDL_GAMEPAD_BUTTON_BACK:
        if (down)
            sdl_player_action_menu_cancel();
        return true;

    default:
        break;
    }

    if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT
        && sdl_gamepad_action_is_confirm(config.gamepad_button_bindings[button]))
    {
        return sdl_player_action_menu_handle_gamepad_confirm(
            (int)button, down);
    }

    return false;
}

bool sdl_player_exchange_handle_gamepad_button(
    SDL_GamepadButton button, bool down)
{
    if (!g_player_exchange_target.active)
        return false;

    if (!sdl_player_exchange_context_active()) {
        sdl_player_exchange_cancel();
        return true;
    }

    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        if (down)
            sdl_player_exchange_move_hover(-1);
        return true;

    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        if (down)
            sdl_player_exchange_move_hover(1);
        return true;

    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_START:
    case SDL_GAMEPAD_BUTTON_BACK:
        if (down)
            sdl_player_exchange_cancel();
        return true;

    default:
        break;
    }

    if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT
        && sdl_gamepad_action_is_confirm(config.gamepad_button_bindings[button]))
    {
        if (down)
            sdl_player_exchange_activate_hover();
        return true;
    }

    return false;
}

bool sdl_player_action_menu_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse, bool secondary)
{
    int kind;

    if (!g_player_action_menu.active)
        return false;
    if (!sdl_main_screen_click_shortcuts_active()) {
        sdl_player_action_menu_cancel();
        return true;
    }

    kind = sdl_player_action_menu_kind_at(x, y);
    if (kind == SDL_PLAYER_ACTION_NONE) {
        sdl_player_action_menu_cancel();
        return true;
    }
    if (secondary && !sdl_player_action_menu_kind_supports_secondary(kind)) {
        sdl_player_action_menu_cancel();
        return true;
    }

    sdl_player_action_menu_cancel_press();
    g_player_action_menu.press_active = true;
    g_player_action_menu.press_mouse = mouse;
    g_player_action_menu.press_secondary = secondary;
    g_player_action_menu.press_finger_id = finger_id;
    g_player_action_menu.press_kind = kind;
    g_player_action_menu.press_start_x = x;
    g_player_action_menu.press_start_y = y;
    g_player_action_menu.press_start_time = SDL_GetTicksNS();
    g_player_action_menu.hover_kind = kind;
    g_state.need_present = true;
    return true;
}

bool sdl_player_action_menu_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int kind;
    float dx;
    float dy;

    if (!g_player_action_menu.active)
        return false;
    if (!sdl_main_screen_click_shortcuts_active()) {
        sdl_player_action_menu_cancel();
        return true;
    }

    kind = sdl_player_action_menu_kind_at(x, y);
    if (g_player_action_menu.hover_kind != kind) {
        g_player_action_menu.hover_kind = kind;
        g_state.need_present = true;
    }

    if (!g_player_action_menu.press_active)
        return true;
    if (g_player_action_menu.press_gamepad)
        return true;
    if (g_player_action_menu.press_mouse != mouse
        || g_player_action_menu.press_finger_id != finger_id)
    {
        return true;
    }

    dx = x - g_player_action_menu.press_start_x;
    dy = y - g_player_action_menu.press_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (kind != g_player_action_menu.press_kind
        || dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        sdl_player_action_menu_cancel_press();
    }

    return true;
}

bool sdl_player_action_menu_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse, bool secondary)
{
    int kind;
    bool activate_secondary;

    if (!g_player_action_menu.active)
        return false;
    if (!sdl_main_screen_click_shortcuts_active()) {
        sdl_player_action_menu_cancel();
        return true;
    }
    if (!g_player_action_menu.press_active)
        return true;
    if (g_player_action_menu.press_gamepad
        || g_player_action_menu.press_mouse != mouse
        || g_player_action_menu.press_secondary != secondary
        || g_player_action_menu.press_finger_id != finger_id)
    {
        return true;
    }

    kind = sdl_player_action_menu_kind_at(x, y);
    if (kind == g_player_action_menu.press_kind) {
        activate_secondary = g_player_action_menu.press_secondary;
        if (!mouse && sdl_player_action_menu_kind_supports_secondary(kind)
            && g_player_action_menu.press_start_time
            && SDL_GetTicksNS() - g_player_action_menu.press_start_time
                >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        {
            activate_secondary = true;
        }
        sdl_player_action_menu_activate_kind(kind, activate_secondary);
    } else {
        sdl_player_action_menu_cancel_press();
    }

    return true;
}

int sdl_player_action_menu_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_player_action_menu.active)
        return -1;
    if (!g_player_action_menu.press_active
        || g_player_action_menu.press_mouse
        || g_player_action_menu.press_secondary
        || !sdl_player_action_menu_kind_supports_secondary(
            g_player_action_menu.press_kind)
        || !g_player_action_menu.press_start_time)
    {
        return -1;
    }

    elapsed = now_ns - g_player_action_menu.press_start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_player_action_menu_flush_pending_press(Uint64 now_ns)
{
    int kind;

    if (sdl_player_action_menu_pending_timeout_ms(now_ns) != 0)
        return false;

    kind = g_player_action_menu.press_kind;
    sdl_player_action_menu_activate_kind(kind, true);
    return true;
}

bool sdl_player_exchange_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int map_y = 0;
    int map_x = 0;

    if (!g_player_exchange_target.active)
        return false;
    if (!sdl_player_exchange_context_active()) {
        sdl_player_exchange_cancel();
        return true;
    }

    sdl_player_exchange_update_hover(x, y);
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)) {
        sdl_player_exchange_cancel();
        return true;
    }

    sdl_player_exchange_cancel_press();
    g_player_exchange_target.press_active = true;
    g_player_exchange_target.press_mouse = mouse;
    g_player_exchange_target.press_finger_id = finger_id;
    g_player_exchange_target.press_y = map_y;
    g_player_exchange_target.press_x = map_x;
    g_player_exchange_target.press_start_x = x;
    g_player_exchange_target.press_start_y = y;
    return true;
}

bool sdl_player_exchange_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int map_y = 0;
    int map_x = 0;
    float dx;
    float dy;

    if (!g_player_exchange_target.active)
        return false;
    if (!sdl_player_exchange_context_active()) {
        sdl_player_exchange_cancel();
        return true;
    }

    sdl_player_exchange_update_hover(x, y);

    if (!g_player_exchange_target.press_active)
        return true;
    if (g_player_exchange_target.press_mouse != mouse
        || g_player_exchange_target.press_finger_id != finger_id)
    {
        return true;
    }

    dx = x - g_player_exchange_target.press_start_x;
    dy = y - g_player_exchange_target.press_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        || map_y != g_player_exchange_target.press_y
        || map_x != g_player_exchange_target.press_x
        || dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        sdl_player_exchange_cancel_press();
    }

    return true;
}

bool sdl_player_exchange_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int map_y = 0;
    int map_x = 0;
    int m_idx = 0;

    if (!g_player_exchange_target.active)
        return false;
    if (!sdl_player_exchange_context_active()) {
        sdl_player_exchange_cancel();
        return true;
    }
    if (!g_player_exchange_target.press_active)
        return true;
    if (g_player_exchange_target.press_mouse != mouse
        || g_player_exchange_target.press_finger_id != finger_id)
    {
        return true;
    }

    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        || map_y != g_player_exchange_target.press_y
        || map_x != g_player_exchange_target.press_x)
    {
        sdl_player_exchange_cancel_press();
        return true;
    }

    sdl_player_exchange_cancel_press();
    if (sdl_player_exchange_target_valid(map_y, map_x, &m_idx)) {
        if (mouse)
            sdl_player_exchange_execute(map_y, map_x);
        else
            sdl_player_exchange_select_or_execute(map_y, map_x, m_idx);
    } else {
        bell("Choose an adjacent monster.");
    }

    return true;
}

void sdl_player_exchange_render_cell(int y, int x, SDL_Color color,
    int alpha, bool thick)
{
    SDL_FRect rect;

    if (!sdl_player_map_rect(y, x, &rect))
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, alpha);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        thick ? 235 : 170);
    SDL_RenderRect(g_state.renderer, &rect);
    if (thick) {
        rect.x += 1.0f;
        rect.y += 1.0f;
        rect.w -= 2.0f;
        rect.h -= 2.0f;
        if (rect.w > 1.0f && rect.h > 1.0f)
            SDL_RenderRect(g_state.renderer, &rect);
    }
}

void sdl_player_exchange_render(void)
{
    SDL_Color available = g_state.palette[TERM_L_BLUE];
    SDL_Color hover = g_state.palette[TERM_L_RED];
    SDL_Color selected = g_state.palette[TERM_YELLOW];

    if (!g_player_exchange_target.active)
        return;
    if (!sdl_player_exchange_context_active())
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    for (int d = 1; d <= 9; d++) {
        int y;
        int x;

        if (d == 5)
            continue;
        y = p_ptr->py + ddy[d];
        x = p_ptr->px + ddx[d];
        if (!sdl_player_exchange_target_valid(y, x, NULL))
            continue;

        sdl_player_exchange_render_cell(y, x, available, 54, false);
    }

    if (g_player_exchange_target.selected) {
        sdl_player_exchange_render_cell(g_player_exchange_target.selected_y,
            g_player_exchange_target.selected_x, selected, 96, true);
    }
    if (g_player_exchange_target.hover_active) {
        sdl_player_exchange_render_cell(g_player_exchange_target.hover_y,
            g_player_exchange_target.hover_x, hover, 82, true);
    }
}

static SDL_FColor sdl_player_action_menu_fcolor(SDL_Color color)
{
    return (SDL_FColor){
        (float)color.r / 255.0f,
        (float)color.g / 255.0f,
        (float)color.b / 255.0f,
        (float)color.a / 255.0f,
    };
}

static void sdl_player_action_menu_render_sector(
    const player_action_menu_entry* entry, SDL_Color fill)
{
    SDL_Vertex vertices[(SDL_PLAYER_ACTION_MENU_SECTOR_SEGMENTS + 1) * 2];
    int indices[SDL_PLAYER_ACTION_MENU_SECTOR_SEGMENTS * 6];
    SDL_FColor color = sdl_player_action_menu_fcolor(fill);
    int vcount = 0;
    int icount = 0;

    if (!entry)
        return;

    for (int i = 0; i <= SDL_PLAYER_ACTION_MENU_SECTOR_SEGMENTS; i++) {
        float t = (float)i / (float)SDL_PLAYER_ACTION_MENU_SECTOR_SEGMENTS;
        float angle = entry->start_angle
            + (entry->end_angle - entry->start_angle) * t;
        float ct = SDL_cosf(angle);
        float st = SDL_sinf(angle);
        SDL_FPoint inner = {
            entry->center_x + ct * entry->inner_radius,
            entry->center_y + st * entry->inner_radius,
        };
        SDL_FPoint outer = {
            entry->center_x + ct * entry->outer_radius,
            entry->center_y + st * entry->outer_radius,
        };

        vertices[vcount++] = (SDL_Vertex){ inner, color, { 0.0f, 0.0f } };
        vertices[vcount++] = (SDL_Vertex){ outer, color, { 0.0f, 0.0f } };
    }

    for (int i = 0; i < SDL_PLAYER_ACTION_MENU_SECTOR_SEGMENTS; i++) {
        int inner_a = i * 2;
        int outer_a = inner_a + 1;
        int inner_b = inner_a + 2;
        int outer_b = inner_a + 3;

        indices[icount++] = inner_a;
        indices[icount++] = outer_a;
        indices[icount++] = inner_b;
        indices[icount++] = inner_b;
        indices[icount++] = outer_a;
        indices[icount++] = outer_b;
    }

    SDL_RenderGeometry(g_state.renderer, NULL, vertices, vcount, indices,
        icount);
}

static void sdl_player_action_menu_render_separator(
    const player_action_menu_entry* entry, float angle, SDL_Color color)
{
    float ct;
    float st;

    if (!entry)
        return;

    ct = SDL_cosf(angle);
    st = SDL_sinf(angle);
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        color.a);
    SDL_RenderLine(g_state.renderer,
        entry->center_x + ct * entry->inner_radius,
        entry->center_y + st * entry->inner_radius,
        entry->center_x + ct * entry->outer_radius,
        entry->center_y + st * entry->outer_radius);
}

static bool sdl_player_action_menu_can_draw_tiles(void)
{
    return g_state.use_tiles && g_state.tileset;
}

static void sdl_player_action_menu_render_icon(
    const player_action_menu_entry* entry, bool hover)
{
    SDL_FRect rect;
    SDL_Color text = g_state.palette[TERM_WHITE];
    float grow;

    if (!entry)
        return;

    rect = entry->rect;
    grow = hover ? sdl_touch_pane_clampf(rect.w * 0.12f, 3.0f, 7.0f) : 0.0f;
    rect.x -= grow;
    rect.y -= grow;
    rect.w += grow * 2.0f;
    rect.h += grow * 2.0f;

    if (sdl_player_action_menu_can_draw_tiles()) {
        if (hover)
            SDL_SetTextureAlphaMod(g_state.tileset, 255);
        else
            SDL_SetTextureAlphaMod(g_state.tileset, 226);
        sdl_draw_tileset_sprite(entry->tile_attr, entry->tile_char, &rect,
            false);
        sdl_restore_tileset_mod();
    } else {
        sdl_touch_pane_draw_button_text_scaled(&rect, NULL,
            entry->fallback ? entry->fallback : entry->label, text, 0.34f,
            0.48f);
    }
}

static bool sdl_player_action_menu_rects_intersect(
    const SDL_FRect* a, const SDL_FRect* b)
{
    if (!a || !b)
        return false;

    return a->x < b->x + b->w && a->x + a->w > b->x
        && a->y < b->y + b->h && a->y + a->h > b->y;
}

static bool sdl_player_action_menu_tooltip_candidate(SDL_FRect* box,
    const SDL_FRect* avoid, float x, float y, const SDL_Rect* screen,
    float screen_margin)
{
    SDL_FRect candidate;

    if (!box || !avoid || !screen)
        return false;

    candidate = *box;
    candidate.x = x;
    candidate.y = y;

    if (candidate.w + screen_margin * 2.0f <= (float)screen->w) {
        candidate.x = sdl_touch_pane_clampf(candidate.x,
            (float)screen->x + screen_margin,
            (float)(screen->x + screen->w) - candidate.w - screen_margin);
    } else {
        candidate.x = (float)screen->x + screen_margin;
    }

    if (candidate.h + screen_margin * 2.0f <= (float)screen->h) {
        candidate.y = sdl_touch_pane_clampf(candidate.y,
            (float)screen->y + screen_margin,
            (float)(screen->y + screen->h) - candidate.h - screen_margin);
    } else {
        candidate.y = (float)screen->y + screen_margin;
    }

    if (sdl_player_action_menu_rects_intersect(&candidate, avoid))
        return false;

    *box = candidate;
    return true;
}

static void sdl_player_action_menu_place_tooltip(SDL_FRect* box,
    const player_action_menu_entry* entry, const SDL_Rect* screen,
    float screen_margin, float gap)
{
    SDL_FRect avoid;
    float dx;
    float dy;
    float left_x;
    float right_x;
    float above_y;
    float below_y;
    float center_x;
    float center_y;
    int order[4];

    if (!box || !entry || !screen)
        return;

    avoid = (SDL_FRect){
        .x = entry->center_x - entry->outer_radius - gap,
        .y = entry->center_y - entry->outer_radius - gap,
        .w = (entry->outer_radius + gap) * 2.0f,
        .h = (entry->outer_radius + gap) * 2.0f,
    };

    dx = (entry->rect.x + entry->rect.w * 0.5f) - entry->center_x;
    dy = (entry->rect.y + entry->rect.h * 0.5f) - entry->center_y;
    center_x = entry->center_x - box->w * 0.5f;
    center_y = entry->center_y - box->h * 0.5f;
    left_x = avoid.x - box->w;
    right_x = avoid.x + avoid.w;
    above_y = avoid.y - box->h;
    below_y = avoid.y + avoid.h;

    if (SDL_fabsf(dx) >= SDL_fabsf(dy)) {
        order[0] = (dx >= 0.0f) ? 0 : 1;
        order[1] = (dx >= 0.0f) ? 1 : 0;
        order[2] = (dy >= 0.0f) ? 3 : 2;
        order[3] = (dy >= 0.0f) ? 2 : 3;
    } else {
        order[0] = (dy >= 0.0f) ? 3 : 2;
        order[1] = (dy >= 0.0f) ? 2 : 3;
        order[2] = (dx >= 0.0f) ? 0 : 1;
        order[3] = (dx >= 0.0f) ? 1 : 0;
    }

    for (int i = 0; i < 4; i++) {
        switch (order[i]) {
        case 0:
            if (sdl_player_action_menu_tooltip_candidate(box, &avoid,
                    right_x, center_y, screen, screen_margin))
                return;
            break;
        case 1:
            if (sdl_player_action_menu_tooltip_candidate(box, &avoid,
                    left_x, center_y, screen, screen_margin))
                return;
            break;
        case 2:
            if (sdl_player_action_menu_tooltip_candidate(box, &avoid,
                    center_x, above_y, screen, screen_margin))
                return;
            break;
        case 3:
            if (sdl_player_action_menu_tooltip_candidate(box, &avoid,
                    center_x, below_y, screen, screen_margin))
                return;
            break;
        default:
            break;
        }
    }

    box->x = center_x;
    box->y = below_y;
    (void)sdl_player_action_menu_tooltip_candidate(box, &avoid, box->x,
        box->y, screen, screen_margin);
}

static void sdl_player_action_menu_render_tooltip(
    const player_action_menu_entry* entry)
{
    SDL_Rect screen;
    TTF_Font* font;
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect box;
    SDL_FRect text_dst;
    SDL_Color text_color = g_state.palette[TERM_WHITE];
    SDL_Color border = g_state.palette[TERM_YELLOW];
    float pad;
    float gap;
    float screen_margin;
    float max_box_w;
    float max_text_w;
    int font_px;

    if (!entry || !entry->description || !entry->description[0])
        return;

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
    surface = sdl_object_tooltip_render_text_surface(font,
        entry->description, text_color, max_text_w);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }

    box.w = (float)surface->w + pad * 2.0f;
    box.h = (float)surface->h + pad * 2.0f;
    sdl_player_action_menu_place_tooltip(&box, entry, &screen, screen_margin,
        gap);

    text_dst = (SDL_FRect){
        .x = box.x + pad,
        .y = box.y + pad,
        .w = (float)surface->w,
        .h = (float)surface->h,
    };

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 218);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
        166);
    SDL_RenderRect(g_state.renderer, &box);

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &text_dst);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

void sdl_player_action_menu_render(void)
{
    player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = 0;
    SDL_Rect clip;
    SDL_Color bg = { 22, 24, 26, 162 };
    SDL_Color hover_bg = { 88, 82, 58, 216 };
    SDL_Color border = { 188, 202, 210, 150 };
    SDL_Color hover_border = g_state.palette[TERM_YELLOW];
    const player_action_menu_entry* hovered = NULL;

    if (!g_player_action_menu.active)
        return;
    if (!sdl_main_screen_click_shortcuts_active())
        return;
    if (!sdl_player_action_menu_layout(entries, &count))
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    clip = g_views[PANE_MAIN].rect;
    SDL_SetRenderClipRect(g_state.renderer, &clip);

    for (int i = 0; i < count; i++) {
        bool hover = entries[i].kind == g_player_action_menu.hover_kind;
        SDL_Color fill = hover ? hover_bg : bg;

        sdl_player_action_menu_render_sector(&entries[i], fill);
        if (hover)
            hovered = &entries[i];
    }

    if (count > 0) {
        sdl_touch_round_draw_circle(entries[0].center_x, entries[0].center_y,
            entries[0].outer_radius, border);
        sdl_touch_round_draw_circle(entries[0].center_x, entries[0].center_y,
            entries[0].outer_radius - 2.0f, border);
        sdl_touch_round_draw_circle(entries[0].center_x, entries[0].center_y,
            entries[0].inner_radius, border);
        for (int i = 0; i < count; i++) {
            sdl_player_action_menu_render_separator(&entries[i],
                entries[i].start_angle, border);
        }
    }

    if (hovered) {
        sdl_touch_round_draw_circle(hovered->center_x, hovered->center_y,
            hovered->outer_radius, hover_border);
        sdl_touch_round_draw_circle(hovered->center_x, hovered->center_y,
            hovered->inner_radius, hover_border);
        sdl_player_action_menu_render_separator(hovered,
            hovered->start_angle, hover_border);
        sdl_player_action_menu_render_separator(hovered,
            hovered->end_angle, hover_border);
    }

    for (int i = 0; i < count; i++) {
        bool hover = entries[i].kind == g_player_action_menu.hover_kind;
        sdl_player_action_menu_render_icon(&entries[i], hover);
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    if (hovered)
        sdl_player_action_menu_render_tooltip(hovered);
}


