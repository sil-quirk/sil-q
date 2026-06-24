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
static cptr r_info_blow_method[]
    = { "", "HIT", "TOUCH", "XXX", "XXX", "CLAW", "BITE", "STING", "PECK",
          "WHIP", "XXX", "CRUSH", "ENGULF", "CRAWL", "THORN", "XXX", "XXX",
          "XXX", "XXX", "SPORE", "XXX", "XXX", "XXX", "XXX", "XXX", NULL };

/*
 * Monster Blow Effects
 */
static cptr r_info_blow_effect[] = { "", "HURT", "WOUND", "BATTER", "SHATTER",
    "UN_BONUS", "UN_POWER", "LOSE_MANA", "SLOW", "EAT_ITEM", "EAT_FOOD", "DARK",
    "HUNGER", "POISON", "ACID", "ELEC", "FIRE", "COLD", "BLIND", "CONFUSE",
    "TERRIFY", "ENTRANCE", "HALLU", "DISEASE", "LOSE_STR", "LOSE_DEX",
    "LOSE_CON", "LOSE_GRA", "LOSE_STR_CON", "LOSE_ALL", "DISARM", NULL };

static errr grab_one_basic_flag(monster_race* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RF1] = &(ptr->flags1);
    f[RF2] = &(ptr->flags2);
    f[RF3] = &(ptr->flags3);
    return grab_one_flag(f, "monster", what);
}

/*
 * Grab one (spell) flag in a monster_race from a textual string
 */
static errr grab_one_spell_flag(monster_race* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RF4] = &(ptr->flags4);
    return grab_one_flag(f, "monster", what);
}

/*
 * Initialize the "r_info" array, by parsing an ascii "template" file
 */
