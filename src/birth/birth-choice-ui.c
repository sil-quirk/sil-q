/* File: birth/birth-choice-ui.c */

#include "angband.h"
#include "birth/birth-internal.h"

/*
 * Generic "get choice from menu" function.
 */
int get_player_choice(birth_menu* choices, int num, int def,
    void (*hook)(birth_menu), const birth_select_page* page)
{
    enum {
        BIRTH_CHOICE_CLICK_BACK = -1,
        BIRTH_CHOICE_CLICK_SELECT = -2,
        BIRTH_CHOICE_CLICK_RANDOM = -4
    };
    int next;
    int i, dir;
    char c;
    bool done = false;
    int cur = (def) ? def : 0;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    bool book_mode = (page && page->intro != NULL);  /* race "book" page */
    /* One-shot: jump a returning book to its choice page on the first render
     * only, so the player can still page back to the story afterwards. */
    bool open_choice_page_pending = (page && page->open_on_choice_page);

    /* Autoselect if able */
    // if (num == 1) done = true;

    /* Choose */
    while (true)
    {
        steamdeck = steamdeck_controls_active();

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        /*
         * Pixel-semantic path: drive the SDL selection screen instead of the
         * terminal grid.  With select_intro set this is the book-mode race page
         * (intro text + grouped selectable list + lore, no detail/pop-ups);
         * otherwise it is the character page (list + detail + lore).  The
         * keyboard/click state machine below is shared; only the drawing differs.
         */
        sdl_character_sheet_screen_begin_select(cur,
            (page && page->title) ? page->title : "");
        {
            bool book = (page && page->intro != NULL);
            cptr* group_headings = page ? page->group_headings : NULL;

            if (page)
            {
                sdl_character_sheet_screen_set_select_detail_size_hint(
                    page->detail_stat_rows_hint,
                    page->detail_ability_rows_hint,
                    page->detail_trait_rows_hint);
            }

            if (book)
            {
                sdl_character_sheet_screen_set_select_intro(page->intro);
                sdl_character_sheet_screen_set_select_frame(page->frame_top,
                    page->frame_bottom);
                if (run_mode_is_blitz())
                    sdl_character_sheet_screen_show_select_choice_page_only();
                else if (open_choice_page_pending)
                    sdl_character_sheet_screen_open_select_choice_page();
            }
            open_choice_page_pending = false;

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
                sdl_character_sheet_screen_set_last_select_row_confirmable(
                    !choices[i].ghost);
                /*
                 * Feed every possible description into the font fitter.  Both
                 * book lore and character sheets must remain stable as focus
                 * moves, while reserving enough lines for the worst wrap.
                 */
                if (book)
                    sdl_character_sheet_screen_add_select_description_candidate(
                        choices[i].text ? choices[i].text : "");
                else
                    sdl_character_sheet_screen_add_select_welcome(
                        choices[i].text ? choices[i].text : "");
            }
            /* Book mode has no detail panel.  Avoid building the selected
             * race's unused stat/trait rows on every highlight change. */
            if (hook && !book)
                hook(choices[cur]);
            sdl_character_sheet_screen_set_select_description(
                choices[cur].text ? choices[cur].text : "");

            /* Size from the longest text among the choices passed by the
             * current race.  This keeps that race's character sheets aligned
             * without coupling separate Noldorin houses. */
            if (!book)
            {
                cptr longest = "";
                cptr longest_first = "";
                cptr longest_body = "";
                size_t longest_len = 0;
                size_t longest_first_len = 0;
                size_t longest_body_len = 0;
                bool split_hint = false;
                char sizing_desc[4096];
                int j;

                sizing_desc[0] = '\0';
                for (j = 0; j < num; j++)
                {
                    cptr t = choices[j].text ? choices[j].text : "";
                    cptr sep = strstr(t, "\n\n");
                    size_t l = strlen(t);

                    if (l > longest_len)
                    {
                        longest_len = l;
                        longest = t;
                    }
                    if (sep)
                    {
                        cptr body = sep;
                        size_t first_len = (size_t)(sep - t);
                        size_t body_len;

                        while (first_len > 0
                            && isspace((unsigned char)t[first_len - 1]))
                        {
                            first_len--;
                        }
                        while (*body && isspace((unsigned char)*body))
                            body++;
                        body_len = strlen(body);
                        if (first_len > 0 && body_len > 0)
                        {
                            split_hint = true;
                            if (first_len > longest_first_len)
                            {
                                longest_first_len = first_len;
                                longest_first = t;
                            }
                            if (body_len > longest_body_len)
                            {
                                longest_body_len = body_len;
                                longest_body = body;
                            }
                        }
                    }
                }
                if (split_hint)
                {
                    strnfmt(sizing_desc, sizeof(sizing_desc), "%.*s\n\n%s",
                        (int)MIN(longest_first_len, (size_t)2048),
                        longest_first, longest_body);
                    sdl_character_sheet_screen_set_select_size_hint(
                        sizing_desc);
                }
                else
                {
                    sdl_character_sheet_screen_set_select_size_hint(longest);
                }
            }
            sdl_character_sheet_screen_commit_select(cur);
        }

        if (done)
        {
            ui_menu_click_clear();
            return (cur);
        }

        /*
         * First-time players: guided callouts over the real selection screen.
         * Shown on the hero list (which carries descriptions, traits and power
         * ratings) rather than the race "book" page.
         */
        if (!book_mode)
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
                if (clicked_choice == SDL_SELECT_CLICK_PAGE_NEXT
                    || clicked_choice == SDL_SELECT_CLICK_PAGE_PREV
                    || clicked_choice == SDL_SELECT_CLICK_CLOSE)
                {
                    /* The centred race-book shortcut skips directly to the
                     * selectable final page. */
                    if (book_mode && click_action != UI_MENU_CLICK_HOVER
                        && !sdl_character_sheet_screen_page_turning())
                    {
                        int bpage =
                            sdl_character_sheet_screen_select_page();
                        int pcount =
                            sdl_character_sheet_screen_select_page_count();
                        int final_page = pcount - 1;

                        if (final_page < 0)
                            final_page = 0;

                        if (clicked_choice == SDL_SELECT_CLICK_CLOSE)
                        {
                            if (bpage < final_page)
                            {
                                sdl_character_sheet_screen_begin_page_turn_to(
                                    final_page);
                            }
                        }
                        else if (clicked_choice == SDL_SELECT_CLICK_PAGE_PREV)
                        {
                            if (bpage <= 0)
                            {
                                c = ESCAPE;
                                click_generated_command = true;
                            }
                            else
                            {
                                sdl_character_sheet_screen_begin_page_turn(-1);
                            }
                        }
                        else if (bpage < final_page)
                        {
                            sdl_character_sheet_screen_begin_page_turn(+1);
                        }
                    }
                    if (!click_generated_command)
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
                        }
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
                case BIRTH_CHOICE_CLICK_RANDOM: c = 'r'; click_generated_command = true; break;
                default: break;
                }
            }
            else if (c == UI_MENU_CLICK_WAKE_KEY)
            {
                sdl_hover_tooltip_clear();
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
                cur = ncur;
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
        // Show the Halls of Mandos: accept both 's' and 'S'
        if (c == 's' || c == 'S')
        {
            show_scores_interactive();
            continue; /* Return to the selection loop after showing scores */
        }
        
        // Show help: accept both 'h' and 'H', plus '?'
        if (c == 'h' || c == 'H' || c == '?')
        {
            do_cmd_help();
            continue; /* Return to the selection loop after showing help */
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

            }
        }

        /* Invalid input */
        else
            bell("Illegal response to question!");

    }

    ui_menu_click_clear();
    return (INVALID_CHOICE);
}

