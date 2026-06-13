/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "ui/file-viewer.h"
#include "fs/io_sdl.h"
#include "externs.h"
#include "log/log.h"
#include "sdl-config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
static void string_lower(char* buf)
{
    char* s;

    /* Lowercase the string */
    for (s = buf; *s != 0; s++)
        *s = tolower((unsigned char)*s);
}

static void file_viewer_prompt_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static void file_viewer_draw_prompt(int row, int wid, bool large_file)
{
    char prompt[160];

    if (wid < 1)
        wid = 80;

    if (sdl_touch_only_device_active())
    {
        const char* large_variants[] = {
            "Swipe or tap to scroll, tap away to close",
            "Swipe to scroll, tap away to close",
            "Tap away to close"
        };
        const char* small_variants[] = {
            "Tap away to close"
        };

        terminal_prompt_pick_variant(prompt, sizeof(prompt), wid - 2, false,
            large_file ? large_variants : small_variants,
            large_file ? N_ELEMENTS(large_variants)
                       : N_ELEMENTS(small_variants));
    }
    else if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char back_label[16];
        char prev_label[16];
        char next_label[16];
        char prompt_full[160];
        char prompt_short[96];
        const char* variants[2];

        file_viewer_prompt_label(steamdeck_confirm_key(), "A",
            confirm_label, sizeof(confirm_label));
        file_viewer_prompt_label(steamdeck_back_key(), "B",
            back_label, sizeof(back_label));
        file_viewer_prompt_label(steamdeck_prev_page_key(), "L1",
            prev_label, sizeof(prev_label));
        file_viewer_prompt_label(steamdeck_next_page_key(), "R1",
            next_label, sizeof(next_label));

        if (large_file)
        {
            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad scroll  %s/%s page  %s next  %s exit",
                prev_label, next_label, confirm_label, back_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "%s/%s page  %s exit", prev_label, next_label, back_label);
        }
        else
        {
            strnfmt(prompt_full, sizeof(prompt_full), "%s exit", back_label);
            SDL_strlcpy(prompt_short, prompt_full, sizeof(prompt_short));
        }

        variants[0] = prompt_full;
        variants[1] = prompt_short;
        terminal_prompt_pick_variant(prompt, sizeof(prompt), wid - 2, false,
            variants, N_ELEMENTS(variants));
    }
    else
    {
        const char* small_variants[] = {
            "Esc exit"
        };
        const char* large_variants[] = {
            "Esc exit  Space page  Dir scroll",
            "Esc exit  Space page",
            "Esc exit"
        };

        terminal_prompt_pick_variant(prompt, sizeof(prompt), wid - 2, false,
            large_file ? large_variants : small_variants,
            large_file ? N_ELEMENTS(large_variants)
                       : N_ELEMENTS(small_variants));
    }

    Term_putstr(1, row, wid - 1, TERM_SLATE, prompt);
}

/*
 * Show the contents of a char buffer on the screen and allow scrolling.
 * Based on show_file.
 */
