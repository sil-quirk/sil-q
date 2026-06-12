#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"
#include "ui/status/status-internal.h"

bool ui_hide_left_panel(void)
{
    return !g_sdl_left_panel_pane_source_active;
}

bool ui_status_system_compact(void)
{
    return false;
}

bool ui_compact_height(void)
{
    return SIL_UI_COMPACT_HEIGHT;
}

bool ui_compact_status_line_handles_song(void)
{
    return false;
}

bool ui_compact_status_line_handles_wounds(void)
{
    return false;
}

bool ui_wound_rows_overlap_status_line(void)
{
    return (ROW_CUT == ROW_STATE) || (ROW_POISONED == ROW_STATE);
}

bool ui_status_pane_owns_left_panel_statuses(void)
{
    return true;
}

bool ui_depth_menu_owns_left_panel_depth(void)
{
    return true;
}


byte g_hidden_left_panel_overlay_rows = 0;
byte g_hidden_left_panel_overlay_start_row = 0;
byte g_hidden_left_panel_overlay_start_cols[16] = { 0 };
byte g_hidden_left_panel_overlay_widths[16] = { 0 };
byte g_hidden_left_panel_overlay_attack_modes[16] = { 0 };
byte g_hidden_left_panel_overlay_attack_start_cols[16] = { 0 };
byte g_hidden_left_panel_overlay_attack_end_cols[16] = { 0 };
byte g_hidden_left_panel_overlay_click_actions[16] = { 0 };
byte g_hidden_left_panel_overlay_click_start_cols[16] = { 0 };
byte g_hidden_left_panel_overlay_click_end_cols[16] = { 0 };
byte g_left_panel_quiver_attack_modes[2] = { 0 };
byte g_left_panel_quiver_attack_start_cols[2] = { 0 };
byte g_left_panel_quiver_attack_end_cols[2] = { 0 };
bool g_suppress_hidden_left_panel_overlay = false;


static byte left_panel_selected_attack_attr(void)
{
    return (byte)(TERM_UI_SELECTED + TERM_YELLOW);
}

static byte left_panel_selected_quiver_attr(void)
{
    return (byte)(TERM_UI_SELECTED + TERM_L_WHITE);
}

static bool left_panel_attr_is_selected(byte attr)
{
    return attr >= TERM_UI_SELECTED;
}

static byte left_panel_selected_bg_from_attr(byte attr)
{
    return left_panel_attr_is_selected(attr) ? attr : 0;
}

static bool pointer_attack_ranged_panel_highlighted(void)
{
    return sdl_pointer_attack_panel_mode_highlighted(SDL_POINTER_ATTACK_RANGED_1)
        || sdl_pointer_attack_panel_mode_highlighted(
            SDL_POINTER_ATTACK_RANGED_2);
}

static byte pointer_attack_panel_attr(int mode, byte base_attr)
{
    return sdl_pointer_attack_panel_mode_highlighted(mode)
        ? left_panel_selected_attack_attr()
        : base_attr;
}

static byte pointer_attack_panel_bg_attr(int mode)
{
    return sdl_pointer_attack_panel_mode_highlighted(mode)
        ? left_panel_selected_attack_attr()
        : 0;
}

static byte pointer_attack_quiver_panel_attr(int mode, byte base_attr)
{
    return sdl_pointer_attack_panel_mode_highlighted(mode)
        ? left_panel_selected_quiver_attr()
        : base_attr;
}

static byte pointer_attack_quiver_panel_bg_attr(int mode)
{
    return sdl_pointer_attack_panel_mode_highlighted(mode)
        ? left_panel_selected_quiver_attr()
        : 0;
}

static byte pointer_attack_ranged_panel_attr(byte base_attr)
{
    return pointer_attack_ranged_panel_highlighted()
        ? left_panel_selected_attack_attr()
        : base_attr;
}

static byte pointer_attack_ranged_panel_bg_attr(void)
{
    return pointer_attack_ranged_panel_highlighted()
        ? left_panel_selected_attack_attr()
        : 0;
}

static bool pointer_attack_ammo_is_throwing(const object_type* ammo)
{
    u32b f1 = 0;
    u32b f2 = 0;
    u32b f3 = 0;
    u32b f4 = 0;

    if (!ammo || !ammo->k_idx)
        return false;

    object_flags4(ammo, &f1, &f2, &f3, &f4);
    return player_can_treat_as_throwing_flags(ammo, f3);
}

byte panel_touch_zone_attr(int action, int row, byte base_attr)
{
#ifdef USE_SDL
    if (sdl_left_panel_pane_renders_character_panel())
        return base_attr;
#endif

    return sdl_character_panel_touch_zone_selected(action, row)
        ? TERM_YELLOW
        : base_attr;
}

byte status_touch_zone_attr(int action, int col, int width,
    byte base_attr)
{
    return sdl_status_line_touch_zone_selected(action, col, width)
        ? TERM_YELLOW
        : base_attr;
}

static const object_type* pointer_attack_ranged_ammo_for_mode(int mode)
{
    switch (mode)
    {
    case SDL_POINTER_ATTACK_RANGED_1:
        return &inventory[INVEN_QUIVER1];
    case SDL_POINTER_ATTACK_RANGED_2:
        return &inventory[INVEN_QUIVER2];
    default:
        return NULL;
    }
}

static int pointer_attack_ranged_display_mode(void)
{
    int mode = sdl_pointer_attack_current_mode();

    if (mode == SDL_POINTER_ATTACK_RANGED_1
        || mode == SDL_POINTER_ATTACK_RANGED_2)
    {
        const object_type* ammo = pointer_attack_ranged_ammo_for_mode(mode);

        if (ammo && ammo->k_idx)
            return mode;
    }

    if (pointer_attack_ammo_is_throwing(&inventory[INVEN_QUIVER1]))
        return SDL_POINTER_ATTACK_RANGED_1;
    if (pointer_attack_ammo_is_throwing(&inventory[INVEN_QUIVER2]))
        return SDL_POINTER_ATTACK_RANGED_2;

    return SDL_POINTER_ATTACK_RANGED_1;
}

static int pointer_attack_throwing_display_attack(const object_type* o_ptr)
{
    const object_type* wield = &inventory[INVEN_WIELD];
    int attack = p_ptr->skill_use[S_MEL] + o_ptr->att;

    attack -= wield->att;
    attack -= axe_bonus(wield);
    attack -= polearm_bonus(wield);
    attack += axe_bonus(o_ptr);
    attack += polearm_bonus(o_ptr);

    if (p_ptr->active_ability[S_MEL][MEL_THROWING]
        || object_grants_ability(o_ptr, S_MEL, MEL_THROWING))
    {
        attack += 1;
    }

    return attack;
}

static bool pointer_attack_ranged_display_stats(int* attack, int* dd, int* ds,
    bool* throwing)
{
    int mode = pointer_attack_ranged_display_mode();
    const object_type* ammo = pointer_attack_ranged_ammo_for_mode(mode);

    if (attack)
        *attack = p_ptr->skill_use[S_ARC];
    if (dd)
        *dd = p_ptr->add;
    if (ds)
        *ds = p_ptr->ads;
    if (throwing)
        *throwing = false;

    if (ammo && ammo->k_idx)
    {
        u32b f1 = 0;
        u32b f2 = 0;
        u32b f3 = 0;
        u32b f4 = 0;

        object_flags4(ammo, &f1, &f2, &f3, &f4);
        if (player_can_treat_as_throwing_flags(ammo, f3))
        {
            int throw_ds = strength_modified_ds(ammo, 0);

            if (attack)
                *attack = pointer_attack_throwing_display_attack(ammo);
            if (dd)
                *dd = ammo->dd;
            if (ds)
                *ds = MAX(0, throw_ds);
            if (throwing)
                *throwing = true;
            return true;
        }

        if ((&inventory[INVEN_BOW])->k_idx && ammo->tval == TV_ARROW)
        {
            if (attack)
                *attack += ammo->att;
            return true;
        }
    }

    return (&inventory[INVEN_BOW])->k_idx ? true : false;
}

static bool pointer_attack_melee_has_offhand(void)
{
    return ((&inventory[INVEN_ARM])->k_idx)
        && ((&inventory[INVEN_ARM])->tval != TV_SHIELD);
}

static bool left_panel_object_icon_is_usable(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx;
}

