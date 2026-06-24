/* File: birth/birth-flow.c */

#include "angband.h"
#include "birth/birth-internal.h"

#define BASE_COLUMN 7
#define STAT_TITLE_ROW 14
#define BASE_STAT_ROW 16

typedef struct birth_keyboard_preset_choice {
    u16b preset;
    cptr name;
    cptr text;
} birth_keyboard_preset_choice;

static const birth_keyboard_preset_choice keyboard_preset_choices[] = {
    {
        SDL_MOVEMENT_PRESET_CLASSIC_SIL,
        "Classic Sil",
        "Traditional Sil movement. Numpad 1-9 moves in all eight directions, "
        "and the Home/End/Page navigation block also works. Numpad 5 or z "
        "waits; Shift+z rests. Shift+direction runs and Ctrl+direction "
        "interacts. This keeps letter commands unshadowed and is the best "
        "choice for numpad users or players who already know Sil."
    },
    {
        SDL_MOVEMENT_PRESET_MODERN_ARROWS,
        "Modern Arrows",
        "Arrow keys move north, south, west, and east. The nearby punctuation "
        "keys fill the diagonals: ; and ' move northwest/northeast, while . "
        "and / move southwest/southeast. Numpad movement still works. "
        "Shift+direction runs and Ctrl+direction interacts. Because . and / "
        "are diagonals in this preset, Shift+. and Shift+/ are movement runs "
        "instead of the usual > and ? commands."
    },
    {
        SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC,
        "Modern WASD+QEZC",
        "Keyboard-cluster movement: W/A/S/D move the four cardinal directions, "
        "Q/E move the upper diagonals, and Z/C move the lower diagonals. "
        "Numpad movement still works. Shift+direction runs and Ctrl+direction "
        "interacts. These letters normally belong to commands, so their "
        "command versions move to Alt+letter; for example Alt+w wields and "
        "Alt+Shift+s toggles stealth. Extra aliases are available in this "
        "preset: n sings, v examines, k activates a staff, and Shift+n "
        "toggles stealth."
    },
    {
        SDL_MOVEMENT_PRESET_VI_KEYS,
        "Vi Keys",
        "Roguelike/Vi movement. h/j/k/l move west/south/north/east, and "
        "y/u/b/n move the diagonals. Numpad movement still works. "
        "Shift+direction runs and Ctrl+direction interacts. Like WASD, these "
        "letters shadow normal commands while you are in the dungeon; use "
        "Alt+letter for the lowercase command and Alt+Shift+letter for the "
        "capital command."
    },
};

static bool keyboard_preset_is_builtin(u16b preset)
{
    for (int i = 0; i < (int)N_ELEMENTS(keyboard_preset_choices); i++)
    {
        if (keyboard_preset_choices[i].preset == preset)
            return true;
    }

    return false;
}

static int keyboard_preset_choice_index(u16b preset)
{
    for (int i = 0; i < (int)N_ELEMENTS(keyboard_preset_choices); i++)
    {
        if (keyboard_preset_choices[i].preset == preset)
            return i;
    }

    return 0;
}

static bool birth_keyboard_preset_prompt_needed(void)
{
    if (sdl_config_keyboard_preset_prompt_seen())
        return false;
    if (!config.movement_keyboard_present)
        return false;
    if (!keyboard_preset_is_builtin(config.movement_keyboard_preset))
        return false;

    return true;
}

static cptr keyboard_preset_longest_text(void)
{
    cptr longest = "";
    size_t longest_len = 0;

    for (int i = 0; i < (int)N_ELEMENTS(keyboard_preset_choices); i++)
    {
        cptr text = keyboard_preset_choices[i].text;
        size_t len = text ? strlen(text) : 0;

        if (len > longest_len)
        {
            longest = text;
            longest_len = len;
        }
    }

    return longest;
}

static void birth_keyboard_preset_center_putstr(int row, byte attr, cptr text)
{
    int wid = 80;
    int hgt = 24;
    int col;

    if (!text)
        text = "";

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (row < 0 || row >= hgt)
        return;

    col = (wid - (int)strlen(text)) / 2;
    if (col < 0)
        col = 0;
    Term_putstr(col, row, -1, attr, text);
}

