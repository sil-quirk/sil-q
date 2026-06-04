/* File: object/object-util.c */

#include "angband.h"
#include "externs.h"
#include "object/object-util.h"
#include "object/object-internal.h"


s16b lookup_kind(int tval, int sval)
{
    int k;

    /* Look for it */
    for (k = 1; k < z_info->k_max; k++)
    {
        object_kind* k_ptr = &k_info[k];

        /* Found a match */
        if ((k_ptr->tval == tval) && (k_ptr->sval == sval))
            return (k);
    }

    /* Oops */
    msg_format("No object (%d,%d)", tval, sval);

    /* Oops */
    return (0);
}

/*
 * Wipe an object clean.
 */
void object_wipe(object_type* o_ptr)
{
    /* Wipe the structure */
    memset(o_ptr, 0, sizeof(object_type));

    /* Reset preferred pickup slot */
    o_ptr->pickup_slot = -1;
}

/*
 * Prepare an object based on an existing object
 */
void object_copy(object_type* o_ptr, const object_type* j_ptr)
{
    /* Copy the structure */
    memcpy(o_ptr, j_ptr, sizeof(object_type));
}

static bool object_is_wooden_chest(const object_type* o_ptr)
{
    if (!o_ptr || o_ptr->tval != TV_CHEST)
        return (false);

    return ((o_ptr->sval == SV_CHEST_SMALL_WOODEN)
        || (o_ptr->sval == SV_CHEST_LARGE_WOODEN));
}

static byte chest_trap_flags_for_pval(int pval)
{
    if (pval < 0)
        pval = -pval;

    if ((pval < 1) || (pval > 25))
        return (0);

    return (chest_traps[pval]);
}

byte object_chest_trap_flags(const object_type* o_ptr)
{
    byte trap;

    if (!o_ptr || o_ptr->tval != TV_CHEST)
        return (0);

    trap = chest_trap_flags_for_pval(o_ptr->pval);

    if (object_is_wooden_chest(o_ptr))
        trap &= (byte)(~CHEST_FLAME);

    return (trap);
}

/*
 * Set Hallucinatory object kind
 */
int random_k_idx(void)
{
    object_kind* k_ptr;
    int kind_idx;

    while (1)
    {
        kind_idx = rand_int(z_info->k_max);
        k_ptr = &k_info[kind_idx];
        if (k_ptr->tval != 0)
            return (kind_idx);
    }
}

/*
 * Prepare an object based on an object kind.
 */
void object_prep(object_type* o_ptr, int k_idx)
{
    int i;

    object_kind* k_ptr = &k_info[k_idx];

    /* Clear the record */
    memset(o_ptr, 0, sizeof(object_type));

    /* Save the kind index */
    o_ptr->k_idx = k_idx;

    /* Save the hallucinatory kind index */
    o_ptr->image_k_idx = random_k_idx();

    /* Efficiency -- tval/sval */
    o_ptr->tval = k_ptr->tval;
    o_ptr->sval = k_ptr->sval;

    /* Default "pval" */
    o_ptr->pval = k_ptr->pval;

    /* Per-stat/skill bonuses */
    for (i = 0; i < A_MAX; i++)
        o_ptr->stat_bonus[i] = k_ptr->stat_bonus[i];
    for (i = 0; i < S_MAX; i++)
        o_ptr->skill_bonus[i] = k_ptr->skill_bonus[i];

    /* Default number */
    o_ptr->number = 1;

    o_ptr->weight = object_roll_base_weight(k_ptr);

    /* Default bonuses to attack and defence */
    o_ptr->att = k_ptr->att;
    o_ptr->dd = k_ptr->dd;
    o_ptr->ds = k_ptr->ds;
    o_ptr->evn = k_ptr->evn;
    o_ptr->pd = k_ptr->pd;
    o_ptr->ps = k_ptr->ps;

    // add the abilities
    for (i = 0; i < k_ptr->abilities; i++)
    {
        o_ptr->skilltype[i] = k_ptr->skilltype[i];
        o_ptr->abilitynum[i] = k_ptr->abilitynum[i];
    }
    o_ptr->abilities = k_ptr->abilities;

    /* Hack -- worthless items are always "broken" */
    if (k_ptr->cost <= 0)
        o_ptr->ident |= (IDENT_BROKEN);

    /* Hack -- cursed items are always "cursed" */
    if (k_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);
}

/*
 * Cheat -- describe a created object for the user
 */