static bool left_panel_icon_is_tile(byte attr, char icon)
{
    return (attr & TILE_FLAG) && (((byte)icon) & TILE_FLAG);
}

static void left_panel_fill_selected_span(int row, int col, int width,
    byte selected_attr)
{
    char fill[LEFT_PANEL_CONTENT_WID + 1];

    if (!Term || !left_panel_attr_is_selected(selected_attr) || width <= 0)
        return;
    if (row < 0 || row >= Term->hgt || col >= Term->wid)
        return;
    if (col < 0)
    {
        width += col;
        col = 0;
    }
    if (width <= 0)
        return;
    if (width > LEFT_PANEL_CONTENT_WID)
        width = LEFT_PANEL_CONTENT_WID;
    if (col + width > Term->wid)
        width = Term->wid - col;
    if (width <= 0)
        return;

    memset(fill, ' ', (size_t)width);
    fill[width] = '\0';
    Term_putstr(col, row, width, selected_attr, fill);
}

static void left_panel_put_space_with_bg(int row, int col, byte bg_attr)
{
    if (bg_attr)
        Term_putch(col, row, bg_attr, ' ');
    else
        Term_putch(col, row, TERM_WHITE, ' ');
}

static void left_panel_put_icon_cell(byte attr, char icon, int row, int col,
    byte bg_attr)
{
    if (bg_attr && left_panel_icon_is_tile(attr, icon))
        Term_queue_char(col, row, attr, icon, bg_attr, ' ');
    else if (bg_attr)
        Term_putch(col, row, bg_attr, icon);
    else
        Term_putch(col, row, attr, icon);
}

static void left_panel_put_bigtile_continuation(byte attr, char icon, int row,
    int col, byte bg_attr)
{
    if (bg_attr && left_panel_icon_is_tile(attr, icon))
        Term_queue_char(col, row, 255, -1, bg_attr, ' ');
    else if (bg_attr)
        left_panel_put_space_with_bg(row, col, bg_attr);
    else if (use_bigtile)
        Term_putch(col, row, 255, -1);
}

static int left_panel_put_object_icon(const object_type* o_ptr, int row,
    int col, bool trailing_space, byte bg_attr)
{
    byte attr;
    char icon;
    int start_col = col;
    int limit_col = COL_MEL + LEFT_PANEL_CONTENT_WID;

    if (!Term || !left_panel_object_icon_is_usable(o_ptr))
        return 0;
    if (row < 0 || row >= Term->hgt || col < COL_MEL || col >= limit_col)
        return 0;

    attr = object_attr(o_ptr);
    icon = object_char(o_ptr);

    left_panel_put_icon_cell(attr, icon, row, col, bg_attr);
    col++;
    if (col < limit_col)
    {
        if (use_bigtile)
            left_panel_put_bigtile_continuation(attr, icon, row, col, bg_attr);
        else
            left_panel_put_icon_cell(attr, icon, row, col, bg_attr);
        col++;
    }
    if (trailing_space && col < limit_col)
    {
        left_panel_put_space_with_bg(row, col, bg_attr);
        col++;
    }

    return col - start_col;
}

static const object_type* left_panel_melee_display_object(void)
{
    return inventory[INVEN_WIELD].k_idx ? &inventory[INVEN_WIELD] : NULL;
}

static const object_type* left_panel_offhand_display_object(void)
{
    return pointer_attack_melee_has_offhand() ? &inventory[INVEN_ARM] : NULL;
}

static const object_type* left_panel_ranged_display_object(bool throwing)
{
    int mode = pointer_attack_ranged_display_mode();
    const object_type* ammo = pointer_attack_ranged_ammo_for_mode(mode);

    if (throwing && ammo && ammo->k_idx)
        return ammo;
    if (inventory[INVEN_BOW].k_idx)
        return &inventory[INVEN_BOW];
    if (ammo && ammo->k_idx)
        return ammo;

    return NULL;
}

static const object_type* left_panel_armour_display_object(void)
{
    static const int slots[] = {
        INVEN_BODY, INVEN_ARM, INVEN_OUTER, INVEN_HEAD, INVEN_HANDS, INVEN_FEET
    };

    for (int i = 0; i < (int)N_ELEMENTS(slots); i++)
    {
        object_type* o_ptr = &inventory[slots[i]];

        if (!o_ptr->k_idx)
            continue;
        if (slots[i] == INVEN_ARM && o_ptr->tval != TV_SHIELD)
            continue;

        return o_ptr;
    }

    return NULL;
}

static void left_panel_format_evasion_value(char* buf, size_t buflen)
{
    bool block;

    if (!buf || buflen == 0)
        return;
    buf[0] = '\0';
    if (!p_ptr)
        return;

    block = p_ptr->active_ability[S_EVN][EVN_BLOCKING];
    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = false;
    strnfmt(buf, buflen, "[%+d,%d-%d]", p_ptr->skill_use[S_EVN],
        p_min(GF_HURT, true), p_max(GF_HURT, true));
    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = block;
}

static void prt_pointer_attack_value_row_icon(cptr label, byte label_attr,
    byte value_attr, cptr value, int row, const object_type* icon_obj,
    byte icon_bg_attr)
{
    int value_len;
    int value_col;
    bool can_draw_icon;
    byte row_bg_attr = icon_bg_attr;

    if (!value || !value[0])
        return;

    if (!row_bg_attr)
        row_bg_attr = left_panel_selected_bg_from_attr(value_attr);
    if (!row_bg_attr)
        row_bg_attr = left_panel_selected_bg_from_attr(label_attr);
    if (row_bg_attr)
        left_panel_fill_selected_span(row, COL_MEL, LEFT_PANEL_CONTENT_WID,
            row_bg_attr);

    value_len = strlen(value);
    can_draw_icon = left_panel_object_icon_is_usable(icon_obj)
        && value_len <= LEFT_PANEL_CONTENT_WID - 2;
    if (can_draw_icon)
    {
        int icon_cols = left_panel_put_object_icon(icon_obj, row, COL_MEL,
            false, row_bg_attr);

        if (icon_cols > 0)
        {
            int label_col = COL_MEL + icon_cols;

            value_col = COL_MEL + LEFT_PANEL_CONTENT_WID - value_len;
            if (value_col < label_col)
                value_col = label_col;
            if (label && label[0]
                && ((int)strlen(label) + 1) <= value_col - label_col)
            {
                c_put_str(label_attr, label, row, label_col);
            }
            else if (label && label[0] && value_col > label_col + 1)
            {
                char short_label[2];

                short_label[0] = label[0];
                short_label[1] = '\0';
                c_put_str(label_attr, short_label, row, label_col);
            }
            c_put_str(value_attr, value, row, value_col);
            return;
        }
    }

    value_col = COL_MEL + LEFT_PANEL_CONTENT_WID - value_len;
    if (value_col < COL_MEL)
        value_col = COL_MEL;

    if (label && label[0] && ((int)strlen(label) + 1) <= value_col - COL_MEL)
    {
        c_put_str(label_attr, label, row, COL_MEL);
    }
    else if (label && label[0] && value_col > COL_MEL + 1)
    {
        char short_label[2];

        short_label[0] = label[0];
        short_label[1] = '\0';
        c_put_str(label_attr, short_label, row, COL_MEL);
    }

    c_put_str(value_attr, value, row, value_col);
}

/*
 * Print character info at given row, column in the left panel field.
 */
static void prt_field(cptr info, int row, int col, byte attr)
{
    /* Dump the full field to clear stale text. */
    Term_erase(col, row, LEFT_PANEL_CONTENT_WID);

    /* Dump the info itself */
    c_put_str(attr, info, row, col);
}

enum { PLAYER_PANEL_NAME_MAX = LEFT_PANEL_CONTENT_WID };

/*
 * Choose the longest whole-word player name prefix that fits the sidebar.
 * Falls back to a clipped first word if even the first word is too long.
 */
