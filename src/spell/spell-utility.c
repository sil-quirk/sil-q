/* File: spell/spell-utility.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

void spells2_prompt_label(int binding, const char* fallback, char* buf,
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
static bool self_knowledge_capture_reflow(self_knowledge_capture* capture,
    int width);
static void self_knowledge_capture_view(self_knowledge_capture* capture);
static bool render_resistance_summary(const char* text);

static self_knowledge_capture pending_self_knowledge_capture;
static bool pending_self_knowledge_capture_active = false;
static int self_knowledge_defer_display_depth = 0;

#define SELF_KNOWLEDGE_CAPTURE_MARGIN_COLS 4

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

void self_knowledge_defer_display_push(void)
{
    self_knowledge_defer_display_depth++;
}

void self_knowledge_defer_display_pop(void)
{
    if (self_knowledge_defer_display_depth > 0)
        self_knowledge_defer_display_depth--;
}

bool self_knowledge_display_pending(void)
{
    self_knowledge_capture capture;

    if (!pending_self_knowledge_capture_active)
        return false;

    capture = pending_self_knowledge_capture;
    SDL_memset(&pending_self_knowledge_capture, 0,
        sizeof(pending_self_knowledge_capture));
    pending_self_knowledge_capture_active = false;

    sdl_push_description_overlay_main_anchor();
    character_icky++;
    self_knowledge_capture_view(&capture);
    character_icky--;
    sdl_pop_description_overlay_main_anchor();

    self_knowledge_capture_free(&capture);
    return true;
}

static void self_knowledge_capture_render_body(char s[][200], char t[][200],
    bool good[], int count)
{
    int term_wid = 80;
    int term_hgt = 24;
    int line = 2;

    Term_get_size(&term_wid, &term_hgt);
    (void)term_hgt;
    if (term_wid < 20)
        term_wid = 20;

    Term_putstr(0, 0, -1, TERM_L_WHITE + TERM_SHADE, "Your Attributes:");

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
    /*
     * term::hgt is a byte, so taller scratch terms wrap to zero and render
     * as blank. Keep this at the maximum representable terminal height.
     */
    enum { SELF_KNOWLEDGE_CAPTURE_ROWS = 255 };
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

    term_wid = sdl_description_overlay_max_cols();
    if (term_wid > SELF_KNOWLEDGE_CAPTURE_MARGIN_COLS)
        term_wid -= SELF_KNOWLEDGE_CAPTURE_MARGIN_COLS;
    Term_get_size(NULL, &term_hgt);
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

/*
 * The scratch terminal is sized before the SDL popup becomes active.  Some
 * layouts can reserve additional space once the popup is shown, leaving fewer
 * visible columns than the capture contains.  Rewrap each captured row before
 * it is displayed so the overlay never clips text on the right.
 */
static bool self_knowledge_capture_reflow(self_knowledge_capture* capture,
    int width)
{
    int max_rows;
    int out_row = 0;
    byte* attrs;
    char* chars;
    byte* story;

    if (!capture || !capture->attrs || !capture->chars || !capture->story
        || width < 2 || width >= capture->width)
    {
        return false;
    }

    max_rows = capture->height * (capture->width / (width - 1) + 2);
    attrs = mem_alloc_array(max_rows * width, byte);
    chars = mem_alloc_array(max_rows * width, char);
    story = mem_alloc_array(max_rows * width, byte);

    for (int row = 0; row < max_rows; row++)
    {
        for (int col = 0; col < width; col++)
        {
            int index = row * width + col;

            attrs[index] = TERM_WHITE;
            chars[index] = ' ';
            story[index] = 0;
        }
    }

    for (int row = 0; row < capture->height; row++)
    {
        int source = row * capture->width;
        int last = capture->width - 1;
        int pos = 0;
        bool first = true;

        while (last >= 0 && capture->chars[source + last] == ' '
            && capture->story[source + last] == 0)
        {
            last--;
        }

        if (last < 0)
        {
            out_row++;
            continue;
        }

        while (pos <= last)
        {
            int dest_col = first ? 0 : 1;
            int available = width - dest_col;
            int end = MIN(pos + available, last + 1);
            int next = end;

            if (end <= last)
            {
                int break_at = -1;

                for (int col = end - 1; col >= pos; col--)
                {
                    if (capture->chars[source + col] == ' ')
                    {
                        break_at = col;
                        break;
                    }
                }

                if (break_at >= pos)
                {
                    end = break_at;
                    next = break_at + 1;
                }
            }

            if (end > pos)
            {
                for (int col = pos; col < end; col++)
                {
                    int dest = out_row * width + dest_col + col - pos;

                    attrs[dest] = capture->attrs[source + col];
                    chars[dest] = capture->chars[source + col];
                    story[dest] = capture->story[source + col];
                }
                out_row++;
            }

            pos = next;
            while (pos <= last && capture->chars[source + pos] == ' ')
                pos++;
            first = false;
        }
    }

    mem_free_null(capture->attrs);
    mem_free_null(capture->chars);
    mem_free_null(capture->story);
    capture->attrs = attrs;
    capture->chars = chars;
    capture->story = story;
    capture->width = width;
    capture->height = out_row;

    return true;
}

