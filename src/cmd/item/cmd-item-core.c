#include "angband.h"
#include "externs.h"
#include "cmd/world/cmd-interact-chest.h"
#include "log/log.h"
#include "metarun.h"
#include "object/object-ui-select.h"
#include "ui/question.h"

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
    /* Literal (broken) items cannot be equipped until repaired. */
    if (object_has_broken_prefix(o_ptr))
        return (false);

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
static int forced_wield_slot = -1;

static bool forced_wield_slot_accepts_object(const object_type* o_ptr,
    int forced_slot)
{
    int natural_slot;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (forced_slot < INVEN_WIELD || forced_slot >= INVEN_TOTAL)
        return false;

    natural_slot = wield_slot(o_ptr);
    if (natural_slot == forced_slot)
        return true;

    switch (forced_slot)
    {
    case INVEN_WIELD:
        return player_can_treat_as_throwing(o_ptr);
    case INVEN_LEFT:
    case INVEN_RIGHT:
        return o_ptr->tval == TV_RING;
    case INVEN_NECK:
        return o_ptr->tval == TV_AMULET;
    case INVEN_QUIVER1:
    case INVEN_QUIVER2:
        return (o_ptr->tval == TV_ARROW)
            || player_can_treat_as_throwing(o_ptr);
    default:
        return false;
    }
}

static int first_floor_item_under_player(void)
{
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    floor_num = scan_floor(
        floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);

    for (int i = 0; i < floor_num; i++)
    {
        int o_idx = floor_list[i];
        const object_type* o_ptr;

        if (o_idx <= 0 || o_idx >= o_max)
            continue;

        o_ptr = &o_list[o_idx];
        if (!o_ptr->k_idx)
            continue;

        if (object_is_searched_skeleton(o_ptr))
            continue;

        return 0 - o_idx;
    }

    return 0;
}

/*
 * Return the first floor object under the player whose primary action is an
 * interaction rather than pickup.  Space normally expands to the "/5"
 * interact-here keymap, but touch shortcuts resolve Space contextually before
 * request_command() sees it.  Keep skeletons and unopened chests on the
 * interaction path instead of rewriting that shortcut to 'g'.
 */
static int first_floor_interaction_under_player(void)
{
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    floor_num = scan_floor(
        floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);

    for (int i = 0; i < floor_num; i++)
    {
        int o_idx = floor_list[i];
        const object_type* o_ptr;

        if (o_idx <= 0 || o_idx >= o_max)
            continue;

        o_ptr = &o_list[o_idx];
        if (!o_ptr->k_idx)
            continue;
        if (o_ptr->tval == TV_SKELETON
            && !object_is_searched_skeleton(o_ptr))
        {
            return 0 - o_idx;
        }
        if (o_ptr->tval == TV_CHEST && o_ptr->pval != 0)
            return 0 - o_idx;
    }

    return 0;
}

cptr item_use_action_name(const object_type* o_ptr, int item)
{
    if (!o_ptr)
        return "Use";

    if (o_ptr->name1 >= ART_MORGOTH_1 && o_ptr->name1 <= ART_MORGOTH_3)
        return "Prise";

    if (o_ptr->tval == TV_SKELETON)
        return "Search";

    if (o_ptr->tval == TV_CHEST)
    {
        if (chest_trap_minigame && o_ptr->pval != 0)
            return "Handle";
        if (o_ptr->pval > 0 && object_chest_trap_flags(o_ptr)
            && object_known_p(o_ptr))
        {
            return "Disarm";
        }
        return "Open";
    }

    if (item < INVEN_WIELD
        && inventory[INVEN_LITE].tval == TV_LIGHT
        && inventory[INVEN_LITE].sval == SV_LIGHT_LANTERN
        && (o_ptr->tval == TV_FLASK
            || (o_ptr->tval == TV_LIGHT
                && o_ptr->sval == SV_LIGHT_LANTERN)))
    {
        return "Refuel";
    }

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
        return (item >= INVEN_WIELD && item < INVEN_TOTAL) ? "Take Off"
                                                           : "Wield";
    case TV_FLASK:
        return "Use";
    case TV_NOTE:
        return "Read";
    case TV_STAFF:
        return (item >= INVEN_WIELD && item < INVEN_TOTAL) ? "Activate"
                                                           : "Wield";
    case TV_HORN:
        return "Play";
    case TV_POTION:
        return "Quaff";
    case TV_FOOD:
        return "Eat";
    default:
        return "Use";
    }
}

/*
 * Perform the targeted floor interaction promised by the unified Use action.
 * This is deliberately limited to the player's square: carried chests and
 * skeletons still have to be put down before they can be opened or searched.
 */
static bool use_floor_interaction_by_index(int item)
{
    int o_idx;
    object_type* o_ptr;

    if (item >= 0)
        return false;

    o_idx = 0 - item;
    if (o_idx <= 0 || o_idx >= o_max)
        return false;

    o_ptr = &o_list[o_idx];
    if (!o_ptr->k_idx || o_ptr->iy != p_ptr->py || o_ptr->ix != p_ptr->px)
        return false;

    if (o_ptr->tval == TV_SKELETON
        && !object_is_searched_skeleton(o_ptr))
    {
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;
        do_cmd_search_skeleton(p_ptr->py, p_ptr->px, o_idx);
        return true;
    }

    if (o_ptr->tval == TV_CHEST && o_ptr->pval != 0)
    {
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;

        if (chest_trap_minigame)
        {
            (void)do_cmd_open_chest(p_ptr->py, p_ptr->px, o_idx);
        }
        else if (o_ptr->pval > 0 && object_chest_trap_flags(o_ptr)
            && object_known_p(o_ptr))
        {
            (void)do_cmd_disarm_chest(p_ptr->py, p_ptr->px, o_idx);
        }
        else
        {
            (void)do_cmd_open_chest(p_ptr->py, p_ptr->px, o_idx);
        }
        return true;
    }

    return false;
}

