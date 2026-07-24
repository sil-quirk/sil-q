#include "angband.h"
#include "sdl-config.h"
#include "sound-config.h"
#include "sdl-sound.h"

extern struct sound_config g_sound_config;
extern int sdl_log_pane_current_rows(enum pane_type pane);
extern void sdl_log_pane_set_rows(enum pane_type pane, int rows);
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
#include "ui/question.h"
#include <SDL3/SDL_keyboard.h>

enum {
    SETTINGS_PREV_OPTION_PAGE = -1001,
    SETTINGS_NEXT_OPTION_PAGE = -1002
};

static int settings_option_page_turn = 0;

static void do_cmd_movement_keybinds(void);

static int settings_menu_key(int key, int prev_page_key, int next_page_key,
    bool click_generated)
{
    if (click_generated)
        return key;

    return steamdeck_menu_key(key, prev_page_key, next_page_key);
}

void clear_skills_and_abilities()
{
    int i, j;

    /* Clear the base values of the skills */
    for (i = 0; i < A_MAX; i++)
        p_ptr->skill_base[i] = 0;

    /* Clear the abilities */
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = false;
            p_ptr->active_ability[i][j] = false;
        }
    }

    ability_log_reset();

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Set the redraw flag for everything */
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

/*
 * Interact with some options
 */
enum {
    SOUND_OPT_ENABLED = 0,
    SOUND_OPT_COMBAT_ENABLED,
    SOUND_OPT_MONSTER_HITS_ENABLED,
    SOUND_OPT_INVENTORY_ENABLED,
    SOUND_OPT_WALK_ENABLED,
    SOUND_OPT_DOORS_ENABLED,
    SOUND_OPT_TRAPS_ENABLED,
    SOUND_OPT_COMBAT_VOLUME,
    SOUND_OPT_MONSTER_HITS_VOLUME,
    SOUND_OPT_INVENTORY_VOLUME,
    SOUND_OPT_WALK_VOLUME,
    SOUND_OPT_DOORS_VOLUME,
    SOUND_OPT_TRAPS_VOLUME,
    SOUND_OPT_OTHER_VOLUME,
    SOUND_OPT_MUSIC_MAIN_ENABLED,
    SOUND_OPT_MUSIC_AMBIENT_ENABLED,
    SOUND_OPT_MUSIC_MAIN_VOLUME,
    SOUND_OPT_MUSIC_AMBIENT_VOLUME,
    SOUND_OPT_MAX
};

struct option_group_marker
{
    int setting_id;
    cptr label;
};

struct settings_value_choice
{
    int value;
    cptr label;
};

#define SOUND_OPTION_ROW_BASE 2048
#define SOUND_OPTION_ROW(index) (SOUND_OPTION_ROW_BASE + (index))
#define IFACE_PANE_ROW_BASE 4096
#define IFACE_PANE_ROW_MAX 40
#define IFACE_DICE_ROLL_MS_STEP 500
#define IFACE_OVERLAY_LOG_ROWS_MIN 1
#define IFACE_OVERLAY_LOG_ROWS_MAX 20

static const struct settings_value_choice running_delay_choices[] = {
    { 0, "0 ms" },
    { 5, "5 ms" },
    { 10, "10 ms" },
    { DEFAULT_RUNNING_DELAY_MS, "17 ms" },
    { 25, "25 ms" },
    { 33, "33 ms" },
    { 50, "50 ms" },
    { 75, "75 ms" },
    { 100, "100 ms" }
};

static byte running_delay_step(byte current, int delta)
{
    if (delta < 0)
    {
        for (int i = (int)N_ELEMENTS(running_delay_choices) - 1; i >= 0; i--)
            if (running_delay_choices[i].value < current)
                return (byte)running_delay_choices[i].value;
        return (byte)running_delay_choices[0].value;
    }

    for (int i = 0; i < (int)N_ELEMENTS(running_delay_choices); i++)
        if (running_delay_choices[i].value > current)
            return (byte)running_delay_choices[i].value;

    return (byte)running_delay_choices[
        N_ELEMENTS(running_delay_choices) - 1].value;
}

static const struct option_group_marker interface_option_groups[] = {
    { OPT_look_objects_sort_by_difficulty, "Look" },
    { OPT_look_nearby_filter_default, "Look" },
    { OPT_song_list_sort_by_recent, "Look" },
    { OPT_styled_player_health_bar, "Health Bars" },
    { OPT_styled_monster_health_bars, "Health Bars" },
    { OPT_styled_monster_tile_health_bars, "Health Bars" },
    { OPT_hide_supporting_panes_fullscreen, "Panels" },
    { OPT_hitpoint_warning, "Warnings" },
    { OPT_supply_menu_random_icons, "Items" },
    { OPT_supply_menu_hide_flavor_compact, "Items" },
    { OPT_hide_secondary_action_ring, "Quick Access" },
    { OPT_show_level_generation_debug, "Debug" },
    { OPT_show_elemental_item_rolls, "Debug" },
    { -1, NULL }
};

static const struct option_group_marker text_option_groups[] = {
    { OPT_story_object_desc, "Overlays" },
    { OPT_story_monster_desc, "Overlays" },
    { OPT_story_monster_desc_pane, "Panes" },
    { OPT_story_lists_inven_pane, "Panes" },
    { OPT_story_lists_equip_pane, "Panes" },
    { -1, NULL }
};

static const struct option_group_marker gameplay_option_groups[] = {
    { OPT_valorous_oath_auto_attack_safety, "Combat Behavior" },
    { OPT_pacifist_attack_warning, "Combat Behavior" },
    { OPT_active_weapon_switch_confirm, "Combat Behavior" },
    { OPT_forgo_attacking_unwary, "Combat Behavior" },
    { OPT_assassination_over_charge, "Combat Behavior" },
    { OPT_lockpick_minigame, "Interaction" },
    { OPT_chest_trap_minigame, "Interaction" },
    { OPT_stop_singing_on_rest, "Rest and Song" },
    { OPT_visual_recognition, "Information" },
    { OPT_know_monster_info, "Information" },
    { OPT_disable_skeleton_note_tutorial, "Tutorial" },
    { OPT_smaller_level_size, "World Generation" },
    { OPT_more_stairs, "World Generation" },
    { OPT_vault_drop_frequency, "World Generation" },
    { OPT_noble_item_spawn_mode, "World Generation" },
    { OPT_min_depth_timer_mode, "World Generation" },
    { OPT_load_blitz_by_default, "Blitz" },
    { -1, NULL }
};

static const struct option_group_marker visual_option_groups[] = {
    { OPT_stealth_vision, "Overlays" },
    { OPT_sleep_icon, "Overlays" },
    { OPT_pixel_monster_status_icons, "Overlays" },
    { OPT_artifact_unique_color, "Items" },
    { OPT_unidentified_items_slate, "Items" },
    { OPT_delay_factor, "Animation" },
    { OPT_running_delay, "Animation" },
    { OPT_mirror_player_tile_facing, "Animation" },
    { OPT_mirror_monster_tile_facing, "Animation" },
    { OPT_center_player, "Camera" },
    { OPT_run_avoid_center, "Camera" },
    { OPT_show_level_entry_banner, "Narrative" },
    { OPT_show_partition_narrative, "Narrative" },
    { OPT_narrative_banner_turns, "Narrative" },
    { OPT_intro_style, "Narrative" },
    { OPT_solid_walls, "ASCII" },
    { OPT_hybrid_walls, "ASCII" },
    { OPT_hilite_player, "Cursor" },
    { OPT_hilite_target, "Cursor" },
    { OPT_hilite_unwary, "Cursor" },
    { OPT_show_smithing_difficulty, "Debug" },
    { OPT_show_smithing_difficulty_look, "Debug" },
    { -1, NULL }
};

static const struct option_group_marker challenge_option_groups[] = {
    { OPT_birth_discon_stair, "Traversal" },
    { OPT_birth_ironman, "Traversal" },
    { OPT_birth_no_artefacts, "Content" },
    { OPT_birth_fixed_exp, "Content" },
    { -1, NULL }
};

static const struct option_group_marker debug_option_groups[] = {
    { OPT_cheat_peek, "Generation" },
    { OPT_cheat_hear, "Generation" },
    { OPT_cheat_room, "Generation" },
    { OPT_cheat_xtra, "Generation" },
    { OPT_cheat_know, "Knowledge" },
    { OPT_cheat_monsters, "Knowledge" },
    { OPT_cheat_noise, "Knowledge" },
    { OPT_cheat_scent, "Knowledge" },
    { OPT_cheat_light, "Knowledge" },
    { OPT_cheat_skill_rolls, "Knowledge" },
    { OPT_cheat_live, "Survival" },
    { OPT_cheat_timestop, "Survival" },
    { -1, NULL }
};

static const struct option_group_marker sound_option_groups[] = {
    { SOUND_OPTION_ROW(SOUND_OPT_ENABLED), "Master" },
    { SOUND_OPTION_ROW(SOUND_OPT_COMBAT_ENABLED), "Effects" },
    { SOUND_OPTION_ROW(SOUND_OPT_MONSTER_HITS_ENABLED), "Effects" },
    { SOUND_OPTION_ROW(SOUND_OPT_INVENTORY_ENABLED), "Effects" },
    { SOUND_OPTION_ROW(SOUND_OPT_WALK_ENABLED), "Effects" },
    { SOUND_OPTION_ROW(SOUND_OPT_DOORS_ENABLED), "Effects" },
    { SOUND_OPTION_ROW(SOUND_OPT_TRAPS_ENABLED), "Effects" },
    { SOUND_OPTION_ROW(SOUND_OPT_COMBAT_VOLUME), "Effect Volume" },
    { SOUND_OPTION_ROW(SOUND_OPT_MONSTER_HITS_VOLUME), "Effect Volume" },
    { SOUND_OPTION_ROW(SOUND_OPT_INVENTORY_VOLUME), "Effect Volume" },
    { SOUND_OPTION_ROW(SOUND_OPT_WALK_VOLUME), "Effect Volume" },
    { SOUND_OPTION_ROW(SOUND_OPT_DOORS_VOLUME), "Effect Volume" },
    { SOUND_OPTION_ROW(SOUND_OPT_TRAPS_VOLUME), "Effect Volume" },
    { SOUND_OPTION_ROW(SOUND_OPT_OTHER_VOLUME), "Effect Volume" },
    { SOUND_OPTION_ROW(SOUND_OPT_MUSIC_MAIN_ENABLED), "Music" },
    { SOUND_OPTION_ROW(SOUND_OPT_MUSIC_AMBIENT_ENABLED), "Music" },
    { SOUND_OPTION_ROW(SOUND_OPT_MUSIC_MAIN_VOLUME), "Music Volume" },
    { SOUND_OPTION_ROW(SOUND_OPT_MUSIC_AMBIENT_VOLUME), "Music Volume" },
    { -1, NULL }
};

static const struct option_group_marker* get_option_groups_for_page(int page)
{
    switch (page)
    {
    case INTERFACE_PAGE: return interface_option_groups;
    case TEXT_PAGE: return text_option_groups;
    case GAMEPLAY_PAGE: return gameplay_option_groups;
    case VISUAL_PAGE: return visual_option_groups;
    case CHALLENGE_PAGE: return challenge_option_groups;
    case DEBUG_PAGE: return debug_option_groups;
    case SOUND_PAGE: return sound_option_groups;
    default: return NULL;
    }
}

static cptr option_group_for_setting(const struct option_group_marker* groups,
    int setting_id)
{
    if (!groups)
        return NULL;

    for (int i = 0; groups[i].setting_id >= 0; i++)
        if (groups[i].setting_id == setting_id)
            return groups[i].label;

    return NULL;
}

static bool option_group_starts_at(const struct option_group_marker* groups,
    const int* settings, int setting_index)
{
    cptr current;
    cptr previous;

    if (!groups || !settings || setting_index < 0)
        return false;

    current = option_group_for_setting(groups, settings[setting_index]);
    if (!current)
        return false;
    if (setting_index == 0)
        return true;

    previous = option_group_for_setting(groups, settings[setting_index - 1]);
    return !previous || !streq(current, previous);
}

static bool option_page_uses_app_config(int page)
{
    return (page == INTERFACE_PAGE) || (page == TEXT_PAGE)
        || (page == VISUAL_PAGE);
}

static int settings_ui_term_wid(void)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    return wid;
}

static int settings_ui_line_width(int col)
{
    int width = settings_ui_term_wid() - col;

    if (width < 1)
        width = 1;

    return width;
}

int settings_utf8_prefix_len(cptr text, int max_cols)
{
    int bytes = 0;
    int cols = 0;

    if (!text || max_cols <= 0)
        return 0;

    while (text[bytes])
    {
        int char_len = utf8_sequence_len(text + bytes);
        int char_width;

        if (char_len <= 0)
            break;

        char_width = utf8_display_width_n(text + bytes, char_len);
        if (char_width > 0 && cols + char_width > max_cols)
            break;

        cols += char_width;
        bytes += char_len;
    }

    return bytes;
}

void settings_ui_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    if (!buflen)
        return;

    if (!text)
        text = "";

    if (max_chars <= 0)
    {
        buf[0] = '\0';
        return;
    }

    if (utf8_display_width_n(text, (int)strlen(text)) <= max_chars)
    {
        SDL_strlcpy(buf, text, buflen);
    }
    else if (max_chars <= 3)
    {
        int copy_len = settings_utf8_prefix_len(text, max_chars);
        if (copy_len >= (int)buflen)
            copy_len = utf8_safe_prefix_len(text, (int)buflen - 1);
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
    }
    else
    {
        int copy_len = settings_utf8_prefix_len(text, max_chars - 3);
        if (copy_len >= (int)buflen)
            copy_len = utf8_safe_prefix_len(text, (int)buflen - 1);
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
        SDL_strlcat(buf, "...", buflen);
    }
}

static cptr settings_ui_pick_label(int max_chars, cptr long_label,
    cptr medium_label, cptr short_label)
{
    cptr labels[3] = { long_label, medium_label, short_label };

    for (int i = 0; i < 3; i++)
    {
        if (labels[i] && labels[i][0]
            && utf8_display_width_n(labels[i], (int)strlen(labels[i])) <= max_chars)
            return labels[i];
    }

    if (short_label && short_label[0])
        return short_label;
    if (medium_label && medium_label[0])
        return medium_label;
    if (long_label && long_label[0])
        return long_label;

    return "";
}

static void settings_menu_begin_scroll_area(int list_start_row, int visible_rows)
{
    ui_scroll_area_begin(list_start_row, list_start_row + visible_rows - 1,
        SDL_TOUCH_MENU_CATEGORY_OTHER);
    ui_scroll_area_set_keys('8', '2', '6', '4');
}

static void settings_ui_format_pair_line(char* buf, size_t buflen, cptr label,
    cptr value, int max_chars, int min_value_chars)
{
    char label_buf[128];
    char value_buf[96];
    int desired_value;
    int value_budget;
    int label_budget;

    if (!buflen)
        return;

    if (!label)
        label = "";
    if (!value)
        value = "";

    if (max_chars <= 0)
    {
        buf[0] = '\0';
        return;
    }

    if (!value[0])
    {
        settings_ui_fit_text(buf, buflen, label, max_chars);
        return;
    }

    desired_value = utf8_display_width_n(value, (int)strlen(value));
    value_budget = MIN(max_chars - 4,
        MAX(min_value_chars, MIN(desired_value, (max_chars * 3) / 5)));

    if (value_budget < 1)
        value_budget = MIN(max_chars, MAX(1, max_chars / 2));

    settings_ui_fit_text(value_buf, sizeof(value_buf), value, value_budget);
    label_budget = max_chars
        - utf8_display_width_n(value_buf, (int)strlen(value_buf)) - 2;

    if (label_budget < 4)
    {
        settings_ui_fit_text(buf, buflen, value, max_chars);
        return;
    }

    settings_ui_fit_text(label_buf, sizeof(label_buf), label, label_budget);
    strnfmt(buf, buflen, "%s: %s", label_buf, value_buf);
}

static void settings_ui_put_fitted(int row, int col, byte attr, cptr text)
{
    char buf[160];
    int width = settings_ui_line_width(col);

    settings_ui_fit_text(buf, sizeof(buf), text, width);
    Term_putstr(col, row, width, attr, buf);
}

#define SETTINGS_CLICK_RETURN 32000
#define SETTINGS_CLICK_SWITCH_GROUP 32001
#define SETTINGS_CLICK_RESET_SELECTED 32002
#define SETTINGS_CLICK_RESET_ALL 32003
#define SETTINGS_CLICK_SAVE 32004
#define SETTINGS_CLICK_RENAME_SELECTED 32005
#define SETTINGS_CLICK_RENAME_GROUP 32006
#define SETTINGS_CLICK_QUICK_ACCESS_SIZE 32007
#define SETTINGS_CLICK_QUICK_ACCESS_ROW 32008
#define SETTINGS_CLICK_QUICK_ACCESS_ADD_CELL 32009
#define SETTINGS_CLICK_PANE_FIELD_BASE 32100
/*
 * Per-row "Reset to default" buttons in the pixel/semantic settings menus.
 * A row's reset button reports SETTINGS_CLICK_RESET_ROW_BASE + row_index so a
 * mouse or touch user can reset a single setting without a keyboard key.
 */
#define SETTINGS_CLICK_RESET_ROW_BASE 32200

static void settings_ui_format_field(char* buf, size_t buflen, cptr text,
    bool selected)
{
    if (!buf || !buflen)
        return;

    if (!text)
        text = "";

    if (selected)
        strnfmt(buf, buflen, "[%s]", text);
    else
        SDL_strlcpy(buf, text, buflen);
}

static void settings_ui_format_auto_value(char* buf, size_t buflen, int value,
    int max_chars)
{
    char raw_buf[16];
    char auto_long[16];
    char auto_short[8];

    if (!buf || !buflen)
        return;

    if (value > 0)
    {
        strnfmt(raw_buf, sizeof(raw_buf), "%d", value);
        settings_ui_fit_text(buf, buflen, raw_buf, max_chars);
        return;
    }

    SDL_strlcpy(auto_long, "auto", sizeof(auto_long));
    SDL_strlcpy(auto_short, "a", sizeof(auto_short));
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, auto_long, auto_long, auto_short),
        max_chars);
}

static bool option_menu_use_compact_layout(void)
{
    return Term && (Term->wid > 0) && (Term->wid <= 60);
}

static bool option_menu_use_narrow_layout(void)
{
    return Term && (Term->wid > 0) && (Term->wid <= 50);
}

static int option_menu_max_line_chars(void)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    /* Options start at column 4; keep one cell free for the cursor. */
    wid -= 5;

    if (wid < 8)
        wid = 8;

    return wid;
}

static void option_menu_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    settings_ui_fit_text(buf, buflen, text, max_chars);
}

static cptr sound_option_label(int index)
{
    bool compact = option_menu_use_compact_layout();
    bool narrow = option_menu_use_narrow_layout();

    if (compact)
    {
        switch (index)
        {
        case SOUND_OPT_ENABLED: return narrow ? "Sounds" : "Game sounds";
        case SOUND_OPT_COMBAT_ENABLED: return narrow ? "Combat sfx" : "Combat sounds";
        case SOUND_OPT_MONSTER_HITS_ENABLED: return narrow ? "Mon hit sfx" : "Monster hit sounds";
        case SOUND_OPT_INVENTORY_ENABLED: return narrow ? "Inv sfx" : "Inventory sounds";
        case SOUND_OPT_WALK_ENABLED: return narrow ? "Walk sfx" : "Walk sounds";
        case SOUND_OPT_DOORS_ENABLED: return narrow ? "Door sfx" : "Door sounds";
        case SOUND_OPT_TRAPS_ENABLED: return narrow ? "Trap sfx" : "Trap sounds";
        case SOUND_OPT_COMBAT_VOLUME: return narrow ? "Combat vol" : "Combat volume";
        case SOUND_OPT_MONSTER_HITS_VOLUME: return narrow ? "Mon hit vol" : "Monster hit volume";
        case SOUND_OPT_INVENTORY_VOLUME: return narrow ? "Inv vol" : "Inventory volume";
        case SOUND_OPT_WALK_VOLUME: return narrow ? "Walk vol" : "Walk volume";
        case SOUND_OPT_DOORS_VOLUME: return narrow ? "Door vol" : "Door volume";
        case SOUND_OPT_TRAPS_VOLUME: return narrow ? "Trap vol" : "Trap volume";
        case SOUND_OPT_OTHER_VOLUME: return narrow ? "Other vol" : "Other volume";
        case SOUND_OPT_MUSIC_MAIN_ENABLED: return "Menu music";
        case SOUND_OPT_MUSIC_AMBIENT_ENABLED: return "Ambient music";
        case SOUND_OPT_MUSIC_MAIN_VOLUME: return narrow ? "Menu vol" : "Menu music volume";
        case SOUND_OPT_MUSIC_AMBIENT_VOLUME: return narrow ? "Ambient vol" : "Ambient music volume";
        default: return "(unknown sound option)";
        }
    }

    switch (index)
    {
    case SOUND_OPT_ENABLED: return "Enable game sounds";
    case SOUND_OPT_COMBAT_ENABLED: return "Enable combat sounds";
    case SOUND_OPT_MONSTER_HITS_ENABLED: return "Enable monster hit sounds";
    case SOUND_OPT_INVENTORY_ENABLED: return "Enable inventory sounds";
    case SOUND_OPT_WALK_ENABLED: return "Enable walk sounds";
    case SOUND_OPT_DOORS_ENABLED: return "Enable door sounds";
    case SOUND_OPT_TRAPS_ENABLED: return "Enable trap sounds";
    case SOUND_OPT_COMBAT_VOLUME: return "Combat sounds volume";
    case SOUND_OPT_MONSTER_HITS_VOLUME: return "Monster hit sounds volume";
    case SOUND_OPT_INVENTORY_VOLUME: return "Inventory sounds volume";
    case SOUND_OPT_WALK_VOLUME: return "Walk sounds volume";
    case SOUND_OPT_DOORS_VOLUME: return "Door sounds volume";
    case SOUND_OPT_TRAPS_VOLUME: return "Trap sounds volume";
    case SOUND_OPT_OTHER_VOLUME: return "Other sounds volume";
    case SOUND_OPT_MUSIC_MAIN_ENABLED: return "Enable main menu music";
    case SOUND_OPT_MUSIC_AMBIENT_ENABLED: return "Enable ambient dungeon music";
    case SOUND_OPT_MUSIC_MAIN_VOLUME: return "Main menu music volume";
    case SOUND_OPT_MUSIC_AMBIENT_VOLUME: return "Ambient music volume";
    default: return "(unknown sound option)";
    }
}

static cptr option_menu_label(int opt)
{
    bool compact = option_menu_use_compact_layout();
    bool narrow = option_menu_use_narrow_layout();

    switch (opt)
    {
    case OPT_delay_factor:
        return compact ? (narrow ? "Anim delay" : "Animation delay")
                       : "Delay factor for animation (0 to 9)";
    case OPT_running_delay:
        return compact ? (narrow ? "Run delay" : "Running delay")
                       : "Running delay";
    case OPT_hitpoint_warning:
        return compact ? (narrow ? "HP warn" : "HP warning")
                       : "Hitpoint warning threshold (0% to 90%)";
    case OPT_styled_player_health_bar:
        return compact ? (narrow ? "Styled HP bar" : "Styled health bar")
                       : "Use styled player health bar in the left panel";
    case OPT_styled_monster_health_bars:
        return compact ? (narrow ? "Monster HP bars" : "Monster health bars")
                       : "Use styled monster health bars in panes/overlays";
    case OPT_styled_monster_tile_health_bars:
        return compact ? (narrow ? "Tile HP bars" : "Monster tile HP bars")
                       : "Monster tile health bars";
    case OPT_pixel_monster_status_icons:
        return compact ? (narrow ? "Pixel status" : "Pixel monster status")
                       : "Use pixel-rendered monster status icons";
    case OPT_hide_supporting_panes_fullscreen:
        return compact ? (narrow ? "Hide panes FS" : "Hide panes full-screen")
                       : "Hide supporting panes on full-screen screens";
    case OPT_show_level_entry_banner:
        return compact ? (narrow ? "Entry text" : "Entry narrative")
                       : "Level entry narrative";
    case OPT_show_partition_narrative:
        return compact ? (narrow ? "Partition text" : "Partition narrative")
                       : "Partition transition narrative";
    case OPT_vault_drop_frequency:
        return compact ? "Vault drops" : "Vault drop frequency";
    case OPT_min_depth_timer_mode:
        return compact ? (narrow ? "Depth pace" : "Min depth pace")
                       : "Minimum depth pace";
    case OPT_noble_item_spawn_mode:
        return compact ? (narrow ? "Noble items" : "Noble item sources")
                       : "Noble item spawns";
    case OPT_look_objects_sort_by_difficulty:
        return compact ? (narrow ? "Look diff sort" : "Look sort by diff")
                       : "Sort look (L) objects by difficulty only";
    case OPT_look_nearby_filter_default:
        return compact ? (narrow ? "Look near def" : "Look nearby default")
                       : "Default look (l) nearby filter";
    case OPT_song_list_sort_by_recent:
        return compact ? (narrow ? "Songs recent" : "Recent songs first")
                       : "Sort song menu by recent use";
    case OPT_supply_menu_random_icons:
        return compact ? (narrow ? "Supply icons" : "Supply icon mode")
                       : "Supply group icon mode";
    case OPT_supply_menu_hide_flavor_compact:
        return compact ? (narrow ? "Hide flavors" : "Hide supply flavors")
                       : "Hide supply flavors in compact mode";
    case OPT_hide_secondary_action_ring:
        return compact ? (narrow ? "Hide 2nd ring" : "Hide 2nd wheel ring")
                       : "Hide the action wheel's secondary ring until hovered";
    case OPT_intro_style:
        return compact ? (narrow ? "Welcome art" : "Welcome screen")
                       : "Welcome screen style";
    case OPT_narrative_banner_turns:
        return compact ? "Banner turns" : "Narrative banner turns";
    case OPT_load_blitz_by_default:
        return compact ? (narrow ? "Load Blitz" : "Load Blitz first")
                       : "Load Blitz by default";
    case OPT_show_level_generation_debug:
        return compact ? (narrow ? "Dbg lvl screen" : "Debug level screen")
                       : "Show detailed level-generation screen info and pause before play";
    case OPT_show_elemental_item_rolls:
        return compact ? (narrow ? "Dbg elem items" : "Debug elemental items")
                       : "Show elemental item break rolls and target probabilities";
    case OPT_show_smithing_difficulty:
        return compact ? (narrow ? "Smith dbg items" : "Debug smithing in items")
                       : "Show {sd,wr} in item descriptions";
    case OPT_show_smithing_difficulty_look:
        return compact ? (narrow ? "Smith dbg look" : "Debug smithing in look")
                       : "Show {sd,wr} in look sidebar and message";
    default:
        break;
    }

    if (compact)
    {
        switch (opt)
        {
        case OPT_hjkl_movement: return narrow ? "hjkl move" : "hjkl movement";
        case OPT_angband_keyset: return narrow ? "Angband keys" : "Angband keyset";
        case OPT_story_lists: return narrow ? "Story look" : "Story font: look/target";
        case OPT_story_lists_inven: return narrow ? "Story inv" : "Story font: inv menu";
        case OPT_story_lists_equip: return narrow ? "Story equip" : "Story font: equip menu";
        case OPT_story_character_sheet: return narrow ? "Story sheet" : "Story font: char sheet";
        case OPT_story_lists_inven_pane: return narrow ? "Story inv pane" : "Story font: inv pane";
        case OPT_story_lists_equip_pane: return narrow ? "Story eq pane" : "Story font: equip pane";
        case OPT_story_monster_desc: return narrow ? "Story mon overlay" : "Story font: monster overlay";
        case OPT_story_monster_desc_pane: return narrow ? "Story mon pane" : "Story font: monster pane";
        case OPT_story_object_desc: return narrow ? "Story obj overlay" : "Story font: object overlay";
        case OPT_valorous_oath_auto_attack_safety: return narrow ? "Valorous safety" : "Valorous oath safety";
        case OPT_pacifist_attack_warning: return narrow ? "Pacifist warn" : "Warn before attacks";
        case OPT_active_weapon_switch_confirm: return narrow ? "Weapon switch" : "Confirm weapon switch";
        case OPT_forgo_attacking_unwary: return narrow ? "Skip unwary hits" : "Forgo unwary attacks";
        case OPT_assassination_over_charge: return narrow ? "Stealth over charge" : "Assassination over Charge";
        case OPT_lockpick_minigame: return narrow ? "Door minigame" : "Locked-door minigame";
        case OPT_chest_trap_minigame: return narrow ? "Chest minigame" : "Chest trap minigame";
        case OPT_stop_singing_on_rest: return narrow ? "Stop song on rest" : "Stop singing on rest";
        case OPT_know_monster_info: return narrow ? "Know monsters" : "Know monster info";
        case OPT_visual_recognition: return narrow ? "Need light to spot" : "Need light to spot";
        case OPT_disable_skeleton_note_tutorial: return narrow ? "Hide skeleton tips" : "Hide skeleton tutorials";
        case OPT_smaller_level_size: return narrow ? "Smaller levels" : "Smaller level size";
        case OPT_more_stairs: return narrow ? "More stairs" : "Extra stairs";
        case OPT_running_delay: return narrow ? "Run delay" : "Running delay";
        case OPT_center_player: return narrow ? "Center map" : "Center map";
        case OPT_run_avoid_center: return narrow ? "No center on run" : "Avoid centering on run";
        case OPT_artifact_unique_color: return narrow ? "Yellow artefacts" : "Yellow unique artefacts";
        case OPT_hilite_player: return narrow ? "Cursor on player" : "Highlight player";
        case OPT_hilite_target: return narrow ? "Cursor on target" : "Highlight target";
        case OPT_hilite_unwary: return narrow ? "Mark unwary" : "Highlight unwary";
        case OPT_solid_walls: return narrow ? "Solid walls" : "Solid walls";
        case OPT_hybrid_walls: return narrow ? "Hybrid walls" : "Hybrid walls";
        case OPT_unidentified_items_slate: return narrow ? "Slate unknown items" : "Slate unidentified items";
        case OPT_stealth_vision: return narrow ? "Stealth vision" : "Stealth vision";
        case OPT_sleep_icon: return narrow ? "Sleep icon" : "Sleep icon";
        case OPT_mirror_player_tile_facing:
            return narrow ? "Direction anim" : "Directional character animation";
        case OPT_mirror_monster_tile_facing:
            return narrow ? "Monster facing" : "Monster tile facing";
        case OPT_look_nearby_filter_default: return narrow ? "Look near def" : "Look nearby default";
        case OPT_birth_discon_stair: return narrow ? "Disc. stairs" : "Disconnected stairs";
        case OPT_birth_ironman: return narrow ? "Straight down" : "Straight down";
        case OPT_birth_no_artefacts: return narrow ? "No artefacts" : "No artefacts";
        case OPT_birth_fixed_exp: return narrow ? "Fixed XP" : "Fixed experience";
        case OPT_cheat_peek: return narrow ? "Debug obj gen" : "Debug object gen";
        case OPT_cheat_hear: return narrow ? "Debug mon gen" : "Debug monster gen";
        case OPT_cheat_room: return narrow ? "Debug room gen" : "Debug dungeon gen";
        case OPT_cheat_xtra: return narrow ? "Debug extra" : "Debug extra";
        case OPT_cheat_know: return narrow ? "Debug know mons" : "Debug know monsters";
        case OPT_cheat_monsters: return narrow ? "Debug show mons" : "Debug show monsters";
        case OPT_cheat_noise: return narrow ? "Debug noise" : "Debug noise";
        case OPT_cheat_scent: return narrow ? "Debug scent" : "Debug scent";
        case OPT_cheat_light: return narrow ? "Debug light" : "Debug light";
        case OPT_cheat_skill_rolls: return narrow ? "Debug skill rolls" : "Debug skill rolls";
        case OPT_cheat_live: return narrow ? "Debug no death" : "Debug avoid death";
        case OPT_cheat_timestop: return narrow ? "Debug time stop" : "Debug time stop";
        default:
            break;
        }
    }

    if (option_desc[opt])
        return option_desc[opt];
    if (option_text[opt])
        return option_text[opt];
    return "(unknown option)";
}

static void option_menu_format_line(char* buf, size_t buflen, cptr label,
    cptr value)
{
    if (!option_menu_use_compact_layout())
    {
        strnfmt(buf, buflen, "%-48s: %s", label, value);
    }
    else
    {
        char label_buf[96];
        char value_buf[48];
        int max_chars = option_menu_max_line_chars();
        int value_len;
        int label_budget;

        option_menu_fit_text(value_buf, sizeof(value_buf), value, max_chars);
        value_len = (int)strlen(value_buf);

        if (value_len <= 0)
        {
            option_menu_fit_text(buf, buflen, label, max_chars);
            return;
        }

        label_budget = max_chars - value_len - 2;
        if (label_budget <= 0)
        {
            option_menu_fit_text(buf, buflen, value_buf, max_chars);
            return;
        }

        option_menu_fit_text(label_buf, sizeof(label_buf), label, label_budget);
        strnfmt(buf, buflen, "%s: %s", label_buf, value_buf);
    }
}

static void settings_semantic_menu_begin(cptr title, int selected_choice)
{
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);

    sdl_character_sheet_screen_begin_select(selected_choice, title ? title : "");
    sdl_character_sheet_screen_set_select_menu_style(true);
}

static void settings_semantic_menu_hide(void)
{
    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();
    ui_scroll_area_clear();
}

static void settings_semantic_add_row(int choice, cptr label, byte attr)
{
    sdl_character_sheet_screen_add_select_row(choice, label ? label : "",
        attr, "");
}

static void settings_semantic_add_pair_row(int choice, cptr label, cptr value,
    byte attr)
{
    char line[160];

    if (value && value[0])
        strnfmt(line, sizeof(line), "%s\t%s", label ? label : "", value);
    else
        SDL_strlcpy(line, label ? label : "", sizeof(line));

    settings_semantic_add_row(choice, line, attr);
}

static void settings_semantic_line_from_menu_line(char* out, size_t outsz,
    cptr line)
{
    const char* colon;
    const char* value;
    size_t label_len;
    char label[160];

    if (!out || !outsz)
        return;

    out[0] = '\0';
    if (!line)
        return;

    colon = strrchr(line, ':');
    if (!colon)
    {
        SDL_strlcpy(out, line, outsz);
        return;
    }

    label_len = (size_t)(colon - line);
    while (label_len > 0 && isspace((unsigned char)line[label_len - 1]))
        label_len--;
    if (label_len >= sizeof(label))
        label_len = sizeof(label) - 1;

    memcpy(label, line, label_len);
    label[label_len] = '\0';

    value = colon + 1;
    while (*value && isspace((unsigned char)*value))
        value++;

    strnfmt(out, outsz, "%s\t%s", label, value);
}

static void option_apply_side_effects(int opt)
{
    if (opt == OPT_story_lists_inven_pane || opt == OPT_story_lists_equip_pane)
        redraw_inven_equip_subwindows();
    if (opt == OPT_story_monster_desc_pane)
        redraw_monster_subwindows();
    if (opt == OPT_hide_supporting_panes_fullscreen)
        sdl_refresh_supporting_panes_layout();
    if (opt == OPT_styled_player_health_bar)
    {
        p_ptr->redraw |= PR_BASIC;
        sdl_left_panel_source_invalidate();
    }
    if (opt == OPT_styled_monster_health_bars)
    {
        p_ptr->redraw |= PR_HEALTHBAR;
        p_ptr->window |= (PW_MONSTER | PW_MONLIST);
        redraw_monster_subwindows();
        sdl_left_panel_source_invalidate();
    }
    if (opt == OPT_styled_monster_tile_health_bars)
        p_ptr->redraw |= PR_MAP;
    if (opt == OPT_stealth_vision || opt == OPT_visual_recognition
        || opt == OPT_sleep_icon || opt == OPT_mirror_player_tile_facing
        || opt == OPT_pixel_monster_status_icons
        || opt == OPT_handcrafted_player_tile_facing
        || opt == OPT_mirror_monster_tile_facing)
        p_ptr->redraw |= (PR_MAP);
}

static byte option_player_tile_facing_mode(void)
{
    if (!op_ptr || !op_ptr->opt[OPT_mirror_player_tile_facing])
        return PLAYER_TILE_FACING_OFF;

    return op_ptr->opt[OPT_handcrafted_player_tile_facing]
        ? PLAYER_TILE_FACING_HANDCRAFTED
        : PLAYER_TILE_FACING_MIRROR;
}

static cptr option_player_tile_facing_mode_label(void)
{
    switch (option_player_tile_facing_mode())
    {
    case PLAYER_TILE_FACING_HANDCRAFTED: return "handcrafted";
    case PLAYER_TILE_FACING_MIRROR:      return "mirror";
    default:                             return "off";
    }
}

static void option_set_player_tile_facing_mode(byte mode)
{
    if (!op_ptr)
        return;

    if (mode > PLAYER_TILE_FACING_MAX)
        mode = PLAYER_TILE_FACING_OFF;

    op_ptr->opt[OPT_mirror_player_tile_facing]
        = (mode != PLAYER_TILE_FACING_OFF);
    op_ptr->opt[OPT_handcrafted_player_tile_facing]
        = (mode == PLAYER_TILE_FACING_HANDCRAFTED);

    option_apply_side_effects(OPT_mirror_player_tile_facing);
}

static byte option_monster_tile_health_bar_mode(void)
{
    if (!op_ptr)
        return MONSTER_TILE_HEALTH_BARS_SHOW;

    if (op_ptr->monster_tile_health_bar_mode > MONSTER_TILE_HEALTH_BARS_MAX)
        return MONSTER_TILE_HEALTH_BARS_SHOW;

    return op_ptr->monster_tile_health_bar_mode;
}

static cptr option_monster_tile_health_bar_mode_label(void)
{
    switch (option_monster_tile_health_bar_mode())
    {
    case MONSTER_TILE_HEALTH_BARS_DAMAGED_ONLY: return "only damaged";
    case MONSTER_TILE_HEALTH_BARS_OFF:          return "off";
    default:                                    return "show";
    }
}

static void option_set_monster_tile_health_bar_mode(byte mode)
{
    if (!op_ptr)
        return;

    if (mode > MONSTER_TILE_HEALTH_BARS_MAX)
        mode = MONSTER_TILE_HEALTH_BARS_SHOW;

    op_ptr->monster_tile_health_bar_mode = mode;
    op_ptr->opt[OPT_styled_monster_tile_health_bars]
        = (mode != MONSTER_TILE_HEALTH_BARS_OFF);
    option_apply_side_effects(OPT_styled_monster_tile_health_bars);
}

#define SETTINGS_VALUE_PICKER_MAX 96

static bool settings_question_button_choice_present(
    const ui_question_button* buttons, int button_count, int choice)
{
    for (int i = 0; buttons && i < button_count; i++)
    {
        if (buttons[i].choice == choice)
            return true;
    }

    return false;
}

static bool settings_pick_value_ex_buttons(cptr title, cptr desc,
    const struct settings_value_choice* choices, int count, int current_value,
    int* out_value, bool letter_shortcuts, const ui_question_button* buttons,
    int button_count, int* out_button_choice)
{
    ui_question_option options[SETTINGS_VALUE_PICKER_MAX];
    char labels[SETTINGS_VALUE_PICKER_MAX][96];
    int default_index = 0;
    int choice;

    if (!choices || count <= 0 || !out_value)
        return false;
    if (out_button_choice)
        *out_button_choice = 0;

    if (count > SETTINGS_VALUE_PICKER_MAX)
        count = SETTINGS_VALUE_PICKER_MAX;

    for (int i = 0; i < count; i++)
    {
        if (choices[i].value == current_value)
            default_index = i;

        SDL_strlcpy(labels[i], choices[i].label ? choices[i].label : "",
            sizeof(labels[i]));
        options[i].key = (letter_shortcuts && count <= 26 && i < 26)
            ? (char)I2A(i) : 0;
        options[i].label = labels[i];
        options[i].attr = (choices[i].value == current_value)
            ? TERM_L_BLUE : TERM_WHITE;
        options[i].disabled = false;
    }

    choice = ui_question_ask_overlay_buttons(title ? title : "Choose Value",
        desc, options, count, buttons, button_count, UI_QUESTION_GLOBAL,
        UI_QUESTION_GLOBAL, default_index);

    if (choice < 0 || choice >= count)
    {
        if (out_button_choice
            && settings_question_button_choice_present(buttons, button_count,
                choice))
        {
            *out_button_choice = choice;
            return true;
        }
        return false;
    }

    *out_value = choices[choice].value;
    return true;
}

static bool settings_pick_value_ex(cptr title, cptr desc,
    const struct settings_value_choice* choices, int count, int current_value,
    int* out_value, bool letter_shortcuts)
{
    return settings_pick_value_ex_buttons(title, desc, choices, count,
        current_value, out_value, letter_shortcuts, NULL, 0, NULL);
}

static bool settings_pick_value(cptr title, cptr desc,
    const struct settings_value_choice* choices, int count, int current_value,
    int* out_value)
{
    return settings_pick_value_ex(title, desc, choices, count, current_value,
        out_value, true);
}

static bool settings_pick_integer_range(cptr title, cptr desc, int min_value,
    int max_value, int current_value, bool show_sign, int* out_value)
{
    struct settings_value_choice choices[SETTINGS_VALUE_PICKER_MAX];
    char labels[SETTINGS_VALUE_PICKER_MAX][24];
    int count;

    if (!out_value || max_value < min_value)
        return false;

    count = max_value - min_value + 1;
    if (count > SETTINGS_VALUE_PICKER_MAX)
        return false;

    for (int i = 0; i < count; i++) {
        int value = min_value + i;

        strnfmt(labels[i], sizeof(labels[i]), show_sign ? "%+d" : "%d",
            value);
        choices[i].value = value;
        choices[i].label = labels[i];
    }

    return settings_pick_value(title, desc, choices, count, current_value,
        out_value);
}

static bool option_pick_from_choices(int opt,
    const struct settings_value_choice* choices, int count, int current_value,
    int* out_value, bool* handled)
{
    if (handled)
        *handled = true;

    return settings_pick_value(option_menu_label(opt), NULL, choices, count,
        current_value, out_value);
}

static bool option_pick_value(int opt, bool* handled)
{
    int value = 0;

    static const struct settings_value_choice delay_choices[] = {
        { 0, "0" }, { 1, "1" }, { 2, "2" }, { 3, "3" }, { 4, "4" },
        { 5, "5" }, { 6, "6" }, { 7, "7" }, { 8, "8" }, { 9, "9" }
    };
    static const struct settings_value_choice hp_warning_choices[] = {
        { 0, "0%" }, { 1, "10%" }, { 2, "20%" }, { 3, "30%" },
        { 4, "40%" }, { 5, "50%" }, { 6, "60%" }, { 7, "70%" },
        { 8, "80%" }, { 9, "90%" }
    };
    static const struct settings_value_choice level_entry_choices[] = {
        { LEVEL_ENTRY_NARRATIVE_BANNER_DELAY, "Banner with animation" },
        { LEVEL_ENTRY_NARRATIVE_BANNER, "Banner without animation" },
        { LEVEL_ENTRY_NARRATIVE_MESSAGE, "Message" },
        { LEVEL_ENTRY_NARRATIVE_OFF, "Off" }
    };
    static const struct settings_value_choice partition_choices[] = {
        { PARTITION_NARRATIVE_BANNER_DELAY, "Banner with animation" },
        { PARTITION_NARRATIVE_BANNER, "Banner without animation" },
        { PARTITION_NARRATIVE_MESSAGE, "Message" },
        { PARTITION_NARRATIVE_OFF, "Off" }
    };
    static const struct settings_value_choice vault_drop_choices[] = {
        { VDF_NORMAL, "Normal (0)" },
        { VDF_MODEST, "Modest (1)" },
        { VDF_SCARCE, "Scarce (2)" },
        { VDF_MEAGER, "Meager (3)" },
        { VDF_PLENTIFUL, "Plentiful (4)" }
    };
    static const struct settings_value_choice min_depth_choices[] = {
        { MIN_DEPTH_TIMER_MODE_NORMAL, "Normal" },
        { MIN_DEPTH_TIMER_MODE_RELAXED, "Relaxed (+30000)" },
        { MIN_DEPTH_TIMER_MODE_HARSH, "Harsh (-30000)" }
    };
    static const struct settings_value_choice noble_spawn_choices[] = {
        { NOBLE_ITEM_SPAWN_RESTRICTED,
            "0 (good+/chests/human+elf skeletons)" },
        { NOBLE_ITEM_SPAWN_INCLUDE_VAULTS, "1 (also vault drops)" }
    };
    static const struct settings_value_choice intro_choices[] = {
        { INTRO_STYLE_FLAME, "Flame Imperishable" },
        { INTRO_STYLE_FEANOR, "Oath of Feanor" },
        { INTRO_STYLE_TWILIGHT, "Twilight of Valinor" },
        { INTRO_STYLE_LUTHIEN, "Song of Luthien" },
        { INTRO_STYLE_HURIN, "Words of Hurin" },
        { INTRO_STYLE_STARLIGHT, "Starlight on Cuivienen" },
        { INTRO_STYLE_NOLDOLANTE, "Lament of the Noldor" },
        { INTRO_STYLE_RANDOM, "Random" }
    };
    static const struct settings_value_choice narrative_turn_choices[] = {
        { 0, "0 dismiss manually" },
        { 1, "1 turn" },
        { 2, "2 turns" },
        { 3, "3 turns" }
    };
    static const struct settings_value_choice supply_icon_choices[] = {
        { 0, "Fixed" },
        { 1, "Random" }
    };
    static const struct settings_value_choice player_tile_facing_choices[] = {
        { PLAYER_TILE_FACING_OFF, "off" },
        { PLAYER_TILE_FACING_MIRROR, "mirror" },
        { PLAYER_TILE_FACING_HANDCRAFTED, "handcrafted" }
    };
    static const struct settings_value_choice monster_tile_health_choices[] = {
        { MONSTER_TILE_HEALTH_BARS_SHOW, "Show" },
        { MONSTER_TILE_HEALTH_BARS_DAMAGED_ONLY, "Only damaged" },
        { MONSTER_TILE_HEALTH_BARS_OFF, "Off" }
    };

    if (handled)
        *handled = false;

    switch (opt)
    {
    case OPT_delay_factor:
        if (option_pick_from_choices(opt, delay_choices,
                (int)N_ELEMENTS(delay_choices), op_ptr->delay_factor, &value,
                handled)
            && value != op_ptr->delay_factor)
        {
            op_ptr->delay_factor = value;
            return true;
        }
        return false;

    case OPT_running_delay:
        if (option_pick_from_choices(opt, running_delay_choices,
                (int)N_ELEMENTS(running_delay_choices),
                op_ptr->running_delay_ms, &value, handled)
            && value != op_ptr->running_delay_ms)
        {
            op_ptr->running_delay_ms = (byte)value;
            return true;
        }
        return false;

    case OPT_hitpoint_warning:
        if (option_pick_from_choices(opt, hp_warning_choices,
                (int)N_ELEMENTS(hp_warning_choices), op_ptr->hitpoint_warn,
                &value, handled)
            && value != op_ptr->hitpoint_warn)
        {
            op_ptr->hitpoint_warn = value;
            return true;
        }
        return false;

    case OPT_show_level_entry_banner:
        value = op_ptr->level_entry_narrative_mode;
        if (value > LEVEL_ENTRY_NARRATIVE_OFF)
            value = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
        if (option_pick_from_choices(opt, level_entry_choices,
                (int)N_ELEMENTS(level_entry_choices), value, &value, handled)
            && value != op_ptr->level_entry_narrative_mode)
        {
            op_ptr->level_entry_narrative_mode = value;
            return true;
        }
        return false;

    case OPT_show_partition_narrative:
        value = op_ptr->partition_narrative_mode;
        if (value > PARTITION_NARRATIVE_BANNER_DELAY)
            value = PARTITION_NARRATIVE_BANNER_DELAY;
        if (option_pick_from_choices(opt, partition_choices,
                (int)N_ELEMENTS(partition_choices), value, &value, handled)
            && value != op_ptr->partition_narrative_mode)
        {
            op_ptr->partition_narrative_mode = value;
            return true;
        }
        return false;

    case OPT_vault_drop_frequency:
        value = op_ptr->vault_drop_frequency;
        if (value > VDF_PLENTIFUL)
            value = VDF_NORMAL;
        if (option_pick_from_choices(opt, vault_drop_choices,
                (int)N_ELEMENTS(vault_drop_choices), value, &value, handled)
            && value != op_ptr->vault_drop_frequency)
        {
            op_ptr->vault_drop_frequency = value;
            return true;
        }
        return false;

    case OPT_min_depth_timer_mode:
        value = op_ptr->min_depth_timer_mode;
        if (value > MIN_DEPTH_TIMER_MODE_MAX)
            value = MIN_DEPTH_TIMER_MODE_NORMAL;
        if (option_pick_from_choices(opt, min_depth_choices,
                (int)N_ELEMENTS(min_depth_choices), value, &value, handled)
            && value != op_ptr->min_depth_timer_mode)
        {
            op_ptr->min_depth_timer_mode = value;
            return true;
        }
        return false;

    case OPT_noble_item_spawn_mode:
        value = op_ptr->noble_item_spawn_mode;
        if (value > NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
            value = NOBLE_ITEM_SPAWN_RESTRICTED;
        if (option_pick_from_choices(opt, noble_spawn_choices,
                (int)N_ELEMENTS(noble_spawn_choices), value, &value, handled)
            && value != op_ptr->noble_item_spawn_mode)
        {
            op_ptr->noble_item_spawn_mode = value;
            return true;
        }
        return false;

    case OPT_intro_style:
        value = op_ptr->intro_style;
        if (value > INTRO_STYLE_RANDOM)
            value = INTRO_STYLE_FLAME;
        if (option_pick_from_choices(opt, intro_choices,
                (int)N_ELEMENTS(intro_choices), value, &value, handled)
            && value != op_ptr->intro_style)
        {
            op_ptr->intro_style = value;
            return true;
        }
        return false;

    case OPT_narrative_banner_turns:
        value = op_ptr->narrative_banner_turns;
        if (value > NARRATIVE_BANNER_TURNS_MAX)
            value = DEFAULT_NARRATIVE_BANNER_TURNS;
        if (option_pick_from_choices(opt, narrative_turn_choices,
                (int)N_ELEMENTS(narrative_turn_choices), value, &value,
                handled)
            && value != op_ptr->narrative_banner_turns)
        {
            op_ptr->narrative_banner_turns = value;
            return true;
        }
        return false;

    case OPT_supply_menu_random_icons:
        value = op_ptr->opt[opt] ? 1 : 0;
        if (option_pick_from_choices(opt, supply_icon_choices,
                (int)N_ELEMENTS(supply_icon_choices), value, &value, handled)
            && value != (op_ptr->opt[opt] ? 1 : 0))
        {
            op_ptr->opt[opt] = (value != 0);
            option_apply_side_effects(opt);
            return true;
        }
        return false;

    case OPT_mirror_player_tile_facing:
        value = option_player_tile_facing_mode();
        if (option_pick_from_choices(opt, player_tile_facing_choices,
                (int)N_ELEMENTS(player_tile_facing_choices), value, &value,
                handled)
            && value != option_player_tile_facing_mode())
        {
            option_set_player_tile_facing_mode((byte)value);
            return true;
        }
        return false;

    case OPT_styled_monster_tile_health_bars:
        value = option_monster_tile_health_bar_mode();
        if (option_pick_from_choices(opt, monster_tile_health_choices,
                (int)N_ELEMENTS(monster_tile_health_choices), value, &value,
                handled)
            && value != option_monster_tile_health_bar_mode())
        {
            option_set_monster_tile_health_bar_mode((byte)value);
            return true;
        }
        return false;

    default:
        return false;
    }
}

static float* sound_option_volume_ptr(struct sound_config* sound_cfg,
    int index)
{
    if (!sound_cfg)
        return NULL;

    switch (index)
    {
    case SOUND_OPT_COMBAT_VOLUME: return &sound_cfg->volume_combat;
    case SOUND_OPT_MONSTER_HITS_VOLUME: return &sound_cfg->volume_monster_hits;
    case SOUND_OPT_INVENTORY_VOLUME: return &sound_cfg->volume_inventory;
    case SOUND_OPT_WALK_VOLUME: return &sound_cfg->volume_walk;
    case SOUND_OPT_DOORS_VOLUME: return &sound_cfg->volume_doors;
    case SOUND_OPT_TRAPS_VOLUME: return &sound_cfg->volume_traps;
    case SOUND_OPT_OTHER_VOLUME: return &sound_cfg->volume_other;
    case SOUND_OPT_MUSIC_MAIN_VOLUME: return &sound_cfg->music_main_volume;
    case SOUND_OPT_MUSIC_AMBIENT_VOLUME: return &sound_cfg->music_ambient_volume;
    default: return NULL;
    }
}

static int sound_volume_percent(float volume)
{
    int percent;

    if (volume < 0.0f)
        volume = 0.0f;
    if (volume > 1.0f)
        volume = 1.0f;

    percent = (int)(volume * 100.0f + 0.5f);
    percent = ((percent + 5) / 10) * 10;

    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;

    return percent;
}

static bool sound_option_pick_value(int index, struct sound_config* sound_cfg,
    bool* handled)
{
    static const struct settings_value_choice volume_choices[] = {
        { 0, "0%" }, { 10, "10%" }, { 20, "20%" }, { 30, "30%" },
        { 40, "40%" }, { 50, "50%" }, { 60, "60%" }, { 70, "70%" },
        { 80, "80%" }, { 90, "90%" }, { 100, "100%" }
    };
    float* volume = sound_option_volume_ptr(sound_cfg, index);
    int current;
    int value;
    float new_volume;

    if (handled)
        *handled = (volume != NULL);

    if (!volume)
        return false;

    current = sound_volume_percent(*volume);
    if (!settings_pick_value(sound_option_label(index), NULL, volume_choices,
            (int)N_ELEMENTS(volume_choices), current, &value))
    {
        return false;
    }

    new_volume = (float)value / 100.0f;
    if (*volume == new_volume)
        return false;

    *volume = new_volume;
    if (index == SOUND_OPT_MUSIC_MAIN_VOLUME
        || index == SOUND_OPT_MUSIC_AMBIENT_VOLUME)
    {
        sdl_music_update_volumes();
        sdl_sound_save_config();
    }

    return true;
}

/*
 * Inline per-overlay-pane rows shown on the Interface options page.
 *
 * These rows live alongside the normal OPT_* options but edit SDL pane
 * configuration (via the get_sdl_ / set_sdl_ accessors) instead of
 * op_ptr->opt[].  They are appended after the real options; an opt[] slot
 * holding a value >=
 * IFACE_PANE_ROW_BASE marks such a row, whose descriptor is stored separately
 * (see do_cmd_options_aux).  Mirrors how the Sound page hosts non-OPT_* rows.
 */
enum iface_pane_field {
    IFACE_PANE_FIELD_ENABLED = 0,
    IFACE_PANE_FIELD_PLACEMENT,
    IFACE_PANE_FIELD_FONT,
    IFACE_PANE_FIELD_SIZE,
    IFACE_PANE_FIELD_COLUMNS,
    IFACE_PANE_FIELD_ROWS,
    IFACE_PANE_FIELD_QA_ARROWS,
    IFACE_PANE_FIELD_QA_LAUNCH,
    IFACE_PANE_FIELD_BUTTONS,
    IFACE_PANE_FIELD_LP_LAUNCH,
    IFACE_PANE_FIELD_LP_COMPACT,
    IFACE_PANE_FIELD_LP_TOUCH_EDGE,
    IFACE_PANE_FIELD_LOG_ROWS,
    IFACE_PANE_FIELD_OVERLAY_LOG_BORDER,
    IFACE_PANE_FIELD_DICE_LOCK,
    IFACE_PANE_FIELD_DICE_OVERLAY,
    IFACE_PANE_FIELD_MAIN_MENU_BUTTON,
    IFACE_PANE_FIELD_POPUP_NOTIFICATION,
    IFACE_PANE_FIELD_CAMERA_CENTER_CLEARANCE
};

struct iface_pane_row {
    int pane_cfg_index;
    enum pane_type type;
    enum iface_pane_field field;
};

#define IFACE_PANE_PLACEMENT_ORDER_STRIDE (MAX_PANE_CONFIGS + 1)

static bool pane_type_is_overlay(enum pane_type type);
static const char* pane_type_display_name(enum pane_type type);
static int build_interface_pane_rows(struct iface_pane_row* rows, int max_rows,
    struct option_group_marker* markers, int* marker_count);
static cptr iface_pane_row_label(const struct iface_pane_row* row);
static cptr iface_pane_row_description(const struct iface_pane_row* row);
static int iface_pane_row_live_index(const struct iface_pane_row* row);
static void iface_pane_row_value(const struct iface_pane_row* row, char* buf,
    size_t buflen);
static bool option_pick_value(int opt, bool* handled);
static bool sound_option_pick_value(int index, struct sound_config* sound_cfg,
    bool* handled);
static bool iface_pane_row_pick_value(const struct iface_pane_row* row,
    bool* handled);
static bool iface_pane_row_adjust(const struct iface_pane_row* row, int delta);
static bool iface_pane_row_resettable(const struct iface_pane_row* row);
static bool iface_pane_row_reset_to_default(const struct iface_pane_row* row);
static void do_cmd_touch_top_widget_button_editor(bool* settings_changed);

/*
 * Reset the option on row k of an options page back to its default value, and
 * flag the matching persistence store dirty.  Drives the per-row "Reset"
 * buttons so a mouse/touch/controller user can reset one setting without a key.
 * Overlay-pane rows on the Interface page are handled by their own editor and
 * are not reset here.
 */
static void options_aux_reset_to_default(int page, const int* opt, int k,
    bool metarun_page, struct sound_config* sound_cfg,
    bool* app_dirty, bool* sound_dirty, bool* metarun_dirty)
{
    int o;

    if (page == SOUND_PAGE)
    {
        struct sound_config def;

        sound_config_set_defaults(&def);
        switch (k)
        {
        case SOUND_OPT_ENABLED:
            sound_cfg->enabled = def.enabled;
            use_sound = sound_cfg->enabled;
            break;
        case SOUND_OPT_COMBAT_ENABLED:
            sound_cfg->enable_combat = def.enable_combat;
            break;
        case SOUND_OPT_MONSTER_HITS_ENABLED:
            sound_cfg->enable_monster_hits = def.enable_monster_hits;
            break;
        case SOUND_OPT_INVENTORY_ENABLED:
            sound_cfg->enable_inventory = def.enable_inventory;
            break;
        case SOUND_OPT_WALK_ENABLED:
            sound_cfg->enable_walk = def.enable_walk;
            break;
        case SOUND_OPT_DOORS_ENABLED:
            sound_cfg->enable_doors = def.enable_doors;
            break;
        case SOUND_OPT_TRAPS_ENABLED:
            sound_cfg->enable_traps = def.enable_traps;
            break;
        case SOUND_OPT_COMBAT_VOLUME:
            sound_cfg->volume_combat = def.volume_combat;
            break;
        case SOUND_OPT_MONSTER_HITS_VOLUME:
            sound_cfg->volume_monster_hits = def.volume_monster_hits;
            break;
        case SOUND_OPT_INVENTORY_VOLUME:
            sound_cfg->volume_inventory = def.volume_inventory;
            break;
        case SOUND_OPT_WALK_VOLUME:
            sound_cfg->volume_walk = def.volume_walk;
            break;
        case SOUND_OPT_DOORS_VOLUME:
            sound_cfg->volume_doors = def.volume_doors;
            break;
        case SOUND_OPT_TRAPS_VOLUME:
            sound_cfg->volume_traps = def.volume_traps;
            break;
        case SOUND_OPT_OTHER_VOLUME:
            sound_cfg->volume_other = def.volume_other;
            break;
        case SOUND_OPT_MUSIC_MAIN_ENABLED:
            sound_cfg->music_main_enabled = def.music_main_enabled;
            break;
        case SOUND_OPT_MUSIC_AMBIENT_ENABLED:
            sound_cfg->music_ambient_enabled = def.music_ambient_enabled;
            break;
        case SOUND_OPT_MUSIC_MAIN_VOLUME:
            sound_cfg->music_main_volume = def.music_main_volume;
            break;
        case SOUND_OPT_MUSIC_AMBIENT_VOLUME:
            sound_cfg->music_ambient_volume = def.music_ambient_volume;
            break;
        default:
            return;
        }
        if (sound_dirty)
            *sound_dirty = true;
        return;
    }

    o = opt[k];
    switch (o)
    {
    case OPT_delay_factor:
        op_ptr->delay_factor = 5;
        break;
    case OPT_running_delay:
        op_ptr->running_delay_ms = DEFAULT_RUNNING_DELAY_MS;
        break;
    case OPT_hitpoint_warning:
        op_ptr->hitpoint_warn = 3;
        break;
    case OPT_show_level_entry_banner:
        op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
        break;
    case OPT_show_partition_narrative:
        op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER_DELAY;
        break;
    case OPT_vault_drop_frequency:
        op_ptr->vault_drop_frequency = VDF_NORMAL;
        break;
    case OPT_min_depth_timer_mode:
        op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;
        break;
    case OPT_narrative_banner_turns:
        op_ptr->narrative_banner_turns = DEFAULT_NARRATIVE_BANNER_TURNS;
        break;
    case OPT_intro_style:
        op_ptr->intro_style = INTRO_STYLE_RANDOM;
        break;
    case OPT_noble_item_spawn_mode:
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
        break;
    case OPT_mirror_player_tile_facing:
        option_set_player_tile_facing_mode(
            option_norm[OPT_mirror_player_tile_facing]
                ? (option_norm[OPT_handcrafted_player_tile_facing]
                    ? PLAYER_TILE_FACING_HANDCRAFTED
                    : PLAYER_TILE_FACING_MIRROR)
                : PLAYER_TILE_FACING_OFF);
        break;
    case OPT_styled_monster_tile_health_bars:
        option_set_monster_tile_health_bar_mode(
            MONSTER_TILE_HEALTH_BARS_SHOW);
        break;
    default:
        op_ptr->opt[o] = option_norm[o];
        option_apply_side_effects(o);
        break;
    }

    if (option_is_app_persistent(o))
    {
        if (app_dirty)
            *app_dirty = true;
    }
    else if (metarun_page)
    {
        if (metarun_dirty)
            *metarun_dirty = true;
    }
}

extern void do_cmd_options_aux(int page, cptr info)
{
    int ch;

    int i, k = 0, n = 0;

    int opt[OPT_PAGE_PER + IFACE_PANE_ROW_MAX];
    struct iface_pane_row pane_rows[IFACE_PANE_ROW_MAX];
    int pane_row_count = 0;
    struct option_group_marker iface_groups[
        OPT_PAGE_PER + IFACE_PANE_ROW_MAX + 1];

    char buf[160];

    int dir;

    bool is_sound_page = (page == SOUND_PAGE);
    bool app_page = option_page_uses_app_config(page);
    bool metarun_page = !app_page && !is_sound_page;
    bool app_settings_dirty = false;
    bool metarun_settings_dirty = false;
    bool sound_settings_dirty = false;
    const struct option_group_marker* groups = get_option_groups_for_page(page);
    struct sound_config* sound_cfg = sdl_sound_get_config();

    settings_option_page_turn = 0;

    /* Scan the options */
    for (i = 0; i < OPT_PAGE_PER; i++)
    {
        /* Collect options on this "page" */
        if (option_page[page][i] != OPT_NONE)
        {
            int candidate = option_page[page][i];

#if defined(__ANDROID__) || defined(SIL_IOS)
            /* These settings control ordinary supporting panes, which mobile
             * builds never create.  Overlay presentation remains available
             * through the Interface page's overlay rows below. */
            if (candidate == OPT_hide_supporting_panes_fullscreen
                || candidate == OPT_story_monster_desc_pane
                || candidate == OPT_story_lists_inven_pane
                || candidate == OPT_story_lists_equip_pane)
            {
                continue;
            }
#endif
            opt[n++] = candidate;
        }
    }

    /* Special case: Sound page uses custom display instead of standard options */
    if (is_sound_page)
    {
        n = SOUND_OPT_MAX;
        for (i = 0; i < n; i++)
            opt[i] = SOUND_OPTION_ROW(i);
    }

    /* Interface page: append inline per-overlay-pane rows after the OPT_* rows. */
    if (page == INTERFACE_PAGE)
    {
        struct option_group_marker pane_markers[IFACE_PANE_ROW_MAX];
        int pane_marker_count = 0;
        int g = 0;
        int s, m;

        pane_row_count = build_interface_pane_rows(pane_rows, IFACE_PANE_ROW_MAX,
            pane_markers, &pane_marker_count);

        for (i = 0; i < pane_row_count; i++)
            opt[n++] = IFACE_PANE_ROW_BASE + i;

        /* Merge stable option ownership with dynamic pane-row ownership. */
        for (s = 0; interface_option_groups[s].setting_id >= 0; s++)
            iface_groups[g++] = interface_option_groups[s];
        for (m = 0; m < pane_marker_count; m++)
            iface_groups[g++] = pane_markers[m];
        iface_groups[g].setting_id = -1;
        iface_groups[g].label = NULL;
        groups = iface_groups;
    }

    /* Interact with the player */
    while (true)
    {
        settings_semantic_menu_begin(info, k);
        if (page == CHALLENGE_PAGE)
        {
            sdl_character_sheet_screen_set_select_description(
                "Challenge options can only be changed during character creation "
                "or on the very first turn.");
        }

        /* Display the options */
        for (i = 0; i < n; i++)
        {
            byte a = TERM_WHITE;
            cptr group_label = option_group_starts_at(groups, opt, i)
                ? option_group_for_setting(groups, opt[i]) : NULL;

            if (group_label)
                sdl_character_sheet_screen_add_select_heading(group_label);

            /* Color current option */
            if (i == k)
                a = TERM_L_BLUE;

            /* Display the option text */
            buf[0] = '\0';
            if (page == INTERFACE_PAGE && opt[i] >= IFACE_PANE_ROW_BASE)
            {
                char value_str[32];
                const struct iface_pane_row* prow =
                    &pane_rows[opt[i] - IFACE_PANE_ROW_BASE];

                iface_pane_row_value(prow, value_str, sizeof(value_str));
                if (prow->field == IFACE_PANE_FIELD_QA_LAUNCH
                    && !get_sdl_touch_top_panel_arrows_visible())
                {
                    a = TERM_SLATE;
                }
                option_menu_format_line(buf, sizeof(buf),
                    iface_pane_row_label(prow), value_str);
            }
            else if (is_sound_page)
            {
                char value_str[32];

                switch (i)
                {
                case SOUND_OPT_ENABLED:
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enabled ? "yes" : "no ");
                    break;
                case SOUND_OPT_COMBAT_ENABLED:
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_combat ? "yes" : "no ");
                    break;
                case SOUND_OPT_MONSTER_HITS_ENABLED:
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_monster_hits ? "yes" : "no ");
                    break;
                case SOUND_OPT_INVENTORY_ENABLED:
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_inventory ? "yes" : "no ");
                    break;
                case SOUND_OPT_WALK_ENABLED:
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_walk ? "yes" : "no ");
                    break;
                case SOUND_OPT_DOORS_ENABLED:
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_doors ? "yes" : "no ");
                    break;
                case SOUND_OPT_TRAPS_ENABLED:
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_traps ? "yes" : "no ");
                    break;
                case SOUND_OPT_COMBAT_VOLUME:
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_combat * 100.0f);
                    break;
                case SOUND_OPT_MONSTER_HITS_VOLUME:
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_monster_hits * 100.0f);
                    break;
                case SOUND_OPT_INVENTORY_VOLUME:
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_inventory * 100.0f);
                    break;
                case SOUND_OPT_WALK_VOLUME:
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_walk * 100.0f);
                    break;
                case SOUND_OPT_DOORS_VOLUME:
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_doors * 100.0f);
                    break;
                case SOUND_OPT_TRAPS_VOLUME:
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_traps * 100.0f);
                    break;
                case SOUND_OPT_OTHER_VOLUME:
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_other * 100.0f);
                    break;
                case SOUND_OPT_MUSIC_MAIN_ENABLED:
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->music_main_enabled ? "yes" : "no ");
                    break;
                case SOUND_OPT_MUSIC_AMBIENT_ENABLED:
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->music_ambient_enabled ? "yes" : "no ");
                    break;
                case SOUND_OPT_MUSIC_MAIN_VOLUME:
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->music_main_volume * 100.0f);
                    break;
                case SOUND_OPT_MUSIC_AMBIENT_VOLUME:
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->music_ambient_volume * 100.0f);
                    break;
                default:
                    strnfmt(value_str, sizeof(value_str), "%s", "");
                    break;
                }

                option_menu_format_line(buf, sizeof(buf), sound_option_label(i),
                    value_str);
            }
            else if (opt[i] == OPT_delay_factor)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d", op_ptr->delay_factor);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_running_delay)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d ms",
                    op_ptr->running_delay_ms);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_hitpoint_warning)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d%%",
                    op_ptr->hitpoint_warn * 10);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_show_level_entry_banner)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->level_entry_narrative_mode)
                {
                case LEVEL_ENTRY_NARRATIVE_BANNER_DELAY:
                    mode_str = compact ? "Banner anim" : "Banner with animation";
                    break;
                case LEVEL_ENTRY_NARRATIVE_BANNER:
                    mode_str = compact ? "Banner no anim" : "Banner without animation";
                    break;
                case LEVEL_ENTRY_NARRATIVE_MESSAGE: mode_str = "Message"; break;
                case LEVEL_ENTRY_NARRATIVE_OFF:     mode_str = "Off"; break;
                default:
                    mode_str = compact ? "Banner anim" : "Banner with animation";
                    break;
                }
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_show_partition_narrative)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->partition_narrative_mode)
                {
                case PARTITION_NARRATIVE_BANNER_DELAY:
                    mode_str = compact ? "Banner anim" : "Banner with animation";
                    break;
                case PARTITION_NARRATIVE_BANNER:
                    mode_str = compact ? "Banner no anim" : "Banner without animation";
                    break;
                case PARTITION_NARRATIVE_OFF:     mode_str = "Off"; break;
                default:                          mode_str = "Message"; break;
                }
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_vault_drop_frequency)
            {
                const char *vdf_names[] = { "Normal", "Modest", "Scarce", "Meager", "Plentiful" };
                char value_str[32];
                byte mode = op_ptr->vault_drop_frequency;
                if (mode > VDF_PLENTIFUL)
                    mode = VDF_NORMAL;
                strnfmt(value_str, sizeof(value_str), "%s (%d)", vdf_names[mode],
                    mode);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_min_depth_timer_mode)
            {
                const char *mode_str;

                switch (op_ptr->min_depth_timer_mode)
                {
                case MIN_DEPTH_TIMER_MODE_RELAXED:
                    mode_str = option_menu_use_compact_layout()
                        ? "+30000 relaxed"
                        : "Relaxed (+30000)";
                    break;
                case MIN_DEPTH_TIMER_MODE_HARSH:
                    mode_str = option_menu_use_compact_layout()
                        ? "-30000 harsh"
                        : "Harsh (-30000)";
                    break;
                default:
                    mode_str = "Normal";
                    break;
                }

                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_noble_item_spawn_mode)
            {
                const char *mode_str
                    = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
                    ? (option_menu_use_compact_layout() ? "1 with vaults" : "1 (also &/! vault drops)")
                    : (option_menu_use_compact_layout() ? "0 restricted" : "0 (good+/chests/human+elf skeletons)");
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_intro_style)
            {
                const char *is_names[] = {
                    "Flame Imperishable", "Oath of Fëanor",
                    "Twilight of Valinor", "Song of Lúthien",
                    "Words of Húrin", "Starlight on Cuiviénen",
                    "Lament of the Noldor", "Random"
                };
                byte m = op_ptr->intro_style;
                if (m > INTRO_STYLE_RANDOM) m = INTRO_STYLE_FLAME;
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    is_names[m]);
            }
            else if (opt[i] == OPT_narrative_banner_turns)
            {
                byte turns = op_ptr->narrative_banner_turns;
                char value_str[32];

                if (turns > NARRATIVE_BANNER_TURNS_MAX)
                    turns = DEFAULT_NARRATIVE_BANNER_TURNS;

                if (turns == 0)
                    strnfmt(value_str, sizeof(value_str), "0 dismiss");
                else
                    strnfmt(value_str, sizeof(value_str), "%d turn%s",
                        turns, (turns == 1) ? "" : "s");

                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_supply_menu_random_icons)
            {
                option_menu_format_line(buf, sizeof(buf),
                    option_menu_label(opt[i]),
                    op_ptr->opt[opt[i]] ? "Random" : "Fixed");
            }
            else if (opt[i] == OPT_mirror_player_tile_facing)
            {
                option_menu_format_line(buf, sizeof(buf),
                    option_menu_label(opt[i]),
                    option_player_tile_facing_mode_label());
            }
            else if (opt[i] == OPT_styled_monster_tile_health_bars)
            {
                option_menu_format_line(buf, sizeof(buf),
                    option_menu_label(opt[i]),
                    option_monster_tile_health_bar_mode_label());
            }
            else
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    op_ptr->opt[opt[i]] ? "yes" : "no ");
            }

            {
                char semantic_buf[160];

                settings_semantic_line_from_menu_line(semantic_buf,
                    sizeof(semantic_buf), buf);
                settings_semantic_add_row(i, semantic_buf, a);
                if (page == INTERFACE_PAGE && opt[i] >= IFACE_PANE_ROW_BASE)
                {
                    if (iface_pane_row_resettable(
                            &pane_rows[opt[i] - IFACE_PANE_ROW_BASE]))
                        sdl_character_sheet_screen_set_last_select_row_reset(
                            SETTINGS_CLICK_RESET_ROW_BASE + i);
                }
                else if (page != CHALLENGE_PAGE || playerturn == 0)
                    sdl_character_sheet_screen_set_last_select_row_reset(
                        SETTINGS_CLICK_RESET_ROW_BASE + i);
            }
        }

        {
            /* Describe the focused option.  Standard game options carry help
             * text in option_desc[]; special rows (pane toggles, sound) have
             * none yet, so the band is left empty.  The challenge page keeps
             * its own note set above. */
            if (page != CHALLENGE_PAGE)
            {
                cptr od = NULL;
                int o = opt[k];

                if (page == INTERFACE_PAGE && o >= IFACE_PANE_ROW_BASE)
                    od = iface_pane_row_description(
                        &pane_rows[o - IFACE_PANE_ROW_BASE]);
                else if (!is_sound_page && o >= 0 && o < OPT_MAX)
                    od = option_desc[o];
                sdl_character_sheet_screen_set_select_description(
                    od ? od : "");
            }
            sdl_character_sheet_screen_commit_select(k);
        }

        /* Get a key */
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        /* A touch-scroll redraw posts a wake key with no pending click. */
        if (ch == UI_MENU_CLICK_WAKE_KEY && !ui_menu_click_has_pending())
            continue;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (click_action == UI_MENU_CLICK_HOVER && clicked_choice < 0)
                    continue;
                if (clicked_choice == -1)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                }
                else if (clicked_choice == -2)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ' ';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RETURN)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                }
                else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice < SETTINGS_CLICK_RESET_ROW_BASE + n)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    k = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    if (page == INTERFACE_PAGE && opt[k] >= IFACE_PANE_ROW_BASE)
                    {
                        if (iface_pane_row_reset_to_default(
                                &pane_rows[opt[k] - IFACE_PANE_ROW_BASE]))
                            app_settings_dirty = true;
                    }
                    else if ((page != CHALLENGE_PAGE) || (playerturn == 0))
                        options_aux_reset_to_default(page, opt, k, metarun_page,
                            sound_cfg, &app_settings_dirty,
                            &sound_settings_dirty, &metarun_settings_dirty);
                    continue;
                }
                else if (clicked_choice >= 0 && clicked_choice < n)
                {
                    bool was_current = (clicked_choice == k);
                    bool touch_primary =
                        sdl_touch_only_device_active()
                        && click_action == UI_MENU_CLICK_PRIMARY;

                    k = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    if (click_action == UI_MENU_CLICK_SECONDARY)
                        ch = '4';
                    else if (was_current || touch_primary)
                        ch = ' ';
                    else
                        continue;
                    click_generated = true;
                }
            }

            ch = settings_menu_key(ch, SETTINGS_PREV_OPTION_PAGE,
                SETTINGS_NEXT_OPTION_PAGE, click_generated);
        }

        if (ch == SETTINGS_PREV_OPTION_PAGE)
            settings_option_page_turn = -1;
        else if (ch == SETTINGS_NEXT_OPTION_PAGE)
            settings_option_page_turn = 1;

        /*
         * HACK - Try to translate the key into a direction
         * to allow using the roguelike keys for navigation.
         */
        dir = target_dir((char)ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        /* Analyze */
        switch (ch)
        {
        case ESCAPE:
        case SETTINGS_PREV_OPTION_PAGE:
        case SETTINGS_NEXT_OPTION_PAGE:
        case '\n':
        case '\r':
        {
            settings_semantic_menu_hide();

            /* Hack -- Notice use of any "cheat" options */
            for (i = OPT_CHEAT; i < OPT_ADULT; i++)
            {
                if (op_ptr->opt[i])
                {
                    /* Set score option */
                    if (!op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)])
                        metarun_settings_dirty = true;
                    op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = true;
                }
            }

            if (sound_settings_dirty)
            {
                sdl_sound_save_config();
                sdl_sound_reload();
            }

            if (app_settings_dirty)
                save_pane_config_to_json();

            if (metarun_settings_dirty)
                metarun_save_persistent_settings();

            return;
        }

        case '-':
        case '8':
        {
            k = (n + k - 1) % n;
            break;
        }

        case '2':
        {
            k = (k + 1) % n;
            break;
        }

        case 't':
        case '5':
        case ' ':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                bool handled = false;
                bool changed = false;

                if (page == INTERFACE_PAGE && opt[k] >= IFACE_PANE_ROW_BASE)
                {
                    changed = iface_pane_row_pick_value(
                        &pane_rows[opt[k] - IFACE_PANE_ROW_BASE], &handled);
                    if (!handled)
                        changed = iface_pane_row_adjust(
                            &pane_rows[opt[k] - IFACE_PANE_ROW_BASE], 0);
                    if (changed)
                        app_settings_dirty = true;
                }
                else if (is_sound_page)
                {
                    changed = sound_option_pick_value(k, sound_cfg, &handled);
                    if (!handled)
                    {
                        switch (k)
                        {
                        case SOUND_OPT_ENABLED:
                            sound_cfg->enabled = !sound_cfg->enabled;
                            use_sound = sound_cfg->enabled;
                            changed = true;
                            break;
                        case SOUND_OPT_COMBAT_ENABLED:
                            sound_cfg->enable_combat = !sound_cfg->enable_combat;
                            changed = true;
                            break;
                        case SOUND_OPT_MONSTER_HITS_ENABLED:
                            sound_cfg->enable_monster_hits = !sound_cfg->enable_monster_hits;
                            changed = true;
                            break;
                        case SOUND_OPT_INVENTORY_ENABLED:
                            sound_cfg->enable_inventory = !sound_cfg->enable_inventory;
                            changed = true;
                            break;
                        case SOUND_OPT_WALK_ENABLED:
                            sound_cfg->enable_walk = !sound_cfg->enable_walk;
                            changed = true;
                            break;
                        case SOUND_OPT_DOORS_ENABLED:
                            sound_cfg->enable_doors = !sound_cfg->enable_doors;
                            changed = true;
                            break;
                        case SOUND_OPT_TRAPS_ENABLED:
                            sound_cfg->enable_traps = !sound_cfg->enable_traps;
                            changed = true;
                            break;
                        case SOUND_OPT_MUSIC_MAIN_ENABLED:
                            sound_cfg->music_main_enabled = !sound_cfg->music_main_enabled;
                            changed = true;
                            break;
                        case SOUND_OPT_MUSIC_AMBIENT_ENABLED:
                            sound_cfg->music_ambient_enabled = !sound_cfg->music_ambient_enabled;
                            changed = true;
                            break;
                        default:
                            break;
                        }
                    }
                    if (changed)
                        sound_settings_dirty = true;
                }
                else
                {
                    changed = option_pick_value(opt[k], &handled);
                    if (!handled)
                    {
                        op_ptr->opt[opt[k]] = !op_ptr->opt[opt[k]];
                        option_apply_side_effects(opt[k]);
                        changed = true;
                    }

                    if (changed)
                    {
                        if (option_is_app_persistent(opt[k]))
                            app_settings_dirty = true;
                        else if (metarun_page)
                            metarun_settings_dirty = true;
                    }
                }
            }
            break;
        }

        case 'y':
        case '6':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                if (page == INTERFACE_PAGE && opt[k] >= IFACE_PANE_ROW_BASE)
                {
                    if (iface_pane_row_adjust(
                            &pane_rows[opt[k] - IFACE_PANE_ROW_BASE], 1))
                        app_settings_dirty = true;
                }
                else if (is_sound_page)
                {
                    switch (k)
                    {
                    case SOUND_OPT_ENABLED:
                        sound_cfg->enabled = true;
                        use_sound = true;
                        break;
                    case SOUND_OPT_COMBAT_ENABLED:
                        sound_cfg->enable_combat = true;
                        break;
                    case SOUND_OPT_MONSTER_HITS_ENABLED:
                        sound_cfg->enable_monster_hits = true;
                        break;
                    case SOUND_OPT_INVENTORY_ENABLED:
                        sound_cfg->enable_inventory = true;
                        break;
                    case SOUND_OPT_WALK_ENABLED:
                        sound_cfg->enable_walk = true;
                        break;
                    case SOUND_OPT_DOORS_ENABLED:
                        sound_cfg->enable_doors = true;
                        break;
                    case SOUND_OPT_TRAPS_ENABLED:
                        sound_cfg->enable_traps = true;
                        break;
                    case SOUND_OPT_COMBAT_VOLUME:
                        sound_cfg->volume_combat = (sound_cfg->volume_combat < 1.0f) ? sound_cfg->volume_combat + 0.1f : 1.0f;
                        break;
                    case SOUND_OPT_MONSTER_HITS_VOLUME:
                        sound_cfg->volume_monster_hits = (sound_cfg->volume_monster_hits < 1.0f) ? sound_cfg->volume_monster_hits + 0.1f : 1.0f;
                        break;
                    case SOUND_OPT_INVENTORY_VOLUME:
                        sound_cfg->volume_inventory = (sound_cfg->volume_inventory < 1.0f) ? sound_cfg->volume_inventory + 0.1f : 1.0f;
                        break;
                    case SOUND_OPT_WALK_VOLUME:
                        sound_cfg->volume_walk = (sound_cfg->volume_walk < 1.0f) ? sound_cfg->volume_walk + 0.1f : 1.0f;
                        break;
                    case SOUND_OPT_DOORS_VOLUME:
                        sound_cfg->volume_doors = (sound_cfg->volume_doors < 1.0f) ? sound_cfg->volume_doors + 0.1f : 1.0f;
                        break;
                    case SOUND_OPT_TRAPS_VOLUME:
                        sound_cfg->volume_traps = (sound_cfg->volume_traps < 1.0f) ? sound_cfg->volume_traps + 0.1f : 1.0f;
                        break;
                    case SOUND_OPT_OTHER_VOLUME:
                        sound_cfg->volume_other = (sound_cfg->volume_other < 1.0f) ? sound_cfg->volume_other + 0.1f : 1.0f;
                        break;
                    case SOUND_OPT_MUSIC_MAIN_ENABLED:
                        sound_cfg->music_main_enabled = true;
                        break;
                    case SOUND_OPT_MUSIC_AMBIENT_ENABLED:
                        sound_cfg->music_ambient_enabled = true;
                        break;
                    case SOUND_OPT_MUSIC_MAIN_VOLUME:
                        sound_cfg->music_main_volume = (sound_cfg->music_main_volume < 1.0f) ? sound_cfg->music_main_volume + 0.1f : 1.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                        break;
                    case SOUND_OPT_MUSIC_AMBIENT_VOLUME:
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume < 1.0f) ? sound_cfg->music_ambient_volume + 0.1f : 1.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                        break;
                    default:
                        break;
                    }
                }
                else if (opt[k] == OPT_delay_factor)
                {
                    op_ptr->delay_factor = (op_ptr->delay_factor < 9)
                        ? op_ptr->delay_factor + 1
                        : 9;
                }
                else if (opt[k] == OPT_running_delay)
                {
                    op_ptr->running_delay_ms = running_delay_step(
                        op_ptr->running_delay_ms, 1);
                }
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn < 9)
                        ? op_ptr->hitpoint_warn + 1
                        : 9;
                }
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode < LEVEL_ENTRY_NARRATIVE_OFF)
                        ? op_ptr->level_entry_narrative_mode + 1
                        : LEVEL_ENTRY_NARRATIVE_OFF;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    switch (op_ptr->partition_narrative_mode)
                    {
                    case PARTITION_NARRATIVE_BANNER_DELAY:
                        op_ptr->partition_narrative_mode =
                            PARTITION_NARRATIVE_BANNER;
                        break;
                    case PARTITION_NARRATIVE_BANNER:
                        op_ptr->partition_narrative_mode =
                            PARTITION_NARRATIVE_MESSAGE;
                        break;
                    case PARTITION_NARRATIVE_MESSAGE:
                        op_ptr->partition_narrative_mode =
                            PARTITION_NARRATIVE_OFF;
                        break;
                    default:
                        op_ptr->partition_narrative_mode =
                            PARTITION_NARRATIVE_OFF;
                        break;
                    }
                }
                else if (opt[k] == OPT_vault_drop_frequency)
                {
                    op_ptr->vault_drop_frequency
                        = (op_ptr->vault_drop_frequency < VDF_PLENTIFUL)
                        ? op_ptr->vault_drop_frequency + 1
                        : VDF_PLENTIFUL;
                }
                else if (opt[k] == OPT_min_depth_timer_mode)
                {
                    op_ptr->min_depth_timer_mode
                        = (op_ptr->min_depth_timer_mode < MIN_DEPTH_TIMER_MODE_MAX)
                        ? op_ptr->min_depth_timer_mode + 1
                        : MIN_DEPTH_TIMER_MODE_MAX;
                }
                else if (opt[k] == OPT_noble_item_spawn_mode)
                {
                    op_ptr->noble_item_spawn_mode
                        = (op_ptr->noble_item_spawn_mode < NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
                        ? op_ptr->noble_item_spawn_mode + 1
                        : NOBLE_ITEM_SPAWN_INCLUDE_VAULTS;
                }
                else if (opt[k] == OPT_intro_style)
                {
                    op_ptr->intro_style
                        = (op_ptr->intro_style < INTRO_STYLE_RANDOM)
                        ? op_ptr->intro_style + 1
                        : INTRO_STYLE_RANDOM;
                }
                else if (opt[k] == OPT_narrative_banner_turns)
                {
                    op_ptr->narrative_banner_turns =
                        (op_ptr->narrative_banner_turns < NARRATIVE_BANNER_TURNS_MAX)
                        ? op_ptr->narrative_banner_turns + 1
                        : NARRATIVE_BANNER_TURNS_MAX;
                }
                else if (opt[k] == OPT_mirror_player_tile_facing)
                {
                    byte mode = option_player_tile_facing_mode();
                    option_set_player_tile_facing_mode(
                        (mode < PLAYER_TILE_FACING_MAX)
                        ? (byte)(mode + 1)
                        : PLAYER_TILE_FACING_MAX);
                }
                else if (opt[k] == OPT_styled_monster_tile_health_bars)
                {
                    byte mode = option_monster_tile_health_bar_mode();
                    option_set_monster_tile_health_bar_mode(
                        (mode < MONSTER_TILE_HEALTH_BARS_MAX)
                        ? (byte)(mode + 1)
                        : MONSTER_TILE_HEALTH_BARS_MAX);
                }
                else
                {
                    op_ptr->opt[opt[k]] = true;
                    option_apply_side_effects(opt[k]);
                }

                if (is_sound_page)
                    sound_settings_dirty = true;
                else if (option_is_app_persistent(opt[k]))
                    app_settings_dirty = true;
                else if (metarun_page)
                    metarun_settings_dirty = true;
            }
            break;
        }

        case 'n':
        case '4':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                if (page == INTERFACE_PAGE && opt[k] >= IFACE_PANE_ROW_BASE)
                {
                    if (iface_pane_row_adjust(
                            &pane_rows[opt[k] - IFACE_PANE_ROW_BASE], -1))
                        app_settings_dirty = true;
                }
                else if (is_sound_page)
                {
                    switch (k)
                    {
                    case SOUND_OPT_ENABLED:
                        sound_cfg->enabled = false;
                        use_sound = false;
                        break;
                    case SOUND_OPT_COMBAT_ENABLED:
                        sound_cfg->enable_combat = false;
                        break;
                    case SOUND_OPT_MONSTER_HITS_ENABLED:
                        sound_cfg->enable_monster_hits = false;
                        break;
                    case SOUND_OPT_INVENTORY_ENABLED:
                        sound_cfg->enable_inventory = false;
                        break;
                    case SOUND_OPT_WALK_ENABLED:
                        sound_cfg->enable_walk = false;
                        break;
                    case SOUND_OPT_DOORS_ENABLED:
                        sound_cfg->enable_doors = false;
                        break;
                    case SOUND_OPT_TRAPS_ENABLED:
                        sound_cfg->enable_traps = false;
                        break;
                    case SOUND_OPT_COMBAT_VOLUME:
                        sound_cfg->volume_combat = (sound_cfg->volume_combat > 0.0f) ? sound_cfg->volume_combat - 0.1f : 0.0f;
                        break;
                    case SOUND_OPT_MONSTER_HITS_VOLUME:
                        sound_cfg->volume_monster_hits = (sound_cfg->volume_monster_hits > 0.0f) ? sound_cfg->volume_monster_hits - 0.1f : 0.0f;
                        break;
                    case SOUND_OPT_INVENTORY_VOLUME:
                        sound_cfg->volume_inventory = (sound_cfg->volume_inventory > 0.0f) ? sound_cfg->volume_inventory - 0.1f : 0.0f;
                        break;
                    case SOUND_OPT_WALK_VOLUME:
                        sound_cfg->volume_walk = (sound_cfg->volume_walk > 0.0f) ? sound_cfg->volume_walk - 0.1f : 0.0f;
                        break;
                    case SOUND_OPT_DOORS_VOLUME:
                        sound_cfg->volume_doors = (sound_cfg->volume_doors > 0.0f) ? sound_cfg->volume_doors - 0.1f : 0.0f;
                        break;
                    case SOUND_OPT_TRAPS_VOLUME:
                        sound_cfg->volume_traps = (sound_cfg->volume_traps > 0.0f) ? sound_cfg->volume_traps - 0.1f : 0.0f;
                        break;
                    case SOUND_OPT_OTHER_VOLUME:
                        sound_cfg->volume_other = (sound_cfg->volume_other > 0.0f) ? sound_cfg->volume_other - 0.1f : 0.0f;
                        break;
                    case SOUND_OPT_MUSIC_MAIN_ENABLED:
                        sound_cfg->music_main_enabled = false;
                        break;
                    case SOUND_OPT_MUSIC_AMBIENT_ENABLED:
                        sound_cfg->music_ambient_enabled = false;
                        break;
                    case SOUND_OPT_MUSIC_MAIN_VOLUME:
                        sound_cfg->music_main_volume = (sound_cfg->music_main_volume > 0.0f) ? sound_cfg->music_main_volume - 0.1f : 0.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                        break;
                    case SOUND_OPT_MUSIC_AMBIENT_VOLUME:
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume > 0.0f) ? sound_cfg->music_ambient_volume - 0.1f : 0.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                        break;
                    default:
                        break;
                    }
                }
                else if (opt[k] == OPT_delay_factor)
                {
                    op_ptr->delay_factor = (op_ptr->delay_factor > 0)
                        ? op_ptr->delay_factor - 1
                        : 0;
                }
                else if (opt[k] == OPT_running_delay)
                {
                    op_ptr->running_delay_ms = running_delay_step(
                        op_ptr->running_delay_ms, -1);
                }
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn > 0)
                        ? op_ptr->hitpoint_warn - 1
                        : 0;
                }
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_BANNER_DELAY)
                        ? op_ptr->level_entry_narrative_mode - 1
                        : LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    switch (op_ptr->partition_narrative_mode)
                    {
                    case PARTITION_NARRATIVE_OFF:
                        op_ptr->partition_narrative_mode =
                            PARTITION_NARRATIVE_MESSAGE;
                        break;
                    case PARTITION_NARRATIVE_MESSAGE:
                        op_ptr->partition_narrative_mode =
                            PARTITION_NARRATIVE_BANNER;
                        break;
                    case PARTITION_NARRATIVE_BANNER:
                        op_ptr->partition_narrative_mode =
                            PARTITION_NARRATIVE_BANNER_DELAY;
                        break;
                    default:
                        op_ptr->partition_narrative_mode =
                            PARTITION_NARRATIVE_BANNER_DELAY;
                        break;
                    }
                }
                else if (opt[k] == OPT_vault_drop_frequency)
                {
                    op_ptr->vault_drop_frequency
                        = (op_ptr->vault_drop_frequency > VDF_NORMAL)
                        ? op_ptr->vault_drop_frequency - 1
                        : VDF_NORMAL;
                }
                else if (opt[k] == OPT_min_depth_timer_mode)
                {
                    op_ptr->min_depth_timer_mode
                        = (op_ptr->min_depth_timer_mode > MIN_DEPTH_TIMER_MODE_NORMAL)
                        ? op_ptr->min_depth_timer_mode - 1
                        : MIN_DEPTH_TIMER_MODE_NORMAL;
                }
                else if (opt[k] == OPT_noble_item_spawn_mode)
                {
                    op_ptr->noble_item_spawn_mode
                        = (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_RESTRICTED)
                        ? op_ptr->noble_item_spawn_mode - 1
                        : NOBLE_ITEM_SPAWN_RESTRICTED;
                }
                else if (opt[k] == OPT_intro_style)
                {
                    op_ptr->intro_style
                        = (op_ptr->intro_style > INTRO_STYLE_FLAME)
                        ? op_ptr->intro_style - 1
                        : INTRO_STYLE_FLAME;
                }
                else if (opt[k] == OPT_narrative_banner_turns)
                {
                    op_ptr->narrative_banner_turns =
                        (op_ptr->narrative_banner_turns > 0)
                        ? op_ptr->narrative_banner_turns - 1
                        : 0;
                }
                else if (opt[k] == OPT_mirror_player_tile_facing)
                {
                    byte mode = option_player_tile_facing_mode();
                    option_set_player_tile_facing_mode(
                        (mode > PLAYER_TILE_FACING_OFF)
                        ? (byte)(mode - 1)
                        : PLAYER_TILE_FACING_OFF);
                }
                else if (opt[k] == OPT_styled_monster_tile_health_bars)
                {
                    byte mode = option_monster_tile_health_bar_mode();
                    option_set_monster_tile_health_bar_mode(
                        (mode > MONSTER_TILE_HEALTH_BARS_SHOW)
                        ? (byte)(mode - 1)
                        : MONSTER_TILE_HEALTH_BARS_SHOW);
                }
                else
                {
                    op_ptr->opt[opt[k]] = false;
                    option_apply_side_effects(opt[k]);
                }

                if (is_sound_page)
                    sound_settings_dirty = true;
                else if (option_is_app_persistent(opt[k]))
                    app_settings_dirty = true;
                else if (metarun_page)
                    metarun_settings_dirty = true;
            }
            break;
        }

        case '0':
        {
            /* On an inline pane Font Size row, 0 resets to auto. */
            if (page == INTERFACE_PAGE && opt[k] >= IFACE_PANE_ROW_BASE)
            {
                const struct iface_pane_row* prow =
                    &pane_rows[opt[k] - IFACE_PANE_ROW_BASE];
                int pane_idx = iface_pane_row_live_index(prow);
                if (prow->field == IFACE_PANE_FIELD_FONT
                    && get_sdl_pane_font_size(pane_idx) != 0)
                {
                    set_sdl_pane_font_size(pane_idx, 0);
                    sdl_apply_config();
                    app_settings_dirty = true;
                }
                else if (prow->field == IFACE_PANE_FIELD_SIZE
                    && get_sdl_touch_top_panel_size()
                        != get_sdl_touch_top_panel_default_size())
                {
                    set_sdl_touch_top_panel_size(
                        get_sdl_touch_top_panel_default_size());
                    sdl_apply_config();
                    app_settings_dirty = true;
                }
                else if (prow->field == IFACE_PANE_FIELD_COLUMNS
                    && get_sdl_touch_top_panel_columns()
                        != get_sdl_touch_top_panel_default_columns())
                {
                    set_sdl_touch_top_panel_columns(
                        get_sdl_touch_top_panel_default_columns());
                    sdl_apply_config();
                    app_settings_dirty = true;
                }
                else if (prow->field == IFACE_PANE_FIELD_ROWS
                    && get_sdl_touch_top_panel_rows()
                        != get_sdl_touch_top_panel_default_rows())
                {
                    set_sdl_touch_top_panel_rows(
                        get_sdl_touch_top_panel_default_rows());
                    sdl_apply_config();
                    app_settings_dirty = true;
                }
                else if (prow->field == IFACE_PANE_FIELD_DICE_LOCK
                    && get_sdl_dice_roll_lock_ms()
                        != SDL_DICE_ROLL_LOCK_DEFAULT_MS)
                {
                    set_sdl_dice_roll_lock_ms(
                        SDL_DICE_ROLL_LOCK_DEFAULT_MS);
                    app_settings_dirty = true;
                }
                else if (prow->field == IFACE_PANE_FIELD_DICE_OVERLAY
                    && get_sdl_dice_roll_overlay_ms()
                        != SDL_DICE_ROLL_OVERLAY_DEFAULT_MS)
                {
                    set_sdl_dice_roll_overlay_ms(
                        SDL_DICE_ROLL_OVERLAY_DEFAULT_MS);
                    app_settings_dirty = true;
                }
                else if (prow->field == IFACE_PANE_FIELD_POPUP_NOTIFICATION
                    && get_sdl_popup_notification_ms()
                        != SDL_POPUP_NOTIFICATION_DEFAULT_MS)
                {
                    set_sdl_popup_notification_ms(
                        SDL_POPUP_NOTIFICATION_DEFAULT_MS);
                    app_settings_dirty = true;
                }
            }
            break;
        }

        default:
        {
            bell("Illegal command for normal options!");
            break;
        }
        }

        if (birth_fixed_exp && playerturn == 0 && p_ptr->exp != PY_FIXED_EXP)
        {
            int total_exp = PY_FIXED_EXP;
            p_ptr->new_exp = total_exp;
            p_ptr->exp = total_exp;
            check_experience();
            clear_skills_and_abilities();
        }
        else if (!birth_fixed_exp && playerturn == 0
            && p_ptr->exp >= PY_FIXED_EXP)
        {
            int total_exp = PY_START_EXP;
            p_ptr->new_exp = total_exp;
            p_ptr->exp = total_exp;
            check_experience();
            clear_skills_and_abilities();
        }
    }
}

/*
 * Display and manage SDL pane settings
 * Interactive menu to edit SDL configuration
 */
static int get_supporting_pane_config_count(void);
static void do_cmd_supporting_pane_layout_editor(bool* settings_changed);
static void do_cmd_supporting_pane_font_editor(bool* settings_changed);
static void do_cmd_touch_pane_button_editor(bool* settings_changed);
static void do_cmd_touch_button_settings(bool* settings_changed);
static void do_cmd_touch_profile_settings(bool* settings_changed);
static void touch_top_widget_format_size(char* buf, size_t buflen, float size);
static bool touch_top_widget_pick_size(void);
static void do_cmd_touch_settings(bool* settings_changed)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((unused))
#endif
    ;
static void do_cmd_touch_control_settings(bool* settings_changed);
static const char* pane_type_short_name(enum pane_type type);

static bool pane_settings_exposes_pane(enum pane_type type)
{
    return type != PANE_MAIN && type != PANE_TOUCH;
}

static bool pane_font_settings_exposes_pane(enum pane_type type)
{
    return pane_settings_exposes_pane(type);
}

static void format_font_size_value(char* buf, size_t buflen, int raw, int effective,
    int max_chars)
{
    char long_buf[24];
    char medium_buf[24];
    char short_buf[16];

    if (!buf || !buflen)
        return;

    if (raw > 0)
    {
        strnfmt(long_buf, sizeof(long_buf), "%d", raw);
        settings_ui_fit_text(buf, buflen, long_buf, max_chars);
        return;
    }

    strnfmt(long_buf, sizeof(long_buf), "auto (%d)", effective);
    strnfmt(medium_buf, sizeof(medium_buf), "auto %d", effective);
    strnfmt(short_buf, sizeof(short_buf), "a%d", effective);
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, long_buf, medium_buf, short_buf),
        max_chars);
}

static const char* sdl_min_terminal_mode_label(int mode)
{
    return (mode == 1) ? "compact (50x18)" : "normal (80x24)";
}

static const char* sdl_config_path_leaf(const char* path)
{
    const char* last_slash;
    const char* last_backslash;
    const char* leaf;

    if (!path || !path[0])
        return "sil_sdl.json";

    last_slash = strrchr(path, '/');
    last_backslash = strrchr(path, '\\');
    leaf = last_slash;
    if (!leaf || (last_backslash && last_backslash > leaf))
        leaf = last_backslash;

    return leaf ? (leaf + 1) : path;
}

static bool sdl_build_file_url(const char* path, char* buf, size_t buflen)
{
    size_t used;
    const char* prefix;

    if (!path || !path[0] || !buf || buflen < 16)
        return false;

    prefix = (path[0] == '/' || path[0] == '\\') ? "file://" : "file:///";
    SDL_strlcpy(buf, prefix, buflen);
    used = strlen(buf);

    for (const unsigned char* src = (const unsigned char*)path; *src; src++) {
        unsigned char ch = *src;
        char normalized = (ch == '\\') ? '/' : (char)ch;

        if (isalnum((unsigned char)normalized) || normalized == '-'
            || normalized == '_' || normalized == '.' || normalized == '~'
            || normalized == '/' || normalized == ':')
        {
            if (used + 1 >= buflen)
                return false;
            buf[used++] = normalized;
            buf[used] = '\0';
        } else {
            if (used + 3 >= buflen)
                return false;
            strnfmt(buf + used, buflen - used, "%%%02X", ch);
            used += 3;
        }
    }

    return true;
}

static void sdl_open_config_file(void)
{
    const char* config_path = get_sdl_config_path();
    char url[2048];

    if (!config_path || !config_path[0]) {
        bell("SDL config path is not available");
        return;
    }

    if (!sdl_build_file_url(config_path, url, sizeof(url))) {
        msg_format("Could not build file URL for %s",
            sdl_config_path_leaf(config_path));
        return;
    }

    if (!SDL_OpenURL(url)) {
        msg_format("Could not open %s (%s)",
            sdl_config_path_leaf(config_path), SDL_GetError());
        return;
    }

    msg_format("Opened %s", sdl_config_path_leaf(config_path));
}

void do_cmd_pane_settings(void)
{
    enum {
        PANE_SETTING_MIN_TERMINAL_SIZE = 0,
        PANE_SETTING_MAIN_VIEW_SCALE,
        PANE_SETTING_TERMINAL_MENU_SCALE_OFFSET,
        PANE_SETTING_MOBILE_STARTING_ZOOM_OFFSET,
#if defined(__ANDROID__) || defined(SIL_IOS)
        PANE_SETTING_MOBILE_PORTRAIT_MODE,
#endif
        PANE_SETTING_ENABLE_SIDE_PANES,
        PANE_SETTING_ENABLE_BOTTOM_PANES,
        PANE_SETTING_FULLSCREEN,
        PANE_SETTING_TILES,
        PANE_SETTING_USE_UNSAFE_AREA,
        PANE_SETTING_WHITE_PANE_BORDERS,
        PANE_SETTING_HIDE_FULLSCREEN_PANES,
        PANE_SETTING_AUX_VIEW_FONT_SIZE,
        PANE_SETTING_VIEW_PANE_CONFIGURATION,
        PANE_SETTING_PANE_FONT_SIZES,
        PANE_SETTING_OPEN_CONFIG_FILE,
        PANE_SETTING_RESET_ALL,
        PANE_SETTING_SAVE_RETURN,
        PANE_SETTING_COUNT
    };
    int k = 0;
    int n = PANE_SETTING_COUNT;
    bool pane_setting_visible[PANE_SETTING_COUNT];
    bool done = false;
    bool settings_changed = false;
    int dir;
    const char* config_path = get_sdl_config_path();
    const char* config_label = (config_path && config_path[0]) ? config_path : "sil_sdl.json";

    for (int i = 0; i < PANE_SETTING_COUNT; i++)
        pane_setting_visible[i] = true;
#if defined(__ANDROID__) || defined(SIL_IOS)
    /* Mobile uses the full display and supports overlay panes only.  Do not
     * offer ordinary pane/window controls whose values are fixed by the
     * platform policy. */
    pane_setting_visible[PANE_SETTING_ENABLE_SIDE_PANES] = false;
    pane_setting_visible[PANE_SETTING_ENABLE_BOTTOM_PANES] = false;
    pane_setting_visible[PANE_SETTING_FULLSCREEN] = false;
    pane_setting_visible[PANE_SETTING_WHITE_PANE_BORDERS] = false;
    pane_setting_visible[PANE_SETTING_HIDE_FULLSCREEN_PANES] = false;
    pane_setting_visible[PANE_SETTING_AUX_VIEW_FONT_SIZE] = false;
    pane_setting_visible[PANE_SETTING_VIEW_PANE_CONFIGURATION] = false;
    pane_setting_visible[PANE_SETTING_PANE_FONT_SIZES] = false;
#endif

    /* Save screen */
    screen_save();

    while (!done)
    {
        int row_width;
        int label_hint;
        settings_semantic_menu_begin("SDL Pane Settings", k);

        /* Display current settings */
        char buf[96];
        char value_buf[32];
        byte a;
        char font_value[24];
        row_width = settings_ui_line_width(2);
        label_hint = MAX(10, row_width - 12);
#define ADD_PANE_SETTING_ROW(CHOICE, OFFSET, ATTR, TEXT)                       \
        do {                                                                   \
            (void)(OFFSET);                                                    \
            if (pane_setting_visible[(CHOICE)]) {                              \
                char semantic_line[160];                                       \
                settings_semantic_line_from_menu_line(semantic_line,           \
                    sizeof(semantic_line), (TEXT));                            \
                settings_semantic_add_row((CHOICE), semantic_line, (ATTR));    \
                if ((CHOICE) < PANE_SETTING_VIEW_PANE_CONFIGURATION)           \
                    sdl_character_sheet_screen_set_last_select_row_reset(      \
                        SETTINGS_CLICK_RESET_ROW_BASE + (CHOICE));             \
            }                                                                  \
        } while (0)

        /* Option 0: Minimum Terminal Size */
        a = (k == PANE_SETTING_MIN_TERMINAL_SIZE) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Minimum Terminal Size",
                "Min Terminal Size",
                "Min Terminal"),
            sdl_min_terminal_mode_label(get_sdl_min_terminal_mode()),
            row_width, 10);
        ADD_PANE_SETTING_ROW(PANE_SETTING_MIN_TERMINAL_SIZE, 0, a, buf);

        /* Option 1: Main View Scale */
        a = (k == PANE_SETTING_MAIN_VIEW_SCALE) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(value_buf, sizeof(value_buf), "%d", get_sdl_main_view_scale());
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Main Terminal Scale (1-max)",
                "Main Terminal Scale",
                "View Scale"),
            value_buf, row_width, 3);
        ADD_PANE_SETTING_ROW(PANE_SETTING_MAIN_VIEW_SCALE, 1, a, buf);

        /* Option 2: terminal-menu scale relative to the layout maximum. */
        a = (k == PANE_SETTING_TERMINAL_MENU_SCALE_OFFSET)
            ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(value_buf, sizeof(value_buf), "%+d",
            get_sdl_terminal_menu_scale_offset());
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Terminal Menu Scale Offset",
                "Menu Scale Offset",
                "Menu Scale"),
            value_buf, row_width, 4);
        ADD_PANE_SETTING_ROW(PANE_SETTING_TERMINAL_MENU_SCALE_OFFSET, 2, a,
            buf);

        /* Option 3: extra zoom applied when mobile gameplay starts. */
        a = (k == PANE_SETTING_MOBILE_STARTING_ZOOM_OFFSET)
            ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(value_buf, sizeof(value_buf), "%+d",
            get_sdl_mobile_starting_zoom_offset());
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Mobile Starting Zoom Offset",
                "Mobile Start Zoom",
                "Start Zoom"),
            value_buf, row_width, 4);
        ADD_PANE_SETTING_ROW(PANE_SETTING_MOBILE_STARTING_ZOOM_OFFSET, 3, a,
            buf);

#if defined(__ANDROID__) || defined(SIL_IOS)
        a = (k == PANE_SETTING_MOBILE_PORTRAIT_MODE)
            ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Portrait Mode",
                "Portrait Mode",
                "Portrait"),
            get_sdl_mobile_portrait_mode() ? "yes" : "no",
            row_width, 3);
        ADD_PANE_SETTING_ROW(PANE_SETTING_MOBILE_PORTRAIT_MODE, 4,
            a, buf);
#endif

        /* Option 4: Enable Side Panes */
        a = (k == PANE_SETTING_ENABLE_SIDE_PANES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Enable Side Panes [Alt+I]",
                "Side Panes [Alt+I]",
                "Side Panes"),
            get_sdl_enable_right_panes() ? "yes" : "no",
            row_width, 3);
        ADD_PANE_SETTING_ROW(PANE_SETTING_ENABLE_SIDE_PANES, 4, a, buf);

        /* Option 5: Enable Bottom Panes */
        a = (k == PANE_SETTING_ENABLE_BOTTOM_PANES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Enable Bottom Panes [Alt+L]",
                "Bottom Panes [Alt+L]",
                "Bottom Panes"),
            get_sdl_enable_bottom_panes() ? "yes" : "no",
            row_width, 3);
        ADD_PANE_SETTING_ROW(PANE_SETTING_ENABLE_BOTTOM_PANES, 5, a, buf);

        /* Option 6: Fullscreen */
        a = (k == PANE_SETTING_FULLSCREEN) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf), "Fullscreen",
            get_sdl_fullscreen() ? "yes" : "no", row_width, 3);
        ADD_PANE_SETTING_ROW(PANE_SETTING_FULLSCREEN, 6, a, buf);

        /* Option 7: Tiles */
        a = (k == PANE_SETTING_TILES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf), "Tiles [Alt+A]",
            get_sdl_tiles() ? "yes" : "no", row_width, 3);
        ADD_PANE_SETTING_ROW(PANE_SETTING_TILES, 7, a, buf);

        /* Option 8: Use Unsafe Area */
        a = (k == PANE_SETTING_USE_UNSAFE_AREA) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Use Unsafe Area (notch/cutout area)",
                "Use Unsafe Area",
                "Unsafe Area"),
            get_sdl_use_unsafe_area() ? "yes" : "no",
            row_width, 3);
        ADD_PANE_SETTING_ROW(PANE_SETTING_USE_UNSAFE_AREA, 8, a, buf);

        /* Option 9: White Pane Borders */
        a = (k == PANE_SETTING_WHITE_PANE_BORDERS) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "White Pane Borders",
                "White Pane Borders",
                "White Borders"),
            get_sdl_show_pane_borders() ? "white" : "black",
            row_width, 5);
        ADD_PANE_SETTING_ROW(PANE_SETTING_WHITE_PANE_BORDERS, 9, a, buf);

        /* Option 10: Hide supporting panes on full-screen screens */
        a = (k == PANE_SETTING_HIDE_FULLSCREEN_PANES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Hide supporting panes on full-screen screens",
                "Hide panes on full-screen screens",
                "Hide panes on full-screen"),
            op_ptr->opt[OPT_hide_supporting_panes_fullscreen] ? "yes" : "no",
            row_width, 3);
        ADD_PANE_SETTING_ROW(PANE_SETTING_HIDE_FULLSCREEN_PANES, 10, a, buf);

        /* Option 11: Aux View Font Size */
        a = (k == PANE_SETTING_AUX_VIEW_FONT_SIZE) ? TERM_L_BLUE : TERM_WHITE;
        format_font_size_value(font_value, sizeof(font_value),
            get_sdl_aux_view_font_size(), get_sdl_effective_aux_view_font_size(),
            MAX(6, MIN(14, row_width / 2)));
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Default Aux Font Size (0=auto, 8-48)",
                "Default Aux Font (0=auto)",
                "Aux Font"),
            font_value, row_width, 6);
        ADD_PANE_SETTING_ROW(PANE_SETTING_AUX_VIEW_FONT_SIZE, 11, a, buf);

        /* Option 12: View Pane Configuration (supporting panes only) */
        a = (k == PANE_SETTING_VIEW_PANE_CONFIGURATION) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%s (%d)",
            settings_ui_pick_label(row_width,
                "View Pane Configuration",
                "Pane Configuration",
                "Pane Layout"),
            get_supporting_pane_config_count());
        {
            char fitted_buf[96];
            settings_ui_fit_text(fitted_buf, sizeof(fitted_buf), buf, row_width);
            SDL_strlcpy(buf, fitted_buf, sizeof(buf));
        }
        ADD_PANE_SETTING_ROW(PANE_SETTING_VIEW_PANE_CONFIGURATION, 12, a, buf);

        /* Option 13: Pane Font Sizes */
        a = (k == PANE_SETTING_PANE_FONT_SIZES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_fit_text(buf, sizeof(buf),
            settings_ui_pick_label(row_width,
                "Pane Font Sizes",
                "Pane Fonts",
                "Pane Fonts"),
            row_width);
        ADD_PANE_SETTING_ROW(PANE_SETTING_PANE_FONT_SIZES, 13, a, buf);

        /* Option 14: Open SDL Config File */
        a = (k == PANE_SETTING_OPEN_CONFIG_FILE) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Open SDL Config File",
                "Open SDL Config",
                "Open Config"),
            sdl_config_path_leaf(config_label), row_width, 12);
        ADD_PANE_SETTING_ROW(PANE_SETTING_OPEN_CONFIG_FILE, 14, a, buf);

        /* Option 15: Reset all interface settings */
        a = (k == PANE_SETTING_RESET_ALL) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_fit_text(buf, sizeof(buf),
            settings_ui_pick_label(row_width,
                "Reset All Interface Settings",
                "Reset All Settings",
                "Reset All"),
            row_width);
        ADD_PANE_SETTING_ROW(PANE_SETTING_RESET_ALL, 15, a, buf);

        /* Option 16: Return (changes are saved automatically on exit). */
        a = (k == PANE_SETTING_SAVE_RETURN) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_fit_text(buf, sizeof(buf),
            "Return to Options Menu",
            row_width);
        ADD_PANE_SETTING_ROW(PANE_SETTING_SAVE_RETURN, 16, a, buf);

#undef ADD_PANE_SETTING_ROW

        /* Display help: describe the focused setting (empty band when a row
         * has no description yet). */
        {
            static const char* const pane_setting_desc[PANE_SETTING_COUNT] = {
                [PANE_SETTING_MIN_TERMINAL_SIZE] =
                    "Smallest character grid the game will use. Larger minimums "
                    "keep text big but leave less room for side and bottom "
                    "panes on small windows.",
                [PANE_SETTING_MAIN_VIEW_SCALE] =
                    "Zoom level of the main map. Higher values enlarge the map "
                    "and its font but show less of the level at once.",
                [PANE_SETTING_TERMINAL_MENU_SCALE_OFFSET] =
                    "Scale of full-screen terminal menus relative to the "
                    "largest scale that fits. Set to 0 for the maximum; -1 "
                    "uses one step below it.",
                [PANE_SETTING_MOBILE_STARTING_ZOOM_OFFSET] =
                    "Extra zoom steps applied when gameplay starts on mobile. "
                    "Set to 0 to start at the configured main-map scale. The "
                    "result is limited to what the screen can display.",
#if defined(__ANDROID__) || defined(SIL_IOS)
                [PANE_SETTING_MOBILE_PORTRAIT_MODE] =
                    "Use the device's real portrait orientation. Turning it "
                    "off returns the game to landscape. Pane and overlay "
                    "layouts are saved separately for portrait and landscape.",
#endif
                [PANE_SETTING_ENABLE_SIDE_PANES] =
                    "Show panes to the side of the map (inventory, monster "
                    "list, and more). Also toggled in play with Alt+I.",
                [PANE_SETTING_ENABLE_BOTTOM_PANES] =
                    "Show panes below the map (messages, combat rolls, and "
                    "more). Also toggled in play with Alt+L.",
                [PANE_SETTING_FULLSCREEN] =
                    "Run the game fullscreen instead of in a window.",
                [PANE_SETTING_TILES] =
                    "Draw the map with graphical tiles instead of letters. "
                    "Also toggled in play with Alt+A.",
                [PANE_SETTING_USE_UNSAFE_AREA] =
                    "Let the display extend into a screen notch or rounded "
                    "cutout. Off keeps everything within the safe area.",
                [PANE_SETTING_WHITE_PANE_BORDERS] =
                    "Use white rather than black separator lines for ordinary "
                    "panes. Configure the overlay-log border in Interface > "
                    "Overlay Log.",
                [PANE_SETTING_HIDE_FULLSCREEN_PANES] =
                    "On full-screen menus (inventory, character sheet, and the "
                    "like), hide the supporting panes to reduce clutter.",
                [PANE_SETTING_AUX_VIEW_FONT_SIZE] =
                    "Font size for the supporting panes. Set to 0 (auto) to let "
                    "the game pick a size that fits.",
                [PANE_SETTING_VIEW_PANE_CONFIGURATION] =
                    "Open the layout editor to move and resize panes for the "
                    "current orientation. Portrait and landscape layouts are "
                    "saved separately; the number in brackets is how many "
                    "panes are configured.",
                [PANE_SETTING_PANE_FONT_SIZES] =
                    "Set the font size of each pane individually.",
                [PANE_SETTING_OPEN_CONFIG_FILE] =
                    "Open the raw sil_sdl.json config file in your system's "
                    "default editor.",
                [PANE_SETTING_RESET_ALL] =
                    "Reset the complete interface configuration at once, "
                    "including pane layout, fonts, controls, display, and "
                    "interface options.",
                [PANE_SETTING_SAVE_RETURN] =
                    "Return to the Options menu. Changes are saved "
                    "automatically.",
            };
            cptr d = (k >= 0 && k < PANE_SETTING_COUNT)
                ? pane_setting_desc[k] : NULL;

            sdl_character_sheet_screen_set_select_description(d ? d : "");
            sdl_character_sheet_screen_commit_select(k);
        }
        /* Get key */
        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                }
                else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice < SETTINGS_CLICK_RESET_ROW_BASE + n)
                {
                    struct sdl_config def;

                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    k = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    sdl_config_set_defaults(&def);
                    switch (k)
                    {
                    case PANE_SETTING_MIN_TERMINAL_SIZE:
                        set_sdl_min_terminal_mode(def.min_terminal_mode);
                        break;
                    case PANE_SETTING_MAIN_VIEW_SCALE:
                        set_sdl_main_view_scale(get_sdl_max_scale());
                        break;
                    case PANE_SETTING_TERMINAL_MENU_SCALE_OFFSET:
                        set_sdl_terminal_menu_scale_offset(
                            def.terminal_menu_scale_offset);
                        break;
                    case PANE_SETTING_MOBILE_STARTING_ZOOM_OFFSET:
                        set_sdl_mobile_starting_zoom_offset(
                            def.mobile_starting_zoom_offset);
                        break;
#if defined(__ANDROID__) || defined(SIL_IOS)
                    case PANE_SETTING_MOBILE_PORTRAIT_MODE:
                        set_sdl_mobile_portrait_mode(def.mobile_portrait_mode);
                        break;
#endif
                    case PANE_SETTING_ENABLE_SIDE_PANES:
                        set_sdl_enable_right_panes(def.enable_right_panes);
                        break;
                    case PANE_SETTING_ENABLE_BOTTOM_PANES:
                        set_sdl_enable_bottom_panes(def.enable_bottom_panes);
                        break;
                    case PANE_SETTING_FULLSCREEN:
                        set_sdl_fullscreen(def.fullscreen);
                        break;
                    case PANE_SETTING_TILES:
                        set_sdl_tiles(def.tiles);
                        break;
                    case PANE_SETTING_USE_UNSAFE_AREA:
                        set_sdl_use_unsafe_area(def.use_unsafe_area);
                        break;
                    case PANE_SETTING_WHITE_PANE_BORDERS:
                        set_sdl_show_pane_borders(def.show_pane_borders);
                        break;
                    case PANE_SETTING_HIDE_FULLSCREEN_PANES:
                        op_ptr->opt[OPT_hide_supporting_panes_fullscreen] =
                            option_norm[OPT_hide_supporting_panes_fullscreen];
                        break;
                    case PANE_SETTING_AUX_VIEW_FONT_SIZE:
                        set_sdl_aux_view_font_size(0);
                        break;
                    default:
                        break;
                    }
                    settings_changed = true;
                    sdl_apply_config();
                    continue;
                }
                else if (clicked_choice >= 0 && clicked_choice < n)
                {
                    bool was_current = (clicked_choice == k);
                    bool touch_primary = sdl_touch_only_device_active()
                        && click_action == UI_MENU_CLICK_PRIMARY;

                    k = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    if (click_action == UI_MENU_CLICK_SECONDARY)
                        ch = '4';
                    else if (was_current || touch_primary)
                        ch = ' ';
                    else
                        continue;
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, 0, 0, click_generated);
        }

        /* Try to translate the key into a direction */
        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        /* Process input */
        switch (ch)
        {
        case ESCAPE:
        {
            /* Exit without needing to navigate to the bottom */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            /* Enter activates the current option for actions; otherwise accept/exit. */
            if (k == PANE_SETTING_VIEW_PANE_CONFIGURATION) /* Supporting Pane Layout */
            {
                settings_semantic_menu_hide();
                do_cmd_supporting_pane_layout_editor(&settings_changed);
                break;
            }
            if (k == PANE_SETTING_PANE_FONT_SIZES) /* Pane Font Sizes */
            {
                settings_semantic_menu_hide();
                do_cmd_supporting_pane_font_editor(&settings_changed);
                break;
            }
            if (k == PANE_SETTING_OPEN_CONFIG_FILE) /* Open SDL Config File */
            {
                settings_semantic_menu_hide();
                sdl_open_config_file();
                break;
            }
            if (k == PANE_SETTING_RESET_ALL)
            {
                settings_semantic_menu_hide();
                if (get_check("Reset all interface settings to defaults? "))
                {
                    sdl_reset_interface_settings_to_defaults();
                    settings_changed = false;
                    msg_print("All interface settings reset to defaults.");
                }
                break;
            }

            /* Save if changed, then exit */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }

        case '-':
        case '8':
        {
            /* Move up */
            do {
                k = (n + k - 1) % n;
            } while (!pane_setting_visible[k]);
            break;
        }

        case '2':
        {
            /* Move down */
            do {
                k = (k + 1) % n;
            } while (!pane_setting_visible[k]);
            break;
        }

        case '0':
        {
            if (k == PANE_SETTING_AUX_VIEW_FONT_SIZE)
            {
                if (get_sdl_aux_view_font_size() != 0)
                {
                    set_sdl_aux_view_font_size(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else
            {
                bell("0 sets the default aux font to auto");
            }
            break;
        }

        case 't':
        case '5':
        case ' ':
        {
            /* Toggle or activate current option */
            if (k == PANE_SETTING_MAIN_VIEW_SCALE)
            {
                int old_value = get_sdl_main_view_scale();
                int value = old_value;

                if (settings_pick_integer_range("Main Terminal Scale", NULL,
                        get_sdl_min_main_view_scale(), get_sdl_max_scale(),
                        old_value, false, &value)
                    && value != old_value)
                {
                    set_sdl_main_view_scale(value);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == PANE_SETTING_TERMINAL_MENU_SCALE_OFFSET)
            {
                int old_value = get_sdl_terminal_menu_scale_offset();
                int value = old_value;

                if (settings_pick_integer_range("Terminal Menu Scale Offset",
                        NULL,
                        SDL_TERMINAL_MENU_SCALE_OFFSET_MIN,
                        SDL_TERMINAL_MENU_SCALE_OFFSET_MAX, old_value, true,
                        &value)
                    && value != old_value)
                {
                    set_sdl_terminal_menu_scale_offset(value);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == PANE_SETTING_MOBILE_STARTING_ZOOM_OFFSET)
            {
                int old_value = get_sdl_mobile_starting_zoom_offset();
                int value = old_value;

                if (settings_pick_integer_range("Mobile Starting Zoom Offset",
                        NULL,
                        SDL_MOBILE_STARTING_ZOOM_OFFSET_MIN,
                        SDL_MOBILE_STARTING_ZOOM_OFFSET_MAX, old_value, true,
                        &value)
                    && value != old_value)
                {
                    set_sdl_mobile_starting_zoom_offset(value);
                    settings_changed = true;
                }
            }
            else if (k == PANE_SETTING_ENABLE_SIDE_PANES)
            {
                set_sdl_enable_right_panes(!get_sdl_enable_right_panes());
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == PANE_SETTING_ENABLE_BOTTOM_PANES)
            {
                set_sdl_enable_bottom_panes(!get_sdl_enable_bottom_panes());
                settings_changed = true;
                sdl_apply_config();
            }
#if defined(__ANDROID__) || defined(SIL_IOS)
            else if (k == PANE_SETTING_MOBILE_PORTRAIT_MODE)
            {
                set_sdl_mobile_portrait_mode(!get_sdl_mobile_portrait_mode());
                settings_changed = true;
            }
#endif
            else if (k == PANE_SETTING_FULLSCREEN)
            {
                set_sdl_fullscreen(!get_sdl_fullscreen());
                settings_changed = true;
            }
            else if (k == PANE_SETTING_TILES)
            {
                set_sdl_tiles(!get_sdl_tiles());
                settings_changed = true;
            }
            else if (k == PANE_SETTING_USE_UNSAFE_AREA)
            {
                set_sdl_use_unsafe_area(!get_sdl_use_unsafe_area());
                settings_changed = true;
            }
            else if (k == PANE_SETTING_WHITE_PANE_BORDERS)
            {
                set_sdl_show_pane_borders(!get_sdl_show_pane_borders());
                settings_changed = true;
                sdl_request_redraw();
            }
            else if (k == PANE_SETTING_HIDE_FULLSCREEN_PANES)
            {
                op_ptr->opt[OPT_hide_supporting_panes_fullscreen]
                    = !op_ptr->opt[OPT_hide_supporting_panes_fullscreen];
                settings_changed = true;
                sdl_refresh_supporting_panes_layout();
            }
            else if (k == PANE_SETTING_MIN_TERMINAL_SIZE) /* Minimum Terminal Size */
            {
                set_sdl_min_terminal_mode(get_sdl_min_terminal_mode() == 0 ? 1 : 0);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == PANE_SETTING_VIEW_PANE_CONFIGURATION) /* Supporting Pane Layout */
            {
                settings_semantic_menu_hide();
                do_cmd_supporting_pane_layout_editor(&settings_changed);
            }
            else if (k == PANE_SETTING_PANE_FONT_SIZES) /* Pane Font Sizes */
            {
                settings_semantic_menu_hide();
                do_cmd_supporting_pane_font_editor(&settings_changed);
            }
            else if (k == PANE_SETTING_OPEN_CONFIG_FILE) /* Open SDL Config File */
            {
                settings_semantic_menu_hide();
                sdl_open_config_file();
            }
            else if (k == PANE_SETTING_RESET_ALL)
            {
                settings_semantic_menu_hide();
                if (get_check("Reset all interface settings to defaults? "))
                {
                    sdl_reset_interface_settings_to_defaults();
                    settings_changed = false;
                    msg_print("All interface settings reset to defaults.");
                }
            }
            else if (k == PANE_SETTING_SAVE_RETURN) /* Save/Return */
            {
                if (settings_changed)
                {
                    if (save_pane_config_to_json())
                    {
                        msg_format("Settings saved to %s", config_label);
                    }
                }
                done = true;
            }
            break;
        }

        case 'y':
        case '6':
        {
            /* Increase value or set to yes */
            int val;

            if (k == PANE_SETTING_MAIN_VIEW_SCALE) /* Main View Scale */
            {
                val = get_sdl_main_view_scale();
                int max_scale = get_sdl_max_scale();
                if (val < max_scale)
                {
                    set_sdl_main_view_scale(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == PANE_SETTING_TERMINAL_MENU_SCALE_OFFSET)
            {
                val = get_sdl_terminal_menu_scale_offset();
                if (val < SDL_TERMINAL_MENU_SCALE_OFFSET_MAX)
                {
                    set_sdl_terminal_menu_scale_offset(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == PANE_SETTING_MOBILE_STARTING_ZOOM_OFFSET)
            {
                val = get_sdl_mobile_starting_zoom_offset();
                if (val < SDL_MOBILE_STARTING_ZOOM_OFFSET_MAX)
                {
                    set_sdl_mobile_starting_zoom_offset(val + 1);
                    settings_changed = true;
                }
            }
#if defined(__ANDROID__) || defined(SIL_IOS)
            else if (k == PANE_SETTING_MOBILE_PORTRAIT_MODE)
            {
                if (!get_sdl_mobile_portrait_mode())
                {
                    set_sdl_mobile_portrait_mode(true);
                    settings_changed = true;
                }
            }
#endif
            else if (k == PANE_SETTING_ENABLE_SIDE_PANES) /* Enable Side Panes */
            {
                set_sdl_enable_right_panes(true);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == PANE_SETTING_ENABLE_BOTTOM_PANES) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(true);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == PANE_SETTING_FULLSCREEN) /* Fullscreen */
            {
                set_sdl_fullscreen(true);
                settings_changed = true;
            }
            else if (k == PANE_SETTING_TILES) /* Tiles */
            {
                set_sdl_tiles(true);
                settings_changed = true;
            }
            else if (k == PANE_SETTING_USE_UNSAFE_AREA) /* Use Unsafe Area */
            {
                set_sdl_use_unsafe_area(true);
                settings_changed = true;
            }
            else if (k == PANE_SETTING_WHITE_PANE_BORDERS) /* White Pane Borders */
            {
                set_sdl_show_pane_borders(true);
                settings_changed = true;
                sdl_request_redraw();
            }
            else if (k == PANE_SETTING_HIDE_FULLSCREEN_PANES) /* Hide panes on full-screen screens */
            {
                op_ptr->opt[OPT_hide_supporting_panes_fullscreen] = true;
                settings_changed = true;
                sdl_refresh_supporting_panes_layout();
            }
            else if (k == PANE_SETTING_MIN_TERMINAL_SIZE) /* Minimum Terminal Size */
            {
                if (get_sdl_min_terminal_mode() != 0)
                {
                    set_sdl_min_terminal_mode(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == PANE_SETTING_AUX_VIEW_FONT_SIZE) /* Aux View Font Size */
            {
                val = get_sdl_aux_view_font_size();
                if (val == 0)
                {
                    set_sdl_aux_view_font_size(get_sdl_effective_aux_view_font_size());
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val < 48)
                {
                    set_sdl_aux_view_font_size(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            break;
        }

        case 'n':
        case '4':
        {
            /* Decrease value or set to no */
            int val;

            if (k == PANE_SETTING_MAIN_VIEW_SCALE) /* Main View Scale */
            {
                val = get_sdl_main_view_scale();
                if (val > get_sdl_min_main_view_scale())
                {
                    set_sdl_main_view_scale(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == PANE_SETTING_TERMINAL_MENU_SCALE_OFFSET)
            {
                val = get_sdl_terminal_menu_scale_offset();
                if (val > SDL_TERMINAL_MENU_SCALE_OFFSET_MIN)
                {
                    set_sdl_terminal_menu_scale_offset(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == PANE_SETTING_MOBILE_STARTING_ZOOM_OFFSET)
            {
                val = get_sdl_mobile_starting_zoom_offset();
                if (val > SDL_MOBILE_STARTING_ZOOM_OFFSET_MIN)
                {
                    set_sdl_mobile_starting_zoom_offset(val - 1);
                    settings_changed = true;
                }
            }
#if defined(__ANDROID__) || defined(SIL_IOS)
            else if (k == PANE_SETTING_MOBILE_PORTRAIT_MODE)
            {
                if (get_sdl_mobile_portrait_mode())
                {
                    set_sdl_mobile_portrait_mode(false);
                    settings_changed = true;
                }
            }
#endif
            else if (k == PANE_SETTING_ENABLE_SIDE_PANES) /* Enable Side Panes */
            {
                set_sdl_enable_right_panes(false);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == PANE_SETTING_ENABLE_BOTTOM_PANES) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(false);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == PANE_SETTING_FULLSCREEN) /* Fullscreen */
            {
                set_sdl_fullscreen(false);
                settings_changed = true;
            }
            else if (k == PANE_SETTING_TILES) /* Tiles */
            {
                set_sdl_tiles(false);
                settings_changed = true;
            }
            else if (k == PANE_SETTING_USE_UNSAFE_AREA) /* Use Unsafe Area */
            {
                set_sdl_use_unsafe_area(false);
                settings_changed = true;
            }
            else if (k == PANE_SETTING_WHITE_PANE_BORDERS) /* White Pane Borders */
            {
                set_sdl_show_pane_borders(false);
                settings_changed = true;
                sdl_request_redraw();
            }
            else if (k == PANE_SETTING_HIDE_FULLSCREEN_PANES) /* Hide panes on full-screen screens */
            {
                op_ptr->opt[OPT_hide_supporting_panes_fullscreen] = false;
                settings_changed = true;
                sdl_refresh_supporting_panes_layout();
            }
            else if (k == PANE_SETTING_MIN_TERMINAL_SIZE) /* Minimum Terminal Size */
            {
                if (get_sdl_min_terminal_mode() != 1)
                {
                    set_sdl_min_terminal_mode(1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == PANE_SETTING_AUX_VIEW_FONT_SIZE) /* Aux View Font Size */
            {
                val = get_sdl_aux_view_font_size();
                if (val == 0)
                {
                    set_sdl_aux_view_font_size(get_sdl_effective_aux_view_font_size());
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val > 8)
                {
                    set_sdl_aux_view_font_size(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            break;
        }

        case 'o':
        case 'O':
        {
            settings_semantic_menu_hide();
            sdl_open_config_file();
            break;
        }

        default:
        {
            bell("Illegal command for pane settings!");
            break;
        }
        }
    }

    /* Restore screen */
    settings_semantic_menu_hide();
    screen_load();
}


static const char* pane_type_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INVENTORY";
    case PANE_SUPPLY: return "SUPPLY";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHARACTER";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MONSTERS";
    case PANE_MAP: return "MAP";
    case PANE_TOUCH: return "TOUCH";
    case PANE_LEFT_PANEL: return "LEFT_PANEL";
    case PANE_STATUS: return "STATUS";
    case PANE_DEPTH: return "DEPTH";
    case PANE_DESCRIPTION: return "DESCRIPTION";
    case PANE_OVERLAY_MENU: return "OVERLAY_MENU";
    case PANE_COMBAT: return "COMBAT";
    case PANE_STATUS_DEPTH: return "STATUS_DEPTH";
    default: return "UNKNOWN";
    }
}

static void do_cmd_supporting_pane_font_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = MAX_PANE_CONFIGS };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (!pane_font_settings_exposes_pane(type))
            continue;
        if (pane_type_is_overlay(type))
            continue;
        pane_indices[pane_count++] = i;
    }

    screen_save();

    if (pane_count <= 0)
    {
        settings_semantic_menu_begin("Pane Fonts", -1);
        sdl_character_sheet_screen_set_select_description(
            "No configurable panes are configured.");
        sdl_character_sheet_screen_commit_select(-1);
        (void)inkey();
        settings_semantic_menu_hide();
        screen_load();
        return;
    }

    {
        int sel = 0;
        bool done = false;
        bool changed = false;
        int dir;

        while (!done)
        {
            int row_width;
            settings_semantic_menu_begin("Pane Fonts", sel);
            row_width = settings_ui_line_width(2);

            for (int i = 0; i < pane_count; i++)
            {
                int idx = pane_indices[i];
                enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
                bool enabled = get_sdl_pane_enabled(idx);
                int raw_font = get_sdl_pane_font_size(idx);
                int effective_font = get_sdl_pane_effective_font_size(idx);
                byte a = (i == sel) ? TERM_L_BLUE : (enabled ? TERM_WHITE : TERM_SLATE);
                char line_buf[96];
                char label_buf[48];
                char font_value[24];
                char font_field[28];
                const char* type_label = settings_ui_pick_label(MAX(8, row_width / 2),
                    pane_type_name(type), pane_type_name(type),
                    pane_type_short_name(type));

                format_font_size_value(font_value, sizeof(font_value), raw_font,
                    effective_font, MAX(6, MIN(14, row_width / 2)));
                settings_ui_format_field(font_field, sizeof(font_field), font_value,
                    i == sel);
                strnfmt(label_buf, sizeof(label_buf), "%s %s", type_label,
                    enabled ? "on" : "off");
                settings_ui_format_pair_line(line_buf, sizeof(line_buf), label_buf,
                    font_field, row_width, 6);
                {
                    char semantic_line[160];

                    settings_semantic_line_from_menu_line(semantic_line,
                        sizeof(semantic_line), line_buf);
                    settings_semantic_add_row(i, semantic_line, a);
                    sdl_character_sheet_screen_set_last_select_row_reset(
                        SETTINGS_CLICK_RESET_ROW_BASE + i);
                }
            }

            {
                char fdesc[160];
                enum pane_type ftype =
                    (enum pane_type)get_sdl_pane_type(pane_indices[sel]);

                strnfmt(fdesc, sizeof(fdesc),
                    "Font size for the %s pane. Left/Right adjusts it; 0 (or "
                    "the row's Reset) uses the automatic size.",
                    pane_type_name(ftype));
                sdl_character_sheet_screen_set_select_description(fdesc);
                sdl_character_sheet_screen_commit_select(sel);
            }
            hide_cursor = true;
            char ch = inkey();
            hide_cursor = false;

            {
                int clicked_choice = 0;
                int click_action = UI_MENU_CLICK_PRIMARY;
                bool click_generated = false;

                if (ui_menu_click_take_action(&clicked_choice, &click_action))
                {
                    if (clicked_choice == SETTINGS_CLICK_RETURN)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        ch = ESCAPE;
                        click_generated = true;
                    }
                    else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                        && clicked_choice
                            < SETTINGS_CLICK_RESET_ROW_BASE + pane_count)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        sel = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                        ch = '0';
                        click_generated = true;
                    }
                    else if (clicked_choice >= 0 && clicked_choice < pane_count)
                    {
                        if (clicked_choice != sel)
                        {
                            sel = clicked_choice;
                            if (click_action == UI_MENU_CLICK_HOVER)
                                continue;
                            if (click_action == UI_MENU_CLICK_PRIMARY)
                                continue;
                        }
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        ch = (click_action == UI_MENU_CLICK_SECONDARY) ? '4' : '6';
                        click_generated = true;
                    }
                }

                ch = (char)settings_menu_key(ch, 0, 0, click_generated);
            }

            dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);

            switch (ch)
            {
            case ESCAPE:
            case '\n':
            case '\r':
                done = true;
                break;

            case '-':
            case '8':
                sel = (pane_count + sel - 1) % pane_count;
                break;

            case '2':
                sel = (sel + 1) % pane_count;
                break;

            case '0':
            {
                int idx = pane_indices[sel];
                if (get_sdl_pane_font_size(idx) != 0)
                {
                    set_sdl_pane_font_size(idx, 0);
                    changed = true;
                    sdl_apply_config();
                }
                break;
            }

            case 'n':
            case '4':
            case 'y':
            case '6':
            {
                int idx = pane_indices[sel];
                int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
                int value = get_sdl_pane_font_size(idx);

                if (value == 0)
                    set_sdl_pane_font_size(idx, get_sdl_pane_effective_font_size(idx));
                else
                    set_sdl_pane_font_size(idx, value + delta);

                changed = true;
                sdl_apply_config();
                break;
            }

            default:
                bell("Illegal command for pane font editor!");
                break;
            }
        }

        if (changed && settings_changed)
            *settings_changed = true;
    }

    settings_semantic_menu_hide();
    screen_load();
}

static const char* pane_type_short_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INV";
    case PANE_SUPPLY: return "SUP";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHAR";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MON";
    case PANE_MAP: return "MAP";
    case PANE_TOUCH: return "TOUCH";
    case PANE_LEFT_PANEL: return "LEFT";
    case PANE_STATUS: return "STAT";
    case PANE_DEPTH: return "DEPTH";
    case PANE_DESCRIPTION: return "DESC";
    case PANE_OVERLAY_MENU: return "MENU";
    case PANE_COMBAT: return "COM";
    case PANE_STATUS_DEPTH: return "STAT+DEPTH";
    default: return "UNK";
    }
}

static const char* pane_where_short_name(enum pane_placement where)
{
    switch (where)
    {
    case PLACE_RIGHT: return "R";
    case PLACE_LEFT: return "L";
    case PLACE_TOP_RIGHT: return "TR";
    case PLACE_TOP_CENTER: return "TC";
    case PLACE_TOP_LEFT: return "TL";
    case PLACE_BOTTOM_RIGHT: return "BR";
    case PLACE_BOTTOM_CENTER: return "BC";
    case PLACE_BOTTOM_LEFT: return "BL";
    case PLACE_LEFT_CENTER: return "LC";
    case PLACE_RIGHT_CENTER: return "RC";
    case PLACE_DOUBLE_RIGHT: return "DR";
    case PLACE_DOUBLE_LEFT: return "DL";
    case PLACE_BOTTOM: return "BOT";
    case PLACE_DOUBLE_BOTTOM: return "DB";
    default: return "?";
    }
}

static int get_supporting_pane_config_count(void)
{
    int count = 0;
    int total = get_pane_config_count();
    for (int i = 0; i < total; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (pane_settings_exposes_pane(type) && !pane_type_is_overlay(type))
            count++;
    }
    return count;
}

static int supporting_pane_master_idx(const int* pane_indices, int pane_count,
    enum pane_placement where)
{
    int fallback = -1;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        if ((enum pane_type)get_sdl_pane_type(idx) == PANE_LEFT_PANEL)
            continue;
        if ((enum pane_placement)get_sdl_pane_where(idx) != where)
            continue;
        if (fallback < 0)
            fallback = idx;
        if (get_sdl_pane_enabled(idx))
            return idx;
    }

    return fallback;
}

static bool supporting_pane_rows_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    if ((enum pane_type)get_sdl_pane_type(idx) == PANE_LEFT_PANEL)
        return true;
    if (pane_placement_is_overlay(where))
        return false;

    return (pane_placement_is_bottom(where) && idx != master_idx);
}

static bool supporting_pane_enabled_locked(int idx)
{
    return (enum pane_type)get_sdl_pane_type(idx) == PANE_LEFT_PANEL;
}

static bool supporting_pane_where_locked(int idx)
{
    (void)idx;
    return false;
}

static bool supporting_pane_cols_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    if ((enum pane_type)get_sdl_pane_type(idx) == PANE_LEFT_PANEL)
        return true;
    if (pane_placement_is_overlay(where))
        return false;

    return (pane_placement_is_side(where) && idx != master_idx);
}

static void supporting_pane_ensure_editable_field(int* field, const int* pane_indices,
    int pane_count, int sel)
{
    int idx;

    if (!field || pane_count <= 0 || sel < 0 || sel >= pane_count)
        return;

    idx = pane_indices[sel];
    while ((*field == 0 && supporting_pane_enabled_locked(idx))
        || (*field == 1 && supporting_pane_where_locked(idx))
        || (*field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
        || (*field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx)))
    {
        *field = (*field + 1) % 4;
    }
}

static bool supporting_pane_normalize_shared_sizes(const int* pane_indices, int pane_count)
{
    bool changed = false;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
        enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
        int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

        if (type == PANE_LEFT_PANEL)
            continue;
        if (pane_placement_is_overlay(where))
            continue;
        if (pane_placement_is_bottom(where) && idx != master_idx
            && get_sdl_pane_rows(idx) != 0)
        {
            set_sdl_pane_rows(idx, 0);
            changed = true;
        }
        else if (pane_placement_is_side(where) && idx != master_idx
            && get_sdl_pane_cols(idx) != 0)
        {
            set_sdl_pane_cols(idx, 0);
            changed = true;
        }
    }

    return changed;
}

static bool pane_type_is_overlay(enum pane_type type)
{
    return (type == PANE_LEFT_PANEL) || (type == PANE_STATUS)
        || (type == PANE_DEPTH) || (type == PANE_ROLLS)
        || (type == PANE_OVERLAY_MENU) || (type == PANE_COMBAT)
        || (type == PANE_STATUS_DEPTH);
}

static const char* pane_type_display_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_SUPPLY: return "Supply";
    case PANE_LEFT_PANEL: return "Left Panel";
    case PANE_STATUS: return "Status";
    case PANE_DEPTH: return "Depth";
    case PANE_ROLLS: return "Overlay Log";
    case PANE_OVERLAY_MENU: return "Quick Access";
    case PANE_COMBAT: return "Combat";
    case PANE_STATUS_DEPTH: return "Status & Depth";
    case PANE_LOG: return "Log";
    default: return pane_type_name(type);
    }
}

/*
 * Build the inline SDL rows (and their group headers) shown on the Interface
 * options page.  Global interface controls come first, followed by one section
 * per configured overlay pane and the remaining overlay timing controls.
 *
 * Returns the number of rows written.
 */
static int build_interface_pane_rows(struct iface_pane_row* rows, int max_rows,
    struct option_group_marker* markers, int* marker_count)
{
    int row_count = 0;
    int mark_count = 0;
    int total = get_pane_config_count();

    if (row_count + 1 <= max_rows)
    {
        rows[row_count].pane_cfg_index = -1;
        rows[row_count].type = PANE_MAX;
        rows[row_count].field = IFACE_PANE_FIELD_CAMERA_CENTER_CLEARANCE;
        markers[mark_count].setting_id = IFACE_PANE_ROW_BASE + row_count;
        markers[mark_count].label = "Camera";
        mark_count++;
        row_count++;
    }

    if (row_count + 1 <= max_rows)
    {
        rows[row_count].pane_cfg_index = -1;
        rows[row_count].type = PANE_MAX;
        rows[row_count].field = IFACE_PANE_FIELD_MAIN_MENU_BUTTON;
        markers[mark_count].setting_id = IFACE_PANE_ROW_BASE + row_count;
        markers[mark_count].label = "Main Menu";
        mark_count++;
        row_count++;
    }

    for (int i = 0; i < total; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        enum iface_pane_field fields[9];
        int field_count = 0;

        if (!pane_type_is_overlay(type))
            continue;

        if (type == PANE_LEFT_PANEL)
        {
            fields[field_count++] = IFACE_PANE_FIELD_LP_LAUNCH;
            fields[field_count++] = IFACE_PANE_FIELD_LP_COMPACT;
            fields[field_count++] = IFACE_PANE_FIELD_LP_TOUCH_EDGE;
            fields[field_count++] = IFACE_PANE_FIELD_FONT;
        }
        else if (type == PANE_OVERLAY_MENU)
        {
            fields[field_count++] = IFACE_PANE_FIELD_ENABLED;
            fields[field_count++] = IFACE_PANE_FIELD_PLACEMENT;
            fields[field_count++] = IFACE_PANE_FIELD_SIZE;
            fields[field_count++] = IFACE_PANE_FIELD_COLUMNS;
            fields[field_count++] = IFACE_PANE_FIELD_ROWS;
            fields[field_count++] = IFACE_PANE_FIELD_QA_ARROWS;
            fields[field_count++] = IFACE_PANE_FIELD_QA_LAUNCH;
            fields[field_count++] = IFACE_PANE_FIELD_BUTTONS;
        }
        else
        {
            fields[field_count++] = IFACE_PANE_FIELD_ENABLED;
            fields[field_count++] = IFACE_PANE_FIELD_PLACEMENT;
            fields[field_count++] = IFACE_PANE_FIELD_FONT;
            if (type == PANE_ROLLS) {
                fields[field_count++] = IFACE_PANE_FIELD_LOG_ROWS;
                fields[field_count++] = IFACE_PANE_FIELD_OVERLAY_LOG_BORDER;
            }
        }

        if (row_count + field_count > max_rows)
            break;

        for (int f = 0; f < field_count; f++)
        {
            rows[row_count].pane_cfg_index = i;
            rows[row_count].type = type;
            rows[row_count].field = fields[f];
            markers[mark_count].setting_id = IFACE_PANE_ROW_BASE + row_count;
            markers[mark_count].label = pane_type_display_name(type);
            mark_count++;
            row_count++;
        }
    }

    if (row_count + 2 <= max_rows)
    {
        rows[row_count].pane_cfg_index = -1;
        rows[row_count].type = PANE_MAX;
        rows[row_count].field = IFACE_PANE_FIELD_DICE_LOCK;
        markers[mark_count].setting_id = IFACE_PANE_ROW_BASE + row_count;
        markers[mark_count].label = "Dice Roll Overlay";
        mark_count++;
        row_count++;

        rows[row_count].pane_cfg_index = -1;
        rows[row_count].type = PANE_MAX;
        rows[row_count].field = IFACE_PANE_FIELD_DICE_OVERLAY;
        markers[mark_count].setting_id = IFACE_PANE_ROW_BASE + row_count;
        markers[mark_count].label = "Dice Roll Overlay";
        mark_count++;
        row_count++;
    }

    if (row_count + 1 <= max_rows)
    {
        rows[row_count].pane_cfg_index = -1;
        rows[row_count].type = PANE_MAX;
        rows[row_count].field = IFACE_PANE_FIELD_POPUP_NOTIFICATION;
        markers[mark_count].setting_id = IFACE_PANE_ROW_BASE + row_count;
        markers[mark_count].label = "Popup Notifications";
        mark_count++;
        row_count++;
    }

    if (marker_count)
        *marker_count = mark_count;
    return row_count;
}

static void iface_format_ms_value(char* buf, size_t buflen, int ms)
{
    int tenths;

    if (!buf || !buflen)
        return;

    if (ms < 0)
        ms = 0;
    tenths = (ms + 50) / 100;
    strnfmt(buf, buflen, "%d.%ds", tenths / 10, tenths % 10);
}

static cptr iface_pane_row_label(const struct iface_pane_row* row)
{
    switch (row->field)
    {
    case IFACE_PANE_FIELD_ENABLED: return "Enabled";
    case IFACE_PANE_FIELD_PLACEMENT: return "Placement";
    case IFACE_PANE_FIELD_FONT: return "Font Size";
    case IFACE_PANE_FIELD_SIZE: return "Size";
    case IFACE_PANE_FIELD_COLUMNS: return "Columns";
    case IFACE_PANE_FIELD_ROWS: return "Rows";
    case IFACE_PANE_FIELD_QA_ARROWS: return "Arrows";
    case IFACE_PANE_FIELD_QA_LAUNCH: return "Launch State";
    case IFACE_PANE_FIELD_BUTTONS: return "Assignments";
    case IFACE_PANE_FIELD_LP_LAUNCH: return "Launch State";
    case IFACE_PANE_FIELD_LP_COMPACT: return "Compact Mode";
    case IFACE_PANE_FIELD_LP_TOUCH_EDGE: return "Touch Left Edge";
    case IFACE_PANE_FIELD_LOG_ROWS: return "Lines";
    case IFACE_PANE_FIELD_OVERLAY_LOG_BORDER: return "White Border";
    case IFACE_PANE_FIELD_DICE_LOCK: return "Lock Time";
    case IFACE_PANE_FIELD_DICE_OVERLAY: return "Result Time";
    case IFACE_PANE_FIELD_MAIN_MENU_BUTTON: return "Show Fixed Button";
    case IFACE_PANE_FIELD_POPUP_NOTIFICATION: return "Display Time";
    case IFACE_PANE_FIELD_CAMERA_CENTER_CLEARANCE:
        return "Recenter Distance";
    default: return "?";
    }
}

static cptr iface_pane_row_description(const struct iface_pane_row* row)
{
    if (!row)
        return NULL;

    if (row->field == IFACE_PANE_FIELD_MAIN_MENU_BUTTON)
    {
        return "Show the fixed Menu button at the top center during play. "
            "Top-center panes begin directly below it while enabled. This is "
            "saved separately for portrait and landscape.";
    }

    if (row->field == IFACE_PANE_FIELD_CAMERA_CENTER_CLEARANCE)
    {
        return "Recenter the player within the relevant visible map zone "
            "when they come within this many cells of a screen edge or any "
            "visible overlay. The same distance shifts the player behind "
            "their travel direction to show more map ahead. The default is "
            "5 on desktop and 3 on mobile.";
    }

    if (row->field == IFACE_PANE_FIELD_PLACEMENT)
    {
        return "The number is this pane's stack order at that position. "
            "Choosing an occupied number inserts this pane there and shifts "
            "the other panes. Portrait and landscape are saved separately.";
    }

    if (row->field == IFACE_PANE_FIELD_QA_ARROWS)
    {
        return "Show the Quick Access border arrow used to open and hide the "
            "panel. With arrows off, Quick Access remains open. This is saved "
            "separately for portrait and landscape.";
    }

    if (row->field == IFACE_PANE_FIELD_QA_LAUNCH)
    {
        return "Choose whether Quick Access starts open or hidden when arrows "
            "are enabled. With arrows off, it is always open. This is saved "
            "separately for portrait and landscape.";
    }

    if (row->field == IFACE_PANE_FIELD_BUTTONS)
    {
        return "Assign the Quick Access short-press and long-press actions "
            "for the current orientation. Portrait and landscape are saved "
            "separately.";
    }

    if (row->field == IFACE_PANE_FIELD_LP_TOUCH_EDGE)
    {
        return "Move the Left Panel and every left-aligned overlay to the "
            "screen's left layout edge. This is saved separately for "
            "portrait and landscape.";
    }

    if (row->field == IFACE_PANE_FIELD_LOG_ROWS)
    {
        return "Set the requested number of visible Overlay Log lines for "
            "the current orientation. Portrait and landscape are saved "
            "separately.";
    }

    if (row->field == IFACE_PANE_FIELD_ENABLED
        || row->field == IFACE_PANE_FIELD_FONT
        || row->field == IFACE_PANE_FIELD_SIZE
        || row->field == IFACE_PANE_FIELD_COLUMNS
        || row->field == IFACE_PANE_FIELD_ROWS
        || row->field == IFACE_PANE_FIELD_QA_ARROWS
        || row->field == IFACE_PANE_FIELD_QA_LAUNCH
        || row->field == IFACE_PANE_FIELD_LP_LAUNCH
        || row->field == IFACE_PANE_FIELD_LP_COMPACT
        || row->field == IFACE_PANE_FIELD_LP_TOUCH_EDGE
        || row->field == IFACE_PANE_FIELD_LOG_ROWS
        || row->field == IFACE_PANE_FIELD_OVERLAY_LOG_BORDER
        || row->field == IFACE_PANE_FIELD_MAIN_MENU_BUTTON
        || row->field == IFACE_PANE_FIELD_DICE_LOCK
        || row->field == IFACE_PANE_FIELD_DICE_OVERLAY
        || row->field == IFACE_PANE_FIELD_POPUP_NOTIFICATION)
    {
        return "This overlay value belongs to the current orientation's "
            "saved layout. Portrait and landscape can be configured "
            "independently.";
    }

    return NULL;
}

static int iface_pane_row_live_index(const struct iface_pane_row* row)
{
    int total;

    if (!row || row->type == PANE_MAX)
        return -1;

    total = get_pane_config_count();
    for (int i = 0; i < total; i++) {
        if ((enum pane_type)get_sdl_pane_type(i) == row->type)
            return i;
    }

    return row->pane_cfg_index;
}

static void iface_pane_row_value(const struct iface_pane_row* row, char* buf,
    size_t buflen)
{
    int idx = iface_pane_row_live_index(row);

    if (!buf || !buflen)
        return;

    switch (row->field)
    {
    case IFACE_PANE_FIELD_ENABLED:
        SDL_strlcpy(buf, get_sdl_pane_enabled(idx) ? "on" : "off", buflen);
        break;
    case IFACE_PANE_FIELD_PLACEMENT:
        strnfmt(buf, buflen, "%s %d",
            pane_placement_display_name(
                (enum pane_placement)get_sdl_pane_where(idx)),
            get_sdl_pane_stack_order(idx));
        break;
    case IFACE_PANE_FIELD_FONT:
        format_font_size_value(buf, buflen, get_sdl_pane_font_size(idx),
            get_sdl_pane_effective_font_size(idx), 16);
        break;
    case IFACE_PANE_FIELD_SIZE:
        touch_top_widget_format_size(buf, buflen,
            get_sdl_touch_top_panel_size());
        break;
    case IFACE_PANE_FIELD_COLUMNS:
        strnfmt(buf, buflen, "%d", get_sdl_touch_top_panel_columns());
        break;
    case IFACE_PANE_FIELD_ROWS:
        strnfmt(buf, buflen, "%d", get_sdl_touch_top_panel_rows());
        break;
    case IFACE_PANE_FIELD_QA_ARROWS:
        SDL_strlcpy(buf, get_sdl_touch_top_panel_arrows_visible()
            ? "on" : "off", buflen);
        break;
    case IFACE_PANE_FIELD_QA_LAUNCH:
        SDL_strlcpy(buf,
            !get_sdl_touch_top_panel_arrows_visible()
                ? "always open"
                : (get_sdl_touch_top_panel_default_open()
                    ? "open" : "hidden"),
            buflen);
        break;
    case IFACE_PANE_FIELD_BUTTONS:
        SDL_strlcpy(buf, "open", buflen);
        break;
    case IFACE_PANE_FIELD_LP_LAUNCH:
        SDL_strlcpy(buf,
            get_sdl_left_panel_expanded_on_launch() ? "full" : "compact", buflen);
        break;
    case IFACE_PANE_FIELD_LP_COMPACT:
        SDL_strlcpy(buf,
            get_sdl_left_panel_compact_mode() == SDL_LEFT_PANEL_COMPACT_ROW
                ? "row" : "column",
            buflen);
        break;
    case IFACE_PANE_FIELD_LP_TOUCH_EDGE:
        SDL_strlcpy(buf, get_sdl_left_overlays_touch_screen_edge()
            ? "on" : "off", buflen);
        break;
    case IFACE_PANE_FIELD_LOG_ROWS:
        strnfmt(buf, buflen, "%d", sdl_log_pane_current_rows(PANE_ROLLS));
        break;
    case IFACE_PANE_FIELD_OVERLAY_LOG_BORDER:
        SDL_strlcpy(buf, get_sdl_show_overlay_log_border() ? "on" : "off",
            buflen);
        break;
    case IFACE_PANE_FIELD_DICE_LOCK:
        iface_format_ms_value(buf, buflen, get_sdl_dice_roll_lock_ms());
        break;
    case IFACE_PANE_FIELD_DICE_OVERLAY:
        iface_format_ms_value(buf, buflen, get_sdl_dice_roll_overlay_ms());
        break;
    case IFACE_PANE_FIELD_MAIN_MENU_BUTTON:
        SDL_strlcpy(buf, get_sdl_show_main_menu_button() ? "on" : "off",
            buflen);
        break;
    case IFACE_PANE_FIELD_POPUP_NOTIFICATION:
        iface_format_ms_value(buf, buflen, get_sdl_popup_notification_ms());
        break;
    case IFACE_PANE_FIELD_CAMERA_CENTER_CLEARANCE:
        strnfmt(buf, buflen, "%d", get_sdl_camera_center_clearance());
        break;
    default:
        SDL_strlcpy(buf, "", buflen);
        break;
    }
}

static void iface_pane_row_apply_change(const struct iface_pane_row* row)
{
    switch (row->field)
    {
    case IFACE_PANE_FIELD_LP_LAUNCH:
    case IFACE_PANE_FIELD_LP_COMPACT:
    case IFACE_PANE_FIELD_OVERLAY_LOG_BORDER:
        sdl_request_redraw();
        break;
    case IFACE_PANE_FIELD_DICE_LOCK:
    case IFACE_PANE_FIELD_DICE_OVERLAY:
    case IFACE_PANE_FIELD_POPUP_NOTIFICATION:
    case IFACE_PANE_FIELD_BUTTONS:
        break;
    default:
        sdl_apply_config();
        break;
    }
}

static bool iface_pane_pick_from_choices(const struct iface_pane_row* row,
    const struct settings_value_choice* choices, int count, int current_value,
    int* out_value, bool* handled)
{
    char title[96];
    cptr group;

    if (row->field == IFACE_PANE_FIELD_MAIN_MENU_BUTTON)
        group = "Main Menu";
    else if (row->field == IFACE_PANE_FIELD_CAMERA_CENTER_CLEARANCE)
        group = "Camera";
    else if (row->field == IFACE_PANE_FIELD_POPUP_NOTIFICATION)
        group = "Popup Notifications";
    else if (row->type == PANE_MAX)
        group = "Dice Roll Overlay";
    else
        group = pane_type_display_name(row->type);

    if (handled)
        *handled = true;

    strnfmt(title, sizeof(title), "%s %s", group, iface_pane_row_label(row));
    return settings_pick_value(title, NULL, choices, count, current_value,
        out_value);
}

static bool iface_pane_row_pick_value(const struct iface_pane_row* row,
    bool* handled)
{
    int idx;
    int value = 0;
    bool changed = false;

    static const struct settings_value_choice off_on_choices[] = {
        { 0, "off" },
        { 1, "on" }
    };
    static const struct settings_value_choice hidden_open_choices[] = {
        { 0, "hidden" },
        { 1, "open" }
    };
    static const struct settings_value_choice compact_full_choices[] = {
        { 0, "compact" },
        { 1, "full" }
    };
    static const struct settings_value_choice compact_mode_choices[] = {
        { SDL_LEFT_PANEL_COMPACT_COLUMN, "column" },
        { SDL_LEFT_PANEL_COMPACT_ROW, "row" }
    };

    if (handled)
        *handled = false;
    if (!row)
        return false;

    idx = iface_pane_row_live_index(row);

    switch (row->field)
    {
    case IFACE_PANE_FIELD_ENABLED:
        value = get_sdl_pane_enabled(idx) ? 1 : 0;
        if (iface_pane_pick_from_choices(row, off_on_choices,
                (int)N_ELEMENTS(off_on_choices), value, &value, handled)
            && value != (get_sdl_pane_enabled(idx) ? 1 : 0))
        {
            set_sdl_pane_enabled(idx, value != 0);
            changed = true;
        }
        break;

    case IFACE_PANE_FIELD_PLACEMENT:
    {
        static const enum pane_placement placements[] = {
            PLACE_BOTTOM, PLACE_RIGHT, PLACE_LEFT, PLACE_DOUBLE_LEFT,
            PLACE_DOUBLE_RIGHT, PLACE_DOUBLE_BOTTOM, PLACE_TOP_LEFT,
            PLACE_TOP_RIGHT, PLACE_BOTTOM_LEFT, PLACE_BOTTOM_RIGHT,
            PLACE_TOP_CENTER, PLACE_BOTTOM_CENTER, PLACE_LEFT_CENTER,
            PLACE_RIGHT_CENTER
        };
        struct settings_value_choice choices[
            N_ELEMENTS(placements) * MAX_PANE_CONFIGS];
        char labels[N_ELEMENTS(placements) * MAX_PANE_CONFIGS][48];
        int count = 0;
        enum pane_placement current_where =
            (enum pane_placement)get_sdl_pane_where(idx);
        int current_order = get_sdl_pane_stack_order(idx);
        int current = current_where * IFACE_PANE_PLACEMENT_ORDER_STRIDE
            + current_order;

        for (int i = 0; i < (int)N_ELEMENTS(placements); i++)
        {
            int other_count;

            if (!pane_type_allows_placement(row->type, placements[i]))
                continue;

            other_count = get_sdl_pane_stack_count(placements[i]);
            if (placements[i] == current_where)
                other_count--;

            for (int order = 1; order <= other_count + 1; order++) {
                choices[count].value = placements[i]
                    * IFACE_PANE_PLACEMENT_ORDER_STRIDE + order;
                strnfmt(labels[count], sizeof(labels[count]), "%s %d",
                    pane_placement_display_name(placements[i]), order);
                choices[count].label = labels[count];
                count++;
            }
        }

        if (count <= 0)
            break;

        if (iface_pane_pick_from_choices(row, choices, count, current, &value,
                handled)
            && value != current)
        {
            enum pane_placement placement = (enum pane_placement)(
                value / IFACE_PANE_PLACEMENT_ORDER_STRIDE);
            int order = value % IFACE_PANE_PLACEMENT_ORDER_STRIDE;

            set_sdl_pane_where_order(idx, placement, order);
            changed = true;
        }
        break;
    }

    case IFACE_PANE_FIELD_FONT:
        return false;

    case IFACE_PANE_FIELD_SIZE:
        if (handled)
            *handled = true;
        changed = touch_top_widget_pick_size();
        break;

    case IFACE_PANE_FIELD_COLUMNS:
    {
        struct settings_value_choice choices[SDL_TOUCH_TOP_PANEL_COLUMNS_MAX + 1];
        char labels[SDL_TOUCH_TOP_PANEL_COLUMNS_MAX + 1][8];
        int count = 0;
        int current = get_sdl_touch_top_panel_columns();
        int max_columns = SDL_TOUCH_TOP_PANEL_CELL_COUNT_MAX
            / get_sdl_touch_top_panel_rows();

        for (int columns = SDL_TOUCH_TOP_PANEL_COLUMNS_MIN;
             columns <= max_columns; columns++)
        {
            strnfmt(labels[count], sizeof(labels[count]), "%d", columns);
            choices[count].value = columns;
            choices[count].label = labels[count];
            count++;
        }

        if (iface_pane_pick_from_choices(row, choices, count, current, &value,
                handled)
            && value != current)
        {
            set_sdl_touch_top_panel_columns(value);
            changed = true;
        }
        break;
    }

    case IFACE_PANE_FIELD_ROWS:
    {
        static const struct settings_value_choice row_choices[] = {
            { 1, "1" }, { 2, "2" },
        };
        int current = get_sdl_touch_top_panel_rows();

        if (iface_pane_pick_from_choices(row, row_choices,
                (int)N_ELEMENTS(row_choices), current, &value, handled)
            && value != current)
        {
            set_sdl_touch_top_panel_rows(value);
            changed = true;
        }
        break;
    }

    case IFACE_PANE_FIELD_QA_ARROWS:
        value = get_sdl_touch_top_panel_arrows_visible() ? 1 : 0;
        if (iface_pane_pick_from_choices(row, off_on_choices,
                (int)N_ELEMENTS(off_on_choices), value, &value, handled)
            && value != (get_sdl_touch_top_panel_arrows_visible() ? 1 : 0))
        {
            set_sdl_touch_top_panel_arrows_visible(value != 0);
            changed = true;
        }
        break;

    case IFACE_PANE_FIELD_QA_LAUNCH:
        if (!get_sdl_touch_top_panel_arrows_visible()) {
            if (handled)
                *handled = true;
            break;
        }
        value = get_sdl_touch_top_panel_default_open() ? 1 : 0;
        if (iface_pane_pick_from_choices(row, hidden_open_choices,
                (int)N_ELEMENTS(hidden_open_choices), value, &value, handled)
            && value != (get_sdl_touch_top_panel_default_open() ? 1 : 0))
        {
            set_sdl_touch_top_panel_default_open(value != 0);
            changed = true;
        }
        break;

    case IFACE_PANE_FIELD_BUTTONS:
        return false;

    case IFACE_PANE_FIELD_LP_LAUNCH:
        value = get_sdl_left_panel_expanded_on_launch() ? 1 : 0;
        if (iface_pane_pick_from_choices(row, compact_full_choices,
                (int)N_ELEMENTS(compact_full_choices), value, &value, handled)
            && value != (get_sdl_left_panel_expanded_on_launch() ? 1 : 0))
        {
            set_sdl_left_panel_expanded_on_launch(value != 0);
            changed = true;
        }
        break;

    case IFACE_PANE_FIELD_LP_COMPACT:
        value = get_sdl_left_panel_compact_mode();
        if (iface_pane_pick_from_choices(row, compact_mode_choices,
                (int)N_ELEMENTS(compact_mode_choices), value, &value, handled)
            && value != get_sdl_left_panel_compact_mode())
        {
            set_sdl_left_panel_compact_mode(value);
            changed = true;
        }
        break;

    case IFACE_PANE_FIELD_LP_TOUCH_EDGE:
        value = get_sdl_left_overlays_touch_screen_edge() ? 1 : 0;
        if (iface_pane_pick_from_choices(row, off_on_choices,
                (int)N_ELEMENTS(off_on_choices), value, &value, handled)
            && value != (get_sdl_left_overlays_touch_screen_edge() ? 1 : 0))
        {
            set_sdl_left_overlays_touch_screen_edge(value != 0);
            changed = true;
        }
        break;

    case IFACE_PANE_FIELD_LOG_ROWS:
    {
        struct settings_value_choice choices[IFACE_OVERLAY_LOG_ROWS_MAX];
        char labels[IFACE_OVERLAY_LOG_ROWS_MAX][8];
        int count = 0;
        int current = sdl_log_pane_current_rows(PANE_ROLLS);

        for (int rows = IFACE_OVERLAY_LOG_ROWS_MIN;
             rows <= IFACE_OVERLAY_LOG_ROWS_MAX; rows++)
        {
            strnfmt(labels[count], sizeof(labels[count]), "%d", rows);
            choices[count].value = rows;
            choices[count].label = labels[count];
            count++;
        }
        if (iface_pane_pick_from_choices(row, choices, count, current, &value,
                handled)
            && value != current)
        {
            sdl_log_pane_set_rows(PANE_ROLLS, value);
            changed = true;
        }
        break;
    }

    case IFACE_PANE_FIELD_OVERLAY_LOG_BORDER:
        value = get_sdl_show_overlay_log_border() ? 1 : 0;
        if (iface_pane_pick_from_choices(row, off_on_choices,
                (int)N_ELEMENTS(off_on_choices), value, &value, handled)
            && value != (get_sdl_show_overlay_log_border() ? 1 : 0))
        {
            set_sdl_show_overlay_log_border(value != 0);
            changed = true;
        }
        break;

    case IFACE_PANE_FIELD_MAIN_MENU_BUTTON:
        value = get_sdl_show_main_menu_button() ? 1 : 0;
        if (iface_pane_pick_from_choices(row, off_on_choices,
                (int)N_ELEMENTS(off_on_choices), value, &value, handled)
            && value != (get_sdl_show_main_menu_button() ? 1 : 0))
        {
            set_sdl_show_main_menu_button(value != 0);
            changed = true;
        }
        break;

    case IFACE_PANE_FIELD_CAMERA_CENTER_CLEARANCE:
    {
        struct settings_value_choice choices[
            SDL_CAMERA_CENTER_CLEARANCE_MAX
            - SDL_CAMERA_CENTER_CLEARANCE_MIN + 1];
        char labels[
            SDL_CAMERA_CENTER_CLEARANCE_MAX
            - SDL_CAMERA_CENTER_CLEARANCE_MIN + 1][8];
        int count = 0;
        int current = get_sdl_camera_center_clearance();

        for (int distance = SDL_CAMERA_CENTER_CLEARANCE_MIN;
             distance <= SDL_CAMERA_CENTER_CLEARANCE_MAX; distance++)
        {
            strnfmt(labels[count], sizeof(labels[count]), "%d", distance);
            choices[count].value = distance;
            choices[count].label = labels[count];
            count++;
        }
        if (iface_pane_pick_from_choices(row, choices, count, current, &value,
                handled)
            && value != current)
        {
            set_sdl_camera_center_clearance(value);
            changed = true;
        }
        break;
    }

    case IFACE_PANE_FIELD_DICE_LOCK:
    case IFACE_PANE_FIELD_DICE_OVERLAY:
    case IFACE_PANE_FIELD_POPUP_NOTIFICATION:
    {
        struct settings_value_choice choices[
            SDL_DICE_ROLL_TIMING_MAX_MS / IFACE_DICE_ROLL_MS_STEP + 1];
        char labels[
            SDL_DICE_ROLL_TIMING_MAX_MS / IFACE_DICE_ROLL_MS_STEP + 1][12];
        int count = 0;
        int current = (row->field == IFACE_PANE_FIELD_DICE_LOCK)
            ? get_sdl_dice_roll_lock_ms()
            : ((row->field == IFACE_PANE_FIELD_DICE_OVERLAY)
                ? get_sdl_dice_roll_overlay_ms()
                : get_sdl_popup_notification_ms());

        current = ((current + IFACE_DICE_ROLL_MS_STEP / 2)
            / IFACE_DICE_ROLL_MS_STEP) * IFACE_DICE_ROLL_MS_STEP;

        for (int ms = 0; ms <= SDL_DICE_ROLL_TIMING_MAX_MS;
             ms += IFACE_DICE_ROLL_MS_STEP)
        {
            iface_format_ms_value(labels[count], sizeof(labels[count]), ms);
            choices[count].value = ms;
            choices[count].label = labels[count];
            count++;
        }

        if (iface_pane_pick_from_choices(row, choices, count, current, &value,
                handled))
        {
            int old_value = (row->field == IFACE_PANE_FIELD_DICE_LOCK)
                ? get_sdl_dice_roll_lock_ms()
                : ((row->field == IFACE_PANE_FIELD_DICE_OVERLAY)
                    ? get_sdl_dice_roll_overlay_ms()
                    : get_sdl_popup_notification_ms());

            if (row->field == IFACE_PANE_FIELD_DICE_LOCK)
                set_sdl_dice_roll_lock_ms(value);
            else if (row->field == IFACE_PANE_FIELD_DICE_OVERLAY)
                set_sdl_dice_roll_overlay_ms(value);
            else
                set_sdl_popup_notification_ms(value);
            changed = (value != old_value);
        }
        break;
    }

    default:
        return false;
    }

    if (changed)
        iface_pane_row_apply_change(row);

    return changed;
}

/*
 * Apply a change to one inline pane row.  `delta` is +1 (next/increase),
 * -1 (prev/decrease) or 0 (toggle/cycle forward).  Mirrors the set_sdl_* +
 * immediate-apply behaviour of do_cmd_pane_settings and the pane sub-editors.
 * Returns true if anything changed.
 */
static bool iface_pane_row_adjust(const struct iface_pane_row* row, int delta)
{
    int idx = iface_pane_row_live_index(row);
    bool changed = false;

    switch (row->field)
    {
    case IFACE_PANE_FIELD_ENABLED:
    {
        bool cur = get_sdl_pane_enabled(idx);
        bool next = (delta == 0) ? !cur : (delta > 0);
        if (next != cur)
        {
            set_sdl_pane_enabled(idx, next);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_PLACEMENT:
    {
        enum pane_placement cur = (enum pane_placement)get_sdl_pane_where(idx);
        int cur_order = get_sdl_pane_stack_order(idx);
        int cur_count = get_sdl_pane_stack_count(cur);
        enum pane_placement next = cur;
        int next_order = cur_order;

        if (delta < 0 && cur_order > 1) {
            next_order--;
        } else if (delta > 0 && cur_order < cur_count) {
            next_order++;
        } else {
            next = pane_next_allowed_placement(row->type, cur,
                (delta < 0) ? -1 : 1);
            next_order = (delta < 0)
                ? get_sdl_pane_stack_count(next) + 1 : 1;
        }

        set_sdl_pane_where_order(idx, next, next_order);
        changed = true;
        break;
    }
    case IFACE_PANE_FIELD_FONT:
    {
        int value = get_sdl_pane_font_size(idx);
        if (value == 0)
            set_sdl_pane_font_size(idx, get_sdl_pane_effective_font_size(idx));
        else
            set_sdl_pane_font_size(idx, value + ((delta < 0) ? -1 : 1));
        changed = true;
        break;
    }
    case IFACE_PANE_FIELD_SIZE:
    {
        float value = get_sdl_touch_top_panel_size();
        float next;

        if (value == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH) {
            next = (delta < 0) ? SDL_TOUCH_TOP_PANEL_SIZE_MAX
                               : SDL_TOUCH_TOP_PANEL_SIZE_MIN;
        } else {
            next = value + ((delta < 0) ? -SDL_TOUCH_TOP_PANEL_SIZE_STEP
                                        : SDL_TOUCH_TOP_PANEL_SIZE_STEP);
            if (next < SDL_TOUCH_TOP_PANEL_SIZE_MIN)
                next = SDL_TOUCH_TOP_PANEL_SIZE_STRETCH;
            else if (next > SDL_TOUCH_TOP_PANEL_SIZE_MAX)
                next = SDL_TOUCH_TOP_PANEL_SIZE_STRETCH;
        }
        set_sdl_touch_top_panel_size(next);
        changed = (get_sdl_touch_top_panel_size() != value);
        break;
    }
    case IFACE_PANE_FIELD_COLUMNS:
    {
        int value = get_sdl_touch_top_panel_columns();
        set_sdl_touch_top_panel_columns(value + ((delta < 0) ? -1 : 1));
        changed = (get_sdl_touch_top_panel_columns() != value);
        break;
    }
    case IFACE_PANE_FIELD_ROWS:
    {
        int value = get_sdl_touch_top_panel_rows();
        int next = (delta == 0)
            ? (value == SDL_TOUCH_TOP_PANEL_ROWS_MIN
                ? SDL_TOUCH_TOP_PANEL_ROWS_MAX
                : SDL_TOUCH_TOP_PANEL_ROWS_MIN)
            : value + ((delta < 0) ? -1 : 1);
        if (next != value)
        {
            set_sdl_touch_top_panel_rows(next);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_QA_ARROWS:
    {
        bool cur = get_sdl_touch_top_panel_arrows_visible();
        bool next = (delta == 0) ? !cur : (delta > 0);
        if (next != cur)
        {
            set_sdl_touch_top_panel_arrows_visible(next);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_QA_LAUNCH:
    {
        bool cur;
        bool next;

        if (!get_sdl_touch_top_panel_arrows_visible())
            break;
        cur = get_sdl_touch_top_panel_default_open();
        next = (delta == 0) ? !cur : (delta > 0);
        if (next != cur)
        {
            set_sdl_touch_top_panel_default_open(next);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_BUTTONS:
        do_cmd_touch_top_widget_button_editor(&changed);
        break;
    case IFACE_PANE_FIELD_LP_LAUNCH:
    {
        bool cur = get_sdl_left_panel_expanded_on_launch();
        bool next = (delta == 0) ? !cur : (delta > 0);
        if (next != cur)
        {
            set_sdl_left_panel_expanded_on_launch(next);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_LP_COMPACT:
        set_sdl_left_panel_compact_mode(
            get_sdl_left_panel_compact_mode() == SDL_LEFT_PANEL_COMPACT_ROW
                ? SDL_LEFT_PANEL_COMPACT_COLUMN
                : SDL_LEFT_PANEL_COMPACT_ROW);
        changed = true;
        break;
    case IFACE_PANE_FIELD_LP_TOUCH_EDGE:
    {
        bool cur = get_sdl_left_overlays_touch_screen_edge();
        bool next = (delta == 0) ? !cur : (delta > 0);

        if (next != cur) {
            set_sdl_left_overlays_touch_screen_edge(next);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_LOG_ROWS:
    {
        int value = sdl_log_pane_current_rows(PANE_ROLLS);
        int next = value + ((delta < 0) ? -1 : 1);

        if (next < IFACE_OVERLAY_LOG_ROWS_MIN)
            next = IFACE_OVERLAY_LOG_ROWS_MIN;
        if (next > IFACE_OVERLAY_LOG_ROWS_MAX)
            next = IFACE_OVERLAY_LOG_ROWS_MAX;
        if (next != value) {
            sdl_log_pane_set_rows(PANE_ROLLS, next);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_OVERLAY_LOG_BORDER:
    {
        bool cur = get_sdl_show_overlay_log_border();
        bool next = (delta == 0) ? !cur : (delta > 0);
        if (next != cur)
        {
            set_sdl_show_overlay_log_border(next);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_MAIN_MENU_BUTTON:
    {
        bool cur = get_sdl_show_main_menu_button();
        bool next = (delta == 0) ? !cur : (delta > 0);
        if (next != cur)
        {
            set_sdl_show_main_menu_button(next);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_CAMERA_CENTER_CLEARANCE:
    {
        int value = get_sdl_camera_center_clearance();
        int next = value + ((delta < 0) ? -1 : 1);

        set_sdl_camera_center_clearance(next);
        changed = (get_sdl_camera_center_clearance() != value);
        break;
    }
    case IFACE_PANE_FIELD_DICE_LOCK:
    {
        int value = get_sdl_dice_roll_lock_ms();
        int next = value + ((delta < 0) ? -IFACE_DICE_ROLL_MS_STEP
                                        : IFACE_DICE_ROLL_MS_STEP);
        set_sdl_dice_roll_lock_ms(next);
        changed = (get_sdl_dice_roll_lock_ms() != value);
        break;
    }
    case IFACE_PANE_FIELD_DICE_OVERLAY:
    {
        int value = get_sdl_dice_roll_overlay_ms();
        int next = value + ((delta < 0) ? -IFACE_DICE_ROLL_MS_STEP
                                        : IFACE_DICE_ROLL_MS_STEP);
        set_sdl_dice_roll_overlay_ms(next);
        changed = (get_sdl_dice_roll_overlay_ms() != value);
        break;
    }
    case IFACE_PANE_FIELD_POPUP_NOTIFICATION:
    {
        int value = get_sdl_popup_notification_ms();
        int next = value + ((delta < 0) ? -IFACE_DICE_ROLL_MS_STEP
                                        : IFACE_DICE_ROLL_MS_STEP);
        set_sdl_popup_notification_ms(next);
        changed = (get_sdl_popup_notification_ms() != value);
        break;
    }
    default:
        break;
    }

    if (changed)
        iface_pane_row_apply_change(row);

    return changed;
}

/*
 * Whether an inline interface-pane row exposes a value that has a meaningful
 * "default" the per-row Reset button can restore.  Enabled/Placement are the
 * user's deliberate layout choices (no per-pane default getter) and Assignments
 * just opens an editor, so those rows get no reset button.
 */
static bool iface_pane_row_resettable(const struct iface_pane_row* row)
{
    if (!row)
        return false;

    /* Everything except "Assignments" (which just opens a sub-editor) maps to a
     * value with a known default the Reset button can restore. */
    return row->field != IFACE_PANE_FIELD_BUTTONS;
}

static bool iface_pane_row_reset_to_default(const struct iface_pane_row* row)
{
    int idx;
    struct sdl_config def;
    bool changed = false;

    if (!row)
        return false;

    idx = iface_pane_row_live_index(row);
    sdl_config_set_defaults(&def);

    switch (row->field)
    {
    case IFACE_PANE_FIELD_ENABLED:
    {
        bool d = get_sdl_pane_default_enabled(idx);
        if (get_sdl_pane_enabled(idx) != d)
        {
            set_sdl_pane_enabled(idx, d);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_PLACEMENT:
    {
        int d = get_sdl_pane_default_where(idx);
        if (get_sdl_pane_where(idx) != d)
        {
            set_sdl_pane_where(idx, d);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_FONT:
        if (get_sdl_pane_font_size(idx) != 0)
        {
            set_sdl_pane_font_size(idx, 0);
            changed = true;
        }
        break;
    case IFACE_PANE_FIELD_SIZE:
    {
        float d = get_sdl_touch_top_panel_default_size();
        if (get_sdl_touch_top_panel_size() != d)
        {
            set_sdl_touch_top_panel_size(d);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_COLUMNS:
    {
        int d = get_sdl_touch_top_panel_default_columns();
        if (get_sdl_touch_top_panel_columns() != d)
        {
            set_sdl_touch_top_panel_columns(d);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_ROWS:
    {
        int d = get_sdl_touch_top_panel_default_rows();
        if (get_sdl_touch_top_panel_rows() != d)
        {
            set_sdl_touch_top_panel_rows(d);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_QA_ARROWS:
    {
        bool d = get_sdl_touch_top_panel_arrows_default_visible();
        if (get_sdl_touch_top_panel_arrows_visible() != d)
        {
            set_sdl_touch_top_panel_arrows_visible(d);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_QA_LAUNCH:
    {
        bool d = get_sdl_touch_top_panel_default_open_default();
        if (get_sdl_touch_top_panel_default_open() != d)
        {
            set_sdl_touch_top_panel_default_open(d);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_LP_LAUNCH:
        if (get_sdl_left_panel_expanded_on_launch()
            != def.left_panel_expanded_on_launch)
        {
            set_sdl_left_panel_expanded_on_launch(
                def.left_panel_expanded_on_launch);
            changed = true;
        }
        break;
    case IFACE_PANE_FIELD_LP_COMPACT:
        if (get_sdl_left_panel_compact_mode() != def.left_panel_compact_mode)
        {
            set_sdl_left_panel_compact_mode(def.left_panel_compact_mode);
            changed = true;
        }
        break;
    case IFACE_PANE_FIELD_LP_TOUCH_EDGE:
        if (get_sdl_left_overlays_touch_screen_edge()
            != def.left_overlays_touch_screen_edge)
        {
            set_sdl_left_overlays_touch_screen_edge(
                def.left_overlays_touch_screen_edge);
            changed = true;
        }
        break;
    case IFACE_PANE_FIELD_LOG_ROWS:
    {
        int d = get_sdl_pane_default_rows(idx);

        if (sdl_log_pane_current_rows(PANE_ROLLS) != d) {
            sdl_log_pane_set_rows(PANE_ROLLS, d);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_OVERLAY_LOG_BORDER:
        if (get_sdl_show_overlay_log_border()
            != (get_sdl_mobile_portrait_mode()
                ? false : def.show_overlay_log_border))
        {
            set_sdl_show_overlay_log_border(get_sdl_mobile_portrait_mode()
                ? false : def.show_overlay_log_border);
            changed = true;
        }
        break;
    case IFACE_PANE_FIELD_DICE_LOCK:
        if (get_sdl_dice_roll_lock_ms() != def.dice_roll_lock_ms)
        {
            set_sdl_dice_roll_lock_ms(def.dice_roll_lock_ms);
            changed = true;
        }
        break;
    case IFACE_PANE_FIELD_DICE_OVERLAY:
        if (get_sdl_dice_roll_overlay_ms() != def.dice_roll_overlay_ms)
        {
            set_sdl_dice_roll_overlay_ms(def.dice_roll_overlay_ms);
            changed = true;
        }
        break;
    case IFACE_PANE_FIELD_MAIN_MENU_BUTTON:
    {
        bool default_visible = get_sdl_mobile_portrait_mode()
            ? false : def.show_main_menu_button;

        if (get_sdl_show_main_menu_button() != default_visible)
        {
            set_sdl_show_main_menu_button(default_visible);
            changed = true;
        }
        break;
    }
    case IFACE_PANE_FIELD_POPUP_NOTIFICATION:
        if (get_sdl_popup_notification_ms() != def.popup_notification_ms)
        {
            set_sdl_popup_notification_ms(def.popup_notification_ms);
            changed = true;
        }
        break;
    case IFACE_PANE_FIELD_CAMERA_CENTER_CLEARANCE:
        if (get_sdl_camera_center_clearance()
            != def.camera_center_clearance)
        {
            set_sdl_camera_center_clearance(def.camera_center_clearance);
            changed = true;
        }
        break;
    default:
        break;
    }

    if (changed)
        iface_pane_row_apply_change(row);

    return changed;
}

static void do_cmd_supporting_pane_layout_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = MAX_PANE_CONFIGS };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (!pane_settings_exposes_pane(type))
            continue;
        if (pane_type_is_overlay(type))
            continue;
        pane_indices[pane_count++] = i;
    }

    screen_save();

    int sel = 0;
    int field = 0; /* 0 = enabled, 1 = where, 2 = rows, 3 = cols */
    bool done = false;
    bool changed = false;
    int dir;

    if (pane_count <= 0)
    {
        settings_semantic_menu_begin("Pane Layout", -1);
        sdl_character_sheet_screen_set_select_description(
            "No configurable panes are configured.");
        sdl_character_sheet_screen_commit_select(-1);
        (void)inkey();
        settings_semantic_menu_hide();
        screen_load();
        return;
    }

    if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
    {
        changed = true;
        sdl_apply_config();
    }
    supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);

    while (!done)
    {
        int row_width = settings_ui_line_width(2);

        settings_semantic_menu_begin("Pane Layout", sel);

        for (int i = 0; i < pane_count; i++)
        {
            int idx = pane_indices[i];
            enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
            enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
            int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);
            bool enabled = get_sdl_pane_enabled(idx);
            bool rows_locked = supporting_pane_rows_locked(pane_indices, pane_count, idx);
            bool cols_locked = supporting_pane_cols_locked(pane_indices, pane_count, idx);
            int rows = get_sdl_pane_rows(idx);
            int cols = get_sdl_pane_cols(idx);
            bool enabled_locked = supporting_pane_enabled_locked(idx);
            bool where_locked = supporting_pane_where_locked(idx);
            bool left_panel = (type == PANE_LEFT_PANEL);
            byte a = (i == sel) ? TERM_L_BLUE : (enabled ? TERM_WHITE : TERM_SLATE);
            char type_buf[24];
            char enabled_field[12];
            char where_field[24];
            char rows_value[16];
            char rows_field[20];
            char cols_value[16];
            char cols_field[20];
            char line_buf[128];
            const char* type_label = settings_ui_pick_label(MAX(8, row_width / 3),
                pane_type_name(type), pane_type_name(type), pane_type_short_name(type));
            const char* where_label = settings_ui_pick_label(MAX(4, row_width / 4),
                pane_placement_display_name(where),
                pane_placement_display_name(where),
                pane_where_short_name(where));

            settings_ui_fit_text(type_buf, sizeof(type_buf), type_label,
                MAX(4, row_width / 3));
            settings_ui_format_field(enabled_field, sizeof(enabled_field),
                enabled ? "on" : "off",
                !enabled_locked && i == sel && field == 0);
            settings_ui_format_field(where_field, sizeof(where_field), where_label,
                !where_locked && i == sel && field == 1);

            if (rows_locked)
            {
                int shared_rows = (master_idx >= 0) ? get_sdl_pane_rows(master_idx) : rows;
                settings_ui_format_auto_value(rows_value, sizeof(rows_value),
                    shared_rows, 4);
            }
            else
                settings_ui_format_auto_value(rows_value, sizeof(rows_value), rows, 4);
            settings_ui_format_field(rows_field, sizeof(rows_field), rows_value,
                !rows_locked && i == sel && field == 2);

            if (cols_locked)
            {
                int shared_cols = (master_idx >= 0) ? get_sdl_pane_cols(master_idx) : cols;
                settings_ui_format_auto_value(cols_value, sizeof(cols_value),
                    shared_cols, 4);
            }
            else
                settings_ui_format_auto_value(cols_value, sizeof(cols_value), cols, 4);
            settings_ui_format_field(cols_field, sizeof(cols_field), cols_value,
                !cols_locked && i == sel && field == 3);

            if (left_panel)
                strnfmt(line_buf, sizeof(line_buf), "%s %s %s",
                    type_buf, where_field, enabled_field);
            else
                strnfmt(line_buf, sizeof(line_buf), "%s %s %s r%s c%s",
                    type_buf, where_field, enabled_field, rows_field,
                    cols_field);
            settings_semantic_add_row(i, line_buf, a);
            sdl_character_sheet_screen_set_last_select_row_reset(
                SETTINGS_CLICK_RESET_ROW_BASE + i);
        }

        sdl_character_sheet_screen_set_select_description(
            "Up and down select a pane. Space chooses the active field. Left/Right or N/Y toggles, cycles, or adjusts the active value. 0 sets rows or columns to auto. Changes apply immediately.");
        sdl_character_sheet_screen_commit_select(sel);

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                }
                else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice
                        < SETTINGS_CLICK_RESET_ROW_BASE + pane_count)
                {
                    int idx;

                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    sel = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    idx = pane_indices[sel];
                    /* Restore the whole pane row to its default: enabled,
                     * placement and (where editable) size. */
                    set_sdl_pane_enabled(idx, get_sdl_pane_default_enabled(idx));
                    set_sdl_pane_where(idx, get_sdl_pane_default_where(idx));
                    if (!supporting_pane_rows_locked(pane_indices, pane_count,
                            idx))
                        set_sdl_pane_rows(idx, get_sdl_pane_default_rows(idx));
                    if (!supporting_pane_cols_locked(pane_indices, pane_count,
                            idx))
                        set_sdl_pane_cols(idx, get_sdl_pane_default_cols(idx));
                    supporting_pane_normalize_shared_sizes(pane_indices,
                        pane_count);
                    supporting_pane_ensure_editable_field(&field, pane_indices,
                        pane_count, sel);
                    changed = true;
                    sdl_apply_config();
                    continue;
                }
                else if (clicked_choice >= SETTINGS_CLICK_PANE_FIELD_BASE
                    && clicked_choice < SETTINGS_CLICK_PANE_FIELD_BASE + pane_count * 4)
                {
                    int offset = clicked_choice - SETTINGS_CLICK_PANE_FIELD_BASE;
                    sel = offset / 4;
                    field = offset % 4;
                    supporting_pane_ensure_editable_field(&field, pane_indices,
                        pane_count, sel);
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = (click_action == UI_MENU_CLICK_SECONDARY) ? '4' : '6';
                    click_generated = true;
                }
                else if (clicked_choice >= 0 && clicked_choice < pane_count)
                {
                    if (clicked_choice != sel)
                    {
                        sel = clicked_choice;
                        supporting_pane_ensure_editable_field(&field, pane_indices,
                            pane_count, sel);
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        if (click_action == UI_MENU_CLICK_PRIMARY)
                            continue;
                    }
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = (click_action == UI_MENU_CLICK_SECONDARY) ? '4' : ' ';
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, 0, 0, click_generated);
        }

        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case ' ':
        case 't':
        case '5':
            field = (field + 1) % 4;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '-':
        case '8':
            sel = (pane_count + sel - 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '2':
            sel = (sel + 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '0':
        {
            int idx = pane_indices[sel];
            if (field == 0 || field == 1)
            {
                bell("Use Left/Right to toggle enabled or cycle placement");
                break;
            }
            if (field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
            {
                bell("Rows are shared within each bottom slot");
                break;
            }
            if (field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx))
            {
                bell("Cols are shared within each side/corner slot");
                break;
            }

            if (field == 2)
                set_sdl_pane_rows(idx, 0);
            else
                set_sdl_pane_cols(idx, 0);

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        case 'n':
        case '4':
        case 'y':
        case '6':
        {
            int idx = pane_indices[sel];
            int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
            enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
            enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);

            if (field == 0)
            {
                set_sdl_pane_enabled(idx, (delta > 0));
            }
            else if (field == 1)
            {
                set_sdl_pane_where(idx, pane_next_allowed_placement(type, where, delta));
            }
            else if (field == 2)
            {
                int rows = get_sdl_pane_rows(idx);

                if (supporting_pane_rows_locked(pane_indices, pane_count, idx))
                {
                    bell("Rows are shared within each bottom slot");
                    break;
                }
                if (rows == 0)
                    set_sdl_pane_rows(idx, get_sdl_pane_current_rows(idx));
                else
                    set_sdl_pane_rows(idx, rows + delta);
            }
            else
            {
                int cols = get_sdl_pane_cols(idx);

                if (supporting_pane_cols_locked(pane_indices, pane_count, idx))
                {
                    bell("Cols are shared within each side/corner slot");
                    break;
                }
                if (cols == 0)
                    set_sdl_pane_cols(idx, get_sdl_pane_current_cols(idx));
                else
                    set_sdl_pane_cols(idx, cols + delta);
            }

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        default:
            bell("Illegal command for pane layout editor!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    settings_semantic_menu_hide();
    screen_load();
}

static const int touch_pane_main_action_choices[] = {
    GAMEPAD_BIND_NONE,
    TOUCH_BIND_TOP_PANEL_OPEN, TOUCH_BIND_TOP_PANEL_CLOSE,
    TOUCH_BIND_MAIN_MENU_KNOWLEDGE, TOUCH_BIND_MAIN_MENU_HINTS_QUESTS,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', 'y', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', KTRL('A'), 'E', 't', 'p', 'q',
    'F', KTRL('F'), 'S', 'l', 'b', 'L', 'm',
    KTRL('Q'), '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int touch_pane_second_action_choices[] = {
    TOUCH_PANE_BIND_INHERIT, GAMEPAD_BIND_NONE,
    TOUCH_BIND_TOP_PANEL_OPEN, TOUCH_BIND_TOP_PANEL_CLOSE,
    TOUCH_BIND_MAIN_MENU_KNOWLEDGE, TOUCH_BIND_MAIN_MENU_HINTS_QUESTS,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', 'y', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', KTRL('A'), 'E', 't', 'p', 'q',
    'F', KTRL('F'), 'S', 'l', 'b', 'L', 'm',
    KTRL('Q'), '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

/* Quick-access and thumb buttons share a compact, command-oriented picker.
 * Movement, pane-management, raw-key, and duplicate command entries belong
 * to the full touch-pane editor, not to these contextual shortcuts. */
static const int touch_context_action_choices[] = {
    GAMEPAD_BIND_NONE,
    'm',
    TOUCH_BIND_MAIN_MENU_KNOWLEDGE, TOUCH_BIND_MAIN_MENU_HINTS_QUESTS,
    TOUCH_BIND_TOGGLE_TILES,
    INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    'a', 'x',
    'M', 'h', 'y', '\t',
    'z', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', KTRL('A'), 't', 'p', 'q',
    'F', KTRL('F'), 'S', 'l', 'b',
    KTRL('Q'), KTRL('Y'), 'J', '0', '?', 'O',
};

static const int touch_pane_visible_button_slots[SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT] = {
    0, 1, 2,
    6, 7, 8,
    9, 10, 11,
    12, 13, 14,
    15, 16, 17,
    3, 4, 5,
    18, 19, 20,
};

static int touch_pane_visible_button_index(int visible_index)
{
    if (visible_index < 0 || visible_index >= SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT)
        return -1;

    return touch_pane_visible_button_slots[visible_index];
}

static const int* touch_pane_action_choices_for_panel(int panel, int* count)
{
    if (count)
        *count = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
            ? (int)N_ELEMENTS(touch_pane_second_action_choices)
            : (int)N_ELEMENTS(touch_pane_main_action_choices);

    return (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? touch_pane_second_action_choices
        : touch_pane_main_action_choices;
}

static const int* touch_swipe_action_choices(int* count)
{
    if (count)
        *count = (int)N_ELEMENTS(touch_pane_main_action_choices);

    return touch_pane_main_action_choices;
}

static int touch_action_choice_index(const int* choices, int count, int binding)
{
    for (int i = 0; i < count; i++)
    {
        if (choices[i] == binding)
            return i;
    }
    return 0;
}

static int touch_pane_action_choice_index(int panel, int binding)
{
    int count = 0;
    const int* choices = touch_pane_action_choices_for_panel(panel, &count);

    return touch_action_choice_index(choices, count, binding);
}

static bool touch_pane_binding_is_confirm(int binding)
{
    return (binding == INPUT_BIND_CONFIRM || binding == ' ' || binding == '\r');
}

static bool touch_pane_main_panel_has_other_confirm(int skip_index)
{
    for (int i = 0; i < SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT; i++)
    {
        int button_index = touch_pane_visible_button_index(i);

        if (button_index < 0 || button_index == skip_index)
            continue;

        if (touch_pane_binding_is_confirm(
                get_sdl_touch_pane_binding_for_panel(SDL_TOUCH_PANE_PANEL_MAIN,
                    button_index)))
            return true;
    }

    return false;
}

static bool touch_pane_main_confirm_change_allowed(int panel, int index, int new_binding)
{
    int current_binding;

    if (panel != SDL_TOUCH_PANE_PANEL_MAIN)
        return true;

    current_binding = get_sdl_touch_pane_binding_for_panel(panel, index);
    if (!touch_pane_binding_is_confirm(current_binding))
        return true;
    if (touch_pane_binding_is_confirm(new_binding))
        return true;

    return touch_pane_main_panel_has_other_confirm(index);
}

static void touch_pane_action_label_for_panel(int panel, int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (binding == TOUCH_PANE_BIND_INHERIT) {
        SDL_strlcpy(buf, "Main panel button", buflen);
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
        get_sdl_touch_pane_panel_name((panel == SDL_TOUCH_PANE_PANEL_SECOND)
                ? SDL_TOUCH_PANE_PANEL_MAIN
                : SDL_TOUCH_PANE_PANEL_SECOND,
            panel_name, sizeof(panel_name));
        strnfmt(buf, buflen, "Switch to %s", panel_name);
        return;
    }

    if (binding == INPUT_BIND_CONFIRM || binding == ' ') {
        SDL_strlcpy(buf, "Confirm (pick)", buflen);
        return;
    }

    binding_action_short(binding, buf, buflen);
}

static void touch_corner_action_label(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    switch (binding) {
    case 'f':
        SDL_strlcpy(buf, "Shoot", buflen);
        return;
    case 'F':
        SDL_strlcpy(buf, "Shoot 2", buflen);
        return;
    default:
        touch_pane_action_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, binding,
            buf, buflen);
        return;
    }
}

static bool settings_choice_value_seen(const struct settings_value_choice* choices,
    int count, int value)
{
    for (int i = 0; i < count; i++)
    {
        if (choices[i].value == value)
            return true;
    }

    return false;
}

static bool touch_pick_action_value_buttons(cptr title, int panel,
    bool corner_labels, const int* values, int value_count, int current_value,
    const ui_question_button* buttons, int button_count, int* out_value,
    int* out_button_choice)
{
    struct settings_value_choice choices[SETTINGS_VALUE_PICKER_MAX];
    char labels[SETTINGS_VALUE_PICKER_MAX][80];
    int count = 0;

    if (!values || value_count <= 0 || !out_value)
        return false;

    for (int i = 0; i < value_count && count < SETTINGS_VALUE_PICKER_MAX; i++)
    {
        int value = values[i];

        if (settings_choice_value_seen(choices, count, value))
            continue;

        if (corner_labels)
            touch_corner_action_label(value, labels[count],
                sizeof(labels[count]));
        else
            touch_pane_action_label_for_panel(panel, value, labels[count],
                sizeof(labels[count]));

        choices[count].value = value;
        choices[count].label = labels[count];
        count++;
    }

    return settings_pick_value_ex_buttons(title ? title : "Choose Action",
        NULL, choices, count, current_value, out_value, false, buttons,
        button_count, out_button_choice);
}

static bool touch_pick_action_value(cptr title, int panel, bool corner_labels,
    const int* values, int value_count, int current_value, int* out_value)
{
    return touch_pick_action_value_buttons(title, panel, corner_labels, values,
        value_count, current_value, NULL, 0, out_value, NULL);
}

enum {
    TOUCH_CONTROL_PANE_ENABLED = 0,
    TOUCH_CONTROL_PANE_DEFAULT_OPEN,
    TOUCH_CONTROL_PANE_KEY_LABELS,
    TOUCH_CONTROL_PANE_INVENTORY_EQUIPMENT_CYCLE,
    TOUCH_CONTROL_PANE_PLACEMENT,
    TOUCH_CONTROL_MENU_INVENTORY_EQUIPMENT,
    TOUCH_CONTROL_MENU_SUPPLY,
    TOUCH_CONTROL_MENU_OTHER,
    TOUCH_CONTROL_MOVEMENT,
    TOUCH_CONTROL_ROUND_MOVEMENT_LAYER,
    TOUCH_CONTROL_CORNER_BUTTON_OVERLAY,
    TOUCH_CONTROL_CORNER_UP_DOWN_SIDE,
    TOUCH_CONTROL_CORNER_TOP_TAP,
    TOUCH_CONTROL_CORNER_TOP_LONG_TAP,
    TOUCH_CONTROL_CORNER_BOTTOM_TAP,
    TOUCH_CONTROL_CORNER_BOTTOM_LONG_TAP,
    TOUCH_CONTROL_CENTER_LEFT_TAP,
    TOUCH_CONTROL_CENTER_LEFT_LONG_TAP,
    TOUCH_CONTROL_CENTER_RIGHT_TAP,
    TOUCH_CONTROL_CENTER_RIGHT_LONG_TAP,
    TOUCH_CONTROL_SWIPE_ENABLED,
    TOUCH_CONTROL_SWIPE_UP,
    TOUCH_CONTROL_SWIPE_DOWN,
    TOUCH_CONTROL_SWIPE_LEFT,
    TOUCH_CONTROL_SWIPE_RIGHT,
    TOUCH_CONTROL_COUNT
};

enum {
    TOUCH_TOP_WIDGET_BUTTON_1_TAP = 0,
    TOUCH_TOP_WIDGET_BUTTON_1_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_2_TAP,
    TOUCH_TOP_WIDGET_BUTTON_2_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_3_TAP,
    TOUCH_TOP_WIDGET_BUTTON_3_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_4_TAP,
    TOUCH_TOP_WIDGET_BUTTON_4_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_5_TAP,
    TOUCH_TOP_WIDGET_BUTTON_5_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_6_TAP,
    TOUCH_TOP_WIDGET_BUTTON_6_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_7_TAP,
    TOUCH_TOP_WIDGET_BUTTON_7_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_8_TAP,
    TOUCH_TOP_WIDGET_BUTTON_8_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_9_TAP,
    TOUCH_TOP_WIDGET_BUTTON_9_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_10_TAP,
    TOUCH_TOP_WIDGET_BUTTON_10_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_11_TAP,
    TOUCH_TOP_WIDGET_BUTTON_11_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_12_TAP,
    TOUCH_TOP_WIDGET_BUTTON_12_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_13_TAP,
    TOUCH_TOP_WIDGET_BUTTON_13_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_14_TAP,
    TOUCH_TOP_WIDGET_BUTTON_14_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_15_TAP,
    TOUCH_TOP_WIDGET_BUTTON_15_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_16_TAP,
    TOUCH_TOP_WIDGET_BUTTON_16_LONG_TAP,
    TOUCH_TOP_WIDGET_BUTTON_COUNT
};

enum {
    TOUCH_THUMB_BUTTON_1_TAP = 0,
    TOUCH_THUMB_BUTTON_1_LONG_TAP,
    TOUCH_THUMB_BUTTON_2_TAP,
    TOUCH_THUMB_BUTTON_2_LONG_TAP,
    TOUCH_THUMB_BUTTON_ROW_COUNT
};

static bool touch_control_is_menu_command_row(int row)
{
    return row >= TOUCH_CONTROL_MENU_INVENTORY_EQUIPMENT
        && row <= TOUCH_CONTROL_MENU_OTHER;
}

static int touch_control_menu_category_for_row(int row)
{
    switch (row) {
    case TOUCH_CONTROL_MENU_INVENTORY_EQUIPMENT:
        return SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT;
    case TOUCH_CONTROL_MENU_SUPPLY:
        return SDL_TOUCH_MENU_CATEGORY_SUPPLY;
    case TOUCH_CONTROL_MENU_OTHER:
        return SDL_TOUCH_MENU_CATEGORY_OTHER;
    default:
        return SDL_TOUCH_MENU_CATEGORY_OTHER;
    }
}

typedef enum {
    TOUCH_CONTROL_BINDING_TOP_PANEL,
    TOUCH_CONTROL_BINDING_CENTER_ZONE,
    TOUCH_CONTROL_BINDING_CORNER_ACTION,
    TOUCH_CONTROL_BINDING_SWIPE,
    TOUCH_CONTROL_BINDING_THUMB,
} touch_control_binding_kind;

typedef struct {
    int row;
    cptr label;
    touch_control_binding_kind kind;
    int index;
    bool long_press;
} touch_control_binding_row;

static const touch_control_binding_row touch_control_binding_rows[] = {
    { TOUCH_CONTROL_CORNER_TOP_TAP, "Corner Top Tap",
        TOUCH_CONTROL_BINDING_CORNER_ACTION, SDL_TOUCH_CORNER_ACTION_TOP_TAP, false },
    { TOUCH_CONTROL_CORNER_TOP_LONG_TAP, "Corner Top Long Tap",
        TOUCH_CONTROL_BINDING_CORNER_ACTION, SDL_TOUCH_CORNER_ACTION_TOP_LONG_TAP, false },
    { TOUCH_CONTROL_CORNER_BOTTOM_TAP, "Corner Bottom Tap",
        TOUCH_CONTROL_BINDING_CORNER_ACTION, SDL_TOUCH_CORNER_ACTION_BOTTOM_TAP, false },
    { TOUCH_CONTROL_CORNER_BOTTOM_LONG_TAP, "Corner Bottom Long Tap",
        TOUCH_CONTROL_BINDING_CORNER_ACTION, SDL_TOUCH_CORNER_ACTION_BOTTOM_LONG_TAP, false },
    { TOUCH_CONTROL_CENTER_LEFT_TAP, "Left Center Tap",
        TOUCH_CONTROL_BINDING_CENTER_ZONE, SDL_TOUCH_ZONE_CENTER_LEFT_TAP, false },
    { TOUCH_CONTROL_CENTER_LEFT_LONG_TAP, "Left Center Long Tap",
        TOUCH_CONTROL_BINDING_CENTER_ZONE, SDL_TOUCH_ZONE_CENTER_LEFT_LONG_TAP, false },
    { TOUCH_CONTROL_CENTER_RIGHT_TAP, "Right Center Tap",
        TOUCH_CONTROL_BINDING_CENTER_ZONE, SDL_TOUCH_ZONE_CENTER_RIGHT_TAP, false },
    { TOUCH_CONTROL_CENTER_RIGHT_LONG_TAP, "Right Center Long Tap",
        TOUCH_CONTROL_BINDING_CENTER_ZONE, SDL_TOUCH_ZONE_CENTER_RIGHT_LONG_TAP, false },
    { TOUCH_CONTROL_SWIPE_UP, "Swipe Up",
        TOUCH_CONTROL_BINDING_SWIPE, TOUCH_SWIPE_DIR_UP, false },
    { TOUCH_CONTROL_SWIPE_DOWN, "Swipe Down",
        TOUCH_CONTROL_BINDING_SWIPE, TOUCH_SWIPE_DIR_DOWN, false },
    { TOUCH_CONTROL_SWIPE_LEFT, "Swipe Left",
        TOUCH_CONTROL_BINDING_SWIPE, TOUCH_SWIPE_DIR_LEFT, false },
    { TOUCH_CONTROL_SWIPE_RIGHT, "Swipe Right",
        TOUCH_CONTROL_BINDING_SWIPE, TOUCH_SWIPE_DIR_RIGHT, false },
};

static const touch_control_binding_row touch_top_widget_binding_rows[] = {
    { TOUCH_TOP_WIDGET_BUTTON_1_TAP, "Quick Access 1 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 0, false },
    { TOUCH_TOP_WIDGET_BUTTON_1_LONG_TAP, "Quick Access 1 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 0, true },
    { TOUCH_TOP_WIDGET_BUTTON_2_TAP, "Quick Access 2 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 1, false },
    { TOUCH_TOP_WIDGET_BUTTON_2_LONG_TAP, "Quick Access 2 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 1, true },
    { TOUCH_TOP_WIDGET_BUTTON_3_TAP, "Quick Access 3 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 2, false },
    { TOUCH_TOP_WIDGET_BUTTON_3_LONG_TAP, "Quick Access 3 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 2, true },
    { TOUCH_TOP_WIDGET_BUTTON_4_TAP, "Quick Access 4 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 3, false },
    { TOUCH_TOP_WIDGET_BUTTON_4_LONG_TAP, "Quick Access 4 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 3, true },
    { TOUCH_TOP_WIDGET_BUTTON_5_TAP, "Quick Access 5 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 4, false },
    { TOUCH_TOP_WIDGET_BUTTON_5_LONG_TAP, "Quick Access 5 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 4, true },
    { TOUCH_TOP_WIDGET_BUTTON_6_TAP, "Quick Access 6 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 5, false },
    { TOUCH_TOP_WIDGET_BUTTON_6_LONG_TAP, "Quick Access 6 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 5, true },
    { TOUCH_TOP_WIDGET_BUTTON_7_TAP, "Quick Access 7 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 6, false },
    { TOUCH_TOP_WIDGET_BUTTON_7_LONG_TAP, "Quick Access 7 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 6, true },
    { TOUCH_TOP_WIDGET_BUTTON_8_TAP, "Quick Access 8 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 7, false },
    { TOUCH_TOP_WIDGET_BUTTON_8_LONG_TAP, "Quick Access 8 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 7, true },
    { TOUCH_TOP_WIDGET_BUTTON_9_TAP, "Quick Access 9 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 8, false },
    { TOUCH_TOP_WIDGET_BUTTON_9_LONG_TAP, "Quick Access 9 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 8, true },
    { TOUCH_TOP_WIDGET_BUTTON_10_TAP, "Quick Access 10 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 9, false },
    { TOUCH_TOP_WIDGET_BUTTON_10_LONG_TAP, "Quick Access 10 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 9, true },
    { TOUCH_TOP_WIDGET_BUTTON_11_TAP, "Quick Access 11 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 10, false },
    { TOUCH_TOP_WIDGET_BUTTON_11_LONG_TAP, "Quick Access 11 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 10, true },
    { TOUCH_TOP_WIDGET_BUTTON_12_TAP, "Quick Access 12 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 11, false },
    { TOUCH_TOP_WIDGET_BUTTON_12_LONG_TAP, "Quick Access 12 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 11, true },
    { TOUCH_TOP_WIDGET_BUTTON_13_TAP, "Quick Access 13 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 12, false },
    { TOUCH_TOP_WIDGET_BUTTON_13_LONG_TAP, "Quick Access 13 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 12, true },
    { TOUCH_TOP_WIDGET_BUTTON_14_TAP, "Quick Access 14 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 13, false },
    { TOUCH_TOP_WIDGET_BUTTON_14_LONG_TAP, "Quick Access 14 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 13, true },
    { TOUCH_TOP_WIDGET_BUTTON_15_TAP, "Quick Access 15 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 14, false },
    { TOUCH_TOP_WIDGET_BUTTON_15_LONG_TAP, "Quick Access 15 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 14, true },
    { TOUCH_TOP_WIDGET_BUTTON_16_TAP, "Quick Access 16 Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 15, false },
    { TOUCH_TOP_WIDGET_BUTTON_16_LONG_TAP, "Quick Access 16 Long Tap",
        TOUCH_CONTROL_BINDING_TOP_PANEL, 15, true },
};

static const touch_control_binding_row touch_thumb_binding_rows[] = {
    { TOUCH_THUMB_BUTTON_1_TAP, "Thumb 1 Tap",
        TOUCH_CONTROL_BINDING_THUMB, 0, false },
    { TOUCH_THUMB_BUTTON_1_LONG_TAP, "Thumb 1 Long Tap",
        TOUCH_CONTROL_BINDING_THUMB, 0, true },
    { TOUCH_THUMB_BUTTON_2_TAP, "Thumb 2 Tap",
        TOUCH_CONTROL_BINDING_THUMB, 1, false },
    { TOUCH_THUMB_BUTTON_2_LONG_TAP, "Thumb 2 Long Tap",
        TOUCH_CONTROL_BINDING_THUMB, 1, true },
};

static const touch_control_binding_row* touch_control_binding_for_row(int row)
{
    for (int i = 0; i < (int)N_ELEMENTS(touch_control_binding_rows); i++) {
        if (touch_control_binding_rows[i].row == row)
            return &touch_control_binding_rows[i];
    }

    return NULL;
}

static const touch_control_binding_row* touch_top_widget_binding_for_row(int row)
{
    for (int i = 0; i < (int)N_ELEMENTS(touch_top_widget_binding_rows); i++) {
        if (touch_top_widget_binding_rows[i].row == row)
            return &touch_top_widget_binding_rows[i];
    }

    return NULL;
}

static const touch_control_binding_row* touch_thumb_binding_for_row(int row)
{
    for (int i = 0; i < (int)N_ELEMENTS(touch_thumb_binding_rows); i++) {
        if (touch_thumb_binding_rows[i].row == row)
            return &touch_thumb_binding_rows[i];
    }

    return NULL;
}

static int touch_control_binding_value(const touch_control_binding_row* binding)
{
    if (!binding)
        return GAMEPAD_BIND_NONE;

    switch (binding->kind) {
    case TOUCH_CONTROL_BINDING_TOP_PANEL:
        return get_sdl_touch_top_panel_binding(binding->index,
            binding->long_press);
    case TOUCH_CONTROL_BINDING_CENTER_ZONE:
        return get_sdl_touch_zone_center_binding(binding->index);
    case TOUCH_CONTROL_BINDING_CORNER_ACTION:
        return get_sdl_touch_corner_action_binding(binding->index);
    case TOUCH_CONTROL_BINDING_SWIPE:
        return get_sdl_touch_swipe_binding(binding->index);
    case TOUCH_CONTROL_BINDING_THUMB:
        return get_sdl_touch_thumb_binding(binding->index,
            binding->long_press);
    default:
        return GAMEPAD_BIND_NONE;
    }
}

static int touch_control_binding_default_value(
    const touch_control_binding_row* binding)
{
    if (!binding)
        return GAMEPAD_BIND_NONE;

    switch (binding->kind) {
    case TOUCH_CONTROL_BINDING_TOP_PANEL:
        return get_sdl_touch_top_panel_default_binding(binding->index,
            binding->long_press);
    case TOUCH_CONTROL_BINDING_CENTER_ZONE:
        return get_sdl_touch_zone_center_default_binding(binding->index);
    case TOUCH_CONTROL_BINDING_CORNER_ACTION:
        return get_sdl_touch_corner_action_default_binding(binding->index);
    case TOUCH_CONTROL_BINDING_SWIPE:
        return get_sdl_touch_swipe_default_binding(binding->index);
    case TOUCH_CONTROL_BINDING_THUMB:
        return get_sdl_touch_thumb_default_binding(binding->index,
            binding->long_press);
    default:
        return GAMEPAD_BIND_NONE;
    }
}

static void touch_control_set_binding(const touch_control_binding_row* binding,
    int value)
{
    if (!binding)
        return;

    switch (binding->kind) {
    case TOUCH_CONTROL_BINDING_TOP_PANEL:
        set_sdl_touch_top_panel_binding(binding->index, binding->long_press,
            value);
        break;
    case TOUCH_CONTROL_BINDING_CENTER_ZONE:
        set_sdl_touch_zone_center_binding(binding->index, value);
        break;
    case TOUCH_CONTROL_BINDING_CORNER_ACTION:
        set_sdl_touch_corner_action_binding(binding->index, value);
        break;
    case TOUCH_CONTROL_BINDING_SWIPE:
        set_sdl_touch_swipe_binding(binding->index, value);
        break;
    case TOUCH_CONTROL_BINDING_THUMB:
        set_sdl_touch_thumb_binding(binding->index, binding->long_press,
            value);
        break;
    default:
        break;
    }
}

static const int* touch_control_binding_choices(
    const touch_control_binding_row* binding, int* count)
{
    if (binding && (binding->kind == TOUCH_CONTROL_BINDING_TOP_PANEL
            || binding->kind == TOUCH_CONTROL_BINDING_THUMB)) {
        if (count)
            *count = (int)N_ELEMENTS(touch_context_action_choices);
        return touch_context_action_choices;
    }

    if (binding && binding->kind == TOUCH_CONTROL_BINDING_SWIPE)
        return touch_swipe_action_choices(count);

    return touch_pane_action_choices_for_panel(SDL_TOUCH_PANE_PANEL_MAIN,
        count);
}

static bool touch_control_pick_binding(const touch_control_binding_row* binding,
    cptr title)
{
    int choice_count = 0;
    const int* choices;
    int current;
    int picked;

    if (!binding)
        return false;

    choices = touch_control_binding_choices(binding, &choice_count);
    if (!choices || choice_count <= 0)
        return false;

    current = touch_control_binding_value(binding);
    if (!touch_pick_action_value(title ? title : binding->label,
            SDL_TOUCH_PANE_PANEL_MAIN,
            binding->kind == TOUCH_CONTROL_BINDING_CORNER_ACTION,
            choices, choice_count, current, &picked)
        || picked == current)
    {
        return false;
    }

    touch_control_set_binding(binding, picked);
    return true;
}

static bool touch_control_binding_label(int row, char* buf, size_t buflen)
{
    const touch_control_binding_row* binding = touch_control_binding_for_row(row);

    if (!binding)
        return false;

    if (binding->kind == TOUCH_CONTROL_BINDING_CORNER_ACTION) {
        touch_corner_action_label(touch_control_binding_value(binding), buf,
            buflen);
        return true;
    }

    touch_pane_action_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN,
        touch_control_binding_value(binding), buf, buflen);
    return true;
}

static bool touch_control_cycle_binding_row(int row, int delta)
{
    const touch_control_binding_row* binding = touch_control_binding_for_row(row);
    int choice_count = 0;
    const int* choices;
    int idx;

    if (!binding)
        return false;

    choices = touch_control_binding_choices(binding, &choice_count);
    if (!choices || choice_count <= 0)
        return false;

    idx = touch_action_choice_index(choices, choice_count,
        touch_control_binding_value(binding));
    idx = ((idx + delta) % choice_count + choice_count) % choice_count;
    touch_control_set_binding(binding, choices[idx]);
    return true;
}

static bool touch_control_reset_binding_row(int row)
{
    const touch_control_binding_row* binding = touch_control_binding_for_row(row);

    if (!binding)
        return false;

    touch_control_set_binding(binding,
        touch_control_binding_default_value(binding));
    return true;
}

static bool touch_top_widget_binding_label(int row, char* buf, size_t buflen)
{
    const touch_control_binding_row* binding = touch_top_widget_binding_for_row(row);
    int value;

    if (!binding)
        return false;

    value = touch_control_binding_value(binding);
    switch (value) {
    case 'a':
        SDL_strlcpy(buf, "Staff", buflen);
        break;
    case 'l':
        SDL_strlcpy(buf, "View", buflen);
        break;
    case 'j':
        SDL_strlcpy(buf, "Supply", buflen);
        break;
    case 'p':
        SDL_strlcpy(buf, "Horn", buflen);
        break;
    case 'f':
        SDL_strlcpy(buf, "Fire 1st quiver", buflen);
        break;
    case 'F':
        SDL_strlcpy(buf, "Fire 2nd quiver", buflen);
        break;
    case 'Z':
        SDL_strlcpy(buf, "Rest", buflen);
        break;
    default:
        touch_pane_action_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN,
            value, buf, buflen);
        break;
    }
    return true;
}

static bool touch_top_widget_remove_cell(int slot)
{
    int count = get_sdl_touch_top_panel_cell_count();

    if (slot < 0 || slot >= count)
        return false;

    for (int i = slot; i < count - 1; i++) {
        set_sdl_touch_top_panel_binding(i, false,
            get_sdl_touch_top_panel_binding(i + 1, false));
        set_sdl_touch_top_panel_binding(i, true,
            get_sdl_touch_top_panel_binding(i + 1, true));
    }
    set_sdl_touch_top_panel_binding(count - 1, false, GAMEPAD_BIND_NONE);
    set_sdl_touch_top_panel_binding(count - 1, true, GAMEPAD_BIND_NONE);
    set_sdl_touch_top_panel_cell_count(count - 1);
    return true;
}

static bool touch_top_widget_cycle_binding_row(int row, int delta)
{
    const touch_control_binding_row* binding = touch_top_widget_binding_for_row(row);
    int choice_count = 0;
    const int* choices;
    int idx;

    if (!binding)
        return false;

    choices = touch_control_binding_choices(binding, &choice_count);
    if (!choices || choice_count <= 0)
        return false;

    idx = touch_action_choice_index(choices, choice_count,
        touch_control_binding_value(binding));
    idx = ((idx + delta) % choice_count + choice_count) % choice_count;
    if (choices[idx] == GAMEPAD_BIND_NONE)
        return touch_top_widget_remove_cell(binding->index);
    else
        touch_control_set_binding(binding, choices[idx]);
    return true;
}

static bool touch_top_widget_reset_binding_row(int row)
{
    const touch_control_binding_row* binding = touch_top_widget_binding_for_row(row);
    int value;

    if (!binding)
        return false;

    value = touch_control_binding_default_value(binding);
    if (value == GAMEPAD_BIND_NONE)
        return touch_top_widget_remove_cell(binding->index);
    touch_control_set_binding(binding, value);
    return true;
}

static bool touch_top_widget_pick_binding_row(int row)
{
    const touch_control_binding_row* binding = touch_top_widget_binding_for_row(row);

    return binding ? do_cmd_touch_top_widget_pick_button(binding->index) : false;
}

static int touch_top_widget_editor_binding_row(int row)
{
    if (row < 0 || row >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return -1;
    return row * 2;
}

static void touch_top_widget_format_size(char* buf, size_t buflen, float size)
{
    int quarters;

    if (!buf || !buflen)
        return;
    if (size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH) {
        SDL_strlcpy(buf, "Stretch", buflen);
        return;
    }

    quarters = (int)(size * 4.0f + 0.5f);
    if ((quarters % 4) == 0)
        strnfmt(buf, buflen, "%dx", quarters / 4);
    else if ((quarters % 2) == 0)
        strnfmt(buf, buflen, "%d.5x", quarters / 4);
    else
        strnfmt(buf, buflen, "%d.%s5x", quarters / 4,
            (quarters % 4) == 1 ? "2" : "7");
}

static bool touch_top_widget_pick_size(void)
{
    struct settings_value_choice choices[32];
    char labels[32][12];
    float current_size = get_sdl_touch_top_panel_size();
    int current = (current_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH)
        ? 0 : (int)(current_size * 4.0f + 0.5f);
    int picked = current;
    int count = 0;

    choices[count].value = 0;
    choices[count].label = "Stretch";
    count++;
    for (int quarters = 4; quarters <= 32; quarters++) {
        touch_top_widget_format_size(labels[count], sizeof(labels[count]),
            (float)quarters / 4.0f);
        choices[count].value = quarters;
        choices[count].label = labels[count];
        count++;
    }

    if (!settings_pick_value_ex("Quick Access Size",
            "Stretch fills the clear horizontal space. Fixed sizes use quarter-step scaling.",
            choices, count, current, &picked, false)
        || picked == current)
    {
        return false;
    }

    set_sdl_touch_top_panel_size((picked == 0)
        ? SDL_TOUCH_TOP_PANEL_SIZE_STRETCH : (float)picked / 4.0f);
    return true;
}

bool do_cmd_touch_top_widget_pick_button(int slot)
{
    const touch_control_binding_row* binding;
    char title[48];
    int choice_count = 0;
    const int* choices;
    int current;
    int picked = GAMEPAD_BIND_NONE;
    int button_choice = 0;
    bool changed = false;

    while (true)
    {
        char row_label[16];
        int cell_count = get_sdl_touch_top_panel_cell_count();
        ui_question_button buttons[] = {
            { SETTINGS_CLICK_QUICK_ACCESS_SIZE, 's', "Size", TERM_L_WHITE,
                false },
            { SETTINGS_CLICK_QUICK_ACCESS_ADD_CELL, 'a', "Add Cell",
                TERM_L_WHITE,
                cell_count >= SDL_TOUCH_TOP_PANEL_CELL_COUNT_MAX },
            { SETTINGS_CLICK_QUICK_ACCESS_ROW, 'r', row_label, TERM_L_WHITE,
                false },
        };

        if (slot < 0 || slot >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
            break;

        SDL_strlcpy(row_label,
            (get_sdl_touch_top_panel_rows() < SDL_TOUCH_TOP_PANEL_ROWS_MAX)
                ? "Add Row" : "Remove Row",
            sizeof(row_label));

        binding = touch_top_widget_binding_for_row(slot * 2);
        if (!binding)
            break;

        choices = touch_control_binding_choices(binding, &choice_count);
        if (!choices || choice_count <= 0)
            break;

        strnfmt(title, sizeof(title), "Quick Access %d", slot + 1);
        current = touch_control_binding_value(binding);
        if (!touch_pick_action_value_buttons(title,
                SDL_TOUCH_PANE_PANEL_MAIN, false, choices, choice_count,
                current, buttons, (int)N_ELEMENTS(buttons), &picked,
                &button_choice))
        {
            break;
        }

        if (button_choice == SETTINGS_CLICK_QUICK_ACCESS_SIZE)
        {
            changed |= touch_top_widget_pick_size();
            continue;
        }

        if (button_choice == SETTINGS_CLICK_QUICK_ACCESS_ADD_CELL)
        {
            if (cell_count >= SDL_TOUCH_TOP_PANEL_CELL_COUNT_MAX) {
                bell("Quick access is already at the maximum number of cells.");
                continue;
            }
            slot = cell_count;
            continue;
        }

        if (button_choice == SETTINGS_CLICK_QUICK_ACCESS_ROW)
        {
            int rows = get_sdl_touch_top_panel_rows();

            set_sdl_touch_top_panel_rows(
                rows < SDL_TOUCH_TOP_PANEL_ROWS_MAX ? rows + 1 : rows - 1);
            changed = true;
            continue;
        }

        if (picked == GAMEPAD_BIND_NONE)
        {
            if (slot < cell_count)
                changed |= touch_top_widget_remove_cell(slot);
            break;
        }

        if (picked != current || slot >= cell_count)
        {
            touch_control_set_binding(binding, picked);
            if (slot >= cell_count)
                set_sdl_touch_top_panel_cell_count(slot + 1);
            changed = true;
        }
        break;
    }

    if (changed)
    {
        if (!save_pane_config_to_json())
            msg_print("Quick access changed, but saving sil_sdl.json failed.");
    }

    return changed;
}

static bool touch_thumb_binding_label(int row, char* buf, size_t buflen)
{
    const touch_control_binding_row* binding = touch_thumb_binding_for_row(row);

    if (!binding)
        return false;

    touch_pane_action_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN,
        touch_control_binding_value(binding), buf, buflen);
    return true;
}

static bool touch_thumb_cycle_binding_row(int row, int delta)
{
    const touch_control_binding_row* binding = touch_thumb_binding_for_row(row);
    int choice_count = 0;
    const int* choices;
    int idx;

    if (!binding)
        return false;

    choices = touch_control_binding_choices(binding, &choice_count);
    if (!choices || choice_count <= 0)
        return false;

    idx = touch_action_choice_index(choices, choice_count,
        touch_control_binding_value(binding));
    idx = ((idx + delta) % choice_count + choice_count) % choice_count;
    touch_control_set_binding(binding, choices[idx]);
    return true;
}

static bool touch_thumb_reset_binding_row(int row)
{
    const touch_control_binding_row* binding = touch_thumb_binding_for_row(row);

    if (!binding)
        return false;

    touch_control_set_binding(binding,
        touch_control_binding_default_value(binding));
    return true;
}

static bool touch_thumb_pick_binding_row(int row)
{
    const touch_control_binding_row* binding = touch_thumb_binding_for_row(row);

    return touch_control_pick_binding(binding, binding ? binding->label : NULL);
}

static void touch_thumb_reset_buttons_to_default(void)
{
    for (int i = 0; i < TOUCH_THUMB_BUTTON_ROW_COUNT; i++)
        (void)touch_thumb_reset_binding_row(i);
    set_sdl_touch_thumb_enabled(get_sdl_touch_thumb_default_enabled());
}

static const char* touch_control_row_name(int row)
{
    const touch_control_binding_row* binding = touch_control_binding_for_row(row);

    if (binding)
        return binding->label;

    switch (row) {
    case TOUCH_CONTROL_PANE_ENABLED:
        return "Touch Pane";
    case TOUCH_CONTROL_PANE_DEFAULT_OPEN:
        return "Touch Pane Starts";
    case TOUCH_CONTROL_PANE_KEY_LABELS:
        return "Touch Pane Key Labels";
    case TOUCH_CONTROL_PANE_INVENTORY_EQUIPMENT_CYCLE:
        return "Inv/Equip Pane Cycle";
    case TOUCH_CONTROL_PANE_PLACEMENT:
        return "Touch Pane Side";
    case TOUCH_CONTROL_MENU_INVENTORY_EQUIPMENT:
        return "Inventory/Equipped Touch";
    case TOUCH_CONTROL_MENU_SUPPLY:
        return "Supply Touch";
    case TOUCH_CONTROL_MENU_OTHER:
        return "Other Menus Touch";
    case TOUCH_CONTROL_MOVEMENT:
        return "Touch Movement";
    case TOUCH_CONTROL_ROUND_MOVEMENT_LAYER:
        return "Round Movement Layer";
    case TOUCH_CONTROL_CORNER_BUTTON_OVERLAY:
        return "Corner Button Overlay";
    case TOUCH_CONTROL_CORNER_UP_DOWN_SIDE:
        return "Corner Up/Down Side";
    case TOUCH_CONTROL_SWIPE_ENABLED:
        return "Swipe Gestures";
    default:
        return "";
    }
}

static const char* touch_profile_label(int profile)
{
    switch (profile) {
    case SDL_TOUCH_PROFILE_CORNERS:
        return "Corners + quick access";
    case SDL_TOUCH_PROFILE_ROUND_WHEEL:
        return "Button wheel + quick access";
    case SDL_TOUCH_PROFILE_TOUCH_PANE:
    default:
        return "Touch pane + touch screen";
    }
}

static const char* touch_movement_mode_label(int mode)
{
    switch (mode) {
    case SDL_TOUCH_MOVEMENT_OFF:
        return "Off";
    case SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY:
        return "Long click only";
    case SDL_TOUCH_MOVEMENT_ON:
    default:
        return "On";
    }
}

static int touch_movement_mode_cycle(int mode, int delta)
{
    static const int modes[] = {
        SDL_TOUCH_MOVEMENT_ON,
        SDL_TOUCH_MOVEMENT_OFF,
        SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY,
    };
    int idx = 0;

    for (int i = 0; i < (int)N_ELEMENTS(modes); i++) {
        if (modes[i] == mode) {
            idx = i;
            break;
        }
    }

    idx = ((idx + delta) % (int)N_ELEMENTS(modes) + (int)N_ELEMENTS(modes))
        % (int)N_ELEMENTS(modes);
    return modes[idx];
}

static const char* touch_zone_overlay_mode_label(int mode)
{
    switch (mode) {
    case SDL_TOUCH_ZONE_OVERLAY_OFF:
        return "No";
    case SDL_TOUCH_ZONE_OVERLAY_BORDERS:
        return "Full borders";
    case SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS:
        return "Full borders + names";
    case SDL_TOUCH_ZONE_OVERLAY_MARKERS:
    default:
        return "Small borders";
    }
}

static int touch_zone_overlay_mode_cycle(int mode, int delta)
{
    int normalized = mode;
    int next;

    if (normalized < SDL_TOUCH_ZONE_OVERLAY_OFF
        || normalized >= SDL_TOUCH_ZONE_OVERLAY_COUNT)
    {
        normalized = SDL_TOUCH_ZONE_OVERLAY_MARKERS;
    }

    next = normalized + delta;
    next = ((next % SDL_TOUCH_ZONE_OVERLAY_COUNT)
        + SDL_TOUCH_ZONE_OVERLAY_COUNT) % SDL_TOUCH_ZONE_OVERLAY_COUNT;
    return next;
}

static const char* mouse_movement_mode_label(int mode)
{
    switch (mode) {
    case SDL_MOUSE_MOVEMENT_OFF:
        return "Off";
    case SDL_MOUSE_MOVEMENT_RIGHT_ONLY:
        return "Right click only";
    case SDL_MOUSE_MOVEMENT_ON:
    default:
        return "On";
    }
}

static int mouse_movement_mode_cycle(int mode, int delta)
{
    static const int modes[] = {
        SDL_MOUSE_MOVEMENT_ON,
        SDL_MOUSE_MOVEMENT_OFF,
        SDL_MOUSE_MOVEMENT_RIGHT_ONLY,
    };
    int idx = 0;

    for (int i = 0; i < (int)N_ELEMENTS(modes); i++) {
        if (modes[i] == mode) {
            idx = i;
            break;
        }
    }

    idx = ((idx + delta) % (int)N_ELEMENTS(modes) + (int)N_ELEMENTS(modes))
        % (int)N_ELEMENTS(modes);
    return modes[idx];
}

static void touch_control_reset_to_default(void)
{
    set_sdl_touch_pane_enabled(true);
    set_sdl_touch_pane_default_open(
        get_sdl_touch_pane_default_open_default());
    set_sdl_touch_pane_key_labels_visible(
        get_sdl_touch_pane_key_labels_default_visible());
    set_sdl_touch_pane_inventory_equipment_cycle(
        get_sdl_touch_pane_inventory_equipment_default_cycle());
    set_sdl_touch_pane_placement(SDL_TOUCH_PANE_PLACEMENT_RIGHT);
    for (int i = 0; i < SDL_TOUCH_MENU_CATEGORY_COUNT; i++) {
        set_sdl_touch_menu_commands_enabled(i,
            get_sdl_touch_menu_commands_default_enabled(i));
    }
    set_sdl_touch_movement_mode(get_sdl_touch_movement_default_mode());
    set_sdl_touch_round_movement_enabled(
        get_sdl_touch_round_movement_default_enabled());
    set_sdl_touch_zone_overlay_mode(get_sdl_touch_zone_overlay_default_mode());
    for (int i = 0; i < SDL_TOUCH_ZONE_CENTER_BINDING_COUNT; i++)
        set_sdl_touch_zone_center_binding(i,
            get_sdl_touch_zone_center_default_binding(i));
    set_sdl_touch_corner_up_down_side(
        get_sdl_touch_corner_up_down_default_side());
    for (int i = 0; i < SDL_TOUCH_CORNER_ACTION_BINDING_COUNT; i++)
        set_sdl_touch_corner_action_binding(i,
            get_sdl_touch_corner_action_default_binding(i));
    set_sdl_touch_swipe_enabled(get_sdl_touch_swipe_default_enabled());
    for (int i = 0; i < TOUCH_SWIPE_DIR_COUNT; i++)
        set_sdl_touch_swipe_binding(i, get_sdl_touch_swipe_default_binding(i));
}

static void touch_pane_reset_buttons_to_default(void)
{
    for (int panel = 0; panel < SDL_TOUCH_PANE_PANEL_COUNT; panel++) {
        set_sdl_touch_pane_panel_name(panel, "");
        for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++) {
            set_sdl_touch_pane_binding_for_panel(panel, i,
                get_sdl_touch_pane_default_binding_for_panel(panel, i));
            clear_sdl_touch_pane_button_label_for_panel(panel, i);
        }
    }
}

static void touch_top_widget_reset_buttons_to_default(void)
{
    for (int i = 0; i < SDL_TOUCH_TOP_PANEL_BUTTON_COUNT; i++) {
        set_sdl_touch_top_panel_binding(i, false,
            get_sdl_touch_top_panel_default_binding(i, false));
        set_sdl_touch_top_panel_binding(i, true,
            get_sdl_touch_top_panel_default_binding(i, true));
    }
    set_sdl_touch_top_panel_cell_count(
        get_sdl_touch_top_panel_default_cell_count());
}

static void touch_corner_action_buttons_reset_to_default(void)
{
    for (int i = 0; i < SDL_TOUCH_CORNER_ACTION_BINDING_COUNT; i++) {
        set_sdl_touch_corner_action_binding(i,
            get_sdl_touch_corner_action_default_binding(i));
    }
}

static void touch_buttons_reset_to_default(void)
{
    touch_pane_reset_buttons_to_default();
    touch_top_widget_reset_buttons_to_default();
    touch_thumb_reset_buttons_to_default();
    touch_corner_action_buttons_reset_to_default();
}

enum {
    TOUCH_PANE_SETTING_COUNT = 0
};

static bool touch_pane_row_is_button(int row)
{
    return row >= TOUCH_PANE_SETTING_COUNT
        && row < TOUCH_PANE_SETTING_COUNT + SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT;
}

static int touch_pane_row_button_index(int row)
{
    if (!touch_pane_row_is_button(row))
        return -1;

    return touch_pane_visible_button_index(row - TOUCH_PANE_SETTING_COUNT);
}

static const char* touch_pane_placement_label(int placement)
{
    return (placement == SDL_TOUCH_PANE_PLACEMENT_LEFT) ? "Left" : "Right";
}

static const char* touch_corner_up_down_side_label(int side)
{
    return (side == SDL_TOUCH_CORNER_UP_DOWN_LEFT) ? "Left" : "Right";
}

static void do_cmd_touch_pane_button_editor(bool* settings_changed)
{
    int highlight = 0;
    int panel = SDL_TOUCH_PANE_PANEL_MAIN;
    bool done = false;
    bool changed = false;

    screen_save();

    while (!done)
    {
        int row_width;
        int total_rows = TOUCH_PANE_SETTING_COUNT
            + SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT;

        row_width = settings_ui_line_width(2);

        if (highlight < 0)
            highlight = 0;
        if (highlight >= total_rows)
            highlight = total_rows - 1;

        settings_semantic_menu_begin("Touch Pane Buttons", highlight);

        for (int i = 0; i < total_rows; i++)
        {
            char action_buf[80];
            char label_buf[SDL_TOUCH_PANE_LABEL_LEN];
            char left_buf[64];
            char line_buf[128];
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            int button_index = touch_pane_row_button_index(i);

            if (button_index < 0)
                continue;

            get_sdl_touch_pane_button_label_for_panel(panel, button_index,
                label_buf, sizeof(label_buf));
            touch_pane_action_label_for_panel(panel,
                get_sdl_touch_pane_binding_for_panel(panel, button_index),
                action_buf, sizeof(action_buf));

            if (label_buf[0])
                strnfmt(left_buf, sizeof(left_buf), "%s %s",
                    get_sdl_touch_pane_slot_name(button_index), label_buf);
            else
                strnfmt(left_buf, sizeof(left_buf), "%s",
                    get_sdl_touch_pane_slot_name(button_index));

            settings_ui_format_pair_line(line_buf, sizeof(line_buf), left_buf,
                action_buf, row_width, 14);
            {
                char semantic_line[160];

                settings_semantic_line_from_menu_line(semantic_line,
                    sizeof(semantic_line), line_buf);
                settings_semantic_add_row(i, semantic_line, a);
                sdl_character_sheet_screen_set_last_select_row_reset(
                    SETTINGS_CLICK_RESET_ROW_BASE + i);
            }
        }

        {
            char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
            char info_buf[96];

            get_sdl_touch_pane_panel_name(panel, panel_name, sizeof(panel_name));
            strnfmt(info_buf, sizeof(info_buf), "Editing %s panel%s",
                panel_name, (panel == SDL_TOUCH_PANE_PANEL_SECOND) ? " (empty = main panel)" : "");
            char desc[512];

            settings_semantic_add_pair_row(SETTINGS_CLICK_SWITCH_GROUP,
                "Switch Button Panel", "Tab", TERM_SLATE);
            settings_semantic_add_pair_row(SETTINGS_CLICK_RENAME_SELECTED,
                "Rename Button Label", "L", TERM_SLATE);
            settings_semantic_add_pair_row(SETTINGS_CLICK_RENAME_GROUP,
                "Rename Panel", "P", TERM_SLATE);
            settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_SELECTED,
                "Reset Selected", "X", TERM_SLATE);
            settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_ALL,
                "Reset Panel Buttons", "M", TERM_SLATE);
            strnfmt(desc, sizeof(desc),
                "%s. Space chooses an action. Left/Right nudges the selected action. Main panel must keep Confirm on at least one button.",
                info_buf);
            sdl_character_sheet_screen_set_select_description(desc);
            sdl_character_sheet_screen_commit_select(highlight);
        }

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_SWITCH_GROUP)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = '\t';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RESET_SELECTED)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'x';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RESET_ALL)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'M';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RENAME_SELECTED)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'l';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RENAME_GROUP)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'p';
                    click_generated = true;
                }
                else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice
                        < SETTINGS_CLICK_RESET_ROW_BASE + total_rows)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    highlight = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    ch = 'x';
                    click_generated = true;
                }
                else if (clicked_choice >= 0 && clicked_choice < total_rows)
                {
                    bool was_current = (clicked_choice == highlight);

                    highlight = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    if (click_action == UI_MENU_CLICK_SECONDARY)
                        ch = '4';
                    else if (was_current)
                        ch = ' ';
                    else
                        continue;
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, '\t', '\t', click_generated);
        }

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case '-':
        case '8':
            highlight = (total_rows + highlight - 1) % total_rows;
            break;

        case '2':
            highlight = (highlight + 1) % total_rows;
            break;

        case 'n':
        case '4':
        {
            int button_index = touch_pane_row_button_index(highlight);
            int choice_count = 0;
            const int* choices;
            int idx;

            if (button_index < 0)
                break;

            choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            idx = touch_pane_action_choice_index(panel,
                get_sdl_touch_pane_binding_for_panel(panel, button_index));

            idx = (choice_count + idx - 1) % choice_count;

            if (!touch_pane_main_confirm_change_allowed(panel, button_index, choices[idx])) {
                bell("Bind Confirm (pick) to another main-panel button first.");
                break;
            }

            set_sdl_touch_pane_binding_for_panel(panel, button_index, choices[idx]);
            changed = true;
            break;
        }

        case 'y':
        case '6':
        {
            int button_index = touch_pane_row_button_index(highlight);
            int choice_count = 0;
            const int* choices;
            int idx;

            if (button_index < 0)
                break;

            choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            idx = touch_pane_action_choice_index(panel,
                get_sdl_touch_pane_binding_for_panel(panel, button_index));

            idx = (idx + 1) % choice_count;

            if (!touch_pane_main_confirm_change_allowed(panel, button_index, choices[idx])) {
                bell("Bind Confirm (pick) to another main-panel button first.");
                break;
            }

            set_sdl_touch_pane_binding_for_panel(panel, button_index, choices[idx]);
            changed = true;
            break;
        }

        case ' ':
        case 't':
        case '5':
        {
            int button_index = touch_pane_row_button_index(highlight);
            int choice_count = 0;
            const int* choices;
            int picked;
            int current;
            char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
            char title[96];

            if (button_index < 0)
                break;

            choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            current = get_sdl_touch_pane_binding_for_panel(panel, button_index);
            get_sdl_touch_pane_panel_name(panel, panel_name,
                sizeof(panel_name));
            strnfmt(title, sizeof(title), "%s %s", panel_name,
                get_sdl_touch_pane_slot_name(button_index));

            if (!touch_pick_action_value(title, panel, false, choices,
                    choice_count, current, &picked)
                || picked == current)
            {
                break;
            }

            if (!touch_pane_main_confirm_change_allowed(panel, button_index,
                    picked))
            {
                bell("Bind Confirm (pick) to another main-panel button first.");
                break;
            }

            set_sdl_touch_pane_binding_for_panel(panel, button_index, picked);
            changed = true;
            break;
        }

        case 'l':
        case 'L':
        {
            char prompt[96];
            char prompt_long[96];
            char prompt_medium[96];
            char prompt_short[64];
            char current_label[SDL_TOUCH_PANE_LABEL_LEN];
            char new_label[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];
            int button_index;

            button_index = touch_pane_row_button_index(highlight);
            if (button_index < 0) {
                bell("Select a touch panel button to rename.");
                break;
            }

            get_sdl_touch_pane_button_label_for_panel(panel, button_index, current_label, sizeof(current_label));
            strnfmt(prompt_long, sizeof(prompt_long),
                "New label for %s (blank = use key label): ",
                get_sdl_touch_pane_slot_name(button_index));
            strnfmt(prompt_medium, sizeof(prompt_medium),
                "New label for %s (blank = default): ",
                get_sdl_touch_pane_slot_name(button_index));
            strnfmt(prompt_short, sizeof(prompt_short), "Label for %s: ",
                get_sdl_touch_pane_slot_name(button_index));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(settings_ui_line_width(0),
                    prompt_long, prompt_medium, prompt_short));
            strnfmt(current_buf, sizeof(current_buf), "Current label: %s", current_label);
            settings_ui_put_fitted(4, 2, TERM_SLATE, current_buf);
            new_label[0] = '\0';
            settings_semantic_menu_hide();
            if (term_get_string(prompt, new_label, sizeof(new_label)))
            {
                set_sdl_touch_pane_button_label_for_panel(panel, button_index, new_label);
                changed = true;
            }
            break;
        }

        case '\t':
            panel = (panel == SDL_TOUCH_PANE_PANEL_MAIN)
                ? SDL_TOUCH_PANE_PANEL_SECOND
                : SDL_TOUCH_PANE_PANEL_MAIN;
            break;

        case 'p':
        case 'P':
        case 'h':
        case 'H':
        {
            char prompt[96];
            char current_name[SDL_TOUCH_PANE_LABEL_LEN];
            char new_name[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];

            get_sdl_touch_pane_panel_name(panel, current_name, sizeof(current_name));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(settings_ui_line_width(0),
                    "Name for current panel (blank = default): ",
                    "Panel name (blank = default): ",
                    "Panel name: "));
            strnfmt(current_buf, sizeof(current_buf), "Current panel name: %s", current_name);
            settings_ui_put_fitted(4, 2, TERM_SLATE, current_buf);
            new_name[0] = '\0';
            settings_semantic_menu_hide();
            if (term_get_string(prompt, new_name, sizeof(new_name)))
            {
                set_sdl_touch_pane_panel_name(panel, new_name);
                changed = true;
            }
            break;
        }

        case 'r':
        case 'x':
        case 'X':
            {
                int button_index = touch_pane_row_button_index(highlight);

                if (button_index < 0)
                    break;

                if (!touch_pane_main_confirm_change_allowed(panel, button_index,
                        get_sdl_touch_pane_default_binding_for_panel(panel, button_index))) {
                    bell("Bind Confirm (pick) to another main-panel button first.");
                    break;
                }

                set_sdl_touch_pane_binding_for_panel(panel, button_index,
                    get_sdl_touch_pane_default_binding_for_panel(panel, button_index));
                clear_sdl_touch_pane_button_label_for_panel(panel, button_index);
            }
            changed = true;
            break;

        case 'R':
        case 'M':
            touch_pane_reset_buttons_to_default();
            changed = true;
            break;

        default:
            bell("Illegal command for touch panel settings!");
            break;
        }
    }

    if (changed)
    {
        if (settings_changed)
            *settings_changed = true;
    }

    settings_semantic_menu_hide();
    screen_load();
}

static void do_cmd_touch_top_widget_button_editor(bool* settings_changed)
{
    int highlight = 0;
    bool done = false;
    bool changed = false;

    screen_save();

    while (!done)
    {
        int row_width = settings_ui_line_width(2);
        int total_rows = get_sdl_touch_top_panel_cell_count();

        if (total_rows > 0 && highlight >= total_rows)
            highlight = total_rows - 1;
        if (highlight < 0)
            highlight = 0;

        settings_semantic_menu_begin("Quick Access Buttons", highlight);

        for (int i = 0; i < total_rows; i++)
        {
            char action_buf[80];
            char button_buf[32];
            char line_buf[128];
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;
            cptr label = button_buf;
            int slot = i;
            const touch_control_binding_row* binding =
                touch_top_widget_binding_for_row(
                    touch_top_widget_editor_binding_row(slot));

            if (!binding)
                continue;
            touch_top_widget_binding_label(
                touch_top_widget_editor_binding_row(slot),
                action_buf, sizeof(action_buf));
            strnfmt(button_buf, sizeof(button_buf), "Quick Access %d",
                slot + 1);
            settings_ui_format_pair_line(line_buf, sizeof(line_buf),
                label, action_buf, row_width, 24);
            {
                char semantic_line[160];

                settings_semantic_line_from_menu_line(semantic_line,
                    sizeof(semantic_line), line_buf);
                settings_semantic_add_row(i, semantic_line, a);
                sdl_character_sheet_screen_set_last_select_row_reset(
                    SETTINGS_CLICK_RESET_ROW_BASE + slot);
            }
        }

        settings_semantic_add_pair_row(SETTINGS_CLICK_QUICK_ACCESS_ADD_CELL,
            "Add Cell", total_rows >= SDL_TOUCH_TOP_PANEL_CELL_COUNT_MAX
                ? "Max" : "+1", TERM_SLATE);
        settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_SELECTED,
            "Reset Selected", "X", TERM_SLATE);
        settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_ALL,
            "Reset All Quick Access Buttons", "M", TERM_SLATE);
        sdl_character_sheet_screen_set_select_description(
            "Add Cell opens a new action picker. Choosing Unbound removes that cell. Rows balance the current cells across one or two rows. X resets selected, M resets all.");
        sdl_character_sheet_screen_commit_select(highlight);

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN) {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                } else if (clicked_choice == SETTINGS_CLICK_RESET_SELECTED) {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'x';
                    click_generated = true;
                } else if (clicked_choice == SETTINGS_CLICK_RESET_ALL) {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'M';
                    click_generated = true;
                } else if (clicked_choice
                    == SETTINGS_CLICK_QUICK_ACCESS_ADD_CELL)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'a';
                    click_generated = true;
                } else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice < SETTINGS_CLICK_RESET_ROW_BASE
                        + total_rows)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    highlight = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    ch = 'x';
                    click_generated = true;
                } else if (clicked_choice >= 0
                    && clicked_choice < total_rows)
                {
                    bool was_current = (clicked_choice == highlight);

                    highlight = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    if (click_action == UI_MENU_CLICK_SECONDARY)
                        ch = '4';
                    else if (was_current)
                    {
                        ch = ' ';
                    }
                    else
                    {
                        continue;
                    }
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, 0, 0, click_generated);
        }

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;
        case '-':
        case '8':
            if (total_rows > 0)
                highlight = (total_rows + highlight - 1) % total_rows;
            break;
        case '2':
            if (total_rows > 0)
                highlight = (highlight + 1) % total_rows;
            break;
        case 'n':
        case '4':
            if (highlight < total_rows)
                changed |= touch_top_widget_cycle_binding_row(
                    touch_top_widget_editor_binding_row(highlight), -1);
            break;
        case 'y':
        case '6':
            if (highlight < total_rows)
                changed |= touch_top_widget_cycle_binding_row(
                    touch_top_widget_editor_binding_row(highlight), 1);
            break;
        case ' ':
        case 't':
        case '5':
            if (highlight < total_rows)
                changed |= touch_top_widget_pick_binding_row(
                    touch_top_widget_editor_binding_row(highlight));
            break;
        case 'a':
        case 'A':
            if (total_rows >= SDL_TOUCH_TOP_PANEL_CELL_COUNT_MAX)
                bell("Quick access is already at the maximum number of cells.");
            else
                changed |= do_cmd_touch_top_widget_pick_button(total_rows);
            break;
        case 'r':
        case 'x':
        case 'X':
            if (highlight < total_rows)
                changed |= touch_top_widget_reset_binding_row(
                    touch_top_widget_editor_binding_row(highlight));
            break;
        case 'R':
        case 'M':
            touch_top_widget_reset_buttons_to_default();
            changed = true;
            break;
        default:
            bell("Illegal command for quick access button settings!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    settings_semantic_menu_hide();
    screen_load();
}

static void do_cmd_touch_thumb_button_editor(bool* settings_changed)
{
    enum { THUMB_ENABLE_ROW = 0 };
    const int total_rows = TOUCH_THUMB_BUTTON_ROW_COUNT + 1;
    int highlight = 0;
    bool done = false;
    bool changed = false;

    screen_save();

    while (!done)
    {
        int row_width = settings_ui_line_width(2);

        settings_semantic_menu_begin("Thumb Buttons", highlight);

        for (int i = 0; i < total_rows; i++)
        {
            char action_buf[80];
            char line_buf[128];
            const char* label;
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            if (i == THUMB_ENABLE_ROW) {
                label = "Thumb Buttons";
                SDL_strlcpy(action_buf,
                    get_sdl_touch_thumb_enabled() ? "On" : "Off",
                    sizeof(action_buf));
            } else {
                const touch_control_binding_row* binding =
                    touch_thumb_binding_for_row(i - 1);

                if (!binding)
                    continue;
                label = binding->label;
                touch_thumb_binding_label(i - 1, action_buf, sizeof(action_buf));
            }

            settings_ui_format_pair_line(line_buf, sizeof(line_buf),
                label, action_buf, row_width, 24);
            {
                char semantic_line[160];

                settings_semantic_line_from_menu_line(semantic_line,
                    sizeof(semantic_line), line_buf);
                settings_semantic_add_row(i, semantic_line, a);
                sdl_character_sheet_screen_set_last_select_row_reset(
                    SETTINGS_CLICK_RESET_ROW_BASE + i);
            }
        }

        settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_SELECTED,
            "Reset Selected", "X", TERM_SLATE);
        settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_ALL,
            "Reset All Thumb Buttons", "M", TERM_SLATE);
        sdl_character_sheet_screen_set_select_description(
            "Space toggles On/Off or chooses an action. Left/Right nudges the selected row. X resets selected, M resets all. Escape or Enter returns.");
        sdl_character_sheet_screen_commit_select(highlight);

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN) {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                } else if (clicked_choice == SETTINGS_CLICK_RESET_SELECTED) {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'x';
                    click_generated = true;
                } else if (clicked_choice == SETTINGS_CLICK_RESET_ALL) {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'M';
                    click_generated = true;
                } else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice < SETTINGS_CLICK_RESET_ROW_BASE
                        + total_rows)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    highlight = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    ch = 'x';
                    click_generated = true;
                } else if (clicked_choice >= 0
                    && clicked_choice < total_rows)
                {
                    bool was_current = (clicked_choice == highlight);

                    highlight = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    if (click_action == UI_MENU_CLICK_SECONDARY)
                        ch = '4';
                    else if (was_current)
                        ch = ' ';
                    else
                        continue;
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, 0, 0, click_generated);
        }

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;
        case '-':
        case '8':
            highlight = (total_rows + highlight - 1) % total_rows;
            break;
        case '2':
            highlight = (highlight + 1) % total_rows;
            break;
        case 'n':
        case '4':
            if (highlight == THUMB_ENABLE_ROW) {
                set_sdl_touch_thumb_enabled(!get_sdl_touch_thumb_enabled());
                changed = true;
            } else {
                changed |= touch_thumb_cycle_binding_row(highlight - 1, -1);
            }
            break;
        case 'y':
        case '6':
            if (highlight == THUMB_ENABLE_ROW) {
                set_sdl_touch_thumb_enabled(!get_sdl_touch_thumb_enabled());
                changed = true;
            } else {
                changed |= touch_thumb_cycle_binding_row(highlight - 1, 1);
            }
            break;
        case ' ':
        case 't':
        case '5':
            if (highlight == THUMB_ENABLE_ROW) {
                set_sdl_touch_thumb_enabled(!get_sdl_touch_thumb_enabled());
                changed = true;
            } else {
                changed |= touch_thumb_pick_binding_row(highlight - 1);
            }
            break;
        case 'r':
        case 'x':
        case 'X':
            if (highlight == THUMB_ENABLE_ROW) {
                set_sdl_touch_thumb_enabled(
                    get_sdl_touch_thumb_default_enabled());
                changed = true;
            } else {
                changed |= touch_thumb_reset_binding_row(highlight - 1);
            }
            break;
        case 'R':
        case 'M':
            touch_thumb_reset_buttons_to_default();
            changed = true;
            break;
        default:
            bell("Illegal command for thumb button settings!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    settings_semantic_menu_hide();
    screen_load();
}

static void do_cmd_touch_button_settings(bool* settings_changed)
{
    enum {
        TOUCH_BUTTON_MENU_PANE = 0,
        TOUCH_BUTTON_MENU_TOP_WIDGET,
        TOUCH_BUTTON_MENU_THUMB,
        TOUCH_BUTTON_MENU_RESET,
        TOUCH_BUTTON_MENU_RETURN,
        TOUCH_BUTTON_MENU_COUNT
    };
    int highlight = 0;
    bool done = false;
    bool changed = false;

    screen_save();

    while (!done)
    {
        int row_width = settings_ui_line_width(2);
        static cptr labels[TOUCH_BUTTON_MENU_COUNT] = {
            "Touch Pane Buttons",
            "Quick Access Buttons",
            "Thumb Buttons",
            "Restore Button Defaults",
            "Return"
        };

        settings_semantic_menu_begin("Touch Buttons Config", highlight);

        for (int i = 0; i < TOUCH_BUTTON_MENU_COUNT; i++) {
            char line_buf[128];
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            settings_ui_fit_text(line_buf, sizeof(line_buf), labels[i], row_width);
            settings_semantic_add_row(i, line_buf, a);
        }

        sdl_character_sheet_screen_set_select_description(
            "Enter or Space opens the selected row. Escape returns to Touch Settings.");
        sdl_character_sheet_screen_commit_select(highlight);

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN) {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                } else if (clicked_choice >= 0
                    && clicked_choice < TOUCH_BUTTON_MENU_COUNT)
                {
                    highlight = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = '\r';
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, '8', '2', click_generated);
        }

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
            done = true;
            break;
        case '-':
        case '8':
            highlight = (TOUCH_BUTTON_MENU_COUNT + highlight - 1)
                % TOUCH_BUTTON_MENU_COUNT;
            break;
        case '2':
            highlight = (highlight + 1) % TOUCH_BUTTON_MENU_COUNT;
            break;
        case '\n':
        case '\r':
        case ' ':
        case '6':
        case '5':
            if (highlight == TOUCH_BUTTON_MENU_PANE) {
                settings_semantic_menu_hide();
                do_cmd_touch_pane_button_editor(&changed);
            } else if (highlight == TOUCH_BUTTON_MENU_TOP_WIDGET) {
                settings_semantic_menu_hide();
                do_cmd_touch_top_widget_button_editor(&changed);
            } else if (highlight == TOUCH_BUTTON_MENU_THUMB) {
                settings_semantic_menu_hide();
                do_cmd_touch_thumb_button_editor(&changed);
            } else if (highlight == TOUCH_BUTTON_MENU_RESET) {
                touch_buttons_reset_to_default();
                changed = true;
            } else {
                done = true;
            }
            break;
        default:
            bell("Illegal command for touch button settings!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    settings_semantic_menu_hide();
    screen_load();
}

static void do_cmd_touch_profile_settings(bool* settings_changed)
{
    static const int profiles[] = {
        SDL_TOUCH_PROFILE_TOUCH_PANE,
        SDL_TOUCH_PROFILE_CORNERS,
        SDL_TOUCH_PROFILE_ROUND_WHEEL,
    };
    int highlight = 0;
    bool done = false;
    bool changed = false;

    for (int i = 0; i < (int)N_ELEMENTS(profiles); i++) {
        if (profiles[i] == get_sdl_touch_profile()) {
            highlight = i;
            break;
        }
    }

    screen_save();

    while (!done)
    {
        int row_width = settings_ui_line_width(2);

        settings_semantic_menu_begin("Set Touch Profile", highlight);

        for (int i = 0; i < (int)N_ELEMENTS(profiles); i++)
        {
            char action_buf[32];
            char line_buf[128];
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            SDL_strlcpy(action_buf,
                (profiles[i] == get_sdl_touch_profile()) ? "Selected" : "",
                sizeof(action_buf));
            settings_ui_format_pair_line(line_buf, sizeof(line_buf),
                touch_profile_label(profiles[i]), action_buf, row_width, 10);
            {
                char semantic_line[160];

                settings_semantic_line_from_menu_line(semantic_line,
                    sizeof(semantic_line), line_buf);
                settings_semantic_add_row(i, semantic_line, a);
            }
        }

        sdl_character_sheet_screen_set_select_description(
            "Space or Enter applies the highlighted touch profile defaults. Escape returns.");
        sdl_character_sheet_screen_commit_select(highlight);

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN) {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                } else if (clicked_choice >= 0
                    && clicked_choice < (int)N_ELEMENTS(profiles))
                {
                    highlight = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = '\r';
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, '8', '2', click_generated);
        }

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
        case ' ':
        case '5':
        case '6':
            if (ch == ESCAPE) {
                done = true;
                break;
            }
            sdl_touch_apply_profile(profiles[highlight]);
            changed = true;
            break;
        case '-':
        case '8':
            highlight = ((int)N_ELEMENTS(profiles) + highlight - 1)
                % (int)N_ELEMENTS(profiles);
            break;
        case '2':
            highlight = (highlight + 1) % (int)N_ELEMENTS(profiles);
            break;
        default:
            bell("Illegal command for touch profile settings!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    settings_semantic_menu_hide();
    screen_load();
}

static void do_cmd_touch_settings(bool* settings_changed)
{
    enum {
        TOUCH_SETTINGS_PROFILE = 0,
        TOUCH_SETTINGS_CORNER_UP_DOWN_SIDE,
        TOUCH_SETTINGS_BUTTONS,
        TOUCH_SETTINGS_DETAILED,
        TOUCH_SETTINGS_TUTORIAL,
        TOUCH_SETTINGS_RETURN,
        TOUCH_SETTINGS_COUNT
    };
    int highlight = 0;
    bool done = false;
    bool changed = false;

    screen_save();

    while (!done)
    {
        int row_width = settings_ui_line_width(2);

        settings_semantic_menu_begin("Touch Settings", highlight);

        for (int i = 0; i < TOUCH_SETTINGS_COUNT; i++)
        {
            char action_buf[80];
            char line_buf[128];
            cptr label = "";
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            action_buf[0] = '\0';
            if (i == TOUCH_SETTINGS_PROFILE) {
                label = "Set Profile";
                SDL_strlcpy(action_buf,
                    touch_profile_label(get_sdl_touch_profile()),
                    sizeof(action_buf));
            } else if (i == TOUCH_SETTINGS_CORNER_UP_DOWN_SIDE) {
                label = "Corner Up/Down Side";
                SDL_strlcpy(action_buf,
                    touch_corner_up_down_side_label(
                        get_sdl_touch_corner_up_down_side()),
                    sizeof(action_buf));
            } else if (i == TOUCH_SETTINGS_BUTTONS) {
                label = "Touch buttons config";
            } else if (i == TOUCH_SETTINGS_DETAILED) {
                label = "Detailed touch controls";
            } else if (i == TOUCH_SETTINGS_TUTORIAL) {
                label = "Touch tutorial";
            } else {
                label = "Return";
            }

            settings_ui_format_pair_line(line_buf, sizeof(line_buf), label,
                action_buf, row_width, 24);
            {
                char semantic_line[160];

                settings_semantic_line_from_menu_line(semantic_line,
                    sizeof(semantic_line), line_buf);
                settings_semantic_add_row(i, semantic_line, a);
                if (i == TOUCH_SETTINGS_CORNER_UP_DOWN_SIDE)
                    sdl_character_sheet_screen_set_last_select_row_reset(
                        SETTINGS_CLICK_RESET_ROW_BASE + i);
            }
        }

        sdl_character_sheet_screen_set_select_description(
            "Enter opens or toggles the selected row. Left/Right changes values where available. Escape returns.");
        sdl_character_sheet_screen_commit_select(highlight);

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN) {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                } else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice < SETTINGS_CLICK_RESET_ROW_BASE
                        + TOUCH_SETTINGS_COUNT)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    highlight = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    if (highlight == TOUCH_SETTINGS_CORNER_UP_DOWN_SIDE) {
                        set_sdl_touch_corner_up_down_side(
                            get_sdl_touch_corner_up_down_default_side());
                        changed = true;
                    }
                    continue;
                } else if (clicked_choice >= 0
                    && clicked_choice < TOUCH_SETTINGS_COUNT)
                {
                    highlight = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = '\r';
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, '8', '2', click_generated);
        }

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
            done = true;
            break;
        case '-':
        case '8':
            highlight = (TOUCH_SETTINGS_COUNT + highlight - 1)
                % TOUCH_SETTINGS_COUNT;
            break;
        case '2':
            highlight = (highlight + 1) % TOUCH_SETTINGS_COUNT;
            break;
        case 'n':
        case '4':
            if (highlight == TOUCH_SETTINGS_CORNER_UP_DOWN_SIDE) {
                set_sdl_touch_corner_up_down_side(
                    SDL_TOUCH_CORNER_UP_DOWN_LEFT);
                changed = true;
            } else {
                bell("Illegal command for touch settings!");
            }
            break;
        case '\n':
        case '\r':
        case ' ':
        case '5':
        case '6':
            if (highlight == TOUCH_SETTINGS_PROFILE) {
                settings_semantic_menu_hide();
                do_cmd_touch_profile_settings(&changed);
            } else if (highlight == TOUCH_SETTINGS_CORNER_UP_DOWN_SIDE) {
                int side = get_sdl_touch_corner_up_down_side();
                set_sdl_touch_corner_up_down_side(
                    side == SDL_TOUCH_CORNER_UP_DOWN_LEFT
                        ? SDL_TOUCH_CORNER_UP_DOWN_RIGHT
                        : SDL_TOUCH_CORNER_UP_DOWN_LEFT);
                changed = true;
            } else if (highlight == TOUCH_SETTINGS_BUTTONS) {
                settings_semantic_menu_hide();
                do_cmd_touch_button_settings(&changed);
            } else if (highlight == TOUCH_SETTINGS_DETAILED) {
                settings_semantic_menu_hide();
                do_cmd_touch_control_settings(&changed);
            } else if (highlight == TOUCH_SETTINGS_TUTORIAL) {
                settings_semantic_menu_hide();
                sdl_touch_request_tutorial_from_settings();
                done = true;
            } else {
                done = true;
            }
            break;
        default:
            bell("Illegal command for touch settings!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    settings_semantic_menu_hide();
    screen_load();
}

static void do_cmd_touch_control_settings(bool* settings_changed)
{
    int highlight = 0;
    bool done = false;
    bool changed = false;

    screen_save();

    while (!done)
    {
        int row_width;
        int total_rows = TOUCH_CONTROL_COUNT;

        row_width = settings_ui_line_width(2);

        if (highlight < 0)
            highlight = 0;
        if (highlight >= total_rows)
            highlight = total_rows - 1;

        settings_semantic_menu_begin("Detailed Touch Controls", highlight);

        for (int i = 0; i < total_rows; i++)
        {
            char action_buf[80];
            char line_buf[128];
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            if (i == TOUCH_CONTROL_PANE_ENABLED) {
                SDL_strlcpy(action_buf,
                    get_sdl_touch_pane_enabled() ? "On" : "Off",
                    sizeof(action_buf));
            } else if (i == TOUCH_CONTROL_PANE_DEFAULT_OPEN) {
                SDL_strlcpy(action_buf,
                    get_sdl_touch_pane_default_open() ? "Open" : "Hidden",
                    sizeof(action_buf));
            } else if (i == TOUCH_CONTROL_PANE_KEY_LABELS) {
                SDL_strlcpy(action_buf,
                    get_sdl_touch_pane_key_labels_visible() ? "Shown" : "Hidden",
                    sizeof(action_buf));
            } else if (i == TOUCH_CONTROL_PANE_INVENTORY_EQUIPMENT_CYCLE) {
                SDL_strlcpy(action_buf,
                    get_sdl_touch_pane_inventory_equipment_cycle() ? "On" : "Off",
                    sizeof(action_buf));
            } else if (i == TOUCH_CONTROL_PANE_PLACEMENT) {
                SDL_strlcpy(action_buf,
                    touch_pane_placement_label(get_sdl_touch_pane_placement()),
                    sizeof(action_buf));
            } else if (touch_control_is_menu_command_row(i)) {
                SDL_strlcpy(action_buf,
                    get_sdl_touch_menu_commands_enabled(
                        touch_control_menu_category_for_row(i)) ? "On" : "Off",
                    sizeof(action_buf));
            } else if (i == TOUCH_CONTROL_MOVEMENT) {
                SDL_strlcpy(action_buf,
                    touch_movement_mode_label(get_sdl_touch_movement_mode()),
                    sizeof(action_buf));
            } else if (i == TOUCH_CONTROL_ROUND_MOVEMENT_LAYER) {
                SDL_strlcpy(action_buf,
                    get_sdl_touch_round_movement_enabled() ? "On" : "Off",
                    sizeof(action_buf));
            } else if (i == TOUCH_CONTROL_CORNER_BUTTON_OVERLAY) {
                SDL_strlcpy(action_buf,
                    touch_zone_overlay_mode_label(
                        get_sdl_touch_zone_overlay_mode()),
                    sizeof(action_buf));
            } else if (i == TOUCH_CONTROL_CORNER_UP_DOWN_SIDE) {
                SDL_strlcpy(action_buf,
                    touch_corner_up_down_side_label(
                        get_sdl_touch_corner_up_down_side()),
                    sizeof(action_buf));
            } else if (i == TOUCH_CONTROL_SWIPE_ENABLED) {
                SDL_strlcpy(action_buf, get_sdl_touch_swipe_enabled() ? "On" : "Off",
                    sizeof(action_buf));
            } else if (!touch_control_binding_label(i, action_buf,
                    sizeof(action_buf))) {
                action_buf[0] = '\0';
            }

            settings_ui_format_pair_line(line_buf, sizeof(line_buf),
                touch_control_row_name(i), action_buf, row_width, 24);
            {
                char semantic_line[160];

                settings_semantic_line_from_menu_line(semantic_line,
                    sizeof(semantic_line), line_buf);
                settings_semantic_add_row(i, semantic_line, a);
                sdl_character_sheet_screen_set_last_select_row_reset(
                    SETTINGS_CLICK_RESET_ROW_BASE + i);
            }
        }

        settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_SELECTED,
            "Reset Selected", "X", TERM_SLATE);
        settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_ALL,
            "Reset All Touch Controls", "M", TERM_SLATE);
        sdl_character_sheet_screen_set_select_description(
            "Controls touch menus, game-screen layers, movement, overlays, and swipes. Space chooses binding rows; Left/Right nudges values; X resets selected, M resets all.");
        sdl_character_sheet_screen_commit_select(highlight);

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RESET_SELECTED)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'x';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RESET_ALL)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'M';
                    click_generated = true;
                }
                else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice
                        < SETTINGS_CLICK_RESET_ROW_BASE + total_rows)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    highlight = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    ch = 'x';
                    click_generated = true;
                }
                else if (clicked_choice >= 0 && clicked_choice < total_rows)
                {
                    bool was_current = (clicked_choice == highlight);

                    highlight = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    if (click_action == UI_MENU_CLICK_SECONDARY)
                        ch = '4';
                    else if (was_current)
                        ch = ' ';
                    else
                        continue;
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, 0, 0, click_generated);
        }

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case '-':
        case '8':
            highlight = (total_rows + highlight - 1) % total_rows;
            break;

        case '2':
            highlight = (highlight + 1) % total_rows;
            break;

        case 'n':
        case '4':
            if (highlight == TOUCH_CONTROL_PANE_ENABLED) {
                set_sdl_touch_pane_enabled(false);
                sdl_apply_config();
            } else if (highlight == TOUCH_CONTROL_PANE_DEFAULT_OPEN) {
                set_sdl_touch_pane_default_open(false);
            } else if (highlight == TOUCH_CONTROL_PANE_KEY_LABELS) {
                set_sdl_touch_pane_key_labels_visible(false);
            } else if (highlight == TOUCH_CONTROL_PANE_INVENTORY_EQUIPMENT_CYCLE) {
                set_sdl_touch_pane_inventory_equipment_cycle(false);
            } else if (highlight == TOUCH_CONTROL_PANE_PLACEMENT) {
                set_sdl_touch_pane_placement(SDL_TOUCH_PANE_PLACEMENT_LEFT);
                sdl_apply_config();
            } else if (touch_control_is_menu_command_row(highlight)) {
                set_sdl_touch_menu_commands_enabled(
                    touch_control_menu_category_for_row(highlight), false);
            } else if (highlight == TOUCH_CONTROL_MOVEMENT) {
                set_sdl_touch_movement_mode(
                    touch_movement_mode_cycle(get_sdl_touch_movement_mode(), -1));
            } else if (highlight == TOUCH_CONTROL_ROUND_MOVEMENT_LAYER) {
                set_sdl_touch_round_movement_enabled(false);
            } else if (highlight == TOUCH_CONTROL_CORNER_BUTTON_OVERLAY) {
                set_sdl_touch_zone_overlay_mode(
                    touch_zone_overlay_mode_cycle(
                        get_sdl_touch_zone_overlay_mode(), -1));
            } else if (highlight == TOUCH_CONTROL_CORNER_UP_DOWN_SIDE) {
                set_sdl_touch_corner_up_down_side(SDL_TOUCH_CORNER_UP_DOWN_LEFT);
            } else if (highlight == TOUCH_CONTROL_SWIPE_ENABLED) {
                set_sdl_touch_swipe_enabled(false);
            } else {
                touch_control_cycle_binding_row(highlight, -1);
            }
            changed = true;
            break;

        case 'y':
        case '6':
        case ' ':
        case 't':
        case '5':
            if ((ch == ' ' || ch == 't' || ch == '5')
                && touch_control_binding_for_row(highlight))
            {
                changed |= touch_control_pick_binding(
                    touch_control_binding_for_row(highlight),
                    touch_control_row_name(highlight));
                break;
            }

            if (highlight == TOUCH_CONTROL_PANE_ENABLED) {
                set_sdl_touch_pane_enabled(!get_sdl_touch_pane_enabled());
                sdl_apply_config();
            } else if (highlight == TOUCH_CONTROL_PANE_DEFAULT_OPEN) {
                set_sdl_touch_pane_default_open(
                    !get_sdl_touch_pane_default_open());
            } else if (highlight == TOUCH_CONTROL_PANE_KEY_LABELS) {
                set_sdl_touch_pane_key_labels_visible(
                    !get_sdl_touch_pane_key_labels_visible());
            } else if (highlight == TOUCH_CONTROL_PANE_INVENTORY_EQUIPMENT_CYCLE) {
                set_sdl_touch_pane_inventory_equipment_cycle(
                    !get_sdl_touch_pane_inventory_equipment_cycle());
            } else if (highlight == TOUCH_CONTROL_PANE_PLACEMENT) {
                int placement = get_sdl_touch_pane_placement();
                set_sdl_touch_pane_placement(
                    placement == SDL_TOUCH_PANE_PLACEMENT_LEFT
                        ? SDL_TOUCH_PANE_PLACEMENT_RIGHT
                        : SDL_TOUCH_PANE_PLACEMENT_LEFT);
                sdl_apply_config();
            } else if (touch_control_is_menu_command_row(highlight)) {
                int category = touch_control_menu_category_for_row(highlight);

                set_sdl_touch_menu_commands_enabled(category,
                    !get_sdl_touch_menu_commands_enabled(category));
            } else if (highlight == TOUCH_CONTROL_MOVEMENT) {
                set_sdl_touch_movement_mode(
                    touch_movement_mode_cycle(get_sdl_touch_movement_mode(), 1));
            } else if (highlight == TOUCH_CONTROL_ROUND_MOVEMENT_LAYER) {
                set_sdl_touch_round_movement_enabled(
                    !get_sdl_touch_round_movement_enabled());
            } else if (highlight == TOUCH_CONTROL_CORNER_BUTTON_OVERLAY) {
                set_sdl_touch_zone_overlay_mode(
                    touch_zone_overlay_mode_cycle(
                        get_sdl_touch_zone_overlay_mode(), 1));
            } else if (highlight == TOUCH_CONTROL_CORNER_UP_DOWN_SIDE) {
                int side = get_sdl_touch_corner_up_down_side();
                set_sdl_touch_corner_up_down_side(
                    side == SDL_TOUCH_CORNER_UP_DOWN_LEFT
                        ? SDL_TOUCH_CORNER_UP_DOWN_RIGHT
                        : SDL_TOUCH_CORNER_UP_DOWN_LEFT);
            } else if (highlight == TOUCH_CONTROL_SWIPE_ENABLED) {
                set_sdl_touch_swipe_enabled(!get_sdl_touch_swipe_enabled());
            } else {
                touch_control_cycle_binding_row(highlight, 1);
            }
            changed = true;
            break;

        case 'r':
        case 'x':
        case 'X':
            if (highlight == TOUCH_CONTROL_PANE_ENABLED) {
                set_sdl_touch_pane_enabled(true);
                sdl_apply_config();
            } else if (highlight == TOUCH_CONTROL_PANE_DEFAULT_OPEN) {
                set_sdl_touch_pane_default_open(
                    get_sdl_touch_pane_default_open_default());
            } else if (highlight == TOUCH_CONTROL_PANE_KEY_LABELS) {
                set_sdl_touch_pane_key_labels_visible(
                    get_sdl_touch_pane_key_labels_default_visible());
            } else if (highlight == TOUCH_CONTROL_PANE_INVENTORY_EQUIPMENT_CYCLE) {
                set_sdl_touch_pane_inventory_equipment_cycle(
                    get_sdl_touch_pane_inventory_equipment_default_cycle());
            } else if (highlight == TOUCH_CONTROL_PANE_PLACEMENT) {
                set_sdl_touch_pane_placement(SDL_TOUCH_PANE_PLACEMENT_RIGHT);
                sdl_apply_config();
            } else if (touch_control_is_menu_command_row(highlight)) {
                int category = touch_control_menu_category_for_row(highlight);

                set_sdl_touch_menu_commands_enabled(
                    category, get_sdl_touch_menu_commands_default_enabled(category));
            } else if (highlight == TOUCH_CONTROL_MOVEMENT) {
                set_sdl_touch_movement_mode(get_sdl_touch_movement_default_mode());
            } else if (highlight == TOUCH_CONTROL_ROUND_MOVEMENT_LAYER) {
                set_sdl_touch_round_movement_enabled(
                    get_sdl_touch_round_movement_default_enabled());
            } else if (highlight == TOUCH_CONTROL_CORNER_BUTTON_OVERLAY) {
                set_sdl_touch_zone_overlay_mode(
                    get_sdl_touch_zone_overlay_default_mode());
            } else if (highlight == TOUCH_CONTROL_CORNER_UP_DOWN_SIDE) {
                set_sdl_touch_corner_up_down_side(
                    get_sdl_touch_corner_up_down_default_side());
            } else if (highlight == TOUCH_CONTROL_SWIPE_ENABLED) {
                set_sdl_touch_swipe_enabled(get_sdl_touch_swipe_default_enabled());
            } else {
                touch_control_reset_binding_row(highlight);
            }
            changed = true;
            break;

        case 'R':
        case 'M':
            touch_control_reset_to_default();
            sdl_apply_config();
            changed = true;
            break;

        default:
            bell("Illegal command for touch control settings!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    settings_semantic_menu_hide();
    screen_load();
}

static void do_cmd_mouse_settings(bool* settings_changed)
{
    enum {
        MOUSE_SETTING_ENABLE = 0,
        MOUSE_SETTING_MOVEMENT,
        MOUSE_SETTING_TILE_POINTER,
        MOUSE_SETTING_TUTORIAL,
        MOUSE_SETTING_COUNT
    };
    int highlight = 0;
    bool done = false;
    bool changed = false;

    screen_save();

    while (!done)
    {
        int row_width = settings_ui_line_width(2);

        settings_semantic_menu_begin("Mouse Input", highlight);

        for (int i = 0; i < MOUSE_SETTING_COUNT; i++)
        {
            char action_buf[80];
            char line_buf[128];
            cptr label;
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            action_buf[0] = '\0';
            if (i == MOUSE_SETTING_ENABLE) {
                label = "Enable Mouse";
                SDL_strlcpy(action_buf,
                    get_sdl_mouse_enabled() ? "On" : "Off",
                    sizeof(action_buf));
            } else if (i == MOUSE_SETTING_MOVEMENT) {
                label = "Mouse Movement";
                SDL_strlcpy(action_buf,
                    mouse_movement_mode_label(get_sdl_mouse_movement_mode()),
                    sizeof(action_buf));
            } else if (i == MOUSE_SETTING_TILE_POINTER) {
                label = "Tile Pointer";
                SDL_strlcpy(action_buf,
                    get_sdl_mouse_tile_pointer() ? "On" : "Off",
                    sizeof(action_buf));
            } else {
                label = "Mouse tutorial";
            }

            settings_ui_format_pair_line(line_buf, sizeof(line_buf),
                label, action_buf, row_width, 18);
            {
                char semantic_line[160];

                settings_semantic_line_from_menu_line(semantic_line,
                    sizeof(semantic_line), line_buf);
                settings_semantic_add_row(i, semantic_line, a);
                if (i != MOUSE_SETTING_TUTORIAL)
                    sdl_character_sheet_screen_set_last_select_row_reset(
                        SETTINGS_CLICK_RESET_ROW_BASE + i);
            }
        }

        {
            settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_SELECTED,
                "Reset Selected", "X", TERM_SLATE);
            settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_ALL,
                "Reset All", "M", TERM_SLATE);
            static const char* const mouse_setting_desc[MOUSE_SETTING_COUNT] = {
                [MOUSE_SETTING_ENABLE] = "Turn mouse input on or off.",
                [MOUSE_SETTING_MOVEMENT] =
                    "How clicking the map moves you: walk to the clicked spot, "
                    "single step toward it, or disabled.",
                [MOUSE_SETTING_TILE_POINTER] =
                    "Use tile row 12, column 31 as the mouse pointer.",
                [MOUSE_SETTING_TUTORIAL] =
                    "Replay the mouse-controls tutorial.",
            };
            cptr md = (highlight >= 0 && highlight < MOUSE_SETTING_COUNT)
                ? mouse_setting_desc[highlight] : NULL;

            sdl_character_sheet_screen_set_select_description(md ? md : "");
            sdl_character_sheet_screen_commit_select(highlight);
        }

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RESET_SELECTED)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'x';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RESET_ALL)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'M';
                    click_generated = true;
                }
                else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice
                        < SETTINGS_CLICK_RESET_ROW_BASE + MOUSE_SETTING_COUNT)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    highlight = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    ch = 'x';
                    click_generated = true;
                }
                else if (clicked_choice >= 0 && clicked_choice < MOUSE_SETTING_COUNT)
                {
                    bool was_current = (clicked_choice == highlight);

                    highlight = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    if (click_action == UI_MENU_CLICK_SECONDARY)
                        ch = '4';
                    else if (was_current)
                        ch = ' ';
                    else
                        continue;
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, 0, 0, click_generated);
        }

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case '-':
        case '8':
            highlight = (MOUSE_SETTING_COUNT + highlight - 1)
                % MOUSE_SETTING_COUNT;
            break;

        case '2':
            highlight = (highlight + 1) % MOUSE_SETTING_COUNT;
            break;

        case 'n':
        case '4':
            if (highlight == MOUSE_SETTING_ENABLE) {
                set_sdl_mouse_enabled(false);
                changed = true;
            } else if (highlight == MOUSE_SETTING_MOVEMENT) {
                set_sdl_mouse_movement_mode(
                    mouse_movement_mode_cycle(get_sdl_mouse_movement_mode(), -1));
                changed = true;
            } else if (highlight == MOUSE_SETTING_TILE_POINTER) {
                set_sdl_mouse_tile_pointer(false);
                changed = true;
            }
            break;

        case 'y':
        case '6':
        case ' ':
        case 't':
        case '5':
            if (highlight == MOUSE_SETTING_ENABLE) {
                set_sdl_mouse_enabled(!get_sdl_mouse_enabled());
                changed = true;
            } else if (highlight == MOUSE_SETTING_MOVEMENT) {
                set_sdl_mouse_movement_mode(
                    mouse_movement_mode_cycle(get_sdl_mouse_movement_mode(), 1));
                changed = true;
            } else if (highlight == MOUSE_SETTING_TILE_POINTER) {
                set_sdl_mouse_tile_pointer(!get_sdl_mouse_tile_pointer());
                changed = true;
            } else if (highlight == MOUSE_SETTING_TUTORIAL) {
                settings_semantic_menu_hide();
                sdl_mouse_request_tutorial_from_settings();
                done = true;
            }
            break;

        case 'r':
        case 'x':
        case 'X':
            if (highlight == MOUSE_SETTING_ENABLE) {
                set_sdl_mouse_enabled(get_sdl_mouse_default_enabled());
                changed = true;
            } else if (highlight == MOUSE_SETTING_MOVEMENT) {
                set_sdl_mouse_movement_mode(get_sdl_mouse_movement_default_mode());
                changed = true;
            } else if (highlight == MOUSE_SETTING_TILE_POINTER) {
                set_sdl_mouse_tile_pointer(
                    get_sdl_mouse_default_tile_pointer());
                changed = true;
            }
            break;

        case 'R':
        case 'M':
            set_sdl_mouse_enabled(get_sdl_mouse_default_enabled());
            set_sdl_mouse_movement_mode(get_sdl_mouse_movement_default_mode());
            set_sdl_mouse_tile_pointer(get_sdl_mouse_default_tile_pointer());
            changed = true;
            break;

        default:
            bell("Illegal command for mouse input settings!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    settings_semantic_menu_hide();
    screen_load();
}


void do_cmd_controller_settings(void);

static bool other_options_choice_is_disabled(int choice)
{
    return (choice == 3);
}

static int other_options_menu(int* highlight)
{
    int ch;
    int options = 4;
    int clicked_choice = 0;
    bool death_view = death_spectator_active();

    if (*highlight < 1)
        *highlight = 1;
    else if (*highlight > options)
        *highlight = options;

    if (death_view && other_options_choice_is_disabled(*highlight))
        *highlight = options;

    settings_semantic_menu_begin("Other Options", *highlight);

#define ADD_OTHER_OPTIONS_ROW(CHOICE, LABEL, ATTR)                             \
    do {                                                                       \
        settings_semantic_add_row((CHOICE), (LABEL), (ATTR));                  \
    } while (0)

    ADD_OTHER_OPTIONS_ROW(1, "m) Choose Palette",
        (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE);
    ADD_OTHER_OPTIONS_ROW(2, "n) Write a note",
        (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE);
    ADD_OTHER_OPTIONS_ROW(3, "s) Suicide",
        death_view ? TERM_L_DARK
                   : ((*highlight == 3) ? TERM_L_BLUE : TERM_WHITE));
    ADD_OTHER_OPTIONS_ROW(4, "o) Return to Options",
        (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE);

#undef ADD_OTHER_OPTIONS_ROW

    {
        char verbuf[128];
        strnfmt(verbuf, sizeof(verbuf), "%s %s", VERSION_NAME, VERSION_STRING);
        static const char* const other_options_row_desc[] = {
            NULL,
            "Choose the color palette used throughout the game.",
            "Write a note into your character's journal.",
            "End your current character permanently.",
            "Return to the Options menu.",
        };
        cptr d = (*highlight >= 1 && *highlight <= 4)
            ? other_options_row_desc[*highlight] : NULL;

        sdl_character_sheet_screen_set_select_description(d ? d : verbuf);
        sdl_character_sheet_screen_commit_select(*highlight);
    }

    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (click_action == UI_MENU_CLICK_HOVER && clicked_choice < 0)
                return (0);
            if (clicked_choice == -1)
                clicked_choice = 4;
            else if (clicked_choice == -2)
                clicked_choice = *highlight;
            if (clicked_choice == SETTINGS_CLICK_RETURN)
                clicked_choice = 7;
            if (clicked_choice < 1 || clicked_choice > options)
                return (0);
            *highlight = clicked_choice;
            if (click_action == UI_MENU_CLICK_HOVER)
                return (0);
            if (death_view && other_options_choice_is_disabled(*highlight))
            {
                msg_print("You can no longer take that action.");
                return (0);
            }
            return (*highlight);
        }
    }

    ch = settings_menu_key(ch, '8', '2', false);

    if ((ch == 'm') || (ch == 'M'))
    {
        *highlight = 1;
        return (1);
    }

    if ((ch == 'n') || (ch == 'N'))
    {
        *highlight = 2;
        return (2);
    }

    if ((ch == 's') || (ch == 'S'))
    {
        if (death_view)
        {
            msg_print("You can no longer take that action.");
            return (0);
        }

        *highlight = 3;
        return (3);
    }

    if ((ch == 'o') || (ch == 'O') || (ch == 'q') || (ch == 'Q')
        || (ch == ESCAPE))
    {
        *highlight = 4;
        return (4);
    }

    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (death_view && other_options_choice_is_disabled(*highlight))
        {
            msg_print("You can no longer take that action.");
            return (0);
        }

        return (*highlight);
    }

    if (ch == '8')
    {
        *highlight = (*highlight + (options - 2)) % options + 1;
        while (death_view && other_options_choice_is_disabled(*highlight))
            *highlight = (*highlight + (options - 2)) % options + 1;
    }

    if (ch == '2')
    {
        *highlight = *highlight % options + 1;
        while (death_view && other_options_choice_is_disabled(*highlight))
            *highlight = *highlight % options + 1;
    }

    return (0);
}

static void do_cmd_other_options(void)
{
    int choice = 0;
    int highlight = 1;
    bool return_to_options = false;
    Term_clear();

    while (!return_to_options)
    {
        choice = other_options_menu(&highlight);
        settings_semantic_menu_hide();

        switch (choice)
        {
        case 1:
        {
            settings_semantic_menu_hide();
            do_cmd_colors();
            Term_clear();
            break;
        }
        case 2:
        {
            settings_semantic_menu_hide();
            do_cmd_note("", p_ptr->depth);
            Term_clear();
            break;
        }
        case 3:
        {
            settings_semantic_menu_hide();
            do_cmd_suicide();
            return_to_options = true;
            Term_clear();
            break;
        }
        case 4:
        {
            settings_semantic_menu_hide();
            return_to_options = true;
            Term_clear();
            break;
        }
        }
    }
}

int options_menu(int* highlight)
{
    int ch;
    int options = 9;
    int clicked_choice = 0;
    char line_buf[80];
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    bool allow_debug_menu = false;
#ifdef SHOW_DEBUG_OPTIONS_MENU
    allow_debug_menu = true;
#endif
    if (allow_debug_menu && p_ptr->noscore)
        options++;

    if (*highlight < 1)
        *highlight = 1;
    else if (*highlight > options)
        *highlight = options;

    settings_semantic_menu_begin("Options", *highlight);

#define ADD_OPTIONS_MENU_ROW(CHOICE, KEY, TEXT)                                \
    do {                                                                       \
        keyed_menu_entry_label(line_buf, sizeof(line_buf), (KEY), (TEXT));     \
        settings_semantic_add_row((CHOICE), line_buf,                          \
            (*highlight == (CHOICE)) ? TERM_L_BLUE : TERM_WHITE);              \
    } while (0)

    ADD_OPTIONS_MENU_ROW(1, 'a', "Input Options");
    ADD_OPTIONS_MENU_ROW(2, 'b', "Pane Settings");
    ADD_OPTIONS_MENU_ROW(3, 'c', "Interface Options");
    ADD_OPTIONS_MENU_ROW(4, 'd', "Visual Options");
    ADD_OPTIONS_MENU_ROW(5, 'e', "Text Options");
    ADD_OPTIONS_MENU_ROW(6, 'f', "Gameplay Options");
    ADD_OPTIONS_MENU_ROW(7, 'g', "Sound Options");
    ADD_OPTIONS_MENU_ROW(8, 'i', "Other Options");
    ADD_OPTIONS_MENU_ROW(9, 'o', "Return to Game");

    if (allow_debug_menu && p_ptr->noscore)
        ADD_OPTIONS_MENU_ROW(10, 'p', "Debugging Options");

#undef ADD_OPTIONS_MENU_ROW

    /* Show product name and version on the bottom of the menu */
    {
        char verbuf[128];
        strnfmt(verbuf, sizeof(verbuf), "%s %s", VERSION_NAME, VERSION_STRING);
        static const char* const options_row_desc[] = {
            NULL,
            "Keyboard, controller, and mouse input settings and tutorials.",
            "Window layout: panes, terminal scale, fullscreen, tiles, and "
            "fonts.",
            "How information is presented and how menus and the HUD behave.",
            "Map and tile appearance, highlighting, and related visuals.",
            "Fonts and text rendering options.",
            "Rules and quality-of-life options that affect play.",
            "Sound effects and volume.",
            "Palette, notes, and other miscellaneous actions.",
            "Close the menu and return to play.",
            "Developer and debugging toggles.",
        };
        cptr d = (*highlight >= 1
            && *highlight < (int)N_ELEMENTS(options_row_desc))
            ? options_row_desc[*highlight] : NULL;

        sdl_character_sheet_screen_set_select_description(d ? d : verbuf);
        sdl_character_sheet_screen_commit_select(*highlight);
    }

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (click_action == UI_MENU_CLICK_HOVER && clicked_choice < 0)
                return (0);
            if (clicked_choice == -1)
                clicked_choice = 9;
            else if (clicked_choice == -2)
                clicked_choice = *highlight;
            if (clicked_choice == SETTINGS_CLICK_RETURN)
                clicked_choice = 10;
            if (clicked_choice < 1 || clicked_choice > options)
                return (0);
            *highlight = clicked_choice;
            if (click_action == UI_MENU_CLICK_HOVER)
                return (0);
            return (*highlight);
        }
    }

    ch = settings_menu_key(ch, '8', '2', false);

    if (menu_letters && ((ch == 'a') || (ch == 'A')))
    {
        *highlight = 1;
        return (1);
    }

    if (menu_letters && ((ch == 'b') || (ch == 'B')))
    {
        *highlight = 2;
        return (2);
    }

    if (menu_letters && ((ch == 'c') || (ch == 'C')))
    {
        *highlight = 3;
        return (3);
    }

    if (menu_letters && ((ch == 'd') || (ch == 'D')))
    {
        *highlight = 4;
        return (4);
    }

    if (menu_letters && ((ch == 'e') || (ch == 'E')))
    {
        *highlight = 5;
        return (5);
    }

    if (menu_letters && ((ch == 'f') || (ch == 'F')))
    {
        *highlight = 6;
        return (6);
    }

    if (menu_letters && ((ch == 'g') || (ch == 'G')))
    {
        *highlight = 7;
        return (7);
    }

    if (menu_letters && ((ch == 'i') || (ch == 'I')))
    {
        *highlight = 8;
        return (8);
    }

    if ((menu_letters && ((ch == 'o') || (ch == 'O') || (ch == 'q')))
        || (ch == ESCAPE) || (steamdeck && ch == steamdeck_back_key()))
    {
        *highlight = 9;
        return (9);
    }

    if (menu_letters && allow_debug_menu && p_ptr->noscore
        && ((ch == 'p') || (ch == 'P')))
    {
        *highlight = 10;
        return (10);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        *highlight = (*highlight + (options - 2)) % options + 1;
    }

    /* Next item */
    if (ch == '2')
    {
        *highlight = *highlight % options + 1;
    }

    return (0);
}

static void do_cmd_keyboard_input_settings(void)
{
    int selected = 1;
    bool done = false;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();

    while (!done)
    {
        int ch;
        int clicked_choice = 0;
        settings_semantic_menu_begin("Keyboard Input", selected);

#define ADD_KEYBOARD_ROW(CHOICE, LABEL, VALUE)                                \
        do {                                                                   \
            settings_semantic_add_pair_row((CHOICE), (LABEL), (VALUE),        \
                selected == (CHOICE) ? TERM_L_BLUE : TERM_WHITE);              \
        } while (0)

        ADD_KEYBOARD_ROW(1, "Movement preset",
            sdl_config_movement_preset_label(config.movement_keyboard_preset));
        ADD_KEYBOARD_ROW(2, "Angband keyset",
            op_ptr->opt[OPT_angband_keyset] ? "on" : "off");
        ADD_KEYBOARD_ROW(3, "Configure movement", "open");
        ADD_KEYBOARD_ROW(4, "Configure command keybinds", "open");
        ADD_KEYBOARD_ROW(5, "Return to Input", "");

#undef ADD_KEYBOARD_ROW

        {
            static const char* const keyboard_row_desc[] = {
                NULL,
                "Pick how you move: a built-in preset (Classic Sil, Modern "
                "Arrows, WASD Grid, or Vi Keys), shown with descriptions and a "
                "recommendation.",
                "Switch the command keys to an Angband-style layout. Changes "
                "command letters only, not how you move.",
                "Edit individual movement key bindings, or cycle the movement "
                "preset.",
                "Rebind the in-game command keys (the keymap).",
                "Return to the Input options menu.",
            };
            cptr d = (selected >= 1 && selected <= 5)
                ? keyboard_row_desc[selected] : NULL;

            sdl_character_sheet_screen_set_select_description(d ? d : "");
            sdl_character_sheet_screen_commit_select(selected);
        }

        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        {
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN)
                    clicked_choice = 5;
                if (clicked_choice >= 1 && clicked_choice <= 5)
                {
                    selected = clicked_choice;
                    if (click_action != UI_MENU_CLICK_HOVER)
                        ch = '\r';
                }
            }
        }

        ch = settings_menu_key(ch, '8', '2', false);

        if (ch == ESCAPE || (steamdeck && ch == steamdeck_back_key())
            || (menu_letters && (ch == 'o' || ch == 'O' || ch == 'q')))
        {
            done = true;
        }
        else if (ch == '8')
            selected = (selected + 3) % 5 + 1;
        else if (ch == '2')
            selected = selected % 5 + 1;
        else if (menu_letters && (ch == 'a' || ch == 'A'))
        {
            selected = 1;
            ch = '\r';
        }
        else if (menu_letters && (ch == 'b' || ch == 'B'))
        {
            selected = 2;
            ch = '\r';
        }
        else if (menu_letters && (ch == 'c' || ch == 'C'))
        {
            selected = 3;
            ch = '\r';
        }
        else if (menu_letters && (ch == 'd' || ch == 'D'))
        {
            selected = 4;
            ch = '\r';
        }

        if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '6'
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            if (selected == 1)
            {
                /* Open the full chooser so the per-preset descriptions and the
                 * general recommendation are visible while picking, instead of
                 * blind-cycling the preset. */
                settings_semantic_menu_hide();
                (void)keyboard_preset_choose_and_apply();
                Term_clear();
            }
            else if (selected == 2)
            {
                op_ptr->opt[OPT_angband_keyset] =
                    !op_ptr->opt[OPT_angband_keyset];
                save_pane_config_to_json();
            }
            else if (selected == 3)
            {
                settings_semantic_menu_hide();
                do_cmd_movement_keybinds();
                Term_clear();
            }
            else if (selected == 4)
            {
                settings_semantic_menu_hide();
                do_cmd_keybinds();
                Term_clear();
            }
            else
                done = true;
        }
    }

    settings_semantic_menu_hide();
}

enum input_option_action {
    INPUT_OPTION_NONE = 0,
    INPUT_OPTION_KEYBOARD,
    INPUT_OPTION_CONTROLLER,
    INPUT_OPTION_TOUCH_TUTORIAL,
    INPUT_OPTION_MOUSE,
    INPUT_OPTION_WHEEL_TUTORIAL,
    INPUT_OPTION_ZONES_TUTORIAL,
    INPUT_OPTION_RETURN,
};

struct input_option_row {
    enum input_option_action action;
    char key;
    cptr label;
    cptr description;
};

static int input_option_rows_collect(struct input_option_row* rows,
    int max_rows)
{
    int count = 0;

#define ADD_INPUT_OPTION(ACTION, KEY, LABEL, DESCRIPTION)                     \
    do {                                                                       \
        if (count < max_rows) {                                                \
            rows[count].action = (ACTION);                                    \
            rows[count].key = (KEY);                                          \
            rows[count].label = (LABEL);                                      \
            rows[count].description = (DESCRIPTION);                          \
            count++;                                                          \
        }                                                                      \
    } while (0)

    if (SDL_HasKeyboard()) {
        ADD_INPUT_OPTION(INPUT_OPTION_KEYBOARD, 'a', "Keyboard Input",
            "Keyboard movement presets, the Angband keyset, and key "
            "rebinding.");
    }
    if (SDL_HasGamepad()) {
        ADD_INPUT_OPTION(INPUT_OPTION_CONTROLLER, 'b', "Controller Settings",
            "Set up the connected game controller or Steam Deck: button "
            "mapping, sticks, and deadzones.");
    }
    if (sdl_touch_tutorial_device_available()) {
        ADD_INPUT_OPTION(INPUT_OPTION_TOUCH_TUTORIAL, 'c', "Touch Tutorial",
            "Replay the touch-controls tutorial.");
    }
    if (SDL_HasMouse()) {
        ADD_INPUT_OPTION(INPUT_OPTION_MOUSE, 'd', "Mouse Input",
            "Mouse behavior, including click-to-move and pointer options.");
    }
    ADD_INPUT_OPTION(INPUT_OPTION_WHEEL_TUTORIAL, 'e',
        "Character Wheel Tutorial",
        "Replay the player action-wheel tutorial.");
    ADD_INPUT_OPTION(INPUT_OPTION_ZONES_TUTORIAL, 'f', "Zones Tutorial",
        "Replay the screen-zones tutorial.");
    ADD_INPUT_OPTION(INPUT_OPTION_RETURN, 'o', "Return to Options",
        "Return to the Options menu.");

#undef ADD_INPUT_OPTION

    return count;
}

static enum input_option_action input_options_menu(int* highlight)
{
    struct input_option_row rows[7];
    int options = input_option_rows_collect(rows, (int)N_ELEMENTS(rows));
    int ch;
    int clicked_choice = 0;
    char line_buf[80];
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();

    if (*highlight < 1)
        *highlight = 1;
    else if (*highlight > options)
        *highlight = options;

    settings_semantic_menu_begin("Input Options", *highlight);

    for (int i = 0; i < options; i++) {
        int choice = i + 1;

        keyed_menu_entry_label(line_buf, sizeof(line_buf), rows[i].key,
            rows[i].label);
        settings_semantic_add_row(choice, line_buf,
            (*highlight == choice) ? TERM_L_BLUE : TERM_WHITE);
    }

    sdl_character_sheet_screen_set_select_description(
        rows[*highlight - 1].description);
    sdl_character_sheet_screen_commit_select(*highlight);

    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (click_action == UI_MENU_CLICK_HOVER && clicked_choice < 0)
                return INPUT_OPTION_NONE;
            if (clicked_choice == -1
                || clicked_choice == SETTINGS_CLICK_RETURN)
            {
                clicked_choice = options;
            }
            else if (clicked_choice == -2)
                clicked_choice = *highlight;
            if (clicked_choice < 1 || clicked_choice > options)
                return INPUT_OPTION_NONE;
            *highlight = clicked_choice;
            if (click_action == UI_MENU_CLICK_HOVER)
                return INPUT_OPTION_NONE;
            return rows[*highlight - 1].action;
        }
    }

    ch = settings_menu_key(ch, '8', '2', false);

    if (menu_letters) {
        for (int i = 0; i < options; i++) {
            if (tolower((unsigned char)ch)
                == tolower((unsigned char)rows[i].key))
            {
                *highlight = i + 1;
                return rows[i].action;
            }
        }
    }

    if ((menu_letters && (ch == 'q' || ch == 'Q'))
        || ch == ESCAPE || (steamdeck && ch == steamdeck_back_key()))
    {
        *highlight = options;
        return INPUT_OPTION_RETURN;
    }

    if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '6'
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        return rows[*highlight - 1].action;
    }

    if (ch == '8')
        *highlight = (*highlight + (options - 2)) % options + 1;

    if (ch == '2')
        *highlight = *highlight % options + 1;

    return INPUT_OPTION_NONE;
}

static void do_cmd_input_options_submenu(int* highlight)
{
    enum input_option_action choice = INPUT_OPTION_NONE;
    bool return_to_options = false;

    Term_clear();

    while (!return_to_options)
    {
        choice = input_options_menu(highlight);
        settings_semantic_menu_hide();

        if (choice == INPUT_OPTION_WHEEL_TUTORIAL) {
            sdl_character_wheel_request_tutorial_from_settings();
            return_to_options = true;
            Term_clear();
            continue;
        }
        if (choice == INPUT_OPTION_ZONES_TUTORIAL) {
            sdl_zones_request_tutorial_from_settings();
            return_to_options = true;
            Term_clear();
            continue;
        }
        if (choice == INPUT_OPTION_RETURN) {
            return_to_options = true;
            Term_clear();
            continue;
        }

        switch (choice)
        {
        case INPUT_OPTION_KEYBOARD:
            settings_semantic_menu_hide();
            do_cmd_keyboard_input_settings();
            Term_clear();
            break;
        case INPUT_OPTION_CONTROLLER:
            settings_semantic_menu_hide();
            do_cmd_controller_settings();
            Term_clear();
            break;
        case INPUT_OPTION_TOUCH_TUTORIAL:
            settings_semantic_menu_hide();
            sdl_touch_request_tutorial_from_settings();
            return_to_options = true;
            Term_clear();
            break;
        case INPUT_OPTION_MOUSE:
            settings_semantic_menu_hide();
            do_cmd_mouse_settings(NULL);
            if (sdl_mouse_settings_tutorial_requested())
                return_to_options = true;
            Term_clear();
            break;
        default:
            break;
        }
    }
}

static bool options_debug_page_available(void)
{
#ifdef SHOW_DEBUG_OPTIONS_MENU
    return p_ptr && p_ptr->noscore;
#else
    return false;
#endif
}

static int options_menu_page_choice_turn(int choice, int direction)
{
    int page_choices[] = { 3, 4, 5, 6, 7, 10 };
    int count = options_debug_page_available()
        ? (int)N_ELEMENTS(page_choices)
        : (int)N_ELEMENTS(page_choices) - 1;

    for (int i = 0; i < count; i++)
    {
        if (page_choices[i] == choice)
        {
            int next = (direction < 0) ? i + count - 1 : i + 1;
            return page_choices[next % count];
        }
    }

    return choice;
}

static void options_queue_page_turn(int choice, int* queued_choice)
{
    if (!queued_choice || !settings_option_page_turn)
        return;

    *queued_choice = options_menu_page_choice_turn(choice,
        settings_option_page_turn);
    settings_option_page_turn = 0;
}

/*
 * Set or unset various options.
 *
 * After using this command, a complete redraw should be performed,
 * in case any visual options have been changed.
 */
void do_cmd_options(void)
{
    int choice = 0;
    int highlight = 1;
    int input_highlight = 1;
    int queued_choice = 0;

    bool return_to_game = false;

    /* Clear any active banner before opening options */
    if (dismiss_active_narrative_banner()) {
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    sdl_push_terminal_menu_scale();
    if (p_ptr && p_ptr->playing)
        sdl_music_play_menu_theme();

    /* Clear screen */
    Term_clear();

    /* Process Events until "Return to Game" is selected */
    while (!return_to_game)
    {
        if (queued_choice)
        {
            choice = queued_choice;
            queued_choice = 0;
            highlight = choice;
        }
        else
        {
            choice = options_menu(&highlight);
        }
        settings_semantic_menu_hide();

        switch (choice)
        {
        case 1:
        {
            do_cmd_input_options_submenu(&input_highlight);
            if (sdl_touch_settings_tutorial_requested()
                || sdl_mouse_settings_tutorial_requested()
                || sdl_character_wheel_settings_tutorial_requested()
                || sdl_zones_settings_tutorial_requested())
                return_to_game = true;
            Term_clear();
            break;
        }
        case 2:
        {
            settings_semantic_menu_hide();
            do_cmd_pane_settings();
            Term_clear();
            break;
        }
        case 3:
        {
            do_cmd_options_aux(INTERFACE_PAGE, "Interface Options");
            options_queue_page_turn(choice, &queued_choice);
            Term_clear();
            break;
        }
        case 4:
        {
            do_cmd_options_aux(VISUAL_PAGE, "Visual Options");
            options_queue_page_turn(choice, &queued_choice);
            Term_clear();
            break;
        }
        case 5:
        {
            do_cmd_options_aux(TEXT_PAGE, "Text Options");
            options_queue_page_turn(choice, &queued_choice);
            Term_clear();
            break;
        }
        case 6:
        {
            do_cmd_options_aux(GAMEPLAY_PAGE, "Gameplay Options");
            options_queue_page_turn(choice, &queued_choice);
            Term_clear();
            break;
        }
        case 7:
        {
            do_cmd_options_aux(SOUND_PAGE, "Sound Options");
            options_queue_page_turn(choice, &queued_choice);
            Term_clear();
            break;
        }
        case 8:
        {
            do_cmd_other_options();
            if (p_ptr && (p_ptr->leaving || !p_ptr->playing))
                return_to_game = true;
            Term_clear();
            break;
        }
        case 9:
        {
            /* Return to Game */
            settings_semantic_menu_hide();
            return_to_game = true;
            Term_clear();
            break;
        }
        case 10:
        {
            /* Debugging Options (only reachable when p_ptr->noscore) */
            do_cmd_options_aux(DEBUG_PAGE, "Debugging Options");
            options_queue_page_turn(choice, &queued_choice);
            Term_clear();
            break;
        }
        }
    }

    /* Flush messages */
    message_flush();

    /* Load screen */
    sdl_character_sheet_screen_hide();
    sdl_pop_terminal_menu_scale();
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    screen_load();
    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
    if (p_ptr)
        handle_stuff();
    if (sdl_touch_settings_tutorial_requested()
        || sdl_mouse_settings_tutorial_requested()
        || sdl_character_wheel_settings_tutorial_requested()
        || sdl_zones_settings_tutorial_requested())
    {
        if (p_ptr && p_ptr->playing)
            do_cmd_redraw();
        else
            Term_fresh();
    }

    /* Replay the zones tutorial now, from this clean post-menu context, rather
     * than the deferred Term_xtra path which does not reliably paint the
     * full-screen tutorial. */
    sdl_zones_show_requested_tutorial();
}

/*
 * Helper to turn a single keycode into printable text for the keybind UI.
 */
static void describe_keycode(byte keycode, char* buf, size_t buflen)
{
    char raw[2];

    if (!buf || !buflen)
        return;

    raw[0] = (char)keycode;
    raw[1] = '\0';

    ascii_to_text(buf, buflen, raw);
}

struct keybind_entry
{
    byte key_code;
    cptr extra_default_keys;
    cptr key_name;
    cptr action;
    bool requires_keymap;
};

static bool key_matches_default(const struct keybind_entry* entry, byte key)
{
    if (key == entry->key_code)
        return true;
    if (entry->extra_default_keys && strchr(entry->extra_default_keys, key))
        return true;
    return false;
}

static bool key_provides_action(int mode, byte key, cptr action, bool requires_keymap)
{
    cptr mapping = keymap_act[mode][key];

    if (requires_keymap)
        return (mapping && streq(mapping, action));

    if (!mapping)
        return true;

    return streq(mapping, action);
}

static bool entry_has_binding(int mode, const struct keybind_entry* entry)
{
    int key;

    if (key_provides_action(mode, entry->key_code, entry->action, entry->requires_keymap))
        return true;

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
                return true;
            extra++;
        }
    }

    for (key = 0; key < 256; key++)
    {
        cptr current = keymap_act[mode][key];

        if (!current || !streq(current, entry->action))
            continue;

        if (key_matches_default(entry, (byte)key))
            continue;

        return true;
    }

    return false;
}

/*
 * Build a comma-separated list of keys that trigger the supplied action.
 */
static void describe_action_bindings(int mode, const struct keybind_entry* entry, char* buf,
    size_t buflen)
{
    int key;
    bool found = false;
    size_t current_len = 0;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!entry->action)
    {
        SDL_strlcpy(buf, "(none)", buflen);
        return;
    }

    if (key_provides_action(mode, entry->key_code, entry->action, entry->requires_keymap))
    {
        char key_label[16];
        describe_keycode(entry->key_code, key_label, sizeof(key_label));
        SDL_strlcpy(buf, key_label, buflen);
        current_len = strlen(buf);
        found = true;
    }

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
            {
                char key_label[16];
                describe_keycode((byte)*extra, key_label, sizeof(key_label));
                if (found)
                    strnfcat(buf, buflen, &current_len, ", %s", key_label);
                else
                {
                    SDL_strlcpy(buf, key_label, buflen);
                    current_len = strlen(buf);
                    found = true;
                }
            }
            extra++;
        }
    }

    for (key = 0; key < 256; key++)
    {
        cptr current = keymap_act[mode][key];

        if (!current || !streq(current, entry->action))
            continue;

        if (key_matches_default(entry, (byte)key))
            continue;

        {
            char key_label[16];
            describe_keycode((byte)key, key_label, sizeof(key_label));
            if (found)
                strnfcat(buf, buflen, &current_len, ", %s", key_label);
            else
            {
                SDL_strlcpy(buf, key_label, buflen);
                current_len = strlen(buf);
                found = true;
            }
        }
    }

    if (!found)
        SDL_strlcpy(buf, "(none)", buflen);
}

/*
 * Remove all key bindings that trigger the specified action.
 */
static void unbind_action(int mode, cptr action)
{
    int key;

    if (!action)
        return;

    for (key = 0; key < 256; key++)
    {
        if (keymap_act[mode][key] && streq(keymap_act[mode][key], action))
        {
            keymap_act[mode][key] = str_free(keymap_act[mode][key]);
        }
    }
}

static byte keybind_current_key(int mode, const struct keybind_entry* entry)
{
    int key;

    if (!entry)
        return 0;

    if (key_provides_action(mode, entry->key_code, entry->action,
            entry->requires_keymap))
        return entry->key_code;

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
                return (byte)*extra;
            extra++;
        }
    }

    for (key = 0; key < 256; key++)
    {
        cptr current = keymap_act[mode][key];

        if (current && streq(current, entry->action))
            return (byte)key;
    }

    return entry->key_code;
}

static bool keybind_choice_add(struct settings_value_choice* choices,
    char labels[][80], int* count, int max_count, byte key, cptr label)
{
    char key_label[32];

    if (!choices || !labels || !count || *count >= max_count)
        return false;

    if (settings_choice_value_seen(choices, *count, key))
        return false;

    describe_keycode(key, key_label, sizeof(key_label));
    if (label && label[0])
        strnfmt(labels[*count], 80, "%s - %s", key_label, label);
    else
        SDL_strlcpy(labels[*count], key_label, 80);

    choices[*count].value = key;
    choices[*count].label = labels[*count];
    (*count)++;
    return true;
}

static void keybind_choices_add_entry(struct settings_value_choice* choices,
    char labels[][80], int* count, int max_count,
    const struct keybind_entry* entry)
{
    if (!entry)
        return;

    keybind_choice_add(choices, labels, count, max_count, entry->key_code,
        entry->key_name);

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            keybind_choice_add(choices, labels, count, max_count, (byte)*extra,
                entry->key_name);
            extra++;
        }
    }
}

#define KEYBIND_PICK_OTHER (-1)

static bool keybind_pick_key(int mode, const struct keybind_entry* entry,
    const struct keybind_entry* primary_entries, int primary_count,
    const struct keybind_entry* secondary_entries, int secondary_count,
    byte* out_key, bool* out_manual)
{
    struct settings_value_choice choices[SETTINGS_VALUE_PICKER_MAX];
    char labels[SETTINGS_VALUE_PICKER_MAX][80];
    char title[96];
    int count = 0;
    int picked;
    byte current;

    if (out_manual)
        *out_manual = false;
    if (!entry || !out_key)
        return false;

    keybind_choices_add_entry(choices, labels, &count,
        SETTINGS_VALUE_PICKER_MAX, entry);

    for (int i = 0; i < primary_count; i++)
        keybind_choices_add_entry(choices, labels, &count,
            SETTINGS_VALUE_PICKER_MAX, &primary_entries[i]);
    for (int i = 0; i < secondary_count; i++)
        keybind_choices_add_entry(choices, labels, &count,
            SETTINGS_VALUE_PICKER_MAX, &secondary_entries[i]);

    if (count < SETTINGS_VALUE_PICKER_MAX)
    {
        choices[count].value = KEYBIND_PICK_OTHER;
        choices[count].label = "Press another key...";
        count++;
    }

    current = keybind_current_key(mode, entry);
    strnfmt(title, sizeof(title), "Bind %s", entry->key_name);
    if (!settings_pick_value(title, NULL, choices, count, current, &picked))
        return false;

    if (picked == KEYBIND_PICK_OTHER)
    {
        if (out_manual)
            *out_manual = true;
        return true;
    }

    *out_key = (byte)picked;
    return true;
}

static bool list_missing_primary_bindings(int mode, const struct keybind_entry* entries,
    int count, char* buffer, size_t buflen)
{
    int i;
    bool ok = true;
    size_t cur = 0;

    if (!buffer || !buflen)
        return true;

    buffer[0] = '\0';

    for (i = 0; i < count; i++)
    {
        if (entry_has_binding(mode, &entries[i]))
            continue;

        if (!ok)
            strnfcat(buffer, buflen, &cur, ", ");
        strnfcat(buffer, buflen, &cur, "%s", entries[i].key_name);
        ok = false;
    }

    return ok;
}

typedef struct movement_setting_entry {
    u16b action;
    u16b direction;
    cptr label;
    bool essential;
} movement_setting_entry;

static const movement_setting_entry movement_settings[] = {
    { MOVEMENT_INPUT_ACTION_MOVE_DIR, MOVEMENT_INPUT_DIRECTION_NORTHWEST, "Move NW", true },
    { MOVEMENT_INPUT_ACTION_MOVE_DIR, MOVEMENT_INPUT_DIRECTION_NORTH, "Move N", true },
    { MOVEMENT_INPUT_ACTION_MOVE_DIR, MOVEMENT_INPUT_DIRECTION_NORTHEAST, "Move NE", true },
    { MOVEMENT_INPUT_ACTION_MOVE_DIR, MOVEMENT_INPUT_DIRECTION_WEST, "Move W", true },
    { MOVEMENT_INPUT_ACTION_MOVE_DIR, MOVEMENT_INPUT_DIRECTION_EAST, "Move E", true },
    { MOVEMENT_INPUT_ACTION_MOVE_DIR, MOVEMENT_INPUT_DIRECTION_SOUTHWEST, "Move SW", true },
    { MOVEMENT_INPUT_ACTION_MOVE_DIR, MOVEMENT_INPUT_DIRECTION_SOUTH, "Move S", true },
    { MOVEMENT_INPUT_ACTION_MOVE_DIR, MOVEMENT_INPUT_DIRECTION_SOUTHEAST, "Move SE", true },
    { MOVEMENT_INPUT_ACTION_WAIT, MOVEMENT_INPUT_DIRECTION_NONE, "Wait", true },
    { MOVEMENT_INPUT_ACTION_RUN_DIR, MOVEMENT_INPUT_DIRECTION_NORTHWEST, "Run NW", true },
    { MOVEMENT_INPUT_ACTION_RUN_DIR, MOVEMENT_INPUT_DIRECTION_NORTH, "Run N", true },
    { MOVEMENT_INPUT_ACTION_RUN_DIR, MOVEMENT_INPUT_DIRECTION_NORTHEAST, "Run NE", true },
    { MOVEMENT_INPUT_ACTION_RUN_DIR, MOVEMENT_INPUT_DIRECTION_WEST, "Run W", true },
    { MOVEMENT_INPUT_ACTION_RUN_DIR, MOVEMENT_INPUT_DIRECTION_EAST, "Run E", true },
    { MOVEMENT_INPUT_ACTION_RUN_DIR, MOVEMENT_INPUT_DIRECTION_SOUTHWEST, "Run SW", true },
    { MOVEMENT_INPUT_ACTION_RUN_DIR, MOVEMENT_INPUT_DIRECTION_SOUTH, "Run S", true },
    { MOVEMENT_INPUT_ACTION_RUN_DIR, MOVEMENT_INPUT_DIRECTION_SOUTHEAST, "Run SE", true },
    { MOVEMENT_INPUT_ACTION_INTERACT_DIR, MOVEMENT_INPUT_DIRECTION_NORTHWEST, "Interact NW", true },
    { MOVEMENT_INPUT_ACTION_INTERACT_DIR, MOVEMENT_INPUT_DIRECTION_NORTH, "Interact N", true },
    { MOVEMENT_INPUT_ACTION_INTERACT_DIR, MOVEMENT_INPUT_DIRECTION_NORTHEAST, "Interact NE", true },
    { MOVEMENT_INPUT_ACTION_INTERACT_DIR, MOVEMENT_INPUT_DIRECTION_WEST, "Interact W", true },
    { MOVEMENT_INPUT_ACTION_INTERACT_DIR, MOVEMENT_INPUT_DIRECTION_EAST, "Interact E", true },
    { MOVEMENT_INPUT_ACTION_INTERACT_DIR, MOVEMENT_INPUT_DIRECTION_SOUTHWEST, "Interact SW", true },
    { MOVEMENT_INPUT_ACTION_INTERACT_DIR, MOVEMENT_INPUT_DIRECTION_SOUTH, "Interact S", true },
    { MOVEMENT_INPUT_ACTION_INTERACT_DIR, MOVEMENT_INPUT_DIRECTION_SOUTHEAST, "Interact SE", true },
    { MOVEMENT_INPUT_ACTION_REST, MOVEMENT_INPUT_DIRECTION_NONE, "Rest", false },
};

static bool movement_entry_matches_binding(const movement_setting_entry* entry,
    const movement_input_binding* binding)
{
    if (!entry || !binding || !movement_input_binding_is_valid(binding))
        return false;
    if (entry->action != binding->action)
        return false;
    if (movement_input_action_is_directional(entry->action))
        return entry->direction == binding->direction;

    return true;
}

static const movement_input_binding* movement_find_binding(
    const struct sdl_config* source_config, const movement_setting_entry* entry)
{
    if (!source_config || !entry)
        return NULL;

    for (u16b i = 0; i < source_config->movement_binding_count; i++)
    {
        const movement_input_binding* binding =
            &source_config->movement_bindings[i];

        if (movement_entry_matches_binding(entry, binding))
            return binding;
    }

    return NULL;
}

/*
 * True when the active bindings map a bare letter key (no modifier) to a
 * movement action. Such presets (WASD Grid, Vi) replace the matching letter
 * commands while in the dungeon, so the menu warns about it.
 */
static bool movement_bindings_shadow_letters(const struct sdl_config* cfg)
{
    if (!cfg)
        return false;

    for (u16b i = 0; i < cfg->movement_binding_count; i++)
    {
        const movement_input_binding* binding = &cfg->movement_bindings[i];
        SDL_Scancode scancode;

        if (!movement_input_binding_is_valid(binding))
            continue;
        if (binding->required_modifiers)
            continue;

        scancode = (SDL_Scancode)binding->trigger;
        if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
            return true;
    }

    return false;
}

static void movement_binding_key_label(SDL_Scancode scancode, char* buf,
    size_t buflen)
{
    const char* name;

    if (!buf || !buflen)
        return;

    name = SDL_GetScancodeName(scancode);
    if (!name || !name[0])
        strnfmt(buf, buflen, "Scancode %d", (int)scancode);
    else if (prefix(name, "Keypad "))
        strnfmt(buf, buflen, "Numpad %s", name + strlen("Keypad "));
    else if (streq(name, "Page Up"))
        SDL_strlcpy(buf, "PageUp", buflen);
    else if (streq(name, "Page Down"))
        SDL_strlcpy(buf, "PageDown", buflen);
    else if (streq(name, "Return"))
        SDL_strlcpy(buf, "Enter", buflen);
    else if (streq(name, "Escape"))
        SDL_strlcpy(buf, "Esc", buflen);
    else
        SDL_strlcpy(buf, name, buflen);
}

static void movement_binding_label(const movement_input_binding* binding,
    char* buf, size_t buflen)
{
    size_t cursor = 0;
    char key_buf[40];

    if (!buf || !buflen)
        return;

    if (!binding || !movement_input_binding_is_valid(binding))
    {
        SDL_strlcpy(buf, "(unbound)", buflen);
        return;
    }

    buf[0] = '\0';
    if (binding->required_modifiers & MOVEMENT_INPUT_MODIFIER_CTRL)
        strnfcat(buf, buflen, &cursor, "Ctrl+");
    if (binding->required_modifiers & MOVEMENT_INPUT_MODIFIER_SHIFT)
        strnfcat(buf, buflen, &cursor, "Shift+");
    if (binding->required_modifiers & MOVEMENT_INPUT_MODIFIER_ALT)
        strnfcat(buf, buflen, &cursor, "Alt+");
    if (binding->required_modifiers & MOVEMENT_INPUT_MODIFIER_META)
        strnfcat(buf, buflen, &cursor, "Meta+");

    movement_binding_key_label((SDL_Scancode)binding->trigger, key_buf,
        sizeof(key_buf));
    strnfcat(buf, buflen, &cursor, "%s", key_buf);
}

static bool movement_list_missing_essentials(char* buffer, size_t buflen)
{
    bool ok = true;
    size_t cursor = 0;

    if (!buffer || !buflen)
        return true;

    buffer[0] = '\0';
    for (int i = 0; i < (int)N_ELEMENTS(movement_settings); i++)
    {
        if (!movement_settings[i].essential)
            continue;
        if (movement_find_binding(&config, &movement_settings[i]))
            continue;

        if (!ok)
            strnfcat(buffer, buflen, &cursor, ", ");
        strnfcat(buffer, buflen, &cursor, "%s", movement_settings[i].label);
        ok = false;
    }

    return ok;
}

static void movement_remove_conflicts(const movement_input_binding* candidate)
{
    u16b i = 0;

    if (!candidate || !movement_input_binding_is_valid(candidate))
        return;

    while (i < config.movement_binding_count)
    {
        if (movement_input_bindings_conflict(candidate,
                &config.movement_bindings[i]))
        {
            memmove(&config.movement_bindings[i], &config.movement_bindings[i + 1],
                (config.movement_binding_count - i - 1)
                    * sizeof(config.movement_bindings[0]));
            config.movement_binding_count--;
            continue;
        }

        i++;
    }
}

static bool movement_default_binding_for_entry(
    const movement_setting_entry* entry, movement_input_binding* out_binding)
{
    struct sdl_config defaults;
    u16b preset = config.movement_keyboard_preset;
    const movement_input_binding* binding;

    if (!entry || !out_binding)
        return false;

    if (preset == SDL_MOVEMENT_PRESET_NONE)
        preset = SDL_MOVEMENT_PRESET_CLASSIC_SIL;

    memset(&defaults, 0, sizeof(defaults));
    sdl_config_set_default_movement_bindings(&defaults, preset);
    binding = movement_find_binding(&defaults, entry);
    if (!binding)
        return false;

    *out_binding = *binding;
    return true;
}

static bool movement_capture_binding(const movement_setting_entry* entry,
    movement_input_binding* out_binding)
{
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    u16b modifiers = 0;
    char prompt[120];

    if (!entry || !out_binding)
        return false;

    settings_semantic_menu_hide();
    Term_clear();
    strnfmt(prompt, sizeof(prompt),
        "Press key for %s (Escape cancels):", entry->label);
    settings_ui_put_fitted(2, 2, TERM_YELLOW, prompt);
    settings_ui_put_fitted(4, 2, TERM_SLATE,
        "Hold Shift/Ctrl/Alt while pressing the key to require that modifier.");
    Term_fresh();

    if (!sdl_keyboard_capture_begin())
        return false;

    /* Pump input until the capture handler reports a captured key. Escape is
     * captured like any other key and rejected below, which cancels. */
    while (true)
    {
        (void)inkey();

        if (sdl_keyboard_capture_poll(&scancode, &modifiers))
            break;
    }

    if (scancode == SDL_SCANCODE_ESCAPE || scancode == SDL_SCANCODE_UNKNOWN)
        return false;

    movement_input_binding_clear(out_binding);
    out_binding->context = MOVEMENT_INPUT_CONTEXT_ANY;
    out_binding->action = entry->action;
    out_binding->direction = entry->direction;
    out_binding->trigger = (u32b)scancode;
    out_binding->required_modifiers = modifiers;
    out_binding->forbidden_modifiers =
        (MOVEMENT_INPUT_MODIFIER_SHIFT | MOVEMENT_INPUT_MODIFIER_CTRL
            | MOVEMENT_INPUT_MODIFIER_ALT | MOVEMENT_INPUT_MODIFIER_META)
        & (u16b)~modifiers;

    return movement_input_binding_is_valid(out_binding);
}

static void movement_apply_binding(const movement_setting_entry* entry,
    const movement_input_binding* binding)
{
    if (!entry || !binding)
        return;

    movement_remove_conflicts(binding);
    (void)sdl_config_set_movement_binding(&config, entry->action,
        entry->direction, binding);
    sdl_config_apply_keyboard_keymaps(&config);
}

static void do_cmd_movement_keybinds(void)
{
    const int entry_count = (int)N_ELEMENTS(movement_settings);
    const int list_start_row = 5;
    int highlight = 0;
    int top = 0;
    bool done = false;
    bool dirty = false;
    int term_w, term_h;

    screen_save();

    while (!done)
    {
        int visible_rows;
        int row_width;
        int display_end;
        int ch;
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;
        bool click_generated = false;
        char line[160];
        char binding_buf[80];
        char preset_line[120];

        Term_get_size(&term_w, &term_h);
        visible_rows = term_h - list_start_row - 5;
        if (visible_rows < 6)
            visible_rows = 6;
        row_width = settings_ui_line_width(2);

        if (highlight < 0)
            highlight = 0;
        if (highlight >= entry_count)
            highlight = entry_count - 1;
        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;
        if (top > entry_count - visible_rows)
            top = MAX(0, entry_count - visible_rows);

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        settings_menu_begin_scroll_area(list_start_row, visible_rows);

        settings_ui_put_fitted(1, 2, TERM_WHITE,
            "Movement Bindings");
        strnfmt(preset_line, sizeof(preset_line), "Preset: %s",
            sdl_config_movement_preset_label(config.movement_keyboard_preset));
        settings_ui_put_fitted(2, 2, TERM_SLATE, preset_line);
        settings_ui_put_fitted(3, 2, TERM_WHITE,
            "Enter bind  R reset selected  P cycle preset  S save  Esc return");
        if (movement_bindings_shadow_letters(&config))
            settings_ui_put_fitted(4, 2, TERM_YELLOW,
                "Note: letter keys control movement; hold Alt for their command (Alt+w wield, Alt+x examine, Alt+Shift+s stealth).");

        display_end = top + visible_rows;
        if (display_end > entry_count)
            display_end = entry_count;

        for (int i = top; i < display_end; i++)
        {
            const movement_input_binding* binding =
                movement_find_binding(&config, &movement_settings[i]);
            int row = list_start_row + (i - top);

            movement_binding_label(binding, binding_buf, sizeof(binding_buf));
            settings_ui_format_pair_line(line, sizeof(line),
                movement_settings[i].label, binding_buf, row_width, 18);
            c_prt((i == highlight) ? TERM_L_BLUE : TERM_WHITE, line, row, 2);
            ui_menu_click_add_full_row(i, row);
        }

        if (dirty)
            c_prt(TERM_YELLOW, "Unsaved movement changes",
                term_h - 2, 2);

        Term_fresh();

        ch = inkey();
        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (clicked_choice == SETTINGS_CLICK_RETURN)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                ch = ESCAPE;
                click_generated = true;
            }
            else if (clicked_choice >= 0 && clicked_choice < entry_count)
            {
                highlight = clicked_choice;
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                ch = '\r';
                click_generated = true;
            }
        }

        ch = settings_menu_key(ch, '8', '2', click_generated);

        if (ch == ESCAPE || ch == 'q' || ch == 'Q')
        {
            char missing[256];

            if (!movement_list_missing_essentials(missing, sizeof(missing)))
            {
                char prompt[512];

                strnfmt(prompt, sizeof(prompt),
                    "Essential movement bindings are missing (%s). Exit anyway? ",
                    missing);
                if (!get_check(prompt))
                    continue;
            }
            done = true;
        }
        else if (ch == '8')
            highlight = (highlight + entry_count - 1) % entry_count;
        else if (ch == '2')
            highlight = (highlight + 1) % entry_count;
        else if (ch == 'p' || ch == 'P')
        {
            u16b preset = sdl_config_next_movement_preset(
                config.movement_keyboard_preset);

            sdl_config_set_default_movement_bindings(&config, preset);
            sdl_config_apply_keyboard_keymaps(&config);
            dirty = true;
        }
        else if (ch == 'r' || ch == 'R')
        {
            movement_input_binding binding;

            if (movement_default_binding_for_entry(&movement_settings[highlight],
                    &binding))
            {
                movement_apply_binding(&movement_settings[highlight], &binding);
                dirty = true;
            }
        }
        else if (ch == 's' || ch == 'S')
        {
            if (save_pane_config_to_json())
            {
                msg_print("Movement bindings saved.");
                dirty = false;
            }
            else
                msg_print("Failed to save movement bindings.");
            message_flush();
        }
        else if (ch == '\r' || ch == '\n' || ch == ' ')
        {
            movement_input_binding binding;

            if (movement_capture_binding(&movement_settings[highlight],
                    &binding))
            {
                movement_apply_binding(&movement_settings[highlight], &binding);
                dirty = true;
            }
        }
    }

    screen_load();

    if (dirty)
    {
        if (get_check("Save movement bindings to sil_sdl.json? "))
        {
            if (save_pane_config_to_json())
                msg_print("Movement bindings saved.");
            else
                msg_print("Failed to save movement bindings.");
            message_flush();
        }
    }
}

/*
 * Keybind configuration menu
 * Allows rebinding of movement commands for players without a numpad
 */
void do_cmd_keybinds(void)
{
    int mode;
    bool done = false;
    bool dirty = false;
    char ch;
    bool showing_primary = true;
    int highlight_primary = 0;
    int highlight_secondary = 0;
    const char* default_file = "sil_sdl.json";
    static const struct keybind_entry primary_keybinds[] = {
        {'i', NULL, "Inventory", "i", false},
        {'e', NULL, "Equipment", "e", false},
        {'u', NULL, "Use item", "u", false},
        {'x', NULL, "Examine item", "x", false},
        {'s', NULL, "Sing / change song", "s", false},
        {'S', NULL, "Toggle stealth", "S", false},
        {'h', "H@", "Character sheet (h / H / @)", "h", false},
        {'\t', NULL, "Change active weapon", "Tab", false},
        {'y', NULL, "Abilities", "y", false},
        {'f', NULL, "Fire 1st quiver", "f", false},
        {'F', NULL, "Fire 2nd quiver", "F", false},
        {'l', NULL, "Look around", "l", false},
        {'T', NULL, "Tunnel / dig", "T", false},
        {'b', NULL, "Bash door", "b", false},
    };

    static const struct keybind_entry secondary_keybinds[] = {
        {'j', NULL, "Supplies overview", "j", false},
        {'w', NULL, "Wear / wield equipment", "w", false},
        {'r', NULL, "Remove equipment", "r", false},
        {'g', NULL, "Pick up items", "g", false},
        {KTRL('F'), NULL, "Swap quivers", "\006", false},
        {'o', NULL, "Open door / chest", "o", false},
        {'c', NULL, "Close door", "c", false},
        {'D', NULL, "Disarm trap / chest", "D", false},
        {'X', NULL, "Exchange places", "X", false},
        {'-', NULL, "Fletch arrows", "-", false},
        {'{', NULL, "Inscribe item", "{", false},
        {'a', NULL, "Activate staff", "a", false},
        {KTRL('A'), NULL, "Swap staff", "\001", false},
        {'E', NULL, "Eat food", "E", false},
        {'t', NULL, "Throw item", "t", false},
        {'p', NULL, "Blow horn", "p", false},
        {'q', NULL, "Quaff potion", "q", false},
        {'M', NULL, "View map", "M", false},
        {'L', NULL, "Pan", "L", false},
        {KTRL('Q'), NULL, "Combat rolls", "\021", false},
        {'0', NULL, "Smithing screen", "0", false},
        {'<', NULL, "Go upstairs", "<", false},
        {'>', NULL, "Go downstairs", ">", false},
        {'m', NULL, "Main menu", "m", false},
        {'?', NULL, "Help", "?", false},
        {'@', NULL, "Character sheet (alternate)", "@", false},
        {'O', NULL, "Options menu", "O", false},
        {':', NULL, "Take notes", ":", false},
        {'~', NULL, "Knowledge browser", "~", false},
        {'[', NULL, "Monster list", "[", false},
        {']', NULL, "Object list", "]", false},
    };

    int primary_count = (int)N_ELEMENTS(primary_keybinds);
    int secondary_count = (int)N_ELEMENTS(secondary_keybinds);

    /* Determine the keyset mode */
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* Save screen */
    screen_save();

    while (!done)
    {
        const struct keybind_entry* keybinds;
        int num_keybinds;
        int* highlight_ptr;
        int highlight;
        int i;
        char binding_buf[80];
        char detail_binding_buf[512];
        char line_buf[128];
        int row_width;

        row_width = settings_ui_line_width(2);

        if (showing_primary)
        {
            keybinds = primary_keybinds;
            num_keybinds = primary_count;
            highlight_ptr = &highlight_primary;
        }
        else
        {
            keybinds = secondary_keybinds;
            num_keybinds = secondary_count;
            highlight_ptr = &highlight_secondary;
        }

        if (*highlight_ptr >= num_keybinds)
            *highlight_ptr = num_keybinds - 1;
        if (*highlight_ptr < 0)
            *highlight_ptr = 0;

        highlight = *highlight_ptr;

        settings_semantic_menu_begin("Keybind Configuration", highlight);

        for (i = 0; i < num_keybinds; i++)
        {
            describe_action_bindings(mode, &keybinds[i], binding_buf,
                sizeof(binding_buf));
            settings_ui_format_pair_line(line_buf, sizeof(line_buf),
                keybinds[i].key_name, binding_buf, row_width, 12);

            {
                char semantic_line[160];

                settings_semantic_line_from_menu_line(semantic_line,
                    sizeof(semantic_line), line_buf);
                settings_semantic_add_row(i, semantic_line,
                    (i == highlight) ? TERM_L_BLUE : TERM_WHITE);
                sdl_character_sheet_screen_set_last_select_row_reset(
                    SETTINGS_CLICK_RESET_ROW_BASE + i);
            }
        }

        {
            char desc[1024];

            describe_action_bindings(mode, &keybinds[highlight],
                detail_binding_buf, sizeof(detail_binding_buf));
            settings_semantic_add_pair_row(SETTINGS_CLICK_SWITCH_GROUP,
                showing_primary ? "Show Supplementary Commands"
                                : "Show Primary Commands",
                "Tab", TERM_SLATE);
            settings_semantic_add_pair_row(SETTINGS_CLICK_SAVE,
                "Save Keybinds", "S", TERM_SLATE);
            settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_SELECTED,
                "Reset Selected", "R", TERM_SLATE);
            strnfmt(desc, sizeof(desc),
                "%s. Enter or Space rebinds the selected command. Tab switches groups. S saves to %s. R resets selected.%s%s",
                detail_binding_buf, default_file,
                dirty ? " " : "",
                dirty ? "Unsaved changes." : "");
            sdl_character_sheet_screen_set_select_description(desc);
            sdl_character_sheet_screen_commit_select(highlight);
        }

        /* Get input */
        ch = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_SWITCH_GROUP)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = '\t';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_SAVE)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 's';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RESET_SELECTED)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'r';
                    click_generated = true;
                }
                else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice
                        < SETTINGS_CLICK_RESET_ROW_BASE + num_keybinds)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    highlight = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    *highlight_ptr = highlight;
                    ch = 'r';
                    click_generated = true;
                }
                else if (clicked_choice >= 0 && clicked_choice < num_keybinds)
                {
                    highlight = clicked_choice;
                    *highlight_ptr = highlight;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = '\r';
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, '\t', '\t', click_generated);
        }

        /* Handle input */
        if (ch == ESCAPE || ch == 'q' || ch == 'Q')
        {
            char missing[256];
            if (!list_missing_primary_bindings(mode, primary_keybinds, primary_count, missing,
                    sizeof(missing)))
            {
                char prompt[512];
                strnfmt(prompt, sizeof(prompt),
                    "Essential commands are unbound (%s). Exit anyway? ", missing);
                settings_semantic_menu_hide();
                if (!get_check(prompt))
                    continue;
            }
            done = true;
        }
        else if (ch == '\t')
        {
            showing_primary = !showing_primary;
            continue;
        }
        else if (ch == '8')
        {
            /* Move up */
            if (num_keybinds > 0)
            {
                highlight = (highlight + num_keybinds - 1) % num_keybinds;
                *highlight_ptr = highlight;
            }
        }
        else if (ch == '2')
        {
            /* Move down */
            if (num_keybinds > 0)
            {
                highlight = (highlight + 1) % num_keybinds;
                *highlight_ptr = highlight;
            }
        }
        else if (ch == '\r' || ch == '\n' || ch == ' ')
        {
            /* Rebind the selected key */
            cptr action = keybinds[highlight].action;
            char key_label[32];
            byte new_key = 0;
            bool manual_key = false;

            if (keybind_pick_key(mode, &keybinds[highlight],
                    primary_keybinds, primary_count,
                    secondary_keybinds, secondary_count, &new_key,
                    &manual_key))
            {
                if (manual_key)
                {
                    char prompt[80];
                    char bind_key;

                    settings_semantic_menu_hide();

                    strnfmt(prompt, sizeof(prompt),
                        "Bind %s (Esc cancels):", keybinds[highlight].key_name);
                    if (!get_com(prompt, &bind_key))
                        break;

                    new_key = (byte)bind_key;
                }

                /* Clear any existing action on the chosen key */
                keymap_act[mode][new_key] = str_free(keymap_act[mode][new_key]);
                keymap_act[mode][new_key] = str_dup(action);
                dirty = true;

                describe_keycode(new_key, key_label, sizeof(key_label));
                msg_format("Key %s now performs %s", key_label, keybinds[highlight].key_name);
                message_flush();
            }
        }
        else if (ch == 'r' || ch == 'R')
        {
            /* Reset to default */
            byte target_key = keybinds[highlight].key_code;
            char key_label[32];
            cptr action = keybinds[highlight].action;

            /* Remove the action from any custom keys */
            unbind_action(mode, action);

            /* Restore default action */
            keymap_act[mode][target_key] = str_free(keymap_act[mode][target_key]);
            if (keybinds[highlight].requires_keymap)
                keymap_act[mode][target_key] = str_dup(action);

            dirty = true;

            describe_keycode(target_key, key_label, sizeof(key_label));
            msg_format("Reset %s to default key %s", keybinds[highlight].key_name, key_label);
            message_flush();
        }
        else if (ch == 's' || ch == 'S')
        {
            settings_semantic_menu_hide();
            if (save_pane_config_to_json())
            {
                msg_format("Keybinds saved to %s.", default_file);
                dirty = false;
            }
            else
            {
                msg_print("Failed to save keybinds.");
            }
            message_flush();
        }

        /* Store updated highlight for the active group */
        *highlight_ptr = highlight;
    }

    /* Load screen */
    settings_semantic_menu_hide();
    screen_load();

    if (dirty)
    {
        char prompt[80];
        strnfmt(prompt, sizeof(prompt), "Save keybinds to %s? ", default_file);
        if (get_check(prompt))
        {
            if (save_pane_config_to_json())
            {
                msg_format("Keybinds saved to %s.", default_file);
                message_flush();
            }
            else
            {
                msg_print("Failed to save keybinds.");
                message_flush();
            }
        }
    }
}

typedef enum controller_entry_type {
    CONTROLLER_ENTRY_TOGGLE = 0,
    CONTROLLER_ENTRY_ACTION,
} controller_entry_type;

typedef enum controller_toggle_id {
    CONTROLLER_TOGGLE_ENABLED = 0,
    CONTROLLER_TOGGLE_AUTO_MODE,
    CONTROLLER_TOGGLE_STEAMDECK_MODE,
    CONTROLLER_TOGGLE_STEAMDECK_INV_EQUIP_SAME_BUTTON_CYCLE,
    CONTROLLER_TOGGLE_DPAD,
    CONTROLLER_TOGGLE_LEFT_STICK,
} controller_toggle_id;

typedef struct controller_entry {
    controller_entry_type type;
    int id;
    const char* label;
} controller_entry;

typedef struct controller_physical_binding_ref {
    int type;
    int id;
} controller_physical_binding_ref;

static bool controller_action_binding_equals(int lhs, int rhs);
static int controller_action_binding_count(int binding, int* out_type,
    int* out_id);
static int controller_combo_action_binding_count(int binding,
    int* out_modifier_type, int* out_modifier_id, int* out_type, int* out_id);
static void controller_combo_binding_label(int modifier_type, int modifier_id,
    int type, int id, char* buf, size_t buflen);
static void controller_combo_binding_short_label(int modifier_type,
    int modifier_id, int type, int id, char* buf, size_t buflen);

static const char* controller_gamepad_button_label(int button)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return "A (South)";
    case SDL_GAMEPAD_BUTTON_EAST: return "B (East)";
    case SDL_GAMEPAD_BUTTON_WEST: return "X (West)";
    case SDL_GAMEPAD_BUTTON_NORTH: return "Y (North)";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1 (Left Shoulder)";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1 (Right Shoulder)";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4 (Left Paddle 1)";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5 (Left Paddle 2)";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4 (Right Paddle 1)";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5 (Right Paddle 2)";
    case SDL_GAMEPAD_BUTTON_START: return "Start (Menu)";
    case SDL_GAMEPAD_BUTTON_BACK: return "Back (View)";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "Left Stick Click";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "Right Stick Click";
    case SDL_GAMEPAD_BUTTON_GUIDE: return "Guide (Steam)";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad Click";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
    case SDL_GAMEPAD_BUTTON_MISC1: return "Misc1";
    case SDL_GAMEPAD_BUTTON_MISC2: return "Misc2";
    case SDL_GAMEPAD_BUTTON_MISC3: return "Misc3";
    case SDL_GAMEPAD_BUTTON_MISC4: return "Misc4";
    case SDL_GAMEPAD_BUTTON_MISC5: return "Misc5";
    case SDL_GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "Unknown Button";
    }
}

static const char* controller_gamepad_button_short_label(int button)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return "A";
    case SDL_GAMEPAD_BUTTON_EAST: return "B";
    case SDL_GAMEPAD_BUTTON_WEST: return "X";
    case SDL_GAMEPAD_BUTTON_NORTH: return "Y";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5";
    case SDL_GAMEPAD_BUTTON_START: return "Start";
    case SDL_GAMEPAD_BUTTON_BACK: return "Back";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "L3";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "R3";
    case SDL_GAMEPAD_BUTTON_GUIDE: return "Guide";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-Up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-Down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-Left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-Right";
    case SDL_GAMEPAD_BUTTON_MISC1: return "Misc1";
    case SDL_GAMEPAD_BUTTON_MISC2: return "Misc2";
    case SDL_GAMEPAD_BUTTON_MISC3: return "Misc3";
    case SDL_GAMEPAD_BUTTON_MISC4: return "Misc4";
    case SDL_GAMEPAD_BUTTON_MISC5: return "Misc5";
    case SDL_GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "?";
    }
}

static const char* controller_gamepad_trigger_label(int index)
{
    if (index == 0)
        return "L2 (Left Trigger)";
    if (index == 1)
        return "R2 (Right Trigger)";
    return "Unknown Trigger";
}

static const char* controller_gamepad_trigger_short_label(int index)
{
    if (index == 0)
        return "L2";
    if (index == 1)
        return "R2";
    return "?";
}

static const char* controller_gamepad_stick_dir_label(int type, int dir,
    bool short_label)
{
    const char* stick = (type == GAMEPAD_CAPTURE_RIGHT_STICK)
        ? (short_label ? "RS" : "Right Stick")
        : (short_label ? "LS" : "Left Stick");
    const char* dir_label = NULL;

    switch (dir) {
    case GAMEPAD_STICK_DIR_UP: dir_label = short_label ? "Up" : "Up"; break;
    case GAMEPAD_STICK_DIR_DOWN: dir_label = short_label ? "Down" : "Down"; break;
    case GAMEPAD_STICK_DIR_LEFT: dir_label = short_label ? "Left" : "Left"; break;
    case GAMEPAD_STICK_DIR_RIGHT: dir_label = short_label ? "Right" : "Right"; break;
    default: dir_label = short_label ? "?" : "Unknown"; break;
    }

    return format("%s %s", stick, dir_label);
}

static const char* controller_gamepad_combo_label(void)
{
    return "L1+R1 Combo";
}

static void controller_binding_label_ex(int type, int id, char* buf,
    size_t buflen, bool short_label)
{
    if (!buf || !buflen)
        return;

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        SDL_strlcpy(buf, short_label
            ? controller_gamepad_button_short_label(id)
            : controller_gamepad_button_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        SDL_strlcpy(buf, short_label
            ? controller_gamepad_trigger_short_label(id)
            : controller_gamepad_trigger_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK || type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        SDL_strlcpy(buf, controller_gamepad_stick_dir_label(type, id,
            short_label), buflen);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        SDL_strlcpy(buf, short_label ? "L1+R1" : controller_gamepad_combo_label(),
            buflen);
    } else {
        SDL_strlcpy(buf, "(unknown)", buflen);
    }
}

static void controller_binding_label(int type, int id, char* buf, size_t buflen)
{
    controller_binding_label_ex(type, id, buf, buflen, false);
}

static void controller_binding_short_label(int type, int id, char* buf,
    size_t buflen)
{
    controller_binding_label_ex(type, id, buf, buflen, true);
}

static void controller_append_binding_text(char* buf, size_t buflen,
    size_t* current_len, bool* found, cptr text)
{
    if (!buf || !buflen || !current_len || !found || !text || !text[0])
        return;

    if (*found)
        strnfcat(buf, buflen, current_len, ", %s", text);
    else
    {
        SDL_strlcpy(buf, text, buflen);
        *current_len = strlen(buf);
        *found = true;
    }
}

static int controller_collect_physical_bindings(int binding,
    controller_physical_binding_ref out[], int max_out)
{
    int count = 0;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (!controller_action_binding_equals(get_sdl_gamepad_button_binding(i),
                binding))
            continue;

        if (out && count < max_out) {
            out[count].type = GAMEPAD_CAPTURE_BUTTON;
            out[count].id = i;
        }
        count++;
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (!controller_action_binding_equals(get_sdl_gamepad_trigger_binding(i),
                binding))
            continue;

        if (out && count < max_out) {
            out[count].type = GAMEPAD_CAPTURE_TRIGGER;
            out[count].id = i;
        }
        count++;
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (!controller_action_binding_equals(get_sdl_gamepad_left_stick_binding(i),
                binding))
            continue;

        if (out && count < max_out) {
            out[count].type = GAMEPAD_CAPTURE_LEFT_STICK;
            out[count].id = i;
        }
        count++;
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (!controller_action_binding_equals(get_sdl_gamepad_right_stick_binding(i),
                binding))
            continue;

        if (out && count < max_out) {
            out[count].type = GAMEPAD_CAPTURE_RIGHT_STICK;
            out[count].id = i;
        }
        count++;
    }

    return count;
}

static void controller_describe_action_bindings_ex(int binding, char* buf,
    size_t buflen, bool short_label)
{
    static const int modifiers[] = {
        GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL,
        GAMEPAD_BIND_ALT,
    };
    static const int combo_types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
        GAMEPAD_CAPTURE_LEFT_STICK,
        GAMEPAD_CAPTURE_RIGHT_STICK,
    };
    controller_physical_binding_ref mod_refs[32];
    bool found = false;
    size_t current_len = 0;
    char binding_buf[96];

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (!controller_action_binding_equals(get_sdl_gamepad_button_binding(i),
                binding))
            continue;

        if (short_label)
            controller_binding_short_label(GAMEPAD_CAPTURE_BUTTON, i,
                binding_buf, sizeof(binding_buf));
        else
            controller_binding_label(GAMEPAD_CAPTURE_BUTTON, i, binding_buf,
                sizeof(binding_buf));
        controller_append_binding_text(buf, buflen, &current_len, &found,
            binding_buf);
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (!controller_action_binding_equals(get_sdl_gamepad_trigger_binding(i),
                binding))
            continue;

        if (short_label)
            controller_binding_short_label(GAMEPAD_CAPTURE_TRIGGER, i,
                binding_buf, sizeof(binding_buf));
        else
            controller_binding_label(GAMEPAD_CAPTURE_TRIGGER, i, binding_buf,
                sizeof(binding_buf));
        controller_append_binding_text(buf, buflen, &current_len, &found,
            binding_buf);
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (!controller_action_binding_equals(get_sdl_gamepad_left_stick_binding(i),
                binding))
            continue;

        if (short_label)
            controller_binding_short_label(GAMEPAD_CAPTURE_LEFT_STICK, i,
                binding_buf, sizeof(binding_buf));
        else
            controller_binding_label(GAMEPAD_CAPTURE_LEFT_STICK, i, binding_buf,
                sizeof(binding_buf));
        controller_append_binding_text(buf, buflen, &current_len, &found,
            binding_buf);
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (!controller_action_binding_equals(get_sdl_gamepad_right_stick_binding(i),
                binding))
            continue;

        if (short_label)
            controller_binding_short_label(GAMEPAD_CAPTURE_RIGHT_STICK, i,
                binding_buf, sizeof(binding_buf));
        else
            controller_binding_label(GAMEPAD_CAPTURE_RIGHT_STICK, i, binding_buf,
                sizeof(binding_buf));
        controller_append_binding_text(buf, buflen, &current_len, &found,
            binding_buf);
    }

    if (controller_action_binding_equals(get_sdl_gamepad_shoulder_combo_binding(),
            binding)) {
        if (short_label)
            controller_binding_short_label(GAMEPAD_CAPTURE_SHOULDER_COMBO, 0,
                binding_buf, sizeof(binding_buf));
        else
            controller_binding_label(GAMEPAD_CAPTURE_SHOULDER_COMBO, 0,
                binding_buf, sizeof(binding_buf));
        controller_append_binding_text(buf, buflen, &current_len, &found,
            binding_buf);
    }

    for (int i = 0; i < (int)N_ELEMENTS(modifiers); i++) {
        int mod_count = controller_collect_physical_bindings(modifiers[i],
            mod_refs, N_ELEMENTS(mod_refs));

        if (mod_count <= 0)
            continue;

        for (int ti = 0; ti < (int)N_ELEMENTS(combo_types); ti++) {
            int count = 0;

            if (combo_types[ti] == GAMEPAD_CAPTURE_BUTTON)
                count = SDL_GAMEPAD_BUTTON_COUNT;
            else if (combo_types[ti] == GAMEPAD_CAPTURE_TRIGGER)
                count = GAMEPAD_TRIGGER_COUNT;
            else
                count = GAMEPAD_STICK_DIR_COUNT;

            for (int id = 0; id < count; id++) {
                if (!controller_action_binding_equals(
                        get_sdl_gamepad_combo_binding(modifiers[i],
                            combo_types[ti], id),
                        binding))
                    continue;

                for (int m = 0; m < mod_count && m < (int)N_ELEMENTS(mod_refs);
                    m++) {
                    if (short_label)
                        controller_combo_binding_short_label(mod_refs[m].type,
                            mod_refs[m].id, combo_types[ti], id, binding_buf,
                            sizeof(binding_buf));
                    else
                        controller_combo_binding_label(mod_refs[m].type,
                            mod_refs[m].id, combo_types[ti], id, binding_buf,
                            sizeof(binding_buf));
                    controller_append_binding_text(buf, buflen, &current_len,
                        &found, binding_buf);
                }
            }
        }
    }

    if (!found)
        SDL_strlcpy(buf, "(unbound)", buflen);
}

static void controller_describe_action_bindings_compact(int binding, char* buf,
    size_t buflen)
{
    controller_describe_action_bindings_ex(binding, buf, buflen, true);
}

static bool controller_action_is_modifier(int binding)
{
    return (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL
        || binding == GAMEPAD_BIND_ALT);
}

static bool controller_action_is_confirm(int binding)
{
    return (binding == INPUT_BIND_CONFIRM || binding == ' ');
}

static bool controller_action_binding_equals(int lhs, int rhs)
{
    if (controller_action_is_confirm(lhs) && controller_action_is_confirm(rhs))
        return true;

    return lhs == rhs;
}

static int controller_store_action_binding(int binding)
{
    if (controller_action_is_confirm(binding))
        return ' ';

    return binding;
}

static int controller_action_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_button_binding(i),
                binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_trigger_binding(i),
                binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_left_stick_binding(i),
                binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_right_stick_binding(i),
                binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    if (controller_action_binding_equals(get_sdl_gamepad_shoulder_combo_binding(),
            binding)) {
        if (count == 0 && out_type && out_id) {
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
            *out_id = 0;
        }
        count++;
    }

    return count;
}

static int controller_physical_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_button_binding(i),
                binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_trigger_binding(i),
                binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_left_stick_binding(i),
                binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_right_stick_binding(i),
                binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    return count;
}

static int controller_combo_action_binding_count(int binding, int* out_modifier_type,
    int* out_modifier_id, int* out_type, int* out_id)
{
    static const int modifiers[] = {
        GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL,
        GAMEPAD_BIND_ALT,
    };
    int total = 0;

    for (int i = 0; i < (int)N_ELEMENTS(modifiers); i++) {
        int mod_type = 0;
        int mod_id = 0;
        int mod_count;
        static const int combo_types[] = {
            GAMEPAD_CAPTURE_BUTTON,
            GAMEPAD_CAPTURE_TRIGGER,
            GAMEPAD_CAPTURE_LEFT_STICK,
            GAMEPAD_CAPTURE_RIGHT_STICK,
        };

        mod_count = controller_physical_binding_count(modifiers[i], &mod_type, &mod_id);
        if (mod_count <= 0)
            continue;

        for (int ti = 0; ti < (int)N_ELEMENTS(combo_types); ti++) {
            int count = 0;

            if (combo_types[ti] == GAMEPAD_CAPTURE_BUTTON)
                count = SDL_GAMEPAD_BUTTON_COUNT;
            else if (combo_types[ti] == GAMEPAD_CAPTURE_TRIGGER)
                count = GAMEPAD_TRIGGER_COUNT;
            else
                count = GAMEPAD_STICK_DIR_COUNT;

            for (int id = 0; id < count; id++) {
                if (!controller_action_binding_equals(
                        get_sdl_gamepad_combo_binding(modifiers[i], combo_types[ti], id),
                        binding))
                    continue;

                if (total == 0) {
                    if (out_modifier_type)
                        *out_modifier_type = mod_type;
                    if (out_modifier_id)
                        *out_modifier_id = mod_id;
                    if (out_type)
                        *out_type = combo_types[ti];
                    if (out_id)
                        *out_id = id;
                }

                total += mod_count;
            }
        }
    }

    return total;
}

static void controller_combo_binding_label(int modifier_type, int modifier_id,
    int type, int id, char* buf, size_t buflen)
{
    char mod_buf[48];
    char base_buf[48];

    if (!buf || !buflen)
        return;

    controller_binding_label(modifier_type, modifier_id, mod_buf, sizeof(mod_buf));
    controller_binding_label(type, id, base_buf, sizeof(base_buf));
    strnfmt(buf, buflen, "%s + %s", mod_buf, base_buf);
}

static void controller_combo_binding_short_label(int modifier_type,
    int modifier_id, int type, int id, char* buf, size_t buflen)
{
    char mod_buf[24];
    char base_buf[24];

    if (!buf || !buflen)
        return;

    controller_binding_short_label(modifier_type, modifier_id, mod_buf,
        sizeof(mod_buf));
    controller_binding_short_label(type, id, base_buf, sizeof(base_buf));
    strnfmt(buf, buflen, "%s + %s", mod_buf, base_buf);
}

static void controller_action_binding_label(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    char binding_buf[48];
    int type = 0;
    int id = 0;
    int mod_type = 0;
    int mod_id = 0;
    int direct_count = controller_action_binding_count(binding, &type, &id);
    int combo_count = controller_combo_action_binding_count(binding,
        &mod_type, &mod_id, &type, &id);
    int total_count = direct_count + combo_count;

    if (total_count <= 0) {
        SDL_strlcpy(buf, "(unbound)", buflen);
    } else if (total_count == 1 && direct_count == 1) {
        controller_binding_label(type, id, buf, buflen);
    } else if (total_count == 1) {
        controller_combo_binding_label(mod_type, mod_id, type, id, buf, buflen);
    } else {
        if (direct_count > 0) {
            controller_binding_label(type, id, binding_buf, sizeof(binding_buf));
        } else {
            controller_combo_binding_label(mod_type, mod_id, type, id, binding_buf,
                sizeof(binding_buf));
        }
        strnfmt(buf, buflen, "%s +%d", binding_buf, total_count - 1);
    }
}

static bool controller_binding_matches_action(int binding, int type, int id)
{
    if (type == GAMEPAD_CAPTURE_BUTTON)
        return controller_action_binding_equals(get_sdl_gamepad_button_binding(id),
            binding);
    if (type == GAMEPAD_CAPTURE_TRIGGER)
        return controller_action_binding_equals(get_sdl_gamepad_trigger_binding(id),
            binding);
    if (type == GAMEPAD_CAPTURE_LEFT_STICK)
        return controller_action_binding_equals(get_sdl_gamepad_left_stick_binding(id),
            binding);
    if (type == GAMEPAD_CAPTURE_RIGHT_STICK)
        return controller_action_binding_equals(get_sdl_gamepad_right_stick_binding(id),
            binding);
    if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO)
        return controller_action_binding_equals(get_sdl_gamepad_shoulder_combo_binding(),
            binding);
    return false;
}

static bool controller_capture_matches_action(int binding, int modifier, int type, int id)
{
    if (modifier != GAMEPAD_BIND_NONE) {
        return controller_action_binding_equals(
            get_sdl_gamepad_combo_binding(modifier, type, id), binding);
    }

    return controller_binding_matches_action(binding, type, id);
}

static bool controller_first_nonstick_physical_binding(int binding, int* out_type,
    int* out_id)
{
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (i == SDL_GAMEPAD_BUTTON_LEFT_STICK
            || i == SDL_GAMEPAD_BUTTON_RIGHT_STICK)
            continue;
        if (!controller_action_binding_equals(get_sdl_gamepad_button_binding(i),
                binding))
            continue;

        if (out_type)
            *out_type = GAMEPAD_CAPTURE_BUTTON;
        if (out_id)
            *out_id = i;
        return true;
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (!controller_action_binding_equals(get_sdl_gamepad_trigger_binding(i),
                binding))
            continue;

        if (out_type)
            *out_type = GAMEPAD_CAPTURE_TRIGGER;
        if (out_id)
            *out_id = i;
        return true;
    }

    if (controller_action_binding_equals(get_sdl_gamepad_shoulder_combo_binding(),
            binding)) {
        if (out_type)
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
        if (out_id)
            *out_id = 0;
        return true;
    }

    return false;
}

static bool controller_first_nonstick_combo_binding(int binding, int* out_mod_type,
    int* out_mod_id, int* out_type, int* out_id)
{
    static const int modifiers[] = {
        GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL,
        GAMEPAD_BIND_ALT,
    };
    static const int combo_types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
    };

    for (int i = 0; i < (int)N_ELEMENTS(modifiers); i++) {
        int mod_type = 0;
        int mod_id = 0;

        if (!controller_first_nonstick_physical_binding(modifiers[i],
                &mod_type, &mod_id))
            continue;

        for (int ti = 0; ti < (int)N_ELEMENTS(combo_types); ti++) {
            int count = (combo_types[ti] == GAMEPAD_CAPTURE_BUTTON)
                ? SDL_GAMEPAD_BUTTON_COUNT
                : GAMEPAD_TRIGGER_COUNT;

            for (int id = 0; id < count; id++) {
                if (combo_types[ti] == GAMEPAD_CAPTURE_BUTTON
                    && (id == SDL_GAMEPAD_BUTTON_LEFT_STICK
                        || id == SDL_GAMEPAD_BUTTON_RIGHT_STICK))
                    continue;

                if (!controller_action_binding_equals(
                        get_sdl_gamepad_combo_binding(modifiers[i],
                            combo_types[ti], id),
                        binding))
                    continue;

                if (out_mod_type)
                    *out_mod_type = mod_type;
                if (out_mod_id)
                    *out_mod_id = mod_id;
                if (out_type)
                    *out_type = combo_types[ti];
                if (out_id)
                    *out_id = id;
                return true;
            }
        }
    }

    return false;
}

void controller_prompt_label_no_sticks(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    int type = 0;
    int id = 0;
    int mod_type = 0;
    int mod_id = 0;

    if (!buf || !buflen)
        return;

    if (controller_first_nonstick_physical_binding(binding, &type, &id))
    {
        controller_binding_short_label(type, id, buf, buflen);
        return;
    }

    if (controller_first_nonstick_combo_binding(binding, &mod_type, &mod_id,
            &type, &id))
    {
        controller_combo_binding_short_label(mod_type, mod_id, type, id, buf,
            buflen);
        return;
    }

    SDL_strlcpy(buf, fallback ? fallback : "", buflen);
}

void controller_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple")) {
        SDL_strlcpy(buf, fallback, buflen);
    }
}

static void controller_entry_value(const controller_entry* entry, char* buf, size_t buflen)
{
    if (!entry || !buf || !buflen)
        return;

    switch (entry->type) {
    case CONTROLLER_ENTRY_TOGGLE:
        switch (entry->id) {
        case CONTROLLER_TOGGLE_ENABLED:
            SDL_strlcpy(buf, get_sdl_gamepad_enabled() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_AUTO_MODE:
            SDL_strlcpy(buf, get_sdl_gamepad_auto_mode() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_STEAMDECK_MODE:
            SDL_strlcpy(buf, get_sdl_steamdeck_mode() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_STEAMDECK_INV_EQUIP_SAME_BUTTON_CYCLE:
            SDL_strlcpy(buf,
                get_sdl_steamdeck_inv_equip_same_button_cycle() ? "On" : "Off",
                buflen);
            break;
        case CONTROLLER_TOGGLE_DPAD:
            SDL_strlcpy(buf, get_sdl_gamepad_use_dpad() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_LEFT_STICK:
            SDL_strlcpy(buf, get_sdl_gamepad_use_left_stick() ? "On" : "Off", buflen);
            break;
        default:
            SDL_strlcpy(buf, "(unknown)", buflen);
            break;
        }
        break;
    case CONTROLLER_ENTRY_ACTION:
        controller_action_binding_label(entry->id, buf, buflen);
        break;
    default:
        SDL_strlcpy(buf, "(unknown)", buflen);
        break;
    }
}

static void controller_set_toggle(int toggle_id, bool value)
{
    switch (toggle_id) {
    case CONTROLLER_TOGGLE_ENABLED:
        set_sdl_gamepad_enabled(value);
        break;
    case CONTROLLER_TOGGLE_AUTO_MODE:
        set_sdl_gamepad_auto_mode(value);
        break;
    case CONTROLLER_TOGGLE_STEAMDECK_MODE:
        set_sdl_steamdeck_mode(value);
        break;
    case CONTROLLER_TOGGLE_STEAMDECK_INV_EQUIP_SAME_BUTTON_CYCLE:
        set_sdl_steamdeck_inv_equip_same_button_cycle(value);
        break;
    case CONTROLLER_TOGGLE_DPAD:
        set_sdl_gamepad_use_dpad(value);
        break;
    case CONTROLLER_TOGGLE_LEFT_STICK:
        set_sdl_gamepad_use_left_stick(value);
        break;
    default:
        break;
    }
}

static bool controller_toggle_default_value(int toggle_id)
{
    switch (toggle_id) {
    case CONTROLLER_TOGGLE_ENABLED:
        return get_sdl_gamepad_default_enabled();
    case CONTROLLER_TOGGLE_AUTO_MODE:
        return get_sdl_gamepad_default_auto_mode();
    case CONTROLLER_TOGGLE_STEAMDECK_MODE:
        return get_sdl_steamdeck_default_mode();
    case CONTROLLER_TOGGLE_STEAMDECK_INV_EQUIP_SAME_BUTTON_CYCLE:
        return get_sdl_steamdeck_default_inv_equip_same_button_cycle();
    case CONTROLLER_TOGGLE_DPAD:
        return get_sdl_gamepad_default_use_dpad();
    case CONTROLLER_TOGGLE_LEFT_STICK:
        return get_sdl_gamepad_default_use_left_stick();
    default:
        return false;
    }
}

static void controller_clear_action_bindings(int binding, int skip_type, int skip_id)
{
    if (binding == GAMEPAD_BIND_NONE)
        return;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_button_binding(i),
                binding)) {
            if (skip_type == GAMEPAD_CAPTURE_BUTTON && skip_id == i)
                continue;
            set_sdl_gamepad_button_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_trigger_binding(i),
                binding)) {
            if (skip_type == GAMEPAD_CAPTURE_TRIGGER && skip_id == i)
                continue;
            set_sdl_gamepad_trigger_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_left_stick_binding(i),
                binding)) {
            if (skip_type == GAMEPAD_CAPTURE_LEFT_STICK && skip_id == i)
                continue;
            set_sdl_gamepad_left_stick_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_right_stick_binding(i),
                binding)) {
            if (skip_type == GAMEPAD_CAPTURE_RIGHT_STICK && skip_id == i)
                continue;
            set_sdl_gamepad_right_stick_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    if (controller_action_binding_equals(get_sdl_gamepad_shoulder_combo_binding(),
            binding)) {
        if (!(skip_type == GAMEPAD_CAPTURE_SHOULDER_COMBO))
            set_sdl_gamepad_shoulder_combo_binding(GAMEPAD_BIND_NONE);
    }
}

static void controller_clear_effective_action_bindings(int binding)
{
    static const int modifiers[] = {
        GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL,
        GAMEPAD_BIND_ALT,
    };

    controller_clear_action_bindings(binding, -1, -1);

    for (int mi = 0; mi < (int)N_ELEMENTS(modifiers); mi++) {
        int modifier = modifiers[mi];

        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
            if (controller_action_binding_equals(
                    get_sdl_gamepad_combo_binding(modifier, GAMEPAD_CAPTURE_BUTTON, i),
                    binding)) {
                set_sdl_gamepad_combo_binding(modifier, GAMEPAD_CAPTURE_BUTTON, i,
                    GAMEPAD_BIND_NONE);
            }
        }
        for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
            if (controller_action_binding_equals(
                    get_sdl_gamepad_combo_binding(modifier, GAMEPAD_CAPTURE_TRIGGER, i),
                    binding)) {
                set_sdl_gamepad_combo_binding(modifier, GAMEPAD_CAPTURE_TRIGGER, i,
                    GAMEPAD_BIND_NONE);
            }
        }
        for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
            if (controller_action_binding_equals(
                    get_sdl_gamepad_combo_binding(modifier, GAMEPAD_CAPTURE_LEFT_STICK, i),
                    binding)) {
                set_sdl_gamepad_combo_binding(modifier, GAMEPAD_CAPTURE_LEFT_STICK, i,
                    GAMEPAD_BIND_NONE);
            }
            if (controller_action_binding_equals(
                    get_sdl_gamepad_combo_binding(modifier, GAMEPAD_CAPTURE_RIGHT_STICK, i),
                    binding)) {
                set_sdl_gamepad_combo_binding(modifier, GAMEPAD_CAPTURE_RIGHT_STICK, i,
                    GAMEPAD_BIND_NONE);
            }
        }
    }
}

static void controller_assign_action_binding(int binding, int type, int id)
{
    binding = controller_store_action_binding(binding);

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        set_sdl_gamepad_button_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        set_sdl_gamepad_trigger_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK) {
        set_sdl_gamepad_left_stick_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        set_sdl_gamepad_right_stick_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        set_sdl_gamepad_shoulder_combo_binding(binding);
    }
}

static void controller_assign_combo_binding(int binding, int modifier, int type, int id)
{
    binding = controller_store_action_binding(binding);
    set_sdl_gamepad_combo_binding(modifier, type, id, binding);
}

static bool controller_restore_action_default_bindings(int binding)
{
    static const int modifiers[] = {
        GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL,
        GAMEPAD_BIND_ALT,
    };
    bool restored = false;

    binding = controller_store_action_binding(binding);

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_default_button_binding(i),
                binding)) {
            controller_assign_action_binding(binding, GAMEPAD_CAPTURE_BUTTON, i);
            restored = true;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_default_trigger_binding(i),
                binding)) {
            controller_assign_action_binding(binding, GAMEPAD_CAPTURE_TRIGGER, i);
            restored = true;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_default_left_stick_binding(i),
                binding)) {
            controller_assign_action_binding(binding, GAMEPAD_CAPTURE_LEFT_STICK, i);
            restored = true;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (controller_action_binding_equals(get_sdl_gamepad_default_right_stick_binding(i),
                binding)) {
            controller_assign_action_binding(binding, GAMEPAD_CAPTURE_RIGHT_STICK, i);
            restored = true;
        }
    }

    if (controller_action_binding_equals(get_sdl_gamepad_default_shoulder_combo_binding(),
            binding)) {
        controller_assign_action_binding(binding, GAMEPAD_CAPTURE_SHOULDER_COMBO, 0);
        restored = true;
    }

    for (int mi = 0; mi < (int)N_ELEMENTS(modifiers); mi++) {
        int modifier = modifiers[mi];

        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
            if (controller_action_binding_equals(
                    get_sdl_gamepad_default_combo_binding(modifier,
                        GAMEPAD_CAPTURE_BUTTON, i),
                    binding)) {
                controller_assign_combo_binding(binding, modifier,
                    GAMEPAD_CAPTURE_BUTTON, i);
                restored = true;
            }
        }

        for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
            if (controller_action_binding_equals(
                    get_sdl_gamepad_default_combo_binding(modifier,
                        GAMEPAD_CAPTURE_TRIGGER, i),
                    binding)) {
                controller_assign_combo_binding(binding, modifier,
                    GAMEPAD_CAPTURE_TRIGGER, i);
                restored = true;
            }
        }

        for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
            if (controller_action_binding_equals(
                    get_sdl_gamepad_default_combo_binding(modifier,
                        GAMEPAD_CAPTURE_LEFT_STICK, i),
                    binding)) {
                controller_assign_combo_binding(binding, modifier,
                    GAMEPAD_CAPTURE_LEFT_STICK, i);
                restored = true;
            }
            if (controller_action_binding_equals(
                    get_sdl_gamepad_default_combo_binding(modifier,
                        GAMEPAD_CAPTURE_RIGHT_STICK, i),
                    binding)) {
                controller_assign_combo_binding(binding, modifier,
                    GAMEPAD_CAPTURE_RIGHT_STICK, i);
                restored = true;
            }
        }
    }

    return restored;
}

void do_cmd_controller_settings(void)
{
    bool done = false;
    int highlight = 0;

    static const controller_entry entries[] = {
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_ENABLED, "Controller Input" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_AUTO_MODE, "Auto Controller Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_STEAMDECK_MODE, "Controller UI Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_STEAMDECK_INV_EQUIP_SAME_BUTTON_CYCLE, "Inv/Equip Same-Button Cycle" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_DPAD, "D-pad Movement" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_LEFT_STICK, "Left Stick Movement" },
        { CONTROLLER_ENTRY_ACTION, '\r', "Enter" },
        { CONTROLLER_ENTRY_ACTION, INPUT_BIND_CONFIRM, "Confirm (Space)" },
        { CONTROLLER_ENTRY_ACTION, ESCAPE, "Escape" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_SHIFT, "Shift modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_CTRL, "Ctrl modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_ALT, "Alt modifier" },
        { CONTROLLER_ENTRY_ACTION, '\t', "Change active weapon (Tab)" },
        { CONTROLLER_ENTRY_ACTION, 'y', "Abilities" },
        { CONTROLLER_ENTRY_ACTION, 'i', "Inventory" },
        { CONTROLLER_ENTRY_ACTION, 'e', "Equipment" },
        { CONTROLLER_ENTRY_ACTION, 'u', "Use item" },
        { CONTROLLER_ENTRY_ACTION, 'x', "Examine item" },
        { CONTROLLER_ENTRY_ACTION, 's', "Sing / change song" },
        { CONTROLLER_ENTRY_ACTION, 'S', "Toggle stealth" },
        { CONTROLLER_ENTRY_ACTION, 'h', "Character sheet" },
        { CONTROLLER_ENTRY_ACTION, 'f', "Fire 1st quiver" },
        { CONTROLLER_ENTRY_ACTION, 'F', "Fire 2nd quiver" },
        { CONTROLLER_ENTRY_ACTION, KTRL('F'), "Swap quivers" },
        { CONTROLLER_ENTRY_ACTION, 'l', "Look around" },
        { CONTROLLER_ENTRY_ACTION, 'T', "Tunnel / dig" },
        { CONTROLLER_ENTRY_ACTION, 'b', "Bash door" },
        { CONTROLLER_ENTRY_ACTION, 'z', "Wait" },
        { CONTROLLER_ENTRY_ACTION, 'j', "Supplies overview" },
        { CONTROLLER_ENTRY_ACTION, '.', "Run" },
        { CONTROLLER_ENTRY_ACTION, '/', "Alt action" },
        { CONTROLLER_ENTRY_ACTION, 'w', "Wear / wield" },
        { CONTROLLER_ENTRY_ACTION, 'r', "Remove equipment" },
        { CONTROLLER_ENTRY_ACTION, 'g', "Pick up items" },
        { CONTROLLER_ENTRY_ACTION, 'Z', "Rest" },
        { CONTROLLER_ENTRY_ACTION, 'o', "Open door / chest" },
        { CONTROLLER_ENTRY_ACTION, 'c', "Close door" },
        { CONTROLLER_ENTRY_ACTION, 'D', "Disarm trap / chest" },
        { CONTROLLER_ENTRY_ACTION, 'X', "Exchange places" },
        { CONTROLLER_ENTRY_ACTION, '-', "Fletch arrows" },
        { CONTROLLER_ENTRY_ACTION, '{', "Inscribe item" },
        { CONTROLLER_ENTRY_ACTION, 'a', "Activate staff" },
        { CONTROLLER_ENTRY_ACTION, KTRL('A'), "Swap staff" },
        { CONTROLLER_ENTRY_ACTION, 'E', "Eat food" },
        { CONTROLLER_ENTRY_ACTION, 't', "Throw item" },
        { CONTROLLER_ENTRY_ACTION, 'p', "Blow horn" },
        { CONTROLLER_ENTRY_ACTION, 'q', "Quaff potion" },
        { CONTROLLER_ENTRY_ACTION, 'M', "View map" },
        { CONTROLLER_ENTRY_ACTION, 'L', "Pan view" },
        { CONTROLLER_ENTRY_ACTION, KTRL('Q'), "Combat rolls" },
        { CONTROLLER_ENTRY_ACTION, '0', "Smithing screen" },
        { CONTROLLER_ENTRY_ACTION, '<', "Go upstairs" },
        { CONTROLLER_ENTRY_ACTION, '>', "Go downstairs" },
        { CONTROLLER_ENTRY_ACTION, 'm', "Main menu" },
        { CONTROLLER_ENTRY_ACTION, '?', "Help" },
        { CONTROLLER_ENTRY_ACTION, 'O', "Options menu" },
        { CONTROLLER_ENTRY_ACTION, ':', "Take notes" },
        { CONTROLLER_ENTRY_ACTION, '~', "Knowledge browser" },
        { CONTROLLER_ENTRY_ACTION, '[', "Monster list" },
        { CONTROLLER_ENTRY_ACTION, ']', "Object list" },
    };

    int entry_count = (int)N_ELEMENTS(entries);

    screen_save();

    while (!done) {
        char value_buf[64];
        char detail_value_buf[512];
        char line_buf[128];
        bool steamdeck = steamdeck_controls_active();
        int row_width;

        row_width = settings_ui_line_width(2);

        if (highlight < 0)
            highlight = 0;
        if (highlight >= entry_count)
            highlight = entry_count - 1;

        settings_semantic_menu_begin("Controller Settings", highlight);

        for (int i = 0; i < entry_count; i++) {
            controller_entry_value(&entries[i], value_buf, sizeof(value_buf));
            settings_ui_format_pair_line(line_buf, sizeof(line_buf), entries[i].label,
                value_buf, row_width, 12);

            {
                char semantic_line[160];

                settings_semantic_line_from_menu_line(semantic_line,
                    sizeof(semantic_line), line_buf);
                settings_semantic_add_row(i, semantic_line,
                    (i == highlight) ? TERM_L_BLUE : TERM_WHITE);
                sdl_character_sheet_screen_set_last_select_row_reset(
                    SETTINGS_CLICK_RESET_ROW_BASE + i);
            }
        }

        {
            char desc[1024];

            if (entries[highlight].type == CONTROLLER_ENTRY_ACTION) {
                controller_describe_action_bindings_compact(entries[highlight].id,
                    detail_value_buf, sizeof(detail_value_buf));
                strnfmt(desc, sizeof(desc),
                    "Bindings: %s. Enter or Space adds a controller control. R resets selected, M resets all. Changes are saved on exit.",
                    detail_value_buf);
            } else {
                cptr toggle_desc;

                switch (entries[highlight].id) {
                case CONTROLLER_TOGGLE_ENABLED:
                    toggle_desc = "Turn controller input on or off.";
                    break;
                case CONTROLLER_TOGGLE_AUTO_MODE:
                    toggle_desc =
                        "Automatically switch to the controller UI when a "
                        "controller is connected or used.";
                    break;
                case CONTROLLER_TOGGLE_STEAMDECK_MODE:
                    toggle_desc =
                        "Use the Steam Deck / controller-oriented UI layout.";
                    break;
                case CONTROLLER_TOGGLE_STEAMDECK_INV_EQUIP_SAME_BUTTON_CYCLE:
                    toggle_desc =
                        "Cycle inventory and equipment with one button instead "
                        "of two separate buttons.";
                    break;
                case CONTROLLER_TOGGLE_DPAD:
                    toggle_desc = "Move with the D-pad.";
                    break;
                case CONTROLLER_TOGGLE_LEFT_STICK:
                    toggle_desc = "Move with the left analog stick.";
                    break;
                default:
                    toggle_desc = "";
                    break;
                }
                strnfmt(desc, sizeof(desc), "%s", toggle_desc);
            }
            settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_SELECTED,
                "Reset Selected", steamdeck ? "X" : "R", TERM_SLATE);
            settings_semantic_add_pair_row(SETTINGS_CLICK_RESET_ALL,
                "Reset All", steamdeck ? "Y" : "M", TERM_SLATE);
            sdl_character_sheet_screen_set_select_description(desc);
            sdl_character_sheet_screen_commit_select(highlight);
        }

        char ch = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_generated = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SETTINGS_CLICK_RETURN)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RESET_SELECTED)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'r';
                    click_generated = true;
                }
                else if (clicked_choice == SETTINGS_CLICK_RESET_ALL)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = 'R';
                    click_generated = true;
                }
                else if (clicked_choice >= SETTINGS_CLICK_RESET_ROW_BASE
                    && clicked_choice
                        < SETTINGS_CLICK_RESET_ROW_BASE + entry_count)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    highlight = clicked_choice - SETTINGS_CLICK_RESET_ROW_BASE;
                    ch = 'r';
                    click_generated = true;
                }
                else if (clicked_choice >= 0 && clicked_choice < entry_count)
                {
                    highlight = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = '\r';
                    click_generated = true;
                }
            }

            ch = (char)settings_menu_key(ch, 0, 0, click_generated);
        }

        if (ch == ESCAPE || ch == 'q' || ch == 'Q' || (steamdeck && ch == steamdeck_back_key())) {
            done = true;
        } else if (ch == '8') {
            highlight = (highlight + entry_count - 1) % entry_count;
        } else if (ch == '2') {
            highlight = (highlight + 1) % entry_count;
        } else if (ch == 'r' || (steamdeck && ch == steamdeck_alt_action_key())) {
            if (entries[highlight].type == CONTROLLER_ENTRY_ACTION) {
                controller_clear_effective_action_bindings(entries[highlight].id);
                if (controller_restore_action_default_bindings(entries[highlight].id)) {
                    msg_print("Bindings reset to defaults.");
                } else {
                    msg_print("No default binding for action.");
                }
                message_flush();
            } else {
                controller_set_toggle(entries[highlight].id,
                    controller_toggle_default_value(entries[highlight].id));
                msg_format("Reset %s to default.", entries[highlight].label);
                message_flush();
            }
        } else if (ch == 'R' || (steamdeck && ch == steamdeck_secondary_key())) {
            sdl_gamepad_reset_bindings_to_default();
            msg_print("All bindings reset to defaults.");
            message_flush();
        } else if (ch == '\r' || ch == '\n' || ch == ' ') {
            const controller_entry* entry = &entries[highlight];

            if (entry->type == CONTROLLER_ENTRY_TOGGLE) {
                char cur[16];
                controller_entry_value(entry, cur, sizeof(cur));
                controller_set_toggle(entry->id, streq(cur, "Off"));
            } else {
                char prompt[80];
                char prompt_long[96];
                char prompt_medium[80];
                char prompt_short[64];
                int cap_type = 0;
                int cap_id = 0;
                int cap_modifier = GAMEPAD_BIND_NONE;
                bool allow_modifier_combo = !controller_action_is_modifier(entry->id);
                if (steamdeck) {
                    char cancel_label[16];
                    controller_prompt_label(steamdeck_back_key(), "B", cancel_label, sizeof(cancel_label));
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press control%s to add %s  (%s=cancel)",
                        allow_modifier_combo ? " or modifier+control" : "",
                        entry->label, cancel_label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Add%s to %s  (%s=cancel)",
                        allow_modifier_combo ? " control/combo" : " control",
                        entry->label, cancel_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "Add %s%s  (%s cancel)", entry->label,
                        allow_modifier_combo ? " combo" : "", cancel_label);
                } else {
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press controller control%s to add %s (Esc=cancel, Backspace=clear)",
                        allow_modifier_combo ? " or modifier+control" : "",
                        entry->label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Add %s%s (Esc=cancel, Bksp=clear)", entry->label,
                        allow_modifier_combo ? " with control/combo" : "");
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "Add %s%s (Esc cancel, Bksp clear)", entry->label,
                        allow_modifier_combo ? " combo" : "");
                }
                strnfmt(prompt, sizeof(prompt), "%s",
                    settings_ui_pick_label(row_width, prompt_long, prompt_medium,
                        prompt_short));
                sdl_character_sheet_screen_set_select_description(prompt);
                sdl_character_sheet_screen_commit_select(highlight);

                flush();
                if (!sdl_gamepad_capture_begin(allow_modifier_combo)) {
                    msg_print("No controller detected.");
                    message_flush();
                    continue;
                }

                bool waiting = true;
                while (waiting) {
                    if (sdl_gamepad_capture_poll(&cap_type, &cap_id, &cap_modifier)) {
                        if (cap_modifier == GAMEPAD_BIND_NONE
                            && controller_binding_matches_action(ESCAPE, cap_type, cap_id)) {
                            sdl_gamepad_capture_cancel();
                            waiting = false;
                            break;
                        }

                        if (!controller_action_is_confirm(entry->id)
                            && controller_capture_matches_action(INPUT_BIND_CONFIRM,
                                cap_modifier, cap_type, cap_id)) {
                            msg_print("Rebind Confirm (Space) directly to change that control.");
                            message_flush();
                            if (!sdl_gamepad_capture_begin(allow_modifier_combo))
                                waiting = false;
                            continue;
                        }

                        if (cap_modifier != GAMEPAD_BIND_NONE) {
                            if (cap_type == GAMEPAD_CAPTURE_BUTTON
                                || cap_type == GAMEPAD_CAPTURE_TRIGGER
                                || cap_type == GAMEPAD_CAPTURE_LEFT_STICK
                                || cap_type == GAMEPAD_CAPTURE_RIGHT_STICK) {
                                controller_assign_combo_binding(entry->id,
                                    cap_modifier, cap_type, cap_id);
                                waiting = false;
                                break;
                            }

                            msg_print("That combo input is not supported.");
                            message_flush();
                            if (!sdl_gamepad_capture_begin(allow_modifier_combo))
                                waiting = false;
                            continue;
                        }

                        controller_assign_action_binding(entry->id, cap_type, cap_id);
                        waiting = false;
                        break;
                    }

                    inkey_scan = true;
                    char choice = inkey();
                    if (choice == ESCAPE) {
                        sdl_gamepad_capture_cancel();
                        waiting = false;
                    } else if (choice == '\b' || choice == 127) {
                        sdl_gamepad_capture_cancel();
                        controller_clear_effective_action_bindings(entry->id);
                        waiting = false;
                    } else if (choice == 0) {
                        Term_xtra(TERM_XTRA_DELAY, 10);
                    }
                }
            }
        }
    }

    settings_semantic_menu_hide();
    screen_load();
}

#ifdef ALLOW_MACROS

/*
 * Hack -- append all current macros to the given file
 */
static errr macro_dump(cptr fname)
{
    static cptr mark = "Macro Dump";

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("macro_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old macros */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic macro dump\n\n");

    /* Dump them */
    for (i = 0; i < macro__num; i++)
    {
        /* Start the macro */
        SDL_IOprintf(fff, "# Macro '%d'\n\n", i);

        /* Extract the macro action */
        ascii_to_text(buf, sizeof(buf), macro__act[i]);

        /* Dump the macro action */
        SDL_IOprintf(fff, "A:%s\n", buf);

        /* Extract the macro pattern */
        ascii_to_text(buf, sizeof(buf), macro__pat[i]);

        /* Dump the macro pattern */
        SDL_IOprintf(fff, "P:%s\n", buf);

        /* End the macro */
        SDL_IOprintf(fff, "\n\n");
    }

    /* Start dumping */
    SDL_IOprintf(fff, "\n\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

/*
 * Hack -- ask for a "trigger" (see below)
 *
 * Note the complex use of the "inkey()" function from support/input.c.
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since input handling is simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux(char* buf)
{
    char ch;

    int n = 0;

    char tmp[1024];

    /* Flush */
    flush();

    /* Do not process macros */
    inkey_base = true;

    /* First key */
    ch = inkey();

    /* Read the pattern */
    while (ch != '\0')
    {
        /* Save the key */
        buf[n++] = ch;

        /* Do not process macros */
        inkey_base = true;

        /* Do not wait for keys */
        inkey_scan = true;

        /* Attempt to read a key */
        ch = inkey();
    }

    /* Terminate */
    buf[n] = '\0';

    /* Flush */
    flush();

    /* Convert the trigger */
    ascii_to_text(tmp, sizeof(tmp), buf);

    /* Hack -- display the trigger */
    Term_addstr(-1, TERM_WHITE, tmp);
}

/*
 * Hack -- ask for a keymap "trigger" (see below)
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since input handling is simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux_keymap(char* buf)
{
    char tmp[1024];

    /* Flush */
    flush();

    /* Get a key */
    buf[0] = inkey();
    buf[1] = '\0';

    /* Convert to ascii */
    ascii_to_text(tmp, sizeof(tmp), buf);

    /* Hack -- display the trigger */
    Term_addstr(-1, TERM_WHITE, tmp);

    /* Flush */
    flush();
}

/*
 * Hack -- Append all keymaps to the given file.
 *
 * Hack -- We only append the keymaps for the "active" mode.
 */
static errr keymap_dump(cptr fname)
{
    static cptr mark = "Keymap Dump";

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("keymap_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old keymaps */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic keymap dump\n\n");

    /* Dump them */
    for (i = 0; i < (int)N_ELEMENTS(keymap_act[mode]); i++)
    {
        char key[2] = "?";

        cptr act;

        /* Loop up the keymap */
        act = keymap_act[mode][i];

        /* Skip empty keymaps */
        if (!act)
            continue;

        /* Encode the action */
        ascii_to_text(buf, sizeof(buf), act);

        /* Dump the keymap action */
        SDL_IOprintf(fff, "A:%s\n", buf);

        /* Convert the key into a string */
        key[0] = i;

        /* Encode the key */
        ascii_to_text(buf, sizeof(buf), key);

        /* Dump the keymap pattern */
        SDL_IOprintf(fff, "C:%d:%s\n", mode, buf);

        /* Skip a line */
        SDL_IOprintf(fff, "\n");
    }

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

#endif

/*
 * Interact with "macros"
 *
 * Could use some helpful instructions on this page.  XXX XXX XXX
 */
void do_cmd_macros(void)
{
#ifndef ALLOW_MACROS
    msg_print("Legacy macros are no longer supported.");
#else
    char ch;

    char tmp[1024];

    char pat[1024];

    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening macros menu */
    if (dismiss_active_narrative_banner()) {
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Process requests until done */
    while (1)
    {
        int term_wid = 80;
        int term_hgt = 24;
        int menu_row = 3;
        int action_label_row;
        int action_row;
        int command_row;
        int input_row;

        Term_get_size(&term_wid, &term_hgt);
        action_label_row = MAX(menu_row + 11, term_hgt - 4);
        action_row = MIN(term_hgt - 2, action_label_row + 1);
        command_row = MAX(action_row + 1, term_hgt - 2);
        input_row = MAX(command_row + 1, term_hgt - 1);

        /* Analyze the current action */
        ascii_to_text(tmp, sizeof(tmp), macro_buffer);

        settings_semantic_menu_begin("Interact with Macros", '1');
        sdl_character_sheet_screen_set_select_description(tmp);
        settings_semantic_add_row('1', "1) Load a user pref file",
            TERM_WHITE);
#ifdef ALLOW_MACROS
        settings_semantic_add_row('2', "2) Append macros to a file",
            TERM_WHITE);
        settings_semantic_add_row('3', "3) Query a macro", TERM_WHITE);
        settings_semantic_add_row('4', "4) Create a macro", TERM_WHITE);
        settings_semantic_add_row('5', "5) Remove a macro", TERM_WHITE);
        settings_semantic_add_row('6', "6) Append keymaps to a file",
            TERM_WHITE);
        settings_semantic_add_row('7', "7) Query a keymap", TERM_WHITE);
        settings_semantic_add_row('8', "8) Create a keymap", TERM_WHITE);
        settings_semantic_add_row('9', "9) Remove a keymap", TERM_WHITE);
        settings_semantic_add_row('0', "0) Enter a new action", TERM_WHITE);
#endif /* ALLOW_MACROS */
        sdl_character_sheet_screen_commit_select('1');

        /* Get a command */
        ch = inkey();
        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action)
                && click_action != UI_MENU_CLICK_HOVER)
            {
                if (clicked_choice == -1)
                    ch = ESCAPE;
                else if (clicked_choice == -2)
                    ch = '1';
                else if (clicked_choice >= 0 && clicked_choice <= 255)
                    ch = (char)clicked_choice;
            }
        }

        /* Leave */
        if (ch == ESCAPE)
            break;

        /* Load a user pref file */
        if (ch == '1')
        {
            settings_semantic_menu_hide();
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(command_row);
        }

#ifdef ALLOW_MACROS

        /* Save macros */
        else if (ch == '2')
        {
            char ftmp[80];

            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Append macros to a file", command_row, 0);

            /* Prompt */
            prt("File: ", input_row, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.legacy", op_ptr->base_name);

            /* Ask for a file */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)macro_dump(ftmp);

            /* Prompt */
            msg_print("Appended macros.");
        }

        /* Query a macro */
        else if (ch == '3')
        {
            int k;

            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Query a macro", command_row, 0);

            /* Prompt */
            prt("Trigger: ", input_row, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Get the action */
            k = macro_find_exact(pat);

            /* Nothing found */
            if (k < 0)
            {
                /* Prompt */
                msg_print("Found no macro.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, macro__act[k], sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);

                /* Display the current action */
                Term_putstr(0, action_row, term_wid, TERM_WHITE, tmp);

                /* Prompt */
                msg_print("Found a macro.");
            }
        }

        /* Create a macro */
        else if (ch == '4')
        {
            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Create a macro", command_row, 0);

            /* Prompt */
            prt("Trigger: ", input_row, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Clear */
            clear_from(action_label_row);

            /* Prompt */
            prt("Action: ", action_row, 0);

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (askfor_aux(tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Link the macro */
                macro_add(pat, macro_buffer);

                /* Prompt */
                msg_print("Added a macro.");
            }
        }

        /* Remove a macro */
        else if (ch == '5')
        {
            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Remove a macro", command_row, 0);

            /* Prompt */
            prt("Trigger: ", input_row, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Link the macro */
            macro_add(pat, pat);

            /* Prompt */
            msg_print("Removed a macro.");
        }

        /* Save keymaps */
        else if (ch == '6')
        {
            char ftmp[80];

            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Append keymaps to a file", command_row, 0);

            /* Prompt */
            prt("File: ", input_row, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.legacy", op_ptr->base_name);

            /* Ask for a file */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)keymap_dump(ftmp);

            /* Prompt */
            msg_print("Appended keymaps.");
        }

        /* Query a keymap */
        else if (ch == '7')
        {
            cptr act;

            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Query a keymap", command_row, 0);

            /* Prompt */
            prt("Keypress: ", input_row, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Look up the keymap */
            act = keymap_act[mode][(byte)(pat[0])];

            /* Nothing found */
            if (!act)
            {
                /* Prompt */
                msg_print("Found no keymap.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, act, sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);

                /* Display the current action */
                Term_putstr(0, action_row, term_wid, TERM_WHITE, tmp);

                /* Prompt */
                msg_print("Found a keymap.");
            }
        }

        /* Create a keymap */
        else if (ch == '8')
        {
            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Create a keymap", command_row, 0);

            /* Prompt */
            prt("Keypress: ", input_row, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Clear */
            clear_from(action_label_row);

            /* Prompt */
            prt("Action: ", action_row, 0);

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (askfor_aux(tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Free old keymap */
                str_free(keymap_act[mode][(byte)(pat[0])]);

                /* Make new keymap */
                keymap_act[mode][(byte)(pat[0])] = str_dup(macro_buffer);

                /* Prompt */
                msg_print("Added a keymap.");
            }
        }

        /* Remove a keymap */
        else if (ch == '9')
        {
            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Remove a keymap", command_row, 0);

            /* Prompt */
            prt("Keypress: ", input_row, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Free old keymap */
            str_free(keymap_act[mode][(byte)(pat[0])]);

            /* Make new keymap */
            keymap_act[mode][(byte)(pat[0])] = NULL;

            /* Prompt */
            msg_print("Removed a keymap.");
        }

        /* Enter a new action */
        else if (ch == '0')
        {
            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Enter a new action", command_row, 0);

            /* Go to the correct location */
            Term_gotoxy(0, action_row);

            /* Analyze the current action */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (askfor_aux(tmp, 80))
            {
                /* Extract an action */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);
            }
        }

#endif /* ALLOW_MACROS */

        /* Oops */
        else
        {
            /* Oops */
            bell("Illegal command for macros!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    settings_semantic_menu_hide();
    screen_load();
#endif
}

#ifdef ALLOW_VISUALS
/*
 * Asks to the player for an extended color. It is done in two steps:
 * 1. Asks for the base color.
 * 2. Asks for a specific shade.
 * It erases the given line.
 * If the user press ESCAPE no changes are made to attr.
 */
static void askfor_shade(byte* attr, int y)
{
    byte base, shade, temp;
    bool changed = false;
    char *msg, *pos;
    int ch;

    /* Start with the given base color */
    base = GET_BASE_COLOR(*attr);

    /* 1. Query for base color */
    while (1)
    {
        /* Clear the line */
        Term_erase(0, y, 255);

        /* Format the query */
        msg = format("1. Choose base color (Left/Right) " COLOR_SAMPLE
                     " %s (attr = %d) ",
            color_names[base], base);

        /* Display it */
        c_put_str(TERM_WHITE, msg, y, 0);

        /* Find the sample */
        pos = strstr(msg, COLOR_SAMPLE);

        /* Show it using the proper color */
        c_put_str(base, COLOR_SAMPLE, y, pos - msg);

        /* Place the cursor at the end of the message */
        Term_gotoxy(strlen(msg), y);

        /* Get a command */
        ch = inkey();

        /* Cancel */
        if (ch == ESCAPE)
        {
            /* Clear the line */
            Term_erase(0, y, 255);
            return;
        }

        /* Accept the current base color */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous color if possible */
        if ((ch == '4') && (base > 0))
        {
            --base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }

        /* Move to the next color if possible */
        if ((ch == '6') && (base < MAX_BASE_COLORS - 1))
        {
            ++base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }
    }

    /* The player selected a different base color, start from shade 0 */
    if (changed)
        shade = 0;
    /* We assume that the player is editing the current shade, go there */
    else
        shade = GET_SHADE(*attr);

    /* 2. Query for specific shade */
    while (1)
    {
        /* Clear the line */
        Term_erase(0, y, 255);

        /* Create the real color */
        temp = MAKE_EXTENDED_COLOR(base, shade);

        /* Format the message */
        msg = format("2. Choose shade (Left/Right) " COLOR_SAMPLE
                     " %s (attr = %d) ",
            get_ext_color_name(temp), temp);

        /* Display it */
        c_put_str(TERM_WHITE, msg, y, 0);

        /* Find the sample */
        pos = strstr(msg, COLOR_SAMPLE);

        /* Show it using the proper color */
        c_put_str(temp, COLOR_SAMPLE, y, pos - msg);

        /* Place the cursor at the end of the message */
        Term_gotoxy(strlen(msg), y);

        /* Get a command */
        ch = inkey();

        /* Cancel */
        if (ch == ESCAPE)
        {
            /* Clear the line */
            Term_erase(0, y, 255);
            return;
        }

        /* Accept the current shade */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous shade if possible */
        if ((ch == '4') && (shade > 0))
        {
            --shade;
            continue;
        }

        /* Move to the next shade if possible */
        if ((ch == '6') && (shade < MAX_SHADES - 1))
        {
            ++shade;
            continue;
        }
    }

    /* Assign the selected shade */
    *attr = temp;

    /* Clear the line. It is needed to fit in the current UI */
    Term_erase(0, y, 255);
}

/*
 * Interact with "visuals"
 */
void do_cmd_visuals(void)
{
    int ch;
    int cx;

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Save screen */
    screen_save();

    /* Interact until done */
    while (1)
    {
        settings_semantic_menu_begin("Interact with Visuals", '1');
        settings_semantic_add_row('1', "1) Load a user pref file", TERM_WHITE);
#ifdef ALLOW_VISUALS
        settings_semantic_add_row('2', "2) Dump monster attr/chars", TERM_WHITE);
        settings_semantic_add_row('3', "3) Dump object attr/chars", TERM_WHITE);
        settings_semantic_add_row('4', "4) Dump feature attr/chars", TERM_WHITE);
        settings_semantic_add_row('5', "5) Dump flavor attr/chars", TERM_WHITE);
        settings_semantic_add_row('6', "6) Change monster attr/chars", TERM_WHITE);
        settings_semantic_add_row('7', "7) Change object attr/chars", TERM_WHITE);
        settings_semantic_add_row('8', "8) Change feature attr/chars", TERM_WHITE);
        settings_semantic_add_row('9', "9) Change flavor attr/chars", TERM_WHITE);
#endif
        settings_semantic_add_row('0', "0) Reset visuals", TERM_WHITE);
        sdl_character_sheet_screen_set_select_description(
            "Visual preference tools. Editor actions open the existing attr/char editor after this menu hides.");
        sdl_character_sheet_screen_commit_select('1');

        /* Prompt */
        ch = inkey();
        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action)
                && click_action != UI_MENU_CLICK_HOVER)
            {
                if (clicked_choice == -1)
                    ch = ESCAPE;
                else if (clicked_choice == -2)
                    ch = '1';
                else if (clicked_choice >= 0 && clicked_choice <= 255)
                    ch = (char)clicked_choice;
            }
        }

        /* Done */
        if (ch == ESCAPE)
            break;

        if ((ch >= '6') && (ch <= '9'))
        {
            int term_wid = 80;
            int term_hgt = 24;

            Term_get_size(&term_wid, &term_hgt);
            if ((term_wid < 60) || (term_hgt < 21))
            {
                settings_semantic_menu_hide();
                msg_print("The attr/char editor requires a larger window than compact mode.");
                message_flush();
                continue;
            }
        }

        /* Load a user pref file */
        if (ch == '1')
        {
            settings_semantic_menu_hide();
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(15);
        }

#ifdef ALLOW_VISUALS

        /* Dump monster attr/chars */
        else if (ch == '2')
        {
            static cptr mark = "Monster attr/chars";
            char ftmp[80];

            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Dump monster attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.legacy", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_monsters: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Monster attr/char definitions\n\n");

            /* Dump monsters */
            for (i = 0; i < z_info->r_max; i++)
            {
                monster_race* r_ptr = &r_info[i];

                /* Skip non-entries */
                if (!r_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (r_name + r_ptr->name));

                /* Dump the monster attr/char info */
                dump_visual_pair(fff, "R", i, r_ptr->x_attr, (byte)r_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped monster attr/chars.");
        }

        /* Dump object attr/chars */
        else if (ch == '3')
        {
            static cptr mark = "Object attr/chars";
            char ftmp[80];

            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Dump object attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.legacy", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_objects: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Object attr/char definitions\n\n");

            /* Dump objects */
            for (i = 0; i < z_info->k_max; i++)
            {
                object_kind* k_ptr = &k_info[i];

                /* Skip non-entries */
                if (!k_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (k_name + k_ptr->name));

                /* Dump the object attr/char info */
                dump_visual_pair(
                    fff, "K", i, k_ptr->x_attr, (byte)k_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped object attr/chars.");
        }

        /* Dump feature attr/chars */
        else if (ch == '4')
        {
            static cptr mark = "Feature attr/chars";
            char ftmp[80];

            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Dump feature attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.legacy", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_features: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Feature attr/char definitions\n\n");

            /* Dump features */
            for (i = 0; i < z_info->f_max; i++)
            {
                feature_type* f_ptr = &f_info[i];

                /* Skip non-entries */
                if (!f_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (f_name + f_ptr->name));

                /* Dump the feature attr/char info */
                dump_visual_pair(
                    fff, "F", i, f_ptr->x_attr, (byte)f_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped feature attr/chars.");
        }

        /* Dump flavor attr/chars */
        else if (ch == '5')
        {
            static cptr mark = "Flavor attr/chars";
            char ftmp[80];

            settings_semantic_menu_hide();
            /* Prompt */
            prt("Command: Dump flavor attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.legacy", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_flavors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Flavor attr/char definitions\n\n");

            /* Dump flavors */
            for (i = 0; i < z_info->flavor_max; i++)
            {
                flavor_type* flavor_ptr = &flavor_info[i];

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (flavor_text + flavor_ptr->text));

                /* Dump the flavor attr/char info */
                dump_visual_pair(
                    fff, "L", i, flavor_ptr->x_attr, (byte)flavor_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped flavor attr/chars.");
        }

        /* Modify monster attr/chars */
        else if (ch == '6')
        {
            static int r = 0;

            settings_semantic_menu_hide();

            /* Prompt */
            prt("Command: Change monster attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                monster_race* r_ptr = &r_info[r];

                byte da = (byte)(r_ptr->d_attr);
                byte dc = (byte)(r_ptr->d_char);
                byte ca = (byte)(r_ptr->x_attr);
                byte cc = (byte)(r_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Monster = %d, Name = %-40.40s", r,
                        (r_name + r_ptr->name)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3u / %3u", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3u / %3u", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = inkey();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    r = (r + z_info->r_max + 1) % z_info->r_max;
                if (cx == 'N')
                    r = (r + z_info->r_max - 1) % z_info->r_max;
                if (cx == 'a')
                    r_ptr->x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    r_ptr->x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    r_ptr->x_char = (byte)(cc + 1);
                if (cx == 'C')
                    r_ptr->x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&r_ptr->x_attr, 22);
                }
            }
        }

        /* Modify object attr/chars */
        else if (ch == '7')
        {
            static int k = 0;

            settings_semantic_menu_hide();

            /* Prompt */
            prt("Command: Change object attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                object_kind* k_ptr = &k_info[k];

                byte da = (byte)(k_ptr->d_attr);
                byte dc = (byte)(k_ptr->d_char);
                byte ca = (byte)(k_ptr->x_attr);
                byte cc = (byte)(k_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Object = %d, Name = %-40.40s", k,
                        (k_name + k_ptr->name)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3d / %3d", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3d / %3d", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = inkey();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    k = (k + z_info->k_max + 1) % z_info->k_max;
                if (cx == 'N')
                    k = (k + z_info->k_max - 1) % z_info->k_max;
                if (cx == 'a')
                    k_info[k].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    k_info[k].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    k_info[k].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    k_info[k].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&k_info[k].x_attr, 22);
                }
            }
        }

        /* Modify feature attr/chars */
        else if (ch == '8')
        {
            static int f = 0;

            settings_semantic_menu_hide();

            /* Prompt */
            prt("Command: Change feature attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                feature_type* f_ptr = &f_info[f];

                byte da = (byte)(f_ptr->d_attr);
                byte dc = (byte)(f_ptr->d_char);
                byte ca = (byte)(f_ptr->x_attr);
                byte cc = (byte)(f_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Terrain = %d, Name = %-40.40s", f,
                        (f_name + f_ptr->name)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3d / %3d", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3d / %3d", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = inkey();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->f_max + 1) % z_info->f_max;
                if (cx == 'N')
                    f = (f + z_info->f_max - 1) % z_info->f_max;
                if (cx == 'a')
                    f_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    f_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    f_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    f_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&f_info[f].x_attr, 22);
                }
            }
        }

        /* Modify flavor attr/chars */
        else if (ch == '9')
        {
            static int f = 0;

            settings_semantic_menu_hide();

            /* Prompt */
            prt("Command: Change flavor attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                flavor_type* flavor_ptr = &flavor_info[f];

                byte da = (byte)(flavor_ptr->d_attr);
                byte dc = (byte)(flavor_ptr->d_char);
                byte ca = (byte)(flavor_ptr->x_attr);
                byte cc = (byte)(flavor_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Flavor = %d, Text = %-40.40s", f,
                        (flavor_text + flavor_ptr->text)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3d / %3d", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3d / %3d", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = inkey();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->flavor_max + 1) % z_info->flavor_max;
                if (cx == 'N')
                    f = (f + z_info->flavor_max - 1) % z_info->flavor_max;
                if (cx == 'a')
                    flavor_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    flavor_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    flavor_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    flavor_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&flavor_info[f].x_attr, 22);
                }
            }
        }

#endif /* ALLOW_VISUALS */

        /* Reset visuals */
        else if (ch == '0')
        {
            settings_semantic_menu_hide();

            /* Reset */
            reset_visuals(true);

            /* Message */
            msg_print("Visual attr/char tables reset.");
        }

        /* Unknown option */
        else
        {
            bell("Illegal command for visuals!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    settings_semantic_menu_hide();
    screen_load();
}
#else
void do_cmd_visuals(void)
{
    msg_print("Legacy visual overrides are no longer supported.");
}
#endif

#ifdef ALLOW_COLORS
/*
 * Asks to the user for specific color values.
 * Returns true if the color was modified.
 */
static bool askfor_color_values(int idx)
{
    char str[10];

    int k, r, g, b;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][1]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Red (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    r = atoi(str);

    /* Check bounds */
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][2]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Green (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    g = atoi(str);

    /* Check bounds */
    if (g < 0)
        g = 0;
    if (g > 255)
        g = 255;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][3]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Blue (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    b = atoi(str);

    /* Check bounds */
    if (b < 0)
        b = 0;
    if (b > 255)
        b = 255;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][0]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Extra (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    k = atoi(str);

    /* Check bounds */
    if (k < 0)
        k = 0;
    if (k > 255)
        k = 255;

    /* Do nothing if the color is not modified */
    if ((k == angband_color_table[idx][0]) && (r == angband_color_table[idx][1])
        && (g == angband_color_table[idx][2])
        && (b == angband_color_table[idx][3]))
        return false;

    /* Modify the color table */
    angband_color_table[idx][0] = k;
    angband_color_table[idx][1] = r;
    angband_color_table[idx][2] = g;
    angband_color_table[idx][3] = b;

    /* Notify the changes */
    return true;
}

/* These two are used to place elements in the grid */
#define COLOR_X(idx) (((idx) / MAX_BASE_COLORS) * 5 + 1)
#define COLOR_Y(idx) ((idx) % MAX_BASE_COLORS + 6)

/* Hack - Note the cast to "int" to prevent overflow */
#define IS_BLACK(idx)                                                          \
    ((int)angband_color_table[idx][1] + (int)angband_color_table[idx][2]       \
            + (int)angband_color_table[idx][3]                                 \
        == 0)

/* We show black as dots to see the shape of the grid */
#define BLACK_SAMPLE "..."

/*
 * The screen used to modify the color table. Only 128 colors can be modified.
 * The remaining entries of the color table are reserved for graphic mode.
 */
static void modify_colors(void)
{
    int x, y, idx, old_idx;
    char ch;
    char msg[100];

    /* Flags */
    bool do_move, do_update;

    /* Clear the screen */
    Term_clear();

    /* Draw the color table */
    for (idx = 0; idx < MAX_COLORS; idx++)
    {
        /* Get coordinates, the x value is adjusted to show a fake cursor */
        x = COLOR_X(idx) + 1;
        y = COLOR_Y(idx);

        /* Show a sample of the color */
        if (IS_BLACK(idx))
            c_put_str(TERM_WHITE, BLACK_SAMPLE, y, x);
        else
            c_put_str(idx, COLOR_SAMPLE, y, x);
    }

    /* Show screen commands and help */
    y = 2;
    x = 42;
    c_put_str(TERM_WHITE, "Commands:", y, x);
    c_put_str(TERM_WHITE, "ESC: Return", y + 2, x);
    c_put_str(TERM_WHITE, "Left/Right: Move to color", y + 3, x);
    c_put_str(TERM_WHITE, "k,K: Incr,Decr extra value", y + 4, x);
    c_put_str(TERM_WHITE, "r,R: Incr,Decr red value", y + 5, x);
    c_put_str(TERM_WHITE, "g,G: Incr,Decr green value", y + 6, x);
    c_put_str(TERM_WHITE, "b,B: Incr,Decr blue value", y + 7, x);
    c_put_str(TERM_WHITE, "c: Copy from color", y + 8, x);
    c_put_str(TERM_WHITE, "v: Set specific values", y + 9, x);
    c_put_str(TERM_WHITE, "First column: base colors", y + 11, x);
    c_put_str(TERM_WHITE, "Second column: first shade, etc.", y + 12, x);

    c_put_str(
        TERM_WHITE, "Shades look like base colors in 16 color ports.", 23, 0);

    /* Hack - We want to show the fake cursor */
    do_move = true;
    do_update = true;

    /* Start with the first color */
    idx = 0;

    /* Used to erase the old position of the fake cursor */
    old_idx = -1;

    while (1)
    {
        /* Movement request */
        if (do_move)
        {
            /* Erase the old fake cursor */
            if (old_idx >= 0)
            {
                /* Get coordinates */
                x = COLOR_X(old_idx);
                y = COLOR_Y(old_idx);

                /* Draw spaces */
                c_put_str(TERM_WHITE, " ", y, x);
                c_put_str(TERM_WHITE, " ", y, x + 4);
            }

            /* Show the current fake cursor */
            /* Get coordinates */
            x = COLOR_X(idx);
            y = COLOR_Y(idx);

            /* Draw the cursor */
            c_put_str(TERM_WHITE, ">", y, x);
            c_put_str(TERM_WHITE, "<", y, x + 4);

            /* Format the name of the color */
            SDL_strlcpy(msg,
                format("Color = %d (0x%02X), Name = %s", idx, idx,
                    get_ext_color_name(idx)),
                sizeof(msg));

            /* Show the name and some whitespace */
            c_put_str(TERM_WHITE, format("%-40s", msg), 2, 0);
        }

        /* Color update request */
        if (do_update)
        {
            /* Get coordinates, adjust x */
            x = COLOR_X(idx) + 1;
            y = COLOR_Y(idx);

            /* Hack - Redraw the sample if needed */
            if (IS_BLACK(idx))
                c_put_str(TERM_WHITE, BLACK_SAMPLE, y, x);
            else
                c_put_str(idx, COLOR_SAMPLE, y, x);

            /* Notify the changes in the color table to the terminal */
            Term_xtra(TERM_XTRA_REACT, 0);

            /* The user is playing with white, redraw all */
            if (idx == TERM_WHITE)
                Term_redraw();

            /* Or reduce flickering by redrawing the changes only */
            else
                Term_redraw_section(x, y, x + 2, y);
        }

        /* Common code, show the values in the color table */
        if (do_move || do_update)
        {
            /* Format the view of the color values */
            SDL_strlcpy(msg,
                format("K = %d / R,G,B = %d, %d, %d",
                    angband_color_table[idx][0], angband_color_table[idx][1],
                    angband_color_table[idx][2], angband_color_table[idx][3]),
                sizeof(msg));

            /* Show color values and some whitespace */
            c_put_str(TERM_WHITE, format("%-40s", msg), 4, 0);
        }

        /* Reset flags */
        do_move = false;
        do_update = false;
        old_idx = -1;

        /* Get a command */
        if (!get_com("Command: Modify colors ", &ch))
            break;

        switch (ch)
        {
        /* Down */
        case '2':
        {
            /* Check bounds */
            if (idx + 1 >= MAX_COLORS)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            ++idx;

            /* Request movement */
            do_move = true;
            break;
        }

        /* Up */
        case '8':
        {
            /* Check bounds */
            if (idx - 1 < 0)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            --idx;

            /* Request movement */
            do_move = true;
            break;
        }

        /* Left */
        case '4':
        {
            /* Check bounds */
            if (idx - 16 < 0)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            idx -= 16;

            /* Request movement */
            do_move = true;
            break;
        }

            /* Right */
        case '6':
        {
            /* Check bounds */
            if (idx + 16 >= MAX_COLORS)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            idx += 16;

            /* Request movement */
            do_move = true;
            break;
        }

            /* Copy from color */
        case 'c':
        {
            char str[10];
            int src;

            /* Get the default value, the base color */
            sprintf(str, "%d", GET_BASE_COLOR(idx));

            /* Query, check for ESCAPE */
            if (!term_get_string(format("Copy from color (0-%d, def. base) ",
                                     MAX_COLORS - 1),
                    str, sizeof(str)))
                break;

            /* Convert to number */
            src = atoi(str);

            /* Check bounds */
            if (src < 0)
                src = 0;
            if (src >= MAX_COLORS)
                src = MAX_COLORS - 1;

            /* Do nothing if the colors are the same */
            if (src == idx)
                break;

            /* Modify the color table */
            angband_color_table[idx][0] = angband_color_table[src][0];
            angband_color_table[idx][1] = angband_color_table[src][1];
            angband_color_table[idx][2] = angband_color_table[src][2];
            angband_color_table[idx][3] = angband_color_table[src][3];

            /* Request update */
            do_update = true;
            break;
        }

        /* Increase the extra value */
        case 'k':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Decrease the extra value */
        case 'K':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Increase the red value */
        case 'r':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Decrease the red value */
        case 'R':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Increase the green value */
        case 'g':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Decrease the green value */
        case 'G':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Increase the blue value */
        case 'b':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Decrease the blue value */
        case 'B':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Ask for specific values */
        case 'v':
        {
            do_update = askfor_color_values(idx);
            break;
        }
        }
    }
}
#endif

/*
 * Interact with "colors"
 */
void do_cmd_colors(void)
{
    int count;
    int selected = 0;
    int choice;
    struct settings_value_choice choices[UI_COLOR_PRESET_MAX];

    if (ui_colors_palette_preset_count() <= 0)
        ui_colors_load_palette_presets();

    count = MIN(ui_colors_palette_preset_count(), UI_COLOR_PRESET_MAX);
    if (count <= 0)
    {
        msg_print("No palette presets are available.");
        return;
    }

    for (int i = 0; i < count; i++)
    {
        choices[i].value = i;
        choices[i].label = ui_colors_palette_preset_label(i);
        if (streq(ui_colors_palette_preset_id(i),
                ui_colors_current_palette_preset()))
        {
            selected = i;
        }
    }

    if (!settings_pick_value("Choose Palette", NULL, choices, count,
            selected, &choice))
    {
        return;
    }

    if (!ui_colors_apply_palette_preset(ui_colors_palette_preset_id(choice)))
    {
        msg_print("Failed to apply palette preset.");
        return;
    }

    SDL_strlcpy(config.palette_preset, ui_colors_current_palette_preset(),
        sizeof(config.palette_preset));
    Term_xtra(TERM_XTRA_REACT, 0);
    Term_redraw();

    if (save_pane_config_to_json())
        msg_format("Palette preset '%s' saved.", config.palette_preset);
    else
        msg_print("Palette changed, but saving sil_sdl.json failed.");
}

/*
 * Take notes.  There are two ways this can happen, either in the message recall
 * or a file.  The command can also be passed a string, which will automatically
 * be written. -CK-
 */
