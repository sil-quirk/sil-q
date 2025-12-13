#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "supplies.h"

#include <string.h>

typedef struct supply_entry
{
    object_type obj;
    int stored_count;
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

#define IS_GEM(o_ptr) ((o_ptr)->tval == TV_GEM)

static bool g_supply_allow_overflow = false;
static bool g_supply_limit_warned = false;


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

static void supplies_sync_gem_entry(supply_entry* entry)
{
    if (!entry)
        return;

    object_type* obj = &entry->obj;
    if (!obj->k_idx || obj->tval != TV_GEM)
        return;

    /* Gems use number, not charges */
    obj->number = entry->stored_count;  /* Reusing stored_count field for gem count */
    obj->pval = -entry->stored_count;

    if (entry->stored_count <= 0)
    {
        obj->ident |= IDENT_EMPTY;
        obj->number = 0;
        obj->pval = 0;
        return;
    }

    obj->ident &= ~(IDENT_EMPTY);
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
        new_entries[i].stored_count = 0;
    }

    g_supply_entries = new_entries;
    g_supply_capacity = new_capacity;
}

static bool supplies_can_add_weight(int current_weight, int addition)
{
    if (addition <= 0)
        return true;

    if (g_supply_allow_overflow)
        return true;

    if (current_weight + addition <= g_supplies_max_weight)
        return true;

    if (!g_supply_limit_warned)
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
}

void supplies_reset_store(void)
{
    supplies_init();
    for (int i = 0; i < g_supply_capacity; i++)
    {
        object_wipe(&g_supply_entries[i].obj);
        g_supply_entries[i].stored_count = 0;
    }
    g_supply_count = 0;
    g_active_supply_action = -1;
    g_pending_action = SUPPLY_MENU_ACTION_NONE;
    g_pending_group = SUPPLY_GROUP_MAX;
    g_pending_hotkey = false;
    g_supply_limit_warned = false;
}

bool supplies_is_supply_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->tval == TV_POTION)
        return true;

    if (IS_GEM(o_ptr))
        return true;

    if (o_ptr->tval == TV_FOOD && o_ptr->sval <= SV_FOOD_SICKNESS)
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
    if (!supplies_is_supply_object(o_ptr))
        return false;

    if (g_supply_allow_overflow)
        return true;

    int current_weight = supplies_total_weight();
    int idx = supplies_find_similar(o_ptr);

    if (o_ptr->tval == TV_GEM)
    {
        if (idx >= 0)
        {
            supply_entry* existing = &g_supply_entries[idx];
            int existing_weight = existing->obj.weight * existing->stored_count;
            current_weight -= existing_weight;
            int new_count = existing->stored_count + o_ptr->number;
            int new_weight = existing->obj.weight * new_count;
            return supplies_can_add_weight(current_weight, new_weight);
        }
        
        int add_weight = o_ptr->weight * o_ptr->number;
        return supplies_can_add_weight(current_weight, add_weight);
    }
    
    int add_weight = o_ptr->weight * o_ptr->number;
    return supplies_can_add_weight(current_weight, add_weight);
}

int supplies_max_absorbable_quantity(const object_type* o_ptr)
{
    if (!supplies_is_supply_object(o_ptr))
        return 0;

    if (g_supply_allow_overflow)
        return o_ptr->number;

    if (o_ptr->weight <= 0)
        return o_ptr->number;

    int current_weight = supplies_total_weight();
    int available_weight = g_supplies_max_weight - current_weight;

    if (available_weight <= 0)
        return 0;

    int idx = supplies_find_similar(o_ptr);

    if (o_ptr->tval == TV_GEM)
    {
        if (idx >= 0)
        {
            supply_entry* existing = &g_supply_entries[idx];
            int existing_weight = existing->obj.weight * existing->stored_count;
            available_weight = g_supplies_max_weight - (current_weight - existing_weight);
        }
        
        int max_count = available_weight / o_ptr->weight;
        return MIN(max_count, o_ptr->number);
    }
    
    int max_count = available_weight / o_ptr->weight;
    return MIN(max_count, o_ptr->number);
}

static void supplies_mark_dirty(void)
{
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    p_ptr->update |= (PU_BONUS);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
}

bool supplies_absorb_object(object_type* src)
{
    if (!supplies_is_supply_object(src))
        return false;

    supplies_init();

    if (src->tval == TV_GEM)
    {
        if (src->number <= 0)
        {
            object_wipe(src);
            return true;
        }
    }
    int idx = supplies_find_similar(src);

    int current_weight = supplies_total_weight();
    if (src->tval == TV_GEM)
    {
        if (idx >= 0)
        {
            supply_entry* existing = &g_supply_entries[idx];
            int existing_weight = existing->obj.weight * existing->stored_count;
            current_weight -= existing_weight;
            int new_count = existing->stored_count + src->number;
            int new_weight = existing->obj.weight * new_count;
            if (!supplies_can_add_weight(current_weight, new_weight))
                return false;
        }
        else
        {
            int add_weight = src->weight * src->number;
            if (!supplies_can_add_weight(current_weight, add_weight))
                return false;
        }
    }
    else
    {
        int add_weight = src->weight * src->number;
        if (!supplies_can_add_weight(current_weight, add_weight))
            return false;
    }


    if (IS_GEM(src))
    {
        if (idx >= 0)
        {
            supply_entry* entry = &g_supply_entries[idx];
            /* Gems use number, not pval */
            entry->stored_count += src->number;
            supplies_sync_gem_entry(entry);
            supplies_apply_auto_identification(&entry->obj);
            object_wipe(src);
            supplies_mark_dirty();
            return true;
        }
    }
    else if (idx >= 0)
    {
        supply_entry* entry = &g_supply_entries[idx];
        int total = entry->obj.number + src->number;
        int moved = MIN(total, 255);
        entry->obj.number = moved;
        entry->stored_count = 0;
        int leftover = total - moved;
        supplies_apply_auto_identification(&entry->obj);
        object_wipe(src);
        supplies_mark_dirty();
        while (leftover > 0)
        {
            supplies_reserve(g_supply_count + 1);
            entry = &g_supply_entries[idx];
            supply_entry* extra = &g_supply_entries[g_supply_count];
            object_copy(&extra->obj, &entry->obj);
            extra->obj.number = MIN(leftover, 255);
            extra->stored_count = 0;
            supplies_apply_auto_identification(&extra->obj);
            leftover -= extra->obj.number;
            g_supply_count++;
        }
        return true;
    }

    supplies_reserve(g_supply_count + 1);
    supply_entry* entry = &g_supply_entries[g_supply_count];
    object_copy(&entry->obj, src);
    entry->stored_count = 0;
    if (src->tval == TV_GEM)
    {
        /* Gems use number, not pval */
        entry->stored_count = src->number;
        supplies_sync_gem_entry(entry);
    }
    else
    {
    }
    supplies_apply_auto_identification(&entry->obj);
    object_wipe(src);
    g_supply_count++;
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

    return g_supply_entries[idx].stored_count;
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
        if (obj->tval == TV_GEM)
        {
            total += obj->weight * entry->stored_count;
        }
        else
            total += obj->weight * obj->number;
    }
    return total;
}

