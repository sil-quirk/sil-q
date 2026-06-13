#include "angband.h"
#include "externs.h"
#include "sdl-config.h"

static void cmd6_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (!buf[0] || streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback ? fallback : "", buflen);
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

static void format_horn_prompt_name(char* buf, size_t max,
    const object_type* o_ptr, bool pref)
{
    char full[80];
    const char* horn_of;

    if (!buf || max == 0)
        return;

    buf[0] = '\0';

    if (!o_ptr || !o_ptr->k_idx)
        return;

    object_desc(full, sizeof(full), o_ptr, pref, 0);

    if (o_ptr->tval != TV_HORN)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    horn_of = strstr(full, "Horn of ");
    if (!horn_of)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    if (!pref)
    {
        SDL_strlcpy(buf, horn_of, max);
        return;
    }

    if (!strncmp(full, "The ", 4))
        strnfmt(buf, max, "The %s", horn_of);
    else if (!strncmp(full, "no more ", 8))
        strnfmt(buf, max, "no more %s", horn_of);
    else
        strnfmt(buf, max, "a %s", horn_of);
}

static void msg_print_object_identified(const object_type* o_ptr)
{
    char o_name[80];
    object_desc(o_name, sizeof(o_name), o_ptr, true, 0);
    msg_format("You identify %s.", o_name);
}

static const object_type* sanctity_target_excluded = NULL;

typedef struct sanctity_target_entry
{
    int item;
    object_type* o_ptr;
} sanctity_target_entry;

enum
{
    MAX_SANCTITY_TARGETS =
        INVEN_PACK + (INVEN_TOTAL - INVEN_WIELD) + MAX_FLOOR_STACK
};

static bool item_tester_hook_sanctity_target(const object_type* o_ptr)
{
    bool can_remove_jinx;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr == sanctity_target_excluded)
        return false;

    can_remove_jinx = p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING]
        && object_has_ego_flag4(o_ptr, TR4_JINX);

    if (!cursed_p(o_ptr)
        && ((o_ptr->ident & IDENT_UNCURSED)
            || (o_ptr->discount == INSCRIP_UNCURSED))
        && !can_remove_jinx)
        return false;

    if (cursed_p(o_ptr))
        return true;

    if (can_remove_jinx)
        return true;

    if (!object_known_p(o_ptr))
        return true;

    return false;
}

static int sanctity_collect_targets(sanctity_target_entry entries[],
    int max_entries, const object_type* gem_o_ptr)
{
    int count = 0;
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    if (!entries || max_entries <= 0)
        return 0;

    sanctity_target_excluded = gem_o_ptr;

    for (int i = 0; i < INVEN_PACK && count < max_entries; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_hook_sanctity_target(o_ptr))
            continue;

        entries[count].item = i;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < max_entries; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_hook_sanctity_target(o_ptr))
            continue;

        entries[count].item = i;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);
    for (int i = 0; i < floor_num && count < max_entries; i++)
    {
        int o_idx = floor_list[i];
        object_type* o_ptr = &o_list[o_idx];

        if (!item_tester_hook_sanctity_target(o_ptr))
            continue;

        entries[count].item = 0 - o_idx;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    sanctity_target_excluded = NULL;
    return count;
}

