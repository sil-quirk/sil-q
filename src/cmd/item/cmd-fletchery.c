#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "ui/question.h"

static bool item_tester_hook_fletchery_source(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    if (o_ptr->tval == TV_ARROW)
    {
        if (o_ptr->name1 || o_ptr->att >= 3)
            return false;
        return true;
    }

    if (o_ptr->tval == TV_LIGHT
        && (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN))
    {
        if (o_ptr->name1 || object_has_ego(o_ptr))
            return false;
        return true;
    }

    if (o_ptr->tval == TV_STAFF)
    {
        if (o_ptr->name1 || object_has_ego(o_ptr))
            return false;
        return true;
    }

    return false;
}

static object_type fletchery_source_snapshot;
static bool fletchery_source_snapshot_valid = false;
static bool fletchery_source_in_pack = false;

static void log_fletchery_object_state(
    const char* tag, const object_type* o_ptr, int slot)
{
    char o_name[160];

    if (!tag)
        tag = "state";

    if (!o_ptr || !o_ptr->k_idx)
    {
        log_debug("fletchery:%s slot=%d empty", tag, slot);
        return;
    }

    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
    log_debug(
        "fletchery:%s slot=%d name='%s' k_idx=%d tval=%d sval=%d num=%d att=%d "
        "evn=%d dd=%d ds=%d pval=%d prefix=%d suffix=%d ident=0x%08x note=%u "
        "discount=%d pickup=%d pickup_slot=%d runtime=%ld",
        tag, slot, o_name, o_ptr->k_idx, o_ptr->tval, o_ptr->sval,
        o_ptr->number, o_ptr->att, o_ptr->evn, o_ptr->dd, o_ptr->ds,
        o_ptr->pval, (int)object_ego_prefix(o_ptr), (int)object_ego_suffix(o_ptr),
        (unsigned int)o_ptr->ident, (unsigned int)o_ptr->obj_note, o_ptr->discount,
        o_ptr->pickup ? 1 : 0, o_ptr->pickup_slot,
        (long)object_runtime_state(o_ptr));
}

static void clear_fletchery_source_snapshot(void)
{
    if (fletchery_source_snapshot_valid)
        log_fletchery_object_state("clear_snapshot", &fletchery_source_snapshot,
            p_ptr->fletch_item);

    object_wipe(&fletchery_source_snapshot);
    fletchery_source_snapshot_valid = false;
    fletchery_source_in_pack = false;
    p_ptr->fletch_item = -1;
}

static void remember_fletchery_source_snapshot(const object_type* o_ptr)
{
    if (!o_ptr)
    {
        clear_fletchery_source_snapshot();
        return;
    }

    object_copy(&fletchery_source_snapshot, o_ptr);
    fletchery_source_snapshot_valid = true;
    log_fletchery_object_state("remember_snapshot", o_ptr, p_ptr->fletch_item);
}

static bool fletchery_source_matches(const object_type* o_ptr)
{
    const object_type* source = &fletchery_source_snapshot;

    if (!fletchery_source_snapshot_valid || !o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->k_idx != source->k_idx)
        return false;
    if (o_ptr->tval != source->tval || o_ptr->sval != source->sval)
        return false;

    if (o_ptr->att != source->att || o_ptr->evn != source->evn)
        return false;
    if (o_ptr->dd != source->dd || o_ptr->ds != source->ds)
        return false;
    if (o_ptr->pd != source->pd || o_ptr->ps != source->ps)
        return false;
    if (o_ptr->pval != source->pval)
        return false;
    if (o_ptr->name1 != source->name1)
        return false;

    if (object_ego_prefix(o_ptr) != object_ego_prefix(source)
        || object_ego_suffix(o_ptr) != object_ego_suffix(source))
    {
        return false;
    }

    if (o_ptr->timeout != source->timeout)
        return false;
    if (o_ptr->obj_note != source->obj_note)
        return false;
    if (o_ptr->discount != source->discount)
        return false;

    if (((o_ptr->ident ^ source->ident) & (IDENT_CURSED | IDENT_BROKEN)) != 0)
    {
        return false;
    }

    if (memcmp(o_ptr->stat_bonus, source->stat_bonus,
            sizeof(o_ptr->stat_bonus))
        != 0)
    {
        return false;
    }

    if (memcmp(o_ptr->skill_bonus, source->skill_bonus,
            sizeof(o_ptr->skill_bonus))
        != 0)
    {
        return false;
    }

    if (object_runtime_state(o_ptr) != object_runtime_state(source))
        return false;

    return true;
}

