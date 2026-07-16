/* File: birth/birth-oath.c */

#include "angband.h"
#include "birth/birth-internal.h"

static int oath_selectable_max_id(void)
{
    int max_oath_id = OATH_LIGHT;

    if (!z_info)
        return max_oath_id;
    if (z_info->oath_max <= 1)
        return 0;
    if (max_oath_id >= z_info->oath_max)
        max_oath_id = z_info->oath_max - 1;
    if (max_oath_id < 0)
        max_oath_id = 0;

    return max_oath_id;
}

static int oath_collect_visible(int available_mask, int* visible_oaths,
    int max_visible)
{
    int visible_count = 0;
    int max_oath_id = oath_selectable_max_id();

    if (visible_oaths && visible_count < max_visible)
        visible_oaths[visible_count] = 0;
    visible_count++;

    for (int i = 1; i <= max_oath_id; i++)
    {
        if (!(available_mask & (1 << (i - 1))) && !oath_banned(i))
            continue;

        if (visible_oaths && visible_count < max_visible)
            visible_oaths[visible_count] = i;
        visible_count++;
    }

    return visible_count;
}

static bool oath_option_selectable(int oath_id, int available_mask)
{
    if (oath_id == 0)
        return true;

    return ((available_mask & (1 << (oath_id - 1))) != 0)
        && !oath_banned(oath_id);
}

static void oath_move_highlight(int* highlight, int direction,
    int available_mask)
{
    int oath_max = oath_selectable_max_id() + 1;
    int original = *highlight;
    int next = *highlight;

    if (oath_max <= 0)
    {
        *highlight = 0;
        return;
    }

    do
    {
        next += direction;
        if (next < 0)
            next = oath_max - 1;
        if (next >= oath_max)
            next = 0;

        if ((next == 0)
            || (available_mask & (1 << (next - 1)))
            || oath_banned(next))
        {
            *highlight = next;
            return;
        }
    } while (next != original);
}

static void oath_add_semantic_detail_line(cptr label, cptr text, byte attr)
{
    char line[1024];

    if (!text || !text[0])
        return;
    if (label && label[0])
        strnfmt(line, sizeof(line), "%s: %s", label, text);
    else
        SDL_strlcpy(line, text, sizeof(line));
    sdl_character_sheet_screen_add_select_detail(line, attr, "");
}

static void oath_add_semantic_details(int oath_id)
{
    if (oath_id < 0 || !z_info || oath_id >= z_info->oath_max)
        return;

    if (oath_banned(oath_id) && oath_id > 0)
    {
        char* banned_text = oath_banned_text(oath_id);

        oath_add_semantic_detail_line(NULL, "OATH BROKEN", TERM_L_RED);
        oath_add_semantic_detail_line(NULL,
            (banned_text && banned_text[0])
                ? banned_text
                : "Thy oath lies shattered, and thy name is marked in shame for this age.",
            TERM_RED);
        return;
    }

    if (oath_id == 0)
    {
        oath_add_semantic_detail_line(NULL, "Walk free of binding words.",
            TERM_SLATE);
        oath_add_semantic_detail_line(NULL,
            "Take no oath and remain unbound by sacred vows.", TERM_SLATE);
        return;
    }

    oath_add_semantic_detail_line("Description", oath_description(oath_id),
        TERM_SLATE);
    oath_add_semantic_detail_line("Pledge", oath_pledge(oath_id),
        TERM_L_BLUE);
    oath_add_semantic_detail_line("Reward", oath_reward_text(oath_id),
        TERM_L_GREEN);
    oath_add_semantic_detail_line("Forbidden", oath_forbidden(oath_id),
        TERM_L_RED);
}

