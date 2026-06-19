/* File: object/object-ui-enhanced.c */

#include "angband.h"
#include "externs.h"
#include "object/object-ui-enhanced.h"
#include "object/object-internal.h"
#include "log/log.h"
#include "sdl-config.h"
#include "supplies.h"
#include <ctype.h>
#include <stdio.h>


static int enhanced_menu_primary_select_action(void)
{
    return (current_menu_command == 'x') ? ENHANCED_ACTION_EXAMINE
                                         : ENHANCED_ACTION_USE;
}

/*
 * Enhanced inventory display with scrolling and navigation
 * EXACTLY replicates show_inven() algorithm then adds highlighting
 */
#define MAX_COMPARE_LINES 2
#define MAX_DESCRIPTION_COMPARE_ITEMS 32
#define ENHANCED_MENU_CLICK_SWITCH (-1000000)
#define ENHANCED_MENU_CLICK_DROP (-1000001)

static void enhanced_menu_highlight_prompt_token(cptr text, cptr token,
    byte attr)
{
    cptr match;

    if (!text || !token || !token[0])
        return;

    match = strstr(text, token);
    if (!match)
        return;

    Term_putstr((int)(match - text), 0, (int)strlen(token), attr, token);
}

static bool enhanced_menu_format_prompt(char* out, size_t out_size,
    int term_wid, bool use_story_font, bool equipment)
{
    const bool controller_controls = steamdeck_controls_active();
    const bool examine_mode = (current_menu_command == 'x');
    const char* context = equipment ? " (Equipped)" : " (Inventory)";
    const char* action = equipment ? "remove" : (examine_mode ? "examine" : "use");
    char full[192];
    char medium[192];
    char short_form[160];
    char tiny[128];
    const char* variants[4];

    if (!out || out_size == 0)
        return false;

    if (sdl_touch_only_device_active())
    {
        char touch_long[96];
        char touch_mid[64];
        const char* touch_variants[3];

        /* The callers register tappable tokens by scanning this string for
         * the words "drop" and "cycle", so keep them present on the wider
         * variants or those affordances would silently disappear on touch. */
        strnfmt(touch_long, sizeof(touch_long),
            "Tap a row to %s, tap drop or cycle, tap away to exit", action);
        strnfmt(touch_mid, sizeof(touch_mid), "Tap to %s; drop; cycle",
            action);
        touch_variants[0] = touch_long;
        touch_variants[1] = touch_mid;
        touch_variants[2] = "Tap a row";
        terminal_prompt_pick_variant(out, out_size, term_wid - 1,
            use_story_font, touch_variants, N_ELEMENTS(touch_variants));
        return false;
    }

    if (controller_controls)
    {
        char confirm_label[16];
        char desc_label[16];
        char cycle_label[16] = "";
        bool show_cycle = false;

        inventory_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        inventory_prompt_label(steamdeck_info_key(), "RS Right", desc_label,
            sizeof(desc_label));

        if (current_menu_command == 'u' || current_menu_command == 'x')
        {
            inventory_prompt_label(current_menu_command,
                current_menu_command == 'u' ? "X" : "RS Right",
                cycle_label, sizeof(cycle_label));
            show_cycle = true;
        }
        else if (inventory_menu_same_button_cycle_enabled())
        {
            inventory_prompt_label(
                equipment ? steamdeck_prev_page_key() : steamdeck_next_page_key(),
                equipment ? "L1" : "R1", cycle_label, sizeof(cycle_label));
            show_cycle = true;
        }

        if (examine_mode)
        {
            if (show_cycle)
            {
                strnfmt(full, sizeof(full),
                    "%s %s  D-pad left drop  %s cycle%s",
                    confirm_label, action, cycle_label, context);
                strnfmt(medium, sizeof(medium),
                    "%s %s  D-left drop  %s cycle",
                    confirm_label, action, cycle_label);
                strnfmt(short_form, sizeof(short_form), "%s %s  %s cycle",
                    confirm_label, action, cycle_label);
                strnfmt(tiny, sizeof(tiny), "%s %s", confirm_label, action);
            }
            else
            {
                strnfmt(full, sizeof(full), "%s %s  D-pad left drop%s",
                    confirm_label, action, context);
                strnfmt(medium, sizeof(medium), "%s %s  D-left drop",
                    confirm_label, action);
                strnfmt(short_form, sizeof(short_form), "%s %s",
                    confirm_label, action);
                SDL_strlcpy(tiny, short_form, sizeof(tiny));
            }
        }
        else if (show_cycle)
        {
            strnfmt(full, sizeof(full),
                "%s %s  %s details  D-pad left drop  %s cycle%s",
                confirm_label, action, desc_label, cycle_label, context);
            strnfmt(medium, sizeof(medium),
                "%s %s  %s details  D-left drop  %s cycle",
                confirm_label, action, desc_label, cycle_label);
            strnfmt(short_form, sizeof(short_form),
                "%s %s  %s details  %s cycle",
                confirm_label, action, desc_label, cycle_label);
            strnfmt(tiny, sizeof(tiny), "%s %s  D-left drop",
                confirm_label, action);
        }
        else
        {
            strnfmt(full, sizeof(full),
                "%s %s  %s details  D-pad left drop%s",
                confirm_label, action, desc_label, context);
            strnfmt(medium, sizeof(medium),
                "%s %s  %s details  D-left drop",
                confirm_label, action, desc_label);
            strnfmt(short_form, sizeof(short_form), "%s %s  %s details",
                confirm_label, action, desc_label);
            strnfmt(tiny, sizeof(tiny), "%s %s", confirm_label, action);
        }
    }
    else
    {
        bool show_cycle = (current_menu_command == 'u'
            || current_menu_command == 'x');

        if (examine_mode)
        {
            if (show_cycle)
            {
                strnfmt(full, sizeof(full),
                    "Space examine  Left drop  %c cycle%s",
                    current_menu_command, context);
                strnfmt(medium, sizeof(medium),
                    "Space examine  Left drop  %c cycle",
                    current_menu_command);
                strnfmt(short_form, sizeof(short_form),
                    "Space examine  %c cycle", current_menu_command);
                SDL_strlcpy(tiny, "Space examine", sizeof(tiny));
            }
            else
            {
                strnfmt(full, sizeof(full), "Space examine  Left drop%s",
                    context);
                SDL_strlcpy(medium, "Space examine  Left drop",
                    sizeof(medium));
                SDL_strlcpy(short_form, "Space examine", sizeof(short_form));
                SDL_strlcpy(tiny, short_form, sizeof(tiny));
            }
        }
        else if (show_cycle)
        {
            strnfmt(full, sizeof(full),
                "Space %s  Right details  Left drop  %c cycle%s",
                action, current_menu_command, context);
            strnfmt(medium, sizeof(medium),
                "Space %s  Right details  Left drop  %c cycle",
                action, current_menu_command);
            strnfmt(short_form, sizeof(short_form),
                "Space %s  Details  Drop  %c cycle",
                action, current_menu_command);
            strnfmt(tiny, sizeof(tiny), "Space %s  Drop", action);
        }
        else
        {
            strnfmt(full, sizeof(full),
                "Space %s  Right details  Left drop%s", action, context);
            strnfmt(medium, sizeof(medium),
                "Space %s  Right details  Left drop", action);
            strnfmt(short_form, sizeof(short_form),
                "Space %s  Details  Drop", action);
            strnfmt(tiny, sizeof(tiny), "Space %s", action);
        }
    }

    variants[0] = full;
    variants[1] = medium;
    variants[2] = short_form;
    variants[3] = tiny;
    terminal_prompt_pick_variant(out, out_size, term_wid - 1, use_story_font,
        variants, N_ELEMENTS(variants));

    return strstr(out, context) != NULL;
}

static void append_compare_slot(int* slots, int* count, int slot)
{
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
        return;

    for (int i = 0; i < *count; i++)
    {
        if (slots[i] == slot)
            return;
    }

    if (*count < MAX_COMPARE_LINES)
        slots[(*count)++] = slot;
}

static void append_description_slot(int* slots, int* count, int capacity,
    int slot)
{
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
        return;

    for (int i = 0; i < *count; i++)
    {
        if (slots[i] == slot)
            return;
    }

    if (*count < capacity)
        slots[(*count)++] = slot;
}

/*
 * Comparison groups for item descriptions.
 *
 * Each group corresponds to an equipment slot (or class of slot) that an item
 * could occupy.  An item is comparable against another item only when their
 * groups overlap, so the description compares "things you could equip in its
 * place".  Most items belong to a single group, but a throwing weapon belongs
 * to both its melee group and the quiver/ammo group (it can be wielded or
 * thrown), which is why a dagger compares against both weapons and the quiver.
 */
#define DESC_GRP_MELEE      0x00000001u
#define DESC_GRP_BOW        0x00000002u
#define DESC_GRP_AMMO       0x00000004u /* quiver: arrows and throwing weapons */
#define DESC_GRP_HEAD       0x00000008u
#define DESC_GRP_BODY       0x00000010u
#define DESC_GRP_SHIELD     0x00000020u
#define DESC_GRP_CLOAK      0x00000040u
#define DESC_GRP_GLOVES     0x00000080u
#define DESC_GRP_BOOTS      0x00000100u
#define DESC_GRP_LIGHT      0x00000200u
#define DESC_GRP_RING       0x00000400u
#define DESC_GRP_AMULET     0x00000800u
#define DESC_GRP_STAFF      0x00001000u
#define DESC_GRP_HORN       0x00002000u
#define DESC_GRP_CONSUMABLE 0x00004000u
#define DESC_GRP_MATERIAL   0x00008000u