static void get_player_panel_name(char* buf, size_t buf_len)
{
    const char* name = op_ptr->full_name;
    const char* cursor;
    const char* fit_end = NULL;

    if (!buf || (buf_len == 0))
        return;

    buf[0] = '\0';

    if (!name)
        return;

    while (*name && isspace((unsigned char)*name))
        name++;

    if (!name[0])
        return;

    if (strlen(name) <= PLAYER_PANEL_NAME_MAX)
    {
        SDL_strlcpy(buf, name, buf_len);
        return;
    }

    cursor = name;
    while (*cursor)
    {
        while (*cursor && !isspace((unsigned char)*cursor))
            cursor++;

        if ((size_t)(cursor - name) <= PLAYER_PANEL_NAME_MAX)
            fit_end = cursor;
        else
            break;

        while (*cursor && isspace((unsigned char)*cursor))
            cursor++;
    }

    if (fit_end)
    {
        size_t copy_len = (size_t)(fit_end - name);

        if (copy_len >= buf_len)
            copy_len = buf_len - 1;

        memcpy(buf, name, copy_len);
        buf[copy_len] = '\0';
        return;
    }

    SDL_strlcpy(buf, name, buf_len);
    buf[MIN(buf_len - 1, (size_t)PLAYER_PANEL_NAME_MAX)] = '\0';
}

void prt_player_name(void)
{
    char panel_name[PLAYER_PANEL_NAME_MAX + 1];

    get_player_panel_name(panel_name, sizeof(panel_name));
    prt_field(panel_name, ROW_NAME, COL_NAME,
        panel_touch_zone_attr(SDL_PANEL_CLICK_CHARACTER, ROW_NAME,
            TERM_L_BLUE));
}

/*
 * Print character stat in given row, column
 */
void prt_stat(int stat)
{
    char tmp[32];
    char trimmed_label[32];
    const char* stat_label;
    int row = ROW_STAT + stat;
    int click_action = (stat == A_GRA)
        ? SDL_PANEL_CLICK_SMITHING
        : SDL_PANEL_CLICK_ABILITIES;
    byte label_attr = panel_touch_zone_attr(click_action, row, TERM_WHITE);
    byte value_attr;
    int len;

    /* Clear the line */
    Term_erase(COL_STAT, row, LEFT_PANEL_CONTENT_WID);

    /* Get the stat name */
    if (p_ptr->stat_drain[stat] < 0)
    {
        stat_label = stat_names_reduced[stat];
    }
    else
    {
        stat_label = stat_names[stat];
    }

    /* Trim trailing spaces for story font rendering */
    SDL_strlcpy(trimmed_label, stat_label, sizeof(trimmed_label));
    len = strlen(trimmed_label);
    while (len > 0 && trimmed_label[len-1] == ' ') {
        trimmed_label[--len] = '\0';
    }

    log_trace("prt_stat: Rendering stat %d ('%s' trimmed to '%s')", stat, stat_label, trimmed_label);

    /* Display stat name with story font */
    log_trace("prt_stat: Enabling story font for stat label");
    sdl_story_font_enable();

    log_trace("prt_stat: Calling c_put_str('%s', %d, %d)", trimmed_label, row, 0);
    c_put_str(label_attr, trimmed_label, row, 0);

    int cursor_x, cursor_y;
    Term_locate(&cursor_x, &cursor_y);
    log_trace("prt_stat: After put_str, cursor at (%d, %d)", cursor_x, cursor_y);
    log_trace("prt_stat: Disabling story font");
    sdl_story_font_disable();

    /* Display stat value with monospace font */
    cnv_stat(p_ptr->stat_use[stat], tmp);
    len = strlen(tmp);
    log_trace("prt_stat: Calling c_put_str('%s', %d, %d) for stat value", tmp, row, COL_STAT + 12 - len);
    if (p_ptr->stat_drain[stat] < 0)
    {
        value_attr = TERM_YELLOW;
    }
    else
    {
        value_attr = TERM_L_GREEN;
    }
    value_attr = panel_touch_zone_attr(click_action, row, value_attr);
    c_put_str(value_attr, tmp, row, COL_STAT + 12 - len);

    /* Indicate temporary modifiers - clear first, then conditionally display */
    if ((stat == A_STR) && p_ptr->tmp_str)
        c_put_str(label_attr, "*", row, 3);
    else if ((stat == A_DEX) && p_ptr->tmp_dex)
        c_put_str(label_attr, "*", row, 3);
    else if ((stat == A_CON) && p_ptr->tmp_con)
        c_put_str(label_attr, "*", row, 3);
    else if ((stat == A_GRA) && p_ptr->tmp_gra)
        c_put_str(label_attr, "*", row, 3);
}

/*
 * Display the experience
 */
void prt_exp(void)
{
    char out_val[32];
    byte attr;
    byte label_attr;
    int len;

    attr = panel_touch_zone_attr(SDL_PANEL_CLICK_SKILL_DISTRIBUTION,
        ROW_EXP, TERM_L_GREEN);
    label_attr = panel_touch_zone_attr(SDL_PANEL_CLICK_SKILL_DISTRIBUTION,
        ROW_EXP, TERM_WHITE);

    /* Clear the whole field so shorter values don't leave stale characters */
    Term_erase(COL_EXP, ROW_EXP, 12);

    sdl_story_font_enable();

    /*Print experience label*/
    c_put_str(label_attr, "Exp", ROW_EXP, 0);

    sdl_story_font_disable();

    comma_number(out_val, p_ptr->new_exp);
    len = strlen(out_val);

    c_put_str(attr, out_val, ROW_EXP, COL_EXP + 12 - len);
}

/*
 * Prints current mel
 */
void prt_mel(void)
{
    char buf[32];
    bool has_offhand = pointer_attack_melee_has_offhand();
    bool can_use_offhand_row = (ROW_MEL - 1) != ROW_LIGHT;
    int main_row = ROW_MEL;
    byte melee_bg_attr = pointer_attack_panel_bg_attr(SDL_POINTER_ATTACK_MELEE);

    if (has_offhand && can_use_offhand_row)
        main_row = ROW_MEL - 1;

    /*
     * Clear the secondary melee row only when it is not reserved for the light
     * display. In compact layouts ROW_MEL - 1 is ROW_LIGHT, so clearing both
     * rows after prt_light() erases the torch indicator.
     */
    if (can_use_offhand_row)
        Term_erase(COL_MEL, ROW_MEL - 1, 12);
    Term_erase(COL_MEL, ROW_MEL, 12);

    /* Melee attacks */
    int meleeColour
        = p_ptr->active_ability[S_MEL][MEL_SMITE] ? TERM_L_RED : TERM_L_WHITE;
    meleeColour = pointer_attack_panel_attr(SDL_POINTER_ATTACK_MELEE,
        meleeColour);
    strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds);
    prt_pointer_attack_value_row_icon(
        p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] ? "M2x" : "Mel",
        pointer_attack_panel_attr(SDL_POINTER_ATTACK_MELEE, TERM_L_WHITE),
        meleeColour, buf, main_row, left_panel_melee_display_object(),
        melee_bg_attr);

    if (has_offhand && can_use_offhand_row)
    {
        strnfmt(buf, sizeof(buf), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod, p_ptr->mdd2,
            p_ptr->mds2);
        prt_pointer_attack_value_row_icon("Off",
            pointer_attack_panel_attr(SDL_POINTER_ATTACK_MELEE, TERM_L_WHITE),
            pointer_attack_panel_attr(SDL_POINTER_ATTACK_MELEE, TERM_L_WHITE),
            buf, ROW_MEL, left_panel_offhand_display_object(), melee_bg_attr);
    }
}

/*
 * Prints current arc
 */
