#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "ui/question.h"
#include <math.h>

static bool queue_deferred_pickup_pack_drop(int item, int amount, bool refill_oil_pool);

static void strip_brass_lantern_turns_suffix(char* o_name, const object_type* o_ptr)
{
    char* fuel_suffix;

    if (!o_name || !o_ptr)
        return;

    if (o_ptr->tval != TV_LIGHT || o_ptr->sval != SV_LIGHT_LANTERN)
        return;

    fuel_suffix = strstr(o_name, " (");
    if (fuel_suffix && strstr(fuel_suffix, " turns)"))
        *fuel_suffix = '\0';
}

void give_player_item(object_type * o_ptr)
{
    char o_name[80];
    object_type copy = *o_ptr;

    int slot = inven_carry(o_ptr, true);

    if (slot == SUPPLIES_INDEX)
    {
        object_desc(o_name, sizeof(o_name), &copy, true, 3);
        strip_brass_lantern_turns_suffix(o_name, &copy);
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("You add %s to your supplies (%c).", o_name, label);
        sound(MSG_PICK);
        return;
    }

    if (slot < 0)
        return;
    
    /* Play pickup sound */
    sound(MSG_PICK);

    /* reset the pointer to the new location to pick up the count of the item
       in the inventory */
    o_ptr = &inventory[slot];

    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    msg_format("You have %s (%c).", o_name, index_to_label(slot));

    /* Update quiver display if this was a throwing weapon or arrow */
    if ((slot == INVEN_QUIVER1 || slot == INVEN_QUIVER2) ||
        (copy.tval == TV_ARROW))
    {
        p_ptr->redraw |= (PR_QUIVER);
    }
}

/*
 * Check if an object was smithed by the player
 */
static const object_type* replacement_filter_incoming = NULL;
static bool item_tester_limit_group(const object_type* o_ptr);

static bool pack_item_matches_replacement_type(const object_type* incoming,
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

    int incoming_slot = wield_slot(incoming);
    if (incoming_slot >= INVEN_WIELD && incoming_slot < INVEN_TOTAL)
    {
        int candidate_slot = wield_slot(candidate);
        if (candidate_slot == incoming_slot)
            return true;
    }

    return false;
}

static void format_staff_prompt_name(char* buf, size_t max,
                                     const object_type* o_ptr, bool pref)
{
    char full[80];
    const char* staff_of;

    if (!buf || max == 0)
        return;

    buf[0] = '\0';

    if (!o_ptr || !o_ptr->k_idx)
        return;

    object_desc(full, sizeof(full), o_ptr, pref, 0);

    if (o_ptr->tval != TV_STAFF)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    staff_of = strstr(full, "Staff of ");
    if (!staff_of)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    if (!pref)
    {
        SDL_strlcpy(buf, staff_of, max);
        return;
    }

    if (!strncmp(full, "The ", 4))
        strnfmt(buf, max, "The %s", staff_of);
    else if (!strncmp(full, "no more ", 8))
        strnfmt(buf, max, "no more %s", staff_of);
    else
        strnfmt(buf, max, "a %s", staff_of);
}

static bool staff_channel_target_matches(const object_type* donor,
                                         const object_type* target)
{
    if (!donor || !target)
        return false;

    if (donor->tval != TV_STAFF)
        return false;

    if (!target->k_idx || target->tval != TV_STAFF)
        return false;

    return (target->k_idx == donor->k_idx) || (target->sval == donor->sval);
}

static object_type* find_staff_channel_target(const object_type* donor,
                                              int* target_slot)
{
    object_type* wielded = &inventory[INVEN_STAFF];

    if (target_slot)
        *target_slot = -1;

    if (staff_channel_target_matches(donor, wielded))
    {
        if (target_slot)
            *target_slot = INVEN_STAFF;
        return wielded;
    }

    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* pack_obj = &inventory[i];

        if (staff_channel_target_matches(donor, pack_obj))
        {
            if (target_slot)
                *target_slot = i;
            return pack_obj;
        }
    }

    return NULL;
}

bool player_channel_floor_staff(object_type* donor, int floor_o_idx)
{
    int target_slot;
    object_type* target;
    int mult = CHANNELING_CHARGE_MULTIPLIER;
    int existing_raw;
    int donor_raw;
    int existing_uses;
    int donor_uses;
    double existing_term;
    double donor_term;
    double sum_terms;
    double combined_uses_raw = 0.0;
    int combined_uses;
    long combined_pval;
    long max_pval;
    int gain_uses;
    char target_name[80];
    char donor_name[80];
    char prompt[120];

    if (!donor || !donor->k_idx || floor_o_idx <= 0 || floor_o_idx >= o_max
        || donor != &o_list[floor_o_idx])
    {
        return false;
    }

    if (!p_ptr->active_ability[S_WIL][WIL_CHANNELING])
        return false;

    if (donor->tval != TV_STAFF || donor->pval <= 0)
        return false;

    target = find_staff_channel_target(donor, &target_slot);
    if (!target)
        return false;

    existing_raw = MAX(target->pval, 0);
    donor_raw = MAX(donor->pval, 0);
    existing_uses = existing_raw / mult;
    donor_uses = donor_raw / mult;
    if (donor_uses <= 0)
        return false;

    existing_term = pow((double)existing_uses, 1.5);
    donor_term = pow((double)donor_uses, 1.5);
    sum_terms = existing_term + donor_term;
    if (sum_terms > 0.0)
        combined_uses_raw = pow(sum_terms, 2.0 / 3.0);

    combined_uses = (int)(combined_uses_raw + 0.5);
    combined_pval = (long)combined_uses * mult;
    max_pval = (long)(32767 / mult) * mult;
    if (combined_pval > max_pval)
        combined_pval = max_pval;

    combined_uses = (int)(combined_pval / mult);
    if (combined_uses <= existing_uses
        && existing_uses < (int)(max_pval / mult))
    {
        combined_uses = existing_uses + 1;
        combined_pval = (long)combined_uses * mult;
    }

    gain_uses = combined_uses - existing_uses;
    if (gain_uses <= 0)
        return false;

    format_staff_prompt_name(target_name, sizeof(target_name), target, false);
    format_staff_prompt_name(donor_name, sizeof(donor_name), donor, true);

    log_debug("Channeling: donor floor staff k_idx=%d pval=%d number=%d, target inv slot %d k_idx=%d pval=%d number=%d",
              donor->k_idx, donor->pval, donor->number,
              target_slot, target->k_idx, target->pval, target->number);

    strnfmt(prompt, sizeof(prompt),
        "Channel %s into your %s (%d charges)?",
        donor_name, target_name, combined_uses);
    if (!get_check(prompt))
        return false;

    target->pval = (s16b)combined_pval;
    target->ident &= ~(IDENT_EMPTY);
    donor->pval = 0;
    donor->ident |= IDENT_EMPTY;

    log_debug("Channeling complete: target now has pval=%d number=%d, donor has pval=%d number=%d",
              target->pval, target->number, donor->pval, donor->number);

    if (target_slot >= 0 && target_slot < INVEN_TOTAL)
        inven_item_charges(target_slot);
    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
    p_ptr->window |= (PW_EQUIP | PW_PLAYER_0 | PW_INVEN);
    msg_format("You channel %d charge%s into your %s (now %d).",
        gain_uses, (gain_uses == 1) ? "" : "s",
        target_name, combined_uses);
    delete_object_idx(floor_o_idx);

    log_debug("Channeling: deleted floor object idx %d", floor_o_idx);

    p_ptr->previous_action[0] = ACTION_MISC;
    p_ptr->energy_use = 100;

    return true;
}