/*
 * Context-sensitive interpretation of touch shortcut bindings, so the
 * on-screen button shows (and performs) the action that fits the player's
 * current situation:
 *
 *   Confirm: in an open description -> pick up ('g'/space) the shown item;
 *          otherwise on stairs -> interact-here with confirmation, on a forge
 *          -> smith, standing on an item -> pick up ('g'), else -> confirm.
 *   '<'/'>' : on the matching staircase -> interact-here with confirmation.
 *   'u'  : names the floor item's real action (Wield, Quaff, Eat, etc.); in an
 *          open description it submits that popup's 'x' action.
 *   'x'  : "Description" until the description popup is open, then names the
 *          floor item's real action.  The key stays 'x' for the popup action.
 *
 * `description_open` is the caller's "an interactive item description popup is
 * showing" state.  Returns true (filling *out_key and label) for contextual
 * bindings while in the dungeon; false otherwise, so the caller keeps the
 * binding's static label/key.
 */
bool touch_shortcut_context_action(int binding, bool description_open,
    int* out_key, char* label, size_t label_len)
{
    int key = binding;
    int floor_interaction = 0;
    const char* name = NULL;

    if (!character_dungeon || !p_ptr || !p_ptr->playing || p_ptr->is_dead)
        return false;

    if (binding == ' ') {
        if (description_open) {
            /* Inside the popup, Space picks up the item being described. */
            key = ' ';
            name = (first_floor_item_under_player() != 0) ? "Pick Up"
                                                          : "Confirm";
        } else if (cave_down_stairs_bold(p_ptr->py, p_ptr->px)) {
            /* Space runs interact-here, which asks before changing levels. */
            key = ' ';
            name = "Go Down";
        } else if (cave_up_stairs_bold(p_ptr->py, p_ptr->px)) {
            /* Space runs interact-here, which asks before changing levels. */
            key = ' ';
            name = "Go Up";
        } else if (cave_forge_bold(p_ptr->py, p_ptr->px)) {
            /* Space runs interact-here, which opens the smithing screen. */
            key = ' ';
            name = "Smith";
        } else if ((floor_interaction =
                first_floor_interaction_under_player()) != 0)
        {
            /*
             * Leave Space intact so request_command() applies its normal
             * "/5" interact-here keymap.
             */
            key = ' ';
            name = item_use_action_name(&o_list[-floor_interaction],
                floor_interaction);
        } else if (first_floor_item_under_player() != 0) {
            key = 'g';
            name = "Pick Up";
        } else {
            key = ' ';
            name = "Confirm";
        }
    } else if (binding == '<') {
        if (!cave_up_stairs_bold(p_ptr->py, p_ptr->px))
            return false;
        /* Keep Space so the interact-here command supplies the confirmation. */
        key = ' ';
        name = "Go Up";
    } else if (binding == '>') {
        if (!cave_down_stairs_bold(p_ptr->py, p_ptr->px))
            return false;
        /* Keep Space so the interact-here command supplies the confirmation. */
        key = ' ';
        name = "Go Down";
    } else if (binding == 'u') {
        int floor_item = first_floor_item_under_player();

        key = (description_open && floor_item != 0) ? 'x' : 'u';
        name = (floor_item != 0)
            ? item_use_action_name(&o_list[-floor_item], floor_item)
            : "Use";
    } else if (binding == 'x') {
        key = 'x';
        if (description_open) {
            int floor_item = first_floor_item_under_player();

            if (floor_item != 0) {
                const object_type* o_ptr = &o_list[-floor_item];

                if ((o_ptr->tval == TV_SKELETON
                        && !object_is_searched_skeleton(o_ptr))
                    || (o_ptr->tval == TV_CHEST && o_ptr->pval != 0))
                {
                    name = item_use_action_name(o_ptr, floor_item);
                }
                else
                {
                    name = item_use_action_name(o_ptr, floor_item);
                }
            } else {
                name = "Use";
            }
        } else {
            name = "Description";
        }
    } else {
        return false;
    }

    if (out_key)
        *out_key = key;
    if (label && label_len)
        strnfmt(label, label_len, "%s", name);
    return true;
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

bool open_inventory_menu_page(supply_menu_page page)
{
    supply_menu_request request = {0};

    request.focus_page = true;
    request.page = page;

    return do_cmd_knowledge_supplies(&request);
}

bool open_inventory_menu_category(inventory_menu_group group)
{
    supply_menu_request request = {0};

    request.focus_page = true;
    request.page = SUPPLY_MENU_PAGE_INVENTORY;
    request.focus_inventory_group = true;
    request.inventory_group = group;

    return do_cmd_knowledge_supplies(&request);
}

static bool replacement_choice_type_matches(const object_type* incoming,
    const object_type* candidate)
{
    if (!incoming || !candidate || !candidate->k_idx)
        return false;

    if (player_oil_container_object(incoming)
        && player_oil_container_object(candidate))
    {
        return true;
    }

    if (incoming->tval == candidate->tval)
        return true;

    {
        int incoming_slot = wield_slot(incoming);

        if (incoming_slot >= INVEN_WIELD && incoming_slot < INVEN_TOTAL)
        {
            int candidate_slot = wield_slot(candidate);

            if (candidate_slot == incoming_slot)
                return true;
        }
    }

    return false;
}

static bool replacement_choice_allowed(const object_type* incoming,
    const object_type* candidate, bool equipped, bool include_equip)
{
    if (!incoming || !incoming->k_idx || !candidate || !candidate->k_idx)
        return false;

    if (equipped)
    {
        if (!include_equip)
            return false;
        if (cursed_p(candidate))
            return false;
    }

    /* Oil flasks are expendable lamp capacity, not a player-facing choice. */
    if (incoming->tval == TV_LIGHT && incoming->sval == SV_LIGHT_LANTERN
        && candidate->tval == TV_FLASK)
    {
        return false;
    }

    if (!inven_carry_limit_can_replace(candidate))
        return false;

    return replacement_choice_type_matches(incoming, candidate);
}

bool open_inventory_replacement_menu(inventory_menu_group group,
    const object_type* incoming, bool include_equip, bool include_supplies,
    cptr reason, int* replacement_item)
{
    object_choice_entry entries[OBJECT_CHOICE_MAX_ENTRIES];
    int count = 0;
    int selected = -1;
    char desc[480];
    char incoming_name[120];
    char incoming_summary[160];

    (void)group;

    if (replacement_item)
        *replacement_item = -1;

    if (!incoming || !incoming->k_idx || !replacement_item)
        return false;

    if (include_equip)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL
             && count < OBJECT_CHOICE_MAX_ENTRIES; i++)
        {
            if (!replacement_choice_allowed(incoming, &inventory[i], true,
                    include_equip))
            {
                continue;
            }

            object_choice_entry_make(&entries[count], i, &inventory[i],
                NULL, NULL);
            count++;
        }
    }

    for (int i = 0; i < INVEN_PACK && count < OBJECT_CHOICE_MAX_ENTRIES; i++)
    {
        if (!replacement_choice_allowed(incoming, &inventory[i], false,
                include_equip))
        {
            continue;
        }

        object_choice_entry_make(&entries[count], i, &inventory[i], NULL,
            NULL);
        count++;
    }

    if (include_supplies)
    {
        for (int i = 0; i < supplies_entry_count()
             && count < OBJECT_CHOICE_MAX_ENTRIES; i++)
        {
            object_type* o_ptr = supplies_entry_at(i);

            if (!replacement_choice_allowed(incoming, o_ptr, false,
                    include_equip))
            {
                continue;
            }

            object_choice_entry_make(&entries[count], SUPPLIES_INDEX + i,
                o_ptr, NULL, NULL);
            count++;
        }
    }

    if (count <= 0)
        return false;

    object_desc(incoming_name, sizeof(incoming_name), incoming, true, 3);
    if (show_weights)
    {
        int incoming_weight = incoming->weight * MAX(incoming->number, 1);

        strnfmt(incoming_summary, sizeof(incoming_summary), "%s  %2d.%1d lb",
            incoming_name, incoming_weight / 10, incoming_weight % 10);
    }
    else
    {
        SDL_strlcpy(incoming_summary, incoming_name,
            sizeof(incoming_summary));
    }
    if (reason && reason[0])
    {
        strnfmt(desc, sizeof(desc), "%s\nPicking up: %s", reason,
            incoming_summary);
    }
    else
    {
        strnfmt(desc, sizeof(desc), "Picking up: %s", incoming_summary);
    }

    if (!object_choice_overlay("What to replace?", desc, entries, count, 0,
            &selected))
    {
        return false;
    }

    if (selected < 0 || selected >= count)
        return false;

    *replacement_item = entries[selected].item;
    return true;
}