void prt_arc(void)
{
    char buf[32];
    int attack = 0;
    int dd = 0;
    int ds = 0;
    bool throwing = false;

    /* Clear the line so shorter values don't leave stale characters */
    Term_erase(COL_ARC, ROW_ARC, 12);

    /* Range attacks */
    if (pointer_attack_ranged_display_stats(&attack, &dd, &ds, &throwing))
    {
        byte arc_attr = pointer_attack_ranged_panel_attr(TERM_UMBER);
        byte arc_bg_attr = pointer_attack_ranged_panel_bg_attr();
        cptr label = throwing ? "Thr" : "Arc";
        const object_type* icon_obj = left_panel_ranged_display_object(throwing);
        bool draw_icon = left_panel_object_icon_is_usable(icon_obj);

        if (!throwing && p_ptr->active_ability[S_ARC][ARC_DEADLY_HAIL]
            && p_ptr->killed_enemy_with_arrow)
        {
            char full_value[32];
            bool draw_special_icon;

            strnfmt(full_value, sizeof(full_value), "(%+d,%dd%d)", attack,
                2 * dd, ds);
            draw_special_icon = draw_icon
                && (int)strlen(full_value) <= LEFT_PANEL_CONTENT_WID - 3;

            if (arc_bg_attr)
                left_panel_fill_selected_span(ROW_ARC, COL_ARC,
                    LEFT_PANEL_CONTENT_WID, arc_bg_attr);
            if (draw_special_icon)
                left_panel_put_object_icon(icon_obj, ROW_ARC, COL_ARC, true,
                    arc_bg_attr);
            else
                c_put_str(pointer_attack_ranged_panel_attr(TERM_L_WHITE), label,
                    ROW_ARC, COL_ARC);
            strnfmt(buf, sizeof(buf), ")");
            c_put_str(arc_attr, buf, ROW_ARC, COL_ARC + 12 - strlen(buf));
            strnfmt(buf, sizeof(buf), "%dd%d", 2 * dd, ds);
            c_put_str(pointer_attack_ranged_panel_attr(TERM_RED), buf,
                ROW_ARC, COL_ARC + 11 - strlen(buf));
            strnfmt(buf, sizeof(buf), "(%+d,", attack);
            if (ds > 9)
                c_put_str(arc_attr, buf, ROW_ARC,
                    COL_ARC + 7 - strlen(buf));
            else
                c_put_str(arc_attr, buf, ROW_ARC,
                    COL_ARC + 8 - strlen(buf));
        }
        else
        {
            strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", attack, dd, ds);
            prt_pointer_attack_value_row_icon(label,
                pointer_attack_ranged_panel_attr(TERM_L_WHITE), arc_attr, buf,
                ROW_ARC, icon_obj, arc_bg_attr);
        }
    }

}

/*
 * Prints current quiver status (current/max for both quivers)
 * Right-aligned to 12 character width, like other stats
 * Same type: icon in middle between counts
 * Different: icon before each count
 */
void prt_quiver(void)
{
    char buf1[16];
    char buf2[16];
    object_type* q1_ptr = &inventory[INVEN_QUIVER1];
    object_type* q2_ptr = &inventory[INVEN_QUIVER2];
    int q1_current = 0;
    int q1_max = 0;
    int q2_current = 0;
    int q2_max = 0;
    bool same_type = false;
    int total_width;
    int start_col;
    byte q1_text_attr = pointer_attack_quiver_panel_attr(
        SDL_POINTER_ATTACK_RANGED_1, TERM_L_WHITE);
    byte q2_text_attr = pointer_attack_quiver_panel_attr(
        SDL_POINTER_ATTACK_RANGED_2, TERM_L_WHITE);
    byte q1_bg_attr = pointer_attack_quiver_panel_bg_attr(
        SDL_POINTER_ATTACK_RANGED_1);
    byte q2_bg_attr = pointer_attack_quiver_panel_bg_attr(
        SDL_POINTER_ATTACK_RANGED_2);

    for (int i = 0; i < 2; i++)
    {
        g_left_panel_quiver_attack_modes[i] = SDL_POINTER_ATTACK_NONE;
        g_left_panel_quiver_attack_start_cols[i] = 0;
        g_left_panel_quiver_attack_end_cols[i] = 0;
    }

    /* Clear the entire line (12 characters) */
    Term_erase(COL_QUIVER, ROW_QUIVER, 12);

    /* Get quiver 1 info */
    if (q1_ptr->k_idx)
    {
        q1_current = q1_ptr->number;
        q1_max = object_stack_limit(q1_ptr);
    }

    /* Get quiver 2 info */
    if (q2_ptr->k_idx)
    {
        q2_current = q2_ptr->number;
        q2_max = object_stack_limit(q2_ptr);
    }

    /* Check if both quivers have the same item type */
    if (q1_ptr->k_idx && q2_ptr->k_idx)
    {
        if (q1_ptr->tval == q2_ptr->tval && q1_ptr->sval == q2_ptr->sval)
        {
            same_type = true;
        }
    }

    /* Format the count strings */
    strnfmt(buf1, sizeof(buf1), "%d/%d", q1_current, q1_max);
    strnfmt(buf2, sizeof(buf2), "%d/%d", q2_current, q2_max);

    /* Calculate total width */
    if (same_type)
    {
        /* Layout: "11/48[->][->]7/7" */
        total_width = strlen(buf1) + (use_bigtile ? 2 : 2) + strlen(buf2);
    }
    else
    {
        /* Layout: "[|][|]11/48[/][/]7/7" */
        total_width = 0;
        if (q1_ptr->k_idx)
            total_width += (use_bigtile ? 2 : 2) + strlen(buf1);
        if (q2_ptr->k_idx)
            total_width += (use_bigtile ? 2 : 2) + strlen(buf2);
    }

    /* Right-align: start at column that makes it end at column 11 */
    start_col = COL_QUIVER + 12 - total_width;
    if (start_col < COL_QUIVER) start_col = COL_QUIVER;

    int col = start_col;

    if (same_type)
    {
        /* Same type: counts with icon in middle */
        byte shared_icon_bg_attr = q1_bg_attr ? q1_bg_attr : q2_bg_attr;
        int q1_start;
        int q2_start;

        /* Q1 count */
        q1_start = col;
        Term_putstr(col, ROW_QUIVER, -1, q1_text_attr, buf1);
        col += strlen(buf1);
        g_left_panel_quiver_attack_modes[0] = SDL_POINTER_ATTACK_RANGED_1;
        g_left_panel_quiver_attack_start_cols[0] = (byte)q1_start;
        g_left_panel_quiver_attack_end_cols[0] = (byte)col;

        /* Icon in middle */
        col += left_panel_put_object_icon(q1_ptr, ROW_QUIVER, col, false,
            shared_icon_bg_attr);

        /* Q2 count */
        q2_start = col;
        Term_putstr(col, ROW_QUIVER, -1, q2_text_attr, buf2);
        col += strlen(buf2);
        g_left_panel_quiver_attack_modes[1] = SDL_POINTER_ATTACK_RANGED_2;
        g_left_panel_quiver_attack_start_cols[1] = (byte)q2_start;
        g_left_panel_quiver_attack_end_cols[1] = (byte)col;
    }
    else
    {
        /* Different types: icon before each count */
        if (q1_ptr->k_idx)
        {
            /* Q1: "[icon][icon]cur/max" */
            int q1_start;

            col += left_panel_put_object_icon(q1_ptr, ROW_QUIVER, col, false,
                q1_bg_attr);

            q1_start = col;
            Term_putstr(col, ROW_QUIVER, -1, q1_text_attr, buf1);
            col += strlen(buf1);
            g_left_panel_quiver_attack_modes[0] = SDL_POINTER_ATTACK_RANGED_1;
            g_left_panel_quiver_attack_start_cols[0] = (byte)q1_start;
            g_left_panel_quiver_attack_end_cols[0] = (byte)col;
        }

        if (q2_ptr->k_idx)
        {
            /* Q2: "[icon][icon]cur/max" */
            int q2_start;

            col += left_panel_put_object_icon(q2_ptr, ROW_QUIVER, col, false,
                q2_bg_attr);

            q2_start = col;
            Term_putstr(col, ROW_QUIVER, -1, q2_text_attr, buf2);
            col += strlen(buf2);
            g_left_panel_quiver_attack_modes[1] = SDL_POINTER_ATTACK_RANGED_2;
            g_left_panel_quiver_attack_start_cols[1] = (byte)q2_start;
            g_left_panel_quiver_attack_end_cols[1] = (byte)col;
        }
    }
}

/*
 * Prints current evn
 */
void prt_evn(void)
{
    char buf[32];
    byte attr = panel_touch_zone_attr(SDL_PANEL_CLICK_INVENTORY, ROW_EVN,
        TERM_SLATE);

    /* Clear the line so shorter values don't leave stale characters */
    Term_erase(COL_EVN, ROW_EVN, 12);

    /* Total Armor */
    left_panel_format_evasion_value(buf, sizeof(buf));
    prt_pointer_attack_value_row_icon(NULL, attr, attr, buf, ROW_EVN,
        left_panel_armour_display_object(), 0);
}

/*
 * Prints Cur/Max hit points
 */
