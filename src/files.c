/* File: files.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "scorefile.h"
#include "score/score_logic.h"
#include "score/score_runs.h"
#include "score/score_ui.h"
#include "player/killer.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "h-basic.h"
#include "log/log.h"
#include "metarun.h"
#include "platform.h"
#include "sdl-config.h"
#include "z-term.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <stdint.h>

#ifdef WINDOWS
#include <windows.h>
#include <direct.h>  /* For _mkdir */
#else
#include <sys/stat.h>  /* For mkdir */
#include <dirent.h>    /* For directory operations */
#endif

/* Helper to build score/meta file path correctly for both portable and normal builds */
static bool build_meta_path(char* buf, size_t len, const char* filename)
{
#ifdef SIL_USE_LOCAL_DATA
    /* Portable build: in apex directory */
    return path_build(buf, len, ANGBAND_DIR_APEX, filename);
#else
    /* Normal build: in meta directory (parent of metaruns) */
    if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
        char meta_dir[1024];
        SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
        char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
        if (last_sep) *last_sep = '\0';
        return path_build(buf, len, meta_dir, filename);
    } else {
        return path_build(buf, len, ANGBAND_DIR_APEX, filename);
    }
#endif
}

// These are copied from birth.c and needed for displaying the character sheet
#define INSTRUCT_ROW 21
#define QUESTION_COL 2

/* Mini screenshot buffers (local to this module) */
static char mini_screenshot_char[7][7];
static byte mini_screenshot_attr[7][7];

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
void safe_setuid_drop(void)
{
#ifdef SET_UID

#ifdef SAFE_SETUID

#ifdef HAVE_SETEGID

    if (setegid(getgid()) != 0)
    {
        quit("setegid(): cannot set permissions correctly!");
    }

#else /* HAVE_SETEGID */

#ifdef SAFE_SETUID_POSIX

    if (setgid(getgid()) != 0)
    {
        quit("setgid(): cannot set permissions correctly!");
    }

#else /* SAFE_SETUID_POSIX */

    if (setregid(getegid(), getgid()) != 0)
    {
        quit("setregid(): cannot set permissions correctly!");
    }

#endif /* SAFE_SETUID_POSIX */

#endif /* HAVE_SETEGID */

#endif /* SAFE_SETUID */

#endif /* SET_UID */
}

/*
 * Hack -- grab permissions
 */
void safe_setuid_grab(void)
{
#ifdef SET_UID

#ifdef SAFE_SETUID

#ifdef HAVE_SETEGID

    if (setegid(player_egid) != 0)
    {
        quit("setegid(): cannot set permissions correctly!");
    }

#else /* HAVE_SETEGID */

#ifdef SAFE_SETUID_POSIX

    if (setgid(player_egid) != 0)
    {
        quit("setgid(): cannot set permissions correctly!");
    }

#else /* SAFE_SETUID_POSIX */

    if (setregid(getegid(), getgid()) != 0)
    {
        quit("setregid(): cannot set permissions correctly!");
    }

#endif /* SAFE_SETUID_POSIX */

#endif /* HAVE_SETEGID */

#endif /* SAFE_SETUID */

#endif /* SET_UID */
}

/*
 * Extract the first few "tokens" from a buffer
 *
 * This function uses "colon" and "slash" as the delimeter characters.
 *
 * We never extract more than "num" tokens.  The "last" token may include
 * "delimeter" characters, allowing the buffer to include a "string" token.
 *
 * We save pointers to the tokens in "tokens", and return the number found.
 *
 * Hack -- Attempt to handle the 'c' character formalism
 *
 * Hack -- An empty buffer, or a final delimeter, yields an "empty" token.
 *
 * Hack -- We will always extract at least one token
 */


/*------------------------------------------------------------------------
 *  A hard-coded priority list of races.  Highest-priority first.
 *  Fill this out with your own RACE_... constants, in the order you like.
 *------------------------------------------------------------------------*/
static const int race_priority[] = {
    3, //Sindar
    2, //Finarfin
    1, //Fingolfin
    5, //Dwarve
    6, //Edain
    0 //Feanor
};

s16b tokenize(char* buf, s16b num, char** tokens)
{
    int i = 0;

    char* s = buf;

    /* Process */
    while (i < num - 1)
    {
        char* t;

        /* Scan the string */
        for (t = s; *t; t++)
        {
            /* Found a delimiter */
            if ((*t == ':') || (*t == '/'))
                break;

            /* Handle single quotes */
            if (*t == '\'')
            {
                /* Advance */
                t++;

                /* Handle backslash */
                if (*t == '\\')
                    t++;

                /* Require a character */
                if (!*t)
                    break;

                /* Advance */
                t++;

                /* Hack -- Require a close quote */
                if (*t != '\'')
                    *t = '\'';
            }

            /* Handle back-slash */
            if (*t == '\\')
                t++;
        }

        /* Nothing left */
        if (!*t)
            break;

        /* Nuke and advance */
        *t++ = '\0';

        /* Save the token */
        tokens[i++] = s;

        /* Advance */
        s = t;
    }

    /* Save the token */
    tokens[i++] = s;

    /* Number found */
    return (i);
}

/*
 * Parse a sub-file of the "extra info" (format shown below)
 *
 * Each "action" line has an "action symbol" in the first column,
 * followed by a colon, followed by some command specific info,
 * usually in the form of "tokens" separated by colons or slashes.
 *
 * Blank lines, lines starting with white space, and lines starting
 * with pound signs ("#") are ignored (as comments).
 *
 * Note the use of "tokenize()" to allow the use of both colons and
 * slashes as delimeters, while still allowing final tokens which
 * may contain any characters including "delimiters".
 *
 * Note the use of "strtol()" to allow all "integers" to be encoded
 * in decimal, hexidecimal, or octal form.
 *
 * Note that "monster zero" is used for the "player" attr/char, "object
 * zero" will be used for the "stack" attr/char, and "feature zero" is
 * used for the "nothing" attr/char.
 *
 * Specify the attr/char values for "monsters" by race index.
 *   R:<num>:<a>/<c>
 *
 * Specify the attr/char values for "objects" by kind index.
 *   K:<num>:<a>/<c>
 *
 * Specify the attr/char values for "features" by feature index.
 *   F:<num>:<a>/<c>
 *
 * Specify the attr/char values for "special" things.
 *   S:<num>:<a>/<c>
 *
 * Specify the attribute values for inventory "objects" by kind tval.
 *   E:<tv>:<a>
 *
 * Define a macro action, given an encoded macro action.
 *   A:<str>
 *
 * Create a macro, given an encoded macro trigger.
 *   P:<str>
 *
 * Create a keymap, given an encoded keymap trigger.
 *   C:<num>:<str>
 *
 * Turn an option off, given its name.
 *   X:<str>
 *
 * Turn an option on, given its name.
 *   Y:<str>
 *
 * Turn a window flag on or off, given a window, flag, and value.
 *   W:<win>:<flag>:<value>
 *
 * Specify visual information, given an index, and some data.
 *   V:<num>:<kv>:<rv>:<gv>:<bv>
 *
 * Specify colors for message-types.
 *   M:<type>:<attr>
 *
 * Specify the attr/char values for "flavors" by flavors index.
 *   L:<num>:<a>/<c>
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

#ifdef CHECK_TIME

/*
 * Operating hours for ANGBAND (defaults to non-work hours)
 */
static char days[7][29] = { "SUN:XXXXXXXXXXXXXXXXXXXXXXXX",
    "MON:XXXXXXXX.........XXXXXXX", "TUE:XXXXXXXX.........XXXXXXX",
    "WED:XXXXXXXX.........XXXXXXX", "THU:XXXXXXXX.........XXXXXXX",
    "FRI:XXXXXXXX.........XXXXXXX", "SAT:XXXXXXXXXXXXXXXXXXXXXXXX" };

/*
 * Restict usage (defaults to no restrictions)
 */
static bool check_time_flag = false;

#endif /* CHECK_TIME */

/*
 * Handle CHECK_TIME
 */
errr check_time(void)
{
#ifdef CHECK_TIME

    time_t c;
    struct tm* tp;

    /* No restrictions */
    if (!check_time_flag)
        return (0);

    /* Check for time violation */
    c = time((time_t*)0);
    tp = localtime(&c);

    /* Violation */
    if (days[tp->tm_wday][tp->tm_hour + 4] != 'X')
        return (1);

#endif /* CHECK_TIME */

    /* Success */
    return (0);
}

/*
 * Initialize CHECK_TIME
 */
errr check_time_init(void)
{
#ifdef CHECK_TIME

    SDL_IOStream* fp;

    char buf[1024];

    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_FILE, "time.txt");

    /* Open the file */
    fp = sdl_fopen(buf, "r");

    /* No file, no restrictions */
    if (!fp)
        return (0);

    /* Assume restrictions */
    check_time_flag = true;

    /* Parse the file */
    while (0 == sdl_fgets(fp, buf, sizeof(buf)))
    {
        /* Skip comments and blank lines */
        if (!buf[0] || (buf[0] == '#'))
            continue;

        /* Chop the buffer */
        buf[sizeof(days[0]) - 1] = '\0';

        /* Extract the info */
        if (prefix(buf, "SUN:"))
            SDL_strlcpy(days[0], buf, sizeof(days[0]));
        if (prefix(buf, "MON:"))
            SDL_strlcpy(days[1], buf, sizeof(days[1]));
        if (prefix(buf, "TUE:"))
            SDL_strlcpy(days[2], buf, sizeof(days[2]));
        if (prefix(buf, "WED:"))
            SDL_strlcpy(days[3], buf, sizeof(days[3]));
        if (prefix(buf, "THU:"))
            SDL_strlcpy(days[4], buf, sizeof(days[4]));
        if (prefix(buf, "FRI:"))
            SDL_strlcpy(days[5], buf, sizeof(days[5]));
        if (prefix(buf, "SAT:"))
            SDL_strlcpy(days[6], buf, sizeof(days[6]));
    }

    /* Close it */
    sdl_fclose(fp);

#endif /* CHECK_TIME */

    /* Success */
    return (0);
}

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


/* ----- story-font aware helpers ---------------------------------------- */

static void story_c_put_str_grid(byte attr, cptr text, int row, int col, int width)
{
    if (sdl_is_story_font_enabled())
        story_print_text_grid(row, col, width, attr, text);
    else
        c_put_str(attr, text, row, col);
}

/* ===== 20-column, right-anchored stat lines ============================= */

#define LINEW20 20

static void put_label_fit(int x, int y, const char* label, int start)
{
    int maxw = start - x;
    if (maxw <= 0)
        return;

    char buf[64];
    strnfmt(buf, sizeof(buf), "%-*.*s", maxw, maxw, label);
    Term_putstr(x, y, -1, TERM_WHITE, buf);
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
/* ======================================================================= */

void display_player_xtra_info(int mode)
{
    const int col_stats = 1;     /* left stats column, width 20 */
    const int col_flags = 23;    /* single flags column in the gap */
    const int col_skills = 41;   /* skills unchanged */

    int row_stats = 2;
    int row_flags = 2;

    int skill, attacks = 1;
    char cur[32], rhs[32], val[64], buf[160];

    byte history_attr = (mode == 2) ? TERM_YELLOW : TERM_WHITE;

    /* -------------------- STATS (col 1..20) ----------------------------- */

    /* Exp: cur(5)/max(6) */
    strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);   /* <= 99999 */
    strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);       /* <= 999999 */
    put_pair20_right(col_stats, row_stats++,
                     "Exp",
                     cur, 5, TERM_L_GREEN,
                     '/', rhs, 6, TERM_L_GREEN);

    /* Burden: cur(4)/max(4) - integer pounds */
    {
        long cur_b = (long)(p_ptr->total_weight / 10L);
        long max_b = (long)(weight_limit() / 10L);
        strnfmt(cur, sizeof(cur), "%ld", cur_b);
        strnfmt(rhs, sizeof(rhs), "%ld", max_b);
        put_pair20_right(col_stats, row_stats++,
                         "Burden",
                         cur, 4, (cur_b <= max_b) ? TERM_L_GREEN : TERM_YELLOW,
                         '/', rhs, 4, TERM_L_GREEN);
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
    }

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
        attacks++;
        put_single20_right(col_stats, row_stats++,
                           "Melee x2", val, 12, TERM_L_BLUE);
    }

    /* Offhand if present */
    if (p_ptr->mds2 > 0)
    {
        attacks++;
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
        strnfmt(val, sizeof(val), "%s",
                b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name);
        put_single20_right(col_stats, row_stats++,
                           "Song", val, 14, TERM_L_BLUE);
    }
    if (p_ptr->song2 != SNG_NOTHING) {
        strnfmt(val, sizeof(val), "%s",
                b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name);
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
    HANDLE_UNIQUE_U("Orome Himself",      UNQ_WIL_FIN,      TERM_VIOLET);
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
        Term_putstr(col_flags, row_flags++, -1, uniq_buf[i].col, uniq_buf[i].txt);
    for (int i = 0; i < ma_n; ++i)
        Term_putstr(col_flags, row_flags++, -1, ma_buf[i].col, ma_buf[i].txt);
    for (int i = 0; i < af_n; ++i)
        Term_putstr(col_flags, row_flags++, -1, af_buf[i].col, af_buf[i].txt);
    for (int i = 0; i < pen_n; ++i)
        Term_putstr(col_flags, row_flags++, -1, pen_buf[i].col, pen_buf[i].txt);

    /* Disable story font after rendering flags/abilities */
    if (story_character_enabled()) {
        sdl_story_font_disable();
    }

    /* -------------------- SKILLS (unchanged position) ------------------- */
    /* Skills will manage their own font switching */
    for (skill = 0; skill < S_MAX; skill++) {
        /* Skip Special abilities skill - not meant for display */
        if (skill == S_SPC) continue;
        display_skill(skill, 6 + skill, col_skills);
    }

    /* -------------------- History (unchanged) --------------------------- */
    if (story_character_enabled()) {
        sdl_story_font_enable();
    }
    
    /* Use full terminal width for history wrapping */
    int wid, h;
    Term_get_size(&wid, &h);
    log_debug("Character history: terminal width=%d, using wrap=%d", wid, wid - 1);
    text_out_wrap   = wid - 1;  /* Leave 1 column margin */
    text_out_indent = 1;
    Term_gotoxy(text_out_indent, 15);
    text_out_to_screen(history_attr, p_ptr->history);
    text_out_wrap   = 0;
    text_out_indent = 0;
    
    Term_fresh();  /* Render history */

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
 * Display interactive character screen tutorial
 * Shows 4 stages explaining different parts of the character screen
 * with actual character data displayed
 */
static void tutorial_prompt_label(int binding, const char* fallback, char* out, size_t out_size)
{
    if (!out || !out_size)
        return;

    sdl_gamepad_action_binding_short_label(binding, out, out_size);
    if (streq(out, "(unbound)") || streq(out, "Multiple"))
        SDL_strlcpy(out, fallback, out_size);
}

void display_character_tutorial(void)
{
    int stage = 0;
    char ch;
    char buf[160];
    char cur[32], rhs[32];
    int row;
    
    /* Display each stage */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        bool steamdeck = steamdeck_controls_active();
        
        /* Stage header */
        Term_putstr(20, 0, -1, TERM_L_BLUE, 
            format("CHARACTER SCREEN TUTORIAL - STAGE %d/4", stage + 1));
        
        row = 2;
        
        /* Stage 1: Core Statistics (left column) */
        if (stage == 0)
        {
            Term_putstr(2, row++, -1, TERM_WHITE, "CORE STATISTICS");
            row++;
            
            /* Experience */
            strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);
            strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);
            put_pair20_right(2, row, "Exp", 
                cur, 6, TERM_L_GREEN, '/', rhs, 6, TERM_GREEN);
            Term_putstr(24, row++, -1, TERM_SLATE, 
                "-Experience: left/total, awarded for reaching depths,");
            Term_putstr(24, row++, -1, TERM_SLATE, 
                " identifying items and spotting and killing monsters");
            
            /* Burden */
            {
                long cur_wgt = p_ptr->total_weight / 10;
                long max_wgt = weight_limit() / 10;
                strnfmt(cur, sizeof(cur), "%ld", cur_wgt);
                strnfmt(rhs, sizeof(rhs), "%ld", max_wgt);
                put_pair20_right(2, row, "Burden",
                    cur, 4, TERM_L_GREEN, '/', rhs, 4, TERM_GREEN);
                Term_putstr(24, row++, -1, TERM_SLATE,
                    "-Weight carried/max capacity (lbs)");
            }
            
            /* Depth */
            if (turn > 0)
            {
                long cur_d = p_ptr->depth * 50;
                long min_d = min_depth() * 50;
                strnfmt(cur, sizeof(cur), "%ld", cur_d);
                strnfmt(rhs, sizeof(rhs), "%ld", min_d);
                put_pair20_right(2, row, "Depth c/m",
                    cur, 4, TERM_L_GREEN, '/', rhs, 4, TERM_GREEN);
                Term_putstr(24, row++, -1, TERM_SLATE,
                    "-Current level/minimum return depth, goes up with time");
            }
            
            /* Turn */
            comma_number(buf, playerturn);
            put_single20_right(2, row, "Turn", buf, 12, TERM_L_GREEN);
            Term_putstr(24, row++, -1, TERM_SLATE, "-Total current game turns");
            
            /* Light */
            strnfmt(buf, sizeof(buf), "%d", p_ptr->cur_light);
            put_single20_right(2, row, "Light", buf, 2, TERM_L_GREEN);
            Term_putstr(24, row++, -1, TERM_SLATE, "-Your current light radius");
            row++;
            
            /* Melee */
            strnfmt(buf, sizeof(buf), "(%+d,%dd%d)",
                p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
            put_single20_right(2, row, "Melee", buf, 12, TERM_L_GREEN);
            Term_putstr(24, row++, -1, TERM_SLATE, "-Main hand: (chance to hit, damage dice)");
            
            /* Bows */
            strnfmt(buf, sizeof(buf), "(%+d,%dd%d)",
                p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
            put_single20_right(2, row, "Bows", buf, 12, TERM_L_GREEN);
            Term_putstr(24, row++, -1, TERM_SLATE, "-Ranged:    (chance to hit, damage dice)");
            
            /* Armor */
            strnfmt(buf, sizeof(buf), "[%+d,%d-%d]",
                p_ptr->skill_use[S_EVN], p_min(GF_HURT, true), p_max(GF_HURT, true));
            put_single20_right(2, row, "Armor", buf, 12, TERM_L_GREEN);
            Term_putstr(24, row++, -1, TERM_SLATE, "-[evasion, protection]");
            Term_putstr(24, row++, -1, TERM_SLATE, " [chance increase, damage absorption]");
            
            /* Health */
            {
                int cur_hp = MIN(p_ptr->chp, 999);
                int max_hp = MIN(p_ptr->mhp, 999);
                byte col = (p_ptr->chp >= p_ptr->mhp) ? TERM_L_GREEN : 
                          (p_ptr->chp > p_ptr->mhp / 4) ? TERM_YELLOW : TERM_RED;
                strnfmt(cur, sizeof(cur), "%d", cur_hp);
                strnfmt(rhs, sizeof(rhs), "%d", max_hp);
                put_pair20_right(2, row, "Health",
                    cur, 3, col, '/', rhs, 3, TERM_GREEN);
                Term_putstr(24, row++, -1, TERM_SLATE, "-Hit points:  current/maximum");
            }
            
            /* Voice */
            {
                int cur_sp = MIN(p_ptr->csp, 999);
                int max_sp = MIN(p_ptr->msp, 999);
                byte col = (p_ptr->csp >= p_ptr->msp) ? TERM_L_GREEN :
                          (p_ptr->csp > p_ptr->msp / 4) ? TERM_YELLOW : TERM_RED;
                strnfmt(cur, sizeof(cur), "%d", cur_sp);
                strnfmt(rhs, sizeof(rhs), "%d", max_sp);
                put_pair20_right(2, row, "Voice",
                    cur, 3, col, '/', rhs, 3, TERM_GREEN);
                Term_putstr(24, row++, -1, TERM_SLATE, "-Song points: current/maximum");
            }
        }
        
        /* Stage 2: Attributes & Skills */
        else if (stage == 1)
        {
            Term_putstr(2, row++, -1, TERM_WHITE, "ATTRIBUTES & SKILLS");
            row++;
            
            Term_putstr(2, row++, -1, TERM_SLATE, "Attributes (Current = Base +equip +misc -drain):");
            
            /* Display stats with short names */
            for (int stat = 0; stat < A_MAX; stat++)
            {
                int use = p_ptr->stat_use[stat];
                int base = p_ptr->stat_base[stat];
                int equip_mod = p_ptr->stat_equip_mod[stat];
                int misc_mod = p_ptr->stat_misc_mod[stat];
                int drain = p_ptr->stat_drain[stat];
                
                byte attr = (use == base) ? TERM_WHITE : 
                           (use > base) ? TERM_L_GREEN : TERM_ORANGE;
                
                /* Use short names for display */
                Term_putstr(4, row, -1, TERM_WHITE, stat_names[stat]);
                
                /* Show breakdown if there are any modifiers */
                char value_buf[8];
                strnfmt(value_buf, sizeof(value_buf), "%2d", use);

                if (equip_mod != 0 || misc_mod != 0 || drain != 0)
                {
                    story_c_put_str_grid(attr, value_buf, row, 8, 2);
                    story_c_put_str_grid(TERM_SLATE, "=", row, 11, 1);

                    char base_buf[8];
                    strnfmt(base_buf, sizeof(base_buf), "%2d", base);
                    story_c_put_str_grid(TERM_WHITE, base_buf, row, 13, 2);

                    int col_pos = 16;
                    if (equip_mod != 0)
                    {
                        char mod_buf[8];
                        strnfmt(mod_buf, sizeof(mod_buf), "%+d", equip_mod);
                        story_c_put_str_grid(
                            equip_mod > 0 ? TERM_L_GREEN : TERM_ORANGE,
                            mod_buf, row, col_pos, 3);
                        col_pos += 3;
                    }
                    if (misc_mod != 0)
                    {
                        char mod_buf[8];
                        strnfmt(mod_buf, sizeof(mod_buf), "%+d", misc_mod);
                        story_c_put_str_grid(
                            misc_mod > 0 ? TERM_L_GREEN : TERM_ORANGE,
                            mod_buf, row, col_pos, 3);
                        col_pos += 3;
                    }
                    if (drain != 0)
                    {
                        char mod_buf[8];
                        strnfmt(mod_buf, sizeof(mod_buf), "%+d", drain);
                        story_c_put_str_grid(TERM_YELLOW, mod_buf, row, col_pos, 3);
                        col_pos += 3;
                    }
                }
                else
                {
                    /* No modifiers, just show the value */
                    story_c_put_str_grid(attr, value_buf, row, 8, 2);
                }
                
                /* Descriptions with full names - shortened to fit */
                if (stat == A_STR)
                    Term_putstr(28, row, -1, TERM_SLATE, "-Strength: melee dice & weight capacity");
                else if (stat == A_DEX)
                    Term_putstr(28, row, -1, TERM_SLATE, "-Dexterity: skill increase (mel, evn, arc, stl)");
                else if (stat == A_CON)
                    Term_putstr(28, row, -1, TERM_SLATE, "-Constitution: HP");
                else if (stat == A_GRA)
                    Term_putstr(28, row, -1, TERM_SLATE, "-Grace: skill increase (wil, per, sng, smt), voice");

                row++;
            }
            row++;
            
            Term_putstr(2, row++, -1, TERM_SLATE, "Skills: Total = Base +stat +equip +misc");
            Term_putstr(2, row++, -1, TERM_SLATE, "(Base determines ability purchase cost)");
            row++;
            
            /* Display all 8 skills with descriptions */
            const char* skill_desc[S_MAX] = {
                "-Melee combat chance to hit",      /* S_MEL */
                "-Ranged attack chance to hit",     /* S_ARC */
                "-Evade attack chance",     /* S_EVN */
                "-Avoid detection",   /* S_STL */
                "-Notice hidden",     /* S_PER */
                "-Mental resistance",     /* S_WIL */
                "-Craft items",       /* S_SMT */
                "-Song power",        /* S_SNG */
                ""                     /* S_SPC - skip */
            };
            
            for (int skill = 0; skill < S_MAX; skill++)
            {
                /* Skip Special abilities skill */
                if (skill == S_SPC) continue;
                
                /* Use full names for display */
                Term_putstr(4, row, -1, TERM_WHITE, skill_names_full[skill]);
                char total_buf[8];
                strnfmt(total_buf, sizeof(total_buf), "%2d", p_ptr->skill_use[skill]);
                story_c_put_str_grid(TERM_L_GREEN, total_buf, row, 16, 2);
                story_c_put_str_grid(TERM_SLATE, "=", row, 19, 1);

                char base_buf[8];
                strnfmt(base_buf, sizeof(base_buf), "%2d", p_ptr->skill_base[skill]);
                story_c_put_str_grid(TERM_GREEN, base_buf, row, 21, 2);
                
                int col_pos = 24;
                if (p_ptr->skill_stat_mod[skill] != 0)
                {
                    char mod_buf[8];
                    strnfmt(mod_buf, sizeof(mod_buf), "%+d", p_ptr->skill_stat_mod[skill]);
                    story_c_put_str_grid(TERM_WHITE, mod_buf, row, col_pos, 3);
                    col_pos += 3;
                }
                if (p_ptr->skill_equip_mod[skill] != 0)
                {
                    char mod_buf[8];
                    strnfmt(mod_buf, sizeof(mod_buf), "%+d", p_ptr->skill_equip_mod[skill]);
                    story_c_put_str_grid(TERM_WHITE, mod_buf, row, col_pos, 3);
                    col_pos += 3;
                }
                if (p_ptr->skill_misc_mod[skill] != 0)
                {
                    char mod_buf[8];
                    strnfmt(mod_buf, sizeof(mod_buf), "%+d", p_ptr->skill_misc_mod[skill]);
                    story_c_put_str_grid(TERM_WHITE, mod_buf, row, col_pos, 3);
                    col_pos += 3;
                }
                
                /* Add description */
                Term_putstr(36, row, -1, TERM_SLATE, skill_desc[skill]);
                row++;
            }
        }
        
        /* Stage 3: Character Traits */
        else if (stage == 2)
        {
            Term_putstr(2, row++, -1, TERM_WHITE, "CHARACTER TRAITS");
            row++;
            
            /* Character name */
            char name[40];
            if (p_ptr->oaths_broken)
            {
                strnfmt(name, sizeof(name), "%s the Oathbreaker", op_ptr->full_name);
                c_put_str(TERM_RED, name, row++, 4);
            }
            else
            {
                strnfmt(name, sizeof(name), "%s%s", op_ptr->full_name, 
                    c_name + current_character_profile->alt_name);
                c_put_str(TERM_L_BLUE, name, row++, 4);
            }
            row++;
            
            Term_putstr(2, row++, -1, TERM_SLATE, "Special Abilities & Modifiers:");
            row++;
            
            /* Show color coding examples in compact format */
            c_put_str(TERM_L_GREEN, "++", row, 4);
            Term_putstr(8, row, -1, TERM_SLATE, "- Mastery  ");
            c_put_str(TERM_GREEN, "+", row, 20);
            Term_putstr(23, row++, -1, TERM_SLATE, "- Affinity");
            
            c_put_str(TERM_RED, "--", row, 4);
            Term_putstr(8, row, -1, TERM_SLATE, "- Major penalty  ");
            c_put_str(TERM_L_RED, "-", row, 26);
            Term_putstr(29, row++, -1, TERM_SLATE, "- Minor penalty");
            
            c_put_str(TERM_VIOLET, "UNIQUE", row, 4);
            Term_putstr(12, row++, -1, TERM_SLATE, "- Special abilities");
            
            c_put_str(TERM_UMBER, "CURSE", row, 4);
            Term_putstr(12, row++, -1, TERM_SLATE, "- Character curses");
            row++;
            
            /* Show ALL actual traits using the same logic as character sheet */
            int race = p_ptr->prace;
            int character = p_ptr->pcharacter;
            
            Term_putstr(2, row++, -1, TERM_SLATE, "Your current traits:");
            
            /* Use buffers to collect and organize traits */
            typedef struct {
                const char *txt;
                byte col;
            } trait_line_t;
            
            trait_line_t trait_uniq[32], trait_ma[16], trait_af[16], trait_pen[32];
            int uniq_cnt = 0, ma_cnt = 0, af_cnt = 0, pen_cnt = 0;
            
            byte col_mastery = TERM_L_GREEN;
            byte col_affinity = TERM_GREEN;
            byte col_penalty = TERM_L_RED;
            byte col_gr_penalty = TERM_RED;
            
#define PUSH_TRAIT(arr, n, text, color) do { (arr)[(n)].txt = (text); (arr)[(n)++].col = (color); } while (0)

#define CHECK_SKILL(LABEL, AFF_FLAG, PEN_FLAG) \
    do { \
        int sc = 0; \
        if (p_info[race].flags & (AFF_FLAG)) sc++; \
        if (c_info[character].flags & (AFF_FLAG)) sc++; \
        if (p_info[race].flags & (PEN_FLAG)) sc--; \
        if (c_info[character].flags & (PEN_FLAG)) sc--; \
        sc += curse_flag_count_rhf(AFF_FLAG); \
        sc -= curse_flag_count_rhf(PEN_FLAG); \
        if (sc > 2) sc = 2; \
        if (sc < -2) sc = -2; \
        if (sc == 2) PUSH_TRAIT(trait_ma, ma_cnt, LABEL "++", col_mastery); \
        else if (sc == 1) PUSH_TRAIT(trait_af, af_cnt, LABEL "+", col_affinity); \
        else if (sc == -1) PUSH_TRAIT(trait_pen, pen_cnt, LABEL "-", col_penalty); \
        else if (sc == -2) PUSH_TRAIT(trait_pen, pen_cnt, LABEL "--", col_gr_penalty); \
    } while (0)

#define CHECK_UNIQUE(LABEL, FLAG, COLOR) \
    do { \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG))) \
            PUSH_TRAIT(trait_uniq, uniq_cnt, (LABEL), (COLOR)); \
    } while (0)

