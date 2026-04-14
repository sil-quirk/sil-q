#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "supplies.h"

#include <string.h>

typedef struct supply_entry
{
    object_type obj;
} supply_entry;

static supply_entry* g_supply_entries = NULL;
static int g_supply_count = 0;
static int g_supply_capacity = 0;
static bool g_supply_initialized = false;

static int g_active_supply_action = -1;

static supply_menu_action g_pending_action = SUPPLY_MENU_ACTION_NONE;
static int g_pending_group = SUPPLY_GROUP_MAX;
static bool g_pending_hotkey = false;


static int g_supplies_max_weight = SUPPLIES_MAX_WEIGHT_DEFAULT;

static bool g_supply_allow_overflow = false;
static bool g_supply_limit_warned = false;
static int g_reserved_torch_lights = 0;
static int g_reserved_lantern_lights = 0;
static int g_reserved_lesser_jewels = 0;
static int g_reserved_feanorian_lamps = 0;

static bool player_light_uses_permanent_cap_sval(int sval)
{
    return (sval == SV_LIGHT_LESSER_JEWEL)
        || (sval == SV_LIGHT_FEANORIAN);
}

static bool ordinary_lesser_jewel_is_supply_light(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_LIGHT
        && o_ptr->sval == SV_LIGHT_LESSER_JEWEL
        && !object_has_ego_idx(o_ptr, EGO_GRACE);
}

static bool supplies_weight_counts_to_limit(const object_type* o_ptr)
{
    return o_ptr && !supplies_is_light_object(o_ptr);
}

static bool supplies_is_edible_object(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && (o_ptr->tval == TV_FOOD);
}

bool supplies_is_herb_object(const object_type* o_ptr)
{
    return supplies_is_edible_object(o_ptr) && (o_ptr->sval < SV_FOOD_MIN_FOOD);
}

bool supplies_is_food_object(const object_type* o_ptr)
{
    return supplies_is_edible_object(o_ptr) && (o_ptr->sval >= SV_FOOD_MIN_FOOD);
}

bool supplies_is_light_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_LIGHT)
        return false;

    return (o_ptr->sval == SV_LIGHT_TORCH)
        || (o_ptr->sval == SV_LIGHT_MALLORN)
        || (o_ptr->sval == SV_LIGHT_LANTERN)
        || ordinary_lesser_jewel_is_supply_light(o_ptr);
}

bool supplies_is_carried_object_pointer(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    if (o_ptr >= inventory && o_ptr < inventory + INVEN_TOTAL)
        return true;

    if (!g_supply_entries || g_supply_count <= 0)
        return false;

    const object_type* first = &g_supply_entries[0].obj;
    const object_type* last = &g_supply_entries[g_supply_count - 1].obj;

    return (o_ptr >= first) && (o_ptr <= last);
}

static bool player_lamp_pointer_uses_pool(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx
        && o_ptr->tval == TV_LIGHT
        && o_ptr->sval == SV_LIGHT_LANTERN
        && supplies_is_carried_object_pointer(o_ptr);
}

static int player_lamp_oil_capacity_from_count(int lantern_count)
{
    if (lantern_count < 0)
        lantern_count = 0;
    if (lantern_count > PLAYER_BRASS_LAMP_CAP)
        lantern_count = PLAYER_BRASS_LAMP_CAP;

    return lantern_count * FUEL_LAMP;
}

int player_light_max_fuel(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;

    if (player_lamp_pointer_uses_pool(o_ptr))
        return player_lamp_oil_capacity();

    switch (o_ptr->tval)
    {
    case TV_FLASK:
        return o_ptr->pval;
    case TV_LIGHT:
        if (o_ptr->sval == SV_LIGHT_TORCH)
            return FUEL_TORCH;
        if (o_ptr->sval == SV_LIGHT_LANTERN)
            return FUEL_LAMP;
        if (o_ptr->sval == SV_LIGHT_MALLORN)
            return FUEL_MALLORN;
        break;
    default:
        break;
    }

    return 0;
}

int player_light_sputter_threshold(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;

    if (o_ptr->tval != TV_LIGHT)
        return 0;

    switch (o_ptr->sval)
    {
    case SV_LIGHT_TORCH:
    case SV_LIGHT_LANTERN:
        return 100;
    case SV_LIGHT_MALLORN:
        return 10;
    default:
        return 0;
    }
}

bool player_light_uses_oil_pool(const object_type* o_ptr)
{
    return player_lamp_pointer_uses_pool(o_ptr);
}