void prt_hp(void)
{
    char tmp[32];
    byte color;
    byte label_attr = panel_touch_zone_attr(SDL_PANEL_CLICK_CHARACTER,
        ROW_HP, TERM_WHITE);
    byte max_attr = panel_touch_zone_attr(SDL_PANEL_CLICK_CHARACTER,
        ROW_HP, TERM_L_GREEN);

    /* Clear the line */
    Term_erase(COL_HP, ROW_HP, LEFT_PANEL_CONTENT_WID);

    sdl_story_font_enable();

    if (p_ptr->mhp >= 100)
    {
        c_put_str(label_attr, "Hth", ROW_HP, COL_HP);
    }
    else
    {
        c_put_str(label_attr, "Health", ROW_HP, COL_HP);
    }

    sdl_story_font_disable();

    /* Get color for current HP */
    color = health_attr(p_ptr->chp, p_ptr->mhp);
    color = panel_touch_zone_attr(SDL_PANEL_CLICK_CHARACTER, ROW_HP, color);

    /* Calculate lengths for left (current) and right (max) parts */
    int chp_len = sprintf(tmp, "%d", p_ptr->chp);
    int mhp_len = sprintf(tmp, "%d", p_ptr->mhp);
    int total_len = chp_len + 1 + mhp_len; /* +1 for the slash */

    /* Print current HP in color */
    sprintf(tmp, "%d", p_ptr->chp);
    c_put_str(color, tmp, ROW_HP, COL_HP + 12 - total_len);

    /* Print slash in green */
    c_put_str(max_attr, "/", ROW_HP, COL_HP + 12 - total_len + chp_len);

    /* Print max HP in green */
    sprintf(tmp, "%d", p_ptr->mhp);
    c_put_str(max_attr, tmp, ROW_HP, COL_HP + 12 - total_len + chp_len + 1);
}

/*
 * Prints a small, monospace graphical health bar under the name.
 * Uses 'x' characters up to 12 symbols to represent current HP proportionally.
 * Colour matches health_attr() (green/yellow/red, etc).
 */
void prt_char_health_graphic(void)
{
    char bar[13]; /* 12 symbols + NUL */
    int max_symbols = 12;
    int filled = 0;
    byte color;

    /* Clear the line first (12 chars) */
    c_put_str(TERM_WHITE, "            ", ROW_NAME + 1, COL_NAME);

    /* Defensive: avoid division by zero */
    if (p_ptr->mhp <= 0)
        return;

    /* Scale current HP to number of symbols (ceiling) */
    filled = (max_symbols * p_ptr->chp + p_ptr->mhp - 1) / p_ptr->mhp;
    if (filled < 0)
        filled = 0;
    if (filled > max_symbols)
        filled = max_symbols;

    /* Build the bar using 'x' for filled and spaces for remainder */
    for (int i = 0; i < filled; i++)
        bar[i] = 'x';
    for (int i = filled; i < max_symbols; i++)
        bar[i] = ' ';
    bar[max_symbols] = '\0';

    /* Colour according to health */
    color = health_attr(p_ptr->chp, p_ptr->mhp);
    color = panel_touch_zone_attr(SDL_PANEL_CLICK_CHARACTER, ROW_NAME + 1,
        color);

    /* Print using a monospace field (no story font) */
    c_put_str(color, format("%12s", bar), ROW_NAME + 1, COL_NAME);
}

/*
 * Build the compact monster health bar used by look/target displays.
 */
int monster_health_bar_text(
    const monster_type* m_ptr, char* buf, size_t buflen, int max_symbols)
{
    int len;
    int writable;

    if (buflen == 0)
        return 0;

    buf[0] = '\0';

    if (!m_ptr || (max_symbols <= 0) || (m_ptr->maxhp <= 0))
        return 0;

    len = (max_symbols * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;
    if (len < 0)
        len = 0;
    if (len > max_symbols)
        len = max_symbols;

    writable = MIN(len, (int)buflen - 1);

    for (int i = 0; i < writable; i++)
    {
        if (m_ptr->confused && m_ptr->stunned)
            buf[i] = (i % 2) ? 's' : 'c';
        else if (m_ptr->confused)
            buf[i] = 'c';
        else if (m_ptr->stunned)
            buf[i] = 's';
        else
            buf[i] = '*';
    }
    buf[writable] = '\0';

    return writable;
}

static enum pane_placement hidden_left_panel_placement(void)
{
    int count = get_pane_config_count();

    for (int i = 0; i < count; i++)
    {
        if ((enum pane_type)get_sdl_pane_type(i) != PANE_LEFT_PANEL)
            continue;

        return (enum pane_placement)get_sdl_pane_where(i);
    }

    return PLACE_TOP_LEFT;
}

bool hidden_left_panel_visible(void)
{
    return false;
}

static bool hidden_left_panel_placement_is_right(enum pane_placement where)
{
    return where == PLACE_TOP_RIGHT || where == PLACE_RIGHT_CENTER
        || where == PLACE_BOTTOM_RIGHT;
}

static bool hidden_left_panel_placement_is_horizontal_center(
    enum pane_placement where)
{
    return where == PLACE_TOP_CENTER || where == PLACE_BOTTOM_CENTER;
}

static bool hidden_left_panel_placement_is_bottom(enum pane_placement where)
{
    return where == PLACE_BOTTOM_LEFT || where == PLACE_BOTTOM_CENTER
        || where == PLACE_BOTTOM_RIGHT;
}

static bool hidden_left_panel_placement_is_vertical_center(
    enum pane_placement where)
{
    return where == PLACE_LEFT_CENTER || where == PLACE_RIGHT_CENTER;
}

static int hidden_left_panel_start_row_for_count(int line_count)
{
    enum pane_placement where = hidden_left_panel_placement();
    int available_rows;
    int start;

    if (!Term || line_count <= 0)
        return ROW_NAME;

    if (line_count > Term->hgt)
        line_count = Term->hgt;

    if (hidden_left_panel_placement_is_bottom(where))
        return MAX(0, Term->hgt - line_count);

    if (hidden_left_panel_placement_is_vertical_center(where))
    {
        available_rows = Term->hgt - ROW_NAME;
        if (available_rows < line_count)
            available_rows = line_count;
        start = ROW_NAME + (available_rows - line_count) / 2;
        if (start + line_count > Term->hgt)
            start = Term->hgt - line_count;
        if (start < 0)
            start = 0;
        return start;
    }

    return ROW_NAME;
}

static int hidden_left_panel_start_col_for_width(int width)
{
    enum pane_placement where = hidden_left_panel_placement();
    int start = 0;

    if (!Term)
        return 0;
    if (width < 0)
        width = 0;
    if (width > Term->wid)
        width = Term->wid;

    if (hidden_left_panel_placement_is_right(where))
        start = Term->wid - width;
    else if (hidden_left_panel_placement_is_horizontal_center(where))
        start = (Term->wid - width) / 2;

    if (start < 0)
        start = 0;
    if (start > Term->wid)
        start = Term->wid;
    return start;
}

static bool current_light_status(bool* infinite, long* fuel, byte* fuel_attr,
                                 byte* icon_attr, char* icon)
{
    object_type* o_ptr = &inventory[INVEN_LITE];
    bool light_is_infinite = false;
    long light_fuel = 0;
    byte attr = TERM_L_WHITE;

    if (!o_ptr->k_idx)
        return false;

    if (icon_attr)
        *icon_attr = object_attr(o_ptr);
    if (icon)
        *icon = object_char(o_ptr);

    if (o_ptr->tval == TV_LIGHT)
    {
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_LANTERN:
        case SV_LIGHT_MALLORN:
            light_fuel = player_light_fuel(o_ptr);
            break;
        default:
            light_is_infinite = true;
            break;
        }
    }
    else
    {
        u32b f1, f2, f3;
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & TR2_LIGHT)
            light_is_infinite = true;
    }

    if (light_is_infinite)
    {
        attr = TERM_L_GREEN;
    }
    else
    {
        if (light_fuel < 0)
            light_fuel = 0;

        if (light_fuel == 0)
            attr = TERM_RED;
        else if (light_fuel <= player_light_sputter_threshold(o_ptr))
            attr = TERM_ORANGE;
    }

    if (infinite)
        *infinite = light_is_infinite;
    if (fuel)
        *fuel = light_fuel;
    if (fuel_attr)
        *fuel_attr = attr;

    return true;
}

