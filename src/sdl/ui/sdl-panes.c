#include "angband.h"
#include "sdl/main-sdl-private.h"

bool sdl_touch_pane_binding_is_direction(int binding)
{
    switch (binding) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '6':
    case '7':
    case '8':
    case '9':
        return true;
    default:
        return false;
    }
}

bool sdl_touch_pane_slot_uses_long_press(int slot, int binding)
{
    return (slot == 0)
        || sdl_touch_pane_binding_is_direction(binding)
        || (binding == 'z')
        || sdl_touch_pane_confirm_binding(binding);
}

bool sdl_touch_pane_confirm_binding(int binding)
{
    return (binding == INPUT_BIND_CONFIRM || binding == ' ' || binding == '\r');
}

bool sdl_touch_pane_main_panel_has_confirm_excluding(int skip_index)
{
    for (int i = 0; i < SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT; i++) {
        int slot = sdl_touch_pane_visible_slot_at(i);

        if (slot < 0 || slot == skip_index)
            continue;

        if (sdl_touch_pane_confirm_binding(config.touch_pane_bindings[slot]))
            return true;
    }

    return false;
}

void sdl_touch_pane_ensure_main_panel_confirm(void)
{
    if (sdl_touch_pane_main_panel_has_confirm_excluding(-1))
        return;

    if (config.touch_pane_bindings[SDL_TOUCH_PANE_CENTER_SLOT] != INPUT_BIND_CONFIRM)
        log_warn("Touch pane main panel had no Confirm (pick) binding; restoring center button");

    config.touch_pane_bindings[SDL_TOUCH_PANE_CENTER_SLOT] = INPUT_BIND_CONFIRM;
    clear_sdl_touch_pane_button_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN,
        SDL_TOUCH_PANE_CENTER_SLOT);
}

void sdl_touch_pane_begin_reset_confirm(void)
{
    g_touch_pane_reset_confirm_active = true;
    g_state.need_present = true;
}

void sdl_touch_pane_finish_reset_confirm(bool confirmed)
{
    g_touch_pane_reset_confirm_active = false;

    if (confirmed) {
        sdl_touch_pane_reset_bindings_to_default();
        msg_print("Touch controls reset to defaults.");
    }

    g_state.need_present = true;
}

void sdl_touch_pane_handle_reset_prompt_pointer(float x, float y)
{
    int slot = -1;
    int panel;
    int binding;

    if (!sdl_touch_pane_point_to_slot(x, y, &slot))
        return;
    if (slot < 0)
        return;

    panel = sdl_touch_pane_active_panel();
    binding = sdl_touch_pane_effective_binding_for_panel(panel, slot);

    if (slot == 0 || binding == ESCAPE) {
        sdl_touch_pane_finish_reset_confirm(false);
    } else if (sdl_touch_pane_confirm_binding(binding)) {
        sdl_touch_pane_finish_reset_confirm(true);
    }
}

bool sdl_touch_pane_compute_layout(const SDL_Rect* pane_rect, SDL_FRect* slot_rects)
{
    float gap;
    float usable_w;
    float usable_h;
    float button_from_w;
    float button_from_h;
    float button_size;
    float grid_w;
    float grid_h;
    float start_x;
    float start_y;

    if (!pane_rect || !slot_rects || pane_rect->w <= 0 || pane_rect->h <= 0)
        return false;

    gap = (float)((pane_rect->w < pane_rect->h) ? pane_rect->w : pane_rect->h) / 40.0f;
    if (gap < 4.0f)
        gap = 4.0f;
    if (gap > 12.0f)
        gap = 12.0f;

    usable_w = (float)pane_rect->w - gap * 2.0f;
    usable_h = (float)pane_rect->h - gap * 2.0f;
    button_from_w = (usable_w - gap * (SDL_TOUCH_PANE_BUTTON_COLS - 1)) / SDL_TOUCH_PANE_BUTTON_COLS;
    button_from_h = (usable_h - gap * (SDL_TOUCH_PANE_VISIBLE_BUTTON_ROWS - 1)) / SDL_TOUCH_PANE_VISIBLE_BUTTON_ROWS;
    button_size = (button_from_w < button_from_h) ? button_from_w : button_from_h;
    if (button_size < 12.0f)
        return false;

    grid_w = button_size * SDL_TOUCH_PANE_BUTTON_COLS + gap * (SDL_TOUCH_PANE_BUTTON_COLS - 1);
    grid_h = button_size * SDL_TOUCH_PANE_VISIBLE_BUTTON_ROWS + gap * (SDL_TOUCH_PANE_VISIBLE_BUTTON_ROWS - 1);
    start_x = (float)pane_rect->x + ((float)pane_rect->w - grid_w) * 0.5f;
    start_y = sdl_mobile_prefer_safe_edge_alignment()
        ? ((float)pane_rect->y + gap)
        : ((float)pane_rect->y + ((float)pane_rect->h - grid_h) * 0.5f);

    for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++)
        slot_rects[i] = (SDL_FRect){ 0 };

    for (int i = 0; i < SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT; i++) {
        int slot = sdl_touch_pane_visible_slot_at(i);
        int row = i / SDL_TOUCH_PANE_BUTTON_COLS;
        int col = i % SDL_TOUCH_PANE_BUTTON_COLS;

        if (slot < 0)
            continue;

        slot_rects[slot] = (SDL_FRect){
            .x = start_x + col * (button_size + gap),
            .y = start_y + row * (button_size + gap),
            .w = button_size,
            .h = button_size,
        };
    }

    return true;
}

bool sdl_touch_pane_current_rect(SDL_Rect* out_rect)
{
    SDL_Rect panes[PANE_MAX];
    const SDL_Rect* pane;
    bool show_supporting_panes = sdl_should_show_supporting_panes();
    bool proto_touch = sdl_touch_pane_proto_mode_active();

    if (!out_rect)
        return false;
    if (!proto_touch && !sdl_touch_pane_is_config_enabled())
        return false;
    if (!proto_touch && sdl_touch_pane_hidden_mode_active())
        return false;
    if (!proto_touch && !sdl_touch_pane_mobile_layout_open())
        return false;

    if (show_supporting_panes || sdl_layout_matches_supporting_pane_visibility()) {
        pane = &g_pane_rects[PANE_TOUCH];
    } else {
        sdl_compute_display_panes(panes);
        pane = &panes[PANE_TOUCH];
    }

    if (pane->w <= 0 || pane->h <= 0)
        return false;

    *out_rect = *pane;
    return true;
}

bool sdl_side_map_pane_current_rect(SDL_Rect* out_rect)
{
    const SDL_Rect* pane;

    if (!sdl_should_show_supporting_panes())
        return false;
    if (!sdl_layout_matches_supporting_pane_visibility())
        return false;

    pane = &g_pane_rects[PANE_MAP];
    if (pane->w <= 0 || pane->h <= 0)
        return false;

    if (out_rect)
        *out_rect = *pane;
    return true;
}

const struct pane_config* sdl_status_pane_config(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_STATUS)
            return &pane_config[i];
    }

    return NULL;
}

const struct pane_config* sdl_depth_pane_config(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_DEPTH)
            return &pane_config[i];
    }

    return NULL;
}

const struct pane_config* sdl_combat_overlay_pane_config(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_COMBAT)
            return &pane_config[i];
    }

    return NULL;
}

const struct pane_config* sdl_status_depth_pane_config(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_STATUS_DEPTH)
            return &pane_config[i];
    }

    return NULL;
}

static bool sdl_combat_overlay_adjacent_to_left_panel(
    enum pane_placement where, bool* out_combat_after_left)
{
    int left_index = -1;
    int combat_index = -1;
    int first;
    int last;

    if (out_combat_after_left)
        *out_combat_after_left = false;

    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_LEFT_PANEL)
            left_index = i;
        else if (pane_config[i].pane == PANE_COMBAT)
            combat_index = i;
    }
    if (left_index < 0 || combat_index < 0)
        return false;
    if (!pane_config[left_index].enabled || !pane_config[combat_index].enabled)
        return false;
    if (pane_config[left_index].where != where
        || pane_config[combat_index].where != where)
    {
        return false;
    }

    first = MIN(left_index, combat_index);
    last = MAX(left_index, combat_index);
    for (int i = first + 1; i < last; i++) {
        if (pane_config[i].enabled && pane_config[i].where == where)
            return false;
    }

    if (out_combat_after_left)
        *out_combat_after_left = combat_index > left_index;
    return true;
}

