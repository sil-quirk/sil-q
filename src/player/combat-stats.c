#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"

/*
 * Determines the total melee damage dice (before criticals and slays)
 */

byte total_mdd(const object_type* o_ptr)
{
    byte dd;

    /* if no weapon is wielded, use 1d1 */
    if (o_ptr->tval == 0)
    {
        dd = 1;
    }
    /* otherwise use the weapon dice */
    else
    {
        dd = o_ptr->dd;
    }
    /* add the modifiers */
    dd += p_ptr->to_mdd;

    if (p_ptr->active_ability[S_WIL][WIL_VENGEANCE])
    {
        dd += p_ptr->vengeance;
    }

    return (dd);
}

/*
 * Determines the strength modified damage sides for a melee or thrown weapon
 * Includes factors for strength and weight, but not bonuses from ring of damage
 * etc
 */
byte strength_modified_ds(const object_type* o_ptr, int str_adjustment)
{
    byte mds;
    int int_mds; /* to allow negative values in the intermediate stages */
    int str_to_mds;
    int divisor;

    str_to_mds = p_ptr->stat_use[A_STR] + str_adjustment;

    /* if no weapon, use 1d1 and don't limit strength bonus */
    if (o_ptr->tval == 0)
    {
        int_mds = 1;
        int_mds += str_to_mds;
    }
    /* if a weapon is being assessed, use its dice and limit bonus */
    else
    {
        int_mds = o_ptr->ds;

        if (two_handed_melee())
        {
            divisor = 10;

            /* Bonus for 'hand and a half' weapons like the bastard sword when
             * used with two hands - but not when using Subtlety */
            if (!p_ptr->active_ability[S_MEL][MEL_CONTROL])
            {
                int_mds += hand_and_a_half_bonus(o_ptr);
            }
        }
        else
        {
            divisor = 10;
        }

        /* limit the strength sides bonus by weapon weight */
        if ((str_to_mds > 0) && (str_to_mds > (o_ptr->weight / divisor)))
        {
            int_mds += o_ptr->weight / divisor;
        }
        else if ((str_to_mds < 0) && (str_to_mds < -(o_ptr->weight / divisor)))
        {
            int_mds += -(o_ptr->weight / divisor);
        }
        else
        {
            int_mds += str_to_mds;
        }
    }

    // add generic damage bonus
    int_mds += p_ptr->to_mds;

    // bonus for users of 'mighty blows' ability
    if (p_ptr->active_ability[S_MEL][MEL_POWER])
    {
        int_mds += 1;
    }

    /* make sure the total is non-negative */
    mds = (int_mds < 0) ? 0 : int_mds;

    return (mds);
}

/*
 * Determines the total melee damage sides (from strength and to_mds)
 * Does include strength and weight modifiers
 *
 * This function seems rather unnecessary these days...
 */
extern byte total_mds(const object_type* o_ptr, int str_adjustment)
{
    byte mds;
    int int_mds; /* to allow negative values in the inetermediate stages */

    int_mds = strength_modified_ds(o_ptr, str_adjustment);

    /* make sure the total is non-negative */
    mds = (int_mds < 0) ? 0 : int_mds;

    return (mds);
}

/*
 * Two handed melee weapon (including bastard sword used two handed)
 */
extern bool two_handed_melee(void)
{
    object_type* o_ptr = &inventory[INVEN_WIELD];

    if (!player_active_weapon_is_melee())
        return (false);

    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        || hand_and_a_half_bonus(o_ptr))
    {
        return (true);
    }

    /* For Maedhros character, hand-and-a-half weapons count as two-handed for ability purposes */
    if ((c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS)
        && (k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF))
        && (&inventory[INVEN_WIELD] == o_ptr) && (!inventory[INVEN_ARM].k_idx))
    {
        return (true);
    }

    return (false);
}

/*
 * Whether an item counts as light armour (has the LIGHT_ARMOR flag, either on
 * the base item or granted by an ego such as the (Light) prefix).
 */
extern bool armour_is_light(const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return (false);

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    return ((f4 & (TR4_LIGHT_ARMOR)) != 0);
}

