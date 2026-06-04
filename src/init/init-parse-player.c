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
static errr grab_one_race_flag(player_race* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RHF] = &(ptr->flags);
    return grab_one_flag(f, "player", what);
}

/*
 * Grab one flag in a character_profile from a textual string
 *
 * Sil:  these used to be the TR1, TR2 and TR3 flags,
 *       but we now use the race/character flags (RHF).
 */
static errr grab_one_character_flag(character_profile *ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));

    f[RHF] = &(ptr->flags);

    return grab_one_flag(f, "player character", what);
}

static errr grab_one_character_uflag(character_profile *ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));

    f[UNQ] = &(ptr->flags_u);      /* NEW: accept unique-flag word */

    return grab_one_flag(f, "player character", what);
}

/*
 * Initialize the "p_info" array, by parsing an ascii "template" file
 */
errr parse_p_info(char* buf, header* head)
{
    int i, j;

    char *s, *t;

    /* Current entry */
    static player_race* pr_ptr = NULL;
    static int cur_equip = 0;

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
        pr_ptr = (player_race*)head->info_ptr + i;

        /* Store the name */
        if (!(pr_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        cur_equip = 0;
    }

    /* Process 'Q' for stable GUID */
    else if (buf[0] == 'Q')
    {
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        u64b guid = 0;
        if (!parse_u64b_hex(buf + 2, &guid))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->guid = score_guid_from_u64(guid);
    }

    /* Process 'S' for "Stats" (one line only) */
    else if (buf[0] == 'S')
    {
        int adj;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Start the string */
        s = buf + 1;

        /* For each stat */
        for (j = 0; j < A_MAX; j++)
        {
            /* Find the colon before the subindex */
            s = strchr(s, ':');

            /* Verify that colon */
            if (!s)
                return (PARSE_ERROR_GENERIC);

            /* Nuke the colon, advance to the subindex */
            *s++ = '\0';

            /* Get the value */
            adj = atoi(s);

            /* Save the value */
            pr_ptr->r_adj[j] = adj;

            /* Next... */
            continue;
        }
    }

    /* Hack -- Process 'I' for "info" and such */
    else if (buf[0] == 'I')
    {
        int hist, b_age, m_age;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &hist, &b_age, &m_age))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->hist = hist;
        pr_ptr->b_age = b_age;
        pr_ptr->m_age = m_age;
    }

    /* Hack -- Process 'H' for "Height" */
    else if (buf[0] == 'H')
    {
        int b_ht, m_ht;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &b_ht, &m_ht))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->b_ht = b_ht;
        pr_ptr->m_ht = m_ht;
    }

    /* Hack -- Process 'W' for "Weight" */
    else if (buf[0] == 'W')
    {
        int b_wt, m_wt;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &b_wt, &m_wt))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->b_wt = b_wt;
        pr_ptr->m_wt = m_wt;
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current pr_ptr */
        if (!pr_ptr)
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
            if (0 != grab_one_race_flag(pr_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'E' for "Starting Equipment" */
    else if (buf[0] == 'E')
    {
        int tval, sval, min, max;

        start_item* e_ptr;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Access the item */
        e_ptr = &pr_ptr->start_items[cur_equip];

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &tval, &sval, &min, &max))
            return (PARSE_ERROR_GENERIC);

        if ((min < 0) || (max < 0) || (min > 99) || (max > 99))
            return (PARSE_ERROR_INVALID_ITEM_NUMBER);

        /* Save the values */
        e_ptr->tval = tval;
        e_ptr->sval = sval;
        e_ptr->min = min;
        e_ptr->max = max;

        /* Next item */
        cur_equip++;

        /* Limit number of starting items */
        if (cur_equip > MAX_START_ITEMS)
            return (PARSE_ERROR_GENERIC);
    }

    /* Hack -- Process 'C' for character choices */
    else if (buf[0] == 'C')
    {
        /* There better be a current pr_ptr */
        if (!pr_ptr)
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

            int bit = atoi(s);   // Converts the string (e.g. "42") to an int
            if (bit >= 0 && bit < FLAG_COUNT) {
                int word = bit / 32;        // Which 32-bit slot (0 or 1)
                int shift = bit % 32;       // Which bit in that slot (0-31)
                pr_ptr->choice[word] |= (1U << shift);  // Set the bit
            } else {
                // Invalid flag index
            }

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(pr_ptr->text), head, s))
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
 * Initialize the "c_info" array, by parsing an ascii "template" file
 */
// errr parse_c_info(char* buf, header* head)
// {
//     int i, j;
//     static int cur_equip = 0;

//     char *s, *t;

//     /* Current entry */
//     static character_profile* ph_ptr = NULL;