void prt_light(void)
{
    int icon_col = COL_LIGHT;
    bool infinite = false;
    long fuel = 0;
    byte fuel_attr = TERM_L_WHITE;
    byte attr;
    char icon;

    /* Clear the line */
    Term_erase(icon_col, ROW_LIGHT, LEFT_PANEL_CONTENT_WID);

    /* Nothing equipped */
    if (!current_light_status(&infinite, &fuel, &fuel_attr, &attr, &icon))
    {
        c_put_str(panel_touch_zone_attr(SDL_PANEL_CLICK_SUPPLIES_LIGHTS,
            ROW_LIGHT, TERM_SLATE), "Torch", ROW_LIGHT, icon_col);
        return;
    }

    fuel_attr = panel_touch_zone_attr(SDL_PANEL_CLICK_SUPPLIES_LIGHTS,
        ROW_LIGHT, fuel_attr);

    /* Draw the icon (supporting bigtile visuals) */
    Term_putch(icon_col, ROW_LIGHT, attr, icon);
    if (use_bigtile)
    {
        Term_putch(icon_col + 1, ROW_LIGHT, 255, -1);
    }
    else
    {
        Term_putch(icon_col + 1, ROW_LIGHT, attr, icon);
    }

    Term_putch(icon_col + 2, ROW_LIGHT, TERM_WHITE, ' ');

    char buf[16];

    if (infinite)
    {
        SDL_strlcpy(buf, "inf", sizeof(buf));
    }
    else
    {
        strnfmt(buf, sizeof(buf), "%ld", fuel);
    }

    c_put_str(fuel_attr, buf, ROW_LIGHT, icon_col + 12 - strlen(buf));
}

/*
 * Prints player's max/cur spell points
 */
void prt_sp(void)
{
    char tmp[32];
    byte color;
    byte label_attr = panel_touch_zone_attr(SDL_PANEL_CLICK_SONG, ROW_SP,
        TERM_WHITE);
    byte max_attr = panel_touch_zone_attr(SDL_PANEL_CLICK_SONG, ROW_SP,
        TERM_L_GREEN);
    int len;

    /* Clear the line */
    Term_erase(COL_SP, ROW_SP, LEFT_PANEL_CONTENT_WID);

    sdl_story_font_enable();

    if (p_ptr->msp >= 100)
        c_put_str(label_attr, "Vce", ROW_SP, COL_SP);
    else
        c_put_str(label_attr, "Voice", ROW_SP, COL_SP);

    sdl_story_font_disable();

    len = sprintf(tmp, "%d:%d", p_ptr->csp, p_ptr->msp);

    c_put_str(max_attr, tmp, ROW_SP, COL_SP + 12 - len);

    /* Done? */
    if (p_ptr->csp >= p_ptr->msp)
        return;

    if (p_ptr->csp > (p_ptr->msp * op_ptr->hitpoint_warn) / 10)
    {
        color = TERM_YELLOW;
    }
    else
    {
        color = TERM_RED;
    }

    /* Show current mana using another color */
    sprintf(tmp, "%d", p_ptr->csp);

    color = panel_touch_zone_attr(SDL_PANEL_CLICK_SONG, ROW_SP, color);
    c_put_str(color, tmp, ROW_SP, COL_SP + 12 - len);
}

static void hidden_left_panel_add_line_mode(hidden_overlay_line* lines,
    int* count, int max_lines, byte attr, cptr text, int pointer_attack_mode)
{
    if (!lines || !count || !text || !text[0])
        return;
    if (*count >= max_lines)
        return;

    SDL_strlcpy(lines[*count].text, text, sizeof(lines[*count].text));
    SDL_strlcpy(lines[*count].short_text, text,
        sizeof(lines[*count].short_text));
    lines[*count].attr = attr;
    lines[*count].has_icon = false;
    lines[*count].icon_attr = TERM_WHITE;
    lines[*count].icon_char = ' ';
    lines[*count].pointer_attack_mode = (byte)pointer_attack_mode;
    lines[*count].click_action = SDL_PANEL_CLICK_NONE;
    (*count)++;
}

static void hidden_left_panel_add_line(hidden_overlay_line* lines, int* count,
                                       int max_lines, byte attr, cptr text)
{
    hidden_left_panel_add_line_mode(lines, count, max_lines, attr, text,
        SDL_POINTER_ATTACK_NONE);
}

static void hidden_left_panel_add_icon_line_mode(hidden_overlay_line* lines,
    int* count, int max_lines, byte attr, cptr text, cptr short_text,
    byte icon_attr, char icon_char, int pointer_attack_mode)
{
    if (!lines || !count || !text || !text[0])
        return;
    if (*count >= max_lines)
        return;

    SDL_strlcpy(lines[*count].text, text, sizeof(lines[*count].text));
    if (short_text && short_text[0])
        SDL_strlcpy(lines[*count].short_text, short_text,
            sizeof(lines[*count].short_text));
    else
        SDL_strlcpy(lines[*count].short_text, text,
            sizeof(lines[*count].short_text));
    lines[*count].attr = attr;
    lines[*count].has_icon = true;
    lines[*count].icon_attr = icon_attr;
    lines[*count].icon_char = icon_char;
    lines[*count].pointer_attack_mode = (byte)pointer_attack_mode;
    lines[*count].click_action = SDL_PANEL_CLICK_NONE;
    (*count)++;
}

static void hidden_left_panel_add_icon_line(hidden_overlay_line* lines,
    int* count, int max_lines, byte attr, cptr text, cptr short_text,
    byte icon_attr, char icon_char)
{
    hidden_left_panel_add_icon_line_mode(lines, count, max_lines, attr, text,
        short_text, icon_attr, icon_char, SDL_POINTER_ATTACK_NONE);
}

static void hidden_left_panel_add_quiver_line(hidden_overlay_line* lines,
    int* count, int max_lines, const object_type* q_ptr, int pointer_attack_mode)
{
    char buf[32];
    byte attr;

    if (!q_ptr || !q_ptr->k_idx || q_ptr->number <= 0)
        return;

    strnfmt(buf, sizeof(buf), "%d", q_ptr->number);
    attr = pointer_attack_quiver_panel_attr(pointer_attack_mode, TERM_L_WHITE);

    hidden_left_panel_add_icon_line_mode(lines, count, max_lines, attr,
        buf, buf, object_attr(q_ptr), object_char(q_ptr),
        pointer_attack_mode);
}

static int hidden_left_panel_line_width(const hidden_overlay_line* line,
    bool use_short_text)
{
    const char* text;
    int width = 0;

    if (!line)
        return 0;

    text = (use_short_text && line->short_text[0])
        ? line->short_text
        : line->text;
    if (line->has_icon)
        width += 3;
    width += (int)strlen(text);

    return width;
}

static int hidden_left_panel_mask_width(int start_col, int width)
{
    int end_col;

    if (width < 0)
        width = 0;

    if (Term && width > Term->wid)
        width = Term->wid;

    end_col = start_col + width;

    if (Term && use_bigtile && end_col > COL_MAP && end_col < Term->wid
        && (((end_col - COL_MAP) & 1) != 0))
    {
        /*
         * Compact left-panel overlays sit over the map. If the text ends on
         * the first half of a bigtile cell, cover the trailing half too;
         * otherwise map redraws can expose stale half-tiles at the edge.
         */
        width++;
    }

    if (Term && start_col + width > Term->wid)
        width = Term->wid - start_col;
    if (width < 0)
        width = 0;

    return width;
}

static int hidden_left_panel_draw_line(const hidden_overlay_line* line, int row,
    int col, bool use_short_text, byte* out_chars, int out_chars_max)
{
    const char* text;
    byte text_attr;
    byte icon_attr;
    int written = 0;

    if (!line || !Term)
        return 0;
    if (row < 0 || row >= Term->hgt || col >= Term->wid)
        return 0;

    text = (use_short_text && line->short_text[0])
        ? line->short_text
        : line->text;
    text_attr = panel_touch_zone_attr(line->click_action, row, line->attr);
    icon_attr = line->icon_attr;

    if (line->has_icon)
    {
        byte icon_bg_attr = left_panel_selected_bg_from_attr(text_attr);

        if (col < Term->wid)
        {
            left_panel_put_icon_cell(icon_attr, line->icon_char, row, col,
                icon_bg_attr);
            if (out_chars && written < out_chars_max)
                out_chars[written] = (byte)line->icon_char;
        }
        written++;

        if (use_bigtile)
        {
            if (col + 1 < Term->wid)
            {
                left_panel_put_bigtile_continuation(icon_attr,
                    line->icon_char, row, col + 1, icon_bg_attr);
                if (out_chars && written < out_chars_max)
                    out_chars[written] = (byte)Term->scr->c[row][col + 1];
            }
        }
        else
        {
            if (col + 1 < Term->wid)
            {
                left_panel_put_icon_cell(icon_attr, line->icon_char, row,
                    col + 1, icon_bg_attr);
                if (out_chars && written < out_chars_max)
                    out_chars[written] = (byte)line->icon_char;
            }
        }
        written++;

        if (col + 2 < Term->wid)
        {
            left_panel_put_space_with_bg(row, col + 2, icon_bg_attr);
            if (out_chars && written < out_chars_max)
                out_chars[written] = (byte)' ';
        }
        written++;

        col += 3;
    }

    if (text[0])
    {
        int text_len = (int)strlen(text);
        if (col + text_len > Term->wid)
            text_len = Term->wid - col;

        if (text_len > 0)
            Term_putstr(col, row, text_len, text_attr, text);
        for (int i = 0; i < text_len; i++)
        {
            if (out_chars && written + i < out_chars_max)
                out_chars[written + i] = (byte)text[i];
        }
        written += text_len;
    }

    return written;
}

