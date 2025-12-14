/* File: cmd4.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
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

struct supply_list_entry
{
    int item_idx;   /* First inventory slot containing this kind */
    int k_idx;      /* Object kind index */
    int total;      /* Total quantity across the pack */
    int supply_idx; /* Index inside the supply cache (-1 if not present) */
};

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
    if (!entry || entry->supply_idx < 0)
        return false;

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
    case TV_GEM:
        do_cmd_activate_staff(o_ptr, SUPPLIES_INDEX);
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
    if (!entry || entry->supply_idx < 0)
        return false;

    object_type* o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    int max_amt = (o_ptr->tval == TV_GEM) ? supplies_entry_units(entry->supply_idx) : o_ptr->number;
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

static cptr supply_group_text[SUPPLY_GROUP_MAX + 1] = {
    "Herbs",
    "Potions",
    "Gems",
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
void do_cmd_character_sheet(void)
{
    char ch;

    int mode = 0;

    /* Clear any active banner before opening character sheet */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Forever */
    while (1)
    {
        /* Display the player */
        display_player(mode);

        /* Prompt - use monospace to ensure correct column alignment */
        /* With story font, proportional widths make column positions unreliable */
        
        /* Print the full command menu in base color */
        /* Prompt - render based on character sheet font setting */
        if (story_character_enabled()) {
            sdl_story_font_enable();
            /* Story font - use more spacing for readability */
            Term_putstr(1, 23, -1, TERM_L_WHITE,
                "n-notes     s-story     f-file     a-abilities     c-curses     i-increase     ?-help     ESC");
        } else {
            /* Mono font - use less spacing for compact display */
            Term_putstr(1, 23, -1, TERM_L_WHITE,
                "n-notes  s-story  f-file  a-abilities  c-curses  i-increase  ?-help  ESC");
        }

        Term_fresh();  /* Render commands */

        if (story_character_enabled()) {
            sdl_story_font_disable();
        }

        /* Query */
        ch = inkey();

        /* Exit */
        if (ch == ESCAPE)
            break;
        if ((ch == '\r') || (ch == '\n') || (ch == 'q') || (ch == 'Q'))
            break;

        /* Increase skills */
        if (ch == 'i')
        {
            gain_skills();
            /* Force redraw after skill changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* Show notes */
        else if ((ch == 'n') || (ch == ' '))
        {
            do_cmd_knowledge_notes();
        }

        /* Change name */
        else if (ch == 's')
        {
            print_metarun_stats();
        }

        /* Curses Menu */
        else if (ch == 'c')
        {
            dbg_show_active_flags();
        }

        /* Abilities */
        else if ((ch == 'a') || (ch == '\t'))
        {
            (void)do_cmd_ability_screen();
            /* Force redraw after ability changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* File dump */
        else if (ch == 'f')
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

        /* Tutorial */
        else if (ch == '?')
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
    screen_load();

    /* Force redraw after screen restore if skills/abilities were changed */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
    handle_stuff();
}

#define COL_SKILL 2
#define COL_ABILITY 16
#define COL_DESCRIPTION 41

/* ------------------------------------------------------------------
 * add_random_curse()
 *   � Marks the item cursed
 *   � Gives it random negative modifiers
 *   Compatible with SIL-QH object_type (no flags1/2/3 fields)
 * ------------------------------------------------------------------ */
void add_random_curse(object_type *o_ptr)
{
    /* 1. make it show up as {cursed} right away */
    o_ptr->ident |= IDENT_CURSED;

    /* 2. negative pval / attack / evasion */
    if (o_ptr->pval > 0)  o_ptr->pval = -(rand_int(3) + 1); /* �1 � �3 */
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

bool prereqs(int skilltype, int abilitynum)
{
    int i;
    ability_type* b_ptr;

    b_ptr = &b_info[ability_index(skilltype, abilitynum)];

    if (p_ptr->skill_base[skilltype] < b_ptr->level)
    {
        return (false);
    }

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

/*
 * Display the available songs (modelled on show_inven) with optional highlighting.
 */
void show_songs_with_highlight(int highlight)
{
    int i, j, k = 0;
    int current_line = 0;

    int col = 26;

    char tmp_val[80];

    int out_index[24];
    char out_desc[24][80];

    /* Display the songs */
    for (k = 0, i = 0; i < SNG_MAX; i++)
    {
        /* Skip Woven Themes (not a singable song) */
        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
            continue;

        /* Is this song acceptable? */
        if (!p_ptr->active_ability[S_SNG][i])
            continue;

        /* Save the index */
        out_index[k] = i;

        /* Save the song name */
        SDL_strlcpy(out_desc[k],
            b_name + (&b_info[ability_index(S_SNG, i)])->name,
            sizeof(out_desc[0]));

        /* Advance to next "line" */
        k++;
    }

    // add a line for the 'stop singing' command

    /* Clear the line */
    prt("", 1, col - 2);

    /* Clear the line with the (possibly indented) index */
    put_str("s)", 1, col);

    /* Display the entry itself - highlight if selected */
    if (highlight == current_line)
        c_put_str(TERM_L_BLUE, "Stop Singing", 1, col + 3);
    else
        c_put_str(TERM_SLATE, "Stop Singing", 1, col + 3);
    current_line++;

    /* Output each entry */
    for (j = 0; j < k; j++)
    {
        /* Get the index */
        i = out_index[j];

        /* Clear the line */
        prt("", j + 2, col - 2);

        /* Prepare an index --(-- */
        sprintf(tmp_val, "%c)", (char)('a' + i));

        /* Clear the line with the (possibly indented) index */
        put_str(tmp_val, j + 2, col);

        /* Display the entry itself - highlight if selected */
        if (highlight == current_line)
            c_put_str(TERM_L_BLUE, out_desc[j], j + 2, col + 3);
        else
            c_put_str(TERM_L_WHITE, out_desc[j], j + 2, col + 3);
        current_line++;
    }

    // add a line for the 'exchange themes' command
    if (p_ptr->song2 != SNG_NOTHING)
    {
        /* Clear the line */
        prt("", j + 2, col - 2);

        /* Clear the line with the (possibly indented) index */
        put_str("x)", j + 2, col);

        /* Display the entry itself - highlight if selected */
        if (highlight == current_line)
            c_put_str(TERM_L_BLUE, "Exchange themes", j + 2, col + 3);
        else
            c_put_str(TERM_L_BLUE, "Exchange themes", j + 2, col + 3);

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

    int options = 0;
    int song_choice = -1;
    int highlight = 0; // Add highlight tracking

    char out_val[80];
    char tmp_val[80];

    char which;

    log_debug("Player opening song selection menu");

    // Check for song lockout timer first
    if (p_ptr->song_lockout_timer > 0)
    {
        msg_format("You cannot sing for %d more turn%s.", 
            p_ptr->song_lockout_timer,
            (p_ptr->song_lockout_timer == 1) ? "" : "s");
        return;
    }

    // count the abilities
    for (i = 0; i < SNG_MAX; i++)
    {
        /* Skip Woven Themes (not a singable song) */
        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
            continue;

        // keep track of the number of options and final song
        if (p_ptr->active_ability[S_SNG][i])
        {
            options += 1;
        }
    }

    // abort if you know no songs
    if (options == 0)
    {
        log_trace("No songs available - player knows no songs of power");
        msg_print("You do not know any songs of power.");
        return;
    }
    
    log_debug("Player has %d songs available", options);

    /* Flush the prompt */
    Term_fresh();

    /* Option to always show a list */
    if (auto_display_lists)
    {
        p_ptr->command_see = true;
    }

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
        sprintf(out_val, "Songs: s");

        // count the abilities
        for (i = 0; i < SNG_MAX; i++)
        {
            /* Skip Woven Themes (not a singable song) */
            if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                continue;

            // keep track of the number of options
            if (p_ptr->active_ability[S_SNG][i])
            {
                SDL_strlcat(out_val, ",", sizeof(out_val));
                sprintf(tmp_val, "%c", (char)'a' + i);

                /* Append */
                SDL_strlcat(out_val, tmp_val, sizeof(out_val));
            }
        }

        // add an 'x' option if using woven themes
        if (p_ptr->song2 != SNG_NOTHING)
        {
            /* Append */
            SDL_strlcat(out_val, ",x", sizeof(out_val));
        }

        /* Indicate ability to "view" */
        if (!p_ptr->command_see)
            SDL_strlcat(out_val, ", * to see", sizeof(out_val));

        /* Build the prompt */
        strnfmt(tmp_val, sizeof(tmp_val), "(%s) Sing which song: ", out_val);

        /* Show the prompt */
        prt(tmp_val, 0, 0);

        /* Get a key */
        which = inkey();

        /* Parse it */
        switch (which)
        {
        case ESCAPE:
        {
            log_trace("Song selection cancelled by player");
            done = true;
            break;
        }

        case '\r': // Enter - select highlighted item when menu is visible, otherwise exit
        {
            if (p_ptr->command_see)
            {
                // Convert highlight to appropriate song choice (same logic as '6' and Space keys)
                if (highlight == 0)
                {
                    song_choice = SNG_NOTHING; // Stop singing
                }
                else
                {
                    // Find the i-th available song
                    int song_count = 1;
                    for (i = 0; i < SNG_MAX; i++)
                    {
                        /* Skip Woven Themes (not a singable song) */
                        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                            continue;

                        if (p_ptr->active_ability[S_SNG][i])
                        {
                            if (song_count == highlight)
                            {
                                song_choice = i;
                                break;
                            }
                            song_count++;
                        }
                    }
                    // Check for exchange themes option
                    if (song_choice == -1 && p_ptr->song2 != SNG_NOTHING && highlight == song_count)
                    {
                        song_choice = SNG_EXCHANGE_THEMES;
                    }
                }
                
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
                // Convert highlight to appropriate song choice (same logic as '6' key)
                if (highlight == 0)
                {
                    song_choice = SNG_NOTHING; // Stop singing
                }
                else
                {
                    // Find the i-th available song
                    int song_count = 1;
                    for (i = 0; i < SNG_MAX; i++)
                    {
                        /* Skip Woven Themes (not a singable song) */
                        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                            continue;

                        if (p_ptr->active_ability[S_SNG][i])
                        {
                            if (song_count == highlight)
                            {
                                song_choice = i;
                                break;
                            }
                            song_count++;
                        }
                    }
                    // Check for exchange themes option
                    if (song_choice == -1 && p_ptr->song2 != SNG_NOTHING && highlight == song_count)
                    {
                        song_choice = SNG_EXCHANGE_THEMES;
                    }
                }
                
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
                // Get total available songs + stop singing + exchange themes
                int total_options = 1; // "Stop Singing"
                for (i = 0; i < SNG_MAX; i++)
                {
                    /* Skip Woven Themes (not a singable song) */
                    if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                        continue;

                    if (p_ptr->active_ability[S_SNG][i])
                        total_options++;
                }
                if (p_ptr->song2 != SNG_NOTHING)
                    total_options++; // "Exchange themes"

                highlight = (highlight + 1) % total_options;
            }
            break;
        }

        case '8': // Up arrow / scroll up
        {
            if (p_ptr->command_see)
            {
                // Get total available songs + stop singing + exchange themes
                int total_options = 1; // "Stop Singing"
                for (i = 0; i < SNG_MAX; i++)
                {
                    /* Skip Woven Themes (not a singable song) */
                    if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                        continue;

                    if (p_ptr->active_ability[S_SNG][i])
                        total_options++;
                }
                if (p_ptr->song2 != SNG_NOTHING)
                    total_options++; // "Exchange themes"

                highlight = (highlight - 1 + total_options) % total_options;
            }
            break;
        }

        case '6': // Right arrow / select highlighted
        {
            if (p_ptr->command_see)
            {
                // Convert highlight to appropriate song choice
                if (highlight == 0)
                {
                    song_choice = SNG_NOTHING; // Stop singing
                }
                else
                {
                    // Find the i-th available song
                    int song_count = 1;
                    for (i = 0; i < SNG_MAX; i++)
                    {
                        /* Skip Woven Themes (not a singable song) */
                        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                            continue;

                        if (p_ptr->active_ability[S_SNG][i])
                        {
                            if (song_count == highlight)
                            {
                                song_choice = i;
                                break;
                            }
                            song_count++;
                        }
                    }
                    // Check for exchange themes option
                    if (song_choice == -1 && p_ptr->song2 != SNG_NOTHING && highlight == song_count)
                    {
                        song_choice = SNG_EXCHANGE_THEMES;
                    }
                }
                
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
            if ((which >= 'a') && (which < 'a' + SNG_MAX))
            {
                song_choice = (int)which - 'a';
                
                /* Skip Woven Themes (not a singable song) */
                if (song_choice == SNG_WOVEN_THEMES || song_choice == SNG_GRA)
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
        if (song_choice != SNG_NOTHING)
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
    }
}

void wipe_screen_from(int col)
{
    int i;

    for (i = 1; i < SCREEN_HGT; i++)
    {
        Term_putstr(col, i, -1, TERM_WHITE,
            "                                                              "
            "                   ");
    }
}

int elf_bane_bonus(monster_type* m_ptr)
{
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return (0);
    else
        r_ptr = &r_info[m_ptr->r_idx];

    // Sil-x: a bit of a hack. Noldor and Sindar are coded as races 0 and 1 in
    // the races.txt file
    if ((r_ptr->flags2 & (RF2_ELFBANE))
        && ((p_ptr->prace == 0) || (p_ptr->prace == 1)))
    {
        // Dagohir must have killed between 32 and 63 elves
        return (5);
    }

    return (0);
}

#define BANE_TYPES 9

static u32b bane_flag[] = { 0L, RF3_ORC, RF3_WOLF, RF3_SPIDER, RF3_TROLL,
    RF3_UNDEAD, RF3_RAUKO, RF3_SERPENT, RF3_DRAGON };

char* bane_name[] = { "Nothing", "Orc", "Wolf", "Spider", "Troll", "Wraith",
    "Rauko", "Serpent", "Dragon" };

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

int spider_bane_bonus(void)
{
    if (bane_flag[p_ptr->bane_type] == RF3_SPIDER)
        return (bane_bonus_aux());
    else
        return (0);
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
    int i, k;

    int ch;
    int options;

    char buf[80];

    byte attr;

    // bane title
    Term_putstr(COL_DESCRIPTION, 2, -1, TERM_WHITE, "Enemy types");

    // clear the description area
    wipe_screen_from(COL_DESCRIPTION);

    // list the enemies
    for (i = 1; i < BANE_TYPES; i++)
    {
        k = bane_type_killed(i);

        // Determine the appropriate colour
        if (k >= 4)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
        }

        strnfmt(buf, 80, "%c) %s", (char)'a' + i - 1, bane_name[i]);
        Term_putstr(COL_DESCRIPTION, i + 3, -1, attr, buf);

        if (*highlight == i)
        {
            // highlight the label
            strnfmt(buf, 80, "%c)", (char)'a' + i - 1);
            Term_putstr(COL_DESCRIPTION, i + 3, -1, TERM_L_BLUE, buf);

            /* Indent output by 2 character, and wrap at column 70 */
            text_out_wrap = 79;
            text_out_indent = COL_DESCRIPTION;

            Term_gotoxy(text_out_indent, BANE_TYPES + 4);

            /* Information */
            if (k >= 4)
            {
                strnfmt(buf, 80, "You have slain %d of these foes.", k);
                text_out_to_screen(TERM_SLATE, buf);
            }
            else
            {
                strnfmt(buf, 80,
                    "You have slain %d of these foes,   and need to slay %d "
                    "more.",
                    k, 4 - k);
                text_out_to_screen(TERM_L_DARK, buf);
            }

            /* Reset text_out() vars */
            text_out_wrap = 0;
            text_out_indent = 0;
        }

        // keep track of the number of options
        options = i;
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(COL_DESCRIPTION, 3 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if ((ch >= 'a') && (ch <= (char)'a' + options - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        bane_menu(highlight);

        return (*highlight);
    }

    if ((ch >= 'A') && (ch <= (char)'A' + options - 1))
    {
        *highlight = (int)ch - 'A' + 1;
        return (*highlight);
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
    {
        return (BANE_TYPES + 1);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
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
    "+2 Light Radius",
};

bool oath_invalid(int i) { return ((p_ptr->oaths_broken & oath_flag[i]) > 0); }

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

int oath_menu(int* highlight)
{
    int i, ch;
    int visible_count = 0;
    /* Support up to 16 oaths without realloc; actual used = z_info->oath_max */
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
    wipe_screen_from(COL_ABILITY);

    // Title in the abilities column
    Term_putstr(COL_ABILITY, 2, -1, TERM_WHITE, "Oaths");

    // Build visible oaths list and display them (1..z_info->oath_max-1)
    for (i = 1; z_info && i < z_info->oath_max; i++)
    {
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
        if (oath_invalid(i))
        {
            strnfmt(buf, 80, "%c) %s", (char)'a' + visible_count, oath_name[i]);
        }
        else
        {
            strnfmt(buf, 80, "%c) %s", (char)'a' + visible_count, oath_name[i]);
        }
        
        // Display in abilities column with proper spacing
        Term_putstr(COL_ABILITY, 4 + visible_count, -1, attr, buf);
        visible_count++;
    }

    // Display detailed description for highlighted oath in description column
    if (*highlight >= 1 && *highlight <= visible_count)
    {
        int oath_idx = visible_oaths[*highlight - 1];
        
        // Clear description area first
        wipe_screen_from(COL_DESCRIPTION);
        
        // Oath title
        Term_putstr(COL_DESCRIPTION, 2, -1, TERM_WHITE, "Oath Details");
        
        if (oath_invalid(oath_idx))
        {
            // Menacing text for broken oaths
            Term_putstr(COL_DESCRIPTION, 4, -1, TERM_L_RED, "OATH BROKEN");
            Term_putstr(COL_DESCRIPTION, 6, -1, TERM_RED, "\"Thy oath lies shattered,");
            Term_putstr(COL_DESCRIPTION, 7, -1, TERM_RED, " thy word worthless as dust.\"");
            Term_putstr(COL_DESCRIPTION, 9, -1, TERM_L_RED, "\"No Valar shall hear thy voice,");
            Term_putstr(COL_DESCRIPTION, 10, -1, TERM_L_RED, " no light shall guide thy path.\"");
            Term_putstr(COL_DESCRIPTION, 12, -1, TERM_RED, "Forever marked as oathbreaker");
            Term_putstr(COL_DESCRIPTION, 13, -1, TERM_RED, "in this age.");
        }
        else
        {
            // Tolkien-themed quote
            Term_putstr(COL_DESCRIPTION, 4, -1, TERM_YELLOW, "Quote:");
            // Split long quotes across multiple lines
            char* quote = (oath_idx < (int)N_ELEMENTS(oath_tolkien_desc)) ? oath_tolkien_desc[oath_idx] : "";
            if (strlen(quote) > 35) // Description column width is ~38 chars
            {
                char line1[40], line2[40];
                int split_pos = 35;
                // Find good split point (space or punctuation)
                while (split_pos > 20 && quote[split_pos] != ' ' && quote[split_pos] != ',' && quote[split_pos] != ';')
                    split_pos--;
                
                strncpy(line1, quote, split_pos);
                line1[split_pos] = '\0';
                strcpy(line2, quote + split_pos + (quote[split_pos] == ' ' ? 1 : 0));
                
                Term_putstr(COL_DESCRIPTION, 5, -1, TERM_SLATE, line1);
                Term_putstr(COL_DESCRIPTION, 6, -1, TERM_SLATE, line2);
            }
            else
            {
                Term_putstr(COL_DESCRIPTION, 5, -1, TERM_SLATE, quote);
            }
            
            // Oath vow
            Term_putstr(COL_DESCRIPTION, 8, -1, TERM_WHITE, "Vow:");
            // Wrap long vows
            if (strlen(oath_desc1[oath_idx]) > 30)
            {
                char vow_line1[35], vow_line2[35];
                int vow_split = 30;
                while (vow_split > 15 && oath_desc1[oath_idx][vow_split] != ' ')
                    vow_split--;
                
                strncpy(vow_line1, oath_desc1[oath_idx], vow_split);
                vow_line1[vow_split] = '\0';
                strcpy(vow_line2, oath_desc1[oath_idx] + vow_split + 1);
                
                Term_putstr(COL_DESCRIPTION, 9, -1, TERM_SLATE, vow_line1);
                Term_putstr(COL_DESCRIPTION, 10, -1, TERM_SLATE, vow_line2);
            }
            else
            {
                Term_putstr(COL_DESCRIPTION, 9, -1, TERM_SLATE, oath_desc1[oath_idx]);
            }
            
            // Restriction
            Term_putstr(COL_DESCRIPTION, 12, -1, TERM_L_RED, "Restriction:");
            Term_putstr(COL_DESCRIPTION, 13, -1, TERM_L_RED, oath_desc2[oath_idx]);
            
            // Reward
            Term_putstr(COL_DESCRIPTION, 15, -1, TERM_L_GREEN, "Reward:");
            Term_putstr(COL_DESCRIPTION, 16, -1, TERM_L_GREEN, oath_reward[oath_idx]);
        }
        
        // Navigation instructions at bottom
        Term_putstr(COL_DESCRIPTION, 22, -1, TERM_SLATE, "2/8 - Navigate");
        Term_putstr(COL_DESCRIPTION, 23, -1, TERM_SLATE, "Enter - Select  ESC - Back");
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
    if ((ch >= 'a') && (ch < 'a' + visible_count))
    {
        *highlight = (int)ch - 'a' + 1;
        return oath_menu(highlight); // Recursive call to update display
    }

    /* Handle capital letter selection (A-Z) for immediate selection */
    if ((ch >= 'A') && (ch < 'A' + visible_count))
    {
        *highlight = (int)ch - 'A' + 1;
        return visible_oaths[*highlight - 1]; // Return actual oath index
    }

    /* ESC or 'q' - exit menu */
    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
    {
        /* Return a sentinel that's outside valid oath indices */
        return (z_info ? z_info->oath_max + 1 : OATH_TYPES + 1);
    }

    /* Enter or Space - select current highlighted oath */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
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

    // title
    Term_putstr(COL_SKILL, 2, -1, TERM_WHITE, "Skills");

    // list the skills
    for (i = 0; i < options; i++)
    {
        strnfmt(buf, 80, "%c) %s", (char)'a' + i, skill_names_full[i]);

        // Highlight the entire line if selected
        Term_putstr(COL_SKILL, i + 4, -1,
            (*highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);
    }

    // clear the abilities area
    wipe_screen_from(COL_ABILITY);

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(COL_SKILL, 3 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if ((ch >= 'a') && (ch <= (char)'a' + options - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        // relist the skills
    for (i = 0; i < options; i++)
        {
            strnfmt(buf, 80, "%c) %s", (char)'a' + i, skill_names_full[i]);

            Term_putstr(COL_SKILL, i + 4, -1,
                (*highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);
        }

        return (*highlight);
    }

    if ((ch >= 'A') && (ch <= (char)'A' + options - 1))
    {
        *highlight = (int)ch - 'A' + 1;

        // relist the skills
    for (i = 0; i < options; i++)
        {
            strnfmt(buf, 80, "%c) %s", (char)'a' + i, skill_names_full[i]);

            Term_putstr(COL_SKILL, i + 4, -1,
                (*highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);
        }

        return (*highlight);
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '\t'))
    {
        return (S_MAX + 1);  // Always return S_MAX + 1 to exit, regardless of options
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
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
    int i, j;

    ability_type* b_ptr;

    int ch;
    int visible_count = 0; // Count of actually visible abilities
    int visible_abilities[ABILITIES_MAX]; // Map display letters to ability numbers

    char buf[80];

    byte attr;

    // clear the abilities and description area
    wipe_screen_from(COL_ABILITY);

    // abilities title with color
    Term_putstr(COL_ABILITY, 1, -1, TERM_L_BLUE, "Abilities");

    // Add display counter for compact menu layout (avoids gaps from filtered abilities)
    int display_counter = 0;
    
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

        // Map this visible ability to its position
        visible_abilities[visible_count] = b_ptr->abilitynum;
        
        // Track first visible ability for highlight adjustment
        if (first_visible_ability == -1) {
            first_visible_ability = b_ptr->abilitynum;
        }

        // Determine the appropriate colour
        if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
        {
            if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            {
                if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                {
                    attr = TERM_WHITE;
                }
                else
                {
                    attr = TERM_RED;
                }
            }
            else
            {
                if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                {
                    attr = TERM_L_GREEN;
                }
                else
                {
                    attr = TERM_RED;
                }
            }
        }
        else
        {
            if (prereqs(skilltype, b_ptr->abilitynum))
                attr = TERM_SLATE;
            else
                attr = TERM_L_DARK;
        }

        if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
            && (p_ptr->bane_type > 0))
        {
            strnfmt(buf, 80, "%c) %s-%s", (char)'a' + visible_count,
                bane_name[p_ptr->bane_type], (b_name + b_ptr->name));
        }
        else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
            && (p_ptr->oath_type > 0))
        {
            strnfmt(buf, 80, "%c) %s: %s", (char)'a' + visible_count,
                (b_name + b_ptr->name), oath_name[p_ptr->oath_type]);
        }
        else
        {
            strnfmt(buf, 80, "%c) %s", (char)'a' + visible_count,
                (b_name + b_ptr->name));
        }
        
        /* Single column layout - starts at row 3 to maximize space */
        int display_row = display_counter + 3;
        
        Term_putstr(COL_ABILITY, display_row, -1, attr, buf);

        if (*highlight == b_ptr->abilitynum + 1)
        {
            // highlight the label with bright blue
            strnfmt(buf, 80, "%c)", (char)'a' + visible_count);
            Term_putstr(COL_ABILITY, display_row, -1, TERM_L_BLUE, buf);

            // print the description of the highlighted ability
            /* (ability_type::text is an offset, so it's always non-negative) */
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
                wipe_screen_from(COL_DESCRIPTION);
                
                /* Display ability name in description area with appropriate color */
                Term_putstr(COL_DESCRIPTION, 1, -1, TERM_YELLOW, b_name + b_ptr->name);
                
                /* Indent output by 2 character, and wrap at column 79 */
                text_out_wrap = 79;
                text_out_indent = COL_DESCRIPTION;

                /* Description starts at row 3 for more space */
                Term_gotoxy(text_out_indent, 3);
                
                if (use_death_message && description_text && description_text[0])
                {
                    /* Display Q: text in red for broken oaths */
                    text_out_to_screen(TERM_RED, description_text);
                }
                else
                {
                    /* Normal ability description in light white */
                    text_out_to_screen(TERM_L_WHITE, b_text + b_ptr->text);
                    
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
                }

                /* Reset text_out() vars */
                text_out_wrap = 0;
                text_out_indent = 0;
            }

            // print more info if you don't have the skill
            if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
            {
                int desc_row = 16;  /* Start prerequisites lower to give description more room (moved down 2 rows) */
                
                // print the prerequisites with color
                Term_putstr(COL_DESCRIPTION, desc_row, -1, TERM_YELLOW, "Prerequisites:");

                strnfmt(buf, 80, "%d skill points (you have %d)", b_ptr->level,
                    p_ptr->skill_base[skilltype]);

                /* Color based on whether requirement is met */
                if (b_ptr->level <= p_ptr->skill_base[skilltype])
                {
                    /* Print immediately below the 'Prerequisites:' line */
                    Term_putstr(COL_DESCRIPTION + 2, desc_row + 1, -1, TERM_L_GREEN, buf);
                }
                else
                {
                    Term_putstr(COL_DESCRIPTION + 2, desc_row + 1, -1, TERM_L_DARK, buf);
                }

                if (!p_ptr->active_ability[S_PER][PER_QUICK_STUDY])
                {
                    for (j = 0; j < b_ptr->prereqs; j++)
                    {
                        if (j == 0)
                        {
                            strnfmt(buf, 80, "%s",
                                b_name
                                    + (&b_info[ability_index(
                                           b_ptr->prereq_skilltype[j],
                                           b_ptr->prereq_abilitynum[j])])
                                          ->name);
                        }
                        else
                        {
                            strnfmt(buf, 80, "or %s",
                                b_name
                                    + (&b_info[ability_index(
                                           b_ptr->prereq_skilltype[j],
                                           b_ptr->prereq_abilitynum[j])])
                                          ->name);
                        }
                        
                        /* Color based on whether you have the prerequisite */
                        byte prereq_attr = TERM_L_DARK;
                        if (p_ptr->innate_ability[b_ptr->prereq_skilltype[j]]
                                                 [b_ptr->prereq_abilitynum[j]])
                        {
                            prereq_attr = TERM_L_GREEN;
                        }
                        
                        if (j == 0)
                        {
                            /* Print prerequisites immediately after the skill-points line */
                            Term_putstr(COL_DESCRIPTION + 2, desc_row + 2 + j, -1,
                                prereq_attr, buf);
                        }
                        else
                        {
                            Term_putstr(COL_DESCRIPTION + 5, desc_row + 2 + j, -1,
                                prereq_attr, buf);
                        }
                    }
                }
                else if (b_ptr->prereqs > 0)
                {
                    strnfmt(buf, 80, "Quick Study");
                    /* Quick Study prints a single line immediately after skill points */
                    Term_putstr(COL_DESCRIPTION + 2, desc_row + 2, -1, TERM_GREEN, buf);
                }

                if (skilltype == S_SPC)
                {
                    // Special abilities cannot be purchased; show as granted only
                }
                else if (prereqs(skilltype, b_ptr->abilitynum))
                {
                    // Normalize flag check to 0 or 1
                    int is_free = (c_info[p_ptr->pcharacter].flags & RHF_FREE) ? 1 : 0;
                    int unit_cost = 500 - 200 * is_free;

                    // Calculate base cost
                    int exp_cost = (abilities_in_skill(skilltype) + 1) * unit_cost;

                    // Subtract free abilities granted by affinity
                    exp_cost -= unit_cost * affinity_level(skilltype);

                    // For song abilities, also subtract minstrel bonus (uncapped)
                    if (skilltype == S_SNG)
                        exp_cost -= unit_cost * minstrel_level();

                    // Clamp to zero
                    if (exp_cost < 0)
                        exp_cost = 0;

                    // print the cost with color coding
                    /* Compute the row immediately after the last prerequisite/Quick Study line
                     * 'Prerequisites:' is at desc_row
                     * skill points are at desc_row + 1
                     * prerequisites (if any) start at desc_row + 2 and occupy b_ptr->prereqs lines
                     * Quick Study (if active) occupies one line at desc_row + 2
                     */
                    int extra_lines = 0;
                    if (!p_ptr->active_ability[S_PER][PER_QUICK_STUDY])
                    {
                        extra_lines = b_ptr->prereqs; /* may be 0 */
                    }
                    else if (b_ptr->prereqs > 0)
                    {
                        extra_lines = 1; /* Quick Study printed a single line */
                    }

                    desc_row = desc_row + 2 + extra_lines; /* next free row */
                    Term_putstr(COL_DESCRIPTION, desc_row, -1, TERM_YELLOW, "Current price:");

                    strnfmt(buf, 80, "%d experience (you have %d)", exp_cost,
                        p_ptr->new_exp);

                    /* Color based on whether you can afford it */
                    if (exp_cost <= p_ptr->new_exp)
                    {
                        /* Print immediately under 'Current price:' */
                        Term_putstr(COL_DESCRIPTION + 2, desc_row + 1, -1, TERM_L_GREEN, buf);
                    }
                    else
                    {
                        Term_putstr(COL_DESCRIPTION + 2, desc_row + 1, -1, TERM_L_DARK, buf);
                    }
                }
            }

            // if you have the ability and it is Bane...
            else if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
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
                
                Term_putstr(COL_DESCRIPTION, 10, -1, TERM_WHITE,
                    format("%s-Bane:", bane_name[p_ptr->bane_type]));
                Term_putstr(COL_DESCRIPTION, 12, -1, TERM_WHITE,
                    format("  %d slain, giving a %+d bonus", killed, current_bonus));
                    
                if (current_bonus == 0 && killed < 2) {
                    Term_putstr(COL_DESCRIPTION, 13, -1, TERM_SLATE,
                        format("  (next bonus at %d slain)", next_threshold));
                } else if (next_threshold <= 64) {  // Don't show if threshold is too high
                    Term_putstr(COL_DESCRIPTION, 13, -1, TERM_SLATE,
                        format("  (next bonus at %d slain)", next_threshold));
                }
            }
            else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
                && (p_ptr->oath_type > 0))
            {
                Term_putstr(COL_DESCRIPTION, 10, -1, TERM_WHITE, "Oath:");
                Term_putstr(COL_DESCRIPTION + 6, 10, -1, TERM_L_BLUE,
                    oath_name[p_ptr->oath_type]);

                /* Indent output by 2 character, and wrap at column 70 */
                text_out_wrap = 79;
                text_out_indent = COL_DESCRIPTION;

                /* History */
                Term_gotoxy(text_out_indent, 11);
                strnfmt(buf, 80, "You have sworn not to %s.",
                    oath_desc2[p_ptr->oath_type]);
                text_out_to_screen(TERM_L_WHITE, buf);

                /* Reset text_out() vars */
                text_out_wrap = 0;
                text_out_indent = 0;

                if (oath_invalid(p_ptr->oath_type))
                    Term_putstr(COL_DESCRIPTION, 14, -1, TERM_RED,
                        "You are an oathbreaker.");
                else
                    Term_putstr(COL_DESCRIPTION, 14, -1, TERM_WHITE,
                        format("Bonus: %s.", oath_reward[p_ptr->oath_type]));
            }
            // if you have the unique bane special ability
            else if ((skilltype == S_SPC) && (b_ptr->abilitynum == SPC_UNIQUE_BANE))
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
                
                Term_putstr(COL_DESCRIPTION, 10, -1, TERM_WHITE, "Unique Bane:");
                Term_putstr(COL_DESCRIPTION, 12, -1, TERM_WHITE,
                    format("  %d uniques slain, giving a %+d bonus", 
                           uniques_killed, current_bonus));
                           
                if (current_bonus == 0 && uniques_killed < 2) {
                    Term_putstr(COL_DESCRIPTION, 13, -1, TERM_SLATE,
                        format("  (next bonus at %d uniques)", next_threshold));
                } else if (next_threshold <= 64) {  // Don't show if threshold is too high
                    Term_putstr(COL_DESCRIPTION, 13, -1, TERM_SLATE,
                        format("  (next bonus at %d uniques)", next_threshold));
                }
            }
        }

        // increment display counter and visible count for next ability
        display_counter++;
        visible_count++;
    }

    /* Safety check: if no abilities are visible, show message and exit */
    if (visible_count == 0) {
        Term_putstr(COL_ABILITY, 4, -1, TERM_L_DARK, "No abilities available for this skill.");
        Term_fresh();
        inkey(); /* Wait for keypress */
        return (ABILITIES_MAX + 1); /* Return to skills menu */
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice - single column layout */
    int cursor_row = -1;
    int highlight_display_index = -1;
    
    /* Find the display index for the highlighted ability */
    for (i = 0; i < visible_count; i++)
    {
        if (visible_abilities[i] == *highlight - 1)
        {
            highlight_display_index = i;
            break;
        }
    }
    
    if (highlight_display_index >= 0)
    {
        cursor_row = 3 + highlight_display_index;
        Term_gotoxy(COL_ABILITY, cursor_row);
    }

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if ((ch >= 'a') && (ch <= (char)'a' + visible_count - 1))
    {
        int selected_index = (int)ch - 'a';
        /* Bounds check for safety */
        if (selected_index >= 0 && selected_index < visible_count) {
            *highlight = visible_abilities[selected_index] + 1;
            return abilities_menu2(skilltype, highlight);
        }
    }

    if ((ch >= 'A') && (ch <= (char)'A' + visible_count - 1))
    {
        int selected_index = (int)ch - 'A';
        /* Bounds check for safety */
        if (selected_index >= 0 && selected_index < visible_count) {
            *highlight = visible_abilities[selected_index] + 1;
            return abilities_menu2(skilltype, highlight);
        }
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
    {
        return (ABILITIES_MAX + 1);
    }

    if (ch == '\t')
    {
        return (ABILITIES_MAX + 2);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
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
        log_trace("ABILITY_SCREEN: Calling abilities_menu1 with highlight1=%d", highlight1);
        skilltype = abilities_menu1(&highlight1) - 1;

        log_trace("ABILITY_SCREEN: abilities_menu1 returned skilltype=%d", skilltype);

        // if a skill has been selected...
        if ((skilltype >= 0) && (skilltype < S_MAX))
        {
            log_trace("ABILITY_SCREEN: Valid skill selected (%d), entering abilities loop", skilltype);
            
            /* Reset highlight2 to 1 when entering a new skill category */
            highlight2 = 1;
            
            while (!return_to_skills)
            {
                log_trace("ABILITY_SCREEN: Calling abilities_menu2 for skilltype=%d with highlight2=%d", skilltype, highlight2);
                abilitynum = abilities_menu2(skilltype, &highlight2) - 1;

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
                        if (prereqs(skilltype, abilitynum))
                        {
                            // Normalize flag check to 0 or 1
                            int is_free = (c_info[p_ptr->pcharacter].flags & RHF_FREE) ? 1 : 0;
                            int unit_cost = 500 - 200 * is_free;

                            // Calculate base cost
                            int exp_cost = (abilities_in_skill(skilltype) + 1) * unit_cost;

                            // Subtract free abilities granted by affinity
                            exp_cost -= unit_cost * affinity_level(skilltype);

                            // For song abilities, also subtract minstrel bonus (uncapped)
                            if (skilltype == S_SNG)
                                exp_cost -= unit_cost * minstrel_level();

                            // Clamp to zero
                            if (exp_cost < 0)
                                exp_cost = 0;

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
                                            && (oathchoice <= (z_info ? z_info->oath_max - 1 : OATH_TYPES)))
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
                                        else if (oathchoice == (z_info ? z_info->oath_max + 1 : OATH_TYPES + 1))
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
                                    if (get_check("Are you sure you wish to "
                                                  "gain this ability? "))
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
                                                    oath_name[oathchoice]),
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
                            bell("Insufficient prerequisites for ability!");
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
#define SMT_MENU_ACCEPT 6

#define SMT_MENU_MAX 6

#define SMT_NUM_MENU_I_ATT 1
#define SMT_NUM_MENU_D_ATT 2
#define SMT_NUM_MENU_I_DS 3
#define SMT_NUM_MENU_D_DS 4
#define SMT_NUM_MENU_I_EVN 5
#define SMT_NUM_MENU_D_EVN 6
#define SMT_NUM_MENU_I_PS 7
#define SMT_NUM_MENU_D_PS 8
#define SMT_NUM_MENU_I_PVAL 9
#define SMT_NUM_MENU_D_PVAL 10
#define SMT_NUM_MENU_I_WGT 11
#define SMT_NUM_MENU_D_WGT 12
#define SMT_NUM_MENU_ALLOY_CYCLE 13
#define SMT_NUM_MENU_ALLOY_CLEAR 14

#define SMT_NUM_MENU_MAX 14

#define COL_SMT1 2
#define COL_SMT2 16
#define COL_SMT3 36
#define COL_SMT4 66

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
    int cat = smith_item_category(o_ptr);
    return (cat == CAT_WEAPON) || (cat == CAT_ARMOUR);
}

static bool smith_apply_alloy(object_type* o_ptr, smith_alloy_state* state, smith_alloy_type new_type)
{
    if (!o_ptr || !state)
        return false;

    smith_remove_alloy_bonus(o_ptr, state);

    if (new_type == SMITH_ALLOY_NONE)
        return true;

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

#define MAX_SMITHING_FLAGS (32 * 3)

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
    { CAT_SKILL, TR1_SNG, 1, "Song" },
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
 * Determines whether the attack bonus of an item is eligible for modification.
 */
int att_valid(void)
{
    switch (smith_o_ptr->tval)
    {
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:
    case TV_DIGGING:
    case TV_BOW:
    case TV_ARROW:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        return (true);
    }

    case TV_RING:
    {
        if (smith_o_ptr->sval == SV_RING_ACCURACY)
            return (true);
        if (smith_o_ptr->name1)
            return (true);
    }
    }

    return (false);
}

/*
 * Determines the maximum legal attack bonus for an item.
 */
int att_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    ego_item_type* e_ptr = &e_info[smith_o_ptr->name2];
    int att = 0;

    switch (smith_o_ptr->tval)
    {
    case TV_ARROW:
    {
        att = 3;
        if (smith_o_ptr->name1)
            att += 8;
        if (smith_o_ptr->name2)
            att += e_ptr->max_att;
        break;
    }
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:
    case TV_DIGGING:
    case TV_BOW:
    {
        att = k_ptr->att + 1;
        if (smith_o_ptr->name2)
            att += e_ptr->max_att;
        if (smith_o_ptr->name1)
            att += 4;
        break;
    }
    case TV_BOOTS:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        att = k_ptr->att + 1;
        if (att > 0)
            att = 0;
        if (smith_o_ptr->name2)
            att += e_ptr->max_att;
        if (smith_o_ptr->name1)
            att += 1;
        break;
    }
    case TV_GLOVES:
    {
        att = k_ptr->att + 1;
        if (att > 0)
            att = 0;
        if (smith_o_ptr->name2)
            att += e_ptr->max_att;
        if (smith_o_ptr->name1)
            att += 2;
        break;
    }
    case TV_RING:
    {
        if (smith_o_ptr->sval == SV_RING_ACCURACY)
            att = 4;
        if (smith_o_ptr->name1)
            att = 4;
        break;
    }
    }

    return (att);
}

/*
 * Determines the minimum legal attack bonus for an item.
 */
int att_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    ego_item_type* e_ptr = &e_info[smith_o_ptr->name2];
    int att = 0;

    switch (smith_o_ptr->tval)
    {
    case TV_ARROW:
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:
    case TV_DIGGING:
    case TV_BOW:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        att = k_ptr->att;
        if (smith_o_ptr->name2 && (e_ptr->max_att > 0))
            att += 1;
        break;
    }
    case TV_RING:
    {
        if (smith_o_ptr->sval == SV_RING_ACCURACY)
            att = 1;
        break;
    }
    }

    return (att);
}

/*
 * Determines whether the damage sides of an item is eligible for modification.
 */
int ds_valid(void)
{
    switch (smith_o_ptr->tval)
    {
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:
    case TV_DIGGING:
    case TV_BOW:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Determines the maximum legal damage sides for an item.
 */
int ds_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    ego_item_type* e_ptr = &e_info[smith_o_ptr->name2];
    int ds = 0;

    switch (smith_o_ptr->tval)
    {
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:
    case TV_DIGGING:
    case TV_BOW:
    {
        ds = k_ptr->ds + 1;
        if (smith_o_ptr->name2)
            ds += e_ptr->to_ds;
        if (smith_o_ptr->name1)
            ds += 2;
        break;
    }
    }

    return (ds);
}

/*
 * Determines the minimum legal damage sides for an item.
 */
int ds_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    ego_item_type* e_ptr = &e_info[smith_o_ptr->name2];
    int ds = 0;

    switch (smith_o_ptr->tval)
    {
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:
    case TV_DIGGING:
    case TV_BOW:
    {
        ds = k_ptr->ds;
        if (smith_o_ptr->name2 && (e_ptr->to_ds > 0))
            ds += 1;
        break;
    }
    }

    return (ds);
}

/*
 * Determines whether the evasion bonus of an item is eligible for modification.
 */
int evn_valid(void)
{
    switch (smith_o_ptr->tval)
    {
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        return (true);
    }

    case TV_RING:
    {
        if (smith_o_ptr->sval == SV_RING_EVASION)
            return (true);
        if (smith_o_ptr->name1)
            return (true);
    }
    }

    if (smith_o_ptr->name1
        && ((smith_o_ptr->tval == TV_SWORD) || (smith_o_ptr->tval == TV_POLEARM)
            || (smith_o_ptr->tval == TV_HAFTED)))
    {
        return (true);
    }

    return (false);
}

/*
 * Determines the maximum legal evasion bonus for an item.
 */
int evn_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    ego_item_type* e_ptr = &e_info[smith_o_ptr->name2];
    int evn = 0;

    switch (smith_o_ptr->tval)
    {
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        evn = k_ptr->evn + 1;
        if (smith_o_ptr->name2)
            evn += e_ptr->max_evn;
        if (smith_o_ptr->name1)
            evn += 1;
        break;
    }

    case TV_RING:
    {
        if (smith_o_ptr->sval == SV_RING_EVASION)
            evn = 4;
        if (smith_o_ptr->name1)
            evn = 4;
        break;
    }

    default:
    {
        evn = k_ptr->evn;
        if (smith_o_ptr->name2)
            evn += e_ptr->max_evn;
        if (smith_o_ptr->name1)
            evn += 1;
    }
    }

    return (evn);
}

/*
 * Determines the minimum legal evasion bonus for an item.
 */
int evn_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    ego_item_type* e_ptr = &e_info[smith_o_ptr->name2];
    int evn = 0;

    switch (smith_o_ptr->tval)
    {
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        evn = k_ptr->evn;
        if (smith_o_ptr->name2 && (e_ptr->max_evn > 0))
            evn += 1;
        break;
    }

    case TV_RING:
    {
        if (smith_o_ptr->sval == SV_RING_EVASION)
            evn = 1;
        break;
    }

    default:
    {
        evn = k_ptr->evn;
        if (smith_o_ptr->name2 && (e_ptr->max_evn > 0))
            evn += 1;
    }
    }

    return (evn);
}

/*
 * Determines whether the protection sides of an item is eligible for
 * modification.
 */
int ps_valid(void)
{
    switch (smith_o_ptr->tval)
    {
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        return (true);
    }

    case TV_RING:
    {
        if (smith_o_ptr->sval == SV_RING_PROTECTION)
            return (true);
        if (smith_o_ptr->name1)
            return (true);
    }
    }

    return (false);
}

/*
 * Determines the maximum legal protection sides for an item.
 */
int ps_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    ego_item_type* e_ptr = &e_info[smith_o_ptr->name2];
    int ps = 0;

    switch (smith_o_ptr->tval)
    {
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        ps = k_ptr->ps + 1;

        // cloaks and robes cannot get extra protection sides
        if ((smith_o_ptr->tval == TV_CLOAK)
            || ((smith_o_ptr->tval == TV_SOFT_ARMOR)
                && (smith_o_ptr->sval == SV_ROBE)))
        {
            ps = 0;
        }
        if ((smith_o_ptr->tval == TV_MAIL)
            && (smith_o_ptr->sval == SV_LONG_CORSLET))
        {
            ps += 1;
        }

        if (smith_o_ptr->name2)
            ps += e_ptr->to_ps;
        if (smith_o_ptr->name1)
            ps += 2;
        break;
    }

    case TV_RING:
    {
        if (smith_o_ptr->sval == SV_RING_PROTECTION)
            ps = 3;
        if (smith_o_ptr->name1)
            ps = 3;
        break;
    }
    }

    return (ps);
}

/*
 * Determines the minimum legal protection sides for an item.
 */
int ps_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    ego_item_type* e_ptr = &e_info[smith_o_ptr->name2];
    int ps = 0;

    switch (smith_o_ptr->tval)
    {
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        ps = k_ptr->ps;
        if (smith_o_ptr->name2 && (e_ptr->to_ps > 0))
            ps += 1;
        break;
    }

    case TV_RING:
    {
        if (smith_o_ptr->sval == SV_RING_PROTECTION)
            ps = 1;
        break;
    }
    }

    return (ps);
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
 */
int pval_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    ego_item_type* e_ptr = &e_info[smith_o_ptr->name2];
    u32b f1, f2, f3;
    int pval = 4;

    object_flags(smith_o_ptr, &f1, &f2, &f3);

    // start with the base pval
    pval = k_ptr->pval;

    // artefacts have pvals that are mostly unlimited
    if (smith_o_ptr->name1)
    {
        pval += 4;
    }

    // non-artefact rings and amulets have a maximum pval of 4
    else if ((smith_o_ptr->tval == TV_RING) || (smith_o_ptr->tval == TV_AMULET))
    {
        pval = 4;
    }

    // special items have pvals that are limited by their 'special.txt' entries
    if (smith_o_ptr->name2)
    {
        if (cursed_p(smith_o_ptr))
        {
            if (e_ptr->max_pval > 0)
                pval -= 1;
        }
        else
        {
            pval += e_ptr->max_pval;
        }
    }

    return (pval);
}

/*
 * Determines the minimum legal pval for an item.
 */
int pval_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    return k_ptr->pval;
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
    strnfmt(buf, 80, "%c)", (char)'a' + old_highlight - 1);
    Term_putstr(col, old_highlight + 1, -1, old_attr, buf);

    // highlight the new label
    strnfmt(buf, 80, "%c)", (char)'a' + new_highlight - 1);
    Term_putstr(col, new_highlight + 1, -1, TERM_L_BLUE, buf);
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
                    inven_item_optimize(slot);
                    inven_item_describe(slot);
                    window_stuff();
                }

                // now give the last stack of mithril to the player
                slot = inven_carry(i_ptr, true);
                inven_item_optimize(slot);
                inven_item_describe(slot);
                window_stuff();

                return (true);
            }

            else
                return (false);
        }
    }

    return (false);
}

