#include "angband.h"
#include "sdl-config.h"
#include "sound-config.h"
#include "sdl-sound.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "pane.h"
#include "cmd/ui/cmd-ui-internal.h"

void write_direction_from_player_to_buffer(
    int y, int x, char* buffer, int buffer_size)
{
    bool north, south, east, west;
    int buffer_offset = 0;

    if (buffer_size < 10)
        return;

    north = p_ptr->py > y;
    south = p_ptr->py < y;
    east = p_ptr->px < x;
    west = p_ptr->px > x;

    if (north)
    {
        strncpy(buffer, "north", 6);
        strcpy(buffer, "north");
        buffer_offset += 5;
    }
    else if (south)
    {
        strncpy(buffer, "south", 6);
        buffer_offset += 5;
    }

    if (east)
    {
        strncpy(buffer + buffer_offset, "east", 5);
        buffer_offset += 4;
    }
    else if (west)
    {
        strncpy(buffer + buffer_offset, "west", 5);
        buffer_offset += 4;
    }
}

#define MAX_VIEW_LINES 50

typedef struct view_monster_data_line view_monster_data_line;
struct view_monster_data_line
{
    int distance;
    char monster_character;
    int monster_color;
    int alert_color;
    char direction[12];
    char name[40];
    char stance[20];
};

typedef struct view_object_data_line view_object_data_line;
struct view_object_data_line
{
    int distance;
    char object_character;
    int object_color;
    int name_color;
    char direction[12];
    char name[80];
};

static byte look_object_name_color(const object_type* o_ptr)
{
    if (weapon_glows(o_ptr))
        return object_display_color(o_ptr, TERM_L_BLUE);

    return object_display_color(o_ptr,
        tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
}

static void append_look_smithing_debug(char* buf, size_t buf_size,
    const object_type* o_ptr)
{
    char smith_buf[20];

    smith_buf[0] = '\0';
    if (op_ptr->opt[OPT_show_smithing_difficulty_look] && object_known_p(o_ptr)
        && object_uses_smithing_difficulty(o_ptr))
    {
        int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
        int sd = object_smithing_difficulty(o_ptr);
        int wr = object_weight_rarity(o_ptr, depth);

        strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
        SDL_strlcat(buf, smith_buf, buf_size);
    }
}

void show_nearby_monsters(bool line_of_sight_only)
{
    view_monster_data_line lines[MAX_VIEW_LINES];

    int i, j;
    int col;
    int longest_name_length = 0;
    int longest_direction_length = 0;
    int longest_stance_length = 0;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;

    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_lines = MIN(MAX_VIEW_LINES, term_hgt - 3); /* Leave space for header and footer */

    get_sorted_target_list(TARGET_LIST_MONSTER, 0);

    j = 0;
    for (i = 0; i < temp_n; i++)
    {
        int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
        monster_type* m_ptr = &mon_list[m_idx];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        char m_name[40];
        int name_length;

        if (j >= max_lines)
            break;
        if (!m_ptr->ml)
            continue;
        if (!player_has_los_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(lines[j].direction, '\0', sizeof(lines[j].direction));
        memset(lines[j].name, '\0', sizeof(lines[j].name));
        memset(lines[j].stance, '\0', sizeof(lines[j].stance));

        monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);
        name_length = strlen(m_name);

        longest_name_length = MAX(longest_name_length, name_length);

        write_direction_from_player_to_buffer(temp_y[i], temp_x[i],
            lines[j].direction, sizeof(lines[j].direction));
        if (!get_alertness_text(m_ptr, sizeof(lines[j].stance), lines[j].stance,
                &lines[j].alert_color))
            return;
        longest_direction_length = MAX(longest_direction_length,
            (int)strlen(lines[j].direction));
        longest_stance_length = MAX(longest_stance_length,
            (int)strlen(lines[j].stance));

        lines[j].monster_character = monster_char(r_ptr);
        lines[j].monster_color = monster_attr(r_ptr);

        lines[j].distance
            = distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);

        strncpy(lines[j].name, m_name, sizeof(lines[j].name));

        j++;
    }

    if (!j)
    {
        int empty_col = MAX(0, (term_wid - 20) / 2);
        Term_erase(0, 1, 255);
        Term_erase(0, 2, 255);
        Term_erase(0, 3, 255);
        Term_putstr(empty_col, 1, term_wid - empty_col, TERM_WHITE,
            "No visible monsters.");
        return;
    }

    col = term_wid - longest_name_length - longest_direction_length
        - longest_stance_length - 9;
    col = MAX(0, col);

    for (i = 0; i < j; ++i)
    {
        int distance_color;
        char monster_char[2];
        int direction_col = col + 6;
        int name_col = direction_col + MAX(longest_direction_length, 1) + 1;
        int stance_col = term_wid - MAX(longest_stance_length, 1) - 1;
        int name_width = stance_col - name_col - 1;
        bool show_stance = true;

        monster_char[0] = lines[i].monster_character;
        monster_char[1] = '\0';

        if (lines[i].distance < 5)
            distance_color = TERM_WHITE;
        else if (lines[i].distance < 10)
            distance_color = TERM_L_WHITE;
        else
            distance_color = TERM_L_DARK;

        /* Clear the line */
        Term_erase(col, i + 1, term_wid - col);

        if (name_width < 8)
        {
            show_stance = false;
            name_width = term_wid - name_col - 1;
        }
        if (name_width < 1)
            name_width = 1;

        c_put_str(lines[i].monster_color, monster_char, i + 1, col + 2);
        if (use_bigtile)
        {
            Term_putch(col + 3, i + 1, 255, -1);
        }
        Term_putstr(direction_col, i + 1, MAX(longest_direction_length, 1),
            distance_color, lines[i].direction);
        Term_putstr(name_col, i + 1, name_width, TERM_WHITE, lines[i].name);
        if (show_stance)
        {
            Term_putstr(stance_col, i + 1, term_wid - stance_col,
                lines[i].alert_color, lines[i].stance);
        }
    }

    if (j)
    {
        Term_erase(col, j + 1, term_wid - col);
    }
}

