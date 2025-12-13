/* File: xtra1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "supplies.h"

static u32b ability_log_turn_value(void)
{
    if (playerturn < 0)
        return 0;
    return (u32b)playerturn;
}

static s16b ability_log_depth_value(void)
{
    if (!p_ptr)
        return 0;
    int depth = p_ptr->depth;
    if (depth < 0)
        depth = 0;
    if (depth > INT16_MAX)
        depth = INT16_MAX;
    return (s16b)depth;
}

static bool ability_log_has_entry(int skilltype, int abilitynum)
{
    if (!p_ptr)
        return false;

    u16b count = p_ptr->ability_timeline_count;
    if (count > ABILITY_TIMELINE_MAX)
        count = ABILITY_TIMELINE_MAX;

    for (u16b i = 0; i < count; i++) {
        if (p_ptr->ability_timeline_skill[i] == skilltype
            && p_ptr->ability_timeline_ability[i] == abilitynum)
            return true;
    }
    return false;
}

static void ability_log_append(int skilltype, int abilitynum,
                               u32b turn_value, s16b depth_value)
{
    if (!p_ptr)
        return;
    if (skilltype < 0 || skilltype >= S_MAX
        || abilitynum < 0 || abilitynum >= ABILITIES_MAX)
        return;
    if (ability_log_has_entry(skilltype, abilitynum))
        return;

    u16b count = p_ptr->ability_timeline_count;
    if (count >= ABILITY_TIMELINE_MAX)
        return;

    p_ptr->ability_timeline_skill[count] = (byte)skilltype;
    p_ptr->ability_timeline_ability[count] = (byte)abilitynum;
    p_ptr->ability_timeline_turn[count] = turn_value;
    p_ptr->ability_timeline_depth[count] = depth_value;
    p_ptr->ability_timeline_count = count + 1;
}

void ability_log_reset(void)
{
    if (!p_ptr)
        return;
    p_ptr->ability_timeline_count = 0;
    memset(p_ptr->ability_timeline_skill, 0,
        sizeof(p_ptr->ability_timeline_skill));
    memset(p_ptr->ability_timeline_ability, 0,
        sizeof(p_ptr->ability_timeline_ability));
    memset(p_ptr->ability_timeline_turn, 0,
        sizeof(p_ptr->ability_timeline_turn));
    memset(p_ptr->ability_timeline_depth, 0,
        sizeof(p_ptr->ability_timeline_depth));
}

void ability_log_record_gain(int skilltype, int abilitynum)
{
    ability_log_append(skilltype, abilitynum,
        ability_log_turn_value(), ability_log_depth_value());
}

void ability_log_sync_missing(void)
{
    if (!p_ptr)
        return;

    s16b depth = ability_log_depth_value();
    for (int skill = 0; skill < S_MAX; skill++) {
        for (int abil = 0; abil < ABILITIES_MAX; abil++) {
            if (!p_ptr->innate_ability[skill][abil])
                continue;
            ability_log_append(skill, abil, 0, depth);
        }
    }
}

/*
 * Determines the total melee damage dice (before criticals and slays)
 */

byte total_mdd(const object_type* o_ptr)
{
    byte dd;

    /* if no weapon is wielded, use 1d1 */
    if (o_ptr->tval == 0)
    {
        dd = 1;
    }
    /* otherwise use the weapon dice */
    else
    {
        dd = o_ptr->dd;
    }
    /* add the modifiers */
    dd += p_ptr->to_mdd;

    if (p_ptr->active_ability[S_WIL][WIL_VENGEANCE])
    {
        dd += p_ptr->vengeance;
    }

    return (dd);
}

/*
 * Determines the strength modified damage sides for a melee or thrown weapon
 * Includes factors for strength and weight, but not bonuses from ring of damage
 * etc
 */
byte strength_modified_ds(const object_type* o_ptr, int str_adjustment)
{
    byte mds;
    int int_mds; /* to allow negative values in the intermediate stages */
    int str_to_mds;
    int divisor;

    str_to_mds = p_ptr->stat_use[A_STR] + str_adjustment;

    /* if no weapon, use 1d1 and don't limit strength bonus */
    if (o_ptr->tval == 0)
    {
        int_mds = 1;
        int_mds += str_to_mds;
    }
    /* if a weapon is being assessed, use its dice and limit bonus */
    else
    {
        int_mds = o_ptr->ds;

        if (two_handed_melee())
        {
            divisor = 10;

            /* Bonus for 'hand and a half' weapons like the bastard sword when
             * used with two hands - but not when using Subtlety */
            if (!p_ptr->active_ability[S_MEL][MEL_CONTROL])
            {
                int_mds += hand_and_a_half_bonus(o_ptr);
            }
        }
        else
        {
            divisor = 10;
        }

        /* limit the strength sides bonus by weapon weight */
        if ((str_to_mds > 0) && (str_to_mds > (o_ptr->weight / divisor)))
        {
            int_mds += o_ptr->weight / divisor;
        }
        else if ((str_to_mds < 0) && (str_to_mds < -(o_ptr->weight / divisor)))
        {
            int_mds += -(o_ptr->weight / divisor);
        }
        else
        {
            int_mds += str_to_mds;
        }
    }

    // add generic damage bonus
    int_mds += p_ptr->to_mds;

    // bonus for users of 'mighty blows' ability
    if (p_ptr->active_ability[S_MEL][MEL_POWER])
    {
        int_mds += 1;
    }

    /* make sure the total is non-negative */
    mds = (int_mds < 0) ? 0 : int_mds;

    return (mds);
}

/*
 * Determines the total melee damage sides (from strength and to_mds)
 * Does include strength and weight modifiers
 *
 * This function seems rather unnecessary these days...
 */
extern byte total_mds(const object_type* o_ptr, int str_adjustment)
{
    byte mds;
    int int_mds; /* to allow negative values in the inetermediate stages */

    int_mds = strength_modified_ds(o_ptr, str_adjustment);

    /* make sure the total is non-negative */
    mds = (int_mds < 0) ? 0 : int_mds;

    return (mds);
}

/*
 * Two handed melee weapon (including bastard sword used two handed)
 */
extern bool two_handed_melee(void)
{
    object_type* o_ptr = &inventory[INVEN_WIELD];

    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        || hand_and_a_half_bonus(o_ptr))
    {
        return (true);
    }
    
    /* For Maedhros character, hand-and-a-half weapons count as two-handed for ability purposes */
    if ((c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS)
        && (k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF))
        && (&inventory[INVEN_WIELD] == o_ptr) && (!inventory[INVEN_ARM].k_idx))
    {
        return (true);
    }
    
    return (false);
}

/*
 * Bonus for 'hand and a half' weapons like the bastard sword when wielded with
 * two hands
 */
extern int hand_and_a_half_bonus(const object_type* o_ptr) //XXX Hand and a half
{
    if ((k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF))
        && (&inventory[INVEN_WIELD] == o_ptr) && (!inventory[INVEN_ARM].k_idx))
    {
        /* Maedhros character gets double the hand-and-a-half bonus */
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS)
        {
            return (3);
        }
        return (2);
    }
    return (0);
}

/*
 * Bonus for certain race/character blends (elves) using bows
 */
int bow_bonus(const object_type* o_ptr)
{
    int bonus = 0;

    if ((rp_ptr->flags & RHF_BOW_PROFICIENCY) && (o_ptr->tval == TV_BOW))
    {
        bonus += 1;
    }
    if ((current_character_profile->flags & RHF_BOW_PROFICIENCY) && (o_ptr->tval == TV_BOW))
    {
        bonus += 1;
    }

    return bonus;
}

/*
 * Bonus for certain race/character blends (dwarves) using axes
 */
int axe_bonus(const object_type* o_ptr)
{
    int bonus = 0;

    u32b f1, f2, f3;

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    if ((rp_ptr->flags & RHF_AXE_PROFICIENCY) && (f3 & (TR3_AXE)))
    {
        bonus += 1;
    }
    if ((current_character_profile->flags & RHF_AXE_PROFICIENCY) && (f3 & (TR3_AXE)))
    {
        bonus += 1;
    }

    return bonus;
}

/*
 * Bonus for people with polearm affinity
 */
int polearm_bonus(const object_type* o_ptr)
{
    int bonus = 0;

    u32b f1, f2, f3;

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    if (p_ptr->active_ability[S_MEL][MEL_POLEARMS] && (f3 & (TR3_POLEARM)))
    {
        bonus += 2;
    }

    return bonus;
}

/*
 * Determines the total damage side for archery
 * based on the weight of the bow, strength, and the sides of the bow
 */

extern byte total_ads(const object_type* j_ptr)
{
    byte ads;
    int int_ads; /* to allow negative values in the intermediate stages */
    int str_to_ads;

    str_to_ads = p_ptr->stat_use[A_STR];

    int_ads = j_ptr->ds;

    /* limit the strength sides bonus by bow weight */
    if ((str_to_ads > 0) && (str_to_ads > (j_ptr->weight / 10)))
    {
        int_ads += j_ptr->weight / 10;
    }
    else if ((str_to_ads < 0) && (str_to_ads < -(j_ptr->weight / 10)))
    {
        int_ads += -(j_ptr->weight / 10);
    }
    else
    {
        int_ads += str_to_ads;
    }

    // add archery damage bonus
    int_ads += p_ptr->to_ads;

    /* make sure the total is non-negative */
    ads = (int_ads < 0) ? 0 : int_ads;

    return (ads);
}

/*
 * Converts stat num into a two-char (right justified) string
 * Sil: rather pointless since stats no longer have and 18/XYZ format
 */
void cnv_stat(int val, char* out_val) { sprintf(out_val, "%2d", val); }

/*
 * Print character info at given row, column in a 13 char field
 */
static void prt_field(cptr info, int row, int col)
{
    /* Dump 13 spaces to clear */
    c_put_str(TERM_WHITE, "             ", row, col);

    sdl_story_font_enable();
    /* Dump the info itself */
    c_put_str(TERM_L_BLUE, info, row, col);
    
    sdl_story_font_disable();
}

/*
 * Print character stat in given row, column
 */
static void prt_stat(int stat)
{
    char tmp[32];
    char trimmed_label[32];
    const char* stat_label;
    int len;

    /* Clear the line */
    put_str("             ", ROW_STAT + stat, 0);

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

    log_trace("prt_stat: Calling put_str('%s', %d, %d)", trimmed_label, ROW_STAT + stat, 0);
    put_str(trimmed_label, ROW_STAT + stat, 0);

    int cursor_x, cursor_y;
    Term_locate(&cursor_x, &cursor_y);
    log_trace("prt_stat: After put_str, cursor at (%d, %d)", cursor_x, cursor_y);
    log_trace("prt_stat: Disabling story font");
    sdl_story_font_disable();

    /* Display stat value with monospace font */
    cnv_stat(p_ptr->stat_use[stat], tmp);
    len = strlen(tmp);
    log_trace("prt_stat: Calling c_put_str('%s', %d, %d) for stat value", tmp, ROW_STAT + stat, COL_STAT + 12 - len);
    if (p_ptr->stat_drain[stat] < 0)
    {
        c_put_str(TERM_YELLOW, tmp, ROW_STAT + stat, COL_STAT + 12 - len);
    }
    else
    {
        c_put_str(TERM_L_GREEN, tmp, ROW_STAT + stat, COL_STAT + 12 - len);
    }

    /* Indicate temporary modifiers - clear first, then conditionally display */
    if ((stat == A_STR) && p_ptr->tmp_str)
        put_str("*", ROW_STAT + stat, 3);
    else if ((stat == A_DEX) && p_ptr->tmp_dex)
        put_str("*", ROW_STAT + stat, 3);
    else if ((stat == A_CON) && p_ptr->tmp_con)
        put_str("*", ROW_STAT + stat, 3);
    else if ((stat == A_GRA) && p_ptr->tmp_gra)
        put_str("*", ROW_STAT + stat, 3);
}

/*
 * Display the experience
 */
static void prt_exp(void)
{
    char out_val[32];
    byte attr;
    int len;

    attr = TERM_L_GREEN;

    sdl_story_font_enable();

    /*Print experience label*/
    put_str("Exp", ROW_EXP, 0);

    sdl_story_font_disable();

    comma_number(out_val, p_ptr->new_exp);
    len = strlen(out_val);

    c_put_str(attr, out_val, ROW_EXP, COL_EXP + 12 - len);
}

/*
 * Prints current mel
 */
static void prt_mel(void)
{
    char buf[32];
    int mod = 0;

    if (((&inventory[INVEN_ARM])->k_idx)
        && ((&inventory[INVEN_ARM])->tval != TV_SHIELD))
        mod = -1;

    /* Melee attacks */
    int meleeColour
        = p_ptr->active_ability[S_MEL][MEL_SMITE] ? TERM_L_RED : TERM_L_WHITE;
    strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds);
    c_put_str(meleeColour, buf, ROW_MEL + mod, COL_MEL + 12 - strlen(buf));

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        c_put_str(TERM_WHITE, "2x", ROW_MEL + mod, COL_MEL);
    }

    if (mod == -1)
    {
        strnfmt(buf, sizeof(buf), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod, p_ptr->mdd2,
            p_ptr->mds2);
        c_put_str(TERM_L_WHITE, buf, ROW_MEL, COL_MEL + 12 - strlen(buf));
    }
    else
    {
        strnfmt(buf, sizeof(buf), "            ");
        c_put_str(TERM_L_BLUE, buf, ROW_MEL - 1, 0);
    }
}

/*
 * Prints current arc
 */
static void prt_arc(void)
{
    char buf[32];

    /* Range attacks */
    if ((&inventory[INVEN_BOW])->k_idx)
    {
        if (p_ptr->active_ability[S_ARC][ARC_DEADLY_HAIL]
            && p_ptr->killed_enemy_with_arrow)
        {
            strnfmt(buf, sizeof(buf), ")");
            c_put_str(TERM_UMBER, buf, ROW_ARC, COL_ARC + 12 - strlen(buf));
            strnfmt(buf, sizeof(buf), "%dd%d", 2 * p_ptr->add, p_ptr->ads);
            c_put_str(TERM_RED, buf, ROW_ARC, COL_ARC + 11 - strlen(buf));
            strnfmt(buf, sizeof(buf), "(%+d,", p_ptr->skill_use[S_ARC]);
            if (p_ptr->ads > 9)
                c_put_str(TERM_UMBER, buf, ROW_ARC, COL_ARC + 7 - strlen(buf));
            else
                c_put_str(TERM_UMBER, buf, ROW_ARC, COL_ARC + 8 - strlen(buf));
        }
        else
        {
            strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_ARC],
                p_ptr->add, p_ptr->ads);
            c_put_str(TERM_UMBER, buf, ROW_ARC, COL_ARC + 12 - strlen(buf));
        }
    }
    else
    {
        strnfmt(buf, sizeof(buf), "            ");
        c_put_str(TERM_L_BLUE, buf, ROW_ARC, 0);
    }
}

