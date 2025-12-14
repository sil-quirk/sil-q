/* File: cmd3.c */

/*
 * Copyright (c) 2001 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"

/*
 * Helper function to determine the equip sound based on item type
 */
static int get_equip_sound(const object_type* o_ptr)
{
    /* Swords */
    if (o_ptr->tval == TV_SWORD)
        return MSG_EQUIP_SWORD;
    
    /* Bows and arrows */
    if (o_ptr->tval == TV_BOW || o_ptr->tval == TV_ARROW)
        return MSG_EQUIP_BOW;
    
    /* Other weapons */
    if (o_ptr->tval == TV_POLEARM || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING)
        return MSG_EQUIP_WEAPON;
    
    /* Chain armor (mail) */
    if (o_ptr->tval == TV_MAIL)
        return MSG_EQUIP_MAIL;
    
    /* Leather armor (soft armor) */
    if (o_ptr->tval == TV_SOFT_ARMOR)
        return MSG_EQUIP_LEATHER;
    
    /* All other types of armor */
    if (o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_CLOAK || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return MSG_EQUIP_ARMOR;
    
    /* Rings and amulets */
    if (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        return MSG_EQUIP_JEWELRY;
    
    /* Default - no sound */
    return -1;
}

/*
 * Helper function to determine the unequip sound based on item type
 */
static int get_unequip_sound(const object_type* o_ptr)
{
    /* Swords */
    if (o_ptr->tval == TV_SWORD)
        return MSG_UNEQUIP_SWORD;
    
    /* Bows and arrows */
    if (o_ptr->tval == TV_BOW || o_ptr->tval == TV_ARROW)
        return MSG_UNEQUIP_BOW;
    
    /* Other weapons */
    if (o_ptr->tval == TV_POLEARM || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING)
        return MSG_UNEQUIP_WEAPON;
    
    /* Chain armor (mail) */
    if (o_ptr->tval == TV_MAIL)
        return MSG_UNEQUIP_MAIL;
    
    /* Leather armor (soft armor) */
    if (o_ptr->tval == TV_SOFT_ARMOR)
        return MSG_UNEQUIP_LEATHER;
    
    /* All other types of armor */
    if (o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_CLOAK || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return MSG_UNEQUIP_ARMOR;
    
    /* Rings and amulets */
    if (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        return MSG_UNEQUIP_JEWELRY;
    
    /* Default - no sound */
    return -1;
}

/*
 * The "wearable" tester
 */
static bool item_tester_hook_wear(const object_type* o_ptr)
{
    // Despite being a crown, the Iron Crown cannot be worn
    if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_3))
        return (false);

    /* Check for a usable slot */
    if (wield_slot(o_ptr) >= INVEN_WIELD)
        return (true);

    /* Assume not wearable */
    return (false);
}

static bool item_tester_hook_ring_slots(const object_type* o_ptr)
{
    return (o_ptr == &inventory[INVEN_LEFT]) || (o_ptr == &inventory[INVEN_RIGHT]);
}

bool throw_slot_menu_active = false;
bool throw_slot_enabled[INVEN_TOTAL];

static bool item_tester_hook_throw_slots(const object_type* o_ptr)
{
    if (!throw_slot_menu_active)
        return false;

    if (!o_ptr)
        return false;

    if ((o_ptr < inventory) || (o_ptr >= inventory + INVEN_TOTAL))
        return false;

    int slot = (int)(o_ptr - inventory);

    return throw_slot_enabled[slot];
}

bool open_supplies_menu_with_context(supply_menu_action default_action, int default_group, bool default_focus, bool default_hotkey)
{
    supply_menu_request request = {0};
    supply_menu_action action = default_action;
    bool hotkey = default_hotkey;
    bool focus = default_focus;
    int group = default_group;

    if (supplies_has_pending_action())
    {
        supply_menu_action pending = supplies_pending_action();
        if (pending != SUPPLY_MENU_ACTION_NONE)
            action = pending;
        hotkey = supplies_pending_hotkey();
        int pending_group = supplies_pending_group();
        if (pending_group >= 0 && pending_group < SUPPLY_GROUP_MAX)
        {
            focus = true;
            group = pending_group;
        }
        supplies_clear_pending_action();
    }

    request.action = action;
    request.hotkey_mode = hotkey;
    if (focus && group >= 0 && group < SUPPLY_GROUP_MAX)
    {
        request.focus_group = true;
        request.group = group;
    }

    return do_cmd_knowledge_supplies(&request);
}

/* Flag indicating enhanced menus need to refresh the main display after closing */
static bool enhanced_drop_refresh_pending = false;

/*
 * Use an item by index, helper for enhanced menus
 */
void do_cmd_use_item_by_index(int item)
{
    if (item == SUPPLIES_INDEX)
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE, -1, false, true);
        return;
    }

    object_type* o_ptr;

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
        log_debug("do_cmd_use_item_by_index: Using item from inventory, index=%d", item);
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
        log_debug("do_cmd_use_item_by_index: Using item from floor, index=%d, o_list index=%d", item, 0 - item);
    }

    // determine the action based on the item type
    switch (o_ptr->tval)
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
    case TV_LIGHT:
    case TV_AMULET:
    case TV_RING:
    case TV_ARROW:
    case TV_FLASK:
    {
        if (item < INVEN_WIELD)
        {
            object_type* l_ptr = &inventory[INVEN_LITE];
            bool try_to_wield = true;

            // possibly refuel a light
            if ((o_ptr->tval == TV_FLASK)
                || ((l_ptr->tval == o_ptr->tval) && (l_ptr->sval == o_ptr->sval)
                    && ((o_ptr->sval == SV_LIGHT_TORCH)
                        || (o_ptr->sval == SV_LIGHT_LANTERN)
                        || (o_ptr->sval == SV_LIGHT_MALLORN))))
            {
                if ((l_ptr->sval == SV_LIGHT_TORCH)
                    && (o_ptr->tval != TV_FLASK))
                {
                    if ((o_ptr->timeout + l_ptr->timeout <= FUEL_TORCH)
                        || get_check(
                            "Refueling from this torch will waste some fuel. "
                            "Proceed? "))
                    {
                        do_cmd_refuel_torch(o_ptr, item, false);
                        try_to_wield = false;
                    }
                }
                else if ((l_ptr->sval == SV_LIGHT_MALLORN)
                    && (o_ptr->tval != TV_FLASK))
                {
                    if ((o_ptr->timeout + l_ptr->timeout <= FUEL_TORCH)
                        || get_check(
                            "Refueling from this mallorn torch will waste "
                            "some fuel. Proceed? "))
                    {
                        do_cmd_refuel_torch(o_ptr, item, true);
                        try_to_wield = false;
                    }
                }
                else if (l_ptr->sval == SV_LIGHT_LANTERN)
                {
                    if ((o_ptr->timeout + l_ptr->timeout <= FUEL_LAMP)
                        || get_check(
                            "Refueling from this flask will waste some oil. "
                            "Proceed? "))
                    {
                        do_cmd_refuel_lamp(o_ptr, item);
                        try_to_wield = false;
                    }
                }
            }

            if (try_to_wield)
            {
                log_debug("do_cmd_use_item_by_index: Calling do_cmd_wield with item=%d (o_ptr tval=%d)", item, o_ptr->tval);
                /* Handle arrows and throwing weapons */
                if (o_ptr->tval == TV_ARROW)
                {
                    do_cmd_wield(o_ptr, item);
                }
                else
                {
                    do_cmd_wield(o_ptr, item);
                }
            }
        }
        else
        {
            /* Handle equipped arrows specially */
            if (o_ptr->tval == TV_ARROW)
            {
                do_cmd_takeoff(o_ptr, item);
            }
            else
            {
                do_cmd_takeoff(o_ptr, item);
            }
        }
        break;
    }
    case TV_NOTE:
    {
        note_info_screen(o_ptr);
        break;
    }
    case TV_METAL:
    {
        msg_print("To melt down pieces of mithril, take them to a forge and "
                  "type (,).");
        break;
    }
    case TV_CHEST:
    {
        msg_print("You would need to put it down to open it.");
        break;
    }
    case TV_STAFF:
    case TV_GEM:
    {
        do_cmd_activate_staff(o_ptr, item);
        break;
    }
    case TV_HORN:
    {
        do_cmd_play_instrument(o_ptr, item);
        break;
    }
    case TV_POTION:
    {
        do_cmd_quaff_potion(o_ptr, item);
        break;
    }
    case TV_FOOD:
    {
        do_cmd_eat_food(o_ptr, item);
        break;
    }
    default:
    {
        msg_print("It has no use.");
        break;
    }
    }
}

/*
 * Use an item, a unified 'use' command.
 */
void do_cmd_use_item(void)
{
    /* Set up for enhanced menu cycling */
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Mark that 'u' command opened this menu */
    current_menu_command = 'u';
    current_menu_state = 0;  /* Start with inventory */
    
    /* Start the enhanced menu system */
    do_cmd_use_item_enhanced();
}

/*
 * Wrapper for wear/wield command with enhanced menu support
 */
void do_cmd_wield_wrapper(void)
{
    /* Set up for enhanced menu cycling */
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Mark that 'w' command opened this menu */
    current_menu_command = 'w';
    current_menu_state = 0;  /* Start with inventory */
    
    /* Start the enhanced menu system */
    do_cmd_wield_enhanced();
}

/*
 * Enhanced wear/wield command that supports cycling between inventory/equipment
 */
void do_cmd_wield_enhanced(void)
{
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Clear any previous menu actions at start of new session */
    extern int enhanced_menu_action;
    extern int enhanced_equip_action;
    enhanced_menu_action = 0;
    enhanced_equip_action = 0;
    
    log_trace("do_cmd_wield_enhanced: Starting enhanced wear/wield cycle");
    
    /* Clear any active banner before starting enhanced menu cycle */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }
    
    /* Set the filter to only show wearable items */
    item_tester_hook = item_tester_hook_wear;
    log_debug("do_cmd_wield_enhanced: Set item_tester_hook to item_tester_hook_wear (%p)", (void*)item_tester_hook);
    
    /* Continue cycling until user escapes or performs an action */
    while (true)
    {
        if (current_menu_state == 0) {
            /* Display inventory */
            do_cmd_inven();
            
            /* Check if user wants to switch to equipment */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                current_menu_state = 1;
                enhanced_menu_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_menu_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_inven */
                enhanced_menu_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
        else {
            /* Display equipment */
            do_cmd_equip();
            
            /* Check if user wants to switch to inventory */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                current_menu_state = 0;
                enhanced_equip_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_equip_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_equip */
                enhanced_equip_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
    }
    
    /* Clear the filter */
    item_tester_hook = NULL;
    
    /* Clear the command state */
    current_menu_command = 0;
    current_menu_state = 0;
}

/*
 * Enhanced use item command that supports cycling between inventory/equipment
 */
void do_cmd_use_item_enhanced(void)
{
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Clear any previous menu actions at start of new session */
    extern int enhanced_menu_action;
    extern int enhanced_equip_action;
    enhanced_menu_action = 0;
    enhanced_equip_action = 0;
    
    log_trace("do_cmd_use_item_enhanced: Starting enhanced use item cycle");
    
    /* Clear any active banner before starting enhanced menu cycle */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }
    
    /* Continue cycling until user escapes or performs an action */
    while (true)
    {
        if (current_menu_state == 0) {
            /* Display inventory */
            do_cmd_inven();
            
            /* Check if user wants to switch to equipment */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                current_menu_state = 1;
                enhanced_menu_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_menu_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_inven */
                enhanced_menu_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
        else {
            /* Display equipment */
            do_cmd_equip();
            
            /* Check if user wants to switch to inventory */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                current_menu_state = 0;
                enhanced_equip_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_equip_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_equip */
                enhanced_equip_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
    }
    
    /* Clear the command state */
    current_menu_command = 0;
    current_menu_state = 0;
}

/*
 * Direct access inventory with cycling support
 */
void do_cmd_inven_direct(void)
{
    log_debug("do_cmd_inven_direct: Starting direct access inventory with cycling");
    
    int menu_state = 0;  /* 0=inventory, 1=equipment */
    
    while (true) {
        if (menu_state == 0) {
            /* Display inventory */
            log_trace("do_cmd_inven_direct: Showing inventory");
            do_cmd_inven();
            
            /* Check action */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                log_trace("do_cmd_inven_direct: Switching to equipment");
                menu_state = 1;
                enhanced_menu_action = 0;
                continue;
            }
            else {
                /* Exit or item examined */
                enhanced_menu_action = 0;
                break;
            }
        }
        else {
            /* Display equipment */
            log_trace("do_cmd_inven_direct: Showing equipment");
            do_cmd_equip();
            
            /* Check action */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                log_trace("do_cmd_inven_direct: Switching to inventory");
                menu_state = 0;
                enhanced_equip_action = 0;
                continue;
            }
            else {
                /* Exit or item examined */
                enhanced_equip_action = 0;
                break;
            }
        }
    }
    
    log_debug("do_cmd_inven_direct: Direct access cycling finished");
}

/*
 * Direct access equipment with cycling support
 */
void do_cmd_equip_direct(void)
{
    log_debug("do_cmd_equip_direct: Starting direct access equipment with cycling");
    
    int menu_state = 1;  /* 0=inventory, 1=equipment */
    
    while (true) {
        if (menu_state == 0) {
            /* Display inventory */
            log_trace("do_cmd_equip_direct: Showing inventory");
            do_cmd_inven();
            
            /* Check action */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                log_trace("do_cmd_equip_direct: Switching to equipment");
                menu_state = 1;
                enhanced_menu_action = 0;
                continue;
            }
            else {
                /* Exit or item examined */
                enhanced_menu_action = 0;
                break;
            }
        }
        else {
            /* Display equipment */
            log_trace("do_cmd_equip_direct: Showing equipment");
            do_cmd_equip();
            
            /* Check action */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                log_trace("do_cmd_equip_direct: Switching to inventory");
                menu_state = 0;
                enhanced_equip_action = 0;
                continue;
            }
            else {
                /* Exit or item examined */
                enhanced_equip_action = 0;
                break;
            }
        }
    }
    
    log_debug("do_cmd_equip_direct: Direct access cycling finished");
}

/*
 * Display inventory
 */
void do_cmd_inven(void)
{
    log_debug("do_cmd_inven: Starting inventory command");
    
    /* Clear any active banner before showing the menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }
    
    /* Hack -- Start in "inventory" mode */
    p_ptr->command_wrk = (USE_INVEN);

    enhanced_inventory_selected_item = -1;

    /* Save screen */
    screen_save();
    log_debug("do_cmd_inven: Screen saved");

    /* Hack -- show empty slots */
    item_tester_full = true;

    /* Force viewing mode */
    p_ptr->command_see = true;

    /* Display the inventory with scrolling capability */
    show_inven_enhanced();

    /* Hack -- hide empty slots */
    item_tester_full = false;

    /* Load screen */
    screen_load();
    log_debug("do_cmd_inven: Screen loaded");

    extern int enhanced_menu_action;
    extern int enhanced_inventory_selected_item;

    int action = enhanced_menu_action;
    int selected_index = enhanced_inventory_selected_item;
    bool death_view = death_spectator_active();

    switch (action)
    {
    case ENHANCED_ACTION_EXAMINE:
    {
        log_trace("do_cmd_inven: Examining item %d", selected_index);
        extern char current_menu_command;
        /* Show comparisons when accessed via 'x' menu OR when examining via arrow-right in direct access */
        bool include_comparisons = (current_menu_command == 'u' || current_menu_command == 'x' || current_menu_command == 0);
        describe_item_with_comparisons(selected_index, include_comparisons);
        break;
    }

    case ENHANCED_ACTION_USE:
        if (death_view)
        {
            msg_print("You can no longer take that action.");
        }
        else
        {
            log_trace("do_cmd_inven: Using item %d", selected_index);
            if (selected_index != -1)
                do_cmd_use_item_by_index(selected_index);
        }
        break;

    case ENHANCED_ACTION_DROP:
        if (death_view)
        {
            msg_print("You can no longer take that action.");
        }
        else
        {
            log_trace("do_cmd_inven: Dropping item %d", selected_index);
            if (selected_index >= 0)
                do_cmd_drop_item_by_index(selected_index);
            else
                bell("Cannot drop floor items from this menu!");
        }
        break;

    case ENHANCED_ACTION_SUPPLIES:
    {
        log_trace("do_cmd_inven: Opening supplies menu (command=%c)", current_menu_command ? current_menu_command : '0');
        supply_menu_action default_action = SUPPLY_MENU_ACTION_NONE;
        bool default_hotkey = false;
        if (current_menu_command == 'u')
        {
            default_action = SUPPLY_MENU_ACTION_USE;
            default_hotkey = true;
        }
        else if (current_menu_command == 'd')
        {
            default_action = SUPPLY_MENU_ACTION_DROP;
            default_hotkey = true;
        }
        open_supplies_menu_with_context(default_action, -1, false, default_hotkey);
        break;
    }

    default:
        break;
    }

    if (enhanced_drop_refresh_pending)
    {
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_MESSAGE);
        enhanced_drop_refresh_pending = false;
    }

    /* Ensure the main display reflects any changes (drops, etc.) */
    handle_stuff();
    Term_fresh();

    if (action != ENHANCED_ACTION_SWITCH)
        enhanced_menu_action = ENHANCED_ACTION_NONE;
    enhanced_inventory_selected_item = -1;
    
    log_debug("do_cmd_inven: Exiting");
}