int player_lamp_oil_capacity(void)
{
    return player_lamp_oil_capacity_with_bonus(0);
}

int player_lamp_oil_capacity_with_bonus(int lantern_bonus)
{
    return player_lamp_oil_capacity_from_count(
        player_carried_light_count_for_sval(SV_LIGHT_LANTERN) + lantern_bonus);
}

int player_lamp_oil(void)
{
    int capacity;

    if (!p_ptr)
        return 0;

    capacity = player_lamp_oil_capacity();

    if (p_ptr->lamp_oil < 0)
        p_ptr->lamp_oil = 0;
    if (p_ptr->lamp_oil > capacity)
        p_ptr->lamp_oil = capacity;

    return p_ptr->lamp_oil;
}

int player_lamp_oil_weight(void)
{
    int oil = player_lamp_oil();

    if (oil <= 0)
        return 0;

    return (oil + 1499) / 1500;
}

void player_set_lamp_oil(int oil)
{
    int capacity;

    if (!p_ptr)
        return;

    capacity = player_lamp_oil_capacity();

    if (oil < 0)
        oil = 0;
    if (oil > capacity)
        oil = capacity;

    p_ptr->lamp_oil = oil;
}

bool player_lamp_oil_would_overflow(int addition)
{
    return player_lamp_oil_would_overflow_with_bonus(addition, 0);
}

bool player_lamp_oil_would_overflow_with_bonus(int addition, int lantern_bonus)
{
    if (addition <= 0)
        return false;

    return player_lamp_oil() + addition
        > player_lamp_oil_capacity_with_bonus(lantern_bonus);
}

bool player_gain_lamp_oil(int addition, bool allow_overflow)
{
    return player_gain_lamp_oil_with_bonus(addition, allow_overflow, 0);
}

bool player_gain_lamp_oil_with_bonus(int addition, bool allow_overflow,
    int lantern_bonus)
{
    int oil;
    int capacity;

    if (!p_ptr)
        return false;

    if (addition <= 0)
        return true;

    oil = player_lamp_oil();
    capacity = player_lamp_oil_capacity_with_bonus(lantern_bonus);

    if (!allow_overflow && oil + addition > capacity)
        return false;

    if (oil + addition > capacity)
        oil = capacity;
    else
        oil += addition;

    if (oil < 0)
        oil = 0;
    if (oil > capacity)
        oil = capacity;
    p_ptr->lamp_oil = oil;

    return true;
}

bool player_prepare_lantern_drop(int lanterns_being_dropped,
    int* oil_to_transfer, int* oil_to_lose)
{
    int pooled_oil;
    int transfer;
    int remaining;

    if (oil_to_transfer)
        *oil_to_transfer = 0;
    if (oil_to_lose)
        *oil_to_lose = 0;

    if (lanterns_being_dropped <= 0)
        return true;

    pooled_oil = player_lamp_oil();
    if (pooled_oil <= 0)
        return true;

    transfer = MIN(pooled_oil, lanterns_being_dropped * FUEL_LAMP);
    remaining = pooled_oil - transfer;
    if (remaining < 0)
        remaining = 0;

    player_set_lamp_oil(remaining);

    if (oil_to_transfer)
        *oil_to_transfer = transfer;
    if (oil_to_lose)
        *oil_to_lose = 0;

    return true;
}

int player_light_fuel(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;

    if (player_lamp_pointer_uses_pool(o_ptr))
        return player_lamp_oil();

    return o_ptr->timeout;
}

bool player_light_has_fuel(const object_type* o_ptr)
{
    return player_light_fuel(o_ptr) > 0;
}

bool player_light_destroyed_on_drop(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_LIGHT)
        return false;

    if (o_ptr->sval != SV_LIGHT_TORCH && o_ptr->sval != SV_LIGHT_MALLORN)
        return false;

    return player_light_fuel(o_ptr) <= player_light_sputter_threshold(o_ptr);
}

void player_light_set_fuel(object_type* o_ptr, int fuel)
{
    int max_fuel;

    if (!o_ptr)
        return;

    max_fuel = player_light_max_fuel(o_ptr);
    if (fuel < 0)
        fuel = 0;
    if (max_fuel > 0 && fuel > max_fuel)
        fuel = max_fuel;

    if (player_lamp_pointer_uses_pool(o_ptr))
    {
        player_set_lamp_oil(fuel);
        o_ptr->timeout = 0;
        return;
    }

    o_ptr->timeout = fuel;
}