static u32b description_groups_for_object(const object_type* o_ptr)
{
    u32b groups = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return 0;

    /* Throwing-capable weapons can also be placed in the quiver. */
    if (player_can_treat_as_throwing(o_ptr))
        groups |= DESC_GRP_AMMO;

    if (supplies_group_matches_object(SUPPLY_GROUP_LIGHTS, o_ptr))
        groups |= DESC_GRP_LIGHT;

    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
        groups |= DESC_GRP_MELEE;
        break;

    case TV_BOW:
        groups |= DESC_GRP_BOW;
        break;

    case TV_ARROW:
        groups |= DESC_GRP_AMMO;
        break;

    case TV_HELM:
    case TV_CROWN:
        groups |= DESC_GRP_HEAD;
        break;

    case TV_SOFT_ARMOR:
    case TV_MAIL:
        groups |= DESC_GRP_BODY;
        break;

    case TV_SHIELD:
        groups |= DESC_GRP_SHIELD;
        break;

    case TV_CLOAK:
        groups |= DESC_GRP_CLOAK;
        break;

    case TV_GLOVES:
        groups |= DESC_GRP_GLOVES;
        break;

    case TV_BOOTS:
        groups |= DESC_GRP_BOOTS;
        break;

    case TV_LIGHT:
    case TV_FLASK:
        groups |= DESC_GRP_LIGHT;
        break;

    case TV_RING:
        groups |= DESC_GRP_RING;
        break;

    case TV_AMULET:
        groups |= DESC_GRP_AMULET;
        break;

    case TV_STAFF:
        groups |= DESC_GRP_STAFF;
        break;

    case TV_HORN:
        groups |= DESC_GRP_HORN;
        break;

    case TV_POTION:
    case TV_FOOD:
    case TV_EASTER:
    case TV_GEM:
        groups |= DESC_GRP_CONSUMABLE;
        break;

    case TV_METAL:
        groups |= DESC_GRP_MATERIAL;
        break;

    default:
        break;
    }

    return groups;
}

static bool description_object_matches(const object_type* base,
    u32b base_groups, const object_type* o_ptr)
{
    u32b groups;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    groups = description_groups_for_object(o_ptr);

    /* Both items belong to a known slot group: compare on slot overlap. */
    if (base_groups && groups)
        return (base_groups & groups) != 0;

    /* Unknown categories fall back to matching the same base item type. */
    return base && base->tval == o_ptr->tval;
}

static bool description_object_already_listed(const object_type* objects[],
    int count, const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    for (int i = 0; i < count; i++)
    {
        if (objects[i] == o_ptr)
            return true;
    }

    return false;
}

static bool append_description_object(const object_type* objects[],
    const char* headings[], char heading_texts[][64], int* count, int capacity,
    const object_type* o_ptr, cptr heading)
{
    if (*count >= capacity)
        return false;

    if (o_ptr && description_object_already_listed(objects, *count, o_ptr))
        return false;

    strnfmt(heading_texts[*count], sizeof(heading_texts[0]), "%s:",
        heading ? heading : "Item");
    headings[*count] = heading_texts[*count];
    objects[(*count)++] = o_ptr;
    return true;
}


static char describe_item_with_comparisons_aux(int item_index,
    bool include_comparisons, bool floor_actions)
{
    const object_type* objects[MAX_DESCRIPTION_COMPARE_ITEMS];
    const char* headings[MAX_DESCRIPTION_COMPARE_ITEMS];
    char heading_texts[MAX_DESCRIPTION_COMPARE_ITEMS][64];
    int count = 0;
    object_type* base_obj;
    bool is_floor = (item_index < 0);
    bool is_supply = (item_index >= SUPPLIES_INDEX);
    u32b base_groups;

    if (item_index == ENHANCED_MENU_NO_SELECTION)
        return 0;

    if (inventory_item_is_supply_summary(item_index))
        return 0;

    if (is_floor || is_supply)
    {
        base_obj = inventory_item_to_object_ptr(item_index);
    }
    else
    {
        if (item_index < 0 || item_index >= INVEN_TOTAL)
            return 0;
        base_obj = &inventory[item_index];
    }

    if (!base_obj || !base_obj->k_idx)
        return 0;

    /* Opening an item description attempts smithing-difficulty identification. */
    {
        bool is_equipped = (!is_floor && item_index >= INVEN_WIELD);
        (void)player_try_identify_smithing_object_on_examine(base_obj,
            is_equipped);
    }

    base_groups = description_groups_for_object(base_obj);

    append_description_object(objects, headings, heading_texts, &count,
        MAX_DESCRIPTION_COMPARE_ITEMS, base_obj,
        is_floor ? "Selected item (floor)"
                 : (is_supply ? "Selected item (supply)" : "Selected item"));

    if (include_comparisons)
    {
        int slots[INVEN_TOTAL - INVEN_WIELD];
        bool slot_allows_empty[INVEN_TOTAL - INVEN_WIELD];
        int slot_count = 0;

        append_description_slot(slots, &slot_count, N_ELEMENTS(slots),
            wield_slot(base_obj));

        if (base_obj->tval == TV_RING)
        {
            append_description_slot(slots, &slot_count, N_ELEMENTS(slots),
                INVEN_LEFT);
            append_description_slot(slots, &slot_count, N_ELEMENTS(slots),
                INVEN_RIGHT);
        }
        else if (base_obj->tval == TV_ARROW
            || player_can_treat_as_throwing(base_obj))
        {
            append_description_slot(slots, &slot_count, N_ELEMENTS(slots),
                INVEN_QUIVER1);
            append_description_slot(slots, &slot_count, N_ELEMENTS(slots),
                INVEN_QUIVER2);
        }

        for (int i = 0; i < slot_count; i++)
            slot_allows_empty[i] = true;

        for (int slot = INVEN_WIELD; slot < INVEN_TOTAL; slot++)
        {
            if (!inventory[slot].k_idx)
                continue;
            if (!description_object_matches(base_obj, base_groups,
                    &inventory[slot]))
            {
                continue;
            }

            int before = slot_count;
            append_description_slot(slots, &slot_count, N_ELEMENTS(slots),
                slot);
            if (slot_count > before)
                slot_allows_empty[slot_count - 1] = false;
        }

        for (int i = 0; i < slot_count
             && count < MAX_DESCRIPTION_COMPARE_ITEMS; i++)
        {
            int slot = slots[i];
            if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
                continue;

            object_type* equip_obj = &inventory[slot];
            if (equip_obj->k_idx)
            {
                append_description_object(objects, headings, heading_texts,
                    &count, MAX_DESCRIPTION_COMPARE_ITEMS, equip_obj,
                    mention_use(slot));
            }
            else if (slot_allows_empty[i])
            {
                append_description_object(objects, headings, heading_texts,
                    &count, MAX_DESCRIPTION_COMPARE_ITEMS, NULL,
                    mention_use(slot));
            }
        }

        for (int i = 0; i < INVEN_PACK
             && count < MAX_DESCRIPTION_COMPARE_ITEMS; i++)
        {
            char heading[32];

            if (!description_object_matches(base_obj, base_groups,
                    &inventory[i]))
                continue;

            strnfmt(heading, sizeof(heading), "Pack %c", index_to_label(i));
            append_description_object(objects, headings, heading_texts, &count,
                MAX_DESCRIPTION_COMPARE_ITEMS, &inventory[i], heading);
        }

        for (int i = 0; i < supplies_entry_count()
             && count < MAX_DESCRIPTION_COMPARE_ITEMS; i++)
        {
            object_type* supply_obj = supplies_entry_at(i);
            char heading[32];

            if (!description_object_matches(base_obj, base_groups, supply_obj))
                continue;

            strnfmt(heading, sizeof(heading), "Supply %c",
                supplies_label_for_entry(i));
            append_description_object(objects, headings, heading_texts, &count,
                MAX_DESCRIPTION_COMPARE_ITEMS, supply_obj, heading);
        }

        if (is_floor)
        {
            int floor_list[MAX_FLOOR_STACK];
            int floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py,
                p_ptr->px, 0x00);

            for (int i = 0; i < floor_num
                 && count < MAX_DESCRIPTION_COMPARE_ITEMS; i++)
            {
                int o_idx = floor_list[i];
                object_type* floor_obj;

                if (o_idx <= 0 || o_idx >= o_max)
                    continue;

                floor_obj = &o_list[o_idx];
                if (!description_object_matches(base_obj, base_groups,
                        floor_obj))
                    continue;

                append_description_object(objects, headings, heading_texts,
                    &count, MAX_DESCRIPTION_COMPARE_ITEMS, floor_obj,
                    "Floor");
            }
        }
    }

    if (floor_actions && is_floor)
    {
        static const object_info_screen_action actions[] = {
            { 'x', "x use" },
            { ' ', "Space pick up" },
            { ESCAPE, "Esc close" }
        };

        return object_info_screen_multi_with_actions(objects, headings, count,
            "x use  Space pick up  Esc close", actions, N_ELEMENTS(actions));
    }

    object_info_screen_multi(objects, headings, count);
    return 0;
}

void describe_item_with_comparisons(int item_index, bool include_comparisons)
{
    (void)describe_item_with_comparisons_aux(item_index,
        include_comparisons, false);
}

