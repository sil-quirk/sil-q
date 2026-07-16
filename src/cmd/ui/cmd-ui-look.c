#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "cmd/ui/cmd-ui-internal.h"

void do_cmd_target(void)
{
    /* Target set */
    if (target_set_interactive(TARGET_KILL, 0))
    {
        msg_print("Target Selected.");
    }

    /* Target aborted */
    else
    {
        msg_print("Target Aborted.");
    }
}

/*
 * Calculate the bounding box of explored areas, detected monsters, and detected objects.
 * Returns true if any explored area or detected entity found, false otherwise
 * 
 * This function includes positions of monsters detected by items like the
 * Gem of Foes (which have MFLAG_MARK set) in the scrollable bounds, and
 * positions of marked objects (e.g., from Gem of Treasures / detection).
 */
static bool get_explored_bounds(int* min_y, int* max_y, int* min_x, int* max_x)
{
    int y, x, i;
    
    *min_x = p_ptr->cur_map_wid;
    *max_x = 0;
    *min_y = p_ptr->cur_map_hgt;
    *max_y = 0;

    /* Check explored grids */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Check if this grid has been seen */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                if (x < *min_x) *min_x = x;
                if (x > *max_x) *max_x = x;
                if (y < *min_y) *min_y = y;
                if (y > *max_y) *max_y = y;
            }
        }
    }

    /* Also include detected monsters (e.g., from Gem of Foes) */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        
        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;
        
        /* Check if monster is detected (MFLAG_MARK set by detection spells/items) */
        if (m_ptr->mflag & (MFLAG_MARK))
        {
            int my = m_ptr->fy;
            int mx = m_ptr->fx;
            
            if (mx < *min_x) *min_x = mx;
            if (mx > *max_x) *max_x = mx;
            if (my < *min_y) *min_y = my;
            if (my > *max_y) *max_y = my;
        }
    }

    /* Also include marked objects (e.g., from Gem of Treasures / object detection) */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        /* Only include marked (detected/memorized) objects */
        if (!o_ptr->marked)
            continue;

        int oy = o_ptr->iy;
        int ox = o_ptr->ix;
        if (!in_bounds_fully(oy, ox))
            continue;

        if (ox < *min_x) *min_x = ox;
        if (ox > *max_x) *max_x = ox;
        if (oy < *min_y) *min_y = oy;
        if (oy > *max_y) *max_y = oy;
    }

    /* Check if any explored area or detected entity was found */
    return (*min_x <= *max_x && *min_y <= *max_y);
}

/*
 * Look command
 */
static bool g_unified_look_has_start = false;
static int g_unified_look_start_y = 0;
static int g_unified_look_start_x = 0;

void do_cmd_look_at(int y, int x)
{
    if (y < 0 || y >= p_ptr->cur_map_hgt || x < 0 || x >= p_ptr->cur_map_wid)
    {
        do_cmd_look();
        return;
    }

    g_unified_look_has_start = true;
    g_unified_look_start_y = y;
    g_unified_look_start_x = x;
    do_cmd_look();
    g_unified_look_has_start = false;
}

void do_cmd_look(void)
{
    /* Block when hallucinating */
    if (p_ptr->image)
    {
        msg_print("Your vision is too distorted to examine things carefully.");
        window_stuff();
        Term_fresh();
        return;
    }

    /* Use the new unified look system */
    do_cmd_unified_look();
}

/*
 * Unified look command - combines look, scroll, and view functionality
 */

static int unified_sidebar_object_group(const object_type* o_ptr)
{
    if (!o_ptr)
        return LOOK_GROUP_OTHER;

    if (artefact_p(o_ptr))
        return LOOK_GROUP_ARTIFACT;

    switch (o_ptr->tval)
    {
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOW:
    case TV_DIGGING:
    case TV_ARROW:
        return LOOK_GROUP_WEAPON;

    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return LOOK_GROUP_ARMOUR;

    case TV_RING:
    case TV_AMULET:
    case TV_HORN:
    case TV_STAFF:
        return LOOK_GROUP_JEWELRY;

    case TV_EASTER:
        return LOOK_GROUP_HERBS;

    case TV_POTION:
        return LOOK_GROUP_POTIONS;

    case TV_GEM:
        return LOOK_GROUP_GEMS;

    case TV_FOOD:
        if (o_ptr->sval < SV_FOOD_MIN_FOOD)
            return LOOK_GROUP_CONSUMABLE;
        break;
    }

    return LOOK_GROUP_OTHER;
}

static bool unified_look_can_show_monster_at(int y, int x)
{
    int m_idx = cave_m_idx[y][x];

    return (m_idx > 0) && mon_list[m_idx].ml && grid_info_is_available(y, x);
}

static bool unified_look_can_show_marked_object_at(int y, int x)
{
    int o_idx = cave_o_idx[y][x];

    return (o_idx > 0) && o_list[o_idx].k_idx && o_list[o_idx].marked
        && grid_info_is_available(y, x);
}

static bool unified_look_sidebar_in_radius(const unified_look_state* state, int y,
    int x);

static int unified_look_count_visible_entities(unified_look_state* state)
{
    int total_entities = 0;
    int i;

    if (state->show_monsters)
    {
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);

        for (i = 0; i < temp_n; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];

            if (!m_idx) continue;
            if (!unified_look_can_show_monster_at(temp_y[i], temp_x[i])) continue;
            if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i])) continue;

            total_entities++;
        }
    }

    if (state->show_objects)
    {
        int group_counts[LOOK_GROUP_COUNT] = {0};

        get_sorted_target_list(TARGET_LIST_OBJECT, 0);

        for (i = 0; i < temp_n; i++)
        {
            int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
            if (!o_idx)
                continue;

            if (!grid_info_is_available(temp_y[i], temp_x[i]))
                continue;

            object_type* o_ptr = &o_list[o_idx];

            if (!o_ptr->k_idx)
                continue;

            /* Only count marked (memorized) objects (matches sidebar display) */
            if (!o_ptr->marked)
                continue;
            if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i]))
                continue;

            if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
                continue;

            int group = unified_sidebar_object_group(o_ptr);
            if (state->object_group_filter >= 0 && group != state->object_group_filter)
                continue;
            if (state->limit_objects_top_five && group_counts[group] >= 5)
                continue;

            group_counts[group]++;
            total_entities++;
        }
    }

    return total_entities;
}

static int unified_look_count_visible_objects_for_group(unified_look_state* state, int group_filter)
{
    int total_objects = 0;
    int i;

    if (!state)
        return 0;

    int group_counts[LOOK_GROUP_COUNT] = {0};

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    for (i = 0; i < temp_n; i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        if (!o_idx)
            continue;

        if (!grid_info_is_available(temp_y[i], temp_x[i]))
            continue;

        object_type* o_ptr = &o_list[o_idx];

        if (!o_ptr->k_idx)
            continue;

        /* Only count marked (memorized) objects (matches sidebar display) */
        if (!o_ptr->marked)
            continue;
        if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i]))
            continue;

        if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
            continue;

        int group = unified_sidebar_object_group(o_ptr);
        if (group_filter >= 0 && group != group_filter)
            continue;

        if (state->limit_objects_top_five && group_counts[group] >= 5)
            continue;

        group_counts[group]++;
        total_objects++;
    }

    return total_objects;
}

static bool unified_look_sidebar_in_radius(const unified_look_state* state, int y,
    int x)
{
    if (!state || !state->nearby_filter)
        return true;

    return distance(p_ptr->py, p_ptr->px, y, x) <= UNIFIED_LOOK_NEAR_RADIUS;
}

static void unified_look_sync_cursor_selection(unified_look_state* state)
{
    int new_selection;

    if (!state)
        return;

    if ((state->look_mode != 0) || state->in_sidebar_mode)
        return;

    new_selection = unified_look_find_cursor_selection(state, state->cursor_y,
        state->cursor_x);

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        if ((new_selection < 0)
            || (state->highlighted_y != state->cursor_y)
            || (state->highlighted_x != state->cursor_x))
        {
            highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
                false);
            state->highlighted_y = -1;
            state->highlighted_x = -1;
            state->highlighted_entity_type = 0;
        }
    }

    state->selected_entity = new_selection;
}

static void unified_look_select_sidebar_entity(unified_look_state* state,
    int entity_index)
{
    if (!state || entity_index < 0)
        return;

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
            false);
        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
    }

    state->selected_entity = entity_index;
    state->in_sidebar_mode = true;
    state->square_cycling_mode = false;
    state->current_square_entity = 0;
}

enum
{
    UNIFIED_LOOK_PROMPT_MAX_BUTTONS = 16,
    UNIFIED_LOOK_CLICK_COMMAND_BASE = -1000,
    UNIFIED_LOOK_CLICK_PROMPT_BACKGROUND = -1300
};

static bool unified_look_prompt_choice_key(int choice, int* key);

static bool unified_look_apply_sidebar_pointer_action(unified_look_state* state,
    bool compact_look_layout, bool* need_redraw, bool* selection_redraw,
    bool* prompt_hover_redraw)
{
    int clicked_entity = -1;
    int click_action = UI_MENU_CLICK_PRIMARY;
    int prompt_key = 0;

    if (!ui_menu_click_take_action(&clicked_entity, &click_action))
        return false;

    if (unified_look_prompt_choice_key(clicked_entity, &prompt_key))
    {
        if (click_action != UI_MENU_CLICK_HOVER)
            Term_keypress(prompt_key);
        else if (prompt_hover_redraw)
            *prompt_hover_redraw = true;
        return true;
    }

    if (clicked_entity == UNIFIED_LOOK_CLICK_PROMPT_BACKGROUND)
    {
        if (click_action == UI_MENU_CLICK_HOVER && prompt_hover_redraw)
            *prompt_hover_redraw = true;
        return true;
    }

    if (state && state->in_sidebar_mode
        && state->selected_entity == clicked_entity)
    {
        if (click_action == UI_MENU_CLICK_SECONDARY)
            Term_keypress(' ');
        return true;
    }

    unified_look_select_sidebar_entity(state, clicked_entity);
    if (need_redraw)
        *need_redraw = true;
    if (selection_redraw)
        *selection_redraw = !compact_look_layout;

    if (click_action == UI_MENU_CLICK_SECONDARY)
        Term_keypress(' ');

    return true;
}

static bool unified_look_apply_map_cell(unified_look_state* state, int map_y,
    int map_x, bool compact_look_layout, bool* need_redraw,
    bool* selection_redraw)
{
    int new_selection;
    bool new_sidebar_mode;

    if (!state)
        return false;
    if (map_y < 0 || map_y >= p_ptr->cur_map_hgt
        || map_x < 0 || map_x >= p_ptr->cur_map_wid)
    {
        sdl_object_tooltip_clear();
        return true;
    }

    new_selection = unified_look_find_cursor_selection(state, map_y, map_x);
    new_sidebar_mode = (new_selection >= 0);

    if ((state->cursor_y == map_y) && (state->cursor_x == map_x)
        && (state->selected_entity == new_selection)
        && (state->in_sidebar_mode == new_sidebar_mode))
    {
        (void)sdl_object_tooltip_show_grid(map_y, map_x, false);
        return true;
    }

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
            false);
        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
    }

    state->cursor_y = map_y;
    state->cursor_x = map_x;
    state->selected_entity = new_selection;
    state->in_sidebar_mode = new_sidebar_mode;
    state->square_cycling_mode = false;
    state->current_square_entity = 0;
    (void)sdl_object_tooltip_show_grid(map_y, map_x, false);

    if (need_redraw)
        *need_redraw = true;
    if (selection_redraw)
        *selection_redraw = !compact_look_layout;

    return true;
}