bool sdl_combat_overlay_connected_to_left_panel(bool* out_combat_below)
{
    const struct pane_config* pc = sdl_combat_overlay_pane_config();
    bool combat_after_left = false;

    if (out_combat_below)
        *out_combat_below = false;

    if (!pc
        || !sdl_left_panel_pane_presentation_active()
        || !sdl_combat_overlay_pane_presentation_active()
        || !sdl_combat_overlay_adjacent_to_left_panel(pc->where,
            &combat_after_left))
    {
        return false;
    }

    if (out_combat_below) {
        *out_combat_below = combat_after_left
            != sdl_left_panel_pane_placement_is_bottom(pc->where);
    }
    return true;
}

bool sdl_combat_overlay_pane_current_rect(SDL_Rect* out_rect)
{
    const struct pane_config* pc = sdl_combat_overlay_pane_config();
    const SDL_Rect* pane;
    SDL_Rect rect;
    SDL_Rect screen;
    int cell_h;
    int cell_w;
    int content_w;
    int content_h;
    int panel_w;
    int panel_h;
    int top_padding_h;
    int bottom_padding_h;
    bool combat_below = false;
    bool connected;

    if (out_rect)
        *out_rect = (SDL_Rect){ 0 };
    if (!pc || !pc->enabled)
        return false;
    if (screen_saved_fullscreen_active())
        return false;
    if (!sdl_combat_overlay_pane_presentation_active())
        return false;

    pane = &g_pane_rects[PANE_COMBAT];
    if (pane->w <= 0 || pane->h <= 0)
        return false;

    cell_h = sdl_effective_pane_cell_height_for_type(PANE_COMBAT);
    if (cell_h < 1)
        cell_h = 1;
    cell_w = cell_h / 2;
    if (cell_w < 1)
        cell_w = 1;

    content_w = PANE_COMBAT_OVERLAY_COLS * cell_w;
    content_h = sdl_combat_overlay_source_row_count() * cell_h;
    if (content_w < 1 || content_h < 1)
        return false;
    panel_w = content_w + cell_w * 2;
    connected = sdl_combat_overlay_connected_to_left_panel(&combat_below);
    top_padding_h = (connected && combat_below) ? 0 : cell_w;
    bottom_padding_h = (connected && !combat_below) ? 0 : cell_w;
    panel_h = content_h + top_padding_h + bottom_padding_h;

    rect = *pane;
    if (rect.w > panel_w) {
        if (pc->where == PLACE_TOP_RIGHT || pc->where == PLACE_RIGHT_CENTER
            || pc->where == PLACE_BOTTOM_RIGHT)
        {
            rect.x += rect.w - panel_w;
        }
        else if (pc->where == PLACE_TOP_CENTER
            || pc->where == PLACE_BOTTOM_CENTER)
        {
            rect.x += (rect.w - panel_w) / 2;
        }
        rect.w = panel_w;
    }
    else if (rect.w < panel_w) {
        int delta = panel_w - rect.w;

        if (pc->where == PLACE_TOP_RIGHT || pc->where == PLACE_RIGHT_CENTER
            || pc->where == PLACE_BOTTOM_RIGHT)
        {
            rect.x -= delta;
        }
        else if (pc->where == PLACE_TOP_CENTER
            || pc->where == PLACE_BOTTOM_CENTER)
        {
            rect.x -= delta / 2;
        }
        rect.w = panel_w;
    }
    if (rect.h > panel_h) {
        if (pc->where == PLACE_BOTTOM_LEFT || pc->where == PLACE_BOTTOM_CENTER
            || pc->where == PLACE_BOTTOM_RIGHT)
        {
            rect.y += rect.h - panel_h;
        }
        else if (pc->where == PLACE_LEFT_CENTER
            || pc->where == PLACE_RIGHT_CENTER)
        {
            rect.y += (rect.h - panel_h) / 2;
        }
        rect.h = panel_h;
    }
    else if (rect.h < panel_h) {
        int delta = panel_h - rect.h;

        if (pc->where == PLACE_BOTTOM_LEFT || pc->where == PLACE_BOTTOM_CENTER
            || pc->where == PLACE_BOTTOM_RIGHT)
        {
            rect.y -= delta;
        }
        else if (pc->where == PLACE_LEFT_CENTER
            || pc->where == PLACE_RIGHT_CENTER)
        {
            rect.y -= delta / 2;
        }
        rect.h = panel_h;
    }

    screen = sdl_get_layout_screen_rect();
    if (sdl_rect_has_area(&screen)) {
        int screen_right = screen.x + screen.w;
        int screen_bottom = screen.y + screen.h;
        const SDL_Rect* left = &g_pane_rects[PANE_LEFT_PANEL];
        SDL_Rect overlap;
        bool combat_after_left = false;
        bool shares_left_slot = sdl_left_panel_pane_runtime_active()
            && pc->where == sdl_left_panel_pane_placement()
            && sdl_rect_has_area(left);
        bool adjacent_to_left = shares_left_slot
            && sdl_combat_overlay_adjacent_to_left_panel(pc->where,
                &combat_after_left);

        /* The measured left panel replaces its nominal overlay slot after
         * generic layout.  Anchor Combat to that measured rectangle when the
         * two panes share a placement so independently-sized panes do not
         * appear horizontally shifted within the same stack. */
        if (shares_left_slot) {
            if (sdl_left_panel_pane_placement_is_right(pc->where)) {
                rect.x = left->x + left->w - rect.w;
            } else if (sdl_left_panel_pane_placement_is_horizontal_center(
                    pc->where))
            {
                rect.x = left->x + (left->w - rect.w) / 2;
            } else {
                rect.x = left->x;
            }
        }

        /* LEFT_PANEL is measured and rendered independently of the nominal
         * overlay slot geometry.  If Combat shares that slot, keep its real
         * panel below (or above for a bottom anchor) the measured left panel
         * instead of drawing over it. */
        if (shares_left_slot) {
            bool overlaps = SDL_GetRectIntersection(&rect, left, &overlap);
            if (adjacent_to_left || overlaps) {
                int below = left->y + left->h;
                int above = left->y - rect.h;
                bool place_below = !sdl_left_panel_pane_placement_is_bottom(
                    pc->where);

                if (adjacent_to_left) {
                    place_below = combat_after_left
                        != sdl_left_panel_pane_placement_is_bottom(pc->where);
                }

                if (!place_below) {
                    rect.y = above;
                    if (rect.y < screen.y
                        && below + rect.h <= screen_bottom)
                    {
                        rect.y = below;
                    }
                } else {
                    rect.y = below;
                    if (rect.y + rect.h > screen_bottom && above >= screen.y)
                        rect.y = above;
                }
            }
        }

        if (rect.x < screen.x) {
            rect.w -= screen.x - rect.x;
            rect.x = screen.x;
        }
        if (rect.y < screen.y) {
            rect.h -= screen.y - rect.y;
            rect.y = screen.y;
        }
        if (rect.x + rect.w > screen_right)
            rect.w = screen_right - rect.x;
        if (rect.y + rect.h > screen_bottom)
            rect.h = screen_bottom - rect.y;
    }
    if (rect.w <= 0 || rect.h <= 0)
        return false;

    if (out_rect)
        *out_rect = rect;
    return true;
}

