/* File: spells2.c */

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
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"

static void spells2_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (!buf[0] || streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback ? fallback : "", buflen);
}

// Function declarations
void analyze_weapon_properties(int* count, char s[][200], char t[][200], bool good[], 
                              bool identify[], int slot, const char* weapon_name);
void display_attributes(char s[][200], char t[][200], bool good[], int count);
void identify_revealed_items(bool identify[]);

typedef struct self_knowledge_capture
{
    int width;
    int height;
    byte* attrs;
    char* chars;
    byte* story;
} self_knowledge_capture;

static void self_knowledge_capture_free(self_knowledge_capture* capture);
static bool self_knowledge_capture_build(char s[][200], char t[][200],
    bool good[], int count, self_knowledge_capture* capture);
static void self_knowledge_capture_view(const self_knowledge_capture* capture);
static bool render_resistance_summary(const char* text);

#define TR1 0
#define TR2 1
#define TR3 2
#define RF1 3
#define RF2 4
#define RF3 5
#define RF4 6
#define RHF 7
#define VLT 8
#define CUR 9
#define UNQ 10
#define MAX_FLAG_SETS 11

static int self_knowledge_capture_used_rows(term* t)
{
    if (!t || !t->scr)
        return 0;

    for (int y = t->hgt - 1; y >= 0; y--)
    {
        for (int x = 0; x < t->wid; x++)
        {
            if ((t->scr->c[y][x] != ' ')
                || (t->scr->a[y][x] != t->attr_blank)
                || (t->scr->story[y][x] != 0))
            {
                return y + 1;
            }
        }
    }

    return 0;
}

static void self_knowledge_capture_free(self_knowledge_capture* capture)
{
    if (!capture)
        return;

    mem_free_null(capture->attrs);
    mem_free_null(capture->chars);
    mem_free_null(capture->story);

    capture->width = 0;
    capture->height = 0;
}

static void self_knowledge_capture_render_body(char s[][200], char t[][200],
    bool good[], int count)
{
    int term_wid = 80;
    int term_hgt = 24;
    int line = 0;

    Term_get_size(&term_wid, &term_hgt);
    (void)term_hgt;
    if (term_wid < 20)
        term_wid = 20;

    text_out_hook = text_out_to_screen;
    text_out_indent = 1;
    text_out_wrap = term_wid - 4;
    if (text_out_wrap < 10)
        text_out_wrap = 10;

    for (int j = 0; j < count; j++)
    {
        int cx;
        int cy;

        if (!Term || line >= Term->hgt - 1)
            break;

        Term_gotoxy(1, line);

        if (t[j][0] == '\0' && render_resistance_summary(s[j]))
        {
            /* handled by helper */
        }
        else
        {
            text_out_c(TERM_WHITE, s[j]);
            if (t[j][0] != '\0')
            {
                text_out(" ");
                text_out_c(good[j] ? TERM_GREEN : TERM_L_RED, t[j]);
            }
        }

        Term_locate(&cx, &cy);
        line = cy + 1;
    }
}

static bool self_knowledge_capture_build(char s[][200], char t[][200],
    bool good[], int count, self_knowledge_capture* capture)
{
    enum { SELF_KNOWLEDGE_CAPTURE_ROWS = 2048 };
    term scratch;
    term* saved_term = Term;
    void (*old_hook)(byte, cptr) = text_out_hook;
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;
    bool scratch_ready = false;
    bool success = false;
    int term_wid = 80;
    int term_hgt = 24;
    int used_rows;

    if (!capture || !saved_term)
        return false;

    SDL_memset(capture, 0, sizeof(*capture));
    SDL_memset(&scratch, 0, sizeof(scratch));

    Term_get_size(&term_wid, &term_hgt);
    (void)term_hgt;
    if (term_wid < 20)
        term_wid = 20;

    if (term_init(&scratch, term_wid, SELF_KNOWLEDGE_CAPTURE_ROWS, 16) != 0)
        goto cleanup;
    scratch_ready = true;

    Term_activate(&scratch);
    Term_clear();
    self_knowledge_capture_render_body(s, t, good, count);

    used_rows = self_knowledge_capture_used_rows(Term);
    if (used_rows < 1)
        used_rows = 1;

    capture->width = term_wid;
    capture->height = used_rows;
    capture->attrs = mem_alloc_array(capture->width * capture->height, byte);
    capture->chars = mem_alloc_array(capture->width * capture->height, char);
    capture->story = mem_alloc_array(capture->width * capture->height, byte);

    for (int y = 0; y < capture->height; y++)
    {
        for (int x = 0; x < capture->width; x++)
        {
            int idx = y * capture->width + x;
            capture->attrs[idx] = scratch.scr->a[y][x];
            capture->chars[idx] = scratch.scr->c[y][x];
            capture->story[idx] = scratch.scr->story[y][x];
        }
    }

    success = true;

cleanup:
    text_out_hook = old_hook;
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;

    if (saved_term && Term != saved_term)
        Term_activate(saved_term);

    if (scratch_ready)
        term_nuke(&scratch);

    if (!success)
        self_knowledge_capture_free(capture);

    return success;
}

static void self_knowledge_capture_draw(
    const self_knowledge_capture* capture, int scroll)
{
    int term_wid = 80;
    int term_hgt = 24;
    int body_top = 1;
    int prompt_row;
    int visible_rows;
    int max_scroll;
    char prompt[96];
    char scroll_buf[32];
    bool old_story_active = false;
    bool old_story_grid = false;

    if (!capture || !capture->attrs || !capture->chars || !capture->story)
        return;

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 3)
        term_hgt = 3;

    prompt_row = term_hgt - 1;
    visible_rows = MAX(1, prompt_row - body_top);
    max_scroll = MAX(0, capture->height - visible_rows);

    if (scroll < 0)
        scroll = 0;
    if (scroll > max_scroll)
        scroll = max_scroll;

    old_story_active = Term->story_font_active;
    old_story_grid = Term->story_font_grid;

    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Your Attributes:");

    for (int row = 0; row < visible_rows; row++)
    {
        int src_row = row + scroll;
        int dst_row = body_top + row;

        if (src_row >= capture->height || dst_row >= prompt_row)
            break;

        for (int col = 0; col < capture->width && col < term_wid; col++)
        {
            int idx = src_row * capture->width + col;

            Term->story_font_active =
                ((capture->story[idx] & STORY_FLAG_USE) != 0);
            Term->story_font_grid =
                ((capture->story[idx] & STORY_FLAG_CELL_ALIGN) != 0);

            Term_queue_char(col, dst_row, capture->attrs[idx],
                capture->chars[idx], 0, 0);
        }
    }

    Term->story_font_active = old_story_active;
    Term->story_font_grid = old_story_grid;

    if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char back_label[16];

        spells2_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        spells2_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(prompt, sizeof(prompt), "D-pad scroll  %s/%s close",
            confirm_label, back_label);
    }
    else if (term_wid >= 70)
    {
        SDL_strlcpy(prompt,
            "Esc closes  Up/Down, wheel, or touch-drag scroll  Space/PgDn page",
            sizeof(prompt));
    }
    else if (term_wid >= 42)
    {
        SDL_strlcpy(prompt, "Esc closes  Up/Down wheel drag scroll",
            sizeof(prompt));
    }
    else
    {
        SDL_strlcpy(prompt, "Esc close  Up/Down scroll", sizeof(prompt));
    }

    Term_putstr(0, prompt_row, -1, TERM_SLATE, prompt);

    if (max_scroll > 0)
    {
        strnfmt(scroll_buf, sizeof(scroll_buf), "[%d/%d]", scroll + 1,
            max_scroll + 1);
        Term_putstr(MAX(0, term_wid - (int)strlen(scroll_buf)), 0, -1,
            TERM_SLATE, scroll_buf);
    }

    Term_fresh();
}

static void self_knowledge_capture_view(const self_knowledge_capture* capture)
{
    int scroll = 0;
    bool done = false;
    bool saved_hide_cursor = hide_cursor;

    if (!capture)
        return;

    hide_cursor = true;

    while (!done)
    {
        int term_wid = 80;
        int term_hgt = 24;
        int prompt_row;
        int visible_rows;
        int max_scroll;
        int page_rows;
        int dir;
        char ch;

        Term_get_size(&term_wid, &term_hgt);
        (void)term_wid;
        if (term_hgt < 3)
            term_hgt = 3;

        prompt_row = term_hgt - 1;
        visible_rows = MAX(1, prompt_row - 1);
        max_scroll = MAX(0, capture->height - visible_rows);
        page_rows = (visible_rows > 1) ? visible_rows - 1 : 1;

        if (scroll < 0)
            scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;

        self_knowledge_capture_draw(capture, scroll);

        ui_menu_click_begin();
        ui_menu_click_add_full_row(ESCAPE, prompt_row);
        ui_scroll_area_begin(1, prompt_row - 1, SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', '6', '4');
        ui_scroll_area_set_tap_key(UI_MENU_CLICK_WAKE_KEY);

        ch = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (click_action != UI_MENU_CLICK_HOVER)
                    ch = (char)clicked_choice;
            }
        }

        ui_menu_click_clear();
        ui_scroll_area_clear();

        if (steamdeck_controls_active())
        {
            if (ch == steamdeck_back_key() || ch == steamdeck_confirm_key())
                ch = ESCAPE;
        }

        dir = target_dir(ch);
        if (dir >= 1 && dir <= 9)
            ch = I2D(dir);

        switch (ch)
        {
        case UI_MENU_CLICK_WAKE_KEY:
            break;
        case ESCAPE:
        case 'q':
        case 'Q':
            done = true;
            break;
        case '8':
        case 'k':
        case 'K':
            if (scroll > 0)
                scroll--;
            break;
        case '2':
        case 'j':
        case 'J':
            if (scroll < max_scroll)
                scroll++;
            break;
        case '9':
        case '-':
        case 'p':
        case 'P':
        case '4':
            scroll -= page_rows;
            if (scroll < 0)
                scroll = 0;
            break;
        case '3':
        case ' ':
        case 'n':
        case 'N':
        case '6':
            scroll += page_rows;
            if (scroll > max_scroll)
                scroll = max_scroll;
            break;
        case '7':
            scroll = 0;
            break;
        case '1':
            scroll = max_scroll;
            break;
        default:
            break;
        }
    }

    hide_cursor = saved_hide_cursor;
    ui_menu_click_clear();
    ui_scroll_area_clear();
}

// Flags with descriptions
flag_name info_flags_desc[] = { 
{"Will Affinity is at 3, and never affected by curses", UNQ, UNQ_EARENDIL}, 
{ "Artifacts take only 1 charge of forge, easier to make fire and light items", UNQ, UNQ_SMT_FEANOR },
{ "Majesty ability is 1.5x effective", UNQ, UNQ_WIL_FIN },
{ "Song of Staying is twice effective", UNQ, UNQ_SNG_FIN },
{ "Song of Lorien is 1.5x effective", UNQ, UNQ_SNG_LUT }, 
{ "Horns are twice effective", UNQ, UNQ_WIL_TUOR },
{ "Song of Threshold and Gem of Warding are twice effective", UNQ, UNQ_SNG_MEL },
{ "Can create very sharp items, easier to create sharp and accurate items", UNQ, UNQ_SMT_TELCHAR },
{ "Using 3 forge charges can create mithril items without mithril", UNQ, UNQ_SMT_GAMIL }, 
{ "All rings cost 30% less to create and ring slots are treated as major slots", UNQ, UNQ_SMT_CELEBRIMBOR },
{ "Song of Slaying is twice effective", UNQ, UNQ_SNG_HURIN },
{ "Song of Mastery is 1.75x effective", UNQ, UNQ_SNG_THINGOL }, 
{ "Starts with all stealth skills", UNQ, UNQ_MIM },
{ "Melee abilities are twice effective, better at one-handed combat", UNQ, UNQ_MEL_MAEDHROS },
{ "Will abilities are twice effective, can break fate-cursed items", UNQ, UNQ_WIL_TURIN },
{ "Song of Disguise checks add your Perception skill", UNQ, UNQ_SNG_TURGON },
{ "Song skill is not reduced for woven minor themes", UNQ, UNQ_WOVEN_MASTER },
{ "If you die story death counter is not increased", RHF, RHF_GIFTERU }, 
{ "Deppending on the number of Silmarils retrieved there is a chance to murder your kin", RHF, RHF_KINSLAYER },
{ "You get more complex curses", RHF, RHF_CURSE }, 
{ "Can steal a Silmaril in the end", RHF, RHF_TREACHERY },
{ "Decreased ability price", RHF, RHF_FREE }, 
{ "Encounter more dangerous creatures", RHF, RHF_MOR_CURSE },
{ "Kheled-zaram gives +30 bonus to identification", RHF, RHF_KHELED_ZARAM }
};

const size_t info_flags_desc_n = sizeof(info_flags_desc) / sizeof(info_flags_desc[0]);

