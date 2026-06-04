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
static runtype_type *rt_ptr = NULL;

errr parse_rt_info(char *buf, header *head)
{
    /* N:<index>:<name> ------------------------------------------- */
    if (buf[0] == 'N')
    {
        int idx;
        char *s = strchr(buf+2, ':');
        if (!s) return PARSE_ERROR_GENERIC;
        *s++ = '\0';
        idx = atoi(buf+2);

        /* normal sequential checks */
        if (idx <= error_idx) return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (idx >= head->info_num) return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = idx;

        rt_ptr = ((runtype_type*)head->info_ptr) + idx;
        memset(rt_ptr, 0, sizeof(runtype_type));
        rt_ptr->id = idx;
        strncpy(rt_ptr->name, s, sizeof(rt_ptr->name)-1);
        return 0;
    }

    /* C:b0|b1|b2 default-curses mask ----------------------------- */
    if (buf[0] == 'C')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        
        /* Initialize curse_stacks array to 0 */
        memset(rt_ptr->curse_stacks, 0, sizeof(rt_ptr->curse_stacks));
        rt_ptr->start_curses = 0;
        
        /* Parse curse specifications */
        char *tok = strtok(buf+2, "|");
        while (tok)
        {
            char *colon = strchr(tok, ':');
            if (colon)
            {
                /* Format: curse_id:stack_count */
                *colon = '\0';
                int curse_id = atoi(tok);
                int stack_count = atoi(colon + 1);
                
                if (curse_id >= 0 && curse_id < METAR_CURSE_SLOTS && stack_count > 0 && stack_count <= 255)
                {
                    rt_ptr->start_curses |= (1ULL << curse_id);
                    rt_ptr->curse_stacks[curse_id] = (byte)stack_count;
                }
            }
            else
            {
                /* Legacy format: just curse_id (default to 1 stack) */
                int curse_id = atoi(tok);
                if (curse_id >= 0 && curse_id < METAR_CURSE_SLOTS)
                {
                    rt_ptr->start_curses |= (1ULL << curse_id);
                    rt_ptr->curse_stacks[curse_id] = 1;
                }
            }
            tok = strtok(NULL, "|");
        }
        return 0;
    }

    /* U:TERM_RED  (colour) --------------------------------------- */
    if (buf[0] == 'U')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        rt_ptr->colour = (byte)color_text_to_attr(buf+2);
        return 0;
    }
    /* W:<num>  - win condition (Silmarils target) ---------------- */
    if (buf[0] == 'W')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        int v = atoi(buf + 2);
        if (v < 1) v = 1;
        if (v > 127) v = 127;
        rt_ptr->win_con = (byte)v;
        return 0;
    }

    /* L:<values>  - blessing point thresholds (score pool needed per point) */
    if (buf[0] == 'L')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        char *arg = buf + 2;
        char *comment = strchr(arg, '#');
        if (comment) *comment = '\0';

        int positional = 0;
        static const int positional_order[] = {
            RUNTYPE_BLESSING_MODE_NORMAL,
            RUNTYPE_BLESSING_MODE_EASIER,
            RUNTYPE_BLESSING_MODE_HARDER
        };

        for (char *tok = strtok(arg, "|"); tok; tok = strtok(NULL, "|"))
        {
            while (tok && isspace((unsigned char)*tok)) tok++;
            if (!tok || !*tok) continue;

            int mode = -1;
            char *value = tok;
            char *sep = strpbrk(tok, "=:");

            if (sep)
            {
                *sep = '\0';
                value = sep + 1;

                char *key = tok;
                while (key && isspace((unsigned char)*key)) key++;
                if (key && *key)
                {
                    char *key_end = key + strlen(key);
                    while (key_end > key && isspace((unsigned char)key_end[-1])) *--key_end = '\0';

                    char lower_key[16];
                    size_t key_len = strlen(key);
                    if (key_len >= sizeof(lower_key)) key_len = sizeof(lower_key) - 1;
                    for (size_t i = 0; i < key_len; i++)
                        lower_key[i] = (char)tolower((unsigned char)key[i]);
                    lower_key[key_len] = '\0';

                    if (streq(lower_key, "easier") || streq(lower_key, "easy"))
                        mode = RUNTYPE_BLESSING_MODE_EASIER;
                    else if (streq(lower_key, "harder") || streq(lower_key, "hard"))
                        mode = RUNTYPE_BLESSING_MODE_HARDER;
                    else if (streq(lower_key, "normal") || streq(lower_key, "default"))
                        mode = RUNTYPE_BLESSING_MODE_NORMAL;
                }
            }

            if (mode < 0)
            {
                if (positional < (int)N_ELEMENTS(positional_order))
                    mode = positional_order[positional];
                else
                    mode = RUNTYPE_BLESSING_MODE_NORMAL;
                positional++;
            }

            while (value && isspace((unsigned char)*value)) value++;
            if (!value || !*value) continue;

            char *value_end = value + strlen(value);
            while (value_end > value && isspace((unsigned char)value_end[-1])) *--value_end = '\0';
            if (!*value) continue;

            int val = atoi(value);
            if (val < 1) val = 1;
            if (val > 65535) val = 65535;

            if (mode >= 0 && mode < RUNTYPE_BLESSING_MODE_COUNT)
                rt_ptr->blessing_threshold_modes[mode] = (u16b)val;
        }

        return 0;
    }

    /* H:*  or  H:i|j|k  - applicable heroes mask (0..63) ---------- */
    if (buf[0] == 'H')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        char *arg = buf + 2;
        /* '*' means all heroes */
        if (*arg == '*') {
            for (int w = 0; w < FLAG_WORDS; ++w) rt_ptr->heroes[w] = 0xFFFFFFFFu;
            /* trim bits above 64 just in case */
            if (FLAG_WORDS > 2) for (int w = 2; w < FLAG_WORDS; ++w) rt_ptr->heroes[w] = 0;
            return 0;
        }
        /* otherwise a | separated list of indices */
        for (char *tok = strtok(arg, "|"); tok; tok = strtok(NULL, "|"))
        {
            int idx = atoi(tok);
            if (0 <= idx && idx < 64) {
                int w = idx >> 5, b = idx & 31;
                rt_ptr->heroes[w] |= (1u << b);
            }
        }
        return 0;
    }


    /* ignore unknown / comment lines                              */
    return PARSE_ERROR_UNDEFINED_DIRECTIVE;
}

#endif /* ALLOW_TEMPLATES */