static int collect_fletchery_source_slots(int slots[INVEN_TOTAL])
{
    int count = 0;
    int preferred_slot = p_ptr->fletch_item;

    if ((preferred_slot >= 0) && (preferred_slot < INVEN_TOTAL)
        && fletchery_source_matches(&inventory[preferred_slot]))
    {
        slots[count++] = preferred_slot;
    }

    if (!fletchery_source_in_pack)
        return count;

    for (int slot = 0; slot < INVEN_PACK; slot++)
    {
        if (slot == preferred_slot)
            continue;
        if (!fletchery_source_matches(&inventory[slot]))
            continue;

        slots[count++] = slot;
    }

    log_debug("fletchery:collect_slots preferred=%d count=%d in_pack=%d",
        preferred_slot, count, fletchery_source_in_pack ? 1 : 0);
    for (int i = 0; i < count; i++)
        log_fletchery_object_state("collect_slot", &inventory[slots[i]], slots[i]);

    return count;
}

enum fletch_source_type
{
    FLETCH_SOURCE_INVEN = 0,
    FLETCH_SOURCE_EQUIP = 1,
    FLETCH_SOURCE_FLOOR = 2,
    FLETCH_SOURCE_SUPPLY = 3
};

typedef struct fletch_choice_s
{
    enum fletch_source_type type;
    int index;
} fletch_choice_t;

static bool supply_object_is_fletchery_torch(
    const object_type* o_ptr, int sval)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_LIGHT
        && o_ptr->sval == sval && item_tester_hook_fletchery_source(o_ptr);
}

static int count_fletchery_supply_torches(int sval)
{
    int total = 0;

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);
        if (!supply_object_is_fletchery_torch(o_ptr, sval))
            continue;

        total += o_ptr->number;
    }

    return total;
}

static int find_fletchery_supply_torch(int sval)
{
    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);
        if (supply_object_is_fletchery_torch(o_ptr, sval))
            return i;
    }

    return -1;
}

static bool choose_fletchery_supply_torch(int* out_sval)
{
    int wooden = count_fletchery_supply_torches(SV_LIGHT_TORCH);
    int mallorn = count_fletchery_supply_torches(SV_LIGHT_MALLORN);
    char wooden_label[40];
    char mallorn_label[40];
    ui_question_option options[2];
    int choice;

    if (!out_sval)
        return false;

    if (wooden <= 0 && mallorn <= 0)
    {
        msg_print("You have no torches in your supplies.");
        return false;
    }

    if (wooden > 0 && mallorn <= 0)
    {
        *out_sval = SV_LIGHT_TORCH;
        return true;
    }

    if (mallorn > 0 && wooden <= 0)
    {
        *out_sval = SV_LIGHT_MALLORN;
        return true;
    }

    strnfmt(wooden_label, sizeof(wooden_label), "Wooden torches (%d)", wooden);
    strnfmt(mallorn_label, sizeof(mallorn_label), "Mallorn torches (%d)",
        mallorn);
    options[0]
        = (ui_question_option){ 'w', wooden_label, TERM_L_WHITE, false };
    options[1]
        = (ui_question_option){ 'm', mallorn_label, TERM_L_WHITE, false };

    choice = ui_question_ask("Use which torches from supplies?", NULL,
        options, 2, UI_QUESTION_GLOBAL, UI_QUESTION_GLOBAL, 0);
    if (choice < 0)
        return false;

    *out_sval = (choice == 1) ? SV_LIGHT_MALLORN : SV_LIGHT_TORCH;
    return true;
}

static bool build_fletchery_supply_source(
    int sval, object_type* out_obj, int* out_total)
{
    int idx;
    int total;
    object_type* o_ptr;

    if (!out_obj || !out_total)
        return false;

    total = count_fletchery_supply_torches(sval);
    idx = find_fletchery_supply_torch(sval);
    if (idx < 0 || total <= 0)
        return false;

    o_ptr = supplies_entry_at(idx);
    if (!o_ptr)
        return false;

    object_copy(out_obj, o_ptr);
    *out_total = total;
    return true;
}