#define CHECK_UNIQUE_U(LABEL, FLAG, COLOR) \
    do { \
        if (c_info[character].flags_u & (FLAG)) \
            PUSH_TRAIT(trait_uniq, uniq_cnt, (LABEL), (COLOR)); \
    } while (0)

            /* Check all skills */
            CHECK_SKILL("melee", RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
            CHECK_SKILL("evasion", RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
            CHECK_SKILL("stealth", RHF_STL_AFFINITY, RHF_STL_PENALTY);
            CHECK_SKILL("archery", RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
            CHECK_SKILL("will", RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
            CHECK_SKILL("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
            CHECK_SKILL("smithing", RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
            CHECK_SKILL("song", RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
            CHECK_SKILL("bow", RHF_BOW_PROFICIENCY, 0);
            CHECK_SKILL("axe", RHF_AXE_PROFICIENCY, 0);
            
            /* Check unique abilities */
            CHECK_UNIQUE_U("Master Artisan", UNQ_SMT_FEANOR, TERM_VIOLET);
            CHECK_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL, TERM_VIOLET);
            CHECK_UNIQUE_U("Chosen of Ulmo", UNQ_WIL_TUOR, TERM_VIOLET);
            CHECK_UNIQUE_U("Indomitable Will", UNQ_EARENDIL, TERM_VIOLET);
            CHECK_UNIQUE_U("Orome Himself", UNQ_WIL_FIN, TERM_VIOLET);
            CHECK_UNIQUE_U("Songs of Power", UNQ_SNG_FIN, TERM_VIOLET);
            CHECK_UNIQUE_U("Elven Dance", UNQ_SNG_LUT, TERM_VIOLET);
            CHECK_UNIQUE_U("Girdle of Melian", UNQ_SNG_MEL, TERM_VIOLET);
            CHECK_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR, TERM_VIOLET);
            CHECK_UNIQUE_U("Old Master", UNQ_SMT_GAMIL, TERM_VIOLET);
            CHECK_UNIQUE_U("Ring Master", UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
            CHECK_UNIQUE_U("Aure entuluva", UNQ_SNG_HURIN, TERM_VIOLET);
            CHECK_UNIQUE_U("Voice of the Girdle", UNQ_SNG_THINGOL, TERM_VIOLET);
            CHECK_UNIQUE_U("Forgotten", UNQ_MIM, TERM_VIOLET);
            CHECK_UNIQUE_U("One Handed", UNQ_MEL_MAEDHROS, TERM_VIOLET);
            CHECK_UNIQUE_U("Agarwaen", UNQ_WIL_TURIN, TERM_VIOLET);
            CHECK_UNIQUE_U("Shadow Walker", UNQ_SNG_TURGON, TERM_VIOLET);
            CHECK_UNIQUE_U("Minstrel", UNQ_MINSTREL, TERM_VIOLET);
            CHECK_UNIQUE_U("Woven Master", UNQ_WOVEN_MASTER, TERM_VIOLET);
            CHECK_UNIQUE("Gift of Eru", RHF_GIFTERU, TERM_VIOLET);
            CHECK_UNIQUE("Seafarer", RHF_FREE, TERM_VIOLET);
            
            /* Check curses */
            CHECK_UNIQUE("Kinslayer", RHF_KINSLAYER, TERM_UMBER);
            CHECK_UNIQUE("Treacherous", RHF_TREACHERY, TERM_UMBER);
            CHECK_UNIQUE("Doom of Mandos", RHF_CURSE, TERM_UMBER);
            CHECK_UNIQUE("Morgoth Curse", RHF_MOR_CURSE, TERM_UMBER);
            
            /* Display in two columns: uniques -> masteries -> affinities -> penalties */
            int total_traits = uniq_cnt + ma_cnt + af_cnt + pen_cnt;
            
            if (total_traits == 0)
            {
                Term_putstr(4, row++, -1, TERM_SLATE, "(No special traits)");
            }
            else
            {
                /* Two-column layout: col1 at x=4, col2 at x=42 */
                int display_row = row;
                int col1_items = 0;
                int col2_items = 0;
                
                /* Count items for each column (try to balance) */
                int half = (total_traits + 1) / 2;
                
                /* Display uniques */
                for (int i = 0; i < uniq_cnt; ++i)
                {
                    if (col1_items < half)
                    {
                        c_put_str(trait_uniq[i].col, trait_uniq[i].txt, display_row++, 4);
                        col1_items++;
                    }
                    else
                    {
                        c_put_str(trait_uniq[i].col, trait_uniq[i].txt, row + col2_items, 42);
                        col2_items++;
                    }
                }
                
                /* Display masteries */
                for (int i = 0; i < ma_cnt; ++i)
                {
                    if (col1_items < half)
                    {
                        c_put_str(trait_ma[i].col, trait_ma[i].txt, display_row++, 4);
                        col1_items++;
                    }
                    else
                    {
                        c_put_str(trait_ma[i].col, trait_ma[i].txt, row + col2_items, 42);
                        col2_items++;
                    }
                }
                
                /* Display affinities */
                for (int i = 0; i < af_cnt; ++i)
                {
                    if (col1_items < half)
                    {
                        c_put_str(trait_af[i].col, trait_af[i].txt, display_row++, 4);
                        col1_items++;
                    }
                    else
                    {
                        c_put_str(trait_af[i].col, trait_af[i].txt, row + col2_items, 42);
                        col2_items++;
                    }
                }
                
                /* Display penalties */
                for (int i = 0; i < pen_cnt; ++i)
                {
                    if (col1_items < half)
                    {
                        c_put_str(trait_pen[i].col, trait_pen[i].txt, display_row++, 4);
                        col1_items++;
                    }
                    else
                    {
                        c_put_str(trait_pen[i].col, trait_pen[i].txt, row + col2_items, 42);
                        col2_items++;
                    }
                }
                
                /* Advance row past all displayed items */
                row = display_row;
            }
            
#undef PUSH_TRAIT
#undef CHECK_SKILL
#undef CHECK_UNIQUE
#undef CHECK_UNIQUE_U
        }
        
        /* Stage 4: History & Most Important Game Controls */
        else if (stage == 3)
        {
            Term_putstr(2, row++, -1, TERM_WHITE, "HISTORY & GAME CONTROLS");
            row++;
            
            /* Display abbreviated history (3 lines max) */
            Term_putstr(2, row++, -1, TERM_SLATE, "Your story:");
            
            text_out_wrap = 76;
            text_out_indent = 4;
            Term_gotoxy(text_out_indent, row);
            
            /* Truncate history for tutorial to fit in 3 lines */
            char hist_preview[150];
            SDL_strlcpy(hist_preview, p_ptr->history, sizeof(hist_preview));
            if (strlen(p_ptr->history) > 140)
            {
                hist_preview[137] = '.';
                hist_preview[138] = '.';
                hist_preview[139] = '.';
                hist_preview[140] = '\0';
            }
            text_out_to_screen(TERM_WHITE, hist_preview);
            
            text_out_wrap = 0;
            text_out_indent = 0;
            
            row += 4;
            
            Term_putstr(2, row++, -1, TERM_SLATE, "Essential Controls:");

            /* Two-column layout for commands */
            int cmd_row = row;

            if (steamdeck) {
                char confirm_label[16];
                char use_label[16];
                char examine_label[16];
                char inven_label[16];
                char equip_label[16];
                char look_label[16];
                char char_label[16];
                char fire_label[16];
                char sing_label[16];
                char activate_label[16];
                char map_label[16];
                char bash_label[16];
                char abilities_label[16];
                char help_label[16];
                char menu_label[16];
                char shift_label[16];
                char ctrl_label[16];

                tutorial_prompt_label(' ', "A", confirm_label, sizeof(confirm_label));
                tutorial_prompt_label('u', "X", use_label, sizeof(use_label));
                tutorial_prompt_label('x', "RS Right", examine_label, sizeof(examine_label));
                tutorial_prompt_label('i', "R1", inven_label, sizeof(inven_label));
                tutorial_prompt_label('e', "L1", equip_label, sizeof(equip_label));
                tutorial_prompt_label('l', "L1+R1", look_label, sizeof(look_label));
                tutorial_prompt_label('h', "R5", char_label, sizeof(char_label));
                tutorial_prompt_label('f', "B", fire_label, sizeof(fire_label));
                tutorial_prompt_label('s', "Y", sing_label, sizeof(sing_label));
                tutorial_prompt_label('a', "RS Left", activate_label, sizeof(activate_label));
                tutorial_prompt_label('M', "RS Up", map_label, sizeof(map_label));
                tutorial_prompt_label('b', "RS Down", bash_label, sizeof(bash_label));
                tutorial_prompt_label('\t', "L5", abilities_label, sizeof(abilities_label));
                tutorial_prompt_label('?', "?", help_label, sizeof(help_label));
                tutorial_prompt_label('m', "Start", menu_label, sizeof(menu_label));
                tutorial_prompt_label(GAMEPAD_BIND_SHIFT, "L2", shift_label, sizeof(shift_label));
                tutorial_prompt_label(GAMEPAD_BIND_CTRL, "R2", ctrl_label, sizeof(ctrl_label));

                /* Left column */
                c_put_str(TERM_L_WHITE, "D-pad/Left Stick", cmd_row, 4);
                Term_putstr(22, cmd_row++, -1, TERM_SLATE, "-Move/attack");

                c_put_str(TERM_L_WHITE, confirm_label, cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Pick up item");

                c_put_str(TERM_L_WHITE, use_label, cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Use item");

                c_put_str(TERM_L_WHITE, examine_label, cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Examine item");

                c_put_str(TERM_L_WHITE, inven_label, cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Inventory");

                c_put_str(TERM_L_WHITE, equip_label, cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Equipment");

                c_put_str(TERM_L_WHITE, look_label, cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Look menu");

                c_put_str(TERM_L_WHITE, char_label, cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Character screen");

                /* Right column */
                cmd_row = row;

                c_put_str(TERM_L_WHITE, fire_label, cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Fire (primary)");

                c_put_str(TERM_L_WHITE, sing_label, cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Sing");

                c_put_str(TERM_L_WHITE, activate_label, cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Activate stuff");

                c_put_str(TERM_L_WHITE, map_label, cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Map");

                c_put_str(TERM_L_WHITE, bash_label, cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Bash");

                c_put_str(TERM_L_WHITE, abilities_label, cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Abilities menu");

                c_put_str(TERM_L_WHITE, help_label, cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Help");

                c_put_str(TERM_L_WHITE, menu_label, cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Main menu");

                {
                    char note_buf[120];
                    strnfmt(note_buf, sizeof(note_buf),
                            "Shift: %s+%s=Stealth, %s+%s=Second quiver",
                            shift_label, sing_label, shift_label, fire_label);
                    Term_putstr(4, cmd_row + 1, -1, TERM_SLATE, note_buf);
                    strnfmt(note_buf, sizeof(note_buf), "Ctrl: %s+dir = Bash/Disarm/Tunnel", ctrl_label);
                    Term_putstr(4, cmd_row + 2, -1, TERM_SLATE, note_buf);
                    cmd_row += 2;
                }

                Term_putstr(4, cmd_row += 1, -1, TERM_SLATE,
                            "Bindings can be changed in Controller Settings.");
            } else {
                /* Left column */
                c_put_str(TERM_L_WHITE, "Numpad", cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Move/attack");

                c_put_str(TERM_L_WHITE, "Space", cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Pick up item");

                c_put_str(TERM_L_WHITE, "u", cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Use item");

                c_put_str(TERM_L_WHITE, "x", cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Examine item");

                c_put_str(TERM_L_WHITE, "i", cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Inventory");

                c_put_str(TERM_L_WHITE, "e", cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Equipment");

                c_put_str(TERM_L_WHITE, "l", cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Look menu");

                c_put_str(TERM_L_WHITE, "Ctrl", cmd_row, 4);
                Term_putstr(11, cmd_row++, -1, TERM_SLATE, "-Bash, disarm, tunnel");

                /* Right column */
                cmd_row = row;

                c_put_str(TERM_L_WHITE, "f/F", cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-fire (ranged attack)");

                c_put_str(TERM_L_WHITE, "s/S", cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Sing/Stealth");

                c_put_str(TERM_L_WHITE, "a", cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Activate stuff");

                c_put_str(TERM_L_WHITE, "c", cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Close door");

                c_put_str(TERM_L_WHITE, "h", cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Character screen");

                c_put_str(TERM_L_WHITE, "m", cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Main menu");

                c_put_str(TERM_L_WHITE, "Tab", cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Abilities menu");

                c_put_str(TERM_L_WHITE, "?", cmd_row, 42);
                Term_putstr(46, cmd_row++, -1, TERM_SLATE, "-Help");

                Term_putstr(11, cmd_row += 2, -1, TERM_SLATE, "Keyboard shortcuts could be changed through user preference");
            }

            row = cmd_row;
        }
        
        /* Footer */
        if (stage < 3)
            Term_putstr(18, 22, -1, TERM_YELLOW,
                steamdeck ? "D-pad left/right to navigate" : "Use arrows or any key to navigate");
        else
            Term_putstr(26, 22, -1, TERM_L_GREEN, "Tutorial complete!");

        if (steamdeck) {
            char next_label[16];
            char back_label[16];
            tutorial_prompt_label(' ', "A", next_label, sizeof(next_label));
            tutorial_prompt_label('b', "b", back_label, sizeof(back_label));
            if (stage > 0)
                Term_putstr(8, 23, -1, TERM_SLATE, "(D-Left Previous)");
            if (stage < 3) {
                char next_buf[32];
                strnfmt(next_buf, sizeof(next_buf), "(%s Next)", next_label);
                Term_putstr(52, 23, -1, TERM_SLATE, next_buf);
            }
            {
                char exit_buf[32];
                strnfmt(exit_buf, sizeof(exit_buf), "(%s to exit)", back_label);
                Term_putstr(30, 23, -1, TERM_SLATE, exit_buf);
            }
        } else {
            if (stage > 0)
                Term_putstr(10, 23, -1, TERM_SLATE, "(4/<- Previous)");
            if (stage < 3)
                Term_putstr(53, 23, -1, TERM_SLATE, "(Next 6/->)");
            Term_putstr(30, 23, -1, TERM_SLATE, "(ESC to exit)");
        }
        
        /* Wait for any key press */
        ch = inkey();
        
        /* Handle navigation */
        if (ch == ESCAPE || (steamdeck && ch == 'b'))
        {
            /* Exit tutorial */
            break;
        }
        else if (ch == '4')
        {
            /* Go back one stage (left arrow/numpad 4) */
            if (stage > 0)
                stage--;
        }
        else if (ch == '6' || ch == ' ' || ch == '\r')
        {
            /* Go forward one stage (right arrow/numpad 6, space, enter) */
            if (stage < 3)
                stage++;
            else
                break;  /* Exit on last stage */
        }
        else
        {
            /* Any other key advances */
            if (stage < 3)
                stage++;
            else
                break;  /* Exit on last stage */
        }
    }
    
    /* Clear screen before returning */
    Term_clear();
}

/*
 * Special display, part 2a
 */
static void display_player_misc_info(void)
{
    /* Name */
    char name[40];
    
    if (story_character_enabled()) {
        sdl_story_font_enable();
    }
    
    if (p_ptr->oaths_broken) {
        /* Show "the Oathbreaker" in red if any oath is broken */
        strnfmt(name, sizeof(name), "%s the Oathbreaker", op_ptr->full_name);
        c_put_str(TERM_RED, name, 0, 20);
    } else {
        /* Normal display with character title */
        strnfmt(name, sizeof(name), "%s%s", op_ptr->full_name, c_name + current_character_profile->alt_name);
        c_put_str(TERM_L_BLUE, name, 0, 20);
    }
    
    if (story_character_enabled()) {
        sdl_story_font_disable();
    }

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
    /* Erase screen */
    clear_from(0);

    /* All Modes Use Stat info */
    display_player_stat_info(1, 41);

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

/*
 * Make a string lower case.
 */
static void string_lower(char* buf)
{
    char* s;

    /* Lowercase the string */
    for (s = buf; *s != 0; s++)
        *s = tolower((unsigned char)*s);
}

/*
 * Show the contents of a char buffer on the screen and allow scrolling.
 * Based on show_file.
 */
bool show_buffer(cptr main_buffer, int line)
{
    int i, j, k;

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
            /* Wait for it */
            Term_putstr(1, hgt - 2, -1, TERM_SLATE, "(press ESC to exit)");
            Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
            Term_putstr(20, hgt - 2, -1, TERM_L_WHITE, "");
        }

        /* Prompt -- large files */
        else
        {
            /* Wait for it */
            Term_putstr(1, hgt - 2, -1, TERM_SLATE,
                "(press ESC to exit, Space for next page, Arrows/Keypad to "
                "scroll)");
            Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
            Term_putstr(21, hgt - 2, -1, TERM_L_WHITE, "Space");
            Term_putstr(42, hgt - 2, -1, TERM_L_WHITE, "Arrows");
            Term_putstr(49, hgt - 2, -1, TERM_L_WHITE, "Keypad");
            Term_putstr(67, hgt - 2, -1, TERM_L_WHITE, "");
        }

        /* Get a keypress */
        ch = inkey();

        /* Back up one line */
        if ((ch == '8') || (ch == '='))
        {
            line = line - 1;
            if (line < 0)
                line = 0;
        }

        /* Advance one line */
        if ((ch == '2') || (ch == '\n') || (ch == '\r'))
        {
            line = line + 1;
        }

        /* Advance one full page */
        if ((ch == '3') || (ch == ' '))
        {
            line = line + (hgt - 5);
        }

        /* Exit on escape */
        if (ch == ESCAPE)
            break;
    }

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
            prt("[Press a Number, or ESC to exit.]", hgt - 1, 0);
        }

        /* Prompt -- small files */
        else if (size <= hgt - 5)
        {
            /* Wait for it */
            Term_putstr(1, hgt - 2, -1, TERM_SLATE, "(press ESC to exit)");
            Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
            Term_putstr(20, hgt - 2, -1, TERM_L_WHITE, "");
        }

        /* Prompt -- large files */
        else
        {
            /* Wait for it */
            Term_putstr(1, hgt - 2, -1, TERM_SLATE,
                "(press ESC to exit, Space for next page, Arrows/Keypad to "
                "scroll)");
            Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
            Term_putstr(21, hgt - 2, -1, TERM_L_WHITE, "Space");
            Term_putstr(42, hgt - 2, -1, TERM_L_WHITE, "Arrows");
            Term_putstr(49, hgt - 2, -1, TERM_L_WHITE, "Keypad");
            Term_putstr(67, hgt - 2, -1, TERM_L_WHITE, "");
        }

        /* Get a keypress */
        ch = inkey();

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
        if ((ch == '2') || (ch == '\n') || (ch == '\r'))
        {
            line = line + 1;
        }

        /* Advance one half page */
        if (ch == '+')
        {
            line = line + ((hgt - 5) / 2);
        }

        /* Advance one full page */
        if ((ch == '3') || (ch == ' '))
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

#define HELP_TOTAL_PAGES 8

/* Drop-in replacement for show_help_screen(int i)
 * Adds a tiny role-based colour shim for consistent, accessible styling.
 *
 * Extras in this version:
 *  - Element-specific roles (darkness, poison, cold, fire, +light, lightning, acid)
 *  - Page 8: Steam Deck controls, with a drawn layout and dynamic key enum/mapping.
 *
 * Usage: paste this whole block where show_help_screen is defined. It only
 * depends on c_put_str() and the TERM_* colour constants already in your codebase.
 */

/* -------- Role-based colour shim ---------------------------------------- */

typedef enum {
    ROLE_HEADER,  /* Page title */
    ROLE_SECTION, /* Section headings */
    ROLE_BODY,    /* Main body text */
    ROLE_SUBTLE,  /* Hints/parentheticals */
    ROLE_GOOD,    /* Positive things, boons, successes */
    ROLE_WARN,    /* Caution/thresholds */
    ROLE_BAD,     /* Harmful/danger state */
    ROLE_TERM,    /* Game terms/keywords */
    ROLE_KEY,     /* Literal keys and glyphs in docs (NOT gameplay glyphs) */
    ROLE_UI,      /* Meta UI labels (menus, screens) */
    /* Element roles (so we can colour by element consistently) */
    ROLE_ELEM_FIRE,
    ROLE_ELEM_COLD,
    ROLE_ELEM_POISON,
    ROLE_ELEM_DARKNESS,
    ROLE_ELEM_LIGHT,
    ROLE__COUNT
} color_role_t;

/* Default theme (dark background). You can swap values at runtime if you add a menu hook. */
static int HELP_THEME[ROLE__COUNT] = {
    [ROLE_HEADER]       = TERM_L_WHITE + TERM_SHADE,
    [ROLE_SECTION]      = TERM_YELLOW,
    [ROLE_BODY]         = TERM_L_WHITE,
    [ROLE_SUBTLE]       = TERM_SLATE,
    [ROLE_GOOD]         = TERM_L_GREEN,
    [ROLE_WARN]         = TERM_ORANGE,
    [ROLE_BAD]          = TERM_L_RED,
    [ROLE_TERM]         = TERM_L_BLUE,
    [ROLE_KEY]          = TERM_WHITE,
    [ROLE_UI]           = TERM_UMBER,
    /* Elements per user request */
    [ROLE_ELEM_FIRE]        = TERM_L_RED,
    [ROLE_ELEM_COLD]        = TERM_BLUE,   /* cold -> blue */
    [ROLE_ELEM_POISON]      = TERM_L_GREEN,  /* poison -> green */
    [ROLE_ELEM_DARKNESS]    = TERM_L_DARK,   /* darkness -> dark */
    [ROLE_ELEM_LIGHT]       = TERM_YELLOW,   /* light/radiance */
};

static inline void put_role(color_role_t role, const char *s, int row, int col) {
    c_put_str(HELP_THEME[role], s, row, col);
}


static void help_binding_action_label(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    switch (binding) {
    case GAMEPAD_BIND_NONE:
        SDL_strlcpy(buf, "Unbound", buflen);
        return;
    case GAMEPAD_BIND_SHIFT:
        SDL_strlcpy(buf, "Shift modifier", buflen);
        return;
    case GAMEPAD_BIND_CTRL:
        SDL_strlcpy(buf, "Ctrl modifier", buflen);
        return;
    case GAMEPAD_BIND_ALT:
        SDL_strlcpy(buf, "Alt modifier", buflen);
        return;
    case ' ':
        SDL_strlcpy(buf, "Confirm (Space)", buflen);
        return;
    case '\r':
        SDL_strlcpy(buf, "Enter", buflen);
        return;
    case ESCAPE:
        SDL_strlcpy(buf, "Back (Esc)", buflen);
        return;
    case '\t':
        SDL_strlcpy(buf, "Abilities (Tab)", buflen);
        return;
    case 'i':
        SDL_strlcpy(buf, "Inventory (i)", buflen);
        return;
    case 'e':
        SDL_strlcpy(buf, "Equipment (e)", buflen);
        return;
    case 'u':
        SDL_strlcpy(buf, "Use item (u)", buflen);
        return;
    case 'x':
        SDL_strlcpy(buf, "Examine item (x)", buflen);
        return;
    case 's':
        SDL_strlcpy(buf, "Sing (s)", buflen);
        return;
    case 'S':
        SDL_strlcpy(buf, "Stealth (S)", buflen);
        return;
    case 'f':
        SDL_strlcpy(buf, "Fire (f)", buflen);
        return;
    case 'F':
        SDL_strlcpy(buf, "Second quiver (F)", buflen);
        return;
    case 'h':
        SDL_strlcpy(buf, "Character sheet (h)", buflen);
        return;
    case 'l':
        SDL_strlcpy(buf, "Look (l)", buflen);
        return;
    case 'o':
        SDL_strlcpy(buf, "Open (o)", buflen);
        return;
    case 'q':
        SDL_strlcpy(buf, "Quaff (q)", buflen);
        return;
    case 'r':
        SDL_strlcpy(buf, "Remove (r)", buflen);
        return;
    case 'a':
        SDL_strlcpy(buf, "Activate (a)", buflen);
        return;
    case 'M':
        SDL_strlcpy(buf, "Map (M)", buflen);
        return;
    case 'b':
        SDL_strlcpy(buf, "Bash (b)", buflen);
        return;
    case 'j':
        SDL_strlcpy(buf, "Supplies (j)", buflen);
        return;
    case 'm':
        SDL_strlcpy(buf, "Main menu (m)", buflen);
        return;
    case '?':
        SDL_strlcpy(buf, "Help (?)", buflen);
        return;
    default:
        if (binding >= 32 && binding <= 126)
            strnfmt(buf, buflen, "Key '%c'", binding);
        else
            strnfmt(buf, buflen, "Key %d", binding);
        return;
    }
}

static void help_binding_action_short(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    switch (binding) {
    case GAMEPAD_BIND_NONE:
        SDL_strlcpy(buf, "Unbound", buflen);
        return;
    case GAMEPAD_BIND_SHIFT:
        SDL_strlcpy(buf, "Shift", buflen);
        return;
    case GAMEPAD_BIND_CTRL:
        SDL_strlcpy(buf, "Ctrl", buflen);
        return;
    case GAMEPAD_BIND_ALT:
        SDL_strlcpy(buf, "Alt", buflen);
        return;
    case ' ':
        SDL_strlcpy(buf, "Confirm", buflen);
        return;
    case ESCAPE:
        SDL_strlcpy(buf, "Back", buflen);
        return;
    case '\t':
        SDL_strlcpy(buf, "Abilities", buflen);
        return;
    case 'i':
        SDL_strlcpy(buf, "Inventory", buflen);
        return;
    case 'e':
        SDL_strlcpy(buf, "Equipment", buflen);
        return;
    case 'u':
        SDL_strlcpy(buf, "Use", buflen);
        return;
    case 'x':
        SDL_strlcpy(buf, "Examine", buflen);
        return;
    case 's':
        SDL_strlcpy(buf, "Sing", buflen);
        return;
    case 'S':
        SDL_strlcpy(buf, "Stealth", buflen);
        return;
    case 'f':
        SDL_strlcpy(buf, "Fire", buflen);
        return;
    case 'F':
        SDL_strlcpy(buf, "Second", buflen);
        return;
    case 'h':
        SDL_strlcpy(buf, "Character", buflen);
        return;
    case 'l':
        SDL_strlcpy(buf, "Look", buflen);
        return;
    case 'o':
        SDL_strlcpy(buf, "Open", buflen);
        return;
    case 'q':
        SDL_strlcpy(buf, "Quaff", buflen);
        return;
    case 'r':
        SDL_strlcpy(buf, "Remove", buflen);
        return;
    case 'a':
        SDL_strlcpy(buf, "Activate", buflen);
        return;
    case 'M':
        SDL_strlcpy(buf, "Map", buflen);
        return;
    case 'b':
        SDL_strlcpy(buf, "Bash", buflen);
        return;
    case 'j':
        SDL_strlcpy(buf, "Supplies", buflen);
        return;
    case 'm':
        SDL_strlcpy(buf, "Menu", buflen);
        return;
    case '?':
        SDL_strlcpy(buf, "Help", buflen);
        return;
    default:
        if (binding >= 32 && binding <= 126)
            strnfmt(buf, buflen, "%c", binding);
        else
            strnfmt(buf, buflen, "%d", binding);
        return;
    }
}

static void help_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

/* -------- Help pages ----------------------------------------------------- */

void show_help_screen(int i)
{
    int row, col, col2;
    char page_header[96];

    switch (i)
    {
    case 1:
    {
        /* SIL-MORE: HELP [1/8]: GOAL & HEROES */
        row = 0; col = 1;
        sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: GOAL & HEROES", i, HELP_TOTAL_PAGES);
        put_role(ROLE_HEADER, page_header, row, col);
        row += 2;

        put_role(ROLE_SECTION, "GOAL", row, col); row++;
        put_role(ROLE_BODY, "- Steal ", row, col);
        put_role(ROLE_TERM, "Silmarils", row, col + 8);
        put_role(ROLE_BODY, " across runs; the saga ends when you've taken ", row, col + 17);
        put_role(ROLE_WARN, "fifteen", row, col + 64);
        put_role(ROLE_BODY, ".", row, col + 71);
        row++;
        put_role(ROLE_BODY, "- Plan for the ", row, col);
        put_role(ROLE_TERM, "long war", row, col + 15);
        put_role(ROLE_BODY, ": every ", row, col + 23);
        put_role(ROLE_TERM, "Silmaril", row, col + 31);
        put_role(ROLE_BODY, " twists ", row, col + 39);
        put_role(ROLE_TERM, "fate", row, col + 47);
        put_role(ROLE_BODY, " and reshapes play.", row, col + 51);
        row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_BAD, "Permadeath", row, col + 2);
        put_role(ROLE_BODY, ": a fallen hero is gone for the saga.", row, col + 12);
        row++;
        put_role(ROLE_BODY, "- The saga ends when no heroes remain. Worthy deaths earn ", row, col);
        put_role(ROLE_GOOD, "Valar blessings", row, col + 58);
        put_role(ROLE_BODY, ".", row, col + 73);
        row += 2;

        put_role(ROLE_SECTION, "HEROES OF LEGEND", row, col); row++;
        put_role(ROLE_BODY, "- Choose a fixed hero: ", row, col);
        put_role(ROLE_TERM, "Feanor, Fingolfin, Beren, Luthien", row, col + 23);
        put_role(ROLE_BODY, ", and others.", row, col + 56);
        row++;
        put_role(ROLE_BODY, "- Each bears a signature trait: ", row, col);
        put_role(ROLE_TERM, "Master Artisan, Elven Dance, Creator of Angrist", row, col + 32);
        put_role(ROLE_BODY, ".", row, col + 80);
        row++;
        put_role(ROLE_BODY, "- Some traits are shared across lines: ", row, col);
        put_role(ROLE_TERM, "Kinslayer, Gift of Eru", row, col + 39);
        put_role(ROLE_BODY, ", and more.", row, col + 61);
        row++;
        put_role(ROLE_BODY, "- A power rating is shown during selection. New? Start with the most powerful.", row, col);
        row++;
        put_role(ROLE_BODY, "- Expert? Forge your own path-synergy beats raw rating.", row, col);
        row++;
        put_role(ROLE_BODY, "- Remember: the game ends after 15 ", row, col);
        put_role(ROLE_TERM, "Silmarils", row, col + 36);
        put_role(ROLE_BODY, ", not one.", row, col + 45);
        row++;
        put_role(ROLE_BODY, "- Tags are intentionally sparse-learn by doing.", row, col);
        row++;
        put_role(ROLE_BODY, "- Hint - The Stave of Self-Knowledge can show hidden traits.", row, col);
        row += 2;

        put_role(ROLE_SECTION, "HELP FROM VALAR", row, col); row++;
        put_role(ROLE_BODY, "- The ", row, col);
        put_role(ROLE_TERM, "Valar", row, col + 6);
        put_role(ROLE_BODY, " guide worthy heroes through ", row, col + 11);
        put_role(ROLE_TERM, "sacred quests", row, col + 40);
        put_role(ROLE_BODY, ".", row, col + 53);
        row++;
        put_role(ROLE_BODY, "- Seek the ", row, col);
        put_role(ROLE_TERM, "halls of knowledge", row, col + 11);
        put_role(ROLE_BODY, " where ancient wisdom dwells.", row, col + 29);
        row++;
        put_role(ROLE_BODY, "- Each quest reveals ", row, col);
        put_role(ROLE_TERM, "hidden truths", row, col + 21);
        put_role(ROLE_BODY, " and grants ", row, col + 34);
        put_role(ROLE_GOOD, "divine blessings", row, col + 46);
        put_role(ROLE_BODY, ".", row, col + 62);
        row++;
        put_role(ROLE_BODY, "- The path of ", row, col);
        put_role(ROLE_WARN, "redemption", row, col + 14);
        put_role(ROLE_BODY, " is always open to those who seek it.", row, col + 24);
        break;
    }

    case 2:
    {
        /* SIL-MORE: HELP [2/8]: START & DEPTH */
        row = 0; col = 1;
        sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: START & DEPTH", i, HELP_TOTAL_PAGES);
        put_role(ROLE_HEADER, page_header, row, col);
        row += 2;

        put_role(ROLE_SECTION, "START", row, col); row++;
        put_role(ROLE_BODY, "- You begin with a ", row, col);
        put_role(ROLE_SUBTLE, "basic weapon", row, col + 19);
        put_role(ROLE_BODY, " and ", row, col + 31);
        put_role(ROLE_WARN, "no armour", row, col + 36);
        put_role(ROLE_BODY, "-", row, col + 45);
        put_role(ROLE_GOOD, "gear up fast", row, col + 46);
        put_role(ROLE_BODY, ".", row, col + 58);
        row++;
        put_role(ROLE_BODY, "- Search early rooms for ", row, col);
        put_role(ROLE_GOOD, "armour, torches, bow and arrows", row, col + 25);
        put_role(ROLE_BODY, ".", row, col + 56);
        row += 2;

        put_role(ROLE_SECTION, "DEPTH & ESCAPE", row, col); row++;
        put_role(ROLE_BODY, "- Angband drags you down: your ", row, col);
        put_role(ROLE_TERM, "Minimum Depth", row, col + 31);
        put_role(ROLE_BODY, " rises as time passes.", row, col + 44);
        row++;
        put_role(ROLE_BODY, "- You cannot climb above it unless bearing a ", row, col);
        put_role(ROLE_TERM, "Silmaril", row, col + 45);
        put_role(ROLE_BODY, ".", row, col + 53);
        row++;
        put_role(ROLE_BODY, "- Every lvl is generated anew-don't be afraid to climb back upstairs if stuck.", row, col);
        row += 2;

        put_role(ROLE_SECTION, "ELEMENTS", row, col); row++;
        /* Fire */
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_ELEM_FIRE, "Fire", row, col + 2);
        put_role(ROLE_BODY, ": common; ", row, col + 6);
        put_role(ROLE_ELEM_FIRE, "burns and ruins", row, col + 16);
        put_role(ROLE_BODY, " many wares; treat ", row, col + 31);
        put_role(ROLE_ELEM_FIRE, "flame of Udun", row, col + 50);
        put_role(ROLE_BODY, " with respect.", row, col + 63);
        row++;
        /* Cold */
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_ELEM_COLD, "Cold", row, col + 2);
        put_role(ROLE_BODY, ": rarer; can ", row, col + 6);
        put_role(ROLE_ELEM_COLD, "destroy potions/oil", row, col + 19);
        put_role(ROLE_BODY, "; ", row, col + 38);
        put_role(ROLE_GOOD, "warmth is life", row, col + 40);
        put_role(ROLE_BODY, ".", row, col + 54);
        row++;
        /* Poison */
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_ELEM_POISON, "Poison", row, col + 2);
        put_role(ROLE_BODY, ": builds a ", row, col + 8);
        put_role(ROLE_WARN, "counter", row, col + 19);
        put_role(ROLE_BODY, " and ", row, col + 26);
        put_role(ROLE_BAD, "bleeds you over time", row, col + 31);
        put_role(ROLE_BODY, "-", row, col + 51);
        put_role(ROLE_GOOD, "cleanse", row, col + 52);
        put_role(ROLE_BODY, " or wait it out.", row, col + 59);
        row++;
        /* Darkness */
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_ELEM_DARKNESS, "Darkness", row, col + 2);
        put_role(ROLE_BODY, ": only bright ", row, col + 10);
        put_role(ROLE_ELEM_LIGHT, "light", row, col + 24);
        put_role(ROLE_BODY, " truly resists it; carry your own dawn.", row, col + 30);
        row++;

        put_role(ROLE_BODY, "- Mixed elemental attacks will roll extra dice when you lack resistance.", row, col);
        row += 2;

        put_role(ROLE_SECTION, "STATUS & MORALE", row, col); row++;
        put_role(ROLE_BODY, "- Foes are Asleep, Unwary, Alert; your noise sets the stage.", row, col);
        row++;
        put_role(ROLE_BODY, "- Stealth turns (S) and waiting are potent for slipping past sentries.", row, col);
        row++;
        put_role(ROLE_BODY, "- Foes can be Aggressive, Confident, Fleeing and run if their morale breaks.", row, col);
        break;
    }

    case 3:
    {
        /* SIL-MORE: HELP [3/8]: COMBAT & DEFENCE */
        row = 0; col = 1;
        sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: COMBAT & DEFENCE", i, HELP_TOTAL_PAGES);
        put_role(ROLE_HEADER, page_header, row, col);
        row += 2;

        put_role(ROLE_SECTION, "COMBAT BASICS", row, col); row++;
        put_role(ROLE_BODY, "- Two opposed rolls decide hits: your Melee vs their ", row, col);
        put_role(ROLE_TERM, "Evasion", row, col + 53);
        put_role(ROLE_BODY, " (and vice versa).", row, col + 60);
        row++;
        put_role(ROLE_BODY, "- On a hit, your Damage meets their ", row, col);
        put_role(ROLE_TERM, "Protection", row, col + 37);
        put_role(ROLE_BODY, "; only the excess gets through.", row, col + 47);
        row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_TERM, "Evasion", row, col + 2);
        put_role(ROLE_BODY, " avoids getting hit; ", row, col + 9);
        put_role(ROLE_TERM, "Armour", row, col + 30);
        put_role(ROLE_BODY, " [", row, col + 36);
        put_role(ROLE_TERM, "Protection", row, col + 38);
        put_role(ROLE_BODY, "] soaks what lands.", row, col + 48);
        row++;
        put_role(ROLE_BODY, "- Being ", row, col);
        put_role(ROLE_BAD, "surrounded", row, col + 8);
        put_role(ROLE_BODY, " crushes your evasion-fight in ", row, col + 18);
        put_role(ROLE_GOOD, "doorways and angles", row, col + 50);
        put_role(ROLE_BODY, ".", row, col + 69);
        row++;
        put_role(ROLE_BODY, "- Firing a bow in melee invites ", row, col);
        put_role(ROLE_BAD, "free strikes", row, col + 33);
        put_role(ROLE_BODY, "; make space before shooting.", row, col + 45);
        row++;
        put_role(ROLE_BODY, "- Great successes can trigger ", row, col);
        put_role(ROLE_GOOD, "criticals", row, col + 30);
        put_role(ROLE_BODY, " for extra hurt.", row, col + 39);
        row += 2;

        put_role(ROLE_SECTION, "NUMBERS AT A GLANCE", row, col); row++;
        put_role(ROLE_BODY, "- Weapons show (attack, damage). ", row, col);
        put_role(ROLE_TERM, "Armour", row, col + 33);
        put_role(ROLE_BODY, " shows [evasion, protection].", row, col + 39);
        row++;
        put_role(ROLE_BODY, "- Ex: You (Str 3, Melee 16, Longsword 2d5, 2.0 lb) vs Orc [+4, 2d4]:", row, col);
        row += 2;
        put_role(ROLE_BODY, "  @ ", row, col);
        put_role(ROLE_GOOD, "(+16) 34", row, col + 4);
        put_role(ROLE_BAD, " 20", row, col + 12);
        put_role(ROLE_BODY, "  14 [+4] o", row, col + 15);
        put_role(ROLE_BODY, "  ->  ", row, col + 26);
        put_role(ROLE_GOOD, "(4d7) 19", row, col + 32);
        put_role(ROLE_BODY, " - ", row, col + 40);
        put_role(ROLE_WARN, "3", row, col + 43);
        put_role(ROLE_BODY, " = ", row, col + 44);
        put_role(ROLE_BAD, "16", row, col + 47);
        row ++;
        put_role(ROLE_BODY, "  Attack:  ", row, col);
        put_role(ROLE_GOOD, "1d20+16=34", row, col + 11);
        put_role(ROLE_BODY, " vs ", row, col + 21);
        put_role(ROLE_BODY, "1d20+4=14", row, col + 25);
        put_role(ROLE_BODY, "  ->  margin ", row, col + 34);
        put_role(ROLE_BAD, "20", row, col + 47);
        put_role(ROLE_BODY, " = ", row, col + 49);
        put_role(ROLE_BAD, "double crit", row, col + 52);
        put_role(ROLE_BODY, "!", row, col + 63);
        row++;
        put_role(ROLE_BODY, "  Damage:  2d5 +2 (Str 3 capped by 2 lb = 2 more sides) = 2d7,", row, col);
        row++;
        put_role(ROLE_BODY, "           +2d7 (1d per 7+weight in margin) = ", row, col);
        put_role(ROLE_GOOD, "4d7=19", row, col + 46);
        put_role(ROLE_BODY, " - ", row, col + 52);
        put_role(ROLE_WARN, "2d4=3", row, col + 55);
        put_role(ROLE_BODY, " = ", row, col + 60);
        put_role(ROLE_BAD, "16 dmg", row, col + 63);
        put_role(ROLE_BODY, "!", row, col + 69);
        row += 2;

        put_role(ROLE_SECTION, "EVASION VS ARMOUR", row, col); row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_TERM, "Evasion", row, col + 2);
        put_role(ROLE_BODY, " helps you not be hit at all; it's reduced if surrounded.", row, col + 9);
        row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_TERM, "Armour", row, col + 2);
        put_role(ROLE_BODY, " reduces damage after a hit; more protection, less pain.", row, col + 8);
        row++;
        put_role(ROLE_BODY, "- Build around one or balance both; edges matter in tight fights.", row, col);
        break;
    }

    case 4:
    {
        /* SIL-MORE: HELP [4/8]: EARLY TIPS */
        row = 0; col = 1;
        sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: EARLY TIPS", i, HELP_TOTAL_PAGES);
        put_role(ROLE_HEADER, page_header, row, col);
        row += 2;

        put_role(ROLE_SECTION, "CRAFT & GEAR", row, col); row++;
        put_role(ROLE_BODY, "- Guaranteed ", row, col);
        put_role(ROLE_GOOD, "forges", row, col + 13);
        put_role(ROLE_BODY, " at 100', 300', and 500'-plan your craft route.", row, col + 19);
        row++;
        put_role(ROLE_BODY, "- Find armour and a bow first; control fights before you win them.", row, col);
        row += 2;

        put_role(ROLE_SECTION, "TACTICS", row, col); row++;
        put_role(ROLE_BODY, "- Do not rush: this is tactical. Lure, isolate, and retreat often.", row, col);
        row++;
        put_role(ROLE_BODY, "- Doors and corners are force multipliers-avoid being surrounded.", row, col);
        row++;
        put_role(ROLE_BODY, "- Stairs are traffic-reset, ambush, or move on; do not loiter.", row, col);
        row += 2;

        put_role(ROLE_SECTION, "ABILITIES", row, col); row++;
        put_role(ROLE_BODY, "- Abilities matter: a single pick can flip a matchup.", row, col);
        row++;
        put_role(ROLE_BODY, "- Choose abilities that reinforce your plan: stealth, control, or brute force.", row, col);
        row += 2;

        put_role(ROLE_SECTION, "TONE & APPROACH", row, col); row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_TERM, "Cunning", row, col + 2);
        put_role(ROLE_BODY, " over ", row, col + 9);
        put_role(ROLE_BAD, "cruelty", row, col + 15);
        put_role(ROLE_BODY, "; ", row, col + 22);
        put_role(ROLE_SECTION, "light", row, col + 24);
        put_role(ROLE_BODY, " over ", row, col + 29);
        put_role(ROLE_SUBTLE, "darkness", row, col + 35);
        put_role(ROLE_BODY, "; ", row, col + 43);
        put_role(ROLE_GOOD, "retreat is wisdom", row, col + 45);
        put_role(ROLE_BODY, ".", row, col + 62);
        row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_GOOD, "Doors, distance, and silence", row, col + 2);
        put_role(ROLE_BODY, " are ", row, col + 30);
        put_role(ROLE_TERM, "weapons", row, col + 35);
        put_role(ROLE_BODY, ".", row, col + 42);
        row++;
        put_role(ROLE_BODY, "- Your ", row, col);
        put_role(ROLE_TERM, "legend", row, col + 7);
        put_role(ROLE_BODY, " is a ", row, col + 13);
        put_role(ROLE_TERM, "mosaic of choices", row, col + 19);
        put_role(ROLE_BODY, "-small ", row, col + 36);
        put_role(ROLE_GOOD, "edges", row, col + 43);
        put_role(ROLE_BODY, " add up.", row, col + 48);
        row++;
        put_role(ROLE_BODY, "- You are the light you carry. Choose your fights; write your legend.", row, col);
        break;
    }

    case 5:
    {
        /* SIL-MORE: HELP [5/8]: MOVEMENT & MISCELLANEOUS */
        sprintf(page_header, "HELP [%d/%d]: MOVEMENT & MISCELLANEOUS", i, HELP_TOTAL_PAGES);
        put_role(ROLE_HEADER, page_header, 0, 1);

        row = 3; col = 3; col2 = col + 8;
        put_role(ROLE_SECTION, "Movement etc", row - 2, col - 1);

        put_role(ROLE_KEY,   "7 8 9", row, col);
        put_role(ROLE_SUBTLE," \\|/ ", row + 1, col);
        put_role(ROLE_KEY,   "4 5 6", row + 2, col);
        put_role(ROLE_SUBTLE,"-", row + 2, col + 1);
        put_role(ROLE_SUBTLE,"-", row + 2, col + 3);
        put_role(ROLE_SUBTLE," /|\\ ", row + 3, col);
        put_role(ROLE_KEY,   "1 2 3", row + 4, col);

        put_role(ROLE_SUBTLE, "Use the numbers or arrow keys", row + 0, col2);
        put_role(ROLE_KEY,    "numbers", row + 0, col2 + 8);
        put_role(ROLE_KEY,    "arrow keys", row + 0, col2 + 19);
        put_role(ROLE_SUBTLE, "to move, attack, or open doors", row + 1, col2);
        put_role(ROLE_SUBTLE, "(You may need numlock)", row + 2, col2);
        put_role(ROLE_KEY,    "numlock", row + 2, col2 + 14);
        put_role(ROLE_SUBTLE, "Use 5 or z to wait a turn (& search)", row + 4, col2);
        put_role(ROLE_KEY,    "5", row + 4, col2 + 4);
        put_role(ROLE_KEY,    "z", row + 4, col2 + 9);

        row += 6;
        put_role(ROLE_SUBTLE, "Use shift or . to move continuously", row, col);
        put_role(ROLE_KEY,    "shift", row, col + 4);
        put_role(ROLE_KEY,    ".", row, col + 13);
        row++;
        put_role(ROLE_SUBTLE, "- direction 5 or z rests until healed", row, col + 2);
        row += 2;

        put_role(ROLE_SUBTLE, "Use control or / to interact with a square:", row, col);
        put_role(ROLE_KEY,    "control", row, col + 4);
        if (angband_keyset) put_role(ROLE_KEY, "+", row, col + 15); else put_role(ROLE_KEY, "/", row, col + 15);
        row++;
        put_role(ROLE_SUBTLE, "- tunnels through rubble/walls", row, col + 2); row++;
        put_role(ROLE_SUBTLE, "- closes open doors", row, col + 2);           row++;
        put_role(ROLE_SUBTLE, "- bashes closed doors", row, col + 2);          row++;
        put_role(ROLE_SUBTLE, "- disarms floor traps", row, col + 2);          row++;
        put_role(ROLE_SUBTLE, "- disarms/opens chests and searches skeletons", row, col + 2); row++;
        put_role(ROLE_SUBTLE, "- attacks monsters without moving", row, col + 2); row += 2;
        put_role(ROLE_SUBTLE, "Interacting with your own square also:", row, col); row++;
        put_role(ROLE_SUBTLE, "- picks up an item", row, col + 2); row++;
        put_role(ROLE_SUBTLE, "- uses a staircase/forge", row, col + 2); row++;
        put_role(ROLE_SUBTLE, "- can be done by pressing , or Space", row, col + 2);
        put_role(ROLE_KEY,    ",", row, col + 28);
        put_role(ROLE_KEY,    "Space", row, col + 33);

        row = 3; col = 52;
        put_role(ROLE_SECTION, "Miscellaneous", row - 2, col);

        put_role(ROLE_KEY,  "f F", row, col - 1); put_role(ROLE_SUBTLE, "/", row, col); put_role(ROLE_SUBTLE, "fire from quiver 1/2", row, col + 3); row++;
        if (angband_keyset) put_role(ROLE_KEY, " a", row, col); else put_role(ROLE_KEY, " s", row, col); put_role(ROLE_SUBTLE, "sing", row, col + 3); row++;
        put_role(ROLE_KEY, " S", row, col); put_role(ROLE_SUBTLE, "stealth mode", row, col + 3); row++;
        put_role(ROLE_KEY, " n", row, col); put_role(ROLE_SUBTLE, "repeat last command", row, col + 3); row++;
        if (angband_keyset) put_role(ROLE_KEY, " 0", row, col); else put_role(ROLE_KEY, " R", row, col); put_role(ROLE_SUBTLE, "repeat next command", row, col + 3); row += 2;
        put_role(ROLE_KEY, " l", row, col); put_role(ROLE_SUBTLE, "look (at things)", row, col + 3); row++;
        put_role(ROLE_KEY, " L", row, col); put_role(ROLE_SUBTLE, "look (around dungeon)", row, col + 3); row++;
        put_role(ROLE_KEY, " M", row, col); put_role(ROLE_SUBTLE, "display map of level", row, col + 3); row += 2;
        put_role(ROLE_KEY, " m", row, col); put_role(ROLE_UI,  "main menu", row, col + 3); row++;
        put_role(ROLE_KEY, "Tab", row, col - 1); put_role(ROLE_UI,  "display ability screen", row, col + 3); row++;
        if (angband_keyset) put_role(ROLE_KEY, " C", row, col); else put_role(ROLE_KEY, " @/h", row, col-2); put_role(ROLE_UI, "display character sheet", row, col + 3); row++;
        if (angband_keyset) put_role(ROLE_KEY, " =", row, col); else put_role(ROLE_KEY, " O", row, col); put_role(ROLE_UI, "set options", row, col + 3); row += 2;
        put_role(ROLE_KEY, "^s", row, col); put_role(ROLE_UI,  "save", row, col + 3); row++;
        put_role(ROLE_KEY, "^x", row, col); put_role(ROLE_UI,  "save and quit", row, col + 3); row++;
        // put_role(ROLE_KEY, " Q", row, col); put_role(ROLE_UI,  "abort current game", row, col + 3);
        break;
    }

    case 6:
    {
        /* SIL-MORE: HELP [6/8]: TERRAIN & ITEMS */
        sprintf(page_header, "HELP [%d/%d]: TERRAIN & ITEMS", i, HELP_TOTAL_PAGES);
        put_role(ROLE_HEADER, page_header, 0, 1);

        row = 3; col = 3;
        put_role(ROLE_SECTION, "Terrain ", row - 2, col - 1);

        /* Keep gameplay glyph colours as-is; only change the labels to ROLE_BODY */
        if (hybrid_walls) { c_put_str(TERM_L_WHITE + (MAX_COLORS * BG_DARK), "#", row, col); }
        else if (solid_walls) { c_put_str(TERM_L_WHITE + (MAX_COLORS * BG_SAME), "#", row, col); }
        else { c_put_str(TERM_L_WHITE, "#", row, col); }
        put_role(ROLE_BODY, "wall", row, col + 2); row++;
        c_put_str(TERM_WHITE + (MAX_COLORS * BG_SAME), "%", row, col); put_role(ROLE_BODY, "quartz vein", row, col + 2); row++;
        c_put_str(TERM_SLATE, ":", row, col); put_role(ROLE_BODY, "rubble", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "+", row, col); put_role(ROLE_BODY, "closed door", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "'", row, col); put_role(ROLE_BODY, "open door", row, col + 2); row++;
        c_put_str(TERM_L_GREEN, "+", row, col); c_put_str(TERM_L_BLUE, "+", row, col + 1); c_put_str(TERM_VIOLET, "+", row, col + 2); put_role(ROLE_BODY, "warded doors", row, col + 4); row++;
        c_put_str(TERM_L_WHITE, ">", row, col); put_role(ROLE_BODY, "staircase down", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, "<", row, col); put_role(ROLE_BODY, "staircase up", row, col + 2); row++;
        c_put_str(TERM_SLATE, "0", row, col); put_role(ROLE_BODY, "forge", row, col + 2); row++;
        c_put_str(TERM_YELLOW, "^", row, col); put_role(ROLE_BODY, "trap", row, col + 2); row++;
        c_put_str(TERM_L_GREEN, ";", row, col); put_role(ROLE_BODY, "warding glyph", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, ".", row, col); put_role(ROLE_BODY, "empty floor", row, col + 2); row++;

        row = 3; col = 27;
        put_role(ROLE_SECTION, "Items", row - 2, col - 1);
        c_put_str(TERM_L_WHITE, "| ", row, col); put_role(ROLE_BODY, "blades", row, col + 2); row++;
        c_put_str(TERM_SLATE, "/ ", row, col); put_role(ROLE_BODY, "axes & polearms", row, col + 2); row++;
        c_put_str(TERM_UMBER, "\\ ", row, col); put_role(ROLE_BODY, "blunt weapons", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "( ", row, col); put_role(ROLE_BODY, "soft armour", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, "[ ", row, col); put_role(ROLE_BODY, "mail", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, ") ", row, col); put_role(ROLE_BODY, "shields", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, "] ", row, col); put_role(ROLE_BODY, "misc armour", row, col + 2); row++;
        c_put_str(TERM_RED, "= ", row, col); put_role(ROLE_BODY, "rings", row, col + 2); row++;
        c_put_str(TERM_ORANGE, "\" ", row, col); put_role(ROLE_BODY, "amulets", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "~ ", row, col); put_role(ROLE_BODY, "light sources", row, col + 2); row++;
        c_put_str(TERM_UMBER, "} ", row, col); put_role(ROLE_BODY, "bows", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "- ", row, col); put_role(ROLE_BODY, "arrows", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, ", ", row, col); put_role(ROLE_BODY, "food", row, col + 2); row++;
        c_put_str(TERM_L_BLUE, "! ", row, col); put_role(ROLE_BODY, "potions", row, col + 2); row++;
        c_put_str(TERM_UMBER, "_ ", row, col); put_role(ROLE_BODY, "staves", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "? ", row, col); put_role(ROLE_BODY, "instruments", row, col + 2); row++;
        c_put_str(TERM_YELLOW, "! ", row, col); put_role(ROLE_BODY, "flasks of oil", row, col + 2); row++;

        row = 3; col = 52;
        put_role(ROLE_SECTION, "Item Commands", row - 2, col - 1);
        if (angband_keyset) put_role(ROLE_KEY, "U", row, col); else put_role(ROLE_KEY, "u", row, col); put_role(ROLE_UI, "use", row, col + 2); row++;
        put_role(ROLE_KEY, "d", row, col); put_role(ROLE_UI, "drop", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "I", row, col); else put_role(ROLE_KEY, "x", row, col); put_role(ROLE_UI, "examine", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "v", row, col); else put_role(ROLE_KEY, "t", row, col); put_role(ROLE_UI, "throw", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "^v", row, col - 1); else put_role(ROLE_KEY, "^t", row, col - 1); put_role(ROLE_UI, "throw (auto-target)", row, col + 2); row++;
        put_role(ROLE_KEY, "k", row, col); put_role(ROLE_UI, "destroy", row, col + 2); row++;
        put_role(ROLE_KEY, "{", row, col); put_role(ROLE_UI, "inscribe", row, col + 2); row++;
        break;
    }

    case 7:
    {
        /* SIL-MORE: HELP [7/8]: ADVANCED COMMANDS */
        sprintf(page_header, "HELP [%d/%d]: ADVANCED COMMANDS", i, HELP_TOTAL_PAGES);
        put_role(ROLE_HEADER, page_header, 0, 1);

        row = 3; col = 3;
        put_role(ROLE_SECTION, "Superfluous", row - 2, col - 1);

        put_role(ROLE_KEY, "i", row, col); put_role(ROLE_UI,  "display inventory", row, col + 2); row++;
        put_role(ROLE_KEY, "e", row, col); put_role(ROLE_UI,  "display equipped items", row, col + 2); row += 2;
        put_role(ROLE_KEY, "g", row, col); put_role(ROLE_UI,  "get", row, col + 2); row++;
        put_role(ROLE_KEY, "w", row, col); put_role(ROLE_UI,  "wear/wield", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "t", row, col); else put_role(ROLE_KEY, "r", row, col); put_role(ROLE_UI,  "remove", row, col + 2); row++;
        put_role(ROLE_KEY, "E", row, col); put_role(ROLE_UI,  "eat food", row, col + 2); row++;
        put_role(ROLE_KEY, "q", row, col); put_role(ROLE_UI,  "quaff potion", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "u", row, col); else put_role(ROLE_KEY, "a", row, col); put_role(ROLE_UI,  "activate staff", row, col + 2); row++;
        put_role(ROLE_KEY, "p", row, col); put_role(ROLE_UI,  "play instrument", row, col + 2); row += 2;

        put_role(ROLE_KEY, "o", row, col); put_role(ROLE_UI,  "open door/chest", row, col + 2); row++;
        put_role(ROLE_KEY, "c", row, col); put_role(ROLE_UI,  "close door", row, col + 2); row++;
        put_role(ROLE_KEY, "b", row, col); put_role(ROLE_UI,  "bash door", row, col + 2); row++;
        put_role(ROLE_KEY, "D", row, col); put_role(ROLE_UI,  "disarm trap", row, col + 2); row++;
        put_role(ROLE_KEY, "T", row, col); put_role(ROLE_UI,  "tunnel", row, col + 2); row++;
        put_role(ROLE_KEY, ">", row, col); put_role(ROLE_UI,  "descend stairs", row, col + 2); row++;
        put_role(ROLE_KEY, "<", row, col); put_role(ROLE_UI,  "ascend stairs", row, col + 2); row++;
        put_role(ROLE_KEY, "0", row, col); put_role(ROLE_UI,  "forge an item", row, col + 2); row++;

        row = 3; col = 34;
        put_role(ROLE_SECTION, "Advanced", row - 2, col);

        put_role(ROLE_KEY,  " :", row, col); put_role(ROLE_UI,   "write a note", row, col + 3); row++;
        put_role(ROLE_KEY,  " )", row, col); put_role(ROLE_UI,   "save screen shot", row, col + 3); row += 2;
        if (angband_keyset) put_role(ROLE_KEY, " @", row, col); else put_role(ROLE_KEY, " $", row, col); put_role(ROLE_UI,   "set macros", row, col + 3); row++;
        put_role(ROLE_KEY,  " &", row, col); put_role(ROLE_UI,   "set colours", row, col + 3); row += 2;
        put_role(ROLE_KEY,  "^p", row, col); put_role(ROLE_UI,   "display prior messages", row, col + 3); row++;
        put_role(ROLE_KEY,  "^r", row, col); put_role(ROLE_UI,   "redraw screen", row, col + 3); row++;
        put_role(ROLE_KEY,  "^e", row, col); put_role(ROLE_UI,   "switch inven/equip display in windows", row, col + 3); row++;
        put_role(ROLE_KEY,  " V", row, col); put_role(ROLE_UI,   "version information", row, col + 3); row++;

        row = 16; col = 35; col2 = 43;
        put_role(ROLE_SECTION, "hjkl movement", row - 2, col - 1);

        put_role(ROLE_KEY,   "y k u", row, col);
        put_role(ROLE_SUBTLE," \\|/ ", row + 1, col);
        put_role(ROLE_KEY,   "h z l", row + 2, col);
        put_role(ROLE_SUBTLE,"-", row + 2, col + 1);
        put_role(ROLE_SUBTLE,"-", row + 2, col + 3);
        put_role(ROLE_SUBTLE," /|\\ ", row + 3, col);
        put_role(ROLE_KEY,   "b j n", row + 4, col);

        put_role(ROLE_SUBTLE, "If the hjkl movement option is on", row, col2);
        put_role(ROLE_SUBTLE, "then these keys move you around", row + 1, col2);
        put_role(ROLE_SUBTLE, "Use shift to 'run'", row + 3, col2); put_role(ROLE_KEY, "shift", row + 3, col2 + 4);
        put_role(ROLE_SUBTLE, "Use control for the underlying", row + 4, col2); put_role(ROLE_KEY, "control", row + 4, col2 + 4);
        put_role(ROLE_SUBTLE, "key-commands", row + 5, col2);
        break;
    }

    case 8:
    {
        /* SIL-MORE: HELP [8/8]: STEAM DECK CONTROLS */
        row = 0; col = 1;
        sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: STEAM DECK CONTROLS", i, HELP_TOTAL_PAGES);
        put_role(ROLE_HEADER, page_header, row, col);
        row += 2;

        /* Movement and Action Controls */
        col = 1;
        put_role(ROLE_SECTION, "MOVEMENT & ACTION", row, col); row += 2;
        put_role(ROLE_KEY, "D-pad / Left Stick", row, col);
        put_role(ROLE_BODY, " - Movement", row, col + 22); row++;

        char action_buf[96];
        int binding = 0;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_SOUTH);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        put_role(ROLE_KEY, "A", row, col); put_role(ROLE_BODY, " - ", row, col + 2);
        put_role(ROLE_BODY, action_buf, row, col + 5); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_WEST);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        put_role(ROLE_KEY, "X", row, col); put_role(ROLE_BODY, " - ", row, col + 2);
        put_role(ROLE_BODY, action_buf, row, col + 5); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_NORTH);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        put_role(ROLE_KEY, "Y", row, col); put_role(ROLE_BODY, " - ", row, col + 2);
        put_role(ROLE_BODY, action_buf, row, col + 5); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_EAST);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        put_role(ROLE_KEY, "B", row, col); put_role(ROLE_BODY, " - ", row, col + 2);
        put_role(ROLE_BODY, action_buf, row, col + 5); row++;

        {
            char rs_up[24];
            char rs_down[24];
            char rs_left[24];
            char rs_right[24];
            char rs_line[120];
            help_binding_action_short(get_sdl_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_UP), rs_up, sizeof(rs_up));
            help_binding_action_short(get_sdl_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_DOWN), rs_down, sizeof(rs_down));
            help_binding_action_short(get_sdl_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_LEFT), rs_left, sizeof(rs_left));
            help_binding_action_short(get_sdl_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_RIGHT), rs_right, sizeof(rs_right));
            strnfmt(rs_line, sizeof(rs_line), "Up:%s  Down:%s  Left:%s  Right:%s",
                    rs_up, rs_down, rs_left, rs_right);
            put_role(ROLE_KEY, "Right Stick", row, col);
            put_role(ROLE_BODY, " - ", row, col + 11);
            put_role(ROLE_BODY, rs_line, row, col + 14);
            row++;
        }

        row += 1;

        /* Left and right side controls */
        int left_header_row = row;
        int left_start_row = row + 2;
        put_role(ROLE_SECTION, "LEFT SIDE CONTROLS", left_header_row, col);

        row = left_start_row;
        const char* input = NULL;
        int text_col = 0;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L1 (Bumper)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_trigger_binding(0);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L2 (Trigger)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L4 (Back)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L5 (Back)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_shoulder_combo_binding();
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L1+R1 Combo";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        int left_end_row = row;

        col = 42;
        row = left_header_row;
        put_role(ROLE_SECTION, "RIGHT SIDE CONTROLS", row, col);
        row = left_start_row;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "R1 (Bumper)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_trigger_binding(1);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "R2 (Trigger)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "R4 (Back)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "R5 (Back)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_START);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "Start (Menu)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_BACK);
        help_binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "Back (View)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        int right_end_row = row;

        row = (left_end_row > right_end_row) ? left_end_row : right_end_row;
        row += 1;
        {
            char shift_label[16];
            char sing_label[16];
            char fire_label[16];
            char note_buf[120];
            help_prompt_label(GAMEPAD_BIND_SHIFT, "L2", shift_label, sizeof(shift_label));
            help_prompt_label('s', "Y", sing_label, sizeof(sing_label));
            help_prompt_label('f', "B", fire_label, sizeof(fire_label));
            strnfmt(note_buf, sizeof(note_buf),
                    "Shift: %s+%s=Stealth, %s+%s=Second quiver",
                    shift_label, sing_label, shift_label, fire_label);
            put_role(ROLE_SUBTLE, note_buf, row, 1);
        }

        row += 1;
        put_role(ROLE_SUBTLE, "Customize bindings via Options -> Controller Settings.", row, 1);
        
        break;
    }
    }
}



/*
 * Peruse the On-Line-Help
 */
void do_cmd_help(void)
{
    int i = 1;
    char ch;

    /* Clear any active banner before opening help */
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

        show_help_screen(i);
        int wid, hgt;

        // get current terminal size
        Term_get_size(&wid, &hgt);
        /* Better navigation prompt */
        {
            char nav[128];
            if (steamdeck_controls_active()) {
                char next_label[16];
                char back_label[16];
                help_prompt_label(' ', "A", next_label, sizeof(next_label));
                help_prompt_label('b', "b", back_label, sizeof(back_label));
                strnfmt(nav, sizeof(nav),
                    "Navigation: D-pad left/right Prev/Next  [%s] Next  [%s] Back",
                    next_label, back_label);
            } else {
                strnfmt(nav, sizeof(nav),
                    "Navigation: [<-/4] Prev  [->/6/Space] Next  [X+1-%d] Page  [Q/Esc] Quit",
                    HELP_TOTAL_PAGES);
            }
            c_put_str(TERM_WHITE, nav, hgt - 1, 1);
        }
        ch = inkey();
        if (steamdeck_controls_active() && ch == 'b')
            ch = ESCAPE;

        /* Enhanced navigation */
        if (ch != EOF)
        {
            /* Quit commands */
            if ((ch == 'q') || (ch == 'Q') || (ch == ESCAPE))
            {
                break;
            }
            /* Previous page */
            else if ((ch == '8') || (ch == '-') || (ch == '4'))
            {
                i--;
                if (i < 1)
                    i = 1;
            }
            /* Next page */
            else if ((ch == '2') || (ch == '6') || (ch == ' ') || (ch == '\r') || (ch == '\n'))
            {
                i++;
            }
            /* Direct page navigation with 'x' prefix */
            else if (ch == 'x' || ch == 'X')
            {
                /* Wait for second key */
                char prompt[32];
                sprintf(prompt, "Page (1-%d): ", HELP_TOTAL_PAGES);
                c_put_str(TERM_YELLOW, prompt, 23, 60);
                char ch2 = inkey();
                if ((ch2 >= '1') && (ch2 <= ('0' + HELP_TOTAL_PAGES)))
                {
                    int target = ch2 - '0';
                    if (target <= HELP_TOTAL_PAGES)
                        i = target;
                }
                /* Clear the prompt */
                c_put_str(TERM_L_WHITE, "                ", 23, 60);
            }
            /* Default: next page */
            else
            {
                i++;
            }
        }

        /* Done */
        if (i > HELP_TOTAL_PAGES)
            break;

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();
}

/*
 * Process the player name and extract a clean "base name".
 *
 * If "sf" is true, then we initialize "savefile" based on player name.
 *
 * Some platforms (Windows) leave the "savefile" empty when a new 
 * character is created, and then when the character is done being 
 * created, they call this function to choose a new savefile name.
 */
void process_player_name(bool sf)
{
    int i;

    /* Process the player name */
    for (i = 0; op_ptr->full_name[i]; i++)
    {
        char c = op_ptr->full_name[i];

        /* No control characters */
        if (iscntrl((unsigned char)c))
        {
            /* Illegal characters */
            quit(format("Illegal control char (0x%02X) in player name", c));
        }

        /* Convert illegal file system characters but preserve some readability */
        if (iscntrl((unsigned char)c) || c == '/' || c == '\\' || c == ':' || 
            c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
        {
            /* Convert illegal characters to underscore */
            c = '_';
        }
        else if (c == ' ')
        {
            /* Convert spaces to underscores for file system compatibility */
            c = '_';
        }
        /* Keep all other characters (letters, digits, punctuation) */

        /* Build "base_name" */
        op_ptr->base_name[i] = c;
    }

    /* Terminate */
    op_ptr->base_name[i] = '\0';

    /* Require a "base" name */
    if (!op_ptr->base_name[0])
    {
        log_debug("No base name provided, using 'nameless'");
        SDL_strlcpy(op_ptr->base_name, "nameless", sizeof(op_ptr->base_name));
    }

    /* Pick savefile name if needed */
    if (sf)
    {
        char temp[128];

        /* Rename the savefile, using the base name */
        strnfmt(temp, sizeof(temp), "%s", op_ptr->base_name);

        /* Build the filename */
        path_build(savefile, sizeof(savefile), ANGBAND_DIR_SAVE, temp);
        log_info("Generated savefile path: %s", savefile);
    }
}

/*
 * Gets a name for the character, reacting to name changes.
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
    Term_putstr(
        QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE, "Enter accept name");
    Term_putstr(
        QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE, "  Tab random name");

    /* Hack - highlight the key names */
    Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "Enter");
    Term_putstr(QUESTION_COL + 2, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "Tab");

    /* Special Prompt? */
    if (character_dungeon)
    {
        Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_SLATE,
            "ESC abort name change                  ");

        /* Hack - highlight the key names */
        Term_putstr(
            QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "ESC");
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
void do_cmd_escape(int silmarils)
{
    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[120];

    /* set the escaped flag */
    p_ptr->escaped = true;

    /* Flush input */
    flush();

    /* Commit suicide */
     p_ptr->is_dead = true;

    /* Stop playing */
    p_ptr->playing = false;

    /* Leaving */
    p_ptr->leaving = true;

    /* Get time */
    (void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));

    /* Add note */
    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    /*killed by */
    sprintf(buf, "You escaped the Iron Hells on %s.", long_day);

    /* Write message */
    do_cmd_note(buf, p_ptr->depth);

    // make a note
    switch (silmarils)
    {
    case 0:
        do_cmd_note("You returned empty handed.", p_ptr->depth);
        break;
    case 1:
        do_cmd_note(
            "You brought back a Silmaril from Morgoth's crown!", p_ptr->depth);
        break;
    case 2:
        do_cmd_note("You brought back two Silmarils from Morgoth's crown!",
            p_ptr->depth);
        break;
    case 3:
        do_cmd_note(
            "You brought back all three Silmarils from Morgoth's crown!",
            p_ptr->depth);
        break;
    default:
        do_cmd_note("You brought back so many Silmarils that people should be "
                    "suspicious!",
            p_ptr->depth);
        break;
    }

    if (p_ptr->oath_type > 0)
    {
        if (oath_invalid(p_ptr->oath_type))
        {
            /* Use oath-specific death/escape message */
            char* death_msg = oath_death_message(p_ptr->oath_type);
            if (death_msg && death_msg[0]) {
                do_cmd_note(death_msg, p_ptr->depth);
            } else {
                /* Fallback to generic message if no specific text found */
                do_cmd_note(
                    "You passed from the world, but the stain of a faithless heart remains. You will be remembered not for your deeds, but as a shameful Oathbreaker.",
                    p_ptr->depth);
            }
        }
        else
        {
            do_cmd_note("You kept your oath to the very end.", p_ptr->depth);
        }
    }

    // (void)inkey();

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    /* Cause of death */
    SDL_strlcpy(p_ptr->died_from, "ripe old age", sizeof(p_ptr->died_from));

    /* Update metarun: escaped with N Silmarils */
    log_info("Player escaped with %d Silmarils", silmarils);
    metarun_update_on_exit(false, true, silmarils, 0);

}

/*
 * Hack -- victory by slaying Morgoth's illusion
 */
void do_cmd_morgoth_victory(void)
{
    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[160];

    /* Ensure the victory flag is set */
    p_ptr->morgoth_slain = true;

    /* Flush input ahead of the scripted sequence */
    flush();

    /* Treat as a completed run */
    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;
    p_ptr->escaped = false;

    /* Mark the calendar moment */
    (void)strftime(long_day, sizeof(long_day), "%d %B %Y", localtime(&ct));

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    strnfmt(buf, sizeof(buf),
            "On %s you broke the illusion binding Morgoth to his throne.",
            long_day);
    do_cmd_note(buf, p_ptr->depth);

    do_cmd_note(
        "The Valar hail your impossible triumph and pour out their blessing.",
        p_ptr->depth);

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    /* Record cause for high scores */
    SDL_strlcpy(p_ptr->died_from, "Morgoth's illusory defeat",
        sizeof(p_ptr->died_from));

    killer_mark_other(SCORE_KILLER_OTHER);
    killer_commit(p_ptr->died_from);
}

/*
 * Hack -- commit suicide
 */
void do_cmd_suicide(void)
{
    char ch;

    /* Flush input */
    flush();

    /* Verify */
    if (!get_check("This will destroy the current character: are you sure? "))
        return;

    /* Special Verification for suicide */
    prt("Please verify ABORTING by typing the '@' sign: ", 0, 0);
    flush();
    ch = inkey();
    prt("", 0, 0);
    if (ch != '@')
        return;

    /* Commit suicide */
    p_ptr->is_dead = true;

    /* Stop playing */
    p_ptr->playing = false;

    /* Leaving */
    p_ptr->leaving = true;

    SDL_strlcpy(p_ptr->died_from, "their own hand", sizeof(p_ptr->died_from));

    killer_mark_other(SCORE_KILLER_SELF);
    killer_commit(p_ptr->died_from);
}

/*
 * Save the game
 */
void do_cmd_save_game(void)
{
    /* Disturb the player */
    disturb(1, 0);

    // in final deployment versions, you cannot save in the tutorial
    if (DEPLOYMENT && p_ptr->game_type != 0)
    {
        if (!save_game_quietly)
        {
            /* Message */
            msg_print("You cannot save games during the tutorial.");
        }
        return;
    }

    /* Clear messages */
    message_flush();

    /* Handle stuff */
    handle_stuff();

    if (!save_game_quietly)
    {
        /* Message */
        prt("Saving game...", 0, 0);
    }

    /* Refresh */
    Term_fresh();

    /* The player is not dead */
    SDL_strlcpy(p_ptr->died_from, "(saved)", sizeof(p_ptr->died_from));

    /* Forbid suspend */
    signals_ignore_tstp();

    /* Save the player */
    /* Make sure meta-run data (curses, flags, etc.) is up-to-date even
      when the player merely saves & quits. */
    log_info("Saving game and updating metarun data");    
   metarun_update_on_exit(false, false, 0, 0);

    if (save_player())
    {
    log_debug("Game saved successfully");
        if (!save_game_quietly)
        {
            prt("Saving game... done.", 0, 0);
        }

        /* Note: upsert_live_score_on_save() is called from close_game() 
         * when quitting, not here. This avoids opening the scores file 
         * multiple times. */

        high_score live_score;
        if (build_live_preview_score(&live_score)) {
            time_t now = time(NULL);
            if (!score_runs_record_current_run(&live_score, now, SCORE_RECORD_ALIVE)) {
                log_warn("Failed to persist live run snapshot for '%s'", op_ptr->full_name);
            }
        }
    }

    /* Save failed (oops) */
    else
    {
    log_error("Game save failed");
        prt("Saving game... failed!", 0, 0);
    }

    /* Allow suspend again */
    signals_handle_tstp();

    /* Refresh */
    Term_fresh();

    /* Note that the player is not dead */
    SDL_strlcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));

    /* Reset the quietly flag */
    save_game_quietly = false;
}

/*
 * Hack - save the time of death
 */
static time_t death_time = (time_t)0;

/*
 * Display a "tomb-stone"
 */
static void print_tomb(high_score* the_score)
{
    if (p_ptr->escaped)
    {
        if (p_ptr->oath_type > 0 && !oath_invalid(p_ptr->oath_type))
            Term_putstr(
                15, 2, -1, TERM_L_BLUE, "You have escaped and kept your oath");
        else
            Term_putstr(15, 2, -1, TERM_L_BLUE, "You have escaped");
    }
    else if (p_ptr->morgoth_slain)
    {
        Term_putstr(15, 2, -1, TERM_YELLOW,
            "You are acclaimed as the Slayer of Morgoth");
    }
    else
    {
        Term_putstr(15, 2, -1, TERM_L_BLUE, "You have been slain");
    }

    /* Show score line */
    display_single_score(TERM_WHITE, 1, 0, 0, false, the_score);

}

/*
 * Display some character info
 */
static void show_info(void)
{
    /* Display player */
    display_player(0);

    /* Prompt for inventory */
    Term_putstr(30, 22, -1, TERM_L_WHITE, "(press any key)");

    /* Allow abort at this point */
    if (inkey() == ESCAPE)
        return;

    /* Show equipment and inventory */

    /* Equipment -- if any */
    if (p_ptr->equip_cnt)
    {
        Term_clear();
        item_tester_full = true;
        show_equip();
        prt("You are using:", 0, 0);
        Term_putstr(30, 16, -1, TERM_L_WHITE, "(press any key)");
        if (inkey() == ESCAPE)
            return;
        item_tester_full = false;
    }

    /* Inventory -- if any */
    if (p_ptr->inven_cnt)
    {
        Term_clear();
        item_tester_full = true;
        show_inven();
        prt("You are carrying:", 0, 0);
        Term_putstr(
            30, p_ptr->inven_cnt + 2, -1, TERM_L_WHITE, "(press any key)");
        if (inkey() == ESCAPE)
            return;
        item_tester_full = false;
    }

    // Display notes
    do_cmd_knowledge_notes();
}


#define highscore_fd (score_file_active_ctx()->fd)
#define scores_file_entry_count (score_file_active_ctx()->entry_count)
#define scores_file_version_major (score_file_active_ctx()->version_major)
#define scores_file_version_minor (score_file_active_ctx()->version_minor)
#define scores_file_version_patch (score_file_active_ctx()->version_patch)
#define scores_file_version_extra (score_file_active_ctx()->version_extra)

/* Forward declaration */
errr create_score(high_score* the_score);

/*
 * Check if the scores file is empty (no entries)
 * Used to determine if this is a first-time player
 * Returns true if file is empty or doesn't exist, false otherwise
 */
extern bool highscore_is_empty()
{
    bool opened_here = false;
    
    /* Open the file on-demand (read-only) */
    if (!highscore_fd) {
        char buf[1024];
        build_meta_path(buf, sizeof(buf), "scores.raw");
        safe_setuid_grab();
        highscore_fd = score_file_open(buf, O_RDONLY);
        safe_setuid_drop();
        if (!highscore_fd) {
            log_debug("highscore_is_empty: cannot open scores file, treating as empty");
            return true; /* File doesn't exist = empty = first time */
        }
        opened_here = true;
    }
    
    /* Check entry count from header */
    bool is_empty = (scores_file_entry_count == 0);
    if (opened_here) { SDL_CloseIO(highscore_fd); highscore_fd = NULL; }
    log_debug("highscore_is_empty: entry_count=%u, returning %s", 
              scores_file_entry_count, is_empty ? "true" : "false");
    return is_empty;
}

/* Removed obsolete duplicated hero_in_scores fragment */

#define RACE_PRIORITIES (sizeof(race_priority) / sizeof(race_priority[0]))

/* ------------------------------------------------------------------ */
/* bit-test whether RACE can belong to CHARACTER                      */
static int race_has_character(uint16_t race, uint16_t character)
{
    if (character >= z_info->c_max) return 0;
    const uint16_t word  = character / 32U;
    const uint16_t shift = character % 32U;
    return (p_info[race].choice[word] & (1U << shift)) != 0U;
}

static int parse_score_id(const char field[3])
{
    if (!field)
        return -1;
    if (!isdigit((unsigned char)field[0]) || !isdigit((unsigned char)field[1]))
        return -1;
    return (field[0] - '0') * 10 + (field[1] - '0');
}

/* ------------------------------------------------------------------ */
/* helper - build a dummy hi-score entry so we can immediately kill it */
static void build_dummy_entry(high_score *e, uint16_t race, uint16_t character)
{
    memset(e, 0, sizeof(*e));

    /* score / gold / turns are all zero so the entry will sort last   */
    strnfmt(e->what, sizeof e->what, "%s",
            "Hero of the First Age");

    /* 15-char player name - character name fits nicely */
    const char *hname = c_name + c_info[character].name;
    strnfmt(e->who,  sizeof e->who,  "%-.15s", hname);

    /* race & character: two digits each, zero-padded                       */
    strnfmt(e->p_r,  sizeof e->p_r,  "%02u", race);
    strnfmt(e->p_h,  sizeof e->p_h,  "%02u", character);

    /* Save the date in standard encoded form */
    time_t now = time(NULL);
    strftime(e->day, sizeof(e->day), "@%Y%m%d",
        localtime(&now));

    /* immediate cause of death - will be overwritten below anyway      */
    strnfmt(e->how,  sizeof e->how, op_ptr->base_name);
}


/*
 * Prints a nice comma spaced natural number
 */
void comma_number(char* output, int number)
{
    if (number >= 1000000)
    {
        sprintf(output, "%d,%03d,%03d", number / 1000000,
            (number % 1000000) / 1000, number % 1000);
    }
    else if (number >= 1000)
    {
        sprintf(output, "%d,%03d", number / 1000, number % 1000);
    }
    else
    {
        sprintf(output, "%d", number);
    }
}

/*
 * Converts a number into the three letter code of a month
 */
void atomonth(int number, char* output)
{
    switch (number)
    {
    case 1:
        sprintf(output, "Jan");
        break;
    case 2:
        sprintf(output, "Feb");
        break;
    case 3:
        sprintf(output, "Mar");
        break;
    case 4:
        sprintf(output, "Apr");
        break;
    case 5:
        sprintf(output, "May");
        break;
    case 6:
        sprintf(output, "Jun");
        break;
    case 7:
        sprintf(output, "Jul");
        break;
    case 8:
        sprintf(output, "Aug");
        break;
    case 9:
        sprintf(output, "Sep");
        break;
    case 10:
        sprintf(output, "Oct");
        break;
    case 11:
        sprintf(output, "Nov");
        break;
    case 12:
        sprintf(output, "Dec");
        break;
    }
}

/*
 * Display a single score.
 * Assumes the high score list is already open.
 */




/*
 * Display the scores in a given range.
 * Assumes the high score list is already open.
 * Only five entries per line, too much info.
 *
 * Mega-Hack -- allow "fake" entry at the given position.
 */


/* Show 20 compact entries per page ---------------------------------- */


/*
 * Hack -- Display the scores in a given range and quit.
 *
 * This function is only called from "main.c" when the user asks
 * to see the "high scores".
 */


/* Public entry - compact list */



/* =============================================================
 * Story display helpers & updated print_story() implementation
 * =============================================================
 *  Added features
 *    1.  Optional `last_parts` argument - when >0 only the *last*
 *        N matching story chapters are shown.
 *    2.  Optional `fade_in` boolean - when true, paragraphs are
 *        displayed with a colour fade-in effect.
 *
 *  Timing (per @Roman request 2025-07-30, amended)
 *    * 125 ms between colour steps
 *    * 1 second pause after each paragraph/"block"
 *
 *  UI tweaks 2025-07-30 - v4
 *    * Prints a **visible blank line** between paragraphs (not just
 *      a row counter increment).
 *    * [Esc] now finishes the *current* page instantly (no fades /
 *      delays) and proceeds through the rest of the story without
 *      further prompts, auto-scrolling as needed. Nothing skipped.
 *    * Footer fully wipes the bottom line before drawing its prompt
 *      to avoid text overlap.
 *
 * ===========================================================*/

#include "angband.h"
#include "externs.h"
#include <stdbool.h>

/* -------------------------------------------------------------
 * Helper: colour fade-in paragraph printer
 * Returns true if completed normally, false if interrupted by Esc
 * ----------------------------------------------------------- */
/* Return values: 0=completed normally, 1=other key pressed (skip paragraph), 2=ESC pressed (fast-forward) */
static int print_paragraph_fade(cptr text, int row, int indent,
                                 int wrap_width)
{
    const byte fade_cols[] = {
        TERM_L_DARK, TERM_SLATE, TERM_L_WHITE, TERM_WHITE
    };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));

    for (int s = 0; s < steps; s++)
    {
        /* Check for key press during fade */
        char ch;
        if (Term_inkey(&ch, false, false) == 0) /* Non-blocking check */
        {
            /* Consume the key */
            Term_inkey(&ch, false, true); /* Remove the key from queue */
            /* Show final text state immediately */
            text_out_indent = indent;
            text_out_wrap   = wrap_width;
            Term_gotoxy(indent, row);
            text_out_to_screen(TERM_WHITE, text);
            text_out_wrap   = 0;
            text_out_indent = 0;
            Term_fresh();
            /* Return different codes for ESC vs other keys */
            return (ch == ESCAPE) ? 2 : 1;
        }

        text_out_indent = indent;
        text_out_wrap   = wrap_width;
        Term_gotoxy(indent, row);
        text_out_to_screen(fade_cols[s], text);
        text_out_wrap   = 0;
        text_out_indent = 0;
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }

    /* Check for key press during final delay */
    char ch;
    if (Term_inkey(&ch, false, false) == 0) /* Non-blocking check */
    {
        /* Consume the key */
        Term_inkey(&ch, false, true); /* Remove the key from queue */
        /* Return different codes for ESC vs other keys */
        return (ch == ESCAPE) ? 2 : 1;
    }

    Term_xtra(TERM_XTRA_DELAY, 1000);
    return 0; /* Completed normally */
}

/* -------------------------------------------------------------
 * Public helper: fade-in a single line/paragraph at a row
 * ----------------------------------------------------------- */
void print_fade_line(cptr text, int row, int indent)
{
    int wid, h;
    Term_get_size(&wid, &h);
    int wrap_width = wid - indent - 1;
    if (wrap_width < 10) wrap_width = 10;
    /* Reuse the paragraph fade; ignore return value here (non-interactive hint) */
    (void)print_paragraph_fade(text, row, indent, wrap_width);
}

/* -------------------------------------------------------------
 * Public helper: fade-in centered text, wrapping at (wid - 15)
 *  - Vertically centers the block of 1..N wrapped lines
 *  - Horizontally centers each line
 *  - Wraps on word boundaries; if a single word exceeds the
 *    width, it will be hard-split to avoid overflow
 * ----------------------------------------------------------- */
void print_fade_centered(cptr text)
{
    if (!text || !*text) return;

    int wid, h;
    Term_get_size(&wid, &h);

    int max_width = wid - 15;
    if (max_width < 10) max_width = (wid > 2 ? wid - 2 : wid);
    if (max_width < 1) max_width = 1;

    /* Simple word-wrapping into a small fixed buffer */
    enum { MAX_LINES = 32, MAX_LEN = 255 };
    char lines[MAX_LINES][MAX_LEN + 1];
    int  nlines = 0;

    const char *p = text;
    while (*p && nlines < MAX_LINES)
    {
        /* Start a new line */
        int linelen = 0;
        lines[nlines][0] = '\0';

        /* Skip leading spaces */
        while (*p && isspace((unsigned char)*p)) p++;

        while (*p)
        {
            /* Identify next word */
            const char *w = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            int wlen = (int)(p - w);

            /* If word does not fit on empty line: hard split */
            if (wlen > max_width && linelen == 0)
            {
                int take = (wlen > max_width) ? max_width : wlen;
                if (take > MAX_LEN) take = MAX_LEN;
                memcpy(lines[nlines], w, (size_t)take);
                linelen = take;
                lines[nlines][linelen] = '\0';

                /* Move pointer back to remaining part of the word */
                w += take;
                wlen -= take;
                p = w; /* continue from remainder */
                break; /* line filled */
            }

            /* Would adding this word (plus space if needed) fit? */
            int need = (linelen ? 1 : 0) + wlen;
            if (linelen + need <= max_width && linelen + need <= MAX_LEN)
            {
                if (linelen)
                    lines[nlines][linelen++] = ' ';
                memcpy(lines[nlines] + linelen, w, (size_t)wlen);
                linelen += wlen;
                lines[nlines][linelen] = '\0';
            }
            else
            {
                /* Doesn't fit: rewind so the word is processed on the next line */
                p = w;
                break;
            }

            /* Skip spaces to next word */
            while (*p && isspace((unsigned char)*p)) { if (*p == '\n') break; p++; }

            /* Stop at hard newline to keep author's breaks */
            if (*p == '\n') { p++; break; }
        }

        nlines++;

        /* Respect explicit newline(s) by collapsing consecutive breaks */
        while (*p == '\n') p++;
    }

    if (nlines == 0) return;

    /* Vertically center the block */
    int start_row = (h - nlines) / 2;
    if (start_row < 0) start_row = 0;

    /* Print each line centered with fade */
    for (int i = 0; i < nlines; i++)
    {
        int len = (int)strlen(lines[i]);
        if (len > wid) len = wid; /* paranoia */
        int indent = (wid - len) / 2;
        if (indent < 0) indent = 0;
        int wrap_width = wid - indent - 1;
        if (wrap_width < len) wrap_width = len; /* print as-is */
        (void)print_paragraph_fade(lines[i], start_row + i, indent, wrap_width);
    }
}

/* -------------------------------------------------------------
 * Public helper: fade-in text at a row, left-aligned with indent
 *  - Starts at the provided row (no vertical centering)
 *  - Left aligned at column >= 14, and for each subsequent line
 *    indentation increases by 2 columns (14, 16, 18, ...)
 *  - Wraps dynamically per line width to ensure nothing is cut off
 *  - Adds a 500 ms delay between lines
 *  - Clears the printed region after a short hold to avoid artifacts
 * ----------------------------------------------------------- */
void print_fade_centered_at_row(cptr text, int row_start)
{
    if (!text || !*text) return;

    int wid, h;
    Term_get_size(&wid, &h);

    /* Force to second row (index 1) if the caller requests anything above it */
    if (row_start < 1) row_start = 1;
    if (row_start >= h) return; /* off-screen */

    sdl_story_font_enable();
    log_debug("Depth banner: story font enabled");

    /* Dynamic per-line wrapping and printing */
    enum { MAX_LINES2 = 32, MAX_LEN2 = 255 };
    const char *p = text;
    int printed_lines = 0;
    /* Removed tracking arrays (line_start_cols/line_lengths) as they were
       only used for a future erase effect that is no longer implemented. */

    /* Start at the requested column; align to left-half in bigtile to avoid residuals */
    int base_indent = 14;
    if (use_bigtile)
    {
        /* Left halves are at COL_MAP, COL_MAP+2, ...; if we hit a right half, bump */
        if (((base_indent - COL_MAP) & 1) != 0) base_indent++;
    }

    while (*p && printed_lines < MAX_LINES2 && (row_start + printed_lines) < h)
    {
    int indent = base_indent + 2 * printed_lines; /* left sticky, step by +2 each line */
    if (indent >= wid - 1) break; /* nothing to show */
    int avail = wid - indent - 1;
    if (avail < 8) avail = 8; /* minimal width */

        char buf[MAX_LEN2 + 1];
        int  linelen = 0;
        buf[0] = '\0';

        /* Skip leading spaces/newlines */
        while (*p && (unsigned char)*p <= ' ') {
            if (*p == '\n') { p++; break; }
            p++; 
        }

        while (*p)
        {
            if (*p == '\n') { p++; break; }
            const char *w = p;
            while (*p && *p != '\n' && !isspace((unsigned char)*p)) p++;
            int wlen = (int)(p - w);

            if (wlen > avail && linelen == 0)
            {
                int take = (wlen > avail) ? avail : wlen;
                if (take > MAX_LEN2) take = MAX_LEN2;
                memcpy(buf, w, (size_t)take);
                linelen = take;
                buf[linelen] = '\0';
                w += take; /* remainder for next loop */
                p = w; 
                break;
            }

            int need = (linelen ? 1 : 0) + wlen;
            if (linelen + need <= avail && linelen + need <= MAX_LEN2)
            {
                if (linelen) buf[linelen++] = ' ';
                memcpy(buf + linelen, w, (size_t)wlen);
                linelen += wlen; buf[linelen] = '\0';
            }
            else
            {
                /* Defer word to the next line */
                p = w;
                break;
            }

            while (*p && isspace((unsigned char)*p)) { if (*p == '\n') break; p++; }
            if (*p == '\n') { p++; break; }
        }

        if (linelen == 0) break; /* nothing collected */

        /* Show this line directly in orange (no fade effect for level entry banners) */
        c_put_str(TERM_ORANGE, buf, row_start + printed_lines, indent);
        Term_fresh();

    /* Tracking of per-line geometry removed (unused). */
        printed_lines++;

        /* 700ms gap before next line if more text remains */
        if (*p && (row_start + printed_lines) < h)
            Term_xtra(TERM_XTRA_DELAY, 800);
    }

    log_debug("Depth banner: story font disabled");
    sdl_story_font_disable();

    /* Do not explicitly erase: allow natural redraws to overwrite the text */
}

/* -------------------------------------------------------------
 * print_story() - paging, subset & fade-in options
 * ----------------------------------------------------------- */
static void story_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static void story_print_hint(int indent, int h)
{
    if (steamdeck_controls_active()) {
        char next_label[16];
        char esc_label[16];
        char prompt_buf[80];

        story_prompt_label(' ', "A", next_label, sizeof(next_label));
        story_prompt_label(ESCAPE, "ESC", esc_label, sizeof(esc_label));

        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] next  *  [%s] fast forward", next_label, esc_label);
        Term_putstr(indent, h - 1, -1, TERM_SLATE, prompt_buf);
    } else {
        Term_putstr(indent, h - 1, -1, TERM_SLATE, "[Enter] next  *  [Esc] fast forward");
    }
}

void print_story(int last_parts, bool fade_in)
{
    int wid, h;
    const int indent = 2;
    bool fast_forward = false;
    bool show_page_instantly = false;
    bool _saved_cursor_state = false;
    bool _saved_hide_cursor = false;

    log_debug("=== Starting story display (parts=%d, fade_in=%s) ===", last_parts, fade_in ? "true" : "false");
    log_debug("last_parts=%d, fade_in=%s", last_parts, fade_in ? "true" : "false");

    /* Convenience macro to keep the bottom-line hint fresh */
#define REDRAW_HINT() \
    story_print_hint(indent, h)

    /* Build list of matching entries ------------------------ */
    int sils   = metar.silmarils;
    byte rt    = metar.type;
    int total  = 0;
    int max_st = z_info->st_max;
    static int sel_idx[1024];
    if (max_st > (int)N_ELEMENTS(sel_idx)) max_st = (int)N_ELEMENTS(sel_idx);

    log_debug("Building story list: sils=%d, rt=%d, max_st=%d", sils, rt, max_st);

    for (int i = 0; i < max_st; i++)
    {
        story_type *st = &st_info[i];
        if (!st->name && !st->text) continue;
        if (st->st_type != 0)               continue; /* opening */
        if (!(st->runtypes == 0 ||
              (rt < 32 && (st->runtypes & (1UL << rt)))))
            continue;                                   /* run-type */
        if (st->order <= (byte)sils) {
            sel_idx[total++] = i;
            log_trace("Added story %d (order=%d) to selection", i, st->order);
        }
    }

    log_debug("Found %d matching stories for display", total);
    
    if (total == 0) {
        log_debug("No stories match criteria - sils=%d, rt=%d", sils, rt);
        return;
    }

    /* Stable insertion sort by order ------------------------ */
    for (int i = 1; i < total; i++)
    {
        int key = sel_idx[i];
        byte key_ord = st_info[key].order;
        int j = i - 1;
        while (j >= 0 && st_info[sel_idx[j]].order > key_ord)
        {
            sel_idx[j + 1] = sel_idx[j];
            j--; }
        sel_idx[j + 1] = key;
    }

    /* Restrict to the last N parts if requested ------------- */
    int start = (last_parts > 0 && last_parts < total) ? total - last_parts : 0;
    log_debug("Story range: start=%d, total=%d", start, total);

    /* Screen prep ------------------------------------------- */
    Term_get_size(&wid, &h);
    screen_save();
    Term_clear();
    /* Hide the cursor during story display and restore it at the end */
    (void)Term_get_cursor(&_saved_cursor_state);
    /* Prevent inkey() from showing the cursor while story is active */
    _saved_hide_cursor = hide_cursor;
    hide_cursor = true;
    (void)Term_set_cursor(false);

    sdl_story_font_enable();  // Enable for entire story display

    Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
    int row = 2;
    REDRAW_HINT();

    /* Main loop -------------------------------------------- */
    for (int idx = start; idx < total; idx++)
    {
        story_type *st = &st_info[sel_idx[idx]];

        /* Calculate wrap width and get text */
        int wrap_width = wid - indent - 1;
        if (wrap_width < 20) wrap_width = 20;
        cptr text = st_text + st->text;
        
        /* Check if we need to paginate BEFORE rendering this story */
        /* Calculate actual space needed based on text content */
        int text_lines = count_wrapped_lines_story(text, wrap_width, indent);
        
        /* Space needed: 1 for heading + text_lines + 1 for blank line */
        int estimated_space_needed = 1 + text_lines + 1;
        
        if (row + estimated_space_needed >= h - 2)
        {
            if (!fast_forward)
            {
                show_page_instantly = false;
                REDRAW_HINT();
                char ch = inkey();
                if (ch == ESCAPE)
                {
                    fast_forward = true;
                    fade_in = false;
                    Term_erase(0, h - 1, wid);
                    log_debug("User pressed ESC - enabling fast forward mode");
                }
                else
                {
                    row = 2;
                    Term_clear();
                    Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
                    REDRAW_HINT();
                }
            }
            else
            {
                row = 2;
                Term_clear();
                Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
            }
        }

        /* Heading */
        Term_putstr(indent, row, -1, TERM_L_BLUE, st_name + st->name);
        row++;
        
        /* Body */

        if (fade_in && !fast_forward && !show_page_instantly)
        {
            /* print_paragraph_fade returns:
             *   0 = completed normally
             *   1 = other key pressed (skip this paragraph)
             *   2 = ESC pressed (enable fast-forward) */
            int fade_result = print_paragraph_fade(text, row, indent, wrap_width);
            if (fade_result == 2) {
                /* ESC pressed - enable fast-forward mode */
                fast_forward = true;
                fade_in = false;
                log_debug("ESC pressed during fade - enabling fast forward mode");
            }
            /* If fade_result == 1, just continue to next paragraph normally */
        }
        else
        {
            text_out_indent = indent;
            text_out_wrap   = wrap_width;
            Term_gotoxy(indent, row);
            text_out_to_screen(TERM_WHITE, text);
            text_out_wrap   = 0;
            text_out_indent = 0;
            if (!fast_forward && !show_page_instantly) 
                Term_xtra(TERM_XTRA_DELAY, 1000);
        }

        /* Get actual cursor position after text rendering */
        int cursor_x, cursor_y;
        Term_locate(&cursor_x, &cursor_y);
    /* Advance row past rendered text */
    row = cursor_y + 1;

        /* Check if we'll have room for the blank line before adding it */
        bool will_add_blank_line = (idx < total - 1);
        
        /* Pagination logic - check if we need to paginate BEFORE adding blank line */
    int space_needed = will_add_blank_line ? 1 : 0;
        
        bool paginated = false;
        if (row + space_needed >= h - 2)
        {
            paginated = true;
            if (!fast_forward)
            {
                /* Reset show_page_instantly for next page */
                show_page_instantly = false;
                
                REDRAW_HINT();
                char ch = inkey();
                if (ch == ESCAPE)
                {
                    fast_forward = true;
                    fade_in      = false;   /* Disable delays for rest of story */
                    Term_erase(0, h - 1, wid); /* clear hint line */
                    log_debug("User pressed ESC - enabling fast forward mode");
                }
                else /* Enter */
                {
                    row = 2;
                    Term_clear();
                    sdl_story_font_enable();
                    Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
                    sdl_story_font_disable();
                    REDRAW_HINT();
                    continue;
                }
            }
            else
            {
                /* fast-forward mode: auto-clear, no waits */
                row = 2;
                Term_clear();
                Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
            }
        }

        /* Add visible blank spacer line between paragraphs if there's room and we didn't paginate */
        if (will_add_blank_line && !paginated)
        {
            Term_putstr(indent, row, -1, TERM_WHITE, "");
            row++;
        }
    }

    /* Footer ------------------------------------------------ */
    Term_erase(0, h - 1, wid); /* clear bottom line entirely */
    if (steamdeck_controls_active()) {
        char next_label[16];
        char prompt_buf[64];
        story_prompt_label(' ', "A", next_label, sizeof(next_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] continue", next_label);
        Term_putstr(indent, h - 1, -1, TERM_L_WHITE, prompt_buf);
    } else {
        Term_putstr(indent, h - 1, -1, TERM_L_WHITE,
                    "[Press any key to continue]");
    }
    (void)inkey();
    
    /* Flush any queued keypresses that accumulated during the story */
    Term_flush();
    
    sdl_story_font_disable();  // Disable after story display
    screen_load();
    /* Restore previous cursor visibility and hide_cursor flag */
    (void)Term_set_cursor(_saved_cursor_state);
    hide_cursor = _saved_hide_cursor;

    log_debug("Story display completed");

#undef REDRAW_HINT
}

/*
 * Hack - save index of player's high score
 */
static int score_idx = -1;

/*
 * Counts the player's silmarils
 */
extern int silmarils_possessed(void)
{
    int silmarils = 0;
    int i;

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        if (((&inventory[i])->tval == TV_LIGHT)
            && ((&inventory[i])->sval == SV_LIGHT_SILMARIL))
            silmarils += (&inventory[i])->number;
        if ((&inventory[i])->name1 == ART_MORGOTH_1)
            silmarils += 1;
        if ((&inventory[i])->name1 == ART_MORGOTH_2)
            silmarils += 2;
        if ((&inventory[i])->name1 == ART_MORGOTH_3)
            silmarils += 3;
    }

    return silmarils;
}

/*
 * Checks if the player has Morgoth's crown (any version) in inventory
 * Returns the crown artifact number (ART_MORGOTH_0-3) or 0 if not found
 */
extern int has_iron_crown(void)
{
    int i;

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        int name1 = (&inventory[i])->name1;
        if ((name1 >= ART_MORGOTH_0) && (name1 <= ART_MORGOTH_3))
        {
            return name1;  // Return which crown variant they have
        }
    }

    return 0;  // No crown
}

/*
 * Creates a score record for the player
 */
errr create_score(high_score* the_score)
{
    /* Clear the record */
    memset(the_score, 0, sizeof(high_score));

    /* Save the version */
    strnfmt(the_score->what, sizeof(the_score->what), "%s", VERSION_STRING);

    /* Store the net curse count (curses - blessings)
     * curse_stacks[i] > 0 means curses, < 0 means blessings, so sum gives net value */
    int curse_total = 0;
    for (int id = 0; id < METAR_CURSE_SLOTS; ++id)
    {
        curse_total += CURSE_GET(id);
    }
    strnfmt(the_score->pts, sizeof(the_score->pts), "%4d", curse_total);

    /* Save the current player turn */
    strnfmt(
        the_score->turns, sizeof(the_score->turns), "%9lu", (long)playerturn);
    the_score->turns[9] = '\0';

    /* Save the date in standard encoded form */
    strftime(the_score->day, sizeof(the_score->day), "@%Y%m%d",
        localtime(&death_time));

    /* Save the player name (15 chars) - fall back to base_name to avoid empty live entries */
    const char* score_name = op_ptr->full_name;
    if (!score_name || !score_name[0]) {
        score_name = op_ptr->base_name[0] ? op_ptr->base_name : "nameless";
        log_warn("create_score: full_name empty, using fallback '%s' for score entry", score_name);
    }
    strnfmt(the_score->who, sizeof(the_score->who), "%-.15s", score_name);

    /* Save the player info XXX XXX XXX */
    strnfmt(the_score->uid, sizeof(the_score->uid), "%7u", player_uid);
    strnfmt(the_score->p_r, sizeof(the_score->p_r), "%2d", p_ptr->prace);
    strnfmt(the_score->p_h, sizeof(the_score->p_h), "%2d", p_ptr->pcharacter);

    /* Save the level and such */
    strnfmt(
        the_score->cur_dun, sizeof(the_score->cur_dun), "%3d", p_ptr->depth);
    the_score->cur_dun[3] = '\0';
    strnfmt(the_score->max_dun, sizeof(the_score->max_dun), "%3d",
        p_ptr->max_depth);
    the_score->max_dun[3] = '\0';

    /* Save unique monsters killed count */
    int uniques_killed = unique_bane_type_killed();
    strnfmt(the_score->cur_lev, sizeof(the_score->cur_lev), "%3d", uniques_killed);
    the_score->cur_lev[3] = '\0';

    /* Save the cause of death (49 chars) */
    strnfmt(the_score->how, sizeof(the_score->how), "%-.49s", p_ptr->died_from);

    /* Save the number of silmarils, whether morgoth is slain, whether the
     * player has escaped */
    int recorded_silmarils = silmarils_possessed();
    if (p_ptr->morgoth_slain && recorded_silmarils < 3)
        recorded_silmarils = 3;
    strnfmt(the_score->silmarils, sizeof(the_score->silmarils), "%1d",
        recorded_silmarils);
    the_score->silmarils[1] = '\0';

    if (p_ptr->morgoth_slain)
    {
        strnfmt(
            the_score->morgoth_slain, sizeof(the_score->morgoth_slain), "t");
    }
    else
    {
        strnfmt(
            the_score->morgoth_slain, sizeof(the_score->morgoth_slain), "f");
    }
    if (p_ptr->escaped)
    {
        strnfmt(the_score->escaped, sizeof(the_score->escaped), "t");
    }
    else
    {
        strnfmt(the_score->escaped, sizeof(the_score->escaped), "f");
    }

    return (0);
}

/*
 * Enters a player's name on a hi-score table, if "legal".
 *
 * Assumes "signals_ignore_tstp()" has been called.
 */
static errr enter_score(high_score* the_score)
{
#ifndef SCORE_CHEATERS
    int j;
#endif /* SCORE_CHEATERS */

    /* No score file */
    if (!highscore_fd)
    {
        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score file found)");
        return (0);
    }

#ifndef SCORE_WIZARDS
    /* Wizard-mode pre-empts scoring */
    if (p_ptr->noscore & 0x000F)
    {
        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score for wizards)");
        score_idx = -1;
        return (0);
    }
#endif

    /* Hack -- Interupted */
    if (!p_ptr->escaped && streq(p_ptr->died_from, "Interrupting"))
    {
        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when interrupted)");
        score_idx = -1;
        return (0);
    }

    /* Allow recording of voluntary death ("their own hand").
       This ensures aborted characters are written to the score file and
       won't be treated as alive on the next startup. */

#ifndef SCORE_CHEATERS
    /* Cheaters are not scored */
    for (j = OPT_SCORE; j < OPT_MAX; ++j)
    {
        if (!op_ptr->opt[j])
            continue;

        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when cheating)");
        score_idx = -1;
        return (0);
    }

    // People who cheated death are not scored
    if (p_ptr->noscore & 0x0001)
    {
        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when cheating)");
        score_idx = -1;
        return (0);
    }
#endif /* SCORE_CHEATERS */

    /* Grab permissions */
    safe_setuid_grab();

    /* Lock (for writing) the highscore file, or fail */
    /* TODO: File locking not supported with FILE* - temporarily disabled */
    /* if (fd_lock(highscore_fd, F_WRLCK)) */
    if (0)
        return (1);

    /* Drop permissions */
    safe_setuid_drop();

    /* Add a new entry to the score list, see where it went */
    score_idx = highscore_add(the_score);

    /* Close the file after writing.
     * Functions that need to read scores will open the file fresh. */
    if (highscore_fd)
    {
        /* Grab permissions */
        safe_setuid_grab();
        
        SDL_CloseIO(highscore_fd);
        highscore_fd = NULL;
        
        /* Drop permissions */
        safe_setuid_drop();
    }

    /* Grab permissions */
    safe_setuid_grab();

    /* Unlock the highscore file, or fail */
    /* TODO: File locking not supported with FILE* - temporarily disabled */
    /* if (fd_lock(highscore_fd, F_UNLCK)) */
    if (0)
        return (1);

    /* Drop permissions */
    safe_setuid_drop();

    /* Success */
    return (0);
}