void player_light_add_fuel(object_type* o_ptr, int amount)
{
    if (!o_ptr || amount == 0)
        return;

    player_light_set_fuel(o_ptr, player_light_fuel(o_ptr) + amount);
}

static bool player_light_matches_sval(const object_type* o_ptr, int sval)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_LIGHT && o_ptr->sval == sval;
}

int player_carried_torch_count(void)
{
    int count = 0;

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx || o_ptr->tval != TV_LIGHT)
            continue;
        if (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN)
            count += MAX(o_ptr->number, 1);
    }

    for (int i = 0; i < g_supply_count; i++)
    {
        object_type* o_ptr = &g_supply_entries[i].obj;
        if (!o_ptr->k_idx || o_ptr->tval != TV_LIGHT)
            continue;
        if (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN)
            count += MAX(o_ptr->number, 1);
    }

    return count;
}

int player_carried_light_count_for_sval(int sval)
{
    int count = 0;

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        if (player_light_matches_sval(&inventory[i], sval))
            count += MAX(inventory[i].number, 1);
    }

    for (int i = 0; i < g_supply_count; i++)
    {
        object_type* o_ptr = &g_supply_entries[i].obj;
        if (player_light_matches_sval(o_ptr, sval))
            count += MAX(o_ptr->number, 1);
    }

    return count;
}

static int player_carried_permanent_light_count(void)
{
    return player_carried_light_count_for_sval(SV_LIGHT_LESSER_JEWEL)
        + player_carried_light_count_for_sval(SV_LIGHT_FEANORIAN);
}

int player_light_carry_cap(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_LIGHT)
        return 0;

    switch (o_ptr->sval)
    {
    case SV_LIGHT_TORCH:
    case SV_LIGHT_MALLORN:
        return PLAYER_TORCH_CAP;
    case SV_LIGHT_LANTERN:
        return PLAYER_BRASS_LAMP_CAP;
    case SV_LIGHT_LESSER_JEWEL:
    case SV_LIGHT_FEANORIAN:
        return PLAYER_PERMANENT_LIGHT_CAP;
    default:
        return 0;
    }
}

int player_light_available_capacity(const object_type* o_ptr)
{
    int cap;
    int used;

    cap = player_light_carry_cap(o_ptr);
    if (cap <= 0)
        return 255;

    if (o_ptr->tval != TV_LIGHT)
        return 255;

    if (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN)
        used = player_carried_torch_count();
    else if (player_light_uses_permanent_cap_sval(o_ptr->sval))
        used = player_carried_permanent_light_count();
    else
        used = player_carried_light_count_for_sval(o_ptr->sval);

    if (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN)
        used += g_reserved_torch_lights;
    else if (o_ptr->sval == SV_LIGHT_LANTERN)
        used += g_reserved_lantern_lights;
    else if (player_light_uses_permanent_cap_sval(o_ptr->sval))
        used += g_reserved_lesser_jewels + g_reserved_feanorian_lamps;

    if (used >= cap)
        return 0;

    return cap - used;
}

void player_light_clear_incoming_reservation(void)
{
    g_reserved_torch_lights = 0;
    g_reserved_lantern_lights = 0;
    g_reserved_lesser_jewels = 0;
    g_reserved_feanorian_lamps = 0;
}

void player_light_reserve_incoming(const object_type* o_ptr, int amount)
{
    player_light_clear_incoming_reservation();

    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_LIGHT || amount <= 0)
        return;

    switch (o_ptr->sval)
    {
    case SV_LIGHT_TORCH:
    case SV_LIGHT_MALLORN:
        g_reserved_torch_lights = amount;
        break;
    case SV_LIGHT_LANTERN:
        g_reserved_lantern_lights = amount;
        break;
    case SV_LIGHT_LESSER_JEWEL:
        g_reserved_lesser_jewels = amount;
        break;
    case SV_LIGHT_FEANORIAN:
        g_reserved_feanorian_lamps = amount;
        break;
    default:
        break;
    }
}


static void supplies_apply_auto_identification(object_type* obj)
{
    if (!obj || !obj->k_idx)
        return;

    if (player_auto_identifies_object(obj))
    {
        ident(obj);
        apply_autoinscription(obj);
    }
}

