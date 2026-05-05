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

static void prise_silmaril(void);

/*
 * Helper function to determine the equip sound based on item type
 */
static int get_equip_sound(const object_type* o_ptr)
{
    /* Fuel-burning light sources */
    if (o_ptr->tval == TV_LIGHT)
    {
        if (((o_ptr->sval == SV_LIGHT_TORCH)
                || (o_ptr->sval == SV_LIGHT_MALLORN))
            && player_light_has_fuel(o_ptr))
        {
            return MSG_TORCH_LIGHT;
        }

        if ((o_ptr->sval == SV_LIGHT_LANTERN)
            && (object_ego_prefix(o_ptr) != EGO_BROKEN_BRASS_LANTERN)
            && player_light_has_fuel(o_ptr))
        {
            return MSG_TORCH_LIGHT;
        }
    }

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

static bool smith_oath_takeoff_hits_pack(const object_type* o_ptr, int source_item)
{
    if (!smith_oath_forbids_object(o_ptr))
        return false;

    if (source_item >= 0 && source_item < INVEN_PACK)
        return inven_carry_okay_after_removing(o_ptr, source_item, 1);

    return inven_carry_okay(o_ptr);
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

static bool handle_iron_crown_silmaril_action(object_type* o_ptr, int item)
{
    object_type* w_ptr;

    if (!o_ptr)
        return false;

    if ((o_ptr->name1 < ART_MORGOTH_1) || (o_ptr->name1 > ART_MORGOTH_3))
        return false;

    if (item >= 0)
    {
        msg_print("You would have to put it down first.");
        return true;
    }

    w_ptr = &inventory[INVEN_WIELD];
    if (!w_ptr->k_idx)
    {
        msg_print(
            "To prise a Silmaril from the crown, you would need to wield a "
            "weapon.");
        return true;
    }

    if (!get_check("Will you try to prise a Silmaril from the Iron Crown? "))
        return true;

    prise_silmaril();

    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;

    return true;
}

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

    if (o_ptr->name1 == ART_MORGOTH_0)
    {
        msg_print("There are no Silmarils left in the Iron Crown.");
        return;
    }

    if (handle_iron_crown_silmaril_action(o_ptr, item))
        return;

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
                    && (o_ptr->sval == SV_LIGHT_LANTERN)))
            {
                if (l_ptr->sval == SV_LIGHT_LANTERN)
                {
                    do_cmd_refuel_lamp(o_ptr, item);
                    try_to_wield = false;
                }
            }

            if (o_ptr->tval == TV_FLASK && try_to_wield)
            {
                if ((l_ptr->tval != TV_LIGHT)
                    || (l_ptr->sval != SV_LIGHT_LANTERN))
                {
                    msg_print("You are not wielding a lantern.");
                }
                try_to_wield = false;
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
        msg_print("To smith with mithril or star-iron, take them to a forge and "
                  "type (,).");
        break;
    }
    case TV_CHEST:
    {
        msg_print("You would need to put it down to open it.");
        break;
    }
    case TV_STAFF:
    {
        extern char current_menu_command;
        /* If wielding ('w' command), equip the staff directly */
        if (current_menu_command == 'w')
        {
            do_cmd_wield(o_ptr, item);
        }
        else
        {
            /* Otherwise, activate it (for 'u' command) */
            do_cmd_activate_staff(o_ptr, item);
        }
        break;
    }
    case TV_GEM:
    {
        do_cmd_use_gem(o_ptr, item);
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

    enhanced_inventory_selected_item = ENHANCED_MENU_NO_SELECTION;

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
            if (selected_index != ENHANCED_MENU_NO_SELECTION)
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
    enhanced_inventory_selected_item = ENHANCED_MENU_NO_SELECTION;
    
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

    enhanced_equipment_selected_item = ENHANCED_MENU_NO_SELECTION;

    /* Save screen */
    screen_save();
    log_debug("do_cmd_equip: Screen saved");

    /* Show every equipment slot; the menu navigates only occupied rows. */
    item_tester_full = true;

    /* Force viewing mode */
    p_ptr->command_see = true;

    /* Display the equipment with scrolling capability */
    show_equip_enhanced();

    /* Keep selector state clean after closing the menu. */
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
        {
            (void)player_try_identify_smithing_object_on_examine(
                &inventory[selected_index], true);
            object_info_screen(&inventory[selected_index]);
        }
        break;

    case ENHANCED_ACTION_USE:
        if (death_view)
        {
            msg_print("You can no longer take that action.");
        }
        else
        {
            log_trace("do_cmd_equip: Using item %d", selected_index);
            if (selected_index != ENHANCED_MENU_NO_SELECTION)
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
    enhanced_equipment_selected_item = ENHANCED_MENU_NO_SELECTION;
    
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
    int supply_index = supplies_current_action();
    bool from_supplies = false;
    int oil_swap_drop_idx = 0;

    u32b f1, f2, f3, f4;

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
        from_supplies = (item == SUPPLIES_INDEX) && (supply_index >= 0);
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

        if (item == SUPPLIES_INDEX)
        {
            open_supplies_menu_with_context(
                SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_LIGHTS, true, true);
            return;
        }

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
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);

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
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
    {
        if (item < 0)
            object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
        else
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        msg_format("You cannot wear or wield %s.", o_name);
        return;
    }

    if ((item < 0) && player_light_carry_cap(o_ptr) > 0)
    {
        object_type* equipped_ptr = &inventory[slot];
        bool replacing_same_group = equipped_ptr->k_idx
            && player_light_share_carry_group(o_ptr, equipped_ptr);

        if (!replacing_same_group && player_light_available_capacity(o_ptr) <= 0)
        {
            if (player_oil_container_object(o_ptr))
                msg_print("You have no free lamp/flask slots.");
            else
                msg_print("You cannot carry any more of those.");
            return;
        }
    }

    if (!from_supplies
        && o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_LANTERN
        && o_ptr->timeout > 0
        && player_lamp_oil_would_overflow_with_bonus(o_ptr->timeout,
            (item < 0) ? 1 : 0)
        && !get_check("Taking this lamp will waste some oil. Proceed? "))
    {
        return;
    }

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

    // Check for paired weapons (e.g., Glamdring + Orcrist)
    // Paired weapons can be wielded together without Two Weapon Fighting
    bool paired_weapon_prompt = false;
    if (o_ptr->name1 && inventory[INVEN_WIELD].k_idx)
    {
        int paired_idx = get_paired_artefact(o_ptr->name1);
        if (paired_idx && inventory[INVEN_WIELD].name1 == paired_idx)
        {
            // The weapon we're trying to wield is paired with our main hand weapon
            if (!(k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
                && !(k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF)))
            {
                if (get_check("Wield alongside its mate in your off-hand? "))
                {
                    slot = INVEN_ARM;
                    paired_weapon_prompt = true;
                }
            }
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
    if (!paired_weapon_prompt
        && (p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON] || grants_two_weapon)
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

    if (smith_oath_forbids_object(o_ptr) && !smith_oath_confirm_break())
        return;

    if (inventory[slot].k_idx && !combine
        && smith_oath_takeoff_hits_pack(&inventory[slot], item)
        && !smith_oath_confirm_break())
    {
        return;
    }

    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && inventory[INVEN_ARM].k_idx
        && smith_oath_takeoff_hits_pack(&inventory[INVEN_ARM], item)
        && !smith_oath_confirm_break())
    {
        return;
    }

    if ((slot == INVEN_ARM)
        && inventory[INVEN_WIELD].k_idx
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED))
        && smith_oath_takeoff_hits_pack(&inventory[INVEN_WIELD], item)
        && !smith_oath_confirm_break())
    {
        return;
    }
    
    /* Oath of Light: warn before equipping shadowed items */
    if (chosen_oath(OATH_LIGHT) && !oath_invalid(OATH_LIGHT))
    {
        object_flags4(o_ptr, &f1, &f2, &f3, &f4);
        if ((f2 & TR2_DARKNESS) || (f4 & TR4_UNLIGHT) || (f3 & TR3_LIGHT_CURSE))
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

    if (!from_supplies && i_ptr->tval == TV_LIGHT
        && i_ptr->sval == SV_LIGHT_LANTERN)
    {
        player_gain_lamp_oil_with_bonus(i_ptr->timeout, true,
            (item < 0) ? 1 : 0);
        i_ptr->timeout = 0;
    }

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
    if (from_supplies)
    {
        supplies_consume_quantity(supply_index, quantity);
    }
    else if (item >= 0)
    {
        log_debug(
            "do_cmd_wield: Before decrease - item=%d, k_idx=%d, ego_pfx=%d, ego_sfx=%d, number=%d",
            item, inventory[item].k_idx, object_ego_prefix(&inventory[item]),
            object_ego_suffix(&inventory[item]), inventory[item].number);
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
    
    log_debug("do_cmd_wield: Wield slot %d - has k_idx=%d, ego_pfx=%d, ego_sfx=%d",
        slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr));

    /* Take off existing item */
    if (o_ptr->k_idx && !combine)
    {
        bool refill_oil_pool_from_takeoff = (item < 0)
            && player_oil_container_object(i_ptr)
            && player_oil_container_object(o_ptr);
        int takeoff_result;

        /*
         * Lights coming from the floor are not counted yet, so reserve them
         * during the takeoff even when swapping within the same carry group.
         * Pack/supplies lights are already counted and only need reservation
         * when the swap crosses carry groups.
         */
        if (slot == INVEN_LITE && player_light_carry_cap(i_ptr) > 0)
        {
            if ((item < 0) || !player_light_share_carry_group(i_ptr, o_ptr))
                player_light_reserve_incoming(i_ptr, i_ptr->number);
            else
                player_light_clear_incoming_reservation();
        }

        log_debug(
            "do_cmd_wield: Taking off existing item from slot %d - k_idx=%d, ego_pfx=%d, ego_sfx=%d",
            slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr));
        /* Take off existing item */
        takeoff_result = inven_takeoff(slot, 255);
        if (refill_oil_pool_from_takeoff && takeoff_result < 0)
            oil_swap_drop_idx = 0 - takeoff_result;
        player_light_clear_incoming_reservation();
        
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
        log_debug(
            "do_cmd_wield: Combining - slot %d has k_idx=%d ego_pfx=%d ego_sfx=%d, adding k_idx=%d ego_pfx=%d ego_sfx=%d",
            slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr),
            i_ptr->k_idx, object_ego_prefix(i_ptr), object_ego_suffix(i_ptr));
        msg_print(
            "You combine them with some that are already in your quiver.");
        object_absorb(o_ptr, i_ptr);
    }
    /* Wear the new stuff */
    else
    {
        log_debug(
            "do_cmd_wield: Copying to slot %d - source k_idx=%d ego_pfx=%d ego_sfx=%d",
            slot, i_ptr->k_idx, object_ego_prefix(i_ptr), object_ego_suffix(i_ptr));
        object_copy(o_ptr, i_ptr);
        log_debug(
            "do_cmd_wield: After copy, slot %d now has k_idx=%d ego_pfx=%d ego_sfx=%d",
            slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr));
    }

    if (oil_swap_drop_idx > 0 && oil_swap_drop_idx < o_max
        && o_list[oil_swap_drop_idx].k_idx)
    {
        if (player_refill_lamp_oil_from_container(&o_list[oil_swap_drop_idx])
            > 0)
        {
            p_ptr->redraw |= (PR_LIGHT);
            p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
        }
    }

    /* Once the player has equipped an item, remember its combat stats forever. */
    o_ptr->ident |= (IDENT_HANDLED);

    /* Increment the equip counter by hand */
    if (!combine)
        p_ptr->equip_cnt++;

    /* Attempt identification immediately upon equipping (before printing message) */
    {
        bool slot_is_quiver1 = (slot == INVEN_QUIVER1);
        bool slot_is_quiver2 = (slot == INVEN_QUIVER2);
        bool quiver2_grants_bonuses = slot_is_quiver2 && is_throwing;
        bool apply_wield_effects
            = !slot_is_quiver1 && (!slot_is_quiver2 || quiver2_grants_bonuses);

        if (apply_wield_effects)
        {
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
        }
    }

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
    else if (slot == INVEN_HORN)
    {
        act = "You are carrying";
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
        if (target_is_quiver && (quantity < original_quantity)
            && ((i_ptr->tval == TV_ARROW) || is_throwing))
        {
            int floor_idx = 0 - item;
            object_type* floor_ptr = &o_list[floor_idx];

            if (floor_ptr->k_idx && floor_ptr->number > 0)
                py_pickup_aux(floor_idx);
        }

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

    /* Items with BREAKS_PERMA_CURSE can break the Oath of Feanor on all equipped items */
    {
        u32b o_f1, o_f2, o_f3, o_f4;
        object_flags4(o_ptr, &o_f1, &o_f2, &o_f3, &o_f4);

        if (o_f4 & TR4_BREAKS_PERMA_CURSE)
        {
            int j;
            bool oath_broken = false;

            /* Check all equipped items for the Oath of Feanor (perma-curse) */
            for (j = INVEN_WIELD; j < INVEN_TOTAL; j++)
            {
                object_type *eq_ptr = &inventory[j];
                u32b eq_f1, eq_f2, eq_f3;

                if (!eq_ptr->k_idx) continue;

                object_flags(eq_ptr, &eq_f1, &eq_f2, &eq_f3);

                if ((eq_f3 & TR3_PERMA_CURSE) && cursed_p(eq_ptr))
                {
                    /* Break the curse - the holy light overcomes the oath */
                    eq_ptr->ident &= ~IDENT_CURSED;
                    oath_broken = true;
                }
            }

            if (oath_broken)
            {
                msg_print("The holy light breaks the Oath of Feanor!");
            }
        }
    }

    if (weapon_less_effective)
    {
        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[INVEN_WIELD], false, 0);

        /* Message */
        msg_format(
            "You are no longer able to wield your %s as effectively.", o_name);
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
    inven_enforce_current_pack_limits();

    /*
     * Smithing identification checks depend on the player's current effective
     * skills, so retry now that equipped bonuses have been applied.
     */
    if (player_try_identify_smithing_object(o_ptr, true, 0))
    {
        /* Ensure the newly-identified item (and any resulting bonuses) display immediately. */
        handle_stuff();
    }
}