bool sdl_combat_overlay_pane_content_rect(SDL_Rect* out_rect)
{
    const struct pane_config* pc = sdl_combat_overlay_pane_config();
    const SDL_Rect* left = &g_pane_rects[PANE_LEFT_PANEL];
    SDL_Rect panel;
    SDL_Rect rect;
    int cell_h;
    int cell_w;
    int content_w;
    int content_h;
    int margin_x;
    int margin_y;
    bool combat_below = false;
    bool connected;
    bool align_with_compact_left_row;

    if (out_rect)
        *out_rect = (SDL_Rect){ 0 };
    if (!sdl_combat_overlay_pane_current_rect(&panel))
        return false;

    cell_h = sdl_effective_pane_cell_height_for_type(PANE_COMBAT);
    if (cell_h < 1)
        cell_h = 1;
    cell_w = cell_h / 2;
    if (cell_w < 1)
        cell_w = 1;

    content_w = PANE_COMBAT_OVERLAY_COLS * cell_w;
    content_h = sdl_combat_overlay_source_row_count() * cell_h;
    if (content_w < 1 || content_h < 1)
        return false;
    connected = sdl_combat_overlay_connected_to_left_panel(&combat_below);
    margin_y = (connected && combat_below) ? 0 : cell_w;

    align_with_compact_left_row = pc
        && sdl_left_panel_pane_runtime_active()
        && sdl_left_panel_pane_collapsed()
        && sdl_left_panel_compact_row_mode()
        && pc->where == sdl_left_panel_pane_placement()
        && sdl_rect_has_area(left);
    if (align_with_compact_left_row) {
        int side_padding = cell_w;

        rect = panel;
        rect.y += margin_y;
        rect.h -= margin_y;
        rect.x += side_padding;
        rect.w -= side_padding * 2;
        if (rect.w > content_w)
            rect.w = content_w;
        if (sdl_left_panel_pane_placement_is_right(pc->where)) {
            rect.x = panel.x + panel.w - side_padding - rect.w;
        } else if (sdl_left_panel_pane_placement_is_horizontal_center(
                pc->where))
        {
            rect.x = panel.x + (panel.w - rect.w) / 2;
        }
    } else {
        margin_x = (panel.w > content_w) ? (panel.w - content_w) / 2 : 0;
        if (margin_x > cell_w)
            margin_x = cell_w;

        rect = (SDL_Rect){
            .x = panel.x + margin_x,
            .y = panel.y + margin_y,
            .w = panel.w - margin_x * 2,
            .h = panel.h - margin_y,
        };
        if (rect.w > content_w)
            rect.w = content_w;
    }
    if (rect.h > content_h)
        rect.h = content_h;
    if (rect.w <= 0 || rect.h <= 0)
        return false;

    if (out_rect)
        *out_rect = rect;
    return true;
}

float sdl_overlay_panel_x(const SDL_Rect* anchor,
    enum pane_placement where, int panel_w)
{
    switch (where) {
    case PLACE_TOP_RIGHT:
    case PLACE_RIGHT_CENTER:
    case PLACE_BOTTOM_RIGHT:
        return (float)(anchor->x + anchor->w - panel_w);
    case PLACE_TOP_CENTER:
    case PLACE_BOTTOM_CENTER:
        return (float)anchor->x + ((float)anchor->w - (float)panel_w) * 0.5f;
    default:
        return (float)anchor->x;
    }
}

float sdl_overlay_panel_y(const SDL_Rect* anchor,
    enum pane_placement where, int panel_h)
{
    switch (where) {
    case PLACE_BOTTOM_LEFT:
    case PLACE_BOTTOM_CENTER:
    case PLACE_BOTTOM_RIGHT:
        return (float)(anchor->y + anchor->h - panel_h);
    case PLACE_LEFT_CENTER:
    case PLACE_RIGHT_CENTER:
        return (float)anchor->y + ((float)anchor->h - (float)panel_h) * 0.5f;
    default:
        return (float)anchor->y;
    }
}

SDL_FRect sdl_overlay_panel_rect(const SDL_Rect* anchor,
    enum pane_placement where, int panel_w, int panel_h,
    const SDL_Rect* screen)
{
    float x = sdl_overlay_panel_x(anchor, where, panel_w);
    float y = sdl_overlay_panel_y(anchor, where, panel_h);

    if (screen) {
        float margin = (float)sdl_overlay_margin_px();
        float min_x = (float)screen->x + margin;
        float min_y = (float)screen->y + margin;
        float max_x = (float)(screen->x + screen->w - panel_w) - margin;
        float max_y = (float)(screen->y + screen->h - panel_h) - margin;

        if (max_x < min_x) {
            min_x = (float)screen->x;
            max_x = (float)(screen->x + screen->w - panel_w);
            if (max_x < min_x)
                max_x = min_x;
        }
        if (max_y < min_y) {
            min_y = (float)screen->y;
            max_y = (float)(screen->y + screen->h - panel_h);
            if (max_y < min_y)
                max_y = min_y;
        }

        if (x < min_x)
            x = min_x;
        if (y < min_y)
            y = min_y;
        if (x > max_x)
            x = max_x;
        if (y > max_y)
            y = max_y;
    }

    return (SDL_FRect){
        .x = x,
        .y = y,
        .w = (float)panel_w,
        .h = (float)panel_h,
    };
}

bool sdl_status_pane_current_rect(SDL_Rect* out_rect,
    enum pane_placement* out_where)
{
    const struct pane_config* pc = sdl_status_pane_config();
    const SDL_Rect* pane;
    SDL_Rect fallback;

    if (!pc || !pc->enabled)
        return false;
    if (screen_saved_fullscreen_active())
        return false;
    if (!sdl_layout_matches_supporting_pane_visibility())
        return false;

    pane = &g_pane_rects[PANE_STATUS];
    if (pane->w <= 0 || pane->h <= 0) {
        fallback = g_pane_rects[PANE_MAIN];
        if (fallback.w <= 0 || fallback.h <= 0)
            fallback = sdl_get_layout_screen_rect();
        pane = &fallback;
    }
    if (pane->w <= 0 || pane->h <= 0)
        return false;

    if (out_rect)
        *out_rect = *pane;
    if (out_where)
        *out_where = pc->where;
    return true;
}