/*
 * Increase player's hit points by the given percentage of maximum, notice
 * effects
 */
bool hp_player(int x, bool percent, bool message)
{
    int points;

    if (percent)
        points = (p_ptr->mhp * x) / 100;
    else
        points = x;

    /* Healing needed */
    if ((p_ptr->chp < p_ptr->mhp) && (points > 0))
    {
        /* Gain hitpoints */
        p_ptr->chp += points;

        /* Enforce maximum */
        if (p_ptr->chp >= p_ptr->mhp)
        {
            p_ptr->chp = p_ptr->mhp;
            p_ptr->chp_frac = 0;
        }

        /* Redraw */
        p_ptr->redraw |= (PR_HP);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);

        if (message)
        {
            /* Heal 0-4 */
            if (points < 5)
            {
                msg_print("You feel a little better.");
            }

            /* Heal 5-10 */
            else if (points < 10)
            {
                msg_print("You feel better.");
            }

            /* Heal 10-25 */
            else if (points < 25)
            {
                msg_print("You feel much better.");
            }

            /* Heal 35+ */
            else
            {
                msg_print("You feel very good.");
            }
        }

        /* Notice */
        return (true);
    }

    /* Ignore */
    return (false);
}

/*
 * Leave a "glyph of warding" which prevents monster movement
 */
void warding_glyph(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    /* XXX XXX XXX */
    if (!cave_clean_bold(py, px))
    {
        msg_print("The object resists the spell.");
        return;
    }

    /* Create a glyph */
    cave_set_feat(py, px, FEAT_GLYPH);
}

/*
 * Array of stat "descriptions"
 */
static cptr desc_stat_pos[] = { "strong", "dextrous", "healthy", "attuned" };

/*
 * Array of stat "descriptions"
 */
static cptr desc_stat_neg[] = { "weak", "awkward", "sickly", "drained" };

/*
 * Lose a "point"
 */
bool do_dec_stat(int stat, monster_type* m_ptr)
{
    bool resistance = false; // default to soothe compiler warnings

    /* Turin character resistance check first */
    if (turin_resist_bad_effect())
        return (true);

    /* Get the "sustain" */
    switch (stat)
    {
    case A_STR:
        resistance = p_ptr->sustain_str;
        break;
    case A_DEX:
        resistance = p_ptr->sustain_dex;
        break;
    case A_CON:
        resistance = p_ptr->sustain_con;
        break;
    case A_GRA:
        resistance = p_ptr->sustain_gra;
        break;
    }

    /* Saving throw */
    if (saving_throw(m_ptr, resistance))
    {
        /* Message */
        msg_format(
            "You feel %s for a moment, but it passes.", desc_stat_neg[stat]);

        // possibly identify relevant items
        switch (stat)
        {
        case A_STR:
            ident_resist(TR2_SUST_STR);
            break;
        case A_DEX:
            ident_resist(TR2_SUST_DEX);
            break;
        case A_CON:
            ident_resist(TR2_SUST_CON);
            break;
        case A_GRA:
            ident_resist(TR2_SUST_GRA);
            break;
        }

        /* Notice effect */
        return (true);
    }

    /* Attempt to reduce the stat */
    if (dec_stat(stat, 1, false))
    {
        /* Message */
        msg_format("You feel %s.", desc_stat_neg[stat]);

        /* Notice effect */
        return (true);
    }

    /* Nothing obvious */
    return (false);
}

/*
 * Restore lost "points" in a stat
 */
bool do_res_stat(int stat, int points)
{
    /* Attempt to increase */
    if (res_stat(stat, points))
    {
        /* Message */
        msg_format("You feel less %s.", desc_stat_neg[stat]);

        /* Notice */
        return (true);
    }

    /* Nothing obvious */
    return (false);
}

/*
 * Gain a "point" in a stat
 */
bool do_inc_stat(int stat)
{
    bool res;

    /* Restore stat */
    res = res_stat(stat, 20);

    /* Attempt to increase */
    if (inc_stat(stat))
    {
        /* Message */
        msg_format("You feel %s!", desc_stat_pos[stat]);

        /* Notice */
        return (true);
    }

    /* Restoration worked */
    if (res)
    {
        /* Message */
        msg_format("You feel less %s.", desc_stat_neg[stat]);

        /* Notice */
        return (true);
    }

    /* Nothing obvious */
    return (false);
}

/*
 * Identify everything being carried.
 */
void identify_pack(void)
{
    int i;

    /* Simply identify and know every item */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Aware and Known */
        object_aware(o_ptr);
        object_known(o_ptr);
    }

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
}

/*
 * Hack -- Removes curse from an object.
 */
void uncurse_object(object_type* o_ptr)
{
    /* Uncurse it */
    o_ptr->ident &= ~(IDENT_CURSED);
    o_ptr->ident |= IDENT_UNCURSED;

    /* Remove special inscription, if any */
    if (o_ptr->discount >= INSCRIP_NULL)
        o_ptr->discount = 0;

    /* The object has been "sensed" */
    o_ptr->ident |= (IDENT_SENSE);

    /* Newly compatible stacks should collapse on the next inventory pass. */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    if ((o_ptr >= inventory) && (o_ptr < inventory + INVEN_TOTAL))
    {
        int slot = (int)(o_ptr - inventory);

        if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
            p_ptr->redraw |= (PR_QUIVER);
        else if (slot == INVEN_LITE)
            p_ptr->redraw |= (PR_LIGHT);
    }
}

/*
 * Removes curses from items in inventory.
 *
 * Note that Items bound by the Oath of Feanor (TR3_PERMA_CURSE)
 * can NEVER be uncursed by normal means - only the holy light
 * of items with the BREAKS_PERMA_CURSE flag can break such an oath.
 *
 * Note that if "all" is false, then Items which are
 * "Heavy-Cursed" (Mormegil, Calris, and Weapons of Morgul)
 * will not be uncursed.
 */
static int remove_curse_aux(bool star_curse)
{
    int i, cnt = 0;

    /* Attempt to uncurse items being worn */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        u32b f1, f2, f3;

        object_type* o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Uncursed already */
        if (!cursed_p(o_ptr))
            continue;

        /* Extract the flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        /* Heavily Cursed Items need a special spell */
        if (!star_curse && (f3 & (TR3_HEAVY_CURSE)))
            continue;

        /* Items bound by the Oath of Feanor can only be freed by a Silmaril */
        if (f3 & (TR3_PERMA_CURSE))
            continue;

        /* Uncurse the object */
        uncurse_object(o_ptr);

        /* Recalculate the bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Window stuff */
        p_ptr->window |= (PW_EQUIP);

        /* Count the uncursings */
        cnt++;
    }

    /* Return "something uncursed" */
    return (cnt);
}

/*
 * Remove most curses
 */
bool remove_curse(bool star_curse) { return (remove_curse_aux(star_curse)); }

static void append_resist_name(char* buf, size_t buf_len, const char* name)
{
    if (buf[0] != '\0')
        SDL_strlcat(buf, ", ", buf_len);
    SDL_strlcat(buf, name, buf_len);
}

static void append_resist_entry(char* buf, size_t buf_len, const char* name,
    int tier)
{
    if (tier <= 1) {
        append_resist_name(buf, buf_len, name);
        return;
    }

    char labeled[32];
    strnfmt(labeled, sizeof(labeled), "%s (x%d)", name, tier);
    append_resist_name(buf, buf_len, labeled);
}

static byte resist_color(const char* name)
{
    char base[32];
    size_t i = 0;

    while (name[i] && name[i] != ' ' && name[i] != '(' && i < sizeof(base) - 1) {
        base[i] = name[i];
        i++;
    }
    base[i] = '\0';

    if (streq(base, "fire"))
        return TERM_L_RED;
    if (streq(base, "cold"))
        return TERM_L_BLUE;
    if (streq(base, "poison"))
        return TERM_GREEN;
    if (streq(base, "bleeding"))
        return TERM_RED;
    if (streq(base, "fear"))
        return TERM_VIOLET;
    if (streq(base, "blindness"))
        return TERM_L_DARK;
    if (streq(base, "confusion"))
        return TERM_VIOLET;
    if (streq(base, "stunning"))
        return TERM_ORANGE;
    if (streq(base, "hallucination"))
        return TERM_VIOLET;

    return TERM_WHITE;
}

static bool render_resistance_summary(const char* text)
{
    const char* prefix_resist = "You resist ";
    const char* prefix_vuln = "You are vulnerable to ";
    const char* prefix_none = "You do not resist ";
    const char* prefix = NULL;

    if (strncmp(text, prefix_resist, strlen(prefix_resist)) == 0)
        prefix = prefix_resist;
    else if (strncmp(text, prefix_vuln, strlen(prefix_vuln)) == 0)
        prefix = prefix_vuln;
    else if (strncmp(text, prefix_none, strlen(prefix_none)) == 0)
        prefix = prefix_none;

    if (!prefix)
        return false;

    const char* list = text + strlen(prefix);
    if (!list[0])
        return false;

    text_out_hook = text_out_to_screen;
    text_out_indent = 1;
    text_out_wrap = Term->wid - 4;

    text_out_c(TERM_WHITE, prefix);

    const char* p = list;
    while (*p)
    {
        const char* comma = strstr(p, ", ");
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len > 0)
        {
            char token[32];
            size_t cap = sizeof(token) - 1;
            if (len > cap)
                len = cap;
            SDL_strlcpy(token, p, len + 1);
            text_out_c(resist_color(token), token);
        }

        if (!comma)
            break;

        text_out(", ");
        p = comma + 2;
    }

    return true;
}

/*
 * Hack -- acquire self knowledge
 *
 * List various information about the player and/or his current equipment.
 *
 * Use the "roff()" routines, perhaps.  XXX XXX XXX
 *
 * Use the "show_file()" method, perhaps.  XXX XXX XXX
 *
 * This function uses page wrapping and column management to ensure content 
 * stays within screen bounds. Long descriptions wrap to the next line.
 */
