#include "angband.h"
#include "sdl/main-sdl-private.h"

bool sdl_point_in_rect(const SDL_Rect* rect, float x, float y)
{
    if (!sdl_rect_has_area(rect))
        return false;

    return x >= (float)rect->x
        && y >= (float)rect->y
        && x < (float)(rect->x + rect->w)
        && y < (float)(rect->y + rect->h);
}

bool sdl_left_panel_pane_hit(float x, float y)
{
    if (get_sdl_hide_left_panel())
        return false;
    if (!sdl_left_panel_pane_runtime_active())
        return false;

    return sdl_point_in_rect(&g_pane_rects[PANE_LEFT_PANEL], x, y);
}

void sdl_left_panel_pane_set_expanded(bool expanded)
{
    if (g_left_panel_pane_expanded == expanded)
        return;

    g_left_panel_pane_expanded = expanded;
    sdl_update_left_panel_pane_rect();
    g_state.need_present = true;
}

bool sdl_point_in_view_rect(enum pane_type pane, float x, float y)
{
    const sdl_view* view;

    if (pane < PANE_MAIN || pane >= PANE_MAX)
        return false;
    if (pane == PANE_LEFT_PANEL)
        return sdl_left_panel_pane_runtime_active()
            && sdl_point_in_rect(&g_pane_rects[PANE_LEFT_PANEL], x, y);
    if (pane == PANE_COMBAT) {
        SDL_Rect rect;

        return sdl_combat_overlay_pane_current_rect(&rect)
            && sdl_point_in_rect(&rect, x, y);
    }
    if ((int)pane >= MAX_TERM_DATA)
        return false;

    view = &g_views[pane];
    if (!view->term_ready || !view->canvas)
        return false;
    /* The overlay log only paints a narrow right-hand band; hit-test that
     * visible strip so taps on the transparent left margin fall through to the
     * map behind it instead of being swallowed by the pane's full grid rect. */
    if (pane == PANE_ROLLS && sdl_view_is_overlay_log_pane(view)) {
        SDL_Rect band;

        return sdl_overlay_log_pane_current_rect(&band)
            && sdl_point_in_rect(&band, x, y);
    }
    return sdl_point_in_rect(&view->rect, x, y);
}

bool sdl_menu_pointer_hits_non_main_pane(float x, float y)
{
    SDL_Rect touch_rect;
    SDL_Rect map_rect;

    if (sdl_touch_pane_current_rect(&touch_rect)
        && sdl_point_in_rect(&touch_rect, x, y))
    {
        return true;
    }

    if (sdl_side_map_pane_current_rect(&map_rect)
        && sdl_point_in_rect(&map_rect, x, y))
    {
        return true;
    }

    if (!sdl_should_show_supporting_panes())
        return false;

    for (int i = PANE_MAIN + 1; i < PANE_MAX; i++)
    {
        if (i == PANE_TOUCH)
            continue;
        if (i == PANE_LEFT_PANEL)
            continue;
        if (sdl_point_in_view_rect((enum pane_type)i, x, y))
            return true;
    }

    return false;
}

/*
 * True when the point falls on a pane drawn on top of the main map: the combat
 * overlay, the overlay log band, the status / depth overlays, a non-modal
 * description overlay, or the side-map / touch pads.  Pointer-to-map conversion
 * treats these as "not the map" so hover popups and mouse/touch pathfinding act
 * on the visible pane instead of the map cell hidden behind it.  (The character
 * panel is handled separately by sdl_main_screen_cell_hits_character_panel,
 * which also covers the compact overlay shown when the panel is hidden.)
 */
bool sdl_main_screen_point_over_overlay_pane(float x, float y)
{
    SDL_Rect rect;
    SDL_FRect frect;

    if (sdl_combat_overlay_pane_current_rect(&rect)
        && sdl_point_in_rect(&rect, x, y))
    {
        return true;
    }
    if (sdl_overlay_log_pane_current_rect(&rect)
        && sdl_point_in_rect(&rect, x, y))
    {
        return true;
    }
    if (sdl_status_pane_current_rect(&rect, NULL)
        && sdl_point_in_rect(&rect, x, y))
    {
        return true;
    }
    if (sdl_depth_menu_pane_current_rect(&frect)
        && sdl_point_in_frect(&frect, x, y))
    {
        return true;
    }
    if (sdl_side_map_pane_current_rect(&rect)
        && sdl_point_in_rect(&rect, x, y))
    {
        return true;
    }
    if (sdl_touch_pane_current_rect(&rect)
        && sdl_point_in_rect(&rect, x, y))
    {
        return true;
    }
    if (sdl_description_overlay_contains_point(x, y))
        return true;

    return false;
}

bool sdl_pane_command_shortcuts_active(void)
{
    return character_generated
        && character_dungeon
        && p_ptr
        && (inkey_flag || character_icky > 0)
        && g_views[PANE_MAIN].term_ready;
}

bool sdl_main_screen_click_shortcuts_active(void)
{
    return sdl_pane_command_shortcuts_active()
        && inkey_flag
        && character_icky == 0
        && !ui_menu_click_is_active();
}

static bool sdl_main_screen_handle_menu_cell_action(int col, int row,
    int action)
{
    if (!ui_menu_click_handle_cell_action(col, row, action))
        return false;

    sdl_unified_look_clear_map_hover();
    Term_keypress((action == UI_MENU_CLICK_SECONDARY)
        ? UI_MENU_CLICK_WAKE_KEY
        : '\r');
    return true;
}

bool sdl_main_screen_handle_menu_text_pointer(float x, float y, int action)
{
    int col = 0;
    int row = 0;

    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;

    return sdl_main_screen_handle_menu_cell_action(col, row, action);
}

static bool sdl_menu_touch_point_to_click_cell(float x, float y,
    bool expanded, int* out_col, int* out_row)
{
#if SIL_SDL_MOBILE_BUILD
    static const int offsets[][2] = {
        {  0, -1 }, {  0,  1 },
        { -1,  0 }, {  1,  0 },
        { -1, -1 }, {  1, -1 }, { -1,  1 }, {  1,  1 },
        { -2,  0 }, {  2,  0 },
        { -2, -1 }, {  2, -1 }, { -2,  1 }, {  2,  1 },
    };
#endif
    int col = 0;
    int row = 0;

    if (!out_col || !out_row)
        return false;
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;

    if (ui_menu_click_has_cell(col, row))
    {
        *out_col = col;
        *out_row = row;
        return true;
    }

#if SIL_SDL_MOBILE_BUILD
    if (!expanded)
        return false;

    for (int i = 0; i < (int)N_ELEMENTS(offsets); i++)
    {
        int test_col = col + offsets[i][0];
        int test_row = row + offsets[i][1];

        if (!ui_menu_click_has_cell(test_col, test_row))
            continue;

        *out_col = test_col;
        *out_row = test_row;
        return true;
    }
#else
    (void)expanded;
#endif

    return false;
}

bool sdl_main_screen_menu_pointer_hits_cell(float x, float y)
{
    int col = 0;
    int row = 0;

    return sdl_main_view_point_to_cell(x, y, &col, &row)
        && ui_menu_click_has_cell(col, row);
}

bool sdl_main_screen_handle_menu_outside_pointer(float x, float y,
    bool primary)
{
    int col = 0;
    int row = 0;
    bool hit_main_cell = false;

    if (!ui_menu_click_outside_cancel_enabled()
        && ui_menu_click_get_touch_category()
            != SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT)
    {
        return false;
    }

    hit_main_cell = sdl_main_view_point_to_cell(x, y, &col, &row);
    if (hit_main_cell && ui_menu_click_has_cell(col, row))
    {
        return false;
    }
    if (hit_main_cell && ui_scroll_area_has_cell(col, row))
    {
        return false;
    }
    if (sdl_menu_pointer_hits_non_main_pane(x, y))
        return false;
    if (primary
        && hit_main_cell
        && sdl_main_screen_menu_outside_armor_cycle_pointer(col, row))
    {
        return true;
    }

    ui_menu_click_clear_pending_hover();
    sdl_unified_look_clear_map_hover();
    Term_keypress(ESCAPE);
    return true;
}

bool sdl_main_screen_menu_outside_armor_cycle_pointer(int col, int row)
{
    if (ui_menu_click_get_touch_category()
        != SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT)
    {
        return false;
    }
    if (!p_ptr || character_icky <= 0)
        return false;
    if (get_sdl_hide_left_panel())
        return false;
    if (row != ROW_EVN || col < 0 || col >= COL_MAP)
        return false;

    ui_menu_click_clear_pending_hover();
    sdl_unified_look_clear_map_hover();
    sdl_enqueue_bypassed_command(
        sdl_inventory_equipment_cycle_binding('i'));
    return true;
}

bool sdl_main_screen_handle_menu_hover_pointer(float x, float y)
{
    int col = 0;
    int row = 0;
    bool wake = false;

    if (g_touch_pane_yes_no_prompt_active)
    {
        ui_menu_click_clear_pending_hover();
        sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
            SDL_PANEL_CLICK_NONE, -1, true);
        return true;
    }

    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (!ui_menu_click_handle_hover_cell(col, row, &wake))
    {
        if (ui_menu_click_clear_hover(&wake) && wake)
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        return false;
    }

    sdl_pointer_attack_set_panel_hover_mode(SDL_POINTER_ATTACK_NONE);
    sdl_unified_look_clear_map_hover();
    sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
        SDL_PANEL_CLICK_NONE, -1, true);
    if (wake)
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_status_line_partition_label_at_col(int row, int col)
{
    cptr long_label = "";
    cptr short_label = "";

    if (!Term || !p_ptr)
        return false;

    switch (level_partition_kind_for_point(p_ptr->py, p_ptr->px))
    {
    case LEVEL_PART_ROOMY:
        long_label = "Room";
        short_label = "Rm";
        break;
    case LEVEL_PART_RUINED:
        long_label = "Ruin";
        short_label = "Ru";
        break;
    case LEVEL_PART_CAVEY:
        long_label = "Cave";
        short_label = "Cv";
        break;
    case LEVEL_PART_BIG_CAVE:
        long_label = "BigCa";
        short_label = "BC";
        break;
    case LEVEL_PART_LABYRINTH:
        long_label = "Labir";
        short_label = "Lb";
        break;
    case LEVEL_PART_CHASM:
        long_label = "Chasm";
        short_label = "Ch";
        break;
    default:
        return false;
    }

    return sdl_screen_segment_col_hits_ci(Term, row, 0, Term->wid, col,
               long_label)
        || sdl_screen_segment_col_hits_ci(Term, row, 0, Term->wid, col,
               short_label);
}

bool sdl_status_line_song_label_at_col(int row, int col)
{
    char song_long[32] = "";
    char song_short[12] = "";

    if (!Term || !p_ptr)
        return false;
    if (sdl_screen_segment_col_hits_ci(Term, row, 0, Term->wid, col,
        "Singing"))
    {
        return true;
    }
    if (p_ptr->song1 == SNG_NOTHING && p_ptr->song2 == SNG_NOTHING)
        return false;

    if (p_ptr->song1 != SNG_NOTHING && p_ptr->song2 != SNG_NOTHING)
    {
        char* song1_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
        char* song2_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;

        strnfmt(song_long, sizeof(song_long), "%s+%s", song1_name + 8,
            song2_name + 8);
    }
    else if (p_ptr->song1 != SNG_NOTHING)
    {
        char* song1_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;

        SDL_strlcpy(song_long, song1_name + 8, sizeof(song_long));
    }
    else if (p_ptr->song2 != SNG_NOTHING)
    {
        char* song2_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;

        SDL_strlcpy(song_long, song2_name + 8, sizeof(song_long));
    }

    if (song_long[0])
        strnfmt(song_short, sizeof(song_short), "S:%.*s", 6, song_long);

    return (song_long[0]
            && sdl_screen_segment_col_hits_ci(Term, row, 0, Term->wid, col,
                song_long))
        || (song_short[0]
            && sdl_screen_segment_col_hits_ci(Term, row, 0, Term->wid, col,
                song_short));
}

bool sdl_status_line_depth_label_at_col(int row, int col)
{
    char depth_long[16] = "";
    char depth_short[16] = "";

    if (!Term || !p_ptr)
        return false;

    if (!p_ptr->depth)
    {
        SDL_strlcpy(depth_long, "Surface", sizeof(depth_long));
        SDL_strlcpy(depth_short, "0'", sizeof(depth_short));
    }
    else
    {
        int feet = p_ptr->depth * 50;

        strnfmt(depth_long, sizeof(depth_long), "%d ft", feet);
        strnfmt(depth_short, sizeof(depth_short), "%d'", feet);
    }

    return sdl_screen_segment_col_hits_ci(Term, row, 0, Term->wid, col,
            depth_long)
        || sdl_screen_segment_col_hits_ci(Term, row, 0, Term->wid, col,
            depth_short);
}

bool sdl_status_line_view_label_at_col(int row, int col)
{
    if (!Term)
        return false;

    return sdl_screen_segment_col_hits_ci(Term, row, 0, Term->wid, col,
            "View")
        || sdl_screen_segment_col_hits_ci(Term, row, 0, Term->wid, col,
            "Vw");
}

bool sdl_status_line_touch_zone_selected(int action, int col, int width)
{
    if (action == SDL_STATUS_CLICK_NONE)
        return false;
    if (g_main_screen_status_selected_action != action)
        return false;
    if (col < 0 || width <= 0)
        return true;

    return g_main_screen_status_selected_col >= col
        && g_main_screen_status_selected_col < col + width;
}

bool sdl_character_panel_touch_zone_selected(int action, int row)
{
    if (action == SDL_PANEL_CLICK_NONE)
        return false;
    if (g_main_screen_panel_selected_action != action)
        return false;
    if (row >= 0 && g_main_screen_panel_selected_row != row)
        return false;

    return true;
}

