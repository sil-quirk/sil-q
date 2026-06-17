/* File: birth/birth-skills.c */

#include "angband.h"
#include "birth/birth-internal.h"

/*
 * Skill point costs.
 *
 * The nth skill point costs (100*n) experience points
 */
int birth_skill_cost(int base, int points)
{
    int total_cost = (points + base) * (points + base + 1) / 2;
    int prev_cost = (base) * (base + 1) / 2;
    return ((total_cost - prev_cost) * 100);
}

static int gain_skills_initial_skill = -1;

void gain_skills_set_initial_skill(int skill)
{
    if (skill < 0 || skill >= S_MAX || skill == S_SPC)
        gain_skills_initial_skill = -1;
    else
        gain_skills_initial_skill = skill;
}

static char gain_skills_screen_char(int row, int col)
{
    unsigned char ch;

    if (!Term || !Term->scr || !Term->scr->c)
        return ' ';
    if (row < 0 || row >= Term->hgt || col < 0 || col >= Term->wid)
        return ' ';

    ch = (unsigned char)Term->scr->c[row][col];
    if (!ch || ch == (unsigned char)Term->char_blank)
        return ' ';

    return (char)ch;
}

static bool gain_skills_screen_text_matches(int row, int col, cptr text,
    int len)
{
    if (!text || len <= 0)
        return false;

    for (int i = 0; i < len; i++)
    {
        if (gain_skills_screen_char(row, col + i) != text[i])
            return false;
    }

    return true;
}

static bool gain_skills_screen_row_has_value(int row, int start_col)
{
    int wid = 0;
    int hgt = 0;

    if (Term)
        Term_get_size(&wid, &hgt);
    (void)hgt;

    for (int col = start_col; col < wid; col++)
    {
        if (gain_skills_screen_char(row, col) == '=')
            return true;
    }

    return false;
}

static void gain_skills_register_visible_skill_clicks(void)
{
    int wid = 80;
    int hgt = 24;

    if (!Term || !Term->scr || !Term->scr->c)
        return;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    for (int skill = 0; skill < S_MAX; skill++)
    {
        cptr name;
        int name_len;
        int match_len;
        bool found = false;

        if (skill == S_SPC)
            continue;

        name = skill_names_full[skill];
        if (!name || !name[0])
            continue;

        name_len = (int)strlen(name);
        match_len = name_len;
        if (match_len > 5)
            match_len = 5;
        if (match_len < 4)
            match_len = name_len;

        for (int row = 0; row < hgt - 1 && !found; row++)
        {
            for (int col = 0; col <= wid - match_len; col++)
            {
                int start_col;

                if (!gain_skills_screen_text_matches(row, col, name,
                    name_len)
                    && !gain_skills_screen_text_matches(row, col, name,
                        match_len))
                {
                    continue;
                }

                if (!gain_skills_screen_row_has_value(row, col + match_len))
                    continue;

                start_col = MAX(0, col - 1);
                ui_menu_click_add(skill, start_col, row, wid - start_col);
                found = true;
                break;
            }
        }
    }
}

/*
 * Increase your skills by spending experience points
 */