static void unified_look_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static bool unified_look_use_compact_layout(void)
{
    return Term && (Term->wid <= 60);
}

static int unified_look_prompt_row(void)
{
    int row;

    if (!Term || Term->hgt <= 0)
        return 0;

    row = ROW_MAP + SCREEN_HGT - 1;
    if (row < 0)
        row = 0;
    if (row >= Term->hgt)
        row = Term->hgt - 1;

    return row;
}

typedef struct unified_look_prompt_button
{
    int key;
    cptr full;
    cptr medium;
    cptr compact;
    cptr tiny;
} unified_look_prompt_button;

static int unified_look_prompt_choice(int key)
{
    return UNIFIED_LOOK_CLICK_COMMAND_BASE - (key & 0xFF);
}

static bool unified_look_prompt_choice_key(int choice, int* key)
{
    int decoded;

    if (choice > UNIFIED_LOOK_CLICK_COMMAND_BASE
        || choice < UNIFIED_LOOK_CLICK_COMMAND_BASE - 0xFF)
    {
        return false;
    }

    decoded = UNIFIED_LOOK_CLICK_COMMAND_BASE - choice;
    if (key)
        *key = decoded;

    return true;
}

static bool unified_look_apply_map_hover(unified_look_state* state,
    bool compact_look_layout, bool* need_redraw, bool* selection_redraw)
{
    int hover_y = 0;
    int hover_x = 0;

    if (!sdl_unified_look_take_map_hover(&hover_y, &hover_x))
        return false;

    return unified_look_apply_map_cell(state, hover_y, hover_x,
        compact_look_layout, need_redraw, selection_redraw);
}

static void unified_look_print_prompt_buttons(
    const unified_look_prompt_button* buttons, int count, bool register_clicks)
{
    int row = unified_look_prompt_row();

    (void)register_clicks;

    if (!buttons || count <= 0)
    {
        sdl_unified_look_prompt_clear();
        return;
    }

    if (count > UNIFIED_LOOK_PROMPT_MAX_BUTTONS)
        count = UNIFIED_LOOK_PROMPT_MAX_BUTTONS;

    sdl_unified_look_prompt_begin(row);
    for (int i = 0; i < count; i++)
    {
        sdl_unified_look_prompt_add(unified_look_prompt_choice(buttons[i].key),
            buttons[i].full, buttons[i].medium, buttons[i].compact,
            buttons[i].tiny);
    }
    sdl_unified_look_prompt_finish();
}

static void unified_look_print_controller_prompt(
    bool compact_look_layout, bool cursor_mode, bool register_clicks)
{
    char prev_label[16];
    char next_label[16];
    char exam_label[16];
    char target_label[16];
    char obj_label[16];
    char mode_label[16];
    char back_label[16];
    char prev_full[32];
    char next_full[32];
    char exam_full[32];
    char target_full[32];
    char obj_full[32];
    char mode_full[32];
    char back_full[32];
    char zoom_in_full[32];
    char zoom_out_full[32];
    char prev_compact[32];
    char next_compact[32];
    char exam_compact[32];
    char target_compact[32];
    char obj_compact[32];
    char mode_compact[32];
    char back_compact[32];
    char zoom_in_compact[32];
    char zoom_out_compact[32];
    cptr obj_action = compact_look_layout ? "View" : "Objects";
    cptr mode_action = cursor_mode ? "Cursor" : "Pan";

    unified_look_prompt_label(steamdeck_prev_page_key(), "L1", prev_label,
        sizeof(prev_label));
    unified_look_prompt_label(steamdeck_next_page_key(), "R1", next_label,
        sizeof(next_label));
    unified_look_prompt_label(' ', "A", exam_label, sizeof(exam_label));
    unified_look_prompt_label('f', "B", target_label, sizeof(target_label));
    unified_look_prompt_label('u', "X", obj_label, sizeof(obj_label));
    unified_look_prompt_label('s', "Y", mode_label, sizeof(mode_label));
    unified_look_prompt_label(ESCAPE, "Esc", back_label, sizeof(back_label));

    strnfmt(prev_full, sizeof(prev_full), "%s Prev", prev_label);
    strnfmt(next_full, sizeof(next_full), "%s Next", next_label);
    strnfmt(exam_full, sizeof(exam_full), "%s Exam", exam_label);
    strnfmt(target_full, sizeof(target_full), "%s Target", target_label);
    strnfmt(obj_full, sizeof(obj_full), "%s %s", obj_label, obj_action);
    strnfmt(mode_full, sizeof(mode_full), "%s %s", mode_label, mode_action);
    strnfmt(back_full, sizeof(back_full), "%s Back", back_label);
    SDL_strlcpy(zoom_in_full, "Zoom +", sizeof(zoom_in_full));
    SDL_strlcpy(zoom_out_full, "Zoom -", sizeof(zoom_out_full));
    strnfmt(prev_compact, sizeof(prev_compact), "%s Prv", prev_label);
    strnfmt(next_compact, sizeof(next_compact), "%s Nxt", next_label);
    strnfmt(exam_compact, sizeof(exam_compact), "%s Ex", exam_label);
    strnfmt(target_compact, sizeof(target_compact), "%s Tgt", target_label);
    strnfmt(obj_compact, sizeof(obj_compact), "%s %s", obj_label,
        compact_look_layout ? "Vw" : "Obj");
    strnfmt(mode_compact, sizeof(mode_compact), "%s %s", mode_label,
        cursor_mode ? "Cur" : "Pan");
    strnfmt(back_compact, sizeof(back_compact), "%s Bk", back_label);
    SDL_strlcpy(zoom_in_compact, "Z+", sizeof(zoom_in_compact));
    SDL_strlcpy(zoom_out_compact, "Z-", sizeof(zoom_out_compact));

    {
        unified_look_prompt_button buttons[UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
        int count = 0;

        buttons[count++] = (unified_look_prompt_button)
            { 'e', prev_full, prev_full, prev_compact, prev_label };
        buttons[count++] = (unified_look_prompt_button)
            { 'i', next_full, next_full, next_compact, next_label };
        buttons[count++] = (unified_look_prompt_button)
            { ' ', exam_full, exam_full, exam_compact, exam_label };
        buttons[count++] = (unified_look_prompt_button)
            { 'f', target_full, target_full, target_compact, target_label };
        buttons[count++] = (unified_look_prompt_button)
            { 'u', obj_full, obj_full, obj_compact, obj_label };
        buttons[count++] = (unified_look_prompt_button)
            { 's', mode_full, mode_full, mode_compact, mode_label };
        buttons[count++] = (unified_look_prompt_button)
            { '+', zoom_in_full, zoom_in_full, zoom_in_compact, zoom_in_compact };
        buttons[count++] = (unified_look_prompt_button)
            { '-', zoom_out_full, zoom_out_full, zoom_out_compact, zoom_out_compact };
        buttons[count++] = (unified_look_prompt_button)
            { ESCAPE, back_full, back_full, back_compact, back_label };

        unified_look_print_prompt_buttons(buttons, count, register_clicks);
    }
}

static void unified_look_print_touch_prompt(bool compact_look_layout,
    bool cursor_mode, cptr filter_action, bool register_clicks)
{
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    bool extended = (term_wid >= 90);
    char filter_full[32];
    char display_full[32];
    char display_compact[32];
    char mode_full[32];
    char mode_compact[32];
    cptr display_action = compact_look_layout ? "View" : "Display";

    if (!filter_action)
        filter_action = "";

    strnfmt(filter_full, sizeof(filter_full), "%s", filter_action);
    strnfmt(display_full, sizeof(display_full), "%s", display_action);
    strnfmt(display_compact, sizeof(display_compact), "%s",
        compact_look_layout ? "View" : "Disp");
    strnfmt(mode_full, sizeof(mode_full), "%s", cursor_mode ? "Cursor" : "Pan");
    strnfmt(mode_compact, sizeof(mode_compact), "%s",
        cursor_mode ? "Cur" : "Pan");

    {
        unified_look_prompt_button buttons[UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
        int count = 0;

        buttons[count++] = (unified_look_prompt_button)
            { '\t', "Next", "Next", "Nxt", "Nxt" };
        buttons[count++] = (unified_look_prompt_button)
            { 'q', "Prev", "Prev", "Prv", "Prv" };
        buttons[count++] = (unified_look_prompt_button)
            { ' ', "Examine", "Exam", "Ex", "Ex" };
        buttons[count++] = (unified_look_prompt_button)
            { 't', "Target", "Target", "Tgt", "Tgt" };
        buttons[count++] = (unified_look_prompt_button)
            { 'i', filter_full, filter_full, filter_full, filter_full };
        buttons[count++] = (unified_look_prompt_button)
            { 'l', display_full, display_full, display_compact,
                display_compact };
        if (extended)
        {
            buttons[count++] = (unified_look_prompt_button)
                { 'm', "Monsters", "Mon", "Mon", "Mon" };
            buttons[count++] = (unified_look_prompt_button)
                { 'o', "Objects", "Obj", "Obj", "Obj" };
            buttons[count++] = (unified_look_prompt_button)
                { 'T', "Top 5", "Top 5", "Top", "Top" };
        }
        buttons[count++] = (unified_look_prompt_button)
            { 's', mode_full, mode_full, mode_compact, mode_compact };
        buttons[count++] = (unified_look_prompt_button)
            { '+', "Zoom +", "Zoom +", "Z+", "Z+" };
        buttons[count++] = (unified_look_prompt_button)
            { '-', "Zoom -", "Zoom -", "Z-", "Z-" };
        buttons[count++] = (unified_look_prompt_button)
            { ESCAPE, "Back", "Back", "Back", "Back" };

        unified_look_print_prompt_buttons(buttons, count, register_clicks);
    }
}

static void unified_look_print_keyboard_prompt(bool cursor_mode,
    cptr filter_action, bool register_clicks)
{
    char filter_full[32];

    if (!filter_action)
        filter_action = "";

    strnfmt(filter_full, sizeof(filter_full), "i %s", filter_action);

    {
        unified_look_prompt_button buttons[UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
        int count = 0;

        buttons[count++] = (unified_look_prompt_button)
            { '\t', "Tab Next", "Tab Next", "Tab N", "Tab" };
        buttons[count++] = (unified_look_prompt_button)
            { 'q', "q Prev", "q Prev", "q P", "q" };
        buttons[count++] = (unified_look_prompt_button)
            { ' ', "Space Exam", "Sp Exam", "Sp Ex", "Sp" };
        buttons[count++] = (unified_look_prompt_button)
            { 't', "t Target", "t Target", "t Tgt", "t" };
        buttons[count++] = (unified_look_prompt_button)
            { 'i', filter_full, filter_full, filter_full, "i" };
        buttons[count++] = (unified_look_prompt_button)
            { 'l', "l Display", "l Disp", "l Dsp", "l" };
        buttons[count++] = (unified_look_prompt_button)
            { 'm', "m Monsters", "m Mon", "m Mon", "m" };
        buttons[count++] = (unified_look_prompt_button)
            { 'o', "o Objects", "o Obj", "o Obj", "o" };
        buttons[count++] = (unified_look_prompt_button)
            { 'T', "T Top5", "T Top", "T Top", "T" };
        buttons[count++] = (unified_look_prompt_button)
            { 's', cursor_mode ? "s Cursor" : "s Pan",
                cursor_mode ? "s Cursor" : "s Pan",
                cursor_mode ? "s Cur" : "s Pan", "s" };
        buttons[count++] = (unified_look_prompt_button)
            { '+', "+ Zoom", "+ Zoom", "+ Zm", "+" };
        buttons[count++] = (unified_look_prompt_button)
            { '-', "- Zoom", "- Zoom", "- Zm", "-" };
        buttons[count++] = (unified_look_prompt_button)
            { ESCAPE, "Esc Back", "Esc Back", "Esc", "Esc" };

        unified_look_print_prompt_buttons(buttons, count, register_clicks);
    }
}

static void unified_look_update_prompt_buttons_ex(bool controller_controls,
    bool compact_look_layout, bool cursor_mode, bool nearby_filter,
    bool register_clicks)
{
    if (sdl_touch_only_device_active())
    {
        unified_look_print_touch_prompt(compact_look_layout, cursor_mode,
            nearby_filter ? "All" : "Near", register_clicks);
    }
    else if (controller_controls)
    {
        unified_look_print_controller_prompt(compact_look_layout, cursor_mode,
            register_clicks);
    }
    else
    {
        unified_look_print_keyboard_prompt(cursor_mode,
            nearby_filter ? "All" : "Near", register_clicks);
    }
}

static void unified_look_restore_map_cursor(const unified_look_state* state)
{
    if (!state)
        return;

    move_cursor_relative(state->cursor_y, state->cursor_x);
}

static void unified_look_redraw_bars(const unified_look_state* state,
    bool controller_controls, bool compact_look_layout, bool register_clicks)
{
    int y;
    int x;

    if (!state)
        return;

    y = state->cursor_y;
    x = state->cursor_x;

    if (state->in_sidebar_mode && state->selected_entity >= 0
        && state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        (void)sdl_object_tooltip_show_grid(state->highlighted_y,
            state->highlighted_x, false);
    }
    else
    {
        (void)sdl_object_tooltip_show_grid(y, x, false);
    }
    unified_look_update_prompt_buttons_ex(controller_controls,
        compact_look_layout, state->look_mode != 0, state->nearby_filter,
        register_clicks);
}

static void unified_look_pause_pointer_handlers(void)
{
    ui_menu_click_clear();
    sdl_unified_look_set_map_hover_enabled(false);
}

static void unified_look_resume_pointer_handlers(void)
{
    sdl_unified_look_set_map_hover_enabled(true);
}

static bool unified_look_examine_object_at(int y, int x, bool use_story_font)
{
    object_type* o_ptr;

    if (!unified_look_can_show_marked_object_at(y, x))
        return false;

    o_ptr = &o_list[cave_o_idx[y][x]];

    if (use_story_font)
        sdl_story_font_disable();
    unified_look_pause_pointer_handlers();

    (void)player_try_identify_smithing_object_on_examine(o_ptr, false);
    screen_save();

    if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
    {
        int slot = wield_slot(o_ptr);
        const object_type* compare_objects[2];
        const char* compare_headings[2];
        char selected_heading[32];
        char equipped_heading[32];

        strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
        strnfmt(equipped_heading, sizeof(equipped_heading), "%s",
            mention_use(slot));

        compare_objects[0] = o_ptr;
        compare_headings[0] = selected_heading;
        compare_objects[1] = inventory[slot].k_idx ? &inventory[slot] : NULL;
        compare_headings[1] = equipped_heading;

        object_info_screen_multi(compare_objects, compare_headings, 2);
    }
    else
    {
        object_info_screen(o_ptr);
    }

    screen_load();
    unified_look_resume_pointer_handlers();

    if (use_story_font)
        sdl_story_font_enable();

    return true;
}

static bool unified_look_examine_monster_at(int y, int x, bool use_story_font)
{
    int m_idx;
    monster_type* m_ptr;

    if (!unified_look_can_show_monster_at(y, x))
        return false;

    m_idx = cave_m_idx[y][x];
    m_ptr = &mon_list[m_idx];

    if (use_story_font)
        sdl_story_font_disable();
    unified_look_pause_pointer_handlers();

    monster_race_track(m_ptr->r_idx);
    health_track(m_idx);
    handle_stuff();

    screen_save();
    (void)screen_roff(m_ptr->r_idx, m_ptr);
    screen_load();

    unified_look_resume_pointer_handlers();

    if (use_story_font)
        sdl_story_font_enable();

    return true;
}

static bool unified_look_target_monster_at(int y, int x)
{
    int m_idx;
    monster_type* m_ptr;
    char m_name[80];

    if (!unified_look_can_show_monster_at(y, x))
        return false;

    m_idx = cave_m_idx[y][x];
    if (!target_able(m_idx)) {
        bell("No clear target.");
        return false;
    }

    m_ptr = &mon_list[m_idx];
    target_set_monster(m_idx);
    health_track(m_idx);
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);
    msg_format("Target set to %s.", m_name);
    p_ptr->redraw |= PR_BASIC | PR_MAP;
    handle_stuff();
    return true;
}

static void unified_look_pan_player_for_sidebar(bool center_vertical)
{
    int max_wy = MAX(p_ptr->cur_map_hgt - SCREEN_HGT, 0);
    int max_wx = MAX(p_ptr->cur_map_wid - SCREEN_WID, 0);
    int desired_player_col = (SCREEN_WID * 2) / 3;
    int new_wy = p_ptr->wy;
    int new_wx;

    if (desired_player_col >= SCREEN_WID)
        desired_player_col = SCREEN_WID - 1;
    if (desired_player_col < 0)
        desired_player_col = 0;

    if (center_vertical)
        new_wy = p_ptr->py - (SCREEN_HGT / 2);

    new_wx = p_ptr->px - desired_player_col;
    if (new_wy < 0) new_wy = 0;
    if (new_wy > max_wy) new_wy = max_wy;
    if (new_wx < 0) new_wx = 0;
    if (new_wx > max_wx) new_wx = max_wx;

    log_trace("Unified look player pan: center_vertical=%d, desired_col=%d, viewport (%d,%d) -> (%d,%d)",
        center_vertical ? 1 : 0, desired_player_col, p_ptr->wy, p_ptr->wx, new_wy, new_wx);

    if (modify_panel(new_wy, new_wx))
        handle_stuff();
}

static void unified_look_constrain_viewport(int* wy, int* wx)
{
    int max_wy = MAX(p_ptr->cur_map_hgt - SCREEN_HGT, 0);
    int max_wx = MAX(p_ptr->cur_map_wid - SCREEN_WID, 0);

    if (!wy || !wx)
        return;

    if (*wy < 0)
        *wy = 0;
    if (*wy > max_wy)
        *wy = max_wy;
    if (*wx < 0)
        *wx = 0;
    if (*wx > max_wx)
        *wx = max_wx;
}

static bool unified_look_map_pan_would_move(int pan_dy, int pan_dx)
{
    int new_wy;
    int new_wx;

    if (pan_dy == 0 && pan_dx == 0)
        return false;

    new_wy = p_ptr->wy + pan_dy;
    new_wx = p_ptr->wx + pan_dx;
    unified_look_constrain_viewport(&new_wy, &new_wx);

    return (new_wy != p_ptr->wy || new_wx != p_ptr->wx);
}

static void unified_look_clear_highlight(unified_look_state* state)
{
    if (!state)
        return;

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
            false);
        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
    }
}

