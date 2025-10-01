#include "angband.h"
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

static void supplies_reserve(int minimum)
{
    if (g_supply_capacity >= minimum)
        return;

    int new_capacity = (g_supply_capacity > 0) ? g_supply_capacity : 8;
    while (new_capacity < minimum)
        new_capacity *= 2;

    supply_entry* new_entries = C_RNEW(new_capacity, supply_entry);
    if (g_supply_entries && g_supply_count > 0)
    {
        C_COPY(new_entries, g_supply_entries, g_supply_count, supply_entry);
        FREE(g_supply_entries);
    }

    for (int i = g_supply_count; i < new_capacity; i++)
    {
        object_wipe(&new_entries[i].obj);
    }

    g_supply_entries = new_entries;
    g_supply_capacity = new_capacity;
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
        FREE(g_supply_entries);

    g_supply_entries = NULL;
    g_supply_capacity = 0;
    g_supply_count = 0;
    g_supply_initialized = false;
    g_active_supply_action = -1;
    g_pending_action = SUPPLY_MENU_ACTION_NONE;
    g_pending_group = SUPPLY_GROUP_MAX;
    g_pending_hotkey = false;
}

void supplies_reset_store(void)
{
    supplies_init();
    for (int i = 0; i < g_supply_capacity; i++)
        object_wipe(&g_supply_entries[i].obj);
    g_supply_count = 0;
    g_active_supply_action = -1;
    g_pending_action = SUPPLY_MENU_ACTION_NONE;
    g_pending_group = SUPPLY_GROUP_MAX;
    g_pending_hotkey = false;
}

bool supplies_is_supply_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->tval == TV_POTION)
        return true;

    if (o_ptr->tval == TV_STAFF)
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

    int idx = supplies_find_similar(src);
    if (idx >= 0)
    {
        object_absorb(&g_supply_entries[idx].obj, src);
        object_wipe(src);
        supplies_mark_dirty();
        return true;
    }

    supplies_reserve(g_supply_count + 1);
    object_copy(&g_supply_entries[g_supply_count].obj, src);
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

int supplies_total_weight(void)
{
    int total = 0;
    for (int i = 0; i < g_supply_count; i++)
    {
        object_type* entry = &g_supply_entries[i].obj;
        if (!entry->k_idx)
            continue;
        total += entry->weight * entry->number;
    }
    return total;
}

void supplies_count_totals(int* potions, int* herbs, int* staves)
{
    if (potions)
        *potions = 0;
    if (herbs)
        *herbs = 0;
    if (staves)
        *staves = 0;

    for (int i = 0; i < g_supply_count; i++)
    {
        object_type* entry = &g_supply_entries[i].obj;
        if (!entry->k_idx)
            continue;

        if (entry->tval == TV_POTION)
        {
            if (potions)
                *potions += entry->number;
        }
        else if (entry->tval == TV_STAFF)
        {
            if (staves)
                *staves += entry->number;
        }
        else if (entry->tval == TV_FOOD && entry->sval <= SV_FOOD_SICKNESS)
        {
            if (herbs)
                *herbs += entry->number;
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
        case SUPPLY_GROUP_STAVES:
            if (entry->tval == TV_STAFF)
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
    int slot = supplies_virtual_slot();
    if (slot < 0)
        return 0;
    return index_to_label(slot);
}

int supplies_virtual_slot(void)
{
    if (g_supply_count == 0)
        return -1;
    /* Supplies are shown after all normal inventory items. */
    return p_ptr->inven_cnt;
}

bool supplies_consume_quantity(int idx, int amount)
{
    object_type* entry = supplies_entry_at(idx);
    if (!entry || !entry->k_idx)
        return false;

    if (amount <= 0)
        return false;

    if (amount >= entry->number)
    {
        object_wipe(entry);
        for (int move = idx + 1; move < g_supply_count; move++)
            g_supply_entries[move - 1] = g_supply_entries[move];
        g_supply_count--;
        supplies_mark_dirty();
        return true;
    }

    entry->number -= amount;
    supplies_mark_dirty();
    return false;
}

void supplies_refresh_entry(int idx)
{
    object_type* entry = supplies_entry_at(idx);
    if (!entry || !entry->k_idx)
        return;

    if (entry->tval == TV_STAFF && entry->pval <= 0)
        supplies_consume_quantity(idx, entry->number);
}

bool supplies_drop_amount(int idx, int amount)
{
    object_type* entry = supplies_entry_at(idx);
    if (!entry || !entry->k_idx)
        return false;

    if (amount <= 0)
        return false;

    if (amount > entry->number)
        amount = entry->number;

    object_type drop;
    object_wipe(&drop);
    object_copy(&drop, entry);
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
        supplies_absorb_object(&copy);
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
