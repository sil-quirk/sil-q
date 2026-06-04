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
errr parse_flavor_info(char* buf, header* head)
{
    int i;

    /* Current entry */
    static flavor_type* flavor_ptr;

    /* Process 'N' for "Number" */
    if (buf[0] == 'N')
    {
        int tval, sval;
        int result;

        /* Scan the value */
        result = sscanf(buf, "N:%d:%d:%d", &i, &tval, &sval);

        /* Either two or three values */
        if ((result != 2) && (result != 3))
            return (PARSE_ERROR_GENERIC);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        flavor_ptr = (flavor_type*)head->info_ptr + i;

        /* Save the tval */
        flavor_ptr->tval = (byte)tval;

        /* Save the sval */
        if (result == 2)
        {
            /* Megahack - unknown sval */
            flavor_ptr->sval = SV_UNKNOWN;
        }
        else
            flavor_ptr->sval = (byte)sval;
    }

    /* Process 'G' for "Graphics" */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current flavor_ptr */
        if (!flavor_ptr)
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
        flavor_ptr->d_attr = d_attr;
        flavor_ptr->d_char = d_char;
    }

    /* Process 'T' for "Tile" graphics (one line only) */
    else if (buf[0] == 'T')
    {
        /* There better be a current flavor_ptr */
        if (!flavor_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse and set tile coordinates */
        return parse_tile_line(buf, &flavor_ptr->x_attr, &flavor_ptr->x_char);
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current flavor_ptr */
        if (!flavor_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[1])
            return (PARSE_ERROR_GENERIC);
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);

        /* Store the text */
        if (!add_text(&flavor_ptr->text, head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
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