float sdl_touch_pane_clampf(float value, float min_value, float max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

int sdl_touch_pane_story_text_width(TTF_Font* font, cptr text)
{
    int width = 0;

    if (!font || !text || !text[0])
        return 0;

    TTF_MeasureString(font, text, strlen(text), 0, &width, NULL);
    return width;
}

SDL_Color sdl_status_pane_color(byte attr)
{
    SDL_Color color = g_state.palette[attr];

    color.a = 255;
    return color;
}

void sdl_status_pane_format_turns(char* buf, size_t buflen, int turns)
{
    if (!buf || buflen == 0)
        return;

    if (turns <= 0) {
        buf[0] = '\0';
        return;
    }

    strnfmt(buf, buflen, "%d", turns);
}

void sdl_status_pane_add(status_pane_entry* entries, int max_entries,
    int* count, cptr label, cptr detail, byte attr)
{
    status_pane_entry* entry;

    if (!entries || !count || *count < 0 || *count >= max_entries)
        return;
    if (!label || !label[0])
        return;

    entry = &entries[*count];
    SDL_strlcpy(entry->label, label, sizeof(entry->label));
    SDL_strlcpy(entry->detail, detail ? detail : "", sizeof(entry->detail));
    entry->attr = attr;
    (*count)++;
}

void sdl_status_pane_add_timed(status_pane_entry* entries,
    int max_entries, int* count, cptr label, int turns, byte attr)
{
    char detail[32];

    sdl_status_pane_format_turns(detail, sizeof(detail), turns);
    sdl_status_pane_add(entries, max_entries, count, label, detail, attr);
}

bool sdl_status_pane_song_name(byte song, char* out, size_t out_sz)
{
    int ability;
    cptr name;
    cptr label;

    if (!out || out_sz == 0)
        return false;
    out[0] = '\0';

    if (song == SNG_NOTHING || !b_info || !b_name)
        return false;

    ability = ability_index(S_SNG, song);
    if (ability < 0)
        return false;

    name = b_name + b_info[ability].name;
    if (!name || !name[0])
        return false;

    label = (strncmp(name, "Song of ", 8) == 0) ? name + 8 : name;
    SDL_strlcpy(out, label, out_sz);
    return out[0] != '\0';
}

bool sdl_status_pane_current_song_detail(char* out, size_t out_sz)
{
    char song1[32] = "";
    char song2[32] = "";

    if (!out || out_sz == 0)
        return false;
    out[0] = '\0';

    if (!p_ptr)
        return false;
    if (p_ptr->song1 == SNG_NOTHING && p_ptr->song2 == SNG_NOTHING)
        return false;

    (void)sdl_status_pane_song_name(p_ptr->song1, song1, sizeof(song1));
    (void)sdl_status_pane_song_name(p_ptr->song2, song2, sizeof(song2));

    if (song1[0] && song2[0])
        strnfmt(out, out_sz, "%s+%s", song1, song2);
    else if (song1[0])
        SDL_strlcpy(out, song1, out_sz);
    else if (song2[0])
        SDL_strlcpy(out, song2, out_sz);
    else
        SDL_strlcpy(out, "active", out_sz);

    return out[0] != '\0';
}

int sdl_status_pane_collect(status_pane_entry* entries, int max_entries)
{
    int count = 0;
    char detail[64];

    if (!entries || max_entries <= 0 || !p_ptr || !character_generated
        || character_icky)
    {
        return 0;
    }

    if (p_ptr->running > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Running",
            p_ptr->running, TERM_L_BLUE);
    if (p_ptr->stealth_mode)
        sdl_status_pane_add(entries, max_entries, &count, "Stealth",
            "", TERM_L_BLUE);
    /* Waiting marks every character as focused, but the state only has a
     * gameplay effect for Focused Attack or Polearm Mastery. */
    if (p_ptr->focused
        && (p_ptr->active_ability[S_PER][PER_FOCUSED_ATTACK]
            || p_ptr->active_ability[S_MEL][MEL_POLEARMS]))
        sdl_status_pane_add(entries, max_entries, &count, "Focused",
            "", TERM_L_BLUE);
    if (p_ptr->active_ability[S_PER][PER_CONCENTRATION]
        && p_ptr->consecutive_attacks > 0
        && p_ptr->last_attack_m_idx > 0
        && p_ptr->last_attack_m_idx < mon_max
        && mon_list[p_ptr->last_attack_m_idx].r_idx)
    {
        int bonus = MIN(p_ptr->consecutive_attacks,
            p_ptr->skill_use[S_PER] / 2);

        if (bonus > 0)
        {
            strnfmt(detail, sizeof(detail), "+%d", bonus);
            sdl_status_pane_add(entries, max_entries, &count,
                "Concentration", detail, TERM_L_BLUE);
        }
    }
    {
        int power_throw_slot = player_power_throw_quiver_slot();

        if (power_throw_slot)
        {
            cptr quiver = (power_throw_slot == INVEN_QUIVER2)
                ? "2nd quiver" : "1st quiver";
            sdl_status_pane_add(entries, max_entries, &count, "Power Throw",
                quiver, TERM_YELLOW);
        }
    }
    if (sdl_status_pane_current_song_detail(detail, sizeof(detail)))
        sdl_status_pane_add(entries, max_entries, &count, "Singing",
            detail, TERM_L_BLUE);
    if (p_ptr->song_contest_player_stacks > 0) {
        strnfmt(detail, sizeof(detail), "%d",
            p_ptr->song_contest_player_stacks);
        sdl_status_pane_add(entries, max_entries, &count, "Contest",
            detail, TERM_L_BLUE);
    }
    if (p_ptr->smithing > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Smithing",
            p_ptr->smithing, TERM_WHITE);
    if (p_ptr->fletching > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Fletching",
            p_ptr->fletching, TERM_WHITE);
    if (p_ptr->resting) {
        if (p_ptr->resting == -1)
            SDL_strlcpy(detail, "until healed", sizeof(detail));
        else if (p_ptr->resting == -2)
            SDL_strlcpy(detail, "until done", sizeof(detail));
        else
            sdl_status_pane_format_turns(detail, sizeof(detail),
                p_ptr->resting);
        sdl_status_pane_add(entries, max_entries, &count, "Resting",
            detail, TERM_WHITE);
    }
    if (p_ptr->command_rep > 0) {
        strnfmt(detail, sizeof(detail), "%d left", p_ptr->command_rep);
        sdl_status_pane_add(entries, max_entries, &count, "Repeat",
            detail, TERM_WHITE);
    }

    if (p_ptr->entranced > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Entranced",
            p_ptr->entranced, TERM_RED);
    if (p_ptr->afraid > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Afraid",
            p_ptr->afraid, TERM_ORANGE);
    if (p_ptr->confused > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Confused",
            p_ptr->confused, TERM_ORANGE);
    if (p_ptr->stun > 100)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Knocked out",
            p_ptr->stun, TERM_RED);
    else if (p_ptr->stun > 50)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Heavy stun",
            p_ptr->stun, TERM_ORANGE);
    else if (p_ptr->stun > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Stunned",
            p_ptr->stun, TERM_ORANGE);
    if (p_ptr->blind > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Blind",
            p_ptr->blind, TERM_ORANGE);
    if (p_ptr->image > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count,
            "Hallucinating", p_ptr->image, TERM_VIOLET);
    if (p_ptr->poisoned > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Poisoned",
            p_ptr->poisoned, TERM_L_GREEN);
    if (p_ptr->cut > 100)
        sdl_status_pane_add_timed(entries, max_entries, &count,
            "Mortal wound", p_ptr->cut, TERM_RED);
    else if (p_ptr->cut > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Bleeding",
            p_ptr->cut, (p_ptr->cut > 20) ? TERM_RED : TERM_L_RED);
    if (p_ptr->darkened > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Darkened",
            p_ptr->darkened, TERM_SLATE);
    if (p_ptr->rage > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Rage",
            p_ptr->rage, TERM_RED);

    if (p_ptr->food < PY_FOOD_STARVE)
        sdl_status_pane_add(entries, max_entries, &count, "Starving",
            "", TERM_RED);
    else if (p_ptr->food < PY_FOOD_WEAK)
        sdl_status_pane_add(entries, max_entries, &count, "Weak",
            "", TERM_ORANGE);
    else if (p_ptr->food < PY_FOOD_ALERT)
        sdl_status_pane_add(entries, max_entries, &count, "Hungry",
            "", TERM_YELLOW);
    else if (p_ptr->food >= PY_FOOD_FULL)
        sdl_status_pane_add(entries, max_entries, &count, "Full",
            "", TERM_L_GREEN);

    if (p_ptr->fast > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Fast",
            p_ptr->fast, TERM_L_GREEN);
    if (p_ptr->slow > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Slow",
            p_ptr->slow, TERM_ORANGE);
    if (!p_ptr->fast && !p_ptr->slow && p_ptr->pspeed > 2)
        sdl_status_pane_add(entries, max_entries, &count, "Fast",
            "", TERM_L_GREEN);
    else if (!p_ptr->fast && !p_ptr->slow && p_ptr->pspeed < 2)
        sdl_status_pane_add(entries, max_entries, &count, "Slow",
            "", TERM_ORANGE);
    if (p_ptr->tmp_str > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Strong",
            p_ptr->tmp_str, TERM_L_GREEN);
    if (p_ptr->tmp_dex > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Agile",
            p_ptr->tmp_dex, TERM_L_GREEN);
    if (p_ptr->tmp_con > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Resilient",
            p_ptr->tmp_con, TERM_L_GREEN);
    if (p_ptr->tmp_gra > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Grace",
            p_ptr->tmp_gra, TERM_L_GREEN);
    if (p_ptr->tmp_per > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count, "Perceptive",
            p_ptr->tmp_per, TERM_L_GREEN);
    if (p_ptr->tim_invis > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count,
            "See invisible", p_ptr->tim_invis, TERM_L_BLUE);
    if (p_ptr->oppose_fire > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count,
            "Resist fire", p_ptr->oppose_fire, TERM_L_RED);
    if (p_ptr->oppose_cold > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count,
            "Resist cold", p_ptr->oppose_cold, TERM_L_BLUE);
    if (p_ptr->oppose_pois > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count,
            "Resist poison", p_ptr->oppose_pois, TERM_L_GREEN);
    if (p_ptr->song_challenge_effect > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count,
            "Challenge echo", p_ptr->song_challenge_effect, TERM_ORANGE);
    if (p_ptr->song_elbereth_effect > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count,
            "Elbereth echo", p_ptr->song_elbereth_effect, TERM_ORANGE);
    if (p_ptr->song_lockout_timer > 0)
        sdl_status_pane_add_timed(entries, max_entries, &count,
            "Song lockout", p_ptr->song_lockout_timer, TERM_SLATE);

    if (p_ptr->climbing)
        sdl_status_pane_add(entries, max_entries, &count, "Climbing",
            "", TERM_WHITE);
    if (p_ptr->leaping)
        sdl_status_pane_add(entries, max_entries, &count, "Leaping",
            "", TERM_WHITE);
    if (p_ptr->knocked_back)
        sdl_status_pane_add(entries, max_entries, &count, "Knocked back",
            "", TERM_ORANGE);
    if (p_ptr->skip_next_turn)
        sdl_status_pane_add(entries, max_entries, &count, "Skip turn",
            "", TERM_ORANGE);
    if (p_ptr->was_entranced)
        sdl_status_pane_add(entries, max_entries, &count, "Waking",
            "", TERM_ORANGE);
    if (p_ptr->vengeance > 0
        && p_ptr->active_ability[S_WIL][WIL_VENGEANCE])
    {
        strnfmt(detail, sizeof(detail), "%d", p_ptr->vengeance);
        sdl_status_pane_add(entries, max_entries, &count, "Vengeance",
            detail, TERM_L_RED);
    }
    if (p_ptr->truce)
        sdl_status_pane_add(entries, max_entries, &count, "Truce",
            "", TERM_L_BLUE);
    if (p_ptr->on_the_run)
        sdl_status_pane_add(entries, max_entries, &count, "On the run",
            "", TERM_L_RED);
    if (p_ptr->cursed)
        sdl_status_pane_add(entries, max_entries, &count, "Cursed",
            "", TERM_ORANGE);

    if (in_bounds(p_ptr->py, p_ptr->px)) {
        if (cave_pit_bold(p_ptr->py, p_ptr->px) && !p_ptr->leaping)
            sdl_status_pane_add(entries, max_entries, &count, "In pit",
                "", TERM_ORANGE);
        else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
            sdl_status_pane_add(entries, max_entries, &count, "In web",
                "", TERM_ORANGE);
        else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT)
            sdl_status_pane_add(entries, max_entries, &count, "Sunlight",
                "", TERM_YELLOW);
    }

    return count;
}

