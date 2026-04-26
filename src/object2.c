/* File: object2.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "supplies.h"

enum inventory_limit_group
{
    INV_LIMIT_NONE = 0,
    INV_LIMIT_ARROW,
    INV_LIMIT_BOW,
    INV_LIMIT_STAFF,
    INV_LIMIT_HORN,
    INV_LIMIT_DIGGING,
    INV_LIMIT_BOOTS,
    INV_LIMIT_GLOVES,
    INV_LIMIT_HELM_CROWN,
    INV_LIMIT_ROUND_SHIELD,
    INV_LIMIT_OTHER_SHIELD,
    INV_LIMIT_CLOAK,
    INV_LIMIT_SOFT_ARMOUR,
    INV_LIMIT_MAIL,
    INV_LIMIT_MELEE_WEAPON,
    INV_LIMIT_SUPPLY_WEIGHT,
    INV_LIMIT_TORCHES,
    INV_LIMIT_BRASS_LAMPS,
    INV_LIMIT_LESSER_JEWEL,
    INV_LIMIT_FEANORIAN_LAMP
};

static bool carry_limit_last_failed = false;
static enum inventory_limit_group carry_limit_last_group = INV_LIMIT_NONE;
static int carry_limit_last_limit = 0;
static char carry_limit_last_label[64];
static enum inventory_limit_group pack_limit_prompt_group = INV_LIMIT_NONE;

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

static void clear_inventory_limit_failure(void)
{
    carry_limit_last_failed = false;
    carry_limit_last_group = INV_LIMIT_NONE;
    carry_limit_last_limit = 0;
    carry_limit_last_label[0] = '\0';
}

static bool inven_index_valid(int item, cptr context)
{
    if ((item >= 0) && (item < INVEN_TOTAL))
        return true;

    log_error("%s: invalid inventory slot %d",
        context ? context : "inventory", item);
    return false;
}

static bool object_is_truly_two_handed(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    switch (o_ptr->tval)
    {
        case TV_HAFTED:
            return (o_ptr->sval == SV_QUARTERSTAFF);
        case TV_POLEARM:
            return (o_ptr->sval == SV_GREAT_SPEAR) || (o_ptr->sval == SV_GLAIVE)
                || (o_ptr->sval == SV_GREAT_AXE);
        case TV_SWORD:
            return (o_ptr->sval == SV_GREAT_SWORD)
                || (o_ptr->sval == SV_STAR_IRON_GREAT_SWORD);
        default:
            break;
    }

    return false;
}

static bool get_inventory_limit_info(const object_type* o_ptr,
                                     enum inventory_limit_group* group,
                                     int* limit,
                                     int* cost)
{
    enum inventory_limit_group local_group = INV_LIMIT_NONE;
    int local_limit = 0;
    int local_cost = 1;
    bool found = true;

    if (!o_ptr || !o_ptr->k_idx)
    {
        found = false;
    }
    else
    {
        switch (o_ptr->tval)
        {
            case TV_ARROW:
                local_group = INV_LIMIT_ARROW;
                local_limit = 2;
                break;
            case TV_BOW:
                local_group = INV_LIMIT_BOW;
                local_limit = 1;
                break;
            case TV_STAFF:
                local_group = INV_LIMIT_STAFF;
                local_limit = 1;
                break;
            case TV_HORN:
                local_group = INV_LIMIT_HORN;
                local_limit = 2;
                break;
            case TV_DIGGING:
                local_group = INV_LIMIT_DIGGING;
                local_limit = 1;
                break;
            case TV_BOOTS:
                local_group = INV_LIMIT_BOOTS;
                local_limit = 2;
                break;
            case TV_GLOVES:
                local_group = INV_LIMIT_GLOVES;
                local_limit = 2;
                break;
            case TV_HELM:
            case TV_CROWN:
                local_group = INV_LIMIT_HELM_CROWN;
                local_limit = 1;
                break;
            case TV_SHIELD:
                if (o_ptr->sval == SV_ROUND_SHIELD || o_ptr->sval == SV_BROKEN_SHIELD)
                {
                    local_group = INV_LIMIT_ROUND_SHIELD;
                    local_limit = 1;
                }
                else
                {
                    local_group = INV_LIMIT_OTHER_SHIELD;
                    local_limit = 0;
                }
                break;
            case TV_CLOAK:
                local_group = INV_LIMIT_CLOAK;
                local_limit = 3;
                break;
            case TV_SOFT_ARMOR:
                if (o_ptr->sval == SV_ROBE)
                {
                    local_group = INV_LIMIT_CLOAK;
                    local_limit = 3;
                }
                else
                {
                    local_group = INV_LIMIT_SOFT_ARMOUR;
                    local_limit = 1;
                }
                break;
            case TV_MAIL:
                local_group = INV_LIMIT_MAIL;
                local_limit = 0;
                break;
            case TV_HAFTED:
            case TV_POLEARM:
            case TV_SWORD:
                local_group = INV_LIMIT_MELEE_WEAPON;
                local_limit = 2;
                local_cost = object_is_truly_two_handed(o_ptr) ? 2 : 1;
                break;
            default:
                found = false;
                break;
        }
    }

    if (found)
    {
        if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR])
        {
            if (local_group == INV_LIMIT_MAIL
                || local_group == INV_LIMIT_HELM_CROWN
                || local_group == INV_LIMIT_ROUND_SHIELD
                || local_group == INV_LIMIT_OTHER_SHIELD)
            {
                local_limit += 1;
            }
        }
    }

    if (group)
        *group = local_group;
    if (limit)
        *limit = local_limit;
    if (cost)
        *cost = local_cost;

    return found;
}

static bool inventory_limit_counts_stacks(const object_type* o_ptr,
                                          enum inventory_limit_group group)
{
    if (group == INV_LIMIT_ARROW)
        return true;

    if ((group == INV_LIMIT_MELEE_WEAPON) && player_can_treat_as_throwing(o_ptr))
        return true;

    return false;
}

static bool inventory_limit_is_stack_counted(const object_type* o_ptr)
{
    enum inventory_limit_group group;

    return get_inventory_limit_info(o_ptr, &group, NULL, NULL)
        && inventory_limit_counts_stacks(o_ptr, group);
}

static int inventory_limit_usage(enum inventory_limit_group group)
{
    int usage = 0;

    if (group == INV_LIMIT_NONE)
        return 0;

    for (int idx = 0; idx <= INVEN_PACK; idx++)
    {
        object_type* slot_ptr = &inventory[idx];

        if (!slot_ptr->k_idx)
            continue;

        enum inventory_limit_group slot_group;
        int slot_limit;
        int slot_cost;

        if (!get_inventory_limit_info(slot_ptr, &slot_group, &slot_limit,
                                       &slot_cost))
            continue;

        if (slot_group != group)
            continue;

        if (inventory_limit_counts_stacks(slot_ptr, slot_group))
            usage += slot_cost;
        else
            usage += slot_cost * MAX(slot_ptr->number, 1);
    }

    return usage;
}

static void fill_inventory_limit_label(enum inventory_limit_group group,
                                       const object_type* o_ptr)
{
    switch (group)
    {
        case INV_LIMIT_ARROW:
            SDL_strlcpy(carry_limit_last_label, "arrow stacks",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_BOW:
            SDL_strlcpy(carry_limit_last_label, "bows",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_STAFF:
            SDL_strlcpy(carry_limit_last_label, "walking staves",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_HORN:
            SDL_strlcpy(carry_limit_last_label, "horns",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_DIGGING:
            SDL_strlcpy(carry_limit_last_label, "digging tools",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_BOOTS:
            SDL_strlcpy(carry_limit_last_label, "pairs of boots",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_GLOVES:
            SDL_strlcpy(carry_limit_last_label, "pairs of gloves",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_HELM_CROWN:
            SDL_strlcpy(carry_limit_last_label, "helms or crowns",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_ROUND_SHIELD:
            SDL_strlcpy(carry_limit_last_label, "round shields",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_OTHER_SHIELD:
            SDL_strlcpy(carry_limit_last_label, "non-round shields",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_CLOAK:
            SDL_strlcpy(carry_limit_last_label, "cloaks",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_SOFT_ARMOUR:
            SDL_strlcpy(carry_limit_last_label, "soft armour",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_MAIL:
            SDL_strlcpy(carry_limit_last_label, "mail armour",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_MELEE_WEAPON:
            if (object_is_truly_two_handed(o_ptr))
                SDL_strlcpy(carry_limit_last_label,
                          "two-handed melee weapons",
                          sizeof(carry_limit_last_label));
            else
                SDL_strlcpy(carry_limit_last_label, "melee weapons",
                          sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_SUPPLY_WEIGHT:
            SDL_strlcpy(carry_limit_last_label, "supply weight",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_TORCHES:
            SDL_strlcpy(carry_limit_last_label, "torches",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_BRASS_LAMPS:
            SDL_strlcpy(carry_limit_last_label, "oil container slots",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_LESSER_JEWEL:
        case INV_LIMIT_FEANORIAN_LAMP:
            SDL_strlcpy(carry_limit_last_label,
                "lesser jewels or Feanorian lamps",
                sizeof(carry_limit_last_label));
            break;
        default:
            SDL_strlcpy(carry_limit_last_label, "items of this type",
                      sizeof(carry_limit_last_label));
            break;
    }
}

static void set_inventory_limit_failure(enum inventory_limit_group group,
                                        int limit,
                                        const object_type* o_ptr);

static enum inventory_limit_group light_limit_group(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return INV_LIMIT_NONE;

    if (player_oil_container_object(o_ptr))
        return INV_LIMIT_BRASS_LAMPS;

    if (o_ptr->tval != TV_LIGHT)
        return INV_LIMIT_NONE;

    switch (o_ptr->sval)
    {
    case SV_LIGHT_TORCH:
    case SV_LIGHT_MALLORN:
        return INV_LIMIT_TORCHES;
    case SV_LIGHT_LESSER_JEWEL:
    case SV_LIGHT_FEANORIAN:
        return INV_LIMIT_LESSER_JEWEL;
    default:
        return INV_LIMIT_NONE;
    }
}

static bool player_light_capacity_okay(const object_type* o_ptr,
                                       bool record_failure)
{
    int cap;
    enum inventory_limit_group group;

    if (!o_ptr || !o_ptr->k_idx)
        return true;

    cap = player_light_carry_cap(o_ptr);
    if (cap <= 0)
        return true;

    if (player_light_available_capacity(o_ptr) >= o_ptr->number)
        return true;

    if (record_failure)
    {
        group = light_limit_group(o_ptr);
        if (group != INV_LIMIT_NONE)
            set_inventory_limit_failure(group, cap, o_ptr);
    }

    return false;
}

static void set_inventory_limit_failure(enum inventory_limit_group group,
                                        int limit,
                                        const object_type* o_ptr)
{
    carry_limit_last_failed = true;
    carry_limit_last_group = group;
    carry_limit_last_limit = limit;
    fill_inventory_limit_label(group, o_ptr);
}

bool inven_carry_limit_can_replace(const object_type* o_ptr)
{
    enum inventory_limit_group group;
    int limit;
    int cost;

    if (!carry_limit_last_failed)
        return false;

    if (carry_limit_last_limit <= 0)
        return false;

    if (!o_ptr)
        return false;

    if (carry_limit_last_group == INV_LIMIT_SUPPLY_WEIGHT)
    {
        return supplies_weight_counts_to_limit(o_ptr)
            && (o_ptr->weight > 0) && (MAX(o_ptr->number, 1) > 0);
    }

    if (o_ptr->k_idx
        && (o_ptr->tval == TV_LIGHT || o_ptr->tval == TV_FLASK))
    {
        group = light_limit_group(o_ptr);
        if (group == INV_LIMIT_NONE)
            return false;

        return (group == carry_limit_last_group) && (MAX(o_ptr->number, 1) > 0);
    }

    if (!get_inventory_limit_info(o_ptr, &group, &limit, &cost))
        return false;

    if (group != carry_limit_last_group)
        return false;

    return (cost > 0);
}

static bool inventory_type_slot_available(const object_type* o_ptr,
                                          bool record_failure)
{
    enum inventory_limit_group group;
    int limit;
    int cost;
    int units;

    if (!get_inventory_limit_info(o_ptr, &group, &limit, &cost))
        return true;

    if (limit <= 0)
    {
        if (record_failure)
            set_inventory_limit_failure(group, limit, o_ptr);
        return false;
    }

    units = inventory_limit_counts_stacks(o_ptr, group) ? 1
                                                        : MAX(o_ptr->number, 1);

    int used = inventory_limit_usage(group);

    if (used + cost * units <= limit)
        return true;

    if (record_failure)
        set_inventory_limit_failure(group, limit, o_ptr);

    return false;
}

static bool inventory_limit_group_is_heavy_armour(
    enum inventory_limit_group group)
{
    return (group == INV_LIMIT_MAIL) || (group == INV_LIMIT_HELM_CROWN)
        || (group == INV_LIMIT_ROUND_SHIELD)
        || (group == INV_LIMIT_OTHER_SHIELD);
}

static bool item_tester_hook_pack_limit_group(const object_type* o_ptr)
{
    enum inventory_limit_group group;
    int limit;
    int cost;

    if (!get_inventory_limit_info(o_ptr, &group, &limit, &cost))
        return false;

    return (group == pack_limit_prompt_group);
}

static int inventory_limit_group_first_slot(enum inventory_limit_group group,
    int* limit)
{
    for (int item = 0; item <= INVEN_PACK; item++)
    {
        object_type* o_ptr = &inventory[item];
        enum inventory_limit_group slot_group;
        int slot_limit;
        int slot_cost;

        if (!o_ptr->k_idx)
            continue;

        if (!get_inventory_limit_info(o_ptr, &slot_group, &slot_limit,
                &slot_cost))
            continue;

        if (slot_group != group)
            continue;

        if (limit)
            *limit = slot_limit;

        return item;
    }

    return -1;
}

static int inventory_limit_group_last_slot(enum inventory_limit_group group)
{
    for (int item = INVEN_PACK; item >= 0; item--)
    {
        object_type* o_ptr = &inventory[item];
        enum inventory_limit_group slot_group;
        int slot_limit;
        int slot_cost;

        if (!o_ptr->k_idx)
            continue;

        if (!get_inventory_limit_info(o_ptr, &slot_group, &slot_limit,
                &slot_cost))
            continue;

        if (slot_group == group)
            return item;
    }

    return -1;
}

static cptr inventory_limit_group_drop_prompt(enum inventory_limit_group group)
{
    switch (group)
    {
        case INV_LIMIT_HELM_CROWN:
            return "Drop which helm or crown? ";
        case INV_LIMIT_ROUND_SHIELD:
            return "Drop which round shield? ";
        case INV_LIMIT_OTHER_SHIELD:
            return "Drop which shield? ";
        case INV_LIMIT_MAIL:
            return "Drop which mail armour? ";
        case INV_LIMIT_HORN:
            return "Drop which horn? ";
        default:
            return "Drop which excess item? ";
    }
}

static cptr inventory_limit_group_label(enum inventory_limit_group group,
    int limit)
{
    switch (group)
    {
        case INV_LIMIT_HELM_CROWN:
            return (limit == 1) ? "helm or crown" : "helms or crowns";
        case INV_LIMIT_ROUND_SHIELD:
            return (limit == 1) ? "round shield" : "round shields";
        case INV_LIMIT_OTHER_SHIELD:
            return (limit == 1) ? "shield" : "shields";
        case INV_LIMIT_MAIL:
            return "mail armour";
        case INV_LIMIT_HORN:
            return (limit == 1) ? "horn" : "horns";
        default:
            return (limit == 1) ? "item of this type"
                                : "items of this type";
    }
}

void inven_enforce_current_pack_limits(void)
{
    static const enum inventory_limit_group heavy_armour_groups[] = {
        INV_LIMIT_MAIL,
        INV_LIMIT_OTHER_SHIELD,
        INV_LIMIT_HELM_CROWN,
        INV_LIMIT_ROUND_SHIELD,
    };

    if (!character_generated || character_xtra || character_icky
        || p_ptr->is_dead)
    {
        return;
    }

    for (size_t i = 0; i < N_ELEMENTS(heavy_armour_groups); i++)
    {
        enum inventory_limit_group group = heavy_armour_groups[i];
        bool warned = false;

        while (true)
        {
            int limit = 0;
            int item = inventory_limit_group_first_slot(group, &limit);
            int used;

            if (item < 0)
                break;

            used = inventory_limit_usage(group);
            if (used <= limit)
                break;

            if (!inventory_limit_group_is_heavy_armour(group))
                break;

            if (!warned)
            {
                if (limit > 0)
                {
                    msg_format("Your pack can now hold only %d %s.", limit,
                        inventory_limit_group_label(group, limit));
                }
                else
                {
                    msg_format("Your pack can no longer hold %s.",
                        inventory_limit_group_label(group, limit));
                }

                warned = true;
            }

            if (limit > 0)
            {
                bool old_item_tester_full = item_tester_full;
                byte old_item_tester_tval = item_tester_tval;
                bool (*old_item_tester_hook)(const object_type*)
                    = item_tester_hook;

                item_tester_full = false;
                item_tester_tval = 0;
                pack_limit_prompt_group = group;
                item_tester_hook = item_tester_hook_pack_limit_group;

                if (!get_item(&item, inventory_limit_group_drop_prompt(group),
                        "You have nothing suitable to drop.", USE_INVEN))
                {
                    item = inventory_limit_group_last_slot(group);
                    if (item >= 0)
                        msg_print("No choice made; dropping one excess item.");
                }

                pack_limit_prompt_group = INV_LIMIT_NONE;
                item_tester_hook = old_item_tester_hook;
                item_tester_tval = old_item_tester_tval;
                item_tester_full = old_item_tester_full;
            }

            if ((item < 0) || (item >= INVEN_WIELD) || !inventory[item].k_idx)
                break;

            inven_drop(item, 1);
            handle_stuff();
        }
    }
}

int object_stack_limit(const object_type* o_ptr)
{
    if (!o_ptr)
        return MAX_STACK_SIZE - 1;

    if (o_ptr->tval == TV_RING)
        return 1;

    if (o_ptr->tval == TV_SWORD && o_ptr->sval == SV_DAGGER)
        return 7;

    if (o_ptr->tval == TV_POLEARM && o_ptr->sval == SV_SPEAR)
        return 5;

    if (o_ptr->tval == TV_POLEARM && o_ptr->sval == SV_HAND_AXE)
        return 3;

    if (o_ptr->tval == TV_ARROW)
        return 48;

    if (o_ptr->tval == TV_HORN)
        return 1;

    return MAX_STACK_SIZE - 1;
}





/*
 * Excise a dungeon object from any stacks
 */
void excise_object_idx(int o_idx)
{
    object_type* j_ptr;

    s16b this_o_idx, next_o_idx = 0;

    s16b prev_o_idx = 0;

    /* Object */
    j_ptr = &o_list[o_idx];

    /* Monster */
    if (j_ptr->held_m_idx)
    {
        monster_type* m_ptr;

        /* Monster */
        m_ptr = &mon_list[j_ptr->held_m_idx];

        /* Scan all objects in the grid */
        for (this_o_idx = m_ptr->hold_o_idx; this_o_idx;
             this_o_idx = next_o_idx)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[this_o_idx];

            /* Get the next object */
            next_o_idx = o_ptr->next_o_idx;

            /* Done */
            if (this_o_idx == o_idx)
            {
                /* No previous */
                if (prev_o_idx == 0)
                {
                    /* Remove from list */
                    m_ptr->hold_o_idx = next_o_idx;
                }

                /* Real previous */
                else
                {
                    object_type* i_ptr;

                    /* Previous object */
                    i_ptr = &o_list[prev_o_idx];

                    /* Remove from list */
                    i_ptr->next_o_idx = next_o_idx;
                }

                /* Forget next pointer */
                o_ptr->next_o_idx = 0;

                /* Done */
                break;
            }

            /* Save prev_o_idx */
            prev_o_idx = this_o_idx;
        }
    }

    /* Dungeon */
    else
    {
        int y = j_ptr->iy;
        int x = j_ptr->ix;

        /* Scan all objects in the grid */
        for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[this_o_idx];

            /* Get the next object */
            next_o_idx = o_ptr->next_o_idx;

            /* Done */
            if (this_o_idx == o_idx)
            {
                /* No previous */
                if (prev_o_idx == 0)
                {
                    /* Remove from list */
                    cave_o_idx[y][x] = next_o_idx;
                }

                /* Real previous */
                else
                {
                    object_type* i_ptr;

                    /* Previous object */
                    i_ptr = &o_list[prev_o_idx];

                    /* Remove from list */
                    i_ptr->next_o_idx = next_o_idx;
                }

                /* Forget next pointer */
                o_ptr->next_o_idx = 0;

                /* Done */
                break;
            }

            /* Save prev_o_idx */
            prev_o_idx = this_o_idx;
        }
    }
}

/*
 * Delete a dungeon object
 *
 * Handle "stacks" of objects correctly.
 */
void delete_object_idx(int o_idx)
{
    object_type* j_ptr;

    /* Excise */
    excise_object_idx(o_idx);

    /* Object */
    j_ptr = &o_list[o_idx];

    /* Dungeon floor */
    if (!(j_ptr->held_m_idx))
    {
        int y, x;

        /* Location */
        y = j_ptr->iy;
        x = j_ptr->ix;

        /* Visual update */
        lite_spot(y, x);
    }

    /* Wipe the object */
    object_wipe(j_ptr);

    /* Count objects */
    o_cnt--;
}

/*
 * Hack -- determine if a template is a damaged item
 *
 */
static bool kind_is_damaged_item(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];
    return k_ptr->flags3 & TR3_DAMAGED;
}

#if 0
/*
 * Hack -- determine if a template is not a damaged item or skeleton
 */
static bool kind_is_not_damaged(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];
    return k_ptr->tval != TV_SKELETON && !kind_is_damaged_item(k_idx);
}
#endif

/*
 * Deletes all objects at given location
 */
void delete_object(int y, int x)
{
    s16b this_o_idx, next_o_idx = 0;

    /* Paranoia */
    if (!in_bounds(y, x))
        return;

    /* Scan all objects in the grid */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Wipe the object */
        object_wipe(o_ptr);

        /* Count objects */
        o_cnt--;
    }

    /* Objects are gone */
    cave_o_idx[y][x] = 0;

    /* Visual update */
    lite_spot(y, x);
}

