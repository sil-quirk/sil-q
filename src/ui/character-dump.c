/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "ui/character-dump.h"
#include "score/score_entry.h"
#include "score/score_logic.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "externs.h"
#include <stdio.h>
#include <string.h>
static char mini_screenshot_char[7][7];
static byte mini_screenshot_attr[7][7];

errr file_character(cptr name, bool full)
{
    int i, x, y;

    byte a;

#define SDL_IOprintf SDL_IOprintf
    char c;

    SDL_IOStream* fd;

    SDL_IOStream* fff = NULL;

    char o_name[80];

    char buf[1024];

    ability_type* b_ptr;

    int holder;

    bool challenges = false;

    high_score the_score;

    /* Unused parameter */
    (void)full;

    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_USER, name);

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Check for existing file */
    fd = sdl_fopen(buf, "rb");

    /* Existing file */
    if (fd)
    {
        char out_val[160];

        /* Close the file */
        sdl_fclose(fd);

        /* Build query */
        strnfmt(out_val, sizeof(out_val), "Replace existing file %s? ", buf);

        /* Ask */
        if (get_check(out_val))
            fd = NULL;
    }

    /* Open the non-existing file */
    if (!fd)
        fff = sdl_fopen(buf, "w");

    /* Invalid file */
    if (!fff)
        return (-1);

    text_out_hook = text_out_to_file;
    text_out_file = fff;

    /* Begin dump */
    SDL_IOprintf(fff, "  [%s %s Character Dump]\n\n", VERSION_NAME, VERSION_STRING);

    /* Display player */
    display_player(0);

    /* Dump part of the screen */
    for (y = 2; y < 23; y++)
    {
        /* Dump each row */
        for (x = 0; x < 79; x++)
        {
            /* Get the attr/char */
            (void)(Term_what(x, y, &a, &c));

            /* Dump it */
            buf[x] = c;
        }

        /* Back up over spaces */
        while ((x > 0) && (buf[x - 1] == ' '))
            --x;

        /* Terminate */
        buf[x] = '\0';

        /* End the row */
        SDL_IOprintf(fff, "%s\n", buf);
    }

    /* If dead, dump last messages and a mini screenshot */
    if (p_ptr->is_dead)
    {
        int x, y;

        i = message_num();
        if (i > 15)
            i = 15;
        SDL_IOprintf(fff, "\n  [Last Messages]\n\n");
        while (i-- > 0)
        {
            SDL_IOprintf(fff, "> %s\n", message_str((s16b)i));
        }
        SDL_IOprintf(fff, "\n");

        SDL_IOprintf(fff, "\n  [Screenshot]\n\n");

        // simple screenshot for those who died in Angband
        if (!p_ptr->escaped)
        {
            for (y = 0; y <= 6; y++)
            {
                SDL_IOprintf(fff, "  ");
                for (x = 0; x <= 6; x++)
                {
                    SDL_IOprintf(fff, "%c", mini_screenshot_char[y][x]);
                }
                SDL_IOprintf(fff, "\n");
            }
        }

        // Special Screenshot for escapees
        else
        {
            // grass
            SDL_IOprintf(fff, "  .......\n");
            SDL_IOprintf(fff, "  ~...#..\n");
            SDL_IOprintf(fff, "  ~~.....\n");
            SDL_IOprintf(fff, "  .~.@...\n");
            SDL_IOprintf(fff, "  .~~...#\n");
            SDL_IOprintf(fff, "  ..~~...\n");
            SDL_IOprintf(fff, "  ...~...\n");
        }
        SDL_IOprintf(fff, "\n");
    }

    /* Dump the equipment */
    if (p_ptr->equip_cnt)
    {
        SDL_IOprintf(fff, "\n  [Equipment]\n\n");
        for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            object_type* o_ptr = &inventory[i];
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

            /* Display the weight if needed */
            if (o_ptr->weight
                && ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM)
                    || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING)
                    || (o_ptr->tval == TV_BOW)))
            {
                int wgt = o_ptr->weight * o_ptr->number;
                char wgt_buf[80];

                sprintf(wgt_buf, " %d.%1d lb", wgt / 10, wgt % 10);
                SDL_strlcat(o_name, wgt_buf, sizeof(o_name));
            }

            SDL_IOprintf(fff, "%c) %s\n", index_to_label(i), o_name);

            /* Describe random object attributes */
            identify_random_gen(o_ptr);
        }
        SDL_IOprintf(fff, "\n\n");
    }

    /* Dump the inventory */
    SDL_IOprintf(fff, "  [Inventory]\n\n");
    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx)
            break;

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Display the weight if needed */
        if (o_ptr->weight
            && ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM)
                || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING)
                || (o_ptr->tval == TV_BOW)))
        {
            int wgt = o_ptr->weight * o_ptr->number;
            char wgt_buf[80];

            sprintf(wgt_buf, " %d.%1d lb", wgt / 10, wgt % 10);
            SDL_strlcat(o_name, wgt_buf, sizeof(o_name));
        }

        SDL_IOprintf(fff, "%c) %s\n", index_to_label(i), o_name);

        /* Describe random object attributes */
        identify_random_gen(o_ptr);
    }

    // Dump abilities.
    SDL_IOprintf(fff, "\n\n  [Abilities]\n\n");
    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        if (!b_ptr->name)
            continue;

        if (p_ptr->innate_ability[b_ptr->skilltype][b_ptr->abilitynum])
        {
            if (b_ptr->skilltype == S_PER && b_ptr->abilitynum == PER_BANE
                && p_ptr->bane_type > 0)
            {
                SDL_IOprintf(fff, "%s-%s\n", bane_name[p_ptr->bane_type],
                    (b_name + b_ptr->name));
            }
            else if (b_ptr->skilltype == S_WIL && b_ptr->abilitynum == WIL_OATH
                && p_ptr->oath_type > 0)
            {
                if (oath_invalid(p_ptr->oath_type))
                    SDL_IOprintf(fff, "%s: %s (Broken)\n", (b_name + b_ptr->name),
                        oath_name[p_ptr->oath_type]);
                else
                    SDL_IOprintf(fff, "%s: %s\n", (b_name + b_ptr->name),
                        oath_name[p_ptr->oath_type]);
            }
            else
                SDL_IOprintf(fff, "%s\n", (b_name + b_ptr->name));
        }
    }

    SDL_IOprintf(fff, "\n\n  [Enemies]\n\n");

    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        if (!l_ptr->psights && !l_ptr->pkills)
        {
            continue;
        }

        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /* Print a message */
            SDL_IOprintf(fff, "  %-7s %s \n", l_ptr->pkills ? "(slain)" : "(seen)",
                (r_name + r_ptr->name));
        }
        else
        {
            /* Print a message */
            SDL_IOprintf(fff, "%3d /%3d  %-40s\n", l_ptr->pkills, l_ptr->psights,
                (r_name + r_ptr->name));
        }
    }

    // Dump found artefacts if dead.
    if (p_ptr->is_dead)
    {
        SDL_IOprintf(fff, "\n\n  [Artefacts]\n\n");

        // Just go to the end of the normal artefacts list, don't also grab
        // forged artefacts.
        for (i = 0; i < z_info->art_norm_max; i++)
        {
            char o_name[120];
            artefact_type* a_ptr;
            object_type* o_ptr;
            object_type object_type_body;
            o_ptr = &object_type_body;

            a_ptr = &a_info[i];
            if (a_ptr->cur_num == 0)
                continue;

            make_fake_artefact(o_ptr, i);
            object_desc_spoil(o_name, sizeof(o_name), o_ptr, true, 0);

            SDL_IOprintf(
                fff, "%s %s\n", o_name, a_ptr->found_num > 0 ? "(found)" : "");
        }
    }

    SDL_IOprintf(fff, "\n\n  [Notes]\n\n");

    /*dump notes to character file*/
    i = 0;
    holder = notes_buffer[i];

    while (holder != '\0')
    {
        /*get a character from the notes buffer*/
        holder = notes_buffer[i];

        /*output it to the character dump*/
        if (holder != '\0')
            SDL_IOprintf(fff, "%c", holder);

        // increment location in notes buffer
        i++;
    }

    SDL_IOprintf(fff, "\n");

    /* Count options */
    for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
    {
        if (option_desc[i] && op_ptr->opt[i])
        {
            challenges = true;
        }
    }

    if (challenges)
    {
        /* Dump options */
        SDL_IOprintf(fff, "  [Challenges]\n\n");

        /* Dump options */
        for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
        {
            if (option_desc[i] && op_ptr->opt[i])
            {
                SDL_IOprintf(fff, "%-45s\n", option_desc[i]);
            }
        }
    }

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    // display a "score"
    create_score(&the_score);
    SDL_IOprintf(fff, "  ['Score' %.9d]\n\n", score_points(&the_score));

    /* Close it */
    sdl_fclose(fff);