/*
 * Enters a player's name on a hi-score table, if "legal", and in any
 * case, displays some relevant portion of the high score list.
 *
 * Assumes "signals_ignore_tstp()" has been called.
 */

/*
 * Predict the player's location, and display it.
 */
bool build_live_preview_score(high_score* out)
{
    if (!out || !character_generated)
        return false;

    char saved_how[sizeof(p_ptr->died_from)];
    SDL_strlcpy(saved_how, p_ptr->died_from, sizeof(saved_how));

    time_t previous_time = death_time;
    time_t now = time(NULL);
    if (now != (time_t)-1)
        death_time = now;

    SDL_strlcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));

    bool ok = (create_score(out) == 0);

    SDL_strlcpy(p_ptr->died_from, saved_how, sizeof(saved_how));
    death_time = previous_time;

    return ok;
}




/* Display the high score table (optionally long form) without committing a new score.
 * If character_generated is true and player is alive, show predicted placement.
 */























/*  Returns NULL when nothing was slain, or a static string with the
 *  character name of the slain hero.  If @do_roll is false, the caller has
 *  already performed the RNG check and we kill un-conditionally.       */
const char *kinslayer_try_kill(uint8_t n_sils, bool do_roll)
{
    log_info("Kinslayer attempt: n_sils=%u", n_sils);

    /* 1) Probability check */
    static const int pct_tab[4] = { 0, 20, 50, 95 };
    if (do_roll) {
        if (n_sils == 0) return NULL;
        if (n_sils > 3)  n_sils = 3;
        int roll = rand_int(100);
        if (roll >= pct_tab[n_sils]) {
            log_debug("Kinslayer roll failed: %d >= %d (n_sils=%d)", roll, pct_tab[n_sils], n_sils);
            return NULL;
        }
    }

    /* 2) Build path to scores.raw */
    char score_path[1024];
    build_meta_path(score_path, sizeof(score_path), "scores.raw");

    /* 3) Open global highscore_fd (version-aware) if not already open */
    if (!highscore_fd) {
        log_trace("highscore_fd < 0, opening %s (version-aware)", score_path);
        safe_setuid_grab();
        highscore_fd = score_file_open(score_path, O_RDWR);
        safe_setuid_drop();
        if (!highscore_fd) {
            quit(format("Cannot open %s (%d)", score_path, errno));
            return NULL; /* NOTREACHED */
        }
        log_trace("opened highscore_fd (score file loaded)");
    }

    /* 4) Determine number of records (exclude header) */
    SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_END);
    off_t file_end = SDL_TellIO(highscore_fd);
    off_t payload  = file_end - (off_t)sizeof(score_file_header);
    int n_recs = (int)(payload / (off_t)sizeof(high_score));
    log_trace("hi-score file size=%lld, payload=%lld, records=%d",
              (long long)file_end, (long long)payload, n_recs);

    /* 5) Build list of races with eligible characters and apply weighted selection */
    
    bool *hero_ineligible = calloc(z_info->c_max, sizeof(*hero_ineligible));
    if (!hero_ineligible) {
        safe_setuid_grab();
        if (SDL_CloseIO(highscore_fd) != 0)
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        safe_setuid_drop();
        highscore_fd = NULL;
        quit("Out of memory in kinslayer_try_kill()");
    }

    if (n_recs > 0 && highscore_seek(0) == 0) {
        high_score entry;
        for (int r = 0; r < n_recs; ++r) {
            if (highscore_read(&entry)) break;
            int character = parse_score_id(entry.p_h);
            if (character < 0 || character >= (int)z_info->c_max)
                continue;
            bool escaped = (tolower((unsigned char)entry.escaped[0]) == 't');
            bool dead = (strcmp(entry.how, "(alive and well)") != 0);
            if (escaped || dead)
                hero_ineligible[character] = true;
        }
    }

    /* 5.a) First pass: identify which races have eligible characters */
    uint16_t eligible_races[RACE_PRIORITIES];
    size_t eligible_count = 0;
    
    for (size_t i = 0; i < RACE_PRIORITIES && eligible_count < RACE_PRIORITIES; ++i) {
        uint16_t race = race_priority[i];
        
        /* Check if this race has any eligible characters */
        bool has_eligible = false;
        for (uint16_t h = 0; h < z_info->c_max; ++h) {
            if (!race_has_character(race, h)) continue;
            if (hero_ineligible[h]) continue;
            const char *hname = c_name + c_info[h].name;
            if (strcmp(hname, op_ptr->base_name) == 0) continue;
            has_eligible = true;
            break;
        }
        
        if (has_eligible) {
            eligible_races[eligible_count++] = race;
            log_trace("race priority[%zu]=%u added to eligible list (position %zu)", 
                      i, race, eligible_count - 1);
        } else {
            log_trace("race priority[%zu]=%u has no eligible characters, skipping", i, race);
        }
    }
    
    if (eligible_count == 0) {
        log_debug("No eligible races found - no kill performed");
        free(hero_ineligible);
        safe_setuid_grab();
        if (SDL_CloseIO(highscore_fd) != 0)
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        safe_setuid_drop();
        highscore_fd = NULL;
        return NULL;
    }
    
    /* 5.b) Apply weighted random selection to first 3 eligible races */
    /* Weights: 50%, 30%, 20% (normalized to 100) */
    static const int weights[3] = { 50, 30, 20 };
    int total_weight = 0;
    int applicable_races = (eligible_count < 3) ? (int)eligible_count : 3;
    
    for (int i = 0; i < applicable_races; ++i) {
        total_weight += weights[i];
    }
    
    /* Select race using weighted random */
    int roll = rand_int(total_weight);
    int cumulative = 0;
    uint16_t selected_race = eligible_races[0]; /* fallback */
    
    for (int i = 0; i < applicable_races; ++i) {
        cumulative += weights[i];
        if (roll < cumulative) {
            selected_race = eligible_races[i];
            log_info("Weighted race selection: chose race %u (position %d, weight %d%%)", 
                     selected_race, i, weights[i]);
            break;
        }
    }
    
    /* 5.c) Now process the selected race */
    uint16_t race = selected_race;
    log_trace("Processing selected race=%u", race);
    
    /* Build pool of eligible characters for selected race */
    uint16_t *pool = malloc(z_info->c_max * sizeof *pool);
    if (!pool) {
        free(hero_ineligible);
        SDL_CloseIO(highscore_fd);
        quit("Out of memory in kinslayer_try_kill()");
    }
    size_t pool_n = 0;
    for (uint16_t h = 0; h < z_info->c_max; ++h) {
        if (!race_has_character(race, h)) continue;
        if (hero_ineligible[h]) continue;
        const char *hname = c_name + c_info[h].name;
        if (strcmp(hname, op_ptr->base_name) == 0) continue;
        pool[pool_n++] = h;
    }
    log_trace("race %u: %zu eligible characters", race, pool_n);
    if (pool_n == 0) {
        free(pool);
        free(hero_ineligible);
        safe_setuid_grab();
        if (SDL_CloseIO(highscore_fd) != 0)
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        safe_setuid_drop();
        highscore_fd = NULL;
        return NULL;
    }

    /* 5.d) Pick one character */
    uint16_t character_sel = pool[rand_int((int)pool_n)];
    const char *hname = c_name + c_info[character_sel].name;
    free(pool);
    pool = NULL;
    free(hero_ineligible);
    hero_ineligible = NULL;
    log_info("Kinslayer selected character %u (%s) for elimination", character_sel, hname);

    /* 5.e) Scan for existing entry */
    int hit = -1;
    high_score entry;
    for (int r = 0; r < n_recs; ++r) {
        if (highscore_seek(r)) break;
        if (highscore_read(&entry)) break;
        if (entry.p_r[0] == '0' + (race/10) &&
            entry.p_r[1] == '0' + (race%10) &&
            entry.p_h[0] == '0' + (character_sel/10) &&
            entry.p_h[1] == '0' + (character_sel%10)) {
            hit = r;
            break;
        }
    }
    log_trace("scan: entry_offset=%d", hit);

    if (hit >= 0) {
        /* 5.f) Found - check alive AND not escaped */
            if (highscore_dead(entry.who)) {
                log_debug("hero already dead - no kill performed");
                if (pool) free(pool);
                if (hero_ineligible) free(hero_ineligible);
                safe_setuid_grab();
                if (SDL_CloseIO(highscore_fd) != 0) {
                    log_warn("fclose(highscore_fd) failed, errno=%d", errno);
                }
                safe_setuid_drop();
                highscore_fd = NULL;
                return NULL;
            }
            /* Also check if hero has escaped */
            if (entry.escaped[0] == 't') {
                log_debug("hero has escaped - no kill performed");
                if (pool) free(pool);
                if (hero_ineligible) free(hero_ineligible);
                safe_setuid_grab();
                if (SDL_CloseIO(highscore_fd) != 0) {
                    log_warn("fclose(highscore_fd) failed, errno=%d", errno);
                }
                safe_setuid_drop();
                highscore_fd = NULL;
                return NULL;
            }
            /* kill existing */
            if (highscore_seek(hit) == 0 && highscore_read(&entry) == 0) {
                strnfmt(entry.how, sizeof entry.how, op_ptr->base_name);
                highscore_seek(hit);
                highscore_write(&entry);
                log_info("Kinslayer killed existing hero: \"%s\"", entry.who);
            } else {
                log_warn("Failed to re-read existing entry at slot %d", hit);
            }
        }
        else {
            /* 5.e) No record - insert dummy */
            high_score dummy;
            build_dummy_entry(&dummy, race, character_sel);
            log_trace("no existing record - inserting dummy \"%s\"", dummy.who);

            /* position for add */
            highscore_seek(0);
            int slot = highscore_add(&dummy);
            if (slot < 0)
                log_error("highscore_add() failed");
            else
                log_info("Kinslayer inserted dummy entry \"%s\" at slot %d",
                        dummy.who, slot);
        }

        /* 6) UI is now handled by metarun_update_on_exit() */
        static char killed_character[32];
        SDL_strlcpy(killed_character, hname, sizeof killed_character);

        /* 7) Close the descriptor and reset before returning */
        if (hero_ineligible) free(hero_ineligible);
        safe_setuid_grab();
        if (SDL_CloseIO(highscore_fd) != 0) {
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        }
        safe_setuid_drop();
        highscore_fd = NULL;
        return killed_character;
}