static void self_knowledge_capture_view(self_knowledge_capture* capture)
{
    int scroll = 0;
    bool done = false;
    bool saved_hide_cursor = hide_cursor;

    if (!capture)
        return;

    hide_cursor = true;

    while (!done)
    {
        int term_hgt = 24;
        int prompt_row;
        int visible_rows = 1;
        int max_scroll = 0;
        int page_rows;
        int dir;
        char ch;
        char footer[96];

        Term_get_size(NULL, &term_hgt);
        if (term_hgt < 3)
            term_hgt = 3;

        prompt_row = term_hgt - 1;

        if (steamdeck_controls_active())
        {
            char back_label[16];
            char prompt_full[96];
            char prompt_short[80];
            const char* variants[2];

            spells2_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad scroll  %s close", back_label);
            strnfmt(prompt_short, sizeof(prompt_short), "%s close",
                back_label);
            variants[0] = prompt_full;
            variants[1] = prompt_short;
            terminal_prompt_pick_variant(footer, sizeof(footer), 80, false,
                variants, N_ELEMENTS(variants));
        }
        else
        {
            const char* variants[] = {
                "Esc close  Dir scroll",
                "Esc close"
            };
            terminal_prompt_pick_variant(footer, sizeof(footer), 80, false,
                variants, N_ELEMENTS(variants));
        }

        sdl_description_overlay_set_footer(footer, true);
        sdl_description_overlay_clear_footer_actions();
        sdl_description_overlay_add_footer_action(ESCAPE, "Esc close");

        if (!sdl_description_overlay_present(capture->attrs, capture->chars,
                NULL, NULL, capture->story, NULL, capture->width,
                capture->height,
                capture->width, scroll, true, &visible_rows, &max_scroll))
        {
            break;
        }

        if (self_knowledge_capture_reflow(capture,
                sdl_description_overlay_visible_cols()))
        {
            scroll = 0;
            continue;
        }

        if (scroll < 0)
            scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;

        page_rows = (visible_rows > 1) ? visible_rows - 1 : 1;

        ui_scroll_area_begin(0, MAX(0, prompt_row),
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', '6', '4');
        ui_scroll_area_set_offset_target(&scroll, max_scroll);
        ui_scroll_area_set_tap_key(ESCAPE);

        ch = inkey();

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

    sdl_description_overlay_clear();
    sdl_description_overlay_clear_footer_actions();
    sdl_description_overlay_set_footer(NULL, false);
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
{ "Song of Lórien is 1.5x effective", UNQ, UNQ_SNG_LUT },
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

    /* Túrin character resistance check first */
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
 * Note that Items bound by the Oath of Fëanor (TR3_PERMA_CURSE)
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

        /* Items bound by the Oath of Fëanor can only be freed by a Silmaril */
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
    u32b f2 = 0L, f3 = 0L;
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
        f2 |= t2; f3 |= t3;

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

    sdl_push_description_overlay_main_anchor();

    if (!self_knowledge_capture_build(s, t, good, count, &capture))
    {
        sdl_pop_description_overlay_main_anchor();
        return;
    }

    if (self_knowledge_defer_display_depth > 0)
    {
        if (pending_self_knowledge_capture_active)
            self_knowledge_capture_free(&pending_self_knowledge_capture);
        pending_self_knowledge_capture = capture;
        pending_self_knowledge_capture_active = true;
        sdl_pop_description_overlay_main_anchor();
        ui_menu_click_clear();
        ui_scroll_area_clear();
        return;
    }

    character_icky++;
    self_knowledge_capture_view(&capture);
    character_icky--;

    sdl_pop_description_overlay_main_anchor();
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