int mithril_items_carried(void)
{
    int number = 0;
    int item;
    u32b f1, f2, f3;

    for (item = 0; item < INVEN_TOTAL; item++)
    {
        object_type* o_ptr = &inventory[item];

        object_flags(o_ptr, &f1, &f2, &f3);

        /* Only count mithril items that can be melted (exclude Gamil-forged) */
        if ((f3 & TR3_MITHRIL) && !(o_ptr->ident & IDENT_CANT_MELT))
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
    u32b f1, f2, f3;
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
    object_flags(o_ptr, &f1, &f2, &f3);
    int att_base = o_ptr->att - smith_alloy.bonus_att;
    int evn_base = o_ptr->evn - smith_alloy.bonus_evn;
    int ds_base = o_ptr->ds - smith_alloy.bonus_ds;
    int ps_base = o_ptr->ps - smith_alloy.bonus_ps;

    /* ------------------------------------------------------------------
     *  GAMIL character bonus
     *  � Craft mithril items without mithril material
     *  � Costs 3 forge uses instead of 1
     *  � Mark item with TR3_CANT_MELT so the melt-menu ignores it
     * ------------------------------------------------------------------ */


    /* Telchar: 25 % discount on Sharpness tiers */
    if (telchar_bonus && (f1 & (TR1_SHARPNESS | TR1_SHARPNESS2) || (f3 & TR3_ACCURATE)))
        dif_mult -= 25;

    /*  FEANOR character bonus
     *  � 40% off on all lamps
     *  � 25% off on any fire- or light-branded object */
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

    // attack bonus
    x = att_base - k_ptr->att;

    // special costs for attack bonus for arrows
    if ((o_ptr->tval == TV_ARROW) && (x > 0))
    {
        int old_di = dif_inc;

        dif_mod(x, 5, &dif_inc);
        dif_inc = (dif_inc - old_di) / 2;
    }
    // special costs for attack bonus for other weapons
    else if ((o_ptr->tval == TV_BOW || o_ptr->tval == TV_SWORD
                 || o_ptr->tval == TV_POLEARM || o_ptr->tval == TV_HAFTED)
        && (x > 0))
    {
        dif_mod(x, 3, &dif_inc);
    }
    // normal costs for other items
    else
    {
        dif_mod(x, 6, &dif_inc);
        if (x > 0)
            dif_inc -= 1;
    }

    // evasion bonus
    x = evn_base - k_ptr->evn;
    dif_mod(x, 6, &dif_inc);
    if (x > 0)
        dif_inc -= 1;

    // damage bonus
    x = (ds_base - k_ptr->ds);
    // dd used to be a factor here, but a shortsword is far more breakable than
    // a great axe adjusted to make >1 damage sides expensive to smith
    dif_mod(x, 3 * x + 2, &dif_inc);

    // protection bonus
    base = (k_ptr->ps > 0) ? ((k_ptr->ps + 1) * k_ptr->pd) : 0;
    int ps_calc = (ps_base > 0) ? ps_base : 0;
    new = (ps_calc > 0) ? ((ps_calc + 1) * o_ptr->pd) : 0;
    x = new - base;

    // special costs for protection sides on hauberks and rings
    if ((o_ptr->tval == TV_MAIL) && (o_ptr->sval == SV_LONG_CORSLET) && (x > 0))
    {
        dif_mod(x, 1, &dif_inc);
        dif_inc += 2;
    }
    else if ((o_ptr->tval == TV_RING) && (x > 0))
    {
        dif_mod(x, 1, &dif_inc);
        dif_inc += 4;
    }
    else
    {
        dif_mod(x, 3, &dif_inc);
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
    if (f3 & TR3_ACCURATE)
    {
        dif_inc += 15;
        smithing_cost.dex += 1;
    }

    // pval dependent bonuses
    if (f1 & TR1_TUNNEL)
    {
        x = o_ptr->pval - k_ptr->pval;
        dif_mod(x, 8, &dif_inc);
        smithing_cost.str += (x > 0) ? x : 0;
    }
    if (o_ptr->pval != 0)
    {
        x = (o_ptr->pval > 0) ? o_ptr->pval : 0;

        if (f1 & TR1_DAMAGE_SIDES)
        {
            dif_mod(x, 18, &dif_inc);
            smithing_cost.str += x;
        }
        if (f1 & TR1_STR)
        {
            dif_mod(x, 14, &dif_inc);
            smithing_cost.str += x;
        }
        if (f1 & TR1_DEX)
        {
            dif_mod(x, 14, &dif_inc);
            smithing_cost.dex += x;
        }
        if (f1 & TR1_CON)
        {
            dif_mod(x, 14, &dif_inc);
            smithing_cost.con += x;
        }
        if (f1 & TR1_GRA)
        {
            dif_mod(x, 14, &dif_inc);
            smithing_cost.gra += x;
        }
        if (f1 & TR1_ARC)
        {
            dif_mod(x, 4, &dif_inc);
        }
        if (f1 & TR1_STL)
        {
            dif_mod(x, 4, &dif_inc);
        }
        if (f1 & TR1_PER)
        {
            dif_mod(x, 3, &dif_inc);
        }
        if (f1 & TR1_WIL)
        {
            dif_mod(x, 3, &dif_inc);
        }
        if (f1 & TR1_SNG)
        {
            dif_mod(x, 4, &dif_inc);
        }

        x = (o_ptr->pval < 0) ? o_ptr->pval : 0;

        if (f1 & TR1_NEG_STR)
        {
            dif_mod(-x, 12, &dif_inc);
            smithing_cost.str -= x;
        }
        if (f1 & TR1_NEG_DEX)
        {
            dif_mod(-x, 12, &dif_inc);
            smithing_cost.dex -= x;
        }
        if (f1 & TR1_NEG_CON)
        {
            dif_mod(-x, 12, &dif_inc);
            smithing_cost.con -= x;
        }
        if (f1 & TR1_NEG_GRA)
        {
            dif_mod(-x, 12, &dif_inc);
            smithing_cost.gra -= x;
        }
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
            dif_dec += 3;
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
        if (f3 & TR2_TRAITOR)
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

   /* Gamil character bonus � override normal mithril cost */
  if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_GAMIL)      /* you�re Gamil */
      && (k_ptr->flags3 & TR3_MITHRIL)                     /* item is mithril */
      && (mithril_carried() < smithing_cost.mithril))      /* no mithril on hand */
  {
      smithing_cost.uses    = MAX(smithing_cost.uses, 3);  /* cost 3 forge uses */
      smithing_cost.mithril = 0;                           /* waive material */
      o_ptr->ident         |= IDENT_CANT_MELT;             /* can�t melt later */
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
    if (smith_o_ptr->name2 && !p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT])
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
    int i;

    for (i = 0; i < 5; i++)
    {
        Term_putstr(1, MAX_SMITHING_TVALS + 3 + i, -1, TERM_WHITE,
            "                                                              "
            "             ");
    }
}

/*
 * Displays the object's name and description at the bottom of the screen.
 */
void prt_object_description(void)
{
    char o_desc[80];
    char buf[80];
    int display_flag;

    wipe_object_description();

    if (p_ptr->smithing_leftover)
    {
        Term_putstr(
            COL_SMT1, MAX_SMITHING_TVALS + 3, -1, TERM_L_BLUE, "In progress:");
        sprintf(buf, "%3d turns left", p_ptr->smithing_leftover);
        Term_putstr(COL_SMT1 - 1, MAX_SMITHING_TVALS + 5, -1, TERM_BLUE, buf);
    }

    // abort if there is no object to display
    if (smith_o_ptr->tval == 0)
        return;

    if (smith_o_ptr->number > 1)
        display_flag = true;
    else
        display_flag = false;

    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, display_flag, 2);

    SDL_strlcat(o_desc,
        format("   %d.%d lb", smith_o_ptr->weight * smith_o_ptr->number / 10,
            (smith_o_ptr->weight * smith_o_ptr->number) % 10),
        sizeof(o_desc));

    Term_putstr(COL_SMT2, MAX_SMITHING_TVALS + 3, -1, TERM_L_WHITE, o_desc);

    Term_gotoxy(COL_SMT2, MAX_SMITHING_TVALS + 4);

    /* Set hooks for character dump */
    object_info_out_flags = object_flags;

    /* Set the indent/wrap */
    text_out_indent = COL_SMT2;
    text_out_wrap = 79;

    text_out_hook = text_out_to_screen;

    text_out_c(TERM_WHITE, k_text + k_info[smith_o_ptr->k_idx].text);

    if ((k_text + k_info[smith_o_ptr->k_idx].text)[0] != '\0')
        text_out(" ");

    /* Dump the info */
    if (object_info_out(smith_o_ptr))
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

    sprintf(buf, "(max %d)",
        p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
    Term_putstr(COL_SMT4 + 5, 4, -1, TERM_L_DARK, buf);

    // display cost information
    if (smithing_cost.weaponsmith)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Weaponsmith");
        costs++;
    }
    if (smithing_cost.armoursmith)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Armoursmith");
        costs++;
    }
    if (smithing_cost.jeweller)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Jeweller");
        costs++;
    }
    if (smithing_cost.enchantment)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Enchantment");
        costs++;
    }
    if (smithing_cost.artifice)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Artifice");
        costs++;
    }
    if (smithing_cost.alloy_mastery)
    {
        Term_putstr(
            COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Alloy Mastery");
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
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);

        sprintf(buf, "(of %d)", forge_uses(p_ptr->py, p_ptr->px));
        Term_putstr(COL_SMT4 + 9, 10 + costs, -1, TERM_L_DARK, buf);
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
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
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
        sprintf(buf, "%d.%d lb Mithril", smithing_cost.mithril / 10,
            smithing_cost.mithril % 10);
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
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
        sprintf(buf, "%d.%d lb Star Iron", smithing_cost.star_iron / 10,
            smithing_cost.star_iron % 10);
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
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
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
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
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
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
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
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
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
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
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
        costs++;
    }
    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        turn_multiplier /= 2;
    }

    attr = TERM_SLATE;
    sprintf(buf, "%d Turns", MAX(10, dif * turn_multiplier));
    Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
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
    Term_putstr(COL_SMT4, 8, -1, attr, "Cost:");
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
    bool is_enchanted = (!is_artifact) && (o_ptr->name2 != 0);

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

    // clear the right of the screen
    wipe_screen_from(COL_SMT4);

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
            strnfmt(buf, 80, "%c) %s", (char)'a' + num, name);
            Term_putstr(COL_SMT3, num + 2, -1,
                valid[num] ? TERM_WHITE : TERM_SLATE, buf);

            /* Remember the object sval */
            sval[num] = k_ptr->sval;

            // count the applicable items
            num++;
        }
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT3, *highlight + 1, -1, TERM_L_BLUE, buf);

    // make a simple version of the object
    create_base_object(tval, sval[*highlight - 1]);

    // display the object description
    prt_object_description();

    // display the object difficulty
    prt_object_difficulty();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if ((ch >= 'a') && (ch <= (char)'a' + MAX_SMITHING_TVALS - 1))
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
    wipe_screen_from(COL_SMT2);

    // clear bottom of the screen
    wipe_object_description();

    /* Wipe the smithing object */
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    for (i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        strnfmt(buf, 80, "%c) %s", (char)'a' + i, smithing_tvals[i].desc);

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

        Term_putstr(
            COL_SMT2, i + 2, -1, valid[i] ? valid_attr : TERM_L_DARK, buf);
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    // choose an option by letter
    if ((ch >= 'a') && (ch <= (char)'a' + MAX_SMITHING_TVALS - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(old_highlight,
            valid[old_highlight] ? TERM_WHITE : TERM_L_DARK, *highlight,
            COL_SMT2);

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
        smith_o_ptr->ps++;
        break;
    case SMT_NUM_MENU_D_PS:
        smith_o_ptr->ps--;
        break;
    case SMT_NUM_MENU_I_PVAL:
        smith_o_ptr->pval++;
        break;
    case SMT_NUM_MENU_D_PVAL:
        smith_o_ptr->pval--;
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
            bell("Alloying only applies to weapons and armour.");
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
    wipe_screen_from(COL_SMT2);

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
    valid[SMT_NUM_MENU_I_PS - 1] = ps_valid() && (smith_o_ptr->ps < ps_max());
    valid[SMT_NUM_MENU_D_PS - 1] = ps_valid() && (smith_o_ptr->ps > ps_min());
    valid[SMT_NUM_MENU_I_PVAL - 1]
        = pval_valid() && (smith_o_ptr->pval < pval_max());
    valid[SMT_NUM_MENU_D_PVAL - 1]
        = pval_valid() && (smith_o_ptr->pval > pval_min());
    valid[SMT_NUM_MENU_I_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight + 5) <= wgt_max());
    valid[SMT_NUM_MENU_D_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight - 5) >= wgt_min());
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
            || (i == SMT_NUM_MENU_ALLOY_CLEAR - 1))
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

    Term_putstr(COL_SMT2, 2, -1, attr[SMT_NUM_MENU_I_ATT - 1],
        "a) increase attack bonus");
    Term_putstr(COL_SMT2, 3, -1, attr[SMT_NUM_MENU_D_ATT - 1],
        "b) decrease attack bonus");
    Term_putstr(COL_SMT2, 4, -1, attr[SMT_NUM_MENU_I_DS - 1],
        "c) increase damage sides");
    Term_putstr(COL_SMT2, 5, -1, attr[SMT_NUM_MENU_D_DS - 1],
        "d) decrease damage sides");
    Term_putstr(COL_SMT2, 6, -1, attr[SMT_NUM_MENU_I_EVN - 1],
        "e) increase evasion bonus");
    Term_putstr(COL_SMT2, 7, -1, attr[SMT_NUM_MENU_D_EVN - 1],
        "f) decrease evasion bonus");
    Term_putstr(COL_SMT2, 8, -1, attr[SMT_NUM_MENU_I_PS - 1],
        "g) increase protection sides");
    Term_putstr(COL_SMT2, 9, -1, attr[SMT_NUM_MENU_D_PS - 1],
        "h) decrease protection sides");
    Term_putstr(COL_SMT2, 10, -1, attr[SMT_NUM_MENU_I_PVAL - 1],
        "i) increase special bonus");
    Term_putstr(COL_SMT2, 11, -1, attr[SMT_NUM_MENU_D_PVAL - 1],
        "j) decrease special bonus");
    Term_putstr(
        COL_SMT2, 12, -1, attr[SMT_NUM_MENU_I_WGT - 1], "k) increase weight");
    Term_putstr(
        COL_SMT2, 13, -1, attr[SMT_NUM_MENU_D_WGT - 1], "l) decrease weight");
    Term_putstr(COL_SMT2, 14, -1, attr[SMT_NUM_MENU_ALLOY_CYCLE - 1],
        "m) cycle alloy (none/mithril/star iron)");
    Term_putstr(COL_SMT2, 15, -1, attr[SMT_NUM_MENU_ALLOY_CLEAR - 1],
        "n) remove alloy bonus");
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
        Term_putstr(COL_SMT2, 16, -1, info_attr, buf);
    }
    else if (!has_alloy_mastery)
    {
        Term_putstr(COL_SMT2, 16, -1, TERM_L_DARK,
            "Alloy requires Alloy mastery.");
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);

    // display the object description
    prt_object_description();

    // display the object difficulty
    prt_object_difficulty();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    // choose an option by letter
    if ((ch >= 'a') && (ch <= (char)'a' + SMT_NUM_MENU_MAX - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(
            old_highlight, attr[old_highlight], *highlight, COL_SMT2);

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

/*
 * Displays a menu for modifying numerical bonuses and weight of an item.
 */
void numbers_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    if (smith_o_ptr->name2)
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
            modify_numbers(choice);
            break;
        }
        }
    }

    /* Load screen */
    screen_load();

    return;
}

