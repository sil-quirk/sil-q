/* File: monster-recall.c */

#include "monster-internal.h"

static void roff_top_live(int r_idx, const monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[r_idx];

    byte a1;
    char c1;

    /* Get the chars and attrs using graphics-aware macros */
    c1 = monster_char(r_ptr);
    a1 = monster_attr(r_ptr);

    /* Clear the top line */
    Term_erase(0, 0, 255);

    /* Reset the cursor */
    Term_gotoxy(0, 0);

    /* A title (use "The" for non-uniques) */
    if (!(r_ptr->flags1 & RF1_UNIQUE))
    {
        Term_addstr(-1, TERM_WHITE, "The ");
    }

    /* Dump the name */
    Term_addstr(-1, TERM_WHITE, (r_name + r_ptr->name));

    /* Append the "standard" attr/char info */
    Term_addstr(-1, TERM_WHITE, " - ");
    Term_addch(a1, c1);
    if (use_bigtile)
    {
        Term_addch(255, -1);
    }

    if (m_ptr && (m_ptr->maxhp > 0))
    {
        char hp_bar[10];
        byte attr = health_attr(m_ptr->hp, m_ptr->maxhp);

        monster_health_bar_text(m_ptr, hp_bar, sizeof(hp_bar), 8);

        Term_addstr(-1, TERM_WHITE, " ");
        Term_addstr(-1, attr, hp_bar[0] ? hp_bar : "-");
    }

    Term_addstr(-1, TERM_SLATE, "");
}

/*
 * Hack -- Display the "name" and "attr/chars" of a monster race
 */
void roff_top(int r_idx)
{
    roff_top_live(r_idx, NULL);
}

static bool monster_recall_overlay_story_font_enabled(void)
{
    return story_monster_desc_main;
}

typedef struct monster_recall_screen_capture
{
    int width;
    int height;
    int target_cols;
    byte* attrs;
    char* chars;
    byte* tattrs;
    char* tchars;
    byte* story;
} monster_recall_screen_capture;

static void monster_recall_screen_capture_free(
    monster_recall_screen_capture* capture)
{
    if (!capture)
        return;

    mem_free_null(capture->attrs);
    mem_free_null(capture->chars);
    mem_free_null(capture->tattrs);
    mem_free_null(capture->tchars);
    mem_free_null(capture->story);

    capture->width = 0;
    capture->height = 0;
}

static int monster_recall_screen_capture_used_rows(term* t)
{
    if (!t || !t->scr)
        return 0;

    for (int y = t->hgt - 1; y >= 0; y--)
    {
        for (int x = 0; x < t->wid; x++)
        {
            if ((t->scr->c[y][x] != ' ')
                || (t->scr->a[y][x] != t->attr_blank)
                || (t->scr->story[y][x] != 0))
            {
                return y + 1;
            }
        }
    }

    return 0;
}

static int monster_recall_screen_capture_used_cols(term* t, int rows)
{
    int used_cols = 0;

    if (!t || !t->scr || rows <= 0)
        return 0;

    if (rows > t->hgt)
        rows = t->hgt;

    for (int y = 0; y < rows; y++)
    {
        for (int x = t->wid - 1; x >= 0; x--)
        {
            if ((t->scr->c[y][x] != ' ')
                || (t->scr->a[y][x] != t->attr_blank)
                || (t->scr->story[y][x] != 0))
            {
                if (x + 1 > used_cols)
                    used_cols = x + 1;
                break;
            }
        }
    }

    return used_cols;
}

static bool monster_recall_screen_capture_build(
    int r_idx, const monster_type* m_ptr,
    monster_recall_screen_capture* capture)
{
    term scratch;
    term* saved_term = Term;
    void (*old_hook)(byte, cptr) = text_out_hook;
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;
    story_font_term_state story_state;
    bool story_pushed = false;
    bool scratch_ready = false;
    bool success = false;
    bool use_story_font = monster_recall_overlay_story_font_enabled();
    int term_wid = 80;
    int term_hgt = 24;
    int target_wid;
    int max_target_wid;
    int used_rows;
    int used_cols;

    if (!capture || !saved_term)
        return false;

    SDL_memset(capture, 0, sizeof(*capture));
    SDL_memset(&scratch, 0, sizeof(scratch));

    Term_get_size(&term_wid, &term_hgt);
    (void)term_hgt;

    target_wid = term_wid;
    max_target_wid = sdl_description_overlay_max_cols();
    if (max_target_wid > 0 && target_wid > max_target_wid)
        target_wid = max_target_wid;
    if (target_wid < 20)
        target_wid = 20;

    term_wid = use_story_font ? 200 : target_wid;
    if (term_wid < target_wid)
        term_wid = target_wid;

    if (term_init(&scratch, term_wid, 255, 16) != 0)
        goto cleanup;
    scratch_ready = true;

    Term_activate(&scratch);
    story_font_term_push_slot(use_story_font, false, STORY_FONT_SLOT_SECONDARY, &story_state);
    story_pushed = true;

    text_out_hook = use_story_font
        ? text_out_to_screen_overlay_story : text_out_to_screen;
    text_out_wrap = use_story_font ? target_wid : 0;
    text_out_indent = 0;

    Term_clear();
    roff_top_live(r_idx, m_ptr);
    Term_gotoxy(0, 1);
    Term_erase(0, 1, 255);
    describe_monster(r_idx, false, m_ptr);

    used_rows = monster_recall_screen_capture_used_rows(Term);
    if (used_rows < 1)
        used_rows = 1;
    used_cols = monster_recall_screen_capture_used_cols(Term, used_rows);
    if (used_cols < 1)
        used_cols = 1;

    capture->width = used_cols;
    capture->height = used_rows;
    capture->target_cols = target_wid;
    capture->attrs = mem_alloc_array(capture->width * capture->height, byte);
    capture->chars = mem_alloc_array(capture->width * capture->height, char);
    capture->tattrs = mem_alloc_array(capture->width * capture->height, byte);
    capture->tchars = mem_alloc_array(capture->width * capture->height, char);
    capture->story = mem_alloc_array(capture->width * capture->height, byte);

    for (int y = 0; y < capture->height; y++)
    {
        for (int x = 0; x < capture->width; x++)
        {
            int idx = y * capture->width + x;
            capture->attrs[idx] = scratch.scr->a[y][x];
            capture->chars[idx] = scratch.scr->c[y][x];
            capture->tattrs[idx] = scratch.scr->ta[y][x];
            capture->tchars[idx] = scratch.scr->tc[y][x];
            capture->story[idx] = scratch.scr->story[y][x];
            if (use_story_font
                && ((capture->chars[idx] != ' ')
                    || (capture->attrs[idx] != scratch.attr_blank)))
            {
                capture->story[idx] |= STORY_FLAG_USE | STORY_FLAG_SLOT2;
            }
        }
    }

    success = true;

cleanup:
    text_out_hook = old_hook;
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;

    if (story_pushed)
        story_font_term_pop(&story_state);

    if (saved_term && Term != saved_term)
        Term_activate(saved_term);

    if (scratch_ready)
        term_nuke(&scratch);

    if (!success)
        monster_recall_screen_capture_free(capture);

    return success;
}