/*
 * Move an object from index i1 to index i2 in the object list
 */
static void compact_objects_aux(int i1, int i2)
{
    int i;

    object_type* o_ptr;

    /* Do nothing */
    if (i1 == i2)
        return;

    /* Repair objects */
    for (i = 1; i < o_max; i++)
    {
        /* Get the object */
        o_ptr = &o_list[i];

        /* Skip "dead" objects */
        if (!o_ptr->k_idx)
            continue;

        /* Repair "next" pointers */
        if (o_ptr->next_o_idx == i1)
        {
            /* Repair */
            o_ptr->next_o_idx = i2;
        }
    }

    /* Get the object */
    o_ptr = &o_list[i1];

    /* Monster */
    if (o_ptr->held_m_idx)
    {
        monster_type* m_ptr;

        /* Get the monster */
        m_ptr = &mon_list[o_ptr->held_m_idx];

        /* Repair monster */
        if (m_ptr->hold_o_idx == i1)
        {
            /* Repair */
            m_ptr->hold_o_idx = i2;
        }
    }

    /* Dungeon */
    else
    {
        int y, x;

        /* Get location */
        y = o_ptr->iy;
        x = o_ptr->ix;

        /* Repair grid */
        if (cave_o_idx[y][x] == i1)
        {
            /* Repair */
            cave_o_idx[y][x] = i2;
        }
    }

    /* Hack -- move object */
    memcpy(&o_list[i2], &o_list[i1], sizeof(object_type));

    /* Hack -- wipe hole */
    object_wipe(o_ptr);
}

/*
 * Compact and Reorder the object list
 *
 * This function can be very dangerous, use with caution!
 *
 * When actually "compacting" objects, we base the saving throw on a
 * combination of object level, distance from player, and current
 * "desperation".
 *
 * After "compacting" (if needed), we "reorder" the objects into a more
 * compact order, and we reset the allocation info, and the "live" array.
 */
void compact_objects(int size)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i, y, x, num, cnt;

    int cur_lev, cur_dis, chance;

    /* Compact */
    if (size)
    {
        /* Message */
        msg_print("Compacting objects...");

        /* Redraw map */
        p_ptr->redraw |= (PR_MAP);

        /* Window stuff */
        p_ptr->window |= (PW_OVERHEAD);
    }

    /* Compact at least 'size' objects */
    for (num = 0, cnt = 1; num < size; cnt++)
    {
        bool saw_non_artefact = false;

        /* Get more vicious each iteration */
        cur_lev = 5 * cnt;

        /* Get closer each iteration */
        cur_dis = 5 * (20 - cnt);

        /* Examine the objects */
        for (i = 1; i < o_max; i++)
        {
            object_type* o_ptr = &o_list[i];

            object_kind* k_ptr = &k_info[o_ptr->k_idx];

            /* Skip dead objects */
            if (!o_ptr->k_idx)
                continue;

            /* Never compact artefacts; dropped artefacts must not disappear. */
            if (artefact_p(o_ptr))
                continue;

            saw_non_artefact = true;

            /* Hack -- High level objects start out "immune" */
            if ((k_ptr->level > cur_lev) && (k_ptr->squelch != SQUELCH_ALWAYS))
                continue;

            /* Monster */
            if (o_ptr->held_m_idx)
            {
                monster_type* m_ptr;

                /* Get the monster */
                m_ptr = &mon_list[o_ptr->held_m_idx];

                /* Get the location */
                y = m_ptr->fy;
                x = m_ptr->fx;

                /* Monsters protect their objects */
                if (percent_chance(90) && (k_ptr->squelch != SQUELCH_ALWAYS))
                    continue;
            }

            /* Dungeon */
            else
            {
                /* Get the location */
                y = o_ptr->iy;
                x = o_ptr->ix;
            }

            /* Nearby objects start out "immune" */
            if ((cur_dis > 0) && (distance(py, px, y, x) < cur_dis)
                && (k_ptr->squelch != SQUELCH_ALWAYS))
                continue;

            /* Saving throw */
            chance = 90;

            /* Squelched items get compacted */
            if ((k_ptr->aware) && (k_ptr->squelch == SQUELCH_ALWAYS))
                chance = 0;

            /* Apply the saving throw */
            if (percent_chance(chance))
                continue;

            /* Delete the object */
            delete_object_idx(i);

            /* Count it */
            num++;
        }

        /* Avoid looping forever when only artefacts remain. */
        if (!saw_non_artefact)
            break;
    }

    /* Excise dead objects (backwards!) */
    for (i = o_max - 1; i >= 1; i--)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip real objects */
        if (o_ptr->k_idx)
            continue;

        /* Move last object into open hole */
        compact_objects_aux(o_max - 1, i);

        /* Compress "o_max" */
        o_max--;
    }
}

/*
 * Delete all the items when player leaves the level
 *
 * Note -- we do NOT visually reflect these (irrelevant) changes
 *
 * Hack -- we clear the "cave_o_idx[y][x]" field for every grid,
 * and the "m_ptr->next_o_idx" field for every monster, since
 * we know we are clearing every object.  Technically, we only
 * clear those fields for grids/monsters containing objects,
 * and we clear it once for every such object.
 */
void wipe_o_list(void)
{
    int i;

    /* Delete the existing objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Unseen artefacts should be eligible to spawn again on a later level.
         * Keep cur_num only for artefacts the player actually found or saw. */
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];
            if (!(a_ptr->seen & ART_SEEN_PHYSICAL) && !a_ptr->found_num)
                a_ptr->cur_num = 0;
        }

        /* Monster */
        if (o_ptr->held_m_idx)
        {
            monster_type* m_ptr;

            /* Monster */
            m_ptr = &mon_list[o_ptr->held_m_idx];

            /* Hack -- see above */
            m_ptr->hold_o_idx = 0;
        }

        /* Dungeon */
        else
        {
            /* Get the location */
            int y = o_ptr->iy;
            int x = o_ptr->ix;

            /* Hack -- see above */
            cave_o_idx[y][x] = 0;
        }

        /*Wipe the randart if necessary*/
        if (o_ptr->name1)
            artefact_wipe(o_ptr->name1);

        /* Wipe the object */
        memset(o_ptr, 0, sizeof(object_type));
    }

    /* Reset "o_max" */
    o_max = 1;

    /* Reset "o_cnt" */
    o_cnt = 0;
}

/*
 * Get and return the index of a "free" object.
 *
 * This routine should almost never fail, but in case it does,
 * we must be sure to handle "failure" of this routine.
 */
s16b o_pop(void)
{
    int attempt;
    int i;

    for (attempt = 0; attempt < 2; attempt++)
    {
        /* Initial allocation */
        if (o_max < z_info->o_max)
        {
            /* Get next space */
            i = o_max;

            /* Expand object array */
            o_max++;

            /* Count objects */
            o_cnt++;

            /* Use this object */
            return (i);
        }

        /* Recycle dead objects */
        for (i = 1; i < o_max; i++)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[i];

            /* Skip live objects */
            if (o_ptr->k_idx)
                continue;

            /* Count objects */
            o_cnt++;

            /* Use this object */
            return (i);
        }

        /* Make space by compacting ordinary objects, then retry once. */
        if (attempt == 0)
            compact_objects(1);
    }

    /* Warn the player (except during dungeon creation) */
    if (character_dungeon)
        msg_print("Too many objects!");

    /* Oops */
    return (0);
}

/*
 * Get the first object at a dungeon location
 * or NULL if there isn't one.
 */
object_type* get_first_object(int y, int x)
{
    s16b o_idx = cave_o_idx[y][x];

    if (o_idx)
        return (&o_list[o_idx]);

    /* No object */
    return (NULL);
}

/*
 * Get the next object in a stack or
 * NULL if there isn't one.
 */
object_type* get_next_object(const object_type* o_ptr)
{
    if (o_ptr->next_o_idx)
        return (&o_list[o_ptr->next_o_idx]);

    /* No more objects */
    return (NULL);
}

/*
 * Apply a "object restriction function" to the "object allocation table"
 */
void get_obj_num_prep(void)
{
    int i;

    /* Get the entry */
    alloc_entry* table = alloc_kind_table;

    /* Scan the allocation table */
    for (i = 0; i < alloc_kind_size; i++)
    {
        /* Accept objects which pass the restriction, if any */
        if (!get_obj_num_hook)
        {
            // damaged items only on skeletons
            if (kind_is_damaged_item(table[i].index))
                table[i].prob2 = 0;
            else
                table[i].prob2 = table[i].prob1;
        }
        else if ((*get_obj_num_hook)(table[i].index))
        {
            /* Accept this object */
            table[i].prob2 = table[i].prob1;
        }
        /* Do not use this object */
        else
        {
            /* Decline this object */
            table[i].prob2 = 0;
        }
    }
}

/*
 * Choose an object kind that seems "appropriate" to the given level
 *
 * This function uses the "prob2" field of the "object allocation table",
 * and various local information, to calculate the "prob3" field of the
 * same table, which is then used to choose an "appropriate" object, in
 * a relatively efficient manner.
 *
 * It is (slightly) more likely to acquire an object of the given level
 * than one of a lower level.  This is done by choosing several objects
 * appropriate to the given level and keeping the "hardest" one.
 *
 * Note that if no objects are "appropriate", then this function will
 * fail, and return zero, but this should *almost* never happen.
 * (but it does happen with certain themed items occasionally). -JG
 */
s16b get_obj_num(int level)
{
    int i, j, p;

    int k_idx;

    long value, total;

    object_kind* k_ptr;

    alloc_entry* table = alloc_kind_table;

    /* Boost level */
    if (level > 0)
    {
        /* Occasional "boost" */
        if (one_in_(GREAT_OBJ))
        {
            // most of the time, choose a new deeper depth, weighted towards the
            // current depth
            if (level < MORGOTH_DEPTH)
            {
                int x = rand_range(level + 1, MORGOTH_DEPTH);
                int y = rand_range(level + 1, MORGOTH_DEPTH);

                level = MIN(x, y);
            }

            // but if it was already very deep, just increment it
            else
            {
                level++;
            }
        }
    }

    /* Reset total */
    total = 0L;

    /* Process probabilities */
    for (i = 0; i < alloc_kind_size; i++)
    {
        /* Objects are sorted by depth */
        if (table[i].level > level)
            break;

        /* Default */
        table[i].prob3 = 0;

        /* Get the index */
        k_idx = table[i].index;

        /* Get the actual kind */
        k_ptr = &k_info[k_idx];

        /* Hack -- prevent embedded chests*/
        if ((object_generation_mode == OB_GEN_MODE_CHEST)
            && (k_ptr->tval == TV_CHEST))
            continue;

        /* Accept */
        table[i].prob3 = table[i].prob2;

        /* Total */
        total += table[i].prob3;
    }

    /* No legal objects */
    if (total <= 0)
        return (0);

    /* Pick an object */
    value = rand_int(total);

    /* Find the object */
    for (i = 0; i < alloc_kind_size; i++)
    {
        /* Found the entry */
        if (value < table[i].prob3)
            break;

        /* Decrement */
        value = value - table[i].prob3;
    }

    /* Power boost */
    p = rand_int(100);

    /* Try for a "better" object once (50%) or twice (10%) */
    if (p < 60)
    {
        /* Save old */
        j = i;

        /* Pick a object */
        value = rand_int(total);

        /* Find the monster */
        for (i = 0; i < alloc_kind_size; i++)
        {
            /* Found the entry */
            if (value < table[i].prob3)
                break;

            /* Decrement */
            value = value - table[i].prob3;
        }

        /* Keep the "best" one */
        if (table[i].level < table[j].level)
            i = j;
    }

    /* Try for a "better" object twice (10%) */
    if (p < 10)
    {
        /* Save old */
        j = i;

        /* Pick a object */
        value = rand_int(total);

        /* Find the object */
        for (i = 0; i < alloc_kind_size; i++)
        {
            /* Found the entry */
            if (value < table[i].prob3)
                break;

            /* Decrement */
            value = value - table[i].prob3;
        }

        /* Keep the "best" one */
        if (table[i].level < table[j].level)
            i = j;
    }

    /* Result */
    return (table[i].index);
}

/*
 * Known is true when the "attributes" of an object are "known".
 *
 * These attributes include tohit, todam, toac, cost, and pval (charges).
 *
 * Note that "knowing" an object gives you everything that an "awareness"
 * gives you, and much more.  In fact, the player is always "aware" of any
 * item which he "knows", except items in stores.
 *
 * But having full knowledge of, say, one "staff of Sanctity", does not, by
 * itself, give you knowledge, or even awareness, of other "staffs of Sanctity".
 * It happens that most "identify" routines (including "buying from a shop")
 * will make the player "aware" of the object as well as "know" it.
 *
 * This routine also removes any inscriptions generated by "feelings".
 */
void object_known(object_type* o_ptr)
{
    /* Remove special inscription, if any */
    if (o_ptr->discount >= INSCRIP_NULL)
        o_ptr->discount = 0;

    /* The object is not "sensed" */
    o_ptr->ident &= ~(IDENT_SENSE);

    /* Now we know about the item */
    o_ptr->ident |= (IDENT_KNOWN);
}

/*
 * The player is now aware of the effects of the given object.
 */
void object_aware(object_type* o_ptr)
{
    int x, y;
    bool flag = k_info[o_ptr->k_idx].aware;
    bool quiet_awareness = !character_generated || character_xtra || character_icky;

    /* Fully aware of the effects */
    k_info[o_ptr->k_idx].aware = true;

    // If newly aware
    if (!flag && !p_ptr->leaving)
    {
        if (!quiet_awareness)
        {
            char o_name[120];

            // gain experience for identification
            int new_exp = 75;
            gain_exp(new_exp);
            p_ptr->ident_exp += new_exp;

            object_desc(o_name, sizeof(o_name), o_ptr, true, 0);
            msg_format("The true virtue of %s is unveiled to you, and 75 experience is won.",
                o_name);
        }

        // remove any autoinscription
        obliterate_autoinscription(o_ptr->k_idx);
    }

    /* If newly aware and squelched, must rearrange stacks */
    if ((!flag) && (k_info[o_ptr->k_idx].squelch == SQUELCH_ALWAYS))
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            for (y = 0; y < p_ptr->cur_map_hgt; y++)
            {
                rearrange_stack(y, x);
            }
        }
    }
}

/*
 * Something has been "sampled"
 */
void object_tried(object_type* o_ptr)
{
    /* Mark it as tried (even if "aware") */
    k_info[o_ptr->k_idx].tried = true;
}

/*
 * Return the "value" of an "unknown" item
 * Make a guess at the value of non-aware items
 */
static s32b object_value_base(const object_type* o_ptr)
{
    int value = 0;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Use template cost for aware objects */
    if (object_aware_p(o_ptr))
    {
        /* Give credit for hit bonus */
        value += ((o_ptr->att - k_ptr->att) * 100L);

        /* Give credit for evasion bonus */
        value += ((o_ptr->evn - k_ptr->evn) * 100L);

        /* Give credit for sides bonus */
        value += ((o_ptr->ps - k_ptr->ps) * o_ptr->pd * 100L);

        /* Give credit for dice bonus */
        value += ((o_ptr->pd - k_ptr->pd) * o_ptr->ps * 100L);

        /* Give credit for sides bonus */
        value += ((o_ptr->ds - k_ptr->ds) * 100L);

        /* Give credit for dice bonus */
        value += ((o_ptr->dd - k_ptr->dd) * o_ptr->ds * 100L);

        // Arrows are worth less since they are perishable
        if (o_ptr->tval == TV_ARROW)
            value /= 10;

        // add in the base cost from the template
        value += k_ptr->cost;
    }

    else
    {
        /* Analyze the type */
        switch (o_ptr->tval)
        {
        /* Un-aware Food */
        case TV_FOOD:
            return (5L);

        /* Un-aware Potions */
        case TV_POTION:
            return (20L);

        /* Un-aware Staffs */
        case TV_STAFF:
            return (70L);

        /* Un-aware Rods */
        case TV_HORN:
            return (90L);

        /* Un-aware Rings */
        case TV_RING:
            return (45L);

        /* Un-aware Amulets */
        case TV_AMULET:
            return (45L);
        }
    }

    return (value);
}

/*
 * Return the "real" price of a "known" item, not including discounts.
 *
 * Wand and staffs get cost for each charge.
 *
 * Armor is worth an extra 100 gold per bonus point to armor class.
 *
 * Weapons are worth an extra 100 gold per bonus point (AC,TH,TD).
 *
 * Missiles are only worth 5 gold per bonus point, since they
 * usually appear in groups of 20, and we want the player to get
 * the same amount of cash for any "equivalent" item.  Note that
 * missiles never have any of the "pval" flags, and in fact, they
 * only have a few of the available flags, primarily of the "slay"
 * and "brand" and "ignore" variety.
 *
 * Weapons with negative hit+damage bonuses are worthless.
 *
 * Every wearable item with a "pval" bonus is worth extra (see below).
 */
static s32b object_value_real(const object_type* o_ptr)
{
    s32b value;

    u32b f1, f2, f3;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Hack -- "worthless" items */
    if (!k_ptr->cost)
        return (0L);

    /* Base cost */
    value = k_ptr->cost;

    /* Extract some flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Artefact */
    if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];

        /* Hack -- "worthless" artefacts */
        if (!a_ptr->cost)
            return (0L);

        /* Hack -- Use the artefact cost instead */
        value = a_ptr->cost;
    }

    /* Ego-Items (prefix and/or suffix) */
    else if (object_has_ego(o_ptr))
    {
        byte ego_prefix = object_ego_prefix(o_ptr);
        if (ego_prefix)
        {
            ego_item_type* e_ptr = &e_info[ego_prefix];
            if (!e_ptr->cost)
                return (0L);
            value += e_ptr->cost;
        }

        byte ego_suffix = object_ego_suffix(o_ptr);
        if (ego_suffix)
        {
            ego_item_type* e_ptr = &e_info[ego_suffix];
            if (!e_ptr->cost)
                return (0L);
            value += e_ptr->cost;
        }
    }

    /* Analyze pval bonus */
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    case TV_LIGHT:
    case TV_AMULET:
    case TV_RING:
    {
        /* Hack -- Negative "pval" is always bad */
        if (o_ptr->pval < 0)
            return (0L);

        /* Give credit for stat bonuses */
        if (f1 & (TR1_STR | TR1_NEG_STR))
            value += ((s32b)o_ptr->stat_bonus[A_STR] * 300L);
        if (f1 & (TR1_DEX | TR1_NEG_DEX))
            value += ((s32b)o_ptr->stat_bonus[A_DEX] * 300L);
        if (f1 & (TR1_CON | TR1_NEG_CON))
            value += ((s32b)o_ptr->stat_bonus[A_CON] * 300L);
        if (f1 & (TR1_GRA | TR1_NEG_GRA))
            value += ((s32b)o_ptr->stat_bonus[A_GRA] * 300L);

        /* Give credit for skills */
        if (f1 & (TR1_MEL))
            value += ((s32b)o_ptr->skill_bonus[S_MEL] * 100L);
        if (f1 & (TR1_ARC))
            value += ((s32b)o_ptr->skill_bonus[S_ARC] * 100L);
        if (f1 & (TR1_STL))
            value += ((s32b)o_ptr->skill_bonus[S_STL] * 100L);
        if (f1 & (TR1_PER))
            value += ((s32b)o_ptr->skill_bonus[S_PER] * 100L);
        if (f1 & (TR1_WIL))
            value += ((s32b)o_ptr->skill_bonus[S_WIL] * 100L);
        if (f1 & (TR1_SMT))
            value += ((s32b)o_ptr->skill_bonus[S_SMT] * 100L);
        if (f1 & (TR1_SNG))
            value += ((s32b)o_ptr->skill_bonus[S_SNG] * 100L);

        /* Give credit for tunneling */
        if (f1 & (TR1_TUNNEL))
            value += (o_ptr->pval * 50L);

        /* Give credit for speed bonus */
        if (f2 & (TR2_SPEED))
            value += 1000L;

        break;
    }
    }

    /* Analyze the item */
    switch (o_ptr->tval)
    {
    /* Staffs and Gems */
    case TV_STAFF:
    case TV_GEM:
    {
        /* Pay extra for charges, depending on standard number of
         * charges.  Handle new-style wands correctly.
         */
        value += ((value / 20) * (o_ptr->pval / o_ptr->number));

        /* Done */
        break;
    }

    /* Rings/Amulets */
    case TV_RING:
    case TV_AMULET:
    {
        /* Hack -- negative bonuses are bad */
        if (o_ptr->att < 0)
            return (0L);
        if (o_ptr->evn < 0)
            return (0L);

        /* Give credit for bonuses */
        value += ((o_ptr->att + o_ptr->evn + o_ptr->ps) * 100L);

        /* Done */
        break;
    }

    /* Armor */
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_SHIELD:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        /* Give credit for hit bonus */
        value += ((o_ptr->att - k_ptr->att) * 100L);

        /* Give credit for evasion bonus */
        value += ((o_ptr->evn - k_ptr->evn) * 100L);

        /* Give credit for sides bonus */
        value += ((o_ptr->ps - k_ptr->ps) * o_ptr->pd * 50L);

        /* Give credit for dice bonus */
        value += ((o_ptr->pd - k_ptr->pd) * o_ptr->ps * 50L);

        /* Done */
        break;
    }

    /* Bows/Weapons */
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_SWORD:
    case TV_POLEARM:
    {
        /* Give credit for hit bonus */
        value += ((o_ptr->att - k_ptr->att) * 100L);

        /* Give credit for evasion bonus */
        value += ((o_ptr->evn - k_ptr->evn) * 100L);

        /* Give credit for sides bonus */
        value += ((o_ptr->ds - k_ptr->ds) * o_ptr->dd * 51L);

        /* Give credit for dice bonus */
        value += ((o_ptr->dd - k_ptr->dd) * o_ptr->ds * 51L);

        /* Done */
        break;
    }

    /* Arrows */
    case TV_ARROW:
    {
        /* Give credit for hit bonus */
        value += ((o_ptr->att - k_ptr->att) * 10L);

        /* Done */
        break;
    }
    }

    /* No negative value */
    if (value < 0)
        value = 0;

    /* Return the value */
    return (value);
}

