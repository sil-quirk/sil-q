/* File: spell/spell-identify.c */

#include "angband.h"
#include "externs.h"
#include "object/object-ui-identify.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_digger(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    object_flags(o_ptr, &f1, &f2, &f3);

    if ((f1 & (TR1_TUNNEL)) && (o_ptr->pval > 0))
    {
        return (true);
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_wieldable_ided_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_wieldable_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    case TV_BOW:
    case TV_ARROW:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_ided_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    case TV_BOW:
    case TV_ARROW:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "armour"
 */
bool item_tester_hook_ided_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_BOOTS:
    case TV_GLOVES:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "armour"
 */
bool item_tester_hook_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_BOOTS:
    case TV_GLOVES:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify non-herb food
 */
bool item_tester_hook_non_herb_food(const object_type* o_ptr)
{
    if ((o_ptr->tval == TV_FOOD) && (o_ptr->pval > 300))
        return (true);

    return (false);
}

/*
 * Hook to specify light with fuel or that does not need fuel
 */
bool item_tester_hook_light_with_fuel(const object_type* o_ptr)
{
    if (o_ptr->tval != TV_LIGHT)
        return (false);

    if (o_ptr->timeout < 1 && fuelable_light_p(o_ptr))
        return (false);

    return (true);
}

/*
 * Hook to specify "enchantable amulet"
 */
bool item_tester_hook_enchantable_amulet(const object_type* o_ptr)
{
    if ((o_ptr->tval == TV_AMULET) && (o_ptr->pval > 0))
        return (true);

    return (false);
}

/*
 * Identify an object chosen from the unified unidentified list.
 * Returns true if an item was identified.
 */
bool ident_spell(bool include_floor)
{
    int item;
    object_type* o_ptr;

    if (!display_unified_identify_menu(include_floor, &item, &o_ptr))
        return false;

    do_ident_item(item, o_ptr);

    return true;
}

/*
 * Hook for "get_item()".  Determine if something is rechargable.
 */
bool item_tester_hook_recharge(const object_type* o_ptr)
{
    /* Recharge staffs */
    if (o_ptr->tval == TV_STAFF)
        return (true);

    /* Nope */
    return (false);
}

typedef struct recharge_target_entry
{
    int item;
    object_type* o_ptr;
} recharge_target_entry;

enum
{
    MAX_RECHARGE_TARGETS =
        INVEN_PACK + (INVEN_TOTAL - INVEN_WIELD) + MAX_FLOOR_STACK
};

static int recharge_collect_targets(recharge_target_entry entries[],
    int max_entries)
{
    int count = 0;
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    if (!entries || max_entries <= 0)
        return 0;

    for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < max_entries; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_hook_recharge(o_ptr))
            continue;

        entries[count].item = i;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    for (int i = 0; i < INVEN_PACK && count < max_entries; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_hook_recharge(o_ptr))
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

        if (!item_tester_hook_recharge(o_ptr))
            continue;

        entries[count].item = 0 - o_idx;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    return count;
}

