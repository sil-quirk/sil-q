#include "angband.h"
#include "metarun-internal.h"

bool metarun_set_difficulty_inline(int choice)
{
    int8_t preserved_stacks[METAR_CURSE_SLOTS];
    u64b preserved_seen;

    if (!runtype_info || choice < 0 || choice >= z_info->rt_max
        || !runtype_info[choice].name[0]
        || choice < metar.max_difficulty_reached)
        return false;
    if (choice == metar.type)
        return true;

    log_info("Changing difficulty from %d to %d", metar.type, choice);
    memcpy(preserved_stacks, metar.curse_stacks, sizeof(preserved_stacks));
    preserved_seen = metar.curses_seen;
    memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
    metar.curses_seen = 0;
    metar.type = (byte)choice;
    apply_difficulty_curses(&metar);

    int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
    for (int curse_id = 0; curse_id < limit; curse_id++) {
        int preserved = preserved_stacks[curse_id];
        int combined;
        int curse_cap;
        int blessing_cap;
        if (!preserved) continue;
        combined = preserved + CURSE_GET(curse_id);
        curse_cap = CURSE_CURSE_CAP(curse_id);
        blessing_cap = CURSE_BLESSING_CAP(curse_id);
        if (curse_cap > 0 && combined > curse_cap)
            combined = curse_cap;
        if (blessing_cap > 0 && combined < -blessing_cap)
            combined = -blessing_cap;
        CURSE_SET(curse_id, combined);
    }
    if (choice > metar.max_difficulty_reached)
        metar.max_difficulty_reached = (byte)choice;
    metar.curses_seen |= preserved_seen;
    if (!sync_current_metarun_slot(false))
        log_warn("Difficulty change failed to sync metarun slot");
    save_metaruns();
    return true;
}

static void get_curse_description(int runtype_id, char *buf, size_t buf_size)
{
    if (!runtype_info || runtype_id >= z_info->rt_max || buf_size < 64)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }

    runtype_type *rt = &runtype_info[runtype_id];

    if (!rt->start_curses)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }

    /* Count curses and determine stack ranges */
    int curse_count = 0;
    int min_stacks = 255, max_stacks = 0;

    int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
    for (int curse_id = 0; curse_id < limit; curse_id++)
    {
        if (rt->start_curses & (1ULL << curse_id))
        {
            curse_count++;
            int stacks = rt->curse_stacks[curse_id];
            if (stacks < min_stacks) min_stacks = stacks;
            if (stacks > max_stacks) max_stacks = stacks;
        }
    }

    if (curse_count == 0)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }

    /* Format the description */
    if (min_stacks == max_stacks)
    {
        if (min_stacks == 1)
            snprintf(buf, buf_size, "Curses: %d x %d stack", curse_count, min_stacks);
        else
            snprintf(buf, buf_size, "Curses: %d x %d stacks", curse_count, min_stacks);
    }
    else
    {
        snprintf(buf, buf_size, "Curses: %d (%d-%d stacks)", curse_count, min_stacks, max_stacks);
    }
}