bool object_has_ego_flag4(const object_type* o_ptr, u32b flag)
{
    byte ego_prefix;
    byte ego_suffix;

    if (!o_ptr || !flag)
        return false;

    ego_prefix = object_ego_prefix(o_ptr);
    if (ego_prefix && (e_info[ego_prefix].flags4 & flag))
        return true;

    ego_suffix = object_ego_suffix(o_ptr);
    if (ego_suffix && (e_info[ego_suffix].flags4 & flag))
        return true;

    return false;
}

/*
 * Return the price of an item including plusses (and charges).
 *
 * This function returns the "value" of the given item (qty one).
 *
 * Never notice "unknown" bonuses or properties, including "curses",
 * since that would give the player information he did not have.
 *
 * Note that discounted items stay discounted forever.
 */
s32b object_value(const object_type* o_ptr)
{
    s32b value;

    /* Known items -- acquire the actual value */
    if (object_known_p(o_ptr))
    {
        /* Broken items -- worthless */
        if (broken_p(o_ptr))
            return (0L);

        /* Cursed items -- worthless */
        if (cursed_p(o_ptr))
            return (0L);

        /* Real value (see above) */
        value = object_value_real(o_ptr);
    }

    /* Unknown items -- acquire the base value */
    else
    {
        /* Hack -- Felt broken items */
        if ((o_ptr->ident & (IDENT_SENSE)) && broken_p(o_ptr))
            return (0L);

        /* Hack -- Felt cursed items */
        if ((o_ptr->ident & (IDENT_SENSE)) && cursed_p(o_ptr))
            return (0L);

        /* Base value (see above) */
        value = object_value_base(o_ptr);
    }

    /* Return the final value */
    return (value);
}

/*
 * Determine if an item can "absorb" a second item
 *
 * See "object_absorb()" for the actual "absorption" code.
 *
 * If permitted, we allow wands/staffs (if they are known to have equal
 * charges) and rods (if fully charged) to combine.  They will unstack
 * (if necessary) when they are used.
 *
 * If permitted, we allow weapons/armor to stack, if fully "known".
 *
 * Missiles will combine if both stacks have the same "known" status.
 * This is done to make unidentified stacks of missiles useful.
 *
 * Food, potions, and "easy know" items always stack.
 *
 * Chests, and activatable items, except rods, never stack (for various
 * reasons).
 */
bool object_similar(const object_type* o_ptr, const object_type* j_ptr)
{
    /* Require identical object types */
    if (o_ptr->k_idx != j_ptr->k_idx)
        return (false);

    /* Require identical weight */
    if (!(o_ptr->weight == j_ptr->weight))
        return (false);

    /* Analyze the items */
    switch (o_ptr->tval)
    {
    /* Chests */
    case TV_SKELETON:
    case TV_CHEST:
    {
        /* Never okay */
        return (false);
    }

    /* Food, Potions, and Gems */
    case TV_FOOD:
    case TV_POTION:
    case TV_GEM:
    {
        /* Assume okay */
        break;
    }

    /* Staves */
    case TV_STAFF:
    {
        /* Don't merge as it messes with charges etc. */
        return (false);
    }

    /* Horns */
    case TV_HORN:
    {
        /* Assume okay */
        break;
    }

    /* Rings, Amulets, Lites and Books */
    case TV_RING:
    case TV_AMULET:
    case TV_LIGHT:
    {
        /* Require both items to be known */
        if (!object_known_p(o_ptr) || !object_known_p(j_ptr))
            return (false);

        __attribute__((fallthrough));
    }

    /* Weapons and Armor */
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        __attribute__((fallthrough));
    }

    /* Missiles & most things from above */
    case TV_ARROW:
    {
        /* Require identical knowledge of both items */
        if (object_known_p(o_ptr) != object_known_p(j_ptr))
            return (false);

        /* Require identical "bonuses" */
        if (o_ptr->att != j_ptr->att)
            return (false);
        if (o_ptr->evn != j_ptr->evn)
            return (false);
        if (o_ptr->ds != j_ptr->ds)
            return (false);
        if (o_ptr->dd != j_ptr->dd)
            return (false);

        // only check protection if at least one item has it
        if ((o_ptr->pd * o_ptr->ps > 0) || (j_ptr->pd * j_ptr->ps > 0))
        {
            if (o_ptr->ps != j_ptr->ps)
                return (false);
            if (o_ptr->pd != j_ptr->pd)
                return (false);
        }

        /* Require identical "pval" code */
        if (o_ptr->pval != j_ptr->pval)
            return (false);

        /* Require identical per-stat/skill bonuses */
        if (memcmp(o_ptr->stat_bonus, j_ptr->stat_bonus, sizeof(o_ptr->stat_bonus)) != 0)
            return (false);
        if (memcmp(o_ptr->skill_bonus, j_ptr->skill_bonus, sizeof(o_ptr->skill_bonus)) != 0)
            return (false);

        /* Require identical "artefact" names */
        if (o_ptr->name1 != j_ptr->name1)
            return (false);

        log_debug("object_similar: checking egos - o_ptr prefix=%d suffix=%d, j_ptr prefix=%d suffix=%d",
                  (int)object_ego_prefix(o_ptr), (int)object_ego_suffix(o_ptr),
                  (int)object_ego_prefix(j_ptr), (int)object_ego_suffix(j_ptr));

        /* Require identical ego affixes */
        if (object_ego_prefix(o_ptr) != object_ego_prefix(j_ptr)
            || object_ego_suffix(o_ptr) != object_ego_suffix(j_ptr))
        {
            log_debug("object_similar: DIFFERENT egos, returning false");
            return (false);
        }

        log_debug("object_similar: checking timeout - o_ptr=%d, j_ptr=%d",
                  o_ptr->timeout, j_ptr->timeout);

        /* Mega-Hack -- Handle lights */
        if (fuelable_light_p(o_ptr))
        {
            if (o_ptr->timeout != j_ptr->timeout)
            {
                log_debug("object_similar: DIFFERENT timeout, returning false");
                return (false);
            }
        }

        /* Hack -- Never stack recharging items */
        else if (o_ptr->timeout || j_ptr->timeout)
            return (false);

        /* Probably okay */
        break;
    }

    /* Various */
    default:
    {
        /* Require knowledge */
        if (!object_known_p(o_ptr) || !object_known_p(j_ptr))
            return (false);

        /* Probably okay */
        break;
    }
    }

    /* Runtime-state items carry per-item repair data and must never stack. */
    if (object_runtime_state(o_ptr) || object_runtime_state(j_ptr))
    {
        return (false);
    }

    /* Hack -- Require identical "cursed" and "broken" status */
    if (((o_ptr->ident & (IDENT_CURSED)) != (j_ptr->ident & (IDENT_CURSED)))
        || ((o_ptr->ident & (IDENT_BROKEN)) != (j_ptr->ident & (IDENT_BROKEN))))
    {
        return (false);
    }

    /* Hack -- Require compatible inscriptions */
    if (o_ptr->obj_note != j_ptr->obj_note)
    {
        /* Normally require matching inscriptions */
        return (false);
    }

    /* Hack -- Require compatible "discount" fields */
    if (o_ptr->discount != j_ptr->discount)
    {
        bool o_uncursed_only = (o_ptr->discount == INSCRIP_UNCURSED)
            && !cursed_p(o_ptr);
        bool j_uncursed_only = (j_ptr->discount == INSCRIP_UNCURSED)
            && !cursed_p(j_ptr);

        /* Allow {uncursed} to stack with an otherwise identical clean item. */
        if ((o_uncursed_only && (j_ptr->discount == 0))
            || (j_uncursed_only && (o_ptr->discount == 0)))
        {
        }
        /* Both are (different) special inscriptions */
        else if ((o_ptr->discount >= INSCRIP_NULL)
            && (j_ptr->discount >= INSCRIP_NULL))
        {
            /* Normally require matching inscriptions */
            return (false);
        }

        /* One is a special inscription, one is a discount or nothing */
        else if ((o_ptr->discount >= INSCRIP_NULL)
            || (j_ptr->discount >= INSCRIP_NULL))
        {
            /* Normally require matching inscriptions */
            return (false);
        }

        /* One is a discount, one is a (different) discount or nothing */
        else
        {
            /* require matching discounts */
            return (false);
        }
    }

    /* Maximal "stacking" limit */
    if (o_ptr->number >= object_stack_limit(o_ptr))
        return (false);
    if (j_ptr->number >= object_stack_limit(j_ptr))
        return (false);

    /* They match, so they must be similar */
    return (true);
}

/*
 * Allow one item to "absorb" another, assuming they are similar.
 *
 * The blending of the "note" field assumes that either (1) one has an
 * inscription and the other does not, or (2) neither has an inscription.
 * In both these cases, we can simply use the existing note, unless the
 * blending object has a note, in which case we use that note.
 *
 * The blending of the "discount" field assumes that either (1) one is a
 * special inscription and one is nothing, or (2) one is a discount and
 * one is a smaller discount, or (3) one is a discount and one is nothing,
 * or (4) both are nothing.  In all of these cases, we can simply use the
 * "maximum" of the two "discount" fields.
 *
 * These assumptions are enforced by the "object_similar()" code.
 */
void object_absorb(object_type* o_ptr, object_type* j_ptr)
{
    /* Log staff absorption attempts - this should never happen! */
    if (o_ptr->tval == TV_STAFF || j_ptr->tval == TV_STAFF)
    {
        log_error("BUG: object_absorb called on staff! o_ptr: tval=%d k_idx=%d number=%d, j_ptr: tval=%d k_idx=%d number=%d",
                  o_ptr->tval, o_ptr->k_idx, o_ptr->number, 
                  j_ptr->tval, j_ptr->k_idx, j_ptr->number);
    }

    int total = o_ptr->number + j_ptr->number;
    int limit = object_stack_limit(o_ptr);

    if (limit > object_stack_limit(j_ptr))
        limit = object_stack_limit(j_ptr);

    if (total > limit)
    {
        o_ptr->number = limit;
        j_ptr->number = total - limit;
    }
    else
    {
        o_ptr->number = total;
        j_ptr->number = 0;
    }

    /* Preserve auto-recovery intent across stack merges and partial absorbs. */
    {
        bool o_pickup = o_ptr->pickup ? true : false;
        bool j_pickup = j_ptr->pickup ? true : false;
        bool pickup = o_pickup || j_pickup;
        bool o_slot_valid = o_pickup
            && ((o_ptr->pickup_slot == INVEN_QUIVER1)
                || (o_ptr->pickup_slot == INVEN_QUIVER2));
        bool j_slot_valid = j_pickup
            && ((j_ptr->pickup_slot == INVEN_QUIVER1)
                || (j_ptr->pickup_slot == INVEN_QUIVER2));
        s16b pickup_slot = -1;

        if (o_slot_valid && j_slot_valid)
        {
            if (o_ptr->pickup_slot == j_ptr->pickup_slot)
                pickup_slot = o_ptr->pickup_slot;
        }
        else if (o_slot_valid)
        {
            pickup_slot = o_ptr->pickup_slot;
        }
        else if (j_slot_valid)
        {
            pickup_slot = j_ptr->pickup_slot;
        }

        o_ptr->pickup = pickup;
        j_ptr->pickup = pickup;
        o_ptr->pickup_slot = pickup ? pickup_slot : -1;
        j_ptr->pickup_slot = pickup ? pickup_slot : -1;
    }

    /* Hack -- Blend "known" status */
    if (object_known_p(j_ptr))
        object_known(o_ptr);
    if (object_known_p(o_ptr))
        object_known(j_ptr);

    /* Blend "handled" status (combat stats stay visible after dropping). */
    if (j_ptr->ident & IDENT_HANDLED)
        o_ptr->ident |= IDENT_HANDLED;
    if (o_ptr->ident & IDENT_HANDLED)
        j_ptr->ident |= IDENT_HANDLED;

    /* Hack -- Blend "notes" */
    if (j_ptr->obj_note != 0)
        o_ptr->obj_note = j_ptr->obj_note;
    if (o_ptr->obj_note != 0)
        j_ptr->obj_note = o_ptr->obj_note;

    /* Mega-Hack -- Blend "discounts" */
    if (o_ptr->discount < j_ptr->discount)
        o_ptr->discount = j_ptr->discount;
    if (j_ptr->discount < o_ptr->discount)
        j_ptr->discount = o_ptr->discount;
}

static int object_weight_flag_adjustment(int base_weight, u32b flags4)
{
    int quarter_basis;

    if (base_weight <= 0)
        return 0;

    quarter_basis = div_round(base_weight, 4);

    /* Weight affixes should move the item by at least 0.5 lb. */
    if (quarter_basis < 5)
        quarter_basis = 5;

    if ((flags4 & TR4_WEIGHT) && !(flags4 & TR4_NEG_WEIGHT))
        return quarter_basis;

    if ((flags4 & TR4_NEG_WEIGHT) && !(flags4 & TR4_WEIGHT))
        return -quarter_basis;

    return 0;
}

static s16b object_roll_base_weight(const object_kind* k_ptr)
{
    int weight;

    if (!k_ptr)
        return 0;

    /* Exact weight for most items, approximate weight for weapons and armour. */
    switch (k_ptr->tval)
    {
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        weight = Rand_normal(k_ptr->weight, k_ptr->weight / 6 + 1);

        /* Round to the nearest multiple of 0.5 lb. */
        weight = div_round(weight * 2, 10);
        weight *= 5;

        /* Restrict weight to within [2/3, 3/2] of the standard. */
        while (weight * 3 < k_ptr->weight * 2)
            weight += 5;
        while (weight * 2 > k_ptr->weight * 3)
            weight -= 5;

        break;
    }
    default:
        weight = k_ptr->weight;
        break;
    }

    return (s16b)weight;
}

static void apply_object_weight_flags(object_type* o_ptr, int base_weight,
    u32b flags4)
{
    int adjusted_weight;
    int weight_adjust = object_weight_flag_adjustment(base_weight, flags4);

    if (weight_adjust == 0)
        return;

    adjusted_weight = o_ptr->weight + weight_adjust;
    /* Weight is stored in tenths of a pound, so the minimum is 0.5 lb. */
    if (adjusted_weight < 5)
        adjusted_weight = 5;

    o_ptr->weight = (s16b)adjusted_weight;
}

void object_refresh_weight(object_type* o_ptr)
{
    object_kind* k_ptr;
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx || artefact_p(o_ptr))
        return;

    k_ptr = &k_info[o_ptr->k_idx];
    o_ptr->weight = object_roll_base_weight(k_ptr);

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    apply_object_weight_flags(o_ptr, k_ptr->weight, f4);
}

/*
 * Find the index of the object_kind with the given tval and sval
 */
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
static void object_mention(const object_type* o_ptr)
{
    char o_name[80];

    /* Describe */
    object_desc_spoil(o_name, sizeof(o_name), o_ptr, false, 0);

    /* Artefact */
    if (artefact_p(o_ptr))
    {
        /* Silly message */
        msg_format("Artefact (%s)", o_name);
    }

    /* Ego-item */
    else if (ego_item_p(o_ptr))
    {
        /* Silly message */
        msg_format("Ego-item (%s)", o_name);
    }

    /* Normal item */
    else
    {
        /* Silly message */
        msg_format("Object (%s)", o_name);
    }
}

/*
 * Attempt to change an object into an special item -MWK-
 * Better only called by apply_magic().
 * The return value says if we picked a cursed item (if allowed) and is
 * passed on to a_m_aux1/2().
 * If no legal ego item is found, this routine returns 0, resulting in
 * an unenchanted item.
 */
static int make_special_item(object_type* o_ptr, bool only_good)
{
    int i, j, level;

    int e_idx;

    long value, total;

    ego_item_type* e_ptr;
    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    alloc_entry* table = alloc_ego_table;

    /* Fail if object already is ego or artefact */
    if (o_ptr->name1)
        return (false);
    if (object_has_ego(o_ptr))
        return (false);

    level = object_level;

    /* Boost level (like with object base types) */
    if (level > 0)
    {
        /* Occasional "boost" */
        if (one_in_(GREAT_SPECIAL))
        {
            // most of the time, choose a new deeper depth, weighted towards the
            // current depth
            if (level < MORGOTH_DEPTH)
            {
                int x = rand_range(level + 1, MORGOTH_DEPTH);
                int y = rand_range(level + 1, MORGOTH_DEPTH);

                level = MIN(x, y);
            }

            // but if it was already very deep, just increment it
            else
            {
                level++;
            }
        }
    }

    /* Reset total */
    total = 0L;

    /* Process probabilities */
    for (i = 0; i < alloc_ego_size; i++)
    {
        /* Default */
        table[i].prob3 = 0;

        /* Objects are sorted by depth */
        if (table[i].level > level)
            continue;

        /* Get the index */
        e_idx = table[i].index;

        /* Get the actual kind */
        e_ptr = &e_info[e_idx];

        /* Some special items can't be generated too deep */
        if ((e_ptr->max_level > 0) && (p_ptr->depth > e_ptr->max_level))
            continue;
        if (e_ptr->flags3 & TR3_DAMAGED)
            continue; /* Damaged drops are reserved for explicit damaged-drop paths. */

        /* If we force fine/special, don't create cursed */
        if (only_good && (e_ptr->flags3 & TR3_LIGHT_CURSE))
            continue;

        /* If we force fine/special, don't useless */
        if (only_good && (e_ptr->cost == 0))
            continue;

        /* Don't mix opposing alignment flags on ego creations. */
        if ((k_ptr->flags4 & TR4_NOBLE_ITEM) && (e_ptr->flags4 & TR4_EVIL_ITEM))
            continue;
        if ((k_ptr->flags4 & TR4_EVIL_ITEM) && (e_ptr->flags4 & TR4_NOBLE_ITEM))
            continue;
        if ((k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && (e_ptr->flags4 & TR4_EVIL_ITEM))
            continue;

        /* Test if this is a legal special item type for this object */
        for (j = 0; j < EGO_TVALS_MAX; j++)
        {
            /* Require identical base type */
            if (o_ptr->tval == e_ptr->tval[j])
            {
                /* Require sval in bounds, lower */
                if (o_ptr->sval >= e_ptr->min_sval[j])
                {
                    /* Require sval in bounds, upper */
                    if (o_ptr->sval <= e_ptr->max_sval[j])
                    {
                        /* Accept */
                        table[i].prob3 = table[i].prob2;
                    }
                }
            }
        }

        /* Total */
        total += table[i].prob3;
    }

    // If there aren't *any* valid items to choose from give up
    if (total == 0)
    {
        return (0);
    }

    /* Pick an special item */
    value = rand_int(total);

    /* Find the object */
    for (i = 0; i < alloc_ego_size; i++)
    {
        /* Found the entry */
        if (value < table[i].prob3)
            break;

        /* Decrement */
        value = value - table[i].prob3;
    }

    /* We have one */
    e_idx = (byte)table[i].index;
    {
        ego_item_type* chosen = &e_info[e_idx];
        const char* raw = e_name + chosen->name;
        if (ego_name_is_prefix(raw))
            object_set_ego_prefix(o_ptr, e_idx);
        else
            object_set_ego_suffix(o_ptr, e_idx);
    }

    return ((e_info[e_idx].flags3 & TR3_LIGHT_CURSE) ? -2 : 2);
}

/*
 * As artefacts are generated, there is an increasing chance to fail to make the
 * next one
 */
static bool too_many_artefacts(void)
{
    int i;

    for (i = 0; i < p_ptr->artefacts; i++)
    {
        if (percent_chance(10))
            return (true);
    }

    return (false);
}

#if 0
/*
 * Mega-Hack -- Attempt to create one of the "Special Objects".
 *
 * We are only called from "make_object()", and we assume that
 * "apply_magic()" is called immediately after we return.
 *
 * Note -- see "make_artefact()" and "apply_magic()".
 *
 * We *prefer* to create the special artefacts in order, but this is
 * normally outweighed by the "rarity" rolls for those artefacts.
 */
static bool make_artefact_special(object_type* o_ptr)
{
    int i;

    int k_idx;

    int depth_check = ((object_generation_mode) ? object_level : p_ptr->depth);

    /* No artefacts, do nothing */
    if (adult_no_artefacts)
        return (false);

    // as more artefacts are generated, the chance for another decreases
    if (too_many_artefacts())
        return (false);

    /* Check the special artefacts */
    for (i = 0; i < z_info->art_spec_max; ++i)
    {
        artefact_type* a_ptr = &a_info[i];

        /* Skip "empty" artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Cannot make an artefact twice */
        if (a_ptr->cur_num)
            continue;

        /* Cannot make an artefact reserved for Valar quest */
        if (valar_reserved_artifacts && valar_reserved_artifacts[i])
            continue;

        /* Enforce minimum "depth" (loosely) */
        if (a_ptr->level > depth_check)
        {
            /* Get the "out-of-depth factor" */
            int d = (a_ptr->level - depth_check) * 2;

            /* Roll for out-of-depth creation */
            if (rand_int(d) != 0)
                continue;
        }

        /* Artefact "rarity roll" */
        if (rand_int(a_ptr->rarity) != 0)
            continue;

        /* Find the base object */
        k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);

        /* Enforce minimum "object" level (loosely) */
        if (k_info[k_idx].level > depth_check)
        {
            /* Get the "out-of-depth factor" */
            int d = (k_info[k_idx].level - depth_check) * 5;

            /* Roll for out-of-depth creation */
            if (rand_int(d) != 0)
                continue;
        }

        /* Assign the template */
        object_prep(o_ptr, k_idx);

        /* Mark the item as an artefact */
        o_ptr->name1 = i;

        /* Success */
        return (true);
    }

    /* Failure */
    return (false);
}
#endif

/*
 * Attempt to change an object into an artefact
 *
 * This routine should only be called by "apply_magic()"
 *
 * Note -- see "make_artefact_special()" and "apply_magic()"
 */