static void birth_keyboard_preset_draw_terminal(int selected)
{
    int wid = 80;
    int hgt = 24;
    int list_row;
    int detail_row;
    int prompt_row;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    list_row = (hgt <= 20) ? 5 : 6;
    detail_row = list_row + (int)N_ELEMENTS(keyboard_preset_choices) + 2;
    prompt_row = hgt - 1;

    Term_clear();
    birth_keyboard_preset_center_putstr(1, TERM_L_BLUE,
        "Choose Keyboard Movement");
    birth_put_wrapped_text(TERM_WHITE,
        "Pick the movement preset for this game. You can change it later from "
        "Options > Input > Keyboard Input.",
        3, 2);

    for (int i = 0; i < (int)N_ELEMENTS(keyboard_preset_choices); i++)
    {
        byte attr = (i == selected) ? TERM_L_BLUE : TERM_WHITE;
        char line[96];

        strnfmt(line, sizeof(line), "%c) %s", I2A(i),
            keyboard_preset_choices[i].name);
        Term_erase(2, list_row + i, wid - 4);
        Term_putstr(2, list_row + i, -1, attr, line);
        ui_menu_click_add(i, 2, list_row + i, wid - 4);
    }

    for (int row = detail_row; row < prompt_row; row++)
        Term_erase(0, row, wid);
    birth_put_wrapped_text(TERM_SLATE, keyboard_preset_choices[selected].text,
        detail_row, 2);

    if (prompt_row >= 0)
    {
        cptr prompt = "Enter select  Esc back";

        Term_erase(0, prompt_row, wid);
        Term_putstr(2, prompt_row, -1, TERM_SLATE, prompt);
        ui_menu_click_add_text_token(-1, 2, prompt_row, prompt, "select");
        ui_menu_click_add_text_token(-2, 2, prompt_row, prompt, "back");
        ui_menu_click_add_text_token(-2, 2, prompt_row, prompt, "Esc");
    }

    Term_fresh();
}

static int birth_keyboard_preset_choose(void)
{
    int selected = keyboard_preset_choice_index(config.movement_keyboard_preset);
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();

    while (true)
    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;
        int key;
        bool semantic_menu;

        steamdeck = steamdeck_controls_active();
        menu_letters = sdl_menu_letters_enabled();

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        semantic_menu = sdl_character_sheet_screen_begin_select(selected,
            "Choose Keyboard Movement");
        if (semantic_menu)
        {
            sdl_character_sheet_screen_set_select_menu_style(true);
            sdl_character_sheet_screen_set_select_dynamic_description(true);
            sdl_character_sheet_screen_set_select_size_hint(
                keyboard_preset_longest_text());
            sdl_character_sheet_screen_set_select_description(
                keyboard_preset_choices[selected].text);

            for (int i = 0; i < (int)N_ELEMENTS(keyboard_preset_choices); i++)
            {
                char label[96];

                if (menu_letters && !sdl_touch_only_device_active())
                    strnfmt(label, sizeof(label), "%c) %s", I2A(i),
                        keyboard_preset_choices[i].name);
                else
                    SDL_strlcpy(label, keyboard_preset_choices[i].name,
                        sizeof(label));
                sdl_character_sheet_screen_add_select_row(i, label,
                    i == selected ? TERM_L_BLUE : TERM_WHITE, "");
            }
            sdl_character_sheet_screen_commit_select(selected);
        }
        else
        {
            birth_keyboard_preset_draw_terminal(selected);
        }

        hide_cursor = true;
        key = inkey();
        hide_cursor = false;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (click_action == UI_MENU_CLICK_HOVER)
            {
                if (clicked_choice >= 0
                    && clicked_choice
                        < (int)N_ELEMENTS(keyboard_preset_choices))
                {
                    selected = clicked_choice;
                }
                continue;
            }

            if (clicked_choice >= 0
                && clicked_choice < (int)N_ELEMENTS(keyboard_preset_choices))
            {
                if (selected == clicked_choice)
                {
                    ui_menu_click_clear();
                    sdl_character_sheet_screen_hide();
                    return selected;
                }
                selected = clicked_choice;
                continue;
            }
            if (clicked_choice == -1)
                key = '\r';
            else if (clicked_choice == -2)
                key = ESCAPE;
        }
        else if (key == UI_MENU_CLICK_WAKE_KEY)
        {
            continue;
        }
        else
        {
            sdl_hover_tooltip_clear();
        }

        key = steamdeck_menu_key((char)key, '8', '2');

        if (key == ESCAPE || key == '4'
            || (steamdeck && key == steamdeck_back_key()))
        {
            ui_menu_click_clear();
            sdl_character_sheet_screen_hide();
            return -1;
        }
        if (key == '8')
        {
            selected = (selected + (int)N_ELEMENTS(keyboard_preset_choices) - 1)
                % (int)N_ELEMENTS(keyboard_preset_choices);
            continue;
        }
        if (key == '2')
        {
            selected = (selected + 1)
                % (int)N_ELEMENTS(keyboard_preset_choices);
            continue;
        }
        if (menu_letters)
        {
            for (int i = 0; i < (int)N_ELEMENTS(keyboard_preset_choices); i++)
            {
                if (key == I2A(i) || key == toupper(I2A(i)))
                {
                    selected = i;
                    key = '\r';
                    break;
                }
            }
        }
        if (birth_confirm_input((char)key, steamdeck) || key == '6')
        {
            ui_menu_click_clear();
            sdl_character_sheet_screen_hide();
            return selected;
        }

        bell("Illegal response to question!");
    }
}

