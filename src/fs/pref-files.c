/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "fs/pref-files.h"
#include "fs/file.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "externs.h"
#include "log/log.h"
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
static bool parse_visual_component(const char* token, bool expect_row, byte* value)
{
    if (!token || !*token || !value)
        return false;

    char prefix = token[0];
    if (expect_row && (prefix == 'R' || prefix == 'r'))
    {
        char* end = NULL;
        long row = strtol(token + 1, &end, 0);
        if (end && (*end == '\0') && (row >= 0) && (row <= TILE_INDEX_MASK))
        {
            *value = TILE_SET_INDEX(TILE_FLAG, (byte)row);
            return true;
        }
    }
    else if (!expect_row && (prefix == 'C' || prefix == 'c'))
    {
        char* end = NULL;
        long col = strtol(token + 1, &end, 0);
        if (end && (*end == '\0') && (col >= 0) && (col <= TILE_INDEX_MASK))
        {
            *value = TILE_SET_INDEX(TILE_FLAG, (byte)col);
            return true;
        }
    }

    char* end = NULL;
    long parsed = strtol(token, &end, 0);
    if (end && (*end == '\0') && (parsed >= 0) && (parsed <= UCHAR_MAX))
    {
        *value = (byte)parsed;
        return true;
    }

    return false;
}

/*
 * Hack -- drop permissions
 */