static int monster_recall_screen_capture_view(
    const monster_recall_screen_capture* capture)
{
    int scroll = 0;
    int exit_key = ESCAPE;
    bool overlay_active = false;

    if (!capture)
        return 0;

    while (true)
    {
        int term_hgt = 24;
        int visible_rows = 1;
        int max_scroll = 0;
        int dir;
        char ch;

        Term_get_size(NULL, &term_hgt);
        if (!sdl_description_overlay_present(capture->attrs, capture->chars,
                capture->tattrs, capture->tchars, capture->story,
                capture->width, capture->height, capture->target_cols, scroll,
                true, &visible_rows, &max_scroll))
        {
            exit_key = 0;
            break;
        }
        overlay_active = true;

        if (scroll > max_scroll)
            scroll = max_scroll;

        ui_scroll_area_begin(0, MAX(0, term_hgt - 1),
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', '6', '4');
        ui_scroll_area_set_tap_key(ESCAPE);

        ch = inkey();
        dir = target_dir(ch);
        if ((dir == 8) || (dir == 2))
            ch = I2D(dir);

        if ((ch == '8') || (ch == '='))
        {
            if (scroll > 0)
                scroll--;
        }
        else if ((ch == '2') || (ch == '\n') || (ch == '\r'))
        {
            if (scroll < max_scroll)
                scroll++;
        }
        else if ((ch == '3') || (ch == ' '))
        {
            scroll += visible_rows;
            if (scroll > max_scroll)
                scroll = max_scroll;
        }
        else if ((ch == '9') || (ch == '-'))
        {
            scroll -= visible_rows;
            if (scroll < 0)
                scroll = 0;
        }
        else
        {
            exit_key = ch;
            break;
        }
    }

    if (overlay_active)
        sdl_description_overlay_clear();
    ui_scroll_area_clear();
    return exit_key;
}

/*
 * Hack -- describe the given monster race at the top of the screen
 *
 * Returns the key that dismissed the scrollable recall view, or zero when
 * the legacy one-screen display was used and the caller should still wait.
 */
int screen_roff(int r_idx, const monster_type* m_ptr)
{
    monster_recall_screen_capture capture;
    bool have_capture;

    if (monster_recall_blocked_by_hallucination())
    {
        monster_recall_show_hallucination_block();
        return ESCAPE;
    }

    /* Flush messages */
    message_flush();

    have_capture = monster_recall_screen_capture_build(r_idx, m_ptr, &capture);
    if (have_capture)
    {
        int exit_key = monster_recall_screen_capture_view(&capture);

        monster_recall_screen_capture_free(&capture);
        return exit_key;
    }

    return 0;
}

/*
 * Hack -- describe the given monster race in the current "term" window
 */
void display_roff(int r_idx, const monster_type* m_ptr)
{
    int y;

    if (monster_recall_blocked_by_hallucination())
    {
        for (y = 0; y < Term->hgt; y++)
        {
            Term_erase(0, y, 255);
        }
        return;
    }

    bool use_story_font = story_monster_desc_enabled();
    story_font_term_state story_state;
    story_font_term_push_slot(use_story_font, false, STORY_FONT_SLOT_SECONDARY, &story_state);

    /* Erase the window */
    for (y = 0; y < Term->hgt; y++)
    {
        /* Erase the line */
        Term_erase(0, y, 255);
    }

    /* Begin recall */
    Term_gotoxy(0, 1);

    /* Output to the screen */
    void (*old_hook)(byte, cptr) = text_out_hook;
    int old_indent = text_out_indent;
    int old_wrap = text_out_wrap;

    int wid = 0, hgt = 0;
    Term_get_size(&wid, &hgt);
    text_out_indent = 0;
    text_out_wrap = (wid > 2) ? (wid - 1) : 0;
    text_out_hook = text_out_to_screen;

    /* Recall monster */
    describe_monster(r_idx, false, m_ptr);

    /* Describe monster */
    roff_top_live(r_idx, m_ptr);

    text_out_hook = old_hook;
    text_out_indent = old_indent;
    text_out_wrap = old_wrap;

    story_font_term_pop(&story_state);
}
