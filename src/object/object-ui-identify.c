/* File: object/object-ui-identify.c */

#include "angband.h"
#include "externs.h"
#include "object/object-ui-identify.h"
#include "object/object-internal.h"
#include "object/object-ui-select.h"
#include "supplies.h"


typedef enum
{
    IDENT_ENTRY_INVEN,
    IDENT_ENTRY_EQUIP,
    IDENT_ENTRY_FLOOR,
    IDENT_ENTRY_SUPPLY
} ident_entry_type;

typedef struct
{
    ident_entry_type type;
    int index;
    int supply_index;
    int floor_o_idx;
    object_type* o_ptr;
    char label[6];
    char prefix[24];
    char desc[80];
    byte color;
} ident_entry;

#define MAX_IDENT_SUPPLY 256
#define MAX_IDENT_ENTRIES (INVEN_PACK + (INVEN_TOTAL - INVEN_WIELD) + MAX_FLOOR_STACK + MAX_IDENT_SUPPLY)

static void build_ident_entry_label(int order, char out[6])
{
    char label = index_to_label(order);
    out[0] = label;
    out[1] = ')';
    out[2] = '\0';
}

bool display_unified_identify_menu(bool include_floor, int* out_item,
    object_type** out_object)
{
    ident_entry entries[MAX_IDENT_ENTRIES];
    object_choice_entry choices[MAX_IDENT_ENTRIES];
    int entry_count = 0;
    int floor_list[MAX_FLOOR_STACK];
    int floor_num = 0;
    int supply_count = supplies_entry_count();
    int selected = -1;

    if (!out_item || !out_object)
        return false;

    if (include_floor)
    {
        floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py,
            p_ptr->px, 0x00);
        for (int i = 0; i < floor_num && entry_count < MAX_IDENT_ENTRIES; i++)
        {
            int o_idx = floor_list[i];
            object_type* o_ptr = &o_list[o_idx];
            ident_entry* entry;

            if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
                continue;

            entry = &entries[entry_count];
            entry->type = IDENT_ENTRY_FLOOR;
            entry->index = 0;
            entry->supply_index = -1;
            entry->floor_o_idx = o_idx;
            entry->o_ptr = o_ptr;
            object_choice_entry_make(&choices[entry_count], 0 - o_idx,
                o_ptr, "-)", NULL);
            entry_count++;
        }
    }

    for (int i = 0; i < supply_count && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = supplies_entry_at(i);
        ident_entry* entry;
        char label[6];
        cptr prefix = "Supplies";

        if (!o_ptr || !o_ptr->k_idx
            || !object_is_unidentified_for_display(o_ptr))
        {
            continue;
        }

        if (o_ptr->tval == TV_POTION)
            prefix = "Supplies (potions)";
        else if (o_ptr->tval == TV_GEM)
            prefix = "Supplies (gems)";
        else if (o_ptr->tval == TV_FOOD)
            prefix = "Supplies (food)";
        else if (supplies_group_matches_object(SUPPLY_GROUP_LIGHTS, o_ptr))
            prefix = "Supplies (lights/oil)";

        entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_SUPPLY;
        entry->index = i;
        entry->supply_index = i;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, label);
        object_choice_entry_make(&choices[entry_count], SUPPLIES_INDEX + i,
            o_ptr, label, prefix);
        entry_count++;
    }

    for (int i = 0; i < INVEN_PACK && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = &inventory[i];
        ident_entry* entry;
        char label[6];

        if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_INVEN;
        entry->index = i;
        entry->supply_index = -1;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, label);
        object_choice_entry_make(&choices[entry_count], i, o_ptr, label,
            NULL);
        entry_count++;
    }

    for (int i = INVEN_WIELD; i < INVEN_TOTAL
         && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = &inventory[i];
        ident_entry* entry;
        char label[6];

        if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_EQUIP;
        entry->index = i;
        entry->supply_index = -1;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, label);
        object_choice_entry_make(&choices[entry_count], i, o_ptr, label,
            mention_use(i));
        entry_count++;
    }

    if (entry_count == 0)
    {
        msg_print("There is nothing unidentified here.");
        return false;
    }

    if (!object_choice_overlay("Identify which item?", NULL, choices,
            entry_count, 0, &selected))
    {
        return false;
    }

    if (selected < 0 || selected >= entry_count)
        return false;

    if (entries[selected].type == IDENT_ENTRY_FLOOR)
    {
        *out_item = 0 - entries[selected].floor_o_idx;
        *out_object = &o_list[entries[selected].floor_o_idx];
    }
    else if (entries[selected].type == IDENT_ENTRY_SUPPLY)
    {
        *out_item = SUPPLIES_INDEX + entries[selected].supply_index;
        *out_object = entries[selected].o_ptr;
    }
    else
    {
        *out_item = entries[selected].index;
        *out_object = &inventory[entries[selected].index];
    }

    return true;
}

/*
 * Returns the paired artefact index for a given artefact, or 0 if none.
 * Paired weapons can be wielded together in main hand and off-hand
 * without requiring Two Weapon Fighting and without off-hand penalties.
 */