static bool sanctity_choose_target_from_entries(
    const sanctity_target_entry entries[], int count, int* out_item)
{
    int current = 0;
    int top = 0;
    int term_wid = 80;
    int term_hgt = 24;
    int list_row = 2;
    int help_row;
    int prompt_row;
    int page_size;
    bool steamdeck = steamdeck_controls_active();

    if (!entries || count <= 0 || !out_item)
        return false;

    if (Term)
        Term_get_size(&term_wid, &term_hgt);

    if (term_wid < 40)
        term_wid = 40;
    if (term_hgt < 8)
        term_hgt = 8;

    help_row = term_hgt - 2;
    prompt_row = term_hgt - 1;
    page_size = help_row - list_row;
    if (page_size < 1)
        page_size = 1;

    screen_save();

    while (true)
    {
        int visible_count;
        char buf[160];
        char key;

        if (current < top)
            top = current;
        if (current >= top + page_size)
            top = current - page_size + 1;

        visible_count = count - top;
        if (visible_count > page_size)
            visible_count = page_size;

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        ui_menu_click_set_touch_category(
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_begin(list_row, help_row - 1,
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_set_keys('8', '2', '6', '4');

        prt("Cleanse which item?", 0, 0);
        strnfmt(buf, sizeof(buf), "%d eligible sanctity target%s",
            count, (count == 1) ? "" : "s");
        prt(buf, 1, 0);

        for (int i = 0; i < visible_count; i++)
        {
            int row = list_row + i;
            int item = entries[top + i].item;
            object_type* o_ptr = entries[top + i].o_ptr;
            char prefix[32];
            char desc[80];
            char label[4];
            int desc_col = steamdeck ? 17 : 20;
            int max_desc = term_wid - desc_col - 1;
            bool highlighted = (top + i == current);
            byte label_attr = highlighted ? TERM_L_BLUE : TERM_WHITE;
            byte desc_attr;

            if (max_desc < 0)
                max_desc = 0;
            if (max_desc >= (int)sizeof(desc))
                max_desc = (int)sizeof(desc) - 1;

            if (item >= 0)
            {
                strnfmt(prefix, sizeof(prefix), "%-12s:", mention_use(item));
                object_desc(desc, sizeof(desc), o_ptr, true, 3);
            }
            else
            {
                strnfmt(prefix, sizeof(prefix), "%-12s:", "On floor");
                object_desc_floor(desc, sizeof(desc), o_ptr, true, 3);
            }
            desc[max_desc] = '\0';

            desc_attr = highlighted
                ? TERM_L_BLUE
                : object_display_color(o_ptr,
                    tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

            if (!steamdeck && i < 26)
                strnfmt(label, sizeof(label), "%c)", I2A(i));
            else
                SDL_strlcpy(label, "  ", sizeof(label));

            Term_erase(0, row, 255);
            Term_putstr(0, row, 2, label_attr, highlighted ? "> " : "  ");
            if (steamdeck)
                Term_putstr(2, row, -1, label_attr, prefix);
            else
            {
                Term_putstr(2, row, -1, label_attr, label);
                Term_putstr(5, row, -1, label_attr, prefix);
            }
            Term_putstr(desc_col, row, -1, desc_attr, desc);
            ui_menu_click_add_full_row(top + i, row);
        }

        for (int i = list_row + visible_count; i < help_row; i++)
            Term_erase(0, i, 255);

        if (count > page_size)
        {
            strnfmt(buf, sizeof(buf), "Showing %d-%d of %d",
                top + 1, top + visible_count, count);
            prt(buf, help_row, 0);
        }
        else
        {
            prt("", help_row, 0);
        }

        if (steamdeck)
        {
            char confirm_label[16];
            char back_label[16];
            char prompt_buf[80];
            const char* variants[3];
            char prompt_full[80];
            char prompt_mid[80];
            char prompt_short[80];

            cmd6_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
                sizeof(confirm_label));
            cmd6_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad choose  %s select  %s cancel", confirm_label,
                back_label);
            strnfmt(prompt_mid, sizeof(prompt_mid), "%s select  %s cancel",
                confirm_label, back_label);
            strnfmt(prompt_short, sizeof(prompt_short), "%s select",
                confirm_label);
            variants[0] = prompt_full;
            variants[1] = prompt_mid;
            variants[2] = prompt_short;
            terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                term_wid, false, variants, N_ELEMENTS(variants));
            prt(prompt_buf, prompt_row, 0);
            ui_menu_click_add_text_token(-2, 0, prompt_row, prompt_buf,
                "select");
            ui_menu_click_add_text_token(-1, 0, prompt_row, prompt_buf,
                "cancel");
        }
        else if (sdl_touch_only_device_active())
        {
            char prompt_buf[80];
            const char* variants[] = {
                "Tap a row to select, tap away to exit",
                "Tap to select, tap away to exit",
                "Tap to select"
            };
            terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                term_wid, false, variants, N_ELEMENTS(variants));
            prt(prompt_buf, prompt_row, 0);
        }
        else
        {
            char prompt_buf[80];
            const char* variants[] = {
                "Letters choose  Dir move  Enter select  Esc cancel",
                "Letters choose  Enter select  Esc cancel",
                "Enter select  Esc cancel"
            };
            terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                term_wid, false, variants, N_ELEMENTS(variants));
            prt(prompt_buf, prompt_row, 0);
            ui_menu_click_add_text_token(-2, 0, prompt_row, prompt_buf,
                "select");
            ui_menu_click_add_text_token(-1, 0, prompt_row, prompt_buf,
                "cancel");
        }
        Term_fresh();

        key = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < count)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != current)
                    {
                        current = clicked_choice;
                        continue;
                    }
                    key = '\r';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = '\r';
            }
        }

        if (steamdeck && key == steamdeck_back_key())
        {
            ui_menu_click_clear();
            ui_scroll_area_clear();
            screen_load();
            return false;
        }

        switch (key)
        {
        case ESCAPE:
            ui_menu_click_clear();
            ui_scroll_area_clear();
            screen_load();
            return false;

        case '\r':
        case '\n':
        case ' ':
#ifdef KC_ENTER
        case KC_ENTER:
#endif
            *out_item = entries[current].item;
            ui_menu_click_clear();
            ui_scroll_area_clear();
            screen_load();
            return true;

        case '8':
        case 'k':
        case 'K':
#ifdef ARROW_UP
        case ARROW_UP:
#endif
            current = (current > 0) ? current - 1 : count - 1;
            break;

        case '2':
        case 'j':
        case 'J':
#ifdef ARROW_DOWN
        case ARROW_DOWN:
#endif
            current = (current + 1 < count) ? current + 1 : 0;
            break;

        default:
        {
            int pick;

            if (steamdeck && key == steamdeck_back_key())
            {
                ui_menu_click_clear();
                ui_scroll_area_clear();
                screen_load();
                return false;
            }

            if (steamdeck && key == steamdeck_confirm_key())
            {
                *out_item = entries[current].item;
                ui_menu_click_clear();
                ui_scroll_area_clear();
                screen_load();
                return true;
            }

            if (steamdeck)
                break;

            if (!isalpha((unsigned char)key))
                break;

            pick = A2I((char)tolower((unsigned char)key));
            if (pick >= 0 && pick < visible_count)
            {
                *out_item = entries[top + pick].item;
                ui_menu_click_clear();
                ui_scroll_area_clear();
                screen_load();
                return true;
            }

            break;
        }
        }
    }
}

