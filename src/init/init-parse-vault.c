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
static errr grab_one_vault_flag(vault_type* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[VLT] = &(ptr->flags);
    return grab_one_flag(f, "vault", what);
}

errr parse_v_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static vault_type* v_ptr = NULL;

    /* Process 'N' for "New/Number/Name" */
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
        v_ptr = (vault_type*)head->info_ptr + i;

        /* Initialize default values */
        v_ptr->color = 0; /* Default to depth color */
        v_ptr->message = 0;
        v_ptr->skeleton_hint = 0;
        v_ptr->style_count = 0;
        for (int j = 0; j < 16; ++j)
        {
            v_ptr->style_idx[j] = -1;
            v_ptr->style_weight[j] = 0;
        }

        /* Store the name */
        if (!(v_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'X' for "Extra info" (one line only) */
    else if (buf[0] == 'X')
    {
        int typ, depth, rarity, max_depth;
        int num_scanned;

        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Try to scan for 4 values (with max_depth) */
        num_scanned = sscanf(buf + 2, "%d:%d:%d:%d", &typ, &depth, &rarity, &max_depth);

        /* If that fails, try scanning for 3 values (backward compatibility) */
        if (num_scanned == 3)
        {
            max_depth = 0; /* 0 = no maximum depth limit */
        }
        else if (num_scanned != 4)
        {
            return (PARSE_ERROR_GENERIC);
        }

        /* Save the values */
        v_ptr->typ = typ;
        v_ptr->depth = depth;
        v_ptr->max_depth = max_depth;
        v_ptr->rarity = rarity;
        v_ptr->hgt = 0;
        v_ptr->wid = 0;
        /* Note: Don't reset color here - it may have been set by a C: line */
    }

    /* Process 'C' for "Color" (one line only) */
    else if (buf[0] == 'C')
    {
        int color;

        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the color value */
        if (1 != sscanf(buf + 2, "%d", &color))
            return (PARSE_ERROR_GENERIC);

        /* Verify color range (0-255) */
        if (color < 0 || color > 255)
            return (PARSE_ERROR_GENERIC);

        /* Save the color value */
        v_ptr->color = color;
    }

    /* Process 'S' for Styles: one or more pairs "sidx:weight" separated by spaces
     * sidx may be:
     *   <number>  => exact style index
     *   *          => the already generated level style (sentinel -1)
     *   $          => any random style suitable for this depth's floors (sentinel -2)
     */
    else if (buf[0] == 'S')
    {
        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        char* s = buf + 2;
        char* tok;
        while (*s)
        {
            while (*s == ' ') s++;
            if (!*s) break;
            tok = s;
            while (*s && *s != ' ') s++;
            if (*s) { *s = '\0'; s++; }
            /* parse tok as sidx:weight; '*' means sidx == -1 (level styles)
             * and '$' means sidx == -2 (any style available at this depth) */
            char* colon = strchr(tok, ':');
            if (!colon) return PARSE_ERROR_GENERIC;
            *colon = '\0';
            int sidx;
            if (tok[0] == '*' && tok[1] == '\0') sidx = -1;        /* level style */
            else if (tok[0] == '$' && tok[1] == '\0') sidx = -2;   /* any depth-available style */
            else sidx = atoi(tok);
            int w = atoi(colon + 1);
            if (v_ptr->style_count < 16 && sidx >= -2 && w > 0)
            {
                v_ptr->style_idx[v_ptr->style_count] = (s16b)sidx;
                v_ptr->style_weight[v_ptr->style_count] = (s16b)w;
                v_ptr->style_count++;
            }
        }
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current k_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry textually */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while (*t == ' ' || *t == '|')
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_vault_flag(v_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'M' for "Entry message" */
    else if (buf[0] == 'M')
    {
        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (!add_text(&v_ptr->message, head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'H' for a unique skeleton hint */
    else if (buf[0] == 'H')
    {
        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (!add_text(&v_ptr->skeleton_hint, head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        if (v_ptr->wid == 0)
        {
            v_ptr->wid = strlen(buf + 2);
        }
        else if (v_ptr->wid != strlen(buf + 2))
        {
            return (PARSE_ERROR_VAULT_NOT_RECTANGULAR);
        }

        /* Store the text */
        if (!add_text(&v_ptr->text, head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        // note if there is a forge in the vault
        if (strchr(buf, '0'))
            v_ptr->forge = true;

        // we've added another row of the vault
        v_ptr->hgt++;

        /* Check for maximum vault sizes */
        if ((v_ptr->typ == 6) && ((v_ptr->wid > 33) || (v_ptr->hgt > 22)))
            return (PARSE_ERROR_VAULT_TOO_BIG);

        if ((v_ptr->typ == 7) && ((v_ptr->wid > 33) || (v_ptr->hgt > 22)))
            return (PARSE_ERROR_VAULT_TOO_BIG);

        if ((v_ptr->typ == 8) && ((v_ptr->wid > 66) || (v_ptr->hgt > 44)))
            return (PARSE_ERROR_VAULT_TOO_BIG);
    }

    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

#endif /* ALLOW_TEMPLATES */