int hidden_left_panel_build_lines(hidden_overlay_line* lines, int max_lines)
{
    int count = 0;
    char buf[32];
    char short_buf[16];
    byte hp_color;
    byte voice_color;

    if (!lines || !Term || !p_ptr || max_lines <= 0)
        return 0;

    hp_color = health_attr(p_ptr->chp, p_ptr->mhp);
    if (p_ptr->csp >= p_ptr->msp)
        voice_color = TERM_L_GREEN;
    else if (p_ptr->csp > (p_ptr->msp * op_ptr->hitpoint_warn) / 10)
        voice_color = TERM_YELLOW;
    else
        voice_color = TERM_RED;

    strnfmt(buf, sizeof(buf), "HP %3d", MIN(p_ptr->chp, 999));
    {
        int old_count = count;
        hidden_left_panel_add_line(lines, &count, max_lines, hp_color, buf);
        if (count > old_count)
            lines[count - 1].click_action = SDL_PANEL_CLICK_CHARACTER;
    }

    strnfmt(buf, sizeof(buf), "VC %3d", MIN(p_ptr->csp, 999));
    {
        int old_count = count;
        hidden_left_panel_add_line(lines, &count, max_lines, voice_color, buf);
        if (count > old_count)
            lines[count - 1].click_action = SDL_PANEL_CLICK_SONG;
    }

    {
        bool infinite = false;
        long fuel = 0;
        byte light_attr = TERM_L_WHITE;
        byte light_icon_attr = TERM_WHITE;
        char light_icon = ' ';

        if (current_light_status(&infinite, &fuel, &light_attr, &light_icon_attr,
            &light_icon)
            && !infinite)
        {
            strnfmt(buf, sizeof(buf), "%ld", fuel);
            int old_count = count;
            hidden_left_panel_add_icon_line(lines, &count, max_lines, light_attr,
                buf, buf, light_icon_attr, light_icon);
            if (count > old_count)
                lines[count - 1].click_action = SDL_PANEL_CLICK_SUPPLIES_LIGHTS;
        }
        else if (!inventory[INVEN_LITE].k_idx)
        {
            int old_count = count;
            hidden_left_panel_add_line(lines, &count, max_lines, TERM_SLATE,
                "Torch");
            if (count > old_count)
                lines[count - 1].click_action = SDL_PANEL_CLICK_SUPPLIES_LIGHTS;
        }
    }

    {
        const object_type* icon_obj = left_panel_melee_display_object();

        strnfmt(buf, sizeof(buf), "%s(%+d,%dd%d)",
            p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] ? "M2" : "M",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
        SDL_strlcpy(short_buf, buf, sizeof(short_buf));
        {
            int old_count = count;
            if (left_panel_object_icon_is_usable(icon_obj))
                hidden_left_panel_add_icon_line_mode(lines, &count, max_lines,
                    pointer_attack_panel_attr(SDL_POINTER_ATTACK_MELEE,
                        TERM_L_WHITE),
                    buf, short_buf, object_attr(icon_obj), object_char(icon_obj),
                    SDL_POINTER_ATTACK_MELEE);
            else
                hidden_left_panel_add_line_mode(lines, &count, max_lines,
                    pointer_attack_panel_attr(SDL_POINTER_ATTACK_MELEE,
                        TERM_L_WHITE),
                    buf, SDL_POINTER_ATTACK_MELEE);
            if (count > old_count)
                SDL_strlcpy(lines[count - 1].short_text, short_buf,
                    sizeof(lines[count - 1].short_text));
        }

        if (pointer_attack_melee_has_offhand())
        {
            const object_type* offhand_icon = left_panel_offhand_display_object();

            strnfmt(buf, sizeof(buf), "O(%+d,%dd%d)",
                p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod, p_ptr->mdd2,
                p_ptr->mds2);
            SDL_strlcpy(short_buf, buf, sizeof(short_buf));
            {
                int old_count = count;
                if (left_panel_object_icon_is_usable(offhand_icon))
                    hidden_left_panel_add_icon_line_mode(lines, &count,
                        max_lines,
                        pointer_attack_panel_attr(SDL_POINTER_ATTACK_MELEE,
                            TERM_L_WHITE),
                        buf, short_buf, object_attr(offhand_icon),
                        object_char(offhand_icon), SDL_POINTER_ATTACK_MELEE);
                else
                    hidden_left_panel_add_line_mode(lines, &count, max_lines,
                        pointer_attack_panel_attr(SDL_POINTER_ATTACK_MELEE,
                            TERM_L_WHITE),
                        buf, SDL_POINTER_ATTACK_MELEE);
                if (count > old_count)
                    SDL_strlcpy(lines[count - 1].short_text, short_buf,
                        sizeof(lines[count - 1].short_text));
            }
        }
    }

    {
        int arc_attack = 0;
        int arc_dd = 0;
        int arc_ds = 0;
        bool arc_throwing = false;

        if (pointer_attack_ranged_display_stats(&arc_attack, &arc_dd, &arc_ds,
            &arc_throwing))
        {
            {
                cptr label = arc_throwing ? "T" : "A";
                const object_type* icon_obj =
                    left_panel_ranged_display_object(arc_throwing);

                strnfmt(buf, sizeof(buf), "%s(%+d,%dd%d)", label, arc_attack,
                    arc_dd, arc_ds);
                strnfmt(short_buf, sizeof(short_buf), "%s(%+d,%dd%d)",
                    label, arc_attack, arc_dd, arc_ds);
                {
                    int old_count = count;
                    if (left_panel_object_icon_is_usable(icon_obj))
                        hidden_left_panel_add_icon_line_mode(lines, &count,
                            max_lines,
                            pointer_attack_ranged_panel_attr(TERM_UMBER),
                            buf, short_buf, object_attr(icon_obj),
                            object_char(icon_obj), SDL_POINTER_ATTACK_RANGED_1);
                    else
                        hidden_left_panel_add_line_mode(lines, &count, max_lines,
                            pointer_attack_ranged_panel_attr(TERM_UMBER),
                            buf, SDL_POINTER_ATTACK_RANGED_1);
                    if (count > old_count)
                        SDL_strlcpy(lines[count - 1].short_text, short_buf,
                            sizeof(lines[count - 1].short_text));
                }
            }
        }
    }

    {
        hidden_left_panel_add_quiver_line(lines, &count, max_lines,
            &inventory[INVEN_QUIVER1], SDL_POINTER_ATTACK_RANGED_1);
        hidden_left_panel_add_quiver_line(lines, &count, max_lines,
            &inventory[INVEN_QUIVER2], SDL_POINTER_ATTACK_RANGED_2);
    }

    {
        const object_type* armour_icon = left_panel_armour_display_object();
        int old_count = count;

        left_panel_format_evasion_value(buf, sizeof(buf));
        if (left_panel_object_icon_is_usable(armour_icon))
            hidden_left_panel_add_icon_line(lines, &count, max_lines,
                TERM_SLATE, buf, buf, object_attr(armour_icon),
                object_char(armour_icon));
        else
            hidden_left_panel_add_line(lines, &count, max_lines, TERM_SLATE,
                buf);
        if (count > old_count)
            lines[count - 1].click_action = SDL_PANEL_CLICK_INVENTORY;
    }

    if (!ui_status_pane_owns_left_panel_statuses()
        && !ui_compact_status_line_handles_wounds())
    {
        if (p_ptr->cut > 100)
        {
            hidden_left_panel_add_line(lines, &count, max_lines, TERM_RED, "MW !!!");
        }
        else if (p_ptr->cut > 20)
        {
            strnfmt(buf, sizeof(buf), "BL %3d", MIN(p_ptr->cut, 999));
            hidden_left_panel_add_line(lines, &count, max_lines, TERM_RED, buf);
        }
        else if (p_ptr->cut > 0)
        {
            strnfmt(buf, sizeof(buf), "BL %3d", MIN(p_ptr->cut, 999));
            hidden_left_panel_add_line(lines, &count, max_lines, TERM_L_RED, buf);
        }

        if (p_ptr->poisoned > 20)
        {
            strnfmt(buf, sizeof(buf), "PS %3d", MIN(p_ptr->poisoned, 999));
            hidden_left_panel_add_line(lines, &count, max_lines, TERM_L_GREEN, buf);
        }
        else if (p_ptr->poisoned > 0)
        {
            strnfmt(buf, sizeof(buf), "PS %3d", MIN(p_ptr->poisoned, 999));
            hidden_left_panel_add_line(lines, &count, max_lines, TERM_GREEN, buf);
        }
    }

    if (!ui_status_pane_owns_left_panel_statuses()
        && !ui_compact_status_line_handles_song()
        && (p_ptr->song1 != SNG_NOTHING || p_ptr->song2 != SNG_NOTHING))
    {
        char* song1_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
        char* song2_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;
        buf[0] = '\0';

        if (p_ptr->song1 != SNG_NOTHING && p_ptr->song2 != SNG_NOTHING)
            strnfmt(buf, sizeof(buf), "%s+%s", song1_name + 8, song2_name + 8);
        else if (p_ptr->song1 != SNG_NOTHING)
            SDL_strlcpy(buf, song1_name + 8, sizeof(buf));
        else if (p_ptr->song2 != SNG_NOTHING)
            SDL_strlcpy(buf, song2_name + 8, sizeof(buf));

        if ((int)strlen(buf) > 8)
            strnfmt(short_buf, sizeof(short_buf), "S:%.*s", 6, buf);
        else
            SDL_strlcpy(short_buf, buf, sizeof(short_buf));

        {
            int old_count = count;
            hidden_left_panel_add_line(lines, &count, max_lines, TERM_L_BLUE, buf);
            if (count > old_count)
            {
                lines[count - 1].click_action = SDL_PANEL_CLICK_SONG;
                SDL_strlcpy(lines[count - 1].short_text, short_buf,
                    sizeof(lines[count - 1].short_text));
            }
        }
    }

    if (p_ptr->health_who
        && mon_list[p_ptr->health_who].ml
        && !p_ptr->image
        && (mon_list[p_ptr->health_who].hp > 0))
    {
        monster_type* m_ptr = &mon_list[p_ptr->health_who];
        char health_bar[10];
        byte attr;

        attr = health_attr(m_ptr->hp, m_ptr->maxhp);
        monster_health_bar_text(m_ptr, health_bar, sizeof(health_bar), 8);

        hidden_left_panel_add_line(lines, &count, max_lines, attr, health_bar);
    }

    return count;
}