/*
 * Display equipment
 */
void do_cmd_equip(void)
{
    log_debug("do_cmd_equip: Starting equipment command");
    
    /* Clear any active banner before showing the menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }
    
    /* Hack -- Start in "equipment" mode */
    p_ptr->command_wrk = (USE_EQUIP);

    enhanced_equipment_selected_item = -1;

    /* Save screen */
    screen_save();
    log_debug("do_cmd_equip: Screen saved");

    /* Hack -- show empty slots */
    item_tester_full = true;

    /* Force viewing mode */
    p_ptr->command_see = true;

    /* Display the equipment with scrolling capability */
    show_equip_enhanced();

    /* Hack -- undo the hack above */
    item_tester_full = false;

    /* Load screen */
    screen_load();
    log_debug("do_cmd_equip: Screen loaded");

    extern int enhanced_equip_action;
    extern int enhanced_equipment_selected_item;

    int action = enhanced_equip_action;
    int selected_index = enhanced_equipment_selected_item;
    bool death_view = death_spectator_active();

    switch (action)
    {
    case ENHANCED_ACTION_EXAMINE:
        log_trace("do_cmd_equip: Examining item %d", selected_index);
        if (selected_index >= INVEN_WIELD && selected_index < INVEN_TOTAL)
            object_info_screen(&inventory[selected_index]);
        break;

    case ENHANCED_ACTION_USE:
        if (death_view)
        {
            msg_print("You can no longer take that action.");
        }
        else
        {
            log_trace("do_cmd_equip: Using item %d", selected_index);
            if (selected_index != -1)
                do_cmd_use_item_by_index(selected_index);
        }
        break;

    case ENHANCED_ACTION_DROP:
        if (death_view)
        {
            msg_print("You can no longer take that action.");
        }
        else
        {
            log_trace("do_cmd_equip: Dropping item %d", selected_index);
            if (selected_index >= INVEN_WIELD)
                do_cmd_drop_item_by_index(selected_index);
        }
        break;

    default:
        break;
    }

    if (enhanced_drop_refresh_pending)
    {
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_MESSAGE);
        enhanced_drop_refresh_pending = false;
    }

    /* Ensure the main display reflects any changes (drops, etc.) */
    handle_stuff();
    Term_fresh();

    if (action != ENHANCED_ACTION_SWITCH)
        enhanced_equip_action = ENHANCED_ACTION_NONE;
    enhanced_equipment_selected_item = -1;
    
    log_debug("do_cmd_equip: Exiting");
}

/*
 * Wield or wear a single item from the pack or floor
 */