void show_nearby_objects(bool line_of_sight_only)
{
    view_object_data_line lines[MAX_VIEW_LINES];

    int i, j;
    int col;
    int longest_name_length = 0;
    int longest_direction_length = 0;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;

    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_lines = MIN(MAX_VIEW_LINES, term_hgt - 3); /* Leave space for header and footer */

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    j = 0;
    for (i = 0; i < temp_n; i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        object_type* o_ptr = &o_list[o_idx];
        char o_name[80];
        int name_length;

        if (j >= max_lines)
            break;
        if (!player_can_see_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(lines[j].direction, '\0', sizeof(lines[j].direction));
        memset(lines[j].name, '\0', sizeof(lines[j].name));
        memset(o_name, '\0', sizeof(o_name));

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        append_look_smithing_debug(o_name, sizeof(o_name), o_ptr);

        write_direction_from_player_to_buffer(temp_y[i], temp_x[i],
            lines[j].direction, sizeof(lines[j].direction));
        lines[j].distance
            = distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);

        if (strlen(lines[j].direction) == 0)
            SDL_strlcpy(lines[j].direction, "underfoot", sizeof(lines[j].direction));

        longest_direction_length = MAX(longest_direction_length,
            (int)strlen(lines[j].direction));

        name_length = strlen(o_name);
        longest_name_length = MAX(longest_name_length, name_length);

        lines[j].object_character = object_char(o_ptr);
        lines[j].object_color = object_attr(o_ptr);
        lines[j].name_color = look_object_name_color(o_ptr);

        SDL_strlcpy(lines[j].name, o_name, sizeof(lines[j].name));

        j++;
    }

    if (!j)
    {
        int empty_col = MAX(0, (term_wid - 19) / 2);
        Term_erase(0, 1, 255);
        Term_erase(0, 2, 255);
        Term_erase(0, 3, 255);
        Term_putstr(empty_col, 1, term_wid - empty_col, TERM_WHITE,
            "No visible objects.");
        return;
    }

    col = term_wid - longest_name_length - longest_direction_length - 9;
    col = MAX(0, col);

    Term_erase(col, 1, term_wid - col);

    for (i = 0; i < j; ++i)
    {
        int distance_color;
        int direction_col = col + 6;
        int name_col = direction_col + MAX(longest_direction_length, 1) + 1;
        int name_width = term_wid - name_col - 1;

        char o_char[2];

        o_char[0] = lines[i].object_character;
        o_char[1] = '\0';

        if (lines[i].distance < 5)
            distance_color = TERM_WHITE;
        else if (lines[i].distance < 10)
            distance_color = TERM_L_WHITE;
        else
            distance_color = TERM_L_DARK;

        /* Clear the line */
        Term_erase(col, i + 1, term_wid - col);

        if (name_width < 1)
            name_width = 1;

        c_put_str(lines[i].object_color, o_char, i + 1, col + 2);
        if (use_bigtile)
        {
            Term_putch(col + 3, i + 1, 255, -1);
        }
        Term_putstr(direction_col, i + 1, MAX(longest_direction_length, 1),
            distance_color, lines[i].direction);
        Term_putstr(name_col, i + 1, name_width, lines[i].name_color,
            lines[i].name);
    }

    if (j)
    {
        Term_erase(col, j + 1, term_wid - col);
    }
}

void do_cmd_view_monsters()
{
    char get_char = '[';
    bool show_los = true;

    /* Clear entry level banner when using [ command */
    if (dismiss_active_narrative_banner())
    {
        do_cmd_redraw();
    }

    while (get_char == '[')
    {
        screen_save();
        show_nearby_monsters(show_los);
        /* Show the prompt */
        if (show_los)
            prt("Monsters you can see (press [ to toggle):", 0, 0);
        else
            prt("Monsters on screen (press [ to toggle):", 0, 0);
        get_char = inkey();
        show_los = !show_los;
        screen_load();
    }
}

void do_cmd_view_objects()
{
    char get_char = ']';
    bool show_los = true;

    /* Clear entry level banner when using ] command */
    if (dismiss_active_narrative_banner())
    {
        do_cmd_redraw();
    }

    while (get_char == ']')
    {
        screen_save();
        show_nearby_objects(show_los);
        /* Show the prompt */
        if (show_los)
            prt("Objects you can see (press ] to toggle):", 0, 0);
        else
            prt("Objects on screen (press ] to toggle):", 0, 0);
        get_char = inkey();
        show_los = !show_los;
        screen_load();
    }
}

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

typedef struct unified_sidebar_sorted_object {
    int o_idx;
    int y, x;
    object_type* o_ptr;
    bool is_artifact;
    int difficulty;
    int level;
    int group;
    int distance;
    int original_index;
} unified_sidebar_sorted_object;

static bool unified_sidebar_object_should_swap(
    const unified_sidebar_sorted_object* a,
    const unified_sidebar_sorted_object* b)
{
    bool sort_by_difficulty_only = look_objects_sort_by_difficulty ? true : false;
    bool a_known = object_known_p(a->o_ptr) ? true : false;
    bool b_known = object_known_p(b->o_ptr) ? true : false;

    if (!sort_by_difficulty_only && a->group != b->group)
        return (b->group < a->group);

    /* Unidentified items stay at the top of the section/list. */
    if (a_known != b_known)
        return (!b_known && a_known);

    if (!a_known)
    {
        if (b->distance < a->distance)
            return true;
        if ((b->distance == a->distance) && (b->original_index < a->original_index))
            return true;
        return false;
    }

    if (b->difficulty > a->difficulty)
        return true;
    if ((b->difficulty == a->difficulty) && (b->distance < a->distance))
        return true;
    if ((b->difficulty == a->difficulty) && (b->distance == a->distance)
        && (b->original_index < a->original_index))
        return true;

    return false;
}

static bool unified_look_sidebar_in_radius(const unified_look_state* state, int y,
    int x)
{
    if (!state || !state->nearby_filter)
        return true;

    return distance(p_ptr->py, p_ptr->px, y, x) <= UNIFIED_LOOK_NEAR_RADIUS;
}

static int unified_sidebar_collect_sorted_objects(const unified_look_state* state,
    unified_sidebar_sorted_object objects[], int max_objects)
{
    int i;
    int valid_objects = 0;

    if (!state || !objects || (max_objects <= 0))
        return 0;

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    for (i = 0; (i < temp_n) && (valid_objects < max_objects); i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        object_type* o_ptr;
        unified_sidebar_sorted_object* entry;

        if (!o_idx)
            continue;

        if (!grid_info_is_available(temp_y[i], temp_x[i]))
            continue;
        if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i]))
            continue;

        o_ptr = &o_list[o_idx];

        if (!o_ptr->k_idx)
            continue;

        /* Only show marked (memorized) objects that the player has actually seen. */
        if (!o_ptr->marked)
            continue;

        if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
            continue;

        entry = &objects[valid_objects];
        entry->o_idx = o_idx;
        entry->y = temp_y[i];
        entry->x = temp_x[i];
        entry->o_ptr = o_ptr;
        entry->is_artifact = artefact_p(o_ptr) ? true : false;
        entry->difficulty = object_difficulty(o_ptr);
        entry->level = k_info[o_ptr->k_idx].level;
        entry->group = unified_sidebar_object_group(o_ptr);
        if ((state->object_group_filter >= 0)
            && (entry->group != state->object_group_filter))
            continue;
        entry->distance = distance(p_ptr->py, p_ptr->px, entry->y, entry->x);
        entry->original_index = i;

        valid_objects++;
    }

    for (i = 0; i < valid_objects - 1; i++) {
        for (int j = i + 1; j < valid_objects; j++) {
            if (unified_sidebar_object_should_swap(&objects[i], &objects[j]))
            {
                unified_sidebar_sorted_object temp = objects[i];
                objects[i] = objects[j];
                objects[j] = temp;
            }
        }
    }

    return valid_objects;
}