bool is_smithed_by_player(const object_type* o_ptr)
{
    return (o_ptr->unused1 != 0);
}

/*
 * Prompt the player to drop an inventory item so a new object can be picked up.
 * Returns true if an item was dropped, false if the player declined or nothing was dropped.
 */
static bool prompt_replace_pack_item(const object_type* incoming)
{
    char incoming_name[80];
    char prompt[160];

    /* Ensure story font is disabled before showing messages */
    extern bool sdl_is_story_font_enabled(void);
    extern void sdl_story_font_disable(void);
    if (sdl_is_story_font_enabled())
        sdl_story_font_disable();

    object_desc(incoming_name, sizeof(incoming_name), incoming, true, 3);
    msg_format("No room for %s.", incoming_name);
    msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt), "Replace which item to pick up %s? ", incoming_name);

    while (true)
    {
        int item;

        if (!open_inventory_item_select_menu(USE_INVEN, prompt,
                "You have nothing to replace.", &item))
        {
            return false;
        }

        if ((item < 0) || (item >= INVEN_PACK))
        {
            bell("Illegal object choice!");
            continue;
        }

        object_type* drop_ptr = &inventory[item];

        if (!drop_ptr->k_idx)
        {
            bell("That slot is empty.");
            continue;
        }

        if (!queue_deferred_pickup_pack_drop(item, drop_ptr->number,
                player_oil_container_object(incoming)
                    && player_oil_container_object(drop_ptr)))
            continue;

        /* Let inventory housekeeping run before we attempt the pickup again */
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        notice_stuff();

        return true;
    }
}

static bool object_is_brass_lamp(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_LIGHT
        && o_ptr->sval == SV_LIGHT_LANTERN;
}

static bool object_is_oil_flask(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_FLASK;
}

static bool object_uses_light_pickup_limit(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && player_light_carry_cap(o_ptr) > 0;
}

typedef enum pickup_failure_result
{
    PICKUP_FAILURE_ABORT = 0,
    PICKUP_FAILURE_RETRY,
    PICKUP_FAILURE_EQUIPPED
} pickup_failure_result;

static bool deferred_pickup_drop_pending = false;
static object_type deferred_pickup_drop;
static int deferred_pickup_drop_oil = 0;
static bool deferred_pickup_refill_oil_pool = false;
static bool brass_lamp_pickup_overflow_checked = false;

static void clear_deferred_pickup_drop(void)
{
    deferred_pickup_drop_pending = false;
    object_wipe(&deferred_pickup_drop);
    deferred_pickup_drop_oil = 0;
    deferred_pickup_refill_oil_pool = false;
}

static void drop_object_at_player_feet_or_nearby(object_type* drop)
{
    bool can_drop_here;

    if (!drop || !drop->k_idx || drop->number <= 0)
        return;

    can_drop_here = (cave_feat[p_ptr->py][p_ptr->px] == FEAT_FLOOR
        || cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT);

    if (can_drop_here && floor_carry(p_ptr->py, p_ptr->px, drop) > 0)
        return;

    (void)drop_near(drop, 0, p_ptr->py, p_ptr->px);
}

static int refill_lamp_oil_from_deferred_drop(void)
{
    int current_oil;
    int free_capacity;
    int oil_to_transfer;

    if (!deferred_pickup_refill_oil_pool || deferred_pickup_drop_oil <= 0)
        return 0;

    current_oil = player_lamp_oil();
    free_capacity = player_lamp_oil_capacity() - current_oil;
    if (free_capacity <= 0)
        return 0;

    oil_to_transfer = MIN(deferred_pickup_drop_oil, free_capacity);
    if (oil_to_transfer <= 0)
        return 0;

    if (!player_gain_lamp_oil(oil_to_transfer, false))
        return 0;

    deferred_pickup_drop_oil -= oil_to_transfer;
    return oil_to_transfer;
}

static void flush_deferred_pickup_drop(void)
{
    if (!deferred_pickup_drop_pending)
        return;

    (void)refill_lamp_oil_from_deferred_drop();

    log_debug("pickup replace: flushing deferred drop at (%d,%d) "
        "cave_o_idx=%d tval=%d sval=%d number=%d oil=%d",
        p_ptr->py, p_ptr->px, cave_o_idx[p_ptr->py][p_ptr->px],
        deferred_pickup_drop.tval, deferred_pickup_drop.sval,
        deferred_pickup_drop.number,
        deferred_pickup_drop_oil);

    if (player_oil_container_object(&deferred_pickup_drop)
        && deferred_pickup_drop_oil > 0)
    {
        int oil_remaining = deferred_pickup_drop_oil;
        int unit_capacity =
            player_oil_container_unit_capacity(&deferred_pickup_drop);

        for (int n = 0; n < deferred_pickup_drop.number; n++)
        {
            object_type single_drop;
            object_wipe(&single_drop);
            object_copy(&single_drop, &deferred_pickup_drop);
            single_drop.number = 1;
            player_oil_container_set_fuel(&single_drop,
                MIN(oil_remaining, unit_capacity));
            oil_remaining -= MIN(oil_remaining, unit_capacity);
            drop_object_at_player_feet_or_nearby(&single_drop);
        }
    }
    else
    {
        drop_object_at_player_feet_or_nearby(&deferred_pickup_drop);
    }

    clear_deferred_pickup_drop();
}