void sdl_main_screen_touch_zone_selection_set(int status_action,
    int status_col, int panel_action, int panel_row, bool redraw)
{
    if (status_action == g_main_screen_status_selected_action
        && status_col == g_main_screen_status_selected_col
        && panel_action == g_main_screen_panel_selected_action
        && panel_row == g_main_screen_panel_selected_row)
    {
        return;
    }

    g_main_screen_status_selected_action = status_action;
    g_main_screen_status_selected_col = status_col;
    g_main_screen_panel_selected_action = panel_action;
    g_main_screen_panel_selected_row = panel_row;

    if (redraw && p_ptr && character_generated && character_icky == 0)
    {
        p_ptr->redraw |= PR_MISC | PR_BASIC | PR_EXTRA | PR_STATE | PR_DEPTH;
        handle_stuff();
        if (Term)
            Term_fresh();
    }

    g_state.need_present = true;
}

int sdl_status_line_click_action_at_cell(int col, int row)
{
    if (!Term || row != ROW_STATUS)
        return SDL_STATUS_CLICK_NONE;
    if (sdl_status_line_view_label_at_col(row, col))
        return SDL_STATUS_CLICK_VIEW;
    if (sdl_status_line_song_label_at_col(row, col))
        return SDL_STATUS_CLICK_SONG;
    if (sdl_status_line_partition_label_at_col(row, col))
        return SDL_STATUS_CLICK_MAP;
    if (sdl_status_line_depth_label_at_col(row, col))
        return SDL_STATUS_CLICK_MAP;
    if (sdl_screen_char_at(Term, row, col) != ' ')
        return SDL_STATUS_CLICK_MAIN_MENU;

    return SDL_STATUS_CLICK_NONE;
}

bool sdl_main_screen_handle_status_line_hover_pointer(float x, float y)
{
    int col = 0;
    int row = 0;
    bool active = sdl_main_screen_click_shortcuts_active();
    int action = SDL_STATUS_CLICK_NONE;

    if (active && Term && sdl_main_view_point_to_cell(x, y, &col, &row)
        && row == ROW_STATUS)
    {
        action = sdl_status_line_click_action_at_cell(col, row);
    }

    if (action != SDL_STATUS_CLICK_NONE)
        sdl_pointer_attack_set_panel_hover_mode(SDL_POINTER_ATTACK_NONE);

    sdl_main_screen_touch_zone_selection_set(action,
        action != SDL_STATUS_CLICK_NONE ? col : -1,
        SDL_PANEL_CLICK_NONE, -1, active);
    return action != SDL_STATUS_CLICK_NONE;
}

bool sdl_status_line_action_is_corner_exempt(int action)
{
    return action == SDL_STATUS_CLICK_MAIN_MENU
        || action == SDL_STATUS_CLICK_MAP
        || action == SDL_STATUS_CLICK_VIEW;
}

bool sdl_handle_status_line_click_action(int action)
{
    sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
        SDL_PANEL_CLICK_NONE, -1, false);

    if (action == SDL_STATUS_CLICK_SONG)
    {
        sdl_enqueue_bypassed_command('s');
        return true;
    }

    if (action == SDL_STATUS_CLICK_MAP)
    {
        sdl_enqueue_bypassed_command('M');
        return true;
    }

    if (action == SDL_STATUS_CLICK_VIEW)
    {
        sdl_enqueue_bypassed_command('l');
        return true;
    }

    sdl_enqueue_bypassed_command('m');
    return true;
}

bool sdl_main_screen_handle_corner_exempt_status_pointer(float x,
    float y)
{
    int col = 0;
    int row = 0;
    int action = SDL_STATUS_CLICK_NONE;

    if (!sdl_touch_zone_controls_active())
        return false;
    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!Term)
        return false;
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (row != ROW_STATUS)
        return false;

    action = sdl_status_line_click_action_at_cell(col, row);
    if (!sdl_status_line_action_is_corner_exempt(action))
        return false;

    return sdl_handle_status_line_click_action(action);
}

bool sdl_main_screen_handle_status_line_pointer(float x, float y)
{
    int col = 0;
    int row = 0;
    int action = SDL_STATUS_CLICK_NONE;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!Term)
        return false;
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (row != ROW_STATUS)
        return false;

    action = sdl_status_line_click_action_at_cell(col, row);
    return sdl_handle_status_line_click_action(action);
}

bool sdl_screen_row_contains_ci(const term* t, int row, cptr needle)
{
    int needle_len;

    if (!t || !t->scr || !t->scr->c || !needle)
        return false;

    needle_len = (int)strlen(needle);
    if (needle_len <= 0 || row < 0 || row >= t->hgt || needle_len > t->wid)
        return false;

    for (int col = 0; col <= t->wid - needle_len; col++) {
        bool match = true;

        for (int i = 0; i < needle_len; i++) {
            unsigned char actual = (unsigned char)t->scr->c[row][col + i];
            unsigned char expected = (unsigned char)needle[i];

            if (!actual || actual == (unsigned char)t->char_blank)
                actual = ' ';

            if (tolower(actual) != tolower(expected)) {
                match = false;
                break;
            }
        }

        if (match)
            return true;
    }

    return false;
}

unsigned char sdl_screen_char_at(const term* t, int row, int col)
{
    unsigned char ch = ' ';

    if (!t || !t->scr || !t->scr->c)
        return ch;
    if (row < 0 || row >= t->hgt || col < 0 || col >= t->wid)
        return ch;

    ch = (unsigned char)t->scr->c[row][col];
    if (!ch || ch == (unsigned char)t->char_blank)
        ch = ' ';

    return ch;
}

bool sdl_screen_segment_col_hits_ci(const term* t, int row,
    int start_col, int width, int hit_col, cptr needle)
{
    int needle_len;
    int end_col;

    if (!t || !needle)
        return false;
    if (row < 0 || row >= t->hgt || width <= 0)
        return false;
    if (start_col < 0)
    {
        width += start_col;
        start_col = 0;
    }
    if (start_col >= t->wid)
        return false;

    end_col = start_col + width;
    if (end_col > t->wid)
        end_col = t->wid;

    needle_len = (int)strlen(needle);
    if (needle_len <= 0 || needle_len > end_col - start_col)
        return false;

    for (int col = start_col; col <= end_col - needle_len; col++)
    {
        bool match = true;

        for (int i = 0; i < needle_len; i++)
        {
            unsigned char actual = sdl_screen_char_at(t, row, col + i);
            unsigned char expected = (unsigned char)needle[i];

            if (tolower(actual) != tolower(expected))
            {
                match = false;
                break;
            }
        }

        if (match && hit_col >= col && hit_col < col + needle_len)
            return true;
    }

    return false;
}

bool sdl_screen_shows_any_key_prompt(void)
{
    const char* prompts[] = { "any key", "-more-" };
    const term* t = Term;

    if (!t || !t->scr || !t->scr->c)
        return false;

    for (int row = 0; row < t->hgt; row++) {
        for (size_t i = 0; i < N_ELEMENTS(prompts); i++) {
            if (sdl_screen_row_contains_ci(t, row, prompts[i]))
                return true;
        }
    }

    return false;
}

bool sdl_screen_shows_welcome_screen(void)
{
    return sdl_welcome_screen_active();
}

bool sdl_welcome_screen_handle_gamepad_button(SDL_GamepadButton button,
    bool down)
{
    if (!down)
        return false;
    if (!g_sdl_blocking_key_wait)
        return false;
    if (button != SDL_GAMEPAD_BUTTON_BACK
        && button != SDL_GAMEPAD_BUTTON_EAST)
    {
        return false;
    }
    if (!sdl_screen_shows_welcome_screen())
        return false;

    Term_keypress(ESCAPE);
    return true;
}

bool sdl_pointer_activate_welcome_screen(void)
{
    if (!g_sdl_blocking_key_wait)
        return false;
    if (!sdl_screen_shows_welcome_screen())
        return false;

    Term_keypress('\r');
    return true;
}

bool sdl_welcome_screen_handle_pointer_motion(float x, float y)
{
    bool hover_continue;
    bool hover_quit;

    if (!sdl_welcome_screen_active())
        return false;

    hover_continue = (g_sdl_welcome_screen.mode == SDL_WELCOME_SCREEN_MENU)
        && sdl_point_in_frect(&g_sdl_welcome_screen.continue_rect, x, y);
    hover_quit = (g_sdl_welcome_screen.mode == SDL_WELCOME_SCREEN_MENU)
        && sdl_point_in_frect(&g_sdl_welcome_screen.quit_rect, x, y);

    if (g_sdl_welcome_screen.hover_continue != hover_continue
        || g_sdl_welcome_screen.hover_quit != hover_quit)
    {
        g_sdl_welcome_screen.hover_continue = hover_continue;
        g_sdl_welcome_screen.hover_quit = hover_quit;
        sdl_welcome_screen_mark_dirty();
    }

    return true;
}

bool sdl_pointer_activate_welcome_screen_at(float x, float y)
{
    int slot = -1;

    if (!g_sdl_blocking_key_wait)
        return false;
    if (!sdl_screen_shows_welcome_screen())
        return false;

    if (sdl_touch_pane_point_to_slot(x, y, &slot) && slot >= 0)
        return false;

    if (g_sdl_welcome_screen.mode == SDL_WELCOME_SCREEN_MENU
        && sdl_point_in_frect(&g_sdl_welcome_screen.quit_rect, x, y))
    {
        Term_keypress(ESCAPE);
        return true;
    }

    Term_keypress('\r');
    return true;
}

void sdl_welcome_touch_cancel_press(void)
{
    g_welcome_touch_press.active = false;
    g_welcome_touch_press.finger_id = 0;
    g_welcome_touch_press.start_x = 0.0f;
    g_welcome_touch_press.start_y = 0.0f;
}

bool sdl_welcome_touch_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id)
{
    int slot = -1;

    if (!g_sdl_blocking_key_wait)
        return false;
    if (!sdl_screen_shows_welcome_screen())
        return false;
    if (sdl_touch_pane_point_to_slot(x, y, &slot) && slot >= 0)
        return false;

    sdl_welcome_touch_cancel_press();
    g_welcome_touch_press.active = true;
    g_welcome_touch_press.finger_id = finger_id;
    g_welcome_touch_press.start_x = x;
    g_welcome_touch_press.start_y = y;
    return true;
}

bool sdl_welcome_touch_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float raw_dx;
    float raw_dy;
    float threshold;
    float start_x;
    float start_y;
    int dir;

    if (!g_welcome_touch_press.active
        || g_welcome_touch_press.finger_id != finger_id)
    {
        return false;
    }

    raw_dx = x - g_welcome_touch_press.start_x;
    raw_dy = y - g_welcome_touch_press.start_y;
    dx = (raw_dx < 0.0f) ? -raw_dx : raw_dx;
    dy = (raw_dy < 0.0f) ? -raw_dy : raw_dy;

    threshold = sdl_touch_swipe_threshold_px();
    if (dx <= threshold && dy <= threshold)
        return true;

    start_x = g_welcome_touch_press.start_x;
    start_y = g_welcome_touch_press.start_y;
    sdl_welcome_touch_cancel_press();
    dir = sdl_touch_swipe_direction_for_delta(raw_dx, raw_dy, threshold);
    if (dir == 4 || dir == 6)
    {
        Term_keypress((dir == 4) ? '\r' : ESCAPE);
        return true;
    }
    if (sdl_touch_swipe_handle_pointer_down(start_x, start_y, finger_id))
        (void)sdl_touch_swipe_handle_pointer_motion(x, y, finger_id);
    return true;
}

bool sdl_welcome_touch_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float threshold;

    if (!g_welcome_touch_press.active
        || g_welcome_touch_press.finger_id != finger_id)
    {
        return false;
    }

    dx = x - g_welcome_touch_press.start_x;
    dy = y - g_welcome_touch_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_touch_swipe_threshold_px();
    sdl_welcome_touch_cancel_press();
    if (dx > threshold || dy > threshold)
        return true;
    if (!g_sdl_blocking_key_wait || !sdl_screen_shows_welcome_screen())
        return true;

    (void)sdl_pointer_activate_welcome_screen_at(x, y);
    return true;
}

bool sdl_pointer_dismiss_any_key_prompt(void)
{
    if (sdl_pointer_activate_welcome_screen())
        return true;
    if (!g_sdl_blocking_key_wait)
        return false;
    if (ui_key_wait_dismiss_is_active())
    {
        Term_keypress(ui_key_wait_dismiss_get_key());
        return true;
    }
    if (!sdl_screen_shows_any_key_prompt())
        return false;

    Term_keypress('\r');
    return true;
}

void sdl_menu_touch_cancel(void)
{
    g_menu_touch_press.active = false;
    g_menu_touch_press.mouse = false;
    g_menu_touch_press.finger_id = 0;
    g_menu_touch_press.col = 0;
    g_menu_touch_press.row = 0;
    g_menu_touch_press.start_x = 0.0f;
    g_menu_touch_press.start_y = 0.0f;
    g_menu_touch_press.start_time = 0;
}

bool sdl_menu_touch_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int col = 0;
    int row = 0;

    if (!sdl_menu_touch_point_to_click_cell(x, y, !mouse, &col, &row))
        return false;

    if (!mouse
        && !ui_menu_click_outside_cancel_enabled()
        && !config.touch_menu_command_enabled[ui_menu_click_get_touch_category()])
        return true;

    sdl_menu_touch_cancel();
    g_menu_touch_press.active = true;
    g_menu_touch_press.mouse = mouse;
    g_menu_touch_press.finger_id = finger_id;
    g_menu_touch_press.col = col;
    g_menu_touch_press.row = row;
    g_menu_touch_press.start_x = x;
    g_menu_touch_press.start_y = y;
    g_menu_touch_press.start_time = SDL_GetTicksNS();
    return true;
}

