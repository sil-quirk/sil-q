/* File: object/object-inventory-limits.c */

#include "angband.h"
#include "externs.h"
#include "object/object-inventory-limits.h"
#include "object/object-internal.h"
#include "log/log.h"
#include "supplies.h"


static bool carry_limit_last_failed = false;
static enum inventory_limit_group carry_limit_last_group = INV_LIMIT_NONE;
static int carry_limit_last_limit = 0;
static char carry_limit_last_label[64];
static enum inventory_limit_group pack_limit_prompt_group = INV_LIMIT_NONE;
void clear_inventory_limit_failure(void)
{
    carry_limit_last_failed = false;
    carry_limit_last_group = INV_LIMIT_NONE;
    carry_limit_last_limit = 0;
    carry_limit_last_label[0] = '\0';
}

bool inven_index_valid(int item, cptr context)
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

static bool object_is_inventory_throwable(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    if (o_ptr->tval == TV_SWORD && o_ptr->sval == SV_DAGGER)
        return true;

    if (o_ptr->tval == TV_POLEARM)
        return o_ptr->sval == SV_HAND_AXE || o_ptr->sval == SV_SPEAR;

    return false;
}

static int inventory_throwable_space_cost(const object_type* o_ptr)
{
    if (o_ptr && o_ptr->tval == TV_POLEARM && o_ptr->sval == SV_SPEAR)
        return 2;

    return 1;
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
                if (object_is_inventory_throwable(o_ptr))
                {
                    local_group = INV_LIMIT_THROWABLE;
                    local_limit = 2;
                    local_cost = inventory_throwable_space_cost(o_ptr);
                }
                else
                {
                    local_group = INV_LIMIT_MELEE_WEAPON;
                    local_limit = 2;
                    local_cost = object_is_truly_two_handed(o_ptr) ? 2 : 1;
                }
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

static bool inventory_limit_counts_stacks(enum inventory_limit_group group)
{
    if (group == INV_LIMIT_ARROW)
        return true;

    if (group == INV_LIMIT_THROWABLE)
        return true;

    return false;
}

bool inventory_limit_is_stack_counted(const object_type* o_ptr)
{
    enum inventory_limit_group group;

    return get_inventory_limit_info(o_ptr, &group, NULL, NULL)
        && inventory_limit_counts_stacks(group);
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

        if (inventory_limit_counts_stacks(slot_group))
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
        case INV_LIMIT_THROWABLE:
            SDL_strlcpy(carry_limit_last_label, "throwing weapons",
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
                "lesser jewels or Fëanorian lamps",
                sizeof(carry_limit_last_label));
            break;
        default:
            SDL_strlcpy(carry_limit_last_label, "items of this type",
                      sizeof(carry_limit_last_label));
            break;
    }
}