/* Oath selection is always presented by the SDL semantic menu. */
NavResult select_oath(void)
{
    enum {
        OATH_CLICK_BACK = -1,
        OATH_CLICK_SELECT = -2
    };
    int available_mask = get_available_oaths_mask();
    int highlight = 1;
    int choice = 0;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();

    if (available_mask == 0)
    {
        p_ptr->oath_type = 0;
        log_debug("No oaths available, skipping oath selection");
        return NAV_OK;
    }

    for (int i = 1; i <= oath_selectable_max_id(); i++)
    {
        if (available_mask & (1 << (i - 1)))
        {
            highlight = i;
            break;
        }
    }

    while (true)
    {
        int visible_oaths[16];
        int visible_count;
        char key;

        visible_count = oath_collect_visible(available_mask, visible_oaths,
            (int)N_ELEMENTS(visible_oaths));
        if (visible_count > (int)N_ELEMENTS(visible_oaths))
            visible_count = (int)N_ELEMENTS(visible_oaths);

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        sdl_character_sheet_screen_begin_select(highlight,
            "Choose your Oath");
        sdl_character_sheet_screen_set_select_menu_style(true);
        sdl_character_sheet_screen_set_select_dynamic_description(true);

        for (int i = 0; i < visible_count; i++)
        {
            int oath_id = visible_oaths[i];
            byte attr;
            char label[128];
            bool show_letter = menu_letters && !sdl_touch_only_device_active();

            if (oath_banned(oath_id) && oath_id > 0)
                attr = TERM_L_RED;
            else if (oath_id == 0)
                attr = TERM_WHITE;
            else
                attr = TERM_L_BLUE;

            if (show_letter)
                strnfmt(label, sizeof(label), "%c) %s", 'a' + i,
                    oath_name_str(oath_id));
            else
                SDL_strlcpy(label, oath_name_str(oath_id), sizeof(label));
            sdl_character_sheet_screen_add_select_row(oath_id, label, attr,
                "");
        }

        oath_add_semantic_details(highlight);
        sdl_character_sheet_screen_commit_select(highlight);
        Term_fresh();
        key = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < z_info->oath_max)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != highlight)
                    {
                        highlight = clicked_choice;
                        continue;
                    }
                    key = '\r';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == OATH_CLICK_BACK)
                    key = ESCAPE;
                else if (clicked_choice == OATH_CLICK_SELECT)
                    key = '\r';
            }
        }

        key = (char)steamdeck_menu_key(key, '4', '6');
        if (key == ESCAPE || key == 'q'
            || (steamdeck && key == steamdeck_back_key()))
        {
            sdl_character_sheet_screen_hide();
            ui_menu_click_clear();
            return NAV_BACK;
        }

        if (birth_confirm_input(key, steamdeck) || key == '6')
        {
            if (oath_option_selectable(highlight, available_mask))
            {
                choice = highlight;
                break;
            }
        }

        if (menu_letters && key >= 'a' && key < 'a' + visible_count)
        {
            int display_pos = key - 'a';
            int oath_id = visible_oaths[display_pos];

            if (oath_option_selectable(oath_id, available_mask))
            {
                choice = oath_id;
                break;
            }
            continue;
        }

        if (key == '8')
            oath_move_highlight(&highlight, -1, available_mask);
        else if (key == '2')
            oath_move_highlight(&highlight, 1, available_mask);
    }

    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();
    p_ptr->oath_type = choice;

    if (choice > 0 && choice < z_info->oath_max)
    {
        oath_type* oath_ptr = &oath_info[choice];

        if (oath_ptr->reward_type > 0 && oath_ptr->reward_value > 0)
        {
            int skill_category = oath_ptr->reward_type;
            int ability_id = oath_ptr->reward_value;

            if (skill_category >= 0 && skill_category < S_MAX
                && ability_id >= 0 && ability_id < ABILITIES_MAX)
            {
                p_ptr->have_ability[skill_category][ability_id] = true;
                p_ptr->innate_ability[skill_category][ability_id] = true;
                p_ptr->active_ability[skill_category][ability_id] = true;
                log_debug("Granted oath %d abilities from data: skill=%d, ability=%d",
                    choice, skill_category, ability_id);
            }
            else
            {
                log_warn("Oath %d ability out of bounds: skill=%d (max %d), ability=%d (max %d)",
                    choice, skill_category, S_MAX - 1, ability_id,
                    ABILITIES_MAX - 1);
            }
        }
        else
            log_debug("No ability reward found for oath %d", choice);
    }

    if (choice == 0)
        log_debug("No oath selected");
    else
        log_debug("Oath selected: %s (%d)", oath_name_str(choice), choice);

    return NAV_OK;
}