static void supplies_reserve(int minimum)
{
    if (g_supply_capacity >= minimum)
        return;

    int new_capacity = (g_supply_capacity > 0) ? g_supply_capacity : 8;
    while (new_capacity < minimum)
        new_capacity *= 2;

    supply_entry* new_entries = mem_alloc_array(new_capacity, supply_entry);
    if (g_supply_entries && g_supply_count > 0)
    {
        memcpy(new_entries, g_supply_entries, g_supply_count * sizeof(supply_entry));
        mem_free_null(g_supply_entries);
    }

    for (int i = g_supply_count; i < new_capacity; i++)
    {
        object_wipe(&new_entries[i].obj);
    }

    g_supply_entries = new_entries;
    g_supply_capacity = new_capacity;
}

static bool supplies_can_add_weight_internal(int current_weight, int addition,
    bool warn)
{
    if (addition <= 0)
        return true;

    if (g_supply_allow_overflow)
        return true;

    if (current_weight + addition <= g_supplies_max_weight)
        return true;

    if (warn && !g_supply_limit_warned)
    {
        msg_print("Your supply cache cannot carry any more weight.");
        g_supply_limit_warned = true;
    }

    return false;
}

void supplies_set_max_weight_cap(int weight_tenths)
{
    if (weight_tenths <= 0)
        weight_tenths = SUPPLIES_MAX_WEIGHT_DEFAULT;

    if (weight_tenths != g_supplies_max_weight) {
        g_supplies_max_weight = weight_tenths;
        g_supply_limit_warned = false;
        log_info("Supplies weight limit set to %d tenths of a pound", weight_tenths);
    }
}

int supplies_current_weight_cap(void)
{
    return g_supplies_max_weight;
}


void supplies_set_allow_overflow(bool allow)
{
    g_supply_allow_overflow = allow;
    if (allow)
        g_supply_limit_warned = false;
}

void supplies_init(void)
{
    if (g_supply_initialized)
        return;

    supplies_reserve(8);
    g_supply_count = 0;
    g_supply_initialized = true;
}

void supplies_dispose(void)
{
    if (!g_supply_initialized)
        return;

    if (g_supply_entries)
        mem_free_null(g_supply_entries);

    g_supply_entries = NULL;
    g_supply_capacity = 0;
    g_supply_count = 0;
    g_supply_initialized = false;
    g_active_supply_action = -1;
    g_pending_action = SUPPLY_MENU_ACTION_NONE;
    g_pending_group = SUPPLY_GROUP_MAX;
    g_pending_hotkey = false;
    g_supply_limit_warned = false;
    player_light_clear_incoming_reservation();
}

void supplies_reset_store(void)
{
    supplies_init();
    for (int i = 0; i < g_supply_capacity; i++)
    {
        object_wipe(&g_supply_entries[i].obj);
    }
    g_supply_count = 0;
    g_active_supply_action = -1;
    g_pending_action = SUPPLY_MENU_ACTION_NONE;
    g_pending_group = SUPPLY_GROUP_MAX;
    g_pending_hotkey = false;
    g_supply_limit_warned = false;
    player_light_clear_incoming_reservation();
}

bool supplies_is_supply_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->tval == TV_POTION)
        return true;

    if (o_ptr->tval == TV_GEM)
        return true;

    if (supplies_is_edible_object(o_ptr))
        return true;

    if (supplies_is_light_object(o_ptr))
        return true;

    return false;
}

static int supplies_find_similar(const object_type* src)
{
    for (int i = 0; i < g_supply_count; i++)
    {
        object_type* entry = &g_supply_entries[i].obj;
        if (!entry->k_idx)
            continue;
        if (object_similar(entry, src))
            return i;
    }
    return -1;
}

bool supplies_can_absorb_object(const object_type* o_ptr)
{
    int add_weight;

    if (!supplies_is_supply_object(o_ptr))
        return false;

    if (g_supply_allow_overflow)
        return player_light_available_capacity(o_ptr) >= o_ptr->number;

    if (player_light_available_capacity(o_ptr) < o_ptr->number)
        return false;

    add_weight = supplies_weight_counts_to_limit(o_ptr)
        ? (o_ptr->weight * o_ptr->number)
        : 0;

    int current_weight = supplies_limit_weight();
    return supplies_can_add_weight_internal(current_weight, add_weight, false);
}