static bool unified_look_apply_map_pan(unified_look_state* state, int pan_dy,
    int pan_dx, bool* need_redraw, bool* selection_redraw)
{
    int old_wy;
    int old_wx;
    int new_wy;
    int new_wx;

    if (!state || (pan_dy == 0 && pan_dx == 0))
        return false;

    old_wy = p_ptr->wy;
    old_wx = p_ptr->wx;
    new_wy = old_wy + pan_dy;
    new_wx = old_wx + pan_dx;
    unified_look_constrain_viewport(&new_wy, &new_wx);

    if (!modify_panel(new_wy, new_wx))
        return true;

    state->cursor_y += p_ptr->wy - old_wy;
    state->cursor_x += p_ptr->wx - old_wx;
    if (state->cursor_y < 0)
        state->cursor_y = 0;
    if (state->cursor_y >= p_ptr->cur_map_hgt)
        state->cursor_y = p_ptr->cur_map_hgt - 1;
    if (state->cursor_x < 0)
        state->cursor_x = 0;
    if (state->cursor_x >= p_ptr->cur_map_wid)
        state->cursor_x = p_ptr->cur_map_wid - 1;

    state->in_sidebar_mode = false;
    state->selected_entity = -1;
    state->square_cycling_mode = false;
    state->current_square_entity = 0;
    unified_look_clear_highlight(state);

    handle_stuff();

    if (need_redraw)
        *need_redraw = true;
    if (selection_redraw)
        *selection_redraw = false;

    return true;
}

static bool unified_look_apply_main_zoom(int scale, bool* need_redraw)
{
    int max_scale = get_sdl_max_main_view_zoom_scale();
    int old_scale = get_sdl_effective_main_view_scale();

    if (max_scale < 1)
        max_scale = 1;
    if (scale < 1)
        scale = 1;
    if (scale > max_scale)
        scale = max_scale;
    if (scale == old_scale)
        return true;

    set_sdl_main_view_zoom_scale(scale);
    if (get_sdl_effective_main_view_scale() == old_scale)
        return true;

    sdl_apply_config_no_redraw();
    p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA | PR_HEALTHBAR | PR_MAP);
    p_ptr->window |= (PW_OVERHEAD);
    handle_stuff();

    if (need_redraw)
        *need_redraw = true;

    return true;
}

static void unified_look_track_cursor_health(const unified_look_state* state)
{
    int cursor_m_idx;

    if (!state)
    {
        health_track(0);
        handle_stuff();
        return;
    }

    cursor_m_idx = cave_m_idx[state->cursor_y][state->cursor_x];
    if ((cursor_m_idx > 0)
        && unified_look_can_show_monster_at(state->cursor_y, state->cursor_x))
    {
        health_track(cursor_m_idx);
    }
    else
    {
        health_track(0);
    }

    handle_stuff();
}

static void unified_look_redraw_overlay(unified_look_state* state,
    bool controller_controls, bool compact_look_layout, bool* overlay_saved,
    bool selection_redraw, bool register_clicks)
{
    if (!state || !overlay_saved)
        return;

    if (selection_redraw && *overlay_saved)
    {
        show_unified_sidebar(state);
    }
    else
    {
        if (*overlay_saved)
        {
            (void)Term_set_extra_cursor(false, 0, 0, false);
            screen_load_quiet();
            *overlay_saved = false;
        }

        unified_look_sync_cursor_selection(state);

        /* Save the clean map before painting look-only UI elements. */
        screen_save();
        *overlay_saved = true;

        show_unified_sidebar(state);
    }

    unified_look_track_cursor_health(state);
    unified_look_redraw_bars(state, controller_controls, compact_look_layout,
        register_clicks);
    unified_look_restore_map_cursor(state);
    Term_fresh();
}

