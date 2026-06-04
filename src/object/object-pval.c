/* File: object/object-pval.c */

#include "angband.h"
#include "externs.h"
#include "object/object-pval.h"


static const u32b pval_stat_flag_pos[A_MAX] = {
    [A_STR] = TR1_STR,
    [A_DEX] = TR1_DEX,
    [A_CON] = TR1_CON,
    [A_GRA] = TR1_GRA,
};

static const u32b pval_stat_flag_neg[A_MAX] = {
    [A_STR] = TR1_NEG_STR,
    [A_DEX] = TR1_NEG_DEX,
    [A_CON] = TR1_NEG_CON,
    [A_GRA] = TR1_NEG_GRA,
};

static const u32b pval_skill_flag[S_MAX] = {
    [S_MEL] = TR1_MEL,
    [S_ARC] = TR1_ARC,
    [S_STL] = TR1_STL,
    [S_PER] = TR1_PER,
    [S_WIL] = TR1_WIL,
    [S_SMT] = TR1_SMT,
    [S_SNG] = TR1_SNG,
};

static u32b bonus_override_flags1(const bool stat_bonus_set[A_MAX],
    const bool skill_bonus_set[S_MAX])
{
    u32b mask = 0;

    if (stat_bonus_set)
    {
        for (int i = 0; i < A_MAX; i++)
        {
            if (!stat_bonus_set[i])
                continue;

            mask |= pval_stat_flag_pos[i];
            mask |= pval_stat_flag_neg[i];
        }
    }

    if (skill_bonus_set)
    {
        for (int i = 0; i < S_MAX; i++)
        {
            if (!skill_bonus_set[i])
                continue;

            mask |= pval_skill_flag[i];
        }
    }

    return mask;
}

u32b object_kind_pval_flags1(const object_kind* k_ptr)
{
    if (!k_ptr)
        return 0;

    return (k_ptr->flags1 & TR1_PVAL_MASK)
        & ~bonus_override_flags1(k_ptr->stat_bonus_set, k_ptr->skill_bonus_set);
}

u32b artefact_pval_flags1(const artefact_type* a_ptr)
{
    if (!a_ptr)
        return 0;

    return (a_ptr->flags1 & TR1_PVAL_MASK)
        & ~bonus_override_flags1(a_ptr->stat_bonus_set, a_ptr->skill_bonus_set);
}

u32b ego_item_pval_flags1(const ego_item_type* e_ptr)
{
    if (!e_ptr)
        return 0;

    return (e_ptr->flags1 & TR1_PVAL_MASK)
        & ~bonus_override_flags1(e_ptr->stat_bonus_set, e_ptr->skill_bonus_set);
}

u32b object_pval_flags1(const object_type* o_ptr)
{
    u32b mask = 0;

    if (!o_ptr)
        return 0;

    if (o_ptr->k_idx)
        mask |= object_kind_pval_flags1(&k_info[o_ptr->k_idx]);

    if (o_ptr->name1)
        mask |= artefact_pval_flags1(&a_info[o_ptr->name1]);

    if (object_ego_prefix(o_ptr))
        mask |= ego_item_pval_flags1(&e_info[object_ego_prefix(o_ptr)]);

    if (object_ego_suffix(o_ptr))
        mask |= ego_item_pval_flags1(&e_info[object_ego_suffix(o_ptr)]);

    return mask;
}

void object_apply_pval_delta_with_mask(object_type* o_ptr, u32b mask, int delta)
{
    if (!o_ptr || delta == 0)
        return;

    if (mask & TR1_STR)
        o_ptr->stat_bonus[A_STR] += (s16b)delta;
    if (mask & TR1_NEG_STR)
        o_ptr->stat_bonus[A_STR] -= (s16b)delta;

    if (mask & TR1_DEX)
        o_ptr->stat_bonus[A_DEX] += (s16b)delta;
    if (mask & TR1_NEG_DEX)
        o_ptr->stat_bonus[A_DEX] -= (s16b)delta;

    if (mask & TR1_CON)
        o_ptr->stat_bonus[A_CON] += (s16b)delta;
    if (mask & TR1_NEG_CON)
        o_ptr->stat_bonus[A_CON] -= (s16b)delta;

    if (mask & TR1_GRA)
        o_ptr->stat_bonus[A_GRA] += (s16b)delta;
    if (mask & TR1_NEG_GRA)
        o_ptr->stat_bonus[A_GRA] -= (s16b)delta;

    if (mask & TR1_MEL)
        o_ptr->skill_bonus[S_MEL] += (s16b)delta;
    if (mask & TR1_ARC)
        o_ptr->skill_bonus[S_ARC] += (s16b)delta;
    if (mask & TR1_STL)
        o_ptr->skill_bonus[S_STL] += (s16b)delta;
    if (mask & TR1_PER)
        o_ptr->skill_bonus[S_PER] += (s16b)delta;
    if (mask & TR1_WIL)
        o_ptr->skill_bonus[S_WIL] += (s16b)delta;
    if (mask & TR1_SMT)
        o_ptr->skill_bonus[S_SMT] += (s16b)delta;
    if (mask & TR1_SNG)
        o_ptr->skill_bonus[S_SNG] += (s16b)delta;
}