static bool queue_deferred_pickup_drop(const object_type* src, int amount,
    int oil_to_drop, bool refill_oil_pool)
{
    if (!src || !src->k_idx || amount <= 0)
        return false;

    if (amount > src->number)
        amount = src->number;

    if (deferred_pickup_drop_pending)
    {
        log_warn("pickup replace: flushing unexpected pre-existing deferred "
            "drop before queueing another");
        flush_deferred_pickup_drop();
    }

    object_wipe(&deferred_pickup_drop);
    object_copy(&deferred_pickup_drop, src);
    deferred_pickup_drop.number = amount;
    deferred_pickup_drop_oil = oil_to_drop;
    deferred_pickup_refill_oil_pool = refill_oil_pool;
    deferred_pickup_drop_pending = true;

    return true;
}

static bool queue_deferred_pickup_supply_drop(int supply_idx, int amount,
    bool refill_oil_pool)
{
    object_type* supply_obj = supplies_entry_at(supply_idx);
    object_type deferred;
    char o_name[80];
    int oil_to_drop = 0;

    if (!supply_obj || !supply_obj->k_idx || amount <= 0)
        return false;

    if (amount > supply_obj->number)
        amount = supply_obj->number;

    if (player_oil_container_object(supply_obj))
    {
        if (!player_prepare_oil_container_drop(supply_obj, amount,
                &oil_to_drop, NULL))
            return false;
    }

    object_wipe(&deferred);
    object_copy(&deferred, supply_obj);
    deferred.number = amount;

    object_desc(o_name, sizeof(o_name), &deferred, true, 3);

    if (player_light_destroyed_on_drop(&deferred))
    {
        msg_format("You discard %s; %s too spent to keep.",
            o_name, (deferred.number > 1) ? "they are" : "it is");
        (void)supplies_consume_quantity(supply_idx, amount);
        p_ptr->redraw |= (PR_MAP | PR_LIGHT);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        return true;
    }

    if (!queue_deferred_pickup_drop(supply_obj, amount, oil_to_drop,
            refill_oil_pool))
        return false;

    (void)supplies_consume_quantity(supply_idx, amount);

    log_debug("pickup replace: queued deferred supply drop supply_idx=%d "
        "amount=%d tval=%d sval=%d oil=%d",
        supply_idx, amount, deferred_pickup_drop.tval,
        deferred_pickup_drop.sval, deferred_pickup_drop_oil);

    return true;
}

static bool queue_deferred_pickup_pack_drop(int item, int amount,
    bool refill_oil_pool)
{
    object_type* drop_ptr;
    object_type deferred;
    char o_name[80];
    int oil_to_drop = 0;

    if ((item < 0) || (item >= INVEN_PACK) || amount <= 0)
        return false;

    drop_ptr = &inventory[item];
    if (!drop_ptr->k_idx)
        return false;

    if (amount > drop_ptr->number)
        amount = drop_ptr->number;

    object_wipe(&deferred);
    object_copy(&deferred, drop_ptr);
    deferred.number = amount;

    if (player_oil_container_object(&deferred))
    {
        if (!player_prepare_oil_container_drop(&deferred, amount,
                &oil_to_drop, NULL))
            return false;
    }

    object_desc(o_name, sizeof(o_name), &deferred, true, 3);

    if (player_light_destroyed_on_drop(&deferred))
    {
        msg_format("You discard %s; %s too spent to keep.",
            o_name, (deferred.number > 1) ? "they are" : "it is");

        inven_item_increase(item, -amount);
        inven_item_describe(item);
        inven_item_optimize(item);
        p_ptr->redraw |= (PR_MAP | PR_LIGHT);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        return true;
    }

    if (!queue_deferred_pickup_drop(drop_ptr, amount, oil_to_drop,
            refill_oil_pool))
        return false;

    inven_item_increase(item, -amount);
    inven_item_describe(item);
    inven_item_optimize(item);

    return true;
}

static bool confirm_oil_pickup_overflow_with_bonus(const object_type* o_ptr,
    int oil_amount, int lantern_bonus)
{
    char o_name[80];
    char prompt[160];

    if (!o_ptr || oil_amount <= 0
        || !player_lamp_oil_would_overflow_with_bonus(oil_amount,
            lantern_bonus))
        return true;

    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
    strnfmt(prompt, sizeof(prompt),
        "Adding the oil from %s will waste some oil. Proceed? ", o_name);
    return get_check(prompt);
}

static bool confirm_oil_pickup_overflow(const object_type* o_ptr, int oil_amount)
{
    return confirm_oil_pickup_overflow_with_bonus(o_ptr, oil_amount, 0);
}

static bool pack_has_two_slot_throwable(void)
{
    for (int item = 0; item <= INVEN_PACK; item++)
    {
        const object_type* o_ptr = &inventory[item];

        if (!o_ptr->k_idx)
            continue;

        if (inventory_limit_group_for_object(o_ptr) == INV_LIMIT_THROWABLE
            && inventory_limit_space_for_object(o_ptr) == 2)
        {
            return true;
        }
    }

    return false;
}

static void format_inventory_limit_reason(char* buf, size_t max,
    const object_type* incoming, cptr label, int limit)
{
    enum inventory_limit_group group = inven_carry_limit_group();

    if (!buf || max == 0)
        return;

    if (group == INV_LIMIT_THROWABLE)
    {
        int used = inventory_limit_usage_for_group(group);
        int needed = inventory_limit_space_for_object(incoming);

        if (pack_has_two_slot_throwable())
        {
            strnfmt(buf, max,
                "No room: %d/%d throwable slots used. Your spear stack uses "
                "2 slots; this item needs %d slot%s.",
                used, limit, needed, (needed == 1) ? "" : "s");
        }
        else
        {
            strnfmt(buf, max,
                "No room: %d/%d throwable slots used; this item needs %d "
                "slot%s.",
                used, limit, needed, (needed == 1) ? "" : "s");
        }
    }
    else if (label)
    {
        strnfmt(buf, max,
            "No room: you already carry the most %s you can (limit %d).",
            label, limit);
    }
    else
    {
        SDL_strlcpy(buf, "No room: drop something to make space.", max);
    }
}