static bool sanctity_choose_target(const object_type* gem_o_ptr,
    object_type** target_o_ptr)
{
    int chosen_item;
    int count;
    sanctity_target_entry entries[MAX_SANCTITY_TARGETS];

    if (!target_o_ptr)
        return false;

    count = sanctity_collect_targets(entries, N_ELEMENTS(entries), gem_o_ptr);
    if (count <= 0)
    {
        msg_print("You have no target to cleanse.");
        return false;
    }

    if (!sanctity_choose_target_from_entries(entries, count, &chosen_item))
        return false;

    *target_o_ptr = (chosen_item >= 0) ? &inventory[chosen_item]
        : &o_list[0 - chosen_item];
    return ((*target_o_ptr != NULL) && (*target_o_ptr)->k_idx);
}

/*
 * This file includes code for eating food, drinking potions,
 * using staffs, playing instruments, and activating artefacts.
 *
 * In all cases, if the player becomes "aware" of the item's use
 * by testing it, mark it as "aware" and reward some experience
 * based on the object's level, always rounding up.  If the player
 * remains "unaware", mark that object "kind" as "tried".
 *
 * Note the overly paranoid warning about potential pack
 * overflow, which allows the player to use and drop a stacked item.
 *
 * In all "unstacking" scenarios, the "used" object is "carried" as if
 * the player had just picked it up.  In particular, this means that if
 * the use of an item induces pack overflow, that item will be dropped.
 *
 * For simplicity, these routines induce a full "pack reorganization"
 * which not only combines similar items, but also reorganizes various
 * items to obey the current "sorting" method.  This may require about
 * 400 item comparisons, but only occasionally.
 *
 * There may be a BIG problem with any "effect" that can cause "changes"
 * to the inventory.  For example, a "scroll of recharging" used to be
 * able to cause a staff to "disappear", moving the inventory up.  Luckily, the
 * scrolls all appear BEFORE the staffs/wands, so this is not a problem.
 * But, for example, a "staff of recharging" could cause MAJOR problems.
 * In such a case, it will be best to either (1) "postpone" the effect
 * until the end of the function, or (2) "change" the effect, say, into
 * giving a staff "negative" charges, or "turning a staff into a stick".
 * It seems as though a "rod of recharging" might in fact cause problems.
 * The basic problem is that the act of recharging (and destroying) an
 * item causes the inducer of that action to "move", causing "o_ptr" to
 * no longer point at the correct item, with horrifying results.
 *
 * Note that food/potions/scrolls no longer use bit-flags for effects,
 * but instead use the "sval" (which is also used to sort the objects).
 */