/*
 * Whether the player is wearing only light armour.
 *
 * Strict check: every occupied protective slot (body, head, shield, gloves,
 * feet) must be light. Cloaks and the light source are always treated as light
 * and are not checked. An off-hand weapon (non-shield in INVEN_ARM) is not
 * armour and is ignored.
 */
extern bool wearing_only_light_armour(void)
{
    static const int slots[] = { INVEN_BODY, INVEN_HEAD, INVEN_HANDS,
        INVEN_FEET, INVEN_ARM };
    int s;

    for (s = 0; s < (int)N_ELEMENTS(slots); s++)
    {
        object_type* o_ptr = &inventory[slots[s]];

        /* Empty slots are fine */
        if (!o_ptr->k_idx)
            continue;

        /* The arm slot only counts as armour when it holds a shield */
        if ((slots[s] == INVEN_ARM) && (o_ptr->tval != TV_SHIELD))
            continue;

        if (!armour_is_light(o_ptr))
            return (false);
    }

    return (true);
}

/*
 * Bonus for 'hand and a half' weapons like the bastard sword when wielded with
 * two hands
 */
extern int hand_and_a_half_bonus(const object_type* o_ptr) //XXX Hand and a half
{
    if ((k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF))
        && (&inventory[INVEN_WIELD] == o_ptr) && (!inventory[INVEN_ARM].k_idx))
    {
        /* Maedhros character gets double the hand-and-a-half bonus */
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS)
        {
            return (3);
        }
        return (2);
    }
    return (0);
}

/*
 * Bonus for certain race/character blends (elves) using bows
 */
int bow_bonus(const object_type* o_ptr)
{
    int bonus = 0;

    if ((rp_ptr->flags & RHF_BOW_PROFICIENCY) && (o_ptr->tval == TV_BOW))
    {
        bonus += 1;
    }
    if ((current_character_profile->flags & RHF_BOW_PROFICIENCY) && (o_ptr->tval == TV_BOW))
    {
        bonus += 1;
    }

    return bonus;
}

/*
 * Bonus for certain race/character blends (dwarves) using axes
 */
int axe_bonus(const object_type* o_ptr)
{
    int bonus = 0;

    u32b f1, f2, f3;

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    if ((rp_ptr->flags & RHF_AXE_PROFICIENCY) && (f3 & (TR3_AXE)))
    {
        bonus += 1;
    }
    if ((current_character_profile->flags & RHF_AXE_PROFICIENCY) && (f3 & (TR3_AXE)))
    {
        bonus += 1;
    }

    return bonus;
}

/*
 * Bonus for people with polearm affinity
 */
int polearm_bonus(const object_type* o_ptr)
{
    int bonus = 0;

    u32b f1, f2, f3;

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    if (p_ptr->active_ability[S_MEL][MEL_POLEARMS] && (f3 & (TR3_POLEARM)))
    {
        bonus += 2;
    }

    return bonus;
}

/*
 * Determines the total damage side for archery
 * based on the weight of the bow, strength, and the sides of the bow
 */

extern byte total_ads(const object_type* j_ptr)
{
    byte ads;
    int int_ads; /* to allow negative values in the intermediate stages */
    int str_to_ads;

    str_to_ads = p_ptr->stat_use[A_STR];

    int_ads = j_ptr->ds;

    /* limit the strength sides bonus by bow weight */
    if ((str_to_ads > 0) && (str_to_ads > (j_ptr->weight / 10)))
    {
        int_ads += j_ptr->weight / 10;
    }
    else if ((str_to_ads < 0) && (str_to_ads < -(j_ptr->weight / 10)))
    {
        int_ads += -(j_ptr->weight / 10);
    }
    else
    {
        int_ads += str_to_ads;
    }

    // add archery damage bonus
    int_ads += p_ptr->to_ads;

    /* make sure the total is non-negative */
    ads = (int_ads < 0) ? 0 : int_ads;

    return (ads);
}

/*
 * Converts stat num into a two-char (right justified) string
 * Sil: rather pointless since stats no longer have and 18/XYZ format
 */
void cnv_stat(int val, char* out_val) { sprintf(out_val, "%2d", val); }