bool sdl_menu_touch_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int col = 0;
    int row = 0;
    float dx;
    float dy;

    if (!g_menu_touch_press.active || g_menu_touch_press.mouse != mouse
        || g_menu_touch_press.finger_id != finger_id)
        return false;

    if (!sdl_menu_touch_point_to_click_cell(x, y, !mouse, &col, &row))
    {
        sdl_menu_touch_cancel();
        return true;
    }

    dx = x - g_menu_touch_press.start_x;
    dy = y - g_menu_touch_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    if (col != g_menu_touch_press.col || row != g_menu_touch_press.row
        || dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        if (!mouse && ui_scroll_area_has_cell(g_menu_touch_press.col,
                g_menu_touch_press.row))
        {
            float start_x = g_menu_touch_press.start_x;
            float start_y = g_menu_touch_press.start_y;

            sdl_menu_touch_cancel();
            if (sdl_menu_scroll_handle_pointer_down(start_x, start_y,
                    finger_id))
            {
                (void)sdl_menu_scroll_handle_pointer_motion(x, y, finger_id);
            }
            return true;
        }

        sdl_menu_touch_cancel();
        return true;
    }

    return true;
}

bool sdl_menu_touch_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int col = 0;
    int row = 0;

    if (!g_menu_touch_press.active || g_menu_touch_press.mouse != mouse
        || g_menu_touch_press.finger_id != finger_id)
        return false;

    if (!sdl_menu_touch_point_to_click_cell(x, y, !mouse, &col, &row)
        || col != g_menu_touch_press.col || row != g_menu_touch_press.row)
    {
        sdl_menu_touch_cancel();
        return true;
    }

    sdl_menu_touch_cancel();
    return sdl_main_screen_handle_menu_cell_action(
        col, row, UI_MENU_CLICK_PRIMARY);
}

int sdl_menu_touch_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_menu_touch_press.active)
        return -1;
    if (g_menu_touch_press.mouse)
        return -1;

    elapsed = now_ns - g_menu_touch_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_menu_touch_flush_pending_press(Uint64 now_ns)
{
    int col;
    int row;

    if (!g_menu_touch_press.active)
        return false;
    if (g_menu_touch_press.mouse)
        return false;
    if (now_ns - g_menu_touch_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    col = g_menu_touch_press.col;
    row = g_menu_touch_press.row;
    sdl_menu_touch_cancel();

    if (!ui_menu_click_handle_cell_action(col, row, UI_MENU_CLICK_SECONDARY))
        return false;

    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_screen_back_gesture_active(void)
{
    return g_screen_back_gesture_depth > 0;
}

void sdl_screen_back_touch_cancel(void)
{
    g_screen_back_touch_press.active = false;
    g_screen_back_touch_press.finger_id = 0;
    g_screen_back_touch_press.start_x = 0.0f;
    g_screen_back_touch_press.start_y = 0.0f;
    g_screen_back_touch_press.start_time = 0;
}

void sdl_screen_back_gesture_begin(void)
{
    g_screen_back_gesture_depth++;
}

void sdl_screen_back_gesture_end(void)
{
    if (g_screen_back_gesture_depth > 0)
        g_screen_back_gesture_depth--;

    if (!sdl_screen_back_gesture_active())
    {
        sdl_screen_back_touch_cancel();
    }
}

void sdl_screen_back_gesture_cancel_touch_inputs(void)
{
    sdl_menu_touch_cancel();
    sdl_menu_scroll_cancel();
    sdl_touch_cancel_all_inputs();
}

bool sdl_screen_back_gesture_handle_event(const SDL_Event* ev)
{
    if (!ev)
        return false;

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP
        && ev->button.button == SDL_BUTTON_RIGHT
        && ev->button.which != SDL_TOUCH_MOUSEID
        && g_screen_back_right_button_pending)
    {
        g_screen_back_right_button_pending = false;
        if (sdl_screen_back_gesture_active())
            Term_keypress(ESCAPE);
        return true;
    }

    if ((ev->type == SDL_EVENT_FINGER_UP
            || ev->type == SDL_EVENT_FINGER_CANCELED)
        && g_screen_back_suppress_touch_up
        && ev->tfinger.fingerID == g_screen_back_suppress_touch_finger_id)
    {
        g_screen_back_suppress_touch_up = false;
        g_screen_back_suppress_touch_finger_id = 0;
        return true;
    }

    if ((ev->type == SDL_EVENT_FINGER_DOWN
            || ev->type == SDL_EVENT_FINGER_MOTION
            || ev->type == SDL_EVENT_FINGER_UP
            || ev->type == SDL_EVENT_FINGER_CANCELED)
        && g_touch_pane_yes_no_prompt_active)
    {
        sdl_screen_back_touch_cancel();
        return false;
    }

    if (!sdl_screen_back_gesture_active())
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.button != SDL_BUTTON_RIGHT
            || ev->button.which == SDL_TOUCH_MOUSEID)
        {
            return false;
        }
        g_screen_back_right_button_pending = true;
        return true;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev->button.button != SDL_BUTTON_RIGHT
            || ev->button.which == SDL_TOUCH_MOUSEID)
        {
            return false;
        }
        Term_keypress(ESCAPE);
        return true;

    case SDL_EVENT_FINGER_DOWN:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return false;
        sdl_screen_back_touch_cancel();
        {
            float x;
            float y;

            if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
                return false;
            g_screen_back_touch_press.start_x = x;
            g_screen_back_touch_press.start_y = y;
        }
        g_screen_back_touch_press.active = true;
        g_screen_back_touch_press.finger_id = ev->tfinger.fingerID;
        g_screen_back_touch_press.start_time = SDL_GetTicksNS();
        return false;

    case SDL_EVENT_FINGER_MOTION:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return false;
        if (!g_screen_back_touch_press.active
            || g_screen_back_touch_press.finger_id != ev->tfinger.fingerID)
        {
            return false;
        }
        {
            float x;
            float y;
            float dx;
            float dy;

            if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
                return false;
            dx = x - g_screen_back_touch_press.start_x;
            dy = y - g_screen_back_touch_press.start_y;
            if (dx < 0.0f)
                dx = -dx;
            if (dy < 0.0f)
                dy = -dy;

            if (dx > sdl_touch_swipe_threshold_px()
                || dy > sdl_touch_swipe_threshold_px())
            {
                sdl_screen_back_touch_cancel();
            }
        }
        return false;

    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return false;
        if (g_screen_back_touch_press.active
            && g_screen_back_touch_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_screen_back_touch_cancel();
        }
        return false;

    default:
        return false;
    }
}

