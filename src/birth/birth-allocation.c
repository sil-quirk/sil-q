/* File: birth/birth-allocation.c */

#include "angband.h"
#include "birth/birth-internal.h"

void birth_register_allocation_prompt_clicks(int row, cptr prompt,
    int col, cptr back_label, cptr confirm_label, cptr quit_label)
{
    if (!prompt)
        return;

    ui_menu_click_add_text_token(-1, col, row, prompt, back_label);
    ui_menu_click_add_text_token(-1, col, row, prompt, "back");
    ui_menu_click_add_text_token(-2, col, row, prompt, confirm_label);
    ui_menu_click_add_text_token(-2, col, row, prompt, "Enter");
    ui_menu_click_add_text_token(-2, col, row, prompt, "enter");
    ui_menu_click_add_text_token(-2, col, row, prompt, "ok");
    ui_menu_click_add_text_token(-2, col, row, prompt, "confirm");
    ui_menu_click_add_text_token(-2, col, row, prompt, "SPACE/ENTER");
    ui_menu_click_add_text_token(-3, col, row, prompt, quit_label);
    ui_menu_click_add_text_token(-3, col, row, prompt, "quit");
    ui_menu_click_add_text_token(-3, col, row, prompt, "char");
    ui_menu_click_add_text_token(-3, col, row, prompt, "character");
    ui_menu_click_add_text_token(-3, col, row, prompt, "selection");
}

void birth_draw_allocation_confirm_status(int row, int col, int end_col,
    cptr status)
{
    int wid = 80;
    int hgt = 24;
    char status_buf[80];
    cptr back_text = "[Esc]";
    cptr confirm_text = "[Confirm]";
    int back_len = (int)strlen(back_text);
    int confirm_len = (int)strlen(confirm_text);
    int controls_len = back_len + 1 + confirm_len;
    int clear_width;
    int controls_col;
    int confirm_col;
    int status_width;
    int status_copy_len;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    if (row < 0 || row >= hgt)
        return;
    if (col < 0)
        col = 0;
    if (end_col >= wid)
        end_col = wid - 1;
    if (end_col < col)
        return;

    clear_width = end_col - col + 1;
    Term_erase(col, row, clear_width);

    if (clear_width < controls_len)
        return;

    controls_col = end_col - controls_len + 1;
    confirm_col = controls_col;
    status_width = controls_col - col - 1;

    if (status && status[0] && status_width > 0)
    {
        status_copy_len = birth_utf8_prefix_len(status, status_width);
        if (status_copy_len < 0)
            status_copy_len = 0;
        if (status_copy_len >= (int)sizeof(status_buf))
            status_copy_len = (int)sizeof(status_buf) - 1;

        SDL_strlcpy(status_buf, status, sizeof(status_buf));
        if (utf8_display_width_n(status_buf, (int)strlen(status_buf)) > status_width)
        {
            SDL_memcpy(status_buf, status, (size_t)status_copy_len);
            status_buf[status_copy_len] = '\0';
        }
        Term_putstr(col, row, status_width, TERM_L_BLUE, status_buf);
    }

    Term_putstr(controls_col, row, controls_len, TERM_SLATE,
        "[Confirm] [Esc]");
    ui_menu_click_add(-2, confirm_col, row, confirm_len);
    ui_menu_click_add_text_token(-2, confirm_col, row, confirm_text,
        "Confirm");
    {
        int back_col = confirm_col + confirm_len + 1;

        ui_menu_click_add(-1, back_col, row, back_len);
        ui_menu_click_add_text_token(-1, back_col, row, back_text, "Esc");
    }
}

#define BIRTH_ALLOCATION_DEFAULT_SKILL_ROW 6
#define BIRTH_ALLOCATION_MIN_SKILL_ROW 5
#define BIRTH_ALLOCATION_MIN_HISTORY_ROWS 3

static int birth_allocation_skill_rows(void)
{
    int rows = 0;

    for (int skill = 0; skill < S_MAX; skill++)
    {
        if (skill != S_SPC)
            rows++;
    }

    return rows;
}