static bool recharge_choose_target(const recharge_target_entry entries[],
    int count, int* out_item)
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

        prt("Recharge which staff?", 0, 0);
        strnfmt(buf, sizeof(buf),
            "%d rechargeable target%s",
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
            strnfmt(buf, sizeof(buf),
                "Showing %d-%d of %d",
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

            spells2_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            spells2_prompt_label(steamdeck_back_key(), "B", back_label,
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
                "Tap a row to recharge, tap away to exit",
                "Tap to recharge, tap away to exit",
                "Tap to recharge"
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

/*
 * Recharge a staff from the pack, equipment, or on the floor.
 *
 * Mage -- Recharge I --> recharge(5)
 * Mage -- Recharge II --> recharge(40)
 * Mage -- Recharge III --> recharge(100)
 *
 * Priest -- Recharge --> recharge(15)
 *
 * Scroll of recharging --> recharge(60)
 *
 * recharge(20) = 1/6 failure for empty 10th level wand
 * recharge(60) = 1/10 failure for empty 10th level wand
 *
 * It is harder to recharge high level, and highly charged wands.
 *
 * XXX XXX XXX Beware of "sliding index errors".
 *
 * Should probably not "destroy" over-charged items, unless we
 * "replace" them by, say, a broken stick or some such.  The only
 * reason this is okay is because "scrolls of recharging" appear
 * BEFORE all staves/wands/rods in the inventory.  Note that the
 * new "auto_sort_pack" option would correctly handle replacing
 * the "broken" wand with any other item (i.e. a broken stick).
 *
 */
bool recharge(int num)
{
    int item;
    int target_count;

    object_type* o_ptr;
    recharge_target_entry targets[MAX_RECHARGE_TARGETS];

    target_count = recharge_collect_targets(targets, N_ELEMENTS(targets));
    if (target_count <= 0)
    {
        msg_print("You have nothing to recharge.");
        return (false);
    }

    if (!recharge_choose_target(targets, target_count, &item))
        return (false);

    /* Get the item (in the pack) */
    if (item >= 0)
        o_ptr = &inventory[item];

    /* Get the item (on the floor) */
    else
        o_ptr = &o_list[0 - item];

    /* Attempt to Recharge a staff, or handle failure to recharge . */
    if (o_ptr->tval == TV_STAFF)
    {
        if (o_ptr->sval == SV_STAFF_RECHARGING
            && p_ptr->active_ability[S_WIL][WIL_CHANNELING])
        {
            num /= 2;
        }

        /* Recharge the staff. */
        o_ptr->pval += num;

        if (object_aware_p(o_ptr) && (o_ptr->ident & (IDENT_EMPTY)))
        {
            object_aware(o_ptr);
            object_known(o_ptr);
        }

        /* Hack -- we no longer think the item is empty */
        o_ptr->ident &= ~(IDENT_EMPTY);
    }

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Something was done */
    return (true);
}

/*
 * Hook to specify "arrows"
 */
bool item_tester_hook_ided_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return false;
    }
    }

    return (false);
}

/*
 * Hook to specify "arrows"
 */
bool item_tester_hook_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify ordinary arrows
 */
bool item_tester_hook_ordinary_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        if (o_ptr->name1 || object_has_ego(o_ptr) || o_ptr->att > 0)
            return false;
        return true;
    }
    }

    return false;
}

/*
 * Identifies all objects in the equipment, inventory and supplies,
 * announcing each one.
 */
void identify_and_describe_pack(void)
{
    int item;
    object_type* o_ptr;

    /* Identify equipment */
    for (item = INVEN_WIELD; item < INVEN_TOTAL; item++)
    {
        /* Get the object */
        o_ptr = &inventory[item];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        do_ident_item(item, o_ptr);
    }

    /* Identify inventory */
    for (item = 0; item < INVEN_WIELD; item++)
    {
        /* Get the object */
        o_ptr = &inventory[item];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        do_ident_item(item, o_ptr);
    }

    /* Identify supplies */
    int supply_count = supplies_entry_count();
    for (int supply_idx = 0; supply_idx < supply_count; supply_idx++)
    {
        o_ptr = supplies_entry_at(supply_idx);
        if (!o_ptr || !o_ptr->k_idx)
            continue;
        if (object_known_p(o_ptr))
            continue;

        do_ident_item(SUPPLIES_INDEX + supply_idx, o_ptr);
    }
}

/* Mass-identify handler */
bool mass_identify(int rad)
{
    /* Direct the ball to the player */
    target_set_location(p_ptr->py, p_ptr->px);

    /* Cast the ball spell */
    fire_ball(GF_IDENTIFY, 5, 0, 0, -1, rad);

    /* Identify equipment, inventory and supplies */
    identify_and_describe_pack();

    /* This spell always works */
    return (true);
}

/*
 * Execute some common code of the identify spells.
 * "item" is used to print the slot occupied by an object in equip/inven.
 * ANY negative value assigned to "item" can be used for specifying an object
 * on the floor (they don't have a slot, example: the code used to handle
 * GF_IDENTIFY in project_o).
 */
void do_ident_item(int item, object_type* o_ptr)
{
    char o_name[80];

    /* Identify it */
    object_aware(o_ptr);
    object_known(o_ptr);

    /* Apply an autoinscription, if necessary */
    apply_autoinscription(o_ptr);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Description */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Describe */
    if (item >= SUPPLIES_INDEX)
    {
        int supply_index = item - SUPPLIES_INDEX;
        msg_format("In your supplies: %s.", o_name);
        supplies_refresh_entry(supply_index);
    }
    else if (item >= INVEN_WIELD)
    {
        msg_format(
            "%^s: %s (%c).", describe_use(item), o_name, index_to_label(item));
    }
    else if (item >= 0)
    {
        msg_format("In your pack: %s (%c).", o_name, index_to_label(item));
    }
    else
    {
        msg_format("On the ground: %s.", o_name);
    }
}
