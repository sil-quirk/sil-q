/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "ui/character-screen.h"
#include "fs/savefile-name.h"
#include "score/score_entry.h"
#include "ui/story_font.h"
#include "log/log.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include "externs.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define INSTRUCT_ROW 21
#define QUESTION_COL 2

static void display_skill(int skill, int row, int col)
{
    /* Enable story font for skill name (if enabled) */
    if (story_character_enabled()) {
        sdl_story_font_enable();
    }
    
    put_str(skill_names_full[skill], row, col);
    
    /* Disable story font - all numbers must use monospace */
    sdl_story_font_disable();
    
    /* All numbers in monospace font */
    c_put_str(
        TERM_L_GREEN, format("%3d", p_ptr->skill_use[skill]), row, col + 11);
    c_put_str(TERM_SLATE, "=", row, col + 15);
    c_put_str(
        TERM_GREEN, format("%2d", p_ptr->skill_base[skill]), row, col + 17);
    if (p_ptr->skill_stat_mod[skill] != 0)
        c_put_str(TERM_SLATE, format("%+3d", p_ptr->skill_stat_mod[skill]), row,
            col + 20);
    if (p_ptr->skill_equip_mod[skill] != 0)
        c_put_str(TERM_SLATE, format("%+3d", p_ptr->skill_equip_mod[skill]),
            row, col + 24);
    if (p_ptr->skill_misc_mod[skill] != 0)
        c_put_str(TERM_SLATE, format("%+3d", p_ptr->skill_misc_mod[skill]), row,
            col + 28);
}

static int display_player_standard_skill_row_override = -1;
static int display_player_standard_history_row_override = -1;
static bool display_player_standard_layout_override_active = false;

void display_player_standard_layout_set(int skill_row, int history_row)
{
    display_player_standard_layout_override_active = true;
    display_player_standard_skill_row_override = skill_row;
    display_player_standard_history_row_override = history_row;
}

void display_player_standard_layout_clear(void)
{
    display_player_standard_layout_override_active = false;
    display_player_standard_skill_row_override = -1;
    display_player_standard_history_row_override = -1;
}


/* ----- story-font aware helpers ---------------------------------------- */

/* ===== 20-column, right-anchored stat lines ============================= */

#define LINEW20 20
#define COMPACT_RIGHT_PAD 2

static int compact_right_column_start(int wid)
{
    int col = wid - COMPACT_RIGHT_PAD - LINEW20;
    if (col < 1)
        col = 1;
    return col;
}

static bool display_player_compact_tight_spacing(void)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    (void)wid;
    if (hgt < 1)
        hgt = 24;

    return (hgt <= 18);
}

static int display_player_compact_start_row(void)
{
    return display_player_compact_tight_spacing() ? 1 : 2;
}

static int display_player_compact_scroll = 0;
static int display_player_compact_max_scroll = 0;

void display_player_compact_set_scroll(int scroll)
{
    if (scroll < 0)
        scroll = 0;

    display_player_compact_scroll = scroll;
}

int display_player_compact_get_max_scroll(void)
{
    return display_player_compact_max_scroll;
}

static cptr display_player_song_name(byte song)
{
    cptr name;

    if (song == SNG_NOTHING)
        return "";

    name = b_name + (&b_info[ability_index(S_SNG, song)])->name;

    if (prefix(name, "Song of "))
        name += 8;

    return name;
}

static void put_label_fit(int x, int y, const char* label, int start)
{
    int maxw = start - x;
    if (maxw <= 0)
        return;

    char buf[64];
    strnfmt(buf, sizeof(buf), "%-*.*s", maxw, maxw, label);
    Term_putstr(x, y, -1, TERM_WHITE, buf);
}

static void format_tenths(char* buf, size_t buflen, long tenths)
{
    long whole;
    long frac;

    if (!buf || buflen == 0)
        return;

    if (tenths < 0)
        tenths = 0;

    whole = tenths / 10L;
    frac = tenths % 10L;
    strnfmt(buf, buflen, "%ld.%ld", whole, frac);
}

static byte burden_attr(void)
{
    return (p_ptr->total_weight <= weight_limit()) ? TERM_L_GREEN : TERM_YELLOW;
}

/* Pair: numbers block ends at x + LINEW20. cur_w + 1 + rhs_w == block width. */
static void put_pair20_right(int x, int y,
                             const char *label,
                             const char *cur,  int cur_w, byte col_cur,
                             char sep,
                             const char *rhs,  int rhs_w, byte col_rhs)
{
    int end   = x + LINEW20;
    int blk_w = cur_w + 1 + rhs_w;
    int start = end - blk_w;

    if (story_character_enabled())
        sdl_story_font_enable();

    put_label_fit(x, y, label, start);

    if (story_character_enabled())
        sdl_story_font_disable();

    /* Clear the numeric block so shorter values don't leave artifacts */
    Term_erase(start, y, blk_w);

    /* Trim both strings to their allotted widths */
    const char *cur_text = cur ? cur : "";
    int cur_len = (int)strlen(cur_text);
    if (cur_len > cur_w)
    {
        cur_text += cur_len - cur_w;
        cur_len = cur_w;
    }

    const char *rhs_text = rhs ? rhs : "";
    int rhs_len = (int)strlen(rhs_text);
    if (rhs_len > rhs_w)
    {
        rhs_text += rhs_len - rhs_w;
        rhs_len = rhs_w;
    }

    /* Right-align the combined "cur<sep>rhs" block as a whole so the slash
     * always hugs the digits while the entire string stays anchored to the
     * column edge. */
    int total_len = cur_len + 1 + rhs_len;
    if (total_len > blk_w)
        total_len = blk_w;
    int text_start = end - total_len;
    if (text_start < start)
        text_start = start;

    if (cur_len > 0)
        Term_putstr(text_start, y, cur_len, col_cur, cur_text);

    char s[2] = { sep, '\0' };
    Term_putstr(text_start + cur_len, y, 1, TERM_WHITE, s);

    if (rhs_len > 0)
        Term_putstr(text_start + cur_len + 1, y, rhs_len, col_rhs, rhs_text);
}

/* Single value: value block ends at x + LINEW20. */
static void put_single20_right(int x, int y,
                               const char *label,
                               const char *val, int val_w, byte col_val)
{
    int end   = x + LINEW20;
    int start = end - val_w;

    if (story_character_enabled())
        sdl_story_font_enable();

    put_label_fit(x, y, label, start);

    if (story_character_enabled())
        sdl_story_font_disable();

    Term_erase(start, y, val_w);
    const char *val_text = val ? val : "";
    int val_len = (int)strlen(val_text);
    if (val_len > val_w)
    {
        val_text += val_len - val_w;
        val_len = val_w;
    }

    if (val_len > 0)
    {
        int text_start = end - val_len;
        if (text_start < start)
            text_start = start;
        Term_putstr(text_start, y, val_len, col_val, val_text);
    }
}

static void put_single_right(int x, int y, int line_w,
                             const char* label,
                             const char* val, int val_w, byte col_val)
{
    int end;
    int start;

    if (line_w < 1)
        return;

    if (val_w > line_w - 1)
        val_w = line_w - 1;
    if (val_w < 1)
        return;

    end = x + line_w - 1;
    start = end - val_w + 1;

    if (story_character_enabled())
        sdl_story_font_enable();

    put_label_fit(x, y, label ? label : "", start);

    if (story_character_enabled())
        sdl_story_font_disable();

    Term_erase(start, y, val_w);
    const char* val_text = val ? val : "";
    int val_len = (int)strlen(val_text);
    if (val_len > val_w)
    {
        val_text += val_len - val_w;
        val_len = val_w;
    }

    if (val_len > 0)
    {
        int text_start = end - val_len + 1;
        if (text_start < start)
            text_start = start;
        Term_putstr(text_start, y, val_len, col_val, val_text);
    }
}

static byte format_deep_call_value(char* buf, size_t buflen, int max_width)
{
    int base_increment = 0;
    int total_increment = 0;
    int effective_total;
    char pct_buf[16];
    byte attr = TERM_L_GREEN;

    if (!buf || buflen == 0)
        return attr;

    buf[0] = '\0';
    if (max_width < 1)
        return attr;
    (void)max_width;

    min_depth_timer_status(&base_increment, NULL, &total_increment, NULL, NULL);

    effective_total = total_increment;
    if (effective_total < 0)
        effective_total = 0;

    if (base_increment > 0)
    {
        long pct = ((long)effective_total * 100L + (base_increment / 2))
            / base_increment;
        if (pct > 999L)
            pct = 999L;
        strnfmt(pct_buf, sizeof(pct_buf), "%ld%%", pct);
    }
    else if (effective_total > 0)
    {
        SDL_strlcpy(pct_buf, "INF%", sizeof(pct_buf));
    }
    else
    {
        SDL_strlcpy(pct_buf, "0%", sizeof(pct_buf));
    }

    if (base_increment <= 0)
        attr = (effective_total > 0) ? TERM_L_GREEN : TERM_YELLOW;
    else if (effective_total > base_increment)
        attr = TERM_L_GREEN;
    else if (effective_total == base_increment)
        attr = TERM_L_BLUE;
    else if (effective_total > 0)
        attr = TERM_YELLOW;
    else
        attr = TERM_L_RED;

    SDL_strlcpy(buf, pct_buf, buflen);

    return attr;
}

static bool format_min_depth_progress_bar(char* buf, size_t buflen, int line_w)
{
    int progress = 0;
    int threshold = 1;
    int bar_width;
    int filled;

    if (!buf || buflen == 0)
        return false;

    buf[0] = '\0';
    if (line_w < 12)
        return false;

    min_depth_timer_status(NULL, NULL, NULL, &progress, &threshold);
    if (threshold < 1)
        threshold = 1;
    if (progress < 0)
        progress = 0;
    if (progress > threshold)
        progress = threshold;

    bar_width = line_w - 2;
    if (bar_width > 32)
        bar_width = 32;
    if (bar_width < 8)
        return false;

    filled = (progress * bar_width) / threshold;
    if (filled < 0)
        filled = 0;
    if (filled > bar_width)
        filled = bar_width;

    if ((size_t)(bar_width + 3) > buflen)
        return false;

    buf[0] = '[';
    for (int i = 0; i < bar_width; i++)
        buf[i + 1] = (i < filled) ? '#' : '.';
    buf[bar_width + 1] = ']';
    buf[bar_width + 2] = '\0';
    return true;
}