//     log_debug("Parsing characters");

//     /* Process 'N' for "New/Number/Name" */
//     if (buf[0] == 'N')
//     {
//         char *s;
//         int   idx, j;

//         /* Find the colon before the name */
//         s = strchr(buf + 2, ':');
//         if (!s) return (PARSE_ERROR_GENERIC);

//         /* Split and advance to the name text */
//         *s++ = '\0';
//         if (!*s) return (PARSE_ERROR_GENERIC);

//         /* Parse the index */
//         idx = atoi(buf + 2);
//         if (idx <= error_idx)            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
//         if (idx >= head->info_num)       return (PARSE_ERROR_TOO_MANY_ENTRIES);
//         error_idx = idx;

//         /* Point at this slot */
//         ph_ptr = (character_profile*)head->info_ptr + idx;

//         /* Store the name offset */
//         if (!(ph_ptr->name = add_name(head, s)))
//             return (PARSE_ERROR_OUT_OF_MEMORY);

//         /* Debug: announce new character and its name */
//         log_debug("New character #%d: \"%s\"", idx,
//                 head->name_ptr + ph_ptr->name);

//         /* Sentinel-initialize all ability slots to "empty" */
//         for (j = 0; j < CHARACTER_ABILITY_MAX; j++)
//         {
//             ph_ptr->a_adj[j][0] = -1;
//             ph_ptr->a_adj[j][1] = -1;
//         }
//         log_debug("  a_adj slots 0..%d set to -1", CHARACTER_ABILITY_MAX - 1);
//     }

//     /* Process 'A' for "Alternate Name" */
//     else if (buf[0] == 'A')
//     {
//         /* Find the colon before the name */
//         s = strchr(buf, ':');

//         /* Verify that colon */
//         if (!s)
//             return (PARSE_ERROR_GENERIC);

//         /* Nuke the colon, advance to the name */
//         *s++ = '\0';

//         /* Paranoia -- require a name */
//         if (!*s)
//             return (PARSE_ERROR_GENERIC);

//         /* Store the name */
//         if (!(ph_ptr->alt_name = add_name(head, s)))
//             return (PARSE_ERROR_OUT_OF_MEMORY);
//     }

//     /* Process 'B' for "Short Name" */
//     else if (buf[0] == 'B')
//     {
//         /* Find the colon before the name */
//         s = strchr(buf, ':');

//         /* Verify that colon */
//         if (!s)
//             return (PARSE_ERROR_GENERIC);

//         /* Nuke the colon, advance to the name */
//         *s++ = '\0';

//         /* Paranoia -- require a name */
//         if (!*s)
//             return (PARSE_ERROR_GENERIC);

//         /* Store the name */
//         if (!(ph_ptr->short_name = add_name(head, s)))
//             return (PARSE_ERROR_OUT_OF_MEMORY);
//     }

//     /* Process 'S' for "Stats" (one line only) */
//     else if (buf[0] == 'S')
//     {
//         int adj;

//         /* There better be a current ph_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Start the string */
//         s = buf + 1;

//         /* For each stat */
//         for (j = 0; j < A_MAX; j++)
//         {
//             /* Find the colon before the subindex */
//             s = strchr(s, ':');

//             /* Verify that colon */
//             if (!s)
//                 return (PARSE_ERROR_GENERIC);

//             /* Nuke the colon, advance to the subindex */
//             *s++ = '\0';

//             /* Get the value */
//             adj = atoi(s);

//             /* Save the value */
//             ph_ptr->h_adj[j] = adj;

//             /* Next... */
//             continue;
//         }
//     }

//     /* Hack -- Process 'F' for flags */
//     else if (buf[0] == 'F')
//     {
//         /* There better be a current pr_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Parse every entry textually */
//         for (s = buf + 2; *s;)
//         {
//             /* Find the end of this entry */
//             for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
//                 ;

//             /* Nuke and skip any dividers */
//             if (*t)
//             {
//                 *t++ = '\0';
//                 while ((*t == ' ') || (*t == '|'))
//                     t++;
//             }

//             /* Parse this entry */
//             if (0 != grab_one_character_flag(ph_ptr, s))
//                 return (PARSE_ERROR_INVALID_FLAG);

//             /* Start the next entry */
//             s = t;
//         }
//     }

//     /* ------------------------------------------------------------ */
//     /* U: list of Unique flags        */
//     /* ------------------------------------------------------------ */
//     else if (buf[0] == 'U')
//     {
//         if (!ph_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

//         for (s = buf + 2; *s; )
//         {
//             /* token = [^ or |]*  */
//             for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
//             if (*t)
//             {
//                 *t++ = '\0';
//                 while ((*t == ' ') || (*t == '|')) t++;
//             }