/*
 * Hack -- Dump a character description file
 *
 * XXX XXX XXX Allow the "full" flag to dump additional info,
 * and trigger its usage from various places in the code.
 */
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

static int final_menu(int* highlight)
{
    char ch;
    bool morgoth_victory = (p_ptr->morgoth_slain && !p_ptr->escaped);

    const char* option_a = morgoth_victory ? "a) Review the Valar's record"
                                           : "a) View scores";
    const char* option_b = morgoth_victory ? "b) Survey Angband one last time"
                                           : "b) Final look";
    const char* option_c = morgoth_victory ? "c) Rehear the proclamations"
                                           : "c) View final messages";
    const char* option_d = morgoth_victory ? "d) Review your legend"
                                           : "d) View character sheet";
    const char* option_e = morgoth_victory ? "e) Append to the annals"
                                           : "e) Add comment to notes";
    const char* option_f = morgoth_victory ? "f) Archive your legend"
                                           : "f) Save character sheet";
    const char* option_exit = "g) Exit";

    Term_putstr(3, 10, -1, TERM_L_DARK,
        "____________________________________________________");
    Term_putstr(15, 12, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        option_a);
    Term_putstr(15, 13, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        option_b);
    Term_putstr(15, 14, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        option_c);
    Term_putstr(15, 15, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        option_d);
    Term_putstr(15, 16, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        option_e);
    Term_putstr(15, 17, -1, (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE,
        option_f);
    Term_putstr(
        15, 19, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE, option_exit);

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(10, 18 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    if (ch == 'a')
    {
        *highlight = 1;
        return (1);
    }

    if (ch == 'b')
    {
        *highlight = 2;
        return (2);
    }

    if (ch == 'c')
    {
        *highlight = 3;
        return (3);
    }

    if (ch == 'd')
    {
        *highlight = 4;
        return (4);
    }

    if (ch == 'e')
    {
        *highlight = 5;
        return (5);
    }

    if (ch == 'f')
    {
        *highlight = 6;
        return (6);
    }

    if ((ch == 'g') || (ch == 'q') || (ch == 'Q'))
    {
        *highlight = 7;
        return (7);
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
            *highlight = 7;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < 7)
            (*highlight)++;
        else if (*highlight == 7)
            *highlight = 1;
    }

    return (0);
}

/*
 * Handle character death
 */
static void close_game_aux(void)
{
    static bool death_processing = false;
    bool wants_to_quit = false;
    high_score the_score;
    int choice = 0, highlight = 1;

    /* Prevent duplicate death processing */
    if (death_processing)
    {
        log_debug("Death processing already in progress - skipping duplicate call");
        return;
    }
    death_processing = true;

    log_debug("Processing character death for '%s' (wizard=%d, noscore=0x%04X, savefile='%s')",
             op_ptr->full_name, p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore, savefile);

    /* Dump bones file */
    // make_bones();

    /* Save dead player */
    log_info("saving dead player (noscore=0x%04X) -> '%s'", (unsigned)p_ptr->noscore, savefile);
    if (!save_player())
    {
        log_error("Death save failed - player data may be lost");
        msg_print("death save failed!");
        message_flush();
    }

    /* Get time of death */
    (void)time(&death_time);

    /* Clear screen */
    Term_clear();

    /* Enter player in high score list */
    log_info("entering score");
    create_score(&the_score);
    score_record_status final_status = p_ptr->escaped ? SCORE_RECORD_ESCAPED : SCORE_RECORD_DEAD;
    if (!score_runs_record_current_run(&the_score, death_time, final_status)) {
        log_warn("Failed to persist run statistics for '%s'", op_ptr->full_name);
    }
    enter_score(&the_score);

    // cure hallucination and rage
    p_ptr->rage = 0;
    p_ptr->image = 0;

    // Automatic character dump
    char curr_time[30], sheet[90];
    time_t ct = time((time_t*)0);
    (void)strftime(curr_time, 30, "%Y%m%d-%H%M%S.txt", localtime(&ct));
    sprintf(sheet, "%s-%s", op_ptr->full_name, curr_time);
    errr err;
    // Save the screen
    screen_save();
    // Dump a character file
    err = file_character(sheet, false);
    // Load the screen
    screen_load();
    // Check result
    if (err)
    {
        // Clear screen
        Term_clear();
        // Warning
        msg_print("Automatic character dump failed!");
        // Flush messages
        message_flush();
    }

    /* Record this run's outcome for the metarun ledger */
    int final_score = score_points(&the_score);
    if (p_ptr->morgoth_slain && !p_ptr->escaped)
    {
        log_info("Player achieved Morgoth victory - updating metarun data");
        metarun_update_on_exit(false, false, 3, final_score);
    }
    else
    {
        log_info("Player died - updating metarun data");
        if (!p_ptr->escaped)
            metarun_update_on_exit(true, false, 0, final_score);
    }

    /* Let the player inspect the final dungeon state before the tomb menu. */
    death_spectator_view();

    /* Restore a clean screen for the tombstone display. */
    Term_clear();

    /* Present the appropriate epitaph */
    print_tomb(&the_score);

    /* Flush all input keys */
    flush();

    /* Flush messages */
    message_flush();

    /* Loop */
    while (!wants_to_quit)
    {
        choice = final_menu(&highlight);

        switch (choice)
        {
        // view scores
        case 1:
        {
            show_scores_interactive_highlight(true, &the_score);
            break;
        }

        // final look
        case 2:
        {
            /* Save screen */
            screen_save();

            death_spectator_view();

            /* Load screen */
            screen_load();

            break;
        }

        // view final messages
        case 3:
        {
            /* Save screen */
            screen_save();

            /* Display messages */
            do_cmd_messages();

            /* Load screen */
            screen_load();
            break;
        }

        // view character sheet
        case 4:
        {
            /* Save screen */
            screen_save();

            /* Show the character */
            show_info();

            /* Load screen */
            screen_load();
            break;
        }

        // add comment to notes
        case 5:
        {
            do_cmd_note("", p_ptr->depth);
            break;
        }

        // save character sheet
        case 6:
        {
            char ftmp[80];

            strnfmt(ftmp, sizeof(ftmp), "%s.txt", op_ptr->base_name);

            if (term_get_string("File name: ", ftmp, sizeof(ftmp)))
            {
                if (ftmp[0] && (ftmp[0] != ' '))
                {
                    errr err;

                    /* Save screen */
                    screen_save();

                    /* Dump a character file */
                    err = file_character(ftmp, false);

                    /* Load screen */
                    screen_load();

                    /* Check result */
                    if (err)
                    {
                        msg_print("Character dump failed!");
                    }
                    else
                    {
                        msg_print("Character dump successful.");
                    }

                    /* Flush messages */
                    message_flush();
                }
            }
            break;
        }

        // exit
        case 7:
        {
            wants_to_quit = true;
            break;
        }
        }
    }

    /* Reset death processing flag for next character */
    death_processing = false;
}

/*
 * Close up the current game (player may or may not be dead)
 *
 * Note that the savefile is not saved until the tombstone is
 * actually displayed and the player has a chance to examine
 * the inventory and such.  This allows cheating if the game
 * is equipped with a "quit without save" method.  XXX XXX XXX
 */
void close_game(void)
{
    char buf[1024];

    log_info("Starting game close sequence for player '%s'", op_ptr->full_name);

    /* Handle stuff */
    handle_stuff();

    /* Flush the messages */
    message_flush();

    /* Flush the input */
    flush();

    /* No suspending now */
    signals_ignore_tstp();

    /* Hack -- Increase "icky" depth */
    character_icky++;
    log_debug("files.c: character_icky incremented to %d (opening scores file)", character_icky);

    /* Build the filename */
    build_meta_path(buf, sizeof(buf), "scores.raw");

    log_debug("Opening scores file for read/write: %s", buf);

    /* Create backup before opening for write operations */
    backup_scores_file(buf);

    /* Grab permissions */
    safe_setuid_grab();

    /* Open the high score file, for reading/writing */
    highscore_fd = score_file_open(buf, O_RDWR);

    /* Drop permissions */
    safe_setuid_drop();
    
    if (!highscore_fd) {
    log_error("Failed to open scores file for read/write");
    }

    /* Handle death */
    if (p_ptr->is_dead)
    {
        /* Auxiliary routine in normal games */
        if (p_ptr->game_type == 0)
        {
            log_info("Player %s died at depth %d in %s.",
                op_ptr->full_name, p_ptr->depth, p_ptr->died_from);
            close_game_aux();
        }
        else if (p_ptr->game_type == -1)
        {
            monster_lore* l_ptr = &l_list[R_IDX_ORC_ARCHER];

            if (p_ptr->chp <= 0)
            {
                if (l_ptr->psights == 0)
                {
                    pause_with_text(tutorial_early_death_text, 5, 10, NULL, 0);
                }
                else
                {
                    pause_with_text(tutorial_late_death_text, 5, 10, NULL, 0);
                }
            }
        }

        /* Now wipe the level */
        wipe_o_list();
        wipe_mon_list();
        cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    }

    /* Still alive */
    else
    {
        /* Save the game */
        do_cmd_save_game();

        Term_putstr(6, 0, -1, TERM_L_BLUE, "View high scores? (ESC to skip)");
        char prompt_key = inkey();
        Term_erase(0, 0, 255);
        if (prompt_key != ESCAPE)
        {
            high_score preview;
            if (build_live_preview_score(&preview))
                show_scores_interactive_highlight(true, &preview);
            else
                show_scores_interactive(true);
        }

        /* Update the live character entry in the scores file so that
           scores.raw acts as a database of current running characters.
           We record an entry with how == "(alive and well)". */
        if (highscore_fd) {
            char saved_how[sizeof(p_ptr->died_from)];
            SDL_strlcpy(saved_how, p_ptr->died_from, sizeof(saved_how));
            SDL_strlcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));
            high_score live_score;
            create_score(&live_score);

            /* Restore original (probably redundant during quit) */
            SDL_strlcpy(p_ptr->died_from, saved_how, sizeof(p_ptr->died_from));

            /* Acquire write lock while we upsert */
            safe_setuid_grab();
            /* TODO: File locking not supported with FILE* - temporarily disabled */
            /* bool have_lock = (fd_lock(highscore_fd, F_WRLCK) == 0); */
            bool have_lock = true; /* assume we have lock for now */
            safe_setuid_drop();

            if (!have_lock) {
                log_warn("Could not acquire lock to upsert live score entry");
            } else {
                /* Try to find existing alive entry for this player */
                if (highscore_seek(0) == 0) {
                    high_score tmp;
                    int idx;
                    bool found = false;
                    for (idx = 0; idx < MAX_HISCORES; idx++) {
                        if (highscore_read(&tmp)) break; /* EOF */
                        if (streq(tmp.who, live_score.who) && streq(tmp.how, "(alive and well)")) {
                            found = true; break;
                        }
                    }
                    if (found) {
                        log_debug("Updating existing live score entry for %s at %d", live_score.who, idx);
                        highscore_seek(idx);
                        highscore_write(&live_score);
                    } else {
                        log_debug("Inserting new live score entry for %s", live_score.who);
                        highscore_add(&live_score); /* adds (may not perfectly shift ordering) */
                    }
                }

                /* Release lock */
                safe_setuid_grab();
                /* TODO: File locking not supported with FILE* - temporarily disabled */
                /* (void)fd_lock(highscore_fd, F_UNLCK); */
                safe_setuid_drop();
            }
        }

        // Sil-y: Sil used to crash on loading a saved game from the main menu
        //        immediately after quitting via Control-X.
        //        adding the following lines seems to stop that.

        /* Now wipe the level */
        wipe_o_list();
        wipe_mon_list();
    }

    /* Shut the high score file */
    log_debug("Closing highscore file");
    SDL_CloseIO(highscore_fd);

    /* Forget the high score fd */
    highscore_fd = NULL;

    log_info("Game close sequence completed");

    /* Hack -- Decrease "icky" depth */
    character_icky--;
    log_debug("files.c: character_icky decremented to %d (scores file closed)", character_icky);

    /* Allow suspending now */
    signals_handle_tstp();
}