errr process_pref_file_command(char* buf)
{
    long i, n1, n2, sq;

    char* zz[16];

    /* Skip "empty" lines */
    if (!buf[0])
        return (0);

    /* Skip "blank" lines */
    if (isspace((unsigned char)buf[0]))
        return (0);

    /* Skip comments */
    if (buf[0] == '#')
        return (0);

    /* Paranoia */
    /* if (strlen(buf) >= 1024) return (1); */

    /* Require "?:*" format */
    if (buf[1] != ':')
        return (1);

    /* Process "R:<num>:<a>/<c>" -- attr/char for monster races */
    if (buf[0] == 'R')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            monster_race* r_ptr;
            byte parsed_attr, parsed_char;
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= (long)z_info->r_max))
                return (1);
            r_ptr = &r_info[i];
            if (parse_visual_component(zz[1], true, &parsed_attr) && parsed_attr)
                r_ptr->x_attr = parsed_attr;
            if (parse_visual_component(zz[2], false, &parsed_char) && parsed_char)
                r_ptr->x_char = (char)parsed_char;
            return (0);
        }
    }

    /* Process "B:<k_idx>:inscription */
    else if (buf[0] == 'B')
    {
        if (2 == tokenize(buf + 2, 2, zz))
        {
            add_autoinscription(strtol(zz[0], NULL, 0), zz[1]);
            return (0);
        }
    }

    /* Process "Q:<idx>:<tval>:<sval>:<y|n>"  -- squelch bits   */
    /* and     "Q:<idx>:<val>"                -- squelch levels */
    /* and     "Q:<val>"                      -- auto_destroy   */
    else if (buf[0] == 'Q')
    {
        i = tokenize(buf + 2, 4, zz);
        if (i == 2)
        {
            n1 = strtol(zz[0], NULL, 0);
            n2 = strtol(zz[1], NULL, 0);
            squelch_level[n1] = n2;
            return (0);
        }
        else if (i == 4)
        {
            i = strtol(zz[0], NULL, 0);
            n1 = strtol(zz[1], NULL, 0);
            n2 = strtol(zz[2], NULL, 0);
            sq = strtol(zz[3], NULL, 0);
            if ((k_info[i].tval == n1) && (k_info[i].sval == n2))
            {
                k_info[i].squelch = sq;
                return (0);
            }
            else
            {
                for (i = 1; i < z_info->k_max; i++)
                {
                    if ((k_info[i].tval == n1) && (k_info[i].sval == n2))
                    {
                        k_info[i].squelch = sq;
                        return (0);
                    }
                }
            }
        }
    }

    /* Process "K:<num>:<a>/<c>"  -- attr/char for object kinds */
    else if (buf[0] == 'K')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            object_kind* k_ptr;
            byte parsed_attr, parsed_char;
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= (long)z_info->k_max))
                return (1);
            k_ptr = &k_info[i];
            if (parse_visual_component(zz[1], true, &parsed_attr) && parsed_attr)
                k_ptr->x_attr = parsed_attr;
            if (parse_visual_component(zz[2], false, &parsed_char) && parsed_char)
                k_ptr->x_char = (char)parsed_char;
            return (0);
        }
    }

    /* Process "F:<num>:<a>/<c>" -- attr/char for terrain features */
    else if (buf[0] == 'F')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            feature_type* f_ptr;
            byte parsed_attr, parsed_char;
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= (long)z_info->f_max))
                return (1);
            f_ptr = &f_info[i];
            if (parse_visual_component(zz[1], true, &parsed_attr) && parsed_attr)
                f_ptr->x_attr = parsed_attr;
            if (parse_visual_component(zz[2], false, &parsed_char) && parsed_char)
                f_ptr->x_char = (char)parsed_char;
            return (0);
        }
    }

    /* Process "L:<num>:<a>/<c>" -- attr/char for flavors */
    else if (buf[0] == 'L')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            flavor_type* flavor_ptr;
            byte parsed_attr, parsed_char;
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= (long)z_info->flavor_max))
                return (1);
            flavor_ptr = &flavor_info[i];
            if (parse_visual_component(zz[1], true, &parsed_attr) && parsed_attr)
                flavor_ptr->x_attr = parsed_attr;
            if (parse_visual_component(zz[2], false, &parsed_char) && parsed_char)
                flavor_ptr->x_char = (char)parsed_char;
            return (0);
        }
    }

    /* Process "S:<num>:<a>/<c>" -- attr/char for special things */
    else if (buf[0] == 'S')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            i = strtol(zz[0], NULL, 0);
            n1 = strtol(zz[1], NULL, 0);
            n2 = strtol(zz[2], NULL, 0);
            if ((i < 0) || (i >= (long)N_ELEMENTS(misc_to_attr)))
                return (1);
            misc_to_attr[i] = (byte)n1;
            misc_to_char[i] = (char)n2;
            return (0);
        }
    }

    /* Process "E:<tv>:<a>" -- attribute for inventory objects */
    else if (buf[0] == 'E')
    {
        if (tokenize(buf + 2, 2, zz) == 2)
        {
            i = strtol(zz[0], NULL, 0) % 128;
            n1 = strtol(zz[1], NULL, 0);
            if ((i < 0) || (i >= (long)N_ELEMENTS(tval_to_attr)))
                return (1);
            if (n1)
                tval_to_attr[i] = (byte)n1;
            return (0);
        }
    }

    /* Process "A:<str>" -- save an "action" for later */
    else if (buf[0] == 'A')
    {
        text_to_ascii(macro_buffer, sizeof(macro_buffer), buf + 2);
        return (0);
    }

    /* Process "P:<str>" -- create macro */
    else if (buf[0] == 'P')
    {
        char tmp[1024];
        text_to_ascii(tmp, sizeof(tmp), buf + 2);
        macro_add(tmp, macro_buffer);
        return (0);
    }

    /* Process "C:<num>:<str>" -- create keymap */
    else if (buf[0] == 'C')
    {
        long mode;

        char tmp[1024];

        if (tokenize(buf + 2, 2, zz) != 2)
            return (1);

        mode = strtol(zz[0], NULL, 0);
        if ((mode < 0) || (mode >= KEYMAP_MODES))
            return (1);

        text_to_ascii(tmp, sizeof(tmp), zz[1]);
        if (!tmp[0] || tmp[1])
            return (1);
        i = (long)tmp[0];

        str_free(keymap_act[mode][i]);

        keymap_act[mode][i] = str_dup(macro_buffer);

        return (0);
    }

    /* Process "V:<num>:<kv>:<rv>:<gv>:<bv>" -- visual info */
    else if (buf[0] == 'V')
    {
        if (tokenize(buf + 2, 5, zz) == 5)
        {
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= 256))
                return (1);
            angband_color_table[i][0] = (byte)strtol(zz[1], NULL, 0);
            angband_color_table[i][1] = (byte)strtol(zz[2], NULL, 0);
            angband_color_table[i][2] = (byte)strtol(zz[3], NULL, 0);
            angband_color_table[i][3] = (byte)strtol(zz[4], NULL, 0);
            return (0);
        }
    }

    /* set macro trigger names and a template */
    /* Process "T:<trigger>:<keycode>:<shift-keycode>" */
    /* Process "T:<template>:<modifier chr>:<modifier name>:..." */
    else if (buf[0] == 'T')
    {
        int tok;

        tok = tokenize(buf + 2, MAX_MACRO_MOD + 2, zz);

        /* Trigger template */
        if (tok >= 4)
        {
            int i;
            int num;

            /* Free existing macro triggers and trigger template */
            macro_trigger_free();

            /* Clear template done */
            if (*zz[0] == '\0')
                return 0;

            /* Count modifier-characters */
            num = strlen(zz[1]);

            /* One modifier-character per modifier */
            if (num + 2 != tok)
                return 1;

            /* Macro template */
            macro_template = str_dup(zz[0]);

            /* Modifier chars */
            macro_modifier_chr = str_dup(zz[1]);

            /* Modifier names */
            for (i = 0; i < num; i++)
            {
                macro_modifier_name[i] = str_dup(zz[2 + i]);
            }
        }
        /* Macro trigger */
        else if (tok >= 2)
        {
            char* buf;
            cptr s;
            char* t;

            if (max_macrotrigger >= MAX_MACRO_TRIGGER)
            {
                msg_print("Too many macro triggers!");
                return 1;
            }

            /* Buffer for the trigger name */
            buf = mem_alloc_array(strlen(zz[0]) + 1, char);

            /* Simulate strcpy() and skip the '\' escape character */
            s = zz[0];
            t = buf;

            while (*s)
            {
                if ('\\' == *s)
                    s++;
                *t++ = *s++;
            }

            /* Terminate the trigger name */
            *t = '\0';

            /* Store the trigger name */
            macro_trigger_name[max_macrotrigger] = str_dup(buf);

            /* Free the buffer */
            mem_free_null(buf);

            /* Normal keycode */
            macro_trigger_keycode[0][max_macrotrigger] = str_dup(zz[1]);

            /* Special shifted keycode */
            if (tok == 3)
            {
                macro_trigger_keycode[1][max_macrotrigger] = str_dup(zz[2]);
            }
            /* Shifted keycode is the same as the normal keycode */
            else
            {
                macro_trigger_keycode[1][max_macrotrigger] = str_dup(zz[1]);
            }

            /* Count triggers */
            max_macrotrigger++;
        }

        return 0;
    }

    /* Process "X:<str>" -- turn option off */
    else if (buf[0] == 'X')
    {
        /* Check non-adult options */
        for (i = 0; i < OPT_ADULT; i++)
        {
            if (option_text[i] && streq(option_text[i], buf + 2))
            {
                op_ptr->opt[i] = false;
                return (0);
            }
        }

        /* Ignore unknown options */
        return (0);
    }

    /* Process "Y:<str>" -- turn option on */
    else if (buf[0] == 'Y')
    {
        /* Check non-adult options */
        for (i = 0; i < OPT_ADULT; i++)
        {
            if (option_text[i] && streq(option_text[i], buf + 2))
            {
                op_ptr->opt[i] = true;
                return (0);
            }
        }

        /* Ignore unknown options */
        return (0);
    }

    /* Process "W:<win>:<flag>:<value>" -- window flags */
    else if (buf[0] == 'W')
    {
        long win, flag, value;

        if (tokenize(buf + 2, 3, zz) == 3)
        {
            win = strtol(zz[0], NULL, 0);
            flag = strtol(zz[1], NULL, 0);
            value = strtol(zz[2], NULL, 0);

            /* Ignore illegal windows */
            /* Hack -- Ignore the main window */
            if ((win <= 0) || (win >= ANGBAND_TERM_MAX))
                return (1);

            /* Ignore illegal flags */
            if ((flag < 0) || (flag >= 32))
                return (1);

            /* Require a real flag */
            if (window_flag_desc[flag])
            {
                if (value)
                {
                    /* Turn flag on */
                    op_ptr->window_flag[win] |= (1L << flag);
                }
                else
                {
                    /* Turn flag off */
                    op_ptr->window_flag[win] &= ~(1L << flag);
                }
            }

            /* Success */
            return (0);
        }
    }

    /* Process "M:<type>:<attr>" -- colors for message-types */
    else if (buf[0] == 'M')
    {
        if (tokenize(buf + 2, 2, zz) == 2)
        {
            long type = strtol(zz[0], NULL, 0);
            int color = color_char_to_attr(zz[1][0]);

            /* Ignore illegal color */
            if (color < 0)
                return (1);

            /* Store the color */
            return (message_color_define((u16b)type, (byte)color));
        }
    }

    /* Failure */
    return (1);
}