static pickup_failure_result prompt_replace_light_limit_item(
    object_type* incoming, int floor_o_idx, const char* incoming_name)
{
    char prompt[160];
    cptr label = inven_carry_limit_label();
    int limit = inven_carry_limit_value();
    bool replaced = false;
    bool old_item_tester_full = item_tester_full;
    byte old_item_tester_tval = item_tester_tval;
    bool (*old_item_tester_hook)(const object_type*) = item_tester_hook;
    const object_type* old_filter = replacement_filter_incoming;
    bool old_expand_supplies = inventory_menu_set_expand_supplies(true);
    int menu_item = -1;
    bool have_menu_item = false;

    extern bool sdl_is_story_font_enabled(void);
    extern void sdl_story_font_disable(void);
    if (sdl_is_story_font_enabled())
        sdl_story_font_disable();

    if (label)
        msg_format("You already carry %s (limit %d).", label, limit);
    else
        msg_print("You cannot carry any more of those.");

    {
        inventory_menu_group menu_group =
            inventory_menu_group_for_limit_group(inven_carry_limit_group());

        if (menu_group != INVENTORY_MENU_GROUP_ALL)
        {
            bool chose_replacement;
            char reason[160];

            format_inventory_limit_reason(reason, sizeof(reason), incoming,
                label, limit);

            msg_print("What to replace?");
            chose_replacement = open_inventory_replacement_menu(menu_group,
                incoming, true, true, reason, &menu_item);

            if (!chose_replacement)
            {
                inventory_menu_set_expand_supplies(old_expand_supplies);
                replacement_filter_incoming = old_filter;
                item_tester_hook = old_item_tester_hook;
                item_tester_tval = old_item_tester_tval;
                item_tester_full = old_item_tester_full;

                return PICKUP_FAILURE_ABORT;
            }

            have_menu_item = true;
        }
    }

    if (!have_menu_item)
        msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt),
            "Replace which item to pick up %s? ", incoming_name);

    replacement_filter_incoming = incoming;
    item_tester_tval = 0;
    item_tester_hook = item_tester_limit_group;
    item_tester_full = false;

    while (true)
    {
        int item;
        object_type* drop_ptr = NULL;
        int remove_amt = 1;

        if (have_menu_item)
        {
            item = menu_item;
            have_menu_item = false;
        }
        else if (!open_inventory_item_select_menu(USE_INVEN | USE_EQUIP,
            prompt, "You have nothing to replace.", &item))
        {
            break;
        }

        if (item >= SUPPLIES_INDEX)
        {
            int supply_idx = item - SUPPLIES_INDEX;
            drop_ptr = supplies_entry_at(supply_idx);

            if (!drop_ptr || !drop_ptr->k_idx)
            {
                bell("That supply entry is empty.");
                continue;
            }
        }
        else
        {
            if ((item < 0) || (item >= INVEN_TOTAL))
            {
                bell("Illegal object choice!");
                continue;
            }

            drop_ptr = &inventory[item];
            if (!drop_ptr->k_idx)
            {
                bell("That slot is empty.");
                continue;
            }

            if ((item >= INVEN_WIELD) && cursed_p(drop_ptr))
            {
                char equipped_name[80];
                object_desc(equipped_name, sizeof(equipped_name), drop_ptr, true,
                    3);
                msg_format("You cannot remove %s.", equipped_name);
                continue;
            }
        }

        if (!inven_carry_limit_can_replace(drop_ptr)
            || !pack_item_matches_replacement_type(incoming, drop_ptr))
        {
            msg_print("That will not make enough room.");
            continue;
        }

        if ((item >= INVEN_WIELD) && (item == wield_slot(incoming)))
        {
            log_debug("pickup light replace: equipping floor item %d directly "
                "into slot %d instead of dropping first", floor_o_idx, item);
            inventory_menu_set_expand_supplies(old_expand_supplies);
            replacement_filter_incoming = old_filter;
            item_tester_hook = old_item_tester_hook;
            item_tester_tval = old_item_tester_tval;
            item_tester_full = old_item_tester_full;
            do_cmd_wield(incoming, 0 - floor_o_idx);
            return PICKUP_FAILURE_EQUIPPED;
        }

        if (player_oil_container_object(incoming)
            && player_oil_container_object(drop_ptr))
        {
            int incoming_cost = player_oil_container_slot_cost(incoming);
            int drop_cost = player_oil_container_slot_cost(drop_ptr);
            int free_slots = player_oil_container_slot_capacity()
                - player_oil_container_slots_used();
            int needed_slots = incoming_cost * MAX(incoming->number, 1)
                - MAX(free_slots, 0);

            remove_amt = MAX(1, (needed_slots + drop_cost - 1) / drop_cost);
        }
        else
        {
            remove_amt = MAX(1,
                incoming->number - player_light_available_capacity(incoming));
        }
        remove_amt = MIN(remove_amt, MAX(drop_ptr->number, 1));

        if (item >= SUPPLIES_INDEX)
        {
            int supply_idx = item - SUPPLIES_INDEX;
            bool refill_oil_pool = player_oil_container_object(incoming)
                && player_oil_container_object(drop_ptr);

            if (floor_o_idx > 0)
            {
                if (!queue_deferred_pickup_supply_drop(supply_idx, remove_amt,
                        refill_oil_pool))
                    continue;
            }
            else
            {
                if (!supplies_drop_amount(supply_idx, remove_amt))
                    continue;
            }
        }
        else
        {
            if ((item < INVEN_WIELD)
                && !queue_deferred_pickup_pack_drop(item, remove_amt,
                    player_oil_container_object(incoming)
                        && player_oil_container_object(drop_ptr)))
            {
                continue;
            }

            if (item >= INVEN_WIELD)
                inven_drop(item, remove_amt);
        }

        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        notice_stuff();
        replaced = true;
        break;
    }

    inventory_menu_set_expand_supplies(old_expand_supplies);
    replacement_filter_incoming = old_filter;
    item_tester_hook = old_item_tester_hook;
    item_tester_tval = old_item_tester_tval;
    item_tester_full = old_item_tester_full;

    return replaced ? PICKUP_FAILURE_RETRY : PICKUP_FAILURE_ABORT;
}

static bool pickup_brass_lamp(int o_idx, object_type* o_ptr)
{
    int oil_amount;
    int pickup_y;
    int pickup_x;

    if (!object_is_brass_lamp(o_ptr))
        return false;

    if (o_ptr->number != 1)
        return false;

    pickup_y = o_ptr->iy;
    pickup_x = o_ptr->ix;
    oil_amount = MIN(o_ptr->timeout, FUEL_LAMP);

    if (player_light_available_capacity(o_ptr) <= 0)
        return false;

    if (!brass_lamp_pickup_overflow_checked
        && !confirm_oil_pickup_overflow_with_bonus(o_ptr, oil_amount, 1))
    {
        msg_print("You leave it on the ground.");
        return true;
    }

    brass_lamp_pickup_overflow_checked = false;
    player_gain_lamp_oil_with_bonus(oil_amount, true, 1);
    o_ptr->timeout = 0;
    give_player_item(o_ptr);
    (void)player_lamp_oil();

    if (!o_ptr->k_idx || o_ptr->number <= 0)
    {
        if (!o_ptr->k_idx)
        {
            log_debug("pickup_brass_lamp: restoring wiped floor object %d to "
                "(%d,%d) before delete", o_idx, pickup_y, pickup_x);
            o_ptr->iy = pickup_y;
            o_ptr->ix = pickup_x;
        }
        delete_object_idx(o_idx);
    }

    flush_deferred_pickup_drop();

    return true;
}