static bool display_player_min_depth_progress_bar_line(int x, int y, int line_w)
{
    char bar_buf[96];
    int bar_len;
    int out_col;

    if (!format_min_depth_progress_bar(bar_buf, sizeof(bar_buf), line_w))
        return false;

    Term_erase(x, y, line_w);
    bar_len = (int)strlen(bar_buf);
    out_col = x + (line_w - bar_len) / 2;
    if (out_col < x)
        out_col = x;
    Term_putstr(out_col, y, bar_len, TERM_L_BLUE, bar_buf);
    return true;
}

static void display_player_deep_call_line(int x, int y, int line_w)
{
    const char* label = (line_w >= 16) ? "Deep Call" : "Call";
    int val_w = line_w - (int)strlen(label);
    char value_buf[96];
    byte value_attr;

    if (line_w < 6)
        return;

    if (val_w < 4)
    {
        label = "";
        val_w = line_w;
    }

    value_attr = format_deep_call_value(value_buf, sizeof(value_buf), val_w);
    put_single_right(x, y, line_w, label, value_buf, val_w, value_attr);
}

static void display_player_trait_putstr_fit(int col, int row, int max_width,
    byte attr, cptr text)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    if (row < 0 || row >= hgt)
        return;
    if (col < 0)
        col = 0;
    if (col >= wid)
        return;

    if (max_width <= 0 || col + max_width > wid)
        max_width = wid - col;
    if (max_width <= 0)
        return;

    Term_erase(col, row, max_width);
    Term_putstr(col, row, max_width, attr, text ? text : "");
}
/* ======================================================================= */

void display_player_xtra_info(int mode)
{
    int term_wid = 80;
    int term_hgt = 24;
    int wide_offset = 0;
    int col_stats;
    int col_flags;
    int col_skills;
    int flags_width;
    int skill_first_row = 6;
    int history_first_row = 15;
    bool compact_overview = (mode == 100);
    bool show_skills = !compact_overview;

    int row_stats = 2;
    int row_flags = 2;

    int skill;
    char cur[32], rhs[32], val[64], buf[160];

    byte history_attr = (mode == 2) ? TERM_YELLOW : TERM_WHITE;

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;

    if (display_player_standard_layout_override_active
        && display_player_standard_skill_row_override >= 0)
    {
        skill_first_row = display_player_standard_skill_row_override;
    }
    if (display_player_standard_layout_override_active)
    {
        history_first_row = display_player_standard_history_row_override;
    }

    if (term_wid > 80)
        wide_offset = (term_wid - 80) / 2;

    col_stats = wide_offset + 1;
    col_flags = wide_offset + 22;
    col_skills = wide_offset + 42;

    if (compact_overview)
        col_flags = col_stats + 21;

    flags_width = col_skills - col_flags - 1;
    if (flags_width < 1)
        flags_width = term_wid - col_flags;
    if (flags_width < 1)
        flags_width = 1;

    /* -------------------- STATS (col 1..20) ----------------------------- */

    /* Exp: cur(5)/max(6) */
    strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);   /* <= 99999 */
    strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);       /* <= 999999 */
    put_pair20_right(col_stats, row_stats++,
                     "Exp",
                     cur, 5, TERM_L_GREEN,
                     '/', rhs, 6, TERM_L_GREEN);

    /* Burden: cur/max in pounds with tenths */
    {
        long cur_b = (long)p_ptr->total_weight;
        long max_b = (long)weight_limit();
        format_tenths(cur, sizeof(cur), cur_b);
        format_tenths(rhs, sizeof(rhs), max_b);
        put_pair20_right(col_stats, row_stats++,
                         "Burden",
                         cur, 6, burden_attr(),
                         '/', rhs, 6, TERM_L_GREEN);
    }

    /* Depth: current / minimum you can return to.
       Use label "Depth c/m", numeric block %4ld/%4ld, max 1000 each. */
    if (turn > 0)
    {
        long cur_d = (long)(p_ptr->depth * 50);    /* <= 1000 */
        long min_d = (long)(min_depth() * 50);

        if (cur_d > 1000) cur_d = 1000;
        if (min_d > 1000) min_d = 1000;

        strnfmt(cur, sizeof(cur), "%ld", cur_d);       /* 4 */
        strnfmt(rhs, sizeof(rhs), "%ld", min_d);       /* 4 */

        put_pair20_right(col_stats, row_stats++,
                         "Depth c/m",
                         cur, 4, (cur_d >= min_d) ? TERM_L_GREEN : TERM_YELLOW,
                         '/', rhs, 4, TERM_L_GREEN);

        if (display_player_min_depth_progress_bar_line(col_stats, row_stats, LINEW20))
            row_stats++;
    }

    display_player_deep_call_line(col_stats, row_stats++, LINEW20);

    /* Turn (commas ok), right-anchored 12 */
    comma_number(buf, playerturn);
    put_single20_right(col_stats, row_stats++,
                       "Turn", buf, 12, TERM_L_GREEN);

    /* Light */
    strnfmt(val, sizeof(val), "%d", p_ptr->cur_light);
    put_single20_right(col_stats, row_stats++,
                       "Light", val, 2, TERM_L_GREEN);

    /* Melee main-hand - keep () */
    strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
    put_single20_right(col_stats, row_stats++,
                       "Melee", val, 12, TERM_L_BLUE);

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        put_single20_right(col_stats, row_stats++,
                           "Melee x2", val, 12, TERM_L_BLUE);
    }

    /* Offhand if present */
    if (p_ptr->mds2 > 0)
    {
        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
                p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
                p_ptr->mdd2, p_ptr->mds2);
        put_single20_right(col_stats, row_stats++,
                           "Offhand", val, 12, TERM_L_BLUE);
    }

    /* Bows */
    strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
    put_single20_right(col_stats, row_stats++,
                       "Bows", val, 12, TERM_L_BLUE);

    /* Armor - keep [] */
    strnfmt(val, sizeof(val), "[%+d,%d-%d]",
            p_ptr->skill_use[S_EVN], p_min(GF_HURT, true), p_max(GF_HURT, true));
    put_single20_right(col_stats, row_stats++,
                       "Armor", val, 12, TERM_L_BLUE);

    /* Health: 3/3, clamp to 999 */
    {
        int chp = p_ptr->chp; if (chp > 999) chp = 999;
        int mhp = p_ptr->mhp; if (mhp > 999) mhp = 999;
        strnfmt(cur, sizeof(cur), "%d", chp);
        strnfmt(rhs, sizeof(rhs), "%d", mhp);
        put_pair20_right(col_stats, row_stats++,
                         "Health",
                         cur, 3, TERM_L_BLUE,
                         '/', rhs, 3, TERM_L_BLUE);
    }

    /* Voice: 3/3, clamp to 999 */
    {
        int csp = p_ptr->csp; if (csp > 999) csp = 999;
        int msp = p_ptr->msp; if (msp > 999) msp = 999;
        strnfmt(cur, sizeof(cur), "%d", csp);
        strnfmt(rhs, sizeof(rhs), "%d", msp);
        put_pair20_right(col_stats, row_stats++,
                         "Voice",
                         cur, 3, TERM_L_BLUE,
                         '/', rhs, 3, TERM_L_BLUE);
    }

    /* Songs (optional) */
    if (p_ptr->song1 != SNG_NOTHING) {
        strnfmt(val, sizeof(val), "%s", display_player_song_name(p_ptr->song1));
        put_single20_right(col_stats, row_stats++,
                           "Song", val, 14, TERM_L_BLUE);
    }
    if (p_ptr->song2 != SNG_NOTHING) {
        strnfmt(val, sizeof(val), "%s", display_player_song_name(p_ptr->song2));
        put_single20_right(col_stats, row_stats++,
                           "Song", val, 14, TERM_L_BLUE);
    }

    /* -------------------- FLAGS (single column at col 22) ---------------- */

    int race  = p_ptr->prace;
    int character = p_ptr->pcharacter;

    byte attr_affinity   = TERM_GREEN;   /* AF */
    byte attr_mastery    = TERM_L_GREEN; /* MA */
    byte attr_penalty    = TERM_RED;     /* PE */
    byte attr_gr_penalty = TERM_L_RED;   /* GP */

    typedef struct {
        const char *txt;
        byte col;
    } line_t;

    line_t uniq_buf[32], ma_buf[16], af_buf[16], pen_buf[32];
    int uniq_n = 0, ma_n = 0, af_n = 0, pen_n = 0;

#define PUSH(arr, n, text, color) do { (arr)[(n)].txt = (text); (arr)[(n)++].col = (color); } while (0)

#define HANDLE_SKILL_EX(LABEL, AFF_FLAG, PEN_FLAG)                                      \
    do {                                                                                \
        int score = 0;                                                                  \
        if (p_info[race].flags      & (AFF_FLAG)) score++;                              \
        if (c_info[character].flags & (AFF_FLAG)) score++;                              \
        if (p_info[race].flags      & (PEN_FLAG)) score--;                              \
        if (c_info[character].flags & (PEN_FLAG)) score--;                              \
        score += curse_flag_count_rhf(AFF_FLAG);                                        \
        score -= curse_flag_count_rhf(PEN_FLAG);                                        \
        if (score >  2) score =  2;                                                     \
        if (score < -2) score = -2;                                                     \
        if (score ==  2)      PUSH(ma_buf,  ma_n,  LABEL "++", attr_mastery);          \
        else if (score == 1)  PUSH(af_buf,  af_n,  LABEL "+ ", attr_affinity);         \
        else if (score == -1) PUSH(pen_buf, pen_n, LABEL "- ", attr_penalty);          \
        else if (score == -2) PUSH(pen_buf, pen_n, LABEL "--", attr_gr_penalty);       \
    } while (0)

#define HANDLE_UNIQUE(LABEL, FLAG, COLOR)                                               \
    do {                                                                                \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG)))        \
            PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                                   \
    } while (0)