static int birth_allocation_history_lines(int wid)
{
    int wrap_width;

    if (!p_ptr || !p_ptr->history[0])
        return 0;

    wrap_width = wid - 1;
    if (wrap_width < 10)
        wrap_width = 10;

    return count_wrapped_lines(p_ptr->history, wrap_width, 1);
}

static int birth_allocation_history_room(int history_row, int body_last_row)
{
    if (history_row < 0 || history_row > body_last_row)
        return 0;

    return body_last_row - history_row + 1;
}

void birth_configure_allocation_sheet_layout(bool stats_screen,
    int* skill_first_row_out, int* status_row_out)
{
    int wid = 80;
    int hgt = 24;
    int prompt_row;
    int body_last_row;
    int skill_rows;
    int history_lines;
    int skill_first_row = BIRTH_ALLOCATION_DEFAULT_SKILL_ROW;
    int status_row = -1;
    int history_row = -1;
    int best_skill_first_row = skill_first_row;
    int best_status_row = -1;
    int best_history_row = -1;
    int best_history_room = -1;
    int best_spacing_score = -1;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;
    (void)hgt;

    prompt_row = birth_prompt_row();
    body_last_row = prompt_row - 1;
    skill_rows = birth_allocation_skill_rows();
    history_lines = birth_allocation_history_lines(wid);

    if (stats_screen)
    {
        int stat_last_row = 1 + A_MAX - 1;

        for (int stat_gap = 1; stat_gap >= 0; stat_gap--)
        {
            for (int status_gap = 1; status_gap >= 0; status_gap--)
            {
                for (int history_gap = 1; history_gap >= 0; history_gap--)
                {
                    int candidate_status = stat_last_row + 1 + stat_gap;
                    int candidate_skill = candidate_status + 1 + status_gap;
                    int candidate_history = (history_lines > 0)
                        ? candidate_skill + skill_rows + history_gap : -1;
                    int candidate_room = birth_allocation_history_room(
                        candidate_history, body_last_row);
                    int spacing_score = stat_gap + status_gap + history_gap;

                    if (candidate_skill + skill_rows - 1 > body_last_row)
                        continue;

                    if (history_lines <= 0 || candidate_room >= history_lines)
                    {
                        status_row = candidate_status;
                        skill_first_row = candidate_skill;
                        history_row = candidate_history;
                        goto birth_allocation_layout_done;
                    }

                    if (candidate_room > best_history_room
                        || (candidate_room == best_history_room
                            && spacing_score > best_spacing_score))
                    {
                        best_status_row = candidate_status;
                        best_skill_first_row = candidate_skill;
                        best_history_row = candidate_history;
                        best_history_room = candidate_room;
                        best_spacing_score = spacing_score;
                    }
                }
            }
        }
    }
    else
    {
        int min_skill_first_row = BIRTH_ALLOCATION_MIN_SKILL_ROW;

        for (int candidate = skill_first_row;
             candidate >= min_skill_first_row; candidate--)
        {
            for (int status_gap = 1; status_gap >= 0; status_gap--)
            {
                for (int history_gap = 1; history_gap >= 0; history_gap--)
                {
                    int candidate_status = candidate + skill_rows + status_gap;
                    int candidate_history = (history_lines > 0)
                        ? candidate_status + 1 + history_gap : -1;
                    int candidate_room = birth_allocation_history_room(
                        candidate_history, body_last_row);
                    int spacing_score = (candidate == skill_first_row ? 2 : 1)
                        + status_gap + history_gap;

                    if (candidate_status > body_last_row)
                        continue;

                    if (history_lines <= 0 || candidate_room >= history_lines)
                    {
                        skill_first_row = candidate;
                        status_row = candidate_status;
                        history_row = candidate_history;
                        goto birth_allocation_layout_done;
                    }

                    if (candidate_room > best_history_room
                        || (candidate_room == best_history_room
                            && spacing_score > best_spacing_score))
                    {
                        best_skill_first_row = candidate;
                        best_status_row = candidate_status;
                        best_history_row = candidate_history;
                        best_history_room = candidate_room;
                        best_spacing_score = spacing_score;
                    }
                }
            }
        }
    }

    if (best_status_row >= 0)
    {
        int min_history_rows = MIN(history_lines,
            BIRTH_ALLOCATION_MIN_HISTORY_ROWS);

        skill_first_row = best_skill_first_row;
        status_row = best_status_row;
        history_row = (best_history_room >= min_history_rows)
            ? best_history_row : -1;
    }

birth_allocation_layout_done:
    if (status_row < 0)
    {
        if (stats_screen)
        {
            int stat_last_row = 1 + A_MAX - 1;

            status_row = MIN(stat_last_row + 1, body_last_row);
            skill_first_row = MIN(status_row + 1, body_last_row);
        }
        else
        {
            status_row = MIN(skill_first_row + skill_rows, body_last_row);
        }
        history_row = -1;
    }

    display_player_standard_layout_set(skill_first_row, history_row);

    if (skill_first_row_out)
        *skill_first_row_out = skill_first_row;
    if (status_row_out)
        *status_row_out = status_row;
}