char describe_item_with_floor_actions(int item_index, bool include_comparisons)
{
    return describe_item_with_comparisons_aux(item_index,
        include_comparisons, true);
}

void show_inven_enhanced(void)
{
    int which;
    bool done = false;
    bool saved_hide_cursor = hide_cursor;
    bool saved_cursor = false;
    char out_val[160];

    bool use_story_font = story_inventory_enabled();
    story_font_term_state story_state;
    int story_term_w = 0;
    story_inventory_list_active = use_story_font;
    story_font_term_push(use_story_font, false, &story_state);
    if (use_story_font) {
        int story_term_h = 0;
        Term_get_size(&story_term_w, &story_term_h);
    }

    (void)Term_get_cursor(&saved_cursor);
    hide_cursor = true;
    (void)Term_set_cursor(false);

    /* Variables exactly matching show_inven() */
    int i, k, z;
    int col, len, lim;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int label_col_base = menu_label_col_for_width(term_wid, show_weights);
    int highlight_row = -1;
    bool highlight_active = false;
    int previous_total_rows = 0;
    int previous_compare_count = 0;
    int previous_highlight_row = -1; /* track last frame highlight for surgical clears */
    int scroll_top = 0;
    int previous_scroll_top = 0;
    int visible_rows = inventory_menu_visible_rows_for_height(term_hgt);
    bool first_render = true;
    bool drop_click_mode = false;
    
    /* Floor items variables */
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;
    bool has_floor_items = false;
    bool include_supplies = !inventory_menu_include_equip && supplies_visible_for_current_filter();
    
    object_type* o_ptr;
    char o_name[80];
    char tmp_val[80];
    
    /* Arrays exactly matching show_inven() - expanded to include floor items */
    int out_index[ENHANCED_MAX_LIST];
    byte out_color[ENHANCED_MAX_LIST];
    char out_desc[ENHANCED_MAX_LIST][80];
    bool out_is_floor[ENHANCED_MAX_LIST];  /* Track which entries are floor items */
    bool out_is_supply[ENHANCED_MAX_LIST]; /* Track which entries are supply items */
    
    /* Default length (exactly like show_inven) */
    len = 29;
    
    /* Maximum space allowed for descriptions (exactly like show_inven) */
    lim = term_wid - 3;
    if (lim < 0)
        lim = 0;
    
    /* Require space for weight if needed (exactly like show_inven) */
    if (show_weights && lim > (weight_col - 1))
        lim = weight_col - 1;
    if (lim < 0)
        lim = 0;
    
    /* Scan floor items first to see if we have any */
    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);
    has_floor_items = false;
    for (i = 0; i < floor_num; i++) {
        o_ptr = &o_list[floor_list[i]];
        if (item_tester_okay(o_ptr)) {
            has_floor_items = true;
            break;
        }
    }
    
    /* Find the "final" slot (exactly like show_inven) */
    z = 0;  /* Initialize z */
    for (i = 0; i < INVEN_PACK; i++)
    {
        o_ptr = &inventory[i];
        if (!o_ptr->k_idx) continue;
        z = i + 1;
    }
    
    /* Limit displayed items to leave room for supplies if they will be shown */
    if (include_supplies)
    {
        int max_items = INVEN_PACK - 1;  /* Reserve one slot for supplies */
        if (z > max_items)
            z = max_items;
    }
    
    /* Build combined list with floor items first, then inventory */
    k = 0;

    if (has_floor_items)
    {
        for (i = 0; i < floor_num; i++)
        {
            o_ptr = &o_list[floor_list[i]];

            if (!item_tester_okay(o_ptr))
                continue;

            object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
            o_name[lim] = '\0';

            out_index[k] = 0 - floor_list[i];
            out_is_floor[k] = true;
            out_is_supply[k] = false;
            out_color[k] = weapon_glows(o_ptr) 
                ? object_display_color(o_ptr, TERM_L_BLUE)
                : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
            SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

            int l = (int)strlen(out_desc[k]) + 5;
            if (show_weights)
                l += 9;
            if (l > len)
                len = l;

            k++;
        }
    }

    if (include_supplies && k < (int)N_ELEMENTS(out_index))
    {
        char supply_desc[80];
        format_supply_summary(supply_desc, sizeof(supply_desc));
        out_index[k] = SUPPLIES_INDEX;
        out_is_floor[k] = false;
        out_is_supply[k] = true;
        out_color[k] = TERM_L_WHITE;
        SDL_strlcpy(out_desc[k], supply_desc, sizeof(out_desc[0]));

        int l = (int)strlen(out_desc[k]) + 5;
        if (show_weights)
            l += 9;
        if (l > len)
            len = l;

        k++;
    }

    for (i = 0; i < z && k < (int)N_ELEMENTS(out_index); i++)
    {
        o_ptr = &inventory[i];

        if (!item_tester_okay(o_ptr))
            continue;

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        o_name[lim] = '\0';

        out_index[k] = i;
        out_is_floor[k] = false;
        out_is_supply[k] = false;
        out_color[k] = weapon_glows(o_ptr) 
            ? object_display_color(o_ptr, TERM_L_BLUE)
            : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
        SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        int l = (int)strlen(out_desc[k]) + 5;
        if (show_weights)
            l += 9;
        if (l > len)
            len = l;

        k++;
    }

    /* Find the column to start in (exactly like show_inven) */
    col = menu_center_col_for_len(term_wid, len);
    
    log_debug("show_inven_enhanced: k=%d items, len=%d, col=%d, story_term_w=%d", k, len, col, story_term_w);
    
    /* Enable highlight if we have items */
    if (k > 0) {
        highlight_row = 0;
        highlight_active = true;
    }
    
    /* Main interaction loop */
    while (!done)
    {
        (void)Term_set_extra_cursor(false, 0, 0, false);
        term_hgt = menu_term_height();
        visible_rows = inventory_menu_visible_rows_for_height(term_hgt);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_begin_cols(col, term_wid - 1, 1, visible_rows,
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_set_keys('8', '2', '6', '4');
        if (sdl_touch_only_device_active())
        {
            ui_scroll_area_set_offset_target(&scroll_top,
                MAX(0, k - visible_rows));
        }
        const bool controller_controls = steamdeck_controls_active();

        bool prompt_context_visible = enhanced_menu_format_prompt(out_val,
            sizeof(out_val), term_wid, use_story_font, false);
        prt(out_val, 0, 0);
        ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_DROP, 0, 0,
            out_val, "drop");
        if (prompt_context_visible)
            ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_SWITCH, 0, 0,
                out_val, "Inventory");
        else
            ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_SWITCH, 0, 0,
                out_val, "cycle");
        if (drop_click_mode)
            enhanced_menu_highlight_prompt_token(out_val, "drop", TERM_YELLOW);

        bool allow_compare = (current_menu_command == 'u' || current_menu_command == 'x');
        
        log_debug("show_inven_enhanced: current_menu_command='%c' (%d), allow_compare=%d", 
            current_menu_command, (int)current_menu_command, allow_compare);

        int compare_count = 0;
        char compare_label[MAX_COMPARE_LINES][4];
        char compare_prefix[MAX_COMPARE_LINES][20];
        char compare_desc[MAX_COMPARE_LINES][80];
        byte compare_attr[MAX_COMPARE_LINES];
        bool compare_has_weight[MAX_COMPARE_LINES];
        int compare_weight[MAX_COMPARE_LINES];
        object_type* compare_obj[MAX_COMPARE_LINES]; /* Track object pointers for tile display */

        for (int c = 0; c < MAX_COMPARE_LINES; c++)
        {
            compare_label[c][0] = '\0';
            compare_prefix[c][0] = '\0';
            compare_desc[c][0] = '\0';
            compare_attr[c] = TERM_SLATE;
            compare_has_weight[c] = false;
            compare_weight[c] = 0;
            compare_obj[c] = NULL;
        }

        /* Surgical pre-clear: only rows that changed (old highlight + its compare lines, new highlight region, trailing cleared rows).
           This avoids full-frame flicker and keeps ordering erase -> print -> redraw. */
        int redraw_y1 = -1, redraw_y2 = -1; /* aggregate redraw bounds */
        if (use_story_font && allow_compare && !first_render)
        {
            int label_col_tmp = label_col_base;
            const int label_width_tmp = 4;
            int max_print_col = label_col_tmp + label_width_tmp;
            int erase_w = (max_print_col > col) ? (max_print_col - col + 1) : 0;
            if (story_term_w > 80) erase_w = (erase_w * story_term_w) / 80;

            /* Clear previous highlight block */
            if (previous_highlight_row >= 0 && previous_highlight_row < k)
            {
                int base = 1 + previous_highlight_row - previous_scroll_top;
                for (int r = 0; base >= 1 && r <= previous_compare_count; ++r)
                {
                    int rr = base + r;
                    if (rr > visible_rows)
                        break;
                    if (erase_w > 0) Term_erase(col, rr, erase_w);
                    if (show_weights) Term_erase(weight_col, rr, 9);
                    if (redraw_y1 < 0 || rr < redraw_y1) redraw_y1 = rr;
                    if (redraw_y2 < 0 || rr > redraw_y2) redraw_y2 = rr;
                }
            }
            /* Clear current highlight prospective block */
            if (highlight_active && highlight_row >= 0 && highlight_row < k)
            {
                int base = 1 + highlight_row - scroll_top;
                for (int r = 0; base >= 1 && r <= MAX_COMPARE_LINES; ++r)
                {
                    int rr = base + r;
                    if (rr > visible_rows)
                        break;
                    if (erase_w > 0) Term_erase(col, rr, erase_w);
                    if (show_weights) Term_erase(weight_col, rr, 9);
                    if (redraw_y1 < 0 || rr < redraw_y1) redraw_y1 = rr;
                    if (redraw_y2 < 0 || rr > redraw_y2) redraw_y2 = rr;
                }
            }
        }

        if (allow_compare && highlight_active && highlight_row >= 0 && highlight_row < k)
        {
            bool highlighted_is_supply = out_is_supply[highlight_row];
            object_type* highlighted_obj = NULL;

            log_debug("COMPARE SETUP CHECK: highlight_row=%d, is_supply=%d, is_floor=%d", 
                highlight_row, highlighted_is_supply, out_is_floor[highlight_row]);

            if (!highlighted_is_supply)
            {
                highlighted_obj = out_is_floor[highlight_row]
                    ? &o_list[0 - out_index[highlight_row]]
                    : &inventory[out_index[highlight_row]];
                    
                log_debug("COMPARE SETUP: highlighted_obj=%p, k_idx=%d", 
                    highlighted_obj, highlighted_obj ? highlighted_obj->k_idx : 0);
            }

            if (highlighted_obj)
            {
                int slot_candidates[MAX_COMPARE_LINES];
                int slot_count = 0;

                int primary_slot = wield_slot(highlighted_obj);
                append_compare_slot(slot_candidates, &slot_count, primary_slot);

                if (highlighted_obj->tval == TV_RING)
                {
                    append_compare_slot(slot_candidates, &slot_count, INVEN_LEFT);
                    append_compare_slot(slot_candidates, &slot_count, INVEN_RIGHT);
                }
                else if (highlighted_obj->tval == TV_ARROW)
                {
                    append_compare_slot(slot_candidates, &slot_count, INVEN_QUIVER1);
                    append_compare_slot(slot_candidates, &slot_count, INVEN_QUIVER2);
                }

                for (int idx = 0; idx < slot_count; idx++)
                {
                    int slot = slot_candidates[idx];

                    strnfmt(compare_label[idx], sizeof(compare_label[idx]), "%c", index_to_label(slot));
                    strnfmt(compare_prefix[idx], sizeof(compare_prefix[idx]), "%-12s: ", mention_use(slot));

                    /* Calculate limit based on actual column positions
                     * Text starts at col + 12 + 2 (after prefix "%-12s: ")
                     * Plus 2-3 more if tile is drawn (assume worst case: +3)
                     * Text ends at column 70 (weight) or 71 (no weight)
                     * Use conservative limit to prevent overflow
                     * For story font, reduce by 20% to account for proportional spacing */
                    int text_start_col = col + 12 + 2 + 3;  /* +3 for tile worst case */
                    int text_end_col = show_weights ? weight_col : label_col_base;
                    int compare_lim = text_end_col - text_start_col;
                    /* We'll enforce the exact visual width at render time; 
                       keep character truncation only as a hard ceiling */
                    
                    if (compare_lim < 0)
                        compare_lim = 0;
                    if (compare_lim >= (int)sizeof(compare_desc[idx]))
                        compare_lim = (int)sizeof(compare_desc[idx]) - 1;

                    object_type* equipped_obj = &inventory[slot];
                    if (equipped_obj->k_idx)
                    {
                        compare_obj[idx] = equipped_obj; /* Store for tile display */
                        object_desc(compare_desc[idx], sizeof(compare_desc[idx]), equipped_obj, true, 3);
                        compare_desc[idx][compare_lim] = '\0';
                        log_debug("COMPARE SETUP slot=%d: orig_len=%d, compare_lim=%d, truncated='%s'",
                            slot, (int)strlen(compare_desc[idx]), compare_lim, compare_desc[idx]);
                        compare_attr[idx] = weapon_glows(equipped_obj) 
                            ? object_display_color(equipped_obj, TERM_L_BLUE)
                            : object_display_color(equipped_obj, tval_to_attr[equipped_obj->tval % N_ELEMENTS(tval_to_attr)]);
                        if (show_weights && equipped_obj->weight)
                        {
                            compare_has_weight[idx] = true;
                            compare_weight[idx] = equipped_obj->weight * equipped_obj->number;
                        }
                    }
                    else
                    {
                        compare_obj[idx] = NULL; /* No object for empty slots */
                        cptr empty_text = describe_empty_slot(slot);
                        SDL_strlcpy(compare_desc[idx], empty_text, sizeof(compare_desc[idx]));
                        if (compare_lim < (int)sizeof(compare_desc[idx]))
                            compare_desc[idx][compare_lim] = '\0';
                        compare_attr[idx] = TERM_SLATE;
                    }
                }

                compare_count = slot_count;
                log_debug("COMPARE SETUP COMPLETE: compare_count=%d", compare_count);
            }
            else
            {
                log_debug("COMPARE SETUP SKIPPED: highlighted_obj is NULL");
            }
        }

        if (sdl_touch_only_device_active())
        {
            int max_scroll_top = MAX(0, k - visible_rows);

            (void)ui_scroll_area_take_touch_scrolled();
            if (scroll_top > max_scroll_top)
                scroll_top = max_scroll_top;
            if (scroll_top < 0)
                scroll_top = 0;
        }
        else
        {
            scroll_top = inventory_menu_scroll_to_selection(scroll_top,
                highlight_row, k, visible_rows,
                allow_compare ? compare_count : 0);
        }

        int clear_rows = 0;
        for (int row_idx = scroll_top; row_idx < k && clear_rows < visible_rows;
             row_idx++)
        {
            clear_rows++;
            if (allow_compare && compare_count > 0 && row_idx == highlight_row)
            {
                int compare_rows = compare_count;
                if (clear_rows + compare_rows > visible_rows)
                    compare_rows = visible_rows - clear_rows;
                if (compare_rows > 0)
                    clear_rows += compare_rows;
            }
        }
        if (previous_total_rows > clear_rows)
            clear_rows = previous_total_rows;
        if (clear_rows > visible_rows)
            clear_rows = visible_rows;

        if (clear_rows > 0)
        {
            for (int clear_row = 1; clear_row <= clear_rows; clear_row++)
            {
                if (use_story_font)
                {
                    int erase_w = 255;
                    int weight_erase_w = 9;
                    if (story_term_w > 80)
                    {
                        erase_w = (erase_w * story_term_w) / 80;
                        weight_erase_w = (weight_erase_w * story_term_w) / 80;
                    }
                    Term_erase(col, clear_row, erase_w);
                    if (show_weights)
                        Term_erase(weight_col, clear_row, weight_erase_w);
                }
                else
                {
                    prt("", clear_row, col);
                    if (show_weights)
                        prt("", clear_row, weight_col);
                }
            }
        }

        /* Render combined list with floor entries first */
        int next_row = 1;
        for (int j = scroll_top; j < k && next_row <= visible_rows; j++)
        {
            bool is_floor_item = out_is_floor[j];
            bool is_supply_item = out_is_supply[j];
            object_type* line_obj = NULL;
            object_type supply_icon;
            object_type* display_obj = NULL;
            bool is_highlight = highlight_active && (highlight_row == j);
            byte selected_attr = inventory_menu_selected_attr(out_color[j]);
            byte line_attr = is_highlight ? selected_attr : out_color[j];
            int row = next_row;

            if (is_floor_item)
                line_obj = &o_list[0 - out_index[j]];
            else if (!is_supply_item)
                line_obj = &inventory[out_index[j]];
            display_obj = is_supply_item
                ? prepare_supply_icon_object(&supply_icon)
                : line_obj;

            int label_col = label_col_base;
            const int label_width = 4;

            ui_menu_click_add(j, col, row, term_wid - col);

            if (use_story_font)
            {
                /* Pre-clear handles row erasing, just do highlighting */
                if (!allow_compare)
                {
                    /* Only erase if comparison is disabled (no pre-clear) */
                    int erase_w = 255;
                    if (story_term_w > 80) erase_w = (erase_w * story_term_w) / 80;
                    Term_erase(col, row, erase_w);
                }
            }
            else
            {
                prt("", row, col);
            }

            /* Draw tile if in graphics mode. */
            int text_col = col;
            int selection_end = label_col + label_width;
            if (is_highlight)
            {
                if (use_story_font)
                    story_fill_rect(row, col, selection_end - col,
                        selected_attr);
                else
                    inventory_menu_fill_selected_span(col, selection_end,
                        row, selected_attr);
            }
            if (display_obj && display_obj->k_idx)
            {
                text_col = draw_item_tile_with_background(col, row,
                    display_obj, is_highlight ? selected_attr : 0);
            }
            log_trace("ITEM ROW %d: col=%d, text_col=%d, is_highlight=%d, desc='%.30s'",
                row, col, text_col, is_highlight, out_desc[j]);

            if (use_story_font)
            {
                int desc_limit = menu_desc_limit(text_col, label_col,
                    weight_col, show_weights);
                
                /* Convert limit from mono columns to story font cells */
                if (story_term_w > 80) {
                    desc_limit = (desc_limit * story_term_w) / 80;
                }
                
                log_trace("ITEM ROW %d STORY: desc_limit=%d (scaled), text_len=%d",
                    row, desc_limit, (int)strlen(out_desc[j]));
                story_print_text(row, text_col, desc_limit, line_attr, out_desc[j]);
            }
            else
            {
                c_put_str(line_attr, out_desc[j], row, text_col);
            }

            if (show_weights)
            {
                int wgt = 0;
                if (is_supply_item)
                    wgt = supplies_limit_weight();
                else if (line_obj)
                    wgt = line_obj->weight * line_obj->number;
                strnfmt(tmp_val, sizeof(tmp_val), "%2d.%1d lb", wgt / 10, wgt % 10);
                if (use_story_font)
                {
                    int weight_width = label_col - weight_col;
                    if (weight_width < 1)
                        weight_width = 1;
                    story_print_text_grid(row, weight_col, weight_width, line_attr, tmp_val);
                }
                else
                    c_put_str(line_attr, tmp_val, row, weight_col);
            }

            /* Print the item letter at the end */
            if (is_floor_item)
                strnfmt(tmp_val, sizeof(tmp_val), " (-)");
            else if (is_supply_item)
            {
                char label = supplies_label_char();
                int slot = supplies_virtual_slot();
                if (!label && slot >= 0)
                    label = index_to_label(slot);
                if (!label)
                    label = 'a';
                strnfmt(tmp_val, sizeof(tmp_val), "(%c)", label);
            }
            else
                strnfmt(tmp_val, sizeof(tmp_val), "(%c)", index_to_label(out_index[j]));

            byte label_attr = is_highlight ? selected_attr : TERM_WHITE;
            log_trace("ITEM RENDER row=%d: label_col=%d, show_weights=%d, label='%s'", 
                row, label_col, show_weights, tmp_val);
            if (use_story_font)
            {
                story_print_text_grid(row, label_col, label_width,
                    label_attr, tmp_val);
            }
            else
            {
                if (is_highlight)
                    c_put_str(label_attr, tmp_val, row, label_col);
                else
                    put_str(tmp_val, row, label_col);
            }

            next_row++;
            
            if (j == 3) {
                log_debug("AT ITEM j=3: compare_count=%d, highlight_row=%d, j==highlight_row=%d", 
                    compare_count, highlight_row, (j == highlight_row));
            }
            
            log_trace("CHECKING COMPARE RENDER: j=%d, compare_count=%d, highlight_row=%d, condition=%d",
                j, compare_count, highlight_row, (compare_count > 0 && j == highlight_row));

            if (compare_count > 0 && j == highlight_row)
            {
                log_trace(">>> INSIDE IF BLOCK: About to render %d comparison lines at next_row=%d", 
                    compare_count, next_row);
                    
                for (int idx = 0; idx < compare_count && next_row <= visible_rows; idx++)
                {
                    int compare_row = next_row;
                    
                    log_trace("COMPARE LINE idx=%d will render at row=%d", idx, compare_row);

                    /* Pre-clear handles row erasing for comparison mode */
                    if (use_story_font && !allow_compare) {
                        int erase_w = 255;
                        if (story_term_w > 80) erase_w = (erase_w * story_term_w) / 80;
                        Term_erase(col, compare_row, erase_w);
                    }
                    else if (!use_story_font)
                        prt("", compare_row, col);

                    if (use_story_font)
                        story_print_equipment_prefix(compare_row, col, TERM_WHITE, compare_prefix[idx]);
                    else
                        c_put_str(TERM_WHITE, compare_prefix[idx], compare_row, col);

                    /* Draw tile if in graphics mode for equipped items */
                    int compare_text_col = col + 12 + 2;
                    if (compare_obj[idx] && compare_obj[idx]->k_idx)
                    {
                        compare_text_col = draw_item_tile(col + 12 + 2, compare_row, compare_obj[idx]);
                    }
                    
                    log_debug("COMPARE RENDER idx=%d row=%d: col=%d, text_col=%d, desc='%s'",
                        idx, compare_row, col, compare_text_col, compare_desc[idx]);

                    if (use_story_font)
                    {
                        /* Bound the story-font rendering to the available grid width,
                           scaled to story cells, so we can safely show more text. */
                        int text_end_col = show_weights ? weight_col : label_col;
                        int desc_limit = text_end_col - compare_text_col;
                        if (desc_limit < 1) desc_limit = 1;
                        if (story_term_w > 80) desc_limit = (desc_limit * story_term_w) / 80;
                        log_debug("COMPARE RENDER STORY: text_col=%d, desc_limit=%d, text_len=%d",
                            compare_text_col, desc_limit, (int)strlen(compare_desc[idx]));
                        story_print_text(compare_row, compare_text_col, desc_limit, compare_attr[idx], compare_desc[idx]);
                    }
                    else
                    {
                        c_put_str(compare_attr[idx], compare_desc[idx], compare_row, compare_text_col);
                    }

                    if (show_weights)
                    {
                        if (compare_has_weight[idx])
                        {
                            strnfmt(tmp_val, sizeof(tmp_val), "%2d.%1d lb", compare_weight[idx] / 10, compare_weight[idx] % 10);
                            if (use_story_font)
                            {
                                int compare_weight_width = label_col - weight_col;
                                if (compare_weight_width < 1)
                                    compare_weight_width = 1;
                                story_print_text_grid(compare_row, weight_col, compare_weight_width, compare_attr[idx], tmp_val);
                            }
                            else
                                c_put_str(compare_attr[idx], tmp_val, compare_row, weight_col);
                        }
                        else
                        {
                            if (use_story_font)
                                Term_erase(weight_col, compare_row, 9);
                            else
                                prt("", compare_row, weight_col);
                        }
                    }

                    /* Print the item letter at the end of compare line */
                    if (compare_label[idx][0])
                    {
                        char label_str[8];
                        strnfmt(label_str, sizeof(label_str), "(%s)", compare_label[idx]);
                        if (use_story_font)
                            story_print_text_grid(compare_row, label_col,
                                label_width, compare_attr[idx], label_str);
                        else
                            c_put_str(compare_attr[idx], label_str, compare_row, label_col);
                    }

                    next_row++;
                }
            }
        }

        int total_rows = next_row - 1;

        /* End-of-frame redraw of just changed rows */
        if (use_story_font && allow_compare)
        {
            if (compare_count > 0 && highlight_row >= 0 && highlight_row < k)
            {
                int base = 1 + highlight_row - scroll_top;
                /* Redraw all items from highlighted row to end, since comparison lines shift everything down */
                int last = total_rows;
                if (base < 1)
                    base = 1;
                if (last > visible_rows)
                    last = visible_rows;
                if (redraw_y1 < 0 || base < redraw_y1) redraw_y1 = base;
                if (redraw_y2 < 0 || last > redraw_y2) redraw_y2 = last;
            }
            /* Include trailing cleared rows if list shrank */
            if (total_rows < previous_total_rows)
            {
                int shrink_start = total_rows + 1;
                int shrink_end = previous_total_rows;
                if (redraw_y1 < 0 || shrink_start < redraw_y1) redraw_y1 = shrink_start;
                if (redraw_y2 < 0 || shrink_end > redraw_y2) redraw_y2 = shrink_end;
            }
            if (redraw_y1 > 0 && redraw_y2 >= redraw_y1)
            {
                int max_col = label_col_base + 4;
                if (max_col > Term->wid - 1) max_col = Term->wid - 1;
                Term_redraw_section(col, redraw_y1, max_col, redraw_y2);
            }
        }

        if (total_rows < previous_total_rows)
        {
            int clear_col = col;
            for (int clear_row = total_rows + 1;
                 clear_row <= previous_total_rows && clear_row <= visible_rows;
                 clear_row++)
            {
                if (use_story_font)
                {
                    int erase_w = 255;
                    int weight_erase_w = 9;
                    if (story_term_w > 80) {
                        erase_w = (erase_w * story_term_w) / 80;
                        weight_erase_w = (weight_erase_w * story_term_w) / 80;
                    }
                    Term_erase(col, clear_row, erase_w);
                    if (show_weights)
                        Term_erase(weight_col, clear_row, weight_erase_w);
                }
                else
                {
                    prt("", clear_row, clear_col);
                    if (show_weights)
                        prt("", clear_row, weight_col);
                }
            }
        }

    previous_total_rows = total_rows;
    previous_compare_count = compare_count;
    previous_highlight_row = highlight_row;
    previous_scroll_top = scroll_top;
    first_render = false;  /* subsequent frames use surgical region */

        if (highlight_active && highlight_row >= 0 && highlight_row < k)
        {
            const char* row_type = out_is_floor[highlight_row] ? "floor"
                : (out_is_supply[highlight_row] ? "supply" : "inventory");
            log_debug("show_inven_enhanced: Highlighted row %d (%s)", highlight_row, row_type);
        }
        
        /* Get a key */
        which = inkey();

        {
            int clicked_row = -1;
            int click_action = UI_MENU_CLICK_PRIMARY;

            bool click_taken =
                ui_menu_click_take_action(&clicked_row, &click_action);

            if (click_taken && clicked_row == ENHANCED_MENU_CLICK_SWITCH)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                done = true;
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                continue;
            }

            if (click_taken && clicked_row == ENHANCED_MENU_CLICK_DROP)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                drop_click_mode = !drop_click_mode;
                continue;
            }

            if (click_taken && clicked_row >= 0 && clicked_row < k)
            {
                highlight_row = clicked_row;
                highlight_active = true;
                enhanced_inventory_selected_item = out_index[highlight_row];

                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                if (click_action == UI_MENU_CLICK_SECONDARY)
                {
                    done = true;
                    enhanced_menu_action = out_is_supply[highlight_row]
                        ? ENHANCED_ACTION_SUPPLIES
                        : ENHANCED_ACTION_EXAMINE;
                }
                else if (drop_click_mode)
                {
                    if (out_is_floor[highlight_row])
                    {
                        bell("Cannot drop floor items!");
                        continue;
                    }
                    if (!death_spectator_allow_menu_action())
                        continue;

                    done = true;
                    enhanced_menu_action = ENHANCED_ACTION_DROP;
                }
                else
                {
                    int primary_action = enhanced_menu_primary_select_action();

                    if (primary_action == ENHANCED_ACTION_USE
                        && !death_spectator_allow_menu_action())
                        continue;

                    done = true;
                    enhanced_menu_action = primary_action;
                }

                continue;
            }
            if (click_taken && click_action == UI_MENU_CLICK_HOVER)
                continue;
            if (!click_taken && which == UI_MENU_CLICK_WAKE_KEY)
                continue;
        }
        which = steamdeck_menu_key(which, 'e', 'i');
        
        log_trace("show_inven_enhanced: Key pressed: %d ('%c')", which, (which >= 32 && which <= 126) ? which : '?');
        
        /* Parse it */
        switch (which)
        {
        case ESCAPE:
            log_trace("show_inven_enhanced: ESC pressed, setting action to 0 and exiting");
            enhanced_menu_action = ENHANCED_ACTION_NONE;  /* Explicitly set to exit */
            done = true;
            break;
            
        case 'i':
            if ((current_menu_command == 0) && inventory_menu_same_button_cycle_enabled()) {
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_inven_enhanced: Direct access I key - same-button cycle to equipment (action=1)");
                done = true;
                break;
            }

            /* Already in inventory */
            break;
            
        case 'e':
            /* Handle E key based on access mode and portable UI support */
            {
                extern char current_menu_command;
                if (current_menu_command != 0) {
                    /* Command access (u/x pressed) */
                    if (controller_controls) {
                        /* Controller UI: E/I switch menus */
                        enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                        done = true;
                    } else {
                        /* Keyboard-only: E/I are just letters, not menu switching */
                        /* Fall through to default letter handling */
                        goto default_case;
                    }
                } else {
                    /* Direct access (i/e pressed) */
                    if (controller_controls) {
                        /* Controller UI: E/I switch menus */
                        enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                        log_trace("show_inven_enhanced: Direct access E key - switching to equipment (action=1)");
                        done = true;
                    } else {
                        /* Keyboard-only: E/I are just letters */
                        goto default_case;
                    }
                }
            }
            break;
            
        /* Handle cycling when the original command is pressed */
        case 'u':
            if (current_menu_command == which) {
                /* Same command - cycle to equipment */
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_inven_enhanced: Command cycling (%c) - switching to equipment (action=1)", which);
                done = true;
            }
            /* Different command does nothing */
            break;

        case 'x':
            if (current_menu_command == which) {
                /* Same command - cycle to equipment */
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_inven_enhanced: Command cycling (%c) - switching to equipment (action=1)", which);
                done = true;
            } else if (controller_controls) {
                if (highlight_active && highlight_row >= 0 && highlight_row < k) {
                    enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                    done = true;
                }
            }
            break;
            
        /* Toggle keys - switch to equipment */
        case '/':           /* / - toggle to equipment */
        case KTRL('I'):     /* Ctrl+I - toggle to equipment */
        case KTRL('E'):     /* Ctrl+E - toggle to equipment */
            enhanced_menu_action = ENHANCED_ACTION_SWITCH;
            done = true;
            break;
            
        case '8':
            if (highlight_active && k > 0) {
                highlight_row = (highlight_row + k - 1) % k;
            }
            break;
            
        case '2':
            if (highlight_active && k > 0) {
                highlight_row = (highlight_row + 1) % k;
            }
            break;
            
        case ' ':        /* Space - use item or examine based on context */
        case '\r':       /* Enter - use item or examine based on context */
        case '\n':       /* Enter (alternative) - use item or examine based on context */
            if (highlight_active && highlight_row >= 0 && highlight_row < k) {
                extern char current_menu_command;
                if (current_menu_command == 'x') {
                    /* Examine mode - mark for examination */
                    done = true;
                    enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                } else {
                    if (!death_spectator_allow_menu_action())
                        break;

                    /* Use mode - defer use until after menu restore */
                    done = true;
                    enhanced_menu_action = ENHANCED_ACTION_USE;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                }
            }
            break;

        case '6':        /* Arrow right - description */
            if (highlight_active && highlight_row >= 0 && highlight_row < k) {
                /* Mark that we want to examine an item */
                enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                enhanced_inventory_selected_item = out_index[highlight_row];
                done = true;
            }
            break;
            
        case '4':        /* Arrow left - drop item */
            if (highlight_active && highlight_row >= 0 && highlight_row < k) {
                if (!out_is_floor[highlight_row]) {
                    if (!death_spectator_allow_menu_action())
                        break;

                    /* Can only drop inventory items, not floor items */
                    done = true;
                    enhanced_menu_action = ENHANCED_ACTION_DROP;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                } else {
                    bell("Cannot drop floor items!");
                }
            }
            break;
            
            default:
            default_case:
            /* Handle item selection by letter or dash */
            if ((which >= 'a' && which <= 'z') || (which >= 'A' && which <= 'Z') || which == '-') {
                extern char current_menu_command;
                bool allow_letters = false;
                
                /* Letter selection is hidden/disabled only in controller UI mode. */
                allow_letters = !controller_controls;
                if (!allow_letters) {
                    bell("Use directions and Space to select items in this mode");
                    break;
                }
                if (death_spectator_active()) {
                    death_spectator_allow_menu_action();
                    break;
                }
                bool item_found = false;
                
                /* Check for dash (-) which selects floor item */
                if (which == '-' && has_floor_items) {
                    /* Find the floor item in our display list */
                    for (i = 0; i < k; i++) {
                        if (out_is_floor[i]) {
                            done = true;
                            item_found = true;
                            
                            /* Handle based on menu context */
                            extern char current_menu_command;
                            if (current_menu_command != 0) {
                                /* Command mode - defer action */
                                if (current_menu_command == 'u') {
                                    if (!death_spectator_allow_menu_action()) {
                                        item_found = true;
                                        done = false;
                                        break;
                                    }
                                    enhanced_menu_action = ENHANCED_ACTION_USE;
                                    enhanced_inventory_selected_item = out_index[i];
                                } else if (current_menu_command == 'x') {
                                    /* For observe mode, mark for examination */
                                    enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                                    enhanced_inventory_selected_item = out_index[i];
                                }
                            } else {
                                /* Direct access mode - use the floor item directly */
                                if (!death_spectator_allow_menu_action()) {
                                    item_found = true;
                                    done = false;
                                } else {
                                    enhanced_menu_action = ENHANCED_ACTION_USE;
                                    enhanced_inventory_selected_item = out_index[i];
                                }
                            }
                            break;
                        }
                    }
                }
                
                /* If no floor item found, check inventory items by letter */
                if (!item_found && which != '-') {
                    int item = label_to_inven(which);
                    if (item >= 0 && (inventory[item].k_idx || (throw_slot_menu_active && throw_slot_enabled[item]))) {
                        /* Check if this inventory item is in our display list */
                        for (i = 0; i < k; i++) {
                            if (!out_is_floor[i] && out_index[i] == item) {
                                done = true;
                                item_found = true;
                                
                                /* Handle based on menu context */
                                extern char current_menu_command;
                                if (current_menu_command != 0) {
                                    /* Command mode - defer action */
                                    if (current_menu_command == 'u') {
                                        if (!death_spectator_allow_menu_action()) {
                                            item_found = true;
                                            done = false;
                                            break;
                                        }
                                        enhanced_menu_action = ENHANCED_ACTION_USE;
                                        enhanced_inventory_selected_item = item;
                                    } else if (current_menu_command == 'x') {
                                        /* For observe mode, mark for examination */
                                        enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                                        enhanced_inventory_selected_item = item;
                                    }
                                } else {
                                    /* Direct access mode - use the traditional method */
                                    if (!death_spectator_allow_menu_action()) {
                                        item_found = true;
                                        done = false;
                                    } else {
                                        p_ptr->command_new = which;
                                        p_ptr->command_see = true;
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                
                if (!item_found) {
                    bell("Illegal object choice!");
                }
            }
            else {
                bell("Invalid command!");
            }
            break;
        }
    }
    
    (void)Term_set_extra_cursor(false, 0, 0, false);
    ui_menu_click_clear();
    ui_scroll_area_clear();
    hide_cursor = saved_hide_cursor;
    (void)Term_set_cursor(saved_cursor);
    story_font_term_pop(&story_state);
    story_inventory_list_active = false;
    log_trace("show_inven_enhanced: Exiting, action=%d", enhanced_menu_action);
}

/* Global variables for equipment menu switching */
int enhanced_equip_action = ENHANCED_ACTION_NONE;
int enhanced_equipment_selected_item = ENHANCED_MENU_NO_SELECTION;

/*
 * Enhanced equipment display with scrolling and navigation
 * EXACTLY replicates show_equip() algorithm then adds highlighting
 * Only navigates through actually equipped items
 */
void show_equip_enhanced(void)
{
    int which;
    bool done = false;
    bool saved_hide_cursor = hide_cursor;
    bool saved_cursor = false;
    char out_val[160];
    bool drop_click_mode = false;
    
    log_debug("show_equip_enhanced: Starting equipment enhanced menu");
    log_debug("show_equip_enhanced: INVEN_BODY=%d, INVEN_FEET=%d, show_weights=%d", 
        INVEN_BODY, INVEN_FEET, show_weights);
    
    bool use_story_font = story_equipment_enabled();
    story_font_term_state story_state;
    int story_term_w = 0;
    story_equipment_list_active = use_story_font;
    story_font_term_push(use_story_font, false, &story_state);
    if (use_story_font) {
        int story_term_h = 0;
        Term_get_size(&story_term_w, &story_term_h);
    }

    (void)Term_get_cursor(&saved_cursor);
    hide_cursor = true;
    (void)Term_set_cursor(false);

    /* Variables exactly matching show_equip() */
    int i, k, l;
    int col, len, lim;
    int clear_col;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int label_col_base = menu_label_col_for_width(term_wid, show_weights);
    int highlight_index = -1;  /* Index in the equipped items array */
    bool highlight_active = false;
    
    object_type* o_ptr;
    char tmp_val[80];
    char o_name[80];
    
    /* Arrays exactly matching show_equip() */
    int out_index[24];        /* Slot numbers of equipped items */
    byte out_color[24];
    char out_desc[24][80];
    int armour_weight = 0;    /* Total armour weight */
    
    /* Default length (exactly like show_equip) */
    len = 29;
    
    /* Maximum space allowed for descriptions (exactly like show_equip) */
    lim = term_wid - 3;
    if (lim < 0)
        lim = 0;
    
    /* Require space for labels (exactly like show_equip) */
    lim -= (14 + 2);
    
    /* Require space for weight if needed (exactly like show_equip) */
    if (show_weights) lim -= 9;

    if (lim < 0)
        lim = 0;
    
    /* Scan the equipment list for display. Empty slots are shown when allowed. */
    log_debug("show_equip_enhanced: Starting equipment scan, show_weights=%d", show_weights);
    for (k = 0, i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];
        bool is_empty = !o_ptr->k_idx;
        
        if (!item_tester_okay(o_ptr)) continue;
        
        if (is_empty)
        {
            SDL_strlcpy(o_name, describe_empty_slot(i), sizeof(o_name));
            out_color[k] = TERM_L_DARK;
        }
        else
        {
            /* Description (exactly like show_equip) */
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
            out_color[k] = object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
        }
        
        /* Truncate the description (exactly like show_equip) */
        o_name[lim] = 0;
        
        /* Save the index (exactly like show_equip) */
        out_index[k] = i;
        
        /* Save the description (exactly like show_equip) */
        SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));
        
        /* Calculate armour weight (for body armour, cloak, shield, helmet, gloves, boots) */
        if (show_weights && o_ptr->weight && (i >= INVEN_BODY) && (i <= INVEN_FEET))
        {
            int item_weight = o_ptr->weight * o_ptr->number;
            armour_weight += item_weight;
            log_debug("show_equip_enhanced: Slot %d (%s) weight=%d, total armour_weight now=%d", 
                i, describe_empty_slot(i), item_weight, armour_weight);
        }
        else if (i >= INVEN_BODY && i <= INVEN_FEET)
        {
            log_trace("show_equip_enhanced: Slot %d (%s) skipped: show_weights=%d, o_ptr->weight=%d, is_empty=%d",
                i, describe_empty_slot(i), show_weights, o_ptr->weight, is_empty);
        }
        
        /* Extract the maximal length (exactly like show_equip) */
        l = strlen(out_desc[k]) + (2 + 3);
        
        /* Increase length for labels (exactly like show_equip) */
        l += (12 + 2);
        
        /* Increase length for weight if needed (exactly like show_equip) */
        if (show_weights) l += 9;
        
        /* Maintain the max-length (exactly like show_equip) */
        if (l > len) len = l;
        
        /* Advance the entry (exactly like show_equip) */
        k++;
    }
    
    log_debug("show_equip_enhanced: Equipment scan complete, k=%d items, total armour_weight=%d", k, armour_weight);
    
    /* Find the column to start in (exactly like show_equip) */
    col = menu_center_col_for_len(term_wid, len);
    clear_col = menu_overlay_clear_col(col);
    
    log_debug("show_equip_enhanced: k=%d equipped items, len=%d, col=%d", k, len, col);
    
    /* Enable highlight if there are occupied rows to navigate. */
    highlight_index = equipment_first_occupied_row(k, out_index);
    if (highlight_index >= 0) {
        highlight_active = true;
    }
    
    /* Main interaction loop */
    while (!done)
    {
        (void)Term_set_extra_cursor(false, 0, 0, false);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        {
            int visible_rows = inventory_menu_visible_rows_for_height(
                menu_term_height());
            int scroll_rows = MIN(k, visible_rows);

            if (scroll_rows > 0)
            {
                ui_scroll_area_begin(1, scroll_rows,
                    SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
                ui_scroll_area_set_keys('8', '2', '6', '4');
            }
            else
            {
                ui_scroll_area_clear();
            }
        }

        /* Display equipment list */
        if (use_story_font)
        {
            log_trace("show_equip_enhanced: Calling draw_equipment_story_rows with highlight_index=%d", highlight_index);
            draw_equipment_story_rows(col, k, out_index, out_color, out_desc,
                highlight_active, highlight_index, show_weights, story_term_w);
            log_trace("show_equip_enhanced: draw_equipment_story_rows FINISHED");
            
            /* Display armour weight total if any armour equipped */
            log_debug("show_equip_enhanced: Checking armour weight display: armour_weight=%d", armour_weight);
            if (armour_weight)
            {
                int divider_row;
                int text_row;

                equipment_weight_layout_rows(1, k, term_hgt, &divider_row,
                    &text_row);
                if ((k + 1) < term_hgt)
                    Term_erase(clear_col, k + 1, 255);
                if ((k + 2) < term_hgt)
                    Term_erase(clear_col, k + 2, 255);

                log_debug("show_equip_enhanced: Displaying armour weight at rows %d/%d (k=%d)",
                    divider_row, text_row, k);

                if (divider_row >= 0)
                {
                    log_trace("show_equip_enhanced: Rendering armour divider at row %d",
                        divider_row);
                    story_print_text_grid(divider_row, weight_col, 8,
                        TERM_L_DARK, "--------");
                }
                strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
                    armour_weight / 10, armour_weight % 10);
                log_debug("show_equip_enhanced: Armour weight text: '%s'", tmp_val);
                if (text_row >= 0)
                    story_print_text_grid(text_row, MAX(0, weight_col - 8), 16,
                        TERM_SLATE, tmp_val);
            }
            else
            {
                log_debug("show_equip_enhanced: NOT displaying armour weight (armour_weight=%d)", armour_weight);
                if (k && (k < term_hgt - 1))
                    Term_erase(clear_col, k + 1, 255);
            }
        }
        else
        {
            log_trace("show_equip_enhanced: Calling show_equip() [mono path]");
            show_equip();
            log_trace("show_equip_enhanced: show_equip() FINISHED");
        }
        
        const bool controller_controls = steamdeck_controls_active();
        bool prompt_context_visible = enhanced_menu_format_prompt(out_val,
            sizeof(out_val), term_wid, use_story_font, true);
        prt(out_val, 0, 0);
        ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_DROP, 0, 0,
            out_val, "drop");
        if (prompt_context_visible)
            ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_SWITCH, 0, 0,
                out_val, "Equipped");
        else
            ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_SWITCH, 0, 0,
                out_val, "cycle");
        if (drop_click_mode)
            enhanced_menu_highlight_prompt_token(out_val, "drop", TERM_YELLOW);
        
        /* Highlight current selection - find the display row for this equipped item */
        if (!use_story_font && highlight_active && highlight_index >= 0 && highlight_index < k)
        {
            /* Get the actual slot index of the highlighted equipped item */
            int highlighted_slot = out_index[highlight_index];
            
            log_trace("show_equip_enhanced: MONO highlight overlay - item %d (slot %d)", highlight_index, highlighted_slot);
            log_debug("show_equip_enhanced: Highlighting equipped item %d (slot %d)", highlight_index, highlighted_slot);
            
            int display_row = highlight_index + 1;
            
            if (display_row > 0)
            {
                /* Get the item */
                object_type* o_ptr = &inventory[highlighted_slot];
                
                log_debug("show_equip_enhanced: Found display row %d for slot %d", display_row, highlighted_slot);
                
                /* Clear the line (exactly like show_equip) */
                Term_erase(clear_col, display_row, 255);

                byte selected_attr =
                    inventory_menu_selected_attr(out_color[highlight_index]);
                byte prefix_attr = selected_attr;
                byte line_attr = selected_attr;
                byte label_attr = selected_attr;
                
                /* Mention the use (exactly like show_equip) */
                strnfmt(tmp_val, sizeof(tmp_val), "%-12s: ", mention_use(highlighted_slot));
                
                /* Draw tile if in graphics mode */
                int text_col = col + 12 + 2;
                int label_col = label_col_base;
                int selection_end = label_col + 4;

                inventory_menu_fill_selected_span(col, selection_end,
                    display_row, selected_attr);
                if (o_ptr->k_idx)
                {
                    text_col = draw_item_tile_with_background(col + 12 + 2,
                        display_row, o_ptr, selected_attr);
                }

                c_put_str(prefix_attr, tmp_val, display_row, col);

                
                /* Display the entry itself (exactly like show_equip) */
                c_put_str(line_attr, out_desc[highlight_index], display_row, text_col);
                
                /* Display the weight if needed (exactly like show_equip) */
                if (show_weights && o_ptr->weight)
                {
                    int wgt = o_ptr->weight * o_ptr->number;
                    sprintf(tmp_val, "%3d.%1d lb", wgt / 10, wgt % 10);
                    c_put_str(line_attr, tmp_val, display_row, weight_col);
                }
                
                if (highlighted_slot == INVEN_QUIVER2)
                {
                    /* Account for potential tile offset when calculating note position */
                    int note_col = text_col + (int)strlen(out_desc[highlight_index]);
                    c_put_str(TERM_L_DARK, " (keeps passive bonuses)", display_row, note_col);
                }

                /* Print the item letter at the end with highlight */
                sprintf(tmp_val, " (%c)", index_to_label(highlighted_slot));
                c_put_str(label_attr, tmp_val, display_row, label_col);
                
                log_debug("show_equip_enhanced: Drew highlight at display row %d, col %d", display_row, col);
            }
        }

        for (int click_i = 0; click_i < k; click_i++)
        {
            int click_slot = out_index[click_i];
            if (click_slot >= INVEN_WIELD && click_slot < INVEN_TOTAL
                && inventory[click_slot].k_idx)
            {
                ui_menu_click_add(click_i, col, click_i + 1, term_wid - col);
            }
        }
        
        /* Get a key */
        which = inkey();

        {
            int clicked_row = -1;
            int click_action = UI_MENU_CLICK_PRIMARY;

            bool click_taken =
                ui_menu_click_take_action(&clicked_row, &click_action);

            if (click_taken && clicked_row == ENHANCED_MENU_CLICK_SWITCH)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                done = true;
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                continue;
            }

            if (click_taken && clicked_row == ENHANCED_MENU_CLICK_DROP)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                drop_click_mode = !drop_click_mode;
                continue;
            }

            if (click_taken && clicked_row >= 0 && clicked_row < k)
            {
                int clicked_slot = out_index[clicked_row];
                if (clicked_slot >= INVEN_WIELD && clicked_slot < INVEN_TOTAL
                    && inventory[clicked_slot].k_idx)
                {
                    highlight_index = clicked_row;
                    highlight_active = true;
                    enhanced_equipment_selected_item = clicked_slot;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;

                    done = true;
                    enhanced_equip_action = (click_action == UI_MENU_CLICK_SECONDARY)
                        ? ENHANCED_ACTION_EXAMINE
                        : (drop_click_mode ? ENHANCED_ACTION_DROP
                                           : enhanced_menu_primary_select_action());
                    if ((enhanced_equip_action == ENHANCED_ACTION_USE
                            || enhanced_equip_action == ENHANCED_ACTION_DROP)
                        && !death_spectator_allow_menu_action())
                    {
                        done = false;
                        enhanced_equip_action = ENHANCED_ACTION_NONE;
                    }
                    continue;
                }
            }
            if (click_taken && click_action == UI_MENU_CLICK_HOVER)
                continue;
            if (!click_taken && which == UI_MENU_CLICK_WAKE_KEY)
                continue;
        }
        which = steamdeck_menu_key(which, 'e', 'i');
        
        log_trace("show_equip_enhanced: Key pressed: %d ('%c')", which, (which >= 32 && which <= 126) ? which : '?');
        
        /* Parse it */
        switch (which)
        {
        case ESCAPE:
            log_trace("show_equip_enhanced: ESC pressed, setting action to 0 and exiting");
            enhanced_equip_action = ENHANCED_ACTION_NONE;  /* Explicitly set to exit */
            done = true;
            break;
        
        case 'e':
            if ((current_menu_command == 0) && inventory_menu_same_button_cycle_enabled()) {
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_equip_enhanced: Direct access E key - same-button cycle to inventory (action=1)");
                done = true;
                break;
            }

            /* Already in equipment */
            break;
        
        case 'i':
            /* Handle I key based on access mode and portable UI support */
            {
                extern char current_menu_command;
                if (current_menu_command != 0) {
                    /* Command access (u/x pressed) */
                    if (controller_controls) {
                        /* Controller UI: E/I switch menus */
                        enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                        done = true;
                    } else {
                        /* Keyboard-only: E/I are just letters, not menu switching */
                        /* Fall through to default letter handling */
                        goto equip_default_case;
                    }
                } else {
                    /* Direct access (i/e pressed) */
                    if (controller_controls) {
                        /* Controller UI: E/I switch menus */
                        enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                        log_trace("show_equip_enhanced: Direct access I key - switching to inventory (action=1)");
                        done = true;
                    } else {
                        /* Keyboard-only: E/I are just letters */
                        goto equip_default_case;
                    }
                }
            }
            break;
        
        /* Handle cycling when the original command is pressed */
        case 'u':
            if (current_menu_command == which) {
                /* Same command - cycle to inventory */
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_equip_enhanced: Command cycling (%c) - switching to inventory (action=1)", which);
                done = true;
            }
            /* Different command does nothing */
            break;

        case 'x':
            if (current_menu_command == which) {
                /* Same command - cycle to inventory */
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_equip_enhanced: Command cycling (%c) - switching to inventory (action=1)", which);
                done = true;
            } else if (controller_controls) {
                if (highlight_active && highlight_index >= 0 && highlight_index < k) {
                    enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_equipment_selected_item = out_index[highlight_index];
                    done = true;
                }
            }
            break;
        
        /* Toggle keys - switch to inventory */
        case '/':           /* / - toggle to inventory */
        case KTRL('I'):     /* Ctrl+I - toggle to inventory */
        case KTRL('E'):     /* Ctrl+E - toggle to inventory */
            enhanced_equip_action = ENHANCED_ACTION_SWITCH;
            done = true;
            break;
        
        case '8':
            if (highlight_active && k > 0) {
                int next = equipment_next_occupied_row(k, out_index,
                    highlight_index, -1);
                if (next >= 0)
                    highlight_index = next;
            }
            break;
        
        case '2':
            if (highlight_active && k > 0) {
                int next = equipment_next_occupied_row(k, out_index,
                    highlight_index, 1);
                if (next >= 0)
                    highlight_index = next;
            }
            break;
        
        case ' ':        /* Space - use item or examine based on context */
        case '\r':       /* Enter - use item or examine based on context */
        case '\n':       /* Enter (alternative) - use item or examine based on context */
            if (highlight_active && highlight_index >= 0 && highlight_index < k) {
                extern char current_menu_command;
                if (current_menu_command == 'x') {
                    /* Examine mode - mark for examination */
                    done = true;
                    enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_equipment_selected_item = out_index[highlight_index];
                } else {
                    if (!death_spectator_allow_menu_action())
                        break;

                    /* Use mode - defer action */
                    done = true;
                    enhanced_equip_action = ENHANCED_ACTION_USE;
                    enhanced_equipment_selected_item = out_index[highlight_index];
                }
            }
            break;

        case '6':        /* Arrow right - description */
            if (highlight_active && highlight_index >= 0 && highlight_index < k) {
                /* Mark that we want to examine an item */
                enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                enhanced_equipment_selected_item = out_index[highlight_index];
                done = true;
            }
            break;

        case '4':        /* Arrow left - drop item */
            if (highlight_active && highlight_index >= 0 && highlight_index < k) {
                if (!death_spectator_allow_menu_action())
                    break;

                done = true;
                enhanced_equip_action = ENHANCED_ACTION_DROP;
                enhanced_equipment_selected_item = out_index[highlight_index];
            }
            break;
        
        default:
        equip_default_case:
            /* Handle item selection by letter */
            if ((which >= 'a' && which <= 'z') || (which >= 'A' && which <= 'Z')) {
                extern char current_menu_command;
                bool allow_letters = false;
                
                /* Letter selection is hidden/disabled only in controller UI mode. */
                allow_letters = !controller_controls;
                
                if (!allow_letters) {
                    bell("Use directions and Space to select items in this mode");
                    break;
                }
                if (death_spectator_active()) {
                    death_spectator_allow_menu_action();
                    break;
                }
                int item = label_to_equip(which);
                if (item >= INVEN_WIELD && item < INVEN_TOTAL && (inventory[item].k_idx || (throw_slot_menu_active && throw_slot_enabled[item]))) {
                    done = true;
                    
                    /* Handle based on menu context */
                    extern char current_menu_command;
                    if (current_menu_command != 0) {
                        /* Command mode - defer action */
                        if (current_menu_command == 'u') {
                            if (!death_spectator_allow_menu_action()) {
                                done = false;
                                break;
                            }
                            enhanced_equip_action = ENHANCED_ACTION_USE;
                            enhanced_equipment_selected_item = item;
                        } else if (current_menu_command == 'x') {
                            /* For observe mode, mark for examination */
                            enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                            enhanced_equipment_selected_item = item;
                        }
                    } else {
                        /* Direct access mode - use the traditional method */
                        if (!death_spectator_allow_menu_action()) {
                            done = false;
                        } else {
                            p_ptr->command_new = which;
                            p_ptr->command_see = true;
                        }
                    }
                }
                else {
                    bell("Illegal object choice!");
                }
            }
            else {
                bell("Invalid command!");
            }
            break;
        }
    }
    
    (void)Term_set_extra_cursor(false, 0, 0, false);
    ui_menu_click_clear();
    ui_scroll_area_clear();
    hide_cursor = saved_hide_cursor;
    (void)Term_set_cursor(saved_cursor);
    story_font_term_pop(&story_state);
    story_equipment_list_active = false;
    log_trace("show_equip_enhanced: Exiting equipment enhanced menu, action=%d", enhanced_equip_action);
}

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

