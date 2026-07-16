#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "quest/quest-internal.h"

static void quest_book_flush_paragraph(char *para, size_t *para_len)
{
    if (!para || !para_len || *para_len == 0)
        return;

    sdl_character_sheet_screen_add_book_paragraph(para);
    para[0] = '\0';
    *para_len = 0;
}

static bool quest_book_line_is(cptr line, size_t line_len, cptr marker)
{
    size_t marker_len;

    if (!line || !marker)
        return false;
    marker_len = strlen(marker);
    return line_len == marker_len && !strncmp(line, marker, marker_len);
}

/* Add one logical input line to a narrative book.  Non-empty adjacent lines
 * belong to the same reflowable paragraph; an empty line ends it. */
static void quest_book_add_line(cptr line, size_t line_len, char *para,
    size_t para_size, size_t *para_len)
{
    size_t copy_len;

    if (!para || para_size == 0 || !para_len)
        return;
    if (!line)
        line_len = 0;

    if (quest_book_line_is(line, line_len, "[newpage]"))
    {
        quest_book_flush_paragraph(para, para_len);
        sdl_character_sheet_screen_break_book_page();
        return;
    }
    if (quest_book_line_is(line, line_len, "[highlight]"))
    {
        quest_book_flush_paragraph(para, para_len);
        sdl_character_sheet_screen_highlight_book_paragraph();
        return;
    }
    if (line_len == 0)
    {
        quest_book_flush_paragraph(para, para_len);
        return;
    }

    if (*para_len > 0 && *para_len + 1 < para_size)
        para[(*para_len)++] = ' ';
    copy_len = MIN(line_len, para_size - 1 - *para_len);
    if (copy_len > 0)
    {
        memcpy(para + *para_len, line, copy_len);
        *para_len += copy_len;
    }
    para[*para_len] = '\0';
}

/*
 * Show quest narrative as a parchment "book" with page-turn navigation, reusing
 * the SDL front-end's character-sheet book.  The incoming texts[] may be line
 * entries from extract_quest_*_texts() or complete multiline speeches.  In
 * either form, consecutive non-empty lines form one paragraph and an empty line
 * is a paragraph break.  Each paragraph is re-flowed onto the page, then the
 * book paginates.
 */
static void quest_show_book(cptr title, cptr texts[], int total_texts,
    int target_page_count)
{
    char para[1024];
    size_t para_len = 0;
    int idx;
    bool done = false;

    screen_save();
    screen_push_supporting_panes_hidden();

    sdl_character_sheet_screen_begin_book(title);
    if (target_page_count < 0)
        target_page_count = sdl_touch_only_device_active() ? 4 : 3;
    sdl_character_sheet_screen_set_book_target_page_count(target_page_count);

    /* Build paragraphs from both line-array and embedded-newline input.  Thrall
     * dialogue uses the latter, so its blank lines remain real paragraph
     * breaks even when it requests a compact one-page layout. */
    para[0] = '\0';
    for (idx = 0; idx < total_texts; idx++)
    {
        cptr text = texts[idx];
        cptr line;

        if (!text || !text[0])
        {
            quest_book_add_line(NULL, 0, para, sizeof(para), &para_len);
            continue;
        }

        line = text;
        while (*line)
        {
            cptr end = line;

            while (*end && *end != '\r' && *end != '\n')
                end++;
            quest_book_add_line(line, (size_t)(end - line), para,
                sizeof(para), &para_len);
            if (!*end)
                break;
            if (*end == '\r' && end[1] == '\n')
                end++;
            line = end + 1;
        }
    }
    quest_book_flush_paragraph(para, &para_len);
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
}

/* The public entry points retain their historical names, but quest dialogue is
 * now rendered exclusively by the SDL parchment book. */
static void quest_typewriter_menu_internal(cptr title, cptr texts[],
    int total_texts, byte title_color, byte text_color, int target_page_count)
{
    (void)title_color;
    (void)text_color;
    quest_show_book(title, texts, total_texts, target_page_count);
}

void quest_typewriter_menu(cptr title, cptr texts[], int total_texts,
    byte title_color, byte text_color)
{
    quest_typewriter_menu_internal(title, texts, total_texts, title_color,
        text_color, -1);
}

void quest_typewriter_menu_pages(cptr title, cptr texts[], int total_texts,
    byte title_color, byte text_color, int target_page_count)
{
    quest_typewriter_menu_internal(title, texts, total_texts, title_color,
        text_color, MAX(0, target_page_count));
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