/*
 * Take off an item
 */
void do_cmd_takeoff(object_type* default_o_ptr, int default_item)
{
    int item;
    bool can_break_curse;

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

    can_break_curse = p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING];

    if (((item == INVEN_QUIVER1) || (item == INVEN_QUIVER2)) && cursed_p(o_ptr))
    {
        msg_print("You cannot bear to part with it.");
        return;
    }
    else if (cursed_p(o_ptr) && can_break_curse)
    {
        {
            object_type carry_preview;
            object_copy(&carry_preview, o_ptr);
            carry_preview.ident &= ~(IDENT_CURSED);
            carry_preview.ident |= IDENT_UNCURSED;

            if (carry_preview.discount >= INSCRIP_NULL)
                carry_preview.discount = 0;

            if (smith_oath_forbids_object(o_ptr) && inven_carry_okay(&carry_preview)
                && !smith_oath_confirm_break())
            {
                return;
            }

            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
    }
    else if (cursed_p(o_ptr))
    {
        /* Oops */
        msg_print("You cannot bear to part with it.");

        /* Nope */
        return;
    }
    else if (smith_oath_forbids_object(o_ptr) && inven_carry_okay(o_ptr)
        && !smith_oath_confirm_break())
    {
        return;
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
    inven_enforce_current_pack_limits();
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

    if (((item == INVEN_QUIVER1) || (item == INVEN_QUIVER2)) && cursed_p(o_ptr))
    {
        msg_print("You cannot bear to part with it.");
        return;
    }

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

    if (((item == INVEN_QUIVER1) || (item == INVEN_QUIVER2)) && cursed_p(o_ptr))
    {
        msg_print("You cannot bear to part with it.");
        return;
    }

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

static void prise_silmaril(void)
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
    if (handle_iron_crown_silmaril_action(o_ptr, -1))
        return;

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

    if (handle_iron_crown_silmaril_action(o_ptr, item))
        return;

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
    if (item < 0)
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
    else
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
    if (item < 0)
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
    else
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

    object_type* o_ptr = NULL;
    object_type* j_ptr;
    int supply_index = supplies_current_action();
    bool from_supplies = false;
    int source_oil = 0;

    cptr q, s;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
        from_supplies = (item == SUPPLIES_INDEX) && (supply_index >= 0);
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = item_tester_refuel_lantern;

        /* Get an item */
        q = "Refill with which source of oil? ";
        s = "You have no sources of oil.";
        supplies_set_pending_action(SUPPLY_MENU_ACTION_USE,
            SUPPLY_GROUP_LIGHTS, true);
        if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR)))
        {
            supplies_clear_pending_action();
            return;
        }

        if (item == SUPPLIES_INDEX)
        {
            supplies_clear_pending_action();
            open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE,
                SUPPLY_GROUP_LIGHTS, true, true);
            return;
        }

        supplies_clear_pending_action();

        /* Get the item (in the pack) */
        if (item >= SUPPLIES_INDEX)
        {
            supply_index = item - SUPPLIES_INDEX;
            o_ptr = supplies_entry_at(supply_index);
            from_supplies = true;
        }
        else if (item >= 0)
        {
            o_ptr = &inventory[item];
        }

        /* Get the item (on the floor) */
        else
        {
            o_ptr = &o_list[0 - item];
        }
    }

    if (!o_ptr)
        return;

    source_oil = (o_ptr->tval == TV_FLASK) ? o_ptr->pval : o_ptr->timeout;

    if (from_supplies)
    {
        if (source_oil > 0)
        {
            player_gain_lamp_oil(source_oil, true);
            player_oil_container_set_fuel(o_ptr, 0);
            supplies_refresh_entry(supply_index);
            msg_print("You add the oil to your lamp stores.");
        }
        else
        {
            msg_print("That oil is already in your lamp stores.");
        }

        p_ptr->redraw |= (PR_LIGHT);
        handle_stuff();
        return;
    }

    /* Get the lantern */
    j_ptr = &inventory[INVEN_LITE];

    if ((j_ptr->tval != TV_LIGHT) || (j_ptr->sval != SV_LIGHT_LANTERN))
    {
        msg_print("You are not wielding a lantern.");
        return;
    }

    if (source_oil <= 0)
    {
        msg_print("There is no oil left in that.");
        return;
    }

    if (source_oil + player_light_fuel(j_ptr) > player_light_max_fuel(j_ptr)
        && !get_check("Refueling this lamp will waste some oil. Proceed? "))
    {
        return;
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Refuel from a latern */
    if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        player_light_add_fuel(j_ptr, source_oil);
    }
    /* Refuel from a flask */
    else
    {
        player_light_add_fuel(j_ptr, source_oil);
    }

    /* Message */
    msg_print("You fuel your lamp.");

    /* Comment */
    if (player_light_fuel(j_ptr) >= player_light_max_fuel(j_ptr))
    {
        player_light_set_fuel(j_ptr, player_light_max_fuel(j_ptr));
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
                if (item < 0)
                    drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
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
        msg_print("You can no longer combine torches.");
    }

    /* It's a torch */
    else if (o_ptr->sval == SV_LIGHT_MALLORN)
    {
        msg_print("You can no longer combine torches.");
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
static bool g_unified_look_has_start = false;
static int g_unified_look_start_y = 0;
static int g_unified_look_start_x = 0;

void do_cmd_look_at(int y, int x)
{
    if (y < 0 || y >= p_ptr->cur_map_hgt || x < 0 || x >= p_ptr->cur_map_wid)
    {
        do_cmd_look();
        return;
    }

    g_unified_look_has_start = true;
    g_unified_look_start_y = y;
    g_unified_look_start_x = x;
    do_cmd_look();
    g_unified_look_has_start = false;
}

void do_cmd_look(void)
{
    /* Block when hallucinating */
    if (p_ptr->image)
    {
        msg_print("Your vision is too distorted to examine things carefully.");
        return;
    }

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

static bool unified_look_can_show_monster_at(int y, int x)
{
    int m_idx = cave_m_idx[y][x];

    return (m_idx > 0) && mon_list[m_idx].ml && grid_info_is_available(y, x);
}

static bool unified_look_can_show_marked_object_at(int y, int x)
{
    int o_idx = cave_o_idx[y][x];

    return (o_idx > 0) && o_list[o_idx].k_idx && o_list[o_idx].marked
        && grid_info_is_available(y, x);
}

static bool unified_look_sidebar_in_radius(const unified_look_state* state, int y,
    int x);

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
            if (!unified_look_can_show_monster_at(temp_y[i], temp_x[i])) continue;
            if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i])) continue;

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

            if (!grid_info_is_available(temp_y[i], temp_x[i]))
                continue;

            object_type* o_ptr = &o_list[o_idx];

            if (!o_ptr->k_idx)
                continue;

            /* Only count marked (memorized) objects (matches sidebar display) */
            if (!o_ptr->marked)
                continue;
            if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i]))
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

        if (!grid_info_is_available(temp_y[i], temp_x[i]))
            continue;

        object_type* o_ptr = &o_list[o_idx];

        if (!o_ptr->k_idx)
            continue;

        /* Only count marked (memorized) objects (matches sidebar display) */
        if (!o_ptr->marked)
            continue;
        if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i]))
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

