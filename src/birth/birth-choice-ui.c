/* File: birth/birth-choice-ui.c */

#include "angband.h"
#include "birth/birth-internal.h"

#define BIRTH_DETAIL_HOVER_CLICK_BASE 16000
#define BIRTH_DETAIL_HOVER_MAX 96

typedef struct {
    int col;
    int row;
    int width;
    char desc[640];
} birth_detail_hover_target;

static birth_detail_hover_target birth_detail_hover_targets[BIRTH_DETAIL_HOVER_MAX];
static int birth_detail_hover_target_count = 0;

static void birth_detail_hover_reset(void)
{
    birth_detail_hover_target_count = 0;
}

static bool birth_detail_hover_is_choice(int choice)
{
    int idx = choice - BIRTH_DETAIL_HOVER_CLICK_BASE;

    return idx >= 0 && idx < birth_detail_hover_target_count;
}

void birth_detail_hover_add(int col, int row, int width, cptr desc)
{
    birth_detail_hover_target* target;

    if (!desc || !desc[0])
        return;
    if (birth_detail_hover_target_count >= BIRTH_DETAIL_HOVER_MAX)
        return;
    if (width <= 0)
        return;

    target = &birth_detail_hover_targets[birth_detail_hover_target_count];
    target->col = col;
    target->row = row;
    target->width = width;
    SDL_strlcpy(target->desc, desc, sizeof(target->desc));

    ui_menu_click_add(BIRTH_DETAIL_HOVER_CLICK_BASE
            + birth_detail_hover_target_count,
        col, row, width);
    birth_detail_hover_target_count++;
}

static bool birth_detail_hover_show(int choice, bool touch)
{
    int idx = choice - BIRTH_DETAIL_HOVER_CLICK_BASE;
    birth_detail_hover_target* target;

    if (idx < 0 || idx >= birth_detail_hover_target_count)
        return false;

    target = &birth_detail_hover_targets[idx];
    return sdl_hover_tooltip_show_text(target->col, target->row,
        target->width, target->desc, touch);
}

static void display_character_description_screen(birth_menu choice)
{
    int wid = 80;
    int hgt = 24;
    int character_idx;
    char full_name[64];
    int name_col = 2;
    int text_top_row = 2;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;
    if (hgt <= 18)
        text_top_row = 1;

    character_idx = character_choice_index_by_name(choice.name);
    if (character_idx >= 0)
    {
        strnfmt(full_name, sizeof(full_name), "%s%s",
            c_name + c_info[character_idx].name,
            c_name + c_info[character_idx].alt_name);
    }
    else
    {
        strnfmt(full_name, sizeof(full_name), "%s", choice.name ? choice.name : "");
    }

    {
        int full_name_width = utf8_display_width_n(full_name, (int)strlen(full_name));

        if (full_name_width < wid)
            name_col = (wid - full_name_width) / 2;
    }
    if (name_col < 2)
        name_col = 2;

    screen_save();
    Term_clear();

    Term_putstr(name_col, 0, -1, TERM_L_BLUE, full_name);

    if (choice.text && choice.text[0] && (hgt > text_top_row))
    {
        int text_row = text_top_row;
        int text_rows = birth_wrapped_line_count(choice.text, 2);
        int available_rows = hgt - text_top_row - 1;

        if ((hgt > 18) && (text_rows < available_rows))
            text_row = text_top_row + ((available_rows - text_rows) / 2);
        if (text_row < text_top_row)
            text_row = text_top_row;
        if (text_row > hgt - 2)
            text_row = hgt - 2;

        birth_put_wrapped_text(TERM_WHITE, choice.text, text_row, 2);
    }

    if (hgt > 0)
    {
        if (steamdeck_controls_active())
        {
            char back_label[16];
            char prompt_buf[48];

            birth_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] return", back_label);
            birth_put_str_fit(TERM_SLATE, prompt_buf, hgt - 1, 2);
        }
        else
        {
            birth_put_str_fit(TERM_SLATE,
                "Press any key to return", hgt - 1, 2);
        }
    }

    Term_fresh();
    ui_menu_click_begin();
    ui_menu_click_add_full_row('\r', hgt - 1);
    (void)inkey();
    ui_menu_click_clear();

    screen_load();
}

