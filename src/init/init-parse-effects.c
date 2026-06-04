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
errr parse_effect_info(char* buf, header* head)
{
    int i;
    char* s;

    /* Current entry index */
    static int effect_idx = -1;
    effect_glyph* glyphs = (effect_glyph*)head->info_ptr;

    /* Process 'V' for "Version" */
    if (buf[0] == 'V')
    {
        return (0);
    }

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

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i < 0 || i >= 256)
            return (PARSE_ERROR_GENERIC);

        /* Save the index */
        effect_idx = i;
    }

    /* Process 'T' for "Tile" graphics */
    else if (buf[0] == 'T')
    {
        int row, col;

        /* Must have a valid effect index */
        if (effect_idx < 0)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse row and column */
        if (2 != sscanf(buf + 2, "%d:%d", &row, &col))
            return (PARSE_ERROR_GENERIC);

        /* Validate range (0-63 for 6-bit index) */
        if (row < 0 || row > 63)
            return (PARSE_ERROR_GENERIC);
        if (col < 0 || col > 63)
            return (PARSE_ERROR_GENERIC);

        if (!glyphs)
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Store in the raw-backed table (and update globals) */
        glyphs[effect_idx].a = (byte)(0x80 | row);
        glyphs[effect_idx].c = (byte)(0x80 | col);
        misc_to_attr[effect_idx] = glyphs[effect_idx].a;
        misc_to_char[effect_idx] = (char)glyphs[effect_idx].c;
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