/*
 * Eat some food (from the pack or floor)
 */
void do_cmd_eat_food(object_type* default_o_ptr, int default_item)
{
    int item;
    bool ident;
    bool aware;
    int kind_index;

    object_type* o_ptr = NULL;
    int supply_index = supplies_current_action();
    bool from_supplies = (supply_index >= 0);
    cptr q, s;

    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = from_supplies ? SUPPLIES_INDEX : default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict choices to food */
        item_tester_tval = TV_FOOD;

        /* Get an item */
        q = "Eat which item? ";
        s = "You have nothing to eat.";
        supplies_set_pending_action(SUPPLY_MENU_ACTION_USE,
            supplies_has_group(SUPPLY_GROUP_HERBS) ? SUPPLY_GROUP_HERBS
                                                   : SUPPLY_GROUP_FOOD,
            true);
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
                supplies_has_group(SUPPLY_GROUP_HERBS) ? SUPPLY_GROUP_HERBS
                                                       : SUPPLY_GROUP_FOOD,
                true, true);
            return;
        }

        supplies_clear_pending_action();

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

        from_supplies = false;
        supply_index = -1;
    }

    if (!o_ptr)
        return;

    /* Sound */
    sound(MSG_EAT);

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Identity not known yet */
    ident = false;

    // Save the k_idx and awareness info
    kind_index = o_ptr->k_idx;
    aware = object_aware_p(o_ptr);

    /* Eat the food */
    use_object(o_ptr, &ident);

    /* We have tried it */
    object_tried(o_ptr);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* The player is now aware of the object */
    if (ident && !object_aware_p(o_ptr))
    {
        object_aware(o_ptr);
        msg_print_object_identified(o_ptr);
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Destroy a food in the pack or supplies */
    if (from_supplies && supply_index >= 0)
    {
        supplies_consume_quantity(supply_index, 1);
        supplies_refresh_entry(supply_index);
    }
    else if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Destroy a food on the floor */
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }

    // allow autoinscribing of the herb
    if (!ident && !aware)
    {
        if (easter_time())
        {
            if (get_check("Autoinscribe this easter egg type? "))
            {
                do_cmd_autoinscribe_item(kind_index);
            }
        }
        else
        {
            if (get_check((o_ptr->sval <= SV_FOOD_SICKNESS)
                    ? "Autoinscribe this herb type? "
                    : "Autoinscribe this food type? "))
            {
                do_cmd_autoinscribe_item(kind_index);
            }
        }
    }
}

