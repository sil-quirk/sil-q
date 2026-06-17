#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "quest/quest-internal.h"

static int quest_typewriter_text_start_row(int hgt)
{
    if (hgt <= 3)
        return MAX(0, hgt - 2);

    return 3;
}

static int quest_typewriter_last_text_row(int hgt)
{
    int start_row = quest_typewriter_text_start_row(hgt);
    int last_row = hgt - 2;

    if (last_row < start_row)
        last_row = start_row;

    return last_row;
}

static void quest_typewriter_draw_title(cptr title, byte title_color, int wid)
{
    int title_col = MAX(0, (wid - (int)strlen(title)) / 2);

    Term_putstr(title_col, 1, -1, title_color, title);
}

static bool quest_typewriter_next_page(cptr title, byte title_color, int wid,
    int hgt, int *row, int *col)
{
    char prompt[48];
    int prompt_row = MAX(0, hgt - 1);
    int prompt_col;
    char k;

    any_key_prompt_text(prompt, sizeof(prompt), "continue");
    prompt_col = MAX(0, (wid - (int)strlen(prompt)) / 2);
    Term_erase(0, prompt_row, 255);
    Term_putstr(prompt_col, prompt_row, -1, TERM_L_WHITE, prompt);
    Term_fresh();

    k = inkey();
    if (k == 'Q' || k == 'q')
        return false;

    Term_clear();
    quest_typewriter_draw_title(title, title_color, wid);
    *row = quest_typewriter_text_start_row(hgt);
    *col = 0;
    return true;
}

static bool quest_typewriter_ensure_row(cptr title, byte title_color, int wid,
    int hgt, int *row, int *col)
{
    if (*row <= quest_typewriter_last_text_row(hgt))
        return true;

    return quest_typewriter_next_page(title, title_color, wid, hgt, row, col);
}

/*
 * Show quest narrative as a parchment "book" with page-turn navigation, reusing
 * the SDL front-end's character-sheet book.  The incoming texts[] are the line
 * entries from extract_quest_*_texts(): consecutive non-empty lines form one
 * paragraph and an empty entry is a paragraph break.  Each paragraph is re-flowed
 * onto the page (terminal line breaks are dropped), then the book paginates.
 *
 * Returns false when the book cannot be shown (no SDL screen), so the caller can
 * fall back to the terminal typewriter below.
 */
static bool quest_show_book(cptr title, cptr texts[], int total_texts)
{
    char para[1024];
    size_t para_len = 0;
    int idx;
    bool done = false;

    screen_save();
    screen_push_supporting_panes_hidden();

    if (!sdl_character_sheet_screen_begin_book(title))
    {
        screen_pop_supporting_panes_hidden();
        screen_load();
        return false;
    }

    /* Build paragraphs from the line entries and push them into the book.  The
     * extra (idx == total_texts) pass flushes the final paragraph. */
    para[0] = '\0';
    for (idx = 0; idx <= total_texts; idx++)
    {
        cptr line = (idx < total_texts) ? texts[idx] : NULL;

        /* A "[newpage]" line flushes the current paragraph and forces the next
         * one onto a fresh page, so an author can lay out a logical page turn. */
        if (line && streq(line, "[newpage]"))
        {
            if (para_len > 0)
                sdl_character_sheet_screen_add_book_paragraph(para);
            para[0] = '\0';
            para_len = 0;
            sdl_character_sheet_screen_break_book_page();
        }
        /* A "[highlight]" line flushes the current paragraph and marks the next
         * one as the quest's task or reward, drawn in light blue so the player
         * can find it without reading the whole passage. */
        else if (line && streq(line, "[highlight]"))
        {
            if (para_len > 0)
                sdl_character_sheet_screen_add_book_paragraph(para);
            para[0] = '\0';
            para_len = 0;
            sdl_character_sheet_screen_highlight_book_paragraph();
        }
        else if (line && line[0])
        {
            if (para_len > 0 && para_len + 1 < sizeof(para))
            {
                para[para_len++] = ' ';
                para[para_len] = '\0';
            }
            if (para_len < sizeof(para))
            {
                SDL_strlcpy(para + para_len, line, sizeof(para) - para_len);
                para_len += strlen(para + para_len);
            }
        }
        else
        {
            if (para_len > 0)
                sdl_character_sheet_screen_add_book_paragraph(para);
            para[0] = '\0';
            para_len = 0;
        }
    }
    sdl_character_sheet_screen_commit_book();

    /* Page-turn input loop (mirrors the race-book loop in get_player_choice). */
    while (!done)
    {
        int c;
        int clicked = 0;
        int action = UI_MENU_CLICK_PRIMARY;
        int page;
        int count;

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        c = inkey();

        if (ui_menu_click_take_action(&clicked, &action))
        {
            ui_menu_click_clear();
            if ((clicked == SDL_SELECT_CLICK_PAGE_NEXT
                    || clicked == SDL_SELECT_CLICK_PAGE_PREV)
                && action != UI_MENU_CLICK_HOVER
                && !sdl_character_sheet_screen_page_turning())
            {
                page = sdl_character_sheet_screen_select_page();
                count = sdl_character_sheet_screen_select_page_count();
                if (clicked == SDL_SELECT_CLICK_PAGE_NEXT)
                {
                    if (page < count - 1)
                        sdl_character_sheet_screen_begin_page_turn(+1);
                    else
                        done = true;
                }
                else if (page > 0)
                {
                    sdl_character_sheet_screen_begin_page_turn(-1);
                }
            }
            continue;
        }
        if (c == UI_MENU_CLICK_WAKE_KEY)
            continue;

        if (steamdeck_controls_active())
            c = steamdeck_menu_key(c, '4', '6');

        /* Swallow keys while a page-curl is mid-flight. */
        if (sdl_character_sheet_screen_page_turning())
            continue;

        page = sdl_character_sheet_screen_select_page();
        count = sdl_character_sheet_screen_select_page_count();

        if (c == ESCAPE || c == 'q' || c == 'Q')
            break;

        if (c == '4')
        {
            if (page > 0)
                sdl_character_sheet_screen_begin_page_turn(-1);
            continue;
        }

        if (c == '6' || c == ' ' || c == '\r' || c == '\n')
        {
            if (page < count - 1)
                sdl_character_sheet_screen_begin_page_turn(+1);
            else
                done = true;   /* a turn past the last page closes the book */
            continue;
        }
    }

    ui_menu_click_clear();
    sdl_character_sheet_screen_hide();
    screen_pop_supporting_panes_hidden();
    screen_load();
    return true;
}