/*
 * Prints current quiver status (current/max for both quivers)
 * Right-aligned to 12 character width, like other stats
 * Same type: icon in middle between counts
 * Different: icon before each count
 */
static void prt_quiver(void)
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
        /* Layout: "11/48[→][→]7/7" */
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
        byte attr = object_attr(q1_ptr);
        char icon = object_char(q1_ptr);
        
        /* Q1 count */
        Term_putstr(col, ROW_QUIVER, -1, TERM_L_WHITE, buf1);
        col += strlen(buf1);
        
        /* Icon in middle */
        Term_putch(col, ROW_QUIVER, attr, icon);
        col++;
        if (use_bigtile)
        {
            Term_putch(col, ROW_QUIVER, 255, -1);
            col++;
        }
        else
        {
            Term_putch(col, ROW_QUIVER, attr, icon);
            col++;
        }
        
        /* Q2 count */
        Term_putstr(col, ROW_QUIVER, -1, TERM_L_WHITE, buf2);
    }
    else
    {
        /* Different types: icon before each count */
        if (q1_ptr->k_idx)
        {
            /* Q1: "[icon][icon]cur/max" */
            byte attr = object_attr(q1_ptr);
            char icon = object_char(q1_ptr);
            Term_putch(col, ROW_QUIVER, attr, icon);
            col++;
            if (use_bigtile)
            {
                Term_putch(col, ROW_QUIVER, 255, -1);
                col++;
            }
            else
            {
                Term_putch(col, ROW_QUIVER, attr, icon);
                col++;
            }
            
            Term_putstr(col, ROW_QUIVER, -1, TERM_L_WHITE, buf1);
            col += strlen(buf1);
        }
        
        if (q2_ptr->k_idx)
        {
            /* Q2: "[icon][icon]cur/max" */
            byte attr = object_attr(q2_ptr);
            char icon = object_char(q2_ptr);
            Term_putch(col, ROW_QUIVER, attr, icon);
            col++;
            if (use_bigtile)
            {
                Term_putch(col, ROW_QUIVER, 255, -1);
                col++;
            }
            else
            {
                Term_putch(col, ROW_QUIVER, attr, icon);
                col++;
            }
            
            Term_putstr(col, ROW_QUIVER, -1, TERM_L_WHITE, buf2);
        }
    }
}

/*
 * Prints current evn
 */
static void prt_evn(void)
{
    char buf[32];

    // Toggle blocking on and off so we don't show the blocking value in
    // the armor total
    bool block = p_ptr->active_ability[S_EVN][EVN_BLOCKING];
    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = false;
    /* Total Armor */
    strnfmt(buf, sizeof(buf), "[%+d,%d-%d]", p_ptr->skill_use[S_EVN],
        p_min(GF_HURT, true), p_max(GF_HURT, true));
    c_put_str(TERM_SLATE, buf, ROW_EVN, COL_EVN + 12 - strlen(buf));
    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = block;
}

/*
 * Prints Cur/Max hit points
 */
static void prt_hp(void)
{
    char tmp[32];
    byte color;

    /* Clear the line */
    put_str("             ", ROW_HP, COL_HP);

    sdl_story_font_enable();

    if (p_ptr->mhp >= 100)
    {
        put_str("Hth", ROW_HP, COL_HP);
    }
    else
    {
        put_str("Health", ROW_HP, COL_HP);
    }

    sdl_story_font_disable();

    /* Get color for current HP */
    color = health_attr(p_ptr->chp, p_ptr->mhp);

    /* Calculate lengths for left (current) and right (max) parts */
    int chp_len = sprintf(tmp, "%d", p_ptr->chp);
    int mhp_len = sprintf(tmp, "%d", p_ptr->mhp);
    int total_len = chp_len + 1 + mhp_len; /* +1 for the slash */

    /* Print current HP in color */
    sprintf(tmp, "%d", p_ptr->chp);
    c_put_str(color, tmp, ROW_HP, COL_HP + 12 - total_len);

    /* Print slash in green */
    c_put_str(TERM_L_GREEN, "/", ROW_HP, COL_HP + 12 - total_len + chp_len);

    /* Print max HP in green */
    sprintf(tmp, "%d", p_ptr->mhp);
    c_put_str(TERM_L_GREEN, tmp, ROW_HP, COL_HP + 12 - total_len + chp_len + 1);
}

/*
 * Prints a small, monospace graphical health bar under the name.
 * Uses 'x' characters up to 12 symbols to represent current HP proportionally.
 * Colour matches health_attr() (green/yellow/red, etc).
 */
static void prt_char_health_graphic(void)
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

    /* Print using a monospace field (no story font) */
    c_put_str(color, format("%12s", bar), ROW_NAME + 1, COL_NAME);
}

static void prt_light(void)
{
    object_type* o_ptr = &inventory[INVEN_LITE];
    int icon_col = COL_LIGHT;

    /* Clear the line */
    Term_erase(icon_col, ROW_LIGHT, 13);

    /* Nothing equipped */
    if (!o_ptr->k_idx)
        return;

    byte attr = object_attr(o_ptr);
    char icon = object_char(o_ptr);

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

    bool infinite = false;
    long fuel = 0;
    byte fuel_attr = TERM_L_WHITE;
    char buf[16];

    if (o_ptr->tval == TV_LIGHT)
    {
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_LANTERN:
        case SV_LIGHT_MALLORN:
            fuel = o_ptr->timeout;
            break;
        default:
            infinite = true;
            break;
        }
    }
    else
    {
        u32b f1, f2, f3;
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & TR2_LIGHT)
            infinite = true;
    }

    if (infinite)
    {
        SDL_strlcpy(buf, "inf", sizeof(buf));
        fuel_attr = TERM_L_GREEN;
    }
    else
    {
        if (fuel < 0)
            fuel = 0;

        if (fuel == 0)
            fuel_attr = TERM_RED;
        else if (fuel <= 100)
            fuel_attr = TERM_ORANGE;

        strnfmt(buf, sizeof(buf), "%ld", fuel);
    }

    c_put_str(fuel_attr, buf, ROW_LIGHT, icon_col + 12 - strlen(buf));
}

/*
 * Prints player's max/cur spell points
 */
static void prt_sp(void)
{
    char tmp[32];
    byte color;
    int len;

    /* Clear the line */
    put_str("             ", ROW_SP, COL_SP);

    sdl_story_font_enable();

    if (p_ptr->msp >= 100)
        put_str("Vce", ROW_SP, COL_SP);
    else
        put_str("Voice", ROW_SP, COL_SP);

    sdl_story_font_disable();

    len = sprintf(tmp, "%d:%d", p_ptr->csp, p_ptr->msp);

    c_put_str(TERM_L_GREEN, tmp, ROW_SP, COL_SP + 12 - len);

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

    c_put_str(color, tmp, ROW_SP, COL_SP + 12 - len);
}

/*
 * Prints player's current song (if any)
 */
static void prt_song(void)
{
    char* song1_name
        = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
    char* song2_name
        = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;

    // wipe old songs
    put_str("             ", ROW_SONG, COL_SONG);
    put_str("             ", ROW_SONG + 1, COL_SONG);

    sdl_story_font_enable();

    // show the first song
    if (p_ptr->song1 != SNG_NOTHING)
    {
        c_put_str(TERM_L_BLUE, song1_name + 8, ROW_SONG, COL_SONG);
    }

    // show the second song
    if (p_ptr->song2 != SNG_NOTHING)
    {
        c_put_str(TERM_BLUE, song2_name + 8, ROW_SONG + 1, COL_SONG);
    }

    sdl_story_font_disable();
}

/*
 * Prints depth in stat area
 */
static void prt_depth(void)
{
    char depths[32];
    s16b attr = TERM_WHITE;

    if (!p_ptr->depth)
    {
        SDL_strlcpy(depths, "Surface", sizeof(depths));
    }
    else
    {
        sprintf(depths, "%d ft", p_ptr->depth * 50);
    }

    /* Get color of level based on feeling  -JSV- */
    if ((p_ptr->depth) && (do_feeling))
    {
        if (feeling == 1)
            attr = TERM_VIOLET;
        else if (feeling == 2)
            attr = TERM_RED;
        else if (feeling == 3)
            attr = TERM_L_RED;
        else if (feeling == 4)
            attr = TERM_ORANGE;
        else if (feeling == 5)
            attr = TERM_ORANGE;
        else if (feeling == 6)
            attr = TERM_YELLOW;
        else if (feeling == 7)
            attr = TERM_YELLOW;
        else if (feeling == 8)
            attr = TERM_WHITE;
        else if (feeling == 9)
            attr = TERM_WHITE;
        else if (feeling == 10)
            attr = TERM_L_WHITE;
        else if (feeling >= LEV_THEME_HEAD)
            attr = TERM_BLUE;
    }

    sdl_story_font_enable();

    /* Right-Adjust the "depth", and clear old values */
    c_prt(attr, format("%7s", depths), ROW_DEPTH, COL_DEPTH);

    sdl_story_font_disable();
}

/*
 * Prints status of hunger
 */
static void prt_hunger(void)
{
    sdl_story_font_enable();

    /* Fainting / Starving */
    if (p_ptr->food < PY_FOOD_STARVE)
    {
        c_put_str(TERM_RED, "Starving", ROW_HUNGRY, COL_HUNGRY);
    }

    /* Weak */
    else if (p_ptr->food < PY_FOOD_WEAK)
    {
        c_put_str(TERM_ORANGE, "Weak    ", ROW_HUNGRY, COL_HUNGRY);
    }

    /* Hungry */
    else if (p_ptr->food < PY_FOOD_ALERT)
    {
        c_put_str(TERM_YELLOW, "Hungry  ", ROW_HUNGRY, COL_HUNGRY);
    }

    /* Normal */
    else if (p_ptr->food < PY_FOOD_FULL)
    {
        c_put_str(TERM_L_GREEN, "        ", ROW_HUNGRY, COL_HUNGRY);
    }

    /* Full */
    else if (p_ptr->food < PY_FOOD_MAX)
    {
        c_put_str(TERM_L_GREEN, "Full    ", ROW_HUNGRY, COL_HUNGRY);
    }

    else
    {
        c_put_str(TERM_GREEN, "Full    ", ROW_HUNGRY, COL_HUNGRY);
    }

    sdl_story_font_disable();
}

/*
 * Prints Blind status
 */
static void prt_blind(void)
{
    sdl_story_font_enable();

    if (p_ptr->blind)
    {
        c_put_str(TERM_ORANGE, "Blind", ROW_BLIND, COL_BLIND);
    }
    else
    {
        put_str("     ", ROW_BLIND, COL_BLIND);
    }

    sdl_story_font_disable();
}

/*
 * Prints Confusion status
 */
static void prt_confused(void)
{
    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_CONFUSED, ROW_CONFUSED, 8);

    if (p_ptr->confused)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Confused", ROW_CONFUSED, COL_CONFUSED);
        sdl_story_font_disable();
    }
}

/*
 * Prints Fear status
 */
static void prt_afraid(void)
{
    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_AFRAID, ROW_AFRAID, 6);

    if (p_ptr->afraid)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Afraid", ROW_AFRAID, COL_AFRAID);
        sdl_story_font_disable();
    }
}

/*
 *  Displays the amount of bleeding.
 *  This is a bit tricky as it is in the same row as poison, *unless* you have
 * both. In which case it is the row above.
 */

static void prt_cut(void)
{
    int c = p_ptr->cut;
    char num_buf[8];

    int r = ROW_CUT;

    if (p_ptr->poisoned)
        r--;

    /* Clear both possible rows (story font has variable widths) */
    Term_erase(COL_CUT, ROW_CUT - 1, 12);
    Term_erase(COL_CUT, ROW_CUT, 12);

    if (c > 100)
    {
        sdl_story_font_enable();
        c_put_str(TERM_RED, "Mortal wound", r, COL_CUT);
        sdl_story_font_disable();
    }
    else if (c > 20)
    {
        sdl_story_font_enable();
        c_put_str(TERM_RED, "Bleeding", r, COL_CUT);
        sdl_story_font_disable();
        sprintf(num_buf, " %-2d", c);
        c_put_str(TERM_RED, num_buf, r, COL_CUT + 8);
    }
    else if (c > 0)
    {
        sdl_story_font_enable();
        c_put_str(TERM_L_RED, "Bleeding", r, COL_CUT);
        sdl_story_font_disable();
        sprintf(num_buf, " %-2d", c);
        c_put_str(TERM_L_RED, num_buf, r, COL_CUT + 8);
    }
}

/*
 * Prints Poisoned status
 */
static void prt_poisoned(void)
{
    int p = p_ptr->poisoned;
    char num_buf[8];

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_POISONED, ROW_POISONED, 12);

    if (p > 20)
    {
        sdl_story_font_enable();
        c_put_str(TERM_L_GREEN, "Poisoned", ROW_POISONED, COL_POISONED);
        sdl_story_font_disable();
        sprintf(num_buf, " %-3d", p);
        c_put_str(TERM_L_GREEN, num_buf, ROW_POISONED, COL_POISONED + 8);
    }
    else if (p > 0)
    {
        sdl_story_font_enable();
        c_put_str(TERM_GREEN, "Poisoned", ROW_POISONED, COL_POISONED);
        sdl_story_font_disable();
        sprintf(num_buf, " %-3d", p);
        c_put_str(TERM_GREEN, num_buf, ROW_POISONED, COL_POISONED + 8);
    }
}

/*
 * Prints Searching, Resting, Entrancement, Smithing, or 'count' status
 * Display is always exactly 10 characters wide (see below)
 *
 * This function was a major bottleneck when resting, so a lot of
 * the text formatting code was optimized in place below.
 */