static char birth_screen_char(int row, int col)
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

static bool birth_screen_text_matches(int row, int col, cptr text)
{
    int len;

    if (!text)
        return false;

    len = (int)strlen(text);
    if (len <= 0)
        return false;

    for (int i = 0; i < len; i++)
    {
        if (birth_screen_char(row, col + i) != text[i])
            return false;
    }

    return true;
}

static int birth_screen_find_text(int row, cptr text, int min_col)
{
    int wid = 0;
    int hgt = 0;
    int len;

    if (Term)
        Term_get_size(&wid, &hgt);
    (void)hgt;

    if (!text || !text[0] || wid <= 0)
        return -1;

    len = (int)strlen(text);
    if (len <= 0 || len > wid)
        return -1;
    if (min_col < 0)
        min_col = 0;

    for (int col = min_col; col <= wid - len; col++)
    {
        if (birth_screen_text_matches(row, col, text))
            return col;
    }

    return -1;
}

static void birth_register_visible_stat_clicks(void)
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

    for (int row = 0; row < hgt - 1; row++)
    {
        int attr_col = birth_screen_find_text(row, "Attributes", 0);
        int skills_col;
        int start_col;
        int end_col;

        if (attr_col < 0)
            continue;

        skills_col = birth_screen_find_text(row, "Skills", attr_col + 1);
        start_col = MAX(0, attr_col - 1);
        end_col = (skills_col > attr_col) ? (skills_col - 1) : wid;
        if (end_col <= start_col)
            end_col = wid;

        for (int stat = 0; stat < A_MAX; stat++)
        {
            int stat_row = row + 1 + stat;

            if (stat_row >= hgt - 1)
                break;
            ui_menu_click_add(stat, start_col, stat_row,
                end_col - start_col);
        }

        return;
    }
}

