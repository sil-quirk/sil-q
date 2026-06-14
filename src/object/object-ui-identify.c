/* File: object/object-ui-identify.c */

#include "angband.h"
#include "externs.h"
#include "object/object-ui-identify.h"
#include "object/object-internal.h"
#include "log/log.h"
#include "sdl-config.h"
#include "supplies.h"
#include <ctype.h>


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

static void draw_ident_line(const ident_entry* entry, int row, int col,
    int weight_col, bool highlight)
{
    byte attr = highlight ? TERM_L_BLUE : entry->color;
    byte label_attr = highlight ? TERM_L_BLUE : TERM_WHITE;
    int offset = col + 3;
    char weight_buf[16];

    prt("", row, col);

    if (highlight)
        c_put_str(label_attr, entry->label, row, col);
    else
        put_str(entry->label, row, col);

    if (entry->prefix[0] != '\0')
    {
        if (highlight)
            c_put_str(attr, entry->prefix, row, offset);
        else
            put_str(entry->prefix, row, offset);
        offset += (int)strlen(entry->prefix);
    }

    c_put_str(attr, entry->desc, row, offset);

    if (show_weights)
    {
        int wgt = entry->o_ptr->weight * entry->o_ptr->number;
        if (entry->o_ptr->weight || entry->type != IDENT_ENTRY_EQUIP)
        {
            strnfmt(weight_buf, sizeof(weight_buf), "%2d.%1d lb", wgt / 10, wgt % 10);
            c_put_str(attr, weight_buf, row, weight_col);
        }
    }
}