static bool make_artefact(object_type* o_ptr, bool allow_insta)
{
    int i;

    int depth_check = ((object_generation_mode) ? object_level : p_ptr->depth);

    /* No artefacts, do nothing */
    if (adult_no_artefacts)
        return (false);

    // as more artefacts are generated, the chance for another decreases
    if (too_many_artefacts())
        return (false);

    /* Check the artefact list (skip the "specials" and randoms) */
    for (i = z_info->art_spec_max; i < z_info->art_norm_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        /* Skip "empty" items */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Cannot make an artefact twice */
        if (a_ptr->cur_num)
            continue;

        /* Cannot make an artefact reserved for Valar quest */
        if (valar_reserved_artifacts && valar_reserved_artifacts[i])
            continue;

        /* Must have the correct fields */
        if (a_ptr->tval != o_ptr->tval)
            continue;
        if (a_ptr->sval != o_ptr->sval)
            continue;

        /* Can only generate 'insta-arts' in certain situations */
        if ((a_ptr->flags3 & (TR3_INSTA_ART)) && !allow_insta)
        {
            continue;
        }

        /* XXX XXX Enforce minimum "depth" (loosely) */
        if (a_ptr->level > depth_check)
        {
            /* Get the "out-of-depth factor" */
            int d = (a_ptr->level - depth_check) * 2;

            /* Roll for out-of-depth creation */
            if (rand_int(d) != 0)
                continue;
        }

        /* We must make the "rarity roll" */
        if (!one_in_(a_ptr->rarity))
            continue;

        /* Mark the item as an artefact */
        o_ptr->name1 = i;

        /* Set stack size for stackable artefacts (arrows, throwing weapons) */
        {
            const object_kind* k_ptr = (o_ptr->k_idx ? &k_info[o_ptr->k_idx] : NULL);
            bool allow_stack = (o_ptr->tval == TV_ARROW)
                || (k_ptr && (k_ptr->flags3 & TR3_THROWING));
            if (allow_stack)
            {
                artefact_type* art_ptr = &a_info[i];
                int desired = art_ptr->spawn_num ? (int)art_ptr->spawn_num : 1;
                int limit = object_stack_limit(o_ptr);
                if (limit > 0 && desired > limit)
                    desired = limit;
                if (desired < 1)
                    desired = 1;
                o_ptr->number = (byte)desired;
            }
            else if (o_ptr->number < 1)
            {
                o_ptr->number = 1;
            }
        }

        /* Success */
        return (true);
    }

    /* Failure */
    return (false);
}

/*
 * Charge a new staff.
 */
static void charge_staff(object_type* o_ptr)
{
    int mult = CHANNELING_CHARGE_MULTIPLIER;

    switch (o_ptr->sval)
    {
    case SV_STAFF_SECRETS:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_IMPRISONMENT:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_FREEDOM:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_LIGHT:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_SANCTITY:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_UNDERSTANDING:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_REVELATIONS:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_TREASURES:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_FOES:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_SLUMBER:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_MAJESTY:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_SELF_KNOWLEDGE:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_DISMAY:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_RECHARGING:
        o_ptr->pval = mult * damroll(2, 2);
        break;

    case SV_STAFF_SUMMONING:
        o_ptr->pval = mult * damroll(6, 2);
        break;
    case SV_STAFF_SHADOWS:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    }
}

/*
 *
 * Determines the theme of a chest.  This function is called
 * from chest_death when the chest is being opened. JG
 *
 */
static int choose_chest_contents(void)
{
    /*
     * chest theme # 2 is potions  (+ herbs of restoring)
     * chest theme # 3 is staffs
     * chest theme # 4 is shields
     * chest theme # 5 is weapons
     * chest theme # 6 is armor
     * chest theme # 7 is boots
     * chest theme # 8 is bow
     * chest theme # 9 is cloak
     * chest theme #10 is gloves
     * chest theme #11 is edged weapons
     * chest theme #12 is polearms
     * chest theme #13 is helms and crowns
     * chest theme #14 is jewellery
     */

    return (dieroll(13) + 1);
}

/*
 * Apply magic to an item known to be a "weapon"
 *
 */
static void a_m_aux_1(object_type* o_ptr, int level)
{
    bool boost_dam = false;
    bool boost_att = false;

    // arrows can only have increased attack value
    if (o_ptr->tval == TV_ARROW)
    {
        o_ptr->att += 3;
        return;
    }

    else
    {
        // small chance of boosting both
        if (percent_chance(level))
        {
            boost_dam = true;
            boost_att = true;
        }
        // otherwise 50/50 chance of dam or att
        else if (one_in_(2))
        {
            boost_dam = true;
        }
        else
        {
            boost_att = true;
        }
    }

    if (boost_dam)
    {
        o_ptr->ds++;
    }
    if (boost_att)
    {
        o_ptr->att++;
    }
}

/*
 * Apply magic to an item known to be "armor"
 *
 */
static void a_m_aux_2(object_type* o_ptr, int level)
{
    bool boost_prot = false;
    bool boost_other = false;

    // for cloaks and robes and filthy rags go for evasion only
    if ((o_ptr->tval == TV_CLOAK)
        || ((o_ptr->tval == TV_SOFT_ARMOR) && (o_ptr->sval == SV_ROBE)))
    {
        boost_other = true;
    }
    // otherwise if there are no penalties to fix, then go for protection only
    else if ((o_ptr->att >= 0) && (o_ptr->evn >= 0))
    {
        boost_prot = true;
    }
    // otherwise choose randomly (protection, other, or both)
    else
    {
        // small chance of boosting both
        if (percent_chance(level))
        {
            boost_prot = true;
            boost_other = true;
        }
        // otherwise 50/50 chance of dam or att
        else if (one_in_(2))
        {
            boost_prot = true;
        }
        else
        {
            boost_other = true;
        }
    }

    if (boost_other)
    {
        if ((o_ptr->att < 0) && (o_ptr->evn < 0))
        {
            if (one_in_(2))
                o_ptr->evn++;
            else
                o_ptr->att++;
        }
        else if (o_ptr->att < 0)
        {
            o_ptr->att++;
        }
        else
        {
            o_ptr->evn++;
        }
    }

    if (boost_prot)
    {
        o_ptr->ps++;
    }
}

/*
 * Apply magic to an item known to be "boring"
 *
 * Hack -- note the special code for various items
 */
static void a_m_aux_4(object_type* o_ptr, int level, bool fine, bool special)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Unused parameters */
    (void)level;

    /* Apply magic (good or bad) according to type */
    switch (o_ptr->tval)
    {
    case TV_LIGHT:
    {
        /* Hack -- Torches -- random fuel */
        if (o_ptr->sval == SV_LIGHT_TORCH)
        {
            int spawn_fuel = 1000;
            int min_fuel = 250;

            if (one_in_(3))
            {
                o_ptr->timeout = rand_range(min_fuel, spawn_fuel);
            }
            else
            {
                o_ptr->timeout = spawn_fuel;
            }
        }

        /* Hack -- Lanterns -- random fuel */
        else if (o_ptr->sval == SV_LIGHT_LANTERN)
        {
            int spawn_fuel = (FUEL_LAMP * 2) / 5;
            int min_fuel = FUEL_LAMP / 15;

            if (one_in_(3))
            {
                o_ptr->timeout = rand_range(min_fuel, spawn_fuel);
            }
            else
            {
                o_ptr->timeout = spawn_fuel;
            }
        }

        /* Mallorn torches -- random fuel */
        if (o_ptr->sval == SV_LIGHT_MALLORN)
        {
            if (one_in_(3))
            {
                o_ptr->timeout = rand_range(40, 100);
            }
            else
            {
                o_ptr->timeout = 100;
            }
        }
        break;
    }

    case TV_STAFF:
    {
        /* Hack -- charge staffs */
        charge_staff(o_ptr);

        break;
    }

    case TV_GEM:
    {
        /* Gems use number instead of charges - spawn same quantity as charge_staff would have given */
        int charges = 0;
        
        switch (o_ptr->sval)
        {
        case SV_GEM_FREEDOM:
        case SV_GEM_LIGHT:
        case SV_GEM_REVELATIONS:
        case SV_GEM_FOES:
            charges = damroll(4, 2);
            break;
        case SV_GEM_SANCTITY:
        case SV_GEM_UNDERSTANDING:
        case SV_GEM_TREASURES:
        case SV_GEM_SELF_KNOWLEDGE:
        case SV_GEM_RECHARGING:
        case SV_GEM_SHADOWS:
            charges = damroll(2, 2);
            break;
        default:
            charges = damroll(2, 2);
            break;
        }
        
        o_ptr->number = charges;
        o_ptr->pval = 0;  /* Gems don't use pval */
        
        break;
    }

    case TV_HORN:
    {
        /* Transfer the pval. */
        o_ptr->pval = k_ptr->pval;
        break;
    }

    case TV_SKELETON:
    {
        /* Not searched. */
        o_ptr->pval = 1;
        break;
    }

    case TV_CHEST:
    {
        /* Hack -- chest level is fixed at player level at time of generation */
        o_ptr->pval = object_level;

        /*chest created with fine flag get a level boost*/
        if (fine)
            o_ptr->pval += 2;

        /*chest created with special flag also gets a level boost*/
        if (special)
            o_ptr->pval += 2;

        /*chests now increase level rating*/
        rating += 5;

        /* Don't exceed "chest level" of 25 */
        if (o_ptr->pval > 25)
            o_ptr->pval = 25;

        /*a minimum pval of 1, or else it will be empty on the surface*/
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;

        /*save the chest theme in xtra1, used in chest death*/
        o_ptr->xtra1 = choose_chest_contents();

        break;
    }
    }
}

void object_into_artefact(object_type* o_ptr, artefact_type* a_ptr)
{
    int i;

    /* Extract the other fields */
    o_ptr->pval = a_ptr->pval;
    for (i = 0; i < A_MAX; i++)
        o_ptr->stat_bonus[i] = a_ptr->stat_bonus[i];
    for (i = 0; i < S_MAX; i++)
        o_ptr->skill_bonus[i] = a_ptr->skill_bonus[i];
    o_ptr->att = a_ptr->att;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->evn = a_ptr->evn;
    o_ptr->pd = a_ptr->pd;
    o_ptr->ps = a_ptr->ps;
    o_ptr->weight = a_ptr->weight;

    // add the abilities
    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
        o_ptr->bane_type[i + o_ptr->abilities] = a_ptr->bane_type[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    /* Hack - mark the depth of artefact creation for the notes function
     * probably a bad idea to use this flag.  It is used when making special
     * items, which currently fails when an item is an artefact.  If this was
     * changed this would be the cause of some major bugs.
     */
    if (p_ptr->depth)
    {
        o_ptr->xtra1 = p_ptr->depth;
    }

    /*hack - mark chest items with a special level so the notes patch
     * knows where it is coming from.
     */
    else if (object_generation_mode == OB_GEN_MODE_CHEST)
        o_ptr->xtra1 = CHEST_LEVEL;
    else if (object_generation_mode == OB_GEN_MODE_SKELETON)
        o_ptr->xtra1 = SKELETON_LEVEL;

    /* Hack -- extract the "broken" flag */
    if (!a_ptr->cost)
        o_ptr->ident |= (IDENT_BROKEN);

    /* Hack -- extract the "cursed" flag */
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);
}

static void apply_delta_byte_clamped(byte* v, int delta)
{
    if (!v)
        return;

    int next = (int)(*v) + delta;
    if (next < 0)
        next = 0;
    if (next > 255)
        next = 255;
    *v = (byte)next;
}

static s16b roll_ego_bonus_range(s16b min_bonus, s16b max_bonus, bool smithing)
{
    if (smithing || min_bonus == max_bonus)
        return min_bonus;

    return (s16b)rand_range(min_bonus, max_bonus);
}

static void apply_ego_explicit_bonus_ranges(object_type* o_ptr,
    const ego_item_type* e_ptr, bool smithing)
{
    if (!o_ptr || !e_ptr)
        return;

    for (int i = 0; i < A_MAX; i++)
    {
        if (!e_ptr->stat_bonus_set[i])
            continue;

        o_ptr->stat_bonus[i] += roll_ego_bonus_range(
            e_ptr->stat_bonus_min[i], e_ptr->stat_bonus[i], smithing);
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (!e_ptr->skill_bonus_set[i])
            continue;

        o_ptr->skill_bonus[i] += roll_ego_bonus_range(
            e_ptr->skill_bonus_min[i], e_ptr->skill_bonus[i], smithing);
    }
}

static bool ego_affix_has_only_flag_effects(const ego_item_type* e_ptr)
{
    if (!e_ptr)
        return false;

    if (e_ptr->abilities != 0 || e_ptr->max_pval != 0 || e_ptr->min_pval != 0
        || e_ptr->max_att != 0 || e_ptr->to_dd != 0 || e_ptr->to_ds != 0
        || e_ptr->max_evn != 0 || e_ptr->to_pd != 0 || e_ptr->to_ps != 0)
    {
        return false;
    }

    for (int i = 0; i < A_MAX; i++)
    {
        if (e_ptr->stat_bonus_set[i] || e_ptr->stat_bonus_min[i] != 0
            || e_ptr->stat_bonus[i] != 0)
        {
            return false;
        }
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (e_ptr->skill_bonus_set[i] || e_ptr->skill_bonus_min[i] != 0
            || e_ptr->skill_bonus[i] != 0)
        {
            return false;
        }
    }

    return true;
}

static bool object_is_fire_breakable_weapon(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->tval == TV_HAFTED)
        return true;

    if (o_ptr->tval == TV_POLEARM)
        return true;

    return false;
}

static s32b pack_fire_broken_weapon_payload(s16b att, byte dd, byte ds)
{
    u32b payload = (u32b)(u16b)att;
    payload |= ((u32b)dd << 16);
    payload |= ((u32b)ds << 24);
    return (s32b)payload;
}

static void unpack_fire_broken_weapon_payload(s32b payload, s16b* att, byte* dd,
    byte* ds)
{
    u32b bits = (u32b)payload;

    if (att)
        *att = (s16b)(bits & 0xFFFFU);
    if (dd)
        *dd = (byte)((bits >> 16) & 0xFFU);
    if (ds)
        *ds = (byte)((bits >> 24) & 0xFFU);
}

bool object_is_fire_broken(const object_type* o_ptr)
{
    return object_runtime_state(o_ptr) == OBJECT_RUNTIME_STATE_FIRE_BROKEN;
}

bool object_break_shafted_weapon_by_fire(object_type* o_ptr)
{
    if (!object_is_fire_breakable_weapon(o_ptr))
        return false;

    if (object_is_fire_broken(o_ptr))
        return true;

    object_set_runtime_payload(
        o_ptr, pack_fire_broken_weapon_payload(o_ptr->att, o_ptr->dd, o_ptr->ds));
    object_set_runtime_state(o_ptr, OBJECT_RUNTIME_STATE_FIRE_BROKEN);

    if (o_ptr->att > SHRT_MIN)
        o_ptr->att--;

    if (o_ptr->ds > 1)
        o_ptr->ds--;
    else if (o_ptr->dd > 1)
        o_ptr->dd--;

    pseudo_id(o_ptr);
    return true;
}

bool object_repair_fire_broken_weapon(object_type* o_ptr)
{
    s16b att = 0;
    byte dd = 0;
    byte ds = 0;

    if (!object_is_fire_broken(o_ptr))
        return false;

    unpack_fire_broken_weapon_payload(
        object_runtime_payload(o_ptr), &att, &dd, &ds);

    o_ptr->att = att;
    o_ptr->dd = dd;
    o_ptr->ds = ds;
    object_set_runtime_state(o_ptr, OBJECT_RUNTIME_STATE_NONE);
    object_set_runtime_payload(o_ptr, 0);

    pseudo_id(o_ptr);
    return true;
}

bool object_break_brass_lantern(object_type* o_ptr)
{
    byte old_prefix;
    bool old_prefix_carried_intrinsic_curse = false;
    bool new_state_is_intrinsically_cursed = false;

    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_LIGHT
        || o_ptr->sval != SV_LIGHT_LANTERN)
    {
        return false;
    }

    old_prefix = object_ego_prefix(o_ptr);
    if (old_prefix == EGO_BROKEN_BRASS_LANTERN)
    {
        o_ptr->ident |= IDENT_BROKEN;
        return true;
    }

    if (old_prefix)
    {
        if (old_prefix >= z_info->e_max)
            return false;

        if (!ego_affix_has_only_flag_effects(&e_info[old_prefix]))
        {
            log_warn(
                "object_break_brass_lantern: unsupported lantern prefix %d",
                old_prefix);
            return false;
        }

        old_prefix_carried_intrinsic_curse
            = (e_info[old_prefix].flags3
                & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
            != 0;
    }

    object_set_ego_prefix(o_ptr, EGO_BROKEN_BRASS_LANTERN);
    o_ptr->ident |= IDENT_BROKEN;

    if (o_ptr->name1
        && (a_info[o_ptr->name1].flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE)))
    {
        new_state_is_intrinsically_cursed = true;
    }

    if (k_info[o_ptr->k_idx].flags3
        & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
    {
        new_state_is_intrinsically_cursed = true;
    }

    if (object_ego_suffix(o_ptr)
        && (e_info[object_ego_suffix(o_ptr)].flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE)))
    {
        new_state_is_intrinsically_cursed = true;
    }

    if (new_state_is_intrinsically_cursed)
        o_ptr->ident |= IDENT_CURSED;
    else if (old_prefix_carried_intrinsic_curse)
        o_ptr->ident &= ~IDENT_CURSED;

    pseudo_id(o_ptr);
    return true;
}