int unified_look_find_cursor_selection(const unified_look_state* state, int cursor_y,
    int cursor_x)
{
    int i;
    int entity_index = 0;

    if (!state)
        return -1;

    if (state->show_monsters)
    {
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);

        for (i = 0; i < temp_n; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];

            if (!m_idx)
                continue;
            if (!grid_info_is_available(temp_y[i], temp_x[i]))
                continue;
            if (!mon_list[m_idx].ml)
                continue;
            if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i]))
                continue;

            if ((temp_y[i] == cursor_y) && (temp_x[i] == cursor_x))
                return entity_index;

            entity_index++;
        }
    }

    if (state->show_objects)
    {
        int group_display_counts[LOOK_GROUP_COUNT] = {0};
        get_sorted_target_list(TARGET_LIST_OBJECT, 0);
        int object_capacity = (temp_n > 0) ? temp_n : 1;
        unified_sidebar_sorted_object objects[object_capacity];
        int valid_objects = unified_sidebar_collect_sorted_objects(state, objects,
            object_capacity);

        for (i = 0; i < valid_objects; i++)
        {
            unified_sidebar_sorted_object* entry = &objects[i];

            if (state->limit_objects_top_five
                && (group_display_counts[entry->group] >= 5))
                continue;

            group_display_counts[entry->group]++;

            if ((entry->y == cursor_y) && (entry->x == cursor_x))
                return entity_index;

            entity_index++;
        }
    }

    return -1;
}

void redraw_inven_equip_subwindows(void)
{
    for (int j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        if (!angband_term[j])
            continue;

        /* Don't overwrite the current options/menu term. */
        if (angband_term[j] == old)
            continue;

        u32b flags = op_ptr->window_flag[j];
        if (!(flags & (PW_INVEN | PW_EQUIP | PW_SUPPLY)))
            continue;

        Term_activate(angband_term[j]);

        if (flags & PW_INVEN)
            display_inven();
        if (flags & PW_EQUIP)
            display_equip();
        if (flags & PW_SUPPLY)
            display_supplies();

        Term_fresh();
        Term_activate(old);
    }
}

void redraw_monster_subwindows(void)
{
    for (int j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        if (!angband_term[j])
            continue;

        /* Don't overwrite the current options/menu term. */
        if (angband_term[j] == old)
            continue;

        u32b flags = op_ptr->window_flag[j];
        if (!(flags & (PW_MONSTER)))
            continue;

        Term_activate(angband_term[j]);

        if (p_ptr->monster_race_idx)
            display_roff(p_ptr->monster_race_idx, NULL);

        Term_fresh();
        Term_activate(old);
    }
}

static void sidebar_trim_spaces(char* s)
{
    if (!s) return;

    char* start = s;
    while (*start && isspace((unsigned char)*start))
        ++start;

    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

static int sidebar_find_stats_pos(const char* s)
{
    if (!s) return -1;

    /* Stats section typically appears after the item name, preceded by a space.
     * Format: "Item Name (dice) [bonus] <pval> {inscription}"
     * We search for the first space-delimited bracket that looks like stats.
     */

    int first_stats_pos = -1;

    /* Look for the first bracket that follows a space or starts the string */
    for (int i = 0; s[i]; ++i)
    {
        char c = s[i];

        /* Found a potential stats delimiter */
        if (c == '(' || c == '[' || c == '<' || c == '{')
        {
            /* Check if this is preceded by a space (or is at start) */
            if (i == 0 || s[i-1] == ' ')
            {
                /* This looks like the start of stats section */
                first_stats_pos = i;
                break;
            }
            /* If preceded by a letter/digit, it might be part of the name */
            /* Keep searching */
        }
    }

    /* If we found stats position at start (i==0), that means NO base name!
     * This shouldn't happen with properly formatted object_desc output.
     * If it does, we should treat the whole thing as base name, not stats.
     */
    if (first_stats_pos == 0)
    {
        log_trace("sidebar_find_stats_pos: stats at position 0 for '%s' - treating as name", s);
        return -1;
    }

    return first_stats_pos;
}

static void sidebar_compact_name(const char* src, int max_len, char* dest, size_t dest_sz)
{
    int src_len;
    int src_width;
    int stats_pos;

    if (!dest_sz)
        return;

    dest[0] = 0;

    if (!src || max_len < 1)
        return;

    src_len = (int)strlen(src);
    src_width = utf8_display_width_n(src, src_len);
    if (src_width <= max_len)
    {
        SDL_strlcpy(dest, src, dest_sz);
        return;
    }

    stats_pos = sidebar_find_stats_pos(src);
    if (stats_pos < 0)
    {
        int copy_len = settings_utf8_prefix_len(src, max_len);

        if (copy_len >= (int)dest_sz)
            copy_len = (int)dest_sz - 1;
        if (copy_len < 0)
            copy_len = 0;

        SDL_memcpy(dest, src, (size_t)copy_len);
        dest[copy_len] = '\0';
        sidebar_trim_spaces(dest);
        return;
    }

    {
        char base_full[128];
        char stats_text[128];
        int base_copy_len;
        int base_width;
        int stats_width;
        int base_budget;

        base_copy_len = stats_pos;
        if (base_copy_len >= (int)sizeof(base_full))
            base_copy_len = (int)sizeof(base_full) - 1;
        base_copy_len = utf8_safe_prefix_len(src, base_copy_len);
        if (base_copy_len < 0)
            base_copy_len = 0;

        SDL_memcpy(base_full, src, (size_t)base_copy_len);
        base_full[base_copy_len] = '\0';
        sidebar_trim_spaces(base_full);

        SDL_strlcpy(stats_text, src + stats_pos, sizeof(stats_text));
        sidebar_trim_spaces(stats_text);

        base_width = utf8_display_width_n(base_full, (int)strlen(base_full));
        stats_width = utf8_display_width_n(stats_text, (int)strlen(stats_text));

        if (max_len <= 6)
        {
            int copy_len = settings_utf8_prefix_len(src, max_len);

            if (copy_len >= (int)dest_sz)
                copy_len = (int)dest_sz - 1;
            if (copy_len < 0)
                copy_len = 0;

            SDL_memcpy(dest, src, (size_t)copy_len);
            dest[copy_len] = '\0';
            sidebar_trim_spaces(dest);
            return;
        }

        if (stats_width >= max_len)
        {
            int base_share = MAX(3, max_len / 2);
            int stats_share = max_len - base_share;
            char base_buf[64];
            char stats_buf[64];
            int copy_len;

            if (stats_share < 3)
                stats_share = 3;
            if (base_share > max_len - 3)
                base_share = max_len - 3;

            copy_len = settings_utf8_prefix_len(base_full, base_share);
            if (copy_len >= (int)sizeof(base_buf))
                copy_len = (int)sizeof(base_buf) - 1;
            if (copy_len < 0)
                copy_len = 0;
            SDL_memcpy(base_buf, base_full, (size_t)copy_len);
            base_buf[copy_len] = '\0';
            sidebar_trim_spaces(base_buf);

            copy_len = settings_utf8_prefix_len(stats_text, stats_share);
            if (copy_len >= (int)sizeof(stats_buf))
                copy_len = (int)sizeof(stats_buf) - 1;
            if (copy_len < 0)
                copy_len = 0;
            SDL_memcpy(stats_buf, stats_text, (size_t)copy_len);
            stats_buf[copy_len] = '\0';
            sidebar_trim_spaces(stats_buf);

            if (base_buf[0] && stats_buf[0])
                strnfmt(dest, dest_sz, "%s %s", base_buf, stats_buf);
            else if (base_buf[0])
                SDL_strlcpy(dest, base_buf, dest_sz);
            else
                SDL_strlcpy(dest, stats_buf, dest_sz);

            sidebar_trim_spaces(dest);
            return;
        }

        base_budget = max_len - stats_width;
        if (base_budget < 0)
            base_budget = 0;

        if (base_full[0] && base_width > 0)
        {
            int copy_len = base_width <= base_budget
                ? (int)strlen(base_full)
                : settings_utf8_prefix_len(base_full, base_budget);

            if (copy_len >= (int)dest_sz)
                copy_len = (int)dest_sz - 1;
            if (copy_len < 0)
                copy_len = 0;

            SDL_memcpy(dest, base_full, (size_t)copy_len);
            dest[copy_len] = '\0';
            sidebar_trim_spaces(dest);

            if (dest[0] && dest[strlen(dest) - 1] != ' ')
                SDL_strlcat(dest, " ", dest_sz);
        }

        SDL_strlcat(dest, stats_text, dest_sz);
        sidebar_trim_spaces(dest);
    }
}

typedef struct unified_sidebar_compact_entry
{
    int entity_index;
    int entity_type;
    int y;
    int x;
    byte symbol_attr;
    byte text_attr;
    char symbol[2];
    char text[128];
} unified_sidebar_compact_entry;

static bool unified_sidebar_use_compact_layout(void)
{
    return Term && (Term->wid <= 60);
}

static int unified_sidebar_compact_last_row(void)
{
    if (!Term || Term->hgt <= 1)
        return -1;

    return Term->hgt - 2;
}

static void unified_sidebar_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    settings_ui_fit_text(buf, buflen, text, max_chars);
}