/*
 * Quest typewriter menu function - displays quest dialog with typewriter effect
 * Based on print_story_intro() style
 */
void quest_typewriter_menu(cptr title, cptr texts[], int total_texts, byte title_color, byte text_color)
{
    int wid, h;
    int wrap_width;
    int row, col;
    const int indent = 2;
    bool skipped = false;

    /* Prefer the parchment book (SDL front-end) with page-turn navigation; fall
     * back to the terminal typewriter when it is unavailable. */
    if (quest_show_book(title, texts, total_texts))
        return;

    /* Save screen and start fresh */
    screen_save();
    screen_push_supporting_panes_hidden();

    /* Get terminal size after any hidden-pane layout change */
    Term_get_size(&wid, &h);
    wrap_width = wid - indent * 2;
    row = quest_typewriter_text_start_row(h);
    col = 0;

    Term_clear();
    Term_flush();
    ui_menu_click_begin();
    for (int click_row = 0; click_row < h; click_row++)
        ui_menu_click_add_full_row('\r', click_row);

    /* Display title */
    quest_typewriter_draw_title(title, title_color, wid);

    for (int idx = 0; idx < total_texts; idx++) {
        const char *s = texts[idx];

        /* Handle empty lines as paragraph breaks */
        if (!s || strlen(s) == 0) {
            /* Empty line - just advance row for paragraph break */
            row++;
            col = 0;
            if ((idx + 1 < total_texts)
                && !quest_typewriter_ensure_row(title, title_color, wid, h, &row, &col)) {
                Term_clear();
                ui_menu_click_clear();
                screen_pop_supporting_panes_hidden();
                screen_load();
                return;
            }
            /* Short pause for empty line */
            if (!skipped) Term_xtra(TERM_XTRA_DELAY, 200);
            continue;
        }

        col = 0;

        /* Print this string with proper word wrapping and typewriter effect */
        int i = 0;
        while (s[i]) {
            /* Handle explicit newlines */
            if (s[i] == '\n') {
                row++;
                col = 0;
                i++;
                if (!quest_typewriter_ensure_row(title, title_color, wid, h, &row, &col)) {
                    Term_clear();
                    ui_menu_click_clear();
                    screen_pop_supporting_panes_hidden();
                    screen_load();
                    return;
                }
                continue;
            }

            /* Find the end of the current word (or until we hit wrap width) */
            int word_start = i;
            int word_len = 0;
            bool has_space_after = false;

            /* Build the current word/phrase until we hit whitespace, newline, or exceed reasonable length */
            while (s[i] && s[i] != '\n' && word_len < wrap_width) {
                if (s[i] == ' ' || s[i] == '\t') {
                    has_space_after = true;
                    break;
                }
                word_len++;
                i++;
            }

            log_trace("WRAP DEBUG: word='%.*s', word_len=%d, col=%d, wrap_width=%d", word_len, &s[word_start], word_len, col, wrap_width);

            /* Check if this word fits on the current line */
            if (col + word_len > wrap_width && col > 0) {
                /* Word doesn't fit, wrap to next line */
                log_trace("WRAP DEBUG: Wrapping word to next line (col=%d + word_len=%d > wrap_width=%d)", col, word_len, wrap_width);
                row++;
                col = 0;
                if (!quest_typewriter_ensure_row(title, title_color, wid, h, &row, &col)) {
                    Term_clear();
                    ui_menu_click_clear();
                    screen_pop_supporting_panes_hidden();
                    screen_load();
                    return;
                }
            }

            /* Print the word character by character with typewriter effect */
            if (skipped) {
                /* Skip mode: print entire word instantly */
                for (int j = word_start; j < word_start + word_len; j++) {
                    Term_putch(indent + col, row, text_color, s[j]);
                    col++;
                }
            }
            else {
                /* Normal mode: typewriter effect with character-by-character */
                for (int j = word_start; j < word_start + word_len; j++) {
                    /* Check for ESC or Enter key press to skip typewriter effect */
                    char check_key;
                    if (Term_inkey(&check_key, false, false) == 0) {
                        /* Only respond to ESC or Enter - consume and check */
                        Term_inkey(&check_key, false, true);
                        if (check_key == ESCAPE || check_key == '\n' || check_key == '\r') {
                            skipped = true;
                            /* Print rest of current word instantly */
                            for (int k = j; k < word_start + word_len; k++) {
                                Term_putch(indent + col, row, text_color, s[k]);
                                col++;
                            }
                            break; /* Exit to continue with rest of text in skip mode */
                        }
                        /* Other keys are ignored (already consumed) */
                    }

                    /* Print character with typewriter effect */
                    Term_putch(indent + col, row, text_color, s[j]);
                    Term_fresh();
                    col++;

                    /* Delay 25 ms after each character for typewriter effect */
                    Term_xtra(TERM_XTRA_DELAY, 25);
                }
            }

            /* Handle the space/whitespace after the word */
            if (has_space_after) {
                if (s[i] == ' ') {
                    /* Only print space if we're not at the end of a line */
                    if (col < wrap_width) {
                        Term_putch(indent + col, row, text_color, ' ');
                        if (!skipped) Term_fresh();
                        col++;

                        /* Delay for space too (unless skipped) */
                        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 25);
                    }
                    i++; /* Skip the space */
                }
                else if (s[i] == '\t') {
                    /* Handle tab - convert to spaces but respect wrap width */
                    int tab_spaces = 4 - (col % 4);
                    for (int t = 0; t < tab_spaces && col < wrap_width; t++) {
                        Term_putch(indent + col, row, text_color, ' ');
                        if (!skipped) Term_fresh();
                        col++;

                        /* Delay for tab spaces (unless skipped) */
                        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 25);
                    }
                    i++; /* Skip the tab */
                }
            }
        }

        /* Move to next line after text */
        row++;
        col = 0;
        if ((idx + 1 < total_texts)
            && !quest_typewriter_ensure_row(title, title_color, wid, h, &row, &col)) {
            Term_clear();
            ui_menu_click_clear();
            screen_pop_supporting_panes_hidden();
            screen_load();
            return;
        }

        /* 400ms pause after each line of text (unless skipped) */
        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 400);
    }

    /* Refresh screen to show all text if skipped */
    if (skipped) Term_fresh();

    /* Final prompt */
    {
        char prompt_buf[48];
        any_key_prompt_text(prompt_buf, sizeof(prompt_buf), "continue");
        Term_putstr(15, h - 1, -1, TERM_L_WHITE, prompt_buf);
    }
    ui_menu_click_begin();
    for (int click_row = 0; click_row < h; click_row++)
        ui_menu_click_add_full_row('\r', click_row);
    inkey();
    ui_menu_click_clear();

    /* Flush any queued keypresses that accumulated during the typewriter effect */
    Term_flush();

    Term_clear();
    screen_pop_supporting_panes_hidden();
    screen_load();
}