void do_cmd_wield(object_type* default_o_ptr, int default_item)
{
    int item, slot;

    object_type* o_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    cptr act;

    cptr q, s;

    int i, quantity, original_quantity;

    bool weapon_less_effective = false;

    bool grants_two_weapon = false;

    char o_name[80];

    bool combine = false;
    bool is_throwing = false;

    u32b f1, f2, f3;

    log_debug("do_cmd_wield: Called with default_o_ptr=%p, default_item=%d", (void*)default_o_ptr, default_item);

    /* Ensure throw_slot_menu_active is false at start */
    throw_slot_menu_active = false;
    for (i = 0; i < INVEN_TOTAL; i++)
        throw_slot_enabled[i] = false;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
        log_debug("do_cmd_wield: Using default item, tval=%d, sval=%d, k_idx=%d", 
            o_ptr->tval, o_ptr->sval, o_ptr->k_idx);
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = item_tester_hook_wear;

        /* Get an item */
        q = "Wear/Wield which item? ";
        s = "You have nothing you can wear or wield.";
        if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR)))
            return;

        /* Get the item (in the pack) */
        if (item >= 0)
        {
            o_ptr = &inventory[item];
        }
        else
        {
            o_ptr = &o_list[0 - item];
        }
    }

    // remember how many there were
    original_quantity = o_ptr->number;

    // Check whether it would be too heavy
    if ((item < 0)
        && (p_ptr->total_weight + o_ptr->weight > weight_limit() * 3 / 2))
    {
        /* Describe it */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        log_debug("do_cmd_wield: Floor item too heavy - total=%d + item=%d > limit=%d", 
            p_ptr->total_weight, o_ptr->weight, weight_limit() * 3 / 2);

        if (o_ptr->k_idx)
            msg_format("You cannot lift %s.", o_name);
        else
            log_debug("do_cmd_wield: WARNING - o_ptr->k_idx is 0, no message shown to user!");

        /* Abort */
        return;
    }
    
    log_debug("do_cmd_wield: Weight check passed or inventory item (item=%d)", item);

    /* Check the slot */
    slot = wield_slot(o_ptr);

    /* Ask for ring to replace */
    if ((o_ptr->tval == TV_RING) && inventory[INVEN_LEFT].k_idx
        && inventory[INVEN_RIGHT].k_idx)
    {
        item_tester_tval = TV_RING;
        item_tester_hook = item_tester_hook_ring_slots;
        item_tester_full = false;

        q = "Replace which ring? ";
        s = "Oops.";
        if (!get_item(&slot, q, s, USE_EQUIP))
        {
            item_tester_tval = 0;
            item_tester_hook = NULL;
            return;
        }

        item_tester_tval = 0;
        item_tester_hook = NULL;
    }

    object_flags(o_ptr, &f1, &f2, &f3);
    is_throwing = player_can_treat_as_throwing_flags(o_ptr, f3);

    log_debug("do_cmd_wield: item=%d, is_throwing=%d, slot=%d", item, is_throwing, slot);

    if (is_throwing)
    {
        bool any_throw_dest = false;
        int slot_choice;

        log_debug("do_cmd_wield: Throwing weapon detected, showing slot menu");
        throw_slot_menu_active = true;

        for (i = 0; i < INVEN_TOTAL; i++)
            throw_slot_enabled[i] = false;

        {
            object_type* wield_ptr = &inventory[INVEN_WIELD];
            bool allow_wield = true;

            if (wield_ptr->k_idx && cursed_p(wield_ptr)
                && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
            {
                allow_wield = false;
            }

            if (allow_wield)
            {
                throw_slot_enabled[INVEN_WIELD] = true;
                any_throw_dest = true;
            }
        }

        {
            object_type* q1_ptr = &inventory[INVEN_QUIVER1];
            bool allow_quiver = true;

            if (q1_ptr->k_idx && cursed_p(q1_ptr)
                && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
            {
                allow_quiver = false;
            }

            if (allow_quiver)
            {
                throw_slot_enabled[INVEN_QUIVER1] = true;
                any_throw_dest = true;
            }
        }

        {
            object_type* q2_ptr = &inventory[INVEN_QUIVER2];
            bool allow_quiver = true;

            if (q2_ptr->k_idx && cursed_p(q2_ptr)
                && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
            {
                allow_quiver = false;
            }

            if (allow_quiver)
            {
                throw_slot_enabled[INVEN_QUIVER2] = true;
                any_throw_dest = true;
            }
        }

        if (!any_throw_dest)
        {
            log_debug("do_cmd_wield: No available slot for throwing weapon, returning");
            msg_print("You have no available slot for that throwing weapon.");
            throw_slot_menu_active = false;
            return;
        }

        slot_choice = slot;

        if (!throw_slot_enabled[slot_choice])
        {
            if (throw_slot_enabled[INVEN_QUIVER1])
                slot_choice = INVEN_QUIVER1;
            else if (throw_slot_enabled[INVEN_QUIVER2])
                slot_choice = INVEN_QUIVER2;
            else
                slot_choice = INVEN_WIELD;
        }

        item_tester_hook = item_tester_hook_throw_slots;
        item_tester_full = false;

        q = "Place throwing weapon where? ";
        s = "Oops.";

        bool saved_command_see = p_ptr->command_see;
        byte saved_command_wrk = p_ptr->command_wrk;
        p_ptr->command_see = true;
        p_ptr->command_wrk = (USE_EQUIP);

        bool slot_selected = get_item(&slot_choice, q, s, USE_EQUIP);

        p_ptr->command_see = saved_command_see;
        p_ptr->command_wrk = saved_command_wrk;

        if (!slot_selected)
        {
            log_debug("do_cmd_wield: User cancelled slot selection, cleaning up and returning");
            item_tester_hook = NULL;
            item_tester_full = false;

            for (i = 0; i < INVEN_TOTAL; i++)
                throw_slot_enabled[i] = false;

            throw_slot_menu_active = false;
            return;
        }

        log_debug("do_cmd_wield: User selected slot %d for throwing weapon", slot_choice);

        item_tester_hook = NULL;
        item_tester_full = false;
        throw_slot_menu_active = false;

        slot = slot_choice;

        if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
        {
            if (inventory[slot].k_idx
                && object_similar(&inventory[slot], o_ptr))
                combine = true;
        }

        for (i = 0; i < INVEN_TOTAL; i++)
            throw_slot_enabled[i] = false;
    }
    else
    {
        // Special cases for merging arrows
        if (object_similar(&inventory[INVEN_QUIVER1], o_ptr))
        {
            slot = INVEN_QUIVER1;
            combine = true;
        }
        else if (object_similar(&inventory[INVEN_QUIVER2], o_ptr))
        {
            slot = INVEN_QUIVER2;
            combine = true;
        }
        /* Ask for arrow set to replace */
        else if (o_ptr->tval == TV_ARROW)
        {
            bool any_quiver_dest = false;
            int slot_choice = slot;

            throw_slot_menu_active = true;

            for (i = 0; i < INVEN_TOTAL; i++)
                throw_slot_enabled[i] = false;

            {
                object_type* q1_ptr = &inventory[INVEN_QUIVER1];
                bool allow_quiver = true;

                if (q1_ptr->k_idx && cursed_p(q1_ptr)
                    && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
                {
                    allow_quiver = false;
                }

                if (allow_quiver)
                {
                    throw_slot_enabled[INVEN_QUIVER1] = true;
                    any_quiver_dest = true;
                }
            }

            {
                object_type* q2_ptr = &inventory[INVEN_QUIVER2];
                bool allow_quiver = true;

                if (q2_ptr->k_idx && cursed_p(q2_ptr)
                    && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
                {
                    allow_quiver = false;
                }

                if (allow_quiver)
                {
                    throw_slot_enabled[INVEN_QUIVER2] = true;
                    any_quiver_dest = true;
                }
            }

            if (!any_quiver_dest)
            {
                msg_print("You have no available quiver slot for those arrows.");
                throw_slot_menu_active = false;
                for (i = 0; i < INVEN_TOTAL; i++)
                    throw_slot_enabled[i] = false;
                return;
            }

            if (!throw_slot_enabled[slot_choice])
            {
                if (throw_slot_enabled[INVEN_QUIVER1])
                    slot_choice = INVEN_QUIVER1;
                else if (throw_slot_enabled[INVEN_QUIVER2])
                    slot_choice = INVEN_QUIVER2;
            }

            item_tester_hook = item_tester_hook_throw_slots;
            item_tester_full = false;

            q = "Place arrows in which quiver? ";
            s = "Oops.";

            bool saved_command_see = p_ptr->command_see;
            byte saved_command_wrk = p_ptr->command_wrk;
            p_ptr->command_see = true;
            p_ptr->command_wrk = (USE_EQUIP);

            bool slot_selected = get_item(&slot_choice, q, s, USE_EQUIP);

            p_ptr->command_see = saved_command_see;
            p_ptr->command_wrk = saved_command_wrk;

            if (!slot_selected)
            {
                item_tester_hook = NULL;
                item_tester_full = false;
                throw_slot_menu_active = false;
                for (i = 0; i < INVEN_TOTAL; i++)
                    throw_slot_enabled[i] = false;
                return;
            }

            item_tester_hook = NULL;
            item_tester_full = false;
            throw_slot_menu_active = false;

            slot = slot_choice;

            if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
            {
                if (inventory[slot].k_idx && object_similar(&inventory[slot], o_ptr))
                    combine = true;
            }

            for (i = 0; i < INVEN_TOTAL; i++)
                throw_slot_enabled[i] = false;
        }
    }

    // Ask about two weapon fighting if necessary
    for (i = 0; i < o_ptr->abilities; i++)
    {
        if ((o_ptr->skilltype[i] == S_MEL)
            && (o_ptr->abilitynum[i] == MEL_TWO_WEAPON)
            && object_known_p(o_ptr))
        {
            grants_two_weapon = true;
        }
    }
    if ((p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON] || grants_two_weapon)
        && ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM)
            || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING)))
    {
        if (!(k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
            && !(k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF)))
        {
            if (get_check("Do you wish to wield it in your off-hand? "))
            {
                slot = INVEN_ARM;
            }
        }
    }

    /* Prevent wielding into a cursed slot */
    if (cursed_p(&inventory[slot]))
    {
        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[slot], false, 0);

        /* Message */
        msg_format("You cannot bear to give up the %s you are %s.", o_name,
            describe_use(slot));

        /* Cancel the command */
        return;
    }

    /* Check if Maedhros character is trying to wield a two-handed weapon */
    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS))
    {
        msg_print("Your injury prevents you from wielding two-handed weapons.");
        return;
    }

    /* Check if Maedhros character is trying to wield a shield */
    if ((o_ptr->tval == TV_SHIELD)
        && (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS))
    {
        msg_print("Your injury prevents you from using shields.");
        return;
    }

    /* Deal with wielding of two-handed weapons when already using a shield */
    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && (inventory[INVEN_ARM].k_idx))
    {
        if (cursed_p(&inventory[INVEN_ARM]))
        {
            if (inventory[INVEN_ARM].tval == TV_SHIELD)
            {
                msg_print("You would need to remove your shield, but cannot "
                          "bear to part "
                          "with it.");
            }
            else
            {
                msg_print("You would need to remove your off-hand weapon, but "
                          "cannot bear to "
                          "part with it.");
            }

            /* Cancel the command */
            return;
        }

        // warn about dropping item in left hand
        if ((item < 0) && (&inventory[INVEN_PACK - 1])->tval)
        {
            /* Flush input */
            flush();

            if (inventory[INVEN_ARM].tval == TV_SHIELD)
            {
                if (!get_check(
                        "This would require removing (and dropping) your "
                        "shield. Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
            else
            {
                msg_print("This would require removing (and dropping) your "
                          "off-hand weapon.");
                if (!get_check("Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
        }
    }

    /* Deal with wielding of shield or second weapon when already wielding a two
     * handed weapon */
    if ((slot == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED)))
    {
        if (cursed_p(&inventory[INVEN_WIELD]))
        {
            msg_print("You would need to put down your weapon, but cannot bear "
                      "to part "
                      "with it.");

            /* Cancel the command */
            return;
        }

        // warn about dropping item in left hand
        if ((item < 0) && (&inventory[INVEN_PACK - 1])->tval)
        {
            /* Flush input */
            flush();

            if (inventory[INVEN_ARM].tval == TV_SHIELD)
            {
                if (!get_check(
                        "This would require removing (and dropping) your "
                        "weapon. Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
            else
            {
                msg_print(
                    "This would require removing (and dropping) your weapon.");
                if (!get_check("Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
        }
    }

    /* Deal with wielding of shield or second weapon when already wielding a
     * hand and a half weapon */
    if ((slot == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_HAND_AND_A_HALF))
        && (!inventory[INVEN_ARM].k_idx))
    {
        weapon_less_effective = true;
    }
    
    /* Oath of Light: warn before equipping shadowed items */
    if (chosen_oath(OATH_LIGHT) && !oath_invalid(OATH_LIGHT))
    {
        object_flags(o_ptr, &f1, &f2, &f3);
        if ((f2 & TR2_DARKNESS) || (f3 & TR3_LIGHT_CURSE))
        {
            char* prompt = oath_confirmation_prompt(OATH_LIGHT);
            if (!prompt || !prompt[0]) {
                prompt = "This item will dim your light. Break the Oath of Light?";
            }
            
            if (!get_check_oath_multiline(prompt))
            {
                log_trace("do_cmd_wield: Player declined to break Oath of Light for item (tval=%d, sval=%d)", o_ptr->tval, o_ptr->sval);
                return;
            }
            
            p_ptr->oaths_broken |= OATH_LIGHT_FLAG;
            p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] = false;
            apply_oath_breaking_curse(OATH_LIGHT);
            metarun_ban_oath(OATH_LIGHT);
            log_trace("do_cmd_wield: Oath of Light broken by equipping shadowed item");
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain local object */
    object_copy(i_ptr, o_ptr);

    bool target_is_quiver = (slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2);

    // Handle quantity differently for arrows or throwing weapons heading to a quiver
    if ((i_ptr->tval == TV_ARROW) || (is_throwing && target_is_quiver))
    {
        if (combine)
        {
            int stack_limit = object_stack_limit(&inventory[slot]);
            quantity = MIN(o_ptr->number,
                stack_limit - (&inventory[slot])->number);
        }
        else
        {
            int stack_limit = object_stack_limit(i_ptr);
            quantity = MIN(o_ptr->number, stack_limit);
        }
    }
    else
    {
        quantity = 1;
    }

    /* Modify quantity */
    i_ptr->number = quantity;

    /* Decrease the item (from the pack) */
    if (item >= 0)
    {
        log_debug("do_cmd_wield: Before decrease - item=%d, k_idx=%d, name2=%d, number=%d", 
                  item, inventory[item].k_idx, inventory[item].name2, inventory[item].number);
        inven_item_increase(item, -quantity);
        inven_item_optimize(item);
        log_debug("do_cmd_wield: After optimize - item=%d, k_idx=%d", 
                  item, inventory[item].k_idx);
    }

    /* Decrease the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -quantity);
        floor_item_optimize(0 - item);
    }

    /* Get the wield slot */
    o_ptr = &inventory[slot];
    
    log_debug("do_cmd_wield: Wield slot %d - has k_idx=%d, name2=%d", 
              slot, o_ptr->k_idx, o_ptr->name2);

    /* Take off existing item */
    if (o_ptr->k_idx && !combine)
    {
        log_debug("do_cmd_wield: Taking off existing item from slot %d - k_idx=%d, name2=%d", 
                  slot, o_ptr->k_idx, o_ptr->name2);
        /* Take off existing item */
        (void)inven_takeoff(slot, 255);
        
        /* Refresh pointer after takeoff */
        o_ptr = &inventory[slot];
        log_debug("do_cmd_wield: After takeoff, slot %d now has k_idx=%d", 
                  slot, o_ptr->k_idx);
    }

    /* Deal with wielding of two-handed weapons when already using a shield */
    if ((k_info[i_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && (inventory[INVEN_ARM].k_idx))
    {
        /* Take off shield */
        check_pack_overflow();
        (void)inven_takeoff(INVEN_ARM, 255);
    }

    /* Deal with wielding of shield or second weapon when already wielding a two
     * handed weapon */
    if ((slot == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED)))
    {
        /* Stop wielding two handed weapon */
        (void)inven_takeoff(INVEN_WIELD, 255);
    }

    /* Combine the new stuff into the equipment */
    if (combine)
    {
        log_debug("do_cmd_wield: Combining - slot %d has k_idx=%d name2=%d, adding k_idx=%d name2=%d",
                  slot, o_ptr->k_idx, o_ptr->name2, i_ptr->k_idx, i_ptr->name2);
        msg_print(
            "You combine them with some that are already in your quiver.");
        object_absorb(o_ptr, i_ptr);
    }
    /* Wear the new stuff */
    else
    {
        log_debug("do_cmd_wield: Copying to slot %d - source k_idx=%d name2=%d",
                  slot, i_ptr->k_idx, i_ptr->name2);
        object_copy(o_ptr, i_ptr);
        log_debug("do_cmd_wield: After copy, slot %d now has k_idx=%d name2=%d",
                  slot, o_ptr->k_idx, o_ptr->name2);
    }

    /* Increment the equip counter by hand */
    if (!combine)
        p_ptr->equip_cnt++;

    /* Where is the item now */
    if ((slot == INVEN_WIELD)
        || ((slot == INVEN_ARM) && (o_ptr->tval != TV_SHIELD)))
    {
        act = "You are wielding";
    }
    else if (slot == INVEN_BOW)
    {
        act = "You are shooting with";
    }
    else if (slot == INVEN_LITE)
    {
        act = "Your light source is";
    }
    else if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
    {
        act = "In your quiver you have";
    }
    else
    {
        act = "You are wearing";
    }

    /* Describe the result */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Message */
    msg_format("%s %s (%c).", act, o_name, index_to_label(slot));

    /* Play equip sound */
    {
        int equip_sound = get_equip_sound(o_ptr);
        if (equip_sound >= 0)
            sound(equip_sound);
    }

    // Deal with wielding from the floor
    if (item < 0)
    {
        /* Forget monster */
        o_ptr->held_m_idx = 0;

        /* Forget location */
        o_ptr->iy = o_ptr->ix = 0;

        // Break the truce if picking up an item from the floor
        break_truce(false);

        // Special effects when picking up all the items from the floor
        if (i_ptr->number == original_quantity)
        {
            /* No longer marked */
            o_ptr->marked = false;
        }
    }

    /* Cursed! */
    if (cursed_p(o_ptr))
    {
        /* Warn the player */
        msg_print("You have a bad feeling about this...");

        /* Remove special inscription, if any */
        if (o_ptr->discount >= INSCRIP_NULL)
            o_ptr->discount = 0;

        /* Sense the object if allowed */
        if (o_ptr->discount == 0)
            o_ptr->discount = INSCRIP_CURSED;

        /* The object has been "sensed" */
        o_ptr->ident |= (IDENT_SENSE);
    }

    if (weapon_less_effective)
    {
        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[INVEN_WIELD], false, 0);

        /* Message */
        msg_format(
            "You are no longer able to wield your %s as effectively.", o_name);
    }

    ident_on_wield(o_ptr);

    // activate all of its new abilities
    for (i = 0; i < o_ptr->abilities; i++)
    {
        if (!p_ptr->have_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]])
        {
            p_ptr->have_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]]
                = true;
            p_ptr->active_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]]
                = true;
        }
    }

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Recalculate mana */
    p_ptr->update |= (PU_MANA);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST | PR_MAP | PR_QUIVER);

    /* Update light display when wielding a light source */
    if (slot == INVEN_LITE)
    {
        p_ptr->redraw |= (PR_LIGHT);
    }

    /* Force immediate sidebar update */
    handle_stuff();
}

/*
 * Take off an item
 */
void do_cmd_takeoff(object_type* default_o_ptr, int default_item)
{
    int item;

    object_type* o_ptr;

    cptr q, s;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
    }
    /* Get an item */
    else
    {
        q = "Remove which item? ";
        s = "You are not wearing anything to remove.";
        if (!get_item(&item, q, s, (USE_EQUIP)))
            return;

        /* Get the item (in the pack) */
        if (item >= 0)
        {
            o_ptr = &inventory[item];
        }

        /* Get the item (on the floor) */
        else
        {
            o_ptr = &o_list[0 - item];
        }
    }

    /* Item is cursed */
    if (cursed_p(o_ptr))
    {
        if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
        {
            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
        else
        {
            /* Oops */
            msg_print("You cannot bear to part with it.");

            /* Nope */
            return;
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get unequip sound before taking off (since o_ptr may be modified) */
    int unequip_sound = get_unequip_sound(o_ptr);

    /* Take off the item */
    (void)inven_takeoff(item, 255);

    /* Play unequip sound */
    if (unequip_sound >= 0)
        sound(unequip_sound);

    /* Deal with wielding of shield when already wielding a hand and a half
     * weapon
     */
    if ((item == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3
            & (TR3_HAND_AND_A_HALF)))
    {
        char o_name[80];

        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[INVEN_WIELD], false, 0);

        /* Message */
        msg_format("You can now wield your %s more effectively.", o_name);
    }

    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST | PR_MAP | PR_QUIVER);

    /* Update light display when removing a light source */
    if (item == INVEN_LITE)
    {
        p_ptr->redraw |= (PR_LIGHT);
    }

    /* Force immediate sidebar update */
    handle_stuff();
}

/*
 * Drop an item by index (for enhanced menus)
 */
void do_cmd_drop_item_by_index(int item)
{
    if (item == SUPPLIES_INDEX)
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_DROP, -1, false, true);
        return;
    }

    int amt;
    object_type* o_ptr;

    /* Paranoia */
    if (item < 0 || item >= INVEN_TOTAL)
        return;

    /* Get the item */
    o_ptr = &inventory[item];

    /* Nothing there */
    if (!o_ptr->k_idx)
        return;

    /* Get a quantity */
    amt = get_quantity(NULL, o_ptr->number);

    /* Allow user abort */
    if (amt <= 0)
        return;

    /* Hack -- Cannot remove cursed items */
    if ((item >= INVEN_WIELD) && cursed_p(o_ptr))
    {
        if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
        {
            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
        else
        {
            /* Oops */
            msg_print("You cannot bear to part with it.");

            /* Nope */
            return;
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 50;

    /* Drop (some of) the item */
    inven_drop(item, amt);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);

    enhanced_drop_refresh_pending = true;
}

/*
 * Drop an item
 */
void do_cmd_drop(void)
{
    int item, amt;

    object_type* o_ptr;

    cptr q, s;

    /* Get an item */
    q = "Drop which item? ";
    s = "You have nothing to drop.";
    if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN)))
        return;

    if (item == SUPPLIES_INDEX)
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_DROP, -1, false, true);
        return;
    }

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* Get a quantity */
    amt = get_quantity(NULL, o_ptr->number);

    /* Allow user abort */
    if (amt <= 0)
        return;

    /* Hack -- Cannot remove cursed items */
    if ((item >= INVEN_WIELD) && cursed_p(o_ptr))
    {
        if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
        {
            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
        else
        {
            /* Oops */
            msg_print("You cannot bear to part with it.");

            /* Nope */
            return;
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Drop (some of) the item */
    inven_drop(item, amt);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);
}

/*
 * An "item_tester_hook" for destroying objects
 */
static bool item_tester_hook_destroy(const object_type* o_ptr)
{
    if (o_ptr) { } // suppresses warnings about this function

    return (true);
}

/*
 *  Shatter the player's wielded weapon.
 */
void shatter_weapon(int silnum)
{
    int i;
    object_type* w_ptr = &inventory[INVEN_WIELD];
    char w_name[80];
    int anger_level;

    log_debug("shatter_weapon: called for silmaril #%d", silnum);
    
    /* Set the appropriate shatter flag for this silmaril */
    if (silnum == 2)
    {
        p_ptr->crown_shatter_sil2 = true;
        log_debug("shatter_weapon: set crown_shatter_sil2 = true");
    }
    else if (silnum == 3)
    {
        p_ptr->crown_shatter_sil3 = true;
        log_debug("shatter_weapon: set crown_shatter_sil3 = true");
    }

    /* Get the basic name of the object */
    object_desc(w_name, sizeof(w_name), w_ptr, false, 0);

    if (silnum == 2)
        msg_print(
            "You strive to free a second Silmaril, but it is not fated to be.");
    else
        msg_print(
            "You strive to free a third Silmaril, but it is not fated to be.");

    msg_format(
        "As you strike the crown, your %s shatters into innumerable pieces.",
        w_name);

    // make more noise
    stealth_score -= 5;

    inven_item_increase(INVEN_WIELD, -1);
    inven_item_optimize(INVEN_WIELD);

    /* Determine anger level based on which Silmaril (2nd = state 3, 3rd = state 4) */
    anger_level = (silnum == 2) ? 3 : 4;

    log_debug("shatter_weapon: anger_level=%d for silmaril #%d", anger_level, silnum);

    /* Process monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* If Morgoth, then anger him */
        if (m_ptr->r_idx == R_IDX_MORGOTH)
        {
            log_debug("shatter_weapon: found Morgoth at (%d,%d), cdis=%d, alertness=%d",
                     m_ptr->fy, m_ptr->fx, m_ptr->cdis, m_ptr->alertness);
            
            if ((m_ptr->cdis <= 5)
                && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
            {
                log_debug("shatter_weapon: Morgoth sees shard strike, calling anger_morgoth(%d)", 
                         anger_level);
                msg_print("A shard strikes Morgoth upon his cheek.");
                set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
                anger_morgoth(anger_level);
            }
            else
            {
                log_debug("shatter_weapon: Morgoth doesn't see/is too far");
            }
        }
    }
}

void prise_silmaril(void)
{
    object_type* o_ptr;
    object_type* w_ptr;
    artefact_type* a_ptr;

    object_type object_type_body;

    cptr freed_msg = NULL; // default to soothe compiler warnings

    bool freed = false;

    int slot = 0;

    int dam = 0;
    int prt = 0;
    int net_dam = 0;
    int prt_percent = 0;
    int hit_result = 0;
    int crit_bonus_dice = 0;
    int pd = 10;
    int noise = 0;
    u32b dummy_noticed_flag;

    int mds = p_ptr->mds;
    int attack_mod = p_ptr->skill_use[S_MEL];

    char o_name[80];

    // the Crown is on the ground
    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

    log_debug("prise_silmaril: attempting to prise silmaril from crown artifact %d", 
             o_ptr->name1);
    log_debug("prise_silmaril: current morgoth_state=%d, silmarils_possessed=%d",
             p_ptr->morgoth_state, silmarils_possessed());

    switch (o_ptr->name1)
    {
    case ART_MORGOTH_3:
    {
        noise = 5;
        freed_msg = "You have freed a Silmaril!";
        break;
    }
    case ART_MORGOTH_2:
    {
        noise = 10;

        if (p_ptr->crown_shatter)
            freed_msg = "The fates be damned! You free a second Silmaril.";
        else
            freed_msg = "You free a second Silmaril.";

        msg_print(
            "As you reach for the second jewel, you feel the weight of "
            "Morgoth's wrath pressing upon you.");
        msg_print(
            "To take another Silmaril will kindle a fury beyond measure.");
        if (!get_check("Will you dare to claim it? "))
            return;

        break;
    }
    case ART_MORGOTH_1:
    {
        noise = 15;

        freed_msg
            = "You free the final Silmaril. You have a very bad feeling about "
              "this.";

        msg_print(
            "Looking into the hallowed light of the final Silmaril, you are "
            "filled with a strange dread.");
        if (!get_check("Are you sure you wish to proceed? "))
            return;

        break;
    }
    }

    /* Get the weapon */
    w_ptr = &inventory[INVEN_WIELD];

    // undo rapid attack penalties
    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        // undo strength adjustment to the attack
        mds = total_mds(w_ptr, 0);

        // undo the dexterity adjustment to the attack
        attack_mod += 3;
    }

    /* Test for hit */
    hit_result = hit_roll(attack_mod, 0, PLAYER, NULL, true);

    /* Make some noise */
    stealth_score -= noise;

    // Determine damage
    if (hit_result > 0)
    {
        crit_bonus_dice = crit_bonus(hit_result, w_ptr->weight,
            &r_info[R_IDX_MORGOTH], S_MEL, false, NULL, w_ptr);

        dam = damroll(p_ptr->mdd + crit_bonus_dice, mds);
        prt = damroll(pd, 4);

        prt_percent = prt_after_sharpness(w_ptr, &dummy_noticed_flag);

        if (prt_percent < 0)
        {
            prt_percent = 0;
        }

        prt = (prt * prt_percent) / 100;
        net_dam = dam - prt;

        /* No negative damage */
        if (net_dam < 0)
            net_dam = 0;

        // update_combat_rolls1b(PLAYER, true);
        update_combat_rolls2(p_ptr->mdd + crit_bonus_dice, mds, dam, pd, 4, prt,
            prt_percent, GF_HURT, true);
    }

    // if you succeed in prising out a Silmaril...
    if (net_dam > 0)
    {
        freed = true;

        switch (o_ptr->name1)
        {
        case ART_MORGOTH_3:
        {
            /* Process monsters - anger Morgoth when 1st Silmaril is taken */
            for (int i = 1; i < mon_max; i++)
            {
                monster_type* m_ptr = &mon_list[i];

                /* If Morgoth, then anger him to state 2 for 1st Silmaril */
                if (m_ptr->r_idx == R_IDX_MORGOTH
                    && m_ptr->alertness >= ALERTNESS_ALERT)
                {
                    log_debug("prise_silmaril: found Morgoth at (%d,%d), cdis=%d",
                             m_ptr->fy, m_ptr->fx, m_ptr->cdis);
                    
                    if ((m_ptr->cdis <= 5)
                        && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
                    {
                        log_debug("prise_silmaril: Morgoth sees 1st silmaril taken, calling anger_morgoth(2)");
                        msg_print("Morgoth roars in fury!");
                        anger_morgoth(2);
                    }
                    else
                    {
                        log_debug("prise_silmaril: Morgoth alert but doesn't see/too far");
                    }
                }
            }
            break;
        }
        case ART_MORGOTH_2:
        {
            /* 50% chance to shatter if not already shattered on 2nd silmaril */
            if (!p_ptr->crown_shatter_sil2 && one_in_(2))
            {
                log_debug("prise_silmaril: 2nd silmaril shatter check failed (50%%), calling shatter_weapon(2)");
                shatter_weapon(2);
                freed = false;
            }
            else
            {
                log_debug("prise_silmaril: 2nd silmaril - no shatter (already_shattered=%d)", 
                         p_ptr->crown_shatter_sil2);
                
                /* Process monsters */
                for (int i = 1; i < mon_max; i++)
                {
                    monster_type* m_ptr = &mon_list[i];

                    /* If Morgoth, then anger him to state 3 for 2nd Silmaril */
                    if (m_ptr->r_idx == R_IDX_MORGOTH
                        && m_ptr->alertness >= ALERTNESS_ALERT)
                    {
                        log_debug("prise_silmaril: found Morgoth at (%d,%d), cdis=%d",
                                 m_ptr->fy, m_ptr->fx, m_ptr->cdis);
                        
                        if ((m_ptr->cdis <= 5)
                            && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
                        {
                            log_debug("prise_silmaril: Morgoth sees 2nd silmaril taken, calling anger_morgoth(3)");
                            msg_print("Morgoth howls with rage!");
                            anger_morgoth(3);
                        }
                        else
                        {
                            log_debug("prise_silmaril: Morgoth alert but doesn't see/too far");
                        }
                    }
                }
            }
            break;
        }
        case ART_MORGOTH_1:
        {
            /* 100% shatter on 3rd silmaril if not already shattered on 3rd */
            if (!p_ptr->crown_shatter_sil3)
            {
                log_debug("prise_silmaril: 3rd silmaril shatter check (100%%), calling shatter_weapon(3)");
                shatter_weapon(3);
                freed = false;
            }
            else
            {
                log_debug("prise_silmaril: 3rd silmaril - no shatter (already_shattered=%d), but cursed!",
                         p_ptr->crown_shatter_sil3);
                p_ptr->cursed = true;
            }
            break;
        }
        }

        if (freed)
        {
            // change its type to that of the crown with one less silmaril
            o_ptr->name1--;

            // get the details of this new crown
            a_ptr = &a_info[o_ptr->name1];

            // modify the existing crown
            object_into_artefact(o_ptr, a_ptr);

            // report success
            msg_print(freed_msg);

            // Get new local object
            o_ptr = &object_type_body;

            // Make Silmaril
            object_prep(o_ptr, lookup_kind(TV_LIGHT, SV_LIGHT_SILMARIL));

            // Get it
            slot = inven_carry(o_ptr, false);

            if (slot == SUPPLIES_INDEX)
            {
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                char label = supplies_label_char();
                if (!label)
                    label = 'a';
                msg_format("You add %s to your supplies (%c).", o_name, label);
            }
            else if (slot >= 0)
            {
                /* Get the object again */
                o_ptr = &inventory[slot];

                /* Describe the object */
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                /* Message */
                msg_format("You have %s (%c).", o_name, index_to_label(slot));
            }
            else
            {
                /* Inventory full - find best adjacent square for Silmaril */
                int dy, dx;
                int best_y = p_ptr->py;
                int best_x = p_ptr->px;
                int backup_y = -1;
                int backup_x = -1;
                bool found_ideal = false;
                bool found_backup = false;
                
                /* First pass: try to find square with no items AND no monsters */
                for (dy = -1; dy <= 1; dy++)
                {
                    for (dx = -1; dx <= 1; dx++)
                    {
                        int ty = p_ptr->py + dy;
                        int tx = p_ptr->px + dx;
                        
                        /* Skip center */
                        if (dy == 0 && dx == 0) continue;
                        
                        /* Check if square is valid, empty floor, no objects, no monsters */
                        if (in_bounds_fully(ty, tx) && 
                            cave_clean_bold(ty, tx) && 
                            cave_m_idx[ty][tx] == 0)
                        {
                            best_y = ty;
                            best_x = tx;
                            found_ideal = true;
                            break;
                        }
                        /* Backup: empty floor with no objects (but monster might be there) */
                        else if (!found_backup && in_bounds_fully(ty, tx) && cave_clean_bold(ty, tx))
                        {
                            backup_y = ty;
                            backup_x = tx;
                            found_backup = true;
                        }
                    }
                    if (found_ideal) break;
                }
                
                /* Use backup square if no ideal square found */
                if (!found_ideal && found_backup)
                {
                    best_y = backup_y;
                    best_x = backup_x;
                    log_debug("prise_silmaril: no monster-free square, using backup at (%d,%d)", best_y, best_x);
                }
                
                /* Drop the Silmaril */
                if (found_ideal)
                {
                    log_debug("prise_silmaril: inventory full, dropping Silmaril at (%d,%d) (no items, no monsters)", 
                             best_y, best_x);
                }
                else if (found_backup)
                {
                    log_debug("prise_silmaril: inventory full, dropping Silmaril at (%d,%d) (WARNING: monster may be present)", 
                             best_y, best_x);
                }
                else
                {
                    log_debug("prise_silmaril: inventory full, no adjacent empty square, using drop_near fallback");
                }
                
                drop_near(o_ptr, 0, best_y, best_x);
                
                /* Describe what we dropped */
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                msg_format("You have no room, so %s drops to the floor.", o_name);
            }

            // Break the truce (always)
            break_truce(true);

            // add a note to the notes file
            do_cmd_note("Cut a Silmaril from Morgoth's crown", p_ptr->depth);
        }
    }

    // if you fail to prise out a Silmaril...
    else
    {
        msg_print("Try though you might, you were unable to free a Silmaril.");

        // Break the truce if creatures see
        break_truce(false);
    }

    // check for taking of final Silmaril
    if (o_ptr->name1 == ART_MORGOTH_0)
    {
        log_debug("prise_silmaril: final silmaril taken! Calling anger_morgoth(4)");
        msg_print("You hear a cry of vengeance echo through the iron hells.");
        msg_print("You feel your doom awaiting you.");
        wake_all_monsters(0);
        anger_morgoth(4);  // Final Silmaril pushes Morgoth to desperate state
    }
    
    log_debug("prise_silmaril: complete, freed=%s, final morgoth_state=%d", 
             freed ? "true" : "false", p_ptr->morgoth_state);
}

/*
 * Destroy an item
 */
void do_cmd_destroy(void)
{
    int item, amt;
    int old_number;
    int old_charges = 0;

    object_type* o_ptr;

    char o_name[80];

    char out_val[160];

    cptr q, s;

    item_tester_hook = item_tester_hook_destroy;

    // Special case for prising Silmarils from the Iron Crown of Morgoth
    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
    if ((o_ptr->name1 >= ART_MORGOTH_1) && (o_ptr->name1 <= ART_MORGOTH_3))
    {
        // Select the melee weapon
        o_ptr = &inventory[INVEN_WIELD];

        // No weapon
        if (!o_ptr->k_idx)
        {
            msg_print(
                "To prise a Silmaril from the crown, you would need to wield a "
                "weapon.");
        }

        // Wielding a weapon
        else
        {
            if (get_check(
                    "Will you try to prise a Silmaril from the Iron Crown? "))
            {
                prise_silmaril();

                /* Take a turn */
                p_ptr->energy_use = 100;

                // store the action type
                p_ptr->previous_action[0] = ACTION_MISC;

                return;
            }
        }
    }

    /* Get an item */
    q = "Destroy which item? ";
    s = "You have nothing to destroy.";
    if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR)))
        return;

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    // Special case for Iron Crown of Morgoth, if it has Silmarils left
    if ((o_ptr->name1 >= ART_MORGOTH_1) && (o_ptr->name1 <= ART_MORGOTH_3))
    {
        if (item >= 0)
        {
            msg_print("You would have to put it down first.");
        }
        else
        {
            /* No weapon */
            if (!o_ptr->k_idx)
            {
                msg_print("To prise a Silmaril from the crown, you would need "
                          "to wield a "
                          "weapon.");
            }
            else
            {
                msg_print(
                    "You decide to try to prise out a Silmaril after all.");

                prise_silmaril();

                /* Take a turn */
                p_ptr->energy_use = 100;

                // store the action type
                p_ptr->previous_action[0] = ACTION_MISC;

                return;
            }
        }
    }

    /* Get a quantity */
    amt = get_quantity(NULL, o_ptr->number);

    /* Allow user abort */
    if (amt <= 0)
        return;

    /* Describe the object */
    old_number = o_ptr->number;

    /* Hack, state the correct number of charges to be destroyed if staff*/
    if ((o_ptr->tval == TV_STAFF) && (amt < o_ptr->number))
    {
        /*save the number of charges*/
        old_charges = o_ptr->pval;

        /*distribute the charges*/
        o_ptr->pval -= o_ptr->pval * amt / o_ptr->number;

        o_ptr->pval = old_charges - o_ptr->pval;
    }

    /*hack -  make sure we get the right amount displayed*/
    o_ptr->number = amt;

    /*now describe with correct amount*/
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /*reverse the hack*/
    o_ptr->number = old_number;

    /* Check for known special items */
    strnfmt(out_val, sizeof(out_val), "Really destroy %s? ", o_name);

    if (!get_check(out_val))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Message */
    msg_format("You destroy %s.", o_name);

    /*hack, restore the proper number of charges after the messages have printed
     * so the proper number of charges are destroyed*/
    if (old_charges)
        o_ptr->pval = old_charges;

    /* Eliminate the item (from the pack) */
    if (item >= 0)
    {
        inven_item_increase(item, -amt);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Eliminate the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -amt);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }
}