/*
 * Helper function for "process_pref_file()"
 *
 * Input:
 *   v: output buffer array
 *   f: final character
 *
 * Output:
 *   result
 */
static cptr process_pref_file_expr(char** sp, char* fp)
{
    cptr v;

    char* b;
    char* s;

    char b1 = '[';
    char b2 = ']';

    char f = ' ';


    /* Initial */
    s = (*sp);

    /* Skip spaces */
    while (isspace((unsigned char)*s))
        s++;

    /* Save start */
    b = s;

    /* Default */
    v = "?o?o?";

    /* Analyze */
    if (*s == b1)
    {
        const char* p;
        const char* t;

        /* Skip b1 */
        s++;

        /* First */
        t = process_pref_file_expr(&s, &f);

        /* Oops */
        if (!*t)
        {
            /* Nothing */
        }

        /* Function: IOR */
        else if (streq(t, "IOR"))
        {
            v = "0";
            while (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
                if (*t && !streq(t, "0"))
                    v = "1";
            }
        }

        /* Function: AND */
        else if (streq(t, "AND"))
        {
            v = "1";
            while (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
                if (*t && streq(t, "0"))
                    v = "0";
            }
        }

        /* Function: NOT */
        else if (streq(t, "NOT"))
        {
            v = "1";
            while (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
                if (*t && !streq(t, "0"))
                    v = "0";
            }
        }

        /* Function: EQU */
        else if (streq(t, "EQU"))
        {
            v = "1";
            if (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
            }
            while (*s && (f != b2))
            {
                p = t;
                t = process_pref_file_expr(&s, &f);
                if (*t && !streq(p, t))
                    v = "0";
            }
        }

        /* Function: LEQ */
        else if (streq(t, "LEQ"))
        {
            v = "1";
            if (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
            }
            while (*s && (f != b2))
            {
                p = t;
                t = process_pref_file_expr(&s, &f);
                if (*t && (strcmp(p, t) >= 0))
                    v = "0";
            }
        }

        /* Function: GEQ */
        else if (streq(t, "GEQ"))
        {
            v = "1";
            if (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
            }
            while (*s && (f != b2))
            {
                p = t;
                t = process_pref_file_expr(&s, &f);
                if (*t && (strcmp(p, t) <= 0))
                    v = "0";
            }
        }

        /* Oops */
        else
        {
            while (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
            }
        }

        /* Verify ending */
        if (f != b2)
            v = "?x?x?";

        /* Extract final and Terminate */
        if ((f = *s) != '\0')
            *s++ = '\0';
    }

    /* Other */
    else
    {
        /* Accept all printables except spaces and brackets */
        while (isprint((unsigned char)*s) && !strchr(" []", *s))
            ++s;

        /* Extract final and Terminate */
        if ((f = *s) != '\0')
            *s++ = '\0';

        /* Variable */
        if (*b == '$')
        {
            /* System */
            if (streq(b + 1, "SYS"))
            {
                v = ANGBAND_SYS;
            }

            /* Graphics */
            else if (streq(b + 1, "GRAF"))
            {
                v = ANGBAND_GRAF;
            }

            /* Race */
            else if (streq(b + 1, "RACE"))
            {
                v = p_name + rp_ptr->name;
            }

            /* Player */
            else if (streq(b + 1, "nameless"))
            {
                v = op_ptr->base_name;
            }

            /* Game version */
            else if (streq(b + 1, "VERSION"))
            {
                v = VERSION_STRING;
            }
        }

        /* Constant */
        else
        {
            v = b;
        }
    }

    /* Save */
    (*fp) = f;

    /* Save */
    (*sp) = s;

    /* Result */
    return (v);
}