/*
 * Quaff a potion (from the pack or the floor)
 */
void do_cmd_quaff_potion(object_type* default_o_ptr, int default_item)
{
    int item;
    bool ident;
    bool aware;
    int kind_index;
    object_type* o_ptr = NULL;
    int supply_index = supplies_current_action();
    bool from_supplies = (supply_index >= 0);
    cptr q, s;

    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = from_supplies ? SUPPLIES_INDEX : default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict choices to potions */
        item_tester_tval = TV_POTION;

        /* Get an item */
        q = "Quaff which potion? ";
        s = "You have no potions to quaff.";
        supplies_set_pending_action(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_POTIONS, true);
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
        {
            supplies_clear_pending_action();
            return;
        }

        if (item == SUPPLIES_INDEX)
        {
            supplies_clear_pending_action();
            open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_POTIONS, true, true);
            return;
        }

        supplies_clear_pending_action();

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

        from_supplies = false;
        supply_index = -1;
    }

    if (!o_ptr)
        return;

    /* Sound */
    sound(MSG_QUAFF);

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Not identified yet */
    ident = false;

    // Save the k_idx and awareness info
    kind_index = o_ptr->k_idx;
    aware = object_aware_p(o_ptr);

    /* Quaff the potion */
    use_object(o_ptr, &ident);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* The item has been tried */
    object_tried(o_ptr);

    /* An identification was made */
    if (ident && !object_aware_p(o_ptr))
    {
        object_aware(o_ptr);
        msg_print_object_identified(o_ptr);
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Destroy a potion in the pack or supplies */
    if (from_supplies && supply_index >= 0)
    {
        supplies_consume_quantity(supply_index, 1);
        supplies_refresh_entry(supply_index);
    }
    else if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Destroy a potion on the floor */
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }

    // allow autoinscribing of the potion
    if (!ident && !aware)
    {
        if (get_check("Autoinscribe this potion type? "))
        {
            do_cmd_autoinscribe_item(kind_index);
        }
    }
}

/*
 * Play an instrument
 */
void do_cmd_play_instrument(object_type* default_o_ptr, int default_item)
{
    bool ident;

    object_type* o_ptr = NULL;

    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
    }
    /* Get an item */
    else
    {
        object_type* horn_slot = &inventory[INVEN_HORN];

        if (horn_slot->k_idx)
        {
            o_ptr = horn_slot;
        }
        else
        {
            msg_print("You are not carrying a horn.");
            return;
        }
    }

    if (!o_ptr)
        return;

    if (o_ptr->tval != TV_HORN)
    {
        msg_print("You can only sound a horn.");
        return;
    }

    if (o_ptr != &inventory[INVEN_HORN])
    {
        object_type* equipped = &inventory[INVEN_HORN];
        char incoming_name[80];
        char equipped_name[80];
        char prompt[160];
        const char* source = "your equipment";

        if (default_item < 0)
            source = "the floor";
        else if (default_item < INVEN_WIELD)
            source = "your pack";

        format_horn_prompt_name(incoming_name, sizeof(incoming_name), o_ptr, true);

        if (equipped->k_idx)
        {
            format_horn_prompt_name(
                equipped_name, sizeof(equipped_name), equipped, false);
            msg_format("You cannot sound a horn from %s.", source);
            strnfmt(prompt, sizeof(prompt),
                "Replace your %s with %s?",
                equipped_name, incoming_name);
        }
        else
        {
            msg_format("You cannot sound a horn from %s.", source);
            strnfmt(prompt, sizeof(prompt),
                "Equip %s now?",
                incoming_name);
        }

        if (get_check(prompt))
            do_cmd_wield(o_ptr, default_item);
        return;
    }

    /* Not identified yet */
    ident = false;

    /* Play the instrument */
    if (!use_object(o_ptr, &ident))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    // end the current song
    change_song(SNG_NOTHING);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Tried the object */
    object_tried(o_ptr);

    /* Experiencing effects helps identify smithing-difficulty items, but does not auto-ID them. */
    if (ident)
    {
        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
        }
        else if (!object_aware_p(o_ptr))
        {
            object_aware(o_ptr);
            msg_print_object_identified(o_ptr);
        }
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