errr parse_r_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static monster_race* r_ptr = NULL;

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
        r_ptr = (monster_race*)head->info_ptr + i;

        /* Store the name */
        if (!(r_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(r_ptr->text), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'Q' for GUID */
    else if (buf[0] == 'Q')
    {
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        u64b guid = 0;
        if (!parse_u64b_hex(buf + 2, &guid))
            return (PARSE_ERROR_GENERIC);

        r_ptr->guid = guid;
    }

    /* Process 'G' for "Graphics" (one line only) */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current r_ptr */
        if (!r_ptr)
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
        r_ptr->d_attr = d_attr;
        r_ptr->d_char = d_char;
    }

    /* Process 'T' for "Tile" graphics (one line only) */
    else if (buf[0] == 'T')
    {
        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse and set tile coordinates */
        return parse_tile_line(buf, &r_ptr->x_attr, &r_ptr->x_char);
    }

    /* Process 'O' for visual "Orientation" (one line only) */
    else if (buf[0] == 'O')
    {
        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = buf + 2;
        while (isspace((unsigned char)*s))
            s++;

        if (streq(s, "FACING_LEFT") || streq(s, "LEFT"))
            r_ptr->tile_facing = MONSTER_TILE_FACING_LEFT;
        else if (streq(s, "FACING_RIGHT") || streq(s, "RIGHT"))
            r_ptr->tile_facing = MONSTER_TILE_FACING_RIGHT;
        else if (streq(s, "FACING_NONE") || streq(s, "NONE"))
            r_ptr->tile_facing = MONSTER_TILE_FACING_NONE;
        else
            return (PARSE_ERROR_GENERIC);
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int spd, hp1, hp2, light;

        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the other values */
        if (4 != sscanf(buf + 2, "%d:%dd%d:%d", &spd, &hp1, &hp2, &light))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        r_ptr->speed = spd;
        r_ptr->hdice = hp1;
        r_ptr->hside = hp2;
        r_ptr->light = light;
    }

    /* Process 'W' for "More Info" (one line only) */
    else if (buf[0] == 'W')
    {
        int lev, rar;

        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &lev, &rar))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        r_ptr->level = lev;
        r_ptr->rarity = rar;
    }

    /* Process 'A' for "Alertness Info" (one line only) */
    else if (buf[0] == 'A')
    {
        int sleep, per, stl, wil;

        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &sleep, &per, &stl, &wil))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        r_ptr->sleep = sleep;
        r_ptr->per = per;
        r_ptr->stl = stl;
        r_ptr->wil = wil;
    }

    /* Process 'P' for "Protection Info" (one line only) */
    else if (buf[0] == 'P')
    {
        int evn, pd = 0, ps = 0, n;

        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        n = sscanf(buf + 2, "[%d,%dd%d]", &evn, &pd, &ps);
        if ((n != 1) && (n != 3))
            return (PARSE_ERROR_GENERIC);

        //		if (3 != sscanf(buf+2, "[%d,%dd%d]",
        //						&evn, &pd, &ps)) return
        //(PARSE_ERROR_GENERIC);

        /* Save the values */
        r_ptr->evn = evn;
        r_ptr->pd = pd;
        r_ptr->ps = ps;
    }

    /* Process 'B' for "Blows" */
    else if (buf[0] == 'B')
    {
        int n1, n2, n;
        int att, dd, ds;

        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Find the next empty blow slot (if any) */
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
            if (!r_ptr->blow[i].method)
                break;

        /* Oops, no more slots */
        if (i == MONSTER_BLOW_MAX)
            return (PARSE_ERROR_GENERIC);

        /* Analyze the first field */
        for (s = t = buf + 2; *t && (*t != ':'); t++) /* loop */
            ;

        /* Terminate the field (if necessary) */
        if (*t == ':')
            *t++ = '\0';

        /* Analyze the method */
        for (n1 = 0; r_info_blow_method[n1]; n1++)
        {
            if (streq(s, r_info_blow_method[n1]))
                break;
        }

        /* Invalid method */
        if (!r_info_blow_method[n1])
            return (PARSE_ERROR_GENERIC);

        /* Analyze the second field */
        for (s = t; *t && (*t != ':'); t++) /* loop */
            ;

        /* Terminate the field (if necessary) */
        if (*t == ':')
            *t++ = '\0';

        /* Analyze effect */
        for (n2 = 0; r_info_blow_effect[n2]; n2++)
        {
            if (streq(s, r_info_blow_effect[n2]))
                break;
        }

        /* Invalid effect */
        if (!r_info_blow_effect[n2])
            return (PARSE_ERROR_GENERIC);

        // reset values
        dd = 0;
        ds = 0;

        n = sscanf(t, "(%d,%dd%d)", &att, &dd, &ds);
        if ((n != 1) && (n != 3))
            return (PARSE_ERROR_GENERIC);

        // s = t;

        /* Scan for the values */
        // if (1 != sscanf(t, "(%d)", &att))
        //{
        //	t = s;
        //	if (3 != sscanf(t, "(%d,%dd%d)", &att, &dd, &ds)) return
        //(PARSE_ERROR_GENERIC);
        //}

        /* Analyze the third field */
        // for (s = t; *t && (*t != 'd'); t++) /* loop */;

        /* Terminate the field (if necessary) */
        // if (*t == 'd') *t++ = '\0';

        /* Save the method */
        r_ptr->blow[i].method = n1;

        /* Save the effect */
        r_ptr->blow[i].effect = n2;

        /* Extract the damage dice and sides */
        r_ptr->blow[i].att = att;
        r_ptr->blow[i].dd = dd;
        r_ptr->blow[i].ds = ds;
    }

    /* Process 'F' for "Basic Flags" (multiple lines) */
    else if (buf[0] == 'F')
    {
        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry */
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
            if (0 != grab_one_basic_flag(r_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'S' for "Spell Flags" (multiple lines) */
    else if (buf[0] == 'S')
    {
        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry */
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

            /* XXX Hack -- Read spell frequency */
            if ((r_ptr->freq_ranged == 0)
                && (1 == sscanf(s, "SPELL_PCT_%d", &i)))
            {
                /* Sanity check */
                if ((i < 1) || (i > 100))
                    return (PARSE_ERROR_INVALID_SPELL_FREQ);

                /* Extract a "frequency" */
                r_ptr->freq_ranged = i;

                /* Start at next entry */
                s = t;

                /* Continue */
                continue;
            }

            /* Read spell power. */
            if ((r_ptr->spell_power == 0) && (1 == sscanf(s, "POW_%d", &i)))
            {
                /* Save spell power. */
                r_ptr->spell_power = i;

                /* Start at next entry */
                s = t;

                /* Continue */
                continue;
            }

            /* Parse this entry */
            if (0 != grab_one_spell_flag(r_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
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
 * Grab one flag in a player_race from a textual string
 *
 * Sil:  these used to be the TR1, TR2 and TR3 flags,
 *       but we now use the race/character flags (RHF).
 */

#endif /* ALLOW_TEMPLATES */
