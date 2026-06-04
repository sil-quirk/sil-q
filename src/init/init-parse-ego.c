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
static bool grab_one_ego_item_flag(ego_item_type* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[TR1] = &(ptr->flags1);
    f[TR2] = &(ptr->flags2);
    f[TR3] = &(ptr->flags3);
    f[TR4] = &(ptr->flags4);
    return grab_one_flag(f, "object", what);
}

/*
 * Initialize the "e_info" array, by parsing an ascii "template" file
 */
errr parse_e_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static ego_item_type* e_ptr = NULL;

    static int cur_t = 0;

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
        e_ptr = (ego_item_type*)head->info_ptr + i;

        /* Store the name */
        if (!(e_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Reset per-stat/skill bonus offsets. */
        for (int si = 0; si < A_MAX; si++)
        {
            e_ptr->stat_bonus_min[si] = 0;
            e_ptr->stat_bonus[si] = 0;
            e_ptr->stat_bonus_set[si] = false;
        }
        for (int sk = 0; sk < S_MAX; sk++)
        {
            e_ptr->skill_bonus_min[sk] = 0;
            e_ptr->skill_bonus[sk] = 0;
            e_ptr->skill_bonus_set[sk] = false;
        }

        /* Start with the first of the tval indices */
        cur_t = 0;

        /* Reset allocation tracking */
        e_ptr->alloc_count = 0;
    }

    /* Process 'W' for "More Info" (one line only) */
    else if (buf[0] == 'W')
    {
        int level, rarity, max_level;
        long cost;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4
            != sscanf(
                buf + 2, "%d:%d:%d:%ld", &level, &rarity, &max_level, &cost))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        e_ptr->level = level;
        e_ptr->rarity = rarity;
        e_ptr->max_level = max_level;
        e_ptr->cost = cost;
    }

    /* Process 'A' for "Allocation" (one line only) */
    else if (buf[0] == 'A')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Reset explicit allocation count */
        e_ptr->alloc_count = 0;

        for (s = buf + 1; s && (s[0] == ':') && s[1];)
        {
            if (e_ptr->alloc_count > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            int depth = atoi(s + 1);
            int rarity = 1;
            t = strchr(s + 1, '/');
            char* next = strchr(s + 1, ':');
            if (t && (!next || t < next))
                rarity = atoi(t + 1);
            if (rarity < 0)
                rarity = 0;

            e_ptr->alloc_depth[e_ptr->alloc_count] = (byte)depth;
            e_ptr->alloc_prob[e_ptr->alloc_count] = (byte)rarity;
            e_ptr->alloc_count++;

            s = next;
        }
    }

    /* Process 'T' for "Types allowed" (up to EGO_TVALS_MAX lines) */
    else if (buf[0] == 'T')
    {
        int tval, sval1, sval2;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval1, &sval2))
            return (PARSE_ERROR_GENERIC);

        /* Allow only a limited number of T: lines */
        if (cur_t >= EGO_TVALS_MAX)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        e_ptr->tval[cur_t] = (byte)tval;
        e_ptr->min_sval[cur_t] = (byte)sval1;
        e_ptr->max_sval[cur_t] = (byte)sval2;

        /* Increase counter for 'possible tval' index */
        cur_t++;
    }

    /* Hack -- Process 'C' for "creation" */
    else if (buf[0] == 'C')
    {
        int max_att, to_dd, to_ds, max_evn, to_pd, to_ps, pv;
        int min_pv = 0;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values (8th field min_pval is optional) */
        int fields = sscanf(buf + 2, "%d:%d:%d:%d:%d:%d:%d:%d", &max_att, &to_dd, &to_ds,
            &max_evn, &to_pd, &to_ps, &pv, &min_pv);
        if (fields < 7)
            return (PARSE_ERROR_GENERIC);

        e_ptr->max_att = max_att;
        e_ptr->to_dd = to_dd;
        e_ptr->to_ds = to_ds;
        e_ptr->max_evn = max_evn;
        e_ptr->to_pd = to_pd;
        e_ptr->to_ps = to_ps;
        e_ptr->max_pval = pv;
        e_ptr->min_pval = (byte)min_pv;

        /* If ego grants pval (max_pval > 0) but min_pval is 0, default to 1 */
        if (e_ptr->max_pval > 0 && e_ptr->min_pval == 0)
            e_ptr->min_pval = 1;
    }

    /* Process 'M' for per-stat/skill bonus ranges (one per line) */
    else if (buf[0] == 'M')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = strchr(buf + 2, ':');
        if (!s)
            return (PARSE_ERROR_GENERIC);

        *s++ = '\0';
        cptr token = buf + 2;
        int min_value = 0;
        int max_value = 0;

        if (!parse_bonus_value_range(s, &min_value, &max_value))
            return (PARSE_ERROR_GENERIC);

        if (!apply_ego_bonus_token_range(token, min_value, max_value,
                &e_ptr->flags1,
                e_ptr->stat_bonus_min, e_ptr->stat_bonus, e_ptr->stat_bonus_set,
                e_ptr->skill_bonus_min, e_ptr->skill_bonus, e_ptr->skill_bonus_set))
        {
            return (PARSE_ERROR_GENERIC);
        }
    }

    /* Process 'X' for elemental shield block chance */
    else if (buf[0] == 'X')
    {
        int elemental_block;

        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (1 != sscanf(buf + 2, "%d", &elemental_block))
            return (PARSE_ERROR_GENERIC);

        if ((elemental_block < 0) || (elemental_block > 100))
            return (PARSE_ERROR_GENERIC);

        e_ptr->elemental_block = (byte)elemental_block;
    }

    /* Process 'B' for "aBilities" (one line only) */
    else if (buf[0] == 'B')
    {
        int i;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            e_ptr->abilitynum[i] = 0;

            /* Store the skilltype */
            e_ptr->skilltype[i] = atoi(s + 1);

            /* List this ability */
            e_ptr->abilities++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int abilitynum = atoi(t + 1);
                if (abilitynum > 0)
                    e_ptr->abilitynum[i] = abilitynum;
            }
        }
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
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
                while ((*t == ' ') || (*t == '|'))
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_ego_item_flag(e_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&e_ptr->text, head, s))
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

/*
 * Grab one (basic) flag in a monster_race from a textual string
 */

#endif /* ALLOW_TEMPLATES */