bool open_inventory_slot_pick_menu(const object_type* incoming,
    const bool* enabled, cptr reason, int* slot_out)
{
    supply_menu_request request = {0};

    if (slot_out)
        *slot_out = -1;

    request.focus_page = true;
    request.page = SUPPLY_MENU_PAGE_INVENTORY;
    request.preview_inventory_description = true;
    request.slot_pick_mode = true;
    request.slot_pick_incoming = incoming;
    request.slot_pick_enabled = enabled;
    request.slot_pick_reason = reason;
    request.slot_pick_item_out = slot_out;

    return do_cmd_knowledge_supplies(&request);
}

bool open_inventory_item_select_menu(int mode, cptr reason, cptr none_msg,
    int* item_out)
{
    bool selected;

    if (!item_out)
        return false;

    *item_out = -1;

    p_ptr->get_item_mode = mode;
    selected = object_item_select_overlay(mode, reason, none_msg, item_out);

    p_ptr->get_item_mode = 0;
    item_tester_tval = 0;
    item_tester_hook = NULL;
    p_ptr->command_wrk = 0;
    p_ptr->command_see = false;

    return selected;
}

static inventory_menu_group inventory_browser_group_for_object(
    const object_type* o_ptr)
{
    enum inventory_limit_group limit_group;
    inventory_menu_group limit_menu_group;

    if (!o_ptr || !o_ptr->k_idx)
        return INVENTORY_MENU_GROUP_ALL;

    limit_group = inventory_limit_group_for_object(o_ptr);
    limit_menu_group = inventory_menu_group_for_limit_group(limit_group);
    if (limit_menu_group != INVENTORY_MENU_GROUP_ALL)
        return limit_menu_group;

    switch (o_ptr->tval)
    {
    case TV_RING:       return INVENTORY_MENU_GROUP_RINGS;
    case TV_AMULET:     return INVENTORY_MENU_GROUP_AMULETS;
    case TV_BOW:        return INVENTORY_MENU_GROUP_BOWS;
    case TV_ARROW:      return INVENTORY_MENU_GROUP_ARROWS;
    case TV_STAFF:      return INVENTORY_MENU_GROUP_STAVES;
    case TV_HORN:       return INVENTORY_MENU_GROUP_HORNS;
    case TV_DIGGING:    return INVENTORY_MENU_GROUP_DIGGING;
    case TV_LIGHT:
    case TV_FLASK:      return INVENTORY_MENU_GROUP_LIGHTS;
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:      return INVENTORY_MENU_GROUP_WEAPONS;
    case TV_MAIL:       return INVENTORY_MENU_GROUP_ARMOUR;
    case TV_SOFT_ARMOR: return (o_ptr->sval == SV_ROBE)
                             ? INVENTORY_MENU_GROUP_CLOAKS
                             : INVENTORY_MENU_GROUP_ARMOUR;
    case TV_CLOAK:      return INVENTORY_MENU_GROUP_CLOAKS;
    case TV_SHIELD:     return INVENTORY_MENU_GROUP_SHIELDS;
    case TV_HELM:
    case TV_CROWN:      return INVENTORY_MENU_GROUP_HEADGEAR;
    case TV_GLOVES:     return INVENTORY_MENU_GROUP_GLOVES;
    case TV_BOOTS:      return INVENTORY_MENU_GROUP_BOOTS;
    default:            return INVENTORY_MENU_GROUP_OTHER;
    }
}

static int supply_browser_group_for_object(const object_type* o_ptr)
{
    if (supplies_group_matches_object(SUPPLY_GROUP_HERBS, o_ptr))
        return SUPPLY_GROUP_HERBS;
    if (supplies_group_matches_object(SUPPLY_GROUP_FOOD, o_ptr))
        return SUPPLY_GROUP_FOOD;
    if (supplies_group_matches_object(SUPPLY_GROUP_POTIONS, o_ptr))
        return SUPPLY_GROUP_POTIONS;
    if (supplies_group_matches_object(SUPPLY_GROUP_GEMS, o_ptr))
        return SUPPLY_GROUP_GEMS;
    if (supplies_group_matches_object(SUPPLY_GROUP_LIGHTS, o_ptr))
        return SUPPLY_GROUP_LIGHTS;

    return SUPPLY_GROUP_SUPPLY;
}