void sdl_status_pane_entry_line(const status_pane_entry* entry,
    char* out, size_t out_sz)
{
    if (!out || out_sz == 0)
        return;
    out[0] = '\0';

    if (!entry || !entry->label[0])
        return;

    if (entry->detail[0])
        strnfmt(out, out_sz, "%s %s", entry->label, entry->detail);
    else
        SDL_strlcpy(out, entry->label, out_sz);
}

int sdl_status_pane_text_width(TTF_Font* font, cptr text)
{
    int width = sdl_touch_pane_story_text_width(font, text);

    if (width < 0)
        width = 0;
    return width;
}

bool sdl_status_pane_entry_prefers_span(const status_pane_entry* entry)
{
    if (!entry || !entry->label[0])
        return false;

    if (strcmp(entry->label, "Singing") == 0)
        return true;

    return false;
}

int sdl_status_pane_layout_entries(TTF_Font* font,
    const status_pane_entry* entries, int visible_count, int more_count,
    status_pane_layout_item* items, int max_items, int max_content_w,
    int max_rows, int item_pad_x, int column_gap_x, int min_item_w,
    int* out_content_w, int* out_row_count)
{
    int item_count = 0;
    int max_column_w;
    int max_normal_w = 0;
    int max_span_w = 0;
    int normal_count = 0;
    int normal_columns;
    int column_limit = sdl_mobile_portrait_layout_active() ? 4
        : SDL_STATUS_PANE_COLUMNS;
    int content_w;
    int column_w;
    int row = 0;
    int col = 0;

    if (out_content_w)
        *out_content_w = 0;
    if (out_row_count)
        *out_row_count = 0;
    if (!font || !items || max_items <= 0 || max_content_w <= 0
        || max_rows <= 0)
    {
        return -1;
    }

    if (column_gap_x < 0)
        column_gap_x = 0;
    if (max_content_w <= column_gap_x)
        column_gap_x = 0;

    if (column_limit < 1)
        column_limit = 1;
    max_column_w = (max_content_w
        - column_gap_x * (column_limit - 1)) / column_limit;
    if (max_column_w < 1)
        max_column_w = max_content_w;

    for (int i = 0; i < visible_count; i++) {
        status_pane_layout_item item = { 0 };
        int line_w;

        item.attr = entries[i].attr;
        sdl_status_pane_entry_line(&entries[i], item.line,
            sizeof(item.line));
        line_w = sdl_status_pane_text_width(font, item.line) + item_pad_x * 2;
        if (line_w < min_item_w)
            line_w = min_item_w;
        if (line_w > max_content_w)
            line_w = max_content_w;

        item.w = line_w;
        item.span = sdl_status_pane_entry_prefers_span(&entries[i])
            || line_w > max_column_w;
        if (item.span)
            max_span_w = MAX(max_span_w, line_w);
        else {
            max_normal_w = MAX(max_normal_w, line_w);
            normal_count++;
        }
        items[item_count++] = item;
        if (item_count >= max_items)
            break;
    }

    if (more_count > 0 && item_count < max_items) {
        status_pane_layout_item item = { 0 };
        int line_w;

        item.attr = TERM_SLATE;
        strnfmt(item.line, sizeof(item.line), "More +%d", more_count);
        line_w = sdl_status_pane_text_width(font, item.line) + item_pad_x * 2;
        if (line_w < min_item_w)
            line_w = min_item_w;
        if (line_w > max_content_w)
            line_w = max_content_w;

        item.w = line_w;
        item.span = line_w > max_column_w;
        if (item.span)
            max_span_w = MAX(max_span_w, line_w);
        else {
            max_normal_w = MAX(max_normal_w, line_w);
            normal_count++;
        }
        items[item_count++] = item;
    }

    if (item_count <= 0)
        return 0;

    normal_columns = MIN(MAX(normal_count, 1), column_limit);
    if (max_normal_w > 0)
        content_w = max_normal_w * normal_columns
            + column_gap_x * (normal_columns - 1);
    else
        content_w = max_span_w;

    content_w = MAX(content_w, max_span_w);
    if (content_w < min_item_w)
        content_w = min_item_w;
    if (content_w > max_content_w)
        content_w = max_content_w;

    column_w = (normal_columns > 1)
        ? (content_w - column_gap_x * (normal_columns - 1))
            / normal_columns
        : content_w;
    if (column_w < 1)
        column_w = content_w;

    for (int i = 0; i < item_count; i++) {
        status_pane_layout_item* item = &items[i];

        if (item->span) {
            if (col != 0) {
                row++;
                col = 0;
            }
            item->row = row;
            item->x = 0;
            item->w = content_w;
            row++;
            col = 0;
        } else {
            item->row = row;
            item->x = col * (column_w + column_gap_x);
            item->w = column_w;
            col++;
            if (col >= normal_columns) {
                row++;
                col = 0;
            }
        }
    }

    if (col != 0)
        row++;

    if (row > max_rows)
        return -1;

    if (out_content_w)
        *out_content_w = content_w;
    if (out_row_count)
        *out_row_count = row;

    return item_count;
}

void sdl_status_pane_draw_text(TTF_Font* font, cptr text,
    SDL_Color color, float x, float y, float max_w, float row_h,
    bool right_align)
{
    SDL_Texture* texture;
    SDL_FRect dst;
    float scale = 1.0f;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || max_w <= 0.0f || row_h <= 0.0f)
        return;

    texture = sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture)
        return;

    if (text_w > 0 && (float)text_w > max_w)
        scale = max_w / (float)text_w;

    dst.w = (float)text_w * scale;
    dst.h = (float)text_h * scale;
    dst.x = right_align ? x + max_w - dst.w : x;
    dst.y = y + (row_h - dst.h) * 0.5f;

    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
}