static void prt_state(void)
{
    byte attr = TERM_WHITE;

    char text[16];

    /* Entrancement */
    if (p_ptr->entranced)
    {
        attr = TERM_RED;

        SDL_strlcpy(text, "Entranced!", sizeof(text));
    }

    /* Smithing */
    if (p_ptr->smithing)
    {
        SDL_strlcpy(text, "Smithing  ", sizeof(text));
    }

    if (p_ptr->fletching)
    {
        SDL_strlcpy(text, "Fletching ", sizeof(text));
    }
    else if (p_ptr->rage)
    {
        attr = TERM_RED;
        SDL_strlcpy(text, "Rage      ", sizeof(text));
    }

    /* Resting */
    else if (p_ptr->resting)
    {
        int i;
        int n = p_ptr->resting;

        /* Start with "Rest" */
        SDL_strlcpy(text, "Rest      ", sizeof(text));

        /* Extensive (timed) rest */
        if (n >= 1000)
        {
            i = n / 100;
            text[9] = '0';
            text[8] = '0';
            text[7] = I2D(i % 10);
            if (i >= 10)
            {
                i = i / 10;
                text[6] = I2D(i % 10);
                if (i >= 10)
                {
                    text[5] = I2D(i / 10);
                }
            }
        }

        /* Long (timed) rest */
        else if (n >= 100)
        {
            i = n;
            text[9] = I2D(i % 10);
            i = i / 10;
            text[8] = I2D(i % 10);
            text[7] = I2D(i / 10);
        }

        /* Medium (timed) rest */
        else if (n >= 10)
        {
            i = n;
            text[9] = I2D(i % 10);
            text[8] = I2D(i / 10);
        }

        /* Short (timed) rest */
        else if (n > 0)
        {
            i = n;
            text[9] = I2D(i);
        }

        /* Rest until healed */
        else if (n == -1)
        {
            text[5] = text[6] = text[7] = text[8] = text[9] = '*';
        }

        /* Rest until done */
        else if (n == -2)
        {
            text[5] = text[6] = text[7] = text[8] = text[9] = '&';
        }
    }

    /* Repeating */
    else if (p_ptr->command_rep)
    {
        if (p_ptr->command_rep > 999)
        {
            sprintf(text, "Rep. %3d00", p_ptr->command_rep / 100);
        }
        else
        {
            sprintf(text, "Repeat %3d", p_ptr->command_rep);
        }
    }

    /* Stealth mode */
    else if (p_ptr->stealth_mode)
    {
        SDL_strlcpy(text, "Stealth   ", sizeof(text));
    }

    /* Nothing interesting */
    else
    {
        text[0] = '\0';
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_STATE, ROW_STATE, 10);

    /* Display the info if any */
    if (text[0])
    {
        sdl_story_font_enable();
        c_put_str(attr, text, ROW_STATE, COL_STATE);
        sdl_story_font_disable();
    }
}

/*
 * Prints the speed of a character.			-CJS-
 */
static void prt_speed(void)
{
    int i = p_ptr->pspeed;

    byte attr = TERM_WHITE;
    char buf[32] = "";

    /* Fast */
    if (i > 2)
    {
        attr = TERM_L_GREEN;
        sprintf(buf, "Fast");
    }

    /* Slow */
    else if (i < 2)
    {
        attr = TERM_ORANGE;
        sprintf(buf, "Slow");
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_SPEED, ROW_SPEED, 4);

    /* Display the speed if not normal */
    if (buf[0])
    {
        sdl_story_font_enable();
        c_put_str(attr, buf, ROW_SPEED, COL_SPEED);
        sdl_story_font_disable();
    }
}

/*
 * Prints message regarding difficult terrain
 */
static void prt_terrain(void)
{
    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_TERRAIN, ROW_TERRAIN, 8);

    if (cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Pit", ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Web", ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT)
    {
        sdl_story_font_enable();
        c_put_str(TERM_YELLOW, "Sunlight", ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }
}

static void prt_stun(void)
{
    int s = p_ptr->stun;

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_STUN, ROW_STUN, 12);

    if (s > 100)
    {
        sdl_story_font_enable();
        c_put_str(TERM_RED, "Knocked out", ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
    else if (s > 50)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Heavy stun", ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
    else if (s)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Stun", ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
}

/*
 *  Represents the different levels of health.
 *  Note that it is a bit odd with fewer health levels in the SOMEWHAT_WOUNDED
 * category. This is due to a rounding off tension between the natural way to do
 * the colours (perfect having its own) and the natural way to do the stars for
 * the health bar (zero having its own). It should be unnoticeable to the
 * player.
 */
int health_level(int current, int max)
{
    int level;

    if (current == max)
    {
        level = HEALTH_UNHURT; // 100%
    }

    else
    {
        switch ((4 * current + max - 1) / max)
        {
        case 4:
            level = HEALTH_SOMEWHAT_WOUNDED;
            break; //  76% - 99%
        case 3:
            level = HEALTH_WOUNDED;
            break; //  51% - 75%
        case 2:
            level = HEALTH_BADLY_WOUNDED;
            break; //  26% - 50%
        case 1:
            level = HEALTH_ALMOST_DEAD;
            break; //   1% - 25%
        default:
            level = HEALTH_DEAD;
            break; //   0%
        }
    }

    return (level);
}

/*
 *  Assigns colours to the health levels.
 */
byte health_attr(int current, int max)
{
    byte a;

    switch (health_level(current, max))
    {
    case HEALTH_UNHURT:
        a = TERM_L_GREEN;
        break; // 100%
    case HEALTH_SOMEWHAT_WOUNDED:
        a = TERM_YELLOW;
        break; //  76% - 99%
    case HEALTH_WOUNDED:
        a = TERM_ORANGE;
        break; //  51% - 75%
    case HEALTH_BADLY_WOUNDED:
        a = TERM_L_RED;
        break; //  26% - 50%
    case HEALTH_ALMOST_DEAD:
        a = TERM_RED;
        break; //   1% - 25%
    default:
        a = TERM_RED;
        break; //   0%
    }

    return (a);
}

/*
 * Gets a text string denoting the alertness level / stance into a buffer, along
 * with the associated colour.
 */
bool get_alertness_text(
    monster_type* m_ptr, int text_size, char* text, int* color)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (m_ptr->alertness < ALERTNESS_UNWARY)
    {
        SDL_strlcpy(text, "Sleeping", text_size);
        *color = TERM_BLUE;
    }
    else if (m_ptr->alertness < ALERTNESS_ALERT)
    {
        SDL_strlcpy(text, "Unwary", text_size);
        *color = TERM_L_BLUE;
    }
    else
    {
        if (r_ptr->flags2 & (RF2_MINDLESS))
        {
            SDL_strlcpy(text, "Mindless", text_size);
            *color = TERM_L_DARK;
        }
        else
        {
            char morale_buf[8];

            if (m_ptr->stance == STANCE_FLEEING)
            {
                SDL_strlcpy(text, "Fleeing", text_size);
                *color = TERM_VIOLET;
            }
            else if (m_ptr->stance == STANCE_CONFIDENT)
            {
                SDL_strlcpy(text, "Confident", text_size);
                *color = TERM_L_WHITE;
            }
            else if (m_ptr->stance == STANCE_AGGRESSIVE)
            {
                SDL_strlcpy(text, "Aggress", text_size);
                *color = TERM_L_WHITE;
            }

            // sometimes (only in debugging?) we are looking at a monster before
            // it has a stance in this case return false so we don't print the
            // strings
            else
            {
                return false;
            }

            if (m_ptr->morale >= 0)
                sprintf(morale_buf, " %d", (m_ptr->morale + 9) / 10);
            else
                sprintf(morale_buf, " %d", m_ptr->morale / 10);

            strncat(text, morale_buf, text_size - strlen(text));
        }
    }

    return true;
}

/*
 * Redraw the "monster health bar"
 *
 * The "monster health bar" provides visual feedback on the "health"
 * of the monster currently being "tracked".  There are several ways
 * to "track" a monster, including targetting it, attacking it, and
 * affecting it (and nobody else) with a ranged attack.  When nothing
 * is being tracked, we clear the health bar.  If the monster being
 * tracked is not currently visible, a special health bar is shown.
 */
static void health_redraw(void)
{
    /* Not tracking */
    if (!p_ptr->health_who)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking an unseen monster */
    else if (!mon_list[p_ptr->health_who].ml)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking a hallucinatory monster */
    else if (p_ptr->image)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking a dead monster (?) */
    else if (mon_list[p_ptr->health_who].hp <= 0)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking a visible monster */
    else
    {
        int len;
        int color;
        char buf[20];

        monster_type* m_ptr = &mon_list[p_ptr->health_who];

        /* Default to almost dead */
        byte attr = health_attr(m_ptr->hp, m_ptr->maxhp);

        /* Afraid */
        // if (m_ptr->stance == STANCE_FLEEING) attr = TERM_VIOLET;

        /* Convert into health bar (using ceiling for length) */
        len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;

        /* Default to "unknown" */
        Term_putstr(COL_INFO, ROW_INFO, 12, TERM_L_DARK, "  --------  ");

        /* Dump the current "health" (handle monster stunning, confusion) */

        if (m_ptr->confused && m_ptr->stunned)
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "cscscscs");
        else if (m_ptr->confused)
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "cccccccc");
        else if (m_ptr->stunned)
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "ssssssss");
        else
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "********");

        Term_erase(COL_INFO, ROW_INFO + 1, 12);

        if (!get_alertness_text(m_ptr, sizeof(buf), buf, &color))
            return;

        Term_putstr(COL_INFO + (13 - strlen(buf)) / 2, ROW_INFO + 1,
            MIN(strlen(buf), 12), color, buf);
    }
}

/*
 * Display basic info (mostly left of map)
 */
static void prt_frame_basic(void)
{
    int i;

    /* Name */
    if (strlen(op_ptr->full_name) <= 12)
    {
        prt_field(op_ptr->full_name, ROW_NAME, COL_NAME);
    }

    /* Small monospace health graphic under the name */
    prt_char_health_graphic();

    /* Level/Experience */
    prt_exp();

    /* All Stats */
    for (i = 0; i < A_MAX; i++)
        prt_stat(i);

    /* Hitpoints */
    prt_hp();

    /* Spellpoints */
    prt_sp();

    /* Light */
    prt_light();

    /* Melee */
    prt_mel();

    /* Archery */
    prt_arc();

    /* Quiver */
    prt_quiver();

    /* Evasion */
    prt_evn();

    /* Song */
    prt_song();

    /* Current depth */
    prt_depth();

    /* redraw monster health */
    health_redraw();
}

/*
 * Display extra info (mostly below map)
 */
static void prt_frame_extra(void)
{
    /* Stun */
    prt_stun();

    /* Food */
    prt_hunger();

    /* Various */
    prt_blind();
    prt_confused();
    prt_afraid();
    prt_poisoned();
    prt_cut();
    prt_terrain();

    /* State */
    prt_state();

    /* Speed */
    prt_speed();
}

/*
 * Hack -- display inventory in sub-windows
 */
static void fix_inven(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_INVEN)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display inventory */
        display_inven();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display monsters in sub-windows
 */
static void fix_monlist(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_MONLIST)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display visible monsters */
        display_monlist();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display combat rolls in sub-windows
 */
static void fix_combat_rolls(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_COMBAT_ROLLS)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display visible monsters */
        display_combat_rolls();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display equipment in sub-windows
 */
static void fix_equip(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_EQUIP)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display equipment */
        display_equip();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display player in sub-windows (mode 0)
 */
static void fix_player_0(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_PLAYER_0)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display player */
        display_player(0);

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display recent messages in sub-windows
 *
 * Adjust for width and split messages.  XXX XXX XXX
 */
static void fix_message(void)
{
    int j, i;
    int w, h;
    int x, y;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_MESSAGE)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Get size */
        Term_get_size(&w, &h);

        /* Dump messages */
        for (i = 0; i < h; i++)
        {
            byte color = message_color((s16b)i);

            /* Dump the message on the appropriate line */
            Term_putstr(0, (h - 1) - i, -1, color, message_str((s16b)i));

            /* Cursor */
            Term_locate(&x, &y);

            /* Clear to end of line */
            Term_erase(x, y, 255);
        }

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display monster recall in sub-windows
 */
static void fix_monster(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_MONSTER)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display monster race info */
        if (p_ptr->monster_race_idx)
            display_roff(p_ptr->monster_race_idx, NULL);

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Calculate maximum voice.
 *
 * This function induces status messages.
 */
extern void calc_voice(void)
{
    int msp;
    int i;
    int tmp;

    /* Get voice value */
    // 20 + a compounding 20% bonus per point of gra

    tmp = 20 * 100;

    if (p_ptr->stat_use[A_GRA] >= 0)
    {
        for (i = 0; i < p_ptr->stat_use[A_GRA]; i++)
        {
            tmp = tmp * 12 / 10;
        }
    }
    else
    {
        for (i = 0; i < -(p_ptr->stat_use[A_GRA]); i++)
        {
            tmp = tmp * 10 / 12;
        }
    }
    msp = tmp / 100;

    /* New maximum hitpoints */
    if (p_ptr->msp != msp)
    {
        int i = 100;

        /* Get percentage of maximum sp */
        if (p_ptr->msp)
            i = ((100 * p_ptr->csp) / p_ptr->msp);

        /* Save new limit */
        p_ptr->msp = msp;

        /* Update current maximum sp */
        p_ptr->csp = ((i * p_ptr->msp) / 100)
            + (((i * p_ptr->msp) % 100 >= 50) ? 1 : 0);

        /* Hack - any change in max voice resets frac */
        p_ptr->csp_frac = 0;

        /* Display sp later */
        p_ptr->redraw |= (PR_VOICE);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }

    /* Hack -- handle "xtra" mode */
    if (character_xtra)
        return;
}

/*
 * Calculate the player's (maximal) hit points
 *
 * Adjust current hitpoints if necessary
 *
 * Sil - modified substantially to reflect absence of chance and fixed bonus,
 * not per level
 */
static void calc_hitpoints(void)
{
    int mhp;
    int i;
    int tmp;

    /* Get hitpoint value */
    // 20 + a compounding 16% bonus per point of con, plus 5 HP flat bonus

    tmp = 20 * 100;
    if (p_ptr->stat_use[A_CON] >= 0)
    {
        for (i = 0; i < p_ptr->stat_use[A_CON]; i++)
        {
            tmp = tmp * 116 / 100;
        }
    }
    else
    {
        for (i = 0; i < -(p_ptr->stat_use[A_CON]); i++)
        {
            tmp = tmp * 100 / 116;
        }
    }
    mhp = tmp / 100 + 5;

    /* New maximum hitpoints */
    if (p_ptr->mhp != mhp)
    {
        int i = 100;

        /* Get percentage of maximum hp */
        if (p_ptr->mhp)
            i = ((100 * p_ptr->chp) / p_ptr->mhp);

        /* Save new limit */
        p_ptr->mhp = mhp;

        /* Update current maximum hp */
        p_ptr->chp = ((i * p_ptr->mhp) / 100)
            + (((i * p_ptr->mhp) % 100 >= 50) ? 1 : 0);

        /* Hack - any change in max hitpoint resets frac */
        p_ptr->chp_frac = 0;

        /* Display hp later */
        p_ptr->redraw |= (PR_HP);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }
}

/*
 * Determine the radius of possibly flickering lights
 */
int light_up_to(int base_radius, object_type* o_ptr)
{
    int radius = base_radius;
    u32b f1, f2, f3;

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    // Some lights flicker
    if (f2 & (TR2_DARKNESS))
    {
        while ((radius > -2) && one_in_(3))
        {
            radius--;
        }
    }
    else if (o_ptr->timeout < 100)
    {
        while ((radius > 0) && one_in_(3))
        {
            radius--;
        }
    }

    return (radius);
}

/*
 *  Determines how much an enemy in a given location should make the sword glow
 */