bool display_unified_identify_menu(bool include_floor, int* out_item, object_type** out_object)
{
    ident_entry entries[MAX_IDENT_ENTRIES];
    int entry_count = 0;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int len = 29;
    const int base_lim = term_wid - 3;
    const int lim_no_weight = base_lim - (show_weights ? 9 : 0);
    int floor_list[MAX_FLOOR_STACK];
    int floor_num = 0;
    int supply_count = supplies_entry_count();

    if (include_floor)
        floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);

    if (include_floor)
    {
        for (int i = 0; i < floor_num && entry_count < MAX_IDENT_ENTRIES; i++)
        {
            int o_idx = floor_list[i];
            object_type* o_ptr = &o_list[o_idx];
            if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
                continue;

            ident_entry* entry = &entries[entry_count];
            entry->type = IDENT_ENTRY_FLOOR;
            entry->index = 0;
            entry->supply_index = -1;
            entry->floor_o_idx = o_idx;
            entry->o_ptr = o_ptr;
            strnfmt(entry->label, sizeof(entry->label), "-)");
            entry->prefix[0] = '\0';

            object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
            if (lim_no_weight >= 0)
                entry->desc[lim_no_weight] = '\0';

            entry->color = weapon_glows(o_ptr) 
                ? object_display_color(o_ptr, TERM_L_BLUE)
                : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

            int row_len = (int)strlen(entry->desc) + 5;
            if (show_weights)
                row_len += 9;
            if (row_len > len)
                len = row_len;

            entry_count++;
        }
    }

    for (int i = 0; i < supply_count && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = supplies_entry_at(i);
        if (!o_ptr || !o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        ident_entry* entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_SUPPLY;
        entry->index = i;
        entry->supply_index = i;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, entry->label);

        const char* supply_prefix = "Supplies: ";
        if (o_ptr->tval == TV_POTION)
            supply_prefix = "Supplies (potions): ";
        else if (o_ptr->tval == TV_GEM)
            supply_prefix = "Supplies (gems): ";
        else if (o_ptr->tval == TV_FOOD)
            supply_prefix = "Supplies (food): ";
        else if (supplies_group_matches_object(SUPPLY_GROUP_LIGHTS, o_ptr))
            supply_prefix = "Supplies (lights/oil): ";

        strnfmt(entry->prefix, sizeof(entry->prefix), "%s", supply_prefix);

        int prefix_len = (int)strlen(entry->prefix);
        int desc_lim = lim_no_weight - prefix_len;
        if (desc_lim < 0)
            desc_lim = 0;

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
        entry->desc[desc_lim] = '\0';

        entry->color = weapon_glows(o_ptr) 
            ? object_display_color(o_ptr, TERM_L_BLUE)
            : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        int row_len = prefix_len + (int)strlen(entry->desc) + 5;
        if (show_weights && o_ptr->weight)
            row_len += 9;
        if (row_len > len)
            len = row_len;

        entry_count++;
    }

    for (int i = 0; i < INVEN_PACK && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        ident_entry* entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_INVEN;
        entry->index = i;
        entry->supply_index = -1;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, entry->label);
        entry->prefix[0] = '\0';

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
        if (lim_no_weight >= 0)
            entry->desc[lim_no_weight] = '\0';

        entry->color = weapon_glows(o_ptr) ? TERM_L_BLUE
            : tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        int row_len = (int)strlen(entry->desc) + 5;
        if (show_weights)
            row_len += 9;
        if (row_len > len)
            len = row_len;

        entry_count++;
    }

    for (int i = INVEN_WIELD; i < INVEN_TOTAL && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        ident_entry* entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_EQUIP;
        entry->index = i;
        entry->supply_index = -1;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, entry->label);
        strnfmt(entry->prefix, sizeof(entry->prefix), "%-12s: ", mention_use(i));

        int prefix_len = (int)strlen(entry->prefix);
        int desc_lim = lim_no_weight - prefix_len;
        if (desc_lim < 0)
            desc_lim = 0;

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
        entry->desc[desc_lim] = '\0';

        entry->color = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        int row_len = prefix_len + (int)strlen(entry->desc) + 5;
        if (show_weights && o_ptr->weight)
            row_len += 9;
        if (row_len > len)
            len = row_len;

        entry_count++;
    }

    if (entry_count == 0)
    {
        msg_print("There is nothing unidentified here.");
        return false;
    }

    int col = menu_center_col_for_len(term_wid, len);
    int highlight = 0;
    bool done = false;
    bool success = false;
    bool controller_controls = steamdeck_controls_active();

    screen_save();

    int clear_start = (col > 1) ? (col - 2) : col;
    int clear_width = term_wid - clear_start;
    if (clear_width < 0)
        clear_width = 0;
    int base_rows = MIN(term_hgt - 1, entry_count + 1);
    int rows_to_clear = base_rows;

    log_trace("display_unified_identify_menu: init clear entry_count=%d, start_col=%d, width=%d, rows=%d",
        entry_count, clear_start, clear_width, rows_to_clear);

    while (!done)
    {
        char prompt[96];

        Term_erase(0, 0, 255);

        for (int row = 1; row <= rows_to_clear && row < term_hgt; row++)
        {
            Term_erase(clear_start, row, clear_width);
        }
        log_trace("display_unified_identify_menu: redraw cleared rows 1-%d from col %d width %d",
            MIN(rows_to_clear, term_hgt - 1), clear_start, clear_width);

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        ui_menu_click_set_touch_category(
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_begin(1, term_hgt - 1,
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_set_keys('8', '2', '6', '4');

        if (controller_controls)
        {
            char confirm_label[16];
            char inspect_label[16];
            char back_label[16];
            const char* variants[4];
            char prompt_full[96];
            char prompt_mid[96];
            char prompt_short[80];

            inventory_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            inventory_prompt_label(steamdeck_info_key(), "RS Right",
                inspect_label, sizeof(inspect_label));
            inventory_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(prompt_full, sizeof(prompt_full),
                "Identify: %s select  %s inspect  %s cancel", confirm_label,
                inspect_label, back_label);
            strnfmt(prompt_mid, sizeof(prompt_mid),
                "%s select  %s inspect  %s cancel", confirm_label,
                inspect_label, back_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "%s select  %s cancel", confirm_label, back_label);
            variants[0] = prompt_full;
            variants[1] = prompt_mid;
            variants[2] = prompt_short;
            variants[3] = "Select  Cancel";
            terminal_prompt_pick_variant(prompt, sizeof(prompt), term_wid,
                false, variants, N_ELEMENTS(variants));
            prt(prompt, 0, 0);
            ui_menu_click_add_text_token(-2, 0, 0, prompt, confirm_label);
            ui_menu_click_add_text_token(-3, 0, 0, prompt, inspect_label);
            ui_menu_click_add_text_token(-3, 0, 0, prompt, "inspect");
            ui_menu_click_add_text_token(-1, 0, 0, prompt, back_label);
            ui_menu_click_add_text_token(-1, 0, 0, prompt, "cancel");
        }
        else if (sdl_touch_only_device_active())
        {
            const char* variants[] = {
                "Tap a row to identify, tap inspect, tap away to exit",
                "Tap to identify, tap inspect",
                "Tap to identify"
            };
            terminal_prompt_pick_variant(prompt, sizeof(prompt), term_wid,
                false, variants, N_ELEMENTS(variants));
            prt(prompt, 0, 0);
            /* Keep the secondary "inspect" action tappable on touch. */
            ui_menu_click_add_text_token(-3, 0, 0, prompt, "inspect");
        }
        else
        {
            const char* variants[] = {
                "Identify: Space select  Left inspect  Esc cancel",
                "Space select  Left inspect  Esc cancel",
                "Space select  Esc cancel"
            };
            terminal_prompt_pick_variant(prompt, sizeof(prompt), term_wid,
                false, variants, N_ELEMENTS(variants));
            prt(prompt, 0, 0);
            ui_menu_click_add_text_token(-2, 0, 0, prompt, "Space");
            ui_menu_click_add_text_token(-3, 0, 0, prompt, "inspect");
            ui_menu_click_add_text_token(-1, 0, 0, prompt, "cancel");
        }

        for (int i = 0; i < entry_count; i++)
        {
            draw_ident_line(&entries[i], i + 1, col, weight_col,
                (i == highlight));
            ui_menu_click_add_full_row(i, i + 1);
        }

        if (entry_count && entry_count < term_hgt - 1)
            prt("", entry_count + 1, col);

        rows_to_clear = base_rows;

        int key = inkey();
        bool click_generated_command = false;
        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < entry_count)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        highlight = clicked_choice;
                        continue;
                    }

                    if (click_action == UI_MENU_CLICK_SECONDARY)
                    {
                        highlight = clicked_choice;
                        key = '4';
                        click_generated_command = true;
                    }
                    else if (clicked_choice != highlight)
                    {
                        highlight = clicked_choice;
                        continue;
                    }
                    else
                    {
                        key = ' ';
                        click_generated_command = true;
                    }
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1) {
                    key = ESCAPE;
                    click_generated_command = true;
                } else if (clicked_choice == -2) {
                    key = ' ';
                    click_generated_command = true;
                } else if (clicked_choice == -3) {
                    key = '4';
                    click_generated_command = true;
                }
            }
        }

        if (!click_generated_command)
            key = steamdeck_menu_key(key, 0, 0);
        if (controller_controls && key == steamdeck_info_key())
            key = '4';

        switch (key)
        {
        case UI_MENU_CLICK_WAKE_KEY:
            break;

        case ESCAPE:
            done = true;
            success = false;
            break;

        case '8':
        case 'k':
        case 'K':
            highlight = (highlight + entry_count - 1) % entry_count;
            break;

        case '2':
        case 'j':
        case 'J':
            highlight = (highlight + 1) % entry_count;
            break;

        case '4':
        case 'h':
        case 'H':
            ui_menu_click_clear();
            ui_scroll_area_clear();
            (void)player_try_identify_smithing_object_on_examine(
                entries[highlight].o_ptr,
                (entries[highlight].type == IDENT_ENTRY_EQUIP));
            object_info_screen(entries[highlight].o_ptr);
            break;

        case ' ':
        case 13:
        case 10:
            success = true;
            done = true;
            break;

        default:
            bell("Invalid command!");
            break;
        }
    }

    ui_menu_click_clear();
    ui_scroll_area_clear();
    screen_load();

    if (!success)
        return false;

    ident_entry* chosen = &entries[highlight];

    if (chosen->type == IDENT_ENTRY_FLOOR)
    {
        *out_item = 0 - chosen->floor_o_idx;
        *out_object = &o_list[chosen->floor_o_idx];
    }
    else if (chosen->type == IDENT_ENTRY_SUPPLY)
    {
        *out_item = SUPPLIES_INDEX + chosen->supply_index;
        *out_object = chosen->o_ptr;
    }
    else
    {
        *out_item = chosen->index;
        *out_object = &inventory[chosen->index];
    }

    return true;
}

/*
 * Returns the paired artefact index for a given artefact, or 0 if none.
 * Paired weapons can be wielded together in main hand and off-hand
 * without requiring Two Weapon Fighting and without off-hand penalties.
 */