extern NavResult gain_skills(void)
{
    int i;

    int row = 6;
    int col = 43;

    int skill = ((gain_skills_initial_skill >= 0
        && gain_skills_initial_skill < S_MAX
        && gain_skills_initial_skill != S_SPC)
        ? gain_skills_initial_skill
        : 0);

    int old_base[S_MAX];
    int skill_gain[S_MAX];
    int skill_costs[S_MAX];

    int old_new_exp = p_ptr->new_exp;
    int total_cost = 0;

    char ch;

    char buf[80];

    NavResult result = NAV_OK;

    int tab = 0;
    bool force_initial_redraw = true;

    log_debug("Starting skills allocation with %d experience points", p_ptr->new_exp);
    gain_skills_initial_skill = -1;

    // hack global variable
    skill_gain_in_progress = true;

    /* save the old skills */
    for (i = 0; i < S_MAX; i++)
        old_base[i] = p_ptr->skill_base[i];

    /* initialise the skill gains */
    for (i = 0; i < S_MAX; i++)
        skill_gain[i] = 0;

    /* Interact */
    while (1)
    {
        bool steamdeck = steamdeck_controls_active();

        /* Recompute points/costs and apply the temporary skill increases */
        total_cost = 0;

        for (i = 0; i < S_MAX; i++)
        {
            /* Skip Special abilities skill - not trainable */
            if (i == S_SPC)
            {
                skill_costs[i] = 0;
                continue;
            }
            skill_costs[i] = birth_skill_cost(old_base[i], skill_gain[i]);
            total_cost += skill_costs[i];
        }

        p_ptr->new_exp = old_new_exp - total_cost;

        if (p_ptr->new_exp < 0)
        {
            bell("Excessive skills!");
            skill_gain[skill]--;
            continue;
        }

        p_ptr->update |= (PU_BONUS);
        p_ptr->redraw |= (PR_EXP | PR_BASIC);

        for (i = 0; i < S_MAX; i++)
        {
            if (i == S_SPC) continue;
            p_ptr->skill_base[i] = old_base[i] + skill_gain[i];
        }

        update_stuff();

        int wid = 80;
        int hgt = 24;
        Term_get_size(&wid, &hgt);
        if (wid < 1) wid = 80;
        if (hgt < 1) hgt = 24;
        bool compact = (wid < 80);
        int wide_offset = (wid > 80) ? (wid - 80) / 2 : 0;
        int sheet_col = col + wide_offset;

        if (compact)
        {
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            birth_display_skill_allocation_compact(skill, old_base, skill_gain, p_ptr->new_exp, steamdeck);
            gain_skills_register_visible_skill_clicks();
            ui_scroll_area_begin(0, hgt - 2, SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('6', '4', '6', '4');
        }
        else
        {
            int prompt_row = birth_prompt_row();
            int status_row = row + S_SNG + 1;
            int skill_first_row = row;
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            birth_configure_allocation_sheet_layout(false, &skill_first_row,
                &status_row);
            row = skill_first_row;
            ui_scroll_area_begin(row, row + S_SNG,
                SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('6', '4', '6', '4');

            /* Display the player */
            display_player(0);
            display_player_standard_layout_clear();

            /* Display the costs header */
            if (!character_dungeon)
            {
                if (p_ptr->new_exp >= 10000)
                    tab = 0;
                else if (p_ptr->new_exp >= 1000)
                    tab = 1;
                else if (p_ptr->new_exp >= 100)
                    tab = 2;
                else if (p_ptr->new_exp >= 10)
                    tab = 3;
                else
                    tab = 4;

                strnfmt(buf, sizeof(buf), "%6d", p_ptr->new_exp);
                c_put_str(TERM_L_GREEN, buf, row - 2, sheet_col + 30);
                c_put_str(TERM_WHITE, "Points Left:", row - 2, sheet_col + 17 + tab);
            }

            /* Display the costs */
            for (i = 0; i < S_MAX; i++)
            {
                /* Skip Special abilities skill - not trainable */
                if (i == S_SPC) continue;

                if (i == skill)
                {
                    byte attr = TERM_L_BLUE;

                    bool use_story = story_character_enabled();
                    if (use_story) {
                        sdl_story_font_enable();
                    }

                    c_put_str(attr, skill_names_full[i], row + i, sheet_col - 1);

                    if (use_story) {
                        sdl_story_font_disable();
                    }

#ifndef MONOCHROME_MODE
                    strnfmt(buf, sizeof(buf), "%6d",
                        birth_skill_cost(old_base[i], skill_gain[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 30);
#else
                    strnfmt(buf, sizeof(buf), "%6d*",
                        birth_skill_cost(old_base[i], skill_gain[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 30);
                    c_put_str(attr, "*", row + i, sheet_col - 2);
#endif
                }
                else
                {
                    byte attr = TERM_L_WHITE;
                    strnfmt(buf, sizeof(buf), "%6d",
                        birth_skill_cost(old_base[i], skill_gain[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 30);
                }
                ui_menu_click_add(i, sheet_col - 2, row + i, 40);
            }

            if (status_row < prompt_row)
            {
                char skill_buf[80];

                strnfmt(skill_buf, sizeof(skill_buf), "Cost:%d Left:%d",
                    birth_skill_cost(old_base[skill], skill_gain[skill]),
                    p_ptr->new_exp);
                birth_draw_allocation_confirm_status(status_row, sheet_col - 1,
                    sheet_col + 37, skill_buf);
            }

            /* Bottom bar follows character sheet font setting */
            if (story_character_enabled()) {
                sdl_story_font_enable();
            }

            if (steamdeck) {
                char confirm_label[16];
                char back_label[16];
                char prompt_buf[160];

                /* Steam Deck UI: A=confirm, B=back */
                birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));

                {
                    char prompt_full[160];
                    char prompt_short[96];
                    const char* variants[2];

                    strnfmt(prompt_full, sizeof(prompt_full),
                        "D-pad allocate  %s back  %s confirm",
                        back_label, confirm_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "%s confirm  %s back", confirm_label, back_label);
                    variants[0] = prompt_full;
                    variants[1] = prompt_short;
                    terminal_prompt_pick_variant(prompt_buf,
                        sizeof(prompt_buf), Term ? Term->wid - QUESTION_COL
                                                  : 80 - QUESTION_COL,
                        story_character_enabled(), variants,
                        N_ELEMENTS(variants));
                }
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt_buf);
                birth_register_allocation_prompt_clicks(prompt_row,
                    prompt_buf, QUESTION_COL, back_label, confirm_label,
                    "q");
            } else if (sdl_touch_only_device_active()) {
                char prompt_text[160];
                /* Skills are tap-to-add and long-tap-to-subtract rows. */
                const char* variants[] = {
                    "Tap skill +  long tap -  confirm  back",
                    "Tap +  long tap -  confirm"
                };
                terminal_prompt_pick_variant(prompt_text, sizeof(prompt_text),
                    Term ? Term->wid - QUESTION_COL : 80 - QUESTION_COL,
                    story_character_enabled(), variants, N_ELEMENTS(variants));
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE,
                    prompt_text);
                birth_register_allocation_prompt_clicks(prompt_row,
                    prompt_text, QUESTION_COL, "back", "confirm", "q");
            } else {
                char prompt_text[160];
                const char* variants[] = {
                    "Dir allocate  Esc back  Enter confirm",
                    "Enter confirm  Esc back"
                };
                terminal_prompt_pick_variant(prompt_text, sizeof(prompt_text),
                    Term ? Term->wid - QUESTION_COL : 80 - QUESTION_COL,
                    story_character_enabled(), variants, N_ELEMENTS(variants));
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE,
                    prompt_text);
                birth_register_allocation_prompt_clicks(prompt_row,
                    prompt_text, QUESTION_COL, "Esc", "Enter", "q");
            }

            if (story_character_enabled()) {
                sdl_story_font_disable();
            }
        }

        if (force_initial_redraw)
        {
            Term_redraw();
            force_initial_redraw = false;
        }

        (void)sdl_character_sheet_screen_show_birth_skills(old_base,
            skill_gain, skill_costs, skill, p_ptr->new_exp);
        (void)Term_set_cursor(false);
        Term_fresh();

        /* First-time players: guided callouts over the real skills screen. */
        birth_coach_show_once(BIRTH_COACH_SKILLS);

        /* Get key */
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;
        bool click_generated_command = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < S_MAX
                    && clicked_choice != S_SPC)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != skill)
                    {
                        skill = clicked_choice;
                        continue;
                    }
                    ch = (click_action == UI_MENU_CLICK_SECONDARY) ? '4' : '6';
                    click_generated_command = true;
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1) {
                    ch = ESCAPE;
                    click_generated_command = true;
                } else if (clicked_choice == -2) {
                    ch = '\r';
                    click_generated_command = true;
                } else if (clicked_choice == -3) {
                    ch = 'q';
                    click_generated_command = true;
                }
            }
        }

        if (!click_generated_command)
            ch = (char)steamdeck_menu_key(ch, 0, 0);

        /* Return to character selection before the game starts */
        if (((ch == 'Q') || (ch == 'q')) && (turn == 0)) {
            /* restore state before leaving */
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC) /* Don't restore Special abilities skill */
                    p_ptr->skill_base[i] = old_base[i];
            }
            skill_gain_in_progress = false;
            ui_menu_click_clear();
            ui_scroll_area_clear();
            sdl_character_sheet_screen_hide();
            return NAV_TO_CHARACTER;
        }

        /* Done */
        if (birth_confirm_input(ch, steamdeck))
        {
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = NAV_OK;
            break;
        }

        /* Abort */
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
        {
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC) /* Don't restore Special abilities skill */
                    p_ptr->skill_base[i] = old_base[i];
            }
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = NAV_BACK;   /* go back to stat allocation */
            break;
        }

        /* Prev skill */
        if (ch == '8')
        {
            do {
                skill = (skill + S_MAX - 1) % S_MAX;
            } while (skill == S_SPC); /* Skip Special abilities skill */
        }

        /* Next skill */
        if (ch == '2')
        {
            do {
                skill = (skill + 1) % S_MAX;
            } while (skill == S_SPC); /* Skip Special abilities skill */
        }

        /* Decrease skill */
        if ((ch == '4') && (skill_gain[skill] > 0))
        {
            skill_gain[skill]--;
        }

        /* Increase stat */
        if (ch == '6')
        {
            /* Don't allow increasing Special abilities skill */
            if (skill != S_SPC) {
                skill_gain[skill]++;
            }
        }
    }

    // reset hack global variable
    ui_menu_click_clear();
    ui_scroll_area_clear();
    sdl_character_sheet_screen_hide();
    skill_gain_in_progress = false;

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Update stuff */
    update_stuff();

    log_debug("Skills allocation completed, spent %d experience", old_new_exp - p_ptr->new_exp);

    /* Done */
    return result;
}