static bool pickup_brass_lamp_oil_only(object_type* o_ptr)
{
    int oil_amount;

    if (!object_is_brass_lamp(o_ptr) || (o_ptr->number != 1))
        return false;

    oil_amount = MIN(o_ptr->timeout, FUEL_LAMP);
    if ((oil_amount <= 0) || !get_check("Take only the oil? "))
        return false;

    if (!confirm_oil_pickup_overflow(o_ptr, oil_amount))
    {
        msg_print("You leave it on the ground.");
        return true;
    }

    player_gain_lamp_oil(oil_amount, true);
    o_ptr->timeout = 0;
    msg_print("You siphon the oil and leave the lamp behind.");
    return true;
}

static int carried_oil_flask_count(void)
{
    int count = 0;

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        if (s_ptr && s_ptr->k_idx && s_ptr->tval == TV_FLASK)
            count += MAX(s_ptr->number, 1);
    }

    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (o_ptr->k_idx && o_ptr->tval == TV_FLASK)
            count += MAX(o_ptr->number, 1);
    }

    return count;
}

static int discard_oil_flasks_for_lamp(int needed_slots)
{
    int discarded = 0;

    while (needed_slots > 0)
    {
        bool removed = false;

        for (int i = 0; i < supplies_entry_count(); i++)
        {
            object_type* s_ptr = supplies_entry_at(i);
            if (!s_ptr || !s_ptr->k_idx || s_ptr->tval != TV_FLASK)
                continue;

            (void)supplies_consume_quantity(i, 1);
            discarded++;
            needed_slots--;
            removed = true;
            break;
        }

        if (removed)
            continue;

        for (int i = 0; i < INVEN_PACK; i++)
        {
            object_type* o_ptr = &inventory[i];
            if (!o_ptr->k_idx || o_ptr->tval != TV_FLASK)
                continue;

            inven_item_increase(i, -1);
            inven_item_optimize(i);
            discarded++;
            needed_slots--;
            removed = true;
            break;
        }

        if (!removed)
            break;
    }

    return discarded;
}

static bool carried_brass_lamps_fill_oil_storage(void)
{
    return player_carried_light_count_for_sval(SV_LIGHT_LANTERN)
        * PLAYER_BRASS_LAMP_SLOT_COST >= PLAYER_OIL_CONTAINER_SLOT_CAP;
}

static int oil_container_pickup_oil_amount(const object_type* o_ptr)
{
    int unit_oil;

    if (!o_ptr || !o_ptr->k_idx)
        return 0;

    if (object_is_brass_lamp(o_ptr))
        unit_oil = MIN(o_ptr->timeout, FUEL_LAMP);
    else if (object_is_oil_flask(o_ptr))
        unit_oil = MIN(o_ptr->pval, FUEL_FLASK);
    else
        return 0;

    if (unit_oil < 0)
        unit_oil = 0;

    return unit_oil * MAX(o_ptr->number, 1);
}

static int oil_flask_units_oil_amount(const object_type* o_ptr, int amount)
{
    int unit_oil;

    if (!object_is_oil_flask(o_ptr) || amount <= 0)
        return 0;

    unit_oil = MIN(o_ptr->pval, FUEL_FLASK);
    if (unit_oil < 0)
        unit_oil = 0;

    return unit_oil * MIN(amount, MAX(o_ptr->number, 1));
}

static bool pickup_oil_flask_oil_only(int o_idx, object_type* o_ptr)
{
    int oil_amount;

    if (!object_is_oil_flask(o_ptr))
        return false;

    if (!carried_brass_lamps_fill_oil_storage())
        return false;

    oil_amount = oil_container_pickup_oil_amount(o_ptr);
    if (oil_amount > 0 && !confirm_oil_pickup_overflow(o_ptr, oil_amount))
    {
        msg_print("You leave it on the ground.");
        return true;
    }

    if (oil_amount > 0)
    {
        player_gain_lamp_oil(oil_amount, true);
        (void)player_lamp_oil();
        msg_format("You pour the oil into your lamp stores and discard the "
            "flask%s.", (MAX(o_ptr->number, 1) == 1) ? "" : "s");
    }
    else
    {
        msg_format("You discard the empty oil flask%s.",
            (MAX(o_ptr->number, 1) == 1) ? "" : "s");
    }

    delete_object_idx(o_idx);
    p_ptr->redraw |= (PR_MAP | PR_LIGHT);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    return true;
}

static int oil_flasks_replaced_for_lamp_oil_amount(int needed_slots)
{
    int oil_amount = 0;

    if (needed_slots <= 0)
        return 0;

    for (int i = 0; i < supplies_entry_count() && needed_slots > 0; i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        int amount;

        if (!object_is_oil_flask(s_ptr))
            continue;

        amount = MIN(needed_slots, MAX(s_ptr->number, 1));
        oil_amount += oil_flask_units_oil_amount(s_ptr, amount);
        needed_slots -= amount;
    }

    for (int i = 0; i < INVEN_PACK && needed_slots > 0; i++)
    {
        object_type* o_ptr = &inventory[i];
        int amount;

        if (!object_is_oil_flask(o_ptr))
            continue;

        amount = MIN(needed_slots, MAX(o_ptr->number, 1));
        oil_amount += oil_flask_units_oil_amount(o_ptr, amount);
        needed_slots -= amount;
    }

    return oil_amount;
}

static bool brass_lamp_pickup_oil_would_overflow_after_discard(
    const object_type* incoming, int discarded_flasks, int discarded_flask_oil)
{
    int oil_amount;
    int resulting_slots;
    int resulting_capacity;
    int current_oil;

    if (!object_is_brass_lamp(incoming))
        return false;

    oil_amount = oil_container_pickup_oil_amount(incoming);
    oil_amount += discarded_flask_oil;
    if (oil_amount <= 0)
        return false;

    resulting_slots = player_oil_container_slots_used()
        - discarded_flasks * PLAYER_OIL_FLASK_SLOT_COST
        + player_oil_container_slot_cost(incoming) * MAX(incoming->number, 1);
    if (resulting_slots < 0)
        resulting_slots = 0;
    if (resulting_slots > PLAYER_OIL_CONTAINER_SLOT_CAP)
        resulting_slots = PLAYER_OIL_CONTAINER_SLOT_CAP;

    resulting_capacity = resulting_slots * FUEL_FLASK;
    current_oil = p_ptr ? p_ptr->lamp_oil : 0;
    if (current_oil < 0)
        current_oil = 0;

    return current_oil + oil_amount > resulting_capacity;
}

