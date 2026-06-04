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
static cptr a_info_act[ACT_MAX] = { "ILLUMINATION", "MAGIC_MAP", "CLAIRVOYANCE",
    "PROT_EVIL", "DISP_EVIL", "HEAL1", "HEAL2", "CURE_WOUNDS", "HASTE1",
    "HASTE2", "FIRE1", "FIRE2", "FIRE3", "FROST1", "FROST2", "FROST3", "FROST4",
    "FROST5", "ACID1", "RECHARGE1", "SLEEP", "LIGHTNING_BOLT", "ELEC2",
    "BANISHMENT", "MASS_BANISHMENT", "IDENTIFY_FULLY", "DRAIN_LIFE1",
    "DRAIN_LIFE2", "BIZZARE", "STAR_BALL", "RAGE_BLESS_RESIST", "PHASE",
    "TRAP_DOOR_DEST", "DETECT", "RESIST", "TELEPORT", "RESTORE_VOICE",
    "MISSILE", "ARROW", "REM_FEAR_POIS", "STINKING_CLOUD", "STONE_TO_MUD",
    "TELE_AWAY", "WOR", "CONFUSE", "PROBE", "FIREBRAND", "STARLIGHT",
    "MANA_BOLT", "BERSERKER", "RES_ACID", "RES_ELEC", "RES_FIRE", "RES_COLD",
    "RES_POIS" };

static errr grab_one_artefact_flag(artefact_type* ptr, cptr what)
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
 * Grab one activation from a textual string
 */
static errr grab_one_activation(artefact_type* a_ptr, cptr what)
{
    int i;

    /* Scan activations */
    for (i = 0; i < ACT_MAX; i++)
    {
        if (streq(what, a_info_act[i]))
        {
            a_ptr->activation = i;
            return (0);
        }
    }

    /* Oops */
    msg_format("Unknown artefact activation '%s'.", what);

    /* Error */
    return (PARSE_ERROR_GENERIC);
}

/*
 * Initialize the "a_info" array, by parsing an ascii "template" file
 */