//             if (grab_one_character_uflag(ph_ptr, s))
//                 return PARSE_ERROR_INVALID_FLAG;

//             s = t;
//         }
//     }

//         /* Process 'E' for "Starting Equipment" */
//     else if (buf[0] == 'E')
//     {
//         int tval, sval, min, max;

//         start_item* e_ptr;

//         /* There better be a current pr_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Access the item */
//         e_ptr = &ph_ptr->start_items[cur_equip];

//         /* Scan for the values */
//         if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &tval, &sval, &min, &max))
//             return (PARSE_ERROR_GENERIC);

//         if ((min < 0) || (max < 0) || (min > 99) || (max > 99))
//             return (PARSE_ERROR_INVALID_ITEM_NUMBER);

//         /* Save the values */
//         e_ptr->tval = tval;
//         e_ptr->sval = sval;
//         e_ptr->min = min;
//         e_ptr->max = max;

//         /* Next item */
//         cur_equip++;

//         /* Limit number of starting items */
//         if (cur_equip > MAX_START_ITEMS)
//             return (PARSE_ERROR_GENERIC);
//     }


//     /* Process 'D' for "Description" */
//     else if (buf[0] == 'D')
//     {
//         /* There better be a current ph_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Get the text */
//         s = buf + 2;

//         /* Store the text */
//         if (!add_text(&(ph_ptr->text), head, s))
//             return (PARSE_ERROR_OUT_OF_MEMORY);
//     }
//     /* Process 'C' for character ability entries */
//     else if (buf[0] == 'C')
//     {
//         char *t = buf + 1;
//         int   pair = 0;

//         if (!ph_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Debug: which character we're parsing into */
//         log_debug("Parsing abilities for character \"%s\"...",
//                 head->name_ptr + ph_ptr->name);

//         /* Read up to CHARACTER_ABILITY_MAX of ":stat:ability" pairs */
//         while (pair < CHARACTER_ABILITY_MAX)
//         {
//             /* stat */
//             t = strchr(t, ':');
//             if (!t) break;
//             *t++ = '\0';
//             ph_ptr->a_adj[pair][0] = (s16b)atoi(t);

//             /* ability */
//             t = strchr(t, ':');
//             if (!t) break;
//             *t++ = '\0';
//             ph_ptr->a_adj[pair][1] = (s16b)atoi(t);

//             log_debug("  parsed slot %d -> stat=%d ability=%d",
//                     pair,
//                     ph_ptr->a_adj[pair][0],
//                     ph_ptr->a_adj[pair][1]);

//             pair++;
//         }

//         log_debug("  total %d ability pairs parsed", pair);
//     }

//     else
//     {
//         /* Oops */
//         return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
//     }

//     /* Success */
//     return (0);
// }