/*
 * Observe an item, displaying what is known about it
 */
void do_cmd_observe(void)
{
    /* Set up for enhanced menu cycling */
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Mark that 'x' command opened this menu */
    current_menu_command = 'x';
    current_menu_state = 0;  /* Start with inventory */
    
    /* Start the enhanced menu system */
    do_cmd_observe_enhanced();
}

/*
 * Enhanced observe command that supports cycling between inventory/equipment
 */
void do_cmd_observe_enhanced(void)
{
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Clear any previous menu actions at start of new session */
    extern int enhanced_menu_action;
    extern int enhanced_equip_action;
    enhanced_menu_action = 0;
    enhanced_equip_action = 0;
    
    log_trace("do_cmd_observe_enhanced: Starting enhanced observe cycle");
    
    /* Clear any active banner before starting enhanced menu cycle */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }
    
    /* Continue cycling until user escapes or performs an action */
    while (true)
    {
        log_trace("do_cmd_observe_enhanced: Loop iteration, current_menu_state=%d", current_menu_state);
        
        if (current_menu_state == 0) {
            log_trace("do_cmd_observe_enhanced: Displaying inventory");
            /* Display inventory */
            do_cmd_inven();
            
            /* Check if user wants to switch to equipment */
            extern int enhanced_menu_action;
            log_trace("do_cmd_observe_enhanced: After inventory, enhanced_menu_action=%d", enhanced_menu_action);
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                log_trace("do_cmd_observe_enhanced: Switching to equipment");
                current_menu_state = 1;
                enhanced_menu_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_menu_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_inven */
                log_trace("do_cmd_observe_enhanced: Examining item, exiting cycle");
                enhanced_menu_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                log_trace("do_cmd_observe_enhanced: Exiting cycle (inven action=%d)", enhanced_menu_action);
                break;
            }
        }
        else {
            /* Display equipment */
            log_trace("do_cmd_observe_enhanced: Displaying equipment, current_menu_state=%d", current_menu_state);
            do_cmd_equip();
            log_trace("do_cmd_observe_enhanced: Returned from equipment, current_menu_state=%d", current_menu_state);
            
            /* Check if user wants to switch to inventory */
            extern int enhanced_equip_action;
            log_trace("do_cmd_observe_enhanced: After equipment, enhanced_equip_action=%d", enhanced_equip_action);
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                log_trace("do_cmd_observe_enhanced: Switching to inventory");
                current_menu_state = 0;
                enhanced_equip_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_equip_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_equip */
                log_trace("do_cmd_observe_enhanced: Examining item, exiting cycle");
                enhanced_equip_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                log_trace("do_cmd_observe_enhanced: Exiting cycle (equip action=%d)", enhanced_equip_action);
                break;
            }
        }
    }
    
    /* Clear the command state */
    current_menu_command = 0;
    current_menu_state = 0;
}

/*
 * Helper function which actually removes the inscription
 */
void uninscribe(object_type* o_ptr)
{
    /* Remove the inscription */
    o_ptr->obj_note = 0;

    /*The object kind has an autoinscription*/
    // Sil-y: removed restriction to known items (through 'object_aware')
    if (!(k_info[o_ptr->k_idx].flags3 & (TR3_INSTA_ART))
        && (get_autoinscription_index(o_ptr->k_idx) != -1))
    {
        char tmp_val[160];
        char o_name2[80];

        /*make a fake object so we can give a proper message*/
        object_type* i_ptr;
        object_type object_type_body;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        /* Create the object */
        object_prep(i_ptr, o_ptr->k_idx);

        /*make it plural*/
        i_ptr->number = 2;

        /*now describe with correct amount*/
        object_desc(o_name2, sizeof(o_name2), i_ptr, false, 0);

        /* Prompt */
        strnfmt(tmp_val, sizeof(tmp_val),
            "Remove automatic inscription for %s? ", o_name2);

        /* Auto-Inscribe if they want that */
        if (get_check(tmp_val))
            obliterate_autoinscription(o_ptr->k_idx);
    }

    /* Message */
    msg_print("Inscription removed.");

    /* Combine the pack */
    p_ptr->notice |= (PN_COMBINE);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

/*
 * Remove the inscription from an object
 * XXX Mention item (when done)?
 */
void do_cmd_uninscribe(void)
{
    int item;

    object_type* o_ptr;

    cptr q, s;

    /* Get an item */
    q = "Un-inscribe which item? ";
    s = "You have nothing to un-inscribe.";
    if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN | USE_FLOOR)))
        return;

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* Nothing to remove */
    if (!o_ptr->obj_note)
    {
        msg_print("That item had no inscription to remove.");
        return;
    }

    // Do the work
    uninscribe(o_ptr);
}

/*
 * Inscribe an object with a comment
 */