int sdl_screen_back_gesture_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!sdl_screen_back_gesture_active())
        return -1;
    if (!g_screen_back_touch_press.active)
        return -1;

    elapsed = now_ns - g_screen_back_touch_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_screen_back_gesture_flush_pending_press(Uint64 now_ns)
{
    SDL_FingerID finger_id;

    if (!sdl_screen_back_gesture_active())
        return false;
    if (!g_screen_back_touch_press.active)
        return false;
    if (now_ns - g_screen_back_touch_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    finger_id = g_screen_back_touch_press.finger_id;
    sdl_screen_back_touch_cancel();
    sdl_screen_back_gesture_cancel_touch_inputs();

    g_screen_back_suppress_touch_up = true;
    g_screen_back_suppress_touch_finger_id = finger_id;
    Term_keypress(ESCAPE);
    return true;
}

void sdl_menu_scroll_send_key_repeated(int key, int count)
{
    if (key == 0)
        return;
    if (count < 0)
        count = -count;
    if (count > 32)
        count = 32;

    for (int i = 0; i < count; i++)
        Term_keypress(key);
}

void sdl_menu_scroll_send_lines(int lines)
{
    if (lines > 0)
        sdl_menu_scroll_send_key_repeated(
            ui_scroll_area_get_vertical_key(1), lines);
    else if (lines < 0)
        sdl_menu_scroll_send_key_repeated(
            ui_scroll_area_get_vertical_key(-1), -lines);
}

bool sdl_menu_scroll_handle_mouse_wheel(const SDL_MouseWheelEvent* wheel)
{
    static sdl_wheel_step_state wheel_state;
    int col = 0;
    int row = 0;
    int lines;

    if (!wheel)
        return false;
    if (wheel->which == SDL_TOUCH_MOUSEID)
        return false;
    if (!sdl_main_view_point_to_cell(wheel->mouse_x, wheel->mouse_y,
        &col, &row))
    {
        return false;
    }
    if (!ui_scroll_area_has_cell(col, row))
        return false;

    lines = sdl_wheel_step_state_consume_axis(&wheel_state, wheel, true);
    if (lines != 0)
    {
        if (ui_scroll_area_has_offset_target())
        {
            bool changed = false;

            for (int i = 0; i < ABS(lines); i++)
                changed |= ui_scroll_area_offset_scroll((lines > 0) ? -1 : 1);
            if (changed)
                Term_keypress(UI_MENU_CLICK_WAKE_KEY);
            return true;
        }
        sdl_menu_scroll_send_lines(lines);
        return true;
    }

    lines = sdl_wheel_step_state_consume_axis(&wheel_state, wheel, false);
    if (lines > 0)
        sdl_menu_scroll_send_key_repeated(
            ui_scroll_area_get_horizontal_key(1), lines);
    else if (lines < 0)
        sdl_menu_scroll_send_key_repeated(
            ui_scroll_area_get_horizontal_key(-1), -lines);

    return true;
}

bool sdl_menu_scroll_handle_mouse_button(float x, float y)
{
    int col = 0;
    int row = 0;
    int tap_key = ui_scroll_area_get_tap_key();

    if (!tap_key)
        return false;
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (!ui_scroll_area_has_cell(col, row))
        return false;

    Term_keypress(tap_key);
    return true;
}

void sdl_menu_scroll_cancel(void)
{
    g_menu_scroll_drag.active = false;
    g_menu_scroll_drag.dragged = false;
    g_menu_scroll_drag.page_fired = false;
    g_menu_scroll_drag.finger_id = 0;
    g_menu_scroll_drag.area_index = -1;
    g_menu_scroll_drag.start_x = 0.0f;
    g_menu_scroll_drag.start_y = 0.0f;
    g_menu_scroll_drag.last_y = 0.0f;
    g_menu_scroll_drag.accum_y = 0.0f;
}

int sdl_minimap_clamp_zoom_step(int step)
{
    if (step < 0)
        return 0;
    if (step > MINIMAP_MAX_ZOOM_STEP)
        return MINIMAP_MAX_ZOOM_STEP;
    return step;
}

float sdl_minimap_zoom_factor_for_step(int step)
{
    static const float factors[MINIMAP_MAX_ZOOM_STEP + 1] = {
        1.0f, 1.25f, 1.5f, 2.0f, 2.6f, 3.4f, 4.5f, 6.0f, 8.0f
    };

    return factors[sdl_minimap_clamp_zoom_step(step)];
}

float sdl_minimap_zoom_factor(void)
{
    return sdl_minimap_zoom_factor_for_step(g_minimap.zoom_step);
}

int sdl_minimap_zoom_step_for_factor(float factor)
{
    if (factor <= sdl_minimap_zoom_factor_for_step(0))
        return 0;

    for (int step = 1; step <= MINIMAP_MAX_ZOOM_STEP; step++) {
        if (factor <= sdl_minimap_zoom_factor_for_step(step))
            return step;
    }

    return MINIMAP_MAX_ZOOM_STEP;
}

int sdl_minimap_default_zoom_step(float fit_scale, const sdl_view* d)
{
    float normal_scale;
    float normal_factor;
    int normal_step;

    if (!d || fit_scale <= 0.0f || d->cell_h <= 0)
        return 0;

    normal_scale = (float)d->cell_h / (float)TILE_SIZE;
    normal_factor = normal_scale / fit_scale;
    normal_step = sdl_minimap_zoom_step_for_factor(normal_factor);

    return sdl_minimap_clamp_zoom_step((normal_step + 1) / 2);
}

float sdl_minimap_clampf(float value, float min_value, float max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

bool sdl_minimap_point_in_rect(float x, float y, const SDL_FRect* rect)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return false;

    return x >= rect->x && x < rect->x + rect->w
        && y >= rect->y && y < rect->y + rect->h;
}

bool sdl_minimap_point_in_rect_expanded(float x, float y,
    const SDL_FRect* rect, float pad)
{
    SDL_FRect expanded;

    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return false;
    if (pad < 0.0f)
        pad = 0.0f;

    expanded = *rect;
    expanded.x -= pad;
    expanded.y -= pad;
    expanded.w += pad * 2.0f;
    expanded.h += pad * 2.0f;
    return sdl_minimap_point_in_rect(x, y, &expanded);
}

bool sdl_minimap_window_to_canvas_point(float x, float y,
    float* out_x, float* out_y)
{
    const sdl_view* d = &g_views[PANE_MAIN];
    float grid_x;
    float grid_y;
    float grid_w;
    float grid_h;
    float local_x;
    float local_y;

    if (!out_x || !out_y)
        return false;
    if (!d->term_ready || !d->canvas || d->cell_w <= 0 || d->cell_h <= 0
        || d->cols <= 0 || d->rows <= 0)
    {
        return false;
    }

    grid_x = (float)(d->rect.x + d->margin_x);
    grid_y = (float)(d->rect.y + d->margin_y);
    grid_w = (float)(d->cols * d->cell_w);
    grid_h = (float)(d->rows * d->cell_h);
    local_x = x - grid_x;
    local_y = y - grid_y;

    if (local_x < 0.0f || local_y < 0.0f
        || local_x >= grid_w || local_y >= grid_h)
    {
        return false;
    }

    *out_x = local_x;
    *out_y = local_y;
    return true;
}

bool sdl_minimap_focus_point_valid(int y, int x)
{
    return p_ptr && y >= 0 && x >= 0
        && y < p_ptr->cur_map_hgt && x < p_ptr->cur_map_wid;
}

bool sdl_minimap_hint_source_in_bounds(const hint_message_meta* meta)
{
    return meta && sdl_minimap_focus_point_valid(meta->source_y,
        meta->source_x);
}

bool sdl_minimap_has_hint_source_at(int y, int x)
{
    byte count;

    if (!sdl_minimap_focus_point_valid(y, x))
        return false;

    count = hint_messages_count_for_save();
    for (int i = 0; i < count; i++) {
        hint_message_meta meta;

        hint_messages_message_meta(i, &meta);
        if (!sdl_minimap_hint_source_in_bounds(&meta))
            continue;
        if (meta.source_y == y && meta.source_x == x)
            return true;
    }

    return false;
}

bool sdl_minimap_grid_opened(int y, int x)
{
    int m_idx;

    if (!sdl_minimap_focus_point_valid(y, x))
        return false;

    m_idx = cave_m_idx[y][x];

    return (cave_info[y][x] & (CAVE_MARK | CAVE_SEEN))
        || m_idx < 0
        || ((m_idx > 0)
            && (mon_list[m_idx].ml || (mon_list[m_idx].mflag & MFLAG_MARK)));
}

void sdl_minimap_focus(int y, int x)
{
    if (!p_ptr->is_dead && g_labyrinth_view_active)
        return;

    if (!sdl_minimap_grid_opened(y, x)
        && !sdl_minimap_has_hint_source_at(y, x))
    {
        return;
    }

    g_minimap_pending_focus_active = true;
    g_minimap_pending_focus_y = y;
    g_minimap_pending_focus_x = x;

    if (g_minimap.active) {
        g_minimap.focus_active = true;
        g_minimap.focus_y = y;
        g_minimap.focus_x = x;
    }
}

void sdl_minimap_clear_gamepad_modal_state(void)
{
    g_gamepad_state.dpad_up = false;
    g_gamepad_state.dpad_down = false;
    g_gamepad_state.dpad_left = false;
    g_gamepad_state.dpad_right = false;
    g_gamepad_state.dpad_dir = 0;
    sdl_gamepad_clear_pending_dpad();
    sdl_gamepad_clear_pending_confirm();

    g_gamepad_state.left_x = 0;
    g_gamepad_state.left_y = 0;
    g_gamepad_state.left_dir = 0;
    g_gamepad_state.left_bind_dir = -1;
    sdl_gamepad_clear_pending_left_stick();

    g_gamepad_state.right_x = 0;
    g_gamepad_state.right_y = 0;
    g_gamepad_state.right_dir = -1;

    g_gamepad_state.left_trigger_down = false;
    g_gamepad_state.right_trigger_down = false;
    g_gamepad_state.left_shoulder_down = false;
    g_gamepad_state.right_shoulder_down = false;
    sdl_gamepad_clear_pending_shoulder();

    g_gamepad_state.shift_held = 0;
    g_gamepad_state.alt_held = 0;
    g_gamepad_state.ctrl_held = g_touch_pane_ctrl_toggle ? 1 : 0;
}

void sdl_minimap_clear_touches(void)
{
    memset(g_minimap.fingers, 0, sizeof(g_minimap.fingers));
    g_minimap.drag_active = false;
    g_minimap.drag_mouse = false;
    g_minimap.drag_finger_id = 0;
    g_minimap.drag_last_x = 0.0f;
    g_minimap.drag_last_y = 0.0f;
    g_minimap.pinch_active = false;
    g_minimap.pinch_finger_a = -1;
    g_minimap.pinch_finger_b = -1;
    g_minimap.pinch_start_distance = 0.0f;
    g_minimap.pinch_start_zoom_step = 0;
}

void sdl_minimap_clear_map_layout(void)
{
    g_minimap.map_layout_valid = false;
    g_minimap.map_rect = (SDL_FRect){0};
    g_minimap.map_min_y = 0;
    g_minimap.map_min_x = 0;
    g_minimap.map_max_y = 0;
    g_minimap.map_max_x = 0;
}

void sdl_minimap_store_map_layout(const SDL_FRect* map_rect,
    int min_y, int min_x, int max_y, int max_x)
{
    if (!g_minimap.active || !map_rect || map_rect->w <= 0.0f
        || map_rect->h <= 0.0f || max_y < min_y || max_x < min_x)
    {
        sdl_minimap_clear_map_layout();
        return;
    }

    g_minimap.map_layout_valid = true;
    g_minimap.map_rect = *map_rect;
    g_minimap.map_min_y = min_y;
    g_minimap.map_min_x = min_x;
    g_minimap.map_max_y = max_y;
    g_minimap.map_max_x = max_x;
}

void sdl_minimap_begin(void)
{
    memset(&g_minimap, 0, sizeof(g_minimap));
    g_minimap.active = true;
    g_minimap.zoom_step = 0;
    g_minimap.pending_hint_index = -1;
    g_minimap.default_zoom_pending = true;
    if (g_minimap_pending_focus_active
        && (sdl_minimap_grid_opened(g_minimap_pending_focus_y,
                g_minimap_pending_focus_x)
            || sdl_minimap_has_hint_source_at(g_minimap_pending_focus_y,
                g_minimap_pending_focus_x)))
    {
        g_minimap.focus_active = true;
        g_minimap.focus_y = g_minimap_pending_focus_y;
        g_minimap.focus_x = g_minimap_pending_focus_x;
    }
    g_minimap_pending_focus_active = false;
    sdl_minimap_clear_gamepad_modal_state();
    sdl_minimap_clear_touches();
}

void sdl_minimap_end(void)
{
    g_minimap.active = false;
    sdl_minimap_clear_map_layout();
    sdl_minimap_clear_touches();
    sdl_minimap_clear_gamepad_modal_state();
}

bool sdl_minimap_redraw(void)
{
    if (!g_minimap.active)
        return false;

    return sdl_display_pixel_map(NULL, NULL);
}

bool sdl_minimap_set_zoom_step(int step)
{
    int new_step = sdl_minimap_clamp_zoom_step(step);

    if (new_step == g_minimap.zoom_step)
        return false;

    g_minimap.zoom_step = new_step;
    g_minimap.default_zoom_pending = false;
    (void)sdl_minimap_redraw();
    return true;
}

bool sdl_minimap_adjust_zoom(int delta)
{
    if (!g_minimap.active || delta == 0)
        return false;

    return sdl_minimap_set_zoom_step(g_minimap.zoom_step + delta);
}

bool sdl_minimap_offset_by(float dx, float dy)
{
    if (!g_minimap.active)
        return false;
    if (dx == 0.0f && dy == 0.0f)
        return false;

    g_minimap.pan_x += dx;
    g_minimap.pan_y += dy;
    return sdl_minimap_redraw();
}

bool sdl_minimap_pan(int dx, int dy)
{
    const sdl_view* d = &g_views[PANE_MAIN];
    float step = 48.0f;

    if (!g_minimap.active || (dx == 0 && dy == 0))
        return false;

    if (d->term_ready && d->cell_h > 0) {
        step = (float)d->cell_h * 3.0f;
        if (step < 32.0f)
            step = 32.0f;
        if (step > 96.0f)
            step = 96.0f;
    }

    return sdl_minimap_offset_by(-(float)dx * step, -(float)dy * step);
}

void sdl_minimap_cancel_drag(void)
{
    g_minimap.drag_active = false;
    g_minimap.drag_mouse = false;
    g_minimap.drag_finger_id = 0;
    g_minimap.drag_last_x = 0.0f;
    g_minimap.drag_last_y = 0.0f;
}

void sdl_minimap_begin_drag(bool mouse, SDL_FingerID finger_id,
    float x, float y)
{
    g_minimap.drag_active = true;
    g_minimap.drag_mouse = mouse;
    g_minimap.drag_finger_id = finger_id;
    g_minimap.drag_last_x = x;
    g_minimap.drag_last_y = y;
}

float sdl_minimap_tap_threshold_px(void)
{
    float threshold = sdl_touch_swipe_threshold_px() * 0.5f;

    if (threshold < 8.0f)
        threshold = 8.0f;
    if (threshold > 32.0f)
        threshold = 32.0f;

    return threshold;
}

bool sdl_minimap_finger_moved_from_start(
    const minimap_touch_finger* finger, float x, float y)
{
    float dx;
    float dy;
    float threshold;

    if (!finger || !finger->active)
        return true;

    dx = x - finger->start_x;
    dy = y - finger->start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_minimap_tap_threshold_px();
    return dx > threshold || dy > threshold;
}

bool sdl_minimap_drag_to(bool mouse, SDL_FingerID finger_id, float x,
    float y)
{
    float dx;
    float dy;

    if (!g_minimap.drag_active || g_minimap.drag_mouse != mouse)
        return false;
    if (!mouse && g_minimap.drag_finger_id != finger_id)
        return false;

    dx = x - g_minimap.drag_last_x;
    dy = y - g_minimap.drag_last_y;
    g_minimap.drag_last_x = x;
    g_minimap.drag_last_y = y;

    (void)sdl_minimap_offset_by(dx, dy);
    return true;
}

void sdl_minimap_close(void)
{
    Term_keypress(ESCAPE);
}

bool sdl_minimap_handle_control_point(float x, float y)
{
    const float hit_pad = 7.0f;

    if (!g_minimap.active)
        return false;

    if (sdl_minimap_point_in_rect(x, y, &g_minimap.close_rect)) {
        sdl_minimap_close();
        return true;
    }

    if (sdl_minimap_point_in_rect(x, y, &g_minimap.zoom_in_rect)) {
        if (g_minimap.zoom_in_enabled)
            (void)sdl_minimap_adjust_zoom(1);
        return true;
    }

    if (sdl_minimap_point_in_rect(x, y, &g_minimap.zoom_out_rect)) {
        if (g_minimap.zoom_out_enabled)
            (void)sdl_minimap_adjust_zoom(-1);
        return true;
    }

    if (sdl_minimap_point_in_rect_expanded(x, y, &g_minimap.close_rect,
            hit_pad))
    {
        sdl_minimap_close();
        return true;
    }

    if (sdl_minimap_point_in_rect_expanded(x, y, &g_minimap.zoom_in_rect,
            hit_pad))
    {
        if (g_minimap.zoom_in_enabled)
            (void)sdl_minimap_adjust_zoom(1);
        return true;
    }

    if (sdl_minimap_point_in_rect_expanded(x, y, &g_minimap.zoom_out_rect,
            hit_pad))
    {
        if (g_minimap.zoom_out_enabled)
            (void)sdl_minimap_adjust_zoom(-1);
        return true;
    }

    return false;
}

bool sdl_minimap_hint_source_marker_rect(const hint_message_meta* meta,
    const SDL_FRect* map_dst, int min_y, int min_x, int max_y, int max_x,
    float min_marker, SDL_FRect* out_marker)
{
    int map_rows;
    int map_cols;
    float grid_w;
    float grid_h;
    SDL_FRect cell;
    SDL_FRect marker;
    float center_x;
    float center_y;

    if (!meta || !map_dst || !out_marker)
        return false;
    if (meta->source_y < min_y || meta->source_y > max_y
        || meta->source_x < min_x || meta->source_x > max_x)
    {
        return false;
    }

    map_rows = max_y - min_y + 1;
    map_cols = max_x - min_x + 1;
    if (map_rows <= 0 || map_cols <= 0)
        return false;

    grid_w = map_dst->w / (float)map_cols;
    grid_h = map_dst->h / (float)map_rows;
    cell.x = map_dst->x + (float)(meta->source_x - min_x) * grid_w;
    cell.y = map_dst->y + (float)(meta->source_y - min_y) * grid_h;
    cell.w = grid_w;
    cell.h = grid_h;

    center_x = cell.x + cell.w * 0.5f;
    center_y = cell.y + cell.h * 0.5f;
    marker = cell;
    if (marker.w < min_marker) {
        marker.w = min_marker;
        marker.x = center_x - marker.w * 0.5f;
    }
    if (marker.h < min_marker) {
        marker.h = min_marker;
        marker.y = center_y - marker.h * 0.5f;
    }

    *out_marker = marker;
    return true;
}

bool sdl_minimap_grid_at_canvas_point(float x, float y,
    int* out_y, int* out_x)
{
    int map_rows;
    int map_cols;
    int grid_y;
    int grid_x;

    if (!out_y || !out_x)
        return false;
    if (!g_minimap.map_layout_valid)
        return false;
    if (!sdl_minimap_point_in_rect(x, y, &g_minimap.map_rect))
        return false;

    map_rows = g_minimap.map_max_y - g_minimap.map_min_y + 1;
    map_cols = g_minimap.map_max_x - g_minimap.map_min_x + 1;
    if (map_rows <= 0 || map_cols <= 0)
        return false;

    grid_x = (int)(((x - g_minimap.map_rect.x) / g_minimap.map_rect.w)
        * (float)map_cols);
    grid_y = (int)(((y - g_minimap.map_rect.y) / g_minimap.map_rect.h)
        * (float)map_rows);
    if (grid_x < 0)
        grid_x = 0;
    if (grid_y < 0)
        grid_y = 0;
    if (grid_x >= map_cols)
        grid_x = map_cols - 1;
    if (grid_y >= map_rows)
        grid_y = map_rows - 1;

    *out_y = g_minimap.map_min_y + grid_y;
    *out_x = g_minimap.map_min_x + grid_x;
    return true;
}

bool sdl_minimap_hint_source_at_canvas_point(float x, float y,
    int* out_index, int* out_y, int* out_x)
{
    byte count;
    int grid_y = -1;
    int grid_x = -1;

    if (!g_minimap.map_layout_valid)
        return false;

    count = hint_messages_count_for_save();
    for (int i = (int)count - 1; i >= 0; --i) {
        hint_message_meta meta;
        SDL_FRect marker;

        hint_messages_message_meta(i, &meta);
        if (!sdl_minimap_hint_source_valid(&meta))
            continue;
        if (!sdl_minimap_hint_source_marker_rect(&meta, &g_minimap.map_rect,
                g_minimap.map_min_y, g_minimap.map_min_x,
                g_minimap.map_max_y, g_minimap.map_max_x, 12.0f, &marker))
        {
            continue;
        }
        if (!sdl_minimap_point_in_rect_expanded(x, y, &marker, 6.0f))
            continue;

        if (out_index)
            *out_index = i;
        if (out_y)
            *out_y = meta.source_y;
        if (out_x)
            *out_x = meta.source_x;
        return true;
    }

    if (!sdl_minimap_grid_at_canvas_point(x, y, &grid_y, &grid_x))
        return false;

    for (int i = (int)count - 1; i >= 0; --i) {
        hint_message_meta meta;

        hint_messages_message_meta(i, &meta);
        if (!sdl_minimap_hint_source_valid(&meta))
            continue;
        if (meta.source_y != grid_y || meta.source_x != grid_x)
            continue;

        if (out_index)
            *out_index = i;
        if (out_y)
            *out_y = meta.source_y;
        if (out_x)
            *out_x = meta.source_x;
        return true;
    }

    return false;
}

bool sdl_minimap_focus_hint_source_at_canvas_point(float x, float y)
{
    int map_y = -1;
    int map_x = -1;

    if (!sdl_minimap_hint_source_at_canvas_point(x, y, NULL, &map_y,
            &map_x))
    {
        return false;
    }

    sdl_minimap_focus(map_y, map_x);
    (void)sdl_minimap_redraw();
    return true;
}

bool sdl_minimap_queue_hint_source_at_canvas_point(float x, float y)
{
    int index = -1;
    int map_y = -1;
    int map_x = -1;

    if (!sdl_minimap_hint_source_at_canvas_point(x, y, &index, &map_y,
            &map_x))
    {
        return false;
    }

    g_minimap.focus_active = true;
    g_minimap.focus_y = map_y;
    g_minimap.focus_x = map_x;
    g_minimap_pending_focus_active = true;
    g_minimap_pending_focus_y = map_y;
    g_minimap_pending_focus_x = map_x;
    g_minimap.pending_hint_open = true;
    g_minimap.pending_hint_index = index;
    (void)sdl_minimap_redraw();
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_minimap_take_hint_click(int* out_index)
{
    if (!g_minimap.pending_hint_open)
        return false;

    if (out_index)
        *out_index = g_minimap.pending_hint_index;
    g_minimap.pending_hint_open = false;
    g_minimap.pending_hint_index = -1;
    return true;
}

int sdl_minimap_find_finger(SDL_FingerID finger_id)
{
    for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
        if (g_minimap.fingers[i].active
            && g_minimap.fingers[i].finger_id == finger_id)
        {
            return i;
        }
    }

    return -1;
}

int sdl_minimap_active_finger_count(void)
{
    int count = 0;

    for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
        if (g_minimap.fingers[i].active)
            count++;
    }

    return count;
}