void self_knowledge(void)
{
    int i = 0, j, k;
    u32b f1 = 0L, f2 = 0L, f3 = 0L;
    object_type* o_ptr;
    
    char s[100][200];
    char t[100][200];
    bool good[100];
    bool identify[INVEN_TOTAL];
    
    int light = 0, mel = 0, arc = 0, stl = 0, medic = 0;

    if (p_ptr->update)
        update_stuff();

    if (level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px)
        != BIG_CAVE_NONE
        || ((cave_info[p_ptr->py][p_ptr->px]
            & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0))
    {
        log_partition_debug_for_point("self_knowledge", p_ptr->py, p_ptr->px);
        log_debug(
            "self_knowledge: base_fire=%d base_cold=%d base_pois=%d fear=%d stun=%d oppose_fire=%d oppose_cold=%d oppose_pois=%d effective_fire=%d effective_cold=%d effective_pois=%d",
            p_ptr->resist_fire, p_ptr->resist_cold, p_ptr->resist_pois,
            p_ptr->resist_fear, p_ptr->resist_stun, p_ptr->oppose_fire,
            p_ptr->oppose_cold, p_ptr->oppose_pois, resist_fire(),
            resist_cold(), resist_pois());
    }

    // Initialize arrays
    for (j = 0; j < 100; j++) {
        s[j][0] = '\0';
        t[j][0] = '\0';
        good[j] = true;
    }
    
    for (j = 0; j < INVEN_TOTAL; j++) {
        identify[j] = false;
    }

    // Get item flags from equipment
    for (k = INVEN_WIELD; k < INVEN_TOTAL; k++) {
        u32b t1, t2, t3, t4;
        o_ptr = &inventory[k];

        if (!o_ptr->k_idx) continue;

        object_flags4(o_ptr, &t1, &t2, &t3, &t4);

        {
            bool is_quiver1 = (k == INVEN_QUIVER1);
            bool is_quiver2 = (k == INVEN_QUIVER2);
            bool is_throwing_item = player_can_treat_as_throwing_flags(o_ptr, t3);

            if (is_quiver1)
                continue;
            if (is_quiver2 && !is_throwing_item)
                continue;
        }
        f1 |= t1; f2 |= t2; f3 |= t3;

        if (t2 & TR2_LIGHT) light++;
        if (t2 & TR2_DARKNESS) light--;
        if (t4 & TR4_UNLIGHT) light--;
        if (t1 & TR1_MEL) mel += o_ptr->skill_bonus[S_MEL];
        if (t1 & TR1_ARC) arc += o_ptr->skill_bonus[S_ARC];
        if (t1 & TR1_STL) stl += o_ptr->skill_bonus[S_STL];
        if (t3 & TR3_MEDIC) medic++;
    }

    // Add curse information
    int active_ids[64], n_active = 0;
    
    for (int id = 0; id < (int)z_info->cu_max && id < 64; id++) {
        if (CURSE_GET(id) > 0) active_ids[n_active++] = id;
    }
    
    // Add race/character trait information
    u32b rhf_bits = p_info[p_ptr->prace].flags | c_info[p_ptr->pcharacter].flags;
    u32b unq_bits = c_info[p_ptr->pcharacter].flags_u;
    int cand[64], n = 0;
    
    for (size_t idx = 0; idx < info_flags_desc_n && n < 64; idx++) {
        const flag_name *d = &info_flags_desc[idx];
        if ((d->set == RHF && (rhf_bits & d->flag)) || 
            (d->set == UNQ && (unq_bits & d->flag))) {
            cand[n++] = (int)idx;
        }
    }
    
    // Show either curse or flag information, not both
    bool show_curse = (n_active > 0) && one_in_(6);
    bool show_flag = (n > 0) && one_in_(6);
    
    if (show_curse) {
        int pick = active_ids[rand_int(n_active)];
        curse_type *c = &cu_info[pick];
        cptr cname = cu_name + c->name;
        cptr cdesc = cu_text + c->text;
        cptr cpower = cu_text + c->power;
        
        strnfmt(s[i], 200, "A shadow upon you: %s", cname);
        strnfmt(t[i], 200, "%s  %s", cdesc, cpower);
        good[i] = false;
        i++;
        CURSE_SEEN_SET(pick);
    }
    if (show_flag) {
        const flag_name *d = &info_flags_desc[cand[rand_int(n)]];
        strnfmt(s[i], 200, "You sense a hidden trait.");
        strnfmt(t[i], 200, "%s", d->name);
        good[i] = true;
        i++;
    }

    // Equipment-based traits
    if (f2 & TR2_TRAITOR) {
        strnfmt(s[i], 80, "You feel doom hastening toward you");
        strnfmt(t[i], 80, "(you will be betrayed)");
        good[i] = false; i++;
    }
    
    if (f3 & TR3_CHEAT_DEATH) {
        strnfmt(s[i], 80, "You are protected from serious harm");
        strnfmt(t[i], 80, "(you will survive a killing blow)");
        good[i] = true; i++;
    }
    
    if (f3 & TR3_AVOID_TRAPS) {
        strnfmt(s[i], 80, "Your feet do not trigger traps");
        strnfmt(t[i], 80, "(does not protect from webs, roosts and pits)");
        good[i] = true; i++;
    }
    
    if (medic > 0) {
        strnfmt(s[i], 80, "You gain extra health from healing items");
        strnfmt(t[i], 80, "(%d%%)", 33 * medic);
        good[i] = true; i++;
    }
    
    if (f3 & TR3_STAND_FAST) {
        strnfmt(s[i], 80, "You stand fast against your foes");
        strnfmt(t[i], 80, "(you cannot be moved by enemy abilities)");
        good[i] = true; i++;
    }

    if (p_ptr->see_inv > 0) {
        strnfmt(s[i], 80, "You can see invisible creatures");
        t[i][0] = '\0';
        good[i] = true; i++;
    }

    if (p_ptr->free_act > 0) {
        strnfmt(s[i], 80, "You move freely");
        t[i][0] = '\0';
        good[i] = true; i++;
    }

    if (p_ptr->regenerate > 0) {
        strnfmt(s[i], 80, "You regenerate quickly");
        t[i][0] = '\0';
        good[i] = true; i++;
    }

    {
        char resist_buf[200];
        char no_resist_buf[200];
        char vuln_buf[200];
        int res;

        resist_buf[0] = '\0';
        no_resist_buf[0] = '\0';
        vuln_buf[0] = '\0';

        res = resist_fire();
        if (res > 1) {
            append_resist_entry(resist_buf, sizeof(resist_buf), "fire",
                res - 1);
        }
        else if (res < 1) {
            int tier = (-res) - 1;
            if (tier < 1)
                tier = 1;
            append_resist_entry(vuln_buf, sizeof(vuln_buf), "fire", tier);
        }
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "fire");

        res = resist_cold();
        if (res > 1) {
            append_resist_entry(resist_buf, sizeof(resist_buf), "cold",
                res - 1);
        }
        else if (res < 1) {
            int tier = (-res) - 1;
            if (tier < 1)
                tier = 1;
            append_resist_entry(vuln_buf, sizeof(vuln_buf), "cold", tier);
        }
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "cold");

        res = resist_pois();
        if (res > 1) {
            append_resist_entry(resist_buf, sizeof(resist_buf), "poison",
                res - 1);
        }
        else if (res < 1) {
            int tier = (-res) - 1;
            if (tier < 1)
                tier = 1;
            append_resist_entry(vuln_buf, sizeof(vuln_buf), "poison", tier);
        }
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "poison");

        res = p_ptr->resist_bleed;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "bleeding");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "bleeding");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "bleeding");

        res = p_ptr->resist_fear;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "fear");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "fear");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "fear");

        res = p_ptr->resist_blind;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "blindness");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "blindness");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "blindness");

        res = p_ptr->resist_confu;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "confusion");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "confusion");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "confusion");

        res = p_ptr->resist_stun;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "stunning");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "stunning");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "stunning");

        res = p_ptr->resist_hallu;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "hallucination");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "hallucination");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "hallucination");

        if (resist_buf[0] != '\0') {
            strnfmt(s[i], 200, "You resist %s", resist_buf);
            t[i][0] = '\0';
            good[i] = true; i++;
        }

        if (vuln_buf[0] != '\0') {
            strnfmt(s[i], 200, "You are vulnerable to %s", vuln_buf);
            t[i][0] = '\0';
            good[i] = false; i++;
        }

        if (no_resist_buf[0] != '\0') {
            strnfmt(s[i], 200, "You do not resist %s", no_resist_buf);
            t[i][0] = '\0';
            good[i] = false; i++;
        }
    }

    // Player state information
    if (p_ptr->pspeed < 2) {
        strnfmt(s[i], 80, "You are moving slowly");
        strnfmt(t[i], 80, "(speed %d)", p_ptr->pspeed);
        good[i] = false; i++;
    } else if (p_ptr->pspeed > 2) {
        strnfmt(s[i], 80, "You are moving quickly");
        strnfmt(t[i], 80, "(speed %d)", p_ptr->pspeed);
        good[i] = true; i++;
    }
    
    if (p_ptr->stealth_mode) {
        strnfmt(s[i], 80, "You are moving carefully");
        strnfmt(t[i], 80, "(+5 Stealth)");
        good[i] = true; i++;
    }

    // Hunger effects
    if (p_ptr->hunger < 0) {
        strnfmt(s[i], 80, "You grow hungry %sslowly", (p_ptr->hunger < -1) ? "very " : "");
        strnfmt(t[i], 80, "(1/%d the normal rate)", int_exp(3, -p_ptr->hunger));
        good[i] = true; i++;
    } else if (p_ptr->hunger > 0) {
        strnfmt(s[i], 80, "You burn with a%s unnatural hunger", (p_ptr->hunger > 1) ? " most" : "n");
        strnfmt(t[i], 80, "(%d times the normal rate)", int_exp(3, p_ptr->hunger));
        good[i] = false; i++;
    }

    // Status effects
    struct { bool condition; const char* text; const char* detail; bool is_good; } status_effects[] = {
        {p_ptr->blind, "You cannot see", "", false},
        {p_ptr->image, "You are hallucinating", "", false},
        {p_ptr->confused, "You are confused", "", false},
        {p_ptr->afraid, "You are terrified", "", false},
        {p_ptr->cut, "You are bleeding", "", false},
        {p_ptr->poisoned, "You are poisoned", "", false},
        {p_ptr->rage, "You are in a dark rage", "", false},
        {0, NULL, NULL, false} // Sentinel
    };
    
    for (int idx = 0; status_effects[idx].text; idx++) {
        if (status_effects[idx].condition) {
            strnfmt(s[i], 80, "%s", status_effects[idx].text);
            strnfmt(t[i], 80, "%s", status_effects[idx].detail);
            good[i] = status_effects[idx].is_good;
            i++;
        }
    }
    
    // Stun with special handling
    if (p_ptr->stun) {
        strnfmt(s[i], 80, "You are %sstunned", (p_ptr->stun <= 50) ? "heavily " : "");
        strnfmt(t[i], 80, "(-%d to all skills)", (p_ptr->stun <= 50) ? 2 : 4);
        good[i] = false; i++;
    }

    // Temporary stat boosts
    struct { bool condition; const char* text; const char* detail; } temp_stats[] = {
        {p_ptr->tmp_str, "You feel stronger", "(+3 Strength)"},
        {p_ptr->tmp_dex, "You feel more agile", "(+3 Dexterity)"},
        {p_ptr->tmp_con, "You feel more resilient", "(+3 Constitution)"},
        {p_ptr->tmp_gra, "You feel more attuned to the world", "(+3 Grace)"},
        {p_ptr->tmp_per, "Your perceptions are heightened", "(+10 Perception)"},
        {0, NULL, NULL} // Sentinel
    };
    
    for (int idx = 0; temp_stats[idx].text; idx++) {
        if (temp_stats[idx].condition) {
            strnfmt(s[i], 80, "%s", temp_stats[idx].text);
            strnfmt(t[i], 80, "%s", temp_stats[idx].detail);
            good[i] = true;
            i++;
        }
    }

    // Add equipment stat modifiers
    const char* stat_names[] = {"Strength", "Dexterity", "Constitution", "Grace"};
    for (int stat = 0; stat < 4; stat++) {
        if (p_ptr->stat_equip_mod[stat] != 0) {
            strnfmt(s[i], 80, "Your %s is affected by your equipment", 
                    (stat == A_STR) ? "strength" : (stat == A_DEX) ? "dexterity" : 
                    (stat == A_CON) ? "constitution" : "grace");
            strnfmt(t[i], 80, "(%+d %s)", p_ptr->stat_equip_mod[stat], stat_names[stat]);
            good[i] = (p_ptr->stat_equip_mod[stat] > 0);
            i++;
        }
    }

    // Add skill modifiers
    if (mel != 0) {
        strnfmt(s[i], 80, "Your melee is affected by your equipment");
        strnfmt(t[i], 80, "(%+d Melee)", mel);
        good[i] = (mel > 0); i++;
    }
    if (arc != 0) {
        strnfmt(s[i], 80, "Your archery is affected by your equipment");
        strnfmt(t[i], 80, "(%+d Archery)", arc);
        good[i] = (arc > 0); i++;
    }
    if (stl != 0) {
        strnfmt(s[i], 80, "Your stealth is affected by your equipment");
        strnfmt(t[i], 80, "(%+d Stealth)", stl);
        good[i] = (stl > 0); i++;
    }

    // Light effects
    if (light > 0) {
        strnfmt(s[i], 80, "Your equipment glows with an inner light");
        strnfmt(t[i], 80, "(%+d radius)", light);
        good[i] = true; i++;
    } else if (light < 0) {
        strnfmt(s[i], 80, "Your equipment radiates an unnatural darkness");
        strnfmt(t[i], 80, "(%+d radius)", light);
        good[i] = false; i++;
    }

    // Analyze weapons and equipment for special properties
    analyze_weapon_properties(&i, s, t, good, identify, INVEN_WIELD, "weapon");
    if (p_ptr->mds2 > 0) {
        analyze_weapon_properties(&i, s, t, good, identify, INVEN_ARM, "off-hand weapon");
    }
    analyze_weapon_properties(&i, s, t, good, identify, INVEN_BOW, "bow");

    // Add abilities from equipment
    for (j = 0; j < S_MAX; j++) {
        for (k = 0; k < ABILITIES_MAX; k++) {
            if (p_ptr->have_ability[j][k] && !p_ptr->innate_ability[j][k]) {
                strnfmt(s[i], 80, "Your equipment grants you the ability: %s",
                        b_name + (&b_info[ability_index(j, k)])->name);
                t[i][0] = '\0'; // No detail text
                good[i] = true;
                i++;
            }
        }
    }

    // Display the information
    display_attributes(s, t, good, i);
    
    // Identify items that revealed information
    identify_revealed_items(identify);
}