void do_cmd_inscribe(void)
{
    int item;

    object_type* o_ptr;

    char o_name[80];

    char tmp[80];

    cptr q, s;

    /* Get an item */
    q = "Inscribe which item? ";
    s = "You have nothing to inscribe.";
    if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN | USE_FLOOR)))
        return;

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* Describe the activity */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Message */
    msg_format("Inscribing %s.", o_name);
    message_flush();

    /* Start with nothing */
    SDL_strlcpy(tmp, "", sizeof(tmp));

    /* Use old inscription */
    if (o_ptr->obj_note)
    {
        /* Start with the old inscription */
        strnfmt(tmp, sizeof(tmp), "%s", quark_str(o_ptr->obj_note));
    }

    /* Get a new inscription (possibly empty) */
    if (term_get_string("Inscription: ", tmp, sizeof(tmp)))
    {
        char tmp_val[160];
        char o_name2[80];

        /*make a fake object so we can give a proper message*/
        object_type* i_ptr;
        object_type object_type_body;

        // if given an empty inscription, then uninscribe instead
        if (strlen(tmp) == 0)
        {
            uninscribe(o_ptr);
            return;
        }

        /* Save the inscription */
        o_ptr->obj_note = quark_add(tmp);

        /* Add an autoinscription? */
        // Sil-y: removed restriction to known items (through 'object_aware')
        if (!(k_info[o_ptr->k_idx].flags3 & (TR3_INSTA_ART)))
        {
            /* Get local object */
            i_ptr = &object_type_body;

            /* Wipe the object */
            object_wipe(i_ptr);

            /* Create the object */
            object_prep(i_ptr, o_ptr->k_idx);

            /*make it plural*/
            i_ptr->number = 2;

            /*now describe with correct amount*/
            object_desc(o_name2, sizeof(o_name2), i_ptr, false, 0);

            /* Prompt */
            strnfmt(tmp_val, sizeof(tmp_val),
                "Automatically inscribe all %s with '%s'? ", o_name2, tmp);

            /* Auto-Inscribe if they want that */
            if (get_check(tmp_val))
                add_autoinscription(o_ptr->k_idx, tmp);
        }

        /* Combine the pack */
        p_ptr->notice |= (PN_COMBINE);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }
}

/*
 * An "item_tester_hook" for refueling lanterns
 */
static bool item_tester_refuel_lantern(const object_type* o_ptr)
{
    /* Flasks of oil are okay */
    if (o_ptr->tval == TV_FLASK)
        return (true);

    /* Non-empty lanterns are okay */
    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN)
        && (o_ptr->timeout > 0))
    {
        return (true);
    }

    /* Assume not okay */
    return (false);
}

/*
 * Refill the player's lamp (from the pack or floor)
 */
void do_cmd_refuel_lamp(object_type* default_o_ptr, int default_item)
{
    int item;

    object_type* o_ptr;
    object_type* j_ptr;

    cptr q, s;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = item_tester_refuel_lantern;

        /* Get an item */
        q = "Refill with which source of oil? ";
        s = "You have no sources of oil.";
        if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR)))
            return;

        /* Get the item (in the pack) */
        if (item >= 0)
        {
            o_ptr = &inventory[item];
        }

        /* Get the item (on the floor) */
        else
        {
            o_ptr = &o_list[0 - item];
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get the lantern */
    j_ptr = &inventory[INVEN_LITE];

    /* Refuel from a latern */
    if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        j_ptr->timeout += o_ptr->timeout;
    }
    /* Refuel from a flask */
    else
    {
        j_ptr->timeout += o_ptr->pval;
    }

    /* Message */
    msg_print("You fuel your lamp.");

    /* Comment */
    if (j_ptr->timeout >= FUEL_LAMP)
    {
        j_ptr->timeout = FUEL_LAMP;
        msg_print("Your lamp is full.");
    }

    /* Refilled from a latern */
    if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        /* Unstack if necessary */
        if (o_ptr->number > 1)
        {
            object_type* i_ptr;
            object_type object_type_body;

            /* Get local object */
            i_ptr = &object_type_body;

            /* Obtain a local object */
            object_copy(i_ptr, o_ptr);

            /* Modify quantity */
            i_ptr->number = 1;

            /* Remove fuel */
            i_ptr->timeout = 0;

            /* Unstack the used item */
            o_ptr->number--;

            /* Carry or drop */
            if (item >= 0)
            {
                item = inven_carry(i_ptr, false);
                if (item == SUPPLIES_INDEX)
                    item = -1;
            }
            else
                drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
        }

        /* Empty a single latern */
        else
        {
            /* No more fuel */
            o_ptr->timeout = 0;
        }

        /* Combine / Reorder the pack (later) */
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN);
    }

    /* Refilled from a flask */
    else
    {
        /* Decrease the item (from the pack) */
        if (item >= 0)
        {
            inven_item_increase(item, -1);
            inven_item_describe(item);
            inven_item_optimize(item);
        }

        /* Decrease the item (from the floor) */
        else
        {
            floor_item_increase(0 - item, -1);
            floor_item_describe(0 - item);
            floor_item_optimize(0 - item);
        }
    }

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP);

    // get another chance to identify the lamp
    ident_on_wield(j_ptr);

    p_ptr->redraw |= (PR_LIGHT);

    /* Force immediate sidebar update */
    handle_stuff();
}

/*
 * An "item_tester_hook" for refueling torches
 */
static bool item_tester_refuel_torch(const object_type* o_ptr)
{
    /* Torches are okay */
    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_TORCH))
        return (true);

    /* Assume not okay */
    return (false);
}

/*
 * An "item_tester_hook" for refueling torches
 */
static bool item_tester_refuel_mallorn(const object_type* o_ptr)
{
    /* Torches are okay */
    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_MALLORN))
        return (true);

    /* Assume not okay */
    return (false);
}

/*
 * Refuel the player's torch (from the pack or floor)
 */
void do_cmd_refuel_torch(
    object_type* default_o_ptr, int default_item, bool is_mallorn)
{
    int item;

    object_type* o_ptr;
    object_type* j_ptr;

    cptr q, s;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = is_mallorn ? item_tester_refuel_mallorn
                                      : item_tester_refuel_torch;

        /* Get an item */
        q = "Refuel with which torch? ";
        s = "You have no extra torches.";
        if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR)))
            return;

        /* Get the item (in the pack) */
        if (item >= 0)
        {
            o_ptr = &inventory[item];
        }

        /* Get the item (on the floor) */
        else
        {
            o_ptr = &o_list[0 - item];
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get the primary torch */
    j_ptr = &inventory[INVEN_LITE];
    
    log_debug("do_cmd_refuel_torch: BEFORE refuel - j_ptr (INVEN_LITE) k_idx=%d timeout=%d",
              j_ptr->k_idx, j_ptr->timeout);
    log_debug("do_cmd_refuel_torch: BEFORE refuel - o_ptr (item=%d) k_idx=%d timeout=%d",
              item, o_ptr->k_idx, o_ptr->timeout);

    /* Refuel */
    j_ptr->timeout += o_ptr->timeout + 5;
    
    log_debug("do_cmd_refuel_torch: AFTER refuel - j_ptr timeout=%d", j_ptr->timeout);

    /* Message */
    msg_print("You combine the torches.");

    /* Over-fuel message */
    int max_fuel = is_mallorn ? FUEL_MALLORN : FUEL_TORCH;
    if (j_ptr->timeout >= max_fuel)
    {
        j_ptr->timeout = max_fuel;
        msg_print("Your torch is fully fueled.");
    }

    /* Refuel message */
    else
    {
        msg_print("Your torch glows more brightly.");
    }

    /* Decrease the item (from the pack) */
    if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Decrease the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP);

    // get another chance to identify the torch
    ident_on_wield(j_ptr);

    p_ptr->redraw |= (PR_LIGHT);

    /* Force immediate sidebar update */
    handle_stuff();
}

/*
 * Refuel the player's lamp or torch
 */
void do_cmd_refuel(void)
{
    object_type* o_ptr;

    /* Get the light */
    o_ptr = &inventory[INVEN_LITE];

    /* It is nothing */
    if (o_ptr->tval != TV_LIGHT)
    {
        msg_print("You are not wielding a light.");
    }

    /* It's a lamp */
    else if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        do_cmd_refuel_lamp(NULL, 0);
    }

    /* It's a torch */
    else if (o_ptr->sval == SV_LIGHT_TORCH)
    {
        do_cmd_refuel_torch(NULL, 0, false);
    }

    /* It's a torch */
    else if (o_ptr->sval == SV_LIGHT_MALLORN)
    {
        do_cmd_refuel_torch(NULL, 0, true);
    }

    /* No torch to refuel */
    else
    {
        msg_print("Your light cannot be refueled.");
    }
}

/*
 * Target command
 */
void do_cmd_target(void)
{
    /* Target set */
    if (target_set_interactive(TARGET_KILL, 0))
    {
        msg_print("Target Selected.");
    }

    /* Target aborted */
    else
    {
        msg_print("Target Aborted.");
    }
}

/*
 * Calculate the bounding box of explored areas, detected monsters, and detected objects.
 * Returns true if any explored area or detected entity found, false otherwise
 * 
 * This function includes positions of monsters detected by items like the
 * Gem of Foes (which have MFLAG_MARK set) in the scrollable bounds, and
 * positions of marked objects (e.g., from Gem of Treasures / detection).
 */
static bool get_explored_bounds(int* min_y, int* max_y, int* min_x, int* max_x)
{
    int y, x, i;
    
    *min_x = p_ptr->cur_map_wid;
    *max_x = 0;
    *min_y = p_ptr->cur_map_hgt;
    *max_y = 0;

    /* Check explored grids */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Check if this grid has been seen */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                if (x < *min_x) *min_x = x;
                if (x > *max_x) *max_x = x;
                if (y < *min_y) *min_y = y;
                if (y > *max_y) *max_y = y;
            }
        }
    }

    /* Also include detected monsters (e.g., from Gem of Foes) */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        
        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;
        
        /* Check if monster is detected (MFLAG_MARK set by detection spells/items) */
        if (m_ptr->mflag & (MFLAG_MARK))
        {
            int my = m_ptr->fy;
            int mx = m_ptr->fx;
            
            if (mx < *min_x) *min_x = mx;
            if (mx > *max_x) *max_x = mx;
            if (my < *min_y) *min_y = my;
            if (my > *max_y) *max_y = my;
        }
    }

    /* Also include marked objects (e.g., from Gem of Treasures / object detection) */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        /* Only include marked (detected/memorized) objects */
        if (!o_ptr->marked)
            continue;

        int oy = o_ptr->iy;
        int ox = o_ptr->ix;
        if (!in_bounds_fully(oy, ox))
            continue;

        if (ox < *min_x) *min_x = ox;
        if (ox > *max_x) *max_x = ox;
        if (oy < *min_y) *min_y = oy;
        if (oy > *max_y) *max_y = oy;
    }

    /* Check if any explored area or detected entity was found */
    return (*min_x <= *max_x && *min_y <= *max_y);
}

/*
 * Look command
 */
void do_cmd_look(void)
{
    /* Use the new unified look system */
    do_cmd_unified_look();
}

/*
 * Unified look command - combines look, scroll, and view functionality
 */

static int unified_sidebar_object_group(const object_type* o_ptr)
{
    if (!o_ptr)
        return LOOK_GROUP_OTHER;

    if (artefact_p(o_ptr))
        return LOOK_GROUP_ARTIFACT;

    switch (o_ptr->tval)
    {
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOW:
    case TV_DIGGING:
    case TV_ARROW:
        return LOOK_GROUP_WEAPON;

    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return LOOK_GROUP_ARMOUR;

    case TV_RING:
    case TV_AMULET:
    case TV_HORN:
    case TV_STAFF:
        return LOOK_GROUP_JEWELRY;

    case TV_EASTER:
        return LOOK_GROUP_HERBS;

    case TV_POTION:
        return LOOK_GROUP_POTIONS;

    case TV_GEM:
        return LOOK_GROUP_GEMS;

    case TV_FOOD:
        if (o_ptr->sval < SV_FOOD_MIN_FOOD)
            return LOOK_GROUP_CONSUMABLE;
        break;
    }

    return LOOK_GROUP_OTHER;
}

static int unified_look_count_visible_entities(unified_look_state* state)
{
    int total_entities = 0;
    int i;

    if (state->show_monsters)
    {
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);

        for (i = 0; i < temp_n; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];

            if (!m_idx) continue;
            if (!mon_list[m_idx].ml) continue;

            total_entities++;
        }
    }

    if (state->show_objects)
    {
        int group_counts[LOOK_GROUP_COUNT] = {0};

        get_sorted_target_list(TARGET_LIST_OBJECT, 0);

        for (i = 0; i < temp_n; i++)
        {
            int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
            if (!o_idx)
                continue;

            object_type* o_ptr = &o_list[o_idx];

            /* Only count marked (memorized) objects (matches sidebar display) */
            if (!o_ptr->marked)
                continue;

            if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
                continue;

            int group = unified_sidebar_object_group(o_ptr);
            if (state->object_group_filter >= 0 && group != state->object_group_filter)
                continue;
            if (state->limit_objects_top_five && group_counts[group] >= 5)
                continue;

            group_counts[group]++;
            total_entities++;
        }
    }

    return total_entities;
}

static int unified_look_count_visible_objects_for_group(unified_look_state* state, int group_filter)
{
    int total_objects = 0;
    int i;

    if (!state)
        return 0;

    int group_counts[LOOK_GROUP_COUNT] = {0};

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    for (i = 0; i < temp_n; i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        if (!o_idx)
            continue;

        object_type* o_ptr = &o_list[o_idx];

        /* Only count marked (memorized) objects (matches sidebar display) */
        if (!o_ptr->marked)
            continue;

        if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
            continue;

        int group = unified_sidebar_object_group(o_ptr);
        if (group_filter >= 0 && group != group_filter)
            continue;

        if (state->limit_objects_top_five && group_counts[group] >= 5)
            continue;

        group_counts[group]++;
        total_objects++;
    }

    return total_objects;
}