bool sdl_minimap_first_two_fingers(int* out_a, int* out_b)
{
    int first = -1;

    for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
        if (!g_minimap.fingers[i].active)
            continue;
        if (first < 0) {
            first = i;
            continue;
        }
        if (out_a) *out_a = first;
        if (out_b) *out_b = i;
        return true;
    }

    return false;
}

float sdl_minimap_finger_distance(int a, int b)
{
    float dx;
    float dy;

    if (a < 0 || a >= MINIMAP_MAX_TOUCH_FINGERS
        || b < 0 || b >= MINIMAP_MAX_TOUCH_FINGERS
        || !g_minimap.fingers[a].active || !g_minimap.fingers[b].active)
    {
        return 0.0f;
    }

    dx = g_minimap.fingers[a].x - g_minimap.fingers[b].x;
    dy = g_minimap.fingers[a].y - g_minimap.fingers[b].y;
    return SDL_sqrtf(dx * dx + dy * dy);
}

void sdl_minimap_start_pinch_if_possible(void)
{
    int a = -1;
    int b = -1;
    float distance;

    if (!sdl_minimap_first_two_fingers(&a, &b)) {
        g_minimap.pinch_active = false;
        return;
    }

    distance = sdl_minimap_finger_distance(a, b);
    if (distance < 8.0f) {
        g_minimap.pinch_active = false;
        return;
    }

    g_minimap.pinch_active = true;
    g_minimap.pinch_finger_a = a;
    g_minimap.pinch_finger_b = b;
    g_minimap.pinch_start_distance = distance;
    g_minimap.pinch_start_zoom_step = g_minimap.zoom_step;
}

void sdl_minimap_start_drag_from_first_finger(void)
{
    for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
        if (!g_minimap.fingers[i].active)
            continue;

        sdl_minimap_begin_drag(false, g_minimap.fingers[i].finger_id,
            g_minimap.fingers[i].x, g_minimap.fingers[i].y);
        return;
    }

    sdl_minimap_cancel_drag();
}

int sdl_minimap_zoom_delta_for_pinch_ratio(float ratio)
{
    int delta = 0;
    const float step_ratio = 1.18f;
    const float inverse_step_ratio = 1.0f / 1.18f;

    while (ratio >= step_ratio && delta < MINIMAP_MAX_ZOOM_STEP) {
        delta++;
        ratio /= step_ratio;
    }

    while (ratio <= inverse_step_ratio && delta > -MINIMAP_MAX_ZOOM_STEP) {
        delta--;
        ratio *= step_ratio;
    }

    return delta;
}

bool sdl_minimap_update_pinch(void)
{
    float distance;
    float ratio;
    int delta;
    int target_step;

    if (!g_minimap.pinch_active)
        return false;
    if (g_minimap.pinch_finger_a < 0
        || g_minimap.pinch_finger_b < 0
        || g_minimap.pinch_start_distance < 8.0f)
    {
        return false;
    }

    distance = sdl_minimap_finger_distance(g_minimap.pinch_finger_a,
        g_minimap.pinch_finger_b);
    if (distance < 8.0f)
        return false;

    ratio = distance / g_minimap.pinch_start_distance;
    delta = sdl_minimap_zoom_delta_for_pinch_ratio(ratio);
    target_step = g_minimap.pinch_start_zoom_step + delta;

    return sdl_minimap_set_zoom_step(target_step);
}

void sdl_minimap_add_or_update_finger(SDL_FingerID finger_id, float x,
    float y)
{
    int index = sdl_minimap_find_finger(finger_id);
    bool added = false;

    if (index < 0) {
        for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
            if (!g_minimap.fingers[i].active) {
                index = i;
                g_minimap.fingers[i].active = true;
                g_minimap.fingers[i].finger_id = finger_id;
                g_minimap.fingers[i].start_x = x;
                g_minimap.fingers[i].start_y = y;
                g_minimap.fingers[i].moved = false;
                added = true;
                break;
            }
        }
    }

    if (index < 0)
        return;

    if (!added
        && sdl_minimap_finger_moved_from_start(&g_minimap.fingers[index],
            x, y))
    {
        g_minimap.fingers[index].moved = true;
    }

    g_minimap.fingers[index].x = x;
    g_minimap.fingers[index].y = y;
}

void sdl_minimap_remove_finger(SDL_FingerID finger_id)
{
    int index = sdl_minimap_find_finger(finger_id);

    if (index >= 0)
        memset(&g_minimap.fingers[index], 0, sizeof(g_minimap.fingers[index]));

    if (g_minimap.drag_active && !g_minimap.drag_mouse
        && g_minimap.drag_finger_id == finger_id)
    {
        sdl_minimap_cancel_drag();
    }

    if (sdl_minimap_active_finger_count() >= 2)
        sdl_minimap_start_pinch_if_possible();
    else {
        g_minimap.pinch_active = false;
        if (sdl_minimap_active_finger_count() == 1)
            sdl_minimap_start_drag_from_first_finger();
    }
}

bool sdl_minimap_handle_touch_down(float x, float y,
    SDL_FingerID finger_id)
{
    float canvas_x;
    float canvas_y;

    if (!sdl_minimap_window_to_canvas_point(x, y, &canvas_x, &canvas_y))
        return true;

    if (sdl_minimap_handle_control_point(canvas_x, canvas_y)) {
        sdl_minimap_clear_touches();
        return true;
    }

    if (sdl_minimap_focus_hint_source_at_canvas_point(canvas_x, canvas_y))
        return true;

    sdl_minimap_add_or_update_finger(finger_id, canvas_x, canvas_y);
    if (sdl_minimap_active_finger_count() >= 2) {
        sdl_minimap_cancel_drag();
        sdl_minimap_start_pinch_if_possible();
    } else {
        sdl_minimap_begin_drag(false, finger_id, canvas_x, canvas_y);
    }
    return true;
}

bool sdl_minimap_handle_touch_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float canvas_x;
    float canvas_y;

    if (sdl_minimap_find_finger(finger_id) < 0)
        return true;
    if (!sdl_minimap_window_to_canvas_point(x, y, &canvas_x, &canvas_y))
        return true;

    sdl_minimap_add_or_update_finger(finger_id, canvas_x, canvas_y);
    if (g_minimap.pinch_active)
        (void)sdl_minimap_update_pinch();
    else
        (void)sdl_minimap_drag_to(false, finger_id, canvas_x, canvas_y);
    return true;
}

bool sdl_minimap_handle_touch_up(float x, float y,
    SDL_FingerID finger_id)
{
    int index = sdl_minimap_find_finger(finger_id);
    bool close_on_tap = false;

    if (index >= 0) {
        minimap_touch_finger* finger = &g_minimap.fingers[index];
        bool moved = finger->moved;
        float canvas_x;
        float canvas_y;

        if (sdl_minimap_window_to_canvas_point(x, y, &canvas_x, &canvas_y))
            moved = moved || sdl_minimap_finger_moved_from_start(finger,
                canvas_x, canvas_y);
        else
            moved = true;

        close_on_tap = !moved
            && !g_minimap.pinch_active
            && sdl_minimap_active_finger_count() == 1;
    }

    sdl_minimap_remove_finger(finger_id);
    if (close_on_tap)
        sdl_minimap_close();
    return true;
}

bool sdl_minimap_handle_mouse_wheel(const SDL_MouseWheelEvent* wheel)
{
    static sdl_wheel_step_state wheel_state;
    int steps;

    if (!wheel)
        return false;
    if (wheel->which == SDL_TOUCH_MOUSEID)
        return true;

    steps = sdl_wheel_step_state_consume_primary_axis(&wheel_state, wheel);
    if (steps != 0)
        (void)sdl_minimap_adjust_zoom(steps);

    return true;
}

bool sdl_minimap_handle_mouse_button(const SDL_MouseButtonEvent* button)
{
    float canvas_x = 0.0f;
    float canvas_y = 0.0f;
    bool has_canvas_point;

    if (!button)
        return false;
    if (button->which == SDL_TOUCH_MOUSEID)
        return true;

    has_canvas_point = sdl_minimap_window_to_canvas_point(
        (float)button->x, (float)button->y, &canvas_x, &canvas_y);

    if (button->down && button->button == SDL_BUTTON_LEFT)
    {
        if (has_canvas_point
            && !sdl_minimap_handle_control_point(canvas_x, canvas_y))
        {
            if (!sdl_minimap_focus_hint_source_at_canvas_point(canvas_x,
                    canvas_y))
            {
                sdl_minimap_begin_drag(true, 0, canvas_x, canvas_y);
            }
        }
        return true;
    }

    if (!button->down && button->button == SDL_BUTTON_LEFT)
    {
        if (g_minimap.drag_active && g_minimap.drag_mouse)
            sdl_minimap_cancel_drag();
        return true;
    }

    if (button->down && button->button == SDL_BUTTON_RIGHT)
    {
        if (has_canvas_point
            && sdl_minimap_queue_hint_source_at_canvas_point(canvas_x,
                canvas_y))
        {
            return true;
        }

        sdl_minimap_close();
        return true;
    }

    return true;
}

bool sdl_minimap_handle_key(const SDL_KeyboardEvent* key_event)
{
    int dx = 0;
    int dy = 0;
    SDL_Keycode key;

    if (!key_event)
        return false;
    if (!key_event->down)
        return true;

    key = key_event->key;
    if (sdl_key_is_escape_or_back(key)) {
        sdl_minimap_close();
        return true;
    }

    switch (key)
    {
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_LGUI:
    case SDLK_RGUI:
        return true;

    case SDLK_PLUS:
    case SDLK_EQUALS:
    case SDLK_KP_PLUS:
        (void)sdl_minimap_adjust_zoom(1);
        return true;

    case SDLK_MINUS:
    case SDLK_UNDERSCORE:
    case SDLK_KP_MINUS:
        (void)sdl_minimap_adjust_zoom(-1);
        return true;

    case SDLK_LEFT:
    case SDLK_KP_4:
        dx = -1;
        break;
    case SDLK_RIGHT:
    case SDLK_KP_6:
        dx = 1;
        break;
    case SDLK_UP:
    case SDLK_KP_8:
        dy = -1;
        break;
    case SDLK_DOWN:
    case SDLK_KP_2:
        dy = 1;
        break;
    case SDLK_HOME:
    case SDLK_KP_7:
        dx = -1;
        dy = -1;
        break;
    case SDLK_PAGEUP:
    case SDLK_KP_9:
        dx = 1;
        dy = -1;
        break;
    case SDLK_END:
    case SDLK_KP_1:
        dx = -1;
        dy = 1;
        break;
    case SDLK_PAGEDOWN:
    case SDLK_KP_3:
        dx = 1;
        dy = 1;
        break;
    default:
        return false;
    }

    (void)sdl_minimap_pan(dx, dy);
    return true;
}

bool sdl_minimap_handle_event(const SDL_Event* ev)
{
    if (!g_minimap.active || !ev)
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        if (ev->motion.which != SDL_TOUCH_MOUSEID) {
            float canvas_x;
            float canvas_y;

            if (sdl_minimap_window_to_canvas_point(ev->motion.x,
                    ev->motion.y, &canvas_x, &canvas_y))
            {
                (void)sdl_minimap_drag_to(true, 0, canvas_x, canvas_y);
            }
        }
        return true;

    case SDL_EVENT_MOUSE_WHEEL:
        return sdl_minimap_handle_mouse_wheel(&ev->wheel);

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return sdl_minimap_handle_mouse_button(&ev->button);

    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
    {
        float x;
        float y;

        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;

        sdl_note_touch_event_device(ev->tfinger.touchID);
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return true;

        if (ev->type == SDL_EVENT_FINGER_DOWN)
            return sdl_minimap_handle_touch_down(x, y,
                ev->tfinger.fingerID);
        if (ev->type == SDL_EVENT_FINGER_MOTION)
            return sdl_minimap_handle_touch_motion(x, y,
                ev->tfinger.fingerID);
        if (ev->type == SDL_EVENT_FINGER_CANCELED) {
            sdl_minimap_remove_finger(ev->tfinger.fingerID);
            return true;
        }

        return sdl_minimap_handle_touch_up(x, y, ev->tfinger.fingerID);
    }

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        return sdl_minimap_handle_key(&ev->key);

    default:
        return false;
    }
}

