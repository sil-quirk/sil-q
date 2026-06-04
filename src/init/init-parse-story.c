#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "h-define.h"
#include "init.h"
#include "log/log.h"
#include "metarun.h"
#include "score/score_guid.h"
#include "init-parse-internal.h"
#include "init-object-bonuses.h"
#include <ctype.h>

#ifdef ALLOW_TEMPLATES
errr parse_st_info(char* buf, header* head)
{
    int i;

    char* s;

    /* Current entry */
    static story_type* st_ptr = NULL;

    /* Process 'N' for "New/Number" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        st_ptr = (story_type*)head->info_ptr + i;
        memset(st_ptr, 0, sizeof(story_type));

        /* Store the name */
        if (!(st_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Sensible defaults */
        st_ptr->st_type  = 0;
        st_ptr->order    = 0;
        st_ptr->runtypes = 0;   /* 0 == ALL runtypes */
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current st_ptr */
        if (!st_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&st_ptr->text, head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }
    /* Process 'T' for type (byte) */
    else if (buf[0] == 'T')
    {
        if (!st_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        int t;
        if (1 != sscanf(buf + 2, "%d", &t)) return (PARSE_ERROR_GENERIC);
        if (t < 0 || t > 255)               return (PARSE_ERROR_OUT_OF_BOUNDS);
        st_ptr->st_type = (byte)t;
    }
    /* Process 'O' for order (byte) */
    else if (buf[0] == 'O')
    {
        if (!st_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        int o;
        if (1 != sscanf(buf + 2, "%d", &o)) return (PARSE_ERROR_GENERIC);
        if (o < 0 || o > 255)               return (PARSE_ERROR_OUT_OF_BOUNDS);
        st_ptr->order = (byte)o;
    }
    /* Process 'R' for runtypes: "*" or "i|j|k" */
    else if (buf[0] == 'R')
    {
        if (!st_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        s = buf + 2;
        while (*s == ' ' || *s == '\t') s++;

        /* "*" => all runtypes (store 0 to mean ALL) */
        if (*s == '*')
        {
            st_ptr->runtypes = 0;  /* wildcard */
        }
        else
        {
            u32b mask = 0;
            char *tok = strtok(s, "|");
            while (tok)
            {
                int bit = atoi(tok);
                if (bit < 0 || bit >= 32)
                {
                    /* silently ignore out-of-range bits */
                }
                else
                {
                    mask |= (1UL << bit);
                }
                tok = strtok(NULL, "|");
            }
            st_ptr->runtypes = mask;
        }
    }
    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

/**********************************************************************
 * Initialise the "cu_info" array by parsing curses.txt
 **********************************************************************/

#endif /* ALLOW_TEMPLATES */