/*
 * Remove quest giver monster by R_IDX without messaging
 */
void remove_quest_giver_silent(int quest_giver_r_idx)
{
    int i;

    log_trace("Attempting to remove quest giver silently with R_IDX: %d", quest_giver_r_idx);

    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];

        /* Skip empty slots */
        if (!m_ptr->r_idx) continue;

        if (m_ptr->r_idx == quest_giver_r_idx)
        {
            log_trace("Found quest giver at (%d, %d), removing silently", m_ptr->fy, m_ptr->fx);
            delete_monster_idx(i);
            return;
        }
    }

    log_trace("Quest giver with R_IDX %d not found on current level (silent remove)", quest_giver_r_idx);
}

/*
 * Remove quest giver monster by R_IDX
 */
void remove_quest_giver(int quest_giver_r_idx)
{
    int i;

    log_trace("Attempting to remove quest giver with R_IDX: %d", quest_giver_r_idx);

    /* Find and remove the quest giver */
    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];

        /* Skip empty slots */
        if (!m_ptr->r_idx) continue;

        /* Check if this is our quest giver */
        if (m_ptr->r_idx == quest_giver_r_idx)
        {
            log_trace("Found quest giver at (%d, %d), removing", m_ptr->fy, m_ptr->fx);

            /* Add a message about the quest giver departing */
            msg_print("The quest giver nods approvingly and fades away, their task complete.");

            /* Remove the monster */
            delete_monster_idx(i);

            log_trace("Quest giver successfully removed");
            return;
        }
    }

    log_trace("Quest giver with R_IDX %d not found on current level", quest_giver_r_idx);
}