#define HANDLE_UNIQUE_U(LABEL, FLAG, COLOR)                                             \
    do {                                                                                \
        if (c_info[character].flags_u & (FLAG))                                         \
            PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                                   \
    } while (0)

    /* Skills */
    HANDLE_SKILL_EX("melee",      RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    HANDLE_SKILL_EX("evasion",    RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    HANDLE_SKILL_EX("stealth",    RHF_STL_AFFINITY, RHF_STL_PENALTY);
    HANDLE_SKILL_EX("archery",    RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    HANDLE_SKILL_EX("will",       RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    HANDLE_SKILL_EX("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    HANDLE_SKILL_EX("smithing",   RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    HANDLE_SKILL_EX("song",       RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    HANDLE_SKILL_EX("bow",        RHF_BOW_PROFICIENCY, 0);
    HANDLE_SKILL_EX("axe",        RHF_AXE_PROFICIENCY, 0);

    /* Uniques (all into one buffer; they'll print first) */
    HANDLE_UNIQUE_U("Master Artisan",     UNQ_SMT_FEANOR,   TERM_VIOLET);
    HANDLE_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Chosen of Ulmo",     UNQ_WIL_TUOR,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Indomitable Will",   UNQ_EARENDIL,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Oromë Himself",      UNQ_WIL_FIN,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Songs of Power",     UNQ_SNG_FIN,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Elven Dance",        UNQ_SNG_LUT,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Girdle of Melian",   UNQ_SNG_MEL,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR,  TERM_VIOLET);
    HANDLE_UNIQUE_U("Old Master",         UNQ_SMT_GAMIL,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Ring Master",        UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    HANDLE_UNIQUE_U("Aure entuluva",      UNQ_SNG_HURIN,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Voice of the Girdle",UNQ_SNG_THINGOL,  TERM_VIOLET);
    HANDLE_UNIQUE_U("Forgotten",          UNQ_MIM,          TERM_VIOLET);
    HANDLE_UNIQUE_U("One Handed",         UNQ_MEL_MAEDHROS, TERM_VIOLET);
    HANDLE_UNIQUE_U("Agarwaen",           UNQ_WIL_TURIN,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Shadow Walker",      UNQ_SNG_TURGON,   TERM_VIOLET);
    HANDLE_UNIQUE_U("Minstrel",           UNQ_MINSTREL,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Woven Master",       UNQ_WOVEN_MASTER, TERM_VIOLET);
    HANDLE_UNIQUE("Gift of Eru",          RHF_GIFTERU,      TERM_VIOLET);
    HANDLE_UNIQUE("Seafarer",             RHF_FREE,         TERM_VIOLET);

    HANDLE_UNIQUE("Kinslayer",            RHF_KINSLAYER,    TERM_UMBER);
    HANDLE_UNIQUE("Treacherous",          RHF_TREACHERY,    TERM_UMBER);
    HANDLE_UNIQUE("Doom of Mandos",       RHF_CURSE,        TERM_UMBER);
    HANDLE_UNIQUE("Morgoth Curse",        RHF_MOR_CURSE,    TERM_UMBER);

    /* Render: uniques -> MA -> AF -> penalties (use story font if enabled) */
    if (story_character_enabled()) {
        sdl_story_font_enable();
    }
    
    for (int i = 0; i < uniq_n; ++i)
        display_player_trait_putstr_fit(col_flags, row_flags++, flags_width,
            uniq_buf[i].col, uniq_buf[i].txt);
    for (int i = 0; i < ma_n; ++i)
        display_player_trait_putstr_fit(col_flags, row_flags++, flags_width,
            ma_buf[i].col, ma_buf[i].txt);
    for (int i = 0; i < af_n; ++i)
        display_player_trait_putstr_fit(col_flags, row_flags++, flags_width,
            af_buf[i].col, af_buf[i].txt);
    for (int i = 0; i < pen_n; ++i)
        display_player_trait_putstr_fit(col_flags, row_flags++, flags_width,
            pen_buf[i].col, pen_buf[i].txt);

    /* Disable story font after rendering flags/abilities */
    if (story_character_enabled()) {
        sdl_story_font_disable();
    }

    /* -------------------- SKILLS ---------------------------------------- */
    if (show_skills)
    {
        int skill_row = 0;

        /* Skills will manage their own font switching */
        for (skill = 0; skill < S_MAX; skill++) {
            /* Skip Special abilities skill - not meant for display */
            if (skill == S_SPC) continue;
            if (skill_first_row + skill_row < term_hgt)
                display_skill(skill, skill_first_row + skill_row, col_skills);
            skill_row++;
        }
    }

    /* -------------------- History (unchanged) --------------------------- */
    if (story_character_enabled()) {
        sdl_story_font_enable();
        /* Render the character description/history in the secondary story font. */
        sdl_story_font_set_slot(STORY_FONT_SLOT_SECONDARY);
    }

    /* Use full terminal width for history wrapping */
    log_debug("Character history: terminal width=%d, using wrap=%d", term_wid, term_wid - 1);
    if (history_first_row >= 0 && history_first_row < term_hgt)
    {
        text_out_wrap   = term_wid - 1;  /* Leave 1 column margin */
        text_out_indent = 1;
        Term_gotoxy(text_out_indent, history_first_row);
        text_out_to_screen(history_attr, p_ptr->history);
    }
    text_out_wrap   = 0;
    text_out_indent = 0;
    
    if (story_character_enabled()) {
        sdl_story_font_disable();
    }

#undef HANDLE_SKILL_EX
#undef HANDLE_UNIQUE
#undef HANDLE_UNIQUE_U
#undef PUSH
}



/*
 * Equippy chars
 */
static void display_player_equippy(int y, int x)
{
    int i;

    byte a;
    char c;

    object_type* o_ptr;

    /* Dump equippy chars */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; ++i)
    {
        /* Object */
        o_ptr = &inventory[i];

        /* Skip empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Get attr/char for display */
        a = object_attr(o_ptr);
        c = object_char(o_ptr);

        /* Dump */
        Term_putch(x + i - INVEN_WIELD, y, a, c);
    }
}

/*
 * Hack -- see below
 */
static const byte display_player_flag_set[4] = { 1, 2, 2, 1 };

/*
 * Hack -- see below
 */
static const u32b display_player_flag_head[4]
    = { TR1_MEL, TR2_RES_COLD, TR2_SLOW_DIGEST, TR1_SLAY_ORC };

/*
 * Hack -- see below
 */
static cptr display_player_flag_names[4][9]
    = { { "  Mel:", "  Arc:", "  Stl:", "  Per:", "  Wil:", "  Smt:", "  Sng:",
            "#####:", "#####:" },

          {
              " Cold:",
              " Fire:",
              " Elec:",
              " Pois:",
              " Dark:",
              " Fear:",
              "Blind:",
              " Conf:",
              " Stun:",
          },

          { "Sustn:", /* TR2_SLOW_DIGEST */
              "Light:", "Regen:", "Invis:", " Free:", "#####:", "Speed:",
              "#####:", "#####:" },

          { "  Orc:", "Troll:", " Wolf:", "Spidr:", " Undd:", "Rauko:",
              "Dragn:", "#####:", "#####:" } };

/*
 * Special display, part 1
 */
static void display_player_flag_info(void)
{
    int x, y, i, n;

    int row, col;

    int set;
    u32b head;
    u32b flag;
    cptr name;

    u32b f[4];

    sdl_story_font_enable();

    /* Four columns */
    for (x = 0; x < 4; x++)
    {
        /* Reset */
        row = 9;
        col = 20 * x - 2;

        /* Header */
        c_put_str(TERM_WHITE, "abcdefghijkl@", row++, col + 8);

        /* Nine rows */
        for (y = 0; y < 9; y++)
        {
            byte name_attr = TERM_WHITE;

            /* Extract set */
            set = display_player_flag_set[x];

            /* Extract head */
            head = display_player_flag_head[x];

            /* Extract flag */
            flag = (head << y);

            /* Extract name */
            name = display_player_flag_names[x][y];

            /* Check equipment */
            for (n = 8, i = INVEN_WIELD; i < INVEN_TOTAL; ++i, ++n)
            {
                byte attr = TERM_SLATE;

                object_type* o_ptr;

                /* Object */
                o_ptr = &inventory[i];

                /* Known flags */
                object_flags_known(o_ptr, &f[1], &f[2], &f[3]);

                /* Color columns by parity */
                if (i % 2)
                    attr = TERM_L_WHITE;

                /* Non-existant objects */
                if (!o_ptr->k_idx)
                    attr = TERM_L_DARK;

                /* Check flags */
                if (f[set] & flag)
                {
                    c_put_str(TERM_L_BLUE, "+", row, col + n);
                    if (name_attr != TERM_L_GREEN)
                        name_attr = TERM_L_BLUE;
                }

                /* Default */
                else
                {
                    c_put_str(attr, ".", row, col + n);
                }
            }

            /* Default */
            c_put_str(TERM_SLATE, ".", row, col + n);

            /* Check flags */
            if (f[set] & flag)
            {
                c_put_str(TERM_L_BLUE, "+", row, col + n);
                if (name_attr != TERM_L_GREEN)
                    name_attr = TERM_L_BLUE;
            }

            /* Header */
            c_put_str(name_attr, name, row, col + 2);

            /* Advance */
            row++;
        }

        /* Footer */
        c_put_str(TERM_WHITE, "abcdefghijkl@", row++, col + 8);

        /* Equippy */
        display_player_equippy(row++, col + 8);
    }

    sdl_story_font_disable();
}

/*
 * The first-run tutorial is now a guided coach drawn over the real character
 * screens (selection -> attributes -> skills).  From the live character sheet
 * it shows the sheet's own callouts.  See birth_coach_show /
 * birth_coach_show_once in the SDL layer.
 */
void display_character_tutorial(void)
{
    birth_coach_show(BIRTH_COACH_SHEET);
}

/*
 * Resolve a short gamepad/key label for a binding, falling back to a literal
 * when it is unbound or ambiguous.  Used by the name-entry prompt below.
 */
static void tutorial_prompt_label(int binding, const char* fallback, char* out,
    size_t out_size)
{
    if (!out || !out_size)
        return;

    sdl_gamepad_action_binding_short_label(binding, out, out_size);
    if (streq(out, "(unbound)") || streq(out, "Multiple"))
        SDL_strlcpy(out, fallback, out_size);
}

/*
 * Special display, part 2a
 */
static void display_player_misc_info(void)
{
    /* Name */
    char name[40];
    int wid = 80;
    int hgt = 24;
    int col = 20;
    
    if (story_character_enabled()) {
        sdl_story_font_enable();
    }
    
    if (p_ptr->oaths_broken) {
        /* Show "the Oathbreaker" in red if any oath is broken */
        strnfmt(name, sizeof(name), "%s the Oathbreaker", op_ptr->full_name);
    } else {
        /* Normal display with character title */
        strnfmt(name, sizeof(name), "%s%s", op_ptr->full_name, c_name + current_character_profile->alt_name);
    }

    Term_get_size(&wid, &hgt);
    if (wid > 0)
    {
        int name_len = (int)strlen(name);
        if (name_len < wid)
            col = (wid - name_len) / 2;
        if (col < 0)
            col = 0;
    }

    if (p_ptr->oaths_broken)
        c_put_str(TERM_RED, name, 0, col);
    else
        c_put_str(TERM_L_BLUE, name, 0, col);
    
    if (story_character_enabled()) {
        sdl_story_font_disable();
    }

}

static int display_player_compact_summary_block(int row_start)
{
    int wid = 80;
    int hgt = 24;
    bool tight_spacing = display_player_compact_tight_spacing();
    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    int row_l = row_start;
    int row_r = row_start;
    int col_l = 1;
    int col_r = 1;
    char cur[32], rhs[32], val[64], buf[160];

    const bool two_col = (wid >= 50);

        if (row_l < 0) row_l = 0;
        if (row_r < 0) row_r = 0;

        /* Single-column fallback for very narrow widths */
        if (!two_col)
        {
        int row = row_start;
        const int col = 1;

        /* Health */
        {
            int chp = p_ptr->chp; if (chp > 999) chp = 999;
            int mhp = p_ptr->mhp; if (mhp > 999) mhp = 999;
            strnfmt(cur, sizeof(cur), "%d", chp);
            strnfmt(rhs, sizeof(rhs), "%d", mhp);
            put_pair20_right(col, row++,
                     "Health",
                     cur, 3, TERM_L_BLUE,
                     '/', rhs, 3, TERM_L_BLUE);
        }

        /* Voice */
        {
            int csp = p_ptr->csp; if (csp > 999) csp = 999;
            int msp = p_ptr->msp; if (msp > 999) msp = 999;
            strnfmt(cur, sizeof(cur), "%d", csp);
            strnfmt(rhs, sizeof(rhs), "%d", msp);
            put_pair20_right(col, row++,
                     "Voice",
                     cur, 3, TERM_L_BLUE,
                     '/', rhs, 3, TERM_L_BLUE);
        }

        /* Melee */
        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
        put_single20_right(col, row++,
                   "Melee", val, 12, TERM_L_BLUE);

        /* Offhand if present */
        if (p_ptr->mds2 > 0)
        {
            strnfmt(val, sizeof(val), "(%+d,%dd%d)",
                p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
                p_ptr->mdd2, p_ptr->mds2);
            put_single20_right(col, row++,
                       "Offhand", val, 12, TERM_L_BLUE);
        }

        /* Bows */
        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
        put_single20_right(col, row++,
                   "Bows", val, 12, TERM_L_BLUE);

        /* Armor */
        strnfmt(val, sizeof(val), "[%+d,%d-%d]",
            p_ptr->skill_use[S_EVN], p_min(GF_HURT, true), p_max(GF_HURT, true));
        put_single20_right(col, row++,
                   "Armor", val, 12, TERM_L_BLUE);

        /* Exp */
        strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);
        strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);
        put_pair20_right(col, row++,
                 "Exp",
                 cur, 5, TERM_L_GREEN,
                 '/', rhs, 6, TERM_L_GREEN);

        /* Burden */
        {
            long cur_b = (long)p_ptr->total_weight;
            long max_b = (long)weight_limit();
            format_tenths(cur, sizeof(cur), cur_b);
            format_tenths(rhs, sizeof(rhs), max_b);
            put_pair20_right(col, row++,
                     "Burden",
                     cur, 6, burden_attr(),
                     '/', rhs, 6, TERM_L_GREEN);
        }

        /* Depth c/m */
        if (turn > 0)
        {
            long cur_d = (long)(p_ptr->depth * 50);
            long min_d = (long)(min_depth() * 50);

            if (cur_d > 1000) cur_d = 1000;
            if (min_d > 1000) min_d = 1000;

            strnfmt(cur, sizeof(cur), "%ld", cur_d);
            strnfmt(rhs, sizeof(rhs), "%ld", min_d);
            put_pair20_right(col, row++,
                     "Depth c/m",
                     cur, 4, (cur_d >= min_d) ? TERM_L_GREEN : TERM_YELLOW,
                     '/', rhs, 4, TERM_L_GREEN);

            if (!tight_spacing
                && display_player_min_depth_progress_bar_line(col, row,
                MAX(1, wid - COMPACT_RIGHT_PAD - col)))
            {
                row++;
            }
        }

        display_player_deep_call_line(col, row++,
            MAX(1, wid - COMPACT_RIGHT_PAD - col));

        /* Turn */
        comma_number(buf, playerturn);
        put_single20_right(col, row++,
                   "Turn", buf, 12, TERM_L_GREEN);

        /* Light */
        strnfmt(val, sizeof(val), "%d", p_ptr->cur_light);
        put_single20_right(col, row++,
                   "Light", val, 2, TERM_L_GREEN);

        /* Songs */
        if (p_ptr->song1 != SNG_NOTHING) {
            strnfmt(val, sizeof(val), "%s", display_player_song_name(p_ptr->song1));
            put_single20_right(col, row++,
                       "Song", val, 14, TERM_L_BLUE);
        }
        if (p_ptr->song2 != SNG_NOTHING) {
            strnfmt(val, sizeof(val), "%s", display_player_song_name(p_ptr->song2));
            put_single20_right(col, row++,
                       "Song", val, 14, TERM_L_BLUE);
        }

        return row + 1;
        }

        /* Two-column summary */
        col_r = compact_right_column_start(wid);

    /* Health (left) */
    {
        int chp = p_ptr->chp; if (chp > 999) chp = 999;
        int mhp = p_ptr->mhp; if (mhp > 999) mhp = 999;
        strnfmt(cur, sizeof(cur), "%d", chp);
        strnfmt(rhs, sizeof(rhs), "%d", mhp);
        put_pair20_right(col_l, row_l++,
                         "Health",
                         cur, 3, TERM_L_BLUE,
                         '/', rhs, 3, TERM_L_BLUE);
    }

    /* Voice (left) */
    {
        int csp = p_ptr->csp; if (csp > 999) csp = 999;
        int msp = p_ptr->msp; if (msp > 999) msp = 999;
        strnfmt(cur, sizeof(cur), "%d", csp);
        strnfmt(rhs, sizeof(rhs), "%d", msp);
        put_pair20_right(col_l, row_l++,
                         "Voice",
                         cur, 3, TERM_L_BLUE,
                         '/', rhs, 3, TERM_L_BLUE);
    }

    /* Melee (left) */
    strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
    put_single20_right(col_l, row_l++,
                       "Melee", val, 12, TERM_L_BLUE);

    /* Offhand if present (left) */
    if (p_ptr->mds2 > 0)
    {
        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
                p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
                p_ptr->mdd2, p_ptr->mds2);
        put_single20_right(col_l, row_l++,
                           "Offhand", val, 12, TERM_L_BLUE);
    }

    /* Bows (left) */
    strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
    put_single20_right(col_l, row_l++,
                       "Bows", val, 12, TERM_L_BLUE);

    /* Armor (left) */
    strnfmt(val, sizeof(val), "[%+d,%d-%d]",
            p_ptr->skill_use[S_EVN], p_min(GF_HURT, true), p_max(GF_HURT, true));
    put_single20_right(col_l, row_l++,
                       "Armor", val, 12, TERM_L_BLUE);

    /* Exp (right) */
    strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);
    strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);
    put_pair20_right(col_r, row_r++,
                     "Exp",
                     cur, 5, TERM_L_GREEN,
                     '/', rhs, 6, TERM_L_GREEN);

    /* Burden (right) */
    {
        long cur_b = (long)p_ptr->total_weight;
        long max_b = (long)weight_limit();
        format_tenths(cur, sizeof(cur), cur_b);
        format_tenths(rhs, sizeof(rhs), max_b);
        put_pair20_right(col_r, row_r++,
                         "Burden",
                         cur, 6, burden_attr(),
                         '/', rhs, 6, TERM_L_GREEN);
    }

    /* Depth c/m (right) */
    if (turn > 0)
    {
        long cur_d = (long)(p_ptr->depth * 50);
        long min_d = (long)(min_depth() * 50);

        if (cur_d > 1000) cur_d = 1000;
        if (min_d > 1000) min_d = 1000;

        strnfmt(cur, sizeof(cur), "%ld", cur_d);
        strnfmt(rhs, sizeof(rhs), "%ld", min_d);
        put_pair20_right(col_r, row_r++,
                         "Depth c/m",
                         cur, 4, (cur_d >= min_d) ? TERM_L_GREEN : TERM_YELLOW,
                         '/', rhs, 4, TERM_L_GREEN);

        if (!tight_spacing
            && display_player_min_depth_progress_bar_line(col_r, row_r, LINEW20))
            row_r++;
    }

    display_player_deep_call_line(col_r, row_r++, LINEW20);

    /* Turn (right) */
    comma_number(buf, playerturn);
    put_single20_right(col_r, row_r++,
                       "Turn", buf, 12, TERM_L_GREEN);

    /* Light (right) */
    strnfmt(val, sizeof(val), "%d", p_ptr->cur_light);
    put_single20_right(col_r, row_r++,
                       "Light", val, 2, TERM_L_GREEN);

    /* Songs (below whichever column is taller) */
    {
        int row_song = (row_l > row_r) ? row_l : row_r;

        if (p_ptr->song1 != SNG_NOTHING) {
            strnfmt(val, sizeof(val), "%s", display_player_song_name(p_ptr->song1));
            put_single20_right(col_l, row_song++,
                               "Song", val, 14, TERM_L_BLUE);
        }
        if (p_ptr->song2 != SNG_NOTHING) {
            strnfmt(val, sizeof(val), "%s", display_player_song_name(p_ptr->song2));
            put_single20_right(col_l, row_song++,
                               "Song", val, 14, TERM_L_BLUE);
        }

        row_l = row_song;
        row_r = row_song;
    }
    return ((row_l > row_r) ? row_l : row_r)
        + (display_player_compact_tight_spacing() ? 0 : 1);
}

