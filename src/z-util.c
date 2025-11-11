/* File: z-util.c */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

/* Purpose: Low level utilities -BEN- */

#include "z-util.h"

/*
 * Convenient storage of the program name
 */
cptr argv0 = NULL;

/*
 * The SDL_strlcpy() function copies up to 'bufsize'-1 characters from 'src'
 * to 'buf' and NUL-terminates the result.  The 'buf' and 'src' strings may
 * not overlap.
 *
 * SDL_strlcpy() returns strlen(src).  This makes checking for truncation
 * easy.  Example: if (SDL_strlcpy(buf, src, sizeof(buf)) >= sizeof(buf)) ...;
 *
 * This function should be equivalent to the strlcpy() function in BSD.
 */
size_t SDL_strlcpy(char* buf, const char* src, size_t bufsize)
{
    size_t len = strlen(src);
    size_t ret = len;

    /* Paranoia */
    if (bufsize == 0)
        return ret;

    /* Truncate */
    if (len >= bufsize)
        len = bufsize - 1;

    /* Copy the string and terminate it */
    (void)memcpy(buf, src, len);
    buf[len] = '\0';

    /* Return strlen(src) */
    return ret;
}

/*
 * The SDL_strlcat() tries to append a string to an existing NUL-terminated
 * string. It never writes more characters into the buffer than indicated by
 * 'bufsize' and NUL-terminates the buffer.  The 'buf' and 'src' strings may not
 * overlap.
 *
 * SDL_strlcat() returns strlen(buf) + strlen(src).  This makes checking for
 * truncation easy.  Example:
 * if (SDL_strlcat(buf, src, sizeof(buf)) >= sizeof(buf)) ...;
 *
 * This function should be equivalent to the strlcat() function in BSD.
 */
size_t SDL_strlcat(char* buf, const char* src, size_t bufsize)
{
    size_t dlen = strlen(buf);

    /* Is there room left in the buffer? */
    if (dlen < bufsize - 1)
    {
        /* Append as much as possible  */
        return (dlen + SDL_strlcpy(buf + dlen, src, bufsize - dlen));
    }
    else
    {
        /* Return without appending */
        return (dlen + strlen(src));
    }
}

/*
 * Redefinable "plog" action
 */
void (*plog_aux)(cptr) = NULL;

/*
 * Print (or log) a "warning" message (ala "perror()")
 * Note the use of the (optional) "plog_aux" hook.
 */
void plog(cptr str)
{
    /* Use the "alternative" function if possible */
    if (plog_aux)
        (*plog_aux)(str);

    /* Just do a labeled fprintf to stderr */
    else
        (void)(fprintf(stderr, "%s: %s\n", argv0 ? argv0 : "?", str));
}

/*
 * Redefinable "quit" action
 */
void (*quit_aux)(cptr) = NULL;

/*
 * Exit (ala "exit()").  If 'str' is NULL, do "exit(0)".
 * If 'str' begins with "+" or "-", do "exit(atoi(str))".
 * Otherwise, plog() 'str' and exit with an error code of -1.
 * But always use 'quit_aux', if set, before anything else.
 */
void quit(cptr str)
{
    /* Attempt to use the aux function */
    if (quit_aux)
        (*quit_aux)(str);

    /* Success */
    if (!str)
        (void)(exit(0));

    /* Extract a "special error code" */
    if ((str[0] == '-') || (str[0] == '+'))
        (void)(exit(atoi(str)));

    /* Send the string to plog() */
    plog(str);

    /* Failure */
    (void)(exit(EXIT_FAILURE));
}

/*
 * Redefinable "core" action
 */
void (*core_aux)(cptr) = NULL;

/*
 * Dump a core file, after printing a warning message
 * As with "quit()", try to use the "core_aux()" hook first.
 */
void core(cptr str)
{
    char* crash = NULL;

    /* Use the aux function */
    if (core_aux)
        (*core_aux)(str);

    /* Dump the warning string */
    if (str)
        plog(str);

    /* Attempt to Crash */
    (*crash) = (*crash);

    /* Be sure we exited */
    quit("core() failed");
}