int supplies_max_absorbable_quantity(const object_type* o_ptr)
{
    int max_count = o_ptr ? o_ptr->number : 0;

    if (!supplies_is_supply_object(o_ptr))
        return 0;

    if (g_supply_allow_overflow)
        return MIN(max_count, player_light_available_capacity(o_ptr));

    max_count = MIN(max_count, player_light_available_capacity(o_ptr));

    if (!supplies_weight_counts_to_limit(o_ptr) || o_ptr->weight <= 0)
        return max_count;

    int current_weight = supplies_limit_weight();
    int available_weight = g_supplies_max_weight - current_weight;

    if (available_weight <= 0)
        return 0;

    return MIN(max_count, available_weight / o_ptr->weight);
}

static void supplies_mark_dirty(void)
{
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    p_ptr->update |= (PU_BONUS);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
}

bool supplies_absorb_object(object_type* src)
{
    object_type normalized;
    if (!supplies_is_supply_object(src))
        return false;

    supplies_init();

    if (src->number <= 0)
    {
        object_wipe(src);
        return true;
    }
    if (player_light_available_capacity(src) < src->number)
        return false;

    object_copy(&normalized, src);
    if (normalized.tval == TV_LIGHT && normalized.sval == SV_LIGHT_LANTERN)
        normalized.timeout = 0;

    int idx = supplies_find_similar(&normalized);

    int current_weight = supplies_limit_weight();
    int add_weight = supplies_weight_counts_to_limit(src)
        ? (src->weight * src->number)
        : 0;
    if (!supplies_can_add_weight_internal(current_weight, add_weight, true))
        return false;

    if (src->tval == TV_LIGHT && src->sval == SV_LIGHT_LANTERN)
        normalized.timeout = 0;

    if (idx >= 0)
    {
        supply_entry* entry = &g_supply_entries[idx];
        int total = entry->obj.number + normalized.number;
        int moved = MIN(total, 255);
        entry->obj.number = moved;
        int leftover = total - moved;
        if (entry->obj.tval == TV_LIGHT && entry->obj.sval == SV_LIGHT_LANTERN)
            entry->obj.timeout = 0;
        supplies_apply_auto_identification(&entry->obj);
        if (src->tval == TV_LIGHT && src->sval == SV_LIGHT_LANTERN
            && !player_gain_lamp_oil(src->timeout * src->number,
                g_supply_allow_overflow))
        {
            log_warn("supplies_absorb_object: failed to pool lantern oil");
        }
        object_wipe(src);
        supplies_mark_dirty();
        while (leftover > 0)
        {
            supplies_reserve(g_supply_count + 1);
            entry = &g_supply_entries[idx];
            supply_entry* extra = &g_supply_entries[g_supply_count];
            object_copy(&extra->obj, &entry->obj);
            extra->obj.number = MIN(leftover, 255);
            if (extra->obj.tval == TV_LIGHT && extra->obj.sval == SV_LIGHT_LANTERN)
                extra->obj.timeout = 0;
            supplies_apply_auto_identification(&extra->obj);
            leftover -= extra->obj.number;
            g_supply_count++;
        }
        return true;
    }

    supplies_reserve(g_supply_count + 1);
    supply_entry* entry = &g_supply_entries[g_supply_count];
    object_copy(&entry->obj, &normalized);
    supplies_apply_auto_identification(&entry->obj);
    g_supply_count++;
    if (src->tval == TV_LIGHT && src->sval == SV_LIGHT_LANTERN
        && !player_gain_lamp_oil(src->timeout * src->number,
            g_supply_allow_overflow))
    {
        log_warn("supplies_absorb_object: failed to pool lantern oil");
    }
    object_wipe(src);
    supplies_mark_dirty();
    return true;
}


int supplies_entry_count(void)
{
    return g_supply_count;
}

object_type* supplies_entry_at(int idx)
{
    if (idx < 0 || idx >= g_supply_count)
        return NULL;
    return &g_supply_entries[idx].obj;
}

int supplies_entry_units(int idx)
{
    if (idx < 0 || idx >= g_supply_count)
        return 0;

    return g_supply_entries[idx].obj.number;
}

int supplies_first_entry_for_kind(int k_idx)
{
    for (int i = 0; i < g_supply_count; i++)
    {
        object_type* obj = &g_supply_entries[i].obj;
        if (!obj->k_idx)
            continue;
        if (obj->k_idx == k_idx)
            return i;
    }

    return -1;
}