static bool auto_replace_flasks_for_brass_lamp(const object_type* incoming,
    bool* aborted)
{
    int incoming_slots;
    int needed_slots;
    int discarded_flask_oil;
    int discarded;

    if (aborted)
        *aborted = false;

    if (!object_is_brass_lamp(incoming))
        return false;

    incoming_slots = player_oil_container_slot_cost(incoming)
        * MAX(incoming->number, 1);
    needed_slots = player_oil_container_slots_used() + incoming_slots
        - player_oil_container_slot_capacity();

    if (needed_slots <= 0)
        return false;

    if (carried_oil_flask_count() < needed_slots)
        return false;

    discarded_flask_oil = oil_flasks_replaced_for_lamp_oil_amount(needed_slots);

    if (brass_lamp_pickup_oil_would_overflow_after_discard(incoming,
            needed_slots, discarded_flask_oil))
    {
        if (!confirm_oil_pickup_overflow(incoming,
                oil_container_pickup_oil_amount(incoming)
                    + discarded_flask_oil))
        {
            if (aborted)
                *aborted = true;
            msg_print("You leave it on the ground.");
            return false;
        }

        brass_lamp_pickup_overflow_checked = true;
    }

    discarded = discard_oil_flasks_for_lamp(needed_slots);
    if (discarded > 0)
    {
        brass_lamp_pickup_overflow_checked = true;
        if (discarded_flask_oil > 0)
            player_gain_lamp_oil(discarded_flask_oil, true);
        msg_format("Your lamp replaces %d oil flask%s, keeping the oil in "
            "your lamp stores.",
            discarded, (discarded == 1) ? "" : "s");
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    }

    return discarded > 0;
}

/*
 * Helper routine for py_pickup() and py_pickup_floor().
 *
 * Add the given dungeon object to the character's inventory.
 *
 * Delete the object afterwards.
 */
static bool prepare_floor_object_for_pickup(int o_idx, object_type* o_ptr);
static void py_pickup_aux_internal(int o_idx, bool allow_channel);

void py_pickup_aux(int o_idx)
{
    py_pickup_aux_internal(o_idx, true);
}

static void py_pickup_aux_internal(int o_idx, bool allow_channel)
{
    object_type* o_ptr;
    char o_name[120];
    
    o_ptr = &o_list[o_idx];

    if (object_is_searched_skeleton(o_ptr))
        return;

    if (allow_channel && player_channel_floor_staff(o_ptr, o_idx))
        return;

    // Remember the floor position even if give_player_item wipes the object
    int pickup_y = o_ptr->iy;
    int pickup_x = o_ptr->ix;

    /*hack - don't pickup &nothings*/
    if (o_ptr->k_idx)
    {
        if (!prepare_floor_object_for_pickup(o_idx, o_ptr))
        {
            flush_deferred_pickup_drop();
            return;
        }

        if (pickup_brass_lamp(o_idx, o_ptr))
            return;

        /* Check for Oath of the Smith violation */
        if (smith_oath_forbids_object(o_ptr))
        {
            if (!smith_oath_confirm_break())
            {
                flush_deferred_pickup_drop();
                return;
            }
        }

        /* Check for supply items with partial pickup option */
        if (supplies_is_supply_object(o_ptr) && o_ptr->number > 1)
        {
            int max_qty = supplies_max_absorbable_quantity(o_ptr);
            
            /* If we can't absorb all of it but can absorb some, offer partial pickup */
            if (max_qty > 0 && max_qty < o_ptr->number)
            {
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                
                char prompt[160];
                strnfmt(prompt, sizeof(prompt), 
                        "Your supply cache can only hold %d of %d. Pick up how many? (0-%d): ",
                        max_qty, o_ptr->number, max_qty);
                
                int qty = get_quantity_touch_category_force_prompt(prompt,
                    max_qty, SDL_TOUCH_MENU_CATEGORY_SUPPLY);
                
                if (qty <= 0)
                {
                    msg_print("You leave it on the ground.");
                    flush_deferred_pickup_drop();
                    return;
                }
                
                /* Create a partial object to pick up */
                object_type partial;
                object_copy(&partial, o_ptr);
                partial.number = qty;
                
                give_player_item(&partial);
                
                /* Reduce the floor object */
                o_ptr->number -= qty;
                
                /* Break the truce if creatures see */
                break_truce(false);

                flush_deferred_pickup_drop();
                
                return;
            }
        }
        
        give_player_item(o_ptr);

        // Break the truce if creatures see
        break_truce(false);

        if (!o_ptr->k_idx || o_ptr->number <= 0)
        {
            if (!o_ptr->k_idx)
            {
                o_ptr->iy = pickup_y;
                o_ptr->ix = pickup_x;
            }
            delete_object_idx(o_idx);
        }

        flush_deferred_pickup_drop();

        return;
    }

    /* Delete the object */
    o_ptr->iy = pickup_y;
    o_ptr->ix = pickup_x;
    delete_object_idx(o_idx);
    flush_deferred_pickup_drop();
}

/*
 * Allow the player to sort through items in a pile and
 * pickup what they want.  This command does not use
 * any energy because it costs a player no extra energy
 * to walk into a grid and automatically pick up items
 */
void do_cmd_pickup_from_pile(void)
{
    bool picked_up_item = false;

    /*
     * Loop through and pick up objects until escape is hit or the backpack
     * can't hold anything else.
     */
    while (true)
    {
        int item;

        int floor_list[MAX_FLOOR_STACK];

        int floor_num;

        ui_question_option options[MAX_FLOOR_STACK];
        char names[MAX_FLOOR_STACK][80];

        /*start with everything updated*/
        handle_stuff();

        /* Scan for floor objects */
        floor_num = scan_floor(
            floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x01);

        /* No pile */
        if (floor_num < 1)
        {
            if (picked_up_item)
                msg_format("There are no more objects where you are standing.");
            else
                msg_format("There are no objects where you are standing.");
            break;
        }

        /* Offer the pile through the question overlay, next to the player */
        for (int i = 0; i < floor_num; i++)
        {
            object_type* o_ptr = &o_list[floor_list[i]];

            object_desc(names[i], sizeof(names[i]), o_ptr, true, 3);
            options[i].key = (i < 26) ? (char)('a' + i) : 0;
            options[i].label = names[i];
            options[i].attr = TERM_L_WHITE;
        }

        item = ui_question_ask("Pick up which object?", NULL, options,
            floor_num, p_ptr->py, p_ptr->px, 0);

        /*player chose escape*/
        if (item < 0)
            break;

        /* Pick up the object */
        py_pickup_aux(floor_list[item]);

        /*Mark that we picked something up*/
        picked_up_item = true;
    }

    /* Combine / Reorder the pack */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);

    /* Just be sure all inventory management is done. */
    notice_stuff();
}