// Helper function to analyze weapon properties
void analyze_weapon_properties(int* count, char s[][200], char t[][200], bool good[], 
                              bool identify[], int slot, const char* weapon_name)
{
    object_type* o_ptr = &inventory[slot];
    if (!o_ptr->k_idx) return;
    
    u32b f1, f2, f3, f4;
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    int i = *count;
    
    // Special attack bonuses
    if (f1 & TR1_SHARPNESS) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s cuts easily through armour", weapon_name);
        strnfmt(t[i], 200, "(ignore 50%% of protection)");
        good[i] = true; i++;
    }
    
    if (f1 & TR1_SHARPNESS2) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s cuts exceptionally easily through armour", weapon_name);
        strnfmt(t[i], 200, "(ignore 100%% of protection)");
        good[i] = true; i++;
    }
    
    if (f1 & TR1_VAMPIRIC) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s drains life from your enemies", weapon_name);
        strnfmt(t[i], 200, "(+7 health per kill)");
        good[i] = true; i++;
    }
    
    if (f3 & TR3_ACCURATE) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s %s", weapon_name, 
                (slot == INVEN_BOW) ? "fires with unerring precision" : "is unusually well balanced");
        strnfmt(t[i], 200, "(reroll missed attacks)");
        good[i] = true; i++;
    }
    
    if (f3 & TR3_CUMBERSOME) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s is cumbersome", weapon_name);
        strnfmt(t[i], 200, "(no critical hits)");
        good[i] = false; i++;
    }

    // Brand effects
    const char* brand_names[] = {"shocks", "burns", "freezes", "poisons"};
    u32b brand_flags[] = {TR1_BRAND_ELEC, TR1_BRAND_FIRE, TR1_BRAND_COLD, TR1_BRAND_POIS};
    
    for (int b = 0; b < 4; b++) {
        if (f1 & brand_flags[b]) {
            identify[slot] = true;
            strnfmt(s[i], 200, "Your %s %s your foes", weapon_name, brand_names[b]);
            strnfmt(t[i], 200, "(+1 damage die)");
            good[i] = true; i++;
        }
    }

    // Slay effects
    typedef struct {
        u32b flag;
        int flagset;
        const char* name;
        bool use_effective;
    } slay_attr_t;

    const slay_attr_t slays[] = {
        { TR1_SLAY_ORC, 1, "orcs", false },
        { TR1_SLAY_TROLL, 1, "trolls", false },
        { TR1_SLAY_WOLF, 1, "wolves", false },
        { TR1_SLAY_SPIDER, 1, "spiders", false },
        { TR1_SLAY_RAUKO, 1, "raukar", false },
        { TR1_SLAY_DRAGON, 1, "dragons", false },
        { TR1_SLAY_UNDEAD, 1, "the undead", true },
        { TR4_SLAY_SERPENT, 4, "serpents", false },
        { TR4_SLAY_VAMPIRE, 4, "vampires", false },
        { TR4_SLAY_HORROR, 4, "horrors", true },
        { TR4_SLAY_CAT, 4, "cats", false },
        { TR4_SLAY_GIANT, 4, "giants", false },
    };

    for (size_t sl = 0; sl < (sizeof(slays) / sizeof(slays[0])); sl++) {
        u32b flags = (slays[sl].flagset == 4) ? f4 : f1;
        if (flags & slays[sl].flag) {
            identify[slot] = true;
            strnfmt(s[i], 200, "Your %s is especially %s against %s", weapon_name,
                    slays[sl].use_effective ? "effective" : "deadly", slays[sl].name);
            strnfmt(t[i], 200, "(+1 damage die)");
            good[i] = true; i++;
        }
    }

    if (f1 & TR1_SLAY_MAN_OR_ELF) {
        identify[slot] = true;
        strnfmt(s[i], 80, "Your %s is especially effective against men", weapon_name);
        strnfmt(t[i], 80, "(+1 damage die)");
        good[i] = true; i++;
        strnfmt(s[i], 80, "Your %s is especially effective against elves", weapon_name);
        strnfmt(t[i], 80, "(+1 damage die)");
        good[i] = true; i++;
    }
    
    *count = i;
}

// Helper function to display attributes with scrolling.
void display_attributes(char s[][200], char t[][200], bool good[], int count)
{
    self_knowledge_capture capture;

    SDL_memset(&capture, 0, sizeof(capture));
    ui_menu_click_clear();
    ui_scroll_area_clear();

    if (!self_knowledge_capture_build(s, t, good, count, &capture))
        return;

    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    self_knowledge_capture_view(&capture);
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    screen_load();

    self_knowledge_capture_free(&capture);
    ui_menu_click_clear();
    ui_scroll_area_clear();
}

// Helper function to identify revealed items
void identify_revealed_items(bool identify[])
{
    for (int i = 0; i < INVEN_TOTAL; i++) {
        if (identify[i]) {
            object_type* o_ptr = &inventory[i];
            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
                continue;
            }

            if (!object_known_p(o_ptr))
            {
                char o_short_name[80], o_full_name[80];

                object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);
                ident(o_ptr);
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format(
                    "You realize that your %s is %s.", o_short_name, o_full_name);
            }
        }
    }
}

/*
 * Detect all doors and traps on the whole level.
 * Used to show map at end of the game.
 */
void detect_all_doors_traps()
{
    int y, x;

    /* Scan the visible area */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            /* Detect invisible traps */
            if (cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
            {
                /* Reveal the trap */
                reveal_trap(y, x);
            }

            /* Detect secret doors */
            if (cave_feat[y][x] == FEAT_SECRET)
            {
                /* Pick a door */
                place_closed_door(y, x);

                /* Hack -- Memorize */
                cave_info[y][x] |= (CAVE_MARK);

                /* Redraw */
                lite_spot(y, x);
            }
        }
    }
}

/*
 * Detect all traps in sight
 */
bool detect_traps(void)
{
    int y, x;

    bool detect = false;

    /* Scan the visible area */
    for (y = p_ptr->py - MAX_SIGHT; y < p_ptr->py + MAX_SIGHT; y++)
    {
        for (x = p_ptr->px - MAX_SIGHT; x < p_ptr->px + MAX_SIGHT; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!player_can_see_bold(y, x))
                continue;

            /* Detect invisible traps */
            if (cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
            {
                /* Reveal the trap */
                reveal_trap(y, x);

                /* Obvious */
                detect = true;
            }
        }
    }

    /* Describe */
    if (detect)
    {
        msg_print("You sense the presence of traps!");
    }

    /* Result */
    return (detect);
}

/*
 * Detect all doors in sight
 */
bool detect_doors(void)
{
    int y, x;

    bool detect = false;

    /* Scan the visible area */
    for (y = p_ptr->py - MAX_SIGHT; y < p_ptr->py + MAX_SIGHT; y++)
    {
        for (x = p_ptr->px - MAX_SIGHT; x < p_ptr->px + MAX_SIGHT; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!player_can_see_bold(y, x))
                continue;

            /* Detect secret doors */
            if (cave_feat[y][x] == FEAT_SECRET)
            {
                /* Pick a door */
                place_closed_door(y, x);

                /* Hack -- Memorize */
                cave_info[y][x] |= (CAVE_MARK);

                /* Redraw */
                lite_spot(y, x);

                detect = true;
            }
        }
    }

    /* Result */
    return (detect);
}

/*
 * Detect all stairs within 20 squares
 */
bool detect_stairs(void)
{
    int y, x;

    bool detect = false;

    /* Scan the visible area */
    for (y = p_ptr->py - 20; y < p_ptr->py + 20; y++)
    {
        for (x = p_ptr->px - 20; x < p_ptr->px + 20; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            /* Detect stairs */
            if (cave_stair_bold(y, x))
            {
                /* Hack -- Memorize */
                cave_info[y][x] |= (CAVE_MARK);

                /* Redraw */
                lite_spot(y, x);

                /* Obvious */
                detect = true;
            }
        }
    }

    /* Describe */
    if (detect)
    {
        msg_print("You sense the presence of stairs!");
    }

    /* Result */
    return (detect);
}

/*
 * Detect all "normal" objects on the current panel
 */
bool detect_objects_normal(int radius)
{
    int i, y, x;

    bool detect = false;

    /* Scan objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        // Skip staffs of treasures (cute, and helps prevent run-away detection
        // spiral)
        if ((o_ptr->tval == TV_STAFF) && (o_ptr->sval == SV_STAFF_TREASURES))
            continue;

        /* Location */
        y = o_ptr->iy;
        x = o_ptr->ix;

        /* Only detect nearby objects (within radius if specified) */
        if (radius > 0)
        {
            int dist = distance(p_ptr->py, p_ptr->px, y, x);
            if (dist > radius)
                continue;
        }

        /* Hack -- memorize it */
        o_ptr->marked = true;

        if (o_ptr->name1)
        {
            a_info[o_ptr->name1].seen |= ART_SEEN_PHYSICAL;
            o_ptr->ident |= IDENT_ARTIFACT_SEEN;
        }

        /* Detection reveals easy smithing items (no distance penalty). */
        (void)player_auto_identify_smithing_object(o_ptr, true);

        /* Redraw */
        lite_spot(y, x);

        /* Detect */
        detect = true;
    }

    /* Scan monsters, looking for object-like ones */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* we want to detect object-like monsters */
        if (!(strchr("|!?-_=~", r_ptr->d_char)))
            continue;

        /* Location */
        y = m_ptr->fy;
        x = m_ptr->fx;

        /*mark them*/
        m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

        /* Optimize -- Repair flags */
        repair_mflag_mark = true;
        repair_mflag_show = true;

        /* Update the monster */
        update_mon(i, false);

        /* Detect */
        detect = true;
    }

    /* Describe */
    if (detect)
    {
        msg_print("You sense the presence of objects!");
    }

    /* Result */
    return (detect);
}

/*
 * Detect all "magic" objects on the current panel.
 *
 * This will light up all spaces with "magic" items, including artefacts,
 * special items, potions, staves, amulets, rings.
 */
bool detect_objects_magic(void)
{
    int i, y, x, tv;

    bool detect = false;

    /* Scan all objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        /* Location */
        y = o_ptr->iy;
        x = o_ptr->ix;

        /* Only detect nearby objects */
        if (!panel_contains(y, x))
            continue;

        /* Examine the tval */
        tv = o_ptr->tval;

        /* Artefacts, misc magic items, or ego items */
        if (artefact_p(o_ptr) || ego_item_p(o_ptr) || (tv == TV_AMULET)
            || (tv == TV_RING) || (tv == TV_STAFF) || (tv == TV_HORN)
            || (tv == TV_POTION))
        {
            /* Memorize the item */
            o_ptr->marked = true;

            /* Redraw */
            lite_spot(y, x);

            /* Detect */
            detect = true;
        }
    }

    /* Describe */
    if (detect)
    {
        msg_print("You sense the presence of enchantments!");
    }

    /* Return result */
    return (detect);
}

/*
 * Detect all "normal" monsters on the current panel
 */
bool detect_monsters(int radius)
{
    int i;

    bool flag = false;

    /* Scan monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Only detect monsters within radius if specified */
        if (radius > 0)
        {
            int dist = distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx);
            if (dist > radius)
                continue;
        }

        /* Optimize -- Repair flags */
        repair_mflag_mark = true;
        repair_mflag_show = true;

        /* Hack -- Detect the monster */
        m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

        /* Update the monster */
        update_mon(i, false);

        /* Detect */
        flag = true;
    }

    /* Describe */
    if (flag)
    {
        /* Describe result */
        msg_print("You sense the presence of your enemies!");
    }

    /* Result */
    return (flag);
}

/*
 * Detect all "invisible" monsters in sight
 */
bool detect_monsters_invis(void)
{
    int i;

    bool flag = false;

    /* Scan monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        monster_lore* l_ptr = &l_list[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Detect invisible monsters */
        if (r_ptr->flags2 & (RF2_INVISIBLE))
        {
            /* Take note that they are invisible */
            l_ptr->flags2 |= (RF2_INVISIBLE);

            /* Update monster recall window */
            if (p_ptr->monster_race_idx == m_ptr->r_idx)
            {
                /* Window stuff */
                p_ptr->window |= (PW_MONSTER);
            }

            /* Optimize -- Repair flags */
            repair_mflag_mark = true;
            repair_mflag_show = true;

            /* Hack -- Detect the monster */
            m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

            /* Update the monster */
            update_mon(i, false);

            /* Detect */
            flag = true;
        }
    }

    /* Describe */
    if (flag)
    {
        /* Describe result */
        msg_print("You sense the presence of invisible creatures!");
    }

    /* Result */
    return (flag);
}

/*
 * Detect everything
 */
bool detect_all(void)
{
    bool detect = false;

    /* Detect everything */
    if (detect_traps())
        detect = true;
    if (detect_doors())
        detect = true;
    if (detect_stairs())
        detect = true;
    if (detect_objects_normal(0))
        detect = true;
    if (detect_monsters_invis())
        detect = true;
    if (detect_monsters(0))
        detect = true;

    /* Result */
    return (detect);
}

/*
 * Create stairs at the player location
 */
