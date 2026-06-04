/* File: object/object-flags.c */

#include "angband.h"
#include "externs.h"
#include "object/object-flags.h"

/*
 * Modes of object_flags_aux()
 */
#define OBJECT_FLAGS_FULL 1 /* Full info */
#define OBJECT_FLAGS_KNOWN 2 /* Only flags known to the player */

/*
 * Obtain the "flags" for an item (extended version with f4)
 */
static void object_flags_aux(
    int mode, const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4)
{
    object_kind* k_ptr;

    if (mode == OBJECT_FLAGS_KNOWN)
    {
        /* Clear */
        (*f1) = (*f2) = (*f3) = (*f4) = 0L;

        /* Must be identified (or being listed in object knowledge) */
        if (!object_known_p(o_ptr) && !(o_ptr->ident & IDENT_SPOIL))
            return;
    }

    k_ptr = &k_info[o_ptr->k_idx];

    /* Base object */
    (*f1) = k_ptr->flags1;
    (*f2) = k_ptr->flags2;
    (*f3) = k_ptr->flags3;
    (*f4) = k_ptr->flags4;

    /* Artefact flags add to the base object (never replace it) */
    if ((mode == OBJECT_FLAGS_FULL) || (mode == OBJECT_FLAGS_KNOWN))
    {
        /* Artefact */
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];

            (*f1) |= a_ptr->flags1;
            (*f2) |= a_ptr->flags2;
            (*f3) |= a_ptr->flags3;
            (*f4) |= a_ptr->flags4;
        }
    }

    /* Ego-items (prefix + suffix) */
    {
        byte ego_prefix = object_ego_prefix(o_ptr);
        if (ego_prefix)
        {
            ego_item_type* e_ptr = &e_info[ego_prefix];
            (*f1) |= e_ptr->flags1;
            (*f2) |= e_ptr->flags2;
            (*f3) |= e_ptr->flags3;
            (*f4) |= e_ptr->flags4;
        }

        byte ego_suffix = object_ego_suffix(o_ptr);
        if (ego_suffix)
        {
            ego_item_type* e_ptr = &e_info[ego_suffix];
            (*f1) |= e_ptr->flags1;
            (*f2) |= e_ptr->flags2;
            (*f3) |= e_ptr->flags3;
            (*f4) |= e_ptr->flags4;
        }
    }

    if (o_ptr->ident & IDENT_UNCURSED)
    {
        (*f3) &= ~(TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE);
    }
}

/*
 * Obtain the "flags" for an item (legacy 3-flag version)
 */
void object_flags(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3)
{
    u32b dummy_f4;
    object_flags_aux(OBJECT_FLAGS_FULL, o_ptr, f1, f2, f3, &dummy_f4);
}

/*
 * Obtain the "flags" for an item (extended version with f4)
 */
void object_flags4(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3,
    u32b* f4)
{
    object_flags_aux(OBJECT_FLAGS_FULL, o_ptr, f1, f2, f3, f4);
}

/*
 * Obtain the "flags" for an item which are known to the player
 */
void object_flags_known(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3)
{
    u32b dummy_f4;
    object_flags_aux(OBJECT_FLAGS_KNOWN, o_ptr, f1, f2, f3, &dummy_f4);
}

/*
 * Obtain the "flags" for an item which are known to the player (extended)
 */
void object_flags_known4(const object_type* o_ptr, u32b* f1, u32b* f2,
    u32b* f3, u32b* f4)
{
    object_flags_aux(OBJECT_FLAGS_KNOWN, o_ptr, f1, f2, f3, f4);
}

bool object_grants_ability(
    const object_type* o_ptr, int skilltype, int abilitynum)
{
    int i;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    for (i = 0; i < o_ptr->abilities; i++)
    {
        if ((o_ptr->skilltype[i] == skilltype)
            && (o_ptr->abilitynum[i] == abilitynum))
        {
            return true;
        }
    }

    return false;
}