static bool consume_fletchery_supply_torches(int sval, int amount)
{
    for (int i = 0; amount > 0 && i < supplies_entry_count();)
    {
        object_type* o_ptr = supplies_entry_at(i);
        int take;

        if (!supply_object_is_fletchery_torch(o_ptr, sval))
        {
            i++;
            continue;
        }

        take = MIN(amount, o_ptr->number);
        if (supplies_consume_quantity(i, take))
        {
            amount -= take;
            continue;
        }

        amount -= take;
        i++;
    }

    return amount == 0;
}

static bool drop_fletchered_arrows_near(object_type* arrows)
{
    object_type drop_obj;
    s16b o_idx;

    if (!arrows || arrows->number <= 0 || arrows->k_idx == 0)
        return false;

    log_fletchery_object_state("drop_attempt", arrows, -1);

    if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_FLOOR)
        || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT))
    {
        object_copy(&drop_obj, arrows);
        o_idx = floor_carry(p_ptr->py, p_ptr->px, &drop_obj);
        if (o_idx > 0)
        {
            log_debug("fletchery:drop_here_success o_idx=%d y=%d x=%d",
                o_idx, p_ptr->py, p_ptr->px);
            log_fletchery_object_state("drop_here_floor", &o_list[o_idx], -1);
            return true;
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
        {
            continue;
        }

        object_copy(&drop_obj, arrows);
        o_idx = floor_carry(yy, xx, &drop_obj);
        if (o_idx > 0)
        {
            log_debug("fletchery:drop_adjacent_success o_idx=%d y=%d x=%d",
                o_idx, yy, xx);
            log_fletchery_object_state("drop_adjacent_floor", &o_list[o_idx], -1);
            return true;
        }
    }

    /*
     * Force placement for crafted-arrow overflow so partial fletchery results
     * do not vanish when the pack is full and nearby floor grids are crowded.
     */
    object_copy(&drop_obj, arrows);
    drop_obj.pickup = true;
    drop_obj.pickup_slot = -1;

    o_idx = drop_near(&drop_obj, 0, p_ptr->py, p_ptr->px);

    if (!o_idx)
    {
        log_warn("drop_fletchered_arrows_near: failed to place %d crafted arrows",
            arrows->number);
        return false;
    }

    /*
     * These are crafted arrows, not auto-recovering fired ammo. Clear the
     * temporary force-place flag after the floor object is created/updated.
     */
    if (o_idx > 0)
    {
        log_debug("fletchery:drop_near_success o_idx=%d", o_idx);
        log_fletchery_object_state("drop_near_floor_before_clear", &o_list[o_idx], -1);
        o_list[o_idx].pickup = false;
        o_list[o_idx].pickup_slot = -1;
        log_fletchery_object_state("drop_near_floor_after_clear", &o_list[o_idx], -1);
    }

    return true;
}