/*
 * Use a staff
 *
 * One charge of one staff disappears.
 *
 * Hack -- staffs of identify can be "cancelled".
 */
void do_cmd_activate_staff(object_type* default_o_ptr, int default_item)
{
    int item;

    bool ident;

    object_type* o_ptr = NULL;

    bool use_charge;

    int supply_index = supplies_current_action();
    bool from_supplies = (supply_index >= 0);
    
    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = from_supplies ? SUPPLIES_INDEX : default_item;
    }
    /* Get an item */
    else
    {
        object_type* staff_slot = &inventory[INVEN_STAFF];

        if (staff_slot->k_idx)
        {
            o_ptr = staff_slot;
            item = INVEN_STAFF;
            from_supplies = false;
            supply_index = -1;
        }
        else
        {
            msg_print("You are not wielding a walking staff.");
            return;
        }
    }

    if (!o_ptr)
        return;

    if (o_ptr->tval != TV_STAFF)
    {
        msg_print("You can only activate a staff.");
        return;
    }

    if (o_ptr->tval == TV_STAFF && o_ptr != &inventory[INVEN_STAFF])
    {
        object_type* wielded = &inventory[INVEN_STAFF];
        char incoming_name[80];
        char equipped_name[80];
        char prompt[160];
        const char* source = from_supplies ? "your supplies" : (default_item >= 0 ? "your pack" : "the floor");

        if (!from_supplies && item < 0
            && player_channel_floor_staff(o_ptr, 0 - item))
        {
            return;
        }

        format_staff_prompt_name(incoming_name, sizeof(incoming_name), o_ptr, true);

        if (from_supplies)
        {
            msg_print("You cannot use a staff from supplies.");
            msg_print("Move it to your pack and equip it first.");
            return;
        }

        if (wielded->k_idx)
        {
            format_staff_prompt_name(
                equipped_name, sizeof(equipped_name), wielded, false);
            msg_format("You cannot activate a staff from %s.", source);
            strnfmt(prompt, sizeof(prompt),
                "Replace your %s with %s?",
                equipped_name, incoming_name);
        }
        else
        {
            msg_format("You cannot activate a staff from %s.", source);
            strnfmt(prompt, sizeof(prompt),
                "Equip %s now?",
                incoming_name);
        }

        if (get_check(prompt))
        {
            do_cmd_wield(o_ptr, default_item);
        }
        return;
    }

    if (o_ptr->ident & (IDENT_EMPTY))
    {
        msg_print("The staff has no charges left.");
        return;
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Not identified yet */
    ident = false;

    /* Notice empty staffs */
    if (o_ptr->pval < CHANNELING_CHARGE_MULTIPLIER)
    {
        flush();
        msg_print("The staff has no charges left.");
        o_ptr->ident |= (IDENT_EMPTY);
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        p_ptr->window |= (PW_INVEN);
        return;
    }

    /* Sound */
    sound(MSG_ZAP);

    /* Use the staff */
    use_charge = use_object(o_ptr, &ident);

    // Break the truce
    break_truce(false);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Tried the item */
    object_tried(o_ptr);

    /* An identification was made */
    if (ident && !object_aware_p(o_ptr))
    {
        object_aware(o_ptr);
        msg_print_object_identified(o_ptr);
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Hack -- some uses are "free" */
    if (!use_charge)
        return;

    /* Consume the item */
    /* Staffs always expend their bundled charges */
    o_ptr->pval -= CHANNELING_CHARGE_MULTIPLIER;
    if (o_ptr->pval < 0)
        o_ptr->pval = 0;
    // mark times used
    o_ptr->xtra1++;

    if (from_supplies && supply_index >= 0)
    {
        supplies_refresh_entry(supply_index);
    }
    else if (item >= 0)
    {
        inven_item_charges(item);
    }
    else
    {
        floor_item_charges(0 - item);
    }
}

/*
 * Use a gem
 *
 * One gem is consumed on use.
 */
void do_cmd_use_gem(object_type* default_o_ptr, int default_item)
{
    int item;
    bool ident;
    object_type* o_ptr = NULL;
    object_type* sanctity_target_o_ptr = NULL;
    bool use_charge;

    int supply_index = supplies_current_action();
    bool from_supplies = (supply_index >= 0);
    cptr q, s;

    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = from_supplies ? SUPPLIES_INDEX : default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict choices to gems */
        item_tester_tval = TV_GEM;

        /* Get an item */
        q = "Use which gem? ";
        s = "You have no gems to use.";
        supplies_set_pending_action(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_GEMS, true);
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
        {
            supplies_clear_pending_action();
            return;
        }

        if (item == SUPPLIES_INDEX)
        {
            supplies_clear_pending_action();
            open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_GEMS, true, true);
            return;
        }

        supplies_clear_pending_action();

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

        from_supplies = false;
        supply_index = -1;
    }

    if (!o_ptr)
        return;

    if (o_ptr->number <= 0)
    {
        msg_print("You have no gems left.");
        return;
    }

    if (o_ptr->sval == SV_GEM_SANCTITY)
    {
        if (!sanctity_choose_target(o_ptr, &sanctity_target_o_ptr))
        {
            return;
        }
    }

    /* Sound */
    sound(MSG_USE_GEM);

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Not identified yet */
    ident = false;

    /* Use the gem */
    if (o_ptr->sval == SV_GEM_SANCTITY)
        use_charge = use_sanctity_gem_on(sanctity_target_o_ptr, &ident);
    else
        use_charge = use_object(o_ptr, &ident);

    // Break the truce
    break_truce(false);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Tried the item */
    object_tried(o_ptr);

    /* An identification was made */
    if (ident && !object_aware_p(o_ptr))
    {
        object_aware(o_ptr);
        msg_print_object_identified(o_ptr);
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Hack -- some uses are "free" */
    if (!use_charge)
        return;

    /* Consume the item */
    o_ptr->xtra1++;

    if (from_supplies && supply_index >= 0)
    {
        supplies_consume_quantity(supply_index, 1);
        supplies_refresh_entry(supply_index);
    }
    else if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }
}