static NavResult birth_maybe_choose_keyboard_preset(void)
{
    int choice;
    u16b preset;

    if (!birth_keyboard_preset_prompt_needed())
        return NAV_OK;

    choice = birth_keyboard_preset_choose();
    if (choice < 0)
        return NAV_BACK;

    preset = keyboard_preset_choices[choice].preset;
    sdl_config_set_default_movement_bindings(&config, preset);
    sdl_config_apply_keyboard_keymaps(&config);
    sdl_config_mark_keyboard_preset_prompt_seen();
    save_pane_config_to_json();
    log_info("Selected first-game keyboard movement preset: %s",
        sdl_config_movement_preset_label(preset));
    return NAV_OK;
}

/*
 * Helper function for 'player_birth()'.
 *
 * See "display_player" for screen layout code.
 */
static NavResult player_birth_aux(void)
{

    log_debug("Initializing character data and history");

    SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name, sizeof(op_ptr->full_name));
    process_player_name(true);  /* CRITICAL: Must pass true to update savefile path! */
    /* Clear the previous history strings */
    p_ptr->history[0] = '\0';
    SDL_strlcat(
                p_ptr->history, (c_text + c_info[p_ptr->pcharacter].text), sizeof(p_ptr->history));

    p_ptr->wt = 0;
    p_ptr->ht = 0;
    p_ptr->age = 0;

    {
        NavResult preset_result = birth_maybe_choose_keyboard_preset();
        if (preset_result != NAV_OK)
            return preset_result;
    }

    /* Oath selection (after character creation, before tutorial/stats) */
    if (run_mode_is_blitz() && !blitz_oaths_enabled())
    {
        p_ptr->oath_type = 0;
    }
    else
    {
        log_debug("Entering oath selection");
        NavResult oath_result = select_oath();
        if (oath_result != NAV_OK) return oath_result;
        log_debug("Oath selection completed");
    }

    if (run_mode_is_blitz())
    {
        NavResult blitz_effects = blitz_configure_effects();
        if (blitz_effects != NAV_OK)
            return blitz_effects;
    }

    /* Point-based flow */
    if (blitz_auto_allocates_stats())
    {
        NavResult auto_result = blitz_auto_build_character();
        if (auto_result != NAV_OK)
            return auto_result;
    }
    else
    {
        int stat_alloc[A_MAX];

        for (int i = 0; i < A_MAX; i++)
            stat_alloc[i] = p_ptr->stat_base[i];

        for (;;)
        {
            /* Stats allocation screen */
            log_debug("Entering stats allocation");
            NavResult s = player_birth_aux_2(stat_alloc);
            if (s == NAV_OK) {
                /* Skill allocation: Esc returns to stats; q returns to character selection. */
                log_debug("Stats accepted, entering skills allocation");
                screen_push_touch_pane_hidden();
                NavResult g = gain_skills();
                screen_pop_touch_pane_hidden();
                if (g == NAV_BACK) continue;
                if (g == NAV_TO_CHARACTER) return NAV_BACK;
                if (g != NAV_OK) return g;
                log_debug("Skills allocation completed");
                break; /* accepted */
            }
            if (s == NAV_BACK)   return NAV_BACK;    /* back to Character Selection */
            if (s == NAV_TO_CHARACTER) return NAV_BACK; /* back to Character Selection */
            if (s == NAV_TO_MAIN) return NAV_TO_MAIN;/* back to main menu */
            if (s == NAV_QUIT)   return NAV_QUIT;    /* hard exit */
            /* any other value: loop again */
        }
    }

    // Reset the number of artefacts
    p_ptr->artefacts = 0;

    log_trace("Final character stats: Str=%d Dex=%d Con=%d Gra=%d",
              p_ptr->stat_base[A_STR], p_ptr->stat_base[A_DEX],
              p_ptr->stat_base[A_CON], p_ptr->stat_base[A_GRA]);

    /* Accept */
    return NAV_OK;
}

