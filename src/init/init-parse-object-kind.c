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
static errr grab_one_kind_flag(object_kind* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[TR1] = &(ptr->flags1);
    f[TR2] = &(ptr->flags2);
    f[TR3] = &(ptr->flags3);
    f[TR4] = &(ptr->flags4);
    return grab_one_flag(f, "object", what);
}

errr parse_k_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static object_kind* k_ptr = NULL;

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
        k_ptr = (object_kind*)head->info_ptr + i;

        /* Store the name */
        if (!(k_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Reset per-stat/skill bonuses. */
        for (int si = 0; si < A_MAX; si++)
        {
            k_ptr->stat_bonus[si] = 0;
            k_ptr->stat_bonus_set[si] = false;
        }
        for (int sk = 0; sk < S_MAX; sk++)
        {
            k_ptr->skill_bonus[sk] = 0;
            k_ptr->skill_bonus_set[sk] = false;
        }
    }

    /* Process 'G' for "Graphics" (one line only) */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current k_ptr */
        if (!k_ptr)
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
        k_ptr->d_attr = d_attr;
        k_ptr->d_char = d_char;
    }

    /* Process 'T' for "Tile" graphics (one line only) */
    else if (buf[0] == 'T')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse and set tile coordinates */
        return parse_tile_line(buf, &k_ptr->x_attr, &k_ptr->x_char);
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int tval, sval, pval;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval, &pval))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        k_ptr->tval = tval;
        k_ptr->sval = sval;
        k_ptr->pval = pval;

        /* Default max pval = base pval (no variation unless R: overrides) */
        k_ptr->max_pval = pval;

        apply_default_pval_bonuses(k_ptr->flags1, k_ptr->pval,
            k_ptr->stat_bonus, k_ptr->stat_bonus_set,
            k_ptr->skill_bonus, k_ptr->skill_bonus_set);
    }

    /* Process 'M' for per-stat/skill bonus overrides (one per line) */
    else if (buf[0] == 'M')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = strchr(buf + 2, ':');
        if (!s)
            return (PARSE_ERROR_GENERIC);

        *s++ = '\0';
        cptr token = buf + 2;
        int value = atoi(s);

        if (!apply_obj_bonus_token(token, value,
                &k_ptr->flags1,
                k_ptr->stat_bonus, k_ptr->stat_bonus_set,
                k_ptr->skill_bonus, k_ptr->skill_bonus_set))
        {
            return (PARSE_ERROR_GENERIC);
        }

        apply_default_pval_bonuses(k_ptr->flags1, k_ptr->pval,
            k_ptr->stat_bonus, k_ptr->stat_bonus_set,
            k_ptr->skill_bonus, k_ptr->skill_bonus_set);
    }

    /* Process 'W' for "More Info" (one line only) */
    else if (buf[0] == 'W')
    {
        int level, extra, wgt;
        long cost;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%ld", &level, &extra, &wgt, &cost))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        k_ptr->level = level;
        k_ptr->extra = extra;
        k_ptr->weight = wgt;
        k_ptr->cost = cost;
    }

    /* Process 'A' for "Allocation" (one line only) */
    else if (buf[0] == 'A')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Reset explicit allocation count */
        k_ptr->alloc_count = 0;

        /* Read each number following a colon */
        for (s = buf + 1; s && (s[0] == ':') && s[1];)
        {
            /* Sanity check */
            if (k_ptr->alloc_count > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            int depth = atoi(s + 1);
            int rarity = 1;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            char* next = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!next || t < next))
                rarity = atoi(t + 1);

            if (rarity < 0)
                rarity = 0;

            /* Store legacy locale/chance for compatibility */
            k_ptr->locale[k_ptr->alloc_count] = (byte)depth;
            k_ptr->chance[k_ptr->alloc_count] = (byte)rarity;

            /* Store explicit allocation entries (supporting zero rarity) */
            k_ptr->alloc_depth[k_ptr->alloc_count] = (byte)depth;
            k_ptr->alloc_prob[k_ptr->alloc_count] = (byte)rarity;
            k_ptr->alloc_count++;

            /* Advance to next colon (if any) */
            s = next;
        }
    }

    /* Hack -- Process 'P' for "power" and such */
    else if (buf[0] == 'P')
    {
        int att, dd, ds, evn, pd, ps;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (6
            != sscanf(
                buf + 2, "%d:%dd%d:%d:%dd%d", &att, &dd, &ds, &evn, &pd, &ps))
            return (PARSE_ERROR_GENERIC);

        k_ptr->att = att;
        k_ptr->dd = dd;
        k_ptr->ds = ds;
        k_ptr->evn = evn;
        k_ptr->pd = pd;
        k_ptr->ps = ps;

        /* Default max values = base values (no variation unless R: overrides) */
        k_ptr->max_att = att;
        k_ptr->max_ds = ds;
        k_ptr->max_evn = evn;
        k_ptr->max_ps = ps;
    }

    /* Process 'R' for "Range" - smithing/drop maximums (one per line) */
    else if (buf[0] == 'R')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = strchr(buf + 2, ':');
        if (!s)
            return (PARSE_ERROR_GENERIC);

        *s++ = '\0';
        cptr stat_name = buf + 2;
        int value = atoi(s);

        if (streq(stat_name, "ATT"))
            k_ptr->max_att = (s16b)value;
        else if (streq(stat_name, "DS"))
            k_ptr->max_ds = (byte)value;
        else if (streq(stat_name, "EVN"))
            k_ptr->max_evn = (s16b)value;
        else if (streq(stat_name, "PS"))
            k_ptr->max_ps = (byte)value;
        else if (streq(stat_name, "PVAL"))
            k_ptr->max_pval = (s16b)value;
        else
            return (PARSE_ERROR_GENERIC);
    }

    /* Process 'X' for elemental shield block chance */
    else if (buf[0] == 'X')
    {
        int elemental_block;

        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (1 != sscanf(buf + 2, "%d", &elemental_block))
            return (PARSE_ERROR_GENERIC);

        if ((elemental_block < 0) || (elemental_block > 100))
            return (PARSE_ERROR_GENERIC);

        k_ptr->elemental_block = (byte)elemental_block;
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
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
            if (0 != grab_one_kind_flag(k_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }

        apply_default_pval_bonuses(k_ptr->flags1, k_ptr->pval,
            k_ptr->stat_bonus, k_ptr->stat_bonus_set,
            k_ptr->skill_bonus, k_ptr->skill_bonus_set);
    }

    /* Process 'B' for "aBilities" (one line only) */
    else if (buf[0] == 'B')
    {
        int i;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            k_ptr->abilitynum[i] = 0;

            /* Store the skilltype */
            k_ptr->skilltype[i] = atoi(s + 1);

            /* List this ability */
            k_ptr->abilities++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int abilitynum = atoi(t + 1);
                if (abilitynum > 0)
                    k_ptr->abilitynum[i] = abilitynum;
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(k_ptr->text), head, s))
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