static bool open_inventory_menu_focused_on_floor(int floor_item,
    bool use_type_group, supply_floor_action floor_action)
{
    supply_menu_request request = {0};
    int floor_o_idx = 0 - floor_item;
    object_type* o_ptr;

    if (floor_o_idx <= 0 || floor_o_idx >= o_max)
        return open_inventory_menu_category(INVENTORY_MENU_GROUP_ALL);

    o_ptr = &o_list[floor_o_idx];
    if (!o_ptr->k_idx)
        return open_inventory_menu_category(INVENTORY_MENU_GROUP_ALL);

    request.focus_floor_item = true;
    request.floor_o_idx = floor_o_idx;
    request.floor_action = floor_action;

    if (supplies_is_supply_object(o_ptr))
    {
        request.focus_group = true;
        request.group = supply_browser_group_for_object(o_ptr);
    }
    else
    {
        request.focus_page = true;
        request.page = SUPPLY_MENU_PAGE_INVENTORY;
        request.focus_inventory_group = true;
        request.inventory_group = use_type_group
            ? inventory_browser_group_for_object(o_ptr)
            : INVENTORY_MENU_GROUP_ALL;
    }

    return do_cmd_knowledge_supplies(&request);
}

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

    if (use_floor_interaction_by_index(item))
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
    case TV_SKELETON:
    {
        msg_print("You would need to put it down to search it.");
        break;
    }
    case TV_STAFF:
    {
        /* A staff has to be equipped before the unified Use action activates
         * it.  Packed and floor staves therefore use the same Equip path as
         * other wearable items. */
        if (item >= INVEN_WIELD && item < INVEN_TOTAL)
            do_cmd_activate_staff(o_ptr, item);
        else
            do_cmd_wield(o_ptr, item);
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
    int floor_item = first_floor_item_under_player();

    if (floor_item)
    {
        log_debug(
            "do_cmd_use_item: Opening browser on floor item under player, item=%d",
            floor_item);
        (void)open_inventory_menu_focused_on_floor(floor_item, true,
            SUPPLY_FLOOR_ACTION_USE);
        return;
    }

    {
        extern char current_menu_command;
        extern int current_menu_state;

        current_menu_command = 0;
        current_menu_state = 0;
    }

    log_debug("do_cmd_use_item: No floor item, opening all inventory browser");
    (void)open_inventory_menu_category(INVENTORY_MENU_GROUP_ALL);
}

/*
 * Wrapper for wear/wield command with enhanced menu support
 */