/*
 * Handle abrupt death of the visual system
 *
 * This routine is called only in very rare situations, and only
 * by certain visual systems, when they experience fatal errors.
 *
 * XXX XXX Hack -- clear the death flag when creating a HANGUP
 * save file so that player can see tombstone when restart.
 */
void exit_game_panic(void)
{
    /* If nothing important has happened, just quit */
    if (!character_generated || character_saved)
        quit("panic");

    /* Mega-Hack -- see "msg_print()" */
    msg_flag = false;

    /* Clear the top line */
    prt("", 0, 0);

    /* Hack -- turn off some things */
    disturb(1, 0);

    /* Hack -- Delay death XXX XXX XXX */
    if (p_ptr->chp <= 0)
        p_ptr->is_dead = false;

    /* Hardcode panic save */
    p_ptr->panic_save = 1;

    /* Forbid suspend */
    signals_ignore_tstp();

    /* Indicate panic save */
    SDL_strlcpy(p_ptr->died_from, "(panic save)", sizeof(p_ptr->died_from));

    /* Panic save, or get worried */
    if (!save_player())
        quit("panic save failed!");

    /* Successful panic save */
    quit("panic save succeeded!");
}

#ifdef HANDLE_SIGNALS

#include <signal.h>

typedef void (*Signal_Handler_t)(int);