static int unified_sidebar_story_text_cols(cptr text)
{
    int cell_w;
    int pixel_w;
    int len;

    if (!text || !text[0] || !sdl_is_story_font_enabled())
        return 0;

    cell_w = sdl_get_cell_width();
    if (cell_w <= 0)
        return 0;

    len = (int)strlen(text);
    pixel_w = sdl_story_font_text_width(text, len);
    if (pixel_w <= 0)
        return 0;

    return (pixel_w + cell_w - 1) / cell_w;
}

static int unified_sidebar_text_hit_width(int pictogram_col, int text_col,
    cptr text, int fallback_width)
{
    int story_cols = unified_sidebar_story_text_cols(text);
    int hit_width = fallback_width;

    if (story_cols > 0)
        hit_width = MAX(hit_width, text_col - pictogram_col + story_cols);

    return MAX(hit_width, 1);
}

static int unified_sidebar_text_pair_hit_width(int pictogram_col, int text_col,
    cptr first, cptr second, int fallback_width)
{
    int first_cols = unified_sidebar_story_text_cols(first);
    int second_cols = unified_sidebar_story_text_cols(second);
    int hit_width = fallback_width;

    if (first_cols > 0 || second_cols > 0)
        hit_width = MAX(hit_width, text_col - pictogram_col + first_cols
            + second_cols);

    return MAX(hit_width, 1);
}

static int unified_sidebar_row_width(int width)
{
    if (!Term || Term->wid <= 0)
        return MAX(width, 1);

    width = MAX(width, 1);

    if (use_bigtile && width < Term->wid && ((width - COL_MAP) & 1))
        width++;

    return MIN(width, Term->wid);
}

static void unified_sidebar_clear_row(int row, int width)
{
    if (!Term || row < 0 || row >= Term->hgt)
        return;

    Term_erase(0, row, unified_sidebar_row_width(width));
}

static int unified_sidebar_compact_build_entries(
    const unified_look_state* state,
    unified_sidebar_compact_entry* entries,
    int max_entries)
{
    int entry_count = 0;
    int entity_index = 0;
    int text_col = use_bigtile ? 3 : 2;
    int text_width = Term ? (Term->wid - text_col - 1) : 40;

    if (!state || !entries || max_entries <= 0)
        return 0;

    if (text_width < 8)
        text_width = 8;

    if (state->show_monsters)
    {
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);

        for (int i = 0; i < temp_n && entry_count < max_entries; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
            monster_type* m_ptr;
            monster_race* r_ptr;
            unified_sidebar_compact_entry* entry;
            char m_name[40];
            char name_buf[80];
            char hp_bar[10];
            char suffix[24];
            int morale_color = TERM_WHITE;
            int morale_num = 0;
            int name_budget;

            if (!m_idx)
                continue;
            if (!grid_info_is_available(temp_y[i], temp_x[i]))
                continue;

            m_ptr = &mon_list[m_idx];
            if (!m_ptr->ml)
                continue;
            if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i]))
                continue;

            r_ptr = &r_info[m_ptr->r_idx];
            monster_desc_race(m_name, sizeof(m_name), m_ptr->r_idx);

            monster_health_bar_text(m_ptr, hp_bar, sizeof(hp_bar), 8);

            if (m_ptr->alertness < ALERTNESS_UNWARY)
            {
                morale_color = TERM_BLUE;
                morale_num = m_ptr->alertness;
            }
            else if (m_ptr->alertness < ALERTNESS_ALERT)
            {
                morale_color = TERM_L_BLUE;
                morale_num = m_ptr->alertness;
            }
            else
            {
                char dummy_text[20];
                if (!get_alertness_text(m_ptr, sizeof(dummy_text), dummy_text,
                        &morale_color))
                {
                    morale_color = TERM_WHITE;
                }

                morale_num = (m_ptr->morale >= 0)
                    ? ((m_ptr->morale + 9) / 10)
                    : (m_ptr->morale / 10);
            }

            strnfmt(suffix, sizeof(suffix), " %s %d", hp_bar, morale_num);
            name_budget = text_width - (int)strlen(suffix);
            if (name_budget < 4)
                name_budget = 4;
            unified_sidebar_fit_text(name_buf, sizeof(name_buf), m_name,
                name_budget);

            entry = &entries[entry_count++];
            entry->entity_index = entity_index++;
            entry->entity_type = 1;
            entry->y = temp_y[i];
            entry->x = temp_x[i];
            entry->symbol_attr = monster_attr(r_ptr);
            entry->text_attr = TERM_WHITE;
            entry->symbol[0] = monster_char(r_ptr);
            entry->symbol[1] = '\0';
            strnfmt(entry->text, sizeof(entry->text), "%s%s", name_buf, suffix);
        }
    }

    if (state->show_objects)
    {
        int group_display_counts[LOOK_GROUP_COUNT] = {0};
        int object_capacity;
        unified_sidebar_sorted_object* objects;
        int valid_objects;

        get_sorted_target_list(TARGET_LIST_OBJECT, 0);
        object_capacity = (temp_n > 0) ? temp_n : 1;
        objects = mem_alloc_array(object_capacity, unified_sidebar_sorted_object);
        valid_objects = unified_sidebar_collect_sorted_objects(state, objects,
            object_capacity);

        for (int i = 0; i < valid_objects && entry_count < max_entries; i++)
        {
            unified_sidebar_sorted_object* sorted = &objects[i];
            object_type* o_ptr = sorted->o_ptr;
            unified_sidebar_compact_entry* entry;
            char o_name[60];
            char name_source[80];
            char name_buf[128];
            char suffix[40];
            char weight_buf[16];
            char smith_buf[16];
            int weight_total;
            int name_budget;
            byte base_color;

            if (state->limit_objects_top_five
                && group_display_counts[sorted->group] >= 5)
            {
                continue;
            }

            group_display_counts[sorted->group]++;

            object_desc_floor(o_name, sizeof(o_name), o_ptr, false, 4);
            SDL_strlcpy(name_source, o_name, sizeof(name_source));
            if (sorted->is_artifact && object_known_p(o_ptr))
            {
                size_t len = strlen(name_source);
                if (len + 1 < sizeof(name_source))
                {
                    memmove(name_source + 1, name_source, len + 1);
                    name_source[0] = '*';
                }
            }

            weight_total = o_ptr->weight * o_ptr->number;
            strnfmt(weight_buf, sizeof(weight_buf), " %d.%1d",
                weight_total / 10, weight_total % 10);

            smith_buf[0] = '\0';
            if (op_ptr->opt[OPT_show_smithing_difficulty_look]
                && object_known_p(o_ptr)
                && object_uses_smithing_difficulty(o_ptr))
            {
                int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
                int sd = object_smithing_difficulty(o_ptr);
                int wr = object_weight_rarity(o_ptr, depth);
                strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
            }

            strnfmt(suffix, sizeof(suffix), "%s%s", weight_buf, smith_buf);
            name_budget = text_width - (int)strlen(suffix);
            if (name_budget < 4)
                name_budget = 4;
            sidebar_compact_name(name_source, name_budget, name_buf,
                sizeof(name_buf));

            base_color = weapon_glows(o_ptr)
                ? object_display_color(o_ptr, TERM_L_BLUE)
                : object_display_color(o_ptr,
                    tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

            entry = &entries[entry_count++];
            entry->entity_index = entity_index++;
            entry->entity_type = 2;
            entry->y = sorted->y;
            entry->x = sorted->x;
            entry->symbol_attr = object_attr(o_ptr);
            entry->text_attr = base_color;
            entry->symbol[0] = object_char(o_ptr);
            entry->symbol[1] = '\0';
            strnfmt(entry->text, sizeof(entry->text), "%s%s", name_buf, suffix);
        }

        objects = mem_free(objects);
    }

    return entry_count;
}