static Uint64 sdl_status_pane_hash_bytes(Uint64 hash, const void* data,
    size_t len)
{
    const byte* bytes = (const byte*)data;

    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool sdl_status_pane_layout_compute(status_pane_layout* out)
{
    static bool stable_cache_valid;
    static Uint64 stable_cache_hash;
    static status_pane_layout stable_cache_layout;
    status_pane_entry entries[SDL_STATUS_PANE_MAX_ENTRIES];
    SDL_Rect anchor;
    SDL_Rect screen;
    enum pane_placement where;
    TTF_Font* font;
    int count;
    int font_px;
    int row_h;
    int pad_x;
    int pad_y;
    int item_pad_x;
    int gap_x;
    int min_item_w;
    int max_panel_w;
    int max_panel_h;
    int max_content_w;
    int max_rows;
    int visible_count;
    int more_count;
    int layout_count;
    int content_w = 0;
    int row_count = 0;
    int panel_w;
    int panel_h;
    int column_limit;
    Uint64 layout_hash = 1469598103934665603ULL;

    if (!out)
        return false;
    *out = (status_pane_layout){ 0 };

    if (!sdl_status_pane_current_rect(&anchor, &where))
        return false;

    count = sdl_status_pane_collect(entries, SDL_STATUS_PANE_MAX_ENTRIES);
    if (count <= 0)
        return false;

    font_px = sdl_effective_pane_cell_height_for_type(PANE_STATUS);
    if (font_px < 8)
        font_px = 8;
    screen = sdl_get_layout_screen_rect();
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &anchor,
        sizeof(anchor));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &screen,
        sizeof(screen));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &where,
        sizeof(where));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &font_px,
        sizeof(font_px));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash,
        &g_state.system_scale, sizeof(g_state.system_scale));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash,
        &g_story_font_generation, sizeof(g_story_font_generation));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &count,
        sizeof(count));
    for (int i = 0; i < count; i++) {
        layout_hash = sdl_status_pane_hash_bytes(layout_hash,
            entries[i].label, strlen(entries[i].label) + 1);
        layout_hash = sdl_status_pane_hash_bytes(layout_hash,
            entries[i].detail, strlen(entries[i].detail) + 1);
        layout_hash = sdl_status_pane_hash_bytes(layout_hash,
            &entries[i].attr, sizeof(entries[i].attr));
    }
    if (stable_cache_valid && stable_cache_hash == layout_hash) {
        *out = stable_cache_layout;
        return true;
    }

    font = sdl_story_font_for_height_slot(font_px,
        SDL_STORY_FONT_SLOT_STATUS);
    if (!font)
        return false;

    row_h = (int)((float)font_px * 1.35f + 0.5f);
    if (row_h < font_px + 2)
        row_h = font_px + 2;
    pad_x = (int)((float)font_px * 0.45f + 0.5f);
    pad_y = (int)((float)font_px * 0.32f + 0.5f);
    if (pad_x < 5)
        pad_x = 5;
    if (pad_y < 4)
        pad_y = 4;
    item_pad_x = pad_x / 2;
    if (item_pad_x < 4)
        item_pad_x = 4;
    gap_x = item_pad_x;
    min_item_w = font_px * 3;

    max_panel_w = screen.w - pad_x * 2;
    max_panel_h = screen.h - pad_y * 2;
    if (max_panel_w <= 0 || max_panel_h <= 0)
        return false;

    max_content_w = max_panel_w - pad_x * 2;
    if (max_content_w <= 0)
        return false;
    max_rows = (max_panel_h - pad_y * 2) / row_h;
    if (max_rows <= 0)
        return false;
    column_limit = sdl_mobile_portrait_layout_active() ? 4
        : SDL_STATUS_PANE_COLUMNS;
    if (sdl_mobile_portrait_layout_active() && max_rows > 2)
        max_rows = 2;

    /* Reserve one cell for the
     * "More" item when truncating, instead of starting at all 48 entries and
     * reformatting/remeasuring them once for every failed row count. */
    visible_count = count;
    if (visible_count > max_rows * column_limit) {
        visible_count = max_rows * column_limit - 1;
        if (visible_count < 0)
            visible_count = 0;
    }
    for (;;) {
        more_count = count - visible_count;
        layout_count = sdl_status_pane_layout_entries(font, entries,
            visible_count, more_count, out->items, SDL_STATUS_PANE_MAX_ENTRIES,
            max_content_w, max_rows, item_pad_x, gap_x, min_item_w,
            &content_w, &row_count);

        if (layout_count >= 0)
            break;
        if (visible_count <= 0)
            return false;
        visible_count--;
    }

    if (layout_count <= 0)
        return false;

    panel_w = pad_x * 2 + content_w;
    if (panel_w > max_panel_w)
        panel_w = max_panel_w;

    panel_h = pad_y * 2 + row_count * row_h;
    if (panel_h > max_panel_h)
        panel_h = max_panel_h;

    out->panel = sdl_overlay_panel_rect(&anchor, where, panel_w,
        panel_h, &screen);
    out->layout_count = layout_count;
    out->content_w = content_w;
    out->font_px = font_px;
    out->row_h = row_h;
    out->pad_x = pad_x;
    out->pad_y = pad_y;
    out->item_pad_x = item_pad_x;
    out->right_align = sdl_left_panel_pane_placement_is_right(where);

    stable_cache_layout = *out;
    stable_cache_hash = layout_hash;
    stable_cache_valid = true;

    return true;
}

bool sdl_status_pane_layout(status_pane_layout* out)
{
    static Uint64 cached_generation;
    static status_pane_layout cached_layout;
    static bool cached_result;
    SDL_Rect anchor;
    SDL_Rect screen;
    enum pane_placement where;

    if (!out)
        return false;
    if (cached_generation == g_sdl_present_generation) {
        *out = cached_layout;
    } else {
        cached_result = sdl_status_pane_layout_compute(&cached_layout);
        cached_generation = g_sdl_present_generation;
        *out = cached_layout;
    }

    if (!cached_result)
        return false;

    /* Stack reflow can move the status anchor after its shape was cached for
     * this frame. Keep the measured contents, but always resolve the live
     * panel position from the current anchor. */
    if (!sdl_status_pane_current_rect(&anchor, &where))
        return false;
    screen = sdl_get_layout_screen_rect();
    out->panel = sdl_overlay_panel_rect(&anchor, where,
        (int)(out->panel.w + 0.5f), (int)(out->panel.h + 0.5f), &screen);
    out->right_align = sdl_left_panel_pane_placement_is_right(where);
    return true;
}

void sdl_status_pane_render(void)
{
    status_pane_layout layout;
    TTF_Font* font;

    if (!sdl_status_pane_layout(&layout))
        return;

    font = sdl_story_font_for_height_slot(layout.font_px,
        SDL_STORY_FONT_SLOT_STATUS);
    if (!font)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 5, 7, 9, 222);
    SDL_RenderFillRect(g_state.renderer, &layout.panel);

    {
        float text_x = layout.panel.x + (float)layout.pad_x;

        if (layout.right_align) {
            text_x = layout.panel.x + layout.panel.w
                - (float)layout.pad_x - (float)layout.content_w;
        }

        for (int i = 0; i < layout.layout_count; i++) {
            status_pane_layout_item* item = &layout.items[i];
            SDL_Color color;
            float item_x = text_x + (float)item->x;
            float item_text_x = item_x + (float)layout.item_pad_x;
            float item_text_w = (float)item->w
                - (float)(layout.item_pad_x * 2);
            float row_y = layout.panel.y + (float)layout.pad_y
                + (float)(item->row * layout.row_h);

            if (item_text_w <= 0.0f)
                continue;

            color = sdl_status_pane_color(item->attr);
            sdl_status_pane_draw_text(font, item->line, color,
                item_text_x, row_y, item_text_w, (float)layout.row_h,
                layout.right_align);
        }
    }
}