int hate_level(int y, int x, int multiplier)
{
    int dist;

    // check distance of monster from player (by noise)
    dist = flow_dist(FLOW_MONSTER_NOISE, y, x);

    // Avoid a division by zero
    if (dist == 0)
        dist = 1;

    // determine the danger level
    return ((50 * multiplier) / dist);
}

/*
 * Determine whether a melee weapon is glowing in response to nearby enemies
 */
bool weapon_glows(const object_type* o_ptr)
{
    int total_hate = 0;
    int i;
    int iy = o_ptr->iy; // weapon location
    int ix = o_ptr->ix;
    int py = p_ptr->py; // player location
    int px = p_ptr->px;
    int y, x; // generic location
    u32b f1, f2, f3;
    bool viewable = false;

    bool glows = false;

    if (!character_dungeon)
        return (false);

    // Must be a melee weapon
    if (wield_slot(o_ptr) != INVEN_WIELD)
        return (false);

    // use the player's position where needed
    if ((iy == 0) && (ix == 0))
    {
        iy = py;
        ix = px;
    }

    // out of LOS objects don't glow (or it can't be seen)
    if (cave_info[iy - 1][ix - 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy - 1][ix] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy - 1][ix + 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy][ix - 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy][ix] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy][ix + 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy + 1][ix - 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy + 1][ix] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy + 1][ix + 1] & (CAVE_VIEW))
        viewable = true;

    if (!viewable)
        return (false);

    // create a 'flow' around the object
    update_flow(iy, ix, FLOW_MONSTER_NOISE);

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Add up the total of creatures vulnerable to the weapon's slays */
    for (i = 1; i < mon_max; i++)
    {
        bool target = false;
        int multiplier = 1;
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        // Determine if a slay is applicable
        if ((f1 & (TR1_SLAY_WOLF)) && (r_ptr->flags3 & (RF3_WOLF)))
            target = true;
        if ((f1 & (TR1_SLAY_SPIDER)) && (r_ptr->flags3 & (RF3_SPIDER)))
            target = true;
        if ((f1 & (TR1_SLAY_UNDEAD)) && (r_ptr->flags3 & (RF3_UNDEAD)))
            target = true;
        if ((f1 & (TR1_SLAY_RAUKO)) && (r_ptr->flags3 & (RF3_RAUKO)))
            target = true;
        if ((f1 & (TR1_SLAY_ORC)) && (r_ptr->flags3 & (RF3_ORC)))
            target = true;
        if ((f1 & (TR1_SLAY_TROLL)) && (r_ptr->flags3 & (RF3_TROLL)))
            target = true;
        if ((f1 & (TR1_SLAY_DRAGON)) && (r_ptr->flags3 & (RF3_DRAGON)))
            target = true;
        // No glow for Morgoth's weapons that slay men and elves

        // skip inapplicable monsters
        if (!target)
            continue;

        // increase the effect for uniques
        if (r_ptr->flags1 & (RF1_UNIQUE))
            multiplier *= 2;

        // increase the effect for individually occuring creatures
        if (!(r_ptr->flags1 & (RF1_FRIENDS)) && !(r_ptr->flags1 & (RF1_FRIEND))
            && !(r_ptr->flags1 & (RF1_ESCORTS))
            && !(r_ptr->flags1 & (RF1_ESCORT)))
            multiplier *= 2;

        // add up the 'hate'
        total_hate += hate_level(m_ptr->fy, m_ptr->fx, multiplier);
    }

    /* Add a similar effect for very nearby webs for spider slaying wearpons */
    if (f1 & (TR1_SLAY_SPIDER))
    {
        for (y = (iy - 2); y <= (iy + 2); y++)
        {
            for (x = (ix - 2); x <= (ix + 2); x++)
            {
                if (in_bounds(y, x))
                {
                    // skip inapplicable squares
                    if (cave_feat[y][x] != FEAT_TRAP_WEB)
                        continue;

                    // add up the 'hate'
                    total_hate += hate_level(y, x, 1);
                }
            }
        }
    }

    if (total_hate >= 15)
        glows = true;

    return (glows);
}

/*
 * Extract and set the current "lite radius"
 */
void calc_torch(void)
{
    int i;
    object_type* o_ptr;
    u32b f1, f2, f3;
    int old_light;

    /* Store old value */
    old_light = p_ptr->cur_light;

    /* Assume no light */
    p_ptr->cur_light = 0;

    /* Loop through all wielded items */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip empty slots */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        /* Does this item glow? */
        if ((f2 & TR2_LIGHT) && (i != INVEN_LITE))
            p_ptr->cur_light++;

        /* Does this item create darkness? */
        if ((f2 & TR2_DARKNESS) && (i != INVEN_LITE))
            p_ptr->cur_light--;

        /* Examine actual light */
        if (o_ptr->tval == TV_LIGHT)
        {
            bool extinguished = false;

            /* Some items provide permanent, bright, light */
            if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)
                p_ptr->cur_light += RADIUS_LESSER_JEWEL;
            else if (o_ptr->sval == SV_LIGHT_FEANORIAN)
                p_ptr->cur_light += RADIUS_FEANORIAN;
            else if (o_ptr->sval == SV_LIGHT_SILMARIL)
                p_ptr->cur_light += RADIUS_SILMARIL;

            /* Torches (with fuel) provide some light */
            else if ((o_ptr->sval == SV_LIGHT_TORCH) && (o_ptr->timeout > 0))
            {
                p_ptr->cur_light += light_up_to(RADIUS_TORCH, o_ptr);
            }

            /* Lanterns (with fuel) provide more light */
            else if ((o_ptr->sval == SV_LIGHT_LANTERN) && (o_ptr->timeout > 0))
            {
                p_ptr->cur_light += light_up_to(RADIUS_LANTERN, o_ptr);
            }

            /* Mallorn torches (with fuel) provide even more light */
            else if ((o_ptr->sval == SV_LIGHT_MALLORN) && (o_ptr->timeout > 0))
            {
                p_ptr->cur_light += light_up_to(RADIUS_MALLORN, o_ptr);
            }

            else
            {
                extinguished = true;
            }

            if (!extinguished && (f2 & TR2_LIGHT))
            {
                p_ptr->cur_light++;
            }
        }
    }

    // increase radius when the player's weapon glows
    if (weapon_glows(&inventory[INVEN_WIELD]))
        p_ptr->cur_light++;
    if (weapon_glows(&inventory[INVEN_ARM]))
        p_ptr->cur_light++;

    /* Player is darkened */
    if (p_ptr->darkened && (p_ptr->cur_light > 0))
        p_ptr->cur_light--;

    // Smithing brightens the room a bit
    if (p_ptr->smithing)
        p_ptr->cur_light += 2;

    // Song of the trees
    if (singing(SNG_TREES))
    {
        p_ptr->cur_light += ability_bonus(S_SNG, SNG_TREES);
    }

    /* Oath of Light reward */
    if (p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] && !oath_invalid(OATH_LIGHT))
    {
        p_ptr->cur_light += 2;
    }

    /* Update the visuals */
    p_ptr->update |= (PU_UPDATE_VIEW);
    p_ptr->update |= (PU_MONSTERS);

    /* Apply light radius curses/blessings */
    {
        int r = curse_flag_count_cur(CUR_LIGHTR);

        /* radius penalty/bonus: +/-1 per stack, never below zero */
        if (r != 0)
            p_ptr->cur_light = MAX(0, p_ptr->cur_light - r);
    }

    /* Notice changes in the "lite radius" */
    if (old_light != p_ptr->cur_light)
    {
        /* Update the visuals */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
    }

    p_ptr->redraw |= (PR_LIGHT);
}

int affinity_level(int skilltype)
{
    int  level         = 0;
    u32b affinity_flag = 0L;
    u32b penalty_flag  = 0L;

    /* map skill → (affinity, penalty) pair */
    switch (skilltype)
    {
        case S_MEL: affinity_flag = RHF_MEL_AFFINITY; penalty_flag = RHF_MEL_PENALTY; break;
        case S_ARC: affinity_flag = RHF_ARC_AFFINITY; penalty_flag = RHF_ARC_PENALTY; break;
        case S_EVN: affinity_flag = RHF_EVN_AFFINITY; penalty_flag = RHF_EVN_PENALTY; break;
        case S_STL: affinity_flag = RHF_STL_AFFINITY; penalty_flag = RHF_STL_PENALTY; break;
        case S_PER: affinity_flag = RHF_PER_AFFINITY; penalty_flag = RHF_PER_PENALTY; break;
        case S_WIL: affinity_flag = RHF_WIL_AFFINITY; penalty_flag = RHF_WIL_PENALTY; break;
        case S_SMT: affinity_flag = RHF_SMT_AFFINITY; penalty_flag = RHF_SMT_PENALTY; break;
        case S_SNG: affinity_flag = RHF_SNG_AFFINITY; penalty_flag = RHF_SNG_PENALTY; break;
        default:    return 0;
    }

    /* race + character */
    if (rp_ptr->flags & affinity_flag) level++;
    if (current_character_profile->flags & affinity_flag) level++;
    if (rp_ptr->flags & penalty_flag)  level--;
    if (current_character_profile->flags & penalty_flag)  level--;

    /* every copy of the same curse flag */
    level += curse_flag_count_rhf(affinity_flag);
    level -= curse_flag_count_rhf(penalty_flag);

    /* keep inside the allowed range */
    if (level >  2) level =  2;
    if (level < -2) level = -2;

    if ((skilltype == S_WIL) && (current_character_profile->flags_u & UNQ_EARENDIL)) level = 3;

    return level;
}

/*
 * Calculate the minstrel bonus for song abilities.
 * Unlike affinity_level, this is uncapped and only affects ability costs.
 * It does not provide skill increases.
 */
int minstrel_level(void)
{
    int level = 0;

    /* Check for MINSTREL unique flag */
    if (current_character_profile->flags_u & UNQ_MINSTREL) level++;

    /* Include curse flags (similar to affinity) */
    level += curse_flag_count_rhf(RHF_SNG_AFFINITY);
    level -= curse_flag_count_rhf(RHF_SNG_PENALTY);

    /* No cap - can go beyond 2 */
    return level;
}

static bool songs_are_synergy_pair(byte song_a, byte song_b)
{
    static const byte synergy_pairs[][2] = {
        { SNG_ELBERETH,  SNG_TREES },
        { SNG_ELBERETH,  SNG_STAUNCHING },
        { SNG_CHALLENGE, SNG_SLAYING },
        { SNG_DELVINGS,  SNG_REVEALING },
        { SNG_FREEDOM,   SNG_ELVENESS },
        { SNG_STAYING,   SNG_CONTEST },
        { SNG_STAYING,   SNG_LAMENT },
        { SNG_SILENCE,   SNG_DISGUISE },
        { SNG_SILENCE,   SNG_LORIEN },
        { SNG_SHATTERING, SNG_MASTERY },
    };

    if ((song_a == SNG_NOTHING) || (song_b == SNG_NOTHING))
        return false;

    for (size_t i = 0; i < N_ELEMENTS(synergy_pairs); i++)
    {
        if ((song_a == synergy_pairs[i][0] && song_b == synergy_pairs[i][1])
            || (song_a == synergy_pairs[i][1] && song_b == synergy_pairs[i][0]))
        {
            return true;
        }
    }

    return false;
}

static int song_synergy_bonus(byte abilitynum, int full_skill)
{
    int synergy = 0;
    byte partner = SNG_NOTHING;

    if (full_skill <= 0)
        return 0;

    if (p_ptr->song1 == abilitynum)
        partner = p_ptr->song2;
    else if (p_ptr->song2 == abilitynum)
        partner = p_ptr->song1;
    else
        return 0;

    if (!songs_are_synergy_pair(abilitynum, partner))
        return 0;

    synergy = (full_skill + 2) / 5;

    return synergy;
}

int ability_bonus(int skilltype, int abilitynum)
{
    int bonus = 0;
    int skill = p_ptr->skill_use[skilltype];

    if (skilltype == S_SNG)
    {
        const int full_skill = skill;

        // penalize minor themes - check if this ability is the minor theme
        // UNLESS the character has the WOVEN_MASTER flag (Daeron)
        if ((p_ptr->song2 == abilitynum) && (p_ptr->song1 != abilitynum))
        {
            if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_WOVEN_MASTER))
                skill /= 2;
        }

        // Song of Silence dampens other songs when woven together
        // EXCEPT for Disguise and Lorien (its synergy pairs)
        // This dampening is applied BEFORE synergy bonus
        if (singing(SNG_SILENCE) && (abilitynum != SNG_SILENCE)
            && (abilitynum != SNG_DISGUISE) && (abilitynum != SNG_LORIEN))
        {
            // Calculate Silence bonus directly to avoid recursion
            int silence_skill = p_ptr->skill_use[S_SNG] / 2;
            int silence_penalty = silence_skill / 2;
            skill -= silence_penalty;
            if (skill < 0) skill = 0;
        }

        // woven theme synergy pairs grant an extra 20% of base song skill
        skill += song_synergy_bonus(abilitynum, full_skill);

        switch (abilitynum)
        {
        case SNG_ELBERETH:
        {
            bonus = skill;
            break;
        }
        case SNG_CHALLENGE:
        {
            bonus = skill;
            break;
        }
        case SNG_FREEDOM:
        {
            bonus = skill;
            break;
        }
        case SNG_STAUNCHING:
        {
            bonus = skill;
            break;
        }
        case SNG_SILENCE:
        {
            bonus = skill / 2;
            break;
        }
        case SNG_DELVINGS:
        {
            bonus = skill;
            break;
        }
        case SNG_REVEALING:
        {
            bonus = skill;
            break;
        }
        case SNG_THRESHOLDS:
        {
            bonus = skill;
            break;
        }
        case SNG_TREES:
        {
            bonus = skill / 5;
            break;
        }
        case SNG_ELVENESS:
        {
            bonus = skill;
            break;
        }
        case SNG_DISGUISE:
        {
            bonus = skill + 5;
            break;
        }
        case SNG_STAYING:
        {
            bonus = ((c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN) ? 2 : 1) * skill; 
            break;
        }
        case SNG_SLAYING:
        {
            bonus = ((c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_HURIN) ? 2 : 1) * skill * 2;
            break;
        }
        case SNG_LORIEN:
        {
            bonus = skill;
            break;
        }
        case SNG_MASTERY:
        {
            bonus = ((c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_THINGOL) ? 2 : 1) * skill;
            break;
        }
        case SNG_SHATTERING:
        {
            bonus = skill;
            break;
        }
        case SNG_CONTEST:
        {
            bonus = skill;
            break;
        }
        case SNG_LAMENT:
        {
            bonus = skill;
            break;
        }
        }

        // these bonuses are never negative
        if (bonus < 0)
            bonus = 0;
    }

    return (bonus);
}

/*
 * Computes current weight limit in tenths of pounds.
 *
 * 100 + a compounding 20% bonus per point of str
 */
