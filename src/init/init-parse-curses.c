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
static errr grab_one_curse_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RHF] = &(cu_ptr->flags);   /* write into the new word we added */
    return grab_one_flag(f, "curse", what);
}

static errr grab_one_curse_unique_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[CUR] = &(cu_ptr->flags_u);   /* write into the new word we added */
    return grab_one_flag(f, "curse unique", what);
}

static errr grab_one_blessing_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RHF] = &(cu_ptr->blessing_flags);
    return grab_one_flag(f, "blessing", what);
}

static errr grab_one_blessing_unique_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[CUR] = &(cu_ptr->blessing_flags_u);
    return grab_one_flag(f, "blessing unique", what);
}

errr parse_cu_info(char *buf, header *head)
{
    int   i, j;
    char *s, *t;

    /* Current entry */
    static curse_type *cu_ptr = NULL;

    /* ------------------------------------------------------------ */
    /* N: idx : name                                                */
    /* ------------------------------------------------------------ */
    if (buf[0] == 'N')
    {
        /* Find name delimiter */
        s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        if (!*s) return PARSE_ERROR_GENERIC;

        i = atoi(buf + 2);
        if (i <= error_idx)          return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (i >= head->info_num)     return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = i;

        cu_ptr = ((curse_type *)head->info_ptr) + i;

        /* Reset fresh record */
        memset(cu_ptr, 0, sizeof(curse_type));       /* clears the record  */
                                                     /* flags included     */
        cu_ptr->weight = 1;      /* sensible defaults           */
        cu_ptr->max_stacks = 0;  /* 0 = unlimited               */
        cu_ptr->max_blessing_stacks = 0;

        if (!(cu_ptr->name = add_name(head, s)))     
            return PARSE_ERROR_OUT_OF_MEMORY;
        cu_ptr->blessing_name = 0;  /* NULL unless B: directive sets it */
    }

    /* ------------------------------------------------------------ */
    /* B: blessing name                                             */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'B')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!(cu_ptr->blessing_name = add_name(head, buf + 2)))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* C: stat adjustments  or  S: (old name kept for back-compat)  */
    /* ------------------------------------------------------------ */
    else if ((buf[0] == 'C') || (buf[0] == 'S'))
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        s = buf + 1;                     /* points to first ':' */
        for (j = 0; j < A_MAX; j++)
        {
            s = strchr(s, ':');
            if (!s) return PARSE_ERROR_GENERIC;
            *s++ = '\0';
            cu_ptr->cu_adj[j] = atoi(s);
        }
    }

    /* ------------------------------------------------------------ */
    /* F: list of RHF flags (MEL_PENALTY | SWORD_AFFINITY ...)        */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'F')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            /* token = [^ or |]*  */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_curse_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }
    /* ------------------------------------------------------------ */
    /* G: list of blessing RHF flags                                */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'G')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_blessing_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }
    /* ------------------------------------------------------------ */
    /* U: list of CUR flags        */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'U')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            /* token = [^ or |]*  */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_curse_unique_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }
    /* ------------------------------------------------------------ */
    /* Y: list of blessing CUR flags                                */
    /* ------------------------------------------------------------ */
    /* V: is reserved for version stamps in data files; do not use. */
    else if (buf[0] == 'Y')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_blessing_unique_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }

    /* ------------------------------------------------------------ */
    /* A: weight / curse_cap [/ blessing_cap]                       */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'A')
    {
        char *u;

        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        /* default is "1/0/0" so zero-initialised files still work   */
        cu_ptr->weight     = 1;
        cu_ptr->max_stacks = 0;
        cu_ptr->max_blessing_stacks = 0;

        s = buf + 2;
        t = strchr(s, '/');
        if (!t) return PARSE_ERROR_GENERIC;

        *t++ = '\0';
        u = strchr(t, '/');
        if (u) *u++ = '\0';

        cu_ptr->weight = (byte)atoi(s);
        cu_ptr->max_stacks = (byte)atoi(t);
        cu_ptr->max_blessing_stacks = u ? (byte)atoi(u) : cu_ptr->max_stacks;
    }


    /* ------------------------------------------------------------ */
    /* D: description line(s)                                       */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'D')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->text), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* E: blessing description line(s)                              */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'E')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->blessing_text), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* P: power/effect description                                   */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'P')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->power), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* H: blessing power/effect description                         */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'H')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->blessing_power), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* anything else is an error                                    */
    /* ------------------------------------------------------------ */
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return 0;
}

#endif /* ALLOW_TEMPLATES */