void do_cmd_unified_look(void)
{
    unified_look_state state;
    int y, x;
    char query;
    bool done = false;
    bool need_redraw = true;
    int original_wy, original_wx; /* Store original viewport */
    
    /* Clear entry level banner when using look command */
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }
    
    /* Enable story font for unified look if the setting is on */
    bool use_story_font = story_look_enabled();
    if (use_story_font)
    {
        log_debug("do_cmd_unified_look: Enabling story font");
        sdl_story_font_enable();
    }
    
    log_trace("=== UNIFIED LOOK STARTED ===");
    
    /* Store original viewport */
    original_wy = p_ptr->wy;
    original_wx = p_ptr->wx;
    
    log_trace("Original viewport: (%d,%d)", original_wy, original_wx);
    
    /* Initialize state */
    state.cursor_y = p_ptr->py;
    state.cursor_x = p_ptr->px;
    state.selected_entity = -1;
    state.show_monsters = true;
    state.show_objects = true;
    state.object_group_filter = -1;
    state.limit_objects_top_five = false;
    state.display_mode = 0; /* 0 = manual, 1 = entity */
    state.highlighted_y = -1;
    state.highlighted_x = -1;
    state.highlighted_entity_type = 0; /* 0 = none, 1 = monster, 2 = object */
    state.in_sidebar_mode = false;
    state.look_mode = 0; /* 0 = normal unified look, 1 = L-style scrolling */
    state.current_square_entity = 0; /* 0 = monster, 1 = object */
    state.square_cycling_mode = false; /* Start in normal sidebar cycling mode */
    
    /* Track monster health at initial cursor position for left sidebar display */
    int initial_m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
    if (initial_m_idx > 0 && mon_list[initial_m_idx].ml)
    {
        /* Track this monster for health display */
        health_track(initial_m_idx);
    }
    else
    {
        /* Clear health tracking when not starting on a visible monster */
        health_track(0);
    }
    
    /* Process redraw flags to update health bar immediately */
    handle_stuff();
    
    /* Main interaction loop */
    while (!done)
    {
        bool screen_saved = false;
        
        if (need_redraw)
        {
            /* Save screen to preserve underlying display */
            screen_save();
            screen_saved = true;
            
            /* Show unified sidebar */
            show_unified_sidebar(&state);
            
            /* Track monster health at current cursor position for left sidebar display */
            /* This handles Tab cycling and any other cursor position updates */
            int cursor_m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
            if (cursor_m_idx > 0 && mon_list[cursor_m_idx].ml)
            {
                /* Track this monster for health display */
                health_track(cursor_m_idx);
            }
            else
            {
                /* Clear health tracking when cursor is not on a visible monster */
                health_track(0);
            }
            
            /* Process redraw flags to update health bar immediately */
            handle_stuff();
            
            /* Show cursor position info */
            y = state.cursor_y;
            x = state.cursor_x;
            
            /* Display entity name in left sidebar if cursor is on something */
            {
                char out_val[256];
                int cursor_m_idx = cave_m_idx[y][x];
                int cursor_o_idx = cave_o_idx[y][x];
                int feat = cave_feat[y][x];
                bool has_visible_monster = (cursor_m_idx > 0) && (mon_list[cursor_m_idx].ml);
                bool has_marked_object = (cursor_o_idx > 0) && (o_list[cursor_o_idx].marked);
                bool has_known_feature = false;
                cptr feature_name = NULL;
                
                /* Check for known/revealed features (traps, doors, stairs, shafts) */
                if (cave_info[y][x] & (CAVE_MARK))
                {
                    /* Traps */
                    if (feat >= FEAT_TRAP_HEAD && feat <= FEAT_TRAP_TAIL)
                    {
                        has_known_feature = true;
                        feature_name = f_name + f_info[feat].name;
                    }
                    /* Doors (closed, locked, jammed) */
                    else if (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
                    {
                        has_known_feature = true;
                        feature_name = f_name + f_info[feat].name;
                    }
                    /* Open door */
                    else if (feat == FEAT_OPEN)
                    {
                        has_known_feature = true;
                        feature_name = "open door";
                    }
                    /* Broken door */
                    else if (feat == FEAT_BROKEN)
                    {
                        has_known_feature = true;
                        feature_name = "broken door";
                    }
                    /* Stairs up */
                    else if (feat == FEAT_LESS)
                    {
                        has_known_feature = true;
                        feature_name = "up staircase";
                    }
                    /* Stairs down */
                    else if (feat == FEAT_MORE)
                    {
                        has_known_feature = true;
                        feature_name = "down staircase";
                    }
                    /* Shaft up */
                    else if (feat == FEAT_LESS_SHAFT)
                    {
                        has_known_feature = true;
                        feature_name = "up shaft";
                    }
                    /* Shaft down */
                    else if (feat == FEAT_MORE_SHAFT)
                    {
                        has_known_feature = true;
                        feature_name = "down shaft";
                    }
                }
                
                /* Priority: monster first, then object (only if marked), then feature */
                if (has_visible_monster)
                {
                    monster_type* m_ptr = &mon_list[cursor_m_idx];
                    char m_name[80];
                    
                    /* Get the monster name with indefinite article */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0x08);
                    
                    /* Display "You see <monster name>" in left sidebar */
                    strnfmt(out_val, sizeof(out_val), "You see %s.", m_name);
                    prt(out_val, 0, 0);
                }
                else if (has_marked_object)
                {
                    object_type* o_ptr = &o_list[cursor_o_idx];
                    char o_name[80];
                    
                    /* Get the object name with indefinite article */
                    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                    
                    /* Display "You see <object name>" in left sidebar */
                    strnfmt(out_val, sizeof(out_val), "You see %s.", o_name);
                    prt(out_val, 0, 0);
                }
                else if (has_known_feature)
                {
                    /* Display "You see <feature name>" in left sidebar */
                    strnfmt(out_val, sizeof(out_val), "You see %s.", feature_name);
                    prt(out_val, 0, 0);
                }
                else
                {
                    /* Display help text based on current mode */
                    if (state.look_mode == 0)
                    {
#ifdef STEAMDECK_SUPPORT
                        prt("[i/e]=Select [Space]=Exam [t]=Target [l]=Disp [m]=Monst [o]=ObjCat [T]=Top5 [s]=Pan [ESC]", 0, 0);
#else
                        prt("[Tab/q]=Select [Space]=Exam [t]=Target [l]=Disp [m]=Monst [o]=ObjCat [T]=Top5 [s]=Pan [ESC]", 0, 0);
#endif
                    }
                    else
                    {
#ifdef STEAMDECK_SUPPORT
                        prt("[i/e]=Select [Space]=Exam [t]=Target [l]=Disp [m]=Monst [o]=ObjCat [T]=Top5 [s]=Curs [ESC]", 0, 0);
#else
                        prt("[Tab/q]=Select [Space]=Exam [t]=Target [l]=Disp [m]=Monst [o]=ObjCat [T]=Top5 [s]=Curs [ESC]", 0, 0);
#endif
                    }
                }
            }
            
            /* Move cursor to position */
            move_cursor_relative(state.cursor_y, state.cursor_x);
            
            need_redraw = false;
        }
        
        /* Get input */
        query = inkey();
        log_trace("Unified look key input: '%c' (%d) [char: %c, isupper: %d]", 
                 query, (int)query, (query >= 32 && query <= 126) ? query : '?', 
                 (query >= 'A' && query <= 'Z') ? 1 : 0);
        
        /* Restore screen after input if we saved it */
        if (screen_saved)
        {
            screen_load();
        }
        
        /* Update health bar display after screen restore */
        handle_stuff();
        
        /* Analyze input */
        log_trace("Processing key: '%c' (%d), backtick is %d", query, (int)query, (int)'`');
        switch (query)
        {
            case 'T':
            {
                state.limit_objects_top_five = !state.limit_objects_top_five;
                log_trace("'T' key pressed - top five toggle now %d", state.limit_objects_top_five ? 1 : 0);

                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }

                handle_stuff();
                need_redraw = true;
                continue;
            }

            /* Handle capital letters - most are now ignored since we use arrows for scrolling */
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
            case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
            case 'O': case 'P': case 'R': case 'S': case 'U':
            case 'V': case 'W': case 'X': case 'Y': case 'Z':
            {
                /* Capital letters are now ignored - use 'l' to switch to panel scroll mode */
                log_trace("Capital letter ignored: '%c' (%d) - use 'l' to switch modes", query, (int)query);
                break;
            }
            
            case ESCAPE:
            case 'Q':
                /* Clear any highlighting before exit */
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                done = true;
                break;
                
            /* Common menu keys - exit unified look and let them be processed normally */
            case '/':            /* Identify symbol */
            case '?':            /* Help */
            case 's':
            {
                /* Switch between cursor mode and panel scrolling mode */
                state.look_mode = (state.look_mode + 1) % 2;
                log_trace("'s' key pressed - look mode changed to: %d", state.look_mode);
                
                /* Update help text based on mode */
                need_redraw = true;
                break;
            }
            
            case 'x':            /* Examine/Look - show description */
            {
                log_trace("EXAMINATION: 'x' key pressed for description");
                
                /* Disable story font for info screens */
                if (use_story_font)
                    sdl_story_font_disable();
                
                /* Same logic as Space/Enter for examination */
                log_trace("EXAMINATION: state.in_sidebar_mode=%d, state.selected_entity=%d", 
                         state.in_sidebar_mode, state.selected_entity);
                log_trace("EXAMINATION: state.highlighted_y=%d, state.highlighted_x=%d", 
                         state.highlighted_y, state.highlighted_x);
                
                if (state.in_sidebar_mode && state.selected_entity >= 0 && 
                    state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions met");
                    
                    int cursor_m_idx = cave_m_idx[state.highlighted_y][state.highlighted_x];
                    int cursor_o_idx = cave_o_idx[state.highlighted_y][state.highlighted_x];
                    
                    log_trace("EXAMINATION: At highlighted position (%d,%d) - m_idx=%d, o_idx=%d, entity_type=%d", 
                             state.highlighted_y, state.highlighted_x, cursor_m_idx, cursor_o_idx, state.highlighted_entity_type);
                    
                    /* Examine the entity based on what was highlighted in the sidebar */
                    /* Entity type: 1 = monster, 2 = object */
                    if (state.highlighted_entity_type == 1 && cursor_m_idx > 0)
                    {
                        /* Monster was highlighted - examine monster */
                        log_trace("EXAMINATION: Highlighted entity is monster, examining monster %d", cursor_m_idx);
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (m_ptr->ml)
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            screen_roff(m_ptr->r_idx, m_ptr);
                            
                            /* Wait for input */
                            inkey();
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else if (state.highlighted_entity_type == 2 && cursor_o_idx > 0)
                    {
                        /* Object was highlighted - examine object */
                        log_trace("EXAMINATION: Highlighted entity is object, examining object %d", cursor_o_idx);
                        /* Object examination */
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        log_trace("EXAMINATION: Showing object info screen");
                        /* Save screen */
                        screen_save();
                        /* Show object info, with comparison if applicable */
                        if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
                        {
                            int slot = wield_slot(o_ptr);
                            const object_type* compare_objects[2];
                            const char* compare_headings[2];
                            char selected_heading[32];
                            char equipped_heading[32];

                            strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
                            strnfmt(equipped_heading, sizeof(equipped_heading), "%s", mention_use(slot));

                            compare_objects[0] = o_ptr;
                            compare_headings[0] = selected_heading;

                            if (inventory[slot].k_idx)
                            {
                                compare_objects[1] = &inventory[slot];
                            }
                            else
                            {
                                compare_objects[1] = NULL;
                            }

                            compare_headings[1] = equipped_heading;

                            object_info_screen_multi(compare_objects, compare_headings, 2);
                        }
                        else
                        {
                            object_info_screen(o_ptr);
                        }

                        /* Restore screen */
                        screen_load();
                        log_trace("EXAMINATION: Object examination completed");
                    }
                    else if (cursor_m_idx > 0)
                    {
                        log_trace("EXAMINATION: Found monster, examining monster %d", cursor_m_idx);
                        /* Monster examination */
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (m_ptr->ml)
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            screen_roff(m_ptr->r_idx, m_ptr);
                            
                            /* Wait for input */
                            inkey();
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else
                    {
                        log_trace("EXAMINATION: No entities found at highlighted position");
                    }
                }
                else
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions NOT met - using cursor position examination");
                    /* Examine cursor position */
                    y = state.cursor_y;
                    x = state.cursor_x;
                    
                    int cursor_m_idx = cave_m_idx[y][x];
                    int cursor_o_idx = cave_o_idx[y][x];
                    bool has_visible_monster = (cursor_m_idx > 0) && (mon_list[cursor_m_idx].ml);
                    bool has_object = (cursor_o_idx > 0);
                    
                    log_trace("EXAMINATION: Cursor position (%d,%d) - has_visible_monster=%d, has_object=%d", 
                             y, x, has_visible_monster, has_object);
                    
                    /* Prioritize OBJECT first, then visible monster */
                    if (has_object)
                    {
                        log_trace("EXAMINATION: Examining object at cursor position");
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        screen_save();

                        if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
                        {
                            int slot = wield_slot(o_ptr);
                            const object_type* compare_objects[2];
                            const char* compare_headings[2];
                            char selected_heading[32];
                            char equipped_heading[32];

                            strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
                            strnfmt(equipped_heading, sizeof(equipped_heading), "%s", mention_use(slot));

                            compare_objects[0] = o_ptr;
                            compare_headings[0] = selected_heading;

                            if (inventory[slot].k_idx)
                            {
                                compare_objects[1] = &inventory[slot];
                            }
                            else
                            {
                                compare_objects[1] = NULL;
                            }

                            compare_headings[1] = equipped_heading;

                            object_info_screen_multi(compare_objects, compare_headings, 2);
                        }
                        else
                        {
                            object_info_screen(o_ptr);
                        }

                        screen_load();
                    }
                    else if (has_visible_monster)
                    {
                        log_trace("EXAMINATION: Examining visible monster at cursor position");
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        screen_save();
                        screen_roff(m_ptr->r_idx, m_ptr);
                        inkey();
                        screen_load();
                    }
                    else
                    {
                        log_trace("EXAMINATION: No visible entities at cursor position");
                    }
                }
                need_redraw = true;
                break;
            }
            
#ifndef STEAMDECK_SUPPORT
            case 'i':            /* Inventory */
            case 'e':            /* Equipment */  
#endif
            case '[':            /* View monsters */
            case ']':            /* View objects */
            case 'f':            /* Fire/Throw */
            case 'w':            /* Wield/Wear */
            case 'd':            /* Drop */
            case 'k':            /* Destroy */
            case 'r':            /* Read scroll */
            case 'u':            /* Use staff */
            case 'a':            /* Activate */
            case 'z':            /* Zap rod */
            case '.':            /* Run */
            case ',':            /* Stay */
            case '<':            /* Go up stairs */
            case '>':            /* Go down stairs */
            case 'g':            /* Get/Pickup */
            case 'c':            /* Close */
            case 'j':            /* Jam */
            case '+':            /* Alter */
            case '*':            /* Target */
            case '@':            /* Center map */
            case '(':            /* Dungeon history */
            case '|':            /* Screenshots */
            case '~':            /* Various things */
            case '!':            /* OS command */
                /* Clear any highlighting before exit */
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                done = true;
                /* Don't consume the key - let it be processed by the main game loop */
                Term_keypress(query);
                break;
                
            case '2':
            case '8':
            case '4':
            case '6':
            case '1':
            case '3':
            case '7':
            case '9':
            {
                /* Arrow key behavior depends on current mode */
                if (state.look_mode == 0)
                {
                    /* Mode 0: Normal unified look - manual cursor scrolling */
                    int dir = target_dir(query);
                    if (dir)
                    {
                        int new_cursor_y = state.cursor_y + ddy[dir];
                        int new_cursor_x = state.cursor_x + ddx[dir];
                        
                        /* Calculate explored bounds */
                        int min_y, max_y, min_x, max_x;
                        bool has_explored = get_explored_bounds(&min_y, &max_y, &min_x, &max_x);
                        
                        if (has_explored)
                        {
                            /* Constrain cursor to explored area */
                            if (new_cursor_y < min_y) new_cursor_y = min_y;
                            if (new_cursor_y > max_y) new_cursor_y = max_y;
                            if (new_cursor_x < min_x) new_cursor_x = min_x;
                            if (new_cursor_x > max_x) new_cursor_x = max_x;
                        }
                        else
                        {
                            /* No explored area, constrain to full map */
                            if (new_cursor_y < 0) new_cursor_y = 0;
                            if (new_cursor_y >= p_ptr->cur_map_hgt) new_cursor_y = p_ptr->cur_map_hgt - 1;
                            if (new_cursor_x < 0) new_cursor_x = 0;
                            if (new_cursor_x >= p_ptr->cur_map_wid) new_cursor_x = p_ptr->cur_map_wid - 1;
                        }
                        
                        state.cursor_y = new_cursor_y;
                        state.cursor_x = new_cursor_x;
                        
                        /* Handle viewport scrolling when cursor reaches screen edge */
                        if (!panel_contains(state.cursor_y, state.cursor_x))
                        {
                            /* Log viewport scrolling */
                            log_trace("Viewport scroll: cursor at (%d,%d), panel (%d,%d)", 
                                     state.cursor_y, state.cursor_x, p_ptr->wy, p_ptr->wx);
                            
                            /* Center the viewport on the cursor */
                            int new_wy = state.cursor_y - SCREEN_HGT / 2;
                            int new_wx = state.cursor_x - SCREEN_WID / 2;
                            
                            /* Constrain viewport to explored bounds if available */
                            if (has_explored)
                            {
                                int explored_min_wy = min_y;
                                int explored_max_wy = max_y - SCREEN_HGT + 1;
                                int explored_min_wx = min_x;
                                int explored_max_wx = max_x - SCREEN_WID + 1;
                                
                                /* Ensure min <= max */
                                if (explored_max_wy < explored_min_wy) explored_max_wy = explored_min_wy;
                                if (explored_max_wx < explored_min_wx) explored_max_wx = explored_min_wx;
                                
                                if (new_wy < explored_min_wy) new_wy = explored_min_wy;
                                if (new_wy > explored_max_wy) new_wy = explored_max_wy;
                                if (new_wx < explored_min_wx) new_wx = explored_min_wx;
                                if (new_wx > explored_max_wx) new_wx = explored_max_wx;
                            }
                            
                            /* Use proper panel management function */
                            if (modify_panel(new_wy, new_wx))
                            {
                                /* Handle viewport updates immediately */
                                handle_stuff();
                            }
                            
                            log_trace("New viewport: (%d,%d)", p_ptr->wy, p_ptr->wx);
                        }
                        
                        state.in_sidebar_mode = false;
                        state.selected_entity = -1;
                        state.square_cycling_mode = false;
                        state.current_square_entity = 0;
                        
                        /* Clear old highlighting */
                        if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                        {
                            highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                            state.highlighted_y = -1;
                            state.highlighted_x = -1;
                            state.highlighted_entity_type = 0;
                        }
                        
                        /* Track monster health at cursor position for left sidebar display */
                        int m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
                        if (m_idx > 0 && mon_list[m_idx].ml)
                        {
                            /* Track this monster for health display */
                            health_track(m_idx);
                        }
                        else
                        {
                            /* Clear health tracking when not on a visible monster */
                            health_track(0);
                        }
                        
                        /* Process redraw flags to update health bar immediately */
                        handle_stuff();
                        
                        need_redraw = true;
                    }
                }
                else
                {
                    /* Mode 1: Panel scrolling - arrows scroll whole panels like capital letters */
                    int dir = target_dir(query);
                    if (dir)
                    {
                        log_trace("Panel scrolling mode: key='%c', dir=%d", query, dir);
                        
                        int old_wy = p_ptr->wy;
                        int old_wx = p_ptr->wx;
                        
                        log_trace("Old viewport: (%d,%d)", old_wy, old_wx);
                        
                        /* Apply the motion by full panels */
                        int new_wy = p_ptr->wy + (ddy[dir] * PANEL_HGT);
                        int new_wx = p_ptr->wx + (ddx[dir] * PANEL_WID);
                        
                        /* Calculate explored bounds for viewport constraint */
                        int min_y, max_y, min_x, max_x;
                        int explored_min_wy, explored_max_wy;
                        int explored_min_wx, explored_max_wx;
                        
                        if (get_explored_bounds(&min_y, &max_y, &min_x, &max_x))
                        {
                            /* Calculate viewport bounds based on explored area */
                            explored_min_wy = min_y;
                            explored_max_wy = max_y - SCREEN_HGT + 1;
                            explored_min_wx = min_x;
                            explored_max_wx = max_x - SCREEN_WID + 1;
                            
                            /* Ensure min <= max */
                            if (explored_max_wy < explored_min_wy) explored_max_wy = explored_min_wy;
                            if (explored_max_wx < explored_min_wx) explored_max_wx = explored_min_wx;
                        }
                        else
                        {
                            /* No explored area, use full map */
                            explored_min_wy = 0;
                            explored_max_wy = p_ptr->cur_map_hgt - SCREEN_HGT;
                            explored_min_wx = 0;
                            explored_max_wx = p_ptr->cur_map_wid - SCREEN_WID;
                        }
                        
                        /* Constrain viewport to explored boundaries */
                        if (new_wy < explored_min_wy) new_wy = explored_min_wy;
                        if (new_wx < explored_min_wx) new_wx = explored_min_wx;
                        if (new_wy > explored_max_wy) new_wy = explored_max_wy;
                        if (new_wx > explored_max_wx) new_wx = explored_max_wx;
                        
                        /* Additional safety checks */
                        if (new_wy < 0) new_wy = 0;
                        if (new_wx < 0) new_wx = 0;
                            
                        log_trace("Constrained viewport: (%d,%d)", new_wy, new_wx);

                        /* Use proper panel management function */
                        if (modify_panel(new_wy, new_wx))
                        {
                            /* Update cursor to same relative position */
                            state.cursor_y = state.cursor_y + (p_ptr->wy - old_wy);
                            state.cursor_x = state.cursor_x + (p_ptr->wx - old_wx);
                            
                            /* Boundary check cursor */
                            if (state.cursor_y < 0) state.cursor_y = 0;
                            if (state.cursor_y >= p_ptr->cur_map_hgt) state.cursor_y = p_ptr->cur_map_hgt - 1;
                            if (state.cursor_x < 0) state.cursor_x = 0;
                            if (state.cursor_x >= p_ptr->cur_map_wid) state.cursor_x = p_ptr->cur_map_wid - 1;
                            
                            log_trace("New cursor: (%d,%d)", state.cursor_y, state.cursor_x);

                            /* Handle viewport updates immediately */
                            handle_stuff();
                            
                            /* Track monster health at cursor position for left sidebar display */
                            int m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
                            if (m_idx > 0 && mon_list[m_idx].ml)
                            {
                                /* Track this monster for health display */
                                health_track(m_idx);
                            }
                            else
                            {
                                /* Clear health tracking when not on a visible monster */
                                health_track(0);
                            }
                            
                            /* Process redraw flags to update health bar immediately */
                            handle_stuff();
                            
                            need_redraw = true;
                        }
                        else
                        {
                            log_trace("Viewport unchanged");
                        }
                    }
                }
                break;
            }
            
            case '\t': /* Tab key */
#ifdef STEAMDECK_SUPPORT
            case 'i': /* I key - forward cycling (Steam Deck) */
#endif
            {
                log_trace("Tab key pressed - cycling entities");
                
                /* Global sidebar cycling only - no square cycling */
                state.in_sidebar_mode = true;
                state.square_cycling_mode = false; /* Always disable square cycling */
                
                /* Count total VISIBLE entities using same logic as sidebar */
                int total_entities = unified_look_count_visible_entities(&state);
                
                log_trace("Total visible entities: %d", total_entities);
                
                if (total_entities > 0)
                {
                    /* Clear previous highlighting */
                    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                    {
                        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    }
                    
                    /* Advance selection */
                    int old_selection = state.selected_entity;
                    state.selected_entity++;
                    if (state.selected_entity >= total_entities)
                        state.selected_entity = 0;
                        
                    log_trace("Entity selection: %d -> %d", old_selection, state.selected_entity);
                }
                
                need_redraw = true;
                break;
            }
            
            case '`': /* Backtick key - reverse Tab cycling */
            case 'q': /* Q key - reverse Tab cycling */
#ifdef STEAMDECK_SUPPORT
            case 'e': /* E key - reverse Tab cycling (Steam Deck) */
#endif
            {
                log_trace("REVERSE CYCLING: Key handler reached - cycling entities backward");
                
                /* Global sidebar cycling only - no square cycling */
                state.in_sidebar_mode = true;
                state.square_cycling_mode = false; /* Always disable square cycling */
                
                /* Count total VISIBLE entities using same logic as sidebar */
                int total_entities = unified_look_count_visible_entities(&state);
                
                log_trace("Total visible entities: %d", total_entities);
                
                if (total_entities > 0)
                {
                    /* Clear previous highlighting */
                    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                    {
                        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    }
                    
                    /* Move backward in selection */
                    int old_selection = state.selected_entity;
                    state.selected_entity--;
                    if (state.selected_entity < 0)
                        state.selected_entity = total_entities - 1;
                        
                    log_trace("Entity selection (backward): %d -> %d", old_selection, state.selected_entity);
                }
                
                need_redraw = true;
                break;
            }
            
            case '\r': /* Enter key */
            case ' ':
            {
                log_trace("EXAMINATION: Enter/Space key pressed for examination");
                
                /* Disable story font for info screens */
                if (use_story_font)
                    sdl_story_font_disable();
                
                /* Examine current target */
                log_trace("EXAMINATION: state.in_sidebar_mode=%d, state.selected_entity=%d", 
                         state.in_sidebar_mode, state.selected_entity);
                log_trace("EXAMINATION: state.highlighted_y=%d, state.highlighted_x=%d", 
                         state.highlighted_y, state.highlighted_x);
                
                if (state.in_sidebar_mode && state.selected_entity >= 0 && 
                    state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions met");
                    
                    int cursor_m_idx = cave_m_idx[state.highlighted_y][state.highlighted_x];
                    int cursor_o_idx = cave_o_idx[state.highlighted_y][state.highlighted_x];
                    
                    log_trace("EXAMINATION: At highlighted position (%d,%d) - m_idx=%d, o_idx=%d, entity_type=%d", 
                             state.highlighted_y, state.highlighted_x, cursor_m_idx, cursor_o_idx, state.highlighted_entity_type);
                    
                    /* Examine the entity based on what was highlighted in the sidebar */
                    /* Entity type: 1 = monster, 2 = object */
                    if (state.highlighted_entity_type == 1 && cursor_m_idx > 0)
                    {
                        /* Monster was highlighted - examine monster */
                        log_trace("EXAMINATION: Highlighted entity is monster, examining monster %d", cursor_m_idx);
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (m_ptr->ml)
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            screen_roff(m_ptr->r_idx, m_ptr);
                            
                            /* Wait for input */
                            inkey();
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else if (state.highlighted_entity_type == 2 && cursor_o_idx > 0)
                    {
                        /* Object was highlighted - examine object */
                        log_trace("EXAMINATION: Highlighted entity is object, examining object %d", cursor_o_idx);
                        /* Object examination */
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        log_trace("EXAMINATION: Showing object info screen");
                        /* Save screen */
                        screen_save();
                        /* Show object info, with comparison if applicable */
                        if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
                        {
                            int slot = wield_slot(o_ptr);
                            const object_type* compare_objects[2];
                            const char* compare_headings[2];
                            char selected_heading[32];
                            char equipped_heading[32];

                            strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
                            strnfmt(equipped_heading, sizeof(equipped_heading), "%s", mention_use(slot));

                            compare_objects[0] = o_ptr;
                            compare_headings[0] = selected_heading;

                            if (inventory[slot].k_idx)
                            {
                                compare_objects[1] = &inventory[slot];
                            }
                            else
                            {
                                compare_objects[1] = NULL;
                            }

                            compare_headings[1] = equipped_heading;

                            object_info_screen_multi(compare_objects, compare_headings, 2);
                        }
                        else
                        {
                            object_info_screen(o_ptr);
                        }

                        /* Restore screen */
                        screen_load();
                        log_trace("EXAMINATION: Object examination completed");
                    }
                    else if (cursor_m_idx > 0)
                    {
                        log_trace("EXAMINATION: Found monster, examining monster %d", cursor_m_idx);
                        /* Monster examination */
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (m_ptr->ml)
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            screen_roff(m_ptr->r_idx, m_ptr);
                            
                            /* Wait for input */
                            inkey();
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else
                    {
                        log_trace("EXAMINATION: No entities found at highlighted position");
                    }
                }
                else
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions NOT met - using cursor position examination");
                    /* Examine cursor position */
                    y = state.cursor_y;
                    x = state.cursor_x;
                    
                    int cursor_m_idx = cave_m_idx[y][x];
                    int cursor_o_idx = cave_o_idx[y][x];
                    bool has_visible_monster = (cursor_m_idx > 0) && (mon_list[cursor_m_idx].ml);
                    bool has_object = (cursor_o_idx > 0);
                    
                    log_trace("EXAMINATION: Cursor position (%d,%d) - has_visible_monster=%d, has_object=%d", 
                             y, x, has_visible_monster, has_object);
                    
                    /* Prioritize OBJECT first, then visible monster */
                    if (has_object)
                    {
                        log_trace("EXAMINATION: Examining object at cursor position");
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        screen_save();

                        if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
                        {
                            int slot = wield_slot(o_ptr);
                            const object_type* compare_objects[2];
                            const char* compare_headings[2];
                            char selected_heading[32];
                            char equipped_heading[32];

                            strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
                            strnfmt(equipped_heading, sizeof(equipped_heading), "%s", mention_use(slot));

                            compare_objects[0] = o_ptr;
                            compare_headings[0] = selected_heading;

                            if (inventory[slot].k_idx)
                            {
                                compare_objects[1] = &inventory[slot];
                            }
                            else
                            {
                                compare_objects[1] = NULL;
                            }

                            compare_headings[1] = equipped_heading;

                            object_info_screen_multi(compare_objects, compare_headings, 2);
                        }
                        else
                        {
                            object_info_screen(o_ptr);
                        }

                        screen_load();
                    }
                    else if (has_visible_monster)
                    {
                        log_trace("EXAMINATION: Examining visible monster at cursor position");
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        screen_save();
                        screen_roff(m_ptr->r_idx, m_ptr);
                        inkey();
                        screen_load();
                    }
                    else
                    {
                        log_trace("EXAMINATION: No visible entities at cursor position");
                    }
                }
                
                /* Re-enable story font */
                if (use_story_font)
                    sdl_story_font_enable();
                
                need_redraw = true;
                break;
            }
            
            case 'm':
            {
                log_trace("'m' key pressed - cycling monster display");
                /* Cycle monsters: monsters -> nothing -> monsters */
                if (state.show_monsters)
                {
                    /* From showing monsters to hiding monsters */
                    state.show_monsters = false;
                    log_trace("Mode changed to: monsters hidden");
                }
                else
                {
                    /* From hiding monsters to showing monsters */
                    state.show_monsters = true;
                    log_trace("Mode changed to: monsters shown");
                }
                
                /* Reset selection when changing display */
                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                
                /* Force a complete redraw */
                handle_stuff();
                need_redraw = true;
                log_trace("'m' key: set need_redraw=true, continuing to redraw immediately");
                /* Continue to top of loop to process redraw immediately */
                continue;
            }
            
            case 'o':
            {
                log_trace("'o' key pressed - cycling object categories");

                /* Cycle: all -> weapons -> armour -> artifacts -> herbs -> potions -> gems -> consumables -> other -> hidden */
                static const int object_filter_cycle[] = {
                    LOOK_GROUP_ARTIFACT,
                    LOOK_GROUP_WEAPON,
                    LOOK_GROUP_ARMOUR,
                    LOOK_GROUP_JEWELRY,
                    LOOK_GROUP_HERBS,
                    LOOK_GROUP_POTIONS,
                    LOOK_GROUP_GEMS,
                    LOOK_GROUP_CONSUMABLE,
                    LOOK_GROUP_OTHER,
                };

                if (!state.show_objects)
                {
                    state.show_objects = true;
                    state.object_group_filter = -1;
                    log_trace("Object display: shown (ALL)");
                }
                else if (state.object_group_filter < 0)
                {
                    /* Skip empty categories */
                    int next_group = -1;
                    for (size_t idx = 0; idx < N_ELEMENTS(object_filter_cycle); ++idx)
                    {
                        int group = object_filter_cycle[idx];
                        if (unified_look_count_visible_objects_for_group(&state, group) > 0)
                        {
                            next_group = group;
                            break;
                        }
                    }

                    if (next_group >= 0)
                    {
                        state.object_group_filter = next_group;
                        log_trace("Object display: filtered (group=%d)", state.object_group_filter);
                    }
                    else
                    {
                        state.show_objects = false;
                        state.object_group_filter = -1;
                        log_trace("Object display: hidden (no non-empty categories)");
                    }
                }
                else
                {
                    int next_group = -1;
                    for (size_t idx = 0; idx < N_ELEMENTS(object_filter_cycle); ++idx)
                    {
                        if (object_filter_cycle[idx] != state.object_group_filter)
                            continue;

                        /* Skip empty categories */
                        for (size_t j = idx + 1; j < N_ELEMENTS(object_filter_cycle); ++j)
                        {
                            int group = object_filter_cycle[j];
                            if (unified_look_count_visible_objects_for_group(&state, group) > 0)
                            {
                                next_group = group;
                                break;
                            }
                        }
                        break;
                    }

                    if (next_group >= 0)
                    {
                        state.object_group_filter = next_group;
                        log_trace("Object display: filtered (group=%d)", state.object_group_filter);
                    }
                    else
                    {
                        state.show_objects = false;
                        state.object_group_filter = -1;
                        log_trace("Object display: hidden");
                    }
                }
                
                /* Reset selection when changing display */
                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                
                /* Force a complete redraw */
                handle_stuff();
                need_redraw = true;
                log_trace("'o' key: set need_redraw=true, continuing to redraw immediately");
                /* Continue to top of loop to process redraw immediately */
                continue;
            }
            
            case 'l':
            {
                log_trace("'l' key pressed - cycling through display modes");
                /* Cycle display modes: monsters+objects -> objects -> nothing -> monsters+objects */
                if (state.show_monsters && state.show_objects)
                {
                    /* From both to objects only */
                    state.show_monsters = false;
                    state.show_objects = true;
                    log_trace("Mode changed to: objects only");
                }
                else if (!state.show_monsters && state.show_objects)
                {
                    /* From objects only to nothing */
                    state.show_monsters = false;
                    state.show_objects = false;
                    log_trace("Mode changed to: nothing (all hidden)");
                }
                else
                {
                    /* From nothing (or monsters only) to both */
                    state.show_monsters = true;
                    state.show_objects = true;
                    log_trace("Mode changed to: both monsters and objects");
                }
                
                /* Reset selection when changing display */
                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                
                /* Force a complete redraw */
                handle_stuff();
                need_redraw = true;
                log_trace("'l' key: set need_redraw=true, continuing to redraw immediately");
                /* Continue to top of loop to process redraw immediately */
                continue;
            }
            
            case 't':
            {
                /* Target monster at cursor position or selected position */
                int target_y = state.cursor_y;
                int target_x = state.cursor_x;
                
                /* Use highlighted position if in sidebar mode */
                if (state.in_sidebar_mode && state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    target_y = state.highlighted_y;
                    target_x = state.highlighted_x;
                }
                
                int m_idx = cave_m_idx[target_y][target_x];
                if (m_idx > 0)
                {
                    /* Set target to the monster */
                    target_set_monster(m_idx);
                    
                    /* Get monster description for message */
                    char m_name[80];
                    monster_desc(m_name, sizeof(m_name), &mon_list[m_idx], 0x80);
                    msg_format("Target set to %s.", m_name);
                    
                    /* Exit unified look after targeting */
                    done = true;
                }
                else
                {
                    msg_print("No monster at cursor position.");
                }
                break;
            }
            
            case 'p':
            {
                /* Return to player position */
                state.cursor_y = p_ptr->py;
                state.cursor_x = p_ptr->px;
                state.in_sidebar_mode = false;
                state.selected_entity = -1;
                need_redraw = true;
                break;
            }
            
            default:
            {
                /* Unhandled key - exit like ESC */
                log_trace("Unhandled key in unified look: '%c' (%d) - exiting", query, (int)query);
                /* Clear any highlighting before exit */
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                    state.highlighted_entity_type = 0;
                }
                done = true;
                break;
            }
        }
    }
    
    /* Clear any highlighting */
    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
    {
        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
        state.highlighted_y = -1;
        state.highlighted_x = -1;
        state.highlighted_entity_type = 0;
    }
    
    log_trace("=== UNIFIED LOOK ENDED ===");
    
    /* Clear health tracking before exiting look command */
    health_track(0);
    
    /* Disable story font if it was enabled */
    if (use_story_font)
    {
        log_debug("do_cmd_unified_look: Disabling story font");
        sdl_story_font_disable();
    }
    
    /* Restore original viewport */
    if (p_ptr->wy != original_wy || p_ptr->wx != original_wx)
    {
        p_ptr->wy = original_wy;
        p_ptr->wx = original_wx;
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_OVERHEAD);
        handle_stuff();
    }
}