/*
 * Open the "user pref file" and parse it.
 */
static errr process_pref_file_aux(cptr name)
{
    SDL_IOStream* fp;

    char buf[1024];

    char old[1024];

    int line = -1;

    errr err = 0;

    bool bypass = false;

    log_debug("Processing preference file: %s", name);

    /* Open the file */
    fp = sdl_fopen(name, "r");

    /* No such file */
    if (!fp) {
        log_debug("Preference file '%s' not found or could not be opened", name);
        return (-1);
    }

    /* Process the file */
    while (0 == sdl_fgets(fp, buf, sizeof(buf)))
    {
        /* Count lines */
        line++;

        /* Skip "empty" lines */
        if (!buf[0])
            continue;

        /* Skip "blank" lines */
        if (isspace((unsigned char)buf[0]))
            continue;

        /* Skip comments */
        if (buf[0] == '#')
            continue;

        /* Save a copy */
        SDL_strlcpy(old, buf, sizeof(old));

        /* Process "?:<expr>" */
        if ((buf[0] == '?') && (buf[1] == ':'))
        {
            char f;
            cptr v;
            char* s;

            /* Start */
            s = buf + 2;

            /* Parse the expr */
            v = process_pref_file_expr(&s, &f);

            /* Set flag */
            bypass = (streq(v, "0") ? true : false);

            /* Continue */
            continue;
        }

        /* Apply conditionals */
        if (bypass)
            continue;

        /* Process "%:<file>" */
        if (buf[0] == '%')
        {
            /* Process that file if allowed */
            (void)process_pref_file(buf + 2);

            /* Continue */
            continue;
        }

        /* Process the line */
        err = process_pref_file_command(buf);

        /* Oops */
        if (err)
            break;
    }

    /* Error */
    if (err)
    {
        /* Print error message */
        /* ToDo: Add better error messages */
        msg_format("Error %d in line %d of file '%s'.", err, line, name);
        msg_format("Parsing '%s'", old);
        message_flush();
    }

    log_debug("Successfully processed preference file '%s' (%d lines)", name, line + 1);

    /* Close the file */
    sdl_fclose(fp);

    /* Result */
    return (err);
}

/*
 * Process the "user pref file" with the given name
 *
 * See the functions above for a list of legal "commands".
 *
 * We also accept the special "?" and "%" directives, which
 * allow conditional evaluation and filename inclusion.
 */
errr process_pref_file(cptr name)
{
    char buf[1024];

    errr err = 0;

    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_PREF, name);

    /* Process the pref file */
    err = process_pref_file_aux(buf);

    /* Stop at parser errors, but not at non-existing file */
    if (err < 1)
    {
        /* Build the filename */
        path_build(buf, sizeof(buf), ANGBAND_DIR_USER, name);

        /* Process the pref file */
        err = process_pref_file_aux(buf);
    }

    /* Result */
    return (err);
}