void stair_creation(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    /* XXX XXX XXX */
    if (!cave_valid_bold(py, px))
    {
        msg_print("The object resists the spell.");
        return;
    }

    /* XXX XXX XXX */
    delete_object(py, px);

    place_random_stairs(py, px);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_digger(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    object_flags(o_ptr, &f1, &f2, &f3);

    if ((f1 & (TR1_TUNNEL)) && (o_ptr->pval > 0))
    {
        return (true);
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_wieldable_ided_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_wieldable_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    case TV_BOW:
    case TV_ARROW:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_ided_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    case TV_BOW:
    case TV_ARROW:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "armour"
 */
bool item_tester_hook_ided_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_BOOTS:
    case TV_GLOVES:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "armour"
 */
bool item_tester_hook_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_BOOTS:
    case TV_GLOVES:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify non-herb food
 */
bool item_tester_hook_non_herb_food(const object_type* o_ptr)
{
    if ((o_ptr->tval == TV_FOOD) && (o_ptr->pval > 300))
        return (true);

    return (false);
}

/*
 * Hook to specify light with fuel or that does not need fuel
 */
bool item_tester_hook_light_with_fuel(const object_type* o_ptr)
{
    if (o_ptr->tval != TV_LIGHT)
        return (false);

    if (o_ptr->timeout < 1 && fuelable_light_p(o_ptr))
        return (false);

    return (true);
}

/*
 * Hook to specify "enchantable amulet"
 */
bool item_tester_hook_enchantable_amulet(const object_type* o_ptr)
{
    if ((o_ptr->tval == TV_AMULET) && (o_ptr->pval > 0))
        return (true);

    return (false);
}

/*
 * Identify an object chosen from the unified unidentified list.
 * Returns true if an item was identified.
 */
bool ident_spell(bool include_floor)
{
    int item;
    object_type* o_ptr;

    if (!display_unified_identify_menu(include_floor, &item, &o_ptr))
        return false;

    int squelch = do_ident_item(item, o_ptr);
    do_squelch_item(squelch, item, o_ptr);

    return true;
}

/*
 * Hook for "get_item()".  Determine if something is rechargable.
 */
bool item_tester_hook_recharge(const object_type* o_ptr)
{
    /* Recharge staffs */
    if (o_ptr->tval == TV_STAFF)
        return (true);

    /* Nope */
    return (false);
}

typedef struct recharge_target_entry
{
    int item;
    object_type* o_ptr;
} recharge_target_entry;

enum
{
    MAX_RECHARGE_TARGETS =
        INVEN_PACK + (INVEN_TOTAL - INVEN_WIELD) + MAX_FLOOR_STACK
};

static int recharge_collect_targets(recharge_target_entry entries[],
    int max_entries)
{
    int count = 0;
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    if (!entries || max_entries <= 0)
        return 0;

    for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < max_entries; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_hook_recharge(o_ptr))
            continue;

        entries[count].item = i;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    for (int i = 0; i < INVEN_PACK && count < max_entries; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_hook_recharge(o_ptr))
            continue;

        entries[count].item = i;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);
    for (int i = 0; i < floor_num && count < max_entries; i++)
    {
        int o_idx = floor_list[i];
        object_type* o_ptr = &o_list[o_idx];

        if (!item_tester_hook_recharge(o_ptr))
            continue;

        entries[count].item = 0 - o_idx;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    return count;
}

static bool recharge_choose_target(const recharge_target_entry entries[],
    int count, int* out_item)
{
    int current = 0;
    int top = 0;
    int term_wid = 80;
    int term_hgt = 24;
    int list_row = 2;
    int help_row;
    int prompt_row;
    int page_size;
    bool steamdeck = steamdeck_controls_active();

    if (!entries || count <= 0 || !out_item)
        return false;

    if (Term)
        Term_get_size(&term_wid, &term_hgt);

    if (term_wid < 40)
        term_wid = 40;
    if (term_hgt < 8)
        term_hgt = 8;

    help_row = term_hgt - 2;
    prompt_row = term_hgt - 1;
    page_size = help_row - list_row;
    if (page_size < 1)
        page_size = 1;

    screen_save();

    while (true)
    {
        int visible_count;
        char buf[160];
        char key;

        if (current < top)
            top = current;
        if (current >= top + page_size)
            top = current - page_size + 1;

        visible_count = count - top;
        if (visible_count > page_size)
            visible_count = page_size;

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_begin(list_row, help_row - 1,
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_set_keys('8', '2', '6', '4');

        prt("Recharge which staff?", 0, 0);
        strnfmt(buf, sizeof(buf),
            "%d rechargeable target%s",
            count, (count == 1) ? "" : "s");
        prt(buf, 1, 0);

        for (int i = 0; i < visible_count; i++)
        {
            int row = list_row + i;
            int item = entries[top + i].item;
            object_type* o_ptr = entries[top + i].o_ptr;
            char prefix[32];
            char desc[80];
            char label[4];
            int desc_col = steamdeck ? 17 : 20;
            int max_desc = term_wid - desc_col - 1;
            bool highlighted = (top + i == current);
            byte label_attr = highlighted ? TERM_L_BLUE : TERM_WHITE;
            byte desc_attr;

            if (max_desc < 0)
                max_desc = 0;
            if (max_desc >= (int)sizeof(desc))
                max_desc = (int)sizeof(desc) - 1;

            if (item >= 0)
            {
                strnfmt(prefix, sizeof(prefix), "%-12s:", mention_use(item));
                object_desc(desc, sizeof(desc), o_ptr, true, 3);
            }
            else
            {
                strnfmt(prefix, sizeof(prefix), "%-12s:", "On floor");
                object_desc_floor(desc, sizeof(desc), o_ptr, true, 3);
            }
            desc[max_desc] = '\0';

            desc_attr = highlighted
                ? TERM_L_BLUE
                : object_display_color(o_ptr,
                    tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

            if (!steamdeck && i < 26)
                strnfmt(label, sizeof(label), "%c)", I2A(i));
            else
                SDL_strlcpy(label, "  ", sizeof(label));

            Term_erase(0, row, 255);
            Term_putstr(0, row, 2, label_attr, highlighted ? "> " : "  ");
            if (steamdeck)
                Term_putstr(2, row, -1, label_attr, prefix);
            else
            {
                Term_putstr(2, row, -1, label_attr, label);
                Term_putstr(5, row, -1, label_attr, prefix);
            }
            Term_putstr(desc_col, row, -1, desc_attr, desc);
            ui_menu_click_add_full_row(top + i, row);
        }

        for (int i = list_row + visible_count; i < help_row; i++)
            Term_erase(0, i, 255);

        if (count > page_size)
        {
            strnfmt(buf, sizeof(buf),
                "Showing %d-%d of %d",
                top + 1, top + visible_count, count);
            prt(buf, help_row, 0);
        }
        else
        {
            prt("", help_row, 0);
        }

        if (steamdeck)
        {
            char confirm_label[16];
            char back_label[16];
            char prompt_buf[80];

            spells2_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            spells2_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                "D-pad choose, %s select, %s cancel", confirm_label,
                back_label);
            prt(prompt_buf, prompt_row, 0);
            ui_menu_click_add_text_token(-2, 0, prompt_row, prompt_buf,
                "select");
            ui_menu_click_add_text_token(-1, 0, prompt_row, prompt_buf,
                "cancel");
        }
        else
        {
            cptr prompt_text =
                "Letters/8/2/arrows choose, Enter select, ESC cancel";
            prt(prompt_text, prompt_row, 0);
            ui_menu_click_add_text_token(-2, 0, prompt_row, prompt_text,
                "select");
            ui_menu_click_add_text_token(-1, 0, prompt_row, prompt_text,
                "cancel");
        }
        Term_fresh();

        key = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < count)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != current)
                    {
                        current = clicked_choice;
                        continue;
                    }
                    key = '\r';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = '\r';
            }
        }

        if (steamdeck && key == steamdeck_back_key())
        {
            ui_menu_click_clear();
            ui_scroll_area_clear();
            screen_load();
            return false;
        }

        switch (key)
        {
        case ESCAPE:
            ui_menu_click_clear();
            ui_scroll_area_clear();
            screen_load();
            return false;

        case '\r':
        case '\n':
        case ' ':
#ifdef KC_ENTER
        case KC_ENTER:
#endif
            *out_item = entries[current].item;
            ui_menu_click_clear();
            ui_scroll_area_clear();
            screen_load();
            return true;

        case '8':
        case 'k':
        case 'K':
#ifdef ARROW_UP
        case ARROW_UP:
#endif
            current = (current > 0) ? current - 1 : count - 1;
            break;

        case '2':
        case 'j':
        case 'J':
#ifdef ARROW_DOWN
        case ARROW_DOWN:
#endif
            current = (current + 1 < count) ? current + 1 : 0;
            break;

        default:
        {
            int pick;

            if (steamdeck && key == steamdeck_back_key())
            {
                ui_menu_click_clear();
                ui_scroll_area_clear();
                screen_load();
                return false;
            }

            if (steamdeck && key == steamdeck_confirm_key())
            {
                *out_item = entries[current].item;
                ui_menu_click_clear();
                ui_scroll_area_clear();
                screen_load();
                return true;
            }

            if (steamdeck)
                break;

            if (!isalpha((unsigned char)key))
                break;

            pick = A2I((char)tolower((unsigned char)key));
            if (pick >= 0 && pick < visible_count)
            {
                *out_item = entries[top + pick].item;
                ui_menu_click_clear();
                ui_scroll_area_clear();
                screen_load();
                return true;
            }

            break;
        }
        }
    }
}

/*
 * Recharge a staff from the pack, equipment, or on the floor.
 *
 * Mage -- Recharge I --> recharge(5)
 * Mage -- Recharge II --> recharge(40)
 * Mage -- Recharge III --> recharge(100)
 *
 * Priest -- Recharge --> recharge(15)
 *
 * Scroll of recharging --> recharge(60)
 *
 * recharge(20) = 1/6 failure for empty 10th level wand
 * recharge(60) = 1/10 failure for empty 10th level wand
 *
 * It is harder to recharge high level, and highly charged wands.
 *
 * XXX XXX XXX Beware of "sliding index errors".
 *
 * Should probably not "destroy" over-charged items, unless we
 * "replace" them by, say, a broken stick or some such.  The only
 * reason this is okay is because "scrolls of recharging" appear
 * BEFORE all staves/wands/rods in the inventory.  Note that the
 * new "auto_sort_pack" option would correctly handle replacing
 * the "broken" wand with any other item (i.e. a broken stick).
 *
 */
bool recharge(int num)
{
    int item;
    int target_count;

    object_type* o_ptr;
    recharge_target_entry targets[MAX_RECHARGE_TARGETS];

    target_count = recharge_collect_targets(targets, N_ELEMENTS(targets));
    if (target_count <= 0)
    {
        msg_print("You have nothing to recharge.");
        return (false);
    }

    if (!recharge_choose_target(targets, target_count, &item))
        return (false);

    /* Get the item (in the pack) */
    if (item >= 0)
        o_ptr = &inventory[item];

    /* Get the item (on the floor) */
    else
        o_ptr = &o_list[0 - item];

    /* Attempt to Recharge a staff, or handle failure to recharge . */
    if (o_ptr->tval == TV_STAFF)
    {
        if (o_ptr->sval == SV_STAFF_RECHARGING
            && p_ptr->active_ability[S_WIL][WIL_CHANNELING])
        {
            num /= 2;
        }

        /* Recharge the staff. */
        o_ptr->pval += num;

        if (object_aware_p(o_ptr) && (o_ptr->ident & (IDENT_EMPTY)))
        {
            object_aware(o_ptr);
            object_known(o_ptr);
        }

        /* Hack -- we no longer think the item is empty */
        o_ptr->ident &= ~(IDENT_EMPTY);
    }

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Something was done */
    return (true);
}

/************************************************************************
 *                                                                      *
 *                           Projection types                           *
 *                                                                      *
 ************************************************************************/

/*
 * Handle bolt spells.
 *
 * Bolts stop as soon as they hit a monster, whiz past missed targets, and
 * (almost) never affect items on the floor.
 */
bool project_bolt(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg)
{
    /* Add the bolt bitflags */
    flg |= PROJECT_STOP | PROJECT_KILL | PROJECT_THRU;

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= PROJECT_PLAY;

    /* Limit range */
    if ((rad > MAX_RANGE) || (rad <= 0))
        rad = MAX_RANGE;

    /* Cast a bolt */
    return (project(who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, 0, false));
}

/*
 * Handle beam spells.
 *
 * Beams affect every grid they touch, go right through monsters, and
 * (almost) never affect items on the floor.
 */
bool project_beam(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg)
{
    /* Add the beam bitflags */
    flg |= PROJECT_BEAM | PROJECT_KILL | PROJECT_THRU;

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= (PROJECT_PLAY);

    /* Limit range */
    if ((rad > MAX_RANGE) || (rad <= 0))
        rad = MAX_RANGE;

    /* Cast a beam */
    return (project(who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, 0, false));
}

/*
 * Handle ball spells.
 *
 * Balls act like bolt spells, except that they do not pass their target,
 * and explode when they hit a monster, a wall, their target, or the edge
 * of sight.  Within the explosion radius, they affect items on the floor.
 *
 * Balls may jump to the target, and have any source diameter (which affects
 * how quickly their damage falls off with distance from the center of the
 * explosion).
 */
bool project_ball(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, bool uniform)
{
    /* Add the ball bitflags */
    flg |= PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;

    /* Add the STOP flag if appropriate */
    if ((who < 0)
        && (!target_okay(0) || y1 != p_ptr->target_row
            || x1 != p_ptr->target_col))
    {
        flg |= (PROJECT_STOP);
    }

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= (PROJECT_PLAY);

    /* Limit radius to nine (up to 256 grids affected) */
    if (rad > 9)
        rad = 9;

    /* Cast a ball */
    return (
        project(who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, 0, uniform));
}