bool sdl_minimap_handle_gamepad_button(SDL_GamepadButton button,
    bool down)
{
    if (!g_minimap.active)
        return false;
    if (!down)
        return true;

    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        (void)sdl_minimap_adjust_zoom(-1);
        break;

    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        (void)sdl_minimap_adjust_zoom(1);
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        (void)sdl_minimap_pan(-1, 0);
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        (void)sdl_minimap_pan(1, 0);
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        (void)sdl_minimap_pan(0, -1);
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        (void)sdl_minimap_pan(0, 1);
        break;

    default:
        sdl_minimap_close();
        break;
    }

    return true;
}

bool sdl_minimap_handle_gamepad_axis(const SDL_GamepadAxisEvent* ev)
{
    int threshold;
    bool pressed;
    int deadzone;
    int dir;

    if (!g_minimap.active || !ev)
        return false;

    if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX
        || ev->axis == SDL_GAMEPAD_AXIS_LEFTY)
    {
        if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX)
            g_gamepad_state.left_x = ev->value;
        else
            g_gamepad_state.left_y = ev->value;

        deadzone = config.gamepad_deadzone;
        if (deadzone < 0)
            deadzone = 0;

        dir = sdl_gamepad_axis_to_dir(g_gamepad_state.left_x,
            g_gamepad_state.left_y, deadzone);
        if (dir != g_minimap.left_stick_dir) {
            g_minimap.left_stick_dir = dir;
            if (dir)
                (void)sdl_minimap_pan(ddx[dir], ddy[dir]);
        }
        return true;
    }

    if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX
        || ev->axis == SDL_GAMEPAD_AXIS_RIGHTY)
    {
        if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX)
            g_gamepad_state.right_x = ev->value;
        else
            g_gamepad_state.right_y = ev->value;

        deadzone = config.gamepad_deadzone;
        if (deadzone < 0)
            deadzone = 0;

        dir = sdl_gamepad_axis_to_dir(g_gamepad_state.right_x,
            g_gamepad_state.right_y, deadzone);
        if (dir != g_minimap.right_stick_dir) {
            g_minimap.right_stick_dir = dir;
            if (dir)
                (void)sdl_minimap_pan(ddx[dir], ddy[dir]);
        }
        return true;
    }

    if (ev->axis != SDL_GAMEPAD_AXIS_LEFT_TRIGGER
        && ev->axis != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
    {
        return false;
    }

    threshold = config.gamepad_trigger_threshold;
    if (threshold < 0)
        threshold = 0;
    pressed = (ev->value >= threshold);

    if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
        if (pressed != g_gamepad_state.left_trigger_down) {
            g_gamepad_state.left_trigger_down = pressed;
            if (pressed)
                (void)sdl_minimap_adjust_zoom(-1);
        }
    } else {
        if (pressed != g_gamepad_state.right_trigger_down) {
            g_gamepad_state.right_trigger_down = pressed;
            if (pressed)
                (void)sdl_minimap_adjust_zoom(1);
        }
    }

    return true;
}

void sdl_minimap_layout_controls(const sdl_view* d, int canvas_w,
    int canvas_h)
{
    float margin;
    float gap;
    float size;
    float available_w;

    g_minimap.zoom_out_rect = (SDL_FRect){0};
    g_minimap.zoom_in_rect = (SDL_FRect){0};
    g_minimap.close_rect = (SDL_FRect){0};
    g_minimap.zoom_out_enabled = g_minimap.zoom_step > 0;
    g_minimap.zoom_in_enabled = g_minimap.zoom_step < MINIMAP_MAX_ZOOM_STEP;

    if (!g_minimap.active || !d || canvas_w <= 0 || canvas_h <= 0)
        return;

    margin = (float)d->cell_h * 0.5f;
    if (margin < 8.0f)
        margin = 8.0f;
    gap = margin * 0.75f;
    if (gap < 6.0f)
        gap = 6.0f;

    size = (float)d->cell_h * 2.1f;
    size = sdl_minimap_clampf(size, 36.0f, 56.0f);
    if (canvas_h > 0)
        size = sdl_minimap_clampf(size, 24.0f, (float)canvas_h - margin * 2.0f);

    available_w = (float)canvas_w - margin * 2.0f - gap * 2.0f;
    if (available_w < size * 3.0f)
        size = available_w / 3.0f;
    if (size < 22.0f)
        return;

    g_minimap.close_rect.w = size;
    g_minimap.close_rect.h = size;
    g_minimap.close_rect.x = (float)canvas_w - margin - size;
    g_minimap.close_rect.y = margin;

    g_minimap.zoom_in_rect.w = size;
    g_minimap.zoom_in_rect.h = size;
    g_minimap.zoom_in_rect.x = g_minimap.close_rect.x - gap - size;
    g_minimap.zoom_in_rect.y = margin;

    g_minimap.zoom_out_rect.w = size;
    g_minimap.zoom_out_rect.h = size;
    g_minimap.zoom_out_rect.x = g_minimap.zoom_in_rect.x - gap - size;
    g_minimap.zoom_out_rect.y = margin;
}

void sdl_minimap_draw_button_symbol(const SDL_FRect* rect, int symbol,
    SDL_Color color)
{
    float stroke;
    float pad;
    SDL_FRect h;
    SDL_FRect v;

    if (!rect)
        return;

    stroke = rect->w / 10.0f;
    stroke = sdl_minimap_clampf(stroke, 2.0f, 5.0f);
    pad = rect->w * 0.30f;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        color.a);

    if (symbol == 2) {
        float x1 = rect->x + pad;
        float x2 = rect->x + rect->w - pad;
        float y1 = rect->y + pad;
        float y2 = rect->y + rect->h - pad;
        int repeats = (int)stroke;

        if (repeats < 1)
            repeats = 1;
        for (int i = 0; i < repeats; i++) {
            float offset = (float)i - ((float)repeats - 1.0f) * 0.5f;
            SDL_RenderLine(g_state.renderer, x1, y1 + offset, x2,
                y2 + offset);
            SDL_RenderLine(g_state.renderer, x1, y2 + offset, x2,
                y1 + offset);
        }
        return;
    }

    h.x = rect->x + pad;
    h.y = rect->y + rect->h * 0.5f - stroke * 0.5f;
    h.w = rect->w - pad * 2.0f;
    h.h = stroke;
    SDL_RenderFillRect(g_state.renderer, &h);

    if (symbol == 1) {
        v.x = rect->x + rect->w * 0.5f - stroke * 0.5f;
        v.y = rect->y + pad;
        v.w = stroke;
        v.h = rect->h - pad * 2.0f;
        SDL_RenderFillRect(g_state.renderer, &v);
    }
}

void sdl_minimap_draw_button(const SDL_FRect* rect, bool enabled,
    int symbol)
{
    SDL_Color icon = enabled
        ? (SDL_Color){235, 238, 242, 255}
        : (SDL_Color){100, 106, 114, 255};
    SDL_Color outline = enabled
        ? (SDL_Color){200, 205, 215, 255}
        : (SDL_Color){75, 80, 88, 255};

    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer,
        enabled ? 24 : 18, enabled ? 28 : 18, enabled ? 34 : 18, 245);
    SDL_RenderFillRect(g_state.renderer, rect);
    SDL_SetRenderDrawColor(g_state.renderer, outline.r, outline.g,
        outline.b, outline.a);
    SDL_RenderRect(g_state.renderer, rect);
    sdl_minimap_draw_button_symbol(rect, symbol, icon);
}

void sdl_minimap_draw_controls(sdl_view* d, int canvas_w, int canvas_h)
{
    if (!g_minimap.active || !d)
        return;

    sdl_minimap_layout_controls(d, canvas_w, canvas_h);
    sdl_minimap_draw_button(&g_minimap.zoom_out_rect,
        g_minimap.zoom_out_enabled, 0);
    sdl_minimap_draw_button(&g_minimap.zoom_in_rect,
        g_minimap.zoom_in_enabled, 1);
    sdl_minimap_draw_button(&g_minimap.close_rect, true, 2);
}

void sdl_minimap_draw_prompt(sdl_view* d, int canvas_w, int canvas_h)
{
    int row;
    int len;
    int col;
    const char* prompt;
    SDL_FRect bar;
    SDL_Color text = {190, 196, 206, 255};

    if (!g_minimap.active || !d || d->rows <= 0 || d->cols <= 0)
        return;

    row = d->rows - 1;
    if (row < 0)
        return;

    bar.x = 0.0f;
    bar.y = (float)canvas_h;
    bar.w = (float)canvas_w;
    bar.h = (float)d->cell_h;
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(g_state.renderer, &bar);

    if (steamdeck_controls_active())
        prompt = "Minimap  d-pad pan  L1/R1 zoom  A/B closes";
    else if (d->cols >= 66)
        prompt = "Minimap  drag/dir pan  +/- wheel pinch zoom  Esc closes";
    else
        prompt = "Minimap  drag/dir pan  +/- zoom  Esc closes";

    len = (int)strlen(prompt);
    if (len > d->cols)
        len = d->cols;
    col = (d->cols - len) / 2;
    if (col < 0)
        col = 0;

    sdl_render_mono_text(d, col, row, len, prompt, text);
}

bool sdl_menu_scroll_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id)
{
    int col = 0;
    int row = 0;

    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (!ui_scroll_area_has_cell(col, row))
        return false;
    if (!config.touch_menu_command_enabled[ui_scroll_area_get_touch_category()]
        && !ui_scroll_area_get_tap_key())
    {
        return true;
    }

    sdl_menu_scroll_cancel();
    g_menu_scroll_drag.active = true;
    g_menu_scroll_drag.dragged = false;
    g_menu_scroll_drag.page_fired = false;
    g_menu_scroll_drag.finger_id = finger_id;
    g_menu_scroll_drag.area_index = ui_scroll_area_selected_index();
    g_menu_scroll_drag.start_x = x;
    g_menu_scroll_drag.start_y = y;
    g_menu_scroll_drag.last_y = y;
    g_menu_scroll_drag.accum_y = 0.0f;
    return true;
}

bool sdl_menu_scroll_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id)
{
    int cell_h = g_views[PANE_MAIN].cell_h;
    float dx;
    float dy;
    float total_dy;
    bool sent_key = false;

    if (!g_menu_scroll_drag.active
        || g_menu_scroll_drag.finger_id != finger_id)
    {
        return false;
    }
    if (!ui_scroll_area_select_index(g_menu_scroll_drag.area_index))
    {
        sdl_menu_scroll_cancel();
        return true;
    }

    if (cell_h <= 0)
        cell_h = 1;

    dx = x - g_menu_scroll_drag.start_x;
    if (dx < 0.0f)
        dx = -dx;
    dy = y - g_menu_scroll_drag.last_y;
    g_menu_scroll_drag.last_y = y;
    g_menu_scroll_drag.accum_y += dy;
    total_dy = y - g_menu_scroll_drag.start_y;
    if (total_dy < 0.0f)
        total_dy = -total_dy;

    if (dx > sdl_touch_swipe_threshold_px()
        || total_dy > sdl_touch_swipe_threshold_px())
    {
        g_menu_scroll_drag.dragged = true;
    }

    if (ui_scroll_area_is_page_mode())
    {
        float threshold = sdl_touch_swipe_threshold_px();
        float sdx = x - g_menu_scroll_drag.start_x;
        float sdy = y - g_menu_scroll_drag.start_y;
        int page_key;

        /*
         * Fire exactly one page turn per gesture: once page_fired is latched
         * nothing more is sent until the finger lifts and a fresh drag starts.
         * dragged is also set so pointer-up does not additionally fire the tap
         * key, keeping a swipe and a tap mutually exclusive.
         */
        if (g_menu_scroll_drag.page_fired)
            return true;
        if (dx < threshold && total_dy < threshold)
            return true;

        if (dx >= total_dy)
            page_key = (sdx < 0.0f)
                ? ui_scroll_area_get_horizontal_key(-1)
                : ui_scroll_area_get_horizontal_key(1);
        else
            page_key = (sdy < 0.0f)
                ? ui_scroll_area_get_vertical_key(-1)
                : ui_scroll_area_get_vertical_key(1);

        g_menu_scroll_drag.dragged = true;
        g_menu_scroll_drag.page_fired = true;
        if (page_key)
            Term_keypress(page_key);
        return true;
    }

    if (ui_scroll_area_has_offset_target())
    {
        /*
         * Touch-only list menus register a viewport offset: drag the list
         * itself without moving (or committing) the selection.  A positive
         * vertical key normally moves the cursor up one row, which reveals
         * earlier entries, i.e. a one-row decrease of the viewport offset.
         */
        while (g_menu_scroll_drag.accum_y >= (float)cell_h)
        {
            (void)ui_scroll_area_offset_scroll(-1);
            g_menu_scroll_drag.accum_y -= (float)cell_h;
            sent_key = true;
        }
        while (g_menu_scroll_drag.accum_y <= -(float)cell_h)
        {
            (void)ui_scroll_area_offset_scroll(1);
            g_menu_scroll_drag.accum_y += (float)cell_h;
            sent_key = true;
        }

        if (sent_key)
        {
            g_menu_scroll_drag.dragged = true;
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }

        return true;
    }

    if (sdl_touch_only_device_active() && ui_scroll_area_get_tap_key() == 0)
        return true;

    while (g_menu_scroll_drag.accum_y >= (float)cell_h)
    {
        Term_keypress(ui_scroll_area_get_vertical_key(1));
        g_menu_scroll_drag.accum_y -= (float)cell_h;
        sent_key = true;
    }
    while (g_menu_scroll_drag.accum_y <= -(float)cell_h)
    {
        Term_keypress(ui_scroll_area_get_vertical_key(-1));
        g_menu_scroll_drag.accum_y += (float)cell_h;
        sent_key = true;
    }

    if (sent_key)
        g_menu_scroll_drag.dragged = true;

    return true;
}