int weight_limit(void)
{
    int i;
    int limit;

    limit = 1000;
    if (p_ptr->stat_use[A_STR] >= 0)
    {
        for (i = 0; i < p_ptr->stat_use[A_STR]; i++)
        {
            limit = limit * 12 / 10;
        }
    }
    else
    {
        for (i = 0; i < -(p_ptr->stat_use[A_STR]); i++)
        {
            limit = limit * 10 / 12;
        }
    }

    /* CUR_WEAK curse reduces weight limit by 20% per stack */
    /* Blessing increases weight limit by 20% per stack */
    int weak_stacks = curse_flag_count_cur(CUR_WEAK);
    if (weak_stacks > 0) {
        /* Curse: reduce by 20% per stack */
        for (i = 0; i < weak_stacks; i++) limit *= 0.8;
    } else if (weak_stacks < 0) {
        /* Blessing: increase by 20% per stack */
        for (i = 0; i < -weak_stacks; i++) limit *= 1.2;
    }

    /* Return the result */
    return (limit);
}

bool sprinting(void)
{
    int i;
    int turns = 1;

    if (p_ptr->active_ability[S_EVN][EVN_SPRINTING])
    {
        for (i = 1; i < 4; i++)
        {
            if ((p_ptr->previous_action[i] >= 1)
                && (p_ptr->previous_action[i] <= 9)
                && (p_ptr->previous_action[i] != 5))
            {
                if ((p_ptr->previous_action[i + 1] >= 1)
                    && (p_ptr->previous_action[i + 1] <= 9)
                    && (p_ptr->previous_action[i + 1] != 5))
                {
                    if (p_ptr->previous_action[i]
                        == p_ptr->previous_action[i + 1])
                    {
                        turns++;
                    }
                    else if (p_ptr->previous_action[i]
                        == cycle[chome[p_ptr->previous_action[i + 1]] - 1])
                    {
                        turns++;
                    }
                    else if (p_ptr->previous_action[i]
                        == cycle[chome[p_ptr->previous_action[i + 1]] + 1])
                    {
                        turns++;
                    }
                }
            }
        }
    }

    return (turns >= 4);
}

/* Calculate stats */
void calc_stats(void)
{
    for (int i = 0; i < A_MAX; i++)
    {
        /* Extract the new "stat_use" value for the stat */
        p_ptr->stat_use[i] = p_ptr->stat_base[i] + p_ptr->stat_equip_mod[i]
            + p_ptr->stat_drain[i] + p_ptr->stat_misc_mod[i];

        /* cap to -9 and 20 */
        if (p_ptr->stat_use[i] < BASE_STAT_MIN)
            p_ptr->stat_use[i] = BASE_STAT_MIN;
        else if (p_ptr->stat_use[i] > BASE_STAT_MAX)
            p_ptr->stat_use[i] = BASE_STAT_MAX;
    }
}

/*
 * Calculate the player's current "state", taking into account
 * not only race/character intrinsics, but also objects being worn
 * and temporary spell effects.
 *
 * See also calc_voice() and calc_hitpoints().
 *
 * The "weapon" and "bow" do *not* add to the bonuses to hit or to
 * damage, since that would affect non-combat things.  These values
 * are actually added in later, at the appropriate place.
 *
 * This function induces various "status" messages.
 */