void create_special(int name2)
{
    // retrieve a backup of the object
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    // set its 'special' name to reflect the chosen type
    smith_o_ptr->name2 = name2;

    // make it into that special type
    object_into_special(smith_o_ptr, p_ptr->skill_use[S_SMT], true);

    // Re-evaluate stack size now that an enchantment is applied
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

/*
 * Performs the interface and selection work for the enchantment menu.
 */
int enchant_menu_aux(int* highlight)
{
    char ch;
    int i, j, num;
    char buf[80];
    bool valid[20];
    int choice[20];

    // clear the right of the screen
    wipe_screen_from(COL_SMT2);

    /* We have to search the whole special item list. */
    for (num = 0, i = 1; i < z_info->e_max; i++)
    {
        ego_item_type* e_ptr = &e_info[i];
        bool acceptable = false;

        /* Don't create cursed */
        // if (e_ptr->flags3 & TR3_LIGHT_CURSE) continue;

        /* Don't create useless */
        // if (e_ptr->cost == 0) continue;

        /* Test if this is a legal special item type for this object */
        for (j = 0; j < EGO_TVALS_MAX; j++)
        {
            /* Require identical base type */
            if (smith_o_ptr->tval == e_ptr->tval[j])
            {
                /* Require sval in bounds, lower */
                if (smith_o_ptr->sval >= e_ptr->min_sval[j])
                {
                    /* Require sval in bounds, upper */
                    if (smith_o_ptr->sval <= e_ptr->max_sval[j])
                    {
                        /* Accept */
                        acceptable = true;
                    }
                }
            }
        }

        if (acceptable)
        {
            // make a 'special' version of the object
            create_special(i);

            // Check whether it is a valid choice for creating
            if (affordable(smith_o_ptr))
            {
                valid[num] = true;
            }
            else
            {
                valid[num] = false;
            }

            /* Print it */
            strnfmt(buf, 80, "%c) %s", (char)'a' + num, e_name + e_ptr->name);
            Term_putstr(COL_SMT2, num + 2, -1,
                valid[num] ? TERM_WHITE : TERM_SLATE, buf);

            /* Remember the object index */
            choice[num] = i;

            // count the applicable items
            num++;
        }
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);

    // make a 'special' version of the object
    create_special(choice[*highlight - 1]);

    // display the object description
    prt_object_description();

    // display the object difficulty
    prt_object_difficulty();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    /* Choose by letter */
    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        // make a 'special' version of the object
        create_special(choice[*highlight - 1]);

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
 * Brings up a menu for making an item into a {special} item.
 */
bool enchant_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;
    bool completed = false;

    /* Save screen */
    screen_save();

    // stop the item being an artefact, if it was
    smith_o_ptr->name1 = 0;
    smith2_o_ptr->name1 = 0;

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = enchant_menu_aux(&highlight);

        if (choice >= 1)
        {
            leave_menu = true;
            completed = true;
        }
        else if (choice == -1)
        {
            leave_menu = true;
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
    smith_a_ptr->cur_num = 1;
    smith_a_ptr->found_num = 1;
    smith_a_ptr->max_num = 1;
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
    u32b f1, f2, f3;

    /* Telchar may always put SHARPNESS II on a melee weapon               */
    if ((f == TR1_SHARPNESS2) &&
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
    object_flags(o_ptr, &f1, &f2, &f3);

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
        }
    }

    return (ok);
}

/*
 * Adds a given flag to the dummy artefact.
 */
void add_artefact_flag(u32b f, int flagset)
{
    log_trace("Adding artifact flag %u in flagset %d", f, flagset);
    
    // prepare the artefact and object for modification
    prepare_artefact();

    // set new flag on the artefact
    if (flagset == 1)
        smith_a_ptr->flags1 |= f;
    if (flagset == 2)
        smith_a_ptr->flags2 |= f;
    if (flagset == 3)
        smith_a_ptr->flags3 |= f;
}

/*
 * Removes a given flag from the dummy artefact.
 */
void remove_artefact_flag(u32b f, int flagset)
{
    log_trace("Removing artifact flag %u from flagset %d", f, flagset);
    
    // prepare the artefact and object for modification
    prepare_artefact();

    // unset new flag on the artefact
    if (flagset == 1)
        smith_a_ptr->flags1 &= ~(f);
    if (flagset == 2)
        smith_a_ptr->flags2 &= ~(f);
    if (flagset == 3)
        smith_a_ptr->flags3 &= ~(f);
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
    wipe_screen_from(COL_SMT3);

    // display the categories
    for (i = 0; smithing_flag_types[i].flag != 0; i++)
    {
        if (category == smithing_flag_types[i].category)
        {
            /* Telchar-only: skip Sharpness2 if not in character Telchar */
            if (smithing_flag_types[i].flag == TR1_SHARPNESS2 &&
                !(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
            {
                /* don�t even consider it */
                continue;
            }
            flag[num] = smithing_flag_types[i].flag;
            flagset[num] = smithing_flag_types[i].flagset;

            if (((flagset[num] == 1) && (smith2_a_ptr->flags1 & flag[num]))
                || ((flagset[num] == 2) && (smith2_a_ptr->flags2 & flag[num]))
                || ((flagset[num] == 3) && (smith2_a_ptr->flags3 & flag[num])))
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
            strnfmt(buf, 80, "%c) %s", (char)'a' + num,
                smithing_flag_types[i].desc);
            Term_putstr(COL_SMT3, num + 2, -1, attr, buf);

            num++;
        }
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT3, *highlight + 1, -1, TERM_L_BLUE, buf);

    // add this flag to the dummy artefact under construction
    add_artefact_flag(flag[*highlight - 1], flagset[*highlight - 1]);

    // display the object description
    prt_object_description();

    // display the object difficulty
    prt_object_difficulty();

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
    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
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

        // restore the backup artefact
        artefact_copy(smith_a_ptr, smith2_a_ptr);

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
            smith_a_ptr->abilities++;
        }
    }

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
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
        }

        smith_a_ptr->skilltype[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->abilitynum[smith_a_ptr->abilities - 1] = 0;

        smith_a_ptr->abilities--;
    }

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
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
    bool ability_present[20] = { false };
    bool ability_valid[20] = { false };
    bool ability_affordable[20] = { false };
    ability_type* b_ptr;
    ability_type* b2_ptr;
    byte attr;

    // clear the right of the screen
    wipe_screen_from(COL_SMT3);

    // list the abilities
    for (i = 0; i < z_info->b_max - 1; i++)
    {
        b_ptr = &b_info[i];
        b2_ptr = &b_info[i + 1];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries where the next entry is not defined (to avoid the
         * stat-improvements) */
        if (!b2_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skill)
            continue;

        // Determine the appropriate colour
        if (has_ability(smith2_a_ptr, skill, num))
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
                add_artefact_ability(skill, num);

                // require that the ability was successfully added
                if (has_ability(smith_a_ptr, skill, num))
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
        strnfmt(buf, 80, "%c) %s", (char)'a' + num, b_name + b_ptr->name);
        Term_putstr(COL_SMT3, num + 2, -1, attr, buf);

        num++;
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT3, *highlight + 1, -1, TERM_L_BLUE, buf);

    // add this ability to the dummy artefact under construction
    add_artefact_ability(skill, *highlight - 1);

    // display the object description
    prt_object_description();

    // display the object difficulty
    prt_object_difficulty();

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
    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int new_highlight = (int)ch - 'a' + 1;

        if (ability_valid[new_highlight - 1])
        {
            if (new_highlight == *highlight)
            {
                // remove an ability if it already existed
                if (ability_present[*highlight - 1])
                    remove_artefact_ability(skill, *highlight - 1);
            }
            else
            {
                // restore the artefact from backup
                artefact_copy(smith_a_ptr, smith2_a_ptr);

                *highlight = new_highlight;

                // remove an ability if it already existed
                if (ability_present[*highlight - 1])
                    remove_artefact_ability(skill, *highlight - 1);

                // otherwise add it
                else
                    add_artefact_ability(skill, *highlight - 1);
            }

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);

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
                remove_artefact_ability(skill, *highlight - 1);

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);

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
            remove_artefact_ability(skill, *highlight - 1);

        // restore the backup artefact
        artefact_copy(smith_a_ptr, smith2_a_ptr);

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

    // Clear the names
    tmp[0] = '\0';
    old_name[0] = '\0';

    // Clear object name
    Term_putstr(COL_SMT2, MAX_SMITHING_TVALS + 3, -1, TERM_L_WHITE,
        "                                                        ");

    // Determine object name
    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, false, -1);

    // Display shortened object name
    Term_putstr(COL_SMT2, MAX_SMITHING_TVALS + 3, -1, TERM_L_WHITE, o_desc);

    // use old name as a default
    SDL_strlcpy(tmp, smith2_a_ptr->name, sizeof(tmp));

    // save a copy too
    SDL_strlcpy(old_name, op_ptr->full_name, sizeof(old_name));

    /* Prompt for a new name */
    Term_gotoxy(COL_SMT2 + strlen(o_desc) + 1, MAX_SMITHING_TVALS + 3);

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
    wipe_screen_from(COL_SMT2);

    // display the categories for flags
    for (i = 0; i < MAX_CATS; i++)
    {
        strnfmt(buf, 80, "%c) %s", (char)'a' + i, smithing_flag_cats[i].desc);
        Term_putstr(COL_SMT2, i + 2, -1, TERM_WHITE, buf);
    }

    // display the categories for abilities (skip Special abilities - S_SPC)
    int display_idx = 0;
    for (i = 0; i < S_MAX; i++)
    {
        /* Skip Special abilities - they cannot be smithed onto items */
        if (i == S_SPC) continue;
        
        strnfmt(
            buf, 80, "%c) %s", (char)'a' + MAX_CATS + display_idx, skill_names_full[i]);
        Term_putstr(COL_SMT2, display_idx + MAX_CATS + 2, -1, TERM_WHITE, buf);
        display_idx++;
    }

    num = MAX_CATS + display_idx + 1;

    // Menu item for naming artefacts
    strnfmt(buf, 80, "%c) %s", (char)'a' + num - 1, "Name Artefact");
    Term_putstr(COL_SMT2, num + 1, -1, TERM_WHITE, buf);

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);

    // display the object description
    prt_object_description();

    // display the object difficulty
    prt_object_difficulty();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    /* Choose by letter */
    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
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
            smith2_o_ptr->pd = 1;
        }
        if (smith_o_ptr->tval == TV_AMULET)
        {
            create_base_object(TV_AMULET, SV_AMULET_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
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
    wipe_screen_from(COL_SMT2);

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
            strnfmt(buf, 80, "%c) %s", (char)'a' + num, desc);

            Term_putstr(COL_SMT2, num + 2, -1, TERM_WHITE, buf);

            strnfmt(
                buf, 80, "%2d.%d lb", o_ptr->weight / 10, o_ptr->weight % 10);
            Term_putstr(COL_SMT2 + 40, num + 2, -1, TERM_WHITE, buf);

            num++;
        }
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    // choose an option by letter
    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
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
    wipe_screen_from(COL_SMT2);

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
        && (smith_o_ptr->tval != TV_RING) && (smith_o_ptr->tval != TV_AMULET)
        && (smith_o_ptr->tval != TV_HORN)
        && !((smith_o_ptr->tval == TV_DIGGING)
            && (smith_o_ptr->sval == SV_SHOVEL));
    valid[SMT_MENU_ARTEFACT - 1] = (!smith_o_ptr->name2)
        && (smith_o_ptr->tval != 0) && (smith_o_ptr->tval != TV_HORN)
        && (p_ptr->self_made_arts
            < z_info->art_self_made_max - z_info->art_rand_max - 2);
    valid[SMT_MENU_NUMBERS - 1] = (smith_o_ptr->tval != 0);
    valid[SMT_MENU_MELT - 1]
        = mithril_items_carried() && cave_forge_bold(p_ptr->py, p_ptr->px);
    valid[SMT_MENU_ACCEPT - 1] = affordable(smith_o_ptr)
        && cave_forge_bold(p_ptr->py, p_ptr->px)
        && (forge_uses(p_ptr->py, p_ptr->px) > 0);

    // display labels
    valid_attr = (p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        ? TERM_WHITE
        : TERM_RED;
    Term_putstr(COL_SMT1, 2, -1,
        valid[SMT_MENU_CREATE - 1] ? valid_attr : TERM_L_DARK, "a) Base Item");
    valid_attr = (p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT]) ? TERM_WHITE
                                                                 : TERM_RED;
    Term_putstr(COL_SMT1, 3, -1,
        valid[SMT_MENU_ENCHANT - 1] ? valid_attr : TERM_L_DARK, "b) Enchant");
    valid_attr
        = (p_ptr->active_ability[S_SMT][SMT_ARTEFACT]) ? TERM_WHITE : TERM_RED;
    Term_putstr(COL_SMT1, 4, -1,
        valid[SMT_MENU_ARTEFACT - 1] ? valid_attr : TERM_L_DARK, "c) Artifice");
    Term_putstr(COL_SMT1, 5, -1,
        valid[SMT_MENU_NUMBERS - 1] ? TERM_WHITE : TERM_L_DARK, "d) Numbers");
    Term_putstr(COL_SMT1, 6, -1,
        valid[SMT_MENU_MELT - 1] ? TERM_WHITE : TERM_L_DARK, "e) Melt");

    if (p_ptr->smithing_leftover == 0)
    {
        Term_putstr(COL_SMT1, 7, -1,
            valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK, "f) Accept");
    }
    else
    {
        Term_putstr(COL_SMT1, 7, -1,
            valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK, "f) Resume");
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
        if (smith_o_ptr->name2)
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
            "Choose a mithril item to melt down.");
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
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT1, *highlight + 1, -1, TERM_L_BLUE, buf);

    // display the object description
    prt_object_description();

    // display the object difficulty
    prt_object_difficulty();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    // choose an option by letter
    if ((ch >= 'a') && (ch <= (char)'a' + SMT_MENU_MAX - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(old_highlight,
            valid[old_highlight] ? TERM_WHITE : TERM_L_DARK, *highlight,
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
            if (mithril_items_carried())
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                melt_menu();
            }
            else
            {
                bell("You don't have any mithril items.");
            }

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

#define MAIN_MENU_RETURN 1
#define MAIN_MENU_CHARACTER 2
#define MAIN_MENU_OPTIONS 3
#define MAIN_MENU_MAP 4
#define MAIN_MENU_SCORES 5
#define MAIN_MENU_KNOWN_OBJECTS 6
#define MAIN_MENU_KNOWN_ARTEFACTS 7
#define MAIN_MENU_KNOWN_MONSTERS 8
#define MAIN_MENU_KNOWN_CURSES 9
#define MAIN_MENU_NOTE 10
#define MAIN_MENU_SCREENSHOT 11
#define MAIN_MENU_MACROS 12
#define MAIN_MENU_COLORS 13
#define MAIN_MENU_MESSAGES 14
#define MAIN_MENU_VERSION 15
#define MAIN_MENU_ABORT 16
#define MAIN_MENU_SAVE 17
#define MAIN_MENU_SAVE_QUIT 18
#define MAIN_MENU_QUEST_STATUS 19
#define MAIN_MENU_HELP 20
#define MAIN_MENU_STORY 21

#define MAIN_MENU_MAX 18

#define COL_MAIN 29

/*
 * Performs the interface and selection work for the main menu.
 */
int main_menu_aux(int* highlight)
{
    char ch;
    int i;
    bool death_view = death_spectator_active();

    if (death_view && (*highlight >= 15) && (*highlight <= 17))
        *highlight = 18;

    for (i = 0; i < MAIN_MENU_MAX + 3; i++)
    {
        Term_putstr(
            COL_MAIN - 2, i, -1, TERM_WHITE, "                           ");
    }

    Term_putstr(COL_MAIN, 2, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        "Character sheet      (c)");
    Term_putstr(COL_MAIN, 3, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        "Known artefacts      (a)");
    Term_putstr(COL_MAIN, 4, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        "Known objects        (b)");
    Term_putstr(COL_MAIN, 5, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        "Known monsters       (n)");
    Term_putstr(COL_MAIN, 6, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        "Known curses         (u)");
    Term_putstr(COL_MAIN, 7, -1, (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE,
        "Quest status         (t)");
    Term_putstr(COL_MAIN, 8, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE,
        "Halls of Mandos      (d)");
    Term_putstr(COL_MAIN, 9, -1, (*highlight == 8) ? TERM_L_BLUE : TERM_WHITE,
        "Run history          (v)");
    Term_putstr(COL_MAIN, 10, -1, (*highlight == 9) ? TERM_L_BLUE : TERM_WHITE,
        "Map                  (m)");
    Term_putstr(COL_MAIN, 11, -1, (*highlight == 10) ? TERM_L_BLUE : TERM_WHITE,
        "Log                  (l)");
    Term_putstr(COL_MAIN, 12, -1, (*highlight == 11) ? TERM_L_BLUE : TERM_WHITE,
        "Combat history       (x)");
    Term_putstr(COL_MAIN, 13, -1, (*highlight == 12) ? TERM_L_BLUE : TERM_WHITE,
        "The story so far     (y)");
    Term_putstr(COL_MAIN, 14, -1, (*highlight == 13) ? TERM_L_BLUE : TERM_WHITE,
        "Options and misc     (o)");
    Term_putstr(COL_MAIN, 15, -1, (*highlight == 14) ? TERM_L_BLUE : TERM_WHITE,
        "Help                 (h)");
    byte suicide_color = death_view ? TERM_L_DARK
        : ((*highlight == 15) ? TERM_L_BLUE : TERM_WHITE);
    Term_putstr(COL_MAIN, 16, -1, suicide_color,
        "Suicide              (k)");
    byte save_color = death_view ? TERM_L_DARK
        : ((*highlight == 16) ? TERM_L_BLUE : TERM_WHITE);
    Term_putstr(COL_MAIN, 17, -1, save_color,
        "Save                 (s)");
    byte quit_color = death_view ? TERM_L_DARK
        : ((*highlight == 17) ? TERM_L_BLUE : TERM_WHITE);
    Term_putstr(COL_MAIN, 18, -1, quit_color,
        "Quit with save       (q)");
    Term_putstr(COL_MAIN, 19, -1, (*highlight == 18) ? TERM_L_BLUE : TERM_WHITE,
        "Return to game       (r)");

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(COL_MAIN, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    // choose an option by letter - alphabetical mapping (updated for new order)
    switch (ch)
    {
    case 'c':
        *highlight = 1;
        return (*highlight);  // Character sheet
    case 'a':
        *highlight = 2;
        return (*highlight);  // Known artefacts
    case 'b':
        *highlight = 3;
        return (*highlight);  // Known objects
    case 'n':
        *highlight = 4;
        return (*highlight);  // Known monsters
    case 'u':
        *highlight = 5;
        return (*highlight);  // Known curses
    case 't':
        *highlight = 6;
        return (*highlight);  // Quest status
    case 'd':
        *highlight = 7;
        return (*highlight);  // Halls of Mandos
    case 'v':
        *highlight = 8;
        return (*highlight);  // Run history
    case 'm':
        *highlight = 9;
        return (*highlight);  // Map
    case 'l':
        *highlight = 10;
        return (*highlight);  // Log
    case 'x':
        *highlight = 11;
        return (*highlight); // Combat history
    case 'y':
        *highlight = 12;
        return (*highlight); // The story so far
    case 'o':
        *highlight = 13;
        return (*highlight); // Options and misc
    case 'h':
        *highlight = 14;
        return (*highlight); // Help
    case 'k':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = 15;
        return (*highlight); // Suicide
    case 's':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = 16;
        return (*highlight); // Save
    case 'q':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = 17;
        return (*highlight); // Quit with save
    case 'r':
        *highlight = 18;
        return (*highlight); // Return to game
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
            *highlight = MAIN_MENU_MAX;
        while (death_view && (*highlight >= 15) && (*highlight <= 17))
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
        while (death_view && (*highlight >= 15) && (*highlight <= 17))
        {
            if (*highlight < MAIN_MENU_MAX)
                (*highlight)++;
            else
                *highlight = 1;
        }
    }

    /* Leave menu */
    if ((ch == ESCAPE) || (ch == '4'))
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

        if (death_spectator_active() && (actiontype >= 15) && (actiontype <= 17))
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
        case 2: // Known artefacts (a)
        {
            do_cmd_knowledge_artefacts();
            leave_menu = true;
            break;
        }
        case 3: // Known objects (b)
        {
            do_cmd_knowledge_objects();
            leave_menu = true;
            break;
        }
        case 4: // Known monsters (n)
        {
            do_cmd_knowledge_monsters();
            leave_menu = true;
            break;
        }
        case 5: // Known curses (u)
        {
            show_known_curses_menu();
            leave_menu = true;
            break;
        }
        case 6: // Quest status (t)
        {
            do_cmd_quest_status();
            leave_menu = true;
            break;
        }
        case 7: // Halls of Mandos (d)
        {
            log_info("main menu: opening Halls of Mandos view");
            show_scores_interactive(true);
            leave_menu = true;
            break;
        }
        case 8: // Run history (v)
        {
            do_cmd_run_history();
            leave_menu = true;
            break;
        }
        case 9: // Map (m)
        {
            do_cmd_view_map();
            leave_menu = true;
            break;
        }
        case 10: // Log (l)
        {
            do_cmd_messages();
            leave_menu = true;
            break;
        }
        case 11: // Combat history (x)
        {
            do_cmd_combat_history();
            leave_menu = true;
            break;
        }
        case 12: // The story so far (y)
        {
            /* Save screen before showing story */
            screen_save();
            print_story(15, 1);
            /* Load screen after story */
            screen_load();
            leave_menu = true;
            break;
        }
        case 13: // Options and misc (o)
        {
            do_cmd_options();
            leave_menu = true;
            break;
        }
        case 14: // Help (h)
        {
            do_cmd_help();
            leave_menu = true;
            break;
        }
        case 15: // Suicide (k)
        {
            do_cmd_suicide();
            leave_menu = true;
            break;
        }
        case 16: // Save (s)
        {
            do_cmd_save_game();
            leave_menu = true;
            break;
        }
        case 17: // Quit with save (q)
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
        case 18: // Return to game (r)
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

}

/*
 * Recall the most recent message
 */
void do_cmd_message_one(void)
{
    /* Recall one message XXX XXX XXX */
    c_prt(message_color(0), format("> %s", message_str(0)), 0, 0);
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

    /* Get size */
    Term_get_size(&wid, &hgt);

    /* Save screen */
    screen_save();

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
extern void do_cmd_options_aux(int page, cptr info)
{
    char ch;

    int i, k = 0, n = 0;

    int opt[OPT_PAGE_PER];

    char buf[80];

    int dir;
    
    bool is_sound_page = (page == SOUND_PAGE);
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
        n = 14; /* 5 enable flags + 5 volume controls + 2 music enable + 2 music volume */
    }

    /* Clear screen */
    Term_clear();

    /* Interact with the player */
    while (true)
    {
        /* Prompt XXX XXX XXX */
        strnfmt(buf, sizeof(buf), "%s", info);
        Term_putstr(2, 1, -1, TERM_WHITE, buf);

        /* Display the options */
        for (i = 0; i < n; i++)
        {
            byte a = TERM_WHITE;

            /* Color current option */
            if (i == k)
                a = TERM_L_BLUE;

            /* Display the option text */
            if (is_sound_page)
            {
                if (i == 0)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %s",
                        "Enable game sounds",
                        sound_cfg->enabled ? "yes" : "no ");
                }
                else if (i == 1)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %s",
                        "Enable combat sounds",
                        sound_cfg->enable_combat ? "yes" : "no ");
                }
                else if (i == 2)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %s",
                        "Enable inventory sounds",
                        sound_cfg->enable_inventory ? "yes" : "no ");
                }
                else if (i == 3)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %s",
                        "Enable walk sounds",
                        sound_cfg->enable_walk ? "yes" : "no ");
                }
                else if (i == 4)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %s",
                        "Enable door sounds",
                        sound_cfg->enable_doors ? "yes" : "no ");
                }
                else if (i == 5)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %.0f%%",
                        "Combat sounds volume",
                        sound_cfg->volume_combat * 100.0f);
                }
                else if (i == 6)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %.0f%%",
                        "Inventory sounds volume",
                        sound_cfg->volume_inventory * 100.0f);
                }
                else if (i == 7)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %.0f%%",
                        "Walk sounds volume",
                        sound_cfg->volume_walk * 100.0f);
                }
                else if (i == 8)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %.0f%%",
                        "Door sounds volume",
                        sound_cfg->volume_doors * 100.0f);
                }
                else if (i == 9)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %.0f%%",
                        "Other sounds volume",
                        sound_cfg->volume_other * 100.0f);
                }
                else if (i == 10)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %s",
                        "Enable main menu music",
                        sound_cfg->music_main_enabled ? "yes" : "no ");
                }
                else if (i == 10)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %s",
                        "Enable main menu music",
                        sound_cfg->music_main_enabled ? "yes" : "no ");
                }
                else if (i == 11)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %s",
                        "Enable ambient dungeon music",
                        sound_cfg->music_ambient_enabled ? "yes" : "no ");
                }
                else if (i == 12)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %.0f%%",
                        "Main menu music volume",
                        sound_cfg->music_main_volume * 100.0f);
                }
                else if (i == 13)
                {
                    strnfmt(buf, sizeof(buf), "%-48s: %.0f%%",
                        "Ambient music volume",
                        sound_cfg->music_ambient_volume * 100.0f);
                }
            }
            else if (opt[i] == OPT_delay_factor)
            {
                strnfmt(buf, sizeof(buf), "%-48s: %d",
                    "Delay factor for animation (0 to 9)",
                    op_ptr->delay_factor);
            }
            else if (opt[i] == OPT_hitpoint_warning)
            {
                strnfmt(buf, sizeof(buf), "%-48s: %d%%",
                    "Hitpoint warning threshold (0% to 90%)",
                    op_ptr->hitpoint_warn * 10);
            }
            else if (opt[i] == OPT_main_combat_rolls)
            {
                strnfmt(buf, sizeof(buf), "%-48s: %d",
                    "Main terminal combat roll lines (0=off, 1-4=lines)",
                    op_ptr->main_combat_rolls);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "%-48s: %s", option_desc[opt[i]],
                    op_ptr->opt[opt[i]] ? "yes" : "no ");
            }

            c_prt(a, buf, i + 3, 2);
        }

        if (page == CHALLENGE_PAGE)
        {
            Term_putstr(2, n + 4, -1, TERM_L_WHITE,
                "Challenge Options can only be altered during character "
                "creation");
            Term_putstr(
                2, n + 5, -1, TERM_L_WHITE, "or on the very first turn");

            if (playerturn == 0)
            {
                Term_putstr(2, n + 7, -1, TERM_SLATE,
                    "(direction keys to set, Return/Escape to accept)");
            }
            else
            {
                Term_putstr(
                    2, n + 7, -1, TERM_SLATE, "(press Return to go back)");
            }
        }
        else
        {
            Term_putstr(2, n + 4, -1, TERM_SLATE,
                "(direction keys to set, Return/Escape to accept)");
        }

        /* Hilite current option */
        move_cursor(k + 3, 52);

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
            /* Save sound settings if on sound page */
            if (is_sound_page)
            {
                sdl_sound_save_config();
                /* Reload sound system to apply changes */
                sdl_sound_reload();
            }
            
            /* Hack -- Notice use of any "cheat" options */
            for (i = OPT_CHEAT; i < OPT_ADULT; i++)
            {
                if (op_ptr->opt[i])
                {
                    /* Set score option */
                    op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = true;
                }
            }

            /* Save persistent settings to metarun after options are changed */
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
                    if (k == 0)
                    {
                        sound_cfg->enabled = !sound_cfg->enabled;
                        use_sound = sound_cfg->enabled;
                    }
                    else if (k == 1) sound_cfg->enable_combat = !sound_cfg->enable_combat;
                    else if (k == 2) sound_cfg->enable_inventory = !sound_cfg->enable_inventory;
                    else if (k == 3) sound_cfg->enable_walk = !sound_cfg->enable_walk;
                    else if (k == 4) sound_cfg->enable_doors = !sound_cfg->enable_doors;
                    else if (k == 10) sound_cfg->music_main_enabled = !sound_cfg->music_main_enabled;
                    else if (k == 11) sound_cfg->music_ambient_enabled = !sound_cfg->music_ambient_enabled;
                    /* Volume controls (5-9, 12-13) don't toggle */
                }
                else
                {
                    op_ptr->opt[opt[k]] = !op_ptr->opt[opt[k]];
                    if (opt[k] == OPT_story_lists_inven_pane || opt[k] == OPT_story_lists_equip_pane)
                        redraw_inven_equip_subwindows();
                    if (opt[k] == OPT_story_monster_desc_pane)
                        redraw_monster_subwindows();
                }
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
                    if (k == 0)
                    {
                        sound_cfg->enabled = true;
                        use_sound = true;
                    }
                    else if (k == 1) sound_cfg->enable_combat = true;
                    else if (k == 2) sound_cfg->enable_inventory = true;
                    else if (k == 3) sound_cfg->enable_walk = true;
                    else if (k == 4) sound_cfg->enable_doors = true;
                    else if (k == 5) sound_cfg->volume_combat = (sound_cfg->volume_combat < 1.0f) ? sound_cfg->volume_combat + 0.1f : 1.0f;
                    else if (k == 6) sound_cfg->volume_inventory = (sound_cfg->volume_inventory < 1.0f) ? sound_cfg->volume_inventory + 0.1f : 1.0f;
                    else if (k == 7) sound_cfg->volume_walk = (sound_cfg->volume_walk < 1.0f) ? sound_cfg->volume_walk + 0.1f : 1.0f;
                    else if (k == 8) sound_cfg->volume_doors = (sound_cfg->volume_doors < 1.0f) ? sound_cfg->volume_doors + 0.1f : 1.0f;
                    else if (k == 9) sound_cfg->volume_other = (sound_cfg->volume_other < 1.0f) ? sound_cfg->volume_other + 0.1f : 1.0f;
                    else if (k == 10) sound_cfg->music_main_enabled = true;
                    else if (k == 11) sound_cfg->music_ambient_enabled = true;
                    else if (k == 12) {
                        sound_cfg->music_main_volume = (sound_cfg->music_main_volume < 1.0f) ? sound_cfg->music_main_volume + 0.1f : 1.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume < 1.0f) ? sound_cfg->music_ambient_volume + 0.1f : 1.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
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
                else
                {
                    op_ptr->opt[opt[k]] = true;
                    if (opt[k] == OPT_story_lists_inven_pane || opt[k] == OPT_story_lists_equip_pane)
                        redraw_inven_equip_subwindows();
                    if (opt[k] == OPT_story_monster_desc_pane)
                        redraw_monster_subwindows();
                }
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
                    if (k == 0)
                    {
                        sound_cfg->enabled = false;
                        use_sound = false;
                    }
                    else if (k == 1) sound_cfg->enable_combat = false;
                    else if (k == 2) sound_cfg->enable_inventory = false;
                    else if (k == 3) sound_cfg->enable_walk = false;
                    else if (k == 4) sound_cfg->enable_doors = false;
                    else if (k == 5) sound_cfg->volume_combat = (sound_cfg->volume_combat > 0.0f) ? sound_cfg->volume_combat - 0.1f : 0.0f;
                    else if (k == 6) sound_cfg->volume_inventory = (sound_cfg->volume_inventory > 0.0f) ? sound_cfg->volume_inventory - 0.1f : 0.0f;
                    else if (k == 7) sound_cfg->volume_walk = (sound_cfg->volume_walk > 0.0f) ? sound_cfg->volume_walk - 0.1f : 0.0f;
                    else if (k == 8) sound_cfg->volume_doors = (sound_cfg->volume_doors > 0.0f) ? sound_cfg->volume_doors - 0.1f : 0.0f;
                    else if (k == 9) sound_cfg->volume_other = (sound_cfg->volume_other > 0.0f) ? sound_cfg->volume_other - 0.1f : 0.0f;
                    else if (k == 10) sound_cfg->music_main_enabled = false;
                    else if (k == 11) sound_cfg->music_ambient_enabled = false;
                    else if (k == 12) {
                        sound_cfg->music_main_volume = (sound_cfg->music_main_volume > 0.0f) ? sound_cfg->music_main_volume - 0.1f : 0.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume > 0.0f) ? sound_cfg->music_ambient_volume - 0.1f : 0.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
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
                else
                {
                    op_ptr->opt[opt[k]] = false;
                    if (opt[k] == OPT_story_lists_inven_pane || opt[k] == OPT_story_lists_equip_pane)
                        redraw_inven_equip_subwindows();
                    if (opt[k] == OPT_story_monster_desc_pane)
                        redraw_monster_subwindows();
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
void do_cmd_pane_settings(void)
{
    int k = 0;
    int n = 9; /* Total number of options (not including save/quit) */
    bool done = false;
    bool settings_changed = false;
    int dir;
    const char* config_path = get_sdl_config_path();
    const char* config_label = (config_path && config_path[0]) ? config_path : "sil_sdl.json";
    
    /* Save screen */
    screen_save();
    
    while (!done)
    {
        /* Clear screen */
        Term_clear();
        
        /* Display title */
        Term_putstr(2, 1, -1, TERM_L_BLUE, "SDL Pane Settings");
        
        /* Display current settings */
        char buf[80];
        int y = 3;
        byte a;
        
        /* Option 0: Main View Scale */
        a = (k == 0) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%-48s: %d", "Main View Scale (1-max) [Alt++/-]", get_sdl_main_view_scale());
        c_prt(a, buf, y++, 2);
        
        /* Option 1: Aux View Font Size */
        a = (k == 1) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%-48s: %d", "Aux View Font Size (8-48)", get_sdl_aux_view_font_size());
        c_prt(a, buf, y++, 2);
        
        /* Option 2: Margin */
        a = (k == 2) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%-48s: %d", "Margin (0-20)", get_sdl_margin());
        c_prt(a, buf, y++, 2);
        
        /* Option 3: Fullscreen */
        a = (k == 3) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%-48s: %s", "Fullscreen", get_sdl_fullscreen() ? "yes" : "no ");
        c_prt(a, buf, y++, 2);
        
        /* Option 4: Tiles */
        a = (k == 4) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%-48s: %s", "Tiles", get_sdl_tiles() ? "yes" : "no ");
        c_prt(a, buf, y++, 2);
        
        /* Option 5: Enable Right Panes */
        a = (k == 5) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%-48s: %s", "Enable Right Panes [Alt+I]", get_sdl_enable_right_panes() ? "yes" : "no ");
        c_prt(a, buf, y++, 2);
        
        /* Option 6: Enable Bottom Panes */
        a = (k == 6) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%-48s: %s", "Enable Bottom Panes [Alt+L]", get_sdl_enable_bottom_panes() ? "yes" : "no ");
        c_prt(a, buf, y++, 2);
        
        y++; /* Blank line */
        
        /* Option 7: View Pane Configuration */
        a = (k == 5) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "View Pane Configuration (%d panes)", get_pane_config_count());
        c_prt(a, buf, y++, 2);
        
        y++; /* Blank line */
        
        /* Option 6: Save/Quit */
        a = (k == 6) ? TERM_L_BLUE : TERM_WHITE;
        if (settings_changed)
        {
            c_prt(a, "Save Changes and Return", y++, 2);
        }
        else
        {
            c_prt(a, "Return to Options Menu", y++, 2);
        }
        
        /* Display help */
        y = Term->hgt - 3;
        if (settings_changed)
        {
            Term_putstr(2, y++, -1, TERM_YELLOW, "Settings changed - changes take effect immediately.");
            Term_putstr(2, y++, -1, TERM_YELLOW, "Will be saved to your SDL config file on exit.");
        }
        Term_putstr(2, y++, -1, TERM_SLATE, "(direction keys to set, Return/Escape to accept)");
        
        /* Hilite current option */
        move_cursor(k + 3, 52);
        
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
        case '\n':
        case '\r':
        {
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
        
        case 't':
        case '5':
        case ' ':
        {
            /* Toggle or activate current option */
            if (k == 3) /* Fullscreen */
            {
                set_sdl_fullscreen(!get_sdl_fullscreen());
                settings_changed = true;
            }
            else if (k == 4) /* Tiles */
            {
                set_sdl_tiles(!get_sdl_tiles());
                settings_changed = true;
            }
            else if (k == 5) /* Enable Right Panes */
            {
                set_sdl_enable_right_panes(!get_sdl_enable_right_panes());
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 6) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(!get_sdl_enable_bottom_panes());
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 7) /* View Pane Configuration */
            {
                char pane_info[8192];
                get_sdl_config_info(pane_info, sizeof(pane_info));
                
                screen_save();
                Term_clear();
                Term_putstr(2, 1, -1, TERM_L_BLUE, "Pane Configuration Details");
                Term_putstr(2, 2, -1, TERM_WHITE, "==========================");
                
                int py = 4;
                char* pline = strtok(pane_info, "\n");
                while (pline != NULL && py < Term->hgt - 3)
                {
                    byte color = TERM_WHITE;
                    if (strstr(pline, "===") != NULL)
                        color = TERM_YELLOW;
                    else if (strstr(pline, "Pane ") == pline)
                        color = TERM_L_GREEN;
                    
                    Term_putstr(2, py++, -1, color, pline);
                    pline = strtok(NULL, "\n");
                }
                
                Term_putstr(2, Term->hgt - 2, -1, TERM_SLATE, "Edit your SDL config file to change pane layout.");
                Term_putstr(2, Term->hgt - 1, -1, TERM_L_BLUE, "Press any key to return...");
                (void)inkey();
                screen_load();
            }
            else if (k == 8) /* Save/Return */
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
            
            if (k == 0) /* Main View Scale */
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
            else if (k == 1) /* Aux View Font Size */
            {
                val = get_sdl_aux_view_font_size();
                if (val < 48)
                {
                    set_sdl_aux_view_font_size(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Margin */
            {
                val = get_sdl_margin();
                if (val < 20)
                {
                    set_sdl_margin(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Fullscreen */
            {
                set_sdl_fullscreen(true);
                settings_changed = true;
            }
            else if (k == 4) /* Tiles */
            {
                set_sdl_tiles(true);
                settings_changed = true;
            }
            else if (k == 5) /* Enable Right Panes */
            {
                set_sdl_enable_right_panes(true);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 6) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(true);
                settings_changed = true;
                sdl_apply_config();
            }
            break;
        }
        
        case 'n':
        case '4':
        {
            /* Decrease value or set to no */
            int val;
            
            if (k == 0) /* Main View Scale */
            {
                val = get_sdl_main_view_scale();
                if (val > 1)
                {
                    set_sdl_main_view_scale(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 1) /* Aux View Font Size */
            {
                val = get_sdl_aux_view_font_size();
                if (val > 8)
                {
                    set_sdl_aux_view_font_size(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Margin */
            {
                val = get_sdl_margin();
                if (val > 0)
                {
                    set_sdl_margin(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Fullscreen */
            {
                set_sdl_fullscreen(false);
                settings_changed = true;
            }
            else if (k == 4) /* Tiles */
            {
                set_sdl_tiles(false);
                settings_changed = true;
            }
            else if (k == 5) /* Enable Right Panes */
            {
                set_sdl_enable_right_panes(false);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 6) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(false);
                settings_changed = true;
                sdl_apply_config();
            }
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


int options_menu(int* highlight)
{
    int ch;
int options = 11; /* added sound option */
#ifdef DEBUG_CURSES
    options = 15;
#endif
    if (p_ptr->noscore)    
        options++;

    Term_putstr(2, 1, -1, TERM_WHITE, "Options and misc");

    Term_putstr(2, 3, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        "a) Set Keybinds");
    Term_putstr(2, 4, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        "b) Pane Settings");
    Term_putstr(2, 5, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        "c) Interface Options");
    Term_putstr(2, 6, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        "d) Visual Options");
    Term_putstr(2, 7, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        "e) Sound Options");
    Term_putstr(2, 8, -1, (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE,
        "f) Load a 'Pref' File");
    Term_putstr(2, 9, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE,
        "g) Append Options to a 'Pref' File");
    Term_putstr(2, 10, -1, (*highlight == 8) ? TERM_L_BLUE : TERM_WHITE,
        "h) Set Macros");
    Term_putstr(2, 11, -1, (*highlight == 9) ? TERM_L_BLUE : TERM_WHITE,
        "i) Set Colours");
    Term_putstr(2, 12, -1, (*highlight == 10) ? TERM_L_BLUE : TERM_WHITE,
        "j) Write a note");
    Term_putstr(2, 13, -1, (*highlight == 11) ? TERM_L_BLUE : TERM_WHITE,
        "k) Return to Game");

    if (p_ptr->noscore)
    {
        Term_putstr(2, 14, -1, (*highlight == 12) ? TERM_L_BLUE : TERM_WHITE,
            "l) Debugging Options");
    }

    /* Show product name and version on the bottom of the menu */
    {
        char verbuf[128];
        strnfmt(verbuf, sizeof(verbuf), "%s %s", VERSION_NAME, VERSION_STRING);
        Term_putstr(2, 17, -1, TERM_SLATE, verbuf);
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, 2 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if ((ch == 'a') || (ch == 'A'))
    {
        *highlight = 1;
        return (1);
    }

    if ((ch == 'b') || (ch == 'B'))
    {
        *highlight = 2;
        return (2);
    }

    if ((ch == 'c') || (ch == 'C'))
    {
        *highlight = 3;
        return (3);
    }

    if ((ch == 'd') || (ch == 'D'))
    {
        *highlight = 4;
        return (4);
    }

    if ((ch == 'e') || (ch == 'E'))
    {
        *highlight = 5;
        return (5);
    }

    if ((ch == 'f') || (ch == 'F'))
    {
        *highlight = 6;
        return (6);
    }

    if ((ch == 'g') || (ch == 'G'))
    {
        *highlight = 7;
        return (7);
    }

    if ((ch == 'h') || (ch == 'H'))
    {
        *highlight = 8;
        return (8);
    }

    if ((ch == 'i') || (ch == 'I'))
    {
        *highlight = 9;
        return (9);
    }

    if ((ch == 'j') || (ch == 'J'))
    {
        *highlight = 10;
        return (10);
    }

    if ((ch == 'k') || (ch == 'K') || (ch == ESCAPE) || (ch == 'q'))
    {
        /* Return to game (now letter 'k') */
        *highlight = 11;
        return (11);
    }

    if (p_ptr->noscore && ((ch == 'l') || (ch == 'L')))
    {
        /* Debugging options */
        *highlight = 12;
        return (12);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
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

    char ftmp[80];

    bool return_to_game = false;

    /* Clear any active banner before opening options */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

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
            do_cmd_keybinds();
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
            do_cmd_options_aux(SOUND_PAGE, "Sound Options");
            Term_clear();
            break;
        }
        case 6:
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(12);
            Term_clear();
            break;
        }
        case 7:
        {
            /* Prompt */
            Term_putstr(2, 14, -1, TERM_SLATE, "(Escape to cancel)");

            /* Prompt */
            prt("File: ", 12, 2);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
            {
                Term_clear();
                continue;
            }

            /* Dump the options */
            if (option_dump(ftmp))
            {
                /* Failure */
                msg_print("Failed!");
            }
            else
            {
                /* Success */
                msg_print("Done.");
            }

            Term_clear();
            break;
        }
        case 8:
        {
            do_cmd_macros();
            Term_clear();
            break;
        }
        case 9:
        {
            do_cmd_colors();
            Term_clear();
            break;
        }
        case 10:
        {
            do_cmd_note("", p_ptr->depth);
            Term_clear();
            break;
        }
        case 11:
        {
            /* Return to Game */
            return_to_game = true;
            Term_clear();
            break;
        }
        case 12:
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
    screen_load();
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
        {'o', NULL, "Open door / chest", "o", false},
        {'c', NULL, "Close door", "c", false},
        {'D', NULL, "Disarm trap / chest", "D", false},
        {'X', NULL, "Exchange places", "X", false},
        {'-', NULL, "Fletch arrows", "-", false},
        {'{', NULL, "Inscribe item", "{", false},
        {'a', NULL, "Activate staff", "a", false},
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
        char binding_buf[80];
        
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
        prt("Keybind Configuration", 1, 0);
        prt("Arrow to navigate, Enter to bind, Tab to switch groups, Escape to return", 2, 0);
        prt(showing_primary ? "Primary Commands: Essential for the gameplay" : "Supplementary Commands", 3, 0);
        
        /* List visible keybinds */
        display_end = *top_ptr + visible_rows;
        if (display_end > num_keybinds)
            display_end = num_keybinds;
        for (i = *top_ptr; i < display_end; i++)
        {
            int entry_row = list_start_row + (i - *top_ptr);
            describe_action_bindings(mode, &keybinds[i], binding_buf, sizeof(binding_buf));

            /* Display the keybind */
            if (i == highlight)
            {
                /* Highlighted */
                c_prt(TERM_L_BLUE, format("%-28s -> %s", keybinds[i].key_name, binding_buf), 
                      entry_row, 2);
            }
            else
            {
                /* Normal */
                prt(format("%-28s -> %s", keybinds[i].key_name, binding_buf), 
                    entry_row, 2);
            }
        }
        
        /* Clear any leftover rows */
        for (i = display_end; i < *top_ptr + visible_rows; i++)
        {
            row = list_start_row + (i - *top_ptr);
            prt("                                        ", row, 2);
        }
        
        /* Instructions at bottom */
        prt(format("Press 's' to save keybinds to %s", default_file), list_start_row + visible_rows + 1, 2);
        prt("Press 'r' to reset selected keybind to default", list_start_row + visible_rows + 2, 2);
        if (dirty)
            c_prt(TERM_YELLOW, "Unsaved changes", list_start_row + visible_rows + 3, 2);
        else
            prt("                    ", list_start_row + visible_rows + 3, 2);
        
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
            int entry_row = list_start_row + (highlight - *top_ptr);

            /* Clear the action area */
            prt("                                                              ", 
                entry_row, 2);
            
            /* Prompt for new binding */
            strnfmt(prompt, sizeof(prompt), "Press key to use for %s (Escape to cancel):",
                keybinds[highlight].key_name);
            c_prt(TERM_YELLOW, prompt, entry_row, 2);
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
            prt("                                                              ", list_start_row + visible_rows + 1, 2);
            prt("File: ", list_start_row + visible_rows + 1, 2);
            
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
        /* Clear screen */
        Term_clear();

        /* Describe */
        prt("Interact with Macros", 2, 0);

        /* Describe that action */
        prt("Current action (if any) shown below:", 20, 0);

        /* Analyze the current action */
        ascii_to_text(tmp, sizeof(tmp), macro_buffer);

        /* Display the current action */
        prt(tmp, 22, 0);

        /* Selections */
        prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_MACROS
        prt("(2) Append macros to a file", 5, 5);
        prt("(3) Query a macro", 6, 5);
        prt("(4) Create a macro", 7, 5);
        prt("(5) Remove a macro", 8, 5);
        prt("(6) Append keymaps to a file", 9, 5);
        prt("(7) Query a keymap", 10, 5);
        prt("(8) Create a keymap", 11, 5);
        prt("(9) Remove a keymap", 12, 5);
        prt("(0) Enter a new action", 13, 5);
#endif /* ALLOW_MACROS */

        /* Prompt */
        prt("Command: ", 16, 0);

        /* Get a command */
        ch = inkey();

        /* Leave */
        if (ch == ESCAPE)
            break;

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(16);
        }

#ifdef ALLOW_MACROS

        /* Save macros */
        else if (ch == '2')
        {
            char ftmp[80];

            /* Prompt */
            prt("Command: Append macros to a file", 16, 0);

            /* Prompt */
            prt("File: ", 18, 0);

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
            prt("Command: Query a macro", 16, 0);

            /* Prompt */
            prt("Trigger: ", 18, 0);

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
                prt(tmp, 22, 0);

                /* Prompt */
                msg_print("Found a macro.");
            }
        }

        /* Create a macro */
        else if (ch == '4')
        {
            /* Prompt */
            prt("Command: Create a macro", 16, 0);

            /* Prompt */
            prt("Trigger: ", 18, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Clear */
            clear_from(20);

            /* Prompt */
            prt("Action: ", 20, 0);

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
            prt("Command: Remove a macro", 16, 0);

            /* Prompt */
            prt("Trigger: ", 18, 0);

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
            prt("Command: Append keymaps to a file", 16, 0);

            /* Prompt */
            prt("File: ", 18, 0);

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
            prt("Command: Query a keymap", 16, 0);

            /* Prompt */
            prt("Keypress: ", 18, 0);

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
                prt(tmp, 22, 0);

                /* Prompt */
                msg_print("Found a keymap.");
            }
        }

        /* Create a keymap */
        else if (ch == '8')
        {
            /* Prompt */
            prt("Command: Create a keymap", 16, 0);

            /* Prompt */
            prt("Keypress: ", 18, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Clear */
            clear_from(20);

            /* Prompt */
            prt("Action: ", 20, 0);

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
            prt("Command: Remove a keymap", 16, 0);

            /* Prompt */
            prt("Keypress: ", 18, 0);

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
            prt("Command: Enter a new action", 16, 0);

            /* Go to the correct location */
            Term_gotoxy(0, 22);

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
    bool know_all = cheat_know || p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT];

    /* Get a list of x_char in this group */
    byte group_tval = object_group_tval[grp_cur];

    /*make a list of artefacts not found*/
    /* Allocate the "object_idx" array */
    okay = mem_alloc_array(z_info->art_max, bool);

    /* Default first,  */
    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        /*start with false*/
        okay[i] = false;

        /* Skip "empty" artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Skip "unfound" artefacts, unless in wizard mode or with Enchantment
         * or cheating */
        if (!know_all && !p_ptr->wizard && !a_ptr->found_num)
            continue;

        /* Skip "ungenerated" artefacts, unless with Lore Mastery or cheating */
        if (!know_all && !a_ptr->cur_num)
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

/*
 * Display the object groups.
 */
static void display_group_list(int col, int row, int wid, int per_page,
    int grp_idx[], cptr group_text[], int grp_cur, int grp_top)
{
    int i;

    /* Display lines until done */
    for (i = 0; i < per_page && (grp_idx[i] >= 0); i++)
    {
        /* Get the group index */
        int grp = grp_idx[grp_top + i];

        /* Choose a color */
        byte attr = (grp_top + i == grp_cur) ? TERM_L_BLUE : TERM_WHITE;

        /* Erase the entire line */
        Term_erase(col, row + i, wid);

        /* Display the group label */
        c_put_str(attr, group_text[grp], row + i, col);
    }
}

static bool supply_kind_matches(int group, int tval, int sval)
{
    switch (group)
    {
    case SUPPLY_GROUP_HERBS:
        return (tval == TV_FOOD) && (sval <= SV_FOOD_SICKNESS);
    case SUPPLY_GROUP_POTIONS:
        return (tval == TV_POTION);
    case SUPPLY_GROUP_GEMS:
        return (tval == TV_GEM);
    default:
        return false;
    }
}

static bool supply_item_matches(int group, const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    return supply_kind_matches(group, o_ptr->tval, o_ptr->sval);
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

        if ((o_ptr->tval == TV_FOOD) && (o_ptr->sval <= SV_FOOD_SICKNESS))
            totals[SUPPLY_GROUP_HERBS] += o_ptr->number;
        else if (o_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += o_ptr->number;
        else if (o_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += o_ptr->number;
    }

    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        if (!s_ptr || !s_ptr->k_idx)
            continue;

        if ((s_ptr->tval == TV_FOOD) && (s_ptr->sval <= SV_FOOD_SICKNESS))
            totals[SUPPLY_GROUP_HERBS] += s_ptr->number;
        else if (s_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += s_ptr->number;
        else if (s_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += supplies_entry_units(i);
    }
}

static bool supply_kind_is_known(const object_kind* k_ptr)
{
    if (!k_ptr)
        return false;

    if (cheat_know || p_ptr->wizard)
        return true;

    return k_ptr->aware || k_ptr->everseen || k_ptr->tried;
}

static int collect_supply_entries(int group_idx, supply_list_entry entries[])
{
    int count = 0;
    int capacity = z_info->k_max;
    int i;

    if (!entries)
        return 0;

    memset(entries, 0, sizeof(supply_list_entry) * capacity);

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

        int value = (s_ptr->tval == TV_GEM) ? supplies_entry_units(i) : s_ptr->number;

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
            count++;
        }
    }

    /* Add known kinds even when none are carried */
    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        int j;

        if (!k_ptr->name)
            continue;

        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (!supply_kind_is_known(k_ptr))
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
            count++;
        }
    }

    if (count < capacity)
    {
        entries[count].k_idx = -1;
        entries[count].item_idx = -1;
        entries[count].total = 0;
        entries[count].supply_idx = -1;
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

        default:
            return TERM_WHITE;
    }
}

static void display_supply_group_list(int col, int row, int wid, int per_page,
    int grp_idx[], int grp_cur, int grp_top, int group_totals[])
{
    int i;
    int total_col = col + wid - 3;

    for (i = 0; i < per_page && (grp_idx[i] >= 0); i++)
    {
        int grp = grp_idx[grp_top + i];
        byte base_color;
        byte attr;
        char buf[8];

        /* Assign color based on group type */
        switch (grp)
        {
            case SUPPLY_GROUP_HERBS:   base_color = TERM_GREEN; break;
            case SUPPLY_GROUP_POTIONS: base_color = TERM_VIOLET;  break;
            case SUPPLY_GROUP_GEMS:    base_color = TERM_BLUE;    break;
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
        c_put_str(attr, supply_group_text[grp], row + i, col);

        strnfmt(buf, sizeof(buf), "%3d", group_totals[grp]);
        c_put_str(attr, buf, row + i, total_col);
    }
}

static void display_supply_list(int col, int row, int per_page,
    supply_list_entry entries[], int entry_cnt, int entry_cur, int entry_top,
    int count_col, int sym_col, int current_group, int column)
{
    int i;

    (void)current_group; /* Not used since we color by specific item type now */

    for (i = 0; i < per_page; i++)
    {
        int idx = entry_top + i;
        int y = row + i;

        Term_erase(col, y, 255);

        if (idx >= entry_cnt)
            continue;

        supply_list_entry* entry = &entries[idx];
        object_type* o_ptr;
        object_type fake;
        object_kind* k_ptr;
        bool aware;
        byte base_attr, cursor_attr, attr;
        byte sym_attr;
        char sym_char;
        char name[80];
        char count_buf[8];

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
        attr = (column == 1 && idx == entry_cur) ? cursor_attr : base_attr;

        if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
        {
            o_ptr = &inventory[entry->item_idx];
        }
        else
        {
            object_wipe(&fake);
            object_prep(&fake, entry->k_idx);
            if (aware)
                fake.ident |= IDENT_KNOWN;
            fake.number = (entry->total > 0) ? entry->total : 1;
            o_ptr = &fake;
        }

        object_desc(name, sizeof(name), o_ptr, true, 3);
        c_prt(attr, name, y, col);

        strnfmt(count_buf, sizeof(count_buf), "x%-3d", entry->total);
        c_put_str(attr, count_buf, y, count_col);

        sym_attr = object_attr(o_ptr);
        sym_char = object_char(o_ptr);
        Term_putch(sym_col, y, sym_attr, sym_char);
        if (use_bigtile)
        {
            if (sym_attr & 0x80)
                Term_putch(sym_col + 1, y, 255, -1);
            else
                Term_putch(sym_col + 1, y, 0, ' ');
        }
    }

    for (; i < per_page; i++)
    {
        Term_erase(col, row + i, 255);
    }
}

/*
 * Move the cursor in a browser window
 */
static void browser_cursor(char ch, int* column, int* grp_cur, int grp_cnt,
    int* list_cur, int list_cnt)
{
    int d;
    int col = *column;
    int grp = *grp_cur;
    int list = *list_cur;

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
            grp += ddy[d] * BROWSER_ROWS;

            /* Verify */
            if (grp >= grp_cnt)
                grp = grp_cnt - 1;
            if (grp < 0)
                grp = 0;
            if (grp != old_grp)
                list = 0;
        }

        /* Browse sub-list list */
        else
        {
            /* Move up or down */
            list += ddy[d] * BROWSER_ROWS;

            /* Verify */
            if (list >= list_cnt)
                list = list_cnt - 1;
            if (list < 0)
                list = 0;
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
        grp += ddy[d];

        /* Verify */
        if (grp >= grp_cnt)
            grp = grp_cnt - 1;
        if (grp < 0)
            grp = 0;
        if (grp != old_grp)
            list = 0;
    }

    /* Browse sub-list list */
    else
    {
        /* Move up or down */
        list += ddy[d];

        /* Verify */
        if (list >= list_cnt)
            list = list_cnt - 1;
        if (list < 0)
            list = 0;
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

    // add the abilities
    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    /*identify it*/
    object_known(o_ptr);

    /*make it a spoiler item*/
    o_ptr->ident |= IDENT_SPOIL;

    /* Hack -- extract the "cursed" flag */
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE))
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
 * Display the objects in a group.
 */
static void display_artefact_list(int col, int row, int per_page,
    int object_idx[], int object_cur, int object_top)
{
    int i;
    char o_name[80];
    object_type* i_ptr;
    object_type object_type_body;

    /* Display lines until done */
    for (i = 0; i < per_page && object_idx[i]; i++)
    {
        /* Get the object index */
        int a_idx = object_idx[object_top + i];

        /* Choose a color */
        byte attr = TERM_WHITE;
        byte cursor = TERM_L_BLUE;
        attr = ((i + object_top == object_cur) ? cursor : attr);

        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        /* Make fake artefact */
        prepare_fake_artefact(i_ptr, a_idx);

        /* Get its name */
        object_desc(o_name, sizeof(o_name), i_ptr, true, 0);

        /* Display the name */
        c_prt(attr, o_name, row + i, col);

        if (cheat_know)
        {
            artefact_type* a_ptr = &a_info[a_idx];

            c_prt(attr, format("%3d", a_idx), row + i, 68);
            c_prt(attr, format("%3d", a_ptr->level), row + i, 72);
            c_prt(attr, format("%3d", a_ptr->rarity), row + i, 76);
        }
    }

    /* Clear remaining lines */
    for (; i < per_page; i++)
    {
        Term_erase(col, row + i, 255);
    }
}

/*
 * Display known artefacts
 */
void do_cmd_knowledge_artefacts(void)
{
    int i, len, max;
    int grp_cur, grp_top;
    int artefact_old, artefact_cur, artefact_top;
    int grp_cnt, grp_idx[100];
    int artefact_cnt;
    int* artefact_idx;

    int column = 0;
    bool flag;
    bool redraw;

    log_debug("Player opened artifacts knowledge screen");

    /* Allocate the "artefact_idx" array */
    artefact_idx = mem_alloc_array(z_info->art_max, int);

    max = 0;
    grp_cnt = 0;

    /* Check every group */
    for (i = 0; object_group_text[i] != NULL; i++)
    {
        /* Measure the label */
        len = strlen(object_group_text[i]);

        /* Save the maximum length */
        if (len > max)
            max = len;

        /* See if artefact are known */
        if (collect_artefacts(i, artefact_idx))
        {
            /* Build a list of groups with known artefacts */
            grp_idx[grp_cnt++] = i;
        }
    }

    /* Terminate the list */
    grp_idx[grp_cnt] = -1;

    grp_cur = grp_top = 0;
    artefact_cur = artefact_top = 0;
    artefact_old = -1;

    flag = false;
    redraw = true;

    while (!flag)
    {
        char ch;

        if (redraw)
        {
            clear_from(0);

            prt("Knowledge - Artefacts", 2, 0);
            prt("Group", 4, 0);
            prt("Name", 4, max + 3);

            if (cheat_know)
            {
                prt("Idx", 4, 68);
                prt("Dep", 4, 72);
                prt("Rar", 4, 76);
            }

            for (i = 0; i < 78; i++)
            {
                Term_putch(i, 5, TERM_L_DARK, '=');
            }

            for (i = 0; i < BROWSER_ROWS; i++)
            {
                Term_putch(max + 1, 6 + i, TERM_L_DARK, '|');
            }

            redraw = false;
        }

        /* Scroll group list */
        if (grp_cur < grp_top)
            grp_top = grp_cur;
        if (grp_cur >= grp_top + BROWSER_ROWS)
            grp_top = grp_cur - BROWSER_ROWS + 1;

        /* Scroll artefact list */
        if (artefact_cur < artefact_top)
            artefact_top = artefact_cur;
        if (artefact_cur >= artefact_top + BROWSER_ROWS)
            artefact_top = artefact_cur - BROWSER_ROWS + 1;

        /* Display a list of object groups */
        display_group_list(0, 6, max, BROWSER_ROWS, grp_idx, object_group_text,
            grp_cur, grp_top);

        /* Get a list of objects in the current group */
        artefact_cnt = collect_artefacts(grp_idx[grp_cur], artefact_idx);

        /* Display a list of objects in the current group */
        display_artefact_list(
            max + 3, 6, BROWSER_ROWS, artefact_idx, artefact_cur, artefact_top);

        /* Prompt */
        Term_putstr(1, 23, -1, TERM_SLATE, "<dir>   recall   ESC");
        Term_putstr(1, 23, -1, TERM_L_WHITE, "<dir>");
        Term_putstr(9, 23, -1, TERM_L_WHITE, "r");
        Term_putstr(18, 23, -1, TERM_L_WHITE, "ESC");

        /* The "current" object changed */
        if (artefact_old != artefact_idx[artefact_cur])
        {
            /* Hack -- handle stuff */
            handle_stuff();

            /* Remember the "current" object */
            artefact_old = artefact_idx[artefact_cur];
        }

        if (!column)
        {
            Term_gotoxy(0, 6 + (grp_cur - grp_top));
        }
        else
        {
            Term_gotoxy(max + 3, 6 + (artefact_cur - artefact_top));
        }

        ch = inkey();

        switch (ch)
        {
        case ESCAPE:
        {
            flag = true;
            break;
        }

        case 'R':
        case 'r':
        {
            /* Recall on screen */
            desc_art_fake(artefact_idx[artefact_cur]);

            redraw = true;
            break;
        }

        default:
        {
            /* Move the cursor */
            browser_cursor(
                ch, &column, &grp_cur, grp_cnt, &artefact_cur, artefact_cnt);
            break;
        }
        }
    }

    /* XXX XXX Free the "object_idx" array */
    mem_free_null(artefact_idx);
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
    Term_erase(0, 23, 255);

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        c_put_str(TERM_L_BLUE,
            format("Total Creatures Slain: %d. ", slay_count), 23, col + 2);
    }
    else
    {
        c_put_str(TERM_L_BLUE,
            format("Known Uniques: %d, Slain Uniques: %d.", known_uniques,
                dead_uniques),
            23, col + 2);
    }
}

/*
 * Display known monsters.
 */
void do_cmd_knowledge_monsters(void)
{
    int i, len, max;
    int grp_cur, grp_top;
    int mon_cur, mon_top;
    int grp_cnt, grp_idx[100];
    monster_list_entry* mon_idx;
    int monster_count;

    int column = 0;
    bool flag;
    bool redraw;

    /* Allocate the "mon_idx" array */
    mon_idx = mem_alloc_array(z_info->r_max, monster_list_entry);

    max = 0;
    grp_cnt = 0;

    /* Check every group */
    for (i = 0; monster_group_text[i] != NULL; i++)
    {
        /* Measure the label */
        len = strlen(monster_group_text[i]);

        /* Save the maximum length */
        if (len > max)
            max = len;

        /* See if any monsters are known */
        if ((monster_group_char[i] == ((char*)-1L))
            || collect_monsters(i, mon_idx, 0x01))
        {
            /* Build a list of groups with known monsters */
            grp_idx[grp_cnt++] = i;
        }
    }

    /* Terminate the list */
    grp_idx[grp_cnt] = -1;

    grp_cur = grp_top = 0;
    mon_cur = mon_top = 0;

    flag = false;
    redraw = true;

    while (!flag)
    {
        char ch;

        if (redraw)
        {
            clear_from(0);

            prt("Knowledge - Monsters", 2, 0);
            prt("Group", 4, 0);
            prt("Name", 4, max + 3);
            if (cheat_know)
                prt("Idx", 4, 60);
            prt("Sym   Kills", 4, 67);

            for (i = 0; i < 78; i++)
            {
                Term_putch(i, 5, TERM_L_DARK, '=');
            }

            for (i = 0; i < BROWSER_ROWS; i++)
            {
                Term_putch(max + 1, 6 + i, TERM_L_DARK, '|');
            }

            redraw = false;
        }

        /* Scroll group list */
        if (grp_cur < grp_top)
            grp_top = grp_cur;
        if (grp_cur >= grp_top + BROWSER_ROWS)
            grp_top = grp_cur - BROWSER_ROWS + 1;

        /* Scroll monster list */
        if (mon_cur < mon_top)
            mon_top = mon_cur;
        if (mon_cur >= mon_top + BROWSER_ROWS)
            mon_top = mon_cur - BROWSER_ROWS + 1;

        /* Display a list of monster groups */
        display_group_list(0, 6, max, BROWSER_ROWS, grp_idx, monster_group_text,
            grp_cur, grp_top);

        /* Get a list of monsters in the current group */
        monster_count = collect_monsters(grp_idx[grp_cur], mon_idx, 0x00);

        /* Display a list of monsters in the current group */
        display_monster_list(
            max + 3, 6, BROWSER_ROWS, mon_idx, mon_cur, mon_top, grp_cur);

        /* Track selected monster, to enable recall in sub-win*/
        p_ptr->monster_race_idx = mon_idx[mon_cur].r_idx;

        /* Prompt */
        Term_putstr(1, 23, -1, TERM_SLATE, "<dir>   recall   ESC");
        Term_putstr(1, 23, -1, TERM_L_WHITE, "<dir>");
        Term_putstr(9, 23, -1, TERM_L_WHITE, "r");
        Term_putstr(18, 23, -1, TERM_L_WHITE, "ESC");

        /* Hack -- handle stuff */
        handle_stuff();

        if (!column)
        {
            Term_gotoxy(0, 6 + (grp_cur - grp_top));
        }
        else
        {
            Term_gotoxy(max + 3, 6 + (mon_cur - mon_top));
        }

        ch = inkey();

        switch (ch)
        {
        case ESCAPE:
        {
            flag = true;
            break;
        }

        case 'R':
        case 'r':
        {
            /* Recall on screen */
            if (mon_idx[mon_cur].r_idx)
            {
                screen_roff(mon_idx[mon_cur].r_idx, NULL);

                (void)inkey();

                redraw = true;
            }
            break;
        }

        default:
        {
            /* Move the cursor */
            browser_cursor(
                ch, &column, &grp_cur, grp_cnt, &mon_cur, monster_count);

            /*Update to a new monster*/
            p_ptr->window |= (PW_MONSTER);

            break;
        }
        }
    }

    /* XXX XXX Free the "mon_idx" array */
    mem_free_null(mon_idx);
}

/*
 * Add a pval so the object descriptions don't look strange*
 */
void apply_magic_fake(object_type* o_ptr)
{
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

        /* Ring of Protection */
        case SV_RING_PROTECTION:
        {
            /* Bonus to protection */
            o_ptr->pd = 1;
            if (o_ptr->ps < 1)
                o_ptr->ps = 1;

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
    supply_list_entry* entries;
    int grp_cur = 0;
    int grp_top = 0;
    int entry_cur = 0;
    int entry_top = 0;
    int column = 0;
    bool flag = false;
    bool redraw = true;
    const int count_col = 68;
    const int sym_col = 75;
    supply_menu_action forced_action = SUPPLY_MENU_ACTION_NONE;
    bool hotkey_mode = false;
    bool acted = false;
    bool refresh_after_close = false;

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

    entries = mem_alloc_array(z_info->k_max, supply_list_entry);

    screen_save();

    while (!flag)
    {
        int entry_cnt;

        compute_supply_group_totals(group_totals);

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
            if (entry_cur >= entry_top + BROWSER_ROWS)
                entry_top = entry_cur - BROWSER_ROWS + 1;
            if (entry_top < 0)
                entry_top = 0;
        }

        if (grp_cur < grp_top)
            grp_top = grp_cur;
        if (grp_cur >= grp_top + BROWSER_ROWS)
            grp_top = grp_cur - BROWSER_ROWS + 1;
        if (grp_top < 0)
            grp_top = 0;

        if (redraw)
        {
            clear_from(0);

            prt("Supplies - Herbs, Potions, Gems", 2, 0);
            prt("Group", 4, 0);
            prt("Name", 4, max + 3);
            prt("Qty", 4, count_col);
            prt("Sym", 4, sym_col);

            for (i = 0; i < 78; i++)
                Term_putch(i, 5, TERM_L_DARK, '=');

            for (i = 0; i < BROWSER_ROWS; i++)
                Term_putch(max + 1, 6 + i, TERM_L_DARK, '|');

            redraw = false;
        }

        display_supply_group_list(0, 6, max, BROWSER_ROWS, grp_idx, grp_cur, grp_top, group_totals);
        display_supply_list(max + 3, 6, BROWSER_ROWS, entries, entry_cnt, entry_cur, entry_top, count_col, sym_col, grp_idx[grp_cur], column);

        /* Bottom bar: grey text with white first letters */
        Term_erase(0, 23, 255);
        c_put_str(TERM_L_WHITE, "<dir>", 23, 1);
        c_put_str(TERM_L_DARK, "   ", 23, 6);
        c_put_str(TERM_L_WHITE, "r", 23, 9);
        c_put_str(TERM_L_DARK, "ecall   ", 23, 10);
        c_put_str(TERM_L_WHITE, "u", 23, 18);
        c_put_str(TERM_L_DARK, "/", 23, 19);
        c_put_str(TERM_L_WHITE, "Space", 23, 20);
        c_put_str(TERM_L_DARK, "   ", 23, 25);
        c_put_str(TERM_L_WHITE, "d", 23, 28);
        c_put_str(TERM_L_DARK, "rop   ", 23, 29);
        c_put_str(TERM_L_WHITE, "ESC", 23, 37);

        if (!column)
            Term_gotoxy(0, 6 + (grp_cur - grp_top));
        else if (entry_cnt)
            Term_gotoxy(max + 3, 6 + (entry_cur - entry_top));
        else
            Term_gotoxy(0, 6 + (grp_cur - grp_top));

        char ch = inkey();

        if ((ch == '\r' || ch == '\n') && column && entry_cnt)
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
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    object_info_screen(&inventory[entry->item_idx]);
                    redraw = true;
                }
                else if (entry->k_idx >= 0)
                {
                    object_kind* k_ptr = &k_info[entry->k_idx];
                    if (k_ptr->aware)
                    {
                        desc_obj_fake(entry->k_idx);
                        redraw = true;
                    }
                    else
                    {
                        bell("You have not identified that yet.");
                        msg_print("You have not identified that yet.");
                    }
                }
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
            browser_cursor(ch, &column, &grp_cur, grp_cnt, &entry_cur, entry_cnt);
            break;
        }
    }

    mem_free_null(entries);
    screen_load();
    Term_erase(0, 23, 255);

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
    int i, len, max;
    int grp_cur, grp_top, grp_max;
    int object_old, object_cur, object_top;
    int grp_cnt, grp_idx[100];
    int object_cnt;
    object_list_entry* object_idx;

    int column = 0;
    bool flag;
    bool redraw;

    max = 0;
    grp_max = 0;
    grp_cnt = 0;

    /* Check every group */
    for (i = 0; object_group_text[i] != NULL; i++)
    {
        /* Measure the label */
        len = strlen(object_group_text[i]);

        /* Save the maximum length */
        if (len > max)
            max = len;

        /* See if any monsters are known */
        object_cnt = collect_objects(i, NULL);
        if (object_cnt)
        {
            /* Build a list of groups with known monsters */
            grp_idx[grp_cnt++] = i;
        }

        if (object_cnt > grp_max)
            grp_max = object_cnt;
    }

    /* Terminate the list */
    grp_idx[grp_cnt] = -1;

    /* Allocate the "object_idx" array */
    object_idx = mem_alloc_array(1 + grp_max, object_list_entry);

    grp_cur = grp_top = 0;
    object_cur = object_top = 0;
    object_old = -1;

    flag = false;
    redraw = true;

    while (!flag)
    {
        char ch;

        if (redraw)
        {
            clear_from(0);

            prt("Knowledge - Objects", 2, 0);
            prt("Group", 4, 0);
            prt("Name", 4, max + 3);
            if (cheat_know)
                prt("Idx", 4, 70);
            prt("Sym", 4, 75);

            for (i = 0; i < 78; i++)
            {
                Term_putch(i, 5, TERM_L_DARK, '=');
            }

            for (i = 0; i < BROWSER_ROWS; i++)
            {
                Term_putch(max + 1, 6 + i, TERM_L_DARK, '|');
            }

            redraw = false;
        }

        /* Scroll group list */
        if (grp_cur < grp_top)
            grp_top = grp_cur;
        if (grp_cur >= grp_top + BROWSER_ROWS)
            grp_top = grp_cur - BROWSER_ROWS + 1;

        /* Scroll monster list */
        if (object_cur < object_top)
            object_top = object_cur;
        if (object_cur >= object_top + BROWSER_ROWS)
            object_top = object_cur - BROWSER_ROWS + 1;

        /* Display a list of object groups */
        display_group_list(0, 6, max, BROWSER_ROWS, grp_idx, object_group_text,
            grp_cur, grp_top);

        /* Get a list of objects in the current group */
        object_cnt = collect_objects(grp_idx[grp_cur], object_idx);

        /* Display a list of objects in the current group */
        display_object_list(
            max + 3, 6, BROWSER_ROWS, object_idx, object_cur, object_top);

        /* Prompt */
        Term_putstr(1, 23, -1, TERM_SLATE, "<dir>   recall   ESC");
        Term_putstr(1, 23, -1, TERM_L_WHITE, "<dir>");
        Term_putstr(9, 23, -1, TERM_L_WHITE, "r");
        Term_putstr(18, 23, -1, TERM_L_WHITE, "ESC");

        /* Mega Hack -- track this monster race */
        if (object_cnt)
            object_kind_track(object_idx[object_cur].idx);

        /* The "current" object changed */
        if (object_old != object_cur)
        {
            /* Hack -- handle stuff */
            handle_stuff();

            /* Remember the "current" object */
            object_old = object_cur;
        }

        if (!column)
        {
            Term_gotoxy(0, 6 + (grp_cur - grp_top));
        }
        else
        {
            Term_gotoxy(max + 3, 6 + (object_cur - object_top));
        }

        ch = inkey();

        switch (ch)
        {
        case ESCAPE:
        {
            flag = true;
            break;
        }

        case 'R':
        case 'r':
        {
            object_list_entry* obj = &object_idx[object_cur];
            if (obj->type == OBJ_NORMAL && k_info[obj->idx].aware)
            {
                /* Recall on screen */
                desc_obj_fake(obj->idx);

                redraw = true;
            }
            break;
        }

        default:
        {
            /* Move the cursor */
            browser_cursor(
                ch, &column, &grp_cur, grp_cnt, &object_cur, object_cnt);
            break;
        }
        }
    }

    /* XXX XXX Free the "object_idx" array */
    mem_free_null(object_idx);
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
        prt("(1) Display known artefacts", 4, 5);
        prt("(2) Display known monsters", 5, 5);
        prt("(3) Display known objects", 6, 5);
        prt("(4) Display supplies overview", 7, 5);
        prt("(5) Display names of the fallen", 8, 5);
        prt("(6) Display kill counts", 9, 5);

        /*allow the player to see the notes taken if that option is selected*/
        c_put_str(TERM_WHITE, "(7) Display character notes file", 10, 5);
        prt("(8) Display oath status", 11, 5);

        /* Prompt */
        prt("Command: ", 13, 0);

        /* Prompt */
        ch = inkey();

        /* Done */
        if (ch == ESCAPE)
            break;

        /* Artefacts */
        if (ch == '1')
        {
            do_cmd_knowledge_artefacts();
        }

        /* Uniques */
        else if (ch == '2')
        {
            do_cmd_knowledge_monsters();
        }

        /* Objects */
        else if (ch == '3')
        {
            do_cmd_knowledge_objects();
        }

        /* Scores */
        else if (ch == '4')
        {
            do_cmd_knowledge_supplies(NULL);
        }

        /* Scores */
        else if (ch == '5')
        {
            show_scores_interactive(true);
        }

        /* Kill counts */
        else if (ch == '6')
        {
            do_cmd_knowledge_kills();
        }

        /* Notes file, if one exists */
        else if (ch == '7')
        {
            /* Spawn */
            do_cmd_knowledge_notes();
        }

        /* Oath status */
        else if (ch == '8')
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
    char direction[12];
    char name[60];
};

void show_nearby_monsters(bool line_of_sight_only)
{
    view_monster_data_line lines[MAX_VIEW_LINES];

    int i, j;
    int col;
    int longest_name_length = 0;
    
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

        lines[j].monster_character = monster_char(r_ptr);
        lines[j].monster_color = monster_attr(r_ptr);

        lines[j].distance
            = distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);

        strncpy(lines[j].name, m_name, sizeof(lines[j].name));

        j++;
    }

    col = 79 - longest_name_length - sizeof(lines[j].direction)
        - sizeof(lines[j].stance) - 9;
    col = MAX(0, col);

    for (i = 0; i < j; ++i)
    {
        int distance_color;
        char monster_char[2];

        monster_char[0] = lines[i].monster_character;
        monster_char[1] = '\0';

        if (lines[i].distance < 5)
            distance_color = TERM_WHITE;
        else if (lines[i].distance < 10)
            distance_color = TERM_L_WHITE;
        else
            distance_color = TERM_L_DARK;

        /* Clear the line */
        prt("", i + 1, col);

        c_put_str(lines[i].monster_color, monster_char, i + 1, col + 2);
        if (use_bigtile)
        {
            Term_putch(col + 3, i + 1, 255, -1);
        }
        c_put_str(distance_color, lines[i].direction, i + 1, col + 6);
        c_put_str(TERM_WHITE, lines[i].name, i + 1,
            col + sizeof(lines[j].direction) + 6);
        c_put_str(lines[i].alert_color, lines[i].stance, i + 1,
            col + sizeof(lines[j].direction) + longest_name_length + 8);
    }

    if (j)
    {
        prt("", j + 1, col);
    }
    else
    {
        prt("", 1, 40);
        c_put_str(TERM_WHITE, "No visible monsters.", 1, 50);
        prt("", 2, 40);
        prt("", 3, 40);
    }
}

void show_nearby_objects(bool line_of_sight_only)
{
    view_object_data_line lines[MAX_VIEW_LINES];

    int i, j;
    int col;
    int longest_name_length = 0;
    
    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_lines = MIN(MAX_VIEW_LINES, term_hgt - 3); /* Leave space for header and footer */

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    j = 0;
    for (i = 0; i < temp_n; i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        object_type* o_ptr = &o_list[o_idx];
        char o_name[60];
        int name_length;

        if (j >= max_lines)
            break;
        if (!player_can_see_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(lines[j].direction, '\0', sizeof(lines[j].direction));
        memset(lines[j].name, '\0', sizeof(lines[j].name));
        memset(o_name, '\0', sizeof(o_name));

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        name_length = strlen(o_name);

        longest_name_length = MAX(longest_name_length, name_length);

        write_direction_from_player_to_buffer(temp_y[i], temp_x[i],
            lines[j].direction, sizeof(lines[j].direction));

        lines[j].distance
            = distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);

        if (strlen(lines[j].direction) == 0)
            strcpy(lines[j].direction, "underfoot"); 

        lines[j].object_character = object_char(o_ptr);
        lines[j].object_color = object_attr(o_ptr);

        strncpy(lines[j].name, o_name, sizeof(lines[j].name));

        j++;
    }

    col = 79 - longest_name_length - sizeof(lines[j].direction) - 9;
    col = MAX(0, col);

    prt("", 1, col);

    for (i = 0; i < j; ++i)
    {
        int distance_color;

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
        prt("", i + 1, col);

        c_put_str(lines[i].object_color, o_char, i + 1, col + 2);
        if (use_bigtile)
        {
            Term_putch(col + 3, i + 1, 255, -1);
        }
        c_put_str(distance_color, lines[i].direction, i + 1, col + 6);
        c_put_str(TERM_WHITE, lines[i].name, i + 1,
            col + sizeof(lines[j].direction) + 6);
    }

    if (j)
    {
        prt("", j + 1, col);
    }
    else
    {
        prt("", 1, 40);
        c_put_str(TERM_WHITE, "No visible objects.", 1, 50);
        prt("", 2, 40);
        prt("", 3, 40);
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
        log_debug("sidebar_find_stats_pos: stats at position 0 for '%s' - treating as name", s);
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
        log_debug("sidebar_compact_name: max_len < 1 for src='%s'", src);
        return;
    }

    if (src_len <= max_len)
    {
        strnfmt(dest, dest_sz, "%s", src);
        log_debug("sidebar_compact_name: no shortening needed src='%s' len=%d max=%d", src, src_len, max_len);
        return;
    }

    int stats_pos = sidebar_find_stats_pos(src);
    log_debug("sidebar_compact_name: shortening src='%s' len=%d max=%d stats_pos=%d", src, src_len, max_len, stats_pos);

    if (stats_pos < 0)
    {
        strnfmt(dest, dest_sz, "%.*s", max_len, src);
        sidebar_trim_spaces(dest);
        log_debug("sidebar_compact_name: no stats segment, result='%s'", dest);
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
        log_debug("sidebar_compact_name: long stats, showing truncated name+stats result='%s'", dest);
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
    log_debug("sidebar_compact_name: combined result='%s'", dest);
}

/*
 * Show unified sidebar with monsters and objects
 */
void show_unified_sidebar(unified_look_state* state)
{
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
            
            /* Skip monsters that are not visible to the player */
            if (!m_ptr->ml) continue;
            
            /* Generate monster name without articles using race name function */
            monster_desc_race(m_name, sizeof(m_name), m_ptr->r_idx);
            
            /* Create HP bar with asterisks */
            int hp_len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;
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
                get_alertness_text(m_ptr, sizeof(dummy_text), dummy_text, &morale_color);
                
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
            bool highlight_this_monster = (state->in_sidebar_mode && state->selected_entity == monster_count);
            
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
                
                /* Display name+health in highlighted color */
                Term_putstr(name_col, line, name_hp_len, TERM_L_BLUE, display_name);
                
                /* Display morale in highlighted color (overrides morale_color when highlighted) */
                Term_putstr(morale_col, line, morale_display_len, TERM_L_BLUE, morale_display);
                
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
        
        /* Get object list */
        get_sorted_target_list(TARGET_LIST_OBJECT, 0);
        
        /* Create array to hold and sort objects */
        typedef struct {
            int o_idx;
            int y, x;
            object_type* o_ptr;
            bool is_artifact;
            int difficulty;
            int level;
            int group;
            int distance;
            int original_index;
        } sorted_object;
        
        sorted_object objects[temp_n];
        int valid_objects = 0;
        
        /* First pass: collect and filter objects */
        for (i = 0; i < temp_n; i++)
        {
            int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
            if (!o_idx)
                continue;

            object_type* o_ptr = &o_list[o_idx];

            /* Only show marked (memorized) objects that the player has actually seen */
            if (!o_ptr->marked)
                continue;

            if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
                continue;

            sorted_object* entry = &objects[valid_objects];

            entry->o_idx = o_idx;
            entry->y = temp_y[i];
            entry->x = temp_x[i];
            entry->o_ptr = o_ptr;
            entry->is_artifact = artefact_p(o_ptr) ? true : false;
            entry->difficulty = object_difficulty(o_ptr);
            entry->level = k_info[o_ptr->k_idx].level;
            entry->group = unified_sidebar_object_group(o_ptr);
            if (state->object_group_filter >= 0 && entry->group != state->object_group_filter)
                continue;
            entry->distance = distance(p_ptr->py, p_ptr->px, entry->y, entry->x);
            entry->original_index = i;

            valid_objects++;
        }

        for (i = 0; i < valid_objects - 1; i++) {
            for (int j = i + 1; j < valid_objects; j++) {
                sorted_object* a = &objects[i];
                sorted_object* b = &objects[j];
                bool should_swap = false;

                if (a->group != b->group) {
                    should_swap = (b->group < a->group);
                }
                else {
                    bool a_known = object_known_p(a->o_ptr) ? true : false;
                    bool b_known = object_known_p(b->o_ptr) ? true : false;

                    /* Identified items first; then difficulty for identified, proximity for unidentified */
                    if (a_known != b_known)
                    {
                        should_swap = (b_known && !a_known);
                    }
                    else if (!a_known)
                    {
                        if (b->distance < a->distance)
                            should_swap = true;
                        else if ((b->distance == a->distance) && (b->original_index < a->original_index))
                            should_swap = true;
                    }
                    else
                    {
                        if (b->difficulty > a->difficulty)
                            should_swap = true;
                        else if ((b->difficulty == a->difficulty) && (b->distance < a->distance))
                            should_swap = true;
                        else if ((b->difficulty == a->difficulty) && (b->distance == a->distance)
                                 && (b->original_index < a->original_index))
                            should_swap = true;
                    }
                }
                if (should_swap) {
                    sorted_object temp = objects[i];
                    objects[i] = objects[j];
                    objects[j] = temp;
                }
            }
        }

        int group_display_counts[LOOK_GROUP_COUNT] = {0};
        int object_start = (state->show_monsters) ? monster_count : 0;
        for (i = 0; i < valid_objects && line < max_display_line; i++)
        {
            sorted_object* entry = &objects[i];
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
            object_desc(o_name, sizeof(o_name), o_ptr, false, 4);

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

            /* Calculate available width for name + weight */
            int available_name_width = term_wid - name_col - 2; /* Leave some margin */
            if (available_name_width < 10) available_name_width = 10;
            
            int weight_len = (int)strlen(weight_buf);
            int max_name_len = available_name_width - weight_len - 1; /* Reserve space for weight */
            if (max_name_len < 4) max_name_len = 4;

            char display_name[128];
            if (max_name_len > (int)sizeof(display_name) - weight_len - 1) 
                max_name_len = (int)sizeof(display_name) - weight_len - 1;

            sidebar_compact_name(name_source, max_name_len, display_name, sizeof(display_name));
            
            /* Append weight right after name */
            SDL_strlcat(display_name, weight_buf, sizeof(display_name));
            int final_name_len = (int)strlen(display_name);
            int original_name_len = (int)strlen(name_source);
            bool shortened = (original_name_len != final_name_len) || (original_name_len > max_name_len);
            log_debug("sidebar object: idx=%d name='%s' compact='%s' color=%d orig_len=%d compact_len=%d max_len=%d name_col=%d weight_len=%d shortened=%d",
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

            bool highlight_this_object = (state->in_sidebar_mode && state->selected_entity == (object_start + object_count));

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
                
                Term_putstr(name_col, line, final_name_len, name_attr, display_name);

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