bool sdl_menu_scroll_handle_pointer_up(SDL_FingerID finger_id)
{
    if (g_menu_scroll_drag.active
        && g_menu_scroll_drag.finger_id == finger_id)
    {
        int tap_key = ui_scroll_area_select_index(g_menu_scroll_drag.area_index)
            ? ui_scroll_area_get_tap_key() : 0;
        bool send_tap = !g_menu_scroll_drag.dragged && tap_key != 0;

        sdl_menu_scroll_cancel();
        if (send_tap)
            Term_keypress(tap_key);
        return true;
    }

    return false;
}

bool sdl_main_screen_cell_hits_character_panel(int col, int row)
{
    if (col < 0 || row < 0)
        return false;

    if (g_last_main_cell_hit_left_panel)
        return true;

    if (!get_sdl_hide_left_panel()) {
        return (row > 0 && col < COL_MAP);
    }

    if (g_suppress_hidden_left_panel_overlay)
        return false;

    row -= g_hidden_left_panel_overlay_start_row;
    if (row < 0 || row >= g_hidden_left_panel_overlay_rows || row >= 16)
        return false;

    return (col >= g_hidden_left_panel_overlay_start_cols[row]
        && col < g_hidden_left_panel_overlay_start_cols[row]
            + g_hidden_left_panel_overlay_widths[row]);
}

int sdl_hidden_left_panel_attack_mode_at_cell(int col, int row)
{
    if (col < 0 || row < 0)
        return SDL_POINTER_ATTACK_NONE;
    if (!get_sdl_hide_left_panel())
        return SDL_POINTER_ATTACK_NONE;

    row -= g_hidden_left_panel_overlay_start_row;
    if (row < 0 || row >= g_hidden_left_panel_overlay_rows || row >= 16)
        return SDL_POINTER_ATTACK_NONE;
    if (g_hidden_left_panel_overlay_attack_modes[row]
            == SDL_POINTER_ATTACK_NONE)
    {
        return SDL_POINTER_ATTACK_NONE;
    }
    if (col < g_hidden_left_panel_overlay_attack_start_cols[row]
        || col >= g_hidden_left_panel_overlay_attack_end_cols[row])
    {
        return SDL_POINTER_ATTACK_NONE;
    }

    return g_hidden_left_panel_overlay_attack_modes[row];
}

int sdl_left_panel_quiver_attack_mode_at_col(int col)
{
    bool has_any_span = false;

    if (col < 0)
        return SDL_POINTER_ATTACK_NONE;

    for (int i = 0; i < 2; i++) {
        if (g_left_panel_quiver_attack_modes[i] == SDL_POINTER_ATTACK_NONE)
            continue;

        has_any_span = true;
        if (col >= g_left_panel_quiver_attack_start_cols[i]
            && col < g_left_panel_quiver_attack_end_cols[i])
        {
            return g_left_panel_quiver_attack_modes[i];
        }
    }

    if (!has_any_span) {
        if (inventory[INVEN_QUIVER1].k_idx && !inventory[INVEN_QUIVER2].k_idx)
            return SDL_POINTER_ATTACK_RANGED_1;
        if (!inventory[INVEN_QUIVER1].k_idx && inventory[INVEN_QUIVER2].k_idx)
            return SDL_POINTER_ATTACK_RANGED_2;
    }

    return SDL_POINTER_ATTACK_NONE;
}

static bool sdl_visible_character_panel_attack_row(int row)
{
    bool melee_uses_two_rows;

    if (row < 0)
        return false;

    melee_uses_two_rows = (ROW_MEL - 1) != ROW_LIGHT
        && inventory[INVEN_ARM].k_idx
        && inventory[INVEN_ARM].tval != TV_SHIELD;

    if (row == ROW_MEL || (melee_uses_two_rows && row == ROW_MEL - 1))
        return true;
    if (row == ROW_ARC)
        return true;
    if (row == ROW_QUIVER)
        return true;

    return false;
}

int sdl_visible_character_panel_attack_mode_at_cell(int col, int row)
{
    bool melee_uses_two_rows;

    if (col < 0 || row < 0 || get_sdl_hide_left_panel())
        return SDL_POINTER_ATTACK_NONE;
    if (col >= LEFT_PANEL_CONTENT_WID)
        return SDL_POINTER_ATTACK_NONE;

    melee_uses_two_rows = (ROW_MEL - 1) != ROW_LIGHT
        && inventory[INVEN_ARM].k_idx
        && inventory[INVEN_ARM].tval != TV_SHIELD;

    if (row == ROW_MEL || (melee_uses_two_rows && row == ROW_MEL - 1))
        return SDL_POINTER_ATTACK_MELEE;
    if (row == ROW_ARC)
        return SDL_POINTER_ATTACK_RANGED_1;
    if (row == ROW_QUIVER)
        return sdl_left_panel_quiver_attack_mode_at_col(col);

    return SDL_POINTER_ATTACK_NONE;
}

static int sdl_combat_overlay_attack_mode_at_cell(int col, int row)
{
    bool melee_uses_two_rows;

    if (col < 0 || row < 0 || col >= PANE_COMBAT_OVERLAY_COLS)
        return SDL_POINTER_ATTACK_NONE;

    melee_uses_two_rows = sdl_combat_overlay_melee_uses_offhand_row();
    if (row == ROW_MEL || (melee_uses_two_rows && row == ROW_MEL - 1))
        return SDL_POINTER_ATTACK_MELEE;
    if (row == ROW_ARC)
        return SDL_POINTER_ATTACK_RANGED_1;
    if (row == ROW_QUIVER)
        return sdl_left_panel_quiver_attack_mode_at_col(col);

    return SDL_POINTER_ATTACK_NONE;
}

void sdl_enqueue_bypassed_command(int command)
{
    if (character_icky > 0)
        Term_keypress(ESCAPE);
    Term_keypress('\\');
    Term_keypress(command);
}

int sdl_hidden_left_panel_click_action_at_cell(int col, int row)
{
    if (col < 0 || row < 0)
        return SDL_PANEL_CLICK_NONE;
    if (!get_sdl_hide_left_panel())
        return SDL_PANEL_CLICK_NONE;

    row -= g_hidden_left_panel_overlay_start_row;
    if (row < 0 || row >= g_hidden_left_panel_overlay_rows || row >= 16)
        return SDL_PANEL_CLICK_NONE;
    if (g_hidden_left_panel_overlay_click_actions[row] == SDL_PANEL_CLICK_NONE)
        return SDL_PANEL_CLICK_NONE;
    if (col < g_hidden_left_panel_overlay_click_start_cols[row]
        || col >= g_hidden_left_panel_overlay_click_end_cols[row])
    {
        return SDL_PANEL_CLICK_NONE;
    }

    return g_hidden_left_panel_overlay_click_actions[row];
}

int sdl_visible_character_panel_click_action_at_cell(int col, int row)
{
    if (col < 0 || row < 0 || get_sdl_hide_left_panel())
        return SDL_PANEL_CLICK_NONE;
    if (col >= LEFT_PANEL_CONTENT_WID)
        return SDL_PANEL_CLICK_NONE;

    /*
     * The visible character panel is sliced into four big stacked "button"
     * blocks that tile every row (blank rows included).  Hovering or clicking
     * anywhere inside a block targets the whole block.
     *
     *   top header (name + graphical health bar, and any rows above) -> compact
     *   stats (Str/Dex/Con/Gra and the gap below)                    -> abilities
     *   vitals (exp, health, voice, light)                           -> inventory
     *   melee/archery/quiver                                         -> attack only
     *   armour and everything below                                  -> equipment
     */
    if (row < ROW_STAT)
        return SDL_PANEL_CLICK_COMPACT;

    if (row < ROW_EXP)
        return SDL_PANEL_CLICK_ABILITIES;

    if (sdl_visible_character_panel_attack_row(row))
        return SDL_PANEL_CLICK_NONE;

    if (row < ROW_EVN)
        return SDL_PANEL_CLICK_INVENTORY;

    return SDL_PANEL_CLICK_EQUIPMENT;
}

/*
 * Topmost source row of the visible-panel block that owns the given click
 * action.  Used to anchor the hover/long-press popup so it stays fixed for the
 * whole block instead of jumping from row to row.
 */
int sdl_visible_character_panel_block_top_row(int click_action)
{
    switch (click_action)
    {
    case SDL_PANEL_CLICK_COMPACT:
        return ROW_NAME;
    case SDL_PANEL_CLICK_ABILITIES:
        return ROW_STAT;
    case SDL_PANEL_CLICK_INVENTORY:
        return ROW_EXP;
    case SDL_PANEL_CLICK_EQUIPMENT:
        return ROW_EVN;
    default:
        return -1;
    }
}

cptr sdl_character_panel_click_tooltip_text(int click_action)
{
    switch (click_action)
    {
    case SDL_PANEL_CLICK_CHARACTER:
        return "Click: open character details.";
    case SDL_PANEL_CLICK_SONG:
        return "Click: open the song menu.";
    case SDL_PANEL_CLICK_SUPPLIES_LIGHTS:
        return "Click: open supplies for lights.";
    case SDL_PANEL_CLICK_SKILL_DISTRIBUTION:
        return "Click: spend experience.";
    case SDL_PANEL_CLICK_INVENTORY:
        return "Click: open inventory.";
    case SDL_PANEL_CLICK_ABILITIES:
        return "Click: open abilities.";
    case SDL_PANEL_CLICK_SMITHING:
        return "Click: open smithing.";
    case SDL_PANEL_CLICK_EQUIPMENT:
        return "Click: open equipment.";
    case SDL_PANEL_CLICK_COMPACT:
        return "Click: collapse to compact panel.";
    default:
        return NULL;
    }
}

cptr sdl_character_panel_attack_tooltip_text(int attack_mode)
{
    switch (attack_mode)
    {
    case SDL_POINTER_ATTACK_MELEE:
        return "Click: select melee attack mode.";
    case SDL_POINTER_ATTACK_RANGED_1:
        return "Click: select ranged attack mode.";
    case SDL_POINTER_ATTACK_RANGED_2:
        return "Click: select second-quiver ranged attack mode.";
    default:
        return NULL;
    }
}

void sdl_character_panel_tooltip_span(int col, int row,
    int attack_mode, int click_action, int* out_col, int* out_cols)
{
    int anchor_col = 0;
    int anchor_cols = LEFT_PANEL_CONTENT_WID;

    if (anchor_cols <= 0)
        anchor_cols = COL_MAP;

    if (get_sdl_hide_left_panel())
    {
        int idx = row - g_hidden_left_panel_overlay_start_row;
        int start_col = -1;
        int end_col = -1;

        if (idx >= 0 && idx < g_hidden_left_panel_overlay_rows && idx < 16)
        {
            if (attack_mode != SDL_POINTER_ATTACK_NONE)
            {
                start_col = g_hidden_left_panel_overlay_attack_start_cols[idx];
                end_col = g_hidden_left_panel_overlay_attack_end_cols[idx];
            }
            else if (click_action != SDL_PANEL_CLICK_NONE)
            {
                start_col = g_hidden_left_panel_overlay_click_start_cols[idx];
                end_col = g_hidden_left_panel_overlay_click_end_cols[idx];
            }

            if (end_col <= start_col)
            {
                start_col = g_hidden_left_panel_overlay_start_cols[idx];
                end_col = start_col + g_hidden_left_panel_overlay_widths[idx];
            }
        }

        if (end_col > start_col)
        {
            anchor_col = start_col;
            anchor_cols = end_col - start_col;
        }
    }
    else if (row == ROW_QUIVER && attack_mode != SDL_POINTER_ATTACK_NONE)
    {
        for (int i = 0; i < 2; i++)
        {
            if (g_left_panel_quiver_attack_modes[i] != attack_mode)
                continue;
            if (col < g_left_panel_quiver_attack_start_cols[i]
                || col >= g_left_panel_quiver_attack_end_cols[i])
            {
                continue;
            }
            if (g_left_panel_quiver_attack_end_cols[i]
                > g_left_panel_quiver_attack_start_cols[i])
            {
                anchor_col = g_left_panel_quiver_attack_start_cols[i];
                anchor_cols = g_left_panel_quiver_attack_end_cols[i]
                    - g_left_panel_quiver_attack_start_cols[i];
            }
            break;
        }
    }

    if (anchor_cols < 1)
        anchor_cols = 1;
    if (out_col)
        *out_col = anchor_col;
    if (out_cols)
        *out_cols = anchor_cols;
}

void sdl_character_panel_show_hover_tooltip(int col, int row,
    int attack_mode, int click_action, bool touch)
{
    cptr text = NULL;
    int anchor_col = 0;
    int anchor_cols = 1;
    int anchor_row = row;

    if (attack_mode != SDL_POINTER_ATTACK_NONE)
        text = sdl_character_panel_attack_tooltip_text(attack_mode);
    if (!text && click_action != SDL_PANEL_CLICK_NONE)
        text = sdl_character_panel_click_tooltip_text(click_action);
    if (!text)
        return;

    sdl_character_panel_tooltip_span(col, row, attack_mode, click_action,
        &anchor_col, &anchor_cols);

    /* Keep the popup pinned to the top of the block so it stays put while the
     * pointer roams within the block (visible panel, click-action blocks). */
    if (attack_mode == SDL_POINTER_ATTACK_NONE
        && click_action != SDL_PANEL_CLICK_NONE
        && !get_sdl_hide_left_panel())
    {
        int top = sdl_visible_character_panel_block_top_row(click_action);

        if (top >= 0)
            anchor_row = top;
    }

    (void)sdl_object_tooltip_show_character_panel_text_at_cell(anchor_col,
        anchor_row, anchor_cols, text, touch);
}