/*
 * Check if a quest giver is present on the current level
 */
bool is_quest_giver_present(int quest_giver_r_idx)
{
    int i;

    /* Find the quest giver */
    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];

        /* Skip empty slots */
        if (!m_ptr->r_idx) continue;

        /* Check if this is our quest giver */
        if (m_ptr->r_idx == quest_giver_r_idx)
        {
            return true;
        }
    }

    return false;
}

/*
 * Spawn a quest giver near the player (for debug completion)
 */
bool spawn_quest_giver_near_player(int quest_giver_r_idx)
{
    int y, x;

    log_trace("Attempting to spawn quest giver R_IDX %d near player", quest_giver_r_idx);

    /* Try to find a suitable spot near the player */
    for (y = p_ptr->py - 3; y <= p_ptr->py + 3; y++)
    {
        for (x = p_ptr->px - 3; x <= p_ptr->px + 3; x++)
        {
            if (in_bounds(y, x) && cave_floor_bold(y, x) &&
                cave_m_idx[y][x] == 0 && distance(p_ptr->py, p_ptr->px, y, x) >= 2)
            {
                if (place_monster_one(y, x, quest_giver_r_idx, true, true, NULL))
                {
                    msg_print("A quest giver materializes nearby!");
                    log_trace("Successfully spawned quest giver at (%d, %d)", y, x);
                    return true;
                }
            }
        }
    }

    log_trace("Failed to spawn quest giver near player");
    return false;
}

bool trigger_adjacent_quest_giver_interaction(
    int quest_giver_r_idx, cptr quest_giver_name, void (*interaction)(void))
{
    int y, x;

    for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++)
    {
        for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++)
        {
            if (y == p_ptr->py && x == p_ptr->px) continue;
            if (!in_bounds(y, x)) continue;
            if (cave_m_idx[y][x] <= 0) continue;

            int m_idx = cave_m_idx[y][x];
            if (m_idx >= mon_max) continue;

            monster_type* m_ptr = &mon_list[m_idx];
            if (m_ptr->r_idx == quest_giver_r_idx)
            {
                log_trace("Found %s adjacent at (%d, %d), triggering interaction",
                    quest_giver_name ? quest_giver_name : "quest giver", y, x);
                interaction();
                return true;
            }
        }
    }

    return false;
}

bool ensure_reward_quest_giver_near_player(
    int quest_giver_r_idx, int radius, cptr quest_giver_name, cptr arrival_message,
    int* spawn_y, int* spawn_x)
{
    int y, x;

    if (spawn_y) *spawn_y = -1;
    if (spawn_x) *spawn_x = -1;

    if (is_quest_giver_present(quest_giver_r_idx))
    {
        log_trace("%s reward: quest giver already exists on current level",
            quest_giver_name ? quest_giver_name : "Quest");
        return true;
    }

    for (y = p_ptr->py - radius; y <= p_ptr->py + radius; y++)
    {
        for (x = p_ptr->px - radius; x <= p_ptr->px + radius; x++)
        {
            if (!in_bounds(y, x)) continue;
            if (!cave_floor_bold(y, x)) continue;
            if (cave_m_idx[y][x] != 0) continue;
            if (distance(p_ptr->py, p_ptr->px, y, x) < 2) continue;

            if (place_monster_one(y, x, quest_giver_r_idx, true, true, NULL))
            {
                if (spawn_y) *spawn_y = y;
                if (spawn_x) *spawn_x = x;
                if (arrival_message) msg_print(arrival_message);
                log_trace("%s reward: placed quest giver at (%d, %d)",
                    quest_giver_name ? quest_giver_name : "Quest", y, x);
                return true;
            }
        }
    }

    log_trace("%s reward: failed to place quest giver near player",
        quest_giver_name ? quest_giver_name : "Quest");
    return false;
}
