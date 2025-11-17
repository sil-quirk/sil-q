/* ui/story_font.c - Story font rendering helpers */

#include "../angband.h"
#include "../externs.h"
#include "../log/log.h"
#include "story_font.h"

/*
 * Count how many lines text will occupy when using story font with pixel-based wrapping.
 * Similar to count_wrapped_lines but accounts for proportional font width.
 */
int count_wrapped_lines_story(cptr str, int wrap_cols, int indent)
{
    if (!str || wrap_cols <= 0)
        return 1;

    /* Convert column-based wrap to pixel width */
    int cell_width = sdl_get_cell_width();
    int wrap_pixels = wrap_cols * cell_width;

    int lines = 1;
    int x = indent;
    cptr s = str;

    while (*s)
    {
        /* Handle newlines */
        if (*s == '\n')
        {
            x = indent;
            lines++;
            s++;
            continue;
        }

        /* Skip leading spaces */
        while (*s == ' ')
        {
            x++;
            if (x >= wrap_cols)
            {
                x = indent;
                lines++;
            }
            s++;
        }

        if (!*s)
            break;

        /* Find the end of the current word */
        cptr word_start = s;
        int word_chars = 0;
        while (s[word_chars] && s[word_chars] != ' ' && s[word_chars] != '\n')
            word_chars++;

        if (word_chars == 0)
            continue;

        /* Measure the word in pixels */
        int word_pixels = sdl_story_font_text_width(word_start, word_chars);
        int current_pixels = x * cell_width;

        /* Check if word fits on current line */
        if (x > indent && (current_pixels + word_pixels) > wrap_pixels)
        {
            x = indent;
            lines++;
        }

        /* Account for the word's width in columns (approximate) */
        x += word_chars;
        if (x >= wrap_cols)
        {
            x = indent;
            lines++;
        }

        /* Move past the word */
        s += word_chars;
    }

    return lines;
}

bool story_inventory_enabled(void) { return story_inventory_lists; }
bool story_equipment_enabled(void) { return story_equipment_lists; }
bool story_look_enabled(void) { return story_display_lists; }
bool story_character_enabled(void) { return story_character_sheet; }

void text_out_to_screen_story(byte a, cptr str)
{
    int x, y;
    int wid, h;
    int wrap_cols;
    cptr s;

    (void)Term_get_size(&wid, &h);
    (void)Term_locate(&x, &y);

    if ((text_out_wrap > 0) && (text_out_wrap < wid))
        wrap_cols = text_out_wrap;
    else
        wrap_cols = wid;

    int cell_width = sdl_get_cell_width();
    int wrap_pixels = wrap_cols * cell_width;

    log_trace("=== text_out_to_screen_story START ===");
    log_trace("Story wrapping: wid=%d, wrap_cols=%d, cell_width=%d, wrap_pixels=%d", wid, wrap_cols, cell_width,
        wrap_pixels);
    log_trace("Initial cursor: x=%d, y=%d, text_out_indent=%d, text='%.50s'", x, y, text_out_indent, str);

    int current_x_pixels = text_out_indent * cell_width;

    s = str;
    while (*s)
    {
        if (*s == '\n')
        {
            x = text_out_indent;
            current_x_pixels = text_out_indent * cell_width;
            y++;
            Term_erase(x, y, 255);
            s++;
            continue;
        }

        while (*s == ' ')
        {
            Term_addch(a, ' ');
            x++;
            current_x_pixels += cell_width;
            s++;
        }

        cptr word_start = s;
        int word_chars = 0;
        while (s[word_chars] && s[word_chars] != ' ' && s[word_chars] != '\n')
            word_chars++;

        if (word_chars == 0)
            continue;

        int word_pixels = sdl_story_font_text_width(word_start, word_chars);

        bool exceeds_pixels = (x > text_out_indent && (current_x_pixels + word_pixels) > wrap_pixels);
        bool exceeds_columns = (x + word_chars >= wid);

        log_trace(
            "Word: '%.*s' (%d chars), pixels=%d, current_x_pixels=%d, current_term_col=%d, exceeds_pixels=%s, exceeds_columns=%s",
            word_chars, word_start, word_chars, word_pixels, current_x_pixels, x,
            exceeds_pixels ? "YES" : "NO", exceeds_columns ? "YES" : "NO");

        if (exceeds_pixels || exceeds_columns)
        {
            x = text_out_indent;
            current_x_pixels = text_out_indent * cell_width;
            y++;
            Term_erase(x, y, 255);
            log_trace("Wrapped to next line, x=%d, current_x_pixels=%d", x, current_x_pixels);
        }

        for (int i = 0; i < word_chars; i++)
        {
            char ch = (isprint((unsigned char)word_start[i]) ? word_start[i] : ' ');
            Term_addch(a, ch);
            x++;
        }

        log_trace("After word output: term_col=%d, pixel_pos=%d, word_pixels=%d, new_pixel_pos=%d", x,
            current_x_pixels, word_pixels, current_x_pixels + word_pixels);

        current_x_pixels += word_pixels;
        s += word_chars;
    }

    log_trace("=== text_out_to_screen_story END === Final term_col=%d, final_pixel_pos=%d", x, current_x_pixels);
}

