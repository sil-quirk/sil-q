#include "angband.h"
#include "support/misc.h"
#include "externs.h"
#include "log/log.h"

bool no_light(void)
{
    /* Consider no special light blocking by default */
    return false;
}

/*
 * Parse a hexadecimal string (optional separators) into an unsigned 64-bit value.
 * Accepts optional "0x" prefix and ignores '-', '_' or whitespace separators.
 */
bool parse_u64b_hex(const char* text, u64b* out)
{
    if (!text || !out)
        return false;

    u64b value = 0;
    int digits = 0;

    while (*text)
    {
        char c = *text++;

        if (c == '-' || c == '_' || c == ' ')
            continue;

        if (digits == 0 && c == '0' && (*text == 'x' || *text == 'X'))
        {
            text++;
            continue;
        }

        int nibble;
        if (c >= '0' && c <= '9')
            nibble = c - '0';
        else if (c >= 'a' && c <= 'f')
            nibble = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F')
            nibble = 10 + (c - 'A');
        else
            return false;

        if (digits >= 16)
            return false;

        value = (value << 4) | (u64b)nibble;
        digits++;
    }

    if (digits == 0)
        return false;

    *out = value;
    return true;
}
#ifdef SET_UID

#ifndef HAVE_USLEEP

/*
 * For those systems that don't have "usleep()" but need it.
 *
 * Fake "usleep()" function grabbed from the inl netrek server -cba
 */
int usleep(unsigned long usecs)
{
    struct timeval Timer;

    int nfds = 0;

#ifdef FD_SET
    fd_set* no_fds = NULL;
#else
    int* no_fds = NULL;
#endif

    /* Paranoia -- No excessive sleeping */
    if (usecs > 4000000L)
        core("Illegal usleep() call");

    /* Wait for it */
    Timer.tv_sec = (usecs / 1000000L);
    Timer.tv_usec = (usecs % 1000000L);

    /* Wait for it */
    if (select(nfds, no_fds, no_fds, no_fds, &Timer) < 0)
    {
        /* Hack -- ignore interrupts */
        if (errno != EINTR)
            return -1;
    }

    /* Success */
    return 0;
}

#endif /* HAVE_USLEEP */

/*
 * Find a default user name from the system.
 */
void user_name(char* buf, size_t len, int id)
{
    struct passwd* pw = NULL;

    /* Look up the user name */
    if ((pw = getpwuid(id)))
    {
        /* Get the first 15 characters of the user name */
        SDL_strlcpy(buf, pw->pw_name, len);

#ifdef CAPITALIZE_USER_NAME
        /* Hack -- capitalize the user name */
        if (islower((unsigned char)buf[0]))
            buf[0] = toupper((unsigned char)buf[0]);
#endif /* CAPITALIZE_USER_NAME */

        return;
    }

    /* Oops.  Hack -- default to "nameless" */
    SDL_strlcpy(buf, "nameless", len);
}

#endif /* SET_UID */

#ifdef CHECK_MODIFICATION_TIME

/* SDL3-compatible modification time check */
errr check_modification_date_sdl(cptr raw_path, cptr txt_path)
{
    SDL_PathInfo txt_info, raw_info;
    
    /* Get info for text file */
    if (!SDL_GetPathInfo(txt_path, &txt_info))
    {
        /* No text file or error - continue with raw */
        log_debug("check_modification_date: Cannot get info for txt file '%s'", txt_path);
        return (0);
    }
    
    /* Get info for raw file */
    if (!SDL_GetPathInfo(raw_path, &raw_info))
    {
        /* No raw file - need to regenerate */
        log_info("check_modification_date: No raw file '%s' - regenerating", raw_path);
        return (-1);
    }
    
    /* Ensure text file is not newer than raw file */
    if (txt_info.modify_time > raw_info.modify_time)
    {
        /* Text file is newer - reprocess */
        log_info("check_modification_date: txt file newer (txt=%lld, raw=%lld) - regenerating '%s'", 
                 (long long)txt_info.modify_time, (long long)raw_info.modify_time, txt_path);
        return (-1);
    }
    
    log_info("check_modification_date: raw file is up to date (txt=%lld, raw=%lld) for '%s'",
              (long long)txt_info.modify_time, (long long)raw_info.modify_time, txt_path);
    return (0);
}

#endif /* CHECK_MODIFICATION_TIME */

int int_exp(int base, int power)
{
    int i;
    int result = 1;

    for (i = 0; i < power; i++)
    {
        result *= base;
    }

    return (result);
}

/*
 * Generates damage for "2d6" style dice rolls
 */
int damroll(int num, int sides)
{
    int i;
    int sum = 0;

    /* Dice with no sides always come up zero */
    if (sides <= 0)
        return (0);

    /* Roll the dice */
    for (i = 0; i < num; i++)
    {
        sum += dieroll(sides);
    }

    return (sum);
}

/*
 * Check a char for "vowel-hood"
 */
bool is_a_vowel(int ch)
{
    switch (ch)
    {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
    case 'A':
    case 'E':
    case 'I':
    case 'O':
    case 'U':
        return (true);
    }

    return (false);
}