/*
 * Wrapper around signal() which it is safe to take the address
 * of, in case signal itself is hidden by some some macro magic.
 */
static Signal_Handler_t wrap_signal(int sig, Signal_Handler_t handler)
{
    return signal(sig, handler);
}

/* Call this instead of calling signal() directly. */
Signal_Handler_t (*signal_aux)(int, Signal_Handler_t) = wrap_signal;

/*
 * Handle signals -- suspend
 *
 * Actually suspend the game, and then resume cleanly
 */
static void handle_signal_suspend(int sig)
{
    /* Protect errno from library calls in signal handler */
    int save_errno = errno;

    /* Disable handler */
    (void)(*signal_aux)(sig, SIG_IGN);

#ifdef SIGSTOP

    /* Flush output */
    Term_fresh();

    /* Suspend the "Term" */
    Term_xtra(TERM_XTRA_ALIVE, 0);

    /* Suspend ourself */
    (void)kill(0, SIGSTOP);

    /* Resume the "Term" */
    Term_xtra(TERM_XTRA_ALIVE, 1);

    /* Redraw the term */
    Term_redraw();

    /* Flush the term */
    Term_fresh();

#endif

    /* Restore handler */
    (void)(*signal_aux)(sig, handle_signal_suspend);

    /* Restore errno */
    errno = save_errno;
}