#undef SDL_IOprintf

    /* Success */
    return (0);
}


static void get_tile(int row, int col, byte* a_def, char* c_def)
{
    byte a;
    char c;

    /* Get the tile from the screen */
    a = Term->scr->a[row][col];
    c = Term->scr->c[row][col];

    /* Return the tile */
    *a_def = a;
    *c_def = c;
}

void mini_screenshot(void)
{
    int x, y, wid, hgt;
    byte a;
    char c;

    int player_y = 0, player_x = 0;

    // These widths and heights are meant to be bigger than the biggest possible
    // terminal window They are a bit of a hack.
    char screen_char[100][200];
    byte screen_attr[100][200];

    /* Retrieve current screen size */
    Term_get_size(&wid, &hgt);

    /* Initialize the arrays */
    for (y = 0; y < 100; y++)
    {
        for (x = 0; x < 200; x++)
        {
            screen_char[y][x] = ' ';
            screen_attr[y][x] = TERM_DARK;
        }
    }

    /* Save the screen */
    for (y = 0; y < hgt; y++)
    {
        for (x = 0; x < wid; x++)
        {
            /* Get the ASCII tile */
            get_tile(y, x, &a, &c);

            // check to see if it is the player
            if ((c == '@')
                && ((a == TERM_WHITE) || (a == TERM_YELLOW)
                    || (a == TERM_ORANGE) || (a == TERM_L_RED)
                    || (a == TERM_RED)))
            {
                player_x = x;
                player_y = y;
            }

            screen_char[y][x] = c;
            screen_attr[y][x] = a;
        }
    }

    if (player_y > 0)
    {
        for (y = 0; y <= 6; y++)
        {
            for (x = 0; x <= 6; x++)
            {
                mini_screenshot_char[y][x]
                    = screen_char[player_y - 3 + y][player_x - 3 + x];
                mini_screenshot_attr[y][x]
                    = screen_attr[player_y - 3 + y][player_x - 3 + x];
            }
        }
    }
    else
    {
        for (y = 0; y <= 6; y++)
        {
            for (x = 0; x <= 6; x++)
            {
                /* Fallback: blank miniature with dark attributes */
                mini_screenshot_char[y][x] = ' ';
                mini_screenshot_attr[y][x] = TERM_DARK;
            }
        }
    }
}