static void birth_display_stats_allocation_compact(const int stats[A_MAX],
    int selected, int points_left, bool steamdeck)
{
    int wid = 80;
    int hgt = 24;
    char buf[160];
    char stat_buf[16];
    char confirm_label[16] = "Enter";
    char back_label[16] = "ESC";
    char quit_label[16] = "q";
    int prompt_row;
    int info_row;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    /* Reuse compact character-sheet stats+skills page with in-place highlighted selection. */
    display_player_compact_stats_skills_highlighted_stat(selected);

    prompt_row = hgt - 1;
    if (prompt_row < 0)
        prompt_row = 0;
    info_row = prompt_row - 1;
    if (info_row < 0)
        info_row = 0;

    Term_erase(0, info_row, 255);
    Term_erase(0, prompt_row, 255);

    if (selected >= 0 && selected < A_MAX)
    {
        int cost = birth_stat_increase_cost(stats[selected]);
        cnv_stat(p_ptr->stat_use[selected], stat_buf);

        strnfmt(buf, sizeof(buf), "Selected: %s %s  Cost: %d  Left: %d",
            stat_names_full[selected], stat_buf, cost, points_left);
        c_put_str(TERM_L_BLUE, buf, info_row, 1);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "Points left: %d", points_left);
        c_put_str(TERM_L_GREEN, buf, info_row, 1);
    }

    if (steamdeck)
    {
        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

        {
            char prompt_full[160];
            char prompt_short[96];
            const char* variants[2];

            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad allocate  %s back  %s ok  %s char",
                back_label, confirm_label, quit_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "%s ok  %s back  %s char", confirm_label, back_label,
                quit_label);
            variants[0] = prompt_full;
            variants[1] = prompt_short;
            terminal_prompt_pick_variant(buf, sizeof(buf), wid - 2,
                story_character_enabled(), variants, N_ELEMENTS(variants));
        }
    }
    else
    {
        const char* variants[] = {
            "Dir select/adjust  Esc back  Enter ok  q char",
            "Dir adjust  Esc back  Enter ok  q char",
            "Enter ok  Esc back  q char"
        };
        terminal_prompt_pick_variant(buf, sizeof(buf), wid - 2,
            story_character_enabled(), variants, N_ELEMENTS(variants));
    }

    c_put_str(TERM_SLATE, buf, prompt_row, 1);
    birth_register_allocation_prompt_clicks(prompt_row, buf, 1,
        back_label, confirm_label, quit_label);
}

void birth_display_skill_allocation_compact(int selected_skill, const int old_base[S_MAX],
    const int skill_gain[S_MAX], int points_left, bool steamdeck)
{
    int wid = 80;
    int hgt = 24;
    char buf[160];
    char confirm_label[16] = "Enter";
    char back_label[16] = "ESC";
    char quit_label[16] = "q";
    int prompt_row;
    int info_row;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    /* Reuse compact character-sheet skills page with in-place highlighted selection. */
    display_player_compact_stats_skills_highlighted(selected_skill);

    prompt_row = hgt - 1;
    if (prompt_row < 0)
        prompt_row = 0;
    info_row = prompt_row - 1;
    if (info_row < 0)
        info_row = 0;

    Term_erase(0, info_row, 255);
    Term_erase(0, prompt_row, 255);

    if (selected_skill >= 0 && selected_skill < S_MAX && selected_skill != S_SPC)
    {
        int base = old_base[selected_skill];
        int gain = skill_gain[selected_skill];
        int now = base + gain;
        int cost = birth_skill_cost(base, gain);

        strnfmt(buf, sizeof(buf), "Selected: %s %2d->%2d  Cost: %d  Left: %d",
            skill_names_full[selected_skill], base, now, cost, points_left);
        c_put_str(TERM_L_BLUE, buf, info_row, 1);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "Points left: %d", points_left);
        c_put_str(TERM_L_GREEN, buf, info_row, 1);
    }

    if (steamdeck)
    {
        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

        {
            char prompt_full[160];
            char prompt_short[96];
            const char* variants[2];

            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad allocate  %s back  %s ok  %s char",
                back_label, confirm_label, quit_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "%s ok  %s back  %s char", confirm_label, back_label,
                quit_label);
            variants[0] = prompt_full;
            variants[1] = prompt_short;
            terminal_prompt_pick_variant(buf, sizeof(buf), wid - 2,
                story_character_enabled(), variants, N_ELEMENTS(variants));
        }
    }
    else
    {
        const char* variants[] = {
            "Dir select/adjust  Esc back  Enter ok  q char",
            "Dir adjust  Esc back  Enter ok  q char",
            "Enter ok  Esc back  q char"
        };
        terminal_prompt_pick_variant(buf, sizeof(buf), wid - 2,
            story_character_enabled(), variants, N_ELEMENTS(variants));
    }

    c_put_str(TERM_SLATE, buf, prompt_row, 1);
    birth_register_allocation_prompt_clicks(prompt_row, buf, 1,
        back_label, confirm_label, quit_label);
}

/*
 * Helper function for 'player_birth()'.
 */