/*
 * Handle signals -- simple (interrupt and quit)
 *
 * This function was causing a *huge* number of problems, so it has
 * been simplified greatly.  We keep a global variable which counts
 * the number of times the user attempts to kill the process, and
 * we commit suicide if the user does this a certain number of times.
 *
 * We attempt to give "feedback" to the user as he approaches the
 * suicide thresh-hold, but without penalizing accidental keypresses.
 *
 * To prevent messy accidents, we should reset this global variable
 * whenever the user enters a keypress, or something like that.
 */
static void handle_signal_simple(int sig)
{
    /* Protect errno from library calls in signal handler */
    int save_errno = errno;

    /* Disable handler */
    (void)(*signal_aux)(sig, SIG_IGN);

    /* Nothing to save, just quit */
    if (!character_generated || character_saved)
        quit(NULL);

    /* Count the signals */
    signal_count++;

    /* Terminate dead characters */
    if (p_ptr->is_dead)
    {
        /* Mark the savefile */
        SDL_strlcpy(p_ptr->died_from, "Aborting", sizeof(p_ptr->died_from));

        /* HACK - Skip the tombscreen if it is already displayed */
        if (score_idx == -1)
        {
            /* Close stuff */
            close_game();
        }

        /* Quit */
        quit("interrupt");
    }

    /* Allow suicide (after 5) */
    else if (signal_count >= 5)
    {
        /* Cause of "death" */
        SDL_strlcpy(p_ptr->died_from, "Interrupting", sizeof(p_ptr->died_from));

        /* Commit suicide */
        p_ptr->is_dead = true;

        /* Stop playing */
        p_ptr->playing = false;

        /* Leaving */
        p_ptr->leaving = true;

        /* Close stuff */
        close_game();

        /* Quit */
        quit("interrupt");
    }

    /* Give warning (after 4) */
    else if (signal_count >= 4)
    {
        /* Make a noise */
        Term_xtra(TERM_XTRA_NOISE, 0);

        /* Clear the top line */
        Term_erase(0, 0, 255);

        /* Display the cause */
        Term_putstr(0, 0, -1, TERM_WHITE, "Contemplating suicide!");

        /* Flush */
        Term_fresh();
    }

    /* Give warning (after 2) */
    else if (signal_count >= 2)
    {
        /* Make a noise */
        Term_xtra(TERM_XTRA_NOISE, 0);
    }

    /* Restore handler */
    (void)(*signal_aux)(sig, handle_signal_simple);

    /* Restore errno */
    errno = save_errno;
}