bool object_apply_ego_affix(object_type* o_ptr, int e_idx, bool smithing)
{
    const object_kind* k_ptr = NULL;
    ego_item_type* e_ptr;
    u32b ef3, ef4;
    int i;
    int max_att;
    int to_dd;
    int to_ds;
    int max_evn;
    int to_pd;
    int to_ps;
    bool enforce_positive_protection;

    if (!o_ptr || !o_ptr->k_idx || e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    k_ptr = &k_info[o_ptr->k_idx];
    e_ptr = &e_info[e_idx];
    ef3 = e_ptr->flags3;
    ef4 = e_ptr->flags4;
    u32b pval_mask = object_kind_pval_flags1(k_ptr) | ego_item_pval_flags1(e_ptr);
    max_att = (int)(int8_t)e_ptr->max_att;
    to_dd = (int)(int8_t)e_ptr->to_dd;
    to_ds = (int)(int8_t)e_ptr->to_ds;
    max_evn = (int)(int8_t)e_ptr->max_evn;
    to_pd = (int)(int8_t)e_ptr->to_pd;
    to_ps = (int)(int8_t)e_ptr->to_ps;
    enforce_positive_protection = ((k_ptr->pd > 0) && (k_ptr->ps > 0))
        || (to_pd > 0) || (to_ps > 0);

    for (i = 0; i < e_ptr->abilities && o_ptr->abilities < (int)N_ELEMENTS(o_ptr->skilltype); i++)
    {
        int idx = o_ptr->abilities;
        o_ptr->skilltype[idx] = e_ptr->skilltype[i];
        o_ptr->abilitynum[idx] = e_ptr->abilitynum[i];
        o_ptr->abilities++;
    }

    if (!e_ptr->cost)
        o_ptr->ident |= (IDENT_BROKEN);
    if (ef3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    if (smithing)
    {
        if (max_att)
            o_ptr->att += (max_att > 0) ? 1 : -1;
        if (max_evn)
            o_ptr->evn += (max_evn > 0) ? 1 : -1;
        if (to_dd)
            apply_delta_byte_clamped(&o_ptr->dd, (to_dd > 0) ? 1 : -1);
        if (to_ds)
            apply_delta_byte_clamped(&o_ptr->ds, (to_ds > 0) ? 1 : -1);
        if (to_pd)
            apply_delta_byte_clamped(&o_ptr->pd, (to_pd > 0) ? 1 : -1);
        if (to_ps)
            apply_delta_byte_clamped(&o_ptr->ps, (to_ps > 0) ? 1 : -1);

        if (e_ptr->max_pval > 0)
        {
            int delta = 1;
            o_ptr->pval += (s16b)delta;
            object_apply_pval_delta_with_mask(o_ptr, pval_mask, delta);
        }
    }
    else
    {
        if (max_att)
            o_ptr->att += (max_att > 0) ? dieroll(max_att) : -dieroll(-max_att);
        if (max_evn)
            o_ptr->evn += (max_evn > 0) ? dieroll(max_evn) : -dieroll(-max_evn);
        if (to_dd)
            apply_delta_byte_clamped(&o_ptr->dd, (to_dd > 0) ? dieroll(to_dd) : -dieroll(-to_dd));
        if (to_ds)
            apply_delta_byte_clamped(&o_ptr->ds, (to_ds > 0) ? dieroll(to_ds) : -dieroll(-to_ds));
        if (to_pd)
            apply_delta_byte_clamped(&o_ptr->pd, (to_pd > 0) ? dieroll(to_pd) : -dieroll(-to_pd));
        if (to_ps)
            apply_delta_byte_clamped(&o_ptr->ps, (to_ps > 0) ? dieroll(to_ps) : -dieroll(-to_ps));

        if (e_ptr->max_pval > 0)
        {
            int delta = dieroll(e_ptr->max_pval);
            o_ptr->pval += (s16b)delta;
            object_apply_pval_delta_with_mask(o_ptr, pval_mask, delta);
        }
    }

    apply_ego_explicit_bonus_ranges(o_ptr, e_ptr, smithing);

    if (k_ptr->dd > 0)
    {
        if (o_ptr->dd < 1)
            o_ptr->dd = 1;

        if (o_ptr->ds < 1)
        {
            int deficit = 1 - (int)o_ptr->ds;
            o_ptr->ds = 1;
            if ((int)o_ptr->dd > deficit)
                o_ptr->dd = (byte)((int)o_ptr->dd - deficit);
            else
                o_ptr->dd = 1;
        }
    }

    if (enforce_positive_protection && ((k_ptr->pd > 0) || (o_ptr->pd > 0)))
    {
        if (o_ptr->pd < 1)
            o_ptr->pd = 1;

        if (o_ptr->ps < 1)
        {
            int deficit = 1 - (int)o_ptr->ps;
            o_ptr->ps = 1;
            if ((int)o_ptr->pd > deficit)
                o_ptr->pd = (byte)((int)o_ptr->pd - deficit);
            else
                o_ptr->pd = 1;
        }
    }

    apply_object_weight_flags(o_ptr, k_ptr->weight, ef4);

    pseudo_id(o_ptr);
    return true;
}

void object_into_special(object_type* o_ptr, int lev, bool smithing)
{
    u32b f1, f2, f3, f4;
    int i;
    const object_kind* k_ptr = NULL;
    bool enforce_positive_protection = false;

    (void)
        lev; // Cast to soothe compilation warnings (currently unused variable)

    if (o_ptr && o_ptr->k_idx)
        k_ptr = &k_info[o_ptr->k_idx];
    if (k_ptr && (k_ptr->pd > 0) && (k_ptr->ps > 0))
        enforce_positive_protection = true;

    /* Examine the item */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* Ensure overall curse state is set before applying pval deltas. */
    if (f3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    /* Apply each ego present (prefix then suffix). */
    byte ego_ids[2] = { object_ego_prefix(o_ptr), object_ego_suffix(o_ptr) };
    for (int ego_slot = 0; ego_slot < 2; ego_slot++)
    {
        byte e_idx = ego_ids[ego_slot];
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        u32b pval_mask = object_kind_pval_flags1(k_ptr) | ego_item_pval_flags1(e_ptr);
        int max_att = (int)(int8_t)e_ptr->max_att;
        int to_dd = (int)(int8_t)e_ptr->to_dd;
        int to_ds = (int)(int8_t)e_ptr->to_ds;
        int max_evn = (int)(int8_t)e_ptr->max_evn;
        int to_pd = (int)(int8_t)e_ptr->to_pd;
        int to_ps = (int)(int8_t)e_ptr->to_ps;
        if ((to_pd > 0) || (to_ps > 0))
            enforce_positive_protection = true;

        /* Add the abilities (bounded by object ability storage). */
        for (i = 0; i < e_ptr->abilities && o_ptr->abilities < (int)N_ELEMENTS(o_ptr->skilltype); i++)
        {
            int idx = o_ptr->abilities;
            o_ptr->skilltype[idx] = e_ptr->skilltype[i];
            o_ptr->abilitynum[idx] = e_ptr->abilitynum[i];
            o_ptr->abilities++;
        }

        /* Acquire "broken"/"cursed" flags. */
        if (!e_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);
        if (e_ptr->flags3 & (TR3_LIGHT_CURSE))
            o_ptr->ident |= (IDENT_CURSED);

        /* Apply numeric bonuses. */
        if (smithing)
        {
            if (max_att)
                o_ptr->att += (max_att > 0) ? 1 : -1;
            if (max_evn)
                o_ptr->evn += (max_evn > 0) ? 1 : -1;
            if (to_dd)
                apply_delta_byte_clamped(&o_ptr->dd, (to_dd > 0) ? 1 : -1);
            if (to_ds)
                apply_delta_byte_clamped(&o_ptr->ds, (to_ds > 0) ? 1 : -1);
            if (to_pd)
                apply_delta_byte_clamped(&o_ptr->pd, (to_pd > 0) ? 1 : -1);
            if (to_ps)
                apply_delta_byte_clamped(&o_ptr->ps, (to_ps > 0) ? 1 : -1);

            if (e_ptr->max_pval > 0)
            {
                int delta = 1;
                o_ptr->pval += (s16b)delta;
                object_apply_pval_delta_with_mask(o_ptr, pval_mask, delta);
            }
        }
        else
        {
            if (max_att)
                o_ptr->att += (max_att > 0) ? dieroll(max_att) : -dieroll(-max_att);
            if (max_evn)
                o_ptr->evn += (max_evn > 0) ? dieroll(max_evn) : -dieroll(-max_evn);
            if (to_dd)
                apply_delta_byte_clamped(&o_ptr->dd, (to_dd > 0) ? dieroll(to_dd) : -dieroll(-to_dd));
            if (to_ds)
                apply_delta_byte_clamped(&o_ptr->ds, (to_ds > 0) ? dieroll(to_ds) : -dieroll(-to_ds));
            if (to_pd)
                apply_delta_byte_clamped(&o_ptr->pd, (to_pd > 0) ? dieroll(to_pd) : -dieroll(-to_pd));
            if (to_ps)
                apply_delta_byte_clamped(&o_ptr->ps, (to_ps > 0) ? dieroll(to_ps) : -dieroll(-to_ps));

            if (e_ptr->max_pval > 0)
            {
                int delta = dieroll(e_ptr->max_pval);
                o_ptr->pval += (s16b)delta;
                object_apply_pval_delta_with_mask(o_ptr, pval_mask, delta);
            }
        }

        /* Apply ego-specific M: rolls after pval-based bonuses. */
        apply_ego_explicit_bonus_ranges(o_ptr, e_ptr, smithing);

    }

    /* Never allow invalid dice/sides on items that normally have them. */
    if (k_ptr && k_ptr->dd > 0)
    {
        if (o_ptr->dd < 1)
            o_ptr->dd = 1;

        if (o_ptr->ds < 1)
        {
            int deficit = 1 - (int)o_ptr->ds;
            o_ptr->ds = 1;
            if ((int)o_ptr->dd > deficit)
                o_ptr->dd = (byte)((int)o_ptr->dd - deficit);
            else
                o_ptr->dd = 1;
        }
    }

    if (enforce_positive_protection && k_ptr
        && ((k_ptr->pd > 0) || (o_ptr->pd > 0)))
    {
        if (o_ptr->pd < 1)
            o_ptr->pd = 1;

        if (o_ptr->ps < 1)
        {
            int deficit = 1 - (int)o_ptr->ps;
            o_ptr->ps = 1;
            if ((int)o_ptr->pd > deficit)
                o_ptr->pd = (byte)((int)o_ptr->pd - deficit);
            else
                o_ptr->pd = 1;
        }
    }

    /* Apply explicit weight flags relative to base kind weight. */
    if (k_ptr)
        apply_object_weight_flags(o_ptr, k_ptr->weight, f4);

    /* Cheat -- describe the item */
    if (cheat_peek)
        object_mention(o_ptr);

    // pseudo-id the item
    pseudo_id(o_ptr);
}

/*
 * Complete the "creation" of an object by applying "magic" to the item
 *
 * This includes not only rolling for random bonuses, but also putting the
 * finishing touches on special items and artefacts, giving charges to wands and
 * staffs, giving fuel to lites, and placing traps on chests.
 *
 * In particular, note that "Instant Artefacts", if "created" by an external
 * routine, must pass through this function to complete the actual creation.
 *
 * The base chance of the item being "fine" increases with the "level"
 * parameter, which is usually derived from the dungeon level, being equal
 * to (level)%.
 * The chance that the object will be "special" (special item or artefact),
 * is also (level)%.
 * If "good" is true, then
 * the object is guaranteed to be either "fine" or "special".
 * If "great" is true, then the object is guaranteed to be
 * both "fine" and "special".
 *
 * If "okay" is true, and the object is going to be "special", then there is
 * a chance that an artefact will be created.  This is true even if both the
 * "good" and "great" arguments are false.  Objects which have both "good" and
 * "great" flags get three extra "attempts" to become an artefact.
 *
 * If "allow_insta" is true, then INSTA_ART artefacts can be generated
 *
 * Note that in the above we are using the new terminology of 'fine' and
 * 'special' where Vanilla Angband used 'good' and 'great'. A big change is that
 * these are now independent: you can have ego items that don't have extra
 * mundane bonuses
 * (+att, +evn, +sides...)
 */
void apply_magic(object_type* o_ptr, int lev, bool okay, bool good, bool great,
    bool allow_insta)
{
    int i, artefact_rolls;

    bool fine = false;
    bool special = false;

    /* Maximum "level" for various things */
    if (lev > MAX_DEPTH - 1)
        lev = MAX_DEPTH - 1;

    /* Roll for "fine" */
    if (percent_chance(lev * 2))
        fine = true;

    /* Roll for "special" */
    if (percent_chance(lev))
        special = true;

    /* guarantee "fine" or "special" for "good" drops */
    if (good)
    {
        if (one_in_(2))
            fine = true;
        else
            special = true;
    }

    /* guarantee "fine" and "special" for "great" drops */
    if (great)
    {
        fine = true;
        special = true;
    }

    /* Assume no rolls */
    artefact_rolls = 0;

    if (special)
        artefact_rolls = 1;

    if (great)
        artefact_rolls = 3;

    /* Get 8 rolls if good and great are both set */
    if ((good) && (great))
        artefact_rolls = 8;

    /* Get no rolls if not allowed */
    if (!okay || o_ptr->name1)
        artefact_rolls = 0;

    /* Roll for artefacts if allowed */
    for (i = 0; i < artefact_rolls; i++)
    {
        /* Roll for an artefact */
        if (make_artefact(o_ptr, allow_insta))
            break;
    }

    /* Hack -- analyze artefacts */
    if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];

        /* Artifact tracking based on generation context:
         * - Monsters: Mark as created (INSTA_ARTs are monster-specific, won't spawn elsewhere)
         * - Chests: Mark as created+seen immediately (prevents chest scumming)
         * - Ground: Mark as created but NOT seen (check visibility later)
         */
        if (object_generation_mode == OB_GEN_MODE_CHEST)
        {
            /* Chest artifacts: mark as created and seen immediately */
            a_ptr->cur_num = 1;
            o_ptr->ident |= IDENT_ARTIFACT_SEEN;
        }
        else if (object_generation_mode == OB_GEN_MODE_NORMAL)
        {
            /* Ground artifacts: mark as created but not yet seen */
            a_ptr->cur_num = 1;
            /* Don't set IDENT_ARTIFACT_SEEN yet - wait for player visibility */
        }
        else
        {
            /* Monster/special artifacts: mark as created (INSTA_ART are unique to monsters) */
            a_ptr->cur_num = 1;
            /* Don't mark as seen - player may not encounter the monster */
        }

        object_into_artefact(o_ptr, a_ptr);

        /* Mega-Hack -- increase the rating */
        rating += 10;

        /* Set the good item flag */
        good_item_flag = true;

        /* Cheat -- peek at the item */
        if (cheat_peek)
            object_mention(o_ptr);

        // pseudo-id the item
        pseudo_id(o_ptr);

        // keep count of artefacts generated (not including insta-arts)
        if (!(a_ptr->flags3 & (TR3_INSTA_ART)))
            p_ptr->artefacts++;

        /* Done */
        return;
    }

    /* Apply magic */
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOW:
    {
        // deal with special items
        if (special)
        {
            int ego_power;

            ego_power = make_special_item(o_ptr, (bool)(good || great));

            // if we were unlucky enough to have no valid special types
            // then at least let it be a fine item
            if (ego_power == 0)
                fine = true;
        }

        // deal with fine items
        if (fine)
        {
            a_m_aux_1(o_ptr, lev);
        }

        // Throwing weapons keep their rolled weight; a generated multi-item
        // stack shares that one roll.
        if ((k_info[o_ptr->k_idx].flags3 & (TR3_THROWING))
            && !artefact_p(o_ptr))
        {
            // often come in multiples, but limited to quiver stack size
            if (one_in_(2))
            {
                int stack_limit = object_stack_limit(o_ptr);
                int max_spawn = (stack_limit < 5) ? stack_limit : 5;
                int min_spawn = (max_spawn < 2) ? 1 : 2;
                o_ptr->number = rand_range(min_spawn, max_spawn);
            }
        }

        break;
    }
    case TV_ARROW:
    {
        // note that arrows can't be both fine and special

        if (special)
        {
            // More special arrows lower down
            make_special_item(o_ptr, (bool)(good || great));
            if (o_ptr->number > 1)
                o_ptr->number /= 2;
        }

        else if (fine)
        {
            a_m_aux_1(o_ptr, lev);
            if (o_ptr->number > 1)
                o_ptr->number /= 2;
        }

        break;
    }

    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_HELM:
    case TV_CROWN:
    case TV_CLOAK:
    case TV_GLOVES:
    case TV_BOOTS:
    {
        if (special)
        {
            int ego_power;

            ego_power = make_special_item(o_ptr, (bool)(good || great));

            // if we were unlucky enough to have no valid special types
            // then at least let it be a fine item
            if (ego_power == 0)
                fine = true;
        }

        if (fine)
        {
            a_m_aux_2(o_ptr, lev);
        }

        break;
    }

    case TV_LIGHT:
    {
        if (special)
        {
            make_special_item(o_ptr, (bool)(good || great));
        }

        /* Fuel it */
        a_m_aux_4(o_ptr, lev, fine, special);
        break;
    }

    default:
    {
        a_m_aux_4(o_ptr, lev, fine, special);
        break;
    }
    }

    /* Hack -- analyze special items */
    if (object_has_ego(o_ptr))
    {
        // apply all the bonuses for the given special item type
        object_into_special(o_ptr, lev, false);

        /* Done */
        return;
    }

    /* Examine real objects */
    if (o_ptr->k_idx)
    {
        object_kind* k_ptr = &k_info[o_ptr->k_idx];

        /* Hack -- acquire "broken" flag */
        if (!k_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);

        /* Hack -- acquire "cursed" flag */
        if (k_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
            o_ptr->ident |= (IDENT_CURSED);

        // identify non-special non-artefact weapons/armour
        switch (o_ptr->tval)
        {
        case TV_DIGGING:
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_SWORD:
        case TV_BOW:
        case TV_ARROW:
        case TV_MAIL:
        case TV_SOFT_ARMOR:
        case TV_SHIELD:
        case TV_HELM:
        case TV_CROWN:
        case TV_CLOAK:
        case TV_GLOVES:
        case TV_BOOTS:
        case TV_LIGHT:
        {
            /* Identify it */
            object_aware(o_ptr);
            object_known(o_ptr);
        }
        }
    }
}

#if 0
/*
 * Hack -- determine if a template is "great".
 *
 * Note that this test only applies to the object *kind*, so it is
 * possible to choose a kind which is "great", and then later cause
 * the actual object to be cursed.  We do explicitly forbid objects
 * which are known to be boring or which start out somewhat damaged.
 */
static bool kind_is_great(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Armor -- great */
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    {
        return (true);
    }

    /* Weapons -- great */
    case TV_BOW:
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        return (true);
    }

    /* Chests -- great */
    case TV_CHEST:
    {
        return (true);
    }
    }

    /* Assume not great */
    return (false);
}
#endif

#if 0
/*
 * Hack -- determine if a template is a chest.
 *
 */
static bool kind_is_chest(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Chests -- */
    case TV_CHEST:
    {
        return (true);
    }
    }

    /* Assume not chest */
    return (false);
}
#endif

/*
 * Hack -- determine if a template is footwear.
 *
 */
static bool kind_is_boots(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* footwear -- */
    case TV_BOOTS:
    {
        return (true);
    }
    }

    /* Assume not footwear */
    return (false);
}

/*
 * Hack -- determine if a template is headgear.
 *
 */
static bool kind_is_headgear(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Headgear -- Suitable */
    case TV_HELM:
    case TV_CROWN:
    {
        return (true);
    }
    }

    /* Assume not headgear */
    return (false);
}

/*
 * Hack -- determine if a template is armor.
 *
 */
static bool kind_is_armor(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Armor -- suitable */
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    {
        return (true);
    }
    }

    /* Assume not armor */
    return (false);
}

/*
 * Hack -- determine if a template is gloves.
 *
 */
static bool kind_is_gloves(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Gloves -- suitable */
    case TV_GLOVES:
    {
        return (true);
    }
    }

    /* Assume not suitable  */
    return (false);
}

/*
 * Hack -- determine if a template is a cloak.
 *
 */
static bool kind_is_cloak(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
        /* Cloaks -- suitable */

    case TV_CLOAK:
    {
        return (true);
    }
    }

    /* Assume not a suitable  */
    return (false);
}

/*
 * Hack -- determine if a template is a shield.
 *
 */
static bool kind_is_shield(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* shield -- suitable */
    case TV_SHIELD:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Hack -- determine if a template is a bow/arrow.
 */

static bool kind_is_bow(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* All bows and arrows are suitable  */
    case TV_BOW:
    {
        return (true);
    }
    }

    /* Assume not suitable  */
    return (false);
}

/*
 * Hack -- determine if a template is a "good" digging tool
 *
 */
static bool kind_is_digging_tool(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Diggers -- Good */
    case TV_DIGGING:
    {
        return (true);
    }
    }

    /* Assume not good */
    return (false);
}

/*
 * Hack -- determine if a template is a edged weapon.
 */
static bool kind_is_edged(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Edged Weapons -- suitable */
    case TV_SWORD:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Hack -- determine if a template is a polearm.
 */
static bool kind_is_polearm(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Weapons -- suitable */
    case TV_POLEARM:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Hack -- determine if a template is a weapon.
 */
static bool kind_is_weapon(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Weapons -- suitable */
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

#if 0
/*
 * Hack -- determine if a potion is good for a chest.
 * includes herb of restoring
 *
 */
static bool kind_is_potion(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /*potions suitable for a chest*/
    case TV_POTION:
    {
        if (k_ptr->sval == SV_POTION_QUICKNESS)
            return (true);
        if (k_ptr->sval == SV_POTION_MIRUVOR)
            return (true);
        if (k_ptr->sval == SV_POTION_HEALING)
            return (true);
        return (false);
    }

    case TV_FOOD:
        /* HACK -  herbs of restoring can be with potions */
        {
            if ((k_ptr->sval == SV_FOOD_RESTORATION)
                && ((k_ptr->level + 5) >= object_level))
                return (true);
            return (false);
        }
    }

    /* Assume not suitable */
    return (false);
}
#endif

#if 0
/*
 * Hack -- determine if a staff is good for a chest.
 *
 */
static bool kind_is_staff(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    if (k_ptr->tval == TV_STAFF || k_ptr->tval == TV_GEM)
    {
        /*staffs suitable for a chest*/
        if (k_ptr->sval == SV_STAFF_UNDERSTANDING)
            return (true);
        if (k_ptr->sval == SV_STAFF_TREASURES)
            return (true);
        if (k_ptr->sval == SV_STAFF_SLUMBER)
            return (true);
        if (k_ptr->sval == SV_STAFF_WARDING || k_ptr->sval == SV_GEM_WARDING)
            return (true);
        if (k_ptr->sval == SV_STAFF_RECHARGING)
            return (true);
    }

    /* Assume not suitable for a chest */
    return (false);
}
#endif

#if 0
/*
 * Hack -- determine if a template is "jewelry for chests".
 *
 */
static bool kind_is_jewelry(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Crowns are suitable for a chest */
    case TV_CROWN:
    {
        return (true);
    }

    case TV_RING:
    {
        if (k_ptr->sval == SV_RING_STR)
            return (true);
        if (k_ptr->sval == SV_RING_DEX)
            return (true);
        if (k_ptr->sval == SV_RING_EVASION)
            return (true);
        if (k_ptr->sval == SV_RING_ERED_LUIN)
            return (true);
        if (k_ptr->sval == SV_RING_ACCURACY)
            return (true);
        if (k_ptr->sval == SV_RING_BARAHIR)
            return (true);
        if (k_ptr->sval == SV_RING_MELIAN)
            return (true);
        return (false);
    }

    case TV_AMULET:
    {
        if (k_ptr->sval == SV_AMULET_TINFANG_GELION)
            return (true);
        if (k_ptr->sval == SV_AMULET_NIMPHELOS)
            return (true);
        if (k_ptr->sval == SV_AMULET_ELESSAR)
            return (true);
        if (k_ptr->sval == SV_AMULET_DWARVES)
            return (true);
        if (k_ptr->sval == SV_AMULET_BLESSED_REALM)
            return (true);
        if (k_ptr->sval == SV_AMULET_CON)
            return (true);
        if (k_ptr->sval == SV_AMULET_GRA)
            return (true);
        if (k_ptr->sval == SV_AMULET_PROTECTION)
            return (true);
        if (k_ptr->sval == SV_AMULET_VIGILANT_EYE)
            return (true);
        if (k_ptr->sval == SV_AMULET_LAST_CHANCES)
            return (true);
        return (false);
    }
    }

    /* Assume not suitable for a chest */
    return (false);
}
#endif

#if 0
/*
 * Hack -- determine if a template is "good".
 *
 * Note that this test only applies to the object *kind*, so it is
 * possible to choose a kind which is "good", and then later cause
 * the actual object to be cursed.  We do explicitly forbid objects
 * which are known to be boring or which start out somewhat damaged.
 */
static bool kind_is_good(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Armor -- Good */
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    {
        return (true);
    }

    /* Weapons -- Good */
    case TV_BOW:
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    {
        return (true);
    }

    /* Arrows -- Good */
    case TV_ARROW:
    {
        return (true);
    }

    /* Rings -- Rings of Speed are good */
    case TV_RING:
    {
        return (false);
    }

    /*the very powerful healing potions can be good*/
    case TV_POTION:
    {
        if (k_ptr->sval == SV_POTION_MIRUVOR)
            return (true);
        if (k_ptr->sval == SV_POTION_QUICKNESS)
            return (true);
        if (k_ptr->sval == SV_POTION_HEALING)
            return (true);
        return (false);
    }

    /* Chests -- Chests are good. */
    case TV_CHEST:
    {
        return (true);
    }
    }

    /* Assume not good */
    return (false);
}
#endif

/*
 * Attempt to make an object (normal or weighted quality)
 *
 * This routine plays nasty games to generate the "special artefacts".
 *
 * This routine uses "object_level" for the "generation level".
 *
 * We assume that the given object has been "wiped".
 */
static void apply_generated_object_rating(object_type* j_ptr, bool* mentioned)
{
    if (!cursed_p(j_ptr) && !broken_p(j_ptr)
        && (k_info[j_ptr->k_idx].level > p_ptr->depth))
    {
        rating += (k_info[j_ptr->k_idx].level - p_ptr->depth);
        if (cheat_peek)
        {
            object_mention(j_ptr);
            if (mentioned)
                *mentioned = true;
        }
    }
}

bool make_object_with_profile(object_type* j_ptr, drop_quality quality,
    int objecttype, const drop_profile* profile)
{
    int depth = object_level;
    bool allow_artefacts = (object_generation_mode == OB_GEN_MODE_CHEST)
        || (object_generation_mode == OB_GEN_MODE_MONSTER_DROP);
    if (!drop_generate_object_profiled(
            depth, quality, objecttype, 0, allow_artefacts, profile, j_ptr))
        return false;

    apply_generated_object_rating(j_ptr, NULL);

    return true;
}

bool make_object(object_type* j_ptr, drop_quality quality, int objecttype)
{
    return make_object_with_profile(j_ptr, quality, objecttype, NULL);
}