NavResult player_birth_aux_2(int stats[A_MAX])
{
    int i;

    int row = 1;
    int col = 43;

    int stat = 0;

    int cost;
    int stat_costs[A_MAX];

    char ch;

    char buf[80];
    NavResult result = NAV_BACK;

    /* Determine experience and things */
    get_extra();

    /*
     * First-time players are coached inline: the guided callout overlay for the
     * attributes screen is shown once, from within the loop below, after the
     * real stats sheet has been drawn (see birth_coach_show_once).
     */

    log_trace("Starting stats allocation interface");
    screen_push_touch_pane_proto();

    /* Interact */
    while (1)
    {
        bool steamdeck = steamdeck_controls_active();
        int wid = 80;
        int hgt = 24;
        Term_get_size(&wid, &hgt);
        if (wid < 1) wid = 80;
        if (hgt < 1) hgt = 24;
        bool compact = (wid < 80);
        int wide_offset = (wid > 80) ? (wid - 80) / 2 : 0;
        int sheet_col = col + wide_offset;

        /* Reset cost */
        cost = 0;

        /* Process stats */
        for (i = 0; i < A_MAX; i++)
        {
            /* Obtain a "bonus" for "race" */
            int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i] + curses_stat_adj(i);

            /* Apply the racial bonuses */
            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;

            /* Total cost */
            cost += birth_stat_costs[stats[i] + 4];
        }

        /* Restrict cost */
        if (cost > MAX_COST)
        {
            /* Warning */
            bell("Excessive stats!");

            /* Reduce stat */
            stats[stat]--;

            /* Recompute costs */
            continue;
        }

        for (i = 0; i < A_MAX; i++)
            stat_costs[i] = birth_stat_increase_cost(stats[i]);

        p_ptr->new_exp = p_ptr->exp = get_start_xp();

        /* Calculate the bonuses and hitpoints */
        p_ptr->update |= (PU_BONUS | PU_HP);

        /* Update stuff */
        update_stuff();

        /* Fully healed */
        p_ptr->chp = p_ptr->mhp;

        /* Fully rested */
        calc_voice();
        p_ptr->csp = p_ptr->msp;

        if (compact)
        {
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            birth_display_stats_allocation_compact(stats, stat, MAX_COST - cost, steamdeck);
            birth_register_visible_stat_clicks();
            ui_scroll_area_begin(0, hgt - 2, SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('6', '4', '6', '4');
        }
        else
        {
            int prompt_row = birth_prompt_row();
            int status_row = row + A_MAX;
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            ui_scroll_area_begin(row, row + A_MAX - 1,
                SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('6', '4', '6', '4');

            /* Display the player */
            birth_configure_allocation_sheet_layout(true, NULL, &status_row);
            display_player(0);
            display_player_standard_layout_clear();

            /* Display the costs header */
            c_put_str(TERM_WHITE, "Points Left:", 0, sheet_col + 21);
            strnfmt(buf, sizeof(buf), "%2d", MAX_COST - cost);
            c_put_str(TERM_L_GREEN, buf, 0, sheet_col + 34);

            /* Display the costs */
            for (i = 0; i < A_MAX; i++)
            {
                if (i == stat)
                {
                    byte attr = TERM_L_BLUE;

                    /* Match the character sheet label rendering: trim trailing spaces.
                     * (stat_names[] include padding for mono layouts.) */
                    const char* stat_label = (p_ptr->stat_drain[i] < 0) ? stat_names_reduced[i] : stat_names[i];
                    char trimmed_label[32];
                    SDL_strlcpy(trimmed_label, stat_label ? stat_label : "", sizeof(trimmed_label));
                    int len = (int)strlen(trimmed_label);
                    while (len > 0 && trimmed_label[len - 1] == ' ') {
                        trimmed_label[--len] = '\0';
                    }

                    bool use_story = story_character_enabled();
                    if (use_story) {
                        sdl_story_font_enable();
                    }

                    c_put_str(attr, trimmed_label, row + i, sheet_col - 1);

                    if (use_story) {
                        sdl_story_font_disable();
                    }

#ifndef MONOCHROME_MODE
                    strnfmt(buf, sizeof(buf), "%4d", birth_stat_increase_cost(stats[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 32);
#else
                    strnfmt(buf, sizeof(buf), "%4d*", birth_stat_increase_cost(stats[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 32);
                    c_put_str(attr, "*", row + i, sheet_col - 2);
#endif
                }
                else
                {
                    byte attr = TERM_L_WHITE;
                    strnfmt(buf, sizeof(buf), "%4d", birth_stat_increase_cost(stats[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 32);
                }
                ui_menu_click_add(i, sheet_col - 2, row + i, 40);
            }

            if (status_row < prompt_row)
            {
                char stat_buf[80];
                char stat_label[16];
                int stat_label_len;

                SDL_strlcpy(stat_label, stat_names[stat], sizeof(stat_label));
                stat_label_len = (int)strlen(stat_label);
                while (stat_label_len > 0
                    && stat_label[stat_label_len - 1] == ' ')
                {
                    stat_label[--stat_label_len] = '\0';
                }

                strnfmt(stat_buf, sizeof(stat_buf),
                    "%s %d Cost:%d Left:%d",
                    stat_label, p_ptr->stat_use[stat],
                    birth_stat_increase_cost(stats[stat]), MAX_COST - cost);
                birth_draw_allocation_confirm_status(status_row, sheet_col - 1,
                    sheet_col + 37, stat_buf);
            }

            /* Bottom bar follows character sheet font setting */
            if (story_character_enabled()) {
                sdl_story_font_enable();
            }

            if (steamdeck) {
                char confirm_label[16];
                char back_label[16];
                char quit_label[16];
                char prompt_buf[160];

                /* Steam Deck UI: A=confirm, B=back, q=character selection */
                birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
                birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

                {
                    char prompt_full[160];
                    char prompt_short[96];
                    const char* variants[2];

                    strnfmt(prompt_full, sizeof(prompt_full),
                        "D-pad allocate  %s back  %s confirm  %s char",
                        back_label, confirm_label, quit_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "%s confirm  %s back  %s char", confirm_label,
                        back_label, quit_label);
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
                    quit_label);
            } else {
                char prompt_text[160];
                const char* variants[] = {
                    "Dir allocate  Esc back  Enter confirm  q character",
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

        (void)sdl_character_sheet_screen_show_birth_stats(stats, stat_costs,
            stat, MAX_COST - cost);
        (void)Term_set_cursor(false);
        Term_fresh();

        /* First-time players: guided callouts over the real attributes screen. */
        birth_coach_show_once(BIRTH_COACH_STATS);

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
                if (clicked_choice >= 0 && clicked_choice < A_MAX)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != stat)
                    {
                        stat = clicked_choice;
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
        if ((ch == 'Q') || (ch == 'q')) {
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = (turn == 0) ? NAV_BACK : NAV_QUIT;
            goto cleanup;
        }

        /* Back to Character Selection */
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
        {
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = NAV_BACK;
            goto cleanup;
        }

        /* Done */
        if (birth_confirm_input(ch, steamdeck))
        {
            if (!birth_confirm_unspent_stat_points(MAX_COST - cost, steamdeck))
                continue;
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = NAV_OK;
            goto cleanup;
        }

        /* Prev stat */
        if (ch == '8')
        {
            stat = (stat + A_MAX - 1) % A_MAX;
        }

        /* Next stat */
        if (ch == '2')
        {
            stat = (stat + 1) % A_MAX;
        }

        /* Decrease stat */
        if ((ch == '4') && (stats[stat] > 0))
        {
            stats[stat]--;
        }

        /* Increase stat */
        if (ch == '6')
        {
            stats[stat]++;
        }
    }

    /* Shouldn't reach; default to back */
cleanup:
    sdl_character_sheet_screen_hide();
    screen_pop_touch_pane_proto();
    ui_scroll_area_clear();
    return result;
}