static bool show_unified_sidebar_compact(unified_look_state* state)
{
    int first_row;
    int last_row;
    int rows;
    int max_entries;
    int entry_count;
    int top = 0;
    int pictogram_col = 0;
    int text_col = use_bigtile ? 3 : 2;
    bool has_sidebar_selection;
    unified_sidebar_compact_entry* entries;

    if (!unified_sidebar_use_compact_layout())
        return false;

    if ((state->look_mode == 0) && !state->in_sidebar_mode
        && (state->selected_entity < 0)
        && ((state->cursor_y != p_ptr->py) || (state->cursor_x != p_ptr->px)))
    {
        (void)Term_set_extra_cursor(false, 0, 0, false);

        if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
            highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
                false);

        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
    }

    first_row = 1;
    last_row = unified_sidebar_compact_last_row();
    rows = last_row - first_row + 1;
    if (rows <= 0)
    {
        (void)Term_set_extra_cursor(false, 0, 0, false);
        ui_menu_click_clear();
        return true;
    }

    (void)Term_set_extra_cursor(false, 0, 0, false);

    Term_erase(0, 0, 255);

    max_entries = MAX(1, mon_max + o_max);
    entries = mem_alloc_array(max_entries, unified_sidebar_compact_entry);
    entry_count = unified_sidebar_compact_build_entries(state, entries,
        max_entries);
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);

    has_sidebar_selection = (state->selected_entity >= 0)
        && (state->in_sidebar_mode || (state->look_mode == 0));

    if (has_sidebar_selection)
    {
        top = state->selected_entity - rows / 2;
        if (top < 0)
            top = 0;
        if (top + rows > entry_count)
            top = entry_count - rows;
        if (top < 0)
            top = 0;
    }

    for (int i = 0; i < rows && top + i < entry_count; i++)
    {
        unified_sidebar_compact_entry* entry = &entries[top + i];
        int row = first_row + i;
        bool highlight_this = has_sidebar_selection
            && (state->selected_entity == entry->entity_index);
        byte text_attr = highlight_this ? TERM_L_BLUE : entry->text_attr;
        int text_width = utf8_display_width_n(entry->text,
            (int)strlen(entry->text));

        int hit_width = unified_sidebar_text_hit_width(pictogram_col, text_col,
            entry->text, MAX(1, text_col + text_width - pictogram_col));
        if (Term)
            hit_width = MAX(hit_width, Term->wid - pictogram_col);
        ui_menu_click_add(entry->entity_index, pictogram_col, row, hit_width);

        c_put_str(entry->symbol_attr, entry->symbol, row, pictogram_col);
        if (use_bigtile)
            Term_putch(pictogram_col + 1, row, 255, -1);

        Term_putstr(text_col, row, -1, text_attr, entry->text);

        if (highlight_this)
        {
            (void)Term_set_extra_cursor(true, pictogram_col, row, use_bigtile);
            state->highlighted_y = entry->y;
            state->highlighted_x = entry->x;
            state->highlighted_entity_type = entry->entity_type;
            state->cursor_y = entry->y;
            state->cursor_x = entry->x;
            highlight_entity_on_map_type(entry->y, entry->x, true,
                entry->entity_type);
        }
    }

    entries = mem_free(entries);
    return true;
}

static cptr unified_sidebar_object_filter_tag(int group_filter)
{
    switch (group_filter)
    {
    case LOOK_GROUP_ARTIFACT:   return "ART";
    case LOOK_GROUP_WEAPON:     return "WEAP";
    case LOOK_GROUP_ARMOUR:     return "ARM";
    case LOOK_GROUP_JEWELRY:    return "JEWL";
    case LOOK_GROUP_HERBS:      return "HERB";
    case LOOK_GROUP_POTIONS:    return "POT";
    case LOOK_GROUP_GEMS:       return "GEM";
    case LOOK_GROUP_CONSUMABLE: return "CONS";
    case LOOK_GROUP_OTHER:      return "OTHER";
    default:                    return "ALL";
    }
}