static void distribute_fletchered_arrows(const object_type* arrows)
{
    if (!arrows || arrows->number <= 0 || arrows->k_idx == 0)
        return;

    log_fletchery_object_state("distribute_input", arrows, -1);

    object_type leftover = *arrows;
    bool combined_existing = false;

    /* Try to top up quiver slots first */
    for (int slot = INVEN_QUIVER1; slot <= INVEN_QUIVER2 && leftover.number > 0; slot++)
    {
        object_type* slot_obj = &inventory[slot];
        if (!slot_obj->k_idx)
            continue;
        if (!object_similar(slot_obj, &leftover))
            continue;
        log_fletchery_object_state("combine_quiver_before_slot", slot_obj, slot);
        log_fletchery_object_state("combine_quiver_before_leftover", &leftover, -1);
        int before = leftover.number;
        object_absorb(slot_obj, &leftover);
        if (leftover.number != before)
        {
            combined_existing = true;
            log_fletchery_object_state("combine_quiver_after_slot", slot_obj, slot);
            log_fletchery_object_state("combine_quiver_after_leftover", &leftover, -1);
        }
    }

    /* Then fill stacks in the main pack */
    for (int slot = 0; slot < INVEN_PACK && leftover.number > 0; slot++)
    {
        object_type* slot_obj = &inventory[slot];
        if (!slot_obj->k_idx)
            continue;
        if (!object_similar(slot_obj, &leftover))
            continue;
        log_fletchery_object_state("combine_pack_before_slot", slot_obj, slot);
        log_fletchery_object_state("combine_pack_before_leftover", &leftover, -1);
        int before = leftover.number;
        object_absorb(slot_obj, &leftover);
        if (leftover.number != before)
        {
            combined_existing = true;
            log_fletchery_object_state("combine_pack_after_slot", slot_obj, slot);
            log_fletchery_object_state("combine_pack_after_leftover", &leftover, -1);
        }
    }

    /* Finally, attempt to add to any other equipped stacks */
    for (int slot = INVEN_WIELD; slot < INVEN_TOTAL && leftover.number > 0; slot++)
    {
        if (slot >= INVEN_QUIVER1 && slot <= INVEN_QUIVER2)
            continue;
        object_type* slot_obj = &inventory[slot];
        if (!slot_obj->k_idx)
            continue;
        if (!object_similar(slot_obj, &leftover))
            continue;
        log_fletchery_object_state("combine_equip_before_slot", slot_obj, slot);
        log_fletchery_object_state("combine_equip_before_leftover", &leftover, -1);
        int before = leftover.number;
        object_absorb(slot_obj, &leftover);
        if (leftover.number != before)
        {
            combined_existing = true;
            log_fletchery_object_state("combine_equip_after_slot", slot_obj, slot);
            log_fletchery_object_state("combine_equip_after_leftover", &leftover, -1);
        }
    }

    if (combined_existing)
    {
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }

    if (leftover.number <= 0)
    {
        log_debug("fletchery:distribute_fully_absorbed");
        return;
    }

    object_type carry_obj = leftover;
    int carry_slot = inven_carry(&carry_obj, true);
    log_debug("fletchery:inven_carry_result slot=%d leftover_after=%d",
        carry_slot, carry_obj.number);
    log_fletchery_object_state("carry_obj_after_inven_carry", &carry_obj, -1);

    if (carry_slot == SUPPLIES_INDEX)
    {
        char arrow_name[80];
        object_desc(arrow_name, sizeof(arrow_name), &carry_obj, true, 3);
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("You add %s to your supplies (%c).", arrow_name, label);
    }
    else if (carry_slot >= 0)
    {
        object_type* carried = &inventory[carry_slot];
        log_fletchery_object_state("carry_slot_object", carried, carry_slot);
        char arrow_name[80];
        object_desc(arrow_name, sizeof(arrow_name), carried, true, 3);
        msg_format("You have %s (%c).", arrow_name, index_to_label(carry_slot));

        if (carry_obj.number > 0)
        {
            if (drop_fletchered_arrows_near(&carry_obj))
                msg_print("Some arrows spill to the ground.");
            else
                msg_print("You lose track of some of the arrows.");
        }
    }
    else
    {
        if (drop_fletchered_arrows_near(&carry_obj))
            msg_print("Your pack is too full; you leave the arrows on the ground.");
        else
            msg_print("Your pack is too full, and you lose track of the arrows.");
    }

    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

static bool fletchery_choose_source(fletch_choice_t* out_choice)
{
    bool old_full = item_tester_full;
    int selection = -1;

    /* Only show fletchery candidates */
    item_tester_full = false;
    item_tester_hook = item_tester_hook_fletchery_source;

    if (!open_inventory_item_select_menu(USE_INVEN | USE_EQUIP | USE_FLOOR,
            "Choose fletchery source.",
            "You have nothing suitable for fletchery.", &selection))
    {
        item_tester_full = old_full;
        return false;
    }

    item_tester_full = old_full;

    if (selection == SUPPLIES_INDEX)
    {
        if (!choose_fletchery_supply_torch(&out_choice->index))
            return false;

        out_choice->type = FLETCH_SOURCE_SUPPLY;
        return true;
    }

    if (selection < 0)
    {
        out_choice->type = FLETCH_SOURCE_FLOOR;
        out_choice->index = 0 - selection;
    }
    else if (selection >= SUPPLIES_INDEX)
    {
        out_choice->type = FLETCH_SOURCE_SUPPLY;
        out_choice->index = selection - SUPPLIES_INDEX;
    }
    else if (selection >= INVEN_WIELD)
    {
        out_choice->type = FLETCH_SOURCE_EQUIP;
        out_choice->index = selection;
    }
    else
    {
        out_choice->type = FLETCH_SOURCE_INVEN;
        out_choice->index = selection;
    }

    return true;
}

void do_cmd_fletchery(void)
{
    object_type* o_ptr;
    object_type supply_source;
    fletch_choice_t choice;
    bool from_supply;

    if (!p_ptr->active_ability[S_ARC][ARC_FLETCHERY])
    {
        msg_print("You need the ability 'fletchery' to use this command.");
        return;
    }

    if (!fletchery_choose_source(&choice))
        return;

    bool from_floor = (choice.type == FLETCH_SOURCE_FLOOR);
    from_supply = (choice.type == FLETCH_SOURCE_SUPPLY);

    int source_index = choice.index;
    int floor_idx = from_floor ? source_index : 0;
    int supply_total = 0;

    if (from_floor)
        o_ptr = &o_list[floor_idx];
    else if (from_supply)
    {
        if (!build_fletchery_supply_source(source_index, &supply_source, &supply_total))
        {
            msg_print("You have nothing suitable for fletchery in your supplies.");
            return;
        }
        o_ptr = &supply_source;
    }
    else
        o_ptr = &inventory[source_index];

    bool is_arrow = (o_ptr->tval == TV_ARROW);
    bool is_torch = (o_ptr->tval == TV_LIGHT)
        && (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN);
    bool is_staff = (o_ptr->tval == TV_STAFF);

    if (is_arrow)
    {
        if (from_floor)
        {
            msg_print("You need to pick up those arrows before you can work on them.");
            return;
        }

        /* Take a turn */
        p_ptr->energy_use = 100;

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;

        msg_print(
            "You begin straightening and adjusting the feathering of the arrows.");

        p_ptr->fletch_item = source_index;
        p_ptr->fletching = o_ptr->number;
        fletchery_source_in_pack = (source_index < INVEN_WIELD);
        log_debug("fletchery:start source_index=%d in_pack=%d turns=%d",
            source_index, fletchery_source_in_pack ? 1 : 0, p_ptr->fletching);
        log_fletchery_object_state("start_source", o_ptr, source_index);
        remember_fletchery_source_snapshot(o_ptr);
        return;
    }

    if (is_torch || is_staff)
    {
        int max_convert = from_supply ? supply_total : o_ptr->number;
        if (max_convert <= 0)
        {
            msg_print("You have nothing to work with.");
            return;
        }

        int amount = get_quantity_action("Convert how many?", "Convert",
            max_convert);
        if (amount <= 0)
            return;

        /* Take a turn */
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;

        object_type source = *o_ptr;
        source.number = amount;

        char source_name[80];
        object_desc(source_name, sizeof(source_name), &source, true, 3);

        int arrows_per = is_staff ? 6 : 3;
        int produced_total = amount * arrows_per;

        msg_format("You carve %d +3 arrow%s from %s.", produced_total,
            (produced_total == 1) ? "" : "s", source_name);

        /* Remove the raw materials */
        if (from_floor)
        {
            floor_item_increase(floor_idx, -amount);
            floor_item_optimize(floor_idx);
        }
        else if (from_supply)
        {
            if (!consume_fletchery_supply_torches(source_index, amount))
            {
                msg_print("You no longer have enough torches in your supplies.");
                return;
            }
        }
        else
        {
            inven_item_increase(source_index, -amount);
            inven_item_optimize(source_index);
        }

        object_type arrow_proto;
        object_prep(&arrow_proto, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
        arrow_proto.number = produced_total;
        arrow_proto.att = 3;

        distribute_fletchered_arrows(&arrow_proto);
        return;
    }

    msg_print("That item cannot be used for fletchery.");

}
void finish_fletching(int turns_left)
{
    object_type source_template;
    object_type* o_ptr = NULL;
    int slots[INVEN_TOTAL];
    int remove_amounts[INVEN_TOTAL];
    int slot_count = 0;
    int count = 0;

    memset(remove_amounts, 0, sizeof(remove_amounts));
    log_debug("fletchery:finish begin turns_left=%d fletch_item=%d active_turns=%d snapshot=%d in_pack=%d",
        turns_left, p_ptr->fletch_item, p_ptr->fletching,
        fletchery_source_snapshot_valid ? 1 : 0, fletchery_source_in_pack ? 1 : 0);

    if (fletchery_source_snapshot_valid)
    {
        object_copy(&source_template, &fletchery_source_snapshot);
        count = source_template.number - turns_left;
        slot_count = collect_fletchery_source_slots(slots);
        log_fletchery_object_state("finish_snapshot", &source_template, p_ptr->fletch_item);
    }
    else if ((p_ptr->fletch_item >= 0) && (p_ptr->fletch_item < INVEN_TOTAL))
    {
        o_ptr = &inventory[p_ptr->fletch_item];
        object_copy(&source_template, o_ptr);
        count = o_ptr->number - turns_left;
        if (o_ptr->k_idx)
            slots[slot_count++] = p_ptr->fletch_item;
        log_fletchery_object_state("finish_fallback_source", o_ptr, p_ptr->fletch_item);
    }

    log_debug("fletchery:finish computed count=%d slot_count=%d", count, slot_count);

    if ((slot_count <= 0) || (count <= 0))
    {
        if (count <= 0)
        {
            msg_print("You did not manage to improve any arrows.");
        }
        else
        {
            msg_print("You can no longer find the arrows you were working on.");
            log_warn("finish_fletching: lost track of source arrows for slot %d",
                p_ptr->fletch_item);
        }

        clear_fletchery_source_snapshot();
        return;
    }

    /* Unstack if necessary */
    if (count > 0)
    {
        int source_total = source_template.number;
        int available_total = 0;
        int improved = 0;
        int remaining_original = 0;

        for (int i = 0; i < slot_count && available_total < source_total; i++)
        {
            int slot = slots[i];
            int available = inventory[slot].number;
            int used = MIN(source_total - available_total, available);

            if (used <= 0)
                continue;

            remove_amounts[i] = used;
            available_total += used;
            log_debug("fletchery:finish slot=%d available=%d remove=%d running_total=%d source_total=%d",
                slot, available, used, available_total, source_total);
        }

        improved = MIN(count, available_total);
        remaining_original = available_total - improved;
        log_debug("fletchery:finish improved=%d remaining_original=%d available_total=%d requested=%d",
            improved, remaining_original, available_total, count);

        if (available_total < count)
        {
            log_warn("finish_fletching: only found %d of %d arrows to improve",
                available_total, count);
        }

        if (improved <= 0)
        {
            msg_print("You can no longer find the arrows you were working on.");
            clear_fletchery_source_snapshot();
            return;
        }

        /* Message */
        msg_format("You improve %d arrows.", improved);

        object_type* i_ptr;
        object_type object_type_body;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Obtain a local object */
        object_copy(i_ptr, &source_template);

        /* Modify quantity */
        i_ptr->number = improved;
        i_ptr->att = 3;
        log_fletchery_object_state("finish_improved_proto", i_ptr, -1);

        /* Reduce the original source stacks. Delay optimization so pack slots stay stable. */
        for (int i = 0; i < slot_count; i++)
        {
            if (remove_amounts[i] <= 0)
                continue;

            log_fletchery_object_state("finish_remove_before", &inventory[slots[i]], slots[i]);
            inven_item_increase(slots[i], -remove_amounts[i]);
            log_fletchery_object_state("finish_remove_after", &inventory[slots[i]], slots[i]);
        }

        for (int i = slot_count - 1; i >= 0; i--)
        {
            if (remove_amounts[i] <= 0)
                continue;

            log_fletchery_object_state("finish_optimize_before", &inventory[slots[i]], slots[i]);
            inven_item_optimize(slots[i]);
            log_fletchery_object_state("finish_optimize_after", &inventory[slots[i]], slots[i]);
        }

        distribute_fletchered_arrows(i_ptr);

        if (remaining_original > 0)
        {
            object_type remainder = source_template;
            remainder.number = remaining_original;
            log_fletchery_object_state("finish_remainder_proto", &remainder, -1);
            distribute_fletchered_arrows(&remainder);
        }
    }
    else
    {
        msg_print("You did not manage to improve any arrows.");
    }

    clear_fletchery_source_snapshot();

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    p_ptr->redraw |= (PR_QUIVER | PR_ARC);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
}