bool show_buffer(cptr main_buffer, int line)
{
    int i, j, k;
    int dir;

    char ch;

    int next = 0;

    char buf[1024];

    int wid, hgt;

    // get current terminal size
    Term_get_size(&wid, &hgt);
    if (hgt <= 0) hgt = 24;

    // count lines in the buffer
    int size = 0;
    for (j = 0; main_buffer[j] != '\0'; j++) {
        if (main_buffer[j] == '\n') size++;
    }
    // add one more if last line doesn't end with newline
    if (j > 0 && main_buffer[j-1] != '\n') size++;

    /* Display the file */
    while (true)
    {
        /* Clear screen */
        Term_clear();

        /* Restrict the visible range */
        if (line > (size - (hgt - 5)))
            line = size - (hgt - 5);
        if (line < 0)
            line = 0;

        /* Goto the selected line */
        next = 0;
        for (j = 0; true; j++)
        {
            if (main_buffer[j] == '\n')
                next++;

            if ((next == line) || (main_buffer[j] == '\0'))
                break;
        }

        // hack: need to step forward a character when not starting with the
        // first line
        if (main_buffer[j] == '\n')
            j++;

        /* Dump the next lines of the file */
        for (i = 0; i < hgt - 5;)
        {
            /* Get a line of the file or stop */
            k = 0;
            while (true)
            {
                ch = main_buffer[j];

                if (ch == '\0')
                {
                    break;
                }

                if (ch == '\n')
                {
                    j++;
                    break;
                }

                buf[k] = ch;

                k++;
                j++;
            }
            buf[k] = '\0';

            /* Dump the line */
            Term_putstr(0, i + 2, -1, TERM_WHITE, buf);

            /* Count the printed lines */
            i++;
        }

        /* Prompt -- small files */
        if (size <= hgt - 5)
        {
            file_viewer_draw_prompt(hgt - 2, wid, false);
        }

        /* Prompt -- large files */
        else
        {
            file_viewer_draw_prompt(hgt - 2, wid, true);
        }

        ui_scroll_area_begin(0, hgt - 1, SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', '6', '4');
        ui_scroll_area_set_tap_key(ESCAPE);

        /* Get a keypress */
        ch = inkey();
        ch = (char)steamdeck_menu_key(ch, '9', '3');

        dir = target_dir(ch);
        if (dir == 8 || dir == 2)
            ch = I2D(dir);

        bool controller_confirm =
            steamdeck_controls_active() && (ch == '\r' || ch == '\n');

        /* Back up one line */
        if ((ch == '8') || (ch == '='))
        {
            line = line - 1;
            if (line < 0)
                line = 0;
        }

        /* Back up one full page */
        if (ch == '9')
        {
            line = line - (hgt - 5);
            if (line < 0)
                line = 0;
        }

        /* Advance one line */
        if ((ch == '2') || (!controller_confirm && ((ch == '\n') || (ch == '\r'))))
        {
            line = line + 1;
        }

        /* Advance one full page */
        if ((ch == '3') || (ch == ' ') || controller_confirm)
        {
            line = line + (hgt - 5);
        }

        /* Exit on escape */
        if (ch == ESCAPE)
            break;
    }

    ui_scroll_area_clear();

    /* Done */
    return (true);
}

/*
 * Recursive file perusal.
 *
 * Return false on "?", otherwise true.
 *
 * Process various special text in the input file, including the "menu"
 * structures used by the "help file" system.
 *
 * This function could be made much more efficient with the use of "seek"
 * functionality, especially when moving backwards through a file, or
 * forwards through a file by less than a page at a time.  XXX XXX XXX
 *
 * Consider using a temporary file, in which special lines do not appear,
 * and which could be pre-padded to 80 characters per line, to allow the
 * use of perfect seeking.  XXX XXX XXX
 *
 * Allow the user to "save" the current file.  XXX XXX XXX
 */
bool show_file(cptr name, cptr what, int line)
{
    int i, k, n;

    char ch;

    /* Number of "real" lines passed by */
    int next = 0;

    /* Number of "real" lines in the file */
    int size;

    /* Backup value for "line" */
    int back = 0;

    /* This screen has sub-screens */
    bool menu = false;

    /* Case sensitive search */
    bool case_sensitive = false;

    /* Current help file */
    SDL_IOStream* fff = NULL;

    /* Find this string (if any) */
    char* find = NULL;

    /* Jump to this tag */
    cptr tag = NULL;

    /* Hold a string to find */
    char finder[80];

    /* Hold a string to show */
    char shower[80];

    /* Filename */
    char filename[1024];

    /* Describe this thing */
    char caption[128];

    /* Path buffer */
    char path[1024];

    /* General buffer */
    char buf[1024];

    /* Lower case version of the buffer, for searching */
    char lc_buf[1024];

    /* Sub-menu information */
    char hook[26][32];

    int wid, hgt;

    /* Wipe finder */
    SDL_strlcpy(finder, "", sizeof(finder));

    /* Wipe shower */
    SDL_strlcpy(shower, "", sizeof(shower));

    /* Wipe caption */
    SDL_strlcpy(caption, "", sizeof(caption));

    /* Wipe the hooks */
    for (i = 0; i < 26; i++)
        hook[i][0] = '\0';

    /* Get size */
    Term_get_size(&wid, &hgt);

    /* Copy the filename */
    SDL_strlcpy(filename, name, sizeof(filename));

    n = strlen(filename);

    /* Extract the tag from the filename */
    for (i = 0; i < n; i++)
    {
        if (filename[i] == '#')
        {
            filename[i] = '\0';
            tag = filename + i + 1;
            break;
        }
    }

    /* Redirect the name */
    name = filename;

    /* Hack XXX XXX XXX */
    if (what)
    {
        /* Caption */
        SDL_strlcpy(caption, what, sizeof(caption));

        /* Get the filename */
        SDL_strlcpy(path, name, sizeof(path));
        
        log_debug("Opening help file: %s", path);

        /* Open */
        fff = sdl_fopen(path, "r");
    }

    /* Oops */
    if (!fff)
    {
        log_warn("Failed to open help file: %s", name);
        /* Message */
        msg_format("Cannot open '%s'.", name);
        message_flush();

        /* Oops */
        return (true);
    }
    
    log_debug("Successfully opened help file: %s", name);

    /* Pre-Parse the file */
    while (true)
    {
        /* Read a line or stop */
        if (sdl_fgets(fff, buf, sizeof(buf)))
            break;

        /* XXX Parse "menu" items */
        if (prefix(buf, "***** "))
        {
            char b1 = '[', b2 = ']';

            /* Notice "menu" requests */
            if ((buf[6] == b1) && isalpha((unsigned char)buf[7])
                && (buf[8] == b2) && (buf[9] == ' '))
            {
                /* This is a menu file */
                menu = true;

                /* Extract the menu item */
                k = A2I(buf[7]);

                /* Store the menu item (if valid) */
                if ((k >= 0) && (k < 26))
                    SDL_strlcpy(hook[k], buf + 10, sizeof(hook[0]));
            }
            /* Notice "tag" requests */
            else if (buf[6] == '<')
            {
                if (tag)
                {
                    /* Remove the closing '>' of the tag */
                    buf[strlen(buf) - 1] = '\0';

                    /* Compare with the requested tag */
                    if (streq(buf + 7, tag))
                    {
                        /* Remember the tagged line */
                        line = next;
                    }
                }
            }

            /* Skip this */
            continue;
        }

        /* Count the "real" lines */
        next++;
    }

    /* Save the number of "real" lines */
    size = next;

    /* Display the file */
    while (true)
    {
        /* Clear screen */
        Term_clear();

        /* Restrict the visible range */
        if (line > (size - (hgt - 5)))
            line = size - (hgt - 5);
        if (line < 0)
            line = 0;

        /* Re-open the file if needed */
        if (next > line)
        {
            /* Close it */
            sdl_fclose(fff);

            /* Hack -- Re-Open the file */
            fff = sdl_fopen(path, "r");

            /* Oops */
            if (!fff)
                return (true);

            /* File has been restarted */
            next = 0;
        }

        /* Goto the selected line */
        while (next < line)
        {
            /* Get a line */
            if (sdl_fgets(fff, buf, sizeof(buf)))
                break;

            /* Skip tags/links */
            if (prefix(buf, "***** "))
                continue;

            /* Count the lines */
            next++;
        }

        /* Dump the next lines of the file */
        for (i = 0; i < hgt - 5;)
        {
            /* Hack -- track the "first" line */
            if (!i)
                line = next;

            /* Get a line of the file or stop */
            if (sdl_fgets(fff, buf, sizeof(buf)))
                break;

            /* Hack -- skip "special" lines */
            if (prefix(buf, "***** "))
                continue;

            /* Count the "real" lines */
            next++;

            /* Make a copy of the current line for searching */
            SDL_strlcpy(lc_buf, buf, sizeof(lc_buf));

            /* Make the line lower case */
            if (!case_sensitive)
                string_lower(lc_buf);

            /* Hack -- keep searching */
            if (find && !i && !strstr(lc_buf, find))
                continue;

            /* Hack -- stop searching */
            find = NULL;

            /* Dump the line */
            Term_putstr(0, i + 2, -1, TERM_WHITE, buf);

            /* Hilite "shower" */
            if (shower[0])
            {
                cptr str = lc_buf;

                /* Display matches */
                while ((str = strstr(str, shower)) != NULL)
                {
                    int len = strlen(shower);

                    /* Display the match */
                    Term_putstr(str - lc_buf, i + 2, len, TERM_YELLOW,
                        &buf[str - lc_buf]);

                    /* Advance */
                    str += len;
                }
            }

            /* Count the printed lines */
            i++;
        }

        /* Hack -- failed search */
        if (find)
        {
            bell("Search string not found!");
            line = back;
            find = NULL;
            continue;
        }

        /* Show a general "title" */
        //		prt(format("[%s %s, %s, Line %d-%d/%d]", VERSION_NAME,
        // VERSION_STRING, 	           caption, line, line + hgt - 4, size),
        // 0, 0);

        /* Prompt -- menu screen */
        if (menu)
        {
            /* Wait for it */
            {
                const char* variants[] = {
                    "Number select  Esc exit",
                    "Esc exit"
                };
                terminal_prompt_put_variant(0, hgt - 1, wid, TERM_SLATE,
                    false, variants, N_ELEMENTS(variants));
            }
        }

        /* Prompt -- small files */
        else if (size <= hgt - 5)
        {
            file_viewer_draw_prompt(hgt - 2, wid, false);
        }

        /* Prompt -- large files */
        else
        {
            file_viewer_draw_prompt(hgt - 2, wid, true);
        }

        /* Get a keypress */
        ch = inkey();
        ch = (char)steamdeck_menu_key(ch, '9', '3');
        bool controller_confirm =
            steamdeck_controls_active() && (ch == '\r' || ch == '\n');

        /* Exit the help */
        if (ch == '?')
            break;

        /* Toggle case sensitive on/off */
        if (ch == '!')
        {
            case_sensitive = !case_sensitive;
        }

        /* Try showing */
        if (ch == '&')
        {
            /* Get "shower" */
            prt("Show: ", hgt - 1, 0);
            (void)askfor_aux(shower, sizeof(shower));

            /* Make the "shower" lowercase */
            if (!case_sensitive)
                string_lower(shower);
        }

        /* Try finding */
        if (ch == '/')
        {
            /* Get "finder" */
            prt("Find: ", hgt - 1, 0);
            if (askfor_aux(finder, sizeof(finder)))
            {
                /* Find it */
                find = finder;
                back = line;
                line = line + 1;

                /* Make the "finder" lowercase */
                if (!case_sensitive)
                    string_lower(finder);

                /* Show it */
                SDL_strlcpy(shower, finder, sizeof(shower));
            }
        }

        /* Go to a specific line */
        if (ch == '#')
        {
            char tmp[80];
            prt("Goto Line: ", hgt - 1, 0);
            SDL_strlcpy(tmp, "0", sizeof(tmp));
            if (askfor_aux(tmp, sizeof(tmp)))
            {
                line = atoi(tmp);
            }
        }

        /* Back up one line */
        if ((ch == '8') || (ch == '='))
        {
            line = line - 1;
            if (line < 0)
                line = 0;
        }

        /* Back up one half page */
        if (ch == '_')
        {
            line = line - ((hgt - 5) / 2);
        }

        /* Back up one full page */
        if ((ch == '9') || (ch == '-'))
        {
            line = line - (hgt - 5);
        }

        /* Back to the top */
        if (ch == '7')
        {
            line = 0;
        }

        /* Advance one line */
        if ((ch == '2') || (!controller_confirm && ((ch == '\n') || (ch == '\r'))))
        {
            line = line + 1;
        }

        /* Advance one half page */
        if (ch == '+')
        {
            line = line + ((hgt - 5) / 2);
        }

        /* Advance one full page */
        if ((ch == '3') || (ch == ' ') || controller_confirm)
        {
            line = line + (hgt - 5);
        }

        /* Advance to the bottom */
        if (ch == '1')
        {
            line = size;
        }

        /* Exit on escape */
        if (ch == ESCAPE)
            break;
    }

    /* Close the file */
    sdl_fclose(fff);

    /* Done */
    return (ch != '?');
}