static bool unified_look_sidebar_in_radius(const unified_look_state* state, int y,
    int x)
{
    if (!state || !state->nearby_filter)
        return true;

    return distance(p_ptr->py, p_ptr->px, y, x) <= UNIFIED_LOOK_NEAR_RADIUS;
}

static void unified_look_sync_cursor_selection(unified_look_state* state)
{
    int new_selection;

    if (!state)
        return;

    if ((state->look_mode != 0) || state->in_sidebar_mode)
        return;

    new_selection = unified_look_find_cursor_selection(state, state->cursor_y,
        state->cursor_x);

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        if ((new_selection < 0)
            || (state->highlighted_y != state->cursor_y)
            || (state->highlighted_x != state->cursor_x))
        {
            highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
                false);
            state->highlighted_y = -1;
            state->highlighted_x = -1;
            state->highlighted_entity_type = 0;
        }
    }

    state->selected_entity = new_selection;
}

static void unified_look_select_sidebar_entity(unified_look_state* state,
    int entity_index)
{
    if (!state || entity_index < 0)
        return;

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
            false);
        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
    }

    state->selected_entity = entity_index;
    state->in_sidebar_mode = true;
    state->square_cycling_mode = false;
    state->current_square_entity = 0;
}

enum
{
    UNIFIED_LOOK_PROMPT_MAX_BUTTONS = 16,
    UNIFIED_LOOK_CLICK_COMMAND_BASE = -1000,
    UNIFIED_LOOK_CLICK_PROMPT_BACKGROUND = -1300
};

static bool unified_look_prompt_choice_key(int choice, int* key);

static bool unified_look_apply_sidebar_pointer_action(unified_look_state* state,
    bool compact_look_layout, bool* need_redraw, bool* selection_redraw,
    bool* prompt_hover_redraw)
{
    int clicked_entity = -1;
    int click_action = UI_MENU_CLICK_PRIMARY;
    int prompt_key = 0;

    if (!ui_menu_click_take_action(&clicked_entity, &click_action))
        return false;

    if (unified_look_prompt_choice_key(clicked_entity, &prompt_key))
    {
        if (click_action != UI_MENU_CLICK_HOVER)
            Term_keypress(prompt_key);
        else if (prompt_hover_redraw)
            *prompt_hover_redraw = true;
        return true;
    }

    if (clicked_entity == UNIFIED_LOOK_CLICK_PROMPT_BACKGROUND)
    {
        if (click_action == UI_MENU_CLICK_HOVER && prompt_hover_redraw)
            *prompt_hover_redraw = true;
        return true;
    }

    if (state && state->in_sidebar_mode
        && state->selected_entity == clicked_entity)
    {
        if (click_action == UI_MENU_CLICK_SECONDARY)
            Term_keypress(' ');
        return true;
    }

    unified_look_select_sidebar_entity(state, clicked_entity);
    if (need_redraw)
        *need_redraw = true;
    if (selection_redraw)
        *selection_redraw = !compact_look_layout;

    if (click_action == UI_MENU_CLICK_SECONDARY)
        Term_keypress(' ');

    return true;
}

static bool unified_look_apply_map_cell(unified_look_state* state, int map_y,
    int map_x, bool compact_look_layout, bool* need_redraw,
    bool* selection_redraw)
{
    int new_selection;
    bool new_sidebar_mode;

    if (!state)
        return false;
    if (map_y < 0 || map_y >= p_ptr->cur_map_hgt
        || map_x < 0 || map_x >= p_ptr->cur_map_wid)
    {
        return true;
    }

    new_selection = unified_look_find_cursor_selection(state, map_y, map_x);
    new_sidebar_mode = (new_selection >= 0);

    if ((state->cursor_y == map_y) && (state->cursor_x == map_x)
        && (state->selected_entity == new_selection)
        && (state->in_sidebar_mode == new_sidebar_mode))
    {
        return true;
    }

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
            false);
        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
    }

    state->cursor_y = map_y;
    state->cursor_x = map_x;
    state->selected_entity = new_selection;
    state->in_sidebar_mode = new_sidebar_mode;
    state->square_cycling_mode = false;
    state->current_square_entity = 0;

    if (need_redraw)
        *need_redraw = true;
    if (selection_redraw)
        *selection_redraw = !compact_look_layout;

    return true;
}

static void unified_look_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static bool unified_look_use_compact_layout(void)
{
    return Term && (Term->wid <= 60);
}

static int unified_look_status_row(void)
{
    return 0;
}

static int unified_look_prompt_row(void)
{
    int row;

    if (!Term || Term->hgt <= 0)
        return 0;

    row = ROW_MAP + SCREEN_HGT - 1;
    if (row < 0)
        row = 0;
    if (row >= Term->hgt)
        row = Term->hgt - 1;

    return row;
}

static void unified_look_put_row(cptr text, int row)
{
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    char buf[192];

    SDL_strlcpy(buf, text ? text : "", sizeof(buf));

    if ((int)strlen(buf) > term_wid && term_wid > 4)
    {
        int cut = term_wid - 4;
        if (cut < 0)
            cut = 0;
        buf[cut] = '\0';
        SDL_strlcat(buf, "...", sizeof(buf));
    }

    prt(buf, row, 0);
}

static void unified_look_put_status(cptr text)
{
    unified_look_put_row(text, unified_look_status_row());
}

static void unified_look_put_prompt(cptr text)
{
    unified_look_put_row(text, unified_look_prompt_row());
}

typedef struct unified_look_prompt_button
{
    int key;
    cptr full;
    cptr medium;
    cptr compact;
    cptr tiny;
} unified_look_prompt_button;

static int unified_look_prompt_choice(int key)
{
    return UNIFIED_LOOK_CLICK_COMMAND_BASE - (key & 0xFF);
}

static bool unified_look_prompt_choice_key(int choice, int* key)
{
    int decoded;

    if (choice > UNIFIED_LOOK_CLICK_COMMAND_BASE
        || choice < UNIFIED_LOOK_CLICK_COMMAND_BASE - 0xFF)
    {
        return false;
    }

    decoded = UNIFIED_LOOK_CLICK_COMMAND_BASE - choice;
    if (key)
        *key = decoded;

    return true;
}