/*
 * Handle ball spells that explode immediately on the target and
 * hurt everything.
 */
bool explosion(
    int who, int rad, int y0, int x0, int dd, int ds, int dif, int typ)
{
    /* Add the explosion bitflags */
    u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_JUMP | PROJECT_ITEM
        | PROJECT_KILL | PROJECT_PLAY;

    /* Explode */
    return (
        project_ball(who, rad, y0, x0, y0, x0, dd, ds, dif, typ, flg, false));
}

/*
 * Handle arc spells.
 *
 * Arcs are a pie-shaped segment (with a width determined by "degrees")
 * of a explosion outwards from the source grid.  They are centered
 * along a line extending from the source towards the target.  -LM-
 *
 * Because all arcs start out as being one grid wide, arc spells with a
 * value for degrees of arc less than (roughly) 60 do not dissipate as
 * quickly.  In the extreme case where degrees of arc is 0, the arc is
 * actually a defined length beam, and loses no strength at all over the
 * ranges found in the game.
 *
 * Arcs affect items on the floor.
 */
bool project_arc(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, int degrees)
{
    /* Radius of zero means no fixed limit. */
    if (rad == 0)
        rad = MAX_SIGHT;

    /* If the arc has no spread, it's actually a beam */
    if (degrees <= 0)
    {
        /* Add the beam bitflags */
        flg |= (PROJECT_BEAM | PROJECT_KILL);
    }

    /* If a full circle is asked for, we cast a ball spell. */
    else if (degrees >= 360)
    {
        /* Add the ball bitflags */
        flg |= PROJECT_STOP | PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM
            | PROJECT_KILL;
    }

    /* Otherwise, we fire an arc */
    else
    {
        /* Add the arc bitflags */
        flg |= PROJECT_ARC | PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM
            | PROJECT_KILL;
    }

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= (PROJECT_PLAY);

    /* Cast an arc (or a ball) */
    return (project(
        who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, degrees, false));
}

/*
 * Handle target grids for projections under the control of
 * the character.  - Chris Wilde, Morgul
 */
static void adjust_target(int dir, int y0, int x0, int* y1, int* x1)
{
    /* If no direction is given, and a target is, use the target. */
    if ((dir == 5) && target_okay(0))
    {
        *y1 = p_ptr->target_row;
        *x1 = p_ptr->target_col;
    }
    else if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
    {
        *y1 = y0;
        *x1 = x0;
    }

    /* Otherwise, use the given direction */
    else
    {
        *y1 = y0 + MAX_RANGE * ddy[dir];
        *x1 = x0 + MAX_RANGE * ddx[dir];
    }
}

/*
 * Apply a "project()" directly to all monsters in view of a certain spot.
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 *
 * This function is not optimized for efficieny.  It should only be used
 * in non-bottleneck functions such as spells. It should not be used in
 * functions that are major code bottlenecks such as process monster or
 * update_view. -JG
 */
bool project_los_not_player(int y1, int x1, int dd, int ds, int dif, int typ)
{
    int i, x, y;

    u32b flg = PROJECT_JUMP | PROJECT_KILL | PROJECT_HIDE;

    bool obvious = false;

    /* Affect all (nearby) monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Location */
        y = m_ptr->fy;
        x = m_ptr->fx;

        /*The LOS function doesn't do well with long distances*/
        if (distance(y1, x1, y, x) > MAX_RANGE)
            continue;

        /* Require line of sight or the monster being right on the square */
        if ((y != y1) || (x != x1))
        {
            if (!los(y1, x1, y, x))
                continue;
        }

        /* Jump directly to the target monster */
        if (project(-1, 0, y, x, y, x, dd, ds, dif, typ, flg, 0, false))
            obvious = true;
    }

    /* Result */
    return (obvious);
}

/*
 * Apply a "project()" directly to all viewable monsters
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 */
bool project_los(int typ, int dd, int ds, int dif, bool silent)
{
    int i, x, y;

    u32b flg = PROJECT_JUMP | PROJECT_KILL | PROJECT_HIDE;
    if (silent)
    {
        flg |= PROJECT_SILENT;
    }

    bool obvious = false;

    /* Affect all (nearby) monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Location */
        y = m_ptr->fy;
        x = m_ptr->fx;

        /* Require line of fire */
        if (!player_can_fire_bold(y, x))
            continue;
        if (!player_has_los_bold(y, x))
            continue;

        /* Jump directly to the target monster */
        if (project(-1, 0, y, x, y, x, dd, ds, dif, typ, flg, 0, false))
            obvious = true;
    }

    /* Result */
    return (obvious);
}

/*
 * Apply a "project()" directly to all viewable grids
 */
bool project_los_grids(int typ, int dd, int ds, int dif)
{
    int x, y;
    u32b flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_HIDE | PROJECT_JUMP;

    bool obvious = false;

    for (y = p_ptr->py - MAX_SIGHT; y < p_ptr->py + MAX_SIGHT; y++)
    {
        for (x = p_ptr->px - MAX_SIGHT; x < p_ptr->px + MAX_SIGHT; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!player_has_los_bold(y, x))
                continue;

            if (project(-1, 0, y, x, y, x, dd, ds, dif, typ, flg, 0, false))
            {
                obvious = true;
            }
        }
    }
    /* Result */
    return (obvious);
}

/*
 * This routine clears the entire "temp" set.
 */
void clear_temp_array(void)
{
    int i;

    /* Apply flag changes */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* No longer in the array */
        cave_info[y][x] &= ~(CAVE_TEMP);
    }

    /* None left */
    temp_n = 0;
}

/*
 * Aux function -- see below
 */
void cave_temp_mark(int y, int x, bool room)
{
    /* Avoid infinite recursion */
    if (cave_info[y][x] & (CAVE_TEMP))
        return;

    /* Option -- do not leave the current room */
    if ((room) && (!(cave_info[y][x] & (CAVE_ROOM))))
        return;

    /* Verify space */
    if (temp_n == TEMP_MAX)
        return;

    /* Mark the grid */
    cave_info[y][x] |= (CAVE_TEMP);

    /* Add it to the marked set */
    temp_y[temp_n] = y;
    temp_x[temp_n] = x;
    temp_n++;
}

/*
 * Mark the nearby area with CAVE_TEMP flags.  Allow limited range.
 */
void spread_cave_temp(int y1, int x1, int range, bool room)
{
    int i, y, x;

    /* Add the initial grid */
    cave_temp_mark(y1, x1, room);

    /* While grids are in the queue, add their neighbors */
    for (i = 0; i < temp_n; i++)
    {
        x = temp_x[i], y = temp_y[i];

        /* Walls get marked, but stop further spread */
        if (!cave_floor_bold(y, x))
            continue;

        /* Note limited range (note:  we spread out one grid further) */
        if ((range) && (distance(y1, x1, y, x) >= range))
            continue;

        /* Spread adjacent */
        cave_temp_mark(y + 1, x, room);
        cave_temp_mark(y - 1, x, room);
        cave_temp_mark(y, x + 1, room);
        cave_temp_mark(y, x - 1, room);

        /* Spread diagonal */
        cave_temp_mark(y + 1, x + 1, room);
        cave_temp_mark(y - 1, x - 1, room);
        cave_temp_mark(y - 1, x + 1, room);
        cave_temp_mark(y + 1, x - 1, room);
    }
}

/*
 * Slow monsters
 */
bool slow_monsters(int power) { return (project_los(GF_SLOW, 0, 0, power, false)); }

/*
 * Sleep monsters
 */
bool sleep_monsters(int power) { return (project_los(GF_SLEEP, 0, 0, power, false)); }

/*
 * Destroy traps
 */
bool destroy_traps(int power)
{
    return (project_los_grids(GF_KILL_TRAP, 0, 0, power));
}

/*
 * Open doors
 */
bool open_doors(int power)
{
    return (project_los_grids(GF_KILL_DOOR, 0, 0, power));
}

/*
 * Close and lock doors
 */
bool lock_doors(int power)
{
    return (project_los_grids(GF_LOCK_DOOR, 0, 0, power));
}

/*
 * Wake up all monsters, and speed up "los" monsters.
 */
void wake_all_monsters(int who)
{
    int i;

    /* Aggravate everyone */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Skip aggravating monster (or player) */
        if (i == who)
            continue;

        // Alert it
        set_alertness(m_ptr, MAX(m_ptr->alertness, ALERTNESS_VERY_ALERT));

        /*possibly update the monster health bar*/
        if (p_ptr->health_who == i)
            p_ptr->redraw |= (PR_HEALTHBAR);
    }
}

/*
 * Set the aggressive flag on nearby monsters (using the sound metric).
 */
bool make_aggressive(void)
{
    int i;
    int notice = false;

    for (i = 1; i < mon_max; i++)
    {
        /* Check the i'th monster */
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        if ((m_ptr->alertness >= ALERTNESS_ALERT)
            && (flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx) <= 10))
        {
            m_ptr->mflag |= (MFLAG_AGGRESSIVE);

            // notice if the monster is visible
            if (m_ptr->ml)
                notice = true;

            if ((r_ptr->flags2 & (RF2_SMART))
                && ((r_ptr->flags1 & (RF1_FRIENDS))
                    || (r_ptr->flags1 & (RF1_FRIEND))
                    || (r_ptr->flags1 & (RF1_UNIQUE_FRIEND))
                    || (r_ptr->flags1 & (RF1_ESCORT))
                    || (r_ptr->flags1 & (RF1_ESCORTS))
                    || (r_ptr->flags4 & (RF4_SHRIEK))))
            {
                tell_allies(m_ptr->fy, m_ptr->fx, MFLAG_AGGRESSIVE);

                // notice if you hear them shout
                notice = true;
            }
        }
    }

    return (notice);
}

/*
 * Delete all non-unique monsters of a given "type" from the level
 */
bool banishment(void)
{
    int i;

    char typ;

    /* Mega-Hack -- Get a monster symbol */
    if (!get_com("Choose a monster race (by symbol) to banish: ", &typ))
        return false;

    /* Delete the monsters of that "type" */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Hack -- Skip Unique Monsters */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            continue;

        /* Skip "wrong" monsters */
        if (r_ptr->d_char != typ)
            continue;

        /* Delete the monster */
        delete_monster_idx(i);

        /* Take some damage */
        killer_mark_other(SCORE_KILLER_SELF);
        take_hit(dieroll(4), "the strain of casting Banishment");
    }

    /* Success */
    return true;
}

/*
 * Delete all nearby (non-unique) monsters
 */
bool mass_banishment(void)
{
    int i;

    bool result = false;

    /* Delete the (nearby) monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Hack -- Skip unique monsters */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            continue;

        /* Skip distant monsters */
        if (m_ptr->cdis > MAX_SIGHT)
            continue;

        /* Delete the monster */
        delete_monster_idx(i);

        /* Take some damage */
        killer_mark_other(SCORE_KILLER_SELF);
        take_hit(dieroll(3), "the strain of casting Mass Banishment");

        /* Note effect */
        result = true;
    }

    return (result);
}

/*
 * The spell of destruction
 *
 * This spell "deletes" monsters (instead of "killing" them).
 *
 * Later we may use one function for both "destruction" and
 * "earthquake" by using the "full" to select "destruction".
 */
void destroy_area(int y1, int x1, int r, bool full)
{
    int y, x, k, t;

    bool flag = false;

    /* Unused parameter */
    (void)full;

    /* No effect on the surface */
    if (!p_ptr->depth)
    {
        msg_print("The ground shakes for a moment.");
        return;
    }

    /* Big area of affect */
    for (y = (y1 - r); y <= (y1 + r); y++)
    {
        for (x = (x1 - r); x <= (x1 + r); x++)
        {
            /* Skip illegal grids */
            if (!in_bounds_fully(y, x))
                continue;

            /* Extract the distance */
            k = distance(y1, x1, y, x);

            /* Stay in the circle of death */
            if (k > r)
                continue;

            /* Lose room and vault */
            cave_info[y][x] &= ~(CAVE_ROOM | CAVE_ICKY);

            /* Lose light and knowledge */
            cave_info[y][x] &= ~(CAVE_GLOW | CAVE_MARK);

            /* Hack -- Notice player affect */
            if (cave_m_idx[y][x] < 0)
            {
                /* Hurt the player later */
                flag = true;

                /* Do not hurt this grid */
                continue;
            }

            /* Hack -- Skip the epicenter */
            if ((y == y1) && (x == x1))
                continue;

            /* Delete the monster (if any) */
            delete_monster(y, x);

            /* Destroy "valid" grids */
            if (cave_valid_bold(y, x))
            {
                int feat = FEAT_FLOOR;

                /* Delete objects */
                delete_object(y, x);

                /* Wall (or floor) type */
                t = rand_int(200);

                /* Granite */
                if (t < 60)
                {
                    /* Create granite wall */
                    feat = FEAT_WALL_EXTRA;
                }

                /* Quartz */
                else if (t < 100)
                {
                    /* Create quartz vein */
                    feat = FEAT_QUARTZ;
                }

                /* Change the feature */
                cave_set_feat(y, x, feat);
            }
        }
    }

    /* Hack -- Affect player */
    if (flag)
    {
        /* Message */
        msg_print("There is a searing blast of light!");

        /* Blind the player */
        if (allow_player_blind(NULL))
        {
            /* Become blind */
            (void)set_blind(p_ptr->blind + damroll(4, 4));
        }
    }

    /* Make a lot of noise */
    monster_perception(true, false, -30);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);
}