static byte birth_selected_attr(void)
{
    return (byte)(TERM_UI_SELECTED + TERM_L_BLUE);
}

static void birth_fill_selected_row(int col, int row, int width, byte attr)
{
    char fill[160];
    int term_wid = Term ? Term->wid : 80;
    int term_hgt = Term ? Term->hgt : 24;

    if (row < 0 || row >= term_hgt || width <= 0)
        return;
    if (col < 0)
    {
        width += col;
        col = 0;
    }
    if (col >= term_wid || width <= 0)
        return;
    if (col + width > term_wid)
        width = term_wid - col;
    if (width >= (int)sizeof(fill))
        width = (int)sizeof(fill) - 1;

    memset(fill, ' ', (size_t)width);
    fill[width] = '\0';
    Term_putstr(col, row, width, attr, fill);
}

/*
 * Generic "get choice from menu" function
 */
int get_player_choice(birth_menu* choices, int num, int def, int col,
    int wid, void (*hook)(birth_menu), bool allow_full_description_screen,
    const birth_select_page* page)
{
    enum {
        BIRTH_CHOICE_CLICK_BACK = -1,
        BIRTH_CHOICE_CLICK_SELECT = -2,
        BIRTH_CHOICE_CLICK_DETAILS = -3,
        BIRTH_CHOICE_CLICK_RANDOM = -4,
        BIRTH_CHOICE_CLICK_RIGHT_BACK = -5
    };
    int top = 0, next;
    int i, dir;
    char c;
    bool done = false;
    bool show_description;
    bool compact_flags;
    bool sdl_select = false;
    int prompt_row;
    int hgt;
    byte attr;
    char prompt[160];
    int cur = (def) ? def : 0;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    bool book_mode = (page && page->intro != NULL);  /* race "book" page */
    int clear_limit = birth_prompt_row() + 1;
    int last_list_rows_drawn = 0;
    int last_description_row = birth_prompt_row();

    /* Autoselect if able */
    // if (num == 1) done = true;

    /* Clear */
    if (clear_limit < TABLE_ROW)
        clear_limit = TABLE_ROW;
    for (i = TABLE_ROW; i < clear_limit; i++)
    {
        /* Clear */
        Term_erase(col, i, 255/* Term->wid - wid */);
    }

    /* Choose */
    while (true)
    {
        int description_row = birth_description_base_row();
        int list_rows_drawn;
        int visible_capacity = choice_visible_capacity(num, choices[cur].text,
            allow_full_description_screen);

        steamdeck = steamdeck_controls_active();

        hgt = visible_capacity - 1;
        if (hgt < 0)
            hgt = 0;

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        birth_detail_hover_reset();

        /*
         * Pixel-semantic path: drive the SDL selection screen instead of the
         * terminal grid.  With select_intro set this is the book-mode race page
         * (intro text + grouped selectable list + lore, no detail/pop-ups);
         * otherwise it is the character page (list + detail + lore).  The
         * keyboard/click state machine below is shared; only the drawing differs.
         */
        sdl_select = sdl_character_sheet_screen_begin_select(cur,
            (page && page->title) ? page->title : "");
        if (sdl_select)
        {
            bool book = (page && page->intro != NULL);
            cptr* group_headings = page ? page->group_headings : NULL;

            if (page)
            {
                sdl_character_sheet_screen_set_select_detail_size_hint(
                    page->detail_stat_rows_hint,
                    page->detail_trait_rows_hint);
            }

            if (book)
            {
                sdl_character_sheet_screen_set_select_intro(page->intro);
                sdl_character_sheet_screen_set_select_frame(page->frame_top,
                    page->frame_bottom);
                if (run_mode_is_blitz())
                    sdl_character_sheet_screen_show_select_choice_page_only();
            }

            for (i = 0; i < num; i++)
            {
                byte rattr = choices[i].ghost ? TERM_SLATE : TERM_WHITE;
                char label[256];

                if (book && group_headings && group_headings[i])
                    sdl_character_sheet_screen_add_select_heading(
                        group_headings[i]);

                if (choices[i].ghost)
                    strnfmt(label, sizeof(label), "%s %s",
                        BIRTH_FALLEN_MARK, choices[i].name);
                else
                    strnfmt(label, sizeof(label), "%s", choices[i].name);
                /* No per-row hover tooltip: the focused choice's text is
                 * already shown in the description area below. */
                sdl_character_sheet_screen_add_select_row(i, label, rattr, "");
            }
            if (hook)
                hook(choices[cur]);
            sdl_character_sheet_screen_set_select_description(
                choices[cur].text ? choices[cur].text : "");

            /* Size the description area for the LONGEST choice text so the
             * layout does not reflow as the highlight moves between choices. */
            {
                cptr longest = "";
                size_t longest_len = 0;
                int j;

                for (j = 0; j < num; j++)
                {
                    cptr t = choices[j].text ? choices[j].text : "";
                    size_t l = strlen(t);

                    if (l > longest_len)
                    {
                        longest_len = l;
                        longest = t;
                    }
                }
                sdl_character_sheet_screen_set_select_size_hint(longest);
            }
            sdl_character_sheet_screen_commit_select(cur);
        }

      if (!sdl_select)
      {
        /* Redraw the list */
        for (i = 0; ((i + top < num) && (i <= hgt)); i++)
        {
            bool selected = (i == (cur - top));

            /* Clear */
            Term_erase(col, i + TABLE_ROW, wid);

            /* Display name part */
            if (selected)
            {
                /* Highlight the current selection */
                if (allow_full_description_screen)
                    attr = birth_selected_attr();
                else if (choices[i + top].ghost)
                    attr = TERM_BLUE;
                else
                    attr = TERM_L_BLUE;
            }
            else
            {
                if (choices[i + top].ghost)
                    attr = TERM_SLATE;
                else
                    attr = TERM_WHITE;
            }

            /* Display character name */
            char name_part[256];
            if (choices[i + top].ghost)
                strnfmt(name_part, sizeof(name_part), "%s %s",
                    BIRTH_FALLEN_MARK, choices[i + top].name);
            else 
                strnfmt(name_part, sizeof(name_part), "%s", choices[i + top].name);

            if (selected && allow_full_description_screen)
                birth_fill_selected_row(col, i + TABLE_ROW, wid, attr);
            Term_putstr(col, i + TABLE_ROW, wid, attr, name_part);
            ui_menu_click_add(i + top, col, i + TABLE_ROW, wid);
        }

        list_rows_drawn = i;

        /*
         * When a newly selected entry needs more description space on a short
         * screen, the visible list can shrink by one or more rows. Clear any
         * rows that belonged to the previous, taller list so stale entries do
         * not remain between the list and the description text.
         */
        for (i = list_rows_drawn; i < last_list_rows_drawn; i++)
            Term_erase(0, TABLE_ROW + i, 255);

        if (!allow_full_description_screen)
        {
            description_row = choice_description_row(list_rows_drawn, false);
            description_row = choice_description_fit_row(description_row,
                list_rows_drawn, choices[cur].text);
        }

        {
            int clear_from_row = description_row;

            if (last_description_row < clear_from_row)
                clear_from_row = last_description_row;

            if (allow_full_description_screen)
            {
                bool compact_flags = character_flags_need_compact_layout();
                bool very_short = (Term->hgt > 0) && (Term->hgt <= 20);
                if (compact_flags && very_short)
                    clear_from_row = description_row - 1;

                /*
                 * On compact screens, the previous race-selection phase can
                 * leave wrapped description text a few rows above the compact
                 * character traits area. Clear from the earlier race
                 * description start as well so stale fragments do not remain
                 * when switching phases.
                 */
                if (compact_flags)
                {
                    int compact_min_clear = TABLE_ROW + list_rows_drawn + 1;
                    int race_visible_rows = choice_visible_capacity(
                        z_info->p_max, p_text + p_info[p_ptr->prace].text, false);
                    int race_description_row = choice_description_row(
                        race_visible_rows, false);
                    race_description_row = choice_description_fit_row(
                        race_description_row, race_visible_rows,
                        p_text + p_info[p_ptr->prace].text);
                    if (compact_min_clear < clear_from_row)
                        clear_from_row = compact_min_clear;
                    if (race_description_row < clear_from_row)
                        clear_from_row = race_description_row;
                }
            }
            for (i = clear_from_row; i < Term->hgt; i++)
                Term_erase(0, i, 255);
        }

        compact_flags = (allow_full_description_screen && character_flags_need_compact_layout());
        show_description = (!compact_flags);

        if (allow_full_description_screen)
            show_description = (show_description && character_description_has_room());

        /* Display auxiliary information before the description so the
         * description text remains the topmost layer on short screens. */
        if (hook)
            hook(choices[cur]);

        if (show_description && choices[cur].text != NULL)
        {
            birth_put_wrapped_text(TERM_WHITE, choices[cur].text,
                description_row, 2);
        }

        last_list_rows_drawn = list_rows_drawn;
        last_description_row = description_row;
      }  /* if (!sdl_select) -- terminal list/description drawing */

        if (done)
        {
            ui_menu_click_clear();
            return (cur);
        }

        if (!sdl_select && Term->hgt > 0)
        {
            prompt_row = birth_prompt_row();
            Term_erase(0, prompt_row, 255);

            if (steamdeck)
            {
                char confirm_label[16];
                char detail_label[16];
                char back_label[16];
                char random_label[16];

                birth_prompt_label(steamdeck_confirm_key(), "A",
                    confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_alt_action_key(), "X",
                    detail_label, sizeof(detail_label));
                birth_prompt_label(steamdeck_back_key(), "B",
                    back_label, sizeof(back_label));
                birth_prompt_label('r', "r", random_label,
                    sizeof(random_label));

                if (allow_full_description_screen)
                {
                    char prompt_full[160];
                    char prompt_short[96];
                    const char* variants[2];

                    strnfmt(prompt_full, sizeof(prompt_full),
                        "D-pad move  %s select  %s details  %s back  %s random",
                        confirm_label, detail_label, back_label, random_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "%s select  %s details  %s back", confirm_label,
                        detail_label, back_label);
                    variants[0] = prompt_full;
                    variants[1] = prompt_short;
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        Term ? Term->wid - QUESTION_COL : 80 - QUESTION_COL,
                        false, variants, N_ELEMENTS(variants));
                }
                else
                {
                    char prompt_full[160];
                    char prompt_short[96];
                    const char* variants[2];

                    strnfmt(prompt_full, sizeof(prompt_full),
                        "D-pad move  %s select  %s back  %s random",
                        confirm_label, back_label, random_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "%s select  %s back", confirm_label, back_label);
                    variants[0] = prompt_full;
                    variants[1] = prompt_short;
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        Term ? Term->wid - QUESTION_COL : 80 - QUESTION_COL,
                        false, variants, N_ELEMENTS(variants));
                }
            }
            else if (sdl_touch_only_device_active())
            {
                /* Keep the secondary action words (details, back, random)
                 * present so the tappable tokens registered below still
                 * resolve on touch; the choices themselves are tappable rows. */
                if (allow_full_description_screen)
                {
                    const char* tvar[] = {
                        "Tap a row to select  details  back  random",
                        "Tap a row to select  details  back",
                        "Tap a row to select"
                    };
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        Term ? Term->wid - QUESTION_COL : 80 - QUESTION_COL,
                        false, tvar, N_ELEMENTS(tvar));
                }
                else
                {
                    const char* tvar[] = {
                        "Tap a row to select  back  random",
                        "Tap a row to select  back",
                        "Tap a row to select"
                    };
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        Term ? Term->wid - QUESTION_COL : 80 - QUESTION_COL,
                        false, tvar, N_ELEMENTS(tvar));
                }
            }
            else if (allow_full_description_screen)
            {
                const char* variants[] = {
                    "Enter select  f description  Esc back  r random",
                    "Enter select  f details  Esc back",
                    "Enter select  Esc back"
                };
                terminal_prompt_pick_variant(prompt, sizeof(prompt),
                    Term ? Term->wid - QUESTION_COL : 80 - QUESTION_COL,
                    false, variants, N_ELEMENTS(variants));
            }
            else
            {
                const char* variants[] = {
                    "Enter select  Esc back  r random",
                    "Enter select  Esc back"
                };
                terminal_prompt_pick_variant(prompt, sizeof(prompt),
                    Term ? Term->wid - QUESTION_COL : 80 - QUESTION_COL,
                    false, variants, N_ELEMENTS(variants));
            }
            Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt);
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_SELECT,
                QUESTION_COL, prompt_row, prompt, "select");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_DETAILS,
                QUESTION_COL, prompt_row, prompt, "description");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_DETAILS,
                QUESTION_COL, prompt_row, prompt, "details");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_BACK,
                QUESTION_COL, prompt_row, prompt, "back");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_BACK,
                QUESTION_COL, prompt_row, prompt, "ESC");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_RANDOM,
                QUESTION_COL, prompt_row, prompt, "random");
        }

        if (!sdl_select && allow_full_description_screen)
        {
            int term_wid = 80;
            int term_hgt = 24;

            Term_get_size(&term_wid, &term_hgt);
            if (term_hgt < 1)
                term_hgt = 24;

            (void)term_wid;
            for (i = 0; i < term_hgt; i++)
                ui_menu_click_add_full_row(BIRTH_CHOICE_CLICK_RIGHT_BACK, i);
        }

        /* Move the cursor */
        if (!sdl_select)
            put_str("", TABLE_ROW + cur - top, col);

        /*
         * First-time players: guided callouts over the real selection screen.
         * Shown on the hero list (which carries descriptions, traits and power
         * ratings) rather than the race "book" page.
         */
        if (sdl_select && !book_mode)
            birth_coach_show_once(BIRTH_COACH_SELECT);

        hide_cursor = true;
        c = inkey();
        hide_cursor = false;
        bool click_generated_command = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (birth_detail_hover_is_choice(clicked_choice))
                {
                    birth_detail_hover_show(clicked_choice,
                        click_action != UI_MENU_CLICK_HOVER);
                    continue;
                }
                else if (clicked_choice == SDL_SELECT_CLICK_PAGE_NEXT
                    || clicked_choice == SDL_SELECT_CLICK_PAGE_PREV)
                {
                    /* Book page-turn buttons (race screen, mouse). */
                    if (book_mode && click_action != UI_MENU_CLICK_HOVER
                        && !sdl_character_sheet_screen_page_turning())
                    {
                        sdl_character_sheet_screen_begin_page_turn(
                            clicked_choice == SDL_SELECT_CLICK_PAGE_NEXT
                                ? +1 : -1);
                    }
                    continue;
                }
                else if (clicked_choice == SDL_SELECT_CLICK_CAROUSEL_PREV
                    || clicked_choice == SDL_SELECT_CLICK_CAROUSEL_NEXT)
                {
                    /* Mobile hero carousel triangles: step to the adjacent
                     * hero (a tap, not a hover). */
                    if (click_action != UI_MENU_CLICK_HOVER)
                    {
                        int ncur = cur
                            + ((clicked_choice == SDL_SELECT_CLICK_CAROUSEL_NEXT)
                                ? 1 : -1);

                        if (ncur >= 0 && ncur < num)
                        {
                            sdl_hover_tooltip_clear();
                            cur = ncur;
                            if (cur < top || cur > top + hgt)
                                top = cur;
                        }
                    }
                    continue;
                }
                else if (click_action == UI_MENU_CLICK_SECONDARY
                    && allow_full_description_screen)
                {
                    c = ESCAPE;
                    click_generated_command = true;
                    clicked_choice = BIRTH_CHOICE_CLICK_BACK;
                    click_action = UI_MENU_CLICK_PRIMARY;
                }
                else if (clicked_choice == BIRTH_CHOICE_CLICK_RIGHT_BACK)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        sdl_hover_tooltip_clear();
                        continue;
                    }
                    continue;
                }
                else if (clicked_choice >= 0 && clicked_choice < num)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != cur)
                    {
                        sdl_hover_tooltip_clear();
                        cur = clicked_choice;
                        if (cur < top || cur > top + hgt)
                            top = cur;
                        continue;
                    }

                    if (choices[cur].ghost)
                        bell("Your race cannot choose that character.");
                    else
                        return (cur);
                    continue;
                }

                if (click_action == UI_MENU_CLICK_HOVER)
                {
                    sdl_hover_tooltip_clear();
                    continue;
                }

                switch (clicked_choice)
                {
                case BIRTH_CHOICE_CLICK_BACK: c = ESCAPE; click_generated_command = true; break;
                case BIRTH_CHOICE_CLICK_SELECT: c = '\r'; click_generated_command = true; break;
                case BIRTH_CHOICE_CLICK_DETAILS: c = 'f'; click_generated_command = true; break;
                case BIRTH_CHOICE_CLICK_RANDOM: c = 'r'; click_generated_command = true; break;
                case BIRTH_CHOICE_CLICK_RIGHT_BACK: break;
                default: break;
                }
            }
            else if (c == UI_MENU_CLICK_WAKE_KEY)
            {
                int hover_choice = 0;

                if (!ui_menu_click_get_hover_choice(&hover_choice)
                    || !birth_detail_hover_is_choice(hover_choice))
                {
                    sdl_hover_tooltip_clear();
                }
                continue;
            }
            else
            {
                sdl_hover_tooltip_clear();
            }
        }

        if (!click_generated_command)
            c = (char)steamdeck_menu_key(c, '4', '6');

        /* Exit the game */
        if ((c == 'Q') || (c == 'q'))
            quit(NULL);

        /*
         * Race "book" navigation.  Any page before the final choice page turns
         * forward on Right/Space/Enter; the final page accepts Enter as the
         * selected race.  Left turns back from later pages, while Left on the
         * first page still backs out of the screen entirely.  Arrow keys reach
         * us as '4'/'6'.
         */
        if (book_mode && !run_mode_is_blitz())
        {
            int bpage = sdl_character_sheet_screen_select_page();
            int pcount = sdl_character_sheet_screen_select_page_count();
            int final_page = pcount - 1;

            /* Ignore keys while a page-curl is mid-flight. */
            if (sdl_character_sheet_screen_page_turning())
                continue;

            if (final_page < 0)
                final_page = 0;

            if (bpage > 0 && c == '4')
            {
                sdl_character_sheet_screen_begin_page_turn(-1);
                continue;
            }

            if (bpage < final_page)
            {
                if (c == '6' || birth_confirm_input(c, steamdeck))
                {
                    sdl_character_sheet_screen_begin_page_turn(+1);
                    continue;
                }
                /* No list on story pages; swallow vertical movement. */
                if (c == '8' || c == '2')
                    continue;
            }
            else
            {
                /* No page beyond the choice page: Right is a no-op. */
                if (c == '6')
                    continue;
            }
        }

        /*
         * Mobile hero carousel: Left/Right (also fed by swipes and the on-screen
         * triangles) step between heroes instead of meaning back/confirm.  Esc
         * still backs out and Enter/tap-name still confirms.
         */
        if (sdl_character_sheet_screen_mobile_carousel_active()
            && (c == '4' || c == '6'))
        {
            int ncur = cur + ((c == '6') ? 1 : -1);

            if (ncur >= 0 && ncur < num)
            {
                cur = ncur;
                if (cur < top || cur > top + hgt)
                    top = cur;
            }
            continue;
        }

        /* Hack - go back */
        if ((c == ESCAPE) || (c == '4')
            || (steamdeck && c == steamdeck_back_key()))
        {
            ui_menu_click_clear();
            return (INVALID_CHOICE);
        }

        /* Make a choice */
        if (birth_confirm_input(c, steamdeck) || (c == '6')) {
            if (choices[cur].ghost)
                bell("Your race cannot choose that character.");
            else
            {
                ui_menu_click_clear();
                return (cur);
            }
        }
        // Show scores (short): accept both 's' and 'S'
        if (c == 's' || c == 'S')
        {
            show_scores_interactive(false);
            continue; /* Return to the selection loop after showing scores */
        }
        
        // Show help: accept both 'h' and 'H', plus '?'
        if (c == 'h' || c == 'H' || c == '?')
        {
            do_cmd_help();
            continue; /* Return to the selection loop after showing help */
        }

        if (allow_full_description_screen
            && (c == 'f' || c == 'F'
                || (steamdeck && c == steamdeck_alt_action_key())))
        {
            display_character_description_screen(choices[cur]);
            continue;
        }

        /* Random choice */
        if (c == 'r')
        {
            /* Ensure legal choice */
            do
            {
                cur = rand_int(num);
            } while (choices[cur].ghost);

            /* Done */
            done = true;
        }

        /* Alphabetic choice */
    else if (menu_letters && isalpha((unsigned char)c))
        {
            /* Options */
            if ((c == 'O') || (c == 'o'))
            {
                do_cmd_options();
            }

            else
            {
                int choice;

                if (islower(c))
                    choice = A2I(c);
                else
                    choice = c - 'A' + 26;

        /* Validate input */
        if ((choice > -1) && (choice < num) && !(choices[choice].ghost))
                {
                    cur = choice;

                    /* Done */
                    done = true;
                }
        else if ((choice > -1) && (choice < num) && choices[choice].ghost)
                {
                    bell("Your race cannot choose that character.");
                }
                else
                {
                    bell("Illegal response to question!");
                }
            }
        }

        /* Move */
        else if (isdigit(c))
        {
            /* Get a direction from the key */
            dir = target_dir(c);

            /* Going up? */
            if (dir == 8)
            {
                next = -1;
                for (i = 0; i < cur; i++)
                {
                    // if (!(choices[i].ghost))
                    // {
                        next = i;
                    // }
                }

                /* Move selection */
                if (next != -1)
                    cur = next;
                /* if (cur != 0) cur--; */

                /* Scroll up */
                if ((top > 0) && ((cur - top) < 4))
                    top--;
            }

            /* Going down? */
            if (dir == 2)
            {
                next = -1;
                for (i = num - 1; i > cur; i--)
                {
                    // if (!(choices[i].ghost))
                    //
                        next = i;
                    // }
                }

                /* Move selection */
                if (next != -1)
                    cur = next;
                /* if (cur != (num - 1)) cur++; */

                /* Scroll down */
                if ((top + hgt < (num - 1)) && ((top + hgt - cur) < 4))
                    top++;
            }
        }

        /* Invalid input */
        else
            bell("Illegal response to question!");

        /* If choice is off screen, move it to the top */
        if ((cur < top) || (cur > top + hgt))
            top = cur;
    }

    ui_menu_click_clear();
    return (INVALID_CHOICE);
}