bool supplies_take_one(int idx, object_type* out)
{
    object_type* obj;

    if (!out || idx < 0 || idx >= g_supply_count)
        return false;

    obj = &g_supply_entries[idx].obj;
    if (!obj->k_idx || obj->number <= 0)
        return false;

    object_copy(out, obj);
    out->number = 1;

    if (obj->number <= 1)
    {
        object_wipe(obj);
        for (int move = idx + 1; move < g_supply_count; move++)
            g_supply_entries[move - 1] = g_supply_entries[move];
        g_supply_count--;
    }
    else
    {
        obj->number--;
    }

    supplies_mark_dirty();
    g_supply_limit_warned = false;
    return true;
}


int supplies_total_weight(void)
{
    int total = 0;
    for (int i = 0; i < g_supply_count; i++)
    {
        supply_entry* entry = &g_supply_entries[i];
        object_type* obj = &entry->obj;
        if (!obj->k_idx)
            continue;
        total += obj->weight * obj->number;
    }
    return total;
}

int supplies_carried_light_item_weight(void)
{
    int total = 0;

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!supplies_is_light_object(o_ptr))
            continue;

        total += o_ptr->weight * MAX(o_ptr->number, 1);
    }

    for (int i = 0; i < g_supply_count; i++)
    {
        supply_entry* entry = &g_supply_entries[i];
        object_type* obj = &entry->obj;
        if (!supplies_is_light_object(obj))
            continue;

        total += obj->weight * obj->number;
    }

    return total;
}

int supplies_limit_weight(void)
{
    int total = 0;

    for (int i = 0; i < g_supply_count; i++)
    {
        supply_entry* entry = &g_supply_entries[i];
        object_type* obj = &entry->obj;
        if (!obj->k_idx || !supplies_weight_counts_to_limit(obj))
            continue;
        total += obj->weight * obj->number;
    }

    return total;
}

void supplies_count_totals(int* herbs, int* food, int* potions, int* gems,
    int* lights)
{
    object_type* light_ptr;

    if (herbs)
        *herbs = 0;
    if (food)
        *food = 0;
    if (potions)
        *potions = 0;
    if (gems)
        *gems = 0;
    if (lights)
        *lights = 0;

    for (int i = 0; i < g_supply_count; i++)
    {
        supply_entry* entry = &g_supply_entries[i];
        object_type* obj = &entry->obj;
        if (!obj->k_idx)
            continue;

        if (supplies_is_herb_object(obj))
        {
            if (herbs)
                *herbs += obj->number;
        }
        else if (supplies_is_food_object(obj))
        {
            if (food)
                *food += obj->number;
        }
        else if (obj->tval == TV_POTION)
        {
            if (potions)
                *potions += obj->number;
        }
        else if (obj->tval == TV_GEM)
        {
            if (gems)
                *gems += obj->number;
        }
        else if (supplies_is_light_object(obj))
        {
            if (lights)
                *lights += obj->number;
        }
    }

    light_ptr = &inventory[INVEN_LITE];
    if (lights && supplies_is_light_object(light_ptr))
        *lights += MAX(light_ptr->number, 1);
}
bool supplies_has_group(int group)
{
    return supplies_first_entry_for_group(group) >= 0;
}

int supplies_first_entry_for_group(int group)
{
    for (int i = 0; i < g_supply_count; i++)
    {
        object_type* entry = &g_supply_entries[i].obj;
        if (!entry->k_idx)
            continue;

        switch (group)
        {
        case SUPPLY_GROUP_HERBS:
            if (supplies_is_herb_object(entry))
                return i;
            break;
        case SUPPLY_GROUP_FOOD:
            if (supplies_is_food_object(entry))
                return i;
            break;
        case SUPPLY_GROUP_POTIONS:
            if (entry->tval == TV_POTION)
                return i;
            break;
        case SUPPLY_GROUP_GEMS:
            if (entry->tval == TV_GEM)
                return i;
            break;
        case SUPPLY_GROUP_LIGHTS:
            if (supplies_is_light_object(entry))
                return i;
            break;
        default:
            break;
        }
    }
    return -1;
}

char supplies_label_char(void)
{
    if (g_supply_count <= 0)
        return 0;
    return 'a';
}

int supplies_virtual_slot(void)
{
    if (g_supply_count == 0)
        return -1;
    /* Supplies occupy a virtual slot before the pack. */
    return 0;
}