static int sdl_status_depth_pane_pack(TTF_Font* font,
    const status_pane_entry* entries, int visible_count, int more_count,
    cptr depth_label, status_depth_pane_layout_item* items, int max_items,
    int max_content_w, int max_rows, int gap_x, int* out_content_w,
    int* out_row_count)
{
    int item_count = 0;
    int row = 0;
    int used_w = 0;
    int row_used_w = 0;
    int token_count = 1 + visible_count + (more_count > 0 ? 1 : 0);

    if (out_content_w)
        *out_content_w = 0;
    if (out_row_count)
        *out_row_count = 0;
    if (!font || !depth_label || !depth_label[0] || !items
        || max_items <= 0 || max_content_w <= 0 || max_rows <= 0)
    {
        return -1;
    }

    for (int token = 0; token < token_count; token++) {
        status_depth_pane_layout_item item = { 0 };
        int right;

        if (token == 0) {
            SDL_strlcpy(item.line, depth_label, sizeof(item.line));
            item.attr = TERM_L_WHITE;
        } else if (token <= visible_count) {
            sdl_status_pane_entry_line(&entries[token - 1], item.line,
                sizeof(item.line));
            item.attr = entries[token - 1].attr;
        } else {
            strnfmt(item.line, sizeof(item.line), "More +%d", more_count);
            item.attr = TERM_SLATE;
        }

        item.w = sdl_status_pane_text_width(font, item.line);
        if (item.w < 1)
            continue;
        if (item.w > max_content_w)
            item.w = max_content_w;

        right = row_used_w > 0 ? row_used_w + gap_x : 0;
        if (right + item.w > max_content_w) {
            row++;
            row_used_w = 0;
            right = 0;
        }
        if (row >= max_rows || item_count >= max_items)
            return -1;

        item.row_from_bottom = row;
        item.right = right;
        items[item_count++] = item;
        row_used_w = right + item.w;
        if (row_used_w > used_w)
            used_w = row_used_w;
    }

    if (out_content_w)
        *out_content_w = used_w;
    if (out_row_count)
        *out_row_count = row + 1;
    return item_count;
}

static bool g_status_depth_pane_layout_computing;

static bool sdl_status_depth_pane_top_edge(enum pane_placement where)
{
    return where == PLACE_TOP_LEFT || where == PLACE_TOP_CENTER
        || where == PLACE_TOP_RIGHT;
}

static bool sdl_status_depth_pane_same_horizontal_edge(
    enum pane_placement first, enum pane_placement second)
{
    return (sdl_left_panel_pane_placement_is_bottom(first)
            && sdl_left_panel_pane_placement_is_bottom(second))
        || (sdl_status_depth_pane_top_edge(first)
            && sdl_status_depth_pane_top_edge(second));
}

/* Keep the Status & Depth anchor fixed while finding how far its line may
 * extend before reaching Quick Access.  Returning less than min_panel_w tells
 * the caller that side-by-side layout is impossible; in that case both panes
 * retain their widths and the vertical collision resolver stacks them. */
static int sdl_status_depth_pane_width_before_quick(
    const SDL_Rect* anchor, const SDL_Rect* screen,
    enum pane_placement where, int min_panel_w, int min_panel_h,
    int max_panel_w, const SDL_FRect* quick)
{
    SDL_FRect probe;
    float allowed = (float)max_panel_w;
    float quick_left;
    float quick_right;
    float gap;

    if (!anchor || !screen || !quick || min_panel_w <= 0
        || max_panel_w <= 0 || quick->w <= 0.0f || quick->h <= 0.0f)
    {
        return max_panel_w;
    }

    probe = sdl_overlay_panel_rect(anchor, where, min_panel_w, min_panel_h,
        screen);
    if (probe.y >= quick->y + quick->h
        || probe.y + probe.h <= quick->y)
    {
        return max_panel_w;
    }
    quick_left = quick->x;
    quick_right = quick->x + quick->w;
    gap = (float)sdl_overlay_inner_gap_px();

    if (sdl_left_panel_pane_placement_is_right(where)) {
        float fixed_right = probe.x + probe.w;

        if (quick_left >= fixed_right)
            return max_panel_w;
        allowed = quick_right <= fixed_right
            ? fixed_right - quick_right - gap : 0.0f;
    } else if (sdl_left_panel_pane_placement_is_horizontal_center(where)) {
        float fixed_center = probe.x + probe.w * 0.5f;

        if (quick_right <= fixed_center) {
            allowed = (fixed_center - quick_right - gap) * 2.0f;
        } else if (quick_left >= fixed_center) {
            allowed = (quick_left - gap - fixed_center) * 2.0f;
        } else {
            allowed = 0.0f;
        }
    } else {
        float fixed_left = probe.x;

        if (quick_right <= fixed_left)
            return max_panel_w;
        allowed = quick_left >= fixed_left
            ? quick_left - gap - fixed_left : 0.0f;
    }

    if (allowed < 0.0f)
        allowed = 0.0f;
    if (allowed > (float)max_panel_w)
        allowed = (float)max_panel_w;
    return (int)allowed;
}

static bool sdl_status_depth_pane_layout_compute(
    status_depth_pane_layout* out)
{
    static bool stable_cache_valid;
    static Uint64 stable_cache_hash;
    static status_depth_pane_layout stable_cache_layout;
    const struct pane_config* pc = sdl_status_depth_pane_config();
    status_pane_entry entries[SDL_STATUS_PANE_MAX_ENTRIES];
    SDL_Rect anchor;
    SDL_Rect screen;
    SDL_Rect quick_anchor;
    SDL_Rect quick_screen;
    SDL_FRect quick_panel = { 0 };
    TTF_Font* font;
    char depth_label[64];
    int count;
    int font_px;
    int max_panel_w;
    int max_panel_h;
    int max_content_w;
    int max_rows;
    int visible_count;
    int more_count;
    int layout_count;
    int content_w = 0;
    int row_count = 0;
    int panel_w;
    int panel_h;
    int min_panel_w;
    enum pane_placement quick_where = PLACE_BOTTOM_CENTER;
    bool have_quick_panel = false;
    Uint64 layout_hash = 1469598103934665603ULL;

    if (!out)
        return false;
    *out = (status_depth_pane_layout){ 0 };

    if (!pc || !pc->enabled || !character_generated || !character_dungeon
        || character_icky || !p_ptr || screen_saved_fullscreen_active())
    {
        return false;
    }
    if (!sdl_layout_matches_supporting_pane_visibility())
        return false;
    if (!sdl_overlay_pane_anchor_rect(PANE_STATUS_DEPTH, &anchor))
        return false;

    sdl_depth_menu_pane_label(depth_label, sizeof(depth_label));
    if (!depth_label[0])
        return false;
    count = sdl_status_pane_collect(entries, SDL_STATUS_PANE_MAX_ENTRIES);

    font_px = sdl_effective_pane_cell_height_for_type(PANE_STATUS_DEPTH);
    if (font_px < 8)
        font_px = 8;
    screen = sdl_get_layout_screen_rect();

    /* Quick Access keeps its configured geometry.  Status & Depth consumes
     * only the side span before it and wraps into additional rows.  When even
     * the depth token cannot fit beside Quick Access, the later vertical
     * collision pass stacks the two full-width panes instead. */
    if (sdl_touch_top_panel_current_anchor(&quick_screen, &quick_anchor,
            &quick_where)
        && sdl_status_depth_pane_same_horizontal_edge(pc->where, quick_where)
        && sdl_touch_top_panel_compute_layout(NULL, &quick_panel)
        && quick_panel.w > 0.0f && quick_panel.h > 0.0f)
    {
        have_quick_panel = true;
    }

    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &anchor,
        sizeof(anchor));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &screen,
        sizeof(screen));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &pc->where,
        sizeof(pc->where));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &font_px,
        sizeof(font_px));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &have_quick_panel,
        sizeof(have_quick_panel));
    if (have_quick_panel) {
        layout_hash = sdl_status_pane_hash_bytes(layout_hash, &quick_where,
            sizeof(quick_where));
        layout_hash = sdl_status_pane_hash_bytes(layout_hash, &quick_panel,
            sizeof(quick_panel));
    }
    layout_hash = sdl_status_pane_hash_bytes(layout_hash,
        &g_state.system_scale, sizeof(g_state.system_scale));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash,
        &g_story_font_generation, sizeof(g_story_font_generation));
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, depth_label,
        strlen(depth_label) + 1);
    layout_hash = sdl_status_pane_hash_bytes(layout_hash, &count,
        sizeof(count));
    for (int i = 0; i < count; i++) {
        layout_hash = sdl_status_pane_hash_bytes(layout_hash,
            entries[i].label, strlen(entries[i].label) + 1);
        layout_hash = sdl_status_pane_hash_bytes(layout_hash,
            entries[i].detail, strlen(entries[i].detail) + 1);
        layout_hash = sdl_status_pane_hash_bytes(layout_hash,
            &entries[i].attr, sizeof(entries[i].attr));
    }
    if (stable_cache_valid && stable_cache_hash == layout_hash) {
        *out = stable_cache_layout;
        return true;
    }

    font = sdl_story_font_for_height_slot(font_px,
        SDL_STORY_FONT_SLOT_STATUS);
    if (!font)
        return false;

    out->font_px = font_px;
    out->row_h = (int)((float)font_px * 1.35f + 0.5f);
    if (out->row_h < font_px + 2)
        out->row_h = font_px + 2;
    out->pad_x = (int)((float)font_px * 0.55f + 0.5f);
    out->pad_y = (int)((float)font_px * 0.30f + 0.5f);
    if (out->pad_x < 6)
        out->pad_x = 6;
    if (out->pad_y < 3)
        out->pad_y = 3;
    out->gap_x = out->pad_x;

    max_panel_w = screen.w - out->pad_x * 2;
    max_panel_h = screen.h - out->pad_y * 2;
    if (max_panel_w <= 0 || max_panel_h <= 0)
        return false;

    min_panel_w = out->pad_x * 2
        + sdl_status_pane_text_width(font, depth_label);
    if (min_panel_w > max_panel_w)
        min_panel_w = max_panel_w;
    if (have_quick_panel) {
        int side_panel_w = sdl_status_depth_pane_width_before_quick(
            &anchor, &screen, pc->where, min_panel_w,
            out->pad_y * 2 + out->row_h, max_panel_w, &quick_panel);

        if (side_panel_w >= min_panel_w && side_panel_w < max_panel_w)
            max_panel_w = side_panel_w;
    }
    max_content_w = max_panel_w - out->pad_x * 2;
    max_rows = (max_panel_h - out->pad_y * 2) / out->row_h;
    if (max_content_w <= 0 || max_rows <= 0)
        return false;

    visible_count = count;
    for (;;) {
        more_count = count - visible_count;
        layout_count = sdl_status_depth_pane_pack(font, entries,
            visible_count, more_count, depth_label, out->items,
            (int)N_ELEMENTS(out->items), max_content_w, max_rows,
            out->gap_x, &content_w, &row_count);
        if (layout_count >= 0)
            break;
        if (visible_count <= 0)
            return false;
        visible_count--;
    }

    if (layout_count <= 0)
        return false;
    panel_w = out->pad_x * 2 + content_w;
    panel_h = out->pad_y * 2 + row_count * out->row_h;
    if (panel_w > max_panel_w)
        panel_w = max_panel_w;
    if (panel_h > max_panel_h)
        panel_h = max_panel_h;

    out->panel = sdl_overlay_panel_rect(&anchor, pc->where, panel_w,
        panel_h, &screen);
    out->layout_count = layout_count;
    out->row_count = row_count;
    out->content_w = content_w;

    stable_cache_layout = *out;
    stable_cache_hash = layout_hash;
    stable_cache_valid = true;
    return true;
}