static void calc_bonuses(void)
{
    int i, j;

    int old_speed;

    int old_telepathy;
    int old_see_inv;

    int old_mdd = p_ptr->mdd;
    int old_mds = p_ptr->mds;

    int old_mdd2 = p_ptr->mdd2;
    int old_mds2 = p_ptr->mds2;

    int old_add = p_ptr->add;
    int old_ads = p_ptr->ads;

    int new_p_min = p_min(GF_HURT, true);
    int new_p_max = p_max(GF_HURT, true);

    int old_stat_use[A_MAX];
    int old_stat_tmp_mod[A_MAX];

    int old_skill_use[S_MAX];

    object_type* o_ptr;

    u32b f1, f2, f3;

    int armour_weight = 0;

    // Remove off-hand weapons if you cannot wield them
    if (!p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON])
    {
        o_ptr = &inventory[INVEN_ARM];

        if ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM)
            || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING))
        {
            char o_name[80];

            /* Full object description */
            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

            /* Print the messages */
            msg_print("You can no longer wield both weapons.");

            // take it off
            do_cmd_takeoff(o_ptr, INVEN_ARM);
        }
    }

    /*** Memorize ***/

    /* Save the old speed */
    old_speed = p_ptr->pspeed;

    /* Save the old vision stuff */
    old_telepathy = p_ptr->telepathy;
    old_see_inv = p_ptr->see_inv;

    /* Save the old stats */
    for (i = 0; i < A_MAX; i++)
    {
        old_stat_use[i] = p_ptr->stat_use[i];
        old_stat_tmp_mod[i] = p_ptr->stat_drain[i];
    }

    /* Save the old skills */
    for (i = 0; i < S_MAX; i++)
    {
        old_skill_use[i] = p_ptr->skill_use[i];
    }

    /*** Reset ***/

    /* Reset player speed */
    p_ptr->pspeed = 2;

    /* Reset "fire" info */
    p_ptr->add = 0;
    p_ptr->ads = 0;
    p_ptr->ammo_tval = 0;

    /* Clear the equipment stat modifiers */
    for (i = 0; i < A_MAX; i++)
        p_ptr->stat_equip_mod[i] = 0;

    /* Clear the misc stat modifiers */
    for (i = 0; i < A_MAX; i++)
        p_ptr->stat_misc_mod[i] = 0;

    /* Clear the total values of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_use[i] = 0;

    /* Clear the stat modifiers of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_stat_mod[i] = 0;

    /* Clear the equipment modifiers of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_equip_mod[i] = 0;

    /* Clear the misc modifiers of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_misc_mod[i] = 0;

    /* Clear other bonuses */
    p_ptr->to_mdd = 0;
    p_ptr->to_mds = 0;
    p_ptr->mdd = 0;
    p_ptr->mds = 0;
    p_ptr->mdd2 = 0;
    p_ptr->mds2 = 0;
    p_ptr->offhand_mel_mod = 0;
    p_ptr->to_ads = 0;

    /* Clear all the flags */
    p_ptr->hunger = 0;
    p_ptr->danger = 0;
    p_ptr->aggravate = 0;
    p_ptr->cowardice = 0;
    p_ptr->haunted = 0;
    p_ptr->see_inv = 0;
    p_ptr->free_act = 0;
    p_ptr->stand_fast = 0;
    p_ptr->avoid_traps = 0;
    p_ptr->regenerate = 0;
    p_ptr->telepathy = 0;
    p_ptr->sustain_str = 0;
    p_ptr->sustain_con = 0;
    p_ptr->sustain_dex = 0;
    p_ptr->sustain_gra = 0;
    p_ptr->resist_fire = 1;
    p_ptr->resist_cold = 1;
    p_ptr->resist_pois = 1;
    p_ptr->resist_bleed = 0;
    p_ptr->resist_fear = 0;
    p_ptr->resist_blind = 0;
    p_ptr->resist_confu = 0;
    p_ptr->resist_stun = 0;
    p_ptr->resist_hallu = 0;

    /* Clear the item granted abilities */
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            /* For Special abilities skill, preserve quest-granted abilities */
            if (i == S_SPC) {
                /* Don't reset special abilities - they're not item-granted */
                continue;
            }
            p_ptr->have_ability[i][j] = p_ptr->innate_ability[i][j];
        }
    }

    /*** Extract race/character info ***/

    // Recalculate total weight
    p_ptr->total_weight = 0;
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];
        p_ptr->total_weight += o_ptr->number * o_ptr->weight;

        // *all* carried objects still cause danger
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & (TR2_DANGER))
            p_ptr->danger += 1;
    }
    p_ptr->total_weight += supplies_total_weight();

    /*** Analyze equipment ***/

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        bool is_quiver1 = (i == INVEN_QUIVER1);
        bool is_quiver2 = (i == INVEN_QUIVER2);
        bool is_throwing_item = player_can_treat_as_throwing_flags(o_ptr, f3);

        bool throwing_quiver = is_quiver2 && is_throwing_item;

        if (is_quiver1)
            continue;
        if (is_quiver2 && !is_throwing_item)
            continue;

        /* Affect stats */
        if (f1 & (TR1_STR))
            p_ptr->stat_equip_mod[A_STR] += o_ptr->pval;
        if (f1 & (TR1_DEX))
            p_ptr->stat_equip_mod[A_DEX] += o_ptr->pval;
        if (f1 & (TR1_CON))
            p_ptr->stat_equip_mod[A_CON] += o_ptr->pval;
        if (f1 & (TR1_GRA))
            p_ptr->stat_equip_mod[A_GRA] += o_ptr->pval;
        if (f1 & (TR1_NEG_STR))
            p_ptr->stat_equip_mod[A_STR] -= o_ptr->pval;
        if (f1 & (TR1_NEG_DEX))
            p_ptr->stat_equip_mod[A_DEX] -= o_ptr->pval;
        if (f1 & (TR1_NEG_CON))
            p_ptr->stat_equip_mod[A_CON] -= o_ptr->pval;
        if (f1 & (TR1_NEG_GRA))
            p_ptr->stat_equip_mod[A_GRA] -= o_ptr->pval;

        /* Affect skills */
        if (f1 & (TR1_MEL))
            p_ptr->skill_equip_mod[S_MEL] += o_ptr->pval;
        if (f1 & (TR1_ARC))
            p_ptr->skill_equip_mod[S_ARC] += o_ptr->pval;
        if (f1 & (TR1_STL))
            p_ptr->skill_equip_mod[S_STL] += o_ptr->pval;
        if (f1 & (TR1_PER))
            p_ptr->skill_equip_mod[S_PER] += o_ptr->pval;
        if (f1 & (TR1_WIL))
            p_ptr->skill_equip_mod[S_WIL] += o_ptr->pval;
        if (f1 & (TR1_SMT))
            p_ptr->skill_equip_mod[S_SMT] += o_ptr->pval;
        if (f1 & (TR1_SNG))
            p_ptr->skill_equip_mod[S_SNG] += o_ptr->pval;

        /* Affect Damage Sides */
        if (f1 & (TR1_DAMAGE_SIDES))
        {
            p_ptr->to_mds += o_ptr->pval;
            p_ptr->to_ads += o_ptr->pval;
        }

        /* Good flags */
        if (f2 & (TR2_SLOW_DIGEST))
            p_ptr->hunger -= 1;
        if (f2 & (TR2_REGEN))
            p_ptr->regenerate += 1;

        if (f2 & (TR2_SEE_INVIS))
        {
            (void)set_blind(0);
            p_ptr->see_inv += 1;
        }
        if (f2 & (TR2_FREE_ACT))
            p_ptr->free_act += 1;
        if (f2 & (TR2_SPEED))
        {
            p_ptr->pspeed += 1;
        }

        if (f3 & (TR3_STAND_FAST))
            p_ptr->stand_fast += 1;

        if (f3 & (TR3_AVOID_TRAPS))
            p_ptr->avoid_traps += 1;

        /* Bad flags */
        if (f2 & (TR2_HUNGER))
            p_ptr->hunger += 1;
        if (f2 & (TR2_SLOWNESS))
            p_ptr->pspeed -= 1;
        if (f2 & (TR2_AGGRAVATE))
            p_ptr->aggravate += 1;
        if (f2 & (TR2_FEAR))
            p_ptr->cowardice += 1;
        if (f2 & (TR2_HAUNTED))
            p_ptr->haunted += 1;

        // danger has already been handled in the general inventory
        // if (f2 & (TR2_DANGER)) p_ptr->danger += 1;

        // darkness and light are handled later...

        /* Resistance flags */
        if (f2 & (TR2_RES_COLD))
            p_ptr->resist_cold += 1;
        if (f2 & (TR2_RES_FIRE))
            p_ptr->resist_fire += 1;
        if (f2 & (TR2_RES_POIS))
            p_ptr->resist_pois += 1;

        if (f2 & (TR2_VUL_COLD))
            p_ptr->resist_cold -= 1;
        if (f2 & (TR2_VUL_FIRE))
            p_ptr->resist_fire -= 1;
        if (f2 & (TR2_VUL_POIS))
            p_ptr->resist_pois -= 1;

        if (f2 & (TR2_RES_BLEED))
            p_ptr->resist_bleed += 1;

        if (f2 & (TR2_RES_FEAR))
            p_ptr->resist_fear += 1;
        if (f2 & (TR2_RES_BLIND))
            p_ptr->resist_blind += 1;
        if (f2 & (TR2_RES_CONFU))
            p_ptr->resist_confu += 1;
        if (f2 & (TR2_RES_STUN))
            p_ptr->resist_stun += 1;
        if (f2 & (TR2_RES_HALLU))
            p_ptr->resist_hallu += 1;

        /* Sustain flags */
        if (f2 & (TR2_SUST_STR))
            p_ptr->sustain_str += 1;
        if (f2 & (TR2_SUST_DEX))
            p_ptr->sustain_dex += 1;
        if (f2 & (TR2_SUST_CON))
            p_ptr->sustain_con += 1;
        if (f2 & (TR2_SUST_GRA))
            p_ptr->sustain_gra += 1;

        // Parrying grants extra bonus for weapon evasion:
        if (p_ptr->active_ability[S_EVN][EVN_PARRY] && (i == INVEN_WIELD))
        {
            p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;
        }

        /* Add up the armour weight */
        if ((i >= INVEN_BODY) && (i <= INVEN_FEET))
            armour_weight += o_ptr->weight;

        // add the abilities
        int ability_count = o_ptr->abilities;
        for (j = 0; j < ability_count; j++)
        {
            p_ptr->have_ability[o_ptr->skilltype[j]][o_ptr->abilitynum[j]]
                = true;
        }

        /* Hack -- do not apply "melee" to-hit bonuses yet */
        if (i == INVEN_WIELD)
            continue;

        /* Hack -- do not apply "melee" to-hit bonuses yet */
        if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD))
            continue;

        /* Hack -- do not apply "bow" to-hit bonuses yet */
        if (i == INVEN_BOW)
            continue;

        /* Hack -- do not apply "arrow" to-hit bonuses at all */
        if (i == INVEN_QUIVER1)
            continue;
        if ((i == INVEN_QUIVER2) && !throwing_quiver)
            continue;

        /* Apply the bonus to hit */
        p_ptr->skill_equip_mod[S_MEL] += o_ptr->att;
        p_ptr->skill_equip_mod[S_ARC] += o_ptr->att;
        
        /* Apply the evasion bonus */
        p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;
    }

    /* Clear the old item granted abilities */
    for (i = 0; i < S_MAX; i++)
    {
        /* Skip special abilities - they persist once granted */
        if (i == S_SPC) continue;
        
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            if (!p_ptr->have_ability[i][j])
            {
                p_ptr->active_ability[i][j] = false;
            }
        }
    }

    /*** Most abilities ***/

    if (p_ptr->active_ability[S_MEL][MEL_STR])
        p_ptr->stat_misc_mod[A_STR]++;
    if (p_ptr->active_ability[S_ARC][ARC_DEX])
        p_ptr->stat_misc_mod[A_DEX]++;
    if (p_ptr->active_ability[S_EVN][EVN_DEX])
        p_ptr->stat_misc_mod[A_DEX]++;
    if (p_ptr->active_ability[S_STL][STL_DEX])
        p_ptr->stat_misc_mod[A_DEX]++;
    if (p_ptr->active_ability[S_PER][PER_GRA])
        p_ptr->stat_misc_mod[A_GRA]++;
    if (p_ptr->active_ability[S_WIL][WIL_CON])
        p_ptr->stat_misc_mod[A_CON]++;
    if (p_ptr->active_ability[S_SMT][SMT_GRA])
        p_ptr->stat_misc_mod[A_GRA]++;
    if (p_ptr->active_ability[S_SNG][SNG_GRA])
        p_ptr->stat_misc_mod[A_GRA]++;

    if (singing(SNG_ELVENESS))
        p_ptr->stat_misc_mod[A_GRA]++;

    if (p_ptr->active_ability[S_WIL][WIL_STRENGTH_IN_ADVERSITY])
    {
        // if <= 50% health, give a bonus to strength and grace
        if (health_level(p_ptr->chp, p_ptr->mhp) <= HEALTH_BADLY_WOUNDED)
        {
            p_ptr->stat_misc_mod[A_STR]++;
            p_ptr->stat_misc_mod[A_DEX]++;
            p_ptr->stat_misc_mod[A_GRA]++;
        }

        // if <= 25% health, give an extra bonus
        if (health_level(p_ptr->chp, p_ptr->mhp) <= HEALTH_ALMOST_DEAD)
        {
            p_ptr->stat_misc_mod[A_STR] += 2;
            p_ptr->stat_misc_mod[A_DEX] += 2;
            p_ptr->stat_misc_mod[A_GRA] += 2;
        }
    }

    /* Oath of Light: wearing shadowed gear immediately breaks the vow */
    if (p_ptr->oath_type == OATH_LIGHT && !oath_invalid(OATH_LIGHT))
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            object_type* o_ptr = &inventory[i];
            if (!o_ptr->k_idx) continue;
            
            u32b f1, f2, f3;
            object_flags(o_ptr, &f1, &f2, &f3);
            if ((f2 & TR2_DARKNESS) || (f3 & TR3_LIGHT_CURSE))
            {
                p_ptr->oaths_broken |= OATH_LIGHT_FLAG;
                p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] = false;
                apply_oath_breaking_curse(OATH_LIGHT);
                break;
            }
        }
    }

    /* Oath bonuses (granted by special oath abilities, disabled if oath is broken) */
    /* Apply dynamic oath bonuses based on oath.txt data */
    for (int oath_idx = 0; oath_idx < z_info->oath_max; oath_idx++)
    {
        oath_type *oath_ptr = &oath_info[oath_idx];
        
        /* Check if player has this oath and it's not broken */
        if (oath_ptr->oath_num >= OATH_MERCY && oath_ptr->oath_num <= OATH_LIGHT)
        {
            int special_ability = -1;
            
            /* Map oath number to special ability */
            switch(oath_ptr->oath_num)
            {
                case OATH_IRON: special_ability = SPC_OATH_IRON; break;
                case OATH_SILENCE: special_ability = SPC_OATH_SILENCE; break;
                case OATH_MERCY: special_ability = SPC_OATH_MERCY; break;
                case OATH_SMITH: special_ability = SPC_OATH_SMITH; break;
                case OATH_VALOROUS: special_ability = SPC_OATH_VALOROUS; break;
                case OATH_LIGHT: special_ability = SPC_OATH_LIGHT; break;
            }
            
            /* Apply bonuses if player has oath and it's not broken */
            if (special_ability >= 0 && 
                p_ptr->active_ability[S_SPC][special_ability] && 
                !oath_invalid(oath_ptr->oath_num))
            {
                /* Apply stat bonuses */
                p_ptr->stat_misc_mod[A_STR] += oath_ptr->stat_bonuses[0];
                p_ptr->stat_misc_mod[A_DEX] += oath_ptr->stat_bonuses[1];
                p_ptr->stat_misc_mod[A_CON] += oath_ptr->stat_bonuses[2];
                p_ptr->stat_misc_mod[A_GRA] += oath_ptr->stat_bonuses[3];
                
                /* Apply skill bonuses */
                if (oath_ptr->skill_type > 0 && oath_ptr->skill_type < S_MAX)
                {
                    p_ptr->skill_misc_mod[oath_ptr->skill_type] += oath_ptr->skill_bonus;
                }
            }
        }
    }

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        p_ptr->skill_misc_mod[S_MEL] -= 3;
    }

    if (p_ptr->active_ability[S_WIL][WIL_POISON_RESISTANCE])
    {
        p_ptr->resist_pois += 1;
    }

    /*** Temporary flags ***/

    /* Apply temporary "stun" */
    if (p_ptr->stun >= 50)
    {
        for (i = 0; i < S_MAX; i++)
        {
            p_ptr->skill_misc_mod[i] -= 4;
        }
    }
    else if (p_ptr->stun)
    {
        for (i = 0; i < S_MAX; i++)
        {
            p_ptr->skill_misc_mod[i] -= 2;
        }
    }

    /* Temporary "Rage" */
    if (p_ptr->rage)
    {
        p_ptr->stat_misc_mod[A_STR] += 1;
        p_ptr->stat_misc_mod[A_DEX] -= 1;
        p_ptr->stat_misc_mod[A_CON] += 1;
        p_ptr->stat_misc_mod[A_GRA] -= 1;
    }

    /* Temporary Strength */
    if (p_ptr->tmp_str)
    {
        p_ptr->stat_misc_mod[A_STR] += 3;
        p_ptr->sustain_str += 1;
    }

    /* Temporary Dexterity */
    if (p_ptr->tmp_dex)
    {
        p_ptr->stat_misc_mod[A_DEX] += 3;
        p_ptr->sustain_dex += 1;
    }

    /* Temporary Constitution */
    if (p_ptr->tmp_con)
    {
        p_ptr->stat_misc_mod[A_CON] += 3;
        p_ptr->sustain_con += 1;
    }

    /* Temporary Grace */
    if (p_ptr->tmp_gra)
    {
        p_ptr->stat_misc_mod[A_GRA] += 3;
        p_ptr->sustain_gra += 1;
    }

    /* Temporary "fast" */
    if (p_ptr->fast)
    {
        p_ptr->pspeed += 1;
    }

    /* Temporary "slow" */
    if (p_ptr->slow)
    {
        p_ptr->pspeed -= 1;
    }

    /* Temporary see invisible, resist blindness, and resist hallucination */
    if (p_ptr->tim_invis)
    {
        /* Hack */
        p_ptr->see_inv += 1;

        /* Hack */
        p_ptr->resist_blind += 1;

        /* Hack */
        p_ptr->resist_hallu += 1;
    }

    /* Weak with hunger */
    if (p_ptr->food < PY_FOOD_WEAK)
    {
        p_ptr->stat_misc_mod[A_STR] -= 1;
    }

    // 'Indomitable' ability provides resist_fear, resist_confusion,
    // resist_stunning and resist_hallucination as well as slowing hunger
    if (p_ptr->active_ability[S_WIL][WIL_INDOMITABLE])
    {
        p_ptr->resist_confu += 1;
        p_ptr->resist_fear += 1;
        p_ptr->resist_stun += 1;
        p_ptr->resist_hallu += 1;
        p_ptr->hunger -= 1;
    }

    /* Meta-run curses/blessings adjusting resistances */
    {
        int shift;

        shift = curse_flag_delta_cur(CUR_RES_FEAR_SHIFT);
        if (shift) p_ptr->resist_fear -= shift;

        shift = curse_flag_delta_cur(CUR_RES_STUN_SHIFT);
        if (shift) p_ptr->resist_stun -= shift;

        shift = curse_flag_delta_cur(CUR_RES_CONFU_SHIFT);
        if (shift) p_ptr->resist_confu -= shift;

        shift = curse_flag_delta_cur(CUR_RES_HALLU_SHIFT);
        if (shift) p_ptr->resist_hallu -= shift;

        shift = curse_flag_delta_cur(CUR_RES_POIS_SHIFT);
        if (shift) p_ptr->resist_pois -= shift;

        shift = curse_flag_delta_cur(CUR_RES_FIRE_SHIFT);
        if (shift) p_ptr->resist_fire -= shift;

        shift = curse_flag_delta_cur(CUR_RES_COLD_SHIFT);
        if (shift) p_ptr->resist_cold -= shift;
    }

    /* CUR_HUNGER curse/blessing: curse increases hunger, blessing decreases it */
    {
        int h = curse_flag_count_cur(CUR_HUNGER);
        if (h != 0) p_ptr->hunger += h;
    }

    // Mandos' Doom special ability grants immunity to fear, hallucination,
    // entrancement, rage, stun and confusion (implemented as high resistance + clear)
    if (p_ptr->have_ability[S_SPC][SPC_MANDOS]) {
        p_ptr->resist_fear += 100; // effectively immune
        p_ptr->resist_hallu += 100;
        p_ptr->resist_stun += 100;
        p_ptr->resist_confu += 100; // added confusion immunity
        log_trace("ABILITY DEBUG: Mandos' Doom active - granting mental immunities (fear+100, hallu+100, stun+100, confu+100). Total resist_confu: %d", p_ptr->resist_confu);
        // Clear timed effects each turn
        if (p_ptr->afraid) {
            (void)set_afraid(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared fear effect");
        }
        if (p_ptr->image) {
            p_ptr->image = 0;  // No set_image function found
            p_ptr->redraw |= (PR_MAP);  // Manually trigger redraw for hallucination
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared hallucination effect");
        }
        if (p_ptr->entranced) {
            (void)set_entranced(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared entrancement effect");
        }
        if (p_ptr->rage) {
            (void)set_rage(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared rage effect");
        }
        if (p_ptr->stun) {
            (void)set_stun(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared stun effect");
        }
        if (p_ptr->confused) {
            (void)set_confused(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared confusion effect");
        }
    } else {
        log_trace("ABILITY DEBUG: Mandos' Doom NOT active - have_ability[S_SPC][SPC_MANDOS] = %d", p_ptr->have_ability[S_SPC][SPC_MANDOS]);
    }

    // Helper function to calculate total monsters seen across all races
    int total_monsters_seen = 0;
    int total_monsters_killed = 0;
    int race_idx;
    for (race_idx = 0; race_idx < z_info->r_max; race_idx++) {
        monster_race *r_ptr = &r_info[race_idx];
        monster_lore *l_ptr = &l_list[race_idx];
        
        /* Skip non-monsters and unique monsters for mercy calculation */
        if (!r_ptr->name) continue;
        if (r_ptr->flags1 & RF1_UNIQUE) continue;
        
        total_monsters_seen += l_ptr->psights;
        total_monsters_killed += l_ptr->pkills;
    }

    // Niena's Gift of Mercy special ability grants enhanced stealth proportional to mercy shown
    if (p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY]) {
        if (total_monsters_seen > 0) {
            /* Calculate stealth bonus: 10*(seen-killed)/seen, rounded up */
            int mercy_ratio_times_10 = (10 * (total_monsters_seen - total_monsters_killed));
            int stealth_bonus = (mercy_ratio_times_10 + total_monsters_seen - 1) / total_monsters_seen; /* Ceiling division */
            
            if (stealth_bonus > 0) {
                p_ptr->skill_misc_mod[S_STL] += stealth_bonus;
                log_trace("ABILITY DEBUG: Niena's Gift of Mercy active - granting +%d stealth (global: seen=%d, killed=%d, ratio=%.2f)", 
                         stealth_bonus, total_monsters_seen, total_monsters_killed,
                         (float)(total_monsters_seen - total_monsters_killed) / total_monsters_seen);
            } else {
                log_trace("ABILITY DEBUG: Niena's Gift of Mercy active but no bonus (global: seen=%d, killed=%d)", 
                         total_monsters_seen, total_monsters_killed);
            }
        }
    }

    /*** Handle stats ***/
    calc_stats();

    /*** Analyze weight ***/

    /* Extract the current weight (in tenth pounds) */
    j = p_ptr->total_weight;

    /* Extract the "weight limit" (in tenth pounds) */
    i = weight_limit();

    /* Apply "encumbrance" from weight */
    if (j > i)
        p_ptr->pspeed -= 1;

    /* Stealth slows the player down (unless they are passing) */
    if (p_ptr->stealth_mode)
    {
        if (p_ptr->previous_action[0] != 5)
            p_ptr->pspeed -= 1;
        p_ptr->skill_misc_mod[S_STL] += STEALTH_MODE_BONUS;
    }

    if (p_ptr->rage)
    {
        p_ptr->skill_misc_mod[S_STL] -= 3;
    }

    /* Speed must lie between 1 and 4 */
    if (p_ptr->pspeed < 1)
        p_ptr->pspeed = 1;
    else if (p_ptr->pspeed > 4)
        p_ptr->pspeed = 4;

    /* Sprinting bonus: only applies if speed < 3, so it caps at 3 */
    if (sprinting())
    {
        if (p_ptr->pspeed < 3)
        {
            p_ptr->pspeed += 1;
        }
    }

    // Increase food consumption if actively regenerating
    if (p_ptr->regenerate
        && (p_ptr->chp < p_ptr->mhp || p_ptr->csp < p_ptr->msp))
    {
        p_ptr->hunger += 1;
    }

    /* armour weight (not inventory weight reduces stealth */
    /* by 1 point per 10 pounds (rounding down) */
    p_ptr->skill_equip_mod[S_STL] -= armour_weight / 100;

    // Penalise stealth based on song(s) being sung
    if (p_ptr->song1 != SNG_NOTHING)
    {
        int song_noise = 0;
        int song;

        for (i = 0; i < 2; i++)
        {
            if (i == 0)
                song = p_ptr->song1;
            else
                song = p_ptr->song2;

            switch (song)
            {
            case SNG_NOTHING:
                song_noise += 0;
                break;
            case SNG_ELBERETH:
                song_noise += 8;
                break;
            case SNG_CHALLENGE:
                song_noise += 12;
                break;
            case SNG_DELVINGS:
                song_noise += 4;
                break;
            case SNG_FREEDOM:
                song_noise += 4;
                break;
            case SNG_SILENCE:
                song_noise += 0;
                break;
            case SNG_STAUNCHING:
                song_noise += 4;
                break;
            case SNG_TREES:
                song_noise += 4;
                break;
            case SNG_ELVENESS:
                song_noise += 6;
                break;
            case SNG_DISGUISE:
                song_noise += 6;
                break;
            case SNG_THRESHOLDS:
                song_noise += 4;
                break;
            case SNG_STAYING:
                song_noise += 8;
                break;
            case SNG_SLAYING:
                song_noise += 8;
                break;
            case SNG_LORIEN:
                song_noise += 4;
                break;
            case SNG_MASTERY:
                song_noise += 8;
                break;
            }
        }

        // average the noise if there are two songs
        if (p_ptr->song2 != SNG_NOTHING)
            song_noise /= 2;

        p_ptr->skill_misc_mod[S_STL] -= song_noise;
    }

    /* Race/Character skill flags */
    p_ptr->skill_misc_mod[S_MEL] += affinity_level(S_MEL);
    p_ptr->skill_misc_mod[S_ARC] += affinity_level(S_ARC);
    p_ptr->skill_misc_mod[S_EVN] += affinity_level(S_EVN);
    p_ptr->skill_misc_mod[S_STL] += affinity_level(S_STL);
    p_ptr->skill_misc_mod[S_PER] += affinity_level(S_PER);
    p_ptr->skill_misc_mod[S_WIL] += affinity_level(S_WIL);
    p_ptr->skill_misc_mod[S_SMT] += affinity_level(S_SMT);
    p_ptr->skill_misc_mod[S_SNG] += affinity_level(S_SNG);

    /*** Modify skills by ability scores ***/

    /* Affect Skill -- melee (DEX) */
    p_ptr->skill_stat_mod[S_MEL] = p_ptr->stat_use[A_DEX];

    /* Affect Skill -- archery (DEX) */
    p_ptr->skill_stat_mod[S_ARC] = p_ptr->stat_use[A_DEX];

    /* Affect Skill -- evasion (DEX) */
    p_ptr->skill_stat_mod[S_EVN] = p_ptr->stat_use[A_DEX];

    /* Affect Skill -- stealth (DEX) */
    p_ptr->skill_stat_mod[S_STL] = p_ptr->stat_use[A_DEX];

    /* Affect Skill -- perception (GRA) */
    p_ptr->skill_stat_mod[S_PER] = p_ptr->stat_use[A_GRA];

    /* Affect Skill -- will (GRA) */
    p_ptr->skill_stat_mod[S_WIL] = p_ptr->stat_use[A_GRA];

    /* Affect Skill -- smithing (GRA) */
    p_ptr->skill_stat_mod[S_SMT] = p_ptr->stat_use[A_GRA];

    /* Affect Skill -- song (GRA) */
    p_ptr->skill_stat_mod[S_SNG] = p_ptr->stat_use[A_GRA];

    // Finalise song first as it modifies some other skills...
    p_ptr->skill_use[S_SNG] = p_ptr->skill_base[S_SNG]
        + p_ptr->skill_equip_mod[S_SNG] + p_ptr->skill_stat_mod[S_SNG]
        + p_ptr->skill_misc_mod[S_SNG];

    // Apply song effects that modify skills
    if (singing(SNG_ELVENESS))
    {
        int song_skill = ability_bonus(S_SNG, SNG_ELVENESS);
        p_ptr->skill_misc_mod[S_EVN] += 1 + song_skill / 7;
    }
    if (singing(SNG_STAYING))
    {
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN) p_ptr->skill_misc_mod[S_WIL] += ability_bonus(S_SNG, SNG_STAYING);
        else p_ptr->skill_misc_mod[S_WIL] += ability_bonus(S_SNG, SNG_STAYING) / 2;
    }
    if (singing(SNG_FREEDOM))
    {
        p_ptr->free_act += 1;
    }

    if (p_ptr->tmp_per)
    {
        p_ptr->skill_misc_mod[S_PER] += 10;
    }

    /*** Finalise all skills other than combat skills  (as bows/weapons must be
     * analysed first) ***/

    p_ptr->skill_use[S_STL] = p_ptr->skill_base[S_STL]
        + p_ptr->skill_equip_mod[S_STL] + p_ptr->skill_stat_mod[S_STL]
        + p_ptr->skill_misc_mod[S_STL];
    p_ptr->skill_use[S_PER] = p_ptr->skill_base[S_PER]
        + p_ptr->skill_equip_mod[S_PER] + p_ptr->skill_stat_mod[S_PER]
        + p_ptr->skill_misc_mod[S_PER];
    p_ptr->skill_use[S_WIL] = p_ptr->skill_base[S_WIL]
        + p_ptr->skill_equip_mod[S_WIL] + p_ptr->skill_stat_mod[S_WIL]
        + p_ptr->skill_misc_mod[S_WIL];
    p_ptr->skill_use[S_SMT] = p_ptr->skill_base[S_SMT]
        + p_ptr->skill_equip_mod[S_SMT] + p_ptr->skill_stat_mod[S_SMT]
        + p_ptr->skill_misc_mod[S_SMT];

    /*** Analyze current bow ***/

    /* Examine the "current bow" */
    o_ptr = &inventory[INVEN_BOW];

    p_ptr->skill_equip_mod[S_ARC] += o_ptr->att;

    /* Analyze launcher */
    // attack bonuses for those with bow proficiency
    p_ptr->skill_misc_mod[S_ARC] += bow_bonus(&inventory[INVEN_BOW]);

    if (o_ptr->k_idx)
    {
        p_ptr->ammo_tval = TV_ARROW;

        p_ptr->add = o_ptr->dd;
        p_ptr->ads = total_ads(o_ptr);

        /* set the archery skill (if using a bow) -- it gets set again later,
         * anyway
         */
        p_ptr->skill_use[S_ARC] = p_ptr->skill_base[S_ARC]
            + p_ptr->skill_equip_mod[S_ARC] + p_ptr->skill_stat_mod[S_ARC]
            + p_ptr->skill_misc_mod[S_ARC];
    }

    /*** Analyze melee weapon ***/

    /* Examine the "current melee weapon" */
    o_ptr = &inventory[INVEN_WIELD];

    // add the weapon's attack mod
    p_ptr->skill_equip_mod[S_MEL] += o_ptr->att;

    // add the weapon's evasion bonus (Parry ability grants this as extra bonus earlier)
    p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;

    // attack bonuses for matched weapon types
    p_ptr->skill_misc_mod[S_MEL] += axe_bonus(o_ptr) + polearm_bonus(o_ptr);

    // deal with the 'Versatility' ability
    if (p_ptr->active_ability[S_ARC][ARC_VERSATILITY]
        && (p_ptr->skill_base[S_ARC] > p_ptr->skill_base[S_MEL]))
    {
        p_ptr->skill_misc_mod[S_MEL]
            += (p_ptr->skill_base[S_ARC] - p_ptr->skill_base[S_MEL]) / 2;
    }

    /* generate the melee dice/sides from weapon, to_mdd, to_mds and strength */
    p_ptr->mdd = total_mdd(o_ptr);
    p_ptr->mds = total_mds(
        o_ptr, p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] ? -3 : 0);

    // determine the off-hand melee score, damage and sides
    if (p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]
        && (((&inventory[INVEN_ARM])->tval != TV_SHIELD)
            && ((&inventory[INVEN_ARM])->tval != 0)))
    {
        // remove main-hand specific bonuses
        p_ptr->offhand_mel_mod
            -= o_ptr->att + axe_bonus(o_ptr) + polearm_bonus(o_ptr);
        if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
            p_ptr->offhand_mel_mod += 3;

        // add off-hand specific bonuses
        o_ptr = &inventory[INVEN_ARM];
        p_ptr->offhand_mel_mod
            += o_ptr->att + axe_bonus(o_ptr) + polearm_bonus(o_ptr) - 3;

        // add off-hand weapon's evasion bonus
        p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;

        p_ptr->mdd2 = total_mdd(o_ptr);
        p_ptr->mds2 = total_mds(o_ptr, -3);
    }

    /* Meta-run curse adjusting melee damage sides */
    {
        int shift = curse_flag_delta_cur(CUR_MDS_SHIFT);
        if (shift != 0) {
            if (p_ptr->mds > 0) {
                int adjusted = p_ptr->mds - shift;
                if (adjusted < 1) adjusted = 1;
                p_ptr->mds = adjusted;
            }
            if (p_ptr->mds2 > 0) {
                int adjusted2 = p_ptr->mds2 - shift;
                if (adjusted2 < 1) adjusted2 = 1;
                p_ptr->mds2 = adjusted2;
            }
        }
    }

    /* Entrancement or being knocked out sets total evasion score to -5 */
    if (p_ptr->entranced || (p_ptr->stun > 100))
    {
        p_ptr->skill_misc_mod[S_EVN] = -5
            - (p_ptr->skill_base[S_EVN] + p_ptr->skill_equip_mod[S_EVN]
                + p_ptr->skill_stat_mod[S_EVN]);
    }

    /* finalise the combat and evasion skills */

    p_ptr->skill_use[S_MEL] = p_ptr->skill_base[S_MEL]
        + p_ptr->skill_equip_mod[S_MEL] + p_ptr->skill_stat_mod[S_MEL]
        + p_ptr->skill_misc_mod[S_MEL];
    p_ptr->skill_use[S_ARC] = p_ptr->skill_base[S_ARC]
        + p_ptr->skill_equip_mod[S_ARC] + p_ptr->skill_stat_mod[S_ARC]
        + p_ptr->skill_misc_mod[S_ARC];
    p_ptr->skill_use[S_EVN] = p_ptr->skill_base[S_EVN]
        + p_ptr->skill_equip_mod[S_EVN] + p_ptr->skill_stat_mod[S_EVN]
        + p_ptr->skill_misc_mod[S_EVN];

    /* Blows (melee attacks per round) and digging power */
    if (o_ptr->k_idx)
    {
        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);
    }

    /*** Notice changes ***/

    /* Analyze stats */
    for (i = 0; i < A_MAX; i++)
    {
        /* Notice changes */
        if (p_ptr->stat_drain[i] != old_stat_tmp_mod[i])
        {
            /* Redisplay the stats later */
            p_ptr->redraw |= (PR_STATS);

            /* Window stuff */
            p_ptr->window |= (PW_PLAYER_0);
        }

        /* Notice changes */
        if (p_ptr->stat_use[i] != old_stat_use[i])
        {
            /* Redisplay the stats later */
            p_ptr->redraw |= (PR_STATS);

            /* Window stuff */
            p_ptr->window |= (PW_PLAYER_0);
            /* Change in CON affects Hitpoints */
            if (i == A_CON)
            {
                p_ptr->update |= (PU_HP);
            }
        }
    }

    /* Recalculate voice needed */
    if (p_ptr->stat_use[A_GRA] != old_stat_use[A_GRA])
    {
        p_ptr->update |= (PU_MANA);
    }

    /* Hack -- Telepathy Change */
    if (p_ptr->telepathy != old_telepathy)
    {
        /* Update monster visibility */
        p_ptr->update |= (PU_MONSTERS);
    }

    /* Hack -- See Invis Change */
    if (p_ptr->see_inv != old_see_inv)
    {
        /* Update monster visibility */
        p_ptr->update |= (PU_MONSTERS);
    }

    /* Redraw speed (if needed) */
    if (p_ptr->pspeed != old_speed)
    {
        /* Redraw speed */
        p_ptr->redraw |= (PR_SPEED);
    }

    /* Always redraw terrain */
    p_ptr->redraw |= (PR_TERRAIN);

    /* Redraw melee (if needed) */
    if ((p_ptr->skill_use[S_MEL] != old_skill_use[S_MEL])
        || (p_ptr->mdd != old_mdd) || (p_ptr->mds != old_mds)
        || (p_ptr->mdd2 != old_mdd2) || (p_ptr->mds2 != old_mds2))
    {
        /* Redraw */
        p_ptr->redraw |= (PR_MEL);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }

    /* Redraw archery (if needed) */
    if ((p_ptr->skill_use[S_ARC] != old_skill_use[S_ARC])
        || (p_ptr->add != old_add) || (p_ptr->ads != old_ads))
    {
        /* Redraw */
        p_ptr->redraw |= (PR_ARC);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }

    /* Redraw armor */
    if ((p_ptr->skill_use[S_EVN] != old_skill_use[S_EVN])
        || (p_ptr->old_p_min != new_p_min) || (p_ptr->old_p_max != new_p_max))
    {
        /* Redraw */
        p_ptr->redraw |= (PR_ARMOR);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);

        p_ptr->old_p_min = new_p_min;
        p_ptr->old_p_max = new_p_max;
    }

    if (c_info[p_ptr->pcharacter].flags & RHF_MOR_CURSE) p_ptr->danger += 1;

    /* Hack -- handle "xtra" mode */
    if (character_xtra)
        return;

    // identify {special} items when the type has been seen before
    id_known_specials();
    reorder_pack(false);
}