void set_inventory_limit_failure(enum inventory_limit_group group,
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

bool player_light_capacity_okay(const object_type* o_ptr,
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

void set_inventory_limit_failure(enum inventory_limit_group group,
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

bool inventory_type_slot_available(const object_type* o_ptr,
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

    units = inventory_limit_counts_stacks(group) ? 1
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

                if (!open_inventory_item_select_menu(USE_INVEN,
                        inventory_limit_group_drop_prompt(group),
                        "You have nothing suitable to drop.", &item))
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

enum inventory_limit_group inventory_limit_group_for_object(
    const object_type* o_ptr)
{
    enum inventory_limit_group group;

    if (!o_ptr || !o_ptr->k_idx)
        return INV_LIMIT_NONE;

    if (o_ptr->tval == TV_LIGHT || o_ptr->tval == TV_FLASK
        || player_oil_container_object(o_ptr))
    {
        group = light_limit_group(o_ptr);
        if (group != INV_LIMIT_NONE)
            return group;
    }

    if (get_inventory_limit_info(o_ptr, &group, NULL, NULL))
        return group;

    return INV_LIMIT_NONE;
}

bool inventory_limit_info_for_object(const object_type* o_ptr,
    enum inventory_limit_group* group, int* limit, int* cost)
{
    enum inventory_limit_group local_group;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    local_group = inventory_limit_group_for_object(o_ptr);
    if (local_group == INV_LIMIT_NONE)
        return false;

    if (group)
        *group = local_group;

    if (local_group == INV_LIMIT_TORCHES
        || local_group == INV_LIMIT_BRASS_LAMPS
        || local_group == INV_LIMIT_LESSER_JEWEL
        || local_group == INV_LIMIT_FEANORIAN_LAMP)
    {
        if (limit)
            *limit = player_light_carry_cap(o_ptr);
        if (cost)
            *cost = player_oil_container_object(o_ptr)
                ? player_oil_container_slot_cost(o_ptr)
                : 1;
        return true;
    }

    return get_inventory_limit_info(o_ptr, NULL, limit, cost);
}

int inventory_limit_usage_for_group(enum inventory_limit_group group)
{
    int usage = 0;

    if (group == INV_LIMIT_NONE)
        return 0;

    if (group >= INV_LIMIT_ARROW && group <= INV_LIMIT_THROWABLE)
        return inventory_limit_usage(group);

    if (group == INV_LIMIT_TORCHES)
        return player_carried_torch_count();

    if (group == INV_LIMIT_BRASS_LAMPS)
        return player_oil_container_slots_used();

    if (group == INV_LIMIT_LESSER_JEWEL
        || group == INV_LIMIT_FEANORIAN_LAMP)
    {
        return player_carried_light_count_for_sval(SV_LIGHT_LESSER_JEWEL)
            + player_carried_light_count_for_sval(SV_LIGHT_FEANORIAN);
    }

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        enum inventory_limit_group object_group;
        int cost;

        if (!o_ptr->k_idx)
            continue;

        object_group = inventory_limit_group_for_object(o_ptr);
        if (object_group != group)
            continue;

        if (!inventory_limit_info_for_object(o_ptr, NULL, NULL, &cost))
            cost = 1;

        usage += cost * MAX(o_ptr->number, 1);
    }

    return usage;
}

int inventory_limit_space_for_object(const object_type* o_ptr)
{
    enum inventory_limit_group group;
    int cost;
    int units;

    if (!inventory_limit_info_for_object(o_ptr, &group, NULL, &cost))
        return 0;

    if (cost <= 0)
        return 0;

    units = inventory_limit_counts_stacks(group) ? 1
                                                 : MAX(o_ptr->number, 1);

    return cost * units;
}

int inventory_limit_limit_for_group(enum inventory_limit_group group)
{
    int limit;

    switch (group)
    {
    case INV_LIMIT_ARROW:
        return 2;
    case INV_LIMIT_BOW:
        return 1;
    case INV_LIMIT_STAFF:
        return 1;
    case INV_LIMIT_HORN:
        return 2;
    case INV_LIMIT_DIGGING:
        return 1;
    case INV_LIMIT_BOOTS:
        return 2;
    case INV_LIMIT_GLOVES:
        return 2;
    case INV_LIMIT_HELM_CROWN:
        limit = 1;
        break;
    case INV_LIMIT_ROUND_SHIELD:
        limit = 1;
        break;
    case INV_LIMIT_OTHER_SHIELD:
        limit = 0;
        break;
    case INV_LIMIT_CLOAK:
        return 3;
    case INV_LIMIT_SOFT_ARMOUR:
        return 1;
    case INV_LIMIT_MAIL:
        limit = 0;
        break;
    case INV_LIMIT_MELEE_WEAPON:
        return 2;
    case INV_LIMIT_THROWABLE:
        return 2;
    case INV_LIMIT_SUPPLY_WEIGHT:
        return supplies_current_weight_cap() / 10;
    case INV_LIMIT_TORCHES:
        return PLAYER_TORCH_CAP;
    case INV_LIMIT_BRASS_LAMPS:
        return PLAYER_OIL_CONTAINER_SLOT_CAP;
    case INV_LIMIT_LESSER_JEWEL:
    case INV_LIMIT_FEANORIAN_LAMP:
        return PLAYER_PERMANENT_LIGHT_CAP;
    default:
        return -1;
    }

    if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR])
        limit += 1;

    return limit;
}

bool inventory_limit_object_matches_group(
    enum inventory_limit_group group, const object_type* o_ptr)
{
    return group != INV_LIMIT_NONE
        && inventory_limit_group_for_object(o_ptr) == group;
}

cptr inventory_limit_group_name(enum inventory_limit_group group)
{
    switch (group)
    {
    case INV_LIMIT_ARROW:
        return "arrow stacks";
    case INV_LIMIT_BOW:
        return "bows";
    case INV_LIMIT_STAFF:
        return "walking staves";
    case INV_LIMIT_HORN:
        return "horns";
    case INV_LIMIT_DIGGING:
        return "digging tools";
    case INV_LIMIT_BOOTS:
        return "boots";
    case INV_LIMIT_GLOVES:
        return "gloves";
    case INV_LIMIT_HELM_CROWN:
        return "helms/crowns";
    case INV_LIMIT_ROUND_SHIELD:
        return "round shields";
    case INV_LIMIT_OTHER_SHIELD:
        return "other shields";
    case INV_LIMIT_CLOAK:
        return "cloaks/robes";
    case INV_LIMIT_SOFT_ARMOUR:
        return "soft armour";
    case INV_LIMIT_MAIL:
        return "mail armour";
    case INV_LIMIT_MELEE_WEAPON:
        return "melee weapons";
    case INV_LIMIT_THROWABLE:
        return "throwables";
    case INV_LIMIT_SUPPLY_WEIGHT:
        return "supply weight";
    case INV_LIMIT_TORCHES:
        return "torches";
    case INV_LIMIT_BRASS_LAMPS:
        return "oil slots";
    case INV_LIMIT_LESSER_JEWEL:
    case INV_LIMIT_FEANORIAN_LAMP:
        return "permanent lights";
    default:
        return "";
    }
}