bool hidden_left_panel_sync_mask(const hidden_overlay_line* lines, int line_count)
{
    bool changed = false;
    int old_rows = g_hidden_left_panel_overlay_rows;
    int max_rows = old_rows;
    int start_row = 0;

    if (line_count > max_rows)
        max_rows = line_count;
    if (line_count > 16)
        line_count = 16;
    if (line_count > 0)
        start_row = hidden_left_panel_start_row_for_count(line_count);
    if (g_hidden_left_panel_overlay_start_row != (byte)start_row)
        changed = true;
    g_hidden_left_panel_overlay_start_row = (byte)start_row;

    for (int i = 0; i < max_rows && i < 16; i++)
    {
        byte new_start_col = 0;
        byte new_width = 0;
        byte new_mode = SDL_POINTER_ATTACK_NONE;
        byte new_start = 0;
        byte new_end = 0;
        byte new_click_action = SDL_PANEL_CLICK_NONE;
        byte new_click_start = 0;
        byte new_click_end = 0;

        if (i < line_count && lines[i].text[0])
        {
            int width = hidden_left_panel_line_width(&lines[i], false);
            int text_start = lines[i].has_icon ? 3 : 0;
            int start_col = hidden_left_panel_start_col_for_width(width);
            width = hidden_left_panel_mask_width(start_col, width);
            start_col = hidden_left_panel_start_col_for_width(width);
            width = hidden_left_panel_mask_width(start_col, width);
            if (width > 255)
                width = 255;
            if (start_col > 255)
                start_col = 255;
            new_start_col = (byte)start_col;
            new_width = (byte)width;
            if (lines[i].click_action != SDL_PANEL_CLICK_NONE
                && text_start < width)
            {
                new_click_action = lines[i].click_action;
                new_click_start = (byte)MIN(start_col + text_start, 255);
                new_click_end = (byte)MIN(start_col + width, 255);
            }
            if (lines[i].pointer_attack_mode != SDL_POINTER_ATTACK_NONE
                && text_start < width)
            {
                new_mode = lines[i].pointer_attack_mode;
                new_start = (byte)MIN(start_col + text_start, 255);
                new_end = (byte)MIN(start_col + width, 255);
            }
        }

        if (g_hidden_left_panel_overlay_start_cols[i] != new_start_col
            || g_hidden_left_panel_overlay_widths[i] != new_width)
        {
            changed = true;
        }

        g_hidden_left_panel_overlay_start_cols[i] = new_start_col;
        g_hidden_left_panel_overlay_widths[i] = new_width;
        g_hidden_left_panel_overlay_attack_modes[i] = new_mode;
        g_hidden_left_panel_overlay_attack_start_cols[i] = new_start;
        g_hidden_left_panel_overlay_attack_end_cols[i] = new_end;
        g_hidden_left_panel_overlay_click_actions[i] = new_click_action;
        g_hidden_left_panel_overlay_click_start_cols[i] = new_click_start;
        g_hidden_left_panel_overlay_click_end_cols[i] = new_click_end;
    }

    for (int i = max_rows; i < 16; i++)
    {
        g_hidden_left_panel_overlay_start_cols[i] = 0;
        g_hidden_left_panel_overlay_widths[i] = 0;
        g_hidden_left_panel_overlay_attack_modes[i] = SDL_POINTER_ATTACK_NONE;
        g_hidden_left_panel_overlay_attack_start_cols[i] = 0;
        g_hidden_left_panel_overlay_attack_end_cols[i] = 0;
        g_hidden_left_panel_overlay_click_actions[i] = SDL_PANEL_CLICK_NONE;
        g_hidden_left_panel_overlay_click_start_cols[i] = 0;
        g_hidden_left_panel_overlay_click_end_cols[i] = 0;
    }

    if (g_hidden_left_panel_overlay_rows != line_count)
        changed = true;

    g_hidden_left_panel_overlay_rows = (byte)MIN(line_count, 16);

    return changed;
}

void prt_hidden_top_vitals(void)
{
    hidden_overlay_line lines[16];
    int line_count;
    int start_row;

    if (!Term || !p_ptr || !hidden_left_panel_visible())
        return;

    line_count = hidden_left_panel_build_lines(lines, 16);
    if (line_count <= 0)
        return;

    start_row = hidden_left_panel_start_row_for_count(line_count);

    for (int i = 0; i < line_count && i < 16; i++)
    {
        int width = hidden_left_panel_line_width(&lines[i], false);
        int start_col = hidden_left_panel_start_col_for_width(width);
        int erase_width;
        int row = start_row + i;

        erase_width = hidden_left_panel_mask_width(start_col, width);
        start_col = hidden_left_panel_start_col_for_width(erase_width);
        erase_width = hidden_left_panel_mask_width(start_col, width);

        if (erase_width <= 0)
            continue;
        if (row < 0 || row >= Term->hgt)
            continue;

        Term_erase(start_col, row, erase_width);
        hidden_left_panel_draw_line(&lines[i], row, start_col, false, NULL, 0);
    }
}

void redraw_hidden_left_panel_overlay(void)
{
    prt_hidden_top_vitals();
}