bool make_guaranteed_artefact_with_profile(object_type* j_ptr,
    drop_quality quality, int objecttype, const drop_profile* profile)
{
    bool allow_artefacts = (object_generation_mode == OB_GEN_MODE_CHEST)
        || (object_generation_mode == OB_GEN_MODE_MONSTER_DROP);

    if (!allow_artefacts || adult_no_artefacts)
        return false;

    bool mentioned = false;

    if (!drop_generate_guaranteed_artefact(
            object_level, object_level, quality, objecttype, profile, j_ptr))
    {
        return false;
    }

    apply_generated_object_rating(j_ptr, &mentioned);

    rating += 10;
    good_item_flag = true;

    if (cheat_peek && !mentioned)
        object_mention(j_ptr);

    pseudo_id(j_ptr);

    if (j_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[j_ptr->name1];
        if (!(a_ptr->flags3 & TR3_INSTA_ART))
            p_ptr->artefacts++;
    }

    return true;
}

bool make_guaranteed_artefact(object_type* j_ptr, drop_quality quality, int objecttype)
{
    return make_guaranteed_artefact_with_profile(
        j_ptr, quality, objecttype, NULL);
}

/*
 * Set the object theme
 */

/*
 * This is an imcomplete list of themes.  Returns false if theme not found.
 * Used primarily for Randarts
 */
bool prep_object_theme(int themetype)
{
    /*get the store creation mode*/
    switch (themetype)
    {
    case DROP_TYPE_SHIELD:
    {
        get_obj_num_hook = kind_is_shield;
        break;
    }
    case DROP_TYPE_WEAPON:
    {
        get_obj_num_hook = kind_is_weapon;
        break;
    }
    case DROP_TYPE_EDGED:
    {
        get_obj_num_hook = kind_is_edged;
        break;
    }
    case DROP_TYPE_POLEARM:
    {
        get_obj_num_hook = kind_is_polearm;
        break;
    }
    case DROP_TYPE_ARMOR:
    {
        get_obj_num_hook = kind_is_armor;
        break;
    }
    case DROP_TYPE_BOOTS:
    {
        get_obj_num_hook = kind_is_boots;
        break;
    }
    case DROP_TYPE_BOW:
    {
        get_obj_num_hook = kind_is_bow;
        break;
    }
    case DROP_TYPE_CLOAK:
    {
        get_obj_num_hook = kind_is_cloak;
        break;
    }
    case DROP_TYPE_GLOVES:
    {
        get_obj_num_hook = kind_is_gloves;
        break;
    }
    case DROP_TYPE_HEADGEAR:
    {
        get_obj_num_hook = kind_is_headgear;
        break;
    }
    case DROP_TYPE_DIGGING:
    {
        get_obj_num_hook = kind_is_digging_tool;

        break;
    }
    case DROP_TYPE_DAMAGED:
    {
        get_obj_num_hook = kind_is_damaged_item;

        break;
    }

    default:
        return (false);
    }

    /*prepare the allocation table*/
    get_obj_num_prep();

    return (true);
}

/*
 * Let the floor carry an object
 */
s16b floor_carry(int y, int x, object_type* j_ptr)
{
    int n = 0;
    bool under_player = (cave_m_idx[y][x] < 0);

    s16b o_idx;

    s16b this_o_idx, next_o_idx = 0;

    /* Scan objects in that grid for combination */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Check for combination */
        if (object_similar(o_ptr, j_ptr))
        {
            /* Combine the items */
            object_absorb(o_ptr, j_ptr);

            if (under_player)
            {
                o_ptr->marked = true;
                lite_spot(y, x);
            }

            if (j_ptr->number == 0)
            {
                /* Result */
                return (this_o_idx);
            }
        }

        /* Count objects */
        n++;
    }

    /* The stack is already too large */
    if (n > MAX_FLOOR_STACK)
        return (0);

    // Sil: force no stacking
    if (n)
        return (0);

    /* Make an object */
    o_idx = o_pop();

    /* Success */
    if (o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[o_idx];

        /* Structure Copy */
        object_copy(o_ptr, j_ptr);

        /* Location */
        o_ptr->iy = y;
        o_ptr->ix = x;

        /* Forget monster */
        o_ptr->held_m_idx = 0;

        /* Link the object to the pile */
        o_ptr->next_o_idx = cave_o_idx[y][x];

        /* Link the floor to the object */
        cave_o_idx[y][x] = o_idx;

        if (under_player)
            o_ptr->marked = true;

        /* Rearrange to reflect squelching */
        rearrange_stack(y, x);

        /* Notice */
        note_spot(y, x);

        /* Redraw */
        lite_spot(y, x);
    }

    /* Result */
    return (o_idx);
}

/*
 * Let an object fall to the ground at or near a location.
 *
 * The initial location is assumed to be "in_bounds_fully()".
 *
 * This function takes a parameter "chance".  This is the percentage
 * chance that the item will "disappear" instead of drop.  If the object
 * has been thrown, then this is the chance of disappearance on contact.
 *
 * Hack -- this function uses "chance" to determine if it should produce
 * some form of "description" of the drop event (under the player).
 *
 * We check several locations to see if we can find a location at which
 * the object can combine, stack, or be placed. Artefacts and thrown/fired
 * auto-recovery objects will try very hard to be placed, including
 * "teleporting" to a useful grid if needed.
 */
s16b drop_near(object_type* j_ptr, int chance, int y, int x)
{
    int i, k, d, s;

    int bs, bn;
    int by, bx;
    int dy, dx;
    int ty, tx;

    object_type* o_ptr;

    char o_name[80];

    bool flag = false;

    bool plural = false;
    const bool is_silmaril = (j_ptr->tval == TV_LIGHT) && (j_ptr->sval == SV_LIGHT_SILMARIL);
    const bool impact_is_floor =
        (cave_feat[y][x] == FEAT_FLOOR) || (cave_feat[y][x] == FEAT_SUNLIGHT);
    const bool force_place = artefact_p(j_ptr) || is_silmaril || j_ptr->pickup;
    const bool try_hard_place = force_place || impact_is_floor;
    const bool can_clobber = force_place;
    const int scan_radius = try_hard_place ? 10 : 4;
    const int scan_dist2_max = (scan_radius * scan_radius) + 1;

    /* Extract plural */
    if (j_ptr->number != 1)
        plural = true;

    /* Describe object */
    object_desc(o_name, sizeof(o_name), j_ptr, false, 0);

    /* Handle normal "breakage" */
    if (!artefact_p(j_ptr) && percent_chance(chance))
    {
        // The potion breaking message has already been displayed
        if (j_ptr->tval != TV_POTION)
        {
            /* Message */
            msg_format("The %s break%s.", o_name, (plural ? "" : "s"));
        }

        /* Debug */
        // if (p_ptr->wizard) msg_print("Breakage (breakage).");

        /* Failure */
        return (0);
    }

    /* Score */
    bs = -1;

    /* Picker */
    bn = 0;

    /* Default */
    by = y;
    bx = x;

    /* Scan local grids */
    for (dy = -scan_radius; dy <= scan_radius; dy++)
    {
        /* Scan local grids */
        for (dx = -scan_radius; dx <= scan_radius; dx++)
        {
            bool comb = false;
            ////int path_n;
            ////u16b path_g[256];
            ////int ty2, tx2; // store a copy of the target grid that can get
            /// changed by project_path()

            /* Calculate actual distance */
            d = (dy * dy) + (dx * dx);

            /* Ignore distant grids */
            if (d > scan_dist2_max)
                continue;

            /* Location */
            ty = y + dy;
            tx = x + dx;

            // copy of the variables
            ////ty2 = ty;
            ////tx2 = tx;

            /* Skip illegal grids */
            if (!in_bounds_fully(ty, tx))
                continue;

            /* Require line of sight */
            if (!los(y, x, ty, tx))
                continue;

            /* Calculate the path */
            ////path_n = project_path(path_g, 10, p_ptr->py, p_ptr->px, &ty2,
            ///&tx2, PROJECT_NO_CHASM);

            // if there was a chasm in the way, skip this spot
            ////if ((ty != ty2) || (tx != tx2)) continue;

            /* Require floor space */
            if (cave_feat[ty][tx] != FEAT_FLOOR
                && cave_feat[ty][tx] != FEAT_SUNLIGHT)
                continue;

            /* Don't put things under peaceful monsters */
            if (cave_m_idx[ty][tx] > 0 && !attacker_at(ty, tx))
                continue;

            /* No objects */
            k = 0;

            /* Scan objects in that grid */
            for (o_ptr = get_first_object(ty, tx); o_ptr;
                 o_ptr = get_next_object(o_ptr))
            {
                /* Check for possible combination */
                if (object_similar(o_ptr, j_ptr))
                    comb = true;

                /* Count objects */
                k++;
            }

            /* Add new object */
            if (!comb)
                k++;

            // Sil: force no stacking
            if (k > 1)
                continue;

            /* Paranoia */
            if (k > MAX_FLOOR_STACK)
                continue;

            /* Calculate score */
            s = 1000 - (d + k * 5);

            /* Skip bad values */
            if (s < bs)
                continue;

            /* New best value */
            if (s > bs)
                bn = 0;

            /* Apply the randomizer to equivalent values */
            if ((++bn >= 2) && (rand_int(bn) != 0))
                continue;

            /* Keep score */
            bs = s;

            /* Track it */
            by = ty;
            bx = tx;

            /* Okay */
            flag = true;
        }
    }

    /* Handle lack of space */
    if (!flag && !try_hard_place)
    {
        /* Debug */
        if (p_ptr->wizard)
            msg_print("Breakage (no floor space).");

        /* Failure */
        return (0);
    }

    /* Don't silently lose items just because there is no nearby empty floor. */
    for (i = 0; try_hard_place && !flag && (i < 20000); i++)
    {
        /* First try */
        if (i == 0)
        {
            ty = y;
            tx = x;
        }

        /* Bounce around */
        else if (i < 100)
        {
            ty = rand_range(by - 1, by + 1);
            tx = rand_range(bx - 1, bx + 1);
        }

        /* Get deperate and teleport it somewhere*/
        else
        {
            ty = rand_int(p_ptr->cur_map_hgt);
            tx = rand_int(p_ptr->cur_map_wid);
        }

        /* Skip illegal grids */
        if (!in_bounds_fully(ty, tx))
            continue;

        /* Require floor space */
        if (cave_feat[ty][tx] != FEAT_FLOOR && cave_feat[ty][tx] != FEAT_SUNLIGHT)
            continue;

        /* Don't put things under peaceful monsters */
        if (cave_m_idx[ty][tx] > 0 && !attacker_at(ty, tx))
            continue;

        /* Bounce to that location */
        by = ty;
        bx = tx;

        // Clear ordinary junk if this object must be force-placed.
        if (can_clobber && cave_o_idx[ty][tx] != 0)
        {
            object_type* o_ptr = &o_list[cave_o_idx[ty][tx]];
            const bool o_is_silmaril =
                (o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_SILMARIL);
            if (!artefact_p(o_ptr) && !o_is_silmaril)
            {
                /* Delete the object */
                delete_object_idx(cave_o_idx[ty][tx]);
            }
        }

        /* Require an empty grid */
        if (cave_o_idx[by][bx] != 0)
            continue;

        /* Require floor space */
        if (cave_feat[by][bx] != FEAT_FLOOR && cave_feat[by][bx] != FEAT_SUNLIGHT)
            continue;

        /* Okay */
        flag = true;
    }

    /* Give it to the floor */
    s16b o_idx = floor_carry(by, bx, j_ptr);
    if (!o_idx)
    {
        /* Message */
        if (player_has_los_bold(y, x))
        {
            msg_format("The %s disappear%s.", o_name, (plural ? "" : "s"));
        }

        /* Debug */
        if (p_ptr->wizard)
            msg_print("Breakage (too many objects).");

        /* Failure */
        return (0);
    }

    update_stuff();

    /* Sound - material-based drop sound (strictly matches design table) */
    {
        int drop_sound = MSG_DROP_GENERIC;
        const bool is_boots = (j_ptr->tval == TV_BOOTS);
        const bool is_gloves = (j_ptr->tval == TV_GLOVES);
        const bool is_greaves = is_boots &&
            (j_ptr->sval == SV_PAIR_OF_STEEL_GREAVES || j_ptr->sval == SV_PAIR_OF_MITHRIL_GREAVES);
        const bool is_gauntlets = is_gloves && (j_ptr->sval == SV_SET_OF_GAUNTLETS);

        if (j_ptr->tval == TV_POTION || j_ptr->tval == TV_FLASK || j_ptr->tval == TV_GEM ||
            (j_ptr->tval == TV_LIGHT && j_ptr->sval == SV_LIGHT_SILMARIL)) {
            drop_sound = MSG_DROP_GLASS;
        }
        else if (j_ptr->tval == TV_RING || j_ptr->tval == TV_AMULET ||
                 (j_ptr->tval == TV_LIGHT && (j_ptr->sval == SV_LIGHT_FEANORIAN ||
                                               j_ptr->sval == SV_LIGHT_LESSER_JEWEL))) {
            drop_sound = MSG_DROP_SMALL_METAL;
        }
        else if ((j_ptr->tval == TV_SOFT_ARMOR && j_ptr->sval == SV_ROBE) ||
                 j_ptr->tval == TV_FOOD || j_ptr->tval == TV_EASTER || j_ptr->tval == TV_NOTE) {
            drop_sound = MSG_DROP_CLOTH;
        }
        else if ((is_boots && !is_greaves) || (is_gloves && !is_gauntlets) ||
                 (j_ptr->tval == TV_SOFT_ARMOR &&
                  (j_ptr->sval == SV_LEATHER_ARMOR || j_ptr->sval == SV_STUDDED_LEATHER))) {
            drop_sound = MSG_DROP_LEATHER;
        }
        else if (j_ptr->tval == TV_MAIL || j_ptr->tval == TV_SHIELD ||
                 j_ptr->tval == TV_CHEST || j_ptr->tval == TV_METAL || j_ptr->tval == TV_DIGGING ||
                 (j_ptr->tval == TV_HELM && (j_ptr->sval == SV_GREAT_HELM || j_ptr->sval == SV_DWARF_MASK))) {
            drop_sound = MSG_DROP_BIG_METAL;
        }
        else if (j_ptr->tval == TV_SWORD || j_ptr->tval == TV_POLEARM || j_ptr->tval == TV_CROWN ||
                 (j_ptr->tval == TV_HELM && j_ptr->sval != SV_GREAT_HELM && j_ptr->sval != SV_DWARF_MASK) ||
                 (j_ptr->tval == TV_LIGHT && j_ptr->sval == SV_LIGHT_LANTERN) ||
                 is_greaves || is_gauntlets) {
            drop_sound = MSG_DROP_METAL_MEDIUM;
        }
        else if (j_ptr->tval == TV_HAFTED || j_ptr->tval == TV_STAFF || j_ptr->tval == TV_HORN ||
                 j_ptr->tval == TV_ARROW ||
                 (j_ptr->tval == TV_LIGHT && (j_ptr->sval == SV_LIGHT_TORCH ||
                                               j_ptr->sval == SV_LIGHT_MALLORN))) {
            drop_sound = MSG_DROP_WOOD;
        }
        else {
            drop_sound = MSG_DROP_GENERIC;
        }

        /* Only play drop sound while the player is actively in a live dungeon. */
        if (character_dungeon) {
            sound(drop_sound);
        }
    }

    /* Mega-Hack -- no message if "dropped" by player */
    /* Message when an object falls under the player */
    if (chance && (cave_m_idx[by][bx] < 0))
    {
        msg_print("You feel something roll beneath your feet.");
    }

    return (o_idx);
}

/*
 * Scatter some weighted-quality objects near the player
 */
void acquirement(int y1, int x1, int num, drop_quality quality)
{
    object_type* i_ptr;
    object_type object_type_body;
    drop_quality spawn_quality =
        (quality < DROP_QUALITY_GOOD) ? DROP_QUALITY_GOOD : quality;

    /* Acquirement */
    while (num--)
    {
        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        /* Make a good-or-better object (if possible) */
        if (!make_object(i_ptr, spawn_quality, DROP_TYPE_NOT_DAMAGED))
            continue;

        /* Drop the object */
        drop_near(i_ptr, -1, y1, x1);
    }
}

/*
 * Attempt to place an object (normal or weighted quality) at the given location.
 */
void place_object(int y, int x, drop_quality quality, int droptype,
    bool allow_artefacts)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Paranoia */
    if (!in_bounds(y, x))
        return;

    /* Hack -- clean floor space */
    if (!cave_clean_bold(y, x))
        return;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Make an object (if possible) */
    int depth = object_level;
    while (!drop_generate_object(depth, quality, droptype, allow_artefacts, i_ptr))
        continue;

    /* Give it to the floor */
    if (!floor_carry(y, x, i_ptr))
    {
        /* Hack -- Preserve artefacts */
        a_info[i_ptr->name1].cur_num = 0;
    }
}

/*
 * Choose a trap type, place it in the dungeon at the given grid and 'hide' it
 *
 */
void place_trap(int y, int x)
{
    int feat;

    /* Paranoia */
    if (!in_bounds(y, x))
        return;

    /* Require empty, clean, floor grid */
    if (!cave_naked_bold(y, x))
        return;

    bool prefer_web = (p_ptr->depth >= 8)
        && (level_partition_kind_for_point(y, x) == LEVEL_PART_CAVEY);

    /* Pick a trap */
    while (1)
    {
        /* Hack -- pick a trap */
        if (prefer_web && (rand_int(100) < 45))
            feat = FEAT_TRAP_WEB;
        else
            feat = rand_range(FEAT_TRAP_HEAD, FEAT_TRAP_TAIL);

        switch (feat)
        {
        case FEAT_TRAP_false_FLOOR:
        {
            // 5-18
            if (p_ptr->depth < 5)
                continue;
            if (p_ptr->depth > 18)
                continue;

            // skip half the time as they are otherwise too common
            if (one_in_(2))
                continue;
            break;
        }
        case FEAT_TRAP_PIT:
        {
            // 5-10
            if (p_ptr->depth < 5)
                continue;
            if (p_ptr->depth > 10)
                continue;
            break;
        }
        case FEAT_TRAP_SPIKED_PIT:
        {
            // 0, 11-17
            if (p_ptr->depth == 0)
                break;
            if (p_ptr->depth < 11)
                continue;
            if (p_ptr->depth > 17)
                continue;
            break;
        }
        case FEAT_TRAP_DART:
        {
            // 8-15
            if (p_ptr->depth < 8)
                continue;
            if (p_ptr->depth > 15)
                continue;
            break;
        }
        case FEAT_TRAP_GAS_CONF:
        {
            // 1-13
            if (p_ptr->depth < 1)
                continue;
            if (p_ptr->depth > 13)
                continue;
            break;
        }
        case FEAT_TRAP_GAS_MEMORY:
        {
            // removed these for now due to player frustration
            continue;

            // 14-

            // if (p_ptr->depth < 14) continue;
            // break;
        }
        case FEAT_TRAP_ALARM:
        {
            // 0-
            break;
        }
        case FEAT_TRAP_FLASH:
        {
            // 1-
            if (p_ptr->depth < 1)
                continue;
            break;
        }
        case FEAT_TRAP_CALTROPS:
        {
            // 0-
            break;
        }
        case FEAT_TRAP_ROOST:
        {
            // 0, 3-6
            if (p_ptr->depth == 0)
                break;
            if (p_ptr->depth < 3)
                continue;
            if (p_ptr->depth > 6)
                continue;
            break;
        }
        case FEAT_TRAP_WEB:
        {
            int d, dir, floor_count = 0;

            // 8-
            if (p_ptr->depth < 8)
                continue;

            // make sure there are at least two adjacent floor squares
            for (d = 0; d < 8; d++)
            {
                dir = cycle[d];

                if (cave_floor_bold(y + ddy[dir], x + ddx[dir]))
                    floor_count++;
            }
            if (floor_count < 2)
                continue;

            break;
        }
        case FEAT_TRAP_DEADFALL:
        {
            // 0, 14-
            if (p_ptr->depth == 0)
                break;
            if (p_ptr->depth < 14)
                continue;
            break;
        }
        case FEAT_TRAP_ACID:
        {
            // 1-
            if (p_ptr->depth < 1)
                continue;
            break;
        }
        case FEAT_TRAP_IMPRISONMENT:
        {
            // 6-
            if (p_ptr->depth < 6)
                continue;

            // skip half the time as they are otherwise too common
            if (one_in_(2))
                continue;

            break;
        }
        }

        /* Done */
        break;
    }

    /* Activate the trap */
    cave_set_feat(y, x, feat);

    // Hide the trap
    cave_info[y][x] |= (CAVE_HIDDEN);
}

/*
 *  Reveal a trap and mark its location on the map.
 */
void reveal_trap(int y, int x)
{
    // remove the 'hidden' flag from the grid
    cave_info[y][x] &= ~(CAVE_HIDDEN);

    /* Notice/Redraw */
    if (character_dungeon)
    {
        /* Notice */
        note_spot(y, x);

        /* Hack -- Memorize */
        cave_info[y][x] |= (CAVE_MARK);

        /* Redraw */
        lite_spot(y, x);
    }
}

/*
 * Place a secret door at the given location
 */
void place_secret_door(int y, int x)
{
    /* Create secret door */
    cave_set_feat(y, x, FEAT_SECRET);
}

/*
 * Place a random type of closed door at the given location.
 */
void place_closed_door(int y, int x)
{
    int tmp, power;

    /* Choose an object */
    tmp = rand_int(100);

    // vault generation
    if (cave_info[y][x] & (CAVE_ICKY))
    {
        /* Closed doors (88%) */
        if (tmp < 88)
        {
            /* Create closed door */
            cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
        }

        /* Locked doors (8%) */
        else if (tmp < 96)
        {
            /* Create locked door */
            power = (10 + p_ptr->depth + dieroll(15)) / 5;
            power = MIN(7, power);
            cave_set_feat(y, x, FEAT_DOOR_HEAD + power);
        }

        /* Jammed doors (4%) */
        else
        {
            /* Create jammed door */
            power = (10 + p_ptr->depth + dieroll(15)) / 5;
            power = MIN(7, power);
            cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x08 + power);
        }
    }

    // normal generation
    else
    {
        /* Closed doors (75%) */
        if (tmp < 75)
        {
            /* Create closed door */
            cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
        }

        /* Locked doors (24%) */
        else if (tmp < 99)
        {
            /* Create locked door */
            power = (p_ptr->depth + dieroll(15)) / 5;
            power = MIN(7, power);
            cave_set_feat(y, x, FEAT_DOOR_HEAD + power);
        }

        /* Stuck doors (1%) */
        else
        {
            /* Create jammed door */
            power = (p_ptr->depth + dieroll(15)) / 5;
            power = MIN(7, power);
            cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x08 + power);
        }
    }
}