bool sdl_status_depth_pane_layout(status_depth_pane_layout* out)
{
    static Uint64 cached_generation;
    static status_depth_pane_layout cached_layout;
    static bool cached_result;
    const struct pane_config* pc = sdl_status_depth_pane_config();
    SDL_Rect anchor;
    SDL_Rect screen;

    if (!out)
        return false;
    if (g_status_depth_pane_layout_computing)
        return false;
    if (cached_generation != g_sdl_present_generation) {
        g_status_depth_pane_layout_computing = true;
        cached_result = sdl_status_depth_pane_layout_compute(&cached_layout);
        g_status_depth_pane_layout_computing = false;
        cached_generation = g_sdl_present_generation;
    }
    *out = cached_layout;
    if (!cached_result || !pc || !pc->enabled)
        return false;

    if (!sdl_overlay_pane_anchor_rect(PANE_STATUS_DEPTH, &anchor))
        return false;
    screen = sdl_get_layout_screen_rect();
    out->panel = sdl_overlay_panel_rect(&anchor, pc->where,
        (int)(out->panel.w + 0.5f), (int)(out->panel.h + 0.5f), &screen);
    return true;
}

bool sdl_status_depth_pane_current_rect(SDL_FRect* out)
{
    status_depth_pane_layout layout;

    if (out)
        *out = (SDL_FRect){ 0 };
    if (!sdl_status_depth_pane_layout(&layout))
        return false;
    if (out)
        *out = layout.panel;
    return layout.panel.w > 0.0f && layout.panel.h > 0.0f;
}

void sdl_status_depth_pane_render(void)
{
    status_depth_pane_layout layout;
    TTF_Font* font;
    float content_right;

    if (!sdl_status_depth_pane_layout(&layout))
        return;
    font = sdl_story_font_for_height_slot(layout.font_px,
        SDL_STORY_FONT_SLOT_STATUS);
    if (!font)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 5, 7, 9,
        SDL_OVERLAY_LOG_PANE_ALPHA);
    SDL_RenderFillRect(g_state.renderer, &layout.panel);

    content_right = layout.panel.x + layout.panel.w - (float)layout.pad_x;
    for (int i = 0; i < layout.layout_count; i++) {
        const status_depth_pane_layout_item* item = &layout.items[i];
        SDL_Color color = sdl_status_pane_color(item->attr);
        float item_x = content_right - (float)item->right - (float)item->w;
        float item_y = layout.panel.y + (float)layout.pad_y
            + (float)((layout.row_count - 1 - item->row_from_bottom)
                * layout.row_h);

        sdl_status_pane_draw_text(font, item->line, color, item_x, item_y,
            (float)item->w, (float)layout.row_h, false);
    }
}

bool sdl_overlay_pane_anchor_rect(enum pane_type pane_type,
    SDL_Rect* out)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    SDL_Rect anchor;
    SDL_Rect screen;
    bool force_main_anchor;

    if (!out)
        return false;
    *out = (SDL_Rect){ 0 };

    force_main_anchor = pane_type == PANE_DESCRIPTION
        && g_description_overlay_full_main_anchor_depth > 0;

    if (pane_type == PANE_DESCRIPTION
        && !force_main_anchor
        && g_description_overlay_main_anchor_depth > 0)
    {
        SDL_Rect panes[PANE_MAX] = { 0 };

        screen = sdl_get_layout_screen_rect();
        sdl_place_active_panes_fitting_main(&screen, panes, true, true, false,
            NULL, NULL);
        if (sdl_rect_has_area(&panes[PANE_DESCRIPTION]))
        {
            anchor = panes[PANE_DESCRIPTION];
            anchor.x = screen.x + (screen.w - anchor.w) / 2;
            *out = anchor;
            return true;
        }
    }

    if (!force_main_anchor
        && pane_type > PANE_MAIN && pane_type < PANE_MAX
        && sdl_rect_has_area(&g_pane_rects[pane_type]))
    {
        *out = g_pane_rects[pane_type];
        return true;
    }

    screen = sdl_get_layout_screen_rect();
    if (view->term_ready && view->canvas && view->cell_w > 0
        && view->cell_h > 0 && view->rect.w > 0 && view->rect.h > 0)
    {
        int visual_cols = sdl_main_view_visual_cols(view);
        int visual_rows = sdl_main_view_visual_rows(view);

        anchor = (SDL_Rect){
            .x = view->rect.x + view->margin_x,
            .y = view->rect.y + view->margin_y,
            .w = visual_cols * view->cell_w,
            .h = visual_rows * view->cell_h,
        };
    }
    else
    {
        anchor = (SDL_Rect){
            .x = screen.x,
            .y = screen.y,
            .w = screen.w,
            .h = screen.h,
        };
    }

    if (!sdl_rect_has_area(&anchor))
        return false;

    *out = anchor;
    return true;
}

void sdl_push_description_overlay_main_anchor(void)
{
    g_description_overlay_main_anchor_depth++;
}

void sdl_pop_description_overlay_main_anchor(void)
{
    if (g_description_overlay_main_anchor_depth > 0)
        g_description_overlay_main_anchor_depth--;
}

void sdl_push_description_overlay_full_main_anchor(void)
{
    g_description_overlay_full_main_anchor_depth++;
}

void sdl_pop_description_overlay_full_main_anchor(void)
{
    if (g_description_overlay_full_main_anchor_depth > 0)
        g_description_overlay_full_main_anchor_depth--;
}