errr parse_c_info(char* buf, header* head)
{
    int j;
    static int cur_equip = 0;

    char *s, *t;

    /* Current entry */
    static character_profile* ph_ptr = NULL;

    log_trace("Parsing characters");

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        char *s;
        int   idx, j;

        /* Find the colon before the name */
        s = strchr(buf + 2, ':');
        if (!s) return (PARSE_ERROR_GENERIC);

        /* Split and advance to the name text */
        *s++ = '\0';
        if (!*s) return (PARSE_ERROR_GENERIC);

        /* Parse the index */
        idx = atoi(buf + 2);
        if (idx <= error_idx)            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        if (idx >= head->info_num)       return (PARSE_ERROR_TOO_MANY_ENTRIES);
        error_idx = idx;

        /* Point at this slot */
        ph_ptr = (character_profile*)head->info_ptr + idx;

        /* RESET equipment counter for new character */
        cur_equip = 0;

        /* Initialize power to default value 1 (average) */
        ph_ptr->power = 1;

        /* Store the name offset */
        if (!(ph_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Debug: announce new character and its name */
        log_trace("New character #%d: \"%s\"", idx,
                head->name_ptr + ph_ptr->name);

        /* Sentinel-initialize all ability slots to "empty" */
        for (j = 0; j < CHARACTER_ABILITY_MAX; j++)
        {
            ph_ptr->a_adj[j][0] = -1;
            ph_ptr->a_adj[j][1] = -1;
        }
        log_trace("  a_adj slots 0..%d set to -1", CHARACTER_ABILITY_MAX - 1);

        /* Initialize starting items array */
        for (j = 0; j < MAX_START_ITEMS; j++)
        {
            ph_ptr->start_items[j].tval = 0;
            ph_ptr->start_items[j].sval = 0;
            ph_ptr->start_items[j].min = 0;
            ph_ptr->start_items[j].max = 0;
        }
        log_debug("  start_items array initialized");
    }

    /* Process 'Q' for stable GUID */
    else if (buf[0] == 'Q')
    {
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        u64b guid = 0;
        if (!parse_u64b_hex(buf + 2, &guid))
            return (PARSE_ERROR_GENERIC);

        ph_ptr->guid = score_guid_from_u64(guid);
    }

    /* Process 'A' for "Alternate Name" */
    else if (buf[0] == 'A')
    {
        /* Find the colon before the name */
        s = strchr(buf, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Store the name */
        if (!(ph_ptr->alt_name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'B' for "Start String" */
    else if (buf[0] == 'B')
    {
        /* Find the colon before the name */
        s = strchr(buf, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Store the name */
        if (!(ph_ptr->start_string = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'S' for "Stats" (one line only) */
    else if (buf[0] == 'S')
    {
        int adj;

        /* There better be a current ph_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Start the string */
        s = buf + 1;

        /* For each stat */
        for (j = 0; j < A_MAX; j++)
        {
            /* Find the colon before the subindex */
            s = strchr(s, ':');

            /* Verify that colon */
            if (!s)
                return (PARSE_ERROR_GENERIC);

            /* Nuke the colon, advance to the subindex */
            *s++ = '\0';

            /* Get the value */
            adj = atoi(s);

            /* Save the value */
            ph_ptr->h_adj[j] = adj;

            /* Next... */
            continue;
        }
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current pr_ptr */
        if (!ph_ptr)
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
            if (0 != grab_one_character_flag(ph_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* ------------------------------------------------------------ */
    /* U: list of Unique flags        */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'U')
    {
        if (!ph_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            /* token = [^ or |]*  */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_character_uflag(ph_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }

        /* Process 'E' for "Starting Equipment" */
    else if (buf[0] == 'E')
    {
        int tval, sval, min, max;

        start_item* e_ptr;

        /* There better be a current pr_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Check if we've exceeded the maximum number of items */
        if (cur_equip >= MAX_START_ITEMS)
        {
            log_debug("Warning: Too many starting items for character (max %d), ignoring", MAX_START_ITEMS);
            return (PARSE_ERROR_GENERIC);
        }

        /* Access the item */
        e_ptr = &ph_ptr->start_items[cur_equip];

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &tval, &sval, &min, &max))
            return (PARSE_ERROR_GENERIC);

        if ((min < 0) || (max < 0) || (min > 99) || (max > 99))
            return (PARSE_ERROR_INVALID_ITEM_NUMBER);

        /* Save the values */
        e_ptr->tval = tval;
        e_ptr->sval = sval;
        e_ptr->min = min;
        e_ptr->max = max;

        /* Debug: show what we parsed */
        log_debug("  Equipment slot %d: tval=%d sval=%d min=%d max=%d", 
                cur_equip, tval, sval, min, max);

        /* Next item */
        cur_equip++;
    }


    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current ph_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(ph_ptr->text), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }
    /* Process 'C' for character ability entries */
    else if (buf[0] == 'C')
    {
        char *t = buf + 2; /* Skip 'C:' */
        int   pair = 0;
        char *stat_start, *ability_start;

        if (!ph_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Debug: which character we're parsing into */
        log_debug("Parsing abilities for character \"%s\" from line: %s",
                head->name_ptr + ph_ptr->name, buf);

        /* Read up to CHARACTER_ABILITY_MAX of ":stat:ability" pairs */
        while (pair < CHARACTER_ABILITY_MAX && t && *t)
        {
            /* Find first colon for stat */
            if (*t == ':') t++; /* Skip leading colon if present */
            stat_start = t;
            
            t = strchr(t, ':');
            if (!t) break;
            *t++ = '\0';
            
            if (!*stat_start) break; /* Empty stat */
            ph_ptr->a_adj[pair][0] = (s16b)atoi(stat_start);

            /* Find second colon for ability */
            ability_start = t;
            t = strchr(t, ':');
            if (t) {
                *t++ = '\0';
            }
            
            if (!*ability_start) break; /* Empty ability */
            ph_ptr->a_adj[pair][1] = (s16b)atoi(ability_start);

            log_debug("  parsed slot %d -> stat=%d ability=%d",
                    pair,
                    ph_ptr->a_adj[pair][0],
                    ph_ptr->a_adj[pair][1]);

            pair++;
            
            /* If no more colons, we're done */
            if (!t) break;
        }

        log_debug("  total %d ability pairs parsed", pair);
    }

    /* Process 'P' for "Power" */
    else if (buf[0] == 'P')
    {
        /* There better be a current ph_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse the power value */
        ph_ptr->power = (byte)atoi(buf + 2);
        
        log_debug("  power set to %d", ph_ptr->power);
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