static void report_pack_limit_failure(const char* o_name, bool still)
{
    if (inven_carry_limit_failed())
    {
        cptr label = inven_carry_limit_label();
        int limit = inven_carry_limit_value();

        if (inven_carry_limit_group() == INV_LIMIT_THROWABLE)
        {
            int used = inventory_limit_usage_for_group(INV_LIMIT_THROWABLE);

            if (pack_has_two_slot_throwable())
            {
                msg_format("Your pack's throwable slots are full (%d/%d); "
                           "your spear stack uses 2 slots.", used, limit);
            }
            else
            {
                msg_format("Your pack's throwable slots are full (%d/%d).",
                           used, limit);
            }
            return;
        }

        if (label)
        {
            /* Special message for supply weight limit */
            if (strcmp(label, "supply weight") == 0)
            {
                msg_format("Your supply cache cannot carry any more weight (limit %d lbs).",
                           limit);
                return;
            }

            if (still)
                msg_format("Your pack still cannot hold more %s (limit %d).", label,
                           limit);
            else
                msg_format("Your pack cannot hold more %s (limit %d).", label, limit);
            return;
        }
    }

    if (still)
        msg_format("You still have no room for %s.", o_name);
    else
        msg_format("You have no room for %s.", o_name);
}

static bool item_tester_limit_group(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (inven_carry_limit_is_supply_weight())
        return inven_carry_limit_can_replace(o_ptr);

    if (replacement_filter_incoming
        && !pack_item_matches_replacement_type(replacement_filter_incoming, o_ptr))
        return false;

    return inven_carry_limit_can_replace(o_ptr);
}

static int supply_weight_replacement_amount(const object_type* incoming,
                                            const object_type* candidate)
{
    int incoming_weight;
    int over_limit;
    int amount;

    if (!incoming || !candidate || !candidate->k_idx)
        return 0;

    if (!supplies_weight_counts_to_limit(incoming)
        || !supplies_weight_counts_to_limit(candidate))
    {
        return 0;
    }

    if (incoming->weight <= 0 || candidate->weight <= 0)
        return 0;

    incoming_weight = incoming->weight * MAX(incoming->number, 1);
    over_limit = supplies_limit_weight() + incoming_weight
        - supplies_current_weight_cap();

    if (over_limit <= 0)
        return 1;

    amount = (over_limit + candidate->weight - 1) / candidate->weight;
    return MIN(amount, MAX(candidate->number, 1));
}

static bool prompt_replace_pack_item_limit(const object_type* incoming,
                                           const char* incoming_name)
{
    char prompt[160];
    cptr label = inven_carry_limit_label();
    int limit = inven_carry_limit_value();
    bool replaced = false;
    bool supply_weight_limit = inven_carry_limit_is_supply_weight();

    bool old_item_tester_full = item_tester_full;
    byte old_item_tester_tval = item_tester_tval;
    bool (*old_item_tester_hook)(const object_type*) = item_tester_hook;
    const object_type* old_filter = replacement_filter_incoming;
    bool old_expand_supplies =
        inventory_menu_set_expand_supplies(supply_weight_limit);
    int menu_item = -1;
    bool have_menu_item = false;

    /* Ensure story font is disabled before showing messages */
    extern bool sdl_is_story_font_enabled(void);
    extern void sdl_story_font_disable(void);
    if (sdl_is_story_font_enabled())
        sdl_story_font_disable();

    if (inven_carry_limit_group() == INV_LIMIT_THROWABLE)
    {
        char reason[160];

        format_inventory_limit_reason(reason, sizeof(reason), incoming, label,
            limit);
        msg_print(reason);
    }
    else if (label)
        msg_format("You already carry %s (limit %d).", label, limit);
    else
        msg_print("You cannot carry any more of those.");

    if (!supply_weight_limit)
    {
        inventory_menu_group menu_group =
            inventory_menu_group_for_limit_group(inven_carry_limit_group());

        if (menu_group != INVENTORY_MENU_GROUP_ALL)
        {
            bool chose_replacement;
            char reason[160];

            format_inventory_limit_reason(reason, sizeof(reason), incoming,
                label, limit);

            msg_print("What to replace?");
            chose_replacement = open_inventory_replacement_menu(menu_group,
                incoming, false, false, reason, &menu_item);

            if (!chose_replacement)
            {
                inventory_menu_set_expand_supplies(old_expand_supplies);
                replacement_filter_incoming = old_filter;
                item_tester_hook = old_item_tester_hook;
                item_tester_tval = old_item_tester_tval;
                item_tester_full = old_item_tester_full;

                return false;
            }

            have_menu_item = true;
        }
    }

    if (!have_menu_item)
        msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt),
            "Replace which item to pick up %s? ", incoming_name);

    replacement_filter_incoming = incoming;
    item_tester_tval = 0;
    item_tester_hook = item_tester_limit_group;
    item_tester_full = false;

    while (true)
    {
        int item;
        object_type* drop_ptr = NULL;
        int remove_amt = 0;

        if (have_menu_item)
        {
            item = menu_item;
            have_menu_item = false;
        }
        else if (!open_inventory_item_select_menu(USE_INVEN, prompt,
            "You have nothing to replace.", &item))
            break;

        if (item >= SUPPLIES_INDEX)
        {
            int supply_idx = item - SUPPLIES_INDEX;
            drop_ptr = supplies_entry_at(supply_idx);

            if (!drop_ptr || !drop_ptr->k_idx)
            {
                bell("That supply entry is empty.");
                continue;
            }
        }
        else if ((item < 0) || (item >= INVEN_PACK))
        {
            bell("Illegal object choice!");
            continue;
        }
        else
        {
            drop_ptr = &inventory[item];

            if (!drop_ptr->k_idx)
            {
                bell("That slot is empty.");
                continue;
            }
        }

        if (!inven_carry_limit_can_replace(drop_ptr))
        {
            msg_print("That will not make enough room.");
            continue;
        }

        if (supply_weight_limit)
        {
            if (item < SUPPLIES_INDEX)
            {
                msg_print("That will not make enough room.");
                continue;
            }

            remove_amt = supply_weight_replacement_amount(incoming, drop_ptr);
            if (remove_amt <= 0)
            {
                msg_print("That will not make enough room.");
                continue;
            }

            if (!queue_deferred_pickup_supply_drop(item - SUPPLIES_INDEX,
                    remove_amt, false))
            {
                continue;
            }
        }
        else
        {
            if (item >= SUPPLIES_INDEX)
            {
                msg_print("That will not make enough room.");
                continue;
            }

            if (!queue_deferred_pickup_pack_drop(item, drop_ptr->number, false))
                continue;
        }

        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        notice_stuff();

        replaced = true;
        break;
    }

    replacement_filter_incoming = old_filter;
    item_tester_hook = old_item_tester_hook;
    item_tester_tval = old_item_tester_tval;
    item_tester_full = old_item_tester_full;
    inventory_menu_set_expand_supplies(old_expand_supplies);

    return replaced;
}