static bool unified_look_apply_map_hover(unified_look_state* state,
    bool compact_look_layout, bool* need_redraw, bool* selection_redraw)
{
    int hover_y = 0;
    int hover_x = 0;

    if (!sdl_unified_look_take_map_hover(&hover_y, &hover_x))
        return false;

    return unified_look_apply_map_cell(state, hover_y, hover_x,
        compact_look_layout, need_redraw, selection_redraw);
}

static cptr unified_look_prompt_button_text(
    const unified_look_prompt_button* button, int variant)
{
    if (!button)
        return "";

    switch (variant)
    {
    case 0: return button->full;
    case 1: return button->medium;
    case 2: return button->compact;
    default: return button->tiny;
    }
}

static int unified_look_prompt_buttons_width(
    const unified_look_prompt_button* buttons, int count, int variant)
{
    int width = 0;

    for (int i = 0; i < count; i++)
    {
        cptr text = unified_look_prompt_button_text(&buttons[i], variant);

        if (!text || !text[0])
            continue;

        if (width > 0)
            width++;
        width += (int)strlen(text) + 2;
    }

    return width;
}

static void unified_look_print_prompt_buttons(
    const unified_look_prompt_button* buttons, int count, bool register_clicks)
{
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    int row = unified_look_prompt_row();
    int starts[UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
    int ends[UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
    int keys[UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
    int registered = 0;
    int variant = 3;
    int hover_choice = 0;
    bool has_hover_choice = ui_menu_click_get_hover_choice(&hover_choice);
    char buf[192];

    if (!buttons || count <= 0)
    {
        unified_look_put_prompt("");
        return;
    }

    if (count > UNIFIED_LOOK_PROMPT_MAX_BUTTONS)
        count = UNIFIED_LOOK_PROMPT_MAX_BUTTONS;

    for (int i = 0; i < 4; i++)
    {
        if (unified_look_prompt_buttons_width(buttons, count, i) <= term_wid)
        {
            variant = i;
            break;
        }
    }

    buf[0] = '\0';
    for (int i = 0; i < count; i++)
    {
        cptr text = unified_look_prompt_button_text(&buttons[i], variant);
        int start;
        int end;

        if (!text || !text[0])
            continue;

        if (buf[0])
            SDL_strlcat(buf, " ", sizeof(buf));

        start = (int)strlen(buf);
        SDL_strlcat(buf, "[", sizeof(buf));
        SDL_strlcat(buf, text, sizeof(buf));
        SDL_strlcat(buf, "]", sizeof(buf));
        end = (int)strlen(buf);

        if (registered < UNIFIED_LOOK_PROMPT_MAX_BUTTONS)
        {
            starts[registered] = start;
            ends[registered] = end;
            keys[registered] = buttons[i].key;
            registered++;
        }
    }

    unified_look_put_prompt(buf);

    for (int i = 0; i < registered; i++)
    {
        int choice = unified_look_prompt_choice(keys[i]);

        if (register_clicks)
        {
            ui_menu_click_add_text_span(choice, 0, row, buf, starts[i],
                ends[i]);
        }
        else if (has_hover_choice && hover_choice == choice)
        {
            Term_putstr(starts[i], row, ends[i] - starts[i], TERM_L_BLUE,
                buf + starts[i]);
        }
    }

    if (register_clicks)
        ui_menu_click_add_full_row(UNIFIED_LOOK_CLICK_PROMPT_BACKGROUND, row);
}

static void unified_look_print_controller_prompt(
    bool compact_look_layout, bool cursor_mode, bool register_clicks)
{
    char prev_label[16];
    char next_label[16];
    char exam_label[16];
    char target_label[16];
    char obj_label[16];
    char mode_label[16];
    char back_label[16];
    char prev_full[32];
    char next_full[32];
    char exam_full[32];
    char target_full[32];
    char obj_full[32];
    char mode_full[32];
    char back_full[32];
    char zoom_in_full[32];
    char zoom_out_full[32];
    char prev_compact[32];
    char next_compact[32];
    char exam_compact[32];
    char target_compact[32];
    char obj_compact[32];
    char mode_compact[32];
    char back_compact[32];
    char zoom_in_compact[32];
    char zoom_out_compact[32];
    cptr obj_action = compact_look_layout ? "View" : "Objects";
    cptr mode_action = cursor_mode ? "Cursor" : "Pan";

    unified_look_prompt_label('e', "L1", prev_label, sizeof(prev_label));
    unified_look_prompt_label('i', "R1", next_label, sizeof(next_label));
    unified_look_prompt_label(' ', "A", exam_label, sizeof(exam_label));
    unified_look_prompt_label('f', "B", target_label, sizeof(target_label));
    unified_look_prompt_label('u', "X", obj_label, sizeof(obj_label));
    unified_look_prompt_label('s', "Y", mode_label, sizeof(mode_label));
    unified_look_prompt_label(ESCAPE, "Esc", back_label, sizeof(back_label));

    strnfmt(prev_full, sizeof(prev_full), "%s Prev", prev_label);
    strnfmt(next_full, sizeof(next_full), "%s Next", next_label);
    strnfmt(exam_full, sizeof(exam_full), "%s Exam", exam_label);
    strnfmt(target_full, sizeof(target_full), "%s Target", target_label);
    strnfmt(obj_full, sizeof(obj_full), "%s %s", obj_label, obj_action);
    strnfmt(mode_full, sizeof(mode_full), "%s %s", mode_label, mode_action);
    strnfmt(back_full, sizeof(back_full), "%s Back", back_label);
    SDL_strlcpy(zoom_in_full, "Zoom +", sizeof(zoom_in_full));
    SDL_strlcpy(zoom_out_full, "Zoom -", sizeof(zoom_out_full));
    strnfmt(prev_compact, sizeof(prev_compact), "%s Prv", prev_label);
    strnfmt(next_compact, sizeof(next_compact), "%s Nxt", next_label);
    strnfmt(exam_compact, sizeof(exam_compact), "%s Ex", exam_label);
    strnfmt(target_compact, sizeof(target_compact), "%s Tgt", target_label);
    strnfmt(obj_compact, sizeof(obj_compact), "%s %s", obj_label,
        compact_look_layout ? "Vw" : "Obj");
    strnfmt(mode_compact, sizeof(mode_compact), "%s %s", mode_label,
        cursor_mode ? "Cur" : "Pan");
    strnfmt(back_compact, sizeof(back_compact), "%s Bk", back_label);
    SDL_strlcpy(zoom_in_compact, "Z+", sizeof(zoom_in_compact));
    SDL_strlcpy(zoom_out_compact, "Z-", sizeof(zoom_out_compact));

    {
        unified_look_prompt_button buttons[UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
        int count = 0;

        buttons[count++] = (unified_look_prompt_button)
            { 'e', prev_full, prev_full, prev_compact, prev_label };
        buttons[count++] = (unified_look_prompt_button)
            { 'i', next_full, next_full, next_compact, next_label };
        buttons[count++] = (unified_look_prompt_button)
            { ' ', exam_full, exam_full, exam_compact, exam_label };
        buttons[count++] = (unified_look_prompt_button)
            { 'f', target_full, target_full, target_compact, target_label };
        buttons[count++] = (unified_look_prompt_button)
            { 'u', obj_full, obj_full, obj_compact, obj_label };
        buttons[count++] = (unified_look_prompt_button)
            { 's', mode_full, mode_full, mode_compact, mode_label };
        buttons[count++] = (unified_look_prompt_button)
            { '+', zoom_in_full, zoom_in_full, zoom_in_compact, zoom_in_compact };
        buttons[count++] = (unified_look_prompt_button)
            { '-', zoom_out_full, zoom_out_full, zoom_out_compact, zoom_out_compact };
        buttons[count++] = (unified_look_prompt_button)
            { ESCAPE, back_full, back_full, back_compact, back_label };

        unified_look_print_prompt_buttons(buttons, count, register_clicks);
    }
}

static void unified_look_print_touch_prompt(bool compact_look_layout,
    bool cursor_mode, cptr filter_action, bool register_clicks)
{
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    bool extended = (term_wid >= 90);
    char filter_full[32];
    char display_full[32];
    char display_compact[32];
    char mode_full[32];
    char mode_compact[32];
    cptr display_action = compact_look_layout ? "View" : "Display";

    if (!filter_action)
        filter_action = "";

    strnfmt(filter_full, sizeof(filter_full), "%s", filter_action);
    strnfmt(display_full, sizeof(display_full), "%s", display_action);
    strnfmt(display_compact, sizeof(display_compact), "%s",
        compact_look_layout ? "View" : "Disp");
    strnfmt(mode_full, sizeof(mode_full), "%s", cursor_mode ? "Cursor" : "Pan");
    strnfmt(mode_compact, sizeof(mode_compact), "%s",
        cursor_mode ? "Cur" : "Pan");

    {
        unified_look_prompt_button buttons[UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
        int count = 0;

        buttons[count++] = (unified_look_prompt_button)
            { '\t', "Next", "Next", "Nxt", "Nxt" };
        buttons[count++] = (unified_look_prompt_button)
            { 'q', "Prev", "Prev", "Prv", "Prv" };
        buttons[count++] = (unified_look_prompt_button)
            { ' ', "Examine", "Exam", "Ex", "Ex" };
        buttons[count++] = (unified_look_prompt_button)
            { 't', "Target", "Target", "Tgt", "Tgt" };
        buttons[count++] = (unified_look_prompt_button)
            { 'i', filter_full, filter_full, filter_full, filter_full };
        buttons[count++] = (unified_look_prompt_button)
            { 'l', display_full, display_full, display_compact,
                display_compact };
        if (extended)
        {
            buttons[count++] = (unified_look_prompt_button)
                { 'm', "Monsters", "Mon", "Mon", "Mon" };
            buttons[count++] = (unified_look_prompt_button)
                { 'o', "Objects", "Obj", "Obj", "Obj" };
            buttons[count++] = (unified_look_prompt_button)
                { 'T', "Top 5", "Top 5", "Top", "Top" };
        }
        buttons[count++] = (unified_look_prompt_button)
            { 's', mode_full, mode_full, mode_compact, mode_compact };
        buttons[count++] = (unified_look_prompt_button)
            { '+', "Zoom +", "Zoom +", "Z+", "Z+" };
        buttons[count++] = (unified_look_prompt_button)
            { '-', "Zoom -", "Zoom -", "Z-", "Z-" };
        buttons[count++] = (unified_look_prompt_button)
            { ESCAPE, "Back", "Back", "Back", "Back" };

        unified_look_print_prompt_buttons(buttons, count, register_clicks);
    }
}

static void unified_look_print_keyboard_prompt(bool cursor_mode,
    cptr filter_action, bool register_clicks)
{
    char filter_full[32];

    if (!filter_action)
        filter_action = "";

    strnfmt(filter_full, sizeof(filter_full), "i %s", filter_action);

    {
        unified_look_prompt_button buttons[UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
        int count = 0;

        buttons[count++] = (unified_look_prompt_button)
            { '\t', "Tab Next", "Tab Next", "Tab N", "Tab" };
        buttons[count++] = (unified_look_prompt_button)
            { 'q', "q Prev", "q Prev", "q P", "q" };
        buttons[count++] = (unified_look_prompt_button)
            { ' ', "Space Exam", "Sp Exam", "Sp Ex", "Sp" };
        buttons[count++] = (unified_look_prompt_button)
            { 't', "t Target", "t Target", "t Tgt", "t" };
        buttons[count++] = (unified_look_prompt_button)
            { 'i', filter_full, filter_full, filter_full, "i" };
        buttons[count++] = (unified_look_prompt_button)
            { 'l', "l Display", "l Disp", "l Dsp", "l" };
        buttons[count++] = (unified_look_prompt_button)
            { 'm', "m Monsters", "m Mon", "m Mon", "m" };
        buttons[count++] = (unified_look_prompt_button)
            { 'o', "o Objects", "o Obj", "o Obj", "o" };
        buttons[count++] = (unified_look_prompt_button)
            { 'T', "T Top5", "T Top", "T Top", "T" };
        buttons[count++] = (unified_look_prompt_button)
            { 's', cursor_mode ? "s Cursor" : "s Pan",
                cursor_mode ? "s Cursor" : "s Pan",
                cursor_mode ? "s Cur" : "s Pan", "s" };
        buttons[count++] = (unified_look_prompt_button)
            { '+', "+ Zoom", "+ Zoom", "+ Zm", "+" };
        buttons[count++] = (unified_look_prompt_button)
            { '-', "- Zoom", "- Zoom", "- Zm", "-" };
        buttons[count++] = (unified_look_prompt_button)
            { ESCAPE, "Esc Back", "Esc Back", "Esc", "Esc" };

        unified_look_print_prompt_buttons(buttons, count, register_clicks);
    }
}

static void unified_look_update_prompt_buttons_ex(bool controller_controls,
    bool compact_look_layout, bool cursor_mode, bool nearby_filter,
    bool register_clicks)
{
    if (sdl_touch_only_device_active())
    {
        unified_look_print_touch_prompt(compact_look_layout, cursor_mode,
            nearby_filter ? "All" : "Near", register_clicks);
    }
    else if (controller_controls)
    {
        unified_look_print_controller_prompt(compact_look_layout, cursor_mode,
            register_clicks);
    }
    else
    {
        unified_look_print_keyboard_prompt(cursor_mode,
            nearby_filter ? "All" : "Near", register_clicks);
    }
}

static void unified_look_restore_map_cursor(const unified_look_state* state)
{
    if (!state)
        return;

    move_cursor_relative(state->cursor_y, state->cursor_x);
}

static bool unified_look_format_monster_status(int m_idx, char* out_val,
    size_t out_len)
{
    monster_type* m_ptr;
    char m_name[80];

    if (!out_val || !out_len || m_idx <= 0)
        return false;

    m_ptr = &mon_list[m_idx];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x08);
    strnfmt(out_val, out_len, "You see %s.", m_name);
    return true;
}

static bool unified_look_format_object_status(int o_idx, char* out_val,
    size_t out_len)
{
    object_type* o_ptr;
    char o_name[80];
    char smith_buf[20];

    if (!out_val || !out_len || o_idx <= 0)
        return false;

    o_ptr = &o_list[o_idx];
    if (!o_ptr->k_idx || !o_ptr->marked)
        return false;

    object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);

    smith_buf[0] = '\0';
    if (op_ptr->opt[OPT_show_smithing_difficulty_look]
        && object_known_p(o_ptr)
        && object_uses_smithing_difficulty(o_ptr))
    {
        int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
        int sd = object_smithing_difficulty(o_ptr);
        int wr = object_weight_rarity(o_ptr, depth);
        strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
    }

    strnfmt(out_val, out_len, "You see %s%s.", o_name, smith_buf);
    return true;
}

static bool unified_look_format_feature_status(int y, int x, char* out_val,
    size_t out_len)
{
    int feat;
    cptr feature_name = NULL;

    if (!out_val || !out_len)
        return false;
    if (!grid_info_is_available(y, x) || !(cave_info[y][x] & (CAVE_MARK)))
        return false;

    feat = cave_feat[y][x];

    if (feat >= FEAT_TRAP_HEAD && feat <= FEAT_TRAP_TAIL)
        feature_name = f_name + f_info[feat].name;
    else if (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
        feature_name = f_name + f_info[feat].name;
    else if (feat == FEAT_OPEN)
        feature_name = "open door";
    else if (feat == FEAT_BROKEN)
        feature_name = "broken door";
    else if (feat == FEAT_LESS)
        feature_name = "up staircase";
    else if (feat == FEAT_MORE)
        feature_name = "down staircase";
    else if (feat == FEAT_LESS_SHAFT)
        feature_name = "up shaft";
    else if (feat == FEAT_MORE_SHAFT)
        feature_name = "down shaft";

    if (!feature_name)
        return false;

    strnfmt(out_val, out_len, "You see %s.", feature_name);
    return true;
}

static void unified_look_format_status(const unified_look_state* state, int y,
    int x, char* out_val, size_t out_len)
{
    if (!out_val || !out_len)
        return;

    out_val[0] = '\0';

    if (state && state->in_sidebar_mode && state->selected_entity >= 0
        && state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        int hy = state->highlighted_y;
        int hx = state->highlighted_x;

        if (state->highlighted_entity_type == 2
            && unified_look_can_show_marked_object_at(hy, hx)
            && unified_look_format_object_status(cave_o_idx[hy][hx],
                out_val, out_len))
        {
            return;
        }

        if (state->highlighted_entity_type == 1
            && unified_look_can_show_monster_at(hy, hx)
            && unified_look_format_monster_status(cave_m_idx[hy][hx],
                out_val, out_len))
        {
            return;
        }
    }

    if (unified_look_can_show_monster_at(y, x)
        && unified_look_format_monster_status(cave_m_idx[y][x], out_val,
            out_len))
    {
        return;
    }

    if (unified_look_can_show_marked_object_at(y, x)
        && unified_look_format_object_status(cave_o_idx[y][x], out_val,
            out_len))
    {
        return;
    }

    (void)unified_look_format_feature_status(y, x, out_val, out_len);
}

static void unified_look_redraw_bars(const unified_look_state* state,
    bool controller_controls, bool compact_look_layout, bool register_clicks)
{
    char out_val[256];
    int y;
    int x;

    if (!state)
        return;

    y = state->cursor_y;
    x = state->cursor_x;

    unified_look_format_status(state, y, x, out_val, sizeof(out_val));
    unified_look_put_status(out_val);
    unified_look_update_prompt_buttons_ex(controller_controls,
        compact_look_layout, state->look_mode != 0, state->nearby_filter,
        register_clicks);
}

static void unified_look_pause_pointer_handlers(void)
{
    ui_menu_click_clear();
    sdl_unified_look_set_map_hover_enabled(false);
}

static void unified_look_resume_pointer_handlers(void)
{
    sdl_unified_look_set_map_hover_enabled(true);
}

static bool unified_look_examine_object_at(int y, int x, bool use_story_font)
{
    object_type* o_ptr;

    if (!unified_look_can_show_marked_object_at(y, x))
        return false;

    o_ptr = &o_list[cave_o_idx[y][x]];

    if (use_story_font)
        sdl_story_font_disable();
    unified_look_pause_pointer_handlers();

    (void)player_try_identify_smithing_object_on_examine(o_ptr, false);
    screen_save();

    if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
    {
        int slot = wield_slot(o_ptr);
        const object_type* compare_objects[2];
        const char* compare_headings[2];
        char selected_heading[32];
        char equipped_heading[32];

        strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
        strnfmt(equipped_heading, sizeof(equipped_heading), "%s",
            mention_use(slot));

        compare_objects[0] = o_ptr;
        compare_headings[0] = selected_heading;
        compare_objects[1] = inventory[slot].k_idx ? &inventory[slot] : NULL;
        compare_headings[1] = equipped_heading;

        object_info_screen_multi(compare_objects, compare_headings, 2);
    }
    else
    {
        object_info_screen(o_ptr);
    }

    screen_load();
    unified_look_resume_pointer_handlers();

    if (use_story_font)
        sdl_story_font_enable();

    return true;
}

static bool unified_look_examine_monster_at(int y, int x, bool use_story_font)
{
    int m_idx;
    monster_type* m_ptr;

    if (!unified_look_can_show_monster_at(y, x))
        return false;

    m_idx = cave_m_idx[y][x];
    m_ptr = &mon_list[m_idx];

    if (use_story_font)
        sdl_story_font_disable();
    unified_look_pause_pointer_handlers();

    monster_race_track(m_ptr->r_idx);
    health_track(m_idx);
    handle_stuff();

    screen_save();
    if (!screen_roff(m_ptr->r_idx, m_ptr))
        (void)inkey();
    screen_load();

    unified_look_resume_pointer_handlers();

    if (use_story_font)
        sdl_story_font_enable();

    return true;
}

static bool unified_look_target_monster_at(int y, int x)
{
    int m_idx;
    monster_type* m_ptr;
    char m_name[80];

    if (!unified_look_can_show_monster_at(y, x))
        return false;

    m_idx = cave_m_idx[y][x];
    if (!target_able(m_idx)) {
        bell("No clear target.");
        return false;
    }

    m_ptr = &mon_list[m_idx];
    target_set_monster(m_idx);
    health_track(m_idx);
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);
    msg_format("Target set to %s.", m_name);
    p_ptr->redraw |= PR_BASIC | PR_MAP;
    handle_stuff();
    return true;
}

static void unified_look_pan_player_for_sidebar(bool center_vertical)
{
    int max_wy = MAX(p_ptr->cur_map_hgt - SCREEN_HGT, 0);
    int max_wx = MAX(p_ptr->cur_map_wid - SCREEN_WID, 0);
    int desired_player_col = (SCREEN_WID * 2) / 3;
    int new_wy = p_ptr->wy;
    int new_wx;

    if (desired_player_col >= SCREEN_WID)
        desired_player_col = SCREEN_WID - 1;
    if (desired_player_col < 0)
        desired_player_col = 0;

    if (center_vertical)
        new_wy = p_ptr->py - (SCREEN_HGT / 2);

    new_wx = p_ptr->px - desired_player_col;
    if (new_wy < 0) new_wy = 0;
    if (new_wy > max_wy) new_wy = max_wy;
    if (new_wx < 0) new_wx = 0;
    if (new_wx > max_wx) new_wx = max_wx;

    log_trace("Unified look player pan: center_vertical=%d, desired_col=%d, viewport (%d,%d) -> (%d,%d)",
        center_vertical ? 1 : 0, desired_player_col, p_ptr->wy, p_ptr->wx, new_wy, new_wx);

    if (modify_panel(new_wy, new_wx))
        handle_stuff();
}

static void unified_look_constrain_viewport(int* wy, int* wx)
{
    int max_wy = MAX(p_ptr->cur_map_hgt - SCREEN_HGT, 0);
    int max_wx = MAX(p_ptr->cur_map_wid - SCREEN_WID, 0);

    if (!wy || !wx)
        return;

    if (*wy < 0)
        *wy = 0;
    if (*wy > max_wy)
        *wy = max_wy;
    if (*wx < 0)
        *wx = 0;
    if (*wx > max_wx)
        *wx = max_wx;
}

static bool unified_look_map_pan_would_move(int pan_dy, int pan_dx)
{
    int new_wy;
    int new_wx;

    if (pan_dy == 0 && pan_dx == 0)
        return false;

    new_wy = p_ptr->wy + pan_dy;
    new_wx = p_ptr->wx + pan_dx;
    unified_look_constrain_viewport(&new_wy, &new_wx);

    return (new_wy != p_ptr->wy || new_wx != p_ptr->wx);
}

static void unified_look_clear_highlight(unified_look_state* state)
{
    if (!state)
        return;

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
            false);
        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
    }
}

static bool unified_look_apply_map_pan(unified_look_state* state, int pan_dy,
    int pan_dx, bool* need_redraw, bool* selection_redraw)
{
    int old_wy;
    int old_wx;
    int new_wy;
    int new_wx;

    if (!state || (pan_dy == 0 && pan_dx == 0))
        return false;

    old_wy = p_ptr->wy;
    old_wx = p_ptr->wx;
    new_wy = old_wy + pan_dy;
    new_wx = old_wx + pan_dx;
    unified_look_constrain_viewport(&new_wy, &new_wx);

    if (!modify_panel(new_wy, new_wx))
        return true;

    state->cursor_y += p_ptr->wy - old_wy;
    state->cursor_x += p_ptr->wx - old_wx;
    if (state->cursor_y < 0)
        state->cursor_y = 0;
    if (state->cursor_y >= p_ptr->cur_map_hgt)
        state->cursor_y = p_ptr->cur_map_hgt - 1;
    if (state->cursor_x < 0)
        state->cursor_x = 0;
    if (state->cursor_x >= p_ptr->cur_map_wid)
        state->cursor_x = p_ptr->cur_map_wid - 1;

    state->in_sidebar_mode = false;
    state->selected_entity = -1;
    state->square_cycling_mode = false;
    state->current_square_entity = 0;
    unified_look_clear_highlight(state);

    handle_stuff();

    if (need_redraw)
        *need_redraw = true;
    if (selection_redraw)
        *selection_redraw = false;

    return true;
}

static bool unified_look_apply_main_zoom(int scale, bool* need_redraw)
{
    int max_scale = get_sdl_max_scale();
    int old_scale = get_sdl_main_view_scale();

    if (max_scale < 1)
        max_scale = 1;
    if (scale < 1)
        scale = 1;
    if (scale > max_scale)
        scale = max_scale;
    if (scale == old_scale)
        return true;

    set_sdl_main_view_scale(scale);
    if (get_sdl_main_view_scale() == old_scale)
        return true;

    sdl_apply_config_no_redraw();
    p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA | PR_HEALTHBAR | PR_MAP);
    p_ptr->window |= (PW_OVERHEAD);
    handle_stuff();

    if (need_redraw)
        *need_redraw = true;

    return true;
}

static void unified_look_track_cursor_health(const unified_look_state* state)
{
    int cursor_m_idx;

    if (!state)
    {
        health_track(0);
        handle_stuff();
        return;
    }

    cursor_m_idx = cave_m_idx[state->cursor_y][state->cursor_x];
    if ((cursor_m_idx > 0)
        && unified_look_can_show_monster_at(state->cursor_y, state->cursor_x))
    {
        health_track(cursor_m_idx);
    }
    else
    {
        health_track(0);
    }

    handle_stuff();
}

static void unified_look_redraw_overlay(unified_look_state* state,
    bool controller_controls, bool compact_look_layout, bool* overlay_saved,
    bool selection_redraw, bool register_clicks)
{
    if (!state || !overlay_saved)
        return;

    if (selection_redraw && *overlay_saved)
    {
        show_unified_sidebar(state);
    }
    else
    {
        if (*overlay_saved)
        {
            (void)Term_set_extra_cursor(false, 0, 0, false);
            screen_load_quiet();
            *overlay_saved = false;
        }

        unified_look_sync_cursor_selection(state);

        /* Save the clean map before painting look-only UI elements. */
        screen_save();
        *overlay_saved = true;

        show_unified_sidebar(state);
    }

    unified_look_track_cursor_health(state);
    unified_look_redraw_bars(state, controller_controls, compact_look_layout,
        register_clicks);
    unified_look_restore_map_cursor(state);
    Term_fresh();
}

void do_cmd_unified_look(void)
{
    unified_look_state state;
    int y, x;
    char query;
    bool done = false;
    bool need_redraw = true;
    bool overlay_saved = false;
    bool selection_redraw = false;
    bool compact_look_layout = false;
    bool original_hide_left_panel = g_hide_left_panel;
    bool original_suppress_hidden_left_panel_overlay
        = g_suppress_hidden_left_panel_overlay;
    bool look_adjusts_left_panel = false;
    bool look_adjusts_supporting_panes = false;
    bool original_hide_supporting_panes_fullscreen = op_ptr
        ? op_ptr->opt[OPT_hide_supporting_panes_fullscreen] : false;
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

    compact_look_layout = unified_look_use_compact_layout();
    
    log_trace("=== UNIFIED LOOK STARTED ===");
    
    /* Store original viewport */
    original_wy = p_ptr->wy;
    original_wx = p_ptr->wx;

    g_suppress_hidden_left_panel_overlay = true;
    g_hide_left_panel = true;
    look_adjusts_left_panel = true;
    if (op_ptr)
        op_ptr->opt[OPT_hide_supporting_panes_fullscreen] = true;
    screen_push_supporting_panes_hidden();
    look_adjusts_supporting_panes = true;
    p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA | PR_HEALTHBAR | PR_MAP);
    p_ptr->window |= (PW_OVERHEAD);
    handle_stuff();
    
    log_trace("Original viewport: (%d,%d)", original_wy, original_wx);
    
    /* Initialize state */
    state.cursor_y = p_ptr->py;
    state.cursor_x = p_ptr->px;
    if (g_unified_look_has_start
        && g_unified_look_start_y >= 0 && g_unified_look_start_y < p_ptr->cur_map_hgt
        && g_unified_look_start_x >= 0 && g_unified_look_start_x < p_ptr->cur_map_wid)
    {
        state.cursor_y = g_unified_look_start_y;
        state.cursor_x = g_unified_look_start_x;

        if (!panel_contains(state.cursor_y, state.cursor_x))
        {
            int max_wy = MAX(p_ptr->cur_map_hgt - SCREEN_HGT, 0);
            int max_wx = MAX(p_ptr->cur_map_wid - SCREEN_WID, 0);
            int new_wy = state.cursor_y - SCREEN_HGT / 2;
            int new_wx = state.cursor_x - SCREEN_WID / 2;

            p_ptr->wy = MIN(MAX(new_wy, 0), max_wy);
            p_ptr->wx = MIN(MAX(new_wx, 0), max_wx);
            p_ptr->redraw |= PR_MAP;
            p_ptr->window |= PW_OVERHEAD;
            handle_stuff();
        }
    }
    state.selected_entity = -1;
    state.show_monsters = true;
    state.show_objects = true;
    state.object_group_filter = -1;
    state.limit_objects_top_five = false;
    state.nearby_filter = look_nearby_filter_default ? true : false;
    state.display_mode = 0; /* 0 = manual, 1 = entity */
    state.highlighted_y = -1;
    state.highlighted_x = -1;
    state.highlighted_entity_type = 0; /* 0 = none, 1 = monster, 2 = object */
    state.in_sidebar_mode = false;
    state.look_mode = 0; /* 0 = normal unified look, 1 = L-style scrolling */
    state.current_square_entity = 0; /* 0 = monster, 1 = object */
    state.square_cycling_mode = false; /* Start in normal sidebar cycling mode */
    const bool controller_controls = steamdeck_controls_active();
    sdl_unified_look_set_active(true);
    sdl_unified_look_set_map_hover_enabled(true);

    if (!g_unified_look_has_start
        || ((state.cursor_y == p_ptr->py) && (state.cursor_x == p_ptr->px)))
    {
        unified_look_pan_player_for_sidebar(false);
    }
    
    /* Track monster health at initial cursor position for left sidebar display */
    int initial_m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
    if ((initial_m_idx > 0)
        && unified_look_can_show_monster_at(state.cursor_y, state.cursor_x))
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
        compact_look_layout = unified_look_use_compact_layout();

        if (need_redraw)
        {
            unified_look_redraw_overlay(&state, controller_controls,
                compact_look_layout, &overlay_saved, selection_redraw, true);
            selection_redraw = false;
            need_redraw = false;
        }
        
        /* Get input */
        query = inkey();
        log_trace("Unified look key input: '%c' (%d) [char: %c, isupper: %d]", 
                 query, (int)query, (query >= 32 && query <= 126) ? query : '?', 
                 (query >= 'A' && query <= 'Z') ? 1 : 0);

        bool pointer_click_pending = ui_menu_click_has_pending();

        /* Keep the overlay live while cycling sidebar selection to avoid
         * flashing back to the map between adjacent redraws. */
        if (overlay_saved
            && query != '\t'
            && query != '`'
            && query != 'q'
            && query != UI_MENU_CLICK_WAKE_KEY
            && !((query == '\r') && pointer_click_pending)
            && !(controller_controls && (query == 'i' || query == 'e')))
        {
            (void)Term_set_extra_cursor(false, 0, 0, false);
            screen_load();
            overlay_saved = false;
            
            /* Update health bar display after screen restore */
            handle_stuff();
        }
        
        /* Analyze input */
        log_trace("Processing key: '%c' (%d), backtick is %d", query, (int)query, (int)'`');
        switch (query)
        {
            case UI_MENU_CLICK_WAKE_KEY:
            {
                int zoom_scale = 0;
                int pan_dy = 0;
                int pan_dx = 0;
                int target_y = 0;
                int target_x = 0;
                int describe_y = 0;
                int describe_x = 0;
                bool prompt_hover_redraw = false;
                bool hover_redraw = ui_menu_click_take_hover_redraw();

                if (sdl_unified_look_take_main_zoom(&zoom_scale))
                {
                    if (overlay_saved)
                    {
                        (void)Term_set_extra_cursor(false, 0, 0, false);
                        screen_load_quiet();
                        overlay_saved = false;
                        handle_stuff();
                    }

                    (void)unified_look_apply_main_zoom(zoom_scale,
                        &need_redraw);
                    break;
                }
                if (sdl_unified_look_take_map_pan(&pan_dy, &pan_dx))
                {
                    bool pan_moves = unified_look_map_pan_would_move(pan_dy,
                        pan_dx);

                    if (!pan_moves)
                    {
                        if (!overlay_saved)
                        {
                            unified_look_redraw_overlay(&state,
                                controller_controls, compact_look_layout,
                                &overlay_saved, false, true);
                        }
                        break;
                    }

                    if (overlay_saved)
                    {
                        (void)Term_set_extra_cursor(false, 0, 0, false);
                        screen_load_quiet();
                        overlay_saved = false;
                        handle_stuff();
                    }

                    if (unified_look_apply_map_pan(&state, pan_dy, pan_dx,
                            &need_redraw, &selection_redraw))
                    {
                        unified_look_redraw_overlay(&state,
                            controller_controls, compact_look_layout,
                            &overlay_saved, false, true);
                        selection_redraw = false;
                        need_redraw = false;
                    }
                    break;
                }
                if (sdl_unified_look_take_map_target(&target_y, &target_x))
                {
                    if (overlay_saved)
                    {
                        (void)Term_set_extra_cursor(false, 0, 0, false);
                        screen_load();
                        overlay_saved = false;
                        handle_stuff();
                    }

                    (void)unified_look_apply_map_cell(&state, target_y,
                        target_x, compact_look_layout, &need_redraw,
                        &selection_redraw);
                    if (unified_look_target_monster_at(target_y, target_x))
                        done = true;
                    else
                        need_redraw = true;
                    selection_redraw = false;
                    break;
                }
                if (sdl_unified_look_take_map_describe(&describe_y,
                        &describe_x))
                {
                    if (overlay_saved)
                    {
                        (void)Term_set_extra_cursor(false, 0, 0, false);
                        screen_load();
                        overlay_saved = false;
                        handle_stuff();
                    }

                    (void)unified_look_apply_map_cell(&state, describe_y,
                        describe_x, compact_look_layout, &need_redraw,
                        &selection_redraw);
                    if (!unified_look_examine_monster_at(describe_y,
                            describe_x, use_story_font)
                        && !unified_look_examine_object_at(describe_y,
                            describe_x, use_story_font))
                    {
                        bell("Nothing to examine.");
                    }
                    need_redraw = true;
                    selection_redraw = false;
                    break;
                }
                if (unified_look_apply_sidebar_pointer_action(&state,
                        compact_look_layout, &need_redraw, &selection_redraw,
                        &prompt_hover_redraw))
                {
                    if (prompt_hover_redraw)
                    {
                        unified_look_update_prompt_buttons_ex(
                            controller_controls, compact_look_layout,
                            state.look_mode != 0, state.nearby_filter, false);
                        unified_look_restore_map_cursor(&state);
                    }
                    break;
                }
                if (unified_look_apply_map_hover(&state, compact_look_layout,
                        &need_redraw, &selection_redraw))
                    break;

                if (hover_redraw)
                {
                    unified_look_update_prompt_buttons_ex(controller_controls,
                        compact_look_layout, state.look_mode != 0,
                        state.nearby_filter, false);
                    unified_look_restore_map_cursor(&state);
                }

                break;
            }

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
                unified_look_pause_pointer_handlers();
                
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
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            if (!screen_roff(m_ptr->r_idx, m_ptr))
                            {
                                /* Wait for input */
                                inkey();
                            }
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else if ((state.highlighted_entity_type == 2)
                        && unified_look_can_show_marked_object_at(
                            state.highlighted_y, state.highlighted_x))
                    {
                        /* Object was highlighted - examine object */
                        log_trace("EXAMINATION: Highlighted entity is object, examining object %d", cursor_o_idx);
                        /* Object examination */
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        (void)player_try_identify_smithing_object_on_examine(
                            o_ptr, false);
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
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            if (!screen_roff(m_ptr->r_idx, m_ptr))
                            {
                                /* Wait for input */
                                inkey();
                            }
                            
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
                    bool has_visible_monster = unified_look_can_show_monster_at(y, x);
                    bool has_object = unified_look_can_show_marked_object_at(y, x);
                    
                    log_trace("EXAMINATION: Cursor position (%d,%d) - has_visible_monster=%d, has_object=%d", 
                             y, x, has_visible_monster, has_object);
                    
                    /* Prioritize OBJECT first, then visible monster */
                    if (has_object)
                    {
                        log_trace("EXAMINATION: Examining object at cursor position");
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        (void)player_try_identify_smithing_object_on_examine(
                            o_ptr, false);
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
                        if (!screen_roff(m_ptr->r_idx, m_ptr))
                            inkey();
                        screen_load();
                    }
                    else
                    {
                        log_trace("EXAMINATION: No visible entities at cursor position");
                    }
                }
                unified_look_resume_pointer_handlers();
                if (use_story_font)
                    sdl_story_font_enable();

                need_redraw = true;
                break;
            }

            case '+':
            {
                (void)unified_look_apply_main_zoom(
                    get_sdl_main_view_scale() + 1, &need_redraw);
                break;
            }

            case '-':
            {
                (void)unified_look_apply_main_zoom(
                    get_sdl_main_view_scale() - 1, &need_redraw);
                break;
            }
            
            case '[':            /* View monsters */
            case ']':            /* View objects */
            case 'w':            /* Wield/Wear */
            case 'd':            /* Drop */
            case 'k':            /* Destroy */
            case 'r':            /* Read scroll */
            case 'a':            /* Activate */
            case 'z':            /* Zap rod */
            case '.':            /* Run */
            case ',':            /* Stay */
            case '<':            /* Go up stairs */
            case '>':            /* Go down stairs */
            case 'g':            /* Get/Pickup */
            case 'c':            /* Close */
            case 'j':            /* Jam */
            case '*':            /* Target */
            case '@':            /* Center map */
            case '(':            /* Dungeon history */
            case '|':            /* Screenshots */
            case '~':            /* Various things */
            case '!':            /* OS command */
command_key:
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
                            int max_wy = MAX(p_ptr->cur_map_hgt - SCREEN_HGT, 0);
                            int max_wx = MAX(p_ptr->cur_map_wid - SCREEN_WID, 0);
                            
                            /* Keep the viewport within the map even if it shows unknown space. */
                            if (new_wy < 0) new_wy = 0;
                            if (new_wy > max_wy)
                                new_wy = max_wy;
                            if (new_wx < 0) new_wx = 0;
                            if (new_wx > max_wx)
                                new_wx = max_wx;
                            
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
                        if ((m_idx > 0)
                            && unified_look_can_show_monster_at(state.cursor_y,
                                state.cursor_x))
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
                            if ((m_idx > 0)
                                && unified_look_can_show_monster_at(state.cursor_y,
                                    state.cursor_x))
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
            case 'i':  /* I key = nearby filter on keyboard, forward cycling in portable UI */
            {
                if (query == 'i' && !controller_controls)
                {
                    state.nearby_filter = !state.nearby_filter;
                    state.selected_entity = -1;
                    state.in_sidebar_mode = false;
                    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                    {
                        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                        state.highlighted_y = -1;
                        state.highlighted_x = -1;
                        state.highlighted_entity_type = 0;
                    }

                    handle_stuff();
                    need_redraw = true;
                    break;
                }
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
                selection_redraw = !compact_look_layout;
                break;
            }
            
            case '`': /* Backtick key - reverse Tab cycling */
            case 'q': /* Q key - reverse Tab cycling */
            case 'e': /* E key - reverse Tab cycling in portable UI */
            {
                if (query == 'e' && !controller_controls)
                    goto command_key;
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
                selection_redraw = !compact_look_layout;
                break;
            }
            
            case '\r': /* Enter key */
            case ' ':
            {
                if (unified_look_apply_sidebar_pointer_action(&state,
                        compact_look_layout, &need_redraw, &selection_redraw,
                        NULL))
                    break;

                log_trace("EXAMINATION: Enter/Space key pressed for examination");
                
                /* Disable story font for info screens */
                if (use_story_font)
                    sdl_story_font_disable();
                unified_look_pause_pointer_handlers();
                
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
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            if (!screen_roff(m_ptr->r_idx, m_ptr))
                            {
                                /* Wait for input */
                                inkey();
                            }
                            
                            /* Restore screen */
                            screen_load();
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else if ((state.highlighted_entity_type == 2)
                        && unified_look_can_show_marked_object_at(
                            state.highlighted_y, state.highlighted_x))
                    {
                        /* Object was highlighted - examine object */
                        log_trace("EXAMINATION: Highlighted entity is object, examining object %d", cursor_o_idx);
                        /* Object examination */
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        (void)player_try_identify_smithing_object_on_examine(
                            o_ptr, false);
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
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            /* Save screen */
                            screen_save();
                            
                            /* Show monster recall */
                            if (!screen_roff(m_ptr->r_idx, m_ptr))
                            {
                                /* Wait for input */
                                inkey();
                            }
                            
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
                    bool has_visible_monster = unified_look_can_show_monster_at(y, x);
                    bool has_object = unified_look_can_show_marked_object_at(y, x);
                    
                    log_trace("EXAMINATION: Cursor position (%d,%d) - has_visible_monster=%d, has_object=%d", 
                             y, x, has_visible_monster, has_object);
                    
                    /* Prioritize OBJECT first, then visible monster */
                    if (has_object)
                    {
                        log_trace("EXAMINATION: Examining object at cursor position");
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        (void)player_try_identify_smithing_object_on_examine(
                            o_ptr, false);
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
                        if (!screen_roff(m_ptr->r_idx, m_ptr))
                            inkey();
                        screen_load();
                    }
                    else
                    {
                        log_trace("EXAMINATION: No visible entities at cursor position");
                    }
                }
                
                /* Re-enable story font */
                unified_look_resume_pointer_handlers();
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
            
            case 'u':
                if (!controller_controls)
                    goto command_key;
                if (compact_look_layout)
                    goto cycle_display_modes;
                /* fallthrough */
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
cycle_display_modes:
            {
                log_trace("'l' key pressed - cycling through display modes");

                if (compact_look_layout)
                {
                    /* Compact cycle: both -> objects -> monsters -> both. */
                    if (state.show_monsters && state.show_objects)
                    {
                        state.show_monsters = false;
                        state.show_objects = true;
                        log_trace("Mode changed to: objects only");
                    }
                    else if (!state.show_monsters && state.show_objects)
                    {
                        state.show_monsters = true;
                        state.show_objects = false;
                        log_trace("Mode changed to: monsters only");
                    }
                    else
                    {
                        state.show_monsters = true;
                        state.show_objects = true;
                        log_trace("Mode changed to: both monsters and objects");
                    }
                }
                else if (state.show_monsters && state.show_objects)
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
            
            case 'f':
                if (!controller_controls)
                    goto command_key;
                /* fall through */
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
                if ((m_idx > 0) && unified_look_can_show_monster_at(target_y, target_x))
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
                unified_look_pan_player_for_sidebar(true);
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

    (void)Term_set_extra_cursor(false, 0, 0, false);
    ui_menu_click_clear();
    sdl_unified_look_set_active(false);
    sdl_unified_look_set_map_hover_enabled(false);

    if (overlay_saved)
    {
        screen_load();
        overlay_saved = false;
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
    
    if (look_adjusts_left_panel)
    {
        g_hide_left_panel = original_hide_left_panel;
        g_suppress_hidden_left_panel_overlay
            = original_suppress_hidden_left_panel_overlay;
        p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA | PR_HEALTHBAR | PR_MAP);
        p_ptr->window |= (PW_OVERHEAD);
    }
    if (look_adjusts_supporting_panes)
    {
        screen_pop_supporting_panes_hidden();
        if (op_ptr)
            op_ptr->opt[OPT_hide_supporting_panes_fullscreen]
                = original_hide_supporting_panes_fullscreen;
        p_ptr->redraw |= (PR_BASIC | PR_LIGHT | PR_EXTRA | PR_HEALTHBAR | PR_MAP);
        p_ptr->window |= (PW_OVERHEAD);
    }

    /* Restore original viewport */
    bool viewport_changed = (p_ptr->wy != original_wy)
        || (p_ptr->wx != original_wx);

    if (viewport_changed)
    {
        p_ptr->wy = original_wy;
        p_ptr->wx = original_wx;
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_OVERHEAD);
    }

    handle_stuff();
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
        object_type* floor_obj = NULL;
        bool has_live_object = false;

        if (cave_o_idx[y][x] > 0)
        {
            floor_obj = &o_list[cave_o_idx[y][x]];
            has_live_object = floor_obj->k_idx ? true : false;
        }

        /* Determine what to display based on entity_type preference */
        /* entity_type: 0=auto-detect, 1=prefer monster, 2=prefer object */

        if (entity_type == 2 && has_live_object)
        {
            /* Prefer object display */
            display_char = object_char(floor_obj);
            display_attr = object_attr(floor_obj); /* Keep original object color */
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
        else if (has_live_object)
        {
            /* Auto-detect: For objects, show normal appearance (no color change) */
            display_char = object_char(floor_obj);
            display_attr = object_attr(floor_obj); /* Keep original object color */
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

    /* Block when hallucinating */
    if (p_ptr->image)
    {
        msg_print("Your vision is too distorted to map your location.");
        return;
    }

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
            query = 0;

            /* Recall (raging players don't get recall) */
            if (recall)
            {
                int recall_key;

                /* Save screen */
                screen_save();

                /* Recall on screen */
                recall_key = screen_roff(who[i], NULL);

                if (recall_key)
                {
                    query = (char)recall_key;
                }
                else
                {
                    /* Hack -- Complete the prompt (again) */
                    Term_addstr(-1, TERM_WHITE, " [(r)ecall, ESC]");
                }
            }

            /* Command */
            if (!recall || !query)
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