errr parse_a_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static artefact_type* a_ptr = NULL;

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
        a_ptr = (artefact_type*)head->info_ptr + i;

        /* Store the name */
        SDL_strlcpy(a_ptr->name, s, MAX_LEN_ART_NAME);

        /* Ignore everything */
        a_ptr->flags3 |= (TR3_IGNORE_MASK);

        /* Sil-y: paranoia: make sure that the default values are 0 */
        a_ptr->d_attr = 0;
        a_ptr->d_char = 0;

        /* Reset per-stat/skill bonuses. */
        for (int si = 0; si < A_MAX; si++)
        {
            a_ptr->stat_bonus[si] = 0;
            a_ptr->stat_bonus_set[si] = false;
        }
        for (int sk = 0; sk < S_MAX; sk++)
        {
            a_ptr->skill_bonus[sk] = 0;
            a_ptr->skill_bonus_set[sk] = false;
        }

        /* Default spawn stack size */
        a_ptr->spawn_num = 1;
    }

    /* Sil -- added this to allow for artefacts that look different to the base
     * type */
    /* Process 'G' for "Graphics" (one line only) */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current a_ptr */
        if (!a_ptr)
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
        a_ptr->d_attr = d_attr;
        a_ptr->d_char = d_char;
    }
    /* Process 'Q' for GUID */
    else if (buf[0] == 'Q')
    {
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        u64b guid;
        if (!parse_u64b_hex(buf + 2, &guid))
            return (PARSE_ERROR_GENERIC);

        a_ptr->guid = score_guid_from_u64(guid);
    }

    /* Process 'S' for spawn stack size */
    else if (buf[0] == 'S')
    {
        int spawn_num;

        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (1 != sscanf(buf + 2, "%d", &spawn_num))
            return (PARSE_ERROR_GENERIC);

        if (spawn_num < 1 || spawn_num > 255)
            return (PARSE_ERROR_GENERIC);

        a_ptr->spawn_num = (byte)spawn_num;
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int tval, sval, pval;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval, &pval))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        a_ptr->tval = tval;
        a_ptr->sval = sval;
        a_ptr->pval = pval;

        apply_default_pval_bonuses(a_ptr->flags1, a_ptr->pval,
            a_ptr->stat_bonus, a_ptr->stat_bonus_set,
            a_ptr->skill_bonus, a_ptr->skill_bonus_set);
    }

    /* Process 'M' for per-stat/skill bonus overrides (one per line) */
    else if (buf[0] == 'M')
    {
        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = strchr(buf + 2, ':');
        if (!s)
            return (PARSE_ERROR_GENERIC);

        *s++ = '\0';
        cptr token = buf + 2;
        int value = atoi(s);

        if (!apply_obj_bonus_token(token, value,
                &a_ptr->flags1,
                a_ptr->stat_bonus, a_ptr->stat_bonus_set,
                a_ptr->skill_bonus, a_ptr->skill_bonus_set))
        {
            return (PARSE_ERROR_GENERIC);
        }

        apply_default_pval_bonuses(a_ptr->flags1, a_ptr->pval,
            a_ptr->stat_bonus, a_ptr->stat_bonus_set,
            a_ptr->skill_bonus, a_ptr->skill_bonus_set);
    }

    /* Process 'W' for "More Info" (one line only) */
    else if (buf[0] == 'W')
    {
        int level, rarity, wgt;
        long cost;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%ld", &level, &rarity, &wgt, &cost))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        a_ptr->level = level;
        a_ptr->rarity = rarity;
        a_ptr->weight = wgt;
        a_ptr->cost = cost;
    }

    /* Process 'P' for "power" and such */
    else if (buf[0] == 'P')
    {
        int att, dd, ds, evn, pd, ps;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (6
            != sscanf(
                buf + 2, "%d:%dd%d:%d:%dd%d", &att, &dd, &ds, &evn, &pd, &ps))
            return (PARSE_ERROR_GENERIC);

        a_ptr->att = att;
        a_ptr->dd = dd;
        a_ptr->ds = ds;
        a_ptr->evn = evn;
        a_ptr->pd = pd;
        a_ptr->ps = ps;
    }

    /* Process 'X' for elemental shield block chance */
    else if (buf[0] == 'X')
    {
        int elemental_block;

        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (1 != sscanf(buf + 2, "%d", &elemental_block))
            return (PARSE_ERROR_GENERIC);

        if ((elemental_block < 0) || (elemental_block > 100))
            return (PARSE_ERROR_GENERIC);

        a_ptr->elemental_block = (byte)elemental_block;
    }

    /* Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current a_ptr */
        if (!a_ptr)
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
            if (0 != grab_one_artefact_flag(a_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }

        apply_default_pval_bonuses(a_ptr->flags1, a_ptr->pval,
            a_ptr->stat_bonus, a_ptr->stat_bonus_set,
            a_ptr->skill_bonus, a_ptr->skill_bonus_set);
    }

    /* Process 'A' for "Activation & time" */
    else if (buf[0] == 'A')
    {
        int ptime, prand;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

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

        /* Get the activation */
        if (grab_one_activation(a_ptr, buf + 2))
            return (PARSE_ERROR_GENERIC);

        /* Scan for the values */
        if (2 != sscanf(s, "%d:%d", &ptime, &prand))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        a_ptr->time = ptime;
        a_ptr->randtime = prand;
    }

    /* Process 'B' for "aBilities" (one line only) */
    /* Format: B:skilltype/abilitynum/banetype:skilltype/abilitynum/banetype:... */
    /* The banetype is optional (defaults to 0 = player choice) */
    else if (buf[0] == 'B')
    {
        int i;
        char* u;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum and bane_type */
            a_ptr->abilitynum[i] = 0;
            a_ptr->bane_type[i] = 0;

            /* Store the skilltype */
            a_ptr->skilltype[i] = atoi(s + 1);

            /* List this ability */
            a_ptr->abilities++;

            /* Find the first slash (abilitynum) */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int abilitynum = atoi(t + 1);
                if (abilitynum > 0)
                    a_ptr->abilitynum[i] = abilitynum;

                /* Look for a second slash (bane_type) */
                u = strchr(t + 1, '/');
                if (u && (!s || u < s))
                {
                    int banetype = atoi(u + 1);
                    if (banetype > 0)
                        a_ptr->bane_type[i] = banetype;
                }
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&a_ptr->text, head, s))
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
 * Add a name to the probability tables
 */

#endif /* ALLOW_TEMPLATES */