/*
 * Creates an earthquake effect centered around (cy,cx) with radius r.
 *
 * Does rd8 damage at the centre, and one less die each square out
 * from there. If a square doesn't have a monster in it after the damage
 * it might be transformed to a different terrain (eg floor to rubble,
 * rubble to wall, wall to rubble), with a damage% chance. Note that
 * no damage is done to the square at the epicentre.
 *
 * If 'pit_y' and 'pit_x' are not zero, then a pit will be created at the
 * specified location. If the player is in this location, they get a chance to
 * move to another square. If there are no squares to jump to, they fall into
 * the pit and take some more damage.
 *
 * Sil-y: Theoretically the non-pit stuff could be moved to the project
 * functions assuming it can pass through walls properly and that it can deal
 * with creating terrain in a square with a monster iff it first kills the
 * monster and it can decay appropriately with distance (this last might be
 * hardest).
 */
void earthquake(int cy, int cx, int pit_y, int pit_x, int r, int who)
{
    int i, t;
    int y, x, dy, dx, yy, xx;

    int dd, ds, damage, net_dam, prt;

    int dist;

    int sn = 0, sy = 0, sx = 0;
    int feat;

    monster_type* creator_m_ptr;

    bool creator_vis = false;
    bool fall_into_pit = false;
    bool already_in_pit = false;

    /* No effect on the surface */
    if (!p_ptr->depth)
    {
        msg_print("The ground shakes for a moment.");
        return;
    }

    // Set the earthquake creator
    if (who < 0)
    {
        creator_m_ptr = PLAYER;
        creator_vis = true;
    }
    else
    {
        creator_m_ptr = &mon_list[who];
        creator_vis = creator_m_ptr->ml;
    }

    /* Paranoia -- Enforce maximum range */
    if (r > 6)
        r = 6;

    // Step 1:
    // deal with pit creation (if a valid location was passed to this function)
    if (in_bounds_fully(pit_y, pit_x))
    {
        // can't dodge out of a pit
        if (cave_pit_bold(p_ptr->py, p_ptr->px))
        {
            already_in_pit = true;
        }

        // deal with the possibility that the player is there
        if ((p_ptr->py == pit_y) && (p_ptr->px == pit_x))
        {
            if (!already_in_pit)
            {
                /* Check around the player for safe locations to dodge to */
                for (i = 0; i < 8; i++)
                {
                    /* Get the location */
                    y = p_ptr->py + ddy_ddd[i];
                    x = p_ptr->px + ddx_ddd[i];

                    /* Skip non-empty grids */
                    if (!cave_empty_bold(y, x))
                        continue;

                    /* Count "safe" grids, apply the randomizer */
                    if ((++sn > 1) && (rand_int(sn) != 0))
                        continue;

                    /* Save the safe location */
                    sy = y;
                    sx = x;
                }
            }

            if (sn > 0)
            {
                monster_swap(p_ptr->py, p_ptr->px, sy, sx);
            }

            else
            {
                // remember to make the player fall into the pit later

                fall_into_pit = true;
            }
        }

        if (cave_valid_bold(pit_y, pit_x))
        {
            /* Delete objects */
            delete_object(pit_y, pit_x);

            /* Change the feature */
            cave_set_feat(pit_y, pit_x, FEAT_TRAP_PIT);
        }
    }

    // Step 2:
    // Earthquake damage

    // flash the area (using project)
    project_ball(-1, r, p_ptr->py, p_ptr->px, p_ptr->py, p_ptr->px, 0, 0, -1,
        GF_EARTHQUAKE, PROJECT_PASS, false);

    for (dy = -r; dy <= r; dy++)
    {
        for (dx = -r; dx <= r; dx++)
        {
            /* Extract the location */
            y = cy + dy;
            x = cx + dx;

            /* Skip illegal grids */
            if (!in_bounds_fully(y, x))
                continue;

            dist = distance(cy, cx, y, x);

            /* Skip distant grids */
            if (dist > r)
                continue;

            // Sil-y: previously lost knowledge of the squares
            // cave_info[y][x] &= ~(CAVE_MARK);

            /* Skip the epicentre */
            if ((y == cy) && (x == cx))
                continue;

            // Roll the damage for this square
            dd = r + 1 - dist;
            ds = 8;
            damage = damroll(dd, ds);

            // If the player is on the square...
            if (cave_m_idx[y][x] < 0)
            {
                // appropriate message
                msg_print("You are pummeled with debris!");

                // apply protection
                prt = protection_roll(GF_HURT, false);
                net_dam = damage - prt;

                // take the damage
                if (net_dam > 0) {
                    killer_mark_other(SCORE_KILLER_OTHER);
                    take_hit(net_dam, "an earthquake");
                }

                // do stunning
                if (allow_player_stun(NULL))
                {
                    set_stun(p_ptr->stun + net_dam * 4);
                }

                // update the combat rolls to display this later
                update_combat_rolls1b(creator_m_ptr, PLAYER, creator_vis);
                update_combat_rolls2(
                    dd, ds, damage, -1, -1, prt, 100, GF_HURT, false);
            }

            // If a monster is on the square...
            else if (cave_m_idx[y][x] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
                monster_race* r_ptr = &r_info[m_ptr->r_idx];

                char m_name[80];

                /* Describe the monster */
                monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                /* Apply armor dice/sides curses/blessings */
                int armor_dice_base = r_ptr->pd - m_ptr->song_armor_dice_penalty;
                if (armor_dice_base < 0)
                    armor_dice_base = 0;
                int armor_dice = armor_dice_base + curse_flag_delta_cur(CUR_MON_ARM_DICE);
                int armor_sides = monster_base_armour_sides(m_ptr) + curse_flag_delta_cur(CUR_MON_ARM_SIDE);
                if (armor_dice < 0) armor_dice = 0;
                if (armor_sides < 1) armor_sides = 1;
                prt = damroll(armor_dice, armor_sides);
                net_dam = damage - prt;

                // apply damage after protection
                if (net_dam > 0)
                {
                    bool killed = false;

                    // message for each visible monster
                    if (m_ptr->ml)
                        msg_format("%^s is hit by falling debris.", m_name);

                    // if visible and caused by the player, update the combat
                    // rolls to display this later Sil-y: does seem to work so
                    // turned off temporarily
                    if (m_ptr->ml)
                    {
                        update_combat_rolls1b(
                            creator_m_ptr, m_ptr, creator_vis);
                        update_combat_rolls2(dd, ds, damage, armor_dice,
                            armor_sides, prt, 100, GF_HURT, false);
                    }

                    // do the damage and check for death
                    killed = mon_take_hit(cave_m_idx[y][x], net_dam, NULL, who);

                    // special effects for survivors
                    if (!killed)
                    {
                        /*some creatures are resistant to stunning*/
                        if (r_ptr->flags3 & RF3_NO_STUN)
                        {
                            monster_lore* l_ptr = &l_list[m_ptr->r_idx];

                            /*mark the lore*/
                            if (m_ptr->ml)
                                l_ptr->flags3 |= (RF3_NO_STUN);
                        }

                        else
                        {
                            stun_monster(m_ptr, net_dam * 4);
                        }

                        // Alert it
                        set_alertness(m_ptr,
                            MAX(m_ptr->alertness + 10, ALERTNESS_VERY_ALERT));

                        // message for non-visible monsters
                        if (!m_ptr->ml)
                            message_pain(cave_m_idx[y][x], damage);
                    }
                }
            }

            // squares without monsters/player will sometimes get transformed
            // (note that the monster may have been there but got killed by now)
            if ((cave_m_idx[y][x] == 0) && percent_chance(damage)
                && !((y == pit_y) && (x == pit_x)))
            {
                /* Destroy location (if valid) */
                if (cave_valid_bold(y, x))
                {
                    int adj_chasms = 0;

                    /* Delete objects */
                    delete_object(y, x);

                    // count adjacent chasm squares
                    for (i = 0; i < 8; i++)
                    {
                        /* Get the location */
                        yy = y + ddy_ddd[i];
                        xx = x + ddx_ddd[i];

                        // count the chasms
                        if (cave_feat[yy][xx] == FEAT_CHASM)
                            adj_chasms++;
                    }

                    /* Wall (or floor) type */
                    t = rand_int(100);

                    // if we started with a chasm
                    if (cave_feat[y][x] == FEAT_CHASM)
                    {
                        // mostly leave it unchanged
                        if (one_in_(10))
                        {
                            if (t < 10)
                                feat = FEAT_RUBBLE;
                            else if (t < 70)
                                feat = FEAT_WALL_EXTRA;
                            else
                                feat = FEAT_QUARTZ;
                        }
                        else
                        {
                            feat = FEAT_CHASM;
                        }
                    }

                    // if we started with open floor
                    else if (cave_floor_bold(y, x))
                    {
                        if (dieroll(8) <= adj_chasms + 1)
                            feat = FEAT_CHASM;

                        else if (t < 40)
                            feat = FEAT_RUBBLE;
                        else if (t < 80)
                            feat = FEAT_WALL_EXTRA;
                        else
                            feat = FEAT_QUARTZ;
                    }

                    // if we started with rubble
                    else if (cave_feat[y][x] == FEAT_RUBBLE)
                    {
                        if (dieroll(32) <= adj_chasms)
                            feat = FEAT_CHASM;

                        else if (t < 40)
                            feat = FEAT_FLOOR;
                        else if (t < 70)
                            feat = FEAT_WALL_EXTRA;
                        else
                            feat = FEAT_QUARTZ;
                    }

                    // if we started with a wall of some sort
                    else
                    {
                        if (dieroll(32) <= adj_chasms)
                            feat = FEAT_CHASM;

                        if (t < 80)
                            feat = FEAT_RUBBLE;
                        else
                            feat = FEAT_FLOOR;
                    }

                    // change the feature (unless it would be making a chasm at
                    // 1000 ft)
                    if (!((feat == FEAT_CHASM)
                            && (p_ptr->depth >= MORGOTH_DEPTH)))
                    {
                        cave_info[y][x] &= ~(CAVE_MARK);

                        cave_set_feat(y, x, feat);
                    }
                }
            }
        }
    }

    // Step 3:
    // Miscellaneous stuff

    // Fall into the pit if there were no safe squares to jump to
    if (fall_into_pit && cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        // Store information for the combat rolls window
        combat_roll_special_char = (&f_info[FEAT_TRAP_PIT])->d_char;
        combat_roll_special_attr = (&f_info[FEAT_TRAP_PIT])->d_attr;

        msg_print("You fall back into the newly made pit!");

        /* Falling damage */
        damage = damroll(2, 4);

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(2, 4, damage, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(damage, "falling into a pit");
    }

    /* Make a lot of noise */
    monster_perception(true, false, -30);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Update the health bar */
    p_ptr->redraw |= (PR_HEALTHBAR);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);
}

/*
 * Attempt to close a single square of chasm.
 * Used by the function below (for Staff of Freedom) and by the Song of Freedom.
 */
bool close_chasm(int y, int x, int power)
{
    int adj_chasms = 0;
    int yy, xx;
    bool effect = false;

    for (yy = y - 1; yy <= y + 1; yy++)
    {
        for (xx = x - 1; xx <= x + 1; xx++)
        {
            if (!((yy == y) && (xx == x)) && in_bounds(yy, xx)
                && (cave_feat[yy][xx] == FEAT_CHASM))
                adj_chasms++;
        }
    }

    // cannot close chasms that are completely surrounded
    if (adj_chasms < 8)
    {
        if (skill_check(PLAYER, power, 20 + adj_chasms, NULL) > 0)
        {
            cave_info[y][x] |= (CAVE_TEMP);
            effect = true;
        }
    }

    return (effect);
}

/*
 * Attempt to close chasms.
 * Can't be done with project as it would depend on the order the grids are
 * processed.
 */