/* Difficulty selection menu */
void choose_difficulty_menu(void)
{
    int choice = metar.type;  /* Start with current difficulty */
    int max_difficulty = (runtype_info && z_info->rt_max > 0) ? z_info->rt_max - 1 : 0;

    screen_save();
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=back */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }

    while (true)
    {
        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);

        /* Title */
        Term_putstr(2, 1, -1, TERM_YELLOW, "=== Select Difficulty Level ===");

        int row = 3;
        for (int i = 0; i <= max_difficulty; i++)
        {
            byte name_color, desc_color;
            byte runtype_color = TERM_WHITE; /* default color */
            bool is_locked = (i < metar.max_difficulty_reached); /* Lock easier difficulties */

            /* Get runtype color from U: field */
            if (runtype_info && i < z_info->rt_max && runtype_info[i].name[0])
            {
                runtype_color = runtype_info[i].colour;
            }
            else
            {
                runtype_color = TERM_WHITE; /* fallback if runtype not loaded */
            }

            if (is_locked) {
                /* Locked (easier) difficulties - greyed out */
                name_color = TERM_L_DARK;
                desc_color = TERM_L_DARK;
                Term_putstr(2, row, -1, TERM_L_DARK, "-");
            }
            else if (i == choice) {
                /* Highlight selected difficulty - use runtype color but brighter */
                name_color = runtype_color;
                desc_color = TERM_L_WHITE;
                Term_putstr(2, row, -1, runtype_color, ">");
            } else if (i == metar.type) {
                /* Show current difficulty in its runtype color but dimmed */
                name_color = runtype_color;
                desc_color = TERM_SLATE;
                Term_putstr(2, row, -1, TERM_L_DARK, " ");
            } else {
                /* Normal difficulty in its runtype color */
                name_color = runtype_color;
                desc_color = TERM_L_DARK;
                Term_putstr(2, row, -1, TERM_L_DARK, " ");
            }

            /* Get dynamic name and stats from runtype */
            const char *rt_name = "Unknown";
            int win_goal = WINCON_SILMARILS;
            u32b blessing_thresh = runtype_threshold_for_mode(i, METARUN_BLESSING_THRESHOLD_NORMAL);

            if (runtype_info && i < z_info->rt_max && runtype_info[i].name[0])
            {
                rt_name = runtype_info[i].name;
                win_goal = runtype_info[i].win_con ? runtype_info[i].win_con : WINCON_SILMARILS;
            }

            char desc_buf[128];
            char curse_buf[64];
            get_curse_description(i, curse_buf, sizeof(curse_buf));

            if (is_locked) {
                snprintf(desc_buf, sizeof(desc_buf), "[LOCKED] Win: %d Silmarils, Threshold: %lu points, %s",
                         win_goal, (unsigned long)blessing_thresh, curse_buf);
            } else {
                snprintf(desc_buf, sizeof(desc_buf), "Win: %d Silmarils, Threshold: %lu points, %s",
                         win_goal, (unsigned long)blessing_thresh, curse_buf);
            }

            char name_buf[128];
            if (!menu_letters) {
                if (is_locked)
                    snprintf(name_buf, sizeof(name_buf), "   %s [LOCKED]", rt_name);
                else
                    snprintf(name_buf, sizeof(name_buf), "   %s", rt_name);
            } else if (is_locked) {
                snprintf(name_buf, sizeof(name_buf), "%c) %s [LOCKED]", 'a'+i, rt_name);
            } else {
                snprintf(name_buf, sizeof(name_buf), "%c) %s", 'a'+i, rt_name);
            }

            Term_putstr(4, row, -1, name_color, name_buf);
            ui_menu_click_add(i, 2, row, 76);
            row++;
            Term_putstr(7, row, -1, desc_color, desc_buf);
            ui_menu_click_add(i, 2, row, 76);
            row++;

            /* Add extra spacing between options */
            row++;
        }

        /* Instructions */
        if (steamdeck) {
            char hint_buf[96];
            char prompt_full[96];
            char prompt_short[80];
            const char* variants[2];

            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad navigate  [%s] accept  [%s] cancel", accept_label,
                back_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "[%s] accept  [%s] cancel", accept_label, back_label);
            variants[0] = prompt_full;
            variants[1] = prompt_short;
            terminal_prompt_pick_variant(hint_buf, sizeof(hint_buf),
                metarun_term_width() - 2, false, variants,
                N_ELEMENTS(variants));
            Term_putstr(2, row + 1, -1, TERM_L_WHITE, hint_buf);
            ui_menu_click_add_text_token(-1, 2, row + 1, hint_buf, "cancel");
        } else if (sdl_touch_only_device_active()) {
            char prompt_text[96];
            const char* variants[] = {
                "Tap a row to select, tap away to exit",
                "Tap to select, tap away to exit",
                "Tap to select"
            };
            terminal_prompt_pick_variant(prompt_text, sizeof(prompt_text),
                metarun_term_width() - 2, false, variants,
                N_ELEMENTS(variants));
            Term_putstr(2, row + 1, -1, TERM_L_WHITE, prompt_text);
        } else {
            char prompt_text[96];
            const char* variants[] = {
                "Dir navigate  Enter accept  Esc cancel",
                "Enter accept  Esc cancel"
            };
            terminal_prompt_pick_variant(prompt_text, sizeof(prompt_text),
                metarun_term_width() - 2, false, variants,
                N_ELEMENTS(variants));
            Term_putstr(2, row + 1, -1, TERM_L_WHITE, prompt_text);
            ui_menu_click_add_text_token(-1, 2, row + 1, prompt_text,
                "Esc Cancel");
            ui_menu_click_add_text_token(-1, 2, row + 1, prompt_text,
                "Esc cancel");
        }

        /* Get input */
        char key = metarun_inkey_hidden();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice <= max_difficulty)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != choice)
                    {
                        choice = clicked_choice;
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

        /* Handle back/cancel - ESC, B button in Steam Deck mode, or 'h' key */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H')))
        {
            ui_menu_click_clear();
            screen_load();
            return;
        }
        else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6')  /* Enter/A button/6 key */
        {
            /* Check if trying to select a locked difficulty */
            if (choice < metar.max_difficulty_reached) {
                /* Show warning and stay in menu */
                Term_putstr(2, row + 3, -1, TERM_RED, "Cannot select easier difficulty - locked for this story run!");
                Term_fresh();
                Term_xtra(TERM_XTRA_DELAY, 2000);
                continue;
            }
            break;  /* Confirm selection */
        }
        else if (key == '8' || key == 'k' || key == '-')  /* Up */
        {
            /* Navigate up but skip locked difficulties */
            int new_choice = choice - 1;
            while (new_choice >= 0 && new_choice < metar.max_difficulty_reached) {
                new_choice--;
            }
            if (new_choice >= 0) choice = new_choice;
        }
        else if (key == '2' || key == 'j' || key == '+')  /* Down */
        {
            /* Navigate down normally */
            if (choice < max_difficulty) choice++;
        }
        else if (menu_letters && key >= 'a' && key <= 'z')  /* Letter selection */
        {
            int new_choice = key - 'a';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    /* Show warning for locked difficulty */
                    Term_putstr(2, row + 3, -1, TERM_RED, "Cannot select easier difficulty - locked for this story run!");
                    Term_fresh();
                    Term_xtra(TERM_XTRA_DELAY, 2000);
                } else {
                    choice = new_choice;
                }
            }
        }
        else if (menu_letters && key >= 'A' && key <= 'Z')  /* Capital letter selection */
        {
            int new_choice = key - 'A';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    /* Show warning for locked difficulty */
                    Term_putstr(2, row + 3, -1, TERM_RED, "Cannot select easier difficulty - locked for this story run!");
                    Term_fresh();
                    Term_xtra(TERM_XTRA_DELAY, 2000);
                } else {
                    choice = new_choice;
                }
            }
        }
    }

    ui_menu_click_clear();

    /* Apply the new difficulty */
    if (choice != metar.type)
    {
        /* Warn if increasing difficulty */
        if (choice > metar.type) {
            int term_wid = metarun_term_width();
            bool portable = portable_controls_active();
            screen_save();
            Term_clear();
            Term_putstr(2, 5, -1, TERM_YELLOW, "WARNING: Increasing Difficulty");
            if (term_wid < 70)
            {
                Term_putstr(2, 7, term_wid - 2, TERM_WHITE,
                    "You cannot return to an easier level");
                Term_putstr(2, 8, term_wid - 2, TERM_WHITE,
                    "for the rest of this story run.");
            }
            else
            {
                Term_putstr(2, 7, -1, TERM_WHITE, "If you increase the difficulty level, you will NOT be able to");
                Term_putstr(2, 8, -1, TERM_WHITE, "go back to an easier level for the rest of this story run.");
            }
            Term_putstr(2, 10, -1, TERM_L_RED, "This change is PERMANENT for this meta-run!");
            if (steamdeck) {
                char prompt_buf[64];
                strnfmt(prompt_buf, sizeof(prompt_buf),
                        "Continue? [%s] yes  [%s] no", accept_label, back_label);
                Term_putstr(2, 12, term_wid - 2, TERM_L_WHITE, prompt_buf);
            } else {
                Term_putstr(2, 12, term_wid - 2, TERM_L_WHITE,
                    portable ? "Do you want to continue? (y/n/sp)"
                             : "Do you want to continue? (y/n)");
            }
            sdl_touch_pane_begin_yes_no_prompt("Do you want to continue?");
            Term_fresh();
            char confirm = metarun_inkey_hidden();
            sdl_touch_pane_end_yes_no_prompt();
            screen_load();

            if (steamdeck) {
                if (confirm == steamdeck_confirm_key() || confirm == ' '
                    || confirm == '\r' || confirm == '\n')
                    confirm = 'y';
                else if (confirm == ESCAPE || confirm == steamdeck_back_key()
                    || confirm == 'h' || confirm == 'H')
                    confirm = 'n';
            } else if (portable
                && (confirm == ' ' || confirm == '\r' || confirm == '\n')) {
                confirm = 'y';
            }

            if (confirm != 'y' && confirm != 'Y') {
                ui_menu_click_clear();
                return; /* Cancel the change */
            }
        }

        (void)metarun_set_difficulty_inline(choice);
    }

    ui_menu_click_clear();
    screen_load();

}

void choose_difficulty_level(void)
{
    choose_difficulty_menu();
    /* The public entry point is used by the story intro, where the statistics
     * screen is not already open.  The story book calls choose_difficulty_menu()
     * directly and refreshes its Difficulty page in place. */
    print_metarun_stats();
}