void supplies_count_totals(int* potions, int* herbs, int* gems)
{
    if (potions)
        *potions = 0;
    if (herbs)
        *herbs = 0;
    if (gems)
        *gems = 0;

    for (int i = 0; i < g_supply_count; i++)
    {
        supply_entry* entry = &g_supply_entries[i];
        object_type* obj = &entry->obj;
        if (!obj->k_idx)
            continue;

        if (obj->tval == TV_POTION)
        {
            if (potions)
                *potions += obj->number;
        }
        else if (obj->tval == TV_GEM)
        {
            if (gems)
                *gems += entry->stored_count;
        }
        else if (obj->tval == TV_FOOD && obj->sval <= SV_FOOD_SICKNESS)
        {
            if (herbs)
                *herbs += obj->number;
        }
    }
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
            if (entry->tval == TV_FOOD && entry->sval <= SV_FOOD_SICKNESS)
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

    if (IS_GEM(obj))
    {
        if (entry->stored_count <= 0)
            return false;

        if (amount >= entry->stored_count)
        {
            object_wipe(obj);
            entry->stored_count = 0;
            for (int move = idx + 1; move < g_supply_count; move++)
                g_supply_entries[move - 1] = g_supply_entries[move];
            g_supply_count--;
            supplies_mark_dirty();
            g_supply_limit_warned = false;
            return true;
        }

        entry->stored_count -= amount;
        if (entry->stored_count <= 0)
        {
            object_wipe(obj);
            entry->stored_count = 0;
            for (int move = idx + 1; move < g_supply_count; move++)
                g_supply_entries[move - 1] = g_supply_entries[move];
            g_supply_count--;
            supplies_mark_dirty();
            g_supply_limit_warned = false;
            return true;
        }

        supplies_sync_gem_entry(entry);
        g_supply_limit_warned = false;
        supplies_mark_dirty();
        return false;
    }

    if (amount >= obj->number)
    {
        object_wipe(obj);
        entry->stored_count = 0;
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

    if (obj->tval == TV_GEM)
    {
        entry->stored_count = obj->number;

        if (entry->stored_count <= 0)
        {
            object_wipe(obj);
            entry->stored_count = 0;
            for (int move = idx + 1; move < g_supply_count; move++)
                g_supply_entries[move - 1] = g_supply_entries[move];
            g_supply_count--;
            supplies_mark_dirty();
            return;
        }

        supplies_sync_gem_entry(entry);
        supplies_apply_auto_identification(obj);
        g_supply_limit_warned = false;
        supplies_mark_dirty();
    }
}

bool supplies_drop_amount(int idx, int amount)
{
    if (idx < 0 || idx >= g_supply_count)
        return false;

    supply_entry* entry = &g_supply_entries[idx];
    object_type* obj = &entry->obj;
    if (!obj->k_idx)
        return false;

    if (obj->tval == TV_GEM)
    {
        if (entry->stored_count <= 0 || amount <= 0)
            return false;

        if (amount > entry->stored_count)
            amount = entry->stored_count;

        object_type drop;
        object_wipe(&drop);
        object_copy(&drop, obj);
        drop.number = amount;
        drop.pval = 0;
        drop.ident &= ~(IDENT_EMPTY);

        drop_near(&drop, 0, p_ptr->py, p_ptr->px);
        bool removed = supplies_consume_quantity(idx, amount);

        if (!removed && idx < g_supply_count && g_supply_entries[idx].obj.tval == TV_GEM)
        {
            supply_entry* updated = &g_supply_entries[idx];
            supplies_sync_gem_entry(updated);
            supplies_mark_dirty();
        }

        return true;
    }

    if (amount <= 0)
        return false;

    if (amount > obj->number)
        amount = obj->number;

    object_type drop;
    object_wipe(&drop);
    object_copy(&drop, obj);
    drop.number = amount;

    drop_near(&drop, 0, p_ptr->py, p_ptr->px);
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
            int total_count = entry->stored_count;

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
                entry->stored_count -= amt;

                /* If all destroyed, wipe the entry */
                if (entry->stored_count <= 0)
                {
                    object_wipe(&entry->obj);
                    entry->stored_count = 0;

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

    /* Return the count */
    return (k);
}