/*
 * Highlight an entity on the map 
 */
void highlight_entity_on_map(int y, int x, bool highlight)
{
    highlight_entity_on_map_type(y, x, highlight, 0); /* Default: auto-detect */
}

void highlight_entity_on_map_type(int y, int x, bool highlight, int entity_type)
{
    if (highlight)
    {
        /* Get the original character and color, but show with blue background */
        char display_char;
        byte display_attr;
        
        /* Determine what to display based on entity_type preference */
        /* entity_type: 0=auto-detect, 1=prefer monster, 2=prefer object */
        
        if (entity_type == 2 && cave_o_idx[y][x] > 0)
        {
            /* Prefer object display */
            object_type* o_ptr = &o_list[cave_o_idx[y][x]];
            display_char = object_char(o_ptr);
            display_attr = object_attr(o_ptr); /* Keep original object color */
            log_trace("Highlighting object '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else if (entity_type == 1 && cave_m_idx[y][x] > 0)
        {
            /* Prefer monster display */
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            display_char = monster_char(r_ptr);
            display_attr = monster_attr(r_ptr); /* Keep original monster color */
            log_trace("Highlighting monster '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else if (cave_m_idx[y][x] > 0)
        {
            /* Auto-detect: For monsters, show normal appearance (no color change) */
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            display_char = monster_char(r_ptr);
            display_attr = monster_attr(r_ptr); /* Keep original monster color */
            log_trace("Highlighting monster '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else if (cave_o_idx[y][x] > 0)
        {
            /* Auto-detect: For objects, show normal appearance (no color change) */
            object_type* o_ptr = &o_list[cave_o_idx[y][x]];
            display_char = object_char(o_ptr);
            display_attr = object_attr(o_ptr); /* Keep original object color */
            log_trace("Highlighting object '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else
        {
            /* Empty space - use a cursor */
            display_char = '+';
            display_attr = TERM_L_BLUE;
            log_trace("Highlighting empty space at (%d,%d) -> showing blue cursor", y, x);
        }
        
        /* Draw highlighted character */
        print_rel(display_char, display_attr, y, x);
        log_trace("Applied blue highlighting: char='%c', attr=%d", 
                 display_char, display_attr);
    }
    else
    {
        /* Restore original display */
        lite_spot(y, x);
        log_trace("Restored original display at (%d,%d)", y, x);
    }
}

/*
 * Allow the player to examine other sectors on the map
 */
void do_cmd_locate(void)
{
    int dir, y1, x1, y2, x2;
    int min_y, max_y, min_x, max_x;
    int explored_min_wy, explored_max_wy;
    int explored_min_wx, explored_max_wx;

    /* Clear entry level banner when using L command */
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Calculate explored bounds */
    if (get_explored_bounds(&min_y, &max_y, &min_x, &max_x))
    {
        /* Calculate viewport bounds based on explored area */
        explored_min_wy = min_y;
        explored_max_wy = max_y - SCREEN_HGT + 1;
        explored_min_wx = min_x;
        explored_max_wx = max_x - SCREEN_WID + 1;
        
        /* Ensure min <= max */
        if (explored_max_wy < explored_min_wy) explored_max_wy = explored_min_wy;
        if (explored_max_wx < explored_min_wx) explored_max_wx = explored_min_wx;
    }
    else
    {
        /* No explored area, use full map */
        explored_min_wy = 0;
        explored_max_wy = p_ptr->cur_map_hgt - SCREEN_HGT;
        explored_min_wx = 0;
        explored_max_wx = p_ptr->cur_map_wid - SCREEN_WID;
    }

    /* Start at current panel */
    y2 = y1 = p_ptr->wy;
    x2 = x1 = p_ptr->wx;

    /* Show panels until done */
    while (true)
    {
        /* Assume no direction */
        dir = 0;

        /* Get a direction */
        while (!dir)
        {
            char command;

            /* Get a command (or Cancel) */
            if (!get_com("Shift viewpoint in which direction? ", &command))
                break;

            /* Extract direction */
            dir = target_dir(command);

            /* Error */
            if (!dir)
                bell("Illegal direction for look (around dungeon)!");
        }

        /* No direction */
        if (!dir)
            break;

        /* Apply the motion */
        y2 += (ddy[dir] * PANEL_HGT);
        x2 += (ddx[dir] * PANEL_WID);

        /* Constrain to explored bounds */
        if (y2 > explored_max_wy)
            y2 = explored_max_wy;
        if (y2 < explored_min_wy)
            y2 = explored_min_wy;

        if (x2 > explored_max_wx)
            x2 = explored_max_wx;
        if (x2 < explored_min_wx)
            x2 = explored_min_wx;

        /* Handle "changes" */
        if ((p_ptr->wy != y2) || (p_ptr->wx != x2))
        {
            /* Update panel */
            p_ptr->wy = y2;
            p_ptr->wx = x2;

            /* Redraw map */
            p_ptr->redraw |= (PR_MAP);

            /* Window stuff */
            p_ptr->window |= (PW_OVERHEAD);

            /* Handle stuff */
            handle_stuff();
        }
    }

    /* Verify panel */
    p_ptr->update |= (PU_PANEL);

    /* Handle stuff */
    handle_stuff();
}

/*
 * The table of "symbol info" -- each entry is a string of the form
 * "X:desc" where "X" is the trigger, and "desc" is the "info".
 */
static cptr ident_info[]
    = { " :A dark grid", "!:A potion (or oil)", "\":An amulet", "#:A wall",
          /* "$:unused", */
          "%:A quartz vein", "&:A plant", "':An open door", "(:Soft armour",
          "):A shield", "*:A gem (or unseen monster)", "+:A closed door",
          ",:Food", "-:Arrows", ".:Floor", "/:An axe or polearm", "0:A forge",
          /* "1:unused", */
          /* "2:unused", */
          /* "3:unused", */
          /* "4:unused", */
          /* "5:unused", */
          /* "6:unused", */
          /* "7:unused", */
          /* "8:unused", */
          /* "9:unused", */
          "::Rubble", ";:A glyph of warding", "<:A staircase up", "=:A ring",
          ">:A staircase down", "?:An instrument", "@:Elf, Dwarf, or Man",
          /* "A:unused", */
          /* "B:unused", */
          "C:Canine", "D:Dragon",
          /* "E:unused", */
          /* "F:unused", */
          "G:Giant", "H:Horror", "I:Insect",
          /* "J:unused", */
          /* "K:unused", */
          /* "L:unused", */
          "M:Spider", "N:Nameless Thing",
          /* "O:unused", */
          "P:Giant",
          /* "Q:unused", */
          "R:Rauko", "S:Ancient Serpent", "T:Troll",
          /* "U:unused", */
          "V:Valar", "W:Wight/Wraith",
          /* "X:unused", */
          /* "Y:unused", */
          /* "Z:unused", */
          "[:Mail", "\\:A blunt weapon (or digger)", "]:Misc. armour",
          "^:A trap", "_:A staff",
          /* "`:unused", */
          /* "a:unused", */
          "b:Bat/Bird",
          /* "c:unused", */
          "d:Dragon",
          /* "e:unused", */
          "f:Feline",
          /* "g:unused", */
          /* "h:unused", */
          /* "i:unused", */
          /* "j:unused", */
          /* "k:unused", */
          /* "l:unused", */
          "m:Young Spider",
          /* "n:unused", */
          "o:Orc",
          /* "p:unused", */
          /* "q:unused", */
          /* "r:unused", */
          "s:Serpent",
          /* "t:unused", */
          /* "u:unused", */
          "v:Vampire", "w:Creeping Shadow",
          /* "x:unused", */
          /* "y:unused", */
          /* "z:unused", */
          /* "{:unused", */
          "|:An edged weapon (sword/dagger/etc)", "}:A bow",
          "~:A tool (or miscellaneous item)", NULL };

/*
 * Sorting hook -- Comp function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform on "u".
 */
bool ang_sort_comp_hook(const void* u, const void* v, int a, int b)
{
    u16b* who = (u16b*)(u);

    u16b* why = (u16b*)(v);

    int w1 = who[a];
    int w2 = who[b];

    int z1, z2;

    /* Sort by player kills */
    if (*why >= 4)
    {
        /* Extract player kills */
        z1 = l_list[w1].pkills;
        z2 = l_list[w2].pkills;

        /* Compare player kills */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by total kills */
    if (*why >= 3)
    {
        /* Extract total kills */
        z1 = l_list[w1].tkills;
        z2 = l_list[w2].tkills;

        /* Compare total kills */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by monster level */
    if (*why >= 2)
    {
        /* Extract levels */
        z1 = r_info[w1].level;
        z2 = r_info[w2].level;

        /* Compare levels */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by monster depth */
    if (*why >= 1)
    {
        /* Extract experience */
        z1 = r_info[w1].level;
        z2 = r_info[w2].level;

        /* Compare experience */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Compare indexes */
    return (w1 <= w2);
}

/*
 * Sorting hook -- Swap function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform.
 */
void ang_sort_swap_hook(void* u, void* v, int a, int b)
{
    u16b* who = (u16b*)(u);

    u16b holder;

    /* Unused parameter */
    (void)v;

    /* Swap */
    holder = who[a];
    who[a] = who[b];
    who[b] = holder;
}

/*
 * Identify a character, allow recall of monsters
 *
 * Several "special" responses recall "multiple" monsters:
 *   ^A (all monsters)
 *   ^U (all unique monsters)
 *   ^N (all non-unique monsters)
 *
 * The responses may be sorted in several ways, see below.
 *
 *
 */
void do_cmd_query_symbol(void)
{
    int i, n, r_idx;
    char sym, query;
    char buf[128];

    bool all = false;
    bool uniq = false;
    bool norm = false;

    bool recall = false;

    u16b why = 0;
    u16b* who;

    /* Get a character, or abort */
    if (!get_com("Enter character to be identified: ", &sym))
        return;

    /* Find that character info, and describe it */
    for (i = 0; ident_info[i]; ++i)
    {
        if (sym == ident_info[i][0])
            break;
    }

    /* Describe */
    if (sym == KTRL('A'))
    {
        all = true;
        SDL_strlcpy(buf, "Full monster list.", sizeof(buf));
    }
    else if (sym == KTRL('U'))
    {
        all = uniq = true;
        SDL_strlcpy(buf, "Unique monster list.", sizeof(buf));
    }
    else if (sym == KTRL('N'))
    {
        all = norm = true;
        SDL_strlcpy(buf, "Non-unique monster list.", sizeof(buf));
    }
    else if (ident_info[i])
    {
        strnfmt(buf, sizeof(buf), "%c - %s.", sym, ident_info[i] + 2);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "%c - %s.", sym, "Unknown Symbol");
    }

    /* Display the result */
    prt(buf, 0, 0);

    /* Allocate the "who" array */
    who = mem_alloc_array(z_info->r_max, u16b);

    /* Collect matching monsters */
    for (n = 0, i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Nothing to recall */
        if (!cheat_know && !l_ptr->tsights && !know_monster_info)
            continue;

        /* Require non-unique monsters if needed */
        if (norm && (r_ptr->flags1 & (RF1_UNIQUE)))
            continue;

        /* Require unique monsters if needed */
        if (uniq && !(r_ptr->flags1 & (RF1_UNIQUE)))
            continue;

        // Ignore monsters that can't be generated
        if (r_ptr->level > 25)
            continue;

        /* Collect "appropriate" monsters */
        if (all || (r_ptr->d_char == sym))
            who[n++] = i;
    }

    /* Nothing to recall */
    if (!n)
    {
        /* XXX XXX Free the "who" array */
        who = mem_free(who);

        return;
    }

    /* Prompt */
    put_str("Recall details? (k/p/y/n): ", 0, 40);

    /* Query */
    query = inkey();

    /* Restore */
    prt(buf, 0, 0);

    /* Sort by kills (and level) */
    if (query == 'k')
    {
        why = 4;
        query = 'y';
    }

    /* Sort by level */
    if (query == 'p')
    {
        why = 2;
        query = 'y';
    }

    /* Catch "escape" */
    if (query != 'y')
    {
        /* XXX XXX Free the "who" array */
        who = mem_free(who);

        return;
    }

    /* Sort if needed */
    if (why)
    {
        /* Select the sort method */
        ang_sort_comp = ang_sort_comp_hook;
        ang_sort_swap = ang_sort_swap_hook;

        /* Sort the array */
        ang_sort(who, &why, n);
    }

    /* Start at the end */
    i = n - 1;

    /* Scan the monster memory */
    while (1)
    {
        /* Extract a race */
        r_idx = who[i];

        /* Hack -- Auto-recall */
        monster_race_track(r_idx);

        /* Hack -- Handle stuff */
        handle_stuff();

        /* Hack -- Begin the prompt */
        roff_top(r_idx);

        /* Hack -- Complete the prompt */
        Term_addstr(-1, TERM_WHITE, " [(r)ecall, ESC]");

        /* Interact */
        while (1)
        {
            /* Recall (raging players don't get recall) */
            if (recall)
            {
                /* Save screen */
                screen_save();

                /* Recall on screen */
                screen_roff(who[i], NULL);

                /* Hack -- Complete the prompt (again) */
                Term_addstr(-1, TERM_WHITE, " [(r)ecall, ESC]");
            }

            /* Command */
            query = inkey();

            /* Unrecall */
            if (recall)
            {
                /* Load screen */
                screen_load();
            }

            /* Normal commands */
            if (query != 'r')
                break;

            /* Toggle recall */
            recall = !recall;
        }

        /* Stop scanning */
        if (query == ESCAPE)
            break;

        /* Move to "prev" monster */
        if (query == '-')
        {
            if (++i == n)
            {
                i = 0;
            }
        }

        /* Move to "next" monster */
        else
        {
            if (i-- == 0)
            {
                i = n - 1;
            }
        }
    }

    /* Re-display the identity */
    prt(buf, 0, 0);

    /* Free the "who" array */
    who = mem_free(who);
}
