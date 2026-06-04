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
static const u32b obj_stat_flag_pos[A_MAX] = { TR1_STR, TR1_DEX, TR1_CON, TR1_GRA };
static const u32b obj_stat_flag_neg[A_MAX]
    = { TR1_NEG_STR, TR1_NEG_DEX, TR1_NEG_CON, TR1_NEG_GRA };

static const u32b obj_skill_flag[S_MAX] = {
    [S_MEL] = TR1_MEL,
    [S_ARC] = TR1_ARC,
    [S_STL] = TR1_STL,
    [S_PER] = TR1_PER,
    [S_WIL] = TR1_WIL,
    [S_SMT] = TR1_SMT,
    [S_SNG] = TR1_SNG,
};

static bool parse_obj_bonus_token(
    const char* token, bool* is_stat, int* index, bool* has_neg_prefix)
{
    if (!token || !token[0] || !is_stat || !index || !has_neg_prefix)
        return false;

    *has_neg_prefix = false;

    const char* name = token;
    if (strncmp(name, "NEG_", 4) == 0)
    {
        *has_neg_prefix = true;
        name += 4;
    }

    if (streq(name, "STR"))
    {
        *is_stat = true;
        *index = A_STR;
        return true;
    }
    if (streq(name, "DEX"))
    {
        *is_stat = true;
        *index = A_DEX;
        return true;
    }
    if (streq(name, "CON"))
    {
        *is_stat = true;
        *index = A_CON;
        return true;
    }
    if (streq(name, "GRA"))
    {
        *is_stat = true;
        *index = A_GRA;
        return true;
    }

    if (*has_neg_prefix)
        return false;

    if (streq(name, "MELEE"))
    {
        *is_stat = false;
        *index = S_MEL;
        return true;
    }
    if (streq(name, "ARCHERY"))
    {
        *is_stat = false;
        *index = S_ARC;
        return true;
    }
    if (streq(name, "STEALTH"))
    {
        *is_stat = false;
        *index = S_STL;
        return true;
    }
    if (streq(name, "PERCEPTION"))
    {
        *is_stat = false;
        *index = S_PER;
        return true;
    }
    if (streq(name, "WILL"))
    {
        *is_stat = false;
        *index = S_WIL;
        return true;
    }
    if (streq(name, "SMITHING"))
    {
        *is_stat = false;
        *index = S_SMT;
        return true;
    }
    if (streq(name, "SONG"))
    {
        *is_stat = false;
        *index = S_SNG;
        return true;
    }

    return false;
}

void apply_default_pval_bonuses(u32b flags1, s16b pval,
    s16b stat_bonus[A_MAX], const bool stat_bonus_set[A_MAX],
    s16b skill_bonus[S_MAX], const bool skill_bonus_set[S_MAX])
{
    for (int i = 0; i < A_MAX; i++)
    {
        if (stat_bonus_set && stat_bonus_set[i])
            continue;

        int bonus = 0;
        if (flags1 & obj_stat_flag_pos[i])
            bonus += pval;
        if (flags1 & obj_stat_flag_neg[i])
            bonus -= pval;
        stat_bonus[i] = (s16b)bonus;
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (skill_bonus_set && skill_bonus_set[i])
            continue;

        const u32b flag = obj_skill_flag[i];
        skill_bonus[i] = (flag && (flags1 & flag)) ? pval : 0;
    }
}

bool apply_obj_bonus_token(const char* token, int value,
    u32b* flags1,
    s16b stat_bonus[A_MAX], bool stat_bonus_set[A_MAX],
    s16b skill_bonus[S_MAX], bool skill_bonus_set[S_MAX])
{
    if (!flags1 || !token)
        return false;

    bool is_stat = false;
    int index = 0;
    bool has_neg_prefix = false;

    if (!parse_obj_bonus_token(token, &is_stat, &index, &has_neg_prefix))
        return false;

    int normalized = value;
    if (has_neg_prefix && normalized > 0)
        normalized = -normalized;

    if (is_stat)
    {
        if (index < 0 || index >= A_MAX)
            return false;

        stat_bonus[index] = (s16b)normalized;
        if (stat_bonus_set)
            stat_bonus_set[index] = true;

        *flags1 &= ~(obj_stat_flag_pos[index] | obj_stat_flag_neg[index]);
        if (normalized >= 0)
            *flags1 |= obj_stat_flag_pos[index];
        else
            *flags1 |= obj_stat_flag_neg[index];

        return true;
    }

    if (index < 0 || index >= S_MAX)
        return false;

    skill_bonus[index] = (s16b)normalized;
    if (skill_bonus_set)
        skill_bonus_set[index] = true;

    if (obj_skill_flag[index])
        *flags1 |= obj_skill_flag[index];

    return true;
}

bool parse_bonus_value_range(char* text, int* min_value, int* max_value)
{
    char* sep;

    if (!text || !text[0] || !min_value || !max_value)
        return false;

    sep = strchr(text, ':');
    if (!sep)
    {
        *min_value = atoi(text);
        *max_value = *min_value;
        return true;
    }

    if (strchr(sep + 1, ':'))
        return false;

    *sep++ = '\0';
    if (!text[0] || !sep[0])
        return false;

    *min_value = atoi(text);
    *max_value = atoi(sep);
    return true;
}

bool apply_ego_bonus_token_range(const char* token, int min_value, int max_value,
    u32b* flags1,
    s16b stat_bonus_min[A_MAX], s16b stat_bonus[A_MAX], bool stat_bonus_set[A_MAX],
    s16b skill_bonus_min[S_MAX], s16b skill_bonus[S_MAX], bool skill_bonus_set[S_MAX])
{
    if (!flags1 || !token)
        return false;

    bool is_stat = false;
    int index = 0;
    bool has_neg_prefix = false;

    if (!parse_obj_bonus_token(token, &is_stat, &index, &has_neg_prefix))
        return false;

    int normalized_min = min_value;
    int normalized_max = max_value;

    if (has_neg_prefix)
    {
        if (normalized_min > 0)
            normalized_min = -normalized_min;
        if (normalized_max > 0)
            normalized_max = -normalized_max;
    }

    if (normalized_min > normalized_max)
    {
        if (!has_neg_prefix)
            return false;

        int tmp = normalized_min;
        normalized_min = normalized_max;
        normalized_max = tmp;
    }

    if (is_stat)
    {
        if (index < 0 || index >= A_MAX)
            return false;
        if (normalized_min < 0 && normalized_max > 0)
            return false;

        stat_bonus_min[index] = (s16b)normalized_min;
        stat_bonus[index] = (s16b)normalized_max;
        if (stat_bonus_set)
            stat_bonus_set[index] = true;

        *flags1 &= ~(obj_stat_flag_pos[index] | obj_stat_flag_neg[index]);
        if (normalized_max < 0)
            *flags1 |= obj_stat_flag_neg[index];
        else
            *flags1 |= obj_stat_flag_pos[index];

        return true;
    }

    if (index < 0 || index >= S_MAX)
        return false;

    skill_bonus_min[index] = (s16b)normalized_min;
    skill_bonus[index] = (s16b)normalized_max;
    if (skill_bonus_set)
        skill_bonus_set[index] = true;

    if (obj_skill_flag[index])
        *flags1 |= obj_skill_flag[index];

    return true;
}

#endif /* ALLOW_TEMPLATES */