typedef struct {
    const char *txt;
    byte col;
} compact_trait_line;

static int collect_compact_trait_lines(compact_trait_line* out, int out_max)
{
    int race = p_ptr->prace;
    int character = p_ptr->pcharacter;
    int total = 0;

    byte attr_affinity   = TERM_GREEN;   /* AF */
    byte attr_mastery    = TERM_L_GREEN; /* MA */
    byte attr_penalty    = TERM_RED;     /* PE */
    byte attr_gr_penalty = TERM_L_RED;   /* GP */

    compact_trait_line uniq_buf[32], ma_buf[16], af_buf[16], pen_buf[32];
    int uniq_n = 0, ma_n = 0, af_n = 0, pen_n = 0;

#define PUSH(arr, n, text, color) do { (arr)[(n)].txt = (text); (arr)[(n)++].col = (color); } while (0)

#define HANDLE_SKILL_EX(LABEL, AFF_FLAG, PEN_FLAG)                                      \
    do {                                                                                \
        int score = 0;                                                                  \
        if (p_info[race].flags      & (AFF_FLAG)) score++;                              \
        if (c_info[character].flags & (AFF_FLAG)) score++;                              \
        if ((PEN_FLAG) && (p_info[race].flags      & (PEN_FLAG))) score--;              \
        if ((PEN_FLAG) && (c_info[character].flags & (PEN_FLAG))) score--;              \
        score += curse_flag_count_rhf(AFF_FLAG);                                        \
        if ((PEN_FLAG)) score -= curse_flag_count_rhf(PEN_FLAG);                        \
        if (score >  2) score =  2;                                                     \
        if (score < -2) score = -2;                                                     \
        if (score ==  2)      PUSH(ma_buf,  ma_n,  LABEL "++", attr_mastery);          \
        else if (score == 1)  PUSH(af_buf,  af_n,  LABEL "+ ", attr_affinity);         \
        else if (score == -1) PUSH(pen_buf, pen_n, LABEL "- ", attr_penalty);          \
        else if (score == -2) PUSH(pen_buf, pen_n, LABEL "--", attr_gr_penalty);       \
    } while (0)

#define HANDLE_UNIQUE(LABEL, FLAG, COLOR)                                               \
    do {                                                                                \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG)))        \
            PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                                   \
    } while (0)

