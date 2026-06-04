/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#ifndef WINDOWS
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#endif

#include "angband.h"
#include "fs/file.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "externs.h"
#include <ctype.h>
#include <time.h>

#ifndef WINDOWS
#include <unistd.h>
#endif
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