bool supplies_consume_quantity(int idx, int amount)
{
    if (idx < 0 || idx >= g_supply_count)
        return false;

    supply_entry* entry = &g_supply_entries[idx];
    object_type* obj = &entry->obj;

    if (!obj->k_idx || amount <= 0)
        return false;

    if (amount >= obj->number)
    {
        object_wipe(obj);
        for (int move = idx + 1; move < g_supply_count; move++)
            g_supply_entries[move - 1] = g_supply_entries[move];
        g_supply_count--;
        supplies_mark_dirty();
        g_supply_limit_warned = false;
        return true;
    }

    obj->number -= amount;
    g_supply_limit_warned = false;
    supplies_mark_dirty();
    return false;
}


void supplies_refresh_entry(int idx)
{
    if (idx < 0 || idx >= g_supply_count)
        return;

    supply_entry* entry = &g_supply_entries[idx];
    object_type* obj = &entry->obj;
    if (!obj->k_idx)
        return;

    if (obj->number <= 0)
    {
        object_wipe(obj);
        for (int move = idx + 1; move < g_supply_count; move++)
            g_supply_entries[move - 1] = g_supply_entries[move];
        g_supply_count--;
        supplies_mark_dirty();
        return;
    }

    supplies_apply_auto_identification(obj);
    if (obj->tval == TV_LIGHT && obj->sval == SV_LIGHT_LANTERN)
        obj->timeout = 0;
    g_supply_limit_warned = false;
    supplies_mark_dirty();
}

bool supplies_drop_amount(int idx, int amount)
{
    int lantern_oil_to_drop = 0;
    if (idx < 0 || idx >= g_supply_count)
        return false;

    supply_entry* entry = &g_supply_entries[idx];
    object_type* obj = &entry->obj;
    if (!obj->k_idx)
        return false;

    if (amount <= 0)
        return false;

    if (amount > obj->number)
        amount = obj->number;

    object_type drop;
    char o_name[120];
    object_wipe(&drop);
    object_copy(&drop, obj);
    drop.number = amount;

    if (drop.tval == TV_LIGHT && drop.sval == SV_LIGHT_LANTERN)
    {
        if (!player_prepare_lantern_drop(amount, &lantern_oil_to_drop, NULL))
            return false;
    }

    object_desc(o_name, sizeof(o_name), &drop, true, 3);

    if (player_light_destroyed_on_drop(&drop))
    {
        msg_format("You discard %s; %s too spent to keep.",
            o_name, (drop.number > 1) ? "they are" : "it is");
        supplies_consume_quantity(idx, amount);
        p_ptr->redraw |= (PR_MAP | PR_LIGHT);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        return true;
    }

    if (drop.tval == TV_LIGHT && drop.sval == SV_LIGHT_LANTERN
        && lantern_oil_to_drop > 0)
    {
        int oil_remaining = lantern_oil_to_drop;
        for (int n = 0; n < amount; n++)
        {
            object_type single_drop;
            object_wipe(&single_drop);
            object_copy(&single_drop, &drop);
            single_drop.number = 1;
            single_drop.timeout = MIN(oil_remaining, FUEL_LAMP);
            oil_remaining -= single_drop.timeout;
            drop_near(&single_drop, 0, p_ptr->py, p_ptr->px);
        }
    }
    else
    {
        drop_near(&drop, 0, p_ptr->py, p_ptr->px);
    }
    supplies_consume_quantity(idx, amount);
    return true;
}


void supplies_ingest_pack(void)
{
    supplies_init();

    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx)
            continue;
        if (!supplies_is_supply_object(o_ptr))
            continue;

        object_type copy;
        object_copy(&copy, o_ptr);
        if (!supplies_absorb_object(&copy))
            continue;
        inven_item_increase(i, -o_ptr->number);
        inven_item_optimize(i);
        i--;
    }
}

void supplies_begin_action(int supply_idx)
{
    g_active_supply_action = supply_idx;
}

void supplies_end_action(void)
{
    g_active_supply_action = -1;
}

int supplies_current_action(void)
{
    return g_active_supply_action;
}

bool supplies_any_match_item_tester(void)
{
    if (g_supply_count == 0)
        return false;

    for (int i = 0; i < g_supply_count; i++)
    {
        object_type* entry = &g_supply_entries[i].obj;
        if (!entry->k_idx)
            continue;

        if (item_tester_tval)
        {
            if (entry->tval != item_tester_tval)
                continue;
        }

        if (item_tester_hook)
        {
            if (!(*item_tester_hook)(entry))
                continue;
        }

        return true;
    }

    return false;
}

void supplies_set_pending_action(supply_menu_action action, int group, bool hotkey)
{
    g_pending_action = action;
    g_pending_group = group;
    g_pending_hotkey = hotkey;
}

