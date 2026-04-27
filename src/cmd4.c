/* File: cmd4.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

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
#include "sdl-config.h"
#include "pane.h"

/* String used to show a color sample */
#define COLOR_SAMPLE "###"

/*max length of note output*/
#define LINEWRAP 75

/*used for knowledge display*/
#define BROWSER_ROWS 16

/* Option changes that affect list rendering should refresh subwindows immediately. */
static void redraw_inven_equip_subwindows(void);
static void redraw_monster_subwindows(void);
static void smith_ui_reset_description_state(void);
static void controller_prompt_label(int binding, const char* fallback, char* buf, size_t buflen);
static void controller_prompt_label_no_sticks(int binding, const char* fallback, char* buf, size_t buflen);
static void desc_obj_fake(int k_idx);

static bool indexed_menu_letters_enabled(void)
{
    return !steamdeck_controls_active();
}

static void indexed_menu_entry_label(char* buf, size_t buflen, int index, cptr text)
{
    if (!buf || !buflen)
        return;

    if (indexed_menu_letters_enabled())
        strnfmt(buf, buflen, "%c) %s", (char)'a' + index, text ? text : "");
    else
        strnfmt(buf, buflen, "%s", text ? text : "");
}

static void keyed_menu_entry_label(char* buf, size_t buflen, char key, cptr text)
{
    if (!buf || !buflen)
        return;

    if (indexed_menu_letters_enabled())
        strnfmt(buf, buflen, "%c) %s", key, text ? text : "");
    else
        strnfmt(buf, buflen, "%s", text ? text : "");
}

static int indexed_menu_prefix_col(int col)
{
    if (indexed_menu_letters_enabled())
        return col;

    return (col >= 2) ? (col - 2) : col;
}

static void indexed_menu_focus_prefix(char* buf, size_t buflen, int index)
{
    if (!buf || !buflen)
        return;

    if (indexed_menu_letters_enabled())
        strnfmt(buf, buflen, "%c)", (char)'a' + index);
    else
        SDL_strlcpy(buf, "> ", buflen);
}

static void indexed_menu_normal_prefix(char* buf, size_t buflen, int index)
{
    if (!buf || !buflen)
        return;

    if (indexed_menu_letters_enabled())
        strnfmt(buf, buflen, "%c)", (char)'a' + index);
    else
        SDL_strlcpy(buf, "  ", buflen);
}

static bool heavy_armour_desc_evasion_bonus_applies(const object_type* o_ptr)
{
    return (o_ptr->tval == TV_MAIL)
        && ((o_ptr->sval == SV_MAIL_CORSLET)
            || (o_ptr->sval == SV_LONG_CORSLET));
}

static int heavy_armour_desc_current_weight(void)
{
    int i;
    int armour_weight = 0;

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        /* Off-hand weapons are not counted as armour weight. */
        if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD))
            continue;

        if (i >= INVEN_BODY)
            armour_weight += o_ptr->weight;
    }

    return armour_weight;
}

static int heavy_armour_desc_current_evasion_bonus(void)
{
    int i;
    int bonus = 0;

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        if (heavy_armour_desc_evasion_bonus_applies(o_ptr))
            bonus++;
    }

    return bonus;
}

typedef struct knowledge_browser_layout knowledge_browser_layout;
typedef struct knowledge_browser_state knowledge_browser_state;

/*
 *  Header and footer marker string for pref file dumps
 */
static cptr dump_seperator = "#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#";

typedef struct monster_list_entry monster_list_entry;
/*
 * Structure for building monster "lists"
 */
struct monster_list_entry
{
    s16b r_idx; /* Monster race index */

    byte amount;
};

typedef struct object_list_entry object_list_entry;
struct object_list_entry
{
    enum
    {
        OBJ_NONE,
        OBJ_NORMAL,
        OBJ_SPECIAL
    } type;
    int idx;
    int e_idx;
    int tval, sval;
};

typedef struct supply_list_entry supply_list_entry;
typedef struct supply_list_columns supply_list_columns;
typedef struct supply_group_icon supply_group_icon;

struct supply_list_entry
{
    int item_idx;   /* Inventory slot backing this row */
    int k_idx;      /* Object kind index */
    int total;      /* Displayed quantity for this row */
    int supply_idx; /* Index inside the supply cache (-1 if not present) */
    int equip_idx;  /* Matching equipped light slot (-1 if not equipped) */
    bool equipped;  /* This supply-kind is currently equipped */
    bool single_item_display; /* Display one item even if the source stack is larger */
};

struct supply_list_columns
{
    int name_col;
    int name_w;
    int weight_col;
    int turns_col;
    int qty_col;
    int sym_hdr_col;
    int sym_col;
    bool show_weight;
    bool show_turns;
    bool show_qty;
    bool show_sym;
};

struct supply_group_icon
{
    bool has_icon;
    object_type obj;
};

#define SUPPLY_COMPACT_TERM_WIDTH 80

struct knowledge_browser_layout
{
    int term_wid;
    int term_hgt;
    int title_row;
    int tabs_row;
    int header_row;
    int divider_row;
    int list_row;
    int list_rows;
    int status_row;
    int prompt_row;
    int group_col;
    int group_w;
    int divider_col;
    int list_col;
    int list_w;
};

struct knowledge_browser_state
{
    int column[4];
    int group_cur[4];
    int group_top[4];
    int entry_cur[4];
    int entry_top[4];
    bool tabs_focus;
};

static int g_knowledge_last_page = KNOWLEDGE_PAGE_ARTEFACTS;

static void dump_visual_pair(
    SDL_IOStream* fff, const char* tag, int index, byte attr, byte chr)
{
    bool attr_tile = (attr & TILE_FLAG) != 0;
    bool char_tile = (chr & TILE_FLAG) != 0;

    SDL_IOprintf(fff, "%s:%d:", tag, index);
    if (attr_tile)
        SDL_IOprintf(fff, "R%d", TILE_GET_INDEX(attr));
    else
        SDL_IOprintf(fff, "0x%02X", attr);

    SDL_WriteU8(fff, ':');

    if (char_tile)
        SDL_IOprintf(fff, "C%d", TILE_GET_INDEX(chr));
    else
        SDL_IOprintf(fff, "0x%02X", (byte)chr);

    SDL_WriteU8(fff, '\n');
    SDL_WriteU8(fff, '\n');
}


static bool supplies_menu_use_entry(supply_list_entry* entry)
{
    if (!entry)
        return false;

    if (entry->supply_idx < 0)
    {
        if (entry->equipped && entry->equip_idx == INVEN_LITE)
        {
            msg_print("That light source is already equipped.");
        }
        return false;
    }

    object_type* o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    supplies_begin_action(entry->supply_idx);

    switch (o_ptr->tval)
    {
    case TV_FOOD:
        do_cmd_eat_food(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_POTION:
        do_cmd_quaff_potion(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_STAFF:
        do_cmd_activate_staff(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_GEM:
        do_cmd_use_gem(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_FLASK:
        do_cmd_refuel_lamp(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_LIGHT:
        do_cmd_wield(o_ptr, SUPPLIES_INDEX);
        break;
    default:
        supplies_end_action();
        bell("Cannot use that item here!");
        msg_print("Cannot use that item here.");
        return false;
    }

    supplies_end_action();
    return true;
}

static bool supplies_menu_drop_entry(supply_list_entry* entry)
{
    if (!entry)
        return false;

    if (entry->supply_idx < 0)
    {
        if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
        {
            do_cmd_drop_item_by_index(entry->equip_idx);
            return true;
        }
        return false;
    }

    object_type* o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    int max_amt = o_ptr->number;
    if (max_amt <= 0)
        return false;

    int actual_amt = get_quantity(NULL, max_amt);
    if (actual_amt <= 0)
        return false;
    supplies_begin_action(entry->supply_idx);
    bool dropped = supplies_drop_amount(entry->supply_idx, actual_amt);
    supplies_end_action();

    if (dropped)
        handle_stuff();

    return dropped;
}

static bool supplies_menu_recall_entry(supply_list_entry* entry)
{
    if (!entry)
        return false;

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
    {
        (void)player_try_identify_smithing_object_on_examine(
            &inventory[entry->equip_idx], true);
        object_info_screen(&inventory[entry->equip_idx]);
        return true;
    }

    if (entry->supply_idx >= 0)
    {
        object_type* o_ptr = supplies_entry_at(entry->supply_idx);
        if (o_ptr)
        {
            object_info_screen(o_ptr);
            return true;
        }
    }

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
    {
        (void)player_try_identify_smithing_object_on_examine(
            &inventory[entry->item_idx], false);
        object_info_screen(&inventory[entry->item_idx]);
        return true;
    }

    if (entry->k_idx >= 0)
    {
        object_kind* k_ptr = &k_info[entry->k_idx];
        if (k_ptr->aware)
        {
            desc_obj_fake(entry->k_idx);
            return true;
        }

        bell("You have not identified that yet.");
        msg_print("You have not identified that yet.");
        return false;
    }

    bell("Nothing to recall.");
    msg_print("Nothing to recall.");
    return false;
}

static cptr supply_group_text[SUPPLY_GROUP_MAX + 1] = {
    "Herbs",
    "Food",
    "Potions",
    "Gems",
    "Lights/Oil",
    NULL
};

/*
 * Remove old lines from pref files
 */
static void remove_old_dump(cptr orig_file, cptr mark)
{
    SDL_IOStream* tmp_fff, *orig_fff;

    char tmp_file[1024];
    char buf[1024];
    bool between_marks = false;
    bool changed = false;
    char expected_line[1024];

    /* Open an old dump file in read-only mode */
    orig_fff = sdl_fopen(orig_file, "r");

    /* If original file does not exist, nothing to do */
    if (!orig_fff)
        return;

    /* Open a new temporary file */
    tmp_fff = sdl_fopen_temp(tmp_file, sizeof(tmp_file));

    if (!tmp_fff)
    {
        msg_format("Failed to create temporary file %s.", tmp_file);
        msg_print(NULL);
        return;
    }

    strnfmt(expected_line, sizeof(expected_line), "%s begin %s", dump_seperator,
        mark);

    /* Loop for every line */
    while (true)
    {
        /* Read a line */
        if (sdl_fgets(orig_fff, buf, sizeof(buf)))
        {
            /* End of file but no end marker */
            if (between_marks)
                changed = false;

            break;
        }

        /* Is this line a header/footer? */
        if (strncmp(buf, dump_seperator, strlen(dump_seperator)) == 0)
        {
            /* Found the expected line? */
            if (strcmp(buf, expected_line) == 0)
            {
                if (!between_marks)
                {
                    /* Expect the footer next */
                    strnfmt(expected_line, sizeof(expected_line), "%s end %s",
                        dump_seperator, mark);

                    between_marks = true;

                    /* There are some changes */
                    changed = true;
                }
                else
                {
                    /* Expect a header next - XXX shouldn't happen */
                    strnfmt(expected_line, sizeof(expected_line), "%s begin %s",
                        dump_seperator, mark);

                    between_marks = false;

                    /* Next line */
                    continue;
                }
            }
            /* Found a different line */
            else
            {
                /* Expected a footer and got something different? */
                if (between_marks)
                {
                    /* Abort */
                    changed = false;
                    break;
                }
            }
        }

        if (!between_marks)
        {
            /* Copy orginal line */
            SDL_IOprintf(tmp_fff, "%s\n", buf);
        }
    }

    /* Close files */
    sdl_fclose(orig_fff);
    sdl_fclose(tmp_fff);

    /* If there are changes, overwrite the original file with the new one */
    if (changed)
    {
        /* Copy contents of temporary file */
        tmp_fff = sdl_fopen(tmp_file, "r");
        orig_fff = sdl_fopen(orig_file, "w");

        while (!sdl_fgets(tmp_fff, buf, sizeof(buf)))
        {
            SDL_IOprintf(orig_fff, "%s\n", buf);
        }

        sdl_fclose(orig_fff);
        sdl_fclose(tmp_fff);
    }

    /* Kill the temporary file */
    fd_kill(tmp_file);
}

/*
 * Output the header of a pref-file dump
 */
static void pref_header(SDL_IOStream* fff, cptr mark)
{
    /* Start of dump */
    SDL_IOprintf(fff, "%s begin %s\n", dump_seperator, mark);

    SDL_IOprintf(fff, "# *Warning!*  The lines below are an automatic dump.\n");
    SDL_IOprintf(fff,
        "# Don't edit them; changes will be deleted and replaced "
        "automatically.\n");
}

/*
 * Output the footer of a pref-file dump
 */
static void pref_footer(SDL_IOStream* fff, cptr mark)
{
    SDL_IOprintf(fff, "# *Warning!*  The lines above are an automatic dump.\n");
    SDL_IOprintf(fff,
        "# Don't edit them; changes will be deleted and replaced "
        "automatically.\n");

    /* End of dump */
    SDL_IOprintf(fff, "%s end %s\n", dump_seperator, mark);
}

/*
 * Hack -- redraw the screen
 *
 * This command performs various low level updates, clears all the "extra"
 * windows, does a total redraw of the main window, and requests all of the
 * interesting updates and redraws that I can think of.
 *
 * This command is also used to "instantiate" the results of the user
 * selecting various things, such as graphics mode, so it must call
 * the "TERM_XTRA_REACT" hook before redrawing the windows.
 */
void do_cmd_redraw(void)
{
    int j;

    term* old = Term;

    /* Low level flush */
    Term_flush();

    /* Reset "inkey()" */
    flush();

    if (g_banner_force_redraw_remaining <= 0)
        clear_active_narrative_banner();

    /* Hack -- React to changes */
    Term_xtra(TERM_XTRA_REACT, 0);

    /* Combine and Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Update stuff */
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw everything */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Window stuff */
    p_ptr->window
        |= (PW_MESSAGE | PW_OVERHEAD | PW_MONSTER | PW_OBJECT | PW_MONLIST);

    /* Clear screen */
    Term_clear();

    /* Hack -- update */
    handle_stuff();

    /* Redraw every window */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Dead window */
        if (!angband_term[j])
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Redraw */
        Term_redraw();

        /* Refresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- character sheet
 */
static void character_sheet_put_prompt_fit(int col, int row, int wid, byte attr, cptr text)
{
    char buf[256];
    int max_len;

    if (!text)
        return;

    if (wid < 1)
        wid = 80;

    max_len = wid - col - 1;
    if (max_len < 1)
        return;

    SDL_strlcpy(buf, text, sizeof(buf));
    if ((int)strlen(buf) > max_len)
        buf[max_len] = '\0';

    Term_putstr(col, row, -1, attr, buf);
}

static bool character_sheet_prompt_append(char* buf, size_t buflen, cptr token, int max_width)
{
    size_t cur_len;
    size_t tok_len;
    int sep = 0;

    if (!buf || !buflen || !token || !token[0])
        return true;

    cur_len = strlen(buf);
    tok_len = strlen(token);
    if (cur_len > 0)
        sep = 2;

    if ((int)(cur_len + sep + tok_len) > max_width)
        return false;

    if (sep)
        SDL_strlcat(buf, "  ", buflen);
    SDL_strlcat(buf, token, buflen);
    return true;
}

static void character_sheet_build_prompt(bool steamdeck, bool include_curses,
    int wid, char* out, size_t outsz)
{
    int max_width;

    if (!out || !outsz)
        return;

    out[0] = '\0';

    if (wid < 1)
        wid = 80;

    max_width = wid - 2;
    if (max_width < 1)
        return;

    if (steamdeck)
    {
        char notes_label[16], story_label[16], file_label[16];
        char abilities_label[16], increase_label[16], help_label[16], back_label[16];
        char token[7][64];

        controller_prompt_label('n', "n", notes_label, sizeof(notes_label));
        controller_prompt_label(steamdeck_secondary_key(), "Y", story_label, sizeof(story_label));
        controller_prompt_label('e', "L1", file_label, sizeof(file_label));
        controller_prompt_label(steamdeck_alt_action_key(), "X", abilities_label, sizeof(abilities_label));
        controller_prompt_label(steamdeck_confirm_key(), "A", increase_label, sizeof(increase_label));
        controller_prompt_label(steamdeck_info_key(), "RS", help_label, sizeof(help_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));

        strnfmt(token[0], sizeof(token[0]), "%s abilities", abilities_label);
        strnfmt(token[1], sizeof(token[1]), "%s increase", increase_label);
        strnfmt(token[2], sizeof(token[2]), "%s help", help_label);
        strnfmt(token[3], sizeof(token[3]), "%s back", back_label);
        strnfmt(token[4], sizeof(token[4]), "%s notes", notes_label);
        strnfmt(token[5], sizeof(token[5]), "%s story", story_label);
        strnfmt(token[6], sizeof(token[6]), "%s file", file_label);

        for (int i = 0; i < 4; i++)
            (void)character_sheet_prompt_append(out, outsz, token[i], max_width);
        for (int i = 4; i < 7; i++)
            (void)character_sheet_prompt_append(out, outsz, token[i], max_width);
    }
    else
    {
        const char* essential[] = {
            "a abilities", "Space/i increase", "? help", "ESC back"
        };
        const char* optional[] = {
            "n notes", "s story", "f file"
        };

        for (int i = 0; i < (int)(sizeof(essential) / sizeof(essential[0])); i++)
            (void)character_sheet_prompt_append(out, outsz, essential[i], max_width);

        if (include_curses)
            (void)character_sheet_prompt_append(out, outsz, "c curses", max_width);

        for (int i = 0; i < (int)(sizeof(optional) / sizeof(optional[0])); i++)
            (void)character_sheet_prompt_append(out, outsz, optional[i], max_width);
    }

    if (!out[0])
    {
        if (steamdeck)
            SDL_strlcpy(out, "B back", outsz);
        else
            SDL_strlcpy(out, "ESC back", outsz);
    }
}

static void character_sheet_draw_page_indicator(int sheet_page, int compact_pages,
    int wid, int row, bool use_story_font)
{
    char page_buf[32];
    int col;

    if (compact_pages <= 1)
        return;

    if (wid < 1)
        wid = 80;

    strnfmt(page_buf, sizeof(page_buf), "%d/%d", sheet_page + 1, compact_pages);
    col = wid - (int)strlen(page_buf) - 1;
    if (col < 0)
        col = 0;

    if (use_story_font)
        sdl_story_font_enable();

    Term_putstr(col, row, -1, TERM_SLATE, page_buf);

    if (use_story_font)
        sdl_story_font_disable();
}

void do_cmd_character_sheet(void)
{
    char ch;

    int mode = 0;
    int sheet_page = 1;
    int body_scroll = 0;
    int last_sheet_page = -1;

    enum {
        CHAR_SHEET_MODE_COMPACT_DESC_FLAGS = 100,
        CHAR_SHEET_MODE_COMPACT_STATS_SKILLS = 101,
    };

    /* Clear any active banner before opening character sheet */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();

    /* Forever */
    while (1)
    {
        int wid = 80;
        int hgt = 24;
        int prompt_row;
        int indicator_row = 1;
        bool compact_sheet;
        int compact_pages;
        int max_body_scroll = 0;
        bool steamdeck = steamdeck_controls_active();

        Term_get_size(&wid, &hgt);
        if (wid < 1)
            wid = 80;
        if (hgt < 1)
            hgt = 24;

        compact_sheet = (wid < 80);
        compact_pages = compact_sheet ? 2 : 1;
        if (sheet_page >= compact_pages)
            sheet_page = compact_pages - 1;
        if (sheet_page < 0)
            sheet_page = 0;
        if (sheet_page != last_sheet_page)
        {
            body_scroll = 0;
            last_sheet_page = sheet_page;
        }

        if (compact_sheet)
        {
            switch (sheet_page)
            {
            case 0: mode = CHAR_SHEET_MODE_COMPACT_DESC_FLAGS; break;
            case 1: mode = CHAR_SHEET_MODE_COMPACT_STATS_SKILLS; break;
            default: mode = CHAR_SHEET_MODE_COMPACT_DESC_FLAGS; break;
            }
        }
        else
        {
            mode = 0;
        }

        if (compact_sheet && sheet_page == 0)
            display_player_compact_set_scroll(body_scroll);
        else
            display_player_compact_set_scroll(0);

        /* Display the player */
        display_player(mode);
        if (compact_sheet && sheet_page == 0)
        {
            max_body_scroll = display_player_compact_get_max_scroll();
            if (body_scroll > max_body_scroll)
            {
                body_scroll = max_body_scroll;
                display_player_compact_set_scroll(body_scroll);
                display_player(mode);
            }
        }

        if (compact_sheet && hgt <= 18)
            indicator_row = 0;

        prompt_row = hgt - 1;
        if (prompt_row < 0)
            prompt_row = 0;
        Term_erase(0, prompt_row, 255);

        /* Prompt - dynamic, width-aware, and user-friendly for new players */
        {
            char prompt_buf[256];
#ifdef DEBUG_CURSES
            const bool include_curses = true;
#else
            const bool include_curses = false;
#endif

            character_sheet_build_prompt(steamdeck, include_curses, wid, prompt_buf, sizeof(prompt_buf));

            if (story_character_enabled())
                sdl_story_font_enable();

            character_sheet_put_prompt_fit(1, prompt_row, wid, TERM_L_WHITE, prompt_buf);

            character_sheet_draw_page_indicator(sheet_page, compact_pages, wid,
                indicator_row,
                story_character_enabled());

            if (story_character_enabled())
                sdl_story_font_disable();
        }

        (void)Term_set_cursor(false);
        Term_fresh();  /* Render commands */

        if (story_character_enabled()) {
            sdl_story_font_disable();
        }

        /* Keep the cursor hidden while the character sheet is active. */
        {
            bool saved_hide_cursor = hide_cursor;
            hide_cursor = true;
            ch = inkey();
            hide_cursor = saved_hide_cursor;
        }

        /* Exit - B button (back) or ESC */
        if (ch == ESCAPE || (steamdeck && ch == steamdeck_back_key()))
            break;
        if ((ch == '\r') || (ch == '\n') || (ch == 'q') || (ch == 'Q'))
            break;

        if (compact_pages > 1)
        {
            if (sheet_page == 0 && max_body_scroll > 0)
            {
                if (ch == '8')
                {
                    if (body_scroll > 0)
                        body_scroll--;
                    continue;
                }
                if (ch == '2')
                {
                    if (body_scroll < max_body_scroll)
                        body_scroll++;
                    continue;
                }
            }

            if ((ch == '4') || ((ch == '8') && !(sheet_page == 0 && max_body_scroll > 0)))
            {
                sheet_page = (sheet_page + compact_pages - 1) % compact_pages;
                continue;
            }
            if ((ch == '6') || ((ch == '2') && !(sheet_page == 0 && max_body_scroll > 0)))
            {
                sheet_page = (sheet_page + 1) % compact_pages;
                continue;
            }
        }

        /* Increase skills - 'i', Space, or confirm button */
        if (ch == 'i' || ch == ' ' || ch == INPUT_BIND_CONFIRM
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            gain_skills();
            /* Force redraw after skill changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* Show notes - 'n' */
        else if (ch == 'n')
        {
            do_cmd_knowledge_notes();
        }

        /* Story stats - 's' or Y button */
        else if (ch == 's' || (steamdeck && ch == steamdeck_secondary_key()))
        {
            print_metarun_stats();
        }

#ifdef DEBUG_CURSES
        /* Curses Menu */
        else if (ch == 'c')
        {
            dbg_show_active_flags();
        }
#endif

        /* Abilities - 'a', Tab, or X button */
        else if ((ch == 'a') || (ch == '\t') || (steamdeck && ch == steamdeck_alt_action_key()))
        {
            (void)do_cmd_ability_screen();
            /* Force redraw after ability changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* File dump - 'f' or L1 ('e') */
        else if (ch == 'f' || (steamdeck && ch == 'e'))
        {
            char ftmp[80];

            strnfmt(ftmp, sizeof(ftmp), "%s.txt", op_ptr->base_name);

            if (term_get_string("File name: ", ftmp, sizeof(ftmp)))
            {
                if (ftmp[0] && (ftmp[0] != ' '))
                {
                    if (file_character(ftmp, false))
                    {
                        msg_print("Character dump failed!");
                    }
                    else
                    {
                        msg_print("Character dump successful.");
                    }
                }
            }
        }

        /* Tutorial / Help - '?' or RS Right */
        else if (ch == '?' || (steamdeck && ch == steamdeck_info_key()))
        {
            display_character_tutorial();
        }

        /* Oops */
        else
        {
            bell("Illegal command for character sheet!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_pop_supporting_panes_hidden();
    screen_load();

    /* Force redraw after screen restore if skills/abilities were changed */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
    handle_stuff();
}

#define COL_SKILL 2
#define COL_ABILITY 16
#define COL_DESCRIPTION 41
#define ABILITY_MENU_LIST_WIDTH (COL_DESCRIPTION - COL_ABILITY)

static bool ability_menu_use_compact_layout(void)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    return (wid < 80);
}
static int ability_menu_list_col(void)
{
    return ability_menu_use_compact_layout() ? COL_SKILL : COL_ABILITY;
}

static int ability_menu_description_col(void)
{
    return ability_menu_use_compact_layout()
        ? COL_SKILL + ABILITY_MENU_LIST_WIDTH
        : COL_DESCRIPTION;
}

static int ability_menu_description_wrap(int desc_col)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    if (wid <= desc_col)
        return desc_col + 1;

    return wid - 1;
}

int abilities_in_skill(int skilltype);
bool prereqs(int skilltype, int abilitynum);

static int ability_purchase_exp_cost(int skilltype)
{
    int is_free = (c_info[p_ptr->pcharacter].flags & RHF_FREE) ? 1 : 0;
    int unit_cost = 500 - 200 * is_free;
    int exp_cost = (abilities_in_skill(skilltype) + 1) * unit_cost;

    exp_cost -= unit_cost * affinity_level(skilltype);

    if (skilltype == S_SNG)
        exp_cost -= unit_cost * minstrel_level();

    exp_cost += 100 * curse_flag_delta_cur(CUR_ABILITY_COST);

    if (exp_cost < 0)
        exp_cost = 0;

    return exp_cost;
}

static int ability_menu_text_width(int desc_col, int indent)
{
    int wrap = ability_menu_description_wrap(desc_col);
    int start = desc_col + indent;

    if (wrap < start)
        return 1;

    return wrap - start + 1;
}

static void ability_menu_format_amount_line(char* buf, size_t buflen,
    cptr long_label, cptr short_label, int need, int have, int max_width)
{
    if (max_width <= 30)
        strnfmt(buf, buflen, "%s %d / %d", short_label, need, have);
    else
        strnfmt(buf, buflen, "%d %s (you have %d)", need, long_label, have);
}

static void ability_menu_append_text(char* out, size_t outsz, size_t* cur,
    cptr text)
{
    if (!out || !outsz || !cur || !text)
        return;

    strnfcat(out, outsz, cur, "%s", text);
}

static cptr ability_menu_controller_text(cptr src, char* out, size_t outsz)
{
    char wait_label[32];
    char fletch_label[32];
    char exchange_label[32];
    char wait_token[64];
    char fletch_token[64];
    char exchange_token[64];
    struct replacement
    {
        cptr from;
        cptr to;
    } replacements[3];
    size_t cur = 0;
    const char* p;

    if (!src || !out || outsz == 0)
        return src;

    if (!steamdeck_controls_active())
        return src;

    controller_prompt_label_no_sticks('z', "z", wait_label, sizeof(wait_label));
    controller_prompt_label_no_sticks('-', "-", fletch_label, sizeof(fletch_label));
    controller_prompt_label_no_sticks('X', "X", exchange_label, sizeof(exchange_label));

    strnfmt(wait_token, sizeof(wait_token), "(%s/5)", wait_label);
    strnfmt(fletch_token, sizeof(fletch_token), "Use %s to", fletch_label);
    strnfmt(exchange_token, sizeof(exchange_token), "Use %s to", exchange_label);

    replacements[0].from = "(z/5)";
    replacements[0].to = wait_token;
    replacements[1].from = "Use '-' to";
    replacements[1].to = fletch_token;
    replacements[2].from = "Use 'X' to";
    replacements[2].to = exchange_token;

    out[0] = '\0';
    p = src;

    while (*p)
    {
        bool replaced = false;

        for (int i = 0; i < (int)N_ELEMENTS(replacements); i++)
        {
            size_t from_len = strlen(replacements[i].from);

            if (!strncmp(p, replacements[i].from, from_len))
            {
                ability_menu_append_text(out, outsz, &cur, replacements[i].to);
                p += from_len;
                replaced = true;
                break;
            }
        }

        if (!replaced)
        {
            char tmp[2] = { *p, '\0' };
            ability_menu_append_text(out, outsz, &cur, tmp);
            p++;
        }
    }

    return out;
}

static int ability_menu_next_row_after_text(int desc_col, int fallback_row)
{
    int x = desc_col;
    int y = fallback_row;

    Term_locate(&x, &y);

    if (x > desc_col)
        y++;

    return y;
}

static void ability_menu_render_prerequisites_block(int skilltype,
    const ability_type* b_ptr, int desc_col)
{
    int j;
    int row = ability_menu_next_row_after_text(desc_col, 3);
    int info_width = ability_menu_text_width(desc_col, 2);
    char buf[80];

    Term_putstr(desc_col, row, -1, TERM_YELLOW, "Prerequisites:");

    ability_menu_format_amount_line(buf, sizeof(buf), "skill points", "Skill",
        b_ptr->level, p_ptr->skill_base[skilltype], info_width);

    Term_putstr(desc_col + 2, row + 1, -1,
        (b_ptr->level <= p_ptr->skill_base[skilltype]) ? TERM_L_GREEN
                                                       : TERM_L_DARK,
        buf);

    row += 2;

    if (!p_ptr->active_ability[S_PER][PER_QUICK_STUDY])
    {
        for (j = 0; j < b_ptr->prereqs; j++)
        {
            if (j == 0)
            {
                strnfmt(buf, sizeof(buf), "%s",
                    b_name
                        + (&b_info[ability_index(b_ptr->prereq_skilltype[j],
                               b_ptr->prereq_abilitynum[j])])
                              ->name);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "or %s",
                    b_name
                        + (&b_info[ability_index(b_ptr->prereq_skilltype[j],
                               b_ptr->prereq_abilitynum[j])])
                              ->name);
            }

            Term_putstr(j == 0 ? desc_col + 2 : desc_col + 5, row + j, -1,
                p_ptr->innate_ability[b_ptr->prereq_skilltype[j]]
                                 [b_ptr->prereq_abilitynum[j]]
                    ? TERM_L_GREEN
                    : TERM_L_DARK,
                buf);
        }

        row += b_ptr->prereqs;
    }
    else if (b_ptr->prereqs > 0)
    {
        Term_putstr(desc_col + 2, row, -1, TERM_GREEN, "Quick Study");
        row++;
    }

    if (skilltype != S_SPC && prereqs(skilltype, b_ptr->abilitynum))
    {
        int exp_cost = ability_purchase_exp_cost(skilltype);

        Term_putstr(desc_col, row, -1, TERM_YELLOW, "Current price:");

        ability_menu_format_amount_line(buf, sizeof(buf), "experience", "Exp",
            exp_cost, p_ptr->new_exp, info_width);
        Term_putstr(desc_col + 2, row + 1, -1,
            (exp_cost <= p_ptr->new_exp) ? TERM_L_GREEN : TERM_L_DARK, buf);

        row += 2;
    }

    Term_gotoxy(desc_col, row);
}

static int ability_menu_stepped_song_bonus(int skill, int first_threshold,
    int next_gap)
{
    int bonus = 1;
    int threshold = first_threshold;
    int gap = next_gap;

    if (skill < 0)
        skill = 0;

    while (skill > threshold)
    {
        bonus++;
        threshold += gap;
        gap++;
    }

    return bonus;
}

static int ability_menu_current_song_score(void)
{
    return MAX(0, p_ptr->skill_use[S_SNG]);
}

static int ability_menu_minor_song_score(int song_skill)
{
    if (song_skill <= 0)
        return 0;

    if (c_info[p_ptr->pcharacter].flags_u & UNQ_WOVEN_MASTER)
        return song_skill;

    return song_skill / 2;
}

static int ability_menu_song_synergy_bonus(int song_skill)
{
    if (song_skill <= 0)
        return 0;

    return (song_skill + 5) / 10;
}

static void ability_menu_render_song_bonus_block(const ability_type* b_ptr)
{
    int song_skill = ability_menu_current_song_score();
    char bonus_text[256];

    bonus_text[0] = '\0';

    switch (b_ptr->abilitynum)
    {
    case SNG_ELBERETH:
    {
        int will_penalty = (song_skill > 0) ? MAX(1, song_skill / 5) : 0;
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: enemy Will -%d.", song_skill,
            will_penalty);
        break;
    }
    case SNG_CHALLENGE:
    {
        int debuff = (song_skill > 0) ? MAX(1, song_skill / 5) : 0;
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: enemy Will and Stealth -%d.",
            song_skill, debuff);
        break;
    }
    case SNG_DELVINGS:
    {
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: delving range %d squares.",
            song_skill, song_skill + 8);
        break;
    }
    case SNG_FREEDOM:
    {
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: freedom checks use Song %d and grant +1 free action while singing.",
            song_skill, song_skill);
        break;
    }
    case SNG_SILENCE:
    {
        int silence_bonus = song_skill / 2;
        int enemy_song_penalty = silence_bonus / 2;
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: +%d to hush/noise checks; enemy songs -%d.",
            song_skill, silence_bonus, enemy_song_penalty);
        break;
    }
    case SNG_STAUNCHING:
    {
        int base_heal = song_skill / 12;
        int extra_turns = song_skill % 12;

        if (extra_turns > 0)
        {
            strnfmt(bonus_text, sizeof(bonus_text),
                "\n\nCurrent effect at Song %d: stops bleeding and heals %d HP/turn, with +1 extra on %d turns in 12.",
                song_skill, base_heal, extra_turns);
        }
        else
        {
            strnfmt(bonus_text, sizeof(bonus_text),
                "\n\nCurrent effect at Song %d: stops bleeding and heals %d HP/turn.",
                song_skill, base_heal);
        }
        break;
    }
    case SNG_THRESHOLDS:
    {
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: door-warding checks use Song %d.",
            song_skill, song_skill);
        break;
    }
    case SNG_TREES:
    {
        int light_radius = ability_menu_stepped_song_bonus(song_skill, 5, 6);
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: +%d light radius.", song_skill,
            light_radius);
        break;
    }
    case SNG_WOVEN_THEMES:
    {
        int minor_skill = ability_menu_minor_song_score(song_skill);
        int synergy_bonus = ability_menu_song_synergy_bonus(song_skill);
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: a minor theme uses Song %d; a valid synergy pair adds +%d Song.",
            song_skill, minor_skill, synergy_bonus);
        break;
    }
    case SNG_SLAYING:
    {
        int hp_threshold = song_skill * 2;
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_HURIN)
            hp_threshold *= 2;

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: criticals can slay foes at %d HP or less.",
            song_skill, hp_threshold);
        break;
    }
    case SNG_REVEALING:
    {
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: revealing range %d squares.",
            song_skill, (song_skill / 2) + 8);
        break;
    }
    case SNG_ELVENESS:
    {
        int evasion_bonus = ability_menu_stepped_song_bonus(song_skill, 7, 8);
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: +1 Grace and +%d Evasion.",
            song_skill, evasion_bonus);
        break;
    }
    case SNG_STAYING:
    {
        int will_bonus = song_skill / 2;
        int protection_dice = 2;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN)
        {
            will_bonus = song_skill * 2;
            protection_dice = 4;
        }

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: +%d Will and [%dd2] protection.",
            song_skill, will_bonus, protection_dice);
        break;
    }
    case SNG_DISGUISE:
    {
        int disguise_bonus = song_skill + 5;
        const char* extra = (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_TURGON)
            ? " + Perception"
            : "";

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: disguise checks use %d + Will%s.",
            song_skill, disguise_bonus, extra);
        break;
    }
    case SNG_LORIEN:
    {
        int sleep_score = song_skill;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_LUT)
            sleep_score = (3 * song_skill) / 2;

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: sleep checks use %d.",
            song_skill, sleep_score);
        break;
    }
    case SNG_SHATTERING:
    {
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: shatter checks use Song %d; each success has a %d%% weaken chance.",
            song_skill, song_skill, song_skill / 3);
        break;
    }
    case SNG_MASTERY:
    {
        int mastery_bonus = song_skill;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_THINGOL)
            mastery_bonus = (7 * song_skill) / 4;

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: mastery rolls are 2d8 + %d.",
            song_skill, mastery_bonus);
        break;
    }
    case SNG_GRA:
    {
        SDL_strlcpy(bonus_text, "\n\nCurrent effect: +1 Grace.",
            sizeof(bonus_text));
        break;
    }
    case SNG_CONTEST:
    {
        int will_penalty = MAX(1, song_skill / 3);
        int stealth_penalty = MAX(1, song_skill / 2);
        int evasion_penalty = MAX(1, song_skill / 5);
        int armour_penalty = MAX(1, song_skill / 12);

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: duel checks use Song + Will/2; victory inflicts -%d Will, -%d Stealth, -%d Evasion, -%d armour die.",
            song_skill, will_penalty, stealth_penalty, evasion_penalty,
            armour_penalty);
        break;
    }
    case SNG_LAMENT:
    {
        int will_penalty = MAX(1, song_skill / 2);
        int attrition_steps = MAX(1, song_skill / 12);

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect at Song %d: duel checks use Song + Will/2; victory inflicts -%d Will and -%d health/damage steps.",
            song_skill, will_penalty, attrition_steps);
        break;
    }
    default:
        break;
    }

    if (bonus_text[0])
        text_out_to_screen(TERM_L_GREEN, bonus_text);
}

/* ------------------------------------------------------------------
 * add_random_curse()
 *    Marks the item cursed
 *    Gives it random negative modifiers
 *   Compatible with SIL-QH object_type (no flags1/2/3 fields)
 * ------------------------------------------------------------------ */
void add_random_curse(object_type *o_ptr)
{
    /* 1. make it show up as {cursed} right away */
    o_ptr->ident |= IDENT_CURSED;

    /* 2. negative pval / attack / evasion */
    int old_pval = o_ptr->pval;
    if (o_ptr->pval > 0)  o_ptr->pval = -(rand_int(3) + 1); /* 1  3 */
    int pval_delta = o_ptr->pval - old_pval;
    if (pval_delta != 0)
        object_apply_pval_delta_with_mask(o_ptr, object_pval_flags1(o_ptr), pval_delta);
    if (o_ptr->att > 0) o_ptr->att = -(rand_int(3) + 1);
    if (o_ptr->evn > 0) o_ptr->evn = -(rand_int(3) + 1);

    /* 3. very small chance to damage dice on weapons / armour */
    if (one_in_(8))
    {
        if (o_ptr->dd) o_ptr->dd = MAX(1, o_ptr->dd - 1);
        if (o_ptr->pd) o_ptr->pd = MAX(1, o_ptr->pd - 1);
    }
}


int ability_index(int skilltype, int abilitynum)
{
    int i;
    ability_type* b_ptr;

    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skilltype)
            continue;

        /* Stop if you get the correct ability number */
        if (b_ptr->abilitynum == abilitynum)
            return (i);
    }

    // Hack: there is no reasonable default value, but this will do
    return (0);
}

/*
 *  Counts the number of innate abilities in a skill
 */

int abilities_in_skill(int skilltype)
{
    int i;
    ability_type* b_ptr;
    int count = 0;

    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skilltype)
            continue;

        /* Add to the count */
        if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            count++;
    }

    return (count);
}

static bool prereq_abilities_met(const ability_type* b_ptr)
{
    int i;

    if (b_ptr->prereqs > 0 && !(p_ptr->active_ability[S_PER][PER_QUICK_STUDY]))
    {
        for (i = 0; i < b_ptr->prereqs; i++)
        {
            if (p_ptr->innate_ability[b_ptr->prereq_skilltype[i]]
                                     [b_ptr->prereq_abilitynum[i]])
                return (true);
        }
        return (false);
    }

    return (true);
}

bool prereqs(int skilltype, int abilitynum)
{
    ability_type* b_ptr;

    b_ptr = &b_info[ability_index(skilltype, abilitynum)];

    if (p_ptr->skill_base[skilltype] < b_ptr->level)
    {
        return (false);
    }

    return prereq_abilities_met(b_ptr);
}

static char song_menu_letter(int song_index)
{
    char letter = (char)('a' + song_index);

    if (letter >= 's')
        letter++;

    return letter;
}

static int song_index_from_menu_letter(char letter)
{
    if (letter < 'a' || letter > 'z')
        return -1;

    if (letter == 's')
        return -1;

    if (letter > 's')
        letter--;

    return (int)(letter - 'a');
}

static u32b song_menu_use_counter = 0;
static u32b song_menu_last_used[SNG_MAX];

static bool song_menu_is_singable(int song)
{
    return (song >= 0) && (song < SNG_MAX) && (song != SNG_WOVEN_THEMES)
        && (song != SNG_GRA);
}

static bool song_menu_sorts_before(int song, int other)
{
    if (op_ptr && song_list_sort_by_recent
        && (song_menu_last_used[song] != song_menu_last_used[other]))
    {
        return song_menu_last_used[song] > song_menu_last_used[other];
    }

    return song < other;
}

static int song_menu_collect_available(int songs[], int max_songs)
{
    int i, j;
    int count = 0;

    for (i = 0; i < SNG_MAX; i++)
    {
        if (!song_menu_is_singable(i))
            continue;

        if (!p_ptr->active_ability[S_SNG][i])
            continue;

        if (count < max_songs)
            songs[count++] = i;
    }

    for (i = 1; i < count; i++)
    {
        int song = songs[i];

        for (j = i - 1; j >= 0 && song_menu_sorts_before(song, songs[j]); j--)
        {
            songs[j + 1] = songs[j];
        }

        songs[j + 1] = song;
    }

    return count;
}

static int song_menu_choice_from_highlight(int highlight, const int songs[],
    int song_count)
{
    if (highlight == 0)
        return SNG_NOTHING;

    if ((highlight > 0) && (highlight <= song_count))
        return songs[highlight - 1];

    if ((p_ptr->song2 != SNG_NOTHING) && (highlight == song_count + 1))
        return SNG_EXCHANGE_THEMES;

    return -1;
}

static int song_menu_total_options(int song_count)
{
    int total = 1 + song_count;

    if (p_ptr->song2 != SNG_NOTHING)
        total++;

    return total;
}

static void song_menu_mark_used(int song)
{
    if (!song_menu_is_singable(song))
        return;

    song_menu_last_used[song] = ++song_menu_use_counter;
}

/*
 * Display the available songs (modelled on show_inven) with optional highlighting.
 */
void show_songs_with_highlight(int highlight)
{
    int i, j;
    int current_line = 0;
    bool steamdeck = steamdeck_controls_active();

    int col = 26;
    int label_col = steamdeck ? indexed_menu_prefix_col(col) : col;
    int text_col = steamdeck ? col : col + 3;

    char tmp_val[80];

    int songs[SNG_MAX];
    int song_count = song_menu_collect_available(songs, SNG_MAX);

    // add a line for the 'stop singing' command

    /* Clear the line */
    prt("", 1, col - 2);

    /* Clear the line with the (possibly indented) index */
    put_str(steamdeck ? ((highlight == current_line) ? "> " : "  ") : "s)",
        1, col);

    /* Display the entry itself - highlight if selected */
    if (highlight == current_line)
        c_put_str(TERM_L_BLUE, "Stop Singing", 1, text_col);
    else
        c_put_str(TERM_SLATE, "Stop Singing", 1, text_col);
    current_line++;

    /* Output each entry */
    for (j = 0; j < song_count; j++)
    {
        cptr desc;

        /* Get the index */
        i = songs[j];
        desc = b_name + (&b_info[ability_index(S_SNG, i)])->name;

        /* Clear the line */
        prt("", j + 2, col - 2);

        /* Prepare an index --(-- */
        if (steamdeck)
            SDL_strlcpy(tmp_val, (highlight == current_line) ? "> " : "  ",
                sizeof(tmp_val));
        else
            sprintf(tmp_val, "%c)", song_menu_letter(i));

        /* Clear the line with the (possibly indented) index */
        put_str(tmp_val, j + 2, label_col);

        /* Display the entry itself - highlight if selected */
        if (highlight == current_line)
            c_put_str(TERM_L_BLUE, desc, j + 2, text_col);
        else
            c_put_str(TERM_L_WHITE, desc, j + 2, text_col);
        current_line++;
    }

    // add a line for the 'exchange themes' command
    if (p_ptr->song2 != SNG_NOTHING)
    {
        /* Clear the line */
        prt("", j + 2, col - 2);

        /* Clear the line with the (possibly indented) index */
        put_str(steamdeck ? ((highlight == current_line) ? "> " : "  ") : "x)",
            j + 2, col);

        /* Display the entry itself - highlight if selected */
        if (highlight == current_line)
            c_put_str(TERM_L_BLUE, "Exchange themes", j + 2, text_col);
        else
            c_put_str(TERM_L_BLUE, "Exchange themes", j + 2, text_col);

        j++;
    }

    /* Make a "shadow" below the list (only if needed) */
    if (j && (j < 23))
        prt("", j + 2, col - 2);
}

/*
 * Display the available songs (modelled on show_inven).
 */
void show_songs(void)
{
    show_songs_with_highlight(-1); // No highlighting
}

void do_cmd_change_song()
{
    int i;
    bool done = false;

    int songs[SNG_MAX];
    int song_count = 0;
    int song_choice = -1;
    int highlight = 0; // Add highlight tracking

    char out_val[80];
    char tmp_val[80];

    char which;
    bool steamdeck = steamdeck_controls_active();

    log_debug("Player opening song selection menu");

    // Check for song lockout timer first
    if (p_ptr->song_lockout_timer > 0)
    {
        msg_format("You cannot sing for %d more turn%s.", 
            p_ptr->song_lockout_timer,
            (p_ptr->song_lockout_timer == 1) ? "" : "s");
        return;
    }

    song_count = song_menu_collect_available(songs, SNG_MAX);

    // abort if you know no songs
    if (song_count == 0)
    {
        log_trace("No songs available - player knows no songs of power");
        msg_print("You do not know any songs of power.");
        return;
    }
    
    log_debug("Player has %d songs available", song_count);

    /* Flush the prompt */
    Term_fresh();

    /* Song selectors always start with the list visible. */
    p_ptr->command_see = true;

    /* Start out in "display" mode */
    if (p_ptr->command_see)
    {
        /* Save screen */
        screen_save();
    }

    /* Repeat until done */
    while (!done)
    {
        /* Redraw if needed */
        if (p_ptr->command_see)
            show_songs_with_highlight(highlight);

        /* Begin the prompt */
        if (steamdeck)
            sprintf(out_val, "D-pad choose");
        else
            sprintf(out_val, "Songs: s");

        for (i = 0; i < song_count; i++)
        {
            if (!steamdeck)
            {
                SDL_strlcat(out_val, ",", sizeof(out_val));
                sprintf(tmp_val, "%c", song_menu_letter(songs[i]));

                /* Append */
                SDL_strlcat(out_val, tmp_val, sizeof(out_val));
            }
        }

        // add an 'x' option if using woven themes
        if (p_ptr->song2 != SNG_NOTHING)
        {
            /* Append */
            if (!steamdeck)
                SDL_strlcat(out_val, ",x", sizeof(out_val));
        }

        /* Indicate ability to "view" */
        if (!p_ptr->command_see)
            SDL_strlcat(out_val, ", * to see", sizeof(out_val));
        else if (steamdeck)
            SDL_strlcat(out_val, ", A select, B back", sizeof(out_val));

        /* Build the prompt */
        strnfmt(tmp_val, sizeof(tmp_val), "(%s) Sing which song: ", out_val);

        /* Show the prompt */
        prt(tmp_val, 0, 0);

        /* Get a key */
        which = inkey();

        if (which == ESCAPE || (steamdeck && which == steamdeck_back_key()))
        {
            log_trace("Song selection cancelled by player");
            done = true;
            continue;
        }

        if (steamdeck && which == steamdeck_confirm_key())
        {
            which = ' ';
        }

        /* Parse it */
        switch (which)
        {
        case '\r': // Enter - select highlighted item when menu is visible, otherwise exit
        {
            if (p_ptr->command_see)
            {
                song_choice =
                    song_menu_choice_from_highlight(highlight, songs, song_count);
                
                if (song_choice >= 0)
                {
                    done = true;
                }
            }
            else
            {
                log_trace("Song selection cancelled by player");
                done = true;
            }
            break;
        }

        case '*':
        case '?':
        {
            /* Hide the list */
            if (p_ptr->command_see)
            {
                /* Flip flag */
                p_ptr->command_see = false;

                /* Load screen */
                screen_load();
            }

            /* Show the list */
            else
            {
                /* Save screen */
                screen_save();

                /* Flip flag */
                p_ptr->command_see = true;
            }

            break;
        }

        case ' ': // Space - select highlighted item when menu is visible, otherwise toggle menu
        {
            if (p_ptr->command_see)
            {
                song_choice =
                    song_menu_choice_from_highlight(highlight, songs, song_count);
                
                if (song_choice >= 0)
                {
                    done = true;
                }
            }
            else
            {
                /* Show the list */
                /* Save screen */
                screen_save();

                /* Flip flag */
                p_ptr->command_see = true;
            }
            break;
        }

        case '2': // Down arrow / scroll down
        {
            if (p_ptr->command_see)
            {
                int total_options = song_menu_total_options(song_count);

                highlight = (highlight + 1) % total_options;
            }
            break;
        }

        case '8': // Up arrow / scroll up
        {
            if (p_ptr->command_see)
            {
                int total_options = song_menu_total_options(song_count);

                highlight = (highlight - 1 + total_options) % total_options;
            }
            break;
        }

        case '6': // Right arrow / select highlighted
        {
            if (p_ptr->command_see)
            {
                song_choice =
                    song_menu_choice_from_highlight(highlight, songs, song_count);
                
                if (song_choice >= 0)
                {
                    done = true;
                }
            }
            break;
        }

        case 's':
        {
            log_debug("Player selected to stop singing");
            song_choice = SNG_NOTHING;
            done = true;
            break;
        }

        case 'x':
        {
            if (steamdeck)
            {
                log_trace("Illegal song choice attempted");
                bell("Illegal song choice.");
                break;
            }
            if (p_ptr->song2 != SNG_NOTHING)
            {
                log_debug("Player exchanging woven themes");
                song_choice = SNG_EXCHANGE_THEMES;
                done = true;
                break;
            }
            else
            {
                log_trace("Illegal song choice - no second theme to exchange");
                bell("Illegal song choice.");
                break;
            }
        }

        default:
        {
            if (steamdeck)
            {
                log_trace("Illegal song choice attempted");
                bell("Illegal song choice.");
                break;
            }

            song_choice = song_index_from_menu_letter(which);

            if (song_choice >= 0 && song_choice < SNG_MAX)
            {
                if (!song_menu_is_singable(song_choice))
                {
                    song_choice = -1;
                }
                else if (p_ptr->active_ability[S_SNG][song_choice])
                {
                    log_debug("Player selected song %d", song_choice);
                    done = true;
                    break;
                }
                else
                {
                    song_choice = -1;
                }
            }

            log_trace("Illegal song choice attempted");
            bell("Illegal song choice.");
            break;
        }
        }
    }

    /* Fix the screen if necessary */
    if (p_ptr->command_see)
    {
        /* Load screen */
        screen_load();

        /* Hack -- Cancel "display" */
        p_ptr->command_see = false;
    }

    /* Clear the prompt line */
    prt("", 0, 0);

    if (song_choice >= 0)
    {
        bool choice_stops_current_song = (song_choice == p_ptr->song1)
            || ((p_ptr->song2 != SNG_NOTHING) && (song_choice == p_ptr->song2));

        if ((song_choice != SNG_NOTHING)
            && (song_choice != SNG_EXCHANGE_THEMES)
            && !choice_stops_current_song)
        {
            if (chosen_oath(OATH_SILENCE) && !oath_invalid(OATH_SILENCE))
            {
                /* Use oath-specific confirmation prompt */
                char* prompt = oath_confirmation_prompt(OATH_SILENCE);
                if (!prompt || !prompt[0]) prompt = "Are you certain you wish to break your Oath of Silence?";
                
                if (get_check_oath_multiline(prompt))
                {
                    log_info("Player broke oath of silence to sing");
                    
                    /* Curse message and selection handled by apply_oath_breaking_curse */
                    do_cmd_note("Broke your oath", p_ptr->depth);
                    
                    /* Apply oath breaking consequences */
                    apply_oath_breaking_curse(OATH_SILENCE);
                    
                    /* Only mark oath as broken if player actually has it */
                    p_ptr->oaths_broken |= OATH_SILENCE_FLAG;
                }
                else
                {
                    log_debug("Player cancelled song due to oath of silence");
                    return;
                }
            }
        }

        log_info("Player changed song to %s", song_choice == SNG_NOTHING ? "silence" : 
                 song_choice == SNG_EXCHANGE_THEMES ? "exchange themes" : "new song");
        change_song(song_choice);
        if (song_menu_is_singable(song_choice) && singing(song_choice))
            song_menu_mark_used(song_choice);
    }
}

void wipe_screen_from(int col)
{
    int i;
    int wid = Term ? Term->wid : 80;
    int hgt = Term ? Term->hgt : 24;

    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;
    if (col >= wid)
        return;

    for (i = 1; i < hgt; i++)
        Term_erase(col, i, wid - col);
}

int elf_bane_bonus(monster_type* m_ptr)
{
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return (0);
    else
        r_ptr = &r_info[m_ptr->r_idx];

    /* race.txt serials 0-2 are Noldor; serial 3 is Sindar */
    if (((r_ptr->flags2 & (RF2_ELFBANE)) && (p_ptr->prace <= 3))
        || ((r_ptr->flags4 & (RF4_NOLDORBANE)) && (p_ptr->prace <= 2))
        || ((r_ptr->flags4 & (RF4_SINDARBANE)) && (p_ptr->prace == 3)))
    {
        return (5);
    }

    return (0);
}

int dwarf_bane_bonus(monster_type* m_ptr)
{
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return (0);
    else
        r_ptr = &r_info[m_ptr->r_idx];

    /* race.txt serial 4 is Naugrim */
    if ((r_ptr->flags4 & (RF4_DWARFBANE)) && (p_ptr->prace == 4))
    {
        return (5);
    }

    return (0);
}

int edain_bane_bonus(monster_type* m_ptr)
{
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return (0);
    else
        r_ptr = &r_info[m_ptr->r_idx];

    /* race.txt serial 5 is Edain */
    if ((r_ptr->flags4 & (RF4_EDAINBANE)) && (p_ptr->prace == 5))
    {
        return (5);
    }

    return (0);
}

#define BANE_TYPES 13

static u32b bane_flag[] = { 0L, RF3_ORC, RF3_WOLF, RF3_SPIDER, RF3_TROLL,
    RF3_UNDEAD, RF3_RAUKO, RF3_SERPENT, RF3_DRAGON, RF3_VAMPIRE,
    RF3_HORROR, RF3_CAT, RF3_GIANT };

char* bane_name[] = { "Nothing", "Orc", "Wolf", "Spider", "Troll", "Wraith",
    "Rauko", "Serpent", "Dragon", "Vampire", "Horror", "Cat", "Giant" };

int bane_type_killed(int i)
{
    int j;
    int k = 0;

    /* Scan the monster races */
    for (j = 1; j < z_info->r_max; j++)
    {
        monster_race* r_ptr = &r_info[j];
        monster_lore* l_ptr = &l_list[j];

        if (r_ptr->flags3 & (bane_flag[i]))
        {
            k += l_ptr->pkills;
        }
    }

    return (k);
}

int bane_bonus_aux(void)
{
    int i = 2;
    int bonus = 0;
    int killed;

    killed = bane_type_killed(p_ptr->bane_type);
    while (i <= killed)
    {
        i *= 2;
        bonus++;
    }

    return (bonus);
}

int bane_bonus(monster_type* m_ptr)
{
    int bonus = 0;
    monster_race* r_ptr;

    // paranoia
    if (m_ptr == NULL)
        return (0);

    // entranced players don't get the bonus
    if (p_ptr->entranced)
        return (0);

    // knocked out players don't get the bonus
    if (p_ptr->stun > 100)
        return (0);

    r_ptr = &r_info[m_ptr->r_idx];

    if (r_ptr->flags3 & (bane_flag[p_ptr->bane_type]))
    {
        bonus = bane_bonus_aux();
    }

    return (bonus);
}

/*
 * Calculate bane bonus for a specific bane type.
 * This is a helper function that can be used for both player bane and artifact bane.
 */
int bane_bonus_for_type(int bane_type_idx)
{
    int i = 2;
    int bonus = 0;
    int killed;

    if (bane_type_idx <= 0 || bane_type_idx >= BANE_TYPES)
        return 0;

    killed = bane_type_killed(bane_type_idx);
    while (i <= killed)
    {
        i *= 2;
        bonus++;
    }

    return bonus;
}

/*
 * Calculate bane bonus from artifact-granted Bane abilities.
 * These use a pre-selected bane type from the artifact definition.
 */
int artifact_bane_bonus(monster_type* m_ptr)
{
    int bonus = 0;
    int i, j;
    monster_race* r_ptr;
    object_type* o_ptr;

    // paranoia
    if (m_ptr == NULL)
        return 0;

    // entranced players don't get the bonus
    if (p_ptr->entranced)
        return 0;

    // knocked out players don't get the bonus
    if (p_ptr->stun > 100)
        return 0;

    r_ptr = &r_info[m_ptr->r_idx];

    // Check all equipped items
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        // Skip empty slots
        if (!o_ptr->k_idx)
            continue;

        // Check all abilities on this item
        for (j = 0; j < o_ptr->abilities; j++)
        {
            // Is this a Bane ability with a pre-selected type?
            if (o_ptr->skilltype[j] == S_PER && o_ptr->abilitynum[j] == PER_BANE
                && o_ptr->bane_type[j] > 0)
            {
                // Skip if this matches the player's innate bane type
                // (they already get bonus from innate, no stacking)
                if (o_ptr->bane_type[j] == p_ptr->bane_type)
                    continue;

                // Does the monster match this bane type?
                if (r_ptr->flags3 & bane_flag[o_ptr->bane_type[j]])
                {
                    int this_bonus = bane_bonus_for_type(o_ptr->bane_type[j]);
                    if (this_bonus > bonus)
                        bonus = this_bonus;
                }
            }
        }
    }

    return bonus;
}

int spider_bane_bonus(void)
{
    if (bane_flag[p_ptr->bane_type] == RF3_SPIDER)
        return (bane_bonus_aux());
    else
        return (0);
}

/*
 * Calculate spider bane bonus from artifact-granted Bane abilities.
 * Used for web-related difficulty checks.
 */
int artifact_spider_bane_bonus(void)
{
    int bonus = 0;
    int i, j;
    object_type* o_ptr;

    // Check all equipped items
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        // Skip empty slots
        if (!o_ptr->k_idx)
            continue;

        // Check all abilities on this item
        for (j = 0; j < o_ptr->abilities; j++)
        {
            // Is this a Bane ability with Spider type? (Spider = 3)
            if (o_ptr->skilltype[j] == S_PER && o_ptr->abilitynum[j] == PER_BANE
                && bane_flag[o_ptr->bane_type[j]] == RF3_SPIDER)
            {
                // Skip if this matches the player's innate bane type
                if (o_ptr->bane_type[j] == p_ptr->bane_type)
                    continue;

                int this_bonus = bane_bonus_for_type(o_ptr->bane_type[j]);
                if (this_bonus > bonus)
                    bonus = this_bonus;
            }
        }
    }

    return bonus;
}

int unique_bane_bonus(monster_type* m_ptr)
{
    int bonus = 0;
    monster_race* r_ptr;

    // paranoia
    if (m_ptr == NULL)
        return (0);

    // entranced players don't get the bonus
    if (p_ptr->entranced)
        return (0);

    // knocked out players don't get the bonus
    if (p_ptr->stun > 100)
        return (0);

    // Must have the unique bane special ability
    if (!p_ptr->active_ability[S_SPC][SPC_UNIQUE_BANE])
        return (0);

    r_ptr = &r_info[m_ptr->r_idx];

    // Check if the monster is unique
    if (r_ptr->flags1 & RF1_UNIQUE)
    {
        // Calculate bonus using the same formula as normal bane
        int uniques_killed = unique_bane_type_killed();
        
        // Use same scaling as bane_bonus_aux: 1, 2, 4, 8, 16, etc.
        int threshold = 2;
        bonus = 0;
        while (threshold <= uniques_killed)
        {
            threshold *= 2;
            bonus++;
        }
    }

    return (bonus);
}

/* Calculate total unique monsters killed for unique bane */
int unique_bane_type_killed(void)
{
    int uniques_killed = 0;
    int i;
    
    // Count all unique monsters that have been killed
    for (i = 1; i < z_info->r_max; i++) {
        monster_race* check_r_ptr = &r_info[i];
        
        // Skip if not unique
        if (!(check_r_ptr->flags1 & RF1_UNIQUE)) continue;
        
        // Check if this unique has been killed (max_num is set to 0 when killed)
        if (check_r_ptr->max_num == 0) {
            uniques_killed++;
        }
    }
    
    return uniques_killed;
}

int bane_menu(int* highlight)
{
    int i;
    int ch;
    int options = BANE_TYPES - 1;
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    bool compact_layout = ability_menu_use_compact_layout();
    int list_col = compact_layout ? ability_menu_list_col()
                                  : ability_menu_description_col();
    int desc_col = compact_layout ? ability_menu_description_col()
                                  : list_col;
    int prefix_col = indexed_menu_prefix_col(list_col);
    int list_first_row = 4;
    int nav_row_1 = MAX(0, term_hgt - 2);
    int nav_row_2 = MAX(0, term_hgt - 1);
    bool steamdeck = steamdeck_controls_active();

    char buf[80];
    char prefix[8];

    byte attr;

    wipe_screen_from(prefix_col);

    Term_putstr(list_col, 2, -1, TERM_WHITE, "Enemy types");

    // list the enemies
    for (i = 1; i < BANE_TYPES; i++)
    {
        int row = list_first_row + i - 1;
        int k = bane_type_killed(i);

        // Determine the appropriate colour
        if (k >= 4)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
        }

        indexed_menu_entry_label(buf, sizeof(buf), i - 1, bane_name[i]);
        Term_putstr(list_col, row, -1, attr, buf);

        indexed_menu_normal_prefix(prefix, sizeof(prefix), i - 1);
        Term_putstr(prefix_col, row, -1, attr, prefix);

        if (*highlight == i)
        {
            // highlight the label
            indexed_menu_focus_prefix(prefix, sizeof(prefix), i - 1);
            Term_putstr(prefix_col, row, -1, TERM_L_BLUE, prefix);
        }
    }

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > options)
        *highlight = options;

    if (*highlight >= 1 && *highlight <= options)
    {
        int k = bane_type_killed(*highlight);
        int old_wrap = text_out_wrap;
        int old_indent = text_out_indent;
        int detail_row = compact_layout ? 4 : (BANE_TYPES + 4);
        byte detail_attr = (k >= 4) ? TERM_SLATE : TERM_L_DARK;

        if (compact_layout)
        {
            wipe_screen_from(desc_col);
            Term_putstr(desc_col, 2, -1, TERM_WHITE, "Enemy Details");
            Term_putstr(desc_col, detail_row, term_wid - desc_col,
                (k >= 4) ? TERM_SLATE : TERM_L_DARK,
                bane_name[*highlight]);
            detail_row += 2;
        }

        text_out_wrap = ability_menu_description_wrap(desc_col);
        text_out_indent = desc_col;

        Term_gotoxy(text_out_indent, detail_row);

        if (k >= 4)
        {
            strnfmt(buf, 80, "You have slain %d of these foes.", k);
        }
        else
        {
            strnfmt(buf, 80,
                "You have slain %d of these foes, and need to slay %d more.",
                k, 4 - k);
        }
        text_out_to_screen(detail_attr, buf);

        text_out_wrap = old_wrap;
        text_out_indent = old_indent;
    }

    if (compact_layout)
    {
        Term_putstr(desc_col, nav_row_1, term_wid - desc_col, TERM_SLATE,
            "8/2 - Navigate");
        Term_putstr(desc_col, nav_row_2, term_wid - desc_col, TERM_SLATE,
            "Enter Select  Esc Back");
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(prefix_col, list_first_row + *highlight - 1);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if (!steamdeck && (ch >= 'a') && (ch <= (char)'a' + options - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        bane_menu(highlight);

        return (*highlight);
    }

    if (!steamdeck && (ch >= 'A') && (ch <= (char)'A' + options - 1))
    {
        *highlight = (int)ch - 'A' + 1;
        return (*highlight);
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        return (BANE_TYPES + 1);
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

#define OATH_TYPES 6

static u32b oath_flag[] = { 0L, OATH_MERCY_FLAG, OATH_SILENCE_FLAG, OATH_IRON_FLAG, OATH_SMITH_FLAG, OATH_VALOROUS_FLAG, OATH_LIGHT_FLAG };

char* oath_name[] = {
    "Nothing",
    "Mercy",
    "Silence",
    "Iron",
    "Smith",
    "Valorous Heart",
    "Light",
};

char* oath_desc1[] = {
    "Nothing",
    "to leave Angband without shedding blood of Man or Elf",
    "to leave Angband as you came, grim and silent",
    "that none will daunt you from facing Morgoth forthwith",
    "to craft all blades and armour by thine own hand",
    "to face your enemy while it has the heart to fight",
    "to bear the light of the stars and refuse all shadowed gear",
};

char* oath_desc2[] = {
    "Nothing",
    "attack Men or Elves",
    "sing",
    "go up stairs without a Silmaril",
    "pick up weapons or armour from the ground",
    "attack or deal damage to enemies that are fleeing in terror",
    "wear items that dim or shroud your light",
};

char* oath_reward[] = {
    "Nothing",
    "+1 Grace",
    "+1 Strength",
    "+2 Constitution",
    "+5 Smithing",
    "+1 Dexterity",
    "+1 Light Radius",
};

static const char* oath_name_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_name)) return "Unknown";
    return oath_name[oath_id];
}

static const char* oath_desc2_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_desc2)) return "";
    return oath_desc2[oath_id];
}

static const char* oath_reward_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_reward)) return "";
    return oath_reward[oath_id];
}

bool oath_invalid(int i)
{
    if (i < 0 || i >= (int)N_ELEMENTS(oath_flag)) return false;
    return ((p_ptr->oaths_broken & oath_flag[i]) > 0);
}

bool chosen_oath(int oath)
{
    return p_ptr->oath_type == oath;
}

/*
 * Helper functions to retrieve oath text from oath_info
 */
char* oath_confirmation_prompt(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].confirmation_prompt) return "";
    return oath_name_text + oath_info[oath_id].confirmation_prompt;
}

char* oath_curse_message(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].curse_message) return "";
    return oath_name_text + oath_info[oath_id].curse_message;
}

char* oath_permanent_message(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].permanent_message) return "";
    return oath_name_text + oath_info[oath_id].permanent_message;
}

char* oath_death_message(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].death_message) return "";
    return oath_name_text + oath_info[oath_id].death_message;
}

char* oath_banned_text(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].banned_text) return "";
    return oath_desc_text + oath_info[oath_id].banned_text;
}

char* oath_name_str(int oath_id)
{
    if (oath_id == 0) return "No oath";
    if (!z_info) return "";
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].name) return "";
    return oath_name_text + oath_info[oath_id].name;
}

char* oath_description(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].text) return "";
    return oath_desc_text + oath_info[oath_id].text;
}

char* oath_pledge(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].pledge_text) return "";
    return oath_name_text + oath_info[oath_id].pledge_text;
}

char* oath_forbidden(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].forbidden_text) return "";
    return oath_name_text + oath_info[oath_id].forbidden_text;
}

char* oath_reward_text(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].reward_text) return "";
    return oath_name_text + oath_info[oath_id].reward_text;
}

static int oath_menu_put_wrapped(int desc_col, int row, byte attr, cptr text)
{
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;

    text_out_wrap = ability_menu_description_wrap(desc_col);
    text_out_indent = desc_col;
    Term_gotoxy(desc_col, row);
    text_out_to_screen(attr, text);

    row = ability_menu_next_row_after_text(desc_col, row);
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;

    return row;
}

int oath_menu(int* highlight)
{
    int i, ch;
    int visible_count = 0;
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    bool compact_layout = ability_menu_use_compact_layout();
    int ability_col = ability_menu_list_col();
    int desc_col = ability_menu_description_col();
    int nav_row_1 = MAX(0, term_hgt - 2);
    int nav_row_2 = MAX(0, term_hgt - 1);
    bool steamdeck = steamdeck_controls_active();
    /* Support up to 16 oaths without realloc. */
    int visible_oaths[16]; // Map display letters to oath indices
    char buf[80];
    byte attr;
    
    /* Tolkien-themed descriptions for better immersion */
    char* oath_tolkien_desc[] = {
        "",
        "\"Let no blood of the Children stain thy blade in these halls of sorrow\"",
        "\"In silence came I, and in silence shall I depart, as befits the wise\"", 
        "\"Though darkness gather and Balrogs rise, I shall not yield nor turn aside\"",
        "\"By mine own hand shall all blades be wrought, and no other's craft shall I bear\"",
        "\"Valor guards the fallen foe; the honorable blade stays when terror takes them\"",
        "\"I will carry unsullied starlight, shunning the shadowed tools that would dim it\""
    };

    // Clear the abilities and description area (following abilities_menu2 pattern)
    wipe_screen_from(ability_col);

    // Title in the abilities column
    Term_putstr(ability_col, 2, -1, TERM_WHITE, "Oaths");

    // Build visible oaths list and display them (1..OATH_TYPES)
    for (i = 1; i <= OATH_TYPES && i < (int)N_ELEMENTS(oath_name); i++)
    {
        if (visible_count >= (int)N_ELEMENTS(visible_oaths)) break;

        // Map this visible oath to its position  
        visible_oaths[visible_count] = i;
        
        // Determine display color based on oath status
        if (oath_invalid(i))
        {
            attr = TERM_L_RED; // Broken oaths in red
        }
        else
        {
            attr = (*highlight == visible_count + 1) ? TERM_L_BLUE : TERM_WHITE;
        }
        
        // Format oath name with status indicator
        indexed_menu_entry_label(buf, sizeof(buf), visible_count,
            oath_name_short(i));
        
        // Display in abilities column with proper spacing
        Term_putstr(ability_col, 4 + visible_count, -1, attr, buf);
        visible_count++;
    }

    // Display detailed description for highlighted oath in description column
    if (*highlight >= 1 && *highlight <= visible_count)
    {
        int oath_idx = visible_oaths[*highlight - 1];
        
        // Clear description area first
        int row = 4;

        wipe_screen_from(desc_col);
        
        // Oath title
        Term_putstr(desc_col, 2, -1, TERM_WHITE, "Oath Details");
        
        if (oath_invalid(oath_idx))
        {
            // Menacing text for broken oaths
            Term_putstr(desc_col, row++, term_wid - desc_col, TERM_L_RED,
                "OATH BROKEN");
            row++;
            row = oath_menu_put_wrapped(desc_col, row, TERM_RED,
                "\"Thy oath lies shattered, thy word worthless as dust.\"");
            row++;
            row = oath_menu_put_wrapped(desc_col, row, TERM_L_RED,
                "\"No Valar shall hear thy voice, no light shall guide thy path.\"");
            row++;
            (void)oath_menu_put_wrapped(desc_col, row, TERM_RED,
                "Forever marked as oathbreaker in this age.");
        }
        else
        {
            // Tolkien-themed quote
            char* quote = (oath_idx < (int)N_ELEMENTS(oath_tolkien_desc)) ? oath_tolkien_desc[oath_idx] : "";

            Term_putstr(desc_col, row++, term_wid - desc_col, TERM_YELLOW,
                "Quote:");
            row = oath_menu_put_wrapped(desc_col, row, TERM_SLATE, quote);
            
            // Oath vow
            Term_putstr(desc_col, row++, term_wid - desc_col, TERM_WHITE,
                "Vow:");
            row = oath_menu_put_wrapped(desc_col, row, TERM_SLATE,
                (oath_idx >= 0 && oath_idx < (int)N_ELEMENTS(oath_desc1))
                    ? oath_desc1[oath_idx]
                    : "");
            
            // Restriction
            if (row < nav_row_1)
            {
                Term_putstr(desc_col, row++, term_wid - desc_col, TERM_L_RED,
                    "Restriction:");
                row = oath_menu_put_wrapped(desc_col, row, TERM_L_RED,
                    oath_desc2_short(oath_idx));
            }
            
            // Reward
            if (row < nav_row_1)
            {
                Term_putstr(desc_col, row++, term_wid - desc_col, TERM_L_GREEN,
                    "Reward:");
                (void)oath_menu_put_wrapped(desc_col, row, TERM_L_GREEN,
                    oath_reward_short(oath_idx));
            }
        }
        
        // Navigation instructions at bottom
        Term_putstr(desc_col, nav_row_1, term_wid - desc_col, TERM_SLATE,
            compact_layout ? "8/2 - Navigate" : "2/8 - Navigate");
        Term_putstr(desc_col, nav_row_2, term_wid - desc_col, TERM_SLATE,
            compact_layout ? "Enter Select  Esc Back"
                           : "Enter - Select  ESC - Back");
    }

    // Ensure highlight is within valid range
    if (*highlight < 1) *highlight = 1;
    if (*highlight > visible_count) *highlight = visible_count;

    /* Flush the prompt */
    Term_fresh();

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    /* Handle letter selection (a-z) for immediate highlighting */
    if (!steamdeck && (ch >= 'a') && (ch < 'a' + visible_count))
    {
        *highlight = (int)ch - 'a' + 1;
        return oath_menu(highlight); // Recursive call to update display
    }

    /* Handle capital letter selection (A-Z) for immediate selection */
    if (!steamdeck && (ch >= 'A') && (ch < 'A' + visible_count))
    {
        *highlight = (int)ch - 'A' + 1;
        return visible_oaths[*highlight - 1]; // Return actual oath index
    }

    /* ESC or 'q' - exit menu */
    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        /* Return a sentinel that's outside valid oath indices */
        return OATH_TYPES + 1;
    }

    /* Enter or Space - select current highlighted oath */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        if (visible_count <= 0) return OATH_TYPES + 1;
        return visible_oaths[*highlight - 1]; // Return actual oath index
    }

    /* Navigation: Up (8) */
    if (ch == '8')
    {
        (*highlight)--;
        if (*highlight < 1) *highlight = visible_count;
    }

    /* Navigation: Down (2) */
    if (ch == '2')
    {
    (*highlight)++;
        if (*highlight > visible_count) *highlight = 1;
    }

    /* Recursive call to continue menu interaction */
    return oath_menu(highlight);
}

int abilities_menu1(int* highlight)
{
    int i;
    int ch;
    int options = S_MAX;
    bool show_special = false;
    bool steamdeck = steamdeck_controls_active();

    // Determine if any special abilities are present (owned or active)
    for (i = 0; i < ABILITIES_MAX; i++) {
        if (p_ptr->have_ability[S_SPC][i]) { 
            show_special = true; 
            break; 
        }
    }
    
    // Debug: Always show special menu for unique bane status
    if (p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE]) {
        show_special = true;
    }
    
    if (!show_special) {
        options = S_MAX - 1; // hide Special category
    }

    char buf[80];

    // Clear the whole screen body so compact-layout submenu rows do not
    // linger when returning from an ability list to the skills list.
    wipe_screen_from(indexed_menu_prefix_col(COL_SKILL));

    // title
    Term_putstr(COL_SKILL, 2, -1, TERM_WHITE, "Skills");

    // list the skills
    for (i = 0; i < options; i++)
    {
        indexed_menu_entry_label(buf, sizeof(buf), i, skill_names_full[i]);

        // Highlight the entire line if selected
        Term_putstr(COL_SKILL, i + 4, -1,
            (*highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(COL_SKILL, 3 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if (!steamdeck && (ch >= 'a') && (ch <= (char)'a' + options - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        // relist the skills
    for (i = 0; i < options; i++)
        {
            indexed_menu_entry_label(buf, sizeof(buf), i, skill_names_full[i]);

            Term_putstr(COL_SKILL, i + 4, -1,
                (*highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);
        }

        return (*highlight);
    }

    if (!steamdeck && (ch >= 'A') && (ch <= (char)'A' + options - 1))
    {
        *highlight = (int)ch - 'A' + 1;

        // relist the skills
    for (i = 0; i < options; i++)
        {
            indexed_menu_entry_label(buf, sizeof(buf), i, skill_names_full[i]);

            Term_putstr(COL_SKILL, i + 4, -1,
                (*highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);
        }

        return (*highlight);
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '\t')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        return (S_MAX + 1);  // Always return S_MAX + 1 to exit, regardless of options
    }

    if (ch == 'i')
    {
        return (S_MAX + 2);
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

int abilities_menu2(int skilltype, int* highlight)
{
    int i;
    bool compact_layout = ability_menu_use_compact_layout();
    bool steamdeck = steamdeck_controls_active();
    int ability_col = ability_menu_list_col();
    int desc_col = ability_menu_description_col();
    int list_first_row = 3;
    int list_rows = (Term && Term->hgt > list_first_row) ? (Term->hgt - list_first_row) : 1;

    ability_type* b_ptr;
    ability_type* visible_entries[ABILITIES_MAX];
    byte visible_attrs[ABILITIES_MAX];

    int ch;
    int visible_count = 0; // Count of actually visible abilities
    int visible_abilities[ABILITIES_MAX]; // Map display letters to ability numbers
    int top_visible = 0;
    int highlight_display_index = -1;

    char buf[80];

    byte attr;

    // In compact layout the abilities list reuses the skills column.
    wipe_screen_from(indexed_menu_prefix_col(
        compact_layout ? COL_SKILL : COL_ABILITY));

    // abilities title with color
    Term_putstr(ability_col, 1, -1, TERM_L_BLUE, "Abilities");

    // For special abilities, we may need to adjust highlight to first visible ability
    int first_visible_ability = -1;

    /* Pre-scan for Special abilities to adjust highlight before display */
    if (skilltype == S_SPC)
    {
        int temp_visible_count = 0;
        int temp_first_visible = -1;
        
        for (i = 0; i < z_info->b_max; i++)
        {
            b_ptr = &b_info[i];
            if (!b_ptr->name || b_ptr->skilltype != skilltype) continue;
            
            if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
            {
                if (temp_first_visible == -1)
                {
                    temp_first_visible = b_ptr->abilitynum;
                }
                temp_visible_count++;
            }
        }
        
        /* Adjust highlight before display if needed */
        if (temp_visible_count > 0 && temp_first_visible != -1)
        {
            /* Check if current highlight corresponds to a visible ability */
            int current_ability_num = *highlight - 1; /* Convert 1-based to 0-based */
            bool highlight_is_visible = false;
            
            for (i = 0; i < z_info->b_max; i++)
            {
                b_ptr = &b_info[i];
                if (!b_ptr->name || b_ptr->skilltype != skilltype) continue;
                
                if (b_ptr->abilitynum == current_ability_num && p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                {
                    highlight_is_visible = true;
                    break;
                }
            }
            
            if (!highlight_is_visible)
            {
                *highlight = temp_first_visible + 1; /* Convert back to 1-based */
            }
        }
    }

    // list the abilities
    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skilltype)
            continue;

        /* For special abilities, only show granted abilities */
        if (skilltype == S_SPC && !p_ptr->have_ability[skilltype][b_ptr->abilitynum])
        {
            continue;
        }

        /* Hide deprecated WIL_OATH ability from menu (now handled at birth) */
        if (skilltype == S_WIL && b_ptr->abilitynum == WIL_OATH)
            continue;

        // Safety check for ability number bounds
        if (b_ptr->abilitynum >= ABILITIES_MAX) {
            continue;
        }

        // Safety check for array bounds
        if (visible_count >= ABILITIES_MAX) {
            break;
        }

        /* Determine the appropriate colour. */
        if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
        {
            if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            {
                if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                    attr = TERM_WHITE;
                else
                    attr = TERM_RED;
            }
            else
            {
                if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                    attr = TERM_L_GREEN;
                else
                    attr = TERM_RED;
            }
        }
        else if (prereqs(skilltype, b_ptr->abilitynum))
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
        }

        visible_entries[visible_count] = b_ptr;
        visible_attrs[visible_count] = attr;

        // Map this visible ability to its position
        visible_abilities[visible_count] = b_ptr->abilitynum;
        
        // Track first visible ability for highlight adjustment
        if (first_visible_ability == -1) {
            first_visible_ability = b_ptr->abilitynum;
        }

        visible_count++;
    }

    /* Safety check: if no abilities are visible, show message and exit */
    if (visible_count == 0) {
        Term_putstr(ability_col, 4, -1, TERM_L_DARK, "No abilities available for this skill.");
        Term_fresh();
        inkey(); /* Wait for keypress */
        return (ABILITIES_MAX + 1); /* Return to skills menu */
    }

    for (i = 0; i < visible_count; i++)
    {
        if (visible_abilities[i] == *highlight - 1)
        {
            highlight_display_index = i;
            break;
        }
    }

    if (highlight_display_index < 0)
        highlight_display_index = 0;

    if (list_rows < 1)
        list_rows = 1;

    if (highlight_display_index < top_visible)
        top_visible = highlight_display_index;
    if (highlight_display_index >= top_visible + list_rows)
        top_visible = highlight_display_index - list_rows + 1;
    if (top_visible < 0)
        top_visible = 0;
    if (top_visible > visible_count - list_rows)
        top_visible = visible_count - list_rows;
    if (top_visible < 0)
        top_visible = 0;

    if (visible_count > list_rows)
    {
        strnfmt(buf, sizeof(buf), "[%d-%d/%d]", top_visible + 1,
            MIN(top_visible + list_rows, visible_count), visible_count);
        Term_putstr(ability_col, 2, -1, TERM_SLATE, buf);
    }

    for (i = top_visible; i < visible_count && i < top_visible + list_rows; i++)
    {
        int display_row = list_first_row + (i - top_visible);

        b_ptr = visible_entries[i];
        attr = visible_attrs[i];

        if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
            && (p_ptr->bane_type > 0))
        {
            char name_buf[80];
            strnfmt(name_buf, sizeof(name_buf), "%s-%s",
                bane_name[p_ptr->bane_type], (b_name + b_ptr->name));
            indexed_menu_entry_label(buf, sizeof(buf), i, name_buf);
        }
        else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
            && (p_ptr->oath_type > 0))
        {
            char name_buf[80];
            strnfmt(name_buf, sizeof(name_buf), "%s: %s",
                (b_name + b_ptr->name), oath_name_short(p_ptr->oath_type));
            indexed_menu_entry_label(buf, sizeof(buf), i, name_buf);
        }
        else
        {
            indexed_menu_entry_label(buf, sizeof(buf), i, (b_name + b_ptr->name));
        }

        Term_putstr(ability_col, display_row, -1, attr, buf);

        if (*highlight == b_ptr->abilitynum + 1)
        {
            /* Highlight the label with bright blue */
            indexed_menu_focus_prefix(buf, sizeof(buf), i);
            Term_putstr(indexed_menu_prefix_col(ability_col), display_row, -1,
                TERM_L_BLUE, buf);

            /* Print the description of the highlighted ability. */
            /* (ability_type::text is an offset, so it's always non-negative) */
            /* Determine compact mode from terminal height; use single newline between
             * sections when space is tight, double newline when there is room. */
            int term_hgt_ab = Term ? Term->hgt : 24;
            bool compact_mode = (term_hgt_ab < 28);
            const char *desc_sep = compact_mode ? "\n" : "\n\n";
            int post_desc_row = 3; /* updated after description renders */
            {
                /* Check if this is a broken oath ability and use Q: text instead */
                char* description_text = NULL;
                bool use_death_message = false;
                
                if (skilltype == S_SPC && 
                    (b_ptr->abilitynum == SPC_OATH_MERCY || 
                     b_ptr->abilitynum == SPC_OATH_SILENCE || 
                     b_ptr->abilitynum == SPC_OATH_IRON ||
                     b_ptr->abilitynum == SPC_OATH_SMITH ||
                     b_ptr->abilitynum == SPC_OATH_VALOROUS ||
                     b_ptr->abilitynum == SPC_OATH_LIGHT))
                {
                    /* Check if this oath is broken */
                    int oath_id = 0;
                    if (b_ptr->abilitynum == SPC_OATH_MERCY) oath_id = OATH_MERCY;
                    else if (b_ptr->abilitynum == SPC_OATH_SILENCE) oath_id = OATH_SILENCE;
                    else if (b_ptr->abilitynum == SPC_OATH_IRON) oath_id = OATH_IRON;
                    else if (b_ptr->abilitynum == SPC_OATH_SMITH) oath_id = OATH_SMITH;
                    else if (b_ptr->abilitynum == SPC_OATH_VALOROUS) oath_id = OATH_VALOROUS;
                    else if (b_ptr->abilitynum == SPC_OATH_LIGHT) oath_id = OATH_LIGHT;
                    
                    if (oath_id > 0 && oath_invalid(oath_id))
                    {
                        description_text = oath_death_message(oath_id);
                        use_death_message = true;
                    }
                }
                
                /* Clear description area first */
                wipe_screen_from(desc_col);
                
                /* Display ability name in description area with appropriate color */
                Term_putstr(desc_col, 1, -1, TERM_YELLOW, b_name + b_ptr->name);
                
                /* Wrap to the active terminal width so compact layouts do not overflow. */
                text_out_wrap = ability_menu_description_wrap(desc_col);
                text_out_indent = desc_col;

                /* Description starts at row 3 for more space */
                Term_gotoxy(text_out_indent, 3);
                
                if (use_death_message && description_text && description_text[0])
                {
                    /* Display Q: text in red for broken oaths */
                    text_out_to_screen(TERM_RED, description_text);
                }
                else
                {
                    /* Display ability description based on ability_desc_mode */
                    char desc_controller_text[2048];
                    char effect_controller_text[2048];
                    const char *desc_text = (b_ptr->text)
                        ? ability_menu_controller_text(b_text + b_ptr->text,
                              desc_controller_text, sizeof(desc_controller_text))
                        : NULL;
                    const char *effect_text = (b_ptr->effect)
                        ? ability_menu_controller_text(b_text + b_ptr->effect,
                              effect_controller_text, sizeof(effect_controller_text))
                        : NULL;
                    bool has_desc = desc_text && desc_text[0];
                    bool has_effect = effect_text && effect_text[0];

                    switch (op_ptr->ability_desc_mode)
                    {
                    case 1: /* Effect first, then description */
                        if (has_effect) text_out_to_screen(TERM_L_WHITE, effect_text);
                        if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                        {
                            if (has_effect) text_out_to_screen(TERM_L_WHITE, desc_sep);
                            ability_menu_render_prerequisites_block(skilltype,
                                b_ptr, desc_col);
                        }
                        if (has_desc) {
                            if (has_effect
                                || !p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                                text_out_to_screen(TERM_L_WHITE, desc_sep);
                            text_out_to_screen(TERM_SLATE, desc_text);
                        }
                        break;
                    case 2: /* Effect only */
                        if (has_effect) text_out_to_screen(TERM_L_WHITE, effect_text);
                        else if (has_desc) text_out_to_screen(TERM_L_WHITE, desc_text);
                        if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                        {
                            if (has_effect || has_desc)
                                text_out_to_screen(TERM_L_WHITE, desc_sep);
                            ability_menu_render_prerequisites_block(skilltype,
                                b_ptr, desc_col);
                        }
                        break;
                    default: /* 0: Description first, then effect */
                        if (has_desc) text_out_to_screen(TERM_SLATE, desc_text);
                        if (has_effect) {
                            if (has_desc) text_out_to_screen(TERM_L_WHITE, desc_sep);
                            text_out_to_screen(TERM_L_WHITE, effect_text);
                        }
                        if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                        {
                            if (has_desc || has_effect)
                                text_out_to_screen(TERM_L_WHITE, desc_sep);
                            ability_menu_render_prerequisites_block(skilltype,
                                b_ptr, desc_col);
                        }
                        break;
                    }

                    if (skilltype == S_SNG)
                        ability_menu_render_song_bonus_block(b_ptr);

                    /* For Nienna's Gift of Mercy, show current bonus */
                    if (skilltype == S_SPC && b_ptr->abilitynum == SPC_NIENA_MERCY && 
                        p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY])
                    {
                        /* Calculate current stealth bonus (same logic as in xtra1.c) */
                        int total_monsters_seen = 0;
                        int total_monsters_killed = 0;
                        
                        /* Sum up global monster tracking (excluding uniques) */
                        for (int i = 1; i < z_info->r_max; i++)
                        {
                            monster_lore *l_ptr = &l_list[i];
                            monster_race *r_ptr = &r_info[i];
                            
                            if (r_ptr->flags1 & RF1_UNIQUE) continue;
                            
                            total_monsters_seen += l_ptr->psights;
                            total_monsters_killed += l_ptr->pkills;
                        }
                        
                        if (total_monsters_seen > 0)
                        {
                            /* Calculate stealth bonus: 10*(seen-killed)/seen, rounded up */
                            int mercy_ratio_times_10 = (10 * (total_monsters_seen - total_monsters_killed));
                            int stealth_bonus = (mercy_ratio_times_10 + total_monsters_seen - 1) / total_monsters_seen;
                            
                            char bonus_text[100];
                            strnfmt(bonus_text, sizeof(bonus_text), 
                                   "\n\nCurrent bonus: +%d stealth (%d seen, %d spared)",
                                   stealth_bonus, total_monsters_seen, 
                                   total_monsters_seen - total_monsters_killed);
                            text_out_to_screen(TERM_L_GREEN, bonus_text);
                        }
                        else
                        {
                            text_out_to_screen(TERM_SLATE, "\n\nCurrent bonus: +0 stealth (no monsters encountered yet)");
                        }
                    }

                    if ((skilltype == S_EVN)
                        && (b_ptr->abilitynum == EVN_HEAVY_ARMOUR))
                    {
                        const int armour_weight = heavy_armour_desc_current_weight();
                        const int protection_bonus = armour_weight / 150;
                        const int evasion_bonus =
                            heavy_armour_desc_current_evasion_bonus();
                        const bool learned =
                            p_ptr->have_ability[skilltype][b_ptr->abilitynum];
                        char bonus_text[160];

                        strnfmt(bonus_text, sizeof(bonus_text),
                            learned
                                ? "\n\nCurrent bonus: +%d protection vs physical attacks and %+d evasion (%d.%d lb counted)"
                                : "\n\nWith current equipment, this would grant +%d protection vs physical attacks and %+d evasion (%d.%d lb counted)",
                            protection_bonus, evasion_bonus, armour_weight / 10,
                            armour_weight % 10);
                        text_out_to_screen(TERM_L_GREEN, bonus_text);
                    }
                }

                /* Capture the row where description text ended for dynamic placement */
                {
                    int pdx;
                    Term_locate(&pdx, &post_desc_row);
                    if (pdx > text_out_indent) post_desc_row++;
                }

                /* Reset text_out() vars */
                text_out_wrap = 0;
                text_out_indent = 0;
            }

            // if you have the ability and it is Bane...
            if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
                && (skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
                && (p_ptr->bane_type > 0))
            {
                int killed = bane_type_killed(p_ptr->bane_type);
                int current_bonus = bane_bonus_aux();
                int next_threshold = 2;
                
                // Calculate next threshold using same formula as bane
                int threshold = 2;
                while (threshold <= killed)
                {
                    threshold *= 2;
                }
                next_threshold = threshold;  // This is the next power of 2
                
                /* Place bane stats dynamically after description text */
                int bane_row = post_desc_row + (compact_mode ? 1 : 2);
                Term_putstr(desc_col, bane_row, -1, TERM_WHITE,
                    format("%s-Bane:", bane_name[p_ptr->bane_type]));
                Term_putstr(desc_col, bane_row + 2, -1, TERM_WHITE,
                    format("  %d slain, giving a %+d bonus", killed, current_bonus));
                    
                if (current_bonus == 0 && killed < 2) {
                    Term_putstr(desc_col, bane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d slain)", next_threshold));
                } else if (next_threshold <= 64) {  // Don't show if threshold is too high
                    Term_putstr(desc_col, bane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d slain)", next_threshold));
                }
            }
            else if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
                && (skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
                && (p_ptr->oath_type > 0))
            {
                /* Place oath info dynamically after description text */
                int oath_row = post_desc_row + (compact_mode ? 1 : 2);
                Term_putstr(desc_col, oath_row, -1, TERM_WHITE, "Oath:");
                Term_putstr(desc_col + 6, oath_row, -1, TERM_L_BLUE,
                    oath_name_short(p_ptr->oath_type));

                /* Wrap to the active terminal width here too. */
                text_out_wrap = ability_menu_description_wrap(desc_col);
                text_out_indent = desc_col;

                /* History */
                Term_gotoxy(text_out_indent, oath_row + 1);
                strnfmt(buf, 80, "You have sworn not to %s.",
                    oath_desc2_short(p_ptr->oath_type));
                text_out_to_screen(TERM_L_WHITE, buf);

                /* Reset text_out() vars */
                text_out_wrap = 0;
                text_out_indent = 0;

                if (oath_invalid(p_ptr->oath_type))
                    Term_putstr(desc_col, oath_row + 4, -1, TERM_RED,
                        "You are an oathbreaker.");
                else
                    Term_putstr(desc_col, oath_row + 4, -1, TERM_WHITE,
                        format("Bonus: %s.", oath_reward_short(p_ptr->oath_type)));
            }
            // if you have the unique bane special ability
            else if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
                && (skilltype == S_SPC) && (b_ptr->abilitynum == SPC_UNIQUE_BANE))
            {
                int uniques_killed = unique_bane_type_killed();
                int current_bonus = 0;
                int next_threshold = 2;
                
                // Calculate current bonus using same formula as bane
                int threshold = 2;
                while (threshold <= uniques_killed)
                {
                    threshold *= 2;
                    current_bonus++;
                }
                
                // Calculate next threshold
                if (current_bonus == 0) {
                    next_threshold = 2;
                } else {
                    next_threshold = threshold;  // This is the next power of 2
                }
                
                /* Place unique bane stats dynamically after description text */
                int ubane_row = post_desc_row + (compact_mode ? 1 : 2);
                Term_putstr(desc_col, ubane_row, -1, TERM_WHITE, "Unique Bane:");
                Term_putstr(desc_col, ubane_row + 2, -1, TERM_WHITE,
                    format("  %d uniques slain, giving a %+d bonus", 
                           uniques_killed, current_bonus));
                           
                if (current_bonus == 0 && uniques_killed < 2) {
                    Term_putstr(desc_col, ubane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d uniques)", next_threshold));
                } else if (next_threshold <= 64) {  // Don't show if threshold is too high
                    Term_putstr(desc_col, ubane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d uniques)", next_threshold));
                }
            }
        }

    }
    
    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice - single column layout */
    if (highlight_display_index >= 0)
    {
        int cursor_row = list_first_row + (highlight_display_index - top_visible);
        if (cursor_row >= list_first_row && cursor_row < list_first_row + list_rows)
            Term_gotoxy(ability_col, cursor_row);
    }

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if (!steamdeck && (ch >= 'a') && (ch <= (char)'a' + visible_count - 1))
    {
        int selected_index = (int)ch - 'a';
        /* Bounds check for safety */
        if (selected_index >= 0 && selected_index < visible_count) {
            *highlight = visible_abilities[selected_index] + 1;
            return abilities_menu2(skilltype, highlight);
        }
    }

    if (!steamdeck && (ch >= 'A') && (ch <= (char)'A' + visible_count - 1))
    {
        int selected_index = (int)ch - 'A';
        /* Bounds check for safety */
        if (selected_index >= 0 && selected_index < visible_count) {
            *highlight = visible_abilities[selected_index] + 1;
            return abilities_menu2(skilltype, highlight);
        }
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        return (ABILITIES_MAX + 1);
    }

    if (ch == '\t')
    {
        return (ABILITIES_MAX + 2);
    }

    if (ch == 'i')
    {
        return (ABILITIES_MAX + 3);
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
        /* Only navigate if there are visible abilities */
        if (visible_count > 0) {
            /* Find current visible index */
            int current_visible_index = -1;
            for (int i = 0; i < visible_count; i++) {
                if (visible_abilities[i] + 1 == *highlight) {
                    current_visible_index = i;
                    break;
                }
            }
            
            /* Move to previous visible ability */
            if (current_visible_index > 0) {
                *highlight = visible_abilities[current_visible_index - 1] + 1;
            } else if (current_visible_index == 0) {
                *highlight = visible_abilities[visible_count - 1] + 1;
            } else {
                /* Fallback if not found - go to first visible */
                *highlight = visible_abilities[0] + 1;
            }
        }
    }

    /* Next item */
    if (ch == '2')
    {
        /* Only navigate if there are visible abilities */
        if (visible_count > 0) {
            /* Find current visible index */
            int current_visible_index = -1;
            for (int i = 0; i < visible_count; i++) {
                if (visible_abilities[i] + 1 == *highlight) {
                    current_visible_index = i;
                    break;
                }
            }
            
            /* Move to next visible ability */
            if (current_visible_index >= 0 && current_visible_index < visible_count - 1) {
                *highlight = visible_abilities[current_visible_index + 1] + 1;
            } else if (current_visible_index == visible_count - 1) {
                *highlight = visible_abilities[0] + 1;
            } else {
                /* Fallback if not found - go to first visible */
                *highlight = visible_abilities[0] + 1;
            }
        }
    }

    return (0);
}

/*
 * Hack -- ability screen
 */
void do_cmd_ability_screen(void)
{
    int skilltype = -1;
    int abilitynum = -1;
    int banechoice = -1;
    int oathchoice = -1;

    int highlight1 = 1;
    int highlight2 = 1;
    int highlight3 = 1;

    bool return_to_game = false;
    bool return_to_skills = false;
    bool return_to_abilities = false;

    bool skip_purchase = false;

    log_trace("ABILITY_SCREEN: Entering ability screen");

    /* Save screen */
    screen_save();

    /* Clear screen */
    Term_clear();

    log_trace("ABILITY_SCREEN: Starting main ability loop");

    /* Process Events until "Return to Game" is selected */
    while (!return_to_game)
    {
        int menu1_choice;

        log_trace("ABILITY_SCREEN: Calling abilities_menu1 with highlight1=%d", highlight1);
        menu1_choice = abilities_menu1(&highlight1);

        if (menu1_choice == (S_MAX + 2))
        {
            log_trace("ABILITY_SCREEN: in-menu skill increase requested (menu1)");
            (void)gain_skills();
            p_ptr->redraw |= (PR_EXP | PR_BASIC);
            p_ptr->update |= (PU_BONUS | PU_MANA);
            handle_stuff();
            continue;
        }

        skilltype = menu1_choice - 1;

        log_trace("ABILITY_SCREEN: abilities_menu1 returned skilltype=%d", skilltype);

        // if a skill has been selected...
        if ((skilltype >= 0) && (skilltype < S_MAX))
        {
            log_trace("ABILITY_SCREEN: Valid skill selected (%d), entering abilities loop", skilltype);
            
            /* Reset highlight2 to 1 when entering a new skill category */
            highlight2 = 1;
            
            while (!return_to_skills)
            {
                int menu2_choice;

                log_trace("ABILITY_SCREEN: Calling abilities_menu2 for skilltype=%d with highlight2=%d", skilltype, highlight2);
                menu2_choice = abilities_menu2(skilltype, &highlight2);

                if (menu2_choice == (ABILITIES_MAX + 3))
                {
                    log_trace("ABILITY_SCREEN: in-menu skill increase requested (menu2)");
                    (void)gain_skills();
                    p_ptr->redraw |= (PR_EXP | PR_BASIC);
                    p_ptr->update |= (PU_BONUS | PU_MANA);
                    handle_stuff();
                    continue;
                }

                abilitynum = menu2_choice - 1;

                log_trace("ABILITY_SCREEN: abilities_menu2 returned abilitynum=%d", abilitynum);

                if ((abilitynum >= 0) && (abilitynum < ABILITIES_MAX))
                {
                    if (!p_ptr->have_ability[skilltype][abilitynum])
                    {
                        // Special abilities cannot be purchased
                        if (skilltype == S_SPC) {
                            bell("This special ability cannot be purchased.");
                            continue;
                        }
                        ability_type* b_ptr = &b_info[ability_index(skilltype, abilitynum)];
                        bool has_skill_prereq = (p_ptr->skill_base[skilltype] >= b_ptr->level);
                        bool has_ability_prereq = prereq_abilities_met(b_ptr);

                        if (has_skill_prereq && has_ability_prereq)
                        {
                            int exp_cost = ability_purchase_exp_cost(skilltype);

                            if (exp_cost > p_ptr->new_exp)
                            {
                                bell("You do not have enough experience to "
                                     "acquire this "
                                     "ability.");
                            }
                            else
                            {
                                // special menu for bane
                                if ((skilltype == S_PER)
                                    && (abilitynum == PER_BANE))
                                {
                                    while (!return_to_abilities)
                                    {
                                        skip_purchase = false;

                                        banechoice = bane_menu(&highlight3);

                                        if ((banechoice >= 1)
                                            && (banechoice <= BANE_TYPES))
                                        {
                                            if (bane_type_killed(banechoice)
                                                < 4)
                                            {
                                                return_to_abilities = false;
                                                skip_purchase = true;
                                                bell("Insufficient kills to "
                                                     "become a bane.");
                                            }
                                            else
                                            {
                                                return_to_abilities = true;
                                            }
                                        }
                                        else if (banechoice == BANE_TYPES + 1)
                                        {
                                            return_to_abilities = true;
                                            return_to_skills = true;
                                            return_to_game = true;
                                            skip_purchase = true;
                                        }
                                    }

                                    return_to_abilities = false;
                                }
                                // special menu for Oath //XXX Oaths
                                if ((skilltype == S_WIL)
                                    && (abilitynum == WIL_OATH))
                                {
                                    while (!return_to_abilities)
                                    {
                                        skip_purchase = false;

                                        oathchoice = oath_menu(&highlight3);

                                        if ((oathchoice >= 1)
                                            && (oathchoice <= OATH_TYPES))
                                        {
                                            if (oath_invalid(oathchoice))
                                            {
                                                return_to_abilities = false;
                                                skip_purchase = true;
                                                bell("This oath was broken "
                                                     "before it was made.");
                                            }
                                            else
                                            {
                                                return_to_abilities = true;
                                            }
                                        }
                                        else if (oathchoice == OATH_TYPES + 1)
                                        {
                                            return_to_abilities = true;
                                            return_to_skills = true;
                                            return_to_game = true;
                                            skip_purchase = true;
                                        }
                                    }

                                    return_to_abilities = false;
                                }

                                // Block purchasing Masterpiece if Aule's Forge already owned
                                if (skilltype == S_SMT && abilitynum == SMT_MASTERPIECE && p_ptr->have_ability[S_SPC][SPC_AULE]) {
                                    bell("Aule's Forge supersedes Masterpiece; you cannot purchase it.");
                                    skip_purchase = true;
                                }

                                if (!skip_purchase)
                                {
                                    if (get_check("Gain this ability? "))
                                    {
                                        p_ptr->innate_ability[skilltype]
                                                             [abilitynum]
                                            = true;
                                        p_ptr->have_ability[skilltype]
                                                           [abilitynum]
                                            = true;
                                        p_ptr->active_ability[skilltype]
                                                             [abilitynum]
                                            = true;
                                        ability_log_record_gain(skilltype, abilitynum);
                                        Term_putstr(0, 0, -1, TERM_WHITE,
                                            "Ability gained.");
                                        p_ptr->new_exp -= exp_cost;

                                        if (banechoice <= 0 && oathchoice <= 0)
                                        {
                                            // make a note in the notes file
                                            do_cmd_note(
                                                format("(%s)",
                                                    b_name
                                                        + (&b_info[ability_index(
                                                               skilltype,
                                                               abilitynum)])
                                                              ->name),
                                                p_ptr->depth);
                                        }
                                        else if (oathchoice <= 0)
                                        {
                                            // set the new bane type
                                            p_ptr->bane_type = banechoice;

                                            // and make a note in the notes file
                                            do_cmd_note(
                                                format("(%s-%s)",
                                                    bane_name[banechoice],
                                                    b_name
                                                        + (&b_info[ability_index(
                                                               skilltype,
                                                               abilitynum)])
                                                              ->name),
                                                p_ptr->depth);
                                        }
                                        else
                                        {
                                            // set the new bane type
                                            p_ptr->oath_type = oathchoice;
                                            
                                            /* Activate the matching oath ability */
                                            int oath_special = -1;
                                            switch (oathchoice) {
                                                case OATH_MERCY: oath_special = SPC_OATH_MERCY; break;
                                                case OATH_SILENCE: oath_special = SPC_OATH_SILENCE; break;
                                                case OATH_IRON: oath_special = SPC_OATH_IRON; break;
                                                case OATH_SMITH: oath_special = SPC_OATH_SMITH; break;
                                                case OATH_VALOROUS: oath_special = SPC_OATH_VALOROUS; break;
                                                case OATH_LIGHT: oath_special = SPC_OATH_LIGHT; break;
                                            }
                                            if (oath_special >= 0) {
                                                p_ptr->have_ability[S_SPC][oath_special] = true;
                                                p_ptr->innate_ability[S_SPC][oath_special] = true;
                                                p_ptr->active_ability[S_SPC][oath_special] = true;
                                                ability_log_record_gain(S_SPC, oath_special);
                                            }

                                            // and make a note in the notes file
                                            do_cmd_note(
                                                format("(%s: %s)",
                                                    b_name
                                                        + (&b_info[ability_index(
                                                               skilltype,
                                                               abilitynum)])
                                                              ->name,
                                                    oath_name_short(oathchoice)),
                                                p_ptr->depth);
                                        }

                                        /* Set the redraw flag for everything */
                                        p_ptr->redraw |= (PR_EXP | PR_BASIC);

                                        /* Recalculate bonuses */
                                        p_ptr->update |= (PU_BONUS);
                                        p_ptr->update |= (PU_MANA);
                                    }
                                }
                                skip_purchase = false;
                                banechoice = -1;
                                oathchoice = -1;
                            }
                        }
                        else
                        {
                            if (!has_skill_prereq)
                                bell("Insufficient skill points for ability!");
                            else
                                bell("Insufficient prerequisite abilities for ability!");
                        }
                    }

                    // if you already have the ability...
                    else
                    {
                        // Prevent oath special abilities from being deactivated or reactivated when broken
                        if (skilltype == S_SPC && (abilitynum == SPC_OATH_MERCY || 
                                                   abilitynum == SPC_OATH_SILENCE || 
                                                   abilitynum == SPC_OATH_IRON ||
                                                   abilitynum == SPC_OATH_SMITH ||
                                                   abilitynum == SPC_OATH_VALOROUS ||
                                                   abilitynum == SPC_OATH_LIGHT))
                        {
                            /* Check if oath is broken */
                            bool oath_broken = false;
                            if (abilitynum == SPC_OATH_MERCY && oath_invalid(OATH_MERCY)) oath_broken = true;
                            if (abilitynum == SPC_OATH_SILENCE && oath_invalid(OATH_SILENCE)) oath_broken = true;
                            if (abilitynum == SPC_OATH_IRON && oath_invalid(OATH_IRON)) oath_broken = true;
                            if (abilitynum == SPC_OATH_SMITH && oath_invalid(OATH_SMITH)) oath_broken = true;
                            if (abilitynum == SPC_OATH_VALOROUS && oath_invalid(OATH_VALOROUS)) oath_broken = true;
                            if (abilitynum == SPC_OATH_LIGHT && oath_invalid(OATH_LIGHT)) oath_broken = true;
                            
                            if (p_ptr->active_ability[skilltype][abilitynum])
                            {
                                Term_putstr(0, 0, -1, TERM_WHITE,
                                    "Sacred oaths cannot be deactivated once sworn.");
                            }
                            else if (oath_broken)
                            {
                                Term_putstr(0, 0, -1, TERM_RED,
                                    "Broken oaths cannot be reactivated. They are lost forever.");
                            }
                            else
                            {
                                p_ptr->active_ability[skilltype][abilitynum] = true;
                                Term_putstr(0, 0, -1, TERM_WHITE,
                                    "Oath ability reactivated.");
                            }
                        }
                        else
                        {
                            // toggle its activity for non-oath abilities
                            if (p_ptr->active_ability[skilltype][abilitynum])
                            {
                                p_ptr->active_ability[skilltype][abilitynum]
                                    = false;
                                Term_putstr(0, 0, -1, TERM_WHITE,
                                    "Ability now switched off.");

                                // need to cancel second song in some cases
                                if ((skilltype == S_SNG)
                                    && (abilitynum == SNG_WOVEN_THEMES))
                                {
                                    p_ptr->song2 = SNG_NOTHING;
                                }
                            }
                            else
                            {
                                p_ptr->active_ability[skilltype][abilitynum] = true;
                                Term_putstr(0, 0, -1, TERM_WHITE,
                                    "Ability now switched on. ");
                            }
                        }

                        /* Set the redraw flag for everything */
                        p_ptr->redraw |= (PR_EXP | PR_BASIC);

                        /* Recalculate bonuses */
                        p_ptr->update |= (PU_BONUS);
                        p_ptr->update |= (PU_MANA);
                    }
                }
                else if (abilitynum == ABILITIES_MAX)
                {
                    return_to_skills = true;
                }
                else if (abilitynum == ABILITIES_MAX + 1)
                {
                    return_to_skills = true;
                    return_to_game = true;
                }
            }

            // reset some things for the next time around
            highlight2 = 1;
            return_to_skills = false;
        }
        else if (skilltype >= S_MAX)
        {
            return_to_game = true;
        }
    }

    /* Flush messages */
    // message_flush();

    /* Load screen */
    screen_load();

    handle_stuff();
    inven_enforce_current_pack_limits();
}

bool enchant_then_numbers;

/*
 * A structure to hold a tval and its description
 */
typedef struct smithing_tval_desc
{
    int category;
    int tval;
    cptr desc;
} smithing_tval_desc;

// object being created
object_type smith_o_body;
object_type* smith_o_ptr = &smith_o_body;

// backup object
object_type smith2_o_body;
object_type* smith2_o_ptr = &smith2_o_body;

// super backup object
object_type smith3_o_body;
object_type* smith3_o_ptr = &smith3_o_body;

typedef enum
{
    SMITH_ALLOY_NONE = 0,
    SMITH_ALLOY_MITHRIL,
    SMITH_ALLOY_STAR_IRON,
} smith_alloy_type;

typedef struct
{
    smith_alloy_type type;
    byte bonus_att;
    byte bonus_ds;
    byte bonus_evn;
    byte bonus_ps;
} smith_alloy_state;

static smith_alloy_state smith_alloy;
static smith_alloy_state smith2_alloy;
static smith_alloy_state smith3_alloy;

// artefact being created
#define smith_a_name (z_info->art_self_made_max - 1)
#define smith_a_ptr (&a_info[smith_a_name])

// backup artefact
#define smith2_a_name (z_info->art_self_made_max - 2)
#define smith2_a_ptr (&a_info[smith2_a_name])

/*
 * A structure to hold the costs of smithing something
 */
typedef struct smithing_cost_type
{
    int str;
    int dex;
    int con;
    int gra;
    int exp;
    int smt;
    int mithril;
    int star_iron;
    int alloy_weight;
    int alloy_metal;
    int alloy_mastery;
    int uses;
    int drain;
    int weaponsmith;
    int armoursmith;
    int jeweller;
    int enchantment;
    int artifice;
} smithing_cost_type;

smithing_cost_type smithing_cost;

#define CAT_WEAPON 0
#define CAT_ARMOUR 1
#define CAT_JEWELRY 2

#define MAX_SMITHING_TVALS 17

#define SMT_MENU_CREATE 1
#define SMT_MENU_ENCHANT 2
#define SMT_MENU_ARTEFACT 3
#define SMT_MENU_NUMBERS 4
#define SMT_MENU_MELT 5
#define SMT_MENU_REPAIR 6
#define SMT_MENU_ACCEPT 7

#define SMT_MENU_MAX 7

#define SMT_NUM_MENU_I_ATT 1
#define SMT_NUM_MENU_D_ATT 2
#define SMT_NUM_MENU_I_DS 3
#define SMT_NUM_MENU_D_DS 4
#define SMT_NUM_MENU_I_EVN 5
#define SMT_NUM_MENU_D_EVN 6
#define SMT_NUM_MENU_I_PS 7
#define SMT_NUM_MENU_D_PS 8
#define SMT_NUM_MENU_I_WGT 9
#define SMT_NUM_MENU_D_WGT 10
#define SMT_NUM_MENU_ALLOY_CYCLE 11
#define SMT_NUM_MENU_ALLOY_CLEAR 12
#define SMT_NUM_MENU_EDIT_BONUSES 13

#define SMT_NUM_MENU_MAX 13

#define COL_SMT1 2
#define COL_SMT2 16
static int smith_ui_last_desc_row = -1;

static int smith_ui_term_wid(void)
{
    return (Term && (Term->wid > 0)) ? Term->wid : 80;
}

static int smith_ui_term_hgt(void)
{
    return (Term && (Term->hgt > 0)) ? Term->hgt : 24;
}

static bool smith_ui_compact_width(void)
{
    return (smith_ui_term_wid() < 72);
}

static bool smith_ui_compact_height(void)
{
    return (smith_ui_term_hgt() <= 18);
}

static int smith_ui_secondary_col(void)
{
    return smith_ui_compact_width() ? COL_SMT2 : 36;
}

static int smith_ui_cost_col(void)
{
    int wid = smith_ui_term_wid();
    int col = wid - (smith_ui_compact_width() ? 15 : 18);
    int min_col = smith_ui_secondary_col() + 14;

    if (col < min_col)
        col = min_col;
    if (col < 32)
        col = 32;
    if (col > wid - 1)
        col = wid - 1;

    return col;
}

static int smith_ui_dense_row0(void)
{
    return smith_ui_compact_height() ? 1 : 2;
}

static int smith_ui_dense_row(int index0)
{
    return smith_ui_dense_row0() + index0;
}

static int smith_ui_dense_highlight_row(int highlight)
{
    return smith_ui_dense_row0() + highlight - 1;
}

static int smith_ui_cost_title_row(void)
{
    return smith_ui_compact_height() ? 6 : 8;
}

static int smith_ui_cost_item_row(int index0)
{
    return smith_ui_cost_title_row() + 2 + index0;
}

static int smith_ui_desc_col(void)
{
    return COL_SMT1;
}

static bool smith_ui_show_lore(void)
{
    return (smith_ui_term_hgt() > 18);
}

static int smith_ui_preferred_desc_lines(void)
{
    int hgt = smith_ui_term_hgt();

    if (hgt <= 18)
        return 2;
    if (hgt <= 20)
        return 3;
    if (hgt <= 22)
        return 4;

    return 5;
}

static void smith_ui_reset_description_state(void)
{
    smith_ui_last_desc_row = -1;
}

static void smith_ui_clear_from_row(int row)
{
    int wid = smith_ui_term_wid();
    int hgt = smith_ui_term_hgt();

    if (row < 0)
        row = 0;
    if (row >= hgt)
        return;

    for (int y = row; y < hgt; y++)
        Term_erase(0, y, wid);
}

static int smith_ui_used_bottom_row(void)
{
    if (!Term || !Term->scr)
        return 0;

    for (int y = smith_ui_term_hgt() - 1; y >= 0; y--)
    {
        for (int x = 0; x < smith_ui_term_wid(); x++)
        {
            if ((Term->scr->c[y][x] != ' ')
                || (Term->scr->a[y][x] != Term->attr_blank)
                || (Term->scr->story[y][x] != 0))
            {
                return y;
            }
        }
    }

    return 0;
}

static int smith_ui_description_row(void)
{
    int hgt = smith_ui_term_hgt();
    int row = MAX(
        smith_ui_used_bottom_row() + 1, hgt - smith_ui_preferred_desc_lines());
    int min_lines = smith_ui_show_lore() ? 2 : 1;

    if ((row >= hgt) || ((hgt - row) < min_lines))
        return -1;

    return row;
}

static int smith_ui_weight_col(void)
{
    int col = smith_ui_cost_col() - 10;

    if (col <= COL_SMT2 + 16)
        return -1;

    return col;
}

static void smith_ui_put_cost_line(int index0, byte attr, cptr text)
{
    Term_putstr(smith_ui_cost_col() + 2, smith_ui_cost_item_row(index0), -1,
        attr, text);
}

#define COL_SMT3 (smith_ui_secondary_col())
#define COL_SMT4 (smith_ui_cost_col())

static void smith_ui_put_menu_label(int col, int row, byte attr, cptr label)
{
    if (!indexed_menu_letters_enabled())
        Term_putstr(indexed_menu_prefix_col(col), row, -1, attr, "  ");

    Term_putstr(col, row, -1, attr, label);
}

static void smith_ui_put_menu_prefix(
    int col, int row, int index, byte attr, bool focused)
{
    char buf[80];

    if (focused)
        indexed_menu_focus_prefix(buf, sizeof(buf), index);
    else
        indexed_menu_normal_prefix(buf, sizeof(buf), index);

    Term_putstr(indexed_menu_prefix_col(col), row, -1, attr, buf);
}

/*
 * A list of tvals and their textual names
 */
static const smithing_tval_desc smithing_tvals[MAX_SMITHING_TVALS] = {
    { CAT_WEAPON, TV_SWORD, "Sword" },
    { CAT_WEAPON, TV_POLEARM, "Axe or Polearm" },
    { CAT_WEAPON, TV_HAFTED, "Blunt Weapon" },
    { CAT_WEAPON, TV_DIGGING, "Digger" },
    { CAT_WEAPON, TV_BOW, "Bow" },
    { CAT_WEAPON, TV_ARROW, "Arrows" },
    { CAT_JEWELRY, TV_RING, "Ring" },
    { CAT_JEWELRY, TV_AMULET, "Amulet" },
    { CAT_JEWELRY, TV_LIGHT, "Light" },
    { CAT_JEWELRY, TV_HORN, "Horn" },
    { CAT_ARMOUR, TV_SOFT_ARMOR, "Soft Armour" },
    { CAT_ARMOUR, TV_MAIL, "Mail" },
    { CAT_ARMOUR, TV_CLOAK, "Cloak" },
    { CAT_ARMOUR, TV_SHIELD, "Shield" },
    { CAT_ARMOUR, TV_HELM, "Helm" },
    { CAT_ARMOUR, TV_GLOVES, "Gloves" },
    { CAT_ARMOUR, TV_BOOTS, "Boots" },
};

static bool object_has_evil_alignment(const object_type* o_ptr);

static void smith_clear_alloy_state(smith_alloy_state* state)
{
    state->type = SMITH_ALLOY_NONE;
    state->bonus_att = 0;
    state->bonus_ds = 0;
    state->bonus_evn = 0;
    state->bonus_ps = 0;
}

static void smith_remove_alloy_bonus(object_type* o_ptr, smith_alloy_state* state)
{
    if (!state)
        return;

    if (state->type != SMITH_ALLOY_NONE && o_ptr && o_ptr->k_idx)
    {
        o_ptr->att -= state->bonus_att;
        if (o_ptr->ds >= state->bonus_ds)
            o_ptr->ds -= state->bonus_ds;
        else
            o_ptr->ds = 0;
        o_ptr->evn -= state->bonus_evn;
        if (o_ptr->ps >= state->bonus_ps)
            o_ptr->ps -= state->bonus_ps;
        else
            o_ptr->ps = 0;
    }

    smith_clear_alloy_state(state);
}

static int smith_item_category(const object_type* o_ptr)
{
    if (!o_ptr)
        return -1;

    for (int i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        if (smithing_tvals[i].tval == o_ptr->tval)
            return smithing_tvals[i].category;
    }

    return -1;
}

static bool smith_alloy_applicable(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (object_has_evil_alignment(o_ptr))
        return false;

    int cat = smith_item_category(o_ptr);
    if ((cat != CAT_WEAPON) && (cat != CAT_ARMOUR))
        return false;

    /* Cannot alloy items that are already made of special metal */
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    if (k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
        return false;

    /* Armour: only major metal pieces */
    if (cat == CAT_ARMOUR)
    {
        switch (o_ptr->tval)
        {
        case TV_MAIL:
        case TV_SHIELD:
        case TV_HELM:
            return true;
        default:
            return false;
        }
    }

    /* Weapons: exclude quarterstaves (wooden) */
    if ((o_ptr->tval == TV_HAFTED) && (o_ptr->sval == SV_QUARTERSTAFF))
        return false;

    return true;
}

static bool smith_apply_alloy(object_type* o_ptr, smith_alloy_state* state, smith_alloy_type new_type)
{
    if (!o_ptr || !state)
        return false;

    smith_remove_alloy_bonus(o_ptr, state);

    if (new_type == SMITH_ALLOY_NONE)
        return true;

    if (!smith_alloy_applicable(o_ptr))
        return false;

    int cat = smith_item_category(o_ptr);
    if (cat == CAT_WEAPON)
    {
        if (new_type == SMITH_ALLOY_MITHRIL)
            state->bonus_att = 1;
        else if (new_type == SMITH_ALLOY_STAR_IRON)
            state->bonus_ds = 1;
    }
    else if (cat == CAT_ARMOUR)
    {
        if (new_type == SMITH_ALLOY_MITHRIL)
            state->bonus_evn = 1;
        else if (new_type == SMITH_ALLOY_STAR_IRON)
            state->bonus_ps = 1;
    }
    else
    {
        return false;
    }

    o_ptr->att += state->bonus_att;
    o_ptr->ds += state->bonus_ds;
    o_ptr->evn += state->bonus_evn;
    o_ptr->ps += state->bonus_ps;
    state->type = new_type;
    return true;
}

static int smith_alloy_weight_required(const object_type* o_ptr)
{
    int total_weight = o_ptr->weight * ((o_ptr->number > 0) ? o_ptr->number : 1);
    return (total_weight + 3) / 4;
}

/*
 * A structure to hold a flag and its smithing category
 */
typedef struct smithing_flag_cat
{
    int category;
    cptr desc;
} smithing_flag_cat;

#define CAT_STAT 1
#define CAT_SUST 2
#define CAT_SKILL 3
#define CAT_MEL 4
#define CAT_SLAY 5
#define CAT_RES 6
#define CAT_MISC 7

#define MAX_CATS 7

#define MAX_SMITHING_FLAGS (32 * 4)

static const smithing_flag_cat smithing_flag_cats[]
    = { { CAT_STAT, "Stat bonuses" }, { CAT_SUST, "Sustains" },
          { CAT_SKILL, "Skill bonuses" }, { CAT_MEL, "Melee powers" },
          { CAT_SLAY, "Slays" }, { CAT_RES, "Resistances" },
          { CAT_MISC, "Misc" } };

/*
 * A structure to hold a flag and its smithing category
 */
typedef struct smithing_flag_desc
{
    int category;
    u32b flag;
    int flagset;
    cptr desc;
} smithing_flag_desc;

/*
 * A list of tvals and their textual names
 */
static const smithing_flag_desc smithing_flag_types[] = { { CAT_STAT, TR1_STR,
                                                              1, "Str bonus" },
    { CAT_STAT, TR1_DEX, 1, "Dex bonus" },
    { CAT_STAT, TR1_CON, 1, "Con bonus" },
    { CAT_STAT, TR1_GRA, 1, "Gra bonus" },
    { CAT_STAT, TR1_NEG_STR, 1, "Str penalty" },
    { CAT_STAT, TR1_NEG_DEX, 1, "Dex penalty" },
    { CAT_STAT, TR1_NEG_CON, 1, "Con penalty" },
    { CAT_STAT, TR1_NEG_GRA, 1, "Gra penalty" },
    { CAT_SKILL, TR1_ARC, 1, "Archery" }, { CAT_SKILL, TR1_STL, 1, "Stealth" },
    { CAT_SKILL, TR1_PER, 1, "Perception" }, { CAT_SKILL, TR1_WIL, 1, "Will" },
    { CAT_SKILL, TR1_SMT, 1, "Smithing" }, { CAT_SKILL, TR1_SNG, 1, "Song" },
    { CAT_MISC, TR1_DAMAGE_SIDES, 1, "Damage bonus" },
    { CAT_MISC, TR2_LIGHT, 2, "Light" },
    { CAT_MISC, TR2_SLOW_DIGEST, 2, "Sustenance" },
    { CAT_MISC, TR2_REGEN, 2, "Regeneration" },
    { CAT_MISC, TR2_SEE_INVIS, 2, "See Invisible" },
    { CAT_MISC, TR2_FREE_ACT, 2, "Free Action" },
    { CAT_MISC, TR2_SPEED, 2, "Speed" },
    { CAT_MISC, TR2_RADIANCE, 2, "Radiance" },
    { CAT_MISC, TR3_CHEAT_DEATH, 3, "Cheat Death" },
    { CAT_MISC, TR3_STAND_FAST, 3, "Stand Fast" },
    { CAT_MISC, TR3_AVOID_TRAPS, 3, "Avoid Traps" },
    { CAT_MISC, TR3_MEDIC, 3, "Medicine Bonus" },
    { CAT_MISC, TR4_PROT_FIRE, 4, "Protection vs Fire" },
    { CAT_MISC, TR4_PROT_COLD, 4, "Protection vs Cold" },
    { CAT_MISC, TR4_PROT_POIS, 4, "Protection vs Poison" },
    { CAT_MISC, TR4_PROT_DARK, 4, "Protection vs Darkness" },
    { CAT_MEL, TR1_TUNNEL, 1, "Tunneling Bonus" },
    { CAT_MEL, TR1_SHARPNESS, 1, "Sharpness" },
    { CAT_MEL, TR1_SHARPNESS2, 1, "Sharpness2" },
    { CAT_MEL, TR1_VAMPIRIC, 1, "Vampiric" },
    { CAT_MEL, TR3_ACCURATE, 3, "Accurate" },
    { CAT_SLAY, TR1_SLAY_ORC, 1, "Slay Orc" },
    { CAT_SLAY, TR1_SLAY_TROLL, 1, "Slay Troll" },
    { CAT_SLAY, TR1_SLAY_WOLF, 1, "Slay Wolf" },
    { CAT_SLAY, TR1_SLAY_SPIDER, 1, "Slay Spider" },
    { CAT_SLAY, TR1_SLAY_UNDEAD, 1, "Slay Undead" },
    { CAT_SLAY, TR1_SLAY_RAUKO, 1, "Slay Rauko" },
    { CAT_SLAY, TR1_SLAY_DRAGON, 1, "Slay Dragon" },
    { CAT_SLAY, TR4_SLAY_SERPENT, 4, "Slay Serpent" },
    { CAT_SLAY, TR4_SLAY_VAMPIRE, 4, "Slay Vampire" },
    { CAT_SLAY, TR4_SLAY_HORROR, 4, "Slay Horror" },
    { CAT_SLAY, TR4_SLAY_CAT, 4, "Slay Cat" },
    { CAT_SLAY, TR4_SLAY_GIANT, 4, "Slay Giant" },
    { CAT_SLAY, TR1_BRAND_COLD, 1, "Brand with Cold" },
    { CAT_SLAY, TR1_BRAND_FIRE, 1, "Brand with Fire" },
    { CAT_SLAY, TR1_BRAND_POIS, 1, "Brand with Poison" },
    { CAT_SUST, TR2_SUST_STR, 2, "Sustain Str" },
    { CAT_SUST, TR2_SUST_DEX, 2, "Sustain Dex" },
    { CAT_SUST, TR2_SUST_CON, 2, "Sustain Con" },
    { CAT_SUST, TR2_SUST_GRA, 2, "Sustain Gra" },
    { CAT_RES, TR2_RES_COLD, 2, "Resist Cold" },
    { CAT_RES, TR2_RES_FIRE, 2, "Resist Fire" },
    { CAT_RES, TR2_RES_POIS, 2, "Resist Poison" },
    { CAT_RES, TR2_RES_BLEED, 2, "Resist Bleeding" },
    { CAT_RES, TR2_RES_FEAR, 2, "Resist Fear" },
    { CAT_RES, TR2_RES_BLIND, 2, "Resist Blindness" },
    { CAT_RES, TR2_RES_CONFU, 2, "Resist Confusion" },
    { CAT_RES, TR2_RES_STUN, 2, "Resist Stunning" },
    { CAT_RES, TR2_RES_HALLU, 2, "Resist Hallucination" }, { 0, 0, 0, "" } };

/*
 * Artifice (custom artefact) bonus limits.
 *
 * When smithing a custom artefact, the item's max values from the R: line
 * are extended by these per-category bonuses.  All artefact-specific limits
 * live in this single table so they are easy to find and tune.
 *
 * 'bonus' fields are ADDED to the normal max (e.g. weapon att = max_att + 4).
 * 'floor' fields set a MINIMUM artefact max (e.g. rings always reach att 4).
 * The result is: artefact_max = max(normal_max + ego + bonus, floor).
 */

/* Forward declarations for data-driven smithing limit functions */
static void smithing_ego_bonus_sums(const object_type* o_ptr,
    int* max_att_sum, int* max_att_min_inc,
    int* to_ds_sum, int* to_ds_min_inc,
    int* max_evn_sum, int* max_evn_min_inc,
    int* to_ps_sum, int* to_ps_min_inc,
    int* max_pval_sum, int* max_pval_min_inc);
int att_max(void);
int att_min(void);
int ds_max(void);
int ds_min(void);
int evn_max(void);
int evn_min(void);
int ps_max(void);
int ps_min(void);

typedef struct
{
    int att_bonus;
    int att_floor;   /* 0 = unused */
    int ds_bonus;
    int evn_bonus;
    int evn_floor;   /* 0 = unused */
    int ps_bonus;
    int ps_floor;    /* 0 = unused */
    int pval_bonus;
} artifice_limits_t;

/* Indexed by a small enum - looked up via artifice_bonus_for(). */
enum {
    ARTIFICE_ARROW,
    ARTIFICE_MELEE,     /* sword, polearm, hafted */
    ARTIFICE_BOW,
    ARTIFICE_DIGGING,
    ARTIFICE_ARMOR,
    ARTIFICE_GLOVES,
    ARTIFICE_RING,
    ARTIFICE_AMULET,
    ARTIFICE_DEFAULT,
    ARTIFICE_MAX
};

static const artifice_limits_t artifice_table[ARTIFICE_MAX] = {
    /*               att_b att_f ds_b evn_b evn_f ps_b ps_f pval_b */
    /* ARROW   */  {  8,    0,    0,   0,    0,    0,   0,   0  },
    /* MELEE   */  {  4,    0,    2,   1,    0,    0,   0,   4  },
    /* BOW     */  {  4,    0,    2,   0,    0,    0,   0,   4  },
    /* DIGGING */  {  4,    0,    2,   0,    0,    0,   0,   4  },
    /* ARMOR   */  {  1,    0,    0,   1,    0,    2,   0,   4  },
    /* GLOVES  */  {  2,    0,    0,   1,    0,    2,   0,   4  },
    /* RING    */  {  0,    4,    0,   0,    4,    0,   0,   4  },
    /* AMULET  */  {  0,    0,    0,   0,    0,    0,   3,   4  },
    /* DEFAULT */  {  0,    0,    0,   0,    0,    0,   0,   4  },
};

static int artifice_category(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:      return ARTIFICE_ARROW;
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:     return ARTIFICE_MELEE;
    case TV_BOW:        return ARTIFICE_BOW;
    case TV_DIGGING:    return ARTIFICE_DIGGING;
    case TV_GLOVES:     return ARTIFICE_GLOVES;
    case TV_BOOTS:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:       return ARTIFICE_ARMOR;
    case TV_RING:       return ARTIFICE_RING;
    case TV_AMULET:     return ARTIFICE_AMULET;
    default:            return ARTIFICE_DEFAULT;
    }
}

static const artifice_limits_t* artifice_bonus_for(const object_type* o_ptr)
{
    return &artifice_table[artifice_category(o_ptr)];
}

/*
 * Determines whether the attack bonus of an item is eligible for modification.
 */
int att_valid(void)
{
    return att_max() > att_min();
}

/*
 * Determines the maximum legal attack bonus for an item.
 * Uses data-driven max_att from object.txt R: lines, ego sums,
 * and the artifice table for custom artefacts.
 */
int att_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, &max_att_sum, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int att = k_ptr->max_att;
    att += max_att_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        att += al->att_bonus;
        if (al->att_floor > att)
            att = al->att_floor;
    }

    return (att);
}

/*
 * Determines the minimum legal attack bonus for an item.
 */
int att_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, &max_att_min_inc, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int att = k_ptr->att;
    att += max_att_min_inc;
    return (att);
}

/*
 * Determines whether the damage sides of an item is eligible for modification.
 */
int ds_valid(void)
{
    return ds_max() > ds_min();
}

/*
 * Determines the maximum legal damage sides for an item.
 */
int ds_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, &to_ds_sum, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int ds = k_ptr->max_ds;
    ds += to_ds_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        ds += al->ds_bonus;
    }

    return (ds);
}

/*
 * Determines the minimum legal damage sides for an item.
 */
int ds_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, &to_ds_min_inc, NULL, NULL, NULL, NULL, NULL, NULL);

    int ds = k_ptr->ds;
    ds += to_ds_min_inc;

    /* Never allow weapons to reach 0-sided damage. */
    if (k_ptr->dd > 0 && ds < 1)
        ds = 1;

    return (ds);
}

/*
 * Determines whether the evasion bonus of an item is eligible for modification.
 */
int evn_valid(void)
{
    return evn_max() > evn_min();
}

/*
 * Determines the maximum legal evasion bonus for an item.
 */
int evn_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, &max_evn_sum, NULL, NULL, NULL, NULL, NULL);

    int evn = k_ptr->max_evn;
    evn += max_evn_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        evn += al->evn_bonus;
        if (al->evn_floor > evn)
            evn = al->evn_floor;
    }

    return (evn);
}

/*
 * Determines the minimum legal evasion bonus for an item.
 */
int evn_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, &max_evn_min_inc, NULL, NULL, NULL, NULL);

    int evn = k_ptr->evn;
    evn += max_evn_min_inc;
    return (evn);
}

/*
 * Determines whether the protection sides of an item is eligible for
 * modification.
 */
int ps_valid(void)
{
    return ps_max() > ps_min();
}

/*
 * Determines the maximum legal protection sides for an item.
 */
int ps_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, &to_ps_sum, NULL, NULL, NULL);

    int ps = k_ptr->max_ps;
    ps += to_ps_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        ps += al->ps_bonus;
        if (al->ps_floor > ps)
            ps = al->ps_floor;
    }

    return (ps);
}

/*
 * Determines the minimum legal protection sides for an item.
 */
int ps_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &to_ps_min_inc, NULL, NULL);

    int ps = k_ptr->ps;
    ps += to_ps_min_inc;
    return (ps);
}

static bool smithing_variable_protection_dice(const object_type* o_ptr)
{
    return o_ptr && o_ptr->tval == TV_AMULET
        && ((o_ptr->sval == SV_AMULET_PROTECTION)
            || (o_ptr->name1 && (o_ptr->pd > 0)));
}

typedef struct
{
    byte pd;
    byte ps;
} smithing_protection_combo;

static const smithing_protection_combo smithing_amulet_protection_combos[] = {
    { 1, 1 },
    { 1, 2 },
    { 1, 3 },
    { 2, 1 },
    { 2, 2 },
    { 2, 3 },
};

static int smithing_protection_combo_index(const object_type* o_ptr)
{
    size_t i;

    if (!smithing_variable_protection_dice(o_ptr))
        return -1;

    for (i = 0; i < N_ELEMENTS(smithing_amulet_protection_combos); i++)
    {
        if ((o_ptr->pd == smithing_amulet_protection_combos[i].pd)
            && (o_ptr->ps == smithing_amulet_protection_combos[i].ps))
        {
            return (int)i;
        }
    }

    return -1;
}

static void smithing_set_protection_combo(object_type* o_ptr, int combo_idx)
{
    if (!o_ptr)
        return;

    if (combo_idx < 0 || combo_idx >= (int)N_ELEMENTS(smithing_amulet_protection_combos))
        return;

    o_ptr->pd = smithing_amulet_protection_combos[combo_idx].pd;
    o_ptr->ps = smithing_amulet_protection_combos[combo_idx].ps;
}

static bool smithing_can_increase_protection(const object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return false;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        if (o_ptr->ps < ps_max())
            return true;

        return false;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx >= 0)
        return combo_idx < (int)N_ELEMENTS(smithing_amulet_protection_combos) - 1;

    return (o_ptr->pd <= 1) && (o_ptr->ps < 1);
}

static bool smithing_can_decrease_protection(const object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return false;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        if (o_ptr->ps > ps_min())
            return true;

        return false;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx > 0)
        return true;

    return combo_idx == 0 && ps_min() < 1;
}

static void smithing_increase_protection(object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        o_ptr->ps++;
        return;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx >= 0)
    {
        smithing_set_protection_combo(o_ptr, combo_idx + 1);
        return;
    }

    smithing_set_protection_combo(o_ptr, 0);
}

static void smithing_decrease_protection(object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        o_ptr->ps--;
        return;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx > 0)
    {
        smithing_set_protection_combo(o_ptr, combo_idx - 1);
        return;
    }

    if (combo_idx == 0 && ps_min() < 1)
    {
        o_ptr->pd = 1;
        o_ptr->ps = 0;
    }
}

/*
 * Determines whether the pval of an item is eligible for modification.
 */
int pval_valid(void)
{
    u32b f1, f2, f3;

    object_flags(smith_o_ptr, &f1, &f2, &f3);

    return (f1 & (TR1_PVAL_MASK));
}

/*
 * Determines the maximum legal pval for an item.
 * Uses data-driven max_pval from object.txt R: lines, ego sums,
 * and the artifice table for custom artefacts.
 */
int pval_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    u32b f1, f2, f3;
    int max_pval_sum = 0;
    int max_pval_min_inc = 0;

    object_flags(smith_o_ptr, &f1, &f2, &f3);
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &max_pval_sum, &max_pval_min_inc);

    /* Start with the data-driven max from R: line */
    int pval = k_ptr->max_pval;

    /* Artefact bonus from the centralized artifice table */
    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        pval += al->pval_bonus;
    }

    /* Ego items have pvals limited by their 'special.txt' C: entries. */
    if (cursed_p(smith_o_ptr))
    {
        pval -= max_pval_min_inc;
    }
    else
    {
        pval += max_pval_sum;
    }

    return (pval);
}

/*
 * Determines the minimum legal pval for an item.
 * Accounts for ego min_pval requirements from special.txt C: line.
 */
int pval_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int base_min = k_ptr->pval;

    /* Check both prefix and suffix egos for min_pval requirements */
    byte egos[2] = { object_ego_prefix(smith_o_ptr), object_ego_suffix(smith_o_ptr) };
    for (int i = 0; i < 2; i++)
    {
        byte e_idx = egos[i];
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        if (e_ptr->min_pval > 0)
        {
            /* Ego requires a minimum pval contribution */
            base_min += e_ptr->min_pval;
        }
        else if (e_ptr->max_pval > 0)
        {
            /* Default: at least +1 pval when ego grants pval */
            base_min += 1;
        }
    }

    return base_min;
}

static void smithing_ego_bonus_sums(const object_type* o_ptr,
    int* max_att_sum, int* max_att_min_inc,
    int* to_ds_sum, int* to_ds_min_inc,
    int* max_evn_sum, int* max_evn_min_inc,
    int* to_ps_sum, int* to_ps_min_inc,
    int* max_pval_sum, int* max_pval_min_inc)
{
    if (max_att_sum) *max_att_sum = 0;
    if (max_att_min_inc) *max_att_min_inc = 0;
    if (to_ds_sum) *to_ds_sum = 0;
    if (to_ds_min_inc) *to_ds_min_inc = 0;
    if (max_evn_sum) *max_evn_sum = 0;
    if (max_evn_min_inc) *max_evn_min_inc = 0;
    if (to_ps_sum) *to_ps_sum = 0;
    if (to_ps_min_inc) *to_ps_min_inc = 0;
    if (max_pval_sum) *max_pval_sum = 0;
    if (max_pval_min_inc) *max_pval_min_inc = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return;

    byte egos[2] = { object_ego_prefix(o_ptr), object_ego_suffix(o_ptr) };
    for (int i = 0; i < 2; i++)
    {
        byte e_idx = egos[i];
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        int max_att = (int)(int8_t)e_ptr->max_att;
        int to_ds = (int)(int8_t)e_ptr->to_ds;
        int max_evn = (int)(int8_t)e_ptr->max_evn;
        int to_ps = (int)(int8_t)e_ptr->to_ps;

        if (max_att)
        {
            if (max_att_sum) *max_att_sum += max_att;
            if (max_att_min_inc)
                (*max_att_min_inc) += (max_att > 0) ? 1 : -1;
        }
        if (to_ds)
        {
            if (to_ds_sum) *to_ds_sum += to_ds;
            if (to_ds_min_inc)
                (*to_ds_min_inc) += (to_ds > 0) ? 1 : -1;
        }
        if (max_evn)
        {
            if (max_evn_sum) *max_evn_sum += max_evn;
            if (max_evn_min_inc)
                (*max_evn_min_inc) += (max_evn > 0) ? 1 : -1;
        }
        if (to_ps)
        {
            if (to_ps_sum) *to_ps_sum += to_ps;
            if (to_ps_min_inc)
                (*to_ps_min_inc) += (to_ps > 0) ? 1 : -1;
        }

        if (e_ptr->max_pval > 0)
        {
            if (max_pval_sum) *max_pval_sum += e_ptr->max_pval;
            if (max_pval_min_inc)
                (*max_pval_min_inc) += (e_ptr->min_pval > 0) ? e_ptr->min_pval : 1;
        }
    }
}

/*
 * Determines whether the weight of an item is eligible for modification.
 */
int wgt_valid(void)
{
    switch (smith_o_ptr->tval)
    {
    case TV_ARROW:
    case TV_RING:
    case TV_AMULET:
    case TV_LIGHT:
    case TV_HORN:
    {
        return (false);
    }
    }

    return (true);
}

/*
 * Determines the maximum legal weight for an item.
 */
int wgt_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int weight = div_round(k_ptr->weight, 2) * 3;
    return (weight);
}

/*
 * Determines the minimum legal weight for an item.
 */
int wgt_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int weight = div_round(k_ptr->weight, 3) * 2;
    return (weight);
}

/*
 * Moves the light blue highlighted letter.
 */
void move_displayed_highlight(
    int old_highlight, byte old_attr, int new_highlight, int col)
{
    char buf[80];

    // remove highlight from the old label
    indexed_menu_normal_prefix(buf, sizeof(buf), old_highlight - 1);
    Term_putstr(indexed_menu_prefix_col(col), old_highlight + 1, -1, old_attr,
        buf);

    // highlight the new label
    indexed_menu_focus_prefix(buf, sizeof(buf), new_highlight - 1);
    Term_putstr(indexed_menu_prefix_col(col), new_highlight + 1, -1,
        TERM_L_BLUE, buf);
}

bool melt_metal_item(int item_num)
{
    int number = 0;
    int item, i;
    u32b f1, f2, f3;

    for (item = 0; item < INVEN_TOTAL; item++)
    {
        object_type* o_ptr = &inventory[item];

        object_flags(o_ptr, &f1, &f2, &f3);

        /* Skip metal items that can't be melted (Gamil-forged) */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON)) && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            number += 1;
        }

        if (number == item_num)
        {
            int slots_needed = o_ptr->weight / 99;
            int empty_slots = 0;

            // Equipments needs an extra slot
            if (item >= INVEN_WIELD)
                slots_needed++;

            // Count empty slots
            for (i = INVEN_PACK - 1; i > 0; i--)
            {
                if (!(&inventory[i])->k_idx)
                    empty_slots++;
            }

            if (empty_slots < slots_needed)
            {
                msg_print("You do not have enough room in your pack.");
                if (slots_needed - empty_slots == 1)
                {
                    msg_print("You must free up another slot.");
                }
                else
                {
                    msg_format("You must free up %d more slots.",
                        slots_needed - empty_slots);
                }
                return (false);
            }

            if (get_check("Are you sure you wish to melt this item down? "))
            {
                int slot;
                object_type* i_ptr;
                object_type object_type_body;
                int metal_sval;

                // Determine which metal type to create
                if (f3 & TR3_STAR_IRON)
                    metal_sval = SV_METAL_STAR_IRON;
                else
                    metal_sval = SV_METAL_MITHRIL;

                // Get local object
                i_ptr = &object_type_body;

                // Prepare the base object for the metal
                object_prep(i_ptr, lookup_kind(TV_METAL, metal_sval));

                // set the appropriate quantity
                i_ptr->number = o_ptr->weight;

                // remove the item
                inven_item_increase(item, -1);
                inven_item_describe(item);
                inven_item_optimize(item);
                window_stuff();

                // give the mithril to the player...

                // if there is too much, then break it up
                while (i_ptr->number > 99)
                {
                    object_type* i_ptr2;
                    object_type object_type_body2;

                    // Get local object
                    i_ptr2 = &object_type_body2;

                    // decrease the main stack
                    i_ptr->number -= 99;

                    // Prepare the base object for the metal
                    object_prep(
                        i_ptr2, lookup_kind(TV_METAL, metal_sval));

                    // increase the new stack
                    i_ptr2->number = 99;

                    // give it to the player
                    slot = inven_carry(i_ptr2, true);
                    if ((slot >= 0) && (slot < INVEN_TOTAL))
                    {
                        inven_item_optimize(slot);
                        inven_item_describe(slot);
                    }
                    else
                    {
                        drop_near(i_ptr2, 0, p_ptr->py, p_ptr->px);
                        msg_print("Some metal falls to the floor.");
                    }
                    window_stuff();
                }

                // now give the last stack of mithril to the player
                slot = inven_carry(i_ptr, true);
                if ((slot >= 0) && (slot < INVEN_TOTAL))
                {
                    inven_item_optimize(slot);
                    inven_item_describe(slot);
                }
                else
                {
                    drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
                    msg_print("Some metal falls to the floor.");
                }
                window_stuff();

                return (true);
            }

            else
                return (false);
        }
    }

    return (false);
}

static int meltable_metal_items_carried(void)
{
    int number = 0;
    int item;
    u32b f1, f2, f3;

    for (item = 0; item < INVEN_TOTAL; item++)
    {
        object_type* o_ptr = &inventory[item];

        object_flags(o_ptr, &f1, &f2, &f3);

        /* Only count metal items that can be melted (exclude Gamil-forged) */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            number += 1;
        }
    }

    return (number);
}

static int metal_carried(byte sval)
{
    int w = 0;
    int item;

    for (item = 0; item < INVEN_WIELD; item++)
    {
        object_type* o_ptr = &inventory[item];

        if ((o_ptr->tval == TV_METAL) && (o_ptr->sval == sval))
        {
            w += o_ptr->number;
        }
    }

    return (w);
}

int mithril_carried(void)
{
    return metal_carried(SV_METAL_MITHRIL);
}

int star_iron_carried(void)
{
    return metal_carried(SV_METAL_STAR_IRON);
}

static void use_metal(byte sval, int cost)
{
    int item;

    for (item = INVEN_WIELD - 1; item >= 0 && cost > 0; item--)
    {
        object_type* o_ptr = &inventory[item];

        if ((o_ptr->tval == TV_METAL) && (o_ptr->sval == sval))
        {
            int use = MIN(o_ptr->number, cost);
            inven_item_increase(item, -use);
            inven_item_describe(item);
            inven_item_optimize(item);
            cost -= use;
        }
    }
}

void use_mithril(int cost)
{
    use_metal(SV_METAL_MITHRIL, cost);
}

void use_star_iron(int cost)
{
    use_metal(SV_METAL_STAR_IRON, cost);
}

/*
 * Determines how many uses are left for a given forge.
 */
int forge_uses(int y, int x)
{
    byte feat = cave_feat[y][x];

    if (!cave_forge_bold(y, x))
        return (0);

    if (feat <= FEAT_FORGE_NORMAL_TAIL)
        return (feat - FEAT_FORGE_NORMAL_HEAD);
    if (feat <= FEAT_FORGE_GOOD_TAIL)
        return (feat - FEAT_FORGE_GOOD_HEAD);
    else
        return (feat - FEAT_FORGE_UNIQUE_HEAD);
}

/*
 * Determines how high a bonus is provided by a given forge.
 */
int forge_bonus(int y, int x)
{
    byte feat = cave_feat[y][x];

    if (!cave_forge_bold(y, x))
        return (0);

    if (feat <= FEAT_FORGE_NORMAL_TAIL)
        return (0);
    if (feat <= FEAT_FORGE_GOOD_TAIL)
        return (3);
    else
        return (7);
}

/*
 * Determines the difficulty modifier for pvals.
 *
 * The marginal difficulty of increasing a pval increases by 1 each time, if the
 * base is up to 5, by 2 each time if the base is 6--10, and so on.
 */
void dif_mod(int value, int positive_base, int* dif_inc)
{
    int mod = 1 + ((positive_base - 1) / 5);

    // deal with positive values in a triangular number influenced way
    if (value > 0)
    {
        *dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
    }
}

/*
 * Signed difficulty modifier.
 *
 * Positive values use the normal triangular progression.
 * Negative values reduce difficulty, but only by half as much as the matching
 * positive bonus would increase it.
 */
static int dif_mod_signed(int value, int positive_base)
{
    int mod = 1 + ((positive_base - 1) / 5);

    if (value > 0)
    {
        return positive_base * value + mod * (value * (value - 1) / 2);
    }
    else if (value < 0)
    {
        int abs_value = -value;
        int negative_base = (positive_base + 1) / 2;
        int negative_mod = 1 + ((negative_base - 1) / 5);
        return -(negative_base * abs_value
            + negative_mod * (abs_value * (abs_value - 1) / 2));
    }

    return 0;
}

/*
 * Determines the difficulty of a given object.
 */
int object_difficulty(object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    int x, new, base;
    int i;
    int dif = 0;
    int dif_inc = 0;
    int dif_dec = 0;
    int weight_factor;
    u32b f1, f2, f3, f4;
    int brands = 0;
    int dif_mult = 100;
    int cat = 0; // default to soothe compilation warnings

    bool telchar_bonus = (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR);
    bool feanor_bonus  = (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_FEANOR);

    // reset smithing costs
    smithing_cost.str = 0;
    smithing_cost.dex = 0;
    smithing_cost.con = 0;
    smithing_cost.gra = 0;
    smithing_cost.exp = 0;
    smithing_cost.mithril = 0;
    smithing_cost.star_iron = 0;
    smithing_cost.alloy_weight = 0;
    smithing_cost.alloy_metal = SMITH_ALLOY_NONE;
    smithing_cost.alloy_mastery = 0;
    smithing_cost.uses = 1;
    smithing_cost.drain = 0;
    smithing_cost.weaponsmith = 0;
    smithing_cost.armoursmith = 0;
    smithing_cost.jeweller = 0;
    smithing_cost.enchantment = 0;
    smithing_cost.artifice = 0;

    // extract object flags
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    int att_base = o_ptr->att;
    int evn_base = o_ptr->evn;
    int ds_base = o_ptr->ds;
    int ps_base = o_ptr->ps;

    /* When smithing, ignore the optional alloy bonus for difficulty/costs. */
    if (o_ptr == smith_o_ptr)
    {
        att_base -= smith_alloy.bonus_att;
        evn_base -= smith_alloy.bonus_evn;
        ds_base -= smith_alloy.bonus_ds;
        ps_base -= smith_alloy.bonus_ps;
    }

    /* ------------------------------------------------------------------
     *  GAMIL character bonus
     *   Craft mithril items without mithril material
     *   Costs 3 forge uses instead of 1
     *   Mark item with TR3_CANT_MELT so the melt-menu ignores it
     * ------------------------------------------------------------------ */


    /* Telchar: 25 % discount on Sharpness tiers */
    if (telchar_bonus && (f1 & (TR1_SHARPNESS | TR1_SHARPNESS2) || (f3 & TR3_ACCURATE)))
        dif_mult -= 25;

    /*  FEANOR character bonus
     *   40% off on all lamps
     *   25% off on any fire- or light-branded object */
    if (feanor_bonus)
    {
        /* 40% off on all lamps */
        if (o_ptr->tval == TV_LIGHT)
            dif_mult -= 40;
        /* 25% off on any fire- or light-branded object */
        else if ((f1 & TR1_BRAND_FIRE) || (f2 & (TR2_LIGHT | TR2_RADIANCE)))
            dif_mult -= 25;
    }

    // special rules for horns
    if (o_ptr->tval == TV_HORN)
    {
        dif_inc += k_ptr->level - 1;
        switch (o_ptr->sval)
        {
        case SV_HORN_TERROR:
            smithing_cost.gra += 1;
            break;
        case SV_HORN_THUNDER:
            smithing_cost.dex += 1;
            break;
        case SV_HORN_FORCE:
            smithing_cost.str += 1;
            break;
        case SV_HORN_BLASTING:
            smithing_cost.con += 1;
            break;
            // SV_HORN_WARNING
        }
    }

    // different rules for most other items
    else if (!((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET)))
    {
        // We need to ignore the flags that are basic
        // to the object type and focus on the special/artefact ones. We can do
        // this by subtracting out the basic flags
        f1 &= ~(k_ptr->flags1);
        f2 &= ~(k_ptr->flags2);
        f3 &= ~(k_ptr->flags3);
        f4 &= ~(k_ptr->flags4);

        // need to add tunneling back in...
        if (k_ptr->flags1 & TR1_TUNNEL)
            f1 |= TR1_TUNNEL;

        // need to add stealth back in...
        if (k_ptr->flags1 & TR1_STL)
            f1 |= TR1_STL;

        // need to add accuracy back in...
        if (k_ptr->flags3 & TR3_ACCURATE)
            f3 |= TR3_ACCURATE;

        // need to add sharpness back in...
        if (k_ptr->flags1 & (TR1_SHARPNESS | TR1_SHARPNESS2))
            f1 |= (k_ptr->flags1 & (TR1_SHARPNESS | TR1_SHARPNESS2));

        // need to add mithril-specific flags back in...
        // These are flags that appear on base mithril items but should
        // count toward difficulty as they are "special" properties
        if (k_ptr->flags1 & TR1_DAMAGE_SIDES)
            f1 |= TR1_DAMAGE_SIDES;
        if (k_ptr->flags2 & TR2_REGEN)
            f2 |= TR2_REGEN;
        if (k_ptr->flags2 & TR2_RES_COLD)
            f2 |= TR2_RES_COLD;
        if (k_ptr->flags2 & TR2_RES_FIRE)
            f2 |= TR2_RES_FIRE;
        if (k_ptr->flags3 & TR3_CHEAT_DEATH)
            f3 |= TR3_CHEAT_DEATH;
        if (k_ptr->flags3 & TR3_STAND_FAST)
            f3 |= TR3_STAND_FAST;
        if (k_ptr->flags3 & TR3_ENCHANTABLE)
            f3 |= TR3_ENCHANTABLE;

        // base item
        dif_inc += k_ptr->level / 2;
    }

    // unusual weight
    if (o_ptr->weight == 0)
        weight_factor = 1100;
    else if (o_ptr->weight > k_ptr->weight)
        weight_factor = 100 * o_ptr->weight / k_ptr->weight;
    else
        weight_factor = 100 * k_ptr->weight / o_ptr->weight;

    dif_inc += (weight_factor - 100) / 20;
    if (f4 & (TR4_WEIGHT | TR4_NEG_WEIGHT))
        dif_inc += 5;

    // Jewelry combat bonuses are paid from zero, regardless of base item mins.
    int smith_base_att = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : k_ptr->att;
    int smith_base_evn = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : k_ptr->evn;
    int smith_base_ds = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : k_ptr->ds;
    int smith_base_prot = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : ((k_ptr->ps > 0) ? ((k_ptr->ps + 1) * k_ptr->pd) : 0);

    // attack bonus
    x = att_base - smith_base_att;

    // special costs for attack bonus for weapons
    if (o_ptr->tval == TV_ARROW || o_ptr->tval == TV_BOW
        || o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM
        || o_ptr->tval == TV_HAFTED)
    {
        dif_inc += dif_mod_signed(x, 3);
    }
    // normal costs for other items
    else
    {
        dif_inc += dif_mod_signed(x, 6);
        if (x > 0)
            dif_inc -= 1;
    }

    // evasion bonus
    x = evn_base - smith_base_evn;
    if (o_ptr->tval == TV_SOFT_ARMOR || o_ptr->tval == TV_MAIL
        || o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_HELM
        || o_ptr->tval == TV_CROWN || o_ptr->tval == TV_CLOAK
        || o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
    {
        dif_inc += dif_mod_signed(x, 6);
        if (x > 0)
            dif_inc -= 1;
    }
    else
    {
        dif_inc += dif_mod_signed(x, 9);
        if (x > 0)
            dif_inc -= 2;
    }

    // damage bonus
    x = (ds_base - smith_base_ds);
    // dd used to be a factor here, but a shortsword is far more breakable than
    // a great axe adjusted to make >1 damage sides expensive to smith
    dif_inc += dif_mod_signed(x, 3 * ABS(x) + 2);

    // protection bonus
    base = smith_base_prot;
    int ps_calc = (ps_base > 0) ? ps_base : 0;
    new = (ps_calc > 0) ? ((ps_calc + 1) * o_ptr->pd) : 0;
    x = new - base;

    // special costs for protection sides on hauberks and amulets
    if ((o_ptr->tval == TV_MAIL) && (o_ptr->sval == SV_LONG_CORSLET))
    {
        dif_inc += dif_mod_signed(x, 1);
        if (x > 0)
            dif_inc += 2;
    }
    else if (o_ptr->tval == TV_AMULET)
    {
        dif_inc += dif_mod_signed(x, 1);
        if (x > 0)
            dif_inc += 4;
    }
    else
    {
        dif_inc += dif_mod_signed(x, 3);
    }

    // weapon modifiers
    if (f1 & TR1_SLAY_ORC)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_TROLL)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_WOLF)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_SPIDER)
    {
        dif_inc += 4;
    }
    if (f1 & TR1_SLAY_UNDEAD)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_RAUKO)
    {
        dif_inc += 4;
    }
    if (f1 & TR1_SLAY_DRAGON)
    {
        dif_inc += 4;
    }
    if (f1 & TR1_SLAY_MAN_OR_ELF)
    {
        dif_inc += 5;
    }

    if (f4 & TR4_SLAY_SERPENT)
    {
        dif_inc += 4;
    }
    if (f4 & TR4_SLAY_VAMPIRE)
    {
        dif_inc += 4;
    }
    if (f4 & TR4_SLAY_HORROR)
    {
        dif_inc += 4;
    }
    if (f4 & TR4_SLAY_CAT)
    {
        dif_inc += 3;
    }
    if (f4 & TR4_SLAY_GIANT)
    {
        dif_inc += 3;
    }

    if (f1 & TR1_BRAND_COLD)
    {
        dif_inc += 18;
        smithing_cost.str += 2;
        brands++;
    }
    if (f1 & TR1_BRAND_FIRE)
    {
        dif_inc += 14;
        smithing_cost.str += 2;
        brands++;
    }
    if (f1 & TR1_BRAND_POIS)
    {
        if (o_ptr->tval == TV_ARROW)
        {
            dif_inc += 12;
            smithing_cost.str += 1;
        }
        else
        {
            dif_inc += 16;
            smithing_cost.str += 2;
            brands++;
        }
    }
    if (f1 & TR1_BRAND_ELEC)
    {
        dif_inc += 16;  // No monsters have HURT_ELEC, same as poison
        smithing_cost.str += 2;
        brands++;
    }
    if (brands > 1)
    {
        dif_inc += (brands - 1) * 20;
    }

    if (f1 & TR1_SHARPNESS)
    {
        int base = (o_ptr->tval == TV_ARROW) ? 14 : 24;
        dif_inc += base;
        smithing_cost.str += (o_ptr->tval == TV_ARROW) ? 1 : 2;
    }
    if (f1 & TR1_SHARPNESS2)
    {
        int base = 40;
        dif_inc += base;
        smithing_cost.str += 4;
    }
    if (f1 & TR1_VAMPIRIC)
    {
        dif_inc += 6;
        smithing_cost.str += 1;
    }
    if (f3 & TR3_WILL_DRAIN)
    {
        dif_inc += 8;  // Like VAMPIRIC+2
    }
    if (f3 & TR3_ACCURATE)
    {
        dif_inc += 15;
        smithing_cost.dex += 1;
    }
    if (f4 & TR4_ARMOR_SHATTER)
    {
        dif_inc += 15;  // Like ACCURATE
    }
    if (f4 & TR4_DEPTH_SCALE_PS)
    {
        dif_inc += 5;  // Situational
    }
    if (f4 & TR4_PAIRED)
    {
        dif_inc += 3;  // Paired weapon bonus
    }
    if (f4 & TR4_SUBTLETY_THROW)
    {
        dif_inc += 15;
    }

    // pval dependent bonuses
    if (f1 & TR1_TUNNEL)
    {
        x = o_ptr->pval - k_ptr->pval;
        dif_mod(x, 8, &dif_inc);
        smithing_cost.str += (x > 0) ? x : 0;
    }

    /* Per-stat/skill bonuses (no longer necessarily tied to a single pval). */
    if (o_ptr->pval > 0 && (f1 & TR1_DAMAGE_SIDES))
    {
        x = o_ptr->pval;
        dif_mod(x, 18, &dif_inc);
        smithing_cost.str += x;
    }

    if (o_ptr->stat_bonus[A_STR] > 0)
    {
        x = o_ptr->stat_bonus[A_STR];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.str += x;
    }
    if (o_ptr->stat_bonus[A_DEX] > 0)
    {
        x = o_ptr->stat_bonus[A_DEX];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.dex += x;
    }
    if (o_ptr->stat_bonus[A_CON] > 0)
    {
        x = o_ptr->stat_bonus[A_CON];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.con += x;
    }
    if (o_ptr->stat_bonus[A_GRA] > 0)
    {
        x = o_ptr->stat_bonus[A_GRA];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.gra += x;
    }

    if (o_ptr->skill_bonus[S_ARC] > 0)
    {
        x = o_ptr->skill_bonus[S_ARC];
        dif_mod(x, 4, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_STL] > 0)
    {
        x = o_ptr->skill_bonus[S_STL];
        dif_mod(x, 4, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_PER] > 0)
    {
        x = o_ptr->skill_bonus[S_PER];
        dif_mod(x, 3, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_WIL] > 0)
    {
        x = o_ptr->skill_bonus[S_WIL];
        dif_mod(x, 3, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_SMT] > 0)
    {
        x = o_ptr->skill_bonus[S_SMT];
        dif_mod(x, 4, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_SNG] > 0)
    {
        x = o_ptr->skill_bonus[S_SNG];
        dif_mod(x, 4, &dif_inc);
    }

    /*
     * Extra difficulty for multiple distinct stat/skill bonuses.
     * First bonus is "free" (already covered by the per-bonus scaling above).
     */
    {
        int stat_count = 0;
        int skill_count = 0;

        if (o_ptr->stat_bonus[A_STR] > 0)
            stat_count++;
        if (o_ptr->stat_bonus[A_DEX] > 0)
            stat_count++;
        if (o_ptr->stat_bonus[A_CON] > 0)
            stat_count++;
        if (o_ptr->stat_bonus[A_GRA] > 0)
            stat_count++;

        if (o_ptr->skill_bonus[S_ARC] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_STL] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_PER] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_WIL] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_SMT] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_SNG] > 0)
            skill_count++;

        if (stat_count > 1)
            dif_inc += (stat_count - 1) * 7;
        if (skill_count > 1)
            dif_inc += (skill_count - 1) * 3;
    }

    // Sustains
    if (f2 & TR2_SUST_STR)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_SUST_DEX)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_SUST_CON)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_SUST_GRA)
    {
        dif_inc += 2;
    }

    // Abilities
    if (f2 & TR2_SLOW_DIGEST)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RADIANCE)
    {
        dif_inc += 6;
        smithing_cost.gra += 1;
    }
    if (f2 & TR2_LIGHT)
    {
        dif_inc += 8;
        smithing_cost.gra += 1;
    }
    if (f2 & TR2_REGEN)
    {
        dif_inc += 4;
    }
    if (f2 & TR2_SEE_INVIS)
    {
        dif_inc += 4;
    }
    if (f2 & TR2_FREE_ACT)
    {
        dif_inc += 7;
    }
    if (f2 & TR2_SPEED)
    {
        dif_inc += 40;
        smithing_cost.con += 5;
    }
    if (f3 & TR3_CHEAT_DEATH)
    {
        dif_inc += 13;
        smithing_cost.con += 1;
    }
    if (f3 & TR3_STAND_FAST)
    {
        dif_inc += 2;
    }
    if (f3 & TR3_AVOID_TRAPS)
    {
        dif_inc += 6;
    }
    if (f3 & TR3_MEDIC)
    {
        dif_inc += 4;
    }
    if (f3 & TR3_OATH_BOOST)
    {
        dif_inc += 5;
    }
    if (f3 & TR3_OATH_NEGATE)
    {
        dif_dec += 5;
    }

    // Elemental Resistances
    if (f2 & TR2_RES_COLD)
    {
        dif_inc += 5;
    }
    if (f2 & TR2_RES_FIRE)
    {
        dif_inc += 5;
    }
    if (f2 & TR2_RES_POIS)
    {
        dif_inc += 5;
    }
    if (f2 & TR2_RES_ELEC)
    {
        dif_inc += 5;
    }

    // Other Resistances
    if (f2 & TR2_RES_BLEED)
    {
        dif_inc += 1;
    }
    if (f2 & TR2_RES_BLIND)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_CONFU)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_STUN)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_FEAR)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_HALLU)
    {
        dif_inc += 1;
    }

    // Penalty Flags
    if (!o_ptr->name1)
    {
        if (f2 & TR2_DANGER)
        {
            dif_dec += 5;
        } // only Danger counts
        if (f2 & TR2_DARKNESS)
        {
            dif_dec += 2;  // Changed from 3
        }
        if (f2 & TR2_AGGRAVATE)
        {
            dif_dec += 3;
        }
        if (f2 & TR2_HAUNTED)
        {
            dif_dec += 5;
        }
        if (f2 & TR2_VUL_COLD)
        {
            dif_dec += 4;
        }
        if (f2 & TR2_VUL_FIRE)
        {
            dif_dec += 4;
        }
        if (f2 & TR2_VUL_POIS)
        {
            dif_dec += 4;
        }
        if (f2 & TR2_TRAITOR)
        {
            dif_dec += 2;
        }
        if (f3 & TR3_LIGHT_CURSE)
        {
            dif_dec += 2;
        }
        if (f3 & TR3_CUMBERSOME)
        {
            dif_dec += 3;
        }
        if (f4 & TR4_UNLIGHT)
        {
            dif_dec += 5;  // Worse than DARKNESS - pure negative, no light bonus
        }
        if (f2 & TR2_SLOWNESS)
        {
            dif_dec += 15;
        }
        if (f2 & TR2_HUNGER)
        {
            dif_dec += 3;
        }
        if (f2 & TR2_FEAR)  // Not RES_FEAR!
        {
            dif_dec += 5;
        }
        if (f3 & TR3_HEAVY_CURSE)
        {
            dif_dec += 4;
        }
        if (f3 & TR3_PERMA_CURSE)
        {
            dif_dec += 6;
        }
    }

    // Abilities
    for (i = 0; i < o_ptr->abilities; i++)
    {
        int level = (&b_info[ability_index(
                         o_ptr->skilltype[i], o_ptr->abilitynum[i])])
                        ->level;

        dif_inc += 5 + (level / 3);
        smithing_cost.exp += 50 * level;
    }

    // Penalty for being an artefact
    if (o_ptr->name1)
    {
        if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_FEANOR)) smithing_cost.uses +=2;
        // else smithing_cost.uses += 2;
    }

    // Set the overall difficulty
    dif = dif_inc - dif_dec;

    // Increased difficulties for minor slots
    switch (wield_slot(o_ptr))
    {
    // case INVEN_WIELD:
    case INVEN_LEFT:
    case INVEN_RIGHT:
    {
        // Celebrimbor: rings are not minor slots (no penalty)
        if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_CELEBRIMBOR))
        {
            dif_mult += 20;
        }
        break;
    }
    // case INVEN_NECK:
    case INVEN_LITE:
    // case INVEN_BODY:
    case INVEN_OUTER:
    // case INVEN_ARM:
    // case INVEN_HEAD:
    case INVEN_HANDS:
    case INVEN_FEET:
    case INVEN_QUIVER1:
    case INVEN_QUIVER2:
    case INVEN_HORN:
    {
        dif_mult += 20;
        break;
    }
    }

    // Decreased difficulties for easily enchatable items
    if (k_ptr->flags3 & (TR3_ENCHANTABLE))
    {
        dif_mult -= 30;
    }

    // Celebrimbor: treat rings as enchantable
    if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_CELEBRIMBOR)
        && (o_ptr->tval == TV_RING))
    {
        dif_mult -= 30;
    }

    // Mithril
    if (k_ptr->flags3 & TR3_MITHRIL)
    {
        smithing_cost.mithril += o_ptr->weight;
    }
    // Star iron
    if (k_ptr->flags3 & TR3_STAR_IRON)
    {
        smithing_cost.star_iron += o_ptr->weight;
    }

    /* Optional alloy bonus */
    if (smith_alloy.type != SMITH_ALLOY_NONE)
    {
        int alloy_weight = smith_alloy_weight_required(o_ptr);
        smithing_cost.alloy_weight = alloy_weight;
        smithing_cost.alloy_metal = smith_alloy.type;

        if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
            smithing_cost.mithril += alloy_weight;
        else if (smith_alloy.type == SMITH_ALLOY_STAR_IRON)
            smithing_cost.star_iron += alloy_weight;
    }

   /* Gamil character bonus  override normal mithril cost */
  if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_GAMIL)      /* youre Gamil */
      && (k_ptr->flags3 & TR3_MITHRIL)                     /* item is mithril */
      && (mithril_carried() < smithing_cost.mithril))      /* no mithril on hand */
  {
      smithing_cost.uses    = MAX(smithing_cost.uses, 3);  /* cost 3 forge uses */
      smithing_cost.mithril = 0;                           /* waive material */
      o_ptr->ident         |= IDENT_CANT_MELT;             /* cant melt later */
  }

    // Apply the difficulty multiplier
    dif = dif * dif_mult / 100;

    // Artefact arrows are much easier
    if ((o_ptr->tval == TV_ARROW) && (o_ptr->name1))
        dif /= 2;

    // Deal with masterpiece and Aule's Forge
    int effective_skill = p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);
    
    if (p_ptr->have_ability[S_SPC][SPC_AULE]) {
        // Aule's Forge: supersedes Masterpiece, allows burning base skill for 2x difficulty allowance
        int max_aule_difficulty = effective_skill + (p_ptr->skill_base[S_SMT] * 2);
        if (dif > effective_skill) {
            if (dif <= max_aule_difficulty) {
                // Can craft this with Aule's Forge - drain base skill efficiently
                int excess = dif - effective_skill;
                smithing_cost.drain += (excess + 1) / 2; // drain 1 skill for every 2 excess points
                log_trace("ABILITY DEBUG: Aule's Forge drain - base_skill: %d, skill_use: %d, effective: %d, max_aule: %d, difficulty: %d, excess: %d, drain: %d", 
                         p_ptr->skill_base[S_SMT], p_ptr->skill_use[S_SMT], effective_skill, max_aule_difficulty, dif, excess, (excess + 1) / 2);
            } else {
                // Too difficult even with Aule's Forge
                smithing_cost.drain += p_ptr->skill_base[S_SMT] + (dif - max_aule_difficulty);
                log_trace("ABILITY DEBUG: Aule's Forge insufficient - max possible: %d, difficulty: %d", max_aule_difficulty, dif);
            }
        } else {
            log_trace("ABILITY DEBUG: Aule's Forge active - no drain needed (difficulty %d <= effective skill %d)", dif, effective_skill);
        }
    } else if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE]) {
        // Regular Masterpiece ability - allows burning base skill for 1x difficulty allowance
        int max_masterpiece_difficulty = effective_skill + p_ptr->skill_base[S_SMT];
        if (dif > effective_skill) {
            if (dif <= max_masterpiece_difficulty) {
                // Can craft this with Masterpiece - drain base skill normally
                smithing_cost.drain += dif - effective_skill;
            } else {
                // Too difficult even with Masterpiece
                smithing_cost.drain += p_ptr->skill_base[S_SMT] + (dif - max_masterpiece_difficulty);
            }
        }
    }

    bool needs_alloy_mastery = ((k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON)) != 0)
        || (smith_alloy.type != SMITH_ALLOY_NONE);

    // determine which additional smithing abilities would be required
    cat = smith_item_category(smith_o_ptr);
    if ((cat == CAT_WEAPON) && !p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
    {
        smithing_cost.weaponsmith = 1;
    }
    if ((cat == CAT_ARMOUR) && !p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
    {
        smithing_cost.armoursmith = 1;
    }
    if ((cat == CAT_JEWELRY) && !p_ptr->active_ability[S_SMT][SMT_JEWELLER])
    {
        smithing_cost.jeweller = 1;
    }
    if (smith_o_ptr->name1 && !p_ptr->active_ability[S_SMT][SMT_ARTEFACT])
    {
        smithing_cost.artifice = 1;
    }
    if (object_has_ego(smith_o_ptr) && !p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT])
    {
        smithing_cost.enchantment = 1;
    }
    if (needs_alloy_mastery && !p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY])
    {
        smithing_cost.alloy_mastery = 1;
    }
    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        smithing_cost.str = 0;
        smithing_cost.dex = 0;
        smithing_cost.con = 0;
        smithing_cost.gra = 0;
        smithing_cost.exp = 0;
    }

    return (dif);
}

/*
 * Clears the object's name and description at the bottom of the screen.
 */
void wipe_object_description(void)
{
    if (smith_ui_last_desc_row >= 0)
        smith_ui_clear_from_row(smith_ui_last_desc_row);

    smith_ui_reset_description_state();
}

/*
 * Displays the object's name and description at the bottom of the screen.
 */
void prt_object_description(void)
{
    char o_desc[80];
    char buf[80];
    int display_flag;
    int desc_row;
    int desc_col;
    int desc_width;

    wipe_object_description();

    // abort if there is no object to display
    if (smith_o_ptr->tval == 0)
        return;

    desc_row = smith_ui_description_row();
    if (desc_row < 0)
        return;

    smith_ui_last_desc_row = desc_row;
    smith_ui_clear_from_row(desc_row);

    desc_col = smith_ui_desc_col();
    desc_width = smith_ui_term_wid() - desc_col;

    if (smith_o_ptr->number > 1)
        display_flag = true;
    else
        display_flag = false;

    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, display_flag, 2);

    SDL_strlcat(o_desc,
        format("   %d.%d lb", smith_o_ptr->weight * smith_o_ptr->number / 10,
            (smith_o_ptr->weight * smith_o_ptr->number) % 10),
        sizeof(o_desc));

    if (p_ptr->smithing_leftover)
    {
        strnfmt(buf, sizeof(buf), "In progress: %d turns left",
            p_ptr->smithing_leftover);
        Term_putstr(desc_col, desc_row, desc_width, TERM_L_BLUE, buf);
        desc_row++;
        if (desc_row >= smith_ui_term_hgt())
            return;
    }

    Term_putstr(desc_col, desc_row, desc_width, TERM_L_WHITE, o_desc);
    desc_row++;
    if (desc_row >= smith_ui_term_hgt())
        return;

    Term_gotoxy(desc_col, desc_row);

    /* Set hooks for character dump */
    object_info_out_flags = object_flags;

    /* Set the indent/wrap */
    text_out_indent = desc_col;
    text_out_wrap = smith_ui_term_wid() - 1;

    text_out_hook = text_out_to_screen;

    if (smith_ui_show_lore())
    {
        text_out_c(TERM_WHITE, k_text + k_info[smith_o_ptr->k_idx].text);

        if ((k_text + k_info[smith_o_ptr->k_idx].text)[0] != '\0')
            text_out(" ");
    }

    /* Dump only the mechanical info on short screens. */
    if (object_info_out(smith_o_ptr) && smith_ui_show_lore())
        text_out("\n");

    /* Reset indent/wrap */
    text_out_indent = 0;
    text_out_wrap = 0;
}

/*
 * Determines whether an item is too difficult to make.
 */
int too_difficult(object_type* o_ptr)
{
    int ability = p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);
    int dif = object_difficulty(o_ptr);

    if (p_ptr->have_ability[S_SPC][SPC_AULE]) {
        // Aule's Forge: can craft up to skill_use + (skill_base * 2)
        int max_aule_difficulty = ability + (p_ptr->skill_base[S_SMT] * 2);
        log_trace("ABILITY DEBUG: Aule's Forge too_difficult check - max possible: %d, difficulty: %d", max_aule_difficulty, dif);
        if (max_aule_difficulty >= dif)
            return (false);
        else
            return (true);
    } else if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE]) {
        // Masterpiece: can craft up to skill_use + skill_base
        ability += p_ptr->skill_base[S_SMT];
    }

    if (ability < dif)
        return (true);
    else
        return (false);
}

/*
 * Displays the object's difficulty and costs in the right hand side of the
 * screen.
 */
void prt_object_difficulty(void)
{
    int dif;
    char buf[80];
    int turn_multiplier = 10;
    int costs = 0;
    byte attr;
    bool affordable = true;
    bool compact = smith_ui_compact_width();
    int cost_title_row = smith_ui_cost_title_row();

    Term_putstr(COL_SMT4, 3, -1, TERM_WHITE, "                 ");

    // abort if there is no object to display
    if (smith_o_ptr->tval == 0)
        return;

    // display difficulty information
    if (too_difficult(smith_o_ptr))
        attr = TERM_L_DARK;
    else
        attr = TERM_SLATE;

    Term_putstr(COL_SMT4, 2, -1, attr, "Difficulty:");

    // change colour if smithing drain is required
    if ((smithing_cost.drain > 0)
        && (smithing_cost.drain <= p_ptr->skill_base[S_SMT]))
    {
        attr = TERM_BLUE;
    }

    // calculate difficulty (and costs)
    dif = object_difficulty(smith_o_ptr);

    sprintf(buf, "%d", dif);
    Term_putstr(COL_SMT4 + 2, 4, -1, attr, buf);

    if (compact)
        strnfmt(buf, sizeof(buf), "/%d",
            p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
    else
        strnfmt(buf, sizeof(buf), "(max %d)",
            p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
    Term_putstr(COL_SMT4 + (compact ? 4 : 5), 4, -1, TERM_L_DARK, buf);

    // display cost information
    if (smithing_cost.weaponsmith)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Weaponsmith");
        costs++;
    }
    if (smithing_cost.armoursmith)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Armoursmith");
        costs++;
    }
    if (smithing_cost.jeweller)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Jeweller");
        costs++;
    }
    if (smithing_cost.enchantment)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Enchantment");
        costs++;
    }
    if (smithing_cost.artifice)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Artifice");
        costs++;
    }
    if (smithing_cost.alloy_mastery)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Alloy Mastery");
        costs++;
    }
    if (smithing_cost.uses > 0)
    {
        if (forge_uses(p_ptr->py, p_ptr->px) >= smithing_cost.uses)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        if (smithing_cost.uses == 1)
        {
            sprintf(buf, "%d Use", smithing_cost.uses);
        }
        else
        {
            sprintf(buf, "%d Uses", smithing_cost.uses);
        }
        if (compact)
        {
            strnfmt(buf, sizeof(buf), "%d/%d uses", smithing_cost.uses,
                forge_uses(p_ptr->py, p_ptr->px));
            smith_ui_put_cost_line(costs, attr, buf);
        }
        else
        {
            smith_ui_put_cost_line(costs, attr, buf);
            strnfmt(buf, sizeof(buf), "(of %d)", forge_uses(p_ptr->py, p_ptr->px));
            Term_putstr(COL_SMT4 + 9, smith_ui_cost_item_row(costs), -1,
                TERM_L_DARK, buf);
        }
        costs++;
    }
    if (smithing_cost.drain > 0)
    {
        if (smithing_cost.drain <= p_ptr->skill_base[S_SMT])
        {
            attr = TERM_BLUE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Smithing", smithing_cost.drain);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.mithril > 0)
    {
        if (smithing_cost.mithril <= mithril_carried())
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, compact ? "%d.%d lb Mith" : "%d.%d lb Mithril",
            smithing_cost.mithril / 10,
            smithing_cost.mithril % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.star_iron > 0)
    {
        if (smithing_cost.star_iron <= star_iron_carried())
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, compact ? "%d.%d lb StIron" : "%d.%d lb Star Iron",
            smithing_cost.star_iron / 10,
            smithing_cost.star_iron % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.str > 0)
    {
        if (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - smithing_cost.str
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Str", smithing_cost.str);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.dex > 0)
    {
        if (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - smithing_cost.dex
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Dex", smithing_cost.dex);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.con > 0)
    {
        if (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - smithing_cost.con
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Con", smithing_cost.con);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.gra > 0)
    {
        if (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - smithing_cost.gra
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Gra", smithing_cost.gra);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.exp > 0)
    {
        if (p_ptr->new_exp >= smithing_cost.exp)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Exp", smithing_cost.exp);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        turn_multiplier /= 2;
    }

    attr = TERM_SLATE;
    sprintf(buf, "%d Turns", MAX(10, dif * turn_multiplier));
    smith_ui_put_cost_line(costs, attr, buf);
    costs++;

    // if (costs == 0)
    //{
    //	Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_SLATE, "-");
    //}

    // display cost title
    if (affordable)
        attr = TERM_SLATE;
    else
        attr = TERM_L_DARK;
    Term_putstr(COL_SMT4, cost_title_row, -1, attr, "Cost:");
}

/*
 * Checks whether you can pay the costs in terms of ability points and
 * experience needed to make the object.
 */
bool affordable(object_type* o_ptr)
{
    bool can_afford = true;

    // can't afford non-existant items
    if (o_ptr->tval == 0)
        return (false);
    if (object_has_evil_alignment(o_ptr))
        return (false);

    if (too_difficult(o_ptr))
        can_afford = false;
    if ((smithing_cost.str > 0)
        && (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - smithing_cost.str
            < -5))
        can_afford = false;
    if ((smithing_cost.dex > 0)
        && (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - smithing_cost.dex
            < -5))
        can_afford = false;
    if ((smithing_cost.con > 0)
        && (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - smithing_cost.con
            < -5))
        can_afford = false;
    if ((smithing_cost.gra > 0)
        && (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - smithing_cost.gra
            < -5))
        can_afford = false;
    if (smithing_cost.exp > p_ptr->new_exp)
        can_afford = false;
    if ((smithing_cost.mithril > 0)
        && (smithing_cost.mithril > mithril_carried()))
        can_afford = false;
    if ((smithing_cost.star_iron > 0)
        && (smithing_cost.star_iron > star_iron_carried()))
        can_afford = false;
    if (forge_uses(p_ptr->py, p_ptr->px) < smithing_cost.uses)
        can_afford = false;

    if (smithing_cost.weaponsmith || smithing_cost.armoursmith
        || smithing_cost.jeweller || smithing_cost.enchantment
        || smithing_cost.artifice || smithing_cost.alloy_mastery)
        can_afford = false;

    return (can_afford);
}

/*
 * Pay the costs in terms of ability points and experience needed to make the
 * object.
 */
void pay_costs()
{
    if (smithing_cost.str > 0)
        p_ptr->stat_drain[A_STR] -= smithing_cost.str;
    if (smithing_cost.dex > 0)
        p_ptr->stat_drain[A_DEX] -= smithing_cost.dex;
    if (smithing_cost.con > 0)
        p_ptr->stat_drain[A_CON] -= smithing_cost.con;
    if (smithing_cost.gra > 0)
        p_ptr->stat_drain[A_GRA] -= smithing_cost.gra;

    if (smithing_cost.exp > 0)
        p_ptr->new_exp -= smithing_cost.exp;
    if (smithing_cost.mithril > 0)
        use_mithril(smithing_cost.mithril);
    if (smithing_cost.star_iron > 0)
        use_star_iron(smithing_cost.star_iron);
    if (smithing_cost.uses > 0)
        cave_feat[p_ptr->py][p_ptr->px] -= smithing_cost.uses;
    if (smithing_cost.drain > 0)
        p_ptr->skill_base[S_SMT] -= smithing_cost.drain;

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Set the redraw flag for everything */
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

typedef struct reforge_preview_type
{
    int scaled_difficulty;
    int raw_delta_difficulty;
    int turns;
    smithing_cost_type cost;
    bool affordable;
} reforge_preview_type;

static void smithing_cost_reset_local(smithing_cost_type* cost)
{
    if (!cost)
        return;

    memset(cost, 0, sizeof(*cost));
}

static bool smith_has_category_ability(const object_type* o_ptr)
{
    int cat;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    cat = smith_item_category(o_ptr);
    if ((cat == CAT_WEAPON) && !p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
        return false;
    if ((cat == CAT_ARMOUR) && !p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
        return false;
    if ((cat == CAT_JEWELRY) && !p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        return false;

    return true;
}

static bool object_has_evil_alignment(const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f3;
    return (f4 & TR4_EVIL_ITEM) != 0;
}

static bool smith_has_alignment_conflict(const object_type* o_ptr,
    int prefix_idx, int suffix_idx)
{
    u32b f1, f2, f3, f4;
    bool has_noble;
    bool has_evil;

    if (!o_ptr)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f3;

    has_noble = ((f4 & TR4_NOBLE_ITEM) != 0);
    has_evil = ((f4 & TR4_EVIL_ITEM) != 0);

    if (prefix_idx > 0)
    {
        if (e_info[prefix_idx].flags4 & TR4_NOBLE_ITEM)
            has_noble = true;
        if (e_info[prefix_idx].flags4 & TR4_EVIL_ITEM)
            has_evil = true;
    }

    if (suffix_idx > 0)
    {
        if (e_info[suffix_idx].flags4 & TR4_NOBLE_ITEM)
            has_noble = true;
        if (e_info[suffix_idx].flags4 & TR4_EVIL_ITEM)
            has_evil = true;
    }

    return has_noble && has_evil;
}

static bool ego_forbids_prefix_combo(int e_idx)
{
    if (e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    return (e_info[e_idx].flags4 & TR4_NO_PREFIX) != 0;
}

static bool smith_ego_is_forbidden_affix(const ego_item_type* e_ptr)
{
    if (!e_ptr)
        return true;
    if (e_ptr->flags3 & (TR3_DAMAGED | TR3_NO_SMITHING))
        return true;
    if (e_ptr->flags4 & (TR4_JINX | TR4_EVIL_ITEM))
        return true;
    return false;
}

static bool smith_ego_matches_item_type(const object_type* o_ptr,
    const ego_item_type* e_ptr)
{
    int j;

    if (!o_ptr || !o_ptr->k_idx || !e_ptr)
        return false;

    for (j = 0; j < EGO_TVALS_MAX; j++)
    {
        if (o_ptr->tval != e_ptr->tval[j])
            continue;
        if (o_ptr->sval < e_ptr->min_sval[j])
            continue;
        if (o_ptr->sval > e_ptr->max_sval[j])
            continue;

        return true;
    }

    return false;
}

static bool smith_ego_can_apply_to_object(const object_type* o_ptr, int e_idx,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix)
{
    ego_item_type* e_ptr;
    const char* raw_name;
    bool is_prefix;

    if (!o_ptr || !o_ptr->k_idx || e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    e_ptr = &e_info[e_idx];
    raw_name = e_name + e_ptr->name;
    is_prefix = ego_name_is_prefix(raw_name);

    if (selecting_prefix != is_prefix)
        return false;
    if (smith_ego_is_forbidden_affix(e_ptr))
        return false;
    if (!smith_ego_matches_item_type(o_ptr, e_ptr))
        return false;

    if (selecting_prefix)
    {
        if (ego_forbids_prefix_combo(fixed_suffix))
            return false;
        if (smith_has_alignment_conflict(o_ptr, e_idx, fixed_suffix))
            return false;
    }
    else
    {
        if ((fixed_prefix != 0) && ego_forbids_prefix_combo(e_idx))
            return false;
        if (smith_has_alignment_conflict(o_ptr, fixed_prefix, e_idx))
            return false;
    }

    return true;
}

static bool ego_prefix_can_apply_to_object(const object_type* o_ptr, int e_idx)
{
    return smith_ego_can_apply_to_object(o_ptr, e_idx, 0, 0, true);
}

static bool object_can_reforge_prefix(const object_type* o_ptr)
{
    int i;

    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (o_ptr->name1)
        return false;
    if (object_is_damaged_item(o_ptr))
        return false;
    if (object_has_evil_alignment(o_ptr))
        return false;
    if (is_smithed_by_player(o_ptr))
        return false;
    if (object_ego_prefix(o_ptr))
        return false;
    if (ego_forbids_prefix_combo((int)object_ego_suffix(o_ptr)))
        return false;
    if (!smith_has_category_ability(o_ptr))
        return false;

    for (i = 1; i < z_info->e_max; i++)
    {
        if (ego_prefix_can_apply_to_object(o_ptr, i))
            return true;
    }

    return false;
}

static int find_reforge_target_item(void)
{
    int i;

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;
        if (object_can_repair_damage(o_ptr) || object_can_reforge_prefix(o_ptr))
            return i;
    }

    return -1;
}

static void smith_eval_object(const object_type* src, int* difficulty,
    smithing_cost_type* cost_out)
{
    object_type smith_backup;
    smith_alloy_state alloy_backup = smith_alloy;
    smithing_cost_type smithing_cost_backup = smithing_cost;

    if (!src || !src->k_idx)
        return;

    object_copy(&smith_backup, smith_o_ptr);
    object_copy(smith_o_ptr, src);
    smith_clear_alloy_state(&smith_alloy);

    if (difficulty)
        *difficulty = object_difficulty(smith_o_ptr);
    else
        (void)object_difficulty(smith_o_ptr);

    if (cost_out)
        *cost_out = smithing_cost;

    object_copy(smith_o_ptr, &smith_backup);
    smith_alloy = alloy_backup;
    smithing_cost = smithing_cost_backup;
}

static bool smith_reforge_difficulty_affordable(int difficulty, int* drain_out)
{
    int effective_skill = p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);

    if (drain_out)
        *drain_out = 0;

    if (p_ptr->have_ability[S_SPC][SPC_AULE])
    {
        int max_aule_difficulty = effective_skill + (p_ptr->skill_base[S_SMT] * 2);

        if (difficulty <= effective_skill)
            return true;
        if (difficulty <= max_aule_difficulty)
        {
            if (drain_out)
                *drain_out = (difficulty - effective_skill + 1) / 2;
            return true;
        }
        if (drain_out)
            *drain_out = p_ptr->skill_base[S_SMT] + (difficulty - max_aule_difficulty);
        return false;
    }

    if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE])
    {
        int max_masterpiece_difficulty = effective_skill + p_ptr->skill_base[S_SMT];

        if (difficulty <= effective_skill)
            return true;
        if (difficulty <= max_masterpiece_difficulty)
        {
            if (drain_out)
                *drain_out = difficulty - effective_skill;
            return true;
        }
        if (drain_out)
            *drain_out = p_ptr->skill_base[S_SMT] + (difficulty - max_masterpiece_difficulty);
        return false;
    }

    return (difficulty <= effective_skill);
}

static void smithing_cost_delta_positive(const smithing_cost_type* before,
    const smithing_cost_type* after, smithing_cost_type* delta)
{
    smithing_cost_reset_local(delta);

    if (!before || !after || !delta)
        return;

    delta->str = MAX(0, after->str - before->str);
    delta->dex = MAX(0, after->dex - before->dex);
    delta->con = MAX(0, after->con - before->con);
    delta->gra = MAX(0, after->gra - before->gra);
    delta->exp = MAX(0, after->exp - before->exp);
    delta->mithril = MAX(0, after->mithril - before->mithril);
    delta->star_iron = MAX(0, after->star_iron - before->star_iron);
}

static bool reforge_preview_build(const object_type* source, int prefix_idx,
    reforge_preview_type* preview)
{
    int before_diff = 0;
    int after_diff = 0;
    int turn_multiplier = 10;
    smithing_cost_type before_cost;
    smithing_cost_type after_cost;

    if (!source || !source->k_idx || !preview || prefix_idx <= 0)
        return false;

    memset(preview, 0, sizeof(*preview));
    smithing_cost_reset_local(&before_cost);
    smithing_cost_reset_local(&after_cost);

    smith_eval_object(source, &before_diff, &before_cost);

    object_copy(smith_o_ptr, source);
    object_set_ego_prefix(smith_o_ptr, prefix_idx);
    if (!object_apply_ego_affix(smith_o_ptr, prefix_idx, true))
        return false;

    smith_eval_object(smith_o_ptr, &after_diff, &after_cost);

    preview->raw_delta_difficulty = MAX(0, after_diff - before_diff);
    preview->scaled_difficulty = (preview->raw_delta_difficulty * 3 + 1) / 2;
    smithing_cost_delta_positive(&before_cost, &after_cost, &preview->cost);
    preview->cost.uses = 1;

    preview->affordable
        = smith_reforge_difficulty_affordable(
            preview->scaled_difficulty, &preview->cost.drain);

    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        preview->cost.str = 0;
        preview->cost.dex = 0;
        preview->cost.con = 0;
        preview->cost.gra = 0;
        preview->cost.exp = 0;
        turn_multiplier /= 2;
    }

    if ((preview->cost.str > 0)
        && (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - preview->cost.str
            < -5))
        preview->affordable = false;
    if ((preview->cost.dex > 0)
        && (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - preview->cost.dex
            < -5))
        preview->affordable = false;
    if ((preview->cost.con > 0)
        && (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - preview->cost.con
            < -5))
        preview->affordable = false;
    if ((preview->cost.gra > 0)
        && (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - preview->cost.gra
            < -5))
        preview->affordable = false;
    if (preview->cost.exp > p_ptr->new_exp)
        preview->affordable = false;
    if ((preview->cost.mithril > 0)
        && (preview->cost.mithril > mithril_carried()))
        preview->affordable = false;
    if ((preview->cost.star_iron > 0)
        && (preview->cost.star_iron > star_iron_carried()))
        preview->affordable = false;
    if (forge_uses(p_ptr->py, p_ptr->px) < preview->cost.uses)
        preview->affordable = false;
    if ((preview->cost.drain > 0)
        && (preview->cost.drain > p_ptr->skill_base[S_SMT]))
        preview->affordable = false;

    preview->turns = MAX(10, preview->scaled_difficulty * turn_multiplier);
    return true;
}

static void pay_smithing_cost_struct(const smithing_cost_type* cost)
{
    if (!cost)
        return;

    if (cost->str > 0)
        p_ptr->stat_drain[A_STR] -= cost->str;
    if (cost->dex > 0)
        p_ptr->stat_drain[A_DEX] -= cost->dex;
    if (cost->con > 0)
        p_ptr->stat_drain[A_CON] -= cost->con;
    if (cost->gra > 0)
        p_ptr->stat_drain[A_GRA] -= cost->gra;
    if (cost->exp > 0)
        p_ptr->new_exp -= cost->exp;
    if (cost->mithril > 0)
        use_mithril(cost->mithril);
    if (cost->star_iron > 0)
        use_star_iron(cost->star_iron);
    if (cost->uses > 0)
    {
        cave_feat[p_ptr->py][p_ptr->px] -= cost->uses;
        lite_spot(p_ptr->py, p_ptr->px);
    }
    if (cost->drain > 0)
        p_ptr->skill_base[S_SMT] -= cost->drain;

    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

// Determine default stack sizes for smithing-created items.
// Normal: arrows 24/18/12, daggers & spears 3/2/1 (normal/enchanted/artefact).
// This keeps arrows and throwable weapons in sensible stack counts.
static byte smith_default_stack_size(const object_type* o_ptr)
{
    bool is_arrow = (o_ptr->tval == TV_ARROW);
    bool is_spear = (o_ptr->tval == TV_POLEARM) && (o_ptr->sval == SV_SPEAR);
    bool is_dagger = (o_ptr->tval == TV_SWORD) && (o_ptr->sval == SV_DAGGER);

    if (!(is_arrow || is_spear || is_dagger))
    {
        return (o_ptr->number ? o_ptr->number : 1);
    }

    bool is_artifact = (o_ptr->name1 != 0);
    bool is_enchanted = (!is_artifact) && object_has_ego(o_ptr);

    if (is_arrow)
    {
        if (is_artifact) return 12;
        if (is_enchanted) return 18;
        return 24;
    }

    if (is_artifact) return 1;
    if (is_enchanted) return 2;
    return 3;
}

/*
 * Creates the base object (not in the dungeon, but just as a work in progress).
 */
void create_base_object(int tval, int sval)
{
    /* Wipe the object */
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    /* Prepare the item */
    object_prep(smith_o_ptr, lookup_kind(tval, sval));

    // set the pval to 1 if needed (and evasion/accuracy for rings)
    apply_magic_fake(smith_o_ptr);

    // use a default weight
    smith_o_ptr->weight = (&k_info[smith_o_ptr->k_idx])->weight;

    // display all attributes
    smith_o_ptr->ident |= (IDENT_KNOWN | IDENT_SPOIL);

    // Apply default stack sizes for smithing output
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

/*
 * Performs the interface and selection work for the sval part of the base item
 * menu.
 */
int create_sval_menu_aux(int tval, int* highlight)
{
    char ch;
    int i, num;
    char buf[80];
    bool valid[20];
    int sval[20];
    int list_col = COL_SMT3;

    // clear the right of the screen
    wipe_screen_from(indexed_menu_prefix_col(list_col));

    /* We have to search the whole itemlist. */
    for (num = 0, i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        char name[80];

        /* Analyze matching items */
        if (k_ptr->tval == tval)
        {
            /* Skip instant artefact item types */
            if (k_ptr->flags3 & (TR3_INSTA_ART))
                continue;
            if (k_ptr->flags4 & TR4_EVIL_ITEM)
                continue;

            /* Skip certain item types that cannot be made */
            if (k_ptr->flags3 & (TR3_NO_SMITHING))
            {
                bool allow_override = false;
                
                /* Check for specific character unique flag and sval overrides */
                if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_EOL) && 
                    (k_ptr->tval == TV_SOFT_ARMOR) && (k_ptr->sval == SV_ARMOUR_OF_GALVORN))
                {
                    allow_override = true;
                }
                
                if (!allow_override)
                    continue;
            }

            /* Get the "name" of object "i" */
            strip_name(name, i);

            // make a simple version of the object
            create_base_object(tval, k_ptr->sval);

            // Check whether it is a valid choice for creating
            if (affordable(smith_o_ptr))
                valid[num] = true;
            else
                valid[num] = false;

            /* Print it */
            indexed_menu_entry_label(buf, sizeof(buf), num, name);
            smith_ui_put_menu_label(list_col, smith_ui_dense_row(num),
                valid[num] ? TERM_WHITE : TERM_SLATE, buf);

            /* Remember the object sval */
            sval[num] = k_ptr->sval;

            // count the applicable items
            num++;
        }
    }

    // highlight the label
    smith_ui_put_menu_prefix(list_col,
        smith_ui_dense_highlight_row(*highlight), *highlight - 1,
        TERM_L_BLUE, true);

    // make a simple version of the object
    create_base_object(tval, sval[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, smith_ui_dense_highlight_row(*highlight));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + MAX_SMITHING_TVALS - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        // make a simple version of the object
        create_base_object(tval, sval[*highlight - 1]);

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Displays a menu for choosing a base item's sval.
 */
bool create_sval_menu(int tval)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;
    bool completed = false;

    /* Save screen */
    screen_save();

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        choice = create_sval_menu_aux(tval, &highlight);

        if (choice >= 1)
        {
            leave_menu = true;
            completed = true;
        }
        else if (choice == -1)
        {
            /* Wipe the object */
            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);

            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();

    return (completed);
}

/*
 * Performs the interface and selection work for the tval part of the base item
 * menu.
 */
int create_tval_menu_aux(int* highlight)
{
    char ch;
    int i;
    char buf[80];
    bool valid[MAX_SMITHING_TVALS];
    byte valid_attr = TERM_WHITE; // default to soothe compilation warnings

    // clear the right of the screen
    wipe_screen_from(indexed_menu_prefix_col(COL_SMT2));

    // clear bottom of the screen
    wipe_object_description();

    /* Wipe the smithing object */
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    for (i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        indexed_menu_entry_label(buf, sizeof(buf), i, smithing_tvals[i].desc);

        if (smithing_tvals[i].category == CAT_WEAPON)
        {
            valid[i] = true;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                ? TERM_WHITE
                : TERM_RED;
        }
        if (smithing_tvals[i].category == CAT_ARMOUR)
        {
            valid[i] = true;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                ? TERM_WHITE
                : TERM_RED;
        }
        if (smithing_tvals[i].category == CAT_JEWELRY)
        {
            valid[i] = true;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_JEWELLER] ? TERM_WHITE
                                                                    : TERM_RED;
        }

        smith_ui_put_menu_label(COL_SMT2, smith_ui_dense_row(i),
            valid[i] ? valid_attr : TERM_L_DARK, buf);
    }

    // highlight the label
    smith_ui_put_menu_prefix(COL_SMT2,
        smith_ui_dense_highlight_row(*highlight), *highlight - 1,
        TERM_L_BLUE, true);

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, smith_ui_dense_highlight_row(*highlight));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    // choose an option by letter
    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + MAX_SMITHING_TVALS - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        smith_ui_put_menu_prefix(COL_SMT2,
            smith_ui_dense_highlight_row(old_highlight), old_highlight - 1,
            valid[old_highlight - 1] ? TERM_WHITE : TERM_L_DARK, false);
        smith_ui_put_menu_prefix(COL_SMT2,
            smith_ui_dense_highlight_row(*highlight), *highlight - 1,
            TERM_L_BLUE, true);

        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = MAX_SMITHING_TVALS;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < MAX_SMITHING_TVALS)
            (*highlight)++;
        else if (*highlight == MAX_SMITHING_TVALS)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        return (-1);
    }

    return (0);
}

/*
 * Displays a menu for choosing a base item's tval.
 */
void create_tval_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = create_tval_menu_aux(&highlight);

        if (choice >= 1)
        {
            if (create_sval_menu(smithing_tvals[choice - 1].tval))
            {
                leave_menu = true;
            }
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    enchant_then_numbers = false;

    /* Load screen */
    screen_load();
}

/*
 * Actually modifies the numbers on an item.
 */
static void smith_apply_stat_skill_flag_delta(object_type* o_ptr, u32b f1_before, u32b f1_after)
{
    if (!o_ptr)
        return;

    int pval = o_ptr->pval;
    int pval_abs = ABS(pval);

    bool before_str = (f1_before & (TR1_STR | TR1_NEG_STR)) != 0;
    bool after_str = (f1_after & (TR1_STR | TR1_NEG_STR)) != 0;
    if (!after_str)
    {
        o_ptr->stat_bonus[A_STR] = 0;
    }
    else if (!before_str)
    {
        o_ptr->stat_bonus[A_STR] = (f1_after & TR1_NEG_STR) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_STR) && !(f1_after & TR1_NEG_STR) && o_ptr->stat_bonus[A_STR] < 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];
    if ((f1_after & TR1_NEG_STR) && !(f1_after & TR1_STR) && o_ptr->stat_bonus[A_STR] > 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];

    bool before_dex = (f1_before & (TR1_DEX | TR1_NEG_DEX)) != 0;
    bool after_dex = (f1_after & (TR1_DEX | TR1_NEG_DEX)) != 0;
    if (!after_dex)
    {
        o_ptr->stat_bonus[A_DEX] = 0;
    }
    else if (!before_dex)
    {
        o_ptr->stat_bonus[A_DEX] = (f1_after & TR1_NEG_DEX) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_DEX) && !(f1_after & TR1_NEG_DEX) && o_ptr->stat_bonus[A_DEX] < 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];
    if ((f1_after & TR1_NEG_DEX) && !(f1_after & TR1_DEX) && o_ptr->stat_bonus[A_DEX] > 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];

    bool before_con = (f1_before & (TR1_CON | TR1_NEG_CON)) != 0;
    bool after_con = (f1_after & (TR1_CON | TR1_NEG_CON)) != 0;
    if (!after_con)
    {
        o_ptr->stat_bonus[A_CON] = 0;
    }
    else if (!before_con)
    {
        o_ptr->stat_bonus[A_CON] = (f1_after & TR1_NEG_CON) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_CON) && !(f1_after & TR1_NEG_CON) && o_ptr->stat_bonus[A_CON] < 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];
    if ((f1_after & TR1_NEG_CON) && !(f1_after & TR1_CON) && o_ptr->stat_bonus[A_CON] > 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];

    bool before_gra = (f1_before & (TR1_GRA | TR1_NEG_GRA)) != 0;
    bool after_gra = (f1_after & (TR1_GRA | TR1_NEG_GRA)) != 0;
    if (!after_gra)
    {
        o_ptr->stat_bonus[A_GRA] = 0;
    }
    else if (!before_gra)
    {
        o_ptr->stat_bonus[A_GRA] = (f1_after & TR1_NEG_GRA) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_GRA) && !(f1_after & TR1_NEG_GRA) && o_ptr->stat_bonus[A_GRA] < 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];
    if ((f1_after & TR1_NEG_GRA) && !(f1_after & TR1_GRA) && o_ptr->stat_bonus[A_GRA] > 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];

    bool before_mel = (f1_before & TR1_MEL) != 0;
    bool after_mel = (f1_after & TR1_MEL) != 0;
    if (!after_mel)
        o_ptr->skill_bonus[S_MEL] = 0;
    else if (!before_mel)
        o_ptr->skill_bonus[S_MEL] = pval;

    bool before_arc = (f1_before & TR1_ARC) != 0;
    bool after_arc = (f1_after & TR1_ARC) != 0;
    if (!after_arc)
        o_ptr->skill_bonus[S_ARC] = 0;
    else if (!before_arc)
        o_ptr->skill_bonus[S_ARC] = pval;

    bool before_stl = (f1_before & TR1_STL) != 0;
    bool after_stl = (f1_after & TR1_STL) != 0;
    if (!after_stl)
        o_ptr->skill_bonus[S_STL] = 0;
    else if (!before_stl)
        o_ptr->skill_bonus[S_STL] = pval;

    bool before_per = (f1_before & TR1_PER) != 0;
    bool after_per = (f1_after & TR1_PER) != 0;
    if (!after_per)
        o_ptr->skill_bonus[S_PER] = 0;
    else if (!before_per)
        o_ptr->skill_bonus[S_PER] = pval;

    bool before_wil = (f1_before & TR1_WIL) != 0;
    bool after_wil = (f1_after & TR1_WIL) != 0;
    if (!after_wil)
        o_ptr->skill_bonus[S_WIL] = 0;
    else if (!before_wil)
        o_ptr->skill_bonus[S_WIL] = pval;

    bool before_smt = (f1_before & TR1_SMT) != 0;
    bool after_smt = (f1_after & TR1_SMT) != 0;
    if (!after_smt)
        o_ptr->skill_bonus[S_SMT] = 0;
    else if (!before_smt)
        o_ptr->skill_bonus[S_SMT] = pval;

    bool before_sng = (f1_before & TR1_SNG) != 0;
    bool after_sng = (f1_after & TR1_SNG) != 0;
    if (!after_sng)
        o_ptr->skill_bonus[S_SNG] = 0;
    else if (!before_sng)
        o_ptr->skill_bonus[S_SNG] = pval;
}

void modify_numbers(int choice)
{
    switch (choice)
    {
    case SMT_NUM_MENU_I_ATT:
    {
        if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)
            smith_o_ptr->att += 3;
        else
            smith_o_ptr->att++;
        break;
    }
    case SMT_NUM_MENU_D_ATT:
    {
        if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)
            smith_o_ptr->att -= 3;
        else
            smith_o_ptr->att--;
        break;
    }
    case SMT_NUM_MENU_I_DS:
        smith_o_ptr->ds++;
        break;
    case SMT_NUM_MENU_D_DS:
        smith_o_ptr->ds--;
        break;
    case SMT_NUM_MENU_I_EVN:
        smith_o_ptr->evn++;
        break;
    case SMT_NUM_MENU_D_EVN:
        smith_o_ptr->evn--;
        break;
    case SMT_NUM_MENU_I_PS:
        smithing_increase_protection(smith_o_ptr);
        break;
    case SMT_NUM_MENU_D_PS:
        smithing_decrease_protection(smith_o_ptr);
        break;
    case SMT_NUM_MENU_I_WGT:
        smith_o_ptr->weight += 5;
        break;
    case SMT_NUM_MENU_D_WGT:
        smith_o_ptr->weight -= 5;
        break;
    case SMT_NUM_MENU_ALLOY_CYCLE:
    {
        if (!p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY])
        {
            bell("You need Alloy mastery to do that.");
            break;
        }
        if (!smith_alloy_applicable(smith_o_ptr))
        {
            bell("Alloying doesn't apply to this item.");
            smith_remove_alloy_bonus(smith_o_ptr, &smith_alloy);
            break;
        }

        smith_alloy_type next = SMITH_ALLOY_NONE;
        if (smith_alloy.type == SMITH_ALLOY_NONE)
            next = SMITH_ALLOY_MITHRIL;
        else if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
            next = SMITH_ALLOY_STAR_IRON;
        else
            next = SMITH_ALLOY_NONE;

        smith_apply_alloy(smith_o_ptr, &smith_alloy, next);
        break;
    }
    case SMT_NUM_MENU_ALLOY_CLEAR:
        smith_remove_alloy_bonus(smith_o_ptr, &smith_alloy);
        break;
    }

    return;
}

/*
 * Performs the interface and selection work for the numbers menu.
 */
int numbers_menu_aux(int* highlight)
{
    int i;
    char ch;
    char buf[80];
    byte attr[SMT_NUM_MENU_MAX];
    bool valid[SMT_NUM_MENU_MAX];
    bool can_afford[SMT_NUM_MENU_MAX] = { false };

    // clear the right of the screen
    wipe_screen_from(indexed_menu_prefix_col(COL_SMT2));

    memset(valid, 0, sizeof(valid));

    valid[SMT_NUM_MENU_I_ATT - 1]
        = att_valid() && (smith_o_ptr->att < att_max());
    valid[SMT_NUM_MENU_D_ATT - 1]
        = att_valid() && (smith_o_ptr->att > att_min());
    valid[SMT_NUM_MENU_I_DS - 1] = ds_valid() && (smith_o_ptr->ds < ds_max());
    valid[SMT_NUM_MENU_D_DS - 1] = ds_valid() && (smith_o_ptr->ds > ds_min());
    valid[SMT_NUM_MENU_I_EVN - 1]
        = evn_valid() && (smith_o_ptr->evn < evn_max());
    valid[SMT_NUM_MENU_D_EVN - 1]
        = evn_valid() && (smith_o_ptr->evn > evn_min());
    valid[SMT_NUM_MENU_I_PS - 1] = ps_valid() && smithing_can_increase_protection(smith_o_ptr);
    valid[SMT_NUM_MENU_D_PS - 1] = ps_valid() && smithing_can_decrease_protection(smith_o_ptr);
    valid[SMT_NUM_MENU_I_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight + 5) <= wgt_max());
    valid[SMT_NUM_MENU_D_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight - 5) >= wgt_min());
    {
        u32b f1, f2, f3;
        object_flags(smith_o_ptr, &f1, &f2, &f3);
        valid[SMT_NUM_MENU_EDIT_BONUSES - 1] = (f1 & (TR1_STR | TR1_NEG_STR | TR1_DEX
                                                     | TR1_NEG_DEX | TR1_CON
                                                     | TR1_NEG_CON | TR1_GRA
                                                     | TR1_NEG_GRA | TR1_MEL
                                                     | TR1_ARC | TR1_STL
                                                     | TR1_PER | TR1_WIL
                                                     | TR1_SMT | TR1_SNG
                                                     | TR1_DAMAGE_SIDES
                                                     | TR1_TUNNEL))
            != 0;
    }
    bool alloy_applicable = smith_alloy_applicable(smith_o_ptr);
    bool has_alloy_mastery = p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY];
    int alloy_weight = alloy_applicable ? smith_alloy_weight_required(smith_o_ptr) : 0;
    int mithril_have = mithril_carried();
    int star_iron_have = star_iron_carried();
    valid[SMT_NUM_MENU_ALLOY_CYCLE - 1] = alloy_applicable && has_alloy_mastery;
    valid[SMT_NUM_MENU_ALLOY_CLEAR - 1] = (smith_alloy.type != SMITH_ALLOY_NONE);

    // retrieve a super backup of the object
    object_copy(smith3_o_ptr, smith_o_ptr);
    smith3_alloy = smith_alloy;
    for (i = 0; i < SMT_NUM_MENU_MAX; i++)
    {
        if ((i == SMT_NUM_MENU_ALLOY_CYCLE - 1)
            || (i == SMT_NUM_MENU_ALLOY_CLEAR - 1)
            || (i == SMT_NUM_MENU_EDIT_BONUSES - 1))
        {
            can_afford[i] = valid[i];
            if (i == SMT_NUM_MENU_ALLOY_CYCLE - 1 && valid[i])
            {
                bool has_any_metal = (mithril_have >= alloy_weight)
                    || (star_iron_have >= alloy_weight);
                attr[i] = has_any_metal ? TERM_WHITE : TERM_SLATE;
            }
            else
            {
                attr[i] = valid[i] ? TERM_WHITE : TERM_L_DARK;
            }
            continue;
        }
        if (valid[i])
        {
            modify_numbers(i + 1);
            can_afford[i] = affordable(smith_o_ptr);

            // retrieve a super backup of the object
            object_copy(smith_o_ptr, smith3_o_ptr);
            smith_alloy = smith3_alloy;
        }

        attr[i] = valid[i] ? (can_afford[i] ? TERM_WHITE : TERM_SLATE)
                           : TERM_L_DARK;
    }

    {
        static cptr number_menu_labels[SMT_NUM_MENU_MAX] = {
            "increase attack bonus",
            "decrease attack bonus",
            "increase damage sides",
            "decrease damage sides",
            "increase evasion bonus",
            "decrease evasion bonus",
            "increase protection",
            "decrease protection",
            "increase weight",
            "decrease weight",
            "cycle alloy (none/mithril/star iron)",
            "remove alloy bonus",
            "adjust special bonuses",
        };

        for (i = 0; i < SMT_NUM_MENU_MAX; i++)
        {
            indexed_menu_entry_label(buf, sizeof(buf), i, number_menu_labels[i]);
            smith_ui_put_menu_label(COL_SMT2, i + 2, attr[i], buf);
        }
    }
    if (alloy_applicable)
    {
        byte info_attr = has_alloy_mastery ? TERM_SLATE : TERM_L_DARK;
        if (!has_alloy_mastery)
        {
            strnfmt(buf, 80, "Alloy needs %d.%d lb metal (requires Alloy mastery)",
                alloy_weight / 10, alloy_weight % 10);
        }
        else
        {
            if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
                info_attr = (mithril_have >= alloy_weight) ? TERM_SLATE : TERM_RED;
            else if (smith_alloy.type == SMITH_ALLOY_STAR_IRON)
                info_attr = (star_iron_have >= alloy_weight) ? TERM_SLATE : TERM_RED;
            strnfmt(buf, 80,
                "Alloy needs %d.%d lb (mithril %d.%d, star iron %d.%d)",
                alloy_weight / 10, alloy_weight % 10, mithril_have / 10,
                mithril_have % 10, star_iron_have / 10, star_iron_have % 10);
        }
        Term_putstr(COL_SMT2, 15, -1, info_attr, buf);
    }
    else if (!has_alloy_mastery)
    {
        Term_putstr(COL_SMT2, 15, -1, TERM_L_DARK,
            "Alloy requires Alloy mastery.");
    }

    // highlight the label
    smith_ui_put_menu_prefix(COL_SMT2, *highlight + 1, *highlight - 1,
        TERM_L_BLUE, true);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    // choose an option by letter
    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + SMT_NUM_MENU_MAX - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(
            old_highlight, attr[old_highlight - 1], *highlight, COL_SMT2);

        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = SMT_NUM_MENU_MAX;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < SMT_NUM_MENU_MAX)
            (*highlight)++;
        else if (*highlight == SMT_NUM_MENU_MAX)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    return (0);
}

typedef enum
{
    SMT_BONUS_ENTRY_STAT = 0,
    SMT_BONUS_ENTRY_SKILL = 1,
    SMT_BONUS_ENTRY_SPECIAL = 2,
} smith_bonus_entry_kind;

typedef enum
{
    SMT_BONUS_SPECIAL_DAMAGE_SIDES = 0,
    SMT_BONUS_SPECIAL_TUNNEL = 1,
} smith_bonus_special_kind;

typedef struct
{
    smith_bonus_entry_kind kind;
    int index;
    u32b flag_pos;
    u32b flag_neg;
    u32b flag;
} smith_bonus_entry;

typedef struct
{
    smith_bonus_entry entry;
    int delta;
} smith_bonus_action;

static const char* smith_bonus_stat_name(int stat)
{
    switch (stat)
    {
    case A_STR:
        return "Strength";
    case A_DEX:
        return "Dexterity";
    case A_CON:
        return "Constitution";
    case A_GRA:
        return "Grace";
    default:
        return "Unknown";
    }
}

static const char* smith_bonus_special_name(int special)
{
    switch (special)
    {
    case SMT_BONUS_SPECIAL_DAMAGE_SIDES:
        return "Damage bonus";
    case SMT_BONUS_SPECIAL_TUNNEL:
        return "Tunneling";
    default:
        return "Unknown";
    }
}

static int smith_collect_bonus_entries(smith_bonus_entry* entries, int max_entries)
{
    u32b f1, f2, f3;
    int n = 0;

    if (!entries || max_entries <= 0)
        return 0;

    object_flags(smith_o_ptr, &f1, &f2, &f3);

    struct stat_flag_map
    {
        int stat;
        u32b flag_pos;
        u32b flag_neg;
    };

    static const struct stat_flag_map stat_flags[A_MAX] = {
        { A_STR, TR1_STR, TR1_NEG_STR },
        { A_DEX, TR1_DEX, TR1_NEG_DEX },
        { A_CON, TR1_CON, TR1_NEG_CON },
        { A_GRA, TR1_GRA, TR1_NEG_GRA },
    };

    for (int i = 0; i < A_MAX && n < max_entries; i++)
    {
        if ((f1 & (stat_flags[i].flag_pos | stat_flags[i].flag_neg)) == 0)
            continue;

        entries[n].kind = SMT_BONUS_ENTRY_STAT;
        entries[n].index = stat_flags[i].stat;
        entries[n].flag_pos = stat_flags[i].flag_pos;
        entries[n].flag_neg = stat_flags[i].flag_neg;
        entries[n].flag = 0;
        n++;
    }

    struct skill_flag_map
    {
        int skill;
        u32b flag;
    };

    static const struct skill_flag_map skill_flags[] = {
        { S_MEL, TR1_MEL },
        { S_ARC, TR1_ARC },
        { S_STL, TR1_STL },
        { S_PER, TR1_PER },
        { S_WIL, TR1_WIL },
        { S_SMT, TR1_SMT },
        { S_SNG, TR1_SNG },
    };

    for (int i = 0; i < (int)N_ELEMENTS(skill_flags) && n < max_entries; i++)
    {
        if ((f1 & skill_flags[i].flag) == 0)
            continue;

        entries[n].kind = SMT_BONUS_ENTRY_SKILL;
        entries[n].index = skill_flags[i].skill;
        entries[n].flag_pos = 0;
        entries[n].flag_neg = 0;
        entries[n].flag = skill_flags[i].flag;
        n++;
    }

    struct special_flag_map
    {
        int special;
        u32b flag;
    };

    static const struct special_flag_map special_flags[] = {
        { SMT_BONUS_SPECIAL_DAMAGE_SIDES, TR1_DAMAGE_SIDES },
        { SMT_BONUS_SPECIAL_TUNNEL, TR1_TUNNEL },
    };

    for (int i = 0; i < (int)N_ELEMENTS(special_flags) && n < max_entries; i++)
    {
        if ((f1 & special_flags[i].flag) == 0)
            continue;

        entries[n].kind = SMT_BONUS_ENTRY_SPECIAL;
        entries[n].index = special_flags[i].special;
        entries[n].flag_pos = 0;
        entries[n].flag_neg = 0;
        entries[n].flag = special_flags[i].flag;
        n++;
    }

    return n;
}

static int smith_collect_bonus_actions(smith_bonus_action* actions, int max_actions)
{
    smith_bonus_entry entries[16];
    int entry_count = smith_collect_bonus_entries(entries, (int)N_ELEMENTS(entries));
    int action_count = 0;

    if (!actions || max_actions <= 0)
        return 0;

    for (int i = 0; i < entry_count && action_count < max_actions; i++)
    {
        actions[action_count].entry = entries[i];
        actions[action_count].delta = 1;
        action_count++;

        if (action_count >= max_actions)
            break;
        actions[action_count].entry = entries[i];
        actions[action_count].delta = -1;
        action_count++;
    }

    return action_count;
}

static bool smith_adjust_bonus_entry(const smith_bonus_entry* entry, int delta)
{
    int max_bonus = pval_max();
    int floor_bonus = pval_min(); /* respect ego min_pval */
    int min_bonus = 0;
    int value = 0;

    if (!entry || !smith_o_ptr || delta == 0)
        return false;

    if (entry->kind == SMT_BONUS_ENTRY_STAT)
    {
        u32b f1, f2, f3;
        object_flags(smith_o_ptr, &f1, &f2, &f3);

        bool has_pos = (f1 & entry->flag_pos) != 0;
        bool has_neg = (f1 & entry->flag_neg) != 0;

        if (has_pos && has_neg)
        {
            min_bonus = -max_bonus;
        }
        else if (has_neg)
        {
            min_bonus = -max_bonus;
            max_bonus = 0;
        }
        else
        {
            /* Positive stat: honour ego min_pval as the lower bound */
            min_bonus = floor_bonus;
        }

        value = smith_o_ptr->stat_bonus[entry->index];
        int new_value = value + delta;
        if (new_value < min_bonus || new_value > max_bonus)
            return false;

        smith_o_ptr->stat_bonus[entry->index] = new_value;
        return true;
    }

    if (entry->kind == SMT_BONUS_ENTRY_SKILL)
    {
        /* Skill bonus: honour ego min_pval as the lower bound */
        value = smith_o_ptr->skill_bonus[entry->index];
        int new_value = value + delta;
        if (new_value < floor_bonus || new_value > max_bonus)
            return false;
        smith_o_ptr->skill_bonus[entry->index] = new_value;
        return true;
    }

    value = smith_o_ptr->pval;
    int new_value = value + delta;
    if (new_value < floor_bonus || new_value > max_bonus)
        return false;
    smith_o_ptr->pval = (s16b)new_value;
    return true;
}

static int smith_bonus_menu_aux(int* highlight)
{
    char ch;
    char buf[80];
    smith_bonus_action actions[26];
    bool valid[26] = { false };
    bool can_afford[26] = { false };
    byte attr[26];
    int num = smith_collect_bonus_actions(actions, (int)N_ELEMENTS(actions));
    const int first_row = 2;
    const int max_row = MAX_SMITHING_TVALS + 2;
    const int max_visible = max_row - first_row + 1;

    wipe_screen_from(indexed_menu_prefix_col(COL_SMT2));

    Term_putstr(COL_SMT2, 1, -1, TERM_WHITE,
        "Adjust special bonuses (ESC to return)");

    if (num <= 0)
    {
        Term_putstr(COL_SMT2, 3, -1, TERM_L_DARK,
            "(No editable special bonuses on this item.)");
        Term_fresh();
        hide_cursor = true;
        (void)inkey();
        hide_cursor = false;
        return -1;
    }

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > num)
        *highlight = num;

    int top = 1;
    if (num > max_visible)
    {
        top = *highlight - max_visible / 2;
        if (top < 1)
            top = 1;
        int max_top = num - max_visible + 1;
        if (top > max_top)
            top = max_top;

        int end = top + max_visible - 1;
        if (end > num)
            end = num;
        strnfmt(buf, sizeof(buf),
            "Adjust special bonuses (ESC to return) [%d-%d/%d]", top, end,
            num);
        Term_putstr(COL_SMT2, 1, -1, TERM_WHITE, buf);
    }

    object_type snapshot;
    smith_alloy_state alloy_snapshot = smith_alloy;

    for (int i = 0; i < num; i++)
    {
        object_copy(&snapshot, smith_o_ptr);

        if (smith_adjust_bonus_entry(&actions[i].entry, actions[i].delta))
        {
            valid[i] = true;
            can_afford[i] = affordable(smith_o_ptr);
        }

        object_copy(smith_o_ptr, &snapshot);
        smith_alloy = alloy_snapshot;

        attr[i] = valid[i] ? (can_afford[i] ? TERM_WHITE : TERM_SLATE)
                           : TERM_L_DARK;

        const char* name = NULL;
        int value = 0;
        if (actions[i].entry.kind == SMT_BONUS_ENTRY_STAT)
        {
            name = smith_bonus_stat_name(actions[i].entry.index);
            value = smith_o_ptr->stat_bonus[actions[i].entry.index];
        }
        else if (actions[i].entry.kind == SMT_BONUS_ENTRY_SKILL)
        {
            name = skill_names_full[actions[i].entry.index];
            value = smith_o_ptr->skill_bonus[actions[i].entry.index];
        }
        else
        {
            name = smith_bonus_special_name(actions[i].entry.index);
            value = smith_o_ptr->pval;
        }
        const char* verb = (actions[i].delta > 0) ? "increase" : "decrease";

        int entry_idx = i + 1;
        int row = first_row + (entry_idx - top);
        if (row >= first_row && row <= max_row)
        {
            char action_label[80];
            strnfmt(action_label, sizeof(action_label), "%s %-12s (%+d)",
                verb, name, value);
            indexed_menu_entry_label(buf, sizeof(buf), i, action_label);
            smith_ui_put_menu_label(COL_SMT2, row, attr[i], buf);
        }
    }

    int hl_row = first_row + (*highlight - top);
    smith_ui_put_menu_prefix(COL_SMT2, hl_row, *highlight - 1,
        TERM_L_BLUE, true);

    prt_object_difficulty();
    prt_object_description();

    Term_fresh();
    Term_gotoxy(2, hl_row);

    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if ((ch == '4') || (ch == ESCAPE))
        return -1;

    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        if (valid[*highlight - 1])
            return (*highlight);

        *highlight = old_highlight;
        bell("Invalid choice.");
    }

    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else
            *highlight = num;
        return 0;
    }

    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else
            *highlight = 1;
        return 0;
    }

    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return *highlight;
        bell("Invalid choice.");
        return 0;
    }

    return 0;
}

static void smith_bonus_menu(void)
{
    int highlight = 1;
    bool leave_menu = false;

    screen_save();

    while (!leave_menu)
    {
        int choice = smith_bonus_menu_aux(&highlight);
        if (choice == -1)
            leave_menu = true;
        else if (choice >= 1)
        {
            smith_bonus_action actions[26];
            int num = smith_collect_bonus_actions(actions, (int)N_ELEMENTS(actions));
            if (choice <= num)
                (void)smith_adjust_bonus_entry(&actions[choice - 1].entry, actions[choice - 1].delta);
        }
    }

    screen_load();
}

/*
 * Displays a menu for modifying numerical bonuses and weight of an item.
 */
void numbers_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    if (object_has_ego(smith_o_ptr))
        enchant_then_numbers = true;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = numbers_menu_aux(&highlight);

        switch (choice)
        {
        case -1:
        {
            leave_menu = true;
            break;
        }

        default:
        {
            if (choice == SMT_NUM_MENU_EDIT_BONUSES)
                smith_bonus_menu();
            else
                modify_numbers(choice);
            break;
        }
        }
    }

    /* Load screen */
    screen_load();

    return;
}

static void ego_name_for_enchant_menu(int e_idx, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;
    buf[0] = '\0';
    if (e_idx <= 0 || e_idx >= z_info->e_max)
        return;

    ego_item_type* e_ptr = &e_info[e_idx];
    const char* raw = e_name + e_ptr->name;
    if (!raw || !raw[0])
        return;

    if (ego_name_is_prefix(raw))
    {
        size_t len = strlen(raw);
        size_t copy_len = (len >= 2) ? (len - 2) : 0;
        if (copy_len >= buflen)
            copy_len = buflen - 1;
        if (copy_len > 0)
        {
            memcpy(buf, raw + 1, copy_len);
            buf[copy_len] = '\0';
        }
        return;
    }

    SDL_strlcpy(buf, raw, buflen);
}

static void prt_reforge_preview(const reforge_preview_type* preview)
{
    char buf[80];
    int costs = 0;
    byte attr = TERM_SLATE;
    bool compact = smith_ui_compact_width();

    wipe_screen_from(COL_SMT4);

    if (!preview)
        return;

    if (!preview->affordable)
        attr = TERM_L_DARK;

    Term_putstr(COL_SMT4, 2, -1, attr, "Reforge Diff:");
    strnfmt(buf, sizeof(buf), "%d", preview->scaled_difficulty);
    Term_putstr(COL_SMT4 + 2, 4, -1, attr, buf);

    if (compact)
        strnfmt(buf, sizeof(buf), "+%d raw", preview->raw_delta_difficulty);
    else
        strnfmt(buf, sizeof(buf), "(+%d raw)", preview->raw_delta_difficulty);
    Term_putstr(COL_SMT4 + (compact ? 4 : 5), 4, -1, TERM_L_DARK, buf);

    Term_putstr(COL_SMT4, smith_ui_cost_title_row(), -1,
        preview->affordable ? TERM_SLATE : TERM_L_DARK, "Cost:");

    if (preview->cost.uses > 0)
    {
        attr = (forge_uses(p_ptr->py, p_ptr->px) >= preview->cost.uses)
            ? TERM_SLATE : TERM_L_DARK;
        if (compact)
            strnfmt(buf, sizeof(buf), "%d/%d uses", preview->cost.uses,
                forge_uses(p_ptr->py, p_ptr->px));
        else
            strnfmt(buf, sizeof(buf), "%d Use%s", preview->cost.uses,
                (preview->cost.uses == 1) ? "" : "s");
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.drain > 0)
    {
        attr = (preview->cost.drain <= p_ptr->skill_base[S_SMT])
            ? TERM_BLUE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Smithing", preview->cost.drain);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.mithril > 0)
    {
        attr = (preview->cost.mithril <= mithril_carried()) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), compact ? "%d.%d lb Mith" : "%d.%d lb Mithril",
            preview->cost.mithril / 10, preview->cost.mithril % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.star_iron > 0)
    {
        attr = (preview->cost.star_iron <= star_iron_carried()) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), compact ? "%d.%d lb StIron" : "%d.%d lb Star Iron",
            preview->cost.star_iron / 10, preview->cost.star_iron % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.str > 0)
    {
        attr = (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR] - preview->cost.str >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Str", preview->cost.str);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.dex > 0)
    {
        attr = (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX] - preview->cost.dex >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Dex", preview->cost.dex);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.con > 0)
    {
        attr = (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON] - preview->cost.con >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Con", preview->cost.con);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.gra > 0)
    {
        attr = (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA] - preview->cost.gra >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Gra", preview->cost.gra);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.exp > 0)
    {
        attr = (p_ptr->new_exp >= preview->cost.exp) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Exp", preview->cost.exp);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }

    strnfmt(buf, sizeof(buf), "%d Turns", preview->turns);
    smith_ui_put_cost_line(costs, TERM_SLATE, buf);
}

static int reforge_prefix_menu(const object_type* source)
{
    char ch;
    char buf[80];
    int i;
    int highlight = 1;
    int entry_count = 0;
    int choice[26];
    bool valid[26];
    reforge_preview_type previews[26];

    if (!source || !source->k_idx)
        return 0;

    screen_save();

    while (true)
    {
        wipe_screen_from(indexed_menu_prefix_col(COL_SMT2));
        Term_putstr(COL_SMT2, 1, -1, TERM_WHITE, "Select prefix:");

        entry_count = 0;
        memset(choice, 0, sizeof(choice));
        memset(valid, 0, sizeof(valid));
        memset(previews, 0, sizeof(previews));

        for (i = 1; i < z_info->e_max && entry_count < (int)N_ELEMENTS(choice); i++)
        {
            char ego_label[64];

            if (!ego_prefix_can_apply_to_object(source, i))
                continue;
            if (!reforge_preview_build(source, i, &previews[entry_count]))
                continue;

            valid[entry_count] = previews[entry_count].affordable;
            choice[entry_count] = i;

            ego_name_for_enchant_menu(i, ego_label, sizeof(ego_label));
            indexed_menu_entry_label(buf, sizeof(buf), entry_count, ego_label);
            smith_ui_put_menu_label(COL_SMT2, entry_count + 2,
                valid[entry_count] ? TERM_WHITE : TERM_L_DARK, buf);
            entry_count++;
        }

        if (entry_count == 0)
        {
            Term_putstr(COL_SMT2, 3, -1, TERM_L_DARK,
                "(No legal prefixes available.)");
            Term_fresh();
            hide_cursor = true;
            (void)inkey();
            hide_cursor = false;
            screen_load();
            return 0;
        }

        if (highlight < 1) highlight = 1;
        if (highlight > entry_count) highlight = entry_count;

        smith_ui_put_menu_prefix(COL_SMT2, highlight + 1, highlight - 1,
            TERM_L_BLUE, true);

        (void)reforge_preview_build(source, choice[highlight - 1],
            &previews[highlight - 1]);
        prt_reforge_preview(&previews[highlight - 1]);
        prt_object_description();

        Term_fresh();
        Term_gotoxy(14, 1 + highlight);

        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        if (!steamdeck_controls_active()
            && (ch >= 'a') && (ch <= (char)'a' + entry_count - 1))
        {
            highlight = (int)ch - 'a' + 1;
            if (!valid[highlight - 1])
                bell("You cannot afford that reforge.");
            else
            {
                screen_load();
                return choice[highlight - 1];
            }
        }
        else if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if (!valid[highlight - 1])
                bell("You cannot afford that reforge.");
            else
            {
                screen_load();
                return choice[highlight - 1];
            }
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            screen_load();
            return 0;
        }
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (highlight > 1)
                highlight--;
            else
                highlight = entry_count;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (highlight < entry_count)
                highlight++;
            else
                highlight = 1;
        }
    }
}

static void create_special(int ego_prefix, int ego_suffix)
{
    /* Retrieve a backup of the object */
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    /* Suffix egos marked NO_PREFIX cannot be combined with any prefix. */
    if (ego_forbids_prefix_combo(ego_suffix))
        ego_prefix = 0;

    /* Apply requested ego affixes */
    object_set_ego_prefix(smith_o_ptr, ego_prefix);
    object_set_ego_suffix(smith_o_ptr, ego_suffix);

    /* Apply ego bonuses */
    if (object_has_ego(smith_o_ptr))
        object_into_special(smith_o_ptr, p_ptr->skill_use[S_SMT], true);

    /* Re-evaluate stack size now that an enchantment is applied */
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

static bool enchant_menu_has_applicable_affix(const object_type* base_o_ptr,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix)
{
    int i;

    if (!base_o_ptr || !smith_o_ptr || base_o_ptr->tval == 0)
        return false;
    if (object_has_evil_alignment(smith_o_ptr))
        return false;

    for (i = 1; i < z_info->e_max; i++)
    {
        if (smith_ego_can_apply_to_object(
                base_o_ptr, i, fixed_prefix, fixed_suffix, selecting_prefix))
            return true;
    }

    return false;
}

/*
 * Performs the interface and selection work for the enchantment menu.
 */
static int enchant_menu_aux(int* highlight, int fixed_prefix, int fixed_suffix,
    bool selecting_prefix, const object_type* base_o_ptr)
{
    char ch;
    int i;
    int entry_count = 0;
    char buf[80];
    bool valid[26];
    int choice[26];

    // clear the right of the screen
    wipe_screen_from(indexed_menu_prefix_col(COL_SMT2));

    /* Header */
    Term_putstr(COL_SMT2, 1, -1, TERM_WHITE,
        selecting_prefix ? "Select prefix:" : "Select suffix:");

    /* Always allow selecting no affix */
    valid[entry_count] = true;
    choice[entry_count] = 0;
    indexed_menu_entry_label(buf, sizeof(buf), entry_count, "(none)");
    smith_ui_put_menu_label(COL_SMT2, entry_count + 2, TERM_WHITE, buf);
    entry_count++;

    /* Suffix egos marked NO_PREFIX only allow "(none)" as the prefix choice. */
    if (selecting_prefix && ego_forbids_prefix_combo(fixed_suffix))
    {
        Term_putstr(COL_SMT2, entry_count + 2, -1, TERM_SLATE,
            "(no prefix allowed with this suffix)");
    }

    /* We have to search the whole special item list. */
    for (i = 1; i < z_info->e_max && entry_count < (int)N_ELEMENTS(choice); i++)
    {
        if (smith_ego_can_apply_to_object(
                base_o_ptr, i, fixed_prefix, fixed_suffix, selecting_prefix))
        {
            /* Make a preview 'special' version of the object */
            if (selecting_prefix)
                create_special(i, fixed_suffix);
            else
                create_special(fixed_prefix, i);

            // Check whether it is a valid choice for creating
            if (affordable(smith_o_ptr))
            {
                valid[entry_count] = true;
            }
            else
            {
                valid[entry_count] = false;
            }

            /* Print it */
            char ego_label[64];
            ego_name_for_enchant_menu(i, ego_label, sizeof(ego_label));
            indexed_menu_entry_label(buf, sizeof(buf), entry_count, ego_label);
            smith_ui_put_menu_label(COL_SMT2, entry_count + 2,
                valid[entry_count] ? TERM_WHITE : TERM_SLATE, buf);

            /* Remember the object index */
            choice[entry_count] = i;

            // count the applicable items
            entry_count++;
        }
    }

    if (*highlight < 1) *highlight = 1;
    if (*highlight > entry_count) *highlight = entry_count;

    // highlight the label
    smith_ui_put_menu_prefix(COL_SMT2, *highlight + 1, *highlight - 1,
        TERM_L_BLUE, true);

    /* Make a preview 'special' version of the object */
    if (selecting_prefix)
        create_special(choice[*highlight - 1], fixed_suffix);
    else
        create_special(fixed_prefix, choice[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    /* Choose by letter */
    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + entry_count - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        /* Make a preview 'special' version of the object */
        if (selecting_prefix)
            create_special(choice[*highlight - 1], fixed_suffix);
        else
            create_special(fixed_prefix, choice[*highlight - 1]);

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
        || (ch == ARROW_RIGHT)
#endif
        )
    {
        return (*highlight);
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE)
#ifdef ARROW_LEFT
        || (ch == ARROW_LEFT)
#endif
        )
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8'
#ifdef ARROW_UP
        || (ch == ARROW_UP)
#endif
        )
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = entry_count;
    }

    /* Next item */
    if (ch == '2'
#ifdef ARROW_DOWN
        || (ch == ARROW_DOWN)
#endif
        )
    {
        if (*highlight < entry_count)
            (*highlight)++;
        else if (*highlight == entry_count)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu for making an item into a {special} item.
 */
bool enchant_menu(void)
{
    int prefix_highlight = 1;
    int suffix_highlight = 1;

    bool completed = false;
    bool leave_menu = false;

    /* Save screen */
    screen_save();

    // stop the item being an artefact, if it was
    smith_o_ptr->name1 = 0;
    smith2_o_ptr->name1 = 0;

    int selected_prefix = (int)object_ego_prefix(smith_o_ptr);
    int selected_suffix = (int)object_ego_suffix(smith_o_ptr);

    bool show_prefix_step =
        enchant_menu_has_applicable_affix(
            smith2_o_ptr, 0, selected_suffix, true) || (selected_prefix != 0);
    bool show_suffix_step =
        enchant_menu_has_applicable_affix(
            smith2_o_ptr, selected_prefix, 0, false) || (selected_suffix != 0);

    if (!show_prefix_step && !show_suffix_step)
    {
        /* Nothing to select; bail out without changing the item. */
        screen_load();
        return false;
    }

    bool selecting_prefix = show_prefix_step;

    /* Process events until menu is abandoned */
    while (!leave_menu)
    {
        if (selecting_prefix)
        {
            int choice_idx = enchant_menu_aux(
                &prefix_highlight, 0, selected_suffix, true, smith2_o_ptr);

            if (choice_idx == -1)
            {
                completed = false;
                leave_menu = true;
                continue;
            }

            if (choice_idx >= 1)
            {
                selected_prefix = (int)object_ego_prefix(smith_o_ptr);
                create_special(selected_prefix, selected_suffix);

                if (show_suffix_step)
                {
                    selecting_prefix = false;
                    continue;
                }

                completed = true;
                leave_menu = true;
                continue;
            }
        }
        else
        {
            int choice_idx = enchant_menu_aux(
                &suffix_highlight, selected_prefix, 0, false, smith2_o_ptr);

            if (choice_idx == -1)
            {
                if (show_prefix_step)
                {
                    /* Back to prefix selection */
                    create_special(selected_prefix, selected_suffix);
                    selecting_prefix = true;
                    continue;
                }

                completed = false;
                leave_menu = true;
                continue;
            }

            if (choice_idx >= 1)
            {
                selected_suffix = (int)object_ego_suffix(smith_o_ptr);
                create_special(selected_prefix, selected_suffix);
                completed = true;
                leave_menu = true;
                continue;
            }
        }
    }

    /* Load screen */
    screen_load();

    return (completed);
}

/*
 * Copies an artefact structure over the top of another one.
 */
void artefact_copy(artefact_type* a1_ptr, artefact_type* a2_ptr)
{
    /* Copy the structure */
    memcpy(a1_ptr, a2_ptr, sizeof(artefact_type));
}

/*
 * Fills in the details on the artefact type being created.
 */
void add_artefact_details(void)
{
    smith_a_ptr->tval = smith_o_ptr->tval;
    smith_a_ptr->sval = smith_o_ptr->sval;
    smith_a_ptr->pval = smith_o_ptr->pval;
    smith_a_ptr->att = smith_o_ptr->att;
    smith_a_ptr->evn = smith_o_ptr->evn;
    smith_a_ptr->dd = smith_o_ptr->dd;
    smith_a_ptr->ds = smith_o_ptr->ds;
    smith_a_ptr->pd = smith_o_ptr->pd;
    smith_a_ptr->ps = smith_o_ptr->ps;
    smith_a_ptr->weight = smith_o_ptr->weight;
    smith_a_ptr->flags1 |= (&k_info[smith_o_ptr->k_idx])->flags1;
    smith_a_ptr->flags2 |= (&k_info[smith_o_ptr->k_idx])->flags2;
    smith_a_ptr->flags3 |= (&k_info[smith_o_ptr->k_idx])->flags3;

    memcpy(smith_a_ptr->stat_bonus, smith_o_ptr->stat_bonus, sizeof(smith_a_ptr->stat_bonus));
    memcpy(smith_a_ptr->skill_bonus, smith_o_ptr->skill_bonus, sizeof(smith_a_ptr->skill_bonus));
    memset(smith_a_ptr->stat_bonus_set, 0, sizeof(smith_a_ptr->stat_bonus_set));
    memset(smith_a_ptr->skill_bonus_set, 0, sizeof(smith_a_ptr->skill_bonus_set));

    smith_a_ptr->cur_num = 1;
    smith_a_ptr->found_num = 1;
    smith_a_ptr->spawn_num = 1;
    smith_a_ptr->level = object_difficulty(smith_o_ptr);
    smith_a_ptr->rarity = 10;
}

/*
 * Prepares an artefact for modification.
 */
void prepare_artefact(void)
{
    int i;

    log_debug("Preparing artifact for modification");

    // retrieve a backup of the artefact
    artefact_copy(smith_a_ptr, smith2_a_ptr);

    // retrieve a backup of the object
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    // set its 'artefact' name to reflect the chosen type
    smith_o_ptr->name1 = smith_a_name;

    // Restore default stack sizes for arrows and other throwable gear
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;

    log_trace("Artifact preparation complete - %d abilities synchronized", smith_a_ptr->abilities);
}

/*
 * Does the given object type support the given flag type?
 */
bool applicable_flag(u32b f, int flagset, object_type* o_ptr)
{
    bool ok = false;
    int i;
    u32b f1, f2, f3, f4;

    /* Telchar may always put SHARPNESS II on a melee weapon               */
    if ((flagset == 1) && (f == TR1_SHARPNESS2) &&
        (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
    {
        switch (smith_o_ptr->tval)                   /* any melee weapon   */
        {
            case TV_SWORD: case TV_HAFTED:
            case TV_POLEARM: case TV_DIGGING:
                return true;
        }
    }

    /* Extract the object flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* Warhammers-only: Smithing bonus requires Brand Fire on the same item. */
    if ((flagset == 1) && (f == TR1_SMT))
    {
        if (o_ptr->tval != TV_HAFTED || o_ptr->sval != SV_WAR_HAMMER)
            return false;
        if (!(f1 & TR1_BRAND_FIRE))
            return false;
        return true;
    }

    /* Go through the list of artefacts and see if the flag is applicable for
     * this type  */
    for (i = ART_ULTIMATE; i < z_info->art_norm_max; i++)
    {
        /* Access the artefact */
        artefact_type* a_ptr = &a_info[i];

        /* Skip other types of artefacts */
        if (a_ptr->tval != o_ptr->tval)
            continue;

        switch (flagset)
        {
        case 1:
        {
            if (a_ptr->flags1 & f)
                ok = true;
            break;
        }
        case 2:
        {
            if (a_ptr->flags2 & f)
                ok = true;
            break;
        }
        case 3:
        {
            if (a_ptr->flags3 & f)
                ok = true;
            break;
        }
        case 4:
        {
            if (a_ptr->flags4 & f)
                ok = true;
            break;
        }
        }
    }

    return (ok);
}

/*
 * Adds a given flag to the dummy artefact.
 */
void add_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Adding artifact flag %u in flagset %d", f, flagset);
    
    // prepare the artefact and object for modification
    prepare_artefact();

    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    // set new flag on the artefact
    if (flagset == 1)
        smith_a_ptr->flags1 |= f;
    if (flagset == 2)
        smith_a_ptr->flags2 |= f;
    if (flagset == 3)
        smith_a_ptr->flags3 |= f;
    if (flagset == 4)
        smith_a_ptr->flags4 |= f;

    object_flags(smith_o_ptr, &f1_after, &f2, &f3);
    smith_apply_stat_skill_flag_delta(smith_o_ptr, f1_before, f1_after);
}

/*
 * Removes a given flag from the dummy artefact.
 */
void remove_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Removing artifact flag %u from flagset %d", f, flagset);
    
    // prepare the artefact and object for modification
    prepare_artefact();

    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    // unset new flag on the artefact
    if (flagset == 1)
        smith_a_ptr->flags1 &= ~(f);
    if (flagset == 2)
        smith_a_ptr->flags2 &= ~(f);
    if (flagset == 3)
        smith_a_ptr->flags3 &= ~(f);
    if (flagset == 4)
        smith_a_ptr->flags4 &= ~(f);

    /* Keep Smithing dependent on Brand Fire. */
    if ((flagset == 1) && (f == TR1_BRAND_FIRE))
        smith_a_ptr->flags1 &= ~(TR1_SMT);

    object_flags(smith_o_ptr, &f1_after, &f2, &f3);
    smith_apply_stat_skill_flag_delta(smith_o_ptr, f1_before, f1_after);
}

/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_flag_menu_aux(int category, int* highlight)
{
    char ch;
    int i, num = 0;
    char buf[80];
    bool flag_present[MAX_SMITHING_FLAGS] = { false };
    bool flag_valid[MAX_SMITHING_FLAGS] = { false };
    bool flag_affordable[MAX_SMITHING_FLAGS] = { false };
    u32b flag[MAX_SMITHING_FLAGS];
    int flagset[MAX_SMITHING_FLAGS];
    byte attr;

    // clear the right of the screen
    wipe_screen_from(indexed_menu_prefix_col(COL_SMT3));

    // display the categories
    for (i = 0; smithing_flag_types[i].flag != 0; i++)
    {
        if (category == smithing_flag_types[i].category)
        {
            /* Telchar-only: skip Sharpness2 if not in character Telchar */
            if ((smithing_flag_types[i].flagset == 1) &&
                (smithing_flag_types[i].flag == TR1_SHARPNESS2) &&
                !(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
            {
                /* don't even consider it */
                continue;
            }
            flag[num] = smithing_flag_types[i].flag;
            flagset[num] = smithing_flag_types[i].flagset;

            if (((flagset[num] == 1) && (smith2_a_ptr->flags1 & flag[num]))
                || ((flagset[num] == 2) && (smith2_a_ptr->flags2 & flag[num]))
                || ((flagset[num] == 3) && (smith2_a_ptr->flags3 & flag[num]))
                || ((flagset[num] == 4) && (smith2_a_ptr->flags4 & flag[num])))
            {
                flag_present[num] = true;
                flag_valid[num] = true;
            }

            else
            {
                // require that the flag can be present on the object
                if (applicable_flag(flag[num], flagset[num], smith_o_ptr))
                {
                    flag_valid[num] = true;

                    // add this flag to the dummy artefact under construction
                    add_artefact_flag(flag[num], flagset[num]);

                    // Check whether it is a valid choice for creating (needs to
                    // be affordable and successful)
                    if (affordable(smith_o_ptr))
                    {
                        flag_affordable[num] = true;
                    }
                }
            }

        // /* Lock Sharpness II behind Telchar forge */
        // if (flag[num] == TR1_SHARPNESS2 &&
        //     !(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
        //     flag_valid[num] = false;

            attr = flag_present[num]
                ? TERM_BLUE
                : (flag_valid[num]
                        ? (flag_affordable[num] ? TERM_WHITE : TERM_SLATE)
                        : TERM_L_DARK);

            /* Display the line */
            indexed_menu_entry_label(buf, sizeof(buf), num,
                smithing_flag_types[i].desc);
            smith_ui_put_menu_label(COL_SMT3, num + 2, attr, buf);

            num++;
        }
    }

    // highlight the label
    smith_ui_put_menu_prefix(COL_SMT3, *highlight + 1, *highlight - 1,
        TERM_L_BLUE, true);

    // add this flag to the dummy artefact under construction
    add_artefact_flag(flag[*highlight - 1], flagset[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    /* Abort if there are no choices */
    if (num == 0)
    {
        return (-1);
    }

    /* Choose by letter */
    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int new_highlight = (int)ch - 'a' + 1;

        if (flag_valid[new_highlight - 1])
        {
            if (new_highlight == *highlight)
            {
                // remove a flag if it already existed
                if (flag_present[*highlight - 1])
                    remove_artefact_flag(
                        flag[*highlight - 1], flagset[*highlight - 1]);
            }
            else
            {
                // restore the artefact from backup
                artefact_copy(smith_a_ptr, smith2_a_ptr);

                *highlight = new_highlight;

                // remove a flag if it already existed
                if (flag_present[*highlight - 1])
                    remove_artefact_flag(
                        flag[*highlight - 1], flagset[*highlight - 1]);

                // otherwise add it
                else
                    add_artefact_flag(
                        flag[*highlight - 1], flagset[*highlight - 1]);
            }

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;

            return (*highlight);
        }
        else
        {
            bell("Invalid choice.");
        }
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (flag_valid[*highlight - 1])
        {
            // remove a flag if it already existed
            if (flag_present[*highlight - 1])
                remove_artefact_flag(
                    flag[*highlight - 1], flagset[*highlight - 1]);

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;

            return (*highlight);
        }

        else
        {
            bell("Invalid choice.");
        }
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;

        // restore the backup artefact and object
        prepare_artefact();

        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu to select individual flags of a given type to
 * add to (or subtract from) an artefact.
 */
void artefact_flag_menu(int category)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_flag_menu_aux(category, &highlight);

        if (choice >= 1)
        {
            // don't leave the menu
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();
}

/*
 * Can this ability be applied to any item at all?
 * Returns false for stat-only abilities like Grace/Strength/etc that have no valid item types.
 */
static bool ability_can_be_smithed(ability_type* b_ptr)
{
    int j;

    /* Check if this ability has any valid item types defined */
    for (j = 0; j < ABILITY_TVALS_MAX; j++)
    {
        if (b_ptr->tval[j] != 0)
            return true;
    }

    return false;
}

/*
 * Does the given object type support the given ability type?
 */
bool applicable_ability(ability_type* b_ptr, object_type* o_ptr)
{
    bool ok = false;
    int j;

    u32b f1, f2, f3;

    /* Test if this is a legal item type for this ability */
    for (j = 0; j < ABILITY_TVALS_MAX; j++)
    {
        /* Require identical base type */
        if (o_ptr->tval == b_ptr->tval[j])
        {
            /* Require sval in bounds, lower */
            if (o_ptr->sval >= b_ptr->min_sval[j])
            {
                /* Require sval in bounds, upper */
                if (o_ptr->sval <= b_ptr->max_sval[j])
                {
                    /* Accept */
                    ok = true;
                }
            }
        }
    }

    // Polearm Mastery is OK for Polearms
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_POLEARM)
    {
        if ((b_ptr->skilltype == S_MEL) && (b_ptr->abilitynum == MEL_POLEARMS))
            ok = true;
    }

    return (ok);
}

/*
 * Adds a given ability to the dummy artefact.
 */
void add_artefact_ability(int skilltype, int abilitynum)
{
    int i;

    log_trace("Adding artifact ability - skill:%d ability:%d", skilltype, abilitynum);

    // prepare the artefact and object for modification
    prepare_artefact();

    // set new ability on the artefact
    if (smith_a_ptr->abilities < 4)
    {
        bool already_present = false;

        for (i = 0; i < smith_a_ptr->abilities; i++)
        {
            if ((smith_a_ptr->skilltype[i] == skilltype)
                && (smith_a_ptr->abilitynum[i] == abilitynum))
            {
                already_present = true;
            }
        }

        if (!already_present)
        {
            smith_a_ptr->skilltype[smith_a_ptr->abilities] = skilltype;
            smith_a_ptr->abilitynum[smith_a_ptr->abilities] = abilitynum;
            smith_a_ptr->bane_type[smith_a_ptr->abilities] = 0; // Player-smithed banes use player choice
            smith_a_ptr->abilities++;
        }
    }

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

/*
 * Removes a given ability from the dummy artefact.
 */
void remove_artefact_ability(int skilltype, int abilitynum)
{
    int i;
    int location = -1;

    log_trace("Removing artifact ability - skill:%d ability:%d", skilltype, abilitynum);

    // prepare the artefact and object for modification
    prepare_artefact();

    // remove new ability on the artefact
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        if ((smith_a_ptr->skilltype[i] == skilltype)
            && (smith_a_ptr->abilitynum[i] == abilitynum))
        {
            location = i;
        }
    }

    if (location >= 0)
    {
        for (i = location; i < smith_a_ptr->abilities - 1; i++)
        {
            smith_a_ptr->skilltype[i] = smith_a_ptr->skilltype[i + 1];
            smith_a_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i + 1];
            smith_a_ptr->bane_type[i] = smith_a_ptr->bane_type[i + 1];
        }

        smith_a_ptr->skilltype[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->abilitynum[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->bane_type[smith_a_ptr->abilities - 1] = 0;

        smith_a_ptr->abilities--;
    }

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

/*
 * Determines if an artefact type has a given ability.
 */
bool has_ability(artefact_type* a_ptr, int skilltype, int abilitynum)
{
    int i;

    for (i = 0; i < a_ptr->abilities; i++)
    {
        if ((a_ptr->skilltype[i] == skilltype)
            && (a_ptr->abilitynum[i] == abilitynum))
            return (true);
    }

    return (false);
}

/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_ability_menu_aux(int skill, int* highlight)
{
    char ch;
    int i, num = 0;
    char buf[80];
    ability_type* b_ptr;
    byte attr;
    
    /* Allocate arrays dynamically based on actual max abilities */
    bool* ability_present = mem_alloc_array(z_info->b_max, bool);
    bool* ability_valid = mem_alloc_array(z_info->b_max, bool);
    bool* ability_affordable = mem_alloc_array(z_info->b_max, bool);
    int* ability_nums = mem_alloc_array(z_info->b_max, int);
    
    /* Initialize arrays to zero/false */
    memset(ability_present, 0, z_info->b_max * sizeof(bool));
    memset(ability_valid, 0, z_info->b_max * sizeof(bool));
    memset(ability_affordable, 0, z_info->b_max * sizeof(bool));

    // clear the right of the screen
    wipe_screen_from(indexed_menu_prefix_col(COL_SMT3));

    // list the abilities
    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skill)
            continue;

        /* Skip abilities that can't be smithed onto any item (like Grace, stat improvements) */
        if (!ability_can_be_smithed(b_ptr))
            continue;

        // Store the mapping from display index to actual ability number
        ability_nums[num] = b_ptr->abilitynum;

        // Determine the appropriate colour
        if (has_ability(smith2_a_ptr, skill, b_ptr->abilitynum))
        {
            ability_present[num] = true;
            ability_valid[num] = true;
        }
        else
        {
            // require that the ability can be present on the object
            if (applicable_ability(b_ptr, smith_o_ptr))
            {
                ability_valid[num] = true;

                // add this flag to the dummy artefact under construction
                add_artefact_ability(skill, b_ptr->abilitynum);

                // require that the ability was successfully added
                if (has_ability(smith_a_ptr, skill, b_ptr->abilitynum))
                {
                    // Check whether it is a valid choice for creating (needs to
                    // be affordable and successful)
                    if (affordable(smith_o_ptr))
                    {
                        ability_affordable[num] = true;
                    }
                }

                // if the ability wasn't added properly (the item had too many),
                // then it is not valid after all
                else
                {
                    ability_valid[num] = false;
                }
            }
        }

        attr = ability_present[num]
            ? TERM_BLUE
            : (ability_valid[num]
                    ? (ability_affordable[num] ? TERM_WHITE : TERM_SLATE)
                    : TERM_L_DARK);

        /* Display the line */
        indexed_menu_entry_label(buf, sizeof(buf), num, b_name + b_ptr->name);
        smith_ui_put_menu_label(COL_SMT3, num + 2, attr, buf);

        num++;
    }

    // highlight the label
    smith_ui_put_menu_prefix(COL_SMT3, *highlight + 1, *highlight - 1,
        TERM_L_BLUE, true);

    // add this ability to the dummy artefact under construction (use actual ability number)
    add_artefact_ability(skill, ability_nums[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    /* Abort if there are no choices */
    if (num == 0)
    {
        return (-1);
    }

    /* Choose by letter */
    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int new_highlight = (int)ch - 'a' + 1;

        if (ability_valid[new_highlight - 1])
        {
            if (new_highlight == *highlight)
            {
                // remove an ability if it already existed
                if (ability_present[*highlight - 1])
                    remove_artefact_ability(skill, ability_nums[*highlight - 1]);
            }
            else
            {
                // restore the artefact from backup
                artefact_copy(smith_a_ptr, smith2_a_ptr);

                *highlight = new_highlight;

                // remove an ability if it already existed
                if (ability_present[*highlight - 1])
                    remove_artefact_ability(skill, ability_nums[*highlight - 1]);

                // otherwise add it
                else
                    add_artefact_ability(skill, ability_nums[*highlight - 1]);
            }

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);

            mem_free(ability_present);
            mem_free(ability_valid);
            mem_free(ability_affordable);
            mem_free(ability_nums);
            return (*highlight);
        }
        else
        {
            bell("Invalid choice.");
        }
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (ability_valid[*highlight - 1])
        {
            // remove an ability if it already existed
            if (ability_present[*highlight - 1])
                remove_artefact_ability(skill, ability_nums[*highlight - 1]);

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);

            mem_free(ability_present);
            mem_free(ability_valid);
            mem_free(ability_affordable);
            mem_free(ability_nums);
            return (*highlight);
        }
        else
        {
            bell("Invalid choice.");
        }
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        // remove any tentatively-added ability from the object
        if (!ability_present[*highlight - 1])
            remove_artefact_ability(skill, ability_nums[*highlight - 1]);

        // restore the backup artefact
        artefact_copy(smith_a_ptr, smith2_a_ptr);

        *highlight = -1;

        mem_free(ability_present);
        mem_free(ability_valid);
        mem_free(ability_affordable);
        mem_free(ability_nums);
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    mem_free(ability_present);
    mem_free(ability_valid);
    mem_free(ability_affordable);
    mem_free(ability_nums);
    return (0);
}

/*
 * Brings up a menu to select individual abilities of a given skill to
 * add to (or subtract from) an artefact.
 */
void artefact_ability_menu(int skill)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_ability_menu_aux(skill, &highlight);

        if (choice >= 1)
        {
            // don't leave the menu
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();
}

/*
 * Allows the player to choose a new name for an artefact.
 */
void rename_artefact(void)
{
    char tmp[20];
    char old_name[20];
    char o_desc[30];
    bool name_selected = false;
    int row = (smith_ui_last_desc_row >= 0) ? smith_ui_last_desc_row
                                            : (smith_ui_term_hgt() - 1);
    int col = smith_ui_desc_col();

    // Clear the names
    tmp[0] = '\0';
    old_name[0] = '\0';

    // Clear object name
    Term_erase(0, row, smith_ui_term_wid());

    // Determine object name
    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, false, -1);

    // Display shortened object name
    Term_putstr(col, row, smith_ui_term_wid() - col, TERM_L_WHITE, o_desc);

    // use old name as a default
    SDL_strlcpy(tmp, smith2_a_ptr->name, sizeof(tmp));

    // save a copy too
    SDL_strlcpy(old_name, op_ptr->full_name, sizeof(old_name));

    /* Prompt for a new name */
    Term_gotoxy(col + strlen(o_desc) + 1, row);

    while (!name_selected)
    {
        if (askfor_name(tmp, sizeof(tmp)))
        {
            SDL_strlcpy(smith2_a_ptr->name, tmp, MAX_LEN_ART_NAME);
            p_ptr->redraw |= (PR_MISC);
        }
        else
        {
            SDL_strlcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
            return;
        }

        if (tmp[0] != '\0')
            name_selected = true;
        else
            SDL_strlcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
    }

    // retrieve a backup of the artefact (all the modifications were done to
    // this backup copy)
    artefact_copy(smith_a_ptr, smith2_a_ptr);
}

/*
 * Performs the interface and selection work for the 1st level artefact menu.
 */
int artefact_menu_aux(int* highlight)
{
    char ch;
    int i, num;
    char buf[80];

    // clear the right of the screen
    wipe_screen_from(indexed_menu_prefix_col(COL_SMT2));

    // display the categories for flags
    for (i = 0; i < MAX_CATS; i++)
    {
        indexed_menu_entry_label(buf, sizeof(buf), i, smithing_flag_cats[i].desc);
        smith_ui_put_menu_label(COL_SMT2, smith_ui_dense_row(i), TERM_WHITE,
            buf);
    }

    // display the categories for abilities (skip Special abilities - S_SPC)
    int display_idx = 0;
    for (i = 0; i < S_MAX; i++)
    {
        /* Skip Special abilities - they cannot be smithed onto items */
        if (i == S_SPC) continue;
        
        indexed_menu_entry_label(buf, sizeof(buf), MAX_CATS + display_idx,
            skill_names_full[i]);
        smith_ui_put_menu_label(COL_SMT2,
            smith_ui_dense_row(MAX_CATS + display_idx), TERM_WHITE, buf);
        display_idx++;
    }

    num = MAX_CATS + display_idx + 1;

    // Menu item for naming artefacts
    indexed_menu_entry_label(buf, sizeof(buf), num - 1, "Name Artefact");
    smith_ui_put_menu_label(COL_SMT2, smith_ui_dense_row(num - 1), TERM_WHITE,
        buf);

    // highlight the label
    smith_ui_put_menu_prefix(COL_SMT2,
        smith_ui_dense_highlight_row(*highlight), *highlight - 1,
        TERM_L_BLUE, true);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, smith_ui_dense_highlight_row(*highlight));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    /* Choose by letter */
    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        smith_ui_put_menu_prefix(COL_SMT2,
            smith_ui_dense_highlight_row(old_highlight), old_highlight - 1,
            TERM_WHITE, false);
        smith_ui_put_menu_prefix(COL_SMT2,
            smith_ui_dense_highlight_row(*highlight), *highlight - 1,
            TERM_L_BLUE, true);

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu for making a base item into an artefact,
 * by adding flags of various types.
 */
void artefact_menu(void)
{
    int choice = -1;
    int highlight = 1;

    char buf[36];
    bool leave_menu = false;

    log_info("Player opened artifact creation menu");

    /* Save screen */
    screen_save();

    if (!smith_o_ptr->name1)
    {
        log_debug("Initializing new artifact creation");
        // wipe the existing artefact (and its backup)
        artefact_wipe(smith_a_name);
        artefact_wipe(smith2_a_name);

        // add 'ignore all'
        smith2_a_ptr->flags3 |= (TR3_IGNORE_MASK);

        // change the SV for rings and amulets when they start to get made into
        // artefacts
        if (smith_o_ptr->tval == TV_RING)
        {
            create_base_object(TV_RING, SV_RING_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
        }
        if (smith_o_ptr->tval == TV_AMULET)
        {
            create_base_object(TV_AMULET, SV_AMULET_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
            smith2_o_ptr->pd = 1;
        }
    }

    // set the backup artefact name to the player character's name
    if (strlen(smith2_a_ptr->name) == 0)
    {
        sprintf(buf, "of %s", op_ptr->full_name);
        SDL_strlcpy(smith2_a_ptr->name, buf, MAX_LEN_ART_NAME);
    }

    // prepare the artefact and object for modification
    prepare_artefact();

    /* Number of skill categories displayed (S_MAX minus Special abilities) */
    int num_skills = S_MAX - 1;

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_menu_aux(&highlight);

        if (choice == MAX_CATS + num_skills + 1)
        {
            rename_artefact();
        }
        else if (choice >= MAX_CATS + 1)
        {
            artefact_ability_menu(choice - MAX_CATS - 1);
        }
        else if (choice >= 1)
        {
            artefact_flag_menu(choice);
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();

    return;
}

/*
 * Performs the interface and selection work for the melting menu.
 */
int melt_menu_aux(int* highlight)
{
    char ch;
    int i;
    int num = 0;
    object_type* o_ptr;
    u32b f1, f2, f3;
    char desc[80];
    char buf[80];

    // clear the right of the screen
    wipe_screen_from(indexed_menu_prefix_col(COL_SMT2));

    // clear bottom of the screen
    wipe_object_description();

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        object_flags(o_ptr, &f1, &f2, &f3);
        
        /* ignore metal items that carry the "can't melt" tag */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON)) && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            object_desc(desc, 80, o_ptr, false, 2);
            indexed_menu_entry_label(buf, sizeof(buf), num, desc);

            smith_ui_put_menu_label(COL_SMT2, smith_ui_dense_row(num),
                TERM_WHITE, buf);

            if (smith_ui_weight_col() > 0)
            {
                strnfmt(buf, 80, "%2d.%d lb", o_ptr->weight / 10,
                    o_ptr->weight % 10);
                Term_putstr(smith_ui_weight_col(), smith_ui_dense_row(num), -1,
                    TERM_WHITE, buf);
            }

            num++;
        }
    }

    // highlight the label
    smith_ui_put_menu_prefix(COL_SMT2,
        smith_ui_dense_highlight_row(*highlight), *highlight - 1,
        TERM_L_BLUE, true);

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, smith_ui_dense_highlight_row(*highlight));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    // choose an option by letter
    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(
            old_highlight, TERM_WHITE, *highlight, COL_SMT2);

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        return (-1);
    }

    return (0);
}

/*
 * Produces the menu for melting down mithril and star-iron items into their metal pieces.
 */
void melt_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = melt_menu_aux(&highlight);

        if (choice >= 1)
        {
            if (melt_metal_item(choice))
            {
                leave_menu = true;
            }
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();
}

static bool smith_item_tester_hook_reforge_target(const object_type* o_ptr)
{
    return object_can_repair_damage(o_ptr) || object_can_reforge_prefix(o_ptr);
}

static bool smith_reforge_item(void)
{
    int slot = -1;
    int prefix_idx = 0;
    char old_name[80];
    char new_name[80];
    object_type smith_backup;
    object_type smith2_backup;
    smith_alloy_state alloy_backup = smith_alloy;
    smith_alloy_state alloy2_backup = smith2_alloy;

    if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        msg_print("You can only reforge items at a forge.");
        return false;
    }

    if (forge_uses(p_ptr->py, p_ptr->px) <= 0)
    {
        msg_print("This forge has no resources left.");
        return false;
    }

    if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
    {
        bell("You do not know how to reforge gear.");
        return false;
    }

    item_tester_hook = smith_item_tester_hook_reforge_target;
    if (!get_item(&slot, "Reforge which item? ",
            "You have nothing to repair or reforge.", (USE_EQUIP | USE_INVEN)))
    {
        item_tester_hook = NULL;
        return false;
    }
    item_tester_hook = NULL;

    if (slot < 0)
        return false;

    object_copy(&smith_backup, smith_o_ptr);
    object_copy(&smith2_backup, smith2_o_ptr);

    if (object_can_repair_damage(&inventory[slot]))
    {
        if (!repair_damaged_item(slot))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot repair that item.");
            return false;
        }

        cave_feat[p_ptr->py][p_ptr->px] -= 1;
        lite_spot(p_ptr->py, p_ptr->px);

        object_desc(new_name, sizeof(new_name), &inventory[slot], true, 0);
        msg_format("You repair %s.", new_name);
    }
    else
    {
        reforge_preview_type preview;

        if (!object_can_reforge_prefix(&inventory[slot]))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot reforge that item.");
            return false;
        }

        prefix_idx = reforge_prefix_menu(&inventory[slot]);
        if (!prefix_idx)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            return false;
        }

        if (!reforge_preview_build(&inventory[slot], prefix_idx, &preview)
            || !preview.affordable)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot afford that reforge.");
            return false;
        }

        object_desc(old_name, sizeof(old_name), &inventory[slot], true, 0);
        object_set_ego_prefix(&inventory[slot], prefix_idx);
        if (!object_apply_ego_affix(&inventory[slot], prefix_idx, true))
        {
            object_set_ego_prefix(&inventory[slot], 0);
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot reforge that item.");
            return false;
        }

        pay_smithing_cost_struct(&preview.cost);
        inventory[slot].unused1 = 2;
        object_aware(&inventory[slot]);
        object_known(&inventory[slot]);
        object_desc(new_name, sizeof(new_name), &inventory[slot], true, 0);
        msg_format("You reforge %s into %s.", old_name, new_name);
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }

    object_copy(smith_o_ptr, &smith_backup);
    object_copy(smith2_o_ptr, &smith2_backup);
    smith_alloy = alloy_backup;
    smith2_alloy = alloy2_backup;

    p_ptr->redraw |= PR_BASIC;
    return true;
}

/*
 * Performs the interface and selection work for the smithing screen.
 */
int smithing_menu_aux(int* highlight)
{
    char ch;
    byte valid_attr;
    bool valid[SMT_MENU_MAX];
    char buf[80];

    // clear the right of the screen
    wipe_screen_from(indexed_menu_prefix_col(COL_SMT2));

    // determine whether or not we can actually make objects here
    if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        Term_putstr(COL_SMT1, 0, -1, TERM_L_BLUE,
            "Exploration mode:  Smithing requires a forge.");
    }
    else if (forge_uses(p_ptr->py, p_ptr->px) == 0)
    {
        Term_putstr(COL_SMT1, 0, -1, TERM_L_BLUE,
            "Exploration mode:  Smithing requires a forge with resources "
            "left.");
    }

    valid[SMT_MENU_CREATE - 1] = true;
    valid[SMT_MENU_ENCHANT - 1] = (!smith_o_ptr->name1)
        && (!enchant_then_numbers) && (smith_o_ptr->tval != 0)
        && (smith_o_ptr->tval != TV_HORN)
        && !((smith_o_ptr->tval == TV_DIGGING)
            && (smith_o_ptr->sval == SV_SHOVEL));
    valid[SMT_MENU_ARTEFACT - 1] = (!object_has_ego(smith_o_ptr))
        && (smith_o_ptr->tval != 0) && (smith_o_ptr->tval != TV_HORN)
        && (p_ptr->self_made_arts
            < z_info->art_self_made_max - z_info->art_rand_max - 2);
    valid[SMT_MENU_NUMBERS - 1] = (smith_o_ptr->tval != 0);
    valid[SMT_MENU_MELT - 1]
        = meltable_metal_items_carried() && cave_forge_bold(p_ptr->py, p_ptr->px);
    valid[SMT_MENU_REPAIR - 1]
        = cave_forge_bold(p_ptr->py, p_ptr->px)
        && (forge_uses(p_ptr->py, p_ptr->px) > 0)
        && p_ptr->active_ability[S_SMT][SMT_REPAIR]
        && (find_reforge_target_item() >= 0);
    valid[SMT_MENU_ACCEPT - 1] = affordable(smith_o_ptr)
        && cave_forge_bold(p_ptr->py, p_ptr->px)
        && (forge_uses(p_ptr->py, p_ptr->px) > 0);

    // display labels
    valid_attr = (p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        ? TERM_WHITE
        : TERM_RED;
    indexed_menu_entry_label(buf, sizeof(buf), SMT_MENU_CREATE - 1, "Base Item");
    smith_ui_put_menu_label(COL_SMT1, 2,
        valid[SMT_MENU_CREATE - 1] ? valid_attr : TERM_L_DARK, buf);
    valid_attr = (p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT]) ? TERM_WHITE
                                                                 : TERM_RED;
    indexed_menu_entry_label(buf, sizeof(buf), SMT_MENU_ENCHANT - 1, "Enchant");
    smith_ui_put_menu_label(COL_SMT1, 3,
        valid[SMT_MENU_ENCHANT - 1] ? valid_attr : TERM_L_DARK, buf);
    valid_attr
        = (p_ptr->active_ability[S_SMT][SMT_ARTEFACT]) ? TERM_WHITE : TERM_RED;
    indexed_menu_entry_label(buf, sizeof(buf), SMT_MENU_ARTEFACT - 1, "Artifice");
    smith_ui_put_menu_label(COL_SMT1, 4,
        valid[SMT_MENU_ARTEFACT - 1] ? valid_attr : TERM_L_DARK, buf);
    indexed_menu_entry_label(buf, sizeof(buf), SMT_MENU_NUMBERS - 1, "Numbers");
    smith_ui_put_menu_label(COL_SMT1, 5,
        valid[SMT_MENU_NUMBERS - 1] ? TERM_WHITE : TERM_L_DARK, buf);
    indexed_menu_entry_label(buf, sizeof(buf), SMT_MENU_MELT - 1, "Melt");
    smith_ui_put_menu_label(COL_SMT1, 6,
        valid[SMT_MENU_MELT - 1] ? TERM_WHITE : TERM_L_DARK, buf);
    valid_attr = p_ptr->active_ability[S_SMT][SMT_REPAIR] ? TERM_WHITE : TERM_RED;
    indexed_menu_entry_label(buf, sizeof(buf), SMT_MENU_REPAIR - 1, "Reforge");
    smith_ui_put_menu_label(COL_SMT1, 7,
        valid[SMT_MENU_REPAIR - 1] ? valid_attr : TERM_L_DARK, buf);

    if (p_ptr->smithing_leftover == 0)
    {
        indexed_menu_entry_label(buf, sizeof(buf), SMT_MENU_ACCEPT - 1, "Accept");
        smith_ui_put_menu_label(COL_SMT1, 8,
            valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK, buf);
    }
    else
    {
        indexed_menu_entry_label(buf, sizeof(buf), SMT_MENU_ACCEPT - 1, "Resume");
        smith_ui_put_menu_label(COL_SMT1, 8,
            valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK, buf);
    }

    // display information about the selected item
    switch (*highlight)
    {
    case SMT_MENU_CREATE:
    {
        Term_putstr(
            COL_SMT2 + 2, 2, -1, TERM_SLATE, "Start with a new base item.");
        break;
    }
    case SMT_MENU_ENCHANT:
    {
        Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
            "Choose a special enchantment to add");
        Term_putstr(COL_SMT2 + 2, 3, -1, TERM_SLATE, "to the base item.");
        if (smith_o_ptr->name1)
            Term_putstr(COL_SMT2 + 2, 5, -1, TERM_L_DARK,
                "(not compatible with Artifice)");
        if (enchant_then_numbers)
        {
            Term_putstr(COL_SMT2 + 2, 5, -1, TERM_L_DARK,
                "(Enchantment cannot be changed");
            Term_putstr(COL_SMT2 + 2, 6, -1, TERM_L_DARK,
                "after using the Numbers menu)");
        }
        break;
    }
    case SMT_MENU_ARTEFACT:
    {
        Term_putstr(
            COL_SMT2 + 2, 2, -1, TERM_SLATE, "Design your own artefact.");
        if (object_has_ego(smith_o_ptr))
            Term_putstr(COL_SMT2 + 2, 4, -1, TERM_L_DARK,
                "(not compatible with Enchant)");
        break;
    }
    case SMT_MENU_NUMBERS:
    {
        Term_putstr(
            COL_SMT2 + 2, 2, -1, TERM_SLATE, "Change the item's key numbers.");
        break;
    }
    case SMT_MENU_MELT:
    {
        Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
            "Choose a mithril or star-iron item");
        Term_putstr(
            COL_SMT2 + 2, 3, -1, TERM_SLATE, "to melt down.");
        break;
    }
    case SMT_MENU_REPAIR:
    {
        Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
            "Repair damaged gear or add a prefix");
        Term_putstr(COL_SMT2 + 2, 3, -1, TERM_SLATE,
            "to a found item at the forge.");
        Term_putstr(COL_SMT2 + 2, 4, -1, TERM_SLATE,
            "Reforging uses 1.5x the difficulty delta.");
        if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
            Term_putstr(COL_SMT2 + 2, 5, -1, TERM_L_DARK,
                "(requires the Reforging ability)");
        else if (find_reforge_target_item() < 0)
            Term_putstr(COL_SMT2 + 2, 5, -1, TERM_L_DARK,
                "(you carry nothing to reforge)");
        break;
    }
    case SMT_MENU_ACCEPT:
    {
        if (forge_uses(p_ptr->py, p_ptr->px) > 0)
        {
            Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
                "Create the item you have designed.");
            Term_putstr(COL_SMT2 + 2, 4, -1, TERM_SLATE,
                "(to cancel it instead, just press Escape)");
        }
        else if (cave_forge_bold(p_ptr->py, p_ptr->px))
        {
            Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
                "This forge has no resources left, so you");
            Term_putstr(COL_SMT2 + 2, 3, -1, TERM_SLATE,
                "cannot create items. To exit, press Escape.");
        }
        else
        {
            Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
                "You are not at a forge and thus cannot");
            Term_putstr(COL_SMT2 + 2, 3, -1, TERM_SLATE,
                "create items. To exit, press Escape.");
        }
        break;
    }
    }

    // highlight the label
    smith_ui_put_menu_prefix(COL_SMT1, *highlight + 1, *highlight - 1,
        TERM_L_BLUE, true);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    // choose an option by letter
    if (!steamdeck_controls_active()
        && (ch >= 'a') && (ch <= (char)'a' + SMT_MENU_MAX - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(old_highlight,
            valid[old_highlight - 1] ? TERM_WHITE : TERM_L_DARK, *highlight,
            COL_SMT1);

        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = SMT_MENU_MAX;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < SMT_MENU_MAX)
            (*highlight)++;
        else if (*highlight == SMT_MENU_MAX)
            *highlight = 1;
    }

    /* Leave menu */
    if ((ch == ESCAPE) || (ch == '4'))
    {
        return (-1);
    }

    return (0);
}

/*
 * Brings up a screen for making new items (only works at a forge).
 * Leads to many submenus which help to determine the item's attributes.
 */
void do_cmd_smithing_screen(void)
{
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool create = false;

    // if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    //{
    //	msg_print("You can only create items at a forge.");
    //	return;
    //}

    if (cave_forge_bold(p_ptr->py, p_ptr->px)
        && forge_uses(p_ptr->py, p_ptr->px) == 0)
    {
        msg_print("The resources of this forge are exhausted.");
        msg_print(
            "You will be able to browse the options but not make new things.");
    }

    /* Save screen */
    screen_save();

    /* Clear screen */
    Term_clear();
    smith_ui_reset_description_state();

    // Hack: flag that we are in the middle of smithing
    p_ptr->smithing = 1;

    // deal with previous interruptions
    if (p_ptr->smithing_leftover > 0)
    {
        // default to 'resume' if an item is already in progress
        highlight = SMT_MENU_ACCEPT;

        // and backup the smithing item
        object_copy(smith2_o_ptr, smith_o_ptr);
        smith2_alloy = smith_alloy;
    }

    // otherwise wipe the smithing item
    else
    {
        object_wipe(smith_o_ptr);
        smith_clear_alloy_state(&smith_alloy);
    }

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        actiontype = smithing_menu_aux(&highlight);

        // if an action has been selected...
        switch (actiontype)
        {
        case SMT_MENU_CREATE:
        {
            // this is not a resumption of smithing an item
            p_ptr->smithing_leftover = 0;

            create_tval_menu();

            // backup the smithing object
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;

            break;
        }
        case SMT_MENU_ENCHANT:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                if (!enchant_menu())
                {
                    // restore the smithing object
                    object_copy(smith_o_ptr, smith2_o_ptr);
                    smith_alloy = smith2_alloy;
                }
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_ARTEFACT:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                artefact_menu();
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_NUMBERS:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                numbers_menu();

                // backup the smithing object
                object_copy(smith2_o_ptr, smith_o_ptr);
                smith2_alloy = smith_alloy;
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_MELT:
        {
            if (meltable_metal_items_carried())
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                melt_menu();
            }
            else
            {
                bell("You don't have any mithril or star-iron items.");
            }

            break;
        }
        case SMT_MENU_REPAIR:
        {
            smith_reforge_item();
            break;
        }
        case SMT_MENU_ACCEPT:
        {
            if (smithing_cost.drain > 0)
            {
                char buf[80];

                sprintf(buf,
                    "This will drain your smithing skill by %d points. "
                    "Proceed? ",
                    smithing_cost.drain);
                if (!get_check(buf))
                    break;
            }

            create = true;
            leave_menu = true;
            break;
        }
        case -1:
        {
            leave_menu = true;
            break;
        }
        }
    }

    if (create)
    {
        int turn_multiplier = 10;

        if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
        {
            turn_multiplier /= 2;
        }

        // Display a message
        msg_print("You begin your work.");

        // add the details to the artefact type if applicable
        if (smith_o_ptr->name1)
            add_artefact_details();

        /* Cancel stealth mode */
        p_ptr->stealth_mode = false;

        // Allow the resumption of interrupted smithing
        if (p_ptr->smithing_leftover > 0)
        {
            p_ptr->smithing = p_ptr->smithing_leftover;
        }
        else
        {
            // Set smithing counter
            p_ptr->smithing
                = MAX(10, object_difficulty(smith_o_ptr) * turn_multiplier);

            // Also set the smithing leftover counter (to allow you to resume if
            // interrupted)
            p_ptr->smithing_leftover = p_ptr->smithing;
        }

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Handle stuff */
        handle_stuff();

        /* Refresh */
        Term_fresh();
    }

    else
    {
        if (p_ptr->smithing_leftover == 0)
        {
            /* Wipe the smithing object */
            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);
        }

        // Hack: flag that we are done with smithing
        p_ptr->smithing = 0;
    }

    /* Load screen */
    smith_ui_reset_description_state();
    screen_load();
}

/*
 * Actually creates the item.
 */
void create_smithing_item(void)
{
    int slot;
    object_type* o_ptr;
    char o_name[80];

    log_debug("Creating smithing item");

    // pay the ability/experience costs of smithing
    pay_costs();

    // if making an artefact, copy its attributes into the proper place in the
    // a_info array
    if (smith_o_ptr->name1)
    {
        log_info("Creating new artifact");
        smith_o_ptr->name1 = z_info->art_rand_max + p_ptr->self_made_arts;

        artefact_copy(&a_info[smith_o_ptr->name1], smith_a_ptr);
        artefact_type* created = &a_info[smith_o_ptr->name1];
        if (score_guid_is_zero(&created->guid)) {
            created->guid = score_guid_random();
        }
        (void)score_artefact_register(created);
        p_ptr->self_made_arts++;

        // make sure to display it as cursed if it is so
        if (smith_a_ptr->flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        {
            smith_o_ptr->ident |= (IDENT_CURSED);
            log_debug("Artifact marked as cursed");
        }

        // Store the depth at which it was created
        smith_o_ptr->xtra1 = p_ptr->depth;
        
        log_debug("Artifact #%d created at depth %d", p_ptr->self_made_arts, p_ptr->depth);
    }

        /* ------------------------------------------------------ */
        /* New escape-curse: smithing can back-fire               */
        /* ------------------------------------------------------ */
        {
            int stacks = curse_flag_count_cur(CUR_SMITHCURSE);          /* 0-3 */
            if (stacks &&            /* must have the curse          */
                !(smith_o_ptr->ident & IDENT_CURSED) &&             /* not already */
                (smith_o_ptr->tval != TV_LIGHT))                    /* skip torches */
            {
                if (rand_int(100) < 10 * stacks)                    /* 10 % / stack */
                {
                    log_debug("Smithing curse triggered - adding random curse");
                    add_random_curse(smith_o_ptr);
                }
            }
        }


    // remove the spoiler ident flag
    smith_o_ptr->ident &= ~(IDENT_SPOIL);

    // identify the object
    ident(smith_o_ptr);

    // create description
    object_desc(o_name, sizeof(o_name), smith_o_ptr, true, 3);

    // Record the depth where the object was created
    do_cmd_note(format("Made %s  %d.%d lb", o_name,
                    (smith_o_ptr->weight * smith_o_ptr->number) / 10,
                    (smith_o_ptr->weight * smith_o_ptr->number) % 10),
        p_ptr->depth);

    // Get the slot of the forged item
    slot = inven_carry(smith_o_ptr, true);

    // Check if the item couldn't fit in inventory (e.g., group limit)
    if (slot < 0)
    {
        // Drop it on the floor instead
        log_debug("Smithed item couldn't fit in inventory, dropping to floor");
        drop_near(smith_o_ptr, 0, p_ptr->py, p_ptr->px);
        
        // Describe the object
        object_desc(o_name, sizeof(o_name), smith_o_ptr, true, 3);
        
        // Message
        msg_format("You have forged %s, but it falls to the floor.", o_name);
        log_info("Created smithing item (dropped): %s", o_name);
    }
    else
    {
        // Get the item itself
        o_ptr = &inventory[slot];
        
        // Mark the item as smithed by the player (using unused1 field)
        o_ptr->unused1 = 1;  /* 1 = smithed by player, 0 = found item */

        // Describe the object
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        // Message
        msg_format("You have %s (%c).", o_name, index_to_label(slot));
        log_info("Created smithing item: %s", o_name);
    }

    // Wipe the smithing object
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);
}

#define MAIN_MENU_CHARACTER 1
#define MAIN_MENU_KNOWLEDGE 2
#define MAIN_MENU_QUEST_STATUS 3
#define MAIN_MENU_HALLS_OF_MANDOS 4
#define MAIN_MENU_RUN_HISTORY 5
#define MAIN_MENU_MAP 6
#define MAIN_MENU_LOG 7
#define MAIN_MENU_COMBAT_HISTORY 8
#define MAIN_MENU_HINT_MESSAGES 9
#define MAIN_MENU_STORY 10
#define MAIN_MENU_OPTIONS 11
#define MAIN_MENU_HELP 12
#define MAIN_MENU_ABOUT 13
#define MAIN_MENU_SAVE 14
#define MAIN_MENU_SAVE_QUIT 15
#define MAIN_MENU_RETURN_GAME 16

#define MAIN_MENU_MAX 16
#define MAIN_MENU_LABEL_WIDTH 21
#define MAIN_MENU_SHORTCUT_WIDTH 6

typedef struct main_menu_about_line
{
    byte attr;
    cptr text;
} main_menu_about_line;

typedef struct main_menu_about_span
{
    byte attr;
    cptr text;
} main_menu_about_span;

static cptr main_menu_title(int choice)
{
    switch (choice)
    {
    case MAIN_MENU_CHARACTER: return "Character sheet";
    case MAIN_MENU_KNOWLEDGE: return "Known lore";
    case MAIN_MENU_QUEST_STATUS: return "Quest status";
    case MAIN_MENU_HALLS_OF_MANDOS: return "Halls of Mandos";
    case MAIN_MENU_RUN_HISTORY: return "Run history";
    case MAIN_MENU_MAP: return "Map";
    case MAIN_MENU_LOG: return "Log";
    case MAIN_MENU_COMBAT_HISTORY: return "Combat history";
    case MAIN_MENU_HINT_MESSAGES: return "Hint messages";
    case MAIN_MENU_STORY: return "The story so far";
    case MAIN_MENU_OPTIONS: return "Options and misc";
    case MAIN_MENU_HELP: return "Help";
    case MAIN_MENU_ABOUT: return "About";
    case MAIN_MENU_SAVE: return "Save";
    case MAIN_MENU_SAVE_QUIT: return "Quit with save";
    case MAIN_MENU_RETURN_GAME: return "Return to game";
    default: return "";
    }
}

static int main_menu_keyboard_key(int choice)
{
    switch (choice)
    {
    case MAIN_MENU_CHARACTER: return 'c';
    case MAIN_MENU_KNOWLEDGE: return 'a';
    case MAIN_MENU_QUEST_STATUS: return 't';
    case MAIN_MENU_HALLS_OF_MANDOS: return 'd';
    case MAIN_MENU_RUN_HISTORY: return 'v';
    case MAIN_MENU_MAP: return 'm';
    case MAIN_MENU_LOG: return 'l';
    case MAIN_MENU_COMBAT_HISTORY: return 'x';
    case MAIN_MENU_HINT_MESSAGES: return 'i';
    case MAIN_MENU_STORY: return 'y';
    case MAIN_MENU_OPTIONS: return 'o';
    case MAIN_MENU_HELP: return 'h';
    case MAIN_MENU_ABOUT: return 'b';
    case MAIN_MENU_SAVE: return 's';
    case MAIN_MENU_SAVE_QUIT: return 'q';
    case MAIN_MENU_RETURN_GAME: return 'r';
    default: return 0;
    }
}

static size_t main_menu_append_fixed(char* buf, size_t buflen, size_t cur,
    cptr text, int width)
{
    int len = text ? (int)strlen(text) : 0;

    if (!buf || !buflen || cur >= buflen)
        return cur;

    if (text && text[0])
    {
        size_t written = strnfmt(buf + cur, buflen - cur, "%s", text);
        cur += written;
        if (cur >= buflen)
            cur = buflen - 1;
    }

    while ((len < width) && (cur + 1 < buflen))
    {
        buf[cur++] = ' ';
        len++;
    }

    buf[cur] = '\0';
    return cur;
}

static void main_menu_append_right_aligned_shortcut(char* buf, size_t buflen,
    size_t* cur, cptr text)
{
    int len = text ? (int)strlen(text) : 0;
    int left_pad = 0;

    if (!buf || !buflen || !cur || *cur >= buflen)
        return;

    if (len < MAIN_MENU_SHORTCUT_WIDTH)
        left_pad = MAIN_MENU_SHORTCUT_WIDTH - len;

    while ((left_pad > 0) && (*cur + 1 < buflen))
    {
        buf[(*cur)++] = ' ';
        left_pad--;
    }

    if (text && text[0])
        strnfcat(buf, buflen, cur, "%s", text);

    buf[*cur] = '\0';
}

static bool main_menu_controller_binding_for_choice(int choice, int* type,
    int* id, const char** fallback)
{
    int out_type = GAMEPAD_CAPTURE_BUTTON;
    int out_id = -1;
    const char* out_fallback = "";

    switch (choice)
    {
    case MAIN_MENU_HALLS_OF_MANDOS:
        out_id = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER; /* L1 */
        out_fallback = "L1";
        break;
    case MAIN_MENU_LOG:
        out_id = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER; /* R1 */
        out_fallback = "R1";
        break;
    case MAIN_MENU_HINT_MESSAGES:
        out_id = SDL_GAMEPAD_BUTTON_NORTH;         /* Y */
        out_fallback = "Y";
        break;
    case MAIN_MENU_OPTIONS:
        out_id = SDL_GAMEPAD_BUTTON_BACK;          /* Back/View */
        out_fallback = "Back";
        break;
    case MAIN_MENU_SAVE_QUIT:
        out_id = SDL_GAMEPAD_BUTTON_WEST;          /* X */
        out_fallback = "X";
        break;
    default:
        return false;
    }

    if (type)
        *type = out_type;
    if (id)
        *id = out_id;
    if (fallback)
        *fallback = out_fallback;

    return true;
}

static int main_menu_controller_choice_from_key(int key)
{
    if (!steamdeck_controls_active())
        return 0;

    for (int choice = 1; choice <= MAIN_MENU_MAX; choice++)
    {
        int type = 0;
        int id = 0;
        int binding;

        if (!main_menu_controller_binding_for_choice(choice, &type, &id, NULL))
            continue;

        binding = (type == GAMEPAD_CAPTURE_TRIGGER)
            ? get_sdl_gamepad_trigger_binding(id)
            : get_sdl_gamepad_button_binding(id);

        if ((binding != GAMEPAD_BIND_NONE) && (key == binding))
            return choice;
    }

    return 0;
}

static void main_menu_controller_label(int choice, char* buf, size_t buflen)
{
    int type = 0;
    int id = 0;
    int binding;
    const char* fallback = "";

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!main_menu_controller_binding_for_choice(choice, &type, &id, &fallback))
        return;

    binding = (type == GAMEPAD_CAPTURE_TRIGGER)
        ? get_sdl_gamepad_trigger_binding(id)
        : get_sdl_gamepad_button_binding(id);

    controller_prompt_label(binding, fallback, buf, buflen);
}

static void main_menu_format_line(int choice, char* buf, size_t buflen)
{
    char label[24];
    size_t cur;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    cur = main_menu_append_fixed(buf, buflen, 0, main_menu_title(choice),
        MAIN_MENU_LABEL_WIDTH);

    if (steamdeck_controls_active())
    {
        main_menu_controller_label(choice, label, sizeof(label));
        if (label[0])
        {
            char shortcut[32];
            strnfmt(shortcut, sizeof(shortcut), "[%s]", label);
            main_menu_append_right_aligned_shortcut(buf, buflen, &cur, shortcut);
        }
        else
            main_menu_append_right_aligned_shortcut(buf, buflen, &cur, "");
    }
    else
    {
        strnfcat(buf, buflen, &cur, "(%c)", main_menu_keyboard_key(choice));
    }
}

static void main_menu_format_controller_prompt(char* buf, size_t buflen)
{
    char confirm_label[16];
    char back_label[16];

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    controller_prompt_label(steamdeck_confirm_key(), "A",
        confirm_label, sizeof(confirm_label));
    controller_prompt_label(steamdeck_back_key(), "B",
        back_label, sizeof(back_label));
    strnfmt(buf, buflen, "D-pad select  %s open  %s back",
        confirm_label, back_label);
}

static int main_menu_calc_width(void)
{
    int max_w = 0;
    for (int i = 1; i <= MAIN_MENU_MAX; i++)
    {
        char line[80];
        int w;

        main_menu_format_line(i, line, sizeof(line));
        w = (int)strlen(line);
        if (w > max_w)
            max_w = w;
    }
    if (steamdeck_controls_active())
    {
        char prompt[96];
        int w;

        main_menu_format_controller_prompt(prompt, sizeof(prompt));
        w = (int)strlen(prompt);
        if (w > max_w)
            max_w = w;
    }
    return max_w;
}

static void main_menu_erase_footprint_row(int col_main, int row, int menu_w)
{
    int clear_x;
    int clear_w;

    if (!Term || row < 0 || row >= Term->hgt)
        return;

    clear_x = col_main - 2;
    if (clear_x < 0)
        clear_x = 0;

    clear_w = menu_w + 4;
    if (clear_x + clear_w > Term->wid)
        clear_w = Term->wid - clear_x;

    if (clear_w > 0)
        Term_erase(clear_x, row, clear_w);
}

static bool main_menu_choice_is_disabled(int choice)
{
    return (choice == MAIN_MENU_SAVE)
        || (choice == MAIN_MENU_SAVE_QUIT);
}

static int main_menu_about_count_rows(int indent, int wrap_right,
    const main_menu_about_line* lines, const bool* blank_visible)
{
    int total = 0;

    for (int i = 0; lines[i].text; i++)
    {
        if (!lines[i].text[0])
        {
            if (!blank_visible || blank_visible[i])
                total++;
        }
        else
            total += count_wrapped_lines(lines[i].text, wrap_right, indent);
    }

    return total;
}

static bool main_menu_about_drop_bottom_blank(bool* blank_visible,
    const main_menu_about_line* lines, int line_count)
{
    for (int i = line_count - 1; i >= 0; i--)
    {
        if (!lines[i].text[0] && blank_visible[i])
        {
            blank_visible[i] = false;
            return true;
        }
    }

    return false;
}

static void main_menu_about_draw_line(int row, int indent, int wrap_right,
    byte attr, cptr text)
{
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;

    text_out_wrap = wrap_right;
    text_out_indent = indent;
    Term_gotoxy(indent, row);
    text_out_to_screen(attr, text ? text : "");
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;
}

static void main_menu_about_draw_spans(int row, int indent, int wrap_right,
    const main_menu_about_span* spans, int span_count)
{
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;

    text_out_wrap = wrap_right;
    text_out_indent = indent;
    Term_gotoxy(indent, row);
    for (int i = 0; i < span_count; i++)
        text_out_to_screen(spans[i].attr, spans[i].text);
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;
}

static void main_menu_about(void)
{
    int wid, hgt;
    int menu_w;
    int box_w;
    int box_left;
    int text_indent;
    int wrap_right;
    int body_rows;
    int row_top;
    int row;
    char ch;
    bool saved_hide_cursor;
    static const main_menu_about_line about_lines[] = {
        { TERM_WHITE, "Sil-More is an evolution of SilQ, a famous roguelike" },
        { TERM_WHITE, "taking place in the First Age of Beleriand." },
        { TERM_WHITE, "" },
        { TERM_WHITE, "Developers: k0rtess and sinefabula." },
        { TERM_WHITE, "Gamedesigner: k0rtess." },
        { TERM_WHITE, "Tileset: MicroChasm." },
        { TERM_WHITE, "Main music theme: sinefabula." },
        { TERM_WHITE, "Ambient music theme: West Wind." },
        { TERM_WHITE, "Logo: sinefabula." },
        { TERM_WHITE, "" },
        { TERM_WHITE, "Our love to Maedhros aka Carcharos for playing so much," },
        { TERM_WHITE, "finding those pescy bugs and giving cool ideas." },
        { TERM_L_BLUE, "Special thanks to original Sil and SilQ" },
        { TERM_L_BLUE, "developers: half, Scatha and Quirk." },
        { TERM_WHITE, "" },
        { TERM_WHITE, "Honorable mentions:" },
        { TERM_WHITE, "Sound: Kenney, qubodup, TomMusic, LeoHPaz." },
        { TERM_WHITE, "Walls: Wolffius, Pine Druid, Backterria, Ninjikin." },
        { TERM_WHITE, "" },
        { TERM_L_RED, "And our deep love to Tolkien and his timeless creations." },
        { TERM_WHITE, "" },
        { 0, NULL }
    };

    if (p_ptr && p_ptr->playing)
        sdl_music_play_death();

    screen_save();

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    menu_w = main_menu_calc_width();
    box_w = MIN(MAX(menu_w + 24, 68), 76);
    if (box_w > (wid > 2 ? wid - 2 : wid))
        box_w = (wid > 2) ? (wid - 2) : wid;
    if (box_w < 1)
        box_w = 1;

    box_left = (wid - box_w) / 2;
    if (box_left < 0)
        box_left = 0;

    text_indent = box_left + 2;
    wrap_right = box_left + box_w - 1;

    Term_clear();

    {
        int line_count = 0;
        bool blank_visible[sizeof(about_lines) / sizeof(about_lines[0])] = { false };
        int max_body_rows;

        while (about_lines[line_count].text)
            line_count++;

        for (int i = 0; i < line_count; i++)
            blank_visible[i] = true;

        body_rows = main_menu_about_count_rows(text_indent, wrap_right,
            about_lines, blank_visible);

        max_body_rows = (hgt > 2) ? (hgt - 2) : 0;
        while ((body_rows > max_body_rows)
            && main_menu_about_drop_bottom_blank(blank_visible, about_lines,
                line_count))
        {
            body_rows -= 1;
        }

        row_top = (hgt > body_rows + 2) ? 1 : 0;

        {
            int panel_h = body_rows + 2;
            if (panel_h > hgt - row_top)
                panel_h = hgt - row_top;
            for (int i = 0; i < panel_h; i++)
            {
                int y = row_top + i;
                if (y >= 0 && y < hgt)
                    Term_erase(box_left, y, box_w);
            }
        }

        {
            cptr title = "About Sil-More";
            int title_x = box_left + MAX((box_w - (int)strlen(title)) / 2 - 2, 0);
            Term_putstr(title_x, row_top, -1, TERM_YELLOW, title);
        }

        row = row_top + 1;
        for (int i = 0; i < line_count; i++)
        {
            cptr text = about_lines[i].text;

            if (!text[0])
            {
                if (blank_visible[i])
                    row++;
                continue;
            }

            main_menu_about_draw_line(row, text_indent, wrap_right,
                about_lines[i].attr, text);

            if (i == 0)
            {
                static const main_menu_about_span intro_label_spans[] = {
                    { TERM_VIOLET, "Sil-More" },
                    { TERM_WHITE, " is an evolution of " },
                    { TERM_L_BLUE, "SilQ" },
                    { TERM_WHITE, ", a famous roguelike" },
                };
                main_menu_about_draw_spans(row, text_indent, wrap_right,
                    intro_label_spans,
                    (int)(sizeof(intro_label_spans)
                        / sizeof(intro_label_spans[0])));
            }
            else if ((i >= 3) && (i <= 8))
            {
                static const main_menu_about_span label_spans[][2] = {
                    {
                        { TERM_YELLOW, "Developers:" },
                        { TERM_WHITE, " k0rtess and sinefabula." },
                    },
                    {
                        { TERM_YELLOW, "Gamedesigner:" },
                        { TERM_WHITE, " k0rtess." },
                    },
                    {
                        { TERM_YELLOW, "Tileset:" },
                        { TERM_WHITE, " MicroChasm." },
                    },
                    {
                        { TERM_YELLOW, "Main music theme:" },
                        { TERM_WHITE, " sinefabula." },
                    },
                    {
                        { TERM_YELLOW, "Ambient music theme:" },
                        { TERM_WHITE, " West Wind." },
                    },
                    {
                        { TERM_YELLOW, "Logo:" },
                        { TERM_WHITE, " sinefabula." },
                    },
                };
                int label_index = i - 3;
                main_menu_about_draw_spans(row, text_indent, wrap_right,
                    label_spans[label_index], 2);
            }
            else if (i == 15)
            {
                static const main_menu_about_span mentions_spans[] = {
                    { TERM_YELLOW, "Honorable mentions:" },
                };
                main_menu_about_draw_spans(row, text_indent, wrap_right,
                    mentions_spans, 1);
            }

            row += count_wrapped_lines(text, wrap_right, text_indent);
        }
    }

    if (row >= hgt)
        row = hgt - 1;

    if (steamdeck_controls_active())
    {
        char back_label[16];
        char prompt_buf[48];

        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] return", back_label);
        Term_putstr(text_indent, row, -1, TERM_L_WHITE, prompt_buf);
    }
    else
    {
        Term_putstr(text_indent, row, -1, TERM_L_WHITE,
            "[Press any key to return]");
    }
    Term_fresh();

    flush();
    saved_hide_cursor = hide_cursor;
    hide_cursor = true;
    ch = inkey();
    hide_cursor = saved_hide_cursor;
    (void)ch;

    screen_load();

    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
}

static void do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x);

/*
 * Performs the interface and selection work for the main menu.
 */
int main_menu_aux(int* highlight)
{
    char ch;
    int i;
    bool death_view = death_spectator_active();
    bool steamdeck = steamdeck_controls_active();

    int menu_w = main_menu_calc_width();
    const int top_pad = 1;
    const int bottom_pad = (Term && (Term->hgt <= 18)) ? 0 : 1;
    const int row_first = top_pad;
    int menu_h = MAIN_MENU_MAX + top_pad + bottom_pad;
    int col_main = 0;
    int row_top = 0;
    if (Term)
    {
        col_main = (Term->wid - menu_w) / 2;
        if (col_main < 0)
            col_main = 0;

        /* Keep the menu fixed vertically.
         * At height 20, start at row 0 so all menu rows fit.
         * Otherwise keep row 0 for message bar and start menu at row 1. */
        if (Term->hgt <= 18)
            row_top = 0;
        else
            row_top = (Term->hgt > 1) ? 1 : 0;
    }

    if (death_view && main_menu_choice_is_disabled(*highlight))
        *highlight = MAIN_MENU_RETURN_GAME;

    for (i = 0; i < menu_h; i++)
    {
        int y = row_top + i;
        main_menu_erase_footprint_row(col_main, y, menu_w);
    }

    for (i = 1; i <= MAIN_MENU_MAX; i++)
    {
        char line[80];
        byte color = (*highlight == i) ? TERM_L_BLUE : TERM_WHITE;

        if (death_view && main_menu_choice_is_disabled(i))
            color = TERM_L_DARK;

        main_menu_format_line(i, line, sizeof(line));
        Term_putstr(col_main, row_top + row_first + i - 1, -1, color, line);
    }

    if (steamdeck && Term)
    {
        int prompt_row = row_top + menu_h;
        char prompt_buf[96];
        int prompt_w = menu_w;

        if (prompt_row >= Term->hgt)
            prompt_row = Term->hgt - 1;
        if (prompt_row >= 0)
        {
            main_menu_erase_footprint_row(col_main, prompt_row, menu_w);
            main_menu_format_controller_prompt(prompt_buf, sizeof(prompt_buf));
            if (col_main + prompt_w > Term->wid)
                prompt_w = Term->wid - col_main;
            if (prompt_w > 0)
                Term_putstr(col_main, prompt_row, prompt_w, TERM_SLATE,
                    prompt_buf);
        }
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    {
        int cursor_y = row_top + row_first + (*highlight - 1);

        if (Term)
        {
            if (cursor_y < 0)
                cursor_y = 0;
            if (cursor_y >= Term->hgt)
                cursor_y = Term->hgt - 1;
        }
        Term_gotoxy(col_main, cursor_y);
    }

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if (steamdeck)
    {
        int controller_choice = main_menu_controller_choice_from_key(ch);

        if (controller_choice > 0)
        {
            *highlight = controller_choice;
            return (*highlight);
        }
    }

    // choose an option by letter - alphabetical mapping (updated for new order)
    if (!steamdeck)
    {
        switch (ch)
        {
        case 'c':
            *highlight = 1;
            return (*highlight);  // Character sheet
        case 'a':
            *highlight = 2;
            return (*highlight);  // Known lore
        case 't':
            *highlight = 3;
            return (*highlight);  // Quest status
        case 'd':
            *highlight = 4;
            return (*highlight);  // Halls of Mandos
        case 'v':
            *highlight = 5;
            return (*highlight);  // Run history
        case 'm':
            *highlight = 6;
            return (*highlight);  // Map
        case 'l':
            *highlight = 7;
            return (*highlight);  // Log
        case 'x':
            *highlight = 8;
            return (*highlight); // Combat history
        case 'i':
            *highlight = 9;
            return (*highlight); // Hint messages
        case 'y':
            *highlight = 10;
            return (*highlight); // The story so far
        case 'o':
            *highlight = MAIN_MENU_OPTIONS;
            return (*highlight); // Options and misc
        case 'h':
            *highlight = MAIN_MENU_HELP;
            return (*highlight); // Help
        case 'b':
            *highlight = MAIN_MENU_ABOUT;
            return (*highlight); // About
        case 's':
            if (death_view) {
                msg_print("You can no longer take that action.");
                break;
            }
            *highlight = MAIN_MENU_SAVE;
            return (*highlight); // Save
        case 'q':
            if (death_view) {
                msg_print("You can no longer take that action.");
                break;
            }
            *highlight = MAIN_MENU_SAVE_QUIT;
            return (*highlight); // Quit with save
        case 'r':
            *highlight = MAIN_MENU_RETURN_GAME;
            return (*highlight); // Return to game
        }
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
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = MAIN_MENU_MAX;
        while (death_view && main_menu_choice_is_disabled(*highlight))
        {
            if (*highlight > 1)
                (*highlight)--;
            else
                *highlight = MAIN_MENU_MAX;
        }
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < MAIN_MENU_MAX)
            (*highlight)++;
        else if (*highlight == MAIN_MENU_MAX)
            *highlight = 1;
        while (death_view && main_menu_choice_is_disabled(*highlight))
        {
            if (*highlight < MAIN_MENU_MAX)
                (*highlight)++;
            else
                *highlight = 1;
        }
    }

    /* Leave menu */
    if ((ch == ESCAPE) || (ch == '4')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        return (-1);
    }

    return (0);
}

/*
 * Brings up a menu for choosing some of the game's more abstruse options.
 */
void do_cmd_main_menu(void)
{
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool pending_hint_look = false;
    int pending_hint_look_y = -1;
    int pending_hint_look_x = -1;

    /* Clear any active banner before opening main menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        actiontype = main_menu_aux(&highlight);

        if (death_spectator_active() && main_menu_choice_is_disabled(actiontype))
        {
            msg_print("You can no longer take that action.");
            continue;
        }

        // if an action has been selected...
        switch (actiontype)
        {
        case 1: // Character sheet (c)
        {
            do_cmd_character_sheet();
            leave_menu = true;
            break;
        }
        case 2: // Known lore (a)
        {
            do_cmd_knowledge_browser_page(g_knowledge_last_page);
            leave_menu = true;
            break;
        }
        case 3: // Quest status (t)
        {
            do_cmd_quest_status();
            leave_menu = true;
            break;
        }
        case 4: // Halls of Mandos (d)
        {
            log_info("main menu: opening Halls of Mandos view");
            show_scores_interactive(true);
            leave_menu = true;
            break;
        }
        case 5: // Run history (v)
        {
            do_cmd_run_history();
            leave_menu = true;
            break;
        }
        case 6: // Map (m)
        {
            do_cmd_view_map();
            leave_menu = true;
            break;
        }
        case 7: // Log (l)
        {
            do_cmd_messages();
            leave_menu = true;
            break;
        }
        case 8: // Combat history (x)
        {
            do_cmd_combat_history();
            leave_menu = true;
            break;
        }
        case 9: // Hint messages (i)
        {
            do_cmd_hint_messages(&pending_hint_look, &pending_hint_look_y,
                &pending_hint_look_x);
            leave_menu = true;
            break;
        }
        case 10: // The story so far (y)
        {
            /* Save screen before showing story */
            screen_save();
            print_story(15, 1);
            /* Load screen after story */
            screen_load();
            leave_menu = true;
            break;
        }
        case 11: // Options and misc (o)
        {
            do_cmd_options();
            leave_menu = true;
            break;
        }
        case 12: // Help (h)
        {
            do_cmd_help();
            leave_menu = true;
            break;
        }
        case 13: // About (b)
        {
            main_menu_about();
            leave_menu = true;
            break;
        }
        case 14: // Save (s)
        {
            do_cmd_save_game();
            leave_menu = true;
            break;
        }
        case 15: // Quit with save (q)
        {
            do_cmd_save_game();

            /* Stop playing */
            p_ptr->playing = false;

            /* Mark that we want to quit to menu, not exit application */
            p_ptr->quit_to_menu = true;

            /* Leaving */
            p_ptr->leaving = true;
            leave_menu = true;
            break;
        }
        case 16: // Return to game (r)
        {
            leave_menu = true;
            break;
        }
        case -1:
        {
            leave_menu = true;
            break;
        }
        default:
        {
            /* Invalid selection - stay in menu */
            break;
        }
        }
    }

    /* Load screen */
    screen_load();

    if (pending_hint_look)
    {
        do_cmd_redraw();
        do_cmd_look_at(pending_hint_look_y, pending_hint_look_x);
    }

}

/*
 * Recall the most recent message
 */
void do_cmd_message_one(void)
{
    /* Recall one message XXX XXX XXX */
    c_prt(message_color(0), format("> %s", message_str(0)), 0, 0);
}

static bool hint_message_has_source(const hint_message_meta* meta)
{
    return meta && meta->source_y >= 0 && meta->source_x >= 0
        && meta->source_y < p_ptr->cur_map_hgt && meta->source_x < p_ptr->cur_map_wid;
}

static bool hint_message_is_word_boundary(char ch)
{
    return (ch == '\0') || !isalnum((unsigned char)ch);
}

static bool hint_message_phrase_matches_ci(const char* line, int offset,
    const char* phrase)
{
    size_t len;

    if (!line || !phrase || !phrase[0])
        return false;

    len = strlen(phrase);
    if (SDL_strncasecmp(line + offset, phrase, len) != 0)
        return false;

    if (offset > 0 && !hint_message_is_word_boundary(line[offset - 1]))
        return false;

    return hint_message_is_word_boundary(line[offset + len]);
}

static bool hint_message_phrase_matches(const char* line, int offset, const char* phrase)
{
    size_t len;

    if (!line || !phrase || !phrase[0])
        return false;

    len = strlen(phrase);
    if (strncmp(line + offset, phrase, len) != 0)
        return false;

    if (offset > 0 && !hint_message_is_word_boundary(line[offset - 1]))
        return false;

    return hint_message_is_word_boundary(line[offset + len]);
}

typedef struct tutorial_highlight_rule {
    const char* phrase;
    byte attr;
} tutorial_highlight_rule;

static const tutorial_highlight_rule tutorial_highlight_rules[] = {
    { "Alt+'+'", TERM_WHITE },
    { "Alt+'-'", TERM_WHITE },
    { "Alt+'i'", TERM_WHITE },
    { "Alt+'l'", TERM_WHITE },
    { "Alt+'p'", TERM_WHITE },
    { "'S'", TERM_WHITE },
    { "critical hit", TERM_L_BLUE },
    { "damage dice", TERM_L_BLUE },
    { "damage die", TERM_L_BLUE },
    { "damage sides", TERM_L_BLUE },
    { "damage side", TERM_L_BLUE },
    { "song points", TERM_L_BLUE },
    { "line of sight", TERM_L_BLUE },
    { "light radius", TERM_YELLOW },
    { "right panel", TERM_UMBER },
    { "bottom panel", TERM_UMBER },
    { "left status panel", TERM_UMBER },
    { "status panel", TERM_UMBER },
    { "bright star rating", TERM_L_GREEN },
    { "mixed elemental", TERM_L_BLUE },
    { "pure elemental", TERM_L_BLUE },
    { "vulnerabilities", TERM_L_RED },
    { "vulnerability", TERM_L_RED },
    { "vulnerable", TERM_L_RED },
    { "resistances", TERM_L_GREEN },
    { "resistance", TERM_L_GREEN },
    { "cursed", TERM_ORANGE },
    { "curse", TERM_ORANGE },
    { "jinx", TERM_ORANGE },
    { "elemental", TERM_L_BLUE },
    { "Protection", TERM_L_BLUE },
    { "protection", TERM_L_BLUE },
    { "Evasion", TERM_L_BLUE },
    { "evasion", TERM_L_BLUE },
    { "Attack", TERM_L_BLUE },
    { "attack", TERM_L_BLUE },
    { "Damage", TERM_L_BLUE },
    { "damage", TERM_L_BLUE },
    { "Stealth", TERM_L_BLUE },
    { "stealth", TERM_L_BLUE },
    { "Will", TERM_L_BLUE },
    { "will", TERM_L_BLUE },
    { "Perception", TERM_L_BLUE },
    { "perception", TERM_L_BLUE },
    { "Constitution", TERM_L_BLUE },
    { "constitution", TERM_L_BLUE },
    { "Dexterity", TERM_L_BLUE },
    { "dexterity", TERM_L_BLUE },
    { "Grace", TERM_L_BLUE },
    { "grace", TERM_L_BLUE },
    { "Strength", TERM_L_BLUE },
    { "strength", TERM_L_BLUE },
    { "Smithing", TERM_L_BLUE },
    { "smithing", TERM_L_BLUE },
    { "Song", TERM_L_BLUE },
    { "song", TERM_L_BLUE },
    { "Archery", TERM_L_BLUE },
    { "archery", TERM_L_BLUE },
    { "HP", TERM_L_BLUE },
    { "XP", TERM_L_BLUE },
    { "quiver", TERM_L_BLUE },
    { "inventory", TERM_L_BLUE },
    { "options", TERM_UMBER },
    { "light", TERM_YELLOW },
    { "fire", TERM_L_RED },
    { "ice", TERM_BLUE },
    { "cold", TERM_BLUE },
    { "poison", TERM_L_GREEN },
};

static int tutorial_hint_match_length(const char* line, int offset, byte* out_attr)
{
    int best_len = 0;
    byte best_attr = TERM_WHITE;

    for (int i = 0; i < (int)N_ELEMENTS(tutorial_highlight_rules); ++i)
    {
        const tutorial_highlight_rule* rule = &tutorial_highlight_rules[i];
        int len;

        if (!hint_message_phrase_matches_ci(line, offset, rule->phrase))
            continue;

        len = (int)strlen(rule->phrase);
        if (len > best_len)
        {
            best_len = len;
            best_attr = rule->attr;
        }
    }

    if (out_attr)
        *out_attr = best_attr;

    return best_len;
}

static int hint_message_match_length(const char* line, int offset,
    const hint_message_meta* meta, byte* out_attr)
{
    int best_len = 0;
    byte best_attr = TERM_WHITE;

    if (!meta)
        return 0;

    for (int cue = 0; cue < meta->cue_count; ++cue)
    {
        const char* dist = meta->cue_dists[cue];
        const char* dir = meta->cue_dirs[cue];

        if (hint_message_phrase_matches(line, offset, dist))
        {
            int len = (int)strlen(dist);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_YELLOW;
            }
        }

        if (hint_message_phrase_matches(line, offset, dir))
        {
            int len = (int)strlen(dir);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_L_BLUE;
            }
        }
    }

    if (out_attr)
        *out_attr = best_attr;

    return best_len;
}

static void hint_message_put_segment(int row, int col, byte attr, const char* text)
{
    if (!text || !text[0])
        return;

    if (sdl_is_story_font_enabled())
        story_print_text(row, col, 0, attr, text);
    else
        Term_putstr(col, row, -1, attr, text);
}

static void hint_message_draw_colored_line(int row, int col, byte base_attr,
    const char* line, const hint_message_meta* meta, bool highlight_tutorial)
{
    int start = 0;
    int cursor = col;
    int len;

    if (!line)
        line = "";

    len = (int)strlen(line);
    for (int i = 0; i < len; )
    {
        byte match_attr = base_attr;
        int match_len = hint_message_match_length(line, i, meta, &match_attr);
        if (highlight_tutorial)
        {
            byte tutorial_attr = base_attr;
            int tutorial_len = tutorial_hint_match_length(line, i, &tutorial_attr);
            if (tutorial_len > match_len)
            {
                match_len = tutorial_len;
                match_attr = tutorial_attr;
            }
        }
        if (match_len > 0)
        {
            if (i > start)
            {
                char plain[256];
                int plain_len = i - start;
                memcpy(plain, line + start, plain_len);
                plain[plain_len] = '\0';
                hint_message_put_segment(row, cursor, base_attr, plain);
                cursor += plain_len;
            }

            {
                char special[256];
                memcpy(special, line + i, match_len);
                special[match_len] = '\0';
                hint_message_put_segment(row, cursor, match_attr, special);
            }

            cursor += match_len;
            i += match_len;
            start = i;
        }
        else
        {
            ++i;
        }
    }

    if (start < len)
    {
        char tail[256];
        int tail_len = len - start;
        memcpy(tail, line + start, tail_len);
        tail[tail_len] = '\0';
        hint_message_put_segment(row, cursor, base_attr, tail);
    }
}

static const char* hint_message_title(int index)
{
    byte line_count = hint_messages_message_line_count(index);
    for (int li = 0; li < line_count; ++li)
    {
        const char* line = hint_messages_message_line(index, li);
        if (line && line[0])
            return line;
    }

    return "";
}

static const char* hint_message_pick_prompt(int wid,
    const char* const prompts[], int prompt_count)
{
    int avail = wid;

    if (avail < 1)
        avail = 1;

    for (int i = 0; i < prompt_count; ++i)
    {
        if (!prompts[i])
            continue;
        if ((int)strlen(prompts[i]) <= avail)
            return prompts[i];
    }

    return (prompt_count > 0 && prompts[prompt_count - 1])
        ? prompts[prompt_count - 1]
        : "";
}

static const char* hint_message_detail_prompt(bool has_source, int wid)
{
    static const char* const simple_prompts[] = {
        "[Press any key to continue]",
        "[Any key]"
    };
    static const char* const source_prompts[] = {
        "[Press any key to continue, or 'l' to look at the skeleton]",
        "[Any key continues; 'l' looks at skeleton]",
        "[Any key; 'l' looks]"
    };

    if (has_source)
        return hint_message_pick_prompt(wid, source_prompts,
            N_ELEMENTS(source_prompts));

    return hint_message_pick_prompt(wid, simple_prompts,
        N_ELEMENTS(simple_prompts));
}

static const char* hint_message_list_prompt(bool show_all_tips,
    int level_n, int tip_n, int wid)
{
    static const char* const tip_list_prompts[] = {
        "[Press '8'/'2' to move, Enter to read, 'h' for level hints, or ESCAPE]",
        "[8/2 move, Enter read, h=level hints, ESC]",
        "[8/2 move, Enter, h, ESC]"
    };
    static const char* const level_list_prompts[] = {
        "[Press '8'/'2' to move, Enter to read, 'h' for all tips, 'l' to look, or ESCAPE]",
        "[8/2 move, Enter read, h tips, l look, ESC]",
        "[8/2, Enter, h, l, ESC]"
    };
    static const char* const no_level_with_tips_prompts[] = {
        "[No level hint messages. Press 'h' for all tips, or ESCAPE]",
        "[No level hints. h=tips, ESC]",
        "[No hints. h, ESC]"
    };
    static const char* const no_level_prompts[] = {
        "[No level hint messages. Press ESCAPE]",
        "[No level hints. ESC]"
    };

    if (show_all_tips)
        return hint_message_pick_prompt(wid, tip_list_prompts,
            N_ELEMENTS(tip_list_prompts));

    if (level_n > 0)
        return hint_message_pick_prompt(wid, level_list_prompts,
            N_ELEMENTS(level_list_prompts));

    if (tip_n > 0)
        return hint_message_pick_prompt(wid, no_level_with_tips_prompts,
            N_ELEMENTS(no_level_with_tips_prompts));

    return hint_message_pick_prompt(wid, no_level_prompts,
        N_ELEMENTS(no_level_prompts));
}

typedef struct hint_message_display_line {
    char text[256];
    byte source_line;
} hint_message_display_line;

enum {
    HINT_MESSAGE_DISPLAY_TEXT_MAX = 256,
    HINT_MESSAGE_DISPLAY_LINES_MAX = 48,
    HINT_MESSAGE_LIST_LINES_MAX = 64
};

static int hint_message_wrap_list_text(const char* text, int wrap_cols,
    hint_message_display_line* lines, int limit);

static int hint_message_list_emit_token(int base_row, int wid, int text_col,
    int max_rows, int* used_rows, int* cursor_col, bool draw, byte attr,
    const char* text)
{
    int full_width;
    int len;
    int row;

    if (!text || !text[0] || !used_rows || !cursor_col)
        return true;

    full_width = wid - text_col - 1;
    if (full_width < 1)
        full_width = 1;

    len = (int)strlen(text);

    if (*used_rows <= 0)
    {
        *used_rows = 1;
        *cursor_col = text_col;
    }

    if (*cursor_col + len > wid - 1 && *cursor_col > text_col)
    {
        if (*used_rows >= max_rows)
            return false;

        row = base_row + *used_rows;
        if (draw)
            Term_erase(0, row, 255);
        (*used_rows)++;
        *cursor_col = text_col;
    }

    row = base_row + (*used_rows - 1);

    if (len <= full_width)
    {
        if (draw)
            hint_message_put_segment(row, *cursor_col, attr, text);
        *cursor_col += len;
        return true;
    }

    {
        hint_message_display_line lines[HINT_MESSAGE_LIST_LINES_MAX];
        int line_count = hint_message_wrap_list_text(text, full_width, lines,
            HINT_MESSAGE_LIST_LINES_MAX);

        for (int li = 0; li < line_count; ++li)
        {
            if (li > 0 || *cursor_col > text_col)
            {
                if (*used_rows >= max_rows)
                    return false;

                row = base_row + *used_rows;
                if (draw)
                    Term_erase(0, row, 255);
                (*used_rows)++;
                *cursor_col = text_col;
            }

            row = base_row + (*used_rows - 1);
            if (draw)
                hint_message_put_segment(row, *cursor_col, attr, lines[li].text);
            *cursor_col += (int)strlen(lines[li].text);
        }
    }

    return true;
}

static int skeleton_tip_template_count(void)
{
    int count = 0;

    if (!skeleton_note_info || !skeleton_note_text || !z_info)
        return 0;

    for (int i = 0; i < z_info->skeleton_note_max; ++i)
    {
        const skeleton_note_template* tip = &skeleton_note_info[i];

        if (tip->role != SKELETON_NOTE_ROLE_HINT || tip->hint != SKEL_HINT_TIP)
            continue;
        if (tip->weight == 0 || tip->text == 0)
            continue;

        count++;
    }

    return count;
}

static const skeleton_note_template* skeleton_tip_template_by_index(int index)
{
    int seen = 0;

    if (index < 0 || !skeleton_note_info || !skeleton_note_text || !z_info)
        return NULL;

    for (int i = 0; i < z_info->skeleton_note_max; ++i)
    {
        const skeleton_note_template* tip = &skeleton_note_info[i];

        if (tip->role != SKELETON_NOTE_ROLE_HINT || tip->hint != SKEL_HINT_TIP)
            continue;
        if (tip->weight == 0 || tip->text == 0)
            continue;

        if (seen == index)
            return tip;

        seen++;
    }

    return NULL;
}

static bool skeleton_tip_text_by_index(int index, char* buf, size_t buf_sz)
{
    const skeleton_note_template* tip = skeleton_tip_template_by_index(index);
    const char* main_text;
    const char* extra_text = NULL;

    if (!buf || buf_sz == 0)
        return false;

    buf[0] = '\0';

    if (!tip || !skeleton_note_text)
        return false;

    main_text = skeleton_note_text + tip->text;
    if (tip->extra_text)
        extra_text = skeleton_note_text + tip->extra_text;

    if (extra_text && extra_text[0])
    {
        strnfmt(buf, buf_sz, "%s %s", main_text, extra_text);
    }
    else
    {
        strnfmt(buf, buf_sz, "%s", main_text);
    }

    return (buf[0] != '\0');
}

static int hint_message_effective_wrap_cols(int wrap_cols,
    size_t line_capacity)
{
    int max_cols = (int)line_capacity - 1;

    if (max_cols < 1)
        max_cols = 1;
    if (wrap_cols < 1)
        wrap_cols = 1;
    if (wrap_cols > max_cols)
        wrap_cols = max_cols;

    return wrap_cols;
}

static int hint_message_max_chars_fit_pixels(const char* text, int max_chars,
    int max_px, int cell_width)
{
    if (!text || max_chars <= 0)
        return 0;

    if (max_px <= 0 || cell_width <= 0)
        return max_chars;

    int lo = 1;
    int hi = max_chars;
    int best = 1;

    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        int width = sdl_story_font_text_width(text, mid);
        if (width <= 0)
            width = mid * cell_width;

        if (width <= max_px)
        {
            best = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }

    return best;
}

static int hint_message_append_wrapped_segment_mono(const char* seg,
    hint_message_display_line* lines, int idx, int limit, int wrap_cols,
    byte source_line)
{
    int len;
    int pos;

    if (!seg || !seg[0] || !lines || limit <= idx)
        return idx;

    wrap_cols = hint_message_effective_wrap_cols(
        wrap_cols, sizeof(lines[0].text));

    len = (int)strlen(seg);
    pos = 0;

    while (pos < len && idx < limit)
    {
        int remaining;
        int take;

        while (pos < len && seg[pos] == ' ')
            pos++;
        if (pos >= len)
            break;

        remaining = len - pos;
        take = (remaining <= wrap_cols) ? remaining : wrap_cols;

        if (remaining > wrap_cols)
        {
            int end = pos + take;
            int split = -1;
            for (int j = end - 1; j > pos; --j)
            {
                if (seg[j] == ' ')
                {
                    split = j;
                    break;
                }
            }
            if (split > pos)
                take = split - pos;
        }

        while (take > 0 && seg[pos + take - 1] == ' ')
            take--;

        if (take <= 0)
            break;

        strnfmt(lines[idx].text, sizeof(lines[idx].text), "%.*s", take, seg + pos);
        lines[idx].source_line = source_line;
        idx++;
        pos += take;
    }

    return idx;
}

static int hint_message_append_wrapped_segment_story(const char* seg,
    hint_message_display_line* lines, int idx, int limit, int wrap_cols,
    byte source_line)
{
    int cell_width;
    int wrap_px;
    int space_px;
    int max_line_chars;
    const char* s;

    if (!seg || !seg[0] || !lines || limit <= idx)
        return idx;

    wrap_cols = hint_message_effective_wrap_cols(
        wrap_cols, sizeof(lines[0].text));

    cell_width = sdl_get_cell_width();
    if (cell_width <= 0)
        return hint_message_append_wrapped_segment_mono(
            seg, lines, idx, limit, wrap_cols, source_line);

    wrap_px = wrap_cols * cell_width;
    space_px = sdl_story_font_text_width(" ", 1);
    if (space_px <= 0)
        space_px = cell_width;

    max_line_chars = wrap_cols;
    if (max_line_chars > HINT_MESSAGE_DISPLAY_TEXT_MAX - 1)
        max_line_chars = HINT_MESSAGE_DISPLAY_TEXT_MAX - 1;

    s = seg;
    while (*s && idx < limit)
    {
        char out[HINT_MESSAGE_DISPLAY_TEXT_MAX];
        int out_len = 0;
        int line_px = 0;
        bool first_word = true;

        while (*s == ' ')
            s++;
        if (!*s)
            break;

        while (*s)
        {
            const char* word;
            int word_len = 0;
            int word_px;
            int add_px;
            int add_chars;

            while (*s == ' ')
                s++;
            if (!*s)
                break;

            word = s;
            while (word[word_len] && word[word_len] != ' ')
                word_len++;

            word_px = sdl_story_font_text_width(word, word_len);
            if (word_px <= 0)
                word_px = word_len * cell_width;

            add_px = word_px + (first_word ? 0 : space_px);
            add_chars = word_len + (first_word ? 0 : 1);

            if (!first_word
                && ((line_px + add_px) > wrap_px
                    || (out_len + add_chars) > max_line_chars))
            {
                break;
            }

            if (first_word && (word_px > wrap_px || word_len > max_line_chars))
            {
                int max_chars = word_len;
                int fit;

                if (max_chars > max_line_chars - out_len)
                    max_chars = max_line_chars - out_len;
                fit = hint_message_max_chars_fit_pixels(
                    word, max_chars, wrap_px, cell_width);
                if (fit <= 0)
                    fit = 1;

                memcpy(out + out_len, word, fit);
                out_len += fit;
                out[out_len] = '\0';
                s += fit;
                break;
            }

            if (!first_word)
            {
                out[out_len++] = ' ';
                line_px += space_px;
            }

            if (word_len > HINT_MESSAGE_DISPLAY_TEXT_MAX - 1 - out_len)
                word_len = HINT_MESSAGE_DISPLAY_TEXT_MAX - 1 - out_len;
            if (word_len > max_line_chars - out_len)
                word_len = max_line_chars - out_len;
            memcpy(out + out_len, word, word_len);
            out_len += word_len;
            out[out_len] = '\0';
            line_px += word_px;

            s += word_len;
            first_word = false;
        }

        if (out_len > 0)
        {
            strnfmt(lines[idx].text, sizeof(lines[idx].text), "%s", out);
            lines[idx].source_line = source_line;
            idx++;
        }

        while (*s == ' ')
            s++;
    }

    return idx;
}

static int hint_message_append_wrapped_text(const char* text,
    hint_message_display_line* lines, int idx, int limit, int wrap_cols,
    byte source_line)
{
    char expanded[512];
    char* seg;

    if (!text || !lines || limit <= idx)
        return idx;

    if (!text[0])
    {
        lines[idx].text[0] = '\0';
        lines[idx].source_line = source_line;
        return idx + 1;
    }

    strnfmt(expanded, sizeof(expanded), "%s", text);
    seg = expanded;

    while (seg && *seg && idx < limit)
    {
        char* next = strchr(seg, '|');
        if (next)
        {
            *next = '\0';
            next++;
        }

        while (*seg == ' ')
            seg++;

        if (*seg)
        {
            if (sdl_is_story_font_enabled() && sdl_story_font_text_width(" ", 1) > 0
                && sdl_get_cell_width() > 0)
            {
                idx = hint_message_append_wrapped_segment_story(
                    seg, lines, idx, limit, wrap_cols, source_line);
            }
            else
            {
                idx = hint_message_append_wrapped_segment_mono(
                    seg, lines, idx, limit, wrap_cols, source_line);
            }
        }
        else
        {
            lines[idx].text[0] = '\0';
            lines[idx].source_line = source_line;
            idx++;
        }

        seg = next;
    }

    return idx;
}

static int hint_message_wrap_list_text(const char* text, int wrap_cols,
    hint_message_display_line* lines, int limit)
{
    int line_count = hint_message_append_wrapped_text(
        text, lines, 0, limit, wrap_cols, 0);

    if (line_count <= 0 && limit > 0)
    {
        lines[0].text[0] = '\0';
        lines[0].source_line = 0;
        line_count = 1;
    }

    return line_count;
}

static int hint_message_draw_wrapped_list_entry(int row, int idx,
    bool selected, int wid, int max_rows, const char* text,
    const hint_message_meta* meta, bool highlight_tutorial)
{
    char prefix[8];
    hint_message_display_line lines[HINT_MESSAGE_LIST_LINES_MAX];
    byte prefix_attr = selected ? TERM_L_BLUE : TERM_WHITE;
    byte title_attr = selected ? TERM_L_WHITE : TERM_WHITE;
    int text_col;
    int line_count;
    int draw_count;

    if (max_rows <= 0)
        return 0;

    strnfmt(prefix, sizeof(prefix), "%2d) ", idx + 1);
    text_col = (int)strlen(prefix);
    line_count = hint_message_wrap_list_text(text ? text : "",
        wid - text_col - 1, lines, HINT_MESSAGE_LIST_LINES_MAX);
    draw_count = MIN(line_count, max_rows);

    for (int li = 0; li < draw_count; ++li)
    {
        Term_erase(0, row + li, 255);

        if (li == 0)
            Term_putstr(0, row + li, -1, prefix_attr, prefix);

        hint_message_draw_colored_line(row + li, text_col, title_attr,
            lines[li].text, meta, highlight_tutorial);
    }

    return draw_count;
}

static int hint_message_layout_list_entry(int row, int idx, bool selected,
    int wid, int max_rows, bool draw)
{
    hint_message_meta meta;
    char prefix[8];
    hint_message_display_line title_lines[HINT_MESSAGE_LIST_LINES_MAX];
    const char* title = hint_message_title(idx);
    byte prefix_attr = selected ? TERM_L_BLUE : TERM_WHITE;
    byte title_attr = selected ? TERM_L_WHITE : TERM_WHITE;
    byte chrome_attr = TERM_SLATE;
    int text_col;
    int title_count;
    int used_rows = 0;
    int cursor_col = 0;

    if (max_rows <= 0)
        return 0;

    hint_messages_message_meta(idx, &meta);
    strnfmt(prefix, sizeof(prefix), "%2d) ", idx + 1);
    text_col = (int)strlen(prefix);
    title_count = hint_message_wrap_list_text(title ? title : "",
        wid - text_col - 1, title_lines, HINT_MESSAGE_LIST_LINES_MAX);

    for (int li = 0; li < title_count && used_rows < max_rows; ++li)
    {
        if (draw)
        {
            Term_erase(0, row + used_rows, 255);
            if (li == 0)
                Term_putstr(0, row + used_rows, -1, prefix_attr, prefix);
            hint_message_put_segment(row + used_rows, text_col, title_attr,
                title_lines[li].text);
        }

        used_rows++;
    }

    if (used_rows <= 0 || meta.cue_count <= 0)
        return used_rows;

    cursor_col = text_col + (int)strlen(title_lines[title_count - 1].text);

    if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
            &used_rows, &cursor_col, draw, chrome_attr, " ["))
    {
        return used_rows;
    }

    for (int cue = 0; cue < meta.cue_count; ++cue)
    {
        if (cue > 0)
        {
            if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
                    &used_rows, &cursor_col, draw, chrome_attr, "; "))
            {
                return used_rows;
            }
        }

        if (meta.cue_dists[cue][0])
        {
            if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
                    &used_rows, &cursor_col, draw, TERM_YELLOW,
                    meta.cue_dists[cue]))
            {
                return used_rows;
            }
        }

        if (meta.cue_dists[cue][0] && meta.cue_dirs[cue][0])
        {
            if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
                    &used_rows, &cursor_col, draw, chrome_attr, " "))
            {
                return used_rows;
            }
        }

        if (meta.cue_dirs[cue][0])
        {
            if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
                    &used_rows, &cursor_col, draw, TERM_L_BLUE,
                    meta.cue_dirs[cue]))
            {
                return used_rows;
            }
        }
    }

    (void)hint_message_list_emit_token(row, wid, text_col, max_rows,
        &used_rows, &cursor_col, draw, chrome_attr, "]");

    return used_rows;
}

static int hint_message_list_entry_height(int idx, int wid)
{
    return hint_message_layout_list_entry(0, idx, false, wid,
        HINT_MESSAGE_LIST_LINES_MAX, false);
}

static int hint_message_draw_list_row(int row, int idx, bool selected, int wid,
    int max_rows)
{
    return hint_message_layout_list_entry(row, idx, selected, wid,
        max_rows, true);
}

static int skeleton_tip_list_entry_height(int idx, int wid)
{
    char text[512];
    hint_message_display_line lines[HINT_MESSAGE_LIST_LINES_MAX];
    char prefix[8];
    int text_col;

    if (!skeleton_tip_text_by_index(idx, text, sizeof(text)))
        text[0] = '\0';

    strnfmt(prefix, sizeof(prefix), "%2d) ", idx + 1);
    text_col = (int)strlen(prefix);

    return hint_message_wrap_list_text(text, wid - text_col - 1,
        lines, HINT_MESSAGE_LIST_LINES_MAX);
}

static int skeleton_tip_draw_list_row(int row, int idx, bool selected, int wid,
    int max_rows)
{
    char tip_text[512];

    if (!skeleton_tip_text_by_index(idx, tip_text, sizeof(tip_text)))
        tip_text[0] = '\0';

    return hint_message_draw_wrapped_list_entry(row, idx, selected, wid,
        max_rows, tip_text, NULL, true);
}

static bool skeleton_tip_show_internal(int index, bool manage_screen)
{
    int wid = 80;
    int hgt = 24;
    int row = 4;
    int col = 8;
    hint_message_display_line lines[HINT_MESSAGE_DISPLAY_LINES_MAX];
    char tip_text[512];
    int line_count = 0;

    if (!skeleton_tip_text_by_index(index, tip_text, sizeof(tip_text)))
        return false;

    if (manage_screen)
        screen_save();

    sdl_story_font_enable();

    while (1)
    {
        Term_clear();
        Term_get_size(&wid, &hgt);
        line_count = 0;

        line_count = hint_message_append_wrapped_text(
            "Hint: Survival Tip", lines, line_count,
            HINT_MESSAGE_DISPLAY_LINES_MAX, wid - col - 1, 0);
        line_count = hint_message_append_wrapped_text(
            tip_text, lines, line_count, HINT_MESSAGE_DISPLAY_LINES_MAX,
            wid - col - 1, 1);

        for (int li = 0; li < line_count && row + li < hgt - 1; ++li)
        {
            bool title_line = (lines[li].source_line == 0);
            byte base_attr = title_line ? TERM_L_WHITE : TERM_WHITE;
            hint_message_draw_colored_line(row + li, col, base_attr, lines[li].text,
                NULL, !title_line);
        }

        prt(hint_message_detail_prompt(false, wid), hgt - 1, 0);
        Term_fresh();

        hide_cursor = true;
        (void)inkey();
        hide_cursor = false;
        break;
    }

    sdl_story_font_disable();
    if (manage_screen)
        screen_load();

    return false;
}

static bool hint_message_show_internal(int index, int* look_y, int* look_x,
    bool manage_screen)
{
    int wid = 80;
    int hgt = 24;
    int row = 4;
    int col = 8;
    hint_message_display_line display_lines[HINT_MESSAGE_DISPLAY_LINES_MAX];
    char ch;
    hint_message_meta meta;
    byte stored_line_count;
    int display_line_count = 0;
    bool request_look = false;
    bool highlight_tutorial = false;

    hint_messages_ensure_level_state();
    stored_line_count = hint_messages_message_line_count(index);
    if (!stored_line_count)
        return false;

    hint_messages_message_meta(index, &meta);
    highlight_tutorial = (strstr(hint_messages_message_line(index, 0), "Survival Tip") != NULL);

    if (manage_screen)
        screen_save();

    sdl_story_font_enable();

    while (1)
    {
        Term_clear();
        Term_get_size(&wid, &hgt);
        display_line_count = 0;

        for (int li = 0; li < stored_line_count; ++li)
        {
            display_line_count = hint_message_append_wrapped_text(
                hint_messages_message_line(index, li),
                display_lines, display_line_count,
                HINT_MESSAGE_DISPLAY_LINES_MAX, wid - col - 1, (byte)li);
        }

        for (int li = 0; li < display_line_count && row + li < hgt - 1; ++li)
        {
            bool title_line = (display_lines[li].source_line == 0);
            byte base_attr = title_line ? TERM_L_WHITE : TERM_WHITE;
            hint_message_draw_colored_line(row + li, col, base_attr,
                display_lines[li].text, title_line ? NULL : &meta,
                (highlight_tutorial && !title_line));
        }

        prt(hint_message_detail_prompt(hint_message_has_source(&meta), wid),
            hgt - 1, 0);

        Term_fresh();

        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        if ((ch == 'l' || ch == 'L') && hint_message_has_source(&meta))
        {
            if (look_y)
                *look_y = meta.source_y;
            if (look_x)
                *look_x = meta.source_x;
            request_look = true;
            break;
        }

        break;
    }

    sdl_story_font_disable();
    if (manage_screen)
        screen_load();

    return request_look;
}

void show_hint_message_screen(int index)
{
    int look_y = -1;
    int look_x = -1;

    if (hint_message_show_internal(index, &look_y, &look_x, true))
    {
        do_cmd_redraw();
        do_cmd_look_at(look_y, look_x);
    }
}

static void do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x)
{
    char ch;

    int wid, hgt;
    bool pending_look = false;
    int look_y = -1;
    int look_x = -1;
    bool show_all_tips = false;

    /* Clear any active banner before opening hint messages */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    hint_messages_ensure_level_state();

    int level_n = (int)hint_messages_count_for_save();
    int tip_n = skeleton_tip_template_count();
    int sel = 0;
    int top = 0;
    show_all_tips = false;

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();

    while (1)
    {
        int n = show_all_tips ? tip_n : level_n;
        int draw_row = 0;

        Term_get_size(&wid, &hgt);
        Term_clear();

        int rows = hgt - 4;
        if (rows < 1)
            rows = 1;

        if (n > 0)
        {
            if (sel < 0)
                sel = 0;
            if (sel >= n)
                sel = n - 1;

            if (sel < top)
                top = sel;
            if (top < 0)
                top = 0;

            while (top < sel)
            {
                int used = 0;

                for (int idx = top; idx <= sel; ++idx)
                {
                    int height = show_all_tips
                        ? skeleton_tip_list_entry_height(idx, wid)
                        : hint_message_list_entry_height(idx, wid);

                    used += MIN(height, rows);
                }

                if (used <= rows)
                    break;

                top++;
            }

            if (top >= n)
                top = n - 1;
        }
        else
        {
            sel = 0;
            top = 0;
        }

        if (show_all_tips)
            prt(format("All Tutorial Hints (%d)", tip_n), 0, 0);
        else
            prt(format("Hint Messages (%d)", level_n), 0, 0);

        prt(hint_message_list_prompt(show_all_tips, level_n, tip_n, wid),
            hgt - 1, 0);

        if (n <= 0)
        {
            Term_putstr(0, 2, -1, TERM_SLATE,
                show_all_tips ? "No tutorial hints are available."
                             : "You recall no hint messages on this level.");
        }

        for (int idx = top; idx < n && draw_row < rows; ++idx)
        {
            int used;

            if (show_all_tips)
                used = skeleton_tip_draw_list_row(2 + draw_row, idx,
                    idx == sel, wid, rows - draw_row);
            else
                used = hint_message_draw_list_row(2 + draw_row, idx,
                    idx == sel, wid, rows - draw_row);

            if (used <= 0)
                break;

            draw_row += used;
        }

        Term_fresh();
        ch = inkey();

        if (ch == ESCAPE)
            break;

        if (ch == 'h' || ch == 'H')
        {
            if (tip_n <= 0)
            {
                bell(NULL);
                continue;
            }

            show_all_tips = !show_all_tips;
            sel = 0;
            top = 0;
            continue;
        }

        if (n <= 0)
        {
            bell(NULL);
            continue;
        }

        if (ch == '8')
        {
            sel = (sel > 0) ? (sel - 1) : (n - 1);
            continue;
        }

        if (ch == '2')
        {
            sel = (sel + 1 < n) ? (sel + 1) : 0;
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
        {
            int selected_look_y = -1;
            int selected_look_x = -1;

            if (show_all_tips)
            {
                (void)skeleton_tip_show_internal(sel, false);
            }
            else if (hint_message_show_internal(sel, &selected_look_y, &selected_look_x, false))
            {
                pending_look = true;
                look_y = selected_look_y;
                look_x = selected_look_x;
                break;
            }
            continue;
        }

        if (ch == 'l' || ch == 'L')
        {
            hint_message_meta meta;

            if (show_all_tips)
            {
                bell(NULL);
                continue;
            }

            hint_messages_message_meta(sel, &meta);
            if (hint_message_has_source(&meta))
            {
                pending_look = true;
                look_y = meta.source_y;
                look_x = meta.source_x;
                break;
            }

            bell(NULL);
            continue;
        }

        bell(NULL);
    }

    /* Load screen */
    screen_pop_supporting_panes_hidden();
    screen_load();

    if (out_pending_look)
        *out_pending_look = pending_look;
    if (out_look_y)
        *out_look_y = look_y;
    if (out_look_x)
        *out_look_x = look_x;
}

/*
 * Show previous messages to the user
 *
 * The screen format uses line 0 and 23 for headers and prompts,
 * skips line 1 and 22, and uses line 2 thru 21 for old messages.
 *
 * This command shows you which commands you are viewing, and allows
 * you to "search" for strings in the recall.
 *
 * Note that messages may be longer than 80 characters, but they are
 * displayed using "infinite" length, with a special sub-command to
 * "slide" the virtual display to the left or right.
 *
 * Attempt to only hilite the matching portions of the string.
 */
void do_cmd_messages(void)
{
    char ch;

    int i, j, n, q;
    int wid, hgt;

    char shower[80];
    char finder[80];

    /* Clear any active banner before opening message history */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Wipe finder */
    SDL_strlcpy(finder, "", sizeof(finder));

    /* Wipe shower */
    SDL_strlcpy(shower, "", sizeof(shower));

    /* Total messages */
    n = message_num();

    /* Start on first message */
    i = 0;

    /* Start at leftmost edge */
    q = 0;

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();

    /* Get size after any hidden-pane layout change */
    Term_get_size(&wid, &hgt);

    /* Process requests until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Dump messages */
        for (j = 0; (j < hgt - 4) && (i + j < n); j++)
        {
            cptr msg = message_str((s16b)(i + j));
            byte attr = message_color((s16b)(i + j));

            /* Apply horizontal scroll */
            msg = ((int)strlen(msg) >= q) ? (msg + q) : "";

            /* Dump the messages, bottom to top */
            Term_putstr(0, hgt - 3 - j, -1, attr, msg);

            /* Hilite "shower" */
            if (shower[0])
            {
                cptr str = msg;

                /* Display matches */
                while ((str = strstr(str, shower)) != NULL)
                {
                    int len = strlen(shower);

                    /* Display the match */
                    Term_putstr(
                        str - msg, hgt - 3 - j, len, TERM_YELLOW, shower);

                    /* Advance */
                    str += len;
                }
            }
        }

        /* Display header XXX XXX XXX */
        prt(format(
                "Message Recall (%d-%d of %d), Offset %d", i, i + j - 1, n, q),
            0, 0);

        /* Display prompt (not very informative) */
        prt("[Press 'p' for older, 'n' for newer, ..., or ESCAPE]", hgt - 1, 0);

        /* Get a command */
        ch = inkey();

        /* Exit on Escape */
        if (ch == ESCAPE)
            break;

        /* Hack -- Save the old index */
        j = i;

        /* Horizontal scroll */
        if (ch == '4')
        {
            /* Scroll left */
            q = (q >= wid / 2) ? (q - wid / 2) : 0;

            /* Success */
            continue;
        }

        /* Horizontal scroll */
        if (ch == '6')
        {
            /* Scroll right */
            q = q + wid / 2;

            /* Success */
            continue;
        }

        /* Hack -- handle show */
        if (ch == '=')
        {
            /* Prompt */
            prt("Show: ", hgt - 1, 0);

            /* Get a "shower" string, or continue */
            if (!askfor_aux(shower, sizeof(shower)))
                continue;

            /* Okay */
            continue;
        }

        /* Hack -- handle find */
        if (ch == '/')
        {
            s16b z;

            /* Prompt */
            prt("Find: ", hgt - 1, 0);

            /* Get a "finder" string, or continue */
            if (!askfor_aux(finder, sizeof(finder)))
                continue;

            /* Show it */
            SDL_strlcpy(shower, finder, sizeof(shower));

            /* Scan messages */
            for (z = i + 1; z < n; z++)
            {
                cptr msg = message_str(z);

                /* Search for it */
                if (strstr(msg, finder))
                {
                    /* New location */
                    i = z;

                    /* Done */
                    break;
                }
            }
        }

        /* Recall 20 older messages */
        if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' '))
        {
            /* Go older if legal */
            if (i + 20 < n)
                i += 20;
        }

        /* Recall 10 older messages */
        if (ch == '+')
        {
            /* Go older if legal */
            if (i + 10 < n)
                i += 10;
        }

        /* Recall 1 older message */
        if ((ch == '8') || (ch == '\n') || (ch == '\r'))
        {
            /* Go newer if legal */
            if (i + 1 < n)
                i += 1;
        }

        /* Recall 20 newer messages */
        if ((ch == 'n') || (ch == KTRL('N')))
        {
            /* Go newer (if able) */
            i = (i >= 20) ? (i - 20) : 0;
        }

        /* Recall 10 newer messages */
        if (ch == '-')
        {
            /* Go newer (if able) */
            i = (i >= 10) ? (i - 10) : 0;
        }

        /* Recall 1 newer messages */
        if (ch == '2')
        {
            /* Go newer (if able) */
            i = (i >= 1) ? (i - 1) : 0;
        }

        /* Hack -- Error of some kind */
        if (i == j)
            bell(NULL);
    }

    /* Load screen */
    screen_pop_supporting_panes_hidden();
    screen_load();
}

/*
 * Ask for a "user pref line" and process it
 */
void do_cmd_pref(void)
{
    char tmp[80];

    /* Default */
    SDL_strlcpy(tmp, "", sizeof(tmp));

    /* Ask for a "user pref command" */
    if (!term_get_string("Pref: ", tmp, sizeof(tmp)))
        return;

    /* Process that pref command */
    (void)process_pref_file_command(tmp);
}

/*
 * Ask for a "user pref file" and process it.
 *
 * This function should only be used by standard interaction commands,
 * in which a standard "Command:" prompt is present on the given row.
 *
 * Allow absolute file names?  XXX XXX XXX
 */
static void do_cmd_pref_file_hack(int row)
{
    char ftmp[80];

    /* Prompt */
    Term_putstr(2, row + 2, -1, TERM_SLATE, "(Escape to cancel)");

    /* Prompt */
    prt("File: ", row, 2);

    /* Default filename */
    strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

    /* Ask for a file (or cancel) */
    if (!askfor_aux(ftmp, sizeof(ftmp)))
        return;

    /* Process the given filename */
    if (process_pref_file(ftmp))
    {
        /* Mention failure */
        msg_format("Failed to load '%s'!", ftmp);
    }
    else
    {
        /* Mention success */
        msg_format("Loaded '%s'.", ftmp);
    }
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
    int before_index;
    cptr label;
};

static const struct option_group_marker interface_option_groups[] = {
    { 0, "Messages" },
    { 3, "Look" },
    { 6, "Panels" },
    { 9, "Warnings" },
    { 10, "Input" },
    { 14, "Items" },
    { 17, "Debug" },
    { -1, NULL }
};

static const struct option_group_marker text_option_groups[] = {
    { 0, "Look and Lore" },
    { 3, "Inventory and Equipment" },
    { 7, "Character" },
    { -1, NULL }
};

static const struct option_group_marker gameplay_option_groups[] = {
    { 0, "Combat Behavior" },
    { 5, "Information" },
    { 8, "World Generation" },
    { 13, "Blitz" },
    { -1, NULL }
};

static const struct option_group_marker efficiency_option_groups[] = {
    { 0, "Animation" },
    { 2, "Camera" },
    { -1, NULL }
};

static const struct option_group_marker visual_option_groups[] = {
    { 0, "Lists" },
    { 2, "Overlay" },
    { 4, "Items" },
    { 6, "Narrative" },
    { 10, "ASCII" },
    { 12, "Cursor" },
    { 15, "Debug" },
    { -1, NULL }
};

static const struct option_group_marker challenge_option_groups[] = {
    { 0, "Traversal" },
    { 2, "Content" },
    { -1, NULL }
};

static const struct option_group_marker debug_option_groups[] = {
    { 0, "Generation" },
    { 4, "Knowledge" },
    { 10, "Survival" },
    { -1, NULL }
};

static const struct option_group_marker sound_option_groups[] = {
    { SOUND_OPT_ENABLED, "Master" },
    { SOUND_OPT_COMBAT_ENABLED, "Effects" },
    { SOUND_OPT_COMBAT_VOLUME, "Effect Volume" },
    { SOUND_OPT_MUSIC_MAIN_ENABLED, "Music" },
    { SOUND_OPT_MUSIC_MAIN_VOLUME, "Music Volume" },
    { -1, NULL }
};

static const struct option_group_marker* get_option_groups_for_page(int page)
{
    switch (page)
    {
    case INTERFACE_PAGE: return interface_option_groups;
    case TEXT_PAGE: return text_option_groups;
    case GAMEPLAY_PAGE: return gameplay_option_groups;
    case EFFICIENCY_PAGE: return efficiency_option_groups;
    case VISUAL_PAGE: return visual_option_groups;
    case CHALLENGE_PAGE: return challenge_option_groups;
    case DEBUG_PAGE: return debug_option_groups;
    case SOUND_PAGE: return sound_option_groups;
    default: return NULL;
    }
}

static int option_group_count_before(const struct option_group_marker* groups,
    int option_index)
{
    int count = 0;

    if (!groups)
        return 0;

    for (int i = 0; groups[i].before_index >= 0; i++) {
        if (groups[i].before_index <= option_index)
            count++;
    }

    return count;
}

static int option_group_total_rows(const struct option_group_marker* groups)
{
    int count = 0;

    if (!groups)
        return 0;

    for (int i = 0; groups[i].before_index >= 0; i++)
        count++;

    return count;
}

static bool option_page_uses_app_config(int page)
{
    return (page == INTERFACE_PAGE) || (page == TEXT_PAGE)
        || (page == EFFICIENCY_PAGE) || (page == VISUAL_PAGE);
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

static void settings_ui_fit_text(char* buf, size_t buflen, cptr text,
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

    if ((int)strlen(text) <= max_chars)
    {
        SDL_strlcpy(buf, text, buflen);
    }
    else if (max_chars <= 3)
    {
        strnfmt(buf, buflen, "%.*s", max_chars, text);
    }
    else
    {
        strnfmt(buf, buflen, "%.*s...", max_chars - 3, text);
    }
}

static cptr settings_ui_pick_label(int max_chars, cptr long_label,
    cptr medium_label, cptr short_label)
{
    cptr labels[3] = { long_label, medium_label, short_label };

    for (int i = 0; i < 3; i++)
    {
        if (labels[i] && labels[i][0] && (int)strlen(labels[i]) <= max_chars)
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

    desired_value = (int)strlen(value);
    value_budget = MIN(max_chars - 4,
        MAX(min_value_chars, MIN(desired_value, (max_chars * 3) / 5)));

    if (value_budget < 1)
        value_budget = MIN(max_chars, MAX(1, max_chars / 2));

    settings_ui_fit_text(value_buf, sizeof(value_buf), value, value_budget);
    label_budget = max_chars - (int)strlen(value_buf) - 2;

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

static cptr settings_ui_wrap_line(cptr text, int max_chars, char* buf,
    size_t buflen)
{
    cptr start;
    cptr end;
    cptr split = NULL;
    size_t copy_len;

    if (!buf || !buflen)
        return text;

    buf[0] = '\0';

    if (!text)
        return text;

    while (*text == ' ')
        text++;

    if (!*text || max_chars <= 0)
        return text;

    start = text;
    end = text;

    while (*end && *end != '\n' && (end - start) < max_chars)
    {
        if (*end == ' ')
            split = end;
        end++;
    }

    if (*end == '\n')
    {
        copy_len = (size_t)(end - start);
        text = end + 1;
    }
    else if (*end && (end - start) >= max_chars)
    {
        if (split && split > start)
        {
            copy_len = (size_t)(split - start);
            text = split + 1;
        }
        else
        {
            copy_len = (size_t)(end - start);
            text = end;
        }
    }
    else
    {
        copy_len = (size_t)(end - start);
        text = end;
    }

    if (copy_len >= buflen)
        copy_len = buflen - 1;

    memcpy(buf, start, copy_len);
    buf[copy_len] = '\0';

    while (*text == ' ')
        text++;

    return text;
}

static void settings_ui_draw_wrapped_block(int row, int col, int max_width,
    int max_rows, byte attr, cptr text)
{
    int i;
    cptr rest = text;
    char line_buf[160];

    if (max_width < 1 || max_rows <= 0)
        return;

    for (i = 0; i < max_rows; i++)
        Term_erase(col, row + i, max_width);

    for (i = 0; i < max_rows && rest && *rest; i++)
    {
        rest = settings_ui_wrap_line(rest, max_width, line_buf,
            sizeof(line_buf));
        Term_putstr(col, row + i, max_width, attr, line_buf);
    }
}

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
    case OPT_hitpoint_warning:
        return compact ? (narrow ? "HP warn" : "HP warning")
                       : "Hitpoint warning threshold (0% to 90%)";
    case OPT_main_combat_rolls:
        return compact ? (narrow ? "Combat lines" : "Combat roll lines")
                       : "Main terminal combat roll lines (0=off, 1-4=lines)";
    case OPT_hide_left_panel:
        return compact ? (narrow ? "Compact panel" : "Compact left panel")
                       : "Hide Left Panel [Alt+P]";
    case OPT_hidden_left_panel_mode:
        return compact ? (narrow ? "Panel place" : "Hidden panel")
                       : "Hidden-panel placement";
    case OPT_top_status_line:
        return compact ? (narrow ? "Top status" : "Top status line")
                       : "Top Status Line (No Msg Row)";
    case OPT_hide_supporting_panes_fullscreen:
        return compact ? (narrow ? "Hide panes FS" : "Hide panes full-screen")
                       : "Hide supporting panes on full-screen screens";
    case OPT_show_level_entry_banner:
        return compact ? (narrow ? "Entry text" : "Entry narrative")
                       : "Level entry narrative";
    case OPT_show_partition_narrative:
        return compact ? (narrow ? "Partition text" : "Partition narrative")
                       : "Partition transition narrative";
    case OPT_ability_desc_mode:
        return compact ? (narrow ? "Ability text" : "Ability descriptions")
                       : "Ability descriptions (0=lore+effect, 1=effect+lore, 2=effect)";
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
    case OPT_inventory_selection_square:
        return compact ? (narrow ? "Item frame" : "Item select frame")
                       : "Item selection frame";
    case OPT_supply_menu_random_icons:
        return compact ? (narrow ? "Supply icons" : "Supply icon mode")
                       : "Supply group icon mode";
    case OPT_supply_menu_hide_flavor_compact:
        return compact ? (narrow ? "Hide flavors" : "Hide supply flavors")
                       : "Hide supply flavors in compact mode";
    case OPT_intro_style:
        return compact ? (narrow ? "Welcome art" : "Welcome screen")
                       : "Welcome screen style";
    case OPT_banner_message_stairs:
        return compact ? "Banner layout" : "Banner message layout";
    case OPT_narrative_banner_turns:
        return compact ? "Banner turns" : "Narrative banner turns";
    case OPT_unlock_blitz_mode:
        return compact ? (narrow ? "Blitz unlocked" : "Unlock Blitz Mode")
                       : "Unlock Blitz Mode";
    default:
        break;
    }

    if (compact)
    {
        switch (opt)
        {
        case OPT_system_beep: return narrow ? "Beep" : "Error beep";
        case OPT_quick_messages: return narrow ? "Quick prompts" : "Quick prompts";
        case OPT_auto_more: return narrow ? "Auto more" : "Auto -more-";
        case OPT_easy_main_menu: return narrow ? "Esc menu" : "Esc main menu";
        case OPT_hjkl_movement: return narrow ? "hjkl move" : "hjkl movement";
        case OPT_angband_keyset: return narrow ? "Angband keys" : "Angband keyset";
        case OPT_space_acts_as_comma: return narrow ? "Space = comma" : "Space acts as comma";
        case OPT_story_lists: return narrow ? "Story look" : "Story font: look/target";
        case OPT_story_lists_inven: return narrow ? "Story inv" : "Story font: inv menu";
        case OPT_story_lists_equip: return narrow ? "Story equip" : "Story font: equip menu";
        case OPT_story_character_sheet: return narrow ? "Story sheet" : "Story font: char sheet";
        case OPT_story_lists_inven_pane: return narrow ? "Story inv pane" : "Story font: inv pane";
        case OPT_story_lists_equip_pane: return narrow ? "Story eq pane" : "Story font: equip pane";
        case OPT_story_monster_desc: return narrow ? "Story mon desc" : "Story font: monster desc";
        case OPT_story_monster_desc_pane: return narrow ? "Story mon pane" : "Story font: monster pane";
        case OPT_valorous_oath_auto_attack_safety: return narrow ? "Valorous safety" : "Valorous oath safety";
        case OPT_pacifist_attack_warning: return narrow ? "Pacifist warn" : "Warn before attacks";
        case OPT_forgo_attacking_unwary: return narrow ? "Skip unwary hits" : "Forgo unwary attacks";
        case OPT_assassination_over_charge: return narrow ? "Stealth over charge" : "Assassination over Charge";
        case OPT_stop_singing_on_rest: return narrow ? "Stop song on rest" : "Stop singing on rest";
        case OPT_know_monster_info: return narrow ? "Know monsters" : "Know monster info";
        case OPT_visual_recognition: return narrow ? "Need light to spot" : "Need light to spot";
        case OPT_disable_skeleton_note_tutorial: return narrow ? "Hide skeleton tips" : "Hide skeleton tutorials";
        case OPT_smaller_level_size: return narrow ? "Smaller levels" : "Smaller level size";
        case OPT_more_stairs: return narrow ? "More stairs" : "Extra stairs";
        case OPT_instant_run: return narrow ? "Fast running" : "Faster running";
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
        case OPT_show_smithing_difficulty: return narrow ? "Smith dbg items" : "Debug smithing in items";
        case OPT_show_smithing_difficulty_look: return narrow ? "Smith dbg look" : "Debug smithing in look";
        case OPT_look_nearby_filter_default: return narrow ? "Look near def" : "Look nearby default";
        case OPT_show_level_generation_debug: return narrow ? "Dbg lvl screen" : "Debug level screen";
        case OPT_show_elemental_item_rolls: return narrow ? "Dbg elem items" : "Debug elemental items";
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

static void option_apply_side_effects(int opt)
{
    if (opt == OPT_story_lists_inven_pane || opt == OPT_story_lists_equip_pane)
        redraw_inven_equip_subwindows();
    if (opt == OPT_story_monster_desc_pane)
        redraw_monster_subwindows();
    if (opt == OPT_top_status_line && p_ptr) {
        p_ptr->update |= (PU_PANEL);
        p_ptr->redraw |= (PR_MAP | PR_EXTRA | PR_DEPTH);
    }
    if (opt == OPT_hide_supporting_panes_fullscreen)
        sdl_refresh_supporting_panes_layout();
    if (opt == OPT_stealth_vision || opt == OPT_visual_recognition
        || opt == OPT_sleep_icon)
        p_ptr->redraw |= (PR_MAP);
}

extern void do_cmd_options_aux(int page, cptr info)
{
    char ch;

    int i, k = 0, n = 0;
    int scroll = 0;

    int opt[OPT_PAGE_PER];

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

    /* Scan the options */
    for (i = 0; i < OPT_PAGE_PER; i++)
    {
        /* Collect options on this "page" */
        if (option_page[page][i] != OPT_NONE)
        {
            opt[n++] = option_page[page][i];
        }
    }
    
    /* Special case: Sound page uses custom display instead of standard options */
    if (is_sound_page)
    {
        n = SOUND_OPT_MAX;
    }

    /* Interact with the player */
    while (true)
    {
        int first_row = 3;
        int footer_rows = (page == CHALLENGE_PAGE) ? 4 : 2;
        int visible_rows = Term->hgt - footer_rows - first_row;
        int total_rows = n + option_group_total_rows(groups);
        int selected_display_row = k + option_group_count_before(groups, k);
        int group_index = 0;
        int display_row = 0;
        int max_scroll;

        if (visible_rows < 1)
            visible_rows = 1;

        max_scroll = total_rows - visible_rows;
        if (max_scroll < 0)
            max_scroll = 0;

        if (selected_display_row < scroll)
            scroll = selected_display_row;
        else if (selected_display_row >= scroll + visible_rows)
            scroll = selected_display_row - visible_rows + 1;
        if (scroll > max_scroll)
            scroll = max_scroll;

        Term_clear();

        /* Prompt XXX XXX XXX */
        strnfmt(buf, sizeof(buf), "%s", info);
        settings_ui_put_fitted(1, 2, TERM_WHITE, buf);

        /* Display the options */
        for (i = 0; i < n; i++)
        {
            byte a = TERM_WHITE;
            int row;

            while (groups && groups[group_index].before_index == i)
            {
                row = first_row + display_row - scroll;
                if (row >= first_row && row < first_row + visible_rows)
                    Term_putstr(2, row, -1, TERM_SLATE, groups[group_index].label);
                display_row++;
                group_index++;
            }

            /* Color current option */
            if (i == k)
                a = TERM_L_BLUE;

            /* Display the option text */
            buf[0] = '\0';
            if (is_sound_page)
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
            else if (opt[i] == OPT_hitpoint_warning)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d%%",
                    op_ptr->hitpoint_warn * 10);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_hide_left_panel)
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    get_sdl_hide_left_panel() ? "yes" : "no ");
            }
            else if (opt[i] == OPT_hidden_left_panel_mode)
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    (get_sdl_hidden_left_panel_mode()
                        == HIDDEN_LEFT_PANEL_TOPLINE)
                    ? "Second row"
                    : "Top left");
            }
            else if (opt[i] == OPT_main_combat_rolls)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d",
                    op_ptr->main_combat_rolls);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_show_level_entry_banner)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->level_entry_narrative_mode)
                {
                case LEVEL_ENTRY_NARRATIVE_BANNER:
                    mode_str = compact ? "Banner" : "Banner without delay";
                    break;
                case LEVEL_ENTRY_NARRATIVE_MESSAGE: mode_str = "Message"; break;
                case LEVEL_ENTRY_NARRATIVE_OFF:     mode_str = "Off"; break;
                default:
                    mode_str = compact ? "Banner delay" : "Banner with delay";
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
                case PARTITION_NARRATIVE_BANNER:
                    mode_str = compact ? "Banner" : "Banner without delay";
                    break;
                case PARTITION_NARRATIVE_OFF:     mode_str = "Off"; break;
                default:                          mode_str = "Message"; break;
                }
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_ability_desc_mode)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->ability_desc_mode)
                {
                case 1:  mode_str = compact ? "1 effect+lore" : "1 (effect+lore)"; break;
                case 2:  mode_str = compact ? "2 effect only" : "2 (effect only)"; break;
                default: mode_str = compact ? "0 lore+effect" : "0 (lore+effect)"; break;
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
                    "Flame Imperishable", "Oath of Feanor",
                    "Twilight of Valinor", "Song of Luthien",
                    "Words of Hurin", "Starlight on Cuivienen",
                    "Lament of the Noldor", "Random"
                };
                byte m = op_ptr->intro_style;
                if (m > INTRO_STYLE_RANDOM) m = INTRO_STYLE_FLAME;
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    is_names[m]);
            }
            else if (opt[i] == OPT_banner_message_stairs)
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    op_ptr->opt[opt[i]] ? "Stair" : "Straight");
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
            else
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    op_ptr->opt[opt[i]] ? "yes" : "no ");
            }

            row = first_row + display_row - scroll;
            if (row >= first_row && row < first_row + visible_rows)
                c_prt(a, buf, row, 4);
            display_row++;
        }

        if (total_rows > visible_rows)
        {
            strnfmt(buf, sizeof(buf), "(scroll: rows %d-%d of %d)",
                scroll + 1, MIN(scroll + visible_rows, total_rows), total_rows);
            settings_ui_put_fitted(Term->hgt - 2, 2, TERM_SLATE, buf);
        }

        if (page == CHALLENGE_PAGE)
        {
            settings_ui_put_fitted(Term->hgt - 4, 2, TERM_L_WHITE,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "Challenge options can only be changed during character creation",
                    "Challenge options only change during character creation",
                    "Challenge options only change at birth"));
            settings_ui_put_fitted(Term->hgt - 3, 2, TERM_L_WHITE,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "or on the very first turn",
                    "or on the first turn",
                    "or on turn 1"));

            if (playerturn == 0)
            {
                settings_ui_put_fitted(Term->hgt - 1, 2, TERM_SLATE,
                    settings_ui_pick_label(settings_ui_line_width(2),
                        "(direction keys to set, Return/Escape to accept)",
                        "(direction keys to set, Enter/Esc to accept)",
                        "(arrows set, Enter/Esc accept)"));
            }
            else
            {
                settings_ui_put_fitted(Term->hgt - 1, 2, TERM_SLATE,
                    settings_ui_pick_label(settings_ui_line_width(2),
                        "(press Return to go back)",
                        "(press Enter to go back)",
                        "(Enter goes back)"));
            }
        }
        else
        {
            settings_ui_put_fitted(Term->hgt - 1, 2, TERM_SLATE,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "(direction keys to set, Return/Escape to accept)",
                    "(direction keys to set, Enter/Esc to accept)",
                    "(arrows set, Enter/Esc accept)"));
        }

        /* Hilite current option */
        move_cursor(first_row + selected_display_row - scroll,
            MIN(54, Term->wid - 1));

        /* Get a key */
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        /*
         * HACK - Try to translate the key into a direction
         * to allow using the roguelike keys for navigation.
         */
        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        /* Analyze */
        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
        {
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
                if (is_sound_page)
                {
                    switch (k)
                    {
                    case SOUND_OPT_ENABLED:
                        sound_cfg->enabled = !sound_cfg->enabled;
                        use_sound = sound_cfg->enabled;
                        break;
                    case SOUND_OPT_COMBAT_ENABLED:
                        sound_cfg->enable_combat = !sound_cfg->enable_combat;
                        break;
                    case SOUND_OPT_MONSTER_HITS_ENABLED:
                        sound_cfg->enable_monster_hits = !sound_cfg->enable_monster_hits;
                        break;
                    case SOUND_OPT_INVENTORY_ENABLED:
                        sound_cfg->enable_inventory = !sound_cfg->enable_inventory;
                        break;
                    case SOUND_OPT_WALK_ENABLED:
                        sound_cfg->enable_walk = !sound_cfg->enable_walk;
                        break;
                    case SOUND_OPT_DOORS_ENABLED:
                        sound_cfg->enable_doors = !sound_cfg->enable_doors;
                        break;
                    case SOUND_OPT_TRAPS_ENABLED:
                        sound_cfg->enable_traps = !sound_cfg->enable_traps;
                        break;
                    case SOUND_OPT_MUSIC_MAIN_ENABLED:
                        sound_cfg->music_main_enabled = !sound_cfg->music_main_enabled;
                        break;
                    case SOUND_OPT_MUSIC_AMBIENT_ENABLED:
                        sound_cfg->music_ambient_enabled = !sound_cfg->music_ambient_enabled;
                        break;
                    default:
                        break;
                    }
                }
                else if (opt[k] == OPT_delay_factor)
                {
                    op_ptr->delay_factor = (op_ptr->delay_factor < 9)
                        ? op_ptr->delay_factor + 1
                        : 0;
                }
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn < 9)
                        ? op_ptr->hitpoint_warn + 1
                        : 0;
                }
                else if (opt[k] == OPT_hide_left_panel)
                {
                    set_sdl_hide_left_panel(!get_sdl_hide_left_panel());
                    sdl_request_redraw();
                }
                else if (opt[k] == OPT_hidden_left_panel_mode)
                {
                    set_sdl_hidden_left_panel_mode(
                        (get_sdl_hidden_left_panel_mode()
                            == HIDDEN_LEFT_PANEL_TOPLINE)
                        ? HIDDEN_LEFT_PANEL_TOP_LEFT
                        : HIDDEN_LEFT_PANEL_TOPLINE);
                    if (p_ptr)
                        p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA
                            | PR_HEALTHBAR | PR_MAP);
                }
                else if (opt[k] == OPT_main_combat_rolls)
                {
                    op_ptr->main_combat_rolls = (op_ptr->main_combat_rolls < 4)
                        ? op_ptr->main_combat_rolls + 1
                        : 0;

                    clear_main_combat_rolls_area();
                    display_main_combat_rolls();
                    p_ptr->redraw |= (PR_MAP);
                }
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode < LEVEL_ENTRY_NARRATIVE_OFF)
                        ? op_ptr->level_entry_narrative_mode + 1
                        : LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode < PARTITION_NARRATIVE_OFF)
                        ? op_ptr->partition_narrative_mode + 1
                        : PARTITION_NARRATIVE_BANNER;
                }
                else if (opt[k] == OPT_intro_style)
                {
                    /* Toggle cycles forward */
                    op_ptr->intro_style
                        = (op_ptr->intro_style < INTRO_STYLE_RANDOM)
                        ? op_ptr->intro_style + 1
                        : INTRO_STYLE_FLAME;
                }
                else if (opt[k] == OPT_ability_desc_mode)
                {
                    op_ptr->ability_desc_mode = (op_ptr->ability_desc_mode < 2)
                        ? op_ptr->ability_desc_mode + 1
                        : 0;
                }
                else if (opt[k] == OPT_vault_drop_frequency)
                {
                    op_ptr->vault_drop_frequency
                        = (op_ptr->vault_drop_frequency < VDF_PLENTIFUL)
                        ? op_ptr->vault_drop_frequency + 1
                        : VDF_NORMAL;
                }
                else if (opt[k] == OPT_min_depth_timer_mode)
                {
                    op_ptr->min_depth_timer_mode
                        = (op_ptr->min_depth_timer_mode < MIN_DEPTH_TIMER_MODE_MAX)
                        ? op_ptr->min_depth_timer_mode + 1
                        : MIN_DEPTH_TIMER_MODE_NORMAL;
                }
                else if (opt[k] == OPT_noble_item_spawn_mode)
                {
                    op_ptr->noble_item_spawn_mode
                        = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_RESTRICTED)
                        ? NOBLE_ITEM_SPAWN_INCLUDE_VAULTS
                        : NOBLE_ITEM_SPAWN_RESTRICTED;
                }
                else if (opt[k] == OPT_narrative_banner_turns)
                {
                    op_ptr->narrative_banner_turns =
                        (op_ptr->narrative_banner_turns < NARRATIVE_BANNER_TURNS_MAX)
                        ? op_ptr->narrative_banner_turns + 1
                        : 0;
                }
                else
                {
                    op_ptr->opt[opt[k]] = !op_ptr->opt[opt[k]];
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

        case 'y':
        case '6':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                if (is_sound_page)
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
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn < 9)
                        ? op_ptr->hitpoint_warn + 1
                        : 9;
                }
                else if (opt[k] == OPT_hide_left_panel)
                {
                    set_sdl_hide_left_panel(true);
                    sdl_request_redraw();
                }
                else if (opt[k] == OPT_hidden_left_panel_mode)
                {
                    set_sdl_hidden_left_panel_mode(
                        HIDDEN_LEFT_PANEL_TOPLINE);
                    if (p_ptr)
                        p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA
                            | PR_HEALTHBAR | PR_MAP);
                }
                else if (opt[k] == OPT_main_combat_rolls)
                {
                    op_ptr->main_combat_rolls = (op_ptr->main_combat_rolls < 4)
                        ? op_ptr->main_combat_rolls + 1
                        : 4;

                    /* Clear all 4 lines and refresh display when option changes */
                    clear_main_combat_rolls_area();
                    display_main_combat_rolls();
                    p_ptr->redraw |= (PR_MAP);
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
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode < PARTITION_NARRATIVE_OFF)
                        ? op_ptr->partition_narrative_mode + 1
                        : PARTITION_NARRATIVE_OFF;
                }
                else if (opt[k] == OPT_ability_desc_mode)
                {
                    op_ptr->ability_desc_mode = (op_ptr->ability_desc_mode < 2)
                        ? op_ptr->ability_desc_mode + 1
                        : 2;
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
                if (is_sound_page)
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
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn > 0)
                        ? op_ptr->hitpoint_warn - 1
                        : 0;
                }
                else if (opt[k] == OPT_hide_left_panel)
                {
                    set_sdl_hide_left_panel(false);
                    sdl_request_redraw();
                }
                else if (opt[k] == OPT_hidden_left_panel_mode)
                {
                    set_sdl_hidden_left_panel_mode(
                        HIDDEN_LEFT_PANEL_TOP_LEFT);
                    if (p_ptr)
                        p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA
                            | PR_HEALTHBAR | PR_MAP);
                }
                else if (opt[k] == OPT_main_combat_rolls)
                {
                    op_ptr->main_combat_rolls = (op_ptr->main_combat_rolls > 0)
                        ? op_ptr->main_combat_rolls - 1
                        : 0;

                    /* Clear all 4 lines and refresh display when option changes */
                    clear_main_combat_rolls_area();
                    display_main_combat_rolls();
                    p_ptr->redraw |= (PR_MAP);
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
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_BANNER)
                        ? op_ptr->partition_narrative_mode - 1
                        : PARTITION_NARRATIVE_BANNER;
                }
                else if (opt[k] == OPT_ability_desc_mode)
                {
                    op_ptr->ability_desc_mode = (op_ptr->ability_desc_mode > 0)
                        ? op_ptr->ability_desc_mode - 1
                        : 0;
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
 * Write all current options to the given preference file in the
 * lib/user directory. Modified from KAmband 1.8.
 */
static errr option_dump(cptr fname)
{
    static cptr mark = "Options Dump";

    int i, j;

    SDL_IOStream* fff;

    char buf[1024];

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("option_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old options */
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
    SDL_IOprintf(fff, "# Automatic option dump\n\n");

    /* Dump options (skip cheat, adult, score) */
    for (i = 0; i < OPT_CHEAT; i++)
    {
        /* Require a real option */
        if (!option_text[i])
            continue;

        /* Comment */
        SDL_IOprintf(fff, "# Option '%s'\n", option_desc[i]);

        /* Dump the option */
        if (op_ptr->opt[i])
        {
            SDL_IOprintf(fff, "Y:%s\n", option_text[i]);
        }
        else
        {
            SDL_IOprintf(fff, "X:%s\n", option_text[i]);
        }

        /* Skip a line */
        SDL_IOprintf(fff, "\n");
    }

    /* Dump window flags */
    for (i = 1; i < ANGBAND_TERM_MAX; i++)
    {
        /* Require a real window */
        if (!angband_term[i])
            continue;

        /* Check each flag */
        for (j = 0; j < 32; j++)
        {
            /* Require a real flag */
            if (!window_flag_desc[j])
                continue;

            /* Comment */
            SDL_IOprintf(fff, "# Window '%s', Flag '%s'\n", angband_term_name[i],
                window_flag_desc[j]);

            /* Dump the flag */
            if (op_ptr->window_flag[i] & (1L << j))
            {
                SDL_IOprintf(fff, "W:%d:%d:1\n", i, j);
            }
            else
            {
                SDL_IOprintf(fff, "W:%d:%d:0\n", i, j);
            }

            /* Skip a line */
            SDL_IOprintf(fff, "\n");
        }
    }

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

/*
 * Display and manage SDL pane settings
 * Interactive menu to edit SDL configuration
 */
static int get_supporting_pane_config_count(void);
static void do_cmd_supporting_pane_layout_editor(bool* settings_changed);
static void do_cmd_supporting_pane_font_editor(bool* settings_changed);
static void do_cmd_touch_pane_button_editor(bool* settings_changed);
static const char* pane_type_short_name(enum pane_type type);

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
        PANE_SETTING_SAVE_RETURN,
        PANE_SETTING_COUNT
    };
    int k = 0;
    int n = PANE_SETTING_COUNT;
    bool done = false;
    bool settings_changed = false;
    int dir;
    const char* config_path = get_sdl_config_path();
    const char* config_label = (config_path && config_path[0]) ? config_path : "sil_sdl.json";
    
    /* Save screen */
    screen_save();
    
    while (!done)
    {
        int row_width;
        int label_hint;

        /* Clear screen */
        Term_clear();

        /* Display title */
        settings_ui_put_fitted(1, 2, TERM_WHITE, "SDL Pane Settings");

        /* Display current settings */
        char buf[96];
        char value_buf[32];
        int y0 = 3;
        byte a;
        char font_value[24];
        row_width = settings_ui_line_width(2);
        label_hint = MAX(10, row_width - 12);

        /* Option 0: Minimum Terminal Size */
        a = (k == PANE_SETTING_MIN_TERMINAL_SIZE) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Minimum Terminal Size",
                "Min Terminal Size",
                "Min Terminal"),
            sdl_min_terminal_mode_label(get_sdl_min_terminal_mode()),
            row_width, 10);
        c_prt(a, buf, y0 + 0, 2);

        /* Option 1: Main View Scale */
        a = (k == PANE_SETTING_MAIN_VIEW_SCALE) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(value_buf, sizeof(value_buf), "%d", get_sdl_main_view_scale());
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Main View Scale (1-max) [Alt++/-]",
                "Main View Scale [Alt++/-]",
                "View Scale"),
            value_buf, row_width, 3);
        c_prt(a, buf, y0 + 1, 2);

        /* Option 2: Enable Side Panes */
        a = (k == PANE_SETTING_ENABLE_SIDE_PANES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Enable Side Panes [Alt+I]",
                "Side Panes [Alt+I]",
                "Side Panes"),
            get_sdl_enable_right_panes() ? "yes" : "no",
            row_width, 3);
        c_prt(a, buf, y0 + 2, 2);

        /* Option 3: Enable Bottom Panes */
        a = (k == PANE_SETTING_ENABLE_BOTTOM_PANES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Enable Bottom Panes [Alt+L]",
                "Bottom Panes [Alt+L]",
                "Bottom Panes"),
            get_sdl_enable_bottom_panes() ? "yes" : "no",
            row_width, 3);
        c_prt(a, buf, y0 + 3, 2);

        /* Option 4: Fullscreen */
        a = (k == PANE_SETTING_FULLSCREEN) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf), "Fullscreen",
            get_sdl_fullscreen() ? "yes" : "no", row_width, 3);
        c_prt(a, buf, y0 + 4, 2);

        /* Option 5: Tiles */
        a = (k == PANE_SETTING_TILES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf), "Tiles",
            get_sdl_tiles() ? "yes" : "no", row_width, 3);
        c_prt(a, buf, y0 + 5, 2);

        /* Option 6: Use Unsafe Area */
        a = (k == PANE_SETTING_USE_UNSAFE_AREA) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Use Unsafe Area (notch/cutout area)",
                "Use Unsafe Area",
                "Unsafe Area"),
            get_sdl_use_unsafe_area() ? "yes" : "no",
            row_width, 3);
        c_prt(a, buf, y0 + 6, 2);

        /* Option 7: White Pane Borders */
        a = (k == PANE_SETTING_WHITE_PANE_BORDERS) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "White Pane Borders",
                "White Pane Borders",
                "White Borders"),
            get_sdl_show_pane_borders() ? "white" : "black",
            row_width, 5);
        c_prt(a, buf, y0 + 7, 2);

        /* Option 8: Hide supporting panes on full-screen screens */
        a = (k == PANE_SETTING_HIDE_FULLSCREEN_PANES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Hide supporting panes on full-screen screens",
                "Hide panes on full-screen screens",
                "Hide panes on full-screen"),
            op_ptr->opt[OPT_hide_supporting_panes_fullscreen] ? "yes" : "no",
            row_width, 3);
        c_prt(a, buf, y0 + 8, 2);

        /* Option 9: Aux View Font Size */
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
        c_prt(a, buf, y0 + 9, 2);

        /* Option 10: View Pane Configuration (supporting panes only) */
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
        c_prt(a, buf, y0 + 10, 2);

        /* Option 11: Pane Font Sizes */
        a = (k == PANE_SETTING_PANE_FONT_SIZES) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_fit_text(buf, sizeof(buf),
            settings_ui_pick_label(row_width,
                "Pane Font Sizes",
                "Pane Fonts",
                "Pane Fonts"),
            row_width);
        c_prt(a, buf, y0 + 11, 2);

        /* Option 12: Open SDL Config File */
        a = (k == PANE_SETTING_OPEN_CONFIG_FILE) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Open SDL Config File",
                "Open SDL Config",
                "Open Config"),
            sdl_config_path_leaf(config_label), row_width, 12);
        c_prt(a, buf, y0 + 12, 2);

        /* Option 13: Save/Return */
        a = (k == PANE_SETTING_SAVE_RETURN) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_fit_text(buf, sizeof(buf),
            settings_changed ? "Save Changes and Return"
                             : "Return to Options Menu",
            row_width);
        c_prt(a, buf, y0 + 13, 2);

        /* Display help */
        int y = Term->hgt - 3;
        if (settings_changed)
        {
            settings_ui_put_fitted(y++, 2, TERM_YELLOW,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "Settings changed - changes take effect immediately.",
                    "Settings changed - active immediately.",
                    "Changes apply immediately."));
            settings_ui_put_fitted(y++, 2, TERM_YELLOW,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "Will be saved to your SDL config file on exit.",
                    "Saved to your SDL config on exit.",
                    "Saved on exit."));
        }
        settings_ui_put_fitted(y++, 2, TERM_SLATE,
            settings_ui_pick_label(settings_ui_line_width(2),
                "(direction keys to set, 0 = auto font, o = open config, Return/Escape to accept)",
                "(arrows move, 4/6 or y/n set, 0 auto, o open config, Enter/Esc)",
                "(arrows move, 4/6 set, 0 auto, o config, Enter/Esc)"));

        /* Get key */
        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;
        
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
                do_cmd_supporting_pane_layout_editor(&settings_changed);
                break;
            }
            if (k == PANE_SETTING_PANE_FONT_SIZES) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
                break;
            }
            if (k == PANE_SETTING_OPEN_CONFIG_FILE) /* Open SDL Config File */
            {
                sdl_open_config_file();
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
            k = (n + k - 1) % n;
            break;
        }
        
        case '2':
        {
            /* Move down */
            k = (k + 1) % n;
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
            if (k == PANE_SETTING_ENABLE_SIDE_PANES)
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
                do_cmd_supporting_pane_layout_editor(&settings_changed);
            }
            else if (k == PANE_SETTING_PANE_FONT_SIZES) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
            }
            else if (k == PANE_SETTING_OPEN_CONFIG_FILE) /* Open SDL Config File */
            {
                sdl_open_config_file();
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
                if (val > 1)
                {
                    set_sdl_main_view_scale(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
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
    screen_load();
}


static const char* pane_type_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INVENTORY";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHARACTER";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MONSTERS";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNKNOWN";
    }
}

static void do_cmd_supporting_pane_font_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (type == PANE_MAIN)
            continue;
        pane_indices[pane_count++] = i;
    }

    screen_save();

    if (pane_count <= 0)
    {
        Term_clear();
        Term_putstr(2, 1, -1, TERM_L_BLUE, "Supporting Pane Fonts");
        Term_putstr(2, 3, -1, TERM_WHITE, "No supporting panes are configured.");
        Term_putstr(2, Term->hgt - 1, -1, TERM_L_BLUE, "Press any key to return...");
        Term_fresh();
        (void)inkey();
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
            int y0 = 4;
            int row_width;
            int term_wid;

            Term_clear();
            term_wid = settings_ui_term_wid();
            row_width = settings_ui_line_width(2);
            settings_ui_put_fitted(1, 2, TERM_L_BLUE, "Supporting Pane Fonts");
            settings_ui_put_fitted(2, 2, TERM_WHITE, "=====================");

            for (int i = 0; i < pane_count && (y0 + i) < Term->hgt - 5; i++)
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
                c_prt(a, line_buf, y0 + i, 2);
            }

            {
                int y = Term->hgt - 4;
                settings_ui_put_fitted(y++, 2, TERM_SLATE,
                    settings_ui_pick_label(term_wid - 2,
                        "Up/Down: select pane   4/6 (or n/y): change font size",
                        "Up/Down select pane   4/6 set font size",
                        "Up/Down select   4/6 set"));
                settings_ui_put_fitted(y++, 2, TERM_SLATE,
                    settings_ui_pick_label(term_wid - 2,
                        "0: auto (uses default aux font / auto main-based size)",
                        "0: auto font size",
                        "0 auto font"));
                settings_ui_put_fitted(y++, 2, TERM_SLATE,
                    settings_ui_pick_label(term_wid - 2,
                        "Changes apply immediately",
                        "Changes apply immediately",
                        "Changes apply now"));
            }

            Term_fresh();

            hide_cursor = true;
            char ch = inkey();
            hide_cursor = false;

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

    screen_load();
}

static const char* pane_type_short_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INV";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHAR";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MON";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNK";
    }
}

static const char* pane_where_short_name(enum pane_placement where)
{
    switch (where)
    {
    case PLACE_RIGHT: return "R";
    case PLACE_LEFT: return "L";
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
        if (type != PANE_MAIN)
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

    return (pane_placement_is_bottom(where) && idx != master_idx);
}

static bool supporting_pane_cols_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    return (pane_placement_is_side(where) && idx != master_idx);
}

static void supporting_pane_ensure_editable_field(int* field, const int* pane_indices,
    int pane_count, int sel)
{
    int idx;

    if (!field || pane_count <= 0 || sel < 0 || sel >= pane_count)
        return;

    idx = pane_indices[sel];
    while ((*field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
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
        enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
        int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

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

static void do_cmd_supporting_pane_layout_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (type == PANE_MAIN)
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
        Term_clear();
        Term_putstr(2, 1, -1, TERM_L_BLUE, "Supporting Pane Layout");
        Term_putstr(2, 3, -1, TERM_WHITE, "No supporting panes are configured.");
        Term_putstr(2, Term->hgt - 1, -1, TERM_L_BLUE, "Press any key to return...");
        Term_fresh();
        (void)inkey();
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
        int y0 = 4;
        int term_wid;
        int row_width;

        Term_clear();
        term_wid = settings_ui_term_wid();
        row_width = settings_ui_line_width(2);
        settings_ui_put_fitted(1, 2, TERM_L_BLUE, "Supporting Pane Layout");
        settings_ui_put_fitted(2, 2, TERM_WHITE, "======================");

        for (int i = 0; i < pane_count && (y0 + i) < Term->hgt - 5; i++)
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
                pane_placement_name(where), pane_placement_name(where),
                pane_where_short_name(where));

            settings_ui_fit_text(type_buf, sizeof(type_buf), type_label,
                MAX(4, row_width / 3));
            settings_ui_format_field(enabled_field, sizeof(enabled_field),
                enabled ? "on" : "off", i == sel && field == 0);
            settings_ui_format_field(where_field, sizeof(where_field), where_label,
                i == sel && field == 1);

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

            strnfmt(line_buf, sizeof(line_buf), "%s %s %s r%s c%s", type_buf,
                where_field, enabled_field, rows_field, cols_field);
            settings_ui_put_fitted(y0 + i, 2, a, line_buf);
        }

        {
            int y = Term->hgt - 4;
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "Up/Down: select pane   Space: choose on/off, where, rows, cols",
                    "Up/Down select pane   Space switch field",
                    "Up/Down select   Space field"));
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "4/6 (or n/y): toggle, cycle, or +/- value   0: set rows/cols to auto",
                    "4/6 or y/n: toggle, cycle, or +/- value   0: auto",
                    "4/6 cycle/set   0 auto"));
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "Each side slot shares cols with its first pane; each bottom slot shares rows",
                    "Side slots share cols; each bottom slot shares rows",
                    "Side share cols; bottom share rows"));
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "ESC/Enter: return (changes apply immediately)",
                    "ESC/Enter: return",
                    "Esc/Enter return"));
        }

        Term_fresh();

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

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
                bell("Use 4/6 to toggle enabled or cycle placement");
                break;
            }
            if (field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
            {
                bell("Rows are shared within each bottom slot");
                break;
            }
            if (field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx))
            {
                bell("Cols are shared within each side slot");
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
                    bell("Cols are shared within each side slot");
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

    screen_load();
}

static const int touch_pane_main_action_choices[] = {
    GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', KTRL('A'), 'E', 't', 'p', 'q',
    'F', KTRL('F'), 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int touch_pane_second_action_choices[] = {
    TOUCH_PANE_BIND_INHERIT, GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', KTRL('A'), 'E', 't', 'p', 'q',
    'F', KTRL('F'), 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

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
    for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++)
    {
        if (i == skip_index)
            continue;

        if (touch_pane_binding_is_confirm(
                get_sdl_touch_pane_binding_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, i)))
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
        strnfmt(buf, buflen, "Switch to %s panel", panel_name);
        return;
    }

    if (binding == INPUT_BIND_CONFIRM || binding == ' ') {
        SDL_strlcpy(buf, "Pick/Confirm", buflen);
        return;
    }

    binding_action_short(binding, buf, buflen);
}

enum {
    TOUCH_SETTING_SWIPE_ENABLED = 0,
    TOUCH_SETTING_SWIPE_UP,
    TOUCH_SETTING_SWIPE_DOWN,
    TOUCH_SETTING_SWIPE_LEFT,
    TOUCH_SETTING_SWIPE_RIGHT,
    TOUCH_SETTING_SWIPE_COUNT
};

static bool touch_setting_is_swipe_row(int row)
{
    return (row >= 0 && row < TOUCH_SETTING_SWIPE_COUNT);
}

static int touch_setting_button_index(int row)
{
    return row - TOUCH_SETTING_SWIPE_COUNT;
}

static int touch_setting_total_rows(void)
{
    return TOUCH_SETTING_SWIPE_COUNT + SDL_TOUCH_PANE_BUTTON_COUNT;
}

static int touch_setting_swipe_dir_for_row(int row)
{
    switch (row) {
    case TOUCH_SETTING_SWIPE_UP:
        return GAMEPAD_STICK_DIR_UP;
    case TOUCH_SETTING_SWIPE_DOWN:
        return GAMEPAD_STICK_DIR_DOWN;
    case TOUCH_SETTING_SWIPE_LEFT:
        return GAMEPAD_STICK_DIR_LEFT;
    case TOUCH_SETTING_SWIPE_RIGHT:
        return GAMEPAD_STICK_DIR_RIGHT;
    default:
        return -1;
    }
}

static const char* touch_setting_swipe_name(int row)
{
    switch (row) {
    case TOUCH_SETTING_SWIPE_ENABLED:
        return "Swipe Gestures";
    case TOUCH_SETTING_SWIPE_UP:
        return "Swipe Up";
    case TOUCH_SETTING_SWIPE_DOWN:
        return "Swipe Down";
    case TOUCH_SETTING_SWIPE_LEFT:
        return "Swipe Left";
    case TOUCH_SETTING_SWIPE_RIGHT:
        return "Swipe Right";
    default:
        return "";
    }
}

static void do_cmd_touch_pane_button_editor(bool* settings_changed)
{
    int highlight = 0;
    int top = 0;
    int panel = SDL_TOUCH_PANE_PANEL_MAIN;
    bool done = false;
    bool changed = false;
    int term_w, term_h;
    const int list_start_row = 5;

    screen_save();

    while (!done)
    {
        int row;
        int visible_rows;
        int row_width;
        int total_rows = touch_setting_total_rows();

        Term_get_size(&term_w, &term_h);
        row_width = settings_ui_line_width(2);
        visible_rows = term_h - list_start_row - 6;
        if (visible_rows < 5)
            visible_rows = 5;

        if (highlight < 0)
            highlight = 0;
        if (highlight >= total_rows)
            highlight = total_rows - 1;

        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;

        Term_clear();
        settings_ui_put_fitted(1, 2, TERM_L_BLUE, "Touch Settings");
        settings_ui_put_fitted(2, 2, TERM_WHITE, "==============");

        row = list_start_row;
        for (int i = top; i < total_rows && i < top + visible_rows; i++)
        {
            char action_buf[80];
            char label_buf[SDL_TOUCH_PANE_LABEL_LEN];
            char left_buf[64];
            char line_buf[128];
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            if (touch_setting_is_swipe_row(i))
            {
                int swipe_dir = touch_setting_swipe_dir_for_row(i);

                strnfmt(left_buf, sizeof(left_buf), "%s", touch_setting_swipe_name(i));
                if (i == TOUCH_SETTING_SWIPE_ENABLED)
                {
                    SDL_strlcpy(action_buf, get_sdl_touch_swipe_enabled() ? "On" : "Off",
                        sizeof(action_buf));
                }
                else
                {
                    touch_pane_action_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN,
                        get_sdl_touch_swipe_binding(swipe_dir), action_buf, sizeof(action_buf));
                }
            }
            else
            {
                int button_index = touch_setting_button_index(i);

                get_sdl_touch_pane_button_label_for_panel(panel, button_index, label_buf, sizeof(label_buf));
                touch_pane_action_label_for_panel(panel,
                    get_sdl_touch_pane_binding_for_panel(panel, button_index), action_buf, sizeof(action_buf));

                if (label_buf[0])
                    strnfmt(left_buf, sizeof(left_buf), "%s %s",
                        get_sdl_touch_pane_slot_name(button_index), label_buf);
                else
                    strnfmt(left_buf, sizeof(left_buf), "%s",
                        get_sdl_touch_pane_slot_name(button_index));
            }

            settings_ui_format_pair_line(line_buf, sizeof(line_buf), left_buf,
                action_buf, row_width, 14);
            c_prt(a, line_buf, row++, 2);
        }

        row = list_start_row + visible_rows + 1;
        {
            char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
            char info_buf[96];

            get_sdl_touch_pane_panel_name(panel, panel_name, sizeof(panel_name));
            strnfmt(info_buf, sizeof(info_buf), "Swipe settings are global. Editing %s panel%s",
                panel_name, (panel == SDL_TOUCH_PANE_PANEL_SECOND) ? " (empty = main panel)" : "");
            settings_ui_put_fitted(3, 2, TERM_SLATE, info_buf);
        }
        settings_ui_put_fitted(row++, 2, TERM_SLATE,
            settings_ui_pick_label(row_width,
                "Up/Down: select item   4/6: previous/next action   l/View: rename button label",
                "Up/Down select   4/6 action   l/View rename",
                "Up/Down select   4/6 action"));
        settings_ui_put_fitted(row++, 2, TERM_SLATE,
            settings_ui_pick_label(row_width,
                "Space on Swipe Gestures toggles on/off   Tab switches button panel   x resets selected",
                "Space toggles swipes   Tab switch   x reset",
                "Space toggles   Tab switch   x reset"));
        settings_ui_put_fitted(row++, 2, TERM_SLATE,
            settings_ui_pick_label(row_width,
                "p/Hero: rename panel   M/Map: reset all   Main panel must keep Pick/Confirm",
                "p rename panel   Map reset all   Keep main confirm",
                "p rename   Map all   Keep confirm"));

        Term_fresh();

        hide_cursor = true;
        char ch = inkey();
        hide_cursor = false;

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
            if (touch_setting_is_swipe_row(highlight))
            {
                int swipe_dir = touch_setting_swipe_dir_for_row(highlight);

                if (highlight == TOUCH_SETTING_SWIPE_ENABLED)
                {
                    set_sdl_touch_swipe_enabled(false);
                }
                else
                {
                    int choice_count = 0;
                    const int* choices = touch_swipe_action_choices(&choice_count);
                    int idx = touch_action_choice_index(choices, choice_count,
                        get_sdl_touch_swipe_binding(swipe_dir));

                    idx = (choice_count + idx - 1) % choice_count;
                    set_sdl_touch_swipe_binding(swipe_dir, choices[idx]);
                }
            }
            else
            {
                int button_index = touch_setting_button_index(highlight);
                int choice_count = 0;
                const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
                int idx = touch_pane_action_choice_index(panel,
                    get_sdl_touch_pane_binding_for_panel(panel, button_index));

                idx = (choice_count + idx - 1) % choice_count;

                if (!touch_pane_main_confirm_change_allowed(panel, button_index, choices[idx])) {
                    bell("Bind Pick/Confirm to another main-panel button first.");
                    break;
                }

                set_sdl_touch_pane_binding_for_panel(panel, button_index, choices[idx]);
            }
            changed = true;
            break;
        }

        case 'y':
        case '6':
        case ' ':
        case 't':
        case '5':
        {
            if (touch_setting_is_swipe_row(highlight))
            {
                int swipe_dir = touch_setting_swipe_dir_for_row(highlight);

                if (highlight == TOUCH_SETTING_SWIPE_ENABLED)
                {
                    set_sdl_touch_swipe_enabled(!get_sdl_touch_swipe_enabled());
                }
                else
                {
                    int choice_count = 0;
                    const int* choices = touch_swipe_action_choices(&choice_count);
                    int idx = touch_action_choice_index(choices, choice_count,
                        get_sdl_touch_swipe_binding(swipe_dir));

                    idx = (idx + 1) % choice_count;
                    set_sdl_touch_swipe_binding(swipe_dir, choices[idx]);
                }
            }
            else
            {
                int button_index = touch_setting_button_index(highlight);
                int choice_count = 0;
                const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
                int idx = touch_pane_action_choice_index(panel,
                    get_sdl_touch_pane_binding_for_panel(panel, button_index));

                idx = (idx + 1) % choice_count;

                if (!touch_pane_main_confirm_change_allowed(panel, button_index, choices[idx])) {
                    bell("Bind Pick/Confirm to another main-panel button first.");
                    break;
                }

                set_sdl_touch_pane_binding_for_panel(panel, button_index, choices[idx]);
            }
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

            if (touch_setting_is_swipe_row(highlight))
            {
                bell("Swipe labels are fixed.");
                break;
            }

            button_index = touch_setting_button_index(highlight);

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
            if (touch_setting_is_swipe_row(highlight))
            {
                int swipe_dir = touch_setting_swipe_dir_for_row(highlight);

                if (highlight == TOUCH_SETTING_SWIPE_ENABLED)
                    set_sdl_touch_swipe_enabled(get_sdl_touch_swipe_default_enabled());
                else
                    set_sdl_touch_swipe_binding(swipe_dir,
                        get_sdl_touch_swipe_default_binding(swipe_dir));
            }
            else
            {
                int button_index = touch_setting_button_index(highlight);

                if (!touch_pane_main_confirm_change_allowed(panel, button_index,
                        get_sdl_touch_pane_default_binding_for_panel(panel, button_index))) {
                    bell("Bind Pick/Confirm to another main-panel button first.");
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
            sdl_touch_pane_reset_bindings_to_default();
            changed = true;
            break;

        default:
            bell("Illegal command for touch settings!");
            break;
        }
    }

    if (changed)
    {
        if (settings_changed)
            *settings_changed = true;
    }

    screen_load();
}


void do_cmd_controller_settings(void);

static bool legacy_options_choice_is_disabled(int choice)
{
    return (choice == 6);
}

static int legacy_options_menu(int* highlight)
{
    int ch;
    int options = 7;
    int term_wid = 80;
    int term_hgt = 24;
    int title_row = 1;
    int row;
    bool death_view = death_spectator_active();

    Term_get_size(&term_wid, &term_hgt);
    if (term_hgt < 20)
        title_row = 0;

    if (*highlight < 1)
        *highlight = 1;
    else if (*highlight > options)
        *highlight = options;

    if (death_view && legacy_options_choice_is_disabled(*highlight))
        *highlight = options;

    row = title_row + 2;

    Term_putstr(2, title_row, -1, TERM_WHITE, "Legacy Options");

    Term_putstr(2, row++, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        "j) Load a 'Pref' File");
    Term_putstr(2, row++, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        "k) Append Options to a 'Pref' File");
    Term_putstr(2, row++, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        "l) Set Macros");
    Term_putstr(2, row++, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        "m) Set Colours");
    Term_putstr(2, row++, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        "n) Write a note");

    {
        byte suicide_color = death_view ? TERM_L_DARK
            : ((*highlight == 6) ? TERM_L_BLUE : TERM_WHITE);
        Term_putstr(2, row++, -1, suicide_color, "s) Suicide");
    }

    Term_putstr(2, row++, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE,
        "o) Return to Options");

    {
        char verbuf[128];
        strnfmt(verbuf, sizeof(verbuf), "%s %s", VERSION_NAME, VERSION_STRING);
        if (row < term_hgt)
            Term_putstr(2, row, term_wid - 2, TERM_SLATE, verbuf);
    }

    Term_fresh();

    Term_gotoxy(2, title_row + 1 + *highlight);

    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if ((ch == 'j') || (ch == 'J'))
    {
        *highlight = 1;
        return (1);
    }

    if ((ch == 'k') || (ch == 'K'))
    {
        *highlight = 2;
        return (2);
    }

    if ((ch == 'l') || (ch == 'L'))
    {
        *highlight = 3;
        return (3);
    }

    if ((ch == 'm') || (ch == 'M'))
    {
        *highlight = 4;
        return (4);
    }

    if ((ch == 'n') || (ch == 'N'))
    {
        *highlight = 5;
        return (5);
    }

    if ((ch == 's') || (ch == 'S'))
    {
        if (death_view)
        {
            msg_print("You can no longer take that action.");
            return (0);
        }

        *highlight = 6;
        return (6);
    }

    if ((ch == 'o') || (ch == 'O') || (ch == 'q') || (ch == 'Q')
        || (ch == ESCAPE))
    {
        *highlight = 7;
        return (7);
    }

    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (death_view && legacy_options_choice_is_disabled(*highlight))
        {
            msg_print("You can no longer take that action.");
            return (0);
        }

        return (*highlight);
    }

    if (ch == '8')
    {
        *highlight = (*highlight + (options - 2)) % options + 1;
        while (death_view && legacy_options_choice_is_disabled(*highlight))
            *highlight = (*highlight + (options - 2)) % options + 1;
    }

    if (ch == '2')
    {
        *highlight = *highlight % options + 1;
        while (death_view && legacy_options_choice_is_disabled(*highlight))
            *highlight = *highlight % options + 1;
    }

    return (0);
}

static void do_cmd_legacy_options(void)
{
    int choice = 0;
    int highlight = 1;
    bool return_to_options = false;
    char ftmp[80];

    Term_clear();

    while (!return_to_options)
    {
        choice = legacy_options_menu(&highlight);

        switch (choice)
        {
        case 1:
        {
            do_cmd_pref_file_hack(12);
            Term_clear();
            break;
        }
        case 2:
        {
            Term_putstr(2, 14, -1, TERM_SLATE, "(Escape to cancel)");

            prt("File: ", 12, 2);

            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            if (!askfor_aux(ftmp, sizeof(ftmp)))
            {
                Term_clear();
                continue;
            }

            if (option_dump(ftmp))
                msg_print("Failed!");
            else
                msg_print("Done.");

            Term_clear();
            break;
        }
        case 3:
        {
            do_cmd_macros();
            Term_clear();
            break;
        }
        case 4:
        {
            do_cmd_colors();
            Term_clear();
            break;
        }
        case 5:
        {
            do_cmd_note("", p_ptr->depth);
            Term_clear();
            break;
        }
        case 6:
        {
            do_cmd_suicide();
            return_to_options = true;
            Term_clear();
            break;
        }
        case 7:
        {
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
    int options = 10;
    int term_wid = 80;
    int term_hgt = 24;
    int title_row = 1;
    int row;
    char line_buf[80];
    bool steamdeck = steamdeck_controls_active();
    bool allow_debug_menu = false;
#ifdef SHOW_DEBUG_OPTIONS_MENU
    allow_debug_menu = true;
#endif
    if (allow_debug_menu && p_ptr->noscore)
        options++;

    Term_get_size(&term_wid, &term_hgt);
    if (term_hgt < 20)
        title_row = 0;

    if (*highlight < 1)
        *highlight = 1;
    else if (*highlight > options)
        *highlight = options;

    row = title_row + 2;

    Term_putstr(2, title_row, -1, TERM_WHITE, "Options and misc");

    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'a', "Input Options");
    Term_putstr(2, row++, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'b', "Pane Settings");
    Term_putstr(2, row++, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'c', "Interface Options");
    Term_putstr(2, row++, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'd', "Visual Options");
    Term_putstr(2, row++, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'e', "Text Options");
    Term_putstr(2, row++, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'f', "Gameplay Options");
    Term_putstr(2, row++, -1, (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'g', "Sound Options");
    Term_putstr(2, row++, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'h', "Efficiency Options");
    Term_putstr(2, row++, -1, (*highlight == 8) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'i', "Legacy Options");
    Term_putstr(2, row++, -1, (*highlight == 9) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'o', "Return to Game");
    Term_putstr(2, row++, -1, (*highlight == 10) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);

    if (allow_debug_menu && p_ptr->noscore)
    {
        keyed_menu_entry_label(line_buf, sizeof(line_buf), 'p',
            "Debugging Options");
        Term_putstr(2, row++, -1, (*highlight == 11) ? TERM_L_BLUE : TERM_WHITE,
            line_buf);
    }

    /* Show product name and version on the bottom of the menu */
    {
        char verbuf[128];
        strnfmt(verbuf, sizeof(verbuf), "%s %s", VERSION_NAME, VERSION_STRING);
        if (row < term_hgt)
            Term_putstr(2, row, term_wid - 2, TERM_SLATE, verbuf);
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, title_row + 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if (!steamdeck && ((ch == 'a') || (ch == 'A')))
    {
        *highlight = 1;
        return (1);
    }

    if (!steamdeck && ((ch == 'b') || (ch == 'B')))
    {
        *highlight = 2;
        return (2);
    }

    if (!steamdeck && ((ch == 'c') || (ch == 'C')))
    {
        *highlight = 3;
        return (3);
    }

    if (!steamdeck && ((ch == 'd') || (ch == 'D')))
    {
        *highlight = 4;
        return (4);
    }

    if (!steamdeck && ((ch == 'e') || (ch == 'E')))
    {
        *highlight = 5;
        return (5);
    }

    if (!steamdeck && ((ch == 'f') || (ch == 'F')))
    {
        *highlight = 6;
        return (6);
    }

    if (!steamdeck && ((ch == 'g') || (ch == 'G')))
    {
        *highlight = 7;
        return (7);
    }

    if (!steamdeck && ((ch == 'h') || (ch == 'H')))
    {
        *highlight = 8;
        return (8);
    }

    if (!steamdeck && ((ch == 'i') || (ch == 'I')))
    {
        *highlight = 9;
        return (9);
    }

    if ((!steamdeck && ((ch == 'o') || (ch == 'O') || (ch == 'q')))
        || (ch == ESCAPE) || (steamdeck && ch == steamdeck_back_key()))
    {
        *highlight = 10;
        return (10);
    }

    if (!steamdeck && allow_debug_menu && p_ptr->noscore
        && ((ch == 'p') || (ch == 'P')))
    {
        *highlight = 11;
        return (11);
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

static int input_options_menu(int* highlight)
{
    int ch;
    int options = 4;
    int term_wid = 80;
    int term_hgt = 24;
    int title_row = 1;
    int row;
    char line_buf[80];
    bool steamdeck = steamdeck_controls_active();

    Term_get_size(&term_wid, &term_hgt);
    if (term_hgt < 20)
        title_row = 0;

    if (*highlight < 1)
        *highlight = 1;
    else if (*highlight > options)
        *highlight = options;

    row = title_row + 2;

    Term_putstr(2, title_row, -1, TERM_WHITE, "Input Options");

    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'a', "Set Keybinds");
    Term_putstr(2, row++, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'b',
        "Controller Settings");
    Term_putstr(2, row++, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'c', "Touch Settings");
    Term_putstr(2, row++, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);
    keyed_menu_entry_label(line_buf, sizeof(line_buf), 'o',
        "Return to Options");
    Term_putstr(2, row++, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        line_buf);

    Term_fresh();
    Term_gotoxy(2, title_row + 1 + *highlight);

    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if (!steamdeck && ((ch == 'a') || (ch == 'A')))
    {
        *highlight = 1;
        return (1);
    }

    if (!steamdeck && ((ch == 'b') || (ch == 'B')))
    {
        *highlight = 2;
        return (2);
    }

    if (!steamdeck && ((ch == 'c') || (ch == 'C')))
    {
        *highlight = 3;
        return (3);
    }

    if ((!steamdeck && ((ch == 'o') || (ch == 'O') || (ch == 'q')))
        || (ch == ESCAPE) || (steamdeck && ch == steamdeck_back_key()))
    {
        *highlight = 4;
        return (4);
    }

    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        return (*highlight);
    }

    if (ch == '8')
    {
        *highlight = (*highlight + (options - 2)) % options + 1;
    }

    if (ch == '2')
    {
        *highlight = *highlight % options + 1;
    }

    return (0);
}

static void do_cmd_input_options_submenu(int* highlight)
{
    int choice = 0;
    bool return_to_options = false;

    Term_clear();

    while (!return_to_options)
    {
        choice = input_options_menu(highlight);

        switch (choice)
        {
        case 1:
            do_cmd_keybinds();
            Term_clear();
            break;
        case 2:
            do_cmd_controller_settings();
            Term_clear();
            break;
        case 3:
            do_cmd_touch_pane_button_editor(NULL);
            Term_clear();
            break;
        case 4:
            return_to_options = true;
            Term_clear();
            break;
        }
    }
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

    bool return_to_game = false;

    /* Clear any active banner before opening options */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();
    if (p_ptr && p_ptr->playing)
        sdl_music_play_menu_theme();

    /* Clear screen */
    Term_clear();

    /* Process Events until "Return to Game" is selected */
    while (!return_to_game)
    {
        choice = options_menu(&highlight);

        switch (choice)
        {
        case 1:
        {
            do_cmd_input_options_submenu(&input_highlight);
            Term_clear();
            break;
        }
        case 2:
        {
            do_cmd_pane_settings();
            Term_clear();
            break;
        }
        case 3:
        {
            do_cmd_options_aux(INTERFACE_PAGE, "Interface Options");
            Term_clear();
            break;
        }
        case 4:
        {
            do_cmd_options_aux(VISUAL_PAGE, "Visual Options");
            Term_clear();
            break;
        }
        case 5:
        {
            do_cmd_options_aux(TEXT_PAGE, "Text Options");
            Term_clear();
            break;
        }
        case 6:
        {
            do_cmd_options_aux(GAMEPLAY_PAGE, "Gameplay Options");
            Term_clear();
            break;
        }
        case 7:
        {
            do_cmd_options_aux(SOUND_PAGE, "Sound Options");
            Term_clear();
            break;
        }
        case 8:
        {
            do_cmd_options_aux(EFFICIENCY_PAGE, "Efficiency Options");
            Term_clear();
            break;
        }
        case 9:
        {
            do_cmd_legacy_options();
            if (p_ptr && (p_ptr->leaving || !p_ptr->playing))
                return_to_game = true;
            Term_clear();
            break;
        }
        case 10:
        {
            /* Return to Game */
            return_to_game = true;
            Term_clear();
            break;
        }
        case 11:
        {
            /* Debugging Options (only reachable when p_ptr->noscore) */
            do_cmd_options_aux(DEBUG_PAGE, "Debugging Options");
            Term_clear();
            break;
        }
        }
    }

    /* Flush messages */
    message_flush();

    /* Load screen */
    screen_pop_supporting_panes_hidden();
    screen_load();
    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
    if (p_ptr && op_ptr && op_ptr->opt[OPT_top_status_line] && Term)
        Term_erase(0, 0, 255);
    if (p_ptr)
        handle_stuff();
}

#ifdef ALLOW_MACROS
/* Forward declaration */
static errr keymap_dump(cptr fname);
#endif

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

static int count_action_bindings(int mode, const struct keybind_entry* entry)
{
    int key;
    int count = 0;

    if (!entry || !entry->action)
        return 0;

    if (key_provides_action(mode, entry->key_code, entry->action,
            entry->requires_keymap))
        count++;

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
                count++;
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

        count++;
    }

    return count;
}

static void summarize_action_bindings_compact(int mode,
    const struct keybind_entry* entry, char* buf, size_t buflen)
{
    int count;

    if (!buf || !buflen)
        return;

    count = count_action_bindings(mode, entry);

    if (count <= 0)
        SDL_strlcpy(buf, "none", buflen);
    else if (count == 1)
        SDL_strlcpy(buf, "1 key", buflen);
    else
        strnfmt(buf, buflen, "%d keys", count);
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
    int top_primary = 0;
    int top_secondary = 0;
    const char* default_file = "user.prf";
    const int list_start_row = 5;
    int term_w, term_h;
    int visible_rows;
    static const struct keybind_entry primary_keybinds[] = {
        {'1', NULL, "Move SW (numpad 1)", ";1", true},
        {'2', NULL, "Move S (numpad 2)", ";2", true},
        {'3', NULL, "Move SE (numpad 3)", ";3", true},
        {'4', NULL, "Move W (numpad 4)", ";4", true},
        {'6', NULL, "Move E (numpad 6)", ";6", true},
        {'7', NULL, "Move NW (numpad 7)", ";7", true},
        {'8', NULL, "Move N (numpad 8)", ";8", true},
        {'9', NULL, "Move NE (numpad 9)", ";9", true},
        {'z', NULL, "Wait (z / numpad 5)", "z", false},
        {'i', NULL, "Inventory", "i", false},
        {'e', NULL, "Equipment", "e", false},
        {'u', NULL, "Use item", "u", false},
        {'x', NULL, "Examine item", "x", false},
        {'s', NULL, "Sing / change song", "s", false},
        {'S', NULL, "Toggle stealth", "S", false},
        {'h', "H@", "Character sheet (h / H / @)", "h", false},
        {'f', NULL, "Fire (primary quiver)", "f", false},
        {'F', NULL, "Fire (secondary quiver)", "F", false},
        {'l', NULL, "Look around", "l", false},
        {'T', NULL, "Tunnel / dig", "T", false},
        {'b', NULL, "Bash door", "b", false},
    };
    
    static const struct keybind_entry secondary_keybinds[] = {
        {'j', NULL, "Supplies overview", "j", false},
        {'.', NULL, "Run (also shift)", ".", false},
        {'/', NULL, "Alt action (also ctrl)", "/", false},
        {'w', NULL, "Wear / wield equipment", "w", false},
        {'r', NULL, "Remove equipment", "r", false},
        {'d', NULL, "Drop item", "d", false},
        {'k', NULL, "Destroy item", "k", false},
        {'g', NULL, "Pick up items", "g", false},
        {'Z', NULL, "Rest", "Z", false},
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
    
    Term_get_size(&term_w, &term_h);
    visible_rows = term_h - list_start_row - 6;
    if (visible_rows < 5)
        visible_rows = 5;
    
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
        int* top_ptr;
        int highlight;
        int display_end;
        int row;
        int i;
        bool compact_width;
        bool detail_mode;
        char binding_buf[80];
        char detail_binding_buf[512];
        char detail_buf[560];
        char line_buf[128];
        int row_width;
        int detail_rows;
        int bottom_reserved;
        int detail_row;
        int info_row;

        Term_get_size(&term_w, &term_h);
        compact_width = (term_w < 70);
        detail_mode = compact_width;
        detail_rows = detail_mode ? 3 : 0;
        bottom_reserved = detail_rows + 3;
        visible_rows = term_h - list_start_row - bottom_reserved;
        if (visible_rows < 5)
            visible_rows = 5;
        row_width = settings_ui_line_width(2);
        
        if (showing_primary)
        {
            keybinds = primary_keybinds;
            num_keybinds = primary_count;
            highlight_ptr = &highlight_primary;
            top_ptr = &top_primary;
        }
        else
        {
            keybinds = secondary_keybinds;
            num_keybinds = secondary_count;
            highlight_ptr = &highlight_secondary;
            top_ptr = &top_secondary;
        }
        
        if (*highlight_ptr >= num_keybinds)
            *highlight_ptr = num_keybinds - 1;
        if (*highlight_ptr < 0)
            *highlight_ptr = 0;
        
        if (*top_ptr > *highlight_ptr)
            *top_ptr = *highlight_ptr;
        if (*top_ptr + visible_rows <= *highlight_ptr)
            *top_ptr = *highlight_ptr - visible_rows + 1;
        if (*top_ptr < 0)
            *top_ptr = 0;
        if (num_keybinds > visible_rows)
        {
            int max_top = num_keybinds - visible_rows;
            if (*top_ptr > max_top)
                *top_ptr = max_top;
        }
        else
        {
            *top_ptr = 0;
        }
        
        highlight = *highlight_ptr;
        
        /* Clear screen */
        Term_clear();

        /* Title */
        settings_ui_put_fitted(1, 0, TERM_WHITE, "Keybind Configuration");
        if (compact_width)
        {
            settings_ui_put_fitted(2, 0, TERM_WHITE,
                "8/2 move  Enter bind  Tab switch  Esc return");
            settings_ui_put_fitted(3, 0, TERM_WHITE,
                showing_primary ? "Primary commands" : "Supplementary commands");
        }
        else
        {
            settings_ui_put_fitted(2, 0, TERM_WHITE,
                "Arrow to navigate, Enter to bind, Tab to switch groups, Escape to return");
            settings_ui_put_fitted(3, 0, TERM_WHITE,
                showing_primary ? "Primary Commands: Essential for the gameplay"
                                : "Supplementary Commands");
        }
        
        /* List visible keybinds */
        display_end = *top_ptr + visible_rows;
        if (display_end > num_keybinds)
            display_end = num_keybinds;
        for (i = *top_ptr; i < display_end; i++)
        {
            int entry_row = list_start_row + (i - *top_ptr);
            if (detail_mode)
                summarize_action_bindings_compact(mode, &keybinds[i],
                    binding_buf, sizeof(binding_buf));
            else
                describe_action_bindings(mode, &keybinds[i], binding_buf,
                    sizeof(binding_buf));
            settings_ui_format_pair_line(line_buf, sizeof(line_buf),
                keybinds[i].key_name, binding_buf, row_width, 12);

            /* Display the keybind */
            if (i == highlight)
            {
                /* Highlighted */
                c_prt(TERM_L_BLUE, line_buf, entry_row, 2);
            }
            else
            {
                /* Normal */
                prt(line_buf, entry_row, 2);
            }
        }
        
        /* Clear any leftover rows */
        for (i = display_end; i < *top_ptr + visible_rows; i++)
        {
            row = list_start_row + (i - *top_ptr);
            Term_erase(2, row, term_w > 2 ? term_w - 2 : 0);
        }

        detail_row = list_start_row + visible_rows;
        info_row = detail_row + detail_rows;
        if (detail_mode)
        {
            describe_action_bindings(mode, &keybinds[highlight],
                detail_binding_buf, sizeof(detail_binding_buf));
            strnfmt(detail_buf, sizeof(detail_buf), "Bindings: %s",
                detail_binding_buf);
            settings_ui_draw_wrapped_block(detail_row, 2, row_width,
                detail_rows, TERM_SLATE, detail_buf);
        }
        
        /* Instructions at bottom */
        if (compact_width)
        {
            settings_ui_put_fitted(info_row, 2, TERM_WHITE,
                "s: save keybinds");
            settings_ui_put_fitted(info_row + 1, 2, TERM_WHITE,
                "r: reset selected");
        }
        else
        {
            strnfmt(line_buf, sizeof(line_buf), "Press 's' to save keybinds to %s",
                default_file);
            settings_ui_put_fitted(info_row, 2, TERM_WHITE,
                line_buf);
            settings_ui_put_fitted(info_row + 1, 2, TERM_WHITE,
                "Press 'r' to reset selected keybind to default");
        }
        if (dirty)
            c_prt(TERM_YELLOW, "Unsaved changes", info_row + 2, 2);
        else
            Term_erase(2, info_row + 2,
                term_w > 2 ? term_w - 2 : 0);
        
        /* Get input */
        ch = inkey();
        
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
            char prompt[80];
            char prompt_long[96];
            char prompt_short[80];
            int entry_row = list_start_row + (highlight - *top_ptr);

            /* Clear the action area */
            Term_erase(2, entry_row, 255);
            
            /* Prompt for new binding */
            strnfmt(prompt_long, sizeof(prompt_long),
                "Press key to use for %s (Escape to cancel):",
                keybinds[highlight].key_name);
            strnfmt(prompt_short, sizeof(prompt_short),
                "Bind %s (Esc cancels):", keybinds[highlight].key_name);
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(row_width, prompt_long, prompt_short,
                    prompt_short));
            settings_ui_put_fitted(entry_row, 2, TERM_YELLOW, prompt);
            Term_fresh();
            
            /* Get the key to bind */
            flush();
            char bind_key = inkey();
            
            if (bind_key != ESCAPE && bind_key != 0)
            {
                byte new_key = (byte)bind_key;
                
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
#ifdef ALLOW_MACROS
            /* Save keybinds to file */
            char ftmp[80];
            
            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s", default_file);
            
            /* Clear prompt area */
            Term_erase(2, info_row, term_w > 2 ? term_w - 2 : 0);
            prt("File: ", info_row, 2);
            
            /* Ask for a file */
            if (askfor_aux(ftmp, sizeof(ftmp)))
            {
                /* Dump the keymaps */
                if (keymap_dump(ftmp) == 0)
                {
                    msg_format("Keybinds saved to %s.", ftmp);
                    dirty = false;
                }
                else
                {
                    msg_print("Failed to save keybinds.");
                }
                message_flush();
            }
#else
            msg_print("Saving keybinds is not available in this build.");
            message_flush();
#endif
        }
        
        /* Store updated highlight for the active group */
        *highlight_ptr = highlight;
    }
    
    /* Load screen */
    screen_load();

    if (dirty)
    {
        char prompt[80];
        strnfmt(prompt, sizeof(prompt), "Save keybinds to %s? ", default_file);
        if (get_check(prompt))
        {
            if (keymap_dump(default_file) == 0)
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

static int controller_action_total_binding_count(int binding)
{
    return controller_action_binding_count(binding, NULL, NULL)
        + controller_combo_action_binding_count(binding, NULL, NULL, NULL, NULL);
}

static void controller_action_binding_summary(int binding, char* buf,
    size_t buflen)
{
    int count;

    if (!buf || !buflen)
        return;

    count = controller_action_total_binding_count(binding);

    if (count <= 0)
        SDL_strlcpy(buf, "none", buflen);
    else if (count == 1)
        SDL_strlcpy(buf, "1 bind", buflen);
    else
        strnfmt(buf, buflen, "%d binds", count);
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

static void controller_prompt_label_no_sticks(int binding, const char* fallback,
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

static void controller_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
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
    int top = 0;
    int term_w, term_h;
    const int list_start_row = 5;

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
        { CONTROLLER_ENTRY_ACTION, '\t', "Abilities (Tab)" },
        { CONTROLLER_ENTRY_ACTION, 'i', "Inventory" },
        { CONTROLLER_ENTRY_ACTION, 'e', "Equipment" },
        { CONTROLLER_ENTRY_ACTION, 'u', "Use item" },
        { CONTROLLER_ENTRY_ACTION, 'x', "Examine item" },
        { CONTROLLER_ENTRY_ACTION, 's', "Sing / change song" },
        { CONTROLLER_ENTRY_ACTION, 'S', "Toggle stealth" },
        { CONTROLLER_ENTRY_ACTION, 'h', "Character sheet" },
        { CONTROLLER_ENTRY_ACTION, 'f', "Fire (primary)" },
        { CONTROLLER_ENTRY_ACTION, 'F', "Fire (secondary)" },
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
        { CONTROLLER_ENTRY_ACTION, 'd', "Drop item" },
        { CONTROLLER_ENTRY_ACTION, 'k', "Destroy item" },
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
        char detail_buf[560];
        char line_buf[128];
        int row;
        bool steamdeck = steamdeck_controls_active();
        bool compact_width;
        bool detail_mode;
        int row_width;
        int detail_rows;
        int bottom_reserved;
        int detail_row;
        int info_row;

        Term_get_size(&term_w, &term_h);
        compact_width = (term_w < 70);
        detail_mode = compact_width;
        detail_rows = detail_mode ? 4 : 0;
        bottom_reserved = detail_rows + 2;
        row_width = settings_ui_line_width(2);
        int visible_rows = term_h - list_start_row - bottom_reserved;
        if (visible_rows < 5)
            visible_rows = 5;

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
        if (entry_count > visible_rows) {
            int max_top = entry_count - visible_rows;
            if (top > max_top)
                top = max_top;
        } else {
            top = 0;
        }

        Term_clear();
        settings_ui_put_fitted(1, 0, TERM_WHITE, "Controller Settings");
        if (steamdeck) {
            char confirm_label[16];
            char back_label[16];
            char prompt_buf[80];
            /* Steam Deck UI: A=bind, B=back */
            controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
            controller_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "D-pad %s add  %s back"
                              : "D-pad navigate  %s add  %s back",
                confirm_label, back_label);
            settings_ui_put_fitted(2, 0, TERM_WHITE, prompt_buf);
        } else {
            settings_ui_put_fitted(2, 0, TERM_WHITE,
                compact_width ? "8/2 move  Enter add  Esc return"
                              : "Arrow to navigate, Enter to add, Escape to return");
        }

        for (int i = top; i < entry_count && i < top + visible_rows; i++) {
            int entry_row = list_start_row + (i - top);
            if (detail_mode && entries[i].type == CONTROLLER_ENTRY_ACTION)
                controller_action_binding_summary(entries[i].id, value_buf,
                    sizeof(value_buf));
            else
                controller_entry_value(&entries[i], value_buf,
                    sizeof(value_buf));
            settings_ui_format_pair_line(line_buf, sizeof(line_buf), entries[i].label,
                value_buf, row_width, 12);

            if (i == highlight) {
                c_prt(TERM_L_BLUE, line_buf, entry_row, 2);
            } else {
                prt(line_buf, entry_row, 2);
            }
        }

        for (row = list_start_row + (entry_count - top); row < list_start_row + visible_rows; row++) {
            Term_erase(2, row, term_w > 2 ? term_w - 2 : 0);
        }

        detail_row = list_start_row + visible_rows;
        info_row = detail_row + detail_rows;
        if (detail_mode) {
            if (entries[highlight].type == CONTROLLER_ENTRY_ACTION) {
                controller_describe_action_bindings_compact(entries[highlight].id,
                    detail_value_buf, sizeof(detail_value_buf));
                strnfmt(detail_buf, sizeof(detail_buf), "Bindings: %s",
                    detail_value_buf);
            } else {
                strnfmt(detail_buf, sizeof(detail_buf),
                    "Press Enter to toggle %s.", entries[highlight].label);
            }
            settings_ui_draw_wrapped_block(detail_row, 2, row_width, detail_rows,
                TERM_SLATE, detail_buf);
        }

        if (steamdeck) {
            char reset_label[16];
            char reset_all_label[16];
            char prompt_buf[80];
            /* Steam Deck UI: X=reset selected, Y=reset all */
            controller_prompt_label(steamdeck_alt_action_key(), "X", reset_label, sizeof(reset_label));
            controller_prompt_label(steamdeck_secondary_key(), "Y", reset_all_label, sizeof(reset_all_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "[%s] reset  [%s] reset all"
                              : "Reset: [%s] selected, [%s] all",
                reset_label, reset_all_label);
            settings_ui_put_fitted(info_row, 2, TERM_WHITE,
                prompt_buf);
        } else {
            settings_ui_put_fitted(info_row, 2, TERM_WHITE,
                compact_width ? "r: reset selected  R: reset all"
                              : "Press 'r' to reset selected binding, 'R' to reset all bindings");
        }
        settings_ui_put_fitted(info_row + 1, 2, TERM_WHITE,
            compact_width ? "Saves on exit." : "Changes are saved on exit.");

        char ch = inkey();

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
            }
        } else if (ch == 'R' || (steamdeck && ch == steamdeck_secondary_key())) {
            sdl_gamepad_reset_bindings_to_default();
            msg_print("All bindings reset to defaults.");
            message_flush();
        } else if (ch == '\r' || ch == '\n' || ch == ' ') {
            const controller_entry* entry = &entries[highlight];
            int entry_row = list_start_row + (highlight - top);

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
                Term_erase(2, entry_row, 255);
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
                settings_ui_put_fitted(entry_row, 2, TERM_YELLOW, prompt);
                Term_fresh();

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
 * Note the complex use of the "inkey()" function from "util.c".
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since "util.c" is much simpler now.  XXX XXX XXX
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
 * no longer be true, since "util.c" is much simpler now.  XXX XXX XXX
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
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Process requests until done */
    while (1)
    {
        int term_wid = 80;
        int term_hgt = 24;
        int title_row = 1;
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

        /* Clear screen */
        Term_clear();

        /* Describe */
        prt("Interact with Macros", title_row, 0);

        /* Describe that action */
        prt("Current action:", action_label_row, 0);

        /* Analyze the current action */
        ascii_to_text(tmp, sizeof(tmp), macro_buffer);

        /* Display the current action */
        Term_putstr(0, action_row, term_wid, TERM_WHITE, tmp);

        /* Selections */
        prt("(1) Load a user pref file", menu_row, 5);
#ifdef ALLOW_MACROS
        prt("(2) Append macros to a file", menu_row + 1, 5);
        prt("(3) Query a macro", menu_row + 2, 5);
        prt("(4) Create a macro", menu_row + 3, 5);
        prt("(5) Remove a macro", menu_row + 4, 5);
        prt("(6) Append keymaps to a file", menu_row + 5, 5);
        prt("(7) Query a keymap", menu_row + 6, 5);
        prt("(8) Create a keymap", menu_row + 7, 5);
        prt("(9) Remove a keymap", menu_row + 8, 5);
        prt("(0) Enter a new action", menu_row + 9, 5);
#endif /* ALLOW_MACROS */

        /* Prompt */
        prt("Command: ", command_row, 0);

        /* Get a command */
        ch = inkey();

        /* Leave */
        if (ch == ESCAPE)
            break;

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(command_row);
        }

#ifdef ALLOW_MACROS

        /* Save macros */
        else if (ch == '2')
        {
            char ftmp[80];

            /* Prompt */
            prt("Command: Append macros to a file", command_row, 0);

            /* Prompt */
            prt("File: ", input_row, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

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

            /* Prompt */
            prt("Command: Append keymaps to a file", command_row, 0);

            /* Prompt */
            prt("File: ", input_row, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

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
    screen_load();
}

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
        msg = format("1. Choose base color (use arrows) " COLOR_SAMPLE
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
        msg = format("2. Choose shade (use arrows) " COLOR_SAMPLE
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
        /* Clear screen */
        Term_clear();

        /* Ask for a choice */
        prt("Interact with Visuals", 2, 0);

        /* Give some choices */
        prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_VISUALS
        prt("(2) Dump monster attr/chars", 5, 5);
        prt("(3) Dump object attr/chars", 6, 5);
        prt("(4) Dump feature attr/chars", 7, 5);
        prt("(5) Dump flavor attr/chars", 8, 5);
        prt("(6) Change monster attr/chars", 9, 5);
        prt("(7) Change object attr/chars", 10, 5);
        prt("(8) Change feature attr/chars", 11, 5);
        prt("(9) Change flavor attr/chars", 12, 5);
#endif
        prt("(0) Reset visuals", 13, 5);

        /* Prompt */
        prt("Command: ", 15, 0);

        /* Prompt */
        ch = inkey();

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
                msg_print("The attr/char editor requires a larger window than compact mode.");
                message_flush();
                continue;
            }
        }

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(15);
        }

#ifdef ALLOW_VISUALS

        /* Dump monster attr/chars */
        else if (ch == '2')
        {
            static cptr mark = "Monster attr/chars";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump monster attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

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

            /* Prompt */
            prt("Command: Dump object attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

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

            /* Prompt */
            prt("Command: Dump feature attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

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

            /* Prompt */
            prt("Command: Dump flavor attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

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
    screen_load();
}

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
    c_put_str(TERM_WHITE, "Arrows: Move to color", y + 3, x);
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

/*
 * Interact with "colors"
 */
void do_cmd_colors(void)
{
    int ch;

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening colors menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Interact until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Ask for a choice */
        prt("Interact with Colors", 2, 0);

        /* Give some choices */
        prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_COLORS
        prt("(2) Dump colors", 5, 5);
        prt("(3) Modify colors", 6, 5);
#endif /* ALLOW_COLORS */

        /* Prompt */
        prt("Command: ", 8, 0);

        /* Prompt */
        ch = inkey();

        /* Done */
        if (ch == ESCAPE)
            break;

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(8);

            /* Could skip the following if loading cancelled XXX XXX XXX */

            /* Mega-Hack -- React to color changes */
            Term_xtra(TERM_XTRA_REACT, 0);

            /* Mega-Hack -- Redraw physical windows */
            Term_redraw();
        }

#ifdef ALLOW_COLORS

        /* Dump colors */
        else if (ch == '2')
        {
            static cptr mark = "Colors";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump colors", 8, 0);

            /* Prompt */
            prt("File: ", 10, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_colors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old colors */
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
            SDL_IOprintf(fff, "# Color redefinitions\n\n");

            /* Dump colors */
            for (i = 0; i < 256; i++)
            {
                int kv = angband_color_table[i][0];
                int rv = angband_color_table[i][1];
                int gv = angband_color_table[i][2];
                int bv = angband_color_table[i][3];

                cptr name = "unknown";

                /* Skip non-entries */
                if (!kv && !rv && !gv && !bv)
                    continue;

                /* Extract the color name */
                if (i < 16)
                    name = color_names[i];

                /* Dump a comment */
                SDL_IOprintf(fff, "# Color '%s'\n", name);

                /* Dump the monster attr/char info */
                SDL_IOprintf(fff, "V:%d:0x%02X:0x%02X:0x%02X:0x%02X\n\n", i, kv, rv,
                    gv, bv);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped color redefinitions.");
        }

        /* Edit colors */
        else if (ch == '3')
        {
            modify_colors();
        }

#endif /* ALLOW_COLORS */

        /* Unknown option */
        else
        {
            bell("Illegal command for colors!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();
}

/*
 * Take notes.  There are two ways this can happen, either in the message recall
 * or a file.  The command can also be passed a string, which will automatically
 * be written. -CK-
 */
void do_cmd_note(char* note, int what_depth)
{
    char buf[120];
    char turn_string[16];

    int length, length_info;
    char info_note[40];
    char depths[10];

    /* Default */
    SDL_strlcpy(buf, "", sizeof(buf));

    /* If a note is passed, use that, otherwise accept user input. */
    if (streq(note, ""))
    {
        if (!term_get_string("Note: ", buf, 57))
            return;
    }
    else
    {
        SDL_strlcpy(buf, note, sizeof(buf));
    }

    /* Ignore empty notes */
    if (!buf[0] || (buf[0] == ' '))
        return;

    /* write it to the notes file */

    /*Artefacts use depth artefact created.  All others use player depth.*/

    /*get depth for recording\
     */
    if (what_depth == 0)
    {
        SDL_strlcpy(depths, "   Gates", sizeof(depths));
    }
    else if (what_depth == CHEST_LEVEL)
    {
        SDL_strlcpy(depths, "   Chest", sizeof(depths));
    }
    else if (what_depth == SKELETON_LEVEL)
    {
        SDL_strlcpy(depths, "   Skeleton", sizeof(depths));
    }
    else
    {
        comma_number(depths, what_depth * 50);
        strnfmt(depths, sizeof(depths), "%5s ft", depths);
    }

    comma_number(turn_string, playerturn);

    /* Make preliminary part of note */
    strnfmt(info_note, sizeof(info_note), "%7s  %s   ", turn_string, depths);

    /*write the info note*/
    SDL_strlcat(notes_buffer, info_note, sizeof(notes_buffer));

    /*get the length of the notes*/
    length_info = strlen(info_note);
    length = strlen(buf);

    /*break up long notes*/
    if ((length + length_info) > LINEWRAP)
    {
        bool keep_going = true;
        int startpoint = 0;
        int endpoint, n;

        while (keep_going)
        {
            /*don't print more than the set linewrap amount*/
            endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;

            /*find a breaking point*/
            while (true)
            {
                /*are we at the end of the line?*/
                if (endpoint >= length)
                {
                    /*print to the end*/
                    endpoint = length;
                    keep_going = false;
                    break;
                }

                /* Mark the most recent space or dash in the string */
                else if ((buf[endpoint] == ' ') || (buf[endpoint] == '-'))
                    break;

                /*no spaces in the line, so break in the middle of text*/
                else if (endpoint == startpoint)
                {
                    endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;
                    break;
                }

                /* check previous char */
                endpoint--;
            }

            /*make a continued note if applicable*/
            if (startpoint)
                SDL_strlcat(
                    notes_buffer, "                    ", sizeof(notes_buffer));

            /* Write that line to file */
            for (n = startpoint; n <= endpoint; n++)
            {
                char ch;

                /* Ensure the character is printable */
                ch = (isprint(buf[n]) ? buf[n] : ' ');

                /* Write out the character */
                SDL_strlcat(notes_buffer, format("%c", ch), sizeof(notes_buffer));
            }

            /*break the line*/
            SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

            /*prepare for the next line*/
            startpoint = endpoint + 1;
        }
    }

    /* Add note to buffer */
    else
    {
        SDL_strlcat(notes_buffer, format("%s\n", buf), sizeof(notes_buffer));
    }
}

/*
 * Mention the current version
 */
void do_cmd_version(void)
{
    /* Silly message - use msg_print so message is shown immediately */
    char verbuf[128];
    strnfmt(verbuf, sizeof(verbuf), "You are playing %s %s.  Type '?' for more info.",
        VERSION_NAME, VERSION_STRING);
    msg_print(verbuf);
}

/*
 * Array of feeling strings
 */
static cptr do_cmd_feeling_text[LEV_THEME_HEAD]
    = { "Looks like any other level.",
          "You feel there is something special about this level.",
          "You have a superb feeling about this level.",
          "You have an excellent feeling...", "You have a very good feeling...",
          "You have a good feeling...", "You feel strangely lucky...",
          "You feel your luck is turning...",
          "You like the look of this place...",
          "This level can't be all bad...", "What a boring place..." };

/*
 * Note that "feeling" is set to zero unless some time has passed.
 * Note that this is done when the level is GENERATED, not entered.
 */
void do_cmd_feeling(void)
{
    /* No useful feeling on the surface */
    if (!p_ptr->depth)
    {
        msg_print("You stand once again upon the surface. Freedom awaits.");
        return;
    }

    /* No useful feelings until enough time has passed */
    if (!do_feeling)
    {
        msg_print("You are still uncertain about this level...");
        return;
    }

    /* Display the feeling */
    else
        msg_print(do_cmd_feeling_text[feeling]);
}

/*
 * Array of feeling strings
 */
static cptr do_cmd_challenge_text[14]
    = { "challenges you from beyond the grave!",
          "thunders 'Prove worthy of your traditions - or die ashamed!'.",
          "desires to test your mettle!",
          "has risen from the dead to test you!",
          "roars 'Fight, or know yourself for a coward!'.",
          "summons you to a duel of life and death!",
          "desires you to know that you face a mighty champion of yore!",
          "demands that you prove your worthiness in combat!",
          "calls you unworthy of your ancestors!",
          "challenges you to a deathmatch!", "walks Middle-Earth once more!",
          "challenges you to demonstrate your prowess!",
          "demands you prove yourself here and now!",
          "asks 'Can ye face the best of those who came before?'." };

/*
 * Personalize, randomize, and announce the challenge of a player ghost. -LM-
 */
void ghost_challenge(void)
{
    monster_race* r_ptr = &r_info[r_ghost];

    /*paranoia*/
    /* Check there is a name/ghost first */
    if (ghost_name[0] == '\0')
    {
        /*there wasn't a ghost*/
        bones_selector = 0;
        return;
    }

    msg_format("%^s, the %^s %s", ghost_name, r_name + r_ptr->name,
        do_cmd_challenge_text[rand_int(14)]);

    message_flush();
}

/*display the notes file*/
void do_cmd_knowledge_notes(void) { show_buffer(notes_buffer, 0); }

/*
 * Display oath status information
 */
void do_cmd_knowledge_oaths(void)
{
    SDL_IOStream* fff;
    char file_name[1024];
    
    /* Temporary file */
    if (!path_temp(file_name, sizeof(file_name)))
        return;

    /* Open a new file */
    fff = sdl_fopen(file_name, "w");

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Scan the oaths */
    SDL_IOprintf(fff, "Oath Status\n\n");
    
    /* Check current character oath */
    if (p_ptr->have_ability[S_SPC][SPC_OATH_MERCY])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_MERCY])
            SDL_IOprintf(fff, "Current Oath: Oath of Mercy (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Mercy (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SILENCE])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SILENCE])
            SDL_IOprintf(fff, "Current Oath: Oath of Silence (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Silence (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_IRON])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_IRON])
            SDL_IOprintf(fff, "Current Oath: Oath of Iron (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Iron (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SMITH])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SMITH])
            SDL_IOprintf(fff, "Current Oath: Oath of the Smith (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of the Smith (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_VALOROUS])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_VALOROUS])
            SDL_IOprintf(fff, "Current Oath: Oath of Valorous Heart (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Valorous Heart (Broken)\n\n");
    }
    else
    {
        SDL_IOprintf(fff, "Current Oath: None\n\n");
    }
    
    /* Display metarun oath status */
    SDL_IOprintf(fff, "Metarun Oath Status:\n");
    
    /* Check unlocked oaths */
    bool has_unlocked = false;
    if (oath_unlocked(OATH_MERCY)) 
    {
        SDL_IOprintf(fff, "  Oath of Mercy: Unlocked");
        if (oath_banned(OATH_MERCY))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_SILENCE)) 
    {
        SDL_IOprintf(fff, "  Oath of Silence: Unlocked");
        if (oath_banned(OATH_SILENCE))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_IRON)) 
    {
        SDL_IOprintf(fff, "  Oath of Iron: Unlocked");
        if (oath_banned(OATH_IRON))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_SMITH)) 
    {
        SDL_IOprintf(fff, "  Oath of the Smith: Unlocked");
        if (oath_banned(OATH_SMITH))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_VALOROUS)) 
    {
        SDL_IOprintf(fff, "  Oath of Valorous Heart: Unlocked");
        if (oath_banned(OATH_VALOROUS))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (!has_unlocked)
    {
        SDL_IOprintf(fff, "  No oaths unlocked yet.\n");
        SDL_IOprintf(fff, "  Complete Valar quests to unlock new oaths.\n");
    }
    
    /* Close the file */
    sdl_fclose(fff);

    /* Display the file contents */
    show_file(file_name, "Oath Status", 0);

    /* Remove the file */
    fd_kill(file_name);
}

/*
 * Description of each object group.
 */
static cptr object_group_text[]
    = { "Herbs", "Potions", "Rings", "Amulets", "Staves", "Horns", "Swords",
          "Axes & Polearms", "Blunt Weapons", "Diggers", "Bows",
          //	"Arrows",
          "Light Sources", "Soft Armour", "Mail", "Shields", "Cloaks", "Gloves",
          "Helms", "Crowns", "Boots", "Chests", NULL };

/*
 * TVALs of items in each group
 */
static byte object_group_tval[] = { TV_FOOD, TV_POTION, TV_RING, TV_AMULET,
    TV_STAFF, TV_HORN, TV_SWORD, TV_POLEARM, TV_HAFTED, TV_DIGGING, TV_BOW,
    //	TV_ARROW,
    TV_LIGHT, TV_SOFT_ARMOR, TV_MAIL, TV_SHIELD, TV_CLOAK, TV_GLOVES, TV_HELM,
    TV_CROWN, TV_BOOTS, TV_CHEST, 0 };

/*
 * Build a list of objects indexes in the given group. Return the number
 * of objects in the group. object_idx[] must be one element larger than the
 * largest number of objects that will be collected.
 *  (Incorporates some code from jdh)
 */
static int collect_objects(int grp_cur, object_list_entry object_idx[])
{
    int i, j, k, object_cnt = 0;
    int max_sval = -1;

    /* Get a list of x_char in this group */
    byte group_tval = object_group_tval[grp_cur];

    /* Check every object */
    for (i = 0; i < z_info->k_max; i++)
    {
        /* Access the object type */
        object_kind* k_ptr = &k_info[i];

        /*used to check for allocation*/
        k = 0;

        /* Skip empty objects */
        if (!k_ptr->name)
            continue;

        /* Skip items with no distribution (including special artefacts) */
        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /*add the rarity, if there is one*/
            k += k_ptr->chance[j];
        }
        /*not in allocation table*/
        if (!(k))
            continue;

        /* Require objects ever seen*/
        // if (!(k_ptr->aware && k_ptr->everseen)) continue;
        if (!(k_ptr->everseen))
            continue;

        /* Check for object in the group */
        if (k_ptr->tval == group_tval)
        {
            /* Save the highest sval in the group for later */
            if (k_ptr->sval > max_sval)
            {
                max_sval = k_ptr->sval;
            }

            /* Add the object type */
            if (object_idx)
            {
                object_idx[object_cnt].type = OBJ_NORMAL;
                object_idx[object_cnt].idx = i;
            }

            object_cnt++;
        }
    }

    /* Add special items to the list */
    /* Skip this part if we don't know any normal items */
    for (i = 0; object_cnt > 0 && i < z_info->e_max; i++)
    {
        /* Access the object type */
        ego_item_type* e_ptr = &e_info[i];

        /* Skip empty objects */
        if (!e_ptr->name)
            continue;

        /* Require objects ever seen*/
        if (!(e_ptr->everseen))
            continue;

        /* Check for object in the group */
        for (j = 0; j < EGO_TVALS_MAX; j++)
        {
            if (e_ptr->tval[j] == group_tval)
            {
                if (object_idx)
                {
                    object_idx[object_cnt].type = OBJ_SPECIAL;
                    object_idx[object_cnt].idx = -1;
                    object_idx[object_cnt].e_idx = i;
                    object_idx[object_cnt].tval = group_tval;
                    object_idx[object_cnt].sval = -1;
                }
                object_cnt++;

                break;
            }
        }
    }

    /* Terminate the list */
    if (object_idx)
        object_idx[object_cnt].type = OBJ_NONE;

    /* Return the number of object types */
    return object_cnt;
}

/*
 * Build a list of artefact indexes in the given group. Return the number
 * of eligible artefacts in that group.
 */
static int collect_artefacts(int grp_cur, int object_idx[])
{
    int i, object_cnt = 0;
    bool* okay;
    bool know_all = cheat_know;

    /* Get a list of x_char in this group */
    byte group_tval = object_group_tval[grp_cur];

    /*make a list of artefacts not found*/
    /* Allocate the "object_idx" array */
    okay = mem_alloc_array(z_info->art_max, bool);

    /* Default first,  */
    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        bool revealed = (a_ptr->seen & ART_SEEN_REVEALED) != 0;

        /*start with false*/
        okay[i] = false;

        /* Skip "empty" artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Skip "unfound" artefacts, unless in wizard mode, cheating,
         * or revealed via quests/lore. */
        if (!know_all && !p_ptr->wizard && !a_ptr->found_num && !revealed)
            continue;

        /* Skip "ungenerated" artefacts, unless cheating or quest-revealed. */
        if (!know_all && !revealed && !a_ptr->cur_num)
            continue;

        /* Skip the later versions of the Iron Crown */
        if ((i == ART_MORGOTH_0) || (i == ART_MORGOTH_1)
            || (i == ART_MORGOTH_2))
            continue;

        /* Skip the special smithing template artefacts */
        if ((i >= ART_ULTIMATE) && (i <= z_info->art_norm_max))
            continue;

        /*assume all created artefacts are good at this point*/
        okay[i] = true;
    }

    /* Finally, go through the list of artefacts and categorize the good ones */
    for (i = 0; i < z_info->art_max; i++)
    {
        /* Access the artefact */
        artefact_type* a_ptr = &a_info[i];

        /* Skip empty artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Require artefacts ever seen*/
        if (okay[i] == false)
            continue;

        /* Check for race in the group */
        if (a_ptr->tval == group_tval)
        {
            /* Add the race */
            object_idx[object_cnt++] = i;
        }
    }

    /* Terminate the list */
    object_idx[object_cnt] = 0;

    /*clear the array*/
    mem_free_null(okay);

    /* Return the number of races */
    return object_cnt;
}

static bool supply_kind_matches(int group, int tval, int sval)
{
    return supplies_group_matches_kind(group, tval, sval);
}

static bool supply_item_matches(int group, const object_type* o_ptr)
{
    return supplies_group_matches_object(group, o_ptr);
}

static int supply_group_uniform_weight(int group_idx)
{
    int weight = -1;

    for (int i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        if (!k_ptr->name)
            continue;
        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (weight < 0)
            weight = k_ptr->weight;
        else if (weight != k_ptr->weight)
            return -1;
    }

    return weight;
}

static void describe_supply_group_status(int group_idx, char* buf, size_t len)
{
    int weight;

    if (!buf || len == 0)
        return;

    buf[0] = '\0';

    switch (group_idx)
    {
    case SUPPLY_GROUP_HERBS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
            strnfmt(buf, len, "All herbs weigh %d.%1d lb each.",
                weight / 10, weight % 10);
        break;
    case SUPPLY_GROUP_FOOD:
        SDL_strlcpy(buf, "Food weight varies; each row shows per-item weight.",
            len);
        break;
    case SUPPLY_GROUP_POTIONS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
            strnfmt(buf, len, "All potions weigh %d.%1d lb each.",
                weight / 10, weight % 10);
        break;
    case SUPPLY_GROUP_GEMS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
            strnfmt(buf, len, "All gems weigh %d.%1d lb each.",
                weight / 10, weight % 10);
        break;
    case SUPPLY_GROUP_LIGHTS:
        SDL_strlcpy(buf,
            "Oil slots: lamp 2, flask 1 (max 4).",
            len);
        break;
    default:
        break;
    }
}

static void build_supply_weight_summary(char* buf, size_t buflen, int term_wid,
    int used_weight, int max_weight, int light_weight, int light_item_weight,
    int light_oil_weight, int lamp_oil, int lamp_capacity, int oil_slots,
    int oil_slot_capacity)
{
    char temp[128];

    if (!buf || buflen == 0)
        return;

    if (term_wid < 1)
        term_wid = 80;

    strnfmt(temp, sizeof(temp),
        "Supply: %d.%1d/%d.%1d lb  Light: %d.%1d lb (%d.%1d items + %d.%1d oil)  Oil: %d/%d  Slots: %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        light_item_weight / 10, light_item_weight % 10,
        light_oil_weight / 10, light_oil_weight % 10,
        lamp_oil, lamp_capacity, oil_slots, oil_slot_capacity);
    if ((int)strlen(temp) <= term_wid)
    {
        SDL_strlcpy(buf, temp, buflen);
        return;
    }

    strnfmt(temp, sizeof(temp),
        "Sup %d.%1d/%d.%1d lb  Lgt %d.%1d lb (%d.%1d itm + %d.%1d oil)  Oil %d/%d  Slots %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        light_item_weight / 10, light_item_weight % 10,
        light_oil_weight / 10, light_oil_weight % 10,
        lamp_oil, lamp_capacity, oil_slots, oil_slot_capacity);
    if ((int)strlen(temp) <= term_wid)
    {
        SDL_strlcpy(buf, temp, buflen);
        return;
    }

    strnfmt(temp, sizeof(temp),
        "Sup %d.%1d/%d.%1d  Lgt %d.%1d (%d.%1d+%d.%1d)  Oil %d/%d  Sl %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        light_item_weight / 10, light_item_weight % 10,
        light_oil_weight / 10, light_oil_weight % 10,
        lamp_oil, lamp_capacity, oil_slots, oil_slot_capacity);
    if ((int)strlen(temp) <= term_wid)
    {
        SDL_strlcpy(buf, temp, buflen);
        return;
    }

    strnfmt(temp, sizeof(temp), "Sup %d.%1d/%d.%1d  Lgt %d.%1d  Oil %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        lamp_oil, lamp_capacity);
    if ((int)strlen(temp) <= term_wid)
    {
        SDL_strlcpy(buf, temp, buflen);
        return;
    }

    strnfmt(temp, sizeof(temp), "S %d.%1d/%d.%1d  L %d.%1d  O %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        lamp_oil, lamp_capacity);
    SDL_strlcpy(buf, temp, buflen);
}

static void strip_supply_light_turns_suffix(char* name)
{
    char* suffix;

    if (!name)
        return;

    suffix = strstr(name, " (");
    if (suffix && strstr(suffix, " turns)"))
        *suffix = '\0';
}

static object_type* supply_entry_display_object(const supply_list_entry* entry,
    bool aware, object_type* fake)
{
    object_type* o_ptr = NULL;

    if (!entry)
        return NULL;

    if (entry->supply_idx >= 0)
    {
        o_ptr = supplies_entry_at(entry->supply_idx);
    }
    else if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
    {
        o_ptr = &inventory[entry->equip_idx];
    }
    else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
    {
        o_ptr = &inventory[entry->item_idx];
    }
    else if (fake)
    {
        object_wipe(fake);
        object_prep(fake, entry->k_idx);
        if (aware)
            fake->ident |= IDENT_KNOWN;
        fake->number = (entry->total > 0) ? entry->total : 1;
        o_ptr = fake;
    }

    if (entry->single_item_display && o_ptr && fake && o_ptr->k_idx
        && o_ptr->number > 1)
    {
        object_copy(fake, o_ptr);
        fake->number = 1;
        o_ptr = fake;
    }

    return o_ptr;
}

static void supply_strip_leading_name_unit(char* buf)
{
    static cptr prefixes[] = {
        "Fragment of ",
        "Fragments of ",
        "Piece of ",
        "Pieces of ",
        "Strip of ",
        "Strips of ",
        NULL
    };
    int i;

    if (!buf || !buf[0])
        return;

    for (i = 0; prefixes[i]; i++)
    {
        size_t len = strlen(prefixes[i]);

        if (strncmp(buf, prefixes[i], len) == 0)
        {
            memmove(buf, buf + len, strlen(buf + len) + 1);
            return;
        }
    }
}

static void supply_strip_compact_kind_word(char* buf, size_t buflen)
{
    static cptr prefixes[] = {
        "Easter Egg of ",
        "Easter Eggs of ",
        "Herb of ",
        "Herbs of ",
        "Potion of ",
        "Potions of ",
        "Gem of ",
        "Gems of ",
        NULL
    };
    static cptr generic_names[] = {
        "Easter Egg",
        "Easter Eggs",
        "Herb",
        "Herbs",
        "Potion",
        "Potions",
        "Gem",
        "Gems",
        NULL
    };
    int i;

    if (!buf || buflen == 0 || !buf[0])
        return;

    for (i = 0; prefixes[i]; i++)
    {
        size_t len = strlen(prefixes[i]);

        if (strncmp(buf, prefixes[i], len) == 0)
        {
            memmove(buf, buf + len, strlen(buf + len) + 1);
            if (!buf[0])
                SDL_strlcpy(buf, "?", buflen);
            return;
        }
    }

    for (i = 0; generic_names[i]; i++)
    {
        if (strcmp(buf, generic_names[i]) == 0)
        {
            SDL_strlcpy(buf, "?", buflen);
            return;
        }
    }
}

static bool supply_entry_compact_flavorless_name(char* buf, size_t buflen,
    const object_type* o_ptr)
{
    object_kind* k_ptr;

    if (!buf || buflen == 0 || !o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->k_idx < 0 || o_ptr->k_idx >= z_info->k_max)
        return false;

    k_ptr = &k_info[o_ptr->k_idx];
    if (!k_ptr->flavor)
        return false;

    if (object_aware_p(o_ptr))
    {
        object_desc_spoil(buf, buflen, o_ptr, false, 0);
        supply_strip_leading_name_unit(buf);
        supply_strip_compact_kind_word(buf, buflen);
        return true;
    }

    switch (o_ptr->tval)
    {
    case TV_FOOD:
        if (o_ptr->sval >= SV_FOOD_MIN_FOOD)
            return false;
        SDL_strlcpy(buf, easter_time() ? "Easter Egg" : "Herb", buflen);
        break;
    case TV_POTION:
        SDL_strlcpy(buf, "Potion", buflen);
        break;
    case TV_GEM:
        SDL_strlcpy(buf, "Gem", buflen);
        break;
    default:
        return false;
    }

    supply_strip_compact_kind_word(buf, buflen);

    if (object_tried_p(o_ptr))
        SDL_strlcat(buf, " {tried}", buflen);

    return true;
}

static void supply_entry_display_name(char* buf, size_t buflen,
    const supply_list_entry* entry, const object_type* o_ptr, int current_group,
    bool compact_names)
{
    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';

    if (!entry || !o_ptr)
        return;

    if (compact_names
        && supply_entry_compact_flavorless_name(buf, buflen, o_ptr))
    {
        return;
    }

    object_desc(buf, buflen, o_ptr, false, 0);
    supply_strip_leading_name_unit(buf);

    if (current_group == SUPPLY_GROUP_LIGHTS)
    {
        strip_supply_light_turns_suffix(buf);
        if (entry->equipped)
            SDL_strlcat(buf, " [equipped]", buflen);
    }
}

static int supply_entry_turns(const supply_list_entry* entry,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return -1;

    if (o_ptr->tval == TV_FLASK)
        return -2;

    if (o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_LANTERN
        && (!entry || !entry->equipped))
    {
        return -2;
    }

    if (o_ptr->tval != TV_LIGHT)
        return -1;

    if (!fuelable_light_p(o_ptr))
        return -1;

    return player_light_fuel(o_ptr);
}

static bool supply_kind_is_seen(const object_kind* k_ptr);

static bool supply_icon_frame_is_big(const object_type* o_ptr)
{
    return use_bigtile && use_graphics != GRAPHICS_NONE
        && use_graphics != GRAPHICS_PSEUDO && o_ptr && o_ptr->k_idx;
}

static void draw_supply_icon(int col, int row, const object_type* o_ptr)
{
    byte sym_attr;
    char sym_char;

    if (!o_ptr || !o_ptr->k_idx)
        return;

    sym_attr = object_attr(o_ptr);
    sym_char = object_char(o_ptr);
    Term_putch(col, row, sym_attr, sym_char);
    if (use_bigtile)
    {
        if (sym_attr & TILE_FLAG)
            Term_putch(col + 1, row, 255, -1);
        else
            Term_putch(col + 1, row, 0, ' ');
    }
}

static void draw_supply_icon_frame(int col, int row, const object_type* o_ptr)
{
    if (op_ptr && op_ptr->opt[OPT_inventory_selection_square])
        (void)Term_set_extra_cursor(true, col, row,
            supply_icon_frame_is_big(o_ptr));
}

static int supply_group_fixed_icon_kind(int group)
{
    int k_idx = 0;

    switch (group)
    {
    case SUPPLY_GROUP_HERBS:
        k_idx = lookup_kind(TV_FOOD, SV_FOOD_HEALING);
        break;
    case SUPPLY_GROUP_FOOD:
        k_idx = lookup_kind(TV_FOOD, SV_FOOD_LEMBAS);
        if (k_idx <= 0)
            k_idx = lookup_kind(TV_FOOD, SV_FOOD_BREAD);
        break;
    case SUPPLY_GROUP_POTIONS:
        k_idx = lookup_kind(TV_POTION, SV_POTION_HEALING);
        break;
    case SUPPLY_GROUP_GEMS:
        k_idx = lookup_kind(TV_GEM, SV_GEM_LIGHT);
        break;
    case SUPPLY_GROUP_LIGHTS:
        k_idx = lookup_kind(TV_LIGHT, SV_LIGHT_TORCH);
        break;
    default:
        break;
    }

    if (k_idx > 0)
        return k_idx;

    for (int i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        if (!k_ptr->name)
            continue;
        if (supply_kind_matches(group, k_ptr->tval, k_ptr->sval))
            return i;
    }

    return 0;
}

static bool supply_group_kind_is_carried(int group, int k_idx)
{
    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (o_ptr->k_idx == k_idx && supply_item_matches(group, o_ptr))
            return true;
    }

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (o_ptr && o_ptr->k_idx == k_idx && supply_item_matches(group, o_ptr))
            return true;
    }

    return false;
}

static bool supply_group_icon_candidate(int group, int k_idx,
    bool require_known_or_carried)
{
    object_kind* k_ptr;

    if (k_idx <= 0 || k_idx >= z_info->k_max)
        return false;

    k_ptr = &k_info[k_idx];
    if (!k_ptr->name)
        return false;
    if (!supply_kind_matches(group, k_ptr->tval, k_ptr->sval))
        return false;

    if (!require_known_or_carried)
        return true;

    return supply_group_kind_is_carried(group, k_idx)
        || supply_kind_is_seen(k_ptr);
}

static int supply_group_random_icon_kind(int group)
{
    int chosen = 0;
    int seen = 0;

    for (int pass = 0; pass < 2 && chosen <= 0; pass++)
    {
        bool require_known_or_carried = (pass == 0);

        seen = 0;
        for (int i = 0; i < z_info->k_max; i++)
        {
            if (!supply_group_icon_candidate(group, i,
                    require_known_or_carried))
                continue;

            seen++;
            if (rand_int(seen) == 0)
                chosen = i;
        }
    }

    return (chosen > 0) ? chosen : supply_group_fixed_icon_kind(group);
}

static void choose_supply_group_icon_kinds(
    int group_icon_kinds[SUPPLY_GROUP_MAX])
{
    bool random_icons = op_ptr && op_ptr->opt[OPT_supply_menu_random_icons];
    u64b saved_state = Rand_state_export();

    for (int group = 0; group < SUPPLY_GROUP_MAX; group++)
    {
        if (group == SUPPLY_GROUP_LIGHTS || !random_icons)
            group_icon_kinds[group] = supply_group_fixed_icon_kind(group);
        else
            group_icon_kinds[group] = supply_group_random_icon_kind(group);
    }

    Rand_state_import(saved_state);
}

static void prepare_supply_group_icons(supply_group_icon icons[SUPPLY_GROUP_MAX],
    const int group_icon_kinds[SUPPLY_GROUP_MAX])
{
    for (int group = 0; group < SUPPLY_GROUP_MAX; group++)
    {
        object_type* icon_obj = &icons[group].obj;
        int k_idx = group_icon_kinds[group];

        icons[group].has_icon = false;
        object_wipe(icon_obj);

        if (group == SUPPLY_GROUP_LIGHTS)
        {
            object_type* light_ptr = &inventory[INVEN_LITE];

            if (light_ptr->k_idx && light_ptr->tval == TV_LIGHT)
            {
                object_copy(icon_obj, light_ptr);
                icons[group].has_icon = true;
                continue;
            }
        }

        if (k_idx <= 0 || k_idx >= z_info->k_max)
            continue;

        object_prep(icon_obj, k_idx);
        icon_obj->ident |= IDENT_KNOWN;
        icons[group].has_icon = true;
    }
}

static void supply_init_columns(const knowledge_browser_layout* layout,
    int current_group, supply_list_columns* cols)
{
    int col;

    if (!layout || !cols)
        return;

    memset(cols, 0, sizeof(*cols));

    cols->show_sym = true;
    cols->show_qty = true;
    cols->show_weight = (current_group == SUPPLY_GROUP_FOOD)
        || (current_group == SUPPLY_GROUP_LIGHTS);
    cols->show_turns = (current_group == SUPPLY_GROUP_LIGHTS);

    col = layout->term_wid;

    if (cols->show_qty)
    {
        col -= 4;
        cols->qty_col = col;
        col -= 1;
    }

    if (cols->show_turns)
    {
        col -= 5;
        cols->turns_col = col;
        col -= 1;
    }

    if (cols->show_weight)
    {
        col -= 5;
        cols->weight_col = col;
        col -= 1;
    }

    cols->name_col = layout->list_col;
    if (cols->show_sym)
    {
        cols->sym_hdr_col = layout->list_col;
        cols->sym_col = layout->list_col;
        cols->name_col = layout->list_col + (use_bigtile ? 2 : 1);
    }

    cols->name_w = col - cols->name_col;
    if (cols->name_w < 1)
        cols->name_w = 1;
}

static int supply_max_name_len(int current_group, supply_list_entry entries[],
    int entry_cnt, bool compact_names)
{
    int max_len = 0;
    int i;

    for (i = 0; i < entry_cnt; i++)
    {
        supply_list_entry* entry = &entries[i];
        object_kind* k_ptr;
        object_type fake;
        object_type* o_ptr;
        char name[128];
        int len;

        if (entry->k_idx < 0 || entry->k_idx >= z_info->k_max)
            continue;

        k_ptr = &k_info[entry->k_idx];
        o_ptr = supply_entry_display_object(entry, k_ptr->aware, &fake);
        if (!o_ptr)
            continue;

        supply_entry_display_name(name, sizeof(name), entry, o_ptr,
            current_group, compact_names);
        len = (int)strlen(name);
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

static bool supply_use_compact_names_for_width(
    const knowledge_browser_layout* layout)
{
    return layout && (layout->term_wid <= SUPPLY_COMPACT_TERM_WIDTH);
}

static void compute_supply_group_totals(int totals[SUPPLY_GROUP_MAX])
{
    int i;

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
        totals[i] = 0;

    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        if (supply_kind_matches(SUPPLY_GROUP_HERBS, o_ptr->tval, o_ptr->sval))
            totals[SUPPLY_GROUP_HERBS] += o_ptr->number;
        else if (supply_kind_matches(SUPPLY_GROUP_FOOD, o_ptr->tval, o_ptr->sval))
            totals[SUPPLY_GROUP_FOOD] += o_ptr->number;
        else if (o_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += o_ptr->number;
        else if (o_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += o_ptr->number;
        else if (supply_item_matches(SUPPLY_GROUP_LIGHTS, o_ptr))
            totals[SUPPLY_GROUP_LIGHTS] += player_oil_container_slot_cost(o_ptr) > 0
                ? player_oil_container_slot_cost(o_ptr) * o_ptr->number
                : o_ptr->number;
    }

    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        if (!s_ptr || !s_ptr->k_idx)
            continue;

        if (supply_kind_matches(SUPPLY_GROUP_HERBS, s_ptr->tval, s_ptr->sval))
            totals[SUPPLY_GROUP_HERBS] += s_ptr->number;
        else if (supply_kind_matches(SUPPLY_GROUP_FOOD, s_ptr->tval, s_ptr->sval))
            totals[SUPPLY_GROUP_FOOD] += s_ptr->number;
        else if (s_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += s_ptr->number;
        else if (s_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += s_ptr->number;
        else if (supply_item_matches(SUPPLY_GROUP_LIGHTS, s_ptr))
            totals[SUPPLY_GROUP_LIGHTS] += player_oil_container_slot_cost(s_ptr) > 0
                ? player_oil_container_slot_cost(s_ptr) * s_ptr->number
                : s_ptr->number;
    }

    object_type* light_ptr = &inventory[INVEN_LITE];
    if (supply_item_matches(SUPPLY_GROUP_LIGHTS, light_ptr))
    {
        totals[SUPPLY_GROUP_LIGHTS] += player_oil_container_slot_cost(light_ptr) > 0
            ? player_oil_container_slot_cost(light_ptr) * MAX(light_ptr->number, 1)
            : MAX(light_ptr->number, 1);
    }
}

static bool supply_kind_is_seen(const object_kind* k_ptr)
{
    if (!k_ptr)
        return false;

    if (cheat_know || p_ptr->wizard)
        return true;

    return k_ptr->everseen || k_ptr->tried;
}

static int collect_supply_entries(int group_idx, supply_list_entry entries[])
{
    int count = 0;
    int capacity = z_info->k_max;
    int i;

    if (!entries)
        return 0;

    memset(entries, 0, sizeof(supply_list_entry) * capacity);
    for (i = 0; i < capacity; i++)
    {
        entries[i].item_idx = -1;
        entries[i].supply_idx = -1;
        entries[i].equip_idx = -1;
        entries[i].k_idx = -1;
        entries[i].single_item_display = false;
    }

    if (group_idx == SUPPLY_GROUP_LIGHTS)
    {
        for (i = 0; i < INVEN_PACK; i++)
        {
            object_type* o_ptr = &inventory[i];
            int value;
            int unit;

            if (!supply_item_matches(group_idx, o_ptr))
                continue;

            value = MAX(o_ptr->number, 1);
            if (o_ptr->tval == TV_FLASK)
            {
                if (count >= capacity)
                    break;

                entries[count].k_idx = o_ptr->k_idx;
                entries[count].item_idx = i;
                entries[count].total = value;
                entries[count].supply_idx = -1;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = false;
                count++;
                continue;
            }

            for (unit = 0; unit < value && count < capacity; unit++)
            {
                entries[count].k_idx = o_ptr->k_idx;
                entries[count].item_idx = i;
                entries[count].total = 1;
                entries[count].supply_idx = -1;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = (o_ptr->number > 1);
                count++;
            }
        }

        for (i = 0; i < supplies_entry_count(); i++)
        {
            object_type* s_ptr = supplies_entry_at(i);
            int value;
            int unit;

            if (!supply_item_matches(group_idx, s_ptr))
                continue;

            value = MAX(s_ptr->number, 1);
            if (s_ptr->tval == TV_FLASK)
            {
                if (count >= capacity)
                    break;

                entries[count].k_idx = s_ptr->k_idx;
                entries[count].item_idx = SUPPLIES_INDEX;
                entries[count].total = value;
                entries[count].supply_idx = i;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = false;
                count++;
                continue;
            }

            for (unit = 0; unit < value && count < capacity; unit++)
            {
                entries[count].k_idx = s_ptr->k_idx;
                entries[count].item_idx = SUPPLIES_INDEX;
                entries[count].total = 1;
                entries[count].supply_idx = i;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = (s_ptr->number > 1);
                count++;
            }
        }

        {
            object_type* l_ptr = &inventory[INVEN_LITE];

            if (supply_item_matches(group_idx, l_ptr) && count < capacity)
            {
                entries[count].k_idx = l_ptr->k_idx;
                entries[count].item_idx = INVEN_LITE;
                entries[count].total = 1;
                entries[count].supply_idx = -1;
                entries[count].equip_idx = INVEN_LITE;
                entries[count].equipped = true;
                entries[count].single_item_display = false;
                count++;
            }
        }
    }
    else
    {

        /* Aggregate carried items first */
        for (i = 0; i < INVEN_PACK; i++)
        {
            object_type* o_ptr = &inventory[i];
            int j;

            if (!o_ptr->k_idx)
                continue;

            if (!supply_item_matches(group_idx, o_ptr))
                continue;

            int value = o_ptr->number;

            for (j = 0; j < count; j++)
            {
                if (entries[j].k_idx == o_ptr->k_idx)
                {
                    entries[j].total += value;
                    if (entries[j].item_idx < 0)
                        entries[j].item_idx = i;
                    break;
                }
            }

            if (j == count)
            {
                if (count >= capacity)
                    break;

                entries[count].k_idx = o_ptr->k_idx;
                entries[count].item_idx = i;
                entries[count].total = value;
                entries[count].supply_idx = -1;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = false;
                count++;
            }
        }

        /* Aggregate supplies from the cache */
        for (i = 0; i < supplies_entry_count(); i++)
        {
            object_type* s_ptr = supplies_entry_at(i);
            int j;

            if (!s_ptr || !s_ptr->k_idx)
                continue;

            if (!supply_item_matches(group_idx, s_ptr))
                continue;

            int value = s_ptr->number;

            for (j = 0; j < count; j++)
            {
                if (entries[j].k_idx == s_ptr->k_idx)
                {
                    entries[j].total += value;
                    if (entries[j].item_idx < 0)
                        entries[j].item_idx = SUPPLIES_INDEX;
                    entries[j].supply_idx = i;
                    break;
                }
            }

            if (j == count)
            {
                if (count >= capacity)
                    break;

                entries[count].k_idx = s_ptr->k_idx;
                entries[count].item_idx = SUPPLIES_INDEX;
                entries[count].total = value;
                entries[count].supply_idx = i;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = false;
                count++;
            }
        }

        if (group_idx == SUPPLY_GROUP_LIGHTS)
        {
            object_type* l_ptr = &inventory[INVEN_LITE];
            int j;

            if (supply_item_matches(group_idx, l_ptr))
            {
                for (j = 0; j < count; j++)
                {
                    if (entries[j].k_idx == l_ptr->k_idx)
                    {
                        entries[j].total += MAX(l_ptr->number, 1);
                        entries[j].equip_idx = INVEN_LITE;
                        entries[j].equipped = true;
                        if (entries[j].item_idx < 0)
                            entries[j].item_idx = INVEN_LITE;
                        break;
                    }
                }

                if (j == count && count < capacity)
                {
                    entries[count].k_idx = l_ptr->k_idx;
                    entries[count].item_idx = INVEN_LITE;
                    entries[count].total = MAX(l_ptr->number, 1);
                    entries[count].supply_idx = -1;
                    entries[count].equip_idx = INVEN_LITE;
                    entries[count].equipped = true;
                    entries[count].single_item_display = false;
                    count++;
                }
            }
        }
    }

    /* Add seen kinds even when none are carried.
     * Lights are listed only when actually carried, to avoid misleading 0-count
     * placeholders in the supply menu. */
    if (group_idx == SUPPLY_GROUP_LIGHTS)
    {
        if (count < capacity)
        {
            entries[count].k_idx = -1;
            entries[count].item_idx = -1;
            entries[count].total = 0;
            entries[count].supply_idx = -1;
            entries[count].equip_idx = -1;
            entries[count].equipped = false;
            entries[count].single_item_display = false;
        }

        return count;
    }

    /* Add seen kinds even when none are carried */
    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        int j;

        if (!k_ptr->name)
            continue;

        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (!supply_kind_is_seen(k_ptr))
            continue;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == i)
                break;
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = i;
            entries[count].item_idx = -1;
            entries[count].total = 0;
            entries[count].supply_idx = -1;
            entries[count].equip_idx = -1;
            entries[count].equipped = false;
            entries[count].single_item_display = false;
            count++;
        }
    }

    if (count < capacity)
    {
        entries[count].k_idx = -1;
        entries[count].item_idx = -1;
        entries[count].total = 0;
        entries[count].supply_idx = -1;
        entries[count].equip_idx = -1;
        entries[count].equipped = false;
        entries[count].single_item_display = false;
    }

    return count;
}

static byte get_supply_item_color(int k_idx, bool aware)
{
    object_kind* k_ptr;

    if (k_idx < 0 || k_idx >= z_info->k_max)
        return TERM_WHITE;

    k_ptr = &k_info[k_idx];

    /* Unidentified items all use slate color */
    if (!aware)
        return TERM_SLATE;

    /* Color by specific item type */
    switch (k_ptr->tval)
    {
        case TV_FOOD: /* Herbs */
            switch (k_ptr->sval)
            {
                case SV_FOOD_RAGE:         return TERM_RED;    /* Red for rage */
                case SV_FOOD_SUSTENANCE:   return TERM_GREEN;    /* Green for sustenance */
                case SV_FOOD_TERROR:       return TERM_VIOLET;   /* Violet for fear */
                case SV_FOOD_HEALING:      return TERM_L_GREEN;  /* Light green for healing */
                case SV_FOOD_RESTORATION:  return TERM_BLUE;     /* Blue for restoration */
                case SV_FOOD_HUNGER:       return TERM_UMBER;    /* Brown for hunger */
                case SV_FOOD_VISIONS:      return TERM_L_UMBER;  /* Light brown for visions */
                case SV_FOOD_ENTRANCEMENT: return TERM_VIOLET;   /* Violet for entrancement */
                case SV_FOOD_WEAKNESS:     return TERM_SLATE;    /* Grey for weakness */
                case SV_FOOD_SICKNESS:     return TERM_L_DARK;   /* Dark grey for sickness */
                case SV_FOOD_LEMBAS:       return TERM_L_WHITE;
                default:                   return TERM_WHITE;
            }

        case TV_POTION:
            switch (k_ptr->sval)
            {
                case SV_POTION_MIRUVOR:          return TERM_WHITE;  /* White for Miruvor */
                case SV_POTION_ORCISH_LIQUOR:    return TERM_UMBER;    /* Brown for liquor */
                case SV_POTION_ESGALDUIN:        return TERM_VIOLET;   /* Violet for Esgalduin */
                case SV_POTION_CLARITY:          return TERM_L_UMBER;  /* Light brown for clarity */
                case SV_POTION_HEALING:          return TERM_L_GREEN;  /* Light green for healing */
                case SV_POTION_VOICE:            return TERM_L_BLUE;  /* White for voice */
                case SV_POTION_true_SIGHT:       return TERM_BLUE;     /* Blue for true sight */
                case SV_POTION_ANTIDOTE:         return TERM_GREEN;    /* Green for antidote */
                case SV_POTION_QUICKNESS:        return TERM_ORANGE;  /* Light brown for speed */
                case SV_POTION_ELEM_RESISTANCE:  return TERM_L_BLUE;   /* Orange for resistance */
                case SV_POTION_STR:              return TERM_RED;      /* Red for strength */
                case SV_POTION_DEX:              return TERM_GREEN;    /* Green for dexterity */
                case SV_POTION_CON:              return TERM_L_RED;     /* Blue for constitution */
                case SV_POTION_GRA:              return TERM_BLUE;   /* Violet for grace */
                case SV_POTION_SLOWNESS:         return TERM_SLATE;    /* Grey for slowness */
                case SV_POTION_POISON:           return TERM_L_DARK;   /* Dark for poison */
                case SV_POTION_BLINDNESS:        return TERM_L_DARK;   /* Dark for blindness */
                case SV_POTION_CONFUSION:        return TERM_SLATE;    /* Grey for confusion */
                case SV_POTION_DEC_DEX:          return TERM_SLATE;    /* Grey for decrease dex */
                case SV_POTION_DEC_GRA:          return TERM_SLATE;    /* Grey for decrease grace */
                default:                         return TERM_WHITE;
            }

        case TV_GEM:
            switch (k_ptr->sval)
            {
                case SV_GEM_FREEDOM:         return TERM_WHITE;  /* White for freedom */
                case SV_GEM_LIGHT:           return TERM_ORANGE;   /* Orange for light */
                case SV_GEM_SANCTITY:        return TERM_L_UMBER;  /* Light brown for sanctity */
                case SV_GEM_UNDERSTANDING:   return TERM_BLUE;     /* Blue for understanding */
                case SV_GEM_REVELATIONS:     return TERM_L_BLUE;   /* Violet for revelations */
                case SV_GEM_TREASURES:       return TERM_ORANGE;   /* Orange for treasures */
                case SV_GEM_FOES:            return TERM_RED;      /* Red for foes */
                case SV_GEM_SELF_KNOWLEDGE:  return TERM_GREEN;  /* Light green for self-knowledge */
                case SV_GEM_WARDING:         return TERM_VIOLET;  /* Light brown for warding */
                case SV_GEM_RECHARGING:      return TERM_BLUE;     /* Blue for recharging */
                case SV_GEM_SHADOWS:         return TERM_L_DARK;   /* Dark for shadows */
                default:                     return TERM_WHITE;
            }

        case TV_FLASK:
            return TERM_YELLOW;

        case TV_LIGHT:
            switch (k_ptr->sval)
            {
                case SV_LIGHT_TORCH:   return TERM_UMBER;
                case SV_LIGHT_MALLORN: return TERM_YELLOW;
                case SV_LIGHT_LANTERN: return TERM_L_UMBER;
                case SV_LIGHT_LESSER_JEWEL: return TERM_L_BLUE;
                case SV_LIGHT_FEANORIAN: return TERM_WHITE;
                default:               return TERM_WHITE;
            }

        default:
            return TERM_WHITE;
    }
}

static void display_supply_group_list(int col, int row, int wid, int per_page,
    int grp_idx[], int grp_cur, int grp_top, int group_totals[],
    const supply_group_icon icons[SUPPLY_GROUP_MAX], bool active)
{
    int i;
    int total_col = col + wid - 3;
    int text_col = col + (use_bigtile ? 2 : 1);

    for (i = 0; i < per_page; i++)
    {
        int grp_pos = grp_top + i;
        int grp;
        byte base_color;
        byte attr;
        char buf[8];

        if (grp_pos >= SUPPLY_GROUP_MAX || grp_idx[grp_pos] < 0)
            break;

        grp = grp_idx[grp_pos];

        /* Assign color based on group type */
        switch (grp)
        {
            case SUPPLY_GROUP_HERBS:   base_color = TERM_GREEN;   break;
            case SUPPLY_GROUP_FOOD:    base_color = TERM_L_GREEN; break;
            case SUPPLY_GROUP_POTIONS: base_color = TERM_VIOLET;  break;
            case SUPPLY_GROUP_GEMS:    base_color = TERM_BLUE;    break;
            case SUPPLY_GROUP_LIGHTS:  base_color = TERM_YELLOW;  break;
            default:                   base_color = TERM_WHITE;   break;
        }

        /* Highlight cursor with white, dim if empty */
        if (grp_top + i == grp_cur)
            attr = TERM_L_WHITE;
        else if (group_totals[grp] == 0)
            attr = TERM_L_DARK;
        else
            attr = base_color;

        Term_erase(col, row + i, wid);
        if (icons && icons[grp].has_icon)
        {
            draw_supply_icon(col, row + i, &icons[grp].obj);
            if (active && grp_top + i == grp_cur)
                draw_supply_icon_frame(col, row + i, &icons[grp].obj);
        }
        c_put_str(attr, supply_group_text[grp], row + i, text_col);

        strnfmt(buf, sizeof(buf), "%3d", group_totals[grp]);
        c_put_str(attr, buf, row + i, total_col);
    }
}

static void display_supply_list(const knowledge_browser_layout* layout, int row,
    int per_page, supply_list_entry entries[], int entry_cnt, int entry_cur,
    int entry_top, int current_group, int column,
    const supply_list_columns* cols, bool compact_names)
{
    int i;

    for (i = 0; i < per_page; i++)
    {
        int idx = entry_top + i;
        int y = row + i;

        Term_erase(layout->list_col, y, 255);

        if (idx >= entry_cnt)
            continue;

        supply_list_entry* entry = &entries[idx];
        object_type fake;
        object_kind* k_ptr;
        bool aware;
        object_type* o_ptr;
        byte base_attr, cursor_attr, attr;
        char name[128];
        char cell_buf[16];
        bool selected = (column == 1 && idx == entry_cur);

        if (entry->k_idx < 0 || entry->k_idx >= z_info->k_max)
            continue;

        k_ptr = &k_info[entry->k_idx];
        aware = k_ptr->aware;
        /* Items with 0 count should be grey */
        if (entry->total == 0)
        {
            base_attr = TERM_L_DARK;
            cursor_attr = TERM_SLATE;
        }
        else
        {
            /* Get color based on specific item type */
            base_attr = get_supply_item_color(entry->k_idx, aware);
            cursor_attr = aware ? TERM_L_WHITE : TERM_WHITE;
        }
        /* Only highlight when right panel is active (column == 1) */
        attr = selected ? cursor_attr : base_attr;

        o_ptr = supply_entry_display_object(entry, aware, &fake);
        if (!o_ptr)
            continue;

        supply_entry_display_name(name, sizeof(name), entry, o_ptr,
            current_group, compact_names);
        if (cols->show_sym)
        {
            draw_supply_icon(cols->sym_col, y, o_ptr);
            if (selected)
                draw_supply_icon_frame(cols->sym_col, y, o_ptr);
        }

        Term_putstr(cols->name_col, y, cols->name_w, attr, name);

        if (cols->show_weight)
        {
            strnfmt(cell_buf, sizeof(cell_buf), "%d.%1d",
                o_ptr->weight / 10, o_ptr->weight % 10);
            Term_putstr(cols->weight_col, y, 5, attr, cell_buf);
        }

        if (cols->show_turns)
        {
            int turns = supply_entry_turns(entry, o_ptr);

            if (turns >= 0)
                strnfmt(cell_buf, sizeof(cell_buf), "%5d", turns);
            else if (turns == -2)
                strnfmt(cell_buf, sizeof(cell_buf), "%5s", "");
            else
                strnfmt(cell_buf, sizeof(cell_buf), "%5s", "inf");
            Term_putstr(cols->turns_col, y, 5, attr, cell_buf);
        }

        if (cols->show_qty)
        {
            strnfmt(cell_buf, sizeof(cell_buf), "x%-3d", entry->total);
            Term_putstr(cols->qty_col, y, 4, attr, cell_buf);
        }

    }

    for (; i < per_page; i++)
    {
        Term_erase(layout->list_col, row + i, 255);
    }
}

/*
 * Move the cursor in a browser window
 */
static int browser_move_index(int cur, int count, int delta, bool wrap)
{
    int next;

    if (count <= 0)
        return 0;

    next = cur + delta;
    if (wrap)
    {
        next %= count;
        if (next < 0)
            next += count;
        return next;
    }

    if (next >= count)
        next = count - 1;
    if (next < 0)
        next = 0;
    return next;
}

static void browser_cursor_with_rows(char ch, int* column, int* grp_cur,
    int grp_cnt, int* list_cur, int list_cnt, int page_rows, bool wrap_rows)
{
    int d;
    int col = *column;
    int grp = *grp_cur;
    int list = *list_cur;
    int page_jump = (page_rows > 0) ? page_rows : BROWSER_ROWS;

    /* Extract direction */
    d = target_dir(ch);

    if (!d)
        return;

    /* Diagonals - hack */
    if ((ddx[d] > 0) && ddy[d])
    {
        /* Browse group list */
        if (!col)
        {
            int old_grp = grp;

            /* Move up or down */
            grp = browser_move_index(grp, grp_cnt, ddy[d] * page_jump,
                wrap_rows);
            if (grp != old_grp)
                list = 0;
        }

        /* Browse sub-list list */
        else
        {
            /* Move up or down */
            list = browser_move_index(list, list_cnt, ddy[d] * page_jump,
                wrap_rows);
        }

        (*grp_cur) = grp;
        (*list_cur) = list;

        return;
    }

    if (ddx[d])
    {
        col += ddx[d];
        if (col < 0)
            col = 0;
        if (col > 1)
            col = 1;

        (*column) = col;

        return;
    }

    /* Browse group list */
    if (!col)
    {
        int old_grp = grp;

        /* Move up or down */
        grp = browser_move_index(grp, grp_cnt, ddy[d], wrap_rows);
        if (grp != old_grp)
            list = 0;
    }

    /* Browse sub-list list */
    else
    {
        /* Move up or down */
        list = browser_move_index(list, list_cnt, ddy[d], wrap_rows);
    }

    (*grp_cur) = grp;
    (*list_cur) = list;
}

/*
 * Hack -- Create a "forged" artefact
 */
static bool prepare_fake_artefact(object_type* o_ptr, byte name1)
{
    s16b i;

    artefact_type* a_ptr = &a_info[name1];

    /* Ignore "empty" artefacts */
    if (a_ptr->tval + a_ptr->sval == 0)
        return false;

    /* Get the "kind" index */
    i = lookup_kind(a_ptr->tval, a_ptr->sval);

    /* Oops */
    if (!i)
        return (false);

    /* Create the artefact */
    object_prep(o_ptr, i);

    /* Save the name */
    o_ptr->name1 = name1;

    /* Extract the fields */
    o_ptr->pval = a_ptr->pval;
    o_ptr->att = a_ptr->att;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->evn = a_ptr->evn;
    o_ptr->pd = a_ptr->pd;
    o_ptr->ps = a_ptr->ps;
    o_ptr->weight = a_ptr->weight;

    memcpy(o_ptr->stat_bonus, a_ptr->stat_bonus, sizeof(o_ptr->stat_bonus));
    memcpy(o_ptr->skill_bonus, a_ptr->skill_bonus, sizeof(o_ptr->skill_bonus));

    // add the abilities
    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
        o_ptr->bane_type[i + o_ptr->abilities] = a_ptr->bane_type[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    /*identify it*/
    object_known(o_ptr);

    /*make it a spoiler item*/
    o_ptr->ident |= IDENT_SPOIL;

    /* Hack -- extract the "cursed" flag */
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    /* Success */
    return (true);
}

/*
 * Describe fake artefact
 */
void desc_art_fake(int a_idx)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Make fake artefact */
    prepare_fake_artefact(i_ptr, a_idx);

    /* Hack -- Handle stuff */
    handle_stuff();

    /* Reset the cursor */
    Term_gotoxy(0, 0);

    object_info_screen(i_ptr);
}

/*
 * Display known artefacts
 */
void do_cmd_knowledge_artefacts(void)
{
    log_debug("Player opened artifacts knowledge screen");
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_ARTEFACTS);
}

/*
 * Description of each monster group.
 */
static cptr monster_group_text[] = { "Uniques", /*All uniques, all letters*/
    /*Unused*/ /*'a'*/
    /*Unused*/ /*'A'*/
    "Bats & Birds", /*'b'*/
    /*Unused*/ /*'B'*/
    /*Unused*/ /*'c'*/
    "Canines", /*'C'*/
    "Young Dragons", /*'d'*/
    "Great Dragons", /*'D'*/
    /*Unused*/ /*'e'*/
    /*Unused*/ /*'E'*/
    "Felines", /*'f'*/
    /*Unused*/ /*'F'*/
    /*Unused*/ /*'g'*/
    "Giants", /*'G'*/
    /*Unused*/ /*'h'*/
    "Horrors", /*'H'*/
    /*Unused*/ /*'i'*/
    "Insects", /*'I'*/
    /*Unused*/ /*'j'*/
    /*Unused*/ /*'J'*/
    /*Unused*/ /*'k'*/
    /*Unused*/ /*'K'*/
    /*Unused*/ /*'l'*/
    /*Unused*/ /*'L'*/
    "Young Spiders", /*'m'*/
    "Spiders", /*'M'*/
    /*Unused*/ /*'n'*/
    "Nameless Things", /*'N'*/
    "Orcs", /*'o'*/
    /*Unused*/ /*'O'*/
    /*Unused*/ /*'p'*/
    /*Unused*/ /*'P'*/
    /*Unused*/ /*'q'*/
    /*Unused*/ /*'Q'*/
    /*Unused*/ /*'r'*/
    "Raukar", /*'R'*/
    "Serpents", /*'s'*/
    "Ancient Serpents", /*'S'*/
    /*Unused*/ /*'t'*/
    "Trolls", /*'T'*/
    /*Unused*/ /*'u'*/
    /*Unused*/ /*'U'*/
    "Vampires", /*'v'*/
    "Valar", /*'V'*/
    "Creeping Shadows", /*'w'*/
    "Wights and Wraiths", /*'W'*/
    /*Unused*/ /*'x'*/
    /*Unused*/ /*'X'*/
    /*Unused*/ /*'y'*/
    /*Unused*/ /*'Y'*/
    /*Unused*/ /*'Z'*/
    /*Unused*/ /*'Z'*/
    "Plants", /*'&'*/
    "People", /*'@'*/
    NULL };

/*
 * Symbols of monsters in each group. Note the "Uniques" group
 * is handled differently.
 */
static cptr monster_group_char[] = { (char*)-1L,
    /*"a", Unused*/
    /*"A", Unused*/
    "b",
    /*"B", Unused*/
    /*"c", Unused*/
    "C", "d", "D",
    /*"e", Unused*/
    /*"E", Unused*/
    "f",
    /*"F", Unused*/
    /*"g", Unused*/
    "G",
    /*"h", Unused*/
    "H",
    /*"i", Unused*/
    "I",
    /*"j", Unused*/
    /*"J", Unused*/
    /*"k", Unused*/
    /*"K", Unused*/
    /*"l", Unused*/
    /*"L", Unused*/
    "m", "M",
    /*"n", Unused*/
    "N", "o",
    /*"O", Unused*/
    /*"p", Unused*/
    /*"P", Unused*/
    /*"q", Unused*/
    /*"Q", Unused*/
    /*"r", Unused*/
    "R", "s", "S",
    /*"t", Unused*/
    "T",
    /*"u", Unused*/
    /*"U", Unused*/
    "v", "V", "w", "W",
    /*"x", Unused*/
    /*"X", Unused*/
    /*"y", Unused*/
    /*"Y", Unused*/
    /*"z", Unused*/
    /*"Z", Unused*/
    "&", // plants
    "@", // human/elf/dwarf
    NULL };

/*
 * Build a list of monster indexes in the given group. Return the number
 * of monsters in the group.
 */
static int collect_monsters(int grp_cur, monster_list_entry* mon_idx, int mode)
{
    int i, mon_count = 0;

    /* Get a list of x_char in this group */
    cptr group_char = monster_group_char[grp_cur];

    /* XXX Hack -- Check if this is the "Uniques" group */
    bool grp_unique = (monster_group_char[grp_cur] == (char*)-1L);

    /* Check every race */
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Access the race */
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Is this a unique? */
        bool unique = (r_ptr->flags1 & (RF1_UNIQUE));

        /* Skip empty race */
        if (!r_ptr->name)
            continue;

        if (grp_unique && !(unique))
            continue;

        /* Require known monsters */
        if (!(mode & 0x02) && (!cheat_know) && (!know_monster_info)
            && (!(l_ptr->tsights)))
            continue;

        // Ignore monsters that can't be generated
        if (r_ptr->level > 25)
            continue;

        /* Check for race in the group */
        if ((grp_unique) || (strchr(group_char, r_ptr->d_char)))
        {
            /* Add the race */
            mon_idx[mon_count++].r_idx = i;

            /* XXX Hack -- Just checking for non-empty group */
            if (mode & 0x01)
                break;
        }
    }

    /* Terminate the list */
    mon_idx[mon_count].r_idx = 0;

    /* Return the number of races */
    return (mon_count);
}

#if 0
/*
 * Display the monsters in a group.
 */
static void display_monster_list(int col, int row, int per_page,
    monster_list_entry* mon_idx, int mon_cur, int mon_top, int grp_cur)
{
    int i;

    u32b known_uniques, dead_uniques, slay_count;

    /* Start with 0 kills*/
    known_uniques = dead_uniques = slay_count = 0;

    /* Count up monster kill counts */
    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        // skip monsters that cannot be generated
        if ((r_ptr->rarity == 0) || (r_ptr->level > 25))
            continue;

        /* Require non-unique monsters */
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            /*Count if we have seen the unique*/
            if (l_ptr->tsights)
            {
                known_uniques++;

                /*Count if the unique is dead*/
                if (r_ptr->max_num == 0)
                {
                    dead_uniques++;
                    slay_count++;
                }
            }

            // increase the uniques count anyway for forewarned or cheaters
            else if (know_monster_info || cheat_know)
            {
                known_uniques++;
            }
        }

        /* Collect "appropriate" monsters */
        else
            slay_count += l_ptr->pkills;
    }

    /* Display lines until done */
    for (i = 0; i < per_page && mon_idx[i].r_idx; i++)
    {
        byte attr;

        /* Get the race index */
        int r_idx = mon_idx[mon_top + i].r_idx;

        /* Access the race */
        monster_race* r_ptr = &r_info[r_idx];
        monster_lore* l_ptr = &l_list[r_idx];

        char race_name[80];

        /* Get the monster race name (singular)*/
        monster_desc_race(race_name, sizeof(race_name), r_idx);

        /* Choose a color */
        attr = ((i + mon_top == mon_cur) ? TERM_L_BLUE : TERM_WHITE);

        /* Display the name */
        c_prt(attr, race_name, row + i, col);

        if (cheat_know)
        {
            c_prt(attr, format("%d", r_idx), row + i, 60);
        }

        /* Display symbol */
        Term_putch(68, row + i, r_ptr->x_attr, r_ptr->x_char);
        if (use_bigtile)
        {
            if ((byte)(r_ptr->x_attr) & 0x80)
                Term_putch(69, row + i, 255, -1);
            else
                Term_putch(69, row + i, 0, ' ');
        }

        /* Display kills */
        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /*use alive/dead for uniques*/
            put_str(format("%s", (r_ptr->max_num == 0) ? " dead" : "alive"),
                row + i, 73);
        }
        else
            put_str(format("%5d", l_ptr->pkills), row + i, 73);
    }

    /* Clear remaining lines */
    for (; i < per_page; i++)
    {
        Term_erase(col, row + i, 255);
    }

    /*Clear the monster count line*/
    Term_erase(0, 22, 255);

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        c_put_str(TERM_L_BLUE,
            format("Total Creatures Slain: %d. ", slay_count), 22, col + 2);
    }
    else
    {
        c_put_str(TERM_L_BLUE,
            format("Known Uniques: %d, Slain Uniques: %d.", known_uniques,
                dead_uniques),
            22, col + 2);
    }
}
#endif

/*
 * Display known monsters.
 */
void do_cmd_knowledge_monsters(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_MONSTERS);
}

/*
 * Add a pval so the object descriptions don't look strange*
 */
void apply_magic_fake(object_type* o_ptr)
{
    s16b old_pval = o_ptr->pval;

    /* Analyze type */
    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    {
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;
        break;
    }

    /*many rings need a pval*/
    case TV_RING:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        /* Strength, Dexterity */
        case SV_RING_STR:
        case SV_RING_DEX:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;

            break;
        }

        /* Ring of Accuracy */
        case SV_RING_ACCURACY:
        {
            /* Bonus to hit */
            if (o_ptr->att < 1)
                o_ptr->att = 1;

            break;
        }

        /* Ring of Evasion */
        case SV_RING_EVASION:
        {
            /* Bonus to evasion */
            if (o_ptr->evn < 1)
                o_ptr->evn = 1;

            break;
        }

        /* Ring of Secrets */
        case SV_RING_SECRETS:
        {
            /* Bonus to perception */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;

            break;
        }

        /* Ring of Ered Luin */
        case SV_RING_ERED_LUIN:
        {
            /* Bonus to will */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Ring of the Laiquendi */
        case SV_RING_LAIQUENDI:
        {
            /* Bonus to stealth and archery */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }
        }

        /*break for TVAL-Rings*/
        break;
    }

    case TV_AMULET:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        /* Various amulets */
        case SV_AMULET_CON:
        case SV_AMULET_GRA:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Amulet of Protection */
        case SV_AMULET_PROTECTION:
        {
            if (o_ptr->pd < 1)
                o_ptr->pd = 1;
            if (o_ptr->ps < 1)
                o_ptr->ps = 1;
            break;
        }

        /* Amulet of the Blessed Realm */
        case SV_AMULET_BLESSED_REALM:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Amulet of the Vigilant Eye */
        case SV_AMULET_VIGILANT_EYE:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        default:
            break;
        }
        /*break for TVAL-Amulets*/
        break;
    }

    case TV_LIGHT:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_MALLORN:
        case SV_LIGHT_LANTERN:
        {
            o_ptr->timeout = 0;

            break;
        }
        }
        /*break for TVAL-Lights*/
        break;
    }

    /*give them one charge*/
    case TV_STAFF:
    {
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;

        break;
    }
    }

    int pval_delta = (int)o_ptr->pval - (int)old_pval;
    if (pval_delta != 0)
        object_apply_pval_delta_with_mask(o_ptr, object_pval_flags1(o_ptr), pval_delta);
}

/*
 * Describe fake object
 */
static void desc_obj_fake(int k_idx)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Create the object */
    object_prep(i_ptr, k_idx);

    /*add minimum bonuses so the descriptions don't look strange*/
    apply_magic_fake(i_ptr);

    /* It's fully known */
    i_ptr->ident |= IDENT_KNOWN;

    /* Hack -- Handle stuff */
    handle_stuff();

    /* Reset the cursor */
    Term_gotoxy(0, 0);

    object_info_screen(i_ptr);
}

#if 0
/*
 * Display the objects in a group. (Incorporates some code from jdh)
 */
static void display_object_list(int col, int row, int per_page,
    object_list_entry object_idx[], int object_cur, int object_top)
{
    int i;

    /* Display lines until done */
    for (i = 0; i < per_page && object_idx[i].type != OBJ_NONE; i++)
    {
        char buf[80];

        /* Get the object index */
        int oidx = object_top + i;
        object_list_entry* obj = &object_idx[oidx];
        object_kind* k_ptr;
        ego_item_type* e_ptr;
        byte attr, cursor;

        switch (obj->type)
        {
        case OBJ_NORMAL:
            /* Access the object */
            k_ptr = &k_info[obj->idx];

            /* Choose a color */
            attr = ((k_ptr->aware) ? TERM_WHITE : TERM_SLATE);
            cursor = ((k_ptr->aware) ? TERM_L_BLUE : TERM_BLUE);
            attr = ((oidx == object_cur) ? cursor : attr);

            /* Acquire the basic "name" of the object*/
            strip_name(buf, obj->idx);

            /* Display the name */
            c_prt(attr, buf, row + i, col);

            if (cheat_know)
                c_prt(attr, format("%d", obj->idx), row + i, 70);

            if (k_ptr->aware)
            {
                /* Obtain attr/char */
                byte a = k_ptr->flavor ? (flavor_info[k_ptr->flavor].x_attr)
                                       : k_ptr->d_attr;
                byte c = k_ptr->flavor ? (flavor_info[k_ptr->flavor].x_char)
                                       : k_ptr->d_char;

                /* Display symbol */
                Term_putch(76, row + i, a, c);
            }

            break;

        case OBJ_SPECIAL:
            e_ptr = &e_info[obj->e_idx];

            /* Choose a color */
            attr = ((e_ptr->aware) ? TERM_WHITE : TERM_SLATE);
            cursor = ((e_ptr->aware) ? TERM_L_BLUE : TERM_BLUE);
            attr = ((oidx == object_cur) ? cursor : attr);

            if (obj->sval == -1)
            {
                buf[0] = '\0';
                snprintf(buf, sizeof(buf), "  %s", &e_name[e_ptr->name]);
            }
            else
            {
                int j;
                char buf2[80];

                /* Find the specific type */
                buf[0] = '\0';
                buf2[0] = '\0';
                for (j = 0; j < z_info->k_max; ++j)
                {
                    if ((k_info[j].tval == obj->tval)
                        && (k_info[j].sval == obj->sval))
                    {
                        strip_name(buf2, j);
                        break;
                    }
                }

                snprintf(buf, sizeof(buf), "%s %s", buf2, &e_name[e_ptr->name]);
            }

            c_prt(attr, buf, row + i, col);

            break;

        case OBJ_NONE:
        default:
            break;
        }
    }

    /* Clear remaining lines */
    for (; i < per_page; i++)
    {
        Term_erase(col, row + i, 255);
    }
}
#endif

static cptr knowledge_page_name(int page)
{
    switch (page)
    {
    case KNOWLEDGE_PAGE_ARTEFACTS:
        return "Artefacts";
    case KNOWLEDGE_PAGE_OBJECTS:
        return "Objects";
    case KNOWLEDGE_PAGE_MONSTERS:
        return "Monsters";
    case KNOWLEDGE_PAGE_CURSES:
        return "Curses";
    default:
        return "Known";
    }
}

static int knowledge_normalize_page(int page)
{
    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return g_knowledge_last_page;

    return page;
}

static cptr knowledge_tab_label(int page)
{
    static const cptr labels[] = {
        "Arts",
        "Objs",
        "Mons",
        "Curses"
    };

    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return "";

    return labels[page];
}

static int knowledge_tab_col(int page)
{
    int i;
    int col = 0;

    for (i = KNOWLEDGE_PAGE_ARTEFACTS; i < page; i++)
        col += (int)strlen(knowledge_tab_label(i)) + 1;

    return col;
}

static void knowledge_init_layout(knowledge_browser_layout* layout,
    int max_group_len, bool has_groups)
{
    int min_group_w = 8;
    int min_list_w = 16;

    Term_get_size(&layout->term_wid, &layout->term_hgt);

    if (layout->term_wid < 1)
        layout->term_wid = 80;
    if (layout->term_hgt < 1)
        layout->term_hgt = 24;

    layout->title_row = 0;
    layout->tabs_row = (layout->term_hgt > 1) ? 1 : 0;
    layout->header_row = (layout->term_hgt > 2) ? 2 : layout->tabs_row;
    layout->divider_row = (layout->term_hgt > 3) ? 3 : layout->header_row;
    layout->list_row = layout->divider_row + 1;
    layout->prompt_row = layout->term_hgt - 1;
    layout->status_row = (layout->prompt_row > layout->list_row)
        ? (layout->prompt_row - 1)
        : layout->prompt_row;
    layout->list_rows = layout->status_row - layout->list_row;
    if (layout->list_rows < 1)
        layout->list_rows = 1;

    if (!has_groups)
    {
        layout->group_col = 0;
        layout->group_w = 0;
        layout->divider_col = -1;
        layout->list_col = 0;
        layout->list_w = layout->term_wid;
        return;
    }

    layout->group_col = 0;
    layout->group_w = max_group_len;
    if (layout->group_w < 10)
        layout->group_w = 10;
    if (layout->group_w > layout->term_wid / 3)
        layout->group_w = layout->term_wid / 3;
    if (layout->group_w < min_group_w)
        layout->group_w = min_group_w;

    while ((layout->group_w > min_group_w)
        && (layout->term_wid - (layout->group_w + 3) < min_list_w))
    {
        layout->group_w--;
    }

    if (layout->term_wid - (layout->group_w + 3) < min_list_w)
    {
        layout->group_w = layout->term_wid - min_list_w - 3;
        if (layout->group_w < min_group_w)
            layout->group_w = min_group_w;
    }

    layout->divider_col = layout->group_w + 1;
    layout->list_col = layout->divider_col + 2;
    layout->list_w = layout->term_wid - layout->list_col;
    if (layout->list_w < 1)
        layout->list_w = 1;
}

static void knowledge_expand_active_column(knowledge_browser_layout* layout)
{
    if (!layout)
        return;

    layout->group_col = 0;
    layout->group_w = layout->term_wid;
    layout->divider_col = -1;
    layout->list_col = 0;
    layout->list_w = layout->term_wid;
}

static void knowledge_draw_tabs(const knowledge_browser_layout* layout, int page,
    bool tabs_focus)
{
    int i;
    int col = 0;

    Term_erase(0, layout->tabs_row, 255);

    for (i = KNOWLEDGE_PAGE_ARTEFACTS; i <= KNOWLEDGE_PAGE_CURSES; i++)
    {
        cptr label = knowledge_tab_label(i);
        byte attr = TERM_SLATE;
        int remaining = layout->term_wid - col;
        int len;

        if (remaining <= 0)
            break;

        if (i == page)
            attr = tabs_focus ? TERM_YELLOW : TERM_L_BLUE;

        len = (int)strlen(label);
        Term_putstr(col, layout->tabs_row, remaining, attr, label);
        col += len;

        if ((i < KNOWLEDGE_PAGE_CURSES) && (col < layout->term_wid))
        {
            Term_putstr(col, layout->tabs_row, layout->term_wid - col,
                TERM_SLATE, " ");
            col++;
        }
    }
}

static void knowledge_draw_frame(const knowledge_browser_layout* layout, int page,
    bool has_groups, cptr list_label, bool tabs_focus)
{
    int i;
    char title[64];
    char page_buf[16];

    Term_clear();

    strnfmt(title, sizeof(title), "Known lore - %s", knowledge_page_name(page));
    Term_putstr(0, layout->title_row, layout->term_wid, TERM_L_WHITE + TERM_SHADE,
        title);

    strnfmt(page_buf, sizeof(page_buf), "%d/4", page + 1);
    if ((int)strlen(page_buf) < layout->term_wid)
    {
        int page_col = layout->term_wid - (int)strlen(page_buf);
        Term_putstr(page_col, layout->title_row, layout->term_wid - page_col,
            TERM_SLATE, page_buf);
    }

    knowledge_draw_tabs(layout, page, tabs_focus);

    Term_erase(0, layout->header_row, 255);
    if (has_groups)
    {
        Term_putstr(layout->group_col, layout->header_row, layout->group_w,
            TERM_SLATE, "Group");
        Term_putstr(layout->list_col, layout->header_row, layout->list_w,
            TERM_SLATE, list_label);
    }
    else
    {
        Term_putstr(0, layout->header_row, layout->term_wid, TERM_SLATE,
            list_label);
    }

    for (i = 0; i < layout->term_wid; i++)
    {
        Term_putch(i, layout->divider_row, TERM_L_DARK, '=');
    }

    if (has_groups && layout->divider_col >= 0)
    {
        for (i = 0; i < layout->list_rows; i++)
        {
            Term_putch(layout->divider_col, layout->list_row + i, TERM_L_DARK, '|');
        }
    }

    if (layout->status_row != layout->prompt_row)
        Term_erase(0, layout->status_row, 255);
    Term_erase(0, layout->prompt_row, 255);
}

static void knowledge_draw_prompt(const knowledge_browser_layout* layout)
{
    char prompt[128];

    if (steamdeck_controls_active())
    {
        char prev_label[16];
        char next_label[16];
        char confirm_label[16];
        char recall_label[16];
        char back_label[16];

        controller_prompt_label('e', "L1", prev_label, sizeof(prev_label));
        controller_prompt_label('i', "R1", next_label, sizeof(next_label));
        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        controller_prompt_label(steamdeck_info_key(), "RS", recall_label,
            sizeof(recall_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(prompt, sizeof(prompt),
            "D-pad move  [%s/%s] page  [%s/%s] recall  [%s] back",
            prev_label, next_label, confirm_label, recall_label, back_label);
        Term_putstr(0, layout->prompt_row, layout->term_wid, TERM_L_DARK, prompt);
    }
    else
    {
        SDL_strlcpy(prompt, "Dir move  e/i page  Up at top=tabs  Space/r recall  Esc",
            sizeof(prompt));
        Term_putstr(0, layout->prompt_row, layout->term_wid, TERM_SLATE, prompt);
    }
}

static void knowledge_clamp_group_state(int* column, int* grp_cur, int* grp_top,
    int grp_cnt, int* entry_cur, int* entry_top, int entry_cnt, int per_page)
{
    if (grp_cnt <= 0)
    {
        *column = 0;
        *grp_cur = 0;
        *grp_top = 0;
        *entry_cur = 0;
        *entry_top = 0;
        return;
    }

    if (*grp_cur >= grp_cnt)
        *grp_cur = grp_cnt - 1;
    if (*grp_cur < 0)
        *grp_cur = 0;
    if (*grp_top > *grp_cur)
        *grp_top = *grp_cur;
    if (*grp_cur >= *grp_top + per_page)
        *grp_top = *grp_cur - per_page + 1;
    if (*grp_top < 0)
        *grp_top = 0;

    if (entry_cnt <= 0)
    {
        *column = 0;
        *entry_cur = 0;
        *entry_top = 0;
    }
    else
    {
        if (*entry_cur >= entry_cnt)
            *entry_cur = entry_cnt - 1;
        if (*entry_cur < 0)
            *entry_cur = 0;
        if (*entry_top > *entry_cur)
            *entry_top = *entry_cur;
        if (*entry_cur >= *entry_top + per_page)
            *entry_top = *entry_cur - per_page + 1;
        if (*entry_top < 0)
            *entry_top = 0;
    }

    if (*column < 0)
        *column = 0;
    if (*column > 1)
        *column = 1;
    if (entry_cnt <= 0)
        *column = 0;
}

static void knowledge_clamp_list_state(int* cur, int* top, int count, int per_page)
{
    if (count <= 0)
    {
        *cur = 0;
        *top = 0;
        return;
    }

    if (*cur >= count)
        *cur = count - 1;
    if (*cur < 0)
        *cur = 0;
    if (*top > *cur)
        *top = *cur;
    if (*cur >= *top + per_page)
        *top = *cur - per_page + 1;
    if (*top < 0)
        *top = 0;
}

static void knowledge_display_groups(const knowledge_browser_layout* layout,
    int grp_idx[], cptr group_text[], int grp_cnt, int grp_cur, int grp_top)
{
    int i;

    for (i = 0; i < layout->list_rows; i++)
    {
        int y = layout->list_row + i;
        int idx = grp_top + i;

        Term_erase(layout->group_col, y, layout->group_w);

        if (idx >= grp_cnt)
            continue;

        Term_putstr(layout->group_col, y, layout->group_w,
            (idx == grp_cur) ? TERM_L_BLUE : TERM_WHITE,
            group_text[grp_idx[idx]]);
    }
}

static int knowledge_artefact_name_width(const knowledge_browser_layout* layout,
    bool* show_debug)
{
    bool debug = cheat_know && (layout->term_wid >= 78);
    int name_w = layout->list_w;

    if (debug)
    {
        int debug_name_w = (layout->term_wid - 12) - layout->list_col - 1;
        if (debug_name_w >= 12)
            name_w = debug_name_w;
        else
            debug = false;
    }

    if (show_debug)
        *show_debug = debug;

    return (name_w > 0) ? name_w : 1;
}

static void knowledge_object_display_name(char* buf, size_t buflen,
    const object_list_entry* obj)
{
    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';

    if (!obj)
        return;

    switch (obj->type)
    {
    case OBJ_NORMAL:
        strip_name(buf, obj->idx);
        break;

    case OBJ_SPECIAL:
    {
        ego_item_type* e_ptr = &e_info[obj->e_idx];

        if (obj->sval == -1)
        {
            strnfmt(buf, buflen, "  %s", &e_name[e_ptr->name]);
        }
        else
        {
            int j;
            char base_name[80];

            base_name[0] = '\0';
            for (j = 0; j < z_info->k_max; ++j)
            {
                if ((k_info[j].tval == obj->tval)
                    && (k_info[j].sval == obj->sval))
                {
                    strip_name(base_name, j);
                    break;
                }
            }

            strnfmt(buf, buflen, "%s %s", base_name, &e_name[e_ptr->name]);
        }
        break;
    }

    case OBJ_NONE:
    default:
        break;
    }
}

static int knowledge_object_name_width(const knowledge_browser_layout* layout,
    bool* show_idx, bool* show_sym)
{
    bool idx = cheat_know && (layout->term_wid >= 70);
    bool sym = (layout->term_wid >= 44);
    int name_w = layout->list_w;

    if (idx)
    {
        int idx_name_w = (layout->term_wid - 5) - layout->list_col - 1;
        if (idx_name_w >= 12 && idx_name_w < name_w)
            name_w = idx_name_w;
        else if (idx_name_w < 12)
            idx = false;
    }

    if (sym)
    {
        int sym_col = layout->term_wid - (use_bigtile ? 2 : 1);
        int sym_name_w = sym_col - layout->list_col - 1;
        if (sym_name_w >= 12 && sym_name_w < name_w)
            name_w = sym_name_w;
        else if (sym_name_w < 12)
            sym = false;
    }

    if (show_idx)
        *show_idx = idx;
    if (show_sym)
        *show_sym = sym;

    return (name_w > 0) ? name_w : 1;
}

static int knowledge_monster_name_width(const knowledge_browser_layout* layout,
    bool* show_sym, bool* show_kills)
{
    bool sym = (layout->term_wid >= 44);
    bool kills = (layout->term_wid >= 56);
    int name_w = layout->list_w;

    if (kills)
    {
        int kills_name_w = (layout->term_wid - 5) - layout->list_col - 1;
        if (kills_name_w >= 12 && kills_name_w < name_w)
            name_w = kills_name_w;
        else if (kills_name_w < 12)
            kills = false;
    }

    if (sym)
    {
        int kills_col = layout->term_wid - 5;
        int sym_col = kills ? (kills_col - 2)
                            : (layout->term_wid - (use_bigtile ? 2 : 1));
        int sym_name_w = sym_col - layout->list_col - 1;
        if (sym_name_w >= 12 && sym_name_w < name_w)
            name_w = sym_name_w;
        else if (sym_name_w < 12)
            sym = false;
    }

    if (show_sym)
        *show_sym = sym;
    if (show_kills)
        *show_kills = kills;

    return (name_w > 0) ? name_w : 1;
}

static int knowledge_max_artefact_name_len(int artefact_idx[], int artefact_cnt)
{
    int max_len = 0;
    int i;

    for (i = 0; i < artefact_cnt; i++)
    {
        object_type object_type_body;
        object_type* i_ptr = &object_type_body;
        char o_name[80];
        int len;

        object_wipe(i_ptr);
        prepare_fake_artefact(i_ptr, artefact_idx[i]);
        object_desc(o_name, sizeof(o_name), i_ptr, true, 0);
        len = (int)strlen(o_name);
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

static int knowledge_max_object_name_len(object_list_entry object_idx[],
    int object_cnt)
{
    int max_len = 0;
    int i;

    for (i = 0; i < object_cnt; i++)
    {
        char buf[80];
        int len;

        knowledge_object_display_name(buf, sizeof(buf), &object_idx[i]);
        len = (int)strlen(buf);
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

static int knowledge_max_monster_name_len(monster_list_entry mon_idx[],
    int mon_cnt)
{
    int max_len = 0;
    int i;

    for (i = 0; i < mon_cnt; i++)
    {
        char race_name[80];
        int len;

        monster_desc_race(race_name, sizeof(race_name), mon_idx[i].r_idx);
        len = (int)strlen(race_name);
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

static bool knowledge_should_use_single_column_for_names(int split_name_w,
    int full_name_w, int max_name_len)
{
    if (split_name_w < 12)
        return full_name_w > split_name_w;

    return (max_name_len > split_name_w) && (full_name_w > split_name_w);
}

static void knowledge_display_artefacts(const knowledge_browser_layout* layout,
    int artefact_idx[], int artefact_cnt, int artefact_cur, int artefact_top)
{
    bool show_debug = false;
    int idx_col = layout->term_wid - 12;
    int dep_col = layout->term_wid - 8;
    int rar_col = layout->term_wid - 4;
    int name_w = knowledge_artefact_name_width(layout, &show_debug);
    int i;

    if (show_debug)
    {
        Term_putstr(idx_col, layout->header_row, 3, TERM_SLATE, "Idx");
        Term_putstr(dep_col, layout->header_row, 3, TERM_SLATE, "Dep");
        Term_putstr(rar_col, layout->header_row, 3, TERM_SLATE, "Rar");
    }

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int idx = artefact_top + i;
        object_type object_type_body;
        object_type* i_ptr = &object_type_body;
        char o_name[80];
        byte attr;

        Term_erase(layout->list_col, row, 255);

        if (idx >= artefact_cnt)
            continue;

        attr = (idx == artefact_cur) ? TERM_L_BLUE : TERM_WHITE;
        object_wipe(i_ptr);
        prepare_fake_artefact(i_ptr, artefact_idx[idx]);
        object_desc(o_name, sizeof(o_name), i_ptr, true, 0);
        Term_putstr(layout->list_col, row, name_w, attr, o_name);

        if (show_debug)
        {
            artefact_type* a_ptr = &a_info[artefact_idx[idx]];
            c_prt(attr, format("%3d", artefact_idx[idx]), row, idx_col);
            c_prt(attr, format("%3d", a_ptr->level), row, dep_col);
            c_prt(attr, format("%3d", a_ptr->rarity), row, rar_col);
        }
    }
}

static void knowledge_display_objects(const knowledge_browser_layout* layout,
    object_list_entry object_idx[], int object_cnt, int object_cur, int object_top)
{
    bool show_idx = false;
    bool show_sym = false;
    int idx_col = layout->term_wid - 5;
    int sym_col = layout->term_wid - (use_bigtile ? 2 : 1);
    int name_w = knowledge_object_name_width(layout, &show_idx, &show_sym);
    int i;

    if (show_idx)
        Term_putstr(idx_col, layout->header_row, 3, TERM_SLATE, "Idx");
    if (show_sym)
        Term_putstr(sym_col, layout->header_row, 3, TERM_SLATE, "Sym");

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int oidx = object_top + i;
        object_list_entry* obj;
        object_kind* k_ptr;
        ego_item_type* e_ptr;
        byte attr;
        byte cursor;
        char buf[80];

        Term_erase(layout->list_col, row, 255);

        if (oidx >= object_cnt)
            continue;

        obj = &object_idx[oidx];

        switch (obj->type)
        {
        case OBJ_NORMAL:
            k_ptr = &k_info[obj->idx];
            attr = k_ptr->aware ? TERM_WHITE : TERM_SLATE;
            cursor = k_ptr->aware ? TERM_L_BLUE : TERM_BLUE;
            attr = (oidx == object_cur) ? cursor : attr;
            knowledge_object_display_name(buf, sizeof(buf), obj);
            Term_putstr(layout->list_col, row, name_w, attr, buf);

            if (show_idx)
                c_prt(attr, format("%d", obj->idx), row, idx_col);

            if (show_sym && k_ptr->aware)
            {
                byte a = k_ptr->flavor ? flavor_info[k_ptr->flavor].x_attr : k_ptr->d_attr;
                byte c = k_ptr->flavor ? flavor_info[k_ptr->flavor].x_char : k_ptr->d_char;
                Term_putch(sym_col, row, a, c);
                if (use_bigtile)
                {
                    if (a & 0x80)
                        Term_putch(sym_col + 1, row, 255, -1);
                    else
                        Term_putch(sym_col + 1, row, 0, ' ');
                }
            }
            break;

        case OBJ_SPECIAL:
            e_ptr = &e_info[obj->e_idx];
            attr = e_ptr->aware ? TERM_WHITE : TERM_SLATE;
            cursor = e_ptr->aware ? TERM_L_BLUE : TERM_BLUE;
            attr = (oidx == object_cur) ? cursor : attr;
            knowledge_object_display_name(buf, sizeof(buf), obj);
            Term_putstr(layout->list_col, row, name_w, attr, buf);
            break;

        case OBJ_NONE:
        default:
            break;
        }
    }
}

static void knowledge_monster_summary(char* buf, size_t buflen, int grp_cur)
{
    int i;
    u32b known_uniques = 0;
    u32b dead_uniques = 0;
    u32b slay_count = 0;

    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        if ((r_ptr->rarity == 0) || (r_ptr->level > 25))
            continue;

        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            if (l_ptr->tsights)
            {
                known_uniques++;
                if (r_ptr->max_num == 0)
                {
                    dead_uniques++;
                    slay_count++;
                }
            }
            else if (know_monster_info || cheat_know)
            {
                known_uniques++;
            }
        }
        else
        {
            slay_count += l_ptr->pkills;
        }
    }

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        strnfmt(buf, buflen, "Total creatures slain: %u.", (unsigned)slay_count);
    }
    else
    {
        strnfmt(buf, buflen, "Known uniques: %u, slain uniques: %u.",
            (unsigned)known_uniques, (unsigned)dead_uniques);
    }
}

static void knowledge_display_monsters(const knowledge_browser_layout* layout,
    monster_list_entry mon_idx[], int mon_cnt, int mon_cur, int mon_top)
{
    bool show_sym = false;
    bool show_kills = false;
    int kills_col = layout->term_wid - 5;
    int sym_col;
    int name_w = knowledge_monster_name_width(layout, &show_sym, &show_kills);
    int i;
    sym_col = show_kills ? (kills_col - 2)
                         : (layout->term_wid - (use_bigtile ? 2 : 1));

    if (show_sym)
        Term_putstr(sym_col, layout->header_row, 3, TERM_SLATE, "Sym");
    if (show_kills)
        Term_putstr(kills_col, layout->header_row, 5, TERM_SLATE, "Kills");

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int idx = mon_top + i;
        int r_idx;
        monster_race* r_ptr;
        monster_lore* l_ptr;
        byte attr;
        char race_name[80];

        Term_erase(layout->list_col, row, 255);

        if (idx >= mon_cnt)
            continue;

        r_idx = mon_idx[idx].r_idx;
        r_ptr = &r_info[r_idx];
        l_ptr = &l_list[r_idx];
        attr = (idx == mon_cur) ? TERM_L_BLUE : TERM_WHITE;

        monster_desc_race(race_name, sizeof(race_name), r_idx);
        Term_putstr(layout->list_col, row, name_w, attr, race_name);

        if (show_sym)
        {
            Term_putch(sym_col, row, r_ptr->x_attr, r_ptr->x_char);
            if (use_bigtile)
            {
                if ((byte)(r_ptr->x_attr) & 0x80)
                    Term_putch(sym_col + 1, row, 255, -1);
                else
                    Term_putch(sym_col + 1, row, 0, ' ');
            }
        }

        if (show_kills)
        {
            if (r_ptr->flags1 & RF1_UNIQUE)
                put_str((r_ptr->max_num == 0) ? " dead" : "alive", row, kills_col);
            else
                put_str(format("%5d", l_ptr->pkills), row, kills_col);
        }
    }
}

static int knowledge_collect_curses(int curse_idx[])
{
    int id;
    int count = 0;

    for (id = 0; id < (int)z_info->cu_max; id++)
    {
        if (CURSE_SEEN(id))
            curse_idx[count++] = id;
    }

    return count;
}

static cptr knowledge_curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;

    if (strncmp(raw, "Curse of ", 9) == 0)
        raw += 9;
    else if (strncmp(raw, "Burden of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Sorrow of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Doom of ", 8) == 0)
        raw += 8;

    return raw;
}

static cptr knowledge_blessing_display_name(int idx)
{
    if (cu_info[idx].blessing_name)
    {
        cptr raw = cu_name + cu_info[idx].blessing_name;

        if (strncmp(raw, "Blessing of ", 12) == 0)
            raw += 12;

        return raw;
    }

    return knowledge_curse_display_name(idx);
}

static void knowledge_display_curses(const knowledge_browser_layout* layout,
    int curse_idx[], int curse_cnt, int curse_cur, int curse_top)
{
    int i;

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int idx = curse_top + i;
        int id;
        byte attr;

        Term_erase(0, row, 255);

        if (idx >= curse_cnt)
            continue;

        id = curse_idx[idx];
        attr = (idx == curse_cur) ? TERM_L_BLUE : TERM_L_RED;
        Term_putstr(0, row, layout->term_wid, attr,
            knowledge_curse_display_name(id));
    }
}

static void knowledge_detail_prompt(int row, bool steamdeck, cptr title,
    cptr accept_label)
{
    Term_erase(0, row, 255);
    if (steamdeck)
    {
        char hint_buf[48];
        strnfmt(hint_buf, sizeof(hint_buf), "(press %s)", accept_label);
        Term_putstr(1, row, -1, TERM_L_WHITE, hint_buf);
    }
    else
    {
        Term_putstr(1, row, -1, TERM_L_WHITE, "(press any key)");
    }

    (void)inkey();
    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, title);
}

static void knowledge_show_curse_detail(int curse_id)
{
    int row = 2;
    int wrap_width = Term->wid - 4;
    int page_limit = Term->hgt - 3;
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    curse_type* c = &cu_info[curse_id];
    cptr cname = cu_name + c->name;
    cptr cdesc = cu_text + c->text;
    cptr cpower = cu_text + c->power;
    cptr bname = knowledge_blessing_display_name(curse_id);
    cptr bdesc = (c->blessing_text) ? (cu_text + c->blessing_text) : "";
    cptr bpower = (c->blessing_power) ? (cu_text + c->blessing_power) : "";
    bool has_blessing_text = bdesc && *bdesc;
    bool has_blessing_effect = bpower && *bpower;
    bool has_blessing_info = has_blessing_text || has_blessing_effect
        || (c->blessing_name != 0);
    char effect_line[256];

    if (wrap_width < 20)
        wrap_width = 20;

    if (steamdeck)
    {
        controller_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }

    screen_save();
    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Known Curse:");

    text_out_hook = text_out_to_screen;
    text_out_wrap = wrap_width;

    c_put_str(TERM_L_RED, cname, row++, 1);

    if (row + count_wrapped_lines(cdesc, text_out_wrap, 3) >= page_limit)
        knowledge_detail_prompt(row, steamdeck, "Known Curse:", accept_label);
    Term_gotoxy(3, row);
    text_out_c(TERM_WHITE, cdesc);
    row += count_wrapped_lines(cdesc, text_out_wrap, 3);

    strnfmt(effect_line, sizeof(effect_line), "Effect: %s",
        (*cpower) ? cpower : "[no additional effect listed]");
    if (row + count_wrapped_lines(effect_line, text_out_wrap, 3) >= page_limit)
        knowledge_detail_prompt(row, steamdeck, "Known Curse:", accept_label);
    Term_gotoxy(3, row);
    text_out_c(TERM_RED, "Effect: ");
    text_out_c(TERM_L_DARK, (*cpower) ? cpower : "[no additional effect listed]");
    row += count_wrapped_lines(effect_line, text_out_wrap, 3);

    row++;

    if (has_blessing_info)
    {
        char blessing_line[256];

        if (row + 1 >= page_limit)
            knowledge_detail_prompt(row, steamdeck, "Known Curse:", accept_label);

        Term_putstr(3, row++, -1, TERM_L_GREEN, format("Blessing: %s", bname));

        if (has_blessing_text)
        {
            if (row + count_wrapped_lines(bdesc, text_out_wrap, 5) >= page_limit)
                knowledge_detail_prompt(row, steamdeck, "Known Curse:",
                    accept_label);
            Term_gotoxy(5, row);
            text_out_c(TERM_WHITE, bdesc);
            row += count_wrapped_lines(bdesc, text_out_wrap, 5);
        }

        strnfmt(blessing_line, sizeof(blessing_line), "Effect: %s",
            has_blessing_effect ? bpower : "[no additional effect listed]");
        if (row + count_wrapped_lines(blessing_line, text_out_wrap, 5) >= page_limit)
            knowledge_detail_prompt(row, steamdeck, "Known Curse:",
                accept_label);
        Term_gotoxy(5, row);
        text_out_c(TERM_L_GREEN, "Effect: ");
        text_out_c(TERM_WHITE,
            has_blessing_effect ? bpower : "[no additional effect listed]");
        row += count_wrapped_lines(blessing_line, text_out_wrap, 5);
    }

    if (row + 1 >= Term->hgt)
        row = Term->hgt - 2;

    knowledge_detail_prompt(row + 1, steamdeck, "Known Curse:", accept_label);
    screen_load();
}

static bool knowledge_handle_page_input(char ch, int* page)
{
    int next_page = *page;

    switch (ch)
    {
    case 'A':
    case 'a':
        next_page = KNOWLEDGE_PAGE_ARTEFACTS;
        break;
    case 'B':
    case 'b':
        next_page = KNOWLEDGE_PAGE_OBJECTS;
        break;
    case 'N':
    case 'n':
        next_page = KNOWLEDGE_PAGE_MONSTERS;
        break;
    case 'U':
    case 'u':
        next_page = KNOWLEDGE_PAGE_CURSES;
        break;
    case '\t':
    case ']':
    case 'I':
    case 'i':
        next_page = (*page + 1) % 4;
        break;
    case '[':
    case 'E':
    case 'e':
        next_page = (*page + 3) % 4;
        break;
    default:
        return false;
    }

    *page = next_page;
    g_knowledge_last_page = next_page;
    return true;
}

static bool knowledge_handle_tab_navigation(char ch, int* page, bool* tabs_focus,
    bool can_focus_tabs)
{
    int d = target_dir(ch);

    if (!*tabs_focus)
    {
        if (can_focus_tabs && d && !ddx[d] && (ddy[d] < 0))
        {
            *tabs_focus = true;
            return true;
        }

        return false;
    }

    if (d)
    {
        if (ddx[d] > 0)
        {
            *page = (*page + 1) % 4;
            g_knowledge_last_page = *page;
            return true;
        }
        if (ddx[d] < 0)
        {
            *page = (*page + 3) % 4;
            g_knowledge_last_page = *page;
            return true;
        }
        if (ddy[d] > 0)
        {
            *tabs_focus = false;
            return true;
        }
        if (ddy[d] < 0)
        {
            return true;
        }
    }

    return false;
}

static bool knowledge_is_recall_input(int ch)
{
    int confirm_key = steamdeck_confirm_key();

    if (ch == ' ' || ch == 'R' || ch == 'r' || ch == 'X' || ch == 'x'
        || ch == INPUT_BIND_CONFIRM)
    {
        return true;
    }

    if (confirm_key != GAMEPAD_BIND_NONE && ch == confirm_key)
        return true;

    return false;
}

void do_cmd_knowledge_browser_page(int page)
{
    int i;
    int artefact_grp_idx[100];
    int object_grp_idx[100];
    int monster_grp_idx[100];
    int* artefact_idx = mem_alloc_array(z_info->art_max, int);
    object_list_entry* object_idx =
        mem_alloc_array(z_info->k_max + z_info->e_max + 1, object_list_entry);
    monster_list_entry* mon_idx =
        mem_alloc_array(z_info->r_max, monster_list_entry);
    int* curse_idx = mem_alloc_array(z_info->cu_max, int);
    int artefact_grp_cnt = 0;
    int object_grp_cnt = 0;
    int monster_grp_cnt = 0;
    int artefact_group_w = 0;
    int object_group_w = 0;
    int monster_group_w = 0;
    int curse_cnt = 0;
    int artefact_old = -1;
    int object_old = -1;
    int monster_old = -1;
    knowledge_browser_state state = { 0 };
    bool done = false;

    page = knowledge_normalize_page(page);
    g_knowledge_last_page = page;

    FILE_TYPE(FILE_TYPE_TEXT);

    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    for (i = 0; object_group_text[i] != NULL; i++)
    {
        int len = (int)strlen(object_group_text[i]);

        if (len > artefact_group_w)
            artefact_group_w = len;
        if (len > object_group_w)
            object_group_w = len;

        if (collect_artefacts(i, artefact_idx))
            artefact_grp_idx[artefact_grp_cnt++] = i;
        if (collect_objects(i, NULL))
            object_grp_idx[object_grp_cnt++] = i;
    }

    for (i = 0; monster_group_text[i] != NULL; i++)
    {
        int len = (int)strlen(monster_group_text[i]);

        if (len > monster_group_w)
            monster_group_w = len;
        if ((monster_group_char[i] == (char*)-1L)
            || collect_monsters(i, mon_idx, 0x01))
        {
            monster_grp_idx[monster_grp_cnt++] = i;
        }
    }

    curse_cnt = knowledge_collect_curses(curse_idx);

    screen_save();
    screen_push_supporting_panes_hidden();
    if (p_ptr && p_ptr->playing)
        sdl_music_play_menu_theme();

    while (!done)
    {
        knowledge_browser_layout layout;
        int ch;

        switch (page)
        {
        case KNOWLEDGE_PAGE_ARTEFACTS:
        {
            int artefact_cnt = 0;
            int selected_artefact = -1;
            bool single_column;
            knowledge_browser_layout draw_layout;
            knowledge_browser_layout full_layout;
            char status[96];
            cptr list_label = "Artefact";
            int split_name_w;
            int full_name_w;
            int max_name_len;

            knowledge_init_layout(&layout, artefact_group_w, true);
            if (artefact_grp_cnt > 0)
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], artefact_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], artefact_cnt, layout.list_rows);
            if (artefact_grp_cnt > 0)
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);
            full_layout = layout;
            knowledge_expand_active_column(&full_layout);
            split_name_w = knowledge_artefact_name_width(&layout, NULL);
            full_name_w = knowledge_artefact_name_width(&full_layout, NULL);
            max_name_len = knowledge_max_artefact_name_len(artefact_idx,
                artefact_cnt);
            single_column = knowledge_should_use_single_column_for_names(
                split_name_w, full_name_w, max_name_len);
            draw_layout = layout;
            if (single_column)
            {
                knowledge_expand_active_column(&draw_layout);
                if ((state.column[page] == 0) || (artefact_grp_cnt <= 0))
                    list_label = "Group";
                else
                    list_label = object_group_text[
                        artefact_grp_idx[state.group_cur[page]]];
            }

            knowledge_draw_frame(&draw_layout, page, !single_column, list_label,
                state.tabs_focus);
            if (!single_column || (state.column[page] == 0))
            {
                knowledge_display_groups(&draw_layout, artefact_grp_idx,
                    object_group_text, artefact_grp_cnt, state.group_cur[page],
                    state.group_top[page]);
            }
            if (!single_column || (state.column[page] == 1))
            {
                knowledge_display_artefacts(&draw_layout, artefact_idx,
                    artefact_cnt, state.entry_cur[page], state.entry_top[page]);
            }

            if (artefact_cnt > 0)
            {
                selected_artefact = artefact_idx[state.entry_cur[page]];
                strnfmt(status, sizeof(status), "%d artefact%s in %s.",
                    artefact_cnt, (artefact_cnt == 1) ? "" : "s",
                    object_group_text[artefact_grp_idx[state.group_cur[page]]]);
            }
            else
            {
                SDL_strlcpy(status, "No known artefacts yet.", sizeof(status));
            }
            if (draw_layout.status_row != draw_layout.prompt_row)
                Term_putstr(0, draw_layout.status_row, draw_layout.term_wid,
                    TERM_L_BLUE, status);
            knowledge_draw_prompt(&draw_layout);

            if (selected_artefact != artefact_old)
            {
                handle_stuff();
                artefact_old = selected_artefact;
            }

            if (state.tabs_focus)
            {
                Term_gotoxy(knowledge_tab_col(page), draw_layout.tabs_row);
            }
            else if (artefact_grp_cnt > 0)
            {
                if (state.column[page] == 0)
                    Term_gotoxy(draw_layout.group_col, draw_layout.list_row
                        + (state.group_cur[page] - state.group_top[page]));
                else
                    Term_gotoxy(draw_layout.list_col, draw_layout.list_row
                        + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (artefact_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (artefact_cnt > 0)
                    desc_art_fake(artefact_idx[state.entry_cur[page]]);
                else
                    bell("Nothing to recall.");
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], artefact_grp_cnt,
                    &state.entry_cur[page], artefact_cnt, layout.list_rows,
                    false);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_OBJECTS:
        {
            int object_cnt = 0;
            int tracked_kind = 0;
            bool single_column;
            knowledge_browser_layout draw_layout;
            knowledge_browser_layout full_layout;
            char status[112];
            cptr list_label = "Object";
            int split_name_w;
            int full_name_w;
            int max_name_len;

            knowledge_init_layout(&layout, object_group_w, true);
            if (object_grp_cnt > 0)
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], object_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], object_cnt, layout.list_rows);
            if (object_grp_cnt > 0)
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);
            full_layout = layout;
            knowledge_expand_active_column(&full_layout);
            split_name_w = knowledge_object_name_width(&layout, NULL, NULL);
            full_name_w = knowledge_object_name_width(&full_layout, NULL, NULL);
            max_name_len = knowledge_max_object_name_len(object_idx, object_cnt);
            single_column = knowledge_should_use_single_column_for_names(
                split_name_w, full_name_w, max_name_len);
            draw_layout = layout;
            if (single_column)
            {
                knowledge_expand_active_column(&draw_layout);
                if ((state.column[page] == 0) || (object_grp_cnt <= 0))
                    list_label = "Group";
                else
                    list_label = object_group_text[
                        object_grp_idx[state.group_cur[page]]];
            }

            knowledge_draw_frame(&draw_layout, page, !single_column, list_label,
                state.tabs_focus);
            if (!single_column || (state.column[page] == 0))
            {
                knowledge_display_groups(&draw_layout, object_grp_idx,
                    object_group_text, object_grp_cnt, state.group_cur[page],
                    state.group_top[page]);
            }
            if (!single_column || (state.column[page] == 1))
            {
                knowledge_display_objects(&draw_layout, object_idx, object_cnt,
                    state.entry_cur[page], state.entry_top[page]);
            }

            if ((object_cnt > 0)
                && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL))
            {
                tracked_kind = object_idx[state.entry_cur[page]].idx;
            }

            if (object_cnt > 0)
            {
                object_list_entry* obj = &object_idx[state.entry_cur[page]];
                if ((obj->type == OBJ_NORMAL) && k_info[obj->idx].aware)
                {
                    strnfmt(status, sizeof(status), "%d object%s in %s. Recall available.",
                        object_cnt, (object_cnt == 1) ? "" : "s",
                        object_group_text[object_grp_idx[state.group_cur[page]]]);
                }
                else
                {
                    strnfmt(status, sizeof(status),
                        "%d object%s in %s. Recall works for identified base items.",
                        object_cnt, (object_cnt == 1) ? "" : "s",
                        object_group_text[object_grp_idx[state.group_cur[page]]]);
                }
            }
            else
            {
                SDL_strlcpy(status, "No known objects yet.", sizeof(status));
            }
            if (draw_layout.status_row != draw_layout.prompt_row)
                Term_putstr(0, draw_layout.status_row, draw_layout.term_wid,
                    TERM_L_BLUE, status);
            knowledge_draw_prompt(&draw_layout);

            if (tracked_kind != object_old)
            {
                object_kind_track(tracked_kind);
                handle_stuff();
                object_old = tracked_kind;
            }

            if (state.tabs_focus)
            {
                Term_gotoxy(knowledge_tab_col(page), draw_layout.tabs_row);
            }
            else if (object_grp_cnt > 0)
            {
                if (state.column[page] == 0)
                    Term_gotoxy(draw_layout.group_col, draw_layout.list_row
                        + (state.group_cur[page] - state.group_top[page]));
                else
                    Term_gotoxy(draw_layout.list_col, draw_layout.list_row
                        + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (object_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if ((object_cnt > 0)
                    && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL)
                    && k_info[object_idx[state.entry_cur[page]].idx].aware)
                {
                    desc_obj_fake(object_idx[state.entry_cur[page]].idx);
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], object_grp_cnt,
                    &state.entry_cur[page], object_cnt, layout.list_rows,
                    false);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_MONSTERS:
        {
            int monster_cnt = 0;
            int selected_r_idx = 0;
            bool single_column;
            knowledge_browser_layout draw_layout;
            knowledge_browser_layout full_layout;
            char status[96];
            cptr list_label = "Monster";
            int split_name_w;
            int full_name_w;
            int max_name_len;

            knowledge_init_layout(&layout, monster_group_w, true);
            if (monster_grp_cnt > 0)
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], monster_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], monster_cnt, layout.list_rows);
            if (monster_grp_cnt > 0)
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);
            full_layout = layout;
            knowledge_expand_active_column(&full_layout);
            split_name_w = knowledge_monster_name_width(&layout, NULL, NULL);
            full_name_w = knowledge_monster_name_width(&full_layout, NULL, NULL);
            max_name_len = knowledge_max_monster_name_len(mon_idx, monster_cnt);
            single_column = knowledge_should_use_single_column_for_names(
                split_name_w, full_name_w, max_name_len);
            draw_layout = layout;
            if (single_column)
            {
                knowledge_expand_active_column(&draw_layout);
                if ((state.column[page] == 0) || (monster_grp_cnt <= 0))
                    list_label = "Group";
                else
                    list_label = monster_group_text[
                        monster_grp_idx[state.group_cur[page]]];
            }

            knowledge_draw_frame(&draw_layout, page, !single_column, list_label,
                state.tabs_focus);
            if (!single_column || (state.column[page] == 0))
            {
                knowledge_display_groups(&draw_layout, monster_grp_idx,
                    monster_group_text, monster_grp_cnt, state.group_cur[page],
                    state.group_top[page]);
            }
            if (!single_column || (state.column[page] == 1))
            {
                knowledge_display_monsters(&draw_layout, mon_idx, monster_cnt,
                    state.entry_cur[page], state.entry_top[page]);
            }

            if (monster_cnt > 0)
            {
                selected_r_idx = mon_idx[state.entry_cur[page]].r_idx;
                knowledge_monster_summary(status, sizeof(status),
                    monster_grp_idx[state.group_cur[page]]);
            }
            else
            {
                SDL_strlcpy(status, "No known monsters in this group yet.",
                    sizeof(status));
            }
            if (draw_layout.status_row != draw_layout.prompt_row)
                Term_putstr(0, draw_layout.status_row, draw_layout.term_wid,
                    TERM_L_BLUE, status);
            knowledge_draw_prompt(&draw_layout);

            if (selected_r_idx != monster_old)
            {
                monster_race_track(selected_r_idx);
                handle_stuff();
                monster_old = selected_r_idx;
            }

            if (state.tabs_focus)
            {
                Term_gotoxy(knowledge_tab_col(page), draw_layout.tabs_row);
            }
            else if (monster_grp_cnt > 0)
            {
                if (state.column[page] == 0)
                    Term_gotoxy(draw_layout.group_col, draw_layout.list_row
                        + (state.group_cur[page] - state.group_top[page]));
                else
                    Term_gotoxy(draw_layout.list_col, draw_layout.list_row
                        + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (monster_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (monster_cnt > 0)
                {
                    screen_roff(mon_idx[state.entry_cur[page]].r_idx, NULL);
                    (void)inkey();
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], monster_grp_cnt,
                    &state.entry_cur[page], monster_cnt, layout.list_rows,
                    false);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_CURSES:
        default:
        {
            char status[256];

            knowledge_init_layout(&layout, 0, false);
            knowledge_clamp_list_state(&state.entry_cur[page], &state.entry_top[page],
                curse_cnt, layout.list_rows);
            knowledge_draw_frame(&layout, page, false, "Known curses",
                state.tabs_focus);
            knowledge_display_curses(&layout, curse_idx, curse_cnt,
                state.entry_cur[page], state.entry_top[page]);

            if (curse_cnt > 0)
            {
                curse_type* c = &cu_info[curse_idx[state.entry_cur[page]]];
                cptr cpower = cu_text + c->power;
                strnfmt(status, sizeof(status), "Effect: %s",
                    (*cpower) ? cpower : "[no additional effect listed]");
            }
            else
            {
                SDL_strlcpy(status, "No known curses yet.", sizeof(status));
            }
            if (layout.status_row != layout.prompt_row)
                Term_putstr(0, layout.status_row, layout.term_wid, TERM_L_BLUE, status);
            knowledge_draw_prompt(&layout);

            if (state.tabs_focus)
            {
                Term_gotoxy(knowledge_tab_col(page), layout.tabs_row);
            }
            else if (curse_cnt > 0)
            {
                Term_gotoxy(0, layout.list_row
                    + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus, (curse_cnt <= 0) || (state.entry_cur[page] == 0)))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (curse_cnt > 0)
                    knowledge_show_curse_detail(curse_idx[state.entry_cur[page]]);
                else
                    bell("Nothing to recall.");
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
            {
                int d = target_dir(ch);
                int page_jump = (layout.list_rows > 0) ? layout.list_rows : 1;

                if (curse_cnt <= 0)
                {
                    state.entry_cur[page] = 0;
                    break;
                }

                if (!d)
                    break;

                if (ddx[d] && ddy[d])
                    state.entry_cur[page] += ddy[d] * page_jump;
                else if (ddy[d])
                    state.entry_cur[page] += ddy[d];

                if (state.entry_cur[page] < 0)
                    state.entry_cur[page] = 0;
                if (state.entry_cur[page] >= curse_cnt)
                    state.entry_cur[page] = curse_cnt - 1;
                break;
            }
            }
            break;
        }
        }
    }

    mem_free_null(curse_idx);
    mem_free_null(mon_idx);
    mem_free_null(object_idx);
    mem_free_null(artefact_idx);

    screen_pop_supporting_panes_hidden();
    screen_load();
    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
}

/*
 * Display known objects
 */
bool do_cmd_knowledge_supplies(const supply_menu_request* request)
{
    int i;
    int max = 0;
    int grp_cnt = SUPPLY_GROUP_MAX;
    int grp_idx[SUPPLY_GROUP_MAX + 1];
    int group_totals[SUPPLY_GROUP_MAX];
    int group_icon_kinds[SUPPLY_GROUP_MAX];
    supply_group_icon group_icons[SUPPLY_GROUP_MAX];
    supply_list_entry* entries;
    int grp_cur = 0;
    int grp_top = 0;
    int entry_cur = 0;
    int entry_top = 0;
    int column = 0;
    bool flag = false;
    bool redraw = true;
    supply_menu_action forced_action = SUPPLY_MENU_ACTION_NONE;
    bool hotkey_mode = false;
    bool acted = false;
    bool refresh_after_close = false;
    bool prev_single_column = false;
    int prev_group = -1;
    int prev_column = -1;
    int prev_term_wid = -1;
    int prev_term_hgt = -1;
    int prev_divider_col = -2;

    if (request)
    {
        forced_action = request->action;
        hotkey_mode = request->hotkey_mode;
        if (request->focus_group && request->group >= 0 && request->group < SUPPLY_GROUP_MAX)
            grp_cur = request->group;
        if (forced_action != SUPPLY_MENU_ACTION_NONE)
            column = 1;
    }

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
    {
        int len = strlen(supply_group_text[i]) + 5;
        if (len > max)
            max = len;
        grp_idx[i] = i;
    }
    grp_idx[grp_cnt] = -1;
    max += 2;

    choose_supply_group_icon_kinds(group_icon_kinds);

    entries = mem_alloc_array(z_info->k_max, supply_list_entry);

    screen_save();

    while (!flag)
    {
        int entry_cnt;
        knowledge_browser_layout layout;
        knowledge_browser_layout draw_layout;
        knowledge_browser_layout full_layout;
        bool single_column;
        supply_list_columns split_cols;
        supply_list_columns full_cols;
        supply_list_columns draw_cols;
        int used_weight;
        int light_item_weight;
        int light_oil_weight;
        int light_weight;
        int lamp_oil;
        int oil_slots;
        int oil_slot_capacity;
        int max_weight;
        char weight_buf[128];
        char status_buf[96];
        int split_name_w;
        int full_name_w;
        int max_name_len;
        bool compact_width;
        bool compact_draw_names;

        prepare_supply_group_icons(group_icons, group_icon_kinds);
        compute_supply_group_totals(group_totals);
        knowledge_init_layout(&layout, max, true);
        used_weight = supplies_limit_weight();
        light_item_weight = supplies_carried_light_item_weight();
        light_oil_weight = player_lamp_oil_weight();
        light_weight = light_item_weight + light_oil_weight;
        lamp_oil = player_lamp_oil();
        oil_slots = player_oil_container_slots_used();
        oil_slot_capacity = player_oil_container_slot_capacity();
        max_weight = supplies_current_weight_cap();

        if (grp_cur >= grp_cnt)
            grp_cur = grp_cnt - 1;
        if (grp_cur < 0)
            grp_cur = 0;

        entry_cnt = collect_supply_entries(grp_idx[grp_cur], entries);

        if (entry_cnt == 0)
        {
            entry_cur = 0;
            entry_top = 0;
            if (column)
                column = 0;
        }
        else
        {
            if (entry_cur >= entry_cnt)
                entry_cur = entry_cnt - 1;
            if (entry_cur < 0)
                entry_cur = 0;

            if (entry_cur < entry_top)
                entry_top = entry_cur;
            if (entry_cur >= entry_top + layout.list_rows)
                entry_top = entry_cur - layout.list_rows + 1;
            if (entry_top < 0)
                entry_top = 0;
        }

        if (grp_cur < grp_top)
            grp_top = grp_cur;
        if (grp_cur >= grp_top + layout.list_rows)
            grp_top = grp_cur - layout.list_rows + 1;
        if (grp_top < 0)
            grp_top = 0;

        full_layout = layout;
        knowledge_expand_active_column(&full_layout);
        supply_init_columns(&layout, grp_idx[grp_cur], &split_cols);
        supply_init_columns(&full_layout, grp_idx[grp_cur], &full_cols);
        split_name_w = split_cols.name_w;
        full_name_w = full_cols.name_w;
        compact_width = supply_use_compact_names_for_width(&layout);
        compact_draw_names = compact_width && op_ptr
            && op_ptr->opt[OPT_supply_menu_hide_flavor_compact];
        max_name_len = supply_max_name_len(grp_idx[grp_cur], entries,
            entry_cnt, compact_draw_names);

        if (split_name_w > 1)
            split_name_w--;

        single_column = knowledge_should_use_single_column_for_names(
            split_name_w, full_name_w, max_name_len);
        draw_layout = single_column ? full_layout : layout;
        draw_cols = single_column ? full_cols : split_cols;
        build_supply_weight_summary(weight_buf, sizeof(weight_buf),
            draw_layout.term_wid, used_weight, max_weight, light_weight,
            light_item_weight, light_oil_weight, lamp_oil,
            player_lamp_oil_capacity(), oil_slots, oil_slot_capacity);

        if (redraw || single_column
            || (single_column != prev_single_column)
            || (grp_idx[grp_cur] != prev_group)
            || (column != prev_column)
            || (draw_layout.term_wid != prev_term_wid)
            || (draw_layout.term_hgt != prev_term_hgt)
            || (draw_layout.divider_col != prev_divider_col))
        {
            cptr list_label = (single_column && column)
                ? supply_group_text[grp_idx[grp_cur]]
                : "Name";
            cptr title_label = (draw_layout.term_wid <= 50)
                ? "Supplies - H/F/P/G/Oil"
                : "Supplies - Herbs, Food, Potions, Gems, Lights/Oil";

            Term_clear();
            Term_putstr(0, draw_layout.title_row, draw_layout.term_wid,
                TERM_L_WHITE + TERM_SHADE, title_label);
            Term_putstr(0, draw_layout.tabs_row, draw_layout.term_wid, TERM_SLATE,
                weight_buf);
            Term_erase(0, draw_layout.header_row, 255);

            if (single_column && !column)
            {
                Term_putstr(0, draw_layout.header_row, draw_layout.term_wid,
                    TERM_SLATE, "Group");
            }
            else
            {
                if (!single_column)
                    Term_putstr(0, draw_layout.header_row, draw_layout.group_w,
                        TERM_SLATE, "Group");
                if (draw_cols.show_sym)
                    Term_putstr(draw_cols.sym_hdr_col, draw_layout.header_row,
                        use_bigtile ? 2 : 1, TERM_SLATE, "S");
                Term_putstr(draw_cols.name_col, draw_layout.header_row,
                    draw_cols.name_w, TERM_SLATE, list_label);
                if (draw_cols.show_weight)
                    Term_putstr(draw_cols.weight_col, draw_layout.header_row, 5,
                        TERM_SLATE, "Wt");
                if (draw_cols.show_turns)
                    Term_putstr(draw_cols.turns_col, draw_layout.header_row, 5,
                        TERM_SLATE, "Turns");
                if (draw_cols.show_qty)
                    Term_putstr(draw_cols.qty_col, draw_layout.header_row, 3,
                        TERM_SLATE, "Qty");
            }

            for (i = 0; i < draw_layout.term_wid; i++)
                Term_putch(i, draw_layout.divider_row, TERM_L_DARK, '=');

            if (!single_column)
            {
                for (i = 0; i < draw_layout.list_rows; i++)
                {
                    Term_putch(draw_layout.divider_col, draw_layout.list_row + i,
                        TERM_L_DARK, '|');
                }
            }

            redraw = false;
        }

        prev_single_column = single_column;
        prev_group = grp_idx[grp_cur];
        prev_column = column;
        prev_term_wid = draw_layout.term_wid;
        prev_term_hgt = draw_layout.term_hgt;
        prev_divider_col = draw_layout.divider_col;

        (void)Term_set_extra_cursor(false, 0, 0, false);

        if (!single_column || !column)
        {
            int group_list_w = (!single_column) ? draw_layout.group_w
                                                : layout.group_w;

            display_supply_group_list(draw_layout.group_col, draw_layout.list_row,
                group_list_w, draw_layout.list_rows, grp_idx, grp_cur,
                grp_top, group_totals, group_icons, column == 0);
        }
        if (!single_column || column)
        {
            display_supply_list(&draw_layout, draw_layout.list_row,
                draw_layout.list_rows, entries, entry_cnt, entry_cur, entry_top,
                grp_idx[grp_cur], column, &draw_cols, compact_draw_names);
        }

        if (draw_layout.status_row != draw_layout.prompt_row)
        {
            describe_supply_group_status(grp_idx[grp_cur], status_buf,
                sizeof(status_buf));
            Term_erase(0, draw_layout.status_row, 255);
            if (status_buf[0] != '\0')
                Term_putstr(0, draw_layout.status_row, draw_layout.term_wid,
                    TERM_L_BLUE,
                    status_buf);
        }

        /* Bottom bar: grey text with white first letters */
        Term_erase(0, draw_layout.prompt_row, 255);
        if (steamdeck_controls_active()) {
            char recall_label[16];
            char use_label[16];
            char confirm_label[16];
            char drop_label[16];
            char back_label[16];
            char prompt_buf[160];

            /* Steam Deck UI: RS Right=recall, X=use, A=confirm, B=drop, Start=back */
            controller_prompt_label(steamdeck_info_key(), "RS Right", recall_label, sizeof(recall_label));
            controller_prompt_label(steamdeck_alt_action_key(), "X", use_label, sizeof(use_label));
            controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
            controller_prompt_label('f', "B", drop_label, sizeof(drop_label));
            controller_prompt_label(ESCAPE, "Start", back_label, sizeof(back_label));

            if (draw_layout.term_wid <= 50)
            {
                strnfmt(prompt_buf, sizeof(prompt_buf),
                    "D-pad move  [%s/%s] use  [%s] drop  [%s] back",
                    use_label, confirm_label, drop_label, back_label);
            }
            else
            {
                strnfmt(prompt_buf, sizeof(prompt_buf),
                    "D-pad move  [%s] recall  [%s/%s] use  [%s] drop  [%s] back",
                    recall_label, use_label, confirm_label, drop_label, back_label);
            }
            Term_putstr(0, draw_layout.prompt_row, draw_layout.term_wid,
                TERM_L_DARK, prompt_buf);
        } else {
            cptr prompt = (draw_layout.term_wid <= 50)
                ? "Dir move  u/Space use  d drop  Esc"
                : "Dir move  r/-> recall  u/Space use  d drop  Esc";
            Term_putstr(0, draw_layout.prompt_row, draw_layout.term_wid,
                TERM_SLATE, prompt);
        }

        if (!column)
            Term_gotoxy(draw_layout.group_col,
                draw_layout.list_row + (grp_cur - grp_top));
        else if (entry_cnt)
            Term_gotoxy(draw_layout.list_col,
                draw_layout.list_row + (entry_cur - entry_top));
        else
            Term_gotoxy(draw_layout.group_col,
                draw_layout.list_row + (grp_cur - grp_top));

        char ch = inkey();
        if (steamdeck_controls_active() && ch == 'f')
            ch = 'd';

        if ((ch == '\r' || ch == '\n' || (steamdeck_controls_active() && ch == steamdeck_confirm_key())) && column && entry_cnt)
        {
            if (forced_action == SUPPLY_MENU_ACTION_USE)
                ch = 'u';
            else if (forced_action == SUPPLY_MENU_ACTION_DROP)
                ch = 'd';
        }

        switch (ch)
        {
        case ESCAPE:
            flag = true;
            break;

        case 'R':
        case 'r':
        case 'X':
        case 'x':
        case '6':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                if (supplies_menu_recall_entry(&entries[entry_cur]))
                    redraw = true;
            }
            break;

        case 'u':
        case 'U':
        case ' ':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool handled = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if (entry->item_idx == SUPPLIES_INDEX && entry->supply_idx >= 0)
                {
                    handled = supplies_menu_use_entry(entry);
                }
                else if (entry->equip_idx == INVEN_LITE && entry->equipped)
                {
                    msg_print("That light source is already equipped.");
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    object_type* o_ptr = &inventory[entry->item_idx];

                    switch (o_ptr->tval)
                    {
                    case TV_FOOD:
                        do_cmd_eat_food(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_POTION:
                        do_cmd_quaff_potion(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_STAFF:
                        do_cmd_activate_staff(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_GEM:
                        do_cmd_use_gem(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_FLASK:
                        do_cmd_refuel_lamp(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_LIGHT:
                        do_cmd_wield(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    default:
                        bell("Cannot use that item here!");
                        break;
                    }

                    if (handled)
                        handle_stuff();
                }
                else
                {
                    bell("You do not have any of that item.");
                    msg_print("You do not have any of that item.");
                }

                if (handled)
                {
                    acted = true;
                    redraw = true;
                    refresh_after_close = true;
                    if (hotkey_mode || forced_action == SUPPLY_MENU_ACTION_USE)
                        flag = true;
                }
            }
            break;

        case 'd':
        case 'D':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool dropped = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if (entry->item_idx == SUPPLIES_INDEX && entry->supply_idx >= 0)
                {
                    dropped = supplies_menu_drop_entry(entry);
                }
                else if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
                {
                    do_cmd_drop_item_by_index(entry->equip_idx);
                    dropped = true;
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    do_cmd_drop_item_by_index(entry->item_idx);
                    dropped = true;
                }
                else
                {
                    bell("Nothing to drop here.");
                    msg_print("Nothing to drop here.");
                }

                if (dropped)
                {
                    acted = true;
                    redraw = true;
                    handle_stuff();
                    refresh_after_close = true;
                    if (hotkey_mode || forced_action == SUPPLY_MENU_ACTION_DROP)
                        flag = true;
                }
            }
            break;

        default:
            browser_cursor_with_rows(ch, &column, &grp_cur, grp_cnt, &entry_cur,
                entry_cnt, layout.list_rows, true);
            break;
        }
    }

    mem_free_null(entries);
    (void)Term_set_extra_cursor(false, 0, 0, false);
    screen_load();

    if (refresh_after_close)
    {
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        Term_fresh();
    }

    return acted;
}

void do_cmd_knowledge_objects(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_OBJECTS);
}

/*
 * Display kill counts
 */
void do_cmd_knowledge_kills(void)
{
    int n, i;

    SDL_IOStream* fff;

    char file_name[1024];

    u16b* who;
    //	u16b why = 4;

    /* Temporary file */
    fff = sdl_fopen_temp(file_name, sizeof(file_name));

    /* Failure */
    if (!fff)
        return;

    /* Allocate the "who" array */
    who = mem_alloc_array(z_info->r_max, u16b);

    /* Collect matching monsters */
    for (n = 0, i = 1; i < z_info->r_max - 1; i++)
    {
        // monster_race *r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Require non-unique monsters */
        // if (r_ptr->flags1 & RF1_UNIQUE) continue;

        /* Collect "appropriate" monsters */
        if (l_ptr->pkills > 0)
            who[n++] = i;
    }

    /* Select the sort method */
    // ang_sort_comp = ang_sort_comp_hook;
    // ang_sort_swap = ang_sort_swap_hook;

    /* Sort by kills (and level) */
    // ang_sort(who, &why, n);

    /* Print the monsters (highest kill counts first) */
    for (i = n - 1; i >= 0; i--)
    {
        monster_race* r_ptr = &r_info[who[i]];
        monster_lore* l_ptr = &l_list[who[i]];

        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /* Print a message */
            SDL_IOprintf(fff, "         %-40s\n", (r_name + r_ptr->name));
        }
        else
        {
            /* Print a message */
            SDL_IOprintf(
                fff, "  %5d  %-40s\n", l_ptr->pkills, (r_name + r_ptr->name));
        }
    }

    /* Free the "who" array */
    mem_free_null(who);

    /* Close the file */
    sdl_fclose(fff);

    /* Display the file contents */
    show_file(file_name, "Kill counts", 0);

    /* Remove the file */
    fd_kill(file_name);
}

/*
 * Interact with "knowledge"
 */
void do_cmd_knowledge(void)
{
    char ch;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening knowledge menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Interact until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Ask for a choice */
        prt("Display current knowledge", 2, 0);

        /* Give some choices */
        prt("(1) Display known lore browser", 4, 5);
        prt("(2) Display supplies overview", 5, 5);
        prt("(3) Display names of the fallen", 6, 5);
        prt("(4) Display kill counts", 7, 5);

        /*allow the player to see the notes taken if that option is selected*/
        c_put_str(TERM_WHITE, "(5) Display character notes file", 8, 5);
        prt("(6) Display oath status", 9, 5);

        /* Prompt */
        prt("Command: ", 11, 0);

        /* Prompt */
        ch = inkey();

        /* Done */
        if (ch == ESCAPE)
            break;

        /* Known lore browser */
        if (ch == '1')
        {
            do_cmd_knowledge_browser_page(g_knowledge_last_page);
        }

        /* Scores */
        else if (ch == '2')
        {
            do_cmd_knowledge_supplies(NULL);
        }

        /* Scores */
        else if (ch == '3')
        {
            show_scores_interactive(true);
        }

        /* Kill counts */
        else if (ch == '4')
        {
            do_cmd_knowledge_kills();
        }

        /* Notes file, if one exists */
        else if (ch == '5')
        {
            /* Spawn */
            do_cmd_knowledge_notes();
        }

        /* Oath status */
        else if (ch == '6')
        {
            do_cmd_knowledge_oaths();
        }

        /* Unknown option */
        else
        {
            bell("Illegal command for knowledge!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();
}

/*
 * Determines the direction from the player and writes it as text into a buffer
 * of at least size 10.
 */
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
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
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
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
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

static void redraw_inven_equip_subwindows(void)
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
        if (!(flags & (PW_INVEN | PW_EQUIP)))
            continue;

        Term_activate(angband_term[j]);

        if (flags & PW_INVEN)
            display_inven();
        if (flags & PW_EQUIP)
            display_equip();

        Term_fresh();
        Term_activate(old);
    }
}

static void redraw_monster_subwindows(void)
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
    if (!dest_sz) return;
    dest[0] = 0;

    if (!src) return;

    int src_len = (int)strlen(src);
    if (max_len < 1)
    {
        log_trace("sidebar_compact_name: max_len < 1 for src='%s'", src);
        return;
    }

    if (src_len <= max_len)
    {
        strnfmt(dest, dest_sz, "%s", src);
        log_trace("sidebar_compact_name: no shortening needed src='%s' len=%d max=%d", src, src_len, max_len);
        return;
    }

    int stats_pos = sidebar_find_stats_pos(src);
    log_trace("sidebar_compact_name: shortening src='%s' len=%d max=%d stats_pos=%d", src, src_len, max_len, stats_pos);

    if (stats_pos < 0)
    {
        strnfmt(dest, dest_sz, "%.*s", max_len, src);
        sidebar_trim_spaces(dest);
        log_trace("sidebar_compact_name: no stats segment, result='%s'", dest);
        return;
    }

    int stats_len = src_len - stats_pos;
    
    /* If stats are very long and would fill the whole space,
     * prioritize showing at least SOME of the base name rather than stats-only.
     */
    if (stats_len >= max_len)
    {
        /* Try to show at least a portion of the base name, even if truncated */
        int base_space = max_len / 2; /* Give half space to name */
        if (base_space < 3) base_space = 3; /* Minimum name chars */
        if (base_space > stats_pos) base_space = stats_pos; /* Don't exceed available name */
        
        int stats_space = max_len - base_space;
        if (stats_space < 3) stats_space = 3; /* Minimum stats chars */
        
        /* Extract truncated base name */
        char base_truncated[64];
        strnfmt(base_truncated, sizeof(base_truncated), "%.*s", base_space, src);
        sidebar_trim_spaces(base_truncated);
        
        /* Extract beginning of stats */
        char stats_truncated[64];
        strnfmt(stats_truncated, sizeof(stats_truncated), "%.*s", stats_space, src + stats_pos);
        
        /* Combine them */
        if (base_truncated[0])
        {
            strnfmt(dest, dest_sz, "%s %s", base_truncated, stats_truncated);
        }
        else
        {
            strnfmt(dest, dest_sz, "%s", stats_truncated);
        }
        sidebar_trim_spaces(dest);
        log_trace("sidebar_compact_name: long stats, showing truncated name+stats result='%s'", dest);
        return;
    }

    int base_space = max_len - stats_len;
    if (base_space < 0) base_space = 0;

    char base_full[128];
    char base_compact[128];
    base_full[0] = 0;
    base_compact[0] = 0;

    if (stats_pos > 0)
    {
        strnfmt(base_full, sizeof(base_full), "%.*s", stats_pos, src);
        sidebar_trim_spaces(base_full);
    }

    if (base_space > 0 && base_full[0])
    {
        int base_full_len = (int)strlen(base_full);
        if (base_full_len <= base_space)
        {
            SDL_strlcpy(base_compact, base_full, sizeof(base_compact));
        }
        else
        {
            const char* word_start[16];
            int word_len[16];
            int word_count = 0;
            const char* p = base_full;

            while (*p && word_count < 16)
            {
                while (*p && isspace((unsigned char)*p))
                    ++p;
                if (!*p)
                    break;

                word_start[word_count] = p;
                const char* q = p;
                while (*q && !isspace((unsigned char)*q))
                    ++q;
                word_len[word_count] = (int)(q - p);
                ++word_count;
                p = q;
            }

            int remaining = base_space;
            bool first_word = true;

            for (int i = 0; i < word_count && remaining > 0; ++i)
            {
                int needed_space = first_word ? 0 : 1;
                if (remaining <= needed_space)
                    break;

                if (!first_word)
                {
                    SDL_strlcat(base_compact, " ", sizeof(base_compact));
                    --remaining;
                }

                int take = word_len[i];
                if (take > remaining)
                {
                    if (first_word)
                    {
                        take = remaining;
                        if (take > 0)
                        {
                            char temp[64];
                            strnfmt(temp, sizeof(temp), "%.*s", take, word_start[i]);
                            SDL_strlcat(base_compact, temp, sizeof(base_compact));
                            remaining -= take;
                        }
                    }
                    else if (remaining > 1)
                    {
                        char temp[64];
                        int partial = remaining;
                        strnfmt(temp, sizeof(temp), "%.*s", partial, word_start[i]);
                        SDL_strlcat(base_compact, temp, sizeof(base_compact));
                        remaining = 0;
                    }
                    else
                    {
                        size_t len = strlen(base_compact);
                        if (len && base_compact[len - 1] == ' ')
                            base_compact[len - 1] = '\0';
                        break;
                    }
                }
                else
                {
                    char temp[64];
                    strnfmt(temp, sizeof(temp), "%.*s", take, word_start[i]);
                    SDL_strlcat(base_compact, temp, sizeof(base_compact));
                    remaining -= take;
                }

                first_word = false;
            }

            sidebar_trim_spaces(base_compact);

            if (!base_compact[0] && base_space > 0)
            {
                int take = (base_space < base_full_len) ? base_space : base_full_len;
                strnfmt(base_compact, sizeof(base_compact), "%.*s", take, base_full);
                sidebar_trim_spaces(base_compact);
            }
        }
    }

    dest[0] = 0;
    if (base_compact[0])
    {
        SDL_strlcpy(dest, base_compact, dest_sz);
        size_t len = strlen(dest);
        if (len && dest[len - 1] != ' ')
            SDL_strlcat(dest, " ", dest_sz);
    }

    SDL_strlcat(dest, src + stats_pos, dest_sz);
    sidebar_trim_spaces(dest);
    log_trace("sidebar_compact_name: combined result='%s'", dest);
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
    return Term && ((Term->hgt <= 18) || (Term->wid <= 60));
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
    if (!buf || !buflen)
        return;

    if (!text)
        text = "";

    if (max_chars <= 0)
    {
        buf[0] = '\0';
        return;
    }

    if ((int)strlen(text) <= max_chars)
        SDL_strlcpy(buf, text, buflen);
    else if (max_chars <= 3)
        strnfmt(buf, buflen, "%.*s", max_chars, text);
    else
        strnfmt(buf, buflen, "%.*s...", max_chars - 3, text);
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
            int hp_len = 0;
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

            if (m_ptr->maxhp > 0)
                hp_len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;
            hp_len = MIN(MAX(hp_len, 0), 8);

            if (m_ptr->confused && m_ptr->stunned)
                strncpy(hp_bar, "cscscscs", hp_len);
            else if (m_ptr->confused)
                strncpy(hp_bar, "cccccccc", hp_len);
            else if (m_ptr->stunned)
                strncpy(hp_bar, "ssssssss", hp_len);
            else
                strncpy(hp_bar, "********", hp_len);
            hp_bar[hp_len] = '\0';

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
        return true;
    }

    first_row = 0;
    last_row = unified_sidebar_compact_last_row();
    rows = last_row - first_row + 1;
    if (rows <= 0)
    {
        (void)Term_set_extra_cursor(false, 0, 0, false);
        return true;
    }

    (void)Term_set_extra_cursor(false, 0, 0, false);

    Term_erase(0, 0, 255);

    max_entries = MAX(1, mon_max + o_max);
    entries = mem_alloc_array(max_entries, unified_sidebar_compact_entry);
    entry_count = unified_sidebar_compact_build_entries(state, entries,
        max_entries);

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
        byte text_attr = entry->text_attr;
        int text_len = (int)strlen(entry->text);

        if (Term && text_len > Term->wid - text_col)
            text_len = Term->wid - text_col;
        if (text_len < 0)
            text_len = 0;

        c_put_str(entry->symbol_attr, entry->symbol, row, pictogram_col);
        if (use_bigtile)
            Term_putch(pictogram_col + 1, row, 255, -1);

        Term_putstr(text_col, row, text_len, text_attr, entry->text);

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

/*
 * Show unified sidebar with monsters and objects
 */
void show_unified_sidebar(unified_look_state* state)
{
    if (show_unified_sidebar_compact(state))
        return;

    int sidebar_col = 0; /* Left side of screen - column 0 */
    int line = 1;
    int i;
    int monster_count = 0;
    int object_count = 0;
    char clear_line[256];
    int clear_width;
    char entity_char[2];
    entity_char[1] = '\0';
    static int previous_line_count = 0; /* Track previous display size */
    static int prev_name_len[256];
    const int prev_array_capacity = (int)(sizeof(prev_name_len) / sizeof(prev_name_len[0]));
    bool has_sidebar_selection;

    
    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_display_line = term_hgt - 2; /* Leave space for bottom line */
    
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
    
    /* Prepare clearing string */
    clear_width = Term->wid - (sidebar_col - 1);
    if (clear_width > 255) clear_width = 255;
    memset(clear_line, ' ', clear_width);
    clear_line[clear_width] = '\0';
    
    log_trace("show_unified_sidebar: previous_line_count=%d, term_hgt=%d, max_display_line=%d", 
              previous_line_count, term_hgt, max_display_line);
    log_trace("show_unified_sidebar: sidebar_col=%d, Term->wid=%d, clear_start=%d, clear_width=%d", 
              sidebar_col, Term->wid, sidebar_col - 1, clear_width);
    log_trace("show_unified_sidebar: show_monsters=%d, show_objects=%d", 
              state->show_monsters ? 1 : 0, state->show_objects ? 1 : 0);

    (void)Term_set_extra_cursor(false, 0, 0, false);

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
        previous_line_count = 0;
        memset(prev_name_len, 0, sizeof(prev_name_len));
        return;
    }

    has_sidebar_selection = (state->selected_entity >= 0)
        && (state->in_sidebar_mode || (state->look_mode == 0));
    
    /* Don't clear anything - let screen_save/screen_load handle restoration */
    log_trace("show_unified_sidebar: skipping clear - letting screen management handle it");
    
    /* Show monsters section */
    if (state->show_monsters)
    {
        log_trace("show_unified_sidebar: displaying MONSTERS header at line %d", line);
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
            
            /* Create HP bar with asterisks */
            int hp_len = 0;
            if (m_ptr->maxhp > 0) {
                hp_len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;
            }
            char hp_bar[10];
            
            /* Build health bar with status indicators */
            if (m_ptr->confused && m_ptr->stunned)
            {
                strncpy(hp_bar, "cscscscs", hp_len);
            }
            else if (m_ptr->confused)
            {
                strncpy(hp_bar, "cccccccc", hp_len);
            }
            else if (m_ptr->stunned)
            {
                strncpy(hp_bar, "ssssssss", hp_len);
            }
            else
            {
                strncpy(hp_bar, "********", hp_len);
            }
            hp_bar[hp_len] = '\0';
            
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
            int max_name_len = available_width - hp_display_len - morale_display_len;
            if (max_name_len < 4) max_name_len = 4;
            if (max_name_len > (int)sizeof(display_name) - hp_display_len - morale_display_len - 1)
                max_name_len = (int)sizeof(display_name) - hp_display_len - morale_display_len - 1;
            
            /* Truncate monster name if needed */
            char truncated_name[80];
            memset(truncated_name, 0, sizeof(truncated_name));
            SDL_strlcpy(truncated_name, m_name, sizeof(truncated_name));
            if (strlen(truncated_name) > (size_t)max_name_len) {
                truncated_name[max_name_len] = '\0';
            }
            
            /* Build complete display string: name + health (without morale) */
            SDL_strlcpy(display_name, truncated_name, sizeof(display_name));
            SDL_strlcat(display_name, hp_display, sizeof(display_name));
            
            int name_hp_len = (int)strlen(display_name);
            int total_span = name_hp_len + morale_display_len;
            if (use_bigtile)
            {
                const int min_sidebar_span = 13;
                if (total_span < min_sidebar_span)
                {
                    int pad_needed = min_sidebar_span - total_span;

                    while (pad_needed > 0 && name_hp_len + 1 < (int)sizeof(display_name))
                    {
                        display_name[name_hp_len++] = ' ';
                        pad_needed--;
                    }
                    display_name[name_hp_len] = '\0';
                    total_span = name_hp_len + morale_display_len;

                    while (pad_needed > 0 && morale_display_len + 1 < (int)sizeof(morale_display))
                    {
                        morale_display[morale_display_len++] = ' ';
                        pad_needed--;
                    }
                    morale_display[morale_display_len] = '\0';
                    total_span = name_hp_len + morale_display_len;
                }

                if ((total_span % 2) == 0)
                {
                    if (morale_display_len + 1 < (int)sizeof(morale_display))
                    {
                        morale_display[morale_display_len++] = ' ';
                        morale_display[morale_display_len] = '\0';
                    }
                    else if (name_hp_len + 1 < (int)sizeof(display_name))
                    {
                        display_name[name_hp_len++] = ' ';
                        display_name[name_hp_len] = '\0';
                    }
                    total_span = name_hp_len + morale_display_len;
                }
            }
            
            /* Calculate column for morale display */
            int morale_col = name_col + name_hp_len;
            
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
                
                /* Display selected row in its normal colors; the tile frame marks selection. */
                Term_putstr(name_col, line, name_hp_len, TERM_WHITE, display_name);
                Term_putstr(morale_col, line, morale_display_len, morale_color, morale_display);
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
                Term_putstr(name_col, line, name_hp_len, TERM_WHITE, display_name);
                
                /* Display morale in its proper color */
                Term_putstr(morale_col, line, morale_display_len, morale_color, morale_display);
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
            int original_name_len = (int)strlen(name_source);
            bool shortened = (original_name_len != final_name_len) || (original_name_len > max_name_len);
            log_trace("sidebar object: idx=%d name='%s' compact='%s' color=%d orig_len=%d compact_len=%d max_len=%d name_col=%d weight_len=%d shortened=%d",
                entry->o_idx, name_source, display_name, base_color, original_name_len, final_name_len, max_name_len, name_col, weight_len, shortened ? 1 : 0);

            if (use_bigtile)
            {
                const int min_sidebar_span = 13;
                if (final_name_len < min_sidebar_span)
                {
                    int pad_needed = min_sidebar_span - final_name_len;
                    while (pad_needed > 0 && final_name_len + 1 < (int)sizeof(display_name))
                    {
                        display_name[final_name_len++] = ' ';
                        pad_needed--;
                    }
                    display_name[final_name_len] = '\0';
                }

                if ((final_name_len % 2) == 0 && (final_name_len + 1 < (int)sizeof(display_name)))
                {
                    display_name[final_name_len++] = ' ';
                    display_name[final_name_len] = '\0';
                }
            }

            int row_index = line;
            if (row_index < 0) row_index = 0;
            if (row_index >= prev_array_capacity) row_index = prev_array_capacity - 1;

            int old_name_len = prev_name_len[row_index];
            if (old_name_len > final_name_len)
            {
                int diff = old_name_len - final_name_len;
                if (diff > 0)
                {
                    char blank[128];
                    if (diff >= (int)sizeof(blank)) diff = (int)sizeof(blank) - 1;
                    memset(blank, ' ', diff);
                    blank[diff] = '\0';
                    Term_putstr(name_col + final_name_len, line, diff, TERM_WHITE, blank);
                }
            }

            bool highlight_this_object = (has_sidebar_selection
                && (state->selected_entity == (object_start + object_count)));

            byte name_attr = base_color;

            if (highlight_this_object)
            {
                log_trace("Highlighting object %d at (%d,%d)", object_start + object_count, entry->y, entry->x);

                Term_erase(pictogram_col, line, 2);
                
                c_put_str(object_attr(o_ptr), entity_char, line, pictogram_col);
                if (use_bigtile)
                {
                    Term_putch(pictogram_col + 1, line, 255, -1);
                }
                
                Term_putstr(name_col, line, final_name_len, name_attr, display_name);
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
                
                Term_putstr(name_col, line, final_name_len, name_attr, display_name);
            }

            prev_name_len[row_index] = final_name_len;

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
