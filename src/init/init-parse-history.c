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
errr parse_h_info(char* buf, header* head)
{
    int i;

    char* s;

    /* Current entry */
    static hist_type* h_ptr = NULL;

    /* Process 'N' for "New/Number" */
    if (buf[0] == 'N')
    {
        int prv, nxt, prc, hou;

        /* Hack - get the index */
        i = error_idx + 1;

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        h_ptr = (hist_type*)head->info_ptr + i;

        /* Scan for the values */
        if (4 != sscanf(buf, "N:%d:%d:%d:%d", &prv, &nxt, &prc, &hou))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        h_ptr->chart = prv;
        h_ptr->next = nxt;
        h_ptr->roll = prc;
        h_ptr->character = hou;
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current h_ptr */
        if (!h_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&h_ptr->text, head, s))
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