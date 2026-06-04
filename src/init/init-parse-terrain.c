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
errr parse_f_info(char* buf, header* head)
{
    int i;

    char* s;

    /* Current entry */
    static feature_type* f_ptr = NULL;

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
        f_ptr = (feature_type*)head->info_ptr + i;

        /* Store the name */
        if (!(f_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Default "mimic" */
        f_ptr->mimic = i;
    }

    /* Process 'M' for "Mimic" (one line only) */
    else if (buf[0] == 'M')
    {
        int mimic;

        /* There better be a current f_ptr */
        if (!f_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (1 != sscanf(buf + 2, "%d", &mimic))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        f_ptr->mimic = mimic;
    }

    /* Process 'G' for "Graphics" (one line only) */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current f_ptr */
        if (!f_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);
        if (!buf[3])
            return (PARSE_ERROR_GENERIC);
        if (!buf[4])
            return (PARSE_ERROR_GENERIC);

        /* Extract d_char */
        d_char = buf[2];

        /* If we have a longer string than expected ... */
        if (buf[5])
        {
            /* Advance "buf" on by 4 */
            buf += 4;

            /* Extract the colour */
            d_attr = color_text_to_attr(buf);
        }
        else
        {
            /* Extract the attr */
            d_attr = color_char_to_attr(buf[4]);
        }

        /* Paranoia */
        if (d_attr < 0)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        f_ptr->d_attr = d_attr;
        f_ptr->d_char = d_char;
    }

    /* Process 'T' for "Tile" graphics (one line only) */
    else if (buf[0] == 'T')
    {
        /* There better be a current f_ptr */
        if (!f_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse and set tile coordinates */
        return parse_tile_line(buf, &f_ptr->x_attr, &f_ptr->x_char);
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