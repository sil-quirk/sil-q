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
errr parse_b_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    static int cur_t = 0;

    /* Current entry */
    static ability_type* b_ptr = NULL;

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
        b_ptr = (ability_type*)head->info_ptr + i;

        /* Store the name */
        if (!(b_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Start with the first of the tval indices */
        cur_t = 0;
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int skilltype, abilitynum, level;

        /* There better be a current k_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &skilltype, &abilitynum, &level))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        b_ptr->skilltype = skilltype;
        b_ptr->abilitynum = abilitynum;
        b_ptr->level = level;
    }

    /* Process 'P' for "Prerequisites" (one line only) */
    else if (buf[0] == 'P')
    {
        int i;

        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            b_ptr->prereq_abilitynum[i] = 0;

            /* Store the skilltype */
            b_ptr->prereq_skilltype[i] = atoi(s + 1);

            /* List this prerequisite */
            b_ptr->prereqs++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int prereq_abilitynum = atoi(t + 1);
                if (prereq_abilitynum > 0)
                    b_ptr->prereq_abilitynum[i] = prereq_abilitynum;
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current k_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(b_ptr->text), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'E' for "Effect" (mechanical description) */
    else if (buf[0] == 'E')
    {
        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the effect text */
        if (!add_text(&(b_ptr->effect), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Types allowed" (up to five lines) */
    else if (buf[0] == 'T')
    {
        int tval, sval1, sval2;

        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval1, &sval2))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        b_ptr->tval[cur_t] = (byte)tval;
        b_ptr->min_sval[cur_t] = (byte)sval1;
        b_ptr->max_sval[cur_t] = (byte)sval2;

        /* Increase counter for 'possible tval' index */
        cur_t++;

        /* Allow only a limited number of T: lines */
        if (cur_t > ABILITY_TVALS_MAX)
            return (PARSE_ERROR_GENERIC);
    }

    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

/*
 * Grab one flag in an artefact_type from a textual string
 */

#endif /* ALLOW_TEMPLATES */