static void story_print_text_internal(int row, int col, int max_cols, byte attr, cptr text, bool force_grid)
{
    if (!text)
        text = "";

    if (max_cols > 0)
        Term_erase(col, row, max_cols);

    if (sdl_is_story_font_enabled())
    {
        log_debug("story_print_text: STORY FONT at row=%d col=%d max_cols=%d text='%.50s'", row, col, max_cols, text);
        log_trace("story_print_text: text_out_indent (BEFORE)=%d, text_out_wrap (BEFORE)=%d", text_out_indent,
            text_out_wrap);

        bool previous_grid = sdl_is_story_font_grid();
        bool restore_grid = false;
        if (force_grid != previous_grid)
        {
            sdl_story_font_set_grid(force_grid);
            restore_grid = true;
        }

        int old_indent = text_out_indent;
        int old_wrap = text_out_wrap;
        void (*old_hook)(byte, cptr) = text_out_hook;

        if (max_cols > 0)
            text_out_wrap = col + max_cols;
        else
            text_out_wrap = 0;

        text_out_indent = col;
        text_out_hook = text_out_to_screen;

        log_trace("story_print_text: Setting text_out_indent=%d, text_out_wrap=%d", text_out_indent, text_out_wrap);
        log_trace("story_print_text: About to call Term_gotoxy(%d, %d)", col, row);

        Term_gotoxy(col, row);

        int cursor_x, cursor_y;
        Term_locate(&cursor_x, &cursor_y);
        log_trace("story_print_text: After Term_gotoxy, cursor at (%d, %d)", cursor_x, cursor_y);

        text_out_c(attr, text);

        Term_locate(&cursor_x, &cursor_y);
        log_trace("story_print_text: After text_out_c, cursor at (%d, %d)", cursor_x, cursor_y);

        text_out_indent = old_indent;
        text_out_wrap = old_wrap;
        text_out_hook = old_hook;

        if (restore_grid)
            sdl_story_font_set_grid(previous_grid);
        return;
    }

    log_debug("story_print_text: MONO FONT at row=%d col=%d: '%.50s'", row, col, text);

    c_put_str(attr, text, row, col);
}

void story_print_text(int row, int col, int max_cols, byte attr, cptr text)
{
    story_print_text_internal(row, col, max_cols, attr, text, false);
}

void story_print_text_grid(int row, int col, int max_cols, byte attr, cptr text)
{
    story_print_text_internal(row, col, max_cols, attr, text, true);
}

void story_print_mono(int row, int col, byte attr, cptr text)
{
    if (!text)
        text = "";

    bool reenable_story = false;
    if (sdl_is_story_font_enabled())
    {
        sdl_story_font_disable();
        reenable_story = true;
    }

    c_put_str(attr, text, row, col);

    if (reenable_story)
        sdl_story_font_enable();
}

void story_fill_rect(int row, int col, int width_cols, byte attr)
{
    if (width_cols <= 0)
        return;

    if (row < 0 || row >= Term->hgt)
        return;

    int start_col = MAX(0, col);
    int end_col = MIN(Term->wid, col + width_cols);

    byte* scr_aa = Term->scr->a[row];
    char* scr_cc = Term->scr->c[row];

    for (int x = start_col; x < end_col; x++)
    {
        scr_aa[x] = attr;
        scr_cc[x] = ' ';
    }

    if (start_col < end_col)
    {
        if (row < Term->y1)
            Term->y1 = row;
        if (row > Term->y2)
            Term->y2 = row;
        if (start_col < Term->x1[row])
            Term->x1[row] = start_col;
        if (end_col - 1 > Term->x2[row])
            Term->x2[row] = end_col - 1;
    }
}