void do_cmd_unified_look(void)
{
    unified_look_state state;
    int y, x;
    char query;
    bool done = false;
    bool need_redraw = true;
    bool overlay_saved = false;
    bool selection_redraw = false;
    bool compact_look_layout = false;
    bool original_hide_left_panel = g_hide_left_panel;
    bool original_suppress_hidden_left_panel_overlay
        = g_suppress_hidden_left_panel_overlay;
    bool look_adjusts_left_panel = false;
    bool look_adjusts_supporting_panes = false;
    bool original_hide_supporting_panes_fullscreen = op_ptr
        ? op_ptr->opt[OPT_hide_supporting_panes_fullscreen] : false;
    int original_wy, original_wx; /* Store original viewport */
    
    /* Clear entry level banner when using look command */
    if (dismiss_active_narrative_banner())
    {
        do_cmd_redraw();
    }
    
    /*
     * Do NOT enable the terminal story font for the look's main view.
     *
     * The look sidebar is drawn as its own SDL overlay
     * (sdl_unified_look_sidebar_*), which selects the story font directly, so
     * it stays styled regardless of the terminal's story-font state.  Turning
     * on the *terminal* story font additionally re-renders the main map's text
     * cells with the proportional, pixel-packed story font: in ASCII display
     * mode that destroys the map grid alignment (and leaves the grid-aligned
     * selection cursor boxes uncovered, so they appear to multiply).  Tile mode
     * hides this because map cells are tiles, not text.  Keep the terminal in
     * its normal monospace font so the map renders correctly in both modes.
     */
    bool use_story_font = false;

    compact_look_layout = unified_look_use_compact_layout();
    
    log_trace("=== UNIFIED LOOK STARTED ===");
    
    /* Store original viewport */
    original_wy = p_ptr->wy;
    original_wx = p_ptr->wx;

    g_suppress_hidden_left_panel_overlay = true;
    g_hide_left_panel = true;
    look_adjusts_left_panel = true;
    if (op_ptr)
        op_ptr->opt[OPT_hide_supporting_panes_fullscreen] = true;
    screen_push_supporting_panes_hidden();
    look_adjusts_supporting_panes = true;
    p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA | PR_HEALTHBAR | PR_MAP);
    p_ptr->window |= (PW_OVERHEAD);
    handle_stuff();
    
    log_trace("Original viewport: (%d,%d)", original_wy, original_wx);
    
    /* Initialize state */
    state.cursor_y = p_ptr->py;
    state.cursor_x = p_ptr->px;
    if (g_unified_look_has_start
        && g_unified_look_start_y >= 0 && g_unified_look_start_y < p_ptr->cur_map_hgt
        && g_unified_look_start_x >= 0 && g_unified_look_start_x < p_ptr->cur_map_wid)
    {
        state.cursor_y = g_unified_look_start_y;
        state.cursor_x = g_unified_look_start_x;

        if (!panel_contains(state.cursor_y, state.cursor_x))
        {
            int max_wy = MAX(p_ptr->cur_map_hgt - SCREEN_HGT, 0);
            int max_wx = MAX(p_ptr->cur_map_wid - SCREEN_WID, 0);
            int new_wy = state.cursor_y - SCREEN_HGT / 2;
            int new_wx = state.cursor_x - SCREEN_WID / 2;

            p_ptr->wy = MIN(MAX(new_wy, 0), max_wy);
            p_ptr->wx = MIN(MAX(new_wx, 0), max_wx);
            p_ptr->redraw |= PR_MAP;
            p_ptr->window |= PW_OVERHEAD;
            handle_stuff();
        }
    }
    state.selected_entity = -1;
    state.show_monsters = true;
    state.show_objects = true;
    state.object_group_filter = -1;
    state.limit_objects_top_five = false;
    state.nearby_filter = look_nearby_filter_default ? true : false;
    state.display_mode = 0; /* 0 = manual, 1 = entity */
    state.highlighted_y = -1;
    state.highlighted_x = -1;
    state.highlighted_entity_type = 0; /* 0 = none, 1 = monster, 2 = object */
    state.in_sidebar_mode = false;
    state.look_mode = 0; /* 0 = normal unified look, 1 = L-style scrolling */
    state.current_square_entity = 0; /* 0 = monster, 1 = object */
    state.square_cycling_mode = false; /* Start in normal sidebar cycling mode */
    const bool controller_controls = steamdeck_controls_active();
    sdl_unified_look_set_active(true);
    sdl_unified_look_set_map_hover_enabled(true);

    if (!g_unified_look_has_start
        || ((state.cursor_y == p_ptr->py) && (state.cursor_x == p_ptr->px)))
    {
        unified_look_pan_player_for_sidebar(false);
    }
    
    /* Track monster health at initial cursor position for left sidebar display */
    int initial_m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
    if ((initial_m_idx > 0)
        && unified_look_can_show_monster_at(state.cursor_y, state.cursor_x))
    {
        /* Track this monster for health display */
        health_track(initial_m_idx);
    }
    else
    {
        /* Clear health tracking when not starting on a visible monster */
        health_track(0);
    }
    
    /* Process redraw flags to update health bar immediately */
    handle_stuff();

    /* Hiding the left panel reflows the map area without changing glyphs, so
     * the cached map cells must be force-repainted or stale (black) cells
     * remain. */
    force_map_redraw();

    /* Main interaction loop */
    while (!done)
    {
        compact_look_layout = unified_look_use_compact_layout();

        if (need_redraw)
        {
            unified_look_redraw_overlay(&state, controller_controls,
                compact_look_layout, &overlay_saved, selection_redraw, true);
            selection_redraw = false;
            need_redraw = false;
        }
        
        /* Get input */
        query = inkey();
        query = steamdeck_menu_key(query, 'e', 'i');
        log_trace("Unified look key input: '%c' (%d) [char: %c, isupper: %d]", 
                 query, (int)query, (query >= 32 && query <= 126) ? query : '?', 
                 (query >= 'A' && query <= 'Z') ? 1 : 0);

        bool pointer_click_pending = ui_menu_click_has_pending();

        /* Keep the overlay live while cycling sidebar selection to avoid
         * flashing back to the map between adjacent redraws. */
        if (overlay_saved
            && query != '\t'
            && query != '`'
            && query != 'q'
            && query != UI_MENU_CLICK_WAKE_KEY
            && !((query == '\r') && pointer_click_pending)
            && !(controller_controls && (query == 'i' || query == 'e')))
        {
            (void)Term_set_extra_cursor(false, 0, 0, false);
            screen_load();
            overlay_saved = false;
            
            /* Update health bar display after screen restore */
            handle_stuff();
        }
        
        /* Analyze input */
        log_trace("Processing key: '%c' (%d), backtick is %d", query, (int)query, (int)'`');
        switch (query)
        {
            case UI_MENU_CLICK_WAKE_KEY:
            {
                int zoom_scale = 0;
                int pan_dy = 0;
                int pan_dx = 0;
                int target_y = 0;
                int target_x = 0;
                int describe_y = 0;
                int describe_x = 0;
                bool prompt_hover_redraw = false;
                bool hover_redraw = ui_menu_click_take_hover_redraw();

                if (sdl_unified_look_take_main_zoom(&zoom_scale))
                {
                    if (overlay_saved)
                    {
                        (void)Term_set_extra_cursor(false, 0, 0, false);
                        screen_load_quiet();
                        overlay_saved = false;
                        handle_stuff();
                    }

                    (void)unified_look_apply_main_zoom(zoom_scale,
                        &need_redraw);
                    break;
                }
                if (sdl_unified_look_take_map_pan(&pan_dy, &pan_dx))
                {
                    bool pan_moves = unified_look_map_pan_would_move(pan_dy,
                        pan_dx);

                    if (!pan_moves)
                    {
                        if (!overlay_saved)
                        {
                            unified_look_redraw_overlay(&state,
                                controller_controls, compact_look_layout,
                                &overlay_saved, false, true);
                        }
                        break;
                    }

                    if (overlay_saved)
                    {
                        (void)Term_set_extra_cursor(false, 0, 0, false);
                        screen_load_quiet();
                        overlay_saved = false;
                        handle_stuff();
                    }

                    if (unified_look_apply_map_pan(&state, pan_dy, pan_dx,
                            &need_redraw, &selection_redraw))
                    {
                        unified_look_redraw_overlay(&state,
                            controller_controls, compact_look_layout,
                            &overlay_saved, false, true);
                        selection_redraw = false;
                        need_redraw = false;
                    }
                    break;
                }
                if (sdl_unified_look_take_map_target(&target_y, &target_x))
                {
                    if (overlay_saved)
                    {
                        (void)Term_set_extra_cursor(false, 0, 0, false);
                        screen_load();
                        overlay_saved = false;
                        handle_stuff();
                    }

                    (void)unified_look_apply_map_cell(&state, target_y,
                        target_x, compact_look_layout, &need_redraw,
                        &selection_redraw);
                    if (unified_look_target_monster_at(target_y, target_x))
                        done = true;
                    else
                        need_redraw = true;
                    selection_redraw = false;
                    break;
                }
                if (sdl_unified_look_take_map_describe(&describe_y,
                        &describe_x))
                {
                    if (overlay_saved)
                    {
                        (void)Term_set_extra_cursor(false, 0, 0, false);
                        screen_load();
                        overlay_saved = false;
                        handle_stuff();
                    }

                    (void)unified_look_apply_map_cell(&state, describe_y,
                        describe_x, compact_look_layout, &need_redraw,
                        &selection_redraw);
                    if (!unified_look_examine_monster_at(describe_y,
                            describe_x, use_story_font)
                        && !unified_look_examine_object_at(describe_y,
                            describe_x, use_story_font))
                    {
                        bell("Nothing to examine.");
                    }
                    need_redraw = true;
                    selection_redraw = false;
                    break;
                }
                if (unified_look_apply_sidebar_pointer_action(&state,
                        compact_look_layout, &need_redraw, &selection_redraw,
                        &prompt_hover_redraw))
                {
                    if (prompt_hover_redraw)
                    {
                        unified_look_update_prompt_buttons_ex(
                            controller_controls, compact_look_layout,
                            state.look_mode != 0, state.nearby_filter, false);
                        unified_look_restore_map_cursor(&state);
                    }
                    break;
                }
                if (unified_look_apply_map_hover(&state, compact_look_layout,
                        &need_redraw, &selection_redraw))
                    break;

                if (hover_redraw)
                {
                    unified_look_update_prompt_buttons_ex(controller_controls,
                        compact_look_layout, state.look_mode != 0,
                        state.nearby_filter, false);
                    unified_look_restore_map_cursor(&state);
                }

                break;
            }

            case 'T':
            {
                state.limit_objects_top_five = !state.limit_objects_top_five;
                log_trace("'T' key pressed - top five toggle now %d", state.limit_objects_top_five ? 1 : 0);

                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }

                handle_stuff();
                need_redraw = true;
                continue;
            }

            /* Handle capital letters - most are now ignored since we use arrows for scrolling */
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
            case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
            case 'O': case 'P': case 'R': case 'S': case 'U':
            case 'V': case 'W': case 'X': case 'Y': case 'Z':
            {
                /* Capital letters are now ignored - use 'l' to switch to panel scroll mode */
                log_trace("Capital letter ignored: '%c' (%d) - use 'l' to switch modes", query, (int)query);
                break;
            }
            
            case ESCAPE:
            case 'Q':
                /* Clear any highlighting before exit */
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                done = true;
                break;
                
            /* Common menu keys - exit unified look and let them be processed normally */
            case '/':            /* Identify symbol */
            case '?':            /* Help */
            case 's':
            {
                /* Switch between cursor mode and panel scrolling mode */
                state.look_mode = (state.look_mode + 1) % 2;
                log_trace("'s' key pressed - look mode changed to: %d", state.look_mode);
                
                /* Update help text based on mode */
                need_redraw = true;
                break;
            }
            
            case 'x':            /* Examine/Look - show description */
            {
                log_trace("EXAMINATION: 'x' key pressed for description");
                
                /* Disable story font for info screens */
                if (use_story_font)
                    sdl_story_font_disable();
                unified_look_pause_pointer_handlers();
                
                /* Same logic as Space/Enter for examination */
                log_trace("EXAMINATION: state.in_sidebar_mode=%d, state.selected_entity=%d", 
                         state.in_sidebar_mode, state.selected_entity);
                log_trace("EXAMINATION: state.highlighted_y=%d, state.highlighted_x=%d", 
                         state.highlighted_y, state.highlighted_x);
                
                if (state.in_sidebar_mode && state.selected_entity >= 0 && 
                    state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions met");
                    
                    int cursor_m_idx = cave_m_idx[state.highlighted_y][state.highlighted_x];
                    int cursor_o_idx = cave_o_idx[state.highlighted_y][state.highlighted_x];
                    
                    log_trace("EXAMINATION: At highlighted position (%d,%d) - m_idx=%d, o_idx=%d, entity_type=%d", 
                             state.highlighted_y, state.highlighted_x, cursor_m_idx, cursor_o_idx, state.highlighted_entity_type);
                    
                    /* Examine the entity based on what was highlighted in the sidebar */
                    /* Entity type: 1 = monster, 2 = object */
                    if (state.highlighted_entity_type == 1 && cursor_m_idx > 0)
                    {
                        /* Monster was highlighted - examine monster */
                        log_trace("EXAMINATION: Highlighted entity is monster, examining monster %d", cursor_m_idx);
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            (void)screen_roff(m_ptr->r_idx, m_ptr);
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else if ((state.highlighted_entity_type == 2)
                        && unified_look_can_show_marked_object_at(
                            state.highlighted_y, state.highlighted_x))
                    {
                        /* Object was highlighted - examine object */
                        log_trace("EXAMINATION: Highlighted entity is object, examining object %d", cursor_o_idx);
                        /* Object examination */
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        (void)player_try_identify_smithing_object_on_examine(
                            o_ptr, false);
                        log_trace("EXAMINATION: Showing object info screen");
                        /* Save screen */
                        screen_save();
                        /* Show object info, with comparison if applicable */
                        if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
                        {
                            int slot = wield_slot(o_ptr);
                            const object_type* compare_objects[2];
                            const char* compare_headings[2];
                            char selected_heading[32];
                            char equipped_heading[32];

                            strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
                            strnfmt(equipped_heading, sizeof(equipped_heading), "%s", mention_use(slot));

                            compare_objects[0] = o_ptr;
                            compare_headings[0] = selected_heading;

                            if (inventory[slot].k_idx)
                            {
                                compare_objects[1] = &inventory[slot];
                            }
                            else
                            {
                                compare_objects[1] = NULL;
                            }

                            compare_headings[1] = equipped_heading;

                            object_info_screen_multi(compare_objects, compare_headings, 2);
                        }
                        else
                        {
                            object_info_screen(o_ptr);
                        }

                        /* Restore screen */
                        screen_load();
                        log_trace("EXAMINATION: Object examination completed");
                    }
                    else if (cursor_m_idx > 0)
                    {
                        log_trace("EXAMINATION: Found monster, examining monster %d", cursor_m_idx);
                        /* Monster examination */
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            (void)screen_roff(m_ptr->r_idx, m_ptr);
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else
                    {
                        log_trace("EXAMINATION: No entities found at highlighted position");
                    }
                }
                else
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions NOT met - using cursor position examination");
                    /* Examine cursor position */
                    y = state.cursor_y;
                    x = state.cursor_x;
                    
                    int cursor_m_idx = cave_m_idx[y][x];
                    int cursor_o_idx = cave_o_idx[y][x];
                    bool has_visible_monster = unified_look_can_show_monster_at(y, x);
                    bool has_object = unified_look_can_show_marked_object_at(y, x);
                    
                    log_trace("EXAMINATION: Cursor position (%d,%d) - has_visible_monster=%d, has_object=%d", 
                             y, x, has_visible_monster, has_object);
                    
                    /* Prioritize OBJECT first, then visible monster */
                    if (has_object)
                    {
                        log_trace("EXAMINATION: Examining object at cursor position");
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        (void)player_try_identify_smithing_object_on_examine(
                            o_ptr, false);
                        screen_save();

                        if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
                        {
                            int slot = wield_slot(o_ptr);
                            const object_type* compare_objects[2];
                            const char* compare_headings[2];
                            char selected_heading[32];
                            char equipped_heading[32];

                            strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
                            strnfmt(equipped_heading, sizeof(equipped_heading), "%s", mention_use(slot));

                            compare_objects[0] = o_ptr;
                            compare_headings[0] = selected_heading;

                            if (inventory[slot].k_idx)
                            {
                                compare_objects[1] = &inventory[slot];
                            }
                            else
                            {
                                compare_objects[1] = NULL;
                            }

                            compare_headings[1] = equipped_heading;

                            object_info_screen_multi(compare_objects, compare_headings, 2);
                        }
                        else
                        {
                            object_info_screen(o_ptr);
                        }

                        screen_load();
                    }
                    else if (has_visible_monster)
                    {
                        log_trace("EXAMINATION: Examining visible monster at cursor position");
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        screen_save();
                        (void)screen_roff(m_ptr->r_idx, m_ptr);
                        screen_load();
                    }
                    else
                    {
                        log_trace("EXAMINATION: No visible entities at cursor position");
                    }
                }
                unified_look_resume_pointer_handlers();
                if (use_story_font)
                    sdl_story_font_enable();

                need_redraw = true;
                break;
            }

            case '+':
            {
                (void)unified_look_apply_main_zoom(
                    get_sdl_effective_main_view_scale() + 1, &need_redraw);
                break;
            }

            case '-':
            {
                (void)unified_look_apply_main_zoom(
                    get_sdl_effective_main_view_scale() - 1, &need_redraw);
                break;
            }
            
            case '[':            /* View monsters */
            case ']':            /* View objects */
            case 'w':            /* Wield/Wear */
            case 'r':            /* Read scroll */
            case 'a':            /* Activate */
            case 'z':            /* Zap rod */
            case '.':            /* Run */
            case ',':            /* Stay */
            case '<':            /* Go up stairs */
            case '>':            /* Go down stairs */
            case 'g':            /* Get/Pickup */
            case 'c':            /* Close */
            case 'j':            /* Jam */
            case '*':            /* Target */
            case '@':            /* Center map */
            case '(':            /* Dungeon history */
            case '|':            /* Screenshots */
            case '~':            /* Various things */
            case '!':            /* OS command */
command_key:
                /* Clear any highlighting before exit */
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                done = true;
                /* Don't consume the key - let it be processed by the main game loop */
                Term_keypress(query);
                break;
                
            case '2':
            case '8':
            case '4':
            case '6':
            case '1':
            case '3':
            case '7':
            case '9':
            {
                /* Arrow key behavior depends on current mode */
                if (state.look_mode == 0)
                {
                    /* Mode 0: Normal unified look - manual cursor scrolling */
                    int dir = target_dir(query);
                    if (dir)
                    {
                        int new_cursor_y = state.cursor_y + ddy[dir];
                        int new_cursor_x = state.cursor_x + ddx[dir];
                        
                        /* Calculate explored bounds */
                        int min_y, max_y, min_x, max_x;
                        bool has_explored = get_explored_bounds(&min_y, &max_y, &min_x, &max_x);
                        
                        if (has_explored)
                        {
                            /* Constrain cursor to explored area */
                            if (new_cursor_y < min_y) new_cursor_y = min_y;
                            if (new_cursor_y > max_y) new_cursor_y = max_y;
                            if (new_cursor_x < min_x) new_cursor_x = min_x;
                            if (new_cursor_x > max_x) new_cursor_x = max_x;
                        }
                        else
                        {
                            /* No explored area, constrain to full map */
                            if (new_cursor_y < 0) new_cursor_y = 0;
                            if (new_cursor_y >= p_ptr->cur_map_hgt) new_cursor_y = p_ptr->cur_map_hgt - 1;
                            if (new_cursor_x < 0) new_cursor_x = 0;
                            if (new_cursor_x >= p_ptr->cur_map_wid) new_cursor_x = p_ptr->cur_map_wid - 1;
                        }
                        
                        state.cursor_y = new_cursor_y;
                        state.cursor_x = new_cursor_x;
                        
                        /* Handle viewport scrolling when cursor reaches screen edge */
                        if (!panel_contains(state.cursor_y, state.cursor_x))
                        {
                            /* Log viewport scrolling */
                            log_trace("Viewport scroll: cursor at (%d,%d), panel (%d,%d)", 
                                     state.cursor_y, state.cursor_x, p_ptr->wy, p_ptr->wx);
                            
                            /* Center the viewport on the cursor */
                            int new_wy = state.cursor_y - SCREEN_HGT / 2;
                            int new_wx = state.cursor_x - SCREEN_WID / 2;
                            int max_wy = MAX(p_ptr->cur_map_hgt - SCREEN_HGT, 0);
                            int max_wx = MAX(p_ptr->cur_map_wid - SCREEN_WID, 0);
                            
                            /* Keep the viewport within the map even if it shows unknown space. */
                            if (new_wy < 0) new_wy = 0;
                            if (new_wy > max_wy)
                                new_wy = max_wy;
                            if (new_wx < 0) new_wx = 0;
                            if (new_wx > max_wx)
                                new_wx = max_wx;
                            
                            /* Use proper panel management function */
                            if (modify_panel(new_wy, new_wx))
                            {
                                /* Handle viewport updates immediately */
                                handle_stuff();
                            }
                            
                            log_trace("New viewport: (%d,%d)", p_ptr->wy, p_ptr->wx);
                        }
                        
                        state.in_sidebar_mode = false;
                        state.selected_entity = -1;
                        state.square_cycling_mode = false;
                        state.current_square_entity = 0;
                        
                        /* Clear old highlighting */
                        if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                        {
                            highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                            state.highlighted_y = -1;
                            state.highlighted_x = -1;
                            state.highlighted_entity_type = 0;
                        }
                        
                        /* Track monster health at cursor position for left sidebar display */
                        int m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
                        if ((m_idx > 0)
                            && unified_look_can_show_monster_at(state.cursor_y,
                                state.cursor_x))
                        {
                            /* Track this monster for health display */
                            health_track(m_idx);
                        }
                        else
                        {
                            /* Clear health tracking when not on a visible monster */
                            health_track(0);
                        }
                        
                        /* Process redraw flags to update health bar immediately */
                        handle_stuff();
                        
                        need_redraw = true;
                    }
                }
                else
                {
                    /* Mode 1: Panel scrolling - arrows scroll whole panels like capital letters */
                    int dir = target_dir(query);
                    if (dir)
                    {
                        log_trace("Panel scrolling mode: key='%c', dir=%d", query, dir);
                        
                        int old_wy = p_ptr->wy;
                        int old_wx = p_ptr->wx;
                        
                        log_trace("Old viewport: (%d,%d)", old_wy, old_wx);
                        
                        /* Apply the motion by full panels */
                        int new_wy = p_ptr->wy + (ddy[dir] * PANEL_HGT);
                        int new_wx = p_ptr->wx + (ddx[dir] * PANEL_WID);
                        
                        /* Calculate explored bounds for viewport constraint */
                        int min_y, max_y, min_x, max_x;
                        int explored_min_wy, explored_max_wy;
                        int explored_min_wx, explored_max_wx;
                        
                        if (get_explored_bounds(&min_y, &max_y, &min_x, &max_x))
                        {
                            /* Calculate viewport bounds based on explored area */
                            explored_min_wy = min_y;
                            explored_max_wy = max_y - SCREEN_HGT + 1;
                            explored_min_wx = min_x;
                            explored_max_wx = max_x - SCREEN_WID + 1;
                            
                            /* Ensure min <= max */
                            if (explored_max_wy < explored_min_wy) explored_max_wy = explored_min_wy;
                            if (explored_max_wx < explored_min_wx) explored_max_wx = explored_min_wx;
                        }
                        else
                        {
                            /* No explored area, use full map */
                            explored_min_wy = 0;
                            explored_max_wy = p_ptr->cur_map_hgt - SCREEN_HGT;
                            explored_min_wx = 0;
                            explored_max_wx = p_ptr->cur_map_wid - SCREEN_WID;
                        }
                        
                        /* Constrain viewport to explored boundaries */
                        if (new_wy < explored_min_wy) new_wy = explored_min_wy;
                        if (new_wx < explored_min_wx) new_wx = explored_min_wx;
                        if (new_wy > explored_max_wy) new_wy = explored_max_wy;
                        if (new_wx > explored_max_wx) new_wx = explored_max_wx;
                        
                        /* Additional safety checks */
                        if (new_wy < 0) new_wy = 0;
                        if (new_wx < 0) new_wx = 0;
                            
                        log_trace("Constrained viewport: (%d,%d)", new_wy, new_wx);

                        /* Use proper panel management function */
                        if (modify_panel(new_wy, new_wx))
                        {
                            /* Update cursor to same relative position */
                            state.cursor_y = state.cursor_y + (p_ptr->wy - old_wy);
                            state.cursor_x = state.cursor_x + (p_ptr->wx - old_wx);
                            
                            /* Boundary check cursor */
                            if (state.cursor_y < 0) state.cursor_y = 0;
                            if (state.cursor_y >= p_ptr->cur_map_hgt) state.cursor_y = p_ptr->cur_map_hgt - 1;
                            if (state.cursor_x < 0) state.cursor_x = 0;
                            if (state.cursor_x >= p_ptr->cur_map_wid) state.cursor_x = p_ptr->cur_map_wid - 1;
                            
                            log_trace("New cursor: (%d,%d)", state.cursor_y, state.cursor_x);

                            /* Handle viewport updates immediately */
                            handle_stuff();
                            
                            /* Track monster health at cursor position for left sidebar display */
                            int m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
                            if ((m_idx > 0)
                                && unified_look_can_show_monster_at(state.cursor_y,
                                    state.cursor_x))
                            {
                                /* Track this monster for health display */
                                health_track(m_idx);
                            }
                            else
                            {
                                /* Clear health tracking when not on a visible monster */
                                health_track(0);
                            }
                            
                            /* Process redraw flags to update health bar immediately */
                            handle_stuff();
                            
                            need_redraw = true;
                        }
                        else
                        {
                            log_trace("Viewport unchanged");
                        }
                    }
                }
                break;
            }
            
            case '\t': /* Tab key */
            case 'i':  /* I key = nearby filter on keyboard, forward cycling in portable UI */
            {
                if (query == 'i' && !controller_controls)
                {
                    state.nearby_filter = !state.nearby_filter;
                    state.selected_entity = -1;
                    state.in_sidebar_mode = false;
                    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                    {
                        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                        state.highlighted_y = -1;
                        state.highlighted_x = -1;
                        state.highlighted_entity_type = 0;
                    }

                    handle_stuff();
                    need_redraw = true;
                    break;
                }
                log_trace("Tab key pressed - cycling entities");
                
                /* Global sidebar cycling only - no square cycling */
                state.in_sidebar_mode = true;
                state.square_cycling_mode = false; /* Always disable square cycling */
                
                /* Count total VISIBLE entities using same logic as sidebar */
                int total_entities = unified_look_count_visible_entities(&state);
                
                log_trace("Total visible entities: %d", total_entities);
                
                if (total_entities > 0)
                {
                    /* Clear previous highlighting */
                    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                    {
                        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    }
                    
                    /* Advance selection */
                    int old_selection = state.selected_entity;
                    state.selected_entity++;
                    if (state.selected_entity >= total_entities)
                        state.selected_entity = 0;
                        
                    log_trace("Entity selection: %d -> %d", old_selection, state.selected_entity);
                }
                
                need_redraw = true;
                selection_redraw = !compact_look_layout;
                break;
            }
            
            case '`': /* Backtick key - reverse Tab cycling */
            case 'q': /* Q key - reverse Tab cycling */
            case 'e': /* E key - reverse Tab cycling in portable UI */
            {
                if (query == 'e' && !controller_controls)
                    goto command_key;
                log_trace("REVERSE CYCLING: Key handler reached - cycling entities backward");
                
                /* Global sidebar cycling only - no square cycling */
                state.in_sidebar_mode = true;
                state.square_cycling_mode = false; /* Always disable square cycling */
                
                /* Count total VISIBLE entities using same logic as sidebar */
                int total_entities = unified_look_count_visible_entities(&state);
                
                log_trace("Total visible entities: %d", total_entities);
                
                if (total_entities > 0)
                {
                    /* Clear previous highlighting */
                    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                    {
                        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    }
                    
                    /* Move backward in selection */
                    int old_selection = state.selected_entity;
                    state.selected_entity--;
                    if (state.selected_entity < 0)
                        state.selected_entity = total_entities - 1;
                        
                    log_trace("Entity selection (backward): %d -> %d", old_selection, state.selected_entity);
                }
                
                need_redraw = true;
                selection_redraw = !compact_look_layout;
                break;
            }
            
            case '\r': /* Enter key */
            case ' ':
            {
                if (unified_look_apply_sidebar_pointer_action(&state,
                        compact_look_layout, &need_redraw, &selection_redraw,
                        NULL))
                    break;

                log_trace("EXAMINATION: Enter/Space key pressed for examination");
                
                /* Disable story font for info screens */
                if (use_story_font)
                    sdl_story_font_disable();
                unified_look_pause_pointer_handlers();
                
                /* Examine current target */
                log_trace("EXAMINATION: state.in_sidebar_mode=%d, state.selected_entity=%d", 
                         state.in_sidebar_mode, state.selected_entity);
                log_trace("EXAMINATION: state.highlighted_y=%d, state.highlighted_x=%d", 
                         state.highlighted_y, state.highlighted_x);
                
                if (state.in_sidebar_mode && state.selected_entity >= 0 && 
                    state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions met");
                    
                    int cursor_m_idx = cave_m_idx[state.highlighted_y][state.highlighted_x];
                    int cursor_o_idx = cave_o_idx[state.highlighted_y][state.highlighted_x];
                    
                    log_trace("EXAMINATION: At highlighted position (%d,%d) - m_idx=%d, o_idx=%d, entity_type=%d", 
                             state.highlighted_y, state.highlighted_x, cursor_m_idx, cursor_o_idx, state.highlighted_entity_type);
                    
                    /* Examine the entity based on what was highlighted in the sidebar */
                    /* Entity type: 1 = monster, 2 = object */
                    if (state.highlighted_entity_type == 1 && cursor_m_idx > 0)
                    {
                        /* Monster was highlighted - examine monster */
                        log_trace("EXAMINATION: Highlighted entity is monster, examining monster %d", cursor_m_idx);
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            (void)screen_roff(m_ptr->r_idx, m_ptr);
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else if ((state.highlighted_entity_type == 2)
                        && unified_look_can_show_marked_object_at(
                            state.highlighted_y, state.highlighted_x))
                    {
                        /* Object was highlighted - examine object */
                        log_trace("EXAMINATION: Highlighted entity is object, examining object %d", cursor_o_idx);
                        /* Object examination */
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        (void)player_try_identify_smithing_object_on_examine(
                            o_ptr, false);
                        log_trace("EXAMINATION: Showing object info screen");
                        /* Save screen */
                        screen_save();
                        /* Show object info, with comparison if applicable */
                        if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
                        {
                            int slot = wield_slot(o_ptr);
                            const object_type* compare_objects[2];
                            const char* compare_headings[2];
                            char selected_heading[32];
                            char equipped_heading[32];

                            strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
                            strnfmt(equipped_heading, sizeof(equipped_heading), "%s", mention_use(slot));

                            compare_objects[0] = o_ptr;
                            compare_headings[0] = selected_heading;

                            if (inventory[slot].k_idx)
                            {
                                compare_objects[1] = &inventory[slot];
                            }
                            else
                            {
                                compare_objects[1] = NULL;
                            }

                            compare_headings[1] = equipped_heading;

                            object_info_screen_multi(compare_objects, compare_headings, 2);
                        }
                        else
                        {
                            object_info_screen(o_ptr);
                        }

                        /* Restore screen */
                        screen_load();
                        log_trace("EXAMINATION: Object examination completed");
                    }
                    else if (cursor_m_idx > 0)
                    {
                        log_trace("EXAMINATION: Found monster, examining monster %d", cursor_m_idx);
                        /* Monster examination */
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            (void)screen_roff(m_ptr->r_idx, m_ptr);
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else
                    {
                        log_trace("EXAMINATION: No entities found at highlighted position");
                    }
                }
                else
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions NOT met - using cursor position examination");
                    /* Examine cursor position */
                    y = state.cursor_y;
                    x = state.cursor_x;
                    
                    int cursor_m_idx = cave_m_idx[y][x];
                    int cursor_o_idx = cave_o_idx[y][x];
                    bool has_visible_monster = unified_look_can_show_monster_at(y, x);
                    bool has_object = unified_look_can_show_marked_object_at(y, x);
                    
                    log_trace("EXAMINATION: Cursor position (%d,%d) - has_visible_monster=%d, has_object=%d", 
                             y, x, has_visible_monster, has_object);
                    
                    /* Prioritize OBJECT first, then visible monster */
                    if (has_object)
                    {
                        log_trace("EXAMINATION: Examining object at cursor position");
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        (void)player_try_identify_smithing_object_on_examine(
                            o_ptr, false);
                        screen_save();

                        if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
                        {
                            int slot = wield_slot(o_ptr);
                            const object_type* compare_objects[2];
                            const char* compare_headings[2];
                            char selected_heading[32];
                            char equipped_heading[32];

                            strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
                            strnfmt(equipped_heading, sizeof(equipped_heading), "%s", mention_use(slot));

                            compare_objects[0] = o_ptr;
                            compare_headings[0] = selected_heading;

                            if (inventory[slot].k_idx)
                            {
                                compare_objects[1] = &inventory[slot];
                            }
                            else
                            {
                                compare_objects[1] = NULL;
                            }

                            compare_headings[1] = equipped_heading;

                            object_info_screen_multi(compare_objects, compare_headings, 2);
                        }
                        else
                        {
                            object_info_screen(o_ptr);
                        }

                        screen_load();
                    }
                    else if (has_visible_monster)
                    {
                        log_trace("EXAMINATION: Examining visible monster at cursor position");
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        screen_save();
                        (void)screen_roff(m_ptr->r_idx, m_ptr);
                        screen_load();
                    }
                    else
                    {
                        log_trace("EXAMINATION: No visible entities at cursor position");
                    }
                }
                
                /* Re-enable story font */
                unified_look_resume_pointer_handlers();
                if (use_story_font)
                    sdl_story_font_enable();
                
                need_redraw = true;
                break;
            }
            
            case 'm':
            {
                log_trace("'m' key pressed - cycling monster display");
                /* Cycle monsters: monsters -> nothing -> monsters */
                if (state.show_monsters)
                {
                    /* From showing monsters to hiding monsters */
                    state.show_monsters = false;
                    log_trace("Mode changed to: monsters hidden");
                }
                else
                {
                    /* From hiding monsters to showing monsters */
                    state.show_monsters = true;
                    log_trace("Mode changed to: monsters shown");
                }
                
                /* Reset selection when changing display */
                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                
                /* Force a complete redraw */
                handle_stuff();
                need_redraw = true;
                log_trace("'m' key: set need_redraw=true, continuing to redraw immediately");
                /* Continue to top of loop to process redraw immediately */
                continue;
            }
            
            case 'u':
                if (!controller_controls)
                    goto command_key;
                if (compact_look_layout)
                    goto cycle_display_modes;
                /* fallthrough */
            case 'o':
            {
                log_trace("'o' key pressed - cycling object categories");

                /* Cycle: all -> weapons -> armour -> artifacts -> herbs -> potions -> gems -> consumables -> other -> hidden */
                static const int object_filter_cycle[] = {
                    LOOK_GROUP_ARTIFACT,
                    LOOK_GROUP_WEAPON,
                    LOOK_GROUP_ARMOUR,
                    LOOK_GROUP_JEWELRY,
                    LOOK_GROUP_HERBS,
                    LOOK_GROUP_POTIONS,
                    LOOK_GROUP_GEMS,
                    LOOK_GROUP_CONSUMABLE,
                    LOOK_GROUP_OTHER,
                };

                if (!state.show_objects)
                {
                    state.show_objects = true;
                    state.object_group_filter = -1;
                    log_trace("Object display: shown (ALL)");
                }
                else if (state.object_group_filter < 0)
                {
                    /* Skip empty categories */
                    int next_group = -1;
                    for (size_t idx = 0; idx < N_ELEMENTS(object_filter_cycle); ++idx)
                    {
                        int group = object_filter_cycle[idx];
                        if (unified_look_count_visible_objects_for_group(&state, group) > 0)
                        {
                            next_group = group;
                            break;
                        }
                    }

                    if (next_group >= 0)
                    {
                        state.object_group_filter = next_group;
                        log_trace("Object display: filtered (group=%d)", state.object_group_filter);
                    }
                    else
                    {
                        state.show_objects = false;
                        state.object_group_filter = -1;
                        log_trace("Object display: hidden (no non-empty categories)");
                    }
                }
                else
                {
                    int next_group = -1;
                    for (size_t idx = 0; idx < N_ELEMENTS(object_filter_cycle); ++idx)
                    {
                        if (object_filter_cycle[idx] != state.object_group_filter)
                            continue;

                        /* Skip empty categories */
                        for (size_t j = idx + 1; j < N_ELEMENTS(object_filter_cycle); ++j)
                        {
                            int group = object_filter_cycle[j];
                            if (unified_look_count_visible_objects_for_group(&state, group) > 0)
                            {
                                next_group = group;
                                break;
                            }
                        }
                        break;
                    }

                    if (next_group >= 0)
                    {
                        state.object_group_filter = next_group;
                        log_trace("Object display: filtered (group=%d)", state.object_group_filter);
                    }
                    else
                    {
                        state.show_objects = false;
                        state.object_group_filter = -1;
                        log_trace("Object display: hidden");
                    }
                }
                
                /* Reset selection when changing display */
                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                
                /* Force a complete redraw */
                handle_stuff();
                need_redraw = true;
                log_trace("'o' key: set need_redraw=true, continuing to redraw immediately");
                /* Continue to top of loop to process redraw immediately */
                continue;
            }
            
            case 'l':
cycle_display_modes:
            {
                log_trace("'l' key pressed - cycling through display modes");

                if (compact_look_layout)
                {
                    /* Compact cycle: both -> objects -> monsters -> both. */
                    if (state.show_monsters && state.show_objects)
                    {
                        state.show_monsters = false;
                        state.show_objects = true;
                        log_trace("Mode changed to: objects only");
                    }
                    else if (!state.show_monsters && state.show_objects)
                    {
                        state.show_monsters = true;
                        state.show_objects = false;
                        log_trace("Mode changed to: monsters only");
                    }
                    else
                    {
                        state.show_monsters = true;
                        state.show_objects = true;
                        log_trace("Mode changed to: both monsters and objects");
                    }
                }
                else if (state.show_monsters && state.show_objects)
                {
                    /* From both to objects only */
                    state.show_monsters = false;
                    state.show_objects = true;
                    log_trace("Mode changed to: objects only");
                }
                else if (!state.show_monsters && state.show_objects)
                {
                    /* From objects only to nothing */
                    state.show_monsters = false;
                    state.show_objects = false;
                    log_trace("Mode changed to: nothing (all hidden)");
                }
                else
                {
                    /* From nothing (or monsters only) to both */
                    state.show_monsters = true;
                    state.show_objects = true;
                    log_trace("Mode changed to: both monsters and objects");
                }
                
                /* Reset selection when changing display */
                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                
                /* Force a complete redraw */
                handle_stuff();
                need_redraw = true;
                log_trace("'l' key: set need_redraw=true, continuing to redraw immediately");
                /* Continue to top of loop to process redraw immediately */
                continue;
            }
            
            case 'f':
                if (!controller_controls)
                    goto command_key;
                /* fall through */
            case 't':
            {
                /* Target monster at cursor position or selected position */
                int target_y = state.cursor_y;
                int target_x = state.cursor_x;
                
                /* Use highlighted position if in sidebar mode */
                if (state.in_sidebar_mode && state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    target_y = state.highlighted_y;
                    target_x = state.highlighted_x;
                }
                
                int m_idx = cave_m_idx[target_y][target_x];
                if ((m_idx > 0) && unified_look_can_show_monster_at(target_y, target_x))
                {
                    /* Set target to the monster */
                    target_set_monster(m_idx);
                    
                    /* Get monster description for message */
                    char m_name[80];
                    monster_desc(m_name, sizeof(m_name), &mon_list[m_idx], 0x80);
                    msg_format("Target set to %s.", m_name);
                    
                    /* Exit unified look after targeting */
                    done = true;
                }
                else
                {
                    msg_print("No monster at cursor position.");
                }
                break;
            }
            
            case 'p':
            {
                /* Return to player position */
                state.cursor_y = p_ptr->py;
                state.cursor_x = p_ptr->px;
                state.in_sidebar_mode = false;
                state.selected_entity = -1;
                unified_look_pan_player_for_sidebar(true);
                need_redraw = true;
                break;
            }
            
            default:
            {
                /* Unhandled key - exit like ESC */
                log_trace("Unhandled key in unified look: '%c' (%d) - exiting", query, (int)query);
                /* Clear any highlighting before exit */
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                    state.highlighted_entity_type = 0;
                }
                done = true;
                break;
            }
        }
    }
    
    /* Clear any highlighting */
    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
    {
        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
        state.highlighted_y = -1;
        state.highlighted_x = -1;
        state.highlighted_entity_type = 0;
    }

    (void)Term_set_extra_cursor(false, 0, 0, false);
    ui_menu_click_clear();
    sdl_unified_look_prompt_clear();
    sdl_unified_look_sidebar_clear();
    sdl_unified_look_set_active(false);
    sdl_unified_look_set_map_hover_enabled(false);

    if (overlay_saved)
    {
        screen_load();
        overlay_saved = false;
    }
    
    log_trace("=== UNIFIED LOOK ENDED ===");
    
    /* Clear health tracking before exiting look command */
    health_track(0);
    
    /* Disable story font if it was enabled */
    if (use_story_font)
    {
        log_debug("do_cmd_unified_look: Disabling story font");
        sdl_story_font_disable();
    }
    
    if (look_adjusts_left_panel)
    {
        g_hide_left_panel = original_hide_left_panel;
        g_suppress_hidden_left_panel_overlay
            = original_suppress_hidden_left_panel_overlay;
        p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA | PR_HEALTHBAR | PR_MAP);
        p_ptr->window |= (PW_OVERHEAD);
    }
    if (look_adjusts_supporting_panes)
    {
        screen_pop_supporting_panes_hidden();
        if (op_ptr)
            op_ptr->opt[OPT_hide_supporting_panes_fullscreen]
                = original_hide_supporting_panes_fullscreen;
        p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA | PR_HEALTHBAR | PR_MAP);
        p_ptr->window |= (PW_OVERHEAD);
    }

    /* Restore original viewport */
    bool viewport_changed = (p_ptr->wy != original_wy)
        || (p_ptr->wx != original_wx);

    if (viewport_changed)
    {
        p_ptr->wy = original_wy;
        p_ptr->wx = original_wx;
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_OVERHEAD);
    }

    handle_stuff();

    /* Restoring the left panel reflows the map area the same way; force the
     * map cells to repaint so no stale (black) cells survive into normal play. */
    force_map_redraw();
    Term_fresh();
}