static pickup_failure_result handle_zero_limit_pickup(object_type* incoming,
                                                      int floor_o_idx,
                                                      const char* incoming_name)
{
    int slot = wield_slot(incoming);

    msg_format("You cannot carry %s in your pack.", incoming_name);

    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
    {
        msg_print("It does not fit anywhere on your body.");
        return PICKUP_FAILURE_ABORT;
    }

    object_type* equip_ptr = &inventory[slot];

    if (!equip_ptr->k_idx)
    {
        if (get_check("Wear it now? "))
        {
            do_cmd_wield(incoming, 0 - floor_o_idx);
            return PICKUP_FAILURE_EQUIPPED;
        }

        msg_print("You leave it on the ground.");
        return PICKUP_FAILURE_ABORT;
    }

    if (cursed_p(equip_ptr))
    {
        char equipped_name[80];
        object_desc(equipped_name, sizeof(equipped_name), equip_ptr, true, 3);
        msg_format("You cannot remove %s.", equipped_name);
        return PICKUP_FAILURE_ABORT;
    }

    screen_save();
    show_equip();
    msg_print(NULL);
    screen_load();

    char equipped_name[80];
    object_desc(equipped_name, sizeof(equipped_name), equip_ptr, true, 3);

    char prompt[160];
    strnfmt(prompt, sizeof(prompt), "Replace %s with %s? ", equipped_name,
            incoming_name);

    if (get_check(prompt))
    {
        do_cmd_wield(incoming, 0 - floor_o_idx);
        return PICKUP_FAILURE_EQUIPPED;
    }

    msg_print("You decide to keep your current equipment.");
    return PICKUP_FAILURE_ABORT;
}

static pickup_failure_result handle_group_limit_pickup(object_type* incoming,
                                                       int floor_o_idx,
                                                       const char* incoming_name)
{
    if (object_uses_light_pickup_limit(incoming))
        return prompt_replace_light_limit_item(incoming, floor_o_idx,
            incoming_name);

    if (!prompt_replace_pack_item_limit(incoming, incoming_name))
        return PICKUP_FAILURE_ABORT;

    return PICKUP_FAILURE_RETRY;
}

static pickup_failure_result resolve_pickup_failure(object_type* incoming,
                                                    int floor_o_idx,
                                                    const char* incoming_name,
                                                    bool attempted_replacement)
{
    bool has_lamp_oil_fallback = object_is_brass_lamp(incoming)
        && (incoming->number == 1) && (incoming->timeout > 0);

    if (inven_carry_limit_failed())
    {
        if (inven_carry_limit_value() <= 0)
            return handle_zero_limit_pickup(incoming, floor_o_idx,
                                            incoming_name);

        pickup_failure_result limit_result =
            handle_group_limit_pickup(incoming, floor_o_idx, incoming_name);

        if ((limit_result == PICKUP_FAILURE_ABORT) && !has_lamp_oil_fallback)
            report_pack_limit_failure(incoming_name, attempted_replacement);

        return limit_result;
    }

    if (prompt_replace_pack_item(incoming))
        return PICKUP_FAILURE_RETRY;

    if (!has_lamp_oil_fallback)
        report_pack_limit_failure(incoming_name, attempted_replacement);

    return PICKUP_FAILURE_ABORT;
}

static bool prepare_floor_object_for_pickup(int o_idx, object_type* o_ptr)
{
    char o_name[120];
    bool attempted_replacement = false;
    bool pickup_aborted = false;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    brass_lamp_pickup_overflow_checked = false;
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    if (pickup_oil_flask_oil_only(o_idx, o_ptr))
        return false;

    auto_replace_flasks_for_brass_lamp(o_ptr, &pickup_aborted);
    if (pickup_aborted)
        return false;

    while (!inven_carry_okay(o_ptr))
    {
        pickup_failure_result failure = resolve_pickup_failure(
            o_ptr, o_idx, o_name, attempted_replacement);

        if (failure == PICKUP_FAILURE_RETRY)
        {
            attempted_replacement = true;
            continue;
        }

        if (failure == PICKUP_FAILURE_EQUIPPED)
            return false;

        if (pickup_brass_lamp_oil_only(o_ptr))
        {
            flush_deferred_pickup_drop();
            return false;
        }

        flush_deferred_pickup_drop();
        return false;
    }

    return true;
}

void py_pickup(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    bool done_pickup = false;

    s16b this_o_idx, next_o_idx = 0;

    object_type* o_ptr;

    char o_name[80];

    /* Scan the pile of objects */
    for (this_o_idx = cave_o_idx[py][px]; this_o_idx; this_o_idx = next_o_idx)
    {
        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        if (object_is_searched_skeleton(o_ptr))
            continue;

        /* Describe the object */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Hack -- disturb */
        disturb(0, 0);

        if (player_channel_floor_staff(o_ptr, this_o_idx))
        {
            done_pickup = true;
            continue;
        }

        // Check whether it would be too heavy
        if (p_ptr->total_weight + o_ptr->weight > weight_limit() * 3 / 2)
        {
            if (o_ptr->k_idx)
                msg_format("You cannot lift %s.", o_name);

            /* Check the next object */
            continue;
        }

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;

        /* Take a turn */
        p_ptr->energy_use = 100;

        /* Pick up the object */
        py_pickup_aux_internal(this_o_idx, false);

        done_pickup = true;
    }

    if (!done_pickup)
    {
        p_ptr->previous_action[0] = ACTION_NOTHING;
        p_ptr->energy_use = 0;
    }
}