/*
 * Place a random type of door at the given location.
 */
void place_random_door(int y, int x)
{
    int tmp;

    /* Choose an object */
    tmp = rand_int(60 + p_ptr->depth);

    /* Open doors */
    if (tmp < 20)
    {
        /* Create open door */
        cave_set_feat(y, x, FEAT_OPEN);
    }

    /* Closed, locked, or stuck doors */
    else if (tmp < 60)
    {
        /* Create closed door */
        place_closed_door(y, x);
    }

    /* Secret doors */
    else
    {
        /* Create secret door */
        cave_set_feat(y, x, FEAT_SECRET);
    }
}

/*
 * Place a random type of forge at the given location.
 */
void place_forge(int y, int x)
{
    int uses, power, p, effective_depth, i;

    effective_depth = p_ptr->depth;

    if (cave_info[y][x] & (CAVE_G_VAULT))
    {
        effective_depth *= 2;
    }

    power = 1;

    // roll once per level of depth and keep the best roll
    for (i = 0; i < effective_depth; i++)
    {
        p = dieroll(1000);

        power = MAX(power, p);
    }

    uses = 2 + damroll(1, 2);

    // to prevent start-scumming on the initial forge
    if (p_ptr->depth <= 2)
    {
        uses = 3;
        power = 0;
    }

    // unique forge
    if ((power >= 1000) && !p_ptr->unique_forge_made)
    {
        uses = 3;
        cave_set_feat(y, x, FEAT_FORGE_UNIQUE_HEAD + uses);

        p_ptr->unique_forge_made = true;

        if (cheat_room)
            msg_print("Orodruth.");
    }

    // enchanted forge
    else if (power >= 990)
    {
        cave_set_feat(y, x, FEAT_FORGE_GOOD_HEAD + uses);
        if (cheat_room)
            msg_print("Enchanted forge.");
    }

    // normal forge
    else
    {
        cave_set_feat(y, x, FEAT_FORGE_NORMAL_HEAD + uses);
        if (cheat_room)
            msg_print("Forge.");
    }
}

/*
 * Describe the charges on an item in the inventory.
 */
void inven_item_charges(int item)
{
    if (!inven_index_valid(item, "inven_item_charges"))
        return;

    int visible_charges = 0;
    object_type* o_ptr = &inventory[item];

    /* Require staff */
    if (o_ptr->tval != TV_STAFF)
        return;

    /* Require known item */
    if (!object_known_p(o_ptr))
        return;

    visible_charges = (o_ptr->pval + CHANNELING_CHARGE_MULTIPLIER - 1)
        / CHANNELING_CHARGE_MULTIPLIER;
    if (visible_charges < 0)
        visible_charges = 0;

    /* Print a message */
    msg_format("You have %d charge%s remaining.", visible_charges,
        (visible_charges != 1) ? "s" : "");
}

/*
 * Describe an item in the inventory.
 */
void inven_item_describe(int item)
{
    if (!inven_index_valid(item, "inven_item_describe"))
        return;

    object_type* o_ptr = &inventory[item];

    char o_name[80];

    if (artefact_p(o_ptr) && object_known_p(o_ptr))
    {
        /* Get a description */
        object_desc(o_name, sizeof(o_name), o_ptr, false, 3);

        /* Print a message */
        msg_format(
            "You no longer have the %s (%c).", o_name, index_to_label(item));
    }
    else
    {
        /* Get a description */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Print a message */
        msg_format("You have %s (%c).", o_name, index_to_label(item));
    }
}

/*
 * Increase the "number" of an item in the inventory
 */
void inven_item_increase(int item, int num)
{
    if (!inven_index_valid(item, "inven_item_increase"))
        return;

    object_type* o_ptr = &inventory[item];

    /* Log staff number changes for debugging */
    if (o_ptr->tval == TV_STAFF)
    {
        log_debug("inven_item_increase called on staff at slot %d: num=%d, current number=%d pval=%d k_idx=%d sval=%d",
                  item, num, o_ptr->number, o_ptr->pval, o_ptr->k_idx, o_ptr->sval);
    }

    /* Apply */
    num += o_ptr->number;

    /* Bounds check */
    if (num > 255)
        num = 255;
    else if (num < 0)
        num = 0;

    /* Un-apply */
    num -= o_ptr->number;

    /* Change the number and weight */
    if (num)
    {
        /* Add the number */
        o_ptr->number += num;

        /* Log staff number after change */
        if (o_ptr->tval == TV_STAFF)
        {
            log_debug("inven_item_increase: staff at slot %d now has number=%d (changed by %d)",
                      item, o_ptr->number, num);
            if (o_ptr->number == 0)
            {
                log_error("WARNING: Staff number changed to 0! This will cause deletion. k_idx=%d sval=%d pval=%d",
                          o_ptr->k_idx, o_ptr->sval, o_ptr->pval);
            }
        }

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Recalculate mana XXX */
        p_ptr->update |= (PU_MANA);

        /* Combine the pack */
        p_ptr->notice |= (PN_COMBINE);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }
}

/*
 * Erase an inventory slot if it has no more items
 */
void inven_item_optimize(int item)
{
    if (!inven_index_valid(item, "inven_item_optimize"))
        return;

    object_type* o_ptr = &inventory[item];

    /* Only optimize real items */
    if (!o_ptr->k_idx)
        return;

    /* Log staff optimization attempts for debugging */
    if (o_ptr->tval == TV_STAFF)
    {
        log_debug("inven_item_optimize called on staff at slot %d: k_idx=%d sval=%d pval=%d number=%d",
                  item, o_ptr->k_idx, o_ptr->sval, o_ptr->pval, o_ptr->number);
    }

    /* Only optimize empty items */
    if (o_ptr->number)
        return;

    /* Log staff deletion */
    if (o_ptr->tval == TV_STAFF)
    {
        log_error("STAFF DELETION BUG: Deleting staff at slot %d with number=0! k_idx=%d sval=%d pval=%d",
                  item, o_ptr->k_idx, o_ptr->sval, o_ptr->pval);
    }

    /* The item is in the pack */
    if (item < INVEN_WIELD)
    {
        int i;

        /* One less item */
        p_ptr->inven_cnt--;

        /* Slide everything down */
        for (i = item; i < INVEN_PACK; i++)
        {
            /* Hack -- slide object */
            memcpy(&inventory[i], &inventory[i + 1], sizeof(object_type));
        }

        /* Hack -- wipe hole */
        memset(&inventory[i], 0, sizeof(object_type));

        /* Window stuff */
        p_ptr->window |= (PW_INVEN);
    }

    /* The item is being wielded */
    else
    {
        /* One less item */
        p_ptr->equip_cnt--;

        /* Erase the empty slot */
        object_wipe(&inventory[item]);

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Recalculate mana XXX */
        p_ptr->update |= (PU_MANA);

        /* Window stuff */
        p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);

        p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
    }
}

/*
 * Describe the charges on an item on the floor.
 */
void floor_item_charges(int item)
{
    int visible_charges = 0;
    object_type* o_ptr = &o_list[item];

    /* Require staff */
    if (o_ptr->tval != TV_STAFF)
        return;

    /* Require known item */
    if (!object_known_p(o_ptr))
        return;

    visible_charges = (o_ptr->pval + CHANNELING_CHARGE_MULTIPLIER - 1)
        / CHANNELING_CHARGE_MULTIPLIER;
    if (visible_charges < 0)
        visible_charges = 0;

    /* Print a message */
    msg_format("There are %d charge%s remaining.", visible_charges,
        (visible_charges != 1) ? "s" : "");
}

/*
 * Describe an item on the floor.
 */
void floor_item_describe(int item)
{
    object_type* o_ptr = &o_list[item];

    char o_name[80];

    /* Get a description */
    object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Print a message */
    if (!p_ptr->blind)
        msg_format("You see %s.", o_name);
}

/*
 * Increase the "number" of an item on the floor
 */
void floor_item_increase(int item, int num)
{
    object_type* o_ptr = &o_list[item];

    /* Apply */
    num += o_ptr->number;

    /* Bounds check */
    if (num > 255)
        num = 255;
    else if (num < 0)
        num = 0;

    /* Un-apply */
    num -= o_ptr->number;

    /* Change the number */
    o_ptr->number += num;
}

/*
 * Optimize an item on the floor (destroy "empty" items)
 */
void floor_item_optimize(int item)
{
    object_type* o_ptr = &o_list[item];

    /* Paranoia -- be sure it exists */
    if (!o_ptr->k_idx)
        return;

    /* Only optimize empty items */
    if (o_ptr->number)
        return;

    /* Delete the object */
    delete_object_idx(item);
}

/*
 *  overflow the player's backpack if needed
 */
void check_pack_overflow(void)
{
    if (inventory[INVEN_PACK].k_idx)
    {
        int item = INVEN_PACK;

        char o_name[80];

        object_type* o_ptr;

        /* Get the slot to be dropped */
        o_ptr = &inventory[item];

        /* Disturbing */
        disturb(0, 0);

        /* Warning */
        msg_print("Your pack overflows!");

        /* Describe */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Message */
        msg_format("You drop %s (%c).", o_name, index_to_label(item));

        /* Drop it (carefully) near the player */
        drop_near(o_ptr, 0, p_ptr->py, p_ptr->px);

        /* Modify, Describe, Optimize */
        inven_item_increase(item, -255);
        inven_item_describe(item);
        inven_item_optimize(item);

        /* Notice stuff (if needed) */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff (if needed) */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff (if needed) */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Window stuff (if needed) */
        if (p_ptr->window)
            window_stuff();
    }
}

/*
 * Check if we have space for an item in the pack without overflow
 */
bool inven_carry_okay(const object_type* o_ptr)
{
    int j;

    clear_inventory_limit_failure();

    if (!player_light_capacity_okay(o_ptr, true))
        return false;

    // Check for combining in quiver first
    if (o_ptr->tval == TV_ARROW)
    {
        int empty_quiver = 0;

        for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
        {
            object_type* j_ptr = &inventory[j];

            if (!j_ptr->k_idx)
            {
                if (empty_quiver == 0)
                    empty_quiver = j;
                continue;
            }

            if (object_similar(j_ptr, o_ptr))
                return (true);
        }

        if ((empty_quiver > 0) && o_ptr->pickup)
            return (true);
    }

    /* Throwing weapons can combine with similar items in quiver, 
       or go back to their original empty quiver slot */
    if (player_can_treat_as_throwing(o_ptr))
    {
        int empty_quiver = 0;
        bool has_desired_slot = (o_ptr->pickup_slot == INVEN_QUIVER1) || 
                                (o_ptr->pickup_slot == INVEN_QUIVER2);
        
        for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
        {
            object_type* j_ptr = &inventory[j];

            if (!j_ptr->k_idx)
            {
                if (empty_quiver == 0)
                    empty_quiver = j;
                continue;
            }

            if (object_similar(j_ptr, o_ptr))
                return (true);
        }
        
        /* Thrown items can go back to an empty quiver slot */
        if ((empty_quiver > 0) && o_ptr->pickup)
            return (true);
            
        /* Or specifically to their original slot if it's empty */
        if (has_desired_slot && (inventory[o_ptr->pickup_slot].k_idx == 0))
            return (true);
    }

    /*
     * Per-item capped gear should check the cap before pack merges can hide
     * extra copies inside an existing stack. Stack-counted gear waits until
     * after similar-stack merges so one pack still counts as one unit.
     */
    if (!inventory_limit_is_stack_counted(o_ptr)
        && !inventory_type_slot_available(o_ptr, true))
    {
        return (false);
    }

    /* Similar slot? */
    for (j = 0; j < INVEN_PACK; j++)
    {
        object_type* j_ptr = &inventory[j];

        if (!j_ptr->k_idx)
            continue;

        if (object_similar(j_ptr, o_ptr))
            return (true);
    }

    if (!inventory_type_slot_available(o_ptr, true))
        return (false);

    bool supply_item = supplies_is_supply_object(o_ptr);
    bool supplies_present = (supplies_entry_count() > 0);
    int logical_items = p_ptr->inven_cnt + (supplies_present ? 1 : 0);

    if (supply_item)
    {
        if (!supplies_present)
        {
            /* Need to allocate one slot for the supplies bundle. */
            if (logical_items >= INVEN_PACK)
                return (false);
        }

        /* Check if the item would exceed the supply weight limit */
        if (!supplies_can_absorb_object(o_ptr))
        {
            /* Check if we can do partial pickup */
            int max_qty = supplies_max_absorbable_quantity(o_ptr);
            if (max_qty > 0 && o_ptr->number > 1)
            {
                /* Partial pickup is possible, allow it through */
                return (true);
            }
            
            /* Can't pick up any, show error */
            set_inventory_limit_failure(INV_LIMIT_SUPPLY_WEIGHT,
                supplies_current_weight_cap() / 10, o_ptr);
            return (false);
        }

        return (true);
    }

    /* Non-supply item */
    if (logical_items >= INVEN_PACK)
        return (false);

    return (true);
}

bool inven_carry_okay_after_removing(
    const object_type* o_ptr, int remove_item, int remove_amt)
{
    object_type saved_item;
    bool had_removed_item = false;
    s16b saved_inven_cnt = p_ptr->inven_cnt;
    bool result;

    if (!o_ptr)
        return false;

    clear_inventory_limit_failure();

    /* Simulate removing the source pack item so swap prompts reflect the real outcome. */
    if (remove_item >= 0 && remove_item < INVEN_PACK && remove_amt > 0
        && inventory[remove_item].k_idx)
    {
        object_copy(&saved_item, &inventory[remove_item]);
        had_removed_item = true;

        if (remove_amt >= inventory[remove_item].number)
        {
            object_wipe(&inventory[remove_item]);
            p_ptr->inven_cnt--;
        }
        else
        {
            inventory[remove_item].number -= remove_amt;
        }
    }

    result = inven_carry_okay(o_ptr);

    if (had_removed_item)
    {
        object_copy(&inventory[remove_item], &saved_item);
        p_ptr->inven_cnt = saved_inven_cnt;
    }

    clear_inventory_limit_failure();
    return result;
}

/*
 * Add an item to the players inventory, and return the slot used.
 *
 * If the new item can combine with an existing item in the inventory,
 * it will do so, using "object_similar()" and "object_absorb()", else,
 * the item will be placed into the "proper" location in the inventory.
 *
 * This function can be used to "over-fill" the player's pack, but only
 * once, and such an action must trigger the "overflow" code immediately.
 * Note that when the pack is being "over-filled", the new item must be
 * placed into the "overflow" slot, and the "overflow" must take place
 * before the pack is reordered, but (optionally) after the pack is
 * combined.  This may be tricky.  See "dungeon.c" for info.
 *
 * Note that this code must remove any location/stack information
 * from the object once it is placed into the inventory.
 */
s16b inven_carry(object_type* o_ptr, bool combine_ammo)
{
    int i = 1; // default value to soothe compilation warnings
    int j, k;
    int n = -1;

    object_type* j_ptr;

    clear_inventory_limit_failure();

    /*paranoia, don't pick up "&nothings"*/
    if (!o_ptr->k_idx)
        return (-1);

    if (!player_light_capacity_okay(o_ptr, true))
        return (-1);

    if (supplies_is_supply_object(o_ptr))
    {
        object_type copy;
        object_copy(&copy, o_ptr);
        if (supplies_absorb_object(&copy))
        {
            object_wipe(o_ptr);
            return SUPPLIES_INDEX;
        }
        set_inventory_limit_failure(INV_LIMIT_SUPPLY_WEIGHT,
            supplies_current_weight_cap() / 10, o_ptr);
        return (-1);
    }

    int desired_slot = o_ptr->pickup_slot;
    bool wanted_auto_recover = o_ptr->pickup ? true : false;
    bool wants_throw_slot = (desired_slot == INVEN_QUIVER1) || (desired_slot == INVEN_QUIVER2);

    if (wants_throw_slot)
    {
        object_type* d_ptr = &inventory[desired_slot];
        bool is_throwing = player_can_treat_as_throwing(o_ptr);
        bool is_arrow = (o_ptr->tval == TV_ARROW);

        if (is_throwing || is_arrow)
        {
            if (d_ptr->k_idx == 0)
            {
                int limit = object_stack_limit(o_ptr);
                int placed = MIN(o_ptr->number, limit);
                object_copy(d_ptr, o_ptr);
                d_ptr->number = placed;
                d_ptr->pickup = false;
                d_ptr->pickup_slot = -1;
                d_ptr->ident |= IDENT_HANDLED;
                o_ptr->number -= placed;

                p_ptr->equip_cnt++;
                p_ptr->notice |= (PN_COMBINE | PN_REORDER);
                p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

                if (o_ptr->number <= 0)
                {
                    o_ptr->pickup = false;
                    o_ptr->pickup_slot = -1;
                    return (desired_slot);
                }

                o_ptr->pickup = wanted_auto_recover;
                o_ptr->pickup_slot = -1;
            }
            else if (object_similar(d_ptr, o_ptr))
            {
                object_absorb(d_ptr, o_ptr);
                d_ptr->pickup = false;
                d_ptr->pickup_slot = -1;
                d_ptr->ident |= IDENT_HANDLED;
                p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

                if (o_ptr->number == 0)
                {
                    o_ptr->pickup = false;
                    o_ptr->pickup_slot = -1;
                    return (desired_slot);
                }

                o_ptr->pickup = wanted_auto_recover;
                o_ptr->pickup_slot = -1;
            }
        }
        o_ptr->pickup_slot = -1;
    }

    // Check for combining in quiver first
    if (o_ptr->tval == TV_ARROW && combine_ammo)
    {
        int empty_quiver = 0;

        // arrows combine with similar arrows
        for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
        {
            j_ptr = &inventory[j];

            /* Skip non-objects */
            if (!j_ptr->k_idx)
            {
                // keep track of the first empty quiver
                if (empty_quiver == 0)
                    empty_quiver = j;
                continue;
            }

            /* Check if the two items can be combined */
            if (object_similar(j_ptr, o_ptr))
            {
                /* Combine the items */
                object_absorb(j_ptr, o_ptr);
                j_ptr->ident |= IDENT_HANDLED;

                /* Window stuff */
                p_ptr->window |= (PW_INVEN);

                if (o_ptr->number == 0)
                {
                    /* Success */
                    return (j);
                }
                else
                {
                    char j_name[80];

                    // combination message
                    msg_print(
                        "You combine them with the arrows in your quiver.");

                    /* Describe the object */
                    object_desc(j_name, sizeof(j_name), j_ptr, true, 3);

                    /* Message */
                    msg_format("You have %s (%c).", j_name, index_to_label(j));
                }
            }
        }

        // arrows that have been fired can also fit back into an empty quiver
        // slot
        if ((empty_quiver > 0) && o_ptr->pickup)
        {
            o_ptr->pickup = false;
            o_ptr->pickup_slot = -1;

            if ((o_ptr >= o_list) && (o_ptr < o_list + o_max))
            {
                int floor_idx = (int)(o_ptr - o_list);
                do_cmd_wield(o_ptr, 0 - floor_idx);
            }

            return (-1);
        }
    }

    /* Handle throwing weapons - try to combine with existing in quiver first */
    if (player_can_treat_as_throwing(o_ptr))
    {
        int empty_quiver = 0;

        /* Check for combining with existing throwing weapons in quiver */
        for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
        {
            j_ptr = &inventory[j];

            if (!j_ptr->k_idx)
            {
                if (empty_quiver == 0)
                    empty_quiver = j;
                continue;
            }

            if (object_similar(j_ptr, o_ptr))
            {
                object_absorb(j_ptr, o_ptr);
                j_ptr->ident |= IDENT_HANDLED;
                p_ptr->window |= (PW_INVEN | PW_EQUIP);

                if (o_ptr->number == 0)
                    return (j);
                
                /* Partial absorption - show message and continue to pack */
                char j_name[80];
                object_desc(j_name, sizeof(j_name), j_ptr, true, 3);
                msg_format("You combine some with %s (%c).", j_name, index_to_label(j));
                break;
            }
        }

        if ((empty_quiver > 0) && o_ptr->pickup)
        {
            int limit = object_stack_limit(o_ptr);
            int placed = MIN(o_ptr->number, limit);
            object_type* d_ptr = &inventory[empty_quiver];

            object_copy(d_ptr, o_ptr);
            d_ptr->number = placed;
            d_ptr->pickup = false;
            d_ptr->pickup_slot = -1;
            d_ptr->ident |= IDENT_HANDLED;
            o_ptr->number -= placed;
            o_ptr->pickup = false;
            o_ptr->pickup_slot = -1;

            p_ptr->equip_cnt++;
            p_ptr->notice |= (PN_COMBINE | PN_REORDER);
            p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

            if (o_ptr->number <= 0)
                return (empty_quiver);
        }

        /* Any overflow will fall through to pack handling below */
    }

    /*
     * Per-item capped gear should respect item-count limits even when an
     * identical pack stack exists. Stack-counted gear gets checked after the
     * combine pass so adding to an existing pack does not consume a new unit.
     */
    if (!inventory_limit_is_stack_counted(o_ptr)
        && !inventory_type_slot_available(o_ptr, true))
    {
        return (-1);
    }

    /* Check for combining */
    for (j = 0; j < INVEN_PACK; j++)
    {
        j_ptr = &inventory[j];

        /* Skip non-objects */
        if (!j_ptr->k_idx)
            continue;

        /* Hack -- track last item */
        n = j;

        /* Check if the two items can be combined */
        if (object_similar(j_ptr, o_ptr))
        {
            /* Combine the items */
            object_absorb(j_ptr, o_ptr);
            j_ptr->ident |= IDENT_HANDLED;

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS);

            /* Window stuff */
            p_ptr->window |= (PW_INVEN);

            if (o_ptr->number == 0)
            {
                /* Success */
                return (j);
            }
            else
            {
                char j_name[80];

                // combination message
                msg_print("You combine them with some items in your pack.");

                /* Describe the object */
                object_desc(j_name, sizeof(j_name), j_ptr, true, 3);

                /* Message */
                msg_format("You have %s (%c).", j_name, index_to_label(j));
            }
        }
    }

    /* Paranoia */
    if (!inventory_type_slot_available(o_ptr, true))
        return (-1);

    /* Check if we have room, accounting for supplies */
    bool supplies_present = (supplies_entry_count() > 0);
    int logical_items = p_ptr->inven_cnt + (supplies_present ? 1 : 0);
    if (logical_items >= INVEN_PACK)
        return (-1);

    /* Find an empty slot */
    for (j = 0; j <= INVEN_PACK; j++)
    {
        j_ptr = &inventory[j];

        /* Use it if found */
        if (!j_ptr->k_idx)
            break;
    }

    /* Use that slot */
    i = j;

    /* Apply an autoinscription */
    apply_autoinscription(o_ptr);

    /* Reset the pickup flag */
    o_ptr->pickup = false;
    o_ptr->pickup_slot = -1;

    /* Reorder the pack */
    if (i < INVEN_PACK)
    {
        s32b o_value, j_value;

        /* Get the "value" of the item */
        o_value = object_value(o_ptr);

        /* Scan every occupied slot */
        for (j = 0; j < INVEN_PACK; j++)
        {
            j_ptr = &inventory[j];

            /* Use empty slots */
            if (!j_ptr->k_idx)
                break;

            /* Objects sort by decreasing type */
            if (o_ptr->tval > j_ptr->tval)
                break;
            if (o_ptr->tval < j_ptr->tval)
                continue;

            /* Non-aware (flavored) items always come last */
            if (!object_aware_p(o_ptr))
                continue;
            if (!object_aware_p(j_ptr))
                break;

            /* Objects sort by increasing sval */
            if (o_ptr->sval < j_ptr->sval)
                break;
            if (o_ptr->sval > j_ptr->sval)
                continue;

            /* Lites sort by decreasing fuel */
            if (o_ptr->tval == TV_LIGHT)
            {
                if (o_ptr->timeout > j_ptr->timeout)
                    break;
                if (o_ptr->timeout < j_ptr->timeout)
                    continue;
            }

            // This next bit is complicated: identified art > pseudo art >
            // identified special > pseudo special > other

            /* Identified artefacts beat the rest */
            if (!(object_known_p(o_ptr) && artefact_p(o_ptr))
                && (object_known_p(j_ptr) && artefact_p(j_ptr)))
                continue;
            if ((object_known_p(o_ptr) && artefact_p(o_ptr))
                && !(object_known_p(j_ptr) && artefact_p(j_ptr)))
                break;

            /* Then pseudo-identified {artefact} */
            if (!(!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr))
                && (!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr)))
                continue;
            if ((!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr))
                && !(!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr)))
                break;

            /* Then identified specials */
            if (!(object_known_p(o_ptr) && ego_item_p(o_ptr))
                && (object_known_p(j_ptr) && ego_item_p(j_ptr)))
                continue;
            if ((object_known_p(o_ptr) && ego_item_p(o_ptr))
                && !(object_known_p(j_ptr) && ego_item_p(j_ptr)))
                break;

            /* Then pseudo-identified {special} */
            if (!(!object_known_p(o_ptr) && special_pseudo_p(o_ptr))
                && (!object_known_p(j_ptr) && special_pseudo_p(j_ptr)))
                continue;
            if ((!object_known_p(o_ptr) && special_pseudo_p(o_ptr))
                && !(!object_known_p(j_ptr) && special_pseudo_p(j_ptr)))
                break;

            /* Determine the "value" of the pack item */
            j_value = object_value(j_ptr);

            /* Objects sort by decreasing value */
            if (o_value > j_value)
                break;
            if (o_value < j_value)
                continue;

            /* Objects sort by increasing weight */
            if (o_ptr->weight < j_ptr->weight)
                break;
            if (o_ptr->weight > j_ptr->weight)
                continue;
        }

        /* Use that slot */
        i = j;

        /* Slide objects */
        for (k = n; k >= i; k--)
        {
            /* Hack -- Slide the item */
            object_copy(&inventory[k + 1], &inventory[k]);
        }

        /* Wipe the empty slot */
        object_wipe(&inventory[i]);
    }

    /* Copy the item */
    object_copy(&inventory[i], o_ptr);

    /* Get the new object */
    j_ptr = &inventory[i];
    j_ptr->ident |= IDENT_HANDLED;

    int limit = object_stack_limit(j_ptr);
    if (j_ptr->number > limit)
    {
        int excess = j_ptr->number - limit;
        j_ptr->number = limit;
        if (o_ptr != j_ptr)
            o_ptr->number = excess;
    }
    else if (o_ptr != j_ptr)
    {
        o_ptr->number -= j_ptr->number;
    }

    /* Forget stack */
    j_ptr->next_o_idx = 0;

    /* Forget monster */
    j_ptr->held_m_idx = 0;

    /* Forget location */
    j_ptr->iy = j_ptr->ix = 0;

    /* No longer marked */
    j_ptr->marked = false;

    /* Count the items */
    p_ptr->inven_cnt++;

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine and Reorder pack */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN);

    /* Return the slot */
    return (i);
}