void prt_mini_screenshot(int col, int row)
{
    int x, y;

    if (!p_ptr->escaped)
    {
        for (y = 0; y <= 6; y++)
        {
            for (x = 0; x <= 6; x++)
            {
                if ((x == 3) && (y == 3))
                {
                    Term_putch(
                        col + x, row + y, TERM_RED, mini_screenshot_char[y][x]);
                }
                else
                {
                    Term_putch(col + x, row + y, mini_screenshot_attr[y][x],
                        mini_screenshot_char[y][x]);
                }
            }
        }
    }
    else
    {
        // grass
        Term_putstr(col, row, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 1, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 2, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 3, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 4, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 5, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 6, -1, TERM_L_GREEN, ".......");

        // river
        Term_putch(col, row + 1, TERM_BLUE, '~');
        Term_putch(col, row + 2, TERM_BLUE, '~');
        Term_putch(col + 1, row + 2, TERM_L_BLUE, '~');
        Term_putch(col + 1, row + 3, TERM_BLUE, '~');
        Term_putch(col + 1, row + 4, TERM_L_BLUE, '~');
        Term_putch(col + 2, row + 4, TERM_BLUE, '~');
        Term_putch(col + 2, row + 5, TERM_BLUE, '~');
        Term_putch(col + 3, row + 5, TERM_L_BLUE, '~');
        Term_putch(col + 3, row + 6, TERM_BLUE, '~');

        // trees
        Term_putch(col + 4, row + 1, TERM_GREEN, '#');
        Term_putch(col + 6, row + 4, TERM_GREEN, '#');

        // player
        Term_putch(col + 3, row + 3, TERM_WHITE, '@');
    }
}

/*
 * Attempt to auto-load the first "alive" character found in the scorefile.
 * If a corresponding savefile cannot be loaded, mark the score entry as
 * dead (cause: "their own hand"), increment the metarun death counter, show
 * a warning, and continue scanning. Returns true if a character was loaded;
 * false if no alive entries remain or none could be loaded.
 */