#define HANDLE_UNIQUE_U(LABEL, FLAG, COLOR)                                             \
    do {                                                                                \
        if (c_info[character].flags_u & (FLAG))                                         \
            PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                                   \
    } while (0)

#define EMIT(arr, n)                                                                    \
    do {                                                                                \
        for (int _i = 0; _i < (n); ++_i) {                                              \
            if (out && total < out_max) out[total] = (arr)[_i];                        \
            total++;                                                                    \
        }                                                                               \
    } while (0)

    HANDLE_SKILL_EX("melee",      RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    HANDLE_SKILL_EX("evasion",    RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    HANDLE_SKILL_EX("stealth",    RHF_STL_AFFINITY, RHF_STL_PENALTY);
    HANDLE_SKILL_EX("archery",    RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    HANDLE_SKILL_EX("will",       RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    HANDLE_SKILL_EX("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    HANDLE_SKILL_EX("smithing",   RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    HANDLE_SKILL_EX("song",       RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    HANDLE_SKILL_EX("bow",        RHF_BOW_PROFICIENCY, 0);
    HANDLE_SKILL_EX("axe",        RHF_AXE_PROFICIENCY, 0);

    HANDLE_UNIQUE_U("Master Artisan",     UNQ_SMT_FEANOR,   TERM_VIOLET);
    HANDLE_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Chosen of Ulmo",     UNQ_WIL_TUOR,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Indomitable Will",   UNQ_EARENDIL,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Oromë Himself",      UNQ_WIL_FIN,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Songs of Power",     UNQ_SNG_FIN,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Elven Dance",        UNQ_SNG_LUT,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Girdle of Melian",   UNQ_SNG_MEL,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR,  TERM_VIOLET);
    HANDLE_UNIQUE_U("Old Master",         UNQ_SMT_GAMIL,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Ring Master",        UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    HANDLE_UNIQUE_U("Aure entuluva",      UNQ_SNG_HURIN,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Voice of the Girdle",UNQ_SNG_THINGOL,  TERM_VIOLET);
    HANDLE_UNIQUE_U("Forgotten",          UNQ_MIM,          TERM_VIOLET);
    HANDLE_UNIQUE_U("One Handed",         UNQ_MEL_MAEDHROS, TERM_VIOLET);
    HANDLE_UNIQUE_U("Agarwaen",           UNQ_WIL_TURIN,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Shadow Walker",      UNQ_SNG_TURGON,   TERM_VIOLET);
    HANDLE_UNIQUE_U("Minstrel",           UNQ_MINSTREL,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Woven Master",       UNQ_WOVEN_MASTER, TERM_VIOLET);
    HANDLE_UNIQUE("Gift of Eru",          RHF_GIFTERU,      TERM_VIOLET);
    HANDLE_UNIQUE("Seafarer",             RHF_FREE,         TERM_VIOLET);

    HANDLE_UNIQUE("Kinslayer",            RHF_KINSLAYER,    TERM_UMBER);
    HANDLE_UNIQUE("Treacherous",          RHF_TREACHERY,    TERM_UMBER);
    HANDLE_UNIQUE("Doom of Mandos",       RHF_CURSE,        TERM_UMBER);
    HANDLE_UNIQUE("Morgoth Curse",        RHF_MOR_CURSE,    TERM_UMBER);

    EMIT(uniq_buf, uniq_n);
    EMIT(ma_buf, ma_n);
    EMIT(af_buf, af_n);
    EMIT(pen_buf, pen_n);

#undef EMIT
#undef HANDLE_UNIQUE_U
#undef HANDLE_UNIQUE
#undef HANDLE_SKILL_EX
#undef PUSH

    return total;
}

static int display_player_compact_traits_block(int row_start, int col, int row_limit,
    int skip_lines)
{
    int row = row_start;
    compact_trait_line lines[96];
    int line_count = collect_compact_trait_lines(lines, 96);

    if (skip_lines < 0)
        skip_lines = 0;

    if (story_character_enabled())
        sdl_story_font_enable();

    for (int i = skip_lines; i < line_count && row < row_limit; ++i)
        Term_putstr(col, row++, -1, lines[i].col, lines[i].txt);

    if (story_character_enabled())
        sdl_story_font_disable();

    return row;
}

static int display_player_compact_trait_max_label_chars(void)
{
    compact_trait_line lines[96];
    int line_count = collect_compact_trait_lines(lines, 96);
    int max_chars = 0;

    for (int i = 0; i < line_count; ++i)
    {
        int len = (int)strlen(lines[i].txt ? lines[i].txt : "");
        if (len > max_chars)
            max_chars = len;
    }

    return max_chars;
}

static bool display_player_compact_can_embed_traits(int row_start)
{
    int wid = 80;
    int hgt = 24;
    int skills_count = 0;
    int attr_block_h = 1 + A_MAX;
    int skill_block_h;
    int trait_lines;
    int trait_block_h;
    int available_rows;
    int col_attr = 1;
    int attr_width = LINEW20;
    int attr_right_edge = col_attr + attr_width - 1;
    int col_skill;
    int col_traits;
    int trait_width;
    int trait_max_chars;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    for (int s = 0; s < S_MAX; ++s)
        if (s != S_SPC)
            skills_count++;

    skill_block_h = 1 + skills_count;
    trait_lines = collect_compact_trait_lines(NULL, 0);
    if (trait_lines <= 0)
        return false;

    trait_block_h = 1 + trait_lines;
    available_rows = hgt - 1 - row_start;
    if (available_rows < 1)
        return false;

    col_skill = compact_right_column_start(wid);
    col_traits = attr_right_edge + 2;
    trait_width = col_skill - col_traits - 2;
    trait_max_chars = display_player_compact_trait_max_label_chars();
    if (trait_max_chars < 6)
        trait_max_chars = 6;

    if (wid < 64)
        return false;
    if (trait_width < trait_max_chars)
        return false;
    if (available_rows < attr_block_h)
        return false;
    if (available_rows < skill_block_h)
        return false;
    if (available_rows < trait_block_h)
        return false;

    return true;
}

static int display_player_compact_wrapped_line_count(const char* text, int col,
    int wrap_col)
{
    int wid = 80;
    int hgt = 24;
    int max_width;
    int line_pos = 0;
    int line_count = 0;
    const char* p = text;
    char line_buf[512];

    if (!text || !text[0])
        return 0;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    if (wrap_col <= col)
        wrap_col = wid - COMPACT_RIGHT_PAD;

    max_width = wrap_col - col;
    if (max_width < 10)
        max_width = 10;

    while (*p)
    {
        while (*p == ' ' && line_pos == 0)
            p++;

        if (*p == '\n')
        {
            line_buf[line_pos] = '\0';
            if (line_pos > 0)
                line_count++;
            line_pos = 0;
            p++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buf[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                int remaining;

                line_buf[wrap_pos] = '\0';
                line_count++;

                remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++)
                    line_buf[i] = line_buf[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_buf[line_pos] = '\0';
                line_count++;
                line_pos = 0;
            }

            continue;
        }

        if (line_pos < (int)sizeof(line_buf) - 2)
            line_buf[line_pos++] = *p;
        p++;
    }

    if (line_pos > 0)
        line_count++;

    return line_count;
}

static int display_player_compact_history_line_count(int wrap_col, int indent)
{
    /*
     * Keep the height estimate aligned with the compact renderer below.
     * If these diverge, the birth compact sheet can incorrectly keep the
     * top summary block and clip long biographies such as Glorfindel's.
     */
    return display_player_compact_wrapped_line_count(p_ptr->history, indent,
        wrap_col);
}

static int display_player_compact_wrapped_offset(const char* text, int start_row,
    int col, int wrap_col, int row_limit, int skip_lines, byte attr)
{
    int wid = 80;
    int hgt = 24;
    int row = start_row;
    int max_width;
    int line_pos = 0;
    int line_idx = 0;
    const char* p = text;
    char line_buf[512];

    if (!text || !text[0])
        return start_row;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    if (wrap_col <= col)
        wrap_col = wid - COMPACT_RIGHT_PAD;

    max_width = wrap_col - col;
    if (max_width < 10)
        max_width = 10;

    if (skip_lines < 0)
        skip_lines = 0;

    while (*p)
    {
        while (*p == ' ' && line_pos == 0)
            p++;

        if (*p == '\n')
        {
            line_buf[line_pos] = '\0';
            if (line_pos > 0)
            {
                if (line_idx >= skip_lines && row < row_limit)
                    Term_putstr(col, row++, -1, attr, line_buf);
                line_idx++;
            }
            line_pos = 0;
            p++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buf[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                line_buf[wrap_pos] = '\0';
                if (line_idx >= skip_lines && row < row_limit)
                    Term_putstr(col, row++, -1, attr, line_buf);

                int remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++)
                    line_buf[i] = line_buf[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_buf[line_pos] = '\0';
                if (line_idx >= skip_lines && row < row_limit)
                    Term_putstr(col, row++, -1, attr, line_buf);
                line_pos = 0;
            }

            line_idx++;
            continue;
        }

        if (line_pos < (int)sizeof(line_buf) - 2)
            line_buf[line_pos++] = *p;
        p++;
    }

    if (line_pos > 0)
    {
        line_buf[line_pos] = '\0';
        if (line_idx >= skip_lines && row < row_limit)
            Term_putstr(col, row++, -1, attr, line_buf);
    }

    return row;
}

static void display_player_compact_history_column(int row_start, int col, int wrap_col,
    int skip_lines, int row_limit)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    if (col < 0)
        col = 0;
    if (wrap_col <= col)
        wrap_col = wid - COMPACT_RIGHT_PAD;

    if (story_character_enabled())
        sdl_story_font_enable();

    (void)display_player_compact_wrapped_offset(p_ptr->history, row_start, col,
        wrap_col, row_limit, skip_lines, TERM_WHITE);

    if (story_character_enabled())
        sdl_story_font_disable();
}

static void display_player_compact_heading(cptr text, int row, int col);

static void display_player_compact_traits_middle_column(int row_start, int col,
    int max_cols, int row_limit)
{
    compact_trait_line lines[96];
    int line_count = collect_compact_trait_lines(lines, 96);
    int max_chars = 0;
    int draw_w;
    int start_col;
    int row = row_start;

    if (line_count <= 0 || max_cols < 6)
        return;

    for (int i = 0; i < line_count; ++i)
    {
        int len = (int)strlen(lines[i].txt ? lines[i].txt : "");
        if (len > max_chars)
            max_chars = len;
    }

    draw_w = max_chars;
    if (draw_w > max_cols)
        draw_w = max_cols;
    if (draw_w < 1)
        draw_w = 1;

    start_col = col + (max_cols - draw_w) / 2;
    display_player_compact_heading("Traits", row++, start_col);

    for (int i = 0; i < line_count && row < row_limit; ++i)
    {
        char line_buf[64];
        const char* src = lines[i].txt ? lines[i].txt : "";

        strnfmt(line_buf, sizeof(line_buf), "%.*s", draw_w, src);
        Term_erase(col, row, max_cols);
        Term_putstr(start_col, row, max_cols, lines[i].col, line_buf);
        row++;
    }
}

static void display_player_compact_heading(cptr text, int row, int col)
{
    bool use_story = story_character_enabled();

    if (use_story)
        sdl_story_font_enable();

    Term_putstr(col, row, -1, TERM_L_BLUE, text ? text : "");

    if (use_story)
        sdl_story_font_disable();
}

static int compact_stat_highlight = -1;

static void display_player_compact_description_and_flags(int row_start,
    int visible_row_start)
{
    int wid = 80;
    int hgt = 24;
    int scroll = display_player_compact_scroll;
    bool tight_spacing = display_player_compact_tight_spacing();
    bool traits_moved_to_stats_page = display_player_compact_can_embed_traits(
        visible_row_start);
    int content_row = row_start;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    int row_limit = hgt - 1;
    int available_rows = row_limit - visible_row_start;
    if (available_rows <= 0)
    {
        display_player_compact_max_scroll = 0;
        return;
    }

    if (traits_moved_to_stats_page)
    {
        int history_lines = display_player_compact_history_line_count(
            wid - COMPACT_RIGHT_PAD, 1);
        int content_height = history_lines;
        int max_scroll = history_lines - available_rows;
        if (max_scroll < 0)
            max_scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;

        if (!tight_spacing && (max_scroll == 0) && (content_height < available_rows))
            content_row += (available_rows - content_height) / 2;

        display_player_compact_max_scroll = max_scroll;
        display_player_compact_history_column(content_row, 1,
            wid - COMPACT_RIGHT_PAD, scroll, row_limit);
        return;
    }

    int trait_lines = collect_compact_trait_lines(NULL, 0);
    int history_lines_stacked = display_player_compact_history_line_count(wid - 1, 1);

    bool can_side_by_side = false;
    int trait_max_chars = display_player_compact_trait_max_label_chars();
    int side_history_col = 1 + trait_max_chars + 2; /* left col + max label + gap */
    int history_lines_side = history_lines_stacked;

    if (side_history_col < 4)
        side_history_col = 4;

    if (wid >= 46)
    {
        int side_width = wid - side_history_col - COMPACT_RIGHT_PAD;
        if (side_width >= 18)
        {
            can_side_by_side = true;
            history_lines_side = display_player_compact_history_line_count(wid - COMPACT_RIGHT_PAD, side_history_col);
        }
    }

    int side_overflow = 1000000;
    if (can_side_by_side)
    {
        int traits_over = (trait_lines > available_rows) ? (trait_lines - available_rows) : 0;
        int history_over = (history_lines_side > available_rows) ? (history_lines_side - available_rows) : 0;
        side_overflow = traits_over + history_over;
    }

    int stacked_total = trait_lines
        + ((trait_lines > 0 && history_lines_stacked > 0) ? 1 : 0)
        + history_lines_stacked;
    int stacked_overflow = (stacked_total > available_rows)
        ? (stacked_total - available_rows)
        : 0;

    bool use_side_by_side = can_side_by_side && (side_overflow <= stacked_overflow);

    log_trace("Compact description+flags fit: wid=%d rows=%d traits=%d max_trait_chars=%d history_col=%d history_stack=%d history_side=%d side_overflow=%d stacked_overflow=%d use_side=%s",
              wid, available_rows, trait_lines, trait_max_chars, side_history_col,
              history_lines_stacked, history_lines_side,
              side_overflow, stacked_overflow, use_side_by_side ? "true" : "false");

    if (use_side_by_side)
    {
        int total_height = (trait_lines > history_lines_side) ? trait_lines : history_lines_side;
        int max_scroll = total_height - available_rows;
        if (max_scroll < 0)
            max_scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;

        if (!tight_spacing && (max_scroll == 0) && (total_height < available_rows))
            content_row += (available_rows - total_height) / 2;

        display_player_compact_max_scroll = max_scroll;

        display_player_compact_traits_block(content_row, 1, row_limit, scroll);
        display_player_compact_history_column(content_row, side_history_col,
            wid - COMPACT_RIGHT_PAD, scroll, row_limit);
        return;
    }

    {
        int max_scroll = stacked_total - available_rows;
        if (max_scroll < 0)
            max_scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;

        if (!tight_spacing && (max_scroll == 0) && (stacked_total < available_rows))
            content_row += (available_rows - stacked_total) / 2;

        display_player_compact_max_scroll = max_scroll;
    }

    int trait_skip = scroll;
    if (trait_skip > trait_lines)
        trait_skip = trait_lines;

    int row_after_flags = display_player_compact_traits_block(content_row, 1, row_limit,
        trait_skip);

    int history_skip = scroll - trait_lines;
    if (trait_lines > 0 && history_lines_stacked > 0)
        history_skip--;
    if (history_skip < 0)
        history_skip = 0;

    if (trait_lines > trait_skip && history_lines_stacked > 0
        && row_after_flags < row_limit && scroll < trait_lines + 1)
        row_after_flags += (display_player_compact_tight_spacing() ? 0 : 1);

    if (history_lines_stacked > 0 && row_after_flags < row_limit)
        display_player_compact_history_column(row_after_flags, 1,
            wid - COMPACT_RIGHT_PAD, history_skip, row_limit);
}

static void display_player_compact_attribute_line(int row, int col, int max_cols, int stat)
{
    if (max_cols < 10)
        return;

    if (stat < 0 || stat >= A_MAX)
        return;

    int use = p_ptr->stat_use[stat];
    int base = p_ptr->stat_base[stat];
    int mod = use - base;

    int val_w = 10;
    if (val_w > max_cols - 4)
        val_w = max_cols - 4;
    if (val_w < 5)
        val_w = 5;

    char val_buf[16];
    if (mod != 0 && val_w >= 10)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d%+d", use, base, mod);
    else if (mod != 0 && val_w >= 8)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d", use, base);
    else if (mod != 0)
        strnfmt(val_buf, sizeof(val_buf), "%2d", use);
    else if (val_w >= 8)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d", use, base);
    else
        strnfmt(val_buf, sizeof(val_buf), "%2d", use);

    int end_col = col + max_cols - 1;
    int val_start = end_col - val_w + 1;
    int label_w = val_start - col;
    byte line_attr = (stat == compact_stat_highlight) ? TERM_L_BLUE : TERM_WHITE;
    if (label_w < 1)
        label_w = 1;

    Term_erase(col, row, label_w);
    Term_erase(val_start, row, val_w);

    const char* stat_label = (p_ptr->stat_drain[stat] < 0) ? stat_names_reduced[stat] : stat_names[stat];
    char label_buf[32];
    SDL_strlcpy(label_buf, stat_label ? stat_label : "", sizeof(label_buf));
    int len = (int)strlen(label_buf);
    while (len > 0 && label_buf[len - 1] == ' ')
        label_buf[--len] = '\0';
    if (len > label_w)
        label_buf[label_w] = '\0';

    if (story_character_enabled() && label_w > 0)
    {
        int cell_w = sdl_get_cell_width();
        int max_pixels = label_w * cell_w;
        size_t label_len = strlen(label_buf);

        while (label_len > 0 && sdl_story_font_text_width(label_buf, (int)label_len) > max_pixels)
        {
            label_buf[--label_len] = '\0';
            while (label_len > 0 && isspace((unsigned char)label_buf[label_len - 1]))
                label_buf[--label_len] = '\0';
        }
    }

    if (story_character_enabled())
        sdl_story_font_enable();

    Term_putstr(col, row, -1, line_attr, label_buf);

    if (story_character_enabled())
        sdl_story_font_disable();

    const char* val_text = val_buf;
    int val_len = (int)strlen(val_text);
    if (val_len > val_w)
    {
        val_buf[val_w] = '\0';
        val_text = val_buf;
        val_len = val_w;
    }

    int out_col = end_col - val_len + 1;
    if (out_col < val_start)
        out_col = val_start;

    byte stat_color = (p_ptr->stat_drain[stat] < 0) ? TERM_YELLOW : TERM_L_GREEN;
    byte value_attr = (stat == compact_stat_highlight) ? TERM_L_BLUE : stat_color;
    Term_putstr(out_col, row, val_len, value_attr, val_text);
}

static void display_player_compact_attributes(int row_start, int max_cols)
{
    int wid = 80;
    int hgt = 24;
    int row = row_start;
    int col = 1;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    display_player_compact_heading("Attributes", row++, col);

    if (max_cols <= 0)
        max_cols = wid - col - COMPACT_RIGHT_PAD;
    if (max_cols < 10)
        max_cols = 10;

    for (int stat = 0; stat < A_MAX && row < hgt - 1; ++stat)
    {
        display_player_compact_attribute_line(row++, col, max_cols, stat);
    }
}

/* Forward declaration: used by combined compact pages. */
static void display_player_compact_skills_list(int row_start);
static int compact_skill_highlight = -1;

static void display_player_compact_skill_line(int row, int col, int max_cols, int skill)
{
    if (max_cols < 10)
        return;

    if (skill < 0 || skill >= S_MAX || skill == S_SPC)
        return;

    int use = p_ptr->skill_use[skill];
    int base = p_ptr->skill_base[skill];
    int mod = use - base;

    /* Reserve a right-aligned numeric block in monospace. */
    int val_w = 10;
    if (val_w > max_cols - 4)
        val_w = max_cols - 4;
    if (val_w < 5)
        val_w = 5;

    char val_buf[16];
    if (mod != 0 && val_w >= 10)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d%+d", use, base, mod);
    else if (mod != 0 && val_w >= 8)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d", use, base);
    else if (mod != 0)
        strnfmt(val_buf, sizeof(val_buf), "%2d", use);
    else if (val_w >= 8)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d", use, base);
    else
        strnfmt(val_buf, sizeof(val_buf), "%2d", use);

    int end_col = col + max_cols - 1;
    int val_start = end_col - val_w + 1;
    int label_w = val_start - col;
    byte line_attr = (skill == compact_skill_highlight) ? TERM_L_BLUE : TERM_WHITE;
    byte value_attr = (skill == compact_skill_highlight) ? TERM_L_BLUE : TERM_L_GREEN;
    if (label_w < 1)
        label_w = 1;

    Term_erase(col, row, label_w);
    Term_erase(val_start, row, val_w);

    const char* name = skill_names_full[skill];
    if (!name)
        name = "";

    char label_buf[64];
    strnfmt(label_buf, sizeof(label_buf), "%.*s", label_w, name);

    if (story_character_enabled() && label_w > 0)
    {
        int cell_w = sdl_get_cell_width();
        int max_pixels = label_w * cell_w;
        size_t len = strlen(label_buf);

        while (len > 0 && sdl_story_font_text_width(label_buf, (int)len) > max_pixels)
        {
            label_buf[--len] = '\0';
            while (len > 0 && isspace((unsigned char)label_buf[len - 1]))
                label_buf[--len] = '\0';
        }
    }

    if (story_character_enabled())
        sdl_story_font_enable();

    Term_putstr(col, row, -1, line_attr, label_buf);

    if (story_character_enabled())
        sdl_story_font_disable();

    const char* val_text = val_buf;
    int val_len = (int)strlen(val_text);
    if (val_len > val_w)
    {
        val_buf[val_w] = '\0';
        val_text = val_buf;
        val_len = val_w;
    }

    int out_col = end_col - val_len + 1;
    if (out_col < val_start)
        out_col = val_start;

    Term_putstr(out_col, row, val_len, value_attr, val_text);
}

static void display_player_compact_attributes_and_skills(int row_start)
{
    int wid = 80;
    int hgt = 24;
    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    int skills_count = 0;
    for (int s = 0; s < S_MAX; ++s)
        if (s != S_SPC)
            skills_count++;

    int attr_block_h = 1 + A_MAX;
    int skill_block_h = 1 + skills_count;
    int block_h = (attr_block_h > skill_block_h) ? attr_block_h : skill_block_h;

    int col_attr = 1;
    int attr_width = LINEW20;
    int attr_right_edge = col_attr + attr_width - 1;
    int col_skill = compact_right_column_start(wid);
    bool embed_traits = display_player_compact_can_embed_traits(row_start);

    bool side_by_side = (wid >= 50)
        && (col_skill >= attr_right_edge + 2)
        && (row_start + block_h <= hgt - 1);

    if (embed_traits)
    {
        int col_traits = attr_right_edge + 2;
        int traits_width = col_skill - col_traits - 2;
        int row = row_start;

        display_player_compact_attributes(row_start, attr_width);
        display_player_compact_heading("Skills", row++, col_skill);

        for (int skill = 0; skill < S_MAX && row < hgt - 1; ++skill)
        {
            if (skill == S_SPC)
                continue;
            display_player_compact_skill_line(row++, col_skill, LINEW20, skill);
        }

        display_player_compact_traits_middle_column(row_start, col_traits,
            traits_width, hgt - 1);
        return;
    }

    /* Render attributes first using width appropriate to the chosen layout. */
    display_player_compact_attributes(row_start, side_by_side ? attr_width : 0);

    if (side_by_side)
    {
        int row = row_start;
        display_player_compact_heading("Skills", row++, col_skill);

        for (int skill = 0; skill < S_MAX && row < hgt - 1; ++skill)
        {
            if (skill == S_SPC)
                continue;
            display_player_compact_skill_line(row++, col_skill, LINEW20, skill);
        }

        return;
    }

    /* Stacked fallback: Skills below Attributes (may truncate if short). */
    int row_skills = row_start + 1 + A_MAX
        + (display_player_compact_tight_spacing() ? 0 : 1);
    if (row_skills < hgt - 1)
        display_player_compact_skills_list(row_skills);
}

static void display_player_compact_skills_list(int row_start)
{
    int wid = 80;
    int hgt = 24;
    int row = row_start;
    int col = 1;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    display_player_compact_heading("Skills", row++, col);

    int max_cols = wid - col - COMPACT_RIGHT_PAD;
    if (max_cols < 10)
        max_cols = 10;

    for (int skill = 0; skill < S_MAX && row < hgt - 1; skill++)
    {
        if (skill == S_SPC)
            continue;

        display_player_compact_skill_line(row++, col, max_cols, skill);
    }
}

static void display_player_compact_history(int row_start)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    display_player_compact_history_column(row_start, 1, wid - COMPACT_RIGHT_PAD,
        0, hgt - 1);
}

static void display_player_compact_desc_flags_page(bool show_misc_info,
    bool show_summary)
{
    int summary_row = show_misc_info ? display_player_compact_start_row() : 0;
    int body_row = summary_row;

    if (show_misc_info)
        display_player_misc_info();

    if (show_summary)
        body_row = display_player_compact_summary_block(summary_row);

    display_player_compact_description_and_flags(body_row, body_row);
}

/*
 * Special display, part 2b
 */
void display_player_stat_info(int row, int col)
{
    int i;

    char buf[80];

    /* First: Display all stat names with story font (if enabled) */
    for (i = 0; i < A_MAX; i++)
    {
        const char* stat_label;
        char trimmed_label[32];
        
        /* Get the stat name */
        if (p_ptr->stat_drain[i] < 0)
        {
            stat_label = stat_names_reduced[i];
        }
        else
        {
            stat_label = stat_names[i];
        }
        
        /* Trim trailing spaces for story font rendering */
        SDL_strlcpy(trimmed_label, stat_label, sizeof(trimmed_label));
        int len = strlen(trimmed_label);
        while (len > 0 && trimmed_label[len-1] == ' ') {
            trimmed_label[--len] = '\0';
        }
        
        if (story_character_enabled()) {
            sdl_story_font_enable();
        }
        
        /* Display trimmed stat name with story font (if enabled) */
        put_str(trimmed_label, row + i, col);
        
        if (story_character_enabled()) {
            sdl_story_font_disable();
        }
    }
    
    /* Second: Display all numbers with monospace font (always) */
    for (i = 0; i < A_MAX; i++)
    {
        /* Resulting "modified" maximum value */
        cnv_stat(p_ptr->stat_use[i], buf);

        if (p_ptr->stat_drain[i] < 0)
            c_put_str(TERM_YELLOW, buf, row + i, col + 5);
        else
            c_put_str(TERM_L_GREEN, buf, row + i, col + 5);

        /* Only display stat_equip_mod if not zero */
        if (p_ptr->stat_equip_mod[i] != 0)
        {
            c_put_str(TERM_SLATE, "=", row + i, col + 8);

            /* Internal "natural" maximum value */
            cnv_stat(p_ptr->stat_base[i], buf);
            c_put_str(TERM_GREEN, buf, row + i, col + 10);

            /* Equipment Bonus */
            strnfmt(buf, sizeof(buf), "%+3d", p_ptr->stat_equip_mod[i]);
            c_put_str(TERM_SLATE, buf, row + i, col + 13);
        }

        /* Only display stat_drain if not zero */
        if (p_ptr->stat_drain[i] != 0)
        {
            c_put_str(TERM_SLATE, "=", row + i, col + 8);

            /* Internal "natural" maximum value */
            cnv_stat(p_ptr->stat_base[i], buf);
            c_put_str(TERM_GREEN, buf, row + i, col + 10);

            /* Reduction */
            strnfmt(buf, sizeof(buf), "%+3d", p_ptr->stat_drain[i]);
            c_put_str(TERM_SLATE, buf, row + i, col + 17);
        }

        /* Only display stat_misc_mod if not zero */
        if (p_ptr->stat_misc_mod[i] != 0)
        {
            c_put_str(TERM_SLATE, "=", row + i, col + 8);

            /* Internal "natural" maximum value */
            cnv_stat(p_ptr->stat_base[i], buf);
            c_put_str(TERM_GREEN, buf, row + i, col + 10);

            /* Modifier */
            strnfmt(buf, sizeof(buf), "%+3d", p_ptr->stat_misc_mod[i]);
            c_put_str(TERM_SLATE, buf, row + i, col + 21);
        }
    }

    /* Leave with story font disabled */
    sdl_story_font_disable();
}

/*
 * Special display, part 2c
 *
 * How to print out the modifications and sustains.
 * Positive mods with no sustain will be light green.
 * Positive mods with a sustain will be dark green.
 * Sustains (with no modification) will be a dark green 's'.
 * Negative mods (from a curse) will be red.
 * Huge mods (>9), like from MICoMorgoth, will be a '*'
 * No mod, no sustain, will be a slate '.'
 */
static void display_player_sust_info(void)
{
    int i, row, col, stats;

    object_type* o_ptr;
    u32b f1, f2, f3;
    u32b ignore_f2, ignore_f3;

    byte a;
    char c;

    sdl_story_font_enable();

    /* Row */
    row = 2;

    /* Column */
    col = 23;

    /* Header */
    c_put_str(TERM_WHITE, "abcdefghijkl@", row - 1, col);

    /* Process equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; ++i)
    {
        /* Get the object */
        o_ptr = &inventory[i];

        /* Get the "known" flags */
        object_flags_known(o_ptr, &f1, &f2, &f3);

        /* Hack -- assume stat modifiers are known */
        object_flags(o_ptr, &f1, &ignore_f2, &ignore_f3);

        /* Initialize color based of sign of pval. */
        for (stats = 0; stats < A_MAX; stats++)
        {
            /* Default */
            a = TERM_SLATE;
            c = '.';

            /* Boost */
            if (f1 & (1 << stats))
            {
                /* Default */
                c = '*';

                /* Neutral */
                if (o_ptr->pval == 0)
                {
                    /* Neutral */
                    c = '.';
                }

                /* Good */
                if (o_ptr->pval > 0)
                {
                    /* Good */
                    a = TERM_L_GREEN;

                    /* Label boost */
                    if (o_ptr->pval < 10)
                        c = I2D(o_ptr->pval);
                }

                /* Bad */
                if (o_ptr->pval < 0)
                {
                    /* Bad */
                    a = TERM_RED;

                    /* Label boost */
                    if (o_ptr->pval > -10)
                        c = I2D(-(o_ptr->pval));
                }
            }

            /* Reverse Boost */
            if (f1 & (1 << (stats + A_MAX)))
            {
                /* Default */
                c = '*';

                /* Neutral */
                if (o_ptr->pval == 0)
                {
                    /* Neutral */
                    c = '.';
                }

                /* Good */
                if (o_ptr->pval < 0)
                {
                    /* Good */
                    a = TERM_L_GREEN;

                    /* Label boost */
                    if (o_ptr->pval > -10)
                        c = I2D(-(o_ptr->pval));
                }

                /* Bad */
                if (o_ptr->pval > 0)
                {
                    /* Bad */
                    a = TERM_RED;

                    /* Label boost */
                    if (o_ptr->pval < 10)
                        c = I2D(o_ptr->pval);
                }
            }

            /* Sustain */
            if (f2 & (1 << stats))
            {
                /* Dark green */
                if (a == TERM_RED)
                    a = TERM_ORANGE;
                else
                    a = TERM_GREEN;

                /* Convert '.' to 's' */
                if (c == '.')
                    c = 's';
            }

            /* Dump proper character */
            Term_putch(col, row + stats, a, c);
        }

        /* Advance */
        col++;
    }

    /* Check stats */
    for (stats = 0; stats < A_MAX; ++stats)
    {
        /* Default */
        a = TERM_SLATE;
        c = '.';

        /* Sustain */
        if (f2 & (1 << stats))
        {
            /* Dark green "s" */
            a = TERM_GREEN;
            c = 's';
        }

        /* Dump */
        Term_putch(col, row + stats, a, c);
    }

    /* Column */
    col = 23;

    /* Footer */
    c_put_str(TERM_WHITE, "abcdefghijkl@", row + 4, col);

    /* Equippy */
    display_player_equippy(row + 5, col);

    sdl_story_font_disable();
}

/*
 * Display the character on the screen (four different modes)
 *
 * The top two lines, and the bottom line (or two) are left blank
 * in the first two modes.
 *
 * Mode 0 = standard display with skills/history
 * Mode 1 = special display with equipment flags
 */
void display_player(int mode)
{
    int wid = 80;
    int hgt = 24;
    int wide_offset = 0;
    bool narrow = false;

    Term_get_size(&wid, &hgt);
    (void)hgt;
    narrow = (wid > 0 && wid < 80);
    if (wid > 80)
        wide_offset = (wid - 80) / 2;

    /* Erase screen */
    clear_from(0);
    display_player_compact_max_scroll = 0;

    if (narrow && (mode == 0))
        mode = DISPLAY_PLAYER_MODE_COMPACT_STATS_SKILLS;

    if (mode == DISPLAY_PLAYER_MODE_COMPACT_DESC_FLAGS)
    {
        display_player_compact_desc_flags_page(true, true);
        if (display_player_compact_max_scroll > 0)
        {
            clear_from(0);
            display_player_compact_desc_flags_page(true, false);
        }
        if (display_player_compact_max_scroll > 0)
        {
            clear_from(0);
            display_player_compact_desc_flags_page(false, false);
        }
        sdl_story_font_reset();
        return;
    }

    if (mode == DISPLAY_PLAYER_MODE_COMPACT_STATS_SKILLS)
    {
        display_player_misc_info();
        int body_row = display_player_compact_summary_block(
            display_player_compact_start_row());
        display_player_compact_attributes_and_skills(body_row);
        sdl_story_font_reset();
        return;
    }

    if (mode == DISPLAY_PLAYER_MODE_COMPACT_SKILLS)
    {
        display_player_misc_info();
        int body_row = display_player_compact_summary_block(
            display_player_compact_start_row());
        display_player_compact_skills_list(body_row);
        sdl_story_font_reset();
        return;
    }

    if (mode == DISPLAY_PLAYER_MODE_COMPACT_HISTORY)
    {
        display_player_misc_info();
        int body_row = display_player_compact_summary_block(
            display_player_compact_start_row());
        display_player_compact_history(body_row);
        sdl_story_font_reset();
        return;
    }

    /* All Modes Use Stat info */
    display_player_stat_info(1, 42 + wide_offset);

    if ((mode) < 2)
    {
        /* Misc info */
        display_player_misc_info();

        /* Special */
        if (mode)
        {
            /* Stat/Sustain flags */
            display_player_sust_info();

            /* Other flags */
            display_player_flag_info();
        }

        /* Standard */
        else
        {
            /* Extra info */
            display_player_xtra_info(0);
        }
    }

    sdl_story_font_reset();
}

void display_player_compact_stats_skills_highlighted(int selected_skill)
{
    compact_stat_highlight = -1;
    compact_skill_highlight = selected_skill;
    display_player(DISPLAY_PLAYER_MODE_COMPACT_STATS_SKILLS);
    compact_skill_highlight = -1;
}

void display_player_compact_stats_skills_highlighted_stat(int selected_stat)
{
    compact_skill_highlight = -1;
    compact_stat_highlight = selected_stat;
    display_player(DISPLAY_PLAYER_MODE_COMPACT_STATS_SKILLS);
    compact_stat_highlight = -1;
}

/*
 * Make a string lower case.
 */

bool get_name(void)
{
    char tmp[14];
    char old_name[14];
    // bool name_selected = false;

    log_info("Starting character name selection process");

    // Clear the names
    tmp[0] = '\0';
    old_name[0] = '\0';

    /* Display the player */
    display_player(0);

    /* Prompt */
    if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char random_label[16];
        char prompt_buf[80];

        tutorial_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        tutorial_prompt_label('\t', "L5", random_label, sizeof(random_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "%s accept name",
            confirm_label);
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
            prompt_buf);
        strnfmt(prompt_buf, sizeof(prompt_buf), "  %s random name",
            random_label);
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
            prompt_buf);
    }
    else
    {
        Term_putstr(
            QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE, "Enter accept name");
        Term_putstr(
            QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE, "  Tab random name");

        /* Hack - highlight the key names */
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "Enter");
        Term_putstr(QUESTION_COL + 2, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "Tab");
    }

    /* Special Prompt? */
    if (character_dungeon)
    {
        if (steamdeck_controls_active())
        {
            char back_label[16];
            char prompt_buf[80];

            tutorial_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf), "%s abort name change",
                back_label);
            Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1,
                TERM_SLATE, prompt_buf);
        }
        else
        {
            Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_SLATE,
                "ESC abort name change                  ");

            /* Hack - highlight the key names */
            Term_putstr(
                QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "ESC");
        }
    }

    // use old name as a default
   // SDL_strlcpy(tmp, op_ptr->full_name, sizeof(tmp));
    SDL_strlcpy(tmp, c_name + c_info[p_ptr->pcharacter].name, sizeof(tmp));

    // save a copy too
    SDL_strlcpy(old_name, c_name + c_info[p_ptr->pcharacter].name, sizeof(old_name));

    /* Prompt for a new name */
    Term_gotoxy(8, 2);

   /* while (!name_selected)
    {
        if (askfor_name(tmp, sizeof(tmp)))
        {
            SDL_strlcpy(op_ptr->full_name, tmp, sizeof(op_ptr->full_name));
            p_ptr->redraw |= (PR_MISC);
        }
        else
        {
            SDL_strlcpy(op_ptr->full_name, old_name, sizeof(op_ptr->full_name));
            return (false);
        }

        if (tmp[0] != '\0')
            name_selected = true;
        else
            bell("You must choose a name.");
    }*/

    /* Process the player name */
    SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name, sizeof(op_ptr->full_name));
    process_player_name(true);
    
    log_info("Character name confirmed: '%s'", op_ptr->full_name);
 
    return (true);
}

/*
 * Hack -- escape from Angband
 */