static void sdl_combat_overlay_show_hover_tooltip(int col, int row,
    int attack_mode, bool touch)
{
    cptr text = sdl_character_panel_attack_tooltip_text(attack_mode);
    int anchor_col = 0;
    int anchor_cols = PANE_COMBAT_OVERLAY_COLS;
    SDL_FRect rect;

    if (!text)
        return;

    sdl_character_panel_tooltip_span(col, row, attack_mode,
        SDL_PANEL_CLICK_NONE, &anchor_col, &anchor_cols);
    if (anchor_cols < 1)
        anchor_cols = 1;
    if (!sdl_combat_overlay_cell_rect(anchor_col, row, anchor_cols, 1,
            &rect))
    {
        return;
    }

    (void)sdl_object_tooltip_show_text_at_rect(&rect, text, touch);
}

bool sdl_handle_character_panel_click_action(int click_action)
{
    switch (click_action)
    {
    case SDL_PANEL_CLICK_CHARACTER:
        sdl_enqueue_bypassed_command('h');
        return true;
    case SDL_PANEL_CLICK_SONG:
        sdl_enqueue_bypassed_command('s');
        return true;
    case SDL_PANEL_CLICK_SUPPLIES_LIGHTS:
        supplies_set_pending_action(SUPPLY_MENU_ACTION_USE,
            SUPPLY_GROUP_LIGHTS, true);
        sdl_enqueue_bypassed_command('j');
        return true;
    case SDL_PANEL_CLICK_SKILL_DISTRIBUTION:
        sdl_enqueue_bypassed_command('H');
        return true;
    case SDL_PANEL_CLICK_INVENTORY:
        sdl_enqueue_bypassed_command(
            sdl_inventory_equipment_cycle_binding('i'));
        return true;
    case SDL_PANEL_CLICK_ABILITIES:
        sdl_enqueue_bypassed_command('y');
        return true;
    case SDL_PANEL_CLICK_SMITHING:
        sdl_enqueue_bypassed_command('0');
        return true;
    case SDL_PANEL_CLICK_EQUIPMENT:
        sdl_enqueue_bypassed_command(
            sdl_inventory_equipment_cycle_binding('e'));
        return true;
    case SDL_PANEL_CLICK_COMPACT:
        sdl_left_panel_pane_set_expanded(false);
        return true;
    default:
        return false;
    }
}

bool sdl_main_screen_handle_character_panel_hover_pointer(float x, float y)
{
    SDL_Rect combat_rect;
    int col = 0;
    int row = 0;
    bool active = sdl_main_screen_click_shortcuts_active();
    bool combat_hit = false;
    int attack_mode = SDL_POINTER_ATTACK_NONE;
    int click_action = SDL_PANEL_CLICK_NONE;

    if (active
        && !sdl_touch_zone_controls_active()
        && sdl_combat_overlay_point_to_cell(x, y, &col, &row))
    {
        attack_mode = sdl_combat_overlay_attack_mode_at_cell(col, row);
        combat_hit = true;
    }
    else if (active
        && !sdl_touch_zone_controls_active()
        && sdl_combat_overlay_pane_current_rect(&combat_rect)
        && sdl_point_in_rect(&combat_rect, x, y))
    {
        sdl_pointer_attack_set_panel_hover_mode(SDL_POINTER_ATTACK_NONE);
        sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
            SDL_PANEL_CLICK_NONE, -1, active);
        return true;
    }
    else if (active
        && !sdl_touch_zone_controls_active()
        && sdl_main_view_point_to_cell(x, y, &col, &row)
        && sdl_main_screen_cell_hits_character_panel(col, row))
    {
        if (get_sdl_hide_left_panel())
        {
            attack_mode = sdl_hidden_left_panel_attack_mode_at_cell(col, row);
            click_action = sdl_hidden_left_panel_click_action_at_cell(col, row);
        }
        else
        {
            attack_mode = sdl_visible_character_panel_attack_mode_at_cell(col,
                row);
            click_action = sdl_visible_character_panel_click_action_at_cell(col,
                row);
        }
    }

    if (attack_mode != SDL_POINTER_ATTACK_NONE)
    {
        sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
            SDL_PANEL_CLICK_NONE, -1, active);
        sdl_pointer_attack_set_panel_hover_mode(attack_mode);
        if (combat_hit)
            sdl_combat_overlay_show_hover_tooltip(col, row, attack_mode,
                false);
        else
            sdl_character_panel_show_hover_tooltip(col, row, attack_mode,
                SDL_PANEL_CLICK_NONE, false);
        return true;
    }

    sdl_pointer_attack_set_panel_hover_mode(SDL_POINTER_ATTACK_NONE);
    sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
        click_action, click_action != SDL_PANEL_CLICK_NONE ? row : -1, active);
    if (click_action != SDL_PANEL_CLICK_NONE)
        sdl_character_panel_show_hover_tooltip(col, row,
            SDL_POINTER_ATTACK_NONE, click_action, false);

    return click_action != SDL_PANEL_CLICK_NONE;
}

bool sdl_binding_opens_pane_menu(int binding)
{
    switch (binding) {
    case 'i':
    case 'e':
    case 'j':
    case 'h':
    case '@':
    case 'y':
    case 'M':
    case 'm':
    case '~':
    case '[':
    case ']':
    case '?':
    case 'O':
    case KTRL('P'):
    case KTRL('Q'):
        return true;
    default:
        return false;
    }
}

bool sdl_main_screen_handle_character_panel_pointer(float x, float y)
{
    SDL_Rect combat_rect;
    int col = 0;
    int row = 0;
    int attack_mode = SDL_POINTER_ATTACK_NONE;
    int click_action = SDL_PANEL_CLICK_NONE;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (sdl_touch_zone_controls_active())
        return false;
    if (sdl_combat_overlay_point_to_cell(x, y, &col, &row)) {
        attack_mode = sdl_combat_overlay_attack_mode_at_cell(col, row);
        if (attack_mode != SDL_POINTER_ATTACK_NONE
            && sdl_pointer_attack_input_context_active())
        {
            sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
                SDL_PANEL_CLICK_NONE, -1, false);
            sdl_pointer_attack_set_mode(attack_mode);
        }
        return true;
    }
    if (sdl_combat_overlay_pane_current_rect(&combat_rect)
        && sdl_point_in_rect(&combat_rect, x, y))
    {
        return true;
    }
    if (sdl_left_panel_pane_collapsed()
        && sdl_left_panel_pane_hit(x, y))
    {
        sdl_left_panel_pane_set_expanded(true);
        sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
            SDL_PANEL_CLICK_NONE, -1, false);
        return true;
    }
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;
    if (!sdl_main_screen_cell_hits_character_panel(col, row))
        return false;

    if (get_sdl_hide_left_panel()) {
        attack_mode = sdl_hidden_left_panel_attack_mode_at_cell(col, row);
        click_action = sdl_hidden_left_panel_click_action_at_cell(col, row);
    } else {
        attack_mode = sdl_visible_character_panel_attack_mode_at_cell(col, row);
        click_action = sdl_visible_character_panel_click_action_at_cell(col, row);
    }

    if (attack_mode != SDL_POINTER_ATTACK_NONE
        && sdl_pointer_attack_input_context_active())
    {
        sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
            SDL_PANEL_CLICK_NONE, -1, false);
        sdl_pointer_attack_set_mode(attack_mode);
        return true;
    }

    if (sdl_handle_character_panel_click_action(click_action))
    {
        sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
            SDL_PANEL_CLICK_NONE, -1, false);
        return true;
    }

    return true;
}

/*
 * Show the block popup prompt for a character-panel point, the same text shown
 * on hover.  Used by right-click and touch long-press so they surface the
 * action of the block they land on instead of triggering it.
 */
bool sdl_main_screen_show_character_panel_popup(float x, float y, bool touch)
{
    int col = 0;
    int row = 0;
    int attack_mode = SDL_POINTER_ATTACK_NONE;
    int click_action = SDL_PANEL_CLICK_NONE;
    bool shown = false;

    /* The popup mirrors a right-click: it should stay up until the next press
     * instead of fading like a transient touch hover tooltip, so show it inside
     * a persistent scope (no-op for the mouse, which never auto-expires). */
    sdl_object_tooltip_begin_persistent();

    if (sdl_combat_overlay_point_to_cell(x, y, &col, &row)) {
        attack_mode = sdl_combat_overlay_attack_mode_at_cell(col, row);
        if (attack_mode != SDL_POINTER_ATTACK_NONE) {
            sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
                SDL_PANEL_CLICK_NONE, -1, true);
            sdl_combat_overlay_show_hover_tooltip(col, row, attack_mode, touch);
            shown = true;
        }
        sdl_object_tooltip_end_persistent();
        return shown;
    }

    if (sdl_main_view_point_to_cell(x, y, &col, &row)
        && sdl_main_screen_cell_hits_character_panel(col, row))
    {
        if (get_sdl_hide_left_panel()) {
            attack_mode = sdl_hidden_left_panel_attack_mode_at_cell(col, row);
            click_action = sdl_hidden_left_panel_click_action_at_cell(col, row);
        } else {
            attack_mode = sdl_visible_character_panel_attack_mode_at_cell(col,
                row);
            click_action = sdl_visible_character_panel_click_action_at_cell(col,
                row);
        }

        if (attack_mode != SDL_POINTER_ATTACK_NONE
            || click_action != SDL_PANEL_CLICK_NONE)
        {
            sdl_main_screen_touch_zone_selection_set(SDL_STATUS_CLICK_NONE, -1,
                click_action, click_action != SDL_PANEL_CLICK_NONE ? row : -1,
                true);
            sdl_character_panel_show_hover_tooltip(col, row, attack_mode,
                click_action, touch);
            shown = true;
        }
    }

    sdl_object_tooltip_end_persistent();
    return shown;
}

bool sdl_main_screen_handle_character_panel_secondary_pointer(float x, float y)
{
    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (sdl_touch_zone_controls_active())
        return false;
    if (!sdl_main_screen_character_panel_pointer_hit(x, y))
        return false;

    (void)sdl_main_screen_show_character_panel_popup(x, y, true);
    return true;
}

bool sdl_main_screen_character_panel_pointer_hit(float x, float y)
{
    SDL_Rect combat_rect;
    int col = 0;
    int row = 0;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (sdl_touch_zone_controls_active())
        return false;
    if (sdl_left_panel_pane_hit(x, y))
        return true;
    if (sdl_combat_overlay_point_to_cell(x, y, &col, &row))
        return true;
    if (sdl_combat_overlay_pane_current_rect(&combat_rect)
        && sdl_point_in_rect(&combat_rect, x, y))
    {
        return true;
    }
    if (!sdl_main_view_point_to_cell(x, y, &col, &row))
        return false;

    return sdl_main_screen_cell_hits_character_panel(col, row);
}

void sdl_character_panel_cancel_press(void)
{
    g_character_panel_press.active = false;
    g_character_panel_press.mouse = false;
    g_character_panel_press.finger_id = 0;
    g_character_panel_press.secondary_enabled = false;
    g_character_panel_press.start_x = 0.0f;
    g_character_panel_press.start_y = 0.0f;
    g_character_panel_press.start_time = 0;
}

bool sdl_character_panel_press_matches(bool mouse,
    SDL_FingerID finger_id)
{
    return g_character_panel_press.active
        && g_character_panel_press.mouse == mouse
        && g_character_panel_press.finger_id == finger_id;
}

bool sdl_character_panel_handle_pointer_down(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    int col = 0;
    int row = 0;
    bool combat_hit;

    if (!sdl_main_screen_character_panel_pointer_hit(x, y))
        return false;

    combat_hit = sdl_combat_overlay_point_to_cell(x, y, &col, &row);
    sdl_character_panel_cancel_press();
    g_character_panel_press.active = true;
    g_character_panel_press.mouse = mouse;
    g_character_panel_press.finger_id = finger_id;
    /* Long-press surfaces the block popup prompt anywhere on the expanded
     * panel, mirroring a right-click. */
    g_character_panel_press.secondary_enabled =
        combat_hit || !sdl_left_panel_pane_collapsed();
    g_character_panel_press.start_x = x;
    g_character_panel_press.start_y = y;
    g_character_panel_press.start_time = SDL_GetTicksNS();
    return true;
}

bool sdl_character_panel_handle_pointer_motion(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float threshold;

    if (!sdl_character_panel_press_matches(mouse, finger_id))
        return false;

    dx = x - g_character_panel_press.start_x;
    dy = y - g_character_panel_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_touch_swipe_threshold_px();
    if (dx > threshold || dy > threshold
        || !sdl_main_screen_character_panel_pointer_hit(x, y))
    {
        sdl_character_panel_cancel_press();
        return true;
    }

    return true;
}

bool sdl_character_panel_handle_pointer_up(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    Uint64 elapsed = 0;
    bool long_press = false;
    bool secondary_enabled;
    float start_x;
    float start_y;

    if (!sdl_character_panel_press_matches(mouse, finger_id))
        return false;

    if (g_character_panel_press.start_time)
        elapsed = SDL_GetTicksNS() - g_character_panel_press.start_time;
    long_press = !mouse
        && elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL;
    secondary_enabled = g_character_panel_press.secondary_enabled;
    start_x = g_character_panel_press.start_x;
    start_y = g_character_panel_press.start_y;

    sdl_character_panel_cancel_press();
    if (!sdl_main_screen_character_panel_pointer_hit(x, y))
        return true;

    if (long_press && secondary_enabled) {
        (void)sdl_main_screen_handle_character_panel_secondary_pointer(
            start_x, start_y);
        return true;
    }
    if (long_press)
        return true;

    (void)sdl_main_screen_handle_character_panel_pointer(x, y);
    return true;
}

int sdl_character_panel_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_character_panel_press.active)
        return -1;
    if (g_character_panel_press.mouse)
        return -1;
    if (!g_character_panel_press.secondary_enabled)
        return -1;

    elapsed = now_ns - g_character_panel_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_character_panel_flush_pending_press(Uint64 now_ns)
{
    float x;
    float y;

    if (!g_character_panel_press.active)
        return false;
    if (g_character_panel_press.mouse)
        return false;
    if (!g_character_panel_press.secondary_enabled)
        return false;
    if (now_ns - g_character_panel_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    x = g_character_panel_press.start_x;
    y = g_character_panel_press.start_y;
    sdl_character_panel_cancel_press();
    return sdl_main_screen_handle_character_panel_secondary_pointer(x, y);
}