bool inven_carry_limit_failed(void)
{
    return carry_limit_last_failed;
}

enum inventory_limit_group inven_carry_limit_group(void)
{
    return carry_limit_last_group;
}

cptr inven_carry_limit_label(void)
{
    if (!carry_limit_last_failed)
        return NULL;

    if (!carry_limit_last_label[0])
        return NULL;

    return carry_limit_last_label;
}

int inven_carry_limit_value(void)
{
    return carry_limit_last_limit;
}

bool inven_carry_limit_is_supply_weight(void)
{
    return carry_limit_last_failed
        && (carry_limit_last_group == INV_LIMIT_SUPPLY_WEIGHT);
}

/*
 * Take off (some of) a non-cursed equipment item
 *
 * Note that only one item at a time can be wielded per slot.
 *
 * Note that taking off an item when "full" may cause that item
 * to fall to the ground.
 *
 * Return the inventory slot into which the item is placed.
 */
s16b inven_takeoff(int item, int amt)
{
    int slot;

    object_type* o_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    cptr act;

    char o_name[80];
    int oil_to_drop = 0;

    /* Get the item to take off */
    o_ptr = &inventory[item];

    /* Paranoia */
    if (amt <= 0)
        return (-1);

    /* Verify */
    if (amt > o_ptr->number)
        amt = o_ptr->number;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain a local object */
    object_copy(i_ptr, o_ptr);

    /* Modify quantity */
    i_ptr->number = amt;

    object_type drop_obj;
    object_copy(&drop_obj, i_ptr);
    drop_obj.pickup = false;
    drop_obj.pickup_slot = -1;

    object_type drop_template;
    object_copy(&drop_template, &drop_obj);

    /* Describe the object */
    object_desc(o_name, sizeof(o_name), i_ptr, true, 3);

    const bool discard_spent_light = (item == INVEN_LITE)
        && player_light_destroyed_on_drop(i_ptr);

    /* Took off weapon */
    if ((item == INVEN_WIELD)
        || ((item == INVEN_ARM) && (i_ptr->tval != TV_SHIELD)))
    {
        act = "You were wielding";
    }

    /* Took off bow */
    else if (item == INVEN_BOW)
    {
        act = "You were holding";
    }

    /* Took off light */
    else if (item == INVEN_LITE)
    {
        act = "You were holding";
    }

    /* Took off arrows */
    else if ((item == INVEN_QUIVER1) || (item == INVEN_QUIVER2))
    {
        act = "You have removed from your quiver";
    }
    else if (item == INVEN_HORN)
    {
        act = "You were carrying";
    }

    /* Took off something */
    else
    {
        act = "You were wearing";
    }

    /* Modify, Optimize */
    log_debug("inven_takeoff: Before decrease - item=%d (k_idx=%d, prefix=%d, suffix=%d, number=%d)",
              item, o_ptr->k_idx, (int)object_ego_prefix(o_ptr), (int)object_ego_suffix(o_ptr), o_ptr->number);
    log_debug("inven_takeoff: Taking off copy - k_idx=%d, prefix=%d, suffix=%d, number=%d",
              i_ptr->k_idx, (int)object_ego_prefix(i_ptr), (int)object_ego_suffix(i_ptr), i_ptr->number);
    inven_item_increase(item, -amt);
    inven_item_optimize(item);

    if (discard_spent_light)
    {
        msg_format("%s %s; %s too spent to keep.", act, o_name,
            (i_ptr->number > 1) ? "they are" : "it is");
        p_ptr->redraw |= (PR_MAP | PR_LIGHT);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        return (-1);
    }

    /*
     * Light-slot supply items should go back into supplies directly when
     * removed, even if the pack is full. This avoids swap flows depending on
     * pack carry checks for an item class that is normally supply-backed.
     */
    if ((item == INVEN_LITE) && supplies_is_supply_object(i_ptr))
    {
        if (supplies_absorb_object(i_ptr))
        {
            char label = supplies_label_char();
            if (!label)
                label = 'a';
            msg_format("%s %s (%c).", act, o_name, label);
            return SUPPLIES_INDEX;
        }
    }

    /* Carry the object */
    log_debug("inven_takeoff: Calling inven_carry with k_idx=%d, prefix=%d, suffix=%d", 
              i_ptr->k_idx, (int)object_ego_prefix(i_ptr), (int)object_ego_suffix(i_ptr));
    slot = inven_carry(i_ptr, false);
    log_debug("inven_takeoff: inven_carry returned slot=%d", slot);

    if (slot == SUPPLIES_INDEX)
    {
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("%s %s (%c).", act, o_name, label);
        return slot;
    }

    if (slot >= 0)
    {
        /* Message */
        msg_format("%s %s (%c).", act, o_name, index_to_label(slot));
        return slot;
    }

    /* Could not carry the item; place it on the floor instead. */
    msg_format("%s %s.", act, o_name);

    if (inven_carry_limit_failed())
    {
        cptr label = inven_carry_limit_label();
        int limit = inven_carry_limit_value();

        if (label)
            msg_format("Your pack cannot hold more %s (limit %d).", label, limit);
        else
            msg_print("You have no room in your pack.");
    }
    else
    {
        msg_print("You have no room in your pack.");
    }

    if (player_oil_container_object(&drop_obj))
    {
        if (!player_prepare_oil_container_drop_after_removal(&drop_obj, amt,
                &oil_to_drop, NULL))
        {
            return (-1);
        }

        player_oil_container_set_fuel(&drop_obj, oil_to_drop);
        object_copy(&drop_template, &drop_obj);
    }

    bool can_drop_here = (cave_feat[p_ptr->py][p_ptr->px] == FEAT_FLOOR
        || cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT);
    s16b o_idx = 0;

    if (can_drop_here)
    {
        o_idx = floor_carry(p_ptr->py, p_ptr->px, &drop_obj);

        if (o_idx > 0)
        {
            msg_print("It lands at your feet.");
            return (0 - o_idx);
        }
    }

    for (int d = 0; d < 8; d++)
    {
        int yy = p_ptr->py + ddy_ddd[d];
        int xx = p_ptr->px + ddx_ddd[d];

        if (!in_bounds_fully(yy, xx))
            continue;

        if (cave_feat[yy][xx] != FEAT_FLOOR
            && cave_feat[yy][xx] != FEAT_SUNLIGHT)
            continue;

        if (cave_o_idx[yy][xx] != 0)
            continue;

        object_copy(&drop_obj, &drop_template);
        o_idx = floor_carry(yy, xx, &drop_obj);
        if (o_idx > 0)
        {
            msg_print("It lands nearby.");
            return (0 - o_idx);
        }
    }

    object_copy(&drop_obj, &drop_template);
    o_idx = drop_near(&drop_obj, 0, p_ptr->py, p_ptr->px);
    if (o_idx > 0)
    {
        msg_print("It falls nearby.");
        return (0 - o_idx);
    }

    msg_print("It falls nearby, but you lose sight of it.");
    return (-1);
}

/*
 * Drop (some of) a non-cursed inventory/equipment item
 *
 * The object will be dropped "near" the current location
 */
void inven_drop(int item, int amt)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int oil_to_drop = 0;

    object_type* o_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    char o_name[120];

    /* Get the original object */
    o_ptr = &inventory[item];

    /* Error check */
    if (amt <= 0)
        return;

    /* Not too many */
    if (amt > o_ptr->number)
        amt = o_ptr->number;

    /* Take off equipment */
    if (item >= INVEN_WIELD)
    {
        /* Take off first */
        item = inven_takeoff(item, amt);

        if (item < 0)
            return;

        /* Get the original object */
        o_ptr = &inventory[item];
    }

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain local object */
    object_copy(i_ptr, o_ptr);

    /* Modify quantity */
    i_ptr->number = amt;

    if (player_oil_container_object(i_ptr))
    {
        if (!player_prepare_oil_container_drop(i_ptr, amt, &oil_to_drop,
                NULL))
            return;
    }

    /* Describe local object */
    object_desc(o_name, sizeof(o_name), i_ptr, true, 3);

    if (player_light_destroyed_on_drop(i_ptr))
    {
        msg_format("You discard %s; %s too spent to keep.",
            o_name, (i_ptr->number > 1) ? "they are" : "it is");

        inven_item_increase(item, -amt);
        inven_item_describe(item);
        inven_item_optimize(item);
        p_ptr->redraw |= (PR_MAP | PR_LIGHT);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        return;
    }

    /* Message */
    msg_format("You drop %s (%c).", o_name, index_to_label(item));

    /* Drop it near the player */
    if (player_oil_container_object(i_ptr) && oil_to_drop > 0)
    {
        int oil_remaining = oil_to_drop;
        int unit_capacity = player_oil_container_unit_capacity(i_ptr);
        for (int n = 0; n < amt; n++)
        {
            object_type single_drop;
            object_wipe(&single_drop);
            object_copy(&single_drop, i_ptr);
            single_drop.number = 1;
            player_oil_container_set_fuel(&single_drop,
                MIN(oil_remaining, unit_capacity));
            oil_remaining -= MIN(oil_remaining, unit_capacity);
            drop_near(&single_drop, 0, py, px);
        }
    }
    else
    {
        drop_near(i_ptr, 0, py, px);
    }

    /* Modify, Describe, Optimize */
    inven_item_increase(item, -amt);
    inven_item_describe(item);
    inven_item_optimize(item);
}

/*
 * Combine items in the pack
 *
 * Note special handling of the "overflow" slot
 */
void combine_pack(void)
{
    int i, j, k;

    object_type* o_ptr;
    object_type* j_ptr;

    bool flag = false;

    /* Combine the pack (backwards) */
    for (i = INVEN_PACK; i > 0; i--)
    {
        /* Get the item */
        o_ptr = &inventory[i];

        /* Skip empty items */
        if (!o_ptr->k_idx)
            continue;

        /* Scan the items above that item */
        for (j = 0; j < i; j++)
        {
            /* Get the item */
            j_ptr = &inventory[j];

            /* Skip empty items */
            if (!j_ptr->k_idx)
                continue;

            /* Can we drop "o_ptr" onto "j_ptr"? */
            if (object_similar(j_ptr, o_ptr))
            {
                /* Take note */
                flag = true;

                /* Add together the item counts */
                object_absorb(j_ptr, o_ptr);

                /* Window stuff */
                p_ptr->window |= (PW_INVEN);

                if (o_ptr->number == 0)
                {
                    /* One object is gone */
                    p_ptr->inven_cnt--;

                    /* Slide everything down */
                    for (k = i; k < INVEN_PACK; k++)
                    {
                        /* Hack -- slide object */
                        memcpy(&inventory[k], &inventory[k + 1], sizeof(object_type));
                    }

                    /* Hack -- wipe hole */
                    object_wipe(&inventory[k]);

                    /* Done */
                    break;
                }
            }
        }
    }

    /* Message */
    if (flag)
        msg_print("You combine some items in your pack.");
}

/*
 * Reorder items in the pack
 *
 * Note special handling of the "overflow" slot
 */
void reorder_pack(bool display_message)
{
    int i, j, k;

    s32b o_value;
    s32b j_value;

    object_type* o_ptr;
    object_type* j_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    bool flag = false;

    /* Re-order the pack (forwards) */
    for (i = 0; i < INVEN_PACK; i++)
    {
        /* Mega-Hack -- allow "proper" over-flow */
        if ((i == INVEN_PACK) && (p_ptr->inven_cnt == INVEN_PACK))
            break;

        /* Get the item */
        o_ptr = &inventory[i];

        /* Skip empty slots */
        if (!o_ptr->k_idx)
            continue;

        /* Get the "value" of the item */
        o_value = object_value(o_ptr);

        /* Scan every occupied slot */
        for (j = 0; j < INVEN_PACK; j++)
        {
            /* Get the item already there */
            j_ptr = &inventory[j];

            /* Use empty slots */
            if (!j_ptr->k_idx)
                break;

            /* Objects sort by decreasing type */
            if (o_ptr->tval > j_ptr->tval)
                break;
            if (o_ptr->tval < j_ptr->tval)
                continue;

            /* Non-aware (flavored) items always come last */
            if (!object_aware_p(o_ptr))
                continue;
            if (!object_aware_p(j_ptr))
                break;

            /* Objects sort by increasing sval */
            if (o_ptr->sval < j_ptr->sval)
                break;
            if (o_ptr->sval > j_ptr->sval)
                continue;

            // This next bit is complicated: identified art > pseudo art >
            // identified special > pseudo special > other

            /* Identified artefacts beat the rest */
            if (!(object_known_p(o_ptr) && artefact_p(o_ptr))
                && (object_known_p(j_ptr) && artefact_p(j_ptr)))
                continue;
            if ((object_known_p(o_ptr) && artefact_p(o_ptr))
                && !(object_known_p(j_ptr) && artefact_p(j_ptr)))
                break;

            /* Then pseudo-identified {artefact} */
            if (!(!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr))
                && (!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr)))
                continue;
            if ((!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr))
                && !(!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr)))
                break;

            /* Then identified specials */
            if (!(object_known_p(o_ptr) && ego_item_p(o_ptr))
                && (object_known_p(j_ptr) && ego_item_p(j_ptr)))
                continue;
            if ((object_known_p(o_ptr) && ego_item_p(o_ptr))
                && !(object_known_p(j_ptr) && ego_item_p(j_ptr)))
                break;

            /* Then pseudo-identified {special} */
            if (!(!object_known_p(o_ptr) && special_pseudo_p(o_ptr))
                && (!object_known_p(j_ptr) && special_pseudo_p(j_ptr)))
                continue;
            if ((!object_known_p(o_ptr) && special_pseudo_p(o_ptr))
                && !(!object_known_p(j_ptr) && special_pseudo_p(j_ptr)))
                break;

            /* Lites sort by decreasing fuel */
            if (o_ptr->tval == TV_LIGHT)
            {
                if (o_ptr->timeout > j_ptr->timeout)
                    break;
                if (o_ptr->timeout < j_ptr->timeout)
                    continue;
            }

            /* Determine the "value" of the pack item */
            j_value = object_value(j_ptr);

            /* Objects sort by decreasing value */
            if (o_value > j_value)
                break;
            if (o_value < j_value)
                continue;

            /* Objects sort by increasing weight */
            if (o_ptr->weight < j_ptr->weight)
                break;
            if (o_ptr->weight > j_ptr->weight)
                continue;
        }

        /* Never move down */
        if (j >= i)
            continue;

        /* Take note */
        flag = true;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Save a copy of the moving item */
        object_copy(i_ptr, &inventory[i]);

        /* Slide the objects */
        for (k = i; k > j; k--)
        {
            /* Slide the item */
            object_copy(&inventory[k], &inventory[k - 1]);
        }

        /* Insert the moving item */
        object_copy(&inventory[j], i_ptr);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN);

        handle_stuff();
    }

    /* Message */
    if (flag && display_message)
        msg_print("You reorder some items in your pack.");
}

/*
 * Check ground artifacts within 33-cell radius of player and mark as seen
 * Only checks changed positions (tracked via lastpx/lastpy) for efficiency
 */
void check_artifact_visibility(void)
{
    int x, y;
    int px = p_ptr->px;
    int py = p_ptr->py;
    static int last_px = -1;
    static int last_py = -1;
    
    /* First call - mark everything in radius */
    if (last_px < 0 || last_py < 0)
    {
        for (y = py - 33; y <= py + 33; y++)
        {
            for (x = px - 33; x <= px + 33; x++)
            {
                if (!in_bounds(y, x)) continue;
                
                /* Check objects at this location */
                s16b this_o_idx = cave_o_idx[y][x];
                while (this_o_idx)
                {
                    object_type* o_ptr = &o_list[this_o_idx];
                    
                    /* If artifact and not already seen */
                    if (o_ptr->name1 && !(o_ptr->ident & IDENT_ARTIFACT_SEEN))
                    {
                        /* Mark as seen */
                        o_ptr->ident |= IDENT_ARTIFACT_SEEN;
                        log_trace("Artifact %d marked as seen at (%d,%d)", o_ptr->name1, y, x);
                    }
                    
                    this_o_idx = o_ptr->next_o_idx;
                }
            }
        }
        last_px = px;
        last_py = py;
        return;
    }
    
    /* Player moved - check only new cells that entered the radius */
    int dx = px - last_px;
    int dy = py - last_py;
    
    if (dx == 0 && dy == 0) return; /* No movement */
    
    /* Check cells that entered the 66x66 radius */
    for (y = py - 33; y <= py + 33; y++)
    {
        for (x = px - 33; x <= px + 33; x++)
        {
            if (!in_bounds(y, x)) continue;
            
            /* Only check if this cell wasn't in the old radius */
            int old_dx = x - last_px;
            int old_dy = y - last_py;
            if (old_dx >= -33 && old_dx <= 33 && old_dy >= -33 && old_dy <= 33)
                continue; /* Was already checked */
            
            /* Check objects at this new location */
            s16b this_o_idx = cave_o_idx[y][x];
            while (this_o_idx)
            {
                object_type* o_ptr = &o_list[this_o_idx];
                
                /* If artifact and not already seen */
                if (o_ptr->name1 && !(o_ptr->ident & IDENT_ARTIFACT_SEEN))
                {
                    /* Mark as seen */
                    o_ptr->ident |= IDENT_ARTIFACT_SEEN;
                    log_trace("Artifact %d marked as seen at (%d,%d)", o_ptr->name1, y, x);
                }
                
                this_o_idx = o_ptr->next_o_idx;
            }
        }
    }
    
    last_px = px;
    last_py = py;
}