void supplies_clear_pending_action(void)
{
    g_pending_action = SUPPLY_MENU_ACTION_NONE;
    g_pending_group = SUPPLY_GROUP_MAX;
    g_pending_hotkey = false;
}

bool supplies_has_pending_action(void)
{
    return g_pending_action != SUPPLY_MENU_ACTION_NONE;
}

supply_menu_action supplies_pending_action(void)
{
    return g_pending_action;
}

int supplies_pending_group(void)
{
    return g_pending_group;
}

bool supplies_pending_hotkey(void)
{
    return g_pending_hotkey;
}

/*
 * Damage supply items based on a type check function
 * Similar to inven_damage but for the supply system
 * Returns number of items destroyed
 */
int supplies_damage(int (*typ)(const object_type*), int perc, int resistance)
{
    int i, j, k, amt;
    object_type* o_ptr;
    char o_name[80];

    /* Count the casualties */
    k = 0;

    /* Scan through supply entries */
    for (i = 0; i < g_supply_count; i++)
    {
        supply_entry* entry = &g_supply_entries[i];
        o_ptr = &entry->obj;

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Hack -- for now, skip artefacts */
        if (artefact_p(o_ptr))
            continue;

        /* Give this item slot a shot at death */
        if ((*typ)(o_ptr))
        {
            int total_count = o_ptr->number;

            /* Count the casualties */
            for (amt = j = 0; j < total_count; ++j)
            {
                if (percent_chance(perc)
                    && ((resistance < 0) || one_in_(resistance)))
                    amt++;
            }

            /* Some casualties */
            if (amt)
            {
                /* Get a description */
                object_desc(o_name, sizeof(o_name), o_ptr, false, 3);

                /* Message */
                msg_format("%sour %s in supply %s destroyed!",
                    ((total_count > 1) ? ((amt == total_count)
                             ? "All of y"
                             : (amt > 1 ? "Some of y" : "One of y"))
                                         : "Y"),
                    o_name, ((amt > 1) ? "were" : "was"));

                /* Reduce the count */
                o_ptr->number -= amt;

                /* If all destroyed, wipe the entry */
                if (o_ptr->number <= 0)
                {
                    object_wipe(&entry->obj);

                    /* Compact the array by shifting remaining entries */
                    for (int shift = i; shift < g_supply_count - 1; shift++)
                    {
                        g_supply_entries[shift] = g_supply_entries[shift + 1];
                    }
                    g_supply_count--;
                    i--; /* Re-check this index since we shifted */
                }

                /* Count this destruction */
                k += amt;
            }
        }
    }

    if (k > 0)
        supplies_mark_dirty();

    /* Return the count */
    return (k);
}

int supplies_damage_cold(int perc, int resistance)
{
    int i, j, k, amt;
    object_type* o_ptr;
    char o_name[80];

    k = 0;

    for (i = 0; i < g_supply_count; i++)
    {
        supply_entry* entry = &g_supply_entries[i];
        o_ptr = &entry->obj;

        if (!o_ptr->k_idx || artefact_p(o_ptr))
            continue;

        if ((o_ptr->tval != TV_POTION) && (o_ptr->tval != TV_GEM)
            && (o_ptr->tval != TV_FLASK))
            continue;

        u32b f1, f2, f3;
        object_flags(o_ptr, &f1, &f2, &f3);
        (void)f1;
        (void)f2;
        if (f3 & TR3_IGNORE_COLD)
            continue;

        for (amt = j = 0; j < o_ptr->number; ++j)
        {
            if (percent_chance(perc)
                && ((resistance < 0) || one_in_(resistance)))
            {
                amt++;
            }
        }

        if (!amt)
            continue;

        object_desc(o_name, sizeof(o_name), o_ptr, false, 3);

        msg_format("%sour %s in supply %s destroyed!",
            ((o_ptr->number > 1) ? ((amt == o_ptr->number)
                     ? "All of y"
                     : (amt > 1 ? "Some of y" : "One of y"))
                                 : "Y"),
            o_name, ((amt > 1) ? "were" : "was"));

        o_ptr->number -= amt;

        if (o_ptr->number <= 0)
        {
            object_wipe(&entry->obj);

            for (int shift = i; shift < g_supply_count - 1; shift++)
            {
                g_supply_entries[shift] = g_supply_entries[shift + 1];
            }
            g_supply_count--;
            i--;
        }

        k += amt;
    }

    if (k > 0)
        supplies_mark_dirty();

    return k;
}