/*
 * Handle "p_ptr->notice"
 */
void notice_stuff(void)
{
    /* Notice stuff */
    if (!p_ptr->notice)
        return;

    /* Combine the pack */
    if (p_ptr->notice & (PN_COMBINE))
    {
        p_ptr->notice &= ~(PN_COMBINE);
        combine_pack();
    }

    /* Reorder the pack */
    if (p_ptr->notice & (PN_REORDER))
    {
        p_ptr->notice &= ~(PN_REORDER);
        reorder_pack(true);
    }

    if (p_ptr->notice & PN_AUTOINSCRIBE)
    {
        p_ptr->notice &= ~(PN_AUTOINSCRIBE);
        autoinscribe_pack();
        autoinscribe_ground();
    }
}

bool player_auto_identifies_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    bool alchemy = p_ptr->active_ability[S_PER][PER_ALCHEMY]
        || p_ptr->have_ability[S_PER][PER_ALCHEMY];
    bool channeling = p_ptr->active_ability[S_WIL][WIL_CHANNELING]
        || p_ptr->have_ability[S_WIL][WIL_CHANNELING];
    bool jeweller = p_ptr->active_ability[S_SMT][SMT_JEWELLER]
        || p_ptr->have_ability[S_SMT][SMT_JEWELLER];
    bool enchantment = p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT]
        || p_ptr->have_ability[S_SMT][SMT_ENCHANTMENT];

    bool is_potion = (o_ptr->tval == TV_POTION);
    bool is_herb = (o_ptr->tval == TV_FOOD) && (o_ptr->sval <= SV_FOOD_SICKNESS);
    bool is_gem = (o_ptr->tval == TV_GEM);
    bool is_staff = (o_ptr->tval == TV_STAFF);
    bool is_horn = (o_ptr->tval == TV_HORN);
    bool is_jewellery = (o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET)
        || (o_ptr->tval == TV_LIGHT) || is_horn;

    if (alchemy && (is_potion || is_herb || is_gem))
        return true;

    if (channeling && (is_staff || is_horn))
        return true;

    if (jeweller && is_jewellery)
        return true;

    if (enchantment && !(is_potion || is_herb || is_gem))
        return true;

    return false;
}

/*
 * Helper function for update_lore()
 */