/*
 * Handle signal -- abort, kill, etc
 */
static void handle_signal_abort(int sig)
{
    /* Disable handler */
    (void)(*signal_aux)(sig, SIG_IGN);

    /* Nothing to save, just quit */
    if (!character_generated || character_saved)
        quit(NULL);

    /* Clear the bottom line */
    Term_erase(0, 23, 255);

    /* Give a warning */
    Term_putstr(
        0, 23, -1, TERM_RED, "A gruesome software bug LEAPS out at you!");

    /* Message */
    Term_putstr(45, 23, -1, TERM_RED, "Panic save...");

    /* Flush output */
    Term_fresh();

    /* Panic Save */
    p_ptr->panic_save = 1;

    /* Panic save */
    SDL_strlcpy(p_ptr->died_from, "(panic save)", sizeof(p_ptr->died_from));

    /* Forbid suspend */
    signals_ignore_tstp();

    /* Attempt to save */
    if (save_player())
    {
        Term_putstr(45, 23, -1, TERM_RED, "Panic save succeeded!");
    }

    /* Save failed */
    else
    {
        Term_putstr(45, 23, -1, TERM_RED, "Panic save failed!");
    }

    /* Flush output */
    Term_fresh();

    /* Quit */
    quit("software bug");
}

/*
 * Ignore SIGTSTP signals (keyboard suspend)
 */
void signals_ignore_tstp(void)
{
#ifdef SIGTSTP
    (void)(*signal_aux)(SIGTSTP, SIG_IGN);
#endif
}

/*
 * Handle SIGTSTP signals (keyboard suspend)
 */
void signals_handle_tstp(void)
{
#ifdef SIGTSTP
    (void)(*signal_aux)(SIGTSTP, handle_signal_suspend);
#endif
}

/*
 * Prepare to handle the relevant signals
 */
void signals_init(void)
{
#ifdef SIGHUP
    (void)(*signal_aux)(SIGHUP, SIG_IGN);
#endif

#ifdef SIGTSTP
    (void)(*signal_aux)(SIGTSTP, handle_signal_suspend);
#endif

#ifdef SIGINT
    (void)(*signal_aux)(SIGINT, handle_signal_simple);
#endif

#ifdef SIGQUIT
    (void)(*signal_aux)(SIGQUIT, handle_signal_simple);
#endif

#ifdef SIGFPE
    (void)(*signal_aux)(SIGFPE, handle_signal_abort);
#endif

#ifdef SIGILL
    (void)(*signal_aux)(SIGILL, handle_signal_abort);
#endif

#ifdef SIGTRAP
    (void)(*signal_aux)(SIGTRAP, handle_signal_abort);
#endif

#ifdef SIGIOT
    (void)(*signal_aux)(SIGIOT, handle_signal_abort);
#endif

#ifdef SIGKILL
    (void)(*signal_aux)(SIGKILL, handle_signal_abort);
#endif

#ifdef SIGBUS
    (void)(*signal_aux)(SIGBUS, handle_signal_abort);
#endif

#ifdef SIGSEGV
    (void)(*signal_aux)(SIGSEGV, handle_signal_abort);
#endif

#ifdef SIGTERM
    (void)(*signal_aux)(SIGTERM, handle_signal_abort);
#endif

#ifdef SIGPIPE
    (void)(*signal_aux)(SIGPIPE, handle_signal_abort);
#endif

#ifdef SIGEMT
    (void)(*signal_aux)(SIGEMT, handle_signal_abort);
#endif

/*
 * SIGDANGER:
 * This is not a common (POSIX, SYSV, BSD) signal, it is used by AIX(?) to
 * signal that the system will soon be out of memory.
 */
#ifdef SIGDANGER
    (void)(*signal_aux)(SIGDANGER, handle_signal_abort);
#endif

#ifdef SIGSYS
    (void)(*signal_aux)(SIGSYS, handle_signal_abort);
#endif

#ifdef SIGXCPU
    (void)(*signal_aux)(SIGXCPU, handle_signal_abort);
#endif

#ifdef SIGPWR
    (void)(*signal_aux)(SIGPWR, handle_signal_abort);
#endif
}

#else /* HANDLE_SIGNALS */

/*
 * Do nothing
 */
void signals_ignore_tstp(void) { }

/*
 * Do nothing
 */
void signals_handle_tstp(void) { }

/*
 * Do nothing
 */
void signals_init(void) { }

#endif /* HANDLE_SIGNALS */

/*
 * Get the tile for a given screen location
 */
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

extern void mini_screenshot(void)
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

extern void prt_mini_screenshot(int col, int row)
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
bool autoload_alive_from_scores(void)
{
    log_info("===== autoload_alive_from_scores: FUNCTION CALLED =====");
    char score_path[1024];
    build_meta_path(score_path, sizeof(score_path), "scores.raw");

    /* Preserve global scorefile state */
    SDL_IOStream* saved_fd = highscore_fd;
    byte saved_major = scores_file_version_major;
    byte saved_minor = scores_file_version_minor;
    byte saved_patch = scores_file_version_patch;
    byte saved_extra = scores_file_version_extra;
    u32b saved_entry_count = scores_file_entry_count;

    /* Open with version detection (read/write so we can patch entries) */
    highscore_fd = score_file_open(score_path, O_RDWR | O_CREAT);
    if (!highscore_fd) {
        log_warn("autoload: could not open scorefile: %s", score_path);
        /* restore */
        highscore_fd = saved_fd;
        scores_file_version_major = saved_major;
        scores_file_version_minor = saved_minor;
        scores_file_version_patch = saved_patch;
        scores_file_version_extra = saved_extra;
        scores_file_entry_count = saved_entry_count;
        return false;
    }

    /* Determine number of records */
    int n_recs;
    SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_END);
    long file_size = SDL_TellIO(highscore_fd);
    SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_SET);
    
    long payload = file_size - (long)sizeof(score_file_header);
    if (payload < 0) payload = 0;
    n_recs = payload / (long)sizeof(high_score);
    /* Prefer header entry count if sane */
    if (scores_file_entry_count > 0 && (int)scores_file_entry_count <= n_recs)
        n_recs = (int)scores_file_entry_count;
    log_trace("autoload: scorefile n_recs=%d header_count=%u", n_recs, scores_file_entry_count);
    
    if (n_recs <= 0) {
        SDL_CloseIO(highscore_fd);
        highscore_fd = saved_fd;
        scores_file_version_major = saved_major;
        scores_file_version_minor = saved_minor;
        scores_file_version_patch = saved_patch;
        scores_file_version_extra = saved_extra;
        scores_file_entry_count = saved_entry_count;
        return false;
    }

    /* Iterate alive entries */
    for (int i = 0; i < n_recs; ++i) {
        if (highscore_seek(i)) break;
        high_score entry;
        if (highscore_read(&entry)) break; /* EOF */
        if (strcmp(entry.how, "(alive and well)") != 0) continue;

        char who_buf[sizeof entry.who + 1];
        memset(who_buf, 0, sizeof who_buf);
        SDL_strlcpy(who_buf, entry.who, sizeof(who_buf));
        /* Trim trailing spaces */
        for (int t = (int)strlen(who_buf) - 1; t >= 0; --t) {
            if (who_buf[t] == ' ' || who_buf[t] == '\t') who_buf[t] = '\0'; else break;
        }
        if (!who_buf[0]) {
            log_warn("autoload: alive entry at index %d has empty name, skipping", i);
            continue;
        }
        log_info("autoload: found alive entry '%s' (index %d) - attempting load", who_buf, i);

        SDL_strlcpy(op_ptr->full_name, who_buf, sizeof(op_ptr->full_name));
        process_player_name(true);

        log_info("autoload: savefile path generated: '%s'", savefile);
        if (load_player()) {
            log_info("autoload: successfully loaded '%s' (normalized)", who_buf);
            SDL_CloseIO(highscore_fd);
            highscore_fd = saved_fd;
            scores_file_version_major = saved_major;
            scores_file_version_minor = saved_minor;
            scores_file_version_patch = saved_patch;
            scores_file_version_extra = saved_extra;
            scores_file_entry_count = saved_entry_count;
            return true;
        }

        /* Legacy spaced filename attempt */
        char savefile_backup[1024];
        char alt_temp[128];
        char alt_path[1024];
        SDL_strlcpy(savefile_backup, savefile, sizeof(savefile_backup));
        strnfmt(alt_temp, sizeof(alt_temp), "%s", who_buf);
        path_build(alt_path, sizeof(alt_path), ANGBAND_DIR_SAVE, alt_temp);
        SDL_strlcpy(savefile, alt_path, sizeof(savefile));
        log_info("autoload: retrying with legacy spaced filename '%s'", savefile);
        if (load_player()) {
            log_info("autoload: successfully loaded '%s' (legacy spaced)", who_buf);
            /* Restore canonical name */
            SDL_strlcpy(op_ptr->full_name, who_buf, sizeof(op_ptr->full_name));
            process_player_name(true);
            SDL_CloseIO(highscore_fd);
            highscore_fd = saved_fd;
            scores_file_version_major = saved_major;
            scores_file_version_minor = saved_minor;
            scores_file_version_patch = saved_patch;
            scores_file_version_extra = saved_extra;
            scores_file_entry_count = saved_entry_count;
            SDL_strlcpy(savefile, savefile_backup, sizeof(savefile));
            return true;
        }
        SDL_strlcpy(savefile, savefile_backup, sizeof(savefile));

        /* Mark as dead and continue */
#if ANTICHEAT
        log_warn("autoload: savefile missing/corrupt for '%s' - marking dead", who_buf);
        strnfmt(entry.how, sizeof entry.how, "%-.49s", "their own hand");
        if (highscore_seek(i) == 0) {
            highscore_write(&entry);
        }
        metarun_increment_deaths();
        (void)save_metaruns();
        msg_format("Warning: Alive entry '%s' had no valid savefile. Marked as dead.", who_buf);
        msg_print("Please do not tamper with savefiles.");
        message_flush();
#else
        log_warn("autoload: savefile missing/corrupt for '%s' - skipping (ANTICHEAT disabled)", who_buf);
        /* Continue to next entry without marking as dead */
#endif
    }

    SDL_CloseIO(highscore_fd);
    highscore_fd = saved_fd;
    scores_file_version_major = saved_major;
    scores_file_version_minor = saved_minor;
    scores_file_version_patch = saved_patch;
    scores_file_version_extra = saved_extra;
    scores_file_entry_count = saved_entry_count;
    return false;
}

/*
 * Delete the current high-score file and immediately recreate an empty
 * placeholder so subsequent sdl_fopen() calls succeed without special cases.
 */
void clear_scorefile(void)
{
    char cur_path[1024];
    bool was_open = (highscore_fd != NULL);

    /* Full path to "scores.raw" */
    build_meta_path(cur_path, sizeof(cur_path), "scores.raw");

    /* Close existing descriptor if open */
    if (was_open) {
        SDL_CloseIO(highscore_fd);
        highscore_fd = NULL;
    }

    /* If the file exists and is non-empty, archive it with timestamp */
    {
        /* Peek size */
        safe_setuid_grab();
        int fd_probe = open(cur_path, O_RDONLY);
        off_t sz = -1;
        if (fd_probe >= 0) {
            sz = lseek(fd_probe, 0, SEEK_END);
            close(fd_probe);
        }
        safe_setuid_drop();

        if (sz > 0) {
            /* Build archive filename: scores-YYYYMMDD-HHMMSS-<run>.raw */
            time_t now = time(NULL);
            struct tm *lt = localtime(&now);
            char stamp[32];
            if (lt) strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", lt);
            else SDL_strlcpy(stamp, "unknown", sizeof stamp);

            /* Include run id if available (metar declared in metarun.h) */
            extern metarun metar; /* declared in metarun.h */
            char arch_leaf[128];
            strnfmt(arch_leaf, sizeof arch_leaf, "scores-%s-%08u.raw",
                    stamp, (unsigned)metar.id);

            char arch_path[1024];
            path_build(arch_path, sizeof arch_path, ANGBAND_DIR_APEX, arch_leaf);

            /* Try to rename; if it fails, fall back to delete */
            safe_setuid_grab();
            int rn = rename(cur_path, arch_path);
            safe_setuid_drop();
            if (rn != 0) {
                (void)fd_kill(cur_path); /* fallback */
            }
        }
        else {
            /* Nothing useful to archive; just remove it */
            (void)fd_kill(cur_path);
        }
    }

    /* Re-create a zero-length file properly */
    safe_setuid_grab();
    SDL_IOStream* fd_new = sdl_fmake(cur_path, 0644);
    if (fd_new) sdl_fclose(fd_new);
    safe_setuid_drop();

    /* If the file was previously open, reopen it for read/write */
    if (was_open) {
        safe_setuid_grab();
        highscore_fd = score_file_open(cur_path, O_RDWR);
        safe_setuid_drop();
    }
}

/*
 * Metarun finalizer: iterate all "alive" entries in scores.raw.
 * For each entry, attempt to load the savefile by name; if load succeeds,
 * flag the character as dead by their own hand and save back. In either
 * case, patch the score entry's how field to "their own hand".
 */
void metarun_finalize_scores_and_saves(void)
{
    log_info("finalize: entry (wizard=%d, noscore=0x%04X, savefile='%s')",
             p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
             p_ptr ? (unsigned)p_ptr->noscore : 0,
             savefile);
    char score_path[1024];
    build_meta_path(score_path, sizeof(score_path), "scores.raw");

    /* Open for read/write so we can patch entries */
    int fd_local;
    safe_setuid_grab();
    fd_local = open(score_path, O_RDWR | O_CREAT, 0644);
    safe_setuid_drop();
    if (fd_local < 0) {
    log_warn("finalize: could not open scorefile: %s", score_path);
        return;
    }

    off_t file_end = lseek(fd_local, 0, SEEK_END);
    off_t payload2 = file_end - (off_t)sizeof(score_file_header);  /* All scores files are versioned */
    int n_recs = (int)(payload2 / (off_t)sizeof(high_score));
    if (n_recs <= 0) {
        safe_setuid_grab();
        close(fd_local);
        safe_setuid_drop();
        return;
    }

    int patched = 0;
    for (int i = 0; i < n_recs; i++) {
        high_score entry;
        if (lseek(fd_local, (off_t)i * (off_t)sizeof entry, SEEK_SET) < 0)
            break;
        ssize_t got = read(fd_local, &entry, sizeof entry);
        if (got != sizeof entry) break;

        /* Only touch entries marked as alive */
        if (strcmp(entry.how, "(alive and well)") != 0) continue;

        /* Patch score entry regardless of save success */
        strnfmt(entry.how, sizeof entry.how, "%-.49s", "their own hand");
        if (lseek(fd_local, (off_t)i * (off_t)sizeof entry, SEEK_SET) >= 0) {
            (void)write(fd_local, &entry, sizeof entry);
        }
        patched++;
    }

    safe_setuid_grab();
    close(fd_local);
    safe_setuid_drop();
    log_info("finalize: patched %d alive entries to 'their own hand'", patched);

    /*
     * If the current character is a noscore wizard/debug run, purge their
     * savefile entirely as part of metarun cleanup, so it can't be resumed.
     *
     * Harmonized with start_new_metarun(): allow either wizard OR debug
     * (0x0008) in combination with any noscore bit (0x000F).
     */
    if (p_ptr && (p_ptr->wizard || (p_ptr->noscore & 0x0008)) && (p_ptr->noscore & 0x000F)) {
        if (savefile[0]) {
            int rc;
            safe_setuid_grab();
            rc = fd_kill(savefile);
            safe_setuid_drop();
            if (rc == 0) {
                log_info("finalize: deleted noscore wizard/debug savefile '%s'", savefile);
            } else {
                log_warn("finalize: failed to delete noscore wizard/debug savefile '%s'", savefile);
            }
        }
    } else {
        log_info("finalize: no direct purge in finalize (wizard=%d, noscore=0x%04X)",
                 p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
                 p_ptr ? (unsigned)p_ptr->noscore : 0);
    }
}

/*
 * Backup all save files to a timestamped ZIP archive and delete originals
 * Called when starting a new metarun to preserve old saves
 */
void backup_and_clear_saves(void)
{
    char save_dir[1024];
    
    /* Use the correct save directory - ANGBAND_DIR_SAVE points to lib/save */
    strnfmt(save_dir, sizeof(save_dir), "%s", ANGBAND_DIR_SAVE);
    
    log_info("Checking for save files to backup in: %s", save_dir);
    
    /* Fast check: Try to open a few common save file patterns to see if anything exists */
    bool has_files = false;
    char test_patterns[][32] = {"*.sav", "*.dat", "*.txt", "character.sav", "save.dat", "Feanor", "player"};
    
    for (int i = 0; i < 7 && !has_files; i++) {
        char test_path[1024];
        path_build(test_path, sizeof(test_path), save_dir, test_patterns[i]);
        
        log_trace("Checking for save file pattern: %s", test_path);
        
        /* Quick test using sdl_fopen - much faster than popen */
        SDL_IOStream* test_fd = sdl_fopen(test_path, "rb");
        if (test_fd) {
            sdl_fclose(test_fd);
            has_files = true;
            log_trace("Found save file: %s", test_path);
            break;
        } else {
            log_trace("File not found: %s", test_path);
        }
    }
    
    /* Also try to detect ANY file in the directory using a directory listing approach */
    if (!has_files) {
        log_trace("No specific patterns found, checking directory contents...");
        
        /* Try some common character names and generic file patterns */
        char common_patterns[][32] = {"save", "char", "game", "*"};
        
        for (int i = 0; i < 4 && !has_files; i++) {
            char test_path[1024];
            path_build(test_path, sizeof(test_path), save_dir, common_patterns[i]);
            
            log_trace("Checking directory pattern: %s", test_path);
            
            SDL_IOStream* test_fd = sdl_fopen(test_path, "rb");
            if (test_fd) {
                sdl_fclose(test_fd);
                has_files = true;
                log_trace("Found file with pattern: %s", test_path);
                break;
            }
        }
    }
    
    /* Super fast exit if no save files exist */
    if (!has_files) {
        log_info("No save files found - skipping backup/clear process");
        log_trace("Backup skipped because no save files were detected");
        return;  /* Exit immediately, no UI messages needed */
    }
    
    /* Create timestamped backup folder */
    char backup_folder[1024];
    char timestamp[64];
    time_t now;
    struct tm *timeinfo;
    
    /* Display progress message to user */
    prt("[Creating save file backup folder...]", 0, 0);
    Term_fresh();
    
    log_info("Found save files to backup and clear");
    log_trace("Starting folder-based backup process for save files");
    
    /* Get current timestamp for backup folder name */
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
    
    /* Create backup folder with timestamp */
    path_build(backup_folder, sizeof(backup_folder), save_dir, format("saves_metarun_%s", timestamp));
    
    log_info("Creating backup folder: %s", backup_folder);
    log_trace("Full backup folder path: %s", backup_folder);
    
    /* Create the backup directory */
    #ifdef WINDOWS
    if (_mkdir(backup_folder) != 0) {
        log_warn("Failed to create backup folder: %s", backup_folder);
        return;
    }
    #else
    if (mkdir(backup_folder, 0755) != 0) {
        log_warn("Failed to create backup folder: %s", backup_folder);
        return;
    }
    #endif
    
    /* Move ALL files to backup folder (except .gitignore and existing backup folders) */
    int files_moved = 0;
    
    
    #ifdef WINDOWS
    /* Windows: Use FindFirstFile/FindNextFile for directory scanning */
    WIN32_FIND_DATA findData;
    char search_path[1024];
    path_build(search_path, sizeof(search_path), save_dir, "*");
    
    HANDLE hFind = FindFirstFile(search_path, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            /* Skip directories and special entries */
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            
            char* filename = findData.cFileName;
            
            /* Skip .gitignore and backup folders */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, "saves_metarun_")) continue; /* Skip existing backup folders */
            
            /* Move this file to backup folder */
            char old_path[1024], new_path[1024];
            path_build(old_path, sizeof(old_path), save_dir, filename);
            path_build(new_path, sizeof(new_path), backup_folder, filename);
            
            /* Use rename() to move the file (atomic operation) */
            if (rename(old_path, new_path) == 0) {
                files_moved++;
                log_trace("Moved file to backup: %s", filename);
            } else {
                log_trace("Failed to move file: %s", filename);
            }
            
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
    #else
    /* Unix/Linux/macOS: Use POSIX opendir/readdir */
    DIR *dir = opendir(save_dir);
    if (dir) {
        struct dirent *entry;
        
        while ((entry = readdir(dir)) != NULL) {
            /* Skip directories and special entries */
            if (entry->d_type == DT_DIR) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            char* filename = entry->d_name;
            
            /* Skip .gitignore and backup folders */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, "saves_metarun_")) continue; /* Skip existing backup folders */
            
            /* Move this file to backup folder */
            char old_path[1024], new_path[1024];
            path_build(old_path, sizeof(old_path), save_dir, filename);
            path_build(new_path, sizeof(new_path), backup_folder, filename);
            
            /* Use rename() to move the file (atomic operation) */
            if (rename(old_path, new_path) == 0) {
                files_moved++;
                log_trace("Moved file to backup: %s", filename);
            } else {
                log_trace("Failed to move file: %s", filename);
            }
        }
        closedir(dir);
    }
    #endif
    
    if (files_moved > 0) {
        log_info("Save backup completed successfully: %s (%d files moved)", backup_folder, files_moved);
        prt("[Save files moved to backup folder]", 0, 0);
        Term_fresh();
    } else {
        log_info("No files found to move to backup");
        /* Remove empty backup folder if no files were moved */
        #ifdef WINDOWS
        _rmdir(backup_folder);
        #else
        rmdir(backup_folder);
        #endif
    }
    
    log_trace("Folder-based backup process completed");
}

