/*
 * Hook to determine if an object is activatable
 */
static bool item_tester_hook_activate(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    /* Not known */
    if (!object_known_p(o_ptr))
        return (false);

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Check activation flag */
    if (f3 & (TR3_ACTIVATE))
        return (true);

    /* Assume not */
    return (false);
}

/*
 * Activate a wielded object.  Wielded objects never stack.
 * And even if they did, activatable objects never stack.
 *
 * Note that it always takes a turn to activate an artefact, even if
 * the user hits "escape" at the "direction" prompt.
 */
void do_cmd_activate(void)
{
    int item, lev, score, difficulty;
    bool ident;
    object_type* o_ptr;

    cptr q, s;

    /* Prepare the hook */
    item_tester_hook = item_tester_hook_activate;

    /* Get an item */
    q = "Activate which item? ";
    s = "You have nothing to activate.";
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

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Extract the item level */
    lev = k_info[o_ptr->k_idx].level;

    /* Hack -- use artefact level instead */
    if (artefact_p(o_ptr))
        lev = a_info[o_ptr->name1].level;

    /* Base chance of success */
    score = p_ptr->skill_use[S_WIL];

    // Base difficulty
    difficulty = lev / 2;

    /* Confusion hurts skill */
    if (p_ptr->confused)
        difficulty += 5;

    /* Roll for usage */
    if (skill_check(PLAYER, score, difficulty, NULL) <= 0)
    {
        flush();
        msg_print("You could not draw upon its powers.");
        return;
    }

    /* Sound */
    sound(MSG_ACTIVATE);

    /* Activate the object */
    (void)use_object(o_ptr, &ident);
}