void update_lore_aux(object_type* o_ptr)
{
    // identify seen items
    if (!object_known_p(o_ptr))
    {
        if (player_auto_identifies_object(o_ptr))
        {
            ident(o_ptr);
        }
    }

    // Mark new identified artefacts/specials and gain experience for them
    if (object_known_p(o_ptr) && !p_ptr->leaving)
    {
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];
            char note[150];
            char shorter_desc[120];
            int new_exp;

            if (a_ptr->found_num == 0)
            {
                // mark
                a_ptr->found_num = 1;

                // gain experience for identification
                new_exp = 100;
                gain_exp(new_exp);
                p_ptr->ident_exp += new_exp;

                // display a note for new artefacts
                if ((o_ptr->name1 != ART_MORGOTH_2)
                    && (o_ptr->name1 != ART_MORGOTH_1)
                    && (o_ptr->name1 != ART_MORGOTH_0))
                {
                    /* Get a shorter description to fit the notes file */
                    object_desc(
                        shorter_desc, sizeof(shorter_desc), o_ptr, true, 0);

                    /* Build note and write */
                    if (o_ptr->xtra1 == p_ptr->depth)
                    {
                        sprintf(note, "Found %s", shorter_desc);
                    }
                    else
                    {
                        sprintf(note, "Found %s (from %d ft)", shorter_desc,
                            o_ptr->xtra1 * 50);
                    }

                    /* Record the depth where the artefact was identified */
                    do_cmd_note(note, p_ptr->depth);
                }
            }
        }

        else if (o_ptr->name2)
        {
            int new_exp;

            /* We now know about the special item type */
            e_info[o_ptr->name2].everseen = true;

            if (!(e_info[o_ptr->name2].aware))
            {
                // mark
                e_info[o_ptr->name2].aware = true;

                // gain experience for identification
                new_exp = 100;
                gain_exp(new_exp);
                p_ptr->ident_exp += new_exp;
            }
        }
    }
}

/*
 * This function does a few book keeping things for item identification.
 *
 * It identifies visible objects for the Lore-Keeper ability,
 * marks artefacts/specials as seen and grants experience for the first
 * sighting.
 */
void update_lore(void)
{
    int i;
    object_type* o_ptr;

    // Scan all dungeon objects that are 'seen' (in LOS and lit)
    for (i = 1; i < o_max; i++)
    {
        /* Get the next object from the dungeon */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        /* If the object is in sight, or under the player... */
        if ((cave_info[o_ptr->iy][o_ptr->ix] & (CAVE_SEEN))
            || ((p_ptr->py == o_ptr->iy) && (p_ptr->px == o_ptr->ix)))
        {
            update_lore_aux(o_ptr);
        }
    }

    // Scan the inventory / equipment
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        /* Get the next object from the inventory/equipment */
        o_ptr = &inventory[i];

        /* Skip empty objects */
        if (!o_ptr->k_idx)
            continue;

        update_lore_aux(o_ptr);
    }

    int supply_count = supplies_entry_count();
    for (int s_idx = 0; s_idx < supply_count; s_idx++)
    {
        object_type* supply_obj = supplies_entry_at(s_idx);
        if (!supply_obj || !supply_obj->k_idx)
            continue;

        update_lore_aux(supply_obj);
    }
}

/*
 * Handle "p_ptr->update"
 */
void update_stuff(void)
{
    update_lore();

    /* Update stuff */
    if (!p_ptr->update) {
        // log_trace("update_stuff: no updates needed");
        return;
    }

    log_debug("update_stuff: processing updates 0x%08X", p_ptr->update);

    if (p_ptr->update & (PU_BONUS))
    {
        p_ptr->update &= ~(PU_BONUS);
        // log_trace("update_stuff: calculating bonuses");
        calc_bonuses();
    }

    if (p_ptr->update & (PU_HP))
    {
        p_ptr->update &= ~(PU_HP);
        // log_trace("update_stuff: calculating hitpoints");
        calc_hitpoints();
    }

    if (p_ptr->update & (PU_MANA))
    {
        p_ptr->update &= ~(PU_MANA);
        // log_trace("update_stuff: calculating voice/mana");
        calc_voice();
    }

    /* Character is not ready yet, no screen updates */
    if (!character_generated) {
        // log_trace("update_stuff: character not generated yet, skipping screen updates");
        return;
    }

    // log_trace("update_stuff: character_icky=%d", character_icky);

    /* Character is in "icky" mode, no screen updates */
    if (character_icky) {
        // log_trace("update_stuff: character in icky mode (value=%d), skipping screen updates", character_icky);
        return;
    }

    if (p_ptr->update & (PU_FORGET_VIEW))
    {
        p_ptr->update &= ~(PU_FORGET_VIEW);
        log_debug("update_stuff: forgetting view");
        forget_view();
    }

    if (p_ptr->update & (PU_UPDATE_VIEW))
    {
        p_ptr->update &= ~(PU_UPDATE_VIEW);
        log_debug("update_stuff: updating view");
        update_view();
        
        /* Check artifact visibility after view update */
        check_artifact_visibility();
    }

    if (p_ptr->update & (PU_DISTANCE))
    {
        p_ptr->update &= ~(PU_DISTANCE);
        p_ptr->update &= ~(PU_MONSTERS);
        log_debug("update_stuff: updating distances and monsters");
        update_monsters(true);
    }

    if (p_ptr->update & (PU_MONSTERS))
    {
        p_ptr->update &= ~(PU_MONSTERS);
        update_monsters(false);
    }

    if (p_ptr->update & (PU_PANEL))
    {
        p_ptr->update &= ~(PU_PANEL);
        verify_panel();
    }

    /* Check quest completion status for metarun tracking */
    // log_trace("update_stuff: About to call metarun_check_and_update_quests()");
    metarun_check_and_update_quests();
    // log_trace("update_stuff: Finished calling metarun_check_and_update_quests()");

    // log_trace("update_stuff: completed all updates");
}

/*
 * Handle "p_ptr->redraw"
 */
void redraw_stuff(void)
{
    /* Redraw stuff */
    if (!p_ptr->redraw) {
        // log_trace("redraw_stuff: no redraws needed");
        return;
    }

    // log_trace("redraw_stuff: processing redraws 0x%08X", p_ptr->redraw);

    /* Character is not ready yet, no screen updates */
    if (!character_generated)
        return;

    // log_trace("redraw_stuff: character_icky=%d, character_generated=%s", 
            //   character_icky, character_generated ? "true" : "false");

    /* Character is in "icky" mode, no screen updates */
    if (character_icky && !p_ptr->is_dead) {
        // log_trace("redraw_stuff: character in icky mode (value=%d), skipping screen updates", character_icky);
        return;
    }

    if (p_ptr->redraw & (PR_MAP))
    {
        p_ptr->redraw &= ~(PR_MAP);
        log_debug("redraw_stuff: redrawing map");
        prt_map();
    }

    if (p_ptr->redraw & (PR_BASIC))
    {
        p_ptr->redraw &= ~(PR_BASIC);
        p_ptr->redraw &= ~(PR_STATS);
        p_ptr->redraw &= ~(PR_MEL | PR_EXP | PR_ARC | PR_QUIVER);
        p_ptr->redraw &= ~(PR_ARMOR | PR_HP | PR_VOICE | PR_SONG | PR_LIGHT);
        p_ptr->redraw &= ~(PR_DEPTH | PR_HEALTHBAR);
        p_ptr->redraw &= ~(PR_RESIST);
        prt_frame_basic();
    }

    if (p_ptr->redraw & (PR_MISC))
    {
        p_ptr->redraw &= ~(PR_MISC);

        /* Name */
        c_put_str(TERM_WHITE, "            ", ROW_NAME, COL_NAME);
        if (strlen(op_ptr->full_name) <= 12)
        {
            prt_field(op_ptr->full_name, ROW_NAME, COL_NAME);
        }
    }

    if (p_ptr->redraw & (PR_EXP))
    {
        p_ptr->redraw &= ~(PR_EXP);
        prt_exp();
    }

    if (p_ptr->redraw & (PR_STATS))
    {
        p_ptr->redraw &= ~(PR_STATS);
        prt_stat(A_STR);
        prt_stat(A_DEX);
        prt_stat(A_CON);
        prt_stat(A_GRA);
    }

    if (p_ptr->redraw & (PR_MEL))
    {
        p_ptr->redraw &= ~(PR_MEL);
        prt_mel();
    }

    if (p_ptr->redraw & (PR_ARC))
    {
        p_ptr->redraw &= ~(PR_ARC);
        prt_arc();
    }

    if (p_ptr->redraw & (PR_QUIVER))
    {
        p_ptr->redraw &= ~(PR_QUIVER);
        prt_quiver();
    }

    if (p_ptr->redraw & (PR_ARMOR))
    {
        p_ptr->redraw &= ~(PR_ARMOR);
        prt_evn();
    }

    if (p_ptr->redraw & (PR_HP))
    {
        p_ptr->redraw &= ~(PR_HP);
        prt_hp();

        /*
         * hack:  redraw player, since the player's color
         * now indicates approximate health.
         */
        if (arg_graphics == GRAPHICS_NONE)
        {
            lite_spot(p_ptr->py, p_ptr->px);
        }

        /* Also update the monospace character health graphic */
        prt_char_health_graphic();
    }

    if (p_ptr->redraw & (PR_VOICE))
    {
        p_ptr->redraw &= ~(PR_VOICE);
        prt_sp();
    }

    if (p_ptr->redraw & (PR_LIGHT))
    {
        p_ptr->redraw &= ~(PR_LIGHT);
        prt_light();
    }

    /* Sil - Hack: always redraw song (really should invent redraw flag for it
     * etc. */
    if (p_ptr->redraw & (PR_SONG))
    {
        p_ptr->redraw &= ~(PR_SONG);
        prt_song();
    }

    if (p_ptr->redraw & (PR_DEPTH))
    {
        p_ptr->redraw &= ~(PR_DEPTH);
        prt_depth();
    }

    if (p_ptr->redraw & (PR_HEALTHBAR))
    {
        p_ptr->redraw &= ~(PR_HEALTHBAR);
        health_redraw();
    }

    if (p_ptr->redraw & (PR_EXTRA))
    {
        p_ptr->redraw &= ~(PR_EXTRA);
        p_ptr->redraw &= ~(PR_CUT | PR_STUN);
        p_ptr->redraw &= ~(PR_HUNGER);
        p_ptr->redraw &= ~(PR_BLIND | PR_CONFUSED);
        p_ptr->redraw &= ~(PR_AFRAID | PR_POISONED);
        p_ptr->redraw &= ~(PR_STATE | PR_SPEED);
        prt_frame_extra();
    }

    if (p_ptr->redraw & (PR_CUT))
    {
        p_ptr->redraw &= ~(PR_CUT);
        prt_cut();
    }

    if (p_ptr->redraw & (PR_STUN))
    {
        p_ptr->redraw &= ~(PR_STUN);
        prt_stun();
    }

    if (p_ptr->redraw & (PR_HUNGER))
    {
        p_ptr->redraw &= ~(PR_HUNGER);
        prt_hunger();
    }

    if (p_ptr->redraw & (PR_BLIND))
    {
        p_ptr->redraw &= ~(PR_BLIND);
        prt_blind();
    }

    if (p_ptr->redraw & (PR_CONFUSED))
    {
        p_ptr->redraw &= ~(PR_CONFUSED);
        prt_confused();
    }

    if (p_ptr->redraw & (PR_AFRAID))
    {
        p_ptr->redraw &= ~(PR_AFRAID);
        prt_afraid();
    }

    if (p_ptr->redraw & (PR_POISONED))
    {
        p_ptr->redraw &= ~(PR_POISONED);
        prt_poisoned();
    }

    if (p_ptr->redraw & (PR_STATE))
    {
        p_ptr->redraw &= ~(PR_STATE);
        prt_state();
    }

    if (p_ptr->redraw & (PR_SPEED))
    {
        p_ptr->redraw &= ~(PR_SPEED);
        prt_speed();
    }

    if (p_ptr->redraw & (PR_TERRAIN))
    {
        p_ptr->redraw &= ~(PR_TERRAIN);
        prt_terrain();
    }

    // log_trace("redraw_stuff: completed all redraws");
}

/*
 * Handle "p_ptr->window"
 */
void window_stuff(void)
{
    int j;

    u32b mask = 0L;

    /* Nothing to do */
    if (!p_ptr->window) {
        // log_trace("window_stuff: no window updates needed");
        return;
    }

    log_debug("window_stuff: processing windows 0x%08X", p_ptr->window);

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Save usable flags */
        if (angband_term[j])
        {
            /* Build the mask */
            mask |= op_ptr->window_flag[j];
        }
    }

    /* Apply usable flags */
    p_ptr->window &= (mask);

    /* Nothing to do */
    if (!p_ptr->window)
        return;

    /* Display inventory */
    if (p_ptr->window & (PW_INVEN))
    {
        p_ptr->window &= ~(PW_INVEN);
        fix_inven();
    }

    /* Display monster list */
    if (p_ptr->window & (PW_MONLIST))
    {
        p_ptr->window &= ~(PW_MONLIST);
        fix_monlist();
    }

    /* Display equipment */
    if (p_ptr->window & (PW_EQUIP))
    {
        log_debug("window_stuff: PW_EQUIP flag set, calling fix_equip()");
        p_ptr->window &= ~(PW_EQUIP);
        fix_equip();
        log_debug("window_stuff: fix_equip() completed");
        
        /* Also trigger quiver redraw since quiver is part of equipment */
        p_ptr->redraw |= (PR_QUIVER);
    }

    /* Display player (mode 0) */
    if (p_ptr->window & (PW_PLAYER_0))
    {
        p_ptr->window &= ~(PW_PLAYER_0);
        fix_player_0();
    }

    /* Display combat rolls */
    if (p_ptr->window & (PW_COMBAT_ROLLS))
    {
        p_ptr->window &= ~(PW_COMBAT_ROLLS);
        fix_combat_rolls();
    }

    /* Display message recall */
    if (p_ptr->window & (PW_MESSAGE))
    {
        p_ptr->window &= ~(PW_MESSAGE);
        fix_message();
    }

    /* Display monster recall */
    if (p_ptr->window & (PW_MONSTER))
    {
        p_ptr->window &= ~(PW_MONSTER);
        fix_monster();
    }

    // log_trace("window_stuff: completed all window updates");
}

/*
 * Handle "p_ptr->update" and "p_ptr->redraw" and "p_ptr->window"
 */
void handle_stuff(void)
{
    log_trace("handle_stuff: starting (update=0x%08X, redraw=0x%08X, window=0x%08X)", 
              p_ptr->update, p_ptr->redraw, p_ptr->window);

    /* Update stuff */
    if (p_ptr->update)
        update_stuff();

    /* Redraw stuff */
    if (p_ptr->redraw)
        redraw_stuff();

    /* Window stuff */
    if (p_ptr->window)
        window_stuff();

    log_trace("handle_stuff: completed");
}