/*
 * Highlight an entity on the map 
 */
void highlight_entity_on_map(int y, int x, bool highlight)
{
    highlight_entity_on_map_type(y, x, highlight, 0); /* Default: auto-detect */
}

void highlight_entity_on_map_type(int y, int x, bool highlight, int entity_type)
{
    if (highlight)
    {
        /* Get the original character and color, but show with blue background */
        char display_char;
        byte display_attr;
        object_type* floor_obj = NULL;
        bool has_live_object = false;

        if (cave_o_idx[y][x] > 0)
        {
            floor_obj = &o_list[cave_o_idx[y][x]];
            has_live_object = floor_obj->k_idx ? true : false;
        }

        /* Determine what to display based on entity_type preference */
        /* entity_type: 0=auto-detect, 1=prefer monster, 2=prefer object */

        if (entity_type == 2 && has_live_object)
        {
            /* Prefer object display */
            display_char = object_char(floor_obj);
            display_attr = object_attr(floor_obj); /* Keep original object color */
            log_trace("Highlighting object '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else if (entity_type == 1 && cave_m_idx[y][x] > 0)
        {
            /* Prefer monster display */
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            display_char = monster_char(r_ptr);
            display_attr = monster_attr(r_ptr); /* Keep original monster color */
            log_trace("Highlighting monster '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else if (cave_m_idx[y][x] > 0)
        {
            /* Auto-detect: For monsters, show normal appearance (no color change) */
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            display_char = monster_char(r_ptr);
            display_attr = monster_attr(r_ptr); /* Keep original monster color */
            log_trace("Highlighting monster '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else if (has_live_object)
        {
            /* Auto-detect: For objects, show normal appearance (no color change) */
            display_char = object_char(floor_obj);
            display_attr = object_attr(floor_obj); /* Keep original object color */
            log_trace("Highlighting object '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else
        {
            /* Empty space - use a cursor */
            display_char = '+';
            display_attr = TERM_L_BLUE;
            log_trace("Highlighting empty space at (%d,%d) -> showing blue cursor", y, x);
        }
        
        /* Draw highlighted character */
        print_rel(display_char, display_attr, y, x);
        log_trace("Applied blue highlighting: char='%c', attr=%d", 
                 display_char, display_attr);
    }
    else
    {
        /* Restore original display */
        lite_spot(y, x);
        log_trace("Restored original display at (%d,%d)", y, x);
    }
}

/*
 * Allow the player to examine other sectors on the map
 */
void do_cmd_locate(void)
{
    int dir, y2, x2;
    int min_y, max_y, min_x, max_x;
    int explored_min_wy, explored_max_wy;
    int explored_min_wx, explored_max_wx;

    /* Block when hallucinating */
    if (p_ptr->image)
    {
        msg_print("Your vision is too distorted to map your location.");
        return;
    }

    /* Clear entry level banner when using L command */
    if (dismiss_active_narrative_banner())
    {
        do_cmd_redraw();
    }

    /* Calculate explored bounds */
    if (get_explored_bounds(&min_y, &max_y, &min_x, &max_x))
    {
        /* Calculate viewport bounds based on explored area */
        explored_min_wy = min_y;
        explored_max_wy = max_y - SCREEN_HGT + 1;
        explored_min_wx = min_x;
        explored_max_wx = max_x - SCREEN_WID + 1;
        
        /* Ensure min <= max */
        if (explored_max_wy < explored_min_wy) explored_max_wy = explored_min_wy;
        if (explored_max_wx < explored_min_wx) explored_max_wx = explored_min_wx;
    }
    else
    {
        /* No explored area, use full map */
        explored_min_wy = 0;
        explored_max_wy = p_ptr->cur_map_hgt - SCREEN_HGT;
        explored_min_wx = 0;
        explored_max_wx = p_ptr->cur_map_wid - SCREEN_WID;
    }

    /* Start at current panel */
    y2 = p_ptr->wy;
    x2 = p_ptr->wx;

    /*
     * Lightweight scroll mode: direction / arrow keys pan the viewport like
     * scrolling the screen (it can also be panned with the mouse or touch).
     * There is no dedicated menu or prompt; any non-direction key leaves
     * scroll mode and recentres on the player.
     */
    bool saved_hide_cursor = hide_cursor;

    hide_cursor = true;
    while (true)
    {
        char command = inkey();

        /* Ignore pointer-wake pseudo-keys so hovering does not exit. */
        if (command == UI_MENU_CLICK_WAKE_KEY)
            continue;

        /* Any non-direction key leaves scroll mode. */
        dir = target_dir(command);
        if (!dir)
            break;

        /* Pan by a screenful in the chosen direction. */
        y2 += (ddy[dir] * PANEL_HGT);
        x2 += (ddx[dir] * PANEL_WID);

        /* Constrain to explored bounds */
        if (y2 > explored_max_wy)
            y2 = explored_max_wy;
        if (y2 < explored_min_wy)
            y2 = explored_min_wy;

        if (x2 > explored_max_wx)
            x2 = explored_max_wx;
        if (x2 < explored_min_wx)
            x2 = explored_min_wx;

        /* Handle "changes" */
        if ((p_ptr->wy != y2) || (p_ptr->wx != x2))
        {
            /* Update panel */
            p_ptr->wy = y2;
            p_ptr->wx = x2;

            /* Redraw map */
            p_ptr->redraw |= (PR_MAP);

            /* Window stuff */
            p_ptr->window |= (PW_OVERHEAD);

            /* Handle stuff */
            handle_stuff();
        }
    }
    hide_cursor = saved_hide_cursor;

    /* Verify panel */
    p_ptr->update |= (PU_PANEL);

    /* Handle stuff */
    handle_stuff();
}

/*
 * The table of "symbol info" -- each entry is a string of the form
 * "X:desc" where "X" is the trigger, and "desc" is the "info".
 */
static cptr ident_info[]
    = { " :A dark grid", "!:A potion (or oil)", "\":An amulet", "#:A wall",
          /* "$:unused", */
          "%:A quartz vein", "&:A plant", "':An open door", "(:Soft armour",
          "):A shield", "*:A gem (or unseen monster)", "+:A closed door",
          ",:Food", "-:Arrows", ".:Floor", "/:An axe or polearm", "0:A forge",
          /* "1:unused", */
          /* "2:unused", */
          /* "3:unused", */
          /* "4:unused", */
          /* "5:unused", */
          /* "6:unused", */
          /* "7:unused", */
          /* "8:unused", */
          /* "9:unused", */
          "::Rubble", ";:A glyph of warding", "<:A staircase up", "=:A ring",
          ">:A staircase down", "?:An instrument", "@:Elf, Dwarf, or Man",
          /* "A:unused", */
          /* "B:unused", */
          "C:Canine", "D:Dragon",
          /* "E:unused", */
          /* "F:unused", */
          "G:Giant", "H:Horror", "I:Insect",
          /* "J:unused", */
          /* "K:unused", */
          /* "L:unused", */
          "M:Spider", "N:Nameless Thing",
          /* "O:unused", */
          "P:Giant",
          /* "Q:unused", */
          "R:Rauko", "S:Ancient Serpent", "T:Troll",
          /* "U:unused", */
          "V:Valar", "W:Wight/Wraith",
          /* "X:unused", */
          /* "Y:unused", */
          /* "Z:unused", */
          "[:Mail", "\\:A blunt weapon (or digger)", "]:Misc. armour",
          "^:A trap", "_:A staff",
          /* "`:unused", */
          /* "a:unused", */
          "b:Bat/Bird",
          /* "c:unused", */
          "d:Dragon",
          /* "e:unused", */
          "f:Feline",
          /* "g:unused", */
          /* "h:unused", */
          /* "i:unused", */
          /* "j:unused", */
          /* "k:unused", */
          /* "l:unused", */
          "m:Young Spider",
          /* "n:unused", */
          "o:Orc",
          /* "p:unused", */
          /* "q:unused", */
          /* "r:unused", */
          "s:Serpent",
          /* "t:unused", */
          /* "u:unused", */
          "v:Vampire", "w:Creeping Shadow",
          /* "x:unused", */
          /* "y:unused", */
          /* "z:unused", */
          /* "{:unused", */
          "|:An edged weapon (sword/dagger/etc)", "}:A bow",
          "~:A tool (or miscellaneous item)", NULL };

/*
 * Sorting hook -- Comp function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform on "u".
 */
bool ang_sort_comp_hook(const void* u, const void* v, int a, int b)
{
    u16b* who = (u16b*)(u);

    u16b* why = (u16b*)(v);

    int w1 = who[a];
    int w2 = who[b];

    int z1, z2;

    /* Sort by player kills */
    if (*why >= 4)
    {
        /* Extract player kills */
        z1 = l_list[w1].pkills;
        z2 = l_list[w2].pkills;

        /* Compare player kills */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by total kills */
    if (*why >= 3)
    {
        /* Extract total kills */
        z1 = l_list[w1].tkills;
        z2 = l_list[w2].tkills;

        /* Compare total kills */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by monster level */
    if (*why >= 2)
    {
        /* Extract levels */
        z1 = r_info[w1].level;
        z2 = r_info[w2].level;

        /* Compare levels */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by monster depth */
    if (*why >= 1)
    {
        /* Extract experience */
        z1 = r_info[w1].level;
        z2 = r_info[w2].level;

        /* Compare experience */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Compare indexes */
    return (w1 <= w2);
}

/*
 * Sorting hook -- Swap function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform.
 */
void ang_sort_swap_hook(void* u, void* v, int a, int b)
{
    u16b* who = (u16b*)(u);

    u16b holder;

    /* Unused parameter */
    (void)v;

    /* Swap */
    holder = who[a];
    who[a] = who[b];
    who[b] = holder;
}

/*
 * Identify a character, allow recall of monsters
 *
 * Several "special" responses recall "multiple" monsters:
 *   ^A (all monsters)
 *   ^U (all unique monsters)
 *   ^N (all non-unique monsters)
 *
 * The responses may be sorted in several ways, see below.
 *
 *
 */
void do_cmd_query_symbol(void)
{
    int i, n, r_idx;
    char sym, query;
    char buf[128];

    bool all = false;
    bool uniq = false;
    bool norm = false;

    bool recall = false;

    u16b why = 0;
    u16b* who;

    /* Get a character, or abort */
    if (!get_com("Enter character to be identified: ", &sym))
        return;

    /* Find that character info, and describe it */
    for (i = 0; ident_info[i]; ++i)
    {
        if (sym == ident_info[i][0])
            break;
    }

    /* Describe */
    if (sym == KTRL('A'))
    {
        all = true;
        SDL_strlcpy(buf, "Full monster list.", sizeof(buf));
    }
    else if (sym == KTRL('U'))
    {
        all = uniq = true;
        SDL_strlcpy(buf, "Unique monster list.", sizeof(buf));
    }
    else if (sym == KTRL('N'))
    {
        all = norm = true;
        SDL_strlcpy(buf, "Non-unique monster list.", sizeof(buf));
    }
    else if (ident_info[i])
    {
        strnfmt(buf, sizeof(buf), "%c - %s.", sym, ident_info[i] + 2);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "%c - %s.", sym, "Unknown Symbol");
    }

    /* Display the result */
    prt(buf, 0, 0);

    /* Allocate the "who" array */
    who = mem_alloc_array(z_info->r_max, u16b);

    /* Collect matching monsters */
    for (n = 0, i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Nothing to recall */
        if (!cheat_know && !l_ptr->tsights && !know_monster_info)
            continue;

        /* Require non-unique monsters if needed */
        if (norm && (r_ptr->flags1 & (RF1_UNIQUE)))
            continue;

        /* Require unique monsters if needed */
        if (uniq && !(r_ptr->flags1 & (RF1_UNIQUE)))
            continue;

        // Ignore monsters that can't be generated
        if (r_ptr->level > 25)
            continue;

        /* Collect "appropriate" monsters */
        if (all || (r_ptr->d_char == sym))
            who[n++] = i;
    }

    /* Nothing to recall */
    if (!n)
    {
        /* XXX XXX Free the "who" array */
        who = mem_free(who);

        return;
    }

    /* Prompt */
    put_str("Recall details? (k/p/y/n): ", 0, 40);

    /* Query */
    query = inkey();

    /* Restore */
    prt(buf, 0, 0);

    /* Sort by kills (and level) */
    if (query == 'k')
    {
        why = 4;
        query = 'y';
    }

    /* Sort by level */
    if (query == 'p')
    {
        why = 2;
        query = 'y';
    }

    /* Catch "escape" */
    if (query != 'y')
    {
        /* XXX XXX Free the "who" array */
        who = mem_free(who);

        return;
    }

    /* Sort if needed */
    if (why)
    {
        /* Select the sort method */
        ang_sort_comp = ang_sort_comp_hook;
        ang_sort_swap = ang_sort_swap_hook;

        /* Sort the array */
        ang_sort(who, &why, n);
    }

    /* Start at the end */
    i = n - 1;

    /* Scan the monster memory */
    while (1)
    {
        /* Extract a race */
        r_idx = who[i];

        /* Hack -- Auto-recall */
        monster_race_track(r_idx);

        /* Hack -- Handle stuff */
        handle_stuff();

        /* Hack -- Begin the prompt */
        roff_top(r_idx);

        /* Hack -- Complete the prompt */
        Term_addstr(-1, TERM_WHITE, " [(r)ecall, ESC]");

        /* Interact */
        while (1)
        {
            query = 0;

            /* Recall (raging players don't get recall) */
            if (recall)
            {
                int recall_key;

                /* Save screen */
                screen_save();

                /* Recall on screen */
                recall_key = screen_roff(who[i], NULL);

                query = (char)recall_key;
            }

            /* Command */
            if (!recall || !query)
                query = inkey();

            /* Unrecall */
            if (recall)
            {
                /* Load screen */
                screen_load();
            }

            /* Normal commands */
            if (query != 'r')
                break;

            /* Toggle recall */
            recall = !recall;
        }

        /* Stop scanning */
        if (query == ESCAPE)
            break;

        /* Move to "prev" monster */
        if (query == '-')
        {
            if (++i == n)
            {
                i = 0;
            }
        }

        /* Move to "next" monster */
        else
        {
            if (i-- == 0)
            {
                i = n - 1;
            }
        }
    }

    /* Re-display the identity */
    prt(buf, 0, 0);

    /* Free the "who" array */
    who = mem_free(who);
}
