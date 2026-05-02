/* File: format.c */

#include "format.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/*
 * Lower-level formatter that supports Angband-specific extensions like "%^"
 * (capitalise first non-space) and "*" (variable precision).
 */
size_t vstrnfmt(char* buf, size_t max, cptr fmt, va_list vp)
{
    size_t n = 0;
    cptr s = fmt;

    char aux[128];
    char tmp[1024];

    while (true)
    {
        /* All done */
        if (!*s || (n == max - 1))
        {
            buf[n] = '\0';
            return n;
        }

        /* Normal characters */
        if (*s != '%')
        {
            buf[n++] = *s++;
            continue;
        }

        /* Reset format builder */
        size_t q = 0;
        bool do_long = false;
        bool do_xtra = false;
        aux[q++] = '%';
        s++;

        while (true)
        {
            if (!*s || q > 100)
            {
                buf[0] = '\0';
                return 0;
            }

            if (isalpha((unsigned char)*s))
            {
                if (*s == 'l')
                {
                    aux[q++] = *s++;
                    do_long = true;
                }
                else if (*s == 'L')
                {
                    buf[0] = '\0';
                    return 0;
                }
                else
                {
                    aux[q++] = *s++;
                    break;
                }
            }
            else if (*s == '*')
            {
                int arg = va_arg(vp, int);
                sprintf(aux + q, "%d", arg);
                while (aux[q])
                    q++;
                s++;
            }
            else if (*s == '^')
            {
                do_xtra = true;
                s++;
            }
            else if (*s == '%')
            {
                aux[q++] = *s++;
                break;
            }
            else
            {
                aux[q++] = *s++;
            }
        }

        aux[q] = '\0';
        tmp[0] = '\0';

        switch (aux[q - 1])
        {
        case '%':
            tmp[0] = '%';
            tmp[1] = '\0';
            break;

        case 'n':
        {
            size_t* arg = va_arg(vp, size_t*);
            *arg = n;
            break;
        }

        case 'p':
            (void)sprintf(tmp, aux, va_arg(vp, void*));
            break;

        case 'c':
            (void)sprintf(tmp, aux, va_arg(vp, int));
            break;

        case 's':
        case 'r':
        case 'v':
        {
            cptr arg = va_arg(vp, cptr);
            if (!arg)
                arg = "";
            if (aux[q - 1] != 's')
                aux[q - 1] = 's';
            (void)snprintf(tmp, sizeof(tmp), aux, arg);
            break;
        }

        case 'd':
        case 'i':
            if (do_long)
                sprintf(tmp, aux, va_arg(vp, long));
            else
                sprintf(tmp, aux, va_arg(vp, int));
            break;

        case 'u':
        case 'o':
        case 'x':
        case 'X':
            if (do_long)
                sprintf(tmp, aux, va_arg(vp, unsigned long));
            else
                sprintf(tmp, aux, va_arg(vp, unsigned int));
            break;

        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
            sprintf(tmp, aux, va_arg(vp, double));
            break;

        default:
            buf[0] = '\0';
            return 0;
        }

        if (do_xtra)
        {
            for (q = 0; tmp[q]; q++)
            {
                if (!isspace((unsigned char)tmp[q]))
                {
                    if (islower((unsigned char)tmp[q]))
                        tmp[q] = toupper((unsigned char)tmp[q]);
                    break;
                }
            }
        }

        for (q = 0; tmp[q]; q++)
        {
            if (n == max - 1)
                break;
            buf[n++] = tmp[q];
        }
    }
}

void strnfcat(char* str, size_t max, size_t* end, cptr fmt, ...)
{
    if (*end >= max)
        return;

    va_list vp;
    va_start(vp, fmt);
    size_t len = vstrnfmt(&str[*end], max - *end, fmt, vp);
    va_end(vp);

    *end += len;
}

size_t strnfmt(char* buf, size_t max, cptr fmt, ...)
{
    va_list vp;
    va_start(vp, fmt);
    size_t len = vstrnfmt(buf, max, fmt, vp);
    va_end(vp);
    return len;
}

/*
 * Format into a static buffer (for compatibility with old code)
 * 
 * WARNING: This uses a static buffer and is NOT thread-safe.
 * The buffer will be overwritten on the next call to this function.
 */
char* format(cptr fmt, ...)
{
    static char buf[2048];  /* Large enough for most uses */
    va_list vp;
    
    va_start(vp, fmt);
    vstrnfmt(buf, sizeof(buf), fmt, vp);
    va_end(vp);
    
    return buf;
}
