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
errr parse_tile_line(const char* buf, byte* x_attr, char* x_char)
{
    int row, col;

    /* Must start with "T:" */
    if (buf[0] != 'T' || buf[1] != ':')
        return PARSE_ERROR_GENERIC;

    /* Parse row and column as decimal */
    if (2 != sscanf(buf + 2, "%d:%d", &row, &col))
        return PARSE_ERROR_GENERIC;

    /* Validate range (0-63 for 6-bit index) */
    if (row < 0 || row > 63)
        return PARSE_ERROR_GENERIC;
    if (col < 0 || col > 63)
        return PARSE_ERROR_GENERIC;

    /* Set values with TILE_FLAG (0x80) */
    *x_attr = (byte)(0x80 | row);
    *x_char = (char)(0x80 | col);

    return 0;
}

/*
 * Monster Blow Methods
 */

errr init_info_txt(
    SDL_IOStream* fp, char* buf, header* head, parse_info_txt_func parse_info_txt_line)
{
    errr err;

    /* Not ready yet */
    bool okay = false;

    /* Just before the first record */
    error_idx = -1;

    /* Just before the first line */
    error_line = 0;

    /* Prepare the "fake" stuff */
    head->name_size = 0;
    head->text_size = 0;

    /* Parse */
    while (0 == sdl_fgets(fp, buf, 1024))
    {
        /* Advance the line number */
        error_line++;

        /* Skip comments and blank lines */
        if (!buf[0] || (buf[0] == '#'))
            continue;

        /* Verify correct directive format.
         * Most files use single-letter directives (X:), but some parsers
         * also accept numbered forms like M1:/M2:. */
        if ((buf[1] != ':') && (buf[2] != ':'))
            return (PARSE_ERROR_GENERIC);

        /* Hack -- Process 'V' for "Version" */
        if (buf[0] == 'V')
        {
            int v1, v2, v3;

            /* Scan for the values */
            if ((3 != sscanf(buf + 2, "%d.%d.%d", &v1, &v2, &v3))
                || (v1 != head->v_major) || (v2 != head->v_minor)
                || (v3 != head->v_patch))
            {
                return (PARSE_ERROR_OBSOLETE_FILE);
            }

            /* Okay to proceed */
            okay = true;

            /* Continue */
            continue;
        }

        /* No version yet */
        if (!okay)
            return (PARSE_ERROR_OBSOLETE_FILE);

        /* Parse the line */
        if ((err = (*parse_info_txt_line)(buf, head)) != 0)
            return (err);
    }

    /* Complete the "name" and "text" sizes */
    if (head->name_size)
        head->name_size++;
    if (head->text_size)
        head->text_size++;

    /* No version yet */
    if (!okay)
        return (PARSE_ERROR_OBSOLETE_FILE);

    /* Success */
    return (0);
}

bool add_text(u32b* offset, header* head, cptr buf)
{
    /* Hack -- Verify space */
    if (head->text_size + strlen(buf) + 8 > z_info->fake_text_size)
        return (false);

    /* New text? */
    if (*offset == 0)
    {
        /* Advance and save the text index */
        *offset = ++head->text_size;
    }

    /* Append chars to the text */
    strcpy(head->text_ptr + head->text_size, buf);

    /* Advance the index */
    head->text_size += strlen(buf);

    /* Ensure proper null termination */
    head->text_ptr[head->text_size] = '\0';

    /* Success */
    return (true);
}

/*
 * Add a name to the name-storage and return an offset to it.
 *
 * Returns 0 when there isn't enough space available to store
 * the name.
 */
u32b add_name(header* head, cptr buf)
{
    u32b index;

    /* Hack -- Verify space */
    if (head->name_size + strlen(buf) + 8 > z_info->fake_name_size)
        return (0);

    /* Advance and save the name index */
    index = ++head->name_size;

    /* Append chars to the names */
    strcpy(head->name_ptr + head->name_size, buf);

    /* Advance the index */
    head->name_size += strlen(buf);

    /* Return the name index */
    return (index);
}

#endif /* ALLOW_TEMPLATES */