bool close_chasms(int power)
{
    int y, x;
    bool effect = false;

    // first find all chasms and mark those that are being closed
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if ((cave_feat[y][x] == FEAT_CHASM)
                && (cave_info[y][x] & (CAVE_VIEW)))
            {
                effect |= close_chasm(y, x, power);
            }
        }
    }

    // then, if any were marked, do the closing
    if (effect)
    {
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_feat[y][x] == FEAT_CHASM)
                    && (cave_info[y][x] & (CAVE_TEMP)))
                {
                    // remove the temporary marking
                    cave_info[y][x] &= ~(CAVE_TEMP);

                    // close the chasm
                    cave_set_feat(y, x, FEAT_FLOOR);

                    // update the visuals
                    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
                }
            }
        }
    }

    return (effect);
}

/*
 * This routine clears the entire "temp" set.
 *
 * This routine will Perma-Lite all "temp" grids.
 *
 * This routine is used (only) by "light_room()"
 *
 * Dark grids are illuminated.
 *
 * Also, process all affected monsters.
 *
 * SMART monsters always wake up when illuminated
 * NORMAL monsters wake up 1/4 the time when illuminated
 * MINDLESS monsters wake up 1/10 the time when illuminated
 */
static void cave_temp_room_light(void)
{
    int i;

    /* Apply flag changes */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* No longer in the array */
        cave_info[y][x] &= ~(CAVE_TEMP);

        /* Perma-Lite */
        cave_info[y][x] |= (CAVE_GLOW);
    }

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Update stuff */
    update_stuff();

    /* Process the grids */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* Redraw the grid */
        lite_spot(y, x);

        /* Process affected monsters */
        if (cave_m_idx[y][x] > 0)
        {
            int alerting_power = damroll(2, 10);

            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            /* Mindless monsters rarely wake up */
            if (r_ptr->flags2 & (RF2_MINDLESS))
                alerting_power /= 2;

            /* Smart monsters mostly wake up */
            if (r_ptr->flags2 & (RF2_SMART))
                alerting_power *= 2;

            /* Alert unwary/sleeping monsters to a degree */
            if (m_ptr->alertness < ALERTNESS_UNWARY)
            {
                set_alertness(m_ptr,
                    MIN(m_ptr->alertness + alerting_power, ALERTNESS_ALERT));

                /*possibly update the monster health bar*/
                if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
                    p_ptr->redraw |= (PR_HEALTHBAR);
            }
        }
    }

    /* None left */
    temp_n = 0;
}

/*
 * This routine clears the entire "temp" set.
 *
 * This routine will "darken" all "temp" grids.
 *
 * In addition, some of these grids will be "unmarked".
 *
 * This routine is used (only) by "darken_room()"
 */
static void cave_temp_room_darken(void)
{
    int i;

    /* Apply flag changes */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* No longer in the array */
        cave_info[y][x] &= ~(CAVE_TEMP);

        /* Darken the grid */
        cave_info[y][x] &= ~(CAVE_GLOW);

        /* Hack -- Forget "boring" grids */
        if (cave_floorlike_bold(y, x))
        {
            /* Forget the grid */
            cave_info[y][x] &= ~(CAVE_MARK);
        }
    }

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Update stuff */
    update_stuff();

    /* Process the grids */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* Redraw the grid */
        lite_spot(y, x);
    }

    /* None left */
    temp_n = 0;
}

/*
 * Aux function -- see below
 */
static void cave_temp_room_aux(int y, int x)
{
    /* Avoid infinite recursion */
    if (cave_info[y][x] & (CAVE_TEMP))
        return;

    /* Do not "leave" the current room */
    if (!(cave_info[y][x] & (CAVE_ROOM)))
        return;

    /* Paranoia -- verify space */
    if (temp_n == TEMP_MAX)
        return;

    /* Mark the grid as "seen" */
    cave_info[y][x] |= (CAVE_TEMP);

    /* Add it to the "seen" set */
    temp_y[temp_n] = y;
    temp_x[temp_n] = x;
    temp_n++;
}

/*
 * Illuminate any room containing the given location.
 */
void light_room(int y1, int x1)
{
    int i, x, y;

    /* Add the initial grid */
    cave_temp_room_aux(y1, x1);

    /* While grids are in the queue, add their neighbors */
    for (i = 0; i < temp_n; i++)
    {
        x = temp_x[i], y = temp_y[i];

        /* Walls get lit, but stop light */
        if (!cave_floor_bold(y, x))
            continue;

        /* Spread adjacent */
        cave_temp_room_aux(y + 1, x);
        cave_temp_room_aux(y - 1, x);
        cave_temp_room_aux(y, x + 1);
        cave_temp_room_aux(y, x - 1);

        /* Spread diagonal */
        cave_temp_room_aux(y + 1, x + 1);
        cave_temp_room_aux(y - 1, x - 1);
        cave_temp_room_aux(y - 1, x + 1);
        cave_temp_room_aux(y + 1, x - 1);
    }

    /* Now, lite them all up at once */
    cave_temp_room_light();
}

/*
 * Darken all rooms containing the given location
 */
void darken_room(int y1, int x1)
{
    int i, x, y;

    /* Add the initial grid */
    cave_temp_room_aux(y1, x1);

    /* Spread, breadth first */
    for (i = 0; i < temp_n; i++)
    {
        x = temp_x[i], y = temp_y[i];

        /* Walls get dark, but stop darkness */
        if (!cave_floor_bold(y, x))
            continue;

        /* Spread adjacent */
        cave_temp_room_aux(y + 1, x);
        cave_temp_room_aux(y - 1, x);
        cave_temp_room_aux(y, x + 1);
        cave_temp_room_aux(y, x - 1);

        /* Spread diagonal */
        cave_temp_room_aux(y + 1, x + 1);
        cave_temp_room_aux(y - 1, x - 1);
        cave_temp_room_aux(y - 1, x + 1);
        cave_temp_room_aux(y + 1, x - 1);
    }

    /* Now, darken them all at once */
    cave_temp_room_darken();
}

/*
 * Hack -- call light around the player
 * Affect all monsters in the projection radius
 */
bool light_area(int dd, int ds, int rad)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_KILL | PROJECT_PASS;

    /* Hack -- Message */
    if (!p_ptr->blind)
    {
        msg_print("You are surrounded by a white light.");
    }

    /* Hook into the "project()" function */
    (void)project(-1, rad, py, px, py, px, dd, ds, -1, GF_LIGHT, flg, 0, false);

    /* Assume seen */
    return (true);
}

/*
 * Hack -- call darkness around the player
 * Affect all monsters in the projection radius
 */
bool darken_area(int dd, int ds, int rad)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_KILL;

    /* Hack -- Message */
    if (!p_ptr->blind)
    {
        msg_print("Darkness surrounds you.");
    }

    /* Hook into the "project()" function */
    (void)project(
        -1, rad, py, px, py, px, dd, ds, -1, GF_DARK_WEAK, flg, 0, false);

    /* Darken the room */
    darken_room(py, px);

    /* Assume seen */
    return (true);
}

/*
 * Character casts a special-purpose bolt or beam spell.
 */
bool fire_bolt_beam_special(
    int typ, int dir, int dd, int ds, int dif, int rad, u32b flg)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* This is a beam spell */
    if (flg & (PROJECT_BEAM))
    {
        /* Cast a beam */
        return (project_beam(
            -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, flg));
    }

    /* This is a bolt spell */
    else
    {
        /* Cast a bolt */
        return (project_bolt(
            -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, flg));
    }
}

/*
 * Character casts a (simple) ball spell.
 */
bool fire_ball(int typ, int dir, int dd, int ds, int dif, int rad)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast a (simple) ball */
    return (project_ball(
        -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L, false));
}

/*
 * Character casts an arc spell.
 */
bool fire_arc(int typ, int dir, int dd, int ds, int dif, int rad, int degrees)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast an arc */
    return (project_arc(
        -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L, degrees));
}

/*
 * Character casts a bolt spell.
 */
bool fire_bolt(int typ, int dir, int dd, int ds, int dif)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast a bolt */
    return (project_bolt(
        -1, MAX_RANGE, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L));
}

/*
 * Character casts a beam spell.
 */
bool fire_beam(int typ, int dir, int dd, int ds, int dif)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast a beam */
    return (project_beam(
        -1, MAX_RANGE, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L));
}

/*
 * Cast a bolt or a beam spell
 */
bool fire_bolt_or_beam(int prob, int typ, int dir, int dd, int ds, int dif)
{
    if (percent_chance(prob))
    {
        return (fire_beam(typ, dir, dd, ds, dif));
    }
    else
    {
        return (fire_bolt(typ, dir, dd, ds, dif));
    }
}

/*
 * Some of the old functions
 */

bool light_line(int dir)
{
    u32b flg = PROJECT_BEAM | PROJECT_GRID;
    return (fire_bolt_beam_special(GF_LIGHT, dir, 6, 4, -1, MAX_RANGE, flg));
}

bool destroy_door(int dir)
{
    u32b flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM;
    return (
        fire_bolt_beam_special(GF_KILL_DOOR, dir, 0, 0, -1, MAX_RANGE, flg));
}

bool disarm_trap(int dir)
{
    u32b flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM;
    return (
        fire_bolt_beam_special(GF_KILL_TRAP, dir, 0, 0, -1, MAX_RANGE, flg));
}

/*
 * Hook to specify "arrows"
 */
bool item_tester_hook_ided_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return false;
    }
    }

    return (false);
}

/*
 * Hook to specify "arrows"
 */
bool item_tester_hook_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify ordinary arrows
 */
bool item_tester_hook_ordinary_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        if (o_ptr->name1 || object_has_ego(o_ptr) || o_ptr->att > 0)
            return false;
        return true;
    }
    }

    return false;
}

/*
 * Identifies all objects in the equipment and inventory.
 * Applies quality/special item squelch in the inventory.
 */
void identify_and_squelch_pack(void)
{
    int item, squelch;
    object_type* o_ptr;

    /* Identify equipment */
    for (item = INVEN_WIELD; item < INVEN_TOTAL; item++)
    {
        /* Get the object */
        o_ptr = &inventory[item];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        (void)do_ident_item(item, o_ptr);
    }

    /* Identify inventory */
    for (item = 0; item < INVEN_WIELD; item++)
    {
        while (true)
        {
            /* Get the object */
            o_ptr = &inventory[item];

            /* Ignore empty objects */
            if (!o_ptr->k_idx)
                break;

            /* Ignore known objects */
            if (object_known_p(o_ptr))
                break;

            /* Identify it and get the squelch setting */
            squelch = do_ident_item(item, o_ptr);

            /*
             * If the object was squelched, keep analyzing
             * the same slot (the inventory was displaced). -DG-
             */
            if (squelch != SQUELCH_YES)
                break;

            /* Now squelch the object */
            do_squelch_item(squelch, item, o_ptr);
        }
    }

    /* Identify supplies */
    int supply_count = supplies_entry_count();
    for (int supply_idx = 0; supply_idx < supply_count; supply_idx++)
    {
        o_ptr = supplies_entry_at(supply_idx);
        if (!o_ptr || !o_ptr->k_idx)
            continue;
        if (object_known_p(o_ptr))
            continue;

        (void)do_ident_item(SUPPLIES_INDEX + supply_idx, o_ptr);
    }
}

/* Mass-identify handler */
bool mass_identify(int rad)
{
    /* Direct the ball to the player */
    target_set_location(p_ptr->py, p_ptr->px);

    /* Cast the ball spell */
    fire_ball(GF_IDENTIFY, 5, 0, 0, -1, rad);

    /* Identify equipment and inventory, apply quality squelch */
    identify_and_squelch_pack();

    /* This spell always works */
    return (true);
}

/*
 * Execute some common code of the identify spells.
 * "item" is used to print the slot occupied by an object in equip/inven.
 * ANY negative value assigned to "item" can be used for specifying an object
 * on the floor (they don't have a slot, example: the code used to handle
 * GF_IDENTIFY in project_o).
 * It returns the value returned by squelch_itemp.
 * The object is NOT squelched here.
 */
int do_ident_item(int item, object_type* o_ptr)
{
    char o_name[80];
    int squelch = SQUELCH_NO;

    /* Identify it */
    object_aware(o_ptr);
    object_known(o_ptr);

    /* Apply an autoinscription, if necessary */
    apply_autoinscription(o_ptr);

    /* Squelch it? */
    if (item < INVEN_WIELD)
        squelch = squelch_itemp(o_ptr, 0, true);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Description */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Describe */
    if (item >= SUPPLIES_INDEX)
    {
        int supply_index = item - SUPPLIES_INDEX;
        msg_format("In your supplies: %s.  %s", o_name, squelch_to_label(squelch));
        supplies_refresh_entry(supply_index);
    }
    else if (item >= INVEN_WIELD)
    {
        msg_format(
            "%^s: %s (%c).", describe_use(item), o_name, index_to_label(item));
    }
    else if (item >= 0)
    {
        msg_format("In your pack: %s (%c).  %s", o_name, index_to_label(item),
            squelch_to_label(squelch));
    }
    else
    {
        msg_format("On the ground: %s.  %s", o_name, squelch_to_label(squelch));
    }

    return (squelch);
}