void do_cmd_wield_wrapper(void)
{
    int floor_item = first_floor_item_under_player();

    log_debug("do_cmd_wield_wrapper: Opening all inventory browser");

    if (floor_item)
    {
        (void)open_inventory_menu_focused_on_floor(floor_item, false,
            SUPPLY_FLOOR_ACTION_WIELD);
        return;
    }

    (void)open_inventory_menu_category(INVENTORY_MENU_GROUP_ALL);
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
    if (dismiss_active_narrative_banner()) {
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
    if (dismiss_active_narrative_banner()) {
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
    log_debug("do_cmd_inven_direct: Opening inventory browser page");
    (void)open_inventory_menu_page(SUPPLY_MENU_PAGE_INVENTORY);
}

/*
 * Direct access equipment with cycling support
 */
void do_cmd_equip_direct(void)
{
    log_debug("do_cmd_equip_direct: Opening equipped browser page");
    (void)open_inventory_menu_page(SUPPLY_MENU_PAGE_EQUIPPED);
}

/*
 * Display inventory
 */
void do_cmd_inven(void)
{
    do_cmd_inven_direct();
}

/*
 * Display equipment
 */
void do_cmd_equip(void)
{
    do_cmd_equip_direct();
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
    int lamp_flasks_to_replace = 0;
    int lamp_flask_oil = 0;
    int lamp_replacement_item = -1;
    bool lamp_flask_replacement_planned = false;

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
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
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

    if (object_has_broken_prefix(o_ptr))
    {
        msg_print("Broken items must be repaired before they can be equipped.");
        return;
    }

    // remember how many there were
    original_quantity = o_ptr->number;

    if (!from_supplies && item < 0 && o_ptr->tval == TV_STAFF
        && player_channel_floor_staff(o_ptr, 0 - item))
    {
        return;
    }

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
    if (forced_wield_slot >= INVEN_WIELD && forced_wield_slot < INVEN_TOTAL)
    {
        if (!forced_wield_slot_accepts_object(o_ptr, forced_wield_slot))
        {
            if (item < 0)
                object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
            else
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
            msg_format("You cannot put %s there.", o_name);
            return;
        }

        slot = forced_wield_slot;
    }
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
    {
        if (item < 0)
            object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
        else
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        msg_format("You cannot wear or wield %s.", o_name);
        return;
    }

    if ((item < 0) && player_light_carry_cap(o_ptr) > 0
        && !(o_ptr->tval == TV_LIGHT
            && o_ptr->sval == SV_LIGHT_LANTERN))
    {
        object_type* equipped_ptr = &inventory[slot];
        bool replacing_same_group = equipped_ptr->k_idx
            && player_light_share_carry_group(o_ptr, equipped_ptr);

        if (!replacing_same_group && player_light_available_capacity(o_ptr) <= 0)
        {
            inventory_menu_group menu_group =
                inventory_menu_group_for_limit_group(
                    inventory_limit_group_for_object(o_ptr));

            if (player_oil_container_object(o_ptr))
                msg_print("You have no free lamp/flask slots.");
            else
                msg_print("You cannot carry any more of those.");
            if (menu_group != INVENTORY_MENU_GROUP_ALL)
                (void)open_inventory_menu_category(menu_group);
            return;
        }
    }

    /* Ask for ring to replace */
    if ((forced_wield_slot < 0) && (o_ptr->tval == TV_RING) && inventory[INVEN_LEFT].k_idx
        && inventory[INVEN_RIGHT].k_idx)
    {
        item_tester_tval = TV_RING;
        item_tester_hook = item_tester_hook_ring_slots;
        item_tester_full = false;

        q = "Replace which ring? ";
        s = "Oops.";
        if (!open_inventory_item_select_menu(USE_EQUIP, q, s, &slot))
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
        if ((forced_wield_slot == INVEN_WIELD
                || forced_wield_slot == INVEN_QUIVER1
                || forced_wield_slot == INVEN_QUIVER2)
            && forced_wield_slot_accepts_object(o_ptr, forced_wield_slot))
        {
            slot = forced_wield_slot;
            if ((slot == INVEN_QUIVER1 || slot == INVEN_QUIVER2)
                && inventory[slot].k_idx
                && object_similar(&inventory[slot], o_ptr))
            {
                combine = true;
            }
        }
        else
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

            /* Count the available destinations to decide if a choice is needed. */
            int throw_dest_count = 0;
            for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
                if (throw_slot_enabled[i])
                    throw_dest_count++;

            bool slot_selected;

            if (throw_dest_count <= 1)
            {
                /* Only one place it can go - no need to ask. */
                slot_selected = true;
            }
            else
            {
                /* Route the hand-or-quiver choice through the new inventory menu. */
                int chosen_slot = -1;

                slot_selected = open_inventory_slot_pick_menu(o_ptr,
                    throw_slot_enabled,
                    "Place this throwing weapon: the hand wields it, "
                    "a quiver lets you throw and swap it.",
                    &chosen_slot);

                if (slot_selected && chosen_slot >= INVEN_WIELD
                    && chosen_slot < INVEN_TOTAL && throw_slot_enabled[chosen_slot])
                    slot_choice = chosen_slot;
                else
                    slot_selected = false;
            }

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
    }
    else
    {
        // Special cases for merging arrows
        if (o_ptr->tval == TV_ARROW
            && ((forced_wield_slot == INVEN_QUIVER1)
                || (forced_wield_slot == INVEN_QUIVER2))
            && forced_wield_slot_accepts_object(o_ptr, forced_wield_slot))
        {
            slot = forced_wield_slot;
            if (inventory[slot].k_idx && object_similar(&inventory[slot], o_ptr))
                combine = true;
        }
        else if (object_similar(&inventory[INVEN_QUIVER1], o_ptr))
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

            bool slot_selected = open_inventory_slot_pick_menu(o_ptr,
                throw_slot_enabled,
                "Place arrows in a quiver.",
                &slot_choice);

            if (!slot_selected || slot_choice < INVEN_WIELD
                || slot_choice >= INVEN_TOTAL || !throw_slot_enabled[slot_choice])
            {
                slot_selected = false;
            }

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
    if ((forced_wield_slot < 0) && o_ptr->name1
        && inventory[INVEN_WIELD].k_idx)
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
    if ((forced_wield_slot < 0) && !paired_weapon_prompt
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

    if ((item >= INVEN_WIELD) && (item < INVEN_TOTAL) && (item != slot)
        && cursed_p(o_ptr))
    {
        object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
        msg_format("You cannot bear to move the %s you are %s.", o_name,
            describe_use(item));
        return;
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

    if (item < 0 && o_ptr->tval == TV_LIGHT
        && o_ptr->sval == SV_LIGHT_LANTERN)
    {
        bool replacement_aborted = false;

        lamp_flask_replacement_planned =
            prepare_brass_lamp_flask_replacement(o_ptr,
                &lamp_flasks_to_replace, &lamp_flask_oil,
                &replacement_aborted);
        if (replacement_aborted)
            return;

        if (!lamp_flask_replacement_planned
            && player_light_available_capacity(o_ptr) <= 0)
        {
            inventory_menu_group menu_group =
                inventory_menu_group_for_limit_group(
                    inventory_limit_group_for_object(o_ptr));

            (void)inven_carry_okay(o_ptr);
            msg_print("You have no free lamp/flask slots.");
            if (menu_group == INVENTORY_MENU_GROUP_ALL
                || !open_inventory_replacement_menu(menu_group, o_ptr, true,
                    true, "Replace a brass lantern to make room.",
                    &lamp_replacement_item))
            {
                return;
            }
        }
    }

    if (!from_supplies
        && o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_LANTERN
        && o_ptr->timeout > 0 && !lamp_flask_replacement_planned
        && lamp_replacement_item < 0
        && player_lamp_oil_would_overflow_with_bonus(o_ptr->timeout,
            (item < 0) ? 1 : 0)
        && !get_check("Taking this lamp will waste some oil. Proceed? "))
    {
        return;
    }
    
    /* Oath of Light: warn before equipping light-dimming items */
    if (chosen_oath(OATH_LIGHT) && !oath_invalid(OATH_LIGHT))
    {
        object_flags4(o_ptr, &f1, &f2, &f3, &f4);
        if ((f2 & TR2_DARKNESS) || (f4 & TR4_UNLIGHT))
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
            log_trace("do_cmd_wield: Oath of Light broken by equipping light-dimming item");
        }
    }

    if (lamp_flask_replacement_planned)
    {
        if (!commit_brass_lamp_flask_replacement(lamp_flasks_to_replace,
                lamp_flask_oil))
        {
            msg_print("The oil flask could not be replaced.");
            return;
        }
    }
    else if (lamp_replacement_item >= 0
        && lamp_replacement_item != slot)
    {
        if (lamp_replacement_item >= SUPPLIES_INDEX)
        {
            if (!supplies_drop_amount(
                    lamp_replacement_item - SUPPLIES_INDEX, 1))
            {
                msg_print("The brass lantern could not be replaced.");
                return;
            }
        }
        else if (lamp_replacement_item < INVEN_WIELD)
        {
            inven_drop(lamp_replacement_item, 1);
        }
        else
        {
            msg_print("That lantern cannot be replaced here.");
            return;
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

    /* Items with BREAKS_PERMA_CURSE can break the Oath of Fëanor on all equipped items */
    {
        u32b o_f1, o_f2, o_f3, o_f4;
        object_flags4(o_ptr, &o_f1, &o_f2, &o_f3, &o_f4);

        if (o_f4 & TR4_BREAKS_PERMA_CURSE)
        {
            int j;
            bool oath_broken = false;

            /* Check all equipped items for the Oath of Fëanor (perma-curse) */
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
                msg_print("The holy light breaks the Oath of Fëanor!");
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

    sdl_quick_access_suggest_equipped_item(o_ptr->tval);
}

void do_cmd_wield_to_slot(
    object_type* default_o_ptr, int default_item, int forced_slot)
{
    int old_forced_slot = forced_wield_slot;

    forced_wield_slot = forced_slot;
    do_cmd_wield(default_o_ptr, default_item);
    forced_wield_slot = old_forced_slot;
}

static int jewelry_preset_inventory_slot(int preset_slot)
{
    switch (preset_slot)
    {
    case JEWELRY_PRESET_SLOT_LEFT:
        return INVEN_LEFT;
    case JEWELRY_PRESET_SLOT_RIGHT:
        return INVEN_RIGHT;
    case JEWELRY_PRESET_SLOT_NECK:
        return INVEN_NECK;
    default:
        return -1;
    }
}

static int jewelry_preset_slot_for_inventory(int inventory_slot)
{
    switch (inventory_slot)
    {
    case INVEN_LEFT:
        return JEWELRY_PRESET_SLOT_LEFT;
    case INVEN_RIGHT:
        return JEWELRY_PRESET_SLOT_RIGHT;
    case INVEN_NECK:
        return JEWELRY_PRESET_SLOT_NECK;
    default:
        return -1;
    }
}

static bool jewelry_preset_objects_match(const object_type* a,
    const object_type* b)
{
    if (!a || !b || !a->k_idx || !b->k_idx)
        return false;

    if (a->k_idx != b->k_idx || a->tval != b->tval || a->sval != b->sval)
        return false;
    if (a->pval != b->pval || a->weight != b->weight)
        return false;
    if (a->name1 != b->name1)
        return false;
    if (object_ego_prefix(a) != object_ego_prefix(b)
        || object_ego_suffix(a) != object_ego_suffix(b))
        return false;
    if (a->att != b->att || a->evn != b->evn)
        return false;
    if (a->dd != b->dd || a->ds != b->ds
        || a->pd != b->pd || a->ps != b->ps)
        return false;
    if (a->abilities != b->abilities)
        return false;
    if (memcmp(a->stat_bonus, b->stat_bonus, sizeof(a->stat_bonus)) != 0)
        return false;
    if (memcmp(a->skill_bonus, b->skill_bonus, sizeof(a->skill_bonus)) != 0)
        return false;
    if (memcmp(a->skilltype, b->skilltype, sizeof(a->skilltype)) != 0)
        return false;
    if (memcmp(a->abilitynum, b->abilitynum, sizeof(a->abilitynum)) != 0)
        return false;
    if (memcmp(a->bane_type, b->bane_type, sizeof(a->bane_type)) != 0)
        return false;
    if (a->unused1 != b->unused1)
        return false;

    return true;
}

static bool jewelry_preset_current_slot_is_settled(int inventory_slot,
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX],
    const bool target_present[JEWELRY_PRESET_SLOT_MAX])
{
    int preset_slot = jewelry_preset_slot_for_inventory(inventory_slot);

    if (preset_slot < 0 || !target_present[preset_slot])
        return false;

    return jewelry_preset_objects_match(&inventory[inventory_slot],
        targets[preset_slot]);
}

static bool jewelry_preset_targets_available(
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX],
    const bool target_present[JEWELRY_PRESET_SLOT_MAX])
{
    bool used[INVEN_TOTAL];
    int jewelry_slots[JEWELRY_PRESET_SLOT_MAX] = {
        INVEN_LEFT, INVEN_RIGHT, INVEN_NECK
    };

    memset(used, 0, sizeof(used));

    for (int preset_slot = 0; preset_slot < JEWELRY_PRESET_SLOT_MAX;
         preset_slot++)
    {
        const object_type* target = targets[preset_slot];
        int preferred = jewelry_preset_inventory_slot(preset_slot);
        bool found = false;

        if (!target_present[preset_slot])
            continue;

        if (preferred >= 0 && !used[preferred]
            && jewelry_preset_objects_match(&inventory[preferred], target))
        {
            used[preferred] = true;
            continue;
        }

        for (int i = 0; i < JEWELRY_PRESET_SLOT_MAX && !found; i++)
        {
            int item = jewelry_slots[i];

            if (used[item])
                continue;
            if (jewelry_preset_objects_match(&inventory[item], target))
            {
                used[item] = true;
                found = true;
            }
        }

        for (int item = 0; item < INVEN_PACK && !found; item++)
        {
            if (used[item])
                continue;
            if (jewelry_preset_objects_match(&inventory[item], target))
            {
                used[item] = true;
                found = true;
            }
        }

        if (!found)
            return false;
    }

    return true;
}

static int jewelry_preset_find_source_for_target(int preset_slot,
    const object_type* target,
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX],
    const bool target_present[JEWELRY_PRESET_SLOT_MAX])
{
    int jewelry_slots[JEWELRY_PRESET_SLOT_MAX] = {
        INVEN_LEFT, INVEN_RIGHT, INVEN_NECK
    };
    int dest = jewelry_preset_inventory_slot(preset_slot);

    if (dest >= 0 && jewelry_preset_objects_match(&inventory[dest], target))
        return dest;

    for (int i = 0; i < JEWELRY_PRESET_SLOT_MAX; i++)
    {
        int item = jewelry_slots[i];

        if (item == dest)
            continue;
        if (jewelry_preset_current_slot_is_settled(item, targets,
                target_present))
            continue;
        if (jewelry_preset_objects_match(&inventory[item], target))
            return item;
    }

    for (int item = 0; item < INVEN_PACK; item++)
    {
        if (jewelry_preset_objects_match(&inventory[item], target))
            return item;
    }

    return -1;
}

static bool jewelry_preset_current_jewelry_can_move(
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX],
    const bool target_present[JEWELRY_PRESET_SLOT_MAX], bool report)
{
    int jewelry_slots[JEWELRY_PRESET_SLOT_MAX] = {
        INVEN_LEFT, INVEN_RIGHT, INVEN_NECK
    };

    for (int i = 0; i < JEWELRY_PRESET_SLOT_MAX; i++)
    {
        int item = jewelry_slots[i];
        object_type* o_ptr = &inventory[item];
        if (!o_ptr->k_idx)
            continue;
        if (jewelry_preset_current_slot_is_settled(item, targets,
                target_present))
            continue;
        if (!cursed_p(o_ptr))
            continue;

        if (report)
        {
            char o_name[80];

            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
            msg_format("You cannot bear to give up the %s you are %s.",
                o_name, describe_use(item));
        }
        return false;
    }

    return true;
}

static bool jewelry_preset_can_apply_now(int preset)
{
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX];
    bool target_present[JEWELRY_PRESET_SLOT_MAX];

    if (death_spectator_active() || preset < 0
        || preset >= JEWELRY_PRESET_MAX || !jewelry_preset_is_set(preset))
    {
        return false;
    }

    for (int slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
    {
        targets[slot] = jewelry_preset_object(preset, slot);
        target_present[slot] = targets[slot] && targets[slot]->k_idx;
        if (!target_present[slot])
            return false;
    }

    return jewelry_preset_targets_available(targets, target_present)
        && jewelry_preset_current_jewelry_can_move(
            targets, target_present, false);
}

bool do_cmd_jewelry_preset_apply(int preset)
{
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX];
    bool target_present[JEWELRY_PRESET_SLOT_MAX];
    bool changed = false;

    if (death_spectator_active())
    {
        msg_print("You can no longer take that action.");
        return false;
    }

    if (preset < 0 || preset >= JEWELRY_PRESET_MAX
        || !jewelry_preset_is_set(preset))
    {
        msg_format("Jewelry set %d is empty.", preset + 1);
        return false;
    }

    for (int slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
    {
        targets[slot] = jewelry_preset_object(preset, slot);
        target_present[slot] = targets[slot] && targets[slot]->k_idx;
    }

    if (!target_present[JEWELRY_PRESET_SLOT_LEFT]
        || !target_present[JEWELRY_PRESET_SLOT_RIGHT]
        || !target_present[JEWELRY_PRESET_SLOT_NECK])
    {
        msg_format("Jewelry set %d is incomplete.", preset + 1);
        return false;
    }

    if (!jewelry_preset_targets_available(targets, target_present))
    {
        msg_format("You no longer have all the items for jewelry set %d.",
            preset + 1);
        return false;
    }

    if (!jewelry_preset_current_jewelry_can_move(
            targets, target_present, true))
        return false;

    for (int slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
    {
        int dest = jewelry_preset_inventory_slot(slot);
        int source;

        if (dest < 0 || !target_present[slot])
            continue;

        if (jewelry_preset_objects_match(&inventory[dest], targets[slot]))
            continue;

        source = jewelry_preset_find_source_for_target(slot, targets[slot],
            targets, target_present);
        if (source < 0)
        {
            msg_format("You no longer have all the items for jewelry set %d.",
                preset + 1);
            return changed;
        }

        do_cmd_wield_to_slot(&inventory[source], source, dest);

        if (!jewelry_preset_objects_match(&inventory[dest], targets[slot]))
        {
            msg_format("Jewelry set %d could not be completed.", preset + 1);
            return changed;
        }

        changed = true;
    }

    if (changed)
        msg_format("Jewelry set %d equipped.", preset + 1);
    else
        msg_format("Jewelry set %d is already equipped.", preset + 1);

    return true;
}

bool do_cmd_jewelry_preset_store(int preset)
{
    if (preset < 0 || preset >= JEWELRY_PRESET_MAX)
        return false;

    if (death_spectator_active())
    {
        msg_print("You can no longer take that action.");
        return false;
    }

    if (jewelry_preset_is_set(preset)
        && !get_check(format("Replace jewelry set %d? ", preset + 1)))
    {
        return false;
    }

    if (!jewelry_preset_store_current(preset))
    {
        msg_print("Wear two rings and an amulet before saving a jewelry set.");
        return false;
    }

    msg_format("Jewelry set %d saved.", preset + 1);
    return true;
}

bool do_cmd_jewelry_preset_clear(int preset)
{
    if (preset < 0 || preset >= JEWELRY_PRESET_MAX)
        return false;

    if (!jewelry_preset_is_set(preset))
    {
        msg_format("Jewelry set %d is already empty.", preset + 1);
        return false;
    }

    if (!get_check(format("Clear jewelry set %d? ", preset + 1)))
        return false;

    jewelry_preset_clear(preset);
    msg_format("Jewelry set %d cleared.", preset + 1);
    return true;
}

void do_cmd_jewelry_preset_shortcut(void)
{
    ui_question_option options[JEWELRY_PRESET_MAX];
    char labels[JEWELRY_PRESET_MAX][32];
    int choice;

    for (int i = 0; i < JEWELRY_PRESET_MAX; i++)
    {
        bool available = jewelry_preset_can_apply_now(i);

        strnfmt(labels[i], sizeof(labels[i]), "Jewelry set %d%s", i + 1,
            jewelry_preset_is_set(i)
                ? (available ? "" : " (unavailable)") : " (empty)");
        options[i].key = (char)('1' + i);
        options[i].label = labels[i];
        options[i].attr = TERM_L_WHITE;
        options[i].disabled = !available;
    }

    choice = ui_question_ask("Wear which jewelry set?",
        "Grey choices cannot be worn right now.", options,
        JEWELRY_PRESET_MAX, UI_QUESTION_GLOBAL, UI_QUESTION_GLOBAL, 0);
    if (choice < 0)
        return;

    (void)do_cmd_jewelry_preset_apply(choice);
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
        if (!open_inventory_item_select_menu(USE_EQUIP, q, s, &item))
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

static bool confirm_drop_item_amount(object_type* o_ptr, int amt)
{
    object_type prompt_obj;
    char prompt_name[80];
    char prompt[120];

    if (!o_ptr || !o_ptr->k_idx || amt <= 0)
        return false;

    object_copy(&prompt_obj, o_ptr);
    prompt_obj.number = amt;
    object_desc(prompt_name, sizeof(prompt_name), &prompt_obj, false, 0);
    strnfmt(prompt, sizeof(prompt), "Drop %s? ", prompt_name);
    return get_check(prompt);
}

/*
 * Drop an item by index (for enhanced menus).  Returns false if the player
 * cancels quantity or confirmation prompts before anything is dropped.
 */
bool do_cmd_drop_item_by_index_confirm(int item, bool confirm)
{
    if (item == SUPPLIES_INDEX)
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_DROP, -1, false, true);
        return true;
    }

    int amt;
    object_type* o_ptr;
    char o_name[80];
    char quantity_prompt[160];

    /* Paranoia */
    if (item < 0 || item >= INVEN_TOTAL)
        return false;

    /* Get the item */
    o_ptr = &inventory[item];

    /* Nothing there */
    if (!o_ptr->k_idx)
        return false;

    /* Get a quantity */
    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
    strnfmt(quantity_prompt, sizeof(quantity_prompt), "Drop how many %s? ",
        o_name);
    amt = get_quantity_action(quantity_prompt, "Drop", o_ptr->number);

    /* Allow user abort */
    if (amt <= 0)
        return false;

    if (((item == INVEN_QUIVER1) || (item == INVEN_QUIVER2)) && cursed_p(o_ptr))
    {
        msg_print("You cannot bear to part with it.");
        return false;
    }

    if (confirm && !confirm_drop_item_amount(o_ptr, amt))
        return false;

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
            return false;
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 50;

    /* Drop (some of) the item */
    inven_drop(item, amt);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);

    return true;
}

/*
 * Drop an item by index (for enhanced menus)
 */
void do_cmd_drop_item_by_index(int item)
{
    (void)do_cmd_drop_item_by_index_confirm(item, false);
}

/*
 * Drop an item
 */
void do_cmd_drop(void)
{
    int item;

    if (!open_inventory_item_select_menu(USE_EQUIP | USE_INVEN,
            "Drop which item?", "You have nothing to drop.", &item))
    {
        return;
    }

    do_cmd_drop_item_by_index(item);
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

bool do_cmd_delete_item_by_index(int item)
{
    int amt;
    int old_number;
    int old_charges = 0;

    object_type* o_ptr;

    char o_name[80];
    char prompt_name[80];
    char quantity_name[80];
    char quantity_prompt[160];
    char prompt[160];

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        if (item >= INVEN_TOTAL)
            return false;
        o_ptr = &inventory[item];
    }

    /* Get the item (on the floor) */
    else
    {
        int o_idx = 0 - item;

        if (o_idx <= 0 || o_idx >= o_max)
            return false;
        o_ptr = &o_list[o_idx];
    }

    if (!o_ptr->k_idx)
        return false;

    if (handle_iron_crown_silmaril_action(o_ptr, item))
        return true;

    /* Get a quantity */
    if (item < 0)
        object_desc_floor(quantity_name, sizeof(quantity_name), o_ptr, false,
            0);
    else
        object_desc(quantity_name, sizeof(quantity_name), o_ptr, false, 0);
    strnfmt(quantity_prompt, sizeof(quantity_prompt), "Delete how many %s? ",
        quantity_name);
    amt = get_quantity_action(quantity_prompt, "Delete", o_ptr->number);

    /* Allow user abort */
    if (amt <= 0)
        return false;

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
    {
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
        object_desc_floor(prompt_name, sizeof(prompt_name), o_ptr, false, 0);
    }
    else
    {
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        object_desc(prompt_name, sizeof(prompt_name), o_ptr, false, 0);
    }

    /*reverse the hack*/
    o_ptr->number = old_number;

    strnfmt(prompt, sizeof(prompt), "Do you really want to DELETE %s? ",
        prompt_name);
    if (!get_check(prompt))
        return false;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Message */
    msg_format("You delete %s.", o_name);

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

    return true;
}

/*
 * Destroy an item
 */
void do_cmd_destroy(void)
{
    int item;
    object_type* o_ptr;

    item_tester_hook = item_tester_hook_destroy;

    // Special case for prising Silmarils from the Iron Crown of Morgoth
    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
    if (handle_iron_crown_silmaril_action(o_ptr, -1))
        return;

    if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR,
            "Delete which item?",
            "You have nothing to delete.", &item))
    {
        return;
    }

    (void)do_cmd_delete_item_by_index(item);
}

/*
 * Observe an item, displaying what is known about it
 */
void do_cmd_observe(void)
{
    supply_menu_request request = {0};
    int floor_item = first_floor_item_under_player();

    {
        extern char current_menu_command;
        extern int current_menu_state;

        current_menu_command = 0;
        current_menu_state = 0;
    }

    /* Shortcut: if standing on an item, examine it directly with comparisons
     * instead of opening the inventory browser. */
    if (floor_item != 0)
    {
        extern char current_menu_command;
        extern int current_menu_state;
        char action;

        log_debug(
            "do_cmd_observe: Examining floor item under player, item=%d",
            floor_item);
        current_menu_command = 'x';
        current_menu_state = 0;
        action = describe_item_with_floor_actions(floor_item, true);
        current_menu_command = 0;
        current_menu_state = 0;

        if (action == 'x')
        {
            current_menu_command = 'u';
            current_menu_state = 0;
            do_cmd_use_item_by_index(floor_item);
            current_menu_command = 0;
            current_menu_state = 0;
        }
        else if (action == ' ')
        {
            do_cmd_pickup();
        }

        return;
    }

    log_debug("do_cmd_observe: Opening inventory browser preview");
    request.focus_page = true;
    request.page = SUPPLY_MENU_PAGE_INVENTORY;
    request.focus_inventory_group = true;
    request.inventory_group = INVENTORY_MENU_GROUP_ALL;
    request.preview_inventory_description = true;
    (void)do_cmd_knowledge_supplies(&request);
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
    if (dismiss_active_narrative_banner()) {
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
    if (!open_inventory_item_select_menu(USE_EQUIP | USE_INVEN | USE_FLOOR,
            q, s, &item))
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
    if (!open_inventory_item_select_menu(USE_EQUIP | USE_INVEN | USE_FLOOR,
            q, s, &item))
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
    if (object_has_broken_prefix(o_ptr))
        return (false);

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
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
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

    if (object_has_broken_prefix(o_ptr))
    {
        msg_print("Broken items must be repaired before they can be used.");
        return;
    }

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

    if (object_has_broken_prefix(j_ptr))
    {
        msg_print("Broken items must be repaired before they can be used.");
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
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
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