static bool show_unified_sidebar_pixel(unified_look_state* state)
{
    unified_sidebar_compact_entry* entries;
    int max_entries;
    int entry_count;
    bool has_sidebar_selection;

    if (!state)
    {
        sdl_unified_look_sidebar_clear();
        return true;
    }

    if ((state->look_mode == 0) && !state->in_sidebar_mode
        && (state->selected_entity < 0)
        && ((state->cursor_y != p_ptr->py) || (state->cursor_x != p_ptr->px)))
    {
        (void)Term_set_extra_cursor(false, 0, 0, false);

        if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
            highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
                false);

        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
    }

    (void)Term_set_extra_cursor(false, 0, 0, false);

    max_entries = MAX(1, mon_max + o_max);
    entries = mem_alloc_array(max_entries, unified_sidebar_compact_entry);
    entry_count = unified_sidebar_compact_build_entries(state, entries,
        max_entries);

    has_sidebar_selection = (state->selected_entity >= 0)
        && (state->in_sidebar_mode || (state->look_mode == 0));
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);

    sdl_unified_look_sidebar_begin(unified_sidebar_use_compact_layout(),
        has_sidebar_selection, state->selected_entity);

    if (state->show_monsters)
    {
        sdl_unified_look_sidebar_add_header("MONSTERS:");
        for (int i = 0; i < entry_count; i++)
        {
            unified_sidebar_compact_entry* entry = &entries[i];

            if (entry->entity_type != 1)
                continue;
            sdl_unified_look_sidebar_add_entry(entry->entity_index,
                entry->entity_type, entry->y, entry->x, entry->symbol_attr,
                entry->text_attr, entry->symbol, entry->text);
        }
    }

    if (state->show_objects)
    {
        char header_buf[32];

        strnfmt(header_buf, sizeof(header_buf), "OBJECTS: %s",
            unified_sidebar_object_filter_tag(state->object_group_filter));
        sdl_unified_look_sidebar_add_header(header_buf);
        for (int i = 0; i < entry_count; i++)
        {
            unified_sidebar_compact_entry* entry = &entries[i];

            if (entry->entity_type != 2)
                continue;
            sdl_unified_look_sidebar_add_entry(entry->entity_index,
                entry->entity_type, entry->y, entry->x, entry->symbol_attr,
                entry->text_attr, entry->symbol, entry->text);
        }
    }

    if (has_sidebar_selection)
    {
        for (int i = 0; i < entry_count; i++)
        {
            unified_sidebar_compact_entry* entry = &entries[i];

            if (state->selected_entity != entry->entity_index)
                continue;

            state->highlighted_y = entry->y;
            state->highlighted_x = entry->x;
            state->highlighted_entity_type = entry->entity_type;
            state->cursor_y = entry->y;
            state->cursor_x = entry->x;
            highlight_entity_on_map_type(entry->y, entry->x, true,
                entry->entity_type);
            break;
        }
    }

    sdl_unified_look_sidebar_finish();
    entries = mem_free(entries);
    return true;
}

/*
 * Show unified sidebar with monsters and objects
 */