/*
 * Create a new character.
 *
 * Note that we may be called with "junk" leftover in the various
 * fields, so we must be sure to clear them first.
 */
NavResult player_birth()
{
    int i;

    char raw_date[25];
    char clean_date[25];
    char month[4];
    time_t ct = time((time_t*)0);

    log_info("Starting character creation process");
    killer_reset();

    /* Create a new character */
    while (1)
    {
        NavResult r = player_birth_aux();
        if (r == NAV_OK) break;
        if (r == NAV_BACK) return NAV_BACK;         /* back to character_selection */
        if (r == NAV_TO_CHARACTER) return NAV_BACK; /* back to character_selection */
        if (r == NAV_TO_MAIN) return NAV_TO_MAIN;   /* back to main menu */
        if (r == NAV_QUIT) return NAV_QUIT;         /* hard exit */
        /* Any other value -> retry loop */
    }

    for (i = 0; i < NOTES_LENGTH; i++)
    {
        notes_buffer[i] = '\0';
    }

    /* Get date */
    (void)strftime(raw_date, sizeof(raw_date), "@%Y%m%d", localtime(&ct));

    sprintf(month, "%.2s", raw_date + 5);
    atomonth(atoi(month), month);

    if (*(raw_date + 7) == '0')
        sprintf(
            clean_date, "%.1s %.3s %.4s", raw_date + 8, month, raw_date + 1);
    else
        sprintf(
            clean_date, "%.2s %.3s %.4s", raw_date + 7, month, raw_date + 1);

    /* Add in "character start" information */
    SDL_strlcat(notes_buffer,
        format("%s of the %s\n", op_ptr->full_name, p_name + rp_ptr->name),
        sizeof(notes_buffer));
    SDL_strlcat(notes_buffer, format("Entered Angband on %s\n", clean_date),
        sizeof(notes_buffer));
    SDL_strlcat(
        notes_buffer, "\n   Turn     Depth   Note\n\n", sizeof(notes_buffer));

    /* Note player birth in the message recall */
    message_add(" ", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add("====================", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add(" ", MSG_GENERIC);

    /* Hack -- outfit the player */
    player_outfit();

    /* Load persistent settings from metarun if this is a continuing metarun */
    if (!run_mode_is_blitz())
        metarun_load_persistent_settings();

    /* Reapply app-wide settings after character creation so UI preferences are
     * not sourced from the metarun or savefile. */
    sdl_config_load_app_options(get_sdl_config_path());

    log_info("Character creation completed: %s the %s", op_ptr->full_name, p_name + rp_ptr->name);

    return NAV_OK;
}