void show_unified_sidebar(unified_look_state* state)
{
    if (show_unified_sidebar_pixel(state))
        return;

    if (show_unified_sidebar_compact(state))
        return;

    int sidebar_col = 0; /* Left side of screen - column 0 */
    int line = 1;
    int i;
    int monster_count = 0;
    int object_count = 0;
    char entity_char[2];
    entity_char[1] = '\0';
    static int previous_line_count = 0; /* Track previous display size */
    static int prev_name_len[256];
    const int prev_array_capacity = (int)(sizeof(prev_name_len) / sizeof(prev_name_len[0]));
    bool has_sidebar_selection;


    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_display_line = term_hgt - 1;
    if (max_display_line < 1)
        max_display_line = 1;

    /* Calculate layout positions once for both monsters and objects */
    int term_wid = Term->wid;
    int available_width = term_wid - sidebar_col - 3;
    int name_width = available_width - 8 - 3 - 2; /* -2 for spaces */

    /* Adjust for bigtile mode - pictogram takes extra space */
    if (use_bigtile) {
        name_width = name_width - 1;  /* Reduce name width by 1 for bigtile */
    }

    if (name_width < 4) name_width = 4; /* minimum name width */

    /* Calculate exact positions */
    int pictogram_col = sidebar_col;
    int name_col = sidebar_col + 2;  /* Name starts right after pictogram (at column 2) */
    int sidebar_hit_width;

    if (COL_MAP > pictogram_col)
        sidebar_hit_width = COL_MAP - pictogram_col;
    else
        sidebar_hit_width = 1;

    if (sidebar_hit_width < 1)
        sidebar_hit_width = 1;

    log_trace("show_unified_sidebar: previous_line_count=%d, term_hgt=%d, max_display_line=%d",
              previous_line_count, term_hgt, max_display_line);
    log_trace("show_unified_sidebar: sidebar_col=%d, Term->wid=%d",
              sidebar_col, Term->wid);
    log_trace("show_unified_sidebar: show_monsters=%d, show_objects=%d",
              state->show_monsters ? 1 : 0, state->show_objects ? 1 : 0);

    (void)Term_set_extra_cursor(false, 0, 0, false);

    if (COL_MAP > 0)
    {
        for (int row = 0; row < term_hgt; row++)
            Term_erase(0, row, COL_MAP);
    }

    if ((state->look_mode == 0) && !state->in_sidebar_mode
        && (state->selected_entity < 0)
        && ((state->cursor_y != p_ptr->py) || (state->cursor_x != p_ptr->px)))
    {
        if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
        {
            highlight_entity_on_map(state->highlighted_y, state->highlighted_x, false);
        }

        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
    }

    has_sidebar_selection = (state->selected_entity >= 0)
        && (state->in_sidebar_mode || (state->look_mode == 0));
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);

    /* Full overlay restoration comes from screen_save/screen_load; rows are
     * cleared only to their dynamic rendered width as they are drawn. */

    /* Show monsters section */
    if (state->show_monsters)
    {
        log_trace("show_unified_sidebar: displaying MONSTERS header at line %d", line);
        unified_sidebar_clear_row(line, unified_sidebar_text_hit_width(
            sidebar_col, sidebar_col, "MONSTERS:    ", 13));
        c_put_str(TERM_WHITE, "MONSTERS:    ", line++, sidebar_col);

        /* Get monster list */
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);

        for (i = 0; i < temp_n && line < max_display_line; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
            monster_type* m_ptr = &mon_list[m_idx];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            char m_name[40];
            char morale_text[8];

            /* Show only visible monsters on screen (like the [ monsters menu) */
            /* Skip empty monster slots */
            if (!m_idx) continue;

            if (!grid_info_is_available(temp_y[i], temp_x[i])) continue;

            /* Skip monsters that are not visible to the player */
            if (!m_ptr->ml) continue;
            if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i])) continue;

            /* Generate monster name without articles using race name function */
            monster_desc_race(m_name, sizeof(m_name), m_ptr->r_idx);

            /* Create compact HP bar */
            char hp_bar[10];
            monster_health_bar_text(m_ptr, hp_bar, sizeof(hp_bar), 8);

            /* Create morale number with proper color */
            int morale_color = TERM_WHITE;
            int morale_num = 0;

            if (m_ptr->alertness < ALERTNESS_UNWARY)
            {
                morale_color = TERM_BLUE;
                morale_num = m_ptr->alertness;
            }
            else if (m_ptr->alertness < ALERTNESS_ALERT)
            {
                morale_color = TERM_L_BLUE;
                morale_num = m_ptr->alertness;
            }
            else
            {
                /* Get proper morale display using alertness function */
                char dummy_text[20];
                if (!get_alertness_text(m_ptr, sizeof(dummy_text), dummy_text, &morale_color))
                {
                    /* Fallback if stance not initialized - use white and calculate from morale */
                    morale_color = TERM_WHITE;
                }

                /* Calculate morale number */
                if (m_ptr->morale >= 0)
                    morale_num = (m_ptr->morale + 9) / 10;
                else
                    morale_num = m_ptr->morale / 10;
            }

            strnfmt(morale_text, sizeof(morale_text), "%d", morale_num);

            /* Use pictogram (tile) appropriate for graphics mode */
            entity_char[0] = monster_char(r_ptr);

            /* Build the complete display string: name + health + morale */
            char display_name[128];
            char hp_display[12];
            char morale_display[12];

            /* Format health and morale as compact strings */
            strnfmt(hp_display, sizeof(hp_display), " %s", hp_bar);
            strnfmt(morale_display, sizeof(morale_display), " %s", morale_text);

            /* Calculate available width for the whole line */
            int available_width = term_wid - name_col - 2;
            if (available_width < 10) available_width = 10;

            int hp_display_len = (int)strlen(hp_display);
            int morale_display_len = (int)strlen(morale_display);
            int hp_display_width = utf8_display_width_n(hp_display,
                hp_display_len);
            int morale_display_width = utf8_display_width_n(morale_display,
                morale_display_len);
            int max_name_len = available_width - hp_display_width
                - morale_display_width;
            if (max_name_len < 4) max_name_len = 4;
            if (max_name_len > (int)sizeof(display_name) - hp_display_len
                - morale_display_len - 1)
            {
                max_name_len = (int)sizeof(display_name) - hp_display_len
                    - morale_display_len - 1;
            }

            /* Truncate monster name if needed */
            char truncated_name[80];
            int truncated_name_len = settings_utf8_prefix_len(m_name,
                max_name_len);
            if (truncated_name_len >= (int)sizeof(truncated_name))
                truncated_name_len = (int)sizeof(truncated_name) - 1;
            if (truncated_name_len < 0)
                truncated_name_len = 0;
            SDL_memcpy(truncated_name, m_name, (size_t)truncated_name_len);
            truncated_name[truncated_name_len] = '\0';

            /* Build complete display string: name + health (without morale) */
            SDL_strlcpy(display_name, truncated_name, sizeof(display_name));
            SDL_strlcat(display_name, hp_display, sizeof(display_name));

            int name_hp_len = (int)strlen(display_name);
            int name_hp_width = utf8_display_width_n(display_name,
                name_hp_len);
            int total_span = name_hp_width + morale_display_width;
            if (use_bigtile)
            {
                const int min_sidebar_span = 13;
                if (total_span < min_sidebar_span)
                {
                    int pad_needed = min_sidebar_span - total_span;

                    while (pad_needed > 0
                        && name_hp_len + 1 < (int)sizeof(display_name))
                    {
                        display_name[name_hp_len++] = ' ';
                        name_hp_width++;
                        pad_needed--;
                    }
                    display_name[name_hp_len] = '\0';
                    total_span = name_hp_width + morale_display_width;

                    while (pad_needed > 0
                        && morale_display_len + 1 < (int)sizeof(morale_display))
                    {
                        morale_display[morale_display_len++] = ' ';
                        morale_display_width++;
                        pad_needed--;
                    }
                    morale_display[morale_display_len] = '\0';
                    total_span = name_hp_width + morale_display_width;
                }

                if ((total_span % 2) == 0)
                {
                    if (morale_display_len + 1 < (int)sizeof(morale_display))
                    {
                        morale_display[morale_display_len++] = ' ';
                        morale_display[morale_display_len] = '\0';
                        morale_display_width++;
                    }
                    else if (name_hp_len + 1 < (int)sizeof(display_name))
                    {
                        display_name[name_hp_len++] = ' ';
                        display_name[name_hp_len] = '\0';
                        name_hp_width++;
                    }
                    total_span = name_hp_width + morale_display_width;
                }
            }

            /* Calculate column for morale display */
            int morale_col = name_col + name_hp_width;
            int monster_hit_width = unified_sidebar_text_pair_hit_width(
                pictogram_col, name_col, display_name, morale_display,
                MAX(sidebar_hit_width, name_col - pictogram_col + total_span));
            unified_sidebar_clear_row(line, monster_hit_width);
            ui_menu_click_add(monster_count, pictogram_col, line,
                monster_hit_width);

            /* Highlight if selected with cursor-style highlighting only */
            bool highlight_this_monster = (has_sidebar_selection
                && (state->selected_entity == monster_count));

            if (highlight_this_monster)
            {
                log_trace("Highlighting monster %d at (%d,%d)", monster_count, temp_y[i], temp_x[i]);

                /* Clear only the exact area where text will be displayed */
                Term_erase(pictogram_col, line, 2);  /* Clear pictogram area (1-2 chars) */

                /* Show pictogram in natural color */
                c_put_str(monster_attr(r_ptr), entity_char, line, pictogram_col);
                if (use_bigtile)
                {
                    Term_putch(pictogram_col + 1, line, 255, -1);
                }

                Term_putstr(name_col, line, -1, TERM_L_BLUE,
                    display_name);
                Term_putstr(morale_col, line, -1, TERM_L_BLUE,
                    morale_display);
                (void)Term_set_extra_cursor(true, pictogram_col, line, use_bigtile);

                /* Update highlighted position and cursor */
                state->highlighted_y = temp_y[i];
                state->highlighted_x = temp_x[i];
                state->highlighted_entity_type = 1; /* Monster */
                state->cursor_y = temp_y[i];
                state->cursor_x = temp_x[i];
                highlight_entity_on_map_type(temp_y[i], temp_x[i], true, 1); /* Prefer monster display */
            }
            else
            {
                /* Clear only the exact area where text will be displayed */
                Term_erase(pictogram_col, line, 2);  /* Clear pictogram area (1-2 chars) */

                /* Normal display - show pictogram and name with proper colors */
                c_put_str(monster_attr(r_ptr), entity_char, line, pictogram_col);
                if (use_bigtile)
                {
                    Term_putch(pictogram_col + 1, line, 255, -1);
                }

                /* Display name+health in white */
                Term_putstr(name_col, line, -1, TERM_WHITE, display_name);

                /* Display morale in its proper color */
                Term_putstr(morale_col, line, -1, morale_color, morale_display);
            }

            line++;
            monster_count++;
        }
    }

    /* Show objects section */
    if (state->show_objects)
    {
        const char* filter_tag = "ALL";
        switch (state->object_group_filter)
        {
        case LOOK_GROUP_ARTIFACT:   filter_tag = "ART"; break;
        case LOOK_GROUP_WEAPON:     filter_tag = "WEAP"; break;
        case LOOK_GROUP_ARMOUR:     filter_tag = "ARM"; break;
        case LOOK_GROUP_JEWELRY:    filter_tag = "JEWL"; break;
        case LOOK_GROUP_HERBS:      filter_tag = "HERB"; break;
        case LOOK_GROUP_POTIONS:    filter_tag = "POT"; break;
        case LOOK_GROUP_GEMS:       filter_tag = "GEM"; break;
        case LOOK_GROUP_CONSUMABLE: filter_tag = "CONS"; break;
        case LOOK_GROUP_OTHER:      filter_tag = "OTHER"; break;
        default:                    filter_tag = "ALL"; break;
        }

        char header_buf[32];
        strnfmt(header_buf, sizeof(header_buf), "OBJECTS: %s", filter_tag);
        unified_sidebar_clear_row(line, unified_sidebar_text_hit_width(
            sidebar_col, sidebar_col, header_buf, (int)strlen(header_buf)));
        c_put_str(TERM_WHITE, header_buf, line++, sidebar_col);

        get_sorted_target_list(TARGET_LIST_OBJECT, 0);
        int object_capacity = (temp_n > 0) ? temp_n : 1;
        unified_sidebar_sorted_object objects[object_capacity];
        int valid_objects = unified_sidebar_collect_sorted_objects(state, objects,
            object_capacity);

        int group_display_counts[LOOK_GROUP_COUNT] = {0};
        int object_start = (state->show_monsters) ? monster_count : 0;
        for (i = 0; i < valid_objects && line < max_display_line; i++)
        {
            unified_sidebar_sorted_object* entry = &objects[i];
            object_type* o_ptr = entry->o_ptr;
            char o_name[60];
            char name_source[80];
            byte base_color = TERM_WHITE;

            if (state->limit_objects_top_five && group_display_counts[entry->group] >= 5)
                continue;

            group_display_counts[entry->group]++;

            /* Generate object name with stats but without articles (mode 4)
             * Mode 4 applies shortening logic that sidebar_compact_name expects.
             * Fixed mode 4 to never produce stats-only output.
             */
            object_desc_floor(o_name, sizeof(o_name), o_ptr, false, 4);

            SDL_strlcpy(name_source, o_name, sizeof(name_source));
            /* Only show asterisk for artifacts that are identified */
            if (entry->is_artifact && object_known_p(o_ptr))
            {
                size_t len = strlen(name_source);
                if (len + 1 < sizeof(name_source))
                {
                    memmove(name_source + 1, name_source, len + 1);
                    name_source[0] = '*';
                }
            }

            base_color = weapon_glows(o_ptr)
                ? object_display_color(o_ptr, TERM_L_BLUE)
                : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

            entity_char[0] = object_char(o_ptr);

            int weight_total = o_ptr->weight * o_ptr->number;
            char weight_buf[16];
            strnfmt(weight_buf, sizeof(weight_buf), " %d.%1d", weight_total / 10, weight_total % 10);

            char smith_buf[16];
            smith_buf[0] = '\0';
            if (op_ptr->opt[OPT_show_smithing_difficulty_look]
                && object_known_p(o_ptr)
                && object_uses_smithing_difficulty(o_ptr))
            {
                int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
                int sd = object_smithing_difficulty(o_ptr);
                int wr = object_weight_rarity(o_ptr, depth);
                strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
            }

            /* Calculate available width for name + weight (+ optional smithing debug) */
            int available_name_width = term_wid - name_col - 2; /* Leave some margin */
            if (available_name_width < 10) available_name_width = 10;

            int weight_len = (int)strlen(weight_buf);
            int smith_len = (int)strlen(smith_buf);
            int max_name_len = available_name_width - weight_len - smith_len - 1; /* Reserve space for suffixes */
            if (max_name_len < 4) max_name_len = 4;

            char display_name[128];
            if (max_name_len > (int)sizeof(display_name) - weight_len - 1)
                max_name_len = (int)sizeof(display_name) - weight_len - 1;

            sidebar_compact_name(name_source, max_name_len, display_name, sizeof(display_name));

            /* Append weight right after name */
            SDL_strlcat(display_name, weight_buf, sizeof(display_name));

            /* Append optional smithing debug right after weight */
            if (smith_buf[0])
                SDL_strlcat(display_name, smith_buf, sizeof(display_name));
            int final_name_len = (int)strlen(display_name);
            int final_name_width = utf8_display_width_n(display_name,
                final_name_len);
            int original_name_len = (int)strlen(name_source);
            int original_name_width = utf8_display_width_n(name_source,
                original_name_len);
            bool shortened = (original_name_width != final_name_width)
                || (original_name_width > max_name_len);
            log_trace("sidebar object: idx=%d name='%s' compact='%s' color=%d orig_len=%d compact_len=%d max_len=%d name_col=%d weight_len=%d shortened=%d",
                entry->o_idx, name_source, display_name, base_color,
                original_name_len, final_name_len, max_name_len, name_col,
                weight_len, shortened ? 1 : 0);

            if (use_bigtile)
            {
                const int min_sidebar_span = 13;
                if (final_name_width < min_sidebar_span)
                {
                    int pad_needed = min_sidebar_span - final_name_width;
                    while (pad_needed > 0 && final_name_len + 1 < (int)sizeof(display_name))
                    {
                        display_name[final_name_len++] = ' ';
                        final_name_width++;
                        pad_needed--;
                    }
                    display_name[final_name_len] = '\0';
                }

                if ((final_name_width % 2) == 0
                    && (final_name_len + 1 < (int)sizeof(display_name)))
                {
                    display_name[final_name_len++] = ' ';
                    display_name[final_name_len] = '\0';
                    final_name_width++;
                }
            }

            int row_index = line;
            if (row_index < 0) row_index = 0;
            if (row_index >= prev_array_capacity) row_index = prev_array_capacity - 1;
            int object_hit_width = unified_sidebar_text_hit_width(
                pictogram_col, name_col, display_name,
                MAX(sidebar_hit_width, name_col - pictogram_col
                    + final_name_width));
            unified_sidebar_clear_row(line, object_hit_width);
            ui_menu_click_add(object_start + object_count, pictogram_col, line,
                object_hit_width);

            int old_name_len = prev_name_len[row_index];
            if (old_name_len > final_name_width)
            {
                int diff = old_name_len - final_name_width;
                if (diff > 0)
                {
                    char blank[128];
                    if (diff >= (int)sizeof(blank)) diff = (int)sizeof(blank) - 1;
                    memset(blank, ' ', diff);
                    blank[diff] = '\0';
                    Term_putstr(name_col + final_name_width, line, diff,
                        TERM_WHITE, blank);
                }
            }

            bool highlight_this_object = (has_sidebar_selection
                && (state->selected_entity == (object_start + object_count)));

            byte name_attr = highlight_this_object ? TERM_L_BLUE : base_color;

            if (highlight_this_object)
            {
                log_trace("Highlighting object %d at (%d,%d)", object_start + object_count, entry->y, entry->x);

                Term_erase(pictogram_col, line, 2);

                c_put_str(object_attr(o_ptr), entity_char, line, pictogram_col);
                if (use_bigtile)
                {
                    Term_putch(pictogram_col + 1, line, 255, -1);
                }

                Term_putstr(name_col, line, -1, name_attr, display_name);
                (void)Term_set_extra_cursor(true, pictogram_col, line, use_bigtile);

                state->highlighted_y = entry->y;
                state->highlighted_x = entry->x;
                state->highlighted_entity_type = 2; /* Object */
                state->cursor_y = entry->y;
                state->cursor_x = entry->x;
                highlight_entity_on_map_type(entry->y, entry->x, true, 2); /* Prefer object display */
            }
            else
            {
                Term_erase(pictogram_col, line, 2);

                c_put_str(object_attr(o_ptr), entity_char, line, pictogram_col);
                if (use_bigtile)
                {
                    Term_putch(pictogram_col + 1, line, 255, -1);
                }

                Term_putstr(name_col, line, -1, name_attr, display_name);
            }

            prev_name_len[row_index] = final_name_width;

            line++;
            object_count++;
        for (int idx = line; idx < prev_array_capacity && idx <= previous_line_count; ++idx)
        {
            prev_name_len[idx] = 0;
        }

        }
    }

    /* Save current line count for next clearing operation */
    int current_line_count = line - 1;

    log_trace("show_unified_sidebar: current_line_count=%d, previous_line_count=%d",
              current_line_count, previous_line_count);

    /* If the new display is shorter than the previous one, don't clear - let screen_load handle it */
    if (previous_line_count > current_line_count)
    {
        log_trace("show_unified_sidebar: display got shorter (%d->%d) but not clearing - screen_load will restore",
                  previous_line_count, current_line_count);
    }

    previous_line_count = current_line_count;
    log_trace("show_unified_sidebar: function complete, set previous_line_count=%d", previous_line_count);
}
